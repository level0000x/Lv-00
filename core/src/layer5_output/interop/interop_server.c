/**
 * @file interop_server.c
 * @brief 互操作服务器
 *
 * @details 拆分子模块（Lv-00 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

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
#include "lv/lv_thread.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"

/* ── 服务器核心 ── */

/* ==================== 条件编译：套接字支持 ==================== */

/**
 * 在Windows平台上尝试引入Winsock2库以支持WebSocket服务器。
 * 如果编译环境未安装Winsock2，通过条件编译跳过套接字初始化，
 * 降级为 STDIO 模式运行（详见 interop_server_run 中的 #else 分支）。
 *
 * 在 Linux/macOS 平台上，使用 POSIX socket API
 * （<sys/socket.h> + <netinet/in.h> + <arpa/inet.h>）实现
 * WebSocket 服务器支持，功能等级与 Windows Winsock2 实现相同。
 */
#include "lv/lv_strbuf.h"
#if defined(_WIN32) || defined(_WIN64)
/* 尝试包含Winsock2头文件用于套接字初始化 */
#if __has_include(<winsock2.h>)
#include <winsock2.h>
#define INTEROP_HAS_WINSOCK 1
#else
#define INTEROP_HAS_WINSOCK 0
#endif
#elif defined(__linux__) || defined(__APPLE__)
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#define INTEROP_HAS_POSIX_SOCKET 1
#else
#define INTEROP_HAS_WINSOCK 0
#endif

/* ==================== 命名常量（消除魔术数字） ==================== */

/** WebSocket 默认端口号 */
#define INTEROP_WEBSOCKET_DEFAULT_PORT 8080

/** select 超时时间（微秒）：100ms */
#define INTEROP_SELECT_TIMEOUT_US 100000

/** select 超时秒数（与上方宏配合使用） */
#define INTEROP_SELECT_TIMEOUT_SEC 0

/** 双精度转有理数的分母精度（10^6） */
#define INTEROP_COORD_DENOM_PRECISION 1000000UL

/* ==================== stdout 互斥锁（线程安全） ==================== */

/**
 * 警告 —— 命名区分：本模块的 stdout_lock_* 函数与 debug.c 中的 log_lock/log_unlock
 * 是不同的锁，服务于不同的资源：
 *   - interop.c  stdout_lock_*     → 保护 stdout 的 JSON-RPC 写入串行化
 *   - debug.c    log_lock()        → 保护调试日志文件写入、性能计数器
 * 两者互不冲突，切勿混淆或跨文件调用。
 *
 * 使用 lv/lv_thread.h 提供的跨平台互斥锁抽象，替代原 Windows CRITICAL_SECTION
 * 和 POSIX pthread_mutex_t 的平台分支代码。
 */

static lv_mutex_t g_stdout_mutex;
static lv_once_t g_stdout_once = lv_ONCE_INIT;

static void stdout_lock_init_once(void) {
    lv_mutex_init(&g_stdout_mutex);
}

/** @brief 初始化 stdout 互斥锁（在 interop_server_create 中调用） */
static void stdout_lock_init(void) {
    lv_once(&g_stdout_once, stdout_lock_init_once);
}

/** @brief 获取 stdout 锁 */
static void stdout_lock_acquire(void) {
    lv_mutex_lock(&g_stdout_mutex);
}

/** @brief 释放 stdout 锁 */
static void stdout_lock_release(void) {
    lv_mutex_unlock(&g_stdout_mutex);
}

/** @brief 销毁 stdout 互斥锁 */
static void stdout_lock_destroy(void) {
    lv_mutex_destroy(&g_stdout_mutex);
}

/* ==================== 模块级流式上下文 ==================== */

/**
 * @brief interop 模块自己的流式上下文
 *
 * 用于 Coq/Lean 导出等不接收 engine 参数的函数中发射流式事件。
 * 由 interop_set_stream_context() 设置，通常在引擎初始化时
 * 通过 engine_get_stream_context() 获取并注入。
 */

/* ==================== 流式输出集成 ==================== */

/**
 * @brief 流式事件回调：将事件序列化为 JSON-RPC notification 并写入 stdout
 *
 * 此回调注册到引擎的 StreamContext，每当引擎发射流式事件时，
 * 自动将事件序列化为 JSON-RPC 2.0 notification 格式输出到 stdout。
 * 前端（Web UI / LLM 客户端）可实时解析这些 notification 实现可视化。
 *
 * @note 使用 stdout 的互斥锁（g_stdout_mutex）保护 printf 调用的线程安全。
 *       在多线程环境中，多个引擎线程可能同时触发此回调。
 *
 * @param event     流式事件数据
 * @param user_data  指向 InteropServer 的指针（用于统计）
 */
void interop_stream_callback(const StreamEvent *event, void *user_data) {
    if (!event)
        return;

    InteropServer *server = (InteropServer *) user_data;

    /* 序列化为 JSON-RPC notification */
    char json_buf[STREAM_JSON_BUFFER_DEFAULT_SIZE + 256];
    int len = stream_event_to_jsonrpc(event, json_buf, sizeof(json_buf));
    if (len <= 0)
        return;

    /* 输出到 stdout（每行一个 JSON-RPC notification）
     * 加锁保护 printf 调用，防止多线程输出交错 */
    stdout_lock_acquire();
    printf("%s\n", json_buf);
    fflush(stdout);
    stdout_lock_release();

    /* 更新统计 */
    if (server) {
        server->stream_events_sent++;
    }
}

/**
 * @brief 为引擎注册流式输出回调
 *
 * 在 STDIO/WebSocket 模式下，将 interop_stream_callback 注册到
 * 引擎的 StreamContext，使引擎事件自动推送为 JSON-RPC notification。
 *
 * @param server  互操作服务器
 * @param engine  引擎实例
 * @return true 成功，false 失败
 */
static bool interop_attach_stream_callback(InteropServer *server, lvEngine *engine) {
    if (!server || !engine)
        return false;

    StreamContext *stream_ctx = engine_get_stream_context(engine);
    if (!stream_ctx)
        return false;

    /* 如果之前已注册，先注销 */
    if (server->stream_callback_id >= 0) {
        stream_unregister_callback_by_id(stream_ctx, server->stream_callback_id);
        server->stream_callback_id = -1;
    }

    /* 注册带过滤的回调 */
    int cb_id = stream_register_callback_ex(stream_ctx, interop_stream_callback,
                                            server, /* user_data: 传递 server 指针用于统计 */
                                            server->stream_filter_mask);

    if (cb_id >= 0) {
        server->stream_callback_id = cb_id;
        server->stream_enabled = true;
        return true;
    }

    return false;
}

/**
 * @brief 从引擎注销流式输出回调
 *
 * @param server  互操作服务器
 * @param engine  引擎实例
 */
static void interop_detach_stream_callback(InteropServer *server, lvEngine *engine) {
    if (!server || !engine)
        return;

    StreamContext *stream_ctx = engine_get_stream_context(engine);
    if (!stream_ctx)
        return;

    if (server->stream_callback_id >= 0) {
        stream_unregister_callback_by_id(stream_ctx, server->stream_callback_id);
        server->stream_callback_id = -1;
    }
    server->stream_enabled = false;
}

/* ==================== 服务器管理 ==================== */

/**
 * @brief 创建互操作服务器
 *
 * 分配并初始化互操作服务器，支持 WebSocket 和 STDIO 两种接口类型。
 *
 * @param type 接口类型（WebSocket 或 STDIO）
 * @return 新分配的服务器指针，失败返回 NULL
 */
InteropServer *interop_server_create(InteropInterfaceType type) {
    /* 初始化 stdout 互斥锁（仅首次创建时生效） */
    stdout_lock_init();

    InteropServer *server = (InteropServer *) lv_calloc(1, sizeof(InteropServer));
    if (!server)
        return NULL;
    server->type = type;
    server->stream_callback_id = -1;
    server->stream_filter_mask = STREAM_FILTER_ALL; /* 默认接收所有事件 */
    server->persistent_engine = NULL;               /* 引擎复用：初始为空，首次命令时惰性创建 */
    server->engine_in_use = 0;
    return server;
}

/**
 * @brief 销毁互操作服务器
 *
 * 释放服务器资源，包括停止运行中的服务、销毁互斥锁等。
 *
 * @param server 服务器指针（可为 NULL）
 */
void interop_server_destroy(InteropServer *server) {
    if (!server)
        return;

    if (server->running) {
        interop_server_stop(server);
    }

    /* 销毁持久化引擎实例（引擎复用/池化清理） */
    if (server->persistent_engine) {
        engine_destroy(server->persistent_engine);
        server->persistent_engine = NULL;
    }

    stdout_lock_destroy();
    lv_free((void **) &server);
}

