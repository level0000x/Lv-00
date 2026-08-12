#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/interop.h"
#include "lv/lv_check.h"
#include "lv/lv_internal.h"
#include "lv/lv_json.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_utils.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_xmacro.h"

/**
 * @file opml_codec.c
 * @brief OPML 格式的证明导入/导出编解码器
 *
 * @details 实现 OPML JSON 格式与 Lv-00 内部证明树之间的双向转换。
 *          包含 JSON 转义/解析辅助函数、证明步骤序列化/反序列化、
 *          theory 段（公理/定义）解析、proof 段（步骤）解析，
 *          以及插件注册接口。
 * @author Lv-00 Project
 * @version 1.0.0
 */

/**
 * @brief JSON 字符串转义
 *
 * 将源字符串中的特殊字符（双引号、反斜杠、控制字符）转义为 JSON 兼容形式。
 * 对于小于 0x20 的控制字符，使用 \\uXXXX 格式转义。
 *
 * @param src      源字符串（UTF-8）
 * @param dst      目标缓冲区
 * @param dst_size 目标缓冲区大小
 * @return 写入目标缓冲区的字符数（不含 null 终止符）
 */
static int json_escape_string(const char *src, char *dst, int dst_size) {
    if (!src || !dst || dst_size <= 0)
        return 0;
    /* 直连公共 API lv_str_json_escape（snprintf 截断语义，省略 strbuf 中转） */
    return (int) lv_str_json_escape(src, strlen(src), dst, (size_t) dst_size);
}

/**
 * @brief 完整 JSON 转义（两遍法，堆分配；调用者 lv_free）
 *
 * 与 json_escape_string 的固定缓冲截断语义不同，本函数精确计算转义后长度并分配，
 * 保证字符串被完整转义（不会截断在转义序列中间产生非法 JSON）。
 *
 * @param src      源字符串（UTF-8，可为 NULL）
 * @param src_len  源字符串长度
 * @return 转义后的堆字符串，失败返回 NULL
 */
static char *json_escape_alloc(const char *src, size_t src_len) {
    return lv_str_json_escape_alloc(src, src_len, NULL);
}

/* Lv-00 证明步骤类型枚举（与 lean4_bridge.c 相同；注意 coq_bridge.c 是 8 项子集且含 UNIFY/EX_FALSO，值定义不重叠时勿混用） */
typedef enum {
    lv_STEP_ADD_NODE = 0,   /* 添加节点 */
    lv_STEP_ADD_CONSTRAINT, /* 添加约束 */
    lv_STEP_REWRITE,        /* 重写 */
    lv_STEP_FUNCTION_APP,   /* 函数应用 */
    lv_STEP_EXACT,          /* 精确匹配 */
    lv_STEP_HAVE,           /* 中间引理 */
    lv_STEP_CALC,           /* 计算链 */
    lv_STEP_NORMALIZATION,  /* 规范化 */
    lv_STEP_ORACLE          /* 外部预言 */
} lvProofStepType;

/* 证明步骤结构体 */
typedef struct {
    int type;              /* 步骤类型（lvProofStepType） */
    char *description;     /* 步骤描述（堆分配，消除 512 定长截断） */
    int id;                /* 步骤编号 */
    lvDArray dependencies; /* 依赖的步骤 ID 列表（int，动态，消除 64 上限） */
} lvProofStep;

/* 内部证明结构体（用于导出/导入） */
typedef struct {
    char theorem_name[256]; /* 定理名称 */
    int step_count;         /* 步骤数量 */
    int step_capacity;      /* 步骤容量 */
    lvProofStep *steps;     /* 步骤数组 */
    char *axioms;           /* 公理列表（逗号分隔，动态拼接，消除 lv_PATH_BUF_SIZE 上限） */
} lvOpmlProof;

/**
 * @brief 将步骤类型枚举映射为字符串名称
 *
 * @param type 步骤类型值（lvProofStepType 枚举）
 * @return 对应的字符串名称，未知类型返回 "unknown"
 */
/* ================================================================
 * 枚举 -> 名称 映射表（数据表化，替代 switch）
 * ================================================================ */

#define LV_STEP_TYPE_OUT_X(x) \
    x(lv_STEP_ADD_NODE, "add_node") \
    x(lv_STEP_ADD_CONSTRAINT, "add_constraint") \
    x(lv_STEP_REWRITE, "rewrite") \
    x(lv_STEP_FUNCTION_APP, "function_app") \
    x(lv_STEP_EXACT, "exact") \
    x(lv_STEP_HAVE, "have") \
    x(lv_STEP_CALC, "calc") \
    x(lv_STEP_NORMALIZATION, "normalization") \
    x(lv_STEP_ORACLE, "oracle")

