/**
 * @file lv_number.c
 * @brief 数值统一抽象层 (lvNumber) 实现 —— 0b：不透明句柄 + 强制池化 + 精确提升
 *
 * @details 本文件是 lvNumber 的唯一实现（design: number-abstraction-layer-design.md，
 *          ND-1/ND-2）：
 *          - 句柄即池节点（常驻池 free-list + 帧池 TLS 栈），禁止逐数系统分配；
 *          - 表示：INTEGER(int64 inline) / RATIONAL(mpq 语义) / FLOAT(double)；
 *          - 跨类型算术精确提升：int ×/±/÷ rational → rational（mpq）；
 *            任一 float → float（近似，与旧行为一致）；int ÷ int 整数截断（契约钉住）；
 *          - mpq 内部 limb 存储走 GMP 默认分配器（同 coeff_pool/mpz 现状；
 *            全局 GMP allocator 接线为独立小步，见头文件注释与批次登记）。
 *
 * @author Lv-00 Project
 * @version 2.0.0-dev（0b）
 */

#include "lv/lv_number.h"
#include "lv/rational.h"        /* 解析复用 lv_rational_from_string（含 <gmp.h>） */
#include "lv/lv_str_utils.h"    /* lv_mpq_to_string（GMP 规范形保真） */
#include "lv/lv_parse_utils.h"  /* lv_parse_double_strict */
#include "lv/lv_utils.h"        /* lv_malloc / lv_realloc / lv_free / lv_free_ptr_array 族 */

#include <gmp.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

/* ============================================================
 * 池化：节点即句柄
 * ============================================================ */

#define lv_NUM_BLOCK_CAP 512u   /* 每块节点数（块 ~24KB，lv_malloc 一次性） */
#define lv_NUM_KIND_NONE 0xFFFFFFFFu

struct lvNumber {
    uint32_t kind;                    /* lvNumberType 或 lv_NUM_KIND_NONE */
    uint32_t framed;                  /* 1 = 帧池对象（frame_end 回收） */
    struct lvNumber *next_free;       /* free-list 链（仅空闲节点有效） */
    union {
        int64_t i;                    /* INTEGER */
        double  f;                    /* FLOAT */
        mpq_t   q;                    /* RATIONAL */
    } u;
};

typedef struct lvNumBlock {
    struct lvNumBlock *next;
    lvNumber nodes[lv_NUM_BLOCK_CAP];
} lvNumBlock;

static lvNumBlock *g_blocks = NULL;   /* 常驻池块链 */
static lvNumber  *g_free_head = NULL; /* 常驻池 free-list */

struct lvNumberFrame {
    lvNumberFrame *prev;
    lvNumber **arr;                   /* 本帧内创建的节点 */
    size_t len;
    size_t cap;
};

#if defined(_MSC_VER)
static __declspec(thread) lvNumberFrame *g_frame = NULL;
#else
static __thread lvNumberFrame *g_frame = NULL;
#endif

/* ---- 常驻池 ---- */

static lvNumber *pool_grow_block(void) {
    lvNumBlock *b = (lvNumBlock *) lv_malloc(sizeof(lvNumBlock));
    if (!b) return NULL;
    memset(b, 0, sizeof(*b));
    b->next = g_blocks;
    g_blocks = b;
    /* 逆序压入 free-list，保持顺序分配 */
    for (unsigned i = lv_NUM_BLOCK_CAP; i-- > 0;) {
        lvNumber *n = &b->nodes[i];
        n->kind = lv_NUM_KIND_NONE;
        n->next_free = g_free_head;
        g_free_head = n;
    }
    lvNumber *out = g_free_head;
    g_free_head = g_free_head->next_free;
    out->next_free = NULL;
    out->framed = 0;
    return out;
}

static lvNumber *node_alloc(void) {
    if (!g_free_head) {
        if (!pool_grow_block()) return NULL;
    }
    lvNumber *n = g_free_head;
    g_free_head = n->next_free;
    n->next_free = NULL;
    n->framed = 0;
    n->kind = lv_NUM_KIND_NONE;
    return n;
}

/** 归还常驻节点（mpq 内部存储一并 clear）；帧节点不得走此路径 */
static void node_release(lvNumber *n) {
    if (!n) return;
    if (n->kind == lv_NUMBER_RATIONAL) {
        mpq_clear(n->u.q);
    }
    n->kind = lv_NUM_KIND_NONE;
    n->framed = 0;
    n->next_free = g_free_head;
    g_free_head = n;
}

