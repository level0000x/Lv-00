/**
 * @file test_interactive_geo.c
 * @brief 交互几何系统 (interactive_geo) 单元测试
 *
 * 测试内容：
 * - 交互几何上下文生命周期
 * - 模式设置/获取
 * - 对象选择/取消选择
 * - 世界/屏幕坐标变换
 * - NULL 输入安全性
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "interactive_geo.h"
#include "lv.h"
#include "test_helpers.h"

/* 全局测试计数器 */
int g_pass_count = 0;
int g_fail_count = 0;

/* ================================================================
 * 测试 1: 交互几何上下文创建与销毁生命周期
 * ================================================================ */
void test_geo_lifecycle(void) {
    printf("  TEST: interactive_geo_init/destroy lifecycle...\n");

    /* 使用 NULL engine 句柄创建（延迟绑定） */
    lvInteractiveGeo *geo = interactive_geo_init(NULL);
    TEST_ASSERT(geo != NULL, "interactive_geo_init(NULL) returns non-NULL");
    TEST_ASSERT(geo->canvas_state.current_mode == GEO_MODE_SELECT, "default mode is GEO_MODE_SELECT");
    TEST_ASSERT(geo->engine_handle == NULL, "engine_handle is NULL");

    /* 销毁 */
    interactive_geo_destroy(geo);

    /* NULL safety on destroy */
    interactive_geo_destroy(NULL);

    printf("  PASS: lifecycle\n");
}

/* ================================================================
 * 测试 2: 模式设置/获取
 * ================================================================ */
void test_geo_mode(void) {
    printf("  TEST: interactive_geo_set_mode/get_mode...\n");

    lvInteractiveGeo *geo = interactive_geo_init(NULL);
    TEST_ASSERT(geo != NULL, "init succeeds");

    /* 默认模式 */
    InteractiveGeoMode mode = interactive_geo_get_mode(geo);
    TEST_ASSERT(mode == GEO_MODE_SELECT, "default mode is SELECT");

    /* 设置各模式并验证 */
    interactive_geo_set_mode(geo, GEO_MODE_POINT);
    TEST_ASSERT(interactive_geo_get_mode(geo) == GEO_MODE_POINT, "set_mode to POINT works");

    interactive_geo_set_mode(geo, GEO_MODE_LINE);
    TEST_ASSERT(interactive_geo_get_mode(geo) == GEO_MODE_LINE, "set_mode to LINE works");

    interactive_geo_set_mode(geo, GEO_MODE_CIRCLE);
    TEST_ASSERT(interactive_geo_get_mode(geo) == GEO_MODE_CIRCLE, "set_mode to CIRCLE works");

    interactive_geo_set_mode(geo, GEO_MODE_SELECT);
    TEST_ASSERT(interactive_geo_get_mode(geo) == GEO_MODE_SELECT, "set_mode to SELECT works");

    /* 切换回同一模式（无操作） */
    interactive_geo_set_mode(geo, GEO_MODE_SELECT);
    TEST_ASSERT(interactive_geo_get_mode(geo) == GEO_MODE_SELECT, "set_mode same mode is no-op");

    interactive_geo_destroy(geo);

    /* NULL input */
    InteractiveGeoMode null_mode = interactive_geo_get_mode(NULL);
    TEST_ASSERT(null_mode == GEO_MODE_SELECT, "get_mode(NULL) returns SELECT (fallback)");

    interactive_geo_set_mode(NULL, GEO_MODE_POINT);
    TEST_ASSERT(1, "set_mode(NULL, ...) does not crash");

    printf("  PASS: mode\n");
}

/* ================================================================
 * 测试 3: 选择/取消选择
 * ================================================================ */
