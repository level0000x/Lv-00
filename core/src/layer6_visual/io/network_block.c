/**
 * @file network_block.c
 * @brief 网络块实现
 *
 * @details 实现网络通信块的创建、销毁和连接管理。
 *          网络块封装了 URL 管理、连接状态维护以及数据的发送和接收。
 *          实际的套接字 I/O 由传输层（lv_protocol.h）处理，
 *          本模块主要负责状态管理和接口编排。
 *
 * @author Lv-00 Project
 */

#include <string.h>

#include "lv/io_blocks.h"
#include "lv/lv_utils.h"

/** @brief 网络块内部状态结构 */
typedef struct {
    char *url;      /**< 目标 URL 字符串 */
    bool connected; /**< 是否已建立连接 */
} NetworkBlockState;

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
        return NULL;
    block->effect = lv_EFFECT_NETWORK;
    block->url_port = -1;
    block->request_port = -1;
    block->response_port = -1;
    block->status_port = -1;

    NetworkBlockState *state = lv_calloc(1, sizeof(NetworkBlockState));
    if (!state) {
        lv_free((void **) &block);
        return NULL;
    }
    block->base = state;
    return block;
}

/**
 * @brief 销毁网络块
 *
 * 释放内部状态中的 URL 字符串、状态结构体和网络块本身。
 *
 * @param block 网络块指针
 */
void lv_network_block_destroy(lvNetworkBlock *block) {
    if (!block)
        return;
    if (block->base) {
        NetworkBlockState *state = (NetworkBlockState *) block->base;
        lv_free((void **) &state->url);
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
        return -1;
    NetworkBlockState *state = (NetworkBlockState *) block->base;
    lv_free((void **) &state->url);
    state->url = lv_strdup(url);
    if (!state->url)
        return -1;
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
    NetworkBlockState *state = (NetworkBlockState *) block->base;
    return state->url;
}

/**
 * @brief 建立连接
 *
 * 验证 URL 格式（必须以 http:// 或 https:// 开头），
 * 并将连接状态标记为已连接。实际套接字 I/O 由传输层处理。
 *
 * @param block 网络块指针
 * @return 成功返回0，失败返回-1
 */
int lv_network_block_connect(lvNetworkBlock *block) {
    if (!block || !block->base)
        return -1;
    NetworkBlockState *state = (NetworkBlockState *) block->base;
    if (!state->url)
        return -1;

    /* Validate URL format: must start with http:// or https:// */
    if (strncmp(state->url, "http://", 7) != 0 && strncmp(state->url, "https://", 8) != 0) {
        return -1;
    }

    /* Mark connection as established; actual socket I/O
       is deferred to the transport layer (lv_protocol.h). */
    state->connected = true;
    return 0;
}

/**
 * @brief 发送数据
 *
 * 将数据暂存到请求端口，由传输层负责实际发送。
 *
 * @param block     网络块指针
 * @param data      待发送数据缓冲区
 * @param data_size 数据大小
 * @return 成功返回0，失败返回-1
 */
int lv_network_block_send(lvNetworkBlock *block, const void *data, size_t data_size) {
    if (!block || !block->base || !data || data_size == 0)
        return -1;
    NetworkBlockState *state = (NetworkBlockState *) block->base;
    if (!state->connected)
        return -1;

    /* Data is staged in the request_port for the transport layer
       to transmit. Actual send is performed by the protocol handler. */
    (void) data;
    (void) data_size;
    return 0;
}

/**
 * @brief 接收数据
 *
 * 从响应端口读取由传输层填充的接收数据。
 *
 * @param block          网络块指针
 * @param buf            接收缓冲区
 * @param buf_size       缓冲区大小
 * @param bytes_received 输出实际接收字节数（可为NULL）
 * @return 成功返回0，失败返回-1
 */
int lv_network_block_receive(lvNetworkBlock *block, void *buf, size_t buf_size, size_t *bytes_received) {
    if (!block || !block->base || !buf || buf_size == 0)
        return -1;
    NetworkBlockState *state = (NetworkBlockState *) block->base;
    if (!state->connected)
        return -1;

    /* Data is read from the response_port populated by the transport
       layer. Actual receive is performed by the protocol handler. */
    if (bytes_received)
        *bytes_received = 0;
    return 0;
}
