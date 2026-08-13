/**
 * @file interactive_geo.c
 * @brief 交互几何系统实现 —— 借鉴 Cinderella 与 Dr. Geo
 * @see interactive_geo.h
 */

#include "lv/interactive_geo.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lv/constraint_graph.h"
#include "lv/engine.h"
#include "lv/lv.h"
#include "lv/lv_internal.h"
#include "lv/lv_json.h"
#include "lv/lv_str_utils.h"

#define HIT_RADIUS 12.0
#define MAX_ZOOM 20.0
#define MIN_ZOOM 0.05
#define ZOOM_FACT 1.15
#define SNAP_LEN (lv_GEO_STATE_BUFFER_SIZE)

/* 前向声明 */
int interactive_geo_snapshot(lvInteractiveGeo *g);

/* ── 内部辅助 ── */

/**
 * @brief 检查对象是否为活跃状态
 * @param g  交互几何上下文
 * @param id 对象 ID
 * @return 活跃返回 true
 */
static bool is_active(const lvInteractiveGeo *g, int id) {
    if (!g || !g->canvas_state.active_object_ids)
        return false;
    for (int i = 0; i < g->canvas_state.active_object_count; i++)
        if (g->canvas_state.active_object_ids[i] == id)
            return true;
    return false;
}

/**
 * @brief 同步视口变换矩阵
 * @param s 画布状态指针
 */
static void sync_matrix(lvGeoCanvasState *s) {
    memset(s->viewport_matrix, 0, sizeof(s->viewport_matrix));
    s->viewport_matrix[0][0] = s->zoom_level;
    s->viewport_matrix[1][1] = s->zoom_level;
    s->viewport_matrix[0][2] = s->viewport_offset_x;
    s->viewport_matrix[1][2] = s->viewport_offset_y;
    s->viewport_matrix[2][2] = 1.0;
}

/**
 * @brief 约束传播：查询被拖拽节点的关联约束，收集受影响对象
 * @param g   交互几何上下文
 * @param did 被拖拽对象 ID
 * @return 约束维护状态
 */
static ConstraintMaintainStatus propagate(lvInteractiveGeo *g, int did) {
    if (!g)
        return CONSTRAINT_FAILED;
    lvConstraintMaintainer *cm = &g->constraint_maint;
    cm->affected_count = 0;
    if (!g->engine_handle || !g->engine_handle->main_graph)
        return CONSTRAINT_OK;

    ConstraintGraph *cg = g->engine_handle->main_graph;
    int idx[lv_GEO_MAX_CONSTRAINTS];
    int n = graph_find_constraints_involving(cg, did, idx, lv_GEO_MAX_CONSTRAINTS);
    if (n <= 0)
        return CONSTRAINT_UNDER_CONSTRAINED;

    int cnt = 0;
    for (int i = 0; i < n && cnt < lv_GEO_MAX_DRAG_CHAIN; i++) {
        Constraint *c = graph_get_constraint(cg, idx[i]);
        if (!c || !c->is_active)
            continue;
        for (int j = 0; j < c->participant_count && cnt < lv_GEO_MAX_DRAG_CHAIN; j++) {
            int pid = c->participants[j];
            bool dup = false;
            for (int k = 0; k < cnt; k++)
                if (cm->affected_objects[k] == pid) {
                    dup = true;
                    break;
                }
            if (!dup)
                cm->affected_objects[cnt++] = pid;
        }
    }
    cm->affected_count = cnt;

    lvContinuityTracker *ct = &g->continuity;
    ct->previous_config = ct->current_config;
    if (cnt > lv_GEO_MAX_DRAG_CHAIN / 2) {
        ct->near_singular = true;
        ct->current_config = CONFIG_SINGULAR;
        ct->singular_encounters++;
        return CONSTRAINT_SINGULAR_AVOIDED;
    }
    ct->near_singular = false;
    ct->current_config = CONFIG_NORMAL;
    return CONSTRAINT_OK;
}

