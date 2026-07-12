/**
 * @file module.c
 * @brief 模块系统实现
 * @details 实现模块的加载、保存和依赖管理。支持 MessagePack 序列化、
 *          自动保存、崩溃恢复和增量快照功能。
 */

#include <ctype.h>
#ifdef _WIN32
    #include <windows.h>
    /* Windows 下使用 FindFirstFile/FindNextFile 替代 POSIX dirent */
    #include <fileapi.h>
#else
    #include <dirent.h>
    #include <unistd.h>
#endif
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "axiom_pkg.h"
#include "lv00/constraint_graph.h"
#include "lexer_shared.h"
#include "error_codes.h"
#include "lv00_internal.h"
#include "lv00_utils.h"
#include "module.h"
#include "stream.h"
#include "symbolic_coord.h"

/* ============== 模块实例结构体定义 ============== */
#include "lv00/module_internal.h"

LV00_THREAD_LOCAL StreamContext *module_stream_ctx = NULL;

void module_set_stream_context(StreamContext *ctx) {
    module_stream_ctx = ctx;
}

/* FNV-1a 哈希常量已在 lv00_internal.h 中统一定义为 LV00_FNV64_OFFSET_BASIS / LV00_FNV64_PRIME；
 * 移除重复的 #ifndef 回退定义，直接使用统一定义。 */

/* LVZ 格式版本 */
#define LVZ_VERSION_MAJOR 1
#define LVZ_VERSION_MINOR 0

/* ============== 辅助函数 ============== */

const char *module_get_last_error(void) {
    return lv00_get_last_error_message();
}

/* safe_strdup 已移除 —— 统一使用 lv00_utils.h 中的 lv00_strdup_safe */

/* ============== 属性访问器实现 ============== */

const char *module_get_name(const Module *mod) {
    return mod ? mod->name : NULL;
}

const char *module_get_version(const Module *mod) {
    return mod ? mod->version : NULL;
}

int module_get_dependency_count(const Module *mod) {
    return mod ? mod->dependency_count : 0;
}

int module_get_axiom_package_count(const Module *mod) {
    return mod ? mod->axiom_package_count : 0;
}

const ConstraintGraph *module_get_graph(const Module *mod) {
    return mod ? mod->graph : NULL;
}

void module_set_graph(Module *mod, ConstraintGraph *graph) {
    if (mod) {
        mod->graph = graph;
    }
}

/* ============== 词法分析器 (Lexer) ============== */
/* 类型定义已提取至 module/module_helpers.h */
#include "module/module_helpers.h"

/* ── 子模块已拆分至 module/ ── */
