/**
 * @file lambda_to_graph.c
 * @brief λ-项到约束图的编译和反向转换
 *
 * 根据 Lv-00 设计文档 8.1 节，λ-演算的几何编码将 λ-项编译为
 * 约束图中的函数块：
 * - λx.M = 标准 Lv-00 函数块，输入端口对应 x，内部子图为 M
 * - 函数应用 = 函数块的输入端口连接到实参的输出端口
 * - α-等价 = 端口连接图相同，自然满足
 *
 * 实现两个方向的转换：
 * 1. lambda_to_graph()：λ-项 → 约束图（编译方向）
 * 2. graph_to_lambda()：约束图 → λ-项（反编译方向）
 */

#include "lambda_to_graph.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/func_block.h"
#include "lv/lambda_term.h"

#include "debug.h"
#include "lv_internal.h"

/* ===========================================================================
 * 内部辅助：λ-编译作用域栈
 *
 * 维护 De Bruijn 索引到端口节点 ID 的映射。
 * 每进入一个 λ-抽象（ABS），将绑定变量的端口压栈；
 * 离开时弹栈。De Bruijn 索引 i 对应栈中从栈顶向下的第 i 个元素
 * （i=0 为最近/最内层的绑定）。
 * =========================================================================== */

#define lv_SCOPE_INIT_CAPACITY 16

/** @brief λ-编译作用域栈 */
typedef struct {
    int *port_ids; /**< De Bruijn 索引 → 端口节点 ID 映射表 */
    int depth;     /**< 当前作用域深度 */
    int capacity;  /**< port_ids 数组容量 */
} LambdaScope;

/**
 * @brief 初始化作用域栈
 */
static void scope_init(LambdaScope *scope) {
    scope->port_ids = lv_calloc((size_t) lv_SCOPE_INIT_CAPACITY, sizeof(int));
    scope->depth = 0;
    scope->capacity = scope->port_ids ? lv_SCOPE_INIT_CAPACITY : 0;
}

/**
 * @brief 销毁作用域栈，释放内部内存
 */
static void scope_destroy(LambdaScope *scope) {
    if (scope && scope->port_ids) {
        lv_free((void **) &scope->port_ids);
    }
    if (scope) {
        scope->depth = 0;
        scope->capacity = 0;
    }
}

/**
 * @brief 将端口 ID 压入作用域栈（绑定新变量）
 */
static bool scope_push(LambdaScope *scope, int port_id) {
    if (!scope)
        return false;

    if (scope->depth >= scope->capacity) {
        if (scope->capacity > INT_MAX / 2)
            return false;
        int new_cap = scope->capacity ? scope->capacity * 2 : lv_SCOPE_INIT_CAPACITY;
        int *new_arr = lv_realloc(scope->port_ids, (size_t) new_cap * sizeof(int));
        if (!new_arr)
            return false;
        scope->port_ids = new_arr;
        scope->capacity = new_cap;
    }

    scope->port_ids[scope->depth++] = port_id;
    return true;
}

/**
 * @brief 从作用域栈弹出一个端口
 */
static void scope_pop(LambdaScope *scope) {
    if (scope && scope->depth > 0) {
        scope->depth--;
    }
}

/**
 * @brief 查询 De Bruijn 索引对应的端口节点 ID
 *
 * @param scope 作用域栈
 * @param index De Bruijn 索引（0 = 最内层绑定）
 * @return 端口节点 ID，越界返回 -1
 */
static int scope_lookup(const LambdaScope *scope, int index) {
    if (!scope || index < 0 || index >= scope->depth)
        return -1;
    /* De Bruijn index 0 = 最近绑定的变量 = 栈顶 */
    return scope->port_ids[scope->depth - 1 - index];
}

/* ===========================================================================
 * 内部辅助：收集函数块的内部节点 ID
 * =========================================================================== */

