/**
 * @file module_serialize_msgpack.c
 * @brief MessagePack 二进制编解码与保存/加载
 *
 * @details 从 module_serialize.c 拆分的子模块（Lv-00 项目 v3.3.0+）。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_file.h"

#include "lv/module.h"
#include "lv/module_internal.h"
#include "lv/sha256.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "module_helpers.h"

/* ================================================================== */
/*  最小化 MessagePack 编码/解码器                                     */
/* ================================================================== */

typedef enum {
    MSGPACK_NIL = 0xc0,
    MSGPACK_FALSE = 0xc2,
    MSGPACK_TRUE = 0xc3,
    MSGPACK_FIXSTR = 0xa0, /* fixstr: 101xxxxx, up to 31 bytes */
    MSGPACK_STR8 = 0xd9,
    MSGPACK_STR16 = 0xda,
    MSGPACK_STR32 = 0xdb,
    MSGPACK_BIN8 = 0xc4,
    MSGPACK_BIN16 = 0xc5,
    MSGPACK_BIN32 = 0xc6,
    MSGPACK_ARRAY16 = 0xdc,
    MSGPACK_MAP16 = 0xde,
    MSGPACK_INT8 = 0xd0,
    MSGPACK_INT16 = 0xd1,
    MSGPACK_INT32 = 0xd2,
    MSGPACK_INT64 = 0xd3,
    MSGPACK_UINT8 = 0xcc,
    MSGPACK_UINT16 = 0xcd,
    MSGPACK_UINT32 = 0xce,
    MSGPACK_UINT64 = 0xcf,
    MSGPACK_FIXINT = 0x00 /* fixint: 0xxxxxxx, 0~127 */
} MsgPackType;

/* 编码器 */
typedef struct {
    uint8_t *buffer;
    size_t capacity;
    size_t pos;
    bool error; /* 编码错误标志：ensure 失败时设置 */
} MsgPackEncoder;

/* 解码器 */
typedef struct {
    const uint8_t *data;
    size_t size;
    size_t pos;
} MsgPackDecoder;

/* ---------- 编码器辅助函数 ---------- */

static bool mp_encoder_init(MsgPackEncoder *enc, size_t initial_capacity) {
    enc->buffer = (uint8_t *) lv_calloc(initial_capacity, 1);
    if (!enc->buffer)
        return false;
    enc->capacity = initial_capacity;
    enc->pos = 0;
    enc->error = false;
    return true;
}

static bool mp_encoder_ensure(MsgPackEncoder *enc, size_t extra) {
    while (enc->pos + extra > enc->capacity) {
        size_t new_cap = enc->capacity * 2;
        uint8_t *new_buf = (uint8_t *) lv_realloc(enc->buffer, new_cap);
        if (!new_buf)
            return false;
        enc->buffer = new_buf;
        enc->capacity = new_cap;
    }
    return true;
}

static void mp_encoder_write_byte(MsgPackEncoder *enc, uint8_t b) {
    if (enc->error)
        return;
    if (!mp_encoder_ensure(enc, 1)) {
        enc->error = true;
        return;
    }
    enc->buffer[enc->pos++] = b;
}

static void mp_encoder_write_u16(MsgPackEncoder *enc, uint16_t v) {
    if (enc->error)
        return;
    if (!mp_encoder_ensure(enc, 2)) {
        enc->error = true;
        return;
    }
    enc->buffer[enc->pos++] = (uint8_t) (v >> 8);
    enc->buffer[enc->pos++] = (uint8_t) (v & 0xff);
}

static void mp_encoder_write_u32(MsgPackEncoder *enc, uint32_t v) {
    if (enc->error)
        return;
    if (!mp_encoder_ensure(enc, 4)) {
        enc->error = true;
        return;
    }
    enc->buffer[enc->pos++] = (uint8_t) (v >> 24);
    enc->buffer[enc->pos++] = (uint8_t) (v >> 16);
    enc->buffer[enc->pos++] = (uint8_t) (v >> 8);
    enc->buffer[enc->pos++] = (uint8_t) (v & 0xff);
}

static void mp_encoder_write_u64(MsgPackEncoder *enc, uint64_t v) {
    if (enc->error)
        return;
    if (!mp_encoder_ensure(enc, 8)) {
        enc->error = true;
        return;
    }
    for (int i = 7; i >= 0; i--) {
        enc->buffer[enc->pos++] = (uint8_t) ((v >> (i * 8)) & 0xff);
    }
}

