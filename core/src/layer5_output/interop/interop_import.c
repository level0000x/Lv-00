/**
 * @file interop_import.c
 * @brief 导入（GeoGebra/SVG）
 *
 * @details 拆分子模块（Lv-00 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/lv_platform.h"
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "lv/lv_file.h"

#include "lv/constraint_graph.h"
#include "lv/engine.h"
#include "lv/interop.h"
#include "lv/lv_json.h"
#include "lv/lv_parse_utils.h"
#include "lv/lv_str_utils.h"
#include "lv/geo_utils.h"
#include "lv/lv_numeric.h"


#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"


/* ── GeoGebra ZIP 解析常量 ── */

#define GGB_EOCD_MIN_SIZE 22
#define GGB_EOCD_SIG 0x06054b50
#define GGB_CENTRAL_DIR_MIN 46
#define GGB_LOCAL_FILE_SIG 0x04034b50
#define GGB_LOCAL_HEADER_MIN 30 /* ZIP local file header fixed size */
#define GGB_CENTRAL_DIR_SIG 0x02014b50

/* EOCD 记录位于文件末尾，尾部最多可附加 65535 字节注释，反向搜索窗口 = 22 + 65535 */
#define GGB_EOCD_MAX_SEARCH 65557

/* ZIP 压缩方法 */
#define GGB_COMP_STORE 0    /* 不压缩（存储） */
#define GGB_COMP_DEFLATE 8  /* Deflate 压缩 */

/* 单次导入最多处理的 element 数量上限 */
#define GGB_MAX_ELEMENTS 4096

/* INTEROP_COORD_DENOM_PRECISION 统一来自 lv/interop.h 的公共常量 */

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
 * @brief 从 XML 开标签中提取属性值（返回原始缓冲区内的指针与长度，避免大值拷贝）
 *
 * 与 ggb_extract_attr 的区别：不复制到调用者缓冲区，而是返回指向
 * 标签文本内部的值指针。适用于 path d / points 等可能很长的属性，
 * 由调用者自行决定拷贝策略。
 *
 * @param tag_start 开标签起始位置（'<' 的位置）
 * @param tag_len   开标签长度（'<' 到 '>' 之间）
 * @param attr_name 属性名称
 * @param out_value [out] 输出属性值指针（位于 tag_start 内部）
 * @param out_len   [out] 输出属性值长度
 * @return true 找到属性，false 未找到
 */
static bool ggb_extract_attr_len(const char *tag_start, size_t tag_len, const char *attr_name,
                                 const char **out_value, size_t *out_len) {
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
            while (val_start + j < tag_len && tag_start[val_start + j] != quote)
                j++;
            *out_value = tag_start + val_start;
            *out_len = j;
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
        double num = 0.0, den = 0.0;
        lv_parse_double(val_buf, &num);
        lv_parse_double(slash + 1, &den);
        if (den == 0.0)
            return false;
        *value = num / den;
        return true;
    }

    lv_parse_double(val_buf, value);
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

/* 前向声明：SVG 圆采样解析器（定义见文件后部），供 GeoGebra 圆导入复用 */
static int svg_parse_circle(double cx, double cy, double r, double *out_points, int max_points);

/* ==================== GeoGebra ZIP 解析器 ==================== */

/**
 * @brief 在文件末尾反向搜索 EOCD（End of Central Directory）记录
 *
 * EOCD 固定签名 0x06054b50，位于 ZIP 文件末尾；因尾部可能附加
 * 最多 65535 字节注释，从 size-22 起向前搜索 GGB_EOCD_MAX_SEARCH 字节。
 *
 * @param data        文件数据缓冲区
 * @param size        数据总大小
 * @param eocd_offset [out] 输出 EOCD 的字节偏移
 * @return true 找到 EOCD，false 未找到
 */
static bool ggb_find_eocd(const uint8_t *data, size_t size, size_t *eocd_offset) {
    if (size < GGB_EOCD_MIN_SIZE)
        return false;
    size_t window = (size > GGB_EOCD_MAX_SEARCH) ? GGB_EOCD_MAX_SEARCH : size;
    size_t min_i = (size > window) ? (size - window) : 0;
    size_t i = size - GGB_EOCD_MIN_SIZE;
    while (i >= min_i) {
        if (ggb_read_u32_le(data, i) == GGB_EOCD_SIG) {
            *eocd_offset = i;
            return true;
        }
        if (i == 0)
            break;
        i--;
    }
    return false;
}

/**
 * @brief 从中央目录中查找指定文件名的条目
 *
 * 遍历中央目录条目（每条 46 字节固定头 + 变长文件名/额外字段/注释），
 * 按文件名精确匹配，返回本地文件头偏移与大小信息。
 *
 * @param data        文件数据缓冲区
 * @param data_size   数据总大小
 * @param eocd_offset EOCD 偏移
 * @param target      目标文件名（如 "geogebra.xml"）
 * @param local_offset [out] 输出本地文件头偏移
 * @param comp_size    [out] 输出压缩后大小
 * @param uncomp_size  [out] 输出解压后大小
 * @param comp_method  [out] 输出压缩方法
 * @return true 找到，false 未找到
 */
static bool ggb_central_find_entry(const uint8_t *data, size_t data_size, size_t eocd_offset,
                                   const char *target, size_t *local_offset, size_t *comp_size,
                                   size_t *uncomp_size, uint16_t *comp_method) {
    size_t total_entries = ggb_read_u16_le(data, eocd_offset + 10);
    size_t cd_size = ggb_read_u32_le(data, eocd_offset + 12);
    size_t cd_offset = ggb_read_u32_le(data, eocd_offset + 16);
    if (cd_offset > data_size || cd_size > data_size - cd_offset)
        return false;

    size_t pos = cd_offset;
    size_t target_len = strlen(target);
    for (size_t e = 0; e < total_entries && pos + GGB_CENTRAL_DIR_MIN <= data_size; e++) {
        if (ggb_read_u32_le(data, pos) != GGB_CENTRAL_DIR_SIG)
            return false;
        uint16_t method = ggb_read_u16_le(data, pos + 10);
        size_t csize = ggb_read_u32_le(data, pos + 20);
        size_t usize = ggb_read_u32_le(data, pos + 24);
        uint16_t name_len = ggb_read_u16_le(data, pos + 28);
        uint16_t extra_len = ggb_read_u16_le(data, pos + 30);
        uint16_t comment_len = ggb_read_u16_le(data, pos + 32);
        size_t local_off = ggb_read_u32_le(data, pos + 42);

        size_t name_pos = pos + GGB_CENTRAL_DIR_MIN;
        if (name_pos > data_size || name_len > data_size - name_pos)
            return false;
        if (name_len == target_len && memcmp(data + name_pos, target, target_len) == 0) {
            *local_offset = local_off;
            *comp_size = csize;
            *uncomp_size = usize;
            *comp_method = method;
            return true;
        }
        pos = name_pos + name_len + extra_len + comment_len;
    }
    return false;
}

/**
 * @brief 从本地文件头中计算实际文件数据的偏移
 *
 * 本地文件头：签名(4) + 固定字段(22) + 文件名长度(2)@26 + 额外字段长度(2)@28，
 * 之后为 文件名 + 额外字段 + 文件数据。
 *
 * @param data         文件数据缓冲区
 * @param data_size    数据总大小
 * @param local_offset 本地文件头偏移
 * @param data_offset  [out] 输出实际文件数据偏移
 * @return true 成功，false 失败
 */
static bool ggb_local_data_offset(const uint8_t *data, size_t data_size, size_t local_offset,
                                  size_t *data_offset) {
    if (local_offset > data_size || GGB_LOCAL_HEADER_MIN > data_size - local_offset)
        return false;
    if (ggb_read_u32_le(data, local_offset) != GGB_LOCAL_FILE_SIG)
        return false;
    uint16_t name_len = ggb_read_u16_le(data, local_offset + 26);
    uint16_t extra_len = ggb_read_u16_le(data, local_offset + 28);
    size_t off = local_offset + GGB_LOCAL_HEADER_MIN + name_len + extra_len;
    if (off > data_size)
        return false;
    *data_offset = off;
    return true;
}

/* ---- raw Deflate（RFC 1951）自研解压器：存储块/固定哈夫曼/动态哈夫曼 ---- */

/** @brief 位读取器（LSB-first） */
typedef struct {
    const uint8_t *src;
    size_t src_len;
    size_t bit_pos;
} GgbBitReader;

/** @brief 读取 n 位（n <= 32），低位在前 */
static bool ggb_bit_read(GgbBitReader *br, unsigned n, uint32_t *out) {
    if (n > 32)
        return false;
    uint32_t val = 0;
    for (unsigned i = 0; i < n; i++) {
        size_t byte_off = br->bit_pos >> 3;
        if (byte_off >= br->src_len)
            return false;
        uint32_t b = (uint32_t) ((br->src[byte_off] >> (br->bit_pos & 7)) & 1u);
        val |= b << i;
        br->bit_pos++;
    }
    *out = val;
    return true;
}

/** @brief 按字节对齐（存储块使用） */
static void ggb_bit_align(GgbBitReader *br) {
    br->bit_pos = (br->bit_pos + 7u) & ~(size_t) 7u;
}

/** @brief 规范哈夫曼解码表（码长上限 15 位） */
typedef struct {
    uint16_t counts[16];      /* counts[len] = 码长为 len 的符号数量 */
    uint16_t symbols[288 + 32]; /* 按 (码长, 码值) 规范排序的符号表 */
    uint16_t offs[16];        /* 每个码长在 symbols 中的起始索引 */
} GgbHuffTable;