/** 新建节点并登记到活动帧（若有）；失败返回 NULL */
static lvNumber *node_new(uint32_t kind) {
    lvNumber *n = node_alloc();
    if (!n) return NULL;
    n->kind = kind;
    if (kind == lv_NUMBER_RATIONAL) {
        mpq_init(n->u.q);
    }
    if (g_frame) {
        n->framed = 1;
        lvNumberFrame *f = g_frame;
        if (f->len == f->cap) {
            size_t nc = f->cap ? f->cap * 2 : 64;
            lvNumber **na = (lvNumber **) lv_realloc(f->arr, nc * sizeof(lvNumber *));
            if (!na) {
                /* 登记失败：回退为常驻对象（调用方负责 destroy），不丢失数据 */
                node_release(n);
                return NULL;
            }
            f->arr = na;
            f->cap = nc;
        }
        f->arr[f->len++] = n;
    }
    return n;
}

/** 销毁新节点分配但尚未发布（或运算失败）的节点：帧内同常驻一样直接回收 */
static void node_discard(lvNumber *n) {
    if (!n) return;
    if (n->framed && g_frame) {
        /* 从帧记录剔除（最后一次出现）——运算失败路径极少，线性扫描可接受 */
        lvNumberFrame *f = g_frame;
        for (size_t i = f->len; i-- > 0;) {
            if (f->arr[i] == n) {
                f->arr[i] = f->arr[--f->len];
                break;
            }
        }
    }
    node_release(n);
}

/* ============================================================
 * 帧池 API（ND-2）
 * ============================================================ */

lvNumberFrame *lv_number_frame_begin(void) {
    lvNumberFrame *f = (lvNumberFrame *) lv_malloc(sizeof(lvNumberFrame));
    if (!f) return NULL;
    f->prev = g_frame;
    f->arr = NULL;
    f->len = 0;
    f->cap = 0;
    g_frame = f;
    return f;
}

void lv_number_frame_end(lvNumberFrame *frame) {
    if (frame && frame != g_frame) return; /* 不匹配：安全拒绝 */
    lvNumberFrame *f = g_frame;
    if (!f) return;
    g_frame = f->prev;
    for (size_t i = 0; i < f->len; i++) {
        node_release(f->arr[i]); /* 帧节点直接归还 free-list（mpq clear 内含） */
    }
    lv_free((void **) &f->arr);
    lv_free((void **) &f);
}

bool lv_number_in_frame(void) {
    return g_frame != NULL;
}

/* ============================================================
 * 内部：类型/值读取与 mpq 提升
 * ============================================================ */

static bool node_is_int(const lvNumber *n)  { return n && n->kind == lv_NUMBER_INTEGER; }
static bool node_is_rat(const lvNumber *n)  { return n && n->kind == lv_NUMBER_RATIONAL; }
static bool node_is_float(const lvNumber *n){ return n && n->kind == lv_NUMBER_FLOAT; }
static bool any_float(const lvNumber *a, const lvNumber *b) {
    return node_is_float(a) || node_is_float(b);
}

/** 把节点值装入已 init 的 mpq（int 提升为分母 1） */
static void node_to_mpq(mpq_t out, const lvNumber *n) {
    if (node_is_int(n)) {
        mpq_set_si(out, n->u.i, 1);
    } else {
        mpq_set(out, n->u.q);
    }
}

static double node_to_double(const lvNumber *n) {
    switch (n->kind) {
        case lv_NUMBER_INTEGER:  return (double) n->u.i;
        case lv_NUMBER_FLOAT:    return n->u.f;
        case lv_NUMBER_RATIONAL: return mpq_get_d(n->u.q);
        default:                 return 0.0;
    }
}

static lvNumber *new_int(int64_t v) {
    lvNumber *n = node_new(lv_NUMBER_INTEGER);
    if (n) n->u.i = v;
    return n;
}

static lvNumber *new_float(double v) {
    lvNumber *n = node_new(lv_NUMBER_FLOAT);
    if (n) n->u.f = v;
    return n;
}

static lvNumber *new_rat_from_mpq(const mpq_t src) {
    lvNumber *n = node_new(lv_NUMBER_RATIONAL);
    if (n) mpq_set(n->u.q, src);
    return n;
}

/* ============================================================
 * 算术：二元（跨类型精确提升）
 * ============================================================ */