/** @brief step_type_name 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_step_type_name_entries[] = {
    lv_XMACRO_TO_ENUM_TABLE(LV_STEP_TYPE_OUT_X)
};

static const char *step_type_name(int type) {
    return lv_enum_to_str(s_step_type_name_entries, lv_ARRAY_SIZE(s_step_type_name_entries), (int) type, "unknown");
}

/**
 * @brief 将内部证明结构导出为 OPML JSON 格式
 *
 * 将 lvOpmlProof 结构序列化为 OPML 兼容的 JSON 字符串，
 * 包含 metadata、theory（公理列表）和 proof（步骤数组）三个主要段。
 *
 * @param proof      指向 lvOpmlProof 结构的指针
 * @param output     目标缓冲区
 * @param output_size 目标缓冲区大小
 * @return 成功返回 0，失败返回 -1
 */
static int opml_export_proof(void *proof, char *output, int output_size) {
    lv_CHECK_NOT_NULL(proof);
    lv_CHECK_NOT_NULL(output);
    lv_CHECK_ARG(output_size > 0, lv_ERROR_INVALID_PARAM, "invalid output_size");

    lvOpmlProof *p = (lvOpmlProof *) proof;

    /* 使用 lvStrBuf 动态构建 JSON，替代手写 pos 游标 + snprintf 偏移 */
    lvStrBuf sb = {0};

    /* 转义定理名称，防止 JSON 注入 */
    char escaped_name[lv_PATH_BUF_SIZE];
    json_escape_string(p->theorem_name, escaped_name, (int) sizeof(escaped_name));

    /* 写入 JSON 头部 */
    lv_strbuf_printf(&sb,
                    "{\n"
                    "  \"opml_version\": \"1.0.0\",\n"
                    "  \"source_system\": \"lv\",\n"
                    "  \"metadata\": {\n"
                    "    \"title\": \"%s\",\n"
                    "    \"date\": \"2026-06-04\"\n"
                    "  },\n",
                    escaped_name);

    /* 生成 theory 段（包含公理） */
    if (lv_str_nonempty(p->axioms)) {
        lv_strbuf_printf(&sb,
                         "  \"theory\": {\n"
                         "    \"axioms\": [\n");

        /* 逐个输出公理 */
        const char *ax = p->axioms;
        int first_axiom = 1;
        while (*ax) {
            /* 跳过空白 */
            while (*ax && (*ax == ' ' || *ax == ',' || *ax == '\t'))
                ax++;
            if (!*ax)
                break;

            /* 找到公理名结尾 */
            const char *ax_end = ax;
            while (*ax_end && *ax_end != ',' && *ax_end != '\n')
                ax_end++;

            if (ax_end > ax) {
                if (!first_axiom) {
                    lv_strbuf_printf(&sb, ",\n");
                }
                first_axiom = 0;

                /* 写入公理条目（名称经完整 JSON 转义） */
                lv_strbuf_printf(&sb, "      { \"name\": \"");
                int ax_len = (int) (ax_end - ax);
                char *esc_ax = json_escape_alloc(ax, (size_t) ax_len);
                lv_strbuf_printf(&sb, "%s", esc_ax ? esc_ax : "");
                lv_free((void **) &esc_ax);
                lv_strbuf_printf(&sb, "\" }");
            }
            ax = ax_end;
        }

        lv_strbuf_printf(&sb,
                         "\n    ]\n"
                         "  },\n");
    } else {
        lv_strbuf_printf(&sb, "  \"theory\": {},\n");
    }

    /* 生成 proof 段（包含步骤数组） */
    lv_strbuf_printf(&sb,
                     "  \"proof\": {\n"
                     "    \"method\": \"forward_chain\",\n"
                     "    \"steps\": [\n");

    /* 遍历每个步骤，生成 JSON 对象 */
    for (int i = 0; i < p->step_count; i++) {
        lvProofStep *step = &p->steps[i];

        if (i > 0) {
            lv_strbuf_printf(&sb, ",\n");
        }

        lv_strbuf_printf(&sb,
                         "      {\n"
                         "        \"id\": %d,\n"
                         "        \"type\": \"%s\",\n"
                         "        \"description\": \"",
                         step->id, step_type_name(step->type));

        /* 写入描述（完整 JSON 转义：引号/反斜杠/控制字符，替代原先只转义 " 和 \ 的手写循环） */
        const char *desc = step->description ? step->description : "";
        char *esc_desc = json_escape_alloc(desc, strlen(desc));
        lv_strbuf_printf(&sb, "%s", esc_desc ? esc_desc : "");
        lv_free((void **) &esc_desc);

        lv_strbuf_printf(&sb, "\",\n");

        /* 写入依赖列表（动态 lvDArray，无 64 上限） */
        lv_strbuf_printf(&sb, "        \"dependencies\": [");

        for (int d = 0; d < step->dependencies.count; d++) {
            if (d > 0) {
                lv_strbuf_printf(&sb, ", ");
            }
            lv_strbuf_printf(&sb, "%d", *(const int *)lv_darray_get(&step->dependencies, d));
        }

        lv_strbuf_printf(&sb,
                         "]\n"
                         "      }");
    }

    /* 写入 proof 段尾部和 JSON 结尾 */
    lv_strbuf_printf(&sb,
                     "\n    ]\n"
                     "  }\n"
                     "}\n");

    /* 拷贝到调用方缓冲区（lvStrBuf 保证 NUL 结尾），并清理 */
    if (sb.len >= (size_t) output_size)
        lv_RETURN_ERROR(lv_ERROR_IO, "output buffer full during export");
    memcpy(output, lv_strbuf_cstr(&sb), sb.len + 1);
    lv_strbuf_destroy(&sb);
    return 0;
}

