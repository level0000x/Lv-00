/**
 * @file unify_helpers.c
 * @brief internal helpers and port matching
 * @details Split from unify.c
 */

#include "unify.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/geometric_primitives.h"
#include "lv/proof.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "normalization.h"
#include "stream.h"
#include "stream_context_util.h"
#include "type_system.h"
#include "lv/lv_strbuf.h"
#include "unify_internal.h"

/* ---------------------------------------------------------------------------
 * 内部辅助函数
 * ------------------------------------------------------------------------- */

/**
 * @brief 比较两个节点的符号坐标是否完全相等
 *
 * 支持多种几何类型的比较：
 *   - GEOM_POINT: 逐个坐标比较 symbolic_coord_compare 结果
 *   - GEOM_LINE_SEGMENT: 比较两个端点的坐标（通过 INCIDENCE 约束的参与者）
 *   - GEOM_REGION: 比较边界线段集合的哈希
 *   - GEOM_PORT: 比较坐标（与 POINT 相同逻辑）
 *   - GEOM_FUNCTION_BLOCK: 比较内部节点集合
 *
 * 若任一节点为 NULL、坐标数量不同或任一坐标为 NULL，返回 0。
 *
 * @param a 第一个节点指针
 * @param b 第二个节点指针
 * @return 1 表示坐标完全相等，0 表示不相等或不可比较
 */

/**
 * @brief 内联辅助函数：比较两个节点的符号坐标是否完全相等
 *
 * 抽取 nodes_coords_equal 中各几何类型分支中重复出现的坐标逐项比较逻辑。
 * 检查流程：
 * 1. NULL 检查：任一节点为 NULL 则判定为不相等
 * 2. 坐标数量一致性：coord_count 不同则不可能相等
 * 3. 逐坐标比较：对每个坐标槽位调用 symbolic_coord_compare，
 *    任一槽位的指针为 NULL 或比较结果非零则判定为不相等
 *
 * 此函数不关心节点的几何类型（POINT/LINE/REGION 等），仅检查
 * symbolic_coords 数组的内容一致性，因此可被多种几何类型复用。
 *
 * @param a 第一个节点
 * @param b 第二个节点
 * @return 1 表示所有坐标均相等，0 表示存在差异或参数无效
 */
static inline int coords_equal_by_type(GeomNode *a, GeomNode *b) {
    if (!a || !b)
        return 0;
    if (a->coord_count != b->coord_count)
        return 0;
    for (int c = 0; c < a->coord_count; c++) {
        if (!a->symbolic_coords[c] || !b->symbolic_coords[c])
            return 0;
        if (symbolic_coord_compare(a->symbolic_coords[c], b->symbolic_coords[c]) != 0) {
            return 0;
        }
    }
    return 1;
}

/* --- 节点坐标相等性比较：函数指针表 --- */
typedef int (*CoordEqualFunc)(GeomNode *a, GeomNode *b);

static int coord_equal_point_port_segment(GeomNode *a, GeomNode *b) {
    return coords_equal_by_type(a, b);
}

static int coord_equal_region(GeomNode *a, GeomNode *b) {
    if (a->data.region.segment_count != b->data.region.segment_count)
        return 0;
    if (a->data.region.segment_count == 0)
        return 1;
    for (int s = 0; s < a->data.region.segment_count; s++) {
        GeomNode *seg_a = a->data.region.boundary_segments[s];
        GeomNode *seg_b = b->data.region.boundary_segments[s];
        if (!seg_a || !seg_b)
            return 0;
        if (seg_a->type != seg_b->type)
            return 0;
        if (!coords_equal_by_type(seg_a, seg_b))
            return 0;
    }
    return 1;
}

static int coord_equal_circle(GeomNode *a, GeomNode *b) {
    if (a->data.circle.center_node_id != b->data.circle.center_node_id)
        return 0;
    if (a->data.circle.radius_node_id != b->data.circle.radius_node_id)
        return 0;
    return 1;
}