static void mp_encoder_write_i16(MsgPackEncoder *enc, int16_t v) {
    mp_encoder_write_u16(enc, (uint16_t) v);
}

static void mp_encoder_write_i32(MsgPackEncoder *enc, int32_t v) {
    mp_encoder_write_u32(enc, (uint32_t) v);
}

static void mp_encoder_write_i64(MsgPackEncoder *enc, int64_t v) {
    mp_encoder_write_u64(enc, (uint64_t) v);
}

/* 编码 fixint (0~127) */
static void mp_encoder_write_fixint(MsgPackEncoder *enc, int8_t v) {
    mp_encoder_write_byte(enc, (uint8_t) v);
}

/* 编码正整数 */
static void mp_encoder_write_uint(MsgPackEncoder *enc, uint64_t v) {
    if (v <= 127) {
        mp_encoder_write_byte(enc, (uint8_t) v);
    } else if (v <= 0xff) {
        mp_encoder_write_byte(enc, MSGPACK_UINT8);
        mp_encoder_write_byte(enc, (uint8_t) v);
    } else if (v <= 0xffff) {
        mp_encoder_write_byte(enc, MSGPACK_UINT16);
        mp_encoder_write_u16(enc, (uint16_t) v);
    } else if (v <= 0xffffffffUL) {
        mp_encoder_write_byte(enc, MSGPACK_UINT32);
        mp_encoder_write_u32(enc, (uint32_t) v);
    } else {
        mp_encoder_write_byte(enc, MSGPACK_UINT64);
        mp_encoder_write_u64(enc, v);
    }
}

/* 编码负整数 */
static void mp_encoder_write_int(MsgPackEncoder *enc, int64_t v) {
    if (v >= 0) {
        mp_encoder_write_uint(enc, (uint64_t) v);
    } else if (v >= -32) {
        mp_encoder_write_byte(enc, (uint8_t) (0xe0 | (int8_t) (-1 - v)));
    } else if (v >= -128) {
        mp_encoder_write_byte(enc, MSGPACK_INT8);
        mp_encoder_write_byte(enc, (uint8_t) v);
    } else if (v >= -32768) {
        mp_encoder_write_byte(enc, MSGPACK_INT16);
        mp_encoder_write_i16(enc, (int16_t) v);
    } else if (v >= -2147483648LL) {
        mp_encoder_write_byte(enc, MSGPACK_INT32);
        mp_encoder_write_i32(enc, (int32_t) v);
    } else {
        mp_encoder_write_byte(enc, MSGPACK_INT64);
        mp_encoder_write_i64(enc, v);
    }
}

/* 编写字符串 */
static void mp_encoder_write_str(MsgPackEncoder *enc, const char *str) {
    if (!str) {
        mp_encoder_write_byte(enc, MSGPACK_NIL);
        return;
    }
    size_t len = strlen(str);
    if (len <= 31) {
        mp_encoder_write_byte(enc, (uint8_t) (MSGPACK_FIXSTR | len));
    } else if (len <= 0xff) {
        mp_encoder_write_byte(enc, MSGPACK_STR8);
        mp_encoder_write_byte(enc, (uint8_t) len);
    } else if (len <= 0xffff) {
        mp_encoder_write_byte(enc, MSGPACK_STR16);
        mp_encoder_write_u16(enc, (uint16_t) len);
    } else {
        mp_encoder_write_byte(enc, MSGPACK_STR32);
        mp_encoder_write_u32(enc, (uint32_t) len);
    }
    mp_encoder_ensure(enc, len);
    if (enc->error)
        return;
    memcpy(enc->buffer + enc->pos, str, len);
    enc->pos += len;
}

/* 编码二进制数据 */
static void mp_encoder_write_bin(MsgPackEncoder *enc, const uint8_t *data, size_t len) {
    if (enc->error)
        return;
    if (len <= 0xff) {
        mp_encoder_write_byte(enc, MSGPACK_BIN8);
        mp_encoder_write_byte(enc, (uint8_t) len);
    } else if (len <= 0xffff) {
        mp_encoder_write_byte(enc, MSGPACK_BIN16);
        mp_encoder_write_u16(enc, (uint16_t) len);
    } else {
        mp_encoder_write_byte(enc, MSGPACK_BIN32);
        mp_encoder_write_u32(enc, (uint32_t) len);
    }
    mp_encoder_ensure(enc, len);
    if (enc->error)
        return;
    memcpy(enc->buffer + enc->pos, data, len);
    enc->pos += len;
}

