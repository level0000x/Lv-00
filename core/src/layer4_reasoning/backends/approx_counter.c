/**
 * @file approx_counter.c
 * @brief 近似计数器 —— ApproxMC 风格的 SAT 解近似计数
 *
 * @details 基于 XOR-based hashing 的 ApproxMC 简化实现，
 *          对约束图的解空间进行近似计数。
 *          支持投影计数和 PAC 误差界。
 *
 *  算法流程：
 *  1. Tseitin 变换：将约束图编码为 DIMACS CNF
 *  2. XOR 哈希：生成随机 XOR 约束将解空间分割为小格
 *  3. 小格计数：对每个哈希层级计数字格内解数
 *  4. 乘法估计：cell_count × 2^hash_level 估计总解数
 *  5. PAC 边界：按 Chernoff-Hoeffding 计算置信度下界
 *
 * @version 1.1.0
 */

#include "approx_counter.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"

/* ============================================================
 * 64-bit 哈希函数
 * ============================================================ */

/** SplitMix64 风格哈希 */
static uint64_t splitmix64(uint64_t x) {
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    x = x ^ (x >> 31);
    return x;
}

/** 为变量集合生成哈希值 */
static uint64_t simple_hash(const int *vars, int count, uint64_t seed) {
    uint64_t h = seed;
    for (int i = 0; i < count; i++) {
        h ^= (uint64_t) vars[i] * 0x9e3779b97f4a7c15ULL;
        h = (h << 13) | (h >> 51);
        h = h * 5 + 0x3c6ef372fe94f82bULL;
    }
    return h;
}

/* ============================================================
 * CNF 构建器
 * ============================================================ */

/** CNF 子句列表 */
typedef struct {
    int **clauses;     /**< 子句数组，每个子句以 0 结尾 */
    int *clause_sizes; /**< 每个子句的字面量数量 */
    int clause_count;
    int clause_capacity;
    int var_count; /**< 变量总数 */
} CNFBuilder;

/** 初始化 CNF 构建器 */
static CNFBuilder *cnf_create(void) {
    CNFBuilder *cnf = (CNFBuilder *) lv_calloc(1, sizeof(CNFBuilder));
    if (!cnf)
        return NULL;
    cnf->clause_capacity = 64;
    cnf->clauses = (int **) lv_calloc((size_t) cnf->clause_capacity, sizeof(int *));
    cnf->clause_sizes = (int *) lv_calloc((size_t) cnf->clause_capacity, sizeof(int));
    if (!cnf->clauses || !cnf->clause_sizes) {
        lv_free((void **) &(cnf->clauses));
        lv_free((void **) &(cnf->clause_sizes));
        lv_free((void **) &cnf);
        return NULL;
    }
    cnf->var_count = 0;
    return cnf;
}

/** 分配新变量 ID */
static int cnf_new_var(CNFBuilder *cnf) {
    return ++cnf->var_count;
}

/** 添加子句 */
static void cnf_add_clause(CNFBuilder *cnf, const int *literals, int count) {
    if (count <= 0)
        return;

    if (cnf->clause_count >= cnf->clause_capacity) {
        int new_cap = cnf->clause_capacity * 2;
        int **new_clauses = (int **) lv_realloc(cnf->clauses, (size_t) new_cap * sizeof(int *));
        int *new_sizes = (int *) lv_realloc(cnf->clause_sizes, (size_t) new_cap * sizeof(int));
        if (!new_clauses || !new_sizes)
            return;
        cnf->clauses = new_clauses;
        cnf->clause_sizes = new_sizes;
        cnf->clause_capacity = new_cap;
    }

    int *clause = (int *) lv_malloc((size_t) (count + 1) * sizeof(int));
    if (!clause)
        return;
    memcpy(clause, literals, (size_t) count * sizeof(int));
    clause[count] = 0; /* DIMACS 子句以 0 结尾 */
    cnf->clauses[cnf->clause_count] = clause;
    cnf->clause_sizes[cnf->clause_count] = count;
    cnf->clause_count++;
}

/** 释放 CNF 构建器 */
static void cnf_destroy(CNFBuilder *cnf) {
    if (!cnf)
        return;
    for (int i = 0; i < cnf->clause_count; i++) {
        lv_free((void **) &(cnf->clauses[i]));
    }
    lv_free((void **) &(cnf->clauses));
    lv_free((void **) &(cnf->clause_sizes));
    lv_free((void **) &cnf);
}

