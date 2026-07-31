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


/* 兼容性宏：PropositionKind 短名 */
#ifndef CONSTRUCTIVE
#define CONSTRUCTIVE PROPOSITION_KIND_CONSTRUCTIVE
#endif
#ifndef NON_CONSTRUCTIVE_ORACLE
#define NON_CONSTRUCTIVE_ORACLE PROPOSITION_KIND_NON_CONSTRUCTIVE_ORACLE
#endif
#ifndef EXPLOSION_PRINCIPLE
#define EXPLOSION_PRINCIPLE PROPOSITION_KIND_EXPLOSION_PRINCIPLE
#endif

/* 线程局部存储用于错误消息（使用lv_internal.h中定义的lv_THREAD_LOCAL） */

lv_THREAD_LOCAL StreamContext *axiom_stream_ctx = NULL;

void axiom_pkg_set_stream_context(StreamContext *ctx) {
    axiom_stream_ctx = ctx;
}

/** SHA-256 输出大小（字节） */
#define AXIOM_SHA256_OUTPUT_SIZE 32

/** SHA-256 哈希十六进制字符串大小（64字符 + 空终止符） */
#define AXIOM_SHA256_HEX_SIZE 65

/** 展开缓存的默认初始容量 */
#define AXIOM_EXPANSION_CACHE_CAP 16

/** 依赖引用缓存的默认初始容量 */
#define AXIOM_DEP_REF_CACHE_CAP 16

/** 最大递归展开深度 */
#define AXIOM_MAX_EXPANSION_DEPTH 8

/** 最大公理源文件大小 (64 MB) */
#define AXIOM_MAX_FILE_SIZE (64 * 1024 * 1024)

/** 规范形式最大参与者类型数量 */
#define AXIOM_MAX_PARTICIPANT_TYPES 8

/** 参与者类型名称的最大长度 */
#define AXIOM_PARTICIPANT_TYPE_LEN 32

/** 测试失败消息缓冲区大小 */
#define AXIOM_TEST_MSG_BUF_SIZE 256

/** 模板参数描述格式字符串最大长度 */
#define AXIOM_PARAM_DESC_MAX_LEN 64

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

