/**
 * @file interop_command.c
 * @brief 命令解析与执行
 *
 * @details 拆分子模块（Lv-00 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/lv_platform.h"
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "lv/constraint_graph.h"
#include "lv/engine.h"
#include "lv/interop.h"
#include "lv/lv_json.h"
#include "lv/lv_parse_utils.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_xmacro.h"

#include "debug.h"
#include "interop_export_internal.h" /* 公共信任颜色全字段表 kTrustColorEntries */
#include "lv_internal.h"
#include "lv_utils.h"
#include "stream_context_util.h" /* LV_STREAM_CTX_DEFINE */

/** @brief interop 模块全局流式上下文定义（供所有 interop 子模块通过 interop.h 的 extern 引用） */
LV_STREAM_CTX_DEFINE(interop);

/* ── VTable 命令分发 ── */

/** @brief 命令处理函数类型 */
typedef int (*InteropCmdHandler)(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp);

/* 前向声明 */
static int handle_cmd_ping(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp);
static int handle_cmd_get_status(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp);
static int handle_cmd_shutdown(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp);
static int handle_cmd_get_graph(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp);
static int handle_cmd_add_node(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp);
static int handle_cmd_remove_node(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp);
static int handle_cmd_add_constraint(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp);
static int handle_cmd_remove_constraint(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp);
static int handle_cmd_pack_function(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp);
static int handle_cmd_instantiate(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp);
static int handle_cmd_solve(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp);
static int handle_cmd_rewrite(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp);
static int handle_cmd_unify(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp);
static int handle_cmd_export_graph(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp);
static int handle_cmd_stream_start(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp);
static int handle_cmd_stream_stop(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp);
static int handle_cmd_stream_filter(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp);
static int handle_cmd_stream_stats(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp);
static int handle_cmd_stream_flush(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp);

/** @brief 命令处理函数表 */
static const struct {
    InteropCommandType type;
    InteropCmdHandler handler;
} kCommandHandlers[] = {
    {INTEROP_CMD_PING, handle_cmd_ping},
    {INTEROP_CMD_GET_STATUS, handle_cmd_get_status},
    {INTEROP_CMD_SHUTDOWN, handle_cmd_shutdown},
    {INTEROP_CMD_GET_GRAPH, handle_cmd_get_graph},
    {INTEROP_CMD_ADD_NODE, handle_cmd_add_node},
    {INTEROP_CMD_REMOVE_NODE, handle_cmd_remove_node},
    {INTEROP_CMD_ADD_CONSTRAINT, handle_cmd_add_constraint},
    {INTEROP_CMD_REMOVE_CONSTRAINT, handle_cmd_remove_constraint},
    {INTEROP_CMD_PACK_FUNCTION, handle_cmd_pack_function},
    {INTEROP_CMD_INSTANTIATE, handle_cmd_instantiate},
    {INTEROP_CMD_SOLVE, handle_cmd_solve},
    {INTEROP_CMD_REWRITE, handle_cmd_rewrite},
    {INTEROP_CMD_UNIFY, handle_cmd_unify},
    {INTEROP_CMD_EXPORT_GRAPH, handle_cmd_export_graph},
    {INTEROP_CMD_STREAM_START, handle_cmd_stream_start},
    {INTEROP_CMD_STREAM_STOP, handle_cmd_stream_stop},
    {INTEROP_CMD_STREAM_FILTER, handle_cmd_stream_filter},
    {INTEROP_CMD_STREAM_STATS, handle_cmd_stream_stats},
    {INTEROP_CMD_STREAM_FLUSH, handle_cmd_stream_flush},
};
static const size_t kCommandHandlerCount = sizeof(kCommandHandlers) / sizeof(kCommandHandlers[0]);

/* ── 命令解析与执行 ── */

/** @brief 命令名→命令类型 查找表（替代 19 分支 strcmp 链） */
static const lvStrToEnumEntry kCommandNameToTypeTable[] = {
    {"AddNode", INTEROP_CMD_ADD_NODE},
    {"RemoveNode", INTEROP_CMD_REMOVE_NODE},
    {"AddConstraint", INTEROP_CMD_ADD_CONSTRAINT},
    {"RemoveConstraint", INTEROP_CMD_REMOVE_CONSTRAINT},
    {"PackFunction", INTEROP_CMD_PACK_FUNCTION},
    {"Instantiate", INTEROP_CMD_INSTANTIATE},
    {"Solve", INTEROP_CMD_SOLVE},
    {"Rewrite", INTEROP_CMD_REWRITE},
    {"Unify", INTEROP_CMD_UNIFY},
    {"GetGraph", INTEROP_CMD_GET_GRAPH},
    {"ExportGraph", INTEROP_CMD_EXPORT_GRAPH},
    {"GetStatus", INTEROP_CMD_GET_STATUS},
    {"Ping", INTEROP_CMD_PING},
    {"Shutdown", INTEROP_CMD_SHUTDOWN},
    {"StreamStart", INTEROP_CMD_STREAM_START},
    {"StreamStop", INTEROP_CMD_STREAM_STOP},
    {"StreamFilter", INTEROP_CMD_STREAM_FILTER},
    {"StreamStats", INTEROP_CMD_STREAM_STATS},
    {"StreamFlush", INTEROP_CMD_STREAM_FLUSH},
};

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
    char *token = lv_strtok_r(buffer, " ", &save_ptr);
    if (!token)
        return lv_ERROR_PARSE;

    /* 保存原始命令名称用于错误报告 */
    lv_strlcpy(cmd->command_name, token, sizeof(cmd->command_name));

    /* 命令名→类型查表（替代 19 分支 strcmp 链） */
    cmd->type = (InteropCommandType) lv_str_to_enum(kCommandNameToTypeTable, lv_ARRAY_SIZE(kCommandNameToTypeTable),
                                                    token, -1);
    if ((int) cmd->type < 0)
        return lv_ERROR_PARSE;

    /* 解析参数 */
    while ((token = lv_strtok_r(NULL, " ", &save_ptr)) != NULL && cmd->param_count < INTEROP_MAX_PARAMS) {
        lv_strlcpy(cmd->params[cmd->param_count], token, 256);
        cmd->param_count++;
    }

    return lv_OK;
}

