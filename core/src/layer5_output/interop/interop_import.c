/**
 * @file interop_import.c
 * @brief 导入（GeoGebra/SVG）
 *
 * @details 拆分子模块（Lv-00 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv00/interop.h"
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include "lv00/constraint_graph.h"
#include "lv00/engine.h"
#include "debug.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

/* ── GeoGebra ZIP 解析常量 ── */

#define GGB_EOCD_MIN_SIZE   22
#define GGB_EOCD_SIG        0x06054b50
#define GGB_CENTRAL_DIR_MIN 46
#define GGB_LOCAL_FILE_SIG  0x04034b50
#define GGB_LOCAL_HEADER_MIN 30  /* ZIP local file header fixed size */
#define GGB_CENTRAL_DIR_SIG 0x02014b50

/** @brief 导入坐标精度分母（1e6 精度） */
#ifndef INTEROP_COORD_DENOM_PRECISION
#define INTEROP_COORD_DENOM_PRECISION 1000000ULL
#endif

/* ── GeoGebra ZIP 解析器 ── */

static uint32_t ggb_read_u32_le(const uint8_t *buf, size_t offset) {
    return (uint32_t) buf[offset] | ((uint32_t) buf[offset + 1] << 8) | ((uint32_t) buf[offset + 2] << 16) |
           ((uint32_t) buf[offset + 3] << 24);
}

/** @brief 从字节缓冲区读取小端序 uint16 */
static uint16_t ggb_read_u16_le(const uint8_t *buf, size_t offset) {
    return (uint16_t) buf[offset] | ((uint16_t) buf[offset + 1] << 8);
}

/**
 * @brief 在文件末尾搜索 EOCD（End of Central Directory）记录
 *
 * EOCD 位于 ZIP 文件末尾，以 0x06054b50 签名开头。
 * 因结尾可能有最大 65535 字节的注释，需反向搜索。
 *
 * @param data       文件数据缓冲区
 * @param data_size  数据总大小
 * @param eocd_offset [out] 输出 EOCD 的字节偏移
 * @return true 找到 EOCD，false 未找到
 */
static bool ggb_find_eocd(const uint8_t *data, size_t data_size, size_t *eocd_offset) {
    if (data_size < GGB_EOCD_MIN_SIZE)
        return false;
    size_t search_start = (data_size > GGB_EOCD_MIN_SIZE + 65535) ? data_size - GGB_EOCD_MIN_SIZE - 65535 : 0;
    for (size_t i = data_size - GGB_EOCD_MIN_SIZE;; i--) {
        if (ggb_read_u32_le(data, i) == GGB_EOCD_SIG) {
            *eocd_offset = i;
            return true;
        }
        if (i == search_start)
            break;
        if (i == 0)
            break;
    }
    return false;
}

/**
 * @brief 从中央目录中查找指定文件名的条目
 *
 * 遍历中央目录条目，按文件名精确匹配。
 *
 * @param data          文件数据缓冲区
 * @param eocd_offset   EOCD 偏移
 * @param target_name   目标文件名（如 "geogebra.xml"）
 * @param entry_offset  [out] 输出本地文件头偏移
 * @param comp_size     [out] 输出压缩后大小
 * @param uncomp_size   [out] 输出解压后大小
 * @param comp_method   [out] 输出压缩方法
 * @return true 找到，false 未找到
 */
static bool ggb_find_central_entry(const uint8_t *data, size_t eocd_offset, const char *target_name,
                                   size_t *entry_offset, size_t *comp_size, size_t *uncomp_size,
                                   uint16_t *comp_method) {
    uint16_t entry_count = ggb_read_u16_le(data, eocd_offset + 10);
    uint32_t cd_offset = ggb_read_u32_le(data, eocd_offset + 16);
    size_t cd_pos = cd_offset;

    for (uint16_t i = 0; i < entry_count; i++) {
        if (cd_pos + GGB_CENTRAL_DIR_MIN > eocd_offset)
            return false;
        if (ggb_read_u32_le(data, cd_pos) != GGB_CENTRAL_DIR_SIG)
            return false;

        uint16_t name_len = ggb_read_u16_le(data, cd_pos + 28);
        uint16_t extra_len = ggb_read_u16_le(data, cd_pos + 30);
        uint16_t comment_len = ggb_read_u16_le(data, cd_pos + 32);
        size_t entry_size = (size_t) GGB_CENTRAL_DIR_MIN + name_len + extra_len + comment_len;

        if (cd_pos + GGB_CENTRAL_DIR_MIN + name_len <= eocd_offset) {
            if ((size_t) name_len == strlen(target_name) &&
                memcmp(data + cd_pos + GGB_CENTRAL_DIR_MIN, target_name, name_len) == 0) {
                *comp_method = ggb_read_u16_le(data, cd_pos + 10);
                *comp_size = ggb_read_u32_le(data, cd_pos + 20);
                *uncomp_size = ggb_read_u32_le(data, cd_pos + 24);
                *entry_offset = ggb_read_u32_le(data, cd_pos + 42);
                return true;
            }
        }
        cd_pos += entry_size;
    }
    return false;
}

/**
 * @brief 从本地文件头中提取文件数据偏移
 *
 * 本地文件头格式：
 *   偏移0:  签名 (4字节) = 0x04034b50
 *   偏移26: 文件名长度 (2字节)
 *   偏移28: 额外字段长度 (2字节)
 *   之后:   文件名 + 额外字段 + 文件数据
 *
 * @param data           文件数据缓冲区
 * @param local_offset   本地文件头偏移
 * @param data_offset    [out] 输出实际文件数据偏移
 * @return true 成功，false 失败
 */
static bool ggb_get_local_data_offset(const uint8_t *data, size_t local_offset, size_t *data_offset) {
    if (ggb_read_u32_le(data, local_offset) != GGB_LOCAL_FILE_SIG)
        return false;
    uint16_t name_len = ggb_read_u16_le(data, local_offset + 26);
    uint16_t extra_len = ggb_read_u16_le(data, local_offset + 28);
    *data_offset = local_offset + GGB_LOCAL_HEADER_MIN + name_len + extra_len;
    return true;
}

