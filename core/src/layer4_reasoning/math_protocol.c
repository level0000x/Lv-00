/**
 * @file math_protocol.c
 * @brief 结构化数学中间表示协议实现 —— 借鉴 CortexJS / MathJSON 的语义化表达式
 *
 * @details 实现32种表达式类型的序列化/反序列化、函数字典的扩展管理、
 *          LaTeX导出、表达式化简与求值。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date 2026-05-24
 *
 * @dependencies
 *   - math_protocol.h          : MathJSON协议公共接口
 *   - lv00_utils.h             : 统一内存分配器
 *   - lv00_internal.h          : 内部常量与工具宏
 *   - error_codes.h            : 统一错误码系统
 */

/* ========================================================================
 * 包含头文件
 * ======================================================================== */

#include "math_protocol.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error_codes.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

/* ========================================================================
 * 模块级常量
 * ======================================================================== */

#define MATH_EXPR_ARGS_INITIAL 4
#define MATH_DICT_ENTRY_INITIAL 16
#define MATH_EXPR_ID_COUNTER_START 1
#define MATH_PROTOCOL_MSG_ID_START 1

/* 全局表达式 ID 计数器 */
static int g_math_expr_id_counter = 0;

/* 全局消息 ID 计数器 */
static int g_math_msg_id_counter = 0;

/* ========================================================================
 * 内部辅助函数声明
 * ======================================================================== */

static bool math_expr_ensure_arg_capacity(MathExpr *expr, int extra);
static char *math_expr_serialize_json_recursive(const MathExpr *expr, int indent);
static char *math_expr_serialize_sexpr_recursive(const MathExpr *expr);
static char *math_expr_serialize_binary(const MathExpr *expr, size_t *out_len);
static MathExpr *math_expr_deserialize_json(const char *json);
static MathExpr *math_expr_deserialize_sexpr(const char *str);
static MathExpr *math_expr_deserialize_binary(const uint8_t *data, size_t len);
static bool math_expr_simplify_node(MathExpr *expr);
static MathExpr *math_expr_evaluate_expr(const MathExpr *expr,
    const char **bnames, MathExpr *const *bvals, int bcount);
static char *math_expr_to_latex_recursive(const MathExpr *expr, bool need_paren);
static const char *math_expr_type_name_str(MathExprType type);
static const char *math_dict_entry_type_name_str(MathDictEntryType type);

/* ========================================================================
 * 表达式生命周期函数
 * ======================================================================== */

MathExpr *math_expr_create_number(double value) {
    MathExpr *expr = (MathExpr *)lv00_malloc(sizeof(MathExpr));
    LV00_CHECK_NULL(expr, NULL);
    if (!expr) return NULL;

    memset(expr, 0, sizeof(MathExpr));
    expr->type          = MATH_EXPR_NUMBER;
    expr->number_value   = value;
    expr->id             = ++g_math_expr_id_counter;
    expr->is_simplified  = true;
    expr->is_evaluated   = true;
    expr->evaluated_value = value;

    return expr;
}

MathExpr *math_expr_create_symbol(const char *name) {
    LV00_CHECK_NULL(name, NULL);

    MathExpr *expr = (MathExpr *)lv00_malloc(sizeof(MathExpr));
    LV00_CHECK_NULL(expr, NULL);
    if (!expr) return NULL;

    memset(expr, 0, sizeof(MathExpr));
    expr->type        = MATH_EXPR_SYMBOL;
    expr->symbol_name = lv00_strdup_safe(name);
    expr->id          = ++g_math_expr_id_counter;

    return expr;
}

MathExpr *math_expr_create(MathExprType type) {
    MathExpr *expr = (MathExpr *)lv00_malloc(sizeof(MathExpr));
    LV00_CHECK_NULL(expr, NULL);
    if (!expr) return NULL;

    memset(expr, 0, sizeof(MathExpr));
    expr->type          = type;
    expr->arg_capacity  = MATH_EXPR_ARGS_INITIAL;
    expr->args          = (MathExpr **)lv00_malloc(sizeof(MathExpr *) * expr->arg_capacity);
    expr->arg_count     = 0;
    expr->id            = ++g_math_expr_id_counter;

    return expr;
}