/**
 * @brief 从 graph 中收集从 start_index 到当前末尾的所有节点 ID
 *
 * @param graph       约束图
 * @param start_index 起始节点数组索引（含）
 * @param out_count   输出：收集到的节点数量
 * @return 新分配的节点 ID 数组（调用者负责 lv_free），失败返回 NULL
 */
static int *collect_node_ids_since(const ConstraintGraph *graph, int start_index, int *out_count) {
    if (!graph || !out_count)
        return NULL;

    int count = graph->node_count - start_index;
    if (count <= 0) {
        *out_count = 0;
        return NULL;
    }

    int *ids = lv_calloc((size_t) count, sizeof(int));
    if (!ids) {
        *out_count = 0;
        return NULL;
    }

    for (int i = 0; i < count; i++) {
        GeomNode *node = graph->nodes[start_index + i];
        ids[i] = node ? node->id : -1;
    }

    *out_count = count;
    return ids;
}

/**
 * @brief 在约束图中检查一个节点 ID 是否为端口且是形式参数
 */
static bool node_is_formal_param(const ConstraintGraph *graph, int node_id) {
    GeomNode *node = graph_get_node(graph, node_id);
    if (!node || node->type != GEOM_PORT)
        return false;
    if (!node->data.port)
        return false;
    return node->data.port->is_formal_param;
}

/**
 * @brief 在约束图中查找某个节点的第一个连接（CONNECTION）目标
 *
 * @param graph     约束图
 * @param node_id   源节点 ID
 * @param direction 方向：0 = 查找 node_id 作为 src 的连接；1 = 查找作为 dst 的连接
 * @param out_target 输出：目标节点 ID
 * @return true 找到连接
 */
static bool find_connection_target(const ConstraintGraph *graph, int node_id, int direction, int *out_target) {
    if (!graph || !out_target)
        return false;
    *out_target = -1;

    int con_indices[32];
    int con_count = graph_find_constraints_involving(graph, node_id, con_indices, 32);

    for (int c = 0; c < con_count; c++) {
        Constraint *con = graph_get_constraint(graph, con_indices[c]);
        if (!con || !con->is_active)
            continue;
        if (con->type != CONNECTION)
            continue;
        if (con->participant_count != 2)
            continue;

        int src = con->participants[0];
        int dst = con->participants[1];

        if (direction == 0 && src == node_id) {
            *out_target = dst;
            return true;
        }
        if (direction == 1 && dst == node_id) {
            *out_target = src;
            return true;
        }
    }

    return false;
}

/* ===========================================================================
 * 内部辅助：递归编译 λ-项（核心实现）
 * =========================================================================== */

/**
 * @brief 递归编译 λ-项的内部实现
 *
 * @param term      待编译的 λ-项
 * @param graph     目标约束图
 * @param scope     当前作用域栈
 * @param depth     当前 λ-嵌套深度（用于端口的 namespace_depth）
 * @param out_node_id 输出：根节点 ID
 * @return true 编译成功
 */
