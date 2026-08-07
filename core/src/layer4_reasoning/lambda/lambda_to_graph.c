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

#include "lv/lambda_to_graph.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/func_block.h"
#include "lv/lambda_term.h"
#include "lv/lv_xmacro.h"

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
        if (!lv_ensure_capacity((void **) &scope->port_ids, scope->depth, &scope->capacity, sizeof(int), 1))
            return false;
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
/**
 * @brief 获取一个已编译节点的"有效输出端口"
 *
 * 对于 PORT 节点（VAR 编译结果）：返回节点自身（PORT_OUTPUT）
 * 对于 FB 节点（ABS 编译结果）：返回 FB 的输出端口
 *
 * @param graph  约束图
 * @param node_id 节点 ID
 * @return 有效输出端口 ID，失败返回 -1
 */
static int get_node_output_port(const ConstraintGraph *graph, int node_id) {
    GeomNode *node = graph_get_node(graph, node_id);
    if (!node)
        return -1;
    if (node->type == GEOM_PORT)
        return node_id;
    if (node->type == GEOM_FUNCTION_BLOCK &&
        node->data.func_block.output_count > 0 &&
        node->data.func_block.output_port_ids) {
        return node->data.func_block.output_port_ids[0];
    }
    return -1;
}

/**
 * @brief 获取一个已编译节点的"有效输入端口"
 *
 * 对于 PORT 节点：返回节点自身（PORT_INPUT 作为 binder）
 * 对于 FB 节点：返回 FB 的第一个输入端口
 *
 * @param graph  约束图
 * @param node_id 节点 ID
 * @return 有效输入端口 ID，失败返回 -1
 */
static int get_node_input_port(const ConstraintGraph *graph, int node_id) {
    GeomNode *node = graph_get_node(graph, node_id);
    if (!node)
        return -1;
    if (node->type == GEOM_PORT)
        return node_id;
    if (node->type == GEOM_FUNCTION_BLOCK &&
        node->data.func_block.input_count > 0 &&
        node->data.func_block.input_port_ids) {
        return node->data.func_block.input_port_ids[0];
    }
    return -1;
}

/**
 * @brief 递归编译 λ-项的内部实现
 *
 * @param term      待编译的 λ-项
 * @param graph     目标约束图
 * @param scope     当前作用域栈
 * @param depth     当前 λ-嵌套深度（用于端口的 namespace_depth）
 * @param out_node_id 输出：根节点 ID
 * @return true 编译成功
 *
 * 端口方向约定（与 graph_add_connection 的 src=PORT_OUTPUT, dst=PORT_INPUT 一致）：
 * - LV_LAMBDA_VAR:  创建 PORT_OUTPUT 节点，代表变量引用（数据生产者）
 * - LV_LAMBDA_ABS:  binder 端口 = PORT_INPUT（接收参数），
 *                   输出端口 = PORT_OUTPUT（向外部提供函数结果），
 *                   body 结果连接到输出端口
 * - LV_LAMBDA_APP:  参数端口的 PORT_OUTPUT → 函数块输入端口的 PORT_INPUT
 *                   （参数字符串作为数据流接入函数块）
 */
/* ── λ-to-graph 编译处理器查找表 ── */
typedef bool (*LambdaToGraphHandler)(LvLambdaTerm *term, ConstraintGraph *graph, LambdaScope *scope, int depth,
                                     int *out_node_id);

/* 前向声明：lambda_to_graph_internal 被 abs 处理器调用 */
static bool lambda_to_graph_internal(LvLambdaTerm *term, ConstraintGraph *graph, LambdaScope *scope, int depth,
                                     int *out_node_id);

static bool lambda_to_graph_var(LvLambdaTerm *term, ConstraintGraph *graph, LambdaScope *scope, int depth,
                                int *out_node_id) {
    int index = term->data.var.index;
    int binder_port_id = scope_lookup(scope, index);
    if (binder_port_id < 0) {
        LOG_ERROR("lambda_to_graph", "自由的 De Bruijn 索引 %d（作用域深度 %d）", index, scope->depth);
        return false;
    }

    AddNodeResult nr = graph_add_port(graph, PORT_OUTPUT, depth, -1);
    if (nr != ADD_NODE_OK) {
        LOG_ERROR("lambda_to_graph", "创建 VAR 引用端口失败");
        return false;
    }
    int ref_port_id = graph_get_last_added_node_id(graph);

    LOG_DEBUG("lambda_to_graph", "编译 VAR(%d): binder=%d, ref_port=%d", index, binder_port_id, ref_port_id);

    *out_node_id = ref_port_id;
    return true;
}

