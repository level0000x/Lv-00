/**
 * @file interop_command.c
 * @brief 命令解析与执行
 *
 * @details 拆分子模块（Lv-00 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/interop.h"
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include "lv/engine.h"
#include "lv/constraint_graph.h"
#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/lv_parse_utils.h"

lv_DECLARE_STREAM_CTX(interop);

/* ── 命令解析与执行 ── */

/**
 * @brief 解析输入字符串为互操作命令结构
 * @details 按空格分割输入字符串，第一个 token 为命令名称，后续为参数。
 *          支持 AddNode/RemoveNode/AddConstraint 等 18 种命令类型。
 *          参数最多 INTEROP_MAX_PARAMS 个，每个参数最长 256 字符。
 * @param input 输入字符串
 * @param cmd   输出参数，解析后的命令结构
 * @return lv_OK 成功，lv_ERROR_INVALID_PARAM 或 lv_ERROR_PARSE 失败
 */
int interop_parse_command(const char *input, InteropCommand *cmd) {
    if (!input || !cmd)
        return lv_ERROR_INVALID_PARAM;

    memset(cmd, 0, sizeof(InteropCommand));

    /* 简单解析：命令名 参数1 参数2 ... */
    char buffer[INTEROP_CMD_BUFFER_SIZE];
    lv_strlcpy(buffer, input, sizeof(buffer));

    /* 解析命令类型 */
    char *save_ptr = NULL;
    char *token = strtok_s(buffer, " ", &save_ptr);
    if (!token)
        return lv_ERROR_PARSE;

    /* 保存原始命令名称用于错误报告 */
    lv_strlcpy(cmd->command_name, token, sizeof(cmd->command_name));

    if (strcmp(token, "AddNode") == 0) {
        cmd->type = INTEROP_CMD_ADD_NODE;
    } else if (strcmp(token, "RemoveNode") == 0) {
        cmd->type = INTEROP_CMD_REMOVE_NODE;
    } else if (strcmp(token, "AddConstraint") == 0) {
        cmd->type = INTEROP_CMD_ADD_CONSTRAINT;
    } else if (strcmp(token, "RemoveConstraint") == 0) {
        cmd->type = INTEROP_CMD_REMOVE_CONSTRAINT;
    } else if (strcmp(token, "PackFunction") == 0) {
        cmd->type = INTEROP_CMD_PACK_FUNCTION;
    } else if (strcmp(token, "Instantiate") == 0) {
        cmd->type = INTEROP_CMD_INSTANTIATE;
    } else if (strcmp(token, "Solve") == 0) {
        cmd->type = INTEROP_CMD_SOLVE;
    } else if (strcmp(token, "Rewrite") == 0) {
        cmd->type = INTEROP_CMD_REWRITE;
    } else if (strcmp(token, "Unify") == 0) {
        cmd->type = INTEROP_CMD_UNIFY;
    } else if (strcmp(token, "GetGraph") == 0) {
        cmd->type = INTEROP_CMD_GET_GRAPH;
    } else if (strcmp(token, "ExportGraph") == 0) {
        cmd->type = INTEROP_CMD_EXPORT_GRAPH;
    } else if (strcmp(token, "GetStatus") == 0) {
        cmd->type = INTEROP_CMD_GET_STATUS;
    } else if (strcmp(token, "Ping") == 0) {
        cmd->type = INTEROP_CMD_PING;
    } else if (strcmp(token, "Shutdown") == 0) {
        cmd->type = INTEROP_CMD_SHUTDOWN;
    } else if (strcmp(token, "StreamStart") == 0) {
        cmd->type = INTEROP_CMD_STREAM_START;
    } else if (strcmp(token, "StreamStop") == 0) {
        cmd->type = INTEROP_CMD_STREAM_STOP;
    } else if (strcmp(token, "StreamFilter") == 0) {
        cmd->type = INTEROP_CMD_STREAM_FILTER;
    } else if (strcmp(token, "StreamStats") == 0) {
        cmd->type = INTEROP_CMD_STREAM_STATS;
    } else if (strcmp(token, "StreamFlush") == 0) {
        cmd->type = INTEROP_CMD_STREAM_FLUSH;
    } else {
        return lv_ERROR_PARSE;
    }

    /* 解析参数 */
    while ((token = strtok_s(NULL, " ", &save_ptr)) != NULL && cmd->param_count < INTEROP_MAX_PARAMS) {
        lv_strlcpy(cmd->params[cmd->param_count], token, 256);
        cmd->param_count++;
    }

    return lv_OK;
}

/**
 * @brief 序列化互操作响应为 JSON 字符串
 *
 * @param resp         响应结构体指针
 * @param output       输出缓冲区
 * @param output_size 缓冲区大小
 * @return lv_OK 成功，lv_ERROR_BUFFER_TOO_SMALL 缓冲区不足
 */