typedef enum { LV_NUM_BIN_ADD, LV_NUM_BIN_SUB, LV_NUM_BIN_MUL, LV_NUM_BIN_DIV } lvNumBinOp;

static lvNumber *bin_op(const lvNumber *a, const lvNumber *b, lvNumBinOp op) {
    if (!a || !b) return NULL;

    /* int op int：保持旧契约（整数除法截断；除零 NULL） */
    if (node_is_int(a) && node_is_int(b)) {
        int64_t x = a->u.i, y = b->u.i;
        switch (op) {
            case LV_NUM_BIN_ADD: return new_int(x + y);
            case LV_NUM_BIN_SUB: return new_int(x - y);
            case LV_NUM_BIN_MUL: return new_int(x * y);
            case LV_NUM_BIN_DIV:
                if (y == 0) return NULL;
                return new_int(x / y);
        }
    }

    /* 任一 float：double 语义（与旧行为一致；float÷0 → inf） */
    if (any_float(a, b)) {
        double x = node_to_double(a), y = node_to_double(b);
        switch (op) {
            case LV_NUM_BIN_ADD: return new_float(x + y);
            case LV_NUM_BIN_SUB: return new_float(x - y);
            case LV_NUM_BIN_MUL: return new_float(x * y);
            case LV_NUM_BIN_DIV: return new_float(x / y);
        }
    }

    /* int / rational → rational（mpq 精确提升） */
    mpq_t x, y;
    mpq_init(x);
    mpq_init(y);
    node_to_mpq(x, a);
    node_to_mpq(y, b);
    lvNumber *r = NULL;
    switch (op) {
        case LV_NUM_BIN_ADD: mpq_add(x, x, y); r = new_rat_from_mpq(x); break;
        case LV_NUM_BIN_SUB: mpq_sub(x, x, y); r = new_rat_from_mpq(x); break;
        case LV_NUM_BIN_MUL: mpq_mul(x, x, y); r = new_rat_from_mpq(x); break;
        case LV_NUM_BIN_DIV:
            if (mpq_sgn(y) == 0) { r = NULL; }
            else { mpq_div(x, x, y); r = new_rat_from_mpq(x); }
            break;
    }
    mpq_clear(x);
    mpq_clear(y);
    return r;
}

lvNumber *lv_number_add(const lvNumber *a, const lvNumber *b) { return bin_op(a, b, LV_NUM_BIN_ADD); }
lvNumber *lv_number_sub(const lvNumber *a, const lvNumber *b) { return bin_op(a, b, LV_NUM_BIN_SUB); }
lvNumber *lv_number_mul(const lvNumber *a, const lvNumber *b) { return bin_op(a, b, LV_NUM_BIN_MUL); }
lvNumber *lv_number_div(const lvNumber *a, const lvNumber *b) { return bin_op(a, b, LV_NUM_BIN_DIV); }

lvNumber *lv_number_neg(const lvNumber *n) {
    if (!n) return NULL;
    switch (n->kind) {
        case lv_NUMBER_INTEGER:  return new_int(-n->u.i);
        case lv_NUMBER_FLOAT:    return new_float(-n->u.f);
        case lv_NUMBER_RATIONAL: {
            lvNumber *r = new_rat_from_mpq(n->u.q);
            if (r) mpq_neg(r->u.q, r->u.q);
            return r;
        }
        default: return NULL;
    }
}

lvNumber *lv_number_abs(const lvNumber *n) {
    if (!n) return NULL;
    if (lv_number_is_negative(n)) return lv_number_neg(n);
    return lv_number_clone(n);
}

lvNumber *lv_number_pow(const lvNumber *base, int exp) {
    if (!base) return NULL;
    if (exp == 0) return new_int(1);
    if (exp < 0) {
        /* 负指数：1 / base^|exp|（double 路径，与旧行为一致） */
        lvNumber *pos = lv_number_pow(base, -exp);
        if (!pos) return NULL;
        double pd = node_to_double(pos);
        if (!g_frame) node_release(pos);
        return new_float(1.0 / pd);
    }
    /* 快速幂（对象乘法） */
    lvNumber *result = new_int(1);
    lvNumber *cur = lv_number_clone(base);
    if (!result || !cur) {
        if (!g_frame) { node_release(result); node_release(cur); }
        return NULL;
    }
    int e = exp;
    while (e > 0) {
        if (e & 1) {
            lvNumber *nr = lv_number_mul(result, cur);
            if (!g_frame) node_release(result);
            result = nr;
            if (!result) { if (!g_frame) node_release(cur); return NULL; }
        }
        e >>= 1;
        if (e > 0) {
            lvNumber *nc = lv_number_mul(cur, cur);
            if (!g_frame) node_release(cur);
            cur = nc;
            if (!cur) { if (!g_frame) node_release(result); return NULL; }
        }
    }
    if (!g_frame) node_release(cur);
    return result;
}

