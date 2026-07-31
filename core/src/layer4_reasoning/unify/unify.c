/**
 * @file unify.c
 * @brief 合一检查实现
 * @details 实现构造与命题之间的合一检查，包括约束匹配、坐标判等、
 *          命题实例化和等价声明。支持哈希过滤和详细匹配结果输出。
 *
 * 【模块使用说明】
 *
 * 本模块是 Lv-00 几何元语言系统的核心合一引擎，负责验证构造图与命题图
 * 之间的结构等价性。主要功能模块如下：
 *
 * 1. 基础合一检查（unify_construction_with_proposition）：
 *    检查两个约束图在归一化后是否在拓扑和约束结构上等价。
 *
 * 2. 坐标级合一检查（unify_construction_with_proposition_coord）：
 *    在基础合一检查的基础上，增加对参与节点符号坐标的逐项判等，
 *    确保不仅拓扑匹配，几何语义也完全一致。
 *
 * 3. 哈希预过滤合一检查（unify_construction_with_proposition_hash）：
 *    使用坐标哈希值预分组，在详细匹配前快速排除不匹配的端口和约束对，
 *    显著提升大规模图的合一检查性能。
 *
 * 4. 命题实例化（unify_instantiate_proposition）：
 *    对含多态类型变量的命题图进行深拷贝，并将指定节点的类型区域
 *    替换为具体类型。使用引用语义（浅拷贝 type_region），调用者需
 *    确保 concrete_type 的生命周期覆盖实例化后的命题图。
 *
 * 5. 深层等价声明（unify_declare_equivalence）：
 *    声明两个节点在更深语义层上的等价性，用于桥接不同构造之间的
 *    语义联系。
 *
 * 6. 精细化匹配函数（端口匹配、约束匹配、坐标判等的独立封装）：
 *    将合一流程拆解为可独立调用的阶段函数，用于调试和自定义检查流程。
 *
 * 【典型调用流程】
 *
 *   // 创建 TypeSystem，用于端口类型等价检查
 *   TypeSystem *ts = type_system_create();
 *   ...
 *   // 执行合一检查
 *   UnifyStatus status = unify_construction_with_proposition_hash(constr, prop);
 *   if (status == UNIFY_STATUS_OK) {
 *       // 合一成功，可实例化类型变量
 *       unify_instantiate_proposition(prop, type_var_id, concrete_type, &inst);
 *   }
 *   ...
 *   // 清理
 *   type_system_destroy(ts);
 *
 * 【注意事项】
 *   - 所有合一函数内部创建 TypeSystem，使用者无需重复创建
 *   - 归一化结果（NormalizationResult）作为中间产物，函数内部管理生命周期
 *   - 哈希预过滤基于坐标哈希值，可能产生哈希碰撞导致误判，
 *     但不会导致正确性错误（最坏退化为全量比较）
 *   - type_region 使用引用语义，详见 unify_instantiate_proposition 文档
 *
 * @author Lv-00 Project
 * @version 3.0.1
 *
 * @dependencies
 *   - unify.h               : 合一检查器公共接口定义
 *   - constraint_graph.h    : 约束图接口
 *   - lv_internal.h       : 内部数据结构与常量
 *   - lv_utils.h          : 统一内存分配器和字符串工具
 *   - normalization.h       : 图规范化引擎
 *   - proof.h               : 证明系统接口（命题颜色更新）
 *   - type_system.h         : 类型系统（端口类型等价检查）
 *   - stream.h              : 流式事件输出
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
#include "lv_utils.h" /* lv_strdup_safe, lv_malloc 等统一内存管理 */
#include "normalization.h"
#include "stream.h"
#include "stream_context_util.h"
#include "type_system.h"
#include "lv/lv_strbuf.h"

lv_DECLARE_STREAM_CTX(unify);

void unify_set_stream_context(StreamContext *ctx) {
    unify_stream_ctx = ctx;
}

/* 哈希值到节点ID的掩码 —— 取哈希值低31位以确保结果为正整数 */
#define UNIFY_HASH_TO_ID_MASK 0x7FFFFFFF

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

