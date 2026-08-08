/**
 * @file lean4_bridge.c
 * @brief Lean 4 证明互操作桥接实现
 *
 * @details 实现 Lv-00 内部证明表示与 Lean 4 证明脚本之间的双向转换：
 *   1. lean4_export_proof — 将内部证明树导出为 Lean 4 兼容的 .lean 脚本
 *   2. lean4_import_proof — 解析 Lean 4 脚本并构建内部证明树（支持嵌套 by 块、match 表达式、. 链）
 *   3. lean4_validate — 对 Lean 4 输入进行基本语法校验（花括号平衡、关键字检测）
 *
 * 步骤映射通过 36 条反向映射表覆盖常见的 Lean 4 tactic。
 *
 * @author Lv-00 Project
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/interop.h"
#include "lv/lv_check.h"
#include "lv/lv_internal.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_strbuf.h"
#include "lv/interop_bridge_common.h"

#include "lv_utils.h"

/**
 * @brief Lv-00 证明步骤类型枚举（Lean 4 映射版）
 *
 * 将 Lv-00 内部证明步骤映射为 Lean 4 证明策略（tactic）。
 * 相比 Coq 版本增加了 EXACT、HAVE、CALC 三种步骤类型。
 */
typedef enum {
    lv_STEP_ADD_NODE = 0,   /**< 添加节点 → intro */
    lv_STEP_ADD_CONSTRAINT, /**< 添加约束 → constructor */
    lv_STEP_REWRITE,        /**< 重写 → rw */
    lv_STEP_FUNCTION_APP,   /**< 函数应用 → apply */
    lv_STEP_EXACT,          /**< 精确匹配 → exact */
    lv_STEP_HAVE,           /**< 中间引理 → have */
    lv_STEP_CALC,           /**< 计算链 → calc */
    lv_STEP_NORMALIZATION,  /**< 规范化 → simp */
    lv_STEP_ORACLE          /**< 外部预言 → sorry */
} lvProofStepType;

/* lvProofStep 和 lvBridgeProof 定义在 lv/interop_bridge_common.h */

/* 映射表大小常量 */
#define LEAN4_TACTIC_MAP_COUNT 9
#define LEAN4_REVERSE_MAP_COUNT 35

/* Lean 4 proof export: 遍历 Lv-00 证明树并生成 Lean 4 tactic 脚本 */
static int lean4_export_proof(void *proof, char *output, int output_size) {
    lv_CHECK_NOT_NULL(proof);
    lv_CHECK_NOT_NULL(output);
    lv_CHECK_ARG(output_size > 0, lv_ERROR_INVALID_PARAM, "invalid output_size");

    /* 步骤类型到 Lean 4 tactic 的映射表 */
    static const lvBridgeTacticMap tactic_map[] = {
        {lv_STEP_ADD_NODE, "intro"},
        {lv_STEP_ADD_CONSTRAINT, "constructor"},
        {lv_STEP_REWRITE, "rw"},
        {lv_STEP_FUNCTION_APP, "apply"},
        {lv_STEP_EXACT, "exact"},
        {lv_STEP_HAVE, "have"},
        {lv_STEP_CALC, "calc"},
        {lv_STEP_NORMALIZATION, "simp"},
        {lv_STEP_ORACLE, "sorry"}};

    /* 输出头/尾：Lean 4 .lean 语法框架（header/footer 语义与原实现逐字一致） */
    const lvBridgeExportSpec spec = {
        "import lv.HilbertAxioms\n\n"
        "theorem ",
        " : Prop := by\n",
        "\n",
        "  ",
        "\n",
        "sorry",
        tactic_map,
        (int) (sizeof(tactic_map) / sizeof(tactic_map[0])),
    };

    return bridge_export_proof(proof, output, output_size, &spec);
}

/* Lean 4 proof import: 解析 Lean 4 tactic 脚本并转换为 Lv-00 证明树 */

/* 辅助：向证明结构体添加一个步骤，自动处理扩容 */
static int lean4_add_step(lvBridgeProof *p, int step_type, const char *desc, int desc_len) {
    lv_CHECK_NOT_NULL(p);
    lv_CHECK_ARG(step_type >= 0, lv_ERROR_INVALID_PARAM, "invalid step type %d", step_type);
    lvProofStep step;
    step.type = step_type;
    step.id = p->steps_da.count;
    if (desc && desc_len > 0) {
        int copy_len = desc_len;
        if (copy_len >= (int) sizeof(step.description))
            copy_len = (int) sizeof(step.description) - 1;
        memcpy(step.description, desc, copy_len);
        step.description[copy_len] = '\0';
    } else {
        step.description[0] = '\0';
    }
    if (lv_darray_push(&p->steps_da, &step) < 0)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "failed to push step");
    return 0;
}

