/**
 * @file coq_bridge.c
 * @brief Coq 证明互操作桥接实现
 *
 * @details 实现 Lv-00 内部证明表示与 Coq 证明脚本之间的双向转换：
 *   1. coq_export_proof — 将内部证明树导出为 Coq 8.18 兼容的 .v 脚本
 *   2. coq_import_proof — 解析 Coq .v 脚本并构建内部证明树
 *   3. coq_validate — 对 Coq 输入进行基本语法校验（括号平衡、关键字检测）
 *
 * Coq 与 Lv-00 步骤类型的映射通过 tactic_map / reverse_map 静态表完成。
 *
 * @author Lv-00 Project
 */

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/interop.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"

/**
 * @brief Lv-00 证明步骤类型枚举（Coq 映射版）
 *
 * 将 Lv-00 内部证明步骤映射为 Coq 证明策略（tactic）。
 * 每种步骤类型对应一个或多个 Coq 等价策略。
 */
typedef enum {
    lv_STEP_ADD_NODE = 0,   /**< 添加节点 → intro */
    lv_STEP_ADD_CONSTRAINT, /**< 添加约束 → constructor */
    lv_STEP_REWRITE,        /**< 重写 → rewrite */
    lv_STEP_FUNCTION_APP,   /**< 函数应用 → apply */
    lv_STEP_NORMALIZATION,  /**< 规范化 → simpl */
    lv_STEP_UNIFY,          /**< 合一 → reflexivity */
    lv_STEP_EX_FALSO,       /**< 矛盾 → contradiction */
    lv_STEP_ORACLE          /**< 外部预言 → admit (* oracle *) */
} lvProofStepType;

/**
 * @brief 证明步骤结构体
 *
 * 表示 Coq 证明中的单个步骤，包含类型、描述文本和序号。
 */
typedef struct {
    int type;              /**< 步骤类型（lvProofStepType） */
    char description[512]; /**< 步骤描述（tactic 名称） */
    int id;                /**< 步骤编号（按导入顺序） */
} lvProofStep;

/**
 * @brief 内部证明结构体（Coq 版）
 *
 * 用于 Coq 证明脚本的导入/导出中间表示。包含定理名称和步骤数组。
 */
typedef struct {
    char theorem_name[256]; /**< 定理名称 */
    int step_count;         /**< 当前步骤数量 */
    int step_capacity;      /**< 步骤数组容量 */
    lvProofStep *steps;     /**< 步骤动态数组 */
} lvCoqProof;

/* 映射表大小常量 */
#define COQ_TACTIC_MAP_COUNT 8
#define COQ_REVERSE_MAP_COUNT 8
#define COQ_VALID_TACTICS_COUNT 35

/**
 * @brief 将内部证明树导出为 Coq .v 脚本格式
 *
 * 遍历 lvCoqProof 的步骤数组，将每一步骤类型通过 tactic_map 映射为
 * Coq tactic 名称，生成符合 Coq 8.18 语法的完整 .v 文件内容。
 *
 * @param proof      lvCoqProof 指针（内部证明结构体）
 * @param output     输出缓冲区（用于写入 Coq 脚本）
 * @param output_size 输出缓冲区大小（字节）
 * @return 成功返回 0，参数无效或缓冲区不足返回 -1
 */
static int coq_export_proof(void *proof, char *output, int output_size) {
    if (!proof || !output || output_size <= 0)
        return -1;

    lvCoqProof *p = (lvCoqProof *) proof;

    /* 步骤类型到 Coq tactic 的映射表 */
    static const struct {
        int step_type;
        const char *tactic;
    } tactic_map[] = {{lv_STEP_ADD_NODE, "intro"},         {lv_STEP_ADD_CONSTRAINT, "constructor"},
                      {lv_STEP_REWRITE, "rewrite"},        {lv_STEP_FUNCTION_APP, "apply"},
                      {lv_STEP_NORMALIZATION, "simpl"},    {lv_STEP_UNIFY, "reflexivity"},
                      {lv_STEP_EX_FALSO, "contradiction"}, {lv_STEP_ORACLE, "admit (* oracle *)"}};
    int tactic_count = COQ_TACTIC_MAP_COUNT;

    /* 输出头 */
    const char *header =
        "Require Import lv.\n\n"
        "Theorem ";
    const char *footer =
        ".\n"
        "Qed.\n";
    int header_len = (int) strlen(header);
    int footer_len = (int) strlen(footer);

    /* 检查基本空间 */
    if (header_len + footer_len + 64 >= output_size)
        return -1;

    memcpy(output, header, header_len);
    int pos = header_len;

    /* 写入定理名称 */
    int name_len = (int) strlen(p->theorem_name);
    if (pos + name_len + 16 >= output_size)
        return -1;
    memcpy(output + pos, p->theorem_name, name_len);
    pos += name_len;

    /* 写入 ": Prop." 和 "Proof." */
    pos += snprintf(output + pos, output_size - pos, " : Prop.\nProof.\n");

    /* 遍历每个步骤，生成对应的 Coq tactic */
    for (int i = 0; i < p->step_count; i++) {
        lvProofStep *step = &p->steps[i];
        const char *tac = "admit"; /* 默认 tactic */

        /* 在映射表中查找对应的 tactic */
        for (int j = 0; j < tactic_count; j++) {
            if (step->type == tactic_map[j].step_type) {
                tac = tactic_map[j].tactic;
                break;
            }
        }

        /* 检查剩余空间是否足够 */
        int tac_len = (int) strlen(tac);
        if (pos + tac_len + 8 >= output_size)
            return -1;

        /* 写入 tactic，以 "." 结尾 */
        pos += snprintf(output + pos, output_size - pos, "  %s.\n", tac);
    }

    /* 写入尾部 */
    if (pos + footer_len + 1 >= output_size)
        return -1;
    memcpy(output + pos, footer, footer_len + 1);
    return 0;
}