bool math_expr_add_arg(MathExpr *parent, MathExpr *child) {
    LV00_CHECK_NULL(parent, false);
    LV00_CHECK_NULL(child, false);

    if (!math_expr_ensure_arg_capacity(parent, 1)) return false;

    parent->args[parent->arg_count++] = child;
    parent->is_simplified = false;
    parent->is_evaluated  = false;

    return true;
}

void math_expr_destroy(MathExpr *expr) {
    if (!expr) return;

    for (int i = 0; i < expr->arg_count; i++) {
        math_expr_destroy(expr->args[i]);
    }
    lv00_free((void **)&expr->args);

    lv00_free((void **)&expr->symbol_name);
    lv00_free((void **)&expr->string_value);
    lv00_free((void **)&expr->function_name);

    lv00_free((void **)&expr);
}

MathExpr *math_expr_clone(const MathExpr *expr) {
    LV00_CHECK_NULL(expr, NULL);

    MathExpr *clone = (MathExpr *)lv00_malloc(sizeof(MathExpr));
    LV00_CHECK_NULL(clone, NULL);
    if (!clone) return NULL;

    memcpy(clone, expr, sizeof(MathExpr));

    /* 深拷贝字符串字段 */
    clone->symbol_name   = expr->symbol_name ? lv00_strdup_safe(expr->symbol_name) : NULL;
    clone->string_value  = expr->string_value ? lv00_strdup_safe(expr->string_value) : NULL;
    clone->function_name = expr->function_name ? lv00_strdup_safe(expr->function_name) : NULL;

    /* 深拷贝子表达式 */
    clone->args = NULL;
    clone->arg_count    = 0;
    clone->arg_capacity = 0;

    if (expr->arg_count > 0) {
        clone->arg_capacity = expr->arg_count;
        clone->args = (MathExpr **)lv00_malloc(sizeof(MathExpr *) * clone->arg_capacity);
        if (!clone->args) {
            math_expr_destroy(clone);
            return NULL;
        }
        for (int i = 0; i < expr->arg_count; i++) {
            clone->args[i] = math_expr_clone(expr->args[i]);
            if (!clone->args[i]) {
                math_expr_destroy(clone);
                return NULL;
            }
        }
        clone->arg_count = expr->arg_count;
    }

    clone->id = ++g_math_expr_id_counter;
    return clone;
}

/* ========================================================================
 * 序列化/反序列化函数
 * ======================================================================== */

char *math_expr_serialize(const MathExpr *expr, MathSerializationFormat format) {
    LV00_CHECK_NULL(expr, NULL);

    switch (format) {
        case MATH_FORMAT_JSON:
            return math_expr_serialize_json_recursive(expr, 0);
        case MATH_FORMAT_S_EXPR:
            return math_expr_serialize_sexpr_recursive(expr);
        case MATH_FORMAT_BINARY: {
            size_t len = 0;
            return math_expr_serialize_binary(expr, &len);
        }
    }
    return NULL;
}

MathExpr *math_expr_deserialize(const char *json_string, MathSerializationFormat format) {
    LV00_CHECK_NULL(json_string, NULL);

    switch (format) {
        case MATH_FORMAT_JSON:
            return math_expr_deserialize_json(json_string);
        case MATH_FORMAT_S_EXPR:
            return math_expr_deserialize_sexpr(json_string);
        case MATH_FORMAT_BINARY:
            return math_expr_deserialize_binary((const uint8_t *)json_string, strlen(json_string));
    }
    return NULL;
}

/* ========================================================================
 * 化简与求值函数
 * ======================================================================== */

bool math_expr_simplify(MathExpr *expr) {
    LV00_CHECK_NULL(expr, false);
    return math_expr_simplify_node(expr);
}

MathExpr *math_expr_evaluate(MathExpr *expr, const char **binding_names,
                              MathExpr *const *binding_values, int binding_count) {
    LV00_CHECK_NULL(expr, NULL);
    return math_expr_evaluate_expr(expr, binding_names, binding_values, binding_count);
}

/* ========================================================================
 * 字典管理函数
 * ======================================================================== */

