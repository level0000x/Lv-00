/**
 * @file bdd_encoding_encode.c
 * @brief 约束图→BDD 编码与坐标 bit-blasting（由 bdd_encoding.c 拆分子模块）
 *
 * @details 各约束类型（incidence/betweenness/intersection/containment/
 *          angle/connection）的 BDD 编码、查找表分发与符号坐标数值提取。
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

/* ── 辅助：根据节点 ID 查找 node_base_var 数组索引（graph_get_node 哈希 → O(1)） ── */
static int lookup_node_base_var(int node_id, int n, const int *node_base_var, const ConstraintGraph *graph) {
    if (!graph || node_id < 0 || graph->node_count <= 0 || !graph->nodes[0])
        return -1;
    GeomNode *node = graph_get_node(graph, node_id);
    if (!node)
        return -1;
    intptr_t byte_off = (intptr_t) node - (intptr_t) graph->nodes[0];
    if (byte_off < 0 || byte_off % (intptr_t) sizeof(GeomNode *) != 0)
        return -1;
    intptr_t j = byte_off / (intptr_t) sizeof(GeomNode *);
    if (j < 0 || j >= n)
        return -1;
    return node_base_var[j];
}

/* ── BDD 编码辅助函数（文件作用域，用于查找表） ── */
typedef BDDNode *(*BDDEncodeFn)(BDDManager *mgr, const Constraint *con, int n, const int *node_base_var, const ConstraintGraph *graph);

static BDDNode *bdd_encode_incidence(BDDManager *mgr, const Constraint *con, int n, const int *node_base_var, const ConstraintGraph *graph) {
    (void)n;
    if (lv_constraint_has_participants(con, 2)) {
        int p_id = con->participants[0];
        int l_id = con->participants[1];
        int p_var = (p_id >= 0) ? lookup_node_base_var(p_id, n, node_base_var, graph) : -1;
        int l_var = (l_id >= 0) ? lookup_node_base_var(l_id, n, node_base_var, graph) : -1;
        if (p_var >= 0 && l_var >= 0) {
            BDDNode *p_lit = bdd_literal(mgr, p_var + 1);
            BDDNode *l_lit = bdd_literal(mgr, l_var + 1);
            BDDNode *result = bdd_and(mgr, p_lit, l_lit);
            bdd_deref(mgr, p_lit);
            bdd_deref(mgr, l_lit);
            return result;
        }
    }
    return NULL;
}

static BDDNode *bdd_encode_betweenness(BDDManager *mgr, const Constraint *con, int n, const int *node_base_var, const ConstraintGraph *graph) {
    (void)n;
    if (lv_constraint_has_participants(con, 3)) {
        int p1_var = (con->participants[0] >= 0) ? lookup_node_base_var(con->participants[0], n, node_base_var, graph) : -1;
        int p2_var = (con->participants[1] >= 0) ? lookup_node_base_var(con->participants[1], n, node_base_var, graph) : -1;
        int p3_var = (con->participants[2] >= 0) ? lookup_node_base_var(con->participants[2], n, node_base_var, graph) : -1;
        if (p1_var >= 0 && p2_var >= 0 && p3_var >= 0) {
            BDDNode *a = bdd_literal(mgr, p1_var + 1);
            BDDNode *b = bdd_literal(mgr, p2_var + 1);
            BDDNode *c = bdd_literal(mgr, p3_var + 1);
            BDDNode *ab = bdd_and(mgr, a, b);
            BDDNode *result = bdd_and(mgr, ab, c);
            bdd_deref(mgr, a);
            bdd_deref(mgr, b);
            bdd_deref(mgr, c);
            bdd_deref(mgr, ab);
            return result;
        }
    }
    return NULL;
}

static BDDNode *bdd_encode_intersection(BDDManager *mgr, const Constraint *con, int n, const int *node_base_var, const ConstraintGraph *graph) {
    (void)n;
    if (lv_constraint_has_participants(con, 3)) {
        int l1_var = (con->participants[0] >= 0) ? lookup_node_base_var(con->participants[0], n, node_base_var, graph) : -1;
        int l2_var = (con->participants[1] >= 0) ? lookup_node_base_var(con->participants[1], n, node_base_var, graph) : -1;
        int p_var = (con->participants[2] >= 0) ? lookup_node_base_var(con->participants[2], n, node_base_var, graph) : -1;
        if (l1_var >= 0 && l2_var >= 0 && p_var >= 0) {
            BDDNode *l1_lit = bdd_literal(mgr, l1_var + 1);
            BDDNode *l2_lit = bdd_literal(mgr, l2_var + 1);
            BDDNode *p_lit = bdd_literal(mgr, p_var + 1);
            BDDNode *l_and = bdd_and(mgr, l1_lit, l2_lit);
            BDDNode *result = bdd_and(mgr, l_and, p_lit);
            bdd_deref(mgr, l1_lit);
            bdd_deref(mgr, l2_lit);
            bdd_deref(mgr, p_lit);
            bdd_deref(mgr, l_and);
            return result;
        }
    }
    return NULL;
}

