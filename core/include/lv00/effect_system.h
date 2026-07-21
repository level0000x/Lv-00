#ifndef LV00_EFFECT_SYSTEM_H
#define LV00_EFFECT_SYSTEM_H

#include "lv00/io_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Effect annotation */
typedef struct Lv00EffectAnnotation {
    Lv00EffectType *effects;
    int effect_count;
} Lv00EffectAnnotation;

/* Effect log entry */
typedef struct Lv00EffectLogEntry {
    Lv00EffectType effect;
    int block_id;
    char description[256];
    double timestamp;
} Lv00EffectLogEntry;

/* Effect tracker */
typedef struct Lv00EffectTracker {
    Lv00EffectLogEntry *entries;
    int entry_count;
    int entry_capacity;

    /* Current combined effect */
    Lv00EffectAnnotation *current_effect;
} Lv00EffectTracker;

/* Lifecycle */

/**
 * @brief 创建副作用追踪器实例
 * @return 成功返回副作用追踪器指针，失败返回 NULL
 */
Lv00EffectTracker *lv00_effect_tracker_create(void);

/**
 * @brief 销毁副作用追踪器并释放所有资源
 * @param tracker 指向待销毁的副作用追踪器的指针
 */
void lv00_effect_tracker_destroy(Lv00EffectTracker *tracker);

/**
 * @brief 重置副作用追踪器，清空所有已记录的副作用日志
 * @param tracker 指向待重置的副作用追踪器的指针
 */
void lv00_effect_tracker_reset(Lv00EffectTracker *tracker);

/* Recording */

/**
 * @brief 记录一条副作用日志条目到追踪器
 * @param tracker 指向副作用追踪器的指针
 * @param effect 副作用类型
 * @param block_id 关联的代码块 ID
 * @param desc 副作用描述信息
 */
void lv00_effect_tracker_record(Lv00EffectTracker *tracker, Lv00EffectType effect,
                                 int block_id, const char *desc);

/* Query */

/**
 * @brief 查询追踪器中是否包含指定类型的副作用
 * @param tracker 指向副作用追踪器的指针
 * @param effect 要查询的副作用类型
 * @return 非零表示存在该副作用，0 表示不存在
 */
int lv00_effect_tracker_has_effect(const Lv00EffectTracker *tracker, Lv00EffectType effect);

/**
 * @brief 检查追踪器是否处于纯净状态（无副作用记录）
 * @param tracker 指向副作用追踪器的指针
 * @return 非零表示纯净无副作用，0 表示存在副作用
 */
int lv00_effect_tracker_is_pure(const Lv00EffectTracker *tracker);

/**
 * @brief 获取当前追踪器中的组合副作用注解
 * @param tracker 指向副作用追踪器的指针
 * @return 返回当前组合副作用注解的常量指针，无副作用时返回 NULL
 */
const Lv00EffectAnnotation *lv00_effect_tracker_current(const Lv00EffectTracker *tracker);

/* Effect composition */

/**
 * @brief 组合两个副作用注解，生成一个新的合并注解
 * @param a 指向第一个副作用注解的指针
 * @param b 指向第二个副作用注解的指针
 * @return 成功返回合并后的副作用注解指针，失败返回 NULL
 */
Lv00EffectAnnotation *lv00_effect_compose(const Lv00EffectAnnotation *a,
                                           const Lv00EffectAnnotation *b);

/**
 * @brief 销毁副作用注解并释放资源
 * @param ann 指向待销毁的副作用注解的指针
 */
void lv00_effect_annotation_destroy(Lv00EffectAnnotation *ann);

/* Effect checking rules */

/**
 * @brief 检查几何相关操作的副作用是否为纯净（无副作用）
 * @param tracker 指向副作用追踪器的指针
 * @return 非零表示几何操作纯净，0 表示存在副作用
 */
int lv00_effect_check_geometry_pure(const Lv00EffectTracker *tracker);

#ifdef __cplusplus
}
#endif

#endif /* LV00_EFFECT_SYSTEM_H */
