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

/* ===== JSON 写入器 ===== */

/**
 * @brief JSON 写入缓冲区
 *
 * 动态增长的 JSON 写入缓冲区。初始容量由 init 参数指定，
 * 写入过程中按需自动扩展（2 倍策略）。
 */
typedef struct {
    char *buffer;    /* 缓冲区数据 */
    size_t capacity; /* 缓冲区总容量 */
    size_t pos;      /* 当前写入位置 */
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

#ifdef __cplusplus
}
#endif

#endif /* lv_JSON_H */
