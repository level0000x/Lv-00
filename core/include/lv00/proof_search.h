/**
 * @file proof_search.h
 * @brief 证明搜索、策略引擎、回溯搜索树、参考项目集成
 *
 * 包含：
 * - 命题等价变换
 * - 依赖链断裂自动降级
 * - ⊥ 的公理包可定义性
 * - 辅助函数
 * - 命题实例化
 * - 不可构造性证明
 * - 回溯搜索树（Newclid 风格）
 * - 多策略证明引擎（JGEX 风格）
 * - 简化版搜索接口
 * - Agda hole-driven 证明编辑
 * - Idris 2 QTT 线性类型标记
 * - Isabelle/HOL Sledgehammer 自动证明策略调度
 */

#ifndef LV00_PROOF_SEARCH_H
#define LV00_PROOF_SEARCH_H

#include "proof_navigator.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============== 命题的等价变换 ============== */

/**
 * @brief 命题等价声明
 */
typedef struct PropositionEquivalence {
    int prop_a_id;
    int prop_b_id;
    ConstraintGraph *transformation; /* 双向变换规则 */
} PropositionEquivalence;

/**
 * @brief 声明两个命题等价
 */
LV00_PUBLIC_API void proof_declare_proposition_equivalence(ProofNavigator *nav, int prop_a_id, int prop_b_id);

/**
 * @brief 查找命题的等价命题
 */
LV00_PUBLIC_API int proof_find_equivalent_proposition(const ProofNavigator *nav, int prop_id, int *equivalent_ids, int max_count);

/* ============== 依赖链断裂自动降级 ============== */

/**
 * @brief 依赖更新结果
 *
 * 当公理包升级后，重新验证所有内引用。
 * 若内容哈希变化，自动降级信任颜色。
 */
typedef struct {
    int dependency_id;
    ProofColor old_color;
    ProofColor new_color;
    bool hash_changed;
} DependencyUpdateResult;

/**
 * @brief 验证并更新所有依赖链
 */
LV00_PUBLIC_API int proof_validate_dependencies(ProofNavigator *nav, DependencyUpdateResult *results, int max_results);

/* ============== ⊥ 的公理包可定义性 ============== */

/**
 * @brief 配置矛盾（⊥）的定义
 */
typedef struct BottomDefinition {
    bool has_input_ports; /* 是否有输入端口 */
    int input_port_count;
    bool allow_explosion; /* 是否允许爆炸原理 */
} BottomDefinition;

/**
 * @brief 设置 ⊥ 的定义
 */
LV00_PUBLIC_API void proof_set_bottom_definition(ProofNavigator *nav, const BottomDefinition *def);

/**
 * @brief 获取 ⊥ 的定义
 */
LV00_PUBLIC_API const BottomDefinition *proof_get_bottom_definition(const ProofNavigator *nav);

/* ============== 引理块折叠 ============== */

/**
 * @brief 设置引理的视图状态
 */
LV00_PUBLIC_API void proof_set_lemma_view_state(ProofNavigator *nav, int step_id, LemmaViewState state);

/**
 * @brief 获取引理的视图状态
 */
LV00_PUBLIC_API LemmaViewState proof_get_lemma_view_state(const ProofNavigator *nav, int step_id);

/* ============== 辅助函数 ============== */

/**
 * @brief 锁定公理库，禁止修改公理集合
 *
 * 锁定后，所有修改公理集合的操作（添加/删除/替换公理）
 * 将被拒绝。用于保护已验证的证明不因公理变化而失效。
 */
LV00_PUBLIC_API void proof_lock_axioms(void);

/**
 * @brief 解锁公理库，允许修改公理集合
 */
LV00_PUBLIC_API void proof_unlock_axioms(void);

/**
 * @brief 查询公理库锁定状态
 *
 * @return true 表示公理库已锁定，禁止修改
 */
LV00_PUBLIC_API bool proof_axioms_is_locked(void);

/**
 * 证明颜色转字符串
 */
LV00_PUBLIC_API const char *proof_color_to_string(ProofColor color);

/**
 * 命题类型转字符串
 */
LV00_PUBLIC_API const char *proposition_type_to_string(PropositionType type);

