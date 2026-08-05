/*
 * @file lv_impl_upper_visual.c
 * @brief Lv-00 upper unified impl - L6 visual layer
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

#include "lv_internal.h" /* lv_RETURN_ERROR / lv_RETURN_ERROR_NULL */
#include "lv/lv_strbuf.h"
#include "lv_impl_upper_internal.h"

/* ============================================================
 * 第8部分:L6 可视化层(visual_editor 5 + view_synchronizer 3 + text_code 3)
 *
 * 说明:该层 12 个包装函数（visual_editor_create/render/update/zoom/destroy、
 * view_synchronizer_create/sync/destroy、text_code_create/set_text/get_text）
 * 经审计确认零外部调用，已按死代码删除。
 * lvObjSlot 对象表基础设施位于 lv_impl_upper_internal.h，不受影响；
 * 底层 lv_visual_editor_* / lv_view_sync_* / lv_text_code_* 真实 API 保留。
 * 保留本文件仅作为占位。
 * ============================================================ */
