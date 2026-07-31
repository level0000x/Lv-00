#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/interop.h"
#include "lv/lv_check.h"
#include "lv/lv_internal.h"
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
    lvStrBuf sb = {0};
    lv_str_escape_json(&sb, src, strlen(src));
    size_t n = sb.len;
    if (n >= (size_t) dst_size)
        n = (size_t) dst_size - 1;
    memcpy(dst, lv_strbuf_cstr(&sb), n);
    dst[n] = '\0';
    lv_strbuf_destroy(&sb);
    return (int) n;
}

/* Lv-00 证明步骤类型枚举（与 coq_bridge.c 一致） */
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
    char description[512]; /* 步骤描述 */
    int id;                /* 步骤编号 */
    int dependencies[64];  /* 依赖的步骤 ID 列表 */
    int dep_count;         /* 依赖数量 */
} lvProofStep;

/* 内部证明结构体（用于导出/导入） */
typedef struct {
    char theorem_name[256]; /* 定理名称 */
    int step_count;         /* 步骤数量 */
    int step_capacity;      /* 步骤容量 */
    lvProofStep *steps;     /* 步骤数组 */
    char axioms[lv_PATH_BUF_SIZE];      /* 公理列表（逗号分隔） */
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

/** @brief 枚举值 -> 名称 映射项（表必须按 code 升序排列） */
typedef struct {
    int code;         /**< 枚举值 */
    const char *name; /**< 名称字符串 */
} opml_NameEntry;

/** @brief 二分查找枚举名称（表需按 code 升序） */
static const char *opml_name_lookup(const opml_NameEntry *table, size_t count, int code) {
    size_t lo = 0, hi = count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (table[mid].code == code)
            return table[mid].name;
        if (table[mid].code < code)
            lo = mid + 1;
        else
            hi = mid;
    }
    return NULL;
}

/** @brief step_type_name 名称表（按枚举值升序） */
static const opml_NameEntry s_step_type_name_entries[] = {
    {lv_STEP_ADD_NODE, "add_node"},
    {lv_STEP_ADD_CONSTRAINT, "add_constraint"},
    {lv_STEP_REWRITE, "rewrite"},
    {lv_STEP_FUNCTION_APP, "function_app"},
    {lv_STEP_EXACT, "exact"},
    {lv_STEP_HAVE, "have"},
    {lv_STEP_CALC, "calc"},
    {lv_STEP_NORMALIZATION, "normalization"},
    {lv_STEP_ORACLE, "oracle"},
};

