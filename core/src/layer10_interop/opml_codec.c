#include "lv00/interop.h"
#include "lv00/lv00_internal.h"
#include "lv00/lv00_utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* JSON 字符串转义：将原始字符串写入输出缓冲区，转义 " \ 和控制字符 */
static int json_escape_string(const char *src, char *dst, int dst_size) {
    int pos = 0;
    if (!src || !dst || dst_size <= 0) return 0;
    while (*src && pos < dst_size - 6) { /* 保留空间用于 \uXXXX */
        unsigned char c = (unsigned char)*src;
        if (c == '"') {
            dst[pos++] = '\\'; dst[pos++] = '"';
        } else if (c == '\\') {
            dst[pos++] = '\\'; dst[pos++] = '\\';
        } else if (c == '\n') {
            dst[pos++] = '\\'; dst[pos++] = 'n';
        } else if (c == '\r') {
            dst[pos++] = '\\'; dst[pos++] = 'r';
        } else if (c == '\t') {
            dst[pos++] = '\\'; dst[pos++] = 't';
        } else if (c < 0x20) {
            pos += snprintf(dst + pos, dst_size - pos, "\\u%04x", c);
        } else {
            dst[pos++] = c;
        }
        src++;
    }
    dst[pos] = '\0';
    return pos;
}

/* Lv-00 证明步骤类型枚举（与 coq_bridge.c 一致） */
typedef enum {
    LV00_STEP_ADD_NODE = 0,      /* 添加节点 */
    LV00_STEP_ADD_CONSTRAINT,    /* 添加约束 */
    LV00_STEP_REWRITE,           /* 重写 */
    LV00_STEP_FUNCTION_APP,      /* 函数应用 */
    LV00_STEP_EXACT,             /* 精确匹配 */
    LV00_STEP_HAVE,              /* 中间引理 */
    LV00_STEP_CALC,              /* 计算链 */
    LV00_STEP_NORMALIZATION,     /* 规范化 */
    LV00_STEP_ORACLE             /* 外部预言 */
} Lv00ProofStepType;

/* 证明步骤结构体 */
typedef struct {
    int type;                     /* 步骤类型（Lv00ProofStepType） */
    char description[512];       /* 步骤描述 */
    int id;                      /* 步骤编号 */
    int dependencies[64];        /* 依赖的步骤 ID 列表 */
    int dep_count;               /* 依赖数量 */
} Lv00ProofStep;

/* 内部证明结构体（用于导出/导入） */
typedef struct {
    char theorem_name[256];      /* 定理名称 */
    int step_count;              /* 步骤数量 */
    int step_capacity;           /* 步骤容量 */
    Lv00ProofStep *steps;        /* 步骤数组 */
    char axioms[1024];           /* 公理列表（逗号分隔） */
} Lv00OpmlProof;

/* 步骤类型名称映射（用于 JSON 输出） */
static const char *step_type_name(int type) {
    switch (type) {
        case LV00_STEP_ADD_NODE:        return "add_node";
        case LV00_STEP_ADD_CONSTRAINT:  return "add_constraint";
        case LV00_STEP_REWRITE:         return "rewrite";
        case LV00_STEP_FUNCTION_APP:    return "function_app";
        case LV00_STEP_EXACT:           return "exact";
        case LV00_STEP_HAVE:            return "have";
        case LV00_STEP_CALC:            return "calc";
        case LV00_STEP_NORMALIZATION:   return "normalization";
        case LV00_STEP_ORACLE:          return "oracle";
        default:                        return "unknown";
    }
}