static int coord_equal_func_block(GeomNode *a, GeomNode *b) {
    int j;
    if (a->data.func_block.internal_node_count != b->data.func_block.internal_node_count)
        return 0;
    if (a->data.func_block.input_count != b->data.func_block.input_count)
        return 0;
    if (a->data.func_block.output_count != b->data.func_block.output_count)
        return 0;
    if (a->data.func_block.determinism_state != b->data.func_block.determinism_state)
        return 0;
    for (int n = 0; n < a->data.func_block.internal_node_count; n++) {
        GeomNode *na = a->data.func_block.internal_nodes[n];
        GeomNode *nb = b->data.func_block.internal_nodes[n];
        if (!na || !nb)
            return 0;
        if (!coords_equal_by_type(na, nb))
            return 0;
    }
    for (j = 0; j < a->data.func_block.input_count; j++) {
        if (a->data.func_block.input_port_ids[j] != b->data.func_block.input_port_ids[j])
            return 0;
    }
    for (j = 0; j < a->data.func_block.output_count; j++) {
        if (a->data.func_block.output_port_ids[j] != b->data.func_block.output_port_ids[j])
            return 0;
    }
    return 1;
}

static CoordEqualFunc s_coord_equal_funcs[] = {
    [GEOM_POINT] = coord_equal_point_port_segment,
    [GEOM_LINE_SEGMENT] = coord_equal_point_port_segment,
    [GEOM_REGION] = coord_equal_region,
    [GEOM_CIRCLE] = coord_equal_circle,
    [GEOM_PORT] = coord_equal_point_port_segment,
    [GEOM_FUNCTION_BLOCK] = coord_equal_func_block,
};
static const int s_coord_equal_func_count = (int)(sizeof(s_coord_equal_funcs) / sizeof(s_coord_equal_funcs[0]));

int nodes_coords_equal(GeomNode *a, GeomNode *b) {
    if (!a || !b)
        return 0;
    if (a->type != b->type)
        return 0;
    if ((int)a->type >= 0 && a->type < s_coord_equal_func_count && s_coord_equal_funcs[a->type]) {
        return s_coord_equal_funcs[a->type](a, b);
    }
    return 0;
}

/* --- 节点坐标哈希：函数指针表 --- */
static uint64_t coord_hash_point_port(GeomNode *node) {
    uint64_t h = 0;
    for (int c = 0; c < node->coord_count; c++) {
        if (node->symbolic_coords[c]) {
            h ^= (uint64_t)symbolic_coord_hash(node->symbolic_coords[c]);
        }
    }
    return h;
}

static uint64_t coord_hash_line_segment(GeomNode *node) {
    uint64_t h = 0;
    h ^= (uint64_t)0x5E5E5E5E5E5E5E5EULL;
    for (int c = 0; c < node->coord_count; c++) {
        if (node->symbolic_coords[c]) {
            h ^= (uint64_t)symbolic_coord_hash(node->symbolic_coords[c]);
        }
    }
    return h;
}

static uint64_t coord_hash_region(GeomNode *node) {
    uint64_t h = 0;
    h ^= (uint64_t)0x3A3A3A3A3A3A3A3AULL;
    for (int s = 0; s < node->data.region.segment_count; s++) {
        GeomNode *seg = node->data.region.boundary_segments[s];
        if (seg) {
            for (int c = 0; c < seg->coord_count; c++) {
                if (seg->symbolic_coords[c]) {
                    h ^= (uint64_t)symbolic_coord_hash(seg->symbolic_coords[c]);
                }
            }
            h ^= (uint64_t)(s + 1) * 0x9E3779B97F4A7C15ULL;
        }
    }
    return h;
}

static uint64_t coord_hash_circle(GeomNode *node) {
    uint64_t h = 0;
    h ^= (uint64_t)0xC1C1C1C1C1C1C1C1ULL;
    h ^= (uint64_t)node->data.circle.center_node_id * 0x9E3779B97F4A7C15ULL;
    h ^= (uint64_t)node->data.circle.radius_node_id * 0x9E3779B97F4A7C16ULL;
    return h;
}

static uint64_t coord_hash_func_block(GeomNode *node) {
    uint64_t h = 0;
    h ^= (uint64_t)0x7B7B7B7B7B7B7B7BULL;
    for (int n = 0; n < node->data.func_block.internal_node_count; n++) {
        GeomNode *inner = node->data.func_block.internal_nodes[n];
        if (inner) {
            for (int c = 0; c < inner->coord_count; c++) {
                if (inner->symbolic_coords[c]) {
                    h ^= (uint64_t)symbolic_coord_hash(inner->symbolic_coords[c]);
                }
            }
            h ^= (uint64_t)(n + 1) * 0x9E3779B97F4A7C15ULL;
        }
    }
    return h;
}

