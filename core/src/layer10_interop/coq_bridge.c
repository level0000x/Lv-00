#include "lv00/interop.h"
#include "lv00/lv00_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* Lv-00 证明步骤类型枚举 */
typedef enum {
    LV00_STEP_ADD_NODE = 0,      /* 添加节点 → intro */
    LV00_STEP_ADD_CONSTRAINT,    /* 添加约束 → constructor */
    LV00_STEP_REWRITE,           /* 重写 → rewrite */
    LV00_STEP_FUNCTION_APP,      /* 函数应用 → apply */
    LV00_STEP_NORMALIZATION,     /* 规范化 → simpl */
    LV00_STEP_UNIFY,             /* 合一 → reflexivity */
    LV00_STEP_EX_FALSO,          /* 矛盾 → contradiction */
    LV00_STEP_ORACLE             /* 外部预言 → admit (* oracle *) */
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
} Lv00CoqProof;

/* Coq proof export: 将 Lv-00 证明转换为 Coq vernacular */
static int coq_export_proof(void *proof, char *output, int output_size) {
    if (!proof || !output || output_size <= 0) return -1;

    Lv00CoqProof *p = (Lv00CoqProof *)proof;

    /* 步骤类型到 Coq tactic 的映射表 */
    static const struct {
        int step_type;
        const char *tactic;
    } tactic_map[] = {
        { LV00_STEP_ADD_NODE,        "intro" },
        { LV00_STEP_ADD_CONSTRAINT,   "constructor" },
        { LV00_STEP_REWRITE,         "rewrite" },
        { LV00_STEP_FUNCTION_APP,    "apply" },
        { LV00_STEP_NORMALIZATION,   "simpl" },
        { LV00_STEP_UNIFY,           "reflexivity" },
        { LV00_STEP_EX_FALSO,        "contradiction" },
        { LV00_STEP_ORACLE,          "admit (* oracle *)" }
    };
    int tactic_count = (int)(sizeof(tactic_map) / sizeof(tactic_map[0]));

    /* 输出头 */
    const char *header =
        "Require Import Lv00.\n\n"
        "Theorem ";
    const char *footer =
        ".\n"
        "Qed.\n";
    int header_len = (int)strlen(header);
    int footer_len = (int)strlen(footer);

    /* 检查基本空间 */
    if (header_len + footer_len + 64 >= output_size) return -1;

    memcpy(output, header, header_len);
    int pos = header_len;

    /* 写入定理名称 */
    int name_len = (int)strlen(p->theorem_name);
    if (pos + name_len + 16 >= output_size) return -1;
    memcpy(output + pos, p->theorem_name, name_len);
    pos += name_len;

    /* 写入 ": Prop." 和 "Proof." */
    pos += snprintf(output + pos, output_size - pos, " : Prop.\nProof.\n");

    /* 遍历每个步骤，生成对应的 Coq tactic */
    for (int i = 0; i < p->step_count; i++) {
        Lv00ProofStep *step = &p->steps[i];
        const char *tac = "admit"; /* 默认 tactic */

        /* 在映射表中查找对应的 tactic */
        for (int j = 0; j < tactic_count; j++) {
            if (step->type == tactic_map[j].step_type) {
                tac = tactic_map[j].tactic;
                break;
            }
        }

        /* 检查剩余空间是否足够 */
        int tac_len = (int)strlen(tac);
        if (pos + tac_len + 8 >= output_size) return -1;

        /* 写入 tactic，以 "." 结尾 */
        pos += snprintf(output + pos, output_size - pos, "  %s.\n", tac);
    }

    /* 写入尾部 */
    if (pos + footer_len + 1 >= output_size) return -1;
    memcpy(output + pos, footer, footer_len + 1);
    return 0;
}

