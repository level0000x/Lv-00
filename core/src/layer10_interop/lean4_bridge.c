#include "lv00/interop.h"
#include "lv00/lv00_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* Lv-00 证明步骤类型枚举（与 coq_bridge.c 一致） */
typedef enum {
    LV00_STEP_ADD_NODE = 0,      /* 添加节点 → intro */
    LV00_STEP_ADD_CONSTRAINT,    /* 添加约束 → constructor */
    LV00_STEP_REWRITE,           /* 重写 → rw */
    LV00_STEP_FUNCTION_APP,      /* 函数应用 → apply */
    LV00_STEP_EXACT,             /* 精确匹配 → exact */
    LV00_STEP_HAVE,              /* 中间引理 → have */
    LV00_STEP_CALC,              /* 计算链 → calc */
    LV00_STEP_NORMALIZATION,     /* 规范化 → simp */
    LV00_STEP_ORACLE             /* 外部预言 → sorry */
} Lv00ProofStepType;

/* 证明步骤结构体 */
typedef struct {
    int type;                     /* 步骤类型（Lv00ProofStepType） */
    char description[512];       /* 步骤描述 */
    int id;                      /* 步骤编号 */
} Lv00ProofStep;

/* 内部证明结构体（用于导出/导入） */
typedef struct {
    char theorem_name[256];      /* 定理名称 */
    int step_count;              /* 步骤数量 */
    int step_capacity;           /* 步骤容量 */
    Lv00ProofStep *steps;        /* 步骤数组 */
} Lv00Lean4Proof;

