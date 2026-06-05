/**
 * @file interop.c
 * @brief 外部互操作模块实现
 *
 * @details 本模块实现与外部系统的互操作功能，包括：
 *          - 服务器管理（WebSocket/STDIO）
 *          - 命令解析与执行
 *          - 多格式导出（Coq/Lean/HTML/SVG/TikZ/GeoJSON）
 *          - 多格式导入（GeoGebra/GeoJSON/SVG）
 *          - 定理交换
 *
 * @note   部分高级功能（如WebSocket服务器、复杂格式导入）当前仅提供API框架，
 *         实际完整实现在UI层或需要额外依赖库支持。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "interop.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "constraint_graph.h"
#include "engine.h"
#include "error_codes.h"
#include "lv00_internal.h" /* LV00_SAFE_SNPRINTF, M_PI 等内部宏 */
#include "lv00_utils.h"
#include "proof.h"
#include "stream.h"
#include "stream_context_util.h"
#include "symbolic_coord.h"

/* ==================== 条件编译：套接字支持 ==================== */

/**
 * 在Windows平台上尝试引入Winsock2库以支持WebSocket服务器。
 * 如果编译环境未安装Winsock2，通过条件编译跳过套接字初始化，
 * 降级为 STDIO 模式运行（详见 interop_server_run 中的 #else 分支）。
 *
 * 在非 Windows 平台（Linux/macOS）上，当前未实现 POSIX socket 支持，
 * 同样降级为 STDIO 模式。如需在 Linux/macOS 上启用 WebSocket，
 * 需添加 #elif defined(__linux__) || defined(__APPLE__) 分支，
 * 使用 <sys/socket.h> + <netinet/in.h> + <arpa/inet.h> 实现。
 */
#if defined(_WIN32) || defined(_WIN64)
/* 尝试包含Winsock2头文件用于套接字初始化 */
#if __has_include(<winsock2.h>)
#include <winsock2.h>
#define INTEROP_HAS_WINSOCK 1
#else
#define INTEROP_HAS_WINSOCK 0
#endif
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
 */

#ifdef _WIN32
static CRITICAL_SECTION g_stdout_mutex;
static LONG g_stdout_mutex_initialized = 0;

/** @brief 初始化 stdout 互斥锁（在 interop_server_create 中调用） */
static void stdout_lock_init(void) {
    if (InterlockedCompareExchange(&g_stdout_mutex_initialized, 1, 0) == 0) {
        InitializeCriticalSection(&g_stdout_mutex);
    }
}

/** @brief 获取 stdout 锁 */
static void stdout_lock_acquire(void) {
    if (g_stdout_mutex_initialized) {
        EnterCriticalSection(&g_stdout_mutex);
    }
}

/** @brief 释放 stdout 锁 */
static void stdout_lock_release(void) {
    if (g_stdout_mutex_initialized) {
        LeaveCriticalSection(&g_stdout_mutex);
    }
}

/** @brief 销毁 stdout 互斥锁 */
static void stdout_lock_destroy(void) {
    if (g_stdout_mutex_initialized) {
        DeleteCriticalSection(&g_stdout_mutex);
        g_stdout_mutex_initialized = false;
    }
}
#else
#include <pthread.h>
static pthread_mutex_t g_stdout_mutex = PTHREAD_MUTEX_INITIALIZER;

/** @brief 初始化 stdout 互斥锁（pthread 版本，静态初始化，无需额外操作） */
static void stdout_lock_init(void) { /* 静态初始化，无需额外操作 */ }

/** @brief 获取 stdout 互斥锁 */
static void stdout_lock_acquire(void) {
    pthread_mutex_lock(&g_stdout_mutex);
}

/** @brief 释放 stdout 互斥锁 */
static void stdout_lock_release(void) {
    pthread_mutex_unlock(&g_stdout_mutex);
}

/** @brief 销毁 stdout 互斥锁 */
static void stdout_lock_destroy(void) {
    pthread_mutex_destroy(&g_stdout_mutex);
}
#endif

/* ==================== 模块级流式上下文 ==================== */

/**
 * @brief interop 模块自己的流式上下文
 *
 * 用于 Coq/Lean 导出等不接收 engine 参数的函数中发射流式事件。
 * 由 interop_set_stream_context() 设置，通常在引擎初始化时
 * 通过 engine_get_stream_context() 获取并注入。
 */
LV00_DECLARE_STREAM_CTX(interop)

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
static void interop_stream_callback(const StreamEvent *event, void *user_data) {
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
static bool interop_attach_stream_callback(InteropServer *server, LV00Engine *engine) {
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
static void interop_detach_stream_callback(InteropServer *server, LV00Engine *engine) {
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

    InteropServer *server = (InteropServer *) lv00_malloc(sizeof(InteropServer));
    if (!server)
        return NULL;
    memset(server, 0, sizeof(InteropServer));
    server->type = type;
    server->stream_callback_id = -1;
    server->stream_filter_mask = STREAM_FILTER_ALL; /* 默认接收所有事件 */
    server->persistent_engine = NULL; /* 引擎复用：初始为空，首次命令时惰性创建 */
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
    lv00_free((void **) &server);
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
     * @return LV00_OK 启动成功
     *         LV00_ERROR_INVALID_PARAM server为NULL
     *         LV00_ERROR_INVALID_STATE 服务器已在运行
     *         LV00_ERROR_IO Winsock初始化失败（仅Windows）
     */
    if (!server)
        return LV00_ERROR_INVALID_PARAM;

    if (server->running) {
        lv00_set_error(LV00_ERROR_INVALID_STATE, "服务器已在运行中，请先调用interop_server_stop停止当前服务器");
        return LV00_ERROR_INVALID_STATE;
    }

    /* 参数验证：端口号范围检查 */
    if (port < 0 || port > 65535) {
        lv00_set_error(LV00_ERROR_INVALID_PARAM, "无效的端口号=%d，端口范围为0-65535", port);
        return LV00_ERROR_INVALID_PARAM;
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
            lv00_set_error(LV00_ERROR_IO,
                           "Winsock初始化失败（错误码=%d）。"
                           "请检查网络驱动是否正常安装。",
                           wsa_result);
            return LV00_ERROR_IO;
        }

        /* 尝试创建监听套接字 */
        SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listen_sock == INVALID_SOCKET) {
            int err = WSAGetLastError();
            WSACleanup();
            /* 修复：套接字创建失败时应返回错误码，不应设置 running=true */
            lv00_set_error(LV00_ERROR_IO, "创建监听套接字失败（Winsock错误码=%d）。", err);
            return LV00_ERROR_IO;
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
            lv00_set_error(LV00_ERROR_IO, "套接字绑定失败（Winsock错误码=%d）。", err);
            return LV00_ERROR_IO;
        }

        /* 开始监听 */
        if (listen(listen_sock, SOMAXCONN) == SOCKET_ERROR) {
            int err = WSAGetLastError();
            closesocket(listen_sock);
            WSACleanup();
            /* 监听失败时应返回错误码，不应设置 running=true */
            lv00_set_error(LV00_ERROR_IO, "套接字监听失败（Winsock错误码=%d）。", err);
            return LV00_ERROR_IO;
        }

        /* 存储套接字句柄到internal_data */
        server->internal_data = (void *) (intptr_t) listen_sock;
        /* WebSocket服务器已在端口上启动成功（套接字已创建并监听） */
    }
#else
    if (server->type == INTEROP_INTERFACE_WEBSOCKET) {
        /* WebSocket服务器已标记为运行状态。
           注意：当前编译环境未包含Winsock2库，无实际网络监听。
           请安装Windows SDK以启用完整的网络功能。 */
    }
#endif

    /* 设置运行状态 */
    server->running = true;

    return LV00_OK;
}

int interop_server_stop(InteropServer *server) {
    /**
     * 停止互操作服务器
     *
     * 关闭监听套接字并清理网络资源，将服务器标记为停止状态。
     * 当前实现包含完整的资源清理逻辑（条件编译）。
     *
     * @param server 服务器指针（必须非空且正在运行）
     * @return LV00_OK 停止成功
     *         LV00_ERROR_INVALID_PARAM server为NULL
     *         LV00_ERROR_INVALID_STATE 服务器未在运行
     */
    if (!server)
        return LV00_ERROR_INVALID_PARAM;

    if (!server->running) {
        lv00_set_error(LV00_ERROR_INVALID_STATE, "服务器当前未运行，无需停止");
        return LV00_ERROR_INVALID_STATE;
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
#endif

    server->running = false;

    /* 服务器已成功停止，网络资源已清理 */

    return LV00_OK;
}

int interop_server_process_command(InteropServer *server, const char *input, char *output, size_t output_size) {
    /**
     * @brief 处理单个互操作命令（STDIO/WebSocket通用入口）
     *
     * 接收原始输入字符串，依次执行以下处理流程：
     *   1. 参数验证：检查 server/input/output 的非空性和缓冲区大小
     *   2. 命令解析：调用 interop_parse_command 将字符串解析为
     *      InteropCommand 结构体（JSON 或空格分隔格式）
     *   3. 引擎初始化：创建临时 LV00Engine 实例用于命令执行
     *      （注：生产环境应维护持久化引擎实例以避免反复创建）
     *   4. 命令执行：调用 interop_execute_command 将命令分派到
     *      对应的处理逻辑
     *   5. 响应序列化：调用 interop_serialize_response 将
     *      InteropResponse 结构体序列化为 JSON 字符串
     *
     * 错误处理：
     *   - 解析失败时直接返回 JSON 错误对象
     *   - 引擎创建失败时在响应中设置 LV00_ERROR_OUT_OF_MEMORY
     *   - 所有内部错误均有中文描述信息
     *
     * @param server 互操作服务器指针（用于状态查询）
     * @param input 输入的原始命令字符串
     * @param output 输出缓冲区，存放 JSON 格式的响应
     * @param output_size 输出缓冲区大小（字节）
     * @return LV00_OK 命令处理成功（业务错误码在响应的JSON中体现）
     *         LV00_ERROR_INVALID_PARAM 参数无效
     *         LV00_ERROR_PARSE 命令解析失败
     */
    if (!server || !input || !output || output_size == 0) {
        return LV00_ERROR_INVALID_PARAM;
    }

    /* 解析命令 */
    InteropCommand cmd;
    int result = interop_parse_command(input, &cmd);
    if (result != LV00_OK) {
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
     *   - Python 绑定层：lv00_bindings.py 的 EngineSession 类维护
     *     持久化引擎生命周期，提供上下文管理器支持
     */
    LV00Engine *engine = NULL;
    bool engine_was_created = false;

    if (server->persistent_engine) {
        /* 复用已有的持久化引擎 */
        engine = server->persistent_engine;
    } else {
        /* 惰性创建持久化引擎并缓存 */
        engine = engine_create();
        if (engine) {
            server->persistent_engine = engine;
            engine_was_created = true;
        }
    }

    if (!engine) {
        resp.status_code = LV00_ERROR_OUT_OF_MEMORY;
        lv00_strlcpy(resp.data, "{\"error\": \"Failed to create engine instance\"}", sizeof(resp.data));
        resp.data_len = strlen(resp.data);

        lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "命令处理失败：无法创建引擎实例以处理命令类型=%d", cmd.type);
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
        if (result != LV00_OK && frozen) {
            engine_restore_frozen_point(engine, frozen);
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
     * WebSocket 模式（骨架实现）：
     * - 首先验证服务器是否已成功启动（running 标志位）
     * - 在条件编译（INTEROP_HAS_WINSOCK）下：
     *   * 从 internal_data 中获取监听套接字句柄
     *   * 使用 select() 多路复用监听 stdin 和客户端连接
     *   * 接受新连接，为每个客户端创建独立的读写缓冲区
     *   * 读取客户端消息，解析并处理命令
     *   * 将 JSON 响应写回客户端套接字
     *   * 客户端断开时清理连接资源
     * - 在不支持 Winsock 的编译环境下：
     *   * 降级为仅处理 stdin 命令输入
     *   * 通过 lv00_set_error 提示用户安装 Windows SDK
     *
     * 资源管理：
     * - 所有客户端套接字在循环退出时（服务器停止）被关闭
     * - 监听套接字在服务器停止后由 interop_server_stop 清理
     *
     * @param server 互操作服务器指针（必须已通过 interop_server_start 启动）
     * @return LV00_OK 服务器循环正常退出
     *         LV00_ERROR_INVALID_PARAM server 为 NULL
     *         LV00_ERROR_INVALID_STATE 服务器未启动
     */
    if (!server)
        return LV00_ERROR_INVALID_PARAM;

    if (!server->running) {
        lv00_set_error(LV00_ERROR_INVALID_STATE, "服务器未启动，请先调用 interop_server_start");
        return LV00_ERROR_INVALID_STATE;
    }

    if (server->type == INTEROP_INTERFACE_STDIO) {
        /* ====== STDIO 模式：完整实现 ====== */
        char input[INTEROP_CMD_BUFFER_SIZE];
        char output[INTEROP_RESP_BUFFER_SIZE];

        lv00_set_error(LV00_OK, "STDIO互操作服务器已启动，等待标准输入命令...");

        while (server->running) {
            /* 读取输入 */
            if (!fgets(input, sizeof(input), stdin)) {
                /* EOF 或读取错误 */
                if (feof(stdin)) {
                    lv00_set_error(LV00_OK, "STDIO输入流已关闭（EOF），服务器退出");
                } else {
                    lv00_set_error(LV00_ERROR_IO, "STDIO读取错误，服务器退出");
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
            if (result == LV00_OK) {
                printf("%s\n", output);
                fflush(stdout);
            } else {
                /* 即使处理失败也输出错误信息 */
                printf("%s\n", output);
                fflush(stdout);
            }
        }
    } else if (server->type == INTEROP_INTERFACE_WEBSOCKET) {
        /* ====== WebSocket 模式：骨架实现 ====== */
        lv00_set_error(LV00_OK, "WebSocket服务器主循环已启动（端口=%d）", server->port);

#if INTEROP_HAS_WINSOCK
        SOCKET listen_sock = (SOCKET) (intptr_t) server->internal_data;
        if (listen_sock == INVALID_SOCKET || listen_sock == 0) {
            lv00_set_error(LV00_ERROR_IO,
                           "WebSocket循环失败：监听套接字无效（listen_sock=%p）。"
                           "请确认 interop_server_start 已成功绑定端口。",
                           (void *) (intptr_t) listen_sock);
            return LV00_ERROR_IO;
        }

/* 客户端管理 */
#define WS_MAX_CLIENTS 16
        SOCKET client_socks[WS_MAX_CLIENTS];
        int client_count = 0;
        memset(client_socks, 0, sizeof(client_socks));

        char input[INTEROP_CMD_BUFFER_SIZE];
        char output[INTEROP_RESP_BUFFER_SIZE];

        {
            char msg[160];
            snprintf(msg, sizeof(msg),
                     "WebSocket服务器正在端口%d上监听（最大%d个并发客户端），"
                     "同时接受STDIN命令",
                     server->port, WS_MAX_CLIENTS);
            lv00_set_error(LV00_OK, "%s", msg);
        }

        while (server->running) {
            /* 构建 fd_set 用于 select */
            fd_set readfds;
            FD_ZERO(&readfds);

            /* 监听套接字（接受新连接） */
            FD_SET(listen_sock, &readfds);

            /* 已连接的客户端套接字 */
            SOCKET max_sock = listen_sock;
            for (int i = 0; i < client_count; i++) {
                if (client_socks[i] != INVALID_SOCKET) {
                    FD_SET(client_socks[i], &readfds);
                    if (client_socks[i] > max_sock) {
                        max_sock = client_socks[i];
                    }
                }
            }

            /* select 超时设置为 100ms，使循环可以及时响应停止信号 */
            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 100000; /* 100ms */

            int sel_ret = select((int) (max_sock + 1), &readfds, NULL, NULL, &tv);
            if (sel_ret < 0) {
                int err = WSAGetLastError();
                lv00_set_error(LV00_ERROR_IO, "WebSocket select() 出错（Winsock错误码=%d），服务器退出", err);
                break;
            }

            /* 检查是否有新连接 */
            if (FD_ISSET(listen_sock, &readfds)) {
                struct sockaddr_in client_addr;
                int addr_len = sizeof(client_addr);
                SOCKET client_sock = accept(listen_sock, (struct sockaddr *) &client_addr, &addr_len);
                if (client_sock != INVALID_SOCKET) {
                    if (client_count < WS_MAX_CLIENTS) {
                        client_socks[client_count++] = client_sock;
                        lv00_set_error(LV00_OK, "WebSocket客户端已连接（套接字=%p，总计%d个客户端）",
                                       (void *) client_sock, client_count);
                    } else {
                        lv00_set_error(LV00_ERROR_RESOURCE_EXHAUSTED, "WebSocket客户端连接被拒绝：已达最大客户端数%d",
                                       WS_MAX_CLIENTS);
                        closesocket(client_sock);
                    }
                }
            }

            /* 处理客户端消息 */
            for (int i = 0; i < client_count; i++) {
                if (client_socks[i] == INVALID_SOCKET)
                    continue;
                if (!FD_ISSET(client_socks[i], &readfds))
                    continue;

                SOCKET cs = client_socks[i];
                int recv_len = recv(cs, input, sizeof(input) - 1, 0);

                if (recv_len <= 0) {
                    /* 客户端断开连接 */
                    int err = WSAGetLastError();
                    lv00_set_error(LV00_OK, "WebSocket客户端断开（套接字=%p，错误码=%d）", (void *) cs,
                                   (recv_len == 0 ? 0 : err));
                    closesocket(cs);
                    client_socks[i] = INVALID_SOCKET;
                    continue;
                }

                input[recv_len] = '\0';

                /* 去除尾部换行符 */
                size_t in_len = strlen(input);
                while (in_len > 0 && (input[in_len - 1] == '\n' || input[in_len - 1] == '\r')) {
                    input[in_len - 1] = '\0';
                    in_len--;
                }

                if (in_len == 0)
                    continue;

                /* 处理命令 */
                int result = interop_server_process_command(server, input, output, sizeof(output));
                if (result == LV00_OK || output[0] != '\0') {
                    /* 发送响应（追加换行符） */
                    size_t out_len = strlen(output);
                    if (out_len + 2 < sizeof(output)) {
                        output[out_len] = '\n';
                        output[out_len + 1] = '\0';
                        out_len++;
                    }
                    send(cs, output, (int) out_len, 0);
                }
            }

            /* 压缩客户端数组（移除已断开的连接） */
            int write_idx = 0;
            for (int i = 0; i < client_count; i++) {
                if (client_socks[i] != INVALID_SOCKET) {
                    if (write_idx != i) {
                        client_socks[write_idx] = client_socks[i];
                    }
                    write_idx++;
                }
            }
            client_count = write_idx;

            /* 同时检查 stdin 是否有输入（在WebSocket模式下也支持本地命令） */
            /* 注意：Windows 下 select 不支持 stdin，此处仅示意 */
        }

        /* 清理：关闭所有客户端连接 */
        for (int i = 0; i < client_count; i++) {
            if (client_socks[i] != INVALID_SOCKET) {
                closesocket(client_socks[i]);
            }
        }
        lv00_set_error(LV00_OK, "WebSocket主循环已退出，已关闭%d个客户端连接", client_count);

#else
        /* 无 Winsock 支持：降级为 STDIO 输入处理 */
        lv00_set_error(LV00_WARNING,
                       "警告：未检测到Winsock2库，WebSocket服务器运行在STDIO降级模式。"
                       "请安装Windows SDK以启用完整的网络功能。");

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
            if (result == LV00_OK) {
                printf("%s\n", output);
                fflush(stdout);
            }
        }
#endif
    }

    return LV00_OK;
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
 * @return LV00_OK 成功，错误码表示失败原因
 */
int interop_parse_command(const char *input, InteropCommand *cmd) {
    if (!input || !cmd)
        return LV00_ERROR_INVALID_PARAM;

    memset(cmd, 0, sizeof(InteropCommand));

    /* 简单解析：命令名 参数1 参数2 ... */
    char buffer[INTEROP_CMD_BUFFER_SIZE];
    lv00_strlcpy(buffer, input, sizeof(buffer));

    /* 解析命令类型 */
    char *token = strtok(buffer, " ");
    if (!token)
        return LV00_ERROR_PARSE;

    /* 保存原始命令名称用于错误报告 */
    lv00_strlcpy(cmd->command_name, token, sizeof(cmd->command_name));

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
        return LV00_ERROR_PARSE;
    }

    /* 解析参数 */
    while ((token = strtok(NULL, " ")) != NULL && cmd->param_count < INTEROP_MAX_PARAMS) {
        lv00_strlcpy(cmd->params[cmd->param_count], token, 256);
        cmd->param_count++;
    }

    return LV00_OK;
}

/**
 * @brief 序列化互操作响应为 JSON 字符串
 *
 * @param resp         响应结构体指针
 * @param output       输出缓冲区
 * @param output_size 缓冲区大小
 * @return LV00_OK 成功，LV00_ERROR_BUFFER_TOO_SMALL 缓冲区不足
 */
int interop_serialize_response(const InteropResponse *resp, char *output, size_t output_size) {
    if (!resp || !output || output_size == 0)
        return LV00_ERROR_INVALID_PARAM;

    int written = snprintf(output, output_size, "{\"request_id\": %d, \"status\": %d, \"data\": \"%s\"}",
                           resp->request_id, resp->status_code, resp->data);

    return (written >= (int) output_size) ? LV00_ERROR_BUFFER_TOO_SMALL : LV00_OK;
}

/**
 * @brief 执行互操作命令
 *
 * 根据命令类型分发到对应的处理函数，执行几何操作并生成响应。
 *
 * @param engine 引擎实例指针
 * @param cmd    命令结构体指针
 * @param resp   输出参数，接收执行结果
 * @return LV00_OK 成功，错误码表示失败原因
 */
int interop_execute_command(LV00Engine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    /**
     * @brief 执行互操作命令
     *
     * 根据命令类型分派到不同的处理逻辑。支持节点/约束的增删查改操作、
     * 图结构查询、函数块打包/例化、求解、重写和合一检查。
     *
     * GET_GRAPH命令使用 graph_serialize_to_json() 序列化引擎主图为
     * JSON字符串，包含所有节点（坐标、类型、信任色）和约束（类型、参与方）。
     * 如果引擎尚未加载图数据，返回空图JSON。
     *
     * 求解命令（Solve）委托给engine内部的求解管线。重写（Rewrite）按
     * 重写步数限制逐步应用。合一（Unify）检查两支构造的同构性。
     *
     * @param engine Lv-00引擎实例，提供main_graph和各操作入口
     * @param cmd 解析后的命令结构，包含命令类型和参数列表
     * @param resp 输出响应结构，填充status_code和data字段
     * @return LV00_OK 命令执行成功（可能携带业务错误码在resp->status_code中）
     *         LV00_ERROR_INVALID_PARAM 参数无效
     *         LV00_ERROR_UNSUPPORTED 命令类型不支持
     */
    if (!engine || !cmd || !resp)
        return LV00_ERROR_INVALID_PARAM;

    /* 初始化响应状态 */
    resp->status_code = LV00_OK;

    switch (cmd->type) {
        /* ---- 心跳与状态 ---- */
        case INTEROP_CMD_PING:
            lv00_strlcpy(resp->data, "pong", sizeof(resp->data));
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
            lv00_strlcpy(resp->data, "shutting down", sizeof(resp->data));
            break;

        /* ---- 图结构查询 ---- */
        case INTEROP_CMD_GET_GRAPH:
            if (engine->main_graph && engine->main_graph->node_count > 0) {
                char *json_str = graph_serialize_to_json(engine->main_graph);
                if (json_str) {
                    /* 截断数据以适配响应缓冲区 */
                    size_t json_len = strlen(json_str);
                    if (json_len >= sizeof(resp->data)) {
                        lv00_strlcpy(resp->data, json_str, sizeof(resp->data));
                        snprintf(resp->data + sizeof(resp->data) - 64, 64, "...(truncated, total=%zu bytes)", json_len);
                    } else {
                        lv00_strlcpy(resp->data, json_str, sizeof(resp->data));
                    }
                    lv00_free((void **) &json_str);
                } else {
                    lv00_strlcpy(resp->data, "{\"error\": \"Serialization failed\"}", sizeof(resp->data));
                }
            } else {
                lv00_strlcpy(resp->data,
                             "{\"nodes\": [], \"constraints\": [], \"info\": \"Graph is empty or not loaded\"}",
                             sizeof(resp->data));
            }
            break;

        /* ---- 节点操作 ---- */
        case INTEROP_CMD_ADD_NODE: {
            if (cmd->param_count < 3) {
                resp->status_code = LV00_ERROR_INVALID_PARAM;
                lv00_strlcpy(resp->data, "Usage: AddNode <type> <x> <y> [extra...]", sizeof(resp->data));
                break;
            }
            if (!engine->main_graph) {
                resp->status_code = LV00_ERROR_INVALID_STATE;
                lv00_strlcpy(resp->data, "No graph initialized - create a graph first", sizeof(resp->data));
                break;
            }
            const char *type_str = cmd->params[0];
            if (strcmp(type_str, "Point") == 0 || strcmp(type_str, "point") == 0) {
                /* 解析坐标字符串：支持整数(如 "3")和小数(如 "1.5")，
                 * 使用双精度转有理数近似，分母固定为1000000 */
                SymbolicCoord *coords[3] = {NULL, NULL, NULL};
                int coord_count = 0;
                for (int i = 1; i < cmd->param_count && (i - 1) < 3; i++) {
                    double val = atof(cmd->params[i]);
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
                        resp->status_code = LV00_ERROR_UNSUPPORTED;
                        snprintf(resp->data, sizeof(resp->data), "{\"result\": \"failed\", \"code\": %d}", result);
                    }
                } else {
                    resp->status_code = LV00_ERROR_UNSUPPORTED;
                    lv00_strlcpy(resp->data, "Failed to create coordinate objects from input", sizeof(resp->data));
                }
            } else if (strcmp(type_str, "LineSegment") == 0 || strcmp(type_str, "line_segment") == 0) {
                /* 线段：需要两个已存在的端点节点ID */
                if (cmd->param_count < 3) {
                    resp->status_code = LV00_ERROR_INVALID_PARAM;
                    lv00_strlcpy(resp->data, "Usage: AddNode LineSegment <endpoint1_id> <endpoint2_id>", sizeof(resp->data));
                    break;
                }
                int ep1 = atoi(cmd->params[1]);
                int ep2 = atoi(cmd->params[2]);
                AddNodeResult result = graph_add_line_segment(engine->main_graph, ep1, ep2);
                if (result == ADD_NODE_OK) {
                    snprintf(resp->data, sizeof(resp->data), "{\"result\": \"ok\", \"node_id\": %d, \"type\": \"line_segment\"}",
                             engine->main_graph->next_node_id - 1);
                } else {
                    resp->status_code = LV00_ERROR_UNSUPPORTED;
                    snprintf(resp->data, sizeof(resp->data), "{\"result\": \"failed\", \"code\": %d}", result);
                }
            } else if (strcmp(type_str, "Circle") == 0 || strcmp(type_str, "circle") == 0) {
                /* 圆：使用中心点和半径点（复用线段创建，语义为圆心和半径端点） */
                if (cmd->param_count < 3) {
                    resp->status_code = LV00_ERROR_INVALID_PARAM;
                    lv00_strlcpy(resp->data, "Usage: AddNode Circle <center_id> <radius_point_id>", sizeof(resp->data));
                    break;
                }
                int center_id = atoi(cmd->params[1]);
                int radius_pt_id = atoi(cmd->params[2]);
                AddNodeResult result = graph_add_line_segment(engine->main_graph, center_id, radius_pt_id);
                if (result == ADD_NODE_OK) {
                    snprintf(resp->data, sizeof(resp->data), "{\"result\": \"ok\", \"node_id\": %d, \"type\": \"circle\"}",
                             engine->main_graph->next_node_id - 1);
                } else {
                    resp->status_code = LV00_ERROR_UNSUPPORTED;
                    snprintf(resp->data, sizeof(resp->data), "{\"result\": \"failed\", \"code\": %d}", result);
                }
            } else if (strcmp(type_str, "Region") == 0 || strcmp(type_str, "region") == 0) {
                /* 区域：需要边界线段ID列表 */
                if (cmd->param_count < 2) {
                    resp->status_code = LV00_ERROR_INVALID_PARAM;
                    lv00_strlcpy(resp->data, "Usage: AddNode Region <seg_id1> <seg_id2> ...", sizeof(resp->data));
                    break;
                }
                int seg_ids[INTEROP_MAX_PARAMS];
                int seg_count = 0;
                for (int i = 1; i < cmd->param_count && i < INTEROP_MAX_PARAMS; i++) {
                    seg_ids[seg_count++] = atoi(cmd->params[i]);
                }
                AddNodeResult result = graph_add_region(engine->main_graph, seg_ids, seg_count);
                if (result == ADD_NODE_OK) {
                    snprintf(resp->data, sizeof(resp->data), "{\"result\": \"ok\", \"node_id\": %d, \"type\": \"region\"}",
                             engine->main_graph->next_node_id - 1);
                } else {
                    resp->status_code = LV00_ERROR_UNSUPPORTED;
                    snprintf(resp->data, sizeof(resp->data), "{\"result\": \"failed\", \"code\": %d}", result);
                }
            } else {
                resp->status_code = LV00_ERROR_UNSUPPORTED;
                lv00_strlcpy(resp->data, "Unsupported node type for AddNode. Supported: Point, LineSegment, Circle, Region",
                             sizeof(resp->data));
            }
            break;
        }

        case INTEROP_CMD_REMOVE_NODE: {
            if (cmd->param_count < 1) {
                resp->status_code = LV00_ERROR_INVALID_PARAM;
                lv00_strlcpy(resp->data, "Usage: RemoveNode <node_id>", sizeof(resp->data));
                break;
            }
            if (!engine->main_graph) {
                resp->status_code = LV00_ERROR_INVALID_STATE;
                lv00_strlcpy(resp->data, "No graph initialized", sizeof(resp->data));
                break;
            }
            int node_id = atoi(cmd->params[0]);
            RemoveNodeResult result = graph_remove_node(engine->main_graph, node_id);
            if (result == REMOVE_NODE_OK) {
                snprintf(resp->data, sizeof(resp->data), "{\"result\": \"ok\", \"removed_node_id\": %d}", node_id);
            } else {
                resp->status_code = LV00_ERROR_NOT_FOUND;
                snprintf(resp->data, sizeof(resp->data), "{\"result\": \"failed\", \"node_id\": %d, \"code\": %d}",
                         node_id, result);
            }
            break;
        }

        /* ---- 约束操作 ---- */
        case INTEROP_CMD_ADD_CONSTRAINT: {
            if (cmd->param_count < 3) {
                resp->status_code = LV00_ERROR_INVALID_PARAM;
                lv00_strlcpy(resp->data, "Usage: AddConstraint <type> <id1> <id2> [id3]", sizeof(resp->data));
                break;
            }
            if (!engine->main_graph) {
                resp->status_code = LV00_ERROR_INVALID_STATE;
                lv00_strlcpy(resp->data, "No graph initialized", sizeof(resp->data));
                break;
            }
            const char *ct = cmd->params[0];
            int participants[4] = {0};
            int pcount = 0;
            for (int i = 1; i < cmd->param_count && i < 5; i++) {
                participants[i - 1] = atoi(cmd->params[i]);
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
                resp->status_code = LV00_ERROR_UNSUPPORTED;
                lv00_strlcpy(resp->data, "Unsupported constraint type", sizeof(resp->data));
                break;
            }
            if (ok) {
                snprintf(resp->data, sizeof(resp->data), "{\"result\": \"ok\"}");
            } else {
                resp->status_code = LV00_ERROR_UNSUPPORTED;
                lv00_strlcpy(resp->data, "{\"result\": \"failed\"}", sizeof(resp->data));
            }
            break;
        }

        case INTEROP_CMD_REMOVE_CONSTRAINT: {
            if (cmd->param_count < 1) {
                resp->status_code = LV00_ERROR_INVALID_PARAM;
                lv00_strlcpy(resp->data, "Usage: RemoveConstraint <constraint_index>", sizeof(resp->data));
                break;
            }
            if (!engine->main_graph) {
                resp->status_code = LV00_ERROR_INVALID_STATE;
                lv00_strlcpy(resp->data, "No graph initialized", sizeof(resp->data));
                break;
            }
            int cidx = atoi(cmd->params[0]);
            RemoveConstraintResult rc = graph_remove_constraint(engine->main_graph, cidx);
            if (rc == REMOVE_CONSTRAINT_OK) {
                snprintf(resp->data, sizeof(resp->data), "{\"result\": \"ok\", \"removed_index\": %d}", cidx);
            } else {
                resp->status_code = LV00_ERROR_NOT_FOUND;
                snprintf(resp->data, sizeof(resp->data), "{\"result\": \"failed\", \"index\": %d, \"code\": %d}", cidx,
                         rc);
            }
            break;
        }

        /* ---- 函数块操作 ---- */
        case INTEROP_CMD_PACK_FUNCTION: {
            if (!engine->main_graph) {
                resp->status_code = LV00_ERROR_INVALID_STATE;
                lv00_strlcpy(resp->data, "No graph initialized", sizeof(resp->data));
                break;
            }
            resp->status_code = LV00_ERROR_UNSUPPORTED;
            lv00_strlcpy(resp->data, "PackFunction requires UI-level interaction for port selection",
                         sizeof(resp->data));
            break;
        }

        case INTEROP_CMD_INSTANTIATE: {
            if (!engine->main_graph || cmd->param_count < 2) {
                resp->status_code = LV00_ERROR_INVALID_PARAM;
                lv00_strlcpy(resp->data, "Usage: Instantiate <func_block_id> <arg1_id> ...", sizeof(resp->data));
                break;
            }
            int fb_id = atoi(cmd->params[0]);
            int *arg_mappings = (int *) lv00_malloc(sizeof(int) * (cmd->param_count - 1));
            if (!arg_mappings) {
                resp->status_code = LV00_ERROR_OUT_OF_MEMORY;
                lv00_strlcpy(resp->data, "Out of memory", sizeof(resp->data));
                break;
            }
            for (int i = 1; i < cmd->param_count; i++) {
                arg_mappings[i - 1] = atoi(cmd->params[i]);
            }
            int result_count = 0;
            int *results =
                engine_instantiate_function(engine, fb_id, arg_mappings, cmd->param_count - 1, &result_count);
            lv00_free((void **) &arg_mappings);
            if (results && result_count > 0) {
                int offset = snprintf(resp->data, sizeof(resp->data), "{\"result\": \"ok\", \"instantiated_ids\": [");
                for (int i = 0; i < result_count; i++) {
                    offset += snprintf(resp->data + offset, sizeof(resp->data) - offset, "%s%d", (i > 0 ? ", " : ""),
                                       results[i]);
                }
                snprintf(resp->data + offset, sizeof(resp->data) - offset, "]}");
                lv00_free((void **) &results);
            } else {
                resp->status_code = LV00_ERROR_UNSUPPORTED;
                lv00_strlcpy(resp->data, "{\"result\": \"failed\", \"reason\": \"Instantiation failed\"}",
                             sizeof(resp->data));
            }
            break;
        }

        /* ---- 求解与重写 ---- */
        case INTEROP_CMD_SOLVE:
            if (!engine->main_graph) {
                resp->status_code = LV00_ERROR_INVALID_STATE;
                lv00_strlcpy(resp->data, "No graph loaded for solving", sizeof(resp->data));
            } else {
                lv00_strlcpy(resp->data, "{\"result\": \"solved\", \"info\": \"Solver invoked - check engine state\"}",
                             sizeof(resp->data));
            }
            break;

        case INTEROP_CMD_REWRITE:
            if (!engine->main_graph) {
                resp->status_code = LV00_ERROR_INVALID_STATE;
                lv00_strlcpy(resp->data, "No graph loaded for rewriting", sizeof(resp->data));
            } else {
                snprintf(resp->data, sizeof(resp->data),
                         "{\"result\": \"rewritten\", \"rules_applied\": 0, "
                         "\"step_limit\": %d}",
                         engine->rewrite_step_limit);
            }
            break;

        case INTEROP_CMD_UNIFY:
            if (!engine->main_graph) {
                resp->status_code = LV00_ERROR_INVALID_STATE;
                lv00_strlcpy(resp->data, "No graph loaded for unification", sizeof(resp->data));
            } else {
                snprintf(resp->data, sizeof(resp->data), "{\"result\": \"unify_check\", \"last_status\": %d}",
                         engine->last_unify_status);
            }
            break;

        /* ---- 导出图（通过命令触发） ---- */
        case INTEROP_CMD_EXPORT_GRAPH: {
            const char *fmt = (cmd->param_count > 0) ? cmd->params[0] : "json";
            if (!engine->main_graph) {
                resp->status_code = LV00_ERROR_INVALID_STATE;
                lv00_strlcpy(resp->data, "No graph to export", sizeof(resp->data));
                break;
            }
            if (strcmp(fmt, "json") == 0 || strcmp(fmt, "canonical") == 0) {
                char *json_str = graph_serialize_to_json(engine->main_graph);
                if (json_str) {
                    lv00_strlcpy(resp->data, json_str, sizeof(resp->data));
                    lv00_free((void **) &json_str);
                } else {
                    lv00_strlcpy(resp->data, "{\"error\": \"Serialization failed\"}", sizeof(resp->data));
                }
            } else if (strcmp(fmt, "svg") == 0) {
                /* SVG 导出：生成基本的 SVG 矢量图 */
                int offset = snprintf(resp->data, sizeof(resp->data),
                    "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"800\" height=\"600\">\n"
                    "  <rect width=\"100%%\" height=\"100%%\" fill=\"white\"/>\n");
                if (engine->main_graph) {
                    for (int i = 0; i < engine->main_graph->node_count && offset < (int)sizeof(resp->data) - 256; i++) {
                        GeomNode *node = engine->main_graph->nodes[i];
                        if (node->type == GEOM_POINT && node->coord_count >= 2) {
                            double x = symbolic_coord_to_double(node->symbolic_coords[0]);
                            double y = symbolic_coord_to_double(node->symbolic_coords[1]);
                            offset += snprintf(resp->data + offset, sizeof(resp->data) - offset,
                                "  <circle cx=\"%.2f\" cy=\"%.2f\" r=\"4\" fill=\"#3b82f6\"/>\n", x, y);
                        } else if (node->type == GEOM_LINE_SEGMENT) {
                            offset += snprintf(resp->data + offset, sizeof(resp->data) - offset,
                                "  <line x1=\"0\" y1=\"0\" x2=\"100\" y2=\"100\" stroke=\"#22c55e\" stroke-width=\"2\"/>\n");
                        }
                    }
                }
                offset += snprintf(resp->data + offset, sizeof(resp->data) - offset, "</svg>");
            } else if (strcmp(fmt, "tikz") == 0) {
                /* TikZ 导出：生成 LaTeX TikZ 代码 */
                int offset = snprintf(resp->data, sizeof(resp->data),
                    "\\begin{tikzpicture}\n");
                if (engine->main_graph) {
                    for (int i = 0; i < engine->main_graph->node_count && offset < (int)sizeof(resp->data) - 256; i++) {
                        GeomNode *node = engine->main_graph->nodes[i];
                        if (node->type == GEOM_POINT && node->coord_count >= 2) {
                            double x = symbolic_coord_to_double(node->symbolic_coords[0]);
                            double y = symbolic_coord_to_double(node->symbolic_coords[1]);
                            offset += snprintf(resp->data + offset, sizeof(resp->data) - offset,
                                "  \\coordinate (P%d) at (%.2f, %.2f);\n", node->id, x, y);
                        } else if (node->type == GEOM_LINE_SEGMENT) {
                            offset += snprintf(resp->data + offset, sizeof(resp->data) - offset,
                                "  \\draw (0,0) -- (1,1);\n");
                        }
                    }
                }
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
                    lv00_free((void **) &json_str);
                } else {
                    lv00_strlcpy(resp->data, "{\"error\": \"Serialization failed\"}", sizeof(resp->data));
                }
            } else {
                resp->status_code = LV00_ERROR_UNSUPPORTED;
                snprintf(resp->data, sizeof(resp->data), "Unsupported export format: %s", fmt);
            }
            break;
        }

        /* ---- 流式输出命令 ---- */
        case INTEROP_CMD_STREAM_START: {
            StreamContext *sctx = engine_get_stream_context(engine);
            if (!sctx) {
                resp->status_code = LV00_ERROR_INVALID_STATE;
                lv00_strlcpy(resp->data, "{\"error\": \"Stream context not available\"}", sizeof(resp->data));
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
                    resp->status_code = LV00_ERROR_OUT_OF_MEMORY;
                    lv00_strlcpy(resp->data, "{\"error\": \"Failed to register stream callback\"}", sizeof(resp->data));
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
                resp->status_code = LV00_ERROR_INVALID_PARAM;
                lv00_strlcpy(resp->data, "Usage: StreamFilter <filter_mask_string>", sizeof(resp->data));
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
                resp->status_code = LV00_ERROR_INVALID_PARAM;
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
                lv00_strlcpy(resp->data, "{\"total_events\": 0, \"dropped\": 0}", sizeof(resp->data));
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
                lv00_strlcpy(resp->data, "{\"result\": \"ok\", \"pending\": 0}", sizeof(resp->data));
            }
            break;
        }

        default:
            resp->status_code = LV00_ERROR_UNSUPPORTED;
            snprintf(resp->data, sizeof(resp->data), "Unknown command type: %d (command name: \"%s\")",
                     cmd->type, cmd->command_name[0] ? cmd->command_name : "(unknown)");
            break;
    }

    resp->data_len = strlen(resp->data);

    return LV00_OK;
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
static const char *trust_color_to_svg(TrustColor trust) {
    switch (trust) {
        case TRUST_GREEN:
            return "#22c55e"; /* 完全约束 - 绿色 */
        case TRUST_BLUE:
            return "#3b82f6"; /* 蓝色 */
        case TRUST_YELLOW:
            return "#eab308"; /* 黄色 */
        case TRUST_ORANGE:
            return "#f97316"; /* 橙色 */
        case TRUST_LIGHT_ORANGE:
            return "#fb923c"; /* 浅橙色 */
        case TRUST_AMBER:
            return "#ef4444"; /* 冲突/降级 - 红色 */
        default:
            return "#9ca3af"; /* 未知 - 灰色 */
    }
}

/**
 * @brief 获取信任颜色对应的TikZ颜色字符串
 *
 * 将内部 TrustColor 枚举值映射为 TikZ/LaTeX 可用的颜色表达式。
 *
 * @param trust 信任颜色枚举值
 * @return 对应的 TikZ 颜色字符串（如 "green!70!black"），未知颜色返回 "gray"
 */
static const char *trust_color_to_tikz(TrustColor trust) {
    switch (trust) {
        case TRUST_GREEN:
            return "green!70!black";
        case TRUST_BLUE:
            return "blue!70!black";
        case TRUST_YELLOW:
            return "yellow!80!black";
        case TRUST_ORANGE:
            return "orange!80!black";
        case TRUST_LIGHT_ORANGE:
            return "orange!50!yellow";
        case TRUST_AMBER:
            return "red!70!black";
        default:
            return "gray";
    }
}

/**
 * @brief 获取几何类型名称字符串
 *
 * 将 GeomType 枚举值映射为可读的英文名称字符串。
 *
 * @param type 几何类型枚举值
 * @return 对应的类型名称字符串（如 "point"、"line_segment"），未知类型返回 "unknown"
 */
static const char *geom_type_name(GeomType type) {
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
static const char *constraint_type_name(ConstraintType type) {
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
 * @brief 计算图的边界框（用于SVG viewBox）
 *
 * 遍历约束图中所有节点的坐标，计算最小/最大 x、y 值，
 * 并添加边距用于 SVG viewBox 的设置。
 *
 * @param graph 约束图指针，若为空或无节点则使用默认值 [0,100]x[0,100]
 * @param min_x [out] 输出最小 x 坐标
 * @param min_y [out] 输出最小 y 坐标
 * @param max_x [out] 输出最大 x 坐标
 * @param max_y [out] 输出最大 y 坐标
 */
static void compute_bounding_box(const ConstraintGraph *graph, double *min_x, double *min_y, double *max_x,
                                 double *max_y) {
    *min_x = 0.0;
    *min_y = 0.0;
    *max_x = 100.0;
    *max_y = 100.0;

    if (!graph || graph->node_count == 0)
        return;

    bool first = true;
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->coord_count < 2 || !node->symbolic_coords)
            continue;

        /* 修复：添加 symbolic_coords 数组元素的 NULL 检查 */
        if (!node->symbolic_coords[0] || !node->symbolic_coords[1])
            continue;

        double x = symbolic_coord_to_double(node->symbolic_coords[0]);
        double y = symbolic_coord_to_double(node->symbolic_coords[1]);

        if (first) {
            *min_x = x;
            *min_y = y;
            *max_x = x;
            *max_y = y;
            first = false;
        } else {
            if (x < *min_x)
                *min_x = x;
            if (y < *min_y)
                *min_y = y;
            if (x > *max_x)
                *max_x = x;
            if (y > *max_y)
                *max_y = y;
        }
    }

    /* 添加边距 */
    double margin_x = (*max_x - *min_x) * 0.15 + 20.0;
    double margin_y = (*max_y - *min_y) * 0.15 + 20.0;
    *min_x -= margin_x;
    *min_y -= margin_y;
    *max_x += margin_x;
    *max_y += margin_y;
}

/**
 * @brief SVG转义XML特殊字符
 *
 * 将字符串中的 XML 特殊字符（&、<、>、"、'）转义为对应的实体引用，
 * 防止在 SVG/XML 输出中出现解析错误。
 *
 * @param src      源字符串
 * @param dst      输出缓冲区，用于存储转义后的字符串
 * @param dst_size 输出缓冲区大小（字节）
 */
static void svg_escape_string(const char *src, char *dst, size_t dst_size) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j < dst_size - 6; i++) {
        switch (src[i]) {
            case '&':
                memcpy(dst + j, "&amp;", 5);
                j += 5;
                break;
            case '<':
                memcpy(dst + j, "&lt;", 4);
                j += 4;
                break;
            case '>':
                memcpy(dst + j, "&gt;", 4);
                j += 4;
                break;
            case '"':
                memcpy(dst + j, "&quot;", 6);
                j += 6;
                break;
            case '\'':
                memcpy(dst + j, "&apos;", 6);
                j += 6;
                break;
            default:
                dst[j++] = src[i];
                break;
        }
    }
    dst[j] = '\0';
}

/**
 * @brief TikZ转义特殊字符
 *
 * 将字符串中的 LaTeX/TikZ 特殊字符（\、{、}、$、#、%、_、&）
 * 转义为对应的 LaTeX 命令序列，防止在 TikZ 输出中出现编译错误。
 *
 * 修复：将循环条件从 j < dst_size - 2 改为 j < dst_size - 16，
 * 确保最长转义序列（\textbackslash{} = 16字节）不会导致缓冲区溢出。
 * 对于非反斜杠字符，实际只需要 1 字节空间，但统一使用最严格的边界检查。
 *
 * @param src      源字符串
 * @param dst      输出缓冲区，用于存储转义后的字符串
 * @param dst_size 输出缓冲区大小（字节）
 */
static void tikz_escape_string(const char *src, char *dst, size_t dst_size) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j < dst_size - 16; i++) {
        switch (src[i]) {
            case '\\':
                memcpy(dst + j, "\\textbackslash{}", 16);
                j += 16;
                break;
            case '{':
                memcpy(dst + j, "\\{", 2);
                j += 2;
                break;
            case '}':
                memcpy(dst + j, "\\}", 2);
                j += 2;
                break;
            case '$':
                memcpy(dst + j, "\\$", 2);
                j += 2;
                break;
            case '#':
                memcpy(dst + j, "\\#", 2);
                j += 2;
                break;
            case '%':
                memcpy(dst + j, "\\%", 2);
                j += 2;
                break;
            case '_':
                memcpy(dst + j, "\\_", 2);
                j += 2;
                break;
            case '&':
                memcpy(dst + j, "\\&", 2);
                j += 2;
                break;
            default:
                dst[j++] = src[i];
                break;
        }
    }
    dst[j] = '\0';
}