static bool lambda_to_graph_abs(LvLambdaTerm *term, ConstraintGraph *graph, LambdaScope *scope, int depth,
                                int *out_node_id) {
    int binder = term->data.abs.binder;
    (void)binder;
    LvLambdaTerm *body = term->data.abs.body;
    if (!body) {
        LOG_ERROR("lambda_to_graph", "ABS 体为空");
        return false;
    }

    int start_node_count = graph->node_count;

    AddNodeResult nr = graph_add_port(graph, PORT_INPUT, depth, -1);
    if (nr != ADD_NODE_OK) {
        LOG_ERROR("lambda_to_graph", "创建 binder 端口失败");
        return false;
    }
    int input_port_id = graph_get_last_added_node_id(graph);

    GeomNode *input_node = graph_get_node(graph, input_port_id);
    if (input_node && input_node->data.port)
        input_node->data.port->is_formal_param = true;

    nr = graph_add_port(graph, PORT_OUTPUT, depth, -1);
    if (nr != ADD_NODE_OK) {
        LOG_ERROR("lambda_to_graph", "创建输出端口失败");
        return false;
    }
    int output_port_id = graph_get_last_added_node_id(graph);

    if (!scope_push(scope, input_port_id)) {
        LOG_ERROR("lambda_to_graph", "作用域压栈失败");
        return false;
    }

    int body_root_id = -1;
    if (!lambda_to_graph_internal(body, graph, scope, depth + 1, &body_root_id)) {
        scope_pop(scope);
        LOG_ERROR("lambda_to_graph", "编译 ABS body 失败");
        return false;
    }

    if (body_root_id >= 0) {
        int body_output = get_node_output_port(graph, body_root_id);
        if (body_output >= 0) {
            GeomNode *out_node = graph_get_node(graph, output_port_id);
            if (out_node && out_node->data.port)
                out_node->data.port->connected_to = graph_get_node(graph, body_output);
        }
    }

    scope_pop(scope);

    int internal_count = 0;
    int *internal_ids = collect_node_ids_since(graph, start_node_count, &internal_count);
    if (!internal_ids || internal_count <= 0) {
        LOG_ERROR("lambda_to_graph", "收集内部节点失败");
        lv_free((void **)&internal_ids);
        return false;
    }

    int input_ids[] = {input_port_id};
    int output_ids[] = {output_port_id};

    nr = graph_add_function_block(graph, internal_ids, internal_count, input_ids, 1, output_ids, 1);
    lv_free((void **)&internal_ids);

    if (nr != ADD_NODE_OK) {
        LOG_ERROR("lambda_to_graph", "创建函数块失败");
        return false;
    }

    int fb_id = graph_get_last_added_node_id(graph);

    GeomNode *fb_node = graph_get_node(graph, fb_id);
    if (fb_node) {
        int *fb_input_ids = fb_node->data.func_block.input_port_ids;
        int *fb_output_ids = fb_node->data.func_block.output_port_ids;
        for (int i = 0; i < fb_node->data.func_block.input_count && fb_input_ids; i++) {
            GeomNode *pn = graph_get_node(graph, fb_input_ids[i]);
            if (pn) pn->parent_block_id = fb_id;
        }
        for (int i = 0; i < fb_node->data.func_block.output_count && fb_output_ids; i++) {
            GeomNode *pn = graph_get_node(graph, fb_output_ids[i]);
            if (pn) pn->parent_block_id = fb_id;
        }
    }

    LOG_DEBUG("lambda_to_graph", "编译 ABS: fb=%d, input=%d, output=%d, internals=%d", fb_id, input_port_id,
              output_port_id, internal_count);

    *out_node_id = fb_id;
    return true;
}

