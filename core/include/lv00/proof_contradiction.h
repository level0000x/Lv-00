/**
 * @file proof_contradiction.h
 * @brief 反证法与矛盾推演系统
 *
 * 实现局部矛盾闭包机制，确保反证法中的临时假设不会污染全局证明上下文。
 *
 * 核心概念：
 * - 假设栈（AssumptionStack）：管理反证法中的临时假设
 * - 局部矛盾闭包（LocalContradictionClosure）：限定矛盾推导范围
 * - 矛盾传播断点（ContradictionBreakpoint）：自动检测推导断点
 *
 * @version 4.0.0
 */

#ifndef LV00_PROOF_CONTRADICTION_H
#define LV00_PROOF_CONTRADICTION_H

#include <stdbool.h>
#include <stdint.h>

#include "proof.h"

/* ============== 前向声明 ============== */
typedef struct Lv00AssumptionStack Lv00AssumptionStack;
typedef struct Lv00ContradictionClosure Lv00ContradictionClosure;
typedef struct Lv00ContradictionBreakpoint Lv00ContradictionBreakpoint;
typedef struct Lv00ProofNavigatorEx Lv00ProofNavigatorEx;

/* ============== 假设类型 ============== */
typedef enum {
    ASSUMPTION_TYPE_TEMPORARY,     /**< 临时假设（反证法中使用） */
    ASSUMPTION_TYPE_LEMMA,         /**< 引理假设（可复用） */
    ASSUMPTION_TYPE_CONDITIONAL,   /**< 条件假设 */
    ASSUMPTION_TYPE_NUMERIC        /**< 数值假设 */
} Lv00AssumptionType;

/* ============== 假设条目 ============== */
typedef struct Lv00AssumptionEntry {
    int assumption_id;              /**< 假设ID */
    Lv00ProofScopeId scope_id;     /**< 所属作用域ID */
    Lv00AssumptionType type;        /**< 假设类型 */
    Proposition *prop;             /**< 假设命题 */
    int depth;                     /**< 假设深度（用于嵌套反证） */
    int parent_assumption_id;      /**< 父假设ID（-1表示顶层） */
    int derivation_step_count;     /**< 从此假设推导出的步骤数 */
    bool is_contradictory;        /**< 是否已检测到矛盾 */
    int64_t timestamp;             /**< 创建时间戳 */
} Lv00AssumptionEntry;

/* ============== 假设栈 ============== */
/**
 * @brief 假设栈结构
 *
 * 管理反证法中的临时假设栈。每个作用域维护独立的假设栈，
 * 确保局部假设不会影响全局证明上下文。
 */
struct Lv00AssumptionStack {
    Lv00AssumptionEntry *entries;   /**< 假设条目数组 */
    int count;                     /**< 当前假设数量 */
    int capacity;                  /**< 栈容量 */
    int max_depth;                 /**< 最大嵌套深度 */
    Lv00ProofScopeId current_scope;/**< 当前作用域ID */
};

/* ============== 矛盾类型 ============== */
typedef enum {
    CONTRADICTION_TYPE_NONE,        /**< 无矛盾 */
    CONTRADICTION_TYPE_DIRECT,      /**< 直接矛盾：P 和 ¬P 同时成立 */
    CONTRADICTION_TYPE_INDIRECT,    /**< 间接矛盾：通过推导得出矛盾 */
    CONTRADICTION_TYPE_PROPAGATED,  /**< 传播矛盾：从子假设传播而来 */
    CONTRADICTION_TYPE_SCOPE_LEAK   /**< 作用域泄露：局部矛盾污染全局 */
} Lv00ContradictionType;

/* ============== 矛盾闭包 ============== */
/**
 * @brief 局部矛盾闭包结构
 *
 * 记录反证法中发现的矛盾及其推导路径。矛盾闭包确保：
 * 1. 矛盾只在其作用域内有效
 * 2. 矛盾推导出 ex falso 只在该作用域内应用
 * 3. 闭包关闭后，所有临时结论被标记为失效
 */
struct Lv00ContradictionClosure {
    int closure_id;                /**< 闭包ID */
    Lv00ProofScopeId scope_id;     /**< 所属作用域 */
    Lv00ContradictionType type;    /**< 矛盾类型 */
    Proposition *contradiction_prop; /**< 矛盾命题 */
    Proposition *origin_prop;      /**< 矛盾来源命题 */
    Lv00AssumptionEntry *triggering_assumption; /**< 触发假设 */
    
    /* 推导路径 */
    int *derivation_path;          /**< 推导路径上的步骤ID */
    int derivation_path_length;    /**< 推导路径长度 */
    