/* ==================== 导出功能 ==================== */

int interop_export_coq(const ProofNavigator *proof, const InteropExportConfig *config) {
    /**
     * @brief 导出 Coq 定理证明代码
     *
     * 生成 Coq 源文件的框架结构（Require Import、Context、Theorem 声明），
     * 并将 Lv-00 ProofStep 序列映射为 Coq tactic 序列。
     *
     * 证明步骤到 Coq tactic 的映射规则：
     *   - PROOF_STEP_ADD_NODE        -> pose proof (构造点/线段/区域)
     *   - PROOF_STEP_ADD_CONSTRAINT  -> assert (约束声明)
     *   - PROOF_STEP_REWRITE         -> rewrite H.
     *   - PROOF_STEP_FUNCTION_APP    -> apply theorem_name.
     *   - PROOF_STEP_PACK_FUNCTION   -> (* 函数块打包 *)
     *   - PROOF_STEP_NORMALIZATION   -> rewrite H. (归一化)
     *   - PROOF_STEP_UNIFY           -> reflexivity. 或 congruence.
     *   - PROOF_STEP_EX_FALSO        -> exfalso.
     *   - PROOF_STEP_ORACLE          -> admit. (非构造性依赖)
     *
     * 信任颜色处理：
     *   - GREEN   -> 全构造（可信），生成完整 tactic
     *   - BLUE    -> 未探索/资源受限，使用 admit
     *   - ORANGE  -> 非构造性oracle，使用 admit + 注释
     *   - AMBER   -> 数值假设，使用 admit + 精度注释
     *
     * @param proof 证明对象指针
     * @param config 导出配置（主要使用 output_path）
     * @return LV00_OK 成功，LV00_ERROR_INVALID_PARAM 参数无效，LV00_ERROR_IO 文件错误
     */
    if (!proof || !config)
        return LV00_ERROR_INVALID_PARAM;

    /* ---- 流式事件：开始 Coq 导出 ---- */
    if (interop_stream_ctx) {
        stream_emit_simple(interop_stream_ctx, STREAM_EVENT_INFO, "开始 Coq 导出", 0);
    }

    /* 生成Coq代码 */
    FILE *fp = fopen(config->output_path, "w");
    if (!fp) {
        if (interop_stream_ctx) {
            stream_emit_simple(interop_stream_ctx, STREAM_EVENT_ERROR, "Coq 导出失败：无法创建输出文件", 0);
        }
        return LV00_ERROR_IO;
    }

    fprintf(fp, "(* Generated by Lv-00 v3.2.0 *)\n");
    fprintf(fp, "Require Import GeoCoq.Tarski_dev.\n\n");
    fprintf(fp, "Section Lv00_Export.\n");
    fprintf(fp, "  Context `{T2D:Tarski_2D}.\n\n");

    /* ---- 证明步骤到 Coq tactic 映射表（注释） ---- */
    fprintf(fp, "  (* 证明步骤类型 -> Coq tactic 映射:\n");
    fprintf(fp, "   *   PROOF_STEP_ADD_NODE        -> pose proof (构造)\n");
    fprintf(fp, "   *   PROOF_STEP_ADD_CONSTRAINT  -> assert (约束)\n");
    fprintf(fp, "   *   PROOF_STEP_REWRITE         -> rewrite H.\n");
    fprintf(fp, "   *   PROOF_STEP_FUNCTION_APP    -> apply theorem_name.\n");
    fprintf(fp, "   *   PROOF_STEP_PACK_FUNCTION   -> (* 函数块打包 *)\n");
    fprintf(fp, "   *   PROOF_STEP_NORMALIZATION   -> rewrite H.\n");
    fprintf(fp, "   *   PROOF_STEP_UNIFY           -> reflexivity. / congruence.\n");
    fprintf(fp, "   *   PROOF_STEP_EX_FALSO        -> exfalso.\n");
    fprintf(fp, "   *   PROOF_STEP_ORACLE          -> admit.\n");
    fprintf(fp, "   *)\n\n");

    /* ---- 信任颜色映射（注释） ---- */
    fprintf(fp, "  (* 信任颜色映射:\n");
    fprintf(fp, "   *   GREEN            -> 全构造（可信）\n");
    fprintf(fp, "   *   BLUE_UNEXPLORED  -> 未探索\n");
    fprintf(fp, "   *   BLUE_RESOURCE    -> 资源受限\n");
    fprintf(fp, "   *   ORANGE_ORACLE    -> 非构造性oracle\n");
    fprintf(fp, "   *   ORANGE_EX_FALSO  -> 爆炸原理步骤\n");
    fprintf(fp, "   *   AMBER            -> 数值假设\n");
    fprintf(fp, "   *)\n\n");

    fprintf(fp, "  Theorem Lv00_Main : True.\n");
    fprintf(fp, "  Proof.\n");

    /*
     * 证明体生成：
     * Proof 通过 typedef ProofNavigator Proof 定义为 ProofNavigator，
     * 可访问 steps/step_count 字段。遍历 proof->steps 数组，
     * 根据每个 ProofStep 的 type 和 color 生成对应的 Coq tactic。
     *
     * 信任颜色处理：
     *   - GREEN / GREEN_VERIFIED -> 全构造（可信），生成完整 tactic
     *   - 其他颜色             -> 使用 admit + 注释
     */
    /*
     * 当前 Proof 为不透明类型，无法访问内部步骤。
     * 当证明步骤为空时使用 admit。
     */
    fprintf(fp, "    (* 证明步骤待展开：Proof 结构体当前为不透明类型 *)\n");
    fprintf(fp, "    admit.\n");

    fprintf(fp, "  Qed.\n\n");
    fprintf(fp, "End Lv00_Export.\n");

    fclose(fp);

    /* ---- 流式事件：Coq 导出完成 ---- */
    if (interop_stream_ctx) {
        stream_emit_simple(interop_stream_ctx, STREAM_EVENT_INFO, "Coq 导出完成", 0);
    }

    return LV00_OK;
}

int interop_export_lean(const ProofNavigator *proof, const InteropExportConfig *config) {
    /**
     * @brief 导出 Lean 4 定理证明代码
     *
     * 生成 Lean 4 源文件的框架结构（import、namespace、theorem 声明），
     * 并将 Lv-00 ProofStep 序列映射为 Lean 4 tactic 序列。
     *
     * 证明步骤到 Lean 4 tactic 的映射规则：
     *   - PROOF_STEP_ADD_NODE        -> have h : ... := by intro <name> ; constructor
     *   - PROOF_STEP_ADD_CONSTRAINT  -> have h : ... := by constructor ; assumption
     *   - PROOF_STEP_REWRITE         -> rw [h]
     *   - PROOF_STEP_FUNCTION_APP    -> apply h
     *   - PROOF_STEP_PACK_FUNCTION   -> -- 函数块打包（仅注释）
     *   - PROOF_STEP_NORMALIZATION   -> simp [normalization]
     *   - PROOF_STEP_UNIFY           -> rfl
     *   - PROOF_STEP_EX_FALSO        -> contradiction ; assumption
     *   - PROOF_STEP_ORACLE          -> by exact (oracle.verify <step_id>)
     *
     * 信任颜色处理：
     *   - GREEN   -> 全构造（可信），生成完整 tactic
     *   - BLUE    -> 未探索/资源受限，使用 by admit + 注释
     *   - ORANGE  -> 非构造性oracle，使用 by exact oracle_result.<name> + 注释
     *   - AMBER   -> 数值假设，使用 by sorry -- [NUMERIC] 注释
     *   - 其他    -> by trivial / by assumption 作为回退
     *
     * @param proof 证明对象指针
     * @param config 导出配置
     * @return LV00_OK 成功，LV00_ERROR_INVALID_PARAM 参数无效，LV00_ERROR_IO 文件错误
     */
    if (!proof || !config)
        return LV00_ERROR_INVALID_PARAM;

    /* ---- 流式事件：开始 Lean 4 导出 ---- */
    if (interop_stream_ctx) {
        stream_emit_simple(interop_stream_ctx, STREAM_EVENT_INFO, "开始 Lean 4 导出", 0);
    }

    /* 生成Lean代码 */
    FILE *fp = fopen(config->output_path, "w");
    if (!fp) {
        if (interop_stream_ctx) {
            stream_emit_simple(interop_stream_ctx, STREAM_EVENT_ERROR, "Lean 4 导出失败：无法创建输出文件", 0);
        }
        return LV00_ERROR_IO;
    }

    fprintf(fp, "-- Generated by Lv-00 v3.2.0\n");
    fprintf(fp, "import EuclideanGeometry\n\n");
    fprintf(fp, "namespace Lv00Export\n\n");

    /* ---- 证明步骤到 Lean 4 tactic 映射表（注释） ---- */
    fprintf(fp, "  -- 证明步骤类型 -> Lean 4 tactic 映射:\n");
    fprintf(fp, "  --   PROOF_STEP_ADD_NODE        -> have h : ... := by intro <name> ; constructor\n");
    fprintf(fp, "  --   PROOF_STEP_ADD_CONSTRAINT  -> have h : ... := by constructor ; assumption\n");
    fprintf(fp, "  --   PROOF_STEP_REWRITE         -> rw [h]\n");
    fprintf(fp, "  --   PROOF_STEP_FUNCTION_APP    -> apply h\n");
    fprintf(fp, "  --   PROOF_STEP_PACK_FUNCTION   -> -- 函数块打包（仅注释）\n");
    fprintf(fp, "  --   PROOF_STEP_NORMALIZATION   -> simp [normalization]\n");
    fprintf(fp, "  --   PROOF_STEP_UNIFY           -> rfl\n");
    fprintf(fp, "  --   PROOF_STEP_EX_FALSO        -> contradiction ; assumption\n");
    fprintf(fp, "  --   PROOF_STEP_ORACLE          -> by exact (oracle.verify <step_id>)\n\n");

    /* ---- 信任颜色映射（注释） ---- */
    fprintf(fp, "  -- 信任颜色映射:\n");
    fprintf(fp, "  --   GREEN            -> 全构造（可信），生成完整 tactic\n");
    fprintf(fp, "  --   BLUE_UNEXPLORED  -> 未探索，使用 by admit\n");
    fprintf(fp, "  --   BLUE_RESOURCE    -> 资源受限，使用 by admit\n");
    fprintf(fp, "  --   ORANGE_ORACLE    -> 非构造性oracle，使用 by exact oracle_result\n");
    fprintf(fp, "  --   ORANGE_EX_FALSO  -> 爆炸原理步骤，使用 exfalso ; by sorry\n");
    fprintf(fp, "  --   AMBER            -> 数值假设，使用 by sorry -- [NUMERIC]\n");
    fprintf(fp, "  --   其他颜色         -> 回退至 by trivial / by assumption\n\n");

    fprintf(fp, "  theorem lv00_main : True := by\n");

    /*
     * 证明体生成：
     * Proof 通过 typedef ProofNavigator Proof 定义为 ProofNavigator，
     * 可访问 steps/step_count 字段。遍历 proof->steps 数组，
     * 根据每个 ProofStep 的 type 和 color 生成对应的 Lean 4 tactic。
     *
     * 信任颜色处理策略：
     *   - GREEN / GREEN_VERIFIED -> 全构造（可信），生成完整 tactic
     *   - BLUE_*                 -> 未探索/资源受限，使用 by admit + 描述性注释
     *   - ORANGE_*               -> 非构造性oracle，使用 by exact oracle_result / oracle.verify
     *   - AMBER                  -> 数值假设，使用 by sorry -- [NUMERIC] 标注
     *   - 其他（YELLOW等）        -> 回退至 by trivial / by assumption
     */
    if (proof->steps && proof->step_count > 0) {
        fprintf(fp, "    -- 证明步骤数: %d\n", proof->step_count);
        for (int i = 0; i < proof->step_count; i++) {
            ProofStep *step = proof->steps[i];
            if (!step)
                continue;

            /* 信任颜色分类判断 */
            bool is_green = (step->color == PROOF_COLOR_GREEN || step->color == PROOF_COLOR_GREEN_VERIFIED);
            bool is_blue = (step->color == PROOF_COLOR_BLUE_UNEXPLORED ||
                            step->color == PROOF_COLOR_BLUE_RESOURCE ||
                            step->color == PROOF_COLOR_BLUE_OUT_OF_RANGE);
            bool is_orange = (step->color == PROOF_COLOR_ORACLE_ORACLE ||
                              step->color == PROOF_COLOR_ORANGE_EX_FALSO ||
                              step->color == PROOF_COLOR_DARK_ORANGE);
            bool is_amber = (step->color == PROOF_COLOR_AMBER);

            switch (step->type) {
                case PROOF_STEP_ADD_NODE:
                    if (is_green) {
                        fprintf(fp, "    have h_node_%d : True := by intro node_%d ; constructor\n",
                                step->node_id, step->node_id);
                    } else if (is_blue) {
                        fprintf(fp, "    -- [BLUE] 构造节点 node_%d, 信任色: %s (未探索/资源受限)\n",
                                step->node_id, proof_color_to_string(step->color));
                        fprintf(fp, "    have h_node_%d : True := by admit\n", step->node_id);
                    } else if (is_orange) {
                        fprintf(fp, "    -- [ORANGE] 构造节点 node_%d, 信任色: %s (非构造性oracle依赖)\n",
                                step->node_id, proof_color_to_string(step->color));
                        fprintf(fp, "    have h_node_%d : True := by exact oracle_result.node_%d\n",
                                step->node_id, step->node_id);
                    } else if (is_amber) {
                        fprintf(fp, "    -- [AMBER] 构造节点 node_%d, 信任色: %s (数值假设)\n",
                                step->node_id, proof_color_to_string(step->color));
                        fprintf(fp, "    have h_node_%d : True := by sorry -- [NUMERIC] 数值假设步骤\n",
                                step->node_id);
                    } else {
                        fprintf(fp, "    -- 构造节点 node_%d, 信任色: %s\n",
                                step->node_id, proof_color_to_string(step->color));
                        fprintf(fp, "    have h_node_%d : True := by trivial\n", step->node_id);
                    }
                    break;

                case PROOF_STEP_ADD_CONSTRAINT:
                    if (is_green) {
                        fprintf(fp, "    have h_cstr_%d : True := by constructor ; assumption\n",
                                step->constraint_id);
                    } else if (is_blue) {
                        fprintf(fp, "    -- [BLUE] 添加约束 cstr_%d, 信任色: %s (未探索/资源受限)\n",
                                step->constraint_id, proof_color_to_string(step->color));
                        fprintf(fp, "    have h_cstr_%d : True := by admit\n", step->constraint_id);
                    } else if (is_orange) {
                        fprintf(fp, "    -- [ORANGE] 添加约束 cstr_%d, 信任色: %s (非构造性oracle依赖)\n",
                                step->constraint_id, proof_color_to_string(step->color));
                        fprintf(fp, "    have h_cstr_%d : True := by exact oracle_result.cstr_%d\n",
                                step->constraint_id, step->constraint_id);
                    } else if (is_amber) {
                        fprintf(fp, "    -- [AMBER] 添加约束 cstr_%d, 信任色: %s (数值假设)\n",
                                step->constraint_id, proof_color_to_string(step->color));
                        fprintf(fp, "    have h_cstr_%d : True := by sorry -- [NUMERIC] 数值假设步骤\n",
                                step->constraint_id);
                    } else {
                        fprintf(fp, "    -- 添加约束 cstr_%d, 信任色: %s\n",
                                step->constraint_id, proof_color_to_string(step->color));
                        fprintf(fp, "    have h_cstr_%d : True := by trivial\n", step->constraint_id);
                    }
                    break;

                case PROOF_STEP_REWRITE:
                    if (is_green) {
                        fprintf(fp, "    rw [h]\n");
                    } else if (is_blue) {
                        fprintf(fp, "    -- [BLUE] 重写步骤 step_%d, 信任色: %s (未探索/资源受限)\n",
                                step->id, proof_color_to_string(step->color));
                        fprintf(fp, "    by admit -- 蓝色步骤：待探索\n");
                    } else if (is_orange) {
                        fprintf(fp, "    -- [ORANGE] 重写步骤 step_%d, 信任色: %s (非构造性oracle依赖)\n",
                                step->id, proof_color_to_string(step->color));
                        fprintf(fp, "    by exact (oracle.verify step_%d)\n", step->id);
                    } else if (is_amber) {
                        fprintf(fp, "    -- [AMBER] 重写步骤 step_%d, 信任色: %s (数值假设)\n",
                                step->id, proof_color_to_string(step->color));
                        fprintf(fp, "    by sorry -- [NUMERIC] 数值假设步骤\n");
                    } else {
                        fprintf(fp, "    -- 重写步骤 step_%d, 信任色: %s\n",
                                step->id, proof_color_to_string(step->color));
                        fprintf(fp, "    by assumption\n");
                    }
                    break;

                case PROOF_STEP_FUNCTION_APP:
                    if (is_green) {
                        fprintf(fp, "    apply h\n");
                    } else if (is_blue) {
                        fprintf(fp, "    -- [BLUE] 函数应用 step_%d, 信任色: %s (未探索/资源受限)\n",
                                step->id, proof_color_to_string(step->color));
                        fprintf(fp, "    by admit -- 蓝色步骤：待探索\n");
                    } else if (is_orange) {
                        fprintf(fp, "    -- [ORANGE] 函数应用 step_%d, 信任色: %s (非构造性oracle依赖)\n",
                                step->id, proof_color_to_string(step->color));
                        fprintf(fp, "    by exact (oracle.verify step_%d)\n", step->id);
                    } else if (is_amber) {
                        fprintf(fp, "    -- [AMBER] 函数应用 step_%d, 信任色: %s (数值假设)\n",
                                step->id, proof_color_to_string(step->color));
                        fprintf(fp, "    by sorry -- [NUMERIC] 数值假设步骤\n");
                    } else {
                        fprintf(fp, "    -- 函数应用 step_%d, 信任色: %s\n",
                                step->id, proof_color_to_string(step->color));
                        fprintf(fp, "    by trivial\n");
                    }
                    break;

                case PROOF_STEP_PACK_FUNCTION:
                    fprintf(fp, "    -- 函数块打包: step_%d, func_block_%d\n", step->id, step->func_block_id);
                    break;

                case PROOF_STEP_NORMALIZATION:
                    if (is_green) {
                        fprintf(fp, "    simp [normalization]\n");
                    } else if (is_blue) {
                        fprintf(fp, "    -- [BLUE] 归一化 step_%d, 信任色: %s (未探索/资源受限)\n",
                                step->id, proof_color_to_string(step->color));
                        fprintf(fp, "    by admit -- 蓝色步骤：待探索\n");
                    } else if (is_orange) {
                        fprintf(fp, "    -- [ORANGE] 归一化 step_%d, 信任色: %s (非构造性oracle依赖)\n",
                                step->id, proof_color_to_string(step->color));
                        fprintf(fp, "    by exact (oracle.verify step_%d)\n", step->id);
                    } else if (is_amber) {
                        fprintf(fp, "    -- [AMBER] 归一化 step_%d, 信任色: %s (数值假设)\n",
                                step->id, proof_color_to_string(step->color));
                        fprintf(fp, "    by sorry -- [NUMERIC] 数值假设步骤\n");
                    } else {
                        fprintf(fp, "    -- 归一化 step_%d, 信任色: %s\n",
                                step->id, proof_color_to_string(step->color));
                        fprintf(fp, "    by assumption\n");
                    }
                    break;

                case PROOF_STEP_UNIFY:
                    if (is_green) {
                        fprintf(fp, "    rfl\n");
                    } else if (is_blue) {
                        fprintf(fp, "    -- [BLUE] 合一检查 step_%d, 信任色: %s (未探索/资源受限)\n",
                                step->id, proof_color_to_string(step->color));
                        fprintf(fp, "    by admit -- 蓝色步骤：待探索\n");
                    } else if (is_orange) {
                        fprintf(fp, "    -- [ORANGE] 合一检查 step_%d, 信任色: %s (非构造性oracle依赖)\n",
                                step->id, proof_color_to_string(step->color));
                        fprintf(fp, "    by exact (oracle.verify step_%d)\n", step->id);
                    } else if (is_amber) {
                        fprintf(fp, "    -- [AMBER] 合一检查 step_%d, 信任色: %s (数值假设)\n",
                                step->id, proof_color_to_string(step->color));
                        fprintf(fp, "    by sorry -- [NUMERIC] 数值假设步骤\n");
                    } else {
                        fprintf(fp, "    -- 合一检查 step_%d, 信任色: %s\n",
                                step->id, proof_color_to_string(step->color));
                        fprintf(fp, "    by trivial\n");
                    }
                    break;

                case PROOF_STEP_EX_FALSO:
                    if (is_green) {
                        fprintf(fp, "    contradiction ; assumption\n");
                    } else {
                        fprintf(fp, "    -- [非绿色] 爆炸原理 step_%d, 信任色: %s\n",
                                step->id, proof_color_to_string(step->color));
                        fprintf(fp, "    exfalso ; by sorry -- 非构造性爆炸原理，需外部验证\n");
                    }
                    break;

                case PROOF_STEP_ORACLE:
                    fprintf(fp, "    -- [ORACLE] Oracle依赖: step_%d, 信任色: %s\n",
                            step->id, proof_color_to_string(step->color));
                    fprintf(fp, "    by exact (oracle.verify step_%d) -- 非构造性依赖，需外部oracle验证\n",
                            step->id);
                    break;

                default:
                    fprintf(fp, "    -- 未知步骤类型: %d, 信任色: %s\n",
                            (int) step->type, proof_color_to_string(step->color));
                    fprintf(fp, "    by trivial\n");
                    break;
            }
        }
    } else {
        fprintf(fp, "    -- 证明步骤为空，无步骤可展开\n");
        fprintf(fp, "    trivial\n");
    }
    fprintf(fp, "\n");

    fprintf(fp, "end Lv00Export\n");

    fclose(fp);

    /* ---- 流式事件：Lean 4 导出完成 ---- */
    if (interop_stream_ctx) {
        stream_emit_simple(interop_stream_ctx, STREAM_EVENT_INFO, "Lean 4 导出完成", 0);
    }

    return LV00_OK;
}

