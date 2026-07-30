#ifndef lv_REGISTRY_H
#define lv_REGISTRY_H

#include <stdbool.h>
#include <stddef.h>
#include "lv_platform.h"  /* for lvMutex, lv_MUTEX_* */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 通用注册表条目：名称 + 创建函数指针 */
typedef struct lvRegistryEntry {
    const char *name;          /**< 后端/插件名称 */
    void *(*create)(void);     /**< 创建函数（返回 void*，调用者转型） */
} lvRegistryEntry;

/** @brief 通用注册表（线程安全） */
typedef struct lvRegistry {
    lvRegistryEntry *entries;  /**< 条目数组 */
    int count;                 /**< 当前条目数 */
    int capacity;              /**< 容量 */
    lvMutex mutex;             /**< 互斥锁 */
} lvRegistry;

/**
 * @brief 初始化注册表
 * @param reg      注册表指针
 * @param capacity 初始容量（0 则使用默认 16）
 */
void lv_registry_init(lvRegistry *reg, int capacity);

/**
 * @brief 销毁注册表
 */
void lv_registry_destroy(lvRegistry *reg);

/**
 * @brief 注册一个条目
 * @param reg    注册表指针
 * @param name   条目名称
 * @param create 创建函数指针
 * @return true 成功，false name 重复或内存不足
 */
bool lv_registry_register(lvRegistry *reg, const char *name, void *(*create)(void));

/**
 * @brief 按名称查找并创建后端
 * @param reg  注册表指针
 * @param name 后端名称
 * @return 创建的对象指针，未找到返回 NULL
 */
void *lv_registry_create(const lvRegistry *reg, const char *name);

/**
 * @brief 按名称查找条目索引
 * @return 索引，未找到返回 -1
 */
int lv_registry_find(const lvRegistry *reg, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* lv_REGISTRY_H */