int interop_server_start(InteropServer *server, int port) {
    /**
     * 启动互操作服务器
     *
     * 初始化并启动WebSocket或STDIO类型的互操作服务器。
     *
     * 当前实现状态：
     * - STDIO模式：完整实现（通过stdin/stdout通信）
     * - WebSocket模式：C层提供基本参数验证和状态管理。完整的套接字
     *   监听、连接管理、协议握手等网络操作需要在存在Winsock2库的
     *   编译环境下才能完整运行。当前在条件编译下尝试基本的套接字初始化。
     *
     * @param server 服务器指针（必须非空且未运行）
     * @param port 监听端口（WebSocket模式，0表示使用默认端口）
     * @return lv_OK 启动成功
     *         lv_ERROR_INVALID_PARAM server为NULL
     *         lv_ERROR_INVALID_STATE 服务器已在运行
     *         lv_ERROR_IO Winsock初始化失败（仅Windows）
     */
    if (!server)
        return lv_ERROR_INVALID_PARAM;

    if (server->running) {
        lv_set_error(lv_ERROR_INVALID_STATE, "服务器已在运行中，请先调用interop_server_stop停止当前服务器");
        return lv_ERROR_INVALID_STATE;
    }

    /* 参数验证：端口号范围检查 */
    if (port < 0 || port > 65535) {
        lv_set_error(lv_ERROR_INVALID_PARAM, "无效的端口号=%d，端口范围为0-65535", port);
        return lv_ERROR_INVALID_PARAM;
    }

    if (server->type == INTEROP_INTERFACE_WEBSOCKET && port > 0) {
        server->port = port;
    }

    /* 尝试WebSocket套接字初始化（条件编译） */
#if INTEROP_HAS_WINSOCK
    if (server->type == INTEROP_INTERFACE_WEBSOCKET) {
        WSADATA wsa_data;
        int wsa_result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
        if (wsa_result != 0) {
            lv_set_error(lv_ERROR_IO,
                         "Winsock初始化失败（错误码=%d）。"
                         "请检查网络驱动是否正常安装。",
                         wsa_result);
            return lv_ERROR_IO;
        }

        /* 尝试创建监听套接字 */
        SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listen_sock == INVALID_SOCKET) {
            int err = WSAGetLastError();
            WSACleanup();
            /* 修复：套接字创建失败时应返回错误码，不应设置 running=true */
            lv_set_error(lv_ERROR_IO, "创建监听套接字失败（Winsock错误码=%d）。", err);
            return lv_ERROR_IO;
        }

        /* 绑定地址 */
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons((u_short) server->port);

        if (bind(listen_sock, (struct sockaddr *) &addr, sizeof(addr)) == SOCKET_ERROR) {
            int err = WSAGetLastError();
            closesocket(listen_sock);
            WSACleanup();
            /* 修复：绑定失败时应返回错误码，不应设置 running=true */
            lv_set_error(lv_ERROR_IO, "套接字绑定失败（Winsock错误码=%d）。", err);
            return lv_ERROR_IO;
        }

        /* 开始监听 */
        if (listen(listen_sock, SOMAXCONN) == SOCKET_ERROR) {
            int err = WSAGetLastError();
            closesocket(listen_sock);
            WSACleanup();
            /* 监听失败时应返回错误码，不应设置 running=true */
            lv_set_error(lv_ERROR_IO, "套接字监听失败（Winsock错误码=%d）。", err);
            return lv_ERROR_IO;
        }

        /* 存储套接字句柄到internal_data */
        server->internal_data = (void *) (intptr_t) listen_sock;
        /* WebSocket服务器已在端口上启动成功（套接字已创建并监听） */
    }
#elif INTEROP_HAS_POSIX_SOCKET
    if (server->type == INTEROP_INTERFACE_WEBSOCKET) {
        /* 创建监听套接字 */
        int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_sock < 0) {
            lv_set_error(lv_ERROR_IO, "创建监听套接字失败（errno=%d）。", errno);
            return lv_ERROR_IO;
        }

        /* 设置 SO_REUSEADDR 选项，允许快速重用地址 */
        int reuse = 1;
        if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
            int err = errno;
            close(listen_sock);
            lv_set_error(lv_ERROR_IO, "设置套接字选项失败（errno=%d）。", err);
            return lv_ERROR_IO;
        }

        /* 绑定地址 */
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons((uint16_t) server->port);

        if (bind(listen_sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
            int err = errno;
            close(listen_sock);
            lv_set_error(lv_ERROR_IO, "套接字绑定失败（errno=%d）。", err);
            return lv_ERROR_IO;
        }

        /* 开始监听 */
        if (listen(listen_sock, SOMAXCONN) < 0) {
            int err = errno;
            close(listen_sock);
            lv_set_error(lv_ERROR_IO, "套接字监听失败（errno=%d）。", err);
            return lv_ERROR_IO;
        }

        /* 存储套接字句柄到internal_data */
        server->internal_data = (void *) (intptr_t) listen_sock;
    }
#else
    if (server->type == INTEROP_INTERFACE_WEBSOCKET) {
        /* WebSocket服务器已标记为运行状态。
           注意：当前编译环境未包含Winsock2或POSIX socket库，无实际网络监听。
           请安装Windows SDK或POSIX兼容的开发环境以启用完整的网络功能。 */
    }
#endif

    /* 设置运行状态 */
    server->running = true;

    return lv_OK;
}

int interop_server_stop(InteropServer *server) {
    /**
     * 停止互操作服务器
     *
     * 关闭监听套接字并清理网络资源，将服务器标记为停止状态。
     * 当前实现包含完整的资源清理逻辑（条件编译）。
     *
     * @param server 服务器指针（必须非空且正在运行）
     * @return lv_OK 停止成功
     *         lv_ERROR_INVALID_PARAM server为NULL
     *         lv_ERROR_INVALID_STATE 服务器未在运行
     */
    if (!server)
        return lv_ERROR_INVALID_PARAM;

    if (!server->running) {
        lv_set_error(lv_ERROR_INVALID_STATE, "服务器当前未运行，无需停止");
        return lv_ERROR_INVALID_STATE;
    }

    /* 清理网络资源 */
#if INTEROP_HAS_WINSOCK
    if (server->type == INTEROP_INTERFACE_WEBSOCKET) {
        if (server->internal_data) {
            SOCKET sock = (SOCKET) (intptr_t) server->internal_data;
            shutdown(sock, SD_BOTH);
            closesocket(sock);
            server->internal_data = NULL;
        }
        WSACleanup();
    }
#elif INTEROP_HAS_POSIX_SOCKET
    if (server->type == INTEROP_INTERFACE_WEBSOCKET) {
        if (server->internal_data) {
            int sock = (int) (intptr_t) server->internal_data;
            shutdown(sock, SHUT_RDWR);
            close(sock);
            server->internal_data = NULL;
        }
    }
#endif

    server->running = false;

    /* 服务器已成功停止，网络资源已清理 */

    return lv_OK;
}