/* 编码数组头 */
static void mp_encoder_write_array_header(MsgPackEncoder *enc, uint16_t count) {
    if (count <= 15) {
        mp_encoder_write_byte(enc, (uint8_t) (0x90 | count));
    } else {
        mp_encoder_write_byte(enc, MSGPACK_ARRAY16);
        mp_encoder_write_u16(enc, count);
    }
}

/* 编码 map 头 */
static void mp_encoder_write_map_header(MsgPackEncoder *enc, uint16_t count) {
    if (count <= 15) {
        mp_encoder_write_byte(enc, (uint8_t) (0x80 | count));
    } else {
        mp_encoder_write_byte(enc, MSGPACK_MAP16);
        mp_encoder_write_u16(enc, count);
    }
}

static void mp_encoder_destroy(MsgPackEncoder *enc) {
    lv_free((void **) &enc->buffer);
    enc->buffer = NULL;
    enc->capacity = 0;
    enc->pos = 0;
    enc->error = false;
}

/* ---------- 解码器辅助函数 ---------- */

static bool mp_decoder_init(MsgPackDecoder *dec, const uint8_t *data, size_t size) {
    dec->data = data;
    dec->size = size;
    dec->pos = 0;
    return data != NULL && size > 0;
}

static bool mp_decoder_has_data(MsgPackDecoder *dec) {
    return dec->pos < dec->size;
}

static uint8_t mp_decoder_peek(MsgPackDecoder *dec) {
    return dec->pos < dec->size ? dec->data[dec->pos] : 0;
}

static uint8_t mp_decoder_read_byte(MsgPackDecoder *dec) {
    return dec->pos < dec->size ? dec->data[dec->pos++] : 0;
}

static uint16_t mp_decoder_read_u16(MsgPackDecoder *dec) {
    uint16_t hi = mp_decoder_read_byte(dec);
    uint16_t lo = mp_decoder_read_byte(dec);
    return (uint16_t) ((hi << 8) | lo);
}

static uint32_t mp_decoder_read_u32(MsgPackDecoder *dec) {
    uint32_t b0 = mp_decoder_read_byte(dec);
    uint32_t b1 = mp_decoder_read_byte(dec);
    uint32_t b2 = mp_decoder_read_byte(dec);
    uint32_t b3 = mp_decoder_read_byte(dec);
    return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
}

static uint64_t mp_decoder_read_u64(MsgPackDecoder *dec) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) {
        v = (v << 8) | mp_decoder_read_byte(dec);
    }
    return v;
}

static int64_t mp_decoder_read_i64(MsgPackDecoder *dec) {
    return (int64_t) mp_decoder_read_u64(dec);
}

/* 解码整数 */
static bool mp_decoder_read_int(MsgPackDecoder *dec, int64_t *out) {
    if (!mp_decoder_has_data(dec))
        return false;
    uint8_t type = mp_decoder_peek(dec);
    mp_decoder_read_byte(dec);

    if (type <= 0x7f) {
        /* fixint positive */
        *out = (int64_t) type;
    } else if (type >= 0xe0) {
        /* fixint negative */
        *out = (int64_t) (int8_t) type;
    } else if (type == MSGPACK_INT8) {
        *out = (int64_t) (int8_t) mp_decoder_read_byte(dec);
    } else if (type == MSGPACK_INT16) {
        int16_t v = (int16_t) mp_decoder_read_u16(dec);
        *out = (int64_t) v;
    } else if (type == MSGPACK_INT32) {
        int32_t v = (int32_t) mp_decoder_read_u32(dec);
        *out = (int64_t) v;
    } else if (type == MSGPACK_INT64) {
        *out = mp_decoder_read_i64(dec);
    } else if (type == MSGPACK_UINT8) {
        *out = (int64_t) mp_decoder_read_byte(dec);
    } else if (type == MSGPACK_UINT16) {
        *out = (int64_t) mp_decoder_read_u16(dec);
    } else if (type == MSGPACK_UINT32) {
        *out = (int64_t) mp_decoder_read_u32(dec);
    } else if (type == MSGPACK_UINT64) {
        *out = (int64_t) mp_decoder_read_u64(dec);
    } else {
        return false;
    }
    return true;
}

