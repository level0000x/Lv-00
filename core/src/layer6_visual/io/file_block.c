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

#include "lv/io_blocks.h"
#include "lv/lv_utils.h"
#include <string.h>
#include <stdio.h>

/** @brief 文件块内部状态结构 */
typedef struct {
    char *path;      /**< 文件路径 */
    bool is_open;    /**< 文件是否已打开（状态标记） */
} FileBlockState;

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
    if (!block) return NULL;
    block->effect = effect;
    block->path_port = -1;
    block->data_port = -1;
    block->result_port = -1;
    block->status_port = -1;

    FileBlockState *state = lv_calloc(1, sizeof(FileBlockState));
    if (!state) {
        lv_free((void **)&block);
        return NULL;
    }
    block->base = state;
    return block;
}

/**
 * @brief 销毁文件块
 *
 * 释放内部状态中的路径字符串、状态结构体和文件块本身。
 *
 * @param block 文件块指针
 */
void lv_file_block_destroy(lvFileBlock *block) {
    if (!block) return;
    if (block->base) {
        FileBlockState *state = (FileBlockState *)block->base;
        lv_free((void **)&state->path);
        lv_free((void **)&state);
    }
    lv_free((void **)&block);
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
    if (!block || !block->base || !path) return -1;
    FileBlockState *state = (FileBlockState *)block->base;
    lv_free((void **)&state->path);
    state->path = lv_strdup(path);
    if (!state->path) return -1;
    return 0;
}

/**
 * @brief 获取文件路径
 *
 * @param block 文件块指针（const）
 * @return 路径字符串，参数无效时返回NULL
 */
const char *lv_file_block_get_path(const lvFileBlock *block) {
    if (!block || !block->base) return NULL;
    FileBlockState *state = (FileBlockState *)block->base;
    return state->path;
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
int lv_file_block_read(lvFileBlock *block, void *buf, size_t buf_size,
                         size_t *bytes_read) {
    if (!block || !block->base || !buf || buf_size == 0) return -1;
    FileBlockState *state = (FileBlockState *)block->base;
    if (!state->path) return -1;

    FILE *f = fopen(state->path, "rb");
    if (!f) {
        if (bytes_read) *bytes_read = 0;
        return -1;
    }
    size_t n = fread(buf, 1, buf_size, f);
    fclose(f);

    if (bytes_read) *bytes_read = n;
    state->is_open = false;
    return (n > 0) ? 0 : -1;
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
int lv_file_block_write(lvFileBlock *block, const void *data,
                          size_t data_size) {
    if (!block || !block->base || !data) return -1;
    FileBlockState *state = (FileBlockState *)block->base;
    if (!state->path) return -1;

    FILE *f = fopen(state->path, "wb");
    if (!f) return -1;
    size_t n = fwrite(data, 1, data_size, f);
    fclose(f);

    state->is_open = false;
    return (n == data_size) ? 0 : -1;
}

/**
 * @brief 检查文件是否打开
 *
 * @param block 文件块指针（const）
 * @return 文件已打开返回true，否则返回false
 */
bool lv_file_block_is_open(const lvFileBlock *block) {
    if (!block || !block->base) return false;
    FileBlockState *state = (FileBlockState *)block->base;
    return state->is_open;
}