/**
 * 步骤类型转字符串
 */
LV00_PUBLIC_API const char *proof_step_type_to_string(ProofStepType type);

/**
 * 合一结果转字符串
 */
LV00_PUBLIC_API const char *unify_result_to_string(UnifyStatus result);

/* ============== 命题实例化 ============== */

/**
 * @brief 检查命题是否包含未实例化的类型变量
 *
 * 扫描命题的模式图中所有端口节点，检查其 type_region 是否为
 * TYPE_KIND_VARIABLE 类型。同时递归检查子命题。
 *
 * @param prop  要检查的命题
 * @return true 如果存在未实例化的类型变量，false 否则
 */
LV00_PUBLIC_API bool proof_has_type_variables(const Proposition *prop);

/**
 * @brief 实例化多态命题
 *
 * 将命题中的类型变量节点替换为具体的类型区域节点。
 * 创建命题的深拷贝，在副本上执行替换，不影响原始命题。
 *
 * @param prop               原始命题（不会被修改）
 * @param type_var_to_concrete  映射数组，交替存放 [type_var_node_id, concrete_node_id, ...]
 * @param mapping_count      映射条目数量（数组长度 = mapping_count * 2）
 * @return 新的已实例化命题，失败返回 NULL
 */
LV00_PUBLIC_API Proposition *proof_instantiate_proposition(const Proposition *prop, const int *type_var_to_concrete, int mapping_count);

/* ============== 不可构造性证明流程 ============== */

/**
 * @brief 不可构造性证明结果
 */
typedef enum {
    UNCONSTRUCT_PROVED,         /**< 证明成功：构造不可行 */
    UNCONSTRUCT_NOT_PROVED,     /**< 未能证明不可构造 */
    UNCONSTRUCT_MAYBE_POSSIBLE, /**< 可能可构造（不在已知不可构造列表中） */
    UNCONSTRUCT_ERROR           /**< 检查过程中出错 */
} UnconstructResult;

/**
 * @brief 不可构造性证明详细信息
 */
typedef struct {
    UnconstructResult result;    /**< 证明结果 */
    const char *matched_problem; /**< 匹配到的已知不可构造问题名 */
    const char *matched_theory;  /**< 匹配到的问题所属理论域 */
    const char *proof_strategy;  /**< 使用的证明策略描述 */
    char *detailed_report;       /**< 详细报告字符串（调用者需用lv00_free释放） */
    int reduction_steps;         /**< 归约步数 */
} UnconstructInfo;

/**
 * @brief 检查构造是否已知不可构造
 *
 * 在已加载的公理包中搜索匹配的不可构造性问题。
 * 检查构造的结构特征是否与已知的不可构造问题（如三等分角、
 * 倍立方、化圆为方等）相匹配。
 *
 * @param nav    证明导航器（含已加载的公理包引用）
 * @param graph  要检查的构造图
 * @param prop   相关的命题（可为NULL）
 * @param info   输出：不可构造性信息（调用者需用 unconstruct_info_destroy 释放）
 * @return 检查结果
 */
LV00_PUBLIC_API UnconstructResult proof_check_unconstructibility(ProofNavigator *nav, const ConstraintGraph *graph,
                                                 const Proposition *prop, UnconstructInfo *info);

/**
 * @brief 尝试系统性地证明不可构造性
 *
 * 使用多策略方法尝试证明构造不可行：
 * 1. 检查已知不可构造问题列表
 * 2. 尝试归约到已知不可构造问题
 * 3. 分析代数方程的可解性
 * 4. 检查是否超出特定几何系统范围
 *
 * @param nav      证明导航器
 * @param graph    要检查的构造图
 * @param prop     相关命题（可为NULL）
 * @param info     输出：不可构造性信息
 * @return 证明结果
 */
LV00_PUBLIC_API UnconstructResult proof_attempt_unconstructibility(ProofNavigator *nav, const ConstraintGraph *graph,
                                                   const Proposition *prop, UnconstructInfo *info);

/**
 * @brief 释放不可构造性信息结构体
 *
 * 释放 UnconstructInfo 中的动态分配内存（detailed_report）。
 * 注意：matched_problem、matched_theory、proof_strategy 指向静态字符串，无需释放。
 *
 * @param info 要释放的不可构造性信息
 */
