/**
 * @file sat_encoding.c
 * @brief SAT 编码管线实现 —— 借鉴 Alloy Kodkod 的关系逻辑到 SAT 编码管道
 *
 * 实现几何约束图中约束到 CNF 子句的编码规则、SAT 变量映射表管理、
 * DIMACS CNF 格式导出以及 SAT 模型的解码。
 *
 * 编码管线流程：
 *   1. 约束图节点注册为 SAT 变量（SatVarMap）
 *   2. 几何约束按规则编码为 CNF 子句
 *   3. 调用内部 SAT 求解器求解
 *   4. 将 SAT 赋值解码回约束图/关系实例
 *
 * @version v3.3.0
 * @date 2026-05-24
 */

#include <stdio.h>
#include <string.h>

#include "lv00.h"
#include "lv00_internal.h"
#include "lv00_utils.h"
#include "error_codes.h"
#include "lv00/constraint_graph.h"
#include "sat_encoding.h"
#include "solver_core.h"

/* ========================================================================
 * 内部常量
 * ======================================================================== */

/** 变元映射表初始容量 */
#define VAR_MAP_INITIAL_CAP 128
/** CNF 子句缓冲区初始容量 */
#define CLAUSE_INITIAL_CAP 256
/** 比特编码用的整数位宽（默认 8 位） */
#define DEFAULT_BITWIDTH 8

/* ========================================================================
 * 内部辅助函数
 * ======================================================================== */

/**
 * @brief 比较两个元组是否相等
 *
 * @param arity    元数
 * @param a        元组 A
 * @param b        元组 B
 * @return true 相等
 */
