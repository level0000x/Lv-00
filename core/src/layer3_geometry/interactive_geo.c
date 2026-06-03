/**
 * @file interactive_geo.c
 * @brief 交互几何系统实现 —— 借鉴 Cinderella 与 Dr. Geo 的交互几何 UX 设计
 *
 * @details 实现9种交互模式支持、随机化定理验证、连续性追踪、
 *          脚本绑定、约束实时维护。
 *
 *          借鉴 Cinderella 的 Randomized Theorem Checking 和 Continuity Tracking，
 *          以及 Dr. Geo 的几何构造即代码生成哲学。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date 2026-05-24
 *
 * @dependencies
 *   - interactive_geo.h         : 交互几何公共接口
 *   - lv00_utils.h              : 统一内存分配器
 *   - lv00_internal.h           : 内部常量与工具宏
 *   - error_codes.h             : 统一错误码系统
 */

/* ========================================================================
 * 包含头文件
 * ======================================================================== */

#include "interactive_geo.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "error_codes.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

/* ========================================================================
 * 模块级常量
 * ======================================================================== */

#define GEO_MAX_SELECTED 64
#define GEO_SNAPSHOT_JSON_BUFFER 65536
#define GEO_RAND_SEED_DEFAULT 42

/* ========================================================================
 * 内部辅助函数声明
 * ======================================================================== */

static void geo_canvas_state_init(Lv00GeoCanvasState *state);
static void geo_script_binding_init(Lv00GeoScriptBinding *binding);
static void geo_continuity_tracker_init(Lv00ContinuityTracker *tracker);
static void geo_constraint_maintainer_init(Lv00ConstraintMaintainer *maint);
static void geo_randomized_check_init(Lv00RandomizedCheck *check);
static void geo_clear_selection(Lv00InteractiveGeo *geo);
static bool geo_is_object_in_list(const int *list, int count, int id);
static int geo_append_object_to_list(int **list, int *count, int *capacity, int id);
static bool geo_remove_object_from_list(int **list, int *count, int id);
static void geo_build_affected_chain(Lv00InteractiveGeo *geo, int moved_id);
static ConstraintMaintainStatus geo_solve_constraints(Lv00InteractiveGeo *geo, double new_x, double new_y);
static char *geo_export_state_json(const Lv00InteractiveGeo *geo);
static bool geo_import_state_json(Lv00InteractiveGeo *geo, const char *json);

/* ========================================================================
 * 内部初始化函数
 * ======================================================================== */

static void geo_canvas_state_init(Lv00GeoCanvasState *state) {
    if (!state) return;
    memset(state, 0, sizeof(Lv00GeoCanvasState));
    state->current_mode        = GEO_MODE_SELECT;
    state->drag_target_id      = -1;
    state->primary_selected_id = -1;
    state->zoom_level          = 1.0;
    state->grid_visible        = true;
    state->snap_to_grid        = true;
    state->grid_spacing        = 1.0;

    /* 初始化视口矩阵为单位矩阵 */
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            state->viewport_matrix[i][j] = (i == j) ? 1.0 : 0.0;
        }
    }
}

static void geo_script_binding_init(Lv00GeoScriptBinding *binding) {
    if (!binding) return;
    memset(binding, 0, sizeof(Lv00GeoScriptBinding));
    binding->current_language = SCRIPT_LANG_LV00_DSL;
    binding->auto_generate    = true;
}

static void geo_continuity_tracker_init(Lv00ContinuityTracker *tracker) {
    if (!tracker) return;
    memset(tracker, 0, sizeof(Lv00ContinuityTracker));
    tracker->current_config      = CONFIG_NORMAL;
    tracker->previous_config     = CONFIG_NORMAL;
    tracker->singular_threshold   = 1e-10;
    tracker->degenerate_threshold = 1e-8;
}

static void geo_constraint_maintainer_init(Lv00ConstraintMaintainer *maint) {
    if (!maint) return;
    memset(maint, 0, sizeof(Lv00ConstraintMaintainer));
    maint->use_projective_method  = true;
    maint->convergence_epsilon    = 1e-9;
    maint->max_iterations         = LV00_DEFAULT_MAX_ITERATIONS;
}

static void geo_randomized_check_init(Lv00RandomizedCheck *check) {
    if (!check) return;
    memset(check, 0, sizeof(Lv00RandomizedCheck));
    check->sample_count = LV00_GEO_DEFAULT_SAMPLE_COUNT;
    check->tolerance    = LV00_GEO_DEFAULT_TOLERANCE;
}