LV00_PUBLIC_API void unconstruct_info_destroy(UnconstructInfo *info);

/* ============== 证明回溯与搜索树可视化（Newclid风格） ============== */

/**
 * @brief 回溯点类型
 */
typedef enum {
    BACKTRACK_CHOICE_POINT, /**< 选择点：多个策略分支 */
    BACKTRACK_FAILURE,      /**< 失败点：此路径不可行 */
    BACKTRACK_SUCCESS,      /**< 成功点：此路径到达目标 */
    BACKTRACK_PRUNE         /**< 剪枝点：启发式跳过 */
} BacktrackNodeType;

/**
 * @brief 证明搜索树节点（Newclid风格）
 *
 * 借鉴 Newclid 的证明搜索树可视化：
 * - 展示证明搜索过程中尝试了哪些路径
 * - 标注在哪些节点进行了回溯
 * - 支持在不同搜索策略之间切换观察效果
 */
typedef struct BacktrackNode {
    int id;                  /**< 节点ID */
    BacktrackNodeType type;  /**< 节点类型 */
    int step_index;          /**< 关联的证明步骤索引（-1 = 无关联） */
    char *label;             /**< 节点标签（如"尝试辅助线AD"） */
    char *strategy_name;     /**< 使用的策略名称 */
    bool is_backtrack_point; /**< 是否为回溯点 */

    /* 树结构 */
    struct BacktrackNode *parent;    /**< 父节点 */
    struct BacktrackNode **children; /**< 子节点数组 */
    int child_count;                 /**< 子节点数量 */
    int child_capacity;              /**< 子节点容量 */

    /* 颜色/状态 */
    bool explored;    /**< 是否已探索 */
    ProofColor color; /**< 节点信任颜色 */
} BacktrackNode;

/**
 * @brief 证明搜索树（Newclid风格）
 */
typedef struct {
    BacktrackNode *root;       /**< 根节点 */
    BacktrackNode **all_nodes; /**< 所有节点（用于遍历） */
    int node_count;            /**< 节点总数 */
    int node_capacity;         /**< 节点容量 */

    /* 统计信息 */
    int success_paths;   /**< 成功路径数 */
    int failure_paths;   /**< 失败路径数 */
    int backtrack_count; /**< 回溯次数 */
    int pruned_branches; /**< 剪枝分支数 */
    int max_depth;       /**< 最大搜索深度 */

    /* 策略信息 */
    char *current_strategy;      /**< 当前使用的搜索策略名称 */
    char **available_strategies; /**< 可用策略名称列表 */
    int strategy_count;          /**< 策略数量 */
} ProofSearchTree;

/* --- API --- */

/**
 * @brief 创建证明搜索树
 * @return 新分配的搜索树，失败返回NULL
 */
LV00_PUBLIC_API ProofSearchTree *proof_search_tree_create(void);

/**
 * @brief 销毁证明搜索树（递归释放所有节点）
 */
LV00_PUBLIC_API void proof_search_tree_destroy(ProofSearchTree *tree);

/**
 * @brief 创建回溯节点
 * @param type   节点类型
 * @param label  节点标签
 * @return 新分配的节点，失败返回NULL
 */
LV00_PUBLIC_API BacktrackNode *backtrack_node_create(BacktrackNodeType type, const char *label);

/**
 * @brief 向搜索树添加子节点
 * @param tree   搜索树
 * @param parent 父节点（传NULL则设为根节点）
 * @param child  子节点
 * @return 是否成功
 */
LV00_PUBLIC_API bool proof_search_tree_add_child(ProofSearchTree *tree, BacktrackNode *parent, BacktrackNode *child);

/**
 * @brief 标记回溯点
 * @param node         要标记的节点
 * @param strategy_name 使用的策略名称
 */
LV00_PUBLIC_API void backtrack_node_mark_backtrack(BacktrackNode *node, const char *strategy_name);

/**
 * @brief 注册可用策略
 * @param tree          搜索树
 * @param strategy_name 策略名称
 */
LV00_PUBLIC_API void proof_search_tree_register_strategy(ProofSearchTree *tree, const char *strategy_name);