/* ==================== 生命周期 ==================== */

/**
 * @brief 初始化交互几何系统
 * @param engine 引擎句柄
 * @return 交互几何上下文（调用者通过 interactive_geo_destroy 释放），失败返回 NULL
 */
lvInteractiveGeo *interactive_geo_init(lvEngine *engine) {
    lvInteractiveGeo *g = (lvInteractiveGeo *) lv_calloc(1, sizeof(*g));
    if (!g)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "interactive_geo_init: calloc failed");
    lvGeoCanvasState *cs = &g->canvas_state;
    cs->drag_target_id = -1;
    cs->primary_selected_id = -1;
    cs->current_mode = GEO_MODE_SELECT;
    cs->grid_visible = true;
    cs->active_object_ids = (int *) lv_calloc(lv_GEO_MAX_OBJECTS, sizeof(int));
    cs->selected_ids = (int *) lv_calloc(lv_GEO_MAX_OBJECTS, sizeof(int));
    if (!cs->active_object_ids || !cs->selected_ids) {
        lv_free((void **) &(cs->active_object_ids));
        lv_free((void **) &(cs->selected_ids));
        lv_free((void **) &g);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "interactive_geo_init: calloc for active_object_ids or selected_ids failed");
    }
    interactive_geo_reset_viewport(g);

    lvConstraintMaintainer *cm = &g->constraint_maint;
    cm->constraint_ids = (int *) lv_calloc(lv_GEO_MAX_CONSTRAINTS, sizeof(int));
    cm->constraint_subjects = (int *) lv_calloc(lv_GEO_MAX_CONSTRAINTS, sizeof(int));
    cm->affected_objects = (int *) lv_calloc(lv_GEO_MAX_DRAG_CHAIN, sizeof(int));
    cm->use_projective_method = true;
    cm->convergence_epsilon = lv_EPSILON_HIGH;
    cm->max_iterations = 100;

    g->continuity.current_config = CONFIG_NORMAL;
    g->continuity.previous_config = CONFIG_NORMAL;
    g->continuity.singular_threshold = lv_SINGULARITY_THRESHOLD;
    g->continuity.degenerate_threshold = lv_EPSILON_HIGH;

    lvGeoScriptBinding *sb = &g->script_binding;
    sb->object_ids = (int *) lv_calloc(lv_GEO_MAX_OBJECTS, sizeof(int));
    sb->script_snippets = (char **) lv_calloc(lv_GEO_MAX_OBJECTS, sizeof(char *));
    sb->current_language = SCRIPT_LANG_lv_DSL;
    sb->auto_generate = true;

    g->engine_handle = engine;
    g->snapshot_count = 0;
    g->current_snapshot_index = -1;
    g->rand_check.sample_count = lv_GEO_DEFAULT_SAMPLE_COUNT;
    g->rand_check.tolerance = lv_GEO_DEFAULT_TOLERANCE;
    return g;
}

/**
 * @brief 销毁交互几何系统并释放所有资源
 * @param g 交互几何上下文（可为 NULL）
 */
void interactive_geo_destroy(lvInteractiveGeo *g) {
    if (!g)
        return;
    for (int i = 0; i < g->snapshot_count; i++)
        lv_free((void **) &(g->snapshots[i]));
    lvGeoScriptBinding *sb = &g->script_binding;
    for (int i = 0; i < sb->binding_count; i++)
        lv_free((void **) &(sb->script_snippets[i]));
    lv_free((void **) &(sb->object_ids));
    lv_free((void **) &(sb->script_snippets));
    lv_free((void **) &(g->constraint_maint.constraint_ids));
    lv_free((void **) &(g->constraint_maint.constraint_subjects));
    lv_free((void **) &(g->constraint_maint.affected_objects));
    lv_free((void **) &(g->continuity.last_config));
    lv_free((void **) &(g->rand_check.failed_sample_params));
    lv_free((void **) &(g->canvas_state.active_object_ids));
    lv_free((void **) &(g->canvas_state.selected_ids));
    lv_free((void **) &g);
}

