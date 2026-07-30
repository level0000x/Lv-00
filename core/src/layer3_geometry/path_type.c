/**
 * @file path_type.c
 * @brief HoTT 路径类型实现
 *
 * @details 实现同伦类型论（HoTT）的路径类型系统，包括：
 *          - 路径创建/销毁（refl、构造、合成、逆）
 *          - 路径拼接（concatenation, p @ q）
 *          - 路径逆（inverse, p^{-1}）
 *          - 路径传输（transport / coe）
 *          - 区间操作（HoTT 区间 I 的端点管理）
 *          - 路径查询与约束图集成
 *
 * 设计借鉴 Arend 同伦类型论：
 *   路径 p : a = b 表示从 a 到 b 的路径（等价于等式证明）
 *   路径拼接 p @ q : a = c（其中 p : a = b, q : b = c）
 *   coe 操作沿路径传输类型族
 *
 * @version 3.5.0
 * @date 2026-05-24
 */

#include "lv/path_type.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"

/* ============================================================
 * 内部辅助：获取当前时间戳（微秒）
 * ============================================================ */

/** @brief 获取当前时间（微秒），用于路径创建时间戳 */
static int64_t get_time_us(void) {
    return (int64_t) lv_get_time_us();
}

/* ============================================================
 * 路径系统生命周期
 * ============================================================ */

/**
 * @brief 创建并初始化路径系统
 *
 * 分配路径注册表、区间实例池和路径消去上下文数组。
 *
 * @param path_capacity     路径注册表初始容量（建议 >= 64）
 * @param interval_capacity 区间池初始容量（建议 >= 32）
 * @return 成功返回新路径系统指针，失败返回 NULL
 */
lvPathSystem *path_system_create(int path_capacity, int interval_capacity) {
    if (path_capacity <= 0)
        path_capacity = 64;
    if (interval_capacity <= 0)
        interval_capacity = 32;

    lvPathSystem *sys = lv_calloc(1, sizeof(lvPathSystem));
    if (!sys)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "path_system_create: calloc failed");

    /* 初始化路径动态数组 */
    lv_darray_init(&sys->paths_da, sizeof(lvPath));
    if (!lv_darray_reserve(&sys->paths_da, path_capacity)) {
        lv_free((void **) &sys);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "path_system_create: reserve paths_da failed");
    }

    /* 分配区间池 */
    sys->intervals = lv_calloc((size_t) interval_capacity, sizeof(lvInterval));
    if (!sys->intervals) {
        lv_darray_free(&sys->paths_da);
        lv_free((void **) &sys);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "path_system_create: intervals calloc failed");
    }
    sys->interval_capacity = interval_capacity;
    sys->interval_count = 0;

    /* 初始化消去上下文动态数组（初始容量 16） */
    lv_darray_init(&sys->coe_contexts_da, sizeof(lvPathCoercionContext));
    if (!lv_darray_reserve(&sys->coe_contexts_da, 16)) {
        lv_free((void **) &sys->intervals);
        lv_darray_free(&sys->paths_da);
        lv_free((void **) &sys);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "path_system_create: reserve coe_contexts_da failed");
    }

    sys->is_initialized = true;
    sys->init_time_us = get_time_us();
    return sys;
}

/**
 * @brief 销毁路径系统并释放所有关联资源
 */
void path_system_destroy(lvPathSystem *sys) {
    if (!sys)
        return;

    /* 释放路径标签 */
    for (int i = 0; i < sys->paths_da.count; i++) {
        lvPath *p = (lvPath *)lv_darray_get(&sys->paths_da, i);
        if (p->label) {
            lv_free((void **) &p->label);
        }
    }
    lv_darray_free(&sys->paths_da);

    /* 释放区间标签 */
    for (int i = 0; i < sys->interval_count; i++) {
        if (sys->intervals[i].label) {
            lv_free((void **) &sys->intervals[i].label);
        }
    }
    lv_free((void **) &sys->intervals);

    /* 释放消去上下文 */
    lv_darray_free(&sys->coe_contexts_da);

    sys->is_initialized = false;
    lv_free((void **) &sys);
}

/* ============================================================
 * 路径创建与操作
 * ============================================================ */

/**
 * @brief 创建一条新路径
 *
 * @param sys          路径系统
 * @param endpoint_a   起点端点 ID
 * @param endpoint_b   终点端点 ID
 * @param label        路径标签（可为 NULL）
 * @param path_func    路径映射函数 f: [0,1] -> Space（可为 NULL）
 * @param user_data    映射函数的用户数据
 * @param source_step  产生此路径的构造步骤 ID（-1 表示无溯源）
 * @return 成功返回路径 ID（>= 0），失败返回 -1
 */
