#include "lv00/interop.h"
#include "lv00/lv00_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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

    /* 写入 JSON 头部 */
    pos += snprintf(output + pos, output_size - pos,
        "{\n"
        "  \"opml_version\": \"1.0.0\",\n"
        "  \"source_system\": \"lv00\",\n"
        "  \"metadata\": {\n"
        "    \"title\": \"%s\",\n"
        "    \"date\": \"2026-06-04\"\n"
        "  },\n",
        p->theorem_name);

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
static int opml_import_proof(const char *input, void **proof) {
    if (!input || !proof) return -1;

    /* 验证输入包含 OPML 版本头 */
    if (!strstr(input, "opml_version")) {
        return -1;
    }

    /* 简化 JSON 解析：提取 theory 和 proof 段 */
    const char *theory_start = strstr(input, "\"theory\"");
    const char *proof_start = strstr(input, "\"proof\"");

    /* 分配证明结构体（占位：当 proof tree API 就绪后替换） */
    typedef struct {
        char theory_section[1024];
        char proof_section[2048];
        int has_theory;
        int has_proof;
    } ImportedProof;

    ImportedProof *p = lv00_calloc(1, sizeof(ImportedProof));
    if (!p) return -1;

    /* 提取 theory 段（简化：复制 theory 起始位置附近的内容） */
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
            if (len >= sizeof(p->theory_section)) len = sizeof(p->theory_section) - 1;
            memcpy(p->theory_section, brace, len);
            p->theory_section[len] = '\0';
            p->has_theory = 1;
        }
    }

    /* 提取 proof 段（简化：复制 proof 起始位置附近的内容） */
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
            if (len >= sizeof(p->proof_section)) len = sizeof(p->proof_section) - 1;
            memcpy(p->proof_section, brace, len);
            p->proof_section[len] = '\0';
            p->has_proof = 1;
        }
    }

    /* TODO: 当 proof tree API 就绪后，从 theory_section 和 proof_section
     *       中解析公理、定义、步骤、策略等，构建完整的 Lv-00 证明树 */
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