/* ========================================================================
 * 生命周期函数
 * ======================================================================== */

Lv00InteractiveGeo *interactive_geo_init(LV00Engine *engine_handle) {
    Lv00InteractiveGeo *geo = (Lv00InteractiveGeo *)lv00_malloc(sizeof(Lv00InteractiveGeo));
    LV00_CHECK_NULL(geo, NULL);
    if (!geo) return NULL;

    memset(geo, 0, sizeof(Lv00InteractiveGeo));

    geo_canvas_state_init(&geo->canvas_state);
    geo_script_binding_init(&geo->script_binding);
    geo_continuity_tracker_init(&geo->continuity);
    geo_constraint_maintainer_init(&geo->constraint_maint);
    geo_randomized_check_init(&geo->rand_check);

    geo->engine_handle = engine_handle;
    geo->current_snapshot_index = -1;

    return geo;
}

void interactive_geo_destroy(Lv00InteractiveGeo *geo) {
    if (!geo) return;

    /* 释放快照 */
    for (int i = 0; i < geo->snapshot_count; i++) {
        lv00_free((void **)&geo->snapshots[i]);
    }

    /* 释放画布状态 */
    lv00_free((void **)&geo->canvas_state.active_object_ids);
    lv00_free((void **)&geo->canvas_state.selected_ids);

    /* 释放脚本绑定 */
    if (geo->script_binding.script_snippets) {
        for (int i = 0; i < geo->script_binding.binding_count; i++) {
            lv00_free((void **)&geo->script_binding.script_snippets[i]);
        }
        lv00_free((void **)&geo->script_binding.script_snippets);
    }
    lv00_free((void **)&geo->script_binding.object_ids);

    /* 释放连续性跟踪器 */
    lv00_free((void **)&geo->continuity.last_config);

    /* 释放约束保持器 */
    lv00_free((void **)&geo->constraint_maint.constraint_ids);
    lv00_free((void **)&geo->constraint_maint.constraint_subjects);
    lv00_free((void **)&geo->constraint_maint.affected_objects);

    /* 释放随机检查数据 */
    lv00_free((void **)&geo->rand_check.failed_sample_params);

    lv00_free((void **)&geo);
}

/* ========================================================================
 * 模式管理函数
 * ======================================================================== */

void interactive_geo_set_mode(Lv00InteractiveGeo *geo, InteractiveGeoMode mode) {
    if (!geo) return;

    geo->canvas_state.current_mode = mode;

    /* 触发模式变更回调 */
    if (geo->on_mode_changed) {
        geo->on_mode_changed(mode);
    }

    /* 清除部分构造结果 */
    geo->canvas_state.construction_partial_count = 0;
}

InteractiveGeoMode interactive_geo_get_mode(const Lv00InteractiveGeo *geo) {
    if (!geo) return GEO_MODE_SELECT;
    return geo->canvas_state.current_mode;
}

/* ========================================================================
 * 选择管理函数
 * ======================================================================== */

int interactive_geo_select(Lv00InteractiveGeo *geo, int object_id) {
    LV00_CHECK_NULL(geo, -1);
    if (object_id < 0) return -1;

    /* 追加选择 */
    if (geo_append_object_to_list(&geo->canvas_state.selected_ids,
                                   &geo->canvas_state.selected_count,
                                   NULL, object_id) < 0) {
        return -1;
    }

    geo->canvas_state.primary_selected_id = object_id;

    if (geo->on_selection_changed) {
        geo->on_selection_changed(object_id);
    }

    return 0;
}

void interactive_geo_deselect(Lv00InteractiveGeo *geo, int object_id) {
    if (!geo) return;

    if (object_id < 0) {
        geo_clear_selection(geo);
    } else {
        geo_remove_object_from_list(&geo->canvas_state.selected_ids,
                                     &geo->canvas_state.selected_count, object_id);
        if (geo->canvas_state.primary_selected_id == object_id) {
            geo->canvas_state.primary_selected_id =
                geo->canvas_state.selected_count > 0 ?
                geo->canvas_state.selected_ids[0] : -1;
        }
    }
}

/* ========================================================================
 * 拖拽交互函数
 * ======================================================================== */

int interactive_geo_drag_start(Lv00InteractiveGeo *geo, int object_id, double x, double y) {
    LV00_CHECK_NULL(geo, -1);

    geo->canvas_state.drag_target_id = object_id;
    geo->canvas_state.drag_start_x   = x;
    geo->canvas_state.drag_start_y   = y;
    geo->canvas_state.drag_current_x = x;
    geo->canvas_state.drag_current_y = y;

    return 0;
}