static BDDNode *bdd_encode_containment(BDDManager *mgr, const Constraint *con, int n, const int *node_base_var, const ConstraintGraph *graph) {
    (void)n;
    if (lv_constraint_has_participants(con, 2)) {
        int r_var = (con->participants[0] >= 0) ? lookup_node_base_var(con->participants[0], n, node_base_var, graph) : -1;
        int p_var = (con->participants[1] >= 0) ? lookup_node_base_var(con->participants[1], n, node_base_var, graph) : -1;
        if (r_var >= 0 && p_var >= 0) {
            BDDNode *r_lit = bdd_literal(mgr, r_var + 1);
            BDDNode *p_lit = bdd_literal(mgr, p_var + 1);
            BDDNode *result = bdd_and(mgr, r_lit, p_lit);
            bdd_deref(mgr, r_lit);
            bdd_deref(mgr, p_lit);
            return result;
        }
    }
    return NULL;
}

static BDDNode *bdd_encode_angle(BDDManager *mgr, const Constraint *con, int n, const int *node_base_var, const ConstraintGraph *graph) {
    (void)n;
    if (lv_constraint_has_participants(con, 2)) {
        int l1_id = con->participants[0];
        int l2_id = con->participants[1];
        int l1_var = (l1_id >= 0) ? lookup_node_base_var(l1_id, n, node_base_var, graph) : -1;
        int l2_var = (l2_id >= 0) ? lookup_node_base_var(l2_id, n, node_base_var, graph) : -1;

        int bucket_count = 1 << 8;
        double bucket_width = lv_HALF_CIRCLE_DEG / (double) bucket_count;
        int target_bucket = (int) (con->numeric_value / bucket_width);
        if (target_bucket < 0)
            target_bucket = 0;
        if (target_bucket >= bucket_count)
            target_bucket = bucket_count - 1;

        BDDNode *acc = bdd_true(mgr);

        if (l1_var >= 0) {
            BDDNode *lit = bdd_literal(mgr, l1_var + 1);
            BDDNode *and1 = bdd_and(mgr, acc, lit);
            bdd_deref(mgr, acc);
            bdd_deref(mgr, lit);
            acc = and1;
        }
        if (l2_var >= 0) {
            BDDNode *lit = bdd_literal(mgr, l2_var + 1);
            BDDNode *and1 = bdd_and(mgr, acc, lit);
            bdd_deref(mgr, acc);
            bdd_deref(mgr, lit);
            acc = and1;
        }

        for (int bit = 0; bit < 8; bit++) {
            char var_name[48];
            lv_snprintf(var_name, sizeof(var_name), "angle_c%d_bit%d", con->id, bit);
            int bit_var = bdd_new_var(mgr, var_name, BDD_BOOLEAN);
            if (bit_var < 0)
                break;
            int bit_value = (target_bucket >> bit) & 1;
            BDDNode *bit_lit = bdd_literal(mgr, bit_value ? (bit_var + 1) : -(bit_var + 1));
            BDDNode *and1 = bdd_and(mgr, acc, bit_lit);
            bdd_deref(mgr, acc);
            bdd_deref(mgr, bit_lit);
            acc = and1;
        }

        return acc;
    }
    return NULL;
}

