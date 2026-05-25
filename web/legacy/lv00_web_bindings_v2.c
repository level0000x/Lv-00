/**
 * @file lv00_web_bindings_v2.c
 * @brief WebAssembly comprehensive bindings for Lv-00 (v2)
 *
 * Comprehensive WASM bindings exposing ALL C engine APIs needed by the frontend panels.
 * Uses EMSCRIPTEN_KEEPALIVE for all exported functions.
 * Complex return types use JSON strings for structured data exchange between C and JS.
 *
 * 全面 WASM 绑定，暴露前端面板所需的全部 C 引擎 API。
 * 所有导出函数使用 EMSCRIPTEN_KEEPALIVE。
 * 复杂返回类型使用 JSON 字符串在 C 和 JS 之间传递结构化数据。
 */

#include <emscripten.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00/lv00.h"

/* ================================================================
 *  Internal helpers / 内部辅助函数
 * ================================================================ */

/** JSON 输出缓冲区最大大小（64KB，足够大多数场景） */
#define JSON_BUF_SIZE 65536

/**
 * @brief 将整数数组序列化为 JSON 数组字符串
 *
 * 格式: [id1, id2, id3, ...]
 * 调用者负责 free() 返回的字符串。
 */
static char *int_array_to_json(const int *arr, int count) {
    if (!arr || count <= 0) {
        char *empty = (char *) malloc(3);
        if (empty)
            strcpy(empty, "[]");
        return empty;
    }

    /* 预估所需空间：每个数字最多 11 位 + 逗号/空格 */
    int needed = 2 + count * 14;
    char *buf = (char *) malloc(needed);
    if (!buf)
        return NULL;

    int pos = 0;
    buf[pos++] = '[';
    for (int i = 0; i < count; i++) {
        if (i > 0)
            buf[pos++] = ',';
        pos += snprintf(buf + pos, needed - pos, "%d", arr[i]);
    }
    buf[pos++] = ']';
    buf[pos] = '\0';

    return buf;
}

/**
 * @brief 将合并候选数组序列化为 JSON 字符串
 *
 * 格式: [{"a":id1,"b":id2}, ...]
 */
static char *merge_candidates_to_json(NodeMergeCandidate *candidates, int count) {
    if (!candidates || count <= 0) {
        char *empty = (char *) malloc(3);
        if (empty)
            strcpy(empty, "[]");
        return empty;
    }

    int needed = 2 + count * 64;
    char *buf = (char *) malloc(needed);
    if (!buf)
        return NULL;

    int pos = 0;
    buf[pos++] = '[';
    for (int i = 0; i < count; i++) {
        if (i > 0) {
            buf[pos++] = ',';
        }
        pos +=
            snprintf(buf + pos, needed - pos, "{\"a\":%d,\"b\":%d}", candidates[i].node_a_id, candidates[i].node_b_id);
    }
    buf[pos++] = ']';
    buf[pos] = '\0';

    return buf;
}

/**
 * @brief 将冲突组数组序列化为 JSON 字符串
 *
 * 格式: [[id1,id2,...], [id3,id4,...], ...]
 */
static char *conflict_groups_to_json(int **groups, int *sizes, int group_count) {
    if (!groups || group_count <= 0) {
        char *empty = (char *) malloc(3);
        if (empty)
            strcpy(empty, "[]");
        return empty;
    }

    int needed = 2 + group_count * 256;
    char *buf = (char *) malloc(needed);
    if (!buf)
        return NULL;

    int pos = 0;
    buf[pos++] = '[';
    for (int i = 0; i < group_count; i++) {
        if (i > 0)
            buf[pos++] = ',';
        buf[pos++] = '[';
        for (int j = 0; j < sizes[i]; j++) {
            if (j > 0)
                buf[pos++] = ',';
            pos += sprintf(buf + pos, "%d", groups[i][j]);
        }
        buf[pos++] = ']';
    }
    buf[pos++] = ']';
    buf[pos] = '\0';

    return buf;
}

/* ================================================================
 *  Graph Operations / 图操作
 * ================================================================ */

/**
 * @brief 创建新的约束图
 * @return 图指针（作为句柄传给其他函数）
 */
EMSCRIPTEN_KEEPALIVE
void *web_graph_create(void) {
    return graph_create();
}

/**
 * @brief 销毁约束图并释放所有资源
 * @param graph 图指针
 */
EMSCRIPTEN_KEEPALIVE
void web_graph_destroy(void *graph) {
    if (graph) {
        graph_destroy((ConstraintGraph *) graph);
    }
}

/**
 * @brief 向图中添加点（有理数坐标）
 * @param graph 图指针
 * @param x_num x 坐标分子
 * @param x_den x 坐标分母
 * @param y_num y 坐标分子
 * @param y_den y 坐标分母
 * @return 新点 ID，失败返回 -1
 */
