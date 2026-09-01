/**
 * @file interop.h
 * @brief 外部互操作模块头文件
 *
 * @details 本模块实现与外部系统的互操作功能，包括：
 *          - WebSocket接口
 *          - stdio接口
 *          - 定理交换（Coq/Lean导出）
 *          - 数据导入（GeoGebra、GeoJSON）
 *          - 规范表示导出
 *
 * @author Lv-00 Project
 * @version 1.1.0
 */

#ifndef lv_INTEROP_H
#define lv_INTEROP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "constraint_graph.h"
#include "lv/cross_platform.h" /* lv_THREAD_LOCAL */
#include "proof.h"

/** @brief interop 模块全局流式上下文（由 interop_command.c 集中定义） */
extern lv_THREAD_LOCAL StreamContext *interop_stream_ctx;

/** @brief 设置 interop 模块的流式上下文 */
void interop_set_stream_context(StreamContext *ctx);

/* 前向声明 —— 避免引入 lv.h 的 16+ 传递依赖 */
struct lvEngine;
typedef struct lvEngine lvEngine;

/* ============================================================
 * 公共常量
 * ============================================================ */

/** 单个约束节点涉及的最大约束数量（统一管理，避免两个文件定义不一致） */
#define lv_MAX_CONSTRAINT_INDICES 64

/** 双精度转有理数的分母精度（10^6），interop 各模块统一使用 */
#ifndef INTEROP_COORD_DENOM_PRECISION
#define INTEROP_COORD_DENOM_PRECISION 1000000ULL
#endif

/** GeoJSON 坐标导入专用有理化分母精度（10^9；缩放因子与整数分母同源，避免 1e9 / 1000000000ULL 失步） */
#ifndef INTEROP_COORD_DENOM_PRECISION_GEOJSON
#define INTEROP_COORD_DENOM_PRECISION_GEOJSON 1000000000ULL
#endif


#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 常量定义 ==================== */

/** 命令缓冲区大小 */
#ifndef INTEROP_CMD_BUFFER_SIZE
#define INTEROP_CMD_BUFFER_SIZE 4096
#endif

/** 响应缓冲区大小 */
#ifndef INTEROP_RESP_BUFFER_SIZE
#define INTEROP_RESP_BUFFER_SIZE 65536
#endif

/** 最大命令参数数量 */
#define INTEROP_MAX_PARAMS 32

/** WebSocket默认端口 */
#define INTEROP_WS_DEFAULT_PORT 8765

/** 导出路径最大长度 */
#ifndef INTEROP_MAX_PATH_LEN
#define INTEROP_MAX_PATH_LEN 512
#endif

/* ==================== 类型定义 ==================== */

/**
 * @brief 互操作接口类型
 */
typedef enum {
    INTEROP_INTERFACE_STDIO = 0, /**< 标准输入输出接口 */
    INTEROP_INTERFACE_WEBSOCKET, /**< WebSocket接口 */
    INTEROP_INTERFACE_PIPE       /**< 管道接口 */
} InteropInterfaceType;

/**
 * @brief 导出格式类型
 */
typedef enum {
    INTEROP_EXPORT_COQ = 0,   /**< Coq格式 */
    INTEROP_EXPORT_LEAN,      /**< Lean格式 */
    INTEROP_EXPORT_HTML,      /**< 独立HTML */
    INTEROP_EXPORT_SVG,       /**< SVG矢量图 */
    INTEROP_EXPORT_PDF,       /**< PDF文档 */
    INTEROP_EXPORT_TIKZ,      /**< LaTeX TikZ */
    INTEROP_EXPORT_GEOJSON,   /**< GeoJSON格式 */
    INTEROP_EXPORT_CANONICAL, /**< 规范表示 */
    INTEROP_EXPORT_ISABELLE,  /**< Isabelle/HOL格式 */
    INTEROP_EXPORT_HOL_LIGHT  /**< HOL Light格式 */
} InteropExportFormat;

/**
 * @brief 导入格式类型
 */
typedef enum {
    INTEROP_IMPORT_GEOGEBRA = 0, /**< GeoGebra格式 */
    INTEROP_IMPORT_GEOJSON,      /**< GeoJSON格式 */
    INTEROP_IMPORT_SVG           /**< SVG格式 */
} InteropImportFormat;

/**
 * @brief 命令类型
 */
