/*
 * @file lv_impl_upper_meta.c
 * @brief Lv-00 upper unified impl - L8 meta verification
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
 * 第10部分:L8 元验证层(meta_verify: 5个检查)
 *
 * 说明:meta_verify_consistency 仅被同为零调用的 lv_upper_full_verify
 * 引用、meta_verify_report 零外部调用，已按死代码删除。
 * meta_verify_completeness/soundness/differential 有测试调用
 * （test/c/test_meta_verify.c），实现位于
 * layer4_reasoning/proof/meta_verify.c，保留。
 * ============================================================ */

/** @cond INTERNAL */
/* 前向声明: 实现在 core/src/layer4_reasoning/proof/meta_verify.c
 * （与 layer8_meta_verify/meta_verify.c 同名不同模块，此前缀区分） */
extern int lv_graph_meta_verify_completeness(const ConstraintGraph *graph);
extern int lv_graph_meta_verify_soundness(const ConstraintGraph *graph);
extern int lv_graph_meta_verify_differential(const ConstraintGraph *graph_a, const ConstraintGraph *graph_b);
/** @endcond */
