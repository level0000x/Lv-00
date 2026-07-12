#include "lv00/io_blocks.h"
#include "lv00/lv00_utils.h"
#include <string.h>
#include <stdio.h>

/* Internal state for file block path management */
typedef struct {
    char *path;
    bool is_open;
} FileBlockState;

Lv00FileBlock *lv00_file_block_create(Lv00EffectType effect) {
    Lv00FileBlock *block = lv00_calloc(1, sizeof(Lv00FileBlock));
    if (!block) return NULL;
    block->effect = effect;
    block->path_port = -1;
    block->data_port = -1;
    block->result_port = -1;
    block->status_port = -1;

    FileBlockState *state = lv00_calloc(1, sizeof(FileBlockState));
    if (!state) {
        lv00_free((void **)&block);
        return NULL;
    }
    block->base = state;
    return block;
}

void lv00_file_block_destroy(Lv00FileBlock *block) {
    if (!block) return;
    if (block->base) {
        FileBlockState *state = (FileBlockState *)block->base;
        lv00_free((void **)&state->path);
        lv00_free((void **)&state);
    }
    lv00_free((void **)&block);
}

int lv00_file_block_set_path(Lv00FileBlock *block, const char *path) {
    if (!block || !block->base || !path) return -1;
    FileBlockState *state = (FileBlockState *)block->base;
    lv00_free((void **)&state->path);
    state->path = lv00_strdup(path);
    if (!state->path) return -1;
    return 0;
}

const char *lv00_file_block_get_path(const Lv00FileBlock *block) {
    if (!block || !block->base) return NULL;
    FileBlockState *state = (FileBlockState *)block->base;
    return state->path;
}

int lv00_file_block_read(Lv00FileBlock *block, void *buf, size_t buf_size,
                         size_t *bytes_read) {
    if (!block || !block->base || !buf || buf_size == 0) return -1;
    FileBlockState *state = (FileBlockState *)block->base;
    if (!state->path) return -1;

    FILE *f = fopen(state->path, "rb");
    if (!f) {
        if (bytes_read) *bytes_read = 0;
        return -1;
    }
    size_t n = fread(buf, 1, buf_size, f);
    fclose(f);

    if (bytes_read) *bytes_read = n;
    state->is_open = false;
    return (n > 0) ? 0 : -1;
}

int lv00_file_block_write(Lv00FileBlock *block, const void *data,
                          size_t data_size) {
    if (!block || !block->base || !data) return -1;
    FileBlockState *state = (FileBlockState *)block->base;
    if (!state->path) return -1;

    FILE *f = fopen(state->path, "wb");
    if (!f) return -1;
    size_t n = fwrite(data, 1, data_size, f);
    fclose(f);

    state->is_open = false;
    return (n == data_size) ? 0 : -1;
}

bool lv00_file_block_is_open(const Lv00FileBlock *block) {
    if (!block || !block->base) return false;
    FileBlockState *state = (FileBlockState *)block->base;
    return state->is_open;
}
