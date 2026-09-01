/**
 * @file lv_reasoning_stack.h
 * @brief 推理分支栈独立模块 —— 管理推理路径的栈结构
 *
 * @details 从 lvContext God Object 中提取的独立推理栈子系统。
 *          提供推理分支栈的管理：推入/弹出帧、查询栈状态等。
 *
 *          推理过程中，前向证明、反证法、假设引入等操作会创建推理分支。
 *          每个分支保存当前推理的快照，以便在分支失败时回滚。
 *
 *          分支栈支持以下推理策略：
 *          - 前向证明 (forward proof)：从已知前提出发，逐步推导结论
 *          - 反证法 (proof by contradiction)：假设命题为假，推导矛盾
 *          - 假设引入 (hypothesis introduction)：引入临时假设进行推理
 *
 *          本模块是 lvContext 中 ReasoningStack 的独立版本，
 *          不依赖 lvContext 或任何上层结构体。
 *
 * @author Lv-00 Project
 * @version 1.0.0
 * @date   2026-07-31
 */
#ifndef lv_LV_REASONING_STACK_H
#define lv_LV_REASONING_STACK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ============================================================
 * 默认配置（与 context.h 中的原始常量保持一致）
 * ============================================================ */

#ifndef lv_REASONING_STACK_DEFAULT_CAPACITY
#define lv_REASONING_STACK_DEFAULT_CAPACITY 8   /**< 推理栈默认初始容量 */
#endif

#ifndef lv_REASONING_STACK_MAX_DEPTH
#define lv_REASONING_STACK_MAX_DEPTH 1000       /**< 推理栈最大深度上限 */
#endif

/* ============================================================
 * 推理分支类型枚举
 * ============================================================ */

typedef enum {
    lv_REASONING_BRANCH_NONE,          /**< 无分支（主推理线） */
    lv_REASONING_BRANCH_FORWARD,       /**< 前向证明分支 */
    lv_REASONING_BRANCH_CONTRADICTION, /**< 反证法分支：假设否定命题成立 */
    lv_REASONING_BRANCH_HYPOTHESIS     /**< 假设引入分支：引入临时假设 */
} lvReasoningBranchType;

/** @brief 向后兼容类型别名 */
typedef lvReasoningBranchType ReasoningBranchType;

/*
 * 注意：lv_REASONING_BRANCH_* 枚举值可直接使用。
 * 不提供 REASONING_BRANCH_* 命名空间的向后兼容宏，
 * 以避免与外部代码中的本地枚举常量冲突。
 * 如有需要，外部代码应直接使用 lv_REASONING_BRANCH_* 前缀。
 */

/* ============================================================
 * 推理分支状态枚举
 * ============================================================ */

typedef enum {
    lv_BRANCH_ACTIVE,   /**< 分支仍在推理中 */
    lv_BRANCH_CLOSED,   /**< 分支已闭合（到达目标） */
    lv_BRANCH_FAILED,   /**< 分支推理失败 */
    lv_BRANCH_ABANDONED /**< 分支被放弃（超时/熔断） */
} lvReasoningBranchStatus;

/** @brief 向后兼容类型别名 */
typedef lvReasoningBranchStatus ReasoningBranchStatus;

/*
 * 不提供 BRANCH_* 向后兼容宏以避免与外部枚举冲突。
 * 请直接使用 lv_BRANCH_* 枚举值。
 */

/* ============================================================
 * 前向声明
 * ============================================================ */
struct ConstraintGraph;

/* ============================================================
 * 推理栈帧结构体
 * ============================================================ */

/**
 * @brief 推理栈帧 —— 记录进入一个推理分支时的状态
 *
 * 每个分支帧包含：
 * - 分支类型和状态，用于追踪推理路径
 * - 分支入口处约束图的深拷贝（用于回滚）
 * - 推理步骤计数器，防止无限深度
 * - 该分支特定的假设列表和目标列表
 */
typedef struct lvReasoningFrame {
    /** 分支类型：前向证明 / 反证法 / 假设引入 */
    lvReasoningBranchType branch_type;

    /** 分支当前状态 */
    lvReasoningBranchStatus status;

    /** 该分支的推理深度（相对于根上下文） */
    int depth;

    /** 该分支内执行的推理步骤计数 */
    int step_count;

    /** 分支入口处约束图的深拷贝快照（回滚目标） */
    struct ConstraintGraph *graph_snapshot;

    /** 该分支引入的假设节点 ID 数组（以 -1 结尾） */
    int *assumption_node_ids;

    /** 假设数量 */
    int assumption_count;

    /** 该分支的目标节点 ID 数组（以 -1 结尾） */
    int *target_node_ids;

    /** 目标数量 */
    int target_count;

    /** 该分支的 AST 语法树根节点浅拷贝（指向上下文 AST 的引用） */
    void *ast_root_ref;

    /** 分支创建时间戳（微秒，用于超时判定） */
    uint64_t created_at_us;

    /** 分支的自行超时时间（毫秒，0 = 继承父上下文超时） */
    uint64_t timeout_ms;

    /** 用户自留的数据指针（用于扩展） */
    void *user_data;
} lvReasoningFrame;

