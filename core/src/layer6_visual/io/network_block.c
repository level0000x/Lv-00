/**
 * @file network_block.c
 * @brief 网络块实现
 *
 * @details 实现网络通信块的创建、销毁、连接管理以及基于平台 socket 的
 *          真实数据收发。连接信息（URL/host/port）保存在 lvIOBlockState 中，
 *          socket 句柄通过文件内静态句柄表按 block 指针关联，避免侵入
 *          lvNetworkBlock 的公共字段语义。
 *          当前仅支持 http 明文连接；https/TLS 不在本实现范围。
 *
 * @author Lv-00 Project
 */

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET lvNetSocket;
#define LV_NET_SOCKET_INVALID INVALID_SOCKET
#define LV_NET_IS_INVALID(s) ((s) == INVALID_SOCKET)
#define LV_NET_CLOSE(s) closesocket(s)
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
typedef int lvNetSocket;
#define LV_NET_SOCKET_INVALID (-1)
#define LV_NET_IS_INVALID(s) ((s) < 0)
#define LV_NET_CLOSE(s) close(s)
#endif

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include "lv/io_block.h"
#include "lv/io_blocks.h"
#include "lv/lv_utils.h"
#include "lv/lv_internal.h"

#define LV_NETWORK_MAX_CONNECTIONS 16

/**
 * @brief 内部句柄表条目：将 lvNetworkBlock 指针关联到平台 socket 句柄
 */
typedef struct {
    lvNetworkBlock *block;
    lvNetSocket fd;
    int in_use;
} lvNetworkHandle;

static lvNetworkHandle g_network_handles[LV_NETWORK_MAX_CONNECTIONS];

#ifdef _WIN32
static WSADATA g_network_wsa_data;
static int g_network_wsa_count = 0;
#endif

/**
 * @brief 按 block 指针查找已建立的 socket 句柄
 */
static lvNetSocket lv_network_find_handle(lvNetworkBlock *block) {
    int i;
    for (i = 0; i < LV_NETWORK_MAX_CONNECTIONS; i++) {
        if (g_network_handles[i].in_use && g_network_handles[i].block == block)
            return g_network_handles[i].fd;
    }
    return LV_NET_SOCKET_INVALID;
}

/**
 * @brief 将新建立的 socket 句柄登记到句柄表
 */
static int lv_network_store_handle(lvNetworkBlock *block, lvNetSocket fd) {
    int i;
    for (i = 0; i < LV_NETWORK_MAX_CONNECTIONS; i++) {
        if (!g_network_handles[i].in_use) {
            g_network_handles[i].block = block;
            g_network_handles[i].fd = fd;
            g_network_handles[i].in_use = 1;
            return 0;
        }
    }
    lv_RETURN_ERROR(lv_ERROR_RESOURCE_EXHAUSTED, "网络句柄表已满");
}

/**
 * @brief 从句柄表移除 block 对应的条目
 */
static void lv_network_remove_handle(lvNetworkBlock *block) {
    int i;
    for (i = 0; i < LV_NETWORK_MAX_CONNECTIONS; i++) {
        if (g_network_handles[i].in_use && g_network_handles[i].block == block) {
            g_network_handles[i].in_use = 0;
            g_network_handles[i].block = NULL;
            g_network_handles[i].fd = LV_NET_SOCKET_INVALID;
            return;
        }
    }
}

/**
 * @brief 递减 Winsock 引用计数，计数归零时调用 WSACleanup
 */
#ifdef _WIN32
static void lv_network_wsa_release(void) {
    if (g_network_wsa_count > 0) {
        g_network_wsa_count--;
        if (g_network_wsa_count == 0)
            WSACleanup();
    }
}
#endif

/**
 * @brief 创建网络块
 *
 * 分配并初始化一个网络块，包含内部状态管理。
 * 初始状态未连接，各端口设为 -1（未分配）。
 *
 * @return 成功返回网络块指针，失败返回NULL
 */
lvNetworkBlock *lv_network_block_create(void) {
    lvNetworkBlock *block = lv_calloc(1, sizeof(lvNetworkBlock));
    if (!block)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "failed to allocate network block");
    block->effect = lv_EFFECT_NETWORK;
    block->url_port = -1;
    block->request_port = -1;
    block->response_port = -1;
    block->status_port = -1;

    lvIOBlockState *state = lv_calloc(1, sizeof(lvIOBlockState));
    if (!state) {
        lv_free((void **) &block);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "failed to allocate network block state");
    }
    block->base = state;
    return block;
}

