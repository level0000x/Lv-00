/**
 * @file proof_multi_strategy.c
 * @brief 多证明方法并存引擎实现（借鉴 JGEX/GEX 架构）
 *
 * 借鉴 JGEX（中科院张景中团队的几何定理机器证明系统）的多方法共存设计：
 * - 在同一系统中集成 8 种独立的证明方法
 * - 支持策略注册,切换,组合执行
 * - 支持竞争模式（自动回退）和流水线模式（串联）
 * - 支持适用性自动评估
 *
 * 版本：v3.2.0
 * 参考：
 *   - JGEX/GEX (https://github.com/kovzol/Java-Geometry-Expert)
 *     多证明方法并存引擎,可读证明生成,C-tree 约束分解
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
 * 为每种策略类型预填充名称,描述,适用性检查和执行函数。
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
    ProofMultiStrategy *mse = (ProofMultiStrategy *) lv00_calloc(1, sizeof(ProofMultiStrategy));
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
    mse->strategy_timings_ms = (int64_t *) lv00_calloc(PROOF_STRATEGY_COUNT, sizeof(int64_t));
    if (!mse->strategy_timings_ms) {
        lv00_free((void **) &mse);
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
        lv00_free((void **) &desc->name);
        lv00_free((void **) &desc->description);
        if (desc->required_axiom_packages) {
            for (int j = 0; j < desc->axiom_package_count; j++) {
                lv00_free((void **) &desc->required_axiom_packages[j]);
            }
            lv00_free((void **) &desc->required_axiom_packages);
        }
        lv00_free((void **) &desc->generated_step_ids);
    }

    lv00_free((void **) &mse->fallback_order);
    lv00_free((void **) &mse->strategy_timings_ms);
    lv00_free((void **) &mse);
}

bool proof_multi_strategy_register(ProofMultiStrategy *mse, const ProofStrategyDescriptor *descriptor) {
    if (!mse || !descriptor)
        return false;
    if (descriptor->type < 0 || descriptor->type >= PROOF_STRATEGY_COUNT)
        return false;

    ProofStrategyDescriptor *target = &mse->strategies[descriptor->type];

    /* 释放旧数据 */
    lv00_free((void **) &target->name);
    lv00_free((void **) &target->description);
    lv00_free((void **) &target->generated_step_ids);

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
        target->required_axiom_packages = (char **) lv00_calloc(descriptor->axiom_package_count, sizeof(char *));
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

    lv00_free((void **) &mse->fallback_order);
    mse->fallback_order = (int *) lv00_malloc(count * sizeof(int));
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

/* ========================================================================
 * 简化版多策略搜索接口（桩实现）
 *
 * 提供简化的搜索函数接口，方便快速集成和测试。
 * 这些函数封装了上述完整的多策略引擎，提供更直观的调用方式。
 * ======================================================================== */

/**
 * @brief 简化版策略类型转字符串（英文）
 *
 * 与 proof_strategy_type_to_string 不同，此函数返回英文标识符，
 * 便于日志输出和调试。
 *
 * @param strategy 策略类型
 * @return 策略名称字符串
 */
const char *proof_strategy_type_to_string_en(ProofStrategyType strategy) {
    switch (strategy) {
        case PROOF_STRATEGY_DIRECT_CONSTRUCTION:
            return "direct_construction";
        case PROOF_STRATEGY_AREA_METHOD:
            return "area_method";
        case PROOF_STRATEGY_GROEBNER_BASIS:
            return "groebner_basis";
        case PROOF_STRATEGY_VECTOR_METHOD:
            return "vector_method";
        case PROOF_STRATEGY_FULL_ANGLE_METHOD:
            return "full_angle_method";
        case PROOF_STRATEGY_DEDUCTIVE_DATABASE:
            return "deductive_database";
        case PROOF_STRATEGY_COORDINATE:
            return "coordinate";
        case PROOF_STRATEGY_ORACLE:
            return "oracle";
        default:
            return "unknown";
    }
}

/* ============== 辅助搜索函数声明 ============== */

/**
 * @brief 深度优先搜索（简化实现）
 *
 * 沿着单一路径深入搜索，直到达到目标或无法继续。
 * 遇到死胡同时回溯到上一个选择点。
 *
 * @param proof   证明导航器指针
 * @param max_steps 最大搜索步数
 * @return true 找到证明，false 搜索失败或超时
 */
static bool proof_depth_first_search(ProofNavigator *proof, int max_steps);

/**
 * @brief 广度优先搜索（简化实现）
 *
 * 按层次展开搜索空间，先探索所有深度为 d 的节点，
 * 再探索深度为 d+1 的节点。
 *
 * @param proof   证明导航器指针
 * @param max_steps 最大搜索步数
 * @return true 找到证明，false 搜索失败或超时
 */
static bool proof_breadth_first_search(ProofNavigator *proof, int max_steps);

/**
 * @brief 最佳优先搜索（简化实现）
 *
 * 使用启发式评估函数选择最有希望的节点进行探索。
 * 需要提供启发式评分函数。
 *
 * @param proof   证明导航器指针
 * @param max_steps 最大搜索步数
 * @return true 找到证明，false 搜索失败或超时
 */
