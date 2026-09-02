/**
 * @file io_block.h
 * @brief IO 块共享内部状态
 *
 * @details 文件块（file_block.c）与网络块（network_block.c）共用
 *          的内部状态结构。target 存放目标路径（文件块）或目标
 *          URL（网络块），active 标记文件是否打开 / 连接是否建立。
 *
 * @author Lv-00 Project
 */

#ifndef lv_IO_BLOCK_H
#define lv_IO_BLOCK_H

#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief IO 块共享内部状态 */
typedef struct lvIOBlockState {
    char *target; /**< 目标路径（文件块）或目标 URL（网络块） */
    bool active;  /**< 打开/连接状态标记 */
} lvIOBlockState;

/**
 * @brief 销毁 IO 块共享内部状态（共享释放函数）
 *
 * 释放 target 字符串与状态结构体本身。收敛 file_block.c / network_block.c
 * 中同构的「释放 state->target + 释放 state 外壳」释放样板（判据 G）。
 * NULL 安全（state 为 NULL 时空操作），lv_free 内部跳过 NULL 指针。
 *
 * @param state IO 块共享内部状态指针（可为 NULL）
 */
lv_PUBLIC_API void lv_io_block_state_destroy(lvIOBlockState *state);

#ifdef __cplusplus
}
#endif

#endif /* lv_IO_BLOCK_H */