typedef enum {
    /* 节点操作 */
    INTEROP_CMD_ADD_NODE = 0, /**< 添加节点 */
    INTEROP_CMD_REMOVE_NODE,  /**< 删除节点 */
    INTEROP_CMD_GET_NODE,     /**< 获取节点信息 */

    /* 约束操作 */
    INTEROP_CMD_ADD_CONSTRAINT,    /**< 添加约束 */
    INTEROP_CMD_REMOVE_CONSTRAINT, /**< 删除约束 */
    INTEROP_CMD_GET_CONSTRAINT,    /**< 获取约束信息 */

    /* 函数块操作 */
    INTEROP_CMD_PACK_FUNCTION, /**< 打包函数 */
    INTEROP_CMD_INSTANTIATE,   /**< 实例化函数 */

    /* 求解与重写 */
    INTEROP_CMD_SOLVE,   /**< 求解 */
    INTEROP_CMD_REWRITE, /**< 重写 */
    INTEROP_CMD_UNIFY,   /**< 合一检查 */

    /* 查询 */
    INTEROP_CMD_GET_GRAPH,    /**< 获取完整图 */
    INTEROP_CMD_EXPORT_GRAPH, /**< 导出图 */
    INTEROP_CMD_GET_STATUS,   /**< 获取状态 */

    /* 系统 */
    INTEROP_CMD_PING,     /**< 心跳检测 */
    INTEROP_CMD_SHUTDOWN, /**< 关闭服务 */

    /* 流式输出 */
    INTEROP_CMD_STREAM_START,  /**< 启用流式输出 */
    INTEROP_CMD_STREAM_STOP,   /**< 禁用流式输出 */
    INTEROP_CMD_STREAM_FILTER, /**< 设置流式事件过滤 */
    INTEROP_CMD_STREAM_STATS,  /**< 获取流式事件统计 */
    INTEROP_CMD_STREAM_FLUSH   /**< 刷新异步队列 */
} InteropCommandType;

/**
 * @brief 命令结构
 */
typedef struct {
    InteropCommandType type;              /**< 命令类型 */
    char command_name[256];               /**< 原始命令名称（用于错误报告） */
    char params[INTEROP_MAX_PARAMS][256]; /**< 参数数组 */
    int param_count;                      /**< 参数数量 */
    int request_id;                       /**< 请求ID */
} InteropCommand;

/**
 * @brief 响应结构
 */
typedef struct {
    int request_id;                      /**< 对应请求ID */
    int status_code;                     /**< 状态码（0=成功） */
    char data[INTEROP_RESP_BUFFER_SIZE]; /**< 响应数据 */
    size_t data_len;                     /**< 数据长度 */
} InteropResponse;

/**
 * @brief 互操作服务器
 */
typedef struct {
    InteropInterfaceType type; /**< 接口类型 */
    int port;                  /**< 端口号（WebSocket） */
    bool running;              /**< 运行状态 */
    void *internal_data;       /**< 内部数据 */

    /* 引擎复用：首次命令时惰性创建 */
    void *persistent_engine; /**< lvEngine* 引擎复用 */
    int engine_in_use;       /**< 引擎使用计数 */

    /* 流式输出 */
    bool stream_enabled;         /**< 流式输出是否启用 */
    int stream_callback_id;      /**< 流式回调注册 ID（-1 表示未注册） */
    uint64_t stream_filter_mask; /**< 当前事件过滤掩码 */
    long stream_events_sent;     /**< 已发送的流式事件总数 */
} InteropServer;

typedef InteropServer lvInteropManager;

/**
 * @brief 外部证明系统类型
 */
typedef enum {
    lv_EXT_COQ,   /**< Coq 证明助手 */
    lv_EXT_LEAN4, /**< Lean 4 证明助手 */
    lv_EXT_JSON,  /**< JSON 格式 (OPML 等) */
    lv_EXT_COUNT
} lvExternalSystem;

/* 前向声明 */
typedef struct lvInteropPlugin lvInteropPlugin;

/**
 * @brief 互操作插件（外部证明系统桥接）
 */
struct lvInteropPlugin {
    char name[64];                              /**< 插件名称 */
    char version[32];                           /**< 版本号 */
    lvExternalSystem system;                    /**< 外部系统类型 */
    int (*export_proof)(void *, char *, int);   /**< 导出证明函数 */
    int (*import_proof)(const char *, void **); /**< 导入证明函数 */
    int (*validate)(const char *);              /**< 验证函数 */
};

/* 兼容宏：旧代码使用 lvPlugin，实际是 lvInteropPlugin */
#ifndef lv_PLUGIN_FULL_TYPE
typedef lvInteropPlugin lvPlugin;
#endif