static const char *step_type_name(int type) {
    const char *name = opml_name_lookup(s_step_type_name_entries, lv_ARRAY_SIZE(s_step_type_name_entries), (int) type);
    return name ? name : "unknown";
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

    int pos = 0;

    /* 转义定理名称，防止 JSON 注入 */
    char escaped_name[lv_PATH_BUF_SIZE];
    json_escape_string(p->theorem_name, escaped_name, (int) sizeof(escaped_name));

    /* 写入 JSON 头部 */
    pos += snprintf(output + pos, output_size - pos,
                    "{\n"
                    "  \"opml_version\": \"1.0.0\",\n"
                    "  \"source_system\": \"lv\",\n"
                    "  \"metadata\": {\n"
                    "    \"title\": \"%s\",\n"
                    "    \"date\": \"2026-06-04\"\n"
                    "  },\n",
                    escaped_name);
    if (pos >= output_size)
        lv_RETURN_ERROR(lv_ERROR_IO, "output buffer full during export");

    /* 生成 theory 段（包含公理） */
    if (strlen(p->axioms) > 0) {
        pos += snprintf(output + pos, output_size - pos,
                                "  \"theory\": {\n"
                                "    \"axioms\": [\n");
        if (pos >= output_size)
            lv_RETURN_ERROR(lv_ERROR_IO, "output buffer full during export");

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
                    if (pos >= output_size)
                        lv_RETURN_ERROR(lv_ERROR_IO, "output buffer full during export");
                    pos += snprintf(output + pos, output_size - pos, ",\n");
                }
                first_axiom = 0;

                /* 写入公理条目 */
                if (pos >= output_size)
                    lv_RETURN_ERROR(lv_ERROR_IO, "output buffer full during export");
                pos += snprintf(output + pos, output_size - pos, "      { \"name\": \"");
                int ax_len = (int) (ax_end - ax);
                if (pos + ax_len + 32 < output_size) {
                    memcpy(output + pos, ax, ax_len);
                    pos += ax_len;
                }
                if (pos >= output_size)
                    lv_RETURN_ERROR(lv_ERROR_IO, "output buffer full during export");
                pos += snprintf(output + pos, output_size - pos, "\" }");
            }
            ax = ax_end;
        }

        if (pos >= output_size)
            lv_RETURN_ERROR(lv_ERROR_IO, "output buffer full during export");
        pos += snprintf(output + pos, output_size - pos,
                        "\n    ]\n"
                        "  },\n");
    } else {
        if (pos >= output_size)
            lv_RETURN_ERROR(lv_ERROR_IO, "output buffer full during export");
        pos += snprintf(output + pos, output_size - pos, "  \"theory\": {},\n");
    }

    /* 生成 proof 段（包含步骤数组） */
    if (pos >= output_size)
        lv_RETURN_ERROR(lv_ERROR_IO, "output buffer full during export");
    pos += snprintf(output + pos, output_size - pos,
                    "  \"proof\": {\n"
                    "    \"method\": \"forward_chain\",\n"
                    "    \"steps\": [\n");

    /* 遍历每个步骤，生成 JSON 对象 */
    for (int i = 0; i < p->step_count; i++) {
        lvProofStep *step = &p->steps[i];

        if (i > 0) {
            if (pos >= output_size)
                lv_RETURN_ERROR(lv_ERROR_IO, "output buffer full during export");
            pos += snprintf(output + pos, output_size - pos, ",\n");
        }

        if (pos >= output_size)
            lv_RETURN_ERROR(lv_ERROR_IO, "output buffer full during export");
        pos += snprintf(output + pos, output_size - pos,
                        "      {\n"
                        "        \"id\": %d,\n"
                        "        \"type\": \"%s\",\n"
                        "        \"description\": \"",
                        step->id, step_type_name(step->type));

        /* 写入描述（转义双引号和反斜杠） */
        const char *desc = step->description;
        while (*desc && pos < output_size - 4) {
            if (*desc == '"' || *desc == '\\') {
                output[pos++] = '\\';
                output[pos++] = *desc;
            } else {
                output[pos++] = *desc;
            }
            desc++;
        }

        if (pos >= output_size)
            lv_RETURN_ERROR(lv_ERROR_IO, "output buffer full during export");
        pos += snprintf(output + pos, output_size - pos, "\",\n");

        /* 写入依赖列表 */
        if (pos >= output_size)
            lv_RETURN_ERROR(lv_ERROR_IO, "output buffer full during export");
        pos += snprintf(output + pos, output_size - pos, "        \"dependencies\": [");

        for (int d = 0; d < step->dep_count; d++) {
            if (d > 0) {
                if (pos >= output_size)
                    lv_RETURN_ERROR(lv_ERROR_IO, "output buffer full during export");
                pos += snprintf(output + pos, output_size - pos, ", ");
            }
            if (pos >= output_size)
                lv_RETURN_ERROR(lv_ERROR_IO, "output buffer full during export");
            pos += snprintf(output + pos, output_size - pos, "%d", step->dependencies[d]);
        }

        if (pos >= output_size)
            lv_RETURN_ERROR(lv_ERROR_IO, "output buffer full during export");
        pos += snprintf(output + pos, output_size - pos,
                        "]\n"
                        "      }");
    }

    /* 写入 proof 段尾部和 JSON 结尾 */
    if (pos >= output_size)
        lv_RETURN_ERROR(lv_ERROR_IO, "output buffer full during export");
    pos += snprintf(output + pos, output_size - pos,
                    "\n    ]\n"
                    "  }\n"
                    "}\n");

    /* 确保以 null 结尾 */
    if (pos >= output_size)
        lv_RETURN_ERROR(lv_ERROR_IO, "output buffer full during export");
    output[pos] = '\0';
    return 0;
}

