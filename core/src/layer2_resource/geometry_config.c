#include "lv/geometry_config.h"

#include <string.h>

#include "lv/lv_thread.h"
#include "lv/cross_platform.h" /* lv_THREAD_LOCAL */

static lvGeometryConfig g_geometry_config;

/** @brief 全局几何配置快照（每线程一份，保证读端在锁外安全使用返回指针） */
static lv_THREAD_LOCAL lvGeometryConfig g_geometry_config_snapshot;

/** @brief 全局几何配置保护互斥锁（懒初始化，首次加锁时自动完成） */
lv_LAZY_LOCK_DEFINE(g_geometry_lock);

lvGeometryConfig lv_geometry_config_default(void) {
    lvGeometryConfig cfg;
    cfg.precision = lv_GEO_DOUBLE;
    cfg.tolerance = 1e-9;
    cfg.dimensions = 3;
    cfg.collinear_epsilon = 1e-9;
    cfg.distance_epsilon = 1e-9;
    cfg.perpendicular_epsilon = 1e-8;
    cfg.parallel_epsilon = 1e-8;
    cfg.angle_epsilon = 1e-6;
    cfg.singular_threshold = 1e-12;
    return cfg;
}

static lv_once_t g_geometry_config_once = lv_ONCE_INIT;

static void lv_geometry_config_init(void) {
    g_geometry_config = lv_geometry_config_default();
}

const lvGeometryConfig *lv_geometry_get_config(void) {
    lv_once(&g_geometry_config_once, lv_geometry_config_init);
    lv_lazy_lock_lock(&g_geometry_lock, g_geometry_lock_init_once);
    g_geometry_config_snapshot = g_geometry_config;
    lv_lazy_lock_unlock(&g_geometry_lock);
    return &g_geometry_config_snapshot;
}

void lv_geometry_set_config(const lvGeometryConfig *cfg) {
    if (cfg != NULL) {
        lv_lazy_lock_lock(&g_geometry_lock, g_geometry_lock_init_once);
        g_geometry_config = *cfg;
        lv_lazy_lock_unlock(&g_geometry_lock);
    }
}
