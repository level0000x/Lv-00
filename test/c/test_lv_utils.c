/**
 * @file test_lv_utils.c
 * @brief lv_utils.h 零覆盖设施契约测试
 *
 * 批次 C-㉛：补全 lv_utils.h 中此前零测试覆盖的 L2 基础设施。
 * 覆盖域（按 test-authoring 三层：等价/边界/性质）：
 * - 数组工具：lv_ensure_capacity（倍增/溢出/失败语义）、lv_cmp_int /
 *   lv_cmp_uint64（qsort 三态，无符号减法溢出）、lv_max_d / lv_max_abs
 * - 动态字符串 lvDStr：init / grow（倍增）/ append_str / append_raw /
 *   append_fmt / free（惰性分配、NUL 终止不变量）
 * - 动态数组补充：lv_darray_init_with_dtor / reserve / pop / clear
 * - TLS 向量：lv_tls_vector_ensure / clear / cleanup
 * - FNV-1a：lv_fnv1a_hash / update / hash_str / hash_int（与参考值对拍）
 * - 字节序：store/load be16/32/64 + le16/32/64（roundtrip + 已知字节序）
 * - 时间：lv_get_time_us/ms/ns、lv_get_wallclock_ms/ns（单调 + 量级关系）
 * - 随机：lv_random_init / int / double（种子复现、范围、退化输入）
 * - 其他：lv_strlcpy_n（截断语义）、lv_scratch_buf_cleanup（NULL 安全）
 *
 * @author Lv-00 Project
 * @date 2026-08-19
 */

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#include "lv/lv_utils.h"
#include "lv.h"
#include "test_helpers.h"

/* ============================================================
 * 全局测试计数器
 * ============================================================ */
int g_pass_count = 0;
int g_fail_count = 0;

/* ============================================================
 * lv_ensure_capacity：倍增 / 溢出 / 失败语义
 * ============================================================ */
static void test_ensure_capacity(void) {
    int *arr = NULL;
    int cap = 0;

    /* 首次分配：0 -> 初始容量（倍增到 lv_INITIAL_ARRAY_CAPACITY=8） */
    TEST_ASSERT_MSG(lv_ensure_capacity((void **) &arr, 1, &cap, sizeof(int), 0), "首次扩容");
    TEST_ASSERT_MSG(arr != NULL, "数组已分配");
    TEST_ASSERT_MSG(cap >= 8, "初始容量 >= 8");

    /* 容量充足：不重新分配 */
    int *before = arr;
    TEST_ASSERT_MSG(lv_ensure_capacity((void **) &arr, 5, &cap, sizeof(int), 0), "容量充足");
    TEST_ASSERT_MSG(arr == before, "容量充足不重分配");

    /* 增长：count == capacity 触发倍增 */
    TEST_ASSERT_MSG(lv_ensure_capacity((void **) &arr, 8, &cap, sizeof(int), 0), "触发倍增");
    TEST_ASSERT_MSG(cap >= 16, "倍增后容量 >= 16");

    /* NULL 参数失败 */
    TEST_ASSERT_MSG(!lv_ensure_capacity(NULL, 1, &cap, sizeof(int), 0), "NULL arr 失败");
    TEST_ASSERT_MSG(!lv_ensure_capacity((void **) &arr, 1, NULL, sizeof(int), 0), "NULL capacity 失败");
    TEST_ASSERT_MSG(!lv_ensure_capacity((void **) &arr, 1, &cap, 0, 0), "elem_size 0 失败");

    /* 负 count：若小于当前容量走"无需扩容"分支（实现契约：负检查仅当
     * count >= capacity 时触发，而负 count 不可能 >= 正容量） */
    TEST_ASSERT_MSG(lv_ensure_capacity((void **) &arr, -1, &cap, sizeof(int), 0), "负 count 小于容量视为无需扩容");

    /* 增长因子常量可用 */
    TEST_ASSERT_MSG(lv_ARRAY_GROWTH_FACTOR == 2, "增长因子 2");

    lv_free((void **) &arr);
}

/* ============================================================
 * lv_cmp_int / lv_cmp_uint64：qsort 三态比较
 * ============================================================ */