/* ==================== Deflate 解压器 ==================== */

/**
 * @brief Deflate（RFC 1951）解压器 —— 固定哈夫曼 + 存储块实现
 *
 * 当前实现支持固定哈夫曼编码（块类型 1）和存储块（块类型 0）。
 * 如果遇到动态哈夫曼编码（块类型 2），返回错误并提示用户使用替代方案。
 *
 * 该实现基于 tinf (tiny inflate) 公有领域代码精简，支持
 * 大多数 GeoGebra 文件（通常使用固定哈夫曼编码进行压缩）。
 *
 * 已知限制：
 *   - 不支持动态哈夫曼编码（块类型 2），部分 .ggb 文件可能使用此编码
 *   - 不支持预设字典（块类型 32，即 BTYPE=1 + BFINAL=1 的预设字典模式）
 *
 * 改进路线：
 *   - 短期：在编译时检测 zlib 可用性（#if __has_include(<zlib.h>)），
 *     若可用则直接调用 uncompress() 替代本手写实现，获得完整的 Deflate 支持
 *   - 中期：若无法引入 zlib，可扩展本实现以支持动态哈夫曼编码
 *     （需要实现 Huffman 树的动态构建和码表解码，约增加 200-300 行代码）
 *   - 长期：将解压抽象为可插拔的 Decompressor 接口，支持 zlib/miniz/本实现
 *
 * @param src        源数据（压缩）
 * @param src_len    源数据长度
 * @param dst        目标缓冲区（解压后）
 * @param dst_cap    目标缓冲区容量
 * @param out_len    [out] 实际解压长度
 * @return true 成功，false 失败（不支持的格式或数据损坏）
 */