/* 辅助：在 tactic 映射表中查找 tactic 名称对应的步骤类型 */
static int lean4_lookup_tactic(const char *name, int name_len) {
    static const struct {
        const char *tactic;
        int step_type;
    } reverse_map[] = {
        {"intro", lv_STEP_ADD_NODE},
        {"constructor", lv_STEP_ADD_CONSTRAINT},
        {"cases", lv_STEP_ADD_CONSTRAINT},
        {"induction", lv_STEP_ADD_CONSTRAINT},
        {"rw", lv_STEP_REWRITE},
        {"rewrite", lv_STEP_REWRITE},
        {"apply", lv_STEP_FUNCTION_APP},
        {"exact", lv_STEP_EXACT},
        {"have", lv_STEP_HAVE},
        {"calc", lv_STEP_CALC},
        {"simp", lv_STEP_NORMALIZATION},
        {"norm", lv_STEP_NORMALIZATION},
        {"ring", lv_STEP_NORMALIZATION},
        {"linarith", lv_STEP_NORMALIZATION},
        {"omega", lv_STEP_NORMALIZATION},
        {"rfl", lv_STEP_EXACT},
        {"refl", lv_STEP_EXACT},
        {"trivial", lv_STEP_EXACT},
        {"assumption", lv_STEP_EXACT},
        {"sorry", lv_STEP_ORACLE},
        {"admit", lv_STEP_ORACLE},
        {"funext", lv_STEP_FUNCTION_APP},
        {"subst", lv_STEP_REWRITE},
        {"conv", lv_STEP_REWRITE},
        {"show", lv_STEP_HAVE},
        {"obtain", lv_STEP_HAVE},
        {"suffices", lv_STEP_HAVE},
        {"left", lv_STEP_ADD_CONSTRAINT},
        {"right", lv_STEP_ADD_CONSTRAINT},
        {"split", lv_STEP_ADD_CONSTRAINT},
        {"first", lv_STEP_ORACLE},
        {"skip", lv_STEP_ORACLE},
        {"done", lv_STEP_EXACT},
        {"at", lv_STEP_REWRITE},
        {"let", lv_STEP_HAVE},
        {"from", lv_STEP_FUNCTION_APP},
    };
    int count = LEAN4_REVERSE_MAP_COUNT;
    for (int j = 0; j < count; j++) {
        if ((int) strlen(reverse_map[j].tactic) == name_len && strncmp(name, reverse_map[j].tactic, name_len) == 0) {
            return reverse_map[j].step_type;
        }
    }
    return -1;
}

/* 辅助：提取标识符（字母/数字/下划线/点/单引号） */
static const char *lean4_extract_ident(const char *p, const char **ident_start, const char **ident_end) {
    p = lv_str_ltrim((char *) p); /* lv_str_ltrim 不修改原串 */
    *ident_start = p;
    while (*p && (isalnum((unsigned char) *p) || *p == '_' || *p == '.' || *p == '\'' || *p == '!'))
        p++;
    *ident_end = p;
    return p;
}

/* 辅助：跳过括号内的内容（包括嵌套），返回结束位置 */
static const char *lean4_skip_bracketed(const char *p, char open, char close) {
    /* 复用统一深度扫描（字符串感知，含转义引号） */
    return lv_str_skip_balanced(p, open, close);
}