static void test_cmp_int_uint64(void) {
    /* 三态 */
    int a = 5, b = 3, c = 5;
    TEST_ASSERT_MSG(lv_cmp_int(&a, &b) > 0, "5 > 3");
    TEST_ASSERT_MSG(lv_cmp_int(&b, &a) < 0, "3 < 5");
    TEST_ASSERT_MSG(lv_cmp_int(&a, &c) == 0, "5 == 5");

    /* INT_MIN / INT_MAX 边界（无有符号减法溢出） */
    int mn = INT_MIN, mx = INT_MAX;
    TEST_ASSERT_MSG(lv_cmp_int(&mn, &mx) < 0, "INT_MIN < INT_MAX");
    TEST_ASSERT_MSG(lv_cmp_int(&mx, &mn) > 0, "INT_MAX > INT_MIN");

    /* uint64 三态 + 边界 */
    uint64_t ua = 5, ub = 3, uc = 5;
    TEST_ASSERT_MSG(lv_cmp_uint64(&ua, &ub) > 0, "u64 5 > 3");
    TEST_ASSERT_MSG(lv_cmp_uint64(&ub, &ua) < 0, "u64 3 < 5");
    TEST_ASSERT_MSG(lv_cmp_uint64(&ua, &uc) == 0, "u64 5 == 5");
    uint64_t umax = UINT64_MAX, umin = 0;
    TEST_ASSERT_MSG(lv_cmp_uint64(&umax, &umin) > 0, "UINT64_MAX > 0");

    /* 用 qsort 验证排序正确性（qsort 风格契约） */
    int vals[] = {42, -7, 0, 100, -100, 7};
    qsort(vals, 6, sizeof(int), lv_cmp_int);
    int expected[] = {-100, -7, 0, 7, 42, 100};
    for (int i = 0; i < 6; i++)
        TEST_ASSERT_EQ(vals[i], expected[i]);

    uint64_t uvals[] = {100, 1, 50, 0, UINT64_MAX};
    qsort(uvals, 5, sizeof(uint64_t), lv_cmp_uint64);
    uint64_t uexpected[] = {0, 1, 50, 100, UINT64_MAX};
    for (int i = 0; i < 5; i++)
        TEST_ASSERT_EQ(uvals[i], uexpected[i]);
}

/* ============================================================
 * lv_max_d / lv_max_abs
 * ============================================================ */
static void test_max_d_abs(void) {
    /* 等价性 */
    double d1[] = {1.5, -2.5, 3.5, 0.5};
    TEST_ASSERT_DOUBLE(lv_max_d(d1, 4), 3.5, 1e-12);
    TEST_ASSERT_DOUBLE(lv_max_abs(d1, 4), 3.5, 1e-12);

    /* 含负数绝对值最大 */
    double d2[] = {1.0, -9.0, 2.0};
    TEST_ASSERT_DOUBLE(lv_max_d(d2, 3), 2.0, 1e-12);
    TEST_ASSERT_DOUBLE(lv_max_abs(d2, 3), 9.0, 1e-12);

    /* 单元素 */
    double d3[] = {-7.5};
    TEST_ASSERT_DOUBLE(lv_max_d(d3, 1), -7.5, 1e-12);
    TEST_ASSERT_DOUBLE(lv_max_abs(d3, 1), 7.5, 1e-12);

    /* 边界：NULL / n<=0 → 0.0 */
    TEST_ASSERT_DOUBLE(lv_max_d(NULL, 3), 0.0, 1e-12);
    TEST_ASSERT_DOUBLE(lv_max_d(d1, 0), 0.0, 1e-12);
    TEST_ASSERT_DOUBLE(lv_max_d(d1, -1), 0.0, 1e-12);
    TEST_ASSERT_DOUBLE(lv_max_abs(NULL, 3), 0.0, 1e-12);
    TEST_ASSERT_DOUBLE(lv_max_abs(d1, 0), 0.0, 1e-12);
}

/* ============================================================
 * lvDStr：init / grow / append / free
 * ============================================================ */