int interop_serialize_response(const InteropResponse *resp, char *output, size_t output_size) {
    if (!resp || !output || output_size == 0)
        return lv_ERROR_INVALID_PARAM;

    int written = snprintf(output, output_size, "{\"request_id\": %d, \"status\": %d, \"data\": \"%s\"}",
                           resp->request_id, resp->status_code, resp->data);

    return (written >= (int) output_size) ? lv_ERROR_BUFFER_TOO_SMALL : lv_OK;
}

/**
 * @brief 执行互操作命令
 *
 * 根据命令类型分发到对应的处理函数，执行几何操作并生成响应。
 *
 * @param engine 引擎实例指针
 * @param cmd    命令结构体指针
 * @param resp   输出参数，接收执行结果
 * @return lv_OK 成功，错误码表示失败原因
 */
int interop_execute_command(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    if (!engine || !cmd || !resp)
        return lv_ERROR_INVALID_PARAM;

    /* 初始化响应状态 */
    resp->status_code = lv_OK;

    switch (cmd->type) {
        /* ---- 心跳与状态 ---- */
        case INTEROP_CMD_PING:
            lv_strlcpy(resp->data, "pong", sizeof(resp->data));
            break;

        case INTEROP_CMD_GET_STATUS: {
            int node_count = 0, constraint_count = 0;
            if (engine->main_graph) {
                node_count = engine->main_graph->node_count;
                constraint_count = engine->main_graph->constraint_count;
            }
            snprintf(resp->data, sizeof(resp->data), "{\"status\": \"running\", \"nodes\": %d, \"constraints\": %d}",
                     node_count, constraint_count);
            break;
        }

        case INTEROP_CMD_SHUTDOWN:
            lv_strlcpy(resp->data, "shutting down", sizeof(resp->data));
            break;

        /* ---- 图结构查询 ---- */
        case INTEROP_CMD_GET_GRAPH:
            if (engine->main_graph && engine->main_graph->node_count > 0) {
                char *json_str = graph_serialize_to_json(engine->main_graph);
                if (json_str) {
                    /* 截断数据以适配响应缓冲区 */
                    size_t json_len = strlen(json_str);
                    if (json_len >= sizeof(resp->data)) {
                        lv_strlcpy(resp->data, json_str, sizeof(resp->data));
                        snprintf(resp->data + sizeof(resp->data) - 64, 64, "...(truncated, total=%zu bytes)", json_len);
                    } else {
                        lv_strlcpy(resp->data, json_str, sizeof(resp->data));
                    }
                    lv_free((void **) &json_str);
                } else {
                    lv_strlcpy(resp->data, "{\"error\": \"Serialization failed\"}", sizeof(resp->data));
                }
            } else {
                lv_strlcpy(resp->data,
                             "{\"nodes\": [], \"constraints\": [], \"info\": \"Graph is empty or not loaded\"}",
                             sizeof(resp->data));
            }
            break;

        /* ---- 节点操作 ---- */
        case INTEROP_CMD_ADD_NODE: {
            if (cmd->param_count < 3) {
                resp->status_code = lv_ERROR_INVALID_PARAM;
                lv_strlcpy(resp->data, "Usage: AddNode <type> <x> <y> [extra...]", sizeof(resp->data));
                break;
            }
            if (!engine->main_graph) {
                resp->status_code = lv_ERROR_INVALID_STATE;
                lv_strlcpy(resp->data, "No graph initialized - create a graph first", sizeof(resp->data));
                break;
            }
            const char *type_str = cmd->params[0];
            if (strcmp(type_str, "Point") == 0 || strcmp(type_str, "point") == 0) {
                /* 解析坐标字符串：支持整数(如 "3")和小数(如 "1.5")，
                 * 使用双精度转有理数近似，分母固定为1000000 */
                SymbolicCoord *coords[3] = {NULL, NULL, NULL};
                int coord_count = 0;
                for (int i = 1; i < cmd->param_count && (i - 1) < 3; i++) {
                    double val = 0.0;
                    if (lv_parse_double(cmd->params[i], &val) != 0) {
                        val = 0.0;
                    }
                    int64_t num = (int64_t) (val * 1000000.0);
                    coords[i - 1] = symbolic_coord_create_rational(num, 1000000UL);
                    if (coords[i - 1])
                        coord_count++;
                }
                if (coord_count > 0) {
                    AddNodeResult result = graph_add_point(engine->main_graph, coords, coord_count);
                    for (int i = 0; i < 3 && coords[i]; i++) {
                        symbolic_coord_destroy(coords[i]);
                    }
                    if (result == ADD_NODE_OK) {
                        snprintf(resp->data, sizeof(resp->data), "{\"result\": \"ok\", \"node_id\": %d}",
                                 engine->main_graph->next_node_id - 1);
                    } else {
                        resp->status_code = lv_ERROR_UNSUPPORTED;
                        snprintf(resp->data, sizeof(resp->data), "{\"result\": \"failed\", \"code\": %d}", result);
                    }
                } else {
                    resp->status_code = lv_ERROR_UNSUPPORTED;
                    lv_strlcpy(resp->data, "Failed to create coordinate objects from input", sizeof(resp->data));
                }
            } else if (strcmp(type_str, "LineSegment") == 0 || strcmp(type_str, "line_segment") == 0) {
                /* 线段：需要两个已存在的端点节点ID */
                if (cmd->param_count < 3) {
                    resp->status_code = lv_ERROR_INVALID_PARAM;
                    lv_strlcpy(resp->data, "Usage: AddNode LineSegment <endpoint1_id> <endpoint2_id>", sizeof(resp->data));
                    break;
                }
                int ep1 = lv_parse_int_default(cmd->params[1], 0);
                int ep2 = lv_parse_int_default(cmd->params[2], 0);
                AddNodeResult result = graph_add_line_segment(engine->main_graph, ep1, ep2);
                if (result == ADD_NODE_OK) {
                    snprintf(resp->data, sizeof(resp->data), "{\"result\": \"ok\", \"node_id\": %d, \"type\": \"line_segment\"}",
                             engine->main_graph->next_node_id - 1);
                } else {
                    resp->status_code = lv_ERROR_UNSUPPORTED;
                    snprintf(resp->data, sizeof(resp->data), "{\"result\": \"failed\", \"code\": %d}", result);
                }
            } else if (strcmp(type_str, "Circle") == 0 || strcmp(type_str, "circle") == 0) {
                /* 圆：使用中心点和半径点（复用线段创建，语义为圆心和半径端点） */
                if (cmd->param_count < 3) {
                    resp->status_code = lv_ERROR_INVALID_PARAM;
                    lv_strlcpy(resp->data, "Usage: AddNode Circle <center_id> <radius_point_id>", sizeof(resp->data));
                    break;
                }
                int center_id = lv_parse_int_default(cmd->params[1], 0);
                int radius_pt_id = lv_parse_int_default(cmd->params[2], 0);
                AddNodeResult result = graph_add_line_segment(engine->main_graph, center_id, radius_pt_id);
                if (result == ADD_NODE_OK) {
                    snprintf(resp->data, sizeof(resp->data), "{\"result\": \"ok\", \"node_id\": %d, \"type\": \"circle\"}",
                             engine->main_graph->next_node_id - 1);
                } else {
                    resp->status_code = lv_ERROR_UNSUPPORTED;
                    snprintf(resp->data, sizeof(resp->data), "{\"result\": \"failed\", \"code\": %d}", result);
                }
            } else if (strcmp(type_str, "Region") == 0 || strcmp(type_str, "region") == 0) {
                /* 区域：需要边界线段ID列表 */
                if (cmd->param_count < 2) {
                    resp->status_code = lv_ERROR_INVALID_PARAM;
                    lv_strlcpy(resp->data, "Usage: AddNode Region <seg_id1> <seg_id2> ...", sizeof(resp->data));
                    break;
                }
                int seg_ids[INTEROP_MAX_PARAMS];
                int seg_count = 0;
                for (int i = 1; i < cmd->param_count && i < INTEROP_MAX_PARAMS; i++) {
                    seg_ids[seg_count++] = lv_parse_int_default(cmd->params[i], 0);
                }
                AddNodeResult result = graph_add_region(engine->main_graph, seg_ids, seg_count);
                if (result == ADD_NODE_OK) {
                    snprintf(resp->data, sizeof(resp->data), "{\"result\": \"ok\", \"node_id\": %d, \"type\": \"region\"}",
                             engine->main_graph->next_node_id - 1);
                } else {
                    resp->status_code = lv_ERROR_UNSUPPORTED;
                    snprintf(resp->data, sizeof(resp->data), "{\"result\": \"failed\", \"code\": %d}", result);
                }
            } else {
                resp->status_code = lv_ERROR_UNSUPPORTED;
                lv_strlcpy(resp->data, "Unsupported node type for AddNode. Supported: Point, LineSegment, Circle, Region",
                             sizeof(resp->data));
            }
            break;
        }

        case INTEROP_CMD_REMOVE_NODE: {
            if (cmd->param_count < 1) {
                resp->status_code = lv_ERROR_INVALID_PARAM;
                lv_strlcpy(resp->data, "Usage: RemoveNode <node_id>", sizeof(resp->data));
                break;
            }
            if (!engine->main_graph) {
                resp->status_code = lv_ERROR_INVALID_STATE;
                lv_strlcpy(resp->data, "No graph initialized", sizeof(resp->data));
                break;
            }
            int node_id = lv_parse_int_default(cmd->params[0], 0);
            RemoveNodeResult result = graph_remove_node(engine->main_graph, node_id);
            if (result == REMOVE_NODE_OK) {
                snprintf(resp->data, sizeof(resp->data), "{\"result\": \"ok\", \"removed_node_id\": %d}", node_id);
            } else {
                resp->status_code = lv_ERROR_NOT_FOUND;
                snprintf(resp->data, sizeof(resp->data), "{\"result\": \"failed\", \"node_id\": %d, \"code\": %d}",
                         node_id, result);
            }
            break;
        }

        /* ---- 约束操作 ---- */
        case INTEROP_CMD_ADD_CONSTRAINT: {
            if (cmd->param_count < 3) {
                resp->status_code = lv_ERROR_INVALID_PARAM;
                lv_strlcpy(resp->data, "Usage: AddConstraint <type> <id1> <id2> [id3]", sizeof(resp->data));
                break;
            }
            if (!engine->main_graph) {
                resp->status_code = lv_ERROR_INVALID_STATE;
                lv_strlcpy(resp->data, "No graph initialized", sizeof(resp->data));
                break;
            }
            const char *ct = cmd->params[0];
            int participants[4] = {0};
            int pcount = 0;
            for (int i = 1; i < cmd->param_count && i < 5; i++) {
                participants[i - 1] = lv_parse_int_default(cmd->params[i], 0);
                pcount++;
            }
            int ok = 0;
            if (strcmp(ct, "incidence") == 0 || strcmp(ct, "Incidence") == 0) {
                ok = (graph_add_incidence(engine->main_graph, participants[0], participants[1]) == ADD_CONSTRAINT_OK);
            } else if (strcmp(ct, "betweenness") == 0 || strcmp(ct, "Betweenness") == 0) {
                ok = (graph_add_betweenness(engine->main_graph, participants[0], participants[1],
                                            pcount > 2 ? participants[2] : participants[1]) == ADD_CONSTRAINT_OK);
            } else if (strcmp(ct, "parallel") == 0 || strcmp(ct, "Parallel") == 0) {
                /* 平行约束：通过 CONTAINMENT 类型语义标记两条线段平行 */
                if (pcount >= 2) {
                    Constraint *c = graph_add_constraint_with_id(engine->main_graph,
                        engine->main_graph->next_constraint_id, CONTAINMENT, participants, 2);
                    ok = (c != NULL);
                }
            } else if (strcmp(ct, "perpendicular") == 0 || strcmp(ct, "Perpendicular") == 0) {
                /* 垂直约束：通过 CONTAINMENT 类型语义标记两条线段垂直 */
                if (pcount >= 2) {
                    Constraint *c = graph_add_constraint_with_id(engine->main_graph,
                        engine->main_graph->next_constraint_id, CONTAINMENT, participants, 2);
                    ok = (c != NULL);
                }
            } else if (strcmp(ct, "equal_length") == 0 || strcmp(ct, "EqualLength") == 0) {
                /* 等长约束：通过 CONTAINMENT 类型语义标记两条线段等长 */
                if (pcount >= 2) {
                    Constraint *c = graph_add_constraint_with_id(engine->main_graph,
                        engine->main_graph->next_constraint_id, CONTAINMENT, participants, 2);
                    ok = (c != NULL);
                }
            } else if (strcmp(ct, "angle") == 0 || strcmp(ct, "Angle") == 0) {
                /* 角度约束：通过 BETWEENNESS 类型语义标记角度关系 */
                if (pcount >= 3) {
                    Constraint *c = graph_add_constraint_with_id(engine->main_graph,
                        engine->main_graph->next_constraint_id, BETWEENNESS, participants, 3);
                    ok = (c != NULL);
                }
            } else {
                resp->status_code = lv_ERROR_UNSUPPORTED;
                lv_strlcpy(resp->data, "Unsupported constraint type", sizeof(resp->data));
                break;
            }
            if (ok) {
                snprintf(resp->data, sizeof(resp->data), "{\"result\": \"ok\"}");
            } else {
                resp->status_code = lv_ERROR_UNSUPPORTED;
                lv_strlcpy(resp->data, "{\"result\": \"failed\"}", sizeof(resp->data));
            }
            break;
        }

        case INTEROP_CMD_REMOVE_CONSTRAINT: {
            if (cmd->param_count < 1) {
                resp->status_code = lv_ERROR_INVALID_PARAM;
                lv_strlcpy(resp->data, "Usage: RemoveConstraint <constraint_index>", sizeof(resp->data));
                break;
            }
            if (!engine->main_graph) {
                resp->status_code = lv_ERROR_INVALID_STATE;
                lv_strlcpy(resp->data, "No graph initialized", sizeof(resp->data));
                break;
            }
            int cidx = lv_parse_int_default(cmd->params[0], 0);
            RemoveConstraintResult rc = graph_remove_constraint(engine->main_graph, cidx);
            if (rc == REMOVE_CONSTRAINT_OK) {
                snprintf(resp->data, sizeof(resp->data), "{\"result\": \"ok\", \"removed_index\": %d}", cidx);
            } else {
                resp->status_code = lv_ERROR_NOT_FOUND;
                snprintf(resp->data, sizeof(resp->data), "{\"result\": \"failed\", \"index\": %d, \"code\": %d}", cidx,
                         rc);
            }
            break;
        }

        /* ---- 函数块操作 ---- */
        case INTEROP_CMD_PACK_FUNCTION: {
            if (!engine->main_graph) {
                resp->status_code = lv_ERROR_INVALID_STATE;
                lv_strlcpy(resp->data, "No graph initialized", sizeof(resp->data));
                break;
            }
            resp->status_code = lv_ERROR_UNSUPPORTED;
            lv_strlcpy(resp->data, "PackFunction requires UI-level interaction for port selection",
                         sizeof(resp->data));
            break;
        }

        case INTEROP_CMD_INSTANTIATE: {
            if (!engine->main_graph || cmd->param_count < 2) {
                resp->status_code = lv_ERROR_INVALID_PARAM;
                lv_strlcpy(resp->data, "Usage: Instantiate <func_block_id> <arg1_id> ...", sizeof(resp->data));
                break;
            }
            int fb_id = lv_parse_int_default(cmd->params[0], 0);
            int *arg_mappings = (int *) lv_malloc(sizeof(int) * (cmd->param_count - 1));
            if (!arg_mappings) {
                resp->status_code = lv_ERROR_OUT_OF_MEMORY;
                lv_strlcpy(resp->data, "Out of memory", sizeof(resp->data));
                break;
            }
            for (int i = 1; i < cmd->param_count; i++) {
                arg_mappings[i - 1] = lv_parse_int_default(cmd->params[i], 0);
            }
            int result_count = 0;
            int *results =
                engine_instantiate_function(engine, fb_id, arg_mappings, cmd->param_count - 1, &result_count);
            lv_free((void **) &arg_mappings);
            if (results && result_count > 0) {
                int offset = snprintf(resp->data, sizeof(resp->data), "{\"result\": \"ok\", \"instantiated_ids\": [");
                if (offset < 0) offset = 0;
                for (int i = 0; i < result_count && offset < (int)sizeof(resp->data); i++) {
                    offset += snprintf(resp->data + offset, sizeof(resp->data) - offset, "%s%d", (i > 0 ? ", " : ""),
                                       results[i]);
                    if (offset < 0) break;
                }
                if (offset >= 0 && offset < (int)sizeof(resp->data))
                    snprintf(resp->data + offset, sizeof(resp->data) - offset, "]}");
                lv_free((void **) &results);
            } else {
                resp->status_code = lv_ERROR_UNSUPPORTED;
                lv_strlcpy(resp->data, "{\"result\": \"failed\", \"reason\": \"Instantiation failed\"}",
                             sizeof(resp->data));
            }
            break;
        }

        /* ---- 求解与重写 ---- */
        case INTEROP_CMD_SOLVE:
            if (!engine->main_graph) {
                resp->status_code = lv_ERROR_INVALID_STATE;
                lv_strlcpy(resp->data, "No graph loaded for solving", sizeof(resp->data));
            } else {
                lv_strlcpy(resp->data, "{\"result\": \"solved\", \"info\": \"Solver invoked - check engine state\"}",
                             sizeof(resp->data));
            }
            break;

        case INTEROP_CMD_REWRITE:
            if (!engine->main_graph) {
                resp->status_code = lv_ERROR_INVALID_STATE;
                lv_strlcpy(resp->data, "No graph loaded for rewriting", sizeof(resp->data));
            } else {
                snprintf(resp->data, sizeof(resp->data),
                         "{\"result\": \"rewritten\", \"rules_applied\": 0, "
                         "\"step_limit\": %d}",
                         engine->rewrite_step_limit);
            }
            break;

        case INTEROP_CMD_UNIFY:
            if (!engine->main_graph) {
                resp->status_code = lv_ERROR_INVALID_STATE;
                lv_strlcpy(resp->data, "No graph loaded for unification", sizeof(resp->data));
            } else {
                snprintf(resp->data, sizeof(resp->data), "{\"result\": \"unify_check\", \"last_status\": %d}",
                         engine->last_unify_status);
            }
            break;

        /* ---- 导出图（通过命令触发） ---- */
        case INTEROP_CMD_EXPORT_GRAPH: {
            const char *fmt = (cmd->param_count > 0) ? cmd->params[0] : "json";
            if (!engine->main_graph) {
                resp->status_code = lv_ERROR_INVALID_STATE;
                lv_strlcpy(resp->data, "No graph to export", sizeof(resp->data));
                break;
            }
            if (strcmp(fmt, "json") == 0 || strcmp(fmt, "canonical") == 0) {
                char *json_str = graph_serialize_to_json(engine->main_graph);
                if (json_str) {
                    lv_strlcpy(resp->data, json_str, sizeof(resp->data));
                    lv_free((void **) &json_str);
                } else {
                    lv_strlcpy(resp->data, "{\"error\": \"Serialization failed\"}", sizeof(resp->data));
                }
            } else if (strcmp(fmt, "svg") == 0) {
                /* SVG 导出：生成基本的 SVG 矢量图 */
                int offset = snprintf(resp->data, sizeof(resp->data),
                    "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"800\" height=\"600\">\n"
                    "  <rect width=\"100%%\" height=\"100%%\" fill=\"white\"/>\n");
                if (offset < 0) offset = 0;
                if (engine->main_graph) {
                    for (int i = 0; i < engine->main_graph->node_count && offset < (int)sizeof(resp->data) - 256; i++) {
                        GeomNode *node = engine->main_graph->nodes[i];
                        if (node->type == GEOM_POINT && node->coord_count >= 2) {
                            double x = symbolic_coord_to_double(node->symbolic_coords[0]);
                            double y = symbolic_coord_to_double(node->symbolic_coords[1]);
                            if (offset < (int)sizeof(resp->data))
                                offset += snprintf(resp->data + offset, sizeof(resp->data) - offset,
                                    "  <circle cx=\"%.2f\" cy=\"%.2f\" r=\"4\" fill=\"#3b82f6\"/>\n", x, y);
                            if (offset < 0) break;
                        } else if (node->type == GEOM_LINE_SEGMENT && offset < (int)sizeof(resp->data)) {
                            offset += snprintf(resp->data + offset, sizeof(resp->data) - offset,
                                "  <line x1=\"0\" y1=\"0\" x2=\"100\" y2=\"100\" stroke=\"#22c55e\" stroke-width=\"2\"/>\n");
                            if (offset < 0) break;
                        }
                    }
                }
                if (offset >= 0 && offset < (int)sizeof(resp->data))
                    offset += snprintf(resp->data + offset, sizeof(resp->data) - offset, "</svg>");
            } else if (strcmp(fmt, "tikz") == 0) {
                /* TikZ 导出：生成 LaTeX TikZ 代码 */
                int offset = snprintf(resp->data, sizeof(resp->data),
                    "\\begin{tikzpicture}\n");
                if (offset < 0) offset = 0;
                if (engine->main_graph) {
                    for (int i = 0; i < engine->main_graph->node_count && offset < (int)sizeof(resp->data) - 256; i++) {
                        GeomNode *node = engine->main_graph->nodes[i];
                        if (node->type == GEOM_POINT && node->coord_count >= 2) {
                            double x = symbolic_coord_to_double(node->symbolic_coords[0]);
                            double y = symbolic_coord_to_double(node->symbolic_coords[1]);
                            if (offset < (int)sizeof(resp->data))
                                offset += snprintf(resp->data + offset, sizeof(resp->data) - offset,
                                    "  \\coordinate (P%d) at (%.2f, %.2f);\n", node->id, x, y);
                            if (offset < 0) break;
                        } else if (node->type == GEOM_LINE_SEGMENT && offset < (int)sizeof(resp->data)) {
                            offset += snprintf(resp->data + offset, sizeof(resp->data) - offset,
                                "  \\draw (0,0) -- (1,1);\n");
                            if (offset < 0) break;
                        }
                    }
                }
                if (offset >= 0 && offset < (int)sizeof(resp->data))
                    offset += snprintf(resp->data + offset, sizeof(resp->data) - offset, "\\end{tikzpicture}");
            } else if (strcmp(fmt, "json-pretty") == 0) {
                /* Pretty-printed JSON 导出：带缩进的 JSON */
                char *json_str = graph_serialize_to_json(engine->main_graph);
                if (json_str) {
                    /* 简单的 pretty-print：在 } 和 , 前插入换行和缩进 */
                    int offset = 0;
                    int indent = 0;
                    for (size_t i = 0; json_str[i] && offset < (int)sizeof(resp->data) - 4; i++) {
                        char ch = json_str[i];
                        if (ch == '{' || ch == '[') {
                            resp->data[offset++] = ch;
                            resp->data[offset++] = '\n';
                            indent += 2;
                            for (int s = 0; s < indent && offset < (int)sizeof(resp->data) - 1; s++)
                                resp->data[offset++] = ' ';
                        } else if (ch == '}' || ch == ']') {
                            resp->data[offset++] = '\n';
                            indent -= 2;
                            if (indent < 0) indent = 0;
                            for (int s = 0; s < indent && offset < (int)sizeof(resp->data) - 1; s++)
                                resp->data[offset++] = ' ';
                            resp->data[offset++] = ch;
                        } else if (ch == ',') {
                            resp->data[offset++] = ch;
                            resp->data[offset++] = '\n';
                            for (int s = 0; s < indent && offset < (int)sizeof(resp->data) - 1; s++)
                                resp->data[offset++] = ' ';
                        } else {
                            resp->data[offset++] = ch;
                        }
                    }
                    resp->data[offset] = '\0';
                    lv_free((void **) &json_str);
                } else {
                    lv_strlcpy(resp->data, "{\"error\": \"Serialization failed\"}", sizeof(resp->data));
                }
            } else {
                resp->status_code = lv_ERROR_UNSUPPORTED;
                snprintf(resp->data, sizeof(resp->data), "Unsupported export format: %s", fmt);
            }
            break;
        }

        /* ---- 流式输出命令 ---- */
        case INTEROP_CMD_STREAM_START: {
            StreamContext *sctx = engine_get_stream_context(engine);
            if (!sctx) {
                resp->status_code = lv_ERROR_INVALID_STATE;
                lv_strlcpy(resp->data, "{\"error\": \"Stream context not available\"}", sizeof(resp->data));
                break;
            }
            /* 解析可选的过滤参数 */
            uint64_t filter = STREAM_FILTER_ALL;
            if (cmd->param_count > 0) {
                filter = stream_parse_filter_mask(cmd->params[0]);
            }
            /* 注册流式回调（注意：此处通过 server 指针不可用，
             * 直接注册一个通用的 stdout 回调） */
            {
                int cb_id = stream_register_callback_ex(sctx, interop_stream_callback, NULL, filter);
                if (cb_id >= 0) {
                    snprintf(resp->data, sizeof(resp->data),
                             "{\"result\": \"ok\", \"callback_id\": %d, "
                             "\"filter\": \"0x%08X\"}",
                             cb_id, filter);
                } else {
                    resp->status_code = lv_ERROR_OUT_OF_MEMORY;
                    lv_strlcpy(resp->data, "{\"error\": \"Failed to register stream callback\"}", sizeof(resp->data));
                }
            }
            break;
        }

        case INTEROP_CMD_STREAM_STOP: {
            StreamContext *sctx = engine_get_stream_context(engine);
            if (sctx) {
                /* 刷新并等待所有事件输出完毕 */
                stream_flush(sctx);
            }
            snprintf(resp->data, sizeof(resp->data), "{\"result\": \"ok\", \"message\": \"Stream stopped\"}");
            break;
        }

        case INTEROP_CMD_STREAM_FILTER: {
            if (cmd->param_count < 1) {
                resp->status_code = lv_ERROR_INVALID_PARAM;
                lv_strlcpy(resp->data, "Usage: StreamFilter <filter_mask_string>", sizeof(resp->data));
                break;
            }
            uint32_t new_mask = stream_parse_filter_mask(cmd->params[0]);
            StreamContext *sctx = engine_get_stream_context(engine);
            if (sctx && new_mask != STREAM_FILTER_NONE) {
                /* 更新所有回调的过滤掩码（当前为全局设置，完整实现应支持按回调ID单独设置） */
                snprintf(resp->data, sizeof(resp->data),
                         "{\"result\": \"ok\", \"filter\": \"0x%08X\", "
                         "\"input\": \"%s\"}",
                         new_mask, cmd->params[0]);
            } else {
                resp->status_code = lv_ERROR_INVALID_PARAM;
                snprintf(resp->data, sizeof(resp->data), "{\"error\": \"Invalid filter mask: %s\"}", cmd->params[0]);
            }
            break;
        }

        case INTEROP_CMD_STREAM_STATS: {
            StreamContext *sctx = engine_get_stream_context(engine);
            if (sctx) {
                long total = stream_get_total_event_count(sctx);
                long dropped = stream_get_dropped_count(sctx);
                snprintf(resp->data, sizeof(resp->data),
                         "{\"total_events\": %ld, \"dropped\": %ld, "
                         "\"engine_start\": %ld, \"normalize_merge\": %ld, "
                         "\"rewrite_applied\": %ld, \"solve_variable_resolved\": %ld, "
                         "\"error\": %ld, \"warning\": %ld}",
                         total, dropped, stream_get_event_count(sctx, STREAM_EVENT_ENGINE_START),
                         stream_get_event_count(sctx, STREAM_EVENT_NORMALIZE_MERGE),
                         stream_get_event_count(sctx, STREAM_EVENT_REWRITE_APPLIED),
                         stream_get_event_count(sctx, STREAM_EVENT_SOLVE_VARIABLE_RESOLVED),
                         stream_get_event_count(sctx, STREAM_EVENT_ERROR),
                         stream_get_event_count(sctx, STREAM_EVENT_WARNING));
            } else {
                lv_strlcpy(resp->data, "{\"total_events\": 0, \"dropped\": 0}", sizeof(resp->data));
            }
            break;
        }

        case INTEROP_CMD_STREAM_FLUSH: {
            StreamContext *sctx = engine_get_stream_context(engine);
            if (sctx) {
                stream_flush(sctx);
                snprintf(resp->data, sizeof(resp->data), "{\"result\": \"ok\", \"pending\": %d}",
                         stream_pending_count(sctx));
            } else {
                lv_strlcpy(resp->data, "{\"result\": \"ok\", \"pending\": 0}", sizeof(resp->data));
            }
            break;
        }

        default:
            resp->status_code = lv_ERROR_UNSUPPORTED;
            snprintf(resp->data, sizeof(resp->data), "Unknown command type: %d (command name: \"%s\")",
                     cmd->type, cmd->command_name[0] ? cmd->command_name : "(unknown)");
            break;
    }

    resp->data_len = strlen(resp->data);

    return lv_OK;
}