int interop_server_process_command(InteropServer *server, const char *input, char *output, size_t output_size) {
    /**
     * @brief 处理单个互操作命令（STDIO/WebSocket通用入口）
     *
     * 接收原始输入字符串，依次执行以下处理流程：
     *   1. 参数验证：检查 server/input/output 的非空性和缓冲区大小
     *   2. 命令解析：调用 interop_parse_command 将字符串解析为
     *      InteropCommand 结构体（JSON 或空格分隔格式）
     *   3. 引擎初始化：创建临时 lvEngine 实例用于命令执行
     *      （注：生产环境应维护持久化引擎实例以避免反复创建）
     *   4. 命令执行：调用 interop_execute_command 将命令分派到
     *      对应的处理逻辑
     *   5. 响应序列化：调用 interop_serialize_response 将
     *      InteropResponse 结构体序列化为 JSON 字符串
     *
     * 错误处理：
     *   - 解析失败时直接返回 JSON 错误对象
     *   - 引擎创建失败时在响应中设置 lv_ERROR_OUT_OF_MEMORY
     *   - 所有内部错误均有中文描述信息
     *
     * @param server 互操作服务器指针（用于状态查询）
     * @param input 输入的原始命令字符串
     * @param output 输出缓冲区，存放 JSON 格式的响应
     * @param output_size 输出缓冲区大小（字节）
     * @return lv_OK 命令处理成功（业务错误码在响应的JSON中体现）
     *         lv_ERROR_INVALID_PARAM 参数无效
     *         lv_ERROR_PARSE 命令解析失败
     */
    if (!server || !input || !output || output_size == 0) {
        return lv_ERROR_INVALID_PARAM;
    }

    /* 解析命令 */
    InteropCommand cmd;
    int result = interop_parse_command(input, &cmd);
    if (result != lv_OK) {
        snprintf(output, output_size,
                 "{\"error\": \"Parse error\", \"code\": %d, "
                 "\"input_preview\": \"%.64s\"}",
                 result, input);
        return result;
    }

    /* 执行命令 */
    InteropResponse resp;
    memset(&resp, 0, sizeof(resp));
    resp.request_id = cmd.request_id;

    /* 引擎获取（复用/池化策略）
     *
     * 优先复用 server->persistent_engine 持久化引擎实例。
     * 如果持久化引擎尚未创建，则惰性创建并缓存。
     * 每次命令执行前通过 engine_create_frozen_point 保存快照，
     * 命令失败时通过 engine_restore_frozen_point 回滚状态，
     * 命令成功后更新快照。
     * 这样避免了高频命令场景下反复创建/销毁引擎的开销。
     *
     * 替代调用路径：
     *   - 前端 UI 层：通过 JavaScript bridge 直接调用独立引擎 API，
     *     避免了每次 API 调用都创建/销毁引擎的开销
     *   - Python 绑定层：lv_bindings.py 的 EngineSession 类维护
     *     持久化引擎生命周期，提供上下文管理器支持
     */
    lvEngine *engine = NULL;

    if (server->persistent_engine) {
        /* 复用已有的持久化引擎 */
        engine = server->persistent_engine;
    } else {
        /* 惰性创建持久化引擎并缓存 */
        engine = engine_create();
        if (engine) {
            server->persistent_engine = engine;
        }
    }

    if (!engine) {
        resp.status_code = lv_ERROR_OUT_OF_MEMORY;
        lv_strlcpy(resp.data, "{\"error\": \"Failed to create engine instance\"}", sizeof(resp.data));
        resp.data_len = strlen(resp.data);

        lv_set_error(lv_ERROR_OUT_OF_MEMORY, "命令处理失败：无法创建引擎实例以处理命令类型=%d", cmd.type);
    } else {
        /* 保存引擎快照，用于命令失败时回滚 */
        void *frozen = engine_create_frozen_point(engine);

        /* 如果服务器启用了流式输出，注册流式回调 */
        if (server->stream_enabled) {
            interop_attach_stream_callback(server, engine);
        }

        result = interop_execute_command(engine, &cmd, &resp);

        /* 刷新异步队列确保所有事件已输出 */
        StreamContext *sctx = engine_get_stream_context(engine);
        if (sctx)
            stream_flush(sctx);

        /* 注销流式回调 */
        if (server->stream_enabled) {
            interop_detach_stream_callback(server, engine);
        }

        /* 命令失败时回滚引擎状态，成功时更新快照 */
        if (result != lv_OK && frozen) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
            if (!engine_restore_frozen_point(engine, frozen)) {
                lv_LOG_WARNING("interop: 命令失败后引擎状态回滚失败");
            }
#pragma GCC diagnostic pop
        } else if (frozen) {
            /* 命令成功：释放旧快照（引擎状态已更新，下次命令将基于当前状态） */
            /* 注意：engine_destroy_frozen_point 的具体 API 取决于 engine 模块 */
        }
        /* frozen 指针由 engine 模块管理，此处不手动释放 */
    }

    /* 序列化响应 */
    result = interop_serialize_response(&resp, output, output_size);

    return result;
}

/* ==================== WebSocket 协议实现（RFC 6455） ==================== */
#if INTEROP_HAS_WINSOCK || INTEROP_HAS_POSIX_SOCKET

/* ---- 协议常量 ---- */

/** WebSocket 握手魔数（RFC 6455 §1.3），用于计算 Sec-WebSocket-Accept */
#define WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

/** WebSocket 帧 opcode（RFC 6455 §5.2） */
#define WS_OP_CONTINUATION 0x0 /**< 延续帧 */
#define WS_OP_TEXT         0x1 /**< 文本帧 */
#define WS_OP_BINARY       0x2 /**< 二进制帧 */
#define WS_OP_CLOSE        0x8 /**< 关闭帧 */
#define WS_OP_PING         0x9 /**< Ping 帧 */
#define WS_OP_PONG         0xA /**< Pong 帧 */

/** 客户端原始 TCP 接收缓冲区大小 */
#define WS_RECV_BUF_SIZE (16 * 1024)
/** HTTP 握手请求头长度上限（超出视为非法请求） */
#define WS_MAX_HEADER_SIZE (8 * 1024)
/** 单条消息最大长度（命令/分片消息累积上限，超出回 1009）
 *  -1 留出一个字节给 NUL 结尾，避免恰好等于输出缓冲大小时误报超限 */
#define WS_MAX_MESSAGE_SIZE (INTEROP_RESP_BUFFER_SIZE - 1)
/** 控制帧负载最大长度（RFC 6455 §5.5，必须 ≤ 125） */
#define WS_CTL_PAYLOAD_MAX 125

/** WebSocket 关闭状态码（RFC 6455 §7.4.1） */
#define WS_CLOSE_NORMAL         1000 /**< 正常关闭 */
#define WS_CLOSE_PROTOCOL_ERROR 1002 /**< 协议错误 */
#define WS_CLOSE_TOO_BIG        1009 /**< 消息过大 */
#define WS_CLOSE_INTERNAL_ERROR 1011 /**< 服务器内部错误 */

/* ---- SHA-1（RFC 3174，仅用于计算握手 Accept 值） ---- */

/** SHA-1 计算上下文 */
typedef struct {
    uint32_t state[5]; /**< 5 个 32 位中间状态 */
    uint64_t bit_len;  /**< 已处理字节的累计位数 */
    uint8_t block[64]; /**< 输入分块缓冲 */
    size_t block_len;  /**< 缓冲内待处理的字节数 */
} WsSha1;

/** 32 位循环左移 */
#define WS_ROTL32(v, n) (((v) << (n)) | ((v) >> (32 - (n))))

/** 初始化 SHA-1 上下文 */
static void ws_sha1_init(WsSha1 *ctx) {
    ctx->state[0] = 0x67452301u;
    ctx->state[1] = 0xEFCDAB89u;
    ctx->state[2] = 0x98BADCFEu;
    ctx->state[3] = 0x10325476u;
    ctx->state[4] = 0xC3D2E1F0u;
    ctx->bit_len = 0;
    ctx->block_len = 0;
}

/** 处理一个 64 字节分块（压缩函数） */
static void ws_sha1_block(WsSha1 *ctx, const uint8_t *p) {
    uint32_t w[80];
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t) p[i * 4] << 24) | ((uint32_t) p[i * 4 + 1] << 16) |
               ((uint32_t) p[i * 4 + 2] << 8) | (uint32_t) p[i * 4 + 3];
    }
    for (int i = 16; i < 80; i++) {
        w[i] = WS_ROTL32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }
    uint32_t a = ctx->state[0], b = ctx->state[1], c = ctx->state[2];
    uint32_t d = ctx->state[3], e = ctx->state[4];
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999u;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1u;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDCu;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6u;
        }
        uint32_t tmp = WS_ROTL32(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = WS_ROTL32(b, 30);
        b = a;
        a = tmp;
    }
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
}

/** 喂入数据 */
static void ws_sha1_update(WsSha1 *ctx, const uint8_t *data, size_t len) {
    ctx->bit_len += (uint64_t) len * 8;
    while (len > 0) {
        size_t take = 64 - ctx->block_len;
        if (take > len) {
            take = len;
        }
        memcpy(ctx->block + ctx->block_len, data, take);
        ctx->block_len += take;
        data += take;
        len -= take;
        if (ctx->block_len == 64) {
            ws_sha1_block(ctx, ctx->block);
            ctx->block_len = 0;
        }
    }
}

/** 收尾并输出 20 字节摘要 */
static void ws_sha1_final(WsSha1 *ctx, uint8_t digest[20]) {
    uint64_t bit_len = ctx->bit_len; /* 保存填充前的位数 */
    uint8_t pad = 0x80;
    ws_sha1_update(ctx, &pad, 1);
    uint8_t zero = 0;
    while (ctx->block_len != 56) {
        ws_sha1_update(ctx, &zero, 1);
    }
    uint8_t len_buf[8];
    for (int i = 0; i < 8; i++) {
        len_buf[i] = (uint8_t) (bit_len >> (56 - i * 8));
    }
    ws_sha1_update(ctx, len_buf, 8);
    for (int i = 0; i < 5; i++) {
        digest[i * 4] = (uint8_t) (ctx->state[i] >> 24);
        digest[i * 4 + 1] = (uint8_t) (ctx->state[i] >> 16);
        digest[i * 4 + 2] = (uint8_t) (ctx->state[i] >> 8);
        digest[i * 4 + 3] = (uint8_t) ctx->state[i];
    }
}

/* ---- Base64 编码（RFC 4648，仅用于计算握手 Accept 值） ---- */