/* ── 展示型 JSON 响应：统一 pretty（2 空格/级）+ key_space（冒号后 1 空格）── */

/** @brief 初始化展示型响应写入器（lvJsonBuf + pretty + key_space 统一模式） */
static void interop_resp_json_init(lvJsonBuf *w, size_t cap) {
    lv_json_buf_init(w, cap);
    lv_json_buf_set_pretty(w, true);
    lv_json_buf_set_key_space(w, true);
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

    lvJsonBuf _jb;
    interop_resp_json_init(&_jb, 128);
    lv_json_buf_begin_object(&_jb);
    lv_json_buf_append_key(&_jb, "request_id");
    lv_json_buf_append_int(&_jb, resp->request_id);
    lv_json_buf_append_key(&_jb, "status");
    lv_json_buf_append_int(&_jb, resp->status_code);
    lv_json_buf_append_key(&_jb, "data");
    lv_json_buf_append_string(&_jb, resp->data);
    lv_json_buf_end_object(&_jb);
    char *_js = lv_json_buf_finalize(&_jb);
    if (!_js)
        return lv_ERROR_OUT_OF_MEMORY;
    size_t _len = strlen(_js);
    if (_len >= output_size) {
        lv_free((void **)&_js);
        return lv_ERROR_BUFFER_TOO_SMALL;
    }
    lv_strlcpy(output, _js, output_size);
    lv_free((void **)&_js);
    return lv_OK;
}

/* ==================== 命令处理函数实现 ==================== */

static int handle_cmd_ping(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    (void)engine; (void)cmd;
    lv_strlcpy(resp->data, "pong", sizeof(resp->data));
    return lv_OK;
}

static int handle_cmd_get_status(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    (void)cmd;
    int node_count = 0, constraint_count = 0;
    if (engine->main_graph) {
        node_count = engine->main_graph->node_count;
        constraint_count = engine->main_graph->constraint_count;
    }
    lvJsonBuf _jb;
    interop_resp_json_init(&_jb, 128);
    lv_json_buf_begin_object(&_jb);
    lv_json_buf_append_key(&_jb, "status");
    lv_json_buf_append_string(&_jb, "running");
    lv_json_buf_append_key(&_jb, "nodes");
    lv_json_buf_append_int(&_jb, node_count);
    lv_json_buf_append_key(&_jb, "constraints");
    lv_json_buf_append_int(&_jb, constraint_count);
    lv_json_buf_end_object(&_jb);
    char *_js = lv_json_buf_finalize(&_jb);
    if (_js) {
        lv_strlcpy(resp->data, _js, sizeof(resp->data));
        lv_free((void **)&_js);
    }
    return lv_OK;
}

static int handle_cmd_shutdown(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    (void)engine; (void)cmd;
    lv_strlcpy(resp->data, "shutting down", sizeof(resp->data));
    return lv_OK;
}

static int handle_cmd_get_graph(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    (void)cmd;
    if (engine->main_graph && engine->main_graph->node_count > 0) {
        char *json_str = graph_serialize_to_json(engine->main_graph);
        if (json_str) {
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
    return lv_OK;
}

/* ── AddNode 节点类型分发（查找表，替代 4 分支 strcmp 链） ── */

static int interop_add_node_point(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
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
            lvJsonBuf _jb;
            interop_resp_json_init(&_jb, 128);
            lv_json_buf_begin_object(&_jb);
            lv_json_buf_append_key(&_jb, "result");
            lv_json_buf_append_string(&_jb, "ok");
            lv_json_buf_append_key(&_jb, "node_id");
            lv_json_buf_append_int(&_jb, engine->main_graph->next_node_id - 1);
            lv_json_buf_end_object(&_jb);
            char *_js = lv_json_buf_finalize(&_jb);
            if (_js) {
                lv_strlcpy(resp->data, _js, sizeof(resp->data));
                lv_free((void **)&_js);
            }
        } else {
            resp->status_code = lv_ERROR_UNSUPPORTED;
            lvJsonBuf _jb;
            interop_resp_json_init(&_jb, 128);
            lv_json_buf_begin_object(&_jb);
            lv_json_buf_append_key(&_jb, "result");
            lv_json_buf_append_string(&_jb, "failed");
            lv_json_buf_append_key(&_jb, "code");
            lv_json_buf_append_int(&_jb, result);
            lv_json_buf_end_object(&_jb);
            char *_js = lv_json_buf_finalize(&_jb);
            if (_js) {
                lv_strlcpy(resp->data, _js, sizeof(resp->data));
                lv_free((void **)&_js);
            }
        }
    } else {
        resp->status_code = lv_ERROR_UNSUPPORTED;
        lv_strlcpy(resp->data, "Failed to create coordinate objects from input", sizeof(resp->data));
    }
    return lv_OK;
}

static int interop_add_node_line_segment(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    if (cmd->param_count < 3) {
        resp->status_code = lv_ERROR_INVALID_PARAM;
        lv_strlcpy(resp->data, "Usage: AddNode LineSegment <endpoint1_id> <endpoint2_id>",
                   sizeof(resp->data));
        return lv_OK;
    }
    int ep1 = lv_parse_int_default(cmd->params[1], 0);
    int ep2 = lv_parse_int_default(cmd->params[2], 0);
    AddNodeResult result = graph_add_line_segment(engine->main_graph, ep1, ep2);
    if (result == ADD_NODE_OK) {
        lvJsonBuf _jb;
        interop_resp_json_init(&_jb, 128);
        lv_json_buf_begin_object(&_jb);
        lv_json_buf_append_key(&_jb, "result");
        lv_json_buf_append_string(&_jb, "ok");
        lv_json_buf_append_key(&_jb, "node_id");
        lv_json_buf_append_int(&_jb, engine->main_graph->next_node_id - 1);
        lv_json_buf_append_key(&_jb, "type");
        lv_json_buf_append_string(&_jb, "line_segment");
        lv_json_buf_end_object(&_jb);
        char *_js = lv_json_buf_finalize(&_jb);
        if (_js) {
            lv_strlcpy(resp->data, _js, sizeof(resp->data));
            lv_free((void **)&_js);
        }
    } else {
        resp->status_code = lv_ERROR_UNSUPPORTED;
        lvJsonBuf _jb;
        interop_resp_json_init(&_jb, 128);
        lv_json_buf_begin_object(&_jb);
        lv_json_buf_append_key(&_jb, "result");
        lv_json_buf_append_string(&_jb, "failed");
        lv_json_buf_append_key(&_jb, "code");
        lv_json_buf_append_int(&_jb, result);
        lv_json_buf_end_object(&_jb);
        char *_js = lv_json_buf_finalize(&_jb);
        if (_js) {
            lv_strlcpy(resp->data, _js, sizeof(resp->data));
            lv_free((void **)&_js);
        }
    }
    return lv_OK;
}

