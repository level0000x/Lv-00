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

#include "lv/lv_bytes.h"
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

/* 编码器：构建在公共字节流设施 lvByteWriter 之上（消除手写 {buf,cap,pos} 游标） */
typedef struct {
    lvByteWriter w; /* 动态字节流（内部 buf/capacity/pos） */
    bool error;     /* 编码错误标志：写失败时设置 */
} MsgPackEncoder;

/* 解码器 */
typedef struct {
    const uint8_t *data;
    size_t size;
    size_t pos;
} MsgPackDecoder;

/* ---------- 编码器辅助函数 ---------- */

static bool mp_encoder_init(MsgPackEncoder *enc, size_t initial_capacity) {
    if (!lv_byte_writer_init(&enc->w, initial_capacity))
        return false;
    enc->error = false;
    return true;
}

static bool mp_encoder_ensure(MsgPackEncoder *enc, size_t extra) {
    return lv_byte_writer_ensure(&enc->w, extra);
}

static void mp_encoder_write_byte(MsgPackEncoder *enc, uint8_t b) {
    if (enc->error)
        return;
    if (!lv_byte_writer_write_u8(&enc->w, b))
        enc->error = true;
}

static void mp_encoder_write_u16(MsgPackEncoder *enc, uint16_t v) {
    if (enc->error)
        return;
    if (!lv_byte_writer_write_u16_be(&enc->w, v))
        enc->error = true;
}

static void mp_encoder_write_u32(MsgPackEncoder *enc, uint32_t v) {
    if (enc->error)
        return;
    if (!lv_byte_writer_write_u32_be(&enc->w, v))
        enc->error = true;
}

static void mp_encoder_write_u64(MsgPackEncoder *enc, uint64_t v) {
    if (enc->error)
        return;
    if (!lv_byte_writer_write_u64_be(&enc->w, v))
        enc->error = true;
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
    if (!lv_byte_writer_write_bytes(&enc->w, str, len))
        enc->error = true;
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
    if (!lv_byte_writer_write_bytes(&enc->w, data, len))
        enc->error = true;
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
    lv_byte_writer_destroy(&enc->w);
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
    /* 大端序，与写端一致（复用公共 lv_load_be16，替换手写位移拼装） */
    if (dec->size - dec->pos < 2)
        return 0;
    uint16_t v = lv_load_be16(dec->data + dec->pos);
    dec->pos += 2;
    return v;
}

static uint32_t mp_decoder_read_u32(MsgPackDecoder *dec) {
    if (dec->size - dec->pos < 4)
        return 0;
    uint32_t v = lv_load_be32(dec->data + dec->pos);
    dec->pos += 4;
    return v;
}

static uint64_t mp_decoder_read_u64(MsgPackDecoder *dec) {
    if (dec->size - dec->pos < 8)
        return 0;
    uint64_t v = lv_load_be64(dec->data + dec->pos);
    dec->pos += 8;
    return v;
}

static int64_t mp_decoder_read_i64(MsgPackDecoder *dec) {
    return (int64_t) mp_decoder_read_u64(dec);
}

/* ── MSGPACK 类型分发查找表（替代手写 type == 相等判断链） ── */

/** @brief 带长度字段的 msgpack 类型表条目：type → 长度字段字节数 */
typedef struct {
    uint8_t type;
    uint8_t len_bytes;
} MpLenEntry;

/** @brief 在长度类型表中查找类型对应的长度字段字节数 */
static bool mp_decoder_find_len(const MpLenEntry *table, size_t count, uint8_t type, uint8_t *len_bytes) {
    for (size_t i = 0; i < count; i++) {
        if (table[i].type == type) {
            *len_bytes = table[i].len_bytes;
            return true;
        }
    }
    return false;
}

/** @brief 字符串长度类型表（替代 STR8/16/32 相等分发） */
static const MpLenEntry kStrLenTable[] = {
    {MSGPACK_STR8, 1},
    {MSGPACK_STR16, 2},
    {MSGPACK_STR32, 4},
};

/** @brief 二进制长度类型表（替代 BIN8/16/32 相等分发） */
static const MpLenEntry kBinLenTable[] = {
    {MSGPACK_BIN8, 1},
    {MSGPACK_BIN16, 2},
    {MSGPACK_BIN32, 4},
};

/** @brief 数组长度类型表（替代 ARRAY16 相等分发） */
static const MpLenEntry kArrayLenTable[] = {
    {MSGPACK_ARRAY16, 2},
};

/** @brief map 长度类型表（替代 MAP16 相等分发） */
static const MpLenEntry kMapLenTable[] = {
    {MSGPACK_MAP16, 2},
};

