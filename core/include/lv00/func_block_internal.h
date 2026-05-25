/**
 * @file func_block_internal.h
 * @brief 函数块模块内部共享头文件
 *
 * @details 此头文件供 func_block 模块的拆分编译单元之间共享内部声明。
 *          包括：
 *          - collect_all_block_ids 的非 static 声明（原为 static，拆分后需跨文件共享）
 *          - 流式上下文外部变量声明
 *          - 拆分模块间共享的常量和类型
 *
 * @warning 此文件仅供 func_block 模块内部使用，不属于公开 API。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */
#ifndef LV00_FUNC_BLOCK_INTERNAL_H
#define LV00_FUNC_BLOCK_INTERNAL_H

/* 首先包含 lv00.h 以获取 LV00_THREAD_LOCAL 宏定义 */
#include "func_block.h"
#include "lv00.h"
#include "stream.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 共享常量 ==================== */

/** 确定性静态检查的默认步数上限（设计文档 8.2 节指定） */
#define LV00_DEFAULT_DETERMINISM_STEP_LIMIT 100

/** 序列化缓冲区的初始大小 */
#define LV00_SERIALIZE_BUFFER_INITIAL_SIZE 1024

/* ==================== 共享类型 ==================== */

/**
 * @brief 确定性静态检查的共享约束统计结果
 *
 * 封装两个静态检查函数的公共输出：约束类型计数与自由度分析。
 * 消除 func_block_check_determinism_static 与
 *     func_block_determinism_check_static 之间的重复逻辑。
 */
typedef struct {
    int linear_count;     /**< 线性约束计数（INCIDENCE / BETWEENNESS / CONTAINMENT） */
    int quadratic_count;  /**< 二次约束计数（INTERSECTION） */
    int connection_count; /**< 连接约束计数（CONNECTION，不影响确定性） */
    int steps;            /**< 实际执行的检查步数 */
    int free_dof;         /**< 自由度数（仅在线性系统中有效） */
    int total_dof;        /**< 总自由度数 */
} DeterminismStaticStats;

/* ==================== 共享函数声明 ==================== */

/**
 * @brief 收集函数块所有相关节点ID（内部节点 + 输入端口 + 输出端口）
 *
 * 此函数是多个操作的共享基础逻辑，被以下函数调用：
 *   - determinism_collect_constraint_stats（确定性检查）
 *   - func_block_check_determinism_dynamic（废弃版动态检查）
 *   - func_block_determinism_check_dynamic（增强版动态检查）
 *   - instantiate_copy_constraints（约束复制）
 *   - instantiate_copy_connection_constraints（CONNECTION约束复制）
 *
 * @param fb           函数块
 * @param out_ids      输出数组（调用者负责 lv00_free）
 * @param out_count    输出数量
 * @return true 成功，false 失败或无节点
 */
bool collect_all_block_ids(const FuncBlock *fb, int **out_ids, int *out_count);

/**
 * @brief 收集节点ID列表并统计涉及内部节点的约束类型
 *
 * 这是两个静态确定性检查函数的共享核心逻辑：
 * 1. 收集内部节点和端口的ID列表
 * 2. 遍历约束图，分类统计涉及内部节点的约束
 * 3. 对于纯线性系统，计算自由度
 *
 * @param fb           函数块（提供内部节点/端口ID）
 * @param graph        约束图
 * @param step_limit   步数上限（0 表示不限制）
 * @param stats        输出参数：填充统计结果
 * @return 分配的 all_ids 数组（调用者负责释放），失败返回 NULL
 */
int *determinism_collect_constraint_stats(const FuncBlock *fb, const ConstraintGraph *graph, int step_limit,
                                          DeterminismStaticStats *stats);

/**
 * @brief 根据线性系统的自由度分析结果判定确定性
 *
 * @param free_dof 自由度数
 * @return -1 过约束（无解），0 恰好约束（唯一解），1 欠约束（多解）
 */
int determinism_evaluate_linear_dof(int free_dof);

/**
 * @brief 清理求解器结果（GroebnerResult）
 *
 * 两个静态检查函数都需要在调用求解器后清理结果，
 * 提取为公共函数以避免重复的释放逻辑。
 */
void determinism_cleanup_groebner(void *gresult);

/* ==================== 流式上下文外部声明 ==================== */

/**
 * @brief 函数块模块的流式上下文指针（定义在 func_block.c 中）
 *
 * 其他拆分编译单元通过 extern 声明访问此变量。
 * 设置此变量应通过 func_block_set_stream_context() 函数。
 */
extern LV00_THREAD_LOCAL StreamContext *func_block_stream_ctx;

#ifdef __cplusplus
}
#endif

#endif /* LV00_FUNC_BLOCK_INTERNAL_H */
