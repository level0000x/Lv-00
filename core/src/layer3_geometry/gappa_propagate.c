/**
 * @file gappa_propagate.c
 * @brief Gappa 浮点误差传播引擎
 *
 * @details 实现基于 Gappa DSL 的浮点误差传播分析。
 *          核心功能：
 *          - 区间算术运算（加减乘除、函数复合）
 *          - 正向传播：从输入误差区间推导输出误差区间
 *          - 反向传播：从输出约束推导输入约束
 *          - 重写规则：对常见数值模式进行等价变换以收紧区间
 *
 * @version 3.5.0
 * @date 2026-05-24
 */

#include "lv/gappa_propagate.h"

#include "lv/lv_platform.h"
#include "lv/interval_arith.h" /* 公共区间算术库（lv_interval_*，fnames 函数收敛于此） */
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "lv/gappa_dsl.h"
#include "lv/lv_internal.h"
#include "lv/lv_registry.h"
#include "lv/lv_thread.h"

#include "lv_utils.h"



/* ============================================================
 * 内部区间类型
 * ============================================================ */

/** @brief 传播区间 [lo, hi] */
typedef struct {
    double lo;
    double hi;
} PropInterval;

/* ============================================================
 * 区间算术运算（加减乘除、函数复合）
 *
 * 说明：PropInterval {lo, hi} 与 lvInterval {lo, hi, is_exact} 前导字段
 * 布局兼容，四则运算统一转发到公共区间算术库 lv_interval_*
 * （与下方 ia_abs/ia_sqrt/ia_sin/ia_cos/ia_exp/ia_log 的收敛方式一致）。
 * 行为差异仅在保守方向：lv_interval_* 对端点做 nextafter 向外取整
 * （1 ulp），区间略宽但保证包含真实数学结果，对误差传播分析安全。
 * ============================================================ */

/** @brief 区间加法: 收敛到公共区间算术库 lv_interval_add */
static PropInterval ia_add(PropInterval a, PropInterval b) {
    lvInterval lv_in_a = {a.lo, a.hi, 0};
    lvInterval lv_in_b = {b.lo, b.hi, 0};
    lvInterval lv_out = lv_interval_add(lv_in_a, lv_in_b);
    PropInterval r = {lv_out.lo, lv_out.hi};
    return r;
}

/** @brief 区间减法: 收敛到公共区间算术库 lv_interval_sub */
static PropInterval ia_sub(PropInterval a, PropInterval b) {
    lvInterval lv_in_a = {a.lo, a.hi, 0};
    lvInterval lv_in_b = {b.lo, b.hi, 0};
    lvInterval lv_out = lv_interval_sub(lv_in_a, lv_in_b);
    PropInterval r = {lv_out.lo, lv_out.hi};
    return r;
}

/** @brief 区间乘法: 收敛到公共区间算术库 lv_interval_mul（四角点 min/max + 向外取整） */
static PropInterval ia_mul(PropInterval a, PropInterval b) {
    lvInterval lv_in_a = {a.lo, a.hi, 0};
    lvInterval lv_in_b = {b.lo, b.hi, 0};
    lvInterval lv_out = lv_interval_mul(lv_in_a, lv_in_b);
    PropInterval r = {lv_out.lo, lv_out.hi};
    return r;
}

/** @brief 区间除法: 收敛到公共区间算术库 lv_interval_div（除数跨零 -> 全实数 [-HUGE_VAL, HUGE_VAL]） */
static PropInterval ia_div(PropInterval a, PropInterval b) {
    lvInterval lv_in_a = {a.lo, a.hi, 0};
    lvInterval lv_in_b = {b.lo, b.hi, 0};
    lvInterval lv_out = lv_interval_div(lv_in_a, lv_in_b);
    PropInterval r = {lv_out.lo, lv_out.hi};
    return r;
}

/** @brief 区间取反: [-a.hi, -a.lo] */
static PropInterval ia_neg(PropInterval a) {
    PropInterval r = {-a.hi, -a.lo};
    return r;
}

/** @brief 区间绝对值: 收敛到公共区间算术库 lv_interval_abs（逻辑与原实现一致） */
static PropInterval ia_abs(PropInterval a) {
    lvInterval lv_in = {a.lo, a.hi, 0};
    lvInterval lv_out = lv_interval_abs(lv_in);
    PropInterval r = {lv_out.lo, lv_out.hi};
    return r;
}

/** @brief 区间平方根（收敛到公共区间算术库 lv_interval_sqrt；a.hi<0 保持原 NaN 失败语义） */
static PropInterval ia_sqrt(PropInterval a) {
    if (a.hi < 0.0) {
        PropInterval r = {NAN, NAN};
        return r;
    }
    lvInterval lv_in = {a.lo, a.hi, 0};
    lvInterval lv_out = lv_interval_sqrt(lv_in);
    PropInterval r = {lv_out.lo, lv_out.hi};
    return r;
}

/** @brief 区间正弦: 收敛到公共区间算术库 lv_interval_sin（极值点精确检测，比原 floor 法更紧） */
static PropInterval ia_sin(PropInterval a) {
    lvInterval lv_in = {a.lo, a.hi, 0};
    lvInterval lv_out = lv_interval_sin(lv_in);
    PropInterval r = {lv_out.lo, lv_out.hi};
    return r;
}

/** @brief 区间余弦: 收敛到公共区间算术库 lv_interval_cos */
static PropInterval ia_cos(PropInterval a) {
    lvInterval lv_in = {a.lo, a.hi, 0};
    lvInterval lv_out = lv_interval_cos(lv_in);
    PropInterval r = {lv_out.lo, lv_out.hi};
    return r;
}