static bool lambda_to_graph_app(LvLambdaTerm *term, ConstraintGraph *graph, LambdaScope *scope, int depth,
                                int *out_node_id) {
    LvLambdaTerm *left = term->data.app.left;
    LvLambdaTerm *right = term->data.app.right;
    if (!left || !right) {
        LOG_ERROR("lambda_to_graph", "APP 子项为空");
        return false;
    }

    int left_node_id = -1;
    if (!lambda_to_graph_internal(left, graph, scope, depth, &left_node_id)) {
        LOG_ERROR("lambda_to_graph", "编译 APP left 失败");
        return false;
    }

    int right_node_id = -1;
    if (!lambda_to_graph_internal(right, graph, scope, depth, &right_node_id)) {
        LOG_ERROR("lambda_to_graph", "编译 APP right 失败");
        return false;
    }

    GeomNode *left_node = graph_get_node(graph, left_node_id);
    if (!left_node) {
        LOG_ERROR("lambda_to_graph", "left 节点 %d 不存在", left_node_id);
        return false;
    }

    if (left_node->type == GEOM_FUNCTION_BLOCK) {
        int left_input = get_node_input_port(graph, left_node_id);
        if (left_input < 0) {
            LOG_ERROR("lambda_to_graph", "函数块 %d 无输入端口", left_node_id);
            return false;
        }

        int right_output = get_node_output_port(graph, right_node_id);
        if (right_output < 0) {
            LOG_ERROR("lambda_to_graph", "实参 %d 无输出端口", right_node_id);
            return false;
        }

        AddConstraintResult cr = graph_add_connection(graph, right_output, left_input);
        if (cr == ADD_CONSTRAINT_OK)
            LOG_DEBUG("lambda_to_graph", "APP redex: arg_out=%d → func_in=%d", right_output, left_input);
        else if (cr == ADD_CONSTRAINT_DUPLICATE)
            LOG_DEBUG("lambda_to_graph", "APP redex: 重复连接 arg_out=%d → func_in=%d", right_output, left_input);
        else
            LOG_WARN("lambda_to_graph", "APP redex 连接 %d→%d 失败 (result=%d)", right_output, left_input, (int)cr);

        int left_output = get_node_output_port(graph, left_node_id);
        if (left_output < 0) {
            LOG_ERROR("lambda_to_graph", "函数块 %d 无输出端口", left_node_id);
            return false;
        }
        *out_node_id = left_output;

        LOG_DEBUG("lambda_to_graph", "编译 APP: redex, left=FB%d, result=port%d", left_node_id, left_output);
    } else {
        /* non-redex：left 不是函数块（变量引用端口或上一层应用的结果端口），
         * 即柯里化应用的后续参数。创建"应用汇"端口对（app_sink）：
         *   - sink_out（PORT_OUTPUT）：结果侧，parent_block_id 标记为
         *     left 节点（应用左端），作为本层应用的结果；
         *   - sink_in（PORT_INPUT）：实参侧，实参输出连接到 sink_in，
         *     parent_block_id 标记为 sink_out。
         * 反编译阶段从 sink_out 出发，经 parent 找到 left、经配套 sink_in
         * 找到实参，重建 APP(left, arg)，使 COMPILE→DECOMPILE 链对
         * 多参应用（柯里化）完整。
         *
         * 对 β-归约无影响：beta_reduce_match 只匹配"函数块输入端口的
         * 外部实参连接"，app_sink 不属于任何函数块的输入端口。 */
        int right_output = get_node_output_port(graph, right_node_id);
        if (right_output >= 0) {
            AddNodeResult nr = graph_add_port(graph, PORT_OUTPUT, depth, -1);
            if (nr == ADD_NODE_OK) {
                int sink_out = graph_get_last_added_node_id(graph);
                GeomNode *sink_out_node = graph_get_node(graph, sink_out);
                if (sink_out_node && sink_out_node->data.port) {
                    sink_out_node->data.port->is_formal_param = false;
                    sink_out_node->parent_block_id = left_node_id;
                }

                nr = graph_add_port(graph, PORT_INPUT, depth, -1);
                if (nr == ADD_NODE_OK) {
                    int sink_in = graph_get_last_added_node_id(graph);
                    GeomNode *sink_in_node = graph_get_node(graph, sink_in);
                    if (sink_in_node && sink_in_node->data.port) {
                        sink_in_node->data.port->is_formal_param = false;
                        sink_in_node->parent_block_id = sink_out;
                    }
                    AddConstraintResult cr = graph_add_connection(graph, right_output, sink_in);
                    if (cr != ADD_CONSTRAINT_OK && cr != ADD_CONSTRAINT_DUPLICATE) {
                        LOG_WARN("lambda_to_graph", "APP non-redex 连接 %d→%d 失败 (result=%d)",
                                 right_output, sink_in, (int)cr);
                    }
                    LOG_DEBUG("lambda_to_graph", "编译 APP: non-redex, arg_out=%d → sink_in=%d, sink_out=%d, left=%d",
                              right_output, sink_in, sink_out, left_node_id);
                    *out_node_id = sink_out;
                } else {
                    LOG_WARN("lambda_to_graph", "APP non-redex: 创建 sink_in 端口失败");
                    *out_node_id = sink_out;
                }
            } else {
                LOG_WARN("lambda_to_graph", "APP non-redex: 创建 sink_out 端口失败");
                *out_node_id = left_node_id;
            }
        } else {
            LOG_WARN("lambda_to_graph", "APP non-redex: 实参 %d 无输出端口", right_node_id);
            *out_node_id = left_node_id;
        }
    }
    return true;
}