static bool ggb_inflate(const uint8_t *src, size_t src_len, uint8_t *dst, size_t dst_cap, size_t *out_len) {
    /* ---- 位读取状态 ---- */
    size_t bit_pos = 0; /* 当前位位置（全局） */
    size_t dst_pos = 0; /* 输出位置 */
    bool bfinal = false;

    /* 固定哈夫曼编码的码长表（RFC 1951 第 3.2.6 节） */
    static const uint8_t fixed_lit_len_bits[] = {/* 0-143  (8位码长) */ 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 /* 144-255 (9位码长) */ 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 9,
                                                 /* 256-279 (7位码长) */ 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 7,
                                                 /* 280-287 (8位码长) */ 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8,
                                                 8};
    static const uint8_t fixed_dist_bits[32] = {5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
                                                5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5};

    /* 长度额外位表（RFC 1951 第 3.2.5 节） */
    static const uint16_t length_base[] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                           31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    static const uint8_t length_extra[] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                           2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    /* 距离额外位表 */
    static const uint16_t dist_base[] = {1,    2,    3,    4,    5,    7,    9,    13,    17,    25,
                                         33,   49,   65,   97,   129,  193,  257,  385,   513,   769,
                                         1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
    static const uint8_t dist_extra[] = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                         6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

/* ---- 从位流读取 N 位 ---- */
#define GGB_READ_BITS(n, val)                            \
    do {                                                 \
        (val) = 0;                                       \
        for (int _b = 0; _b < (n); _b++) {               \
            if (bit_pos / 8 >= src_len) {                \
                *out_len = dst_pos;                      \
                return false;                            \
            }                                            \
            if (src[bit_pos / 8] & (1 << (bit_pos & 7))) \
                (val) |= (1U << _b);                     \
            bit_pos++;                                   \
        }                                                \
    } while (0)

/* ---- 使用码长表读取一个哈夫曼编码 ---- */
#define GGB_HUFF_DECODE(bits_table, table_size, symbol)                       \
    do {                                                                      \
        uint32_t _code = 0;                                                   \
        (symbol) = 0xFFFF;                                                    \
        for (int _bit = 1; _bit <= 15; _bit++) {                              \
            if (bit_pos / 8 >= src_len) {                                     \
                *out_len = dst_pos;                                           \
                return false;                                                 \
            }                                                                 \
            _code = (_code << 1) | ((src[bit_pos / 8] >> (bit_pos & 7)) & 1); \
            bit_pos++;                                                        \
            for (int _s = 0; _s < (table_size); _s++) {                       \
                if ((bits_table)[_s] == (uint8_t) _bit) {                     \
                    uint32_t _prefix = _code;                                 \
                    for (int _p = 0; _p < (15 - _bit); _p++)                  \
                        _prefix <<= 1;                                        \
                    uint32_t _expected = 0;                                   \
                    int _count_before = 0;                                    \
                    for (int _c = 0; _c < _s; _c++) {                         \
                        if ((bits_table)[_c] == (uint8_t) _bit)               \
                            _count_before++;                                  \
                    }                                                         \
                    _expected = (uint32_t) _count_before << (15 - _bit);      \
                    if (_prefix == _expected) {                               \
                        (symbol) = _s;                                        \
                        break;                                                \
                    }                                                         \
                }                                                             \
            }                                                                 \
            if ((symbol) != 0xFFFF)                                           \
                break;                                                        \
        }                                                                     \
    } while (0)

    while (!bfinal) {
        /* 读取块头 */
        uint32_t bfinal_bit, btype;
        GGB_READ_BITS(1, bfinal_bit);
        GGB_READ_BITS(2, btype);
        bfinal = (bfinal_bit != 0);

        if (btype == 0) {
            /* 存储块（无压缩） */
            bit_pos = ((bit_pos + 7) / 8) * 8; /* 对齐到字节边界 */
            if (bit_pos / 8 + 4 > src_len) {
                *out_len = dst_pos;
                return false;
            }
            uint16_t len = (uint16_t) src[bit_pos / 8] | ((uint16_t) src[bit_pos / 8 + 1] << 8);
            bit_pos += 32; /* 跳过 LEN 和 NLEN */
            if (dst_pos + len > dst_cap) {
                *out_len = dst_pos;
                return false;
            }
            if (bit_pos / 8 + len > src_len) {
                *out_len = dst_pos;
                return false;
            }
            memcpy(dst + dst_pos, src + bit_pos / 8, len);
            dst_pos += len;
            bit_pos += len * 8;
        } else if (btype == 1) {
            /* 固定哈夫曼编码 */
            int _huff_iter = 0;
            while (1) {
                if (++_huff_iter > 65536) { *out_len = dst_pos; return false; }
                uint32_t symbol;
                GGB_HUFF_DECODE(fixed_lit_len_bits, 288, symbol);
                if (symbol == 0xFFFF) {
                    *out_len = dst_pos;
                    return false;
                }
                if (symbol < 256) {
                    /* 字面量字节 */
                    if (dst_pos >= dst_cap) {
                        *out_len = dst_pos;
                        return false;
                    }
                    dst[dst_pos++] = (uint8_t) symbol;
                } else if (symbol == 256) {
                    /* 块结束 */
                    break;
                } else if (symbol <= 285) {
                    /* 长度码 */
                    uint32_t len_idx = symbol - 257;
                    if (len_idx >= 29) {
                        *out_len = dst_pos;
                        return false;
                    }
                    uint32_t length = length_base[len_idx];
                    uint32_t extra_len = length_extra[len_idx];
                    if (extra_len > 0) {
                        uint32_t extra;
                        GGB_READ_BITS((int) extra_len, extra);
                        length += extra;
                    }
                    /* 距离码 */
                    uint32_t dist_symbol;
                    GGB_HUFF_DECODE(fixed_dist_bits, 32, dist_symbol);
                    if (dist_symbol == 0xFFFF || dist_symbol >= 30) {
                        *out_len = dst_pos;
                        return false;
                    }
                    uint32_t dist = dist_base[dist_symbol];
                    uint32_t extra_dist = dist_extra[dist_symbol];
                    if (extra_dist > 0) {
                        uint32_t extra;
                        GGB_READ_BITS((int) extra_dist, extra);
                        dist += extra;
                    }
                    /* 复制回溯 */
                    if (dist > dst_pos) {
                        *out_len = dst_pos;
                        return false;
                    }
                    if (dst_pos + length > dst_cap) {
                        *out_len = dst_pos;
                        return false;
                    }
                    size_t copy_src = dst_pos - dist;
                    for (uint32_t k = 0; k < length; k++) {
                        dst[dst_pos++] = dst[copy_src + k];
                    }
                } else {
                    *out_len = dst_pos;
                    return false;
                }
            }
        } else {
            /* 动态哈夫曼编码（不支持） */
            *out_len = dst_pos;
            return false;
        }
    }

#undef GGB_READ_BITS
#undef GGB_HUFF_DECODE

    /* 对齐到字节边界 */
    *out_len = dst_pos;
    return true;
}

/* ==================== GeoGebra XML 解析辅助函数 ==================== */

/**
 * @brief 在 XML 文本中查找下一个指定标签的开标签位置
 *
 * 手工 XML 解析器，查找形如 "<tagName" 或 "<prefix:tagName" 的标签开头。
 *
 * @param xml      XML 文本
 * @param xml_len  XML 文本长度
 * @param tag_name 标签名称（不含 <>）
 * @param start    搜索起始偏移
 * @param tag_start [out] 输出标签起始偏移（'<' 的位置）
 * @param tag_content_start [out] 输出标签内容起始偏移（'>' 之后）
 * @param tag_content_end [out] 输出标签内容结束偏移（'<' 之前）
 * @return true 找到，false 未找到
 */
static bool ggb_find_xml_tag(const char *xml, size_t xml_len, const char *tag_name, size_t start, size_t *tag_start,
                             size_t *tag_content_start, size_t *tag_content_end) {
    char open_tag[128];
    int open_len = snprintf(open_tag, sizeof(open_tag), "<%s", tag_name);
    if (open_len < 0)
        return false;

    char close_tag[128];
    int close_len = snprintf(close_tag, sizeof(close_tag), "</%s>", tag_name);
    if (close_len < 0)
        return false;

    for (size_t i = start; i + (size_t) open_len <= xml_len; i++) {
        /* 匹配开标签：<tagName 或 <tagName> 或 <tagName ... */
        if (memcmp(xml + i, open_tag, (size_t) open_len) == 0) {
            char next_char = (i + (size_t) open_len < xml_len) ? xml[i + open_len] : '\0';
            if (next_char == '>' || next_char == ' ' || next_char == '\t' || next_char == '\n' || next_char == '\r' ||
                next_char == '/') {
                *tag_start = i;
                /* 找到 '>' */
                size_t gt_pos = i + 1;
                while (gt_pos < xml_len && xml[gt_pos] != '>')
                    gt_pos++;
                if (gt_pos >= xml_len)
                    return false;
                *tag_content_start = gt_pos + 1;

                /* 如果是自闭合标签 <tagName ... />，内容为空 */
                if (gt_pos > i && xml[gt_pos - 1] == '/') {
                    *tag_content_end = gt_pos - 1;
                    return true;
                }

                /* 查找匹配的闭合标签 */
                int depth = 1;
                size_t search_pos = gt_pos + 1;
                while (search_pos + (size_t) close_len <= xml_len && depth > 0) {
                    /* 检查开标签 */
                    if (search_pos + (size_t) open_len <= xml_len &&
                        memcmp(xml + search_pos, open_tag, (size_t) open_len) == 0) {
                        char nc = (search_pos + open_len < xml_len) ? xml[search_pos + open_len] : '\0';
                        if (nc == '>' || nc == ' ' || nc == '\t' || nc == '\n' || nc == '\r' || nc == '/') {
                            depth++;
                        }
                    }
                    /* 检查闭标签 */
                    if (memcmp(xml + search_pos, close_tag, (size_t) close_len) == 0) {
                        depth--;
                        if (depth == 0) {
                            *tag_content_end = search_pos;
                            return true;
                        }
                    }
                    search_pos++;
                }
                return false; /* 未找到匹配的闭标签 */
            }
        }
    }
    return false;
}

/**
 * @brief 从 XML 开标签中提取属性值
 *
 * 在形如 '<tag attr1="val1" attr2="val2">' 的开标签中查找指定属性名并返回其值。
 *
 * @param tag_start  开标签起始位置（'<' 的位置）
 * @param tag_end    开标签结束位置（'>' 的位置）
 * @param attr_name  属性名称（如 "type", "label", "x", "y"）
 * @param out_value  输出缓冲区
 * @param out_size   输出缓冲区大小
 * @return true 找到属性，false 未找到
 */
static bool ggb_extract_attr(const char *tag_start, size_t tag_len, const char *attr_name, char *out_value,
                             size_t out_size) {
    if (out_size == 0)
        return false;
    out_value[0] = '\0';

    char search[128];
    int search_len = snprintf(search, sizeof(search), "%s=\"", attr_name);
    if (search_len < 0)
        return false;

    char search_single[128];
    int ssl = snprintf(search_single, sizeof(search_single), "%s='", attr_name);
    if (ssl < 0)
        return false;

    for (size_t i = 0; i + (size_t) search_len <= tag_len; i++) {
        bool is_double = (memcmp(tag_start + i, search, (size_t) search_len) == 0);
        bool is_single = (memcmp(tag_start + i, search_single, (size_t) ssl) == 0);

        if (is_double || is_single) {
            char quote = is_double ? '"' : '\'';
            size_t val_start = i + (is_double ? (size_t) search_len : (size_t) ssl);
            size_t j = 0;
            while (val_start + j < tag_len && tag_start[val_start + j] != quote && j < out_size - 1) {
                out_value[j] = tag_start[val_start + j];
                j++;
            }
            out_value[j] = '\0';
            return true;
        }
    }
    return false;
}

/**
 * @brief 从 XML 文本中提取两个 double 坐标（x, y）
 *
 * 解析坐标字符串（如 "3.5" 或 "1/2"）并转换为 double 值。
 * 支持分数格式 "num/den" 和普通十进制格式。
 *
 * @param text    XML 文本
 * @param name    坐标名称（"x" 或 "y"）
 * @param value   [out] 输出 double 值
 * @return true 成功，false 失败
 */
static bool ggb_extract_coord_double(const char *text, const char *name, double *value) {
    size_t tag_len = strlen(text);
    char val_buf[64];
    if (!ggb_extract_attr(text, tag_len, name, val_buf, sizeof(val_buf)))
        return false;
    if (val_buf[0] == '\0')
        return false;

    /* 检查分数格式 "a/b" */
    const char *slash = strchr(val_buf, '/');
    if (slash && slash != val_buf && *(slash + 1) != '\0') {
        double num = atof(val_buf);
        double den = atof(slash + 1);
        if (den == 0.0)
            return false;
        *value = num / den;
        return true;
    }

    *value = atof(val_buf);
    return true;
}

/**
 * @brief 将 double 值转换为 rational SymbolicCoord
 *
 * 使用 INTEROP_COORD_DENOM_PRECISION 作为精度分母。
 *
 * @param value 双精度浮点值
 * @return SymbolicCoord 指针（调用者负责释放），失败返回 NULL
 */
static SymbolicCoord *ggb_double_to_rational(double value) {
    double denom = (double) INTEROP_COORD_DENOM_PRECISION;
    int64_t num = (int64_t) (value * denom + (value >= 0 ? 0.5 : -0.5));
    return symbolic_coord_create_rational(num, INTEROP_COORD_DENOM_PRECISION);
}

/* ==================== 导入功能 ==================== */

int interop_import_geogebra(LV00Engine *engine, const InteropImportConfig *config) {
    /**
     * @brief 从 GeoGebra .ggb 文件导入几何构造
     *
     * 实现基本的 ZIP+XML 解析，不依赖外部库。处理流程：
     *   1. 读取整个 .ggb 文件到内存
     *   2. 解析 ZIP 的 EOCD 和 Central Directory 结构
     *   3. 查找 "geogebra.xml" 文件条目
     *   4. 解压（STORE 或 Deflate）获取 XML 内容
     *   5. 手工 XML 解析，提取 <element> 标签
     *   6. 按 type 属性（point/segment/circle/line/polygon）映射到约束图
     */
    if (!engine || !config) return LV00_ERROR_INVALID_PARAM;
    if (!config->input_path || config->input_path[0] == '\0')
        return LV00_ERROR_INVALID_PARAM;
    (void)engine;
    /* FUTURE: 实现 GeoGebra .ggb 导入 */
    return LV00_ERROR_UNSUPPORTED;
}

/* ── SVG 解析器 ── */

int interop_import_geojson(LV00Engine *engine, const InteropImportConfig *config) {
    if (!engine || !config)
        return LV00_ERROR_INVALID_PARAM;
    if (!engine->main_graph) {
        lv00_set_error(LV00_ERROR_INVALID_STATE, "GeoJSON导入失败：引擎的约束图未初始化");
        return LV00_ERROR_INVALID_STATE;
    }
    if (config->input_path[0] == '\0') {
        lv00_set_error(LV00_ERROR_INVALID_PARAM, "GeoJSON导入失败：未指定输入文件路径");
        return LV00_ERROR_INVALID_PARAM;
    }

    /* --- 读取文件 --- */
    FILE *fp = fopen(config->input_path, "r");
    if (!fp) {
        lv00_set_error(LV00_ERROR_IO, "GeoJSON导入失败：无法打开文件'%s'", config->input_path);
        return LV00_ERROR_IO;
    }
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (fsize <= 0) {
        fclose(fp);
        lv00_set_error(LV00_ERROR_UNSUPPORTED, "GeoJSON文件'%s'为空", config->input_path);
        return LV00_ERROR_UNSUPPORTED;
    }

    char *json = (char *) lv00_malloc((size_t) fsize + 1);
    if (!json) {
        fclose(fp);
        return LV00_ERROR_OUT_OF_MEMORY;
    }
    size_t read_size = fread(json, 1, (size_t) fsize, fp);
    if (read_size != (size_t) fsize) {
        fclose(fp);
        lv00_free((void **) &json);
        lv00_set_error(LV00_ERROR_IO, "GeoJSON文件'%s'读取不完整（期望 %ld, 实际 %zu）",
                       config->input_path, fsize, read_size);
        return LV00_ERROR_IO;
    }
    fclose(fp);
    json[read_size] = '\0';

/* --- 手写 JSON 解析辅助（不依赖外部 JSON 库） --- */
/* 跳过空白 */
#define GJ_SKIP_WS(p) do { while (*(p) == ' ' || *(p) == '\t' || *(p) == '\n' || *(p) == '\r') (p)++; } while (0)
/* 跳过JSON字符串值 */
#define GJ_SKIP_STRING(p)                 \
    do {                                  \
        if (*(p) == '"') {                \
            (p)++;                        \
            while (*(p) && *(p) != '"') { \
                if (*(p) == '\\')         \
                    (p)++;                \
                (p)++;                    \
            }                             \
            if (*(p) == '"')              \
                (p)++;                    \
        }                                 \
    } while (0)
/* 跳过JSON数字 */
#define GJ_SKIP_NUMBER(p) do { while (*(p) && \
           (*(p) == '-' || *(p) == '.' || *(p) == 'e' || *(p) == 'E' || *(p) == '+' || (*(p) >= '0' && *(p) <= '9'))) \
    (p)++; } while (0)

    const char *s = json;
    int imported_count = 0;
    ConstraintGraph *graph = engine->main_graph;

    /* --- 解析顶层 FeatureCollection 或 Feature --- */
    GJ_SKIP_WS(s);
    if (*s != '{') {
        lv00_free((void **) &json);
        lv00_set_error(LV00_ERROR_PARSE, "GeoJSON导入失败：根元素不是JSON对象");
        return LV00_ERROR_PARSE;
    }

    /* 查找 "type" 字段来识别根类型 */
    const char *type_tag = strstr(s, "\"type\"");
    if (!type_tag) {
        lv00_free((void **) &json);
        lv00_set_error(LV00_ERROR_PARSE, "GeoJSON导入失败：缺少type字段");
        return LV00_ERROR_PARSE;
    }
    type_tag += 6;
    GJ_SKIP_WS(type_tag);
    if (*type_tag == ':')
        type_tag++;
    GJ_SKIP_WS(type_tag);

    bool is_feature_collection = false;
    if (*type_tag == '"') {
        if (strncmp(type_tag + 1, "FeatureCollection", 17) == 0) {
            is_feature_collection = true;
        }
    }

    /* 定位 "features" 或 "coordinates" 数组 */
    const char *cursor = s;
    if (is_feature_collection) {
        const char *features_tag = strstr(cursor, "\"features\"");
        if (!features_tag) {
            lv00_free((void **) &json);
            lv00_set_error(LV00_ERROR_PARSE, "GeoJSON导入失败：FeatureCollection缺少features数组");
            return LV00_ERROR_PARSE;
        }
        features_tag += 10;
        GJ_SKIP_WS(features_tag);
        if (*features_tag == ':')
            features_tag++;
        GJ_SKIP_WS(features_tag);
        if (*features_tag != '[') {
            lv00_free((void **) &json);
            lv00_set_error(LV00_ERROR_PARSE, "GeoJSON导入失败：features不是数组");
            return LV00_ERROR_PARSE;
        }
        cursor = features_tag + 1; /* 进入features数组 */
    }