/* OPML JSON export: 将 Lv-00 证明转换为 OPML JSON 格式 */
static int opml_export_proof(void *proof, char *output, int output_size) {
    if (!proof || !output || output_size <= 0) return -1;

    Lv00OpmlProof *p = (Lv00OpmlProof *)proof;

    int pos = 0;

    /* 转义定理名称，防止 JSON 注入 */
    char escaped_name[1024];
    json_escape_string(p->theorem_name, escaped_name, (int)sizeof(escaped_name));

    /* 写入 JSON 头部 */
    pos += snprintf(output + pos, output_size - pos,
        "{\n"
        "  \"opml_version\": \"1.0.0\",\n"
        "  \"source_system\": \"lv00\",\n"
        "  \"metadata\": {\n"
        "    \"title\": \"%s\",\n"
        "    \"date\": \"2026-06-04\"\n"
        "  },\n",
        escaped_name);

    /* 生成 theory 段（包含公理） */
    if (strlen(p->axioms) > 0) {
        pos += snprintf(output + pos, output_size - pos,
            "  \"theory\": {\n"
            "    \"axioms\": [\n");

        /* 逐个输出公理 */
        const char *ax = p->axioms;
        int first_axiom = 1;
        while (*ax) {
            /* 跳过空白 */
            while (*ax && (*ax == ' ' || *ax == ',' || *ax == '\t')) ax++;
            if (!*ax) break;

            /* 找到公理名结尾 */
            const char *ax_end = ax;
            while (*ax_end && *ax_end != ',' && *ax_end != '\n') ax_end++;

            if (ax_end > ax) {
                if (!first_axiom) {
                    pos += snprintf(output + pos, output_size - pos, ",\n");
                }
                first_axiom = 0;

                /* 写入公理条目 */
                pos += snprintf(output + pos, output_size - pos, "      { \"name\": \"");
                int ax_len = (int)(ax_end - ax);
                if (pos + ax_len + 32 < output_size) {
                    memcpy(output + pos, ax, ax_len);
                    pos += ax_len;
                }
                pos += snprintf(output + pos, output_size - pos, "\" }");
            }
            ax = ax_end;
        }

        pos += snprintf(output + pos, output_size - pos,
            "\n    ]\n"
            "  },\n");
    } else {
        pos += snprintf(output + pos, output_size - pos,
            "  \"theory\": {},\n");
    }

    /* 生成 proof 段（包含步骤数组） */
    pos += snprintf(output + pos, output_size - pos,
        "  \"proof\": {\n"
        "    \"method\": \"forward_chain\",\n"
        "    \"steps\": [\n");

    /* 遍历每个步骤，生成 JSON 对象 */
    for (int i = 0; i < p->step_count; i++) {
        Lv00ProofStep *step = &p->steps[i];

        if (i > 0) {
            pos += snprintf(output + pos, output_size - pos, ",\n");
        }

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

        pos += snprintf(output + pos, output_size - pos, "\",\n");

        /* 写入依赖列表 */
        pos += snprintf(output + pos, output_size - pos,
            "        \"dependencies\": [");

        for (int d = 0; d < step->dep_count; d++) {
            if (d > 0) {
                pos += snprintf(output + pos, output_size - pos, ", ");
            }
            pos += snprintf(output + pos, output_size - pos, "%d", step->dependencies[d]);
        }

        pos += snprintf(output + pos, output_size - pos, "]\n"
            "      }");
    }

    /* 写入 proof 段尾部和 JSON 结尾 */
    pos += snprintf(output + pos, output_size - pos,
        "\n    ]\n"
        "  }\n"
        "}\n");

    /* 确保以 null 结尾 */
    if (pos >= output_size) return -1;
    output[pos] = '\0';
    return 0;
}

/* OPML JSON 导入 —— 解析 OPML JSON 并构建 Lv-00 证明树 */