/**
 * @brief 注册互操作插件
 * @param mgr 互操作管理器
 * @param plugin 插件信息
 * @return 成功返回0，失败返回-1
 */
int lv_interop_register_plugin(lvInteropManager *mgr, const lvPlugin *plugin);

/**
 * @brief 经插件注册表导出证明（插件体系分发入口）
 *
 * 按插件名在进程级插件表中查找并调用其 export_proof 回调。
 * 所有插件的 export_proof 统一接受 ProofNavigator*（navigator 语义），
 * 各插件内部转换为自己的证明表示（lvBridgeProof / lvOpmlProof）。
 *
 * @param[in]  plugin_name  插件名（"coq" / "lean4" / "opml"）
 * @param[in]  proof        ProofNavigator 指针
 * @param[out] output       输出缓冲区（写入证明器脚本/OPML JSON）
 * @param[in]  output_size  输出缓冲区大小（字节）
 * @return 成功返回 0；插件未找到返回 lv_ERROR_NOT_FOUND，其余返回对应负错误码
 */
int lv_interop_export_proof(const char *plugin_name, const ProofNavigator *proof, char *output, int output_size);

/**
 * @brief 注册 Coq 证明互操作插件（coq_bridge.c，tactic 集合为本地枚举豁免）
 * @param mgr 互操作管理器指针（非 NULL）
 * @return 成功返回 0，失败返回 -1
 */
int lv_register_coq_plugin(lvInteropManager *mgr);

/**
 * @brief 注册 Lean 4 证明互操作插件（lean4_bridge.c）
 * @param mgr 互操作管理器指针（非 NULL）
 * @return 成功返回 0，失败返回 -1
 */
int lv_register_lean4_plugin(lvInteropManager *mgr);

/**
 * @brief 注册 OPML 证明互操作插件（opml_codec.c）
 * @param mgr 互操作管理器指针（非 NULL）
 * @return 成功返回 0，失败返回 -1
 */
int lv_register_opml_plugin(lvInteropManager *mgr);

/**
 * @brief 重置互操作插件注册表（清空所有已注册插件）
 *
 * 插件注册表是进程级全局单例（固定容量，只增不减）。
 * 此函数用于测试隔离与进程复用场景：在服务器销毁或测试夹具
 * 清理时调用，使下一次注册重新从空表开始。
 *
 * @return 成功返回0
 */
int lv_interop_reset_plugins(void);

/**
 * @brief 导出配置
 */
typedef struct {
    InteropExportFormat format;             /**< 导出格式 */
    char output_path[INTEROP_MAX_PATH_LEN]; /**< 输出路径 */
    bool include_proofs;                    /**< 包含证明 */
    bool include_metadata;                  /**< 包含元数据 */
    bool pretty_print;                      /**< 美化输出 */
    int compression_level;                  /**< 压缩级别 */
} InteropExportConfig;

/**
 * @brief 导入配置
 */
typedef struct {
    InteropImportFormat format;            /**< 导入格式 */
    char input_path[INTEROP_MAX_PATH_LEN]; /**< 输入路径 */
    bool preserve_ids;                     /**< 保留原始ID */
    bool validate_geometry;                /**< 验证几何 */
} InteropImportConfig;

/**
 * @brief 定理交换上下文
 */
typedef struct {
    char trust_base_name[64];    /**< 信任基名称 */
    char trust_base_version[32]; /**< 信任基版本 */
    char *exported_calls;        /**< 导出的调用序列（NUL 结尾，calls_len 为有效长度） */
    size_t calls_len;            /**< 调用序列长度 */
    size_t calls_capacity;       /**< 调用序列缓冲区容量（含 NUL，lv_ensure_capacity 式倍增维护） */
} InteropTheoremContext;

/* ==================== 服务器管理 ==================== */

/**
 * @brief 创建互操作服务器
 * @param type 接口类型
 * @return 服务器指针，失败返回NULL
 */
InteropServer *interop_server_create(InteropInterfaceType type);

/**
 * @brief 销毁互操作服务器
 * @param server 服务器指针
 */
void interop_server_destroy(InteropServer *server);

/**
 * @brief 启动服务器
 * @param server 服务器指针
 * @param port 端口号（WebSocket）
 * @return 成功返回0，失败返回错误码
 */
int interop_server_start(InteropServer *server, int port);

/**
 * @brief 停止服务器
 * @param server 服务器指针
 * @return 成功返回0，失败返回错误码
 */
int interop_server_stop(InteropServer *server);

