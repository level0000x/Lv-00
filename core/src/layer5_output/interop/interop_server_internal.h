#ifndef INTEROP_SERVER_INTERNAL_H
#define INTEROP_SERVER_INTERNAL_H

#include <stdint.h>

#include "lv/interop.h" /* InteropServer 结构 */

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
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#define INTEROP_HAS_POSIX_SOCKET 1
#else
#define INTEROP_HAS_WINSOCK 0
#endif

/** 套接字句柄（Winsock 的 SOCKET 与 POSIX 的 int 统一为 intptr_t） */
typedef intptr_t WsSock;

/* 定义在 interop_server_ws.c（WebSocket 主循环，RFC 6455） */
int interop_ws_run(InteropServer *server, WsSock listen_sock);

#endif /* INTEROP_SERVER_INTERNAL_H */