static int nodes_coords_equal(GeomNode *a, GeomNode *b) {
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
static uint64_t compute_node_coord_hash(GeomNode *node) {
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
static bool match_ports(const ConstraintGraph *construction, const ConstraintGraph *proposition,
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

/* ---------------------------------------------------------------------------
 * 基础合一
 * ------------------------------------------------------------------------- */

UnifyStatus unify_construction_with_proposition(const ConstraintGraph *construction,
                                                const ConstraintGraph *proposition) {
    /* 合一前执行图规范化遍（设计文档 3.8 节） */
    if (construction) {
        geo_normalize((ConstraintGraph *) construction, true);
    }
    if (proposition) {
        geo_normalize((ConstraintGraph *) proposition, true);
    }
    if (unify_stream_ctx) {
        stream_emit_simple(unify_stream_ctx, STREAM_EVENT_PROOF_UNIFY, "合一检查开始", 0);
    }
    NormalizationResult *nc = graph_normalize(construction, true);
    NormalizationResult *np = graph_normalize(proposition, true);
    if (!nc || !np) {
        if (nc)
            normalization_result_destroy(nc);
        if (np)
            normalization_result_destroy(np);
        return UNIFY_STATUS_FAILED;
    }
    if (nc->merged_count != np->merged_count) {
        normalization_result_destroy(nc);
        normalization_result_destroy(np);
        return UNIFY_STATUS_COORD_MISMATCH;
    }
    /* 跟踪已匹配的构造端口，防止多对一匹配 */
    int construction_port_count = 0;
    for (int j = 0; j < construction->node_count; j++) {
        if (construction->nodes[j]->type == GEOM_PORT)
            construction_port_count++;
    }
    /* 【安全性修复】避免 lv_calloc(0, sizeof(int)) 的实现定义行为。
     * C标准规定 calloc(0, N) 可能返回 NULL 或一个不可解引用的非NULL指针。
     * 当 construction_port_count == 0 时：
     *   - 若返回 NULL：used_construction_ports 为 NULL，后续依赖其非NULL的代码存在隐患
     *   - 若返回非NULL：该指针在函数退出时未被 lv_free()，造成内存泄漏
     * 修复方式：当 port_count 为 0 时分配最小单元（1个元素），确保行为统一且无泄漏。
     * 该额外分配的1个元素在后续循环中不会被使用（循环条件跳过无端口的图）。 */
    int alloc_count = construction_port_count > 0 ? construction_port_count : 1;
    int *used_construction_ports = lv_calloc(alloc_count, sizeof(int));
    if (!used_construction_ports) {
        normalization_result_destroy(nc);
        normalization_result_destroy(np);
        return UNIFY_STATUS_FAILED;
    }

    /* 在循环外创建 TypeSystem 以提高性能 */
    TypeSystem *ts = type_system_create();

    if (!match_ports(construction, proposition, used_construction_ports, ts)) {
        lv_free((void **) &used_construction_ports);
        if (ts)
            type_system_destroy(ts);
        normalization_result_destroy(nc);
        normalization_result_destroy(np);
        if (unify_stream_ctx) {
            stream_emit_simple(unify_stream_ctx, STREAM_EVENT_PROOF_UNIFY, "合一检查失败：端口类型不匹配", 0);
        }
        return UNIFY_STATUS_PORT_TYPE_MISMATCH;
    }
    lv_free((void **) &used_construction_ports);
    if (ts)
        type_system_destroy(ts);
    for (int i = 0; i < proposition->constraint_count; i++) {
        Constraint *pc = proposition->constraints[i];
        bool found_match = false;
        for (int j = 0; j < construction->constraint_count; j++) {
            Constraint *cc = construction->constraints[j];
            if (pc->type != cc->type)
                continue;
            if (pc->participant_count != cc->participant_count)
                continue;
            bool same = true;
            for (int k = 0; k < pc->participant_count; k++) {
                if (pc->participants[k] != cc->participants[k]) {
                    same = false;
                    break;
                }
            }
            if (same) {
                found_match = true;
                break;
            }
        }
        if (!found_match) {
            normalization_result_destroy(nc);
            normalization_result_destroy(np);
            if (unify_stream_ctx) {
                stream_emit_simple(unify_stream_ctx, STREAM_EVENT_PROOF_UNIFY, "合一检查失败：约束不匹配", 0);
            }
            return UNIFY_STATUS_CONSTRAINT_MISMATCH;
        }
    }
    normalization_result_destroy(nc);
    normalization_result_destroy(np);
    if (unify_stream_ctx) {
        stream_emit_simple(unify_stream_ctx, STREAM_EVENT_PROOF_UNIFY, "合一检查成功", 0);
    }
    return UNIFY_STATUS_OK;
}

/* ---------------------------------------------------------------------------
 * Task 1a: 带坐标级别相等检查的合一
 *
 * 在约束匹配阶段，除了检查约束类型和参与者 ID 之外，
 * 还验证对应参与者的符号坐标是否相等。
 * 这确保了深层子图同构不仅匹配拓扑结构，还匹配几何语义。
 * ------------------------------------------------------------------------- */

UnifyStatus unify_construction_with_proposition_coord(const ConstraintGraph *construction,
                                                      const ConstraintGraph *proposition) {
    /*
     * 执行带坐标级判等的合一检查。流程分为四个阶段：
     *
     * 【阶段A - 归一化】对构造图和命题图分别进行归一化（含代数化简），
     *   比较合并节点数。若数量不等，直接返回 COORD_MISMATCH。
     *
     * 【阶段B - 端口类型匹配】遍历命题图的所有端口节点，在构造图中
     *   查找类型、命名空间深度、类型区域均匹配的端口。每个构造端口
     *   最多匹配一个命题端口（通过 used_construction_ports 数组防多对一）。
     *
     * 【阶段C - 约束匹配 + 坐标判等】在归一化约束匹配的基础上，
     *   对每一对匹配的约束进一步验证所有参与者的符号坐标是否相等。
     *   坐标检查仅对 GEOM_POINT 类型且有坐标的节点生效；
     *   非 POINT 或无坐标的节点视为自动通过。
     *
     * 【阶段D - 结果返回】所有端口和约束均匹配成功返回 OK，
     *   否则返回对应的错误状态码。
     */

    NormalizationResult *nc = graph_normalize(construction, true);
    NormalizationResult *np = graph_normalize(proposition, true);
    if (!nc || !np) {
        if (nc)
            normalization_result_destroy(nc);
        if (np)
            normalization_result_destroy(np);
        return UNIFY_STATUS_FAILED;
    }
    if (nc->merged_count != np->merged_count) {
        normalization_result_destroy(nc);
        normalization_result_destroy(np);
        return UNIFY_STATUS_COORD_MISMATCH;
    }

    /* 跟踪已匹配的构造端口（防止多对一匹配） */
    int construction_port_count = 0;
    for (int j = 0; j < construction->node_count; j++) {
        if (construction->nodes[j]->type == GEOM_PORT)
            construction_port_count++;
    }
    int *used_construction_ports = lv_calloc(construction_port_count > 0 ? construction_port_count : 1, sizeof(int));

    /* 创建 TypeSystem 用于端口类型等价检查 */
    TypeSystem *ts = type_system_create();

    /* 阶段B：端口类型匹配 —— 调用公共 match_ports() 辅助函数 */
    if (!match_ports(construction, proposition, used_construction_ports, ts)) {
        lv_free((void **) &used_construction_ports);
        if (ts)
            type_system_destroy(ts);
        normalization_result_destroy(nc);
        normalization_result_destroy(np);
        return UNIFY_STATUS_PORT_TYPE_MISMATCH;
    }
    lv_free((void **) &used_construction_ports);
    if (ts)
        type_system_destroy(ts);

    /* 阶段C：约束匹配 + 坐标级别判等
     * 在归一化约束匹配成功后，验证所有参与节点的符号坐标相等 */
    for (int i = 0; i < proposition->constraint_count; i++) {
        Constraint *pc = proposition->constraints[i];
        bool found_match = false;
        for (int j = 0; j < construction->constraint_count; j++) {
            Constraint *cc = construction->constraints[j];
            if (pc->type != cc->type)
                continue;
            if (pc->participant_count != cc->participant_count)
                continue;
            bool same = true;
            for (int k = 0; k < pc->participant_count; k++) {
                if (pc->participants[k] != cc->participants[k]) {
                    same = false;
                    break;
                }
            }
            if (!same)
                continue;

            /* 坐标级别相等检查：验证对应参与者的符号坐标 */
            bool coords_ok = true;
            for (int k = 0; k < pc->participant_count; k++) {
                GeomNode *p_node = graph_get_node(proposition, pc->participants[k]);
                GeomNode *c_node = graph_get_node(construction, cc->participants[k]);
                if (!nodes_coords_equal(p_node, c_node)) {
                    /* 如果两个节点都不是 POINT 类型或都没有坐标，
                     * 则跳过坐标检查（不视为不匹配） */
                    if (p_node && c_node && p_node->type == GEOM_POINT && c_node->type == GEOM_POINT &&
                        p_node->coord_count > 0 && c_node->coord_count > 0) {
                        coords_ok = false;
                        break;
                    }
                }
            }
            if (!coords_ok)
                continue;

            found_match = true;
            break;
        }
        if (!found_match) {
            normalization_result_destroy(nc);
            normalization_result_destroy(np);
            return UNIFY_STATUS_COORD_MISMATCH;
        }
    }

    normalization_result_destroy(nc);
    normalization_result_destroy(np);
    return UNIFY_STATUS_OK;
}

/* ---------------------------------------------------------------------------
 * Task 1b: 带哈希预过滤的合一
 *
 * 在进行详细的约束匹配之前，先使用 symbolic_coord_hash() 对节点
 * 按坐标哈希分组。只有哈希相同的节点组之间才进行比较，大幅减少
 * 不必要的约束匹配次数。
 * ------------------------------------------------------------------------- */

UnifyStatus unify_construction_with_proposition_hash_filtered(const ConstraintGraph *construction,
                                                              const ConstraintGraph *proposition) {
    /* 合一前执行图规范化遍（设计文档 3.8 节） */
    if (construction) {
        geo_normalize((ConstraintGraph *) construction, true);
    }
    if (proposition) {
        geo_normalize((ConstraintGraph *) proposition, true);
    }
    NormalizationResult *nc = graph_normalize((ConstraintGraph *) construction, true);
    NormalizationResult *np = graph_normalize((ConstraintGraph *) proposition, true);
    if (!nc || !np) {
        if (nc)
            normalization_result_destroy(nc);
        if (np)
            normalization_result_destroy(np);
        return UNIFY_STATUS_FAILED;
    }
    if (nc->merged_count != np->merged_count) {
        normalization_result_destroy(nc);
        normalization_result_destroy(np);
        return UNIFY_STATUS_COORD_MISMATCH;
    }

    /* 计算命题图中所有节点的坐标哈希 */
    uint64_t *prop_hashes = lv_calloc((size_t) proposition->node_count, sizeof(uint64_t));
    if (!prop_hashes) {
        normalization_result_destroy(nc);
        normalization_result_destroy(np);
        return UNIFY_STATUS_FAILED;
    }
    for (int i = 0; i < proposition->node_count; i++) {
        prop_hashes[i] = compute_node_coord_hash(proposition->nodes[i]);
    }

    /* 计算构造图中所有节点的坐标哈希 */
    uint64_t *con_hashes = lv_calloc((size_t) construction->node_count, sizeof(uint64_t));
    if (!con_hashes) {
        lv_free((void **) &prop_hashes);
        normalization_result_destroy(nc);
        normalization_result_destroy(np);
        return UNIFY_STATUS_FAILED;
    }
    for (int i = 0; i < construction->node_count; i++) {
        con_hashes[i] = compute_node_coord_hash(construction->nodes[i]);
    }

    /* 防止多个命题端口匹配到同一个构造端口 */
    bool *used_construction_ports = lv_calloc((size_t) construction->node_count, sizeof(bool));
    if (!used_construction_ports && construction->node_count > 0) {
        lv_free((void **) &prop_hashes);
        lv_free((void **) &con_hashes);
        normalization_result_destroy(nc);
        normalization_result_destroy(np);
        return UNIFY_STATUS_FAILED;
    }

    /* 创建类型系统，用于端口类型等价检查 */
    TypeSystem *ts = type_system_create();

    /* 端口类型匹配（使用哈希预过滤：只比较相同哈希组的端口） */
    for (int i = 0; i < proposition->node_count; i++) {
        GeomNode *pn = proposition->nodes[i];
        if (pn->type != GEOM_PORT)
            continue;
        Port *pp = pn->data.port;
        bool found_match = false;
        for (int j = 0; j < construction->node_count; j++) {
            GeomNode *cn = construction->nodes[j];
            if (cn->type != GEOM_PORT)
                continue;

            /* 跳过已被其他命题端口匹配的构造端口，防止多对一 */
            if (used_construction_ports[j])
                continue;

            Port *cp = cn->data.port;

            /* 哈希预过滤：使用端口类型哈希进行快速排除。
             * 端口节点的 coord_count=0，所以我们哈希端口属性
             *（类型 + namespace_depth）而不是坐标。 */
            {
                uint64_t p_port_hash = ((uint64_t) pp->type << 32) | ((uint64_t) pp->namespace_depth << 16);
                uint64_t c_port_hash = ((uint64_t) cp->type << 32) | ((uint64_t) cp->namespace_depth << 16);
                if (p_port_hash != c_port_hash)
                    continue;
            }
            if (pp->type != cp->type)
                continue;
            if (pp->namespace_depth != cp->namespace_depth)
                continue;
            if (pp->parent_block_id != cp->parent_block_id)
                continue;
            if (pp->is_formal_param != cp->is_formal_param)
                continue;

            /* 类型等价检查（TypeSystem） */
            if (pp->type_region && cp->type_region && ts) {
                TypeEquivResult equiv = type_check_equivalence(ts, pp->type_region, cp->type_region, false);
                if (equiv == TYPE_EQUIV_NOT_EQUIV)
                    continue;
            }

            /* 匹配成功，标记该构造端口为已使用 */
            used_construction_ports[j] = true;
            found_match = true;
            break;
        }
        if (!found_match) {
            lv_free((void **) &used_construction_ports);
            if (ts)
                type_system_destroy(ts);
            lv_free((void **) &prop_hashes);
            lv_free((void **) &con_hashes);
            normalization_result_destroy(nc);
            normalization_result_destroy(np);
            return UNIFY_STATUS_PORT_TYPE_MISMATCH;
        }
    }
    lv_free((void **) &used_construction_ports);
    if (ts)
        type_system_destroy(ts);

    /* 约束匹配：使用哈希预过滤加速 */
    for (int i = 0; i < proposition->constraint_count; i++) {
        Constraint *pc = proposition->constraints[i];
        bool found_match = false;

        /* 计算此命题约束所有参与者的坐标哈希签名 */
        uint64_t p_sig = 0;
        for (int k = 0; k < pc->participant_count; k++) {
            GeomNode *p_node = graph_get_node(proposition, pc->participants[k]);
            if (p_node) {
                p_sig ^= compute_node_coord_hash(p_node);
            }
        }

        for (int j = 0; j < construction->constraint_count; j++) {
            Constraint *cc = construction->constraints[j];
            if (pc->type != cc->type)
                continue;
            if (pc->participant_count != cc->participant_count)
                continue;

            /* 哈希预过滤：计算构造约束的哈希签名，
             * 如果签名不同则跳过详细比较 */
            uint64_t c_sig = 0;
            for (int k = 0; k < cc->participant_count; k++) {
                GeomNode *c_node = graph_get_node(construction, cc->participants[k]);
                if (c_node) {
                    c_sig ^= compute_node_coord_hash(c_node);
                }
            }
            if (p_sig != c_sig)
                continue;

            /* 详细匹配：检查参与者 ID */
            bool same = true;
            for (int k = 0; k < pc->participant_count; k++) {
                if (pc->participants[k] != cc->participants[k]) {
                    same = false;
                    break;
                }
            }
            if (same) {
                found_match = true;
                break;
            }
        }
        if (!found_match) {
            lv_free((void **) &prop_hashes);
            lv_free((void **) &con_hashes);
            normalization_result_destroy(nc);
            normalization_result_destroy(np);
            return UNIFY_STATUS_CONSTRAINT_MISMATCH;
        }
    }

    lv_free((void **) &prop_hashes);
    lv_free((void **) &con_hashes);
    normalization_result_destroy(nc);
    normalization_result_destroy(np);
    return UNIFY_STATUS_OK;
}

/* ---------------------------------------------------------------------------
 * 简化命题与证明
 * ------------------------------------------------------------------------- */

SimpleProposition *simple_proposition_create(const char *name, int *input_port_ids, int input_count,
                                             int *output_port_ids, int output_count) {
    SimpleProposition *prop = lv_calloc(1, sizeof(SimpleProposition));
    if (!prop)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "simple_proposition_create: calloc prop failed");

    /* 使用 lv_strdup_safe 替代裸 strdup，统一内存管理，
     * 确保内存统计正确且避免混用标准 free 与 lv_free。
     * 当 name 为 NULL 时，使用空字符串作为默认值。 */
    prop->name = name ? lv_strdup_safe(name) : lv_strdup_safe("");
    if (!prop->name) {
        lv_free((void **) &prop);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "simple_proposition_create: strdup name failed");
    }

    prop->pattern = graph_create();
    if (!prop->pattern) {
        lv_free((void **) &prop->name);
        lv_free((void **) &prop);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "simple_proposition_create: graph_create failed");
    }

    /* 检查 input_port_ids 和 output_port_ids 是否为 NULL。
     * 当 count > 0 但对应数组为 NULL 时，视为参数错误，返回 NULL。 */
    if ((input_count > 0 && !input_port_ids) || (output_count > 0 && !output_port_ids)) {
        graph_destroy(prop->pattern);
        lv_free((void **) &prop->name);
        lv_free((void **) &prop);
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "simple_proposition_create: NULL port_ids with non-zero count");
    }

    prop->input_port_ids = input_count > 0 ? lv_calloc((size_t) input_count, sizeof(int)) : NULL;
    if (input_count > 0 && prop->input_port_ids) {
        memcpy(prop->input_port_ids, input_port_ids, (size_t) input_count * sizeof(int));
    }
    prop->output_port_ids = output_count > 0 ? lv_calloc((size_t) output_count, sizeof(int)) : NULL;
    if (output_count > 0 && prop->output_port_ids) {
        memcpy(prop->output_port_ids, output_port_ids, (size_t) output_count * sizeof(int));
    }
    prop->input_count = input_count;
    prop->output_count = output_count;
    return prop;
}