/**
 * @brief 设置当前策略
 * @param tree          搜索树
 * @param strategy_name 策略名称
 */
LV00_PUBLIC_API void proof_search_tree_set_strategy(ProofSearchTree *tree, const char *strategy_name);

/**
 * @brief 导出搜索树为JSON（用于Web GUI可视化）
 * @param tree      搜索树
 * @param filepath   输出文件路径
 * @return 是否成功
 */
LV00_PUBLIC_API bool proof_search_tree_export_json(const ProofSearchTree *tree, const char *filepath);

/**
 * @brief 导出搜索树为DOT格式（Graphviz）
 * @param tree      搜索树
 * @param filepath   输出文件路径
 * @return 是否成功
 */
LV00_PUBLIC_API bool proof_search_tree_export_dot(const ProofSearchTree *tree, const char *filepath);

/* ========================================================================
 * 多证明方法并存引擎（v3.2.0 新增，借鉴 JGEX/GEX 架构）
 *
 * 借鉴 JGEX（中科院张景中团队）的多证明方法共存设计：
 * - 在同一系统中集成多种独立的证明方法
 * - 用户可在不同策略之间切换
 * - 每种方法有独立的搜索空间和输出格式
 *
 * JGEX 集成了六种证明方法：
 *   Wu's Method, Area Method, Groebner Basis, Vector Method,
 *   Full-Angle Method, Deductive Database
 *
 * Lv-00 将其适配为几何元语言环境下的多策略架构。
 * ======================================================================== */

/**
 * @brief 证明策略类型（借鉴 JGEX 的六种方法）
 */
typedef enum {
    PROOF_STRATEGY_DIRECT_CONSTRUCTION, /**< 直接构造法：通过几何构造直接满足命题模式 */
    PROOF_STRATEGY_AREA_METHOD,         /**< 面积法：利用面积关系和消点法（借鉴 JGEX Area Method） */
    PROOF_STRATEGY_GROEBNER_BASIS,      /**< Groebner 基法：代数方程求解（借鉴 JGEX/Wu's Method） */
    PROOF_STRATEGY_VECTOR_METHOD,       /**< 向量法：矢量代数推导 */
    PROOF_STRATEGY_FULL_ANGLE_METHOD,   /**< 全角法：利用全角关系进行角度推理 */
    PROOF_STRATEGY_DEDUCTIVE_DATABASE,  /**< 演绎数据库法：前向链推理 */
    PROOF_STRATEGY_COORDINATE,          /**< 坐标法：解析几何坐标计算 */
    PROOF_STRATEGY_ORACLE,              /**< Oracle 法：外部求解器辅助（不可构造性） */
    PROOF_STRATEGY_COUNT                /**< 策略总数（用于数组大小） */
} ProofStrategyType;

/* Forward declaration: ProofMultiStrategy 结构体在下方完整定义 */
struct ProofMultiStrategy;

/**
 * @brief 证明策略状态
 */
typedef enum {
    PROOF_STRATEGY_AVAILABLE,   /**< 可用（已加载所需公理包） */
    PROOF_STRATEGY_UNAVAILABLE, /**< 不可用（缺少公理包） */
    PROOF_STRATEGY_ACTIVE,      /**< 当前激活 */
    PROOF_STRATEGY_COMPLETED,   /**< 已完成 */
    PROOF_STRATEGY_FAILED       /**< 失败 */
} ProofStrategyStatus;

/**
 * @brief 证明策略描述符
 *
 * 每种证明方法对应一个策略描述符，记录其：
 * - 基本元数据（名称、描述）
 * - 依赖的公理包
 * - 适用的问题类型
 * - 产生的证明步骤
 */