static bool lambda_to_graph_internal(LvLambdaTerm *term, ConstraintGraph *graph, LambdaScope *scope, int depth,
                                     int *out_node_id) {
    if (!term || !graph || !scope || !out_node_id)
        return false;
    *out_node_id = -1;

    switch (term->type) {
        /* ================================================================
     * LV_LAMBDA_VAR(index): 创建端口节点
     *
     * 在约束图中创建一个 PORT_INPUT 节点，代表对该绑定变量的引用。
     * is_formal_param 设为 false（这是变量引用而非绑定声明），
     * 但为了兼容性，对于变量引用我们设 is_formal_param = false。
     * ================================================================ */
        case LV_LAMBDA_VAR: {
            /* 查找 De Bruijn 索引对应的端口 */
            int index = term->data.var.index;
            int port_id = scope_lookup(scope, index);
            if (port_id < 0) {
                LOG_ERROR("lambda_to_graph", "自由的 De Bruijn 索引 %d（作用域深度 %d）", index, scope->depth);
                return false;
            }
            *out_node_id = port_id;
            return true;
        }

        /* ================================================================
     * LV_LAMBDA_ABS(binder, body): 创建函数块
     *
     * 1. 创建输入端（PORT_INPUT, is_formal_param=true）
     * 2. 创建输出端（PORT_OUTPUT）
     * 3. 将 binder 压入作用域
     * 4. 递归编译 body
     * 5. 将 body 的输出连接到函数块的输出端
     * 6. 收集所有新节点打包成函数块
     * ================================================================ */
        case LV_LAMBDA_ABS: {
            int binder = term->data.abs.binder;
            LvLambdaTerm *body = term->data.abs.body;
            if (!body) {
                LOG_ERROR("lambda_to_graph", "ABS 体为空");
                return false;
            }

            /* 记录当前节点数，用于后续收集内部节点 */
            int start_node_count = graph->node_count;

            /* 1. 创建输入端口（对应 binder） */
            AddNodeResult nr = graph_add_port(graph, PORT_INPUT, depth, -1);
            if (nr != ADD_NODE_OK) {
                LOG_ERROR("lambda_to_graph", "创建输入端口失败");
                return false;
            }
            int input_port_id = graph_get_last_added_node_id(graph);

            /* 设置 is_formal_param = true 标记为形式参数 */
            GeomNode *input_node = graph_get_node(graph, input_port_id);
            if (input_node && input_node->data.port) {
                input_node->data.port->is_formal_param = true;
            }

            /* 2. 创建输出端口 */
            nr = graph_add_port(graph, PORT_OUTPUT, depth, -1);
            if (nr != ADD_NODE_OK) {
                LOG_ERROR("lambda_to_graph", "创建输出端口失败");
                return false;
            }
            int output_port_id = graph_get_last_added_node_id(graph);

            /* 3. 将 binder 压入作用域 */
            if (!scope_push(scope, input_port_id)) {
                LOG_ERROR("lambda_to_graph", "作用域压栈失败");
                return false;
            }

            /* 4. 递归编译 body */
            int body_root_id = -1;
            if (!lambda_to_graph_internal(body, graph, scope, depth + 1, &body_root_id)) {
                scope_pop(scope);
                LOG_ERROR("lambda_to_graph", "编译 ABS body 失败");
                return false;
            }

            /* 5. body 的输出 → 函数块的输出端 */
            if (body_root_id >= 0) {
                AddConstraintResult cr = graph_add_connection(graph, body_root_id, output_port_id);
                if (cr != ADD_CONSTRAINT_OK && cr != ADD_CONSTRAINT_DUPLICATE) {
                    LOG_WARN("lambda_to_graph", "连接 body->output 失败");
                }
            }

            scope_pop(scope);

            /* 6. 收集所有新创建的节点，打包成函数块 */
            int internal_count = 0;
            int *internal_ids = collect_node_ids_since(graph, start_node_count, &internal_count);
            if (!internal_ids || internal_count <= 0) {
                LOG_ERROR("lambda_to_graph", "收集内部节点失败");
                lv_free((void **) &internal_ids);
                return false;
            }

            int input_ids[] = {input_port_id};
            int output_ids[] = {output_port_id};

            nr = graph_add_function_block(graph, internal_ids, internal_count, input_ids, 1, output_ids, 1);
            lv_free((void **) &internal_ids);

            if (nr != ADD_NODE_OK) {
                LOG_ERROR("lambda_to_graph", "创建函数块失败");
                return false;
            }

            int fb_id = graph_get_last_added_node_id(graph);

            /* 更新端口节点的 parent_block_id */
            GeomNode *fb_node = graph_get_node(graph, fb_id);
            if (fb_node) {
                int *fb_input_ids = fb_node->data.func_block.input_port_ids;
                int *fb_output_ids = fb_node->data.func_block.output_port_ids;
                for (int i = 0; i < fb_node->data.func_block.input_count && fb_input_ids; i++) {
                    GeomNode *pn = graph_get_node(graph, fb_input_ids[i]);
                    if (pn)
                        pn->parent_block_id = fb_id;
                }
                for (int i = 0; i < fb_node->data.func_block.output_count && fb_output_ids; i++) {
                    GeomNode *pn = graph_get_node(graph, fb_output_ids[i]);
                    if (pn)
                        pn->parent_block_id = fb_id;
                }
            }

            LOG_DEBUG("lambda_to_graph", "编译 ABS: fb=%d, input=%d, output=%d, internals=%d", fb_id, input_port_id,
                      output_port_id, internal_count);

            *out_node_id = fb_id;
            return true;
        }

        /* ================================================================
     * LV_LAMBDA_APP(left, right): 连接函数与实参
     *
     * 1. 递归编译 left（函数）→ left_node_id
     * 2. 递归编译 right（实参）→ right_node_id
     * 3. 将 left 的输出端口连接到 right 的输入端口
     * 4. 结果为 left 的节点 ID（代表应用的结果）
     * ================================================================ */
        case LV_LAMBDA_APP: {
            LvLambdaTerm *left = term->data.app.left;
            LvLambdaTerm *right = term->data.app.right;
            if (!left || !right) {
                LOG_ERROR("lambda_to_graph", "APP 子项为空");
                return false;
            }

            /* 1. 编译 left */
            int left_node_id = -1;
            if (!lambda_to_graph_internal(left, graph, scope, depth, &left_node_id)) {
                LOG_ERROR("lambda_to_graph", "编译 APP left 失败");
                return false;
            }

            /* 2. 编译 right */
            int right_node_id = -1;
            if (!lambda_to_graph_internal(right, graph, scope, depth, &right_node_id)) {
                LOG_ERROR("lambda_to_graph", "编译 APP right 失败");
                return false;
            }

            /* 3. 连接：left 的输出 → right 的输入
         * 对于 ABS（函数块），输出是 output_port；
         * 对于 VAR（端口），输出是端口本身 */
            int left_output = -1;
            GeomNode *left_node = graph_get_node(graph, left_node_id);
            if (!left_node) {
                LOG_ERROR("lambda_to_graph", "left 节点 %d 不存在", left_node_id);
                return false;
            }

            if (left_node->type == GEOM_FUNCTION_BLOCK) {
                /* 函数块的输出端口是 output_port_ids[0] */
                if (left_node->data.func_block.output_count > 0 && left_node->data.func_block.output_port_ids) {
                    left_output = left_node->data.func_block.output_port_ids[0];
                } else {
                    LOG_ERROR("lambda_to_graph", "函数块 %d 无输出端口", left_node_id);
                    return false;
                }
            } else if (left_node->type == GEOM_PORT) {
                left_output = left_node_id;
            } else {
                LOG_ERROR("lambda_to_graph", "left 节点类型 %d 不支持作为函数", (int) left_node->type);
                return false;
            }

            int right_input = -1;
            GeomNode *right_node = graph_get_node(graph, right_node_id);
            if (!right_node) {
                LOG_ERROR("lambda_to_graph", "right 节点 %d 不存在", right_node_id);
                return false;
            }

            if (right_node->type == GEOM_FUNCTION_BLOCK) {
                /* 实参为函数块时，取其输入端口 */
                if (right_node->data.func_block.input_count > 0 && right_node->data.func_block.input_port_ids) {
                    right_input = right_node->data.func_block.input_port_ids[0];
                } else {
                    LOG_ERROR("lambda_to_graph", "右函数块 %d 无输入端口", right_node_id);
                    return false;
                }
            } else if (right_node->type == GEOM_PORT) {
                right_input = right_node_id;
            } else {
                LOG_ERROR("lambda_to_graph", "right 节点类型 %d 不支持作为实参", (int) right_node->type);
                return false;
            }

            /* 创建连接：left_output → right_input */
            if (left_output >= 0 && right_input >= 0) {
                AddConstraintResult cr = graph_add_connection(graph, left_output, right_input);
                if (cr != ADD_CONSTRAINT_OK && cr != ADD_CONSTRAINT_DUPLICATE) {
                    LOG_WARN("lambda_to_graph", "APP 连接 %d→%d 失败 (result=%d)", left_output, right_input, (int) cr);
                }
            }

            LOG_DEBUG("lambda_to_graph", "编译 APP: left=%d, right=%d, conn=%d→%d", left_node_id, right_node_id,
                      left_output, right_input);

            /* 4. 结果为 left 节点 */
            *out_node_id = left_node_id;
            return true;
        }

        default:
            LOG_ERROR("lambda_to_graph", "未知 λ-项类型 %d", (int) term->type);
            return false;
    }
}