void simple_proposition_destroy(SimpleProposition *prop) {
    if (prop) {
        lv_free((void **) &prop->name);
        lv_free((void **) &prop->input_port_ids);
        lv_free((void **) &prop->output_port_ids);
        graph_destroy(prop->pattern);
        lv_free((void **) &prop);
    }
}

SimpleProof *simple_proof_create(SimpleProposition *prop, ConstraintGraph *construction) {
    SimpleProof *proof = lv_calloc(1, sizeof(SimpleProof));
    if (!proof)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "simple_proof_create: calloc proof failed");
    proof->proposition = prop;
    proof->construction = construction;
    proof->normalized = false;
    proof->passed = false;
    return proof;
}

void simple_proof_destroy(SimpleProof *proof) {
    if (proof) {
        graph_destroy(proof->construction);
        lv_free((void **) &proof);
    }
}

bool simple_proof_check(SimpleProof *proof) {
    if (!proof->normalized) {
        simple_proof_normalize(proof);
    }

    /* 层级1：基本构造-命题合一 */
    UnifyStatus status = unify_construction_with_proposition(proof->construction, proof->proposition->pattern);

    if (status != UNIFY_STATUS_OK) {
        proof->passed = false;
        return false;
    }

    /* 层级2：坐标级别的等价验证。
     * 根据 design_v2.9.md Section 10.2：基本匹配通过后，
     * 验证坐标是否代数等价。 */
    UnifyStatus coord_status =
        unify_construction_with_proposition_coord(proof->construction, proof->proposition->pattern);

    proof->passed = (coord_status == UNIFY_STATUS_OK);
    return proof->passed;
}

void simple_proof_normalize(SimpleProof *proof) {
    NormalizationResult *nr = graph_normalize(proof->construction, true);
    if (nr) {
        proof->normalized = true;
        normalization_result_destroy(nr);
    }
}

/* ---------------------------------------------------------------------------
 * 不匹配位置的具体报告
 * ------------------------------------------------------------------------- */

void unify_failure_info_destroy(UnifyFailureInfo *info) {
    if (info) {
        lv_free((void **) &info->description);
    }
}

/**
 * @brief 初始化失败信息结构体
 *
 * @param info 失败信息结构体指针
 */
static void failure_info_init(UnifyFailureInfo *info) {
    if (info) {
        info->status = UNIFY_STATUS_OK;
        info->failed_constraint_id = -1;
        info->failed_node_id = -1;
        info->failed_port_index = -1;
        info->description = NULL;
    }
}

/**
 * @brief 设置失败信息结构体的值
 *
 * @param info           失败信息结构体指针
 * @param status         合一状态码
 * @param constraint_id  失败的约束 ID
 * @param node_id        失败的节点 ID
 * @param port_index     失败的端口索引
 * @param fmt            格式字符串
 * @param ...            可变参数
 */
static void failure_info_set(UnifyFailureInfo *info, UnifyStatus status, int constraint_id, int node_id, int port_index,
                             const char *fmt, ...) {
    if (!info)
        return;
    info->status = status;
    info->failed_constraint_id = constraint_id;
    info->failed_node_id = node_id;
    info->failed_port_index = port_index;
    if (info->description) {
        lv_free((void **) &info->description);
    }
    if (fmt) {
        /* 用 lvStrBuf 一次性格式化，替代两遍 vsnprintf */
        lvStrBuf sb = {0};
        va_list args;
        va_start(args, fmt);
        lv_strbuf_vprintf(&sb, fmt, args);
        va_end(args);
        if (sb.len > 0)
            info->description = lv_strbuf_to_string(&sb);
        else
            lv_strbuf_destroy(&sb);
    }
}

