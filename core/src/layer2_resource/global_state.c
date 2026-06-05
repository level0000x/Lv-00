/**
 * @file global_state.c
 * @brief Lv-00 全局状态管理
 *
 * 提供全局配置参数的集中存储和访问接口。
 * 支持整数、浮点数、字符串、布尔值四种参数类型。
 *
 * 注意：核心配置管理（lv00_config_* 系列 API）在 lv00_utils.c 中实现。
 * 本模块提供独立的、轻量级的全局状态存储，用于不需要完整配置管理器的场景。
 *
 * @version 3.3.0
 * @author Lv-00 Team
 * @license MIT
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <errno.h>

#include "lv00/lv00_utils.h"

/* ── 常量 ─────────────────────────────────────────────────────────── */

#define LV00_GS_MAX_PARAMS       256
#define LV00_GS_MAX_KEY_LEN       128
#define LV00_GS_MAX_VALUE_LEN     512
#define LV00_GS_MAX_ERROR_LEN     256

/* ── 参数类型 ─────────────────────────────────────────────────────── */

typedef enum {
    LV00_GS_TYPE_INT    = 0,
    LV00_GS_TYPE_DOUBLE = 1,
    LV00_GS_TYPE_STRING = 2,
    LV00_GS_TYPE_BOOL   = 3
} Lv00GsParamType;

/* ── 参数条目 ─────────────────────────────────────────────────────── */

typedef struct {
    char              key[LV00_GS_MAX_KEY_LEN];
    Lv00GsParamType  type;
    union {
        int    int_val;
        double dbl_val;
        char   str_val[LV00_GS_MAX_VALUE_LEN];
        bool   bool_val;
    } value;
    bool              is_set;
} Lv00GsParam;

/* ── 全局状态结构体 ─────────────────────────────────────────────────── */

typedef struct {
    Lv00GsParam  params[LV00_GS_MAX_PARAMS];
    int          param_count;
    bool         initialized;
    char         version[64];
} Lv00GlobalState;

static Lv00GlobalState g_state = {0};

/* ── 内部辅助 ─────────────────────────────────────────────────────── */

static int find_param_index(const char *key) {
    if (!key) return -1;
    for (int i = 0; i < g_state.param_count; i++) {
        if (strcmp(g_state.params[i].key, key) == 0) return i;
    }
    return -1;
}

static int find_or_create_param(const char *key, Lv00GsParamType type) {
    if (!key) return -1;
    int idx = find_param_index(key);
    if (idx >= 0) return idx;
    if (g_state.param_count >= LV00_GS_MAX_PARAMS) return -1;
    idx = g_state.param_count++;
    memset(&g_state.params[idx], 0, sizeof(Lv00GsParam));
    strncpy(g_state.params[idx].key, key, LV00_GS_MAX_KEY_LEN - 1);
    g_state.params[idx].type = type;
    return idx;
}

/* ── 公共 API ──────────────────────────────────────────────────────── */

int lv00_global_state_init(void) {
    if (g_state.initialized) return 0;
    memset(&g_state, 0, sizeof(Lv00GlobalState));
    g_state.param_count = 0;
    g_state.initialized = true;
    strncpy(g_state.version, "3.3.0", sizeof(g_state.version) - 1);
    return 0;
}

void lv00_global_state_cleanup(void) {
    if (!g_state.initialized) return;
    memset(&g_state, 0, sizeof(Lv00GlobalState));
}

bool lv00_global_state_is_initialized(void) {
    return g_state.initialized;
}

/* ── 整型参数 ──────────────────────────────────────────────────────── */

int lv00_global_state_set_int(const char *key, int value) {
    if (!g_state.initialized) return -1;
    int idx = find_or_create_param(key, LV00_GS_TYPE_INT);
    if (idx < 0) return -1;
    g_state.params[idx].value.int_val = value;
    g_state.params[idx].is_set = true;
    return 0;
}

int lv00_global_state_get_int(const char *key, int default_val) {
    if (!g_state.initialized) return default_val;
    int idx = find_param_index(key);
    if (idx < 0 || !g_state.params[idx].is_set) return default_val;
    if (g_state.params[idx].type != LV00_GS_TYPE_INT) return default_val;
    return g_state.params[idx].value.int_val;
}

/* ── 浮点参数 ──────────────────────────────────────────────────────── */

int lv00_global_state_set_double(const char *key, double value) {
    if (!g_state.initialized) return -1;
    int idx = find_or_create_param(key, LV00_GS_TYPE_DOUBLE);
    if (idx < 0) return -1;
    g_state.params[idx].value.dbl_val = value;
    g_state.params[idx].is_set = true;
    return 0;
}

double lv00_global_state_get_double(const char *key, double default_val) {
    if (!g_state.initialized) return default_val;
    int idx = find_param_index(key);
    if (idx < 0 || !g_state.params[idx].is_set) return default_val;
    if (g_state.params[idx].type != LV00_GS_TYPE_DOUBLE) return default_val;
    return g_state.params[idx].value.dbl_val;
}

/* ── 字符串参数 ────────────────────────────────────────────────────── */

int lv00_global_state_set_string(const char *key, const char *value) {
    if (!g_state.initialized) return -1;
    int idx = find_or_create_param(key, LV00_GS_TYPE_STRING);
    if (idx < 0) return -1;
    if (value) {
        strncpy(g_state.params[idx].value.str_val, value, LV00_GS_MAX_VALUE_LEN - 1);
    } else {
        g_state.params[idx].value.str_val[0] = '\0';
    }
    g_state.params[idx].is_set = true;
    return 0;
}

const char *lv00_global_state_get_string(const char *key, const char *default_val) {
    if (!g_state.initialized) return default_val;
    int idx = find_param_index(key);
    if (idx < 0 || !g_state.params[idx].is_set) return default_val;
    if (g_state.params[idx].type != LV00_GS_TYPE_STRING) return default_val;
    return g_state.params[idx].value.str_val;
}

/* ── 布尔参数 ──────────────────────────────────────────────────────── */

int lv00_global_state_set_bool(const char *key, bool value) {
    if (!g_state.initialized) return -1;
    int idx = find_or_create_param(key, LV00_GS_TYPE_BOOL);
    if (idx < 0) return -1;
    g_state.params[idx].value.bool_val = value;
    g_state.params[idx].is_set = true;
    return 0;
}

bool lv00_global_state_get_bool(const char *key, bool default_val) {
    if (!g_state.initialized) return default_val;
    int idx = find_param_index(key);
    if (idx < 0 || !g_state.params[idx].is_set) return default_val;
    if (g_state.params[idx].type != LV00_GS_TYPE_BOOL) return default_val;
    return g_state.params[idx].value.bool_val;
}

/* ── 重置 ────────────────────────────────────────────────────────────── */

int lv00_global_state_reset(void) {
    if (!g_state.initialized) return -1;
    g_state.param_count = 0;
    return 0;
}

/* ── 版本 ────────────────────────────────────────────────────────────── */

const char *lv00_global_state_get_version(void) {
    return g_state.version;
}