typedef struct ProofStrategyDescriptor {
    ProofStrategyType type;         /**< 策略类型 */
    ProofStrategyStatus status;     /**< 当前状态 */
    char *name;                     /**< 策略名称（如"面积法"） */
    char *description;              /**< 策略描述 */
    char **required_axiom_packages; /**< 依赖的公理包名称列表 */
    int axiom_package_count;        /**< 公理包数量 */

    /* 适用性评估 */
    bool (*applicability_check)(/**< 适用性检查函数 */
                                const struct ProofMultiStrategy *mse, const ConstraintGraph *graph,
                                const Proposition *prop);

    /* 策略执行 */
    bool (*execute)(/**< 策略执行函数 */
                    struct ProofMultiStrategy *mse, ProofNavigator *nav);

    /* 生成的证明步骤 */
    int generated_step_count; /**< 生成的步骤数 */
    int *generated_step_ids;  /**< 生成的步骤ID列表 */
} ProofStrategyDescriptor;

/**
 * @brief 多策略证明引擎（借鉴 JGEX 架构）
 *
 * 管理多种证明方法的注册、切换、组合执行。
 * 支持：
 * - 策略注册与发现
 * - 策略切换（运行时）
 * - 策略组合（流水线：一个方法的输出作为另一个的输入）
 * - 策略竞争（多方法并行，取最先成功者）
 * - 策略适用性自动评估
 */
typedef struct ProofMultiStrategy {
    ProofStrategyDescriptor strategies[PROOF_STRATEGY_COUNT]; /**< 策略数组 */
    int active_strategy_index;                                /**< 当前激活的策略索引（-1 = 未选择） */
    ProofNavigator *shared_navigator;                         /**< 共享的证明导航器 */

    /* 策略组合配置 */
    bool enable_fallback; /**< 是否启用回退（主策略失败后尝试其他） */
    int *fallback_order;  /**< 回退顺序（策略索引数组） */
    int fallback_count;   /**< 回退策略数量 */

    /* 执行统计 */
    int total_attempts;           /**< 总尝试次数 */
    int success_count;            /**< 成功次数 */
    int64_t *strategy_timings_ms; /**< 每种策略的耗时（毫秒） */
} ProofMultiStrategy;

/* --- 多策略引擎 API --- */

/**
 * @brief 创建多策略证明引擎
 * @param nav  共享的证明导航器（可为NULL，稍后设置）
 * @return 新分配的多策略引擎，失败返回NULL
 */
LV00_PUBLIC_API ProofMultiStrategy *proof_multi_strategy_create(ProofNavigator *nav);

/**
 * @brief 销毁多策略证明引擎
 */
LV00_PUBLIC_API void proof_multi_strategy_destroy(ProofMultiStrategy *mse);

/**
 * @brief 注册证明策略
 * @param mse        多策略引擎
 * @param descriptor 策略描述符
 * @return 是否成功
 */
LV00_PUBLIC_API bool proof_multi_strategy_register(ProofMultiStrategy *mse, const ProofStrategyDescriptor *descriptor);

/**
 * @brief 激活指定策略
 * @param mse           多策略引擎
 * @param strategy_type 要激活的策略类型
 * @return 是否成功
 */
LV00_PUBLIC_API bool proof_multi_strategy_activate(ProofMultiStrategy *mse, ProofStrategyType strategy_type);

/**
 * @brief 获取当前激活的策略
 * @return 策略描述符指针（不可修改），无激活策略返回NULL
 */
LV00_PUBLIC_API const ProofStrategyDescriptor *proof_multi_strategy_get_active(const ProofMultiStrategy *mse);

/**
 * @brief 评估所有可用策略的适用性
 *
 * 遍历所有已注册的策略，调用其 applicability_check 函数，
 * 返回适用策略的列表，按适用性评分排序。
 *
 * @param mse    多策略引擎
 * @param graph  目标构造图
 * @param prop   目标命题
 * @param out_applicable_types 输出：适用的策略类型数组
 * @param max_count            最多返回数量
 * @return 实际返回的适用策略数量
 */
LV00_PUBLIC_API int proof_multi_strategy_evaluate_applicability(ProofMultiStrategy *mse, const ConstraintGraph *graph,
                                                const Proposition *prop, ProofStrategyType *out_applicable_types,
                                                int max_count);

/**
 * @brief 使用当前策略执行证明
 * @return 是否成功
 */
LV00_PUBLIC_API bool proof_multi_strategy_execute(ProofMultiStrategy *mse);