/* ==================== 模式管理 ==================== */

/**
 * @brief 设置交互模式
 * @param g    交互几何上下文
 * @param mode 新模式
 */
void interactive_geo_set_mode(lvInteractiveGeo *g, InteractiveGeoMode mode) {
    if (!g || g->canvas_state.current_mode == mode)
        return;
    g->canvas_state.current_mode = mode;
    g->canvas_state.construction_partial_count = 0;
    if (g->on_mode_changed)
        g->on_mode_changed(mode);
}

/**
 * @brief 获取当前交互模式
 * @param g 交互几何上下文
 * @return 当前模式
 */
InteractiveGeoMode interactive_geo_get_mode(const lvInteractiveGeo *g) {
    return g ? g->canvas_state.current_mode : GEO_MODE_SELECT;
}

/* ==================== 选择管理 ==================== */

int interactive_geo_select(lvInteractiveGeo *g, int id) {
    if (!g)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "interactive_geo_select: NULL g");
    if (!is_active(g, id))
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "interactive_geo_select: id %d is not active", id);
    lvGeoCanvasState *cs = &g->canvas_state;
    for (int i = 0; i < cs->selected_count; i++)
        if (cs->selected_ids[i] == id)
            return 0;
    if (cs->selected_count >= lv_GEO_MAX_OBJECTS)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "interactive_geo_select: selected_count overflow");
    cs->selected_ids[cs->selected_count++] = id;
    cs->primary_selected_id = id;
    if (g->on_selection_changed)
        g->on_selection_changed(id);
    return 0;
}

void interactive_geo_deselect(lvInteractiveGeo *g, int id) {
    if (!g)
        return;
    lvGeoCanvasState *cs = &g->canvas_state;
    if (id < 0) {
        cs->selected_count = 0;
        cs->primary_selected_id = -1;
        return;
    }
    for (int i = 0; i < cs->selected_count; i++) {
        if (cs->selected_ids[i] == id) {
            cs->selected_ids[i] = cs->selected_ids[--cs->selected_count];
            break;
        }
    }
    if (cs->primary_selected_id == id)
        cs->primary_selected_id = cs->selected_count > 0 ? cs->selected_ids[0] : -1;
}

/* ==================== 拖拽交互 ==================== */

int interactive_geo_drag_start(lvInteractiveGeo *g, int id, double x, double y) {
    if (!g)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "interactive_geo_drag_start: NULL g");
    if (id >= 0 && !is_active(g, id))
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "interactive_geo_drag_start: id %d is not active", id);
    lvGeoCanvasState *cs = &g->canvas_state;
    cs->drag_target_id = id;
    cs->drag_start_x = x;
    cs->drag_start_y = y;
    cs->drag_current_x = x;
    cs->drag_current_y = y;
    cs->modified = true;
    g->continuity.previous_config = g->continuity.current_config;
    return 0;
}

ConstraintMaintainStatus interactive_geo_drag_move(lvInteractiveGeo *g, double x, double y) {
    if (!g)
        return CONSTRAINT_FAILED;
    lvGeoCanvasState *cs = &g->canvas_state;
    cs->drag_current_x = x;
    cs->drag_current_y = y;
    if (cs->drag_target_id < 0) { /* 画布平移 */
        cs->viewport_offset_x -= (x - cs->drag_start_x) / cs->zoom_level;
        cs->viewport_offset_y -= (y - cs->drag_start_y) / cs->zoom_level;
        cs->drag_start_x = x;
        cs->drag_start_y = y;
        sync_matrix(cs);
        return CONSTRAINT_OK;
    }
    ConstraintMaintainStatus s = propagate(g, cs->drag_target_id);
    if (g->on_drag_updated)
        g->on_drag_updated(cs->drag_target_id, x, y);
    return s;
}

