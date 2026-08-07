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
#include "lv/engine.h"
#include "lv/proof.h"
#include "lv/solver.h"
#include "lv/lv_xmacro.h"

#include "atp_backend.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/lambda_to_graph.h"
#include "lv/lambda_unify.h"
#include "normalization.h"
#include "proof_engine_enhanced.h" /* 经典策略引擎（系统A）：桥接策略依赖其公共入口 */
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
 * @brief 顶层自由 λ-变量槽位判定
 *
 * 与 lambda_unify_apply_to_graph 的槽位语义一致：PORT_OUTPUT、
 * parent_block_id == -1、非形式参数、非函数块内部节点。
 */
static bool graph_node_is_lambda_slot(const ConstraintGraph *graph, const GeomNode *node) {
    if (!node || !node->is_active || node->type != GEOM_PORT)
        return false;
    Port *port = node->data.port;
    if (!port || port->type != PORT_OUTPUT || port->is_formal_param)
        return false;
    if (node->parent_block_id != -1)
        return false;
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *fb = graph->nodes[i];
        if (!fb || !fb->is_active || fb->type != GEOM_FUNCTION_BLOCK)
            continue;
        if (!fb->data.func_block.internal_nodes)
            continue;
        for (int j = 0; j < fb->data.func_block.internal_node_count; j++) {
            if (fb->data.func_block.internal_nodes[j] &&
                fb->data.func_block.internal_nodes[j]->id == node->id)
                return false;
        }
    }
    return true;
}

/** @brief 统计 λ-项的前导抽象层数 */
static int lambda_leading_abs_count(const LvLambdaTerm *term) {
    int n = 0;
    while (term && term->type == LV_LAMBDA_ABS) {
        n++;
        term = term->data.abs.body;
    }
    return n;
}

/**
 * @brief 构造目标模式 λx1..λxn. F x1..xn
 *
 * F 为元变量（De Bruijn 索引 meta_index，须 >= binder_count 以保持自由），
 * n 个 binder 由外到内对应 De Bruijn 索引 n-1, n-2, ..., 0。
 */
static LvLambdaTerm *lambda_build_target_pattern(int meta_index, int binder_count) {
    LvLambdaTerm *result = lv_lambda_create_var(meta_index);
    if (!result)
        return NULL;
    for (int k = 0; k < binder_count; k++) {
        result = lv_lambda_create_app(result, lv_lambda_create_var(binder_count - 1 - k));
        if (!result)
            return NULL; /* create_app 失败时已销毁子项 */
    }
    for (int i = 0; i < binder_count; i++) {
        result = lv_lambda_create_abs(0, result);
        if (!result)
            return NULL; /* create_abs 失败时已销毁 body */
    }
    return result;
}

/**
 * @brief λ-演算合一策略执行
 *
 * 1. 从约束图收集 FUNCTION_BLOCK 节点（λ-抽象）与顶层自由 λ-变量槽位
 * 2. 将函数块还原为 λ-项，与目标模式 λx1..λxn.F x1..xn（F 取首个
 *    槽位索引）做 Miller 模式合一，求解槽位应实例化的 λ-项
 * 3. 通过 lambda_unify_apply_to_graph 把替换真实应用到约束图
 * 4. 成功时记录合一证明步骤（PROOF_STEP_UNIFY，绿色）
 *
 * 当图中无函数块、无槽位、还原失败或合一不可应用时，策略诚实返回
 * false（不产生任何图变化）。
 */
