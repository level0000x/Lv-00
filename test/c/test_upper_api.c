/**
 * @file test_upper_api.c
 * @brief 上层统一接口（lv_upper_api.h）单元测试
 *
 * 覆盖恢复实现的上层便捷接口族：
 * - ID 句柄分配（lv_upper_alloc_id 递增）
 * - visual_editor / view_synchronizer / text_code 生命周期与往返
 * - geom_evol / atp_backend 生命周期与错误路径（无效句柄返回负值）
 * - func_block_preset 包装族（init/count/exists/字段访问/cleanup）
 * - interop 导出（TikZ 内存导出；coq/lean/opml 显式 UNSUPPORTED）
 * - 元验证流水线（lv_upper_full_verify）
 *
 * @author Lv-00 Project
 */

#include <stdio.h>
#include <string.h>

#include "lv.h"
#include "lv/lv_upper_api.h"
#include "lv/interop.h" /* lv_interop_export_proof 插件分发 */
#include "lv/proof.h"   /* proof_navigator_create / destroy */
#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============================================================
 * 1. ID 句柄分配
 * ============================================================ */

static void test_upper_alloc_id(void) {
    int64_t a = lv_upper_alloc_id(NULL);
    int64_t b = lv_upper_alloc_id(NULL);
    lv_ASSERT(a >= 0);
    lv_ASSERT(b > a);
    lv_ASSERT(lv_upper_get_id_counter(NULL) > b);
}

/* ============================================================
 * 2. visual_editor 生命周期
 * ============================================================ */

static void test_visual_editor_lifecycle(void) {
    int64_t id = visual_editor_create(NULL);
    lv_ASSERT(id >= 0);
    /* 空编辑器无 block_graph：render/zoom 返回负错误码（INVALID_STATE）为合法语义；
       update 委托 reset（仅重置编辑器状态，不依赖 block_graph）返回 0 */
    lv_ASSERT(visual_editor_render(NULL, id) < 0);
    lv_ASSERT(visual_editor_update(NULL, id, 0, 10, 20) == 0);
    lv_ASSERT(visual_editor_zoom(NULL, id, 0) < 0);
    lv_ASSERT(visual_editor_destroy(NULL, id) == 0);
    lv_ASSERT(visual_editor_render(NULL, id) < 0);
    lv_ASSERT(visual_editor_destroy(NULL, id) < 0);
    lv_ASSERT(visual_editor_create(NULL) >= 0); /* 槽位可复用 */
    lv_ASSERT(visual_editor_render(NULL, -1) < 0); /* 非法句柄 */
}

/* ============================================================
 * 3. text_code 往返
 * ============================================================ */

static void test_text_code_roundtrip(void) {
    int64_t id = text_code_create(NULL);
    lv_ASSERT(id >= 0);
    lv_ASSERT(text_code_set_text(NULL, id, "hello") == 0);
    const char *got = text_code_get_text(NULL, id);
    lv_ASSERT(got != NULL);
    TEST_ASSERT_STR_EQ(got, "hello");
    lv_ASSERT(text_code_destroy(NULL, id) == 0);
    lv_ASSERT(text_code_set_text(NULL, id, "x") < 0);
    lv_ASSERT(text_code_set_text(NULL, 123456789, "x") < 0);
}

/* ============================================================
 * 4. view_synchronizer 生命周期
 * ============================================================ */

static void test_view_sync_lifecycle(void) {
    int64_t id = view_synchronizer_create(NULL);
    lv_ASSERT(id >= 0);
    lv_ASSERT(view_synchronizer_sync(NULL, id, 0, 1) == 0);
    lv_ASSERT(view_synchronizer_destroy(NULL, id) == 0);
    lv_ASSERT(view_synchronizer_sync(NULL, id, 0, 1) < 0);
}

/* ============================================================
 * 5. geom_evol 生命周期与错误路径
 * ============================================================ */