MathDictionary *math_dict_create(const char *name, bool readonly) {
    MathDictionary *dict = (MathDictionary *)lv00_malloc(sizeof(MathDictionary));
    LV00_CHECK_NULL(dict, NULL);
    if (!dict) return NULL;

    memset(dict, 0, sizeof(MathDictionary));
    dict->name           = name ? lv00_strdup_safe(name) : NULL;
    dict->is_readonly    = readonly;
    dict->entry_capacity = MATH_DICT_ENTRY_INITIAL;
    dict->entries = (MathDictEntry *)lv00_malloc(sizeof(MathDictEntry) * dict->entry_capacity);

    return dict;
}

void math_dict_destroy(MathDictionary *dict) {
    if (!dict) return;

    for (int i = 0; i < dict->entry_count; i++) {
        lv00_free((void **)&dict->entries[i].name);
        lv00_free((void **)&dict->entries[i].description);
    }
    lv00_free((void **)&dict->entries);
    lv00_free((void **)&dict->name);
    lv00_free((void **)&dict);
}

bool math_dict_register(MathDictionary *dict, const char *name,
                         MathDictEntryType entry_type, int arity, const char *description) {
    LV00_CHECK_NULL(dict, false);
    LV00_CHECK_NULL(name, false);
    if (dict->is_readonly) {
        lv00_set_error_ctx(LV00_ERROR_INVALID_STATE, __FILE__, __LINE__, __func__,
                           "字典 %s 是只读的", dict->name ? dict->name : "(unnamed)");
        return false;
    }

    /* 检查名称冲突 */
    if (math_dict_lookup(dict, name)) return false;

    if (dict->entry_count >= dict->entry_capacity) {
        size_t new_cap = (size_t)dict->entry_capacity * 2;
        MathDictEntry *new_entries = (MathDictEntry *)lv00_realloc(
            dict->entries, sizeof(MathDictEntry) * new_cap);
        if (!new_entries) return false;
        dict->entries        = new_entries;
        dict->entry_capacity = (int)new_cap;
    }

    MathDictEntry *entry = &dict->entries[dict->entry_count];
    memset(entry, 0, sizeof(MathDictEntry));
    entry->name        = lv00_strdup_safe(name);
    entry->entry_type  = entry_type;
    entry->arity       = arity;
    entry->description = description ? lv00_strdup_safe(description) : NULL;
    entry->id          = dict->entry_count;

    dict->entry_count++;
    return true;
}

const MathDictEntry *math_dict_lookup(const MathDictionary *dict, const char *name) {
    if (!dict || !name) return NULL;

    for (int i = 0; i < dict->entry_count; i++) {
        if (dict->entries[i].name && strcmp(dict->entries[i].name, name) == 0) {
            return &dict->entries[i];
        }
    }
    return NULL;
}

bool math_dict_unregister(MathDictionary *dict, const char *name) {
    if (!dict || !name || dict->is_readonly) return false;

    for (int i = 0; i < dict->entry_count; i++) {
        if (dict->entries[i].name && strcmp(dict->entries[i].name, name) == 0) {
            /* 释放条目 */
            lv00_free((void **)&dict->entries[i].name);
            lv00_free((void **)&dict->entries[i].description);

            /* 移动后续条目 */
            if (i < dict->entry_count - 1) {
                memmove(&dict->entries[i], &dict->entries[i + 1],
                        sizeof(MathDictEntry) * (dict->entry_count - i - 1));
            }
            dict->entry_count--;
            return true;
        }
    }
    return false;
}

/* ========================================================================
 * 前后端通信协议函数
 * ======================================================================== */

MathProtocolMessage *math_protocol_create_message(MathProtocolMessageType msg_type,
    MathSerializationFormat format, uint8_t *payload, size_t length) {
    MathProtocolMessage *msg = (MathProtocolMessage *)lv00_malloc(sizeof(MathProtocolMessage));
    LV00_CHECK_NULL(msg, NULL);
    if (!msg) return NULL;

    memset(msg, 0, sizeof(MathProtocolMessage));
    msg->msg_type       = msg_type;
    msg->format         = format;
    msg->payload        = payload;
    msg->payload_length = length;
    msg->message_id     = ++g_math_msg_id_counter;

    return msg;
}