static bool execute_lambda_unify(ProofMultiStrategy *mse, ProofNavigator *nav) {
    (void) mse;
    if (!nav || !nav->construction)
        return false;

    ConstraintGraph *graph = nav->construction;

    /* 收集函数块与顶层自由 λ-变量槽位 */
    int fb_ids[64];
    int fb_count = 0;
    int slot_indices[64];
    int slot_count = 0;
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || !node->is_active)
            continue;
        if (node->type == GEOM_FUNCTION_BLOCK && fb_count < 64) {
            fb_ids[fb_count++] = node->id;
        } else if (graph_node_is_lambda_slot(graph, node) && slot_count < 64) {
            slot_indices[slot_count++] = node->namespace_depth;
        }
    }
    if (fb_count == 0 || slot_count == 0)
        return false;

    /* 对每个函数块尝试合一：目标模式绑定层数取函数块 λ-项的前导抽象数 */
    for (int f = 0; f < fb_count; f++) {
        int fb_id = fb_ids[f];
        GeomNode *fb_node = graph_get_node(graph, fb_id);
        if (!fb_node || fb_node->type != GEOM_FUNCTION_BLOCK)
            continue;

        LvLambdaTerm *term = graph_to_lambda(graph, fb_id);
        if (!term)
            continue;
        int binder_count = lambda_leading_abs_count(term);
        if (binder_count <= 0) {
            lv_lambda_destroy(term);
            continue;
        }

        /* 元变量 F 取第一个 index >= binder_count 的槽位索引（F 在目标模式中须自由） */
        int meta_index = -1;
        for (int s = 0; s < slot_count; s++) {
            if (slot_indices[s] >= binder_count) {
                meta_index = slot_indices[s];
                break;
            }
        }
        if (meta_index < 0) {
            lv_lambda_destroy(term);
            continue;
        }

        LvLambdaTerm *target = lambda_build_target_pattern(meta_index, binder_count);
        if (!target) {
            lv_lambda_destroy(term);
            continue;
        }

        LambdaSubstitution *subs = NULL;
        LambdaUnifyStatus status = lambda_pattern_unify(target, term, &subs, 1024);
        lv_lambda_destroy(target);

        bool applied = false;
        if (status == LAMBDA_UNIFY_OK && subs) {
            int rc = lambda_unify_apply_to_graph(graph, subs, 0);
            applied = (rc == 0);
        }
        lambda_substitution_list_destroy(subs);
        lv_lambda_destroy(term);

        if (applied) {
            ProofStep *step = proof_step_create(PROOF_STEP_UNIFY);
            if (step) {
                step->color = PROOF_COLOR_GREEN;
                step->func_block_id = fb_id;
                step->note = lv_strdup_safe("λ-演算合一：匹配函数块端口签名，实例化顶层 λ-变量");
                proof_navigator_add_step(nav, step);
            }
            return true; /* 一次执行最多成功一个合一 */
        }
    }

    return false;
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
    ProofSearchAlgorithm search_algorithm; /**< 该策略使用的证明搜索算法（默认 DFS） */
    const char **required_axiom_packages;  /**< 依赖的公理包名称列表（静态字符串，填充时逐项深拷贝） */
    int axiom_package_count;               /**< 依赖的公理包数量（0 = 无依赖） */
} StrategyRegistration;

/* ── 经典策略体系（系统A：proof_strategy.c / lvProofEngine）桥接 ──
 * 两套平行策略体系并存：
 *   A: lvStrategyType（10 策略）+ kStrategyDispatch + lv_proof_engine_prove_with_strategy
 *   B: ProofStrategyType（12 策略）+ default_strategy_table + proof_multi_strategy_execute
 * 桥接策略把 B 的执行上下文（ProofNavigator）转交给 A 的公共入口，
 * 使 A 的 10 个策略对 B 可见，而无需改动 A 的任何策略实现。 */

/**
 * @brief 桥接枚举 → 经典引擎策略类型映射
 */
