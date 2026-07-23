/**
 * @file lv_layer_msg.h
 * @brief 层间通信消息格式
 *
 * @details 定义跨层调用的统一消息结构体 lvLayerMessage。
 *          每层对外只暴露一个入口函数:
 *            lvResult lv_lN_handle_message(lvLayerMessage *msg);
 *
 * @author Lv-00 Project
 * @version 1.1.0
 */
#ifndef lv_LAYER_MSG_H
#define lv_LAYER_MSG_H

#include <stddef.h>
#include <stdint.h>

#include "lv_api_spec.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 消息方向
 * ============================================================ */

typedef enum {
    lv_MSG_UP = 0,   /**< 低层→高层（结果报告/事件通知） */
    lv_MSG_DOWN = 1, /**< 高层→低层（指令下发/配置注入） */
} lvMsgDirection;

/* ============================================================
 * 消息类型枚举（按层级分组编号）
 * ============================================================ */

typedef enum {
    /* ---- 通用消息 (0-99) ---- */
    lv_MSG_NONE = 0,
    lv_MSG_ERROR = 1,
    lv_MSG_HEARTBEAT = 99,

    /* ---- Layer 1: 解析层 (100-199) ---- */
    lv_MSG_PARSE_TEXT = 100,
    lv_MSG_PARSE_DSL = 101,
    lv_MSG_PARSE_FORMULA = 102,
    lv_MSG_PARSE_RESULT = 150,

    /* ---- Layer 2: 资源层 (200-299) ---- */
    lv_MSG_RESOURCE_ALLOC = 200,
    lv_MSG_RESOURCE_FREE = 201,
    lv_MSG_CONFIG_GET = 202,
    lv_MSG_CONFIG_SET = 203,

    /* ---- Layer 3: 几何层 (300-399) ---- */
    lv_MSG_GRAPH_ADD_NODE = 300,
    lv_MSG_GRAPH_REMOVE_NODE = 301,
    lv_MSG_GRAPH_ADD_CONSTRAINT = 302,
    lv_MSG_GRAPH_SERIALIZE = 303,
    lv_MSG_GRAPH_QUERY = 304,
    lv_MSG_GRAPH_SOLVED = 350,

    /* ---- Layer 4: 推理层 (400-499) ---- */
    lv_MSG_PROOF_ADD_STEP = 400,
    lv_MSG_PROOF_REMOVE_STEP = 401,
    lv_MSG_PROOF_QUERY = 402,
    lv_MSG_PROOF_COMPLETED = 450,
    lv_MSG_PROOF_FAILED = 451,

    lv_MSG_SOLVER_RUN = 460,
    lv_MSG_SOLVER_RESULT = 461,

    lv_MSG_REWRITE_APPLY = 470,
    lv_MSG_REWRITE_RESULT = 471,

    lv_MSG_UNIFY_CHECK = 480,
    lv_MSG_UNIFY_RESULT = 481,

    lv_MSG_TYPE_CHECK = 490,
    lv_MSG_TYPE_RESULT = 491,

    /* ---- Layer 5: 输出层 (500-599) ---- */
    lv_MSG_EXPORT_GEOJSON = 500,
    lv_MSG_EXPORT_TIKZ = 501,
    lv_MSG_EXPORT_COQ = 502,
    lv_MSG_EXPORT_LEAN = 503,

    /* ---- Layer 6: 可视层 (600-699) ---- */
    lv_MSG_VISUAL_RENDER = 600,
    lv_MSG_VISUAL_QUERY = 601,

    /* ---- Layer 7: 编排层 (700-799) ---- */
    lv_MSG_ORCHESTRATE = 700,
    lv_MSG_ORCHESTRATE_RESULT = 701,

    /* ---- Layer 8: 验证层 (800-899) ---- */
    lv_MSG_META_VERIFY = 800,
    lv_MSG_META_RESULT = 801,

    /* ---- Layer 9: 应用层 (900-999) ---- */
    lv_MSG_APP_INIT = 900,
    lv_MSG_APP_SHUTDOWN = 901,
    lv_MSG_APP_QUERY = 902,

    /* ---- Layer 10: 外部集成层 (1000-1099) ---- */
    lv_MSG_INTEROP_EXPORT = 1000,
    lv_MSG_INTEROP_IMPORT = 1001,
    lv_MSG_INTEROP_RESULT = 1002,
} lvMsgType;

/* ============================================================
 * 消息体
 * ============================================================ */

#define lv_MSG_NAME_LEN 64
#define lv_MSG_DESC_LEN 256
#define lv_MSG_JSON_LEN 4096

typedef struct {
    /* ---- 路由信息 ---- */
    lvMsgType type;
    lvMsgDirection direction;
    int sender_layer;
    int target_layer;
    uint32_t msg_id;   /**< 单调递增消息 ID */
    uint32_t reply_to; /**< 回复哪个 msg_id（0 = 新消息） */

    /* ---- 元数据 ---- */
    char name[lv_MSG_NAME_LEN];        /**< 人类可读消息名（调试用） */
    char description[lv_MSG_DESC_LEN]; /**< 消息描述 */

    /* ---- 载荷 ---- */
    void *payload;       /**< 类型化载荷（由 type 决定类型） */
    size_t payload_size; /**< 载荷字节数（0 表示无效） */

    /* ---- JSON 缓冲（可选，用于序列化场景） ---- */
    char json_buf[lv_MSG_JSON_LEN];

    /* ---- 错误信息 ---- */
    int error_code;      /**< 0 = 成功，非零参考 lvErrorCode */
    char error_ctx[256]; /**< 错误上下文描述 */
} lvLayerMessage;

/* ============================================================
 * 消息生命周期
 * ============================================================ */

/**
 * @brief 创建一条空消息
 * @param type    消息类型
 * @param dir     消息方向
 * @param sender  发送者层号
 * @param target  目标层号
 * @return 堆分配的消息，失败返回 NULL
 */
lv_API lvLayerMessage *lv_msg_create(lvMsgType type, lvMsgDirection dir, int sender, int target);

/**
 * @brief 销毁消息并释放 payload（如果 payload 非空）
 * @param msg 消息指针
 */
lv_API void lv_msg_destroy(lvLayerMessage *msg);

/**
 * @brief 对消息签名（填充 msg_id 等元数据）
 *
 * 调用后 msg_id 被写入一个全局递增的序号，
 * 同时将 type 的字符串表示写入 name 字段。
 *
 * @param msg 待签名的消息
 */
lv_API void lv_msg_sign(lvLayerMessage *msg);

#ifdef __cplusplus
}
#endif
#endif /* lv_LAYER_MSG_H */
