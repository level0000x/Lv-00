/* ============================================================================
 * 模块名称:Lv-00 上层统一实现 (lv_impl_upper)
 *
 * 本文件为跨层级 API 的集中包含入口，具体实现已拆分至
 * lv_impl_upper_internal.h 对应的 lv_impl_upper_*.c 子模块，
 * 此处仅汇总各子系统头文件。
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

#include "lv/lv_internal.h" /* lv_RETURN_ERROR / lv_RETURN_ERROR_NULL */
#include "lv_impl_upper_internal.h"
#include "lv/lv_strbuf.h"