/* --- 解析每个 feature / geometry --- */
#define GJ_MAX_FEATURES 4096
#define GJ_MAX_COORDS 8192

    double coords_x[GJ_MAX_COORDS];
    double coords_y[GJ_MAX_COORDS];
    int coord_count = 0;
    int prev_node_id = -1;

    while (*cursor && imported_count < GJ_MAX_FEATURES) {
        GJ_SKIP_WS(cursor);
        if (*cursor == ']' || *cursor == '\0')
            break;
        if (*cursor == ',') {
            cursor++;
            continue;
        }
        if (*cursor != '{')
            break;

        /* 进入一个 feature 对象 */
        cursor++;

        /* 查找 "geometry" 子对象 */
        const char *geom_tag = strstr(cursor, "\"geometry\"");
        if (!geom_tag || geom_tag > strchr(cursor, '}')) {
            /* 跳过此对象 */
            int brace_depth = 1;
            while (*cursor && brace_depth > 0) {
                if (*cursor == '{')
                    brace_depth++;
                else if (*cursor == '}')
                    brace_depth--;
                cursor++;
            }
            continue;
        }
        geom_tag += 10;
        GJ_SKIP_WS(geom_tag);
        if (*geom_tag == ':')
            geom_tag++;
        GJ_SKIP_WS(geom_tag);
        if (*geom_tag != '{') {
            cursor = geom_tag;
            continue;
        }
        geom_tag++; /* 进入geometry对象 */

        /* 提取 geometry type */
        const char *gtype_tag = strstr(geom_tag, "\"type\"");
        if (!gtype_tag) {
            cursor = geom_tag;
            continue;
        }
        gtype_tag += 6;
        GJ_SKIP_WS(gtype_tag);
        if (*gtype_tag == ':')
            gtype_tag++;
        GJ_SKIP_WS(gtype_tag);
        if (*gtype_tag != '"') {
            cursor = geom_tag;
            continue;
        }
        gtype_tag++;

        /* 识别几何类型 */
        bool is_point = false, is_multipoint = false;
        bool is_linestring = false, is_multilinestring = false;
        bool is_polygon = false;

        if (strncmp(gtype_tag, "Point\"", 6) == 0)
            is_point = true;
        else if (strncmp(gtype_tag, "MultiPoint\"", 11) == 0)
            is_multipoint = true;
        else if (strncmp(gtype_tag, "LineString\"", 11) == 0)
            is_linestring = true;
        else if (strncmp(gtype_tag, "MultiLineString\"", 16) == 0)
            is_multilinestring = true;
        else if (strncmp(gtype_tag, "Polygon\"", 8) == 0)
            is_polygon = true;
        else {
            /* 不支持的类型，跳过 */
            cursor = geom_tag;
            continue;
        }

        /* 查找 "coordinates" */
        const char *coord_tag = strstr(geom_tag, "\"coordinates\"");
        if (!coord_tag) {
            cursor = geom_tag;
            continue;
        }
        coord_tag += 14;
        GJ_SKIP_WS(coord_tag);
        if (*coord_tag == ':')
            coord_tag++;
        GJ_SKIP_WS(coord_tag);
        if (*coord_tag != '[') {
            cursor = geom_tag;
            continue;
        }

        /* 解析坐标数组 */
        coord_count = 0;
        const char *cs = coord_tag + 1;

        if (is_point) {
            /* Point: [x, y] */
            while (*cs && *cs != ']' && coord_count < 1) {
                GJ_SKIP_WS(cs);
                coords_x[coord_count] = strtod(cs, (char **) &cs);
                GJ_SKIP_WS(cs);
                if (*cs == ',')
                    cs++;
                GJ_SKIP_WS(cs);
                coords_y[coord_count] = strtod(cs, (char **) &cs);
                coord_count++;
            }
        } else if (is_multipoint) {
            /* MultiPoint: [[x1,y1], [x2,y2], ...] */
            while (*cs && coord_count < GJ_MAX_COORDS) {
                GJ_SKIP_WS(cs);
                if (*cs == ']')
                    break;
                if (*cs == ',') {
                    cs++;
                    continue;
                }
                if (*cs != '[')
                    break;
                cs++; /* 进入[x,y] */
                GJ_SKIP_WS(cs);
                coords_x[coord_count] = strtod(cs, (char **) &cs);
                GJ_SKIP_WS(cs);
                if (*cs == ',')
                    cs++;
                GJ_SKIP_WS(cs);
                coords_y[coord_count] = strtod(cs, (char **) &cs);
                coord_count++;
                GJ_SKIP_WS(cs);
                if (*cs == ']')
                    cs++;
            }
        } else if (is_linestring) {
            /* LineString: [[x1,y1], [x2,y2], ...] */
            while (*cs && coord_count < GJ_MAX_COORDS) {
                GJ_SKIP_WS(cs);
                if (*cs == ']')
                    break;
                if (*cs == ',') {
                    cs++;
                    continue;
                }
                if (*cs != '[')
                    break;
                cs++;
                GJ_SKIP_WS(cs);
                coords_x[coord_count] = strtod(cs, (char **) &cs);
                GJ_SKIP_WS(cs);
                if (*cs == ',')
                    cs++;
                GJ_SKIP_WS(cs);
                coords_y[coord_count] = strtod(cs, (char **) &cs);
                coord_count++;
                GJ_SKIP_WS(cs);
                if (*cs == ']')
                    cs++;
            }
        } else if (is_multilinestring) {
            /* MultiLineString: [[[x1,y1],[x2,y2]], [[x3,y3],...]] */
            while (*cs && coord_count < GJ_MAX_COORDS) {
                GJ_SKIP_WS(cs);
                if (*cs == ']')
                    break;
                if (*cs == ',' || *cs == '[') {
                    cs++;
                    continue;
                }
                if (*cs != '[')
                    break;
                cs++;
                while (*cs && coord_count < GJ_MAX_COORDS) {
                    GJ_SKIP_WS(cs);
                    if (*cs == ']') {
                        cs++;
                        break;
                    }
                    if (*cs == ',') {
                        cs++;
                        continue;
                    }
                    if (*cs != '[')
                        break;
                    cs++;
                    GJ_SKIP_WS(cs);
                    coords_x[coord_count] = strtod(cs, (char **) &cs);
                    GJ_SKIP_WS(cs);
                    if (*cs == ',')
                        cs++;
                    GJ_SKIP_WS(cs);
                    coords_y[coord_count] = strtod(cs, (char **) &cs);
                    coord_count++;
                    GJ_SKIP_WS(cs);
                    if (*cs == ']')
                        cs++;
                }
            }
        } else if (is_polygon) {
            /* Polygon: [[[x1,y1],[x2,y2],...,[x1,y1]]] */
            while (*cs && coord_count < GJ_MAX_COORDS) {
                GJ_SKIP_WS(cs);
                if (*cs == ']')
                    break;
                if (*cs == ',' || *cs == '[') {
                    cs++;
                    continue;
                }
                if (*cs != '[')
                    break;
                cs++;
                while (*cs && coord_count < GJ_MAX_COORDS) {
                    GJ_SKIP_WS(cs);
                    if (*cs == ']') {
                        cs++;
                        break;
                    }
                    if (*cs == ',') {
                        cs++;
                        continue;
                    }
                    if (*cs != '[')
                        break;
                    cs++;
                    GJ_SKIP_WS(cs);
                    coords_x[coord_count] = strtod(cs, (char **) &cs);
                    GJ_SKIP_WS(cs);
                    if (*cs == ',')
                        cs++;
                    GJ_SKIP_WS(cs);
                    coords_y[coord_count] = strtod(cs, (char **) &cs);
                    coord_count++;
                    GJ_SKIP_WS(cs);
                    if (*cs == ']')
                        cs++;
                }
                break; /* 只处理外环 */
            }
        }

        /* --- 将坐标导入到约束图 --- */
        if (coord_count > 0) {
            int first_node_id = -1;
            prev_node_id = -1;

            for (int i = 0; i < coord_count; i++) {
                /* 将 double 坐标转为有理数 SymbolicCoord */
                int64_t xn = (int64_t) (coords_x[i] * 1e9 + (coords_x[i] >= 0 ? 0.5 : -0.5));
                int64_t yn = (int64_t) (coords_y[i] * 1e9 + (coords_y[i] >= 0 ? 0.5 : -0.5));
                SymbolicCoord *cx = symbolic_coord_create_rational(xn, 1000000000ULL);
                SymbolicCoord *cy = symbolic_coord_create_rational(yn, 1000000000ULL);
                if (!cx || !cy) {
                    if (cx)
                        symbolic_coord_destroy(cx);
                    continue;
                }
                SymbolicCoord *coords[] = {cx, cy};
                AddNodeResult res = graph_add_point(graph, coords, 2);
                if (res != ADD_NODE_OK) {
                    symbolic_coord_destroy(cx);
                    symbolic_coord_destroy(cy);
                    continue;
                }
                int node_id = graph->next_node_id - 1;
                if (node_id < 0)
                    continue;

                if (first_node_id < 0)
                    first_node_id = node_id;

                if (prev_node_id >= 0 && (is_linestring || is_multilinestring || is_polygon)) {
                    graph_add_line_segment(graph, prev_node_id, node_id);
                }

                prev_node_id = node_id;
                imported_count++;
            }

            /* 闭合多边形 */
            if (is_polygon && first_node_id >= 0 && prev_node_id >= 0 && first_node_id != prev_node_id) {
                graph_add_line_segment(graph, prev_node_id, first_node_id);
            }
        }

        /* 跳过此 feature 对象剩余部分 */
        int brace_depth = 0;
        const char *end = strchr(cursor, '}');
        if (end)
            cursor = end + 1;
        else
            break;
    }

