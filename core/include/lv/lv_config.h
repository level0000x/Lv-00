#ifndef lv_LV_CONFIG_H
#define lv_LV_CONFIG_H
#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */

/* ========================================================================
 * lvConfig 运行时配置
 *
 * 使用方式：
 *   #include "lv/lv_config.h"
 *   const lvConfig *cfg = lv_config_current();
 *   int limit = cfg->proof.proof_max_branches;
 *
 * 注意：
 *   此头文件仅用于运行时配置查询。编译期常量（如 MAX_*) 应在
 *   config.h 中定义。
 * ======================================================================== */

/* 前向声明 */
typedef struct lvConfig lvConfig;

/* 获取当前运行时配置（F43/K15 方案 B：不可变快照，无锁原子读） */
lv_PUBLIC_API const lvConfig *lv_config_current(void);

/* 释放全部延迟回收的配置快照（lv_cleanup 单线程阶段调用，F43/K15 方案 B） */
lv_PUBLIC_API void lv_config_snapshot_cleanup(void);

#endif