/* ===========================================================================
 * lambda_to_graph：顶层编译入口
 * =========================================================================== */

bool lambda_to_graph(LvLambdaTerm *term, ConstraintGraph *graph, int *out_node_id) {
    if (!term || !graph || !out_node_id)
        return false;

    *out_node_id = -1;

    LambdaScope scope;
    scope_init(&scope);

    bool result = lambda_to_graph_internal(term, graph, &scope, 0, out_node_id);

    scope_destroy(&scope);

    if (!result) {
        LOG_ERROR("lambda_to_graph", "λ-项编译失败");
    } else {
        LOG_DEBUG("lambda_to_graph", "编译成功: root=%d", *out_node_id);
    }

    return result;
}

/* ===========================================================================
 * graph_to_lambda：从约束图还原 λ-项
 * =========================================================================== */

/**
 * @brief 递归重建 λ-项的内部实现
 *
 * @param graph     约束图
 * @param node_id   当前节点 ID
 * @param binder_port_ids 外部绑定端口 ID 栈（用于确定 De Bruijn 索引）
 * @param binder_count    外部绑定数量
 * @param binder_capacity 数组容量
 * @return 重建的 LvLambdaTerm*，失败返回 NULL
 */
static LvLambdaTerm *graph_to_lambda_internal(ConstraintGraph *graph, int node_id, int *binder_port_ids,
                                              int binder_count, int binder_capacity) {
    if (!graph)
        return NULL;

    GeomNode *node = graph_get_node(graph, node_id);
    if (!node) {
        LOG_ERROR("graph_to_lambda", "节点 %d 不存在", node_id);
        return NULL;
    }

    switch (node->type) {
        /* ============================================================
     * GEOM_PORT：变量（VAR）
     *
     * 查找该端口在 binder_port_ids 栈中的位置来确定 De Bruijn 索引。
     * 如果在栈中找不到（自由变量），使用 namespace_depth 作为索引。
     * ============================================================ */
        case GEOM_PORT: {
            if (!node->data.port) {
                LOG_ERROR("graph_to_lambda", "端口节点 %d port 数据为空", node_id);
                return NULL;
            }

            int de_bruijn_index = -1;

            /* 查找该端口在 binder 栈中的位置 */
            for (int i = 0; i < binder_count; i++) {
                if (binder_port_ids[i] == node_id) {
                    /* De Bruijn index: 最内层绑定 (栈顶) 为 0 */
                    de_bruijn_index = binder_count - 1 - i;
                    break;
                }
            }

            if (de_bruijn_index < 0) {
                /* 自由变量：使用 namespace_depth 作为兜底 */
                de_bruijn_index = node->namespace_depth;
            }

            LvLambdaTerm *var = lv_lambda_create_var(de_bruijn_index);
            if (!var) {
                LOG_ERROR("graph_to_lambda", "创建 VAR 项失败");
            }
            return var;
        }

        /* ============================================================
     * GEOM_FUNCTION_BLOCK：抽象（ABS）
     *
     * 1. 获取输入端口（binder）和输出端口
     * 2. 将输入端口加入 binder 栈
     * 3. 沿输出端口的 CONNECTION 找到 body 的根节点
     * 4. 递归重建 body
     * ============================================================ */
        case GEOM_FUNCTION_BLOCK: {
            if (node->data.func_block.input_count < 1 || !node->data.func_block.input_port_ids) {
                LOG_ERROR("graph_to_lambda", "函数块 %d 无输入端口", node_id);
                return NULL;
            }

            int input_port_id = node->data.func_block.input_port_ids[0];
            int output_port_id = -1;
            if (node->data.func_block.output_count > 0 && node->data.func_block.output_port_ids) {
                output_port_id = node->data.func_block.output_port_ids[0];
            }

            if (output_port_id < 0) {
                LOG_ERROR("graph_to_lambda", "函数块 %d 无输出端口", node_id);
                return NULL;
            }

            /* 扩展 binder 栈 */
            int new_cap = binder_capacity;
            if (binder_count + 1 > new_cap) {
                new_cap = new_cap ? new_cap * 2 : 8;
            }

            int *new_binders = NULL;
            if (new_cap > binder_capacity) {
                new_binders = lv_calloc((size_t) new_cap, sizeof(int));
                if (!new_binders)
                    return NULL;
                if (binder_port_ids && binder_count > 0) {
                    memcpy(new_binders, binder_port_ids, (size_t) binder_count * sizeof(int));
                }
            } else {
                new_binders = binder_port_ids;
            }

            new_binders[binder_count] = input_port_id;

            /* 查找 body 根节点：沿输出端口的输入连接追踪 */
            int body_root_id = -1;

            /* 优先查找 CONNECTION(src → output_port) */
            if (!find_connection_target(graph, output_port_id, 1, &body_root_id)) {
                /* 如果没有连接，尝试使用输出端口自身 */
                body_root_id = output_port_id;
            }

            LOG_DEBUG("graph_to_lambda", "函数块 %d: input=%d, output=%d, body_root=%d", node_id, input_port_id,
                      output_port_id, body_root_id);

            /* 递归重建 body */
            LvLambdaTerm *body = graph_to_lambda_internal(graph, body_root_id, new_binders, binder_count + 1, new_cap);

            /* 释放 binder 栈（如果是新分配的） */
            if (new_binders != binder_port_ids) {
                lv_free((void **) &new_binders);
            }

            if (!body) {
                LOG_ERROR("graph_to_lambda", "重建函数块 %d 的 body 失败", node_id);
                return NULL;
            }

            /* 创建 ABS 项。binder 参数传递 0（函数块的输入端口序号） */
            LvLambdaTerm *abs = lv_lambda_create_abs(0, body);
            if (!abs) {
                LOG_ERROR("graph_to_lambda", "创建 ABS 项失败");
                lv_lambda_destroy(body);
                return NULL;
            }

            return abs;
        }

        /* ============================================================
     * GEOM_PORT 但已通过 CONNECTION 关联：应用（APP）
     *
     * 查找从该端口出发的所有 CONNECTION。
     * 但这里的重建逻辑需要从更高层的函数块视角处理。
     * 此处的兜底处理：将 GEOM_PORT 节点当作 VAR。
     * ============================================================ */
        default:
            LOG_WARN("graph_to_lambda", "不支持的节点类型 %d (id=%d)，尝试作为 VAR", (int) node->type, node_id);
            /* 兜底：创建变量项 */
            return lv_lambda_create_var(node->namespace_depth);
    }
}

LvLambdaTerm *graph_to_lambda(ConstraintGraph *graph, int node_id) {
    if (!graph)
        return NULL;

    GeomNode *node = graph_get_node(graph, node_id);
    if (!node) {
        LOG_ERROR("graph_to_lambda", "根节点 %d 不存在", node_id);
        return NULL;
    }

    /* 创建空的 binder 栈 */
    int *binder_port_ids = NULL;
    int binder_count = 0;
    int binder_capacity = 0;

    LvLambdaTerm *result = graph_to_lambda_internal(graph, node_id, binder_port_ids, binder_count, binder_capacity);

    if (!result) {
        LOG_ERROR("graph_to_lambda", "从节点 %d 重建 λ-项失败", node_id);
    } else {
        LOG_DEBUG("graph_to_lambda", "重建成功: root_type=%d", (int) result->type);
    }

    return result;
}