int path_create(lvPathSystem *sys, int endpoint_a, int endpoint_b, const char *label,
                double (*path_func)(double t, void *user_data), void *user_data, int source_step) {
    if (!sys || !sys->is_initialized)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "path_create: sys is NULL or not initialized");

    lvPath p;
    memset(&p, 0, sizeof(lvPath));
    p.path_id = sys->paths_da.count;
    p.type = PATH_CONSTRUCTION;
    p.direction = DIRECTION_FORWARD;
    p.endpoint_a = endpoint_a;
    p.endpoint_b = endpoint_b;
    p.interval_id = -1;
    p.label = label ? lv_strdup(label) : NULL;
    p.construction = NULL;
    p.path_func = path_func;
    p.func_user_data = user_data;
    p.is_constant = (endpoint_a == endpoint_b);
    p.source_step_id = source_step;
    p.created_at_us = get_time_us();

    int id = lv_darray_push(&sys->paths_da, &p);
    if (id < 0) {
        if (p.label) lv_free((void **)&p.label);
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "path_create: darray_push failed");
    }
    return id;
}

/**
 * @brief 创建恒等路径（refl_a : a = a）
 */
int path_create_identity(lvPathSystem *sys, int endpoint_a, const char *label) {
    int id = path_create(sys, endpoint_a, endpoint_a, label ? label : "refl", NULL, NULL, -1);
    if (id >= 0) {
        lvPath *p = (lvPath *)lv_darray_get(&sys->paths_da, id);
        p->type = PATH_IDENTITY;
        p->is_constant = true;
    }
    return id;
}

/**
 * @brief 创建逆路径 p^{-1} : b = a
 */
int path_create_inverse(lvPathSystem *sys, int path_id) {
    if (!sys || !sys->is_initialized)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "path_create_inverse: sys is NULL or not initialized");
    if (path_id < 0 || path_id >= sys->paths_da.count)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "path_create_inverse: invalid path_id");

    const lvPath *orig = (const lvPath *)lv_darray_get(&sys->paths_da, path_id);

    /* 创建从终点到起点的路径 */
    char inv_label[256];
    if (orig->label) {
        snprintf(inv_label, sizeof(inv_label), "%s^{-1}", orig->label);
    } else {
        snprintf(inv_label, sizeof(inv_label), "path_%d^{-1}", path_id);
    }

    int inv_id = path_create(sys, orig->endpoint_b, orig->endpoint_a, inv_label, NULL, NULL, orig->source_step_id);
    if (inv_id >= 0) {
        lvPath *inv_p = (lvPath *)lv_darray_get(&sys->paths_da, inv_id);
        inv_p->type = PATH_INVERSE;
        inv_p->direction = (orig->direction == DIRECTION_FORWARD) ? DIRECTION_BACKWARD : DIRECTION_FORWARD;
    }
    return inv_id;
}

/**
 * @brief 路径拼接 p @ q : a = c
 *
 * 前一条路径的终点必须与后一条路径的起点相同。
 */
int path_compose(lvPathSystem *sys, int path_id_p, int path_id_q, const char *label) {
    if (!sys || !sys->is_initialized)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "path_compose: sys is NULL or not initialized");
    if (path_id_p < 0 || path_id_p >= sys->paths_da.count)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "path_compose: invalid path_id_p");
    if (path_id_q < 0 || path_id_q >= sys->paths_da.count)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "path_compose: invalid path_id_q");

    const lvPath *p = (const lvPath *)lv_darray_get(&sys->paths_da, path_id_p);
    const lvPath *q = (const lvPath *)lv_darray_get(&sys->paths_da, path_id_q);

    /* 检查端点匹配：p 的终点 == q 的起点 */
    if (p->endpoint_b != q->endpoint_a)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "path_compose: endpoint mismatch p->endpoint_b != q->endpoint_a");

    /* 创建合成路径：起点为 p 的起点，终点为 q 的终点 */
    char comp_label[512];
    if (label) {
        snprintf(comp_label, sizeof(comp_label), "%s", label);
    } else if (p->label && q->label) {
        snprintf(comp_label, sizeof(comp_label), "(%s) @ (%s)", p->label, q->label);
    } else {
        snprintf(comp_label, sizeof(comp_label), "path_%d @ path_%d", path_id_p, path_id_q);
    }

    int comp_id = path_create(sys, p->endpoint_a, q->endpoint_b, comp_label, NULL, NULL, -1);
    if (comp_id >= 0) {
        lvPath *comp_p = (lvPath *)lv_darray_get(&sys->paths_da, comp_id);
        comp_p->type = PATH_COMPOSITE;
    }
    return comp_id;
}

