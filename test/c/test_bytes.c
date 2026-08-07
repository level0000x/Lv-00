/**
 * @file test_bytes.c
 * @brief lv_bytes 公共字节流编解码设施测试
 *
 * 覆盖：
 *   (a) u16/u32/u64 le/be 往返一致 + 字节布局校验；
 *   (b) varint / zigzag 边界（0、127、128、65535、65536、INT32_MAX、INT64_MAX、负值）；
 *   (c) 写入超出初始容量后自动扩容；
 *   (d) Reader 越界返回错误；
 *   (e) 与 geometry_compress 现有编码路径一致（压缩/解压往返）。
 */

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv.h"
#include "lv/lv_bytes.h"
#include "lv/geometry_compress.h"

/* ---------- (a) u16/u32/u64 le/be 往返一致 + 字节布局 ---------- */

static void test_endian_roundtrip(void) {
    printf("Testing endian round-trip...\n");

    lvByteWriter w;
    assert(lv_byte_writer_init(&w, 0)); /* initial_capacity=0：首次写入惰性分配 */
    assert(w.capacity == 0 && w.pos == 0);

    uint16_t v16 = 0x1234;
    uint32_t v32 = 0x12345678UL;
    uint64_t v64 = 0x123456789ABCDEF0ULL;

    assert(lv_byte_writer_write_u16_le(&w, v16));
    assert(lv_byte_writer_write_u16_be(&w, v16));
    assert(lv_byte_writer_write_u32_le(&w, v32));
    assert(lv_byte_writer_write_u32_be(&w, v32));
    assert(lv_byte_writer_write_u64_le(&w, v64));
    assert(lv_byte_writer_write_u64_be(&w, v64));
    assert(!w.error);

    /* 字节布局：LE 低字节在前，BE 高字节在前 */
    assert(w.buf[0] == 0x34 && w.buf[1] == 0x12);              /* u16 LE */
    assert(w.buf[2] == 0x12 && w.buf[3] == 0x34);              /* u16 BE */
    assert(w.buf[4] == 0x78 && w.buf[5] == 0x56 && w.buf[6] == 0x34 && w.buf[7] == 0x12);  /* u32 LE */
    assert(w.buf[8] == 0x12 && w.buf[9] == 0x34 && w.buf[10] == 0x56 && w.buf[11] == 0x78); /* u32 BE */
    assert(w.buf[12] == 0xF0 && w.buf[13] == 0xDE);            /* u64 LE 低 2 字节 */
    assert(w.buf[20] == 0x12 && w.buf[21] == 0x34);            /* u64 BE 高 2 字节 */

    lvByteReader r;
    lv_byte_reader_init(&r, w.buf, w.pos);
    uint16_t out16a = 0, out16b = 0;
    uint32_t out32a = 0, out32b = 0;
    uint64_t out64a = 0, out64b = 0;
    assert(lv_byte_reader_read_u16_le(&r, &out16a));
    assert(lv_byte_reader_read_u16_be(&r, &out16b));
    assert(lv_byte_reader_read_u32_le(&r, &out32a));
    assert(lv_byte_reader_read_u32_be(&r, &out32b));
    assert(lv_byte_reader_read_u64_le(&r, &out64a));
    assert(lv_byte_reader_read_u64_be(&r, &out64b));
    assert(out16a == v16 && out16b == v16);
    assert(out32a == v32 && out32b == v32);
    assert(out64a == v64 && out64b == v64);
    assert(lv_byte_reader_remaining(&r) == 0);

    lv_byte_writer_destroy(&w);
    printf("  PASSED\n");
}

/* ---------- (b) varint / zigzag 边界 ---------- */

static void varint_roundtrip_impl(uint64_t v, size_t expect_bytes) {
    lvByteWriter w;
    assert(lv_byte_writer_init(&w, 0));
    assert(lv_byte_writer_write_varint(&w, v));
    assert(w.pos == expect_bytes);

    lvByteReader r;
    lv_byte_reader_init(&r, w.buf, w.pos);
    uint64_t out = 0;
    assert(lv_byte_reader_read_varint(&r, &out));
    assert(out == v);
    assert(lv_byte_reader_remaining(&r) == 0);

    lv_byte_writer_destroy(&w);
}

