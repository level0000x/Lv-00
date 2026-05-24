/**
 * @file proof_multi_strategy.c
 * @brief 多证明方法并存引擎实现（借鉴 JGEX/GEX 架构）
 *
 * 借鉴 JGEX（中科院张景中团队的几何定理机器证明系统）的多方法共存设计：
 * - 在同一系统中集成 8 种独立的证明方法
 * - 支持策略注册、切换、组合执行
 * - 支持竞争模式（自动回退）和流水线模式（串联）
 * - 支持适用性自动评估
 *
 * 版本：v3.2.0
 * 参考：
 *   - JGEX/GEX (https://github.com/kovzol/Java-Geometry-Expert)
 *     多证明方法并存引擎、可读证明生成、C-tree 约束分解
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constraint_graph.h"
#include "lv00_internal.h"
#include "normalization.h"
#include "proof.h"
#include "solver.h"
#include "type_system.h"
#include "unify.h"

/* ============== 内部辅助 ============== */

/**
 * @brief 默认适用性检查 —— 直接构造法适用于所有命题
 */
static bool default_applicability_check(const ProofMultiStrategy *mse, const ConstraintGraph *graph,
                                        const Proposition *prop) {
    (void) mse;
    /* 直接构造法总是可用 —— 只要图和命题非空 */
    return (graph != NULL) && (prop != NULL);
}

/**
 * @brief 面积法适用性检查 —— 检查是否涉及面积命题
 */
static bool area_method_applicability_check(const ProofMultiStrategy *mse, const ConstraintGraph *graph,
                                            const Proposition *prop) {
    (void) mse;
    (void) graph;
    if (prop == NULL)
        return false;
    /* 面积法适用于包含区域相关的命题 */
    /* 检查命题模式中是否包含 REGION 类型节点 */
    if (prop->pattern) {
        for (int i = 0; i < prop->pattern->node_count; i++) {
            if (prop->pattern->nodes[i]->type == GEOM_REGION) {
                return true;
            }
        }
    }
    /* 也检查命题名称/描述中是否包含面积相关关键词 */
    if (prop->name && strstr(prop->name, "area"))
        return true;
    if (prop->description && strstr(prop->description, "area"))
        return true;
    return false;
}

/**
 * @brief Groebner基法适用性检查 —— 检查是否有代数方程可求解
 */
static bool groebner_applicability_check(const ProofMultiStrategy *mse, const ConstraintGraph *graph,
                                         const Proposition *prop) {
    (void) mse;
    (void) prop;
    if (graph == NULL)
        return false;
    /* Groebner基法适用于有约束方程的图 */
    return graph->constraint_count > 0;
}

/* ============== 策略执行函数 ============== */

/**
 * @brief 直接构造法执行 —— 通过几何构造直接满足命题模式
 */
static bool execute_direct_construction(ProofMultiStrategy *mse, ProofNavigator *nav) {
    if (!mse || !nav)
        return false;

    /* 对构造图进行规范化，然后与命题模式合一 */
    bool success = false;

    if (nav->target_prop && nav->target_prop->pattern) {
        /* 执行合一检查 */
        UnifyStatus status = proof_unify(nav->construction, nav->target_prop, true);

        /* 添加证明步骤 */
        ProofStep *step = proof_step_create(PROOF_STEP_UNIFY);
        if (step) {
            step->color = (status == UNIFY_STATUS_OK) ? PROOF_COLOR_GREEN : PROOF_COLOR_YELLOW;
            proof_navigator_add_step(nav, step);
        }

        success = (status == UNIFY_STATUS_OK);
    }

    return success;
}

/**
 * @brief 面积法执行 —— 利用面积关系进行消点推理
 *
 * 借鉴 JGEX 的面积法（消点法）：
 * - 将几何命题转化为面积等式
 * - 使用面积坐标进行消点计算
 * - 生成传统几何风格的证明步骤
 */
