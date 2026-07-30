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
size_t lv_file_size(FILE *fp);

#ifdef __cplusplus
}
#endif

#endif /* lv_FILE_H */
