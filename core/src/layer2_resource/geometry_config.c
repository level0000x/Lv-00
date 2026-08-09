#include "lv/geometry_config.h"

#include <string.h>

#include "lv/lv_thread.h"
#include "lv/cross_platform.h" /* lv_THREAD_LOCAL */
#include "lv/config.h"         /* lv_GEO_*_EPSILON / lv_SINGULARITY_THRESHOLD 权威值 */

/* ============================================================
 * 与配置系统 A（lvConfig，config.h / lv_config.c）的关系
 *
 * 本文件是独立的几何配置子系统：lvGeometryConfig 结构体 + 全局实例 +
 * 每线程快照（读端锁外安全）+ 惰性互斥锁，公共 API 为
 * lv_geometry_get_config() / lv_geometry_set_config()（返回/接收整个结构体）。
 *
 * 配置系统 A 的 LV_CONFIG_DOUBLE_KEYS 中虽有部分几何相关键
 * （geo_min_zoom / geo_max_zoom / geo_sym_coord_eps 等），但两者
 * 语义与访问模式不同（A 为标量字段 + 字符串键分发；本模块为结构体整体
 * 读写 + 线程本地快照），并入 A 需把整个结构体嵌进 lvConfig 并迁移全部
 * lv_geometry_get_config() 调用点，收益低、风险高，故保持独立。
 * 如需在 A 的 JSON 配置中覆盖几何参数，可经 lv_geometry_set_config()
 * 在加载配置后显式同步（未实现，属已知差异）。
 * ============================================================ */

static lvGeometryConfig g_geometry_config;

/** @brief 全局几何配置快照（每线程一份，保证读端在锁外安全使用返回指针） */
static lv_THREAD_LOCAL lvGeometryConfig g_geometry_config_snapshot;

/** @brief 全局几何配置保护互斥锁（懒初始化，首次加锁时自动完成） */
lv_LAZY_LOCK_DEFINE(g_geometry_lock);

lvGeometryConfig lv_geometry_config_default(void) {
    lvGeometryConfig cfg;
    cfg.precision = lv_GEO_DOUBLE;
    cfg.tolerance = 1e-9; /* 通用容差：config.h 无对应常量，保持历史值（数值与 lv_GEO_COLLINEAR_EPSILON 一致） */
    cfg.dimensions = 3;
    /* 收敛说明（P0-2）：epsilon 默认值全部改为引用 config.h 权威常量，config.h 成为
     * 唯一默认值权威源。行为变更：distance_epsilon 由 1e-9 → lv_GEO_DISTANCE_EPSILON
     * （1e-8，差 10 倍）、angle_epsilon 由 1e-6 → lv_GEO_ANGLE_EPSILON（1e-10，
     * 差 1e4 倍），均对齐 config.h 权威值；perpendicular/parallel 历史上即为 1e-8，
     * config.h 无专用常量，借用 lv_GEO_DISTANCE_EPSILON（1e-8）保持数值不变。 */
    cfg.collinear_epsilon = lv_GEO_COLLINEAR_EPSILON;    /* 对齐 config.h 权威值（1e-9，与历史一致） */
    cfg.distance_epsilon = lv_GEO_DISTANCE_EPSILON;      /* 对齐 config.h 权威值（1e-8，行为变更：原 1e-9） */
    cfg.perpendicular_epsilon = lv_GEO_DISTANCE_EPSILON; /* 对齐 config.h 权威值（1e-8，与历史一致） */
    cfg.parallel_epsilon = lv_GEO_DISTANCE_EPSILON;      /* 对齐 config.h 权威值（1e-8，与历史一致） */
    cfg.angle_epsilon = lv_GEO_ANGLE_EPSILON;            /* 对齐 config.h 权威值（1e-10，行为变更：原 1e-6） */
    cfg.singular_threshold = lv_SINGULARITY_THRESHOLD;   /* 对齐 config.h 权威值（1e-12，与历史一致） */
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