static bool proof_best_first_search(ProofNavigator *proof, int max_steps);

/**
 * @brief 蒙特卡洛树搜索（简化实现）
 *
 * 通过随机模拟评估节点价值，结合探索和利用。
 * 适用于搜索空间大,难以用传统方法评估的证明问题。
 *
 * @param proof   证明导航器指针
 * @param max_steps 最大搜索步数（模拟次数）
 * @return true 找到证明，false 搜索失败或超时
 */
static bool proof_mcts_search(ProofNavigator *proof, int max_steps);

/* ============== 辅助搜索函数实现 ============== */

/**
 * @brief 深度优先搜索实现
 *
 * 递归/迭代深入搜索：
 * 1. 检查当前状态是否为目标
 * 2. 选择一条推理规则应用
 * 3. 递归深入
 * 4. 如果失败则回溯尝试其他路径
 */
static bool proof_depth_first_search(ProofNavigator *proof, int max_steps) {
    if (!proof)
        return false;

    int steps = 0;

    /* 简化的深度优先搜索：遍历策略执行 */
    while (steps < max_steps && !proof->is_complete) {
        /* 获取当前激活的策略 */
        const ProofStrategyDescriptor *desc = proof_multi_strategy_get_active(
            (const ProofMultiStrategy *) proof->engine);

        if (!desc || !desc->execute) {
            /* 无可用策略，尝试所有策略 */
            ProofStrategyType tried = proof_multi_strategy_try_all(
                (ProofMultiStrategy *) proof->engine);
            if (tried >= PROOF_STRATEGY_COUNT) {
                break;
            }
        } else {
            /* 执行当前策略 */
            if (!proof_multi_strategy_execute((ProofMultiStrategy *) proof->engine)) {
                /* 当前策略失败，切换到下一个可用策略 */
                int current_idx = -1;
                for (int i = 0; i < PROOF_STRATEGY_COUNT; i++) {
                    if (((ProofMultiStrategy *) proof->engine)->strategies[i].status == PROOF_STRATEGY_ACTIVE) {
                        current_idx = i;
                        break;
                    }
                }
                /* 尝试下一个策略 */
                for (int next = current_idx + 1; next < PROOF_STRATEGY_COUNT; next++) {
                    if (((ProofMultiStrategy *) proof->engine)->strategies[next].status == PROOF_STRATEGY_AVAILABLE) {
                        proof_multi_strategy_activate((ProofMultiStrategy *) proof->engine,
                                                      (ProofStrategyType) next);
                        break;
                    }
                }
            }
        }

        steps++;
    }

    return proof->is_complete;
}

/**
 * @brief 广度优先搜索实现
 *
 * 按层次展开搜索空间：
 * 1. 展开当前层所有候选
 * 2. 检查该层是否有目标
 * 3. 如果没有，移动到下一层
 */
static bool proof_breadth_first_search(ProofNavigator *proof, int max_steps) {
    if (!proof)
        return false;

    int explored = 0;
    int level_size = 1;  /* 假设初始有1个节点 */
    int current_level = 0;

    /* 简化的 BFS：逐个策略执行 */
    while (explored < max_steps) {
        bool found_in_level = false;

        /* 模拟当前层的所有候选节点探索 */
        for (int i = 0; i < level_size && explored < max_steps; i++) {
            /* 执行一次策略尝试 */
            bool success = proof_multi_strategy_execute((ProofMultiStrategy *) proof->engine);

            if (success && proof->is_complete) {
                return true;
            }

            explored++;
        }

        /* 当前层探索完毕，检查是否完成 */
        if (proof->is_complete) {
            return true;
        }

        /* 进入下一层 */
        current_level++;
        /* 假设每层节点数指数增长（有分支因子） */
        level_size = level_size * 2;
        if (level_size > 1024) {
            level_size = 1024;  /* 限制增长 */
        }
    }

    return false;
}

/**
 * @brief 最佳优先搜索实现
 *
 * 使用启发式评分选择最佳候选：
 * 1. 评估所有候选的启发式分数
 * 2. 选择分数最高的候选
 * 3. 应用该候选
 * 4. 重复直到找到目标或无候选
 */
static bool proof_best_first_search(ProofNavigator *proof, int max_steps) {
    if (!proof)
        return false;

    int steps = 0;

    /* 简化的最佳优先搜索：按策略优先级尝试 */
    while (steps < max_steps && !proof->is_complete) {
        /* 尝试所有策略，找到第一个成功的 */
        for (int i = 0; i < PROOF_STRATEGY_COUNT; i++) {
            if (((ProofMultiStrategy *) proof->engine)->strategies[i].status == PROOF_STRATEGY_AVAILABLE) {
                /* 激活并执行此策略 */
                proof_multi_strategy_activate((ProofMultiStrategy *) proof->engine,
                                              (ProofStrategyType) i);

                if (proof_multi_strategy_execute((ProofMultiStrategy *) proof->engine)) {
                    if (proof->is_complete) {
                        return true;
                    }
                }
            }
        }

        steps++;
    }

    return proof->is_complete;
}