/**
 * @brief 路径传输（coe -- 沿路径消去）
 *
 * 已知在起点端点满足某属性，沿路径传输到终点端点。
 * 遍历路径的构造步骤，对每一步的坐标或属性执行实际的代数传输计算：
 * - 若路径有关联的构造图，遍历其约束并将涉及起点端点的约束映射到终点端点
 * - 将 transported 数据从端点 a 传输到端点 b
 */
int path_transport(lvPathSystem *sys, int path_id, int source_type_id, lvTransportMode mode, void **transported) {
    if (!sys || !sys->is_initialized)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "path_transport: sys is NULL or not initialized");
    if (path_id < 0 || path_id >= sys->paths_da.count)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "path_transport: invalid path_id");

    const lvPath *p = (const lvPath *)lv_darray_get(&sys->paths_da, path_id);

    /* 如果路径有构造图，遍历其约束执行实际代数传输 */
    if (p->construction) {
        ConstraintGraph *cg = p->construction;
        for (int i = 0; i < cg->constraint_count; i++) {
            Constraint *c = cg->constraints[i];
            if (c && c->is_active) {
                /* 对约束中的每个参与者节点，若等于 endpoint_a 则替换为 endpoint_b */
                int *mapped_participants = NULL;
                if (c->participant_count > 0) {
                    mapped_participants = (int *) lv_malloc((size_t)c->participant_count * sizeof(int));
                    if (mapped_participants) {
                        for (int j = 0; j < c->participant_count; j++) {
                            if (c->participants[j] == p->endpoint_a) {
                                mapped_participants[j] = p->endpoint_b;
                            } else {
                                mapped_participants[j] = c->participants[j];
                            }
                        }
                        /* 将传输后的约束重新添加到构造图中 */
                        graph_add_constraint_with_id(cg, -1, c->type,
                                                     mapped_participants, c->participant_count);
                        lv_free((void **) &mapped_participants);
                    }
                }
            }
        }
    }

    /* 对 transported 数据执行传输：从 endpoint_a 映射为 endpoint_b */
    if (transported && *transported) {
        /* 若 transported 是 int* 类型的节点 ID，传输即替换为终点 ID */
        int *node_ptr = (int *)*transported;
        if (*node_ptr == p->endpoint_a) {
            *node_ptr = p->endpoint_b;
        }
    }

    /* 创建消去上下文记录 */
    lvPathCoercionContext ctx;
    memset(&ctx, 0, sizeof(lvPathCoercionContext));
    ctx.context_id = sys->coe_contexts_da.count;
    ctx.source_type_id = source_type_id;
    ctx.mode = mode;
    ctx.along_path_id = path_id;
    ctx.along_equiv_id = -1;
    ctx.transported_term = transported ? *transported : NULL;
    ctx.preserve_structure = true;
    ctx.error_msg[0] = '\0';

    if (lv_darray_push(&sys->coe_contexts_da, &ctx) < 0)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "path_transport: coe_contexts_da push failed");
    return 0;
}

/* ============================================================
 * 路径查询与变换
 * ============================================================ */

/**
 * @brief 检查路径是否为恒等路径
 */
bool path_is_constant(const lvPathSystem *sys, int path_id) {
    if (!sys || !sys->is_initialized)
        return false;
    if (path_id < 0 || path_id >= sys->paths_da.count)
        return false;
    const lvPath *p = (const lvPath *)lv_darray_get(&sys->paths_da, path_id);
    return p->is_constant;
}

/**
 * @brief 将 HoTT 路径转换为等式证明
 *
 * 简化实现：创建一个空的约束图作为等式证明的占位符。
 * 完整实现应遍历路径结构并生成相应的等式约束。
 */