static lvStrategyType legacy_bridge_to_lv_strategy(ProofStrategyType t) {
    switch (t) {
    case PROOF_STRATEGY_LEGACY_DIRECT:         return STRATEGY_DIRECT;
    case PROOF_STRATEGY_LEGACY_CONTRADICTION:  return STRATEGY_CONTRADICTION;
    case PROOF_STRATEGY_LEGACY_CONTRAPOSITIVE: return STRATEGY_CONTRAPOSITIVE;
    case PROOF_STRATEGY_LEGACY_INDUCTION:      return STRATEGY_INDUCTION;
    case PROOF_STRATEGY_LEGACY_CASES:          return STRATEGY_CASES;
    case PROOF_STRATEGY_LEGACY_CONSTRUCTION:   return STRATEGY_CONSTRUCTION;
    case PROOF_STRATEGY_LEGACY_UNFOLDING:      return STRATEGY_UNFOLDING;
    case PROOF_STRATEGY_LEGACY_BACKWARD:       return STRATEGY_BACKWARD;
    case PROOF_STRATEGY_LEGACY_FORWARD:        return STRATEGY_FORWARD;
    case PROOF_STRATEGY_LEGACY_HYBRID:         return STRATEGY_HYBRID;
    default:                                   return STRATEGY_DIRECT; /* 不可达 */
    }
}

/**
 * @brief 桥接策略适用性检查：经典引擎未挂载时不可用
 */
static bool legacy_bridge_applicability_check(const ProofMultiStrategy *mse, const ConstraintGraph *graph,
                                              const Proposition *prop) {
    if (!mse || !mse->legacy_proof_engine)
        return false; /* 未挂载经典引擎：桥接策略不可用 */
    return (graph != NULL) && (prop != NULL);
}

/**
 * @brief 桥接策略执行：将目标转交给经典引擎的公共入口
 *
 * 以 nav->target_prop 为目标、nav->construction 为约束图，
 * 调用 lv_proof_engine_prove_with_strategy 执行对应的 lvStrategyType 策略。
 * 经典引擎的 graph 由该入口内部设置；navigator 手动挂接以提供完整上下文。
 */
static bool execute_legacy_bridge(ProofMultiStrategy *mse, ProofNavigator *nav) {
    if (!mse || !nav || !nav->target_prop)
        return false;

    lvProofEngine *pe = mse->legacy_proof_engine;
    if (!pe)
        return false;

    if (mse->active_strategy_index < 0 || mse->active_strategy_index >= PROOF_STRATEGY_COUNT)
        return false;

    ProofStrategyType bridge_type = mse->strategies[mse->active_strategy_index].type;
    lvStrategyType legacy_type = legacy_bridge_to_lv_strategy(bridge_type);

    pe->navigator = nav;

    lvProofTraceTree *trace = NULL;
    bool ok = lv_proof_engine_prove_with_strategy(pe, nav->target_prop, nav->construction, legacy_type, &trace);

    /* 切断经典引擎对临时溯源树的引用，避免后续 lv_proof_engine_destroy 二次释放 */
    pe->current_trace = NULL;
    if (trace)
        lv_trace_tree_destroy(trace);

    return ok;
}

/**
 * @brief 默认策略注册表（新增策略只需在这里添加一条记录）
 *
 * 搜索算法配置说明（默认 PROOF_SEARCH_DFS，保持既有行为不变）：
 * - GROEBNER_BASIS / DEDUCTIVE_DATABASE -> BFS：前向链演绎与代数方程组合的
 *   系统分层探索与 BFS 语义一致（从已知条件逐步推出新事实）。
 * - AREA_METHOD / COORDINATE -> BEST_FIRST：best_first 搜索的内置启发式
 *   已包含面积/约束密度评分，A* 优先展开最有希望的解析路径。
 * - 其余策略保持 DFS；MCTS 保留为可选能力（通过 register 按需启用）。
 */