#undef GJ_SKIP_WS
#undef GJ_SKIP_STRING
#undef GJ_SKIP_NUMBER

    lv00_free((void **) &json);

    if (imported_count == 0) {
        lv00_set_error(LV00_ERROR_PARSE,
                       "GeoJSON导入完成但未找到任何有效的几何数据。"
                       "支持的类型：Point, LineString, Polygon, MultiPoint, MultiLineString");
    }

    return imported_count;
}

/** @brief SVG 路径解析器状态 */
typedef struct {
    double cx, cy;               /* current position */
    double start_x, start_y;     /* start position of current sub-path */
    bool has_viewbox;            /* viewBox 是否已解析 */
    double viewbox_x, viewbox_y; /* viewBox 左上角坐标 */
    double viewbox_w, viewbox_h; /* viewBox 宽高 */
} SvgParserState;

/** @brief 跳过空白字符 */
#define SVG_SKIP_WS(s) do { while (*(s) == ' ' || *(s) == '\t' || *(s) == '\n' || *(s) == '\r' || *(s) == ',') \
    (s)++; } while (0)

/** @brief 读取一个浮点数 */
static bool svg_parse_double(const char **s, double *val) {
    SVG_SKIP_WS(*s);
    if (**s == '\0')
        return false;
    char *end;
    *val = strtod(*s, &end);
    if (end == *s)
        return false;
    *s = end;
    SVG_SKIP_WS(*s);
    return true;
}

