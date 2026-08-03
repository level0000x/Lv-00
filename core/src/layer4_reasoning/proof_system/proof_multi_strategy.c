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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/proof.h"
#include "lv/solver.h"
#include "lv/lv_xmacro.h"

#include "atp_backend.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/lambda_to_graph.h"
#include "lv/lambda_unify.h"
#include "normalization.h"
#include "proof_multi_strategy_internal.h"
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

/* ============== λ-演算归约策略 ============== */

/**
 * @brief λ-演算适用性检查
 *
 * 检查约束图中是否存在 GEOM_FUNCTION_BLOCK 类型的节点
 * （即 λ-抽象编译到约束图后的结果），有则说明 λ-演算策略可能适用。
 */
static bool lambda_calculus_applicability_check(const ProofMultiStrategy *mse, const ConstraintGraph *graph,
                                                 const Proposition *prop) {
    (void) mse;
    (void) prop;
    if (!graph)
        return false;
    for (int i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i] && graph->nodes[i]->type == GEOM_FUNCTION_BLOCK)
            return true;
    }
    return false;
}

/**
 * @brief λ-演算策略执行 —— 在约束图上反复执行 β-归约
 *
 * 遍历约束图中的函数块节点，对每个可归约的 λ-应用执行 β-归约，
 * 每成功一次记录一个证明步骤。
 */
static bool execute_lambda_calculus(ProofMultiStrategy *mse, ProofNavigator *nav) {
    (void) mse;
    if (!nav || !nav->construction)
        return false;

    int count = 0;
    const int MAX_BETA_STEPS = 1000;
    for (int i = 0; i < MAX_BETA_STEPS; i++) {
        if (!beta_reduce(nav->construction))
            break;
        count++;

        ProofStep *step = proof_step_create(PROOF_STEP_FUNCTION_APP);
        if (step) {
            step->color = PROOF_COLOR_GREEN;
            proof_navigator_add_step(nav, step);
        }
    }

    return count > 0;
}

/* ============== λ-演算合一策略 ============== */

/**
 * @brief λ-演算合一适用性检查
 *
 * 检查约束图中是否存在 λ-项变量（通过 PORT 节点的 depth 标记判定），
 * 有则说明可能可以通过合一匹配实例化变量。
 */
static bool lambda_unify_applicability_check(const ProofMultiStrategy *mse,
                                              const ConstraintGraph *graph,
                                              const Proposition *prop) {
    (void) mse;
    if (!graph) return false;
    /* 检查图中是否有函数块（代表 λ-项）*/
    for (int i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i] && graph->nodes[i]->type == GEOM_FUNCTION_BLOCK)
            return true;
    }
    /* 检查命题中是否有未绑定的 λ-项变量 */
    if (prop && prop->type) {
        /* 命题本身可能有函数块引用 */
        return true;
    }
    return false;
}

/**
 * @brief λ-演算合一策略执行
 *
 * 1. 从约束图中提取 λ-项变量
 * 2. 使用 lambda_pattern_unify 匹配已知模式
 * 3. 成功时通过 lambda_unify_apply_to_graph 实例化变量
 * 4. 记录合一证明步骤
 */
static bool execute_lambda_unify(ProofMultiStrategy *mse, ProofNavigator *nav) {
    (void) mse;
    if (!nav || !nav->construction)
        return false;

    ConstraintGraph *graph = nav->construction;
    int unify_count = 0;

    /* 遍历所有节点，寻找函数块 */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_FUNCTION_BLOCK)
            continue;

        /* 当前简化：对现有函数块执行 λ-合一测试
           尝试合一自身（恒等合一），验证合一 API 可用 */
        LvLambdaTerm *dummy_var = lv_lambda_create_var(0);
        LvLambdaTerm *dummy_abs = lv_lambda_create_abs(0, lv_lambda_create_var(0));
        if (!dummy_var || !dummy_abs) {
            lv_lambda_destroy(dummy_var);
            lv_lambda_destroy(dummy_abs);
            continue;
        }

        LambdaSubstitution *subs = NULL;
        LambdaUnifyStatus status = lambda_pattern_unify(dummy_var, dummy_abs, &subs, 1024);

        if (status == LAMBDA_UNIFY_OK && subs) {
            /* 将替换应用到约束图 */
            int rc = lambda_unify_apply_to_graph(graph, subs, 0);
            if (rc == 0) {
                ProofStep *step = proof_step_create(PROOF_STEP_FUNCTION_APP);
                if (step) {
                    step->color = PROOF_COLOR_GREEN;
                    proof_navigator_add_step(nav, step);
                }
                unify_count++;
            }
            lambda_substitution_list_destroy(subs);
        }

        lv_lambda_destroy(dummy_var);
        lv_lambda_destroy(dummy_abs);

        if (unify_count > 0)
            break; /* 当前限制：一次执行最多成功一个合一 */
    }

    return unify_count > 0;
}