    /* 闭包状态 */
    bool is_closed;                /**< 是否已关闭 */
    int64_t created_timestamp;     /**< 创建时间 */
    int64_t closed_timestamp;      /**< 关闭时间 */
};

/* ============== 断点类型 ============== */
typedef enum {
    BREAKPOINT_TYPE_ASSUMPTION,     /**< 假设引入断点 */
    BREAKPOINT_TYPE_DERIVATION,    /**< 推导步骤断点 */
    BREAKPOINT_TYPE_CONTRADICTION,  /**< 矛盾发现断点 */
    BREAKPOINT_TYPE_BACKTRACK       /**< 回溯断点 */
} Lv00BreakpointType;

/* ============== 矛盾传播断点 ============== */
/**
 * @brief 矛盾传播断点结构
 *
 * 自动检测反证法推导过程中的关键断点位置，便于调试和证明可视化。
 */
struct Lv00ContradictionBreakpoint {
    int breakpoint_id;             /**< 断点ID */
    Lv00BreakpointType type;       /**< 断点类型 */
    Lv00ProofScopeId scope_id;     /**< 所属作用域 */
    int step_id;                   /**< 关联步骤ID */
    int depth;                     /**< 断点深度 */
    char *description;             /**< 断点描述 */
    bool is_active;                /**< 是否激活 */
    int64_t timestamp;             /**< 创建时间 */
};

/* ============== 扩展证明导航器 ============== */
/**
 * @brief 扩展证明导航器（支持反证法）
 *
 * 在 ProofNavigator 基础上增加反证法专用功能：
 * - 假设栈管理
 * - 局部矛盾闭包
 * - 断点检测
 */
struct Lv00ProofNavigatorEx {
    ProofNavigator base;           /**< 基类证明导航器 */
    
    /* 假设栈 */
    Lv00AssumptionStack *assumption_stack;
    
    /* 矛盾闭包管理 */
    Lv00ContradictionClosure **closures;
    int closure_count;
    int closure_capacity;
    
    /* 断点管理 */
    Lv00ContradictionBreakpoint **breakpoints;
    int breakpoint_count;
    int breakpoint_capacity;
    
    /* 统计信息 */
    int total_assumptions;
    int total_closures;
    int total_breakpoints;
};

/* ============== API 函数声明 ============== */

/**
 * @brief 创建假设栈
 * @param capacity 初始容量
 * @return 假设栈指针，失败返回 NULL
 */
LV00_PUBLIC_API Lv00AssumptionStack *lv00_assumption_stack_create(int capacity);

/**
 * @brief 销毁假设栈
 * @param stack 假设栈指针
 */
LV00_PUBLIC_API void lv00_assumption_stack_destroy(Lv00AssumptionStack *stack);

/**
 * @brief 推送假设到栈
 * @param stack 假设栈
 * @param scope_id 作用域ID
 * @param type 假设类型
 * @param prop 假设命题
 * @return 新假设ID，失败返回 -1
 */
LV00_PUBLIC_API int lv00_assumption_stack_push(
    Lv00AssumptionStack *stack,
    Lv00ProofScopeId scope_id,
    Lv00AssumptionType type,
    Proposition *prop
);

/**
 * @brief 从栈中弹出假设
 * @param stack 假设栈
 * @return 弹出的假设条目，栈空返回 NULL
 */
LV00_PUBLIC_API Lv00AssumptionEntry *lv00_assumption_stack_pop(Lv00AssumptionStack *stack);

/**
 * @brief 查找假设
 * @param stack 假设栈
 * @param assumption_id 假设ID
 * @return 假设条目，未找到返回 NULL
 */
LV00_PUBLIC_API Lv00AssumptionEntry *lv00_assumption_stack_find(
    Lv00AssumptionStack *stack,
    int assumption_id
);

/**
 * @brief 获取作用域内的所有假设
 * @param stack 假设栈
 * @param scope_id 作用域ID
 * @param out 输出数组
 * @param max_out 最大输出数量
 * @return 实际输出数量
 */
LV00_PUBLIC_API int lv00_assumption_stack_get_by_scope(
    Lv00AssumptionStack *stack,
    Lv00ProofScopeId scope_id,
    Lv00AssumptionEntry **out,
    int max_out
);

/**
 * @brief 创建矛盾闭包
 * @param scope_id 作用域ID
 * @param type 矛盾类型
 * @param prop 矛盾命题
 * @return 闭包指针，失败返回 NULL
 */
LV00_PUBLIC_API Lv00ContradictionClosure *lv00_contradiction_closure_create(
    Lv00ProofScopeId scope_id,
    Lv00ContradictionType type,
    Proposition *prop
);