/* OPML JSON 导入 —— 解析 OPML JSON 并构建 Lv-00 证明树 */

/**
 * @brief 解析嵌套 JSON 对象并返回结束位置
 *
 * 从 '{' 开始，正确处理嵌套的花括号和字符串转义内容，
 * 返回匹配的 '}' 之后的位置。
 *
 * @param p 指向 '{' 的指针
 * @return 匹配的 '}' 之后的位置指针；不平衡返回 NULL
 */
static const char *json_skip_object(const char *p) {
    /* 复用统一深度扫描（字符串感知，含转义引号） */
    return lv_str_skip_balanced(p, '{', '}');
}

/**
 * @brief 从 theory 段提取公理和定义名称列表
 *
 * 基于统一 JSON 解析器 lvJsonParser（lv/lv_json.h），
 * 替代原手写 json_find_key/json_extract_string 实现。
 * 输出为 lvDArray<char *>（元素为堆分配字符串，调用方负责逐个释放），
 * 无固定数量上限（原 32 上限已消除）。
 */
static void parse_theory_section(const char *theory_json, lvDArray *axioms, lvDArray *definitions) {
    lv_darray_clear(axioms);
    lv_darray_clear(definitions);
    if (!theory_json)
        return;

    /* 查找 "axioms" 键（统一 lv_json_find_key） */
    const char *axioms_val = lv_json_find_key(theory_json, "axioms", 6);
    if (axioms_val && *axioms_val == '[') {
        lvJsonParser p;
        lv_json_parser_init(&p, axioms_val, strlen(axioms_val));
        lv_json_next(&p); /* 跳过 '[' */
        for (;;) {
            char c = lv_json_peek(&p);
            if (c == ']' || c == '\0')
                break;
            if (c == ',') {
                lv_json_next(&p);
                continue;
            }
            if (c == '"') {
                char *s = lv_json_parse_string(&p);
                if (s) {
                    if (lv_darray_push(axioms, &s) < 0) {
                        lv_free((void **) &s);
                        break;
                    }
                }
            } else {
                lv_json_skip_value(&p);
            }
        }
    }

    /* 查找 "definitions" 键（统一 lv_json_find_key） */
    const char *defs_val = lv_json_find_key(theory_json, "definitions", 11);
    if (defs_val && *defs_val == '[') {
        lvJsonParser p;
        lv_json_parser_init(&p, defs_val, strlen(defs_val));
        lv_json_next(&p); /* 跳过 '[' */
        for (;;) {
            char c = lv_json_peek(&p);
            if (c == ']' || c == '\0')
                break;
            if (c == ',') {
                lv_json_next(&p);
                continue;
            }
            if (c == '"') {
                char *s = lv_json_parse_string(&p);
                if (s) {
                    if (lv_darray_push(definitions, &s) < 0) {
                        lv_free((void **) &s);
                        break;
                    }
                }
            } else {
                lv_json_skip_value(&p);
            }
        }
    }
}

