#ifndef LV00_CONFIG_H
#define LV00_CONFIG_H

/* ========================================================================
 * Lv00Config 运行时配置
 *
 * 使用方式：
 *   #include "lv00/lv00_config.h"
 *   const Lv00Config *cfg = lv00_config_current();
 *   int limit = cfg->max_proof_depth;
 *
 * 注意：
 *   此头文件仅用于运行时配置查询。编译期常量（如 MAX_*) 应在
 *   config.h 中定义。
 * ======================================================================== */

/* 前向声明 */
typedef struct Lv00Config Lv00Config;

/* 获取当前运行时配置 */
const Lv00Config *lv00_config_current(void);

#endif