/* 递归解析 tactic 脚本，处理嵌套 by 块、match 表达式、. 链 */
static void lean4_parse_tactics(const char *start, const char *end, lvBridgeProof *p, int base_indent) {
    const char *pos = start;
    while (pos < end) {
        /* 跳过空白和空行 */
        while (pos < end && isspace((unsigned char) *pos))
            pos++;
        if (pos >= end)
            break;

        /* 计算当前缩进级别 */
        int cur_indent = 0;
        while (pos < end && *pos == ' ') {
            cur_indent++;
            pos++;
        }
        if (pos >= end)
            break;

        /* 跳过注释行 */
        if (*pos == '-' && *(pos + 1) == '-') {
            while (pos < end && *pos != '\n')
                pos++;
            continue;
        }

        /* 提取 tactic 标识符 */
        const char *ident_start = NULL, *ident_end = NULL;
        pos = lean4_extract_ident(pos, &ident_start, &ident_end);

        if (ident_end <= ident_start) {
            /* 无法识别的 token，跳过到行尾 */
            while (pos < end && *pos != '\n')
                pos++;
            continue;
        }

        int ident_len = (int) (ident_end - ident_start);

        /* 处理 match 表达式：match h with | ... => ... */
        if (ident_len == 5 && strncmp(ident_start, "match", 5) == 0) {
            lean4_add_step(p, lv_STEP_ADD_CONSTRAINT, "match", 5);
            /* 跳过 match 目标表达式（到 "with"） */
            const char *with_kw = NULL;
            for (const char *s = pos; s + 5 < end; s++) {
                if (isspace((unsigned char) s[0]) && strncmp(s + 1, "with", 4) == 0 && isspace((unsigned char) s[5])) {
                    with_kw = s + 6;
                    break;
                }
            }
            if (with_kw) {
                pos = with_kw;
                /* 解析 match 分支：| pattern => tactic */
                while (pos < end) {
                    while (pos < end && isspace((unsigned char) *pos))
                        pos++;
                    if (pos >= end)
                        break;
                    if (*pos == '|') {
                        pos++;
                        /* 跳过 pattern */
                        const char *arrow = pos;
                        int found_arrow = 0;
                        while (arrow < end) {
                            if (strncmp(arrow, "=>", 2) == 0) {
                                /* 检查 => 前后不是字符串的一部分 */
                                if ((arrow == pos || isspace((unsigned char) *(arrow - 1)) || *(arrow - 1) == ':' ||
                                     *(arrow - 1) == '_') &&
                                    (arrow[2] == ' ' || arrow[2] == '\n')) {
                                    found_arrow = 1;
                                    break;
                                }
                            }
                            arrow++;
                        }
                        if (found_arrow) {
                            pos = arrow + 2;
                            while (pos < end && isspace((unsigned char) *pos))
                                pos++;
                            /* 检查分支 tactic 是否是嵌套的 by 块 */
                            if (strncmp(pos, "by", 2) == 0 && (pos[2] == ' ' || pos[2] == '\n' || pos[2] == '\t')) {
                                pos += 2;
                                /* 找到嵌套 by 块的结束（缩进恢复或文件结束） */
                                const char *nested_start = pos;
                                int nested_indent = cur_indent + 2;
                                const char *nested_end = pos;
                                while (nested_end < end) {
                                    const char *nl = strchr(nested_end, '\n');
                                    if (!nl) {
                                        nested_end = end;
                                        break;
                                    }
                                    nested_end = nl + 1;
                                    /* 检查下一行的缩进 */
                                    int next_indent = 0;
                                    const char *check = nested_end;
                                    while (check < end && *check == ' ') {
                                        next_indent++;
                                        check++;
                                    }
                                    if (check >= end)
                                        break;
                                    if (*check == '\n' || *check == '\r')
                                        continue;
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
                                while (tac_line_end < end && *tac_line_end != '\n')
                                    tac_line_end++;
                                lean4_parse_tactics(pos, tac_line_end, p, cur_indent + 2);
                                pos = tac_line_end;
                            }
                        } else {
                            /* 没找到 =>，跳过到行尾 */
                            while (pos < end && *pos != '\n')
                                pos++;
                        }
                    } else if (*pos == '\n' || *pos == '\r') {
                        pos++;
                    } else {
                        /* match 体中未识别内容，跳过 */
                        while (pos < end && *pos != '\n' && *pos != '|')
                            pos++;
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
                if (!nl) {
                    nested_end = end;
                    break;
                }
                nested_end = nl + 1;
                int next_indent = 0;
                const char *check = nested_end;
                while (check < end && *check == ' ') {
                    next_indent++;
                    check++;
                }
                if (check >= end)
                    break;
                if (*check == '\n' || *check == '\r')
                    continue;
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
            while (pos < end && isspace((unsigned char) *pos))
                pos++;
            if (pos >= end)
                break;
            if (*pos == '(') {
                pos = lean4_skip_bracketed(pos, '(', ')');
                if (!pos) pos = end; /* 不平衡：终止解析 */
            } else if (*pos == '[') {
                pos = lean4_skip_bracketed(pos, '[', ']');
                if (!pos) pos = end;
            } else if (*pos == '{') {
                pos = lean4_skip_bracketed(pos, '{', '}');
                if (!pos) pos = end;
            } else if (*pos == '.' && *(pos + 1) != '.') {
                /* tactic 链分隔符：跳过 . 后解析下一个 tactic */
                pos++;
                const char *chain_start = NULL, *chain_end = NULL;
                pos = lean4_extract_ident(pos, &chain_start, &chain_end);
                if (chain_end > chain_start) {
                    int chain_type = lean4_lookup_tactic(chain_start, (int) (chain_end - chain_start));
                    if (chain_type >= 0) {
                        lean4_add_step(p, chain_type, chain_start, (int) (chain_end - chain_start));
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
    lv_CHECK_NOT_NULL(input);
    lv_CHECK_NOT_NULL(proof);

    /* 验证输入非空 */
    if (strlen(input) == 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "empty input");

    /* 查找 theorem 关键字 */
    const char *theorem_kw = strstr(input, "theorem");
    if (!theorem_kw)
        lv_RETURN_ERROR(lv_ERROR_PARSE, "missing 'theorem' keyword");

    /* 提取定理名（theorem 后的第一个标识符） */
    const char *name_start = NULL;
    size_t name_len = 0;
    if (bridge_extract_theorem_name(theorem_kw + 7, &name_start, &name_len) != 0)
        lv_RETURN_ERROR(lv_ERROR_PARSE, "empty theorem name");

    /* 提取 tactic 脚本（":= by" 之后的内容） */
    const char *by_kw = strstr(input, ":= by");
    if (!by_kw)
        lv_RETURN_ERROR(lv_ERROR_PARSE, "missing ':= by' keyword");
    const char *script_start = by_kw + 5; /* 跳过 ":= by" */
    script_start = lv_str_ltrim((char *) script_start); /* lv_str_ltrim 不修改原串 */

    /* 分配证明结构体 */
    lvBridgeProof *p = (lvBridgeProof *) lv_calloc(1, sizeof(lvBridgeProof));
    if (!p)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "failed to allocate lean4 proof");

    /* 保存定理名 */
    {
        size_t nlen = name_len;
        if (nlen >= sizeof(p->theorem_name))
            nlen = sizeof(p->theorem_name) - 1;
        memcpy(p->theorem_name, name_start, nlen);
        p->theorem_name[nlen] = '\0';
    }

    /* 初始化步骤动态数组 */
    lv_darray_init(&p->steps_da, sizeof(lvProofStep));
    if (!lv_darray_reserve(&p->steps_da, 16)) {
        lv_free((void **) &(p));
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "failed to reserve steps array");
    }

    /* 确定脚本结束位置（到下一个顶层关键字或文件末尾） */
    const char *script_end = script_start + strlen(script_start);

    /* 使用递归解析器处理 tactic 脚本（支持嵌套 by 块、match 表达式、. 链） */
    lean4_parse_tactics(script_start, script_end, p, 0);

    *proof = p;
    return 0;
}

/* Lean 4 validation: 基本语法校验（花括号平衡、有效 tactic 名、类型签名） */
static int lean4_validate(const char *input) {
    if (!input)
        return 0;

    /* 检查输入非空 */
    if (strlen(input) == 0)
        return 0;

    /* 检查花括号平衡 */
    if (!lv_str_check_balanced(input, '{', '}'))
        return 0; /* 花括号不平衡 */

    /* 检查是否包含 theorem 关键字 */
    if (!strstr(input, "theorem"))
        return 0;

    /* 检查定理声明是否包含类型签名（冒号后跟类型表达式） */
    const char *thm = strstr(input, "theorem");
    if (thm) {
        const char *colon = strchr(thm, ':');
        if (!colon)
            return 0; /* 缺少类型签名 */
        /* 确认冒号后不是 ":="（即确实有类型声明） */
        if (colon[1] == '=' && colon[2] == ' ')
            return 0;
    }

    /* 检查有效 tactic 名称（常见 Lean 4 tactic 列表） */
    static const char *valid_tactics[] = {"intro", "constructor", "rw",       "rewrite",   "apply", "exact", "have",
                                          "calc",  "simp",        "cases",    "induction", "refl",  "rfl",   "trivial",
                                          "sorry", "assumption",  "exact",    "by",        "fun",   "let",   "show",
                                          "from",  "obtain",      "suffices", "at",        "left",  "right", "split",
                                          "first", "skip",        "done",     NULL};

    /* 如果包含 ":= by"，检查 by 后是否有已知 tactic */
    const char *by_kw = strstr(input, ":= by");
    if (by_kw) {
        const char *script = by_kw + 5;
        if (lv_str_match_any(script, valid_tactics) < 0)
            return 0;
    }

    return 1; /* 校验通过 */
}

/* 注册 Lean 4 插件 */
int lv_register_lean4_plugin(lvInteropManager *mgr) {
    /* 插件注册骨架收敛于公共 helper bridge_register */
    return bridge_register(mgr, "lean4", "4.14.0", lv_EXT_LEAN4, lean4_export_proof, lean4_import_proof,
                           lean4_validate);
}
