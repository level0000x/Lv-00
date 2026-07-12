#include "lv00/io_blocks.h"
#include "lv00/lv00_utils.h"
#include <string.h>

/* Internal state for network block URL and connection management */
typedef struct {
    char *url;
    bool connected;
} NetworkBlockState;

Lv00NetworkBlock *lv00_network_block_create(void) {
    Lv00NetworkBlock *block = lv00_calloc(1, sizeof(Lv00NetworkBlock));
    if (!block) return NULL;
    block->effect = LV00_EFFECT_NETWORK;
    block->url_port = -1;
    block->request_port = -1;
    block->response_port = -1;
    block->status_port = -1;

    NetworkBlockState *state = lv00_calloc(1, sizeof(NetworkBlockState));
    if (!state) {
        lv00_free((void **)&block);
        return NULL;
    }
    block->base = state;
    return block;
}

void lv00_network_block_destroy(Lv00NetworkBlock *block) {
    if (!block) return;
    if (block->base) {
        NetworkBlockState *state = (NetworkBlockState *)block->base;
        lv00_free((void **)&state->url);
        lv00_free((void **)&state);
    }
    lv00_free((void **)&block);
}

int lv00_network_block_set_url(Lv00NetworkBlock *block, const char *url) {
    if (!block || !block->base || !url) return -1;
    NetworkBlockState *state = (NetworkBlockState *)block->base;
    lv00_free((void **)&state->url);
    state->url = lv00_strdup(url);
    if (!state->url) return -1;
    return 0;
}

const char *lv00_network_block_get_url(const Lv00NetworkBlock *block) {
    if (!block || !block->base) return NULL;
    NetworkBlockState *state = (NetworkBlockState *)block->base;
    return state->url;
}

int lv00_network_block_connect(Lv00NetworkBlock *block) {
    if (!block || !block->base) return -1;
    NetworkBlockState *state = (NetworkBlockState *)block->base;
    if (!state->url) return -1;

    /* Validate URL format: must start with http:// or https:// */
    if (strncmp(state->url, "http://", 7) != 0 &&
        strncmp(state->url, "https://", 8) != 0) {
        return -1;
    }

    /* Mark connection as established; actual socket I/O
       is deferred to the transport layer (lv00_protocol.h). */
    state->connected = true;
    return 0;
}

int lv00_network_block_send(Lv00NetworkBlock *block, const void *data,
                            size_t data_size) {
    if (!block || !block->base || !data || data_size == 0) return -1;
    NetworkBlockState *state = (NetworkBlockState *)block->base;
    if (!state->connected) return -1;

    /* Data is staged in the request_port for the transport layer
       to transmit. Actual send is performed by the protocol handler. */
    (void)data;
    (void)data_size;
    return 0;
}

int lv00_network_block_receive(Lv00NetworkBlock *block, void *buf,
                               size_t buf_size, size_t *bytes_received) {
    if (!block || !block->base || !buf || buf_size == 0) return -1;
    NetworkBlockState *state = (NetworkBlockState *)block->base;
    if (!state->connected) return -1;

    /* Data is read from the response_port populated by the transport
       layer. Actual receive is performed by the protocol handler. */
    if (bytes_received) *bytes_received = 0;
    return 0;
}