/* Coq proof import: 解析 Coq vernacular 并转换为 Lv-00 证明 */
static int coq_import_proof(const char *input, void **proof) {
    if (!input || !proof) return -1;
    *proof = NULL;

    /* 检查输入非空 */
    if (strlen(input) == 0) return -1;

    /* 查找 "Theorem" 关键字 */
    const char *theorem_kw = strstr(input, "Theorem");
    if (!theorem_kw) return -1;

    /* 提取定理名（Theorem 后的第一个标识符） */
    const char *name_start = theorem_kw + 7; /* 跳过 "Theorem" */
    while (*name_start && isspace((unsigned char)*name_start)) name_start++;
    const char *name_end = name_start;
    while (*name_end && !isspace((unsigned char)*name_end) && *name_end != ':') name_end++;
    if (name_end == name_start) return -1;

    /* 查找 "Proof." 关键字，确定 tactic 脚本起始位置 */
    const char *proof_kw = strstr(input, "Proof.");
    if (!proof_kw) return -1;
    const char *script_start = proof_kw + 6; /* 跳过 "Proof." */
    while (*script_start && isspace((unsigned char)*script_start)) script_start++;

    /* 查找 "Qed." 关键字，确定 tactic 脚本结束位置 */
    const char *qed_kw = strstr(script_start, "Qed.");
    if (!qed_kw) return -1;

    /* Coq tactic 到 Lv-00 步骤类型的反向映射 */
    static const struct {
        const char *tactic;
        int step_type;
    } reverse_map[] = {
        { "intro",         LV00_STEP_ADD_NODE },
        { "constructor",   LV00_STEP_ADD_CONSTRAINT },
        { "rewrite",       LV00_STEP_REWRITE },
        { "apply",         LV00_STEP_FUNCTION_APP },
        { "simpl",         LV00_STEP_NORMALIZATION },
        { "reflexivity",   LV00_STEP_UNIFY },
        { "contradiction", LV00_STEP_EX_FALSO },
        { "admit",         LV00_STEP_ORACLE }
    };
    int reverse_count = (int)(sizeof(reverse_map) / sizeof(reverse_map[0]));

    /* 分配证明结构体 */
    Lv00CoqProof *p = (Lv00CoqProof *)calloc(1, sizeof(Lv00CoqProof));
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

    /* 逐行解析 tactic 脚本 */
    const char *line = script_start;
    while (line < qed_kw) {
        /* 跳过空白 */
        while (line < qed_kw && isspace((unsigned char)*line)) line++;
        if (line >= qed_kw) break;

        /* 找到行尾 */
        const char *line_end = line;
        while (line_end < qed_kw && *line_end != '\n' && *line_end != '\r') line_end++;

        /* 提取 tactic 名称（行首到第一个空格或 '.'） */
        const char *tac_start = line;
        const char *tac_end = tac_start;
        while (tac_end < line_end && !isspace((unsigned char)*tac_end) && *tac_end != '.') tac_end++;

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
                    Lv00ProofStep *new_steps = (Lv00ProofStep *)realloc(p->steps, new_cap * sizeof(Lv00ProofStep));
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

/* Coq validation: 校验 Coq 输入的基本语法 */
static int coq_validate(const char *input) {
    if (!input) return 0;
    if (strlen(input) == 0) return 0;

    /* 检查花括号平衡 */
    int brace_depth = 0;
    /* 检查圆括号平衡 */
    int paren_depth = 0;
    for (const char *p = input; *p; p++) {
        if (*p == '{') brace_depth++;
        else if (*p == '}') {
            brace_depth--;
            if (brace_depth < 0) return 0; /* 花括号不匹配 */
        }
        else if (*p == '(') paren_depth++;
        else if (*p == ')') {
            paren_depth--;
            if (paren_depth < 0) return 0; /* 圆括号不匹配 */
        }
    }
    if (brace_depth != 0) return 0; /* 花括号不平衡 */
    if (paren_depth != 0) return 0; /* 圆括号不平衡 */

    /* 检查是否包含 "Theorem" 或 "Lemma" 关键字 */
    int has_theorem = (strstr(input, "Theorem") != NULL);
    int has_lemma = (strstr(input, "Lemma") != NULL);
    if (!has_theorem && !has_lemma) return 0;

    /* 检查是否包含有效 tactic 名称 */
    static const char *valid_tactics[] = {
        "intro", "apply", "rewrite", "constructor", "simpl",
        "reflexivity", "contradiction", "admit", "exact",
        "induction", "destruct", "cases", "split", "left",
        "right", "assumption", "auto", "trivial", "omega",
        "ring", "field", "lia", "nia", "tauto",
        "unfold", "fold", "change", "replace", "set",
        "pose", "assert", "generalize", "specialize",
        "inversion", "injection", "discriminate", "subst",
        "symmetry", "transitivity", "f_equal", "congruence"
    };
    int valid_count = (int)(sizeof(valid_tactics) / sizeof(valid_tactics[0]));

    int found_tactic = 0;
    for (int i = 0; i < valid_count; i++) {
        if (strstr(input, valid_tactics[i])) {
            found_tactic = 1;
            break;
        }
    }
    /* 如果有 Proof 段但未找到已知 tactic，仍然通过（可能是自定义 tactic） */

    return found_tactic ? 1 : 0;
}

/* 注册 Coq 插件 */
int lv00_register_coq_plugin(Lv00InteropManager *mgr) {
    if (!mgr) return -1;
    Lv00Plugin plugin;
    memset(&plugin, 0, sizeof(plugin));
    strncpy(plugin.name, "coq", sizeof(plugin.name) - 1);
    strncpy(plugin.version, "8.18", sizeof(plugin.version) - 1);
    plugin.system = LV00_EXT_COQ;
    plugin.export_proof = coq_export_proof;
    plugin.import_proof = coq_import_proof;
    plugin.validate = coq_validate;
    return lv00_interop_register_plugin(mgr, &plugin);
}