static void test_geom_evol_lifecycle(void) {
    int64_t id = geom_evol_create(NULL, 2);
    lv_ASSERT(id >= 0);
    lv_ASSERT(geom_evol_step(NULL, id, 1) >= 0);
    lv_ASSERT(geom_evol_destroy(NULL, id) == 0);
    lv_ASSERT(geom_evol_step(NULL, id, 1) < 0);
    lv_ASSERT(geom_evol_create(NULL, 0) < 0);
    lv_ASSERT(geom_evol_create(NULL, 100000) < 0);
}

/* ============================================================
 * 6. atp_backend 生命周期与错误路径
 * ============================================================ */

static void test_atp_backend_lifecycle(void) {
    int64_t id = atp_backend_create(NULL, "vampire");
    lv_ASSERT(id >= 0);
    lv_ASSERT(atp_backend_destroy(NULL, id) == 0);
    lv_ASSERT(atp_backend_destroy(NULL, id) < 0);
    lv_ASSERT(atp_backend_result(NULL, 123456789) == -2);
}

/* ============================================================
 * 7. func_block_preset 包装族
 * ============================================================ */

static void test_preset_wrappers(void) {
    /* 成功返回 0（registry_init 失败时返回负错误码） */
    lv_ASSERT(func_block_preset_init(NULL) == 0);
    int64_t total = upper_func_block_preset_count(NULL);
    lv_ASSERT(total > 0);
    lv_ASSERT(upper_func_block_preset_exists(NULL, "perpendicular_bisector") == 1);
    lv_ASSERT(upper_func_block_preset_exists(NULL, "no_such_preset_xyz") == 0);
    lv_ASSERT(func_block_preset_input_count(NULL, "perpendicular_bisector") >= 0);
    lv_ASSERT(func_block_preset_output_count(NULL, "perpendicular_bisector") >= 0);
    lv_ASSERT(func_block_preset_input_count(NULL, NULL) < 0);
    /* 名称/枚举字符串访问器（真实查表/metadata，非占位） */
    lv_ASSERT(strcmp(func_block_preset_category_name(NULL, 0), "CONSTRUCTION") == 0);
    lv_ASSERT(strcmp(func_block_preset_category_name(NULL, 999), "UNKNOWN") == 0);
    lv_ASSERT(strcmp(func_block_preset_param_type_name(NULL, 0), "POINT") == 0);
    lv_ASSERT(strcmp(func_block_preset_complexity_name(NULL, 2), "O(n)") == 0);
    const char *desc = func_block_preset_description(NULL, "perpendicular_bisector");
    lv_ASSERT(desc != NULL && desc[0] != '\0');
    const char *ver = func_block_preset_version(NULL, "perpendicular_bisector");
    lv_ASSERT(ver != NULL && strchr(ver, '.') != NULL); /* "x.y.z" 格式（内置预设版本字段可能为 0） */
    const char *defn = func_block_preset_definition(NULL, "perpendicular_bisector");
    lv_ASSERT(defn != NULL && defn[0] != '\0');
    lv_ASSERT(func_block_preset_inverse_name(NULL, "no_such_preset_xyz") == NULL);
    lv_ASSERT(strcmp(func_block_preset_default_value(NULL, "no_such_preset_xyz", 0), "N/A") == 0);
    /* registration_time 无字段支撑：显式不支持（非模拟固定值） */
    lv_ASSERT(func_block_preset_registration_time(NULL, "perpendicular_bisector") < 0);

    /* metadata / bindings JSON 生成路径：C-⑱-补 修复 lv_free 传值误用缺陷族
     * （4 处 _js 传值 → 取址），此处以内存差值断言钉住修复不得泄漏 */
    {
        MemoryStats ms_before;
        lv_get_memory_stats(&ms_before);
        char meta_buf[4096];
        int64_t meta_len = func_block_preset_metadata(NULL, "perpendicular_bisector", meta_buf,
                                                      (int64_t) sizeof(meta_buf));
        lv_ASSERT(meta_len > 0);
        lv_ASSERT(strstr(meta_buf, "\"name\"") != NULL);
        char bind_buf[2048];
        int64_t bind_len = func_block_preset_bindings(NULL, 1, bind_buf, (int64_t) sizeof(bind_buf));
        lv_ASSERT(bind_len > 0);
        MemoryStats ms_after;
        lv_get_memory_stats(&ms_after);
        lv_ASSERT(ms_after.current_used <= ms_before.current_used + 4096);
    }

    lv_ASSERT(func_block_preset_cleanup(NULL) == 0);
}