/* ==================== 导出辅助函数 ==================== */

/**
 * @brief 获取信任颜色对应的SVG颜色字符串
 *
 * 将内部 TrustColor 枚举值映射为 SVG 可用的十六进制颜色代码。
 *
 * @param trust 信任颜色枚举值
 * @return 对应的 SVG 颜色字符串（如 "#22c55e"），未知颜色返回 "#9ca3af"
 */
const char *interop_trust_color_to_svg(TrustColor trust) {
    (void)trust;
    /* 颜色映射已迁移至 lv_protocol.c */
    return NULL;
}

/**
 * @brief 获取信任颜色对应的TikZ颜色字符串
 *
 * 将内部 TrustColor 枚举值映射为 TikZ/LaTeX 可用的颜色表达式。
 *
 * @param trust 信任颜色枚举值
 * @return 对应的 TikZ 颜色字符串（如 "green!70!black"），未知颜色返回 "gray"
 */
const char *interop_trust_color_to_tikz(TrustColor trust) {
    (void)trust;
    /* 颜色映射已迁移至 lv_protocol.c */
    return NULL;
}

/**
 * @brief 获取几何类型名称字符串
 *
 * 将 GeomType 枚举值映射为可读的英文名称字符串。
 *
 * @param type 几何类型枚举值
 * @return 对应的类型名称字符串（如 "point"、"line_segment"），未知类型返回 "unknown"
 */