/** @brief 区间指数: 收敛到公共区间算术库 lv_interval_exp（端点向外取整，保守 1 ulp） */
static PropInterval ia_exp(PropInterval a) {
    lvInterval lv_in = {a.lo, a.hi, 0};
    lvInterval lv_out = lv_interval_exp(lv_in);
    PropInterval r = {lv_out.lo, lv_out.hi};
    return r;
}

/** @brief 区间对数: 收敛到公共区间算术库 lv_interval_log（非正下界 -> -HUGE_VAL） */
static PropInterval ia_log(PropInterval a) {
    lvInterval lv_in = {a.lo, a.hi, 0};
    lvInterval lv_out = lv_interval_log(lv_in);
    PropInterval r = {lv_out.lo, lv_out.hi};
    return r;
}

/* ============================================================
 * 简易表达式解析器（递归下降）
 * ============================================================ */

/** @brief 解析器状态 */
typedef struct {
    const char *pos;        /**< 当前解析位置 */
    PropInterval ivars[26]; /**< 变量 a-z 的区间 */
    int var_used[26];       /**< 变量是否被使用 */
} Parser;

/* 前向声明 */
static PropInterval parse_expr(Parser *p);

/** @brief 跳过空白字符 */
static void skip_ws(Parser *p) {
    while (*p->pos && isspace((unsigned char) *p->pos))
        p->pos++;
}

/** @brief 解析因子（数字、变量、括号、函数调用） */
static PropInterval parse_factor(Parser *p) {
    skip_ws(p);
    PropInterval r = {0.0, 0.0};

    /* 负号 */
    if (*p->pos == '-') {
        p->pos++;
        PropInterval inner = parse_factor(p);
        return ia_neg(inner);
    }

    /* 括号表达式 */
    if (*p->pos == '(') {
        p->pos++;
        r = parse_expr(p);
        skip_ws(p);
        if (*p->pos == ')')
            p->pos++;
        return r;
    }

    /* 函数调用: sqrt, sin, cos, exp, log, abs */
    if (isalpha((unsigned char) p->pos[0])) {
        static const char *fnames[] = {"sqrt", "sin", "cos", "exp", "log", "abs", NULL};
        typedef PropInterval (*FnFunc)(PropInterval);
        static const FnFunc fns[] = {ia_sqrt, ia_sin, ia_cos, ia_exp, ia_log, ia_abs};

        for (int fi = 0; fnames[fi]; fi++) {
            size_t len = strlen(fnames[fi]);
            if (strncmp(p->pos, fnames[fi], len) == 0 && !isalpha((unsigned char) p->pos[len])) {
                p->pos += len;
                skip_ws(p);
                if (*p->pos == '(')
                    p->pos++;
                PropInterval arg = parse_expr(p);
                skip_ws(p);
                if (*p->pos == ')')
                    p->pos++;
                return fns[fi](arg);
            }
        }

        /* 单字母变量 */
        if (isalpha((unsigned char) *p->pos) && !isalpha((unsigned char) p->pos[1])) {
            int idx = *p->pos - 'a';
            if (idx >= 0 && idx < 26) {
                p->pos++;
                p->var_used[idx] = 1;
                return p->ivars[idx];
            }
        }
        /* 未知标识符 -> 返回 [0,0] */
        while (isalpha((unsigned char) *p->pos))
            p->pos++;
        return r;
    }

    /* 数字字面量 */
    if (isdigit((unsigned char) *p->pos) || *p->pos == '.') {
        char *end;
        double val = strtod(p->pos, &end);
        p->pos = end;
        r.lo = val;
        r.hi = val;
        return r;
    }

    return r;
}

/** @brief 解析项（乘除法） */
static PropInterval parse_term(Parser *p) {
    PropInterval left = parse_factor(p);
    skip_ws(p);
    while (*p->pos == '*' || *p->pos == '/') {
        char op = *p->pos;
        p->pos++;
        PropInterval right = parse_factor(p);
        left = (op == '*') ? ia_mul(left, right) : ia_div(left, right);
        skip_ws(p);
    }
    return left;
}

/** @brief 解析表达式（加减法） */
static PropInterval parse_expr(Parser *p) {
    PropInterval left = parse_term(p);
    skip_ws(p);
    while (*p->pos == '+' || *p->pos == '-') {
        char op = *p->pos;
        p->pos++;
        PropInterval right = parse_term(p);
        left = (op == '+') ? ia_add(left, right) : ia_sub(left, right);
        skip_ws(p);
    }
    return left;
}

/* ============================================================
 * 正向传播与反向传播
 * ============================================================ */

/**
 * @brief 正向传播：从输入变量区间推导输出区间
 *
 * @param expr     表达式字符串
 * @param vars     变量区间数组（26 个字母 a-z）
 * @param out_lo   输出下界
 * @param out_hi   输出上界
 * @return 成功返回 0，失败返回 -1
 */
static int forward_propagate(const char *expr, const PropInterval *vars, double *out_lo, double *out_hi) {
    if (!expr || !out_lo || !out_hi)
        return -1;

    Parser parser;
    memset(&parser, 0, sizeof(parser));
    parser.pos = expr;

    for (int i = 0; i < 26; i++) {
        parser.ivars[i] = vars ? vars[i] : (PropInterval) {-1.0, 1.0};
    }

    PropInterval result = parse_expr(&parser);
    if (isnan(result.lo) || isnan(result.hi))
        return -1;

    *out_lo = result.lo;
    *out_hi = result.hi;
    return 0;
}