/* 向后兼容别名（ReasoningBranchType 别名已在枚举下方定义） */
typedef lvReasoningFrame ReasoningFrame;

/* ============================================================
 * 推理分支栈结构体
 * ============================================================ */

/**
 * @brief 推理分支栈 —— 管理推理路径的栈结构
 *
 * 最大深度由 max_depth 限制。
 * 当栈满时，新的分支创建会失败并返回错误码。
 */
typedef struct lvReasoningStack {
    /** 栈帧数组（动态增长，但不超过 max_depth） */
    lvReasoningFrame *frames;

    /** 当前栈顶索引（-1 表示空栈，0 表示根帧） */
    int top;

    /** 栈当前容量 */
    int capacity;

    /** 栈绝对最大深度（在上下文创建时设定） */
    int max_depth;

    /** 不透明快照释放回调（F24/I5：L2 不依赖 L3，graph_snapshot 由
     *  L0 注入的 ConstraintGraph 销毁回调承接；NULL = 快照不释放） */
    void (*graph_snapshot_free)(void *snapshot);
} lvReasoningStack;

/** @brief 向后兼容别名 */
typedef lvReasoningStack ReasoningStack;

/* ============================================================
 * 推理栈 API
 *
 * 这些函数直接操作 lvReasoningStack 结构体，
 * 不依赖 lvContext 或任何上层结构体。
 * ============================================================ */

/**
 * @brief 初始化推理栈为空栈
 *
 * 将栈设置为空状态：
 *   - frames = NULL
 *   - top = -1
 *   - capacity = 0
 *   - max_depth = lv_REASONING_STACK_MAX_DEPTH
 *
 * @param stack 推理栈指针（非 NULL）
 */
void lv_reasoning_stack_init(lvReasoningStack *stack);

/**
 * @brief 清空推理栈，释放所有帧资源
 *
 * 释放每帧的 graph_snapshot、assumption_node_ids、target_node_ids，
 * 然后释放 frames 数组，最后将栈重置为空状态。
 *
 * @param stack 推理栈指针（非 NULL）
 */
void lv_reasoning_stack_clear(lvReasoningStack *stack);

/**
 * @brief 确保推理栈有足够容量（内部辅助，供上层使用）
 *
 * 如果当前容量不足以再放一个元素，按 2 倍因子扩容，
 * 但不超过 max_depth 限制。
 *
 * @param stack 推理栈指针（非 NULL）
 * @return lv_OK 成功，lv_ERROR_OUT_OF_MEMORY 内存不足，
 *         lv_ERROR_RESOURCE_EXHAUSTED 已达最大深度
 */
int lv_reasoning_stack_ensure_capacity(lvReasoningStack *stack);

/**
 * @brief 在栈顶压入一个新帧
 *
 * 自动扩容，初始化帧的基本字段（branch_type、status、depth、step_count 归零）。
 * 调用者需要额外设置以下字段：
 *   - timeout_ms
 *   - created_at_us
 *   - ast_root_ref
 *   - graph_snapshot（约束图快照）
 *
 * @param stack       推理栈指针（非 NULL）
 * @param branch_type 分支类型
 * @return lv_OK 成功，lv_ERROR_OUT_OF_MEMORY 内存不足，
 *         lv_ERROR_RESOURCE_EXHAUSTED 已达最大深度
 */
int lv_reasoning_stack_push(lvReasoningStack *stack, lvReasoningBranchType branch_type);

/**
 * @brief 弹出栈顶帧并释放其资源
 *
 * 释放 top 帧的 graph_snapshot、assumption_node_ids、target_node_ids，
 * 将帧内存清零，然后递减 top 索引。
 *
 * @param stack 推理栈指针（非 NULL）
 * @return lv_OK 成功，lv_ERROR_INVALID_STATE 栈为空
 */
int lv_reasoning_stack_pop(lvReasoningStack *stack);

/**
 * @brief 获取推理栈中的帧数量
 *
 * @param stack 推理栈指针（非 NULL）
 * @return 栈中帧的数量（0 = 空栈）
 */
int lv_reasoning_stack_count(const lvReasoningStack *stack);

/**
 * @brief 获取栈顶帧指针
 *
 * @param stack 推理栈指针（非 NULL）
 * @return 栈顶帧指针，栈为空时返回 NULL
 */
lvReasoningFrame *lv_reasoning_stack_top(lvReasoningStack *stack);

#ifdef __cplusplus
}
#endif

#endif /* lv_LV_REASONING_STACK_H */
