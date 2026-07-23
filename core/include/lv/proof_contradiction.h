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
#ifndef lv_PROOF_CONTRADICTION_H
#define lv_PROOF_CONTRADICTION_H
#include <stdbool.h>
#include <stdint.h>
#include "proof.h"
/* ============== 前向声明 ============== */
typedef struct lvAssumptionStack lvAssumptionStack;
typedef struct lvContradictionClosure lvContradictionClosure;
typedef struct lvContradictionBreakpoint lvContradictionBreakpoint;
typedef struct lvProofNavigatorEx lvProofNavigatorEx;
/* ============== 假设类型 ============== */
typedef enum {
    ASSUMPTION_TYPE_TEMPORARY,     /**< 临时假设（反证法中使用） */
    ASSUMPTION_TYPE_LEMMA,         /**< 引理假设（可复用） */
    ASSUMPTION_TYPE_CONDITIONAL,   /**< 条件假设 */
    ASSUMPTION_TYPE_NUMERIC        /**< 数值假设 */
} lvAssumptionType;
/* ============== 假设条目 ============== */
typedef struct lvAssumptionEntry {
    int assumption_id;              /**< 假设ID */
    lvProofScopeId scope_id;     /**< 所属作用域ID */
    lvAssumptionType type;        /**< 假设类型 */
    Proposition *prop;             /**< 假设命题 */
    int depth;                     /**< 假设深度（用于嵌套反证） */
    int parent_assumption_id;      /**< 父假设ID（-1表示顶层） */
    int derivation_step_count;     /**< 从此假设推导出的步骤数 */
    bool is_contradictory;        /**< 是否已检测到矛盾 */
    int64_t timestamp;             /**< 创建时间戳 */
} lvAssumptionEntry;
/* ============== 假设栈 ============== */
struct lvAssumptionStack {
    lvAssumptionEntry *entries;   /**< 假设条目数组 */
    int count;                     /**< 当前假设数量 */
    int capacity;                  /**< 栈容量 */
    int max_depth;                 /**< 最大嵌套深度 */
    lvProofScopeId current_scope;/**< 当前作用域ID */
};
/* ============== 矛盾类型 ============== */
typedef enum {
    CONTRADICTION_TYPE_NONE,        /**< 无矛盾 */
    CONTRADICTION_TYPE_DIRECT,      /**< 直接矛盾：P 和 ¬P 同时成立 */
    CONTRADICTION_TYPE_INDIRECT,    /**< 间接矛盾：通过推导得出矛盾 */
    CONTRADICTION_TYPE_PROPAGATED,  /**< 传播矛盾：从子假设传播而来 */
    CONTRADICTION_TYPE_SCOPE_LEAK   /**< 作用域泄露：局部矛盾污染全局 */
} lvContradictionType;
/* ============== 矛盾闭包 ============== */
struct lvContradictionClosure {
    int closure_id;                /**< 闭包ID */
    lvProofScopeId scope_id;     /**< 所属作用域 */
    lvContradictionType type;    /**< 矛盾类型 */
    Proposition *contradiction_prop; /**< 矛盾命题 */
    Proposition *origin_prop;      /**< 矛盾来源命题 */
    lvAssumptionEntry *triggering_assumption; /**< 触发假设 */
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
} lvBreakpointType;
/* ============== 矛盾传播断点 ============== */
struct lvContradictionBreakpoint {
    int breakpoint_id;             /**< 断点ID */
    lvBreakpointType type;       /**< 断点类型 */
    lvProofScopeId scope_id;     /**< 所属作用域 */
    int step_id;                   /**< 关联步骤ID */
    int depth;                     /**< 断点深度 */
    char *description;             /**< 断点描述 */
    bool is_active;                /**< 是否激活 */
    int64_t timestamp;             /**< 创建时间 */
};
/* ============== 扩展证明导航器 ============== */
struct lvProofNavigatorEx {
    ProofNavigator base;           /**< 基类证明导航器 */
    lvAssumptionStack *assumption_stack;
    lvContradictionClosure **closures;
    int closure_count;
    int closure_capacity;
    lvContradictionBreakpoint **breakpoints;
    int breakpoint_count;
    int breakpoint_capacity;
    int total_assumptions;
    int total_closures;
    int total_breakpoints;
};
/* ============== API 函数声明 ============== */
lv_PUBLIC_API lvAssumptionStack *lv_assumption_stack_create(int capacity);
lv_PUBLIC_API void lv_assumption_stack_destroy(lvAssumptionStack *stack);
lv_PUBLIC_API int lv_assumption_stack_push(lvAssumptionStack *stack, lvProofScopeId scope_id, lvAssumptionType type, Proposition *prop);
lv_PUBLIC_API lvAssumptionEntry *lv_assumption_stack_pop(lvAssumptionStack *stack);
lv_PUBLIC_API lvAssumptionEntry *lv_assumption_stack_find(lvAssumptionStack *stack, int assumption_id);
lv_PUBLIC_API int lv_assumption_stack_get_by_scope(lvAssumptionStack *stack, lvProofScopeId scope_id, lvAssumptionEntry **out, int max_out);
lv_PUBLIC_API lvContradictionClosure *lv_contradiction_closure_create(lvProofScopeId scope_id, lvContradictionType type, Proposition *prop);
lv_PUBLIC_API void lv_contradiction_closure_destroy(lvContradictionClosure *closure);
lv_PUBLIC_API bool lv_contradiction_closure_close(lvContradictionClosure *closure);
lv_PUBLIC_API bool lv_contradiction_propagation_detect(lvProofScopeId scope, lvContradictionClosure **closures, int closure_count);
lv_PUBLIC_API lvContradictionBreakpoint *lv_contradiction_breakpoint_create(lvBreakpointType type, lvProofScopeId scope_id, int step_id, const char *description);
lv_PUBLIC_API void lv_contradiction_breakpoint_destroy(lvContradictionBreakpoint *bp);
lv_PUBLIC_API int lv_contradiction_detect_breakpoints(lvProofNavigatorEx *navigator, lvContradictionBreakpoint **breakpoints, int max_breakpoints);
lv_PUBLIC_API lvProofNavigatorEx *lv_proof_navigator_ex_create(void);
lv_PUBLIC_API void lv_proof_navigator_ex_destroy(lvProofNavigatorEx *navigator);
lv_PUBLIC_API lvProofScopeId lv_proof_begin_contradiction(lvProofNavigatorEx *navigator, Proposition *negation_prop);
lv_PUBLIC_API bool lv_proof_end_contradiction(lvProofNavigatorEx *navigator, lvProofScopeId scope_id, lvContradictionClosure **out_closure);
lv_PUBLIC_API bool lv_proof_scope_is_valid(lvProofNavigatorEx *navigator, lvProofScopeId scope_id);
lv_PUBLIC_API char *lv_proof_export_contradiction_trace(lvProofNavigatorEx *navigator, lvProofScopeId scope_id);
#endif /* lv_PROOF_CONTRADICTION_H */