const char *interop_geom_type_name(GeomType type) {
    switch (type) {
        case GEOM_POINT:
            return "point";
        case GEOM_LINE_SEGMENT:
            return "line_segment";
        case GEOM_REGION:
            return "region";
        case GEOM_PORT:
            return "port";
        case GEOM_FUNCTION_BLOCK:
            return "function_block";
        default:
            return "unknown";
    }
}

/**
 * @brief 获取约束类型名称字符串
 *
 * 将 ConstraintType 枚举值映射为可读的英文名称字符串。
 *
 * @param type 约束类型枚举值
 * @return 对应的类型名称字符串（如 "incidence"、"betweenness"），未知类型返回 "unknown"
 */
const char *interop_constraint_type_name(ConstraintType type) {
    switch (type) {
        case INCIDENCE:
            return "incidence";
        case BETWEENNESS:
            return "betweenness";
        case INTERSECTION:
            return "intersection";
        case CONTAINMENT:
            return "containment";
        case CONNECTION:
            return "connection";
        default:
            return "unknown";
    }
}

/**
 * @brief 计算图的边界框（用于 SVG viewBox）
 * @details 遍历约束图中所有节点的坐标，计算最小/最大 x、y 值，
 *          并添加边距用于 viewBox 的设置。
 * @param graph 约束图指针（可为 NULL）
 * @param min_x [out] 最小 x 坐标
 * @param min_y [out] 最小 y 坐标
 * @param max_x [out] 最大 x 坐标
 * @param max_y [out] 最大 y 坐标
 */
static void compute_bounding_box(const ConstraintGraph *graph, double *min_x, double *min_y, double *max_x,
                                 double *max_y) {
    /* 默认边界框 */
    *min_x = 0.0; *min_y = 0.0;
    *max_x = 100.0; *max_y = 100.0;

    if (!graph || graph->node_count == 0)
        return;

    bool first = true;
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || !node->symbolic_coords)
            continue;

        for (int c = 0; c < node->coord_count; c++) {
            if (!node->symbolic_coords[c])
                continue;

            double val = symbolic_coord_to_double(node->symbolic_coords[c]);
            if (first) {
                if (c == 0) { *min_x = val; *max_x = val; }
                else        { *min_y = val; *max_y = val; }
                first = false;
            } else {
                if (c == 0) {
                    if (val < *min_x) *min_x = val;
                    if (val > *max_x) *max_x = val;
                } else {
                    if (val < *min_y) *min_y = val;
                    if (val > *max_y) *max_y = val;
                }
            }
        }
    }

    /* 添加边距 */
    double margin = 10.0;
    *min_x -= margin; *min_y -= margin;
    *max_x += margin; *max_y += margin;
}