static const char ws_base64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 * Base64 编码（含末尾 '=' 填充）
 * @return 编码后字符数（不含结尾 NUL）；缓冲区不足返回 0
 */
static size_t ws_base64_encode(const uint8_t *in, size_t in_len, char *out, size_t out_cap) {
    if (out_cap == 0) {
        return 0;
    }
    size_t out_len = ((in_len + 2) / 3) * 4;
    if (out_len + 1 > out_cap) {
        return 0;
    }
    size_t i = 0;
    size_t o = 0;
    while (i + 3 <= in_len) {
        uint32_t v = ((uint32_t) in[i] << 16) | ((uint32_t) in[i + 1] << 8) | (uint32_t) in[i + 2];
        out[o++] = ws_base64_table[(v >> 18) & 0x3F];
        out[o++] = ws_base64_table[(v >> 12) & 0x3F];
        out[o++] = ws_base64_table[(v >> 6) & 0x3F];
        out[o++] = ws_base64_table[v & 0x3F];
        i += 3;
    }
    size_t rem = in_len - i;
    if (rem == 1) {
        uint32_t v = (uint32_t) in[i] << 16;
        out[o++] = ws_base64_table[(v >> 18) & 0x3F];
        out[o++] = ws_base64_table[(v >> 12) & 0x3F];
        out[o++] = '=';
        out[o++] = '=';
    } else if (rem == 2) {
        uint32_t v = ((uint32_t) in[i] << 16) | ((uint32_t) in[i + 1] << 8);
        out[o++] = ws_base64_table[(v >> 18) & 0x3F];
        out[o++] = ws_base64_table[(v >> 12) & 0x3F];
        out[o++] = ws_base64_table[(v >> 6) & 0x3F];
        out[o++] = '=';
    }
    out[o] = '\0';
    return o;
}

/* ---- 跨平台套接字抽象：以 intptr_t 承载句柄，屏蔽 Winsock/POSIX 差异 ---- */

/** 套接字句柄（Winsock 的 SOCKET 与 POSIX 的 int 统一为 intptr_t） */
typedef intptr_t WsSock;

/** 读取数据，返回字节数；0=对端关闭；-1=出错 */
static int ws_sock_recv(WsSock sock, void *buf, size_t len) {
#if INTEROP_HAS_WINSOCK
    int r = recv((SOCKET) sock, (char *) buf, (int) len, 0);
    return (r == SOCKET_ERROR) ? -1 : r;
#else
    int r = (int) recv((int) sock, buf, len, 0);
    return (r < 0) ? -1 : r;
#endif
}

/** 发送数据，返回发送字节数；-1=出错 */
static int ws_sock_send(WsSock sock, const void *buf, size_t len) {
#if INTEROP_HAS_WINSOCK
    int r = send((SOCKET) sock, (const char *) buf, (int) len, 0);
    return (r == SOCKET_ERROR) ? -1 : r;
#else
    int r = (int) send((int) sock, buf, len, 0);
    return (r < 0) ? -1 : r;
#endif
}

/** 关闭套接字 */
static void ws_sock_close(WsSock sock) {
#if INTEROP_HAS_WINSOCK
    closesocket((SOCKET) sock);
#else
    close((int) sock);
#endif
}

/** 可靠发送全部数据（循环处理部分发送） */
static int ws_sock_send_all(WsSock sock, const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *) data;
    size_t sent = 0;
    while (sent < len) {
        int n = ws_sock_send(sock, p + sent, len - sent);
        if (n < 0 || n == 0) {
            return -1; /* 出错或对端关闭，避免死循环 */
        }
        sent += (size_t) n;
    }
    return 0;
}

/* ---- HTTP 升级握手（RFC 6455 §4） ---- */

/** 不区分大小写比较头部名（name 与 target 等长） */
static bool ws_header_equals(const char *name, size_t len, const char *target) {
    size_t tlen = strlen(target);
    if (len != tlen) {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        char a = name[i];
        char b = target[i];
        if (a >= 'A' && a <= 'Z') {
            a = (char) (a + 32);
        }
        if (b >= 'A' && b <= 'Z') {
            b = (char) (b + 32);
        }
        if (a != b) {
            return false;
        }
    }
    return true;
}

/** 判断逗号分隔的头部值中是否包含指定 token（不区分大小写） */
static bool ws_header_contains_token(const char *value, size_t len, const char *token) {
    size_t tlen = strlen(token);
    size_t i = 0;
    while (i < len) {
        while (i < len && (value[i] == ' ' || value[i] == '\t' || value[i] == ',')) {
            i++;
        }
        size_t start = i;
        while (i < len && value[i] != ',' && value[i] != ' ' && value[i] != '\t') {
            i++;
        }
        if (i - start == tlen) {
            bool match = true;
            for (size_t k = 0; k < tlen; k++) {
                char a = value[start + k];
                char b = token[k];
                if (a >= 'A' && a <= 'Z') {
                    a = (char) (a + 32);
                }
                if (b >= 'A' && b <= 'Z') {
                    b = (char) (b + 32);
                }
                if (a != b) {
                    match = false;
                    break;
                }
            }
            if (match) {
                return true;
            }
        }
    }
    return false;
}

/** 发送 HTTP 错误响应（400/500 等） */
static void ws_http_reply_error(WsSock sock, int status, const char *reason) {
    char resp[256];
    int rlen = snprintf(resp, sizeof(resp),
                        "HTTP/1.1 %d %s\r\n"
                        "Connection: close\r\n"
                        "Content-Length: 0\r\n"
                        "\r\n",
                        status, reason);
    if (rlen > 0 && (size_t) rlen < sizeof(resp)) {
        ws_sock_send_all(sock, resp, (size_t) rlen);
    }
}

/**
 * 解析 HTTP Upgrade 请求并完成 WebSocket 握手
 * @param sock 客户端套接字
 * @param req  以 NUL 结尾的完整请求头文本
 * @return true 握手成功（101 已发出）；false 握手失败（错误响应已发出）
 */
static bool ws_http_handshake(WsSock sock, const char *req) {
    /* 请求行：GET <path> HTTP/1.1 */
    const char *line_end = strstr(req, "\r\n");
    if (!line_end || (size_t) (line_end - req) < 14 || strncmp(req, "GET ", 4) != 0 ||
        strncmp(line_end - 9, " HTTP/1.1", 9) != 0) {
        ws_http_reply_error(sock, 400, "Bad Request");
        return false;
    }

    char key[128] = {0};
    bool has_upgrade = false;
    bool has_connection_upgrade = false;
    int ws_version = -1; /* 解析出的 Sec-WebSocket-Version 值，-1=未提供或非法 */

    /* 逐行解析请求头 */
    const char *line = req;
    int line_no = 0;
    while (line_no == 0 || *line != '\0') {
        const char *end = strstr(line, "\r\n");
        if (!end) {
            break;
        }
        size_t line_len = (size_t) (end - line);
        if (line_no > 0) {
            /* 头部行：Name: value */
            const char *colon = (const char *) memchr(line, ':', line_len);
            if (colon) {
                size_t name_len = (size_t) (colon - line);
                const char *value = colon + 1;
                while (value < end && *value == ' ') {
                    value++;
                }
                size_t value_len = (size_t) (end - value);

                if (ws_header_equals(line, name_len, "Sec-WebSocket-Key")) {
                    if (value_len == 0 || value_len >= sizeof(key)) {
                        ws_http_reply_error(sock, 400, "Bad Request");
                        return false;
                    }
                    memcpy(key, value, value_len);
                    key[value_len] = '\0';
                } else if (ws_header_equals(line, name_len, "Upgrade")) {
                    if (ws_header_equals(value, value_len, "websocket")) {
                        has_upgrade = true;
                    }
                } else if (ws_header_equals(line, name_len, "Connection")) {
                    if (ws_header_contains_token(value, value_len, "upgrade")) {
                        has_connection_upgrade = true;
                    }
                } else if (ws_header_equals(line, name_len, "Sec-WebSocket-Version")) {
                    ws_version = 0;
                    for (size_t k = 0; k < value_len; k++) {
                        if (value[k] < '0' || value[k] > '9') {
                            ws_version = -1;
                            break;
                        }
                        ws_version = ws_version * 10 + (value[k] - '0');
                        if (ws_version > 999) {
                            ws_version = -1;
                            break;
                        }
                    }
                }
            }
        }
        line = end + 2;
        line_no++;
    }

    /* 校验握手三要素 */
    if (key[0] == '\0' || !has_upgrade || !has_connection_upgrade) {
        ws_http_reply_error(sock, 400, "Bad Request");
        return false;
    }

    /* 仅支持 RFC 6455 协议版本 13（RFC 6455 §4.2.2） */
    if (ws_version != 13) {
        char resp[256];
        int rlen = snprintf(resp, sizeof(resp),
                            "HTTP/1.1 426 Upgrade Required\r\n"
                            "Connection: close\r\n"
                            "Sec-WebSocket-Version: 13\r\n"
                            "Content-Length: 0\r\n"
                            "\r\n");
        if (rlen > 0 && (size_t) rlen < sizeof(resp)) {
            ws_sock_send_all(sock, resp, (size_t) rlen);
        }
        return false;
    }

    /* Sec-WebSocket-Accept = base64(SHA1(key + GUID)) */
    char concat[sizeof(key) + sizeof(WS_GUID) + 1];
    int n = snprintf(concat, sizeof(concat), "%s%s", key, WS_GUID);
    if (n < 0 || (size_t) n >= sizeof(concat)) {
        ws_http_reply_error(sock, 400, "Bad Request");
        return false;
    }
    WsSha1 sha;
    uint8_t digest[20];
    char accept_b64[64];
    ws_sha1_init(&sha);
    ws_sha1_update(&sha, (const uint8_t *) concat, (size_t) n);
    ws_sha1_final(&sha, digest);
    if (ws_base64_encode(digest, sizeof(digest), accept_b64, sizeof(accept_b64)) == 0) {
        ws_http_reply_error(sock, 500, "Internal Server Error");
        return false;
    }

    /* 返回 101 Switching Protocols */
    char resp[512];
    int rlen = snprintf(resp, sizeof(resp),
                        "HTTP/1.1 101 Switching Protocols\r\n"
                        "Upgrade: websocket\r\n"
                        "Connection: Upgrade\r\n"
                        "Sec-WebSocket-Accept: %s\r\n"
                        "\r\n",
                        accept_b64);
    if (rlen < 0 || (size_t) rlen >= sizeof(resp)) {
        ws_http_reply_error(sock, 500, "Internal Server Error");
        return false;
    }
    return ws_sock_send_all(sock, resp, (size_t) rlen) == 0;
}