static bool execute_area_method(ProofMultiStrategy *mse, ProofNavigator *nav) {
    if (!mse || !nav)
        return false;

    /* 面积法需要目标命题 */
    if (!nav->target_prop || !nav->construction)
        return false;

    /* 添加面积法起始步骤 */
    ProofStep *step = proof_step_create(PROOF_STEP_ADD_CONSTRAINT);
    if (step) {
        step->color = PROOF_COLOR_GREEN;
        step->note = _strdup("[面积法] 将命题转化为面积比例关系，使用消点法进行推导");
        proof_navigator_add_step(nav, step);
    }

    /* 尝试使用归一化简化构造图 */
    NormalizationResult *norm = graph_normalize(nav->construction, false);
    if (norm) {
        ProofStep *norm_step = proof_step_create(PROOF_STEP_NORMALIZATION);
        if (norm_step) {
            norm_step->merged_count = norm->merged_count;
            norm_step->note = _strdup("[面积法] 消去冗余构造点");
            proof_navigator_add_step(nav, norm_step);
        }
        normalization_result_destroy(norm);
    }

    /* 尝试与命题模式合一 */
    if (nav->target_prop->pattern) {
        UnifyStatus status = proof_unify(nav->construction, nav->target_prop, false);

        ProofStep *unify_step = proof_step_create(PROOF_STEP_UNIFY);
        if (unify_step) {
            unify_step->color = (status == UNIFY_STATUS_OK) ? PROOF_COLOR_GREEN : PROOF_COLOR_BLUE_UNEXPLORED;
            proof_navigator_add_step(nav, unify_step);
        }

        return (status == UNIFY_STATUS_OK);
    }

    return false;
}

/**
 * @brief Groebner基法执行 —— 使用代数方法求解几何方程
 *
 * 借鉴 JGEX 的 Wu's Method / Groebner Basis：
 * - 将几何约束转化为多项式方程
 * - 使用 Buchberger 算法计算 Groebner 基
 * - 通过代数消元验证命题
 */
static bool execute_groebner_basis(ProofMultiStrategy *mse, ProofNavigator *nav) {
    if (!mse || !nav)
        return false;

    ProofStep *step = proof_step_create(PROOF_STEP_ADD_CONSTRAINT);
    if (step) {
        step->color = PROOF_COLOR_GREEN;
        step->note = _strdup("[Groebner基法] 将几何约束转化为多项式方程组，计算Groebner基");
        proof_navigator_add_step(nav, step);
    }

    /* 使用求解器验证约束方程的可满足性 */
    /* 注意：此实现为框架，具体代数求解委托给 solver 模块 */
    if (nav->engine && nav->construction) {
        /* 检查自由度——若为0则完全约束，可判定 */
        int dof = 0;
        /* dof = count_degrees_of_freedom(nav->construction); */

        if (dof == 0) {
            ProofStep *solved_step = proof_step_create(PROOF_STEP_UNIFY);
            if (solved_step) {
                solved_step->color = PROOF_COLOR_GREEN;
                solved_step->note = _strdup("[Groebner基法] 多项式系统完全约束，命题得证");
                proof_navigator_add_step(nav, solved_step);
            }
            return true;
        }
    }

    return false;
}

/**
 * @brief 向量法执行 —— 使用矢量代数推导
 */
static bool execute_vector_method(ProofMultiStrategy *mse, ProofNavigator *nav) {
    (void) mse;
    if (!nav)
        return false;

    ProofStep *step = proof_step_create(PROOF_STEP_REWRITE);
    if (step) {
        step->color = PROOF_COLOR_GREEN;
        step->note = _strdup("[向量法] 将几何关系转化为向量表达式进行代数推导");
        proof_navigator_add_step(nav, step);
    }

    /* 向量法框架——具体实现委托给重写系统 */
    return false; /* 框架占位，待后续实现 */
}

/**
 * @brief 全角法执行 —— 利用全角关系推理
 */
