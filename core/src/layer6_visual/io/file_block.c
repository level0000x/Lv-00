/**
 * @file file_block.c
 * @brief 文件块实现
 *
 * @details 实现文件 I/O 块的创建、销毁、路径管理和文件读写操作。
 *          文件块支持二进制文件的读取和写入，通过路径端口和数据端口
 *          与数据流系统集成。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>
#include <string.h>

#include "lv/lv_file.h"

#include "lv/io_block.h"
#include "lv/io_blocks.h"
#include "lv/lv_utils.h"
#include "lv/lv_internal.h"


/**
 * @brief 创建文件块
 *
 * 分配并初始化一个文件块，包含内部状态管理。
 * 各端口初始设为 -1（未分配）。
 *
 * @param effect 效果类型
 * @return 成功返回文件块指针，失败返回NULL
 */
lvFileBlock *lv_file_block_create(lvEffectType effect) {
    lvFileBlock *block = lv_calloc(1, sizeof(lvFileBlock));
    if (!block)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "failed to allocate file block");
    block->effect = effect;
    block->path_port = -1;
    block->data_port = -1;
    block->result_port = -1;
    block->status_port = -1;

    lvIOBlockState *state = lv_calloc(1, sizeof(lvIOBlockState));
    if (!state) {
        lv_free((void **) &block);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "failed to allocate file block state");
    }
    block->base = state;
    return block;
}

/**
 * @brief 销毁 IO 块共享内部状态（共享释放函数实现，声明见 io_block.h）
 *
 * 释放 target 字符串与状态结构体本身，供 lv_file_block_destroy 与
 * lv_network_block_destroy 复用（判据 G 收敛同构释放样板）。
 *
 * @param state IO 块共享内部状态指针（可为 NULL）
 */
void lv_io_block_state_destroy(lvIOBlockState *state) {
    if (!state)
        return;
    lv_free((void **) &state->target);
    lv_free((void **) &state);
}

/**
 * @brief 销毁文件块
 *
 * 释放内部状态中的路径字符串、状态结构体和文件块本身。
 *
 * @param block 文件块指针
 */
void lv_file_block_destroy(lvFileBlock *block) {
    if (!block)
        return;
    lv_io_block_state_destroy((lvIOBlockState *) block->base);
    lv_free((void **) &block);
}

/**
 * @brief 设置文件路径
 *
 * 设置文件块的目标路径，自动释放旧路径并复制新字符串。
 *
 * @param block 文件块指针
 * @param path  文件路径字符串
 * @return 成功返回0，失败返回-1
 */
int lv_file_block_set_path(lvFileBlock *block, const char *path) {
    if (!block || !block->base || !path)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "NULL block, base, or path");
    lvIOBlockState *state = (lvIOBlockState *) block->base;
    lv_free((void **) &state->target);
    state->target = lv_strdup(path);
    if (!state->target)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "failed to strdup path");
    return 0;
}

/**
 * @brief 获取文件路径
 *
 * @param block 文件块指针（const）
 * @return 路径字符串，参数无效时返回NULL
 */
const char *lv_file_block_get_path(const lvFileBlock *block) {
    if (!block || !block->base)
        return NULL;
    lvIOBlockState *state = (lvIOBlockState *) block->base;
    return state->target;
}

/**
 * @brief 读取文件
 *
 * 以二进制模式打开文件并读取内容到指定缓冲区。
 *
 * @param block       文件块指针
 * @param buf         读取缓冲区
 * @param buf_size    缓冲区大小
 * @param bytes_read  输出实际读取字节数（可为NULL）
 * @return 成功返回0，失败返回-1
 */
int lv_file_block_read(lvFileBlock *block, void *buf, size_t buf_size, size_t *bytes_read) {
    if (!block || !block->base || !buf || buf_size == 0)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "NULL block, base, or buf, or zero size");
    lvIOBlockState *state = (lvIOBlockState *) block->base;
    if (!state->target)
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "file path not set");

    FILE *f = lv_file_open(state->target, "rb");
    if (!f) {
        if (bytes_read)
            *bytes_read = 0;
        lv_RETURN_ERROR(lv_ERROR_IO, "failed to open file for reading");
    }
    size_t n = fread(buf, 1, buf_size, f);
    lv_file_close(f);

    if (bytes_read)
        *bytes_read = n;
    state->active = false;
    if (n > 0)
        return 0;
    lv_RETURN_ERROR(lv_ERROR_IO, "no bytes read from file");
}

/**
 * @brief 写入文件
 *
 * 以二进制模式打开文件并将数据写入。
 *
 * @param block     文件块指针
 * @param data      待写入数据缓冲区
 * @param data_size 数据大小
 * @return 成功返回0，失败返回-1
 */
int lv_file_block_write(lvFileBlock *block, const void *data, size_t data_size) {
    if (!block || !block->base || !data)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "NULL block, base, or data");
    lvIOBlockState *state = (lvIOBlockState *) block->base;
    if (!state->target)
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "file path not set for write");

    FILE *f = lv_file_open(state->target, "wb");
    if (!f)
        lv_RETURN_ERROR(lv_ERROR_IO, "failed to open file for writing");
    size_t n = fwrite(data, 1, data_size, f);
    lv_file_close(f);

    state->active = false;
    if (n == data_size)
        return 0;
    lv_RETURN_ERROR(lv_ERROR_IO, "incomplete file write");
}

/**
 * @brief 检查文件是否打开
 *
 * @param block 文件块指针（const）
 * @return 文件已打开返回true，否则返回false
 */
bool lv_file_block_is_open(const lvFileBlock *block) {
    if (!block || !block->base)
        return false;
    lvIOBlockState *state = (lvIOBlockState *) block->base;
    return state->active;
}
