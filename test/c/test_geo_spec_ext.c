/**
 * @file test_geo_spec_ext.c
 * @brief 几何规范解析契约测试（批次 C-㊺续37：geo_spec.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（2 个）：
 *   lv_geo_spec_parse / lv_geo_spec_destroy
 *
 * 契约要点（与 geo_spec.c 核对）：
 *   - parse：type=="point"/"Point" → 0（*out 为 lvGeoSpecPoint*）；
 *     "polygon"/"Polygon" → 1；NULL 参数 → 错误码；未知 → lv_ERROR_UNSUPPORTED。
 *   - destroy：NULL 安全；释放点/多边形结构。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>

#include "lv/geo_spec.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：点解析 ============== */

static void test_parse_point(void) {
    lvGeoSpecPoint *pt = NULL;
    int rc = lv_geo_spec_parse("{\"type\":\"point\",\"x\":1.5,\"y\":2.5}", &pt);
    TEST_ASSERT_EQ(rc, 0);
    TEST_ASSERT_NOT_NULL(pt);
    if (pt) {
        TEST_ASSERT_DOUBLE(pt->x, 1.5, 1e-12);
        TEST_ASSERT_DOUBLE(pt->y, 2.5, 1e-12);
    }
    lv_geo_spec_destroy(pt);
}

/* ============== 测试：多边形解析 ============== */

static void test_parse_polygon(void) {
    lvGeoSpecPolygon *poly = NULL;
    int rc = lv_geo_spec_parse("{\"type\":\"polygon\",\"count\":3,\"points\":[{\"x\":0,\"y\":0},{\"x\":1,\"y\":0},{\"x\":0,\"y\":1}]}",
                               &poly);
    TEST_ASSERT_EQ(rc, 1);
    TEST_ASSERT_NOT_NULL(poly);
    if (poly) {
        TEST_ASSERT_EQ(poly->count, 3);
        TEST_ASSERT_NOT_NULL(poly->pts);
    }
    lv_geo_spec_destroy(poly);
}

/* ============== 测试：NULL 与未知类型 ============== */

static void test_null_and_unknown(void) {
    void *out = NULL;

    /* NULL 参数 */
    TEST_ASSERT(lv_geo_spec_parse(NULL, &out) != 0, "NULL json");
    TEST_ASSERT(lv_geo_spec_parse("{\"type\":\"point\"}", NULL) != 0, "NULL out");

    /* 未知类型 */
    TEST_ASSERT(lv_geo_spec_parse("{\"type\":\"circle\",\"r\":1}", &out) != 0, "unknown type");

    /* destroy NULL 安全 */
    lv_geo_spec_destroy(NULL);
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("GeoSpecExt")

    printf("\n--- geo_spec (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_parse_point);
    TEST_MAIN_RUN(test_parse_polygon);
    TEST_MAIN_RUN(test_null_and_unknown);

TEST_MAIN_END()