/* 辅助：跳过 JSON 空白字符 */
static const char *json_skip_ws(const char *p) {
    while (p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    return p;
}

/* 辅助：提取 JSON 字符串值（从 " 开始到下一个 "），返回值写入 buf */
static const char *json_extract_string(const char *p, char *buf, int buf_size) {
    if (!p || *p != '"') return NULL;
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
                    if (hc >= '0' && hc <= '9') cp |= (unsigned int)(hc - '0');
                    else if (hc >= 'a' && hc <= 'f') cp |= (unsigned int)(hc - 'a' + 10);
                    else if (hc >= 'A' && hc <= 'F') cp |= (unsigned int)(hc - 'A' + 10);
                }
                p += 4; /* 跳过 uXXXX */
                /* BMP 字符直接写UTF-8 (最多3字节) */
                if (cp < 0x80) {
                    buf[i++] = (char)cp;
                } else if (cp < 0x800) {
                    buf[i++] = (char)(0xC0 | (cp >> 6));
                    if (i < buf_size - 1) buf[i++] = (char)(0x80 | (cp & 0x3F));
                } else {
                    buf[i++] = (char)(0xE0 | (cp >> 12));
                    if (i < buf_size - 2) {
                        buf[i++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                        buf[i++] = (char)(0x80 | (cp & 0x3F));
                    }
                }
                continue; /* 跳过下面的 buf[i++] = *p++ */
            }
        }
        buf[i++] = *p++;
    }
    buf[i] = '\0';
    if (*p == '"') p++; /* 跳过结尾 " */
    return p;
}

/* 辅助：在 JSON 对象中查找指定键，返回键对应的值位置 */
static const char *json_find_key(const char *obj_start, const char *key) {
    if (!obj_start || !key) return NULL;
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = obj_start;
    while (p && *p) {
        p = strstr(p, search);
        if (!p) return NULL;
        p += strlen(search);
        p = json_skip_ws(p);
        if (*p == ':') {
            p++;
            p = json_skip_ws(p);
            return p;
        }
    }
    return NULL;
}

/* 辅助：匹配 JSON 字符串值（与给定 literal 比较） */
static int json_match_string(const char *p, const char *literal) {
    if (!p || *p != '"') return 0;
    p++;
    size_t len = strlen(literal);
    if (strncmp(p, literal, len) != 0) return 0;
    if (p[len] != '"') return 0;
    return 1;
}

/* 辅助：提取 JSON 数组中的所有字符串值 */
static int json_extract_string_array(const char *arr_start, char **out, int max_count, int max_len) {
    if (!arr_start || *arr_start != '[') return 0;
    arr_start++; /* 跳过 [ */
    int count = 0;
    const char *p = arr_start;
    while (p && *p && *p != ']' && count < max_count) {
        p = json_skip_ws(p);
        if (*p == '"') {
            out[count] = (char *)lv00_calloc(1, max_len);
            if (!out[count]) break;
            p = json_extract_string(p, out[count], max_len);
            if (p) count++;
        } else {
            p++;
        }
    }
    return count;
}

/* 辅助：解析嵌套 JSON 对象（花括号匹配），返回对象结束后的位置 */
static const char *json_skip_object(const char *p) {
    if (!p || *p != '{') return p;
    int depth = 0;
    for (; *p; p++) {
        if (*p == '{') depth++;
        else if (*p == '}') { depth--; if (depth == 0) { p++; break; } }
        else if (*p == '"') {
            p++; /* 跳过字符串内容 */
            while (*p && *p != '"') { if (*p == '\\' && *(p+1)) p++; p++; }
        }
    }
    return p;
}

/* 辅助：解析嵌套 JSON 数组（方括号匹配），返回数组结束后的位置 */
static const char *json_skip_array(const char *p) {
    if (!p || *p != '[') return p;
    int depth = 0;
    for (; *p; p++) {
        if (*p == '[') depth++;
        else if (*p == ']') { depth--; if (depth == 0) { p++; break; } }
        else if (*p == '"') {
            p++;
            while (*p && *p != '"') { if (*p == '\\' && *(p+1)) p++; p++; }
        }
    }
    return p;
}