static uint64_t (*s_coord_hash_funcs[])(GeomNode *) = {
    [GEOM_POINT] = coord_hash_point_port,
    [GEOM_LINE_SEGMENT] = coord_hash_line_segment,
    [GEOM_REGION] = coord_hash_region,
    [GEOM_CIRCLE] = coord_hash_circle,
    [GEOM_PORT] = coord_hash_point_port,
    [GEOM_FUNCTION_BLOCK] = coord_hash_func_block,
};
static const int s_coord_hash_func_count = (int)(sizeof(s_coord_hash_funcs) / sizeof(s_coord_hash_funcs[0]));

/**
 * @brief 计算节点的坐标哈希值（用于预过滤分组）
 *
 * 对各类型节点计算包含其几何特征的哈希值：
 *   - GEOM_POINT / GEOM_PORT: 对所有坐标的 symbolic_coord_hash 取异或
 *   - GEOM_LINE_SEGMENT: 对坐标哈希取异或，并混入类型标记
 *   - GEOM_REGION: 对所有边界线段的坐标哈希取异或
 *   - GEOM_FUNCTION_BLOCK: 对所有内部节点的坐标哈希取异或
 *
 * @param node 节点指针
 * @return 坐标哈希值
 */
uint64_t compute_node_coord_hash(GeomNode *node) {
    if (!node || node->coord_count == 0)
        return 0;
    if ((int)node->type >= 0 && node->type < s_coord_hash_func_count && s_coord_hash_funcs[node->type]) {
        return s_coord_hash_funcs[node->type](node);
    }
    return 0;
}

/* ---------------------------------------------------------------------------
 *  match_ports - 端口匹配辅助函数
 *
 * 为命题图中的每个端口节点在构造图中查找类型、命名空间深度、
 * 类型区域均等价的端口。每个构造端口最多匹配一个命题端口。
 * 匹配成功时标记 used_construction_ports 数组中的对应位置。
 * ------------------------------------------------------------------------- */

/**
 * @brief 端口匹配：遍历命题端口并在构造图中查找匹配
 *
 * 使用 cidx 计数器索引 used_construction_ports 数组（仅在遇到 GEOM_PORT
 * 类型节点时递增），确保每个构造端口最多匹配一个命题端口。
 * 匹配条件包括：端口类型相同、命名空间深度相同、类型区域等价。
 *
 * @param construction            构造图
 * @param proposition             命题图
 * @param used_construction_ports 已用构造端口标记数组（int* 类型，cidx 索引）
 * @param ts                      TypeSystem 实例（可为 NULL，跳过类型区域检查）
 * @return true  所有命题端口均找到匹配
 * @return false 存在无法匹配的命题端口（调用者负责清理资源）
 */
bool match_ports(const ConstraintGraph *construction, const ConstraintGraph *proposition,
                        int *used_construction_ports, TypeSystem *ts) {
    for (int i = 0; i < proposition->node_count; i++) {
        GeomNode *pn = proposition->nodes[i];
        if (pn->type != GEOM_PORT)
            continue;
        Port *pp = pn->data.port;
        bool found_match = false;
        int cidx = 0;
        for (int j = 0; j < construction->node_count; j++) {
            GeomNode *cn = construction->nodes[j];
            if (cn->type != GEOM_PORT)
                continue;
            if (used_construction_ports[cidx]) {
                cidx++;
                continue;
            }
            Port *cp = cn->data.port;
            if (pp->type != cp->type) {
                cidx++;
                continue;
            }
            if (pp->namespace_depth != cp->namespace_depth) {
                cidx++;
                continue;
            }
            /* 比较父函数块 ID：确保端口属于相同的函数块作用域 */
            if (pp->parent_block_id != cp->parent_block_id) {
                cidx++;
                continue;
            }
            /* 比较形式参数标志 */
            if (pp->is_formal_param != cp->is_formal_param) {
                cidx++;
                continue;
            }
            if (pp->type_region && cp->type_region && ts) {
                TypeEquivResult equiv = type_check_equivalence(ts, pp->type_region, cp->type_region, false);
                if (equiv == TYPE_EQUIV_NOT_EQUIV) {
                    cidx++;
                    continue;
                }
            }
            used_construction_ports[cidx] = 1;
            found_match = true;
            break;
        }
        if (!found_match) {
            return false;
        }
    }
    return true;
}