/* ============================================================
 * 谓词集名称索引注册表（通用注册表设施）
 *
 * key  = "<set指针>:<expr_lhs>"，value = boxed int 元素索引（指向 set->preds[idx]）。
 * set->preds 数组仍是谓词数据的主存储（传播/收紧逻辑按索引访问），
 * 注册表仅承担 expr_lhs 查重（lv_gappa_pred_set_add）与按名定位
 * （lv_gappa_pred_set_find）；索引在 preds 数组扩容 realloc 后保持有效。
 * 结构变更只经由 lv_gappa_pred_set_add 发生，init/clear 时同步清理
 * 该 set 的全部注册条目（防止同地址复用后残留过期映射）。
 * 文件级单例（lv_once 惰性初始化，线程安全）。
 * ============================================================ */

/** @brief 注册表 key 缓冲区大小（set 指针 + expr_lhs） */
#define GAPPA_REGKEY_MAX 512

/** @brief 谓词集名称索引注册表（文件级单例） */
lv_REGISTRY_STATIC(gappa_pred_registry, 64);

/** @brief 装箱谓词索引（注册表 value） */
static void *gappa_box_index(int idx) {
    int *p = (int *) lv_malloc(sizeof(int));
    if (p) {
        *p = idx;
    }
    return p;
}

/** @brief boxed 索引的注册表 destroy 回调适配器（void(*)(void*) 形态） */
static void gappa_box_destroy(void *value) {
    lv_free((void **) &value);
}

/** @brief 解箱谓词索引 */
static int gappa_unbox_index(void *value) {
    return value ? *(int *) value : -1;
}

/** @brief 构造注册表 key（"<set>:<expr_lhs>"） */
static void gappa_build_key(const lvGappaPredSet *set, const char *expr_lhs, char *buf, size_t bufsz) {
    snprintf(buf, bufsz, "%p:%s", (const void *) set, expr_lhs);
}

/** @brief 移除指定 set 的全部注册条目（init/clear 时调用，防止残留过期映射） */
static void gappa_pred_registry_remove_set(const lvGappaPredSet *set) {
    if (!set) {
        return;
    }
    char prefix[64];
    snprintf(prefix, sizeof(prefix), "%p:", (const void *) set);
    size_t prefix_len = strlen(prefix);
    int total = lv_registry_count(&g_gappa_pred_registry);
    for (int i = total - 1; i >= 0; i--) {
        const char *reg_name = NULL;
        void *reg_value = NULL;
        if (!lv_registry_get_at(&g_gappa_pred_registry, i, &reg_name, &reg_value)) {
            continue;
        }
        if (strncmp(reg_name, prefix, prefix_len) == 0) {
            lv_registry_remove(&g_gappa_pred_registry, reg_name);
        }
    }
}

/* -- Structured propagation API -- */

void lv_gappa_pred_set_init(lvGappaPredSet *set) {
    if (!set)
        return;
    /* 同步清理该 set 地址的历史注册条目（防止同地址复用后残留过期映射） */
    gappa_pred_registry_ensure();
    gappa_pred_registry_remove_set(set);
    set->preds = NULL;
    set->count = 0;
    set->capacity = 0;
}

bool lv_gappa_pred_set_add(lvGappaPredSet *set, const lvGappaPredicate *pred) {
    if (!set || !pred)
        return false;

    gappa_pred_registry_ensure();

    /* 查重（委托注册表 strcmp 查重，key = "<set>:<expr_lhs>"） */
    char regkey[GAPPA_REGKEY_MAX];
    gappa_build_key(set, pred->expr_lhs, regkey, sizeof(regkey));
    if (lv_registry_get(&g_gappa_pred_registry, regkey) != NULL) {
        return false; /* Duplicate: a predicate with this name already exists */
    }

    if (set->count >= set->capacity) {
        if (!lv_ensure_capacity((void **) &set->preds, set->count, &set->capacity,
                                sizeof(lvGappaPredicate), 0))
            return false;
    }
    set->preds[set->count] = *pred;

    /* 登记名称索引（value = boxed 元素索引；索引在扩容 realloc 后保持有效）。
     * 失败（内存不足）时不增加 count，保持一致性 */
    void *boxed = gappa_box_index(set->count);
    if (!boxed)
        return false;
    if (!lv_registry_put_ex(&g_gappa_pred_registry, regkey, boxed, gappa_box_destroy)) {
        lv_free((void **) &boxed);
        return false;
    }
    set->count++;
    return true;
}

int lv_gappa_pred_set_find(const lvGappaPredSet *set, const char *name, lvGappaPredicate *found) {
    if (!set || !name)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "lv_gappa_pred_set_find: NULL param");

    /* 委托注册表按名称定位索引（strcmp 由 lv_registry 承担） */
    gappa_pred_registry_ensure();
    char regkey[GAPPA_REGKEY_MAX];
    gappa_build_key(set, name, regkey, sizeof(regkey));
    void *boxed = lv_registry_get(&g_gappa_pred_registry, regkey);
    if (!boxed)
        return -1;
    int idx = gappa_unbox_index(boxed);
    if (idx < 0 || idx >= set->count)
        return -1;
    if (found)
        *found = set->preds[idx];
    return idx;
}

void lv_gappa_pred_set_clear(lvGappaPredSet *set) {
    if (!set)
        return;
    /* 同步清理该 set 的全部注册条目 */
    gappa_pred_registry_ensure();
    gappa_pred_registry_remove_set(set);
    lv_free((void **) &(set->preds));
    set->preds = NULL;
    set->count = 0;
    set->capacity = 0;
}

