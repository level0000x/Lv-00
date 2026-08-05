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
 * 第11部分:L9 应用层(application: run/quick_verify/batch/get_version/destroy)
 *
 * 说明:lv_application_quick_verify/batch/destroy 经审计确认零外部
 * 调用，已按死代码删除。lv_application_run / lv_application_get_version
 * 不在删除清单，保留。
 * ============================================================ */

/** 应用层结构(前向声明 + 定义) */
typedef struct lvApplication {
    int64_t app_id;
    char *app_name;
    int64_t session_count;
    lvEngine *engine;
    lvOrchestrator *orch;
} lvApplication;

/** 运行应用 */
lvApplication *lv_application_run(lvEngine *ctx, const char *app_name) {
    (void) ctx;
    lvApplication *app = lv_calloc(1, sizeof(lvApplication));
    if (!app)
        lv_RETURN_ERROR_NULL(lv_ERROR_INTERNAL, "lv_application_run: calloc app failed");
    app->app_id = s_upper_state.upper_id++;
    app->app_name = lv_strdup_safe(app_name ? app_name : "default");
    if (!app->app_name) {
        lv_free((void **) &app);
        lv_RETURN_ERROR_NULL(lv_ERROR_INTERNAL, "lv_application_run: strdup app_name failed");
    }
    app->session_count = 0;
    app->engine = ctx;
    /* 创建编排器并执行默认管线 */
    app->orch = lv_orchestrator_create(ctx);
    if (!app->orch) {
        lv_free((void **) &app->app_name);
        lv_free((void **) &app);
        lv_RETURN_ERROR_NULL(lv_ERROR_INTERNAL, "lv_application_run: orchestrator_create failed");
    }
    return app;
}

/** 获取版本号字符串 */
const char *lv_application_get_version(lvEngine *ctx) {
    (void) ctx;
    return "Lv-00 v1.1.0 (GMP exact arithmetic)";
}