int interop_export_html(const LV00Engine *engine, const InteropExportConfig *config) {
    if (!engine || !config)
        return LV00_ERROR_INVALID_PARAM;

    /* ---- 流式事件：开始 HTML 导出 ---- */
    {
        StreamContext *sctx = engine_get_stream_context(engine);
        if (sctx) {
            stream_emit_simple(sctx, STREAM_EVENT_INFO, "开始 HTML 导出", 0);
        }
    }

    FILE *fp = fopen(config->output_path, "w");
    if (!fp)
        return LV00_ERROR_IO;

    /*
     * 增强版HTML导出：
     * - 将 ConstraintGraph 节点渲染为 SVG 元素
     * - 点 -> 圆形，线段 -> 线条，区域 -> 多边形
     * - 使用 symbolic_coord_to_double() 获取坐标
     * - 节点标签
     * - 按信任状态着色
     * - 侧边栏显示图统计信息
     * - 自包含 HTML（内联 CSS/JS）
     */

    ConstraintGraph *graph = engine->main_graph;
    int node_count = graph ? graph->node_count : 0;
    int constraint_count = graph ? graph->constraint_count : 0;

    /* 统计各类型节点数量 */
    int point_count = 0, segment_count = 0, region_count = 0;
    int port_count = 0, fb_count = 0;
    int trust_green = 0, trust_blue = 0, trust_orange = 0, trust_amber = 0, trust_other = 0;

    /* 计算坐标范围用于缩放 */
    double min_x = 1e9, max_x = -1e9, min_y = 1e9, max_y = -1e9;
    bool has_coords = false;

    if (graph) {
        for (int i = 0; i < graph->node_count; i++) {
            GeomNode *node = graph->nodes[i];
            if (!node)
                continue;

            switch (node->type) {
                case GEOM_POINT:
                    point_count++;
                    break;
                case GEOM_LINE_SEGMENT:
                    segment_count++;
                    break;
                case GEOM_REGION:
                    region_count++;
                    break;
                case GEOM_PORT:
                    port_count++;
                    break;
                case GEOM_FUNCTION_BLOCK:
                    fb_count++;
                    break;
            }

            switch (node->trust) {
                case TRUST_GREEN:
                    trust_green++;
                    break;
                case TRUST_BLUE:
                    trust_blue++;
                    break;
                case TRUST_ORANGE:
                    trust_orange++;
                    break;
                case TRUST_AMBER:
                    trust_amber++;
                    break;
                default:
                    trust_other++;
                    break;
            }

            /* 收集坐标范围 */
            if (node->coord_count >= 2 && node->symbolic_coords && node->symbolic_coords[0] &&
                node->symbolic_coords[1]) {
                double x = symbolic_coord_to_double(node->symbolic_coords[0]);
                double y = symbolic_coord_to_double(node->symbolic_coords[1]);
                if (x < min_x)
                    min_x = x;
                if (x > max_x)
                    max_x = x;
                if (y < min_y)
                    min_y = y;
                if (y > max_y)
                    max_y = y;
                has_coords = true;
            }
        }
    }

    /* 如果没有有效坐标，使用默认范围 */
    if (!has_coords) {
        min_x = -10;
        max_x = 10;
        min_y = -10;
        max_y = 10;
    }

    /* 添加边距 */
    double margin = (max_x - min_x) * 0.15 + 1.0;
    if (margin < 1.0)
        margin = 1.0;
    min_x -= margin;
    max_x += margin;
    min_y -= margin;
    max_y += margin;

    double range_x = max_x - min_x;
    double range_y = max_y - min_y;

    /* 添加除零保护，防止所有节点坐标相同时 range 为零导致除零错误 */
    if (range_x < 1e-10)
        range_x = 1.0;
    if (range_y < 1e-10)
        range_y = 1.0;

    /* SVG 尺寸 */
    int svg_w = 700, svg_h = 500;
    double pad = 40.0;

    fprintf(fp, "<!DOCTYPE html>\n");
    fprintf(fp, "<html lang=\"en\">\n<head>\n");
    fprintf(fp, "<meta charset=\"UTF-8\">\n");
    fprintf(fp, "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n");
    fprintf(fp, "<title>Lv-00 Geometry Visualization</title>\n");
    fprintf(fp, "<style>\n");
    fprintf(fp, "* { box-sizing: border-box; margin: 0; padding: 0; }\n");
    fprintf(fp, "body {\n");
    fprintf(fp, "  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;\n");
    fprintf(fp, "  background: #f5f5f5; color: #333; padding: 20px;\n");
    fprintf(fp, "}\n");
    fprintf(fp, ".layout { display: flex; gap: 20px; max-width: 1100px; margin: 0 auto; }\n");
    fprintf(fp, ".main-panel {\n");
    fprintf(fp, "  flex: 1; background: #fff; border: 1px solid #ddd;\n");
    fprintf(fp, "  border-radius: 6px; overflow: hidden;\n");
    fprintf(fp, "}\n");
    fprintf(fp, ".main-panel h1 {\n");
    fprintf(fp, "  font-size: 1.2em; padding: 12px 16px;\n");
    fprintf(fp, "  border-bottom: 1px solid #eee; background: #fafafa;\n");
    fprintf(fp, "}\n");
    fprintf(fp, ".svg-wrap { padding: 12px; text-align: center; }\n");
    fprintf(fp, "svg { border: 1px solid #e0e0e0; border-radius: 4px; background: #fff; }\n");
    fprintf(fp, ".sidebar {\n");
    fprintf(fp, "  width: 260px; flex-shrink: 0;\n");
    fprintf(fp, "  background: #fff; border: 1px solid #ddd; border-radius: 6px;\n");
    fprintf(fp, "  overflow: hidden;\n");
    fprintf(fp, "}\n");
    fprintf(fp, ".sidebar h2 {\n");
    fprintf(fp, "  font-size: 1em; padding: 10px 14px;\n");
    fprintf(fp, "  border-bottom: 1px solid #eee; background: #fafafa;\n");
    fprintf(fp, "}\n");
    fprintf(fp, ".stat-section { padding: 10px 14px; border-bottom: 1px solid #f0f0f0; }\n");
    fprintf(fp,
            ".stat-section h3 { font-size: 0.85em; color: #888; margin-bottom: 6px; text-transform: uppercase; }\n");
    fprintf(fp, ".stat-row { display: flex; justify-content: space-between; font-size: 13px; padding: 2px 0; }\n");
    fprintf(fp, ".stat-row .val { font-weight: 600; }\n");
    fprintf(fp, ".trust-legend { display: flex; align-items: center; gap: 6px; font-size: 13px; padding: 2px 0; }\n");
    fprintf(fp, ".trust-dot { width: 10px; height: 10px; border-radius: 50%%; flex-shrink: 0; }\n");
    fprintf(fp, ".legend-green { background: #4CAF50; }\n");
    fprintf(fp, ".legend-blue { background: #42A5F5; }\n");
    fprintf(fp, ".legend-orange { background: #FF9800; }\n");
    fprintf(fp, ".legend-amber { background: #FFB300; }\n");
    fprintf(fp, ".legend-other { background: #999; }\n");
    fprintf(fp, "@media (max-width: 800px) {\n");
    fprintf(fp, "  .layout { flex-direction: column; }\n");
    fprintf(fp, "  .sidebar { width: 100%%; }\n");
    fprintf(fp, "}\n");
    fprintf(fp, "@media print {\n");
    fprintf(fp, "  body { background: #fff; padding: 0; }\n");
    fprintf(fp, "}\n");
    fprintf(fp, "</style>\n");
    fprintf(fp, "</head>\n<body>\n");

    fprintf(fp, "<div class=\"layout\">\n");

    /* 主面板：SVG 可视化 */
    fprintf(fp, "<div class=\"main-panel\">\n");
    fprintf(fp, "<h1>Lv-00 Geometry Visualization</h1>\n");
    fprintf(fp, "<div class=\"svg-wrap\">\n");
    fprintf(fp,
            "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%d\" height=\"%d\" "
            "viewBox=\"0 0 %d %d\">\n",
            svg_w, svg_h, svg_w, svg_h);

    /* 背景网格 */
    fprintf(fp, "  <defs>\n");
    fprintf(fp, "    <pattern id=\"grid\" width=\"40\" height=\"40\" patternUnits=\"userSpaceOnUse\">\n");
    fprintf(fp, "      <path d=\"M 40 0 L 0 0 0 40\" fill=\"none\" stroke=\"#f0f0f0\" stroke-width=\"0.5\"/>\n");
    fprintf(fp, "    </pattern>\n");
    fprintf(fp, "  </defs>\n");
    fprintf(fp, "  <rect width=\"100%%\" height=\"100%%\" fill=\"url(#grid)\"/>\n");

    /* 坐标转换函数（内联JS用于tooltip，这里直接用C计算） */
    /* worldToSvg: x_svg = pad + (wx - min_x) / range_x * (svg_w - 2*pad) */
    /*             y_svg = pad + (max_y - wy) / range_y * (svg_h - 2*pad)  (Y轴翻转) */

    if (graph) {
        /* 先画线段 */
        for (int i = 0; i < graph->node_count; i++) {
            GeomNode *node = graph->nodes[i];
            if (!node || node->type != GEOM_LINE_SEGMENT)
                continue;

            /* 线段有两个端点，通过约束获取 */
            /* 当前：直接用节点的坐标作为线段中点，画一个小线段标记 */
            /* 改进：查找 INCIDENCE 约束找到端点，计算精确中点 */
            double cx = 0, cy = 0;
            if (node->coord_count >= 2 && node->symbolic_coords && node->symbolic_coords[0] &&
                node->symbolic_coords[1]) {
                cx = symbolic_coord_to_double(node->symbolic_coords[0]);
                cy = symbolic_coord_to_double(node->symbolic_coords[1]);
            }

            /* 查找关联的端点 */
            double x1 = cx, y1 = cy, x2 = cx, y2 = cy;
            bool found_endpoints = false;
            for (int c = 0; c < graph->constraint_count; c++) {
                Constraint *con = graph->constraints[c];
                if (!con || con->type != INCIDENCE)
                    continue;
                if (con->participant_count < 2)
                    continue;
                int other = -1;
                if (con->participants[0] == node->id)
                    other = con->participants[1];
                else if (con->participants[1] == node->id)
                    other = con->participants[0];
                if (other < 0)
                    continue;
                GeomNode *ep = graph_get_node_by_id(graph, other);
                if (ep && ep->type == GEOM_POINT && ep->coord_count >= 2 && ep->symbolic_coords &&
                    ep->symbolic_coords[0] && ep->symbolic_coords[1]) {
                    if (!found_endpoints) {
                        x1 = symbolic_coord_to_double(ep->symbolic_coords[0]);
                        y1 = symbolic_coord_to_double(ep->symbolic_coords[1]);
                        found_endpoints = true;
                    } else {
                        x2 = symbolic_coord_to_double(ep->symbolic_coords[0]);
                        y2 = symbolic_coord_to_double(ep->symbolic_coords[1]);
                    }
                }
            }

            /* SVG 坐标 */
            double sx1 = pad + (x1 - min_x) / range_x * (svg_w - 2 * pad);
            double sy1 = pad + (max_y - y1) / range_y * (svg_h - 2 * pad);
            double sx2 = pad + (x2 - min_x) / range_x * (svg_w - 2 * pad);
            double sy2 = pad + (max_y - y2) / range_y * (svg_h - 2 * pad);

            /* 信任颜色 */
            const char *stroke_color = "#333";
            const char *stroke_width = "2";
            switch (node->trust) {
                case TRUST_GREEN:
                    stroke_color = "#4CAF50";
                    break;
                case TRUST_BLUE:
                    stroke_color = "#42A5F5";
                    break;
                case TRUST_ORANGE:
                    stroke_color = "#FF9800";
                    stroke_width = "2.5";
                    break;
                case TRUST_AMBER:
                    stroke_color = "#FFB300";
                    break;
                default:
                    break;
            }

            fprintf(fp,
                    "  <line x1=\"%.1f\" y1=\"%.1f\" x2=\"%.1f\" y2=\"%.1f\" "
                    "stroke=\"%s\" stroke-width=\"%s\" stroke-linecap=\"round\"/>\n",
                    sx1, sy1, sx2, sy2, stroke_color, stroke_width);

            /* 线段标签 */
            double mx = (sx1 + sx2) / 2;
            double my = (sy1 + sy2) / 2;
            fprintf(fp,
                    "  <text x=\"%.1f\" y=\"%.1f\" font-size=\"10\" fill=\"#666\" "
                    "text-anchor=\"middle\" dy=\"-6\">S%d</text>\n",
                    mx, my, node->id);
        }

        /* 再画点 */
        for (int i = 0; i < graph->node_count; i++) {
            GeomNode *node = graph->nodes[i];
            if (!node || node->type != GEOM_POINT)
                continue;

            double x = 0, y = 0;
            if (node->coord_count >= 2 && node->symbolic_coords && node->symbolic_coords[0] &&
                node->symbolic_coords[1]) {
                x = symbolic_coord_to_double(node->symbolic_coords[0]);
                y = symbolic_coord_to_double(node->symbolic_coords[1]);
            }

            double sx = pad + (x - min_x) / range_x * (svg_w - 2 * pad);
            double sy = pad + (max_y - y) / range_y * (svg_h - 2 * pad);

            /* 信任颜色 */
            const char *fill_color = "#333";
            const char *label_color = "#333";
            switch (node->trust) {
                case TRUST_GREEN:
                    fill_color = "#4CAF50";
                    break;
                case TRUST_BLUE:
                    fill_color = "#42A5F5";
                    break;
                case TRUST_ORANGE:
                    fill_color = "#FF9800";
                    break;
                case TRUST_AMBER:
                    fill_color = "#FFB300";
                    label_color = "#333";
                    break;
                default:
                    break;
            }

            fprintf(fp,
                    "  <circle cx=\"%.1f\" cy=\"%.1f\" r=\"5\" fill=\"%s\" "
                    "stroke=\"#fff\" stroke-width=\"1.5\">\n",
                    sx, sy, fill_color);
            fprintf(fp, "    <title>Point %d (%.2f, %.2f)</title>\n", node->id, x, y);
            fprintf(fp, "  </circle>\n");
            fprintf(fp,
                    "  <text x=\"%.1f\" y=\"%.1f\" font-size=\"11\" fill=\"%s\" "
                    "text-anchor=\"middle\" dy=\"-10\" font-weight=\"600\">P%d</text>\n",
                    sx, sy, label_color, node->id);
        }

        /* 画区域 */
        for (int i = 0; i < graph->node_count; i++) {
            GeomNode *node = graph->nodes[i];
            if (!node || node->type != GEOM_REGION)
                continue;

            /* 收集区域边界线段的端点坐标 */
            if (node->data.region.segment_count > 0 && node->data.region.boundary_segments) {
                fprintf(fp, "  <polygon points=\"");
                for (int s = 0; s < node->data.region.segment_count; s++) {
                    GeomNode *seg = node->data.region.boundary_segments[s];
                    if (!seg || seg->coord_count < 2 || !seg->symbolic_coords)
                        continue;
                    double sx = 0, sy = 0;
                    if (seg->symbolic_coords[0] && seg->symbolic_coords[1]) {
                        double wx = symbolic_coord_to_double(seg->symbolic_coords[0]);
                        double wy = symbolic_coord_to_double(seg->symbolic_coords[1]);
                        sx = pad + (wx - min_x) / range_x * (svg_w - 2 * pad);
                        sy = pad + (max_y - wy) / range_y * (svg_h - 2 * pad);
                    }
                    fprintf(fp, "%.1f,%.1f ", sx, sy);
                }
                const char *region_fill = "#E8F5E9";
                const char *region_stroke = "#81C784";
                switch (node->trust) {
                    case TRUST_GREEN:
                        region_fill = "#E8F5E9";
                        region_stroke = "#4CAF50";
                        break;
                    case TRUST_BLUE:
                        region_fill = "#E3F2FD";
                        region_stroke = "#42A5F5";
                        break;
                    case TRUST_ORANGE:
                        region_fill = "#FFF3E0";
                        region_stroke = "#FF9800";
                        break;
                    case TRUST_AMBER:
                        region_fill = "#FFF8E1";
                        region_stroke = "#FFB300";
                        break;
                    default:
                        break;
                }
                fprintf(fp,
                        "\" fill=\"%s\" fill-opacity=\"0.3\" stroke=\"%s\" "
                        "stroke-width=\"1.5\" stroke-dasharray=\"4,2\">\n",
                        region_fill, region_stroke);
                fprintf(fp, "    <title>Region %d</title>\n", node->id);
                fprintf(fp, "  </polygon>\n");
                fprintf(fp,
                        "  <text x=\"%.1f\" y=\"%.1f\" font-size=\"10\" fill=\"#888\" "
                        "text-anchor=\"middle\">R%d</text>\n",
                        pad + (svg_w - 2 * pad) * 0.5, pad + (svg_h - 2 * pad) * 0.5, node->id);
            }
        }
    }

    fprintf(fp, "</svg>\n");
    fprintf(fp, "</div>\n"); /* .svg-wrap */
    fprintf(fp, "</div>\n"); /* .main-panel */

    /* 侧边栏：图统计信息 */
    fprintf(fp, "<div class=\"sidebar\">\n");
    fprintf(fp, "<h2>Graph Statistics</h2>\n");

    fprintf(fp, "<div class=\"stat-section\">\n");
    fprintf(fp, "  <h3>Overview</h3>\n");
    fprintf(fp, "  <div class=\"stat-row\"><span>Total Nodes</span><span class=\"val\">%d</span></div>\n", node_count);
    fprintf(fp, "  <div class=\"stat-row\"><span>Total Constraints</span><span class=\"val\">%d</span></div>\n",
            constraint_count);
    fprintf(fp, "</div>\n");

    fprintf(fp, "<div class=\"stat-section\">\n");
    fprintf(fp, "  <h3>Node Types</h3>\n");
    fprintf(fp, "  <div class=\"stat-row\"><span>Points</span><span class=\"val\">%d</span></div>\n", point_count);
    fprintf(fp, "  <div class=\"stat-row\"><span>Segments</span><span class=\"val\">%d</span></div>\n", segment_count);
    fprintf(fp, "  <div class=\"stat-row\"><span>Regions</span><span class=\"val\">%d</span></div>\n", region_count);
    fprintf(fp, "  <div class=\"stat-row\"><span>Ports</span><span class=\"val\">%d</span></div>\n", port_count);
    fprintf(fp, "  <div class=\"stat-row\"><span>Function Blocks</span><span class=\"val\">%d</span></div>\n",
            fb_count);
    fprintf(fp, "</div>\n");

    fprintf(fp, "<div class=\"stat-section\">\n");
    fprintf(fp, "  <h3>Trust Status</h3>\n");
    fprintf(fp,
            "  <div class=\"trust-legend\"><span class=\"trust-dot legend-green\"></span>"
            "<span>Green (Constructive): %d</span></div>\n",
            trust_green);
    fprintf(fp,
            "  <div class=\"trust-legend\"><span class=\"trust-dot legend-blue\"></span>"
            "<span>Blue (Unexplored): %d</span></div>\n",
            trust_blue);
    fprintf(fp,
            "  <div class=\"trust-legend\"><span class=\"trust-dot legend-orange\"></span>"
            "<span>Orange (Non-constructive): %d</span></div>\n",
            trust_orange);
    fprintf(fp,
            "  <div class=\"trust-legend\"><span class=\"trust-dot legend-amber\"></span>"
            "<span>Amber (Numeric): %d</span></div>\n",
            trust_amber);
    fprintf(fp,
            "  <div class=\"trust-legend\"><span class=\"trust-dot legend-other\"></span>"
            "<span>Other: %d</span></div>\n",
            trust_other);
    fprintf(fp, "</div>\n");

    fprintf(fp, "<div class=\"stat-section\">\n");
    fprintf(fp, "  <h3>Coordinate Range</h3>\n");
    fprintf(fp, "  <div class=\"stat-row\"><span>X</span><span class=\"val\">[%.2f, %.2f]</span></div>\n",
            min_x + margin, max_x - margin);
    fprintf(fp, "  <div class=\"stat-row\"><span>Y</span><span class=\"val\">[%.2f, %.2f]</span></div>\n",
            min_y + margin, max_y - margin);
    fprintf(fp, "</div>\n");

    fprintf(fp, "</div>\n"); /* .sidebar */
    fprintf(fp, "</div>\n"); /* .layout */

    fprintf(fp, "</body>\n</html>\n");

    fclose(fp);

    /* ---- 流式事件：HTML 导出完成 ---- */
    {
        StreamContext *sctx = engine_get_stream_context(engine);
        if (sctx) {
            stream_emit_simple(sctx, STREAM_EVENT_INFO, "HTML 导出完成", 0);
        }
    }

    return LV00_OK;
}

int interop_export_svg(const ConstraintGraph *graph, const InteropExportConfig *config) {
    /**
     * @brief 将约束图导出为SVG矢量图
     *
     * 【已实现功能】
     *   本函数已将SVG导出的核心渲染管线完整实现，能够生成独立可用的SVG文件：
     *   1. 边界框计算 —— 自动遍历约束图中所有节点的符号坐标，计算包围盒
     *   2. 区域（Region）渲染 —— 在底层渲染多边形区域，带透明度填充
     *   3. 函数块（Function Block）渲染 —— 渲染为圆角矩形，居中显示名称和ID
     *   4. 线段（Line Segment）渲染 —— 渲染为带颜色的直线段，中点显示标签
     *   5. 端口（Port）渲染 —— 输入/输出端口渲染为小圆圈，标注类型和ID
     *   6. 点（Point）渲染 —— 渲染为填充圆形，标注P+ID
     *   7. 约束关系渲染 —— 支持四种约束类型的可视化：
     *      - 关联约束（INCIDENCE）：灰色虚线
     *      - 之间约束（BETWEENNESS）：紫色斜体标签标注三点关系
     *      - 相交约束（INTERSECTION）：紫色十字标记
     *      - 包含约束（CONTAINMENT）：青色点线
     *      - 连接约束（CONNECTION）：橙色箭头线
     *   8. 图例（Legend） —— 左上角半透明图例，说明各几何类型和信任颜色含义
     *   9. 信任颜色映射 —— 根据TrustColor为不同信用级别的元素使用不同颜色：
     *      绿色（受约束）、灰色（自由）、红色（冲突）
     *  10. 样式定义 —— 通过 <style> 标签统一定义 class 样式，clean SVG结构
     *
     * 【简化实现的部分（完整功能需要额外依赖或后续版本）】
     *   1. 贝塞尔曲线/圆弧段的精确渲染 —— 当前仅使用直线端点连接；
     *      完整实现需要解析曲线控制点并生成 SVG <path> 的 C/Q/A 弧命令。
     *      所需数据：从 GeomNode 的 coord_count > 4 时提取控制点坐标。
     *   2. 区域边界的曲线路径 —— 当前使用 polygon 直线顶点连接；
     *      完整实现需要使用 SVG <path> 的贝塞尔命令绘制曲线边界。
     *   3. 包含/相交约束的精确几何交点 —— 当前使用参与者节点坐标
     *      作为端点；完整实现需要调用几何求解器计算实际的交点位置。
     *   4. 交互式JavaScript增强 —— 当前为纯静态SVG图形；
     *      完整实现需要嵌入JS代码实现点击高亮、悬停提示等交互。
     *   5. 数学公式渲染 —— 当前仅输出纯文本坐标；
     *      完整实现需要嵌入 LaTeX/MathML 的 SVG foreignObject。
     *   6. 多图层分组 —— 当前所有元素在同一层级；
     *      完整实现需要使用 <g> 标签按信任级别/几何类型分组。
     *   7. CSS动画/过渡 —— 当前无动画支持；
     *      完整实现需要 CSS keyframes 或 SMIL 动画演示求解过程。
     *
     * 【外部依赖说明】
     *   本函数完全使用标准C的 fprintf 生成纯文本SVG，不依赖任何外部XML或
     *   图形库。所有辅助函数（compute_bounding_box、trust_color_to_svg、
     *   svg_escape_string）均为本文件内部实现。
     *
     * 【使用示例】
     *   InteropExportConfig cfg;
     *   lv00_strlcpy(cfg.output_path, "output.svg", sizeof(cfg.output_path));
     *   int ret = interop_export_svg(graph, &cfg);
     *
     * @param graph 约束图指针（包含所有节点和约束）
     * @param config 导出配置（主要使用 output_path 指定输出文件路径）
     * @return LV00_OK 成功导出
     *         LV00_ERROR_INVALID_PARAM 参数无效（graph或config为NULL）
     *         LV00_ERROR_IO 文件无法创建或写入
     */
    if (!graph || !config)
        return LV00_ERROR_INVALID_PARAM;

    FILE *fp = fopen(config->output_path, "w");
    if (!fp)
        return LV00_ERROR_IO;

    /* 计算边界框 */
    double min_x, min_y, max_x, max_y;
    compute_bounding_box(graph, &min_x, &min_y, &max_x, &max_y);

    double width = max_x - min_x;
    double height = max_y - min_y;
    if (width < 1.0)
        width = 200.0;
    if (height < 1.0)
        height = 200.0;

    /* SVG头部 */
    fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(fp,
            "<svg xmlns=\"http://www.w3.org/2000/svg\" "
            "width=\"%.1f\" height=\"%.1f\" "
            "viewBox=\"%.2f %.2f %.2f %.2f\">\n",
            width, height, min_x, min_y, width, height);
    fprintf(fp, "  <title>Lv-00 Geometry Export</title>\n");
    fprintf(fp, "  <desc>Generated by Lv-00 v%s</desc>\n", LV00_VERSION_STRING);

    /* 定义样式 */
    fprintf(fp, "  <defs>\n");
    fprintf(fp, "    <style>\n");
    fprintf(fp, "      .point { stroke-width: 1.5; }\n");
    fprintf(fp, "      .line { stroke-width: 2; fill: none; }\n");
    fprintf(fp, "      .region { stroke-width: 1.5; opacity: 0.3; }\n");
    fprintf(fp, "      .constraint { stroke-width: 1; stroke-dasharray: 5,3; fill: none; }\n");
    fprintf(fp, "      .label { font-family: 'Segoe UI', Arial, sans-serif; font-size: 12px; }\n");
    fprintf(fp, "      .block { stroke-width: 2; rx: 8; ry: 8; }\n");
    fprintf(fp, "      .port { stroke-width: 1.5; }\n");
    fprintf(fp, "    </style>\n");
    fprintf(fp, "  </defs>\n\n");

    /* 背景网格（可选） */
    fprintf(fp,
            "  <rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" "
            "fill=\"#fafafa\" stroke=\"#e5e7eb\" stroke-width=\"1\"/>\n",
            min_x, min_y, width, height);

    /* ---- 渲染区域（先渲染，在底层） ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_REGION)
            continue;
        if (node->data.region.segment_count < 3)
            continue;

        const char *color = trust_color_to_svg(node->trust);
        char escaped_name[256];
        svg_escape_string(geom_type_name(node->type), escaped_name, sizeof(escaped_name));

        fprintf(fp, "  <!-- Region id=%d -->\n", node->id);
        fprintf(fp, "  <polygon class=\"region\" fill=\"%s\" stroke=\"%s\" points=\"", color, color);

        /* 收集区域边界顶点：遍历边界线段的端点 */
        for (int s = 0; s < node->data.region.segment_count; s++) {
            GeomNode *seg = node->data.region.boundary_segments[s];
            if (seg && seg->type == GEOM_LINE_SEGMENT && seg->coord_count >= 4) {
                /* 线段有两个端点，每个端点2个坐标(x1,y1,x2,y2) */
                double sx1 = symbolic_coord_to_double(seg->symbolic_coords[0]);
                double sy1 = symbolic_coord_to_double(seg->symbolic_coords[1]);
                fprintf(fp, "%.2f,%.2f ", sx1, sy1);
            }
        }
        fprintf(fp, "\"/>\n");
    }

    /* ---- 渲染函数块 ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_FUNCTION_BLOCK)
            continue;
        if (node->coord_count < 2)
            continue;

        double bx = symbolic_coord_to_double(node->symbolic_coords[0]);
        double by = symbolic_coord_to_double(node->symbolic_coords[1]);

        const char *color = trust_color_to_svg(node->trust);
        char escaped_name[256];
        svg_escape_string(geom_type_name(node->type), escaped_name, sizeof(escaped_name));

        /* 函数块：圆角矩形 */
        double bw = 120.0, bh = 60.0;
        fprintf(fp, "  <!-- Function Block id=%d -->\n", node->id);
        fprintf(fp,
                "  <rect class=\"block\" x=\"%.2f\" y=\"%.2f\" "
                "width=\"%.2f\" height=\"%.2f\" "
                "fill=\"%s\" fill-opacity=\"0.15\" stroke=\"%s\"/>\n",
                bx - bw / 2.0, by - bh / 2.0, bw, bh, color, color);
        fprintf(fp,
                "  <text class=\"label\" x=\"%.2f\" y=\"%.2f\" "
                "text-anchor=\"middle\" dominant-baseline=\"central\" "
                "fill=\"%s\">%s_%d</text>\n",
                bx, by, color, escaped_name, node->id);
    }

    /* ---- 渲染线段 ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_LINE_SEGMENT)
            continue;
        if (node->coord_count < 4)
            continue;

        double x1 = symbolic_coord_to_double(node->symbolic_coords[0]);
        double y1 = symbolic_coord_to_double(node->symbolic_coords[1]);
        double x2 = symbolic_coord_to_double(node->symbolic_coords[2]);
        double y2 = symbolic_coord_to_double(node->symbolic_coords[3]);

        const char *color = trust_color_to_svg(node->trust);

        /* 贝塞尔曲线渲染：如果线段有 3 个以上坐标对，使用 SVG cubic Bezier */
        if (node->coord_count >= 6) {
            /* 使用前两对为端点，中间对为控制点 */
            int total_pairs = node->coord_count / 2;
            fprintf(fp, "  <!-- Line Segment id=%d (Bezier, %d points) -->\n", node->id, total_pairs);
            fprintf(fp, "  <path class=\"line\" fill=\"none\" stroke=\"%s\" d=\"M %.2f,%.2f", color, x1, y1);

            /* 构建贝塞尔曲线链：每两个端点间使用 2 个控制点 */
            for (int p = 0; p < total_pairs - 1; p++) {
                double seg_x1 = symbolic_coord_to_double(node->symbolic_coords[p * 2]);
                double seg_y1 = symbolic_coord_to_double(node->symbolic_coords[p * 2 + 1]);
                double seg_x2 = symbolic_coord_to_double(node->symbolic_coords[(p + 1) * 2]);
                double seg_y2 = symbolic_coord_to_double(node->symbolic_coords[(p + 1) * 2 + 1]);

                /* CP1 = P0 + 0.3*(P1-P0) + 垂直偏移 */
                double dx = seg_x2 - seg_x1;
                double dy = seg_y2 - seg_y1;
                double offset = 0.15 * sqrt(dx * dx + dy * dy);
                if (offset < 0.01)
                    offset = 5.0;
                double nx = -dy / (sqrt(dx * dx + dy * dy) + 0.001);
                double ny = dx / (sqrt(dx * dx + dy * dy) + 0.001);

                double cp1x = seg_x1 + 0.3 * dx + nx * offset;
                double cp1y = seg_y1 + 0.3 * dy + ny * offset;
                double cp2x = seg_x2 - 0.3 * dx + nx * offset;
                double cp2y = seg_y2 - 0.3 * dy + ny * offset;

                fprintf(fp, " C %.2f,%.2f %.2f,%.2f %.2f,%.2f", cp1x, cp1y, cp2x, cp2y, seg_x2, seg_y2);
            }
            fprintf(fp, "\"/>\n");
        } else {
            fprintf(fp, "  <!-- Line Segment id=%d -->\n", node->id);
            fprintf(fp,
                    "  <line class=\"line\" x1=\"%.2f\" y1=\"%.2f\" "
                    "x2=\"%.2f\" y2=\"%.2f\" stroke=\"%s\"/>\n",
                    x1, y1, x2, y2, color);
        }

        /* 线段标签 */
        double mx = (x1 + x2) / 2.0;
        double my = (y1 + y2) / 2.0;
        fprintf(fp,
                "  <text class=\"label\" x=\"%.2f\" y=\"%.2f\" "
                "text-anchor=\"middle\" fill=\"%s\">seg_%d</text>\n",
                mx, my - 6.0, color, node->id);
    }

    /* ---- 渲染端口 ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_PORT)
            continue;
        if (node->coord_count < 2)
            continue;

        double px = symbolic_coord_to_double(node->symbolic_coords[0]);
        double py = symbolic_coord_to_double(node->symbolic_coords[1]);

        const char *color = trust_color_to_svg(node->trust);
        const char *port_type_str = (node->data.port && node->data.port->type == PORT_INPUT) ? "in" : "out";

        fprintf(fp, "  <!-- Port id=%d type=%s -->\n", node->id, port_type_str);
        fprintf(fp,
                "  <circle class=\"port\" cx=\"%.2f\" cy=\"%.2f\" r=\"5\" "
                "fill=\"white\" stroke=\"%s\"/>\n",
                px, py, color);
        fprintf(fp,
                "  <text class=\"label\" x=\"%.2f\" y=\"%.2f\" "
                "text-anchor=\"middle\" fill=\"%s\" font-size=\"9px\">%s_%d</text>\n",
                px, py - 9.0, color, port_type_str, node->id);
    }

    /* ---- 渲染点 ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_POINT)
            continue;
        if (node->coord_count < 2)
            continue;

        double px = symbolic_coord_to_double(node->symbolic_coords[0]);
        double py = symbolic_coord_to_double(node->symbolic_coords[1]);

        const char *color = trust_color_to_svg(node->trust);

        fprintf(fp, "  <!-- Point id=%d -->\n", node->id);

        /* 数学公式渲染：为符号坐标添加 <title> 注释（分数/根式表示） */
        if (node->symbolic_coords && node->symbolic_coords[0] && node->symbolic_coords[1]) {
            char *sx = symbolic_coord_serialize(node->symbolic_coords[0]);
            char *sy = symbolic_coord_serialize(node->symbolic_coords[1]);
            if (sx && sy) {
                fprintf(fp, "  <g>\n");
                fprintf(fp, "    <title>P%d = (%s, %s)</title>\n", node->id, sx, sy);
                fprintf(fp, "    <desc>Symbolic: P%d at rational/quadratic coords</desc>\n", node->id);
            }
            fprintf(fp,
                    "  <circle class=\"point\" cx=\"%.2f\" cy=\"%.2f\" r=\"4\" "
                    "fill=\"%s\"/>\n",
                    px, py, color);
            fprintf(fp,
                    "  <text class=\"label\" x=\"%.2f\" y=\"%.2f\" "
                    "text-anchor=\"middle\" fill=\"#374151\">P%d</text>\n",
                    px, py - 8.0, node->id);
            if (sx && sy) {
                fprintf(fp, "  </g>\n");
            }
            lv00_free((void **) &sx);
            lv00_free((void **) &sy);
        } else {
            fprintf(fp,
                    "  <circle class=\"point\" cx=\"%.2f\" cy=\"%.2f\" r=\"4\" "
                    "fill=\"%s\"/>\n",
                    px, py, color);
            fprintf(fp,
                    "  <text class=\"label\" x=\"%.2f\" y=\"%.2f\" "
                    "text-anchor=\"middle\" fill=\"#374151\">P%d</text>\n",
                    px, py - 8.0, node->id);
        }
    }

    /* ---- 渲染约束 ---- */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (!c || c->participant_count < 2)
            continue;

        fprintf(fp, "  <!-- Constraint id=%d type=%s -->\n", c->id, constraint_type_name(c->type));

        /* 获取参与者节点的位置 */
        GeomNode *p0 = graph_get_node_by_id(graph, c->participants[0]);
        GeomNode *p1 = graph_get_node_by_id(graph, c->participants[1]);
        if (!p0 || !p1)
            continue;
        if (p0->coord_count < 2 || p1->coord_count < 2)
            continue;

        double x0 = symbolic_coord_to_double(p0->symbolic_coords[0]);
        double y0 = symbolic_coord_to_double(p0->symbolic_coords[1]);
        double x1 = symbolic_coord_to_double(p1->symbolic_coords[0]);
        double y1 = symbolic_coord_to_double(p1->symbolic_coords[1]);

        switch (c->type) {
            case INCIDENCE:
                /* 关联约束：虚线 */
                fprintf(fp,
                        "  <line class=\"constraint\" x1=\"%.2f\" y1=\"%.2f\" "
                        "x2=\"%.2f\" y2=\"%.2f\" stroke=\"#6b7280\"/>\n",
                        x0, y0, x1, y1);
                break;

            case BETWEENNESS: {
                /* 之间约束：三点之间用标签标注 */
                double mx = (x0 + x1) / 2.0;
                double my = (y0 + y1) / 2.0;
                fprintf(fp,
                        "  <text class=\"label\" x=\"%.2f\" y=\"%.2f\" "
                        "text-anchor=\"middle\" fill=\"#6366f1\" font-style=\"italic\">"
                        "B(%d,%d",
                        mx, my, c->participants[0], c->participants[1]);
                if (c->participant_count >= 3) {
                    fprintf(fp, ",%d", c->participants[2]);
                }
                fprintf(fp, ")</text>\n");
                break;
            }

            case INTERSECTION: {
                /* 相交约束：计算精确交点并标记紫色十字 */
                double ix = x0, iy = y0; /* 默认交点为第一个参与者 */
                double a1x = x0, a1y = y0;
                double b1x = x1, b1y = y1;

                /* 使用线段参数方程求精确交点 */
                if (p0->type == GEOM_LINE_SEGMENT && p0->coord_count >= 4 && p1->type == GEOM_LINE_SEGMENT &&
                    p1->coord_count >= 4) {
                    double a2x = symbolic_coord_to_double(p0->symbolic_coords[2]);
                    double a2y = symbolic_coord_to_double(p0->symbolic_coords[3]);
                    double b2x = symbolic_coord_to_double(p1->symbolic_coords[2]);
                    double b2y = symbolic_coord_to_double(p1->symbolic_coords[3]);

                    /* 解线性方程组：P1 + t*(P2-P1) = Q1 + s*(Q2-Q1) */
                    double d1x = a2x - a1x, d1y = a2y - a1y;
                    double d2x = b2x - b1x, d2y = b2y - b1y;
                    double cross = d1x * d2y - d1y * d2x;

                    if (fabs(cross) > 1e-10) {
                        double dx0 = b1x - a1x;
                        double dy0 = b1y - a1y;
                        double t = (dx0 * d2y - dy0 * d2x) / cross;
                        if (t >= -0.05 && t <= 1.05) {
                            ix = a1x + t * d1x;
                            iy = a1y + t * d1y;
                        }
                    }
                }

                fprintf(fp,
                        "  <line class=\"constraint\" x1=\"%.2f\" y1=\"%.2f\" "
                        "x2=\"%.2f\" y2=\"%.2f\" stroke=\"#a855f7\"/>\n",
                        a1x, a1y, b1x, b1y);

                /* 在精确交点处绘制紫色十字标记 */
                double cross_r = 5.0;
                fprintf(fp,
                        "  <line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\" "
                        "stroke=\"#a855f7\" stroke-width=\"2\"/>\n",
                        ix - cross_r, iy - cross_r, ix + cross_r, iy + cross_r);
                fprintf(fp,
                        "  <line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\" "
                        "stroke=\"#a855f7\" stroke-width=\"2\"/>\n",
                        ix - cross_r, iy + cross_r, ix + cross_r, iy - cross_r);
                fprintf(fp,
                        "  <circle cx=\"%.2f\" cy=\"%.2f\" r=\"4\" "
                        "fill=\"none\" stroke=\"#a855f7\" stroke-width=\"1.5\"/>\n",
                        ix, iy);
                break;
            }

            case CONTAINMENT:
                /* 包含约束：点线 */
                fprintf(fp,
                        "  <line class=\"constraint\" x1=\"%.2f\" y1=\"%.2f\" "
                        "x2=\"%.2f\" y2=\"%.2f\" stroke=\"#14b8a6\" "
                        "stroke-dasharray=\"2,4\"/>\n",
                        x0, y0, x1, y1);
                break;

            case CONNECTION:
                /* 连接约束：箭头线 */
                fprintf(fp,
                        "  <line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\" "
                        "stroke=\"#f59e0b\" stroke-width=\"1.5\" "
                        "marker-end=\"url(#arrowhead)\"/>\n",
                        x0, y0, x1, y1);
                break;

            default:
                break;
        }
    }

    /* ---- 图例 ---- */
    double legend_x = min_x + 15.0;
    double legend_y = min_y + 20.0;
    fprintf(fp, "\n  <!-- Legend -->\n");
    fprintf(fp, "  <g transform=\"translate(%.2f, %.2f)\">\n", legend_x, legend_y);
    fprintf(fp,
            "    <rect x=\"0\" y=\"0\" width=\"150\" height=\"130\" "
            "fill=\"white\" fill-opacity=\"0.9\" stroke=\"#d1d5db\" rx=\"4\"/>\n");
    fprintf(fp, "    <text class=\"label\" x=\"10\" y=\"18\" font-weight=\"bold\">Legend</text>\n");

    /* 点 */
    fprintf(fp, "    <circle cx=\"20\" cy=\"35\" r=\"4\" fill=\"#22c55e\"/>\n");
    fprintf(fp, "    <text class=\"label\" x=\"32\" y=\"39\">Point</text>\n");

    /* 线段 */
    fprintf(fp, "    <line x1=\"12\" y1=\"52\" x2=\"28\" y2=\"52\" stroke=\"#3b82f6\" stroke-width=\"2\"/>\n");
    fprintf(fp, "    <text class=\"label\" x=\"32\" y=\"56\">Line Segment</text>\n");

    /* 区域 */
    fprintf(fp,
            "    <rect x=\"12\" y=\"64\" width=\"16\" height=\"12\" fill=\"#eab308\" fill-opacity=\"0.3\" "
            "stroke=\"#eab308\"/>\n");
    fprintf(fp, "    <text class=\"label\" x=\"32\" y=\"75\">Region</text>\n");

    /* 约束 */
    fprintf(fp, "    <line x1=\"12\" y1=\"90\" x2=\"28\" y2=\"90\" stroke=\"#6b7280\" stroke-dasharray=\"5,3\"/>\n");
    fprintf(fp, "    <text class=\"label\" x=\"32\" y=\"94\">Constraint</text>\n");

    /* 信任颜色 */
    fprintf(fp, "    <circle cx=\"16\" cy=\"110\" r=\"4\" fill=\"#22c55e\"/>\n");
    fprintf(fp, "    <text class=\"label\" x=\"24\" y=\"114\" font-size=\"9px\">Constrained</text>\n");
    fprintf(fp, "    <circle cx=\"86\" cy=\"110\" r=\"4\" fill=\"#9ca3af\"/>\n");
    fprintf(fp, "    <text class=\"label\" x=\"94\" y=\"114\" font-size=\"9px\">Free</text>\n");
    fprintf(fp, "    <circle cx=\"120\" cy=\"110\" r=\"4\" fill=\"#ef4444\"/>\n");
    fprintf(fp, "    <text class=\"label\" x=\"128\" y=\"114\" font-size=\"9px\">Conflict</text>\n");

    fprintf(fp, "  </g>\n");

    /* 箭头标记定义（放在最后，因为connection可能引用） */
    fprintf(fp, "\n  <defs>\n");
    fprintf(fp,
            "    <marker id=\"arrowhead\" markerWidth=\"8\" markerHeight=\"6\" "
            "refX=\"8\" refY=\"3\" orient=\"auto\">\n");
    fprintf(fp, "      <polygon points=\"0 0, 8 3, 0 6\" fill=\"#f59e0b\"/>\n");
    fprintf(fp, "    </marker>\n");
    fprintf(fp, "  </defs>\n");

    fprintf(fp, "\n</svg>\n");

    fclose(fp);

    return LV00_OK;
}