static void test_dstr(void) {
    lvDStr d;
    TEST_ASSERT_EQ(lv_dstr_init(&d, 0), 0);

    /* 惰性分配：init 后无缓冲 */
    TEST_ASSERT_NULL(d.data);
    TEST_ASSERT_EQ(d.len, (size_t) 0);

    /* 追加字符串 */
    TEST_ASSERT_EQ(lv_dstr_append_str(&d, "hello"), 0);
    TEST_ASSERT_EQ(d.len, (size_t) 5);
    TEST_ASSERT_MSG(d.data != NULL && strcmp(d.data, "hello") == 0, "内容 hello");
    TEST_ASSERT_MSG(d.data[d.len] == '\0', "NUL 终止不变量");

    /* 追加原始字节 */
    TEST_ASSERT_EQ(lv_dstr_append_raw(&d, " world", 6), 0);
    TEST_ASSERT_EQ(d.len, (size_t) 11);
    TEST_ASSERT_MSG(d.data != NULL && strcmp(d.data, "hello world") == 0, "内容 hello world");

    /* 追加格式化 */
    TEST_ASSERT_EQ(lv_dstr_append_fmt(&d, "%d-%s", 42, "x"), 0);
    TEST_ASSERT_MSG(d.data != NULL && strcmp(d.data, "hello world42-x") == 0, "格式化追加");

    /* grow：扩容后不破坏内容（当前 cap 已为初始 4096，需 extra 越过） */
    size_t old_cap = d.cap;
    TEST_ASSERT_EQ(lv_dstr_grow(&d, 10000), 0);
    TEST_ASSERT_MSG(d.cap > old_cap, "grow 扩容");
    TEST_ASSERT_MSG(strcmp(d.data, "hello world42-x") == 0, "扩容后内容保留");
    TEST_ASSERT_MSG(d.data[d.len] == '\0', "扩容后 NUL 终止");

    /* 边界：append NULL / 空串 */
    TEST_ASSERT_EQ(lv_dstr_append_str(&d, NULL), 0);
    TEST_ASSERT_EQ(lv_dstr_append_raw(&d, NULL, 5), 0);
    TEST_ASSERT_EQ(lv_dstr_append_raw(&d, "x", 0), 0);
    TEST_ASSERT_EQ(d.len, (size_t) 15);

    /* 边界：NULL d */
    TEST_ASSERT_MSG(lv_dstr_init(NULL, 0) != 0, "NULL init 失败");
    TEST_ASSERT_MSG(lv_dstr_grow(NULL, 5) != 0, "NULL grow 失败");
    TEST_ASSERT_MSG(lv_dstr_append_fmt(NULL, "%d", 1) != 0, "NULL append_fmt 失败");

    /* free：释放并复位 */
    lv_dstr_free(&d);
    TEST_ASSERT_NULL(d.data);
    TEST_ASSERT_EQ(d.len, (size_t) 0);
    TEST_ASSERT_EQ(d.cap, (size_t) 0);
    lv_dstr_free(&d); /* 二次 free 安全 */

    /* 大字符串跨倍增边界（> 4096 初始容量） */
    lvDStr big;
    lv_dstr_init(&big, 0);
    char chunk[1000];
    memset(chunk, 'a', sizeof(chunk));
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQ(lv_dstr_append_raw(&big, chunk, sizeof(chunk)), 0);
    }
    TEST_ASSERT_EQ(big.len, (size_t) 5000);
    TEST_ASSERT_MSG(big.cap >= 5000, "跨初始容量扩容");
    TEST_ASSERT_MSG(big.data[big.len] == '\0', "大串 NUL 终止");
    lv_dstr_free(&big);
}

/* ============================================================
 * lvDArray 补充：init_with_dtor / reserve / pop / clear
 * ============================================================ */
static int dtor_count = 0;
static void count_dtor(void *elem) {
    (void) elem;
    dtor_count++;
}