EMSCRIPTEN_KEEPALIVE
int web_graph_add_point(void *graph, int64_t x_num, uint64_t x_den, int64_t y_num, uint64_t y_den) {
    if (!graph)
        return -1;

    SymbolicCoord *cx = symbolic_coord_create_rational(x_num, x_den);
    SymbolicCoord *cy = symbolic_coord_create_rational(y_num, y_den);

    if (!cx || !cy) {
        if (cx)
            symbolic_coord_destroy(cx);
        if (cy)
            symbolic_coord_destroy(cy);
        return -1;
    }

    SymbolicCoord *coords[2] = {cx, cy};
    /* 所有权语义：graph_add_point 内部深拷贝坐标，调用方需释放原始坐标 */
    AddNodeResult result = graph_add_point((ConstraintGraph *) graph, coords, 2);

    symbolic_coord_destroy(cx);
    symbolic_coord_destroy(cy);

    if (result == ADD_NODE_OK) {
        return ((ConstraintGraph *) graph)->next_node_id - 1;
    }
    return -1;
}

/**
 * @brief 向图中添加线段
 * @param graph 图指针
 * @param p1 端点1 ID
 * @param p2 端点2 ID
 * @return 新线段 ID，失败返回 -1
 */
EMSCRIPTEN_KEEPALIVE
int web_graph_add_line_segment(void *graph, int p1, int p2) {
    if (!graph)
        return -1;
    AddNodeResult result = graph_add_line_segment((ConstraintGraph *) graph, p1, p2);
    if (result == ADD_NODE_OK) {
        return ((ConstraintGraph *) graph)->next_node_id - 1;
    }
    return -1;
}

/**
 * @brief 向图中添加区域（由边界线段围成）
 * @param graph 图指针
 * @param boundary_json 边界线段 ID 的 JSON 数组字符串，如 "[1,2,3]"
 * @return 新区域 ID，失败返回 -1
 */
EMSCRIPTEN_KEEPALIVE
int web_graph_add_region(void *graph, const char *boundary_json) {
    if (!graph || !boundary_json)
        return -1;

    /* 简易 JSON 数组解析：提取逗号分隔的整数 */
    int seg_ids[256];
    int seg_count = 0;

    const char *p = boundary_json;
    while (*p && seg_count < 256) {
        /* 跳过非数字字符 */
        while (*p && (*p < '0' || *p > '9') && *p != '-')
            p++;
        if (!*p)
            break;
        seg_ids[seg_count++] = atoi(p);
        /* 跳过数字 */
        while (*p && (*p >= '0' && *p <= '9'))
            p++;
    }

    if (seg_count == 0)
        return -1;

    AddNodeResult result = graph_add_region((ConstraintGraph *) graph, seg_ids, seg_count);
    if (result == ADD_NODE_OK) {
        return ((ConstraintGraph *) graph)->next_node_id - 1;
    }
    return -1;
}

/**
 * @brief 从图中移除节点
 * @param graph 图指针
 * @param node_id 节点 ID
 * @return 0 成功，-1 失败
 */
EMSCRIPTEN_KEEPALIVE
int web_graph_remove_node(void *graph, int node_id) {
    if (!graph)
        return -1;
    RemoveNodeResult result = graph_remove_node((ConstraintGraph *) graph, node_id);
    return (result == REMOVE_NODE_OK) ? 0 : -1;
}

/**
 * @brief 获取图中节点数量
 * @param graph 图指针
 * @return 节点数量
 */
EMSCRIPTEN_KEEPALIVE
int web_graph_get_node_count(void *graph) {
    if (!graph)
        return 0;
    return graph_get_node_count((ConstraintGraph *) graph);
}

/**
 * @brief 获取图中约束数量
 * @param graph 图指针
 * @return 约束数量
 */
EMSCRIPTEN_KEEPALIVE
int web_graph_get_constraint_count(void *graph) {
    if (!graph)
        return 0;
    return graph_get_constraint_count((ConstraintGraph *) graph);
}

/**
 * @brief 将图序列化为 JSON 字符串
 * @param graph 图指针
 * @return JSON 字符串（调用者需通过 web_free_string 释放），失败返回 NULL
 */
EMSCRIPTEN_KEEPALIVE
char *web_graph_serialize_to_json(void *graph) {
    if (!graph)
        return NULL;
    return graph_serialize_to_json((ConstraintGraph *) graph);
}

/**
 * @brief 从 JSON 字符串反序列化图
 * @param json JSON 字符串
 * @return 图指针（调用者需通过 web_graph_destroy 释放），失败返回 NULL
 */
EMSCRIPTEN_KEEPALIVE
void *web_graph_deserialize_from_json(const char *json) {
    if (!json)
        return NULL;
    return graph_deserialize_from_json(json);
}

/* ================================================================
 *  Constraint Operations / 约束操作
 * ================================================================ */

/**
 * @brief 添加关联约束（点在线/区域上）
 * @param graph 图指针
 * @param point_id 点 ID
 * @param segment_id 线段或区域 ID
 * @return 约束 ID，失败返回 -1
 */
EMSCRIPTEN_KEEPALIVE
int web_graph_add_incidence(void *graph, int point_id, int segment_id) {
    if (!graph)
        return -1;
    AddConstraintResult result = graph_add_incidence((ConstraintGraph *) graph, point_id, segment_id);
    if (result == ADD_CONSTRAINT_OK) {
        return ((ConstraintGraph *) graph)->next_constraint_id - 1;
    }
    return -1;
}

