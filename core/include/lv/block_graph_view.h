/**
 * @file block_graph_view.h
 * @brief 块图视图统一定义（layer6 视觉层）
 *
 * 收敛原先散落于 4 个源文件（converter/block_to_geometry.c、
 * converter/block_to_node.c、converter/block_to_text.c、
 * runtime/block_scheduler.c）的 BlockGraphView 重复定义，
 * 以及 2 处（converter/block_to_geometry.c、converter/block_to_text.c）
 * 字段略有漂移的 SimpleBlockGraph 重复定义。
 * 纯文本提升：结构与各本地定义保持逐字节一致，行为不变。
 */

#ifndef lv_BLOCK_GRAPH_VIEW_H
#define lv_BLOCK_GRAPH_VIEW_H

#include "lv/func_block.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 块图视图
 *
 * 轻量只读视图：FuncBlock 指针数组 + 数量。
 * 由 scheduler 与 converter 通过 void*（如 sched->graph / converter 入参）
 * 强制转换后共享使用。
 */
typedef struct {
    FuncBlock **blocks; /**< 函数块指针数组 */
    int count;          /**< 块数量 */
} BlockGraphView;

/**
 * @brief 简单块图
 *
 * 可增长的 FuncBlock 容器：解析（文本/几何）构建块图时使用。
 * 字段与 BlockGraphView 的差异为额外的 cap 容量字段，
 * 通过 lv_ensure_capacity 统一扩容。
 */
typedef struct {
    FuncBlock **blocks; /**< 函数块指针数组 */
    int count;          /**< 当前块数量 */
    int cap;            /**< 数组容量 */
} SimpleBlockGraph;

#ifdef __cplusplus
}
#endif

#endif /* lv_BLOCK_GRAPH_VIEW_H */
