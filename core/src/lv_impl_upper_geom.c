/*
 * @file lv_impl_upper_geom.c
 * @brief Lv-00 upper unified impl - L3 geometry extensions
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
 * 第2部分:L3 几何扩展(geom_evol / atp_backend / proof_tptp)
 *
 * 说明:该层 9 个包装函数（geom_evol_create/step/destroy、
 * atp_backend_create/submit/result/destroy、proof_tptp_export/verify）
 * 经审计确认零外部调用，已按死代码删除。
 * 底层 geoevol_* / atp_* 真实 API 保留，不受影响。
 * 保留本文件仅作为占位。
 * ============================================================ */

/* ============================================================
 * 第3-6部分已提取到独立文件：
 *   core/src/impl_preset_basic_geometry.c
 *   core/src/impl_preset_transformations.c
 *   core/src/impl_preset_measurements.c
 *   core/src/impl_preset_polygons.c
 * ============================================================ */