/* 辅助：从 theory JSON 中提取 axiom 名称列表和 definition 名称列表 */
static void parse_theory_section(const char *theory_json,
                                  char axioms[][256], int *axiom_count, int max_axioms,
                                  char definitions[][256], int *def_count, int max_defs) {
    *axiom_count = 0;
    *def_count = 0;
    if (!theory_json) return;

    /* 查找 "axioms" 键 */
    const char *axioms_val = json_find_key(theory_json, "axioms");
    if (axioms_val && *axioms_val == '[') {
        const char *end = axioms_val;
        while (*axiom_count < max_axioms) {
            end = json_skip_ws(end + 1); /* 跳过 [ 或 , */
            if (*end == ']' || !*end) break;
            end = json_skip_ws(end);
            if (*end == '"') {
                end = json_extract_string(end, axioms[*axiom_count], 256);
                if (end) (*axiom_count)++;
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
            if (*end == ']' || !*end) break;
            end = json_skip_ws(end);
            if (*end == '"') {
                end = json_extract_string(end, definitions[*def_count], 256);
                if (end) (*def_count)++;
            } else {
                end++;
            }
        }
    }
}

/* 辅助：从 proof JSON 中提取 proof steps（name + type） */
static void parse_proof_steps(const char *proof_json,
                               Lv00OpmlProof *proof, int max_steps) {
    if (!proof_json || !proof) return;

    /* 查找 "steps" 键 */
    const char *steps_val = json_find_key(proof_json, "steps");
    if (!steps_val || *steps_val != '[') return;

    const char *p = steps_val + 1; /* 跳过 [ */
    while (p && *p && *p != ']' && proof->step_count < max_steps) {
        p = json_skip_ws(p);
        if (*p != '{') { p++; continue; }

        /* 解析单个 step 对象 */
        const char *step_end = json_skip_object(p);
        const char *name_val = json_find_key(p, "name");
        const char *type_val = json_find_key(p, "type");

        Lv00ProofStep *step = &proof->steps[proof->step_count];
        memset(step, 0, sizeof(Lv00ProofStep));
        step->id = proof->step_count;

        if (name_val && *name_val == '"') {
            json_extract_string(name_val, step->description, sizeof(step->description));
        }
        if (type_val && *type_val == '"') {
            char type_name[64];
            json_extract_string(type_val, type_name, sizeof(type_name));
            /* 映射类型名到 Lv00ProofStepType */
            if (strcmp(type_name, "axiom") == 0) step->type = LV00_STEP_ADD_NODE;
            else if (strcmp(type_name, "definition") == 0) step->type = LV00_STEP_ADD_CONSTRAINT;
            else if (strcmp(type_name, "rewrite") == 0) step->type = LV00_STEP_REWRITE;
            else if (strcmp(type_name, "apply") == 0) step->type = LV00_STEP_FUNCTION_APP;
            else if (strcmp(type_name, "exact") == 0) step->type = LV00_STEP_EXACT;
            else if (strcmp(type_name, "have") == 0) step->type = LV00_STEP_HAVE;
            else if (strcmp(type_name, "calc") == 0) step->type = LV00_STEP_CALC;
            else if (strcmp(type_name, "normalization") == 0) step->type = LV00_STEP_NORMALIZATION;
            else step->type = LV00_STEP_ORACLE;
        }

        proof->step_count++;
        p = step_end;
    }
}

static int opml_import_proof(const char *input, void **proof) {
    if (!input || !proof) return -1;

    /* 验证输入包含 OPML 版本头 */
    if (!strstr(input, "opml_version")) {
        return -1;
    }

    /* 查找 theory 和 proof 段 */
    const char *theory_start = strstr(input, "\"theory\"");
    const char *proof_start = strstr(input, "\"proof\"");

    /* 分配 Lv00OpmlProof 结构体 */
    Lv00OpmlProof *p = (Lv00OpmlProof *)lv00_calloc(1, sizeof(Lv00OpmlProof));
    if (!p) return -1;

    /* 初始化步骤数组 */
    p->step_capacity = 64;
    p->steps = (Lv00ProofStep *)lv00_calloc(p->step_capacity, sizeof(Lv00ProofStep));
    if (!p->steps) { lv00_free((void **)&p); return -1; }

    /* 提取并解析 theory 段 */
    if (theory_start) {
        const char *brace = strchr(theory_start, '{');
        if (brace) {
            int depth = 0;
            const char *end = brace;
            for (; *end; end++) {
                if (*end == '{') depth++;
                else if (*end == '}') depth--;
                if (depth == 0) { end++; break; }
            }
            size_t len = (size_t)(end - brace);
            char *theory_buf = (char *)lv00_calloc(1, len + 1);
            if (theory_buf) {
                memcpy(theory_buf, brace, len);
                theory_buf[len] = '\0';

                /* 从 theory 中提取公理和定义名称 */
                char axiom_names[32][256];
                char def_names[32][256];
                int axiom_count = 0, def_count = 0;
                parse_theory_section(theory_buf, axiom_names, &axiom_count, 32,
                                      def_names, &def_count, 32);

                /* 将公理名称写入 axioms 字段（逗号分隔） */
                int pos = 0;
                for (int i = 0; i < axiom_count && pos < (int)sizeof(p->axioms) - 1; i++) {
                    if (i > 0 && pos < (int)sizeof(p->axioms) - 2) {
                        p->axioms[pos++] = ',';
                        p->axioms[pos++] = ' ';
                    }
                    int slen = (int)strlen(axiom_names[i]);
                    if (pos + slen >= (int)sizeof(p->axioms)) slen = (int)sizeof(p->axioms) - pos - 1;
                    memcpy(p->axioms + pos, axiom_names[i], slen);
                    pos += slen;
                }
                p->axioms[pos] = '\0';

                lv00_free((void **)&theory_buf);
            }
        }
    }

    /* 提取并解析 proof 段 */
    if (proof_start) {
        const char *brace = strchr(proof_start, '{');
        if (brace) {
            int depth = 0;
            const char *end = brace;
            for (; *end; end++) {
                if (*end == '{') depth++;
                else if (*end == '}') depth--;
                if (depth == 0) { end++; break; }
            }
            size_t len = (size_t)(end - brace);
            char *proof_buf = (char *)lv00_calloc(1, len + 1);
            if (proof_buf) {
                memcpy(proof_buf, brace, len);
                proof_buf[len] = '\0';

                /* 从 proof 中提取步骤 */
                parse_proof_steps(proof_buf, p, p->step_capacity);

                lv00_free((void **)&proof_buf);
            }
        }
    }

    /* 查找定理名称（可选：从 "theorem_name" 或 "name" 键） */
    const char *name_val = json_find_key(input, "theorem_name");
    if (!name_val) name_val = json_find_key(input, "name");
    if (name_val && *name_val == '"') {
        json_extract_string(name_val, p->theorem_name, sizeof(p->theorem_name));
    }

    *proof = p;
    return 0;
}

/* OPML 校验 */
static int opml_validate(const char *input) {
    if (!input) return 0;
    /* 检查 OPML 版本头 */
    return strstr(input, "opml_version") != NULL;
}

/* 注册 OPML 插件 */
int lv00_register_opml_plugin(Lv00InteropManager *mgr) {
    if (!mgr) return -1;
    Lv00Plugin plugin;
    memset(&plugin, 0, sizeof(plugin));
    strncpy(plugin.name, "opml", sizeof(plugin.name) - 1);
    strncpy(plugin.version, "1.0.0", sizeof(plugin.version) - 1);
    plugin.system = LV00_EXT_JSON;
    plugin.export_proof = opml_export_proof;
    plugin.import_proof = opml_import_proof;
    plugin.validate = opml_validate;
    return lv00_interop_register_plugin(mgr, &plugin);
}
