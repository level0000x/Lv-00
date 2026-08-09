/**
 * @file geo_invariant_type.c
 * @brief Implementation of geometric invariant types
 *
 * Provides creation, destruction, consistency checking, and
 * type-region attachment for geometric invariants with trust coloring.
 *
 * @version 1.0.0
 */

#include "geo_invariant_type.h"
#include "lv/lv_xmacro.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "lv/lv_json.h"
#include "lv_internal.h"
#include "lv_utils.h"

/* ========================================================================
 * Internal helpers
 * ======================================================================== */

/* ================================================================
 * 枚举 -> 值域 静态查找表（数据表化，替代 switch）
 * ================================================================ */

/** @brief 不变量类型期望值域表条目 */
typedef struct {
    double min; /**< 期望最小值 */
    double max; /**< 期望最大值 */
} InvariantRangeEntry;

/** @brief get_invariant_range 值域表（按枚举值升序，仅覆盖有特定值域的类型） */
static const InvariantRangeEntry s_invariant_range_table[] = {
    [GEO_INV_DISTANCE] = {0.0, 1e30},            /* practical upper bound */
    [GEO_INV_AREA] = {0.0, 1e30},                /* practical upper bound */
    [GEO_INV_VOLUME] = {0.0, 1e30},              /* practical upper bound */
    [GEO_INV_PERIMETER] = {0.0, 1e30},           /* practical upper bound */
    [GEO_INV_MOMENT_OF_INERTIA] = {0.0, 1e30},   /* practical upper bound */
    [GEO_INV_ANGLE] = {0.0, 360.0},              /* degrees */
    [GEO_INV_DIHEDRAL_ANGLE] = {0.0, 360.0},     /* degrees */
    [GEO_INV_SOLID_ANGLE] = {0.0, 360.0},        /* degrees */
    [GEO_INV_CROSS_RATIO] = {-1e30, 1e30},
    [GEO_INV_CURVATURE] = {-1e30, 1e30},
    [GEO_INV_TORSION] = {-1e30, 1e30},
    [GEO_INV_BARYCENTER] = {-1e30, 1e30},
    [GEO_INV_PARALLELISM] = {0.0, 1.0},          /* Boolean-like: 0.0 = false, 1.0 = true */
    [GEO_INV_ORTHOGONALITY] = {0.0, 1.0},        /* Boolean-like: 0.0 = false, 1.0 = true */
};

/**
 * @brief Get the expected value range for an invariant kind
 *
 * @param kind       Invariant kind
 * @param out_min    Output minimum value (may be -INFINITY)
 * @param out_max    Output maximum value (may be +INFINITY)
 */
static void get_invariant_range(GeoInvariantKind kind, double *out_min, double *out_max) {
    /* 查表获取期望值域；未知/越界类型回退到默认值域（原 default 分支） */
    if ((unsigned) kind < lv_ARRAY_SIZE(s_invariant_range_table)) {
        *out_min = s_invariant_range_table[kind].min;
        *out_max = s_invariant_range_table[kind].max;
    } else {
        *out_min = -1e30;
        *out_max = 1e30;
    }
}

/**
 * @brief Get a human-readable name for an invariant kind
 */
/* ================================================================
 * 枚举 -> 名称 映射表（数据表化，替代 switch）
 * ================================================================ */

/** @brief kind_default_name 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_kind_default_name_entries[] = {
    {"distance", GEO_INV_DISTANCE},
    {"angle", GEO_INV_ANGLE},
    {"area", GEO_INV_AREA},
    {"volume", GEO_INV_VOLUME},
    {"cross_ratio", GEO_INV_CROSS_RATIO},
    {"curvature", GEO_INV_CURVATURE},
    {"torsion", GEO_INV_TORSION},
    {"perimeter", GEO_INV_PERIMETER},
    {"dihedral_angle", GEO_INV_DIHEDRAL_ANGLE},
    {"solid_angle", GEO_INV_SOLID_ANGLE},
    {"barycenter", GEO_INV_BARYCENTER},
    {"moment_of_inertia", GEO_INV_MOMENT_OF_INERTIA},
    {"parallelism", GEO_INV_PARALLELISM},
    {"orthogonality", GEO_INV_ORTHOGONALITY},
};

static const char *kind_default_name(GeoInvariantKind kind) {
    return lv_enum_to_str(s_kind_default_name_entries, lv_ARRAY_SIZE(s_kind_default_name_entries), (int) kind, "unknown");
}

/* ========================================================================
 * Functions
 * ======================================================================== */

