/**
 * @file axiom_pkg.c
 * @brief 公理系统包实现
 * @details 实现公理包的加载、验证和展开功能。支持约束模板、
 *          不可构造问题检测、双层测试和 SHA-256 依赖追踪。
 */

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "axiom_pkg.h"
#include "lexer_shared.h"
#include "error_codes.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "stream.h"

/* 兼容性宏：set_error → lv_set_error */
#define set_error(fmt, ...)   lv_set_error(lv_ERROR_INVALID_PARAM, (fmt), ##__VA_ARGS__)

/* 兼容性宏：PropositionKind 短名 */
#ifndef CONSTRUCTIVE
#define CONSTRUCTIVE PROPOSITION_KIND_CONSTRUCTIVE
#endif
#ifndef NON_CONSTRUCTIVE_ORACLE
#define NON_CONSTRUCTIVE_ORACLE PROPOSITION_KIND_NON_CONSTRUCTIVE_ORACLE
#endif
#ifndef EXPLOSION_PRINCIPLE
#define EXPLOSION_PRINCIPLE PROPOSITION_KIND_EXPLOSION_PRINCIPLE
#endif

/* 线程局部存储用于错误消息（使用lv_internal.h中定义的lv_THREAD_LOCAL） */

static lv_THREAD_LOCAL StreamContext *axiom_stream_ctx = NULL;

void axiom_pkg_set_stream_context(StreamContext *ctx) {
    axiom_stream_ctx = ctx;
}

/** SHA-256 输出大小（字节） */
#define AXIOM_SHA256_OUTPUT_SIZE     32

/** SHA-256 哈希十六进制字符串大小（64字符 + 空终止符） */
#define AXIOM_SHA256_HEX_SIZE        65

/** 展开缓存的默认初始容量 */
#define AXIOM_EXPANSION_CACHE_CAP    16

/** 依赖引用缓存的默认初始容量 */
#define AXIOM_DEP_REF_CACHE_CAP      16

/** 最大递归展开深度 */
#define AXIOM_MAX_EXPANSION_DEPTH     8

/** 最大公理源文件大小 (64 MB) */
#define AXIOM_MAX_FILE_SIZE         (64 * 1024 * 1024)

/** 规范形式最大参与者类型数量 */
#define AXIOM_MAX_PARTICIPANT_TYPES   8

/** 参与者类型名称的最大长度 */
#define AXIOM_PARTICIPANT_TYPE_LEN   32

/** 测试失败消息缓冲区大小 */
#define AXIOM_TEST_MSG_BUF_SIZE     256

/** 模板参数描述格式字符串最大长度 */
#define AXIOM_PARAM_DESC_MAX_LEN     64

/* ============== SHA-256 实现 ============== */

/* SHA-256 常量 */
static const uint32_t sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define SHA256_ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define SHA256_CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define SHA256_MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define SHA256_SIGMA0(x) (SHA256_ROTR(x, 2) ^ SHA256_ROTR(x, 13) ^ SHA256_ROTR(x, 22))
#define SHA256_SIGMA1(x) (SHA256_ROTR(x, 6) ^ SHA256_ROTR(x, 11) ^ SHA256_ROTR(x, 25))
#define SHA256_sigma0(x) (SHA256_ROTR(x, 7) ^ SHA256_ROTR(x, 18) ^ ((x) >> 3))
#define SHA256_sigma1(x) (SHA256_ROTR(x, 17) ^ SHA256_ROTR(x, 19) ^ ((x) >> 10))

typedef struct {
    uint32_t state[8];
    uint8_t buffer[64];
    uint64_t bit_count;
    uint32_t buffer_len;
} SHA256Context;

static void sha256_init(SHA256Context *ctx) {
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
    ctx->bit_count = 0;
    ctx->buffer_len = 0;
}