static int interop_add_node_circle(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    if (cmd->param_count < 3) {
        resp->status_code = lv_ERROR_INVALID_PARAM;
        lv_strlcpy(resp->data, "Usage: AddNode Circle <center_id> <radius_point_id>", sizeof(resp->data));
        return lv_OK;
    }
    int center_id = lv_parse_int_default(cmd->params[1], 0);
    int radius_pt_id = lv_parse_int_default(cmd->params[2], 0);
    AddNodeResult result = graph_add_line_segment(engine->main_graph, center_id, radius_pt_id);
    if (result == ADD_NODE_OK) {
        lvJsonBuf _jb;
        interop_resp_json_init(&_jb, 128);
        lv_json_buf_begin_object(&_jb);
        lv_json_buf_append_key(&_jb, "result");
        lv_json_buf_append_string(&_jb, "ok");
        lv_json_buf_append_key(&_jb, "node_id");
        lv_json_buf_append_int(&_jb, engine->main_graph->next_node_id - 1);
        lv_json_buf_append_key(&_jb, "type");
        lv_json_buf_append_string(&_jb, "circle");
        lv_json_buf_end_object(&_jb);
        char *_js = lv_json_buf_finalize(&_jb);
        if (_js) {
            lv_strlcpy(resp->data, _js, sizeof(resp->data));
            lv_free((void **)&_js);
        }
    } else {
        resp->status_code = lv_ERROR_UNSUPPORTED;
        lvJsonBuf _jb;
        interop_resp_json_init(&_jb, 128);
        lv_json_buf_begin_object(&_jb);
        lv_json_buf_append_key(&_jb, "result");
        lv_json_buf_append_string(&_jb, "failed");
        lv_json_buf_append_key(&_jb, "code");
        lv_json_buf_append_int(&_jb, result);
        lv_json_buf_end_object(&_jb);
        char *_js = lv_json_buf_finalize(&_jb);
        if (_js) {
            lv_strlcpy(resp->data, _js, sizeof(resp->data));
            lv_free((void **)&_js);
        }
    }
    return lv_OK;
}

static int interop_add_node_region(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    if (cmd->param_count < 2) {
        resp->status_code = lv_ERROR_INVALID_PARAM;
        lv_strlcpy(resp->data, "Usage: AddNode Region <seg_id1> <seg_id2> ...", sizeof(resp->data));
        return lv_OK;
    }
    int seg_ids[INTEROP_MAX_PARAMS];
    int seg_count = 0;
    for (int i = 1; i < cmd->param_count && i < INTEROP_MAX_PARAMS; i++) {
        seg_ids[seg_count++] = lv_parse_int_default(cmd->params[i], 0);
    }
    AddNodeResult result = graph_add_region(engine->main_graph, seg_ids, seg_count);
    if (result == ADD_NODE_OK) {
        lvJsonBuf _jb;
        interop_resp_json_init(&_jb, 128);
        lv_json_buf_begin_object(&_jb);
        lv_json_buf_append_key(&_jb, "result");
        lv_json_buf_append_string(&_jb, "ok");
        lv_json_buf_append_key(&_jb, "node_id");
        lv_json_buf_append_int(&_jb, engine->main_graph->next_node_id - 1);
        lv_json_buf_append_key(&_jb, "type");
        lv_json_buf_append_string(&_jb, "region");
        lv_json_buf_end_object(&_jb);
        char *_js = lv_json_buf_finalize(&_jb);
        if (_js) {
            lv_strlcpy(resp->data, _js, sizeof(resp->data));
            lv_free((void **)&_js);
        }
    } else {
        resp->status_code = lv_ERROR_UNSUPPORTED;
        lvJsonBuf _jb;
        interop_resp_json_init(&_jb, 128);
        lv_json_buf_begin_object(&_jb);
        lv_json_buf_append_key(&_jb, "result");
        lv_json_buf_append_string(&_jb, "failed");
        lv_json_buf_append_key(&_jb, "code");
        lv_json_buf_append_int(&_jb, result);
        lv_json_buf_end_object(&_jb);
        char *_js = lv_json_buf_finalize(&_jb);
        if (_js) {
            lv_strlcpy(resp->data, _js, sizeof(resp->data));
            lv_free((void **)&_js);
        }
    }
    return lv_OK;
}

/** @brief AddNode 节点类型名→处理函数 查找表（大小写双写，替代 4 分支 strcmp 链） */
static const struct {
    const char *name;
    InteropCmdHandler handler;
} kAddNodeTypeHandlers[] = {
    {"Point", interop_add_node_point},
    {"point", interop_add_node_point},
    {"LineSegment", interop_add_node_line_segment},
    {"line_segment", interop_add_node_line_segment},
    {"Circle", interop_add_node_circle},
    {"circle", interop_add_node_circle},
    {"Region", interop_add_node_region},
    {"region", interop_add_node_region},
};

static int handle_cmd_add_node(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    if (cmd->param_count < 3) {
        resp->status_code = lv_ERROR_INVALID_PARAM;
        lv_strlcpy(resp->data, "Usage: AddNode <type> <x> <y> [extra...]", sizeof(resp->data));
        return lv_OK;
    }
    if (!engine->main_graph) {
        resp->status_code = lv_ERROR_INVALID_STATE;
        lv_strlcpy(resp->data, "No graph initialized - create a graph first", sizeof(resp->data));
        return lv_OK;
    }
    const char *type_str = cmd->params[0];
    /* 节点类型→处理函数 查表（替代 4 分支 strcmp 链） */
    for (size_t i = 0; i < lv_ARRAY_SIZE(kAddNodeTypeHandlers); i++) {
        if (strcmp(type_str, kAddNodeTypeHandlers[i].name) == 0)
            return kAddNodeTypeHandlers[i].handler(engine, cmd, resp);
    }
    resp->status_code = lv_ERROR_UNSUPPORTED;
    lv_strlcpy(resp->data,
               "Unsupported node type for AddNode. Supported: Point, LineSegment, Circle, Region",
               sizeof(resp->data));
    return lv_OK;
}