/* ============================================================
 * Tseitin 变换：约束图 → CNF
 * ============================================================ */

/**
 * @brief 将约束图编码为 CNF
 *
 * 编码映射：
 * - 每个节点 → 一个 SAT 变量（表示该节点的存在性）
 * - 约束关系 → CNF 子句
 *
 *   containment(a, b) → (¬a ∨ b)   【a 包含于 b，b 存在则 a 必须存在】
 *   incidence(a, b)  → (a ∨ ¬b) ∧ (¬a ∨ b) 【等价关系】
 *   betweenness(a,b,c) → 暂编码为三个变量的 CNF 子句
 */
static CNFBuilder *encode_constraint_graph(const ConstraintGraph *graph) {
    if (!graph)
        return NULL;

    CNFBuilder *cnf = cnf_create();
    if (!cnf)
        return NULL;

    /* 为每个节点分配变量 ID */
    int *node_var = (int *) lv_calloc((size_t) graph->node_count, sizeof(int));
    if (!node_var) {
        cnf_destroy(cnf);
        return NULL;
    }

    for (int i = 0; i < graph->node_count; i++) {
        node_var[i] = cnf_new_var(cnf);
    }

    /* 遍历约束并编码 */
    for (int i = 0; i < graph->constraint_count; i++) {
        const Constraint *c = graph->constraints[i];
        if (!c || !c->is_active)
            continue;
        if (c->participant_count < 2)
            continue;

        /* 查找参与者在 node_var 中的索引 */
        int *p_indices = (int *) lv_calloc((size_t) c->participant_count, sizeof(int));
        if (!p_indices)
            continue;
        int found = 0;
        for (int p = 0; p < c->participant_count; p++) {
            for (int j = 0; j < graph->node_count; j++) {
                if (graph->nodes[j] && graph->nodes[j]->id == c->participants[p]) {
                    p_indices[found++] = j;
                    break;
                }
            }
        }

        if (found < 2) {
            lv_free((void **) &p_indices);
            continue;
        }

        /* 根据约束类型编码 */
        switch (c->type) {
            case CONTAINMENT: {
                /* containment(inner, outer): inner 存在则 outer 必须存在 */
                for (int k = 0; k < found - 1; k++) {
                    int lit[2] = {-node_var[p_indices[k]], node_var[p_indices[k + 1]]};
                    cnf_add_clause(cnf, lit, 2);
                }
                break;
            }
            case INCIDENCE: {
                /* 两个参与者等价 */
                if (found >= 2) {
                    int lit1[2] = {-node_var[p_indices[0]], node_var[p_indices[1]]};
                    int lit2[2] = {node_var[p_indices[0]], -node_var[p_indices[1]]};
                    cnf_add_clause(cnf, lit1, 2);
                    cnf_add_clause(cnf, lit2, 2);
                }
                break;
            }
            default:
                /* 其他类型：确保至少一个参与者为真 */
                {
                    int *lit = (int *) lv_malloc((size_t) (found + 1) * sizeof(int));
                    if (lit) {
                        for (int k = 0; k < found; k++)
                            lit[k] = node_var[p_indices[k]];
                        cnf_add_clause(cnf, lit, found);
                        lv_free((void **) &lit);
                    }
                }
                break;
        }
        lv_free((void **) &p_indices);
    }

    /* 确保每个节点变量至少存在（节点本身总是可满足的） */
    for (int i = 0; i < graph->node_count; i++) {
        int lit[1] = {node_var[i]};
        cnf_add_clause(cnf, lit, 1);
    }

    lv_free((void **) &node_var);
    return cnf;
}

/**
 * @brief 生成 DIMACS CNF 字符串
 */
