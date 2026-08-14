/**
 * @file node_deep_copy.c
 * @brief 几何节点深拷贝公共实现
 * @details 提供统一的节点、端口和符号坐标深拷贝函数，
 *          消除 engine.c、proof.c、rewrite.c 中的重复实现。
 *
 * 所有权语义说明：
 * - type_region 执行浅拷贝（指针赋值），所有权由 TypeSystem 统一管理。
 * - connected_to 指针置为 NULL，需调用者通过 ID 映射更新连接关系。
 * - symbolic_coords 执行深拷贝，所有权归新节点所有。
 *
 * 深拷贝策略覆盖四种坐标类型：
 * - RATIONAL: 通过 mpq_set 深拷贝，避免 mpz_get_si/mpz_get_ui 截断问题
 * - ALGEBRAIC: 通过 algebraic_create 重建，包含最小多项式和隔离区间
 * - QUADRATIC: 通过 mpq_set 深拷贝有理数分量，再调用 quadratic_create
 * - TRANSCENDENTAL: 通过 transcendental_create 重建，深拷贝表达式树
 *
 * 【2026-08 重构】节点类型特定数据（union data：port / region.boundary_segments /
 * func_block 三数组 / circle 标量）的深拷贝已收敛至 GeomNodeVTable::clone
 * （graph_node_alloc.c 各类型 vtable 的 *_clone 实现）。本文件的
 * kDataCopyHandlers 分发表（copy_port / copy_region / copy_circle /
 * copy_func_block 三数组手写拷贝）与 graph_node_alloc.c 的 clone 并行实现
 * 已删除，node_deep_copy_geom_node 委托 graph_node_deep_copy_detached
 * （内部创建临时图适配 clone 的"目标节点已存在"契约）。
 *
 * @author Lv-00 Project
 */

#include "lv/node_deep_copy.h"

#include <string.h>

#include "lv/constraint_graph.h" /* GeomNode type */

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"

#include "constraint_graph/graph_node_internal.h" /* graph_node_deep_copy_detached（同层 L3，相对 include） */

/* ============================================================
 * 符号坐标深拷贝
 * ============================================================ */

/**
 * @brief 深拷贝符号坐标
 *
 * 根据坐标类型执行相应的深拷贝策略：
 * - RATIONAL: 通过 mpq_set 深拷贝，避免 mpz_get_si/mpz_get_ui 截断问题
 * - ALGEBRAIC: 通过 algebraic_create 重建，包含最小多项式和隔离区间
 * - QUADRATIC: 通过 mpq_set 深拷贝有理数分量，再调用 quadratic_create
 * - TRANSCENDENTAL: 通过 transcendental_create 重建，深拷贝表达式树
 *
 * @param orig 原始符号坐标指针
 * @return 新分配的坐标副本，失败返回 NULL
 */

SymbolicCoord *node_deep_copy_symbolic_coord(const SymbolicCoord *orig) {
    if (!orig)
        return NULL;

    /* 委托给权威实现 symbolic_coord_copy()（kCoordOpsVTable 的 copy_data+copy_check
     * 双步契约，覆盖 RATIONAL/ALGEBRAIC/QUADRATIC/TRANSCENDENTAL 四类），
     * 消除两套坐标深拷贝并存。 */
    return symbolic_coord_copy(orig);
}

/* ============================================================
 * 端口深拷贝
 * ============================================================ */

/**
 * @brief 深拷贝端口
 *
 * 创建一个新的 Port，复制所有标量字段。注意：
 * - connected_to 指针被置为 NULL，后续需要通过 ID 映射更新。
 * - type_region 执行完整深拷贝（递归复制所有子类型和数组），
 *   并设置 owns_type_region = true 标记所有权归属。
 *
 * 【2026-05-24 更新：原技术债务已消除，三阶段方案全部实施完成。】
 *
 * @param orig 原始端口
 * @return 新分配的端口副本，失败返回 NULL
 */
Port *node_deep_copy_port(const Port *orig) {
    if (!orig)
        return NULL;

    Port *copy = lv_calloc(1, sizeof(Port));
    if (!copy)
        return NULL;

    /* 标量字段收敛至共享辅助 port_copy_fields（graph_node_internal.h，
     * 与 GeomNodeVTable::clone 槽 port_clone 共用同一字段拷贝逻辑）：
     * id/type/namespace_depth/parent_block_id/is_formal_param/is_polymorphic
     * 直接赋值；type_region 浅拷贝（指针赋值），所有权由 TypeSystem 统一管理 */
    port_copy_fields(copy, orig);
    copy->connected_to = NULL; /* 后续通过 ID 映射更新连接关系 */

    return copy;
}

/* ============================================================
 * 几何节点深拷贝
 * ============================================================ */

/**
 * @brief 深拷贝几何节点
 *
 * 创建指定节点及其所有持有数据的深拷贝副本。
 * 拷贝策略：
 * - 标量/增强字段：直接赋值（trust / is_active / lo_subtype / numeric_precision /
 *   namespace_depth / parent_block_id）
 * - numeric_assumption_declaration：通过 lv_strdup_safe 深拷贝字符串
 * - symbolic_coords 数组：深拷贝（委托 symbolic_coord_copy）
 * - union data（data.port / data.region.boundary_segments / data.func_block
 *   三数组 / data.circle 标量）：经 orig->vtable->clone 深拷贝
 *   （graph_node_deep_copy_detached 内部创建临时图适配 clone 契约）；
 *   指针类字段保留源引用，由调用方通过 ID 映射重绑定
 *
 * @param orig   原始节点
 * @param id_map 旧节点ID到新节点ID的映射（可为 NULL）
 * @return 深拷贝后的新节点，失败返回 NULL
 */

/* 类型特定数据（union data）的深拷贝已收敛至 GeomNodeVTable::clone
 * （graph_node_alloc.c 各类型 *_clone 实现）；原 kDataCopyHandlers 分发表
 * （copy_port / copy_region / copy_circle / copy_func_block）已删除。
 * node_deep_copy_geom_node 委托 graph_node_deep_copy_detached 完成全部
 * 深拷贝（外壳 + symbolic_coords + 增强字段 + vtable->clone）。 */

GeomNode *node_deep_copy_geom_node(const GeomNode *orig, const int *id_map) {
    if (!orig)
        return NULL;

    /* id_map 重映射语义保持原实现：id_map[0] != 0 时副本 ID 取映射值，否则保持源 ID */
    int new_id = (id_map && id_map[0] != 0) ? id_map[0] : orig->id;

    /* 委托 graph_node_deep_copy_detached：内部创建临时图适配 vtable->clone 的
     * "目标节点已存在"契约，深拷贝外壳/坐标/增强字段/union data 后返回游离副本 */
    return graph_node_deep_copy_detached(orig, new_id);
}
