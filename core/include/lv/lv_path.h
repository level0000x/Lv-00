/**
 * @file lv_path.h
 * @brief 统一路径工具族 —— 拼接、basename/dirname、去扩展名、逐级建目录
 *
 * 收敛各模块手写的路径拼接/解析代码（#ifdef 双平台分隔符、strrchr 双分隔符、
 * 手写逐级 mkdir 等）。路径分隔符统一使用 config.h 的 lv_PATH_SEPARATOR
 * （唯一权威来源，lv.h 中的 lv_PATH_SEPARATOR_CHAR/STR 与之保持同源）。
 */
#ifndef lv_PATH_H
#define lv_PATH_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 拼接目录与文件名（自动使用平台分隔符）
 * @param dir      目录路径
 * @param file     文件名
 * @param out      输出缓冲区
 * @param out_size 输出缓冲区大小（字节）
 * @return true 成功（结果完整写入 out），false 参数无效或缓冲区不足
 * @note 无条件在 dir 与 file 之间插入分隔符（与历史手写拼接行为一致）；
 *       缓冲区不足时返回 false。
 */
bool lv_path_join(const char *dir, const char *file, char *out, size_t out_size);

/**
 * @brief 提取路径中的文件名部分（最后一个分隔符之后）
 * @param path 路径
 * @return 指向 path 内文件名部分的视图指针（无分隔符时为 path 本身），
 *         path 为 NULL 时返回 NULL
 * @note 返回输入字符串内的视图，不分配内存；同时识别 '/' 与 '\\'
 */
const char *lv_path_basename(const char *path);

/**
 * @brief 提取路径中的目录部分
 * @param path    路径
 * @param out_len 输出：目录部分长度（不含分隔符）
 * @return 无分隔符时返回 NULL（*out_len 置 0）；有分隔符时返回 path，
 *         目录部分即 path[0 .. *out_len)（分隔符本身不包含在内）
 */
const char *lv_path_dirname(const char *path, size_t *out_len);

/**
 * @brief 就地去掉最后一个扩展名（最后一个 '.' 及之后的内容）
 * @param path 路径（原地修改）
 * @return path 本身；path 为 NULL 时返回 NULL
 * @note 无 '.' 时不变
 */
char *lv_path_strip_ext(char *path);

/**
 * @brief 逐级创建目录（多级路径一次性创建）
 * @param path 要创建的目录路径
 * @return 0 成功，-1 失败
 * @note 已存在的目录视为成功（EEXIST 不报错）；
 *       lv_strlcpy 截断语义：超出内部缓冲区（lv_PATH_BUF_SIZE）的路径截断处理
 */
int lv_path_mkdirs(const char *path);

/**
 * @brief 删除文件或目录（目录递归删除）
 * @param path 要删除的文件/目录路径
 * @return 0 成功（含路径不存在，视为成功）；-1 参数无效或删除失败
 * @note 收敛 test_proof_version.c 等处 #ifdef _WIN32 system("rmdir /s /q")
 *       / #else system("rm -rf") 双平台镜像样板；目录先递归删除子项再
 *       移除自身，行为与 rm -rf / rmdir /s /q 一致。
 */
int lv_path_remove(const char *path);

/**
 * @brief 目录遍历回调
 * @param ctx  透传上下文
 * @param name 目录项名称（不含路径，已跳过 "." 与 ".."）
 * @return true 继续遍历；false 中止遍历
 */
typedef bool (*lv_dir_entry_fn)(void *ctx, const char *name);

/**
 * @brief 遍历目录条目（自动跳过 "." 与 ".."）
 * @param dir 目录路径
 * @param fn  回调（不可为 NULL）
 * @param ctx 透传上下文
 * @return 0 成功（含目录不存在或为空，与历史手写遍历语义一致）；-1 参数无效
 * @note 收敛 plugin_system_autoload.c 中 FindFirstFileA / opendir 双平台
 *       手写镜像样板；条目过滤（如按扩展名）由调用方在回调中完成。
 */
int lv_dir_foreach(const char *dir, lv_dir_entry_fn fn, void *ctx);

/**
 * @brief 生成唯一的临时文件路径（替换 tmpnam；不创建文件）
 * @param out      输出缓冲区
 * @param out_size 缓冲区大小（字节）
 * @return true 成功；false 参数无效或缓冲区不足
 * @note 目录取系统临时目录（Windows: GetTempPath；POSIX: TMPDIR 或 /tmp），
 *       文件名由 PID + 时间戳 + 计数器保证进程内唯一。
 */
bool lv_temp_path(char *out, size_t out_size);

/**
 * @brief 获取用户主目录（跨平台，环境变量空值统一处理）
 * @return 主目录路径字符串（始终非 NULL）
 * @note 收敛 debug_state.c 手写 get_home_dir 的 #ifdef 双平台样板与
 *       lv_temp_path 中"env 空值回落默认"的重复模式。
 *       Windows：USERPROFILE，缺失回落 HOMEDRIVE+HOMEPATH，均缺失返回空串；
 *       POSIX：HOME，缺失或为空回落 "/tmp"。
 *       返回指向内部静态缓冲区（Windows）或环境变量视图（POSIX）的指针，
 *       调用方不得修改，且 Windows 路径不得跨线程使用。
 */
const char *lv_path_home_dir(void);

#ifdef __cplusplus
}
#endif

#endif /* lv_PATH_H */
