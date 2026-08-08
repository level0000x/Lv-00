/**
 * @file lv_json.h
 * @brief Lv-00 统一 JSON 解析与写入库
 *
 * @details 提供统一的 JSON 解析器（lvJsonParser）和写入器（lvJsonBuf）API。
 *          解析器基于 graph_serialize.c 的 JsonParser（游标式），
 *          写入器基于 graph_serialize.c 的 JsonBuf（动态缓冲）。
 *          lv_json_find_key 等功能来自 lv_config.c 的独立 JSON 函数。
 *
 * @author Lv-00 Project
 */

#ifndef lv_JSON_H
#define lv_JSON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ===== JSON 解析器 ===== */

/**
 * @brief JSON 解析器状态结构体
 *
 * 基于游标的流式 JSON 解析器。管理指向输入数据的指针和当前位置。
 * 所有解析函数都会自动跳过空白。
 */
typedef struct {
    const char *data;  /* 输入数据起始位置 */
    size_t pos;        /* 当前解析位置 */
    size_t size;       /* 总数据长度 */
} lvJsonParser;

/** @brief 初始化解析器 */
void lv_json_parser_init(lvJsonParser *p, const char *data, size_t size);

/** @brief 跳过空白字符 */
void lv_json_skip_ws(lvJsonParser *p);

/**
 * @brief 偷看下一个字符（自动跳过空白，不解码）
 * @return 下一个字符，若已到达末尾返回 '\0'
 */
char lv_json_peek(lvJsonParser *p);

/**
 * @brief 读取下一个字符并前进（自动跳过空白）
 * @return 读取的字符，若已到达末尾返回 '\0'
 */
char lv_json_next(lvJsonParser *p);

/**
 * @brief 期望下一个字符为 c，否则返回 false
 * @param p 解析器指针
 * @param c 期望的字符
 * @return true 匹配成功，false 不匹配或已到末尾
 */
bool lv_json_expect(lvJsonParser *p, char c);

/**
 * @brief 解析 JSON 字符串
 *
 * 处理标准 JSON 转义序列：\\、\"、\/、\b、\f、\n、\r、\t、\uXXXX
 *
 * @param p 解析器指针
 * @return 新分配的内存中的字符串（调用者需 lv_free），失败返回 NULL
 */
char *lv_json_parse_string(lvJsonParser *p);

/**
 * @brief 解析 JSON 整数值（支持负号）
 * @param p   解析器指针
 * @param out 输出解析结果
 * @return true 解析成功，false 解析失败
 */
bool lv_json_parse_int(lvJsonParser *p, int *out);

/**
 * @brief 解析 JSON 64 位整数值（支持负号）
 *
 * 基于 lv_json_parse_int 的骨架扩展为 64 位，带溢出检测：
 * 数字超出 int64_t 范围时返回 false。
 *
 * @param p   解析器指针
 * @param out 输出解析结果
 * @return true 解析成功，false 解析失败（含溢出）
 */
bool lv_json_parse_int64(lvJsonParser *p, int64_t *out);

/**
 * @brief 解析 JSON 无符号 64 位整数值
 *
 * 不接受负号（遇到 '-' 返回 false），带溢出检测：
 * 数字超出 uint64_t 范围时返回 false。
 *
 * @param p   解析器指针
 * @param out 输出解析结果
 * @return true 解析成功，false 解析失败（含溢出）
 */
bool lv_json_parse_uint64(lvJsonParser *p, uint64_t *out);

/**
 * @brief 解析 JSON 浮点数值
 * @param p   解析器指针
 * @param out 输出解析结果
 * @return true 解析成功，false 解析失败
 */
bool lv_json_parse_double(lvJsonParser *p, double *out);

/**
 * @brief 解析 JSON 布尔值
 * @param p   解析器指针
 * @param out 输出解析结果
 * @return true 解析成功（true/false），false 解析失败
 */
bool lv_json_parse_bool(lvJsonParser *p, bool *out);

/**
 * @brief 在 JSON 对象顶层按名称查找键值
 *
 * 扫描 JSON 字符串，找到指定 key 后返回其对应值的起始位置。
 * 不会递归进入嵌套对象/数组，仅在最外层对象中搜索。
 *
 * @param json    JSON 字符串
 * @param key     要查找的键名（长度由 key_len 指定，不能为 NULL）
 * @param key_len 键名长度
 * @return 值的起始位置（跳过冒号和空白），未找到返回 NULL
 */
const char *lv_json_find_key(const char *json, const char *key, size_t key_len);

/**
 * @brief 跳过当前 JSON 值（对象/数组/字符串/数字/布尔/null）
 *
 * 解析器应位于某个 JSON 值的起始位置。调用后，解析器将越过该值，
 * 停在值的结束位置之后。
 *
 * @param p 解析器指针
 */
void lv_json_skip_value(lvJsonParser *p);

