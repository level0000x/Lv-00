/* ============================================================================
 * 模块名称:Lv-00 上层统一实现 (lv_impl_upper)
 *
 * 功能概述:
 *   为 L3-L10 各层级提供统一的外部 API 实现入口。
 *   本文件汇聚了跨层级调用的 API 函数,通过内部静态表管理
 *   L3 几何演化引擎,ATP 后端,L6 可视化,L7 编排器,
 *   L8 元验证,L9 应用入口,L10 互操作等子系统的实例。
 *
 * 架构说明:
 *   各层的领域逻辑实现在对应的 layer/ 子目录中,
 *   本文件仅提供跨层 API 的注册/调度/生命周期管理。
 *   新功能应优先在对应层的 .c 文件中实现,
 *   仅当确实需要跨层统一入口时,才在此文件中注册。
 *
 * 内部结构(14 部分):
 *   第 1 部分   全局状态与内部表
 *   第 2-7 部分 L3-L4 几何与推理预设(geom_evol/atp/presets)
 *   第 8 部分   L6 可视化层封装
 *   第 9 部分   L7 编排层
 *   第 10 部分  L8 元验证层
 *   第 11 部分  L9 应用层
 *   第 12 部分  L10 互操作层
 *   第 13 部分  func_block_preset 统一封装
 *   第 14 部分  综合工具函数
 *
 * 设计文档参考:第四章 分层架构
 *
 * ============================================================================ */

/* ============================================================
 * 第1部分:头部与全局状态
 * ============================================================ */
#include <gmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/atp_backend.h"
#include "lv/conflict_detector.h"
#include "lv/engine.h"
#include "lv/func_block.h"
#include "lv/func_block_preset.h"
#include "lv/func_block_registry.h"
#include "lv/geom_evol.h"
#include "lv/interop.h"
#include "lv/lv_json.h"
#include "lv/lv_utils.h"
#include "lv/meta_verify.h"
#include "lv/orchestrator.h"
#include "lv/preset_algebraic.h"
#include "lv/preset_basic_geometry.h"
#include "lv/preset_measurements.h"
#include "lv/preset_polygons.h"
#include "lv/preset_transformations.h"
#include "lv/visual_editor.h"

#include "lv_internal.h" /* lv_RETURN_ERROR / lv_RETURN_ERROR_NULL */
#include "lv_impl_upper_internal.h"
#include "lv/lv_strbuf.h"

/** 前向声明 -- 本文件内部使用的轻量级编配器 */
typedef struct lvOrchestrator lvOrchestrator;

/* ============================================================
 * 文件级静态内部表 -- 用于 L3 实现中的 ID→object 映射
 * ============================================================ */


UpperState s_upper_state = {0};