/**
 * @brief 销毁网络块
 *
 * 关闭已建立的 socket 连接、释放内部状态中的 URL 字符串、
 * 状态结构体和网络块本身。
 *
 * @param block 网络块指针
 */
void lv_network_block_destroy(lvNetworkBlock *block) {
    if (!block)
        return;
    lvNetSocket fd = lv_network_find_handle(block);
    if (!LV_NET_IS_INVALID(fd)) {
        LV_NET_CLOSE(fd);
#ifdef _WIN32
        lv_network_wsa_release();
#endif
    }
    lv_network_remove_handle(block);
    if (block->base) {
        lvIOBlockState *state = (lvIOBlockState *) block->base;
        lv_free((void **) &state->target);
        lv_free((void **) &state);
    }
    lv_free((void **) &block);
}

/**
 * @brief 设置目标 URL
 *
 * 设置网络块的目标 URL，自动释放旧的 URL 并复制新字符串。
 *
 * @param block 网络块指针
 * @param url   目标 URL 字符串
 * @return 成功返回0，失败返回-1
 */
int lv_network_block_set_url(lvNetworkBlock *block, const char *url) {
    if (!block || !block->base || !url)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "NULL block, base, or url");
    lvIOBlockState *state = (lvIOBlockState *) block->base;
    lv_free((void **) &state->target);
    state->target = lv_strdup(url);
    if (!state->target)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "failed to strdup url");
    return 0;
}

/**
 * @brief 获取目标 URL
 *
 * @param block 网络块指针（const）
 * @return URL 字符串，参数无效时返回NULL
 */
const char *lv_network_block_get_url(const lvNetworkBlock *block) {
    if (!block || !block->base)
        return NULL;
    lvIOBlockState *state = (lvIOBlockState *) block->base;
    return state->target;
}

/**
 * @brief 建立连接
 *
 * 解析 URL 中的 host 与端口（http 默认 80，https 默认 443），
 * 通过 getaddrinfo + socket + connect 建立真实的 TCP 连接。
 * 仅支持 http 明文；https 需要 TLS 加密，超出当前实现范围。
 * 连接成功后把 socket 句柄登记到内部句柄表并置 active 状态。
 *
 * @param block 网络块指针
 * @return 成功返回0，失败返回-1
 */
int lv_network_block_connect(lvNetworkBlock *block) {
    if (!block || !block->base)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "NULL block or base");
    lvIOBlockState *state = (lvIOBlockState *) block->base;
    if (!state->target)
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "URL not set before connect");
    if (state->active)
        return 0;

    const char *url = state->target;
    const char *host = NULL;
    int use_https = 0;
    if (lv_str_startswith(url, "http://")) {
        host = url + 7;
    } else if (lv_str_startswith(url, "https://")) {
        use_https = 1;
        host = url + 8;
    } else {
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "invalid URL format, must start with http:// or https://");
    }
    if (use_https)
        lv_RETURN_ERROR(lv_ERROR_UNSUPPORTED, "https/TLS 不在当前实现范围，仅支持 http 明文连接");
    if (!*host)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "URL 缺少 host");

    char host_buf[512];
    size_t i = 0;
    while (host[i] && host[i] != ':' && host[i] != '/' && i < sizeof(host_buf) - 1) {
        host_buf[i] = host[i];
        i++;
    }
    host_buf[i] = '\0';
    if (i == 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "URL 缺少 host");

    int port = 80;
    if (host[i] == ':') {
        long p = strtol(host + i + 1, NULL, 10);
        if (p <= 0 || p > 65535)
            lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "URL 端口无效");
        port = (int) p;
    }

#ifdef _WIN32
    if (g_network_wsa_count == 0) {
        if (WSAStartup(MAKEWORD(2, 2), &g_network_wsa_data) != 0)
            lv_RETURN_ERROR(lv_ERROR_IO, "WSAStartup failed");
    }
    g_network_wsa_count++;