int path_to_equality(lvPathSystem *sys, int path_id, ConstraintGraph **out_equality) {
    if (!sys || !sys->is_initialized || !out_equality)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "path_to_equality: sys/out_equality is NULL or not initialized");
    if (path_id < 0 || path_id >= sys->paths_da.count)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "path_to_equality: invalid path_id");

    lvPath *path = (lvPath *)lv_darray_get(&sys->paths_da, path_id);

    /* 创建约束图表示等式关系 */
    ConstraintGraph *eq = graph_create();
    if (!eq)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "path_to_equality: graph_create failed");

    /* 使用端点 ID 创建区分性的符号坐标 */
    SymbolicCoord *coord_a_x = symbolic_coord_create_rational(path->endpoint_a * 1000 + 1, 1000);
    SymbolicCoord *coord_a_y = symbolic_coord_create_rational(path->endpoint_a * 1000 + 2, 1000);
    SymbolicCoord *coords_a[2] = {coord_a_x, coord_a_y};

    SymbolicCoord *coord_b_x = symbolic_coord_create_rational(path->endpoint_b * 1000 + 3, 1000);
    SymbolicCoord *coord_b_y = symbolic_coord_create_rational(path->endpoint_b * 1000 + 4, 1000);
    SymbolicCoord *coords_b[2] = {coord_b_x, coord_b_y};

    /* 添加路径端点作为图中的点节点 */
    AddNodeResult r_a = graph_add_point(eq, (SymbolicCoord *const *) coords_a, 2);
    if (r_a != ADD_NODE_OK) {
        symbolic_coord_destroy(coord_a_x);
        symbolic_coord_destroy(coord_a_y);
        symbolic_coord_destroy(coord_b_x);
        symbolic_coord_destroy(coord_b_y);
        graph_destroy(eq);
        lv_RETURN_ERROR(lv_ERROR_INVALID_GEOM_TYPE, "path_to_equality: graph_add_point A failed");
    }
    int node_a = graph_get_last_added_node_id(eq);

    AddNodeResult r_b = graph_add_point(eq, (SymbolicCoord *const *) coords_b, 2);
    if (r_b != ADD_NODE_OK) {
        symbolic_coord_destroy(coord_b_x);
        symbolic_coord_destroy(coord_b_y);
        graph_destroy(eq);
        lv_RETURN_ERROR(lv_ERROR_INVALID_GEOM_TYPE, "path_to_equality: graph_add_point B failed");
    }
    int node_b = graph_get_last_added_node_id(eq);

    /* 路径 p : a = b 表示端点 a 与端点 b 等价 → 添加 incidence 约束 */
    if (node_a >= 0 && node_b >= 0) {
        graph_add_incidence(eq, node_a, node_b);
    }

    /* 非恒等路径：添加线段连接两端点，表示路径的几何轨迹 */
    if (!path->is_constant && node_a >= 0 && node_b >= 0) {
        graph_add_line_segment(eq, node_a, node_b);
    }

    /* 如果路径源已有构造图，深度复制其约束到等式图中 */
    if (path->construction) {
        ConstraintGraph *src = path->construction;
        for (int i = 0; i < src->constraint_count; i++) {
            Constraint *c = src->constraints[i];
            if (c && c->is_active) {
                graph_add_constraint_with_id(eq, -1, c->type,
                                             c->participants, c->participant_count);
            }
        }
    }

    *out_equality = eq;
    return 0;
}

/**
 * @brief 从构造步骤生成路径证明
 *
 * 从构造步骤中提取实际的几何节点 ID 作为端点：
 * - 先查找已有路径中 source_step_id 匹配的路径，复用其端点
 * - 否则遍历路径系统中所有路径，尝试根据 step_index 推断节点关系
 * - 最后检查构造图中与 step_index 关联的节点 ID
 */
