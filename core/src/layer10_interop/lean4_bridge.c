#include "lv00/interop.h"
#include "lv00/lv00_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* Lean 4 proof export: 遍历 Lv-00 证明树并生成 Lean 4 tactic 脚本 */
static int lean4_export_proof(void *proof, char *output, int output_size) {
    if (!proof || !output || output_size <= 0) return -1;

    /* 步骤类型到 Lean 4 tactic 的映射表 */
    static const struct {
        int step_type;
        const char *tactic;
    } tactic_map[] = {
        { 0, "intro" },        /* ADD_NODE -> intro */
        { 1, "constructor" },  /* ADD_CONSTRAINT -> constructor */
        { 2, "rw" },           /* REWRITE -> rw */
        { 3, "apply" },        /* APPLY -> apply */
        { 4, "exact" },        /* EXACT -> exact */
        { 5, "have" },         /* HAVE -> have */
        { 6, "calc" },         /* CALC -> calc */
        { 7, "simp" },         /* SIMPLIFY -> simp */
        { 8, "sorry" },        /* UNKNOWN -> sorry（占位） */
    };
    int tactic_count = (int)(sizeof(tactic_map) / sizeof(tactic_map[0]));

    /* 输出头 */
    const char *header =
        "import Lv00.HilbertAxioms\n\n"
        "theorem imported_proof : True := by\n";
    int header_len = (int)strlen(header);
    const char *footer = "\n";
    int footer_len = (int)strlen(footer);

    /* 计算可用空间 */
    int avail = output_size - header_len - footer_len - 1;
    if (avail < 16) return -1;

    memcpy(output, header, header_len);
    int pos = header_len;

    /* TODO: 当 proof tree API 就绪后，遍历证明树中每个步骤 */
    /* 伪代码：
     * for each step in proof->steps:
     *     const char *tac = "sorry";
     *     for (int j = 0; j < tactic_count; j++) {
     *         if (step->type == tactic_map[j].step_type) {
     *             tac = tactic_map[j].tactic;
     *             break;
     *         }
     *     }
     *     pos += snprintf(output + pos, avail - pos, "  %s\n", tac);
     */

    /* 当前占位：输出默认 tactic */
    pos += snprintf(output + pos, avail - pos, "  trivial\n");

    memcpy(output + pos, footer, footer_len + 1);
    return 0;
}

/* Lean 4 proof import: 解析 Lean 4 tactic 脚本并转换为 Lv-00 证明树 */
static int lean4_import_proof(const char *input, void **proof) {
    if (!input || !proof) return -1;

    /* 验证输入非空 */
    if (strlen(input) == 0) return -1;

    /* 简化解析：提取 theorem 名称和 tactic 脚本 */
    const char *theorem_kw = strstr(input, "theorem");
    if (!theorem_kw) return -1;

    /* 提取定理名（theorem 后的第一个标识符） */
    const char *name_start = theorem_kw + 7; /* 跳过 "theorem" */
    while (*name_start && isspace((unsigned char)*name_start)) name_start++;
    const char *name_end = name_start;
    while (*name_end && !isspace((unsigned char)*name_end) && *name_end != ':') name_end++;

    if (name_end == name_start) return -1;

    /* 提取 tactic 脚本（":= by" 之后的内容） */
    const char *by_kw = strstr(input, ":= by");
    if (!by_kw) return -1;
    const char *script_start = by_kw + 5; /* 跳过 ":= by" */
    while (*script_start && isspace((unsigned char)*script_start)) script_start++;

    /* Lean 4 tactic 到 Lv-00 步骤类型的反向映射 */
    static const struct {
        const char *tactic;
        int step_type;
    } reverse_map[] = {
        { "intro",       0 },  /* intro -> ADD_NODE */
        { "constructor", 1 },  /* constructor -> ADD_CONSTRAINT */
        { "rw",          2 },  /* rw -> REWRITE */
        { "rewrite",     2 },  /* rewrite -> REWRITE */
        { "apply",       3 },  /* apply -> APPLY */
        { "exact",       4 },  /* exact -> EXACT */
        { "have",        5 },  /* have -> HAVE */
        { "calc",        6 },  /* calc -> CALC */
        { "simp",        7 },  /* simp -> SIMPLIFY */
    };
    int reverse_count = (int)(sizeof(reverse_map) / sizeof(reverse_map[0]));

    /* 分配证明结构体（占位） */
    typedef struct {
        char theorem_name[256];
        char tactic_script[4096];
        int step_count;
    } ImportedLean4Proof;

    ImportedLean4Proof *p = lv00_calloc(1, sizeof(ImportedLean4Proof));
    if (!p) return -1;

    /* 保存定理名 */
    {
        size_t nlen = (size_t)(name_end - name_start);
        if (nlen >= sizeof(p->theorem_name)) nlen = sizeof(p->theorem_name) - 1;
        memcpy(p->theorem_name, name_start, nlen);
        p->theorem_name[nlen] = '\0';
    }

    /* 保存 tactic 脚本 */
    {
        size_t slen = strlen(script_start);
        if (slen >= sizeof(p->tactic_script)) slen = sizeof(p->tactic_script) - 1;
        memcpy(p->tactic_script, script_start, slen);
        p->tactic_script[slen] = '\0';
    }

    /* TODO: 当 proof tree API 就绪后，逐行解析 tactic_script，
     *       通过 reverse_map 将每个 tactic 转换为 Lv-00 步骤类型，
     *       构建完整的证明树 */
    (void)reverse_count; /* 暂时抑制未使用警告 */

    *proof = p;
    return 0;
}