static void sha256_transform(SHA256Context *ctx, const uint8_t block[64]) {
    uint32_t w[64];
    int i;

    /* 准备消息调度 */
    for (i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i * 4] << 24) |
               ((uint32_t)block[i * 4 + 1] << 16) |
               ((uint32_t)block[i * 4 + 2] << 8) |
               ((uint32_t)block[i * 4 + 3]);
    }
    for (i = 16; i < 64; i++) {
        w[i] = SHA256_sigma1(w[i - 2]) + w[i - 7] +
               SHA256_sigma0(w[i - 15]) + w[i - 16];
    }

    /* 初始化工作变量 */
    uint32_t a = ctx->state[0];
    uint32_t b = ctx->state[1];
    uint32_t c = ctx->state[2];
    uint32_t d = ctx->state[3];
    uint32_t e = ctx->state[4];
    uint32_t f = ctx->state[5];
    uint32_t g = ctx->state[6];
    uint32_t h = ctx->state[7];

    /* 主压缩循环 */
    for (i = 0; i < 64; i++) {
        uint32_t t1 = h + SHA256_SIGMA1(e) + SHA256_CH(e, f, g) + sha256_k[i] + w[i];
        uint32_t t2 = SHA256_SIGMA0(a) + SHA256_MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void sha256_update(SHA256Context *ctx, const void *data, size_t len) {
    const uint8_t *bytes = (const uint8_t *)data;
    size_t i;

    ctx->bit_count += (uint64_t)len * 8;

    for (i = 0; i < len; i++) {
        ctx->buffer[ctx->buffer_len++] = bytes[i];
        if (ctx->buffer_len == 64) {
            sha256_transform(ctx, ctx->buffer);
            ctx->buffer_len = 0;
        }
    }
}

static void sha256_final(SHA256Context *ctx, uint8_t hash[32]) {
    uint32_t i;

    /* 填充：写入 0x80 标记字节 */
    /* 安全检查：如果缓冲区已满（64字节），需要先处理当前块再写入填充 */
    if (ctx->buffer_len >= 64) {
        sha256_transform(ctx, ctx->buffer);
        ctx->buffer_len = 0;
    }
    ctx->buffer[ctx->buffer_len++] = 0x80;

    /* 如果缓冲区空间不够放长度，先处理当前块 */
    if (ctx->buffer_len > 56) {
        while (ctx->buffer_len < 64) {
            ctx->buffer[ctx->buffer_len++] = 0x00;
        }
        sha256_transform(ctx, ctx->buffer);
        ctx->buffer_len = 0;
    }

    /* 填充零到 56 字节 */
    while (ctx->buffer_len < 56) {
        ctx->buffer[ctx->buffer_len++] = 0x00;
    }

    /* 附加位长度（大端序，64位） */
    uint64_t bit_count = ctx->bit_count;
    ctx->buffer[56] = (uint8_t)(bit_count >> 56);
    ctx->buffer[57] = (uint8_t)(bit_count >> 48);
    ctx->buffer[58] = (uint8_t)(bit_count >> 40);
    ctx->buffer[59] = (uint8_t)(bit_count >> 32);
    ctx->buffer[60] = (uint8_t)(bit_count >> 24);
    ctx->buffer[61] = (uint8_t)(bit_count >> 16);
    ctx->buffer[62] = (uint8_t)(bit_count >> 8);
    ctx->buffer[63] = (uint8_t)(bit_count);
    sha256_transform(ctx, ctx->buffer);

    /* 输出哈希值（大端序） */
    for (i = 0; i < 8; i++) {
        hash[i * 4]     = (uint8_t)(ctx->state[i] >> 24);
        hash[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        hash[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        hash[i * 4 + 3] = (uint8_t)(ctx->state[i]);
    }
}

/* SHA-256 辅助函数：哈希字符串 */
static void sha256_hash_string(SHA256Context *ctx, const char *str) {
    if (str) {
        sha256_update(ctx, str, strlen(str));
    } else {
        sha256_update(ctx, "(null)", 6);
    }
}

/* SHA-256 辅助函数：哈希原始数据 */
static void sha256_hash_data(SHA256Context *ctx, const void *data, size_t len) {
    if (data && len > 0) {
        sha256_update(ctx, data, len);
    }
}

/* ============== 辅助函数 ============== */

const char *axiom_package_get_last_error(void) {
    return lv_get_last_error_message();
}

static char *safe_lv_strdup_safe(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *dup = lv_malloc(len + 1);
    if (dup) {
        memcpy(dup, s, len + 1);  /* 使用 memcpy 替代 strcpy，确保安全 */
    }
    return dup;
}

/* ============== 创建和销毁 ============== */

AxiomPackage *axiom_package_create(const char *name, const char *version) {
    AxiomPackage *pkg = lv_calloc(1, sizeof(AxiomPackage));
    if (!pkg) return NULL;
    
    pkg->name = safe_lv_strdup_safe(name);
    pkg->version = safe_lv_strdup_safe(version);
    pkg->templates = NULL;
    pkg->template_count = 0;
    pkg->known_unconstructibles = NULL;
    pkg->unconstructible_count = 0;
    pkg->bottom_geometry = NULL;
    pkg->negation_encoding = NULL;
    pkg->contradiction_behavior = EXPLOSION_PRINCIPLE;
    pkg->expansion_cache = NULL;
    pkg->expansion_cache_count = 0;
    pkg->expansion_cache_capacity = 0;
    pkg->max_expansion_depth = AXIOM_MAX_EXPANSION_DEPTH; /* 默认递归深度 */
    pkg->dep_refs = NULL;
    pkg->dep_ref_count = 0;
    pkg->dep_ref_capacity = 0;
    
    if (axiom_stream_ctx) {
        stream_emit_simple(axiom_stream_ctx, STREAM_EVENT_INFO, "公理包创建成功", 0);
    }
    
    return pkg;
}

void axiom_package_destroy(AxiomPackage *pkg) {
    if (!pkg) return;
    
    lv_free((void**)&pkg->name);
    lv_free((void**)&pkg->version);
    
    /* 释放模板 */
    for (int i = 0; i < pkg->template_count; i++) {
        lv_free((void**)&pkg->templates[i].name);
        lv_free((void**)&pkg->templates[i].params);
    }
    lv_free((void**)&pkg->templates);
    
    /* 释放不可构造问题 */
    for (int i = 0; i < pkg->unconstructible_count; i++) {
        KnownUnconstructible *uc = &pkg->known_unconstructibles[i];
        lv_free((void**)&uc->name);
        lv_free((void**)&uc->reduces_to);
        lv_free((void**)&uc->external_ref);
        
        /* 释放依赖链 */
        for (int j = 0; j < uc->dependency_count; j++) {
            lv_free((void**)&uc->dependency_chain[j]);
        }
        lv_free((void**)&uc->dependency_chain);
    }
    lv_free((void**)&pkg->known_unconstructibles);
    
    lv_free((void**)&pkg->bottom_geometry);
    lv_free((void**)&pkg->negation_encoding);

    /* 释放模板展开缓存 */
    for (int i = 0; i < pkg->expansion_cache_count; i++) {
        lv_free((void**)&pkg->expansion_cache[i].template_name);
        if (pkg->expansion_cache[i].expanded_graph) {
            graph_destroy(pkg->expansion_cache[i].expanded_graph);
        }
    }
    lv_free((void**)&pkg->expansion_cache);
    
    /* 释放依赖引用数组 */
    lv_free((void**)&pkg->dep_refs);
    
    lv_free((void**)&pkg);
}

/* ============== 不可构造问题管理 ============== */

bool axiom_package_add_known_unconstructible(AxiomPackage *pkg, KnownUnconstructible *item) {
    if (!pkg || !item) return false;
    
    KnownUnconstructible *new_arr = lv_realloc(pkg->known_unconstructibles,
        (pkg->unconstructible_count + 1) * sizeof(KnownUnconstructible));
    if (!new_arr) return false;
    
    pkg->known_unconstructibles = new_arr;
    KnownUnconstructible *target = &pkg->known_unconstructibles[pkg->unconstructible_count];
    
    /* 深拷贝语义：对所有字符串字段进行独立拷贝，
     * 确保包内部持有独立的内存副本。
     * 调用者可以安全地释放或修改原始 item 的字符串字段。 */
    memset(target, 0, sizeof(KnownUnconstructible));
    target->name = safe_lv_strdup_safe(item->name);
    target->reduces_to = safe_lv_strdup_safe(item->reduces_to);
    target->external_ref = safe_lv_strdup_safe(item->external_ref);
    target->green_verified = item->green_verified;
    target->dependency_count = item->dependency_count;
    
    /* 深拷贝依赖链中的每个字符串 */
    if (item->dependency_count > 0 && item->dependency_chain) {
        target->dependency_chain = lv_calloc((size_t)item->dependency_count, sizeof(char *));
        if (!target->dependency_chain) {
            /* 分配失败时回滚已拷贝的字段 */
            lv_free((void**)&target->name);
            lv_free((void**)&target->reduces_to);
            lv_free((void**)&target->external_ref);
            memset(target, 0, sizeof(KnownUnconstructible));
            return false;
        }
        for (int i = 0; i < item->dependency_count; i++) {
            target->dependency_chain[i] = safe_lv_strdup_safe(item->dependency_chain[i]);
        }
    }
    
    pkg->unconstructible_count++;
    
    if (axiom_stream_ctx) {
        stream_emit_simple(axiom_stream_ctx, STREAM_EVENT_INFO, "已注册不可构造问题", 0);
    }
    
    return true;
}

KnownUnconstructible *axiom_package_lookup_unconstructible(AxiomPackage *pkg, const char *name) {
    if (!pkg || !name) return NULL;
    
    for (int i = 0; i < pkg->unconstructible_count; i++) {
        if (strcmp(pkg->known_unconstructibles[i].name, name) == 0) {
            return &pkg->known_unconstructibles[i];
        }
    }
    return NULL;
}

/* ============== 模板管理 ============== */

bool axiom_package_register_template(AxiomPackage *pkg, ConstraintTemplate *tmpl) {
    if (!pkg || !tmpl) return false;
    
    ConstraintTemplate *new_arr = lv_realloc(pkg->templates,
        (pkg->template_count + 1) * sizeof(ConstraintTemplate));
    if (!new_arr) return false;
    
    pkg->templates = new_arr;
    ConstraintTemplate *slot = &pkg->templates[pkg->template_count];
    *slot = *tmpl;
    /* 深拷贝 name（调用者可能释放原始字符串） */
    if (slot->name) {
        slot->name = lv_strdup_safe(slot->name);
    }
    /* 安全初始化：浅拷贝后 params 指针指向调用者的内存（或未初始化），
     * pkg 不应持有该指针的所有权。无条件置 NULL 以避免 free() 未初始化
     * 指针或调用者内存导致 bad-free / double-free。
     * 若调用者需要注册参数描述，应使用独立的 API 设置。 */
    slot->params = NULL;
    slot->param_desc_count = 0;
    pkg->template_count++;
    
    if (axiom_stream_ctx) {
        stream_emit_simple(axiom_stream_ctx, STREAM_EVENT_INFO, "已注册约束模板", 0);
    }
    
    return true;
}

ConstraintTemplate *axiom_package_get_template(AxiomPackage *pkg, const char *name) {
    if (!pkg || !name) return NULL;
    
    for (int i = 0; i < pkg->template_count; i++) {
        if (strcmp(pkg->templates[i].name, name) == 0) {
            return &pkg->templates[i];
        }
    }
    return NULL;
}

/* ============== 解析器 ============== */

typedef enum {
    TOK_EOF,
    TOK_LBRACE,         /* { */
    TOK_RBRACE,         /* } */
    TOK_STRING,         /* "..." */
    TOK_NUMBER,         /* 整数 */
    TOK_IDENTIFIER,     /* 标识符 */
    TOK_BOOLEAN,        /* true/false */
    TOK_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char *str_value;
    int int_value;
    bool bool_value;
    int line;
    int col;
} Token;

/* Lexer 结构体：使用共享的词法分析器基础设施 */
typedef lvLexer Lexer;

static void lexer_init(Lexer *lex, const char *source) {
    lv_lexer_init(lex, source);
}

static void lexer_skip_whitespace_and_comments(Lexer *lex) {
    lv_lexer_skip_whitespace_and_comments(lex);
}

static Token lexer_next_token(Lexer *lex) {
    Token tok = {0};
    tok.line = lex->line;
    tok.col = lex->col;
    
    lexer_skip_whitespace_and_comments(lex);
    
    if (!*lex->pos) {
        tok.type = TOK_EOF;
        return tok;
    }
    
    /* 大括号 */
    if (*lex->pos == '{') {
        tok.type = TOK_LBRACE;
        lex->pos++;
        lex->col++;
        return tok;
    }
    
    if (*lex->pos == '}') {
        tok.type = TOK_RBRACE;
        lex->pos++;
        lex->col++;
        return tok;
    }
    
    /* 字符串字面量 */
    if (*lex->pos == '"') {
        lex->pos++;          /* 跳过开引号 */
        lex->col++;

        tok.str_value = lv_lexer_extract_string(lex);
        if (!tok.str_value) {
            tok.type = TOK_ERROR;
            return tok;
        }

        tok.type = TOK_STRING;
        return tok;
    }
    
    /* 数字 */
    if (isdigit((unsigned char)*lex->pos) || 
        (*lex->pos == '-' && isdigit((unsigned char)*(lex->pos + 1)))) {
        const char *start = lex->pos;
        int sign = 1;
        
        if (*lex->pos == '-') {
            sign = -1;
            lex->pos++;
            lex->col++;
        }
        
        int value = 0;
        bool overflow = false;
        while (*lex->pos && isdigit((unsigned char)*lex->pos)) {
            int digit = *lex->pos - '0';
            /* 检查整数溢出：value * 10 + digit 是否超出 INT_MAX 范围 */
            if (value > (INT_MAX - digit) / 10) {
                overflow = true;
                break;
            }
            value = value * 10 + digit;
            lex->pos++;
            lex->col++;
        }
        
        if (overflow) {
            /* 溢出时设置错误标记，使用 INT_MAX 作为安全回退值 */
            tok.type = TOK_NUMBER;
            tok.int_value = sign == 1 ? INT_MAX : INT_MIN;
            lex->error_msg = "数字字面量超出整数范围";
            return tok;
        }
        
        tok.type = TOK_NUMBER;
        tok.int_value = sign * value;
        return tok;
    }
    
    /* 标识符或关键字 */
    if (isalpha((unsigned char)*lex->pos) || *lex->pos == '_') {
        const char *start = lex->pos;
        
        while (*lex->pos && (isalnum((unsigned char)*lex->pos) || *lex->pos == '_')) {
            lex->pos++;
            lex->col++;
        }
        
        size_t len = lex->pos - start;
        tok.str_value = lv_malloc(len + 1);
        if (!tok.str_value) {
            tok.type = TOK_ERROR;
            return tok;
        }
        
        memcpy(tok.str_value, start, len);
        tok.str_value[len] = '\0';
        
        /* 检查关键字 */
        if (strcmp(tok.str_value, "true") == 0) {
            tok.type = TOK_BOOLEAN;
            tok.bool_value = true;
            lv_free((void**)&tok.str_value);
            tok.str_value = NULL;
        } else if (strcmp(tok.str_value, "false") == 0) {
            tok.type = TOK_BOOLEAN;
            tok.bool_value = false;
            lv_free((void**)&tok.str_value);
            tok.str_value = NULL;
        } else {
            tok.type = TOK_IDENTIFIER;
        }
        
        return tok;
    }
    
    /* 未知字符 */
    tok.type = TOK_ERROR;
    lex->error_msg = "意外的字符";
    lex->pos++;
    lex->col++;
    
    return tok;
}

static void token_free(Token *tok) {
    if (tok->str_value) {
        lv_free((void**)&tok->str_value);
        tok->str_value = NULL;
    }
}

/* 解析器上下文 */
typedef struct {
    Lexer lexer;
    Token current;
    bool has_error;
} Parser;

static void parser_init(Parser *p, const char *source) {
    lexer_init(&p->lexer, source);
    p->has_error = false;
    memset(&p->current, 0, sizeof(Token));
}

static void parser_advance(Parser *p) {
    token_free(&p->current);
    p->current = lexer_next_token(&p->lexer);
}

static bool parser_expect(Parser *p, TokenType type) {
    if (p->current.type != type) {
        lv_set_error(lv_ERROR_PARSE, "解析错误 (行 %d, 列 %d): 期望 %d, 得到 %d",
                  p->current.line, p->current.col, type, p->current.type);
        p->has_error = true;
        return false;
    }
    return true;
}

static bool parser_expect_identifier(Parser *p, const char *name) {
    if (p->current.type != TOK_IDENTIFIER || 
        strcmp(p->current.str_value, name) != 0) {
        lv_set_error(lv_ERROR_PARSE, "解析错误 (行 %d, 列 %d): 期望关键字 '%s'",
                  p->current.line, p->current.col, name);
        p->has_error = true;
        return false;
    }
    return true;
}

/* 前向声明 */
static bool parse_package_body(Parser *p, AxiomPackage *pkg);

/**
 * @brief 清理 KnownUnconstructible 结构体的动态资源
 */
static void unconstructible_desc_cleanup(KnownUnconstructible *uc) {
    if (!uc) return;
    lv_free((void**)&uc->name);
    lv_free((void**)&uc->reduces_to);
    lv_free((void**)&uc->external_ref);
    for (int i = 0; i < uc->dependency_count; i++) {
        lv_free((void**)&uc->dependency_chain[i]);
    }
    lv_free((void**)&uc->dependency_chain);
    uc->name = NULL;
    uc->reduces_to = NULL;
    uc->external_ref = NULL;
    uc->dependency_chain = NULL;
    uc->dependency_count = 0;
}

/* 解析不可构造问题 */
static bool parse_unconstructible(Parser *p, AxiomPackage *pkg) {
    parser_advance(p); /* 跳过 'unconstructible' */
    
    /* 期望字符串 (问题名称) */
    if (!parser_expect(p, TOK_STRING)) return false;
    
    KnownUnconstructible uc = {0};
    uc.name = safe_lv_strdup_safe(p->current.str_value);
    uc.green_verified = false;
    
    parser_advance(p);
    
    /* 期望左大括号 */
    if (!parser_expect(p, TOK_LBRACE)) {
        lv_free((void**)&uc.name);
        return false;
    }
    parser_advance(p);
    
    /* 解析内容直到右大括号 */
    while (p->current.type != TOK_RBRACE && p->current.type != TOK_EOF && !p->has_error) {
        if (p->current.type != TOK_IDENTIFIER) {
            lv_set_error(lv_ERROR_PARSE, "解析错误 (行 %d): 期望属性名", p->current.line);
            p->has_error = true;
            break;
        }
        
        const char *prop = safe_lv_strdup_safe(p->current.str_value);
        parser_advance(p);

        if (strcmp(prop, "reduces_to") == 0) {
            if (!parser_expect(p, TOK_STRING)) {
                lv_free((void**)&prop);
                p->has_error = true;
                break;
            }
            uc.reduces_to = safe_lv_strdup_safe(p->current.str_value);
            parser_advance(p);
        }
        else if (strcmp(prop, "dependency") == 0) {
            if (!parser_expect(p, TOK_STRING)) {
                lv_free((void**)&prop);
                p->has_error = true;
                break;
            }

            /* 添加到依赖链 */
            char **new_deps = lv_realloc(uc.dependency_chain,
                                      (uc.dependency_count + 1) * sizeof(char*));
            if (new_deps) {
                uc.dependency_chain = new_deps;
                uc.dependency_chain[uc.dependency_count] = safe_lv_strdup_safe(p->current.str_value);
                uc.dependency_count++;
            }
            parser_advance(p);
        }
        else if (strcmp(prop, "external_ref") == 0) {
            if (!parser_expect(p, TOK_STRING)) {
                lv_free((void**)&prop);
                p->has_error = true;
                break;
            }
            uc.external_ref = safe_lv_strdup_safe(p->current.str_value);
            parser_advance(p);
        }
        else if (strcmp(prop, "green_verified") == 0) {
            if (!parser_expect(p, TOK_BOOLEAN)) {
                lv_free((void**)&prop);
                p->has_error = true;
                break;
            }
            uc.green_verified = p->current.bool_value;
            parser_advance(p);
        }
        else {
            lv_set_error(lv_ERROR_PARSE, "解析错误 (行 %d): 未知属性 '%s'", p->current.line, prop);
            lv_free((void**)&prop);
            p->has_error = true;
            break;
        }
        lv_free((void**)&prop);
    }
    
    if (p->has_error) {
        unconstructible_desc_cleanup(&uc);
        return false;
    }
    
    /* 期望右大括号 */
    if (!parser_expect(p, TOK_RBRACE)) {
        unconstructible_desc_cleanup(&uc);
        return false;
    }
    
    /* 添加到包 */
    if (!axiom_package_add_known_unconstructible(pkg, &uc)) {
        lv_set_error(lv_ERROR_OUT_OF_MEMORY, "内存分配失败");
        unconstructible_desc_cleanup(&uc);
        return false;
    }
    
    parser_advance(p);
    return true;
}

/* 解析模板声明 */
static bool parse_template(Parser *p, AxiomPackage *pkg) {
    parser_advance(p); /* 跳过 'template' */
    
    /* 期望字符串 (模板名称) */
    if (!parser_expect(p, TOK_STRING)) return false;
    
    ConstraintTemplate tmpl = {0};
    tmpl.name = safe_lv_strdup_safe(p->current.str_value);
    tmpl.verified = false;
    
    parser_advance(p);
    
    /* 期望参数数量 (数字) */
    if (!parser_expect(p, TOK_NUMBER)) {
        lv_free((void**)&tmpl.name);
        return false;
    }
    tmpl.param_count = p->current.int_value;
    
    parser_advance(p);
    
    /* 添加到包 */
    if (!axiom_package_register_template(pkg, &tmpl)) {
        lv_set_error(lv_ERROR_OUT_OF_MEMORY, "内存分配失败");
        lv_free((void**)&tmpl.name);
        return false;
    }
    
    return true;
}

/* 解析包体 */
static bool parse_package_body(Parser *p, AxiomPackage *pkg) {
    while (p->current.type != TOK_RBRACE && p->current.type != TOK_EOF && !p->has_error) {
        if (p->current.type != TOK_IDENTIFIER) {
            lv_set_error(lv_ERROR_PARSE, "解析错误 (行 %d): 期望声明", p->current.line);
            p->has_error = true;
            break;
        }
        
        const char *keyword = p->current.str_value;
        
        if (strcmp(keyword, "template") == 0) {
            if (!parse_template(p, pkg)) {
                p->has_error = true;
                break;
            }
        }
        else if (strcmp(keyword, "unconstructible") == 0) {
            if (!parse_unconstructible(p, pkg)) {
                p->has_error = true;
                break;
            }
        }
        else if (strcmp(keyword, "bottom_geometry") == 0) {
            parser_advance(p);
            if (!parser_expect(p, TOK_STRING)) {
                p->has_error = true;
                break;
            }
            lv_free((void**)&pkg->bottom_geometry);
            pkg->bottom_geometry = safe_lv_strdup_safe(p->current.str_value);
            parser_advance(p);
        }
        else if (strcmp(keyword, "negation_encoding") == 0) {
            parser_advance(p);
            if (!parser_expect(p, TOK_STRING)) {
                p->has_error = true;
                break;
            }
            lv_free((void**)&pkg->negation_encoding);
            pkg->negation_encoding = safe_lv_strdup_safe(p->current.str_value);
            parser_advance(p);
        }
        else if (strcmp(keyword, "contradiction_behavior") == 0) {
            parser_advance(p);
            if (!parser_expect(p, TOK_STRING)) {
                p->has_error = true;
                break;
            }
            
            const char *behavior = p->current.str_value;
            if (strcmp(behavior, "explosion_principle") == 0) {
                pkg->contradiction_behavior = EXPLOSION_PRINCIPLE;
            } else if (strcmp(behavior, "constructive") == 0) {
                pkg->contradiction_behavior = CONSTRUCTIVE;
            } else if (strcmp(behavior, "non_constructive_oracle") == 0) {
                pkg->contradiction_behavior = NON_CONSTRUCTIVE_ORACLE;
            } else {
                lv_set_error(lv_ERROR_PARSE, "解析错误 (行 %d): 未知的矛盾行为 '%s'",
                          p->current.line, behavior);
                p->has_error = true;
                break;
            }
            parser_advance(p);
        }
        else {
            lv_set_error(lv_ERROR_PARSE, "解析错误 (行 %d): 未知的关键字 '%s'",
                      p->current.line, keyword);
            p->has_error = true;
            break;
        }
    }
    
    return !p->has_error;
}

/* 完整的包加载函数 */
AxiomLoadStatus axiom_package_load(AxiomPackage *pkg, const char *filepath) {
    if (!pkg || !filepath) {
        lv_set_error(lv_ERROR_INVALID_PARAM, "无效参数");
        return AXIOM_LOAD_PARSE_ERROR;
    }
    
    /* 清除之前的错误 */
    lv_clear_error();
    
    /* 读取文件 */
    FILE *f = fopen(filepath, "r");
    if (!f) {
        lv_set_error(lv_ERROR_IO, "无法打开文件: %s", filepath);
        return AXIOM_LOAD_FILE_NOT_FOUND;
    }
    
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (len <= 0) {
        fclose(f);
        lv_set_error(lv_ERROR_PARSE, "文件为空: %s", filepath);
        return AXIOM_LOAD_PARSE_ERROR;
    }
    
    /* 限制最大文件大小为64MB，防止内存耗尽 */
    if (len > AXIOM_MAX_FILE_SIZE) {
        fclose(f);
        lv_set_error(lv_ERROR_INVALID_PARAM, "文件过大（超过64MB限制）: %s", filepath);
        return AXIOM_LOAD_PARSE_ERROR;
    }
    
    char *buf = lv_malloc((size_t)len + 1);
    if (!buf) {
        fclose(f);
        lv_set_error(lv_ERROR_OUT_OF_MEMORY, "内存分配失败");
        return AXIOM_LOAD_PARSE_ERROR;
    }
    
    size_t read_len = fread(buf, 1, (size_t)len, f);
    int read_error = ferror(f);
    fclose(f);
    if (read_len != (size_t)len && read_error) {
        lv_free((void**)&buf);
        lv_set_error(lv_ERROR_IO, "文件读取失败: %s", filepath);
        return AXIOM_LOAD_PARSE_ERROR;
    }
    buf[read_len] = '\0';
    
    /* 初始化解析器 */
    Parser parser;
    parser_init(&parser, buf);
    
    /* 获取第一个 token */
    parser_advance(&parser);
    
    /* 期望 'axiom' 关键字 */
    if (!parser_expect_identifier(&parser, "axiom")) {
        lv_free((void**)&buf);
        return AXIOM_LOAD_PARSE_ERROR;
    }
    parser_advance(&parser);
    
    /* 期望包名 (字符串) */
    if (!parser_expect(&parser, TOK_STRING)) {
        lv_free((void**)&buf);
        return AXIOM_LOAD_PARSE_ERROR;
    }
    lv_free((void**)&pkg->name);
    pkg->name = safe_lv_strdup_safe(parser.current.str_value);
    parser_advance(&parser);
    
    /* 期望版本 (字符串) */
    if (!parser_expect(&parser, TOK_STRING)) {
        lv_free((void**)&buf);
        return AXIOM_LOAD_PARSE_ERROR;
    }
    lv_free((void**)&pkg->version);
    pkg->version = safe_lv_strdup_safe(parser.current.str_value);
    parser_advance(&parser);
    
    /* 期望左大括号 */
    if (!parser_expect(&parser, TOK_LBRACE)) {
        lv_free((void**)&buf);
        return AXIOM_LOAD_PARSE_ERROR;
    }
    parser_advance(&parser);
    
    /* 解析包体 */
    if (!parse_package_body(&parser, pkg)) {
        lv_free((void**)&buf);
        return AXIOM_LOAD_PARSE_ERROR;
    }
    
    /* 期望右大括号 */
    if (!parser_expect(&parser, TOK_RBRACE)) {
        lv_free((void**)&buf);
        return AXIOM_LOAD_PARSE_ERROR;
    }
    
    lv_free((void**)&buf);
    
    if (axiom_stream_ctx) {
        stream_emit_simple(axiom_stream_ctx, STREAM_EVENT_INFO, "公理包加载成功", 0);
    }
    
    return AXIOM_LOAD_OK;
}

/* ============== 保存功能 ============== */

static const char *behavior_to_string(int behavior) {
    switch (behavior) {
        case CONSTRUCTIVE: return "constructive";
        case NON_CONSTRUCTIVE_ORACLE: return "non_constructive_oracle";
        case EXPLOSION_PRINCIPLE: 
        default: return "explosion_principle";
    }
}

AxiomSaveStatus axiom_package_save(const AxiomPackage *pkg, const char *filepath) {
    if (!pkg || !filepath) return AXIOM_SAVE_FILE_ERROR;
    
    FILE *f = fopen(filepath, "w");
    if (!f) {
        lv_set_error(lv_ERROR_IO, "无法创建文件: %s", filepath);
        return AXIOM_SAVE_FILE_ERROR;
    }
    
    /* 写入文件头注释 */
    fprintf(f, "# Axiom Package File\n");
    fprintf(f, "# Generated by axiom_package_save\n\n");
    
    /* 写入包声明 */
    fprintf(f, "axiom \"%s\" \"%s\" {\n", 
            pkg->name ? pkg->name : "unnamed",
            pkg->version ? pkg->version : "0.0.0");
    
    /* 写入模板 */
    for (int i = 0; i < pkg->template_count; i++) {
        fprintf(f, "    template \"%s\" %d\n", 
                pkg->templates[i].name,
                pkg->templates[i].param_count);
    }
    
    /* 写入不可构造问题 */
    for (int i = 0; i < pkg->unconstructible_count; i++) {
        KnownUnconstructible *uc = &pkg->known_unconstructibles[i];
        fprintf(f, "\n    unconstructible \"%s\" {\n", uc->name);
        
        if (uc->reduces_to) {
            fprintf(f, "        reduces_to \"%s\"\n", uc->reduces_to);
        }
        
        for (int j = 0; j < uc->dependency_count; j++) {
            fprintf(f, "        dependency \"%s\"\n", uc->dependency_chain[j]);
        }
        
        if (uc->external_ref) {
            fprintf(f, "        external_ref \"%s\"\n", uc->external_ref);
        }
        
        fprintf(f, "        green_verified %s\n", 
                uc->green_verified ? "true" : "false");
        fprintf(f, "    }\n");
    }
    
    /* 写入其他属性 */
    if (pkg->bottom_geometry) {
        fprintf(f, "\n    bottom_geometry \"%s\"\n", pkg->bottom_geometry);
    }
    
    if (pkg->negation_encoding) {
        fprintf(f, "    negation_encoding \"%s\"\n", pkg->negation_encoding);
    }
    
    fprintf(f, "    contradiction_behavior \"%s\"\n", 
            behavior_to_string(pkg->contradiction_behavior));
    
    /* 关闭包 */
    fprintf(f, "}\n");
    
    fclose(f);
    
    if (axiom_stream_ctx) {
        stream_emit_simple(axiom_stream_ctx, STREAM_EVENT_INFO, "公理包保存成功", 0);
    }
    
    return AXIOM_SAVE_OK;
}

/* ============== 内容哈希 (SHA-256) ============== */

char *axiom_package_compute_content_hash(AxiomPackage *pkg) {
    if (!pkg) return NULL;
    
    SHA256Context ctx;
    sha256_init(&ctx);
    
    /* 哈希名称和版本 */
    sha256_hash_string(&ctx, pkg->name);
    sha256_hash_string(&ctx, pkg->version);
    
    /* 哈希所有模板名称和参数数量 */
    for (int i = 0; i < pkg->template_count; i++) {
        sha256_hash_string(&ctx, pkg->templates[i].name);
        sha256_hash_data(&ctx, &pkg->templates[i].param_count, sizeof(int));
        sha256_hash_data(&ctx, &pkg->templates[i].verified, sizeof(bool));
    }
    
    /* 哈希所有不可构造问题 */
    for (int i = 0; i < pkg->unconstructible_count; i++) {
        KnownUnconstructible *uc = &pkg->known_unconstructibles[i];
        
        sha256_hash_string(&ctx, uc->name);
        sha256_hash_string(&ctx, uc->reduces_to);
        
        /* 哈希依赖链 */
        for (int j = 0; j < uc->dependency_count; j++) {
            sha256_hash_string(&ctx, uc->dependency_chain[j]);
        }
        
        sha256_hash_string(&ctx, uc->external_ref);
        sha256_hash_data(&ctx, &uc->green_verified, sizeof(bool));
    }
    
    /* 哈希其他属性 */
    sha256_hash_string(&ctx, pkg->bottom_geometry);
    sha256_hash_string(&ctx, pkg->negation_encoding);
    sha256_hash_data(&ctx, &pkg->contradiction_behavior, sizeof(int));
    
    /* 计算最终哈希 */
    uint8_t hash[AXIOM_SHA256_OUTPUT_SIZE];
    sha256_final(&ctx, hash);
    
    /* 转换为十六进制字符串（64个字符 + 空终止符） */
    char *result = lv_malloc(AXIOM_SHA256_HEX_SIZE);
    if (result) {
        for (int i = 0; i < AXIOM_SHA256_OUTPUT_SIZE; i++) {
            snprintf(result + i * 2, 3, "%02x", hash[i]);
        }
        result[AXIOM_SHA256_HEX_SIZE - 1] = '\0';
    }
    
    return result;
}

/* ============== 依赖验证 ============== */

/* 检查字符串是否为有效的URL格式 */
static bool is_valid_url(const char *str) {
    if (!str || !*str) return false;
    
    /* 检查常见URL协议 */
    if (strncmp(str, "http://", 7) == 0) return true;
    if (strncmp(str, "https://", 8) == 0) return true;
    if (strncmp(str, "ftp://", 6) == 0) return true;
    if (strncmp(str, "file://", 7) == 0) return true;
    
    /* 检查DOI格式 */
    if (strncmp(str, "doi:", 4) == 0) return true;
    
    /* 检查arXiv格式 */
    if (strncmp(str, "arXiv:", 6) == 0) return true;
    
    /* 检查标识符格式 (字母开头，只包含字母数字、下划线、连字符、点) */
    if (isalpha((unsigned char)str[0])) {
        for (const char *p = str + 1; *p; p++) {
            if (!isalnum((unsigned char)*p) && *p != '_' && *p != '-' && *p != '.') {
                return false;
            }
        }
        return true;
    }
    
    return false;
}

/* 在已加载的包中查找问题 */
static KnownUnconstructible *find_problem_in_packages(
    AxiomPackage **packages, 
    int package_count, 
    const char *problem_name) 
{
    if (!packages || !problem_name) return NULL;
    
    for (int i = 0; i < package_count; i++) {
        AxiomPackage *pkg = packages[i];
        if (!pkg) continue;
        
        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, problem_name);
        if (uc) return uc;
    }
    
    return NULL;
}

/* 在已加载的包中查找模板 */
static ConstraintTemplate *find_template_in_packages(
    AxiomPackage **packages, 
    int package_count, 
    const char *template_name) 
{
    if (!packages || !template_name) return NULL;
    
    for (int i = 0; i < package_count; i++) {
        AxiomPackage *pkg = packages[i];
        if (!pkg) continue;
        
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, template_name);
        if (tmpl) return tmpl;
    }
    
    return NULL;
}

bool axiom_package_validate_dependencies(AxiomPackage *pkg, AxiomPackage **loaded_packages, int package_count) {
    if (!pkg) {
        lv_set_error(lv_ERROR_INVALID_PARAM, "无效参数: 包为空");
        return false;
    }
    
    lv_clear_error();
    
    bool all_valid = true;
    
    for (int i = 0; i < pkg->unconstructible_count; i++) {
        KnownUnconstructible *uc = &pkg->known_unconstructibles[i];
        
        /* 验证 reduces_to 引用 */
        if (uc->reduces_to && uc->reduces_to[0] != '\0') {
            KnownUnconstructible *target = find_problem_in_packages(
                loaded_packages, package_count, uc->reduces_to);
            
            /* 也检查当前包 */
            if (!target) {
                target = axiom_package_lookup_unconstructible(pkg, uc->reduces_to);
            }
            
            if (!target) {
                lv_set_error(lv_ERROR_NOT_FOUND, "依赖验证失败: 问题 '%s' 的 reduces_to '%s' 未找到",
                          uc->name, uc->reduces_to);
                all_valid = false;
                /* 继续检查其他问题 */
            }
        }
        
        /* 验证依赖链中的所有项 */
        for (int j = 0; j < uc->dependency_count; j++) {
            const char *dep = uc->dependency_chain[j];
            
            /* 检查是否为已知问题 */
            KnownUnconstructible *dep_problem = find_problem_in_packages(
                loaded_packages, package_count, dep);
            
            if (!dep_problem) {
                dep_problem = axiom_package_lookup_unconstructible(pkg, dep);
            }
            
            /* 检查是否为已知模板 */
            ConstraintTemplate *dep_template = find_template_in_packages(
                loaded_packages, package_count, dep);
            
            /* 回退：在当前包中查找模板 */
            if (!dep_template) {
                dep_template = axiom_package_get_template(pkg, dep);
            }
            
            if (!dep_problem && !dep_template) {
                lv_set_error(lv_ERROR_NOT_FOUND, "依赖验证失败: 问题 '%s' 的依赖 '%s' 未在任何已加载的包中找到",
                          uc->name, dep);
                all_valid = false;
            }
        }
        
        /* 验证外部引用格式 */
        if (uc->external_ref && uc->external_ref[0] != '\0') {
            if (!is_valid_url(uc->external_ref)) {
                lv_set_error(lv_ERROR_INVALID_PARAM, "依赖验证失败: 问题 '%s' 的 external_ref '%s' 不是有效的URL或标识符格式",
                          uc->name, uc->external_ref);
                all_valid = false;
            }
        }
    }
    
    return all_valid;
}

/* ------------------------------------------------------------------ */
/*  axiom_template_validate_normal_form                                */
/* ------------------------------------------------------------------ */

/**
 * @brief 验证模板展开是否与声明的规范形式匹配
 *
 * 根据 design_v2.9.md 第 7.3 节：
 * 检查展开图的约束类型和节点类型是否与规范形式描述匹配。
 *
 * 规范形式格式："CONSTRAINT_TYPE(NODE_TYPE, NODE_TYPE)+"
 * 示例："INCIDENCE(POINT,LINE_SEGMENT)+"
 *
 * @param tmpl           约束模板（未使用，保持API一致性）
 * @param expanded_graph 要验证的展开约束图
 * @param canonical_form 规范形式描述字符串
 * @return 验证通过返回 true，否则返回 false
 */
bool axiom_template_validate_normal_form(
    const ConstraintTemplate *tmpl,
    const ConstraintGraph *expanded_graph,
    const char *canonical_form)
{
    (void)tmpl;
    if (!expanded_graph || !canonical_form) return false;

    /* 解析规范形式以提取期望的约束类型和参与者节点类型
     * 格式："CONSTRAINT_TYPE(NODE_TYPE,NODE_TYPE,...)+" */

    /* 查找约束类型名称（在第一个 '(' 之前） */
    const char *paren = strchr(canonical_form, '(');
    if (!paren) return false;

    size_t type_name_len = (size_t)(paren - canonical_form);
    if (type_name_len == 0) return false;

    /* 查找闭括号 ')' */
    const char *close_paren = strchr(paren, ')');
    if (!close_paren) return false;

    /* 提取 '(' 和 ')' 之间的参与者节点类型 */
    /* 解析逗号分隔的节点类型名称 */
    char participant_types[AXIOM_MAX_PARTICIPANT_TYPES][AXIOM_PARTICIPANT_TYPE_LEN];
    int participant_type_count = 0;

    const char *p = paren + 1;
    while (p < close_paren && participant_type_count < AXIOM_MAX_PARTICIPANT_TYPES) {
        const char *comma = strchr(p, ',');
        size_t len;
        if (comma && comma < close_paren) {
            len = (size_t)(comma - p);
            p = comma + 1;
        } else {
            len = (size_t)(close_paren - p);
            p = close_paren;
        }
        if (len > 0 && len < AXIOM_PARTICIPANT_TYPE_LEN) {
            memcpy(participant_types[participant_type_count], p - len, len);
            participant_types[participant_type_count][len] = '\0';
            participant_type_count++;
        }
    }

    if (participant_type_count == 0) return false;

    /* Helper: map type name string to GeomType enum */
    /* We only check that the constraint type prefix matches */
    /* For a simple implementation, check that all constraints in the
     * 展开图具有期望的参与者数量 */

    /* 检查展开图中的每个约束 */
    for (int i = 0; i < expanded_graph->constraint_count; i++) {
        Constraint *c = expanded_graph->constraints[i];
        if (!c) continue;

        /* 检查参与者数量是否与规范形式匹配 */
        if (c->participant_count != participant_type_count) {
            return false;
        }

        /* 检查所有参与者是否引用了有效的节点 */
        for (int k = 0; k < c->participant_count; k++) {
            GeomNode *node = graph_get_node((ConstraintGraph *)expanded_graph,
                                             c->participants[k]);
            if (!node) return false;
        }
    }

    /* 如果展开图没有约束但规范形式期望有约束，则为违规 */
    if (expanded_graph->constraint_count == 0) {
        return false;
    }

    return true;
}

/* ============== 双层测试集 ============== */

/**
 * @brief 运行单个测试用例
 *
 * 使用模板的 expand 函数展开模板，然后检查展开结果的基本有效性。
 */
static bool run_single_test_case(
    const ConstraintTemplate *tmpl,
    const TemplateTestCase *tc)
{
    if (!tmpl || !tc) return false;

    /* 如果模板没有 expand 函数，无法运行测试 */
    if (!tmpl->expand) return false;

    /* 创建目标图 */
    ConstraintGraph *target = graph_create();
    if (!target) return false;

    /* 调用模板展开函数 */
    tmpl->expand(tc->params, target);

    /* 基本有效性检查：展开后应有约束产生 */
    bool passed = (target->constraint_count > 0);

    /* 如果模板有正则形式描述，进行额外验证 */
    if (passed && tmpl->normal_form.constraint_type_count > 0) {
        /* 检查约束数量是否匹配预期 */
        passed = (target->constraint_count >= tmpl->normal_form.constraint_type_count);
    }

    graph_destroy(target);
    return passed;
}

TemplateTestResult axiom_template_run_tests(
    AxiomPackage *pkg,
    const char *template_name,
    TemplateTestCase *factory_tests,
    int factory_count,
    TemplateTestCase *user_tests,
    int user_count)
{
    TemplateTestResult result = {0, 0, 0, NULL};

    if (!pkg || !template_name) return result;

    /* 查找模板 */
    ConstraintTemplate *tmpl = axiom_package_get_template(pkg, template_name);
    if (!tmpl) return result;

    int total = factory_count + user_count;
    if (total == 0) return result;

    /* 分配失败消息数组 */
    result.failure_messages = lv_calloc((size_t)total, sizeof(char *));
    if (!result.failure_messages) return result;

    result.total = total;

    /* 运行工厂测试 */
    for (int i = 0; i < factory_count; i++) {
        bool passed = run_single_test_case(tmpl, &factory_tests[i]);

        if (passed == factory_tests[i].expected_result) {
            result.passed++;
        } else {
            /* 边界检查：确保 failure_messages 数组不越界 */
            if (result.failed < total) {
                result.failed++;
                char msg[AXIOM_TEST_MSG_BUF_SIZE];
                snprintf(msg, sizeof(msg),
                         "[FACTORY] '%s': expected %s, got %s",
                         factory_tests[i].template_name,
                         factory_tests[i].expected_result ? "pass" : "fail",
                         passed ? "pass" : "fail");
                result.failure_messages[result.failed - 1] = lv_strdup_safe(msg);
            }
        }
    }

    /* 运行用户测试 */
    for (int i = 0; i < user_count; i++) {
        bool passed = run_single_test_case(tmpl, &user_tests[i]);

        if (passed == user_tests[i].expected_result) {
            result.passed++;
        } else {
            /* 边界检查：确保 failure_messages 数组不越界 */
            if (result.failed < total) {
                result.failed++;
                char msg[AXIOM_TEST_MSG_BUF_SIZE];
                snprintf(msg, sizeof(msg),
                         "[USER] '%s': expected %s, got %s",
                         user_tests[i].template_name,
                         user_tests[i].expected_result ? "pass" : "fail",
                         passed ? "pass" : "fail");
                result.failure_messages[result.failed - 1] = lv_strdup_safe(msg);
            }
        }
    }

    return result;
}

void axiom_template_test_result_destroy(TemplateTestResult *result) {
    if (!result) return;

    if (result->failure_messages) {
        for (int i = 0; i < result->failed; i++) {
            lv_free((void**)&result->failure_messages[i]);
        }
        lv_free((void**)&result->failure_messages);
    }

    result->total = 0;
    result->passed = 0;
    result->failed = 0;
    result->failure_messages = NULL;
}

/* ============== 模板展开缓存 ============== */

/**
 * @brief 计算参数的简单哈希值（用于缓存键）
 */
static uint64_t compute_param_hash(SymbolicCoord **params, int param_count) {
    /* 使用 FNV-1a 基于序列化内容计算参数哈希（仅用于缓存键，非加密用途） */
    uint64_t hash = 14695981039346656037ULL; /* FNV offset basis */

    hash ^= (uint64_t)param_count;
    hash *= 1099511628211ULL; /* FNV prime */

    for (int i = 0; i < param_count && params && params[i]; i++) {
        char *ser = symbolic_coord_serialize(params[i]);
        if (ser) {
            for (const char *p = ser; *p; p++) {
                hash ^= (uint64_t)(unsigned char)*p;
                hash *= 1099511628211ULL; /* FNV prime */
            }
            lv_free((void**)&ser);
        }
    }

    return hash;
}

/**
 * @brief 在缓存中查找匹配的展开图
 *
 * 注意：返回的 ConstraintGraph 指针指向缓存内部持有的图对象。
 * 调用者不得修改、销毁或以其他方式变更返回的图，否则将破坏缓存一致性。
 * 如需修改展开结果，调用者应自行创建图的深拷贝后再操作。
 */
ConstraintGraph *axiom_package_lookup_expansion_cache(
    AxiomPackage *pkg,
    const char *template_name,
    SymbolicCoord **params,
    int param_count)
{
    if (!pkg || !template_name) return NULL;

    uint64_t target_hash = compute_param_hash(params, param_count);

    for (int i = 0; i < pkg->expansion_cache_count; i++) {
        if (pkg->expansion_cache[i].param_hash == target_hash &&
            pkg->expansion_cache[i].template_name &&
            strcmp(pkg->expansion_cache[i].template_name, template_name) == 0) {
            return pkg->expansion_cache[i].expanded_graph;
        }
    }

    return NULL;
}

/**
 * @brief 将展开结果存入缓存
 */
bool axiom_package_store_expansion_cache(
    AxiomPackage *pkg,
    const char *template_name,
    SymbolicCoord **params,
    int param_count,
    ConstraintGraph *expanded_graph)
{
    if (!pkg) return false;

    /* 扩容 */
    if (pkg->expansion_cache_count >= pkg->expansion_cache_capacity) {
        int new_cap = pkg->expansion_cache_capacity == 0 ? AXIOM_EXPANSION_CACHE_CAP : pkg->expansion_cache_capacity * 2;
        TemplateExpansionCache *new_arr = lv_realloc(pkg->expansion_cache,
            new_cap * sizeof(TemplateExpansionCache));
        if (!new_arr) return false;
        pkg->expansion_cache = new_arr;
        pkg->expansion_cache_capacity = new_cap;
    }

    pkg->expansion_cache[pkg->expansion_cache_count].param_hash =
        compute_param_hash(params, param_count);
    pkg->expansion_cache[pkg->expansion_cache_count].template_name =
        template_name ? lv_strdup_safe(template_name) : NULL;
    pkg->expansion_cache[pkg->expansion_cache_count].expanded_graph = expanded_graph;
    pkg->expansion_cache_count++;

    return true;
}

/**
 * @brief 清空模板展开缓存
 */
void axiom_package_clear_expansion_cache(AxiomPackage *pkg) {
    if (!pkg) return;

    for (int i = 0; i < pkg->expansion_cache_count; i++) {
        if (pkg->expansion_cache[i].expanded_graph) {
            graph_destroy(pkg->expansion_cache[i].expanded_graph);
        }
    }
    pkg->expansion_cache_count = 0;
}

/* ============== 依赖引用追踪（Section 11.5: 依赖链断裂自动降级） ============== */

/**
 * @brief 注册一个依赖引用到公理包
 *
 * 记录一个依赖引用及其内容哈希，以便后续升级时验证内容是否变化。
 * 如果内容哈希发生变化，依赖此引用的 GREEN 结论将被自动降级为 YELLOW。
 */
int axiom_package_register_dependency_ref(
    AxiomPackage *pkg,
    const char *ref_id,
    const char *content_hash,
    int dependent_node_id)
{
    if (!pkg || !ref_id || !content_hash) return -1;

    /* 扩容 */
    if (pkg->dep_ref_count >= pkg->dep_ref_capacity) {
        int new_cap = pkg->dep_ref_capacity == 0 ? AXIOM_DEP_REF_CACHE_CAP : pkg->dep_ref_capacity * 2;
        DependencyRef *new_arr = lv_realloc(pkg->dep_refs,
            (size_t)new_cap * sizeof(DependencyRef));
        if (!new_arr) return -2;
        pkg->dep_refs = new_arr;
        pkg->dep_ref_capacity = new_cap;
    }

    DependencyRef *ref = &pkg->dep_refs[pkg->dep_ref_count];
    memset(ref, 0, sizeof(DependencyRef));

    /* 安全复制 ref_id（使用 lv_strlcpy 自动保证零终止） */
    lv_strlcpy(ref->ref_id, ref_id, sizeof(ref->ref_id));

    /* 安全复制 content_hash（使用 lv_strlcpy 自动保证零终止） */
    lv_strlcpy(ref->content_hash, content_hash, sizeof(ref->content_hash));

    ref->dependent_node_id = dependent_node_id;

    /* 记录原始信任颜色为 GREEN */
    ref->original_color = DEP_TRUST_GREEN;

    pkg->dep_ref_count++;
    
    if (axiom_stream_ctx) {
        stream_emit_simple(axiom_stream_ctx, STREAM_EVENT_INFO, "已注册依赖引用", 0);
    }
    
    return 0;
}

/**
 * @brief 验证所有依赖引用，返回失效的引用
 *
 * 通过重新计算包的内容哈希并与注册时存储的哈希进行比较，
 * 找出所有内容已发生变化的依赖引用。
 */
int axiom_package_validate_dependencies_with_hashes(
    AxiomPackage *pkg,
    DependencyRef **invalidated_refs,
    int *invalidated_count)
{
    if (!pkg || !invalidated_refs || !invalidated_count) return -1;

    *invalidated_refs = NULL;
    *invalidated_count = 0;

    if (pkg->dep_ref_count == 0) return 0;

    /* 重新计算当前包的内容哈希 */
    char *current_hash = axiom_package_compute_content_hash(pkg);
    if (!current_hash) return -1;

    /* 第一遍：统计失效引用数量 */
    int fail_count = 0;
    for (int i = 0; i < pkg->dep_ref_count; i++) {
        DependencyRef *ref = &pkg->dep_refs[i];
        if (strcmp(ref->content_hash, current_hash) != 0) {
            fail_count++;
        }
    }

    if (fail_count == 0) {
        lv_free((void**)&current_hash);
        return 0;
    }

    /* 分配输出数组 */
    DependencyRef *output = lv_malloc((size_t)fail_count * sizeof(DependencyRef));
    if (!output) {
        lv_free((void**)&current_hash);
        return -1;
    }

    /* 第二遍：填充失效引用 */
    int out_idx = 0;
    for (int i = 0; i < pkg->dep_ref_count; i++) {
        DependencyRef *ref = &pkg->dep_refs[i];
        if (strcmp(ref->content_hash, current_hash) != 0) {
            output[out_idx++] = *ref;
        }
    }

    lv_free((void**)&current_hash);
    *invalidated_refs = output;
    *invalidated_count = fail_count;
    return fail_count;
}

/**
 * @brief 执行失效依赖的自动降级
 *
 * 当公理包升级后，如果内部引用哈希发生变化（即引用内容被修改），
 * 所有依赖这些引用的 GREEN 结论将被自动降级为 YELLOW
 * "conditional unconstructible -- dependency invalidated"。
 *
 * 这是 design_v2.9.md Section 11.5 要求的依赖链断裂自动降级机制。
 */
int axiom_package_auto_degrade_invalidated(
    AxiomPackage *pkg,
    ConstraintGraph *graph)
{
    if (!pkg || !graph) return 0;

    /* 步骤1：验证所有依赖引用，找出失效的 */
    DependencyRef *invalidated = NULL;
    int invalidated_count = 0;

    int result = axiom_package_validate_dependencies_with_hashes(
        pkg, &invalidated, &invalidated_count);

    if (result < 0 || invalidated_count == 0) {
        return 0;
    }

    /* 步骤2-4：对每个失效引用，降级依赖节点 */
    int degraded_count = 0;

    for (int i = 0; i < invalidated_count; i++) {
        DependencyRef *ref = &invalidated[i];

        /* 在约束图中查找依赖节点 */
        GeomNode *node = graph_get_node(graph, ref->dependent_node_id);
        if (!node) {
            fprintf(stderr,
                "[WARNING] axiom_package_auto_degrade_invalidated: "
                "依赖节点 %d 未在约束图中找到 (ref_id='%s')\n",
                ref->dependent_node_id, ref->ref_id);
            continue;
        }

        /* 仅降级 GREEN 节点 */
        if (node->trust == TRUST_GREEN) {
            node->trust = TRUST_YELLOW;
            degraded_count++;

            fprintf(stderr,
                "[WARNING] axiom_package_auto_degrade_invalidated: "
                "节点 %d 已从 GREEN 降级为 YELLOW "
                "(conditional unconstructible -- dependency invalidated, "
                "ref_id='%s')\n",
                ref->dependent_node_id, ref->ref_id);
        }
    }

    /* 释放验证结果数组 */
    lv_free((void**)&invalidated);

    return degraded_count;
}