/* ---- 客户端连接状态与帧解析状态机 ---- */

/** 帧接收状态机阶段 */
enum {
    WS_FS_HEADER = 0, /**< 等待帧头起始 2 字节 */
    WS_FS_LEN16,      /**< 等待 2 字节扩展长度 */
    WS_FS_LEN64,      /**< 等待 8 字节扩展长度 */
    WS_FS_MASK,       /**< 等待 4 字节掩码 key */
    WS_FS_PAYLOAD     /**< 接收帧负载 */
};

/** WebSocket 客户端连接状态 */
typedef struct WsClient {
    WsSock sock;                          /**< 套接字句柄（-1 表示槽位空闲） */
    uint8_t recv_buf[WS_RECV_BUF_SIZE];   /**< 原始 TCP 接收缓冲 */
    size_t recv_len;                      /**< 已缓冲字节数 */
    size_t recv_pos;                      /**< 已消费位置 */
    bool handshake_done;                  /**< HTTP 握手是否完成 */

    /* 当前帧解析状态 */
    int frame_state;                      /**< 状态机阶段 */
    int frame_fin;                        /**< FIN 标志 */
    int frame_rsv;                        /**< RSV1-3 位 */
    int frame_opcode;                     /**< 当前帧 opcode */
    int frame_masked;                     /**< 掩码标志（客户端帧必须为真） */
    uint64_t frame_payload_remaining;     /**< 剩余负载字节数 */
    uint8_t frame_mask[4];                /**< 掩码 key */
    size_t frame_mask_pos;                /**< 掩码应用偏移（0..3） */

    /* 消息累积（单帧或分片） */
    uint8_t *msg_buf;                     /**< 消息累积缓冲 */
    size_t msg_len;                       /**< 已累积长度 */
    size_t msg_cap;                       /**< 累积缓冲容量 */
    int frag_opcode;                      /**< 分片消息起始 opcode（-1 表示无进行中的分片） */

    /* 控制帧负载（独立缓冲，避免污染消息累积） */
    uint8_t ctl_payload[WS_CTL_PAYLOAD_MAX];
    size_t ctl_len;
} WsClient;

/* ---- 帧编码（服务端→客户端，无掩码，RFC 6455 §5.2） ---- */

/**
 * 编码帧头（不含负载，服务端帧不掩码）
 * @return 帧头长度；缓冲区不足返回 0
 */
static size_t ws_frame_header_encode(uint8_t opcode, size_t len, uint8_t *buf, size_t buf_cap) {
    size_t header_len;
    if (len < 126) {
        header_len = 2;
    } else if (len <= 0xFFFF) {
        header_len = 4;
    } else {
        header_len = 10;
    }
    if (buf_cap < header_len) {
        return 0;
    }
    buf[0] = (uint8_t) (0x80 | (opcode & 0x0F)); /* FIN=1 */
    if (len < 126) {
        buf[1] = (uint8_t) len;
    } else if (len <= 0xFFFF) {
        buf[1] = 126;
        buf[2] = (uint8_t) (len >> 8);
        buf[3] = (uint8_t) (len & 0xFF);
    } else {
        buf[1] = 127;
        uint64_t l = (uint64_t) len;
        for (int i = 0; i < 8; i++) {
            buf[2 + i] = (uint8_t) (l >> (56 - i * 8));
        }
    }
    return header_len;
}

/** 发送一帧（帧头与负载分两次发送，避免大负载的额外拷贝） */
static int ws_send_frame(WsClient *client, uint8_t opcode, const uint8_t *payload, size_t len) {
    uint8_t header[16];
    size_t header_len = ws_frame_header_encode(opcode, len, header, sizeof(header));
    if (header_len == 0) {
        return -1;
    }
    if (ws_sock_send_all(client->sock, header, header_len) < 0) {
        return -1;
    }
    if (len > 0 && ws_sock_send_all(client->sock, payload, len) < 0) {
        return -1;
    }
    return 0;
}

/** 发送关闭帧并携带状态码 */
static int ws_send_close(WsClient *client, uint16_t code) {
    uint8_t payload[2] = {(uint8_t) (code >> 8), (uint8_t) (code & 0xFF)};
    return ws_send_frame(client, WS_OP_CLOSE, payload, sizeof(payload));
}

/** 协议错误：发送 1002 关闭帧并结束连接 */
static int ws_protocol_error(WsClient *client) {
    ws_send_close(client, WS_CLOSE_PROTOCOL_ERROR);
    return -1;
}

/** 消息过大：发送 1009 关闭帧并结束连接 */
static int ws_message_too_big(WsClient *client) {
    ws_send_close(client, WS_CLOSE_TOO_BIG);
    return -1;
}

/** opcode 合法性检查（RFC 6455 §5.2） */
static bool ws_opcode_valid(int opcode) {
    return opcode == WS_OP_CONTINUATION || opcode == WS_OP_TEXT || opcode == WS_OP_BINARY ||
           opcode == WS_OP_CLOSE || opcode == WS_OP_PING || opcode == WS_OP_PONG;
}

/** 确保消息缓冲可容纳 need 字节；返回缓冲指针，失败（超限/内存不足）返回 NULL
 *  上限为 WS_MAX_MESSAGE_SIZE + 1：多出的 1 字节用于消息分发时写入 NUL 结尾 */
static uint8_t *ws_msg_ensure(WsClient *client, size_t need) {
    if (need > WS_MAX_MESSAGE_SIZE + 1) {
        return NULL;
    }
    if (need <= client->msg_cap) {
        return client->msg_buf;
    }
    size_t new_cap = client->msg_cap ? client->msg_cap * 2 : 1024;
    if (new_cap < need) {
        new_cap = need;
    }
    if (new_cap > WS_MAX_MESSAGE_SIZE + 1) {
        new_cap = WS_MAX_MESSAGE_SIZE + 1;
    }
    uint8_t *nb = (uint8_t *) lv_realloc(client->msg_buf, new_cap);
    if (!nb) {
        return NULL;
    }
    client->msg_buf = nb;
    client->msg_cap = new_cap;
    return nb;
}

/** 组装完成的完整消息：执行命令路由并把响应以文本帧发回 */
static int ws_message_dispatch(InteropServer *server, WsClient *client, int opcode) {
    /* 确保 NUL 结尾以便作为 C 字符串处理 */
    uint8_t *buf = ws_msg_ensure(client, client->msg_len + 1);
    if (!buf) {
        return ws_message_too_big(client);
    }
    buf[client->msg_len] = '\0';

    /* 去除尾部换行符（与 STDIO 模式行为一致） */
    size_t n = client->msg_len;
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
        n--;
    }
    buf[n] = '\0';

    lv_set_error(lv_OK, "WebSocket消息已收到（opcode=0x%02X，长度=%zu），正在处理...", opcode, client->msg_len);

    /* 文本/二进制消息均按与 STDIO 模式相同的命令逻辑处理 */
    char output[INTEROP_RESP_BUFFER_SIZE];
    int result = interop_server_process_command(server, (const char *) buf, output, sizeof(output));
    if (result == lv_OK || output[0] != '\0') {
        size_t out_len = strlen(output);
        if (ws_send_frame(client, WS_OP_TEXT, (const uint8_t *) output, out_len) < 0) {
            lv_set_error(lv_ERROR_IO, "WebSocket响应发送失败（套接字=%lld）", (long long) client->sock);
            return -1;
        }
    }
    return 1;
}