static const StrategyRegistration default_strategy_table[] = {
    {PROOF_STRATEGY_DIRECT_CONSTRUCTION, "直接构造法",
     "通过几何构造直接满足命题模式，构造即证明",
     default_applicability_check, execute_direct_construction, PROOF_SEARCH_DFS},

    {PROOF_STRATEGY_AREA_METHOD, "面积法",
     "利用面积关系和消点法进行几何推理（借鉴JGEX面积法）",
     area_method_applicability_check, execute_area_method, PROOF_SEARCH_BEST_FIRST},

    {PROOF_STRATEGY_GROEBNER_BASIS, "Groebner基法",
     "将几何约束转化为多项式方程组，使用Buchberger算法求解",
     groebner_applicability_check, execute_groebner_basis, PROOF_SEARCH_BFS,
     (const char *[]) { "field_theory" }, 1},

    {PROOF_STRATEGY_VECTOR_METHOD, "向量法",
     "使用矢量代数进行几何关系推导",
     default_applicability_check, execute_vector_method, PROOF_SEARCH_DFS},

    {PROOF_STRATEGY_FULL_ANGLE_METHOD, "全角法",
     "利用全角关系进行角度推理和消点（借鉴JGEX全角法）",
     default_applicability_check, execute_full_angle_method, PROOF_SEARCH_DFS},

    {PROOF_STRATEGY_DEDUCTIVE_DATABASE, "演绎数据库法",
     "前向链推理，从已知条件逐步演绎新事实",
     default_applicability_check, execute_deductive_database, PROOF_SEARCH_BFS,
     (const char *[]) { "classical_propositional_logic" }, 1},

    {PROOF_STRATEGY_COORDINATE, "坐标法",
     "建立坐标系，使用解析几何方法进行计算和验证",
     default_applicability_check, execute_coordinate, PROOF_SEARCH_BEST_FIRST,
     (const char *[]) { "field_theory" }, 1},

    {PROOF_STRATEGY_LAMBDA_CALCULUS, "λ-演算归约法",
     "通过 β-归约化简 λ-项，基于 Church 编码进行函数式计算",
     lambda_calculus_applicability_check, execute_lambda_calculus, PROOF_SEARCH_DFS},

    {PROOF_STRATEGY_LAMBDA_UNIFY, "λ-演算合一法",
     "通过 λ-项模式合一自动匹配并实例化证明中的变量",
     lambda_unify_applicability_check, execute_lambda_unify, PROOF_SEARCH_DFS},

    {PROOF_STRATEGY_HOL_LIGHT, "HOL Light 微内核验证",
     "使用 10 条基本推理规则验证证明步骤的形式化正确性",
     default_applicability_check, execute_hol_light, PROOF_SEARCH_DFS},

    {PROOF_STRATEGY_ORACLE, "Oracle法",
     "调用外部求解器辅助验证非构造性命題",
     NULL, execute_oracle, PROOF_SEARCH_DFS},

    {PROOF_STRATEGY_NUMERIC_VERIFICATION, "数值验证",
     "使用区间算术求值 + FPTaylor 误差界分级验证浮点数值命题（含实数常量/比较谓词）",
     numeric_verification_applicability_check, execute_numeric_verification, PROOF_SEARCH_DFS},

    /* ── 经典策略体系（系统A）桥接条目 ──
     * execute 统一走 execute_legacy_bridge，将目标转交给
     * lv_proof_engine_prove_with_strategy。默认状态 UNAVAILABLE
     * （proof_multi_strategy_create 中设置），未挂载经典引擎时
     * 所有搜索算法跳过这些条目，既有默认行为完全不变。 */
    {PROOF_STRATEGY_LEGACY_DIRECT, "经典-直接证明",
     "桥接经典策略引擎：直接证明（合一+规则匹配）",
     legacy_bridge_applicability_check, execute_legacy_bridge, PROOF_SEARCH_DFS},

    {PROOF_STRATEGY_LEGACY_CONTRADICTION, "经典-反证法",
     "桥接经典策略引擎：假设目标否定推导矛盾",
     legacy_bridge_applicability_check, execute_legacy_bridge, PROOF_SEARCH_DFS},

    {PROOF_STRATEGY_LEGACY_CONTRAPOSITIVE, "经典-逆否证明",
     "桥接经典策略引擎：证明目标命题的逆否形式",
     legacy_bridge_applicability_check, execute_legacy_bridge, PROOF_SEARCH_DFS},

    {PROOF_STRATEGY_LEGACY_INDUCTION, "经典-数学归纳法",
     "桥接经典策略引擎：数学归纳法证明",
     legacy_bridge_applicability_check, execute_legacy_bridge, PROOF_SEARCH_DFS},

    {PROOF_STRATEGY_LEGACY_CASES, "经典-分情况讨论",
     "桥接经典策略引擎：分情况讨论证明",
     legacy_bridge_applicability_check, execute_legacy_bridge, PROOF_SEARCH_DFS},

    {PROOF_STRATEGY_LEGACY_CONSTRUCTION, "经典-构造性证明",
     "桥接经典策略引擎：构造性证明",
     legacy_bridge_applicability_check, execute_legacy_bridge, PROOF_SEARCH_DFS},

    {PROOF_STRATEGY_LEGACY_UNFOLDING, "经典-定义展开",
     "桥接经典策略引擎：定义展开证明",
     legacy_bridge_applicability_check, execute_legacy_bridge, PROOF_SEARCH_DFS},

    {PROOF_STRATEGY_LEGACY_BACKWARD, "经典-逆向推理",
     "桥接经典策略引擎：逆向推理证明",
     legacy_bridge_applicability_check, execute_legacy_bridge, PROOF_SEARCH_DFS},

    {PROOF_STRATEGY_LEGACY_FORWARD, "经典-正向推理",
     "桥接经典策略引擎：正向推理证明",
     legacy_bridge_applicability_check, execute_legacy_bridge, PROOF_SEARCH_DFS},

    {PROOF_STRATEGY_LEGACY_HYBRID, "经典-混合策略",
     "桥接经典策略引擎：混合策略（正向推理+反证法回退）",
     legacy_bridge_applicability_check, execute_legacy_bridge, PROOF_SEARCH_DFS},
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
            desc->search_algorithm = default_strategy_table[i].search_algorithm;

            /* 复制公理包依赖（与 proof_multi_strategy_register 相同的深拷贝约定：
             * 逐项 lv_strdup_safe，由 destroy 逐项 lv_free 释放） */
            if (default_strategy_table[i].required_axiom_packages && default_strategy_table[i].axiom_package_count > 0) {
                desc->axiom_package_count = default_strategy_table[i].axiom_package_count;
                desc->required_axiom_packages =
                    (char **) lv_calloc((size_t) desc->axiom_package_count, sizeof(char *));
                if (desc->required_axiom_packages) {
                    for (int j = 0; j < desc->axiom_package_count; j++) {
                        if (default_strategy_table[i].required_axiom_packages[j]) {
                            desc->required_axiom_packages[j] =
                                lv_strdup_safe(default_strategy_table[i].required_axiom_packages[j]);
                        }
                    }
                }
            }
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

    /* 经典策略桥接默认不可用：未挂载经典引擎时，所有搜索算法
     * （DFS/BFS/BEST_FIRST/MCTS/sledge/try_all）跳过这些条目，
     * 保证既有默认行为完全不变；挂载后由 setter 统一启用 */
    for (int t = PROOF_STRATEGY_LEGACY_DIRECT; t <= PROOF_STRATEGY_LEGACY_HYBRID; t++) {
        mse->strategies[t].status = PROOF_STRATEGY_UNAVAILABLE;
    }

    /* 分配计时数组 */
    mse->strategy_timings_ms = (int64_t *) lv_calloc(PROOF_STRATEGY_COUNT, sizeof(int64_t));
    if (!mse->strategy_timings_ms) {
        lv_free((void **) &mse);
        return NULL;
    }

    /* 默认回退顺序：直接构造 -> 面积法 -> Groebner -> 向量 -> 全角 -> 演绎 -> 坐标 -> HOL Light -> 数值验证 */
    int default_order[] = {
        PROOF_STRATEGY_DIRECT_CONSTRUCTION, PROOF_STRATEGY_AREA_METHOD,       PROOF_STRATEGY_GROEBNER_BASIS,
        PROOF_STRATEGY_VECTOR_METHOD,       PROOF_STRATEGY_FULL_ANGLE_METHOD, PROOF_STRATEGY_DEDUCTIVE_DATABASE,
        PROOF_STRATEGY_COORDINATE,          PROOF_STRATEGY_HOL_LIGHT,
        PROOF_STRATEGY_NUMERIC_VERIFICATION, /* 追加于末尾：既有策略全部失败后才尝试，纯增量 */
    };
    int default_count = sizeof(default_order) / sizeof(default_order[0]);
    proof_multi_strategy_set_fallback_order(mse, default_order, default_count);

    return mse;
}

