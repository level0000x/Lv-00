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

/* 解码器：构建在公共字节流设施 lvByteReader 之上（消除手写 {data,size,pos} 游标） */
typedef struct {
    lvByteReader r; /* 有界只读游标（内部 buf/size/pos） */
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
    lv_byte_reader_init(&dec->r, data, size);
    return data != NULL && size > 0;
}

static bool mp_decoder_has_data(MsgPackDecoder *dec) {
    return lv_byte_reader_remaining(&dec->r) > 0;
}

/* 只读窥探当前字节（不推进游标） */
static uint8_t mp_decoder_peek(MsgPackDecoder *dec) {
    return dec->r.pos < dec->r.size ? dec->r.buf[dec->r.pos] : 0;
}

static uint8_t mp_decoder_read_byte(MsgPackDecoder *dec) {
    uint8_t b = 0;
    if (!lv_byte_reader_read_u8(&dec->r, &b))
        return 0;
    return b;
}

static uint16_t mp_decoder_read_u16(MsgPackDecoder *dec) {
    /* 大端序，与写端一致（复用公共 lv_byte_reader_read_u16_be，替换手写剩余检查+位移拼装） */
    uint16_t v = 0;
    if (!lv_byte_reader_read_u16_be(&dec->r, &v))
        return 0;
    return v;
}

static uint32_t mp_decoder_read_u32(MsgPackDecoder *dec) {
    uint32_t v = 0;
    if (!lv_byte_reader_read_u32_be(&dec->r, &v))
        return 0;
    return v;
}