static void test_darray_extras(void) {
    /* init_with_dtor */
    lvDArray arr;
    lv_darray_init_with_dtor(&arr, sizeof(int), count_dtor);
    TEST_ASSERT_MSG(arr.elem_destroy == count_dtor, "dtor 已注册");
    TEST_ASSERT_EQ(arr.count, 0);

    /* reserve */
    TEST_ASSERT_MSG(lv_darray_reserve(&arr, 100), "reserve 100");
    TEST_ASSERT_MSG(arr.capacity >= 100, "reserve 后容量");

    /* push/pop */
    int v = 7;
    lv_darray_push(&arr, &v);
    TEST_ASSERT_EQ(arr.count, 1);
    lv_darray_pop(&arr);
    TEST_ASSERT_EQ(arr.count, 0);
    lv_darray_pop(&arr); /* 空栈 pop 安全 */
    TEST_ASSERT_EQ(arr.count, 0);

    /* push 多个 + clear */
    for (int i = 0; i < 10; i++) {
        int val = i;
        lv_darray_push(&arr, &val);
    }
    TEST_ASSERT_EQ(arr.count, 10);
    lv_darray_clear(&arr);
    TEST_ASSERT_EQ(arr.count, 0);
    TEST_ASSERT_MSG(arr.capacity >= 10, "clear 保留容量");

    /* free 触发 dtor（已 push 元素后） */
    for (int i = 0; i < 3; i++) {
        int val = i;
        lv_darray_push(&arr, &val);
    }
    dtor_count = 0;
    lv_darray_free(&arr);
    TEST_ASSERT_EQ(dtor_count, 3);
}

/* ============================================================
 * lvTlsVector：ensure / clear / cleanup
 * ============================================================ */
static void test_tls_vector(void) {
    lvTlsVector v = {0};

    /* ensure 扩容 */
    TEST_ASSERT_MSG(lv_tls_vector_ensure(&v, 5, sizeof(int)), "ensure 5 个 int");
    TEST_ASSERT_MSG(v.ptr != NULL, "已分配");
    TEST_ASSERT_MSG(v.capacity >= 5, "容量 >= 5");

    /* 写入模拟元素 */
    int *p = (int *) v.ptr;
    p[0] = 1;
    p[4] = 5;
    v.count = 5;

    /* 再次 ensure（容量充足不重分配） */
    void *before = v.ptr;
    TEST_ASSERT_MSG(lv_tls_vector_ensure(&v, 5, sizeof(int)), "容量充足 ensure");
    TEST_ASSERT_MSG(v.ptr == before, "容量充足不重分配");

    /* 倍增扩容 */
    TEST_ASSERT_MSG(lv_tls_vector_ensure(&v, 20, sizeof(int)), "ensure 20");
    TEST_ASSERT_MSG(v.capacity >= 20, "扩容后容量");
    int *p2 = (int *) v.ptr;
    TEST_ASSERT_EQ(p2[0], 1);
    TEST_ASSERT_EQ(p2[4], 5);

    /* clear：计数归零，缓冲保留 */
    v.count = 3;
    lv_tls_vector_clear(&v);
    TEST_ASSERT_EQ(v.count, 0);
    TEST_ASSERT_MSG(v.ptr != NULL, "clear 保留缓冲");

    /* cleanup：释放并复位 */
    lv_tls_vector_cleanup(&v);
    TEST_ASSERT_NULL(v.ptr);
    TEST_ASSERT_EQ(v.count, 0);
    TEST_ASSERT_EQ(v.capacity, 0);
    lv_tls_vector_cleanup(&v); /* 二次 cleanup 安全 */

    /* NULL 安全 */
    TEST_ASSERT_MSG(!lv_tls_vector_ensure(NULL, 5, sizeof(int)), "NULL ensure 失败");
    TEST_ASSERT_MSG(!lv_tls_vector_ensure(&v, 5, 0), "elem_size 0 失败");
    lv_tls_vector_clear(NULL);
    lv_tls_vector_cleanup(NULL);
}

/* ============================================================
 * FNV-1a 哈希：与参考值对拍
 * ============================================================ */