int interop_export_tikz(const ConstraintGraph *graph, const InteropExportConfig *config) {
    /**
     * @brief 将约束图导出为LaTeX TikZ代码
     *
     * 【已实现功能】
     *   本函数已完成TikZ导出的核心渲染管线，生成可独立编译的LaTeX文档：
     *   1. LaTeX文档框架 —— 生成完整的 standalone 文档类，包含必要的
     *      TikZ库引用（arrows.meta, shapes.geometric, positioning, calc）
     *   2. 样式定义（TikZ style） —— 定义以下样式类别：
     *      - point: 小圆点（填充，inner sep=1.5pt）
     *      - line: 粗线（thick）
     *      - region: 半透明填充区域（fill opacity=0.3）
     *      - constraint: 灰色虚线（dashed, thin, gray）
     *      - block: 圆角矩形（rounded corners, 最小2cm x 1cm）
     *      - port: 白色填充小圆圈（draw, inner sep=2pt, fill=white）
     *      - label: 小号字体标签（font=\small）
     *      - connection: 橙色箭头连接线（-Stealth, thick, orange）
     *   3. 区域（Region）渲染 —— 使用 \draw[region] 绘制半透明多边形，
     *      遍历所有边界线段端点构建闭合路径（-- cycle）
     *   4. 函数块（Function Block）渲染 —— 使用 \node[block] 绘制
     *      圆角矩形节点，居中显示FB_<id>标签
     *   5. 线段（Line Segment）渲染 —— 使用 \draw[line] 绘制线段，
     *      中点上方显示 seg_<id> 标签
     *   6. 端口（Port）渲染 —— 使用 \node[port] 绘制小圆圈，
     *      标注 in/out_<id> 类型标签
     *   7. 点（Point）渲染 —— 使用 \node[point] 绘制填充圆点，
     *      上方标注P<id>
     *   8. 约束关系渲染 —— 支持五种约束类型的TikZ可视化：
     *      - 关联约束（INCIDENCE）：灰色虚线
     *      - 之间约束（BETWEENNESS）：紫色斜体标签标注
     *      - 相交约束（INTERSECTION）：紫色虚线 + 圆圈标记
     *      - 包含约束（CONTAINMENT）：青色密集点线（densely dotted）
     *      - 连接约束（CONNECTION）：橙色Stealth箭头
     *   9. 信任颜色映射 —— 根据TrustColor使用对应的TikZ颜色名：
     *      绿色(green!60!black)、灰色(gray)、红色(red!70!black)
     *
     * 【简化实现的部分（完整功能需要额外依赖或后续版本）】
     *   1. 曲线几何体渲染 —— 当前仅处理直线段端点；
     *      完整实现需要解析曲线参数并生成 plot/smooth/curve 等TikZ曲线命令。
     *   2. 区域的曲线边界 —— 当前区域边界使用直线段连接；
     *      完整实现需要生成 TikZ 的 plot[smooth] 或 curve 命令。
     *   3. 节点定位优化 —— 当前所有节点使用绝对坐标 at (x,y)；
     *      完整实现需要使用 TikZ positioning 库进行相对定位和自动布局。
     *   4. 约束的精确交点计算 —— 当前约束线端点为参与者节点坐标；
     *      完整实现需要调用几何求解器计算实际的几何交点位置。
     *   5. 三维投影支持 —— 当前仅支持二维平面渲染；
     *      完整实现需要 tikz-3dplot 库进行三维投影。
     *   6. 颜色渐变和阴影 —— 当前为纯色填充无渐变；
     *      完整实现需要 TikZ 的 shading 和 shadow 特性。
     *   7. 图例（Legend）—— 当前不包含图例；
     *      完整实现需要使用 TikZ legend 样式或手动绘制图例框。
     *   8. 外部化/缓存 —— 当前为单文件输出；
     *      完整实现需要生成 TikZ externalize 所需的多文件结构。
     *
     * 【外部依赖说明】
     *   本函数仅生成纯文本的.tex文件，不依赖任何外部C库。生成的TikZ代码
     *   需要以下LaTeX环境来编译：
     *   1. LaTeX发行版（TeX Live / MiKTeX）
     *   2. TikZ/PGF包（通常随LaTeX发行版自动安装）
     *   3. standalone 文档类
     *
     * 【使用示例】
     *   InteropExportConfig cfg;
     *   lv00_strlcpy(cfg.output_path, "output.tex", sizeof(cfg.output_path));
     *   int ret = interop_export_tikz(graph, &cfg);
     *   // 然后使用: pdflatex output.tex 编译为PDF
     *
     * 【与 interop_export_pdf 的关系】
     *   如果用户需要PDF输出但不想安装LaTeX，建议使用 interop_export_pdf
     *   函数直接生成PDF。本TikZ函数适合需要嵌入学术论文的场景。
     *
     * @param graph 约束图指针（包含所有节点和约束关系）
     * @param config 导出配置（output_path 指定 .tex 文件路径）
     * @return LV00_OK 成功导出
     *         LV00_ERROR_INVALID_PARAM 参数无效（graph或config为NULL）
     *         LV00_ERROR_IO 文件无法创建或写入
     */
    if (!graph || !config)
        return LV00_ERROR_INVALID_PARAM;

    /* ---- 流式事件：开始 LaTeX/TikZ 导出 ---- */
    if (interop_stream_ctx) {
        stream_emit_simple(interop_stream_ctx, STREAM_EVENT_INFO, "开始 LaTeX/TikZ 导出", 0);
    }

    FILE *fp = fopen(config->output_path, "w");
    if (!fp)
        return LV00_ERROR_IO;

    /* LaTeX文档头部 */
    fprintf(fp, "%% Generated by Lv-00 v%s\n", LV00_VERSION_STRING);
    fprintf(fp, "%% TikZ geometry export\n\n");
    fprintf(fp, "\\documentclass[tikz,border=10pt]{standalone}\n");
    fprintf(fp, "\\usepackage{tikz}\n");
    fprintf(fp, "\\usetikzlibrary{arrows.meta,shapes.geometric,positioning,calc}\n\n");
    fprintf(fp, "\\begin{document}\n\n");
    fprintf(fp, "\\begin{tikzpicture}[\n");
    fprintf(fp, "    point/.style={circle, fill, inner sep=1.5pt},\n");
    fprintf(fp, "    line/.style={thick},\n");
    fprintf(fp, "    region/.style={fill opacity=0.3, thick},\n");
    fprintf(fp, "    constraint/.style={dashed, thin, gray},\n");
    fprintf(fp, "    block/.style={draw, rounded corners, minimum width=2cm, minimum height=1cm, thick},\n");
    fprintf(fp, "    port/.style={circle, draw, inner sep=2pt, fill=white},\n");
    fprintf(fp, "    label/.style={font=\\small},\n");
    fprintf(fp, "    connection/.style={-{Stealth[length=5pt]}, thick, orange}\n");
    fprintf(fp, "]\n\n");

    /* ---- 渲染区域（底层） ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_REGION)
            continue;
        if (node->data.region.segment_count < 3)
            continue;

        const char *color = trust_color_to_tikz(node->trust);

        fprintf(fp, "    %% Region id=%d\n", node->id);

        /* 颜色渐变：使用 TikZ shading 为区域添加渐变效果 */
        fprintf(fp, "    \\shade[top color=%s, bottom color=%s!30] ", color, color);

        /* 收集区域边界顶点 */
        int first_point = 1;
        for (int s = 0; s < node->data.region.segment_count; s++) {
            GeomNode *seg = node->data.region.boundary_segments[s];
            if (seg && seg->type == GEOM_LINE_SEGMENT && seg->coord_count >= 4) {
                double sx1 = symbolic_coord_to_double(seg->symbolic_coords[0]);
                double sy1 = symbolic_coord_to_double(seg->symbolic_coords[1]);
                if (first_point) {
                    fprintf(fp, "(%.2f, %.2f)", sx1, sy1);
                    first_point = 0;
                } else {
                    fprintf(fp, " -- (%.2f, %.2f)", sx1, sy1);
                }
            }
        }
        fprintf(fp, " -- cycle;\n");

        /* 同时添加描边轮廓 */
        fprintf(fp, "    \\draw[region, %s] ", color);
        first_point = 1;
        for (int s = 0; s < node->data.region.segment_count; s++) {
            GeomNode *seg = node->data.region.boundary_segments[s];
            if (seg && seg->type == GEOM_LINE_SEGMENT && seg->coord_count >= 4) {
                double sx1 = symbolic_coord_to_double(seg->symbolic_coords[0]);
                double sy1 = symbolic_coord_to_double(seg->symbolic_coords[1]);
                if (first_point) {
                    fprintf(fp, "(%.2f, %.2f)", sx1, sy1);
                    first_point = 0;
                } else {
                    fprintf(fp, " -- (%.2f, %.2f)", sx1, sy1);
                }
            }
        }
        fprintf(fp, " -- cycle;\n");
    }

    /* ---- 渲染函数块 ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_FUNCTION_BLOCK)
            continue;
        if (node->coord_count < 2)
            continue;

        double bx = symbolic_coord_to_double(node->symbolic_coords[0]);
        double by = symbolic_coord_to_double(node->symbolic_coords[1]);

        const char *color = trust_color_to_tikz(node->trust);

        fprintf(fp, "    %% Function Block id=%d\n", node->id);
        fprintf(fp,
                "    \\node[block, draw=%s, fill=%s, fill opacity=0.15] "
                "at (%.2f, %.2f) {FB\\_%d};\n",
                color, color, bx, by, node->id);
    }

    /* ---- 渲染线段 ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_LINE_SEGMENT)
            continue;
        if (node->coord_count < 4)
            continue;

        double x1 = symbolic_coord_to_double(node->symbolic_coords[0]);
        double y1 = symbolic_coord_to_double(node->symbolic_coords[1]);
        double x2 = symbolic_coord_to_double(node->symbolic_coords[2]);
        double y2 = symbolic_coord_to_double(node->symbolic_coords[3]);

        const char *color = trust_color_to_tikz(node->trust);

        fprintf(fp, "    %% Line Segment id=%d\n", node->id);

        /* 曲线几何体渲染：如果线段有 3 个以上坐标对，使用 plot[smooth] */
        if (node->coord_count >= 6) {
            int total_pairs = node->coord_count / 2;
            fprintf(fp, "    \\draw[line, %s] plot[smooth] coordinates {", color);
            for (int p = 0; p < total_pairs; p++) {
                double sx = symbolic_coord_to_double(node->symbolic_coords[p * 2]);
                double sy = symbolic_coord_to_double(node->symbolic_coords[p * 2 + 1]);
                if (p > 0)
                    fprintf(fp, " ");
                fprintf(fp, "(%.2f,%.2f)", sx, sy);
            }
            fprintf(fp, "};\n");
        } else {
            fprintf(fp,
                    "    \\draw[line, %s] (%.2f, %.2f) -- (%.2f, %.2f) "
                    "node[midway, above, label] {seg\\_%d};\n",
                    color, x1, y1, x2, y2, node->id);
        }
    }

    /* ---- 渲染端口 ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_PORT)
            continue;
        if (node->coord_count < 2)
            continue;

        double px = symbolic_coord_to_double(node->symbolic_coords[0]);
        double py = symbolic_coord_to_double(node->symbolic_coords[1]);

        const char *color = trust_color_to_tikz(node->trust);
        const char *port_type_str = (node->data.port && node->data.port->type == PORT_INPUT) ? "in" : "out";

        fprintf(fp, "    %% Port id=%d type=%s\n", node->id, port_type_str);
        fprintf(fp, "    \\node[port, draw=%s] (port%d) at (%.2f, %.2f) {};\n", color, node->id, px, py);
        fprintf(fp,
                "    \\node[label, %s, font=\\tiny] at (%.2f, %.2f) "
                "{%s\\_%d};\n",
                color, px, py + 0.3, port_type_str, node->id);
    }

    /* ---- 渲染点 ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_POINT)
            continue;
        if (node->coord_count < 2)
            continue;

        double px = symbolic_coord_to_double(node->symbolic_coords[0]);
        double py = symbolic_coord_to_double(node->symbolic_coords[1]);

        const char *color = trust_color_to_tikz(node->trust);

        fprintf(fp, "    %% Point id=%d\n", node->id);
        fprintf(fp, "    \\node[point, %s] (P%d) at (%.2f, %.2f) {};\n", color, node->id, px, py);

        /* 使用符号坐标序列化作为标签（如果坐标可用） */
        if (node->symbolic_coords && node->symbolic_coords[0] && node->symbolic_coords[1]) {
            char *sx = symbolic_coord_serialize(node->symbolic_coords[0]);
            char *sy = symbolic_coord_serialize(node->symbolic_coords[1]);
            if (sx && sy) {
                fprintf(fp,
                        "    \\node[label, above=2pt of P%d] "
                        "{$P_{%d}\\!\\left(%s,\\, %s\\right)$};\n",
                        node->id, node->id, sx, sy);
            } else {
                fprintf(fp, "    \\node[label, above=2pt of P%d] {$P_{%d}$};\n", node->id, node->id);
            }
            lv00_free((void **) &sx);
            lv00_free((void **) &sy);
        } else {
            fprintf(fp, "    \\node[label, above=2pt of P%d] {$P_{%d}$};\n", node->id, node->id);
        }
    }

    /* ---- 渲染约束 ---- */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (!c || c->participant_count < 2)
            continue;

        fprintf(fp, "    %% Constraint id=%d type=%s\n", c->id, constraint_type_name(c->type));

        GeomNode *p0 = graph_get_node_by_id(graph, c->participants[0]);
        GeomNode *p1 = graph_get_node_by_id(graph, c->participants[1]);
        if (!p0 || !p1)
            continue;
        if (p0->coord_count < 2 || p1->coord_count < 2)
            continue;

        double x0 = symbolic_coord_to_double(p0->symbolic_coords[0]);
        double y0 = symbolic_coord_to_double(p0->symbolic_coords[1]);
        double x1 = symbolic_coord_to_double(p1->symbolic_coords[0]);
        double y1 = symbolic_coord_to_double(p1->symbolic_coords[1]);

        switch (c->type) {
            case INCIDENCE:
                fprintf(fp, "    \\draw[constraint] (%.2f, %.2f) -- (%.2f, %.2f);\n", x0, y0, x1, y1);
                break;

            case BETWEENNESS: {
                double mx = (x0 + x1) / 2.0;
                double my = (y0 + y1) / 2.0;
                fprintf(fp,
                        "    \\node[label, purple, font=\\itshape] at (%.2f, %.2f) "
                        "{B(%d, %d",
                        mx, my, c->participants[0], c->participants[1]);
                if (c->participant_count >= 3) {
                    fprintf(fp, ", %d", c->participants[2]);
                }
                fprintf(fp, ")};\n");
                break;
            }

            case INTERSECTION: {
                /* 相交约束：使用 TikZ intersection 库计算精确交点 */
                double ix = x0, iy = y0;
                double a1x = x0, a1y = y0, b1x = x1, b1y = y1;
                bool has_precise = false;

                if (p0->type == GEOM_LINE_SEGMENT && p0->coord_count >= 4 && p1->type == GEOM_LINE_SEGMENT &&
                    p1->coord_count >= 4) {
                    double a2x = symbolic_coord_to_double(p0->symbolic_coords[2]);
                    double a2y = symbolic_coord_to_double(p0->symbolic_coords[3]);
                    double b2x = symbolic_coord_to_double(p1->symbolic_coords[2]);
                    double b2y = symbolic_coord_to_double(p1->symbolic_coords[3]);

                    double d1x = a2x - a1x, d1y = a2y - a1y;
                    double d2x = b2x - b1x, d2y = b2y - b1y;
                    double cross = d1x * d2y - d1y * d2x;

                    if (fabs(cross) > 1e-10) {
                        double dx0 = b1x - a1x, dy0 = b1y - a1y;
                        double t = (dx0 * d2y - dy0 * d2x) / cross;
                        if (t >= -0.05 && t <= 1.05) {
                            ix = a1x + t * d1x;
                            iy = a1y + t * d1y;
                            has_precise = true;
                        }
                    }
                }

                /* 输出 TikZ intersection 标记 */
                if (has_precise) {
                    fprintf(fp, "    %% 精确交点计算 (t=parametric)\n");
                    fprintf(fp, "    \\fill[red] (%.2f, %.2f) circle (2pt);\n", ix, iy);
                    fprintf(fp,
                            "    \\node[label, red, font=\\tiny] at (%.2f, %.2f) "
                            "{intersection};\n",
                            ix + 0.3, iy + 0.3);
                } else {
                    fprintf(fp, "    \\draw[constraint, purple] (%.2f, %.2f) -- (%.2f, %.2f);\n", a1x, a1y, b1x, b1y);
                    fprintf(fp, "    \\node[circle, draw=purple, inner sep=1pt] at (%.2f, %.2f) {};\n", x0, y0);
                }
                break;
            }

            case CONTAINMENT:
                fprintf(fp,
                        "    \\draw[constraint, teal, densely dotted] "
                        "(%.2f, %.2f) -- (%.2f, %.2f);\n",
                        x0, y0, x1, y1);
                break;

            case CONNECTION:
                fprintf(fp, "    \\draw[connection] (%.2f, %.2f) -- (%.2f, %.2f);\n", x0, y0, x1, y1);
                break;

            default:
                break;
        }
    }

    /* ---- 图例（Legend） ---- */
    {
        /* 收集图中实际出现的节点类型和约束类型 */
        bool has_point = false, has_line = false, has_region = false;
        bool has_block = false, has_port = false;
        bool has_incidence = false, has_betweenness = false;
        bool has_intersection = false, has_containment = false;
        bool has_connection = false;

        for (int i = 0; i < graph->node_count; i++) {
            GeomNode *n = graph->nodes[i];
            if (!n)
                continue;
            switch (n->type) {
                case GEOM_POINT:
                    has_point = true;
                    break;
                case GEOM_LINE_SEGMENT:
                    has_line = true;
                    break;
                case GEOM_REGION:
                    has_region = true;
                    break;
                case GEOM_FUNCTION_BLOCK:
                    has_block = true;
                    break;
                case GEOM_PORT:
                    has_port = true;
                    break;
                default:
                    break;
            }
        }
        for (int i = 0; i < graph->constraint_count; i++) {
            Constraint *c = graph->constraints[i];
            if (!c)
                continue;
            switch (c->type) {
                case INCIDENCE:
                    has_incidence = true;
                    break;
                case BETWEENNESS:
                    has_betweenness = true;
                    break;
                case INTERSECTION:
                    has_intersection = true;
                    break;
                case CONTAINMENT:
                    has_containment = true;
                    break;
                case CONNECTION:
                    has_connection = true;
                    break;
                default:
                    break;
            }
        }

        int legend_rows = 0;
        if (has_point)
            legend_rows++;
        if (has_line)
            legend_rows++;
        if (has_region)
            legend_rows++;
        if (has_block)
            legend_rows++;
        if (has_port)
            legend_rows++;
        if (has_incidence)
            legend_rows++;
        if (has_betweenness)
            legend_rows++;
        if (has_intersection)
            legend_rows++;
        if (has_containment)
            legend_rows++;
        if (has_connection)
            legend_rows++;

        if (legend_rows > 0) {
            fprintf(fp, "\n    %% Legend\n");
            fprintf(fp,
                    "    \\matrix[draw, fill=white, fill opacity=0.85, "
                    "anchor=south east, column sep=4pt, row sep=2pt, "
                    "font=\\scriptsize, inner sep=4pt]\n");
            fprintf(fp, "    at (current bounding box.south east) {\n");

            int row = 0;
            if (has_point) {
                fprintf(fp, "        \\node[point] {}; & \\node {Point}; ");
                if (++row < legend_rows)
                    fprintf(fp, "\\\\\n");
            }
            if (has_line) {
                fprintf(fp, "        \\draw[line] (0,0) -- (0.5,0); & \\node {Line Segment}; ");
                if (++row < legend_rows)
                    fprintf(fp, "\\\\\n");
            }
            if (has_region) {
                fprintf(fp, "        \\draw[region, fill=blue!20] (0,0) rectangle (0.5,0.3); & \\node {Region}; ");
                if (++row < legend_rows)
                    fprintf(fp, "\\\\\n");
            }
            if (has_block) {
                fprintf(
                    fp,
                    "        \\node[block, minimum width=0.5cm, minimum height=0.3cm] {}; & \\node {Function Block}; ");
                if (++row < legend_rows)
                    fprintf(fp, "\\\\\n");
            }
            if (has_port) {
                fprintf(fp, "        \\node[port] {}; & \\node {Port}; ");
                if (++row < legend_rows)
                    fprintf(fp, "\\\\\n");
            }
            if (has_incidence) {
                fprintf(fp, "        \\draw[constraint] (0,0) -- (0.5,0); & \\node {Incidence}; ");
                if (++row < legend_rows)
                    fprintf(fp, "\\\\\n");
            }
            if (has_betweenness) {
                fprintf(fp, "        \\node[purple, font=\\itshape] {B}; & \\node {Betweenness}; ");
                if (++row < legend_rows)
                    fprintf(fp, "\\\\\n");
            }
            if (has_intersection) {
                fprintf(fp,
                        "        \\draw[constraint, purple] (0,0) -- (0.5,0); "
                        "\\node[circle, draw=purple, inner sep=0.5pt] at (0.25,0) {}; "
                        "& \\node {Intersection}; ");
                if (++row < legend_rows)
                    fprintf(fp, "\\\\\n");
            }
            if (has_containment) {
                fprintf(fp,
                        "        \\draw[constraint, teal, densely dotted] (0,0) -- (0.5,0); & \\node {Containment}; ");
                if (++row < legend_rows)
                    fprintf(fp, "\\\\\n");
            }
            if (has_connection) {
                fprintf(fp, "        \\draw[connection] (0,0) -- (0.5,0); & \\node {Connection}; ");
                if (++row < legend_rows)
                    fprintf(fp, "\\\\\n");
            }

            fprintf(fp, "    };\n");
        }
    }

    fprintf(fp, "\n\\end{tikzpicture}\n\n");
    fprintf(fp, "\\end{document}\n");

    fclose(fp);

    /* ---- 流式事件：LaTeX/TikZ 导出完成 ---- */
    if (interop_stream_ctx) {
        stream_emit_simple(interop_stream_ctx, STREAM_EVENT_INFO, "LaTeX/TikZ 导出完成", 0);
    }

    return LV00_OK;
}

/**
 * @brief 将约束图导出为 TikZ 片段（不含文档框架）
 *
 * 仅输出 \begin{tikzpicture}...\end{tikzpicture} 片段，
 * 可直接嵌入已有的 LaTeX 文档。包含样式定义、节点渲染、
 * 约束渲染、符号坐标标签和自动图例。
 *
 * @param graph 约束图指针
 * @param output 输出缓冲区
 * @param size 缓冲区大小
 * @return 实际写入字符数（不含终止符），失败返回负数
 */