/* OPML JSON 导入 —— 解析 OPML JSON 并构建 Lv-00 证明树 */

/**
 * @brief 跳过 JSON 空白字符
 */
static const char *json_skip_ws(const char *p) {
    while (p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
        p++;
    return p;
}

/**
 * @brief 提取 JSON 字符串值并写入缓冲区
 */
static const char *json_extract_string(const char *p, char *buf, int buf_size) {
    if (!p || *p != '"')
        lv_RETURN_ERROR_NULL(lv_ERROR_PARSE, "expected JSON string");
    p++; /* 跳过开头 " */
    int i = 0;
    while (*p && *p != '"' && i < buf_size - 1) {
        if (*p == '\\' && *(p + 1)) {
            p++; /* 跳过反斜杠 */
            if (*p == 'u' && p[1] && p[2] && p[3] && p[4]) {
                /* \uXXXX: 解析4位十六进制并写入UTF-8 */
                unsigned int cp = 0;
                for (int k = 0; k < 4; k++) {
                    char hc = p[k + 1];
                    cp <<= 4;
                    if (hc >= '0' && hc <= '9')
                        cp |= (unsigned int) (hc - '0');
                    else if (hc >= 'a' && hc <= 'f')
                        cp |= (unsigned int) (hc - 'a' + 10);
                    else if (hc >= 'A' && hc <= 'F')
                        cp |= (unsigned int) (hc - 'A' + 10);
                }
                p += 4; /* 跳过 uXXXX */
                /* BMP 字符直接写UTF-8 (最多3字节) */
                if (cp < 0x80) {
                    buf[i++] = (char) cp;
                } else if (cp < 0x800) {
                    buf[i++] = (char) (0xC0 | (cp >> 6));
                    if (i < buf_size - 1)
                        buf[i++] = (char) (0x80 | (cp & 0x3F));
                } else {
                    buf[i++] = (char) (0xE0 | (cp >> 12));
                    if (i < buf_size - 2) {
                        buf[i++] = (char) (0x80 | ((cp >> 6) & 0x3F));
                        buf[i++] = (char) (0x80 | (cp & 0x3F));
                    }
                }
                continue; /* 跳过下面的 buf[i++] = *p++ */
            }
        }
        buf[i++] = *p++;
    }
    buf[i] = '\0';
    if (*p == '"')
        p++; /* 跳过结尾 " */
    return p;
}

/**
 * @brief 在 JSON 对象中查找指定键的值位置
 */
static const char *json_find_key(const char *obj_start, const char *key) {
    if (!obj_start || !key)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "NULL obj_start or key");
    lvStrBuf sb = {0};
    lv_strbuf_printf(&sb, "\"%s\"", key);
    const char *p = obj_start;
    while (p && *p) {
        p = strstr(p, sb.data);
        if (!p)
            lv_RETURN_ERROR_NULL(lv_ERROR_NOT_FOUND, "key \"%s\" not found", key);
        p += strlen(sb.data);
        p = json_skip_ws(p);
        if (*p == ':') {
            p++;
            p = json_skip_ws(p);
            lv_strbuf_destroy(&sb);
            return p;
        }
    }
    lv_RETURN_ERROR_NULL(lv_ERROR_NOT_FOUND, "key \"%s\" not found in object", key);
}

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
 */