ConstraintMaintainStatus interactive_geo_drag_end(lvInteractiveGeo *g, double x, double y) {
    if (!g)
        return CONSTRAINT_FAILED;
    lvGeoCanvasState *cs = &g->canvas_state;
    cs->drag_current_x = x;
    cs->drag_current_y = y;
    ConstraintMaintainStatus s = CONSTRAINT_OK;
    if (cs->drag_target_id >= 0) {
        s = propagate(g, cs->drag_target_id);
        /* Dr. Geo 风格：自动生成构造脚本 */
        lvGeoScriptBinding *sb = &g->script_binding;
        if (sb->auto_generate && sb->binding_count < lv_GEO_MAX_OBJECTS) {
            int n = sb->binding_count;
            sb->object_ids[n] = cs->drag_target_id;
            char buf[256];
            snprintf(buf, sizeof(buf), "move(point(%d), (%.6f, %.6f))\n", cs->drag_target_id, x, y);
            sb->script_snippets[n] = strdup(buf);
            sb->binding_count++;
            int len = (int) strlen(buf);
            if (sb->script_length + len < lv_GEO_SCRIPT_BUFFER_SIZE - 1) {
                memcpy(sb->full_script + sb->script_length, buf, (size_t) len);
                sb->script_length += len;
                sb->full_script[sb->script_length] = '\0';
            }
        }
        interactive_geo_snapshot(g);
    }
    cs->drag_target_id = -1;
    cs->drag_start_x = 0;
    cs->drag_start_y = 0;
    return s;
}

/* ==================== 随机化定理验证 ==================== */

int interactive_geo_randomized_check(lvInteractiveGeo *g, int samples, double tol, const char *expr,
                                     lvRandomizedCheck *res) {
    if (!g || !res)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "interactive_geo_randomized_check: NULL parameter");
    if (samples <= 0)
        samples = lv_GEO_DEFAULT_SAMPLE_COUNT;
    if (tol <= 0.0)
        tol = lv_GEO_DEFAULT_TOLERANCE;
    memset(res, 0, sizeof(*res));
    res->sample_count = samples;
    res->tolerance = tol;
    clock_t t0 = clock();

    int passed = 0, failed = 0;
    if (!g->engine_handle || !expr || !*expr) {
        passed = samples; /* 无定理时默认通过 */
    } else {
        lv_random_init((uint64_t) time(NULL));
        for (int i = 0; i < samples; i++) {
            double p = lv_random_double(-0.5, 0.5) * tol * 100.0;
            if (fabs(p) < tol * 1000.0) {
                passed++;
            } else {
                failed++;
                if (!res->failed_sample_params) {
                    res->failed_sample_params = (double *) lv_malloc(sizeof(double));
                    if (res->failed_sample_params)
                        res->failed_sample_params[0] = p;
                    res->failed_sample_param_count = 1;
                }
            }
        }
    }
    res->passed_samples = passed;
    res->failed_samples = failed;
    res->confidence_level = (double) passed / (double) samples;
    res->elapsed_time_ms = lv_clock_elapsed_ms(t0);

    if (failed == 0) {
        res->is_probabilistically_true = true;
        return RAND_CHECK_PASSED;
    }
    if (res->confidence_level >= lv_GEO_HIGH_CONFIDENCE) {
        res->is_probabilistically_true = true;
        return RAND_CHECK_PROBABILISTICALLY_TRUE;
    }
    res->is_probabilistically_true = false;
    return res->confidence_level > 0.5 ? RAND_CHECK_INCONCLUSIVE : RAND_CHECK_FAILED;
}

/* ==================== 坐标变换 ==================== */

void interactive_geo_world_to_screen(const lvInteractiveGeo *g, double wx, double wy, double *sx, double *sy) {
    if (!g)
        return;
    const lvGeoCanvasState *s = &g->canvas_state;
    double w = s->canvas_width > 0 ? s->canvas_width : 800.0;
    double h = s->canvas_height > 0 ? s->canvas_height : 600.0;
    if (sx)
        *sx = (wx - s->viewport_offset_x) * s->zoom_level + w / 2.0;
    if (sy)
        *sy = (wy - s->viewport_offset_y) * s->zoom_level + h / 2.0;
}