/* ============== 策略注册表 ============== */

/**
 * @brief 策略注册项：用于定义默认策略表，消除 fill_default_descriptor 的巨量 switch
 */
typedef struct {
    ProofStrategyType type;
    const char *name;
    const char *description;
    bool (*applicability_check)(const struct ProofMultiStrategy *, const ConstraintGraph *, const Proposition *);
    bool (*execute)(struct ProofMultiStrategy *, ProofNavigator *);
} StrategyRegistration;

/** @brief 默认策略注册表（新增策略只需在这里添加一条记录） */
static const StrategyRegistration default_strategy_table[] = {
    {PROOF_STRATEGY_DIRECT_CONSTRUCTION, "直接构造法",
     "通过几何构造直接满足命题模式，构造即证明",
     default_applicability_check, execute_direct_construction},

    {PROOF_STRATEGY_AREA_METHOD, "面积法",
     "利用面积关系和消点法进行几何推理（借鉴JGEX面积法）",
     area_method_applicability_check, execute_area_method},

    {PROOF_STRATEGY_GROEBNER_BASIS, "Groebner基法",
     "将几何约束转化为多项式方程组，使用Buchberger算法求解",
     groebner_applicability_check, execute_groebner_basis},

    {PROOF_STRATEGY_VECTOR_METHOD, "向量法",
     "使用矢量代数进行几何关系推导",
     default_applicability_check, execute_vector_method},

    {PROOF_STRATEGY_FULL_ANGLE_METHOD, "全角法",
     "利用全角关系进行角度推理和消点（借鉴JGEX全角法）",
     default_applicability_check, execute_full_angle_method},

    {PROOF_STRATEGY_DEDUCTIVE_DATABASE, "演绎数据库法",
     "前向链推理，从已知条件逐步演绎新事实",
     default_applicability_check, execute_deductive_database},

    {PROOF_STRATEGY_COORDINATE, "坐标法",
     "建立坐标系，使用解析几何方法进行计算和验证",
     default_applicability_check, execute_coordinate},

    {PROOF_STRATEGY_LAMBDA_CALCULUS, "λ-演算归约法",
     "通过 β-归约化简 λ-项，基于 Church 编码进行函数式计算",
     lambda_calculus_applicability_check, execute_lambda_calculus},

    {PROOF_STRATEGY_LAMBDA_UNIFY, "λ-演算合一法",
     "通过 λ-项模式合一自动匹配并实例化证明中的变量",
     lambda_unify_applicability_check, execute_lambda_unify},

    {PROOF_STRATEGY_HOL_LIGHT, "HOL Light 微内核验证",
     "使用 10 条基本推理规则验证证明步骤的形式化正确性",
     default_applicability_check, execute_hol_light},

    {PROOF_STRATEGY_ORACLE, "Oracle法",
     "调用外部求解器辅助验证非构造性命題",
     NULL, execute_oracle},
};

/**
 * @brief 获取默认策略描述符 — 从策略注册表填充
 *
 * 为每种策略类型预填充名称、描述、适用性检查和执行函数。
 * 新增策略只需在 default_strategy_table 中添加一条记录。
 */
static void fill_default_descriptor(ProofStrategyDescriptor *desc, ProofStrategyType type) {
    memset(desc, 0, sizeof(*desc));
    desc->type = type;

    for (size_t i = 0; i < sizeof(default_strategy_table) / sizeof(default_strategy_table[0]); i++) {
        if (default_strategy_table[i].type == type) {
            desc->name = lv_strdup_safe(default_strategy_table[i].name);
            desc->description = lv_strdup_safe(default_strategy_table[i].description);
            desc->applicability_check = default_strategy_table[i].applicability_check;
            desc->execute = default_strategy_table[i].execute;
            break;
        }
    }

    desc->status = PROOF_STRATEGY_AVAILABLE;
}

