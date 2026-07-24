/**
 * @file test_geo_invariant.c
 * @brief Tests for the geometric invariant type module
 *
 * Tests cover:
 * - Invariant creation with various kinds
 * - Consistency checking (valid and invalid cases)
 * - Type region attachment
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "geo_invariant_type.h"
#include "lv.h"
#include "test_helpers.h"

/* Global test counters */
int g_pass_count = 0;
int g_fail_count = 0;

/* ========================================================================
 * Test: Create invariant
 * ======================================================================== */

void test_geo_invariant_create(void) {
    printf("Testing geo_invariant_create...\n");

    /* Create a distance invariant */
    int entities[] = {1, 2};
    GeoInvariant *dist = geo_invariant_create(GEO_INV_DISTANCE, "test_distance", 5.0, 0.95, entities, 2);
    TEST_ASSERT_NOT_NULL(dist);
    TEST_ASSERT(dist->kind == GEO_INV_DISTANCE, "Kind should be DISTANCE");
    TEST_ASSERT(dist->entity_count == 2, "Entity count should be 2");
    TEST_ASSERT(dist->entity_ids[0] == 1, "First entity ID should be 1");
    TEST_ASSERT(dist->entity_ids[1] == 2, "Second entity ID should be 2");

    /* Create with NULL name (should use default) */
    GeoInvariant *angle = geo_invariant_create(GEO_INV_ANGLE, NULL, 90.0, 1.0, NULL, 0);
    TEST_ASSERT_NOT_NULL(angle);
    TEST_ASSERT(angle->name != NULL, "Default name should not be NULL");
    TEST_ASSERT(strcmp(angle->name, "angle") == 0, "Default name for ANGLE should be 'angle'");
    TEST_ASSERT(angle->entity_count == 0, "Entity count should be 0");
    TEST_ASSERT(angle->entity_ids == NULL, "Entity IDs should be NULL when count is 0");

    /* Create with custom name */
    GeoInvariant *area = geo_invariant_create(GEO_INV_AREA, "triangle_area", 6.0, 0.8, NULL, 0);
    TEST_ASSERT_NOT_NULL(area);
    TEST_ASSERT(strcmp(area->name, "triangle_area") == 0, "Custom name should be preserved");

    geo_invariant_destroy(dist);
    geo_invariant_destroy(angle);
    geo_invariant_destroy(area);
    printf("  PASSED\n");
}

/* ========================================================================
 * Test: Consistency checking
 * ======================================================================== */

void test_geo_invariant_consistency(void) {
    printf("Testing geo_invariant_check_consistency...\n");

    /* Valid distance invariant */
    GeoInvariant *valid_dist = geo_invariant_create(GEO_INV_DISTANCE, "valid_dist", 10.0, 0.9, NULL, 0);
    TEST_ASSERT(geo_invariant_check_consistency(valid_dist), "Valid distance invariant should pass consistency check");

    /* Invalid: negative distance */
    GeoInvariant *neg_dist = geo_invariant_create(GEO_INV_DISTANCE, "neg_dist", -5.0, 0.9, NULL, 0);
    TEST_ASSERT(!geo_invariant_check_consistency(neg_dist), "Negative distance should fail consistency check");

    /* Invalid: trust out of range (> 1.0) */
    GeoInvariant *bad_trust = geo_invariant_create(GEO_INV_ANGLE, "bad_trust", 45.0, 1.5, NULL, 0);
    TEST_ASSERT(!geo_invariant_check_consistency(bad_trust), "Trust > 1.0 should fail consistency check");

    /* Invalid: trust out of range (< 0.0) */
    GeoInvariant *neg_trust = geo_invariant_create(GEO_INV_ANGLE, "neg_trust", 45.0, -0.5, NULL, 0);
    TEST_ASSERT(!geo_invariant_check_consistency(neg_trust), "Trust < 0.0 should fail consistency check");

    /* Valid: zero distance (degenerate case) */
    GeoInvariant *zero_dist = geo_invariant_create(GEO_INV_DISTANCE, "zero_dist", 0.0, 1.0, NULL, 0);
    TEST_ASSERT(geo_invariant_check_consistency(zero_dist), "Zero distance should pass consistency check");

    /* Valid: parallelism (boolean-like, 0 or 1) */
    GeoInvariant *parallel = geo_invariant_create(GEO_INV_PARALLELISM, "is_parallel", 1.0, 1.0, NULL, 0);
    TEST_ASSERT(geo_invariant_check_consistency(parallel), "Parallelism = 1.0 should pass consistency check");

    /* Invalid: parallelism out of range */
    GeoInvariant *bad_parallel = geo_invariant_create(GEO_INV_PARALLELISM, "bad_parallel", 2.0, 0.5, NULL, 0);
    TEST_ASSERT(!geo_invariant_check_consistency(bad_parallel), "Parallelism = 2.0 should fail consistency check");

    /* NULL invariant */
    TEST_ASSERT(!geo_invariant_check_consistency(NULL), "NULL invariant should fail consistency check");

    geo_invariant_destroy(valid_dist);
    geo_invariant_destroy(neg_dist);
    geo_invariant_destroy(bad_trust);
    geo_invariant_destroy(neg_trust);
    geo_invariant_destroy(zero_dist);
    geo_invariant_destroy(parallel);
    geo_invariant_destroy(bad_parallel);
    printf("  PASSED\n");
}

/* ========================================================================
 * Test: Attach to type region
 * ======================================================================== */

void test_geo_invariant_attach(void) {
    printf("Testing geo_invariant_attach_to_type...\n");

    GeoInvariant *inv = geo_invariant_create(GEO_INV_DISTANCE, "edge_length", 7.0, 0.99, NULL, 0);
    TEST_ASSERT_NOT_NULL(inv);

    /* Attach to a type region */
    int rc = geo_invariant_attach_to_type(inv, 42, "triangle_edge");
    TEST_ASSERT(rc == 0, "Attach should succeed");
    TEST_ASSERT(inv->metadata != NULL, "Metadata should be set after attach");

    /* Verify metadata content */
    TEST_ASSERT(strstr(inv->metadata, "\"type_id\": 42") != NULL, "Metadata should contain type_id");
    TEST_ASSERT(strstr(inv->metadata, "\"region\": \"triangle_edge\"") != NULL, "Metadata should contain region name");

    /* Re-attach should overwrite */
    rc = geo_invariant_attach_to_type(inv, 99, "quad_edge");
    TEST_ASSERT(rc == 0, "Re-attach should succeed");
    TEST_ASSERT(strstr(inv->metadata, "\"type_id\": 99") != NULL, "Metadata should be updated after re-attach");

    /* NULL invariant should fail */
    rc = geo_invariant_attach_to_type(NULL, 1, "test");
    TEST_ASSERT(rc == -1, "NULL invariant should fail attach");

    /* NULL region name should fail */
    rc = geo_invariant_attach_to_type(inv, 1, NULL);
    TEST_ASSERT(rc == -1, "NULL region name should fail attach");

    /* Negative type_id should fail */
    rc = geo_invariant_attach_to_type(inv, -1, "test");
    TEST_ASSERT(rc == -1, "Negative type_id should fail attach");

    geo_invariant_destroy(inv);
    printf("  PASSED\n");
}

/* ========================================================================
 * Main
 * ======================================================================== */

int main(void) {
    TEST_SUITE_BEGIN("geo_invariant");

    TEST_RUN(test_geo_invariant_create);
    TEST_RUN(test_geo_invariant_consistency);
    TEST_RUN(test_geo_invariant_attach);

    TEST_SUITE_END();
    return g_fail_count > 0 ? 1 : 0;
}