void proof_multi_strategy_set_legacy_engine(ProofMultiStrategy *mse, lvProofEngine *engine) {
    if (!mse)
        return;

    mse->legacy_proof_engine = engine;

    /* 挂载/卸载经典引擎时同步启用/禁用桥接策略：
     * 默认 UNAVAILABLE（见 create），保证未挂载时既有行为不变 */
    for (int t = PROOF_STRATEGY_LEGACY_DIRECT; t <= PROOF_STRATEGY_LEGACY_HYBRID; t++) {
        mse->strategies[t].status = engine ? PROOF_STRATEGY_AVAILABLE : PROOF_STRATEGY_UNAVAILABLE;
    }
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

    /* 复制搜索算法配置；非法值（含调用方未初始化）回退 DFS，保持默认行为 */
    target->search_algorithm = descriptor->search_algorithm;
    if (target->search_algorithm < PROOF_SEARCH_DFS || target->search_algorithm >= PROOF_SEARCH_ALGO_COUNT) {
        target->search_algorithm = PROOF_SEARCH_DFS;
    }

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

/* ============== 公理包依赖校验 ============== */

/**
 * @brief 校验策略声明的公理包依赖是否已全部加载
 *
 * 将策略声明的依赖包名与引擎已加载公理包逐一比较
 * （engine->axiom_packages[0..axiom_package_count) 的 pkg->name，
 * 包名取自 .lvz 文件首行 `axiom "name" "version"`，与 INDEX.json 键名一致）。
 *
 * 未声明依赖（count<=0 或数组为 NULL）恒返回 true —— 保持默认行为不变。
 * 声明了依赖但引擎/导航器不可用、或任一依赖包未加载 → 返回 false。
 */
static bool proof_multi_strategy_axiom_deps_loaded(const ProofMultiStrategy *mse,
                                                   const ProofStrategyDescriptor *desc) {
    if (!mse || !desc || desc->axiom_package_count <= 0 || !desc->required_axiom_packages)
        return true; /* 未声明依赖：恒可用 */

    const ProofNavigator *nav = mse->shared_navigator;
    const lvEngine *engine = nav ? nav->engine : NULL;
    if (!engine || engine->axiom_package_count <= 0 || !engine->axiom_packages)
        return false; /* 声明了依赖，但引擎没有已加载的公理包 */

    for (int i = 0; i < desc->axiom_package_count; i++) {
        const char *want = desc->required_axiom_packages[i];
        if (!want || want[0] == '\0')
            continue; /* 空名称条目忽略 */

        bool found = false;
        for (int j = 0; j < engine->axiom_package_count; j++) {
            const AxiomPackage *pkg = engine->axiom_packages[j];
            if (pkg && pkg->name && strcmp(pkg->name, want) == 0) {
                found = true;
                break;
            }
        }
        if (!found)
            return false; /* 任一依赖未加载即视为依赖不足 */
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

    /* 公理包依赖校验：声明依赖的策略仅在依赖包全部已加载时才可激活；
     * 依赖不足 → 跳过该策略（不激活、不改状态），不影响其他策略 */
    if (!proof_multi_strategy_axiom_deps_loaded(mse, desc)) {
        lv_LOG_WARN("策略 '%s' 因公理包依赖未全部加载被跳过（%d 个依赖）", desc->name ? desc->name : "?",
                    desc->axiom_package_count);
        return false;
    }

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

        if (!proof_multi_strategy_activate(mse, type)) {
            return false; /* 流水线中断（含公理包依赖不足） */
        }
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
    {"经典-直接证明", PROOF_STRATEGY_LEGACY_DIRECT},
    {"经典-反证法", PROOF_STRATEGY_LEGACY_CONTRADICTION},
    {"经典-逆否证明", PROOF_STRATEGY_LEGACY_CONTRAPOSITIVE},
    {"经典-数学归纳法", PROOF_STRATEGY_LEGACY_INDUCTION},
    {"经典-分情况讨论", PROOF_STRATEGY_LEGACY_CASES},
    {"经典-构造性证明", PROOF_STRATEGY_LEGACY_CONSTRUCTION},
    {"经典-定义展开", PROOF_STRATEGY_LEGACY_UNFOLDING},
    {"经典-逆向推理", PROOF_STRATEGY_LEGACY_BACKWARD},
    {"经典-正向推理", PROOF_STRATEGY_LEGACY_FORWARD},
    {"经典-混合策略", PROOF_STRATEGY_LEGACY_HYBRID},
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
    {"numeric_verification", PROOF_STRATEGY_NUMERIC_VERIFICATION},
    {"legacy_direct", PROOF_STRATEGY_LEGACY_DIRECT},
    {"legacy_contradiction", PROOF_STRATEGY_LEGACY_CONTRADICTION},
    {"legacy_contrapositive", PROOF_STRATEGY_LEGACY_CONTRAPOSITIVE},
    {"legacy_induction", PROOF_STRATEGY_LEGACY_INDUCTION},
    {"legacy_cases", PROOF_STRATEGY_LEGACY_CASES},
    {"legacy_construction", PROOF_STRATEGY_LEGACY_CONSTRUCTION},
    {"legacy_unfolding", PROOF_STRATEGY_LEGACY_UNFOLDING},
    {"legacy_backward", PROOF_STRATEGY_LEGACY_BACKWARD},
    {"legacy_forward", PROOF_STRATEGY_LEGACY_FORWARD},
    {"legacy_hybrid", PROOF_STRATEGY_LEGACY_HYBRID},
};

const char *proof_strategy_type_to_string_en(ProofStrategyType strategy) {
    return lv_enum_to_str(s_proof_strategy_type_to_string_en_entries, lv_ARRAY_SIZE(s_proof_strategy_type_to_string_en_entries), (int) strategy, "unknown");
}

/* ============== 策略处理函数查找表 ============== */

/**
 * @brief 搜索算法到搜索执行函数的查找表（替代 switch-case）
 *
 * 每个策略通过其描述符的 search_algorithm 字段索引本表，
 * 从而按策略配置选择 DFS/BFS/A*（最佳优先）/MCTS 搜索算法。
 * 数组按 ProofSearchAlgorithm 枚举值升序排列。
 * 添加新搜索算法时只需在此表中添加一条记录。
 */
static bool (*const kSearchAlgorithmHandlers[PROOF_SEARCH_ALGO_COUNT])(ProofNavigator *, int) = {
    [PROOF_SEARCH_DFS]        = proof_depth_first_search,
    [PROOF_SEARCH_BFS]        = proof_breadth_first_search,
    [PROOF_SEARCH_BEST_FIRST] = proof_best_first_search,
    [PROOF_SEARCH_MCTS]       = proof_mcts_search,
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

    /* 按策略配置的搜索算法查表派发（替代 switch-case）
     * 非法搜索算法值回退 DFS，保证默认行为不变 */
    ProofMultiStrategy *mse = (ProofMultiStrategy *) proof->engine;
    ProofSearchAlgorithm algo = mse->strategies[strategy].search_algorithm;
    if (algo < PROOF_SEARCH_DFS || algo >= PROOF_SEARCH_ALGO_COUNT) {
        algo = PROOF_SEARCH_DFS;
    }
    return kSearchAlgorithmHandlers[algo](proof, max_steps);
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