/* ── 步骤类型字符串↔枚举 X-macro 列表 ── */

#define LV_STEP_TYPE_X(x) \
    x(lv_STEP_ADD_NODE, "axiom") \
    x(lv_STEP_ADD_CONSTRAINT, "definition") \
    x(lv_STEP_REWRITE, "rewrite") \
    x(lv_STEP_FUNCTION_APP, "apply") \
    x(lv_STEP_EXACT, "exact") \
    x(lv_STEP_HAVE, "have") \
    x(lv_STEP_CALC, "calc") \
    x(lv_STEP_NORMALIZATION, "normalization")

static const lvStrToEnumEntry step_type_map[] = {
    lv_XMACRO_TO_ENUM_TABLE(LV_STEP_TYPE_X)
};

/**
 * @brief 从 proof 段提取证明步骤
 *
 * 基于统一 JSON 解析器 lvJsonParser（lv/lv_json.h），
 * 替代原手写 json_find_key/json_extract_string 实现。
 */
static void parse_proof_steps(const char *proof_json, lvOpmlProof *proof, int max_steps) {
    if (!proof_json || !proof)
        return;

    /* 查找 "steps" 键（统一 lv_json_find_key） */
    const char *steps_val = lv_json_find_key(proof_json, "steps", 5);
    if (!steps_val || *steps_val != '[')
        return;

    lvJsonParser p;
    lv_json_parser_init(&p, steps_val, strlen(steps_val));
    lv_json_next(&p); /* 跳过 '[' */

    while (proof->step_count < max_steps) {
        char c = lv_json_peek(&p);
        if (c == ']' || c == '\0')
            break;
        if (c == ',') {
            lv_json_next(&p);
            continue;
        }
        if (c != '{') {
            lv_json_skip_value(&p);
            continue;
        }

        lv_json_next(&p); /* 跳过 '{' */

        lvProofStep *step = &proof->steps[proof->step_count];
        memset(step, 0, sizeof(lvProofStep));
        step->id = proof->step_count;

        /* 遍历 step 对象字段（键序无关） */
        char *key = NULL;
        while (lv_json_parse_field(&p, &key)) {
            if (strcmp(key, "name") == 0 && lv_json_peek(&p) == '"') {
                char *val = lv_json_parse_string(&p);
                if (val) {
                    /* 描述动态存储，消除 512 定长截断 */
                    lv_free((void **) &step->description);
                    step->description = lv_strdup_safe(val);
                    lv_free((void **) &val);
                }
            } else if (strcmp(key, "type") == 0 && lv_json_peek(&p) == '"') {
                char *val = lv_json_parse_string(&p);
                if (val) {
                    /* 映射类型名到 lvProofStepType */
                    step->type = (int) lv_str_to_enum(step_type_map, 8, val, lv_STEP_ORACLE);
                    lv_free((void **) &val);
                }
            } else {
                lv_json_skip_value(&p);
            }
            lv_free((void **) &key);
        }
        if (lv_json_peek(&p) == '}')
            lv_json_next(&p);

        proof->step_count++;
    }
}

/**
 * @brief OPML JSON 导入
 */