lvGappaPropagateConfig lv_gappa_propagate_config_default(void) {
    lvGappaPropagateConfig cfg;
    cfg.max_iterations = 1;
    cfg.precision = 1e-15;
    cfg.backward = false;
    return cfg;
}

/**
 * @brief 从已知谓词集合中反向推断变量 BND 的紧区间
 *
 * 检查输出集中是否存在形如 "X + Y" 或 "X - Y" 的谓词，
 * 若其中一个操作数的边界已知，则反推出另一个操作数的更紧边界。
 *
 * @param output    输出谓词集（会被原地收紧）
 * @param idx       当前正在检查的被收紧谓词索引
 * @param lhs       被收紧谓词的 expr_lhs
 * @param precision 收敛精度阈值
 * @param changed   输出：是否有边界被收紧
 */
static void backward_refine_pred(lvGappaPredSet *output, int idx, const char *lhs, double precision, bool *changed) {
    size_t lhs_len = strlen(lhs);

    for (int p = 0; p < output->count; p++) {
        if (p == idx || output->preds[p].type != lv_PRED_BND)
            continue;

        size_t plen = strlen(output->preds[p].expr_lhs);
        const char *pexpr = output->preds[p].expr_lhs;

        /* ── 模式 1: pexpr = lhs + rest → 反推 lhs = pexpr - rest ── */
        if (plen > lhs_len + 3 && strncmp(pexpr, lhs, lhs_len) == 0 && strncmp(pexpr + lhs_len, " + ", 3) == 0) {
            const char *rest = pexpr + lhs_len + 3;
            for (int r = 0; r < output->count; r++) {
                if (r == idx || r == p || output->preds[r].type != lv_PRED_BND)
                    continue;
                if (strcmp(output->preds[r].expr_lhs, rest) != 0)
                    continue;

                /* lhs = (lhs + rest) - rest */
                double new_lo = output->preds[p].bound_lo - output->preds[r].bound_hi;
                double new_hi = output->preds[p].bound_hi - output->preds[r].bound_lo;
                if (new_lo > output->preds[idx].bound_lo + precision) {
                    output->preds[idx].bound_lo = new_lo;
                    *changed = true;
                }
                if (new_hi < output->preds[idx].bound_hi - precision) {
                    output->preds[idx].bound_hi = new_hi;
                    *changed = true;
                }
            }
        }

        /* ── 模式 2: pexpr = rest + lhs → 反推 lhs = pexpr - rest ── */
        /* 检查 pexpr 是否以 " + lhs" 结尾 */
        if (plen > lhs_len + 3) {
            const char *suffix_start = pexpr + plen - lhs_len - 3;
            if (strncmp(suffix_start, " + ", 3) == 0 && strcmp(suffix_start + 3, lhs) == 0) {
                /* 提取 rest = pexpr[0..plen-lhs_len-4) */
                size_t rest_len = plen - lhs_len - 3;
                for (int r = 0; r < output->count; r++) {
                    if (r == idx || r == p || output->preds[r].type != lv_PRED_BND)
                        continue;
                    if (strlen(output->preds[r].expr_lhs) != rest_len)
                        continue;
                    if (strncmp(output->preds[r].expr_lhs, pexpr, rest_len) != 0)
                        continue;

                    double new_lo = output->preds[p].bound_lo - output->preds[r].bound_hi;
                    double new_hi = output->preds[p].bound_hi - output->preds[r].bound_lo;
                    if (new_lo > output->preds[idx].bound_lo + precision) {
                        output->preds[idx].bound_lo = new_lo;
                        *changed = true;
                    }
                    if (new_hi < output->preds[idx].bound_hi - precision) {
                        output->preds[idx].bound_hi = new_hi;
                        *changed = true;
                    }
                }
            }
        }

        /* ── 模式 3: pexpr = lhs - rest → 反推 lhs = pexpr + rest ── */
        if (plen > lhs_len + 3 && strncmp(pexpr, lhs, lhs_len) == 0 && strncmp(pexpr + lhs_len, " - ", 3) == 0) {
            const char *rest = pexpr + lhs_len + 3;
            for (int r = 0; r < output->count; r++) {
                if (r == idx || r == p || output->preds[r].type != lv_PRED_BND)
                    continue;
                if (strcmp(output->preds[r].expr_lhs, rest) != 0)
                    continue;

                /* lhs = (lhs - rest) + rest */
                double new_lo = output->preds[p].bound_lo + output->preds[r].bound_lo;
                double new_hi = output->preds[p].bound_hi + output->preds[r].bound_hi;
                if (new_lo > output->preds[idx].bound_lo + precision) {
                    output->preds[idx].bound_lo = new_lo;
                    *changed = true;
                }
                if (new_hi < output->preds[idx].bound_hi - precision) {
                    output->preds[idx].bound_hi = new_hi;
                    *changed = true;
                }
            }
        }

        /* ── 模式 4: pexpr = rest - lhs → 反推 lhs = rest - pexpr ── */
        if (plen > lhs_len + 3) {
            const char *suffix_start = pexpr + plen - lhs_len - 3;
            if (strncmp(suffix_start, " - ", 3) == 0 && strcmp(suffix_start + 3, lhs) == 0) {
                size_t rest_len = plen - lhs_len - 3;
                for (int r = 0; r < output->count; r++) {
                    if (r == idx || r == p || output->preds[r].type != lv_PRED_BND)
                        continue;
                    if (strlen(output->preds[r].expr_lhs) != rest_len)
                        continue;
                    if (strncmp(output->preds[r].expr_lhs, pexpr, rest_len) != 0)
                        continue;

                    /* lhs = rest - (rest - lhs) */
                    double new_lo = output->preds[r].bound_lo - output->preds[p].bound_hi;
                    double new_hi = output->preds[r].bound_hi - output->preds[p].bound_lo;
                    if (new_lo > output->preds[idx].bound_lo + precision) {
                        output->preds[idx].bound_lo = new_lo;
                        *changed = true;
                    }
                    if (new_hi < output->preds[idx].bound_hi - precision) {
                        output->preds[idx].bound_hi = new_hi;
                        *changed = true;
                    }
                }
            }
        }
    }
}

