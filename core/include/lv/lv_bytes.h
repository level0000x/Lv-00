/**
 * @file lv_bytes.h
 * @brief 公共字节流编解码设施：lvByteWriter / lvByteReader
 *
 * @details 统一 Lv-00 各模块手写字节编解码的公共设施：
 *   - 动态增长写入游标（lvByteWriter），复用 lv_ensure_capacity 扩容；
 *   - 有界读取游标（lvByteReader），越界读取返回错误（bool）；
 *   - 显式大端/小端 u16/u32/u64 原语，复用 lv_utils.h 的 lv_store/load_*；
 *   - 通用 varint / zigzag 变长整数编码。
 *
 * 端序规则：网络/文件格式应显式选择 le/be 变体，禁止按主机序 memcpy 整数，
 * 以免移植到大端主机时写读不一致。
 *
 * @author Lv-00 Project
 */

#ifndef lv_lv_BYTES_H
#define lv_lv_BYTES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef lv_PUBLIC_API
#define lv_PUBLIC_API
#endif

/* ============================================================
 * 写入游标：lvByteWriter
 *
 * {buf, capacity, pos} 结构，写入超出容量时自动倍增扩容（经
 * lv_ensure_capacity），扩容失败或溢出时写函数返回 false 并置 error。
 * ============================================================ */

typedef struct {
    uint8_t *buf;    /* 缓冲区数据（lv_malloc 家族分配，lv_byte_writer_destroy 释放） */
    size_t capacity; /* 缓冲区总容量（字节） */
    size_t pos;      /* 当前写入位置（已写字节数） */
    bool error;      /* 最近一次写操作是否失败（OOM/溢出），写函数返回 false 时置位 */
} lvByteWriter;

/* ============================================================
 * 读取游标：lvByteReader
 *
 * 有界只读游标，所有 read_* 在数据不足时返回 false（不越界读取）。
 * ============================================================ */

typedef struct {
    const uint8_t *buf; /* 只读数据源（不持有所有权） */
    size_t size;        /* 数据总长度（字节） */
    size_t pos;         /* 当前读取位置 */
} lvByteReader;

/* ============================================================
 * varint 分档标记字节
 *
 * 通用 varint 编码（载荷为大端序，值域分档推广自 msgpack 的
 * 127/0xff/0xffff/0xffffffff 分档方案）：
 *   v <= 0x7F          → [v]                          （1 字节）
 *   v <= 0xFF          → [0xFF][u8]                   （2 字节）
 *   v <= 0xFFFF        → [0xFE][u16 BE]               （3 字节）
 *   v <= 0xFFFFFFFF    → [0xFD][u32 BE]               （5 字节）
 *   其它               → [0xFC][u64 BE]               （9 字节）
 * 首字节 0x80..0xFB 为保留值，lv_byte_reader_read_varint 视为错误。
 * ============================================================ */

enum {
    lv_BYTE_VARINT_U8_MARKER = 0xff,
    lv_BYTE_VARINT_U16_MARKER = 0xfe,
    lv_BYTE_VARINT_U32_MARKER = 0xfd,
    lv_BYTE_VARINT_U64_MARKER = 0xfc
};

/* ============================================================
 * lvByteWriter API
 * ============================================================ */

/**
 * @brief 初始化写入游标
 * @param w               游标指针（未初始化即可）
 * @param initial_capacity 初始容量（可为 0，首次写入时惰性分配）
 * @return true 成功，false 内存分配失败
 */
lv_PUBLIC_API bool lv_byte_writer_init(lvByteWriter *w, size_t initial_capacity);

/**
 * @brief 释放写入游标缓冲区
 * @param w 游标指针（释放后 buf 置 NULL，pos/capacity/error 归零）
 */
lv_PUBLIC_API void lv_byte_writer_destroy(lvByteWriter *w);

/**
 * @brief 确保游标至少有 extra 字节的剩余空间（不足则倍增扩容）
 * @param w     游标指针
 * @param extra 需要追加的字节数
 * @return true 成功，false 扩容失败（OOM/溢出，置 w->error）
 */
lv_PUBLIC_API bool lv_byte_writer_ensure(lvByteWriter *w, size_t extra);

/**
 * @brief 写入单字节
 * @return true 成功，false 失败
 */
lv_PUBLIC_API bool lv_byte_writer_write_u8(lvByteWriter *w, uint8_t v);

/**
 * @brief 按小端序写入 16 位整数
 * @return true 成功，false 失败
 */
lv_PUBLIC_API bool lv_byte_writer_write_u16_le(lvByteWriter *w, uint16_t v);

/**
 * @brief 按大端序写入 16 位整数
 * @return true 成功，false 失败
 */
lv_PUBLIC_API bool lv_byte_writer_write_u16_be(lvByteWriter *w, uint16_t v);

/**
 * @brief 按小端序写入 32 位整数
 * @return true 成功，false 失败
 */