UnifyStatus unify_construction_with_proposition_detailed(const ConstraintGraph *construction,
                                                         const ConstraintGraph *pattern,
                                                         UnifyFailureInfo *out_failure) {
    if (out_failure)
        failure_info_init(out_failure);

    if (unify_stream_ctx) {
        stream_emit_simple(unify_stream_ctx, STREAM_EVENT_PROOF_UNIFY, "详细合一检查开始", 0);
    }

    if (!construction || !pattern) {
        failure_info_set(out_failure, UNIFY_STATUS_FAILED, -1, -1, -1, "NULL graph argument");
        if (unify_stream_ctx) {
            stream_emit_simple(unify_stream_ctx, STREAM_EVENT_PROOF_UNIFY, "详细合一检查失败：图参数为空", 0);
        }
        return UNIFY_STATUS_FAILED;
    }

    NormalizationResult *nc = graph_normalize(construction, true);
    NormalizationResult *np = graph_normalize(pattern, true);
    if (!nc || !np) {
        if (nc)
            normalization_result_destroy(nc);
        if (np)
            normalization_result_destroy(np);
        failure_info_set(out_failure, UNIFY_STATUS_FAILED, -1, -1, -1, "Normalization failed");
        if (unify_stream_ctx) {
            stream_emit_simple(unify_stream_ctx, STREAM_EVENT_PROOF_UNIFY, "详细合一检查失败：归一化失败", 0);
        }
        return UNIFY_STATUS_FAILED;
    }

    if (nc->merged_count != np->merged_count) {
        normalization_result_destroy(nc);
        normalization_result_destroy(np);
        failure_info_set(out_failure, UNIFY_STATUS_COORD_MISMATCH, -1, -1, -1,
                         "Merged node count mismatch: construction has %d, pattern has %d", nc->merged_count,
                         np->merged_count);
        if (unify_stream_ctx) {
            stream_emit_simple(unify_stream_ctx, STREAM_EVENT_PROOF_UNIFY, "详细合一检查失败：合并节点数量不匹配", 0);
        }
        return UNIFY_STATUS_COORD_MISMATCH;
    }

    /* 跟踪已匹配的 construction 端口 */
    int construction_port_count = 0;
    for (int j = 0; j < construction->node_count; j++) {
        if (construction->nodes[j]->type == GEOM_PORT)
            construction_port_count++;
    }
    int *used_construction_ports =
        lv_calloc(construction_port_count > 0 ? (size_t) construction_port_count : 1, sizeof(int));

    TypeSystem *ts = type_system_create();

    /* 端口类型匹配（带详细失败报告） */
    int prop_port_index = 0;
    for (int i = 0; i < pattern->node_count; i++) {
        GeomNode *pn = pattern->nodes[i];
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
            if (pp->parent_block_id != cp->parent_block_id) {
                cidx++;
                continue;
            }
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
            lv_free((void **) &used_construction_ports);
            if (ts)
                type_system_destroy(ts);
            normalization_result_destroy(nc);
            normalization_result_destroy(np);
            failure_info_set(out_failure, UNIFY_STATUS_PORT_TYPE_MISMATCH, -1, pn->id, prop_port_index,
                             "No matching port in construction for pattern port %d "
                             "(type=%d, namespace_depth=%d)",
                             pn->id, (int) pp->type, pp->namespace_depth);
            if (unify_stream_ctx) {
                stream_emit_simple(unify_stream_ctx, STREAM_EVENT_PROOF_UNIFY, "详细合一检查失败：端口类型不匹配", 0);
            }
            return UNIFY_STATUS_PORT_TYPE_MISMATCH;
        }
        prop_port_index++;
    }
    lv_free((void **) &used_construction_ports);
    if (ts)
        type_system_destroy(ts);

    /* 约束匹配（带详细失败报告） */
    for (int i = 0; i < pattern->constraint_count; i++) {
        Constraint *pc = pattern->constraints[i];
        bool found_match = false;
        for (int j = 0; j < construction->constraint_count; j++) {
            Constraint *cc = construction->constraints[j];
            if (pc->type != cc->type)
                continue;
            if (pc->participant_count != cc->participant_count)
                continue;
            bool same = true;
            for (int k = 0; k < pc->participant_count; k++) {
                if (pc->participants[k] != cc->participants[k]) {
                    same = false;
                    break;
                }
            }
            if (same) {
                found_match = true;
                break;
            }
        }
        if (!found_match) {
            normalization_result_destroy(nc);
            normalization_result_destroy(np);
            failure_info_set(out_failure, UNIFY_STATUS_CONSTRAINT_MISMATCH, pc->id, -1, -1,
                             "No matching constraint in construction for pattern "
                             "constraint %d (type=%d, participants=%d)",
                             pc->id, (int) pc->type, pc->participant_count);
            if (unify_stream_ctx) {
                stream_emit_simple(unify_stream_ctx, STREAM_EVENT_PROOF_UNIFY, "详细合一检查失败：约束不匹配", 0);
            }
            return UNIFY_STATUS_CONSTRAINT_MISMATCH;
        }
    }

    normalization_result_destroy(nc);
    normalization_result_destroy(np);
    if (unify_stream_ctx) {
        stream_emit_simple(unify_stream_ctx, STREAM_EVENT_PROOF_UNIFY, "详细合一检查成功", 0);
    }
    return UNIFY_STATUS_OK;
}

/* ---------------------------------------------------------------------------
 * 命题的等价变换
 * ------------------------------------------------------------------------- */

/* 等价声明存储 */
#define MAX_EQUIVALENCES 256

static lv_THREAD_LOCAL PropositionEquivalence g_equivalences[MAX_EQUIVALENCES];
static lv_THREAD_LOCAL int g_equivalence_count = 0;

bool unify_declare_proposition_equivalence(int prop_a_id, int prop_b_id, ConstraintGraph *transformation_rule) {
    if (g_equivalence_count >= MAX_EQUIVALENCES) {
        LOG_WARN("unify", "Proposition equivalence table full (max %d), cannot add more", MAX_EQUIVALENCES);
        return false;
    }

    /* 检查是否已存在相同的等价声明 */
    for (int i = 0; i < g_equivalence_count; i++) {
        if ((g_equivalences[i].prop_a_id == prop_a_id && g_equivalences[i].prop_b_id == prop_b_id) ||
            (g_equivalences[i].prop_a_id == prop_b_id && g_equivalences[i].prop_b_id == prop_a_id)) {
            /* 已存在，更新变换规则 */
            if (g_equivalences[i].transformation) {
                graph_destroy(g_equivalences[i].transformation);
            }
            g_equivalences[i].transformation = transformation_rule;
            return true;
        }
    }

    g_equivalences[g_equivalence_count].prop_a_id = prop_a_id;
    g_equivalences[g_equivalence_count].prop_b_id = prop_b_id;
    g_equivalences[g_equivalence_count].transformation = transformation_rule;
    g_equivalence_count++;
    return true;
}

int unify_find_equivalent_proposition(int prop_id, int *equivalent_ids, int max_count) {
    if (!equivalent_ids || max_count <= 0)
        return 0;

    int found = 0;
    for (int i = 0; i < g_equivalence_count && found < max_count; i++) {
        if (g_equivalences[i].prop_a_id == prop_id) {
            equivalent_ids[found++] = g_equivalences[i].prop_b_id;
        } else if (g_equivalences[i].prop_b_id == prop_id) {
            equivalent_ids[found++] = g_equivalences[i].prop_a_id;
        }
    }
    return found;
}

void unify_clear_equivalences(void) {
    for (int i = 0; i < g_equivalence_count; i++) {
        if (g_equivalences[i].transformation) {
            graph_destroy(g_equivalences[i].transformation);
            g_equivalences[i].transformation = NULL;
        }
    }
    g_equivalence_count = 0;
}

void unify_equivalence_storage_cleanup(void) {
    unify_clear_equivalences();
}

/* ---------------------------------------------------------------------------
 * 命题的实例化
 * ------------------------------------------------------------------------- */

/**
 * @brief ID映射表条目结构
 *
 * 用于在深拷贝过程中建立旧ID到新ID的映射关系
 */
typedef struct {
    int old_id; /**< 源图中的节点ID */
    int new_id; /**< 目标图中的节点ID */
} IdMappingEntry;

/**
 * @brief ID映射表结构
 */
typedef struct {
    IdMappingEntry *entries; /**< 映射条目数组 */
    int count;               /**< 当前条目数量 */
    int capacity;            /**< 数组容量 */
} IdMappingTable;

/**
 * @brief 初始化ID映射表
 * @param table 映射表指针
 * @param initial_capacity 初始容量
 */
/**
 * @brief 初始化ID映射表
 *
 * 分配映射条目数组，若分配失败则将 entries 设为 NULL 并将 capacity 设为 0，
 * 后续 id_mapping_add 会安全地检测到此状态并返回失败。
 *
 * @param table            映射表指针
 * @param initial_capacity 初始容量（必须 > 0）
 * @return true 初始化成功，false 内存分配失败
 */
static bool id_mapping_init(IdMappingTable *table, int initial_capacity) {
    table->entries = (IdMappingEntry *) lv_calloc((size_t) initial_capacity, sizeof(IdMappingEntry));
    if (!table->entries) {
        table->capacity = 0;
        table->count = 0;
        lv_RETURN_ERROR_BOOL(lv_ERROR_OUT_OF_MEMORY, "id_mapping_init: calloc entries failed");
    }
    table->count = 0;
    table->capacity = initial_capacity;
    return true;
}

/**
 * @brief 销毁ID映射表
 * @param table 映射表指针
 */