static BDDNode *bdd_encode_connection(BDDManager *mgr, const Constraint *con, int n, const int *node_base_var, const ConstraintGraph *graph) {
    (void)n;
    if (!mgr || !graph || !lv_constraint_has_participants(con, 2))
        return NULL;
    int p1_id = con->participants[0];
    int p2_id = con->participants[1];
    GeomNode *p1 = graph_get_node(graph, p1_id);
    GeomNode *p2 = graph_get_node(graph, p2_id);
    if (!p1 || !p2 || p1->type != GEOM_PORT || p2->type != GEOM_PORT)
        return NULL;

    /* 端口连接 = 数据流等值：两个端口各占用 64 位坐标/数据 bit-blast 变量。
     * 优先复用节点坐标变量（node_base_var），端口缺坐标（默认 -1）时动态分配
     * 数据位变量组，分配模式与 bdd_encode_angle 的动态位变量一致。 */
    int base1 = (p1_id >= 0) ? lookup_node_base_var(p1_id, n, node_base_var, graph) : -1;
    int base2 = (p2_id >= 0) ? lookup_node_base_var(p2_id, n, node_base_var, graph) : -1;

    if (base1 < 0) {
        base1 = mgr->var_count;
        for (int bit = 0; bit < 64; bit++) {
            char var_name[48];
            lv_snprintf(var_name, sizeof(var_name), "conn_%d_src_bit%d", con->id, bit);
            if (bdd_new_var(mgr, var_name, BDD_BOOLEAN) < 0)
                return NULL;
        }
    }
    if (base2 < 0) {
        base2 = mgr->var_count;
        for (int bit = 0; bit < 64; bit++) {
            char var_name[48];
            lv_snprintf(var_name, sizeof(var_name), "conn_%d_dst_bit%d", con->id, bit);
            if (bdd_new_var(mgr, var_name, BDD_BOOLEAN) < 0)
                return NULL;
        }
    }

    /* 逐位等值编码：bit_eq = (a∧b)∨(¬a∧¬b)（XNOR），64 位全部相等
     * → 两端口坐标/数据相等，结果非空（不返回 NULL）。 */
    BDDNode *acc = bdd_true(mgr);
    for (int bit = 0; bit < 64; bit++) {
        BDDNode *a = bdd_literal(mgr, base1 + bit + 1);
        BDDNode *b = bdd_literal(mgr, base2 + bit + 1);
        BDDNode *not_b = bdd_not(mgr, b);
        BDDNode *eq = bdd_ite(mgr, a, b, not_b);
        BDDNode *and1 = bdd_and(mgr, acc, eq);
        bdd_deref(mgr, a);
        bdd_deref(mgr, b);
        bdd_deref(mgr, not_b);
        bdd_deref(mgr, eq);
        bdd_deref(mgr, acc);
        acc = and1;
    }
    return acc;
}

static const BDDEncodeFn kBddEncodeTable[] = {
    bdd_encode_incidence,    /* INCIDENCE */
    bdd_encode_betweenness,  /* BETWEENNESS */
    bdd_encode_intersection, /* INTERSECTION */
    bdd_encode_containment,  /* CONTAINMENT */
    bdd_encode_connection,   /* CONNECTION */
    bdd_encode_angle         /* ANGLE */
};
static const int kBddEncodeTableCount =
    (int)(sizeof(kBddEncodeTable) / sizeof(kBddEncodeTable[0]));

/* ========================================================================
 * constraint_graph_to_bdd —— 约束图 -> BDD 编码
 *
 * 枚举所有布尔组合，构建 BDD。
 * 对于约束图中 n 个节点，有 2^n 种布尔赋值。
 * 每种赋值对应 BDD 的一个满足路径。
 * ======================================================================== */

BDDNode *constraint_graph_to_bdd(const ConstraintGraph *graph, BDDManager *mgr) {
    if (!graph || !mgr)
        return NULL;

    int n = graph->node_count;
    if (n <= 0)
        return bdd_true(mgr);

    /* 阶段 1: 为每个节点的坐标分配 BDD 变量范围 */
    int *node_base_var = (int *) lv_malloc((size_t) n * sizeof(int));
    if (!node_base_var)
        return NULL;

    int next_var = 0;
    for (int i = 0; i < n; i++) {
        GeomNode *gn = graph->nodes[i];
        if (!gn || !gn->symbolic_coords || gn->coord_count <= 0) {
            node_base_var[i] = -1;
            continue;
        }
        node_base_var[i] = next_var;
        int bits = coord_to_bdd_var(gn->symbolic_coords[0], mgr, next_var);
        if (bits > 0)
            next_var += bits;
    }

    /* 阶段 2: 遍历所有活跃约束，按类型编码 BDD 子公式 */
    BDDNode *constraint_bdd = bdd_true(mgr);

    for (int ci = 0; ci < graph->constraint_count; ci++) {
        Constraint *con = graph->constraints[ci];
        if (!con || !con->is_active)
            continue;

        BDDNode *sub = LV_DISPATCH(kBddEncodeTable, con->type, NULL, mgr, con, n, node_base_var, graph);

        if (sub) {
            BDDNode *new_bdd = bdd_and(mgr, constraint_bdd, sub);
            bdd_deref(mgr, constraint_bdd);
            bdd_deref(mgr, sub);
            constraint_bdd = new_bdd;
        }
    }

    lv_free((void **) &node_base_var);
    return constraint_bdd;
}