GeoInvariant *geo_invariant_create(GeoInvariantKind kind, const char *name, double value, double trust,
                                   const int *entity_ids, int entity_count) {
    GeoInvariant *inv = (GeoInvariant *) lv_malloc(sizeof(GeoInvariant));
    if (!inv)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "geo_invariant_create: malloc inv failed");
    memset(inv, 0, sizeof(GeoInvariant));

    inv->kind = kind;
    inv->value = value;
    inv->trust = trust;

    /* Do NOT clamp trust here; consistency check will validate the range.
     * Previously, clamping prevented geo_invariant_check_consistency from
     * detecting out-of-range trust values. */

    /* Copy name */
    if (name) {
        /* 手写 malloc+memcpy 收敛为 lv_strdup */
        inv->name = lv_strdup(name);
    } else {
        /* Use default name from kind */
        const char *def = kind_default_name(kind);
        inv->name = lv_strdup(def);
    }

    /* Copy entity IDs */
    if (entity_ids && entity_count > 0) {
        inv->entity_ids = (int *) lv_malloc(sizeof(int) * (size_t) entity_count);
        if (inv->entity_ids) {
            memcpy(inv->entity_ids, entity_ids, sizeof(int) * (size_t) entity_count);
            inv->entity_count = entity_count;
        }
    }

    return inv;
}

void geo_invariant_destroy(GeoInvariant *inv) {
    if (!inv)
        return;
    if (inv->name)
        lv_free((void **) &inv->name);
    if (inv->entity_ids)
        lv_free((void **) &inv->entity_ids);
    if (inv->metadata)
        lv_free((void **) &inv->metadata);
    lv_free((void **) &inv);
}

bool geo_invariant_check_consistency(const GeoInvariant *inv) {
    if (!inv)
        return false;

    /* Check trust range */
    if (inv->trust < 0.0 || inv->trust > 1.0)
        return false;

    /* Check value range for the kind */
    double min_val, max_val;
    get_invariant_range(inv->kind, &min_val, &max_val);

    if (inv->value < min_val - lv_EPSILON_HIGH || inv->value > max_val + lv_EPSILON_HIGH) {
        return false;
    }

    /* Check entity count is non-negative */
    if (inv->entity_count < 0)
        return false;

    /* If entity_count > 0, entity_ids must not be NULL */
    if (inv->entity_count > 0 && !inv->entity_ids)
        return false;

    return true;
}

int geo_invariant_attach_to_type(GeoInvariant *inv, int type_id, const char *region_name) {
    if (!inv || !region_name)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "geo_invariant_attach_to_type: NULL inv or region_name");
    if (type_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "geo_invariant_attach_to_type: type_id < 0");

    /* Store the type attachment as JSON metadata using lvJsonBuf */
    size_t region_len = strlen(region_name);
    lvJsonBuf buf;
    if (!lv_json_buf_init(&buf, 128 + region_len))
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "geo_invariant_attach_to_type: lv_json_buf_init failed");

    /* region 名经 append_string 自动 JSON 转义，防引号/控制字符注入 */
    lv_json_buf_append_raw(&buf, "{\"type_id\": ");
    lv_json_buf_append_fmt(&buf, "%d", type_id);
    lv_json_buf_append_raw(&buf, ", \"region\": ");
    lv_json_buf_append_string(&buf, region_name);
    lv_json_buf_append_raw(&buf, "}");

    char *meta = lv_json_buf_finalize(&buf);
    if (!meta)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "geo_invariant_attach_to_type: lv_json_buf_finalize failed");

    /* Free existing metadata if any */
    if (inv->metadata) {
        lv_free((void **) &inv->metadata);
    }

    inv->metadata = meta;
    return 0;
}
