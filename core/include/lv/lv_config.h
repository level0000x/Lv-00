#ifndef lv_CONFIG_H
#define lv_CONFIG_H

/* ========================================================================
 * lvConfig 运行时配置
 *
 * 使用方式：
 *   #include "lv/lv_config.h"
 *   const lvConfig *cfg = lv_config_current();
 *   int limit = cfg->max_proof_depth;
 *
 * 注意：
 *   此头文件仅用于运行时配置查询。编译期常量（如 MAX_*) 应在
 *   config.h 中定义。
 * ======================================================================== */

/* 前向声明 */
typedef struct lvConfig lvConfig;

/* 获取当前运行时配置 */
const lvConfig *lv_config_current(void);

#endif