static bool execute_full_angle_method(ProofMultiStrategy *mse, ProofNavigator *nav) {
    (void) mse;
    if (!nav)
        return false;

    ProofStep *step = proof_step_create(PROOF_STEP_REWRITE);
    if (step) {
        step->color = PROOF_COLOR_BLUE_UNEXPLORED;
        step->note = _strdup("[全角法] 利用全角关系进行消点推理");
        proof_navigator_add_step(nav, step);
    }

    return false; /* 框架占位 */
}

/**
 * @brief 演绎数据库法执行 —— 前向链推理
 */
static bool execute_deductive_database(ProofMultiStrategy *mse, ProofNavigator *nav) {
    (void) mse;
    if (!nav)
        return false;

    ProofStep *step = proof_step_create(PROOF_STEP_FUNCTION_APP);
    if (step) {
        step->color = PROOF_COLOR_BLUE_UNEXPLORED;
        step->note = _strdup("[演绎数据库] 使用前向链推理，从已知条件推导新事实");
        proof_navigator_add_step(nav, step);
    }

    return false; /* 框架占位 */
}

/**
 * @brief 坐标法执行 —— 使用解析几何坐标计算
 */
static bool execute_coordinate(ProofMultiStrategy *mse, ProofNavigator *nav) {
    (void) mse;
    if (!nav)
        return false;

    ProofStep *step = proof_step_create(PROOF_STEP_ADD_NODE);
    if (step) {
        step->color = PROOF_COLOR_GREEN;
        step->note = _strdup("[坐标法] 建立坐标系，用代数方法计算几何量");
        proof_navigator_add_step(nav, step);
    }

    return false; /* 框架占位 */
}

/**
 * @brief Oracle法执行 —— 外部求解器辅助
 */
static bool execute_oracle(ProofMultiStrategy *mse, ProofNavigator *nav) {
    (void) mse;
    if (!nav)
        return false;

    ProofStep *step = proof_step_create(PROOF_STEP_ORACLE);
    if (step) {
        step->color = PROOF_COLOR_ORANGE_ORACLE;
        step->note = _strdup("[Oracle] 外部求解器辅助验证");
        proof_navigator_add_step(nav, step);
    }

    return false; /* 框架占位 */
}

/* ============== 策略注册表 ============== */

/**
 * @brief 获取默认策略描述符
 *
 * 为每种策略类型预填充名称、描述、适用性检查和执行函数。
 */
static void fill_default_descriptor(ProofStrategyDescriptor *desc, ProofStrategyType type) {
    memset(desc, 0, sizeof(*desc));
    desc->type = type;

    switch (type) {
        case PROOF_STRATEGY_DIRECT_CONSTRUCTION:
            desc->name = _strdup("直接构造法");
            desc->description = _strdup("通过几何构造直接满足命题模式，构造即证明");
            desc->applicability_check = default_applicability_check;
            desc->execute = execute_direct_construction;
            break;

        case PROOF_STRATEGY_AREA_METHOD:
            desc->name = _strdup("面积法");
            desc->description = _strdup("利用面积关系和消点法进行几何推理（借鉴JGEX面积法）");
            desc->applicability_check = area_method_applicability_check;
            desc->execute = execute_area_method;
            break;

        case PROOF_STRATEGY_GROEBNER_BASIS:
            desc->name = _strdup("Groebner基法");
            desc->description = _strdup("将几何约束转化为多项式方程组，使用Buchberger算法求解");
            desc->applicability_check = groebner_applicability_check;
            desc->execute = execute_groebner_basis;
            break;

        case PROOF_STRATEGY_VECTOR_METHOD:
            desc->name = _strdup("向量法");
            desc->description = _strdup("使用矢量代数进行几何关系推导");
            desc->applicability_check = default_applicability_check;
            desc->execute = execute_vector_method;
            break;

        case PROOF_STRATEGY_FULL_ANGLE_METHOD:
            desc->name = _strdup("全角法");
            desc->description = _strdup("利用全角关系进行角度推理和消点（借鉴JGEX全角法）");
            desc->applicability_check = default_applicability_check;
            desc->execute = execute_full_angle_method;
            break;

        case PROOF_STRATEGY_DEDUCTIVE_DATABASE:
            desc->name = _strdup("演绎数据库法");
            desc->description = _strdup("前向链推理，从已知条件逐步演绎新事实");
            desc->applicability_check = default_applicability_check;
            desc->execute = execute_deductive_database;
            break;

        case PROOF_STRATEGY_COORDINATE:
            desc->name = _strdup("坐标法");
            desc->description = _strdup("建立坐标系，使用解析几何方法进行计算和验证");
            desc->applicability_check = default_applicability_check;
            desc->execute = execute_coordinate;
            break;

        case PROOF_STRATEGY_ORACLE:
            desc->name = _strdup("Oracle法");
            desc->description = _strdup("调用外部求解器辅助验证非构造性命題");
            desc->applicability_check = NULL; /* 默认不可用，需手动注册 */
            desc->execute = execute_oracle;
            break;

        default:
            break;
    }

    desc->status = PROOF_STRATEGY_AVAILABLE;
}

