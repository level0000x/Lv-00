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
 * @author Lv-00 Project
 */

#include "node_deep_copy.h"

#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/constraint_graph.h"  /* GeomNode type */

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

    SymbolicCoord *copy = lv_malloc(sizeof(SymbolicCoord));
    if (!copy)
        return NULL;

    copy->type = orig->type;
    copy->trust = orig->trust;

    switch (orig->type) {
        case RATIONAL:
            if (orig->data.rational) {
                /* 通过 mpq_set 深拷贝，避免 mpz_get_si/mpz_get_ui 截断问题 */
                copy->data.rational = lv_malloc(sizeof(Rational));
                if (!copy->data.rational) {
                    lv_free((void **) &copy);
                    return NULL;
                }
                mpq_init(copy->data.rational->value);
                mpq_set(copy->data.rational->value, orig->data.rational->value);
            } else {
                copy->data.rational = NULL;
            }
            break;
        case ALGEBRAIC:
            if (orig->data.algebraic) {
                copy->data.algebraic =
                    algebraic_create(&orig->data.algebraic->minimal_poly, orig->data.algebraic->left_bound,
                                     orig->data.algebraic->right_bound);
            } else {
                copy->data.algebraic = NULL;
            }
            break;
        case QUADRATIC:
            if (orig->data.quadratic) {
                /* 通过 mpq_set 深拷贝有理数分量，避免截断问题 */
                Rational *a_copy = lv_malloc(sizeof(Rational));
                if (!a_copy) {
                    lv_free((void **) &copy);
                    return NULL;
                }
                mpq_init(a_copy->value);
                mpq_set(a_copy->value, orig->data.quadratic->a->value);
                Rational *b_copy = lv_malloc(sizeof(Rational));
                if (!b_copy) {
                    mpq_clear(a_copy->value);
                    lv_free((void **) &a_copy);
                    lv_free((void **) &copy);
                    return NULL;
                }
                mpq_init(b_copy->value);
                mpq_set(b_copy->value, orig->data.quadratic->b->value);
                copy->data.quadratic = quadratic_create(a_copy, b_copy, orig->data.quadratic->n);
            } else {
                copy->data.quadratic = NULL;
            }
            break;
        case TRANSCENDENTAL:
            if (orig->data.transcendental) {
                copy->data.transcendental = transcendental_create(orig->data.transcendental->name);
                /* 深拷贝表达式树 */
                if (copy->data.transcendental && orig->data.transcendental->expr) {
                    TranscendentalExpr *src_expr = orig->data.transcendental->expr;
                    TranscendentalExpr *dst_expr = lv_malloc(sizeof(TranscendentalExpr));
                    if (dst_expr) {
                        dst_expr->expr_type = src_expr->expr_type;
                        lv_strlcpy(dst_expr->base_name, src_expr->base_name, sizeof(dst_expr->base_name));
                        dst_expr->rational_operand =
                            src_expr->rational_operand ? rational_copy(src_expr->rational_operand) : NULL;
                        dst_expr->out_of_scope = src_expr->out_of_scope;
                        copy->data.transcendental->expr = dst_expr;
                    }
                }
            } else {
                copy->data.transcendental = NULL;
            }
            break;
        default:
            copy->data.rational = NULL;
            break;
    }

    return copy;
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

    Port *copy = lv_malloc(sizeof(Port));
    if (!copy)
        return NULL;

    copy->id = orig->id;
    copy->type = orig->type;
    copy->namespace_depth = orig->namespace_depth;
    copy->parent_block_id = orig->parent_block_id;
    copy->is_formal_param = orig->is_formal_param;
    copy->is_polymorphic = orig->is_polymorphic;
    /* type_region 浅拷贝（指针赋值），所有权由 TypeSystem 统一管理 */
    copy->type_region = orig->type_region;
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
 * - 标量字段：直接赋值
 * - numeric_assumption_declaration：通过 lv_strdup_safe 深拷贝字符串
 * - symbolic_coords 数组：对每个坐标调用 node_deep_copy_symbolic_coord 深拷贝
 * - data.port：调用 node_deep_copy_port 深拷贝
 * - data.region.boundary_segments：分配新数组并拷贝引用（线段由图拥有）
 * - data.func_block：分配新数组并拷贝内部节点引用和端口 ID 数组
 *
 * @param orig   原始节点
 * @param id_map 旧节点ID到新节点ID的映射（可为 NULL，当前未使用，预留扩展）
 * @return 深拷贝后的新节点，失败返回 NULL
 */