static void id_mapping_destroy(IdMappingTable *table) {
    if (table->entries) {
        lv_free((void **) &table->entries);
        table->entries = NULL;
    }
    table->count = 0;
    table->capacity = 0;
}

/**
 * @brief 添加ID映射
 * @param table 映射表指针
 * @param old_id 旧ID
 * @param new_id 新ID
 * @return 成功返回true，失败返回false（内存不足或表未初始化）
 */
static bool id_mapping_add(IdMappingTable *table, int old_id, int new_id) {
    /* 检查表是否已初始化（init 失败时 entries 为 NULL） */
    if (!table->entries)
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_STATE, "id_mapping_add: table not initialized");
    if (!lv_ensure_capacity((void **)&table->entries, table->count,
                            &table->capacity, sizeof(IdMappingEntry), 1))
        return false;

    table->entries[table->count].old_id = old_id;
    table->entries[table->count].new_id = new_id;
    table->count++;
    return true;
}

/**
 * @brief 查找旧ID对应的新ID
 * @param table 映射表指针
 * @param old_id 旧ID
 * @return 找到返回新ID，未找到返回-1
 */
static int id_mapping_find(const IdMappingTable *table, int old_id) {
    for (int i = 0; i < table->count; i++) {
        if (table->entries[i].old_id == old_id) {
            return table->entries[i].new_id;
        }
    }
    return -1;
}

/**
 * @brief 辅助函数：深拷贝约束图（带完整ID映射）
 *
 * 创建约束图的深拷贝，包括所有节点和约束。
 * 使用ID映射表确保LINE_SEGMENT端点和REGION边界正确更新。
 *
 * 【拷贝流程 —— 五阶段策略】
 * 由于不同节点类型之间存在拓扑依赖关系（例如 REGION 引用 LINE_SEGMENT），
 * 深拷贝必须按依赖顺序分阶段进行：
 *
 *   阶段0 - 预初始化：创建空目标图，初始化 ID 映射表（预分配 node_count+16 项）
 *   阶段1 - 拷贝无依赖节点（POINT + PORT）：这些节点不引用其他节点，
 *           可在无映射表的情况下直接拷贝，完成后填充映射表
 *   阶段2 - 拷贝 LINE_SEGMENT：需要查找端点ID在映射表中的新ID，
 *           通过符号坐标哈希值查找端点映射
 *   阶段3 - 拷贝 REGION：需要查找边界线段ID在映射表中的新ID
 *   阶段4 - 拷贝 FUNCTION_BLOCK：需要查找内部节点、输入输出端口ID的新ID
 *   阶段5 - 拷贝约束：使用ID映射表转换约束中的所有参与者引用
 *
 * 【错误处理】任何阶段的分配失败或添加失败均通过 goto fail 跳转，
 * 在 fail 标签处统一销毁 ID 映射表和已构建的目标图。
 *
 * @param src 源约束图
 * @return 深拷贝后的约束图，失败返回NULL
 */

/* --- 约束拷贝：函数指针表 --- */
typedef AddConstraintResult (*ConstraintCopyFunc)(ConstraintGraph *dst, int *participants, int count);

static AddConstraintResult copy_incidence(ConstraintGraph *dst, int *p, int n) {
    if (n >= 2)
        return graph_add_incidence(dst, p[0], p[1]);
    return ADD_CONSTRAINT_CONFLICT;
}

static AddConstraintResult copy_betweenness(ConstraintGraph *dst, int *p, int n) {
    if (n >= 3)
        return graph_add_betweenness(dst, p[0], p[1], p[2]);
    return ADD_CONSTRAINT_CONFLICT;
}

static AddConstraintResult copy_intersection(ConstraintGraph *dst, int *p, int n) {
    if (n >= 3)
        return graph_add_intersection(dst, p[0], p[1], p[2]);
    return ADD_CONSTRAINT_CONFLICT;
}

static AddConstraintResult copy_containment(ConstraintGraph *dst, int *p, int n) {
    if (n >= 2)
        return graph_add_containment(dst, p[0], p[1]);
    return ADD_CONSTRAINT_CONFLICT;
}

static AddConstraintResult copy_angle(ConstraintGraph *dst, int *p, int n) {
    if (n >= 2)
        return graph_add_angle(dst, p[0], p[1], 0.0);
    return ADD_CONSTRAINT_CONFLICT;
}

static AddConstraintResult copy_connection(ConstraintGraph *dst, int *p, int n) {
    if (n >= 2)
        return graph_add_connection(dst, p[0], p[1]);
    return ADD_CONSTRAINT_CONFLICT;
}

static ConstraintCopyFunc s_constraint_copy_funcs[] = {
    [INCIDENCE] = copy_incidence,
    [BETWEENNESS] = copy_betweenness,
    [INTERSECTION] = copy_intersection,
    [CONTAINMENT] = copy_containment,
    [CONNECTION] = copy_connection,
    [ANGLE] = copy_angle,
};
static const int s_constraint_copy_func_count = (int)(sizeof(s_constraint_copy_funcs) / sizeof(s_constraint_copy_funcs[0]));

