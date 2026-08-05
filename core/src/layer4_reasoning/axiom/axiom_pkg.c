/**
 * @file axiom_pkg.c
 * @brief 公理系统包实现
 * @details 实现公理包的加载、验证和展开功能。支持约束模板、
 *          不可构造问题检测、双层测试和 SHA-256 依赖追踪。
 */

#include "axiom_pkg.h"
#include "axiom_pkg_internal.h"

#include "lv/lv_file.h"

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "lv/sha256.h"


#include "debug.h"
#include "error_codes.h"
#include "lexer_shared.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "stream.h"


/* 线程局部存储用于错误消息（使用lv_internal.h中定义的lv_THREAD_LOCAL） */

lv_THREAD_LOCAL StreamContext *axiom_stream_ctx = NULL;

void axiom_pkg_set_stream_context(StreamContext *ctx) {
    axiom_stream_ctx = ctx;
}

/* ============== 辅助函数 ============== */

const char *axiom_package_get_last_error(void) {
    return lv_get_last_error_message();
}

char *safe_lv_strdup_safe(const char *s) {
    if (!s)
        return NULL;
    size_t len = strlen(s);
    char *dup = lv_malloc(len + 1);
    if (dup) {
        memcpy(dup, s, len + 1); /* 使用 memcpy 替代 strcpy，确保安全 */
    }
    return dup;
}

