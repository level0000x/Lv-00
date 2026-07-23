#ifndef lv_GEO_INVARIANT_TYPE_H
#define lv_GEO_INVARIANT_TYPE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

/* ── Invariant kind enum ── */
typedef enum {
    GEO_INV_DISTANCE = 0,
    GEO_INV_ANGLE,
    GEO_INV_AREA,
    GEO_INV_VOLUME,
    GEO_INV_CROSS_RATIO,
    GEO_INV_CURVATURE,
    GEO_INV_TORSION,
    GEO_INV_PERIMETER,
    GEO_INV_DIHEDRAL_ANGLE,
    GEO_INV_SOLID_ANGLE,
    GEO_INV_BARYCENTER,
    GEO_INV_MOMENT_OF_INERTIA,
    GEO_INV_PARALLELISM,
    GEO_INV_ORTHOGONALITY,
    GEO_INV_CONCURRENCY,
    GEO_INV_COLLINEARITY,
    GEO_INV_CONCYCLICITY
} GeoInvariantKind;

/* ── Legacy typedef for lvGeoInvariantType ── */
typedef GeoInvariantKind lvGeoInvariantType;
typedef GeoInvariantKind GeoInvariantType;

/* ── Main invariant struct ── */
typedef struct GeoInvariant {
    GeoInvariantKind kind;
    char *name;
    double value;
    double trust;
    int *entity_ids;
    int entity_count;
    char *metadata;
} GeoInvariant;

/* ── Legacy name ── */
typedef GeoInvariant lvGeoInvariant;

/* ── API ── */
/**
 * @brief 创建几何不变量对象
 *
 * @param kind 不变量类型
 * @param name 不变量名称
 * @param value 不变量数值
 * @param trust 信任度（0.0～1.0）
 * @param entity_ids 关联的几何实体 ID 数组
 * @param entity_count 实体数量
 * @return 成功返回 GeoInvariant 指针，失败返回 NULL
 */
GeoInvariant *geo_invariant_create(GeoInvariantKind kind, const char *name, double value, double trust,
                                   const int *entity_ids, int entity_count);
/**
 * @brief 销毁几何不变量对象，释放占用的内存
 *
 * @param inv 不变量对象指针
 */
void geo_invariant_destroy(GeoInvariant *inv);
/**
 * @brief 检查不变量数据的一致性与有效性
 *
 * @param inv 不变量对象指针
 * @return 数据一致返回 true，不一致返回 false
 */
bool geo_invariant_check_consistency(const GeoInvariant *inv);
/**
 * @brief 设置不变量对象的元数据信息
 *
 * @param inv 不变量对象指针
 * @param meta 元数据字符串
 */
void geo_invariant_set_metadata(GeoInvariant *inv, const char *meta);
/**
 * @brief 获取不变量对象的元数据字符串
 *
 * @param inv 不变量对象指针
 * @return 元数据字符串指针，如果无元数据返回 NULL
 */
const char *geo_invariant_get_metadata(const GeoInvariant *inv);
/**
 * @brief 将不变量对象序列化为 JSON 字符串
 *
 * @param inv 不变量对象指针
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return 实际写入的字符数，失败返回负数
 */
int geo_invariant_to_json(const GeoInvariant *inv, char *buf, size_t buf_size);
/**
 * @brief 将不变量附加到指定的几何类型与区域
 *
 * @param inv 不变量对象指针
 * @param type_id 目标几何类型 ID
 * @param region_name 目标区域名称
 * @return 附加成功返回 0，失败返回负数
 */
int geo_invariant_attach_to_type(GeoInvariant *inv, int type_id, const char *region_name);

/* ── Legacy alias ── */
#define lv_geo_invariant_check(inv, pts, dim) (geo_invariant_check_consistency(inv) ? 1 : 0)

#ifdef __cplusplus
}
#endif
#endif