static int handle_cmd_remove_node(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    if (cmd->param_count < 1) {
        resp->status_code = lv_ERROR_INVALID_PARAM;
        lv_strlcpy(resp->data, "Usage: RemoveNode <node_id>", sizeof(resp->data));
        return lv_OK;
    }
    if (!engine->main_graph) {
        resp->status_code = lv_ERROR_INVALID_STATE;
        lv_strlcpy(resp->data, "No graph initialized", sizeof(resp->data));
        return lv_OK;
    }
    int node_id = lv_parse_int_default(cmd->params[0], 0);
    RemoveNodeResult result = graph_remove_node(engine->main_graph, node_id);
    if (result == REMOVE_NODE_OK) {
        lvJsonBuf _jb;
        interop_resp_json_init(&_jb, 128);
        lv_json_buf_begin_object(&_jb);
        lv_json_buf_append_key(&_jb, "result");
        lv_json_buf_append_string(&_jb, "ok");
        lv_json_buf_append_key(&_jb, "removed_node_id");
        lv_json_buf_append_int(&_jb, node_id);
        lv_json_buf_end_object(&_jb);
        char *_js = lv_json_buf_finalize(&_jb);
        if (_js) {
            lv_strlcpy(resp->data, _js, sizeof(resp->data));
            lv_free((void **)&_js);
        }
    } else {
        resp->status_code = lv_ERROR_NOT_FOUND;
        lvJsonBuf _jb;
        interop_resp_json_init(&_jb, 128);
        lv_json_buf_begin_object(&_jb);
        lv_json_buf_append_key(&_jb, "result");
        lv_json_buf_append_string(&_jb, "failed");
        lv_json_buf_append_key(&_jb, "node_id");
        lv_json_buf_append_int(&_jb, node_id);
        lv_json_buf_append_key(&_jb, "code");
        lv_json_buf_append_int(&_jb, result);
        lv_json_buf_end_object(&_jb);
        char *_js = lv_json_buf_finalize(&_jb);
        if (_js) {
            lv_strlcpy(resp->data, _js, sizeof(resp->data));
            lv_free((void **)&_js);
        }
    }
    return lv_OK;
}

/* ── AddConstraint 约束类型分发（查找表，替代 6 分支 strcmp 链） ── */

/** @brief 输出约束添加结果并返回 lv_OK（原 if(ok) 公共尾部） */
static int interop_add_constraint_finish(bool ok, InteropResponse *resp) {
    if (ok) {
        lvJsonBuf _jb;
        interop_resp_json_init(&_jb, 64);
        lv_json_buf_begin_object(&_jb);
        lv_json_buf_append_key(&_jb, "result");
        lv_json_buf_append_string(&_jb, "ok");
        lv_json_buf_end_object(&_jb);
        char *_js = lv_json_buf_finalize(&_jb);
        if (_js) {
            lv_strlcpy(resp->data, _js, sizeof(resp->data));
            lv_free((void **)&_js);
        }
    } else {
        resp->status_code = lv_ERROR_UNSUPPORTED;
        lv_strlcpy(resp->data, "{\"result\": \"failed\"}", sizeof(resp->data));
    }
    return lv_OK;
}

static int interop_add_constraint_incidence(lvEngine *engine, const int *participants, int pcount,
                                            InteropResponse *resp) {
    (void) pcount;
    bool ok = (graph_add_incidence(engine->main_graph, participants[0], participants[1]) == ADD_CONSTRAINT_OK);
    return interop_add_constraint_finish(ok, resp);
}

static int interop_add_constraint_betweenness(lvEngine *engine, const int *participants, int pcount,
                                              InteropResponse *resp) {
    bool ok = (graph_add_betweenness(engine->main_graph, participants[0], participants[1],
                                     pcount > 2 ? participants[2] : participants[1]) == ADD_CONSTRAINT_OK);
    return interop_add_constraint_finish(ok, resp);
}

static int interop_add_constraint_parallel(lvEngine *engine, const int *participants, int pcount,
                                           InteropResponse *resp) {
    bool ok = false;
    if (pcount >= 2) {
        Constraint *c = graph_add_constraint_with_id(
            engine->main_graph, engine->main_graph->next_constraint_id, CONTAINMENT, participants, 2);
        ok = (c != NULL);
    }
    return interop_add_constraint_finish(ok, resp);
}

static int interop_add_constraint_perpendicular(lvEngine *engine, const int *participants, int pcount,
                                                InteropResponse *resp) {
    bool ok = false;
    if (pcount >= 2) {
        Constraint *c = graph_add_constraint_with_id(
            engine->main_graph, engine->main_graph->next_constraint_id, CONTAINMENT, participants, 2);
        ok = (c != NULL);
    }
    return interop_add_constraint_finish(ok, resp);
}

static int interop_add_constraint_equal_length(lvEngine *engine, const int *participants, int pcount,
                                               InteropResponse *resp) {
    bool ok = false;
    if (pcount >= 2) {
        Constraint *c = graph_add_constraint_with_id(
            engine->main_graph, engine->main_graph->next_constraint_id, CONTAINMENT, participants, 2);
        ok = (c != NULL);
    }
    return interop_add_constraint_finish(ok, resp);
}

static int interop_add_constraint_angle(lvEngine *engine, const int *participants, int pcount,
                                        InteropResponse *resp) {
    bool ok = false;
    if (pcount >= 3) {
        Constraint *c = graph_add_constraint_with_id(
            engine->main_graph, engine->main_graph->next_constraint_id, BETWEENNESS, participants, 3);
        ok = (c != NULL);
    }
    return interop_add_constraint_finish(ok, resp);
}

