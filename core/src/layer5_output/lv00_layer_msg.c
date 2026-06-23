/**
 * @file lv00_layer_msg.c
 * @brief 层间通信消息实现
 */

#include "lv00/lv00_layer_msg.h"
#include "lv00/lv00_utils.h"
#include <stdlib.h>
#include <string.h>

static uint32_t g_msg_sequence = 0;

/* ---- 消息类型名称映射 ---- */

static const char *msg_type_name(Lv00MsgType type) {
    switch (type) {
        case LV00_MSG_NONE:              return "NONE";
        case LV00_MSG_ERROR:             return "ERROR";
        case LV00_MSG_HEARTBEAT:         return "HEARTBEAT";
        case LV00_MSG_PARSE_TEXT:        return "PARSE_TEXT";
        case LV00_MSG_PARSE_DSL:         return "PARSE_DSL";
        case LV00_MSG_PARSE_FORMULA:     return "PARSE_FORMULA";
        case LV00_MSG_PARSE_RESULT:      return "PARSE_RESULT";
        case LV00_MSG_RESOURCE_ALLOC:    return "RESOURCE_ALLOC";
        case LV00_MSG_RESOURCE_FREE:     return "RESOURCE_FREE";
        case LV00_MSG_CONFIG_GET:        return "CONFIG_GET";
        case LV00_MSG_CONFIG_SET:        return "CONFIG_SET";
        case LV00_MSG_GRAPH_ADD_NODE:    return "GRAPH_ADD_NODE";
        case LV00_MSG_GRAPH_REMOVE_NODE: return "GRAPH_REMOVE_NODE";
        case LV00_MSG_GRAPH_ADD_CONSTRAINT: return "GRAPH_ADD_CONSTRAINT";
        case LV00_MSG_GRAPH_SERIALIZE:   return "GRAPH_SERIALIZE";
        case LV00_MSG_GRAPH_QUERY:       return "GRAPH_QUERY";
        case LV00_MSG_GRAPH_SOLVED:      return "GRAPH_SOLVED";
        case LV00_MSG_PROOF_ADD_STEP:    return "PROOF_ADD_STEP";
        case LV00_MSG_PROOF_REMOVE_STEP: return "PROOF_REMOVE_STEP";
        case LV00_MSG_PROOF_QUERY:       return "PROOF_QUERY";
        case LV00_MSG_PROOF_COMPLETED:   return "PROOF_COMPLETED";
        case LV00_MSG_PROOF_FAILED:      return "PROOF_FAILED";
        case LV00_MSG_SOLVER_RUN:        return "SOLVER_RUN";
        case LV00_MSG_SOLVER_RESULT:     return "SOLVER_RESULT";
        case LV00_MSG_REWRITE_APPLY:     return "REWRITE_APPLY";
        case LV00_MSG_REWRITE_RESULT:    return "REWRITE_RESULT";
        case LV00_MSG_UNIFY_CHECK:       return "UNIFY_CHECK";
        case LV00_MSG_UNIFY_RESULT:      return "UNIFY_RESULT";
        case LV00_MSG_TYPE_CHECK:        return "TYPE_CHECK";
        case LV00_MSG_TYPE_RESULT:       return "TYPE_RESULT";
        case LV00_MSG_EXPORT_GEOJSON:    return "EXPORT_GEOJSON";
        case LV00_MSG_EXPORT_TIKZ:       return "EXPORT_TIKZ";
        case LV00_MSG_EXPORT_COQ:        return "EXPORT_COQ";
        case LV00_MSG_EXPORT_LEAN:       return "EXPORT_LEAN";
        case LV00_MSG_VISUAL_RENDER:     return "VISUAL_RENDER";
        case LV00_MSG_VISUAL_QUERY:      return "VISUAL_QUERY";
        case LV00_MSG_ORCHESTRATE:       return "ORCHESTRATE";
        case LV00_MSG_ORCHESTRATE_RESULT:return "ORCHESTRATE_RESULT";
        case LV00_MSG_META_VERIFY:       return "META_VERIFY";
        case LV00_MSG_META_RESULT:       return "META_RESULT";
        case LV00_MSG_APP_INIT:          return "APP_INIT";
        case LV00_MSG_APP_SHUTDOWN:      return "APP_SHUTDOWN";
        case LV00_MSG_APP_QUERY:         return "APP_QUERY";
        case LV00_MSG_INTEROP_EXPORT:    return "INTEROP_EXPORT";
        case LV00_MSG_INTEROP_IMPORT:    return "INTEROP_IMPORT";
        case LV00_MSG_INTEROP_RESULT:    return "INTEROP_RESULT";
        default:                         return "UNKNOWN";
    }
}

/* ---- 生命周期 ---- */

Lv00LayerMessage *lv00_msg_create(Lv00MsgType type, Lv00MsgDirection dir,
                                  int sender, int target) {
    Lv00LayerMessage *msg = (Lv00LayerMessage *)lv00_malloc(sizeof(Lv00LayerMessage));
    if (!msg) return NULL;

    memset(msg, 0, sizeof(*msg));
    msg->type = type;
    msg->direction = dir;
    msg->sender_layer = sender;
    msg->target_layer = target;
    snprintf(msg->name, sizeof(msg->name), "%s", msg_type_name(type));

    return msg;
}

void lv00_msg_destroy(Lv00LayerMessage *msg) {
    if (!msg) return;
    /* 如果 payload 是堆分配的字符串/缓冲区，调用者应在此之前自行释放。
       这里只释放消息结构本身。 */
    if (msg->payload) {
        lv00_free((void **)&msg->payload);
    }
    lv00_free((void **)&msg);
}

void lv00_msg_sign(Lv00LayerMessage *msg) {
    if (!msg) return;
    msg->msg_id = ++g_msg_sequence;
    snprintf(msg->name, sizeof(msg->name), "%s#%u",
             msg_type_name(msg->type), msg->msg_id);
}