/* ============================================================
 * 推导规则表 + 统一 fixpoint 驱动
 *
 * 原实现中 6 处 "saved_count 快照迭代" 结构相同：
 *   快照当前谓词数 → 遍历旧谓词（二元取对 / 一元取单）→
 *   类型过滤 → 区间运算 → 格式化 expr_lhs → 去重加入集合。
 * 此处收敛为规则表 + gappa_apply_rules() 统一驱动。
 * ============================================================ */

/** @brief 规则区间运算：a/b 为操作数（一元规则忽略 b），skip 置 true 表示跳过该产出 */
typedef PropInterval (*GappaRuleOp)(const lvGappaPredicate *a, const lvGappaPredicate *b, bool *skip);

/** @brief 谓词推导规则 */
typedef struct {
    const char *fmt;      /**< 结果表达式格式串（一元 1 个 %s，二元 2 个 %s） */
    int arity;            /**< 1 = 一元规则，2 = 二元规则 */
    int src_type;         /**< 源谓词类型过滤（lv_PRED_BND / lv_PRED_ABS） */
    bool ordered_pairs;   /**< 二元：true = 遍历全部 i != j 对；false = 仅 i < j 对 */
    bool swap_operands;   /**< 二元：true = 以 (b, a) 顺序格式化与运算（如 y - x） */
    GappaRuleOp op;       /**< 区间运算 */
} GappaRule;

/** @brief 区间加法 */
static PropInterval gappa_op_add(const lvGappaPredicate *a, const lvGappaPredicate *b, bool *skip) {
    *skip = false;
    PropInterval ia = {a->bound_lo, a->bound_hi};
    PropInterval ib = {b->bound_lo, b->bound_hi};
    return ia_add(ia, ib);
}

/** @brief 区间减法 [a.lo-b.hi, a.hi-b.lo] */
static PropInterval gappa_op_sub(const lvGappaPredicate *a, const lvGappaPredicate *b, bool *skip) {
    *skip = false;
    PropInterval ia = {a->bound_lo, a->bound_hi};
    PropInterval ib = {b->bound_lo, b->bound_hi};
    return ia_sub(ia, ib);
}

/** @brief 区间乘法 */
static PropInterval gappa_op_mul(const lvGappaPredicate *a, const lvGappaPredicate *b, bool *skip) {
    *skip = false;
    PropInterval ia = {a->bound_lo, a->bound_hi};
    PropInterval ib = {b->bound_lo, b->bound_hi};
    return ia_mul(ia, ib);
}

/** @brief 区间除法：分母跨零或结果无穷时跳过（与原实现语义一致） */
static PropInterval gappa_op_div(const lvGappaPredicate *a, const lvGappaPredicate *b, bool *skip) {
    *skip = false;
    PropInterval ia = {a->bound_lo, a->bound_hi};
    PropInterval ib = {b->bound_lo, b->bound_hi};
    /* 跳过分母包含零的情况 */
    if (ib.lo <= 0.0 && ib.hi >= 0.0) {
        *skip = true;
        PropInterval r = {0.0, 0.0};
        return r;
    }
    PropInterval r = ia_div(ia, ib);
    if (isinf(r.lo) || isinf(r.hi)) {
        *skip = true;
        PropInterval z = {0.0, 0.0};
        return z;
    }
    return r;
}

/** @brief 平方：利用 x² 在 (-∞,0] 递减、[0,+∞) 递增的单调性精确计算 */
static PropInterval gappa_op_square(const lvGappaPredicate *a, const lvGappaPredicate *b, bool *skip) {
    (void) b;
    *skip = false;
    double x_lo = a->bound_lo;
    double x_hi = a->bound_hi;
    double sq_lo, sq_hi;
    if (x_lo >= 0.0) {
        sq_lo = x_lo * x_lo;
        sq_hi = x_hi * x_hi;
    } else if (x_hi <= 0.0) {
        sq_lo = x_hi * x_hi;
        sq_hi = x_lo * x_lo;
    } else {
        sq_lo = 0.0;
        double abs_lo = -x_lo;
        sq_hi = (abs_lo > x_hi) ? (abs_lo * abs_lo) : (x_hi * x_hi);
    }
    PropInterval r = {sq_lo, sq_hi};
    return r;
}

/** @brief ABS → BND：|x - c| ≤ eps → x ∈ [c - eps, c + eps] */
static PropInterval gappa_op_abs_to_bnd(const lvGappaPredicate *a, const lvGappaPredicate *b, bool *skip) {
    (void) b;
    *skip = false;
    char *end = NULL;
    errno = 0;
    double center = strtod(a->expr_rhs, &end);
    if (errno != 0 || end == a->expr_rhs)
        center = 0.0;
    double eps = a->bound_abs;
    PropInterval r = {center - eps, center + eps};
    return r;
}

