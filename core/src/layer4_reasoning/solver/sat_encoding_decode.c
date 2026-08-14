/**
 * @file sat_encoding_decode.c
 * @brief SAT 模型求解与解码（由 sat_encoding.c 拆分子模块）
 *
 * @details 求解入口（含增量求解）、SAT 赋值解码回约束图/关系实例。
 * @author Lv-00 Project
 * @version 3.3.0
 */
#include "lv/sat_encoding.h"

#include <stdio.h>
#include <string.h>

#include "lv/constraint_graph.h"

#include "lv/error_codes.h"
#include "lv/lv_internal.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_utils.h"
#include "lv/solver_core.h"
#include "sat_encoding_internal.h"
/* ========================================================================
 * SAT 求解与解码
 * ======================================================================== */

SatResult sat_solve_and_decode(SatEncoding *enc, SatModel **out_model) {
    lv_CHECK_NULL(enc, SAT_ERROR);
    lv_CHECK_NULL(out_model, SAT_ERROR);

    /* 创建 CDCL 求解器，添加所有编码子句，求解并提取模型 */
    lvSolver *solver = lv_solver_create();
    if (!solver)
        return SAT_ERROR;

    /* 将编码的子句加入求解器，同时计算最大变量 ID */
    int max_var_id = 0;
    for (int i = 0; i < enc->clause_count; i++) {
        int *clause = enc->clauses[i];
        int size = enc->clause_sizes[i];
        lvSolverLit *lits = (lvSolverLit *) lv_malloc((size_t) size * sizeof(lvSolverLit));
        if (!lits) {
            lv_solver_destroy(solver);
            return SAT_ERROR;
        }
        for (int j = 0; j < size; j++) {
            lits[j] = clause[j];
            int var = (clause[j] < 0) ? -clause[j] : clause[j];
            if (var > max_var_id)
                max_var_id = var;
        }
        lv_solver_add_constraint(solver, lits, size);
        lv_free((void **) &lits);
    }

    /* 确保求解器注册了子句中引用的所有变量 */
    if (max_var_id > lv_solver_var_count(solver)) {
        lv_solver_new_vars(solver, max_var_id - lv_solver_var_count(solver));
    }

    lvSolverResult result = lv_solver_solve(solver);

    if (result == lv_SOLVER_SAT) {
        SatModel *model = (SatModel *) lv_malloc(sizeof(SatModel));
        if (!model) {
            lv_solver_destroy(solver);
            return SAT_ERROR;
        }
        memset(model, 0, sizeof(SatModel));
        model->var_count = enc->total_vars;
        model->true_count = 0;
        model->true_vars = (int *) lv_malloc((size_t) enc->total_vars * sizeof(int));
        if (!model->true_vars) {
            lv_free((void **) &model);
            lv_solver_destroy(solver);
            return SAT_ERROR;
        }

        /* 收集赋值为真的变量 */
        for (int v = 1; v <= enc->next_var_id - 1; v++) {
            int val = lv_solver_get_value(solver, v);
            if (val > 0) {
                model->true_vars[model->true_count++] = v;
            }
        }

        model->decoded_graph = NULL;
        model->decoded_instance = NULL;
        *out_model = model;
    } else {
        /* 协同求解：SAT 无解 → 代数回退（Gröbner 基验证）
         *
         * 语义（见 doc/docs/14_solver_backends.md「代数协同」）：
         * CDCL 判定 UNSAT/UNKNOWN 时，对同一组 CNF 子句调用代数路径
         * lv_solver_solve_algebraic()（内部经 groebner_parallel 计算
         * 子句理想的多项式 Gröbner 基）。
         *   - 代数路径证实无解（约化基含非零常数 1，理想 = <1>）
         *     → 结果收敛为 UNSAT（可将 CDCL 的 UNKNOWN 升级为确定判定）；
         *   - 代数路径判定有解或无法判定 → 保持原 CDCL 判定不变。
         * 注：当前 groebner_parallel API 只提供 SAT/UNSAT 判定，不提供
         * 具体模型/坐标提取，故代数有解时无法解码坐标输出，不伪造模型，
         * 以原判定返回（安全优先）。
         * 安全保证：SAT 有解路径完全不经由此分支，行为逐位不变。 */
        lvSolverResult algebraic = lv_solver_solve_algebraic(solver);
        if (algebraic == lv_SOLVER_UNSAT) {
            result = lv_SOLVER_UNSAT;
        }
    }

    lv_solver_destroy(solver);

    /* 查找表：lvSolverResult → SatResult */
    {
        static const SatResult kSolverResultMap[] = {
            SAT_UNKNOWN,  /* lv_SOLVER_UNKNOWN = 0 */
            SAT_OK,       /* lv_SOLVER_SAT     = 1 */
            SAT_UNSAT     /* lv_SOLVER_UNSAT   = 2 */
        };
        SatResult sr = (result >= 0 && result < (int)(sizeof(kSolverResultMap) / sizeof(kSolverResultMap[0])))
                            ? kSolverResultMap[result]
                            : SAT_UNKNOWN;
        return sr;
    }
}

