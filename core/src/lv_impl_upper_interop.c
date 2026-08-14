/*
 * @file lv_impl_upper_interop.c
 * @brief Lv-00 upper unified impl - L10 interop layer
 * @details Split from lv_impl_upper.c
 */

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
#include "lv/lv_strbuf.h"
#include "lv_impl_upper_internal.h"

/* ============================================================
 * 第12部分:L10 互操作层(interop: 6种导出,含 malloc/snprintf)
 *
 * 说明:该层 6 个导出包装函数（upper_interop_export_coq、
 * interop_export_lean4、interop_export_opml、
 * upper_interop_export_geojson/svg/tikz）经审计确认零外部调用
 * （其中前三个仅被同为零调用的 lv_upper_export_all 链引用），
 * 已按死代码删除。
 * 底层 interop_export_* 真实 API（core/include/lv/interop.h）保留。
 * 保留本文件仅作为占位。
 * ============================================================ */