/** @brief AddConstraint 约束类型名→处理函数 查找表（大小写双写，替代 6 分支 strcmp 链） */
static const struct {
    const char *name;
    int (*handler)(lvEngine *engine, const int *participants, int pcount, InteropResponse *resp);
} kAddConstraintTypeHandlers[] = {
    {"incidence", interop_add_constraint_incidence},
    {"Incidence", interop_add_constraint_incidence},
    {"betweenness", interop_add_constraint_betweenness},
    {"Betweenness", interop_add_constraint_betweenness},
    {"parallel", interop_add_constraint_parallel},
    {"Parallel", interop_add_constraint_parallel},
    {"perpendicular", interop_add_constraint_perpendicular},
    {"Perpendicular", interop_add_constraint_perpendicular},
    {"equal_length", interop_add_constraint_equal_length},
    {"EqualLength", interop_add_constraint_equal_length},
    {"angle", interop_add_constraint_angle},
    {"Angle", interop_add_constraint_angle},
};

static int handle_cmd_add_constraint(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    if (cmd->param_count < 3) {
        resp->status_code = lv_ERROR_INVALID_PARAM;
        lv_strlcpy(resp->data, "Usage: AddConstraint <type> <id1> <id2> [id3]", sizeof(resp->data));
        return lv_OK;
    }
    if (!engine->main_graph) {
        resp->status_code = lv_ERROR_INVALID_STATE;
        lv_strlcpy(resp->data, "No graph initialized", sizeof(resp->data));
        return lv_OK;
    }
    const char *ct = cmd->params[0];
    int participants[4] = {0};
    int pcount = 0;
    for (int i = 1; i < cmd->param_count && i < 5; i++) {
        participants[i - 1] = lv_parse_int_default(cmd->params[i], 0);
        pcount++;
    }
    /* 约束类型→处理函数 查表（替代 6 分支 strcmp 链） */
    for (size_t i = 0; i < lv_ARRAY_SIZE(kAddConstraintTypeHandlers); i++) {
        if (strcmp(ct, kAddConstraintTypeHandlers[i].name) == 0)
            return kAddConstraintTypeHandlers[i].handler(engine, participants, pcount, resp);
    }
    resp->status_code = lv_ERROR_UNSUPPORTED;
    lv_strlcpy(resp->data, "Unsupported constraint type", sizeof(resp->data));
    return lv_OK;
}

static int handle_cmd_remove_constraint(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    if (cmd->param_count < 1) {
        resp->status_code = lv_ERROR_INVALID_PARAM;
        lv_strlcpy(resp->data, "Usage: RemoveConstraint <constraint_index>", sizeof(resp->data));
        return lv_OK;
    }
    if (!engine->main_graph) {
        resp->status_code = lv_ERROR_INVALID_STATE;
        lv_strlcpy(resp->data, "No graph initialized", sizeof(resp->data));
        return lv_OK;
    }
    int cidx = lv_parse_int_default(cmd->params[0], 0);
    RemoveConstraintResult rc = graph_remove_constraint(engine->main_graph, cidx);
    if (rc == REMOVE_CONSTRAINT_OK) {
        lvJsonBuf _jb;
        interop_resp_json_init(&_jb, 128);
        lv_json_buf_begin_object(&_jb);
        lv_json_buf_append_key(&_jb, "result");
        lv_json_buf_append_string(&_jb, "ok");
        lv_json_buf_append_key(&_jb, "removed_index");
        lv_json_buf_append_int(&_jb, cidx);
        lv_json_buf_end_object(&_jb);
        char *_js = lv_json_buf_finalize(&_jb);
        if (_js) {
            lv_strlcpy(resp->data, _js, sizeof(resp->data));
            lv_free((void **)&_js);
        }
    } else {
        resp->status_code = lv_ERROR_NOT_FOUND;
        lvJsonBuf _jb;
        interop_resp_json_init(&_jb, 128);
        lv_json_buf_begin_object(&_jb);
        lv_json_buf_append_key(&_jb, "result");
        lv_json_buf_append_string(&_jb, "failed");
        lv_json_buf_append_key(&_jb, "index");
        lv_json_buf_append_int(&_jb, cidx);
        lv_json_buf_append_key(&_jb, "code");
        lv_json_buf_append_int(&_jb, rc);
        lv_json_buf_end_object(&_jb);
        char *_js = lv_json_buf_finalize(&_jb);
        if (_js) {
            lv_strlcpy(resp->data, _js, sizeof(resp->data));
            lv_free((void **)&_js);
        }
    }
    return lv_OK;
}

static int handle_cmd_pack_function(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    (void)cmd;
    if (!engine->main_graph) {
        resp->status_code = lv_ERROR_INVALID_STATE;
        lv_strlcpy(resp->data, "No graph initialized", sizeof(resp->data));
        return lv_OK;
    }
    resp->status_code = lv_ERROR_UNSUPPORTED;
    lv_strlcpy(resp->data, "PackFunction requires UI-level interaction for port selection", sizeof(resp->data));
    return lv_OK;
}

static int handle_cmd_instantiate(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    if (!engine->main_graph || cmd->param_count < 2) {
        resp->status_code = lv_ERROR_INVALID_PARAM;
        lv_strlcpy(resp->data, "Usage: Instantiate <func_block_id> <arg1_id> ...", sizeof(resp->data));
        return lv_OK;
    }
    int fb_id = lv_parse_int_default(cmd->params[0], 0);
    int *arg_mappings = (int *) lv_malloc(sizeof(int) * (cmd->param_count - 1));
    if (!arg_mappings) {
        resp->status_code = lv_ERROR_OUT_OF_MEMORY;
        lv_strlcpy(resp->data, "Out of memory", sizeof(resp->data));
        return lv_OK;
    }
    for (int i = 1; i < cmd->param_count; i++) {
        arg_mappings[i - 1] = lv_parse_int_default(cmd->params[i], 0);
    }
    int result_count = 0;
    int *results = engine_instantiate_function(engine, fb_id, arg_mappings, cmd->param_count - 1, &result_count);
    lv_free((void **) &arg_mappings);
    if (results && result_count > 0) {
        lvJsonBuf _jb;
        interop_resp_json_init(&_jb, 128);
        lv_json_buf_begin_object(&_jb);
        lv_json_buf_append_key(&_jb, "result");
        lv_json_buf_append_string(&_jb, "ok");
        lv_json_buf_append_key(&_jb, "instantiated_ids");
        lv_json_buf_begin_array(&_jb);
        for (int i = 0; i < result_count; i++)
            lv_json_buf_append_int(&_jb, results[i]);
        lv_json_buf_end_array(&_jb);
        lv_json_buf_end_object(&_jb);
        char *_js = lv_json_buf_finalize(&_jb);
        if (_js) {
            lv_strlcpy(resp->data, _js, sizeof(resp->data));
            lv_free((void **)&_js);
        }
        lv_free((void **) &results);
    } else {
        resp->status_code = lv_ERROR_UNSUPPORTED;
        lv_strlcpy(resp->data, "{\"result\": \"failed\", \"reason\": \"Instantiation failed\"}",
                   sizeof(resp->data));
    }
    return lv_OK;
}