/**
 * @brief 添加介于约束（B 介于 A 和 C 之间）
 * @param graph 图指针
 * @param p1 端点1 ID
 * @param p2 中间点 ID
 * @param p3 端点2 ID
 * @return 约束 ID，失败返回 -1
 */
EMSCRIPTEN_KEEPALIVE
int web_graph_add_betweenness(void *graph, int p1, int p2, int p3) {
    if (!graph)
        return -1;
    AddConstraintResult result = graph_add_betweenness((ConstraintGraph *) graph, p1, p2, p3);
    if (result == ADD_CONSTRAINT_OK) {
        return ((ConstraintGraph *) graph)->next_constraint_id - 1;
    }
    return -1;
}

/**
 * @brief 添加相交约束（两条线段相交于某点）
 * @param graph 图指针
 * @param seg1 线段1 ID
 * @param seg2 线段2 ID
 * @param result_point_id 交点 ID
 * @return 约束 ID，失败返回 -1
 */
EMSCRIPTEN_KEEPALIVE
int web_graph_add_intersection(void *graph, int seg1, int seg2, int result_point_id) {
    if (!graph)
        return -1;
    AddConstraintResult result = graph_add_intersection((ConstraintGraph *) graph, seg1, seg2, result_point_id);
    if (result == ADD_CONSTRAINT_OK) {
        return ((ConstraintGraph *) graph)->next_constraint_id - 1;
    }
    return -1;
}

/**
 * @brief 添加包含约束（内部元素在容器内）
 * @param graph 图指针
 * @param inner_id 内部元素 ID
 * @param outer_id 外部容器 ID
 * @return 约束 ID，失败返回 -1
 */
EMSCRIPTEN_KEEPALIVE
int web_graph_add_containment(void *graph, int inner_id, int outer_id) {
    if (!graph)
        return -1;
    AddConstraintResult result = graph_add_containment((ConstraintGraph *) graph, inner_id, outer_id);
    if (result == ADD_CONSTRAINT_OK) {
        return ((ConstraintGraph *) graph)->next_constraint_id - 1;
    }
    return -1;
}

/**
 * @brief 添加连接约束（源端口到目标端口）
 * @param graph 图指针
 * @param src 源端口 ID
 * @param dst 目标端口 ID
 * @return 约束 ID，失败返回 -1
 */
EMSCRIPTEN_KEEPALIVE
int web_graph_add_connection(void *graph, int src, int dst) {
    if (!graph)
        return -1;
    AddConstraintResult result = graph_add_connection((ConstraintGraph *) graph, src, dst);
    if (result == ADD_CONSTRAINT_OK) {
        return ((ConstraintGraph *) graph)->next_constraint_id - 1;
    }
    return -1;
}

/* ================================================================
 *  Analysis Operations / 分析操作
 * ================================================================ */

/**
 * @brief 规范化约束图（合并共线线段、重叠区域等）
 * @param graph 图指针
 * @return 合并的节点数量，失败返回 -1
 */
EMSCRIPTEN_KEEPALIVE
int web_graph_normalize(void *graph) {
    if (!graph)
        return -1;

    NormalizationResult *result = graph_normalize((ConstraintGraph *) graph, false);
    if (result) {
        int merged = result->merged_count;
        normalization_result_destroy(result);
        return merged;
    }
    return -1;
}

/**
 * @brief 查找合并候选节点
 * @param graph 图指针
 * @return JSON 数组字符串，格式 [{"a":id1,"b":id2}, ...]
 *         调用者需通过 web_free_string 释放
 */
EMSCRIPTEN_KEEPALIVE
char *web_find_merge_candidates(void *graph) {
    if (!graph) {
        char *empty = (char *) malloc(3);
        if (empty)
            strcpy(empty, "[]");
        return empty;
    }

    int count = 0;
    NodeMergeCandidate *candidates = find_merge_candidates((ConstraintGraph *) graph, &count);

    char *json = merge_candidates_to_json(candidates, count);

    if (candidates) {
        merge_candidates_destroy(candidates, count);
    }

    return json ? json : strdup("[]");
}

/**
 * @brief 检测冗余约束
 * @param graph 图指针
 * @return JSON 数组字符串，包含冗余约束 ID，如 [3,7,12]
 *         调用者需通过 web_free_string 释放
 */
EMSCRIPTEN_KEEPALIVE
char *web_graph_detect_redundant(void *graph) {
    if (!graph) {
        char *empty = (char *) malloc(3);
        if (empty)
            strcpy(empty, "[]");
        return empty;
    }

    int count = 0;
    int *redundant_ids = graph_detect_redundant_constraints((ConstraintGraph *) graph, &count);

    char *json = int_array_to_json(redundant_ids, count);

    if (redundant_ids) {
        free(redundant_ids);
    }

    return json ? json : strdup("[]");
}