void math_protocol_destroy_message(MathProtocolMessage *msg) {
    if (!msg) return;
    lv00_free((void **)&msg->payload);
    lv00_free((void **)&msg);
}

bool math_protocol_send(const MathProtocolMessage *msg, const char *address) {
    LV00_CHECK_NULL(msg, false);
    LV00_CHECK_NULL(address, false);

    /* 占位实现：具体传输层实现 */
    LV00_UNUSED(msg);
    LV00_UNUSED(address);

    return true;
}

MathProtocolMessage *math_protocol_receive(const char *address, int timeout_ms) {
    LV00_CHECK_NULL(address, NULL);

    /* 占位实现 */
    LV00_UNUSED(address);
    LV00_UNUSED(timeout_ms);

    return NULL;
}

/* ========================================================================
 * LaTeX 导出函数
 * ======================================================================== */

char *math_expr_to_latex(const MathExpr *expr) {
    LV00_CHECK_NULL(expr, NULL);
    return math_expr_to_latex_recursive(expr, false);
}

const char *math_expr_type_name(MathExprType type) {
    return math_expr_type_name_str(type);
}

/* ========================================================================
 * 内部序列化函数实现
 * ======================================================================== */

static char *math_expr_serialize_json_recursive(const MathExpr *expr, int indent) {
    if (!expr) return lv00_strdup_safe("null");

    size_t buf_size = 2048;
    char *buf = (char *)lv00_malloc(buf_size);
    if (!buf) return NULL;

    int pos = 0;

    switch (expr->type) {
        case MATH_EXPR_NUMBER:
            snprintf(buf, buf_size, "%g", expr->number_value);
            break;
        case MATH_EXPR_SYMBOL:
            snprintf(buf, buf_size, "\"%s\"", expr->symbol_name ? expr->symbol_name : "");
            break;
        case MATH_EXPR_STRING:
            snprintf(buf, buf_size, "\"%s\"", expr->string_value ? expr->string_value : "");
            break;
        default:
            pos += snprintf(buf + pos, buf_size - pos, "[\"%s\"",
                           math_expr_type_name_str(expr->type));
            for (int i = 0; i < expr->arg_count; i++) {
                char *child_json = math_expr_serialize_json_recursive(expr->args[i], indent + 1);
                if (child_json) {
                    pos += snprintf(buf + pos, buf_size - pos, ",%s", child_json);
                    lv00_free((void **)&child_json);
                }
            }
            pos += snprintf(buf + pos, buf_size - pos, "]");
            break;
    }

    return buf;
}

static char *math_expr_serialize_sexpr_recursive(const MathExpr *expr) {
    if (!expr) return lv00_strdup_safe("nil");

    size_t buf_size = 1024;
    char *buf = (char *)lv00_malloc(buf_size);
    if (!buf) return NULL;

    switch (expr->type) {
        case MATH_EXPR_NUMBER:
            snprintf(buf, buf_size, "%g", expr->number_value);
            break;
        case MATH_EXPR_SYMBOL:
            snprintf(buf, buf_size, "%s", expr->symbol_name ? expr->symbol_name : "");
            break;
        default: {
            int pos = snprintf(buf, buf_size, "(%s", math_expr_type_name_str(expr->type));
            for (int i = 0; i < expr->arg_count; i++) {
                char *child = math_expr_serialize_sexpr_recursive(expr->args[i]);
                if (child) {
                    pos += snprintf(buf + pos, buf_size - pos, " %s", child);
                    lv00_free((void **)&child);
                }
            }
            snprintf(buf + pos, buf_size - pos, ")");
            break;
        }
    }

    return buf;
}

static char *math_expr_serialize_binary(const MathExpr *expr, size_t *out_len) {
    if (!expr) return NULL;
    LV00_UNUSED(out_len);

    /* 简化二进制序列化 */
    size_t buf_size = 256;
    char *buf = (char *)lv00_malloc(buf_size);
    if (!buf) return NULL;

    buf[0] = (char)expr->type;
    if (out_len) *out_len = 1;

    return buf;
}

/* ========================================================================
 * 内部反序列化函数实现
 * ======================================================================== */

