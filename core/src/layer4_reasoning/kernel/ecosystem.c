/**
 * @file ecosystem.c
 * @brief 插件生态系统管理模块（子目录版本）
 *
 * 管理 Lv-00 系统中的模块注册、生命周期和查询。
 * 提供简单的模块注册表，支持按名称和层级查询已注册模块。
 */

#include "lv/ecosystem.h"
#include "lv/lv_numeric.h"

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

/** @brief 插件生态系统全局状态 */
typedef struct {
    int initialized;                               /**< 初始化标志 */
    int count;                                     /**< 已注册模块数量 */
    EcosystemEntry modules[ECOSYSTEM_MAX_MODULES]; /**< 模块注册表 */
} EcosystemState;

/** @brief 插件生态系统全局单例 */
static EcosystemState s_ecosystem_state = {0};

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
/* exempt: 惰性守卫豁免 —— 生态系统为"init→shutdown 可重入"模式：
 * lv_ecosystem_shutdown 将 initialized 置 0（由 lv_cleanup 在 lv.c 中调用），
 * 允许再次 init（lv_once 不可重置，转换后 shutdown 无法恢复）；
 * L90 的 !initialized 为消费者活性检查（register 拒绝未初始化操作）。
 * 生命周期由 lv_init/lv_cleanup 显式串行驱动，故保留手写标志检查，不迁移。 */
int lv_ecosystem_init(void) {
    if (s_ecosystem_state.initialized) {
        return 0; /* 已初始化，幂等返回 */
    }
    memset(s_ecosystem_state.modules, 0, sizeof(s_ecosystem_state.modules));
    s_ecosystem_state.count = 0;
    s_ecosystem_state.initialized = 1;
    return 0;
}

/**
 * @brief 关闭插件生态系统
 *
 * 标记所有模块为非激活状态，重置计数器。
 */
void lv_ecosystem_shutdown(void) {
    int i;
    for (i = 0; i < s_ecosystem_state.count; i++) {
        s_ecosystem_state.modules[i].active = 0;
    }
    s_ecosystem_state.count = 0;
    s_ecosystem_state.initialized = 0;
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

    if (!name || !s_ecosystem_state.initialized) {
        return -1;
    }
    if (s_ecosystem_state.count >= ECOSYSTEM_MAX_MODULES) {
        return -1;
    }
    if (layer < 1 || layer > 10) {
        return -1;
    }

    /* 检查是否已存在同名模块 */
    {
        int i;
        for (i = 0; i < s_ecosystem_state.count; i++) {
            if (strcmp(s_ecosystem_state.modules[i].name, name) == 0) {
                return -1; /* 重复注册 */
            }
        }
    }

    entry = &s_ecosystem_state.modules[s_ecosystem_state.count];
    strncpy(entry->name, name, ECOSYSTEM_NAME_MAX_LEN - 1);
    entry->name[ECOSYSTEM_NAME_MAX_LEN - 1] = '\0';
    entry->layer = layer;
    entry->active = 1;
    s_ecosystem_state.count++;

    return 0;
}

/**
 * @brief 获取已注册模块总数
 *
 * @return 已注册模块数量
 */
int lv_ecosystem_module_count(void) {
    return s_ecosystem_state.count;
}

/**
 * @brief 按索引获取模块名称
 *
 * @param idx 模块索引 (0-based)
 * @return 模块名称字符串（内部存储，勿释放），无效索引返回 NULL
 */
const char *lv_ecosystem_module_name(int idx) {
    if (!lv_index_in_range(idx, s_ecosystem_state.count)) {
        return NULL;
    }
    return s_ecosystem_state.modules[idx].name;
}