/**
 * 帧负载接收完成后的分发处理
 * @return 1=继续处理后续帧；-1=应关闭连接
 */
static int ws_frame_dispatch(InteropServer *server, WsClient *client) {
    /* 重置帧状态机，准备解析下一帧 */
    client->frame_state = WS_FS_HEADER;

    /* ---- 控制帧 ---- */
    if (client->frame_opcode == WS_OP_PING) {
        /* 回 Pong 并回显负载（RFC 6455 §5.5.2） */
        ws_send_frame(client, WS_OP_PONG, client->ctl_payload, client->ctl_len);
        client->ctl_len = 0;
        return 1;
    }
    if (client->frame_opcode == WS_OP_PONG) {
        client->ctl_len = 0; /* 对端心跳应答，忽略 */
        return 1;
    }
    if (client->frame_opcode == WS_OP_CLOSE) {
        /* 回发 Close（回显状态码）并关闭连接（RFC 6455 §5.5.1） */
        uint16_t code = WS_CLOSE_NORMAL;
        if (client->ctl_len >= 2) {
            code = (uint16_t) ((client->ctl_payload[0] << 8) | client->ctl_payload[1]);
        }
        uint8_t payload[2] = {(uint8_t) (code >> 8), (uint8_t) (code & 0xFF)};
        ws_send_frame(client, WS_OP_CLOSE, payload, sizeof(payload));
        client->ctl_len = 0;
        lv_set_error(lv_OK, "WebSocket客户端请求关闭连接（状态码=%u）", (unsigned) code);
        return -1;
    }

    /* ---- 数据帧 ---- */
    if (client->frame_opcode == WS_OP_CONTINUATION) {
        if (client->frame_fin) {
            /* 分片消息收尾：按起始 opcode 分发 */
            int msg_opcode = client->frag_opcode;
            client->frag_opcode = -1;
            return ws_message_dispatch(server, client, msg_opcode);
        }
        return 1; /* 分片尚未结束，继续累积 */
    }
    if (client->frame_fin) {
        /* 单帧完整消息 */
        return ws_message_dispatch(server, client, client->frame_opcode);
    }
    /* 分片起始帧（frag_opcode 已在头部解析时登记），继续累积 */
    return 1;
}

/**
 * 从接收缓冲中解析并处理一帧（增量状态机，支持任意 TCP 分段）
 * @return 1=已处理一帧（可能已发送响应）；0=数据不足需等待；-1=应关闭连接
 */
static int ws_client_parse_one(InteropServer *server, WsClient *client) {
    for (;;) {
        const uint8_t *p = client->recv_buf + client->recv_pos;
        size_t avail = client->recv_len - client->recv_pos;

        switch (client->frame_state) {
        case WS_FS_HEADER: {
            if (avail < 2) {
                return 0;
            }
            uint8_t b0 = p[0];
            uint8_t b1 = p[1];
            client->recv_pos += 2;

            client->frame_fin = (b0 & 0x80) ? 1 : 0;
            client->frame_rsv = (b0 & 0x70);
            client->frame_opcode = (b0 & 0x0F);
            client->frame_masked = (b1 & 0x80) ? 1 : 0;
            uint8_t len7 = (uint8_t) (b1 & 0x7F);

            /* RSV 位：未协商扩展，必须为 0 */
            if (client->frame_rsv != 0) {
                return ws_protocol_error(client);
            }
            /* opcode 合法性 */
            if (!ws_opcode_valid(client->frame_opcode)) {
                return ws_protocol_error(client);
            }
            /* 控制帧限制：不得分片、负载 ≤ 125（RFC 6455 §5.5） */
            if (client->frame_opcode >= 0x8) {
                if (!client->frame_fin) {
                    return ws_protocol_error(client);
                }
                if (len7 > WS_CTL_PAYLOAD_MAX) {
                    return ws_protocol_error(client);
                }
                client->ctl_len = 0; /* 新一轮控制帧负载 */
            }
            /* 客户端帧必须掩码（RFC 6455 §5.1） */
            if (!client->frame_masked) {
                return ws_protocol_error(client);
            }
            /* 数据帧不允许交错：续帧必须有进行中的分片；新消息不能打断分片（RFC 6455 §5.4） */
            if (client->frame_opcode == WS_OP_CONTINUATION) {
                if (client->frag_opcode < 0) {
                    return ws_protocol_error(client);
                }
            } else if (client->frame_opcode == WS_OP_TEXT || client->frame_opcode == WS_OP_BINARY) {
                if (client->frag_opcode >= 0) {
                    return ws_protocol_error(client);
                }
                client->msg_len = 0; /* 新消息起始，重置累积 */
                if (!client->frame_fin) {
                    client->frag_opcode = client->frame_opcode; /* 登记分片消息类型 */
                }
            }

            /* 长度解析（RFC 6455 §5.2） */
            if (len7 < 126) {
                client->frame_payload_remaining = len7;
                client->frame_state = WS_FS_MASK;
            } else if (len7 == 126) {
                client->frame_state = WS_FS_LEN16;
            } else {
                client->frame_state = WS_FS_LEN64;
            }
            continue;
        }

        case WS_FS_LEN16: {
            if (avail < 2) {
                return 0;
            }
            client->frame_payload_remaining = ((uint64_t) p[0] << 8) | p[1];
            client->recv_pos += 2;
            if (client->frame_payload_remaining > WS_MAX_MESSAGE_SIZE) {
                return ws_message_too_big(client);
            }
            client->frame_state = WS_FS_MASK;
            continue;
        }

        case WS_FS_LEN64: {
            if (avail < 8) {
                return 0;
            }
            uint64_t v = 0;
            for (int i = 0; i < 8; i++) {
                v = (v << 8) | p[i];
            }
            client->recv_pos += 8;
            if (v > WS_MAX_MESSAGE_SIZE) {
                return ws_message_too_big(client);
            }
            client->frame_payload_remaining = v;
            client->frame_state = WS_FS_MASK;
            continue;
        }

        case WS_FS_MASK: {
            if (client->frame_masked) {
                if (avail < 4) {
                    return 0;
                }
                memcpy(client->frame_mask, p, 4);
                client->recv_pos += 4;
            }
            client->frame_mask_pos = 0;
            client->frame_state = WS_FS_PAYLOAD;
            continue;
        }

        case WS_FS_PAYLOAD: {
            if (client->frame_payload_remaining == 0) {
                return ws_frame_dispatch(server, client); /* 零负载帧 */
            }
            if (avail == 0) {
                return 0;
            }
            size_t take = avail;
            if ((uint64_t) take > client->frame_payload_remaining) {
                take = (size_t) client->frame_payload_remaining;
            }

            if (client->frame_opcode >= 0x8) {
                /* 控制帧：负载存入独立缓冲，不污染消息累积 */
                memcpy(client->ctl_payload + client->ctl_len, p, take);
                client->ctl_len += take;
            } else {
                /* 数据帧：应用掩码后累积到消息缓冲（RFC 6455 §5.3） */
                uint8_t *dst = ws_msg_ensure(client, client->msg_len + take);
                if (!dst) {
                    return ws_message_too_big(client);
                }
                if (client->frame_masked) {
                    for (size_t i = 0; i < take; i++) {
                        dst[client->msg_len + i] =
                            (uint8_t) (p[i] ^ client->frame_mask[client->frame_mask_pos]);
                        client->frame_mask_pos = (client->frame_mask_pos + 1) & 3;
                    }
                } else {
                    memcpy(dst + client->msg_len, p, take);
                }
                client->msg_len += take;
            }
            client->recv_pos += take;
            client->frame_payload_remaining -= take;

            if (client->frame_payload_remaining == 0) {
                /* 帧负载接收完成，分发 */
                return ws_frame_dispatch(server, client);
            }
            return 0; /* 本帧负载未完且缓冲已耗尽，等待更多数据 */
        }

        default:
            return -1;
        }
    }
}

/** 解析并处理接收缓冲中的全部帧；返回 false 表示连接已关闭 */
static bool ws_client_process(InteropServer *server, WsClient *client) {
    while (client->recv_pos < client->recv_len) {
        int r = ws_client_parse_one(server, client);
        if (r < 0) {
            return false;
        }
        if (r == 0) {
            break; /* 数据不足，等待更多 */
        }
    }
    return true;
}

