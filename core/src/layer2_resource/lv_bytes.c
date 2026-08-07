/**
 * @file lv_bytes.c
 * @brief 公共字节流编解码设施实现（lvByteWriter / lvByteReader）
 *
 * @details 端序原语复用 lv_utils.h 的 lv_store/load_be16/32/64、lv_store/load_le16/32/64；
 *          扩容统一委托 lv_ensure_capacity（内部含溢出检查与倍增）。
 */

#include "lv/lv_bytes.h"
#include "lv/lv_utils.h"

#include <limits.h>
#include <string.h>

/* ============================================================
 * lvByteWriter
 * ============================================================ */

bool lv_byte_writer_init(lvByteWriter *w, size_t initial_capacity) {
    if (!w)
        return false;
    w->buf = NULL;
    w->capacity = 0;
    w->pos = 0;
    w->error = false;
    if (initial_capacity > 0) {
        w->buf = (uint8_t *) lv_calloc(initial_capacity, 1);
        if (!w->buf)
            return false;
        w->capacity = initial_capacity;
    }
    return true;
}

void lv_byte_writer_destroy(lvByteWriter *w) {
    if (!w)
        return;
    lv_free((void **) &w->buf);
    w->capacity = 0;
    w->pos = 0;
    w->error = false;
}

bool lv_byte_writer_ensure(lvByteWriter *w, size_t extra) {
    if (!w)
        return false;
    if (extra <= w->capacity - w->pos)
        return true;

    /* 需要扩容：经局部 int 桥接后统一委托 lv_ensure_capacity（内部含溢出检查与倍增） */
    if (w->pos > (size_t) INT_MAX || extra > (size_t) INT_MAX - w->pos) {
        w->error = true;
        return false;
    }
    int cap = (int) w->capacity;
    if (!lv_ensure_capacity((void **) &w->buf, (int) (w->pos + extra), &cap, 1, 0)) {
        w->error = true;
        return false;
    }
    w->capacity = (size_t) cap;
    return true;
}

bool lv_byte_writer_write_u8(lvByteWriter *w, uint8_t v) {
    if (!w)
        return false;
    if (!lv_byte_writer_ensure(w, 1))
        return false;
    w->buf[w->pos++] = v;
    return true;
}

bool lv_byte_writer_write_u16_le(lvByteWriter *w, uint16_t v) {
    if (!w)
        return false;
    if (!lv_byte_writer_ensure(w, 2))
        return false;
    lv_store_le16(w->buf + w->pos, v);
    w->pos += 2;
    return true;
}

bool lv_byte_writer_write_u16_be(lvByteWriter *w, uint16_t v) {
    if (!w)
        return false;
    if (!lv_byte_writer_ensure(w, 2))
        return false;
    lv_store_be16(w->buf + w->pos, v);
    w->pos += 2;
    return true;
}

bool lv_byte_writer_write_u32_le(lvByteWriter *w, uint32_t v) {
    if (!w)
        return false;
    if (!lv_byte_writer_ensure(w, 4))
        return false;
    lv_store_le32(w->buf + w->pos, v);
    w->pos += 4;
    return true;
}

bool lv_byte_writer_write_u32_be(lvByteWriter *w, uint32_t v) {
    if (!w)
        return false;
    if (!lv_byte_writer_ensure(w, 4))
        return false;
    lv_store_be32(w->buf + w->pos, v);
    w->pos += 4;
    return true;
}

bool lv_byte_writer_write_u64_le(lvByteWriter *w, uint64_t v) {
    if (!w)
        return false;
    if (!lv_byte_writer_ensure(w, 8))
        return false;
    lv_store_le64(w->buf + w->pos, v);
    w->pos += 8;
    return true;
}

bool lv_byte_writer_write_u64_be(lvByteWriter *w, uint64_t v) {
    if (!w)
        return false;
    if (!lv_byte_writer_ensure(w, 8))
        return false;
    lv_store_be64(w->buf + w->pos, v);
    w->pos += 8;
    return true;
}

bool lv_byte_writer_write_bytes(lvByteWriter *w, const void *data, size_t n) {
    if (!w || (n > 0 && !data))
        return false;
    if (!lv_byte_writer_ensure(w, n))
        return false;
    if (n > 0)
        memcpy(w->buf + w->pos, data, n);
    w->pos += n;
    return true;
}

bool lv_byte_writer_write_varint(lvByteWriter *w, uint64_t v) {
    if (!w)
        return false;
    if (v <= 0x7f)
        return lv_byte_writer_write_u8(w, (uint8_t) v);
    if (v <= 0xff) {
        if (!lv_byte_writer_write_u8(w, lv_BYTE_VARINT_U8_MARKER))
            return false;
        return lv_byte_writer_write_u8(w, (uint8_t) v);
    }
    if (v <= 0xffff) {
        if (!lv_byte_writer_write_u8(w, lv_BYTE_VARINT_U16_MARKER))
            return false;
        return lv_byte_writer_write_u16_be(w, (uint16_t) v);
    }
    if (v <= 0xffffffffULL) {
        if (!lv_byte_writer_write_u8(w, lv_BYTE_VARINT_U32_MARKER))
            return false;
        return lv_byte_writer_write_u32_be(w, (uint32_t) v);
    }
    if (!lv_byte_writer_write_u8(w, lv_BYTE_VARINT_U64_MARKER))
        return false;
    return lv_byte_writer_write_u64_be(w, v);
}