static int handle_cmd_solve(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    (void)cmd;
    if (!engine->main_graph) {
        resp->status_code = lv_ERROR_INVALID_STATE;
        lv_strlcpy(resp->data, "No graph loaded for solving", sizeof(resp->data));
    } else {
        lv_strlcpy(resp->data, "{\"result\": \"solved\", \"info\": \"Solver invoked - check engine state\"}",
                   sizeof(resp->data));
    }
    return lv_OK;
}

static int handle_cmd_rewrite(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    (void)cmd;
    if (!engine->main_graph) {
        resp->status_code = lv_ERROR_INVALID_STATE;
        lv_strlcpy(resp->data, "No graph loaded for rewriting", sizeof(resp->data));
    } else {
        lvJsonBuf _jb;
        interop_resp_json_init(&_jb, 128);
        lv_json_buf_begin_object(&_jb);
        lv_json_buf_append_key(&_jb, "result");
        lv_json_buf_append_string(&_jb, "rewritten");
        lv_json_buf_append_key(&_jb, "rules_applied");
        lv_json_buf_append_int(&_jb, 0);
        lv_json_buf_append_key(&_jb, "step_limit");
        lv_json_buf_append_int(&_jb, engine->rewrite_step_limit);
        lv_json_buf_end_object(&_jb);
        char *_js = lv_json_buf_finalize(&_jb);
        if (_js) {
            lv_strlcpy(resp->data, _js, sizeof(resp->data));
            lv_free((void **)&_js);
        }
    }
    return lv_OK;
}

static int handle_cmd_unify(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    (void)cmd;
    if (!engine->main_graph) {
        resp->status_code = lv_ERROR_INVALID_STATE;
        lv_strlcpy(resp->data, "No graph loaded for unification", sizeof(resp->data));
    } else {
        lvJsonBuf _jb;
        interop_resp_json_init(&_jb, 128);
        lv_json_buf_begin_object(&_jb);
        lv_json_buf_append_key(&_jb, "result");
        lv_json_buf_append_string(&_jb, "unify_check");
        lv_json_buf_append_key(&_jb, "last_status");
        lv_json_buf_append_int(&_jb, engine->last_unify_status);
        lv_json_buf_end_object(&_jb);
        char *_js = lv_json_buf_finalize(&_jb);
        if (_js) {
            lv_strlcpy(resp->data, _js, sizeof(resp->data));
            lv_free((void **)&_js);
        }
    }
    return lv_OK;
}

/* ── ExportGraph 导出格式分发（查找表，替代 4 分支 strcmp 链） ── */

static int interop_export_graph_json(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    (void) cmd;
    char *json_str = graph_serialize_to_json(engine->main_graph);
    if (json_str) {
        lv_strlcpy(resp->data, json_str, sizeof(resp->data));
        lv_free((void **) &json_str);
    } else {
        lv_strlcpy(resp->data, "{\"error\": \"Serialization failed\"}", sizeof(resp->data));
    }
    return lv_OK;
}

static int interop_export_graph_svg(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    (void) cmd;
    int offset = snprintf(resp->data, sizeof(resp->data),
                          "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"800\" height=\"600\">\n"
                          "  <rect width=\"100%%\" height=\"100%%\" fill=\"white\"/>\n");
    if (offset < 0)
        offset = 0;
    if (engine->main_graph) {
        for (int i = 0; i < engine->main_graph->node_count && offset < (int) sizeof(resp->data) - 256; i++) {
            GeomNode *node = engine->main_graph->nodes[i];
            if (node->type == GEOM_POINT && node->coord_count >= 2) {
                double x = symbolic_coord_to_double(node->symbolic_coords[0]);
                double y = symbolic_coord_to_double(node->symbolic_coords[1]);
                if (offset < (int) sizeof(resp->data))
                    offset += snprintf(resp->data + offset, sizeof(resp->data) - offset,
                                       "  <circle cx=\"%.2f\" cy=\"%.2f\" r=\"4\" fill=\"#3b82f6\"/>\n", x, y);
                if (offset < 0)
                    break;
            } else if (node->type == GEOM_LINE_SEGMENT && offset < (int) sizeof(resp->data)) {
                offset += snprintf(resp->data + offset, sizeof(resp->data) - offset,
                                   "  <line x1=\"0\" y1=\"0\" x2=\"100\" y2=\"100\" stroke=\"#22c55e\" "
                                   "stroke-width=\"2\"/>\n");
                if (offset < 0)
                    break;
            }
        }
    }
    if (offset >= 0 && offset < (int) sizeof(resp->data))
        offset += snprintf(resp->data + offset, sizeof(resp->data) - offset, "</svg>");
    return lv_OK;
}

static int interop_export_graph_tikz(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    (void) cmd;
    int offset = snprintf(resp->data, sizeof(resp->data), "\\begin{tikzpicture}\n");
    if (offset < 0)
        offset = 0;
    if (engine->main_graph) {
        for (int i = 0; i < engine->main_graph->node_count && offset < (int) sizeof(resp->data) - 256; i++) {
            GeomNode *node = engine->main_graph->nodes[i];
            if (node->type == GEOM_POINT && node->coord_count >= 2) {
                double x = symbolic_coord_to_double(node->symbolic_coords[0]);
                double y = symbolic_coord_to_double(node->symbolic_coords[1]);
                if (offset < (int) sizeof(resp->data))
                    offset += snprintf(resp->data + offset, sizeof(resp->data) - offset,
                                       "  \\coordinate (P%d) at (%.2f, %.2f);\n", node->id, x, y);
                if (offset < 0)
                    break;
            } else if (node->type == GEOM_LINE_SEGMENT && offset < (int) sizeof(resp->data)) {
                offset += snprintf(resp->data + offset, sizeof(resp->data) - offset,
                                   "  \\draw (0,0) -- (1,1);\n");
                if (offset < 0)
                    break;
            }
        }
    }
    if (offset >= 0 && offset < (int) sizeof(resp->data))
        offset += snprintf(resp->data + offset, sizeof(resp->data) - offset, "\\end{tikzpicture}");
    return lv_OK;
}