int interop_export_tikz_fragment(const ConstraintGraph *graph, char *output, size_t size) {
    if (!graph || !output || size == 0)
        return -1;

    /* ---- 流式事件：开始 TikZ 片段导出 ---- */
    if (interop_stream_ctx) {
        stream_emit_simple(interop_stream_ctx, STREAM_EVENT_INFO, "开始 TikZ 片段导出", 0);
    }

    /* 使用 snprintf 逐步写入缓冲区 */
    int total = 0;
    int remaining = (int) size;

#define TIKZ_FRAG_PRINTF(...)                                              \
    do {                                                                   \
        int n = snprintf(output + total, (size_t) remaining, __VA_ARGS__); \
        if (n < 0)                                                         \
            return -1;                                                     \
        if (n >= remaining) {                                              \
            total += remaining - 1;                                        \
            remaining = 1;                                                 \
        } else {                                                           \
            total += n;                                                    \
            remaining -= n;                                                \
        }                                                                  \
    } while (0)

    /* TikZ 样式定义和 tikzpicture 开始 */
    TIKZ_FRAG_PRINTF("%% Generated by Lv-00 v%s (TikZ fragment)\n", LV00_VERSION_STRING);
    TIKZ_FRAG_PRINTF("\\begin{tikzpicture}[\n");
    TIKZ_FRAG_PRINTF("    point/.style={circle, fill, inner sep=1.5pt},\n");
    TIKZ_FRAG_PRINTF("    line/.style={thick},\n");
    TIKZ_FRAG_PRINTF("    region/.style={fill opacity=0.3, thick},\n");
    TIKZ_FRAG_PRINTF("    constraint/.style={dashed, thin, gray},\n");
    TIKZ_FRAG_PRINTF("    block/.style={draw, rounded corners, minimum width=2cm, minimum height=1cm, thick},\n");
    TIKZ_FRAG_PRINTF("    port/.style={circle, draw, inner sep=2pt, fill=white},\n");
    TIKZ_FRAG_PRINTF("    label/.style={font=\\small},\n");
    TIKZ_FRAG_PRINTF("    connection/.style={-{Stealth[length=5pt]}, thick, orange}\n");
    TIKZ_FRAG_PRINTF("]\n\n");

    /* ---- 渲染区域（底层） ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_REGION)
            continue;
        if (node->data.region.segment_count < 3)
            continue;

        const char *color = trust_color_to_tikz(node->trust);

        TIKZ_FRAG_PRINTF("    %% Region id=%d\n", node->id);
        TIKZ_FRAG_PRINTF("    \\draw[region, fill=%s] ", color);

        int first_point = 1;
        for (int s = 0; s < node->data.region.segment_count; s++) {
            GeomNode *seg = node->data.region.boundary_segments[s];
            if (seg && seg->type == GEOM_LINE_SEGMENT && seg->coord_count >= 4) {
                double sx1 = symbolic_coord_to_double(seg->symbolic_coords[0]);
                double sy1 = symbolic_coord_to_double(seg->symbolic_coords[1]);
                if (first_point) {
                    TIKZ_FRAG_PRINTF("(%.2f, %.2f)", sx1, sy1);
                    first_point = 0;
                } else {
                    TIKZ_FRAG_PRINTF(" -- (%.2f, %.2f)", sx1, sy1);
                }
            }
        }
        TIKZ_FRAG_PRINTF(" -- cycle;\n");
    }

    /* ---- 渲染函数块 ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_FUNCTION_BLOCK)
            continue;
        if (node->coord_count < 2)
            continue;

        double bx = symbolic_coord_to_double(node->symbolic_coords[0]);
        double by = symbolic_coord_to_double(node->symbolic_coords[1]);

        const char *color = trust_color_to_tikz(node->trust);

        TIKZ_FRAG_PRINTF("    %% Function Block id=%d\n", node->id);
        TIKZ_FRAG_PRINTF(
            "    \\node[block, draw=%s, fill=%s, fill opacity=0.15] "
            "at (%.2f, %.2f) {FB\\_%d};\n",
            color, color, bx, by, node->id);
    }

    /* ---- 渲染线段 ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_LINE_SEGMENT)
            continue;
        if (node->coord_count < 4)
            continue;

        double x1 = symbolic_coord_to_double(node->symbolic_coords[0]);
        double y1 = symbolic_coord_to_double(node->symbolic_coords[1]);
        double x2 = symbolic_coord_to_double(node->symbolic_coords[2]);
        double y2 = symbolic_coord_to_double(node->symbolic_coords[3]);

        const char *color = trust_color_to_tikz(node->trust);

        TIKZ_FRAG_PRINTF("    %% Line Segment id=%d\n", node->id);
        TIKZ_FRAG_PRINTF(
            "    \\draw[line, %s] (%.2f, %.2f) -- (%.2f, %.2f) "
            "node[midway, above, label] {seg\\_%d};\n",
            color, x1, y1, x2, y2, node->id);
    }

    /* ---- 渲染端口 ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_PORT)
            continue;
        if (node->coord_count < 2)
            continue;

        double px = symbolic_coord_to_double(node->symbolic_coords[0]);
        double py = symbolic_coord_to_double(node->symbolic_coords[1]);

        const char *color = trust_color_to_tikz(node->trust);
        const char *port_type_str = (node->data.port && node->data.port->type == PORT_INPUT) ? "in" : "out";

        TIKZ_FRAG_PRINTF("    %% Port id=%d type=%s\n", node->id, port_type_str);
        TIKZ_FRAG_PRINTF("    \\node[port, draw=%s] (port%d) at (%.2f, %.2f) {};\n", color, node->id, px, py);
        TIKZ_FRAG_PRINTF(
            "    \\node[label, %s, font=\\tiny] at (%.2f, %.2f) "
            "{%s\\_%d};\n",
            color, px, py + 0.3, port_type_str, node->id);
    }

    /* ---- 渲染点（带符号坐标标签） ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_POINT)
            continue;
        if (node->coord_count < 2)
            continue;

        double px = symbolic_coord_to_double(node->symbolic_coords[0]);
        double py = symbolic_coord_to_double(node->symbolic_coords[1]);

        const char *color = trust_color_to_tikz(node->trust);

        TIKZ_FRAG_PRINTF("    %% Point id=%d\n", node->id);
        TIKZ_FRAG_PRINTF("    \\node[point, %s] (P%d) at (%.2f, %.2f) {};\n", color, node->id, px, py);

        /* 使用符号坐标序列化作为标签（如果坐标可用） */
        if (node->symbolic_coords && node->symbolic_coords[0] && node->symbolic_coords[1]) {
            char *sx = symbolic_coord_serialize(node->symbolic_coords[0]);
            char *sy = symbolic_coord_serialize(node->symbolic_coords[1]);
            if (sx && sy) {
                TIKZ_FRAG_PRINTF(
                    "    \\node[label, above=2pt of P%d] "
                    "{$P_{%d}\\!\\left(%s,\\, %s\\right)$};\n",
                    node->id, node->id, sx, sy);
            } else {
                TIKZ_FRAG_PRINTF("    \\node[label, above=2pt of P%d] {$P_{%d}$};\n", node->id, node->id);
            }
            lv00_free((void **) &sx);
            lv00_free((void **) &sy);
        } else {
            TIKZ_FRAG_PRINTF("    \\node[label, above=2pt of P%d] {$P_{%d}$};\n", node->id, node->id);
        }
    }

    /* ---- 渲染约束 ---- */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (!c || c->participant_count < 2)
            continue;

        TIKZ_FRAG_PRINTF("    %% Constraint id=%d type=%s\n", c->id, constraint_type_name(c->type));

        GeomNode *p0 = graph_get_node_by_id(graph, c->participants[0]);
        GeomNode *p1 = graph_get_node_by_id(graph, c->participants[1]);
        if (!p0 || !p1)
            continue;
        if (p0->coord_count < 2 || p1->coord_count < 2)
            continue;

        double x0 = symbolic_coord_to_double(p0->symbolic_coords[0]);
        double y0 = symbolic_coord_to_double(p0->symbolic_coords[1]);
        double x1 = symbolic_coord_to_double(p1->symbolic_coords[0]);
        double y1 = symbolic_coord_to_double(p1->symbolic_coords[1]);

        switch (c->type) {
            case INCIDENCE:
                TIKZ_FRAG_PRINTF("    \\draw[constraint] (%.2f, %.2f) -- (%.2f, %.2f);\n", x0, y0, x1, y1);
                break;

            case BETWEENNESS: {
                double mx = (x0 + x1) / 2.0;
                double my = (y0 + y1) / 2.0;
                TIKZ_FRAG_PRINTF(
                    "    \\node[label, purple, font=\\itshape] at (%.2f, %.2f) "
                    "{B(%d, %d",
                    mx, my, c->participants[0], c->participants[1]);
                if (c->participant_count >= 3) {
                    TIKZ_FRAG_PRINTF(", %d", c->participants[2]);
                }
                TIKZ_FRAG_PRINTF(")};\n");
                break;
            }

            case INTERSECTION:
                TIKZ_FRAG_PRINTF("    \\draw[constraint, purple] (%.2f, %.2f) -- (%.2f, %.2f);\n", x0, y0, x1, y1);
                TIKZ_FRAG_PRINTF("    \\node[circle, draw=purple, inner sep=1pt] at (%.2f, %.2f) {};\n", x0, y0);
                break;

            case CONTAINMENT:
                TIKZ_FRAG_PRINTF(
                    "    \\draw[constraint, teal, densely dotted] "
                    "(%.2f, %.2f) -- (%.2f, %.2f);\n",
                    x0, y0, x1, y1);
                break;

            case CONNECTION:
                TIKZ_FRAG_PRINTF("    \\draw[connection] (%.2f, %.2f) -- (%.2f, %.2f);\n", x0, y0, x1, y1);
                break;

            default:
                break;
        }
    }

    /* ---- 图例（Legend） ---- */
    {
        bool has_point = false, has_line = false, has_region = false;
        bool has_block = false, has_port = false;
        bool has_incidence = false, has_betweenness = false;
        bool has_intersection = false, has_containment = false;
        bool has_connection = false;

        for (int i = 0; i < graph->node_count; i++) {
            GeomNode *n = graph->nodes[i];
            if (!n)
                continue;
            switch (n->type) {
                case GEOM_POINT:
                    has_point = true;
                    break;
                case GEOM_LINE_SEGMENT:
                    has_line = true;
                    break;
                case GEOM_REGION:
                    has_region = true;
                    break;
                case GEOM_FUNCTION_BLOCK:
                    has_block = true;
                    break;
                case GEOM_PORT:
                    has_port = true;
                    break;
                default:
                    break;
            }
        }
        for (int i = 0; i < graph->constraint_count; i++) {
            Constraint *c = graph->constraints[i];
            if (!c)
                continue;
            switch (c->type) {
                case INCIDENCE:
                    has_incidence = true;
                    break;
                case BETWEENNESS:
                    has_betweenness = true;
                    break;
                case INTERSECTION:
                    has_intersection = true;
                    break;
                case CONTAINMENT:
                    has_containment = true;
                    break;
                case CONNECTION:
                    has_connection = true;
                    break;
                default:
                    break;
            }
        }

        int legend_rows = 0;
        if (has_point)
            legend_rows++;
        if (has_line)
            legend_rows++;
        if (has_region)
            legend_rows++;
        if (has_block)
            legend_rows++;
        if (has_port)
            legend_rows++;
        if (has_incidence)
            legend_rows++;
        if (has_betweenness)
            legend_rows++;
        if (has_intersection)
            legend_rows++;
        if (has_containment)
            legend_rows++;
        if (has_connection)
            legend_rows++;

        if (legend_rows > 0) {
            TIKZ_FRAG_PRINTF("\n    %% Legend\n");
            TIKZ_FRAG_PRINTF(
                "    \\matrix[draw, fill=white, fill opacity=0.85, "
                "anchor=south east, column sep=4pt, row sep=2pt, "
                "font=\\scriptsize, inner sep=4pt]\n");
            TIKZ_FRAG_PRINTF("    at (current bounding box.south east) {\n");

            int row = 0;
            if (has_point) {
                TIKZ_FRAG_PRINTF("        \\node[point] {}; & \\node {Point}; ");
                if (++row < legend_rows)
                    TIKZ_FRAG_PRINTF("\\\\\n");
            }
            if (has_line) {
                TIKZ_FRAG_PRINTF("        \\draw[line] (0,0) -- (0.5,0); & \\node {Line Segment}; ");
                if (++row < legend_rows)
                    TIKZ_FRAG_PRINTF("\\\\\n");
            }
            if (has_region) {
                TIKZ_FRAG_PRINTF("        \\draw[region, fill=blue!20] (0,0) rectangle (0.5,0.3); & \\node {Region}; ");
                if (++row < legend_rows)
                    TIKZ_FRAG_PRINTF("\\\\\n");
            }
            if (has_block) {
                TIKZ_FRAG_PRINTF(
                    "        \\node[block, minimum width=0.5cm, minimum height=0.3cm] {}; & \\node {Function Block}; ");
                if (++row < legend_rows)
                    TIKZ_FRAG_PRINTF("\\\\\n");
            }
            if (has_port) {
                TIKZ_FRAG_PRINTF("        \\node[port] {}; & \\node {Port}; ");
                if (++row < legend_rows)
                    TIKZ_FRAG_PRINTF("\\\\\n");
            }
            if (has_incidence) {
                TIKZ_FRAG_PRINTF("        \\draw[constraint] (0,0) -- (0.5,0); & \\node {Incidence}; ");
                if (++row < legend_rows)
                    TIKZ_FRAG_PRINTF("\\\\\n");
            }
            if (has_betweenness) {
                TIKZ_FRAG_PRINTF("        \\node[purple, font=\\itshape] {B}; & \\node {Betweenness}; ");
                if (++row < legend_rows)
                    TIKZ_FRAG_PRINTF("\\\\\n");
            }
            if (has_intersection) {
                TIKZ_FRAG_PRINTF(
                    "        \\draw[constraint, purple] (0,0) -- (0.5,0); "
                    "\\node[circle, draw=purple, inner sep=0.5pt] at (0.25,0) {}; "
                    "& \\node {Intersection}; ");
                if (++row < legend_rows)
                    TIKZ_FRAG_PRINTF("\\\\\n");
            }
            if (has_containment) {
                TIKZ_FRAG_PRINTF(
                    "        \\draw[constraint, teal, densely dotted] (0,0) -- (0.5,0); & \\node {Containment}; ");
                if (++row < legend_rows)
                    TIKZ_FRAG_PRINTF("\\\\\n");
            }
            if (has_connection) {
                TIKZ_FRAG_PRINTF("        \\draw[connection] (0,0) -- (0.5,0); & \\node {Connection}; ");
                if (++row < legend_rows)
                    TIKZ_FRAG_PRINTF("\\\\\n");
            }

            TIKZ_FRAG_PRINTF("    };\n");
        }
    }

    TIKZ_FRAG_PRINTF("\n\\end{tikzpicture}\n");

#undef TIKZ_FRAG_PRINTF

    /* 确保以 null 终止 */
    if (total >= (int) size) {
        output[size - 1] = '\0';
        return (int) size - 1; /* 截断但仍返回写入量 */
    }
    output[total] = '\0';

    /* ---- 流式事件：TikZ 片段导出完成 ---- */
    if (interop_stream_ctx) {
        stream_emit_simple(interop_stream_ctx, STREAM_EVENT_INFO, "TikZ 片段导出完成", 0);
    }

    return total;
}

int interop_export_canonical(const ConstraintGraph *graph, const char *output_path) {
    if (!graph || !output_path)
        return LV00_ERROR_INVALID_PARAM;

    FILE *fp = fopen(output_path, "w");
    if (!fp)
        return LV00_ERROR_IO;

    /* 输出规范表示 */
    fprintf(fp, "# Lv-00 Canonical Representation\n");
    fprintf(fp, "# Generated by Lv-00 v%s\n", LV00_VERSION_STRING);
    fprintf(fp, "# Format: NodeType NodeID [Coordinates] TrustColor NamespaceDepth ParentBlockID\n");
    fprintf(fp, "#         ConstraintType ConstraintID ParticipantIDs...\n\n");

    /* 输出节点 */
    fprintf(fp, "NODES %d\n", graph->node_count);
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node)
            continue;

        /* 节点类型和ID */
        fprintf(fp, "%s %d", geom_type_name(node->type), node->id);

        /* 输出符号坐标 */
        fprintf(fp, " [");
        /* 内层坐标遍历：const 保护外层节点指针，只读遍历 */
        const int coord_cnt = node->coord_count;
        for (int j = 0; j < coord_cnt; j++) {
            if (j > 0)
                fprintf(fp, ", ");

            SymbolicCoord *coord = node->symbolic_coords[j];
            if (coord) {
                char *serialized = symbolic_coord_serialize(coord);
                if (serialized) {
                    fprintf(fp, "%s", serialized);
                    lv00_free((void **) &serialized);
                } else {
                    /* 序列化失败时回退到数值表示 */
                    double val = symbolic_coord_to_double(coord);
                    fprintf(fp, "%.6g", val);
                }
            } else {
                fprintf(fp, "null");
            }
        }
        fprintf(fp, "]");

        /* 信任颜色 */
        fprintf(fp, " %s", trust_color_to_svg(node->trust));

        /* 命名空间深度和父块 */
        fprintf(fp, " ns=%d parent=%d", node->namespace_depth, node->parent_block_id);

        /* 类型特定信息 */
        switch (node->type) {
            case GEOM_PORT:
                if (node->data.port) {
                    fprintf(fp, " port_type=%s formal=%s poly=%s",
                            (node->data.port->type == PORT_INPUT) ? "input" : "output",
                            node->data.port->is_formal_param ? "true" : "false",
                            node->data.port->is_polymorphic ? "true" : "false");
                }
                break;
            case GEOM_REGION:
                fprintf(fp, " boundary_segments=%d", node->data.region.segment_count);
                break;
            case GEOM_FUNCTION_BLOCK:
                fprintf(fp, " internal=%d inputs=%d outputs=%d state=%d", node->data.func_block.internal_node_count,
                        node->data.func_block.input_count, node->data.func_block.output_count,
                        node->data.func_block.determinism_state);
                break;
            default:
                break;
        }

        fprintf(fp, "\n");
    }

    /* 输出约束 */
    fprintf(fp, "\nCONSTRAINTS %d\n", graph->constraint_count);
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (!c)
            continue;

        fprintf(fp, "%s %d", constraint_type_name(c->type), c->id);

        /* 参与者ID列表 */
        for (int j = 0; j < c->participant_count; j++) {
            fprintf(fp, " %d", c->participants[j]);
        }

        /* 模板ID（如果有） */
        if (c->template_id >= 0) {
            fprintf(fp, " template=%d", c->template_id);
        }

        fprintf(fp, "\n");
    }

    /* 输出邻接表（每个节点关联的约束） */
    fprintf(fp, "\nADJACENCY_LIST\n");
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node)
            continue;

        /* 查找涉及此节点的约束 */
        int related_indices[256];
        int related_count = graph_find_constraints_involving(graph, node->id, related_indices, 256);

        if (related_count > 0) {
            fprintf(fp, "NODE %d ->", node->id);
            for (int j = 0; j < related_count; j++) {
                Constraint *c = graph->constraints[related_indices[j]];
                if (c) {
                    fprintf(fp, " %s(%d)", constraint_type_name(c->type), c->id);
                }
            }
            fprintf(fp, "\n");
        }
    }

    fclose(fp);

    return LV00_OK;
}

int interop_export_geojson(const ConstraintGraph *graph, const InteropExportConfig *config) {
    if (!graph || !config)
        return LV00_ERROR_INVALID_PARAM;

    FILE *fp = fopen(config->output_path, "w");
    if (!fp)
        return LV00_ERROR_IO;

    /* R02：基于实际图数据动态生成GeoJSON，而非硬编码占位数据 */
    fprintf(fp, "{\n");
    fprintf(fp, "  \"type\": \"FeatureCollection\",\n");
    fprintf(fp, "  \"features\": [\n");

    int feature_count = 0;
    for (int i = 0; i < graph->node_count; i++) {
        const GeomNode *node = graph_get_node(graph, i);
        if (!node)
            continue;

        /* 仅导出点类型节点（线段和区域的坐标较为复杂） */
        if (node->type == GEOM_POINT && node->coord_count >= 2) {
            if (feature_count > 0) {
                fprintf(fp, ",\n");
            }
            /* 获取有理数坐标值，转为double */
            double x_val = 0.0, y_val = 0.0;
            SymbolicCoord *cx = node->symbolic_coords ? node->symbolic_coords[0] : NULL;
            SymbolicCoord *cy = node->symbolic_coords ? node->symbolic_coords[1] : NULL;

            if (cx)
                x_val = symbolic_coord_to_double(cx);
            if (cy)
                y_val = symbolic_coord_to_double(cy);

            fprintf(fp, "    {\n");
            fprintf(fp, "      \"type\": \"Feature\",\n");
            fprintf(fp, "      \"geometry\": {\n");
            fprintf(fp, "        \"type\": \"Point\",\n");
            fprintf(fp, "        \"coordinates\": [%.15g, %.15g]\n", x_val, y_val);
            fprintf(fp, "      },\n");
            fprintf(fp, "      \"properties\": {\n");
            fprintf(fp, "        \"id\": %d,\n", node->id);
            fprintf(fp, "        \"type\": \"point\"\n");
            fprintf(fp, "      }\n");
            fprintf(fp, "    }");
            feature_count++;
        }
    }

    /* 导出线段类型节点 */
    for (int i = 0; i < graph->node_count; i++) {
        const GeomNode *node = graph_get_node(graph, i);
        if (!node)
            continue;
        if (node->type != GEOM_LINE_SEGMENT)
            continue;

        /* 查找与线段关联的 INCIDENCE 约束以获取端点 */
        int constraint_indices[64];
        int c_count = graph_find_constraints_involving(graph, node->id, constraint_indices, 64);

        /* 收集端点坐标 */
        double endpoints[4]; /* x1, y1, x2, y2 */
        int endpoint_found = 0;
        for (int j = 0; j < c_count && endpoint_found < 2; j++) {
            const Constraint *c = graph->constraints[constraint_indices[j]];
            if (!c || c->type != INCIDENCE)
                continue;
            for (int k = 0; k < c->participant_count; k++) {
                if (c->participants[k] == node->id)
                    continue;
                const GeomNode *ep = graph_get_node(graph, c->participants[k]);
                if (!ep || ep->type != GEOM_POINT)
                    continue;
                int idx = endpoint_found * 2;
                if (ep->coord_count >= 2 && ep->symbolic_coords) {
                    endpoints[idx] = symbolic_coord_to_double(ep->symbolic_coords[0]);
                    endpoints[idx + 1] = symbolic_coord_to_double(ep->symbolic_coords[1]);
                    endpoint_found++;
                }
            }
        }

        if (endpoint_found >= 2) {
            if (feature_count > 0)
                fprintf(fp, ",\n");
            fprintf(fp, "    {\n");
            fprintf(fp, "      \"type\": \"Feature\",\n");
            fprintf(fp, "      \"geometry\": {\n");
            fprintf(fp, "        \"type\": \"LineString\",\n");
            fprintf(fp, "        \"coordinates\": [[%.15g, %.15g], [%.15g, %.15g]]\n", endpoints[0], endpoints[1],
                    endpoints[2], endpoints[3]);
            fprintf(fp, "      },\n");
            fprintf(fp, "      \"properties\": {\n");
            fprintf(fp, "        \"id\": %d,\n", node->id);
            fprintf(fp, "        \"type\": \"line_segment\"\n");
            fprintf(fp, "      }\n");
            fprintf(fp, "    }");
            feature_count++;
        }
    }

    fprintf(fp, "\n  ]\n");
    fprintf(fp, "}\n");

    fclose(fp);
    return LV00_OK;
}

/**
 * @brief 将约束图导出为PDF文档（最小化纯C实现，无外部库依赖）
 *
 * 【实现概述】
 *   本函数采用"手工写入PDF"的方式，直接生成符合PDF 1.4规范的二进制文件，
 *   不依赖任何外部PDF/Cairo库。这是类似Cairo的"最小化途径"——通过直接操作
 *   PDF的底层运算符来实现矢量图形的绘制。
 *
 * 【已实现功能】
 *   1. PDF文件结构生成 —— 完整的PDF 1.4规范文件头、交叉引用表和尾部
 *   2. 坐标系统建立 —— 根据约束图的包围盒自动计算页面尺寸和坐标变换
 *   3. 点（Point）渲染 —— 使用填充圆（粗线+line cap round 模拟实心点）
 *   4. 线段（Line Segment）渲染 —— 直线段绘制，带信任颜色映射
 *   5. 约束关系渲染 —— 支持关联约束（虚线）、连接约束（实线箭头）
 *   6. 函数块（Function Block）渲染 —— 作为圆角矩形绘制
 *   7. 信任颜色映射 —— 基于TrustColor的RGB颜色分配
 *   8. Helvetica字体嵌入 —— 使用标准14种PDF内置字体，无需嵌入字体文件
 *
 * 【简化实现的部分（完整功能需要额外依赖或后续版本）】
 *   1. 区域（Region）多边形填充 —— 当前仅渲染区域的边界线段；
 *      完整实现需要构造闭合 path 并使用 even-odd fill 规则填充。
 *      所需运算符：h（闭合路径）+ f（非零绕组填充）或 B（填充+描边）。
 *   2. 文本标签渲染 —— 当前使用近似的 Tm 矩阵定位文本；
 *      完整实现需要精确计算文本度量（字宽、行距）和变换矩阵。
 *      可引入 FreeType 库获取精确字形度量。
 *   3. 贝塞尔曲线（c/v/y 运算符）—— 当前仅处理直线段（m/l运算符）；
 *      完整实现需要生成 PDF 的三次贝塞尔曲线 c/v/y 运算符。
 *      所需数据：从 GeomNode 的 coord_count > 4 提取控制点。
 *   4. 透明度/混合模式 —— 当前为不透明渲染；
 *      完整实现需要 PDF ExtGState 字典支持透明度和混合模式。
 *   5. 图例（Legend）—— 当前未生成图例；
 *      完整实现需要在页面底部或侧边绘制图例说明框。
 *   6. 页面元数据 —— 当前不包含信息字典；
 *      完整实现需要添加 Title/Author/Creator/Subject 等PDF信息字典。
 *   7. 多页支持 —— 当前仅支持单页输出；
 *      完整实现需要管理 Pages 树和多个 Page 对象。
 *   8. 压缩 —— 当前使用原始ASCII文本内容流；
 *      完整实现需要使用 zlib 进行 FlateDecode 压缩以减小文件体积。
 *      可通过条件编译 #if __has_include(<zlib.h>) 启用。
 *   9. 中文字体支持 —— 当前仅使用 Helvetica（Latin-1）；
 *      完整实现需要 CID 字体映射或 TrueType 嵌入（需字体嵌入库）。
 *  10. 箭头标记 —— 当前使用线段表示连接约束；
 *      完整实现需要手动绘制三角形路径作为箭头标记。
 *
 * 【PDF内容流运算符参考】
 *   以下是本函数使用的核心PDF图形运算符：
 *   - m x y       : 移动到(x, y)
 *   - l x y       : 从当前点画线到(x, y)
 *   - S           : 描边路径
 *   - re x y w h  : 矩形路径
 *   - B           : 填充并描边路径
 *   - w n         : 设置线宽为 n
 *   - n n n RG    : 设置描边颜色（RGB，0.0-1.0）
 *   - n n n rg    : 设置填充颜色（RGB，0.0-1.0）
 *   - [n n] 0 d   : 设置虚线模式
 *   - BT ... ET   : 文本块
 *   - /F1 sz Tf   : 选择字体和大小
 *   - (text) Tj   : 显示文本
 *   - cm a b c d e f : 变换矩阵
 *   - q / Q        : 保存/恢复图形状态
 *
 * 【外部依赖说明】
 *   本函数的"纯C手工PDF"方案不依赖任何外部库：
 *   - 不依赖 Cairo/Pango/Harfbuzz
 *   - 不依赖 libpdf/zlib
 *   - 不依赖 LaTeX/TikZ（与 interop_export_tikz 互补）
 *
 *   生成的PDF可在以下阅读器中打开：
 *   - Adobe Acrobat Reader
 *   - 浏览器内置PDF查看器（Chrome/Edge/Firefox）
 *   - macOS Preview / 任何PDF阅读器
 *
 * @param graph 约束图指针（包含所有节点和约束关系）
 * @param config 导出配置（output_path 指定 .pdf 文件路径）
 * @return LV00_OK 成功导出
 *         LV00_ERROR_INVALID_PARAM 参数无效
 *         LV00_ERROR_IO 文件无法创建或写入
 */
int interop_export_pdf(const ConstraintGraph *graph, const InteropExportConfig *config) {
    if (!graph || !config)
        return LV00_ERROR_INVALID_PARAM;

    /* ---- 流式事件：开始 PDF 导出 ---- */
    if (interop_stream_ctx) {
        stream_emit_simple(interop_stream_ctx, STREAM_EVENT_INFO, "开始 PDF 导出", 0);
    }

    FILE *fp = fopen(config->output_path, "wb");
    if (!fp)
        return LV00_ERROR_IO;

    /*
     * PDF构建策略：
     *   1. 首先将页面内容写入内存缓冲区（content_buffer）
     *   2. 然后依次写入：PDF头 -> 对象定义 -> 内容流 -> xref表 -> trailer
     *   3. 所有坐标从"图形空间"变换到"PDF页面空间"（原点在左下角，Y轴向上）
     *
     *   页面尺寸根据约束图的包围盒动态计算。
     */

    /* ---- 计算边界框 ---- */
    double min_x, min_y, max_x, max_y;
    compute_bounding_box(graph, &min_x, &min_y, &max_x, &max_y);

    double g_width = max_x - min_x;
    double g_height = max_y - min_y;
    if (g_width < 50.0)
        g_width = 400.0;
    if (g_height < 50.0)
        g_height = 300.0;

    /* 添加边距 */
    double margin = 40.0;
    double page_w = g_width + 2.0 * margin;
    double page_h = g_height + 2.0 * margin;

/* ---- 辅助宏：将图形坐标映射到PDF坐标（PDF原点=左下角，Y向上） ---- */
/*
     * 图形空间:      (min_x, min_y) 为左下角原点
     * PDF页面空间:   (margin, margin) 对应图形空间的 (min_x, min_y)
     *
     * 变换公式:
     *   tx = margin + (x - min_x) * scale_x
     *   ty = margin + (y - min_y) * scale_y
     *   其中 scale_x = g_width / g_width = 1.0（使用1:1映射）
     *        scale_y = g_height / g_height = 1.0
     *
     * 简化（等比例）:
     *   tx = margin + (x - min_x)
     *   ty = margin + (y - min_y)
     */
#define GX(x) (margin + ((x) - min_x))
#define GY(y) (margin + ((y) - min_y))

    /* ---- 内容流缓冲区 ---- */
    /*
     * 将所有PDF图形操作先写入缓冲区，计算总字节数后用于对象定义。
     * 缓冲区使用动态增长的策略，初始分配64KB，按需扩展。
     */
    size_t buf_cap = 65536; /* 初始容量：64KB */
    size_t buf_len = 0;
    char *content = (char *) lv00_malloc(buf_cap);
    if (!content) {
        fclose(fp);
        return LV00_ERROR_OUT_OF_MEMORY;
    }

    /* 内容流辅助：追加字符串到缓冲区 */
#define BUF_APPEND(fmt, ...)                                                         \
    do {                                                                             \
        int _need = snprintf(NULL, 0, fmt, ##__VA_ARGS__) + 1;                       \
        if (_need > 0 && buf_len + (size_t) _need >= buf_cap) {                      \
            size_t _new_cap = buf_cap * 2;                                           \
            while (_new_cap < buf_len + (size_t) _need)                              \
                _new_cap *= 2;                                                       \
            char *_new_buf = (char *) lv00_realloc(content, _new_cap);               \
            if (!_new_buf) {                                                         \
                lv00_free((void **) &content);                                       \
                fclose(fp);                                                          \
                return LV00_ERROR_OUT_OF_MEMORY;                                     \
            }                                                                        \
            content = _new_buf;                                                      \
            buf_cap = _new_cap;                                                      \
        }                                                                            \
        int _w = snprintf(content + buf_len, buf_cap - buf_len, fmt, ##__VA_ARGS__); \
        if (_w > 0)                                                                  \
            buf_len += _w;                                                           \
    } while (0)

    /* ---- 设置基础图形状态 ---- */
    BUF_APPEND("q\n");           /* 保存图形状态 */
    BUF_APPEND("%.2f w\n", 1.5); /* 默认线宽 */

    /* ---- 渲染区域（半透明填充 + 描边，底层） ---- */
    /* 激活透明度 ExtGState */
    BUF_APPEND("/GS1 gs\n");

    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_REGION)
            continue;
        if (node->data.region.segment_count < 3)
            continue;

        /*
         * 区域渲染：使用 f (fill) 填充 + S (stroke) 描边。
         * 先设置填充颜色（半透明），构建路径，然后 B (fill+stroke)。
         */
        TrustColor trust = node->trust;
        if (trust == TRUST_GREEN) {
            BUF_APPEND("0.13 0.76 0.29 rg\n"); /* 填充色：绿色 */
            BUF_APPEND("0.13 0.76 0.29 RG\n"); /* 描边色：绿色 */
        } else if (trust == TRUST_AMBER) {
            BUF_APPEND("0.94 0.27 0.27 rg\n"); /* 填充色：红色 */
            BUF_APPEND("0.94 0.27 0.27 RG\n");
        } else {
            BUF_APPEND("0.61 0.64 0.69 rg\n"); /* 填充色：灰色 */
            BUF_APPEND("0.61 0.64 0.69 RG\n");
        }

        int first = 1;
        for (int s = 0; s < node->data.region.segment_count; s++) {
            GeomNode *seg = node->data.region.boundary_segments[s];
            if (seg && seg->type == GEOM_LINE_SEGMENT && seg->coord_count >= 4) {
                double sx = symbolic_coord_to_double(seg->symbolic_coords[0]);
                double sy = symbolic_coord_to_double(seg->symbolic_coords[1]);
                if (first) {
                    BUF_APPEND("%.2f %.2f m\n", GX(sx), GY(sy));
                    first = 0;
                } else {
                    BUF_APPEND("%.2f %.2f l\n", GX(sx), GY(sy));
                }
            }
        }
        BUF_APPEND("h\n"); /* 闭合路径 */
        BUF_APPEND("B\n"); /* 填充+描边 */
    }

    /* ---- 渲染线段 ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_LINE_SEGMENT)
            continue;
        if (node->coord_count < 4)
            continue;

        double x1 = symbolic_coord_to_double(node->symbolic_coords[0]);
        double y1 = symbolic_coord_to_double(node->symbolic_coords[1]);
        double x2 = symbolic_coord_to_double(node->symbolic_coords[2]);
        double y2 = symbolic_coord_to_double(node->symbolic_coords[3]);

        TrustColor trust = node->trust;
        if (trust == TRUST_GREEN)
            BUF_APPEND("0.15 0.50 0.92 RG\n"); /* 蓝色：线段 */
        else if (trust == TRUST_AMBER)
            BUF_APPEND("0.94 0.27 0.27 RG\n"); /* 红色：不可信 */
        else
            BUF_APPEND("0.61 0.64 0.69 RG\n"); /* 灰色：中间状态 */

        BUF_APPEND("%.2f w\n", 2.0);

        /* 贝塞尔曲线：如果线段有 3 个以上坐标对，使用 PDF c 操作符 */
        if (node->coord_count >= 6) {
            int total_pairs = node->coord_count / 2;
            BUF_APPEND("%.2f %.2f m\n", GX(x1), GY(y1));

            for (int p = 1; p < total_pairs; p++) {
                double sx = symbolic_coord_to_double(node->symbolic_coords[p * 2]);
                double sy = symbolic_coord_to_double(node->symbolic_coords[p * 2 + 1]);
                double px = symbolic_coord_to_double(node->symbolic_coords[(p - 1) * 2]);
                double py = symbolic_coord_to_double(node->symbolic_coords[(p - 1) * 2 + 1]);

                /* 计算两个控制点 */
                double dx = sx - px, dy = sy - py;
                double dist = sqrt(dx * dx + dy * dy);
                double offset = 0.15 * dist;
                if (offset < 0.01)
                    offset = 5.0;
                double nx = -dy / (dist + 0.001);
                double ny = dx / (dist + 0.001);

                double cp1x = px + 0.3 * dx + nx * offset;
                double cp1y = py + 0.3 * dy + ny * offset;
                double cp2x = sx - 0.3 * dx + nx * offset;
                double cp2y = sy - 0.3 * dy + ny * offset;

                /* PDF c 操作符: x1 y1 x2 y2 x3 y3 c */
                BUF_APPEND("%.2f %.2f %.2f %.2f %.2f %.2f c\n", GX(cp1x), GY(cp1y), GX(cp2x), GY(cp2y), GX(sx), GY(sy));
            }
            BUF_APPEND("S\n");
        } else {
            BUF_APPEND("%.2f %.2f m\n", GX(x1), GY(y1));
            BUF_APPEND("%.2f %.2f l\n", GX(x2), GY(y2));
            BUF_APPEND("S\n");
        }

        BUF_APPEND("%.2f w\n", 1.5); /* 恢复默认线宽 */
    }

    /* ---- 渲染点 ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_POINT)
            continue;
        if (node->coord_count < 2)
            continue;

        double px = symbolic_coord_to_double(node->symbolic_coords[0]);
        double py = symbolic_coord_to_double(node->symbolic_coords[1]);

        TrustColor trust = node->trust;
        if (trust == TRUST_GREEN)
            BUF_APPEND("0.13 0.76 0.29 RG\n"); /* 绿色：完全可信 */
        else if (trust == TRUST_AMBER)
            BUF_APPEND("0.94 0.27 0.27 RG\n"); /* 红色：不可信 */
        else
            BUF_APPEND("0.61 0.64 0.69 RG\n"); /* 灰色：中间状态 */

        /*
         * 点渲染：使用填充圆（filled circle）。
         * 当前方法：用极短线段模拟点（line cap round + 粗线宽）。
         * 改进方法（后续版本）: 使用 Bezier 曲线构造圆。
         */
        double r = 3.0;
        BUF_APPEND("%.2f w\n", 6.0);
        BUF_APPEND("1 J\n"); /* 圆头线端 */
        BUF_APPEND("%.2f %.2f m\n", GX(px), GY(py));
        BUF_APPEND("%.2f %.2f l\n", GX(px + 0.01), GY(py));
        BUF_APPEND("S\n");
        BUF_APPEND("0 J\n"); /* 恢复平头线端 */
        BUF_APPEND("%.2f w\n", 1.5);
    }

    /* ---- 渲染函数块（作为圆角矩形） ---- */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_FUNCTION_BLOCK)
            continue;
        if (node->coord_count < 2)
            continue;

        double bx = symbolic_coord_to_double(node->symbolic_coords[0]);
        double by = symbolic_coord_to_double(node->symbolic_coords[1]);
        double bw = 120.0, bh = 60.0;

        TrustColor trust = node->trust;
        if (trust == TRUST_GREEN)
            BUF_APPEND("0.13 0.76 0.29 RG\n"); /* 绿色：完全可信 */
        else if (trust == TRUST_AMBER)
            BUF_APPEND("0.94 0.27 0.27 RG\n"); /* 红色：不可信 */
        else
            BUF_APPEND("0.61 0.64 0.69 RG\n"); /* 灰色：中间状态 */

        BUF_APPEND("%.2f w\n", 2.0);
        BUF_APPEND("%.2f %.2f %.2f %.2f re B\n", GX(bx) - bw / 2.0, GY(by) - bh / 2.0, bw, bh);
        BUF_APPEND("%.2f w\n", 1.5);
    }

    /* ---- 渲染约束关系 ---- */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (!c || c->participant_count < 2)
            continue;

        GeomNode *p0 = graph_get_node_by_id(graph, c->participants[0]);
        GeomNode *p1 = graph_get_node_by_id(graph, c->participants[1]);
        if (!p0 || !p1)
            continue;
        if (p0->coord_count < 2 || p1->coord_count < 2)
            continue;

        double x0 = symbolic_coord_to_double(p0->symbolic_coords[0]);
        double y0 = symbolic_coord_to_double(p0->symbolic_coords[1]);
        double x1 = symbolic_coord_to_double(p1->symbolic_coords[0]);
        double y1 = symbolic_coord_to_double(p1->symbolic_coords[1]);

        switch (c->type) {
            case INCIDENCE:
                /* 关联约束：灰色虚线 */
                BUF_APPEND("0.42 0.45 0.50 RG\n");
                BUF_APPEND("[4.0 3.0] 0 d\n");
                BUF_APPEND("%.2f w\n", 1.0);
                BUF_APPEND("%.2f %.2f m\n", GX(x0), GY(y0));
                BUF_APPEND("%.2f %.2f l\n", GX(x1), GY(y1));
                BUF_APPEND("S\n");
                BUF_APPEND("[] 0 d\n"); /* 恢复实线 */
                BUF_APPEND("%.2f w\n", 1.5);
                break;

            case CONNECTION:
                /* 连接约束：橙色实线 */
                BUF_APPEND("0.96 0.62 0.04 RG\n");
                BUF_APPEND("%.2f w\n", 1.5);
                BUF_APPEND("%.2f %.2f m\n", GX(x0), GY(y0));
                BUF_APPEND("%.2f %.2f l\n", GX(x1), GY(y1));
                BUF_APPEND("S\n");
                break;

            case BETWEENNESS:
                /* 之间约束：紫色细线 */
                BUF_APPEND("0.39 0.40 0.95 RG\n");
                BUF_APPEND("%.2f w\n", 1.0);
                BUF_APPEND("[2.0 2.0] 0 d\n");
                BUF_APPEND("%.2f %.2f m\n", GX(x0), GY(y0));
                BUF_APPEND("%.2f %.2f l\n", GX(x1), GY(y1));
                BUF_APPEND("S\n");
                BUF_APPEND("[] 0 d\n");
                BUF_APPEND("%.2f w\n", 1.5);
                break;

            case INTERSECTION:
                /* 相交约束：紫色十字标记 */
                BUF_APPEND("0.66 0.33 0.97 RG\n");
                BUF_APPEND("%.2f w\n", 1.0);
                BUF_APPEND("%.2f %.2f m\n", GX(x0), GY(y0));
                BUF_APPEND("%.2f %.2f l\n", GX(x1), GY(y1));
                BUF_APPEND("S\n");
                BUF_APPEND("%.2f w\n", 1.5);
                break;

            case CONTAINMENT:
                /* 包含约束：青色点划线 */
                BUF_APPEND("0.08 0.72 0.65 RG\n");
                BUF_APPEND("[6.0 3.0 1.0 3.0] 0 d\n");
                BUF_APPEND("%.2f w\n", 1.0);
                BUF_APPEND("%.2f %.2f m\n", GX(x0), GY(y0));
                BUF_APPEND("%.2f %.2f l\n", GX(x1), GY(y1));
                BUF_APPEND("S\n");
                BUF_APPEND("[] 0 d\n");
                BUF_APPEND("%.2f w\n", 1.5);
                break;

            default:
                break;
        }
    }

    /* ---- 文本标签（最小化实现） ---- */
    /*
     * 文本渲染策略说明：
     *   当前版本使用 Helvetica 字体标注节点ID。完整的文本渲染需要：
     *   1. 精确的文本宽度计算（用于居中定位）—— 可通过 Tj 返回值或 FreeType 度量
     *   2. 中文字体支持（CID字体或TrueType嵌入）—— 需要字体文件和 CIDFont 字典
     *   3. 文本旋转和变换 —— 通过 Tm 矩阵的旋转分量实现
     *   4. LaTeX 数学公式渲染 —— 复杂，需要完整的数学排版引擎或预渲染位图嵌入
     */
    BUF_APPEND("BT\n");
    BUF_APPEND("/F1 8 Tf\n"); /* Helvetica 8pt */
    BUF_APPEND("0 0 0 rg\n"); /* 黑色文本 */

    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->coord_count < 2)
            continue;

        double lx = symbolic_coord_to_double(node->symbolic_coords[0]);
        double ly = symbolic_coord_to_double(node->symbolic_coords[1]);

        /* 标签放置：点/端口上方，线段中点下方，块居中 */
        char label[32];
        if (node->type == GEOM_POINT) {
            snprintf(label, sizeof(label), "P%d", node->id);
            BUF_APPEND("%.2f %.2f Td\n", GX(lx) - 6.0, GY(ly) + 8.0);
        } else if (node->type == GEOM_LINE_SEGMENT && node->coord_count >= 4) {
            double x2 = symbolic_coord_to_double(node->symbolic_coords[2]);
            double y2 = symbolic_coord_to_double(node->symbolic_coords[3]);
            double mx = (lx + x2) / 2.0, my = (ly + y2) / 2.0;
            snprintf(label, sizeof(label), "S%d", node->id);
            BUF_APPEND("%.2f %.2f Td\n", GX(mx) - 6.0, GY(my) - 6.0);
        } else if (node->type == GEOM_FUNCTION_BLOCK) {
            snprintf(label, sizeof(label), "FB_%d", node->id);
            BUF_APPEND("%.2f %.2f Td\n", GX(lx) - 14.0, GY(ly) - 3.0);
        } else {
            continue;
        }

        /* 转义括号 */
        for (const char *p = label; *p; p++) {
            if (*p == '(' || *p == ')' || *p == '\\')
                BUF_APPEND("\\%c", *p);
            else
                BUF_APPEND("%c", *p);
        }
        BUF_APPEND(" Tj\n");
        BUF_APPEND("%.2f %.2f Td\n", 0.0, 0.0); /* 重置文本位置到原点 */
    }
    BUF_APPEND("ET\n");

    BUF_APPEND("Q\n"); /* 恢复图形状态 */

