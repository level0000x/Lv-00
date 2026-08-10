/*
 * @file lv_impl_upper_app.c
 * @brief Lv-00 upper unified impl - L9 application layer
 * @details Split from lv_impl_upper.c
 */

#include <gmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/atp_backend.h"
#include "lv/application.h"
#include "lv/conflict_detector.h"
#include "lv/constraint_graph.h"
#include "lv/dsl_compiler.h"
#include "lv/engine.h"
#include "lv/func_block.h"
#include "lv/func_block_preset.h"
#include "lv/func_block_registry.h"
#include "lv/geom_evol.h"
#include "lv/interop.h"
#include "lv/lv.h"
#include "lv/lv_file.h"
#include "lv/lv_json.h"
#include "lv/lv_utils.h"
#include "lv/meta_verify.h"
#include "lv/orchestrator.h"
#include "lv/preset_algebraic.h"
#include "lv/preset_basic_geometry.h"
#include "lv/preset_measurements.h"
#include "lv/preset_polygons.h"
#include "lv/preset_transformations.h"
#include "lv/tikz_export.h"
#include "lv/visual_editor.h"

#include "lv_internal.h" /* lv_RETURN_ERROR / lv_RETURN_ERROR_NULL */
#include "lv/lv_strbuf.h"
#include "lv_impl_upper_internal.h"

/* ============================================================
 * 第11部分:L9 应用层(application: run/quick_verify/batch/get_version/destroy)
 *
 * 实现说明:应用层封装 L7 编排会话（lv_orchestrator_*）与 L8 元验证
 * （lv_meta_verify_session）。五种命令:
 *   LOAD      -> 完整流水线运行
 *   VERIFY    -> 流水线 + 六项元验证检查
 *   BATCH     -> 批量 VERIFY
 *   EXPORT    -> 构建约束图并按格式写文件 (json/canonical/dot)
 *   VISUALIZE -> 构建约束图并渲染 TikZ 到文件
 * ============================================================ */

static int read_file_text(const char *path, char *buf, size_t buf_size) {
    if (!path || !buf || buf_size == 0)
        return -1;
    FILE *fp = lv_file_open(path, "rb");
    if (!fp)
        return -1;
    size_t n = fread(buf, 1, buf_size - 1, fp);
    lv_file_close(fp);
    buf[n] = '\0';
    return 0;
}

static int write_text_file(const char *path, const char *text) {
    if (!path || !text)
        return -1;
    return lv_file_write_all(path, text, strlen(text));
}

/* 从输入文件构建约束图（真实调用 layer1 + layer3） */
static ConstraintGraph *build_graph_from_file(const char *input_path) {
    if (!input_path || !input_path[0])
        return NULL;
    char src[8192];
    if (read_file_text(input_path, src, sizeof(src)) != 0)
        return NULL;
    ConstraintGraph *g = graph_create();
    if (!g)
        return NULL;
    DslCompileConfig cc;
    dsl_compile_config_default(&cc);
    if (!dsl_compile_and_load(src, &cc, g)) {
        graph_destroy(g);
        return NULL;
    }
    return g;
}

/* EXPORT：按格式导出约束图到文件 */
static int do_export(ConstraintGraph *g, const char *fmt, const char *out_path) {
    if (!g || !out_path || !out_path[0])
        return -1;
    if (!fmt || !fmt[0])
        fmt = "json";
    if (strcmp(fmt, "json") == 0) {
        char *j = graph_serialize_to_json(g);
        if (!j)
            return -1;
        int rc = write_text_file(out_path, j);
        lv_free((void **)&j);
        return rc;
    }
    if (strcmp(fmt, "canonical") == 0)
        return interop_export_canonical(g, out_path);
    if (strcmp(fmt, "dot") == 0) {
        DOTExportConfig dc = dot_export_config_default();
        int rc = graph_export_dot_file(g, &dc, out_path);
        return rc == lv_OK ? 0 : -1;
    }
    return -1;
}