lv_PUBLIC_API bool lv_byte_writer_write_u32_le(lvByteWriter *w, uint32_t v);

/**
 * @brief 按大端序写入 32 位整数
 * @return true 成功，false 失败
 */
lv_PUBLIC_API bool lv_byte_writer_write_u32_be(lvByteWriter *w, uint32_t v);

/**
 * @brief 按小端序写入 64 位整数
 * @return true 成功，false 失败
 */
lv_PUBLIC_API bool lv_byte_writer_write_u64_le(lvByteWriter *w, uint64_t v);

/**
 * @brief 按大端序写入 64 位整数
 * @return true 成功，false 失败
 */
lv_PUBLIC_API bool lv_byte_writer_write_u64_be(lvByteWriter *w, uint64_t v);

/**
 * @brief 写入原始字节块
 * @param w    游标指针
 * @param data 数据源（n == 0 时可为 NULL）
 * @param n    字节数
 * @return true 成功，false 失败
 */
lv_PUBLIC_API bool lv_byte_writer_write_bytes(lvByteWriter *w, const void *data, size_t n);

/**
 * @brief 写入无符号 varint（分档标记编码，见文件头注释）
 * @return true 成功，false 失败
 */
lv_PUBLIC_API bool lv_byte_writer_write_varint(lvByteWriter *w, uint64_t v);

/**
 * @brief 写入有符号整数（zigzag 映射后走 varint）
 *
 * 映射规则：0→0, -1→1, 1→2, -2→3, 2→4, ...（INT64_MIN/MAX 均可表示）
 *
 * @return true 成功，false 失败
 */
lv_PUBLIC_API bool lv_byte_writer_write_zigzag(lvByteWriter *w, int64_t v);

/* ============================================================
 * lvByteReader API
 * ============================================================ */

/**
 * @brief 初始化读取游标
 * @param r    游标指针
 * @param buf  只读数据源（调用者持有所有权）
 * @param size 数据长度
 */
lv_PUBLIC_API void lv_byte_reader_init(lvByteReader *r, const uint8_t *buf, size_t size);

/**
 * @brief 返回剩余未读字节数
 */
lv_PUBLIC_API size_t lv_byte_reader_remaining(const lvByteReader *r);

/**
 * @brief 读取单字节
 * @param r   游标指针
 * @param out 输出值
 * @return true 成功，false 数据不足（不越界，pos 不变）
 */
lv_PUBLIC_API bool lv_byte_reader_read_u8(lvByteReader *r, uint8_t *out);

/**
 * @brief 按小端序读取 16 位整数
 * @return true 成功，false 数据不足
 */
lv_PUBLIC_API bool lv_byte_reader_read_u16_le(lvByteReader *r, uint16_t *out);

/**
 * @brief 按大端序读取 16 位整数
 * @return true 成功，false 数据不足
 */
lv_PUBLIC_API bool lv_byte_reader_read_u16_be(lvByteReader *r, uint16_t *out);

/**
 * @brief 按小端序读取 32 位整数
 * @return true 成功，false 数据不足
 */
lv_PUBLIC_API bool lv_byte_reader_read_u32_le(lvByteReader *r, uint32_t *out);

/**
 * @brief 按大端序读取 32 位整数
 * @return true 成功，false 数据不足
 */
lv_PUBLIC_API bool lv_byte_reader_read_u32_be(lvByteReader *r, uint32_t *out);

/**
 * @brief 按小端序读取 64 位整数
 * @return true 成功，false 数据不足
 */
lv_PUBLIC_API bool lv_byte_reader_read_u64_le(lvByteReader *r, uint64_t *out);

/**
 * @brief 按大端序读取 64 位整数
 * @return true 成功，false 数据不足
 */
lv_PUBLIC_API bool lv_byte_reader_read_u64_be(lvByteReader *r, uint64_t *out);

/**
 * @brief 读取原始字节块
 * @param r    游标指针
 * @param out  输出缓冲区（n == 0 时可为 NULL）
 * @param n    字节数
 * @return true 成功，false 数据不足（不越界，pos 不变）
 */
lv_PUBLIC_API bool lv_byte_reader_read_bytes(lvByteReader *r, void *out, size_t n);

/**
 * @brief 读取无符号 varint（与 lv_byte_writer_write_varint 对偶）
 * @return true 成功，false 数据不足或首字节为保留值（0x80..0xFB）
 */
lv_PUBLIC_API bool lv_byte_reader_read_varint(lvByteReader *r, uint64_t *out);

/**
 * @brief 读取有符号整数（zigzag 解码，与 lv_byte_writer_write_zigzag 对偶）
 * @return true 成功，false 数据不足或编码非法
 */
lv_PUBLIC_API bool lv_byte_reader_read_zigzag(lvByteReader *r, int64_t *out);

#ifdef __cplusplus
}
#endif

#endif /* lv_lv_BYTES_H */