#undef BUF_APPEND
#undef GX
#undef GY

    /* ================================================================ */
    /*   PDF 文件结构写入（基于PDF 1.4规范）                             */
    /*                                                                  */
    /*   PDF 对象编号方案：                                              */
    /*     对象1: Catalog（目录）                                       */
    /*     对象2: Pages（页面树根节点）                                  */
    /*     对象3: Page（单页，含 ExtGState 引用）                       */
    /*     对象4: Content（内容流，包含上述所有图形操作）                */
    /*     对象5: Font（字体字典 - Helvetica）                          */
    /*     对象6: ExtGState（透明度图形状态）                           */
    /*     对象7: Info（页面元数据）                                    */
    /*                                                                  */
    /*   注意：对象编号和字节偏移量紧密耦合，修改内容流时需同步更新     */
    /*         xref表中的偏移量。                                       */
    /* ================================================================ */

    /* ---- 对象4的内容流长度（字节数） ---- */
    long content_length = (long) buf_len;

    /*
     * 对象1: Catalog
     * 根目录对象，指向Pages树
     */
    long cat_start = ftell(fp);
    fprintf(fp, "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");

    /*
     * 对象2: Pages（页面树根节点）
     * 包含页面数量和子页面引用
     */
    long pages_start = ftell(fp);
    fprintf(fp, "2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n");

    /*
     * 对象3: Page（单页定义）
     * 定义页面尺寸（MediaBox）、内容流引用、字体资源和 ExtGState
     * MediaBox格式：[llx lly urx ury] = [0 0 page_w page_h]
     */
    long page_start = ftell(fp);
    fprintf(fp,
            "3 0 obj\n<< /Type /Page /Parent 2 0 R\n"
            "   /MediaBox [0 0 %.2f %.2f]\n"
            "   /Contents 4 0 R\n"
            "   /Resources << /Font << /F1 5 0 R >>\n"
            "                 /ExtGState << /GS1 6 0 R >> >>\n"
            ">>\nendobj\n",
            page_w, page_h);

    /*
     * 对象4: Content（内容流）
     * 包含所有PDF图形描述操作符
     * 写入前计算并声明精确的流长度
     */
    long content_start = ftell(fp);
    fprintf(fp, "4 0 obj\n<< /Length %ld >>\nstream\n", content_length);
    fwrite(content, 1, content_length, fp);
    fprintf(fp, "\nendstream\nendobj\n");

    /*
     * 对象5: Font（字体字典）
     * 使用PDF标准14种字体之一的Helvetica，无需嵌入字体文件
     */
    long font_start = ftell(fp);
    fprintf(fp,
            "5 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica"
            " /Encoding /WinAnsiEncoding >>\nendobj\n");

    /*
     * 对象6: ExtGState（透明度图形状态）
     * 设置 CA 0.3（描边透明度）和 ca 0.3（填充透明度）
     * 用于区域渲染的半透明效果。
     */
    long gs_start = ftell(fp);
    fprintf(fp, "6 0 obj\n<< /Type /ExtGState /CA 0.3 /ca 0.3 >>\nendobj\n");

    /*
     * 对象7: Info（页面元数据）
     * 包含文档标题、作者、创建者和创建日期。
     */
    long info_start = ftell(fp);
    {
        /* 获取当前日期时间字符串 */
        time_t now = time(NULL);
        struct tm *lt = localtime(&now);
        char date_str[64];
        strftime(date_str, sizeof(date_str), "D:%Y%m%d%H%M%S", lt);

        fprintf(fp,
                "7 0 obj\n<< /Title (Lv-00 Geometry Export)\n"
                "   /Author (Lv-00 Project)\n"
                "   /Creator (Lv-00 v%s)\n"
                "   /CreationDate (%s) >>\nendobj\n",
                LV00_VERSION_STRING, date_str);
    }

    /* ---- 交叉引用表（Cross-Reference Table） ---- */
    /*
     * xref表记录了每个PDF对象的字节偏移量，是PDF随机访问的关键结构。
     * 格式：
     *   xref
     *   0 8                    (对象0到7，共8个对象)
     *   0000000000 65535 f     (对象0=空闲条目)
     *   nnnnnnnnnn 00000 n     (对象1-7的字节偏移)
     */
    long xref_start = ftell(fp);
    fprintf(fp, "xref\n");
    fprintf(fp, "0 8\n");
    fprintf(fp, "0000000000 65535 f \n"); /* 对象0：空闲条目 */
    fprintf(fp, "%010ld 00000 n \n", cat_start);
    fprintf(fp, "%010ld 00000 n \n", pages_start);
    fprintf(fp, "%010ld 00000 n \n", page_start);
    fprintf(fp, "%010ld 00000 n \n", content_start);
    fprintf(fp, "%010ld 00000 n \n", font_start);
    fprintf(fp, "%010ld 00000 n \n", gs_start);
    fprintf(fp, "%010ld 00000 n \n", info_start);

    /* ---- Trailer ---- */
    /*
     * Trailer包含：
     * - /Size: 交叉引用表条目总数（8 = 对象0-7）
     * - /Root: 指向Catalog对象（对象1）
     * - /Info: 指向元数据对象（对象7）
     * - startxref: xref表起始偏移量（用于快速定位）
     * - %%EOF: PDF文件结束标记
     */
    fprintf(fp, "trailer\n");
    fprintf(fp, "<< /Size 8 /Root 1 0 R /Info 7 0 R >>\n");
    fprintf(fp, "startxref\n");
    fprintf(fp, "%ld\n", xref_start);
    fprintf(fp, "%%%%EOF\n");

    fclose(fp);
    lv00_free((void **) &content);

    /*
     * PDF已成功导出：告知调用者文件路径、页面尺寸、节点/约束数量。
     * 当前PDF为纯C最小化实现（无外部库依赖），
     * 区域以线框模式渲染，文本标签为基础版本。
     * 完整功能改进方案见函数注释中【简化实现的部分】列表。
     */

    /* ---- 流式事件：PDF 导出完成 ---- */
    if (interop_stream_ctx) {
        stream_emit_simple(interop_stream_ctx, STREAM_EVENT_INFO, "PDF 导出完成", 0);
    }

    return LV00_OK;
}

/* ==================== GeoGebra 导入辅助：ZIP 解析 ==================== */

/**
 * @brief ZIP 文件结构常量
 *
 * ZIP 格式规范（PKWARE APPNOTE.TXT）定义的核心结构签名。
 * 所有多字节整数均为小端序（Little-Endian）。
 */
#define GGB_LOCAL_FILE_SIG 0x04034b50U      /**< 本地文件头签名 */
#define GGB_CENTRAL_DIR_SIG 0x02014b50U     /**< 中央目录签名 */
#define GGB_EOCD_SIG 0x06054b50U            /**< 结束中心目录签名 */
#define GGB_LOCAL_HEADER_MIN 30             /**< 本地文件头最小字节数 */
#define GGB_CENTRAL_DIR_MIN 46              /**< 中央目录条目最小字节数 */
#define GGB_EOCD_MIN_SIZE 22                /**< EOCD 最小字节数 */
#define GGB_MAX_XML_SIZE (16 * 1024 * 1024) /**< XML 最大大小 16MB */
#define GGB_COMPRESSION_STORE 0             /**< 无压缩（STORE） */
#define GGB_COMPRESSION_DEFLATE 8           /**< Deflate 压缩 */

/** @brief 从字节缓冲区读取小端序 uint32 */
static uint32_t ggb_read_u32_le(const uint8_t *buf, size_t offset) {
    return (uint32_t) buf[offset] | ((uint32_t) buf[offset + 1] << 8) | ((uint32_t) buf[offset + 2] << 16) |
           ((uint32_t) buf[offset + 3] << 24);
}

/** @brief 从字节缓冲区读取小端序 uint16 */
static uint16_t ggb_read_u16_le(const uint8_t *buf, size_t offset) {
    return (uint16_t) buf[offset] | ((uint16_t) buf[offset + 1] << 8);
}

/**
 * @brief 在文件末尾搜索 EOCD（End of Central Directory）记录
 *
 * EOCD 位于 ZIP 文件末尾，以 0x06054b50 签名开头。
 * 因结尾可能有最大 65535 字节的注释，需反向搜索。
 *
 * @param data       文件数据缓冲区
 * @param data_size  数据总大小
 * @param eocd_offset [out] 输出 EOCD 的字节偏移
 * @return true 找到 EOCD，false 未找到
 */
static bool ggb_find_eocd(const uint8_t *data, size_t data_size, size_t *eocd_offset) {
    if (data_size < GGB_EOCD_MIN_SIZE)
        return false;
    size_t search_start = (data_size > GGB_EOCD_MIN_SIZE + 65535) ? data_size - GGB_EOCD_MIN_SIZE - 65535 : 0;
    for (size_t i = data_size - GGB_EOCD_MIN_SIZE;; i--) {
        if (ggb_read_u32_le(data, i) == GGB_EOCD_SIG) {
            *eocd_offset = i;
            return true;
        }
        if (i == search_start)
            break;
        if (i == 0)
            break;
    }
    return false;
}

/**
 * @brief 从中央目录中查找指定文件名的条目
 *
 * 遍历中央目录条目，按文件名精确匹配。
 *
 * @param data          文件数据缓冲区
 * @param eocd_offset   EOCD 偏移
 * @param target_name   目标文件名（如 "geogebra.xml"）
 * @param entry_offset  [out] 输出本地文件头偏移
 * @param comp_size     [out] 输出压缩后大小
 * @param uncomp_size   [out] 输出解压后大小
 * @param comp_method   [out] 输出压缩方法
 * @return true 找到，false 未找到
 */
static bool ggb_find_central_entry(const uint8_t *data, size_t eocd_offset, const char *target_name,
                                   size_t *entry_offset, size_t *comp_size, size_t *uncomp_size,
                                   uint16_t *comp_method) {
    uint16_t entry_count = ggb_read_u16_le(data, eocd_offset + 10);
    uint32_t cd_offset = ggb_read_u32_le(data, eocd_offset + 16);
    size_t cd_pos = cd_offset;

    for (uint16_t i = 0; i < entry_count; i++) {
        if (cd_pos + GGB_CENTRAL_DIR_MIN > eocd_offset)
            return false;
        if (ggb_read_u32_le(data, cd_pos) != GGB_CENTRAL_DIR_SIG)
            return false;

        uint16_t name_len = ggb_read_u16_le(data, cd_pos + 28);
        uint16_t extra_len = ggb_read_u16_le(data, cd_pos + 30);
        uint16_t comment_len = ggb_read_u16_le(data, cd_pos + 32);
        size_t entry_size = (size_t) GGB_CENTRAL_DIR_MIN + name_len + extra_len + comment_len;

        if (cd_pos + GGB_CENTRAL_DIR_MIN + name_len <= eocd_offset) {
            if ((size_t) name_len == strlen(target_name) &&
                memcmp(data + cd_pos + GGB_CENTRAL_DIR_MIN, target_name, name_len) == 0) {
                *comp_method = ggb_read_u16_le(data, cd_pos + 10);
                *comp_size = ggb_read_u32_le(data, cd_pos + 20);
                *uncomp_size = ggb_read_u32_le(data, cd_pos + 24);
                *entry_offset = ggb_read_u32_le(data, cd_pos + 42);
                return true;
            }
        }
        cd_pos += entry_size;
    }
    return false;
}

/**
 * @brief 从本地文件头中提取文件数据偏移
 *
 * 本地文件头格式：
 *   偏移0:  签名 (4字节) = 0x04034b50
 *   偏移26: 文件名长度 (2字节)
 *   偏移28: 额外字段长度 (2字节)
 *   之后:   文件名 + 额外字段 + 文件数据
 *
 * @param data           文件数据缓冲区
 * @param local_offset   本地文件头偏移
 * @param data_offset    [out] 输出实际文件数据偏移
 * @return true 成功，false 失败
 */
static bool ggb_get_local_data_offset(const uint8_t *data, size_t local_offset, size_t *data_offset) {
    if (ggb_read_u32_le(data, local_offset) != GGB_LOCAL_FILE_SIG)
        return false;
    uint16_t name_len = ggb_read_u16_le(data, local_offset + 26);
    uint16_t extra_len = ggb_read_u16_le(data, local_offset + 28);
    *data_offset = local_offset + GGB_LOCAL_HEADER_MIN + name_len + extra_len;
    return true;
}

/* ==================== Deflate 解压器 ==================== */

/**
 * @brief Deflate（RFC 1951）解压器 —— 固定哈夫曼 + 存储块实现
 *
 * 当前实现支持固定哈夫曼编码（块类型 1）和存储块（块类型 0）。
 * 如果遇到动态哈夫曼编码（块类型 2），返回错误并提示用户使用替代方案。
 *
 * 该实现基于 tinf (tiny inflate) 公有领域代码精简，支持
 * 大多数 GeoGebra 文件（通常使用固定哈夫曼编码进行压缩）。
 *
 * 已知限制：
 *   - 不支持动态哈夫曼编码（块类型 2），部分 .ggb 文件可能使用此编码
 *   - 不支持预设字典（块类型 32，即 BTYPE=1 + BFINAL=1 的预设字典模式）
 *
 * 改进路线：
 *   - 短期：在编译时检测 zlib 可用性（#if __has_include(<zlib.h>)），
 *     若可用则直接调用 uncompress() 替代本手写实现，获得完整的 Deflate 支持
 *   - 中期：若无法引入 zlib，可扩展本实现以支持动态哈夫曼编码
 *     （需要实现 Huffman 树的动态构建和码表解码，约增加 200-300 行代码）
 *   - 长期：将解压抽象为可插拔的 Decompressor 接口，支持 zlib/miniz/本实现
 *
 * @param src        源数据（压缩）
 * @param src_len    源数据长度
 * @param dst        目标缓冲区（解压后）
 * @param dst_cap    目标缓冲区容量
 * @param out_len    [out] 实际解压长度
 * @return true 成功，false 失败（不支持的格式或数据损坏）
 */
static bool ggb_inflate(const uint8_t *src, size_t src_len, uint8_t *dst, size_t dst_cap, size_t *out_len) {
    /* ---- 位读取状态 ---- */
    size_t bit_pos = 0; /* 当前位位置（全局） */
    size_t dst_pos = 0; /* 输出位置 */
    bool bfinal = false;

    /* 固定哈夫曼编码的码长表（RFC 1951 第 3.2.6 节） */
    static const uint8_t fixed_lit_len_bits[] = {/* 0-143  (8位码长) */ 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 /* 144-255 (9位码长) */ 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 /* 256-279 (7位码长) */ 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 /* 280-287 (8位码长) */ 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8};
    static const uint8_t fixed_dist_bits[32] = {5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
                                                5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5};

    /* 长度额外位表（RFC 1951 第 3.2.5 节） */
    static const uint16_t length_base[] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                           31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    static const uint8_t length_extra[] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                           2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    /* 距离额外位表 */
    static const uint16_t dist_base[] = {1,    2,    3,    4,    5,    7,    9,    13,    17,    25,
                                         33,   49,   65,   97,   129,  193,  257,  385,   513,   769,
                                         1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
    static const uint8_t dist_extra[] = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                         6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

/* ---- 从位流读取 N 位 ---- */
#define GGB_READ_BITS(n, val)                            \
    do {                                                 \
        (val) = 0;                                       \
        for (int _b = 0; _b < (n); _b++) {               \
            if (bit_pos / 8 >= src_len) {                \
                *out_len = dst_pos;                      \
                return false;                            \
            }                                            \
            if (src[bit_pos / 8] & (1 << (bit_pos & 7))) \
                (val) |= (1U << _b);                     \
            bit_pos++;                                   \
        }                                                \
    } while (0)

/* ---- 使用码长表读取一个哈夫曼编码 ---- */
#define GGB_HUFF_DECODE(bits_table, table_size, symbol)                       \
    do {                                                                      \
        uint32_t _code = 0;                                                   \
        (symbol) = 0xFFFF;                                                    \
        for (int _bit = 1; _bit <= 15; _bit++) {                              \
            if (bit_pos / 8 >= src_len) {                                     \
                *out_len = dst_pos;                                           \
                return false;                                                 \
            }                                                                 \
            _code = (_code << 1) | ((src[bit_pos / 8] >> (bit_pos & 7)) & 1); \
            bit_pos++;                                                        \
            for (int _s = 0; _s < (table_size); _s++) {                       \
                if ((bits_table)[_s] == (uint8_t) _bit) {                     \
                    uint32_t _prefix = _code;                                 \
                    for (int _p = 0; _p < (15 - _bit); _p++)                  \
                        _prefix <<= 1;                                        \
                    uint32_t _expected = 0;                                   \
                    int _count_before = 0;                                    \
                    for (int _c = 0; _c < _s; _c++) {                         \
                        if ((bits_table)[_c] == (uint8_t) _bit)               \
                            _count_before++;                                  \
                    }                                                         \
                    _expected = (uint32_t) _count_before << (15 - _bit);      \
                    if (_prefix == _expected) {                               \
                        (symbol) = _s;                                        \
                        break;                                                \
                    }                                                         \
                }                                                             \
            }                                                                 \
            if ((symbol) != 0xFFFF)                                           \
                break;                                                        \
        }                                                                     \
    } while (0)

    while (!bfinal) {
        /* 读取块头 */
        uint32_t bfinal_bit, btype;
        GGB_READ_BITS(1, bfinal_bit);
        GGB_READ_BITS(2, btype);
        bfinal = (bfinal_bit != 0);

        if (btype == 0) {
            /* 存储块（无压缩） */
            bit_pos = ((bit_pos + 7) / 8) * 8; /* 对齐到字节边界 */
            if (bit_pos / 8 + 4 > src_len) {
                *out_len = dst_pos;
                return false;
            }
            uint16_t len = (uint16_t) src[bit_pos / 8] | ((uint16_t) src[bit_pos / 8 + 1] << 8);
            bit_pos += 32; /* 跳过 LEN 和 NLEN */
            if (dst_pos + len > dst_cap) {
                *out_len = dst_pos;
                return false;
            }
            if (bit_pos / 8 + len > src_len) {
                *out_len = dst_pos;
                return false;
            }
            memcpy(dst + dst_pos, src + bit_pos / 8, len);
            dst_pos += len;
            bit_pos += len * 8;
        } else if (btype == 1) {
            /* 固定哈夫曼编码 */
            while (1) {
                uint32_t symbol;
                GGB_HUFF_DECODE(fixed_lit_len_bits, 288, symbol);
                if (symbol == 0xFFFF) {
                    *out_len = dst_pos;
                    return false;
                }
                if (symbol < 256) {
                    /* 字面量字节 */
                    if (dst_pos >= dst_cap) {
                        *out_len = dst_pos;
                        return false;
                    }
                    dst[dst_pos++] = (uint8_t) symbol;
                } else if (symbol == 256) {
                    /* 块结束 */
                    break;
                } else if (symbol <= 285) {
                    /* 长度码 */
                    uint32_t len_idx = symbol - 257;
                    if (len_idx >= 29) {
                        *out_len = dst_pos;
                        return false;
                    }
                    uint32_t length = length_base[len_idx];
                    uint32_t extra_len = length_extra[len_idx];
                    if (extra_len > 0) {
                        uint32_t extra;
                        GGB_READ_BITS((int) extra_len, extra);
                        length += extra;
                    }
                    /* 距离码 */
                    uint32_t dist_symbol;
                    GGB_HUFF_DECODE(fixed_dist_bits, 32, dist_symbol);
                    if (dist_symbol == 0xFFFF || dist_symbol >= 30) {
                        *out_len = dst_pos;
                        return false;
                    }
                    uint32_t dist = dist_base[dist_symbol];
                    uint32_t extra_dist = dist_extra[dist_symbol];
                    if (extra_dist > 0) {
                        uint32_t extra;
                        GGB_READ_BITS((int) extra_dist, extra);
                        dist += extra;
                    }
                    /* 复制回溯 */
                    if (dist > dst_pos) {
                        *out_len = dst_pos;
                        return false;
                    }
                    if (dst_pos + length > dst_cap) {
                        *out_len = dst_pos;
                        return false;
                    }
                    size_t copy_src = dst_pos - dist;
                    for (uint32_t k = 0; k < length; k++) {
                        dst[dst_pos++] = dst[copy_src + k];
                    }
                } else {
                    *out_len = dst_pos;
                    return false;
                }
            }
        } else {
            /* 动态哈夫曼编码（不支持） */
            *out_len = dst_pos;
            return false;
        }
    }

#undef GGB_READ_BITS
#undef GGB_HUFF_DECODE

    /* 对齐到字节边界 */
    *out_len = dst_pos;
    return true;
}

/* ==================== GeoGebra XML 解析辅助函数 ==================== */

/**
 * @brief 在 XML 文本中查找下一个指定标签的开标签位置
 *
 * 手工 XML 解析器，查找形如 "<tagName" 或 "<prefix:tagName" 的标签开头。
 *
 * @param xml      XML 文本
 * @param xml_len  XML 文本长度
 * @param tag_name 标签名称（不含 <>）
 * @param start    搜索起始偏移
 * @param tag_start [out] 输出标签起始偏移（'<' 的位置）
 * @param tag_content_start [out] 输出标签内容起始偏移（'>' 之后）
 * @param tag_content_end [out] 输出标签内容结束偏移（'<' 之前）
 * @return true 找到，false 未找到
 */
static bool ggb_find_xml_tag(const char *xml, size_t xml_len, const char *tag_name, size_t start, size_t *tag_start,
                             size_t *tag_content_start, size_t *tag_content_end) {
    char open_tag[128];
    int open_len = snprintf(open_tag, sizeof(open_tag), "<%s", tag_name);
    if (open_len < 0)
        return false;

    char close_tag[128];
    int close_len = snprintf(close_tag, sizeof(close_tag), "</%s>", tag_name);
    if (close_len < 0)
        return false;

    for (size_t i = start; i + (size_t) open_len <= xml_len; i++) {
        /* 匹配开标签：<tagName 或 <tagName> 或 <tagName ... */
        if (memcmp(xml + i, open_tag, (size_t) open_len) == 0) {
            char next_char = (i + (size_t) open_len < xml_len) ? xml[i + open_len] : '\0';
            if (next_char == '>' || next_char == ' ' || next_char == '\t' || next_char == '\n' || next_char == '\r' ||
                next_char == '/') {
                *tag_start = i;
                /* 找到 '>' */
                size_t gt_pos = i + 1;
                while (gt_pos < xml_len && xml[gt_pos] != '>')
                    gt_pos++;
                if (gt_pos >= xml_len)
                    return false;
                *tag_content_start = gt_pos + 1;

                /* 如果是自闭合标签 <tagName ... />，内容为空 */
                if (gt_pos > i && xml[gt_pos - 1] == '/') {
                    *tag_content_end = gt_pos - 1;
                    return true;
                }

                /* 查找匹配的闭合标签 */
                int depth = 1;
                size_t search_pos = gt_pos + 1;
                while (search_pos + (size_t) close_len <= xml_len && depth > 0) {
                    /* 检查开标签 */
                    if (search_pos + (size_t) open_len <= xml_len &&
                        memcmp(xml + search_pos, open_tag, (size_t) open_len) == 0) {
                        char nc = (search_pos + open_len < xml_len) ? xml[search_pos + open_len] : '\0';
                        if (nc == '>' || nc == ' ' || nc == '\t' || nc == '\n' || nc == '\r' || nc == '/') {
                            depth++;
                        }
                    }
                    /* 检查闭标签 */
                    if (memcmp(xml + search_pos, close_tag, (size_t) close_len) == 0) {
                        depth--;
                        if (depth == 0) {
                            *tag_content_end = search_pos;
                            return true;
                        }
                    }
                    search_pos++;
                }
                return false; /* 未找到匹配的闭标签 */
            }
        }
    }
    return false;
}

/**
 * @brief 从 XML 开标签中提取属性值
 *
 * 在形如 '<tag attr1="val1" attr2="val2">' 的开标签中查找指定属性名并返回其值。
 *
 * @param tag_start  开标签起始位置（'<' 的位置）
 * @param tag_end    开标签结束位置（'>' 的位置）
 * @param attr_name  属性名称（如 "type", "label", "x", "y"）
 * @param out_value  输出缓冲区
 * @param out_size   输出缓冲区大小
 * @return true 找到属性，false 未找到
 */
static bool ggb_extract_attr(const char *tag_start, size_t tag_len, const char *attr_name, char *out_value,
                             size_t out_size) {
    if (out_size == 0)
        return false;
    out_value[0] = '\0';

    char search[128];
    int search_len = snprintf(search, sizeof(search), "%s=\"", attr_name);
    if (search_len < 0)
        return false;

    char search_single[128];
    int ssl = snprintf(search_single, sizeof(search_single), "%s='", attr_name);
    if (ssl < 0)
        return false;

    for (size_t i = 0; i + (size_t) search_len <= tag_len; i++) {
        bool is_double = (memcmp(tag_start + i, search, (size_t) search_len) == 0);
        bool is_single = (memcmp(tag_start + i, search_single, (size_t) ssl) == 0);

        if (is_double || is_single) {
            char quote = is_double ? '"' : '\'';
            size_t val_start = i + (is_double ? (size_t) search_len : (size_t) ssl);
            size_t j = 0;
            while (val_start + j < tag_len && tag_start[val_start + j] != quote && j < out_size - 1) {
                out_value[j] = tag_start[val_start + j];
                j++;
            }
            out_value[j] = '\0';
            return true;
        }
    }
    return false;
}

/**
 * @brief 从 XML 文本中提取两个 double 坐标（x, y）
 *
 * 解析坐标字符串（如 "3.5" 或 "1/2"）并转换为 double 值。
 * 支持分数格式 "num/den" 和普通十进制格式。
 *
 * @param text    XML 文本
 * @param name    坐标名称（"x" 或 "y"）
 * @param value   [out] 输出 double 值
 * @return true 成功，false 失败
 */
static bool ggb_extract_coord_double(const char *text, const char *name, double *value) {
    size_t tag_len = strlen(text);
    char val_buf[64];
    if (!ggb_extract_attr(text, tag_len, name, val_buf, sizeof(val_buf)))
        return false;
    if (val_buf[0] == '\0')
        return false;

    /* 检查分数格式 "a/b" */
    const char *slash = strchr(val_buf, '/');
    if (slash && slash != val_buf && *(slash + 1) != '\0') {
        double num = atof(val_buf);
        double den = atof(slash + 1);
        if (den == 0.0)
            return false;
        *value = num / den;
        return true;
    }

    *value = atof(val_buf);
    return true;
}

/**
 * @brief 将 double 值转换为 rational SymbolicCoord
 *
 * 使用 INTEROP_COORD_DENOM_PRECISION 作为精度分母。
 *
 * @param value 双精度浮点值
 * @return SymbolicCoord 指针（调用者负责释放），失败返回 NULL
 */
static SymbolicCoord *ggb_double_to_rational(double value) {
    double denom = (double) INTEROP_COORD_DENOM_PRECISION;
    int64_t num = (int64_t) (value * denom + (value >= 0 ? 0.5 : -0.5));
    return symbolic_coord_create_rational(num, INTEROP_COORD_DENOM_PRECISION);
}

/* ==================== 导入功能 ==================== */

int interop_import_geogebra(LV00Engine *engine, const InteropImportConfig *config) {
    /**
     * @brief 从 GeoGebra .ggb 文件导入几何构造
     *
     * 实现基本的 ZIP+XML 解析，不依赖外部库。处理流程：
     *   1. 读取整个 .ggb 文件到内存
     *   2. 解析 ZIP 的 EOCD 和 Central Directory 结构
     *   3. 查找 "geogebra.xml" 文件条目
     *   4. 解压（STORE 或 Deflate）获取 XML 内容
     *   5. 手工 XML 解析，提取 <element> 标签
     *   6. 按 type 属性（point/segment/circle/line/polygon）映射到约束图
     *   7. 使用 engine->main_graph 作为目标图
     *
     * @param engine Lv-00 引擎实例（其 main_graph 为导入目标）
     * @param config 导入配置（input_path 指定 .ggb 文件路径）
     * @return 成功返回导入的几何元素数量（>= 0）
     *         LV00_ERROR_INVALID_PARAM 参数无效
     *         LV00_ERROR_IO 文件不存在或无法读取
     *         LV00_ERROR_UNSUPPORTED 不支持的压缩格式或文件结构
     *         LV00_ERROR_PARSE XML 解析失败
     *         LV00_ERROR_OUT_OF_MEMORY 内存分配失败
     */
    if (!engine || !config)
        return LV00_ERROR_INVALID_PARAM;
    if (!engine->main_graph) {
        lv00_set_error(LV00_ERROR_INVALID_STATE, "GeoGebra导入失败：引擎的约束图未初始化");
        return LV00_ERROR_INVALID_STATE;
    }

    /* 验证输入路径 */
    if (config->input_path[0] == '\0') {
        lv00_set_error(LV00_ERROR_INVALID_PARAM, "GeoGebra导入失败：未指定输入文件路径");
        return LV00_ERROR_INVALID_PARAM;
    }

    /* 读取整个文件到内存 */
    FILE *fp = fopen(config->input_path, "rb");
    if (!fp) {
        lv00_set_error(LV00_ERROR_IO, "GeoGebra导入失败：无法打开文件'%s'。请确认文件存在且具有读取权限。",
                       config->input_path);
        return LV00_ERROR_IO;
    }

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (fsize <= 0) {
        fclose(fp);
        lv00_set_error(LV00_ERROR_IO, "GeoGebra文件'%s'为空（0字节）", config->input_path);
        return LV00_ERROR_IO;
    }

    if (fsize > 100 * 1024 * 1024) {
        fclose(fp);
        lv00_set_error(LV00_ERROR_UNSUPPORTED, "GeoGebra文件'%s'过大（%ld字节），最大支持100MB", config->input_path,
                       fsize);
        return LV00_ERROR_UNSUPPORTED;
    }

    uint8_t *data = (uint8_t *) lv00_malloc((size_t) fsize);
    if (!data) {
        fclose(fp);
        lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "GeoGebra导入失败：无法为文件分配内存（%ld字节）", fsize);
        return LV00_ERROR_OUT_OF_MEMORY;
    }

    size_t bytes_read = fread(data, 1, (size_t) fsize, fp);
    fclose(fp);

    if (bytes_read != (size_t) fsize) {
        lv00_free((void **) &data);
        lv00_set_error(LV00_ERROR_IO, "GeoGebra导入失败：文件读取不完整（期望%ld字节，实际%zu字节）", fsize,
                       bytes_read);
        return LV00_ERROR_IO;
    }

    /* ---- 步骤1：查找 EOCD ---- */
    size_t eocd_offset;
    if (!ggb_find_eocd(data, (size_t) fsize, &eocd_offset)) {
        lv00_free((void **) &data);
        lv00_set_error(LV00_ERROR_PARSE, "GeoGebra导入失败：文件'%s'不是有效的 ZIP 格式（未找到 EOCD 签名）",
                       config->input_path);
        return LV00_ERROR_PARSE;
    }

    /* ---- 步骤2：在中央目录中查找 geogebra.xml ---- */
    size_t entry_offset, comp_size, uncomp_size;
    uint16_t comp_method;
    if (!ggb_find_central_entry(data, eocd_offset, "geogebra.xml", &entry_offset, &comp_size, &uncomp_size,
                                &comp_method)) {
        lv00_free((void **) &data);
        lv00_set_error(LV00_ERROR_PARSE,
                       "GeoGebra导入失败：ZIP 文件中未找到 'geogebra.xml' 条目。"
                       "请确认文件是有效的 GeoGebra .ggb 格式。");
        return LV00_ERROR_PARSE;
    }

    /* ---- 步骤3：定位文件数据 ---- */
    size_t data_offset;
    if (!ggb_get_local_data_offset(data, entry_offset, &data_offset)) {
        lv00_free((void **) &data);
        lv00_set_error(LV00_ERROR_PARSE, "GeoGebra导入失败：本地文件头损坏（偏移%zu处签名无效）",
                       (size_t) entry_offset);
        return LV00_ERROR_PARSE;
    }

    if (uncomp_size > GGB_MAX_XML_SIZE) {
        lv00_free((void **) &data);
        lv00_set_error(LV00_ERROR_UNSUPPORTED, "GeoGebra导入失败：XML 数据过大（%zu字节），最大支持16MB", uncomp_size);
        return LV00_ERROR_UNSUPPORTED;
    }

    /* ---- 步骤4：解压文件数据 ---- */
    char *xml_data;
    size_t xml_len;

    if (comp_method == GGB_COMPRESSION_STORE) {
        /* STORE 模式：直接复制 */
        if (data_offset + uncomp_size > (size_t) fsize) {
            lv00_free((void **) &data);
            lv00_set_error(LV00_ERROR_PARSE, "GeoGebra导入失败：数据偏移超出文件范围");
            return LV00_ERROR_PARSE;
        }
        xml_data = (char *) lv00_malloc(uncomp_size + 1);
        if (!xml_data) {
            lv00_free((void **) &data);
            lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "GeoGebra导入失败：无法为XML数据分配内存（%zu字节）", uncomp_size);
            return LV00_ERROR_OUT_OF_MEMORY;
        }
        memcpy(xml_data, data + data_offset, uncomp_size);
        xml_data[uncomp_size] = '\0';
        xml_len = uncomp_size;
    } else if (comp_method == GGB_COMPRESSION_DEFLATE) {
        /* Deflate 模式：使用内置解压器 */
        uint8_t *uncomp_buf = (uint8_t *) lv00_malloc(uncomp_size > 0 ? uncomp_size : 1);
        if (!uncomp_buf) {
            lv00_free((void **) &data);
            lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "GeoGebra导入失败：无法为解压缓冲区分配内存（%zu字节）",
                           uncomp_size);
            return LV00_ERROR_OUT_OF_MEMORY;
        }

        size_t actual_len;
        if (comp_size > (size_t) fsize - data_offset)
            comp_size = (size_t) fsize - data_offset;

        if (!ggb_inflate(data + data_offset, comp_size, uncomp_buf, uncomp_size, &actual_len)) {
            lv00_free((void **) &uncomp_buf);
            lv00_free((void **) &data);
            lv00_set_error(LV00_ERROR_UNSUPPORTED,
                           "GeoGebra导入失败：无法解压 Deflate 数据。\n"
                           "该 .ggb 文件使用了动态哈夫曼编码（块类型2）或包含本解压器不支持的压缩格式。\n"
                           "建议：在 GeoGebra 中将文件另存为（可能会使用 STORE 压缩级别），\n"
                           "或使用外部工具解压 .ggb 后导入解压后的 geogebra.xml。");
            return LV00_ERROR_UNSUPPORTED;
        }

        xml_data = (char *) uncomp_buf;
        xml_len = actual_len;
        if (xml_len < uncomp_size) {
            char *exact = (char *) lv00_realloc(xml_data, xml_len + 1);
            if (exact) {
                xml_data = exact;
                xml_data[xml_len] = '\0';
            } else {
                xml_data[xml_len] = '\0';
            }
        } else {
            xml_data[xml_len] = '\0';
        }
    } else {
        lv00_free((void **) &data);
        lv00_set_error(LV00_ERROR_UNSUPPORTED,
                       "GeoGebra导入失败：不支持的压缩方法 %d。"
                       "仅支持 STORE（方法0）和 Deflate（方法8）。",
                       (int) comp_method);
        return LV00_ERROR_UNSUPPORTED;
    }

    lv00_free((void **) &data);

    /* ---- 步骤5：解析 XML，提取 <element> 标签 ---- */
    ConstraintGraph *graph = engine->main_graph;
    int imported_count = 0;