/**
 * @brief 检测冲突约束组
 * @param graph 图指针
 * @return JSON 数组字符串，格式 [[id1,id2,...], [id3,id4,...], ...]
 *         每个子数组是一个冲突组
 *         调用者需通过 web_free_string 释放
 */
EMSCRIPTEN_KEEPALIVE
char *web_graph_detect_conflicts(void *graph) {
    if (!graph) {
        char *empty = (char *) malloc(3);
        if (empty)
            strcpy(empty, "[]");
        return empty;
    }

    int conflict_count = 0;
    int *conflict_sizes = NULL;
    int **conflicts = graph_detect_conflicts((ConstraintGraph *) graph, &conflict_count, &conflict_sizes);

    char *json = conflict_groups_to_json(conflicts, conflict_sizes, conflict_count);

    /* 释放冲突组内存 */
    if (conflicts) {
        for (int i = 0; i < conflict_count; i++) {
            if (conflicts[i])
                free(conflicts[i]);
        }
        free(conflicts);
    }
    if (conflict_sizes) {
        free(conflict_sizes);
    }

    return json ? json : strdup("[]");
}

/**
 * @brief 计算约束图的自由度
 * @param graph 图指针
 * @return 自由度数量（0 表示完全确定），出错返回 -1
 */
EMSCRIPTEN_KEEPALIVE
int web_count_degrees_of_freedom(void *graph) {
    if (!graph)
        return -1;
    int *free_var_ids = NULL;
    int dof = count_degrees_of_freedom((ConstraintGraph *) graph, &free_var_ids);
    if (free_var_ids) {
        free(free_var_ids);
    }
    return dof;
}

/**
 * @brief 对约束图进行拓扑排序
 * @param graph 图指针
 * @return JSON 数组字符串，包含排序后的约束 ID，如 [1,3,5,2]
 *         调用者需通过 web_free_string 释放
 */
EMSCRIPTEN_KEEPALIVE
char *web_graph_topological_sort(void *graph) {
    if (!graph) {
        char *empty = (char *) malloc(3);
        if (empty)
            strcpy(empty, "[]");
        return empty;
    }

    /* 执行拓扑排序（原地修改图的约束顺序） */
    graph_topological_sort_stable((ConstraintGraph *) graph);

    /* 收集排序后的约束 ID */
    ConstraintGraph *g = (ConstraintGraph *) graph;
    int *sorted_ids = (int *) malloc(g->constraint_count * sizeof(int));
    if (!sorted_ids)
        return strdup("[]");

    for (int i = 0; i < g->constraint_count; i++) {
        sorted_ids[i] = g->constraints[i]->id;
    }

    char *json = int_array_to_json(sorted_ids, g->constraint_count);
    free(sorted_ids);

    return json ? json : strdup("[]");
}

/**
 * @brief 计算约束图的结构哈希
 * @param graph 图指针
 * @return 哈希字符串（十六进制格式），调用者需通过 web_free_string 释放
 */
EMSCRIPTEN_KEEPALIVE
char *web_graph_hash(void *graph) {
    if (!graph)
        return strdup("0");

    GraphHash *hash = compute_complete_graph_hash((ConstraintGraph *) graph);
    if (!hash)
        return strdup("0");

    /* 将 uint64_t 哈希值转换为十六进制字符串 */
    char *buf = (char *) malloc(20);
    if (buf) {
        sprintf(buf, "%016llx", (unsigned long long) hash->hash);
    }

    graph_hash_destroy(hash);
    return buf ? buf : strdup("0");
}

/* ================================================================
 *  Solver / Engine Operations / 求解器/引擎操作
 * ================================================================ */

/**
 * @brief 创建引擎实例
 * @return 引擎指针
 */
EMSCRIPTEN_KEEPALIVE
void *web_engine_create(void) {
    return engine_create();
}

/**
 * @brief 销毁引擎实例
 * @param engine 引擎指针
 */
EMSCRIPTEN_KEEPALIVE
void web_engine_destroy(void *engine) {
    if (engine) {
        engine_destroy((LV00Engine *) engine);
    }
}

/**
 * @brief 执行完整求解流水线
 * @param engine 引擎指针
 * @return 状态码：0=OK, 1=CONFLICT, 2=TIMEOUT, 3=ERROR
 */
EMSCRIPTEN_KEEPALIVE
int web_engine_solve(void *engine) {
    if (!engine)
        return 3; /* ENGINE_SOLVE_ERROR */
    return (int) engine_solve((LV00Engine *) engine);
}

/**
 * @brief 执行重写-求解协作工作流
 * @param engine 引擎指针
 * @param max_rewrite 最大重写步数
 * @param max_solve 最大求解步数
 * @return 总执行步数，出错返回负值
 */
EMSCRIPTEN_KEEPALIVE
int web_engine_rewrite_and_solve(void *engine, int max_rewrite, int max_solve) {
    if (!engine)
        return -1;
    return engine_rewrite_and_solve((LV00Engine *) engine, max_rewrite, max_solve);
}

/**
 * @brief 获取引擎的主图指针（用于将图操作与引擎关联）
 * @param engine 引擎指针
 * @return 图指针
 */
