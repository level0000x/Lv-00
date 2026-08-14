/**
 * @file interop_import_ggb_zip.c
 * @brief GeoGebra ZIP 解析 + Deflate 解压（由 interop_import.c 拆分子模块）
 *
 * @details 包含 EOCD 搜索、中央目录查找、本地文件头提取与 raw Deflate
 *          （RFC 1951）自研解压器（存储块/固定哈夫曼/动态哈夫曼）。
 *          本模块提供 ggb_find_eocd / ggb_central_find_entry /
 *          ggb_local_data_offset / ggb_extract_entry 供 ggb_xml 子模块复用
 *          （声明见 interop_import_internal.h）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/lv_platform.h"
#include <string.h>
#include <zlib.h>

#include "lv/lv_utils.h"

#include "interop_import_internal.h"


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

/* INTEROP_COORD_DENOM_PRECISION 统一来自 lv/interop.h 的公共常量 */

/* ── GeoGebra ZIP 解析器 ──
 * 小端字节序读取统一走公共设施 lv_load_le32/lv_load_le16（lv_utils.h，
 * 判据 B 收敛：本地 ggb_read_u32_le/ggb_read_u16_le 手写展开已删）。 */

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
bool ggb_find_eocd(const uint8_t *data, size_t size, size_t *eocd_offset) {
    if (size < GGB_EOCD_MIN_SIZE)
        return false;
    size_t window = (size > GGB_EOCD_MAX_SEARCH) ? GGB_EOCD_MAX_SEARCH : size;
    size_t min_i = (size > window) ? (size - window) : 0;
    size_t i = size - GGB_EOCD_MIN_SIZE;
    while (i >= min_i) {
        if (lv_load_le32(data + i) == GGB_EOCD_SIG) {
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
bool ggb_central_find_entry(const uint8_t *data, size_t data_size, size_t eocd_offset,
                            const char *target, size_t *local_offset, size_t *comp_size,
                            size_t *uncomp_size, uint16_t *comp_method) {
    size_t total_entries = lv_load_le16(data + eocd_offset + 10);
    size_t cd_size = lv_load_le32(data + eocd_offset + 12);
    size_t cd_offset = lv_load_le32(data + eocd_offset + 16);
    if (cd_offset > data_size || cd_size > data_size - cd_offset)
        return false;

    size_t pos = cd_offset;
    size_t target_len = strlen(target);
    for (size_t e = 0; e < total_entries && pos + GGB_CENTRAL_DIR_MIN <= data_size; e++) {
        if (lv_load_le32(data + pos) != GGB_CENTRAL_DIR_SIG)
            return false;
        uint16_t method = lv_load_le16(data + pos + 10);
        size_t csize = lv_load_le32(data + pos + 20);
        size_t usize = lv_load_le32(data + pos + 24);
        uint16_t name_len = lv_load_le16(data + pos + 28);
        uint16_t extra_len = lv_load_le16(data + pos + 30);
        uint16_t comment_len = lv_load_le16(data + pos + 32);
        size_t local_off = lv_load_le32(data + pos + 42);

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
bool ggb_local_data_offset(const uint8_t *data, size_t data_size, size_t local_offset,
                           size_t *data_offset) {
    if (local_offset > data_size || GGB_LOCAL_HEADER_MIN > data_size - local_offset)
        return false;
    if (lv_load_le32(data + local_offset) != GGB_LOCAL_FILE_SIG)
        return false;
    uint16_t name_len = lv_load_le16(data + local_offset + 26);
    uint16_t extra_len = lv_load_le16(data + local_offset + 28);
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
bool ggb_extract_entry(const uint8_t *data, size_t data_size, size_t data_offset,
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
