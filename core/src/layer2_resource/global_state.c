/**
 * @file global_state.c
 * @brief Lv-00 全局状态管理
 *
 * 提供全局配置参数的集中存储和访问接口。
 * 支持整数、浮点数、字符串、布尔值四种参数类型。
 *
 * 注意：核心配置管理（lv_config_* 系列 API）在 lv_utils.c 中实现。
 * 本模块提供独立的、轻量级的全局状态存储，用于不需要完整配置管理器的场景。
 *
 * @version 3.3.0
 * @author Lv-00 Team
 * @license MIT
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <errno.h>

#include "lv/lv_utils.h"

/* ── 线程安全 ─────────────────────────────────────────────────────── */

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#define GS_LOCK()   EnterCriticalSection(&g_state_mutex)
#define GS_UNLOCK() LeaveCriticalSection(&g_state_mutex)
#define GS_INIT_LOCK() InitializeCriticalSection(&g_state_mutex)
#define GS_DESTROY_LOCK() DeleteCriticalSection(&g_state_mutex)
static CRITICAL_SECTION g_state_mutex;
#else
#include <pthread.h>
#define GS_LOCK()   pthread_mutex_lock(&g_state_mutex)
#define GS_UNLOCK() pthread_mutex_unlock(&g_state_mutex)
#define GS_INIT_LOCK() pthread_mutex_init(&g_state_mutex, NULL)
#define GS_DESTROY_LOCK() pthread_mutex_destroy(&g_state_mutex)
static pthread_mutex_t g_state_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

/* ── 常量 ─────────────────────────────────────────────────────────── */

#define lv_GS_MAX_PARAMS       256
#define lv_GS_MAX_KEY_LEN       128
#define lv_GS_MAX_VALUE_LEN     512
#define lv_GS_MAX_ERROR_LEN     256

/* ── 参数类型 ─────────────────────────────────────────────────────── */

typedef enum {
    lv_GS_TYPE_INT    = 0,
    lv_GS_TYPE_DOUBLE = 1,
    lv_GS_TYPE_STRING = 2,
    lv_GS_TYPE_BOOL   = 3
} lvGsParamType;

/* ── 参数条目 ─────────────────────────────────────────────────────── */

typedef struct {
    char              key[lv_GS_MAX_KEY_LEN];
    lvGsParamType  type;
    union {
        int    int_val;
        double dbl_val;
        char   str_val[lv_GS_MAX_VALUE_LEN];
        bool   bool_val;
    } value;
    bool              is_set;
} lvGsParam;

/* ── 全局状态结构体 ─────────────────────────────────────────────────── */

typedef struct {
    lvGsParam  params[lv_GS_MAX_PARAMS];
    int          param_count;
    bool         initialized;
    char         version[64];
} lvGlobalState;

static lvGlobalState g_state = {0};

/* ── 内部辅助 ─────────────────────────────────────────────────────── */

/* 注意：调用此函数前必须持有 GS_LOCK() */
static int find_param_index(const char *key) {
    if (!key) return -1;
    for (int i = 0; i < g_state.param_count; i++) {
        if (strcmp(g_state.params[i].key, key) == 0) return i;
    }
    return -1;
}

/* 注意：调用此函数前必须持有 GS_LOCK() */
static int find_or_create_param(const char *key, lvGsParamType type) {
    if (!key) return -1;
    int idx = find_param_index(key);
    if (idx >= 0) return idx;
    if (g_state.param_count >= lv_GS_MAX_PARAMS) return -1;
    idx = g_state.param_count++;
    memset(&g_state.params[idx], 0, sizeof(lvGsParam));
    strncpy(g_state.params[idx].key, key, lv_GS_MAX_KEY_LEN - 1);
    g_state.params[idx].type = type;
    return idx;
}

/* ── 公共 API ──────────────────────────────────────────────────────── */

int lv_global_state_init(void) {
    GS_LOCK();
    if (g_state.initialized) {
        GS_UNLOCK();
        return 0;
    }
    memset(&g_state, 0, sizeof(lvGlobalState));
    g_state.param_count = 0;
    g_state.initialized = true;
    strncpy(g_state.version, "3.3.0", sizeof(g_state.version) - 1);
    GS_UNLOCK();
    return 0;
}

void lv_global_state_cleanup(void) {
    if (!g_state.initialized) return;
    GS_LOCK();
    memset(&g_state, 0, sizeof(lvGlobalState));
    GS_UNLOCK();
    GS_DESTROY_LOCK();
}

bool lv_global_state_is_initialized(void) {
    return g_state.initialized;
}

