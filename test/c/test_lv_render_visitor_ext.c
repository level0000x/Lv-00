/**
 * @file test_lv_render_visitor_ext.c
 * @brief 渲染访问器契约测试（批次 C-㊺续35：lv_render_visitor.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（3 个）：
 *   lv_render_scene / lv_render_visitor_tikz_create / lv_render_visitor_tikz_destroy
 *
 * 契约要点（与 lv_render_visitor.c / lv_render_visitor_tikz.c 核对）：
 *   - render_scene：visitor/scene NULL → false；空场景成功 → true。
 *   - tikz_create：path/visitor NULL → false；成功填充全部回调与 user_data。
 *   - tikz_destroy：NULL/user_data NULL 安全。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>
#include <string.h>

#include "lv/geo_visual.h"
#include "lv/lv_render_visitor.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：render_scene NULL 契约与空场景 ============== */

static void test_render_scene(void) {
    /* NULL 契约 */
    TEST_ASSERT(!lv_render_scene(NULL, NULL), "both NULL");
    TEST_ASSERT(!lv_render_scene(NULL, (const lvVisualScene *) (void *) 1), "visitor NULL");

    /* 空场景：成功（begin + 无对象 + end） */
    lvVisualScene scene;
    memset(&scene, 0, sizeof(scene));
    scene.object_count = 0;
    scene.objects = NULL;

    lvRenderVisitor v;
    memset(&v, 0, sizeof(v));
    /* 计数回调 */
    int begin_calls = 0;
    (void) begin_calls;
    TEST_ASSERT(lv_render_scene(&v, &scene), "empty scene renders");
}

/* ============== 测试：TikZ 后端工厂 ============== */

static void test_tikz_factory(void) {
    /* NULL 契约 */
    TEST_ASSERT(!lv_render_visitor_tikz_create(NULL, NULL), "both NULL");
    TEST_ASSERT(!lv_render_visitor_tikz_create("x.tex", NULL), "visitor NULL");

    lvRenderVisitor v;
    memset(&v, 0, sizeof(v));

    /* 成功创建（写临时文件） */
    TEST_ASSERT(lv_render_visitor_tikz_create("render_visitor_tmp.tex", &v), "create ok");
    TEST_ASSERT_NOT_NULL(v.user_data);
    TEST_ASSERT_NOT_NULL(v.begin_scene);
    TEST_ASSERT_NOT_NULL(v.end_scene);
    TEST_ASSERT_NOT_NULL(v.visit_point);
    TEST_ASSERT_NOT_NULL(v.visit_segment);
    TEST_ASSERT_NOT_NULL(v.visit_circle);

    /* 用空场景渲染 */
    lvVisualScene scene;
    memset(&scene, 0, sizeof(scene));
    TEST_ASSERT(lv_render_scene(&v, &scene), "render with tikz");

    /* 销毁 */
    lv_render_visitor_tikz_destroy(&v);
    TEST_ASSERT_NULL(v.user_data);

    /* 再次销毁安全 */
    lv_render_visitor_tikz_destroy(&v);
    lv_render_visitor_tikz_destroy(NULL);

    remove("render_visitor_tmp.tex");
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("RenderVisitorExt")

    printf("\n--- lv_render_visitor (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_render_scene);
    TEST_MAIN_RUN(test_tikz_factory);

TEST_MAIN_END()