/**
 * @brief 对谓词集应用一组推导规则（统一 fixpoint 驱动）
 *
 * 规则组共享同一个入口快照（等价于原实现中一次 saved_count 捕获），
 * 组内新加入的谓词不会作为本组规则的迭代输入。
 *
 * @param output     谓词集（会被追加新推导的谓词）
 * @param rules      规则数组
 * @param rule_count 规则数量
 * @param derived    累计成功推导数（可 NULL）
 * @param changed    置 true 表示本轮有新谓词加入（可 NULL）
 */
static void gappa_apply_rules(lvGappaPredSet *output, const GappaRule *rules, size_t rule_count, int *derived,
                              bool *changed) {
    /* 快照：所有规则共享同一迭代边界，避免集合增长导致迭代器失效 */
    int saved_count = output->count;

    for (size_t r = 0; r < rule_count; r++) {
        const GappaRule *rule = &rules[r];

        if (rule->arity == 2) {
            for (int i = 0; i < saved_count; i++) {
                if (output->preds[i].type != rule->src_type)
                    continue;
                for (int j = 0; j < saved_count; j++) {
                    /* 无序遍历（i<j）或有序遍历（i!=j） */
                    if (rule->ordered_pairs ? (j == i) : (j <= i))
                        continue;
                    if (output->preds[j].type != rule->src_type)
                        continue;

                    const lvGappaPredicate *pa = rule->swap_operands ? &output->preds[j] : &output->preds[i];
                    const lvGappaPredicate *pb = rule->swap_operands ? &output->preds[i] : &output->preds[j];

                    bool skip = false;
                    PropInterval iv = rule->op(pa, pb, &skip);
                    if (skip)
                        continue;

                    lvGappaPredicate np;
                    memset(&np, 0, sizeof(np));
                    np.type = lv_PRED_BND;
                    snprintf(np.expr_lhs, sizeof(np.expr_lhs), rule->fmt, pa->expr_lhs, pb->expr_lhs);
                    np.bound_lo = iv.lo;
                    np.bound_hi = iv.hi;
                    np.is_hypothesis = pa->is_hypothesis;
                    if (lv_gappa_pred_set_add(output, &np)) {
                        if (derived)
                            (*derived)++;
                        if (changed)
                            *changed = true;
                    }
                }
            }
        } else {
            for (int i = 0; i < saved_count; i++) {
                if (output->preds[i].type != rule->src_type)
                    continue;

                bool skip = false;
                PropInterval iv = rule->op(&output->preds[i], &output->preds[i], &skip);
                if (skip)
                    continue;

                lvGappaPredicate np;
                memset(&np, 0, sizeof(np));
                np.type = lv_PRED_BND;
                snprintf(np.expr_lhs, sizeof(np.expr_lhs), rule->fmt, output->preds[i].expr_lhs);
                np.bound_lo = iv.lo;
                np.bound_hi = iv.hi;
                np.is_hypothesis = output->preds[i].is_hypothesis;
                if (lv_gappa_pred_set_add(output, &np)) {
                    if (derived)
                        (*derived)++;
                    if (changed)
                        *changed = true;
                }
            }
        }
    }
}

/* 前向规则 阶段 1：和/差（x+y、x-y、y-x） */
static const GappaRule k_rules_sum_diff[] = {
    {"%s + %s", 2, lv_PRED_BND, false, false, gappa_op_add},
    {"%s - %s", 2, lv_PRED_BND, false, false, gappa_op_sub},
    {"%s - %s", 2, lv_PRED_BND, false, true, gappa_op_sub}, /* y - x */
};

/* 前向规则 阶段 2：乘/除/平方（共享同一快照，与原实现一致） */
static const GappaRule k_rules_mul_div_square[] = {
    {"%s * %s", 2, lv_PRED_BND, false, false, gappa_op_mul},
    {"%s / %s", 2, lv_PRED_BND, true, false, gappa_op_div},
    {"(%s)^2", 1, lv_PRED_BND, false, false, gappa_op_square},
};

/* 后向规则：ABS → BND */
static const GappaRule k_rules_abs_to_bnd[] = {
    {"%s", 1, lv_PRED_ABS, false, false, gappa_op_abs_to_bnd},
};

