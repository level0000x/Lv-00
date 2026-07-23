/**
 * @file lv_layer_msg.c
 * @brief 层间通信消息实现
 *
 * @details 实现 Lv-00 系统各层之间的消息传递机制：
 *          - 消息创建与销毁（lv_msg_create / lv_msg_destroy）
 *          - 消息签名（lv_msg_sign）：分配唯一序列号
 *          - 消息类型名称映射（msg_type_name）
 *          消息携带类型、方向、发送者/目标层 ID、负载（payload）等字段。
 *
 * @author Lv-00 Project
 */

#include "lv/lv_layer_msg.h"

#include <stdlib.h>
#include <string.h>

#include "lv/lv_utils.h"

/** 全局消息序列号计数器，每创建一条消息自增一次 */
static uint32_t g_msg_sequence = 0;

/* ---- 消息类型名称映射 ---- */

/**
 * @brief 将消息类型枚举值转换为人类可读的字符串
 *
 * @param type 消息类型枚举值
 * @return 类型名称字符串（如 "PARSE_TEXT"、"GRAPH_ADD_NODE"），未知类型返回 "UNKNOWN"
 */
static const char *msg_type_name(lvMsgType type) {
    switch (type) {
        case lv_MSG_NONE:
            return "NONE";
        case lv_MSG_ERROR:
            return "ERROR";
        case lv_MSG_HEARTBEAT:
            return "HEARTBEAT";
        case lv_MSG_PARSE_TEXT:
            return "PARSE_TEXT";
        case lv_MSG_PARSE_DSL:
            return "PARSE_DSL";
        case lv_MSG_PARSE_FORMULA:
            return "PARSE_FORMULA";
        case lv_MSG_PARSE_RESULT:
            return "PARSE_RESULT";
        case lv_MSG_RESOURCE_ALLOC:
            return "RESOURCE_ALLOC";
        case lv_MSG_RESOURCE_FREE:
            return "RESOURCE_FREE";
        case lv_MSG_CONFIG_GET:
            return "CONFIG_GET";
        case lv_MSG_CONFIG_SET:
            return "CONFIG_SET";
        case lv_MSG_GRAPH_ADD_NODE:
            return "GRAPH_ADD_NODE";
        case lv_MSG_GRAPH_REMOVE_NODE:
            return "GRAPH_REMOVE_NODE";
        case lv_MSG_GRAPH_ADD_CONSTRAINT:
            return "GRAPH_ADD_CONSTRAINT";
        case lv_MSG_GRAPH_SERIALIZE:
            return "GRAPH_SERIALIZE";
        case lv_MSG_GRAPH_QUERY:
            return "GRAPH_QUERY";
        case lv_MSG_GRAPH_SOLVED:
            return "GRAPH_SOLVED";
        case lv_MSG_PROOF_ADD_STEP:
            return "PROOF_ADD_STEP";
        case lv_MSG_PROOF_REMOVE_STEP:
            return "PROOF_REMOVE_STEP";
        case lv_MSG_PROOF_QUERY:
            return "PROOF_QUERY";
        case lv_MSG_PROOF_COMPLETED:
            return "PROOF_COMPLETED";
        case lv_MSG_PROOF_FAILED:
            return "PROOF_FAILED";
        case lv_MSG_SOLVER_RUN:
            return "SOLVER_RUN";
        case lv_MSG_SOLVER_RESULT:
            return "SOLVER_RESULT";
        case lv_MSG_REWRITE_APPLY:
            return "REWRITE_APPLY";
        case lv_MSG_REWRITE_RESULT:
            return "REWRITE_RESULT";
        case lv_MSG_UNIFY_CHECK:
            return "UNIFY_CHECK";
        case lv_MSG_UNIFY_RESULT:
            return "UNIFY_RESULT";
        case lv_MSG_TYPE_CHECK:
            return "TYPE_CHECK";
        case lv_MSG_TYPE_RESULT:
            return "TYPE_RESULT";
        case lv_MSG_EXPORT_GEOJSON:
            return "EXPORT_GEOJSON";
        case lv_MSG_EXPORT_TIKZ:
            return "EXPORT_TIKZ";
        case lv_MSG_EXPORT_COQ:
            return "EXPORT_COQ";
        case lv_MSG_EXPORT_LEAN:
            return "EXPORT_LEAN";
        case lv_MSG_VISUAL_RENDER:
            return "VISUAL_RENDER";
        case lv_MSG_VISUAL_QUERY:
            return "VISUAL_QUERY";
        case lv_MSG_ORCHESTRATE:
            return "ORCHESTRATE";
        case lv_MSG_ORCHESTRATE_RESULT:
            return "ORCHESTRATE_RESULT";
        case lv_MSG_META_VERIFY:
            return "META_VERIFY";
        case lv_MSG_META_RESULT:
            return "META_RESULT";
        case lv_MSG_APP_INIT:
            return "APP_INIT";
        case lv_MSG_APP_SHUTDOWN:
            return "APP_SHUTDOWN";
        case lv_MSG_APP_QUERY:
            return "APP_QUERY";
        case lv_MSG_INTEROP_EXPORT:
            return "INTEROP_EXPORT";
        case lv_MSG_INTEROP_IMPORT:
            return "INTEROP_IMPORT";
        case lv_MSG_INTEROP_RESULT:
            return "INTEROP_RESULT";
        default:
            return "UNKNOWN";
    }
}

/* ---- 生命周期 ---- */

lvLayerMessage *lv_msg_create(lvMsgType type, lvMsgDirection dir, int sender, int target) {
    lvLayerMessage *msg = (lvLayerMessage *) lv_calloc(1, sizeof(lvLayerMessage));
    if (!msg)
        return NULL;

    msg->type = type;
    msg->direction = dir;
    msg->sender_layer = sender;
    msg->target_layer = target;
    snprintf(msg->name, sizeof(msg->name), "%s", msg_type_name(type));

    return msg;
}

void lv_msg_destroy(lvLayerMessage *msg) {
    if (!msg)
        return;
    /* 如果 payload 是堆分配的字符串/缓冲区，调用者应在此之前自行释放。
       这里只释放消息结构本身。 */
    if (msg->payload) {
        lv_free((void **) &msg->payload);
    }
    lv_free((void **) &msg);
}

void lv_msg_sign(lvLayerMessage *msg) {
    if (!msg)
        return;
    msg->msg_id = ++g_msg_sequence;
    snprintf(msg->name, sizeof(msg->name), "%s#%u", msg_type_name(msg->type), msg->msg_id);
}