/* ── 整型参数 ──────────────────────────────────────────────────────── */

int lv_global_state_set_int(const char *key, int value) {
    if (!g_state.initialized) return -1;
    GS_LOCK();
    int idx = find_or_create_param(key, lv_GS_TYPE_INT);
    if (idx < 0) { GS_UNLOCK(); return -1; }
    g_state.params[idx].value.int_val = value;
    g_state.params[idx].is_set = true;
    GS_UNLOCK();
    return 0;
}

int lv_global_state_get_int(const char *key, int default_val) {
    if (!g_state.initialized) return default_val;
    GS_LOCK();
    int idx = find_param_index(key);
    if (idx < 0 || !g_state.params[idx].is_set) { GS_UNLOCK(); return default_val; }
    if (g_state.params[idx].type != lv_GS_TYPE_INT) { GS_UNLOCK(); return default_val; }
    int ret = g_state.params[idx].value.int_val;
    GS_UNLOCK();
    return ret;
}

/* ── 浮点参数 ──────────────────────────────────────────────────────── */

int lv_global_state_set_double(const char *key, double value) {
    if (!g_state.initialized) return -1;
    GS_LOCK();
    int idx = find_or_create_param(key, lv_GS_TYPE_DOUBLE);
    if (idx < 0) { GS_UNLOCK(); return -1; }
    g_state.params[idx].value.dbl_val = value;
    g_state.params[idx].is_set = true;
    GS_UNLOCK();
    return 0;
}

double lv_global_state_get_double(const char *key, double default_val) {
    if (!g_state.initialized) return default_val;
    GS_LOCK();
    int idx = find_param_index(key);
    if (idx < 0 || !g_state.params[idx].is_set) { GS_UNLOCK(); return default_val; }
    if (g_state.params[idx].type != lv_GS_TYPE_DOUBLE) { GS_UNLOCK(); return default_val; }
    double ret = g_state.params[idx].value.dbl_val;
    GS_UNLOCK();
    return ret;
}

/* ── 字符串参数 ────────────────────────────────────────────────────── */

int lv_global_state_set_string(const char *key, const char *value) {
    if (!g_state.initialized) return -1;
    GS_LOCK();
    int idx = find_or_create_param(key, lv_GS_TYPE_STRING);
    if (idx < 0) { GS_UNLOCK(); return -1; }
    if (value) {
        strncpy(g_state.params[idx].value.str_val, value, lv_GS_MAX_VALUE_LEN - 1);
    } else {
        g_state.params[idx].value.str_val[0] = '\0';
    }
    g_state.params[idx].is_set = true;
    GS_UNLOCK();
    return 0;
}

const char *lv_global_state_get_string(const char *key, const char *default_val) {
    if (!g_state.initialized) return default_val;
    GS_LOCK();
    int idx = find_param_index(key);
    if (idx < 0 || !g_state.params[idx].is_set) { GS_UNLOCK(); return default_val; }
    if (g_state.params[idx].type != lv_GS_TYPE_STRING) { GS_UNLOCK(); return default_val; }
    const char *ret = g_state.params[idx].value.str_val;
    GS_UNLOCK();
    return ret;
}

/* ── 布尔参数 ──────────────────────────────────────────────────────── */

int lv_global_state_set_bool(const char *key, bool value) {
    if (!g_state.initialized) return -1;
    GS_LOCK();
    int idx = find_or_create_param(key, lv_GS_TYPE_BOOL);
    if (idx < 0) { GS_UNLOCK(); return -1; }
    g_state.params[idx].value.bool_val = value;
    g_state.params[idx].is_set = true;
    GS_UNLOCK();
    return 0;
}

bool lv_global_state_get_bool(const char *key, bool default_val) {
    if (!g_state.initialized) return default_val;
    GS_LOCK();
    int idx = find_param_index(key);
    if (idx < 0 || !g_state.params[idx].is_set) { GS_UNLOCK(); return default_val; }
    if (g_state.params[idx].type != lv_GS_TYPE_BOOL) { GS_UNLOCK(); return default_val; }
    bool ret = g_state.params[idx].value.bool_val;
    GS_UNLOCK();
    return ret;
}

/* ── 重置 ────────────────────────────────────────────────────────────── */

int lv_global_state_reset(void) {
    if (!g_state.initialized) return -1;
    GS_LOCK();
    g_state.param_count = 0;
    GS_UNLOCK();
    return 0;
}

/* ── 版本 ────────────────────────────────────────────────────────────── */

const char *lv_global_state_get_version(void) {
    return g_state.version;
}