static int opml_import_proof(const char *input, void **proof) {
    lv_CHECK_NOT_NULL(input);
    lv_CHECK_NOT_NULL(proof);

    /* 验证输入包含 OPML 版本头 */
    if (!strstr(input, "opml_version")) {
        lv_RETURN_ERROR(lv_ERROR_PARSE, "missing opml_version in input");
    }

    /* 查找 theory 和 proof 段 */
    const char *theory_start = strstr(input, "\"theory\"");
    const char *proof_start = strstr(input, "\"proof\"");

    /* 分配 lvOpmlProof 结构体 */
    lvOpmlProof *p = (lvOpmlProof *) lv_calloc(1, sizeof(lvOpmlProof));
    if (!p)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "failed to allocate opml proof");

    /* 初始化步骤数组 */
    p->step_capacity = 64;
    p->steps = (lvProofStep *) lv_calloc(p->step_capacity, sizeof(lvProofStep));
    if (!p->steps) {
        lv_free((void **) &p);
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "failed to allocate steps array");
    }

    /* 提取并解析 theory 段 */
    if (theory_start) {
        const char *brace = strchr(theory_start, '{');
        if (brace) {
            const char *end = lv_str_skip_balanced(brace, '{', '}');
            if (!end)
                end = brace + strlen(brace); /* 不平衡：取到字符串末尾 */
            size_t len = (size_t) (end - brace);
            char *theory_buf = (char *) lv_calloc(1, len + 1);
            if (theory_buf) {
                lv_strlcpy_n(theory_buf, len + 1, brace, (size_t) len);

                /* 从 theory 中提取公理和定义名称（动态数组，无 32 上限） */
                lvDArray axiom_names;
                lv_darray_init(&axiom_names, sizeof(char *));
                lvDArray def_names;
                lv_darray_init(&def_names, sizeof(char *));
                parse_theory_section(theory_buf, &axiom_names, &def_names);

                /* 将公理名称写入 axioms 字段（逗号分隔，统一走 lv_strbuf_join 骨架） */
                lvStrBuf ax_sb = {0};
                const char **ax_names = NULL;
                if (axiom_names.count > 0) {
                    ax_names = lv_malloc((size_t) axiom_names.count * sizeof(char *));
                    if (ax_names) {
                        for (int i = 0; i < axiom_names.count; i++)
                            ax_names[i] = *(const char **) lv_darray_get(&axiom_names, i);
                    }
                }
                lv_strbuf_join(&ax_sb, ax_names, (size_t) axiom_names.count, ", ");
                lv_free((void **) &ax_names);
                p->axioms = lv_strdup_safe(lv_strbuf_cstr(&ax_sb));
                lv_strbuf_destroy(&ax_sb);

                /* 释放名称数组（元素为 parse 阶段堆分配的字符串） */
                for (int i = 0; i < axiom_names.count; i++) {
                    lv_free((void **) lv_darray_get(&axiom_names, i));
                }
                lv_darray_free(&axiom_names);
                for (int i = 0; i < def_names.count; i++) {
                    lv_free((void **) lv_darray_get(&def_names, i));
                }
                lv_darray_free(&def_names);

                lv_free((void **) &theory_buf);
            }
        }
    }

    /* 提取并解析 proof 段 */
    if (proof_start) {
        const char *brace = strchr(proof_start, '{');
        if (brace) {
            const char *end = lv_str_skip_balanced(brace, '{', '}');
            if (!end)
                end = brace + strlen(brace); /* 不平衡：取到字符串末尾 */
            size_t len = (size_t) (end - brace);
            char *proof_buf = (char *) lv_calloc(1, len + 1);
            if (proof_buf) {
                lv_strlcpy_n(proof_buf, len + 1, brace, (size_t) len);

                /* 从 proof 中提取步骤 */
                parse_proof_steps(proof_buf, p, p->step_capacity);

                lv_free((void **) &proof_buf);
            }
        }
    }

    /* 查找定理名称（可选：从 "theorem_name" 或 "name" 键，统一 lv_json_find_key） */
    const char *name_val = lv_json_find_key(input, "theorem_name", 13);
    if (!name_val)
        name_val = lv_json_find_key(input, "name", 4);
    if (name_val && *name_val == '"') {
        lvJsonParser tp;
        lv_json_parser_init(&tp, name_val, strlen(name_val));
        char *tname = lv_json_parse_string(&tp);
        if (tname) {
            lv_strlcpy(p->theorem_name, tname, sizeof(p->theorem_name));
            lv_free((void **) &tname);
        }
    }

    *proof = p;
    return 0;
}

/**
 * @brief OPML 输入校验
 */
static int opml_validate(const char *input) {
    if (!input)
        return 0;
    /* 检查 OPML 版本头 */
    return lv_str_contains(input, "opml_version");
}

/**
 * @brief 注册 OPML 互操作插件到管理器
 *
 * 将 OPML 编解码器注册到 lvInteropManager，使其能够处理
 * OPML JSON 格式的证明导入/导出和验证。
 *
 * @param mgr 互操作管理器指针
 * @return 成功返回 0，失败返回 -1
 */
int lv_register_opml_plugin(lvInteropManager *mgr) {
    lv_CHECK_NOT_NULL(mgr);
    lvPlugin plugin;
    memset(&plugin, 0, sizeof(plugin));
    lv_strlcpy(plugin.name, "opml", sizeof(plugin.name));
    lv_strlcpy(plugin.version, "1.0.0", sizeof(plugin.version));
    plugin.system = lv_EXT_JSON;
    plugin.export_proof = opml_export_proof;
    plugin.import_proof = opml_import_proof;
    plugin.validate = opml_validate;
    return lv_interop_register_plugin(mgr, &plugin);
}