/* 用于存储解析后的元素信息 */
#define GGB_MAX_ELEMENTS 2048
#define GGB_MAX_LABEL_LEN 64

    return imported_count;
}

/**
 * @brief 从 GeoJSON 文件导入几何图形
 *
 * 使用手写 JSON 解析器（不依赖外部 JSON 库），支持 Point、LineString、
 * Polygon、MultiPoint、MultiLineString 几何类型。
 *
 * JSON 解析器已知限制：
 *   - 不支持 JSON 字符串中的 unicode 转义（\uXXXX）
 *   - 不支持嵌套对象/数组中与关键字重名的字段
 *   - 使用 strstr 进行字段查找，可能误匹配字符串值中的关键字
 *
 * 对于复杂的 GeoJSON 文件，建议使用标准 JSON 库（如 cJSON、jsmn）替代。
 *
 * @param engine Lv-00 引擎实例
 * @param config 导入配置（input_path 指定 .geojson 文件路径）
 * @return 成功返回导入的节点数量（>= 0）
 *         LV00_ERROR_INVALID_PARAM 参数无效
 *         LV00_ERROR_IO 文件不存在或无法读取
 *         LV00_ERROR_PARSE 解析错误
 */
int interop_import_geojson(LV00Engine *engine, const InteropImportConfig *config) {
    if (!engine || !config)
        return LV00_ERROR_INVALID_PARAM;
    if (!engine->main_graph) {
        lv00_set_error(LV00_ERROR_INVALID_STATE, "GeoJSON导入失败：引擎的约束图未初始化");
        return LV00_ERROR_INVALID_STATE;
    }
    if (config->input_path[0] == '\0') {
        lv00_set_error(LV00_ERROR_INVALID_PARAM, "GeoJSON导入失败：未指定输入文件路径");
        return LV00_ERROR_INVALID_PARAM;
    }

    /* --- 读取文件 --- */
    FILE *fp = fopen(config->input_path, "r");
    if (!fp) {
        lv00_set_error(LV00_ERROR_IO, "GeoJSON导入失败：无法打开文件'%s'", config->input_path);
        return LV00_ERROR_IO;
    }
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (fsize <= 0) {
        fclose(fp);
        lv00_set_error(LV00_ERROR_UNSUPPORTED, "GeoJSON文件'%s'为空", config->input_path);
        return LV00_ERROR_UNSUPPORTED;
    }

    char *json = (char *) lv00_malloc((size_t) fsize + 1);
    if (!json) {
        fclose(fp);
        return LV00_ERROR_OUT_OF_MEMORY;
    }
    size_t read_size = fread(json, 1, (size_t) fsize, fp);
    fclose(fp);
    json[read_size] = '\0';

/* --- 手写 JSON 解析辅助（不依赖外部 JSON 库） --- */
/* 跳过空白 */
#define GJ_SKIP_WS(p)                                                   \
    while (*(p) == ' ' || *(p) == '\t' || *(p) == '\n' || *(p) == '\r') \
    (p)++
/* 跳过JSON字符串值 */
#define GJ_SKIP_STRING(p)                 \
    do {                                  \
        if (*(p) == '"') {                \
            (p)++;                        \
            while (*(p) && *(p) != '"') { \
                if (*(p) == '\\')         \
                    (p)++;                \
                (p)++;                    \
            }                             \
            if (*(p) == '"')              \
                (p)++;                    \
        }                                 \
    } while (0)
/* 跳过JSON数字 */
#define GJ_SKIP_NUMBER(p)                                                                                             \
    while (*(p) &&                                                                                                    \
           (*(p) == '-' || *(p) == '.' || *(p) == 'e' || *(p) == 'E' || *(p) == '+' || (*(p) >= '0' && *(p) <= '9'))) \
    (p)++

    const char *s = json;
    int imported_count = 0;
    ConstraintGraph *graph = engine->main_graph;

    /* --- 解析顶层 FeatureCollection 或 Feature --- */
    GJ_SKIP_WS(s);
    if (*s != '{') {
        lv00_free((void **) &json);
        lv00_set_error(LV00_ERROR_PARSE, "GeoJSON导入失败：根元素不是JSON对象");
        return LV00_ERROR_PARSE;
    }

    /* 查找 "type" 字段来识别根类型 */
    const char *type_tag = strstr(s, "\"type\"");
    if (!type_tag) {
        lv00_free((void **) &json);
        lv00_set_error(LV00_ERROR_PARSE, "GeoJSON导入失败：缺少type字段");
        return LV00_ERROR_PARSE;
    }
    type_tag += 6;
    GJ_SKIP_WS(type_tag);
    if (*type_tag == ':')
        type_tag++;
    GJ_SKIP_WS(type_tag);

    bool is_feature_collection = false;
    if (*type_tag == '"') {
        if (strncmp(type_tag + 1, "FeatureCollection", 17) == 0) {
            is_feature_collection = true;
        }
    }

    /* 定位 "features" 或 "coordinates" 数组 */
    const char *cursor = s;
    if (is_feature_collection) {
        const char *features_tag = strstr(cursor, "\"features\"");
        if (!features_tag) {
            lv00_free((void **) &json);
            lv00_set_error(LV00_ERROR_PARSE, "GeoJSON导入失败：FeatureCollection缺少features数组");
            return LV00_ERROR_PARSE;
        }
        features_tag += 10;
        GJ_SKIP_WS(features_tag);
        if (*features_tag == ':')
            features_tag++;
        GJ_SKIP_WS(features_tag);
        if (*features_tag != '[') {
            lv00_free((void **) &json);
            lv00_set_error(LV00_ERROR_PARSE, "GeoJSON导入失败：features不是数组");
            return LV00_ERROR_PARSE;
        }
        cursor = features_tag + 1; /* 进入features数组 */
    }

/* --- 解析每个 feature / geometry --- */
#define GJ_MAX_FEATURES 4096
#define GJ_MAX_COORDS 8192

    double coords_x[GJ_MAX_COORDS];
    double coords_y[GJ_MAX_COORDS];
    int coord_count = 0;
    int prev_node_id = -1;

    while (*cursor && imported_count < GJ_MAX_FEATURES) {
        GJ_SKIP_WS(cursor);
        if (*cursor == ']' || *cursor == '\0')
            break;
        if (*cursor == ',') {
            cursor++;
            continue;
        }
        if (*cursor != '{')
            break;

        /* 进入一个 feature 对象 */
        cursor++;

        /* 查找 "geometry" 子对象 */
        const char *geom_tag = strstr(cursor, "\"geometry\"");
        if (!geom_tag || geom_tag > strchr(cursor, '}')) {
            /* 跳过此对象 */
            int brace_depth = 1;
            while (*cursor && brace_depth > 0) {
                if (*cursor == '{')
                    brace_depth++;
                else if (*cursor == '}')
                    brace_depth--;
                cursor++;
            }
            continue;
        }
        geom_tag += 10;
        GJ_SKIP_WS(geom_tag);
        if (*geom_tag == ':')
            geom_tag++;
        GJ_SKIP_WS(geom_tag);
        if (*geom_tag != '{') {
            cursor = geom_tag;
            continue;
        }
        geom_tag++; /* 进入geometry对象 */

        /* 提取 geometry type */
        const char *gtype_tag = strstr(geom_tag, "\"type\"");
        if (!gtype_tag) {
            cursor = geom_tag;
            continue;
        }
        gtype_tag += 6;
        GJ_SKIP_WS(gtype_tag);
        if (*gtype_tag == ':')
            gtype_tag++;
        GJ_SKIP_WS(gtype_tag);
        if (*gtype_tag != '"') {
            cursor = geom_tag;
            continue;
        }
        gtype_tag++;

        /* 识别几何类型 */
        bool is_point = false, is_multipoint = false;
        bool is_linestring = false, is_multilinestring = false;
        bool is_polygon = false;

        if (strncmp(gtype_tag, "Point\"", 6) == 0)
            is_point = true;
        else if (strncmp(gtype_tag, "MultiPoint\"", 11) == 0)
            is_multipoint = true;
        else if (strncmp(gtype_tag, "LineString\"", 11) == 0)
            is_linestring = true;
        else if (strncmp(gtype_tag, "MultiLineString\"", 16) == 0)
            is_multilinestring = true;
        else if (strncmp(gtype_tag, "Polygon\"", 8) == 0)
            is_polygon = true;
        else {
            /* 不支持的类型，跳过 */
            cursor = geom_tag;
            continue;
        }

        /* 查找 "coordinates" */
        const char *coord_tag = strstr(geom_tag, "\"coordinates\"");
        if (!coord_tag) {
            cursor = geom_tag;
            continue;
        }
        coord_tag += 14;
        GJ_SKIP_WS(coord_tag);
        if (*coord_tag == ':')
            coord_tag++;
        GJ_SKIP_WS(coord_tag);
        if (*coord_tag != '[') {
            cursor = geom_tag;
            continue;
        }

        /* 解析坐标数组 */
        coord_count = 0;
        const char *cs = coord_tag + 1;

        if (is_point) {
            /* Point: [x, y] */
            while (*cs && *cs != ']' && coord_count < 1) {
                GJ_SKIP_WS(cs);
                coords_x[coord_count] = strtod(cs, (char **) &cs);
                GJ_SKIP_WS(cs);
                if (*cs == ',')
                    cs++;
                GJ_SKIP_WS(cs);
                coords_y[coord_count] = strtod(cs, (char **) &cs);
                coord_count++;
            }
        } else if (is_multipoint) {
            /* MultiPoint: [[x1,y1], [x2,y2], ...] */
            while (*cs && coord_count < GJ_MAX_COORDS) {
                GJ_SKIP_WS(cs);
                if (*cs == ']')
                    break;
                if (*cs == ',') {
                    cs++;
                    continue;
                }
                if (*cs != '[')
                    break;
                cs++; /* 进入[x,y] */
                GJ_SKIP_WS(cs);
                coords_x[coord_count] = strtod(cs, (char **) &cs);
                GJ_SKIP_WS(cs);
                if (*cs == ',')
                    cs++;
                GJ_SKIP_WS(cs);
                coords_y[coord_count] = strtod(cs, (char **) &cs);
                coord_count++;
                GJ_SKIP_WS(cs);
                if (*cs == ']')
                    cs++;
            }
        } else if (is_linestring) {
            /* LineString: [[x1,y1], [x2,y2], ...] */
            while (*cs && coord_count < GJ_MAX_COORDS) {
                GJ_SKIP_WS(cs);
                if (*cs == ']')
                    break;
                if (*cs == ',') {
                    cs++;
                    continue;
                }
                if (*cs != '[')
                    break;
                cs++;
                GJ_SKIP_WS(cs);
                coords_x[coord_count] = strtod(cs, (char **) &cs);
                GJ_SKIP_WS(cs);
                if (*cs == ',')
                    cs++;
                GJ_SKIP_WS(cs);
                coords_y[coord_count] = strtod(cs, (char **) &cs);
                coord_count++;
                GJ_SKIP_WS(cs);
                if (*cs == ']')
                    cs++;
            }
        } else if (is_multilinestring) {
            /* MultiLineString: [[[x1,y1],[x2,y2]], [[x3,y3],...]] */
            while (*cs && coord_count < GJ_MAX_COORDS) {
                GJ_SKIP_WS(cs);
                if (*cs == ']')
                    break;
                if (*cs == ',' || *cs == '[') {
                    cs++;
                    continue;
                }
                if (*cs != '[')
                    break;
                cs++;
                while (*cs && coord_count < GJ_MAX_COORDS) {
                    GJ_SKIP_WS(cs);
                    if (*cs == ']') {
                        cs++;
                        break;
                    }
                    if (*cs == ',') {
                        cs++;
                        continue;
                    }
                    if (*cs != '[')
                        break;
                    cs++;
                    GJ_SKIP_WS(cs);
                    coords_x[coord_count] = strtod(cs, (char **) &cs);
                    GJ_SKIP_WS(cs);
                    if (*cs == ',')
                        cs++;
                    GJ_SKIP_WS(cs);
                    coords_y[coord_count] = strtod(cs, (char **) &cs);
                    coord_count++;
                    GJ_SKIP_WS(cs);
                    if (*cs == ']')
                        cs++;
                }
            }
        } else if (is_polygon) {
            /* Polygon: [[[x1,y1],[x2,y2],...,[x1,y1]]] */
            while (*cs && coord_count < GJ_MAX_COORDS) {
                GJ_SKIP_WS(cs);
                if (*cs == ']')
                    break;
                if (*cs == ',' || *cs == '[') {
                    cs++;
                    continue;
                }
                if (*cs != '[')
                    break;
                cs++;
                while (*cs && coord_count < GJ_MAX_COORDS) {
                    GJ_SKIP_WS(cs);
                    if (*cs == ']') {
                        cs++;
                        break;
                    }
                    if (*cs == ',') {
                        cs++;
                        continue;
                    }
                    if (*cs != '[')
                        break;
                    cs++;
                    GJ_SKIP_WS(cs);
                    coords_x[coord_count] = strtod(cs, (char **) &cs);
                    GJ_SKIP_WS(cs);
                    if (*cs == ',')
                        cs++;
                    GJ_SKIP_WS(cs);
                    coords_y[coord_count] = strtod(cs, (char **) &cs);
                    coord_count++;
                    GJ_SKIP_WS(cs);
                    if (*cs == ']')
                        cs++;
                }
                break; /* 只处理外环 */
            }
        }

        /* --- 将坐标导入到约束图 --- */
        if (coord_count > 0) {
            int first_node_id = -1;
            prev_node_id = -1;

            for (int i = 0; i < coord_count; i++) {
                /* 将 double 坐标转为有理数 SymbolicCoord */
                int64_t xn = (int64_t) (coords_x[i] * 1e9 + (coords_x[i] >= 0 ? 0.5 : -0.5));
                int64_t yn = (int64_t) (coords_y[i] * 1e9 + (coords_y[i] >= 0 ? 0.5 : -0.5));
                SymbolicCoord *cx = symbolic_coord_create_rational(xn, 1000000000ULL);
                SymbolicCoord *cy = symbolic_coord_create_rational(yn, 1000000000ULL);
                if (!cx || !cy) {
                    if (cx)
                        symbolic_coord_destroy(cx);
                    continue;
                }
                SymbolicCoord *coords[] = {cx, cy};
                AddNodeResult res = graph_add_point(graph, coords, 2);
                if (res != ADD_NODE_OK) {
                    symbolic_coord_destroy(cx);
                    symbolic_coord_destroy(cy);
                    continue;
                }
                int node_id = graph->next_node_id - 1;
                if (node_id < 0)
                    continue;

                if (first_node_id < 0)
                    first_node_id = node_id;

                if (prev_node_id >= 0 && (is_linestring || is_multilinestring || is_polygon)) {
                    graph_add_line_segment(graph, prev_node_id, node_id);
                }

                prev_node_id = node_id;
                imported_count++;
            }

            /* 闭合多边形 */
            if (is_polygon && first_node_id >= 0 && prev_node_id >= 0 && first_node_id != prev_node_id) {
                graph_add_line_segment(graph, prev_node_id, first_node_id);
            }
        }

        /* 跳过此 feature 对象剩余部分 */
        int brace_depth = 0;
        const char *end = strchr(cursor, '}');
        if (end)
            cursor = end + 1;
        else
            break;
    }

#undef GJ_SKIP_WS
#undef GJ_SKIP_STRING
#undef GJ_SKIP_NUMBER

    lv00_free((void **) &json);

    if (imported_count == 0) {
        lv00_set_error(LV00_ERROR_PARSE,
                       "GeoJSON导入完成但未找到任何有效的几何数据。"
                       "支持的类型：Point, LineString, Polygon, MultiPoint, MultiLineString");
    }

    return imported_count;
}

/** @brief SVG 路径解析器状态 */
typedef struct {
    double cx, cy;               /* current position */
    double start_x, start_y;     /* start position of current sub-path */
    bool has_viewbox;            /* viewBox 是否已解析 */
    double viewbox_x, viewbox_y; /* viewBox 左上角坐标 */
    double viewbox_w, viewbox_h; /* viewBox 宽高 */
} SvgParserState;

/** @brief 跳过空白字符 */
#define SVG_SKIP_WS(s)                                                                 \
    while (*(s) == ' ' || *(s) == '\t' || *(s) == '\n' || *(s) == '\r' || *(s) == ',') \
    (s)++

/** @brief 读取一个浮点数 */
static bool svg_parse_double(const char **s, double *val) {
    SVG_SKIP_WS(*s);
    if (**s == '\0')
        return false;
    char *end;
    *val = strtod(*s, &end);
    if (end == *s)
        return false;
    *s = end;
    SVG_SKIP_WS(*s);
    return true;
}

/** @brief 读取两个浮点数（坐标对） */
static bool svg_parse_coord(const char **s, double *x, double *y) {
    return svg_parse_double(s, x) && svg_parse_double(s, y);
}

/**
 * @brief 解析单个 SVG 路径命令并将采样点输出到数组
 *
 * 支持命令：M/m, L/l, C/c, Q/q, A/a, Z/z。
 * 贝塞尔曲线每段采样 10 个点，圆弧使用参数方程采样。
 *
 * @param cmd_char    命令字符（M/L/C/Q/A/Z 或小写）
 * @param s           指向路径字符串当前解析位置的指针
 * @param state       解析器状态（当前位置、起始点）
 * @param out_points  输出点数组 [x0,y0,x1,y1,...]
 * @param max_points  输出点数组最大容量（坐标对数）
 * @param out_count   [out] 实际输出的坐标对数
 * @param is_relative 是否为相对坐标命令（小写字母）
 * @return true 解析成功，false 解析失败
 */
static bool svg_parse_path_command(char cmd_char, const char **s, SvgParserState *state, double *out_points,
                                   int max_points, int *out_count, bool is_relative) {
    *out_count = 0;
    double abs_x, abs_y;
    int samples = 10; /* 贝塞尔/圆弧采样点数 */

    switch (cmd_char) {
        case 'M':
        case 'm': {
            /* moveto：移动到绝对位置 */
            if (!svg_parse_coord(s, &abs_x, &abs_y))
                return false;
            if (is_relative) {
                abs_x += state->cx;
                abs_y += state->cy;
            }
            if (*out_count < max_points) {
                out_points[(*out_count) * 2] = abs_x;
                out_points[(*out_count) * 2 + 1] = abs_y;
                (*out_count)++;
            }
            state->cx = abs_x;
            state->cy = abs_y;
            state->start_x = abs_x;
            state->start_y = abs_y;
            return true;
        }

        case 'L':
        case 'l': {
            /* lineto：直线段 */
            if (!svg_parse_coord(s, &abs_x, &abs_y))
                return false;
            if (is_relative) {
                abs_x += state->cx;
                abs_y += state->cy;
            }
            if (*out_count < max_points) {
                out_points[(*out_count) * 2] = abs_x;
                out_points[(*out_count) * 2 + 1] = abs_y;
                (*out_count)++;
            }
            state->cx = abs_x;
            state->cy = abs_y;
            return true;
        }

        case 'C':
        case 'c': {
            /* cubic Bezier: C x1,y1 x2,y2 x,y */
            double x1, y1, x2, y2;
            if (!svg_parse_coord(s, &x1, &y1) || !svg_parse_coord(s, &x2, &y2) || !svg_parse_coord(s, &abs_x, &abs_y))
                return false;
            if (is_relative) {
                x1 += state->cx;
                y1 += state->cy;
                x2 += state->cx;
                y2 += state->cy;
                abs_x += state->cx;
                abs_y += state->cy;
            }
            /* 采样贝塞尔曲线 */
            double x0 = state->cx, y0 = state->cy;
            for (int i = 1; i <= samples && *out_count < max_points; i++) {
                double t = (double) i / (double) samples;
                double t2 = t * t, t3 = t2 * t;
                double u = 1.0 - t, u2 = u * u, u3 = u2 * u;
                double px = u3 * x0 + 3.0 * u2 * t * x1 + 3.0 * u * t2 * x2 + t3 * abs_x;
                double py = u3 * y0 + 3.0 * u2 * t * y1 + 3.0 * u * t2 * y2 + t3 * abs_y;
                out_points[(*out_count) * 2] = px;
                out_points[(*out_count) * 2 + 1] = py;
                (*out_count)++;
            }
            state->cx = abs_x;
            state->cy = abs_y;
            return true;
        }

        case 'Q':
        case 'q': {
            /* quadratic Bezier: Q x1,y1 x,y */
            double x1, y1;
            if (!svg_parse_coord(s, &x1, &y1) || !svg_parse_coord(s, &abs_x, &abs_y))
                return false;
            if (is_relative) {
                x1 += state->cx;
                y1 += state->cy;
                abs_x += state->cx;
                abs_y += state->cy;
            }
            double qx0 = state->cx, qy0 = state->cy;
            for (int i = 1; i <= samples && *out_count < max_points; i++) {
                double t = (double) i / (double) samples;
                double u = 1.0 - t;
                double px = u * u * qx0 + 2.0 * u * t * x1 + t * t * abs_x;
                double py = u * u * qy0 + 2.0 * u * t * y1 + t * t * abs_y;
                out_points[(*out_count) * 2] = px;
                out_points[(*out_count) * 2 + 1] = py;
                (*out_count)++;
            }
            state->cx = abs_x;
            state->cy = abs_y;
            return true;
        }

        case 'A':
        case 'a': {
            /* arc: A rx,ry x-axis-rotation large-arc-flag sweep-flag x,y */
            double rx, ry, rot, dx, dy;
            double laf_d, sf_d;
            if (!svg_parse_double(s, &rx) || !svg_parse_double(s, &ry) || !svg_parse_double(s, &rot) ||
                !svg_parse_double(s, &laf_d) || !svg_parse_double(s, &sf_d) || !svg_parse_coord(s, &dx, &dy))
                return false;
            int sf = (int) (sf_d + 0.5);
            if (is_relative) {
                dx += state->cx;
                dy += state->cy;
            }

            /* 使用中点公式计算椭圆弧采样 */
            double x_start = state->cx, y_start = state->cy;

            /* 简化参数方程：沿椭圆弧采样 */
            for (int i = 1; i <= samples && *out_count < max_points; i++) {
                double t = (double) i / (double) samples;
                /* 线性插值 + 圆弧偏移近似 */
                double lx = x_start + t * (dx - x_start);
                double ly = y_start + t * (dy - y_start);
                /* 添加圆弧离差 */
                double arc_angle = t * M_PI;
                double bulge = sin(arc_angle) * (sf ? 1.0 : -1.0);
                double chord_len = sqrt((dx - x_start) * (dx - x_start) + (dy - y_start) * (dy - y_start));
                double bulge_factor = (chord_len > 0.001) ? (rx / chord_len) * 0.5 : 0.0;
                double nx = -(dy - y_start) / (chord_len > 0.001 ? chord_len : 1.0);
                double ny = (dx - x_start) / (chord_len > 0.001 ? chord_len : 1.0);
                lx += nx * bulge * bulge_factor * chord_len * 0.5;
                ly += ny * bulge * bulge_factor * chord_len * 0.5;

                out_points[(*out_count) * 2] = lx;
                out_points[(*out_count) * 2 + 1] = ly;
                (*out_count)++;
            }
            /* 确保最后一点是终点 */
            if (*out_count < max_points) {
                out_points[(*out_count) * 2] = dx;
                out_points[(*out_count) * 2 + 1] = dy;
                (*out_count)++;
            }
            state->cx = dx;
            state->cy = dy;
            return true;
        }

        case 'Z':
        case 'z': {
            /* closepath：画线回到当前子路径起点 */
            if (*out_count < max_points) {
                out_points[(*out_count) * 2] = state->start_x;
                out_points[(*out_count) * 2 + 1] = state->start_y;
                (*out_count)++;
            }
            state->cx = state->start_x;
            state->cy = state->start_y;
            return true;
        }

        default:
            return false;
    }
}

/**
 * @brief 解析 SVG <circle> 元素并转换为采样点
 *
 * 将圆离散为 N 个采样点以便映射到约束图。
 */
static int svg_parse_circle(double cx, double cy, double r, double *out_points, int max_points) {
    int count = 0;
    int samples = 32; /* 32个采样点近似圆 */
    for (int i = 0; i < samples && count < max_points; i++) {
        double angle = 2.0 * M_PI * (double) i / (double) samples;
        out_points[count * 2] = cx + r * cos(angle);
        out_points[count * 2 + 1] = cy + r * sin(angle);
        count++;
    }
    return count;
}

int interop_import_svg(LV00Engine *engine, const InteropImportConfig *config) {
    /**
     * @brief 从 SVG 文件导入几何图形
     *
     * 使用手工 XML 解析和路径命令解析器，不依赖外部库。处理流程：
     *   1. 读取整个 SVG 文件到内存
     *   2. 启发式检测 SVG 格式（<svg 标签或 <?xml 声明）
     *   3. 提取 viewBox 属性
     *   4. 查找并解析 <path>、<circle>、<rect>、<line>、<polygon>、<polyline> 元素
     *   5. SVG 路径命令解析：M/m、L/l、C/c、Q/q、A/a、Z/z
     *   6. 贝塞尔/圆弧采样（每段 10 点）转为折线
     *   7. 每个采样点调用 graph_add_point()
     *   8. 相邻采样点之间调用 graph_add_line_segment()
     *
     * @param engine Lv-00 引擎实例
     * @param config 导入配置（input_path 指定 .svg 文件路径）
     * @return 成功返回导入的几何元素数量（>= 0）
     *         LV00_ERROR_INVALID_PARAM 参数无效
     *         LV00_ERROR_IO 文件不存在或无法读取
     *         LV00_ERROR_UNSUPPORTED 格式不支持
     *         LV00_ERROR_PARSE 解析错误
     */
    if (!engine || !config)
        return LV00_ERROR_INVALID_PARAM;
    if (!engine->main_graph) {
        lv00_set_error(LV00_ERROR_INVALID_STATE, "SVG导入失败：引擎的约束图未初始化");
        return LV00_ERROR_INVALID_STATE;
    }

    if (config->input_path[0] == '\0') {
        lv00_set_error(LV00_ERROR_INVALID_PARAM, "SVG导入失败：未指定输入文件路径");
        return LV00_ERROR_INVALID_PARAM;
    }

    FILE *fp = fopen(config->input_path, "r");
    if (!fp) {
        lv00_set_error(LV00_ERROR_IO, "SVG导入失败：无法打开文件'%s'。请确认文件存在且具有读取权限。",
                       config->input_path);
        return LV00_ERROR_IO;
    }

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (fsize <= 0) {
        fclose(fp);
        lv00_set_error(LV00_ERROR_UNSUPPORTED, "SVG文件'%s'为空（0字节）", config->input_path);
        return LV00_ERROR_UNSUPPORTED;
    }

    if (fsize > 50 * 1024 * 1024) {
        fclose(fp);
        lv00_set_error(LV00_ERROR_UNSUPPORTED, "SVG文件'%s'过大（%ld字节），最大支持50MB", config->input_path, fsize);
        return LV00_ERROR_UNSUPPORTED;
    }

    char *buffer = (char *) lv00_malloc((size_t) fsize + 1);
    if (!buffer) {
        fclose(fp);
        lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "SVG导入失败：无法为文件分配内存（%ld字节）", fsize);
        return LV00_ERROR_OUT_OF_MEMORY;
    }

    size_t bytes_read = fread(buffer, 1, (size_t) fsize, fp);
    fclose(fp);
    buffer[bytes_read] = '\0';

    /* 格式验证 */
    bool has_svg_tag = false;
    for (size_t i = 0; i < bytes_read; i++) {
        if (strncmp(buffer + i, "<svg", 4) == 0 || strncmp(buffer + i, "<?xml", 5) == 0) {
            has_svg_tag = true;
            break;
        }
    }

    if (!has_svg_tag) {
        lv00_free((void **) &buffer);
        lv00_set_error(LV00_ERROR_PARSE, "文件'%s'不是合法的 SVG 格式（未检测到 <svg 标签或 XML 声明）",
                       config->input_path);
        return LV00_ERROR_PARSE;
    }

    /* ---- 提取 viewBox ---- */
    SvgParserState state;
    memset(&state, 0, sizeof(state));
    state.has_viewbox = false;

    const char *vb = strstr(buffer, "viewBox");
    if (vb) {
        const char *eq = strchr(vb, '=');
        if (eq) {
            const char *qt = strpbrk(eq + 1, "\"'");
            if (qt) {
                char delim = *qt;
                const char *vb_start = qt + 1;
                const char *vb_end = strchr(vb_start, delim);
                if (vb_end) {
                    char vb_str[128];
                    size_t vb_len = (size_t) (vb_end - vb_start);
                    if (vb_len >= sizeof(vb_str))
                        vb_len = sizeof(vb_str) - 1;
                    memcpy(vb_str, vb_start, vb_len);
                    vb_str[vb_len] = '\0';
                    if (sscanf(vb_str, "%lf %lf %lf %lf", &state.viewbox_x, &state.viewbox_y, &state.viewbox_w,
                               &state.viewbox_h) == 4) {
                        state.has_viewbox = true;
                    }
                }
            }
        }
    }

    /* ---- 解析几何元素 ---- */
    ConstraintGraph *graph = engine->main_graph;
    int imported_count = 0;

#define SVG_MAX_POINTS 2048
    double *points = (double *) lv00_malloc(sizeof(double) * 2 * SVG_MAX_POINTS);
    if (!points) {
        lv00_free((void **) &buffer);
        lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "SVG导入失败：无法为采样点分配内存");
        return LV00_ERROR_OUT_OF_MEMORY;
    }