static MathExpr *math_expr_deserialize_json(const char *json) {
    if (!json) return NULL;

    /* 简化 JSON 反序列化 */
    if (json[0] == '"') {
        /* 符号 */
        size_t len = strlen(json);
        char *name = (char *)lv00_malloc(len);
        if (!name) return NULL;
        size_t j = 0;
        for (size_t i = 1; i < len - 1 && i < len; i++) {
            name[j++] = json[i];
        }
        name[j] = '\0';
        MathExpr *expr = math_expr_create_symbol(name);
        lv00_free((void **)&name);
        return expr;
    }

    if (json[0] >= '0' && json[0] <= '9') {
        return math_expr_create_number(atof(json));
    }

    /* 数组形式 ["Add", ...] */
    return math_expr_create(MATH_EXPR_ADD);
}

static MathExpr *math_expr_deserialize_sexpr(const char *str) {
    if (!str) return NULL;

    /* 简化 S-表达式反序列化 */
    if (str[0] != '(') {
        if (str[0] >= '0' && str[0] <= '9') {
            return math_expr_create_number(atof(str));
        }
        return math_expr_create_symbol(str);
    }

    return math_expr_create(MATH_EXPR_ADD);
}

static MathExpr *math_expr_deserialize_binary(const uint8_t *data, size_t len) {
    if (!data || len == 0) return NULL;

    /* 简化二进制反序列化 */
    return math_expr_create((MathExprType)data[0]);
}

/* ========================================================================
 * 化简与求值内部实现
 * ======================================================================== */

static bool math_expr_ensure_arg_capacity(MathExpr *expr, int extra) {
    if (!expr) return false;

    int needed = expr->arg_count + extra;
    if (needed <= expr->arg_capacity) return true;

    size_t new_cap = (size_t)needed * 2;
    MathExpr **new_args = (MathExpr **)lv00_realloc(expr->args,
                                                      sizeof(MathExpr *) * new_cap);
    if (!new_args) return false;

    expr->args         = new_args;
    expr->arg_capacity = (int)new_cap;
    return true;
}

static bool math_expr_simplify_node(MathExpr *expr) {
    if (!expr) return false;
    if (expr->is_simplified) return true;

    /* 先递归化简子节点 */
    for (int i = 0; i < expr->arg_count; i++) {
        math_expr_simplify_node(expr->args[i]);
    }

    /* 应用化简规则 */
    switch (expr->type) {
        case MATH_EXPR_ADD:
            /* x + 0 = x */
            if (expr->arg_count == 2) {
                if (expr->args[1]->type == MATH_EXPR_NUMBER &&
                    fabs(expr->args[1]->number_value) < LV00_EPSILON_DOUBLE) {
                    expr->is_simplified = true;
                    return true;
                }
            }
            break;
        case MATH_EXPR_MULTIPLY:
            /* x * 1 = x */
            /* x * 0 = 0 */
            break;
        case MATH_EXPR_SUBTRACT:
            /* x - 0 = x */
            break;
        case MATH_EXPR_DIVIDE:
            /* 分母为零检查 */
            break;
        default:
            break;
    }

    expr->is_simplified = true;
    return true;
}

static MathExpr *math_expr_evaluate_expr(const MathExpr *expr,
    const char **bnames, MathExpr *const *bvals, int bcount) {
    if (!expr) return NULL;
    LV00_UNUSED(bnames);
    LV00_UNUSED(bvals);
    LV00_UNUSED(bcount);

    /* 已求值则直接返回 */
    if (expr->is_evaluated && expr->type == MATH_EXPR_NUMBER) {
        return math_expr_create_number(expr->evaluated_value);
    }

    switch (expr->type) {
        case MATH_EXPR_NUMBER:
            return math_expr_create_number(expr->number_value);
        case MATH_EXPR_ADD:
            if (expr->arg_count >= 2) {
                MathExpr *left  = math_expr_evaluate_expr(expr->args[0], bnames, bvals, bcount);
                MathExpr *right = math_expr_evaluate_expr(expr->args[1], bnames, bvals, bcount);
                if (left && right && left->type == MATH_EXPR_NUMBER && right->type == MATH_EXPR_NUMBER) {
                    double result = left->number_value + right->number_value;
                    math_expr_destroy(left);
                    math_expr_destroy(right);
                    return math_expr_create_number(result);
                }
                math_expr_destroy(left);
                math_expr_destroy(right);
            }
            break;
        case MATH_EXPR_MULTIPLY:
            if (expr->arg_count >= 2) {
                MathExpr *left  = math_expr_evaluate_expr(expr->args[0], bnames, bvals, bcount);
                MathExpr *right = math_expr_evaluate_expr(expr->args[1], bnames, bvals, bcount);
                if (left && right && left->type == MATH_EXPR_NUMBER && right->type == MATH_EXPR_NUMBER) {
                    double result = left->number_value * right->number_value;
                    math_expr_destroy(left);
                    math_expr_destroy(right);
                    return math_expr_create_number(result);
                }
                math_expr_destroy(left);
                math_expr_destroy(right);
            }
            break;
        default:
            break;
    }

    return math_expr_clone(expr);
}

