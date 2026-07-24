/**
 * @file test_layer5_output_ops.c
 * @brief Layer5 输出层操作测试
 *
 * 覆盖 TikZ 导出、证明编译器、插件系统的基础 API。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv.h"
#include "plugin_system.h"
#include "proof_compiler.h"
#include "tikz_export.h"

#define TEST(n) printf("  [TEST] %s ... ", n)
#define PASS()            \
    do {                  \
        printf("PASS\n"); \
        P++;              \
    } while (0)
#define FAIL(m)                  \
    do {                         \
        printf("FAIL: %s\n", m); \
        F++;                     \
    } while (0)

static int P = 0, F = 0;

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== layer5_output 操作测试 ===\n\n");

    /* ── 组 1: TikZ 导出 ── */
    printf("[组 1] TikZ 导出\n");
    {
        lv_init();
        lvEngine *e = lv_engine_create();
        if (!e) {
            FAIL("create");
            F = 99;
            goto g1_end;
        }

        lv_add_point(e, 0, 1, 0, 1);
        lv_add_point(e, 1, 1, 0, 1);
        lv_add_point(e, 0, 1, 1, 1);
        lv_add_line_segment(e, 0, 1);
        lv_add_line_segment(e, 1, 2);
        lv_add_line_segment(e, 2, 0);

        TEST("export_to_buffer_NULL安全");
        int r = lv_tikz_export(NULL, NULL, 0);
        if (r < 0)
            PASS();
        else
            FAIL("!负值");

        TEST("export_file_NULL安全");
        r = lv_tikz_export_file(NULL, "test.tex");
        if (r < 0)
            PASS();
        else
            FAIL("!负值");

        lv_engine_destroy(e);
    g1_end:
        lv_cleanup();
    }

    /* ── 组 2: 证明编译器 ── */
    printf("[组 2] 证明编译器\n");
    {
        TEST("proof_object 创建/销毁");
        lvProofObject *po = lv_proof_object_create();
        if (po) {
            lv_proof_object_destroy(po);
            PASS();
        } else
            FAIL("NULL");

        TEST("proof_object NULL安全");
        lv_proof_object_destroy(NULL);
        PASS();

        TEST("proof_object 初始状态");
        po = lv_proof_object_create();
        if (po && lv_proof_object_get_step_count(po) == 0 && !lv_proof_object_is_valid(po)) {
            lv_proof_object_destroy(po);
            PASS();
        } else {
            lv_proof_object_destroy(po);
            FAIL("!初始");
        }

        TEST("compiler 创建/销毁");
        lvCompilerConfig cfg = lv_compiler_config_default();
        lvProofCompiler *pc = lv_proof_compiler_create(&cfg);
        if (pc) {
            lv_proof_compiler_destroy(pc);
            PASS();
        } else
            FAIL("NULL");

        TEST("compiler NULL安全");
        lv_proof_compiler_destroy(NULL);
        PASS();

        TEST("compiler_config_default");
        cfg = lv_compiler_config_default();
        if (cfg.format == OUTPUT_FORMAT_TEXT)
            PASS();
        else
            FAIL("!TEXT");

        TEST("step_record 创建/销毁");
        lvProofStepRecord *sr = lv_proof_step_record_create();
        if (sr) {
            lv_proof_step_record_destroy(sr);
            PASS();
        } else
            FAIL("NULL");

        TEST("step_record NULL安全");
        lv_proof_step_record_destroy(NULL);
        PASS();

        /* ── compile 完整流程 ── */
        TEST("compile 完整流程");
        po = lv_proof_object_create();
        cfg = lv_compiler_config_default();
        pc = lv_proof_compiler_create(&cfg);
        char *out = lv_proof_compiler_compile(pc, po, NULL);
        if (out) {
            lv_free_ptr(out);
            lv_proof_compiler_destroy(pc);
            lv_proof_object_destroy(po);
            PASS();
        } else {
            lv_proof_compiler_destroy(pc);
            lv_proof_object_destroy(po);
            FAIL("!compile");
        }

        TEST("to_json 安全处理NULL");
        out = lv_proof_compiler_to_json(NULL, NULL);
        if (out)
            lv_free_ptr(out);
        PASS();

        TEST("to_text 安全处理NULL");
        out = lv_proof_compiler_to_text(NULL, "zh");
        if (out)
            lv_free_ptr(out);
        PASS();

        TEST("compile 带步骤");
        po = lv_proof_object_create();
        lvProofStepRecord *step = lv_proof_step_record_create();
        if (step) {
            step->step_id = 1;
            step->type = (ProofStepType) 0;
            int added = lv_proof_object_add_step(po, step);
            if (added >= 0) {
                cfg = lv_compiler_config_default();
                pc = lv_proof_compiler_create(&cfg);
                out = lv_proof_compiler_compile(pc, po, NULL);
                if (out) {
                    lv_free_ptr(out);
                    lv_proof_compiler_destroy(pc);
                    lv_proof_object_destroy(po);
                    PASS();
                } else {
                    lv_proof_compiler_destroy(pc);
                    lv_proof_object_destroy(po);
                    FAIL("!compile_with_steps");
                }
            } else {
                lv_proof_step_record_destroy(step);
                lv_proof_object_destroy(po);
                FAIL("!add_step");
            }
        } else {
            lv_proof_object_destroy(po);
            FAIL("!step_create");
        }
    }

    /* ── 组 3: 插件系统 ── */
    printf("[组 3] 插件系统\n");
    {
        TEST("config 创建/销毁");
        lvPluginConfig *pc = lv_plugin_config_create();
        if (pc) {
            lv_plugin_config_destroy(pc);
            PASS();
        } else
            FAIL("NULL");

        TEST("config NULL安全");
        lv_plugin_config_destroy(NULL);
        PASS();

        TEST("system 创建");
        lvPluginSystem *ps = lv_plugin_system_create(NULL);
        if (ps) {
            lv_plugin_system_destroy(ps);
            PASS();
        } else
            FAIL("NULL");

        TEST("system NULL安全");
        lv_plugin_system_destroy(NULL);
        PASS();

        TEST("find NULL 返回 NULL");
        lvPlugin *pl = lv_plugin_find(NULL, "test");
        if (pl == NULL)
            PASS();
        else
            FAIL("!NULL");

        TEST("get_all NULL 返回 NULL");
        int cnt = 0;
        lvPlugin **all = lv_plugin_get_all(NULL, &cnt);
        if (all == NULL && cnt == 0)
            PASS();
        else
            FAIL("!expected");

        TEST("get_by_type NULL 返回 NULL");
        lvPlugin **by_type = lv_plugin_get_by_type(NULL, 0, &cnt);
        if (by_type == NULL && cnt == 0)
            PASS();
        else
            FAIL("!expected");

        TEST("get_by_state NULL 返回 NULL");
        lvPlugin **by_state = lv_plugin_get_by_state(NULL, 0, &cnt);
        if (by_state == NULL && cnt == 0)
            PASS();
        else
            FAIL("!expected");

        TEST("plugin_is_active NULL=false");
        if (!lv_plugin_is_active(NULL))
            PASS();
        else
            FAIL("!false");

        TEST("plugin_get_state NULL=UNKNOWN");
        if (lv_plugin_get_state(NULL) == 0)
            PASS();
        else
            FAIL("!UNKNOWN");

        TEST("resolve_dependencies NULL=-1");
        if (lv_plugin_resolve_dependencies(NULL, NULL) < 0)
            PASS();
        else
            FAIL("!负值");

        TEST("check_dependencies NULL安全");
        (void) lv_plugin_check_dependencies(NULL); /* 不应崩溃 */
        PASS();

        TEST("broadcast_event NULL=-1");
        if (lv_plugin_broadcast_event(NULL, 0, NULL, (size_t) 0) < 0)
            PASS();
        else
            FAIL("!负值");

        TEST("register_interface NULL=-1");
        if (lv_plugin_register_interface(NULL, NULL) < 0)
            PASS();
        else
            FAIL("!负值");

        TEST("unregister_interface NULL=-1");
        if (lv_plugin_unregister_interface(NULL, "test") < 0)
            PASS();
        else
            FAIL("!负值");

        TEST("query_interface NULL=NULL");
        if (lv_plugin_query_interface(NULL, "test", 0) == NULL)
            PASS();
        else
            FAIL("!NULL");

        TEST("query_interfaces NULL=NULL");
        lvPluginInterface **ifaces = lv_plugin_query_interfaces(NULL, NULL, &cnt);
        if (ifaces == NULL && cnt == 0)
            PASS();
        else
            FAIL("!expected");

        TEST("send_event NULL=-1");
        if (lv_plugin_send_event(NULL, 0, 0, (size_t) 0) < 0)
            PASS();
        else
            FAIL("!负值");

        TEST("apply_config NULL=-1");
        if (lv_plugin_apply_config(NULL, NULL) < 0)
            PASS();
        else
            FAIL("!负值");
    }

    printf("\n=== %d passed, %d failed ===\n", P, F);
    return F > 0 ? 1 : 0;
}