static void parse_theory_section(const char *theory_json, char axioms[][256], int *axiom_count, int max_axioms,
                                 char definitions[][256], int *def_count, int max_defs) {
    *axiom_count = 0;
    *def_count = 0;
    if (!theory_json)
        return;

    /* 查找 "axioms" 键 */
    const char *axioms_val = json_find_key(theory_json, "axioms");
    if (axioms_val && *axioms_val == '[') {
        const char *end = axioms_val;
        while (*axiom_count < max_axioms) {
            end = json_skip_ws(end + 1); /* 跳过 [ 或 , */
            if (*end == ']' || !*end)
                break;
            end = json_skip_ws(end);
            if (*end == '"') {
                end = json_extract_string(end, axioms[*axiom_count], 256);
                if (end)
                    (*axiom_count)++;
            } else {
                end++;
            }
        }
    }

    /* 查找 "definitions" 键 */
    const char *defs_val = json_find_key(theory_json, "definitions");
    if (defs_val && *defs_val == '[') {
        const char *end = defs_val;
        while (*def_count < max_defs) {
            end = json_skip_ws(end + 1);
            if (*end == ']' || !*end)
                break;
            end = json_skip_ws(end);
            if (*end == '"') {
                end = json_extract_string(end, definitions[*def_count], 256);
                if (end)
                    (*def_count)++;
            } else {
                end++;
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
 */
static void parse_proof_steps(const char *proof_json, lvOpmlProof *proof, int max_steps) {
    if (!proof_json || !proof)
        return;

    /* 查找 "steps" 键 */
    const char *steps_val = json_find_key(proof_json, "steps");
    if (!steps_val || *steps_val != '[')
        return;

    const char *p = steps_val + 1; /* 跳过 [ */
    while (p && *p && *p != ']' && proof->step_count < max_steps) {
        p = json_skip_ws(p);
        if (*p != '{') {
            p++;
            continue;
        }

        /* 解析单个 step 对象 */
        const char *step_end = json_skip_object(p);
        const char *name_val = json_find_key(p, "name");
        const char *type_val = json_find_key(p, "type");

        lvProofStep *step = &proof->steps[proof->step_count];
        memset(step, 0, sizeof(lvProofStep));
        step->id = proof->step_count;

        if (name_val && *name_val == '"') {
            json_extract_string(name_val, step->description, sizeof(step->description));
        }
        if (type_val && *type_val == '"') {
            char type_name[64];
            json_extract_string(type_val, type_name, sizeof(type_name));
            /* 映射类型名到 lvProofStepType */
            step->type = (int)lv_str_to_enum(step_type_map, 8, type_name, lv_STEP_ORACLE);
        }

        proof->step_count++;
        p = step_end;
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
                memcpy(theory_buf, brace, len);
                theory_buf[len] = '\0';

                /* 从 theory 中提取公理和定义名称 */
                char axiom_names[32][256];
                char def_names[32][256];
                int axiom_count = 0, def_count = 0;
                parse_theory_section(theory_buf, axiom_names, &axiom_count, 32, def_names, &def_count, 32);

                /* 将公理名称写入 axioms 字段（逗号分隔） */
                int pos = 0;
                for (int i = 0; i < axiom_count && pos < (int) sizeof(p->axioms) - 1; i++) {
                    if (i > 0 && pos < (int) sizeof(p->axioms) - 2) {
                        p->axioms[pos++] = ',';
                        p->axioms[pos++] = ' ';
                    }
                    int slen = (int) strlen(axiom_names[i]);
                    if (pos + slen >= (int) sizeof(p->axioms))
                        slen = (int) sizeof(p->axioms) - pos - 1;
                    memcpy(p->axioms + pos, axiom_names[i], slen);
                    pos += slen;
                }
                p->axioms[pos] = '\0';

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
                memcpy(proof_buf, brace, len);
                proof_buf[len] = '\0';

                /* 从 proof 中提取步骤 */
                parse_proof_steps(proof_buf, p, p->step_capacity);

                lv_free((void **) &proof_buf);
            }
        }
    }

    /* 查找定理名称（可选：从 "theorem_name" 或 "name" 键） */
    const char *name_val = json_find_key(input, "theorem_name");
    if (!name_val)
        name_val = json_find_key(input, "name");
    if (name_val && *name_val == '"') {
        json_extract_string(name_val, p->theorem_name, sizeof(p->theorem_name));
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
    strncpy(plugin.name, "opml", sizeof(plugin.name) - 1);
    plugin.name[sizeof(plugin.name) - 1] = '\0';
    strncpy(plugin.version, "1.0.0", sizeof(plugin.version) - 1);
    plugin.version[sizeof(plugin.version) - 1] = '\0';
    plugin.system = lv_EXT_JSON;
    plugin.export_proof = opml_export_proof;
    plugin.import_proof = opml_import_proof;
    plugin.validate = opml_validate;
    return lv_interop_register_plugin(mgr, &plugin);
}
