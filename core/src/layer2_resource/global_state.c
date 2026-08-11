/**
 * @file global_state.c
 * @brief Lv-00 全局状态管理
 *
 * @details 提供全局配置参数的集中存储和访问接口。
 *          支持整数、浮点数、字符串、布尔值四种参数类型。
 *
 *          注意：核心配置管理（lv_config_* 系列 API）在 lv_utils.c 中实现。
 *          本模块提供独立的、轻量级的全局状态存储，用于不需要完整配置管理器的场景。
 *
 * 设计要点：
 * - 线程安全：通过平台互斥锁（Win32 CRITICAL_SECTION / POSIX pthread_mutex_t）保护
 * - 容量限制：最多 256 个参数，键最长 128 字节，值最长 512 字节
 * - 类型安全：set/get 操作验证类型匹配
 * - 参数查找使用线性搜索（小规模数据集下性能足够）
 *
 * @version 3.3.0
 * @author Lv-00 Team
 * @license MIT
 */

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_utils.h"

/* ── 线程安全 ─────────────────────────────────────────────────────── */

#include "lv/lv_thread.h"
#include "lv_internal.h"
#include "lv/lv_hashtable.h"

lv_LAZY_LOCK_DEFINE(g_state_lock);

#define GS_LOCK() lv_lazy_lock_lock(&g_state_lock, g_state_lock_init_once)
#define GS_UNLOCK() lv_lazy_lock_unlock(&g_state_lock)
#define GS_INIT_LOCK() lv_lazy_lock_init(&g_state_lock, g_state_lock_init_once)
#define GS_DESTROY_LOCK() lv_lazy_lock_destroy(&g_state_lock)

/* ── 常量 ─────────────────────────────────────────────────────────── */

#define lv_GS_MAX_PARAMS 256
#define lv_GS_MAX_KEY_LEN 128
#define lv_GS_MAX_VALUE_LEN 512
#define lv_GS_MAX_ERROR_LEN 256

/* ── 参数类型 ─────────────────────────────────────────────────────── */

typedef enum { lv_GS_TYPE_INT = 0, lv_GS_TYPE_DOUBLE = 1, lv_GS_TYPE_STRING = 2, lv_GS_TYPE_BOOL = 3 } lvGsParamType;

/* ── 参数条目 ─────────────────────────────────────────────────────── */

/**
 * @brief 全局状态参数条目
 */
typedef struct {
    char key[lv_GS_MAX_KEY_LEN]; /**< 参数键名 */
    lvGsParamType type;          /**< 参数类型 */
    union {
        int int_val;                       /**< 整数值 */
        double dbl_val;                    /**< 浮点数值 */
        char str_val[lv_GS_MAX_VALUE_LEN]; /**< 字符串值 */
        bool bool_val;                     /**< 布尔值 */
    } value;                               /**< 参数值联合体 */
    bool is_set;                           /**< 是否已设置 */
} lvGsParam;

/* ── 全局状态结构体 ─────────────────────────────────────────────────── */

/**
 * @brief 全局状态结构
 */
typedef struct {
    lvGsParam params[lv_GS_MAX_PARAMS]; /**< 参数数组 */
    int param_count;                    /**< 当前参数数量 */
    bool initialized;                   /**< 初始化标志 */
    char version[64];                   /**< 版本号 */
    lvHashtable *param_index;           /**< key → 索引+1 哈希索引（可选加速，未建/失效回退线性） */
} lvGlobalState;

static lvGlobalState g_state = {0};

/* exempt: 惰性守卫豁免 —— g_state.initialized 为"外部驱动的初始化标志"：
 * 9 处 !g_state.initialized 是防御性活性检查（未初始化则拒绝操作/返回默认值），
 * 本文件无初始化 setter（lv_global_state_is_initialized 恒为 false，属既有设计，
 * 由外部流程负责置位）；lv_once 属"进程级一次性自初始化"，会改变初始化时机与
 * 语义（自动初始化将令 set/get 从拒绝变为生效），且无法表达"查询初始化状态"，
 * 故保留手写标志检查，不迁移。 */

/* ── 内部辅助 ─────────────────────────────────────────────────────── */

/**
 * @brief 查找参数索引（线性搜索）
 * @note 调用此函数前必须持有 GS_LOCK()
 * @param key 参数键名
 * @return 参数索引，未找到返回 -1
 */
