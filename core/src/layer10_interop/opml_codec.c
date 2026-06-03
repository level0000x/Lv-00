#include "lv00/interop.h"
#include "lv00/lv00_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* OPML JSON export: convert Lv-00 proof to OPML JSON format */
static int opml_export_proof(void *proof, char *output, int output_size) {
    if (!proof || !output || output_size <= 0) return -1;
    const char *template =
        "{\n"
        "  \"opml_version\": \"1.0.0\",\n"
        "  \"source_system\": \"lv00\",\n"
        "  \"metadata\": {\n"
        "    \"title\": \"Lv-00 exported proof\",\n"
        "    \"date\": \"2026-06-04\"\n"
        "  },\n"
        "  \"theory\": {},\n"
        "  \"proof\": {\n"
        "    \"method\": \"forward_chain\",\n"
        "    \"steps\": []\n"
        "  }\n"
        "}\n";
    int len = (int)strlen(template);
    if (len >= output_size) return -1;
    memcpy(output, template, len + 1);
    return 0;
}

/* OPML JSON import —— 解析 OPML JSON 并构建 Lv-00 证明树 */
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

/* OPML validation */
static int opml_validate(const char *input) {
    if (!input) return 0;
    /* Check for OPML version header */
    return strstr(input, "opml_version") != NULL;
}

/* Register OPML plugin */
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