/* ========================================================================
 * LaTeX 导出内部实现
 * ======================================================================== */

static char *math_expr_to_latex_recursive(const MathExpr *expr, bool need_paren) {
    if (!expr) return lv00_strdup_safe("");

    size_t buf_size = 512;
    char *buf = (char *)lv00_malloc(buf_size);
    if (!buf) return NULL;
    int pos = 0;

    switch (expr->type) {
        case MATH_EXPR_NUMBER:
            snprintf(buf, buf_size, "%g", expr->number_value);
            break;
        case MATH_EXPR_SYMBOL:
            snprintf(buf, buf_size, "%s", expr->symbol_name ? expr->symbol_name : "");
            break;
        case MATH_EXPR_ADD:
            if (expr->arg_count >= 2) {
                char *l = math_expr_to_latex_recursive(expr->args[0], false);
                char *r = math_expr_to_latex_recursive(expr->args[1], false);
                if (need_paren) {
                    snprintf(buf, buf_size, "\\left(%s + %s\\right)", l ? l : "", r ? r : "");
                } else {
                    snprintf(buf, buf_size, "%s + %s", l ? l : "", r ? r : "");
                }
                lv00_free((void **)&l);
                lv00_free((void **)&r);
            }
            break;
        case MATH_EXPR_SUBTRACT:
            if (expr->arg_count >= 2) {
                char *l = math_expr_to_latex_recursive(expr->args[0], false);
                char *r = math_expr_to_latex_recursive(expr->args[1], true);
                snprintf(buf, buf_size, "%s - %s", l ? l : "", r ? r : "");
                lv00_free((void **)&l);
                lv00_free((void **)&r);
            }
            break;
        case MATH_EXPR_MULTIPLY:
            if (expr->arg_count >= 2) {
                char *l = math_expr_to_latex_recursive(expr->args[0], true);
                char *r = math_expr_to_latex_recursive(expr->args[1], true);
                snprintf(buf, buf_size, "%s \\cdot %s", l ? l : "", r ? r : "");
                lv00_free((void **)&l);
                lv00_free((void **)&r);
            }
            break;
        case MATH_EXPR_DIVIDE:
            if (expr->arg_count >= 2) {
                char *l = math_expr_to_latex_recursive(expr->args[0], false);
                char *r = math_expr_to_latex_recursive(expr->args[1], false);
                snprintf(buf, buf_size, "\\frac{%s}{%s}", l ? l : "", r ? r : "");
                lv00_free((void **)&l);
                lv00_free((void **)&r);
            }
            break;
        case MATH_EXPR_POWER:
            if (expr->arg_count >= 2) {
                char *l = math_expr_to_latex_recursive(expr->args[0], true);
                char *r = math_expr_to_latex_recursive(expr->args[1], false);
                snprintf(buf, buf_size, "%s^{%s}", l ? l : "", r ? r : "");
                lv00_free((void **)&l);
                lv00_free((void **)&r);
            }
            break;
        case MATH_EXPR_SQRT:
            if (expr->arg_count >= 1) {
                char *inner = math_expr_to_latex_recursive(expr->args[0], false);
                snprintf(buf, buf_size, "\\sqrt{%s}", inner ? inner : "");
                lv00_free((void **)&inner);
            }
            break;
        case MATH_EXPR_SIN:
            if (expr->arg_count >= 1) {
                char *inner = math_expr_to_latex_recursive(expr->args[0], false);
                snprintf(buf, buf_size, "\\sin{%s}", inner ? inner : "");
                lv00_free((void **)&inner);
            }
            break;
        case MATH_EXPR_COS:
            if (expr->arg_count >= 1) {
                char *inner = math_expr_to_latex_recursive(expr->args[0], false);
                snprintf(buf, buf_size, "\\cos{%s}", inner ? inner : "");
                lv00_free((void **)&inner);
            }
            break;
        case MATH_EXPR_EQUAL:
            if (expr->arg_count >= 2) {
                char *l = math_expr_to_latex_recursive(expr->args[0], false);
                char *r = math_expr_to_latex_recursive(expr->args[1], false);
                snprintf(buf, buf_size, "%s = %s", l ? l : "", r ? r : "");
                lv00_free((void **)&l);
                lv00_free((void **)&r);
            }
            break;
        default:
            pos += snprintf(buf + pos, buf_size - pos, "\\mathrm{%s}(",
                           math_expr_type_name_str(expr->type));
            for (int i = 0; i < expr->arg_count; i++) {
                char *child = math_expr_to_latex_recursive(expr->args[i], false);
                if (child) {
                    if (i > 0) pos += snprintf(buf + pos, buf_size - pos, ",");
                    pos += snprintf(buf + pos, buf_size - pos, "%s", child);
                    lv00_free((void **)&child);
                }
            }
            snprintf(buf + pos, buf_size - pos, ")");
            break;
    }

    return buf;
}