/**
 * @brief 尝试所有可用策略（竞争模式）
 *
 * 按回退顺序依次尝试每个可用策略，直到某个策略成功或全部失败。
 * 借鉴 JGEX 的用户可选策略机制。
 *
 * @return 成功的策略类型，失败返回 PROOF_STRATEGY_COUNT
 */
LV00_PUBLIC_API ProofStrategyType proof_multi_strategy_try_all(ProofMultiStrategy *mse);

/**
 * @brief 使用多个策略组合证明（流水线模式）
 *
 * 将多个策略按顺序串联：前一个策略的输出作为后一个策略的输入。
 * 例如：先用面积法建立引理，再用直接构造法完成主证明。
 *
 * @param mse            多策略引擎
 * @param pipeline       策略类型流水线（按顺序执行）
 * @param pipeline_count 流水线长度
 * @return 是否全部成功
 */
LV00_PUBLIC_API bool proof_multi_strategy_pipeline(ProofMultiStrategy *mse, const ProofStrategyType *pipeline, int pipeline_count);

/**
 * @brief 设置回退顺序
 * @param mse             多策略引擎
 * @param fallback_order  策略索引数组（按优先级排序）
 * @param count           回退策略数量
 */
LV00_PUBLIC_API void proof_multi_strategy_set_fallback_order(ProofMultiStrategy *mse, const int *fallback_order, int count);

/**
 * @brief 切换策略（保存当前策略状态后切换）
 * @param mse           多策略引擎
 * @param strategy_type 目标策略类型
 * @return 是否成功
 */
LV00_PUBLIC_API bool proof_multi_strategy_switch(ProofMultiStrategy *mse, ProofStrategyType strategy_type);

/**
 * @brief 获取策略执行统计
 * @param mse              多策略引擎
 * @param out_total_attempts  输出：总尝试次数
 * @param out_success_count   输出：成功次数
 */
LV00_PUBLIC_API void proof_multi_strategy_get_stats(const ProofMultiStrategy *mse, int *out_total_attempts, int *out_success_count);

/**
 * @brief 策略类型转字符串
 */
LV00_PUBLIC_API const char *proof_strategy_type_to_string(ProofStrategyType type);

/**
 * @brief 策略状态转字符串
 */
LV00_PUBLIC_API const char *proof_strategy_status_to_string(ProofStrategyStatus status);

/**
 * @brief 策略类型转字符串（英文版）
 *
 * 与 proof_strategy_type_to_string 不同，返回英文标识符，
 * 便于日志输出和调试。
 */
LV00_PUBLIC_API const char *proof_strategy_type_to_string_en(ProofStrategyType type);

/* ============== 简化版搜索接口 ============== */

/**
 * @brief 使用指定策略执行证明搜索（简化接口）
 *
 * 封装多策略引擎，提供更直观的调用方式。
 *
 * @param proof      证明导航器指针
 * @param strategy   搜索策略类型
 * @param max_steps  最大搜索步数
 * @return true 找到证明，false 搜索失败或超时
 */
LV00_PUBLIC_API bool proof_search_with_strategy(ProofNavigator *proof, ProofStrategyType strategy, int max_steps);

/**
 * @brief 使用蒙特卡洛树搜索执行证明（简化接口）
 */
LV00_PUBLIC_API bool proof_mcts_execute(ProofNavigator *proof, int max_steps);

/**
 * @brief 执行广度优先搜索证明（简化接口）
 */
LV00_PUBLIC_API bool proof_bfs_execute(ProofNavigator *proof, int max_steps);

/**
 * @brief 执行最佳优先搜索证明（简化接口）
 */
LV00_PUBLIC_API bool proof_best_first_execute(ProofNavigator *proof, int max_steps);


/* ================================================================
 * === 第六梯队参考项目落地 (P1) — 2026-05-24 ======================
 * 新增 API 声明来自 Agda/Idris2/Isabelle/HOL Light/F* 五个项目
 * ================================================================ */

/* --- 前向声明 --- */
typedef struct ConstraintSolver ConstraintSolver;

/* ================================================================
 * 1. Agda — hole-driven 证明编辑
 *    借鉴：逐"洞"填充的交互式证明方式，Lv-00 Web GUI 对应功能
 * ================================================================ */