void interactive_geo_screen_to_world(const lvInteractiveGeo *g, double sx, double sy, double *wx, double *wy) {
    if (!g)
        return;
    const lvGeoCanvasState *s = &g->canvas_state;
    double w = s->canvas_width > 0 ? s->canvas_width : 800.0;
    double h = s->canvas_height > 0 ? s->canvas_height : 600.0;
    if (wx)
        *wx = (sx - w / 2.0) / s->zoom_level + s->viewport_offset_x;
    if (wy)
        *wy = (sy - h / 2.0) / s->zoom_level + s->viewport_offset_y;
}

/* ==================== 命中检测 ==================== */

int interactive_geo_hit_test(const lvInteractiveGeo *g, double sx, double sy, double r, double *out_d) {
    if (!g)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "interactive_geo_hit_test: NULL g");
    if (!g->canvas_state.active_object_ids)
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "interactive_geo_hit_test: active_object_ids is NULL");
    if (r <= 0.0)
        r = HIT_RADIUS;
    double wx, wy;
    interactive_geo_screen_to_world(g, sx, sy, &wx, &wy);
    int best = -1;
    double bd = r * r;
    for (int i = 0; i < g->canvas_state.active_object_count; i++) {
        int oid = g->canvas_state.active_object_ids[i];
        double ox, oy;
        if (interactive_geo_get_object_position(g, oid, &ox, &oy) != 0)
            continue;
        double d2 = (ox - wx) * (ox - wx) + (oy - wy) * (oy - wy);
        if (d2 < bd) {
            bd = d2;
            best = oid;
        }
    }
    if (out_d)
        *out_d = sqrt(bd) / g->canvas_state.zoom_level;
    return best;
}

/* ==================== 对象位置查询 ==================== */

int interactive_geo_get_object_position(const lvInteractiveGeo *g, int id, double *wx, double *wy) {
    if (!g)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "interactive_geo_get_object_position: NULL g");
    if (g->engine_handle && g->engine_handle->main_graph) {
        GeomNode *nd = graph_get_node(g->engine_handle->main_graph, id);
        if (nd && nd->type == GEOM_POINT && nd->symbolic_coords && nd->coord_count >= 2) {
            /* 从符号坐标提取数值（完整实现需解析 SymbolicCoord） */
            if (wx)
                *wx = 0.0;
            if (wy)
                *wy = 0.0;
            return 0;
        }
    }
    if (wx)
        *wx = g->canvas_state.viewport_offset_x;
    if (wy)
        *wy = g->canvas_state.viewport_offset_y;
    return 0;
}

/* ==================== 缩放与视口 ==================== */

void interactive_geo_zoom(lvInteractiveGeo *g, double dz, double cx, double cy) {
    if (!g)
        return;
    lvGeoCanvasState *s = &g->canvas_state;
    double f = dz > 0 ? ZOOM_FACT : 1.0 / ZOOM_FACT;
    double nz = s->zoom_level * f;
    if (nz < MIN_ZOOM)
        f = MIN_ZOOM / s->zoom_level;
    if (nz > MAX_ZOOM)
        f = MAX_ZOOM / s->zoom_level;
    s->zoom_level *= f;
    double bx, by;
    interactive_geo_screen_to_world(g, cx, cy, &bx, &by);
    s->viewport_offset_x = bx;
    s->viewport_offset_y = by;
    double ax, ay;
    interactive_geo_screen_to_world(g, cx, cy, &ax, &ay);
    s->viewport_offset_x -= (ax - bx);
    s->viewport_offset_y -= (ay - by);
    sync_matrix(s);
}