EMSCRIPTEN_KEEPALIVE
void *web_engine_get_graph(void *engine) {
    if (!engine)
        return NULL;
    return ((LV00Engine *) engine)->main_graph;
}

/**
 * @brief 获取引擎最近一次错误描述
 * @param engine 引擎指针
 * @return 错误描述字符串（内部存储，勿 free）
 */
EMSCRIPTEN_KEEPALIVE
const char *web_engine_get_last_error(void *engine) {
    if (!engine)
        return "Engine is NULL";
    return engine_get_last_error((LV00Engine *) engine);
}

/* ================================================================
 *  Function Block Operations / 函数块操作
 * ================================================================ */

/**
 * @brief 打包函数块
 * @param graph 图指针
 * @param internal_nodes_json 内部节点 ID 的 JSON 数组，如 "[1,2,3]"
 * @param input_ports_json 输入端口 ID 的 JSON 数组，如 "[4,5]"
 * @param output_ports_json 输出端口 ID 的 JSON 数组，如 "[6]"
 * @return 函数块 ID，失败返回 -1
 */
EMSCRIPTEN_KEEPALIVE
int web_func_block_pack(void *graph, const char *internal_nodes_json, const char *input_ports_json,
                        const char *output_ports_json) {
    if (!graph || !internal_nodes_json)
        return -1;

    /* 解析 JSON 数组为 int 数组 */
    int internal_ids[512];
    int input_ids[256];
    int output_ids[256];
    int internal_count = 0, input_count = 0, output_count = 0;

    /* 解析内部节点 */
    const char *p = internal_nodes_json;
    while (*p && internal_count < 512) {
        while (*p && (*p < '0' || *p > '9') && *p != '-')
            p++;
        if (!*p)
            break;
        internal_ids[internal_count++] = atoi(p);
        while (*p && (*p >= '0' && *p <= '9'))
            p++;
    }

    /* 解析输入端口 */
    if (input_ports_json) {
        p = input_ports_json;
        while (*p && input_count < 256) {
            while (*p && (*p < '0' || *p > '9') && *p != '-')
                p++;
            if (!*p)
                break;
            input_ids[input_count++] = atoi(p);
            while (*p && (*p >= '0' && *p <= '9'))
                p++;
        }
    }

    /* 解析输出端口 */
    if (output_ports_json) {
        p = output_ports_json;
        while (*p && output_count < 256) {
            while (*p && (*p < '0' || *p > '9') && *p != '-')
                p++;
            if (!*p)
                break;
            output_ids[output_count++] = atoi(p);
            while (*p && (*p >= '0' && *p <= '9'))
                p++;
        }
    }

    if (internal_count == 0)
        return -1;

    /* 创建函数块并设置属性 */
    FuncBlock *fb = func_block_create(0);
    if (!fb)
        return -1;

    func_block_set_internal_nodes(fb, internal_ids, internal_count);
    if (input_count > 0) {
        func_block_set_input_ports(fb, input_ids, input_count);
    }
    if (output_count > 0) {
        func_block_set_output_ports(fb, output_ids, output_count);
    }

    /* 执行打包 */
    FuncBlock *out_fb = NULL;
    PackResult pack_result = func_block_pack((ConstraintGraph *) graph, internal_ids, internal_count, input_ids,
                                             input_count, output_ids, output_count, NULL, 0, &out_fb);

    if (pack_result == PACK_OK && out_fb) {
        int block_id = out_fb->id;
        func_block_destroy(out_fb);
        func_block_destroy(fb);
        return block_id;
    }

    func_block_destroy(fb);
    return -1;
}

/**
 * @brief 例化函数块
 * @param graph 图指针
 * @param block_id 函数块 ID
 * @param arg_mappings_json 实参映射 JSON 数组，如 "[1,2]"（按输入端口顺序）
 * @return JSON 字符串，包含新创建的节点 ID，如 "[10,11,12,13]"
 *         调用者需通过 web_free_string 释放
 */
EMSCRIPTEN_KEEPALIVE
char *web_func_block_instantiate(void *graph, int block_id, const char *arg_mappings_json) {
    /* 注意：当前 C API 的 func_block_instantiate 需要 FuncBlock* 指针，
     * 此处提供简化版本，返回空结果。
     * 完整实现需要维护一个函数块注册表。 */
    (void) graph;
    (void) block_id;
    (void) arg_mappings_json;
    return strdup("[]");
}

/**
 * @brief 检查函数块的确定性
 * @param graph 图指针
 * @param block_id 函数块 ID
 * @return 确定性状态字符串："verified", "non_deterministic",
 *         "partially_verified", "unverified"
 *         调用者需通过 web_free_string 释放
 */
EMSCRIPTEN_KEEPALIVE
char *web_func_block_check_determinism(void *graph, int block_id) {
    (void) graph;
    (void) block_id;
    /* 简化实现：返回未验证状态 */
    return strdup("unverified");
}

/* ================================================================
 *  Formula Operations / 公式操作
 * ================================================================ */