/* ── 坐标类型数值提取函数（文件作用域，用于查找表）── */
typedef double (*CoordValueFn)(const SymbolicCoord *coord);
static double coord_value_rational(const SymbolicCoord *coord) {
    if (coord->data.rational)
        return mpq_get_d(coord->data.rational->value);
    return 0.0;
}
static double coord_value_algebraic(const SymbolicCoord *coord) {
    if (coord->data.algebraic) {
        return (coord->data.algebraic->left_bound + coord->data.algebraic->right_bound) / 2.0;
    }
    return 0.0;
}
static double coord_value_quadratic(const SymbolicCoord *coord) {
    if (coord->data.quadratic) {
        Quadratic *q = coord->data.quadratic;
        double a_val = (q->a) ? mpq_get_d(q->a->value) : 0.0;
        double b_val = (q->b) ? mpq_get_d(q->b->value) : 0.0;
        return a_val + b_val * sqrt((double) q->n);
    }
    return 0.0;
}
static double coord_value_transcendental(const SymbolicCoord *coord) {
    return symbolic_coord_to_double(coord);
}
static const CoordValueFn kCoordValueTable[] = {
    coord_value_rational,      /* RATIONAL */
    coord_value_algebraic,     /* ALGEBRAIC */
    coord_value_quadratic,     /* QUADRATIC */
    coord_value_transcendental /* TRANSCENDENTAL */
};
static const int kCoordValueTableCount =
    (int)(sizeof(kCoordValueTable) / sizeof(kCoordValueTable[0]));

/* ========================================================================
 * coord_to_bdd_var —— 坐标 bit-blasting
 *
 * IEEE 754 双精度位表示：
 * 符号位 + 11 位指数 + 52 位尾数 = 64 位。
 * 每位编码为一个 BDD 变量。
 * ======================================================================== */

int coord_to_bdd_var(const SymbolicCoord *coord, BDDManager *mgr, int base_var) {
    if (!coord || !mgr)
        return -1;

/* 64 位 IEEE 754 双精度编码 */
#define IEEE754_DOUBLE_BITS 64

    /* 提取坐标的数值近似（使用 double），支持所有坐标类型 */
    double value = 0.0;

    if (coord->cache_valid) {
        /* 优先使用已缓存的数值近似 */
        value = coord->cached_value;
    } else {
        value = LV_DISPATCH(kCoordValueTable, coord->type, 0.0, coord);
    }

    /* 将 double 的 64 位分别编码为 BDD 变量 */
    union {
        double d;
        uint64_t u;
    } ieee;
    ieee.d = value;

    /* 为每一位注册一个 BDD 变量 */
    for (int bit = 0; bit < IEEE754_DOUBLE_BITS; bit++) {
        int var_id = base_var + bit;
        /* 获取或创建变量 */
        if (var_id >= mgr->var_count) {
            /* 检查 var_order 容量，不足时扩容 */
            int needed = var_id + 1;
            if (needed > mgr->var_capacity) {
                /* 三数组同步扩容至 lv_ensure_capacity（独立容量变量保证三数组容量一致） */
                int *new_order = mgr->var_order;
                char **new_names = mgr->var_names;
                BDDVarType *new_types = mgr->var_types;
                int order_cap = mgr->var_capacity;
                int names_cap = mgr->var_capacity;
                int types_cap = mgr->var_capacity;
                if (!lv_ensure_capacity((void **) &new_order, needed, &order_cap, sizeof(int), 0) ||
                    !lv_ensure_capacity((void **) &new_names, needed, &names_cap, sizeof(char *), 0) ||
                    !lv_ensure_capacity((void **) &new_types, needed, &types_cap, sizeof(BDDVarType), 0)) {
                    /* 部分失败：已成功扩容的临时指针指向新内存，释放以免泄漏；
                     * 失败/未执行的调用保持旧指针（与 mgr-> 相同），不可释放。 */
                    if (new_order != mgr->var_order)
                        lv_free((void **) &new_order);
                    if (new_names != mgr->var_names)
                        lv_free((void **) &new_names);
                    if (new_types != mgr->var_types)
                        lv_free((void **) &new_types);
                    return -1;
                }
                mgr->var_order = new_order;
                mgr->var_names = new_names;
                mgr->var_types = new_types;
                mgr->var_capacity = order_cap; /* 三数组容量一致 */
            }
            /* 初始化新增的变量条目 */
            for (int v = mgr->var_count; v < needed; v++) {
                mgr->var_order[v] = v;
                mgr->var_names[v] = NULL;
                mgr->var_types[v] = BDD_BOOLEAN;
            }
            mgr->var_count = needed;
        }
    }

    return IEEE754_DOUBLE_BITS;

#undef IEEE754_DOUBLE_BITS
}