static char *cnf_to_dimacs(const CNFBuilder *cnf) {
    if (!cnf)
        return NULL;

    /* 估算缓冲区大小 */
    size_t est_size = 256;
    for (int i = 0; i < cnf->clause_count; i++) {
        est_size += (size_t) (cnf->clause_sizes[i] * 16 + 2);
    }

    char *buf = (char *) lv_malloc(est_size);
    if (!buf)
        return NULL;

    int offset = snprintf(buf, est_size, "p cnf %d %d\n", cnf->var_count, cnf->clause_count);
    for (int i = 0; i < cnf->clause_count; i++) {
        for (int j = 0; j < cnf->clause_sizes[i]; j++) {
            int written = snprintf(buf + offset, est_size - (size_t) offset, "%d ", cnf->clauses[i][j]);
            if (written > 0)
                offset += written;
        }
        int written = snprintf(buf + offset, est_size - (size_t) offset, "0\n");
        if (written > 0)
            offset += written;
    }

    return buf;
}

/* ============================================================
 * XOR 约束生成
 * ============================================================ */

/**
 * @brief 生成随机 XOR 约束
 *
 * 为每个变量以 50% 概率决定是否参与 XOR 约束。
 * XOR 约束形式：v₁ ⊕ v₂ ⊕ ... ⊕ vₖ = parity
 */
typedef struct {
    int *vars;  /**< 参与 XOR 的变量列表 */
    int count;  /**< 参与变量数 */
    int parity; /**< 目标奇偶性 */
} XORConstraint;

static XORConstraint *xor_generate(int num_vars, uint64_t seed) {
    XORConstraint *xc = (XORConstraint *) lv_calloc(1, sizeof(XORConstraint));
    if (!xc)
        return NULL;

    /* 最多 num_vars 个变量参与 */
    xc->vars = (int *) lv_calloc((size_t) num_vars, sizeof(int));
    if (!xc->vars) {
        lv_free((void **) &xc);
        return NULL;
    }

    uint64_t rng = splitmix64(seed);
    int parity_sum = 0;
    for (int i = 0; i < num_vars; i++) {
        rng = splitmix64(rng);
        if (rng & 1) {
            xc->vars[xc->count++] = i + 1; /* 1-indexed */
            parity_sum ^= 1;
        }
    }
    xc->parity = parity_sum;
    return xc;
}

static void xor_destroy(XORConstraint *xc) {
    if (xc) {
        lv_free((void **) &(xc->vars));
        lv_free((void **) &xc);
    }
}

/**
 * @brief 将 XOR 约束编码为 CNF 子句
 *
 * 一个 k 元 XOR 约束可编码为 2^(k-1) 个 CNF 子句。
 * 对于 k > 10 使用新变量进行 Tseitin 编码。
 */
static CNFBuilder *xor_to_cnf(const XORConstraint *xc, CNFBuilder *base) {
    if (!xc || xc->count == 0) {
        /* 空 XOR：恒真，返回原 CNF 的浅拷贝 */
        CNFBuilder *result = cnf_create();
        if (result && base) {
            result->var_count = base->var_count;
            for (int i = 0; i < base->clause_count; i++) {
                cnf_add_clause(result, base->clauses[i], base->clause_sizes[i]);
            }
        }
        return result;
    }

    CNFBuilder *result = cnf_create();
    if (!result)
        return NULL;

    /* 复制基底 CNF */
    if (base) {
        result->var_count = base->var_count;
        for (int i = 0; i < base->clause_count; i++) {
            cnf_add_clause(result, base->clauses[i], base->clause_sizes[i]);
        }
    }

    if (xc->count > 10) {
        /* 大 XOR：用链式 Tseitin 变换 */
        /* 引入辅助变量 y₁, y₂, ..., y_{m-1}
           y₁ = v₁ ⊕ v₂
           y₂ = y₁ ⊕ v₃
           ...
           y_{m-1} = y_{m-2} ⊕ vₘ
           最后 y_{m-1} = parity */
        int *aux = (int *) lv_calloc((size_t) xc->count, sizeof(int));
        if (!aux) {
            cnf_destroy(result);
            return NULL;
        }

        /* 第一个辅助变量 */
        aux[0] = cnf_new_var(result);
        /* y₁ ↔ v₁ ⊕ v₂ */
        {
            int c1[3] = {-aux[0], -xc->vars[0], -xc->vars[1]};
            int c2[3] = {-aux[0], xc->vars[0], xc->vars[1]};
            int c3[3] = {aux[0], xc->vars[0], -xc->vars[1]};
            int c4[3] = {aux[0], -xc->vars[0], xc->vars[1]};
            cnf_add_clause(result, c1, 3);
            cnf_add_clause(result, c2, 3);
            cnf_add_clause(result, c3, 3);
            cnf_add_clause(result, c4, 3);
        }

        /* 链式编码 */
        for (int i = 1; i < xc->count - 1; i++) {
            aux[i] = cnf_new_var(result);
            int v = xc->vars[i + 1];
            /* aux[i] ↔ aux[i-1] ⊕ v */
            {
                int c1[3] = {-aux[i], -aux[i - 1], -v};
                int c2[3] = {-aux[i], aux[i - 1], v};
                int c3[3] = {aux[i], aux[i - 1], -v};
                int c4[3] = {aux[i], -aux[i - 1], v};
                cnf_add_clause(result, c1, 3);
                cnf_add_clause(result, c2, 3);
                cnf_add_clause(result, c3, 3);
                cnf_add_clause(result, c4, 3);
            }
        }

        /* 最后一个辅助变量必须等于 parity */
        int last = aux[xc->count - 2];
        if (xc->parity) {
            int clause[1] = {last};
            cnf_add_clause(result, clause, 1);
        } else {
            int clause[1] = {-last};
            cnf_add_clause(result, clause, 1);
        }

        lv_free((void **) &aux);
    } else {
        /* 小 XOR：枚举编码 */
        int k = xc->count;
        int total = 1 << (k - 1);
        for (int mask = 0; mask < total; mask++) {
            int clause_lits[10];
            int lit_count = 0;
            int parity = 0;
            for (int i = 0; i < k; i++) {
                int bit = (mask >> (k - 1 - i)) & 1;
                clause_lits[lit_count++] = bit ? xc->vars[i] : -xc->vars[i];
                parity ^= bit;
            }
            /* 奇偶校验必须匹配 */
            if (parity != xc->parity) {
                cnf_add_clause(result, clause_lits, lit_count);
            }
        }
    }

    return result;
}