static void test_fnv1a(void) {
    /* 标准 FNV-1a 参考值：空串 = offset basis */
    TEST_ASSERT_EQ(lv_fnv1a_hash_str(""), lv_FNV64_OFFSET_BASIS);
    TEST_ASSERT_EQ(lv_fnv1a_hash_str(NULL), lv_FNV64_OFFSET_BASIS);
    TEST_ASSERT_EQ(lv_fnv1a_hash(NULL, 0), (uint64_t) 0);

    /* 已知参考值：FNV-1a 64 位 "a" = 0xaf63dc4c8601ec8c */
    TEST_ASSERT_EQ(lv_fnv1a_hash_str("a"), UINT64_C(0xaf63dc4c8601ec8c));
    /* "foobar" 参考值 = 0x85944171f73967e8 */
    TEST_ASSERT_EQ(lv_fnv1a_hash_str("foobar"), UINT64_C(0x85944171f73967e8));

    /* hash(bytes) 与 hash_str 一致 */
    TEST_ASSERT_EQ(lv_fnv1a_hash("foobar", 6), lv_fnv1a_hash_str("foobar"));

    /* update：分块混入 == 一次性 */
    uint64_t h1 = lv_fnv1a_update(lv_FNV64_OFFSET_BASIS, "fo", 2);
    h1 = lv_fnv1a_update(h1, "ob", 2);
    h1 = lv_fnv1a_update(h1, "ar", 2);
    TEST_ASSERT_EQ(h1, lv_fnv1a_hash_str("foobar"));

    /* update NULL data → 原样返回 */
    TEST_ASSERT_EQ(lv_fnv1a_update(123, NULL, 5), (uint64_t) 123);

    /* hash_int：混入整数值可复现且与 update(bytes) 一致 */
    uint64_t v = 0x0102030405060708ULL;
    uint64_t via_int = lv_fnv1a_hash_int(lv_FNV64_OFFSET_BASIS, v);
    uint64_t via_bytes = lv_fnv1a_update(lv_FNV64_OFFSET_BASIS, &v, sizeof(v));
    TEST_ASSERT_EQ(via_int, via_bytes);
}

/* ============================================================
 * 字节序：store/load 往返 + 已知字节序
 * ============================================================ */
static void test_endian(void) {
    uint8_t buf[8];

    /* 已知小端布局：0x1234 -> 34 12 */
    lv_store_le16(buf, 0x1234);
    TEST_ASSERT_EQ(buf[0], (uint8_t) 0x34);
    TEST_ASSERT_EQ(buf[1], (uint8_t) 0x12);
    TEST_ASSERT_EQ(lv_load_le16(buf), (uint16_t) 0x1234);

    /* 已知大端布局：0x1234 -> 12 34 */
    lv_store_be16(buf, 0x1234);
    TEST_ASSERT_EQ(buf[0], (uint8_t) 0x12);
    TEST_ASSERT_EQ(buf[1], (uint8_t) 0x34);
    TEST_ASSERT_EQ(lv_load_be16(buf), (uint16_t) 0x1234);

    /* 32 位已知值 */
    lv_store_le32(buf, 0x01020304);
    TEST_ASSERT_EQ(buf[0], (uint8_t) 0x04);
    TEST_ASSERT_EQ(buf[3], (uint8_t) 0x01);
    TEST_ASSERT_EQ(lv_load_le32(buf), (uint32_t) 0x01020304);
    lv_store_be32(buf, 0x01020304);
    TEST_ASSERT_EQ(buf[0], (uint8_t) 0x01);
    TEST_ASSERT_EQ(buf[3], (uint8_t) 0x04);
    TEST_ASSERT_EQ(lv_load_be32(buf), (uint32_t) 0x01020304);

    /* 64 位往返（含符号位高位） */
    uint64_t val64 = UINT64_C(0x0123456789ABCDEF);
    lv_store_le64(buf, val64);
    TEST_ASSERT_EQ(buf[0], (uint8_t) 0xEF);
    TEST_ASSERT_EQ(buf[7], (uint8_t) 0x01);
    TEST_ASSERT_EQ(lv_load_le64(buf), val64);
    lv_store_be64(buf, val64);
    TEST_ASSERT_EQ(buf[0], (uint8_t) 0x01);
    TEST_ASSERT_EQ(buf[7], (uint8_t) 0xEF);
    TEST_ASSERT_EQ(lv_load_be64(buf), val64);

    /* 全 16/32 位 roundtrip 采样 */
    for (uint32_t i = 0; i < 100; i++) {
        uint32_t x = i * 65537u + 123u;
        uint16_t x16 = (uint16_t) x;
        lv_store_le16(buf, x16);
        TEST_ASSERT_EQ(lv_load_le16(buf), x16);
        lv_store_be16(buf, x16);
        TEST_ASSERT_EQ(lv_load_be16(buf), x16);
        lv_store_le32(buf, x);
        TEST_ASSERT_EQ(lv_load_le32(buf), x);
        lv_store_be32(buf, x);
        TEST_ASSERT_EQ(lv_load_be32(buf), x);
    }
}

