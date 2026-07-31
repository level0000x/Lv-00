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

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief IO 块共享内部状态 */
typedef struct lvIOBlockState {
    char *target; /**< 目标路径（文件块）或目标 URL（网络块） */
    bool active;  /**< 打开/连接状态标记 */
} lvIOBlockState;

#ifdef __cplusplus
}
#endif

#endif /* lv_IO_BLOCK_H */