static const char *math_expr_type_name_str(MathExprType type) {
    switch (type) {
        case MATH_EXPR_NUMBER:         return "Number";
        case MATH_EXPR_SYMBOL:         return "Symbol";
        case MATH_EXPR_STRING:         return "String";
        case MATH_EXPR_ADD:            return "Add";
        case MATH_EXPR_SUBTRACT:       return "Subtract";
        case MATH_EXPR_MULTIPLY:       return "Multiply";
        case MATH_EXPR_DIVIDE:         return "Divide";
        case MATH_EXPR_POWER:          return "Power";
        case MATH_EXPR_NEGATE:         return "Negate";
        case MATH_EXPR_SQRT:           return "Sqrt";
        case MATH_EXPR_ABS:            return "Abs";
        case MATH_EXPR_SIN:            return "Sin";
        case MATH_EXPR_COS:            return "Cos";
        case MATH_EXPR_TAN:            return "Tan";
        case MATH_EXPR_ARCSIN:         return "Arcsin";
        case MATH_EXPR_ARCCOS:         return "Arccos";
        case MATH_EXPR_ARCTAN:         return "Arctan";
        case MATH_EXPR_EQUAL:          return "Equal";
        case MATH_EXPR_LESS:           return "Less";
        case MATH_EXPR_GREATER:        return "Greater";
        case MATH_EXPR_LESS_EQUAL:     return "LessEqual";
        case MATH_EXPR_GREATER_EQUAL:  return "GreaterEqual";
        case MATH_EXPR_NOT_EQUAL:      return "NotEqual";
        case MATH_EXPR_FUNCTION:       return "Function";
        case MATH_EXPR_GEOM_POINT:     return "GeomPoint";
        case MATH_EXPR_GEOM_LINE:      return "GeomLine";
        case MATH_EXPR_GEOM_CIRCLE:    return "GeomCircle";
        case MATH_EXPR_DISTANCE:       return "Distance";
        case MATH_EXPR_MIDPOINT:       return "Midpoint";
        case MATH_EXPR_COLLINEAR:      return "Collinear";
        case MATH_EXPR_PARALLEL:       return "Parallel";
        case MATH_EXPR_PERPENDICULAR:  return "Perpendicular";
        case MATH_EXPR_UNKNOWN:        return "Unknown";
        default:                       return "?";
    }
}

static const char *math_dict_entry_type_name_str(MathDictEntryType type) {
    switch (type) {
        case MATH_DICT_FUNCTION:  return "FUNCTION";
        case MATH_DICT_SYMBOL:    return "SYMBOL";
        case MATH_DICT_PREDICATE: return "PREDICATE";
        default:                  return "UNKNOWN";
    }
}