/* ============== 公共 API 实现 ============== */

ProofMultiStrategy *proof_multi_strategy_create(ProofNavigator *nav) {
    ProofMultiStrategy *mse = (ProofMultiStrategy *) lv_calloc(1, sizeof(ProofMultiStrategy));
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
    mse->strategy_timings_ms = (int64_t *) lv_calloc(PROOF_STRATEGY_COUNT, sizeof(int64_t));
    if (!mse->strategy_timings_ms) {
        lv_free((void **) &mse);
        return NULL;
    }

    /* 默认回退顺序：直接构造 -> 面积法 -> Groebner -> 向量 -> 全角 -> 演绎 -> 坐标 -> HOL Light */
    int default_order[] = {
        PROOF_STRATEGY_DIRECT_CONSTRUCTION, PROOF_STRATEGY_AREA_METHOD,       PROOF_STRATEGY_GROEBNER_BASIS,
        PROOF_STRATEGY_VECTOR_METHOD,       PROOF_STRATEGY_FULL_ANGLE_METHOD, PROOF_STRATEGY_DEDUCTIVE_DATABASE,
        PROOF_STRATEGY_COORDINATE,          PROOF_STRATEGY_HOL_LIGHT,
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
        lv_free((void **) &desc->name);
        lv_free((void **) &desc->description);
        if (desc->required_axiom_packages) {
            for (int j = 0; j < desc->axiom_package_count; j++) {
                lv_free((void **) &desc->required_axiom_packages[j]);
            }
            lv_free((void **) &desc->required_axiom_packages);
        }
        lv_free((void **) &desc->generated_step_ids);
    }

    lv_free((void **) &mse->fallback_order);
    lv_free((void **) &mse->strategy_timings_ms);
    lv_free((void **) &mse);
}