/* 解码字符串（返回 malloc 分配的字符串，调用者负责 free） */
static bool mp_decoder_read_str(MsgPackDecoder *dec, char **out) {
    if (!mp_decoder_has_data(dec))
        return false;
    uint8_t type = mp_decoder_peek(dec);
    mp_decoder_read_byte(dec);

    size_t len = 0;
    if (type >= 0xa0 && type <= 0xbf) {
        len = type & 0x1f;
    } else if (type == MSGPACK_STR8) {
        len = mp_decoder_read_byte(dec);
    } else if (type == MSGPACK_STR16) {
        len = mp_decoder_read_u16(dec);
    } else if (type == MSGPACK_STR32) {
        len = mp_decoder_read_u32(dec);
    } else {
        return false;
    }

    if (dec->pos + len > dec->size)
        return false;

    char *str = (char *) lv_calloc(len + 1, 1);
    if (!str)
        return false;
    memcpy(str, dec->data + dec->pos, len);
    str[len] = '\0';
    dec->pos += len;
    *out = str;
    return true;
}

/* 解码二进制数据 */
static bool mp_decoder_read_bin(MsgPackDecoder *dec, uint8_t **out, size_t *out_len) {
    if (!mp_decoder_has_data(dec))
        return false;
    uint8_t type = mp_decoder_peek(dec);
    mp_decoder_read_byte(dec);

    size_t len = 0;
    if (type == MSGPACK_BIN8) {
        len = mp_decoder_read_byte(dec);
    } else if (type == MSGPACK_BIN16) {
        len = mp_decoder_read_u16(dec);
    } else if (type == MSGPACK_BIN32) {
        len = mp_decoder_read_u32(dec);
    } else {
        return false;
    }

    if (dec->pos + len > dec->size)
        return false;

    uint8_t *buf = (uint8_t *) lv_calloc(len, 1);
    if (!buf)
        return false;
    memcpy(buf, dec->data + dec->pos, len);
    dec->pos += len;
    *out = buf;
    *out_len = len;
    return true;
}

/* 解码数组头 */
static bool mp_decoder_read_array_header(MsgPackDecoder *dec, uint16_t *count) {
    if (!mp_decoder_has_data(dec))
        return false;
    uint8_t type = mp_decoder_peek(dec);
    mp_decoder_read_byte(dec);

    if (type >= 0x90 && type <= 0x9f) {
        *count = type & 0x0f;
    } else if (type == MSGPACK_ARRAY16) {
        *count = mp_decoder_read_u16(dec);
    } else {
        return false;
    }
    return true;
}

/* 解码 map 头 */
static bool mp_decoder_read_map_header(MsgPackDecoder *dec, uint16_t *count) {
    if (!mp_decoder_has_data(dec))
        return false;
    uint8_t type = mp_decoder_peek(dec);
    mp_decoder_read_byte(dec);

    if (type >= 0x80 && type <= 0x8f) {
        *count = type & 0x0f;
    } else if (type == MSGPACK_MAP16) {
        *count = mp_decoder_read_u16(dec);
    } else {
        return false;
    }
    return true;
}

