/**
 * @file preset_mathematical_logic.c
 * @brief 数理逻辑预设函数块模块 - 实现
 *
 * 实现理论数学研究项目Lv-00中数理逻辑领域的预设函数块。
 * 采用v2统一宏模式，使用 REGISTER_LOGIC 宏简化注册流程。
 *
 * 模块包含40个预设，分为五大类别：
 *   - 命题逻辑（12个）：合取、析取、否定、蕴涵、等价、异或、
 *     与非、或非、重言式判定、矛盾式判定、可满足性判定、析取范式转换
 *   - 一阶逻辑（10个）：全称量化、存在量化、量词否定、项代入、
 *     自由变量检查、约束变量检查、全称实例化、存在泛化、
 *     前束范式、Skolem范式
 *   - 证明论（8个）：假言推理、否定后件、合取引入、析取消除、
 *     归谬法、条件证明、反证法、自然演绎系统
 *   - 模型论（5个）：模型满足关系、理论一致性判定、初等等价、
 *     紧致性定理、Lowenheim-Skolem定理
 *   - 递归论（5个）：可计算函数判定、图灵机模拟、停机问题、
 *     递归可枚举判定、可判定性检查
 *
 * @module MathematicalLogic
 * @category PRESET_CATEGORY_LOGIC
 * @version 2.0.0
 * @author Lv-00 开发团队
 */

#include "lv/preset_mathematical_logic.h"

#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/preset_blocks.h"
#include "lv/preset_common.h"

/* 内部别名：与 preset_mathematical_logic.h 中 MATHEMATICAL_LOGIC_PRESET_COUNT 一致 */
#define MATH_LOGIC_PRESET_COUNT MATHEMATICAL_LOGIC_PRESET_COUNT

/* ==================== 预设函数块数量 ==================== */

/** 数理逻辑模块预设函数块总数 */

/**
 * @brief 获取数理逻辑预设函数块数量
 *
 * @return int 数理逻辑模块预设函数块总数（40）
 */
int preset_mathematical_logic_count(void) {
    return MATH_LOGIC_PRESET_COUNT;
}