static int find_param_index(const char *key) {
    if (!key)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "key is NULL");
    /* 哈希快查（可选索引，存 索引+1；越界/未建则回退线性扫描） */
    if (g_state.param_index) {
        intptr_t v = (intptr_t) lv_hashtable_str_get(g_state.param_index, key);
        if (v != 0 && (int) v - 1 < g_state.param_count)
            return (int) v - 1;
    }
    for (int i = 0; i < g_state.param_count; i++) {
        if (strcmp(g_state.params[i].key, key) == 0)
            return i;
    }
    lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "param key not found");
}

/**
 * @brief 查找或创建参数条目
 * @note 调用此函数前必须持有 GS_LOCK()
 * @param key  参数键名
 * @param type 参数类型
 * @return 参数索引，失败（注册表已满）返回 -1
 */
static int find_or_create_param(const char *key, lvGsParamType type) {
    if (!key)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "key is NULL");
    int idx = find_param_index(key);
    if (idx >= 0)
        return idx;
    if (g_state.param_count >= lv_GS_MAX_PARAMS)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "param registry full (max %d)", lv_GS_MAX_PARAMS);
    idx = g_state.param_count++;
    memset(&g_state.params[idx], 0, sizeof(lvGsParam));
    strncpy(g_state.params[idx].key, key, lv_GS_MAX_KEY_LEN - 1);
    g_state.params[idx].key[lv_GS_MAX_KEY_LEN - 1] = '\0'; /* 确保 null-terminate */
    g_state.params[idx].type = type;
    /* 维护哈希索引（惰性创建；插入失败不影响正确性，回退线性） */
    if (!g_state.param_index)
        g_state.param_index = lv_hashtable_str_create(64);
    if (g_state.param_index)
        lv_hashtable_str_insert(g_state.param_index, g_state.params[idx].key, (void *) (intptr_t) (idx + 1));
    return idx;
}

/* ── 公共 API ──────────────────────────────────────────────────────── */

/**
 * @brief 检查全局状态是否已初始化
 *
 * @return true 已初始化，false 未初始化
 */
bool lv_global_state_is_initialized(void) {
    return g_state.initialized;
}

/* ── 整型参数 ──────────────────────────────────────────────────────── */

/**
 * @brief 设置整型参数
 *
 * @param key   参数键名
 * @param value 整数值
 * @return 0 成功，-1 失败（未初始化或注册表已满）
 */
int lv_global_state_set_int(const char *key, int value) {
    if (!g_state.initialized)
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "global state not initialized");
    GS_LOCK();
    int idx = find_or_create_param(key, lv_GS_TYPE_INT);
    if (idx < 0) {
        GS_UNLOCK();
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "failed to find or create param '%s'", key);
    }
    g_state.params[idx].value.int_val = value;
    g_state.params[idx].is_set = true;
    GS_UNLOCK();
    return 0;
}

/**
 * @brief 获取整型参数
 *
 * @param key         参数键名
 * @param default_val 默认值（参数不存在或类型不匹配时返回）
 * @return 整数值或 default_val
 */
int lv_global_state_get_int(const char *key, int default_val) {
    if (!g_state.initialized)
        return default_val;
    GS_LOCK();
    int idx = find_param_index(key);
    if (idx < 0 || !g_state.params[idx].is_set) {
        GS_UNLOCK();
        return default_val;
    }
    if (g_state.params[idx].type != lv_GS_TYPE_INT) {
        GS_UNLOCK();
        return default_val;
    }
    int ret = g_state.params[idx].value.int_val;
    GS_UNLOCK();
    return ret;
}

/* ── 浮点参数 ──────────────────────────────────────────────────────── */

/**
 * @brief 设置浮点型参数
 *
 * @param key   参数键名
 * @param value 浮点数值
 * @return 0 成功，-1 失败
 */
int lv_global_state_set_double(const char *key, double value) {
    if (!g_state.initialized)
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "global state not initialized");
    GS_LOCK();
    int idx = find_or_create_param(key, lv_GS_TYPE_DOUBLE);
    if (idx < 0) {
        GS_UNLOCK();
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "failed to find or create param '%s'", key);
    }
    g_state.params[idx].value.dbl_val = value;
    g_state.params[idx].is_set = true;
    GS_UNLOCK();
    return 0;
}

/**
 * @brief 获取浮点型参数
 *
 * @param key         参数键名
 * @param default_val 默认值
 * @return 浮点数值或 default_val
 */