/**
 * @brief 解析公式字符串
 * @param input 公式字符串
 * @param syntax 语法类型：0=auto, 1=latex, 2=python, 3=dsl
 * @return JSON 字符串，格式：
 *   成功: {"success":true,"ast":<pointer_as_int>,"error":null}
 *   失败: {"success":false,"ast":0,"error":"error message"}
 *   调用者需通过 web_free_string 释放
 */
EMSCRIPTEN_KEEPALIVE
char *web_formula_parse(const char *input, int syntax) {
    if (!input) {
        return strdup("{\"success\":false,\"ast\":0,\"error\":\"input is null\"}");
    }

    /* 语法类型映射 */
    const char *syntax_str = "auto";
    switch (syntax) {
        case 1:
            syntax_str = "latex";
            break;
        case 2:
            syntax_str = "python";
            break;
        case 3:
            syntax_str = "dsl";
            break;
        default:
            syntax_str = "auto";
            break;
    }

    FormulaNode *ast = formula_parse(input, syntax_str);

    if (ast) {
        /* 将指针转换为整数，供 JS 端作为句柄使用 */
        char buf[128];
        sprintf(buf, "{\"success\":true,\"ast\":%lld,\"error\":null}", (long long) (intptr_t) ast);
        return strdup(buf);
    }

    const char *err = formula_parser_get_last_error();
    char buf[512];
    if (err) {
        /* 转义 JSON 字符串中的特殊字符 */
        int bi = 0;
        buf[bi++] = '{';
        sprintf(buf + bi, "\"success\":false,\"ast\":0,\"error\":\"");
        bi = (int) strlen(buf);
        for (int i = 0; err[i] && bi < 480; i++) {
            if (err[i] == '"' || err[i] == '\\') {
                buf[bi++] = '\\';
            }
            buf[bi++] = err[i];
        }
        buf[bi++] = '"';
        buf[bi++] = '}';
        buf[bi] = '\0';
    } else {
        strcpy(buf, "{\"success\":false,\"ast\":0,\"error\":\"unknown parse error\"}");
    }

    return strdup(buf);
}

/**
 * @brief 渲染公式 AST 为指定格式字符串
 * @param ast_ptr AST 指针（整数形式传入）
 * @param format 输出格式：0=latex, 1=python, 2=dsl
 * @return 渲染后的字符串，调用者需通过 web_free_string 释放
 */
EMSCRIPTEN_KEEPALIVE
char *web_formula_render(intptr_t ast_ptr, int format) {
    if (!ast_ptr)
        return strdup("");

    FormulaNode *ast = (FormulaNode *) ast_ptr;
    OutputFormat fmt = (OutputFormat) format;

    char *rendered = formula_render(ast, fmt);
    return rendered ? rendered : strdup("");
}

/**
 * @brief 将公式 AST 转换为约束图操作
 * @param ast_ptr AST 指针（整数形式传入）
 * @param graph 图指针
 * @return JSON 字符串，格式：
 *   {"success":true,"node_ids":[1,2,3],"constraint_ids":[1,2],"error":null}
 *   调用者需通过 web_free_string 释放
 */
EMSCRIPTEN_KEEPALIVE
char *web_formula_to_graph(intptr_t ast_ptr, void *graph) {
    if (!ast_ptr || !graph) {
        return strdup("{\"success\":false,\"node_ids\":[],\"constraint_ids\":[],\"error\":\"null arguments\"}");
    }

    FormulaNode *ast = (FormulaNode *) ast_ptr;
    FormulaToGraphResult *result = formula_to_graph(ast, (ConstraintGraph *) graph);

    if (!result) {
        return strdup("{\"success\":false,\"node_ids\":[],\"constraint_ids\":[],\"error\":\"conversion failed\"}");
    }

    char *node_json = int_array_to_json(result->created_node_ids, result->created_node_count);
    char *con_json = int_array_to_json(result->created_constraint_ids, result->created_constraint_count);

    char *response = NULL;
    if (result->success) {
        int needed = 64 + (node_json ? strlen(node_json) : 2) + (con_json ? strlen(con_json) : 2);
        response = (char *) malloc(needed);
        if (response) {
            sprintf(response, "{\"success\":true,\"node_ids\":%s,\"constraint_ids\":%s,\"error\":null}",
                    node_json ? node_json : "[]", con_json ? con_json : "[]");
        }
    } else {
        int needed = 128 + strlen(result->error_message);
        response = (char *) malloc(needed);
        if (response) {
            sprintf(response, "{\"success\":false,\"node_ids\":[],\"constraint_ids\":[],\"error\":\"%s\"}",
                    result->error_message);
        }
    }

    if (node_json)
        free(node_json);
    if (con_json)
        free(con_json);
    formula_to_graph_result_destroy(result);

    return response ? response
                    : strdup("{\"success\":false,\"node_ids\":[],\"constraint_ids\":[],\"error\":\"memory error\"}");
}

/**
 * @brief 将约束图转换为公式
 * @param graph 图指针
 * @return JSON 字符串，格式：
 *   {"success":true,"latex":"...","python":"...","dsl":"...","error":null}
 *   调用者需通过 web_free_string 释放
 */