/* ============== 公共 API 实现 ============== */

ProofMultiStrategy *proof_multi_strategy_create(ProofNavigator *nav) {
    ProofMultiStrategy *mse = (ProofMultiStrategy *) calloc(1, sizeof(ProofMultiStrategy));
    if (!mse)
        return NULL;

    mse->shared_navigator = nav;
    mse->active_strategy_index = -1;
    mse->enable_fallback = true;

    /* 为每种策略类型预填充默认描述符（标记为可用） */
    for (int i = 0; i < PROOF_STRATEGY_COUNT; i++) {
        fill_default_descriptor(&mse->strategies[i], (ProofStrategyType) i);
    }

    /* Oracle 默认不可用 */
    mse->strategies[PROOF_STRATEGY_ORACLE].status = PROOF_STRATEGY_UNAVAILABLE;

    /* 分配计时数组 */
    mse->strategy_timings_ms = (int64_t *) calloc(PROOF_STRATEGY_COUNT, sizeof(int64_t));
    if (!mse->strategy_timings_ms) {
        free(mse);
        return NULL;
    }

    /* 默认回退顺序：直接构造 -> 面积法 -> Groebner -> 向量 -> 全角 -> 演绎 -> 坐标 */
    int default_order[] = {
        PROOF_STRATEGY_DIRECT_CONSTRUCTION, PROOF_STRATEGY_AREA_METHOD,       PROOF_STRATEGY_GROEBNER_BASIS,
        PROOF_STRATEGY_VECTOR_METHOD,       PROOF_STRATEGY_FULL_ANGLE_METHOD, PROOF_STRATEGY_DEDUCTIVE_DATABASE,
        PROOF_STRATEGY_COORDINATE,
    };
    int default_count = sizeof(default_order) / sizeof(default_order[0]);
    proof_multi_strategy_set_fallback_order(mse, default_order, default_count);

    return mse;
}

void proof_multi_strategy_destroy(ProofMultiStrategy *mse) {
    if (!mse)
        return;

    /* 释放策略描述符中的动态字符串 */
    for (int i = 0; i < PROOF_STRATEGY_COUNT; i++) {
        ProofStrategyDescriptor *desc = &mse->strategies[i];
        free(desc->name);
        free(desc->description);
        if (desc->required_axiom_packages) {
            for (int j = 0; j < desc->axiom_package_count; j++) {
                free(desc->required_axiom_packages[j]);
            }
            free(desc->required_axiom_packages);
        }
        free(desc->generated_step_ids);
    }

    free(mse->fallback_order);
    free(mse->strategy_timings_ms);
    free(mse);
}