/** @brief 由码长数组构建规范哈夫曼表 */
static void ggb_huff_build(GgbHuffTable *t, const uint8_t *lengths, int n) {
    memset(t, 0, sizeof(*t));
    for (int i = 0; i < n; i++)
        t->counts[lengths[i]]++;
    t->counts[0] = 0;

    int offs[16];
    offs[0] = 0;
    for (int len = 1; len < 16; len++)
        offs[len] = offs[len - 1] + (int) t->counts[len - 1];
    for (int i = 0; i < n; i++) {
        if (lengths[i] != 0)
            t->symbols[offs[lengths[i]]++] = (uint16_t) i;
    }
    t->offs[1] = 0;
    for (int len = 2; len < 16; len++)
        t->offs[len] = (uint16_t) ((int) t->offs[len - 1] + (int) t->counts[len - 1]);
}

/** @brief 从哈夫曼表解码一个符号 */
static bool ggb_huff_decode(GgbBitReader *br, const GgbHuffTable *t, int *symbol) {
    uint32_t code = 0;
    int first = 0;
    int index = 0;
    for (int len = 1; len <= 15; len++) {
        uint32_t bit;
        if (!ggb_bit_read(br, 1, &bit))
            return false;
        code = (code << 1) | bit;
        int count = (int) t->counts[len];
        if (code - (uint32_t) first < (uint32_t) count) {
            *symbol = (int) t->symbols[index + (int) (code - (uint32_t) first)];
            return true;
        }
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return false;
}

/* Deflate 长度码表（符号 257..285） */
static const uint16_t ggb_length_base[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};
static const uint8_t ggb_length_extra[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
};

/* Deflate 距离码表（符号 0..29） */
static const uint16_t ggb_dist_base[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
};
static const uint8_t ggb_dist_extra[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};

/** @brief 用字面/长度与距离两张哈夫曼表解码 LZ77 符号流 */
static bool ggb_inflate_lz77(GgbBitReader *br, const GgbHuffTable *lit, const GgbHuffTable *dist,
                             uint8_t *dst, size_t dst_cap, size_t *out_pos) {
    for (;;) {
        int sym;
        if (!ggb_huff_decode(br, lit, &sym))
            return false;
        if (sym < 256) {
            if (*out_pos >= dst_cap)
                return false;
            dst[(*out_pos)++] = (uint8_t) sym;
        } else if (sym == 256) {
            return true; /* 块结束 */
        } else {
            int len_idx = sym - 257;
            if (len_idx < 0 || len_idx >= 29)
                return false;
            uint32_t extra;
            if (!ggb_bit_read(br, ggb_length_extra[len_idx], &extra))
                return false;
            size_t length = (size_t) ggb_length_base[len_idx] + (size_t) extra;

            int d_sym;
            if (!ggb_huff_decode(br, dist, &d_sym))
                return false;
            if (d_sym < 0 || d_sym >= 30)
                return false;
            if (!ggb_bit_read(br, ggb_dist_extra[d_sym], &extra))
                return false;
            size_t distance = (size_t) ggb_dist_base[d_sym] + (size_t) extra;
            if (distance == 0 || distance > *out_pos)
                return false;
            if (*out_pos + length > dst_cap)
                return false;
            /* LZ77 拷贝（支持重叠，逐字节保证正确性） */
            for (size_t i = 0; i < length; i++)
                dst[*out_pos + i] = dst[*out_pos - distance + i];
            *out_pos += length;
        }
    }
}

/** @brief 构建动态哈夫曼表（块类型 2 的码长描述部分） */
static bool ggb_huff_dynamic(GgbBitReader *br, GgbHuffTable *lit, GgbHuffTable *dist) {
    static const uint8_t kCodeOrder[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    uint32_t hlit, hdist, hclen;
    if (!ggb_bit_read(br, 5, &hlit) || !ggb_bit_read(br, 5, &hdist) || !ggb_bit_read(br, 4, &hclen))
        return false;
    size_t nlit = (size_t) hlit + 257u;
    size_t ndist = (size_t) hdist + 1u;
    size_t nclen = (size_t) hclen + 4u;
    if (nlit > 288 || ndist > 32 || nclen > 19)
        return false;

    uint8_t cl_lengths[19];
    memset(cl_lengths, 0, sizeof(cl_lengths));
    for (size_t i = 0; i < nclen; i++) {
        uint32_t v;
        if (!ggb_bit_read(br, 3, &v))
            return false;
        cl_lengths[kCodeOrder[i]] = (uint8_t) v;
    }

    GgbHuffTable cl_tree;
    ggb_huff_build(&cl_tree, cl_lengths, 19);

    uint8_t lengths[288 + 32];
    size_t total = nlit + ndist;
    size_t n = 0;
    while (n < total) {
        int sym;
        if (!ggb_huff_decode(br, &cl_tree, &sym))
            return false;
        if (sym < 16) {
            if (n >= sizeof(lengths))
                return false;
            lengths[n++] = (uint8_t) sym;
        } else if (sym == 16) {
            uint32_t rep;
            if (!ggb_bit_read(br, 2, &rep))
                return false;
            if (n == 0)
                return false;
            uint8_t prev = lengths[n - 1];
            for (uint32_t i = 0; i < rep + 3u; i++) {
                if (n >= total)
                    return false;
                lengths[n++] = prev;
            }
        } else if (sym == 17) {
            uint32_t rep;
            if (!ggb_bit_read(br, 3, &rep))
                return false;
            for (uint32_t i = 0; i < rep + 3u; i++) {
                if (n >= total)
                    return false;
                lengths[n++] = 0;
            }
        } else { /* 18 */
            uint32_t rep;
            if (!ggb_bit_read(br, 7, &rep))
                return false;
            for (uint32_t i = 0; i < rep + 11u; i++) {
                if (n >= total)
                    return false;
                lengths[n++] = 0;
            }
        }
    }
    if (n != total)
        return false;

    ggb_huff_build(lit, lengths, (int) nlit);
    ggb_huff_build(dist, lengths + nlit, (int) ndist);
    return true;
}

/** @brief 解压一个 raw Deflate 流（可含多个块） */
static bool ggb_inflate_blocks(GgbBitReader *br, uint8_t *dst, size_t dst_cap, size_t *out_len) {
    size_t out_pos = 0;
    for (;;) {
        uint32_t bfinal, btype;
        if (!ggb_bit_read(br, 1, &bfinal) || !ggb_bit_read(br, 2, &btype))
            return false;
        if (btype == 0) {
            /* 存储块：字节对齐 + LEN/NLEN + 原始数据 */
            ggb_bit_align(br);
            uint32_t len, nlen;
            if (!ggb_bit_read(br, 16, &len) || !ggb_bit_read(br, 16, &nlen))
                return false;
            if ((len ^ 0xFFFFu) != nlen)
                return false;
            if (out_pos + (size_t) len > dst_cap)
                return false;
            size_t byte_start = br->bit_pos >> 3;
            if (byte_start > br->src_len || (size_t) len > br->src_len - byte_start)
                return false;
            memcpy(dst + out_pos, br->src + byte_start, len);
            out_pos += len;
            br->bit_pos += (size_t) len * 8u;
        } else if (btype == 1) {
            /* 固定哈夫曼 */
            uint8_t fixed_lit[288];
            int i;
            for (i = 0; i < 144; i++) fixed_lit[i] = 8;
            for (; i < 256; i++) fixed_lit[i] = 9;
            for (; i < 280; i++) fixed_lit[i] = 7;
            for (; i < 288; i++) fixed_lit[i] = 8;
            uint8_t fixed_dist[30];
            for (i = 0; i < 30; i++) fixed_dist[i] = 5;
            GgbHuffTable lit, dist;
            ggb_huff_build(&lit, fixed_lit, 288);
            ggb_huff_build(&dist, fixed_dist, 30);
            if (!ggb_inflate_lz77(br, &lit, &dist, dst, dst_cap, &out_pos))
                return false;
        } else {
            /* 动态哈夫曼（块类型 2） */
            GgbHuffTable lit, dist;
            if (!ggb_huff_dynamic(br, &lit, &dist))
                return false;
            if (!ggb_inflate_lz77(br, &lit, &dist, dst, dst_cap, &out_pos))
                return false;
        }
        if (bfinal)
            break;
    }
    *out_len = out_pos;
    return true;
}

/**
 * @brief raw Deflate（RFC 1951）解压
 *
 * ZIP 中 Deflate 条目是裸 deflate 流（无 zlib 头）。
 *
 * 默认使用自研解压器（支持存储块/固定哈夫曼/动态哈夫曼），
 * 不依赖外部链接；若构建系统通过 -DLV_USE_ZLIB 显式启用并链接
 * zlib 库，则优先调用 zlib（inflateInit2 以 windowBits=-15 禁用
 * zlib 头校验）。
 *
 * @param src      源数据（压缩）
 * @param src_len  源数据长度
 * @param dst      目标缓冲区（解压后）
 * @param dst_cap  目标缓冲区容量
 * @param out_len  [out] 实际解压长度
 * @return true 成功，false 失败
 */
static bool ggb_inflate_raw(const uint8_t *src, size_t src_len, uint8_t *dst, size_t dst_cap,
                            size_t *out_len) {
#ifdef LV_USE_ZLIB
    if (dst_cap > UINT_MAX || src_len > UINT_MAX)
        return false;
    z_stream strm;
    memset(&strm, 0, sizeof(strm));
    if (inflateInit2(&strm, -MAX_WBITS) != Z_OK)
        return false;
    strm.next_in = (Bytef *) src;
    strm.avail_in = (uInt) src_len;
    strm.next_out = (Bytef *) dst;
    strm.avail_out = (uInt) dst_cap;
    int ret = inflate(&strm, Z_FINISH);
    size_t produced = dst_cap - (size_t) strm.avail_out;
    inflateEnd(&strm);
    if (ret != Z_STREAM_END)
        return false;
    *out_len = produced;
    return true;
#else
    GgbBitReader br;
    br.src = src;
    br.src_len = src_len;
    br.bit_pos = 0;
    return ggb_inflate_blocks(&br, dst, dst_cap, out_len);
#endif
}

/**
 * @brief 提取并解压 ZIP 条目（STORE 直接拷贝 / Deflate 用 zlib 解压）
 *
 * @param data         文件数据缓冲区
 * @param data_size    数据总大小
 * @param data_offset  条目数据偏移（来自本地文件头）
 * @param comp_size    压缩后大小
 * @param uncomp_size  解压后大小
 * @param comp_method  压缩方法（GGB_COMP_STORE / GGB_COMP_DEFLATE）
 * @param out_buf      [out] 输出的条目内容（调用者 lv_free）
 * @param out_len      [out] 输出内容长度
 * @return true 成功，false 失败
 */
static bool ggb_extract_entry(const uint8_t *data, size_t data_size, size_t data_offset,
                              size_t comp_size, size_t uncomp_size, uint16_t comp_method,
                              uint8_t **out_buf, size_t *out_len) {
    if (data_offset > data_size || comp_size > data_size - data_offset)
        return false;
    if (comp_method == GGB_COMP_STORE) {
        uint8_t *buf = (uint8_t *) lv_malloc(comp_size ? comp_size : 1);
        if (!buf)
            return false;
        memcpy(buf, data + data_offset, comp_size);
        *out_buf = buf;
        *out_len = comp_size;
        return true;
    }
    if (comp_method == GGB_COMP_DEFLATE) {
        uint8_t *buf = (uint8_t *) lv_malloc(uncomp_size ? uncomp_size : 1);
        if (!buf)
            return false;
        size_t produced = 0;
        if (!ggb_inflate_raw(data + data_offset, comp_size, buf, uncomp_size, &produced)) {
            lv_free((void **) &buf);
            return false;
        }
        *out_buf = buf;
        *out_len = produced;
        return true;
    }
    return false;
}

/* ==================== GeoGebra XML 解析器 ==================== */

/**
 * @brief 在 XML 文本中查找下一个 <element 开标签
 *
 * 只匹配完整的 "element" 标签名（后续字符必须是空白/'>'/'/'，避免
 * 误匹配 "elementx" 之类的前缀），跳过闭合标签与声明/注释。
 *
 * @param xml           XML 文本
 * @param len           XML 文本长度
 * @param start         搜索起始偏移
 * @param open_start    [out] 输出开标签起始偏移（'<'）
 * @param open_end      [out] 输出开标签结束偏移（'>'）
 * @param content_start [out] 输出内容起始偏移（'>' 之后）
 * @return true 找到，false 未找到
 */
static bool ggb_find_element_open(const char *xml, size_t len, size_t start, size_t *open_start,
                                  size_t *open_end, size_t *content_start) {
    static const char kTag[] = "element";
    const size_t tlen = sizeof(kTag) - 1;
    for (size_t i = start; i + tlen + 2 < len; i++) {
        if (xml[i] != '<')
            continue;
        if (xml[i + 1] == '/' || xml[i + 1] == '!' || xml[i + 1] == '?')
            continue;
        if (memcmp(xml + i + 1, kTag, tlen) != 0)
            continue;
        char after = xml[i + 1 + tlen];
        if (after != ' ' && after != '>' && after != '/' && after != '\t' && after != '\n' && after != '\r')
            continue;
        const char *gt = memchr(xml + i, '>', len - i);
        if (!gt)
            return false;
        *open_start = i;
        *open_end = (size_t) (gt - xml);
        *content_start = *open_end + 1;
        return true;
    }
    return false;
}

/**
 * @brief 查找指定标签的闭合标签 "</tagName" 的起始位置
 *
 * @param xml       XML 文本
 * @param len       XML 文本长度
 * @param start     搜索起始偏移
 * @param tag_name  标签名称（不含 <>）
 * @return 闭合标签起始偏移，未找到返回 (size_t) -1
 */
static size_t ggb_find_close_tag(const char *xml, size_t len, size_t start, const char *tag_name) {
    size_t tlen = strlen(tag_name);
    for (size_t i = start; i + tlen + 2 < len; i++) {
        if (xml[i] != '<' || xml[i + 1] != '/')
            continue;
        if (memcmp(xml + i + 2, tag_name, tlen) != 0)
            continue;
        char after = xml[i + 2 + tlen];
        if (after == ' ' || after == '>' || after == '\t' || after == '\n' || after == '\r')
            return i;
    }
    return (size_t) -1;
}

/**
 * @brief 在内容范围内查找指定子标签的开标签
 *
 * @param xml        XML 文本
 * @param start      搜索起始偏移
 * @param end        搜索结束偏移（不含）
 * @param tag_name   子标签名称（如 "coords"、"center"）
 * @param tag_start  [out] 输出开标签起始偏移（'<'）
 * @param tag_end    [out] 输出开标签结束偏移（'>'）
 * @return true 找到，false 未找到
 */
static bool ggb_find_child_tag(const char *xml, size_t start, size_t end, const char *tag_name,
                               size_t *tag_start, size_t *tag_end) {
    size_t tlen = strlen(tag_name);
    for (size_t i = start; i + tlen + 1 < end; i++) {
        if (xml[i] != '<')
            continue;
        if (xml[i + 1] == '/' || xml[i + 1] == '!' || xml[i + 1] == '?')
            continue;
        if (memcmp(xml + i + 1, tag_name, tlen) != 0)
            continue;
        char after = xml[i + 1 + tlen];
        if (after != ' ' && after != '>' && after != '/' && after != '\t' && after != '\n' && after != '\r')
            continue;
        const char *gt = memchr(xml + i, '>', end - i);
        if (!gt)
            return false;
        *tag_start = i;
        *tag_end = (size_t) (gt - xml);
        return true;
    }
    return false;
}

/**
 * @brief 提取子标签的文本内容（如 <equation>...</equation>）
 *
 * @param xml       XML 文本
 * @param start     内容范围起始偏移
 * @param end       内容范围结束偏移（不含）
 * @param tag_name  子标签名称
 * @param out       输出缓冲区
 * @param out_size  输出缓冲区大小
 * @return true 成功，false 失败
 */
static bool ggb_extract_child_text(const char *xml, size_t start, size_t end, const char *tag_name,
                                   char *out, size_t out_size) {
    if (out_size == 0)
        return false;
    out[0] = '\0';
    size_t tlen = strlen(tag_name);
    for (size_t i = start; i + tlen + 2 <= end; i++) {
        if (xml[i] != '<')
            continue;
        if (xml[i + 1] == '/' || xml[i + 1] == '!' || xml[i + 1] == '?')
            continue;
        if (memcmp(xml + i + 1, tag_name, tlen) != 0)
            continue;
        char after = xml[i + 1 + tlen];
        if (after != ' ' && after != '>' && after != '/' && after != '\t' && after != '\n' && after != '\r')
            continue;
        const char *gt = memchr(xml + i, '>', end - i);
        if (!gt)
            return false;
        const char *text = gt + 1;
        const char *close = memchr(text, '<', (size_t) ((xml + end) - text));
        if (!close)
            return false;
        lv_strlcpy_n(out, out_size, text, (size_t) (close - text));
        return true;
    }
    return false;
}

/* ==================== GeoGebra 导入辅助 ==================== */

/** @brief XML 中单个 <element> 的解析结果 */
typedef struct {
    char type[32];    /**< type 属性（point/segment/circle/line/polygon...） */
    char label[256];  /**< label 属性（如 "A"、"poly1"） */
    size_t open_start;   /**< 开标签起始偏移（'<'） */
    size_t open_end;     /**< 开标签结束偏移（'>'） */
    size_t content_start; /**< 内容起始偏移（开标签 '>' 之后） */
    size_t content_end;   /**< 内容结束偏移（闭合标签 '<'） */
    bool self_closing;    /**< 是否为自闭合标签 <element .../> */
} GgbElementEntry;

/**
 * @brief 将 double 坐标转为有理数并作为点节点加入约束图
 *
 * @param graph 约束图
 * @param x     x 坐标
 * @param y     y 坐标
 * @return 新节点 ID，失败返回 -1
 */
static int ggb_add_point_node(ConstraintGraph *graph, double x, double y) {
    SymbolicCoord *cx = ggb_double_to_rational(x);
    SymbolicCoord *cy = ggb_double_to_rational(y);
    if (!cx || !cy) {
        if (cx)
            symbolic_coord_destroy(cx);
        if (cy)
            symbolic_coord_destroy(cy);
        return -1;
    }
    SymbolicCoord *coords[2] = {cx, cy};
    AddNodeResult res = graph_add_point(graph, coords, 2);
    if (res != ADD_NODE_OK) {
        symbolic_coord_destroy(cx);
        symbolic_coord_destroy(cy);
        return -1;
    }
    return (int) (graph->next_node_id - 1);
}

/**
 * @brief 将采样点序列导入约束图（相邻点连线段，可闭合）
 *
 * @param graph     约束图
 * @param pts       采样点数组 [x0,y0,x1,y1,...]
 * @param n         采样点数量（坐标对数）
 * @param close_loop 是否将末点连回首点（闭合）
 * @return 实际导入的点数
 */
static int ggb_import_point_sequence(ConstraintGraph *graph, const double *pts, int n, bool close_loop) {
    int imported = 0;
    int first_id = -1, prev_id = -1;
    for (int i = 0; i < n; i++) {
        int node_id = ggb_add_point_node(graph, pts[i * 2], pts[i * 2 + 1]);
        if (node_id < 0)
            continue;
        if (first_id < 0)
            first_id = node_id;
        if (prev_id >= 0)
            graph_add_line_segment(graph, prev_id, node_id);
        prev_id = node_id;
        imported++;
    }
    if (close_loop && first_id >= 0 && prev_id >= 0 && first_id != prev_id)
        graph_add_line_segment(graph, prev_id, first_id);
    return imported;
}

/**
 * @brief 提取点元素坐标（元素标签 x/y 或 <coords x y> 子标签）
 *
 * @param xml  XML 文本
 * @param e    元素解析结果
 * @param px   [out] 输出 x 坐标
 * @param py   [out] 输出 y 坐标
 * @return true 成功，false 失败
 */
static bool ggb_element_point_coord(const char *xml, const GgbElementEntry *e, double *px, double *py) {
    size_t tag_len = e->open_end - e->open_start;
    char xb[64], yb[64];
    if (ggb_extract_attr(xml + e->open_start, tag_len, "x", xb, sizeof(xb)) &&
        ggb_extract_attr(xml + e->open_start, tag_len, "y", yb, sizeof(yb))) {
        if (lv_parse_double(xb, px) == 0 && lv_parse_double(yb, py) == 0)
            return true;
    }
    if (!e->self_closing) {
        size_t cs = 0, ce = 0;
        if (ggb_find_child_tag(xml, e->content_start, e->content_end, "coords", &cs, &ce)) {
            size_t cl = ce - cs;
            if (ggb_extract_attr(xml + cs, cl, "x", xb, sizeof(xb)) &&
                ggb_extract_attr(xml + cs, cl, "y", yb, sizeof(yb))) {
                if (lv_parse_double(xb, px) == 0 && lv_parse_double(yb, py) == 0)
                    return true;
            }
        }
    }
    return false;
}

/**
 * @brief 解析 element 内容中 child_tag 引用的点节点
 *
 * 优先级：子标签 P 属性（construction 索引）→ 子标签 label 属性 →
 * 子标签内联 x/y 坐标。前两者命中已导入的节点时返回节点 ID（>=0），
 * 内联坐标经 has_coord 与 ox/oy 输出。
 *
 * @param xml        XML 文本
 * @param e          元素解析结果
 * @param child_tag  子标签名称（startPoint/endPoint/center）
 * @param entries    全部元素解析结果数组
 * @param node_by_idx 按 construction 索引映射的节点 ID 数组（-1 表示未导入）
 * @param el_count   元素总数
 * @param ox         [out] 内联 x 坐标
 * @param oy         [out] 内联 y 坐标
 * @param has_coord  [out] 是否得到内联坐标
 * @return 引用节点 ID；无法引用时返回 -1
 */
static int ggb_resolve_point_ref(const char *xml, const GgbElementEntry *e, const char *child_tag,
                                 const GgbElementEntry *entries, const int *node_by_idx, int el_count,
                                 double *ox, double *oy, bool *has_coord) {
    *has_coord = false;
    if (e->self_closing)
        return -1;
    size_t cs = 0, ce = 0;
    if (!ggb_find_child_tag(xml, e->content_start, e->content_end, child_tag, &cs, &ce))
        return -1;
    size_t clen = ce - cs;
    char buf[128];

    /* P 属性：construction 全局索引 */
    if (ggb_extract_attr(xml + cs, clen, "P", buf, sizeof(buf))) {
        int idx = atoi(buf);
        if (idx >= 0 && idx < el_count && node_by_idx[idx] >= 0)
            return node_by_idx[idx];
    }
    /* label 属性：按标签名匹配已导入的 point */
    if (ggb_extract_attr(xml + cs, clen, "label", buf, sizeof(buf))) {
        for (int i = 0; i < el_count; i++) {
            if (node_by_idx[i] >= 0 && entries[i].label[0] != '\0' &&
                lv_str_eq(entries[i].label, buf))
                return node_by_idx[i];
        }
    }
    /* 内联坐标 */
    char xb[64], yb[64];
    if (ggb_extract_attr(xml + cs, clen, "x", xb, sizeof(xb)) &&
        ggb_extract_attr(xml + cs, clen, "y", yb, sizeof(yb))) {
        if (lv_parse_double(xb, ox) == 0 && lv_parse_double(yb, oy) == 0)
            *has_coord = true;
    }
    return -1;
}

/**
 * @brief 从 <equation> 子标签文本提取圆的半径
 *
 * 支持两种常见格式：
 *   "((x - (0))^(2)) + ((y - (0))^(2)) = (1)^(2)"  — 括号内是 r^2
 *   "x^2 + y^2 = 4"                                — RHS 直接是 r^2
 *
 * @param xml     XML 文本
 * @param e       元素解析结果
 * @param radius  [out] 输出半径
 * @return true 成功，false 失败
 */
static bool ggb_extract_equation_radius(const char *xml, const GgbElementEntry *e, double *radius) {
    if (e->self_closing)
        return false;
    char eq[512];
    if (!ggb_extract_child_text(xml, e->content_start, e->content_end, "equation", eq, sizeof(eq)))
        return false;
    const char *rhs = strchr(eq, '=');
    if (!rhs)
        return false;
    rhs++;
    while (*rhs == ' ' || *rhs == '\t')
        rhs++;
    /* 格式1：= (num)^(2) */
    if (*rhs == '(') {
        char num[64];
        size_t k = 0;
        const char *p = rhs + 1;
        while (k < sizeof(num) - 1 && ((*p >= '0' && *p <= '9') || *p == '.' || *p == '-'))
            num[k++] = *p++;
        num[k] = '\0';
        if (k > 0) {
            double val = 0;
            if (lv_parse_double(num, &val) == 0 && val >= 0.0) {
                *radius = sqrt(val);
                return true;
            }
        }
    }
    /* 格式2：RHS 直接为数值 */
    double val = 0;
    if (lv_parse_double(rhs, &val) == 0 && val >= 0.0) {
        *radius = sqrt(val);
        return true;
    }
    return false;
}

/**
 * @brief 导入 segment / line 元素（startPoint + endPoint → 线段）
 *
 * @return 导入的线段数（0 表示无有效端点，-1 表示内部错误）
 */
static int ggb_import_segment_or_line(ConstraintGraph *graph, const char *xml, const GgbElementEntry *e,
                                      const GgbElementEntry *entries, const int *node_by_idx,
                                      int el_count) {
    double sx = 0, sy = 0, ex = 0, ey = 0;
    bool has_s = false, has_e = false;
    int s_id = ggb_resolve_point_ref(xml, e, "startPoint", entries, node_by_idx, el_count, &sx, &sy, &has_s);
    int e_id = ggb_resolve_point_ref(xml, e, "endPoint", entries, node_by_idx, el_count, &ex, &ey, &has_e);

    if (s_id >= 0 && e_id >= 0) {
        graph_add_line_segment(graph, s_id, e_id);
        return 1;
    }
    if (s_id >= 0 && has_e) {
        int en = ggb_add_point_node(graph, ex, ey);
        if (en < 0)
            return 0;
        graph_add_line_segment(graph, s_id, en);
        return 1;
    }
    if (e_id >= 0 && has_s) {
        int sn = ggb_add_point_node(graph, sx, sy);
        if (sn < 0)
            return 0;
        graph_add_line_segment(graph, sn, e_id);
        return 1;
    }
    if (has_s && has_e) {
        int sn = ggb_add_point_node(graph, sx, sy);
        int en = ggb_add_point_node(graph, ex, ey);
        if (sn < 0 || en < 0)
            return 0;
        graph_add_line_segment(graph, sn, en);
        return 1;
    }
    return 0;
}

/**
 * @brief 导入 circle 元素（圆心 + 半径 → 32 采样点闭合）
 *
 * 圆心优先取 center 引用节点坐标，其次取内联坐标；
 * 半径从 <equation> 提取。采样点按 svg_parse_circle 思路离散。
 *
 * @return 导入的点数（0 表示跳过），-1 表示内部错误
 */
static int ggb_import_circle(ConstraintGraph *graph, const char *xml, const GgbElementEntry *e,
                             const GgbElementEntry *entries, const int *node_by_idx, int el_count) {
    double cx = 0, cy = 0;
    bool has_c = false;
    int c_id = ggb_resolve_point_ref(xml, e, "center", entries, node_by_idx, el_count, &cx, &cy, &has_c);

    double radius = 0;
    if (!ggb_extract_equation_radius(xml, e, &radius) || radius <= 0.0)
        return 0;

    if (c_id >= 0) {
        GeomNode *node = graph_get_node(graph, c_id);
        if (!node || node->coord_count < 2 || !node->symbolic_coords)
            return 0;
        cx = symbolic_coord_to_double(node->symbolic_coords[0]);
        cy = symbolic_coord_to_double(node->symbolic_coords[1]);
    } else if (has_c) {
        int nid = ggb_add_point_node(graph, cx, cy);
        if (nid < 0)
            return 0;
    } else {
        return 0;
    }

    double pts[64];
    int n = svg_parse_circle(cx, cy, radius, pts, (int) lv_ARRAY_SIZE(pts));
    return ggb_import_point_sequence(graph, pts, n, true);
}

/**
 * @brief 导入 polygon 元素（<points> 内 <point> 引用列表 → 闭合折线）
 *
 * 支持两种形式：子标签 <point> 带 P/label 引用（优先），
 * 或带内联 x/y 坐标。
 *
 * @return 导入的点数（0 表示跳过），-1 表示内部错误
 */
static int ggb_import_polygon(ConstraintGraph *graph, const char *xml, const GgbElementEntry *e,
                              const GgbElementEntry *entries, const int *node_by_idx, int el_count) {
    if (e->self_closing)
        return 0;
    size_t points_tag_start = 0, points_tag_end = 0;
    if (!ggb_find_child_tag(xml, e->content_start, e->content_end, "points", &points_tag_start, &points_tag_end))
        return 0;
    if (points_tag_end + 1 >= e->content_end)
        return 0;

    int node_ids[512];
    int count = 0;
    size_t pos = points_tag_end + 1;
    while (count < (int) lv_ARRAY_SIZE(node_ids) && pos + 2 <= e->content_end) {
        size_t ps = 0, pe = 0;
        if (!ggb_find_child_tag(xml, pos, e->content_end, "point", &ps, &pe))
            break;
        if (pe >= e->content_end)
            break;
        size_t plen = pe - ps;
        char buf[128];
        int node_id = -1;
        if (ggb_extract_attr(xml + ps, plen, "P", buf, sizeof(buf))) {
            int idx = atoi(buf);
            if (idx >= 0 && idx < el_count && node_by_idx[idx] >= 0)
                node_id = node_by_idx[idx];
        }
        if (node_id < 0 && ggb_extract_attr(xml + ps, plen, "label", buf, sizeof(buf))) {
            for (int i = 0; i < el_count; i++) {
                if (node_by_idx[i] >= 0 && entries[i].label[0] != '\0' &&
                    lv_str_eq(entries[i].label, buf)) {
                    node_id = node_by_idx[i];
                    break;
                }
            }
        }
        if (node_id < 0) {
            char xb[64], yb[64];
            double x = 0, y = 0;
            if (ggb_extract_attr(xml + ps, plen, "x", xb, sizeof(xb)) &&
                ggb_extract_attr(xml + ps, plen, "y", yb, sizeof(yb)) &&
                lv_parse_double(xb, &x) == 0 && lv_parse_double(yb, &y) == 0) {
                node_id = ggb_add_point_node(graph, x, y);
                if (node_id < 0)
                    break;
            }
        }
        if (node_id >= 0) {
            if (count > 0)
                graph_add_line_segment(graph, node_ids[count - 1], node_id);
            node_ids[count++] = node_id;
        }
        pos = pe + 1;
    }
    if (count > 1)
        graph_add_line_segment(graph, node_ids[count - 1], node_ids[0]);
    return count;
}

/**
 * @brief 按 element 类型分发导入
 *
 * @return 导入的实体数（0 表示跳过），-1 表示内部错误
 */
static int ggb_import_shaped_element(ConstraintGraph *graph, const char *xml, const GgbElementEntry *e,
                                     const GgbElementEntry *entries, const int *node_by_idx,
                                     int el_count) {
    if (lv_str_eq(e->type, "segment") || lv_str_eq(e->type, "line"))
        return ggb_import_segment_or_line(graph, xml, e, entries, node_by_idx, el_count);
    if (lv_str_eq(e->type, "circle"))
        return ggb_import_circle(graph, xml, e, entries, node_by_idx, el_count);
    if (lv_str_eq(e->type, "polygon"))
        return ggb_import_polygon(graph, xml, e, entries, node_by_idx, el_count);
    return 0;
}

/**
 * @brief 解析 XML 中全部 <element> 标签到数组
 *
 * @param xml         XML 文本
 * @param xml_len     XML 文本长度
 * @param entries     输出数组
 * @param max_entries 数组容量
 * @return 解析到的元素数量
 */
static int ggb_parse_elements(const char *xml, size_t xml_len, GgbElementEntry *entries, int max_entries) {
    int n = 0;
    size_t pos = 0;
    while (n < max_entries) {
        size_t os = 0, oe = 0, cs = 0;
        if (!ggb_find_element_open(xml, xml_len, pos, &os, &oe, &cs))
            break;
        size_t tag_len = oe - os;
        GgbElementEntry *e = &entries[n];
        memset(e, 0, sizeof(*e));
        ggb_extract_attr(xml + os, tag_len, "type", e->type, sizeof(e->type));
        ggb_extract_attr(xml + os, tag_len, "label", e->label, sizeof(e->label));
        e->open_start = os;
        e->open_end = oe;
        e->content_start = cs;
        e->self_closing = (oe > os && xml[oe - 1] == '/');
        e->content_end = cs;
        if (!e->self_closing) {
            size_t ce = ggb_find_close_tag(xml, xml_len, cs, "element");
            if (ce == (size_t) -1)
                break;
            e->content_end = ce;
        }
        n++;
        pos = e->self_closing ? oe + 1 : e->content_end;
    }
    return n;
}

/**
 * @brief 将解析后的 geogebra.xml 导入约束图
 *
 * 两遍处理：
 *   第一遍：导入全部 point 元素，按 construction 索引记录节点 ID；
 *   第二遍：segment/line/circle/polygon 通过 P/label 引用或内联坐标导入。
 *
 * @param engine  引擎（engine->main_graph 为目标图）
 * @param xml     geogebra.xml 文本（NUL 结尾）
 * @param xml_len XML 文本长度
 * @return 成功导入的实体数
 */
static int ggb_import_xml(lvEngine *engine, const char *xml, size_t xml_len) {
    ConstraintGraph *graph = engine->main_graph;
    GgbElementEntry *entries = (GgbElementEntry *) lv_malloc(sizeof(GgbElementEntry) * (size_t) GGB_MAX_ELEMENTS);
    if (!entries)
        return 0;
    int *node_by_idx = (int *) lv_malloc(sizeof(int) * (size_t) GGB_MAX_ELEMENTS);
    if (!node_by_idx) {
        lv_free((void **) &entries);
        return 0;
    }

    int el_count = ggb_parse_elements(xml, xml_len, entries, GGB_MAX_ELEMENTS);
    for (int i = 0; i < el_count; i++)
        node_by_idx[i] = -1;

    int imported = 0;

    /* 第一遍：point */
    for (int i = 0; i < el_count; i++) {
        if (lv_str_ne(entries[i].type, "point"))
            continue;
        double px = 0, py = 0;
        if (!ggb_element_point_coord(xml, &entries[i], &px, &py))
            continue;
        int node_id = ggb_add_point_node(graph, px, py);
        if (node_id >= 0) {
            node_by_idx[i] = node_id;
            imported++;
        }
    }

    /* 第二遍：segment/line/circle/polygon */
    for (int i = 0; i < el_count; i++) {
        if (lv_str_eq(entries[i].type, "point"))
            continue;
        int added = ggb_import_shaped_element(graph, xml, &entries[i], entries, node_by_idx, el_count);
        if (added < 0)
            break;
        imported += added;
    }

    lv_free((void **) &node_by_idx);
    lv_free((void **) &entries);
    return imported;
}

int interop_import_geogebra(lvEngine *engine, const InteropImportConfig *config) {
    /**
     * @brief 从 GeoGebra .ggb 文件导入几何构造
     *
     * 处理流程：
     *   1. 读取整个 .ggb 文件到内存
     *   2. 解析 ZIP 的 EOCD 和 Central Directory 结构
     *   3. 查找 "geogebra.xml" 文件条目
     *   4. 解压（STORE 或 Deflate）获取 XML 内容
     *   5. 手工 XML 解析，提取 <element> 标签
     *   6. 按 type 属性（point/segment/circle/line/polygon）映射到约束图
     */
    if (!engine || !config)
        return lv_ERROR_INVALID_PARAM;
    printf("[GGB-DBG-ENTRY] entered interop_import_geogebra\n");
    if (config->input_path[0] == '\0')
        return lv_ERROR_INVALID_PARAM;
    if (!engine->main_graph) {
        lv_RETURN_ERROR_VAL(lv_ERROR_INVALID_STATE, lv_ERROR_INVALID_STATE, "GeoGebra导入失败：引擎的约束图未初始化");
    }

    size_t fsize = 0;
    uint8_t *data = lv_file_read_all(config->input_path, &fsize);
    if (!data) {
        lv_RETURN_ERROR_VAL(lv_ERROR_IO, lv_ERROR_IO,
                            "GeoGebra导入失败：无法读取文件'%s'（不存在、为空或读取失败）", config->input_path);
    }

    size_t eocd = 0;
    if (!ggb_find_eocd(data, fsize, &eocd)) {
        lv_free((void **) &data);
        lv_RETURN_ERROR_VAL(lv_ERROR_PARSE, lv_ERROR_PARSE,
                            "GeoGebra导入失败：未找到ZIP的EOCD记录，文件可能不是有效的.ggb档案");
    }

    size_t local_off = 0, comp_size = 0, uncomp_size = 0;
    uint16_t comp_method = 0;
    if (!ggb_central_find_entry(data, fsize, eocd, "geogebra.xml", &local_off, &comp_size, &uncomp_size,
                                &comp_method)) {
        lv_free((void **) &data);
        lv_RETURN_ERROR_VAL(lv_ERROR_PARSE, lv_ERROR_PARSE,
                            "GeoGebra导入失败：ZIP中央目录中未找到geogebra.xml条目");
    }

    size_t data_off = 0;
    if (!ggb_local_data_offset(data, fsize, local_off, &data_off)) {
        lv_free((void **) &data);
        lv_RETURN_ERROR_VAL(lv_ERROR_PARSE, lv_ERROR_PARSE,
                            "GeoGebra导入失败：geogebra.xml本地文件头无效");
    }

    uint8_t *xml = NULL;
    size_t xml_len = 0;
    if (!ggb_extract_entry(data, fsize, data_off, comp_size, uncomp_size, comp_method, &xml, &xml_len)) {
        lv_free((void **) &data);
        lv_RETURN_ERROR_VAL(lv_ERROR_PARSE, lv_ERROR_PARSE,
                            "GeoGebra导入失败：geogebra.xml解压失败（压缩方法=%u）", (unsigned) comp_method);
    }
    lv_free((void **) &data);

    /* 确保解压结果以 NUL 结尾，便于字符串 API 使用 */
    if (xml_len == 0 || xml[xml_len - 1] != '\0') {
        uint8_t *tmp = (uint8_t *) lv_malloc(xml_len + 1);
        if (!tmp) {
            lv_free((void **) &xml);
            return 0;
        }
        lv_strlcpy_n((char *) tmp, xml_len + 1, (const char *) xml, (size_t) xml_len);
        lv_free((void **) &xml);
        xml = tmp;
    }

    {
        FILE *df = fopen("build3/_verify_import/ggb_dump.xml", "wb");
        printf("[GGB-DBG] xml_len=%zu comp=%zu uncomp=%zu method=%u fopen=%p\n",
               xml_len, comp_size, uncomp_size, (unsigned) comp_method, (void *) df);
        if (df) {
            fwrite(xml, 1, xml_len, df);
            fclose(df);
        }
    }
    int imported = ggb_import_xml(engine, (const char *) xml, xml_len);
    lv_free((void **) &xml);

    if (imported == 0) {
        lv_set_error(lv_ERROR_PARSE,
                     "GeoGebra导入完成但未找到任何可导入的几何元素（支持：point/segment/circle/line/polygon）");
    }
    return imported;
}

/* ── SVG 解析器 ── */

/* ── GeoJSON 解析辅助（基于统一 lvJsonParser，替代原手写 GJ_* 宏） ── */

#define GJ_MAX_FEATURES 4096
#define GJ_MAX_COORDS 8192

/* 解析 [[x,y],[x,y],...] 点列表，输出到 xs/ys（各至多 max 个），返回点数 */
static int gj_parse_coord_list(lvJsonParser *p, double *xs, double *ys, int max) {
    if (lv_json_peek(p) != '[')
        return 0;
    lv_json_next(p); /* 跳过 '[' */
    int n = 0;
    for (;;) {
        char c = lv_json_peek(p);
        if (c == ']') {
            lv_json_next(p);
            break;
        }
        if (c == ',') {
            lv_json_next(p);
            continue;
        }
        if (n >= max)
            break;
        double pair[2];
        size_t cnt = 0;
        if (!lv_json_parse_double_array(p, pair, 2, &cnt))
            break;
        if (cnt >= 2) {
            xs[n] = pair[0];
            ys[n] = pair[1];
        }
        n++;
        c = lv_json_peek(p);
        if (c == ',') {
            lv_json_next(p);
            continue;
        }
        if (c == ']') {
            lv_json_next(p);
            break;
        }
        break; /* 意外 token，停止 */
    }
    return n;
}

/* 解析 geometry 对象（p 位于 '{' 处）并把坐标导入约束图 */
static void gj_import_geometry(lvJsonParser *p, ConstraintGraph *graph, int *imported_count, int *prev_node_id,
                               double *coords_x, double *coords_y) {
    if (lv_json_peek(p) != '{')
        return;
    lv_json_next(p); /* 跳过 '{' */

    bool is_point = false, is_multipoint = false;
    bool is_linestring = false, is_multilinestring = false;
    bool is_polygon = false;
    const char *coords_val = NULL;

    /* 遍历 geometry 对象字段（键序无关：先收集 type，再记录 coordinates 值位置） */
    char *key = NULL;
    while (lv_json_parse_field(p, &key)) {
        if (lv_str_eq(key, "type") && lv_json_peek(p) == '"') {
            char *t = lv_json_parse_string(p);
            if (t) {
                if (lv_str_eq(t, "Point"))
                    is_point = true;
                else if (lv_str_eq(t, "MultiPoint"))
                    is_multipoint = true;
                else if (lv_str_eq(t, "LineString"))
                    is_linestring = true;
                else if (lv_str_eq(t, "MultiLineString"))
                    is_multilinestring = true;
                else if (lv_str_eq(t, "Polygon"))
                    is_polygon = true;
                lv_free((void **) &t);
            }
        } else if (lv_str_eq(key, "coordinates")) {
            coords_val = p->data + p->pos; /* 记录值起始位置 */
            lv_json_skip_value(p);
        } else {
            lv_json_skip_value(p);
        }
        lv_free((void **) &key);
    }
    if (lv_json_peek(p) == '}')
        lv_json_next(p);

    /* 类型未知或缺少 coordinates：无导入 */
    if (!is_point && !is_multipoint && !is_linestring && !is_multilinestring && !is_polygon)
        return;
    if (!coords_val || *coords_val != '[')
        return;

    /* 解析坐标数组 */
    int coord_count = 0;
    lvJsonParser cp;
    lv_json_parser_init(&cp, coords_val, strlen(coords_val));

    if (is_point) {
        /* Point: [x, y(, z)] — 与原实现一致仅取前两个元素 */
        double pair[4];
        size_t cnt = 0;
        if (lv_json_parse_double_array(&cp, pair, 4, &cnt) && cnt >= 2) {
            coords_x[0] = pair[0];
            coords_y[0] = pair[1];
            coord_count = 1;
        }
    } else if (is_multipoint || is_linestring) {
        /* MultiPoint / LineString: [[x,y],...] */
        coord_count = gj_parse_coord_list(&cp, coords_x, coords_y, GJ_MAX_COORDS);
    } else if (is_multilinestring) {
        /* MultiLineString: [[[x,y],...], ...] — 展平所有线段 */
        if (lv_json_peek(&cp) == '[') {
            lv_json_next(&cp);
            int n = 0;
            for (;;) {
                if (lv_json_peek(&cp) == ']') {
                    lv_json_next(&cp);
                    break;
                }
                if (lv_json_peek(&cp) == ',') {
                    lv_json_next(&cp);
                    continue;
                }
                int m = gj_parse_coord_list(&cp, coords_x + n, coords_y + n, GJ_MAX_COORDS - n);
                n += m;
                if (n >= GJ_MAX_COORDS)
                    break;
                if (lv_json_peek(&cp) == ',') {
                    lv_json_next(&cp);
                    continue;
                }
                break;
            }
            coord_count = n;
        }
    } else if (is_polygon) {
        /* Polygon: [[[x,y],...], [内环...]] — 只处理外环 */
        if (lv_json_peek(&cp) == '[') {
            lv_json_next(&cp);
            coord_count = gj_parse_coord_list(&cp, coords_x, coords_y, GJ_MAX_COORDS);
            lv_json_skip_value(&cp); /* 跳过剩余内环 */
        }
    }

    /* --- 将坐标导入到约束图（与原实现逻辑一致） --- */
    if (coord_count > 0) {
        int first_node_id = -1;
        *prev_node_id = -1;

        for (int i = 0; i < coord_count; i++) {
            /* 将 double 坐标转为有理数 SymbolicCoord */
            int64_t xn = (int64_t) (coords_x[i] * (double) INTEROP_COORD_DENOM_PRECISION_GEOJSON + (coords_x[i] >= 0 ? 0.5 : -0.5));
            int64_t yn = (int64_t) (coords_y[i] * (double) INTEROP_COORD_DENOM_PRECISION_GEOJSON + (coords_y[i] >= 0 ? 0.5 : -0.5));
            SymbolicCoord *cx = symbolic_coord_create_rational(xn, INTEROP_COORD_DENOM_PRECISION_GEOJSON);
            SymbolicCoord *cy = symbolic_coord_create_rational(yn, INTEROP_COORD_DENOM_PRECISION_GEOJSON);
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

            if (*prev_node_id >= 0 && (is_linestring || is_multilinestring || is_polygon)) {
                graph_add_line_segment(graph, *prev_node_id, node_id);
            }

            *prev_node_id = node_id;
            (*imported_count)++;
        }

        /* 闭合多边形 */
        if (is_polygon && first_node_id >= 0 && *prev_node_id >= 0 && first_node_id != *prev_node_id) {
            graph_add_line_segment(graph, *prev_node_id, first_node_id);
        }
    }
}

int interop_import_geojson(lvEngine *engine, const InteropImportConfig *config) {
    if (!engine || !config)
        return lv_ERROR_INVALID_PARAM;
    if (!engine->main_graph) {
        lv_RETURN_ERROR_VAL(lv_ERROR_INVALID_STATE, lv_ERROR_INVALID_STATE, "GeoJSON导入失败：引擎的约束图未初始化");
    }
    if (config->input_path[0] == '\0') {
        lv_RETURN_ERROR_VAL(lv_ERROR_INVALID_PARAM, lv_ERROR_INVALID_PARAM, "GeoJSON导入失败：未指定输入文件路径");
    }

    /* --- 读取文件（lv_file_read_all：失败/空文件返回 NULL，成功时缓冲以 NUL 结尾） --- */
    size_t fsize = 0;
    char *json = (char *) lv_file_read_all(config->input_path, &fsize);
    if (!json) {
        lv_RETURN_ERROR_VAL(lv_ERROR_IO, lv_ERROR_IO, "GeoJSON导入失败：无法读取文件'%s'（不存在、为空或读取失败）", config->input_path);
    }

    /* 统一 JSON 解析器（lvJsonParser，替代原手写 GJ_* 宏） */
    lvJsonParser p;
    lv_json_parser_init(&p, json, strlen(json));

    int imported_count = 0;
    ConstraintGraph *graph = engine->main_graph;

    /* --- 解析顶层 FeatureCollection 或 Feature --- */
    if (lv_json_peek(&p) != '{') {
        lv_free((void **) &json);
        lv_RETURN_ERROR_VAL(lv_ERROR_PARSE, lv_ERROR_PARSE, "GeoJSON导入失败：根元素不是JSON对象");
    }

    /* 查找 "type" 字段来识别根类型 */
    const char *type_val = lv_json_find_key(json, "type", 4);
    if (!type_val) {
        lv_free((void **) &json);
        lv_RETURN_ERROR_VAL(lv_ERROR_PARSE, lv_ERROR_PARSE, "GeoJSON导入失败：缺少type字段");
    }

    bool is_feature_collection = false;
    if (*type_val == '"') {
        lvJsonParser tp;
        lv_json_parser_init(&tp, type_val, strlen(type_val));
        char *tstr = lv_json_parse_string(&tp);
        if (tstr) {
            /* 与原实现一致的宽松前缀比较（strncmp 17 字符，不要求结尾引号） */
            if (lv_str_startswith(tstr, "FeatureCollection"))
                is_feature_collection = true;
            lv_free((void **) &tstr);
        }
    }

    /* 定位 "features" 数组 */
    if (is_feature_collection) {
        const char *features_val = lv_json_find_key(json, "features", 8);
        if (!features_val) {
            lv_free((void **) &json);
            lv_RETURN_ERROR_VAL(lv_ERROR_PARSE, lv_ERROR_PARSE, "GeoJSON导入失败：FeatureCollection缺少features数组");
        }
        if (*features_val != '[') {
            lv_free((void **) &json);
            lv_RETURN_ERROR_VAL(lv_ERROR_PARSE, lv_ERROR_PARSE, "GeoJSON导入失败：features不是数组");
        }
        lv_json_parser_init(&p, features_val, strlen(features_val));
        lv_json_next(&p); /* 跳过 '['，进入 features 数组 */
    }

    double coords_x[GJ_MAX_COORDS];
    double coords_y[GJ_MAX_COORDS];
    int prev_node_id = -1;

    while (imported_count < GJ_MAX_FEATURES) {
        char c = lv_json_peek(&p);
        if (c == ']' || c == '\0')
            break;
        if (c == ',') {
            lv_json_next(&p);
            continue;
        }
        if (c != '{')
            break;

        /* 进入一个 feature 对象 */
        lv_json_next(&p);

        /* 遍历 feature 对象字段，处理 geometry 子对象 */
        char *key = NULL;
        while (lv_json_parse_field(&p, &key)) {
            if (lv_str_eq(key, "geometry")) {
                gj_import_geometry(&p, graph, &imported_count, &prev_node_id, coords_x, coords_y);
            } else {
                lv_json_skip_value(&p);
            }
            lv_free((void **) &key);
        }
        if (lv_json_peek(&p) == '}')
            lv_json_next(&p);
    }

    lv_free((void **) &json);

    if (imported_count == 0) {
        lv_set_error(lv_ERROR_PARSE,
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
#define SVG_SKIP_WS(s)                                                                     \
    do {                                                                                   \
        while (*(s) == ' ' || *(s) == '\t' || *(s) == '\n' || *(s) == '\r' || *(s) == ',') \
            (s)++;                                                                         \
    } while (0)

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
/** @brief SVG path 命令处理器类型 */
typedef bool (*SvgPathHandler)(const char **s, SvgParserState *state, double *out_points,
                               int max_points, int *out_count, bool is_relative);

/** @brief 贝塞尔/圆弧采样点数 */
#define SVG_PATH_SAMPLES 10

/** @brief moveto：移动到绝对位置（M/m） */
static bool svg_path_moveto(const char **s, SvgParserState *state, double *out_points,
                            int max_points, int *out_count, bool is_relative) {
    double abs_x, abs_y;
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

/** @brief lineto：直线段（L/l） */
static bool svg_path_lineto(const char **s, SvgParserState *state, double *out_points,
                            int max_points, int *out_count, bool is_relative) {
    double abs_x, abs_y;
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

/** @brief cubic Bezier: C x1,y1 x2,y2 x,y（C/c） */
static bool svg_path_cubic_bezier(const char **s, SvgParserState *state, double *out_points,
                                  int max_points, int *out_count, bool is_relative) {
    double x1, y1, x2, y2, abs_x, abs_y;
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
    for (int i = 1; i <= SVG_PATH_SAMPLES && *out_count < max_points; i++) {
        double t = (double) i / (double) SVG_PATH_SAMPLES;
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

/** @brief quadratic Bezier: Q x1,y1 x,y（Q/q） */
static bool svg_path_quadratic_bezier(const char **s, SvgParserState *state, double *out_points,
                                      int max_points, int *out_count, bool is_relative) {
    double x1, y1, abs_x, abs_y;
    if (!svg_parse_coord(s, &x1, &y1) || !svg_parse_coord(s, &abs_x, &abs_y))
        return false;
    if (is_relative) {
        x1 += state->cx;
        y1 += state->cy;
        abs_x += state->cx;
        abs_y += state->cy;
    }
    double qx0 = state->cx, qy0 = state->cy;
    for (int i = 1; i <= SVG_PATH_SAMPLES && *out_count < max_points; i++) {
        double t = (double) i / (double) SVG_PATH_SAMPLES;
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

/** @brief arc: A rx,ry x-axis-rotation large-arc-flag sweep-flag x,y（A/a） */
static bool svg_path_arc(const char **s, SvgParserState *state, double *out_points,
                         int max_points, int *out_count, bool is_relative) {
    double rx, ry, rot, dx, dy;
    double laf_d, sf_d;
    if (!svg_parse_double(s, &rx) || !svg_parse_double(s, &ry) || !svg_parse_double(s, &rot) ||
        !svg_parse_double(s, &laf_d) || !svg_parse_double(s, &sf_d) || !svg_parse_coord(s, &dx, &dy))
        return false;
    lv_UNUSED(ry);
    lv_UNUSED(rot);
    lv_UNUSED(laf_d); /* parsed for future SVG arc implementation */
    int sf = (int) round(sf_d);
    if (is_relative) {
        dx += state->cx;
        dy += state->cy;
    }

    /* 使用中点公式计算椭圆弧采样 */
    double x_start = state->cx, y_start = state->cy;

    /* 简化参数方程：沿椭圆弧采样 */
    for (int i = 1; i <= SVG_PATH_SAMPLES && *out_count < max_points; i++) {
        double t = (double) i / (double) SVG_PATH_SAMPLES;
        /* 线性插值 + 圆弧偏移近似 */
        double lx = lv_lerp(x_start, dx, t);
        double ly = lv_lerp(y_start, dy, t);
        /* 添加圆弧离差 */
        double arc_angle = t * M_PI;
        double bulge = sin(arc_angle) * (sf ? 1.0 : -1.0);
        double chord_len = geo_distance_2d(x_start, y_start, dx, dy);
        double bulge_factor = (chord_len > lv_GEO_LENGTH_GUARD) ? (rx / chord_len) * 0.5 : 0.0;
        double nx = -(dy - y_start) / (chord_len > lv_GEO_LENGTH_GUARD ? chord_len : 1.0);
        double ny = (dx - x_start) / (chord_len > lv_GEO_LENGTH_GUARD ? chord_len : 1.0);
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

/** @brief closepath：画线回到当前子路径起点（Z/z） */
static bool svg_path_closepath(const char **s, SvgParserState *state, double *out_points,
                               int max_points, int *out_count, bool is_relative) {
    (void) s;
    (void) is_relative;
    if (*out_count < max_points) {
        out_points[(*out_count) * 2] = state->start_x;
        out_points[(*out_count) * 2 + 1] = state->start_y;
        (*out_count)++;
    }
    state->cx = state->start_x;
    state->cy = state->start_y;
    return true;
}

/** @brief SVG path 命令字符→处理器 查找表（替代 12 分支 switch；大小写映射到同组处理器） */
static const struct {
    char cmd;              /**< 命令字符 */
    SvgPathHandler handler; /**< 处理器 */
} kSvgPathHandlers[] = {
    {'M', svg_path_moveto},
    {'m', svg_path_moveto},
    {'L', svg_path_lineto},
    {'l', svg_path_lineto},
    {'C', svg_path_cubic_bezier},
    {'c', svg_path_cubic_bezier},
    {'Q', svg_path_quadratic_bezier},
    {'q', svg_path_quadratic_bezier},
    {'A', svg_path_arc},
    {'a', svg_path_arc},
    {'Z', svg_path_closepath},
    {'z', svg_path_closepath},
};

static bool svg_parse_path_command(char cmd_char, const char **s, SvgParserState *state, double *out_points,
                                   int max_points, int *out_count, bool is_relative) {
    *out_count = 0;

    /* 命令查表分发（替代 12 分支 switch；未命中返回 false，与 default 分支一致） */
    for (size_t i = 0; i < lv_ARRAY_SIZE(kSvgPathHandlers); i++) {
        if (kSvgPathHandlers[i].cmd == cmd_char)
            return kSvgPathHandlers[i].handler(s, state, out_points, max_points, out_count, is_relative);
    }
    return false;
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

/* ==================== SVG 导入辅助 ==================== */

/** @brief 单条 path 最多采集的采样点上限（坐标对数） */
#ifndef SVG_PATH_MAX_POINTS
#define SVG_PATH_MAX_POINTS 8192
#endif

/**
 * @brief 将采样点序列导入约束图（相邻点连线段，可闭合）
 *
 * 坐标先减去 viewBox 原点 (ox, oy)（平移映射到局部坐标系）。
 * 每点按 ggb_double_to_rational 转为 1e6 精度有理数 SymbolicCoord。
 *
 * @param graph      约束图
 * @param pts        采样点数组 [x0,y0,x1,y1,...]
 * @param n          采样点数量（坐标对数）
 * @param close_loop 是否闭合（末点连回首点）
 * @param ox         viewBox 原点 x（无 viewBox 时传 0）
 * @param oy         viewBox 原点 y（无 viewBox 时传 0）
 * @return 实际导入的点数
 */
static int svg_import_samples(ConstraintGraph *graph, const double *pts, int n, bool close_loop,
                              double ox, double oy) {
    int imported = 0;
    int first_id = -1, prev_id = -1;
    for (int i = 0; i < n; i++) {
        SymbolicCoord *cx = ggb_double_to_rational(pts[i * 2] - ox);
        SymbolicCoord *cy = ggb_double_to_rational(pts[i * 2 + 1] - oy);
        if (!cx || !cy) {
            if (cx)
                symbolic_coord_destroy(cx);
            if (cy)
                symbolic_coord_destroy(cy);
            continue;
        }
        SymbolicCoord *coords[2] = {cx, cy};
        AddNodeResult res = graph_add_point(graph, coords, 2);
        if (res != ADD_NODE_OK) {
            symbolic_coord_destroy(cx);
            symbolic_coord_destroy(cy);
            continue;
        }
        int node_id = (int) (graph->next_node_id - 1);
        if (first_id < 0)
            first_id = node_id;
        if (prev_id >= 0)
            graph_add_line_segment(graph, prev_id, node_id);
        prev_id = node_id;
        imported++;
    }
    if (close_loop && first_id >= 0 && prev_id >= 0 && first_id != prev_id)
        graph_add_line_segment(graph, prev_id, first_id);
    return imported;
}

/**
 * @brief 提取 SVG 标签名（'<' 与空白/'/'/'>' 之间）
 *
 * @param tag      标签起始（'<' 之后）
 * @param tag_len  标签长度
 * @param out      输出缓冲区
 * @param out_size 输出缓冲区大小
 */
static void svg_tag_name(const char *tag, size_t tag_len, char *out, size_t out_size) {
    size_t n = 0;
    while (n < out_size - 1 && n < tag_len) {
        char c = tag[n];
        if (c == ' ' || c == '>' || c == '/' || c == '\t' || c == '\n' || c == '\r')
            break;
        out[n] = c;
        n++;
    }
    out[n] = '\0';
}

/**
 * @brief 解析 points 属性（"x1,y1 x2,y2 ..."）到采样点数组
 *
 * @param text points 属性值（NUL 结尾）
 * @param pts  输出采样点数组 [x0,y0,x1,y1,...]
 * @param max  数组容量（坐标对数）
 * @return 解析到的坐标对数
 */
static int svg_parse_points_attr(const char *text, double *pts, int max) {
    const char *s = text;
    int n = 0;
    while (n < max) {
        double x, y;
        if (!svg_parse_coord(&s, &x, &y))
            break;
        pts[n * 2] = x;
        pts[n * 2 + 1] = y;
        n++;
        if (*s == '\0')
            break;
    }
    return n;
}

/**
 * @brief 解析 SVG <path d="..."> 并导入采样点（相邻点连线段）
 *
 * 支持 M/L/C/Q/A/Z（含相对小写），贝塞尔/圆弧由文件既有的
 * svg_parse_path_command 处理器采样。所有采样点顺序导入。
 *
 * @param graph  约束图
 * @param d      d 属性值（NUL 结尾）
 * @param count  [in/out] 累计导入点数
 * @param ox     viewBox 原点 x
 * @param oy     viewBox 原点 y
 */
static void svg_import_path(ConstraintGraph *graph, const char *d, int *count, double ox, double oy) {
    SvgParserState state;
    memset(&state, 0, sizeof(state));
    double *pts = (double *) lv_malloc(sizeof(double) * 2 * (size_t) SVG_PATH_MAX_POINTS);
    if (!pts)
        return;

    int total = 0;
    const char *s = d;
    while (*s) {
        SVG_SKIP_WS(s);
        char cmd = *s;
        if (cmd == '\0')
            break;
        bool is_relative = (cmd >= 'a' && cmd <= 'z');
        char upper = is_relative ? (char) (cmd - ('a' - 'A')) : cmd;
        if (strchr("MLCQAZ", upper)) {
            s++;
            /* 同一命令可能携带多组参数（如 "L 1,1 2,2 3,3"） */
            for (;;) {
                double out_points[SVG_PATH_SAMPLES * 2 + 16];
                int cnt = 0;
                const char *before = s;
                if (!svg_parse_path_command(cmd, &s, &state, out_points,
                                            (int) lv_ARRAY_SIZE(out_points), &cnt, is_relative))
                    break;
                /* 无参数命令（Z 闭合）不消费输入，处理一次即退出，防止死循环 */
                if (s == before)
                    break;
                for (int i = 0; i < cnt && total < SVG_PATH_MAX_POINTS; i++) {
                    pts[total * 2] = out_points[i * 2];
                    pts[total * 2 + 1] = out_points[i * 2 + 1];
                    total++;
                }
                SVG_SKIP_WS(s);
                if (*s == '\0' || *s == ',' || (*s >= '0' && *s <= '9') || *s == '-' || *s == '+' || *s == '.')
                    continue;
                break;
            }
        } else {
            s++;
        }
    }

    if (total > 0)
        *count += svg_import_samples(graph, pts, total, false, ox, oy);
    lv_free((void **) &pts);
}

int interop_import_svg(lvEngine *engine, const InteropImportConfig *config) {
    if (!engine || !config)
        return lv_ERROR_INVALID_PARAM;
    if (config->input_path[0] == '\0')
        return lv_ERROR_INVALID_PARAM;
    if (!engine->main_graph) {
        lv_RETURN_ERROR_VAL(lv_ERROR_INVALID_STATE, lv_ERROR_INVALID_STATE, "SVG导入失败：引擎的约束图未初始化");
    }

    size_t fsize = 0;
    char *svg = (char *) lv_file_read_all(config->input_path, &fsize);
    if (!svg) {
        lv_RETURN_ERROR_VAL(lv_ERROR_IO, lv_ERROR_IO,
                            "SVG导入失败：无法读取文件'%s'（不存在、为空或读取失败）", config->input_path);
    }
    size_t len = strlen(svg);

    ConstraintGraph *graph = engine->main_graph;
    int imported = 0;
    double ox = 0, oy = 0;
    bool has_viewbox = false;

    size_t pos = 0;
    while (pos < len) {
        const char *lt = memchr(svg + pos, '<', len - pos);
        if (!lt)
            break;
        size_t lt_off = (size_t) (lt - svg);
        const char *gt = memchr(lt, '>', len - lt_off);
        if (!gt)
            break;
        size_t gt_off = (size_t) (gt - svg);
        const char *tag = lt + 1;
        size_t tag_len = gt_off - lt_off;

        bool is_close = (tag_len > 0 && tag[0] == '/');
        bool is_decl = (tag_len > 0 && (tag[0] == '?' || tag[0] == '!'));
        if (!is_close && !is_decl) {
            char name[64];
            svg_tag_name(tag, tag_len, name, sizeof(name));

            if (lv_str_eq(name, "svg")) {
                char vb[128];
                if (ggb_extract_attr(svg + lt_off, tag_len, "viewBox", vb, sizeof(vb))) {
                    double w = 0, h = 0;
                    if (sscanf(vb, "%lf %lf %lf %lf", &ox, &oy, &w, &h) == 4)
                        has_viewbox = true;
                }
            } else if (lv_str_eq(name, "path")) {
                const char *d = NULL;
                size_t d_len = 0;
                if (ggb_extract_attr_len(svg + lt_off, tag_len, "d", &d, &d_len)) {
                    char *dbuf = (char *) lv_malloc(d_len + 1);
                    if (dbuf) {
                        lv_strlcpy_n(dbuf, d_len + 1, d, (size_t) d_len);
                        svg_import_path(graph, dbuf, &imported, has_viewbox ? ox : 0, has_viewbox ? oy : 0);
                        lv_free((void **) &dbuf);
                    }
                }
            } else if (lv_str_eq(name, "circle")) {
                char buf[64];
                double cx = 0, cy = 0, r = 0;
                if (ggb_extract_attr(svg + lt_off, tag_len, "cx", buf, sizeof(buf)) && lv_parse_double(buf, &cx) == 0 &&
                    ggb_extract_attr(svg + lt_off, tag_len, "cy", buf, sizeof(buf)) && lv_parse_double(buf, &cy) == 0 &&
                    ggb_extract_attr(svg + lt_off, tag_len, "r", buf, sizeof(buf)) && lv_parse_double(buf, &r) == 0 &&
                    r > 0.0) {
                    double pts[64];
                    int n = svg_parse_circle(cx, cy, r, pts, (int) lv_ARRAY_SIZE(pts));
                    imported += svg_import_samples(graph, pts, n, true, has_viewbox ? ox : 0, has_viewbox ? oy : 0);
                }
            } else if (lv_str_eq(name, "line")) {
                char buf[64];
                double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
                if (ggb_extract_attr(svg + lt_off, tag_len, "x1", buf, sizeof(buf)) && lv_parse_double(buf, &x1) == 0 &&
                    ggb_extract_attr(svg + lt_off, tag_len, "y1", buf, sizeof(buf)) && lv_parse_double(buf, &y1) == 0 &&
                    ggb_extract_attr(svg + lt_off, tag_len, "x2", buf, sizeof(buf)) && lv_parse_double(buf, &x2) == 0 &&
                    ggb_extract_attr(svg + lt_off, tag_len, "y2", buf, sizeof(buf)) && lv_parse_double(buf, &y2) == 0) {
                    double pts[4] = {x1, y1, x2, y2};
                    imported += svg_import_samples(graph, pts, 2, false, has_viewbox ? ox : 0, has_viewbox ? oy : 0);
                }
            } else if (lv_str_eq(name, "rect")) {
                char buf[64];
                double rx = 0, ry = 0, rw = 0, rh = 0;
                if (ggb_extract_attr(svg + lt_off, tag_len, "x", buf, sizeof(buf)) && lv_parse_double(buf, &rx) == 0 &&
                    ggb_extract_attr(svg + lt_off, tag_len, "y", buf, sizeof(buf)) && lv_parse_double(buf, &ry) == 0 &&
                    ggb_extract_attr(svg + lt_off, tag_len, "width", buf, sizeof(buf)) && lv_parse_double(buf, &rw) == 0 &&
                    ggb_extract_attr(svg + lt_off, tag_len, "height", buf, sizeof(buf)) && lv_parse_double(buf, &rh) == 0 &&
                    rw > 0.0 && rh > 0.0) {
                    double pts[8] = {rx, ry, rx + rw, ry, rx + rw, ry + rh, rx, ry + rh};
                    imported += svg_import_samples(graph, pts, 4, true, has_viewbox ? ox : 0, has_viewbox ? oy : 0);
                }
            } else if (lv_str_eq(name, "polyline") || lv_str_eq(name, "polygon")) {
                const char *pv = NULL;
                size_t pv_len = 0;
                if (ggb_extract_attr_len(svg + lt_off, tag_len, "points", &pv, &pv_len)) {
                    char *pbuf = (char *) lv_malloc(pv_len + 1);
                    if (pbuf) {
                        lv_strlcpy_n(pbuf, pv_len + 1, pv, (size_t) pv_len);
                        double *pts = (double *) lv_malloc(sizeof(double) * 2 * (size_t) SVG_PATH_MAX_POINTS);
                        if (pts) {
                            int n = svg_parse_points_attr(pbuf, pts, SVG_PATH_MAX_POINTS);
                            if (n > 0)
                                imported += svg_import_samples(graph, pts, n, lv_str_eq(name, "polygon"),
                                                              has_viewbox ? ox : 0, has_viewbox ? oy : 0);
                            lv_free((void **) &pts);
                        }
                        lv_free((void **) &pbuf);
                    }
                }
            }
        }
        pos = gt_off + 1;
    }

    lv_free((void **) &svg);

    if (imported == 0) {
        lv_set_error(lv_ERROR_PARSE,
                     "SVG导入完成但未找到任何可导入的几何元素（支持：path/circle/line/rect/polyline/polygon）");
    }
    return imported;
}