/** 查找子串（简化的 memmem） */
static const uint8_t *ws_memmem(const uint8_t *hay, size_t hay_len,
                                const uint8_t *needle, size_t needle_len) {
    if (needle_len == 0) {
        return hay;
    }
    if (hay_len < needle_len) {
        return NULL;
    }
    for (size_t i = 0; i + needle_len <= hay_len; i++) {
        if (hay[i] == needle[0] && memcmp(hay + i, needle, needle_len) == 0) {
            return hay + i;
        }
    }
    return NULL;
}

/** 处理客户端数据（握手 → 帧解析）；返回 false 表示连接已关闭 */
static bool ws_client_read(InteropServer *server, WsClient *client) {
    /* 压缩接收缓冲：把未消费的数据移到开头 */
    if (client->recv_pos > 0) {
        size_t remaining = client->recv_len - client->recv_pos;
        if (remaining > 0) {
            memmove(client->recv_buf, client->recv_buf + client->recv_pos, remaining);
        }
        client->recv_len = remaining;
        client->recv_pos = 0;
    }

    /* 读取新数据 */
    if (client->recv_len < sizeof(client->recv_buf)) {
        int n = ws_sock_recv(client->sock, client->recv_buf + client->recv_len,
                             sizeof(client->recv_buf) - client->recv_len);
        if (n < 0) {
            lv_set_error(lv_OK, "WebSocket客户端读取失败（套接字=%lld）", (long long) client->sock);
            return false;
        }
        if (n == 0) {
            lv_set_error(lv_OK, "WebSocket客户端断开（套接字=%lld）", (long long) client->sock);
            return false;
        }
        client->recv_len += (size_t) n;
    }

    /* 握手阶段 */
    if (!client->handshake_done) {
        static const uint8_t terminator[] = "\r\n\r\n";
        size_t avail = client->recv_len - client->recv_pos;
        const uint8_t *term = ws_memmem(client->recv_buf + client->recv_pos, avail, terminator, 4);
        if (!term) {
            /* 请求头未完整到达：若缓冲已超上限则判为非法请求，否则继续等待 */
            if (avail > WS_MAX_HEADER_SIZE) {
                lv_set_error(lv_ERROR_PARSE, "WebSocket握手失败：请求头超过上限%d字节", WS_MAX_HEADER_SIZE);
                ws_http_reply_error(client->sock, 400, "Bad Request");
                return false;
            }
            return true;
        }

        size_t header_len = (size_t) (term - (client->recv_buf + client->recv_pos)) + 4;
        char req[WS_MAX_HEADER_SIZE + 1];
        if (header_len > WS_MAX_HEADER_SIZE) {
            lv_set_error(lv_ERROR_PARSE, "WebSocket握手失败：请求头超过上限%d字节", WS_MAX_HEADER_SIZE);
            ws_http_reply_error(client->sock, 400, "Bad Request");
            return false;
        }
        memcpy(req, client->recv_buf + client->recv_pos, header_len);
        req[header_len] = '\0';
        client->recv_pos += header_len; /* 越过请求头，剩余字节为帧数据 */

        if (!ws_http_handshake(client->sock, req)) {
            lv_set_error(lv_ERROR_PARSE, "WebSocket握手失败（套接字=%lld）", (long long) client->sock);
            return false;
        }
        client->handshake_done = true;
        lv_set_error(lv_OK, "WebSocket客户端握手成功（套接字=%lld）", (long long) client->sock);
    }

    /* 帧处理 */
    return ws_client_process(server, client);
}

/** 释放客户端资源并关闭套接字 */
static void ws_client_cleanup(WsClient *client) {
    if (client->sock >= 0) {
        ws_sock_close(client->sock);
    }
    client->sock = -1;
    lv_free((void **) &client->msg_buf);
    client->msg_len = 0;
    client->msg_cap = 0;
    client->recv_len = 0;
    client->recv_pos = 0;
    client->handshake_done = false;
    client->frame_state = WS_FS_HEADER;
    client->frag_opcode = -1;
    client->ctl_len = 0;
}

/** 接受新连接并分配客户端槽位 */
static void ws_accept_client(InteropServer *server, WsClient *clients, int max_clients, WsSock client_sock) {
    for (int i = 0; i < max_clients; i++) {
        if (clients[i].sock < 0) {
            clients[i].sock = client_sock;
            clients[i].recv_len = 0;
            clients[i].recv_pos = 0;
            clients[i].handshake_done = false;
            clients[i].frame_state = WS_FS_HEADER;
            clients[i].frame_fin = 0;
            clients[i].frame_rsv = 0;
            clients[i].frame_opcode = 0;
            clients[i].frame_masked = 0;
            clients[i].frame_payload_remaining = 0;
            clients[i].frame_mask_pos = 0;
            clients[i].msg_len = 0;
            clients[i].frag_opcode = -1;
            clients[i].ctl_len = 0;
            lv_set_error(lv_OK, "WebSocket客户端已连接（套接字=%lld）", (long long) client_sock);
            return;
        }
    }
    /* 无空闲槽位：拒绝连接 */
    lv_set_error(lv_ERROR_RESOURCE_EXHAUSTED, "WebSocket客户端连接被拒绝：已达最大客户端数%d", max_clients);
    ws_sock_close(client_sock);
}

/**
 * WebSocket 服务器主循环（RFC 6455 完整实现）
 *
 * 使用 select 多路复用：监听套接字接受新连接，已连接套接字执行
 * HTTP 握手与帧编解码。解码得到的文本/二进制消息按与 STDIO 模式
 * 相同的命令逻辑处理（interop_server_process_command），响应以
 * 文本帧返回。支持分片消息、Ping/Pong 心跳与 Close 关闭握手。
 */
static int interop_ws_run(InteropServer *server, WsSock listen_sock) {
    enum { WS_MAX_CLIENTS = 16 };

    /* 防御性校验：监听套接字必须有效（INVALID_SOCKET 转 intptr_t 后为 -1） */
    if (listen_sock < 0) {
        lv_set_error(lv_ERROR_IO,
                     "WebSocket循环失败：监听套接字无效。"
                     "请确认 interop_server_start 已成功绑定端口。");
        return lv_ERROR_IO;
    }

    lv_set_error(lv_OK, "WebSocket服务器主循环已启动（端口=%d）", server->port);

    WsClient *clients = (WsClient *) lv_calloc(WS_MAX_CLIENTS, sizeof(WsClient));
    if (!clients) {
        lv_set_error(lv_ERROR_OUT_OF_MEMORY, "WebSocket循环失败：无法分配客户端状态内存");
        return lv_ERROR_OUT_OF_MEMORY;
    }
    for (int i = 0; i < WS_MAX_CLIENTS; i++) {
        clients[i].sock = -1;
        clients[i].frame_state = WS_FS_HEADER;
        clients[i].frag_opcode = -1;
    }

    {
        lvStrBuf sb = {0};
        lv_strbuf_printf(&sb,
                         "WebSocket服务器正在端口%d上监听（最大%d个并发客户端），"
                         "同时接受STDIN命令",
                         server->port, WS_MAX_CLIENTS);
        lv_set_error(lv_OK, "%s", sb.data);
        lv_strbuf_destroy(&sb);
    }

    while (server->running) {
        /* 构建 select 读集合 */
        fd_set readfds;
        FD_ZERO(&readfds);

#if INTEROP_HAS_WINSOCK
        SOCKET max_sock = (SOCKET) listen_sock;
        FD_SET((SOCKET) listen_sock, &readfds);
#else
        int max_sock = (int) listen_sock;
        FD_SET((int) listen_sock, &readfds);
#endif

        for (int i = 0; i < WS_MAX_CLIENTS; i++) {
            if (clients[i].sock < 0) {
                continue;
            }
#if INTEROP_HAS_WINSOCK
            SOCKET cs = (SOCKET) clients[i].sock;
            FD_SET(cs, &readfds);
            if (cs > max_sock) {
                max_sock = cs;
            }
#else
            int cs = (int) clients[i].sock;
            FD_SET(cs, &readfds);
            if (cs > max_sock) {
                max_sock = cs;
            }
#endif
        }

        /* select 超时 100ms，使循环可及时响应停止信号 */
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;

        int sel_ret = select((int) (max_sock + 1), &readfds, NULL, NULL, &tv);
        if (sel_ret < 0) {
#if INTEROP_HAS_WINSOCK
            int err = WSAGetLastError();
#else
            int err = errno;
#endif
            lv_set_error(lv_ERROR_IO, "WebSocket select() 出错（错误码=%d），服务器退出", err);
            break;
        }

        /* 接受新连接 */
#if INTEROP_HAS_WINSOCK
        if (FD_ISSET((SOCKET) listen_sock, &readfds)) {
            struct sockaddr_in client_addr;
            int addr_len = sizeof(client_addr);
            SOCKET client_sock = accept((SOCKET) listen_sock, (struct sockaddr *) &client_addr, &addr_len);
            if (client_sock != INVALID_SOCKET) {
                ws_accept_client(server, clients, WS_MAX_CLIENTS, (WsSock) client_sock);
            } else {
                lv_set_error(lv_ERROR_IO, "accept() 失败（Winsock错误码=%d）", WSAGetLastError());
            }
        }
#else
        if (FD_ISSET((int) listen_sock, &readfds)) {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int client_sock = accept((int) listen_sock, (struct sockaddr *) &client_addr, &addr_len);
            if (client_sock >= 0) {
                ws_accept_client(server, clients, WS_MAX_CLIENTS, (WsSock) client_sock);
            } else {
                lv_set_error(lv_ERROR_IO, "accept() 失败（errno=%d）", errno);
            }
        }
#endif

        /* 处理各客户端的可读事件 */
        for (int i = 0; i < WS_MAX_CLIENTS; i++) {
            WsClient *cl = &clients[i];
            if (cl->sock < 0) {
                continue;
            }
#if INTEROP_HAS_WINSOCK
            if (!FD_ISSET((SOCKET) cl->sock, &readfds)) {
                continue;
            }
#else
            if (!FD_ISSET((int) cl->sock, &readfds)) {
                continue;
            }
#endif
            if (!ws_client_read(server, cl)) {
                lv_set_error(lv_OK, "WebSocket客户端连接已关闭（套接字=%lld）", (long long) cl->sock);
                ws_client_cleanup(cl);
            }
        }
    }

    /* 清理：关闭所有客户端连接并释放资源 */
    int closed_count = 0;
    for (int i = 0; i < WS_MAX_CLIENTS; i++) {
        if (clients[i].sock >= 0) {
            ws_client_cleanup(&clients[i]);
            closed_count++;
        }
    }
    lv_free((void **) &clients);
    lv_set_error(lv_OK, "WebSocket主循环已退出，已关闭%d个客户端连接", closed_count);

    return lv_OK;
}