#endif

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo *res = NULL;
    int ga_ret = getaddrinfo(host_buf, port_str, &hints, &res);
    if (ga_ret != 0) {
#ifdef _WIN32
        lv_network_wsa_release();
#endif
        lv_RETURN_ERROR(lv_ERROR_IO, "getaddrinfo failed for host %s", host_buf);
    }

    lvNetSocket fd = LV_NET_SOCKET_INVALID;
    struct addrinfo *rp;
    for (rp = res; rp; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (LV_NET_IS_INVALID(fd))
            continue;
        if (connect(fd, rp->ai_addr, (int) rp->ai_addrlen) == 0)
            break;
        LV_NET_CLOSE(fd);
        fd = LV_NET_SOCKET_INVALID;
    }
    freeaddrinfo(res);

    if (LV_NET_IS_INVALID(fd)) {
#ifdef _WIN32
        lv_network_wsa_release();
#endif
        lv_RETURN_ERROR(lv_ERROR_IO, "socket/connect failed for %s:%d", host_buf, port);
    }

    if (lv_network_store_handle(block, fd) != 0) {
        LV_NET_CLOSE(fd);
#ifdef _WIN32
        lv_network_wsa_release();
#endif
        lv_RETURN_ERROR(lv_ERROR_RESOURCE_EXHAUSTED, "网络连接句柄表已满");
    }

    state->active = true;
    return 0;
}

/**
 * @brief 发送数据
 *
 * 将 data 通过已建立的 socket 完整发送（处理部分发送与中断重试）。
 *
 * @param block     网络块指针
 * @param data      待发送数据缓冲区
 * @param data_size 数据大小
 * @return 成功返回0，失败返回-1
 */
int lv_network_block_send(lvNetworkBlock *block, const void *data, size_t data_size) {
    if (!block || !block->base || !data || data_size == 0)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "NULL block, base, or data, or zero size");
    lvIOBlockState *state = (lvIOBlockState *) block->base;
    if (!state->active)
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "not connected");

    lvNetSocket fd = lv_network_find_handle(block);
    if (LV_NET_IS_INVALID(fd))
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "not connected");

    const char *p = (const char *) data;
    size_t remaining = data_size;
    while (remaining > 0) {
        size_t chunk = remaining;
#ifdef _WIN32
        int n;
        if (chunk > (size_t) INT_MAX)
            chunk = (size_t) INT_MAX;
        n = send(fd, p, (int) chunk, 0);
        if (n == SOCKET_ERROR) {
            if (WSAGetLastError() == WSAEINTR)
                continue;
            lv_RETURN_ERROR(lv_ERROR_IO, "send failed");
        }
#else
        ssize_t n;
        n = send(fd, p, chunk, 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            lv_RETURN_ERROR(lv_ERROR_IO, "send failed");
        }
#endif
        p += (size_t) n;
        remaining -= (size_t) n;
    }
    return 0;
}

/**
 * @brief 接收数据
 *
 * 从已建立的 socket 读取数据到 buf，直到收到数据或对端关闭（EOF），
 * 输出实际接收字节数。
 *
 * @param block          网络块指针
 * @param buf            接收缓冲区
 * @param buf_size       缓冲区大小
 * @param bytes_received 输出实际接收字节数（可为NULL）
 * @return 成功返回0，失败返回-1
 */
int lv_network_block_receive(lvNetworkBlock *block, void *buf, size_t buf_size, size_t *bytes_received) {
    if (!block || !block->base || !buf || buf_size == 0)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "NULL block, base, or buf, or zero size");
    lvIOBlockState *state = (lvIOBlockState *) block->base;
    if (!state->active)
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "not connected");

    lvNetSocket fd = lv_network_find_handle(block);
    if (LV_NET_IS_INVALID(fd))
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "not connected");

    size_t chunk = buf_size;
#ifdef _WIN32
    int n;
    if (chunk > (size_t) INT_MAX)
        chunk = (size_t) INT_MAX;
    n = recv(fd, (char *) buf, (int) chunk, 0);
    if (n == SOCKET_ERROR) {
        if (WSAGetLastError() == WSAEINTR)
            n = recv(fd, (char *) buf, (int) chunk, 0);
        if (n == SOCKET_ERROR)
            lv_RETURN_ERROR(lv_ERROR_IO, "recv failed");
    }
#else
    ssize_t n;
    n = recv(fd, buf, chunk, 0);
    if (n < 0) {
        if (errno == EINTR)
            n = recv(fd, buf, chunk, 0);
        if (n < 0)
            lv_RETURN_ERROR(lv_ERROR_IO, "recv failed");
    }
#endif
    if (bytes_received)
        *bytes_received = (size_t) n;
    return 0;
}