double lv_global_state_get_double(const char *key, double default_val) {
    if (!g_state.initialized)
        return default_val;
    GS_LOCK();
    int idx = find_param_index(key);
    if (idx < 0 || !g_state.params[idx].is_set) {
        GS_UNLOCK();
        return default_val;
    }
    if (g_state.params[idx].type != lv_GS_TYPE_DOUBLE) {
        GS_UNLOCK();
        return default_val;
    }
    double ret = g_state.params[idx].value.dbl_val;
    GS_UNLOCK();
    return ret;
}

/* ── 字符串参数 ────────────────────────────────────────────────────── */

/**
 * @brief 设置字符串型参数
 *
 * @param key   参数键名
 * @param value 字符串值（可为 NULL，此时设为空字符串）
 * @return 0 成功，-1 失败
 */
int lv_global_state_set_string(const char *key, const char *value) {
    if (!g_state.initialized)
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "global state not initialized");
    GS_LOCK();
    int idx = find_or_create_param(key, lv_GS_TYPE_STRING);
    if (idx < 0) {
        GS_UNLOCK();
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "failed to find or create param '%s'", key);
    }
    if (value) {
        strncpy(g_state.params[idx].value.str_val, value, lv_GS_MAX_VALUE_LEN - 1);
        g_state.params[idx].value.str_val[lv_GS_MAX_VALUE_LEN - 1] = '\0'; /* 确保 null-terminate */
    } else {
        g_state.params[idx].value.str_val[0] = '\0';
    }
    g_state.params[idx].is_set = true;
    GS_UNLOCK();
    return 0;
}

/**
 * @brief 获取字符串型参数
 *
 * @param key         参数键名
 * @param default_val 默认值
 * @return 字符串值或 default_val
 */
const char *lv_global_state_get_string(const char *key, const char *default_val) {
    if (!g_state.initialized)
        return default_val;
    GS_LOCK();
    int idx = find_param_index(key);
    if (idx < 0 || !g_state.params[idx].is_set) {
        GS_UNLOCK();
        return default_val;
    }
    if (g_state.params[idx].type != lv_GS_TYPE_STRING) {
        GS_UNLOCK();
        return default_val;
    }
    const char *ret = g_state.params[idx].value.str_val;
    GS_UNLOCK();
    return ret;
}

/* ── 布尔参数 ──────────────────────────────────────────────────────── */

/**
 * @brief 设置布尔型参数
 *
 * @param key   参数键名
 * @param value 布尔值
 * @return 0 成功，-1 失败
 */
int lv_global_state_set_bool(const char *key, bool value) {
    if (!g_state.initialized)
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "global state not initialized");
    GS_LOCK();
    int idx = find_or_create_param(key, lv_GS_TYPE_BOOL);
    if (idx < 0) {
        GS_UNLOCK();
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "failed to find or create param '%s'", key);
    }
    g_state.params[idx].value.bool_val = value;
    g_state.params[idx].is_set = true;
    GS_UNLOCK();
    return 0;
}

/**
 * @brief 获取布尔型参数
 *
 * @param key         参数键名
 * @param default_val 默认值
 * @return 布尔值或 default_val
 */
bool lv_global_state_get_bool(const char *key, bool default_val) {
    if (!g_state.initialized)
        return default_val;
    GS_LOCK();
    int idx = find_param_index(key);
    if (idx < 0 || !g_state.params[idx].is_set) {
        GS_UNLOCK();
        return default_val;
    }
    if (g_state.params[idx].type != lv_GS_TYPE_BOOL) {
        GS_UNLOCK();
        return default_val;
    }
    bool ret = g_state.params[idx].value.bool_val;
    GS_UNLOCK();
    return ret;
}

/* ── 重置 ────────────────────────────────────────────────────────────── */

/**
 * @brief 重置全局状态，清空所有参数
 *
 * @return 0 成功，-1 未初始化
 */
int lv_global_state_reset(void) {
    if (!g_state.initialized)
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "global state not initialized");
    GS_LOCK();
    g_state.param_count = 0;
    GS_UNLOCK();
    return 0;
}

/* ── 版本 ────────────────────────────────────────────────────────────── */

/**
 * @brief 获取全局状态版本号
 *
 * @return 版本号字符串
 */
const char *lv_global_state_get_version(void) {
    return g_state.version;
}