void test_geo_select_deselect(void) {
    printf("  TEST: interactive_geo_select/deselect...\n");

    lvInteractiveGeo *geo = interactive_geo_init(NULL);
    TEST_ASSERT(geo != NULL, "init succeeds");

    /* 手动设置活跃对象（没有 engine 时模拟） */
    geo->canvas_state.active_object_ids[0] = 1;
    geo->canvas_state.active_object_ids[1] = 2;
    geo->canvas_state.active_object_ids[2] = 3;
    geo->canvas_state.active_object_count = 3;

    /* 初始选中状态 */
    TEST_ASSERT(geo->canvas_state.selected_count == 0, "initial selected_count == 0");

    /* 选中活跃对象 */
    int rc = interactive_geo_select(geo, 1);
    TEST_ASSERT(rc == 0, "select active object returns 0");
    TEST_ASSERT(geo->canvas_state.selected_count == 1, "selected_count == 1 after select");
    TEST_ASSERT(geo->canvas_state.primary_selected_id == 1, "primary_selected_id == 1");

    /* 选不活跃的对象返回 -1 */
    rc = interactive_geo_select(geo, 99);
    TEST_ASSERT(rc == -1, "select inactive object returns -1");

    /* 追加选中 */
    rc = interactive_geo_select(geo, 2);
    TEST_ASSERT(rc == 0, "select second object returns 0");
    TEST_ASSERT(geo->canvas_state.selected_count == 2, "selected_count == 2");

    /* 重复选中同一对象（已是选中状态） */
    rc = interactive_geo_select(geo, 1);
    TEST_ASSERT(rc == 0, "re-select already-selected object returns 0");
    TEST_ASSERT(geo->canvas_state.selected_count == 2, "selected_count still 2");

    /* 取消选中单个对象 */
    interactive_geo_deselect(geo, 1);
    TEST_ASSERT(geo->canvas_state.selected_count == 1, "selected_count == 1 after deselect");
    TEST_ASSERT(geo->canvas_state.primary_selected_id == 2, "primary_selected_id updated to 2");

    /* 取消所有选中 */
    interactive_geo_deselect(geo, -1);
    TEST_ASSERT(geo->canvas_state.selected_count == 0, "selected_count == 0 after deselect all");
    TEST_ASSERT(geo->canvas_state.primary_selected_id == -1, "primary_selected_id == -1");

    interactive_geo_destroy(geo);

    /* NULL input */
    rc = interactive_geo_select(NULL, 0);
    TEST_ASSERT(rc == -1, "select(NULL, ...) returns -1");

    interactive_geo_deselect(NULL, 0);
    TEST_ASSERT(1, "deselect(NULL, ...) does not crash");

    printf("  PASS: select/deselect\n");
}

/* ================================================================
 * 测试 4: 世界/屏幕坐标变换
 * ================================================================ */
void test_geo_coord_transform(void) {
    printf("  TEST: world/screen coordinate transforms...\n");

    lvInteractiveGeo *geo = interactive_geo_init(NULL);
    TEST_ASSERT(geo != NULL, "init succeeds");

    /* 重置视口到默认状态 */
    interactive_geo_reset_viewport(geo);

    /* 默认参数: zoom=1.0, offset=(0,0), canvas=(800,600) */
    /* world(0,0) -> screen(400, 300) */
    double sx, sy;
    interactive_geo_world_to_screen(geo, 0.0, 0.0, &sx, &sy);
    TEST_ASSERT(fabs(sx - 400.0) < 1e-9, "world_to_screen(0,0).x == 400");
    TEST_ASSERT(fabs(sy - 300.0) < 1e-9, "world_to_screen(0,0).y == 300");

    /* screen(400, 300) -> world(0, 0) */
    double wx, wy;
    interactive_geo_screen_to_world(geo, 400.0, 300.0, &wx, &wy);
    TEST_ASSERT(fabs(wx - 0.0) < 1e-9, "screen_to_world(400,300).x == 0");
    TEST_ASSERT(fabs(wy - 0.0) < 1e-9, "screen_to_world(400,300).y == 0");

    /* 测试 world(100, 50) -> screen */
    interactive_geo_world_to_screen(geo, 100.0, 50.0, &sx, &sy);
    TEST_ASSERT(fabs(sx - 500.0) < 1e-9, "world_to_screen(100,0).x == 500");
    TEST_ASSERT(fabs(sy - 350.0) < 1e-9, "world_to_screen(0,50).y == 350");

    /* 设置自定义画布尺寸后再次测试 */
    interactive_geo_reset_viewport(geo);
    interactive_geo_set_canvas_size(geo, 1920.0, 1080.0);

    interactive_geo_world_to_screen(geo, 0.0, 0.0, &sx, &sy);
    TEST_ASSERT(fabs(sx - 960.0) < 1e-9, "updated canvas: world_to_screen(0,0).x == 960");
    TEST_ASSERT(fabs(sy - 540.0) < 1e-9, "updated canvas: world_to_screen(0,0).y == 540");

    /* NULL pointer safety in output arguments */
    interactive_geo_world_to_screen(geo, 1.0, 2.0, NULL, NULL);
    TEST_ASSERT(1, "world_to_screen with NULL output does not crash");

    interactive_geo_screen_to_world(geo, 1.0, 2.0, NULL, NULL);
    TEST_ASSERT(1, "screen_to_world with NULL output does not crash");

    interactive_geo_destroy(geo);

    /* NULL geo input */
    interactive_geo_world_to_screen(NULL, 1.0, 2.0, &sx, &sy);
    TEST_ASSERT(1, "world_to_screen(NULL, ...) does not crash");

    interactive_geo_screen_to_world(NULL, 1.0, 2.0, &wx, &wy);
    TEST_ASSERT(1, "screen_to_world(NULL, ...) does not crash");

    printf("  PASS: coordinate transforms\n");
}

