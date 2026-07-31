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

#include "lv/lv_internal.h"
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
/* ================================================================
 * 枚举 -> 名称 映射表（数据表化，替代 switch）
 * ================================================================ */

/** @brief 枚举值 -> 名称 映射项（表必须按 code 升序排列） */
typedef struct {
    int code;         /**< 枚举值 */
    const char *name; /**< 名称字符串 */
} lv_msg_NameEntry;

/** @brief 二分查找枚举名称（表需按 code 升序） */
static const char *lv_msg_name_lookup(const lv_msg_NameEntry *table, size_t count, int code) {
    size_t lo = 0, hi = count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (table[mid].code == code)
            return table[mid].name;
        if (table[mid].code < code)
            lo = mid + 1;
        else
            hi = mid;
    }
    return NULL;
}

/** @brief msg_type_name 名称表（按枚举值升序） */
static const lv_msg_NameEntry s_msg_type_name_entries[] = {
    {lv_MSG_NONE, "NONE"},
    {lv_MSG_ERROR, "ERROR"},
    {lv_MSG_HEARTBEAT, "HEARTBEAT"},
    {lv_MSG_PARSE_TEXT, "PARSE_TEXT"},
    {lv_MSG_PARSE_DSL, "PARSE_DSL"},
    {lv_MSG_PARSE_FORMULA, "PARSE_FORMULA"},
    {lv_MSG_PARSE_RESULT, "PARSE_RESULT"},
    {lv_MSG_RESOURCE_ALLOC, "RESOURCE_ALLOC"},
    {lv_MSG_RESOURCE_FREE, "RESOURCE_FREE"},
    {lv_MSG_CONFIG_GET, "CONFIG_GET"},
    {lv_MSG_CONFIG_SET, "CONFIG_SET"},
    {lv_MSG_GRAPH_ADD_NODE, "GRAPH_ADD_NODE"},
    {lv_MSG_GRAPH_REMOVE_NODE, "GRAPH_REMOVE_NODE"},
    {lv_MSG_GRAPH_ADD_CONSTRAINT, "GRAPH_ADD_CONSTRAINT"},
    {lv_MSG_GRAPH_SERIALIZE, "GRAPH_SERIALIZE"},
    {lv_MSG_GRAPH_QUERY, "GRAPH_QUERY"},
    {lv_MSG_GRAPH_SOLVED, "GRAPH_SOLVED"},
    {lv_MSG_PROOF_ADD_STEP, "PROOF_ADD_STEP"},
    {lv_MSG_PROOF_REMOVE_STEP, "PROOF_REMOVE_STEP"},
    {lv_MSG_PROOF_QUERY, "PROOF_QUERY"},
    {lv_MSG_PROOF_COMPLETED, "PROOF_COMPLETED"},
    {lv_MSG_PROOF_FAILED, "PROOF_FAILED"},
    {lv_MSG_SOLVER_RUN, "SOLVER_RUN"},
    {lv_MSG_SOLVER_RESULT, "SOLVER_RESULT"},
    {lv_MSG_REWRITE_APPLY, "REWRITE_APPLY"},
    {lv_MSG_REWRITE_RESULT, "REWRITE_RESULT"},
    {lv_MSG_UNIFY_CHECK, "UNIFY_CHECK"},
    {lv_MSG_UNIFY_RESULT, "UNIFY_RESULT"},
    {lv_MSG_TYPE_CHECK, "TYPE_CHECK"},
    {lv_MSG_TYPE_RESULT, "TYPE_RESULT"},
    {lv_MSG_EXPORT_GEOJSON, "EXPORT_GEOJSON"},
    {lv_MSG_EXPORT_TIKZ, "EXPORT_TIKZ"},
    {lv_MSG_EXPORT_COQ, "EXPORT_COQ"},
    {lv_MSG_EXPORT_LEAN, "EXPORT_LEAN"},
    {lv_MSG_VISUAL_RENDER, "VISUAL_RENDER"},
    {lv_MSG_VISUAL_QUERY, "VISUAL_QUERY"},
    {lv_MSG_ORCHESTRATE, "ORCHESTRATE"},
    {lv_MSG_ORCHESTRATE_RESULT, "ORCHESTRATE_RESULT"},
    {lv_MSG_META_VERIFY, "META_VERIFY"},
    {lv_MSG_META_RESULT, "META_RESULT"},
    {lv_MSG_APP_INIT, "APP_INIT"},
    {lv_MSG_APP_SHUTDOWN, "APP_SHUTDOWN"},
    {lv_MSG_APP_QUERY, "APP_QUERY"},
    {lv_MSG_INTEROP_EXPORT, "INTEROP_EXPORT"},
    {lv_MSG_INTEROP_IMPORT, "INTEROP_IMPORT"},
    {lv_MSG_INTEROP_RESULT, "INTEROP_RESULT"},
};

static const char *msg_type_name(lvMsgType type) {
    const char *name = lv_msg_name_lookup(s_msg_type_name_entries, lv_ARRAY_SIZE(s_msg_type_name_entries), (int) type);
    return name ? name : "UNKNOWN";
}

/* ---- 生命周期 ---- */

lvLayerMessage *lv_msg_create(lvMsgType type, lvMsgDirection dir, int sender, int target) {
    lvLayerMessage *msg = (lvLayerMessage *) lv_calloc(1, sizeof(lvLayerMessage));
    if (!msg)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_msg_create: lv_calloc failed");

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
