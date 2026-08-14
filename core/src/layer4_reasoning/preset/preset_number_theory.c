/**
 * @file preset_number_theory.c
 * @brief 数论预设函数块 - 实现
 *
 * H1 评估结论（2026-08-07）：注册样板已由 LV_PRESET_REGISTER 宏抽象（每条目 1 行），无需代码生成；运行时注册由 module/presets/number_theory.lvz 数据驱动（convert_presets.py 生成）。
 *
 * 实现理论数学研究中常用的数论运算预设函数块。
 * 所有预设函数块都遵循模块化、确定性原则。
 *
 * @module NumberTheory
 * @category PRESET_CATEGORY_NUMBER_THEORY
 * @version 5.0.0
 */

/*
 * ============================================================
 * 头文件包含说明
 * ============================================================
 * preset_number_theory.h -> preset_blocks.h -> func_block_registry.h
 *   -> 提供 PresetType 枚举、preset_blocks_register_simple() 声明
 *   -> 提供 PresetCategory 枚举（PRESET_CATEGORY_NUMBER_THEORY 等）
 * preset_common.h
 *   -> 提供 PRESET_REGISTER 等宏、preset_register_common() 内联函数
 *   -> 提供 PRESET_SAFE_MALLOC 等安全内存操作宏
 * lv_internal.h / lv_utils.h
 *   -> 提供 lv_malloc、lv_free、lv_strdup、lv_log_* 等
 * ============================================================
 */
#include "lv/preset_number_theory.h"

#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/preset_blocks.h"
#include "lv/preset_common.h" /* 预设公共宏与辅助函数（PRESET_ERROR_LOG 等） */

/* ==================== 预设函数块数量 ==================== */

/** 数论模块预设函数块总数：28（与头文件中 NUMBER_THEORY_PRESET_COUNT 一致） */