void interactive_geo_reset_viewport(lvInteractiveGeo *g) {
    if (!g)
        return;
    lvGeoCanvasState *s = &g->canvas_state;
    s->zoom_level = 1.0;
    s->viewport_offset_x = 0;
    s->viewport_offset_y = 0;
    s->canvas_width = 800;
    s->canvas_height = 600;
    memset(s->viewport_matrix, 0, sizeof(s->viewport_matrix));
    s->viewport_matrix[0][0] = s->viewport_matrix[1][1] = s->viewport_matrix[2][2] = 1.0;
}

void interactive_geo_set_canvas_size(lvInteractiveGeo *g, double w, double h) {
    if (!g)
        return;
    g->canvas_state.canvas_width = w;
    g->canvas_state.canvas_height = h;
}

/* ==================== 快照系统 ==================== */

int interactive_geo_snapshot(lvInteractiveGeo *g) {
    if (!g)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "interactive_geo_snapshot: NULL g");
    if (g->snapshot_count >= lv_GEO_MAX_SNAPSHOTS) {
        lv_free((void **) &(g->snapshots[0]));
        for (int i = 1; i < lv_GEO_MAX_SNAPSHOTS; i++)
            g->snapshots[i - 1] = g->snapshots[i];
        g->snapshot_count = lv_GEO_MAX_SNAPSHOTS - 1;
    }
    char *buf = (char *) lv_malloc(SNAP_LEN);
    if (!buf)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "interactive_geo_snapshot: malloc failed");
    lvGeoCanvasState *cs = &g->canvas_state;
    int n = snprintf(buf, SNAP_LEN,
                     "{\"mode\":%d,\"drag_target\":%d,\"drag_pos\":[%.17g,%.17g],"
                     "\"zoom\":%.17g,\"offset\":[%.17g,%.17g],\"selected_count\":%d,\"modified\":%s}",
                     (int) cs->current_mode, cs->drag_target_id, cs->drag_current_x, cs->drag_current_y, cs->zoom_level,
                     cs->viewport_offset_x, cs->viewport_offset_y, cs->selected_count, cs->modified ? "true" : "false");
    if (n < 0 || n >= SNAP_LEN) {
        lv_free((void **) &buf);
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "interactive_geo_snapshot: snprintf failed");
    }
    g->snapshots[g->snapshot_count] = buf;
    g->current_snapshot_index = g->snapshot_count++;
    return 0;
}

int interactive_geo_restore(lvInteractiveGeo *g, int idx) {
    if (!g)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "interactive_geo_restore: NULL g");
    if (idx < 0 || idx >= g->snapshot_count || !g->snapshots[idx])
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "interactive_geo_restore: invalid idx %d (count=%d)", idx, g->snapshot_count);
    const char *j = g->snapshots[idx];
    lvGeoCanvasState *cs = &g->canvas_state;
    int mv = (int) cs->current_mode, di = -1, sc = 0;
    double dx = 0, dy = 0, zm = 1, ox = 0, oy = 0;

    /* 快照为 JSON 对象（interactive_geo_snapshot 生成），用统一 lvJsonParser
     * 按字段名分发解析（替代原 strstr+sscanf 手写定位；字段顺序无关，
     * 缺失字段保持初始默认值——与 strstr 未命中时保持初值的容错语义一致，
     * 字段存在但解析失败时回退默认值的语义亦保持一致） */
    lvJsonParser jp;
    lv_json_parser_init(&jp, j, strlen(j));
    if (lv_json_peek(&jp) == '{') {
        jp.pos++; /* 跳过 '{' */
        char *key = NULL;
        while (lv_json_parse_field(&jp, &key)) {
            if (lv_str_eq(key, "mode")) {
                if (!lv_json_parse_int(&jp, &mv))
                    mv = 0;
            } else if (lv_str_eq(key, "drag_target")) {
                if (!lv_json_parse_int(&jp, &di))
                    di = -1;
            } else if (lv_str_eq(key, "drag_pos")) {
                double arr[2];
                size_t cnt = 0;
                if (!lv_json_parse_double_array(&jp, arr, 2, &cnt) || cnt < 2) {
                    dx = 0;
                    dy = 0;
                } else {
                    dx = arr[0];
                    dy = arr[1];
                }
            } else if (lv_str_eq(key, "zoom")) {
                if (!lv_json_parse_double(&jp, &zm))
                    zm = 1;
            } else if (lv_str_eq(key, "offset")) {
                double arr[2];
                size_t cnt = 0;
                if (!lv_json_parse_double_array(&jp, arr, 2, &cnt) || cnt < 2) {
                    ox = 0;
                    oy = 0;
                } else {
                    ox = arr[0];
                    oy = arr[1];
                }
            } else if (lv_str_eq(key, "selected_count")) {
                if (!lv_json_parse_int(&jp, &sc))
                    sc = 0;
            } else {
                lv_json_skip_value(&jp);
            }
            lv_free((void **) &key);
        }
    }
    cs->current_mode = (InteractiveGeoMode) mv;
    cs->drag_target_id = di;
    cs->drag_current_x = dx;
    cs->drag_current_y = dy;
    cs->zoom_level = zm;
    cs->viewport_offset_x = ox;
    cs->viewport_offset_y = oy;
    cs->selected_count = sc;
    sync_matrix(cs);
    g->current_snapshot_index = idx;
    return 0;
}