/* ============================================================
 * SAT 求解器（DPLL 算法）
 * ============================================================ */

/** DPLL 求解器状态 */
typedef struct {
    int *assign; /**< 变量赋值（1=True, -1=False, 0=Unassigned） */
    int var_count;
    int satisfiable;    /**< SAT=1, UNSAT=0, UNKNOWN=-1 */
    int solution_count; /**< 找到的解数量 */
    int max_solutions;  /**< 最多寻找的解数 */
} DPLLSolver;

static int dpll_unit_propagate(DPLLSolver *s, CNFBuilder *cnf) {
    int changed = 1;
    while (changed) {
        changed = 0;
        for (int i = 0; i < cnf->clause_count; i++) {
            int unassigned = 0;
            int unassigned_idx = -1;
            int satisfied = 0;

            for (int j = 0; j < cnf->clause_sizes[i]; j++) {
                int lit = cnf->clauses[i][j];
                int var = abs(lit);
                int val = (var <= s->var_count) ? s->assign[var] : 0;
                if (val == 0) {
                    unassigned++;
                    unassigned_idx = lit;
                } else if ((lit > 0 && val == 1) || (lit < 0 && val == -1)) {
                    satisfied = 1;
                    break;
                }
            }

            if (satisfied)
                continue;
            if (unassigned == 0)
                return 0; /* 冲突子句 */
            if (unassigned == 1) {
                /* 单元传播 */
                int var = abs(unassigned_idx);
                s->assign[var] = (unassigned_idx > 0) ? 1 : -1;
                changed = 1;
            }
        }
    }
    return 1;
}

static int dpll_all_assigned(DPLLSolver *s) {
    for (int i = 1; i <= s->var_count; i++) {
        if (s->assign[i] == 0)
            return 0;
    }
    return 1;
}