/**
 * @brief 处理单个命令（stdio模式）
 * @param server 服务器指针
 * @param input 输入命令字符串
 * @param output 输出缓冲区
 * @param output_size 缓冲区大小
 * @return 成功返回0，失败返回错误码
 */
int interop_server_process_command(InteropServer *server, const char *input, char *output, size_t output_size);

/**
 * @brief 运行服务器主循环（阻塞）
 * @param server 服务器指针
 * @return 成功返回0，失败返回错误码
 */
int interop_server_run(InteropServer *server);

/* ==================== 命令处理 ==================== */

/**
 * @brief 解析命令字符串
 * @param input 输入字符串
 * @param cmd 输出命令结构
 * @return 成功返回0，失败返回错误码
 */
int interop_parse_command(const char *input, InteropCommand *cmd);

/**
 * @brief 序列化响应为字符串
 * @param resp 响应结构
 * @param output 输出缓冲区
 * @param output_size 缓冲区大小
 * @return 成功返回0，失败返回错误码
 */
int interop_serialize_response(const InteropResponse *resp, char *output, size_t output_size);

/**
 * @brief 执行命令
 * @param engine 引擎
 * @param cmd 命令
 * @param resp 输出响应
 * @return 成功返回0，失败返回错误码
 */
int interop_execute_command(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp);

/* ==================== 导出功能 ==================== */

/**
 * @brief 导出证明到Coq格式
 * @param proof 证明
 * @param config 导出配置
 * @return 成功返回0，失败返回错误码
 */
int interop_export_coq(const ProofNavigator *proof, const InteropExportConfig *config);

/**
 * @brief 导出证明到Lean格式
 * @param proof 证明
 * @param config 导出配置
 * @return 成功返回0，失败返回错误码
 */
int interop_export_lean(const ProofNavigator *proof, const InteropExportConfig *config);

/**
 * @brief 导出为独立HTML演示包
 * @param engine 引擎
 * @param config 导出配置
 * @return 成功返回0，失败返回错误码
 */
int interop_export_html(const lvEngine *engine, const InteropExportConfig *config);

/**
 * @brief 导出为SVG矢量图
 * @param graph 约束图
 * @param config 导出配置
 * @return 成功返回0，失败返回错误码
 */
int interop_export_svg(const ConstraintGraph *graph, const InteropExportConfig *config);

/**
 * @brief 导出规范表示
 * @param graph 约束图
 * @param output_path 输出路径
 * @return 成功返回0，失败返回错误码
 */
int interop_export_canonical(const ConstraintGraph *graph, const char *output_path);

/**
 * @brief 导出为GeoJSON格式
 * @param graph 约束图
 * @param config 导出配置
 * @return 成功返回0，失败返回错误码
 */
int interop_export_geojson(const ConstraintGraph *graph, const InteropExportConfig *config);

/**
 * @brief 从 ProofNavigator 导出 OPML JSON（上层导出入口）
 *
 * 遍历导航器证明步骤构造 OPML 内部表示并序列化为兼容 JSON。
 * 实现于 layer10_interop/opml_codec.c。
 *
 * @param proof       证明导航器（NULL 时返回 INVALID_PARAM）
 * @param output      目标缓冲区
 * @param output_size 目标缓冲区大小
 * @return 成功返回 0，失败返回负错误码（lv_ERROR_*）
 */
int lv_opml_export_navigator(const ProofNavigator *proof, char *output, int output_size);

/* ==================== 导入功能 ==================== */

/**
 * @brief 从GeoGebra导入
 * @param engine 引擎
 * @param config 导入配置
 * @return 成功返回0，失败返回错误码
 */
int interop_import_geogebra(lvEngine *engine, const InteropImportConfig *config);

/**
 * @brief 从GeoJSON导入
 * @param engine 引擎
 * @param config 导入配置
 * @return 成功返回0，失败返回错误码
 */
int interop_import_geojson(lvEngine *engine, const InteropImportConfig *config);

/**
 * @brief 从SVG导入
 * @param engine 引擎
 * @param config 导入配置
 * @return 成功返回0，失败返回错误码
 */
int interop_import_svg(lvEngine *engine, const InteropImportConfig *config);

/* ==================== 定理交换 ==================== */

/**
 * @brief 创建定理交换上下文
 * @param trust_base_name 信任基名称
 * @param trust_base_version 信任基版本
 * @return 上下文指针，失败返回NULL
 */
InteropTheoremContext *interop_theorem_context_create(const char *trust_base_name, const char *trust_base_version);

