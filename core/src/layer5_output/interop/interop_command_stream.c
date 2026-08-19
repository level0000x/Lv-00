/**
 * @file interop_command_stream.c
 * @brief Stream 命令族（由 interop_command.c 拆分子模块）
 *
 * @details StreamStart/Stop/Filter/Stats/Flush 命令：流式回调注册、
 *          过滤器设置与统计查询。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/lv_platform.h"
#include <float.h>
#include <math.h>
#include <stdarg.h>
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

#include "lv/debug.h"
#include "interop_export_internal.h" /* 公共信任颜色全字段表 kTrustColorEntries */
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/stream.h" /* LV_STREAM_CTX_DEFINE */
#include "interop_command_internal.h"

static int s_stream_callback_id = -1;

int handle_cmd_stream_start(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
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
            lv_snprintf(resp->data, sizeof(resp->data),
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

int handle_cmd_stream_stop(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
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

int handle_cmd_stream_filter(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    if (cmd->param_count < 1) {
        resp->status_code = lv_ERROR_INVALID_PARAM;
        lv_strlcpy(resp->data, "Usage: StreamFilter <filter_mask_string>", sizeof(resp->data));
        return lv_OK;
    }
    uint32_t new_mask = stream_parse_filter_mask(cmd->params[0]);
    StreamContext *sctx = engine_get_stream_context(engine);
    if (sctx && new_mask != STREAM_FILTER_NONE) {
        char filter_buf[32];
        lv_snprintf(filter_buf, sizeof(filter_buf), "0x%08X", new_mask);
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
        lv_snprintf(err_buf, sizeof(err_buf), "Invalid filter mask: %s", cmd->params[0]);
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

int handle_cmd_stream_stats(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    (void)cmd;
    StreamContext *sctx = engine_get_stream_context(engine);
    if (sctx) {
        long total = stream_get_total_event_count(sctx);
        long dropped = stream_get_dropped_count(sctx);
        lv_snprintf(resp->data, sizeof(resp->data),
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

int handle_cmd_stream_flush(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
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