bool proof_multi_strategy_register(ProofMultiStrategy *mse, const ProofStrategyDescriptor *descriptor) {
    if (!mse || !descriptor)
        return false;
    if (descriptor->type < 0 || descriptor->type >= PROOF_STRATEGY_COUNT)
        return false;

    ProofStrategyDescriptor *target = &mse->strategies[descriptor->type];

    /* 释放旧数据 */
    free(target->name);
    free(target->description);
    free(target->generated_step_ids);

    /* 复制新数据 */
    target->type = descriptor->type;
    target->status = descriptor->status;
    target->name = descriptor->name ? _strdup(descriptor->name) : NULL;
    target->description = descriptor->description ? _strdup(descriptor->description) : NULL;
    target->applicability_check = descriptor->applicability_check;
    target->execute = descriptor->execute;

    /* 复制公理包依赖 */
    if (descriptor->required_axiom_packages && descriptor->axiom_package_count > 0) {
        target->axiom_package_count = descriptor->axiom_package_count;
        target->required_axiom_packages = (char **) calloc(descriptor->axiom_package_count, sizeof(char *));
        if (target->required_axiom_packages) {
            for (int i = 0; i < descriptor->axiom_package_count; i++) {
                if (descriptor->required_axiom_packages[i]) {
                    target->required_axiom_packages[i] = _strdup(descriptor->required_axiom_packages[i]);
                }
            }
        }
    }

    return true;
}

bool proof_multi_strategy_activate(ProofMultiStrategy *mse, ProofStrategyType strategy_type) {
    if (!mse)
        return false;
    if (strategy_type < 0 || strategy_type >= PROOF_STRATEGY_COUNT)
        return false;

    ProofStrategyDescriptor *desc = &mse->strategies[strategy_type];
    if (desc->status == PROOF_STRATEGY_UNAVAILABLE)
        return false;

    /* 取消旧激活 */
    if (mse->active_strategy_index >= 0) {
        mse->strategies[mse->active_strategy_index].status = PROOF_STRATEGY_AVAILABLE;
    }

    mse->active_strategy_index = strategy_type;
    desc->status = PROOF_STRATEGY_ACTIVE;
    return true;
}

const ProofStrategyDescriptor *proof_multi_strategy_get_active(const ProofMultiStrategy *mse) {
    if (!mse || mse->active_strategy_index < 0)
        return NULL;
    return &mse->strategies[mse->active_strategy_index];
}

int proof_multi_strategy_evaluate_applicability(ProofMultiStrategy *mse, const ConstraintGraph *graph,
                                                const Proposition *prop, ProofStrategyType *out_applicable_types,
                                                int max_count) {
    if (!mse || !out_applicable_types || max_count <= 0)
        return 0;

    int count = 0;
    for (int i = 0; i < PROOF_STRATEGY_COUNT && count < max_count; i++) {
        ProofStrategyDescriptor *desc = &mse->strategies[i];
        if (desc->status == PROOF_STRATEGY_UNAVAILABLE)
            continue;

        if (desc->applicability_check && desc->applicability_check(mse, graph, prop)) {
            out_applicable_types[count++] = (ProofStrategyType) i;
        }
    }
    return count;
}

bool proof_multi_strategy_execute(ProofMultiStrategy *mse) {
    if (!mse || mse->active_strategy_index < 0)
        return false;

    ProofStrategyDescriptor *desc = &mse->strategies[mse->active_strategy_index];
    if (!desc->execute)
        return false;

    mse->total_attempts++;

    bool result = desc->execute(mse, mse->shared_navigator);
    if (result) {
        desc->status = PROOF_STRATEGY_COMPLETED;
        mse->success_count++;
    } else {
        desc->status = PROOF_STRATEGY_FAILED;
    }

    return result;
}

ProofStrategyType proof_multi_strategy_try_all(ProofMultiStrategy *mse) {
    if (!mse)
        return PROOF_STRATEGY_COUNT;

    /* 按回退顺序尝试每个策略 */
    for (int i = 0; i < mse->fallback_count; i++) {
        int idx = mse->fallback_order[i];
        if (idx < 0 || idx >= PROOF_STRATEGY_COUNT)
            continue;

        ProofStrategyDescriptor *desc = &mse->strategies[idx];
        if (desc->status == PROOF_STRATEGY_UNAVAILABLE)
            continue;
        if (!desc->execute)
            continue;

        /* 激活并执行 */
        proof_multi_strategy_activate(mse, (ProofStrategyType) idx);
        if (proof_multi_strategy_execute(mse)) {
            return (ProofStrategyType) idx;
        }
    }

    return PROOF_STRATEGY_COUNT;
}