/* ================================================================
 * 测试 5: NULL 输入安全性综合测试
 * ================================================================ */
void test_geo_null_safety(void) {
    printf("  TEST: NULL input safety for all public functions...\n");

    /* 已在各测试中覆盖的 NULL 输入 */
    /* interactive_geo_init — test_geo_lifecycle (NULL engine is valid) */
    /* interactive_geo_destroy — test_geo_lifecycle */
    /* interactive_geo_set_mode — test_geo_mode */
    /* interactive_geo_get_mode — test_geo_mode */
    /* interactive_geo_select — test_geo_select_deselect */
    /* interactive_geo_deselect — test_geo_select_deselect */
    /* interactive_geo_world_to_screen — test_geo_coord_transform */
    /* interactive_geo_screen_to_world — test_geo_coord_transform */
    /* interactive_geo_set_canvas_size — test_geo_coord_transform */
    /* interactive_geo_reset_viewport — test_geo_coord_transform */

    /* interactive_geo_drag_start */
    int rc = interactive_geo_drag_start(NULL, 0, 0.0, 0.0);
    TEST_ASSERT(rc == -1, "drag_start(NULL, ...) returns -1");

    /* interactive_geo_drag_move */
    ConstraintMaintainStatus cs = interactive_geo_drag_move(NULL, 0.0, 0.0);
    TEST_ASSERT(cs == CONSTRAINT_FAILED, "drag_move(NULL, ...) returns CONSTRAINT_FAILED");

    /* interactive_geo_drag_end */
    cs = interactive_geo_drag_end(NULL, 0.0, 0.0);
    TEST_ASSERT(cs == CONSTRAINT_FAILED, "drag_end(NULL, ...) returns CONSTRAINT_FAILED");

    /* interactive_geo_randomized_check */
    int rcc = interactive_geo_randomized_check(NULL, 0, 0.0, NULL, NULL);
    TEST_ASSERT(rcc == -1, "randomized_check(NULL, ...) returns -1");

    /* interactive_geo_hit_test */
    int hit = interactive_geo_hit_test(NULL, 0.0, 0.0, 0.0, NULL);
    TEST_ASSERT(hit == -1, "hit_test(NULL, ...) returns -1");

    /* interactive_geo_get_object_position */
    int pos = interactive_geo_get_object_position(NULL, 0, NULL, NULL);
    TEST_ASSERT(pos == -1, "get_object_position(NULL, ...) returns -1");

    /* interactive_geo_zoom */
    interactive_geo_zoom(NULL, 1.0, 0.0, 0.0);
    TEST_ASSERT(1, "zoom(NULL, ...) does not crash");

    /* interactive_geo_reset_viewport */
    interactive_geo_reset_viewport(NULL);
    TEST_ASSERT(1, "reset_viewport(NULL) does not crash");

    /* interactive_geo_set_canvas_size */
    interactive_geo_set_canvas_size(NULL, 800.0, 600.0);
    TEST_ASSERT(1, "set_canvas_size(NULL, ...) does not crash");

    printf("  PASS: NULL safety\n");
}

/* ================================================================
 * 主函数
 * ================================================================ */
int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    lv_init();

    printf("=== InteractiveGeo (交互几何系统) 单元测试 ===\n\n");

    test_geo_lifecycle();
    test_geo_mode();
    test_geo_select_deselect();
    test_geo_coord_transform();
    test_geo_null_safety();

    int total = g_pass_count + g_fail_count;
    printf("\n=== Result: %d passed, %d failed ===\n", g_pass_count, g_fail_count);

    return g_fail_count > 0 ? 1 : 0;
}