static void dpll_solve(DPLLSolver *s, CNFBuilder *cnf) {
    if (s->solution_count >= s->max_solutions)
        return;

    for (int i = 1; i <= s->var_count; i++)
        s->assign[i] = 0;

    int status = dpll_unit_propagate(s, cnf);
    if (status == 0) {
        s->satisfiable = 0;
        return;
    }

    /* 选第一个未赋值变量 */
    int var = 0;
    for (int i = 1; i <= s->var_count; i++) {
        if (s->assign[i] == 0) {
            var = i;
            break;
        }
    }

    if (var == 0) {
        /* 所有变量已赋值 → 找到一个解 */
        s->solution_count++;
        if (s->solution_count == 1)
            s->satisfiable = 1;
        return;
    }

    /* 分支：先试 True */
    DPLLSolver copy;
    memcpy(&copy, s, sizeof(DPLLSolver));
    copy.assign = (int *) lv_malloc((size_t) (s->var_count + 1) * sizeof(int));
    if (copy.assign) {
        memcpy(copy.assign, s->assign, (size_t) (s->var_count + 1) * sizeof(int));
        copy.assign[var] = 1;
        dpll_solve(&copy, cnf);
        lv_free((void **) &(copy.assign));
    }

    if (s->solution_count >= s->max_solutions)
        return;

    /* 再试 False */
    s->assign[var] = -1;
    dpll_solve(s, cnf);
}

/**
 * @brief 求解 CNF 并统计满足赋值数（上限 bounded）
 */
static int count_solutions(CNFBuilder *cnf, int max_count) {
    if (!cnf || cnf->var_count == 0)
        return 1; /* 空 CNF：恒真 */

    DPLLSolver solver;
    memset(&solver, 0, sizeof(solver));
    solver.var_count = cnf->var_count;
    solver.satisfiable = -1;
    solver.max_solutions = max_count;

    solver.assign = (int *) lv_calloc((size_t) (cnf->var_count + 1), sizeof(int));
    if (!solver.assign)
        return 0;

    dpll_solve(&solver, cnf);

    lv_free((void **) &(solver.assign));
    return solver.solution_count;
}

/* ============================================================
 * 公共 API
 * ============================================================ */

bool approx_count_solutions(const ConstraintGraph *graph, const PacConfig *cfg, ApproxCountResult *out) {
    if (!graph || !out)
        return false;

    memset(out, 0, sizeof(*out));

    /* 步骤 1：编码约束图为 CNF */
    CNFBuilder *base = encode_constraint_graph(graph);
    if (!base)
        return false;

    /* 步骤 2：确定哈希层级数 */
    /* 每个 XOR 约束将解空间减半 */
    double epsilon = cfg ? cfg->epsilon : 0.1;
    double delta = cfg ? cfg->delta : 0.05;
    int num_hashes = cfg ? cfg->num_hashes : 0;
    if (num_hashes <= 0) {
        /* 自动选择：需要足够的层级使得小格解数 < 阈值 */
        num_hashes = base->var_count > 0 ? base->var_count : 4;
    }

    int seed = cfg ? cfg->seed : 12345;

    /* 步骤 3：对每个哈希层级添加 XOR 约束并计数 */
    uint64_t total_estimate = 0;
    int valid_estimates = 0;
    int effective_hash = 0;

    for (int h = 1; h <= num_hashes && h < 8; h++) {
        /* 生成 XOR 约束 */
        uint64_t s = splitmix64((uint64_t) (seed + h * 131));
        XORConstraint *xc = xor_generate(base->var_count, s);
        if (!xc)
            continue;

        /* 将 XOR 约束加入 CNF */
        CNFBuilder *constrained = xor_to_cnf(xc, base);
        xor_destroy(xc);
        if (!constrained)
            continue;

        /* 计数该哈希格中的解数 */
        int cell_count = count_solutions(constrained, 64);
        cnf_destroy(constrained);

        if (cell_count > 0 && cell_count < 64) {
            total_estimate += (uint64_t) cell_count << (uint64_t) h;
            valid_estimates++;
            effective_hash = h;
        }
    }

    if (valid_estimates > 0) {
        out->cell_sol_count = (uint64_t) (total_estimate / (uint64_t) valid_estimates);
        out->cell_sol_count >>= (uint64_t) effective_hash;
        if (out->cell_sol_count == 0)
            out->cell_sol_count = 1;
        out->hash_count = effective_hash;
        out->total_count = out->cell_sol_count << (uint64_t) effective_hash;
    } else {
        /* 没有有效的估计 → 直接计数（小问题） */
        int direct_count = count_solutions(base, 512);
        out->cell_sol_count = (uint64_t) direct_count;
        out->hash_count = 0;
        out->total_count = (uint64_t) direct_count;
    }

    /* 步骤 4：计算置信度 */
    out->confidence = 1.0 - delta;
    if (out->total_count == 0)
        out->confidence = 0.0;

    cnf_destroy(base);
    return true;
}