/**
 * @brief 销毁定理交换上下文
 * @param ctx 上下文指针
 */
void interop_theorem_context_destroy(InteropTheoremContext *ctx);

/**
 * @brief 添加定理调用到上下文
 * @param ctx 上下文
 * @param theorem_name 定理名称
 * @param params 参数数组
 * @param param_count 参数数量
 * @return 成功返回0，失败返回错误码
 */
int interop_theorem_add_call(InteropTheoremContext *ctx, const char *theorem_name, const char **params,
                             int param_count);

/**
 * @brief 导出定理调用序列
 * @param ctx 上下文
 * @param format 目标格式（coq/lean）
 * @param output 输出缓冲区
 * @param output_size 缓冲区大小
 * @return 成功返回0，失败返回错误码
 */
int interop_theorem_export_calls(const InteropTheoremContext *ctx, InteropExportFormat format, char *output,
                                 size_t output_size);

/**
 * @brief 导入外部定理为可信基块
 * @param engine 引擎
 * @param trust_base_name 信任基名称
 * @param content_hash 内容哈希
 * @param description 描述
 * @param block_id 输出块ID
 * @return 成功返回0，失败返回错误码
 */
int interop_import_external_theorem(lvEngine *engine, const char *trust_base_name, const char *content_hash,
                                    const char *description, int *block_id);

/* ==================== 工具函数 ==================== */

/**
 * @brief 获取导出格式名称
 * @param format 导出格式
 * @return 格式名称字符串
 */
const char *interop_export_format_name(InteropExportFormat format);

/**
 * @brief 获取导入格式名称
 * @param format 导入格式
 * @return 格式名称字符串
 */
const char *interop_import_format_name(InteropImportFormat format);

/**
 * @brief 从字符串解析导出格式
 * @param str 格式字符串
 * @return 导出格式，无效返回-1
 */
InteropExportFormat interop_parse_export_format(const char *str);

/**
 * @brief 从字符串解析导入格式
 * @param str 格式字符串
 * @return 导入格式，无效返回-1
 */
InteropImportFormat interop_parse_import_format(const char *str);

/**
 * @brief 验证文件路径
 * @param path 文件路径
 * @return 有效返回1，无效返回0
 */
int interop_validate_path(const char *path);

/**
 * @brief 获取文件扩展名
 * @param path 文件路径
 * @return 扩展名字符串（不含点）
 */
const char *interop_get_file_extension(const char *path);

/* ==================== 命令补全 ==================== */

#define INTEROP_MAX_COMPLETIONS 64

/**
 * @brief 获取命令补全建议
 *
 * 根据当前引擎状态和已输入的前缀，返回匹配的命令补全字符串列表。
 * 补全建议包括：内置命令（add point / add segment / normalize 等）、
 * 当前图中已存在的节点名称、约束名称以及所有支持的子命令。
 *
 * 调用者负责释放返回的字符串数组（使用 interop_free_completions）。
 *
 * @param[in]  engine   引擎句柄
 * @param[in]  prefix   已输入的命令前缀
 * @param[out] out_count 输出匹配的补全数量
 * @return 补全字符串数组（NULL 表示无匹配或错误），由调用者释放
 */
char **interop_get_command_completions(lvEngine *engine, const char *prefix, int *out_count);

/**
 * @brief 释放命令补全结果
 *
 * @param[in] completions 补全字符串数组
 * @param[in] count       补全数量
 */
void interop_free_completions(char **completions, int count);

/* ==================== 内部共享函数 ==================== */

/** @brief 获取信任颜色对应的 SVG 颜色字符串 */
const char *interop_trust_color_to_svg(TrustColor trust);
/** @brief 获取信任颜色对应的 TikZ 颜色字符串 */
const char *interop_trust_color_to_tikz(TrustColor trust);
/** @brief 获取几何类型名称字符串 */
const char *interop_geom_type_name(GeomType type);
/** @brief 获取约束类型名称字符串 */
const char *interop_constraint_type_name(ConstraintType type);
/** @brief 流式输出回调（供 interop_command.c 使用） */
void interop_stream_callback(const struct StreamEvent *event, void *user_data);

/* 兼容宏 */
#define trust_color_to_svg(trust) interop_trust_color_to_svg(trust)
#define trust_color_to_tikz(trust) interop_trust_color_to_tikz(trust)
#define geom_type_name(type) interop_geom_type_name(type)
#define constraint_type_name(type) interop_constraint_type_name(type)

#ifdef __cplusplus
}
#endif

#endif /* lv_INTEROP_H */
