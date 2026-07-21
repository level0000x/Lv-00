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

/* 映射表大小常量 */
#define LEAN4_TACTIC_MAP_COUNT   9
#define LEAN4_REVERSE_MAP_COUNT 35
#define LEAN4_VALID_TACTICS_COUNT 31

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
    int tactic_count = LEAN4_TACTIC_MAP_COUNT;

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

/* 辅助：向证明结构体添加一个步骤，自动处理扩容 */
static int lean4_add_step(Lv00Lean4Proof *p, int step_type,
                           const char *desc, int desc_len) {
    if (!p || step_type < 0) return -1;
    if (p->step_count >= p->step_capacity) {
        int new_cap = p->step_capacity * 2;
        Lv00ProofStep *new_steps = (Lv00ProofStep *)lv00_realloc(
            p->steps, new_cap * sizeof(Lv00ProofStep));
        if (!new_steps) return -1;
        p->steps = new_steps;
        p->step_capacity = new_cap;
    }
    Lv00ProofStep *step = &p->steps[p->step_count];
    step->type = step_type;
    step->id = p->step_count;
    if (desc && desc_len > 0) {
        int copy_len = desc_len;
        if (copy_len >= (int)sizeof(step->description))
            copy_len = (int)sizeof(step->description) - 1;
        memcpy(step->description, desc, copy_len);
        step->description[copy_len] = '\0';
    } else {
        step->description[0] = '\0';
    }
    p->step_count++;
    return 0;
}

/* 辅助：在 tactic 映射表中查找 tactic 名称对应的步骤类型 */
static int lean4_lookup_tactic(const char *name, int name_len) {
    static const struct {
        const char *tactic;
        int step_type;
    } reverse_map[] = {
        { "intro",       LV00_STEP_ADD_NODE },
        { "constructor", LV00_STEP_ADD_CONSTRAINT },
        { "cases",       LV00_STEP_ADD_CONSTRAINT },
        { "induction",   LV00_STEP_ADD_CONSTRAINT },
        { "rw",          LV00_STEP_REWRITE },
        { "rewrite",     LV00_STEP_REWRITE },
        { "apply",       LV00_STEP_FUNCTION_APP },
        { "exact",       LV00_STEP_EXACT },
        { "have",        LV00_STEP_HAVE },
        { "calc",        LV00_STEP_CALC },
        { "simp",        LV00_STEP_NORMALIZATION },
        { "norm",        LV00_STEP_NORMALIZATION },
        { "ring",        LV00_STEP_NORMALIZATION },
        { "linarith",    LV00_STEP_NORMALIZATION },
        { "omega",       LV00_STEP_NORMALIZATION },
        { "rfl",         LV00_STEP_EXACT },
        { "refl",        LV00_STEP_EXACT },
        { "trivial",     LV00_STEP_EXACT },
        { "assumption",  LV00_STEP_EXACT },
        { "sorry",       LV00_STEP_ORACLE },
        { "admit",       LV00_STEP_ORACLE },
        { "funext",      LV00_STEP_FUNCTION_APP },
        { "subst",       LV00_STEP_REWRITE },
        { "conv",        LV00_STEP_REWRITE },
        { "show",        LV00_STEP_HAVE },
        { "obtain",      LV00_STEP_HAVE },
        { "suffices",    LV00_STEP_HAVE },
        { "left",        LV00_STEP_ADD_CONSTRAINT },
        { "right",       LV00_STEP_ADD_CONSTRAINT },
        { "split",       LV00_STEP_ADD_CONSTRAINT },
        { "first",       LV00_STEP_ORACLE },
        { "skip",        LV00_STEP_ORACLE },
        { "done",        LV00_STEP_EXACT },
        { "at",          LV00_STEP_REWRITE },
        { "let",         LV00_STEP_HAVE },
        { "from",        LV00_STEP_FUNCTION_APP },
    };
    int count = LEAN4_REVERSE_MAP_COUNT;
    for (int j = 0; j < count; j++) {
        if ((int)strlen(reverse_map[j].tactic) == name_len &&
            strncmp(name, reverse_map[j].tactic, name_len) == 0) {
            return reverse_map[j].step_type;
        }
    }
    return -1;
}

