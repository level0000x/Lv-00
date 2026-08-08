/**
 * @file preset_order_theory.c
 * @brief 序理论预设函数块模块 - 实现
 *
 * H1 评估结论（2026-08-07）：注册样板已由 LV_PRESET_REGISTER 宏抽象（每条目 1 行），无需代码生成；运行时注册由 module/presets/order_theory.lvz 数据驱动（convert_presets.py 生成）。
 *
 * 实现理论数学研究项目Lv-00中序理论领域的预设函数块。
 * 采用v2统一宏模式，使用 REGISTER_ORDER 宏简化注册流程。
 *
 * 模块包含8个预设，分为四大类别：
 *   - 偏序与格论（3个）：偏序关系构造、格的上确界（join）、格的下确界（meet）
 *   - 分解与选择公理（2个）：链分解（Dilworth定理）、Zorn引理应用
 *   - 不动点理论（1个）：Tarski/Knaster不动点定理
 *   - Galois连接与完备化（2个）：Galois连接、完备化
 *
 * @module OrderTheory
 * @category PRESET_CATEGORY_LOGIC
 * @version 1.0.0
 * @author Lv-00 开发团队
 */

#include "preset_order_theory.h"

#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 序理论模块预设函数块总数：8（与头文件中 ORDER_THEORY_PRESET_COUNT 一致） */


/**
 * @brief 获取序理论预设函数块数量
 *
 * @return int 序理论模块预设函数块总数（8）
 */
int preset_order_theory_count(void) {
    return ORDER_THEORY_PRESET_COUNT;
}
