/**
 * @file ecosystem.c
 * @brief 插件生态系统管理模块（子目录版本）
 *
 * 管理 Lv-00 系统中的模块注册、生命周期和查询。
 * 提供简单的模块注册表，支持按名称和层级查询已注册模块。
 */

#include "lv/ecosystem.h"

#include <stdlib.h>
#include <string.h>

/* ================================================================
 *  内部常量与数据结构
 * ================================================================ */

#define ECOSYSTEM_MAX_MODULES 128 /**< 最大模块数量 */
#define ECOSYSTEM_NAME_MAX_LEN 64 /**< 模块名称最大长度 */

/**
 * @brief 已注册模块条目
 */
typedef struct {
    char name[ECOSYSTEM_NAME_MAX_LEN]; /**< 模块名称 */
    int layer;                         /**< 所属层级 (1-10) */
    int active;                        /**< 是否激活 */
} EcosystemEntry;

/* ================================================================
 *  模块全局状态
 * ================================================================ */

static int g_ecosystem_initialized = 0;                           /**< 初始化标志 */
static int g_ecosystem_count = 0;                                 /**< 已注册模块数量 */
static EcosystemEntry g_ecosystem_modules[ECOSYSTEM_MAX_MODULES]; /**< 模块注册表 */

/* ================================================================
 *  公共 API 实现
 * ================================================================ */

/**
 * @brief 初始化插件生态系统
 *
 * 清零模块注册表，设置初始化标志。
 * 重复调用是安全的（幂等）。
 *
 * @return 0 成功
 */
int lv_ecosystem_init(void) {
    if (g_ecosystem_initialized) {
        return 0; /* 已初始化，幂等返回 */
    }
    memset(g_ecosystem_modules, 0, sizeof(g_ecosystem_modules));
    g_ecosystem_count = 0;
    g_ecosystem_initialized = 1;
    return 0;
}

/**
 * @brief 关闭插件生态系统
 *
 * 标记所有模块为非激活状态，重置计数器。
 */
void lv_ecosystem_shutdown(void) {
    int i;
    for (i = 0; i < g_ecosystem_count; i++) {
        g_ecosystem_modules[i].active = 0;
    }
    g_ecosystem_count = 0;
    g_ecosystem_initialized = 0;
}

/**
 * @brief 注册一个模块到生态系统
 *
 * @param name   模块名称（非 NULL，内部复制）
 * @param layer  所属层级 (1-10)
 * @return 0 成功，-1 参数错误或注册表已满
 */
int lv_ecosystem_register_module(const char *name, int layer) {
    EcosystemEntry *entry;

    if (!name || !g_ecosystem_initialized) {
        return -1;
    }
    if (g_ecosystem_count >= ECOSYSTEM_MAX_MODULES) {
        return -1;
    }
    if (layer < 1 || layer > 10) {
        return -1;
    }

    /* 检查是否已存在同名模块 */
    {
        int i;
        for (i = 0; i < g_ecosystem_count; i++) {
            if (strcmp(g_ecosystem_modules[i].name, name) == 0) {
                return -1; /* 重复注册 */
            }
        }
    }

    entry = &g_ecosystem_modules[g_ecosystem_count];
    strncpy(entry->name, name, ECOSYSTEM_NAME_MAX_LEN - 1);
    entry->name[ECOSYSTEM_NAME_MAX_LEN - 1] = '\0';
    entry->layer = layer;
    entry->active = 1;
    g_ecosystem_count++;

    return 0;
}

/**
 * @brief 获取已注册模块总数
 *
 * @return 已注册模块数量
 */
int lv_ecosystem_module_count(void) {
    return g_ecosystem_count;
}

/**
 * @brief 按索引获取模块名称
 *
 * @param idx 模块索引 (0-based)
 * @return 模块名称字符串（内部存储，勿释放），无效索引返回 NULL
 */
const char *lv_ecosystem_module_name(int idx) {
    if (idx < 0 || idx >= g_ecosystem_count) {
        return NULL;
    }
    return g_ecosystem_modules[idx].name;
}