/* 跳过一条完整的 MessagePack 值（用于跳过未知的 map 值） */
static bool mp_decoder_skip_value(MsgPackDecoder *dec) {
    if (!mp_decoder_has_data(dec))
        return false;
    uint8_t type = mp_decoder_peek(dec);
    mp_decoder_read_byte(dec); /* consume type byte */

    if (type <= 0x7f)
        return true; /* positive fixint */
    if (type >= 0xe0)
        return true; /* negative fixint */
    if (type == 0xc0 || type == 0xc2 || type == 0xc3)
        return true; /* nil, false, true */

    if (type == 0xcc) { /* uint8 */
        return mp_decoder_has_data(dec) && (mp_decoder_read_byte(dec), true);
    }
    if (type == 0xcd) { /* uint16 */
        if (dec->pos + 2 > dec->size)
            return false;
        dec->pos += 2;
        return true;
    }
    if (type == 0xce) { /* uint32 */
        if (dec->pos + 4 > dec->size)
            return false;
        dec->pos += 4;
        return true;
    }
    if (type == 0xd0) { /* int8 */
        return mp_decoder_has_data(dec) && (mp_decoder_read_byte(dec), true);
    }
    if (type == 0xd1) { /* int16 */
        if (dec->pos + 2 > dec->size)
            return false;
        dec->pos += 2;
        return true;
    }
    if (type == 0xd2) { /* int32 */
        if (dec->pos + 4 > dec->size)
            return false;
        dec->pos += 4;
        return true;
    }
    if (type == 0xd3) { /* int64 */
        if (dec->pos + 8 > dec->size)
            return false;
        dec->pos += 8;
        return true;
    }
    if (type == 0xca || type == 0xcb) { /* float32/64 */
        uint32_t skip = (type == 0xca) ? 4 : 8;
        if (dec->pos + skip > dec->size)
            return false;
        dec->pos += skip;
        return true;
    }

    /* fixstr: 0xa0-0xbf */
    if (type >= 0xa0 && type <= 0xbf) {
        uint8_t len = type & 0x1f;
        if (dec->pos + len > dec->size)
            return false;
        dec->pos += len;
        return true;
    }
    /* str8 */
    if (type == 0xd9) {
        if (!mp_decoder_has_data(dec))
            return false;
        uint8_t len = mp_decoder_read_byte(dec);
        if (dec->pos + len > dec->size)
            return false;
        dec->pos += len;
        return true;
    }
    /* str16 */
    if (type == 0xda) {
        if (dec->pos + 2 > dec->size)
            return false;
        uint16_t len = (uint16_t) (dec->data[dec->pos] << 8 | dec->data[dec->pos + 1]);
        dec->pos += 2;
        if (dec->pos + len > dec->size)
            return false;
        dec->pos += len;
        return true;
    }
    /* str32 */
    if (type == 0xdb) {
        if (dec->pos + 4 > dec->size)
            return false;
        uint32_t len = (uint32_t) ((uint32_t) dec->data[dec->pos] << 24 | (uint32_t) dec->data[dec->pos + 1] << 16 |
                                   (uint32_t) dec->data[dec->pos + 2] << 8 | (uint32_t) dec->data[dec->pos + 3]);
        dec->pos += 4;
        if (dec->pos + len > dec->size)
            return false;
        dec->pos += len;
        return true;
    }

    /* bin8/16/32 */
    if (type == 0xc4) {
        if (!mp_decoder_has_data(dec))
            return false;
        uint8_t len = mp_decoder_read_byte(dec);
        if (dec->pos + len > dec->size)
            return false;
        dec->pos += len;
        return true;
    }
    if (type == 0xc5) {
        if (dec->pos + 2 > dec->size)
            return false;
        uint16_t len = (uint16_t) (dec->data[dec->pos] << 8 | dec->data[dec->pos + 1]);
        dec->pos += 2;
        if (dec->pos + len > dec->size)
            return false;
        dec->pos += len;
        return true;
    }
    if (type == 0xc6) {
        if (dec->pos + 4 > dec->size)
            return false;
        uint32_t len = (uint32_t) ((uint32_t) dec->data[dec->pos] << 24 | (uint32_t) dec->data[dec->pos + 1] << 16 |
                                   (uint32_t) dec->data[dec->pos + 2] << 8 | (uint32_t) dec->data[dec->pos + 3]);
        dec->pos += 4;
        if (dec->pos + len > dec->size)
            return false;
        dec->pos += len;
        return true;
    }

    /* fixarray: 0x90-0x9f */
    if (type >= 0x90 && type <= 0x9f) {
        uint8_t count = type & 0x0f;
        for (uint8_t i = 0; i < count; i++) {
            if (!mp_decoder_skip_value(dec))
                return false;
        }
        return true;
    }
    /* array16 */
    if (type == 0xdc) {
        if (dec->pos + 2 > dec->size)
            return false;
        uint16_t count = (uint16_t) (dec->data[dec->pos] << 8 | dec->data[dec->pos + 1]);
        dec->pos += 2;
        for (uint16_t i = 0; i < count; i++) {
            if (!mp_decoder_skip_value(dec))
                return false;
        }
        return true;
    }

    /* fixmap: 0x80-0x8f */
    if (type >= 0x80 && type <= 0x8f) {
        uint8_t count = type & 0x0f;
        for (uint8_t i = 0; i < count; i++) {
            if (!mp_decoder_skip_value(dec))
                return false;
            if (!mp_decoder_skip_value(dec))
                return false;
        }
        return true;
    }
    /* map16 */
    if (type == 0xde) {
        if (dec->pos + 2 > dec->size)
            return false;
        uint16_t count = (uint16_t) (dec->data[dec->pos] << 8 | dec->data[dec->pos + 1]);
        dec->pos += 2;
        for (uint16_t i = 0; i < count; i++) {
            if (!mp_decoder_skip_value(dec))
                return false;
            if (!mp_decoder_skip_value(dec))
                return false;
        }
        return true;
    }

    /* 未知类型——无法安全跳过 */
    return false;
}