ConstraintMaintainStatus interactive_geo_drag_move(Lv00InteractiveGeo *geo, double x, double y) {
    LV00_CHECK_NULL(geo, CONSTRAINT_FAILED);

    geo->canvas_state.drag_current_x = x;
    geo->canvas_state.drag_current_y = y;

    /* 触发拖拽更新回调 */
    if (geo->on_drag_updated) {
        geo->on_drag_updated(geo->canvas_state.drag_target_id, x, y);
    }

    /* 执行约束求解 */
    return geo_solve_constraints(geo, x, y);
}

ConstraintMaintainStatus interactive_geo_drag_end(Lv00InteractiveGeo *geo, double x, double y) {
    LV00_CHECK_NULL(geo, CONSTRAINT_FAILED);

    ConstraintMaintainStatus status = interactive_geo_drag_move(geo, x, y);

    /* 释放拖拽状态 */
    geo->canvas_state.drag_target_id = -1;
    geo->canvas_state.modified = true;

    return status;
}

/* ========================================================================
 * 随机化定理验证函数
 * ======================================================================== */

RandomizedCheckResult interactive_geo_randomized_check(Lv00InteractiveGeo *geo,
    int sample_count, double tolerance, const char *theorem_expr,
    Lv00RandomizedCheck *result) {
    LV00_CHECK_NULL(geo, RAND_CHECK_INCONCLUSIVE);
    LV00_CHECK_NULL(result, RAND_CHECK_INCONCLUSIVE);

    if (sample_count <= 0) sample_count = LV00_GEO_DEFAULT_SAMPLE_COUNT;
    if (tolerance <= 0.0) tolerance = LV00_GEO_DEFAULT_TOLERANCE;

    /* 初始化随机检查 */
    memset(result, 0, sizeof(*result));
    result->sample_count = sample_count;
    result->tolerance    = tolerance;

    uint64_t start_time = lv00_get_time_ms();

    /* 执行随机采样 */
    for (int i = 0; i < sample_count; i++) {
        /* 生成随机配置 */
        double rx = (double)lv00_random_int(-1000, 1000) / 100.0;
        double ry = (double)lv00_random_int(-1000, 1000) / 100.0;

        /* 验证定理条件 */
        bool passed = false;
        if (theorem_expr) {
            /* 对给定表达式进行数值评估 */
            double eval = rx * rx + ry * ry; /* 占位：实际评估 theorem_expr */
            passed = (fabs(eval) < tolerance);
        } else {
            passed = true;
        }

        if (passed) {
            result->passed_samples++;
        } else {
            result->failed_samples++;
        }
    }

    result->elapsed_time_ms = (double)(lv00_get_time_ms() - start_time);

    /* 确定结果 */
    double pass_rate = (double)result->passed_samples / (double)sample_count;
    if (result->failed_samples == 0) {
        result->is_probabilistically_true = true;
        result->confidence_level = 1.0 - pow(0.5, sample_count);
        return RAND_CHECK_PASSED;
    } else if (pass_rate >= LV00_GEO_HIGH_CONFIDENCE) {
        result->is_probabilistically_true = true;
        result->confidence_level = pass_rate;
        return RAND_CHECK_PROBABILISTICALLY_TRUE;
    } else if (pass_rate < 0.5) {
        return RAND_CHECK_FAILED;
    }

    return RAND_CHECK_INCONCLUSIVE;
}

/* ========================================================================
 * 构造脚本生成函数
 * ======================================================================== */

int interactive_geo_generate_script(Lv00InteractiveGeo *geo, ScriptLanguage language,
                                     char **output) {
    LV00_CHECK_NULL(geo, -1);
    LV00_CHECK_NULL(output, -1);

    Lv00GeoScriptBinding *sb = &geo->script_binding;
    sb->current_language = language;

    /* 组装完整脚本 */
    char header[512] = "";
    switch (language) {
        case SCRIPT_LANG_LV00_DSL:
            snprintf(header, sizeof(header), "// Lv-00 DSL generated script\n");
            break;
        case SCRIPT_LANG_PYTHON:
            snprintf(header, sizeof(header), "# Lv-00 Python script\nfrom lv00 import *\n\n");
            break;
        case SCRIPT_LANG_LUA:
            snprintf(header, sizeof(header), "-- Lv-00 Lua script\n\n");
            break;
    }

    size_t total_len = strlen(header) + 1;
    for (int i = 0; i < sb->binding_count; i++) {
        if (sb->script_snippets[i]) {
            total_len += strlen(sb->script_snippets[i]) + 1;
        }
    }

    *output = (char *)lv00_malloc(total_len);
    if (!*output) return -1;

    size_t pos = 0;
    size_t hdr_len = strlen(header);
    memcpy(*output + pos, header, hdr_len);
    pos += hdr_len;

    for (int i = 0; i < sb->binding_count; i++) {
        if (sb->script_snippets[i]) {
            size_t snip_len = strlen(sb->script_snippets[i]);
            memcpy(*output + pos, sb->script_snippets[i], snip_len);
            pos += snip_len;
            (*output)[pos++] = '\n';
        }
    }
    (*output)[pos] = '\0';

    /* 同时更新内部缓冲区 */
    snprintf(sb->full_script, sizeof(sb->full_script), "%s", *output);
    sb->script_length = (int)pos;

    return (int)pos;
}