/* ============================================================
 * 比较（跨类型：任一 float → double；其余 mpq 精确）
 * ============================================================ */

int lv_number_compare(const lvNumber *a, const lvNumber *b) {
    if (!a || !b) return 0;
    if (node_is_int(a) && node_is_int(b)) {
        return (a->u.i < b->u.i) ? -1 : (a->u.i > b->u.i ? 1 : 0);
    }
    if (any_float(a, b)) {
        double x = node_to_double(a), y = node_to_double(b);
        return (x < y) ? -1 : (x > y ? 1 : 0);
    }
    mpq_t x, y;
    mpq_init(x); mpq_init(y);
    node_to_mpq(x, a); node_to_mpq(y, b);
    int c = mpq_cmp(x, y);
    mpq_clear(x); mpq_clear(y);
    return (c < 0) ? -1 : (c > 0 ? 1 : 0);
}

bool lv_number_eq(const lvNumber *a, const lvNumber *b)  { return (a && b) ? lv_number_compare(a, b) == 0 : false; }
bool lv_number_lt(const lvNumber *a, const lvNumber *b)  { return (a && b) ? lv_number_compare(a, b) < 0 : false; }
bool lv_number_gt(const lvNumber *a, const lvNumber *b)  { return (a && b) ? lv_number_compare(a, b) > 0 : false; }
bool lv_number_lte(const lvNumber *a, const lvNumber *b) { return (a && b) ? lv_number_compare(a, b) <= 0 : false; }
bool lv_number_gte(const lvNumber *a, const lvNumber *b) { return (a && b) ? lv_number_compare(a, b) >= 0 : false; }

/* ============================================================
 * 转换 / 查询
 * ============================================================ */

double lv_number_to_double(const lvNumber *n) {
    if (!n) return 0.0;
    return node_to_double(n);
}

int64_t lv_number_to_int(const lvNumber *n) {
    if (!n) return 0;
    return (int64_t) node_to_double(n); /* 截断语义（契约钉住） */
}

char *lv_number_to_string(const lvNumber *n) {
    if (!n) return NULL;
    switch (n->kind) {
        case lv_NUMBER_INTEGER: {
            char buf[32];
            int len = lv_snprintf(buf, sizeof(buf), "%" PRId64, n->u.i);
            if (len < 0) len = 0;
            char *s = (char *) lv_malloc((size_t) len + 1);
            if (!s) return NULL;
            memcpy(s, buf, (size_t) len + 1);
            return s;
        }
        case lv_NUMBER_RATIONAL:
            return lv_mpq_to_string(n->u.q, true); /* 规范形：分母 1 省略 */
        case lv_NUMBER_FLOAT: {
            char buf[64];
            int len = lv_snprintf(buf, sizeof(buf), "%.17g", n->u.f);
            if (len < 0) len = 0;
            char *s = (char *) lv_malloc((size_t) len + 1);
            if (!s) return NULL;
            memcpy(s, buf, (size_t) len + 1);
            return s;
        }
        default:
            return NULL;
    }
}

bool lv_number_is_zero(const lvNumber *n) {
    if (!n) return false;
    switch (n->kind) {
        case lv_NUMBER_INTEGER:  return n->u.i == 0;
        case lv_NUMBER_FLOAT:    return n->u.f == 0.0;
        case lv_NUMBER_RATIONAL: return mpq_sgn(n->u.q) == 0;
        default:                 return false;
    }
}

bool lv_number_is_one(const lvNumber *n) {
    if (!n) return false;
    switch (n->kind) {
        case lv_NUMBER_INTEGER:  return n->u.i == 1;
        case lv_NUMBER_FLOAT:    return n->u.f == 1.0;
        case lv_NUMBER_RATIONAL: return mpq_cmp_ui(n->u.q, 1, 1) == 0;
        default:                 return false;
    }
}