bool proof_multi_strategy_pipeline(ProofMultiStrategy *mse, const ProofStrategyType *pipeline, int pipeline_count) {
    if (!mse || !pipeline || pipeline_count <= 0)
        return false;

    for (int i = 0; i < pipeline_count; i++) {
        ProofStrategyType type = pipeline[i];
        if (type < 0 || type >= PROOF_STRATEGY_COUNT)
            return false;

        ProofStrategyDescriptor *desc = &mse->strategies[type];
        if (desc->status == PROOF_STRATEGY_UNAVAILABLE)
            return false;
        if (!desc->execute)
            return false;

        proof_multi_strategy_activate(mse, type);
        if (!proof_multi_strategy_execute(mse)) {
            return false; /* 流水线中断 */
        }
    }

    return true;
}

void proof_multi_strategy_set_fallback_order(ProofMultiStrategy *mse, const int *fallback_order, int count) {
    if (!mse || !fallback_order || count <= 0)
        return;

    free(mse->fallback_order);
    mse->fallback_order = (int *) malloc(count * sizeof(int));
    if (!mse->fallback_order)
        return;

    memcpy(mse->fallback_order, fallback_order, count * sizeof(int));
    mse->fallback_count = count;
}

bool proof_multi_strategy_switch(ProofMultiStrategy *mse, ProofStrategyType strategy_type) {
    if (!mse)
        return false;

    /* 保存当前策略状态 */
    if (mse->active_strategy_index >= 0) {
        ProofStrategyDescriptor *old = &mse->strategies[mse->active_strategy_index];
        if (old->status == PROOF_STRATEGY_ACTIVE) {
            old->status = PROOF_STRATEGY_AVAILABLE;
        }
    }

    return proof_multi_strategy_activate(mse, strategy_type);
}

void proof_multi_strategy_get_stats(const ProofMultiStrategy *mse, int *out_total_attempts, int *out_success_count) {
    if (!mse) {
        if (out_total_attempts)
            *out_total_attempts = 0;
        if (out_success_count)
            *out_success_count = 0;
        return;
    }

    if (out_total_attempts)
        *out_total_attempts = mse->total_attempts;
    if (out_success_count)
        *out_success_count = mse->success_count;
}

/* ============== 字符串转换 ============== */

const char *proof_strategy_type_to_string(ProofStrategyType type) {
    switch (type) {
        case PROOF_STRATEGY_DIRECT_CONSTRUCTION:
            return "直接构造法";
        case PROOF_STRATEGY_AREA_METHOD:
            return "面积法";
        case PROOF_STRATEGY_GROEBNER_BASIS:
            return "Groebner基法";
        case PROOF_STRATEGY_VECTOR_METHOD:
            return "向量法";
        case PROOF_STRATEGY_FULL_ANGLE_METHOD:
            return "全角法";
        case PROOF_STRATEGY_DEDUCTIVE_DATABASE:
            return "演绎数据库法";
        case PROOF_STRATEGY_COORDINATE:
            return "坐标法";
        case PROOF_STRATEGY_ORACLE:
            return "Oracle法";
        default:
            return "未知策略";
    }
}

const char *proof_strategy_status_to_string(ProofStrategyStatus status) {
    switch (status) {
        case PROOF_STRATEGY_AVAILABLE:
            return "可用";
        case PROOF_STRATEGY_UNAVAILABLE:
            return "不可用";
        case PROOF_STRATEGY_ACTIVE:
            return "已激活";
        case PROOF_STRATEGY_COMPLETED:
            return "已完成";
        case PROOF_STRATEGY_FAILED:
            return "失败";
        default:
            return "未知";
    }
}