static const LambdaToGraphHandler lambda_to_graph_table[LV_LAMBDA_APP + 1] = {
    [LV_LAMBDA_VAR] = lambda_to_graph_var,
    [LV_LAMBDA_ABS] = lambda_to_graph_abs,
    [LV_LAMBDA_APP] = lambda_to_graph_app,
};

static bool lambda_to_graph_internal(LvLambdaTerm *term, ConstraintGraph *graph, LambdaScope *scope, int depth,
                                     int *out_node_id) {
    if (!term || !graph || !scope || !out_node_id)
        return false;
    *out_node_id = -1;

    /* 越界类型保留诊断日志；正常类型统一走 LV_DISPATCH（含 NULL 槽回退） */
    if ((unsigned) term->type >= (unsigned) LV_LAMBDA_APP + 1) {
        LOG_ERROR("lambda_to_graph", "未知 λ-项类型 %d", (int)term->type);
        return false;
    }
    return LV_DISPATCH(lambda_to_graph_table, term->type, false, term, graph, scope, depth, out_node_id);
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

/**
 * @brief 在约束图中查找 parent_block_id == parent_id 的 PORT_INPUT 端口
 *
 * 用于反编译阶段定位 app_sink 端口对的实参侧（sink_in）：
 * 编译 non-redex APP 时，sink_in 的 parent_block_id 被标记为配套的
 * sink_out 结果端口。sink_in 是非形式参数（is_formal_param == false），
 * 与 binder 端口（形式参数）可区分。
 *
 * @param graph    约束图
 * @param parent_id 期望的 parent_block_id
 * @return sink_in 端口节点 ID，未找到返回 -1
 */
static int find_app_sink_input(const ConstraintGraph *graph, int parent_id) {
    if (!graph || parent_id < 0)
        return -1;
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || !node->is_active)
            continue;
        if (node->type != GEOM_PORT)
            continue;
        if (node->parent_block_id != parent_id)
            continue;
        if (node->data.port && node->data.port->type == PORT_INPUT &&
            !node->data.port->is_formal_param) {
            return node->id;
        }
    }
    return -1;
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
     * GEOM_PORT：变量（VAR）或应用（APP）
     *
     * 如果该端口是函数块的输出端口且不在本函数块的 body 内部
     * （binder 栈中不含该函数块的输入端口）：
     *   - 检查函数块的输入端口是否有外部 CONNECTION 进入
     *   - 有：重建 APP(func=函数块作为 ABS, arg=连接来源)
     *   - 无：重定向到函数块重建 ABS
     *
     * 否则：查找端口在 binder 栈中的位置确定 De Bruijn 索引。
     * ============================================================ */
        case GEOM_PORT: {
            if (!node->data.port) {
                LOG_ERROR("graph_to_lambda", "端口节点 %d port 数据为空", node_id);
                return NULL;
            }

            /* ── 检测 app_sink 端口对 ──
             * 编译阶段为柯里化应用的后续参数创建（见 lambda_to_graph_app 的
             * non-redex 分支）：
             *   - sink_out（PORT_OUTPUT）：parent_block_id 指向应用左端节点
             *     （PORT），经配套 sink_in 的 CONNECTION 来源找到实参，
             *     重建 APP(left, arg)；
             *   - sink_in（PORT_INPUT）：parent_block_id 指向配套 sink_out。
             * 不会误判 binder 端口（is_formal_param=true）与普通 VAR 引用
             * 端口（parent_block_id 为 -1 或函数块，而非 PORT）。 */
            if (!node->data.port->is_formal_param && node->parent_block_id >= 0) {
                int left_id = node->parent_block_id;
                GeomNode *left_node = graph_get_node(graph, left_id);
                if (left_node && left_node->type == GEOM_PORT) {
                    int arg_src = -1;
                    if (node->data.port->type == PORT_OUTPUT) {
                        /* sink_out：经配套 sink_in 找实参来源 */
                        int sink_in = find_app_sink_input(graph, node_id);
                        if (sink_in >= 0)
                            find_connection_target(graph, sink_in, 1, &arg_src);
                    } else {
                        /* sink_in：直接找进入本端口的实参来源 */
                        find_connection_target(graph, node_id, 1, &arg_src);
                    }
                    if (arg_src >= 0) {
                        LvLambdaTerm *left_term = graph_to_lambda_internal(graph, left_id,
                                                                           binder_port_ids,
                                                                           binder_count, binder_capacity);
                        if (!left_term) return NULL;

                        LvLambdaTerm *arg_term = graph_to_lambda_internal(graph, arg_src,
                                                                          binder_port_ids,
                                                                          binder_count, binder_capacity);
                        if (!arg_term) {
                            lv_lambda_destroy(left_term);
                            return NULL;
                        }
                        LOG_DEBUG("graph_to_lambda", "APP(non-redex): func=node%d, arg=node%d", left_id, arg_src);
                        return lv_lambda_create_app(left_term, arg_term);
                    }
                }
            }

            /* ── 检测是否是函数块输出端口 ── */
            int parent_fb_id = node->parent_block_id;
            if (parent_fb_id >= 0) {
                GeomNode *fb_node = graph_get_node(graph, parent_fb_id);
                if (fb_node && fb_node->type == GEOM_FUNCTION_BLOCK &&
                    fb_node->data.func_block.output_count > 0 &&
                    fb_node->data.func_block.output_port_ids) {

                    /* 确认本节点是函数块的输出端口 */
                    bool is_output = false;
                    for (int i = 0; i < fb_node->data.func_block.output_count; i++) {
                        if (fb_node->data.func_block.output_port_ids[i] == node_id) {
                            is_output = true;
                            break;
                        }
                    }

                    if (is_output && fb_node->data.func_block.input_count > 0 &&
                        fb_node->data.func_block.input_port_ids) {
                        int fb_input = fb_node->data.func_block.input_port_ids[0];

                        /* 检查是否在本函数块 body 内部（binder 栈中有此输入端口） */
                        bool inside_body = false;
                        for (int i = 0; i < binder_count; i++) {
                            if (binder_port_ids[i] == fb_input) {
                                inside_body = true;
                                break;
                            }
                        }

                        if (!inside_body) {
                            /* 在外部：检查是否有 APP 模式（外部 CONNECTION 进入输入端口） */
                            int arg_src = -1;
                            bool has_app = find_connection_target(graph, fb_input, 1, &arg_src);

                            if (has_app && arg_src >= 0 && arg_src != node_id) {
                                /* APP: func=函数块作为 ABS, arg=连接来源 */
                                LvLambdaTerm *func = graph_to_lambda_internal(graph, parent_fb_id,
                                                                               binder_port_ids,
                                                                               binder_count, binder_capacity);
                                if (!func) return NULL;

                                LvLambdaTerm *arg = graph_to_lambda_internal(graph, arg_src,
                                                                              binder_port_ids,
                                                                              binder_count, binder_capacity);
                                if (!arg) {
                                    lv_lambda_destroy(func);
                                    return NULL;
                                }

                                LOG_DEBUG("graph_to_lambda", "APP: func=FB%d, arg=node%d", parent_fb_id, arg_src);
                                return lv_lambda_create_app(func, arg);
                            }

                            /* 无 APP：重定向到函数块重建 ABS */
                            return graph_to_lambda_internal(graph, parent_fb_id, binder_port_ids,
                                                            binder_count, binder_capacity);
                        }
                    }
                }
            }

            /* ── 常规 VAR 查找逻辑 ── */
            int de_bruijn_index = -1;
            for (int i = 0; i < binder_count; i++) {
                if (binder_port_ids[i] == node_id) {
                    de_bruijn_index = binder_count - 1 - i;
                    break;
                }
            }

            if (de_bruijn_index < 0) {
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
     * 3. 沿 connected_to 指针或 CONNECTION 找到 body 的根节点
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

            /* 查找 body 根节点：
             * 优先使用 connected_to 指针（ABS 编译时设置 body_output→output_port 的关联）
             * 然后查找 CONNECTION(src → output_port)
             * 最后回退到输出端口自身 */
            int body_root_id = -1;
            GeomNode *op_node = graph_get_node(graph, output_port_id);
            if (op_node && op_node->data.port && op_node->data.port->connected_to) {
                body_root_id = op_node->data.port->connected_to->id;
            } else if (!find_connection_target(graph, output_port_id, 1, &body_root_id)) {
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

        default:
            LOG_ERROR("graph_to_lambda", "不支持的节点类型 %d (id=%d)，无法重建", (int) node->type, node_id);
            return NULL;
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