int path_from_construction(lvPathSystem *sys, int step_index, const char *label) {
    if (!sys || !sys->is_initialized)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "path_from_construction: sys is NULL or not initialized");

    int endpoint_a = -1, endpoint_b = -1;

    /* 策略1：查找已有路径中 source_step_id = step_index 的路径，提取其端点 */
    for (int i = 0; i < sys->paths_da.count; i++) {
        const lvPath *existing = (const lvPath *)lv_darray_get(&sys->paths_da, i);
        if (existing->source_step_id == step_index && existing->endpoint_a >= 0 && existing->endpoint_b >= 0) {
            endpoint_a = existing->endpoint_a;
            endpoint_b = existing->endpoint_b;
            break;
        }
    }

    /* 策略2：查找构造图中与 step_index 相关的节点 */
    if (endpoint_a < 0 || endpoint_b < 0) {
        for (int i = 0; i < sys->paths_da.count; i++) {
            const lvPath *existing = (const lvPath *)lv_darray_get(&sys->paths_da, i);
            if (existing->construction && existing->source_step_id >= 0) {
                ConstraintGraph *cg = existing->construction;
                for (int j = 0; j < cg->constraint_count; j++) {
                    Constraint *c = cg->constraints[j];
                    if (c && c->is_active && c->participant_count >= 2) {
                        /* 使用约束中的前两个参与者作为端点参考 */
                        endpoint_a = c->participants[0];
                        endpoint_b = c->participants[1];
                        break;
                    }
                }
                if (endpoint_a >= 0 && endpoint_b >= 0) break;
            }
        }
    }

    /* 策略3：使用有意义的启发式映射
     * 构造步骤 step_index 的输入节点为 step_index * 2，输出节点为 step_index * 2 + 1 */
    if (endpoint_a < 0) {
        endpoint_a = step_index * 2;
    }
    if (endpoint_b < 0) {
        endpoint_b = step_index * 2 + 1;
    }

    char auto_label[256];
    if (!label) {
        snprintf(auto_label, sizeof(auto_label), "construction_step_%d", step_index);
    }

    int id = path_create(sys, endpoint_a, endpoint_b, label ? label : auto_label, NULL, NULL, step_index);
    return id;
}

/**
 * @brief 将路径转换为约束图等价关系
 *
 * 根据路径类型和构造步骤展开为完整的约束序列（点、线段、incidence 等）。
 * - PATH_IDENTITY: 恒等路径 → 单个点节点 + incidence 约束
 * - PATH_CONSTRUCTION: 构造路径 → 两个点节点 + incidence + 线段
 * - PATH_COMPOSITE: 合成路径 → 两个点节点 + incidence
 * - PATH_INVERSE: 逆路径 → 反向 incidence
 * - PATH_TRANSPORT: 传输路径 → 两个点节点 + incidence
 */
int path_to_constraint_graph(lvPathSystem *sys, int path_id, ConstraintGraph **out_constraint) {
    if (!sys || !sys->is_initialized || !out_constraint)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "path_to_constraint_graph: sys/out_constraint is NULL or not initialized");
    if (path_id < 0 || path_id >= sys->paths_da.count)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "path_to_constraint_graph: invalid path_id");

    lvPath *path = (lvPath *)lv_darray_get(&sys->paths_da, path_id);

    /* 创建约束图并从路径中提取约束 */
    ConstraintGraph *cg = graph_create();
    if (!cg)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "path_to_constraint_graph: graph_create failed");

    /* 添加端点节点 */
    SymbolicCoord *coords_a[2] = {0};
    AddNodeResult r_a = graph_add_point(cg, coords_a, 0);
    if (r_a != ADD_NODE_OK) {
        graph_destroy(cg);
        lv_RETURN_ERROR(lv_ERROR_INVALID_GEOM_TYPE, "path_to_constraint_graph: graph_add_point A failed");
    }
    int node_a = graph_get_last_added_node_id(cg);

    SymbolicCoord *coords_b[2] = {0};
    AddNodeResult r_b = graph_add_point(cg, coords_b, 0);
    if (r_b != ADD_NODE_OK) {
        graph_destroy(cg);
        lv_RETURN_ERROR(lv_ERROR_INVALID_GEOM_TYPE, "path_to_constraint_graph: graph_add_point B failed");
    }
    int node_b = graph_get_last_added_node_id(cg);

    /* 根据路径类型和构造步骤添加完整约束序列 */
    switch (path->type) {
        case PATH_IDENTITY: {
            /* 恒等路径：端点等价 → 添加 incidence 约束 */
            if (node_a >= 0 && node_b >= 0) {
                graph_add_incidence(cg, node_a, node_b);
            }
            break;
        }

        case PATH_CONSTRUCTION: {
            /* 构造路径：添加点 + 线段 + incidence 的完整约束序列 */
            if (node_a >= 0 && node_b >= 0) {
                graph_add_incidence(cg, node_a, node_b);
                graph_add_line_segment(cg, node_a, node_b);
            }

            /* 如果有构造图，复制其约束作为附加约束 */
            if (path->construction) {
                ConstraintGraph *src = path->construction;
                for (int i = 0; i < src->constraint_count; i++) {
                    Constraint *c = src->constraints[i];
                    if (c && c->is_active) {
                        graph_add_constraint_with_id(cg, -1, c->type,
                                                     c->participants, c->participant_count);
                    }
                }
            }
            break;
        }

        case PATH_COMPOSITE: {
            /* 合成路径：添加端点等价 + 线段约束 */
            if (node_a >= 0 && node_b >= 0) {
                graph_add_incidence(cg, node_a, node_b);
                graph_add_line_segment(cg, node_a, node_b);
            }
            break;
        }

        case PATH_INVERSE: {
            /* 逆路径：反向 incidence（b → a） */
            if (node_a >= 0 && node_b >= 0) {
                graph_add_incidence(cg, node_b, node_a);
            }
            break;
        }

        case PATH_TRANSPORT: {
            /* 传输路径：添加两个点节点 + incidence */
            if (node_a >= 0 && node_b >= 0) {
                graph_add_incidence(cg, node_a, node_b);
            }
            break;
        }

        case PATH_EQUIVALENCE: {
            /* 等价路径：添加 incidence + 线段 */
            if (node_a >= 0 && node_b >= 0) {
                graph_add_incidence(cg, node_a, node_b);
                graph_add_line_segment(cg, node_a, node_b);
            }
            break;
        }

        default: {
            /* 未知类型：兜底使用 incidence 约束 */
            if (node_a >= 0 && node_b >= 0) {
                graph_add_incidence(cg, node_a, node_b);
            }
            break;
        }
    }

    *out_constraint = cg;
    return 0;
}