/** @brief 按长度字段字节数读取 msgpack 长度（1/2/4 字节） */
static bool mp_decoder_read_len_field(MsgPackDecoder *dec, uint8_t len_bytes, uint32_t *len) {
    if (len_bytes == 1) {
        *len = mp_decoder_read_byte(dec);
        return true;
    }
    if (len_bytes == 2) {
        *len = mp_decoder_read_u16(dec);
        return true;
    }
    *len = mp_decoder_read_u32(dec);
    return true;
}

typedef bool (*IntDecoder)(MsgPackDecoder *dec, int64_t *out);

static bool mp_decode_int8(MsgPackDecoder *dec, int64_t *out) {
    *out = (int64_t) (int8_t) mp_decoder_read_byte(dec);
    return true;
}

static bool mp_decode_int16(MsgPackDecoder *dec, int64_t *out) {
    *out = (int64_t) (int16_t) mp_decoder_read_u16(dec);
    return true;
}

static bool mp_decode_int32(MsgPackDecoder *dec, int64_t *out) {
    *out = (int64_t) (int32_t) mp_decoder_read_u32(dec);
    return true;
}

static bool mp_decode_int64(MsgPackDecoder *dec, int64_t *out) {
    *out = mp_decoder_read_i64(dec);
    return true;
}

static bool mp_decode_uint8(MsgPackDecoder *dec, int64_t *out) {
    *out = (int64_t) mp_decoder_read_byte(dec);
    return true;
}

static bool mp_decode_uint16(MsgPackDecoder *dec, int64_t *out) {
    *out = (int64_t) mp_decoder_read_u16(dec);
    return true;
}

static bool mp_decode_uint32(MsgPackDecoder *dec, int64_t *out) {
    *out = (int64_t) mp_decoder_read_u32(dec);
    return true;
}

static bool mp_decode_uint64(MsgPackDecoder *dec, int64_t *out) {
    *out = (int64_t) mp_decoder_read_u64(dec);
    return true;
}

/** @brief 整数类型→解码函数 查找表（替代 INT8/16/32/64、UINT8/16/32/64 相等分发） */
static const struct {
    uint8_t type;
    IntDecoder decoder;
} kIntDecodeTable[] = {
    {MSGPACK_INT8, mp_decode_int8},
    {MSGPACK_INT16, mp_decode_int16},
    {MSGPACK_INT32, mp_decode_int32},
    {MSGPACK_INT64, mp_decode_int64},
    {MSGPACK_UINT8, mp_decode_uint8},
    {MSGPACK_UINT16, mp_decode_uint16},
    {MSGPACK_UINT32, mp_decode_uint32},
    {MSGPACK_UINT64, mp_decode_uint64},
};

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
    } else {
        /* 查表分发（替代 8 分支相等判断链） */
        for (size_t i = 0; i < lv_ARRAY_SIZE(kIntDecodeTable); i++) {
            if (kIntDecodeTable[i].type == type)
                return kIntDecodeTable[i].decoder(dec, out);
        }
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
    } else {
        /* 长度类型查表（替代 STR8/16/32 相等分发） */
        uint8_t len_bytes = 0;
        uint32_t raw_len = 0;
        if (!mp_decoder_find_len(kStrLenTable, lv_ARRAY_SIZE(kStrLenTable), type, &len_bytes) ||
            !mp_decoder_read_len_field(dec, len_bytes, &raw_len))
            return false;
        len = raw_len;
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
    /* 长度类型查表（替代 BIN8/16/32 相等分发） */
    uint8_t len_bytes = 0;
    uint32_t raw_len = 0;
    if (!mp_decoder_find_len(kBinLenTable, lv_ARRAY_SIZE(kBinLenTable), type, &len_bytes) ||
        !mp_decoder_read_len_field(dec, len_bytes, &raw_len))
        return false;
    len = raw_len;

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
    } else {
        /* 长度类型查表（替代 ARRAY16 相等分发） */
        uint8_t len_bytes = 0;
        uint32_t raw = 0;
        if (!mp_decoder_find_len(kArrayLenTable, lv_ARRAY_SIZE(kArrayLenTable), type, &len_bytes) ||
            !mp_decoder_read_len_field(dec, len_bytes, &raw))
            return false;
        *count = (uint16_t) raw;
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
    } else {
        /* 长度类型查表（替代 MAP16 相等分发） */
        uint8_t len_bytes = 0;
        uint32_t raw = 0;
        if (!mp_decoder_find_len(kMapLenTable, lv_ARRAY_SIZE(kMapLenTable), type, &len_bytes) ||
            !mp_decoder_read_len_field(dec, len_bytes, &raw))
            return false;
        *count = (uint16_t) raw;
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

    *out_data = enc.w.buf;
    *out_size = enc.w.pos;
    /* 注意：不调用 mp_encoder_destroy，因为 buffer 已转移给调用者 */
    if (enc.error) {
        lv_set_error(lv_ERROR_OUT_OF_MEMORY, "module_save_to_binary: 编码过程中内存不足");
        lv_free((void **) &enc.w.buf);
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