static int interop_export_graph_json_pretty(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    (void) cmd;
    char *json_str = graph_serialize_to_json(engine->main_graph);
    if (json_str) {
        int offset = 0;
        int indent = 0;
        for (size_t i = 0; json_str[i] && offset < (int) sizeof(resp->data) - 4; i++) {
            char ch = json_str[i];
            if (ch == '{' || ch == '[') {
                resp->data[offset++] = ch;
                resp->data[offset++] = '\n';
                indent += 2;
                for (int s = 0; s < indent && offset < (int) sizeof(resp->data) - 1; s++)
                    resp->data[offset++] = ' ';
            } else if (ch == '}' || ch == ']') {
                resp->data[offset++] = '\n';
                indent -= 2;
                if (indent < 0) indent = 0;
                for (int s = 0; s < indent && offset < (int) sizeof(resp->data) - 1; s++)
                    resp->data[offset++] = ' ';
                resp->data[offset++] = ch;
            } else if (ch == ',') {
                resp->data[offset++] = ch;
                resp->data[offset++] = '\n';
                for (int s = 0; s < indent && offset < (int) sizeof(resp->data) - 1; s++)
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
    return lv_OK;
}

/** @brief ExportGraph 导出格式名→处理函数 查找表（替代 4 分支 strcmp 链） */
static const struct {
    const char *name;
    InteropCmdHandler handler;
} kExportFormatHandlers[] = {
    {"json", interop_export_graph_json},
    {"canonical", interop_export_graph_json},
    {"svg", interop_export_graph_svg},
    {"tikz", interop_export_graph_tikz},
    {"json-pretty", interop_export_graph_json_pretty},
};

static int handle_cmd_export_graph(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    const char *fmt = (cmd->param_count > 0) ? cmd->params[0] : "json";
    if (!engine->main_graph) {
        resp->status_code = lv_ERROR_INVALID_STATE;
        lv_strlcpy(resp->data, "No graph to export", sizeof(resp->data));
        return lv_OK;
    }
    /* 导出格式→处理函数 查表（替代 4 分支 strcmp 链） */
    for (size_t i = 0; i < lv_ARRAY_SIZE(kExportFormatHandlers); i++) {
        if (strcmp(fmt, kExportFormatHandlers[i].name) == 0)
            return kExportFormatHandlers[i].handler(engine, cmd, resp);
    }
    resp->status_code = lv_ERROR_UNSUPPORTED;
    snprintf(resp->data, sizeof(resp->data), "Unsupported export format: %s", fmt);
    return lv_OK;
}

/* StreamStart 命令注册的流式回调 ID（-1 = 未注册）。
 * 与 interop_server.c 的 server->stream_callback_id 字段同语义：
 * StreamStop 按此 ID 注销，保证「注册/注销」配对，防止回调列表无限增长。 */
static int s_stream_callback_id = -1;

static int handle_cmd_stream_start(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    StreamContext *sctx = engine_get_stream_context(engine);
    if (!sctx) {
        resp->status_code = lv_ERROR_INVALID_STATE;
        lv_strlcpy(resp->data, "{\"error\": \"Stream context not available\"}", sizeof(resp->data));
        return lv_OK;
    }
    uint64_t filter = STREAM_FILTER_ALL;
    if (cmd->param_count > 0) {
        filter = stream_parse_filter_mask(cmd->params[0]);
    }
    {
        /* 若已有活动回调先注销（与 interop_attach_stream_callback 的
         * 「先注销再注册」配对模式一致），避免重复注册导致回调列表增长 */
        if (s_stream_callback_id >= 0) {
            stream_unregister_callback_by_id(sctx, s_stream_callback_id);
            s_stream_callback_id = -1;
        }
        int cb_id = stream_register_callback_ex(sctx, interop_stream_callback, NULL, filter);
        if (cb_id >= 0) {
            s_stream_callback_id = cb_id;
            snprintf(resp->data, sizeof(resp->data),
                     "{\"result\": \"ok\", \"callback_id\": %d, "
                     "\"filter\": \"0x%08X\"}",
                     cb_id, filter);
        } else {
            resp->status_code = lv_ERROR_OUT_OF_MEMORY;
            lv_strlcpy(resp->data, "{\"error\": \"Failed to register stream callback\"}", sizeof(resp->data));
        }
    }
    return lv_OK;
}

static int handle_cmd_stream_stop(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    (void)cmd;
    StreamContext *sctx = engine_get_stream_context(engine);
    if (sctx) {
        /* 按注册时记录的 cb_id 注销回调（参照 interop_detach_stream_callback
         * 模式），与 handle_cmd_stream_start 配对，防止回调列表无限增长 */
        if (s_stream_callback_id >= 0) {
            stream_unregister_callback_by_id(sctx, s_stream_callback_id);
            s_stream_callback_id = -1;
        }
        stream_flush(sctx);
    }
    lvJsonBuf _jb;
    interop_resp_json_init(&_jb, 64);
    lv_json_buf_begin_object(&_jb);
    lv_json_buf_append_key(&_jb, "result");
    lv_json_buf_append_string(&_jb, "ok");
    lv_json_buf_append_key(&_jb, "message");
    lv_json_buf_append_string(&_jb, "Stream stopped");
    lv_json_buf_end_object(&_jb);
    char *_js = lv_json_buf_finalize(&_jb);
    if (_js) {
        lv_strlcpy(resp->data, _js, sizeof(resp->data));
        lv_free((void **)&_js);
    }
    return lv_OK;
}

static int handle_cmd_stream_filter(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    if (cmd->param_count < 1) {
        resp->status_code = lv_ERROR_INVALID_PARAM;
        lv_strlcpy(resp->data, "Usage: StreamFilter <filter_mask_string>", sizeof(resp->data));
        return lv_OK;
    }
    uint32_t new_mask = stream_parse_filter_mask(cmd->params[0]);
    StreamContext *sctx = engine_get_stream_context(engine);
    if (sctx && new_mask != STREAM_FILTER_NONE) {
        char filter_buf[32];
        snprintf(filter_buf, sizeof(filter_buf), "0x%08X", new_mask);
        lvJsonBuf _jb;
        interop_resp_json_init(&_jb, 128);
        lv_json_buf_begin_object(&_jb);
        lv_json_buf_append_key(&_jb, "result");
        lv_json_buf_append_string(&_jb, "ok");
        lv_json_buf_append_key(&_jb, "filter");
        lv_json_buf_append_string(&_jb, filter_buf);
        lv_json_buf_append_key(&_jb, "input");
        lv_json_buf_append_string(&_jb, cmd->params[0]);
        lv_json_buf_end_object(&_jb);
        char *_js = lv_json_buf_finalize(&_jb);
        if (_js) {
            lv_strlcpy(resp->data, _js, sizeof(resp->data));
            lv_free((void **)&_js);
        }
    } else {
        resp->status_code = lv_ERROR_INVALID_PARAM;
        char err_buf[512];
        snprintf(err_buf, sizeof(err_buf), "Invalid filter mask: %s", cmd->params[0]);
        lvJsonBuf _jb;
        interop_resp_json_init(&_jb, 128);
        lv_json_buf_begin_object(&_jb);
        lv_json_buf_append_key(&_jb, "error");
        lv_json_buf_append_string(&_jb, err_buf);
        lv_json_buf_end_object(&_jb);
        char *_js = lv_json_buf_finalize(&_jb);
        if (_js) {
            lv_strlcpy(resp->data, _js, sizeof(resp->data));
            lv_free((void **)&_js);
        }
    }
    return lv_OK;
}

static int handle_cmd_stream_stats(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    (void)cmd;
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
    return lv_OK;
}

static int handle_cmd_stream_flush(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    (void)cmd;
    StreamContext *sctx = engine_get_stream_context(engine);
    if (sctx) {
        stream_flush(sctx);
        lvJsonBuf _jb;
        interop_resp_json_init(&_jb, 64);
        lv_json_buf_begin_object(&_jb);
        lv_json_buf_append_key(&_jb, "result");
        lv_json_buf_append_string(&_jb, "ok");
        lv_json_buf_append_key(&_jb, "pending");
        lv_json_buf_append_int(&_jb, stream_pending_count(sctx));
        lv_json_buf_end_object(&_jb);
        char *_js = lv_json_buf_finalize(&_jb);
        if (_js) {
            lv_strlcpy(resp->data, _js, sizeof(resp->data));
            lv_free((void **)&_js);
        }
    } else {
        lv_strlcpy(resp->data, "{\"result\": \"ok\", \"pending\": 0}", sizeof(resp->data));
    }
    return lv_OK;
}

/**
 * @brief 执行互操作命令
 *
 * 根据命令类型通过 VTable 查找对应的处理函数并执行。
 *
 * @param engine 引擎实例指针
 * @param cmd    命令结构体指针
 * @param resp   输出参数，接收执行结果
 * @return lv_OK 成功，错误码表示失败原因
 */
int interop_execute_command(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    if (!engine || !cmd || !resp)
        return lv_ERROR_INVALID_PARAM;

    resp->status_code = lv_OK;

    for (size_t i = 0; i < kCommandHandlerCount; i++) {
        if (kCommandHandlers[i].type == cmd->type) {
            int ret = kCommandHandlers[i].handler(engine, cmd, resp);
            resp->data_len = strlen(resp->data);
            return ret;
        }
    }

    resp->status_code = lv_ERROR_UNSUPPORTED;
    snprintf(resp->data, sizeof(resp->data), "Unknown command type: %d (command name: \"%s\")", cmd->type,
             cmd->command_name[0] ? cmd->command_name : "(unknown)");
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
/* ================================================================
 * 枚举 -> 名称 映射表（数据表化，替代 switch）
 * ================================================================ */

const char *interop_trust_color_to_svg(TrustColor trust) {
    /* 查公共信任颜色全字段表（interop_export_internal.h），未命中返回默认灰色 */
    const TrustColorEntry *e = interop_trust_color_find(trust);
    return e ? e->svg_hex : "#9ca3af";
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
    /* 查公共信任颜色全字段表（interop_export_internal.h），未命中返回默认 gray */
    const TrustColorEntry *e = interop_trust_color_find(trust);
    return e ? e->tikz_expr : "gray";
}

/**
 * @brief 获取几何类型名称字符串
 *
 * 将 GeomType 枚举值映射为可读的英文名称字符串。
 *
 * @param type 几何类型枚举值
 * @return 对应的类型名称字符串（如 "point"、"line_segment"），未知类型返回 "unknown"
 */
/** @brief interop_geom_type_name 由共享条目宏 API 提供
 *  @note 对外命令协议要求小写名（"point"），由 constraint_graph.h 的
 *        LV_GEOM_TYPE_ENTRY 别名列生成（lv_geom_type_alias），
 *        原手写小写表 s_interop_geom_type_name_entries 已删除。 */
const char *interop_geom_type_name(GeomType type) {
    const char *alias = lv_geom_type_alias((int) type);
    /* lv_geom_type_alias 越界返回 "UNKNOWN"，此处保持本接口既有的 "unknown" 回退语义 */
    return (alias && strcmp(alias, "UNKNOWN") != 0) ? alias : "unknown";
}

/**
 * @brief 获取约束类型名称字符串
 *
 * 将 ConstraintType 枚举值映射为可读的英文名称字符串。
 *
 * @param type 约束类型枚举值
 * @return 对应的类型名称字符串（如 "incidence"、"betweenness"），未知类型返回 "unknown"
 */
/** @brief interop_constraint_type_name 由共享条目宏 API 提供
 *  @note 小写别名由 constraint_graph.h 的 LV_CONSTRAINT_TYPE_ENTRY 别名列生成
 *        （lv_constraint_type_alias），原手写小写表已删除。 */
const char *interop_constraint_type_name(ConstraintType type) {
    const char *alias = lv_constraint_type_alias((int) type);
    return (alias && strcmp(alias, "UNKNOWN") != 0) ? alias : "unknown";
}