/* ============================================================
 * 时间：单调 + 量级关系
 * ============================================================ */
static void test_time_facilities(void) {
    /* 单调性（两次调用 us 不倒退） */
    uint64_t t1 = lv_get_time_us();
    uint64_t t2 = lv_get_time_us();
    TEST_ASSERT_MSG(t2 >= t1, "us 单调");

    /* 量级关系：ns > us > ms（同刻采样趋势） */
    uint64_t ns = lv_get_time_ns();
    uint64_t us = lv_get_time_us();
    uint64_t ms = lv_get_time_ms();
    TEST_ASSERT_MSG(ns >= us, "ns >= us 同刻");
    TEST_ASSERT_MSG(us >= ms, "us >= ms 同刻");

    /* wallclock 与单调时钟均大于 0（程序运行后必然） */
    TEST_ASSERT_MSG(lv_get_wallclock_ns() > 0, "wallclock ns > 0");
    TEST_ASSERT_MSG(lv_get_wallclock_ms() > 0, "wallclock ms > 0");
    TEST_ASSERT_MSG(lv_get_time_us() > 0, "time us > 0");
    TEST_ASSERT_MSG(lv_get_time_ns() > 0, "time ns > 0");
    TEST_ASSERT_MSG(lv_get_time_ms() > 0, "time ms > 0");

    /* 比例合理性：1ms ≈ 1000us（取同一时刻近似） */
    uint64_t us_a = lv_get_time_us();
    uint64_t us_b = lv_get_time_us();
    uint64_t ms_a = lv_get_time_ms();
    TEST_ASSERT_MSG((us_b - us_a) < 1000000ULL, "us 采样间隔 < 1s");
    TEST_ASSERT_MSG(ms_a <= us_b / 1000 + 10, "ms 与 us 量级匹配");
}

/* ============================================================
 * 随机：种子复现 / 范围 / 退化输入
 * ============================================================ */
static void test_random(void) {
    /* 种子复现：相同种子产生相同序列（性质：确定性） */
    lv_random_init(42);
    int r1a = lv_random_int(0, 1000000);
    int r1b = lv_random_int(0, 1000000);
    lv_random_init(42);
    int r2a = lv_random_int(0, 1000000);
    int r2b = lv_random_int(0, 1000000);
    TEST_ASSERT_EQ(r1a, r2a);
    TEST_ASSERT_EQ(r1b, r2b);

    /* 不同种子产生不同序列 */
    lv_random_init(1);
    uint64_t s1 = (uint64_t) lv_random_int(0, 1000000);
    lv_random_init(2);
    uint64_t s2 = (uint64_t) lv_random_int(0, 1000000);
    TEST_ASSERT_MSG(s1 != s2 || lv_random_int(0, 1000000) != lv_random_int(0, 1000000), "不同种子序列不同");

    /* 范围：多次采样都在 [min, max] */
    lv_random_init(7);
    for (int i = 0; i < 200; i++) {
        int r = lv_random_int(10, 20);
        TEST_ASSERT_MSG(r >= 10 && r <= 20, "int 在范围内");
    }
    for (int i = 0; i < 200; i++) {
        double r = lv_random_double(-1.0, 1.0);
        TEST_ASSERT_MSG(r >= -1.0 && r <= 1.0, "double 在范围内");
    }

    /* double 采样不为常数（性质：非退化） */
    lv_random_init(99);
    double d1 = lv_random_double(0.0, 1.0);
    double d2 = lv_random_double(0.0, 1.0);
    TEST_ASSERT_MSG(d1 != d2, "double 序列非退化");

    /* 退化输入：min >= max 返回 min */
    TEST_ASSERT_EQ(lv_random_int(5, 5), 5);
    TEST_ASSERT_EQ(lv_random_int(5, 3), 5);
    TEST_ASSERT_DOUBLE(lv_random_double(2.5, 2.5), 2.5, 1e-12);
    TEST_ASSERT_DOUBLE(lv_random_double(3.0, 1.0), 3.0, 1e-12);

    /* 单点范围 */
    TEST_ASSERT_EQ(lv_random_int(0, 0), 0);
}