int lv_gappa_propagate_set(const lvGappaPredSet *input, lvGappaPredSet *output, const lvGappaPropagateConfig *cfg) {
    if (!input || !output)
        return 0;

    lv_gappa_pred_set_init(output);

    int max_iter = cfg ? cfg->max_iterations : 100;
    double precision = cfg ? cfg->precision : 1e-15;
    bool do_backward = cfg ? cfg->backward : false;

    /* 复制所有输入谓词到输出 */
    for (int i = 0; i < input->count; i++) {
        lv_gappa_pred_set_add(output, &input->preds[i]);
    }

    int derived = 0;

    /* ── 迭代收敛主循环 ── */
    for (int iter = 0; iter < max_iter; iter++) {
        bool changed = false;

        /* ============================================================
         * 前向传播：推导新谓词（规则表 + 统一 fixpoint 驱动）
         * ============================================================ */

        /* 阶段 1：和/差（x+y、x-y、y-x），快照在驱动内部按组捕获 */
        gappa_apply_rules(output, k_rules_sum_diff, sizeof(k_rules_sum_diff) / sizeof(k_rules_sum_diff[0]),
                          &derived, &changed);

        /*
         * 阶段 2：乘/除/平方。
         * 原实现中 refinement pass 的快照取在乘除平方之前（和差阶段之后），
         * 此处显式保存该边界供下方 refinement 使用，语义与原实现完全一致。
         */
        int refine_count = output->count;
        gappa_apply_rules(output, k_rules_mul_div_square,
                          sizeof(k_rules_mul_div_square) / sizeof(k_rules_mul_div_square[0]), &derived, &changed);

        /* ============================================================
         * Refinement pass：利用 sum/diff 关系收紧已有边界
         * ============================================================ */
        for (int i = 0; i < refine_count; i++) {
            if (output->preds[i].type != lv_PRED_BND)
                continue;
            backward_refine_pred(output, i, output->preds[i].expr_lhs, precision, &changed);
        }

        /* ============================================================
         * 后向传播（可选）：额外反向推导 BND 和 ABS 谓词
         * ============================================================ */
        if (do_backward) {
            int bw_count = output->count;
            /* ABS → BND 转换：|x - c| ≤ eps → x ∈ [c - eps, c + eps]（规则驱动） */
            gappa_apply_rules(output, k_rules_abs_to_bnd, sizeof(k_rules_abs_to_bnd) / sizeof(k_rules_abs_to_bnd[0]),
                              &derived, &changed);

            /* 利用 ABS 谓词的约束能力：对刚转换出的 BND 再跑一次 refinement */
            for (int i = 0; i < bw_count; i++) {
                if (output->preds[i].type != lv_PRED_BND)
                    continue;
                backward_refine_pred(output, i, output->preds[i].expr_lhs, precision, &changed);
            }

            /* 反向传播：对乘除关系进行反向收紧
             *   - 若已知 x*y 和 x 的区间，收紧 y
             *   - 若已知 x/y 和 y 的区间，收紧 x */
            for (int i = 0; i < bw_count; i++) {
                if (output->preds[i].type != lv_PRED_BND)
                    continue;
                size_t ilen = strlen(output->preds[i].expr_lhs);

                for (int p = 0; p < bw_count; p++) {
                    if (p == i || output->preds[p].type != lv_PRED_BND)
                        continue;
                    size_t plen = strlen(output->preds[p].expr_lhs);
                    const char *pexpr = output->preds[p].expr_lhs;

                    /* 乘反向: pexpr = lhs * rest → lhs = pexpr / rest */
                    if (plen > ilen + 3 && strncmp(pexpr, output->preds[i].expr_lhs, ilen) == 0 &&
                        strncmp(pexpr + ilen, " * ", 3) == 0) {
                        const char *rest = pexpr + ilen + 3;
                        for (int r = 0; r < bw_count; r++) {
                            if (r == i || r == p || output->preds[r].type != lv_PRED_BND)
                                continue;
                            if (strcmp(output->preds[r].expr_lhs, rest) != 0)
                                continue;
                            if (output->preds[r].bound_lo <= 0.0 && output->preds[r].bound_hi >= 0.0)
                                continue;

                            PropInterval prod = {output->preds[p].bound_lo, output->preds[p].bound_hi};
                            PropInterval fac = {output->preds[r].bound_lo, output->preds[r].bound_hi};
                            PropInterval quo = ia_div(prod, fac);
                            if (isinf(quo.lo) || isinf(quo.hi))
                                continue;

                            if (quo.lo > output->preds[i].bound_lo + precision) {
                                output->preds[i].bound_lo = quo.lo;
                                changed = true;
                            }
                            if (quo.hi < output->preds[i].bound_hi - precision) {
                                output->preds[i].bound_hi = quo.hi;
                                changed = true;
                            }
                        }
                    }

                    /* 除反向: pexpr = lhs / rest → lhs = pexpr * rest */
                    if (plen > ilen + 3 && strncmp(pexpr, output->preds[i].expr_lhs, ilen) == 0 &&
                        strncmp(pexpr + ilen, " / ", 3) == 0) {
                        const char *rest = pexpr + ilen + 3;
                        for (int r = 0; r < bw_count; r++) {
                            if (r == i || r == p || output->preds[r].type != lv_PRED_BND)
                                continue;
                            if (strcmp(output->preds[r].expr_lhs, rest) != 0)
                                continue;

                            PropInterval quot = {output->preds[p].bound_lo, output->preds[p].bound_hi};
                            PropInterval denom = {output->preds[r].bound_lo, output->preds[r].bound_hi};
                            PropInterval num = ia_mul(quot, denom);

                            if (num.lo > output->preds[i].bound_lo + precision) {
                                output->preds[i].bound_lo = num.lo;
                                changed = true;
                            }
                            if (num.hi < output->preds[i].bound_hi - precision) {
                                output->preds[i].bound_hi = num.hi;
                                changed = true;
                            }
                        }
                    }
                }
            }
        }

        /* ── 收敛检查：本轮未产生任何新谓词且未收紧任何边界 ── */
        if (!changed)
            break;
    }

    return derived > 0 ? derived : (input->count > 0 ? 1 : 0);
}