/**
 * @brief 解析 JSON 整数数组（形如 [1,2,3]）
 *
 * 解析器应位于 '[' 处。解析过程中自动跳过空白；遇到 ']' 或
 * 元素数量达到 max_count（越界）时停止收集。越界时剩余元素会被
 * 跳过，解析器停在数组结束位置之后。
 *
 * @param p         解析器指针
 * @param out       输出整数数组缓冲区（元素数量不超过 max_count）
 * @param max_count 输出缓冲区可容纳的最大元素数
 * @param out_count 实际解析的元素数量（可为 NULL）
 * @return true 数组结构合法，false 数组未闭合或元素类型不匹配
 */
bool lv_json_parse_int_array(lvJsonParser *p, int *out, size_t max_count, size_t *out_count);

/**
 * @brief 解析 JSON 浮点数组（形如 [1.5,2.5]）
 *
 * 解析器应位于 '[' 处。解析过程中自动跳过空白；遇到 ']' 或
 * 元素数量达到 max_count（越界）时停止收集。越界时剩余元素会被
 * 跳过，解析器停在数组结束位置之后。
 *
 * @param p         解析器指针
 * @param out       输出浮点数组缓冲区（元素数量不超过 max_count）
 * @param max_count 输出缓冲区可容纳的最大元素数
 * @param out_count 实际解析的元素数量（可为 NULL）
 * @return true 数组结构合法，false 数组未闭合或元素类型不匹配
 */
bool lv_json_parse_double_array(lvJsonParser *p, double *out, size_t max_count, size_t *out_count);

/* ===== JSON 写入器 ===== */

/**
 * @brief JSON 写入缓冲区
 *
 * 动态增长的 JSON 写入缓冲区。初始容量由 init 参数指定，
 * 写入过程中按需自动扩展（2 倍策略）。
 *
 * depth/has_elem/key_pending/pretty 由对象级 API（begin/end/append_key 等）
 * 维护，配合 lv_json_buf_init 初始化，旧 API 不感知这些成员。
 */
typedef struct {
    char *buffer;    /* 缓冲区数据 */
    size_t capacity; /* 缓冲区总容量 */
    size_t pos;      /* 当前写入位置 */
    unsigned depth;  /* 当前嵌套深度（begin/end 配对增减，缩进输出上限 64 级） */
    bool has_elem;   /* 当前容器是否已写入元素（决定元素前是否加逗号/换行） */
    bool key_pending; /* append_key 后等待值写入（值为 key 紧邻值时不再加逗号） */
    bool pretty;     /* 是否 pretty 输出（换行 + 2 空格/级缩进） */
} lvJsonBuf;

/**
 * @brief 初始化 JSON 写入缓冲区
 * @param buf           lvJsonBuf 指针
 * @param initial_size  初始缓冲区大小（字节）
 * @return true 初始化成功，false 内存分配失败
 */
bool lv_json_buf_init(lvJsonBuf *buf, size_t initial_size);

/**
 * @brief 确保缓冲区至少有 extra 字节的额外空间
 * @param buf   lvJsonBuf 指针
 * @param extra 需要的额外字节数
 */
void lv_json_buf_ensure(lvJsonBuf *buf, size_t extra);

/**
 * @brief 追加 JSON 字符串（自动转义特殊字符）
 *
 * 以 "..." 格式写入，对字符串中的 "、\\、\n、\t、\r、\b、\f
 * 及控制字符进行 JSON 转义。
 *
 * @param buf lvJsonBuf 指针
 * @param str 要追加的字符串（可为 NULL，此时写入 "null"）
 */
void lv_json_buf_append_string(lvJsonBuf *buf, const char *str);

/**
 * @brief 追加原始字符串（不转义）
 *
 * 直接将字符串内容写入缓冲区，不添加引号，不进行转义。
 * 用于直接拼接已格式化的 JSON 片段。
 *
 * @param buf lvJsonBuf 指针
 * @param str 要追加的原始字符串
 */
void lv_json_buf_append_raw(lvJsonBuf *buf, const char *str);

/**
 * @brief 追加单个字符
 * @param buf lvJsonBuf 指针
 * @param c   要追加的字符
 */
void lv_json_buf_append_char(lvJsonBuf *buf, char c);

/**
 * @brief 追加格式化内容（printf 风格）
 * @param buf lvJsonBuf 指针
 * @param fmt printf 格式字符串
 * @param ... 可变参数
 */
void lv_json_buf_append_fmt(lvJsonBuf *buf, const char *fmt, ...);

/**
 * @brief 终结 JSON 缓冲区并返回内容
 *
 * 将缓冲区所有权转移给调用者。调用者负责使用 lv_free 释放返回的字符串。
 * 调用后 lvJsonBuf 不应再被使用。
 *
 * @param buf lvJsonBuf 指针
 * @return 缓冲区内容字符串（调用者 lv_free），失败返回 NULL
 */
char *lv_json_buf_finalize(lvJsonBuf *buf);

/**
 * @brief 释放 JSON 写入缓冲区（不返回内容）
 * @param buf lvJsonBuf 指针
 */
void lv_json_buf_free(lvJsonBuf *buf);

/* ===== JSON 对象级写入 API ===== */