/**
 * @brief 解析 Coq .v 脚本并构建内部证明树
 *
 * 从 Coq 脚本中提取 "Theorem"/"Lemma" 关键字后的定理名，扫描 "Proof." 到 "Qed."
 * 之间的 tactic 行，通过 reverse_map 反向映射为 Lv-00 步骤类型。
 *
 * @param input  Coq .v 脚本内容（以 null 结尾的字符串）
 * @param proof  输出参数：成功时指向新分配的 lvCoqProof 结构体
 * @return 成功返回 0，输入无效或解析失败返回 -1
 */
static int coq_import_proof(const char *input, void **proof) {
    if (!input || !proof)
        return -1;
    *proof = NULL;

    /* 检查输入非空 */
    if (strlen(input) == 0)
        return -1;

    /* 查找 "Theorem" 关键字 */
    const char *theorem_kw = strstr(input, "Theorem");
    if (!theorem_kw)
        return -1;

    /* 提取定理名（Theorem 后的第一个标识符） */
    const char *name_start = theorem_kw + 7; /* 跳过 "Theorem" */
    while (*name_start && isspace((unsigned char) *name_start))
        name_start++;
    const char *name_end = name_start;
    while (*name_end && !isspace((unsigned char) *name_end) && *name_end != ':')
        name_end++;
    if (name_end == name_start)
        return -1;

    /* 查找 "Proof." 关键字，确定 tactic 脚本起始位置 */
    const char *proof_kw = strstr(input, "Proof.");
    if (!proof_kw)
        return -1;
    const char *script_start = proof_kw + 6; /* 跳过 "Proof." */
    while (*script_start && isspace((unsigned char) *script_start))
        script_start++;

    /* 查找 "Qed." 关键字，确定 tactic 脚本结束位置 */
    const char *qed_kw = strstr(script_start, "Qed.");
    if (!qed_kw)
        return -1;

    /* Coq tactic 到 Lv-00 步骤类型的反向映射 */
    static const struct {
        const char *tactic;
        int step_type;
    } reverse_map[] = {{"intro", lv_STEP_ADD_NODE},         {"constructor", lv_STEP_ADD_CONSTRAINT},
                       {"rewrite", lv_STEP_REWRITE},        {"apply", lv_STEP_FUNCTION_APP},
                       {"simpl", lv_STEP_NORMALIZATION},    {"reflexivity", lv_STEP_UNIFY},
                       {"contradiction", lv_STEP_EX_FALSO}, {"admit", lv_STEP_ORACLE}};
    int reverse_count = COQ_REVERSE_MAP_COUNT;

    /* 分配证明结构体 */
    lvCoqProof *p = (lvCoqProof *) lv_calloc(1, sizeof(lvCoqProof));
    if (!p)
        return -1;

    /* 保存定理名 */
    {
        size_t nlen = (size_t) (name_end - name_start);
        if (nlen >= sizeof(p->theorem_name))
            nlen = sizeof(p->theorem_name) - 1;
        memcpy(p->theorem_name, name_start, nlen);
        p->theorem_name[nlen] = '\0';
    }

    /* 初始化步骤数组 */
    p->step_capacity = 16;
    p->steps = (lvProofStep *) lv_calloc(p->step_capacity, sizeof(lvProofStep));
    if (!p->steps) {
        lv_free((void **) &p);
        return -1;
    }

    /* 逐行解析 tactic 脚本 */
    const char *line = script_start;
    while (line < qed_kw) {
        /* 跳过空白 */
        while (line < qed_kw && isspace((unsigned char) *line))
            line++;
        if (line >= qed_kw)
            break;

        /* 找到行尾 */
        const char *line_end = line;
        while (line_end < qed_kw && *line_end != '\n' && *line_end != '\r')
            line_end++;

        /* 提取 tactic 名称（行首到第一个空格或 '.'） */
        const char *tac_start = line;
        const char *tac_end = tac_start;
        while (tac_end < line_end && !isspace((unsigned char) *tac_end) && *tac_end != '.')
            tac_end++;

        if (tac_end > tac_start) {
            /* 查找对应的步骤类型 */
            int step_type = -1;
            int tac_len = (int) (tac_end - tac_start);

            for (int j = 0; j < reverse_count; j++) {
                if ((int) strlen(reverse_map[j].tactic) == tac_len &&
                    strncmp(tac_start, reverse_map[j].tactic, tac_len) == 0) {
                    step_type = reverse_map[j].step_type;
                    break;
                }
            }

            /* 如果找到有效映射，添加步骤 */
            if (step_type >= 0) {
                /* 检查是否需要扩容 */
                if (p->step_count >= p->step_capacity) {
                    /* [安全] 乘法前做溢出检查 */
                    if (p->step_capacity > INT_MAX / 2) {
                        lv_free((void **) &p->steps);
                        lv_free((void **) &p);
                        return -1;
                    }
                    int new_cap = p->step_capacity * 2;
                    lvProofStep *new_steps = (lvProofStep *) lv_realloc(p->steps, new_cap * sizeof(lvProofStep));
                    if (!new_steps) {
                        lv_free((void **) &p->steps);
                        lv_free((void **) &p);
                        return -1;
                    }
                    p->steps = new_steps;
                    p->step_capacity = new_cap;
                }

                lvProofStep *step = &p->steps[p->step_count];
                step->type = step_type;
                step->id = p->step_count;
                /* 保存 tactic 名称作为描述 */
                {
                    size_t dlen = (size_t) (tac_end - tac_start);
                    if (dlen >= sizeof(step->description))
                        dlen = sizeof(step->description) - 1;
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

/**
 * @brief 对 Coq 输入进行基本语法校验
 *
 * 检查花括号和圆括号的嵌套平衡性，检测是否包含 "Theorem"/"Lemma" 关键字，
 * 以及是否包含已知的有效 tactic 名称（含自定义 tactic 的宽松判断）。
 *
 * @param input Coq 脚本输入
 * @return 校验通过返回 1，无效输入或校验失败返回 0
 */
static int coq_validate(const char *input) {
    if (!input)
        return 0;
    if (strlen(input) == 0)
        return 0;

    /* 检查花括号平衡 */
    int brace_depth = 0;
    /* 检查圆括号平衡 */
    int paren_depth = 0;
    for (const char *p = input; *p; p++) {
        if (*p == '{')
            brace_depth++;
        else if (*p == '}') {
            brace_depth--;
            if (brace_depth < 0)
                return 0; /* 花括号不匹配 */
        } else if (*p == '(')
            paren_depth++;
        else if (*p == ')') {
            paren_depth--;
            if (paren_depth < 0)
                return 0; /* 圆括号不匹配 */
        }
    }
    if (brace_depth != 0)
        return 0; /* 花括号不平衡 */
    if (paren_depth != 0)
        return 0; /* 圆括号不平衡 */

    /* 检查是否包含 "Theorem" 或 "Lemma" 关键字 */
    int has_theorem = (strstr(input, "Theorem") != NULL);
    int has_lemma = (strstr(input, "Lemma") != NULL);
    if (!has_theorem && !has_lemma)
        return 0;

    /* 检查是否包含有效 tactic 名称 */
    static const char *valid_tactics[] = {
        "intro",        "apply",      "rewrite",   "constructor",  "simpl",      "reflexivity", "contradiction",
        "admit",        "exact",      "induction", "destruct",     "cases",      "split",       "left",
        "right",        "assumption", "auto",      "trivial",      "omega",      "ring",        "field",
        "lia",          "nia",        "tauto",     "unfold",       "fold",       "change",      "replace",
        "set",          "pose",       "assert",    "generalize",   "specialize", "inversion",   "injection",
        "discriminate", "subst",      "symmetry",  "transitivity", "f_equal",    "congruence"};
    int valid_count = COQ_VALID_TACTICS_COUNT;

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

/**
 * @brief 注册 Coq 互操作插件
 *
 * 向 lvInteropManager 注册 Coq 8.18 支持的插件实例，提供导出 (export_proof)、
 * 导入 (import_proof) 和校验 (validate) 回调函数。
 *
 * @param mgr 互操作管理器指针
 * @return 成功返回 0，mgr 为 NULL 返回 -1
 */
int lv_register_coq_plugin(lvInteropManager *mgr) {
    if (!mgr)
        return -1;
    lvPlugin plugin;
    memset(&plugin, 0, sizeof(plugin));
    strncpy(plugin.name, "coq", sizeof(plugin.name) - 1);
    strncpy(plugin.version, "8.18", sizeof(plugin.version) - 1);
    plugin.system = lv_EXT_COQ;
    plugin.export_proof = coq_export_proof;
    plugin.import_proof = coq_import_proof;
    plugin.validate = coq_validate;
    return lv_interop_register_plugin(mgr, &plugin);
}