/* ================================================================== */
/*  module_save_to_binary / module_load_from_binary                    */
/* ================================================================== */

/*
 * 二进制格式结构 (MessagePack map):
 * {
 *     "name": string,
 *     "version": string,
 *     "dependencies": [
 *         {"name": string, "version_constraint": string}
 *     ],
 *     "exports": {
 *         "function_blocks": [int, ...],
 *         "type_regions": [int, ...]
 *     },
 *     "axiom_package": binary   // 嵌套的公理包二进制数据（暂存名称列表）
 * }
 */

ModuleSaveStatus module_save_to_binary(const Module *mod, uint8_t **out_data, size_t *out_size) {
    if (!mod || !out_data || !out_size) {
        lv_set_error(lv_ERROR_INVALID_PARAM, "module_save_to_binary: 无效参数");
        return MODULE_SAVE_WRITE_ERROR;
    }

    MsgPackEncoder enc;
    if (!mp_encoder_init(&enc, 1024)) {
        lv_set_error(lv_ERROR_OUT_OF_MEMORY, "module_save_to_binary: 内存分配失败");
        return MODULE_SAVE_WRITE_ERROR;
    }

    /* 顶层 map: 5 个键 */
    mp_encoder_write_map_header(&enc, 5);

    /* "name" */
    mp_encoder_write_str(&enc, "name");
    mp_encoder_write_str(&enc, mod->name ? mod->name : "");

    /* "version" */
    mp_encoder_write_str(&enc, "version");
    mp_encoder_write_str(&enc, mod->version ? mod->version : "");

    /* "dependencies" */
    mp_encoder_write_str(&enc, "dependencies");
    mp_encoder_write_array_header(&enc, (uint16_t) mod->dependencies.count);
    for (int i = 0; i < mod->dependencies.count; i++) {
        mp_encoder_write_map_header(&enc, 2);
        mp_encoder_write_str(&enc, "name");
        mp_encoder_write_str(&enc, ((ModuleDependency *) mod->dependencies.data)[i].name ? ((ModuleDependency *) mod->dependencies.data)[i].name : "");
        mp_encoder_write_str(&enc, "version_constraint");
        mp_encoder_write_str(&enc,
                             ((ModuleDependency *) mod->dependencies.data)[i].version_constraint ? ((ModuleDependency *) mod->dependencies.data)[i].version_constraint : "");
    }

    /* "exports" */
    mp_encoder_write_str(&enc, "exports");
    mp_encoder_write_map_header(&enc, 2);
    /* function_blocks */
    mp_encoder_write_str(&enc, "function_blocks");
    mp_encoder_write_array_header(&enc, (uint16_t) (mod->exports ? mod->exports->function_block_ids.count : 0));
    if (mod->exports) {
        for (int i = 0; i < mod->exports->function_block_ids.count; i++) {
            mp_encoder_write_int(&enc, (int64_t) ((int *) mod->exports->function_block_ids.data)[i]);
        }
    }
    /* type_regions */
    mp_encoder_write_str(&enc, "type_regions");
    mp_encoder_write_array_header(&enc, (uint16_t) (mod->exports ? mod->exports->type_region_ids.count : 0));
    if (mod->exports) {
        for (int i = 0; i < mod->exports->type_region_ids.count; i++) {
            mp_encoder_write_int(&enc, (int64_t) ((int *) mod->exports->type_region_ids.data)[i]);
        }
    }

    /* "axiom_packages" - 存储公理包名称列表 */
    mp_encoder_write_str(&enc, "axiom_packages");
    mp_encoder_write_array_header(&enc, (uint16_t) mod->axiom_packages.count);
    for (int i = 0; i < mod->axiom_packages.count; i++) {
        if (((AxiomPackage **) mod->axiom_packages.data)[i]) {
            mp_encoder_write_str(&enc, ((AxiomPackage **) mod->axiom_packages.data)[i]->name ? ((AxiomPackage **) mod->axiom_packages.data)[i]->name : "");
        } else {
            mp_encoder_write_str(&enc, "");
        }
    }

    *out_data = enc.buffer;
    *out_size = enc.pos;
    /* 注意：不调用 mp_encoder_destroy，因为 buffer 已转移给调用者 */
    if (enc.error) {
        lv_set_error(lv_ERROR_OUT_OF_MEMORY, "module_save_to_binary: 编码过程中内存不足");
        lv_free((void **) &enc.buffer);
        *out_data = NULL;
        *out_size = 0;
        return MODULE_SAVE_WRITE_ERROR;
    }
    return MODULE_SAVE_OK;
}

