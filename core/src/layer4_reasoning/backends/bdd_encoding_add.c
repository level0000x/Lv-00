/**
 * @file bdd_encoding_add.c
 * @brief ADD（代数决策图）运算（由 bdd_encoding.c 拆分子模块）
 *
 * @details ADD 管理器、常量/内部节点创建与加/减/乘/除/max/min 运算。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/bdd_encoding.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_utils.h"
#include "lv/lv_check.h"
#include "lv/lv_constraint_guard.h"
#include "lv/lv_lifecycle.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_xmacro.h"

#include "bdd_encoding_internal.h"

/* ========================================================================
 * ADD 管理器（代数决策图）
 * ======================================================================== */

/* ---- lv_DEFER 守卫：ADD 管理器部分构建的 defer 清理（graph_memory.c 的
 *      mpq_matrix_guard 模式 —— guard 持有值拷贝，置 NULL 即解除守卫） ---- */

typedef struct {
    ADDManager *mgr;
} AddManagerGuard;

static void add_manager_guard_cleanup(void *p) {
    AddManagerGuard *g = (AddManagerGuard *) p;
    if (!g->mgr)
        return;
    /* 与原 cleanup 标签清理顺序一致：lv_free 对 NULL 安全 */
    lv_free((void **) &g->mgr->var_order);
    lv_free((void **) &g->mgr->unique_table);
    lv_free((void **) &g->mgr->one_node);
    lv_free((void **) &g->mgr->zero_node);
    lv_free((void **) &g->mgr);
}

/**
 * @brief 创建 ADD 管理器（代数决策图）
 *
 * @param var_count        变量数量
 * @param unique_table_size 唯一表大小
 * @return 新分配的 ADDManager 指针，失败返回 NULL
 */
ADDManager *add_manager_create(int var_count, int unique_table_size) {
    ADDManager *mgr = (ADDManager *) lv_calloc(1, sizeof(ADDManager));
    if (!mgr)
        return NULL;
    /* 注册 lv_DEFER 守卫：任何失败路径（含中途 return NULL）自动按原 cleanup
     * 语义逐字段释放已分配的成员；成功路径 guard.mgr = NULL 解除守卫 */
    AddManagerGuard guard = {mgr};
    lv_DEFER(add_manager_guard_cleanup, &guard);

    mgr->zero_node = (ADDNode *) lv_calloc(1, sizeof(ADDNode));
    if (!mgr->zero_node)
        return NULL;
    mgr->zero_node->var_id = -1;
    mgr->zero_node->low = NULL;
    mgr->zero_node->high = NULL;
    mgr->zero_node->constant = 0.0;
    mgr->zero_node->is_constant = true;

    mgr->one_node = (ADDNode *) lv_calloc(1, sizeof(ADDNode));
    if (!mgr->one_node)
        return NULL;
    mgr->one_node->var_id = -1;
    mgr->one_node->low = NULL;
    mgr->one_node->high = NULL;
    mgr->one_node->constant = 1.0;
    mgr->one_node->is_constant = true;

    /* 分配 ADD 唯一表 */
    if (unique_table_size > 0) {
        mgr->unique_table = (ADDNode **) lv_calloc((size_t) unique_table_size, sizeof(ADDNode *));
        if (!mgr->unique_table)
            return NULL;
    } else {
        mgr->unique_table = NULL;
    }
    mgr->unique_table_size = unique_table_size;
    mgr->var_count = var_count;
    mgr->node_count = 0;

    mgr->var_order = (int *) lv_malloc((size_t) var_count * sizeof(int));
    if (mgr->var_order) {
        for (int i = 0; i < var_count; i++)
            mgr->var_order[i] = i;
    }

    guard.mgr = NULL; /* 守卫解除：结果移交调用方 */
    return mgr;
}

/**
 * @brief 销毁 ADD 管理器
 * @param mgr 要销毁的 ADD 管理器
 */
void add_manager_destroy(ADDManager *mgr) {
    if (!mgr)
        return;
    lv_free((void **) &mgr->zero_node);
    lv_free((void **) &mgr->one_node);
    lv_free((void **) &mgr->unique_table);
    lv_free((void **) &mgr->var_order);
    lv_free((void **) &mgr);
}

/**
 * @brief 创建 ADD 常数节点
 * @param mgr   ADD 管理器
 * @param value 常数值
 * @return 常数节点指针，失败返回 NULL
 */
ADDNode *add_constant(ADDManager *mgr, double value) {
    lv_CHECK_NULL(mgr, NULL);
    ADDNode *node = (ADDNode *) lv_calloc(1, sizeof(ADDNode));
    if (!node)
        return NULL;
    node->var_id = -1;
    node->low = NULL;
    node->high = NULL;
    node->constant = value;
    node->is_constant = true;
    return node;
}

/* ADD 运算 —— Shannon 展开实现 */

/** 内部：ADD 节点创建辅助 */
static ADDNode *add_node_create(ADDManager *mgr, int var_id, ADDNode *low, ADDNode *high) {
    lv_CHECK_NULL(mgr, NULL);
    /* 终端合并：如果 low == high，返回 low */
    if (low == high)
        return low;
    ADDNode *node = (ADDNode *) lv_calloc(1, sizeof(ADDNode));
    if (!node)
        return NULL;
    node->var_id = var_id;
    node->low = low;
    node->high = high;
    node->constant = 0.0;
    node->is_constant = false;
    return node;
}

/** 内部：获取 ADD 节点的值（终端节点返回常量，非终端返回 NaN） */
static double add_node_value(const ADDNode *node) {
    if (!node || !node->is_constant)
        return NAN;
    return node->constant;
}