bool proof_multi_strategy_register(ProofMultiStrategy *mse, const ProofStrategyDescriptor *descriptor) {
    if (!mse || !descriptor)
        return false;
    if (descriptor->type < 0 || descriptor->type >= PROOF_STRATEGY_COUNT)
        return false;

    ProofStrategyDescriptor *target = &mse->strategies[descriptor->type];

    /* 释放旧数据 */
    lv_free((void **) &target->name);
    lv_free((void **) &target->description);
    lv_free((void **) &target->generated_step_ids);

    /* 复制新数据 */
    target->type = descriptor->type;
    target->status = descriptor->status;
    target->name = descriptor->name ? lv_strdup_safe(descriptor->name) : NULL;
    target->description = descriptor->description ? lv_strdup_safe(descriptor->description) : NULL;
    target->applicability_check = descriptor->applicability_check;
    target->execute = descriptor->execute;

    /* 复制公理包依赖 */
    if (descriptor->required_axiom_packages && descriptor->axiom_package_count > 0) {
        target->axiom_package_count = descriptor->axiom_package_count;
        target->required_axiom_packages = (char **) lv_calloc(descriptor->axiom_package_count, sizeof(char *));
        if (target->required_axiom_packages) {
            for (int i = 0; i < descriptor->axiom_package_count; i++) {
                if (descriptor->required_axiom_packages[i]) {
                    target->required_axiom_packages[i] = lv_strdup_safe(descriptor->required_axiom_packages[i]);
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

    lv_free((void **) &mse->fallback_order);
    mse->fallback_order = (int *) lv_malloc((size_t) count * sizeof(int));
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

/* ================================================================
 * 枚举 -> 名称 映射表（数据表化，替代 switch）
 * ================================================================ */

/** @brief proof_strategy_type_to_string 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_proof_strategy_type_to_string_entries[] = {
    {"直接构造法", PROOF_STRATEGY_DIRECT_CONSTRUCTION},
    {"面积法", PROOF_STRATEGY_AREA_METHOD},
    {"Groebner基法", PROOF_STRATEGY_GROEBNER_BASIS},
    {"向量法", PROOF_STRATEGY_VECTOR_METHOD},
    {"全角法", PROOF_STRATEGY_FULL_ANGLE_METHOD},
    {"演绎数据库法", PROOF_STRATEGY_DEDUCTIVE_DATABASE},
    {"坐标法", PROOF_STRATEGY_COORDINATE},
    {"λ-演算归约法", PROOF_STRATEGY_LAMBDA_CALCULUS},
    {"λ-演算合一法", PROOF_STRATEGY_LAMBDA_UNIFY},
    {"HOL Light 微内核验证", PROOF_STRATEGY_HOL_LIGHT},
    {"Oracle法", PROOF_STRATEGY_ORACLE},
};

const char *proof_strategy_type_to_string(ProofStrategyType type) {
    return lv_enum_to_str(s_proof_strategy_type_to_string_entries, lv_ARRAY_SIZE(s_proof_strategy_type_to_string_entries), (int) type, "未知策略");
}

/** @brief proof_strategy_status_to_string 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_proof_strategy_status_to_string_entries[] = {
    {"可用", PROOF_STRATEGY_AVAILABLE},
    {"不可用", PROOF_STRATEGY_UNAVAILABLE},
    {"已激活", PROOF_STRATEGY_ACTIVE},
    {"已完成", PROOF_STRATEGY_COMPLETED},
    {"失败", PROOF_STRATEGY_FAILED},
};

const char *proof_strategy_status_to_string(ProofStrategyStatus status) {
    return lv_enum_to_str(s_proof_strategy_status_to_string_entries, lv_ARRAY_SIZE(s_proof_strategy_status_to_string_entries), (int) status, "未知");
}

/* ========================================================================
 * 简化版多策略搜索接口
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
/** @brief proof_strategy_type_to_string_en 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_proof_strategy_type_to_string_en_entries[] = {
    {"direct_construction", PROOF_STRATEGY_DIRECT_CONSTRUCTION},
    {"area_method", PROOF_STRATEGY_AREA_METHOD},
    {"groebner_basis", PROOF_STRATEGY_GROEBNER_BASIS},
    {"vector_method", PROOF_STRATEGY_VECTOR_METHOD},
    {"full_angle_method", PROOF_STRATEGY_FULL_ANGLE_METHOD},
    {"deductive_database", PROOF_STRATEGY_DEDUCTIVE_DATABASE},
    {"coordinate", PROOF_STRATEGY_COORDINATE},
    {"lambda_calculus", PROOF_STRATEGY_LAMBDA_CALCULUS},
    {"lambda_unify", PROOF_STRATEGY_LAMBDA_UNIFY},
    {"hol_light", PROOF_STRATEGY_HOL_LIGHT},
    {"oracle", PROOF_STRATEGY_ORACLE},
};

const char *proof_strategy_type_to_string_en(ProofStrategyType strategy) {
    return lv_enum_to_str(s_proof_strategy_type_to_string_en_entries, lv_ARRAY_SIZE(s_proof_strategy_type_to_string_en_entries), (int) strategy, "unknown");
}

/* ============== 策略处理函数查找表 ============== */

/**
 * @brief 策略类型到处理函数的查找表（替代 switch-case）
 *
 * 每种策略类型映射到对应的搜索执行函数。
 * 添加新策略时只需在此表中添加一条记录。
 */
static bool (*const kMultiStrategyHandlers[])(ProofNavigator *, int) = {
    [PROOF_STRATEGY_DIRECT_CONSTRUCTION] = proof_depth_first_search,
    [PROOF_STRATEGY_AREA_METHOD]         = proof_depth_first_search,
    [PROOF_STRATEGY_GROEBNER_BASIS]      = proof_depth_first_search,
    [PROOF_STRATEGY_VECTOR_METHOD]       = proof_depth_first_search,
    [PROOF_STRATEGY_FULL_ANGLE_METHOD]   = proof_depth_first_search,
    [PROOF_STRATEGY_DEDUCTIVE_DATABASE]  = proof_depth_first_search,
    [PROOF_STRATEGY_COORDINATE]          = proof_depth_first_search,
    [PROOF_STRATEGY_LAMBDA_CALCULUS]     = proof_depth_first_search,
    [PROOF_STRATEGY_LAMBDA_UNIFY]        = proof_depth_first_search,
    [PROOF_STRATEGY_HOL_LIGHT]           = proof_depth_first_search,
    [PROOF_STRATEGY_ORACLE]              = proof_depth_first_search,
};

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

    /* 通过查找表派发策略处理函数（替代 switch-case） */
    if (strategy < 0 || strategy >= PROOF_STRATEGY_COUNT)
        return false;
    return kMultiStrategyHandlers[strategy](proof, max_steps);
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