EMSCRIPTEN_KEEPALIVE
char *web_graph_to_formula(void *graph) {
    if (!graph) {
        return strdup("{\"success\":false,\"latex\":null,\"python\":null,\"dsl\":null,\"error\":\"null graph\"}");
    }

    GraphToFormulaResult *result = graph_to_formula((ConstraintGraph *) graph);

    if (!result) {
        return strdup(
            "{\"success\":false,\"latex\":null,\"python\":null,\"dsl\":null,\"error\":\"conversion failed\"}");
    }

    const char *latex = result->latex_output ? result->latex_output : "";
    const char *python = result->python_output ? result->python_output : "";
    const char *dsl = result->dsl_output ? result->dsl_output : "";

    /* 转义字符串中的特殊字符 */
    char latex_escaped[4096] = "";
    char python_escaped[4096] = "";
    char dsl_escaped[4096] = "";

    for (int i = 0; latex[i] && i < 4000; i++) {
        if (latex[i] == '"' || latex[i] == '\\') {
            strcat(latex_escaped, "\\");
        }
        char tmp[2] = {latex[i], '\0'};
        strcat(latex_escaped, tmp);
    }
    for (int i = 0; python[i] && i < 4000; i++) {
        if (python[i] == '"' || python[i] == '\\') {
            strcat(python_escaped, "\\");
        }
        char tmp[2] = {python[i], '\0'};
        strcat(python_escaped, tmp);
    }
    for (int i = 0; dsl[i] && i < 4000; i++) {
        if (dsl[i] == '"' || dsl[i] == '\\') {
            strcat(dsl_escaped, "\\");
        }
        char tmp[2] = {dsl[i], '\0'};
        strcat(dsl_escaped, tmp);
    }

    int needed = 256 + strlen(latex_escaped) + strlen(python_escaped) + strlen(dsl_escaped);
    char *response = (char *) malloc(needed);
    if (response) {
        if (result->success) {
            sprintf(response, "{\"success\":true,\"latex\":\"%s\",\"python\":\"%s\",\"dsl\":\"%s\",\"error\":null}",
                    latex_escaped, python_escaped, dsl_escaped);
        } else {
            sprintf(response, "{\"success\":false,\"latex\":null,\"python\":null,\"dsl\":null,\"error\":\"%s\"}",
                    result->error_message);
        }
    }

    graph_to_formula_result_destroy(result);

    return response
               ? response
               : strdup("{\"success\":false,\"latex\":null,\"python\":null,\"dsl\":null,\"error\":\"memory error\"}");
}

/**
 * @brief 销毁公式 AST 节点
 * @param ast_ptr AST 指针（整数形式传入）
 */
EMSCRIPTEN_KEEPALIVE
void web_formula_node_destroy(intptr_t ast_ptr) {
    if (ast_ptr) {
        formula_node_destroy((FormulaNode *) ast_ptr);
    }
}

/**
 * @brief 检测公式语法类型
 * @param input 公式字符串
 * @return 语法类型代码：0=unknown, 1=latex, 2=python, 3=dsl
 */
EMSCRIPTEN_KEEPALIVE
int web_formula_detect_syntax(const char *input) {
    if (!input)
        return 0;
    const char *syntax = formula_detect_syntax(input);
    if (strcmp(syntax, "latex") == 0)
        return 1;
    if (strcmp(syntax, "python") == 0)
        return 2;
    if (strcmp(syntax, "dsl") == 0)
        return 3;
    return 0;
}

/* ================================================================
 *  Point/Segment/Constraint Query / 点/线段/约束查询
 * ================================================================ */

/**
 * @brief 获取所有点信息
 * @param graph 图指针
 * @return JSON 数组字符串，格式：
 *   [{"id":1,"x":0.5,"y":1.0,"type":0}, ...]
 *   type: 0=POINT, 3=PORT, 4=FUNCTION_BLOCK
 *   调用者需通过 web_free_string 释放
 */
EMSCRIPTEN_KEEPALIVE
char *web_get_all_points(void *graph) {
    if (!graph)
        return strdup("[]");

    ConstraintGraph *g = (ConstraintGraph *) graph;
    char *buf = (char *) malloc(JSON_BUF_SIZE);
    if (!buf)
        return strdup("[]");

    int pos = 0;
    buf[pos++] = '[';

    for (int i = 0; i < g->node_count; i++) {
        GeomNode *node = g->nodes[i];
        if (!node)
            continue;
        if (node->type != GEOM_POINT && node->type != GEOM_PORT && node->type != GEOM_FUNCTION_BLOCK)
            continue;

        if (pos > 1)
            buf[pos++] = ',';

        double x = 0.0, y = 0.0;
        if (node->symbolic_coords && node->coord_count >= 2) {
            x = symbolic_coord_to_double(node->symbolic_coords[0]);
            y = symbolic_coord_to_double(node->symbolic_coords[1]);
        }

        pos += sprintf(buf + pos, "{\"id\":%d,\"x\":%.15g,\"y\":%.15g,\"type\":%d}", node->id, x, y, (int) node->type);
    }

    buf[pos++] = ']';
    buf[pos] = '\0';

    return buf;
}