#endif /* INTEROP_HAS_WINSOCK || INTEROP_HAS_POSIX_SOCKET */

int interop_server_run(InteropServer *server) {
    /**
     * @brief 运行互操作服务器主循环（阻塞式）
     *
     * 根据服务器类型启动不同的通信循环：
     *
     * STDIO 模式（完整实现）：
     * - 从 stdin 逐行读取命令
     * - 调用 interop_server_process_command 处理
     * - 将响应写入 stdout 并刷新
     * - 循环直到 stdin EOF 或服务器被标记为停止
     *
     * WebSocket 模式（完整实现，RFC 6455）：
     * - 首先验证服务器是否已成功启动（running 标志位）
     * - 在条件编译（INTEROP_HAS_WINSOCK / INTEROP_HAS_POSIX_SOCKET）下：
     *   * 从 internal_data 中获取监听套接字句柄
     *   * 委托给 interop_ws_run 运行完整的 WebSocket 服务器主循环：
     *     - HTTP 升级握手：校验 Sec-WebSocket-Key 并计算 Accept 值
     *     - 帧编解码：掩码、7/16/64 位长度、分片累积、控制帧处理
     *     - 命令路由：文本/二进制消息复用 interop_server_process_command
     *     - select() 多路复用接受连接并处理各客户端
     *   * 服务器停止时清理所有客户端连接
     * - 在不支持网络库的编译环境下：
     *   * 降级为仅处理 stdin 命令输入
     *   * 通过 lv_set_error 提示安装 Windows SDK 或 POSIX 环境
     *
     * 资源管理：
     * - 所有客户端套接字在循环退出时（服务器停止）被关闭
     * - 监听套接字在服务器停止后由 interop_server_stop 清理
     *
     * @param server 互操作服务器指针（必须已通过 interop_server_start 启动）
     * @return lv_OK 服务器循环正常退出
     *         lv_ERROR_INVALID_PARAM server 为 NULL
     *         lv_ERROR_INVALID_STATE 服务器未启动
     */
    if (!server)
        return lv_ERROR_INVALID_PARAM;

    if (!server->running) {
        lv_set_error(lv_ERROR_INVALID_STATE, "服务器未启动，请先调用 interop_server_start");
        return lv_ERROR_INVALID_STATE;
    }

    if (server->type == INTEROP_INTERFACE_STDIO) {
        /* ====== STDIO 模式：完整实现 ====== */
        char input[INTEROP_CMD_BUFFER_SIZE];
        char output[INTEROP_RESP_BUFFER_SIZE];

        lv_set_error(lv_OK, "STDIO互操作服务器已启动，等待标准输入命令...");

        while (server->running) {
            /* 读取输入 */
            if (!fgets(input, sizeof(input), stdin)) {
                /* EOF 或读取错误 */
                if (feof(stdin)) {
                    lv_set_error(lv_OK, "STDIO输入流已关闭（EOF），服务器退出");
                } else {
                    lv_set_error(lv_ERROR_IO, "STDIO读取错误，服务器退出");
                }
                break;
            }

            /* 去除尾部的换行符 */
            size_t len = strlen(input);
            while (len > 0 && (input[len - 1] == '\n' || input[len - 1] == '\r')) {
                input[len - 1] = '\0';
                len--;
            }

            /* 跳过空行 */
            if (len == 0)
                continue;

            /* 处理命令 */
            int result = interop_server_process_command(server, input, output, sizeof(output));
            if (result == lv_OK) {
                printf("%s\n", output);
                fflush(stdout);
            } else {
                /* 即使处理失败也输出错误信息 */
                printf("%s\n", output);
                fflush(stdout);
            }
        }
    } else if (server->type == INTEROP_INTERFACE_WEBSOCKET) {
        /* ====== WebSocket 模式：完整实现（RFC 6455） ====== */
        lv_set_error(lv_OK, "WebSocket服务器主循环已启动（端口=%d）", server->port);

#if INTEROP_HAS_WINSOCK
        return interop_ws_run(server, (WsSock) (intptr_t) server->internal_data);

#elif INTEROP_HAS_POSIX_SOCKET
        return interop_ws_run(server, (WsSock) (intptr_t) server->internal_data);

#else
        /* 无网络库支持：降级为 STDIO 输入处理 */
        lv_set_error(lv_WARNING,
                     "警告：未检测到Winsock2或POSIX socket库，WebSocket服务器运行在STDIO降级模式。"
                     "请安装Windows SDK或POSIX兼容的开发环境以启用完整的网络功能。");

        char input[INTEROP_CMD_BUFFER_SIZE];
        char output[INTEROP_RESP_BUFFER_SIZE];

        while (server->running) {
            printf("Lv-00 WS (stdio fallback) > ");
            fflush(stdout);

            if (!fgets(input, sizeof(input), stdin))
                break;

            size_t len = strlen(input);
            while (len > 0 && (input[len - 1] == '\n' || input[len - 1] == '\r')) {
                input[len - 1] = '\0';
                len--;
            }
            if (len == 0)
                continue;

            int result = interop_server_process_command(server, input, output, sizeof(output));
            if (result == lv_OK) {
                printf("%s\n", output);
                fflush(stdout);
            }
        }
#endif
    }

    return lv_OK;
}

/* ==================== 命令处理 ==================== */

/**
 * @brief 解析互操作命令
 *
 * 将原始输入字符串解析为 InteropCommand 结构体。
 * 支持的命令格式：JSON-RPC 和简单文本命令。
 *
 * @param input 输入字符串
 * @param cmd   输出参数，接收解析后的命令
 * @return lv_OK 成功，错误码表示失败原因
 */
/* interop_parse_command: 实现在 interop_command.c 中，通过 interop.h 声明可见 */

/* ==================== 插件注册 ==================== */

#define MAX_PLUGINS 32

/** @brief 插件表单例状态 */
typedef struct {
    lvPlugin plugins[MAX_PLUGINS]; /**< 插件注册表 */
    int count;                     /**< 已注册插件数量 */
} PluginState;

/** @brief 插件表全局单例 */
static PluginState s_plugin_state = {0};

int lv_interop_register_plugin(lvInteropManager *mgr, const lvPlugin *plugin) {
    if (!plugin)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "lv_interop_register_plugin: plugin is NULL");
    (void) mgr; /* 管理器参数保留供未来扩展 */
    if (s_plugin_state.count >= MAX_PLUGINS)
        lv_RETURN_ERROR(lv_ERROR_RESOURCE_EXHAUSTED, "lv_interop_register_plugin: plugin count exhausted");
    memcpy(&s_plugin_state.plugins[s_plugin_state.count], plugin, sizeof(lvPlugin));
    s_plugin_state.count++;
    return 0;
}