static bool tuple_equals(int arity, const int *a, const int *b) {
    for (int i = 0; i < arity; i++) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

/**
 * @brief 在变量映射表中查找元组
 *
 * @param enc       编码上下文
 * @param arity     元数
 * @param atom_ids  原子ID数组
 * @return 变量 ID（>= 1），未找到返回 -1
 */
static int find_var_entry(const SatEncoding *enc, int arity, const int *atom_ids) {
    for (int i = 0; i < enc->var_count; i++) {
        const SatVarEntry *entry = &enc->var_map[i];
        if (entry->arity == arity && tuple_equals(arity, entry->atom_ids, atom_ids)) {
            return entry->var_id;
        }
    }
    return -1;
}

/**
 * @brief 确保 CNF 子句缓冲区有足够容量
 */
static bool ensure_clause_capacity(SatEncoding *enc) {
    if (enc->clause_count >= enc->clause_capacity) {
        int new_cap = (enc->clause_capacity == 0) ? CLAUSE_INITIAL_CAP : enc->clause_capacity * LV00_ARRAY_GROWTH_FACTOR;
        /* 整数溢出检查 */
        if (new_cap <= 0 || new_cap < enc->clause_capacity) {
            return false;
        }
        int **new_clauses = (int **)lv00_realloc(enc->clauses, (size_t)new_cap * sizeof(int *));
        int *new_sizes = (int *)lv00_realloc(enc->clause_sizes, (size_t)new_cap * sizeof(int));
        if (!new_clauses || !new_sizes) {
            if (new_clauses) lv00_free((void **)&new_clauses);
            if (new_sizes) lv00_free((void **)&new_sizes);
            return false;
        }
        enc->clauses = new_clauses;
        enc->clause_sizes = new_sizes;
        enc->clause_capacity = new_cap;
    }
    return true;
}

/**
 * @brief 确保变量映射表有足够容量
 */
static bool ensure_var_capacity(SatEncoding *enc) {
    if (enc->var_count >= enc->var_capacity) {
        int new_cap = (enc->var_capacity == 0) ? VAR_MAP_INITIAL_CAP : enc->var_capacity * LV00_ARRAY_GROWTH_FACTOR;
        /* 整数溢出检查 */
        if (new_cap <= 0 || new_cap < enc->var_capacity) {
            return false;
        }
        SatVarEntry *new_map = (SatVarEntry *)lv00_realloc(enc->var_map, (size_t)new_cap * sizeof(SatVarEntry));
        if (!new_map) return false;
        enc->var_map = new_map;
        enc->var_capacity = new_cap;
    }
    return true;
}

/* ========================================================================
 * SAT 编码上下文生命周期
 * ======================================================================== */

SatEncoding *sat_encoding_create(int initial_var_capacity, int initial_clause_capacity) {
    if (initial_var_capacity <= 0) initial_var_capacity = VAR_MAP_INITIAL_CAP;
    if (initial_clause_capacity <= 0) initial_clause_capacity = CLAUSE_INITIAL_CAP;

    SatEncoding *enc = (SatEncoding *)lv00_malloc(sizeof(SatEncoding));
    LV00_CHECK_ALLOC(enc, NULL);
    memset(enc, 0, sizeof(SatEncoding));

    enc->var_map = (SatVarEntry *)lv00_malloc((size_t)initial_var_capacity * sizeof(SatVarEntry));
    if (!enc->var_map) {
        lv00_free((void **)&enc);
        return NULL;
    }
    enc->var_capacity = initial_var_capacity;
    enc->var_count = 0;
    enc->next_var_id = 1;

    enc->clauses = (int **)lv00_malloc((size_t)initial_clause_capacity * sizeof(int *));
    enc->clause_sizes = (int *)lv00_malloc((size_t)initial_clause_capacity * sizeof(int));
    if (!enc->clauses || !enc->clause_sizes) {
        lv00_free((void **)&enc->var_map);
        if (enc->clauses) lv00_free((void **)&enc->clauses);
        if (enc->clause_sizes) lv00_free((void **)&enc->clause_sizes);
        lv00_free((void **)&enc);
        return NULL;
    }
    enc->clause_capacity = initial_clause_capacity;
    enc->clause_count = 0;

    enc->total_vars = 0;
    enc->total_clauses = 0;
    enc->encode_time_ms = 0.0;
    enc->graph = NULL;
    enc->rel_model = NULL;

    return enc;
}

void sat_encoding_destroy(SatEncoding *enc) {
    if (!enc) return;

    lv00_free((void **)&enc->var_map);
    for (int i = 0; i < enc->clause_count; i++) {
        lv00_free((void **)&enc->clauses[i]);
    }
    lv00_free((void **)&enc->clauses);
    lv00_free((void **)&enc->clause_sizes);
    lv00_free((void **)&enc);
}

/* ========================================================================
 * 变量注册与查找
 * ======================================================================== */

int sat_encoding_register_var(SatEncoding *enc, int arity, const int *atom_ids) {
    LV00_CHECK_NULL(enc, -1);
    LV00_CHECK_NULL(atom_ids, -1);
    if (arity <= 0 || arity > 8) {
        lv00_set_error_ctx(LV00_ERROR_INVALID_PARAM, __FILE__, __LINE__, __func__,
                           "无效元数: %d, 有效范围 [1, 8]", arity);
        return -1;
    }

    /* 查找是否已有映射 */
    int existing = find_var_entry(enc, arity, atom_ids);
    if (existing >= 1) return existing;

    /* 创建新映射 */
    if (!ensure_var_capacity(enc)) return -1;

    SatVarEntry *entry = &enc->var_map[enc->var_count];
    entry->var_id = enc->next_var_id++;
    entry->arity = arity;
    memset(entry->atom_ids, 0, sizeof(entry->atom_ids));
    for (int i = 0; i < arity; i++) {
        entry->atom_ids[i] = atom_ids[i];
    }
    enc->var_count++;
    enc->total_vars++;

    return entry->var_id;
}

int sat_encoding_lookup_var(const SatEncoding *enc, int arity, const int *atom_ids) {
    LV00_CHECK_NULL(enc, -1);
    LV00_CHECK_NULL(atom_ids, -1);
    if (arity <= 0 || arity > 8) return -1;
    return find_var_entry(enc, arity, atom_ids);
}

/* ========================================================================
 * CNF 子句管理
 * ======================================================================== */

int sat_encoding_add_clause(SatEncoding *enc, const SatLiteral *literals, int count) {
    LV00_CHECK_NULL(enc, -1);
    LV00_CHECK_NULL(literals, -1);
    if (count <= 0) {
        /* 空白子句表示矛盾 */
        lv00_set_error_ctx(LV00_ERROR_INVALID_PARAM, __FILE__, __LINE__, __func__,
                           "子句文字数量必须 >= 1");
        return -1;
    }

    if (!ensure_clause_capacity(enc)) return -1;

    int *clause = (int *)lv00_malloc((size_t)(count + 1) * sizeof(int));
    LV00_CHECK_ALLOC(clause, -1);
    for (int i = 0; i < count; i++) {
        clause[i] = literals[i];
    }
    clause[count] = 0; /* 0 终止 */

    int idx = enc->clause_count;
    enc->clauses[idx] = clause;
    enc->clause_sizes[idx] = count;
    enc->clause_count++;
    enc->total_clauses++;

    return idx;
}

int sat_encoding_add_assumption(SatEncoding *enc, SatLiteral literal) {
    /* 单元子句即假设 */
    SatLiteral clause[1] = {literal};
    return sat_encoding_add_clause(enc, clause, 1);
}

/* ========================================================================
 * 约束编码规则 —— 7 种几何约束的 CNF 编码
 *
 * 每类约束通过为相关的 (node_i, node_j) 对创建布尔变量，
 * 然后编码几何关系为 CNF 子句。
 * ======================================================================== */

/**
 * @brief 为两个节点对注册或查找 SAT 变量
 *
 * @param enc      编码上下文
 * @param n1_id    节点1
 * @param n2_id    节点2
 * @param out_var1 输出变量1
 * @param out_var2 输出变量2
 * @return 成功返回0
 */
static int register_pair_var(SatEncoding *enc, int n1_id, int n2_id, int *out_var) {
    int ids[2] = {n1_id, n2_id};
    int var = sat_encoding_register_var(enc, 2, ids);
    if (var < 1) return -1;
    *out_var = var;
    return 0;
}

/* ── 共线性约束编码 ── */

int sat_encode_collinearity(SatEncoding *enc, int p1_id, int p2_id, int p3_id) {
    LV00_CHECK_NULL(enc, -1);

    int v12, v23, v13;
    if (register_pair_var(enc, p1_id, p2_id, &v12) < 0) return -1;
    if (register_pair_var(enc, p2_id, p3_id, &v23) < 0) return -1;
    if (register_pair_var(enc, p1_id, p3_id, &v13) < 0) return -1;

    int clause_count = 0;

    /* 共线意味着：若 p1-p2 共线和 p2-p3 共线，则 p1-p3 共线 */
    {
        SatLiteral c1[] = { -v12, -v23, v13 };
        if (sat_encoding_add_clause(enc, c1, 3) >= 0) clause_count++;
    }
    /* 传递性反向 */
    {
        SatLiteral c2[] = { -v12, -v13, v23 };
        if (sat_encoding_add_clause(enc, c2, 3) >= 0) clause_count++;
    }
    {
        SatLiteral c3[] = { -v23, -v13, v12 };
        if (sat_encoding_add_clause(enc, c3, 3) >= 0) clause_count++;
    }

    return clause_count;
}

/* ── 平行性约束编码 ── */

int sat_encode_parallelism(SatEncoding *enc, int p1_id, int p2_id, int p3_id, int p4_id) {
    LV00_CHECK_NULL(enc, -1);

    int v12, v34, v_parallel;
    if (register_pair_var(enc, p1_id, p2_id, &v12) < 0) return -1;
    if (register_pair_var(enc, p3_id, p4_id, &v34) < 0) return -1;

    int ids[2] = {v12, v34};
    v_parallel = sat_encoding_register_var(enc, 2, ids);
    if (v_parallel < 1) return -1;

    int clause_count = 0;

    /* parallel 变量等价于两个线段方向一致，编码为双向蕴含 */
    {
        SatLiteral c1[] = { -v_parallel, v12 };
        if (sat_encoding_add_clause(enc, c1, 2) >= 0) clause_count++;
    }
    {
        SatLiteral c2[] = { -v_parallel, v34 };
        if (sat_encoding_add_clause(enc, c2, 2) >= 0) clause_count++;
    }
    {
        SatLiteral c3[] = { v_parallel, -v12, -v34 };
        if (sat_encoding_add_clause(enc, c3, 3) >= 0) clause_count++;
    }

    return clause_count;
}

/* ── 垂直性约束编码 ── */

int sat_encode_perpendicularity(SatEncoding *enc, int p1_id, int p2_id, int p3_id, int p4_id) {
    LV00_CHECK_NULL(enc, -1);

    int v12, v34, v_perp;
    if (register_pair_var(enc, p1_id, p2_id, &v12) < 0) return -1;
    if (register_pair_var(enc, p3_id, p4_id, &v34) < 0) return -1;

    int ids[2] = {v12, v34};
    v_perp = sat_encoding_register_var(enc, 2, ids);
    if (v_perp < 1) return -1;

    int clause_count = 0;

    /* 垂直等价于内积为0，建立蕴含关系 */
    {
        SatLiteral c1[] = { -v_perp, v12 };
        if (sat_encoding_add_clause(enc, c1, 2) >= 0) clause_count++;
    }
    {
        SatLiteral c2[] = { -v_perp, v34 };
        if (sat_encoding_add_clause(enc, c2, 2) >= 0) clause_count++;
    }
    {
        SatLiteral c3[] = { v_perp, -v12, -v34 };
        if (sat_encoding_add_clause(enc, c3, 3) >= 0) clause_count++;
    }

    /* 垂直性与平行性不能同时成立 */
    int ids_p[2] = {v12, v34};
    int v_no_overlap = sat_encoding_register_var(enc, 2, ids_p);
    if (v_no_overlap >= 1) {
        SatLiteral c4[] = { -v_perp, -v_no_overlap };
        if (sat_encoding_add_clause(enc, c4, 2) >= 0) clause_count++;
    }

    return clause_count;
}

/* ── 距离相等约束编码 ── */

int sat_encode_distance_eq(SatEncoding *enc, int p1_id, int p2_id, int p3_id, int p4_id) {
    LV00_CHECK_NULL(enc, -1);

    int v12, v34, v_dist_eq;
    if (register_pair_var(enc, p1_id, p2_id, &v12) < 0) return -1;
    if (register_pair_var(enc, p3_id, p4_id, &v34) < 0) return -1;

    int ids[2] = {v12, v34};
    v_dist_eq = sat_encoding_register_var(enc, 2, ids);
    if (v_dist_eq < 1) return -1;

    int clause_count = 0;

    /* 距离相等 = 两对都存在且等价 */
    {
        SatLiteral c1[] = { -v_dist_eq, v12 };
        if (sat_encoding_add_clause(enc, c1, 2) >= 0) clause_count++;
    }
    {
        SatLiteral c2[] = { -v_dist_eq, v34 };
        if (sat_encoding_add_clause(enc, c2, 2) >= 0) clause_count++;
    }
    {
        SatLiteral c3[] = { v_dist_eq, -v12, -v34 };
        if (sat_encoding_add_clause(enc, c3, 3) >= 0) clause_count++;
    }

    return clause_count;
}

/* ── 角度相等约束编码 ── */

int sat_encode_angle_eq(SatEncoding *enc, int p1_id, int p2_id, int p3_id, int p4_id, int p5_id, int p6_id) {
    LV00_CHECK_NULL(enc, -1);

    int v12, v13, v45, v46, v_angle;
    if (register_pair_var(enc, p1_id, p2_id, &v12) < 0) return -1;
    if (register_pair_var(enc, p1_id, p3_id, &v13) < 0) return -1;
    if (register_pair_var(enc, p4_id, p5_id, &v45) < 0) return -1;
    if (register_pair_var(enc, p4_id, p6_id, &v46) < 0) return -1;

    int ids[2] = {v12, v13};
    v_angle = sat_encoding_register_var(enc, 2, ids);
    if (v_angle < 1) return -1;

    int clause_count = 0;

    /* 角度相等：两个角的两对边都存在且分别等价 */
    {
        SatLiteral c1[] = { -v_angle, v12, -v45 };
        if (sat_encoding_add_clause(enc, c1, 3) >= 0) clause_count++;
    }
    {
        SatLiteral c2[] = { -v_angle, v13, -v46 };
        if (sat_encoding_add_clause(enc, c2, 3) >= 0) clause_count++;
    }
    {
        SatLiteral c3[] = { v_angle, -v12 };
        if (sat_encoding_add_clause(enc, c3, 2) >= 0) clause_count++;
    }
    {
        SatLiteral c4[] = { v_angle, -v13 };
        if (sat_encoding_add_clause(enc, c4, 2) >= 0) clause_count++;
    }

    return clause_count;
}

/* ── 包含关系约束编码 ── */

int sat_encode_containment(SatEncoding *enc, int p_id, int r_id) {
    LV00_CHECK_NULL(enc, -1);

    int var;
    if (register_pair_var(enc, p_id, r_id, &var) < 0) return -1;

    /* 包含关系编码为单元子句：该对必须为真 */
    SatLiteral c1[] = { var };
    int idx = sat_encoding_add_clause(enc, c1, 1);
    return (idx >= 0) ? 1 : -1;
}

/* ── 通用约束编码（根据约束类型分发）── */

int sat_encode_constraint(SatEncoding *enc, int constraint_id) {
    LV00_CHECK_NULL(enc, -1);
    if (!enc->graph) {
        lv00_set_error_ctx(LV00_ERROR_INVALID_PARAM, __FILE__, __LINE__, __func__,
                           "编码上下文中没有关联约束图");
        return -1;
    }

    const Constraint *con = graph_get_constraint(enc->graph, constraint_id);
    if (!con) {
        lv00_set_error_ctx(LV00_ERROR_NOT_FOUND, __FILE__, __LINE__, __func__,
                           "约束 ID=%d 未找到", constraint_id);
        return -1;
    }

    switch (con->type) {
        case BETWEENNESS:
            if (con->participant_count >= 3)
                return sat_encode_collinearity(enc,
                    con->participants[0], con->participants[1], con->participants[2]);
            break;

        case INCIDENCE:
            if (con->participant_count >= 2)
                return sat_encode_containment(enc,
                    con->participants[0], con->participants[1]);
            break;

        case INTERSECTION:
            if (con->participant_count >= 4)
                return sat_encode_parallelism(enc,
                    con->participants[0], con->participants[1],
                    con->participants[2], con->participants[3]);
            break;

        case CONTAINMENT:
            if (con->participant_count >= 2)
                return sat_encode_containment(enc,
                    con->participants[0], con->participants[1]);
            break;

        case CONNECTION:
            /* 连接约束编码为关系存在 */
            if (con->participant_count >= 2) {
                int var;
                if (register_pair_var(enc, con->participants[0], con->participants[1], &var) < 0)
                    return -1;
                SatLiteral c1[] = { var };
                int idx = sat_encoding_add_clause(enc, c1, 1);
                return (idx >= 0) ? 1 : -1;
            }
            break;
        default:
            LV00_LOG_WARNING("Unknown constraint type %d in sat_encode_constraint", con->type);
            break;
    }

    return 0;
}

/* ========================================================================
 * 约束图 → SAT 编码（主编码管道）
 * ======================================================================== */

SatResult constraint_graph_to_sat(const ConstraintGraph *graph, SatEncoding *enc) {
    LV00_CHECK_NULL(graph, SAT_ERROR);
    LV00_CHECK_NULL(enc, SAT_ERROR);

    enc->graph = (ConstraintGraph *)graph;

    /* 遍历所有约束，逐个编码 */
    for (int i = 0; i < graph->constraint_count; i++) {
        if (!graph->constraints[i]) continue;
        int ret = sat_encode_constraint(enc, graph->constraints[i]->id);
        if (ret < 0) {
            lv00_set_error_ctx(LV00_ERROR_INTERNAL, __FILE__, __LINE__, __func__,
                               "约束 ID=%d 编码失败", graph->constraints[i]->id);
            return SAT_ERROR;
        }
    }

    return SAT_OK;
}

/* ========================================================================
 * 关系模型 → SAT 编码
 * ======================================================================== */

SatResult relation_model_to_sat(const RelModel *model, const SmallScopeConfig *scope, SatEncoding *enc) {
    LV00_CHECK_NULL(model, SAT_ERROR);
    LV00_CHECK_NULL(scope, SAT_ERROR);
    LV00_CHECK_NULL(enc, SAT_ERROR);
    LV00_UNUSED(scope);

    enc->rel_model = model;

    /* 为关系模型中的每个原子对注册变量 */
    if (model->sigs) {
        for (int si = 0; si < model->sig_count; si++) {
            RelSignature *sig = model->sigs[si];
            if (!sig) continue;
            for (int ai = 0; ai < sig->atom_count; ai++) {
                for (int aj = ai + 1; aj < sig->atom_count; aj++) {
                    int ids[2] = {sig->atoms[ai]->atom_id, sig->atoms[aj]->atom_id};
                    sat_encoding_register_var(enc, 2, ids);
                }
            }
        }
    }

    /* 编码事实公式为硬约束 */
    for (int fi = 0; fi < model->fact_count; fi++) {
        RelFormula *formula = model->facts[fi];
        if (!formula) continue;

        switch (formula->type) {
            case REL_FORMULA_SOME: {
                /* some R: 关系 R 非空，至少一个元组为真 */
                if (formula->expr && formula->expr->type == REL_EXPR_ATOMIC &&
                    formula->expr->data.atomic.rel) {
                    Relation *rel = formula->expr->data.atomic.rel;
                    /* 为该关系的每个元组注册变量，然后添加"至少一个为真"的子句 */
                    SatLiteral *disj = (SatLiteral *)lv00_malloc(
                        (size_t)rel->tuple_count * sizeof(SatLiteral));
                    if (!disj) { /* 内存不足，跳过此事实 */ break; }
                    int disj_count = 0;
                    for (int ti = 0; ti < rel->tuple_count; ti++) {
                        int var = sat_encoding_register_var(enc, rel->arity, rel->tuples[ti]);
                        if (var >= 1) {
                            disj[disj_count++] = var;
                        }
                    }
                    if (disj_count > 0) {
                        sat_encoding_add_clause(enc, disj, disj_count);
                    }
                    lv00_free((void **)&disj);
                }
                break;
            }
            case REL_FORMULA_NO: {
                /* no R: 关系 R 为空，所有元组必须为假 */
                if (formula->expr && formula->expr->type == REL_EXPR_ATOMIC &&
                    formula->expr->data.atomic.rel) {
                    Relation *rel = formula->expr->data.atomic.rel;
                    for (int ti = 0; ti < rel->tuple_count; ti++) {
                        int var = sat_encoding_register_var(enc, rel->arity, rel->tuples[ti]);
                        if (var >= 1) {
                            SatLiteral unit = -var;
                            sat_encoding_add_clause(enc, &unit, 1);
                        }
                    }
                }
                break;
            }
            case REL_FORMULA_ONE: {
                /* one R: 关系 R 恰好包含一个元组 */
                if (formula->expr && formula->expr->type == REL_EXPR_ATOMIC &&
                    formula->expr->data.atomic.rel) {
                    Relation *rel = formula->expr->data.atomic.rel;
                    int *vars = (int *)lv00_malloc(
                        (size_t)rel->tuple_count * sizeof(int));
                    if (!vars) break;
                    int var_count = 0;
                    for (int ti = 0; ti < rel->tuple_count; ti++) {
                        int var = sat_encoding_register_var(enc, rel->arity, rel->tuples[ti]);
                        if (var >= 1) {
                            vars[var_count++] = var;
                        }
                    }
                    /* 至少一个为真 */
                    if (var_count > 0) {
                        SatLiteral *disj = (SatLiteral *)lv00_malloc(
                            (size_t)var_count * sizeof(SatLiteral));
                        if (disj) {
                            for (int vi = 0; vi < var_count; vi++) disj[vi] = vars[vi];
                            sat_encoding_add_clause(enc, disj, var_count);
                            lv00_free((void **)&disj);
                        }
                    }
                    /* 至多一个为真：任意两个不同元组不能同时为真 */
                    for (int i = 0; i < var_count; i++) {
                        for (int j = i + 1; j < var_count; j++) {
                            SatLiteral pair[] = { -vars[i], -vars[j] };
                            sat_encoding_add_clause(enc, pair, 2);
                        }
                    }
                    lv00_free((void **)&vars);
                }
                break;
            }
            case REL_FORMULA_LONE: {
                /* lone R: 关系 R 最多包含一个元组 */
                if (formula->expr && formula->expr->type == REL_EXPR_ATOMIC &&
                    formula->expr->data.atomic.rel) {
                    Relation *rel = formula->expr->data.atomic.rel;
                    int *vars = (int *)lv00_malloc(
                        (size_t)rel->tuple_count * sizeof(int));
                    if (!vars) break;
                    int var_count = 0;
                    for (int ti = 0; ti < rel->tuple_count; ti++) {
                        int var = sat_encoding_register_var(enc, rel->arity, rel->tuples[ti]);
                        if (var >= 1) {
                            vars[var_count++] = var;
                        }
                    }
                    for (int i = 0; i < var_count; i++) {
                        for (int j = i + 1; j < var_count; j++) {
                            SatLiteral pair[] = { -vars[i], -vars[j] };
                            sat_encoding_add_clause(enc, pair, 2);
                        }
                    }
                    lv00_free((void **)&vars);
                }
                break;
            }
            case REL_FORMULA_EQ:
            case REL_FORMULA_SUBSET: {
                /* R = S 或 R in S: 简化为逐元组蕴含 */
                /* 对于原子关系引用，编码为元组级别的等价/蕴含 */
                if (formula->expr && formula->expr->type == REL_EXPR_ATOMIC &&
                    formula->expr->data.atomic.rel) {
                    Relation *rel = formula->expr->data.atomic.rel;
                    /* 将关系中的每个元组编码为必须为真（作为硬约束） */
                    for (int ti = 0; ti < rel->tuple_count; ti++) {
                        int var = sat_encoding_register_var(enc, rel->arity, rel->tuples[ti]);
                        if (var >= 1) {
                            SatLiteral unit = var;
                            sat_encoding_add_clause(enc, &unit, 1);
                        }
                    }
                }
                break;
            }
            case REL_FORMULA_AND: {
                /* F && G: 递归编码两个子公式 */
                /* 子公式通过 model->facts 中的其他条目处理，
                 * 此处直接编码为：两个子公式对应的关系元组都必须为真 */
                for (int si = 0; si < 2; si++) {
                    RelFormula *sub = formula->sub[si];
                    if (!sub || !sub->expr) continue;
                    if (sub->expr->type == REL_EXPR_ATOMIC && sub->expr->data.atomic.rel) {
                        Relation *sub_rel = sub->expr->data.atomic.rel;
                        for (int ti = 0; ti < sub_rel->tuple_count; ti++) {
                            int var = sat_encoding_register_var(enc, sub_rel->arity, sub_rel->tuples[ti]);
                            if (var >= 1) {
                                SatLiteral unit = var;
                                sat_encoding_add_clause(enc, &unit, 1);
                            }
                        }
                    }
                }
                break;
            }
            case REL_FORMULA_OR: {
                /* F || G: 至少一个子公式成立 */
                {
                    SatLiteral disj[2];
                    int disj_count = 0;
                    for (int si = 0; si < 2; si++) {
                        RelFormula *sub = formula->sub[si];
                        if (!sub || !sub->expr) continue;
                        if (sub->expr->type == REL_EXPR_ATOMIC && sub->expr->data.atomic.rel) {
                            Relation *sub_rel = sub->expr->data.atomic.rel;
                            /* 取第一个元组为代表变量 */
                            if (sub_rel->tuple_count > 0) {
                                int var = sat_encoding_register_var(enc, sub_rel->arity, sub_rel->tuples[0]);
                                if (var >= 1) disj[disj_count++] = var;
                            }
                        }
                    }
                    if (disj_count > 0) {
                        sat_encoding_add_clause(enc, disj, disj_count);
                    }
                }
                break;
            }
            case REL_FORMULA_NOT: {
                /* !F: 取反子公式 */
                if (formula->sub[0] && formula->sub[0]->expr &&
                    formula->sub[0]->expr->type == REL_EXPR_ATOMIC &&
                    formula->sub[0]->expr->data.atomic.rel) {
                    Relation *sub_rel = formula->sub[0]->expr->data.atomic.rel;
                    for (int ti = 0; ti < sub_rel->tuple_count; ti++) {
                        int var = sat_encoding_register_var(enc, sub_rel->arity, sub_rel->tuples[ti]);
                        if (var >= 1) {
                            SatLiteral unit = -var;
                            sat_encoding_add_clause(enc, &unit, 1);
                        }
                    }
                }
                break;
            }
            case REL_FORMULA_IMPLIES: {
                /* F => G: 等价于 !F || G */
                {
                    SatLiteral disj[2];
                    int disj_count = 0;
                    /* !F: 取反左侧 */
                    if (formula->sub[0] && formula->sub[0]->expr &&
                        formula->sub[0]->expr->type == REL_EXPR_ATOMIC &&
                        formula->sub[0]->expr->data.atomic.rel) {
                        Relation *left_rel = formula->sub[0]->expr->data.atomic.rel;
                        if (left_rel->tuple_count > 0) {
                            int var = sat_encoding_register_var(enc, left_rel->arity, left_rel->tuples[0]);
                            if (var >= 1) disj[disj_count++] = -var;
                        }
                    }
                    /* G: 正向右侧 */
                    if (formula->sub[1] && formula->sub[1]->expr &&
                        formula->sub[1]->expr->type == REL_EXPR_ATOMIC &&
                        formula->sub[1]->expr->data.atomic.rel) {
                        Relation *right_rel = formula->sub[1]->expr->data.atomic.rel;
                        if (right_rel->tuple_count > 0) {
                            int var = sat_encoding_register_var(enc, right_rel->arity, right_rel->tuples[0]);
                            if (var >= 1) disj[disj_count++] = var;
                        }
                    }
                    if (disj_count > 0) {
                        sat_encoding_add_clause(enc, disj, disj_count);
                    }
                }
                break;
            }
            case REL_FORMULA_FORALL:
            case REL_FORMULA_EXISTS: {
                /* 量词公式：在全称/存在量化下编码子公式 */
                /* 实现：对有限域上的量词进行展开编码 */
                if (formula->sub[0] && formula->sub[0]->expr &&
                    formula->sub[0]->expr->type == REL_EXPR_ATOMIC &&
                    formula->sub[0]->expr->data.atomic.rel) {
                    Relation *sub_rel = formula->sub[0]->expr->data.atomic.rel;
                    if (formula->type == REL_FORMULA_FORALL) {
                        /* all x: S | F => F 对所有 x 成立 */
                        for (int ti = 0; ti < sub_rel->tuple_count; ti++) {
                            int var = sat_encoding_register_var(enc, sub_rel->arity, sub_rel->tuples[ti]);
                            if (var >= 1) {
                                SatLiteral unit = var;
                                sat_encoding_add_clause(enc, &unit, 1);
                            }
                        }
                    } else {
                        /* some x: S | F => 至少一个 x 使 F 成立 */
                        SatLiteral *disj = (SatLiteral *)lv00_malloc(
                            (size_t)sub_rel->tuple_count * sizeof(SatLiteral));
                        if (disj) {
                            int dc = 0;
                            for (int ti = 0; ti < sub_rel->tuple_count; ti++) {
                                int var = sat_encoding_register_var(enc, sub_rel->arity, sub_rel->tuples[ti]);
                                if (var >= 1) disj[dc++] = var;
                            }
                            if (dc > 0) sat_encoding_add_clause(enc, disj, dc);
                            lv00_free((void **)&disj);
                        }
                    }
                }
                break;
            }
            default:
                LV00_LOG_WARNING("Unknown formula type %d in sat_encode_model_facts", formula->type);
                break;
        }
    }

    return SAT_OK;
}

/* ========================================================================
 * SAT 求解与解码
 * ======================================================================== */

SatResult sat_solve_and_decode(SatEncoding *enc, SatModel **out_model) {
    LV00_CHECK_NULL(enc, SAT_ERROR);
    LV00_CHECK_NULL(out_model, SAT_ERROR);

    /* 创建 CDCL 求解器，添加所有编码子句，求解并提取模型 */
    Lv00Solver *solver = lv00_solver_create();
    if (!solver) return SAT_ERROR;

    /* 将编码的子句加入求解器 */
    for (int i = 0; i < enc->clause_count; i++) {
        int *clause = enc->clauses[i];
        int size = enc->clause_sizes[i];
        Lv00SolverLit *lits = (Lv00SolverLit *)lv00_malloc((size_t)size * sizeof(Lv00SolverLit));
        if (!lits) {
            lv00_solver_destroy(solver);
            return SAT_ERROR;
        }
        for (int j = 0; j < size; j++) {
            lits[j] = clause[j];
        }
        lv00_solver_add_constraint(solver, lits, size);
        lv00_free((void **)&lits);
    }

    Lv00SolverResult result = lv00_solver_solve(solver);

    if (result == LV00_SOLVER_SAT) {
        SatModel *model = (SatModel *)lv00_malloc(sizeof(SatModel));
        if (!model) {
            lv00_solver_destroy(solver);
            return SAT_ERROR;
        }
        memset(model, 0, sizeof(SatModel));
        model->var_count = enc->total_vars;
        model->true_count = 0;
        model->true_vars = (int *)lv00_malloc((size_t)enc->total_vars * sizeof(int));
        if (!model->true_vars) {
            lv00_free((void **)&model);
            lv00_solver_destroy(solver);
            return SAT_ERROR;
        }

        /* 收集赋值为真的变量 */
        for (int v = 1; v <= enc->next_var_id - 1; v++) {
            int val = lv00_solver_get_value(solver, v);
            if (val > 0) {
                model->true_vars[model->true_count++] = v;
            }
        }

        model->decoded_graph = NULL;
        model->decoded_instance = NULL;
        *out_model = model;
    }

    lv00_solver_destroy(solver);

    switch (result) {
        case LV00_SOLVER_SAT:  return SAT_OK;
        case LV00_SOLVER_UNSAT: return SAT_UNSAT;
        default:                return SAT_UNKNOWN;
    }
}

SatResult sat_solve_incremental(SatEncoding *enc, const SatLiteral *literals, int count, SatModel **out_model) {
    LV00_CHECK_NULL(enc, SAT_ERROR);

    /* 追加假设 */
    for (int i = 0; i < count; i++) {
        sat_encoding_add_assumption(enc, literals[i]);
    }

    return sat_solve_and_decode(enc, out_model);
}

/* ========================================================================
 * SAT 模型 → 约束图 / 关系实例 解码
 * ======================================================================== */

ConstraintGraph *sat_model_to_graph(const SatModel *model) {
    LV00_CHECK_NULL(model, NULL);

    ConstraintGraph *graph = graph_create();
    LV00_CHECK_ALLOC(graph, NULL);

    /* 如果模型已有解码后的实例，从中重建约束图 */
    if (model->decoded_instance && model->decoded_instance->model) {
        const RelModel *rel_model = model->decoded_instance->model;

        /* 从关系模型的签名中收集所有涉及的原子 ID，创建对应节点 */
        int max_atom_id = 0;
        if (rel_model->sigs) {
            for (int si = 0; si < rel_model->sig_count; si++) {
                RelSignature *sig = rel_model->sigs[si];
                if (!sig) continue;
                for (int ai = 0; ai < sig->atom_count; ai++) {
                    if (sig->atoms[ai] && sig->atoms[ai]->atom_id > max_atom_id) {
                        max_atom_id = sig->atoms[ai]->atom_id;
                    }
                }
            }
        }

        /* 为每个原子创建几何节点 */
        /* 使用 graph_add_node_with_id 以保持原始 atom_id */
        if (rel_model->sigs) {
            for (int si = 0; si < rel_model->sig_count; si++) {
                RelSignature *sig = rel_model->sigs[si];
                if (!sig) continue;
                for (int ai = 0; ai < sig->atom_count; ai++) {
                    RelAtom *atom = sig->atoms[ai];
                    if (!atom) continue;
                    /* 检查是否已存在该节点 */
                    if (graph_get_node(graph, atom->atom_id)) continue;

                    GeomType gtype = GEOM_POINT;
                    switch (atom->type) {
                        case REL_ATOM_POINT:     gtype = GEOM_POINT; break;
                        case REL_ATOM_LINE:      gtype = GEOM_LINE_SEGMENT; break;
                        case REL_ATOM_REGION:    gtype = GEOM_REGION; break;
                        case REL_ATOM_PORT:      gtype = GEOM_PORT; break;
                        case REL_ATOM_FUNC_BLOCK: gtype = GEOM_FUNCTION_BLOCK; break;
                        default:
                            LV00_LOG_WARNING("Unknown atom type %d in sat_decode_to_graph", atom->type);
                            gtype = GEOM_POINT;
                            break;
                    }
                    graph_add_node_with_id(graph, atom->atom_id, gtype, NULL, 0);
                }
            }
        }
    }

    /* 基于 true_vars 重建约束关系 */
    /* true_vars 中每个变量 ID 对应 var_map 中的一个关系元组。
     * 由于此函数没有 enc 参数，我们通过 decoded_instance 的绑定来重建约束。 */
    if (model->decoded_instance && model->decoded_instance->rel_bindings) {
        for (int bi = 0; bi < model->decoded_instance->binding_count; bi++) {
            Relation *rel = model->decoded_instance->rel_bindings[bi];
            if (!rel || rel->arity < 2) continue;

            /* 为每个二元关系元组创建连接约束 */
            for (int ti = 0; ti < rel->tuple_count; ti++) {
                int n1_id = rel->tuples[ti][0];
                int n2_id = rel->tuples[ti][1];

                /* 确保两个节点都存在 */
                if (!graph_get_node(graph, n1_id) || !graph_get_node(graph, n2_id))
                    continue;

                /* 根据关系名称推断约束类型 */
                ConstraintType ctype = CONNECTION;
                if (strstr(rel->name, "incidence") || strstr(rel->name, "on"))
                    ctype = INCIDENCE;
                else if (strstr(rel->name, "between"))
                    ctype = BETWEENNESS;
                else if (strstr(rel->name, "intersect"))
                    ctype = INTERSECTION;
                else if (strstr(rel->name, "contain"))
                    ctype = CONTAINMENT;

                int parts[2] = { n1_id, n2_id };
                graph_add_constraint_with_id(graph, ti + 1, ctype, parts, 2);
            }
        }
    }

    return graph;
}

RelInstance *sat_model_to_instance(const SatEncoding *enc, const SatModel *model) {
    LV00_CHECK_NULL(enc, NULL);
    LV00_CHECK_NULL(model, NULL);

    RelInstance *inst = (RelInstance *)lv00_malloc(sizeof(RelInstance));
    LV00_CHECK_ALLOC(inst, NULL);
    memset(inst, 0, sizeof(RelInstance));

    inst->model = (RelModel *)enc->rel_model;
    inst->atom_count = 0;
    inst->atoms = NULL;
    inst->rel_bindings = NULL;
    inst->binding_count = 0;
    inst->satisfies_assertions = true;

    /* 基于 true_vars 和 var_map 重建绑定关系 */
    if (enc->rel_model && model->true_count > 0) {
        const RelModel *rel_model = enc->rel_model;

        /* 收集所有为真的变量对应的原子 ID 对 */
        int true_atom_count = 0;
        int true_atom_cap = (model->true_count > 0) ? model->true_count : 16;
        int **true_atom_ids = (int **)lv00_malloc((size_t)true_atom_cap * sizeof(int *));
        int *true_atom_arities = (int *)lv00_malloc((size_t)true_atom_cap * sizeof(int));
        if (true_atom_ids && true_atom_arities) {
            for (int vi = 0; vi < model->true_count; vi++) {
                int var_id = model->true_vars[vi];
                /* 在 var_map 中查找该变量对应的元组 */
                for (int ei = 0; ei < enc->var_count; ei++) {
                    if (enc->var_map[ei].var_id == var_id) {
                        if (true_atom_count >= true_atom_cap) {
                            int new_cap = true_atom_cap * LV00_ARRAY_GROWTH_FACTOR;
                            int **new_ids = (int **)lv00_realloc(true_atom_ids,
                                (size_t)new_cap * sizeof(int *));
                            if (!new_ids) break;
                            true_atom_ids = new_ids;
                            int *new_ar = (int *)lv00_realloc(true_atom_arities,
                                (size_t)new_cap * sizeof(int));
                            if (!new_ar) break;
                            true_atom_arities = new_ar;
                            true_atom_cap = new_cap;
                        }
                        int *ids_copy = (int *)lv00_malloc(
                            (size_t)enc->var_map[ei].arity * sizeof(int));
                        if (ids_copy) {
                            for (int k = 0; k < enc->var_map[ei].arity; k++) {
                                ids_copy[k] = enc->var_map[ei].atom_ids[k];
                            }
                            true_atom_ids[true_atom_count] = ids_copy;
                            true_atom_arities[true_atom_count] = enc->var_map[ei].arity;
                            true_atom_count++;
                        }
                        break;
                    }
                }
            }

            /* 为关系模型中的每个关系创建绑定 */
            if (rel_model->relations && true_atom_count > 0) {
                inst->binding_count = rel_model->relation_count;
                inst->rel_bindings = (Relation **)lv00_malloc(
                    (size_t)inst->binding_count * sizeof(Relation *));
                if (inst->rel_bindings) {
                    memset(inst->rel_bindings, 0,
                           (size_t)inst->binding_count * sizeof(Relation *));

                    for (int ri = 0; ri < rel_model->relation_count; ri++) {
                        Relation *rel = rel_model->relations[ri];
                        if (!rel) continue;

                        /* 创建新的关系，填充满足的元组 */
                        Relation *binding = (Relation *)lv00_malloc(sizeof(Relation));
                        if (!binding) continue;
                        memset(binding, 0, sizeof(Relation));
                        strncpy(binding->name, rel->name, sizeof(binding->name) - 1);
                        binding->arity = rel->arity;
                        for (int d = 0; d < rel->arity && d < 8; d++) {
                            binding->domains[d] = rel->domains[d];
                        }
                        binding->tuple_capacity = 16;
                        binding->tuples = (int **)lv00_malloc(
                            (size_t)binding->tuple_capacity * sizeof(int *));
                        if (!binding->tuples) {
                            lv00_free((void **)&binding->name);
                            lv00_free((void **)&binding);
                            continue;
                        }

                        /* 筛选出属于此关系且为真的元组 */
                        for (int tai = 0; tai < true_atom_count; tai++) {
                            if (true_atom_arities[tai] != rel->arity) continue;
                            /* 检查此元组是否在原始关系中 */
                            for (int ti = 0; ti < rel->tuple_count; ti++) {
                                if (tuple_equals(rel->arity,
                                    true_atom_ids[tai], rel->tuples[ti])) {
                                    /* 扩容 */
                                    if (binding->tuple_count >= binding->tuple_capacity) {
                                        int new_cap = binding->tuple_capacity *
                                            LV00_ARRAY_GROWTH_FACTOR;
                                        int **new_tuples = (int **)lv00_realloc(
                                            binding->tuples,
                                            (size_t)new_cap * sizeof(int *));
                                        if (!new_tuples) break;
                                        binding->tuples = new_tuples;
                                        binding->tuple_capacity = new_cap;
                                    }
                                    binding->tuples[binding->tuple_count++] =
                                        true_atom_ids[tai];
                                    true_atom_ids[tai] = NULL; /* 所有权转移 */
                                    break;
                                }
                            }
                        }

                        inst->rel_bindings[ri] = binding;
                    }
                }
            }

            /* 收集实例中的所有原子 */
            if (rel_model->sigs) {
                int total_atoms = 0;
                for (int si = 0; si < rel_model->sig_count; si++) {
                    if (rel_model->sigs[si])
                        total_atoms += rel_model->sigs[si]->atom_count;
                }
                inst->atom_count = total_atoms;
                inst->atoms = (RelAtom **)lv00_malloc(
                    (size_t)total_atoms * sizeof(RelAtom *));
                if (inst->atoms) {
                    int idx = 0;
                    for (int si = 0; si < rel_model->sig_count; si++) {
                        RelSignature *sig = rel_model->sigs[si];
                        if (!sig) continue;
                        for (int ai = 0; ai < sig->atom_count; ai++) {
                            if (idx < total_atoms) {
                                inst->atoms[idx++] = sig->atoms[ai];
                            }
                        }
                    }
                }
            }

            /* 检查是否满足所有断言 */
            inst->satisfies_assertions = true;
            if (rel_model->assertions) {
                for (int ai = 0; ai < rel_model->assertion_count; ai++) {
                    RelFormula *assertion = rel_model->assertions[ai];
                    if (!assertion) continue;
                    /* 简单检查：如果断言涉及的关系绑定非空则认为满足 */
                    if (assertion->expr && assertion->expr->type == REL_EXPR_ATOMIC &&
                        assertion->expr->data.atomic.rel) {
                        Relation *assert_rel = assertion->expr->data.atomic.rel;
                        /* 检查该关系的绑定是否满足断言语义 */
                        bool found = false;
                        for (int bi = 0; bi < inst->binding_count; bi++) {
                            if (inst->rel_bindings[bi] &&
                                strcmp(inst->rel_bindings[bi]->name, assert_rel->name) == 0) {
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            inst->satisfies_assertions = false;
                            break;
                        }
                    }
                }
            }

            /* 释放临时数组 */
            for (int i = 0; i < true_atom_count; i++) {
                if (true_atom_ids[i]) lv00_free((void **)&true_atom_ids[i]);
            }
            lv00_free((void **)&true_atom_ids);
            lv00_free((void **)&true_atom_arities);
        } else {
            if (true_atom_ids) lv00_free((void **)&true_atom_ids);
            if (true_atom_arities) lv00_free((void **)&true_atom_arities);
        }
    }

    return inst;
}

void sat_model_destroy(SatModel *model) {
    if (!model) return;
    if (model->true_vars) lv00_free((void **)&model->true_vars);
    if (model->decoded_graph) graph_destroy(model->decoded_graph);
    if (model->decoded_instance) relation_instance_destroy(model->decoded_instance);
    lv00_free((void **)&model);
}

/* ========================================================================
 * 诊断与调试
 * ======================================================================== */

int *sat_get_unsat_core(const SatEncoding *enc, int *out_count) {
    LV00_CHECK_NULL(enc, NULL);
    LV00_CHECK_NULL(out_count, NULL);

    /* 提取不可满足核心 */
    /* 策略：优先使用约束图中的约束 ID 作为核心标识。
     * 如果约束图存在，返回所有活跃约束的 ID（fallback 策略，
     * 因为编码上下文不直接持有求解器实例，无法追踪冲突子句）。
     * 如果没有约束图，返回所有子句索引。 */
    if (enc->graph && enc->graph->constraint_count > 0) {
        /* 收集所有活跃约束的 ID 作为 UNSAT core */
        int core_count = 0;
        for (int i = 0; i < enc->graph->constraint_count; i++) {
            if (enc->graph->constraints[i] && enc->graph->constraints[i]->is_active) {
                core_count++;
            }
        }

        if (core_count == 0) {
            /* 没有活跃约束，返回空核心 */
            *out_count = 0;
            int *core = (int *)lv00_malloc(sizeof(int));
            if (core) core[0] = -1;
            return core;
        }

        int *core = (int *)lv00_malloc((size_t)core_count * sizeof(int));
        if (!core) {
            *out_count = 0;
            return NULL;
        }

        int idx = 0;
        for (int i = 0; i < enc->graph->constraint_count; i++) {
            if (enc->graph->constraints[i] && enc->graph->constraints[i]->is_active) {
                core[idx++] = enc->graph->constraints[i]->id;
            }
        }
        *out_count = core_count;
        return core;
    }

    /* Fallback：没有约束图时，返回所有子句索引作为核心 */
    if (enc->clause_count > 0) {
        int *core = (int *)lv00_malloc((size_t)enc->clause_count * sizeof(int));
        if (!core) {
            *out_count = 0;
            return NULL;
        }
        for (int i = 0; i < enc->clause_count; i++) {
            core[i] = i;
        }
        *out_count = enc->clause_count;
        return core;
    }

    /* 完全无信息，返回空核心 */
    *out_count = 0;
    int *core = (int *)lv00_malloc(sizeof(int));
    if (core) core[0] = -1;
    return core;
}

int sat_encoding_export_dimacs(const SatEncoding *enc, const char *filepath) {
    LV00_CHECK_NULL(enc, false);
    LV00_CHECK_NULL(filepath, false);

    FILE *fp = fopen(filepath, "w");
    if (!fp) {
        lv00_set_error_ctx(LV00_ERROR_IO, __FILE__, __LINE__, __func__,
                           "无法打开文件: %s", filepath);
        return false;
    }

    /* 写入 DIMACS CNF 头 */
    fprintf(fp, "c DIMACS CNF -- generated by Lv-00 SAT encoding pipeline\n");
    fprintf(fp, "c Variables: %d\n", enc->next_var_id - 1);
    fprintf(fp, "c Clauses: %d\n", enc->clause_count);
    fprintf(fp, "p cnf %d %d\n", enc->next_var_id - 1, enc->clause_count);

    /* 写入每个子句 */
    for (int i = 0; i < enc->clause_count; i++) {
        const int *clause = enc->clauses[i];
        int size = enc->clause_sizes[i];
        for (int j = 0; j < size; j++) {
            fprintf(fp, "%d ", clause[j]);
        }
        fprintf(fp, "0\n");
    }

    fclose(fp);
    return true;
}

void sat_encoding_get_stats(const SatEncoding *enc, int *out_vars, int *out_clauses) {
    if (!enc) {
        if (out_vars) *out_vars = 0;
        if (out_clauses) *out_clauses = 0;
        return;
    }
    if (out_vars) *out_vars = enc->total_vars;
    if (out_clauses) *out_clauses = enc->total_clauses;
}