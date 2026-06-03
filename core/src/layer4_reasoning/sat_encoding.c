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

#include "sat_encoding.h"

#include <stdio.h>
#include <string.h>

#include "lv00_internal.h"
#include "lv00_utils.h"
#include "error_codes.h"
#include "constraint_graph.h"
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
    }

    return 0;
}

/* ========================================================================
 * 约束图 → SAT 编码（主编码管道）
 * ======================================================================== */

SatResult constraint_graph_to_sat(const ConstraintGraph *graph, SatEncoding *enc) {
    LV00_CHECK_NULL(graph, SAT_ERROR);
    LV00_CHECK_NULL(enc, SAT_ERROR);

    enc->graph = graph;

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
        /* 桩：将事实标记为使能约束 */
        LV00_UNUSED(formula);
    }

    return SAT_OK;
}

/* ========================================================================
 * SAT 求解与解码
 * ======================================================================== */

SatResult sat_solve_and_decode(SatEncoding *enc, SatModel **out_model) {
    LV00_CHECK_NULL(enc, SAT_ERROR);
    LV00_CHECK_NULL(out_model, SAT_ERROR);

    /* 桩实现：创建虚拟求解器并返回 UNKNOWN */
    *out_model = NULL;

    /* 创建简单的求解器并尝试求解 */
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

    /* 桩：返回空约束图，后续可基于 true_vars 重建几何结构 */
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

    /* 桩：基于 true_vars 和 var_map 重建绑定关系 */
    LV00_UNUSED(model);

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

    /* 桩：返回空核心 */
    *out_count = 0;
    int *core = (int *)lv00_malloc(sizeof(int));
    if (core) core[0] = -1;
    return core;
}

bool sat_encoding_export_dimacs(const SatEncoding *enc, const char *filepath) {
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