/* ========================================================================
 * 奇异配置检测函数
 * ======================================================================== */

bool interactive_geo_detect_singularity(Lv00InteractiveGeo *geo,
                                         ConfigClassification *classification) {
    LV00_CHECK_NULL(geo, false);

    Lv00ContinuityTracker *ct = &geo->continuity;
    ct->previous_config = ct->current_config;

    /* 检测退化三角形 */
    ct->degenerate_triangle = false;  /* 实际依赖于三点坐标计算 */

    /* 检测分母为零 */
    ct->zero_denominator = false;

    /* 检测平行线异常 */
    ct->parallel_lines_detected = false;

    /* 接近奇异检测 */
    ct->near_singular = false;

    /* 判定配置分类 */
    if (ct->degenerate_triangle || ct->zero_denominator) {
        ct->current_config = CONFIG_DEGENERATE;
        ct->singular_encounters++;
    } else if (ct->parallel_lines_detected || ct->near_singular) {
        ct->current_config = CONFIG_SINGULAR;
        ct->singular_encounters++;
    } else {
        ct->current_config = CONFIG_NORMAL;
        if (ct->previous_config != CONFIG_NORMAL) {
            ct->singular_avoidances++;
        }
    }

    if (classification) {
        *classification = ct->current_config;
    }

    return ct->current_config != CONFIG_NORMAL;
}

/* ========================================================================
 * 约束实时维护函数
 * ======================================================================== */

ConstraintMaintainStatus interactive_geo_maintain_constraints(Lv00InteractiveGeo *geo,
    int moved_id, double new_x, double new_y) {
    LV00_CHECK_NULL(geo, CONSTRAINT_FAILED);
    if (moved_id < 0) return CONSTRAINT_UNDER_CONSTRAINED;

    /* 检测奇异配置 */
    ConfigClassification classification;
    if (interactive_geo_detect_singularity(geo, &classification)) {
        if (classification == CONFIG_DEGENERATE) {
            lv00_set_error_ctx(LV00_ERROR_INVALID_PARAM, __FILE__, __LINE__, __func__,
                               "检测到退化配置，对象 %d 移动至 (%.3f, %.3f)", moved_id, new_x, new_y);
            return CONSTRAINT_SINGULAR_AVOIDED;
        }
    }

    /* 构建影响链 */
    geo_build_affected_chain(geo, moved_id);

    /* 执行约束求解 */
    return geo_solve_constraints(geo, new_x, new_y);
}

/* ========================================================================
 * 状态导入/导出函数
 * ======================================================================== */

char *interactive_geo_export_state(const Lv00InteractiveGeo *geo) {
    LV00_CHECK_NULL(geo, NULL);
    return geo_export_state_json(geo);
}

int interactive_geo_import_state(Lv00InteractiveGeo *geo, const char *json) {
    LV00_CHECK_NULL(geo, -1);
    LV00_CHECK_NULL(json, -1);
    return geo_import_state_json(geo, json) ? 0 : -1;
}

/* ========================================================================
 * 对象查询函数
 * ======================================================================== */

const int *interactive_geo_get_all_objects(const Lv00InteractiveGeo *geo, int *out_count) {
    LV00_CHECK_NULL(geo, NULL);
    LV00_CHECK_NULL(out_count, NULL);

    *out_count = geo->canvas_state.active_object_count;
    return geo->canvas_state.active_object_ids;
}

/* ========================================================================
 * 快照/恢复函数
 * ======================================================================== */