bool lv_number_is_negative(const lvNumber *n) {
    if (!n) return false;
    switch (n->kind) {
        case lv_NUMBER_INTEGER:  return n->u.i < 0;
        case lv_NUMBER_FLOAT:    return n->u.f < 0.0;
        case lv_NUMBER_RATIONAL: return mpq_sgn(n->u.q) < 0;
        default:                 return false;
    }
}

bool lv_number_is_positive(const lvNumber *n) {
    if (!n) return false;
    if (lv_number_is_zero(n)) return false;
    return !lv_number_is_negative(n);
}

bool lv_number_is_integer(const lvNumber *n) {
    if (!n) return false;
    if (n->kind == lv_NUMBER_INTEGER) return true;
    if (n->kind == lv_NUMBER_RATIONAL) {
        return mpz_cmp_ui(mpq_denref(n->u.q), 1) == 0;
    }
    /* float：整值判定（与旧 double 检查一致） */
    double d = n->u.f;
    return fabs(d - (double) (int64_t) d) < 1e-9;
}

lvNumberType lv_number_type(const lvNumber *n) {
    if (!n) return lv_NUMBER_RATIONAL;
    return (lvNumberType) n->kind;
}

uint64_t lv_number_hash(const lvNumber *n) {
    if (!n) return 0;
    /* 哈希统一取「double 表示位」：保证 eq → 同哈希 不变式跨 kind 成立
     * （如 int 2 与 rational 6/3、float 2.0 同值同哈希）；碰撞容忍。 */
    union { double d; uint64_t u; } cv;
    switch (n->kind) {
        case lv_NUMBER_INTEGER:  cv.d = (double) n->u.i;   return cv.u;
        case lv_NUMBER_RATIONAL: cv.d = mpq_get_d(n->u.q); return cv.u;
        case lv_NUMBER_FLOAT:    cv.d = n->u.f;            return cv.u;
        default:                 return 0;
    }
}

lvNumber *lv_number_clone(const lvNumber *n) {
    if (!n) return NULL;
    switch (n->kind) {
        case lv_NUMBER_INTEGER:  return new_int(n->u.i);
        case lv_NUMBER_FLOAT:    return new_float(n->u.f);
        case lv_NUMBER_RATIONAL: return new_rat_from_mpq(n->u.q);
        default:                 return NULL;
    }
}

void lv_number_destroy(lvNumber *n) {
    if (!n) return;
    if (n->framed) return; /* 帧对象：frame_end 统一回收，逐对象 destroy 为无害空操作 */
    node_release(n);
}

/* ============================================================
 * 工厂函数
 * ============================================================ */

lvNumber *lv_number_from_rational(int64_t num, uint64_t den) {
    if (den == 0) return NULL;
    lvNumber *n = node_new(lv_NUMBER_RATIONAL);
    if (!n) return NULL;
    mpq_set_si(n->u.q, num, (unsigned long) den);
    return n;
}

lvNumber *lv_number_from_double(double val) {
    return new_float(val);
}

lvNumber *lv_number_from_int(int64_t val) {
    return new_int(val);
}

lvNumber *lv_number_from_string(const char *str) {
    if (!str) return NULL;

    /* 有理数优先（与旧行为一致：整数串 → RATIONAL） */
    lvRational *r = lv_rational_from_string(str);
    if (r) {
        lvNumber *n = node_new(lv_NUMBER_RATIONAL);
        if (n) {
            mpq_set(n->u.q, r->value);
        }
        lv_rational_destroy(&r);
        return n;
    }

    /* 严格双精度整串消费（与旧 lv_parse_double_strict 一致） */
    double d = 0.0;
    if (lv_parse_double_strict(str, &d) == 0) {
        return new_float(d);
    }

    return NULL;
}

/* ============================================================
 * 类型信息（名称契约钉住：勿改已有字符串）
 * ============================================================ */

const char *lv_number_type_name(lvNumberType type) {
    static const char *const kNumberTypeNames[] = {
        [lv_NUMBER_RATIONAL]  = "Rational",
        [lv_NUMBER_ALGEBRAIC] = "Algebraic",
        [lv_NUMBER_INTERVAL]  = "Interval",
        [lv_NUMBER_FLOAT]     = "Float",
        [lv_NUMBER_INTEGER]   = "Integer",
        [lv_NUMBER_REAL_MPFR] = "RealMpfr",
    };
    if ((unsigned) type < sizeof(kNumberTypeNames) / sizeof(kNumberTypeNames[0])
        && kNumberTypeNames[type]) {
        return kNumberTypeNames[type];
    }
    return "Unknown";
}