/**
 * @brief 设置/关闭 pretty 输出模式
 *
 * 开启后，对象/数组开始符输出后换行并按嵌套深度缩进（2 空格/级，
 * 超过 64 级按 64 级处理），元素间以 "," + 换行分隔，结束符前换行并回退缩进；
 * 关闭时输出保持紧凑。可在写入前或写入中随时切换。
 *
 * @param buf    lvJsonBuf 指针
 * @param pretty true 开启 pretty，false 关闭
 */
void lv_json_buf_set_pretty(lvJsonBuf *buf, bool pretty);

/**
 * @brief 开始一个 JSON 对象（写 '{'）
 *
 * 作为容器内元素时自动处理前置逗号/换行/缩进；作为对象值时需先
 * lv_json_buf_append_key。必须与 lv_json_buf_end_object 配对。
 *
 * @param buf lvJsonBuf 指针
 * @return true 成功，false buf 为空
 */
bool lv_json_buf_begin_object(lvJsonBuf *buf);

/**
 * @brief 结束当前 JSON 对象（写 '}'）
 * @param buf lvJsonBuf 指针
 * @return true 成功，false buf 为空
 */
bool lv_json_buf_end_object(lvJsonBuf *buf);

/**
 * @brief 开始一个 JSON 数组（写 '['）
 *
 * 语义与 lv_json_buf_begin_object 相同，必须与 lv_json_buf_end_array 配对。
 *
 * @param buf lvJsonBuf 指针
 * @return true 成功，false buf 为空
 */
bool lv_json_buf_begin_array(lvJsonBuf *buf);

/**
 * @brief 结束当前 JSON 数组（写 ']'）
 * @param buf lvJsonBuf 指针
 * @return true 成功，false buf 为空
 */
bool lv_json_buf_end_array(lvJsonBuf *buf);

/**
 * @brief 写入对象键（写 '"key":'）
 *
 * 自动处理键名引号与 JSON 转义，以及元素间逗号/换行/缩进。
 * 写入后必须紧跟一个值写入 API（append_int/double/bool/null/
 * append_string/begin_object/begin_array），该值前不会重复加逗号。
 *
 * @param buf lvJsonBuf 指针
 * @param key 键名（不能为 NULL）
 * @return true 成功，false buf 或 key 为空
 */
bool lv_json_buf_append_key(lvJsonBuf *buf, const char *key);

/**
 * @brief 写入整数数值（紧凑十进制，long long 范围）
 *
 * 作为对象值时需先 lv_json_buf_append_key；作为数组元素时自动处理
 * 元素间逗号/换行/缩进。
 *
 * @param buf lvJsonBuf 指针
 * @param v   整数值
 * @return true 成功，false buf 为空
 */
bool lv_json_buf_append_int(lvJsonBuf *buf, long long v);

/**
 * @brief 写入浮点数值（%.15g 风格，与现有序列化一致）
 * @param buf lvJsonBuf 指针
 * @param v   浮点值
 * @return true 成功，false buf 为空
 */
bool lv_json_buf_append_double(lvJsonBuf *buf, double v);

/**
 * @brief 写入布尔值（true/false）
 * @param buf lvJsonBuf 指针
 * @param v   布尔值
 * @return true 成功，false buf 为空
 */
bool lv_json_buf_append_bool(lvJsonBuf *buf, bool v);

/**
 * @brief 写入 null
 * @param buf lvJsonBuf 指针
 * @return true 成功，false buf 为空
 */
bool lv_json_buf_append_null(lvJsonBuf *buf);

/* ===== JSON 便利查询函数 ===== */

/**
 * @brief 在 JSON 对象中按名称查找并解析字符串值
 *
 * 典型的顶层键值查找。在 json 字符串的顶层对象中查找 key，
 * 将其值作为字符串解析并复制到 out 缓冲区。
 *
 * @param json    JSON 字符串
 * @param key     要查找的键名
 * @param out     输出缓冲区
 * @param out_size 输出缓冲区大小
 * @return true 查找并解析成功，false 键不存在或值不是字符串
 */
bool lv_json_get_string(const char *json, const char *key, char *out, size_t out_size);

/**
 * @brief 在 JSON 对象中按名称查找并解析整数值
 * @param json JSON 字符串
 * @param key  要查找的键名
 * @param out  输出解析结果
 * @return true 查找并解析成功
 */
bool lv_json_get_int(const char *json, const char *key, int *out);

/**
 * @brief 在 JSON 对象中按名称查找并解析浮点数值
 * @param json JSON 字符串
 * @param key  要查找的键名
 * @param out  输出解析结果
 * @return true 查找并解析成功
 */
bool lv_json_get_double(const char *json, const char *key, double *out);

/**
 * @brief 在 JSON 对象中按名称查找并解析布尔值
 * @param json JSON 字符串
 * @param key  要查找的键名
 * @param out  输出解析结果
 * @return true 查找并解析成功
 */
bool lv_json_get_bool(const char *json, const char *key, bool *out);

#ifdef __cplusplus
}
#endif

#endif /* lv_JSON_H */