/* 辅助：提取标识符（字母/数字/下划线/点/单引号） */
static const char *lean4_extract_ident(const char *p, const char **ident_start,
                                        const char **ident_end) {
    while (p && isspace((unsigned char)*p)) p++;
    *ident_start = p;
    while (*p && (isalnum((unsigned char)*p) || *p == '_' || *p == '.' ||
                  *p == '\'' || *p == '!')) p++;
    *ident_end = p;
    return p;
}

/* 辅助：跳过括号内的内容（包括嵌套），返回结束位置 */
static const char *lean4_skip_bracketed(const char *p, char open, char close) {
    if (!p || *p != open) return p;
    int depth = 0;
    for (; *p; p++) {
        if (*p == open) depth++;
        else if (*p == close) { depth--; if (depth == 0) { p++; break; } }
        else if (*p == '"') {
            p++;
            while (*p && *p != '"') { if (*p == '\\' && *(p+1)) p++; p++; }
        }
    }
    return p;
}

/* 递归解析 tactic 脚本，处理嵌套 by 块、match 表达式、. 链 */
static void lean4_parse_tactics(const char *start, const char *end,
                                 Lv00Lean4Proof *p, int base_indent) {
    const char *pos = start;
    while (pos < end) {
        /* 跳过空白和空行 */
        while (pos < end && isspace((unsigned char)*pos)) pos++;
        if (pos >= end) break;

        /* 计算当前缩进级别 */
        int cur_indent = 0;
        while (pos < end && *pos == ' ') { cur_indent++; pos++; }
        if (pos >= end) break;

        /* 跳过注释行 */
        if (*pos == '-' && *(pos + 1) == '-') {
            while (pos < end && *pos != '\n') pos++;
            continue;
        }

        /* 提取 tactic 标识符 */
        const char *ident_start = NULL, *ident_end = NULL;
        pos = lean4_extract_ident(pos, &ident_start, &ident_end);

        if (ident_end <= ident_start) {
            /* 无法识别的 token，跳过到行尾 */
            while (pos < end && *pos != '\n') pos++;
            continue;
        }

        int ident_len = (int)(ident_end - ident_start);

        /* 处理 match 表达式：match h with | ... => ... */
        if (ident_len == 5 && strncmp(ident_start, "match", 5) == 0) {
            lean4_add_step(p, LV00_STEP_ADD_CONSTRAINT, "match", 5);
            /* 跳过 match 目标表达式（到 "with"） */
            const char *with_kw = NULL;
            for (const char *s = pos; s + 5 < end; s++) {
                if (isspace((unsigned char)s[0]) && strncmp(s + 1, "with", 4) == 0 &&
                    isspace((unsigned char)s[5])) {
                    with_kw = s + 6;
                    break;
                }
            }
            if (with_kw) {
                pos = with_kw;
                /* 解析 match 分支：| pattern => tactic */
                while (pos < end) {
                    while (pos < end && isspace((unsigned char)*pos)) pos++;
                    if (pos >= end) break;
                    if (*pos == '|') {
                        pos++;
                        /* 跳过 pattern */
                        const char *arrow = pos;
                        int found_arrow = 0;
                        while (arrow < end) {
                            if (strncmp(arrow, "=>", 2) == 0) {
                                /* 检查 => 前后不是字符串的一部分 */
                                if ((arrow == pos || isspace((unsigned char)*(arrow-1)) ||
                                     *(arrow-1) == ':' || *(arrow-1) == '_') &&
                                    (arrow[2] == ' ' || arrow[2] == '\n')) {
                                    found_arrow = 1;
                                    break;
                                }
                            }
                            arrow++;
                        }
                        if (found_arrow) {
                            pos = arrow + 2;
                            while (pos < end && isspace((unsigned char)*pos)) pos++;
                            /* 检查分支 tactic 是否是嵌套的 by 块 */
                            if (strncmp(pos, "by", 2) == 0 &&
                                (pos[2] == ' ' || pos[2] == '\n' || pos[2] == '\t')) {
                                pos += 2;
                                /* 找到嵌套 by 块的结束（缩进恢复或文件结束） */
                                const char *nested_start = pos;
                                int nested_indent = cur_indent + 2;
                                const char *nested_end = pos;
                                while (nested_end < end) {
                                    const char *nl = strchr(nested_end, '\n');
                                    if (!nl) { nested_end = end; break; }
                                    nested_end = nl + 1;
                                    /* 检查下一行的缩进 */
                                    int next_indent = 0;
                                    const char *check = nested_end;
                                    while (check < end && *check == ' ') { next_indent++; check++; }
                                    if (check >= end) break;
                                    if (*check == '\n' || *check == '\r') continue;
                                    if (next_indent <= cur_indent) {
                                        nested_end = nl;
                                        break;
                                    }
                                }
                                /* 递归解析嵌套 by 块 */
                                lean4_parse_tactics(nested_start, nested_end, p, nested_indent);
                                pos = nested_end;
                            } else {
                                /* 单行 tactic */
                                const char *tac_line_end = pos;
                                while (tac_line_end < end && *tac_line_end != '\n') tac_line_end++;
                                lean4_parse_tactics(pos, tac_line_end, p, cur_indent + 2);
                                pos = tac_line_end;
                            }
                        } else {
                            /* 没找到 =>，跳过到行尾 */
                            while (pos < end && *pos != '\n') pos++;
                        }
                    } else if (*pos == '\n' || *pos == '\r') {
                        pos++;
                    } else {
                        /* match 体中未识别内容，跳过 */
                        while (pos < end && *pos != '\n' && *pos != '|') pos++;
                    }
                }
            }
            continue;
        }

        /* 处理嵌套 by 块：tactic_arg := by ... */
        if (ident_len == 2 && strncmp(ident_start, "by", 2) == 0) {
            /* 找到嵌套 by 块的结束 */
            const char *nested_start = pos;
            const char *nested_end = pos;
            while (nested_end < end) {
                const char *nl = strchr(nested_end, '\n');
                if (!nl) { nested_end = end; break; }
                nested_end = nl + 1;
                int next_indent = 0;
                const char *check = nested_end;
                while (check < end && *check == ' ') { next_indent++; check++; }
                if (check >= end) break;
                if (*check == '\n' || *check == '\r') continue;
                if (next_indent <= cur_indent) {
                    nested_end = nl;
                    break;
                }
            }
            /* 递归解析嵌套 by 块 */
            lean4_parse_tactics(nested_start, nested_end, p, cur_indent + 2);
            pos = nested_end;
            continue;
        }

        /* 查找 tactic 类型 */
        int step_type = lean4_lookup_tactic(ident_start, ident_len);

        /* 处理 . 分隔的 tactic 链（如 "intro h. apply lemma"） */
        if (step_type >= 0) {
            lean4_add_step(p, step_type, ident_start, ident_len);
        }

        /* 跳过当前 tactic 的参数（括号内容） */
        while (pos < end) {
            while (pos < end && isspace((unsigned char)*pos)) pos++;
            if (pos >= end) break;
            if (*pos == '(') {
                pos = lean4_skip_bracketed(pos, '(', ')');
            } else if (*pos == '[') {
                pos = lean4_skip_bracketed(pos, '[', ']');
            } else if (*pos == '{') {
                pos = lean4_skip_bracketed(pos, '{', '}');
            } else if (*pos == '.' && *(pos + 1) != '.') {
                /* tactic 链分隔符：跳过 . 后解析下一个 tactic */
                pos++;
                const char *chain_start = NULL, *chain_end = NULL;
                pos = lean4_extract_ident(pos, &chain_start, &chain_end);
                if (chain_end > chain_start) {
                    int chain_type = lean4_lookup_tactic(chain_start,
                                                          (int)(chain_end - chain_start));
                    if (chain_type >= 0) {
                        lean4_add_step(p, chain_type, chain_start,
                                        (int)(chain_end - chain_start));
                    }
                }
            } else if (*pos == '\n' || *pos == '\r') {
                pos++;
                break;
            } else {
                pos++;
            }
        }
    }
}

static int lean4_import_proof(const char *input, void **proof) {
    if (!input || !proof) return -1;

    /* 验证输入非空 */
    if (strlen(input) == 0) return -1;

    /* 查找 theorem 关键字 */
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
    if (!p->steps) { lv00_free((void **)&(p)); return -1; }

    /* 确定脚本结束位置（到下一个顶层关键字或文件末尾） */
    const char *script_end = script_start + strlen(script_start);

    /* 使用递归解析器处理 tactic 脚本（支持嵌套 by 块、match 表达式、. 链） */
    lean4_parse_tactics(script_start, script_end, p, 0);

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
    int valid_count = LEAN4_VALID_TACTICS_COUNT;

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
        if (!found_valid) return 0;
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