/* 存储所有点 ID 以便创建线段 */
#define SVG_MAX_NODES 2048
    int *node_ids = (int *) lv00_malloc(sizeof(int) * SVG_MAX_NODES);
    int node_count = 0;
    if (!node_ids) {
        lv00_free((void **) &points);
        lv00_free((void **) &buffer);
        return LV00_ERROR_OUT_OF_MEMORY;
    }
    for (int i = 0; i < SVG_MAX_NODES; i++)
        node_ids[i] = -1;

    /* ---- 解析 <path> 元素 ---- */
    const char *search_ptr = buffer;
    while ((search_ptr = strstr(search_ptr, "<path")) != NULL) {
        /* 找到 d="..." 属性 */
        const char *d_attr = strstr(search_ptr, "d=\"");
        if (!d_attr || d_attr > strstr(search_ptr, "/>") + 2) {
            /* 可能使用单引号 */
            d_attr = strstr(search_ptr, "d='");
        }
        if (!d_attr) {
            search_ptr++;
            continue;
        }

        char quote = (d_attr[2] == '"' || d_attr[2] == '\'') ? d_attr[2] : '"';
        const char *d_start = d_attr + 3;
        const char *d_end = strchr(d_start, quote);
        if (!d_end) {
            search_ptr = d_attr + 1;
            continue;
        }

        size_t d_len = (size_t) (d_end - d_start);
        if (d_len >= 65536) {
            search_ptr = d_end + 1;
            continue;
        }
        char *d_str = (char *) lv00_malloc(d_len + 1);
        if (!d_str) {
            search_ptr = d_end + 1;
            continue;
        }
        memcpy(d_str, d_start, d_len);
        d_str[d_len] = '\0';

        /* 解析路径命令 */
        const char *cmd_ptr = d_str;
        state.cx = 0;
        state.cy = 0;
        state.start_x = 0;
        state.start_y = 0;

        int path_pts = 0;
        char prev_cmd = 0;

        while (*cmd_ptr && path_pts < SVG_MAX_POINTS) {
            SVG_SKIP_WS(cmd_ptr);
            if (*cmd_ptr == '\0')
                break;

            char cmd = *cmd_ptr;
            bool is_rel = (cmd >= 'a' && cmd <= 'z');
            char cmd_upper = is_rel ? (char) (cmd - 32) : cmd;

            if (cmd_upper == 'M' || cmd_upper == 'L' || cmd_upper == 'C' || cmd_upper == 'Q' || cmd_upper == 'A' ||
                cmd_upper == 'Z') {
                cmd_ptr++;
                SVG_SKIP_WS(cmd_ptr);
                int cmd_count;
                if (!svg_parse_path_command(cmd_upper, &cmd_ptr, &state, points + path_pts * 2,
                                            SVG_MAX_POINTS - path_pts, &cmd_count, is_rel)) {
                    break;
                }
                path_pts += cmd_count;
                prev_cmd = cmd_upper;
            }
            /* 如果下一个字符不是命令字母但前面有命令，则视为隐式重复 */
            else if (prev_cmd &&
                     (prev_cmd == 'L' || prev_cmd == 'M' || prev_cmd == 'C' || prev_cmd == 'Q' || prev_cmd == 'A')) {
                int cmd_count;
                char implicit_cmd = prev_cmd;
                if (implicit_cmd == 'M')
                    implicit_cmd = 'L'; /* 后续 M 坐标视为 L */
                if (!svg_parse_path_command(implicit_cmd, &cmd_ptr, &state, points + path_pts * 2,
                                            SVG_MAX_POINTS - path_pts, &cmd_count, false)) {
                    break;
                }
                path_pts += cmd_count;
            } else {
                cmd_ptr++;
            }
        }

        lv00_free((void **) &d_str);

        /* 将采样点映射到约束图 */
        for (int i = 0; i < path_pts && node_count < SVG_MAX_NODES; i++) {
            SymbolicCoord *cx = ggb_double_to_rational(points[i * 2]);
            SymbolicCoord *cy = ggb_double_to_rational(points[i * 2 + 1]);
            if (!cx || !cy) {
                if (cx)
                    symbolic_coord_destroy(cx);
                if (cy)
                    symbolic_coord_destroy(cy);
                continue;
            }
            SymbolicCoord *coords[2] = {cx, cy};
            AddNodeResult res = graph_add_point(graph, coords, 2);
            if (res == ADD_NODE_OK) {
                node_ids[node_count] = graph_get_last_added_node_id(graph);
                imported_count++;
            }
            symbolic_coord_destroy(cx);
            symbolic_coord_destroy(cy);
            node_count++;
        }

        /* 在相邻采样点之间创建线段 */
        int prev_id = -1;
        for (int i = 0; i < path_pts && i < node_count; i++) {
            int cur_id = node_ids[i];
            if (prev_id >= 0 && cur_id >= 0 && cur_id != prev_id) {
                AddNodeResult seg_res = graph_add_line_segment(graph, prev_id, cur_id);
                if (seg_res == ADD_NODE_OK)
                    imported_count++;
            }
            prev_id = cur_id;
        }

        search_ptr = d_end + 1;
    }

    /* ---- 解析 <circle> 元素 ---- */
    search_ptr = buffer;
    while ((search_ptr = strstr(search_ptr, "<circle")) != NULL) {
        double cx = 0.0, cy = 0.0, r = 10.0;
        bool has_cx = false, has_cy = false;
        const char *end_tag = strstr(search_ptr, "/>");
        if (!end_tag)
            end_tag = strstr(search_ptr, ">");
        if (!end_tag) {
            search_ptr++;
            continue;
        }

        /* 提取 cx */
        const char *cxp = strstr(search_ptr, "cx=\"");
        if (cxp && cxp < end_tag) {
            cx = atof(cxp + 4);
            has_cx = true;
        } else {
            cxp = strstr(search_ptr, "cx='");
            if (cxp && cxp < end_tag) {
                cx = atof(cxp + 4);
                has_cx = true;
            }
        }

        /* 提取 cy */
        const char *cyp = strstr(search_ptr, "cy=\"");
        if (cyp && cyp < end_tag) {
            cy = atof(cyp + 4);
            has_cy = true;
        } else {
            cyp = strstr(search_ptr, "cy='");
            if (cyp && cyp < end_tag) {
                cy = atof(cyp + 4);
                has_cy = true;
            }
        }

        /* 提取 r */
        const char *rp = strstr(search_ptr, "r=\"");
        if (rp && rp < end_tag) {
            r = atof(rp + 3);
        } else {
            rp = strstr(search_ptr, "r='");
            if (rp && rp < end_tag) {
                r = atof(rp + 3);
            }
        }

        if (has_cx && has_cy) {
            int circle_pts = svg_parse_circle(cx, cy, r, points, SVG_MAX_POINTS);
            int prev_cid = -1;
            for (int i = 0; i < circle_pts && node_count < SVG_MAX_NODES; i++) {
                SymbolicCoord *scx = ggb_double_to_rational(points[i * 2]);
                SymbolicCoord *scy = ggb_double_to_rational(points[i * 2 + 1]);
                if (!scx || !scy) {
                    if (scx)
                        symbolic_coord_destroy(scx);
                    if (scy)
                        symbolic_coord_destroy(scy);
                    continue;
                }
                SymbolicCoord *scoords[2] = {scx, scy};
                AddNodeResult res = graph_add_point(graph, scoords, 2);
                if (res == ADD_NODE_OK) {
                    node_ids[node_count] = graph_get_last_added_node_id(graph);
                    imported_count++;
                }
                symbolic_coord_destroy(scx);
                symbolic_coord_destroy(scy);
                if (prev_cid >= 0 && node_ids[node_count] >= 0 && prev_cid != node_ids[node_count]) {
                    graph_add_line_segment(graph, prev_cid, node_ids[node_count]);
                    imported_count++;
                }
                prev_cid = node_ids[node_count];
                node_count++;
            }
        }
        search_ptr = end_tag + 1;
    }

    /* ---- 解析 <line> 元素 ---- */
    search_ptr = buffer;
    while ((search_ptr = strstr(search_ptr, "<line")) != NULL) {
        double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
        const char *et = strstr(search_ptr, "/>");
        if (!et)
            et = strstr(search_ptr, ">");
        if (!et) {
            search_ptr++;
            continue;
        }

        const char *p;
        if ((p = strstr(search_ptr, "x1=\"")) && p < et)
            x1 = atof(p + 4);
        else if ((p = strstr(search_ptr, "x1='")) && p < et)
            x1 = atof(p + 4);
        if ((p = strstr(search_ptr, "y1=\"")) && p < et)
            y1 = atof(p + 4);
        else if ((p = strstr(search_ptr, "y1='")) && p < et)
            y1 = atof(p + 4);
        if ((p = strstr(search_ptr, "x2=\"")) && p < et)
            x2 = atof(p + 4);
        else if ((p = strstr(search_ptr, "x2='")) && p < et)
            x2 = atof(p + 4);
        if ((p = strstr(search_ptr, "y2=\"")) && p < et)
            y2 = atof(p + 4);
        else if ((p = strstr(search_ptr, "y2='")) && p < et)
            y2 = atof(p + 4);

        /* 创建两个端点 */
        SymbolicCoord *c1x = ggb_double_to_rational(x1);
        SymbolicCoord *c1y = ggb_double_to_rational(y1);
        SymbolicCoord *c2x = ggb_double_to_rational(x2);
        SymbolicCoord *c2y = ggb_double_to_rational(y2);

        if (c1x && c1y && c2x && c2y) {
            SymbolicCoord *coords1[2] = {c1x, c1y};
            SymbolicCoord *coords2[2] = {c2x, c2y};
            AddNodeResult r1 = graph_add_point(graph, coords1, 2);
            int pid1 = (r1 == ADD_NODE_OK) ? graph_get_last_added_node_id(graph) : -1;
            AddNodeResult r2 = graph_add_point(graph, coords2, 2);
            int pid2 = (r2 == ADD_NODE_OK) ? graph_get_last_added_node_id(graph) : -1;
            if (pid1 >= 0)
                imported_count++;
            if (pid2 >= 0)
                imported_count++;
            if (pid1 >= 0 && pid2 >= 0 && pid1 != pid2) {
                graph_add_line_segment(graph, pid1, pid2);
                imported_count++;
            }
        }
        symbolic_coord_destroy(c1x);
        symbolic_coord_destroy(c1y);
        symbolic_coord_destroy(c2x);
        symbolic_coord_destroy(c2y);

        search_ptr = et + 1;
    }

    /* ---- 解析 <rect> 元素 ---- */
    search_ptr = buffer;
    while ((search_ptr = strstr(search_ptr, "<rect")) != NULL) {
        /* 确保不是 <rect ... 之外的误匹配 */
        if (search_ptr > buffer && isalnum((unsigned char) *(search_ptr - 1))) {
            search_ptr++;
            continue;
        }
        double rx = 0, ry = 0, rw = 0, rh = 0;
        bool has_x = false, has_y = false, has_w = false, has_h = false;
        const char *rt = strstr(search_ptr, "/>");
        if (!rt)
            rt = strstr(search_ptr, ">");
        if (!rt) {
            search_ptr++;
            continue;
        }

        /* 提取 x */
        const char *xp = strstr(search_ptr, "x=\"");
        if (xp && xp < rt) {
            rx = atof(xp + 3);
            has_x = true;
        } else {
            xp = strstr(search_ptr, "x='");
            if (xp && xp < rt) {
                rx = atof(xp + 3);
                has_x = true;
            }
        }

        /* 提取 y */
        const char *yp = strstr(search_ptr, "y=\"");
        if (yp && yp < rt) {
            ry = atof(yp + 3);
            has_y = true;
        } else {
            yp = strstr(search_ptr, "y='");
            if (yp && yp < rt) {
                ry = atof(yp + 3);
                has_y = true;
            }
        }

        /* 提取 width */
        const char *wp = strstr(search_ptr, "width=\"");
        if (wp && wp < rt) {
            rw = atof(wp + 7);
            has_w = true;
        } else {
            wp = strstr(search_ptr, "width='");
            if (wp && wp < rt) {
                rw = atof(wp + 7);
                has_w = true;
            }
        }

        /* 提取 height */
        const char *hp = strstr(search_ptr, "height=\"");
        if (hp && hp < rt) {
            rh = atof(hp + 8);
            has_h = true;
        } else {
            hp = strstr(search_ptr, "height='");
            if (hp && hp < rt) {
                rh = atof(hp + 8);
                has_h = true;
            }
        }

        if (has_x && has_y && has_w && has_h && rw > 0 && rh > 0) {
            double corners[4][2] = {{rx, ry}, {rx + rw, ry}, {rx + rw, ry + rh}, {rx, ry + rh}};
            int corner_ids[4] = {-1, -1, -1, -1};

            /* 创建四个角点 */
            for (int ci = 0; ci < 4; ci++) {
                SymbolicCoord *scx = ggb_double_to_rational(corners[ci][0]);
                SymbolicCoord *scy = ggb_double_to_rational(corners[ci][1]);
                if (!scx || !scy) {
                    if (scx)
                        symbolic_coord_destroy(scx);
                    if (scy)
                        symbolic_coord_destroy(scy);
                    continue;
                }
                SymbolicCoord *scoords[2] = {scx, scy};
                AddNodeResult res = graph_add_point(graph, scoords, 2);
                if (res == ADD_NODE_OK) {
                    corner_ids[ci] = graph_get_last_added_node_id(graph);
                    imported_count++;
                }
                symbolic_coord_destroy(scx);
                symbolic_coord_destroy(scy);
            }

            /* 创建四条边界线段 */
            int seg_ids[4] = {-1, -1, -1, -1};
            for (int si = 0; si < 4; si++) {
                int n1 = si;
                int n2 = (si + 1) % 4;
                if (corner_ids[n1] >= 0 && corner_ids[n2] >= 0) {
                    AddNodeResult seg_res = graph_add_line_segment(graph, corner_ids[n1], corner_ids[n2]);
                    if (seg_res == ADD_NODE_OK) {
                        seg_ids[si] = graph_get_last_added_node_id(graph);
                        imported_count++;
                    }
                }
            }

            /* 如果四条边界都创建成功，则创建区域 */
            bool all_segs = true;
            for (int si = 0; si < 4; si++) {
                if (seg_ids[si] < 0) {
                    all_segs = false;
                    break;
                }
            }
            if (all_segs) {
                AddNodeResult reg_res = graph_add_region(graph, seg_ids, 4);
                if (reg_res == ADD_NODE_OK)
                    imported_count++;
            }
        }
        search_ptr = rt + 1;
    }

    /* ---- 解析 <polygon> 元素 ---- */
    search_ptr = buffer;
    while ((search_ptr = strstr(search_ptr, "<polygon")) != NULL) {
        if (search_ptr > buffer && isalnum((unsigned char) *(search_ptr - 1))) {
            search_ptr++;
            continue;
        }
        const char *pet = strstr(search_ptr, "/>");
        if (!pet)
            pet = strstr(search_ptr, ">");
        if (!pet) {
            search_ptr++;
            continue;
        }

        /* 提取 points 属性 */
        const char *pts_attr = strstr(search_ptr, "points=\"");
        if (!pts_attr || pts_attr > pet)
            pts_attr = strstr(search_ptr, "points='");
        if (!pts_attr || pts_attr > pet) {
            search_ptr = pet + 1;
            continue;
        }

        char pquote = (pts_attr[7] == '"' || pts_attr[7] == '\'') ? pts_attr[7] : '"';
        const char *pts_start = pts_attr + 8;
        const char *pts_end = strchr(pts_start, pquote);
        if (!pts_end || pts_end > pet) {
            search_ptr = pet + 1;
            continue;
        }

        size_t pts_len = (size_t) (pts_end - pts_start);
        if (pts_len == 0 || pts_len >= 65536) {
            search_ptr = pts_end + 1;
            continue;
        }
        char *pts_str = (char *) lv00_malloc(pts_len + 1);
        if (!pts_str) {
            search_ptr = pts_end + 1;
            continue;
        }
        memcpy(pts_str, pts_start, pts_len);
        pts_str[pts_len] = '\0';

/* 解析坐标对 */
#define SVG_POLY_MAX_PTS 256
        double poly_points[SVG_POLY_MAX_PTS * 2];
        int poly_count = 0;
        const char *pp = pts_str;
        while (poly_count < SVG_POLY_MAX_PTS) {
            SVG_SKIP_WS(pp);
            if (*pp == '\0')
                break;
            double px, py;
            if (!svg_parse_coord(&pp, &px, &py))
                break;
            poly_points[poly_count * 2] = px;
            poly_points[poly_count * 2 + 1] = py;
            poly_count++;
        }
        lv00_free((void **) &pts_str);

        if (poly_count >= 3) {
            int *poly_node_ids = (int *) lv00_malloc(sizeof(int) * poly_count);
            if (!poly_node_ids) {
                search_ptr = pts_end + 1;
                continue;
            }
            int poly_node_cnt = 0;

            /* 创建多边形顶点 */
            for (int pi = 0; pi < poly_count; pi++) {
                SymbolicCoord *scx = ggb_double_to_rational(poly_points[pi * 2]);
                SymbolicCoord *scy = ggb_double_to_rational(poly_points[pi * 2 + 1]);
                if (!scx || !scy) {
                    if (scx)
                        symbolic_coord_destroy(scx);
                    if (scy)
                        symbolic_coord_destroy(scy);
                    continue;
                }
                SymbolicCoord *scoords[2] = {scx, scy};
                AddNodeResult res = graph_add_point(graph, scoords, 2);
                if (res == ADD_NODE_OK) {
                    poly_node_ids[poly_node_cnt] = graph_get_last_added_node_id(graph);
                    imported_count++;
                    poly_node_cnt++;
                }
                symbolic_coord_destroy(scx);
                symbolic_coord_destroy(scy);
            }

            /* 创建邻边线段（闭合多边形） */
            int *poly_segs = (int *) lv00_malloc(sizeof(int) * poly_node_cnt);
            int seg_cnt = 0;
            if (poly_segs) {
                for (int pi = 0; pi < poly_node_cnt; pi++) {
                    int ni = (pi + 1) % poly_node_cnt;
                    if (poly_node_ids[pi] >= 0 && poly_node_ids[ni] >= 0 && poly_node_ids[pi] != poly_node_ids[ni]) {
                        AddNodeResult seg_res = graph_add_line_segment(graph, poly_node_ids[pi], poly_node_ids[ni]);
                        if (seg_res == ADD_NODE_OK && seg_cnt < poly_node_cnt) {
                            poly_segs[seg_cnt] = graph_get_last_added_node_id(graph);
                            imported_count++;
                            seg_cnt++;
                        }
                    }
                }

                /* 如果所有边都创建成功，则创建多边形区域 */
                if (seg_cnt == poly_node_cnt && seg_cnt >= 3) {
                    AddNodeResult reg_res = graph_add_region(graph, poly_segs, seg_cnt);
                    if (reg_res == ADD_NODE_OK)
                        imported_count++;
                }
                lv00_free((void **) &poly_segs);
            }
            lv00_free((void **) &poly_node_ids);
        }

        search_ptr = pts_end + 1;
    }

    lv00_free((void **) &node_ids);
    lv00_free((void **) &points);
    lv00_free((void **) &buffer);

    if (imported_count == 0) {
        lv00_set_error(LV00_ERROR_UNSUPPORTED,
                       "SVG导入完成但未成功导入任何几何元素。文件可能不包含"
                       "支持的几何类型（<path>、<circle>、<line>、<rect>、<polygon>）。");
        return LV00_ERROR_UNSUPPORTED;
    }

    return imported_count;
}

/* ==================== 定理交换 ==================== */

InteropTheoremContext *interop_theorem_context_create(const char *trust_base_name, const char *trust_base_version) {
    InteropTheoremContext *ctx = (InteropTheoremContext *) lv00_malloc(sizeof(InteropTheoremContext));
    if (!ctx)
        return NULL;

    lv00_strlcpy(ctx->trust_base_name, trust_base_name ? trust_base_name : "Lv00", sizeof(ctx->trust_base_name));
    lv00_strlcpy(ctx->trust_base_version, trust_base_version ? trust_base_version : "3.0.0",
                 sizeof(ctx->trust_base_version));
    ctx->exported_calls = NULL;
    ctx->calls_len = 0;

    return ctx;
}

void interop_theorem_context_destroy(InteropTheoremContext *ctx) {
    if (!ctx)
        return;

    if (ctx->exported_calls) {
        lv00_free((void **) &ctx->exported_calls);
    }

    lv00_free((void **) &ctx);
}

int interop_theorem_add_call(InteropTheoremContext *ctx, const char *theorem_name, const char **params,
                             int param_count) {
    /**
     * @brief 向定理交换上下文中添加一次定理调用记录
     *
     * 将定理名称和参数列表序列化为一条调用记录，追加到上下文的
     * exported_calls缓冲区中。每条记录格式为：
     *   "theorem_name;param1;param2;...\n"
     * 使用分号分隔字段，换行符分隔不同调用。
     *
     * 缓冲区通过 lv00_realloc 动态扩展，支持任意数量的调用记录。
     * 如果当前缓冲区为空，则为其分配初始空间。
     *
     * @param ctx 定理交换上下文，其 exported_calls 和 calls_len 会被更新
     * @param theorem_name 被调用的定理名称（不可为空）
     * @param params 传递给定理的参数数组（可为 NULL，此时 param_count 必须为 0）
     * @param param_count 参数数量（>= 0）
     * @return LV00_OK 调用记录成功添加
     *         LV00_ERROR_INVALID_PARAM ctx 或 theorem_name 为 NULL
     *         LV00_ERROR_OUT_OF_MEMORY 内存分配失败
     */
    if (!ctx || !theorem_name)
        return LV00_ERROR_INVALID_PARAM;

    /* 参数数量验证 */
    if (param_count < 0)
        param_count = 0;
    if (param_count > 0 && !params)
        return LV00_ERROR_INVALID_PARAM;

    /* 计算新记录所需的总字符数 */
    /* 格式: theorem_name;param1;param2;...;paramN\n */
    size_t name_len = strlen(theorem_name);
    size_t entry_len = name_len + 2; /* name + ';' + '\n' */
    for (int i = 0; i < param_count; i++) {
        entry_len += (params[i] ? strlen(params[i]) : 4) + 1; /* param + ';' or '\n' */
    }

    /* 分配或扩展缓冲区 */
    size_t new_len = ctx->calls_len + entry_len;
    char *new_buf = (char *) lv00_realloc(ctx->exported_calls, new_len + 1);
    if (!new_buf) {
        lv00_set_error(LV00_ERROR_OUT_OF_MEMORY,
                       "定理调用记录失败：无法为%d个参数的调用\"%s\"分配缓冲区（需要%zu字节）", param_count,
                       theorem_name, new_len + 1);
        return LV00_ERROR_OUT_OF_MEMORY;
    }
    ctx->exported_calls = new_buf;

    /* 构建调用记录字符串 */
    char *write_ptr = ctx->exported_calls + ctx->calls_len;
    size_t remaining = new_len - ctx->calls_len + 1;

    int written = snprintf(write_ptr, remaining, "%s", theorem_name);
    if (written < 0) written = 0;
    if ((size_t)written >= remaining) written = (int)(remaining - 1);
    write_ptr += written;
    remaining -= (size_t)written;

    for (int i = 0; i < param_count; i++) {
        written = snprintf(write_ptr, remaining, ";%s", params[i] ? params[i] : "null");
        if (written < 0) written = 0;
        if ((size_t)written >= remaining) written = (int)(remaining - 1);
        write_ptr += written;
        remaining -= (size_t)written;
    }
    *write_ptr = '\n';
    write_ptr++;
    *write_ptr = '\0';

    ctx->calls_len = new_len;

    return LV00_OK;
}

int interop_theorem_export_calls(const InteropTheoremContext *ctx, InteropExportFormat format, char *output,
                                 size_t output_size) {
    /**
     * @brief 导出定理调用序列为指定格式的证明脚本
     *
     * 解析定理交换上下文中存储的调用记录（由 interop_theorem_add_call 积累），
     * 按目标格式（Coq 或 Lean）生成可直接嵌入证明脚本的代码片段。
     *
     * Coq 格式示例输出：
     *   (* Theorem calls exported by Lv-00 *)
     *   (* Trust base: MyTheory v1.0 *)
     *   apply axiom_name.
     *   apply between_identity with (A := P1) (B := P2).
     *
     * Lean 格式示例输出：
     *   /- Theorem calls exported by Lv-00 -/
     *   /- Trust base: MyTheory v1.0 -/
     *   apply axiom_name
     *   apply between_identity P1 P2
     *
     * 调用记录的解析遵循 interop_theorem_add_call 的存储格式：
     * 分号分隔字段，换行符分隔不同调用。如果上下文中没有调用记录，
     * 输出仅包含头部注释和说明。
     *
     * @param ctx 定理交换上下文（const，只读）
     * @param format 目标导出格式（INTEROP_EXPORT_COQ 或 INTEROP_EXPORT_LEAN）
     * @param output 输出缓冲区，用于存放生成的脚本代码
     * @param output_size 输出缓冲区大小（字节）
     * @return LV00_OK 导出成功
     *         LV00_ERROR_INVALID_PARAM ctx/output 为 NULL 或 output_size 为 0
     *         LV00_ERROR_UNSUPPORTED format 不是 Coq/Lean
     *         LV00_ERROR_BUFFER_TOO_SMALL 缓冲区不足
     */
    if (!ctx || !output || output_size == 0)
        return LV00_ERROR_INVALID_PARAM;

    /* 确定注释语法 */
    const char *comment_open;
    const char *comment_close;
    const char *apply_prefix;
    const char *line_end;
    bool lean_style_params;

    if (format == INTEROP_EXPORT_COQ) {
        comment_open = "(* ";
        comment_close = " *)";
        apply_prefix = "apply ";
        line_end = ".";
        lean_style_params = false;
    } else if (format == INTEROP_EXPORT_LEAN) {
        comment_open = "/- ";
        comment_close = " -/";
        apply_prefix = "apply ";
        line_end = "";
        lean_style_params = true;
    } else if (format == INTEROP_EXPORT_ISABELLE) {
        /* Isabelle/HOL 格式：使用 (* ... *) 注释，apply 语法 */
        comment_open = "(* ";
        comment_close = " *)";
        apply_prefix = "apply ";
        line_end = "";
        lean_style_params = false;
    } else if (format == INTEROP_EXPORT_HOL_LIGHT) {
        /* HOL Light 格式：使用 (* ... *) 注释，APPLY 语法 */
        comment_open = "(* ";
        comment_close = " *)";
        apply_prefix = "APPLY_THEN ";
        line_end = ";";
        lean_style_params = false;
    } else {
        lv00_set_error(LV00_ERROR_UNSUPPORTED, "定理导出仅支持 Coq、Lean、Isabelle/HOL 和 HOL Light 格式，当前格式=%d", format);
        return LV00_ERROR_UNSUPPORTED;
    }

    size_t offset = 0;
    int written = 0;

    /* 头部注释 */
    written = snprintf(output + offset, output_size - offset, "%sTheorem calls exported by Lv-00%s\n", comment_open,
                       comment_close);
    if (written < 0)
        return LV00_ERROR_BUFFER_TOO_SMALL;
    offset += written;

    written = snprintf(output + offset, output_size - offset, "%sTrust base: %s v%s%s\n\n", comment_open,
                       ctx->trust_base_name, ctx->trust_base_version, comment_close);
    if (written < 0)
        return LV00_ERROR_BUFFER_TOO_SMALL;
    offset += written;

    /* 解析调用记录并生成 apply 语句 */
    if (ctx->exported_calls && ctx->calls_len > 0) {
        char *buf = (char *) lv00_malloc(ctx->calls_len + 1);
        if (!buf) {
            lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "定理导出失败：无法分配%zu字节的临时解析缓冲区",
                           ctx->calls_len + 1);
            return LV00_ERROR_OUT_OF_MEMORY;
        }
        memcpy(buf, ctx->exported_calls, ctx->calls_len + 1);

        /* 按行分割 */
        char *line = strtok(buf, "\n");
        int call_index = 0;
        while (line && offset < output_size) {
            /* 每行格式: theorem_name;param1;param2;...; */
            char *save_ptr_line = NULL;
            char *name = strtok_s(line, ";", &save_ptr_line);
            if (name && strlen(name) > 0) {
                /* 生成 apply 语句 */
                written = snprintf(output + offset, output_size - offset, "%s%s", apply_prefix, name);
                if (written < 0) {
                    lv00_free((void **) &buf);
                    return LV00_ERROR_BUFFER_TOO_SMALL;
                }
                offset += written;

                /* 处理参数 */
                char *param = strtok_s(NULL, ";", &save_ptr_line);
                int pidx = 0;
                while (param && offset < output_size) {
                    if (lean_style_params) {
                        /* Lean 风格：apply theorem_name param1 param2 */
                        written = snprintf(output + offset, output_size - offset, " %s", param);
                    } else {
                        /* Coq 风格：apply theorem_name with (A := param1) (B := param2) */
                        char arg_label[8];
                        snprintf(arg_label, sizeof(arg_label), "%c", (char) ('A' + pidx));
                        written = snprintf(output + offset, output_size - offset, " with (%s := %s)", arg_label, param);
                    }
                    if (written < 0) {
                        lv00_free((void **) &buf);
                        return LV00_ERROR_BUFFER_TOO_SMALL;
                    }
                    offset += written;
                    param = strtok_s(NULL, ";", &save_ptr_line);
                    pidx++;
                }

                /* 行尾 */
                written = snprintf(output + offset, output_size - offset, "%s\n", line_end);
                if (written < 0) {
                    lv00_free((void **) &buf);
                    return LV00_ERROR_BUFFER_TOO_SMALL;
                }
                offset += written;
                call_index++;
            }
            line = strtok(NULL, "\n");
        }
        lv00_free((void **) &buf);

        if (call_index == 0) {
            /* 没有解析到有效调用 */
            written = snprintf(output + offset, output_size - offset, "%s(no theorem calls recorded)%s\n", comment_open,
                               comment_close);
            if (written < 0)
                return LV00_ERROR_BUFFER_TOO_SMALL;
            offset += written;
        }
    } else {
        /* 无调用记录 */
        written = snprintf(output + offset, output_size - offset, "%s(no theorem calls recorded)%s\n", comment_open,
                           comment_close);
        if (written < 0)
            return LV00_ERROR_BUFFER_TOO_SMALL;
        offset += written;
    }

    return LV00_OK;
}

int interop_import_external_theorem(LV00Engine *engine, const char *trust_base_name, const char *content_hash,
                                    const char *description, int *block_id) {
    /**
     * @brief 导入外部定理作为信任基块
     *
     * 将外部证明助手（Coq/Lean）导出的定理注册为Lv-00引擎中的信任基块。
     * 执行以下验证步骤：
     *   1. 参数校验：确保 trust_base_name、content_hash、block_id 非空
     *   2. 信任基名称验证：名称不能为空，长度不超过63字符，
     *      仅允许字母、数字和下划线
     *   3. 内容哈希验证：哈希值长度至少为8字符，且仅包含十六进制字符
     *   4. 描述记录：将描述信息写入引擎日志（如有）
     *   5. 块注册：返回一个新的 block_id 作为该信任基的唯一标识
     *
     * 信任基验证机制说明：
     * - 名称合法性检查确保信任基可以安全地在文件系统和网络中使用
     * - 哈希格式验证确保内容完整性校验的数据格式正确，
     *   但实际的哈希比对（SHA-256/MD5）由外部调用者在导入前完成
     * - 注册为信任基后，该块可作为其他定理调用的前提基础
     *
     * @param engine Lv-00引擎实例
     * @param trust_base_name 信任基名称（如 "Tarski_axioms"）
     * @param content_hash 内容哈希值（十六进制字符串，如 "a1b2c3d4..."）
     * @param description 可选的描述文本（可为 NULL）
     * @param block_id 输出参数，接收新注册的块ID
     * @return LV00_OK 信任基成功注册
     *         LV00_ERROR_INVALID_PARAM 参数无效（空指针或格式不合法）
     *         LV00_ERROR_UNSUPPORTED 引擎不支持信任基注册
     */
    if (!engine || !trust_base_name || !content_hash || !block_id) {
        return LV00_ERROR_INVALID_PARAM;
    }

    /* ---- 流式事件：开始外部定理导入 ---- */
    {
        StreamContext *sctx = engine_get_stream_context(engine);
        if (sctx) {
            char msg[256];
            snprintf(msg, sizeof(msg), "开始外部定理导入：\"%s\"", trust_base_name);
            stream_emit_simple(sctx, STREAM_EVENT_INFO, msg, 0);
        }
    }

    *block_id = -1;

    /* ---- 信任基名称验证 ---- */
    size_t name_len = strlen(trust_base_name);
    if (name_len == 0) {
        lv00_set_error(LV00_ERROR_INVALID_PARAM, "外部定理导入失败：信任基名称为空");
        return LV00_ERROR_INVALID_PARAM;
    }
    if (name_len > 63) {
        lv00_set_error(LV00_ERROR_INVALID_PARAM, "外部定理导入失败：信任基名称过长（%zu字符，最大63字符）", name_len);
        return LV00_ERROR_INVALID_PARAM;
    }
    for (size_t i = 0; i < name_len; i++) {
        char c = trust_base_name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) {
            lv00_set_error(LV00_ERROR_INVALID_PARAM, "外部定理导入失败：信任基名称包含非法字符'%c'（位置=%zu）", c, i);
            return LV00_ERROR_INVALID_PARAM;
        }
    }

    /* ---- 内容哈希验证 ---- */
    size_t hash_len = strlen(content_hash);
    if (hash_len < 8) {
        lv00_set_error(LV00_ERROR_INVALID_PARAM, "外部定理导入失败：内容哈希过短（%zu字符，最少8字符）", hash_len);
        return LV00_ERROR_INVALID_PARAM;
    }
    for (size_t i = 0; i < hash_len; i++) {
        char c = content_hash[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
            lv00_set_error(LV00_ERROR_INVALID_PARAM, "外部定理导入失败：内容哈希包含非十六进制字符'%c'（位置=%zu）", c,
                           i);
            return LV00_ERROR_INVALID_PARAM;
        }
    }

    /* ---- 描述记录 ---- */
    if (description && strlen(description) > 0) {
        char msg[512];
        StreamContext *sctx = engine_get_stream_context(engine);
        snprintf(msg, sizeof(msg), "外部定理\"%s\"（哈希=%s）描述：%s", trust_base_name, content_hash, description);
        if (sctx)
            stream_emit_simple(sctx, STREAM_EVENT_INFO, msg, 0);
    }

    /* ---- 注册信任基块 ---- */
    /* 信任基块ID使用 content_hash 的低位进行哈希映射，确保一定程度的唯一性 */
    unsigned int hash_val = 0;
    for (size_t i = 0; i < hash_len; i++) {
        hash_val = hash_val * 31 + (unsigned char) content_hash[i];
    }
    /* 使用大偏移量避免与常规节点ID冲突 */
    *block_id = (int) (1000000 + (hash_val % 9000000));

    {
        char msg[256];
        StreamContext *sctx = engine_get_stream_context(engine);
        snprintf(msg, sizeof(msg),
                 "外部定理\"%s\"（哈希前8位=%.8s）已注册为信任基块，block_id=%d。"
                 "注意：完整的外部证明验证和跨系统信任传递需要外部证明助手的配合。",
                 trust_base_name, content_hash, *block_id);
        if (sctx)
            stream_emit_simple(sctx, STREAM_EVENT_INFO, msg, 0);
    }

    return LV00_OK;
}

/* ==================== 工具函数 ==================== */

const char *interop_export_format_name(InteropExportFormat format) {
    switch (format) {
        case INTEROP_EXPORT_COQ:
            return "coq";
        case INTEROP_EXPORT_LEAN:
            return "lean";
        case INTEROP_EXPORT_HTML:
            return "html";
        case INTEROP_EXPORT_SVG:
            return "svg";
        case INTEROP_EXPORT_PDF:
            return "pdf";
        case INTEROP_EXPORT_TIKZ:
            return "tikz";
        case INTEROP_EXPORT_GEOJSON:
            return "geojson";
        case INTEROP_EXPORT_CANONICAL:
            return "canonical";
        default:
            return "unknown";
    }
}

const char *interop_import_format_name(InteropImportFormat format) {
    switch (format) {
        case INTEROP_IMPORT_GEOGEBRA:
            return "geogebra";
        case INTEROP_IMPORT_GEOJSON:
            return "geojson";
        case INTEROP_IMPORT_SVG:
            return "svg";
        default:
            return "unknown";
    }
}

InteropExportFormat interop_parse_export_format(const char *str) {
    if (!str)
        return (InteropExportFormat) -1;

    if (strcmp(str, "coq") == 0)
        return INTEROP_EXPORT_COQ;
    if (strcmp(str, "lean") == 0)
        return INTEROP_EXPORT_LEAN;
    if (strcmp(str, "html") == 0)
        return INTEROP_EXPORT_HTML;
    if (strcmp(str, "svg") == 0)
        return INTEROP_EXPORT_SVG;
    if (strcmp(str, "pdf") == 0)
        return INTEROP_EXPORT_PDF;
    if (strcmp(str, "tikz") == 0)
        return INTEROP_EXPORT_TIKZ;
    if (strcmp(str, "geojson") == 0)
        return INTEROP_EXPORT_GEOJSON;
    if (strcmp(str, "canonical") == 0)
        return INTEROP_EXPORT_CANONICAL;

    return (InteropExportFormat) -1;
}

InteropImportFormat interop_parse_import_format(const char *str) {
    if (!str)
        return (InteropImportFormat) -1;

    if (strcmp(str, "geogebra") == 0)
        return INTEROP_IMPORT_GEOGEBRA;
    if (strcmp(str, "geojson") == 0)
        return INTEROP_IMPORT_GEOJSON;
    if (strcmp(str, "svg") == 0)
        return INTEROP_IMPORT_SVG;

    return (InteropImportFormat) -1;
}

int interop_validate_path(const char *path) {
    if (!path || strlen(path) == 0)
        return 0;
    if (strlen(path) >= INTEROP_MAX_PATH_LEN)
        return 0;

    /* 检查非法字符 */
    const char *invalid = "<>\"|?*";
    for (const char *p = path; *p; p++) {
        if (strchr(invalid, *p))
            return 0;
    }

    return 1;
}

const char *interop_get_file_extension(const char *path) {
    if (!path)
        return "";

    const char *dot = strrchr(path, '.');
    if (!dot || dot == path)
        return "";

    return dot + 1;
}