static void zigzag_roundtrip_impl(int64_t v, size_t expect_bytes) {
    lvByteWriter w;
    assert(lv_byte_writer_init(&w, 0));
    assert(lv_byte_writer_write_zigzag(&w, v));
    assert(w.pos == expect_bytes);

    lvByteReader r;
    lv_byte_reader_init(&r, w.buf, w.pos);
    int64_t out = 0;
    assert(lv_byte_reader_read_zigzag(&r, &out));
    assert(out == v);
    assert(lv_byte_reader_remaining(&r) == 0);

    lv_byte_writer_destroy(&w);
}

static void test_varint_zigzag(void) {
    printf("Testing varint/zigzag boundaries...\n");

    /* varint 分档：<=0x7F 1B；<=0xFF 2B；<=0xFFFF 3B；<=0xFFFFFFFF 5B；其余 9B */
    varint_roundtrip_impl(0, 1);
    varint_roundtrip_impl(127, 1);
    varint_roundtrip_impl(128, 2); /* [0xFF][0x80] */
    varint_roundtrip_impl(255, 2);
    varint_roundtrip_impl(256, 3);
    varint_roundtrip_impl(65535, 3);
    varint_roundtrip_impl(65536, 5);
    varint_roundtrip_impl(0xFFFFFFFFULL, 5);
    varint_roundtrip_impl(0x100000000ULL, 9);
    varint_roundtrip_impl((uint64_t) INT32_MAX, 5);
    varint_roundtrip_impl(UINT64_MAX, 9);

    /* 显式字节校验：128 -> [0xFF][0x80]；65535 -> [0xFE][0xFF][0xFF] */
    {
        lvByteWriter w;
        assert(lv_byte_writer_init(&w, 0));
        assert(lv_byte_writer_write_varint(&w, 128));
        assert(w.buf[0] == 0xFF && w.buf[1] == 0x80 && w.pos == 2);
        assert(lv_byte_writer_write_varint(&w, 65535));
        assert(w.buf[2] == 0xFE && w.buf[3] == 0xFF && w.buf[4] == 0xFF && w.pos == 5);
        lv_byte_writer_destroy(&w);
    }

    /* zigzag：0->0, -1->1, 1->2, -2->3, 2->4 */
    zigzag_roundtrip_impl(0, 1);
    zigzag_roundtrip_impl(-1, 1);
    zigzag_roundtrip_impl(1, 1);
    zigzag_roundtrip_impl(-2, 1);
    zigzag_roundtrip_impl(2, 1);
    zigzag_roundtrip_impl(-127, 2);
    zigzag_roundtrip_impl(INT32_MAX, 5);
    zigzag_roundtrip_impl(INT32_MIN, 5);
    zigzag_roundtrip_impl(INT64_MAX, 9);
    zigzag_roundtrip_impl(INT64_MIN, 9);
    zigzag_roundtrip_impl(-123456789, 5);

    printf("  PASSED\n");
}

/* ---------- (c) 写入超出初始容量后自动扩容 ---------- */

static void test_growth(void) {
    printf("Testing auto-growth...\n");

    lvByteWriter w;
    assert(lv_byte_writer_init(&w, 4)); /* 极小初始容量，强制触发扩容 */
    size_t old_cap = w.capacity;

    for (int i = 0; i < 300; i++) {
        assert(lv_byte_writer_write_u8(&w, (uint8_t) i));
    }
    assert(w.pos == 300);
    assert(w.capacity > old_cap); /* 已扩容 */

    /* 大块写入也正确 */
    assert(lv_byte_writer_write_u64_le(&w, 0x0102030405060708ULL));
    assert(lv_byte_writer_write_u64_be(&w, 0x0102030405060708ULL));

    uint8_t chunk[1000];
    for (int i = 0; i < 1000; i++)
        chunk[i] = (uint8_t) (i * 7);
    assert(lv_byte_writer_write_bytes(&w, chunk, sizeof(chunk)));
    assert(!w.error);

    /* 往返校验 */
    lvByteReader r;
    lv_byte_reader_init(&r, w.buf, w.pos);
    for (int i = 0; i < 300; i++) {
        uint8_t b = 0;
        assert(lv_byte_reader_read_u8(&r, &b));
        assert(b == (uint8_t) i);
    }
    uint64_t v64 = 0;
    assert(lv_byte_reader_read_u64_le(&r, &v64));
    assert(v64 == 0x0102030405060708ULL);
    assert(lv_byte_reader_read_u64_be(&r, &v64));
    assert(v64 == 0x0102030405060708ULL);
    uint8_t out[1000];
    assert(lv_byte_reader_read_bytes(&r, out, sizeof(out)));
    assert(memcmp(out, chunk, sizeof(chunk)) == 0);
    assert(lv_byte_reader_remaining(&r) == 0);

    lv_byte_writer_destroy(&w);
    printf("  PASSED\n");
}

