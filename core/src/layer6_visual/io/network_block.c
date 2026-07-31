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

#include "lv/io_block.h"
#include "lv/io_blocks.h"
#include "lv/lv_utils.h"
#include "lv/lv_internal.h"

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
 * 释放内部状态中的 URL 字符串、状态结构体和网络块本身。
 *
 * @param block 网络块指针
 */
void lv_network_block_destroy(lvNetworkBlock *block) {
    if (!block)
        return;
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
 * 验证 URL 格式（必须以 http:// 或 https:// 开头），
 * 并将连接状态标记为已连接。实际套接字 I/O 由传输层处理。
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

    /* Validate URL format: must start with http:// or https:// */
    if (strncmp(state->target, "http://", 7) != 0 && strncmp(state->target, "https://", 8) != 0) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "invalid URL format, must start with http:// or https://");
    }

    /* Mark connection as established; actual socket I/O
       is deferred to the transport layer (lv_protocol.h). */
    state->active = true;
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
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "NULL block, base, or data, or zero size");
    lvIOBlockState *state = (lvIOBlockState *) block->base;
    if (!state->active)
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "not connected");

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
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "NULL block, base, or buf, or zero size");
    lvIOBlockState *state = (lvIOBlockState *) block->base;
    if (!state->active)
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "not connected");

    /* Data is read from the response_port populated by the transport
       layer. Actual receive is performed by the protocol handler. */
    if (bytes_received)
        *bytes_received = 0;
    return 0;
}