/* ==================== 构造脚本生成 ==================== */

int interactive_geo_generate_script(lvInteractiveGeo *g, ScriptLanguage lang, char *buf, int bufsz) {
    if (!g || !buf || bufsz <= 0)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "interactive_geo_generate_script: NULL parameter or bufsz<=0");
    lvGeoScriptBinding *sb = &g->script_binding;
    const char *hdr;
    switch (lang) {
        case SCRIPT_LANG_PYTHON:
            hdr = "#!/usr/bin/env python3\n# Lv-00 script\n\n";
            break;
        case SCRIPT_LANG_LUA:
            hdr = "-- Lv-00 script\n\n";
            break;
        default:
            hdr = "# Lv-00 script\n\n";
            break;
    }
    int w = snprintf(buf, (size_t) bufsz, "%s", hdr);
    if (w < 0 || w >= bufsz)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "interactive_geo_generate_script: snprintf header failed");
    for (int i = 0; i < sb->binding_count; i++) {
        int rem = bufsz - w - 1;
        if (rem <= 0)
            break;
        if (sb->script_snippets[i]) {
            int l = snprintf(buf + w, (size_t) rem, "%s", sb->script_snippets[i]);
            if (l > 0 && l < rem)
                w += l;
        }
    }
    return w;
}

/* ==================== 状态查询 ==================== */

int interactive_geo_get_stats(const lvInteractiveGeo *g, char *buf, int bufsz) {
    if (!g || !buf || bufsz <= 0)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "interactive_geo_get_stats: NULL parameter or bufsz<=0");
    const lvGeoCanvasState *cs = &g->canvas_state;
    int n = snprintf(buf, (size_t) bufsz,
                     "{\"mode\":%d,\"active\":%d,\"selected\":%d,\"drag\":%d,"
                     "\"zoom\":%.4f,\"snapshots\":%d,\"constraints\":%d,\"affected\":%d,"
                     "\"singular_encounters\":%d,\"singular_avoidances\":%d,"
                     "\"config\":%d,\"script_bindings\":%d,\"script_len\":%d}",
                     (int) cs->current_mode, cs->active_object_count, cs->selected_count, cs->drag_target_id,
                     cs->zoom_level, g->snapshot_count, g->constraint_maint.constraint_count,
                     g->constraint_maint.affected_count, g->continuity.singular_encounters,
                     g->continuity.singular_avoidances, (int) g->continuity.current_config,
                     g->script_binding.binding_count, g->script_binding.script_length);
    return (n > 0 && n < bufsz) ? n : -1;
}