bool lv_byte_writer_write_zigzag(lvByteWriter *w, int64_t v) {
    /* 标准 zigzag：v>>63 为算术右移（符号扩展 0/-1），保证 -1 -> 1、1 -> 2 */
    uint64_t z = ((uint64_t) v << 1) ^ (uint64_t) (v >> 63);
    return lv_byte_writer_write_varint(w, z);
}

/* ============================================================
 * lvByteReader
 * ============================================================ */

void lv_byte_reader_init(lvByteReader *r, const uint8_t *buf, size_t size) {
    if (!r)
        return;
    r->buf = buf;
    r->size = size;
    r->pos = 0;
}

size_t lv_byte_reader_remaining(const lvByteReader *r) {
    if (!r)
        return 0;
    return r->size - r->pos;
}

bool lv_byte_reader_read_u8(lvByteReader *r, uint8_t *out) {
    if (!r || !out)
        return false;
    if (r->pos >= r->size)
        return false;
    *out = r->buf[r->pos++];
    return true;
}

bool lv_byte_reader_read_u16_le(lvByteReader *r, uint16_t *out) {
    if (!r || !out)
        return false;
    if (r->size - r->pos < 2)
        return false;
    *out = lv_load_le16(r->buf + r->pos);
    r->pos += 2;
    return true;
}

bool lv_byte_reader_read_u16_be(lvByteReader *r, uint16_t *out) {
    if (!r || !out)
        return false;
    if (r->size - r->pos < 2)
        return false;
    *out = lv_load_be16(r->buf + r->pos);
    r->pos += 2;
    return true;
}

bool lv_byte_reader_read_u32_le(lvByteReader *r, uint32_t *out) {
    if (!r || !out)
        return false;
    if (r->size - r->pos < 4)
        return false;
    *out = lv_load_le32(r->buf + r->pos);
    r->pos += 4;
    return true;
}

bool lv_byte_reader_read_u32_be(lvByteReader *r, uint32_t *out) {
    if (!r || !out)
        return false;
    if (r->size - r->pos < 4)
        return false;
    *out = lv_load_be32(r->buf + r->pos);
    r->pos += 4;
    return true;
}

bool lv_byte_reader_read_u64_le(lvByteReader *r, uint64_t *out) {
    if (!r || !out)
        return false;
    if (r->size - r->pos < 8)
        return false;
    *out = lv_load_le64(r->buf + r->pos);
    r->pos += 8;
    return true;
}

bool lv_byte_reader_read_u64_be(lvByteReader *r, uint64_t *out) {
    if (!r || !out)
        return false;
    if (r->size - r->pos < 8)
        return false;
    *out = lv_load_be64(r->buf + r->pos);
    r->pos += 8;
    return true;
}

bool lv_byte_reader_read_bytes(lvByteReader *r, void *out, size_t n) {
    if (!r || (n > 0 && !out))
        return false;
    if (r->size - r->pos < n)
        return false;
    if (n > 0)
        memcpy(out, r->buf + r->pos, n);
    r->pos += n;
    return true;
}

bool lv_byte_reader_read_varint(lvByteReader *r, uint64_t *out) {
    if (!r || !out)
        return false;
    uint8_t first = 0;
    if (!lv_byte_reader_read_u8(r, &first))
        return false;

    if (first <= 0x7f) {
        *out = first;
        return true;
    }

    switch (first) {
    case lv_BYTE_VARINT_U8_MARKER: {
        uint8_t v = 0;
        if (!lv_byte_reader_read_u8(r, &v))
            return false;
        *out = v;
        return true;
    }
    case lv_BYTE_VARINT_U16_MARKER: {
        uint16_t v = 0;
        if (!lv_byte_reader_read_u16_be(r, &v))
            return false;
        *out = v;
        return true;
    }
    case lv_BYTE_VARINT_U32_MARKER: {
        uint32_t v = 0;
        if (!lv_byte_reader_read_u32_be(r, &v))
            return false;
        *out = v;
        return true;
    }
    case lv_BYTE_VARINT_U64_MARKER: {
        uint64_t v = 0;
        if (!lv_byte_reader_read_u64_be(r, &v))
            return false;
        *out = v;
        return true;
    }
    default:
        /* 0x80..0xFB 为保留标记，视为非法编码 */
        return false;
    }
}

bool lv_byte_reader_read_zigzag(lvByteReader *r, int64_t *out) {
    if (!out)
        return false;
    uint64_t z = 0;
    if (!lv_byte_reader_read_varint(r, &z))
        return false;
    *out = (int64_t) ((z >> 1) ^ (uint64_t) (-(int64_t) (z & 1)));
    return true;
}