/* ============================================================
 * 8. interop 导出（TikZ 内存导出 + 显式 UNSUPPORTED）
 * ============================================================ */

static void test_interop_export(void) {
    lvEngine *engine = engine_create();
    lv_ASSERT(engine != NULL);
    ConstraintGraph *graph = engine_get_main_graph(engine);
    lv_ASSERT(graph != NULL);

    /* 添加一个点使图非空 */
    SymbolicCoord *coords[2];
    coords[0] = symbolic_coord_create_rational(1, 1);
    coords[1] = symbolic_coord_create_rational(2, 1);
    lv_ASSERT(coords[0] && coords[1]);
    AddNodeResult ar = graph_add_point(graph, coords, 2);
    symbolic_coord_destroy(coords[0]);
    symbolic_coord_destroy(coords[1]);
    lv_ASSERT(ar == ADD_NODE_OK);

    char buf[8192];
    int64_t n = upper_interop_export_tikz(engine, 0, buf, (int64_t) sizeof(buf));
    lv_ASSERT(n > 0);
    lv_ASSERT(strstr(buf, "tikzpicture") != NULL);

    n = upper_interop_export_tikz(NULL, 0, buf, (int64_t) sizeof(buf));
    lv_ASSERT(n < 0);

    n = upper_interop_export_coq(engine, 0, buf, (int64_t) sizeof(buf));
    lv_ASSERT(n > 0);
    lv_ASSERT(strstr(buf, "Proof") != NULL || strstr(buf, "Theorem") != NULL || strstr(buf, "theorem") != NULL);
    n = interop_export_lean4(engine, 0, buf, (int64_t) sizeof(buf));
    lv_ASSERT(n > 0);
    n = interop_export_opml(engine, 0, buf, (int64_t) sizeof(buf));
    lv_ASSERT(n > 0);
    lv_ASSERT(strstr(buf, "opml_version") != NULL);

    n = upper_interop_export_coq(NULL, 0, buf, (int64_t) sizeof(buf));
    lv_ASSERT(n < 0);
    n = interop_export_opml(NULL, 0, buf, (int64_t) sizeof(buf));
    lv_ASSERT(n < 0);

    /* 插件体系分发：未注册插件显式 NOT_FOUND（负值）；NULL 参数校验 */
    ProofNavigator *nav = proof_navigator_create(NULL, engine);
    lv_ASSERT(nav != NULL);
    lv_ASSERT(lv_interop_export_proof("no_such_plugin", nav, buf, (int) sizeof(buf)) < 0);
    lv_ASSERT(lv_interop_export_proof(NULL, nav, buf, (int) sizeof(buf)) < 0);
    lv_ASSERT(lv_interop_export_proof("opml", NULL, buf, (int) sizeof(buf)) < 0);
    proof_navigator_destroy(nav);

    engine_destroy(engine);
}

/* ============================================================
 * 9. 元验证流水线
 * ============================================================ */

static void test_full_verify(void) {
    lvEngine *engine = engine_create();
    lv_ASSERT(engine != NULL);
    lv_ASSERT(lv_upper_full_verify(engine) == 1);
    lv_ASSERT(lv_upper_full_verify(NULL) < 0);
    engine_destroy(engine);
}

/* ============================================================
 * main
 * ============================================================ */

TEST_MAIN_BEGIN("Upper Unified API")
    lv_init();
    TEST_MAIN_RUN(test_upper_alloc_id);
    TEST_MAIN_RUN(test_visual_editor_lifecycle);
    TEST_MAIN_RUN(test_text_code_roundtrip);
    TEST_MAIN_RUN(test_view_sync_lifecycle);
    TEST_MAIN_RUN(test_geom_evol_lifecycle);
    TEST_MAIN_RUN(test_atp_backend_lifecycle);
    TEST_MAIN_RUN(test_preset_wrappers);
    TEST_MAIN_RUN(test_interop_export);
    TEST_MAIN_RUN(test_full_verify);
    lv_cleanup();
TEST_MAIN_END()
