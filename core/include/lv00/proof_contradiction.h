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
 * @version 1.1.0
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
struct Lv00ContradictionClosure {
    int closure_id;                /**< 闭包ID */
    Lv00ProofScopeId scope_id;     /**< 所属作用域 */
    Lv00ContradictionType type;    /**< 矛盾类型 */
    Proposition *contradiction_prop; /**< 矛盾命题 */
    Proposition *origin_prop;      /**< 矛盾来源命题 */
    Lv00AssumptionEntry *triggering_assumption; /**< 触发假设 */
    int *derivation_path;          /**< 推导路径上的步骤ID */
    int derivation_path_length;    /**< 推导路径长度 */
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
struct Lv00ProofNavigatorEx {
    ProofNavigator base;           /**< 基类证明导航器 */
    Lv00AssumptionStack *assumption_stack;
    Lv00ContradictionClosure **closures;
    int closure_count;
    int closure_capacity;
    Lv00ContradictionBreakpoint **breakpoints;
    int breakpoint_count;
    int breakpoint_capacity;
    int total_assumptions;
    int total_closures;
    int total_breakpoints;
};
/* ============== API 函数声明 ============== */
LV00_PUBLIC_API Lv00AssumptionStack *lv00_assumption_stack_create(int capacity);
LV00_PUBLIC_API void lv00_assumption_stack_destroy(Lv00AssumptionStack *stack);
LV00_PUBLIC_API int lv00_assumption_stack_push(Lv00AssumptionStack *stack, Lv00ProofScopeId scope_id, Lv00AssumptionType type, Proposition *prop);
LV00_PUBLIC_API Lv00AssumptionEntry *lv00_assumption_stack_pop(Lv00AssumptionStack *stack);
LV00_PUBLIC_API Lv00AssumptionEntry *lv00_assumption_stack_find(Lv00AssumptionStack *stack, int assumption_id);
LV00_PUBLIC_API int lv00_assumption_stack_get_by_scope(Lv00AssumptionStack *stack, Lv00ProofScopeId scope_id, Lv00AssumptionEntry **out, int max_out);
LV00_PUBLIC_API Lv00ContradictionClosure *lv00_contradiction_closure_create(Lv00ProofScopeId scope_id, Lv00ContradictionType type, Proposition *prop);
LV00_PUBLIC_API void lv00_contradiction_closure_destroy(Lv00ContradictionClosure *closure);
LV00_PUBLIC_API int lv00_contradiction_closure_close(Lv00ContradictionClosure *closure);
LV00_PUBLIC_API int lv00_contradiction_propagation_detect(Lv00ProofScopeId scope, Lv00ContradictionClosure **closures, int closure_count);
LV00_PUBLIC_API Lv00ContradictionBreakpoint *lv00_contradiction_breakpoint_create(Lv00BreakpointType type, Lv00ProofScopeId scope_id, int step_id, const char *description);
LV00_PUBLIC_API void lv00_contradiction_breakpoint_destroy(Lv00ContradictionBreakpoint *bp);
LV00_PUBLIC_API int lv00_contradiction_detect_breakpoints(Lv00ProofNavigatorEx *navigator, Lv00ContradictionBreakpoint **breakpoints, int max_breakpoints);
LV00_PUBLIC_API Lv00ProofNavigatorEx *lv00_proof_navigator_ex_create(void);
LV00_PUBLIC_API void lv00_proof_navigator_ex_destroy(Lv00ProofNavigatorEx *navigator);
LV00_PUBLIC_API Lv00ProofScopeId lv00_proof_begin_contradiction(Lv00ProofNavigatorEx *navigator, Proposition *negation_prop);
LV00_PUBLIC_API int lv00_proof_end_contradiction(Lv00ProofNavigatorEx *navigator, Lv00ProofScopeId scope_id, Lv00ContradictionClosure **out_closure);
LV00_PUBLIC_API bool lv00_proof_scope_is_valid(Lv00ProofNavigatorEx *navigator, Lv00ProofScopeId scope_id);
LV00_PUBLIC_API char *lv00_proof_export_contradiction_trace(Lv00ProofNavigatorEx *navigator, Lv00ProofScopeId scope_id);
#endif /* LV00_PROOF_CONTRADICTION_H */