SatResult sat_solve_incremental(SatEncoding *enc, const SatLiteral *literals, int count, SatModel **out_model) {
    lv_CHECK_NULL(enc, SAT_ERROR);

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
    lv_CHECK_NULL(model, NULL);

    ConstraintGraph *graph = graph_create();
    lv_CHECK_ALLOC(graph, NULL);

    /* 如果模型已有解码后的实例，从中重建约束图 */
    if (model->decoded_instance && model->decoded_instance->model) {
        const RelModel *rel_model = model->decoded_instance->model;

        /* 从关系模型的签名中收集所有涉及的原子 ID，创建对应节点 */
        int max_atom_id = 0;
        if (rel_model->sigs) {
            for (int si = 0; si < rel_model->sig_count; si++) {
                RelSignature *sig = rel_model->sigs[si];
                if (!sig)
                    continue;
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
                if (!sig)
                    continue;
                for (int ai = 0; ai < sig->atom_count; ai++) {
                    RelAtom *atom = sig->atoms[ai];
                    if (!atom)
                        continue;
                    /* 检查是否已存在该节点 */
                    if (graph_get_node(graph, atom->atom_id))
                        continue;

                    GeomType gtype;
                    /* 查找表：RelAtomType → GeomType */
                    {
                        static const GeomType kAtomTypeToGeomMap[] = {
                            GEOM_POINT,          /* REL_ATOM_POINT      = 0 */
                            GEOM_LINE_SEGMENT,   /* REL_ATOM_LINE       = 1 */
                            GEOM_REGION,         /* REL_ATOM_REGION     = 2 */
                            GEOM_PORT,           /* REL_ATOM_PORT       = 3 */
                            GEOM_FUNCTION_BLOCK  /* REL_ATOM_FUNC_BLOCK = 4 */
                        };
                        if (atom->type >= 0 && atom->type < (int)(sizeof(kAtomTypeToGeomMap) / sizeof(kAtomTypeToGeomMap[0]))) {
                            gtype = kAtomTypeToGeomMap[atom->type];
                        } else {
                            lv_LOG_WARNING("Unknown atom type %d in sat_decode_to_graph", atom->type);
                            gtype = GEOM_POINT;
                        }
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
            if (!rel || rel->arity < 2)
                continue;

            /* 为每个二元关系元组创建连接约束 */
            for (int ti = 0; ti < rel->tuple_count; ti++) {
                int n1_id = rel->tuples[ti][0];
                int n2_id = rel->tuples[ti][1];

                /* 确保两个节点都存在 */
                if (!graph_get_node(graph, n1_id) || !graph_get_node(graph, n2_id))
                    continue;

                /* 根据关系名称推断约束类型（子串关键词查找表） */
                static const char *const kRelTypeKeywords[] = {"incidence", "on", "between", "intersect", "contain", NULL};
                ConstraintType ctype = CONNECTION;
                int kidx = lv_str_match_any(rel->name, kRelTypeKeywords);
                if (kidx >= 0) {
                    static const ConstraintType kRelTypes[] = {INCIDENCE, INCIDENCE, BETWEENNESS, INTERSECTION, CONTAINMENT};
                    ctype = kRelTypes[kidx];
                }

                int parts[2] = {n1_id, n2_id};
                graph_add_constraint_with_id(graph, ti + 1, ctype, parts, 2);
            }
        }
    }

    return graph;
}

RelInstance *sat_model_to_instance(const SatEncoding *enc, const SatModel *model) {
    lv_CHECK_NULL(enc, NULL);
    lv_CHECK_NULL(model, NULL);

    RelInstance *inst = (RelInstance *) lv_malloc(sizeof(RelInstance));
    lv_CHECK_ALLOC(inst, NULL);
    memset(inst, 0, sizeof(RelInstance));

    inst->model = (RelModel *) enc->rel_model;
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
        int **true_atom_ids = (int **) lv_malloc((size_t) true_atom_cap * sizeof(int *));
        int *true_atom_arities = (int *) lv_malloc((size_t) true_atom_cap * sizeof(int));
        if (true_atom_ids && true_atom_arities) {
            for (int vi = 0; vi < model->true_count; vi++) {
                int var_id = model->true_vars[vi];
                /* 在 var_map 中查找该变量对应的元组 */
                for (int ei = 0; ei < enc->var_map.count; ei++) {
                        SatVarEntry *ve = (SatVarEntry *)lv_darray_get(&enc->var_map, ei);
                        if (!ve || ve->var_id != var_id)
                            continue;
                        if (true_atom_count >= true_atom_cap) {
                            int old_cap = true_atom_cap;
                            /* 第一次：扩容 true_atom_ids（溢出检查由 lv_ensure_capacity 内部完成） */
                            if (!lv_ensure_capacity((void **) &true_atom_ids, old_cap,
                                                    &true_atom_cap, sizeof(int *), 1))
                                break;
                            /* 第二次：扩容 true_atom_arities。临时回退容量指针使扩容真实执行 */
                            true_atom_cap = old_cap;
                            if (!lv_ensure_capacity((void **) &true_atom_arities, old_cap,
                                                    &true_atom_cap, sizeof(int), 1))
                                break;
                        }
                        int *ids_copy = (int *) lv_malloc((size_t) ve->arity * sizeof(int));
                        if (ids_copy) {
                            for (int k = 0; k < ve->arity; k++) {
                                ids_copy[k] = ve->atom_ids[k];
                            }
                            true_atom_ids[true_atom_count] = ids_copy;
                            true_atom_arities[true_atom_count] = ve->arity;
                            true_atom_count++;
                        }
                        break;
                    }
            }

            /* 为关系模型中的每个关系创建绑定 */
            if (rel_model->relations && true_atom_count > 0) {
                inst->binding_count = rel_model->relation_count;
                inst->rel_bindings = (Relation **) lv_malloc((size_t) inst->binding_count * sizeof(Relation *));
                if (inst->rel_bindings) {
                    memset(inst->rel_bindings, 0, (size_t) inst->binding_count * sizeof(Relation *));

                    for (int ri = 0; ri < rel_model->relation_count; ri++) {
                        Relation *rel = rel_model->relations[ri];
                        if (!rel)
                            continue;

                        /* 创建新的关系，填充满足的元组 */
                        Relation *binding = (Relation *) lv_calloc(1, sizeof(Relation));
                        if (!binding)
                            continue;
                        lv_strlcpy(binding->name, rel->name, sizeof(binding->name));
                        binding->arity = rel->arity;
                        for (int d = 0; d < rel->arity && d < 8; d++) {
                            binding->domains[d] = rel->domains[d];
                        }
                        binding->tuple_capacity = 16;
                        binding->tuples = (int **) lv_malloc((size_t) binding->tuple_capacity * sizeof(int *));
                        if (!binding->tuples) {
                            /* binding->name 为 Relation 内嵌固定数组（char name[128]），
                             * 随 lv_calloc 统一分配，不可单独 lv_free（否则堆损坏），
                             * 仅释放 binding 本身即可 */
                            lv_free((void **) &binding);
                            continue;
                        }

                        /* 筛选出属于此关系且为真的元组 */
                        for (int tai = 0; tai < true_atom_count; tai++) {
                            if (true_atom_arities[tai] != rel->arity)
                                continue;
                            /* 检查此元组是否在原始关系中 */
                            for (int ti = 0; ti < rel->tuple_count; ti++) {
                                if (tuple_equals(rel->arity, true_atom_ids[tai], rel->tuples[ti])) {
                                    /* 扩容 */
                                    if (binding->tuple_count >= binding->tuple_capacity) {
                                        if (!lv_ensure_capacity((void **) &binding->tuples, binding->tuple_count + 1,
                                                                &binding->tuple_capacity, sizeof(int *), 0))
                                            break;
                                    }
                                    binding->tuples[binding->tuple_count++] = true_atom_ids[tai];
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
                inst->atoms = (RelAtom **) lv_malloc((size_t) total_atoms * sizeof(RelAtom *));
                if (inst->atoms) {
                    int idx = 0;
                    for (int si = 0; si < rel_model->sig_count; si++) {
                        RelSignature *sig = rel_model->sigs[si];
                        if (!sig)
                            continue;
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
                    if (!assertion)
                        continue;
                    /* 简单检查：如果断言涉及的关系绑定非空则认为满足 */
                    if (assertion->expr && assertion->expr->type == REL_EXPR_ATOMIC &&
                        assertion->expr->data.atomic.rel) {
                        Relation *assert_rel = assertion->expr->data.atomic.rel;
                        /* 检查该关系的绑定是否满足断言语义 */
                        bool found = false;
                        for (int bi = 0; bi < inst->binding_count; bi++) {
                            if (inst->rel_bindings[bi] && lv_str_eq(inst->rel_bindings[bi]->name, assert_rel->name)) {
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
                if (true_atom_ids[i])
                    lv_free((void **) &true_atom_ids[i]);
            }
            lv_free((void **) &true_atom_ids);
            lv_free((void **) &true_atom_arities);
        } else {
            if (true_atom_ids)
                lv_free((void **) &true_atom_ids);
            if (true_atom_arities)
                lv_free((void **) &true_atom_arities);
        }
    }

    return inst;
}

void sat_model_destroy(SatModel *model) {
    if (!model)
        return;
    if (model->true_vars)
        lv_free((void **) &model->true_vars);
    if (model->decoded_graph)
        graph_destroy(model->decoded_graph);
    if (model->decoded_instance)
        relation_instance_destroy(model->decoded_instance);
    lv_free((void **) &model);
}