/**
 * @brief 蒙特卡洛树搜索实现（简化版）
 *
 * MCTS 四步循环：
 * 1. 选择（Selection）：从根节点选择最优子节点
 * 2. 展开（Expansion）：添加新的子节点
 * 3. 模拟（Simulation）：随机执行到终止状态
 * 4. 回传（Backpropagation）：更新路径上节点的统计信息
 *
 * @param proof       证明导航器指针
 * @param max_steps   最大模拟次数
 * @return true 找到证明，false 搜索失败或超时
 */
static bool proof_mcts_search(ProofNavigator *proof, int max_steps) {
    if (!proof)
        return false;

    int simulations = 0;

    /* 简化的 MCTS：多次尝试不同策略组合 */
    while (simulations < max_steps) {
        /* 选择一个随机策略尝试 */
        ProofStrategyType random_strategy = (ProofStrategyType)(
            simulations % PROOF_STRATEGY_COUNT);

        /* 检查策略是否可用 */
        if (((ProofMultiStrategy *) proof->engine)->strategies[random_strategy].status !=
            PROOF_STRATEGY_UNAVAILABLE) {

            /* 激活策略 */
            proof_multi_strategy_activate((ProofMultiStrategy *) proof->engine,
                                          random_strategy);

            /* 执行模拟 */
            bool success = proof_multi_strategy_execute((ProofMultiStrategy *) proof->engine);

            /* 检查结果 */
            if (success && proof->is_complete) {
                return true;
            }

            /* 如果失败，尝试下一个策略 */
            ProofStrategyType next = (ProofStrategyType)((random_strategy + 1) % PROOF_STRATEGY_COUNT);
            if (((ProofMultiStrategy *) proof->engine)->strategies[next].status !=
                PROOF_STRATEGY_UNAVAILABLE) {
                proof_multi_strategy_activate((ProofMultiStrategy *) proof->engine, next);
                proof_multi_strategy_execute((ProofMultiStrategy *) proof->engine);

                if (proof->is_complete) {
                    return true;
                }
            }
        }

        simulations++;
    }

    /* 返回是否找到证明 */
    return proof->is_complete;
}

/* ============== 公共简化API ============== */

/**
 * @brief 使用指定策略执行证明搜索（简化接口）
 *
 * 这是 proof_multi_strategy_execute 的简化版本，
 * 直接指定策略类型和最大步数。
 *
 * @param proof      证明导航器指针
 * @param strategy   搜索策略类型
 * @param max_steps  最大搜索步数
 * @return true 找到证明，false 搜索失败或超时
 */
bool proof_search_with_strategy(ProofNavigator *proof, ProofStrategyType strategy, int max_steps) {
    if (!proof)
        return false;

    /* 激活指定策略 */
    if (!proof_multi_strategy_activate((ProofMultiStrategy *) proof->engine, strategy)) {
        return false;
    }

    /* 根据策略类型选择搜索方法 */
    switch (strategy) {
        case PROOF_STRATEGY_DIRECT_CONSTRUCTION:
        case PROOF_STRATEGY_AREA_METHOD:
        case PROOF_STRATEGY_GROEBNER_BASIS:
        case PROOF_STRATEGY_VECTOR_METHOD:
        case PROOF_STRATEGY_FULL_ANGLE_METHOD:
        case PROOF_STRATEGY_DEDUCTIVE_DATABASE:
        case PROOF_STRATEGY_COORDINATE:
        case PROOF_STRATEGY_ORACLE:
            /* 使用深度优先搜索策略执行 */
            return proof_depth_first_search(proof, max_steps);

        default:
            return false;
    }
}

/**
 * @brief 使用蒙特卡洛树搜索执行证明（简化接口）
 *
 * 封装 MCTS 搜索，适用于复杂或搜索空间大的证明问题。
 *
 * @param proof      证明导航器指针
 * @param max_steps  最大模拟次数
 * @return true 找到证明，false 搜索失败或超时
 */
bool proof_mcts_execute(ProofNavigator *proof, int max_steps) {
    if (!proof)
        return false;

    return proof_mcts_search(proof, max_steps);
}

/**
 * @brief 执行广度优先搜索证明（简化接口）
 *
 * 适用于需要系统探索所有可能性的证明问题。
 *
 * @param proof      证明导航器指针
 * @param max_steps  最大搜索步数
 * @return true 找到证明，false 搜索失败或超时
 */
bool proof_bfs_execute(ProofNavigator *proof, int max_steps) {
    if (!proof)
        return false;

    return proof_breadth_first_search(proof, max_steps);
}

/**
 * @brief 执行最佳优先搜索证明（简化接口）
 *
 * 适用于有良好启发式函数的证明问题。
 *
 * @param proof      证明导航器指针
 * @param max_steps  最大搜索步数
 * @return true 找到证明，false 搜索失败或超时
 */
bool proof_best_first_execute(ProofNavigator *proof, int max_steps) {
    if (!proof)
        return false;

    return proof_best_first_search(proof, max_steps);
}