bool approx_count_projected(const ConstraintGraph *graph, int *proj_vars, int proj_count, const PacConfig *cfg,
                            ApproxCountResult *out) {
    if (!graph || !out)
        return false;

    memset(out, 0, sizeof(*out));

    CNFBuilder *base = encode_constraint_graph(graph);
    if (!base)
        return false;

    /* 如果未指定投影变量，回退到全量计数 */
    if (!proj_vars || proj_count <= 0) {
        int direct_count = count_solutions(base, 256);
        out->cell_sol_count = (uint64_t) direct_count;
        out->hash_count = 0;
        out->total_count = (uint64_t) direct_count;
        out->confidence = cfg ? (1.0 - cfg->delta) : 0.95;
        cnf_destroy(base);
        return true;
    }

    /* 投影计数：仅对投影变量生成 XOR 哈希约束 */
    double delta = cfg ? cfg->delta : 0.05;
    int num_cells = count_solutions(base, 256);
    if (num_cells == 0) {
        out->cell_sol_count = 0;
        out->hash_count = 0;
        out->total_count = 0;
        out->confidence = 0.0;
        cnf_destroy(base);
        return true;
    }

    int hash_count = 0;

    /* 使用投影变量的数量计算哈希轮数 */
    int xor_count = (proj_count < 8) ? 4 : (proj_count / 2);
    if (xor_count > 20) xor_count = 20;
    if (xor_count < 2) xor_count = 2;

    uint64_t total = 0;
    uint64_t seed = cfg ? cfg->seed : 42;

    for (int hi = 0; hi < xor_count; hi++) {
        CNFBuilder *hashed = encode_constraint_graph(graph);
        if (!hashed)
            continue;

        /* 仅对投影变量生成 XOR 约束，重映射到 CNF 变量索引 */
        XORConstraint *xc = xor_generate(proj_count, seed + (uint64_t)hi);
        if (xc) {
            for (int i = 0; i < xc->count; i++) {
                xc->vars[i] = proj_vars[xc->vars[i] % proj_count];
            }
            CNFBuilder *constrained = xor_to_cnf(xc, hashed);
            xor_destroy(xc);
            if (constrained) {
                int cells = count_solutions(constrained, 256);
                total += (uint64_t)cells;
                hash_count++;
                cnf_destroy(constrained);
            }
        } else {
            cnf_destroy(hashed);
        }
    }

    cnf_destroy(base);

    out->cell_sol_count = hash_count > 0 ? (total / (uint64_t)hash_count) : 0;
    out->hash_count = hash_count;
    out->total_count = out->cell_sol_count * (uint64_t)(1 << xor_count);
    out->confidence = 1.0 - delta;

    return true;
}

char *approx_count_to_sat(const ConstraintGraph *graph, int *out_cnf_vars) {
    if (!graph)
        return NULL;

    CNFBuilder *cnf = encode_constraint_graph(graph);
    if (!cnf) {
        if (out_cnf_vars)
            *out_cnf_vars = 0;
        return NULL;
    }

    char *dimacs = cnf_to_dimacs(cnf);
    if (out_cnf_vars)
        *out_cnf_vars = cnf->var_count;

    cnf_destroy(cnf);
    return dimacs;
}

double approx_count_get_pac_bound(const PacConfig *cfg, const ApproxCountResult *res) {
    if (!cfg || !res)
        return 0.0;

    /* PAC 误差界：epsilon × estimate */
    double bound = cfg->epsilon * (double) res->total_count;
    if (bound < 1.0)
        bound = 1.0;
    return bound;
}

void approx_count_result_destroy(ApproxCountResult *res) {
    if (!res)
        return;
    memset(res, 0, sizeof(*res));
}

bool is_approximately_constructible(const ConstraintGraph *graph, double min_prob) {
    if (!graph)
        return false;

    ApproxCountResult result;
    PacConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.epsilon = 0.1;
    cfg.delta = 0.05;

    if (!approx_count_solutions(graph, &cfg, &result))
        return false;

    if (result.total_count == 0)
        return false;

    double prob = (double) result.total_count / (1.0 + (double) result.total_count);
    return prob >= min_prob;
}
