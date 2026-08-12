#ifndef lv_FILE_H
#define lv_FILE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/**
 * @file lv_file.h
 * @brief 统一文件 I/O 抽象层 —— 自动错误处理与日志
 *
 * 封装 fopen/fread/fwrite/fclose 模式，统一错误处理。
 */

/**
 * @brief 打开文件
 * @param path  文件路径
 * @param mode  打开模式（同 fopen）
 * @return      文件指针，失败返回 NULL 并记录错误日志
 */
FILE *lv_file_open(const char *path, const char *mode);

/**
 * @brief 安全关闭文件
 * @param fp    文件指针（可 NULL）
 * @return      0 成功，-1 失败
 */
int lv_file_close(FILE *fp);

/**
 * @brief 读取整个文件到堆缓冲区
 * @param path     文件路径
 * @param out_len  输出：读取的字节数
 * @return         堆分配的缓冲区（调用者 lv_free），失败返回 NULL
 */
uint8_t *lv_file_read_all(const char *path, size_t *out_len);

/**
 * @brief 读取整个文件到堆缓冲区（带大小上限校验）
 *
 * 与 lv_file_read_all 行为一致，额外增加 max_size 上限校验：
 * 文件大小超过上限时返回 NULL（防止无界分配）。
 *
 * @param path     文件路径
 * @param out_len  输出：读取的字节数
 * @param max_size 允许的最大文件大小（字节）；文件大小等于 max_size 时允许读取
 * @return         堆分配的缓冲区（调用者 lv_free），失败返回 NULL
 */
uint8_t *lv_file_read_all_limited(const char *path, size_t *out_len, size_t max_size);

/**
 * @brief 读取文本文件到调用方缓冲区（固定缓冲整读）
 * @param path     文件路径
 * @param buf      目标缓冲区
 * @param buf_size 缓冲区大小（必须 >= 2）
 * @return true 成功，false 失败（参数非法 / 打开失败）
 * @note 读取 min(文件大小, buf_size-1) 字节并保证 NUL 终止；
 *       收敛对象：lv_impl_upper_app.c / lv_impl_upper_orchestrator.c 孪生 read_file_text
 *       （fopen + fread(buf_size-1) + 手写 NUL 样板），不分配堆内存。
 */
bool lv_file_read_text(const char *path, char *buf, size_t buf_size);

/**
 * @brief 写入缓冲区到文件
 * @param path  文件路径
 * @param data  数据
 * @param len   数据长度
 * @return      0 成功，-1 失败
 */
int lv_file_write_all(const char *path, const void *data, size_t len);

/**
 * @brief 获取文件大小
 * @param fp  已打开的文件指针
 * @return    文件大小（字节），失败返回 0
 */
/**
 * @brief 检查文件是否存在
 * @param path 文件路径
 * @return true 存在，false 不存在（含 NULL 参数）
 */
bool lv_file_exists(const char *path);

size_t lv_file_size(FILE *fp);

#ifdef __cplusplus
}
#endif

#endif /* lv_FILE_H */