static ConstraintGraph *deep_copy_graph(const ConstraintGraph *src) {
    if (!src)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "deep_copy_graph: src is NULL");

    ConstraintGraph *dst = graph_create();
    if (!dst)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "deep_copy_graph: graph_create failed");

    /* 创建ID映射表 */
    IdMappingTable id_map;
    /* 初始化 ID 映射表，用于记录源图节点 ID 到目标图节点 ID 的对应关系。
     * 若映射表初始化失败，释放已分配的目标图和 ID 映射表并返回 NULL。 */
    id_mapping_init(&id_map, src->node_count + 16);
    if (!id_map.entries) {
        graph_destroy(dst);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "deep_copy_graph: id_mapping_init failed");
    }

    /*
     * 阶段1：拷贝无依赖节点（POINT 和 PORT）
     *
     * 这些节点类型不引用任何尚未创建的节点，可以在映射表
     * 尚未完全填充的情况下安全地逐个拷贝。拷贝后立即将
     * (old_id, new_id) 写入映射表，供后续阶段使用。
     *
     * LINE_SEGMENT、REGION、FUNCTION_BLOCK 在此阶段跳过，
     * 等待其依赖的节点先完成拷贝。
     */

    /* 先拷贝POINT和PORT节点 */
    for (int i = 0; i < src->node_count; i++) {
        GeomNode *src_node = src->nodes[i];
        int old_id = src_node->id;
        int new_id = -1;

        switch (src_node->type) {
            case GEOM_POINT: {
                AddNodeResult r = graph_add_point(dst, src_node->symbolic_coords, src_node->coord_count);
                if (r != ADD_NODE_OK)
                    goto fail;
                new_id = dst->nodes[dst->node_count - 1]->id;
                break;
            }
            case GEOM_PORT: {
                PortType pt = PORT_INPUT;
                int ns_depth = 0;
                int parent_id = -1;
                if (src_node->data.port) {
                    pt = src_node->data.port->type;
                    ns_depth = src_node->data.port->namespace_depth;
                    parent_id = src_node->data.port->parent_block_id;
                }
                AddNodeResult r = graph_add_port(dst, pt, ns_depth, parent_id);
                if (r != ADD_NODE_OK)
                    goto fail;
                new_id = dst->nodes[dst->node_count - 1]->id;

                /* 复制端口属性到新节点 */
                GeomNode *new_node = dst->nodes[dst->node_count - 1];
                if (new_node && new_node->data.port && src_node->data.port) {
                    new_node->data.port->is_formal_param = src_node->data.port->is_formal_param;
                    new_node->data.port->is_polymorphic = src_node->data.port->is_polymorphic;
                    new_node->data.port->type_region = src_node->data.port->type_region;
                    /* 注意：type_region 不做深拷贝，共享引用 */
                }
                break;
            }
            case GEOM_LINE_SEGMENT:
            case GEOM_REGION:
            case GEOM_CIRCLE:
            case GEOM_FUNCTION_BLOCK:
                /* 这些类型在第二阶段处理 */
                continue;
        }

        /* 记录ID映射 */
        if (new_id >= 0) {
            if (!id_mapping_add(&id_map, old_id, new_id))
                goto fail;
        }
    }

    /*
     * 阶段2：拷贝 LINE_SEGMENT 节点
     *
     * LINE_SEGMENT 依赖两个端点（POINT 节点），这些端点在阶段1中
     * 已完成拷贝并写入映射表。由于 graph_add_line_segment 将端点坐标
     * 拷贝到 LINE_SEGMENT 的 symbolic_coords 数组中（而非通过图约束
     * 存储端点 ID），此处需要通过坐标匹配在源图中查找端点：
     *
     *   1. 取 LINE_SEGMENT 的 symbolic_coords[0] 和 symbolic_coords[1]
     *      （分别对应端点1和端点2的第一个坐标维度）
     *   2. 遍历源图的 POINT 节点，比较 symbolic_coords[0] 是否匹配
     *   3. 将找到的端点旧 ID 通过映射表转换为新 ID
     *   4. 调用 graph_add_line_segment 创建新线段
     *
     * 若无法确定端点（无坐标或找不到匹配 POINT），使用 -1 占位符。
     */
    for (int i = 0; i < src->node_count; i++) {
        GeomNode *src_node = src->nodes[i];
        if (src_node->type != GEOM_LINE_SEGMENT)
            continue;

        int old_id = src_node->id;

        /*
         * 通过坐标匹配查找端点 POINT 节点。
         * LINE_SEGMENT 的 symbolic_coords 是端点坐标的副本，
         * 因此可以通过比较坐标值找到原始端点。
         */
        int endpoint1_old = -1, endpoint2_old = -1;

        if (src_node->coord_count >= 2 && src_node->symbolic_coords[0] && src_node->symbolic_coords[1]) {
            /*
             * 遍历源图 POINT 节点，找到坐标匹配的端点。
             * 每个 POINT 的 symbolic_coords[0] 与端点坐标比较。
             */
            for (int j = 0; j < src->node_count; j++) {
                GeomNode *candidate = src->nodes[j];
                if (candidate->type != GEOM_POINT)
                    continue;
                if (!candidate->symbolic_coords || !candidate->symbolic_coords[0])
                    continue;

                /* 比较候选 POINT 的坐标是否与端点1坐标匹配 */
                if (endpoint1_old < 0 &&
                    symbolic_coord_compare(src_node->symbolic_coords[0], candidate->symbolic_coords[0]) == 0) {
                    endpoint1_old = candidate->id;
                    continue;
                }

                /* 比较候选 POINT 的坐标是否与端点2坐标匹配 */
                if (endpoint2_old < 0 &&
                    symbolic_coord_compare(src_node->symbolic_coords[1], candidate->symbolic_coords[0]) == 0) {
                    endpoint2_old = candidate->id;
                }
            }
        }

        /* 查找端点的新ID */
        int endpoint1_new = id_mapping_find(&id_map, endpoint1_old);
        int endpoint2_new = id_mapping_find(&id_map, endpoint2_old);

        /* 如果找不到映射，使用-1（占位符） */
        if (endpoint1_new < 0)
            endpoint1_new = -1;
        if (endpoint2_new < 0)
            endpoint2_new = -1;

        AddNodeResult r = graph_add_line_segment(dst, endpoint1_new, endpoint2_new);
        if (r != ADD_NODE_OK)
            goto fail;

        int new_id = dst->nodes[dst->node_count - 1]->id;
        if (!id_mapping_add(&id_map, old_id, new_id))
            goto fail;
    }

    /*
     * 阶段3：拷贝 REGION 节点
     *
     * REGION 依赖其边界线段（LINE_SEGMENT 节点），这些线段在阶段2中
     * 已完成拷贝。此处遍历边界线段数组，通过映射表将旧线段ID转换为
     * 新ID，再调用 graph_add_region 创建新区域节点。
     */
    for (int i = 0; i < src->node_count; i++) {
        GeomNode *src_node = src->nodes[i];
        if (src_node->type != GEOM_REGION)
            continue;

        int old_id = src_node->id;

        /* 转换边界线段ID */
        int *new_boundary_ids = NULL;
        int new_segment_count = 0;

        if (src_node->data.region.boundary_segments && src_node->data.region.segment_count > 0) {
            new_boundary_ids = lv_calloc(src_node->data.region.segment_count, sizeof(int));
            if (!new_boundary_ids)
                goto fail;

            for (int j = 0; j < src_node->data.region.segment_count; j++) {
                int old_seg_id = src_node->data.region.boundary_segments[j]->id;
                int new_seg_id = id_mapping_find(&id_map, old_seg_id);
                if (new_seg_id >= 0) {
                    new_boundary_ids[new_segment_count++] = new_seg_id;
                }
            }
        }

        AddNodeResult r = graph_add_region(dst, new_boundary_ids, new_segment_count);
        if (new_boundary_ids)
            lv_free((void **) &new_boundary_ids);
        if (r != ADD_NODE_OK)
            goto fail;

        int new_id = dst->nodes[dst->node_count - 1]->id;
        if (!id_mapping_add(&id_map, old_id, new_id))
            goto fail;
    }

    /*
     * 阶段4：拷贝 FUNCTION_BLOCK 节点
     *
     * FUNCTION_BLOCK 是三阶段中最复杂的节点类型，需要转换三类引用：
     *   a) 内部节点 (internal_nodes)：在阶段1中作为 POINT/PORT 已拷贝
     *   b) 输入端口 (input_port_ids)：在阶段1中作为 PORT 已拷贝
     *   c) 输出端口 (output_port_ids)：在阶段1中作为 PORT 已拷贝
     * 所有ID通过映射表查找新ID后，调用 graph_add_function_block 创建。
     * 拷贝后同步 determinism_state 确定性状态标志。
     */
    for (int i = 0; i < src->node_count; i++) {
        GeomNode *src_node = src->nodes[i];
        if (src_node->type != GEOM_FUNCTION_BLOCK)
            continue;

        int old_id = src_node->id;

        /* 转换内部节点ID */
        int *new_internal_ids = NULL;
        int new_internal_count = 0;
        if (src_node->data.func_block.internal_nodes && src_node->data.func_block.internal_node_count > 0) {
            new_internal_ids = lv_calloc(src_node->data.func_block.internal_node_count, sizeof(int));
            if (!new_internal_ids)
                goto fail;

            for (int j = 0; j < src_node->data.func_block.internal_node_count; j++) {
                int old_internal_id = src_node->data.func_block.internal_nodes[j]->id;
                int new_internal_id = id_mapping_find(&id_map, old_internal_id);
                if (new_internal_id >= 0) {
                    new_internal_ids[new_internal_count++] = new_internal_id;
                }
            }
        }

        /* 转换端口ID */
        int *new_input_ids = NULL;
        int new_input_count = 0;
        if (src_node->data.func_block.input_port_ids && src_node->data.func_block.input_count > 0) {
            new_input_ids = lv_calloc(src_node->data.func_block.input_count, sizeof(int));
            if (!new_input_ids) {
                lv_free((void **) &new_internal_ids);
                goto fail;
            }
            for (int j = 0; j < src_node->data.func_block.input_count; j++) {
                int old_port_id = src_node->data.func_block.input_port_ids[j];
                int new_port_id = id_mapping_find(&id_map, old_port_id);
                if (new_port_id >= 0) {
                    new_input_ids[new_input_count++] = new_port_id;
                }
            }
        }

        int *new_output_ids = NULL;
        int new_output_count = 0;
        if (src_node->data.func_block.output_port_ids && src_node->data.func_block.output_count > 0) {
            new_output_ids = lv_calloc(src_node->data.func_block.output_count, sizeof(int));
            if (!new_output_ids) {
                lv_free((void **) &new_internal_ids);
                lv_free((void **) &new_input_ids);
                goto fail;
            }
            for (int j = 0; j < src_node->data.func_block.output_count; j++) {
                int old_port_id = src_node->data.func_block.output_port_ids[j];
                int new_port_id = id_mapping_find(&id_map, old_port_id);
                if (new_port_id >= 0) {
                    new_output_ids[new_output_count++] = new_port_id;
                }
            }
        }

        AddNodeResult r = graph_add_function_block(dst, new_internal_ids, new_internal_count, new_input_ids,
                                                   new_input_count, new_output_ids, new_output_count);

        lv_free((void **) &new_internal_ids);
        lv_free((void **) &new_input_ids);
        lv_free((void **) &new_output_ids);

        if (r != ADD_NODE_OK)
            goto fail;

        /* 复制确定性状态 */
        GeomNode *new_node = dst->nodes[dst->node_count - 1];
        if (new_node && new_node->type == GEOM_FUNCTION_BLOCK) {
            new_node->data.func_block.determinism_state = src_node->data.func_block.determinism_state;
        }

        int new_id = new_node->id;
        if (!id_mapping_add(&id_map, old_id, new_id))
            goto fail;
    }

    /*
     * 阶段5：拷贝约束
     *
     * 所有节点在阶段1-4中已完成拷贝，映射表已完全填充。
     * 遍历源图的所有约束，将每个约束的 participant IDs 通过映射表
     * 转换为目标图中的新ID，再调用对应的 graph_add_* 函数添加约束。
     *
     * 找不到映射的参与者ID保留原始值（可能是外部引用），
     * 约束添加失败不视为致命错误（通过 (void)r 丢弃返回值），
     * 因为部分约束可能由于节点映射不完整而无法创建。
     */
    for (int i = 0; i < src->constraint_count; i++) {
        Constraint *sc = src->constraints[i];
        AddConstraintResult r;

        /* 转换约束中的参与者ID */
        int *new_participants = lv_calloc(sc->participant_count, sizeof(int));
        if (!new_participants)
            goto fail;
        int new_participant_count = 0;

        for (int j = 0; j < sc->participant_count; j++) {
            int new_id = id_mapping_find(&id_map, sc->participants[j]);
            if (new_id >= 0) {
                new_participants[new_participant_count++] = new_id;
            } else {
                /* 如果找不到映射，使用原始ID（可能是外部引用） */
                new_participants[new_participant_count++] = sc->participants[j];
            }
        }

        if (sc->type >= 0 && sc->type < s_constraint_copy_func_count && s_constraint_copy_funcs[sc->type]) {
            r = s_constraint_copy_funcs[sc->type](dst, new_participants, new_participant_count);
        } else {
            r = ADD_CONSTRAINT_CONFLICT;
        }
        (void) r; /* 约束添加失败不视为致命错误 */
        lv_free((void **) &new_participants);
    }

    /* 清理ID映射表 */
    id_mapping_destroy(&id_map);

    return dst;

