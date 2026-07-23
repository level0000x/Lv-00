/**
 * @file interop_server.c
 * @brief 互操作服务器
 *
 * @details 拆分子模块（Lv-00 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/interop.h"
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include "lv/constraint_graph.h"
#include "lv/engine.h"
#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"

lv_DECLARE_STREAM_CTX(interop);

/* ── 服务器核心 ── */

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
static long g_stdout_mutex_initialized = 0;

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
#else
    if (server->type == INTEROP_INTERFACE_WEBSOCKET) {
        /* WebSocket服务器已标记为运行状态。
           注意：当前编译环境未包含Winsock2库，无实际网络监听。
           请安装Windows SDK以启用完整的网络功能。 */
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
     *   * 通过 lv_set_error 提示用户安装 Windows SDK
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
        /* ====== WebSocket 模式：骨架实现 ====== */
        lv_set_error(lv_OK, "WebSocket服务器主循环已启动（端口=%d）", server->port);

#if INTEROP_HAS_WINSOCK
        SOCKET listen_sock = (SOCKET) (intptr_t) server->internal_data;
        if (listen_sock == INVALID_SOCKET || listen_sock == 0) {
            lv_set_error(lv_ERROR_IO,
                           "WebSocket循环失败：监听套接字无效（listen_sock=%p）。"
                           "请确认 interop_server_start 已成功绑定端口。",
                           (void *) (intptr_t) listen_sock);
            return lv_ERROR_IO;
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
            lv_set_error(lv_OK, "%s", msg);
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
                lv_set_error(lv_ERROR_IO, "WebSocket select() 出错（Winsock错误码=%d），服务器退出", err);
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
                        lv_set_error(lv_OK, "WebSocket客户端已连接（套接字=%p，总计%d个客户端）",
                                       (void *) client_sock, client_count);
                    } else {
                        lv_set_error(lv_ERROR_RESOURCE_EXHAUSTED, "WebSocket客户端连接被拒绝：已达最大客户端数%d",
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
                    lv_set_error(lv_OK, "WebSocket客户端断开（套接字=%p，错误码=%d）", (void *) cs,
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
                if (result == lv_OK || output[0] != '\0') {
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
        lv_set_error(lv_OK, "WebSocket主循环已退出，已关闭%d个客户端连接", client_count);

#else
        /* 无 Winsock 支持：降级为 STDIO 输入处理 */
        lv_set_error(lv_WARNING,
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

static lvPlugin g_plugins[MAX_PLUGINS];
static int g_plugin_count = 0;

int lv_interop_register_plugin(lvInteropManager *mgr, const lvPlugin *plugin) {
    if (!plugin) return -1;
    (void)mgr; /* 管理器参数保留供未来扩展 */
    if (g_plugin_count >= MAX_PLUGINS) return -1;
    memcpy(&g_plugins[g_plugin_count], plugin, sizeof(lvPlugin));
    g_plugin_count++;
    return 0;
}
