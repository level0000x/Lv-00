/**
 * @file module_helpers.h
 * @brief 模块子系统共享内部类型和辅助函数声明
 *
 * 将 LvzTokenType/LvzToken/LvzParser、JsonReader 等类型定义
 * 和相关函数从 module.c / module_lvz.c / module_serialize.c 提取到此头文件，
 * 以便 module_delta.c 等兄弟文件也能使用。
 *
 * 说明：JSON 写入器（原 JsonWriter）已迁移至公共库 lv/lv_json.h 的 lvJsonBuf，
 *      模块内不再保留重复实现。
 *
 * 仅限模块子系统内部使用，外部代码不应包含此头文件。
 */

#ifndef lv_MODULE_HELPERS_H
#define lv_MODULE_HELPERS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lv/module.h"

#include "lexer_shared.h"

/* ============== LVZ 词法 Token 类型 ============== */

typedef enum {
    TOK_EOF,
    TOK_STRING,     /* "..." */
    TOK_NUMBER,     /* 整数或浮点数 */
    TOK_IDENTIFIER, /* 标识符 */
    TOK_LBRACE,     /* { */
    TOK_RBRACE,     /* } */
    TOK_ERROR
} LvzTokenType;

typedef struct {
    LvzTokenType type;
    char *str_value;
    double num_value;
    int line;
    int col;
} LvzToken;

/* LvzLexer 是 lvLexer 的别名 */
typedef lvLexer LvzLexer;

/* ============== LVZ 解析器 ============== */

/**
 * @brief LVZ 文件解析器上下文
 *
 * 包含词法分析器状态、当前 token、错误标志和模块目录路径。
 */
typedef struct {
    LvzLexer lexer;
    LvzToken current;
    bool has_error;
    char *module_dir;
} LvzParser;

/* LVZ 解析器函数声明 */
void lvz_parser_init(LvzParser *p, const char *source);
void lvz_parser_cleanup(LvzParser *p);
bool lvz_parse(LvzParser *p, Module *mod);

/* 从 .lvz 文件加载预设定义并注册 */
bool lvz_load_presets_file(const char *filepath);

/* LVZ 辅助函数（供 lvz_parse 内部使用的递归下降解析器） */
void lvz_parser_advance(LvzParser *p);
bool lvz_parser_expect(LvzParser *p, LvzTokenType type);
bool lvz_parser_expect_identifier(LvzParser *p, const char *name);
bool lvz_parser_expect_number(LvzParser *p, int *value);
bool lvz_parser_expect_string(LvzParser *p, char **out);

/* ============== JSON 读取器 ============== */

/**
 * @brief 最小化 JSON 读取器
 *
 * 只读的 JSON 反序列化辅助结构。
 */
typedef struct {
    const char *data;
    size_t size;
    size_t pos;
} JsonReader;

/* JSON 读取器函数声明 */
void json_reader_init(JsonReader *r, const char *data, size_t size);
void json_reader_skip_whitespace(JsonReader *r);
char json_reader_peek(JsonReader *r);
char json_reader_next(JsonReader *r);
bool json_reader_expect_char(JsonReader *r, char c);
char *json_reader_read_string(JsonReader *r);
bool json_reader_read_int(JsonReader *r, int64_t *out);
int json_reader_count_array_elements(JsonReader *r);

/* ============== LVZ 辅助词法分析函数 ============== */
/* 这些函数在 lexer_shared.c 中实现，此处声明以便模块文件使用 */
void lvz_lexer_init(LvzLexer *lex, const char *source);
void lvz_lexer_skip_whitespace_and_comments(LvzLexer *lex);
char *lvz_lexer_extract_string(LvzLexer *lex);
LvzToken lvz_lexer_next_token(LvzLexer *lex);

/* ============== 模块内部辅助函数 ============== */

/**
 * @brief 检查模块是否已在已访问列表中
 */
bool dependency_exists(Module **visited, int count, Module *mod);

/**
 * @brief 模块流式上下文（在 module.c 中定义，模块子文件共享使用）
 */
#include "stream.h"
extern lv_THREAD_LOCAL StreamContext *module_stream_ctx;

#ifdef __cplusplus
}
#endif

#endif /* lv_MODULE_HELPERS_H */