/** @brief 读取两个浮点数（坐标对） */
static bool svg_parse_coord(const char **s, double *x, double *y) {
    return svg_parse_double(s, x) && svg_parse_double(s, y);
}

/**
 * @brief 解析单个 SVG 路径命令并将采样点输出到数组
 *
 * 支持命令：M/m, L/l, C/c, Q/q, A/a, Z/z。
 * 贝塞尔曲线每段采样 10 个点，圆弧使用参数方程采样。
 *
 * @param cmd_char    命令字符（M/L/C/Q/A/Z 或小写）
 * @param s           指向路径字符串当前解析位置的指针
 * @param state       解析器状态（当前位置、起始点）
 * @param out_points  输出点数组 [x0,y0,x1,y1,...]
 * @param max_points  输出点数组最大容量（坐标对数）
 * @param out_count   [out] 实际输出的坐标对数
 * @param is_relative 是否为相对坐标命令（小写字母）
 * @return true 解析成功，false 解析失败
 */
static bool svg_parse_path_command(char cmd_char, const char **s, SvgParserState *state, double *out_points,
                                   int max_points, int *out_count, bool is_relative) {
    *out_count = 0;
    double abs_x, abs_y;
    int samples = 10; /* 贝塞尔/圆弧采样点数 */

    switch (cmd_char) {
        case 'M':
        case 'm': {
            /* moveto：移动到绝对位置 */
            if (!svg_parse_coord(s, &abs_x, &abs_y))
                return false;
            if (is_relative) {
                abs_x += state->cx;
                abs_y += state->cy;
            }
            if (*out_count < max_points) {
                out_points[(*out_count) * 2] = abs_x;
                out_points[(*out_count) * 2 + 1] = abs_y;
                (*out_count)++;
            }
            state->cx = abs_x;
            state->cy = abs_y;
            state->start_x = abs_x;
            state->start_y = abs_y;
            return true;
        }

        case 'L':
        case 'l': {
            /* lineto：直线段 */
            if (!svg_parse_coord(s, &abs_x, &abs_y))
                return false;
            if (is_relative) {
                abs_x += state->cx;
                abs_y += state->cy;
            }
            if (*out_count < max_points) {
                out_points[(*out_count) * 2] = abs_x;
                out_points[(*out_count) * 2 + 1] = abs_y;
                (*out_count)++;
            }
            state->cx = abs_x;
            state->cy = abs_y;
            return true;
        }

        case 'C':
        case 'c': {
            /* cubic Bezier: C x1,y1 x2,y2 x,y */
            double x1, y1, x2, y2;
            if (!svg_parse_coord(s, &x1, &y1) || !svg_parse_coord(s, &x2, &y2) || !svg_parse_coord(s, &abs_x, &abs_y))
                return false;
            if (is_relative) {
                x1 += state->cx;
                y1 += state->cy;
                x2 += state->cx;
                y2 += state->cy;
                abs_x += state->cx;
                abs_y += state->cy;
            }
            /* 采样贝塞尔曲线 */
            double x0 = state->cx, y0 = state->cy;
            for (int i = 1; i <= samples && *out_count < max_points; i++) {
                double t = (double) i / (double) samples;
                double t2 = t * t, t3 = t2 * t;
                double u = 1.0 - t, u2 = u * u, u3 = u2 * u;
                double px = u3 * x0 + 3.0 * u2 * t * x1 + 3.0 * u * t2 * x2 + t3 * abs_x;
                double py = u3 * y0 + 3.0 * u2 * t * y1 + 3.0 * u * t2 * y2 + t3 * abs_y;
                out_points[(*out_count) * 2] = px;
                out_points[(*out_count) * 2 + 1] = py;
                (*out_count)++;
            }
            state->cx = abs_x;
            state->cy = abs_y;
            return true;
        }

        case 'Q':
        case 'q': {
            /* quadratic Bezier: Q x1,y1 x,y */
            double x1, y1;
            if (!svg_parse_coord(s, &x1, &y1) || !svg_parse_coord(s, &abs_x, &abs_y))
                return false;
            if (is_relative) {
                x1 += state->cx;
                y1 += state->cy;
                abs_x += state->cx;
                abs_y += state->cy;
            }
            double qx0 = state->cx, qy0 = state->cy;
            for (int i = 1; i <= samples && *out_count < max_points; i++) {
                double t = (double) i / (double) samples;
                double u = 1.0 - t;
                double px = u * u * qx0 + 2.0 * u * t * x1 + t * t * abs_x;
                double py = u * u * qy0 + 2.0 * u * t * y1 + t * t * abs_y;
                out_points[(*out_count) * 2] = px;
                out_points[(*out_count) * 2 + 1] = py;
                (*out_count)++;
            }
            state->cx = abs_x;
            state->cy = abs_y;
            return true;
        }

        case 'A':
        case 'a': {
            /* arc: A rx,ry x-axis-rotation large-arc-flag sweep-flag x,y */
            double rx, ry, rot, dx, dy;
            double laf_d, sf_d;
            if (!svg_parse_double(s, &rx) || !svg_parse_double(s, &ry) || !svg_parse_double(s, &rot) ||
                !svg_parse_double(s, &laf_d) || !svg_parse_double(s, &sf_d) || !svg_parse_coord(s, &dx, &dy))
                return false;
            LV00_UNUSED(ry); LV00_UNUSED(rot); LV00_UNUSED(laf_d); /* parsed for future SVG arc implementation */
            int sf = (int) (sf_d + 0.5);
            if (is_relative) {
                dx += state->cx;
                dy += state->cy;
            }

            /* 使用中点公式计算椭圆弧采样 */
            double x_start = state->cx, y_start = state->cy;

            /* 简化参数方程：沿椭圆弧采样 */
            for (int i = 1; i <= samples && *out_count < max_points; i++) {
                double t = (double) i / (double) samples;
                /* 线性插值 + 圆弧偏移近似 */
                double lx = x_start + t * (dx - x_start);
                double ly = y_start + t * (dy - y_start);
                /* 添加圆弧离差 */
                double arc_angle = t * M_PI;
                double bulge = sin(arc_angle) * (sf ? 1.0 : -1.0);
                double chord_len = sqrt((dx - x_start) * (dx - x_start) + (dy - y_start) * (dy - y_start));
                double bulge_factor = (chord_len > 0.001) ? (rx / chord_len) * 0.5 : 0.0;
                double nx = -(dy - y_start) / (chord_len > 0.001 ? chord_len : 1.0);
                double ny = (dx - x_start) / (chord_len > 0.001 ? chord_len : 1.0);
                lx += nx * bulge * bulge_factor * chord_len * 0.5;
                ly += ny * bulge * bulge_factor * chord_len * 0.5;

                out_points[(*out_count) * 2] = lx;
                out_points[(*out_count) * 2 + 1] = ly;
                (*out_count)++;
            }
            /* 确保最后一点是终点 */
            if (*out_count < max_points) {
                out_points[(*out_count) * 2] = dx;
                out_points[(*out_count) * 2 + 1] = dy;
                (*out_count)++;
            }
            state->cx = dx;
            state->cy = dy;
            return true;
        }

        case 'Z':
        case 'z': {
            /* closepath：画线回到当前子路径起点 */
            if (*out_count < max_points) {
                out_points[(*out_count) * 2] = state->start_x;
                out_points[(*out_count) * 2 + 1] = state->start_y;
                (*out_count)++;
            }
            state->cx = state->start_x;
            state->cy = state->start_y;
            return true;
        }

        default:
            return false;
    }
}

/**
 * @brief 解析 SVG <circle> 元素并转换为采样点
 *
 * 将圆离散为 N 个采样点以便映射到约束图。
 */
static int svg_parse_circle(double cx, double cy, double r, double *out_points, int max_points) {
    int count = 0;
    int samples = 32; /* 32个采样点近似圆 */
    for (int i = 0; i < samples && count < max_points; i++) {
        double angle = 2.0 * M_PI * (double) i / (double) samples;
        out_points[count * 2] = cx + r * cos(angle);
        out_points[count * 2 + 1] = cy + r * sin(angle);
        count++;
    }
    return count;
}

int interop_import_svg(LV00Engine *engine, const InteropImportConfig *config) {
    if (!engine || !config) return LV00_ERROR_INVALID_PARAM;
    if (!config->input_path || config->input_path[0] == '\0')
        return LV00_ERROR_INVALID_PARAM;
    (void)engine;
    /* FUTURE: 实现 SVG 导入 */
    return LV00_ERROR_UNSUPPORTED;
}