ModuleLoadStatus module_load_from_binary(const uint8_t *data, size_t size, Module **out_module) {
    if (!data || size == 0 || !out_module) {
        lv_set_error(lv_ERROR_INVALID_PARAM, "module_load_from_binary: 无效参数");
        return MODULE_LOAD_PARSE_ERROR;
    }

    MsgPackDecoder dec;
    if (!mp_decoder_init(&dec, data, size)) {
        lv_set_error(lv_ERROR_INVALID_PARAM, "module_load_from_binary: 无效数据");
        return MODULE_LOAD_PARSE_ERROR;
    }

    uint16_t map_count = 0;
    if (!mp_decoder_read_map_header(&dec, &map_count)) {
        lv_set_error(lv_ERROR_PARSE, "module_load_from_binary: 无法读取顶层 map");
        return MODULE_LOAD_PARSE_ERROR;
    }

    /* 临时变量 */
    char *name = NULL;
    char *version = NULL;
    Module *mod = NULL;

    for (uint16_t i = 0; i < map_count; i++) {
        char *key = NULL;
        if (!mp_decoder_read_str(&dec, &key)) {
            lv_set_error(lv_ERROR_PARSE, "module_load_from_binary: 无法读取 map 键");
            lv_free((void **) &name);
            lv_free((void **) &version);
            return MODULE_LOAD_PARSE_ERROR;
        }

        if (strcmp(key, "name") == 0) {
            lv_free((void **) &key);
            if (!mp_decoder_read_str(&dec, &name)) {
                lv_set_error(lv_ERROR_PARSE, "module_load_from_binary: 无法读取 name");
                lv_free((void **) &name);
                lv_free((void **) &version);
                if (mod)
                    module_destroy(mod);
                return MODULE_LOAD_PARSE_ERROR;
            }
            /* 如果模块尚未创建且已有 name，立即创建 */
            if (!mod && name) {
                mod = module_create(name, version ? version : "0.0.0");
                if (!mod) {
                    lv_free((void **) &name);
                    lv_free((void **) &version);
                    return MODULE_LOAD_PARSE_ERROR;
                }
            }
        } else if (strcmp(key, "version") == 0) {
            lv_free((void **) &key);
            if (!mp_decoder_read_str(&dec, &version)) {
                lv_set_error(lv_ERROR_PARSE, "module_load_from_binary: 无法读取 version");
                lv_free((void **) &name);
                lv_free((void **) &version);
                if (mod)
                    module_destroy(mod);
                return MODULE_LOAD_PARSE_ERROR;
            }
            /* 如果模块已创建，更新版本 */
            if (mod && version) {
                lv_free((void **) &mod->version);
                mod->version = lv_strdup_safe(version);
            }
        } else if (strcmp(key, "dependencies") == 0) {
            lv_free((void **) &key);
            uint16_t dep_count = 0;
            if (!mp_decoder_read_array_header(&dec, &dep_count)) {
                lv_set_error(lv_ERROR_PARSE, "module_load_from_binary: 无法读取 dependencies 数组");
                lv_free((void **) &name);
                lv_free((void **) &version);
                if (mod)
                    module_destroy(mod);
                return MODULE_LOAD_PARSE_ERROR;
            }
            for (uint16_t j = 0; j < dep_count; j++) {
                uint16_t dep_map_count = 0;
                if (!mp_decoder_read_map_header(&dec, &dep_map_count)) {
                    lv_free((void **) &name);
                    lv_free((void **) &version);
                    if (mod)
                        module_destroy(mod);
                    return MODULE_LOAD_PARSE_ERROR;
                }
                char *dep_name = NULL;
                char *dep_ver = NULL;
                for (uint16_t k = 0; k < dep_map_count; k++) {
                    char *dk = NULL;
                    if (!mp_decoder_read_str(&dec, &dk)) {
                        lv_free((void **) &name);
                        lv_free((void **) &version);
                        lv_free((void **) &dep_name);
                        lv_free((void **) &dep_ver);
                        lv_free((void **) &dk);
                        return MODULE_LOAD_PARSE_ERROR;
                    }
                    if (strcmp(dk, "name") == 0) {
                        lv_free((void **) &dk);
                        mp_decoder_read_str(&dec, &dep_name);
                    } else if (strcmp(dk, "version_constraint") == 0) {
                        lv_free((void **) &dk);
                        mp_decoder_read_str(&dec, &dep_ver);
                    } else {
                        lv_free((void **) &dk);
                        /* 跳过未知值 */
                        mp_decoder_skip_value(&dec);
                    }
                }
                if (mod && dep_name) {
                    module_add_dependency(mod, dep_name, dep_ver ? dep_ver : "");
                }
                lv_free((void **) &dep_name);
                lv_free((void **) &dep_ver);
            }
        } else if (strcmp(key, "exports") == 0) {
            lv_free((void **) &key);
            uint16_t exp_map_count = 0;
            if (!mp_decoder_read_map_header(&dec, &exp_map_count)) {
                lv_free((void **) &name);
                lv_free((void **) &version);
                if (mod)
                    module_destroy(mod);
                return MODULE_LOAD_PARSE_ERROR;
            }
            for (uint16_t j = 0; j < exp_map_count; j++) {
                char *ek = NULL;
                if (!mp_decoder_read_str(&dec, &ek)) {
                    lv_free((void **) &name);
                    lv_free((void **) &version);
                    if (mod)
                        module_destroy(mod);
                    return MODULE_LOAD_PARSE_ERROR;
                }
                if (strcmp(ek, "function_blocks") == 0) {
                    lv_free((void **) &ek);
                    uint16_t fb_count = 0;
                    if (!mp_decoder_read_array_header(&dec, &fb_count)) {
                        lv_free((void **) &name);
                        lv_free((void **) &version);
                        if (mod)
                            module_destroy(mod);
                        return MODULE_LOAD_PARSE_ERROR;
                    }
                    for (uint16_t k = 0; k < fb_count; k++) {
                        int64_t val = 0;
                        if (mp_decoder_read_int(&dec, &val) && mod) {
                            module_export_function_block(mod, (int) val);
                        }
                    }
                } else if (strcmp(ek, "type_regions") == 0) {
                    lv_free((void **) &ek);
                    uint16_t tr_count = 0;
                    if (!mp_decoder_read_array_header(&dec, &tr_count)) {
                        lv_free((void **) &name);
                        lv_free((void **) &version);
                        if (mod)
                            module_destroy(mod);
                        return MODULE_LOAD_PARSE_ERROR;
                    }
                    for (uint16_t k = 0; k < tr_count; k++) {
                        int64_t val = 0;
                        if (mp_decoder_read_int(&dec, &val) && mod) {
                            module_export_type_region(mod, (int) val);
                        }
                    }
                } else {
                    lv_free((void **) &ek);
                    /* 跳过未知值 */
                    mp_decoder_skip_value(&dec);
                }
            }
        } else if (strcmp(key, "axiom_packages") == 0) {
            lv_free((void **) &key);
            uint16_t pkg_count = 0;
            if (!mp_decoder_read_array_header(&dec, &pkg_count)) {
                lv_free((void **) &name);
                lv_free((void **) &version);
                return MODULE_LOAD_PARSE_ERROR;
            }
            for (uint16_t j = 0; j < pkg_count; j++) {
                char *pkg_name = NULL;
                if (mp_decoder_read_str(&dec, &pkg_name) && mod && pkg_name) {
                    AxiomPackage *pkg = lv_axiom_package_create(pkg_name, "0.0.0");
                    if (pkg) {
                        module_add_axiom_package(mod, pkg);
                    }
                }
                lv_free((void **) &pkg_name);
            }
        } else {
            lv_free((void **) &key);
            /* 跳过未知键的值 */
            mp_decoder_skip_value(&dec);
        }
    }

    /* 创建模块（如果尚未创建） */
    if (!mod && name) {
        mod = module_create(name, version ? version : "0.0.0");
    }

    if (!mod) {
        lv_set_error(lv_ERROR_OUT_OF_MEMORY, "module_load_from_binary: 无法创建模块");
        lv_free((void **) &name);
        lv_free((void **) &version);
        return MODULE_LOAD_PARSE_ERROR;
    }

    /* 确保模块有名称（map 键顺序不可依赖） */
    if (!mod->name) {
        mod->name = lv_strdup_safe("unnamed_module");
    }

    lv_free((void **) &name);
    lv_free((void **) &version);
    *out_module = mod;
    return MODULE_LOAD_OK;
}

