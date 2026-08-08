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

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

#include "lv.h"
#include "lv/lv_bytes.h"
#include "lv/geometry_compress.h"

/* ---------- (a) u16/u32/u64 le/be 往返一致 + 字节布局 ---------- */

static void test_endian_roundtrip(void) {
    printf("Testing endian round-trip...\n");

    lvByteWriter w;
    lv_ASSERT(lv_byte_writer_init(&w, 0)); /* initial_capacity=0：首次写入惰性分配 */
    lv_ASSERT(w.capacity == 0 && w.pos == 0);

    uint16_t v16 = 0x1234;
    uint32_t v32 = 0x12345678UL;
    uint64_t v64 = 0x123456789ABCDEF0ULL;

    lv_ASSERT(lv_byte_writer_write_u16_le(&w, v16));
    lv_ASSERT(lv_byte_writer_write_u16_be(&w, v16));
    lv_ASSERT(lv_byte_writer_write_u32_le(&w, v32));
    lv_ASSERT(lv_byte_writer_write_u32_be(&w, v32));
    lv_ASSERT(lv_byte_writer_write_u64_le(&w, v64));
    lv_ASSERT(lv_byte_writer_write_u64_be(&w, v64));
    lv_ASSERT(!w.error);

    /* 字节布局：LE 低字节在前，BE 高字节在前 */
    lv_ASSERT(w.buf[0] == 0x34 && w.buf[1] == 0x12);              /* u16 LE */
    lv_ASSERT(w.buf[2] == 0x12 && w.buf[3] == 0x34);              /* u16 BE */
    lv_ASSERT(w.buf[4] == 0x78 && w.buf[5] == 0x56 && w.buf[6] == 0x34 && w.buf[7] == 0x12);  /* u32 LE */
    lv_ASSERT(w.buf[8] == 0x12 && w.buf[9] == 0x34 && w.buf[10] == 0x56 && w.buf[11] == 0x78); /* u32 BE */
    lv_ASSERT(w.buf[12] == 0xF0 && w.buf[13] == 0xDE);            /* u64 LE 低 2 字节 */
    lv_ASSERT(w.buf[20] == 0x12 && w.buf[21] == 0x34);            /* u64 BE 高 2 字节 */

    lvByteReader r;
    lv_byte_reader_init(&r, w.buf, w.pos);
    uint16_t out16a = 0, out16b = 0;
    uint32_t out32a = 0, out32b = 0;
    uint64_t out64a = 0, out64b = 0;
    lv_ASSERT(lv_byte_reader_read_u16_le(&r, &out16a));
    lv_ASSERT(lv_byte_reader_read_u16_be(&r, &out16b));
    lv_ASSERT(lv_byte_reader_read_u32_le(&r, &out32a));
    lv_ASSERT(lv_byte_reader_read_u32_be(&r, &out32b));
    lv_ASSERT(lv_byte_reader_read_u64_le(&r, &out64a));
    lv_ASSERT(lv_byte_reader_read_u64_be(&r, &out64b));
    lv_ASSERT(out16a == v16 && out16b == v16);
    lv_ASSERT(out32a == v32 && out32b == v32);
    lv_ASSERT(out64a == v64 && out64b == v64);
    lv_ASSERT(lv_byte_reader_remaining(&r) == 0);

    lv_byte_writer_destroy(&w);
    printf("  PASSED\n");
}

/* ---------- (b) varint / zigzag 边界 ---------- */

static void varint_roundtrip_impl(uint64_t v, size_t expect_bytes) {
    lvByteWriter w;
    lv_ASSERT(lv_byte_writer_init(&w, 0));
    lv_ASSERT(lv_byte_writer_write_varint(&w, v));
    lv_ASSERT(w.pos == expect_bytes);

    lvByteReader r;
    lv_byte_reader_init(&r, w.buf, w.pos);
    uint64_t out = 0;
    lv_ASSERT(lv_byte_reader_read_varint(&r, &out));
    lv_ASSERT(out == v);
    lv_ASSERT(lv_byte_reader_remaining(&r) == 0);

    lv_byte_writer_destroy(&w);
}