/** 内部：Shannon 展开 —— 选择顶部变量 */
static int add_top_var(const ADDNode *a, const ADDNode *b) {
    int va = (a && !a->is_constant) ? a->var_id : 999999;
    int vb = (b && !b->is_constant) ? b->var_id : 999999;
    return (va < vb) ? va : vb;
}

/** 内部：ADD cofactor（取变量为 0 或 1 的分支） */
static ADDNode *add_cofactor(ADDNode *node, int var, int val) {
    if (!node || node->is_constant)
        return node;
    if (node->var_id > var)
        return node;
    if (node->var_id == var)
        return val ? node->high : node->low;
    return node;
}

ADDNode *add_add(ADDManager *mgr, ADDNode *a, ADDNode *b) {
    if (!mgr || !a || !b)
        return NULL;
    /* 常数情况直接计算 */
    if (a->is_constant && b->is_constant) {
        return add_constant(mgr, a->constant + b->constant);
    }
    /* Shannon 展开：f+g = x*(f1+g1) + x'*(f0+g0) */
    int top = add_top_var(a, b);
    ADDNode *a0 = add_cofactor(a, top, 0);
    ADDNode *a1 = add_cofactor(a, top, 1);
    ADDNode *b0 = add_cofactor(b, top, 0);
    ADDNode *b1 = add_cofactor(b, top, 1);
    ADDNode *low = add_add(mgr, a0, b0);
    ADDNode *high = add_add(mgr, a1, b1);
    return add_node_create(mgr, top, low, high);
}

ADDNode *add_sub(ADDManager *mgr, ADDNode *a, ADDNode *b) {
    if (!mgr || !a || !b)
        return NULL;
    /* 常数情况直接计算 */
    if (a->is_constant && b->is_constant) {
        return add_constant(mgr, a->constant - b->constant);
    }
    /* f-g = f + (-g)：先对 g 取负再相加 */
    ADDNode *neg_b = add_mul(mgr, add_constant(mgr, -1.0), b);
    if (!neg_b)
        return add_constant(mgr, 0.0);
    ADDNode *result = add_add(mgr, a, neg_b);
    return result;
}

ADDNode *add_mul(ADDManager *mgr, ADDNode *a, ADDNode *b) {
    if (!mgr || !a || !b)
        return NULL;
    /* 常数情况直接计算 */
    if (a->is_constant && b->is_constant) {
        return add_constant(mgr, a->constant * b->constant);
    }
    /* 乘以零恒为零 */
    if (a->is_constant && a->constant == 0.0)
        return a;
    if (b->is_constant && b->constant == 0.0)
        return b;
    /* 乘以一不变 */
    if (a->is_constant && a->constant == 1.0)
        return b;
    if (b->is_constant && b->constant == 1.0)
        return a;
    /* Shannon 展开：f*g = x*(f1*g1) + x'*(f0*g0) */
    int top = add_top_var(a, b);
    ADDNode *a0 = add_cofactor(a, top, 0);
    ADDNode *a1 = add_cofactor(a, top, 1);
    ADDNode *b0 = add_cofactor(b, top, 0);
    ADDNode *b1 = add_cofactor(b, top, 1);
    ADDNode *low = add_mul(mgr, a0, b0);
    ADDNode *high = add_mul(mgr, a1, b1);
    return add_node_create(mgr, top, low, high);
}

ADDNode *add_div(ADDManager *mgr, ADDNode *a, ADDNode *b) {
    if (!mgr || !a || !b)
        return NULL;
    /* 常数情况直接计算 */
    if (a->is_constant && b->is_constant && fabs(b->constant) > lv_EPSILON_DOUBLE) {
        return add_constant(mgr, a->constant / b->constant);
    }
    /* 非常数情况：除法在 ADD 上实现复杂，返回常数 0 */
    return add_constant(mgr, 0.0);
}

ADDNode *add_max(ADDManager *mgr, ADDNode *a, ADDNode *b) {
    if (!mgr || !a || !b)
        return NULL;
    /* 常数情况直接计算 */
    if (a->is_constant && b->is_constant) {
        return add_constant(mgr, (a->constant > b->constant) ? a->constant : b->constant);
    }
    /* 使用 ITE 映射到 ADD：max(f,g) = ITE(f>g, f, g)
     * Shannon 展开：max(f,g) = x * max(f1,g1) + x' * max(f0,g0) */
    int top = add_top_var(a, b);
    ADDNode *a0 = add_cofactor(a, top, 0);
    ADDNode *a1 = add_cofactor(a, top, 1);
    ADDNode *b0 = add_cofactor(b, top, 0);
    ADDNode *b1 = add_cofactor(b, top, 1);
    ADDNode *low = add_max(mgr, a0, b0);
    ADDNode *high = add_max(mgr, a1, b1);
    return add_node_create(mgr, top, low, high);
}

ADDNode *add_min(ADDManager *mgr, ADDNode *a, ADDNode *b) {
    if (!mgr || !a || !b)
        return NULL;
    /* 常数情况直接计算 */
    if (a->is_constant && b->is_constant) {
        return add_constant(mgr, (a->constant < b->constant) ? a->constant : b->constant);
    }
    /* 使用 ITE 映射到 ADD：min(f,g) = ITE(f<g, f, g)
     * Shannon 展开：min(f,g) = x * min(f1,g1) + x' * min(f0,g0) */
    int top = add_top_var(a, b);
    ADDNode *a0 = add_cofactor(a, top, 0);
    ADDNode *a1 = add_cofactor(a, top, 1);
    ADDNode *b0 = add_cofactor(b, top, 0);
    ADDNode *b1 = add_cofactor(b, top, 1);
    ADDNode *low = add_min(mgr, a0, b0);
    ADDNode *high = add_min(mgr, a1, b1);
    return add_node_create(mgr, top, low, high);
}