static uint64_t mp_decoder_read_u64(MsgPackDecoder *dec) {
    uint64_t v = 0;
    if (!lv_byte_reader_read_u64_be(&dec->r, &v))
        return 0;
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

    if (lv_byte_reader_remaining(&dec->r) < len)
        return false;

    char *str = (char *) lv_calloc(len + 1, 1);
    if (!str)
        return false;
    memcpy(str, dec->r.buf + dec->r.pos, len);
    str[len] = '\0';
    dec->r.pos += len;
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

    if (lv_byte_reader_remaining(&dec->r) < len)
        return false;

    uint8_t *buf = (uint8_t *) lv_calloc(len, 1);
    if (!buf)
        return false;
    memcpy(buf, dec->r.buf + dec->r.pos, len);
    dec->r.pos += len;
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

/* ── mp_decoder_skip_value 跳转表（替代 ~20 分支 if 链） ──
 *
 * 256 项全量类型跳转表：type 字节 → {长度字段字节数, 值类别, 跳过函数}，
 * 与文件既有 kStrLenTable/kBinLenTable/kArrayLenTable/kMapLenTable +
 * mp_decoder_find_len 的查找表风格一致，但为 O(1) 直接索引。
 *
 * len_bytes 语义：
 *   - > 0：长度/计数字段占 len_bytes 字节（1/2/4，跟随类型字节之后）
 *   - = 0：fix 系列（fixstr/fixarray/fixmap）——长度/计数编码在类型字节
 *           低位，由 skip_fn 用 type & mask 提取
 *
 * 未登记的槽位零初始化 → container == MP_SKIP_INVALID → 返回 false
 * （未知类型不可安全跳过，与原 if 链末尾 return false 语义一致；
 *  0xcf uint64 原实现即未支持，此处同样不登记以保持行为）。 */
typedef enum {
    MP_SKIP_INVALID = 0, /* 未知类型：不可安全跳过（未登记槽位 = 零初始化） */
    MP_SKIP_NONE,        /* 无负载：nil/false/true/fixint */
    MP_SKIP_FIXED,       /* 固定长度负载：uint/int/float 系列 */
    MP_SKIP_STR,         /* 字符串：fixstr/str8/16/32 */
    MP_SKIP_BIN,         /* 二进制：bin8/16/32 */
    MP_SKIP_ARR,         /* 数组：fixarray/array16 */
    MP_SKIP_MAP,         /* map：fixmap/map16 */
} MpSkipKind;

typedef bool (*MpSkipFn)(MsgPackDecoder *dec, uint8_t type, uint8_t len_bytes);

typedef struct {
    uint8_t len_bytes; /* 长度/计数字段字节数；0 = 编码在类型字节低位 */
    uint8_t container; /* MpSkipKind 值类别 */
    MpSkipFn skip_fn;
} MpSkipEntry;

/* 前向声明：mp_skip_array_value / mp_skip_map_value 递归引用跳转表驱动函数 */
static bool mp_decoder_skip_value(MsgPackDecoder *dec);

/* 无负载：nil/false/true/fixint（已消费类型字节，无附加数据） */
static bool mp_skip_none(MsgPackDecoder *dec, uint8_t type, uint8_t len_bytes) {
    (void) dec;
    (void) type;
    (void) len_bytes;
    return true;
}

/* 固定长度负载：跳 len_bytes 字节（uint/int/float 系列） */
static bool mp_skip_fixed_payload(MsgPackDecoder *dec, uint8_t type, uint8_t len_bytes) {
    (void) type;
    if (lv_byte_reader_remaining(&dec->r) < len_bytes)
        return false;
    dec->r.pos += len_bytes;
    return true;
}

/* 字符串：fixstr（len_bytes=0，长度在 type 低位）或 str8/16/32（长度字段） */
static bool mp_skip_str_value(MsgPackDecoder *dec, uint8_t type, uint8_t len_bytes) {
    uint32_t len;
    if (len_bytes == 0) {
        len = type & 0x1f; /* fixstr: 101xxxxx */
    } else {
        if (lv_byte_reader_remaining(&dec->r) < len_bytes)
            return false;
        mp_decoder_read_len_field(dec, len_bytes, &len);
    }
    if (lv_byte_reader_remaining(&dec->r) < len)
        return false;
    dec->r.pos += len;
    return true;
}

/* 二进制：bin8/16/32（均有长度字段） */
static bool mp_skip_bin_value(MsgPackDecoder *dec, uint8_t type, uint8_t len_bytes) {
    (void) type;
    uint32_t len;
    if (lv_byte_reader_remaining(&dec->r) < len_bytes)
        return false;
    mp_decoder_read_len_field(dec, len_bytes, &len);
    if (lv_byte_reader_remaining(&dec->r) < len)
        return false;
    dec->r.pos += len;
    return true;
}

/* 数组：fixarray（len_bytes=0，计数在 type 低位）或 array16（长度字段） */
static bool mp_skip_array_value(MsgPackDecoder *dec, uint8_t type, uint8_t len_bytes) {
    uint32_t count;
    if (len_bytes == 0) {
        count = type & 0x0f; /* fixarray: 1001xxxx */
    } else {
        if (lv_byte_reader_remaining(&dec->r) < len_bytes)
            return false;
        mp_decoder_read_len_field(dec, len_bytes, &count);
    }
    for (uint32_t i = 0; i < count; i++) {
        if (!mp_decoder_skip_value(dec))
            return false;
    }
    return true;
}

/* map：fixmap（len_bytes=0，计数在 type 低位）或 map16（长度字段）；每项含键和值 */
static bool mp_skip_map_value(MsgPackDecoder *dec, uint8_t type, uint8_t len_bytes) {
    uint32_t count;
    if (len_bytes == 0) {
        count = type & 0x0f; /* fixmap: 1000xxxx */
    } else {
        if (lv_byte_reader_remaining(&dec->r) < len_bytes)
            return false;
        mp_decoder_read_len_field(dec, len_bytes, &count);
    }
    for (uint32_t i = 0; i < count; i++) {
        if (!mp_decoder_skip_value(dec))
            return false;
        if (!mp_decoder_skip_value(dec))
            return false;
    }
    return true;
}

/* 无负载条目（fixint/nil/false/true）区间展开宏：完全标准 C，
 * 避免 GNU 范围指定初始化器 [a ... b] 扩展 */
#define MP_SKIP_NONE_ENTRY {0, MP_SKIP_NONE, mp_skip_none},
#define MP_SKIP_NONE_X8 \
    MP_SKIP_NONE_ENTRY MP_SKIP_NONE_ENTRY MP_SKIP_NONE_ENTRY MP_SKIP_NONE_ENTRY \
    MP_SKIP_NONE_ENTRY MP_SKIP_NONE_ENTRY MP_SKIP_NONE_ENTRY MP_SKIP_NONE_ENTRY
#define MP_SKIP_NONE_X16 MP_SKIP_NONE_X8 MP_SKIP_NONE_X8
#define MP_SKIP_NONE_X32 MP_SKIP_NONE_X16 MP_SKIP_NONE_X16
#define MP_SKIP_NONE_X128 MP_SKIP_NONE_X32 MP_SKIP_NONE_X32 MP_SKIP_NONE_X32 MP_SKIP_NONE_X32

/* 未登记槽位显式占位（零初始化语义的等价显式写法） */
#define MP_SKIP_INVALID_ENTRY {0, MP_SKIP_INVALID, NULL},

/* fixmap 区间（0x80-0x8f）条目 */
#define MP_SKIP_MAP_FIX_ENTRY {0, MP_SKIP_MAP, mp_skip_map_value},
#define MP_SKIP_MAP_FIX_X8 \
    MP_SKIP_MAP_FIX_ENTRY MP_SKIP_MAP_FIX_ENTRY MP_SKIP_MAP_FIX_ENTRY MP_SKIP_MAP_FIX_ENTRY \
    MP_SKIP_MAP_FIX_ENTRY MP_SKIP_MAP_FIX_ENTRY MP_SKIP_MAP_FIX_ENTRY MP_SKIP_MAP_FIX_ENTRY
#define MP_SKIP_MAP_FIX_X16 MP_SKIP_MAP_FIX_X8 MP_SKIP_MAP_FIX_X8

/* fixarray 区间（0x90-0x9f）条目 */
#define MP_SKIP_ARR_FIX_ENTRY {0, MP_SKIP_ARR, mp_skip_array_value},
#define MP_SKIP_ARR_FIX_X8 \
    MP_SKIP_ARR_FIX_ENTRY MP_SKIP_ARR_FIX_ENTRY MP_SKIP_ARR_FIX_ENTRY MP_SKIP_ARR_FIX_ENTRY \
    MP_SKIP_ARR_FIX_ENTRY MP_SKIP_ARR_FIX_ENTRY MP_SKIP_ARR_FIX_ENTRY MP_SKIP_ARR_FIX_ENTRY
#define MP_SKIP_ARR_FIX_X16 MP_SKIP_ARR_FIX_X8 MP_SKIP_ARR_FIX_X8

/* fixstr 区间（0xa0-0xbf）条目 */
#define MP_SKIP_STR_FIX_ENTRY {0, MP_SKIP_STR, mp_skip_str_value},
#define MP_SKIP_STR_FIX_X8 \
    MP_SKIP_STR_FIX_ENTRY MP_SKIP_STR_FIX_ENTRY MP_SKIP_STR_FIX_ENTRY MP_SKIP_STR_FIX_ENTRY \
    MP_SKIP_STR_FIX_ENTRY MP_SKIP_STR_FIX_ENTRY MP_SKIP_STR_FIX_ENTRY MP_SKIP_STR_FIX_ENTRY
#define MP_SKIP_STR_FIX_X16 MP_SKIP_STR_FIX_X8 MP_SKIP_STR_FIX_X8
#define MP_SKIP_STR_FIX_X32 MP_SKIP_STR_FIX_X16 MP_SKIP_STR_FIX_X16

/** @brief mp_decoder_skip_value 跳转表：256 个类型字节 → 跳过策略（替代 ~20 分支 if 链） */
static const MpSkipEntry kSkipTable[256] = {
    /* 0x00-0x7f positive fixint：无负载 */
    MP_SKIP_NONE_X128
    /* 0x80-0x8f fixmap */
    MP_SKIP_MAP_FIX_X16
    /* 0x90-0x9f fixarray */
    MP_SKIP_ARR_FIX_X16
    /* 0xa0-0xbf fixstr */
    MP_SKIP_STR_FIX_X32
    /* 0xc0-0xdf 单条目（全部显式列出，槽位与注释一一对应） */
    MP_SKIP_NONE_ENTRY          /* 0xc0 nil */
    MP_SKIP_INVALID_ENTRY       /* 0xc1 保留类型：原 if 链不可跳过 */
    MP_SKIP_NONE_ENTRY          /* 0xc2 false */
    MP_SKIP_NONE_ENTRY          /* 0xc3 true */
    {1, MP_SKIP_BIN, mp_skip_bin_value},    /* 0xc4 bin8 */
    {2, MP_SKIP_BIN, mp_skip_bin_value},    /* 0xc5 bin16 */
    {4, MP_SKIP_BIN, mp_skip_bin_value},    /* 0xc6 bin32 */
    MP_SKIP_INVALID_ENTRY       /* 0xc7 保留类型 */
    MP_SKIP_INVALID_ENTRY       /* 0xc8 保留类型 */
    MP_SKIP_INVALID_ENTRY       /* 0xc9 保留类型 */
    {4, MP_SKIP_FIXED, mp_skip_fixed_payload}, /* 0xca float32 */
    {8, MP_SKIP_FIXED, mp_skip_fixed_payload}, /* 0xcb float64 */
    {1, MP_SKIP_FIXED, mp_skip_fixed_payload}, /* 0xcc uint8 */
    {2, MP_SKIP_FIXED, mp_skip_fixed_payload}, /* 0xcd uint16 */
    {4, MP_SKIP_FIXED, mp_skip_fixed_payload}, /* 0xce uint32 */
    MP_SKIP_INVALID_ENTRY       /* 0xcf uint64 未登记（与原实现一致：原 if 链无 0xcf 分支） */
    {1, MP_SKIP_FIXED, mp_skip_fixed_payload}, /* 0xd0 int8 */
    {2, MP_SKIP_FIXED, mp_skip_fixed_payload}, /* 0xd1 int16 */
    {4, MP_SKIP_FIXED, mp_skip_fixed_payload}, /* 0xd2 int32 */
    {8, MP_SKIP_FIXED, mp_skip_fixed_payload}, /* 0xd3 int64 */
    MP_SKIP_INVALID_ENTRY       /* 0xd4 保留类型 */
    MP_SKIP_INVALID_ENTRY       /* 0xd5 保留类型 */
    MP_SKIP_INVALID_ENTRY       /* 0xd6 保留类型 */
    MP_SKIP_INVALID_ENTRY       /* 0xd7 保留类型 */
    MP_SKIP_INVALID_ENTRY       /* 0xd8 保留类型 */
    {1, MP_SKIP_STR, mp_skip_str_value},    /* 0xd9 str8 */
    {2, MP_SKIP_STR, mp_skip_str_value},    /* 0xda str16 */
    {4, MP_SKIP_STR, mp_skip_str_value},    /* 0xdb str32 */
    {2, MP_SKIP_ARR, mp_skip_array_value},  /* 0xdc array16 */
    MP_SKIP_INVALID_ENTRY       /* 0xdd 保留类型 */
    {2, MP_SKIP_MAP, mp_skip_map_value},    /* 0xde map16 */
    MP_SKIP_INVALID_ENTRY       /* 0xdf 保留类型 */
    /* 0xe0-0xff negative fixint：无负载 */
    MP_SKIP_NONE_X32
};

/* 跳过一条完整的 MessagePack 值（用于跳过未知的 map 值） */
static bool mp_decoder_skip_value(MsgPackDecoder *dec) {
    if (!mp_decoder_has_data(dec))
        return false;
    uint8_t type = mp_decoder_peek(dec);
    const MpSkipEntry *entry = &kSkipTable[type];
    if (entry->container == MP_SKIP_INVALID)
        return false; /* 未知类型——无法安全跳过 */
    mp_decoder_read_byte(dec); /* consume type byte */
    return entry->skip_fn(dec, type, entry->len_bytes);
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

/* ── msgpack 序列化/反序列化键分发表（X-macro 单一事实源生成） ──
 *
 * 键名与字段名一致（LV_MODULE_FIELD_X），由宏生成 {键名, handler} 表，
 * 与 JSON（module_serialize_json.c）/版本哈希（module_serialize.c）共享
 * 同一字段枚举。msgpack 二进制格式按历史约定不含 graph：其 handler 槽位
 * 显式置 NULL（序列化时跳过 → 顶层 map 计数恒为 5；反序列化时命中该键
 * 按未知键 skip 值），输出字节与历史实现完全一致。 */

typedef void (*ModuleMpWriteFn)(MsgPackEncoder *enc, const Module *mod);

static void mp_write_name(MsgPackEncoder *enc, const Module *mod) {
    mp_encoder_write_str(enc, mod->name ? mod->name : "");
}

static void mp_write_version(MsgPackEncoder *enc, const Module *mod) {
    mp_encoder_write_str(enc, mod->version ? mod->version : "");
}

static void mp_write_dependencies(MsgPackEncoder *enc, const Module *mod) {
    mp_encoder_write_array_header(enc, (uint16_t) mod->dependencies.count);
    for (int i = 0; i < mod->dependencies.count; i++) {
        mp_encoder_write_map_header(enc, 2);
        mp_encoder_write_str(enc, "name");
        mp_encoder_write_str(enc, ((ModuleDependency *) mod->dependencies.data)[i].name ? ((ModuleDependency *) mod->dependencies.data)[i].name : "");
        mp_encoder_write_str(enc, "version_constraint");
        mp_encoder_write_str(enc,
                             ((ModuleDependency *) mod->dependencies.data)[i].version_constraint ? ((ModuleDependency *) mod->dependencies.data)[i].version_constraint : "");
    }
}

static void mp_write_exports(MsgPackEncoder *enc, const Module *mod) {
    mp_encoder_write_map_header(enc, 2);
    /* function_blocks */
    mp_encoder_write_str(enc, "function_blocks");
    mp_encoder_write_array_header(enc, (uint16_t) (mod->exports ? mod->exports->function_block_ids.count : 0));
    if (mod->exports) {
        for (int i = 0; i < mod->exports->function_block_ids.count; i++) {
            mp_encoder_write_int(enc, (int64_t) ((int *) mod->exports->function_block_ids.data)[i]);
        }
    }
    /* type_regions */
    mp_encoder_write_str(enc, "type_regions");
    mp_encoder_write_array_header(enc, (uint16_t) (mod->exports ? mod->exports->type_region_ids.count : 0));
    if (mod->exports) {
        for (int i = 0; i < mod->exports->type_region_ids.count; i++) {
            mp_encoder_write_int(enc, (int64_t) ((int *) mod->exports->type_region_ids.data)[i]);
        }
    }
}

static void mp_write_axiom_packages(MsgPackEncoder *enc, const Module *mod) {
    mp_encoder_write_array_header(enc, (uint16_t) mod->axiom_packages.count);
    for (int i = 0; i < mod->axiom_packages.count; i++) {
        if (((AxiomPackage **) mod->axiom_packages.data)[i]) {
            mp_encoder_write_str(enc, ((AxiomPackage **) mod->axiom_packages.data)[i]->name ? ((AxiomPackage **) mod->axiom_packages.data)[i]->name : "");
        } else {
            mp_encoder_write_str(enc, "");
        }
    }
}

/* graph 不进入 msgpack 二进制格式（历史约定）；NULL 槽位 = 序列化跳过、反序列化 skip 值 */
static const ModuleMpWriteFn mp_write_graph = NULL;

#define LV_MODULE_MP_WRITE_ENTRY(field) { #field, mp_write_##field },
static const struct {
    const char *name;
    ModuleMpWriteFn fn;
} kModuleMpWriters[] = {
    LV_MODULE_FIELD_X(LV_MODULE_MP_WRITE_ENTRY)
};

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

    /* 顶层 map 计数 = 非 NULL handler 槽位数（graph 跳过 → 恒为 5，与历史一致） */
    uint16_t map_count = 0;
    for (size_t i = 0; i < lv_ARRAY_SIZE(kModuleMpWriters); i++) {
        if (kModuleMpWriters[i].fn)
            map_count++;
    }
    mp_encoder_write_map_header(&enc, map_count);

    /* 按 X-macro 字段清单顺序写键值对 */
    for (size_t i = 0; i < lv_ARRAY_SIZE(kModuleMpWriters); i++) {
        if (kModuleMpWriters[i].fn) {
            mp_encoder_write_str(&enc, kModuleMpWriters[i].name);
            kModuleMpWriters[i].fn(&enc, mod);
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

/* msgpack 反序列化字段 handler：每个 handler 解析一个顶层键的值。
 * name/version 先收集到临时变量、模块延迟创建（键序不可依赖）。
 * 解析失败返回 false（内部已按历史语义清理临时资源），主循环统一
 * return MODULE_LOAD_PARSE_ERROR。graph 键的 handler 槽为 NULL → 按
 * 未知键 skip 值处理（msgpack 格式不含 graph，保持历史行为）。 */

typedef bool (*ModuleMpParseFn)(MsgPackDecoder *dec, Module **mod, char **name, char **version);

static bool mp_parse_name(MsgPackDecoder *dec, Module **mod, char **name, char **version) {
    if (!mp_decoder_read_str(dec, name)) {
        lv_set_error(lv_ERROR_PARSE, "module_load_from_binary: 无法读取 name");
        lv_free((void **) name);
        lv_free((void **) version);
        if (*mod)
            module_destroy(*mod);
        return false;
    }
    /* 如果模块尚未创建且已有 name，立即创建 */
    if (!*mod && *name) {
        *mod = module_create(*name, *version ? *version : "0.0.0");
        if (!*mod) {
            lv_free((void **) name);
            lv_free((void **) version);
            return false;
        }
    }
    return true;
}

static bool mp_parse_version(MsgPackDecoder *dec, Module **mod, char **name, char **version) {
    if (!mp_decoder_read_str(dec, version)) {
        lv_set_error(lv_ERROR_PARSE, "module_load_from_binary: 无法读取 version");
        lv_free((void **) name);
        lv_free((void **) version);
        if (*mod)
            module_destroy(*mod);
        return false;
    }
    /* 如果模块已创建，更新版本 */
    if (*mod && *version) {
        lv_free((void **) &(*mod)->version);
        (*mod)->version = lv_strdup_safe(*version);
    }
    return true;
}

static bool mp_parse_dependencies(MsgPackDecoder *dec, Module **mod, char **name, char **version) {
    uint16_t dep_count = 0;
    if (!mp_decoder_read_array_header(dec, &dep_count)) {
        lv_set_error(lv_ERROR_PARSE, "module_load_from_binary: 无法读取 dependencies 数组");
        lv_free((void **) name);
        lv_free((void **) version);
        if (*mod)
            module_destroy(*mod);
        return false;
    }
    for (uint16_t j = 0; j < dep_count; j++) {
        uint16_t dep_map_count = 0;
        if (!mp_decoder_read_map_header(dec, &dep_map_count)) {
            lv_free((void **) name);
            lv_free((void **) version);
            if (*mod)
                module_destroy(*mod);
            return false;
        }
        char *dep_name = NULL;
        char *dep_ver = NULL;
        for (uint16_t k = 0; k < dep_map_count; k++) {
            char *dk = NULL;
            if (!mp_decoder_read_str(dec, &dk)) {
                lv_free((void **) name);
                lv_free((void **) version);
                lv_free((void **) &dep_name);
                lv_free((void **) &dep_ver);
                lv_free((void **) &dk);
                return false;
            }
            if (strcmp(dk, "name") == 0) {
                lv_free((void **) &dk);
                mp_decoder_read_str(dec, &dep_name);
            } else if (strcmp(dk, "version_constraint") == 0) {
                lv_free((void **) &dk);
                mp_decoder_read_str(dec, &dep_ver);
            } else {
                lv_free((void **) &dk);
                /* 跳过未知值 */
                mp_decoder_skip_value(dec);
            }
        }
        if (*mod && dep_name) {
            module_add_dependency(*mod, dep_name, dep_ver ? dep_ver : "");
        }
        lv_free((void **) &dep_name);
        lv_free((void **) &dep_ver);
    }
    return true;
}

static bool mp_parse_exports(MsgPackDecoder *dec, Module **mod, char **name, char **version) {
    uint16_t exp_map_count = 0;
    if (!mp_decoder_read_map_header(dec, &exp_map_count)) {
        lv_free((void **) name);
        lv_free((void **) version);
        if (*mod)
            module_destroy(*mod);
        return false;
    }
    for (uint16_t j = 0; j < exp_map_count; j++) {
        char *ek = NULL;
        if (!mp_decoder_read_str(dec, &ek)) {
            lv_free((void **) name);
            lv_free((void **) version);
            if (*mod)
                module_destroy(*mod);
            return false;
        }
        if (strcmp(ek, "function_blocks") == 0) {
            lv_free((void **) &ek);
            uint16_t fb_count = 0;
            if (!mp_decoder_read_array_header(dec, &fb_count)) {
                lv_free((void **) name);
                lv_free((void **) version);
                if (*mod)
                    module_destroy(*mod);
                return false;
            }
            for (uint16_t k = 0; k < fb_count; k++) {
                int64_t val = 0;
                if (mp_decoder_read_int(dec, &val) && *mod) {
                    module_export_function_block(*mod, (int) val);
                }
            }
        } else if (strcmp(ek, "type_regions") == 0) {
            lv_free((void **) &ek);
            uint16_t tr_count = 0;
            if (!mp_decoder_read_array_header(dec, &tr_count)) {
                lv_free((void **) name);
                lv_free((void **) version);
                if (*mod)
                    module_destroy(*mod);
                return false;
            }
            for (uint16_t k = 0; k < tr_count; k++) {
                int64_t val = 0;
                if (mp_decoder_read_int(dec, &val) && *mod) {
                    module_export_type_region(*mod, (int) val);
                }
            }
        } else {
            lv_free((void **) &ek);
            /* 跳过未知值 */
            mp_decoder_skip_value(dec);
        }
    }
    return true;
}

static bool mp_parse_axiom_packages(MsgPackDecoder *dec, Module **mod, char **name, char **version) {
    uint16_t pkg_count = 0;
    if (!mp_decoder_read_array_header(dec, &pkg_count)) {
        lv_free((void **) name);
        lv_free((void **) version);
        return false;
    }
    for (uint16_t j = 0; j < pkg_count; j++) {
        char *pkg_name = NULL;
        if (mp_decoder_read_str(dec, &pkg_name) && *mod && pkg_name) {
            AxiomPackage *pkg = lv_axiom_package_create(pkg_name, "0.0.0");
            if (pkg) {
                module_add_axiom_package(*mod, pkg);
            }
        }
        lv_free((void **) &pkg_name);
    }
    return true;
}

/* graph 键不进入 msgpack 二进制格式（历史约定）；NULL 槽位 → 按未知键 skip 值 */
static const ModuleMpParseFn mp_parse_graph = NULL;

#define LV_MODULE_MP_PARSE_ENTRY(field) { #field, mp_parse_##field },
static const struct {
    const char *name;
    ModuleMpParseFn fn;
} kModuleMpParsers[] = {
    LV_MODULE_FIELD_X(LV_MODULE_MP_PARSE_ENTRY)
};

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

        /* 键分发：查 X-macro 生成的字段处理表（替代 5 分支 strcmp 链） */
        bool handled = false;
        for (size_t j = 0; j < lv_ARRAY_SIZE(kModuleMpParsers); j++) {
            if (strcmp(key, kModuleMpParsers[j].name) == 0) {
                handled = true;
                lv_free((void **) &key);
                if (kModuleMpParsers[j].fn) {
                    if (!kModuleMpParsers[j].fn(&dec, &mod, &name, &version))
                        return MODULE_LOAD_PARSE_ERROR; /* handler 已按历史语义清理临时资源 */
                } else {
                    /* 已知键但该格式不含此字段（graph）：跳过值 */
                    mp_decoder_skip_value(&dec);
                }
                break;
            }
        }
        if (!handled) {
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