/* 构建并运行会话，返回元验证通过数（6 项全过返回 1） */
static int run_verify_session(const lvApplicationConfig *config) {
    lvSessionConfig sc;
    lv_orchestrator_config_default(&sc);
    if (config->timeout_ms > 0)
        sc.timeout_ms = config->timeout_ms;
    if (config->max_reasoning_depth > 0)
        sc.max_reasoning_depth = config->max_reasoning_depth;
    sc.enable_visualization = config->enable_visualization;
    if (config->output_format && config->output_format[0])
        snprintf(sc.output_format, sizeof(sc.output_format), "%s", config->output_format);
    lvSession *s = lv_orchestrator_create(&sc);
    if (!s)
        return -1;
    int rc = lv_orchestrator_run(s, config->input_path);
    if (rc != 0) {
        const char *e = lv_orchestrator_last_error(s);
        if (e && e[0])
            fprintf(stderr, "[lv_application] orchestrator error: %s\n", e);
        lv_orchestrator_destroy(s);
        return -1;
    }
    lvMetaVerifier *v = lv_meta_verifier_create();
    if (!v) {
        lv_orchestrator_destroy(s);
        return -1;
    }
    lvVerifyReport rep = lv_meta_verify_session(v, s);
    lv_meta_verifier_destroy(v);
    lv_orchestrator_destroy(s);
    return lv_verify_report_passed(&rep);
}

int lv_application_run(const lvApplicationConfig *config) {
    if (!config)
        return -1;
    switch (config->command) {
    case LV_APP_CMD_LOAD: {
        lvSessionConfig sc;
        lv_orchestrator_config_default(&sc);
        if (config->timeout_ms > 0)
            sc.timeout_ms = config->timeout_ms;
        if (config->max_reasoning_depth > 0)
            sc.max_reasoning_depth = config->max_reasoning_depth;
        sc.enable_visualization = config->enable_visualization;
        if (config->output_format && config->output_format[0])
            snprintf(sc.output_format, sizeof(sc.output_format), "%s", config->output_format);
        lvSession *s = lv_orchestrator_create(&sc);
        if (!s)
            return -1;
        int rc = lv_orchestrator_run(s, config->input_path);
        lv_orchestrator_destroy(s);
        return rc;
    }
    case LV_APP_CMD_VERIFY:
        return run_verify_session(config);
    case LV_APP_CMD_BATCH:
        return lv_application_batch(config->batch_inputs, config->batch_count);
    case LV_APP_CMD_EXPORT: {
        ConstraintGraph *g = build_graph_from_file(config->input_path);
        if (!g)
            return -1;
        int rc = do_export(g, config->output_format, config->output_path);
        graph_destroy(g);
        return rc;
    }
    case LV_APP_CMD_VISUALIZE: {
        ConstraintGraph *g = build_graph_from_file(config->input_path);
        if (!g)
            return -1;
        if (!config->output_path || !config->output_path[0]) {
            graph_destroy(g);
            return -1;
        }
        char buf[16384];
        int n = lv_tikz_export((void *)g, buf, sizeof(buf));
        graph_destroy(g);
        if (n <= 0)
            return -1;
        return write_text_file(config->output_path, buf);
    }
    default:
        return -1;
    }
}

const char *lv_application_get_version(void) {
    return lv_VERSION_STRING;
}

int lv_application_quick_verify(const char *filepath) {
    if (!filepath || !filepath[0])
        return 0;
    lvApplicationConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.command = LV_APP_CMD_VERIFY;
    cfg.input_path = filepath;
    int r = run_verify_session(&cfg);
    return r > 0 ? 1 : 0;
}

int lv_application_batch(const char *const *filepaths, int count) {
    if (!filepaths || count <= 0)
        return -1;
    int passed = 0;
    for (int i = 0; i < count; i++) {
        int r = lv_application_quick_verify(filepaths[i]);
        if (r > 0)
            passed += r;
    }
    return passed;
}

void lv_application_shutdown(void) {
    /* 无全局状态，空实现保持 ABI 稳定 */
}