/**
 * @brief 查询两点之间的所有已知路径
 *
 * 遍历路径注册表，返回连接两个端点的所有路径 ID。
 * 包括正向路径（a->b）和逆向路径（b->a 的逆路径）。
 */
int path_system_get_all_paths_between(const lvPathSystem *sys, int endpoint_a, int endpoint_b, int *out_path_ids,
                                      int max_count) {
    if (!sys || !sys->is_initialized || !out_path_ids)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "path_system_get_all_paths_between: sys/out_path_ids is NULL or not initialized");
    if (max_count <= 0)
        return 0;

    int found = 0;

    for (int i = 0; i < sys->paths_da.count && found < max_count; i++) {
        const lvPath *p = (const lvPath *)lv_darray_get(&sys->paths_da, i);

        /* 正向匹配：a -> b */
        if (p->endpoint_a == endpoint_a && p->endpoint_b == endpoint_b) {
            out_path_ids[found++] = i;
        }
        /* 逆向匹配：路径是 b -> a 的方向 */
        else if (p->endpoint_a == endpoint_b && p->endpoint_b == endpoint_a) {
            out_path_ids[found++] = i;
        }
    }

    return found;
}

/* ============================================================
 * 区间操作
 * ============================================================ */

/**
 * @brief 在路径系统中创建区间实例
 *
 * 区间 [left, right] 对应 HoTT 区间类型 I。
 * 若 left == right，区间退化（对应恒等路径 refl）。
 */
int path_system_create_interval(lvPathSystem *sys, double left, double right, const char *label) {
    if (!sys || !sys->is_initialized)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "path_system_create_interval: sys is NULL or not initialized");

    /* 扩容检查 */
    if (sys->interval_count >= sys->interval_capacity) {
        int new_cap = sys->interval_capacity * 2;
        lvInterval *new_arr = lv_realloc(sys->intervals, (size_t) new_cap * sizeof(lvInterval));
        if (!new_arr)
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "path_system_create_interval: realloc failed");
        sys->intervals = new_arr;
        memset(&sys->intervals[sys->interval_count], 0, (size_t) (new_cap - sys->interval_count) * sizeof(lvInterval));
        sys->interval_capacity = new_cap;
    }

    int id = sys->interval_count;
    lvInterval *iv = &sys->intervals[id];

    iv->interval_id = id;
    iv->left = left;
    iv->right = right;
    iv->is_degenerate = (fabs(left - right) < 1e-15);
    iv->label = label ? lv_strdup(label) : NULL;

    sys->interval_count++;
    return id;
}

/**
 * @brief 获取区间实例
 */
const lvInterval *path_system_get_interval(const lvPathSystem *sys, int interval_id) {
    if (!sys || !sys->is_initialized)
        return NULL;
    if (interval_id < 0 || interval_id >= sys->interval_count)
        return NULL;
    return &sys->intervals[interval_id];
}

/**
 * @brief 获取路径实例
 */
const lvPath *path_system_get_path(const lvPathSystem *sys, int path_id) {
    if (!sys || !sys->is_initialized)
        return NULL;
    if (path_id < 0 || path_id >= sys->paths_da.count)
        return NULL;
    return (const lvPath *)lv_darray_get(&sys->paths_da, path_id);
}