/**
 * @brief 获取所有线段信息
 * @param graph 图指针
 * @return JSON 数组字符串，格式：
 *   [{"id":1,"p1":0,"p2":1}, ...]
 *   调用者需通过 web_free_string 释放
 */
EMSCRIPTEN_KEEPALIVE
char *web_get_all_segments(void *graph) {
    if (!graph)
        return strdup("[]");

    ConstraintGraph *g = (ConstraintGraph *) graph;
    char *buf = (char *) malloc(JSON_BUF_SIZE);
    if (!buf)
        return strdup("[]");

    int pos = 0;
    buf[pos++] = '[';

    for (int i = 0; i < g->node_count; i++) {
        GeomNode *node = g->nodes[i];
        if (!node || node->type != GEOM_LINE_SEGMENT)
            continue;

        if (pos > 1)
            buf[pos++] = ',';

        /* 线段端点通过关联约束获取（简化实现：使用 coord_count 和 symbolic_coords） */
        int p1 = -1, p2 = -1;
        if (node->coord_count >= 2 && node->symbolic_coords) {
            /* 线段节点存储端点坐标，但端点 ID 需要通过约束查找 */
            /* 简化：直接输出节点 ID，前端通过关联约束关联端点 */
        }

        pos += sprintf(buf + pos, "{\"id\":%d,\"p1\":%d,\"p2\":%d}", node->id, p1, p2);
    }

    buf[pos++] = ']';
    buf[pos] = '\0';

    return buf;
}

/**
 * @brief 获取所有约束信息
 * @param graph 图指针
 * @return JSON 数组字符串，格式：
 *   [{"id":1,"type":"incidence","args":[0,1]}, ...]
 *   type 可能的值: "incidence", "betweenness", "intersection",
 *                 "containment", "connection"
 *   调用者需通过 web_free_string 释放
 */
EMSCRIPTEN_KEEPALIVE
char *web_get_all_constraints(void *graph) {
    if (!graph)
        return strdup("[]");

    ConstraintGraph *g = (ConstraintGraph *) graph;
    char *buf = (char *) malloc(JSON_BUF_SIZE);
    if (!buf)
        return strdup("[]");

    int pos = 0;
    buf[pos++] = '[';

    for (int i = 0; i < g->constraint_count; i++) {
        Constraint *con = g->constraints[i];
        if (!con)
            continue;

        if (pos > 1)
            buf[pos++] = ',';

        /* 约束类型字符串 */
        const char *type_str = "unknown";
        switch (con->type) {
            case INCIDENCE:
                type_str = "incidence";
                break;
            case BETWEENNESS:
                type_str = "betweenness";
                break;
            case INTERSECTION:
                type_str = "intersection";
                break;
            case CONTAINMENT:
                type_str = "containment";
                break;
            case CONNECTION:
                type_str = "connection";
                break;
        }

        /* 参与者 ID 数组 */
        buf[pos++] = '{';
        pos += sprintf(buf + pos, "\"id\":%d,\"type\":\"%s\",\"args\":[", con->id, type_str);
        for (int j = 0; j < con->participant_count; j++) {
            if (j > 0)
                buf[pos++] = ',';
            pos += sprintf(buf + pos, "%d", con->participants[j]);
        }
        buf[pos++] = ']';
        buf[pos++] = '}';
    }

    buf[pos++] = ']';
    buf[pos] = '\0';

    return buf;
}

/* ================================================================
 *  Coordinate Operations / 坐标操作
 * ================================================================ */

/**
 * @brief 创建有理数坐标
 * @param num 分子
 * @param den 分母
 * @return 坐标指针
 */
EMSCRIPTEN_KEEPALIVE
void *web_coord_create_rational(int64_t num, uint64_t den) {
    if (den == 0)
        return NULL;
    return (void *) symbolic_coord_create_rational(num, den);
}

/**
 * @brief 销毁坐标
 * @param coord 坐标指针
 */
EMSCRIPTEN_KEEPALIVE
void web_coord_destroy(void *coord) {
    if (coord) {
        symbolic_coord_destroy((SymbolicCoord *) coord);
    }
}

/**
 * @brief 序列化坐标为字符串
 * @param coord 坐标指针
 * @return 字符串，调用者需通过 web_free_string 释放
 */
EMSCRIPTEN_KEEPALIVE
char *web_coord_serialize(void *coord) {
    if (!coord)
        return strdup("null");
    return symbolic_coord_serialize((SymbolicCoord *) coord);
}

/* ================================================================
 *  Utility Functions / 工具函数
 * ================================================================ */

/**
 * @brief 释放由 C 分配的字符串内存
 * @param str 字符串指针
 */
EMSCRIPTEN_KEEPALIVE
void web_free_string(char *str) {
    if (str) {
        free(str);
    }
}

/**
 * @brief 获取引擎版本号
 * @return 版本字符串
 */
EMSCRIPTEN_KEEPALIVE
const char *web_get_version(void) {
    return LV00_VERSION_STRING;
}