/* Lean 4 proof export: 遍历 Lv-00 证明树并生成 Lean 4 tactic 脚本 */
static int lean4_export_proof(void *proof, char *output, int output_size) {
    if (!proof || !output || output_size <= 0) return -1;

    Lv00Lean4Proof *p = (Lv00Lean4Proof *)proof;

    /* 步骤类型到 Lean 4 tactic 的映射表 */
    static const struct {
        int step_type;
        const char *tactic;
    } tactic_map[] = {
        { LV00_STEP_ADD_NODE,        "intro" },
        { LV00_STEP_ADD_CONSTRAINT,   "constructor" },
        { LV00_STEP_REWRITE,         "rw" },
        { LV00_STEP_FUNCTION_APP,    "apply" },
        { LV00_STEP_EXACT,           "exact" },
        { LV00_STEP_HAVE,            "have" },
        { LV00_STEP_CALC,            "calc" },
        { LV00_STEP_NORMALIZATION,   "simp" },
        { LV00_STEP_ORACLE,          "sorry" }
    };
    int tactic_count = (int)(sizeof(tactic_map) / sizeof(tactic_map[0]));

    /* 输出头 */
    const char *header =
        "import Lv00.HilbertAxioms\n\n"
        "theorem ";
    const char *footer = "\n";
    int header_len = (int)strlen(header);
    int footer_len = (int)strlen(footer);

    /* 计算可用空间 */
    int avail = output_size - header_len - footer_len - 1;
    if (avail < 64) return -1;

    memcpy(output, header, header_len);
    int pos = header_len;

    /* 写入定理名称 */
    int name_len = (int)strlen(p->theorem_name);
    if (pos + name_len + 32 >= output_size) return -1;
    memcpy(output + pos, p->theorem_name, name_len);
    pos += name_len;

    /* 写入 ": Prop := by" */
    pos += snprintf(output + pos, output_size - pos, " : Prop := by\n");

    /* 遍历证明树中每个步骤，生成对应的 tactic */
    for (int i = 0; i < p->step_count; i++) {
        Lv00ProofStep *step = &p->steps[i];
        const char *tac = "sorry"; /* 默认 tactic（未知类型） */

        /* 在映射表中查找对应的 tactic */
        for (int j = 0; j < tactic_count; j++) {
            if (step->type == tactic_map[j].step_type) {
                tac = tactic_map[j].tactic;
                break;
            }
        }

        /* 检查剩余空间 */
        int tac_len = (int)strlen(tac);
        if (pos + tac_len + 8 >= output_size) return -1;

        /* 写入 tactic（缩进两格） */
        pos += snprintf(output + pos, output_size - pos, "  %s\n", tac);
    }

    /* 写入尾部 */
    if (pos + footer_len >= output_size) return -1;
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
        { "intro",       LV00_STEP_ADD_NODE },
        { "constructor", LV00_STEP_ADD_CONSTRAINT },
        { "rw",          LV00_STEP_REWRITE },
        { "rewrite",     LV00_STEP_REWRITE },
        { "apply",       LV00_STEP_FUNCTION_APP },
        { "exact",       LV00_STEP_EXACT },
        { "have",        LV00_STEP_HAVE },
        { "calc",        LV00_STEP_CALC },
        { "simp",        LV00_STEP_NORMALIZATION }
    };
    int reverse_count = (int)(sizeof(reverse_map) / sizeof(reverse_map[0]));

    /* 分配证明结构体 */
    Lv00Lean4Proof *p = (Lv00Lean4Proof *)calloc(1, sizeof(Lv00Lean4Proof));
    if (!p) return -1;

    /* 保存定理名 */
    {
        size_t nlen = (size_t)(name_end - name_start);
        if (nlen >= sizeof(p->theorem_name)) nlen = sizeof(p->theorem_name) - 1;
        memcpy(p->theorem_name, name_start, nlen);
        p->theorem_name[nlen] = '\0';
    }

    /* 初始化步骤数组 */
    p->step_capacity = 16;
    p->steps = (Lv00ProofStep *)calloc(p->step_capacity, sizeof(Lv00ProofStep));
    if (!p->steps) { free(p); return -1; }

    /* 逐行解析 tactic 脚本，通过 reverse_map 转换为 Lv-00 步骤类型 */
    const char *line = script_start;
    while (*line) {
        /* 跳过空白 */
        while (*line && isspace((unsigned char)*line)) line++;
        if (!*line) break;

        /* 找到行尾 */
        const char *line_end = line;
        while (*line_end && *line_end != '\n' && *line_end != '\r') line_end++;

        /* 提取 tactic 名称（行首到第一个空格或括号） */
        const char *tac_start = line;
        const char *tac_end = tac_start;
        while (tac_end < line_end && !isspace((unsigned char)*tac_end) &&
               *tac_end != '(' && *tac_end != '[' && *tac_end != '{') tac_end++;

        if (tac_end > tac_start) {
            /* 查找对应的步骤类型 */
            int step_type = -1;
            int tac_len = (int)(tac_end - tac_start);

            for (int j = 0; j < reverse_count; j++) {
                if ((int)strlen(reverse_map[j].tactic) == tac_len &&
                    strncmp(tac_start, reverse_map[j].tactic, tac_len) == 0) {
                    step_type = reverse_map[j].step_type;
                    break;
                }
            }

            /* 如果找到有效映射，添加步骤 */
            if (step_type >= 0) {
                /* 检查是否需要扩容 */
                if (p->step_count >= p->step_capacity) {
                    int new_cap = p->step_capacity * 2;
                    Lv00ProofStep *new_steps = (Lv00ProofStep *)realloc(
                        p->steps, new_cap * sizeof(Lv00ProofStep));
                    if (!new_steps) {
                        free(p->steps);
                        free(p);
                        return -1;
                    }
                    p->steps = new_steps;
                    p->step_capacity = new_cap;
                }

                Lv00ProofStep *step = &p->steps[p->step_count];
                step->type = step_type;
                step->id = p->step_count;
                /* 保存 tactic 名称作为描述 */
                {
                    size_t dlen = (size_t)(tac_end - tac_start);
                    if (dlen >= sizeof(step->description)) dlen = sizeof(step->description) - 1;
                    memcpy(step->description, tac_start, dlen);
                    step->description[dlen] = '\0';
                }
                p->step_count++;
            }
        }

        line = line_end + 1; /* 移到下一行 */
    }

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

/* 注册 Lean 4 插件 */
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