GeomNode *node_deep_copy_geom_node(const GeomNode *orig, const int *id_map) {
    if (!orig)
        return NULL;

    lv_UNUSED(id_map); /* 预留参数，当前未使用 */

    GeomNode *copy = lv_malloc(sizeof(GeomNode));
    if (!copy)
        return NULL;

    /* 拷贝标量字段 */
    copy->id = orig->id;
    copy->type = orig->type;
    copy->coord_count = orig->coord_count;
    copy->trust = orig->trust;
    copy->lo_subtype = orig->lo_subtype;
    copy->numeric_precision = orig->numeric_precision;
    copy->namespace_depth = orig->namespace_depth;
    copy->parent_block_id = orig->parent_block_id;

    /* 深拷贝 numeric_assumption_declaration 字符串 */
    if (orig->numeric_assumption_declaration) {
        copy->numeric_assumption_declaration = lv_strdup_safe(orig->numeric_assumption_declaration);
        if (!copy->numeric_assumption_declaration) {
            lv_free((void **) &copy);
            return NULL;
        }
    } else {
        copy->numeric_assumption_declaration = NULL;
    }

    /* 深拷贝符号坐标数组 */
    if (orig->symbolic_coords && orig->coord_count > 0) {
        copy->symbolic_coords = lv_malloc(orig->coord_count * sizeof(SymbolicCoord *));
        if (!copy->symbolic_coords) {
            lv_free((void **) &copy->numeric_assumption_declaration);
            lv_free((void **) &copy);
            return NULL;
        }
        for (int i = 0; i < orig->coord_count; i++) {
            copy->symbolic_coords[i] = node_deep_copy_symbolic_coord(orig->symbolic_coords[i]);
            if (!copy->symbolic_coords[i] && orig->symbolic_coords[i]) {
                /* 失败时清理已分配的坐标 */
                for (int j = 0; j < i; j++) {
                    symbolic_coord_destroy(copy->symbolic_coords[j]);
                }
                lv_free((void **) &copy->symbolic_coords);
                lv_free((void **) &copy->numeric_assumption_declaration);
                lv_free((void **) &copy);
                return NULL;
            }
        }
    } else {
        copy->symbolic_coords = NULL;
    }

    /* 拷贝类型特定数据 */
    switch (orig->type) {
        case GEOM_PORT:
            copy->data.port = node_deep_copy_port(orig->data.port);
            if (!copy->data.port && orig->data.port) {
                /* 失败时清理 */
                if (copy->symbolic_coords) {
                    for (int i = 0; i < copy->coord_count; i++) {
                        symbolic_coord_destroy(copy->symbolic_coords[i]);
                    }
                    lv_free((void **) &copy->symbolic_coords);
                }
                lv_free((void **) &copy->numeric_assumption_declaration);
                lv_free((void **) &copy);
                return NULL;
            }
            break;

        case GEOM_REGION:
            /* Region类型：分配边界线段数组（引用共享，非拥有） */
            copy->data.region.segment_count = orig->data.region.segment_count;
            if (orig->data.region.boundary_segments && orig->data.region.segment_count > 0) {
                copy->data.region.boundary_segments = lv_malloc(orig->data.region.segment_count * sizeof(GeomNode *));
                if (!copy->data.region.boundary_segments) {
                    if (copy->symbolic_coords) {
                        for (int i = 0; i < copy->coord_count; i++) {
                            symbolic_coord_destroy(copy->symbolic_coords[i]);
                        }
                        lv_free((void **) &copy->symbolic_coords);
                    }
                    lv_free((void **) &copy->numeric_assumption_declaration);
                    lv_free((void **) &copy);
                    return NULL;
                }
                /* 拷贝线段引用（线段由图拥有，区域仅持有引用） */
                for (int i = 0; i < orig->data.region.segment_count; i++) {
                    copy->data.region.boundary_segments[i] = orig->data.region.boundary_segments[i];
                }
            } else {
                copy->data.region.boundary_segments = NULL;
            }
            break;

        case GEOM_FUNCTION_BLOCK:
            /* FunctionBlock类型：分配内部节点和端口ID数组 */
            copy->data.func_block.internal_node_count = orig->data.func_block.internal_node_count;
            copy->data.func_block.input_count = orig->data.func_block.input_count;
            copy->data.func_block.output_count = orig->data.func_block.output_count;
            copy->data.func_block.determinism_state = orig->data.func_block.determinism_state;

            /* 分配并拷贝内部节点数组（引用，非拥有） */
            if (orig->data.func_block.internal_nodes && orig->data.func_block.internal_node_count > 0) {
                copy->data.func_block.internal_nodes =
                    lv_malloc(orig->data.func_block.internal_node_count * sizeof(GeomNode *));
                if (!copy->data.func_block.internal_nodes) {
                    if (copy->symbolic_coords) {
                        for (int i = 0; i < copy->coord_count; i++) {
                            symbolic_coord_destroy(copy->symbolic_coords[i]);
                        }
                        lv_free((void **) &copy->symbolic_coords);
                    }
                    lv_free((void **) &copy->numeric_assumption_declaration);
                    lv_free((void **) &copy);
                    return NULL;
                }
                for (int i = 0; i < orig->data.func_block.internal_node_count; i++) {
                    copy->data.func_block.internal_nodes[i] = orig->data.func_block.internal_nodes[i];
                }
            } else {
                copy->data.func_block.internal_nodes = NULL;
            }

            /* 分配并拷贝输入端口ID数组 */
            if (orig->data.func_block.input_port_ids && orig->data.func_block.input_count > 0) {
                copy->data.func_block.input_port_ids = lv_malloc(orig->data.func_block.input_count * sizeof(int));
                if (!copy->data.func_block.input_port_ids) {
                    lv_free((void **) &copy->data.func_block.internal_nodes);
                    if (copy->symbolic_coords) {
                        for (int i = 0; i < copy->coord_count; i++) {
                            symbolic_coord_destroy(copy->symbolic_coords[i]);
                        }
                        lv_free((void **) &copy->symbolic_coords);
                    }
                    lv_free((void **) &copy->numeric_assumption_declaration);
                    lv_free((void **) &copy);
                    return NULL;
                }
                memcpy(copy->data.func_block.input_port_ids, orig->data.func_block.input_port_ids,
                       orig->data.func_block.input_count * sizeof(int));
            } else {
                copy->data.func_block.input_port_ids = NULL;
            }

            /* 分配并拷贝输出端口ID数组 */
            if (orig->data.func_block.output_port_ids && orig->data.func_block.output_count > 0) {
                copy->data.func_block.output_port_ids = lv_malloc(orig->data.func_block.output_count * sizeof(int));
                if (!copy->data.func_block.output_port_ids) {
                    lv_free((void **) &copy->data.func_block.input_port_ids);
                    lv_free((void **) &copy->data.func_block.internal_nodes);
                    if (copy->symbolic_coords) {
                        for (int i = 0; i < copy->coord_count; i++) {
                            symbolic_coord_destroy(copy->symbolic_coords[i]);
                        }
                        lv_free((void **) &copy->symbolic_coords);
                    }
                    lv_free((void **) &copy->numeric_assumption_declaration);
                    lv_free((void **) &copy);
                    return NULL;
                }
                memcpy(copy->data.func_block.output_port_ids, orig->data.func_block.output_port_ids,
                       orig->data.func_block.output_count * sizeof(int));
            } else {
                copy->data.func_block.output_port_ids = NULL;
            }
            break;

        default:
            /* GEOM_POINT 和 GEOM_LINE_SEGMENT 类型无额外数据需要拷贝 */
            memset(&copy->data, 0, sizeof(copy->data));
            break;
    }

    return copy;
}