/** @brief 填充建议结构体 — Agda hole-driven 证明编辑 */
typedef enum { FILL_EXACT, FILL_LAMBDA, FILL_CONSTRUCTOR, FILL_CASE_SPLIT, FILL_REFINE } FillKind;

typedef struct FillSuggestion {
    FillKind kind;
    char *label;        /* 建议描述 */
    char *code_snippet; /* 填充代码片段 */
    int arity;          /* 构造器元数 */
    struct FillSuggestion *next;
} FillSuggestion;

/**
 * @brief 引导式洞填充 — 声明几何命题后，系统引导用户逐步填充证明
 * @param solver      约束求解器上下文
 * @param goal_type   目标几何命题的类型声明（如"等腰三角形面积公式"）
 * @param goal_dim    维度
 * @return 填充建议链表，调用者用 fill_suggestions_destroy() 释放
 */
LV00_PUBLIC_API FillSuggestion *proof_guided_fill(ConstraintSolver *solver, const char *goal_type, int goal_dim);
LV00_PUBLIC_API void fill_suggestions_destroy(FillSuggestion *list);

/* ================================================================
 * 2. Idris 2 — QTT 线性类型标记（0/1/ω），证明仅编译期
 * ================================================================ */

/** @brief QTT 用量标注（借鉴 Idris 2 Quantitative Type Theory） */
typedef enum { PROOF_QTT_ERASED = 0, PROOF_QTT_LINEAR = 1, PROOF_QTT_UNRESTRICTED = 2 } ProofQuantifier;

/**
 * @brief 标记构造步骤为 Ghost（仅编译期存在，运行时擦除）
 * @param nav      证明导航器（持有 ghost 标记表）
 * @param step_id  证明步骤 ID
 * @param quant    用量标注（ERASED=仅证明，LINEAR=精确一次，UNRESTRICTED=可多次）
 * @return 是否成功
 */
LV00_PUBLIC_API bool proof_mark_ghost(ProofNavigator *nav, int step_id, ProofQuantifier quant);

/**
 * @brief 检查 Ghost 冲突 — 确认被运行时计算依赖的步骤未被标记为 ERASED
 * @param nav  证明导航器（持有 ghost 标记表）
 * @return 冲突数量（0 = 无冲突）
 */
LV00_PUBLIC_API int proof_check_ghost_conflicts(ProofNavigator *nav);

/* ================================================================
 * 3. Isabelle/HOL — Sledgehammer 自动证明策略调度
 * ================================================================ */

/** @brief Sledgehammer 调用模式 */
typedef enum { SLEDGE_SYNC, SLEDGE_ASYNC, SLEDGE_TIMEOUT } SledgehammerMode;

/** @brief Isar 结构化证明层级 */
typedef enum { ISAR_LEMMA, ISAR_HAVE, ISAR_SHOW, ISAR_QED } IsarStructureLevel;

/** @brief Sledgehammer 单个策略执行结果 */
typedef struct {
    ProofStrategyType strategy;
    bool success;
    double LV00_TOLERATED_FLOAT(elapsed_sec); /* @note tolerated: timing only */
    char *isar_proof_script; /* 自动生成的 Isar 证明脚本 */
} SledgehammerStrategyResult;

/** @brief Sledgehammer 批量调度报告 */
typedef struct {
    SledgehammerStrategyResult *results;
    int result_count;
    int best_index; /* 最优（最简）证明的索引 */
    double LV00_TOLERATED_FLOAT(total_time_sec); /* @note tolerated: timing only */
    const char *error_msg;
} SledgehammerReport;

/**
 * @brief Sledgehammer 风格 — 自动尝试多个证明策略，返回最优结果
 * @param mse          多策略引擎
 * @param mode         调度模式（同步/异步/超时）
 * @param timeout_ms   超时毫秒（0 = 不限）
 * @return 调度报告，调用者用 sledgehammer_report_destroy() 释放
 */
LV00_PUBLIC_API SledgehammerReport *proof_sledgehammer_dispatch(ProofMultiStrategy *mse, SledgehammerMode mode, int timeout_ms);
LV00_PUBLIC_API void sledgehammer_report_destroy(SledgehammerReport *report);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PROOF_SEARCH_H */