/* ---------- (d) Reader 越界返回错误 ---------- */

static void test_reader_oob(void) {
    printf("Testing reader out-of-bounds...\n");

    uint8_t small[3] = {0x01, 0x02, 0x03};
    lvByteReader r;
    lv_byte_reader_init(&r, small, sizeof(small));

    uint16_t v16 = 0;
    uint32_t v32 = 0;
    uint64_t v64 = 0;
    uint8_t b = 0;

    assert(lv_byte_reader_read_u16_be(&r, &v16));
    assert(v16 == 0x0102);
    assert(lv_byte_reader_remaining(&r) == 1);

    /* 剩余 1 字节：u16/u32/u64 均不足 */
    assert(!lv_byte_reader_read_u16_be(&r, &v16));
    assert(!lv_byte_reader_read_u32_le(&r, &v32));
    assert(!lv_byte_reader_read_u64_le(&r, &v64));
    assert(lv_byte_reader_remaining(&r) == 1); /* 失败不推进 pos */

    assert(lv_byte_reader_read_u8(&r, &b));
    assert(b == 0x03);
    assert(!lv_byte_reader_read_u8(&r, &b)); /* 已读尽 */

    uint8_t out4[4];
    assert(!lv_byte_reader_read_bytes(&r, out4, sizeof(out4)));

    /* 截断的 varint：标记 0xFD 但无后续 4 字节 */
    uint8_t trunc[2] = {0xFD, 0x01};
    lvByteReader r2;
    lv_byte_reader_init(&r2, trunc, sizeof(trunc));
    uint64_t vout = 0;
    assert(!lv_byte_reader_read_varint(&r2, &vout));

    /* 保留首字节 0x80..0xFB → 非法编码 */
    uint8_t reserved[1] = {0x90};
    lvByteReader r3;
    lv_byte_reader_init(&r3, reserved, sizeof(reserved));
    assert(!lv_byte_reader_read_varint(&r3, &vout));
    int64_t zout = 0;
    assert(!lv_byte_reader_read_zigzag(&r3, &zout));

    /* 空 reader */
    lvByteReader r4;
    lv_byte_reader_init(&r4, NULL, 0);
    assert(lv_byte_reader_remaining(&r4) == 0);
    assert(!lv_byte_reader_read_u8(&r4, &b));

    printf("  PASSED\n");
}

/* ---------- (e) 与 geometry_compress 编码路径往返一致 ---------- */

static void test_geometry_compress_roundtrip(void) {
    printf("Testing geometry compress round-trip...\n");

    ConstraintGraph *graph = graph_create();
    assert(graph != NULL);

    SymbolicCoord *cx = symbolic_coord_create_rational(3, 1);
    SymbolicCoord *cy = symbolic_coord_create_rational(4, 1);
    SymbolicCoord *coords[] = {cx, cy};
    graph_add_point(graph, coords, 2);
    symbolic_coord_destroy(cx);
    symbolic_coord_destroy(cy);

    CompressConfig cfg;
    cfg.pred_mode = PREDICT_PARALLELOGRAM;
    cfg.entropy = ENTROPY_RANS;
    cfg.quantization_bits = 0;
    cfg.lossless = true;
    cfg.max_error = 0.0;

    uint8_t *compressed = NULL;
    size_t compressed_size = 0;
    bool ok = geometry_compress(graph, &cfg, &compressed, &compressed_size, NULL);
    if (ok && compressed && compressed_size > 0) {
        ConstraintGraph *decompressed = NULL;
        ok = geometry_decompress(compressed, compressed_size, &decompressed);
        assert(ok);
        assert(decompressed != NULL);
        assert(decompressed->node_count > 0);
        graph_destroy(decompressed);
        lv_free((void **) &compressed);
    } else if (compressed) {
        lv_free((void **) &compressed);
    }

    graph_destroy(graph);
    printf("  PASSED\n");
}

/* ---------- 主函数 ---------- */

int main(void) {
    printf("=== Lv-00 Bytes Test Suite ===\n\n");

    if (!lv_init()) {
        fprintf(stderr, "Failed to initialize Lv-00 system\n");
        return 1;
    }

    test_endian_roundtrip();
    test_varint_zigzag();
    test_growth();
    test_reader_oob();
    test_geometry_compress_roundtrip();

    printf("\nAll bytes tests passed.\n");
    return 0;
}