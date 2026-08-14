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

#include "lv/constraint_graph.h"

#include "lv/axiom_pkg.h"
#include "lv/error_codes.h"
#include "lv/lexer_shared.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/module.h"
#include "lv/stream.h"
#include "lv/stream.h" /* LV_STREAM_CTX_DEFINE */
#include "lv/symbolic_coord.h"

/* ============== 模块实例结构体定义 ============== */
#include "lv/module_internal.h"

LV_STREAM_CTX_DEFINE(module);

/* FNV-1a 哈希常量已在 lv_internal.h 中统一定义为 lv_FNV64_OFFSET_BASIS / lv_FNV64_PRIME；
 * 移除重复的 #ifndef 回退定义，直接使用统一定义。 */

/* LVZ 格式版本 */
#define LVZ_VERSION_MAJOR 1
#define LVZ_VERSION_MINOR 0

/* ============== 辅助函数 ============== */

const char *module_get_last_error(void) {
    return lv_get_last_error_message();
}

/* safe_strdup 已移除 —— 统一使用 lv_utils.h 中的 lv_strdup_safe */

/* ============== 属性访问器实现 ============== */

const char *module_get_name(const Module *mod) {
    return mod ? mod->name : NULL;
}

const char *module_get_version(const Module *mod) {
    return mod ? mod->version : NULL;
}

int module_get_dependency_count(const Module *mod) {
    return mod ? mod->dependencies.count : 0;
}

int module_get_axiom_package_count(const Module *mod) {
    return mod ? mod->axiom_packages.count : 0;
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