fail:
    id_mapping_destroy(&id_map);
    graph_destroy(dst);
    lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "deep_copy_graph: allocation failed during deep copy");
}

/**
 * @brief 实例化命题图中的多态类型变量
 *
 * @details 对命题图进行深拷贝后，将指定节点（type_var_node_id）的端口类型区域
 *          设置为传入的具体类型 concrete_type。此函数用于将带有多态类型变量的
 *          命题模式实例化为具体类型版本，通常发生在合一检查确定类型绑定之后。
 *
 * 【类型区域生命周期管理规则 —— 调用者必读】
 *
 *   本函数对 concrete_type 使用引用语义（浅拷贝/指针赋值），不创建深拷贝。
 *   这意味着调用者必须承担以下生命周期责任：
 *
 *   1. concrete_type 指向的内存必须在 out_instantiated 指向的图存在期间
 *      始终保持有效。任何在实例化图销毁之前释放 concrete_type 的行为都将
 *      导致 inst 图中端口的 type_region 指针成为悬垂指针。
 *
 *   2. 禁止修改 concrete_type 的子字段（子类型、约束ID等），因为所有通过
 *      此函数实例化的端口共享同一份 concrete_type 引用，修改会导致
 *      不可预期的副作用。
 *
 *   3. 销毁规则：调用者应先销毁 out_instantiated（调用 graph_destroy），
 *      再销毁 concrete_type（调用 type_region_destroy）。反向操作将导致
 *      type_region 悬垂指针。注意 graph_destroy 不会释放 type_region，
 *      因为 type_region 的所有权属于调用者。
 *
 *   4. 如果调用者无法保证上述生命周期覆盖，应在调用本函数之前先通过
 *      其他方式创建 concrete_type 的深拷贝，再将拷贝传入。
 *      type_system 模块中已有 type_region_deep_copy() 静态函数提供此能力，
 *      但尚未导出为公共API。规划中将在后续版本中将其导出为
 *      type_system_deep_copy_type_region() 公共接口。
 *
 * 【内存所有权模型】
 *   - proposition: 输入，本函数不获取其所有权，不修改
 *   - concrete_type: 输入，本函数获取其引用（非所有权），调用者负责释放
 *   - out_instantiated: 输出，调用者获得所有权，使用完毕后需 graph_destroy
 *
 * @param proposition     含多态类型变量的命题图（输入，不修改）
 * @param type_var_node_id 待实例化的类型变量节点ID
 * @param concrete_type   具体类型区域（输入，生命周期由调用者管理）
 * @param out_instantiated 输出：实例化后的命题图（调用者获得所有权）
 * @return true 表示实例化成功，false 表示失败（参数无效或内存不足）
 */
bool unify_instantiate_proposition(ConstraintGraph *proposition, int type_var_node_id, const TypeRegion *concrete_type,
                                   ConstraintGraph **out_instantiated) {
    if (!proposition || !concrete_type || !out_instantiated)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "unify_instantiate_proposition: NULL parameter");

    *out_instantiated = NULL;

    /* 深拷贝命题图 */
    ConstraintGraph *inst = deep_copy_graph(proposition);
    if (!inst)
        lv_RETURN_ERROR_BOOL(lv_ERROR_OUT_OF_MEMORY, "unify_instantiate_proposition: deep_copy_graph failed");

    /* 查找类型变量节点 */
    GeomNode *type_var_node = NULL;
    for (int i = 0; i < inst->node_count; i++) {
        if (inst->nodes[i]->id == type_var_node_id) {
            type_var_node = inst->nodes[i];
            break;
        }
    }

    if (type_var_node && type_var_node->type == GEOM_PORT && type_var_node->data.port) {
        /*
         * 【类型区域赋值 —— 引用语义安全性说明】
         *
         * 此处将 concrete_type 指针直接赋值给 type_region，使用浅拷贝
         * （引用语义），而非深拷贝。这是有意为之的设计决策，原因如下：
         *
         * A. 为什么当前使用浅拷贝：
         *    - type_system 模块中已有 type_region_deep_copy() 静态函数，
         *      但该函数尚未导出为公共API（static 修饰，仅 type_system.c 内可见）。
         *    - 将 type_region_deep_copy 提升为公共API需要重新考虑：
         *      a) 深拷贝后谁负责释放（所有权转移语义）
         *      b) 递归子类型的生命周期管理（first_type/second_type 等）
         *      c) contained_node_ids / constraint_ids 在深拷贝后的有效性
         *    - 当前所有调用 unify_instantiate_proposition 的路径都确保
         *      concrete_type 来自 TypeSystem 的注册表（type_regions 数组），
         *      其生命周期由 TypeSystem 管理，通常覆盖整个引擎生命周期。
         *      因此浅拷贝在当前调用场景下实际上是安全的。
         *
         * B. 浅拷贝的风险（在非标准调用路径中）：
         *    1. 悬垂指针：若调用者传入栈上分配的临时 TypeRegion，或调用后
         *       立即销毁 concrete_type，则实例化图中的指针将失效。
         *    2. 别名修改：多个端口共享同一 concrete_type 时，任何一方通过
         *       指针修改其字段（如约束ID、子类型等）将影响所有引用者。
         *    3. 双重释放：若调用者在 graph_destroy 之后仍尝试 type_region_destroy
         *       concrete_type，因为 graph_destroy 中不会释放 type_region（端口
         *       不拥有 type_region 的所有权），不会双重释放；但若未来修改
         *       graph_destroy 实现，则需注意此问题。
         *
         * C. 规划中的修复方案：
         *    1. 短期：在 debug.h 中添加 TYPE_REGION_LIFECYCLE_CHECK 宏，
         *       在调试模式下对 concrete_type 添加哨兵标记，检测悬垂访问。
         *    2. 长期：将 type_region_deep_copy() 导出为公共API
         *       type_system_deep_copy_type_region()，并在本函数中使用它，
         *       同时明确文档化深拷贝后的所有权转移规则。
         *
         * D. 当前的安全保障（调用约定）：
         *    - 调用者约定：concrete_type 必须在 out_instantiated 的整个
         *      生命周期内有效（详见函数头部的生命周期管理规则文档）。
         *    - 调试模式断言：以下检查在调试模式下警告调用者注意生命周期：
          */
        if (debug_is_debug_mode()) {
            if (!concrete_type->alias_name && !concrete_type->variable_name && concrete_type->kind == 0 &&
                concrete_type->level == 0) {
                /* 如果 concrete_type 的所有可识别字段均为零/空，可能是已被销毁
                  * 或未初始化的对象。仅在调试模式下记录警告，不中断执行，
                  * 因为某些合法的类型区域可能确实全部为零值。 */
                debug_log(LOG_LEVEL_WARN, "unify",
                          "unify_instantiate_proposition: concrete_type at %p appears "
                          "to be zero-initialized or destroyed — possible dangling pointer "
                          "risk for node %d",
                          (const void *) concrete_type, type_var_node_id);
            }
        }
        type_var_node->data.port->type_region = (TypeRegion *) concrete_type;
        type_var_node->data.port->is_polymorphic = false;
    }

    *out_instantiated = inst;
    return true;
}

/* ===========================================================================
 * 精细化匹配函数 —— 将合一流程中的各阶段抽取为可独立调用的函数
 *
 * 这些函数将完整的合一检查分解为端口匹配、约束匹配、坐标判等三个
 * 独立阶段，允许外部调用者进行更精细的控制和调试。
 *
 * 所有函数均集成流式事件输出。
 * ===========================================================================
 */

/**
 * @brief 单独执行端口类型匹配
 *
 * 遍历命题图的所有端口节点，在构造图中查找类型、命名空间深度、
 * 类型区域都等价的端口。一个构造端口最多匹配一个命题端口。
 *
 * 流式输出: 匹配每对端口时发出 PROOF_UNIFY 事件，
 * 包含端口类型和命名空间深度的 JSON 详细信息。
 */