/**
 * @brief 销毁矛盾闭包
 * @param closure 闭包指针
 */
LV00_PUBLIC_API void lv00_contradiction_closure_destroy(Lv00ContradictionClosure *closure);

/**
 * @brief 关闭矛盾闭包
 *
 * 关闭后，闭包内的所有临时结论被标记为失效。
 * @param closure 闭包指针
 * @return 成功返回 true
 */
LV00_PUBLIC_API bool lv00_contradiction_closure_close(Lv00ContradictionClosure *closure);

/**
 * @brief 检测矛盾传播
 *
 * 检查是否存在从子假设传播到父假设的矛盾。
 * @param scope 作用域ID
 * @param closures 闭包数组
 * @param closure_count 闭包数量
 * @return 存在传播矛盾返回 true
 */
LV00_PUBLIC_API bool lv00_contradiction_propagation_detect(
    Lv00ProofScopeId scope,
    Lv00ContradictionClosure **closures,
    int closure_count
);

/**
 * @brief 创建断点
 * @param type 断点类型
 * @param scope_id 作用域ID
 * @param step_id 关联步骤ID
 * @param description 断点描述
 * @return 断点指针，失败返回 NULL
 */
LV00_PUBLIC_API Lv00ContradictionBreakpoint *lv00_contradiction_breakpoint_create(
    Lv00BreakpointType type,
    Lv00ProofScopeId scope_id,
    int step_id,
    const char *description
);

/**
 * @brief 销毁断点
 * @param bp 断点指针
 */
LV00_PUBLIC_API void lv00_contradiction_breakpoint_destroy(Lv00ContradictionBreakpoint *bp);

/**
 * @brief 检测推导断点
 *
 * 分析证明路径，自动识别关键断点位置。
 * @param navigator 证明导航器
 * @param breakpoints 输出断点数组
 * @param max_breakpoints 最大断点数量
 * @return 发现的断点数量
 */
LV00_PUBLIC_API int lv00_contradiction_detect_breakpoints(
    Lv00ProofNavigatorEx *navigator,
    Lv00ContradictionBreakpoint **breakpoints,
    int max_breakpoints
);

/**
 * @brief 创建扩展证明导航器
 * @return 导航器指针，失败返回 NULL
 */
LV00_PUBLIC_API Lv00ProofNavigatorEx *lv00_proof_navigator_ex_create(void);

/**
 * @brief 销毁扩展证明导航器
 * @param navigator 导航器指针
 */
LV00_PUBLIC_API void lv00_proof_navigator_ex_destroy(Lv00ProofNavigatorEx *navigator);

/**
 * @brief 开始反证法证明
 *
 * 创建新的作用域并开始反证法证明。
 * @param navigator 导航器
 * @param negation_prop 要否定的命题
 * @return 新作用域ID，失败返回 LV00_PROOF_SCOPE_INVALID
 */
LV00_PUBLIC_API Lv00ProofScopeId lv00_proof_begin_contradiction(
    Lv00ProofNavigatorEx *navigator,
    Proposition *negation_prop
);

/**
 * @brief 结束反证法证明
 *
 * 关闭当前作用域，检查是否发现矛盾。
 * @param navigator 导航器
 * @param scope_id 要关闭的作用域ID
 * @param out_closure 输出矛盾闭包（可为NULL）
 * @return 成功返回 true，scope_id 无效返回 false
 */
LV00_PUBLIC_API bool lv00_proof_end_contradiction(
    Lv00ProofNavigatorEx *navigator,
    Lv00ProofScopeId scope_id,
    Lv00ContradictionClosure **out_closure
);

/**
 * @brief 检查作用域是否有效
 * @param navigator 导航器
 * @param scope_id 作用域ID
 * @return 有效返回 true
 */
LV00_PUBLIC_API bool lv00_proof_scope_is_valid(
    Lv00ProofNavigatorEx *navigator,
    Lv00ProofScopeId scope_id
);

/**
 * @brief 导出反证证明跟踪
 *
 * 将反证证明路径导出为可读的字符串。
 * @param navigator 导航器
 * @param scope_id 作用域ID（LV00_PROOF_SCOPE_GLOBAL 表示全局）
 * @return 跟踪字符串（调用者负责释放），失败返回 NULL
 */
LV00_PUBLIC_API char *lv00_proof_export_contradiction_trace(
    Lv00ProofNavigatorEx *navigator,
    Lv00ProofScopeId scope_id
);

#endif /* LV00_PROOF_CONTRADICTION_H */