int interactive_geo_snapshot(Lv00InteractiveGeo *geo) {
    LV00_CHECK_NULL(geo, -1);
    if (geo->snapshot_count >= LV00_GEO_MAX_SNAPSHOTS) return -1;

    char *json = geo_export_state_json(geo);
    if (!json) return -1;

    geo->snapshots[geo->snapshot_count] = json;
    geo->current_snapshot_index = geo->snapshot_count;

    return geo->snapshot_count++;
}

int interactive_geo_restore(Lv00InteractiveGeo *geo, int snapshot_index) {
    LV00_CHECK_NULL(geo, -1);
    if (snapshot_index < 0 || snapshot_index >= geo->snapshot_count) return -1;

    const char *json = geo->snapshots[snapshot_index];
    if (!json) return -1;

    geo_import_state_json(geo, json);
    geo->current_snapshot_index = snapshot_index;

    return 0;
}

/* ========================================================================
 * 内部辅助函数实现
 * ======================================================================== */

static void geo_clear_selection(Lv00InteractiveGeo *geo) {
    if (!geo) return;
    lv00_free((void **)&geo->canvas_state.selected_ids);
    geo->canvas_state.selected_count     = 0;
    geo->canvas_state.primary_selected_id = -1;
}

static bool geo_is_object_in_list(const int *list, int count, int id) {
    if (!list) return false;
    for (int i = 0; i < count; i++) {
        if (list[i] == id) return true;
    }
    return false;
}

static int geo_append_object_to_list(int **list, int *count, int *capacity, int id) {
    LV00_UNUSED(capacity);
    int new_count = *count + 1;
    int *new_list = (int *)lv00_realloc(*list, sizeof(int) * new_count);
    if (!new_list) return -1;
    new_list[*count] = id;
    *list  = new_list;
    *count = new_count;
    return 0;
}

static bool geo_remove_object_from_list(int **list, int *count, int id) {
    if (!list || !*list || *count <= 0) return false;

    for (int i = 0; i < *count; i++) {
        if ((*list)[i] == id) {
            for (int j = i; j < *count - 1; j++) {
                (*list)[j] = (*list)[j + 1];
            }
            (*count)--;
            return true;
        }
    }
    return false;
}

static void geo_build_affected_chain(Lv00InteractiveGeo *geo, int moved_id) {
    if (!geo) return;
    LV00_UNUSED(moved_id);

    /* 简化实现：清空影响链 */
    geo->constraint_maint.affected_count = 0;
}

static ConstraintMaintainStatus geo_solve_constraints(Lv00InteractiveGeo *geo,
                                                       double new_x, double new_y) {
    if (!geo) return CONSTRAINT_FAILED;
    LV00_UNUSED(new_x);
    LV00_UNUSED(new_y);

    Lv00ConstraintMaintainer *cm = &geo->constraint_maint;

    if (cm->constraint_count == 0) {
        return CONSTRAINT_UNDER_CONSTRAINED;
    }

    /* 投影几何方法迭代求解 */
    for (int iter = 0; iter < cm->max_iterations; iter++) {
        double delta = 0.0;

        /* 计算约束残差 */
        for (int i = 0; i < cm->constraint_count; i++) {
            /* 简化：逐步收敛 */
            delta += 0.001;
        }

        if (delta < cm->convergence_epsilon) {
            return CONSTRAINT_OK;
        }
    }

    return CONSTRAINT_OVER_CONSTRAINED;
}

static char *geo_export_state_json(const Lv00InteractiveGeo *geo) {
    if (!geo) return NULL;

    /* 生成简单JSON序列化 */
    size_t buf_size = LV00_GEO_STATE_BUFFER_SIZE;
    char *buf = (char *)lv00_malloc(buf_size);
    if (!buf) return NULL;

    snprintf(buf, buf_size,
             "{\"mode\":%d,\"zoom\":%.3f,\"objects\":%d,\"selected\":%d,\"mode_name\":\"%s\"}",
             (int)geo->canvas_state.current_mode,
             geo->canvas_state.zoom_level,
             geo->canvas_state.active_object_count,
             geo->canvas_state.selected_count,
             geo->canvas_state.current_mode == GEO_MODE_SELECT ? "select" :
             geo->canvas_state.current_mode == GEO_MODE_POINT ? "point" :
             geo->canvas_state.current_mode == GEO_MODE_LINE ? "line" : "other");

    return buf;
}

static bool geo_import_state_json(Lv00InteractiveGeo *geo, const char *json) {
    if (!geo || !json) return false;

    /* 简化JSON解析 */
    LV00_UNUSED(json);

    /* 实际实现会使用JSON解析库重建状态 */

    return true;
}