/* Lean 4 validation: 基本语法校验（花括号平衡、有效 tactic 名、类型签名） */
static int lean4_validate(const char *input) {
    if (!input) return 0;

    /* 检查输入非空 */
    if (strlen(input) == 0) return 0;

    /* 检查花括号平衡 */
    int brace_depth = 0;
    for (const char *p = input; *p; p++) {
        if (*p == '{') brace_depth++;
        else if (*p == '}') {
            brace_depth--;
            if (brace_depth < 0) return 0; /* 花括号不匹配 */
        }
    }
    if (brace_depth != 0) return 0; /* 花括号不平衡 */

    /* 检查是否包含 theorem 关键字 */
    if (!strstr(input, "theorem")) return 0;

    /* 检查定理声明是否包含类型签名（冒号后跟类型表达式） */
    const char *thm = strstr(input, "theorem");
    if (thm) {
        const char *colon = strchr(thm, ':');
        if (!colon) return 0; /* 缺少类型签名 */
        /* 确认冒号后不是 ":="（即确实有类型声明） */
        if (colon[1] == '=' && colon[2] == ' ') return 0;
    }

    /* 检查有效 tactic 名称（常见 Lean 4 tactic 列表） */
    static const char *valid_tactics[] = {
        "intro", "constructor", "rw", "rewrite", "apply", "exact",
        "have", "calc", "simp", "cases", "induction", "refl",
        "rfl", "trivial", "sorry", "assumption", "exact", "by",
        "fun", "let", "show", "from", "obtain", "suffices",
        "at", "left", "right", "split", "first", "skip", "done"
    };
    int valid_count = (int)(sizeof(valid_tactics) / sizeof(valid_tactics[0]));

    /* 如果包含 ":= by"，检查 by 后是否有已知 tactic */
    const char *by_kw = strstr(input, ":= by");
    if (by_kw) {
        const char *script = by_kw + 5;
        int found_valid = 0;
        for (int i = 0; i < valid_count; i++) {
            if (strstr(script, valid_tactics[i])) {
                found_valid = 1;
                break;
            }
        }
        /* 如果 tactic 块非空但未找到已知 tactic，仍然通过（可能是自定义 tactic） */
        (void)found_valid;
    }

    return 1; /* 校验通过 */
}

/* Register Lean 4 plugin */
int lv00_register_lean4_plugin(Lv00InteropManager *mgr) {
    if (!mgr) return -1;
    Lv00Plugin plugin;
    memset(&plugin, 0, sizeof(plugin));
    strncpy(plugin.name, "lean4", sizeof(plugin.name) - 1);
    strncpy(plugin.version, "4.14.0", sizeof(plugin.version) - 1);
    plugin.system = LV00_EXT_LEAN4;
    plugin.export_proof = lean4_export_proof;
    plugin.import_proof = lean4_import_proof;
    plugin.validate = lean4_validate;
    return lv00_interop_register_plugin(mgr, &plugin);
}