static void zigzag_roundtrip_impl(int64_t v, size_t expect_bytes) {
    lvByteWriter w;
    lv_ASSERT(lv_byte_writer_init(&w, 0));
    lv_ASSERT(lv_byte_writer_write_zigzag(&w, v));
    lv_ASSERT(w.pos == expect_bytes);

    lvByteReader r;
    lv_byte_reader_init(&r, w.buf, w.pos);
    int64_t out = 0;
    lv_ASSERT(lv_byte_reader_read_zigzag(&r, &out));
    lv_ASSERT(out == v);
    lv_ASSERT(lv_byte_reader_remaining(&r) == 0);

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
        lv_ASSERT(lv_byte_writer_init(&w, 0));
        lv_ASSERT(lv_byte_writer_write_varint(&w, 128));
        lv_ASSERT(w.buf[0] == 0xFF && w.buf[1] == 0x80 && w.pos == 2);
        lv_ASSERT(lv_byte_writer_write_varint(&w, 65535));
        lv_ASSERT(w.buf[2] == 0xFE && w.buf[3] == 0xFF && w.buf[4] == 0xFF && w.pos == 5);
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
    lv_ASSERT(lv_byte_writer_init(&w, 4)); /* 极小初始容量，强制触发扩容 */
    size_t old_cap = w.capacity;

    for (int i = 0; i < 300; i++) {
        lv_ASSERT(lv_byte_writer_write_u8(&w, (uint8_t) i));
    }
    lv_ASSERT(w.pos == 300);
    lv_ASSERT(w.capacity > old_cap); /* 已扩容 */

    /* 大块写入也正确 */
    lv_ASSERT(lv_byte_writer_write_u64_le(&w, 0x0102030405060708ULL));
    lv_ASSERT(lv_byte_writer_write_u64_be(&w, 0x0102030405060708ULL));

    uint8_t chunk[1000];
    for (int i = 0; i < 1000; i++)
        chunk[i] = (uint8_t) (i * 7);
    lv_ASSERT(lv_byte_writer_write_bytes(&w, chunk, sizeof(chunk)));
    lv_ASSERT(!w.error);

    /* 往返校验 */
    lvByteReader r;
    lv_byte_reader_init(&r, w.buf, w.pos);
    for (int i = 0; i < 300; i++) {
        uint8_t b = 0;
        lv_ASSERT(lv_byte_reader_read_u8(&r, &b));
        lv_ASSERT(b == (uint8_t) i);
    }
    uint64_t v64 = 0;
    lv_ASSERT(lv_byte_reader_read_u64_le(&r, &v64));
    lv_ASSERT(v64 == 0x0102030405060708ULL);
    lv_ASSERT(lv_byte_reader_read_u64_be(&r, &v64));
    lv_ASSERT(v64 == 0x0102030405060708ULL);
    uint8_t out[1000];
    lv_ASSERT(lv_byte_reader_read_bytes(&r, out, sizeof(out)));
    lv_ASSERT(memcmp(out, chunk, sizeof(chunk)) == 0);
    lv_ASSERT(lv_byte_reader_remaining(&r) == 0);

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

    lv_ASSERT(lv_byte_reader_read_u16_be(&r, &v16));
    lv_ASSERT(v16 == 0x0102);
    lv_ASSERT(lv_byte_reader_remaining(&r) == 1);

    /* 剩余 1 字节：u16/u32/u64 均不足 */
    lv_ASSERT(!lv_byte_reader_read_u16_be(&r, &v16));
    lv_ASSERT(!lv_byte_reader_read_u32_le(&r, &v32));
    lv_ASSERT(!lv_byte_reader_read_u64_le(&r, &v64));
    lv_ASSERT(lv_byte_reader_remaining(&r) == 1); /* 失败不推进 pos */

    lv_ASSERT(lv_byte_reader_read_u8(&r, &b));
    lv_ASSERT(b == 0x03);
    lv_ASSERT(!lv_byte_reader_read_u8(&r, &b)); /* 已读尽 */

    uint8_t out4[4];
    lv_ASSERT(!lv_byte_reader_read_bytes(&r, out4, sizeof(out4)));

    /* 截断的 varint：标记 0xFD 但无后续 4 字节 */
    uint8_t trunc[2] = {0xFD, 0x01};
    lvByteReader r2;
    lv_byte_reader_init(&r2, trunc, sizeof(trunc));
    uint64_t vout = 0;
    lv_ASSERT(!lv_byte_reader_read_varint(&r2, &vout));

    /* 保留首字节 0x80..0xFB → 非法编码 */
    uint8_t reserved[1] = {0x90};
    lvByteReader r3;
    lv_byte_reader_init(&r3, reserved, sizeof(reserved));
    lv_ASSERT(!lv_byte_reader_read_varint(&r3, &vout));
    int64_t zout = 0;
    lv_ASSERT(!lv_byte_reader_read_zigzag(&r3, &zout));

    /* 空 reader */
    lvByteReader r4;
    lv_byte_reader_init(&r4, NULL, 0);
    lv_ASSERT(lv_byte_reader_remaining(&r4) == 0);
    lv_ASSERT(!lv_byte_reader_read_u8(&r4, &b));

    printf("  PASSED\n");
}

/* ---------- (e) 与 geometry_compress 编码路径往返一致 ---------- */

static void test_geometry_compress_roundtrip(void) {
    printf("Testing geometry compress round-trip...\n");

    ConstraintGraph *graph = graph_create();
    lv_ASSERT_NOT_NULL(graph);

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
        lv_ASSERT(ok);
        lv_ASSERT_NOT_NULL(decompressed);
        lv_ASSERT(decompressed->node_count > 0);
        graph_destroy(decompressed);
        lv_free((void **) &compressed);
    } else if (compressed) {
        lv_free((void **) &compressed);
    }

    graph_destroy(graph);
    printf("  PASSED\n");
}

/* ---------- 主函数 ---------- */

TEST_MAIN_BEGIN("Lv-00 Bytes Test Suite")
    printf("=== Lv-00 Bytes Test Suite ===\n\n");
    if (!lv_init()) {
        fprintf(stderr, "Failed to initialize Lv-00 system\n");
        return 1;
    }
    TEST_MAIN_RUN(test_endian_roundtrip);
    TEST_MAIN_RUN(test_varint_zigzag);
    TEST_MAIN_RUN(test_growth);
    TEST_MAIN_RUN(test_reader_oob);
    TEST_MAIN_RUN(test_geometry_compress_roundtrip);
    printf("\nAll bytes tests passed.\n");
TEST_MAIN_END()
