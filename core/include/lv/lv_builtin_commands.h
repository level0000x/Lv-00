/**
 * @file lv_builtin_commands.h
 * @brief 内置终端命令共享列表
 *
 * @details 供 lv_protocol.c（lv_proto_completions）与
 *          interop_theorem.c（interop_get_command_completions）
 *          共用同一份内置命令补全列表，避免重复维护。
 *
 * @author Lv-00 Project
 */

#ifndef lv_BUILTIN_COMMANDS_H
#define lv_BUILTIN_COMMANDS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 内置命令补全列表（NULL 结尾），定义于 lv_protocol.c */
extern const char *const lv_builtin_commands[];

/** @brief 内置命令总数（不含 NULL 结尾符） */
extern const size_t lv_builtin_command_count;

#ifdef __cplusplus
}
#endif

#endif /* lv_BUILTIN_COMMANDS_H */
