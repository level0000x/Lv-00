/*
 * @file prop_verifier_context.c
 * @brief Proposition verifier module - proof context
 * @details Split from prop_verifier.c
 */

#include "lv/prop_verifier.h"
#include "prop_verifier_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/stream.h"
#include "lv/stream_context_util.h"

/* ============================================================
 * 证明用上下文 - 内部数据结构
 * ============================================================ */

/**
 * @brief 获取墙上时钟时间（毫秒）
 *
 * 使用 C 标准 time() 获取墙上时钟时间，而非 clock() 获取处理器时间。
 * clock() 在多线程或 I/O 等待场景下不准确（仅计 CPU 时间而非实时时间）。
 * 返回值仅用于计算超时时差，绝对值无意义。
 *
 * @return 当前时间的毫秒级数值
 */
#include <time.h>
#include "lv/lv_strbuf.h"
uint64_t get_time_ms(void) {
    return (uint64_t) time(NULL) * PROP_TIME_MS_PER_SEC;
}

