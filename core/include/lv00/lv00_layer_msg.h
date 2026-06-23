/**
 * @file lv00_layer_msg.h
 * @brief 层间通信消息格式
 *
 * @details 定义跨层调用的统一消息结构体 Lv00LayerMessage。
 *          每层对外只暴露一个入口函数:
 *            Lv00Result lv00_lN_handle_message(Lv00LayerMessage *msg);
 *
 * @author Lv-00 Project
 * @version 3.0.0
 */
#ifndef LV00_LAYER_MSG_H
#define LV00_LAYER_MSG_H

#include <stddef.h>
#include <stdint.h>
#include "lv00_api_spec.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 消息方向
 * ============================================================ */

typedef enum {
    LV00_MSG_UP = 0,     /**< 低层→高层（结果报告/事件通知） */
    LV00_MSG_DOWN = 1,   /**< 高层→低层（指令下发/配置注入） */
} Lv00MsgDirection;

/* ============================================================
 * 消息类型枚举（按层级分组编号）
 * ============================================================ */

typedef enum {
    /* ---- 通用消息 (0-99) ---- */
    LV00_MSG_NONE = 0,
    LV00_MSG_ERROR = 1,
    LV00_MSG_HEARTBEAT = 99,

    /* ---- Layer 1: 解析层 (100-199) ---- */
    LV00_MSG_PARSE_TEXT = 100,
    LV00_MSG_PARSE_DSL = 101,
    LV00_MSG_PARSE_FORMULA = 102,
    LV00_MSG_PARSE_RESULT = 150,

    /* ---- Layer 2: 资源层 (200-299) ---- */
    LV00_MSG_RESOURCE_ALLOC = 200,
    LV00_MSG_RESOURCE_FREE = 201,
    LV00_MSG_CONFIG_GET = 202,
    LV00_MSG_CONFIG_SET = 203,

    /* ---- Layer 3: 几何层 (300-399) ---- */
    LV00_MSG_GRAPH_ADD_NODE = 300,
    LV00_MSG_GRAPH_REMOVE_NODE = 301,
    LV00_MSG_GRAPH_ADD_CONSTRAINT = 302,
    LV00_MSG_GRAPH_SERIALIZE = 303,
    LV00_MSG_GRAPH_QUERY = 304,
    LV00_MSG_GRAPH_SOLVED = 350,

    /* ---- Layer 4: 推理层 (400-499) ---- */
    LV00_MSG_PROOF_ADD_STEP = 400,
    LV00_MSG_PROOF_REMOVE_STEP = 401,
    LV00_MSG_PROOF_QUERY = 402,
    LV00_MSG_PROOF_COMPLETED = 450,
    LV00_MSG_PROOF_FAILED = 451,

    LV00_MSG_SOLVER_RUN = 460,
    LV00_MSG_SOLVER_RESULT = 461,

    LV00_MSG_REWRITE_APPLY = 470,
    LV00_MSG_REWRITE_RESULT = 471,

    LV00_MSG_UNIFY_CHECK = 480,
    LV00_MSG_UNIFY_RESULT = 481,

    LV00_MSG_TYPE_CHECK = 490,
    LV00_MSG_TYPE_RESULT = 491,

    /* ---- Layer 5: 输出层 (500-599) ---- */
    LV00_MSG_EXPORT_GEOJSON = 500,
    LV00_MSG_EXPORT_TIKZ = 501,
    LV00_MSG_EXPORT_COQ = 502,
    LV00_MSG_EXPORT_LEAN = 503,

    /* ---- Layer 6: 可视层 (600-699) ---- */
    LV00_MSG_VISUAL_RENDER = 600,
    LV00_MSG_VISUAL_QUERY = 601,

    /* ---- Layer 7: 编排层 (700-799) ---- */
    LV00_MSG_ORCHESTRATE = 700,
    LV00_MSG_ORCHESTRATE_RESULT = 701,

    /* ---- Layer 8: 验证层 (800-899) ---- */
    LV00_MSG_META_VERIFY = 800,
    LV00_MSG_META_RESULT = 801,

    /* ---- Layer 9: 应用层 (900-999) ---- */
    LV00_MSG_APP_INIT = 900,
    LV00_MSG_APP_SHUTDOWN = 901,
    LV00_MSG_APP_QUERY = 902,

    /* ---- Layer 10: 外部集成层 (1000-1099) ---- */
    LV00_MSG_INTEROP_EXPORT = 1000,
    LV00_MSG_INTEROP_IMPORT = 1001,
    LV00_MSG_INTEROP_RESULT = 1002,
} Lv00MsgType;

/* ============================================================
 * 消息体
 * ============================================================ */

#define LV00_MSG_NAME_LEN 64
#define LV00_MSG_DESC_LEN 256
#define LV00_MSG_JSON_LEN 4096

typedef struct {
    /* ---- 路由信息 ---- */
    Lv00MsgType      type;
    Lv00MsgDirection direction;
    int              sender_layer;
    int              target_layer;
    uint32_t         msg_id;          /**< 单调递增消息 ID */
    uint32_t         reply_to;        /**< 回复哪个 msg_id（0 = 新消息） */

    /* ---- 元数据 ---- */
    char name[LV00_MSG_NAME_LEN];     /**< 人类可读消息名（调试用） */
    char description[LV00_MSG_DESC_LEN]; /**< 消息描述 */

    /* ---- 载荷 ---- */
    void            *payload;          /**< 类型化载荷（由 type 决定类型） */
    size_t           payload_size;     /**< 载荷字节数（0 表示无效） */

    /* ---- JSON 缓冲（可选，用于序列化场景） ---- */
    char json_buf[LV00_MSG_JSON_LEN];

    /* ---- 错误信息 ---- */
    int error_code;                    /**< 0 = 成功，非零参考 Lv00ErrorCode */
    char error_ctx[256];              /**< 错误上下文描述 */
} Lv00LayerMessage;

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
LV00_API Lv00LayerMessage *lv00_msg_create(Lv00MsgType type, Lv00MsgDirection dir,
                                  int sender, int target);

/**
 * @brief 销毁消息并释放 payload（如果 payload 非空）
 * @param msg 消息指针
 */
LV00_API void lv00_msg_destroy(Lv00LayerMessage *msg);

/**
 * @brief 对消息签名（填充 msg_id 等元数据）
 *
 * 调用后 msg_id 被写入一个全局递增的序号，
 * 同时将 type 的字符串表示写入 name 字段。
 *
 * @param msg 待签名的消息
 */
LV00_API void lv00_msg_sign(Lv00LayerMessage *msg);

#ifdef __cplusplus
}
#endif
#endif /* LV00_LAYER_MSG_H */