int unify_match_ports(const ConstraintGraph *construction, const ConstraintGraph *proposition, int *out_port_bindings,
                      int max_bindings) {
    if (!construction || !proposition)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "unify_match_ports: NULL construction or proposition");

    if (unify_stream_ctx) {
        stream_emit_simple(unify_stream_ctx, STREAM_EVENT_PROOF_UNIFY, "精细端口匹配开始", 0);
    }

    /* 统计命题端口节点数量 */
    int prop_port_count = 0;
    for (int i = 0; i < proposition->node_count; i++) {
        if (proposition->nodes[i]->type == GEOM_PORT)
            prop_port_count++;
    }

    /* 跟踪已匹配的构造端口 */
    bool *used = lv_calloc((size_t) construction->node_count, sizeof(bool));
    if (!used && construction->node_count > 0)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "unify_match_ports: calloc used failed");

    /* 创建 TypeSystem 用于端口类型等价检查 */
    TypeSystem *ts = type_system_create();

    int match_count = 0;

    for (int i = 0; i < proposition->node_count; i++) {
        GeomNode *pn = proposition->nodes[i];
        if (pn->type != GEOM_PORT)
            continue;
        Port *pp = pn->data.port;
        if (!pp)
            continue;

        bool found = false;
        for (int j = 0; j < construction->node_count; j++) {
            GeomNode *cn = construction->nodes[j];
            if (cn->type != GEOM_PORT)
                continue;
            if (used[j])
                continue;
            Port *cp = cn->data.port;
            if (!cp)
                continue;

            /* 类型和命名空间深度匹配 */
            if (pp->type != cp->type)
                continue;
            if (pp->namespace_depth != cp->namespace_depth)
                continue;
            if (pp->parent_block_id != cp->parent_block_id)
                continue;
            if (pp->is_formal_param != cp->is_formal_param)
                continue;

            /* TypeSystem 等价检查 */
            if (pp->type_region && cp->type_region && ts) {
                TypeEquivResult equiv = type_check_equivalence(ts, pp->type_region, cp->type_region, false);
                if (equiv == TYPE_EQUIV_NOT_EQUIV)
                    continue;
            }

            /* 找到匹配 */
            used[j] = true;
            found = true;

            if (out_port_bindings && match_count < max_bindings) {
                out_port_bindings[match_count * 2] = pn->id;
                out_port_bindings[match_count * 2 + 1] = cn->id;
            }
            match_count++;

            if (unify_stream_ctx) {
                StreamEvent ev;
                memset(&ev, 0, sizeof(ev));
                ev.type = STREAM_EVENT_PROOF_UNIFY;
                ev.timestamp_ms = stream_timestamp_ms();
                ev.node_id = cn->id;
                ev.step_number = match_count;
                ev.var_id = pn->id;
                ev.description = "端口匹配成功";
                lvStrBuf sb = {0};
                lv_strbuf_printf(&sb,
                         "{\"prop_port_id\":%d,\"const_port_id\":%d,"
                         "\"port_type\":%d,\"namespace_depth\":%d}",
                         pn->id, cn->id, (int) pp->type, pp->namespace_depth);
                ev.detail_json = sb.data;
                stream_emit(unify_stream_ctx, &ev);
                lv_strbuf_destroy(&sb);
            }
            break;
        }

        if (!found) {
            /* 此命题端口没有匹配的 construction 端口 */
            lv_free((void **) &used);
            if (ts)
                type_system_destroy(ts);
            if (unify_stream_ctx) {
                lvStrBuf sb_2 = {0};
                lv_strbuf_printf(&sb_2, "端口匹配失败: 命题端口 %d (type=%d) 无对应构造端口", pn->id,
                         pp ? (int) pp->type : -1);
                stream_emit_simple(unify_stream_ctx, STREAM_EVENT_PROOF_UNIFY, sb_2.data, match_count);
                lv_strbuf_destroy(&sb_2);
            }
            return -1;
        }
    }

    lv_free((void **) &used);
    if (ts)
        type_system_destroy(ts);

    if (unify_stream_ctx) {
        lvStrBuf sb_3 = {0};
        lv_strbuf_printf(&sb_3, "精细端口匹配完成: %d 对端口匹配成功", match_count);
        stream_emit_simple(unify_stream_ctx, STREAM_EVENT_PROOF_UNIFY, sb_3.data, match_count);
        lv_strbuf_destroy(&sb_3);
    }

    return match_count;
}

/**
 * @brief 单独执行约束匹配
 *
 * 遍历命题图的所有约束，在构造图中查找类型一致且参与者数量
 * 一致的对应约束。仅检查约束拓扑结构，不涉及坐标判等。
 *
 * 流式输出: 匹配每对约束时发出 PROOF_UNIFY 事件。
 */
int unify_match_constraints(const ConstraintGraph *construction, const ConstraintGraph *proposition,
                            int *out_constraint_bindings) {
    if (!construction || !proposition)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "unify_match_constraints: NULL construction or proposition");

    if (unify_stream_ctx) {
        stream_emit_simple(unify_stream_ctx, STREAM_EVENT_PROOF_UNIFY, "精细约束匹配开始", 0);
    }

    int match_count = 0;

    /* 跟踪已匹配的构造约束 */
    bool *used = lv_calloc((size_t) construction->constraint_count, sizeof(bool));
    if (!used && construction->constraint_count > 0)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "unify_match_constraints: calloc used failed");

    for (int i = 0; i < proposition->constraint_count; i++) {
        const Constraint *pc = proposition->constraints[i];
        if (!pc)
            continue;

        bool found = false;
        for (int j = 0; j < construction->constraint_count; j++) {
            const Constraint *cc = construction->constraints[j];
            if (!cc || used[j])
                continue;

            /* 约束类型必须匹配 */
            if (pc->type != cc->type)
                continue;

            /* 参与者数量必须匹配 */
            if (pc->participant_count != cc->participant_count)
                continue;

            /* 检查参与者 ID */
            bool same = true;
            for (int k = 0; k < pc->participant_count; k++) {
                if (pc->participants[k] != cc->participants[k]) {
                    same = false;
                    break;
                }
            }

            if (same) {
                used[j] = true;
                found = true;

                if (out_constraint_bindings) {
                    out_constraint_bindings[match_count * 2] = pc->id;
                    out_constraint_bindings[match_count * 2 + 1] = cc->id;
                }
                match_count++;

                if (unify_stream_ctx) {
                    StreamEvent ev;
                    memset(&ev, 0, sizeof(ev));
                    ev.type = STREAM_EVENT_PROOF_UNIFY;
                    ev.timestamp_ms = stream_timestamp_ms();
                    ev.constraint_id = cc->id;
                    ev.step_number = match_count;
                    ev.description = "约束匹配成功";
                    lvStrBuf sb_4 = {0};
                    lv_strbuf_printf(&sb_4,
                             "{\"prop_constraint_id\":%d,\"const_constraint_id\":%d,"
                             "\"type\":%d,\"participants\":%d}",
                             pc->id, cc->id, (int) pc->type, pc->participant_count);
                    ev.detail_json = sb_4.data;
                    stream_emit(unify_stream_ctx, &ev);
                    lv_strbuf_destroy(&sb_4);
                }
                break;
            }
        }

        if (!found) {
            lv_free((void **) &used);
            if (unify_stream_ctx) {
                lvStrBuf sb_5 = {0};
                lv_strbuf_printf(&sb_5, "约束匹配失败: 命题约束 %d (type=%d) 无对应构造约束", pc->id,
                         (int) pc->type);
                stream_emit_simple(unify_stream_ctx, STREAM_EVENT_PROOF_UNIFY, sb_5.data, match_count);
                lv_strbuf_destroy(&sb_5);
            }
            return -1;
        }
    }

    lv_free((void **) &used); /* 使用 lv_calloc/lv_free 统一内存管理 */

    if (unify_stream_ctx) {
        lvStrBuf sb_6 = {0};
        lv_strbuf_printf(&sb_6, "精细约束匹配完成: %d 对约束匹配成功", match_count);
        stream_emit_simple(unify_stream_ctx, STREAM_EVENT_PROOF_UNIFY, sb_6.data, match_count);
        lv_strbuf_destroy(&sb_6);
    }

    return match_count;
}

/**
 * @brief 单独执行符号坐标判等
 *
 * 使用 symbolic_coord_compare 比较两个坐标，返回比较结果。
 * 0 表示完全相等，非 0 表示不相等。
 * 此函数也为未来扩展坐标等价的更多语义（如归一化后的等价、
 * 模变换后的等价等）预留了扩展点。
 *
 * 流式输出: 仅在结果不相等时发出 PROOF_UNIFY 事件（含详细差异信息）。
 */
int unify_match_coords(const SymbolicCoord *c1, const SymbolicCoord *c2) {
    if (!c1 && !c2)
        return 0; /* 两者均为 NULL 视为相等 */
    if (!c1 || !c2)
        return -1; /* 仅一个为 NULL：不相等 */

    int result = symbolic_coord_compare(c1, c2);

    if (result != 0 && unify_stream_ctx) {
        char *s1 = symbolic_coord_serialize(c1);
        char *s2 = symbolic_coord_serialize(c2);
        lvStrBuf sb_7 = {0};
        lv_strbuf_printf(&sb_7, "坐标不相等: \"%s\" vs \"%s\"", s1 ? s1 : "(null)", s2 ? s2 : "(null)");
        stream_emit_simple(unify_stream_ctx, STREAM_EVENT_PROOF_UNIFY, sb_7.data, 0);
        lv_free((void **) &s1);
        lv_free((void **) &s2);
        lv_strbuf_destroy(&sb_7);
    }

    return result;
}