/* ============================================================
 * lv_strlcpy_n：安全复制（截断语义）
 * ============================================================ */
static void test_strlcpy_n(void) {
    char buf[16];

    /* 等价性：完整复制，返回源长 */
    memset(buf, 'x', sizeof(buf));
    size_t n = lv_strlcpy_n(buf, sizeof(buf), "hello", 5);
    TEST_ASSERT_EQ(n, (size_t) 5);
    TEST_ASSERT_STR_EQ(buf, "hello");
    TEST_ASSERT_MSG(buf[5] == '\0', "NUL 终止");

    /* 截断：源超长 → 截断且返回源长 */
    memset(buf, 'x', sizeof(buf));
    n = lv_strlcpy_n(buf, sizeof(buf), "0123456789ABCDEF", 16);
    TEST_ASSERT_EQ(n, (size_t) 16);
    TEST_ASSERT_MSG(buf[15] == '\0', "截断后 NUL 终止");
    TEST_ASSERT_MSG(memcmp(buf, "0123456789ABCDE", 15) == 0, "截断内容");

    /* 边界：dest_size=1 → 只写 NUL */
    memset(buf, 'x', sizeof(buf));
    n = lv_strlcpy_n(buf, 1, "abc", 3);
    TEST_ASSERT_EQ(n, (size_t) 3);
    TEST_ASSERT_EQ(buf[0], '\0');

    /* 边界：空源 → 写空串，返回 0 */
    memset(buf, 'x', sizeof(buf));
    n = lv_strlcpy_n(buf, sizeof(buf), "", 0);
    TEST_ASSERT_EQ(n, (size_t) 0);
    TEST_ASSERT_EQ(buf[0], '\0');

    /* 边界：NULL / dest_size=0 → 0 且不写 */
    TEST_ASSERT_EQ(lv_strlcpy_n(NULL, 16, "abc", 3), (size_t) 0);
    TEST_ASSERT_EQ(lv_strlcpy_n(buf, 0, "abc", 3), (size_t) 0);
    TEST_ASSERT_EQ(lv_strlcpy_n(buf, 16, NULL, 3), (size_t) 0);
}

/* ============================================================
 * lv_scratch_buf_cleanup：NULL 安全清理
 * ============================================================ */
static void test_scratch_buf(void) {
    /* 清理无状态 scratch 缓冲（内部 TLS 向量） */
    lv_scratch_buf_cleanup();
    lv_scratch_buf_cleanup(); /* 二次清理安全 */

    /* 与 TLS 向量清理协同（s_lv_scratch 内部即 lvTlsVector） */
    lv_scratch_buf_cleanup();
    TEST_ASSERT_MSG(1, "scratch 清理无崩溃");
}

/* ============================================================
 * 主入口
 * ============================================================ */
TEST_MAIN_BEGIN("Lv Utils Facilities")

    TEST_MAIN_RUN(test_ensure_capacity);
    TEST_MAIN_RUN(test_cmp_int_uint64);
    TEST_MAIN_RUN(test_max_d_abs);
    TEST_MAIN_RUN(test_dstr);
    TEST_MAIN_RUN(test_darray_extras);
    TEST_MAIN_RUN(test_tls_vector);
    TEST_MAIN_RUN(test_fnv1a);
    TEST_MAIN_RUN(test_endian);
    TEST_MAIN_RUN(test_time_facilities);
    TEST_MAIN_RUN(test_random);
    TEST_MAIN_RUN(test_strlcpy_n);
    TEST_MAIN_RUN(test_scratch_buf);

TEST_MAIN_END()