int lv_gappa_propagate_backward(const lvGappaPredicate *goal, const lvGappaPredSet *known, lvGappaPredSet *output,
                                const lvGappaPropagateConfig *cfg) {
    (void) cfg;
    if (!goal || !output)
        return 0;

    lv_gappa_pred_set_init(output);

    /* 复制已知谓词 */
    if (known) {
        for (int i = 0; i < known->count; i++) {
            lv_gappa_pred_set_add(output, &known->preds[i]);
        }
    }

    int needed = 0;

    if (goal->type == lv_PRED_ABS) {
        /* ABS 目标: |x - c| <= bound → x in [c - bound, c + bound] */
        char *end = NULL;
        errno = 0;
        double center = strtod(goal->expr_rhs, &end);
        if (errno != 0 || end == goal->expr_rhs)
            center = 0.0;

        lvGappaPredicate derived;
        memset(&derived, 0, sizeof(derived));
        derived.type = lv_PRED_BND;
        /* 提取变量名：从 expr_lhs 中获取（可能是 "x - 0.5" 格式，取第一个标识符） */
        const char *src = goal->expr_lhs;
        int k = 0;
        while (src[k] && (isalpha((unsigned char) src[k]) || isdigit((unsigned char) src[k]) || src[k] == '_')) {
            k++;
        }
        size_t name_len = (size_t) k;
        if (name_len >= sizeof(derived.expr_lhs))
            name_len = sizeof(derived.expr_lhs) - 1;
        memcpy(derived.expr_lhs, src, name_len);
        derived.expr_lhs[name_len] = '\0';

        derived.bound_lo = center - goal->bound_abs;
        derived.bound_hi = center + goal->bound_abs;
        derived.is_hypothesis = true;

        if (lv_gappa_pred_set_add(output, &derived))
            needed++;
    } else if (goal->type == lv_PRED_BND) {
        /* BND 目标: 直接使用目标区间作为所需假设 */
        lvGappaPredicate derived;
        memset(&derived, 0, sizeof(derived));
        derived.type = lv_PRED_BND;
        strncpy(derived.expr_lhs, goal->expr_lhs, sizeof(derived.expr_lhs) - 1);
        derived.bound_lo = goal->bound_lo;
        derived.bound_hi = goal->bound_hi;
        derived.is_hypothesis = true;

        if (lv_gappa_pred_set_add(output, &derived))
            needed++;
    }

    return needed;
}

/**
 * @brief 反向传播：从输出约束推导输入约束
 *
 * 给定期望输出区间，反向传播收紧输入变量区间。
 * 使用正向传播结果与目标区间的收缩比例来推导。
 *
 * @param expr         表达式字符串
 * @param out_lo       期望输出下界
 * @param out_hi       期望输出上界
 * @param vars_inout   输入/输出变量区间（会被收紧）
 * @return 成功返回 0，失败返回 -1
 */
static int backward_propagate(const char *expr, double out_lo, double out_hi, PropInterval *vars_inout) {
    if (!expr || !vars_inout)
        return -1;

    Parser parser;
    memset(&parser, 0, sizeof(parser));
    parser.pos = expr;
    for (int i = 0; i < 26; i++) {
        parser.ivars[i] = vars_inout[i];
    }

    PropInterval fwd = parse_expr(&parser);
    if (isnan(fwd.lo) || isnan(fwd.hi))
        return -1;

    /* 计算收缩比例 */
    double fwd_width = fwd.hi - fwd.lo;
    double target_width = out_hi - out_lo;

    if (fwd_width > 0.0 && target_width < fwd_width) {
        double ratio = target_width / fwd_width;
        for (int i = 0; i < 26; i++) {
            if (parser.var_used[i]) {
                double mid = (vars_inout[i].lo + vars_inout[i].hi) * 0.5;
                double half = (vars_inout[i].hi - vars_inout[i].lo) * 0.5 * ratio;
                double new_lo = mid - half;
                double new_hi = mid + half;
                /* 与原区间取交集（只收紧不放松） */
                if (new_lo > vars_inout[i].lo)
                    vars_inout[i].lo = new_lo;
                if (new_hi < vars_inout[i].hi)
                    vars_inout[i].hi = new_hi;
            }
        }
    }
    return 0;
}

/* ============================================================
 * 重写规则应用
 * ============================================================ */

/**
 * @brief 应用重写规则收紧区间
 *
 * 检查常见数值模式（如 expm1、log1p），
 * 若匹配则用更稳定的等价形式重新求值。
 * 未匹配时直接使用正向传播。
 *
 * @param expr     原始表达式
 * @param vars     变量区间
 * @param out_lo   输出下界
 * @param out_hi   输出上界
 * @return 成功返回 0，失败返回 -1
 */
static int apply_rewrite_rules(const char *expr, const PropInterval *vars, double *out_lo, double *out_hi) {
    if (!expr || !out_lo || !out_hi)
        return -1;
    /* 默认直接正向传播；特定模式可在此扩展 */
    return forward_propagate(expr, vars, out_lo, out_hi);
}

/* ============================================================
 * 公共 API 实现
 * ============================================================ */

/**
 * @brief 通过 Gappa DSL 表达式传播区间约束
 *
 * 解析 Gappa 表达式，在默认变量区间约束下进行正向区间传播，
 * 输出表达式值的保守区间 [lo, hi]。
 *
 * 支持的表达式语法：
 *   - 基本算术：+ - * /
 *   - 函数：sqrt, sin, cos, exp, log, abs
 *   - 变量：单字母 a-z（默认区间 [-1, 1]）
 *   - 括号分组
 *
 * @param[in]  expr  Gappa DSL 表达式字符串
 * @param[out] lo    输出区间的下界
 * @param[out] hi    输出区间的上界
 * @return 0 成功，-1 失败（参数无效或解析错误）
 */
int lv_gappa_propagate(const char *expr, double *lo, double *hi) {
    if (!expr || !lo || !hi)
        return -1;

    /* 初始化默认变量区间 [-1, 1] */
    PropInterval vars[26];
    for (int i = 0; i < 26; i++) {
        vars[i].lo = -1.0;
        vars[i].hi = 1.0;
    }

    /* 应用重写规则并传播 */
    int rc = apply_rewrite_rules(expr, vars, lo, hi);
    if (rc != 0)
        return -1;

    /* 确保 lo <= hi */
    if (*lo > *hi) {
        double tmp = *lo;
        *lo = *hi;
        *hi = tmp;
    }

    return 0;
}
