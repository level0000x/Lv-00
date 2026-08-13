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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/interop.h"
#include "lv/lv_check.h"
#include "lv/lv_internal.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_xmacro.h"
#include "lv/lv_utils.h"
#include "lv/interop_bridge_common.h"

/**
 * @brief Lv-00 证明步骤类型枚举（Coq 映射版）
 *
 * 将 Lv-00 内部证明步骤映射为 Coq 证明策略（tactic）。
 * 每种步骤类型对应一个或多个 Coq 等价策略。
 *
 * exempt: Coq 的 tactic 集合与 Lean 4/OPML 不同（含 UNIFY/EX_FALSO、
 * 缺 EXACT/HAVE/CALC，且 NORMALIZATION=4 与 lv/interop_step_type.h 的
 * EXACT=4 数值分叉），属互操作外部契约，禁止与 lvProofStepType 单源合并。
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

/* lvProofStep 和 lvBridgeProof 定义在 lv/interop_bridge_common.h */

/* 映射表大小常量 */
#define COQ_TACTIC_MAP_COUNT 8
#define COQ_REVERSE_MAP_COUNT 8

/**
 * @brief 将内部证明树导出为 Coq .v 脚本格式
 *
 * 遍历 lvBridgeProof 的步骤数组，将每一步骤类型通过 tactic_map 映射为
 * Coq tactic 名称，生成符合 Coq 8.18 语法的完整 .v 文件内容。
 *
 * @param proof      lvCoqProof 指针（内部证明结构体）
 * @param output     输出缓冲区（用于写入 Coq 脚本）
 * @param output_size 输出缓冲区大小（字节）
 * @return 成功返回 0，参数无效或缓冲区不足返回 -1
 */
static int coq_export_proof(void *proof, char *output, int output_size) {
    lv_CHECK_NOT_NULL(proof);
    lv_CHECK_NOT_NULL(output);
    lv_CHECK_ARG(output_size > 0, lv_ERROR_INVALID_PARAM, "invalid output_size");

    /* 步骤类型到 Coq tactic 的映射表 */
    static const lvBridgeTacticMap tactic_map[] = {
        {lv_STEP_ADD_NODE, "intro"},
        {lv_STEP_ADD_CONSTRAINT, "constructor"},
        {lv_STEP_REWRITE, "rewrite"},
        {lv_STEP_FUNCTION_APP, "apply"},
        {lv_STEP_NORMALIZATION, "simpl"},
        {lv_STEP_UNIFY, "reflexivity"},
        {lv_STEP_EX_FALSO, "contradiction"},
        {lv_STEP_ORACLE, "admit (* oracle *)"}};

    /* 输出头/尾：Coq 8.18 .v 语法框架（header/footer 语义与原实现逐字一致） */
    const lvBridgeExportSpec spec = {
        "Require Import lv.\n\n"
        "Theorem ",
        " : Prop.\nProof.\n",
        ".\nQed.\n",
        "  ",
        ".\n",
        "admit",
        tactic_map,
        (int) (sizeof(tactic_map) / sizeof(tactic_map[0])),
    };

    return bridge_export_proof(proof, output, output_size, &spec);
}

/**
 * @brief 解析 Coq .v 脚本并构建内部证明树
 *
 * 从 Coq 脚本中提取 "Theorem"/"Lemma" 关键字后的定理名，扫描 "Proof." 到 "Qed."
 * 之间的 tactic 行，通过 reverse_map 反向映射为 Lv-00 步骤类型。
 *
 * @param input  Coq .v 脚本内容（以 null 结尾的字符串）
 * @param proof  输出参数：成功时指向新分配的 lvBridgeProof 结构体
 * @return 成功返回 0，输入无效或解析失败返回 -1
 */
static int coq_import_proof(const char *input, void **proof) {
    lv_CHECK_NOT_NULL(input);
    lv_CHECK_NOT_NULL(proof);
    *proof = NULL;

    /* 检查输入非空 */
    if (strlen(input) == 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "empty input");

    /* 查找 "Theorem" 关键字 */
    const char *theorem_kw = strstr(input, "Theorem");
    if (!theorem_kw)
        lv_RETURN_ERROR(lv_ERROR_PARSE, "missing 'Theorem' keyword");

    /* 提取定理名（Theorem 后的第一个标识符） */
    const char *name_start = NULL;
    size_t name_len = 0;
    if (bridge_extract_theorem_name(theorem_kw + 7, &name_start, &name_len) != 0)
        lv_RETURN_ERROR(lv_ERROR_PARSE, "empty theorem name");

    /* 查找 "Proof." 关键字，确定 tactic 脚本起始位置 */
    const char *proof_kw = strstr(input, "Proof.");
    if (!proof_kw)
        lv_RETURN_ERROR(lv_ERROR_PARSE, "missing 'Proof.' keyword");
    const char *script_start = proof_kw + 6; /* 跳过 "Proof." */
    script_start = lv_str_ltrim((char *) script_start); /* lv_str_ltrim 不修改原串 */

    /* 查找 "Qed." 关键字，确定 tactic 脚本结束位置 */
    const char *qed_kw = strstr(script_start, "Qed.");
    if (!qed_kw)
        lv_RETURN_ERROR(lv_ERROR_PARSE, "missing 'Qed.' keyword");

    /* Coq tactic 到 Lv-00 步骤类型的反向映射（name→enum 机制收敛到 lvStrToEnumEntry；互操作字符串内容逐字保留） */
    static const lvStrToEnumEntry reverse_map[] = {
        {"intro", lv_STEP_ADD_NODE},
        {"constructor", lv_STEP_ADD_CONSTRAINT},
        {"rewrite", lv_STEP_REWRITE},
        {"apply", lv_STEP_FUNCTION_APP},
        {"simpl", lv_STEP_NORMALIZATION},
        {"reflexivity", lv_STEP_UNIFY},
        {"contradiction", lv_STEP_EX_FALSO},
        {"admit", lv_STEP_ORACLE}};
    int reverse_count = COQ_REVERSE_MAP_COUNT;

    /* 分配证明结构体 */
    lvBridgeProof *p = (lvBridgeProof *) lv_calloc(1, sizeof(lvBridgeProof));
    if (!p)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "failed to allocate coq proof");

    /* 保存定理名 */
    lv_strlcpy_n(p->theorem_name, sizeof(p->theorem_name), name_start, (size_t) name_len);

    /* 初始化步骤动态数组 */
    lv_darray_init(&p->steps_da, sizeof(lvProofStep));
    if (!lv_darray_reserve(&p->steps_da, 16)) {
        lv_free((void **) &p);
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "failed to reserve steps array");
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
            /* 查找对应的步骤类型（收敛到 lv_str_to_enum，需 NUL 终止临时副本） */
            int step_type = -1;
            int tac_len = (int) (tac_end - tac_start);
            char tac_buf[64];

            if (tac_len < (int) sizeof(tac_buf)) {
                lv_strlcpy_n(tac_buf, sizeof(tac_buf), tac_start, (size_t) tac_len);
                step_type = lv_str_to_enum(reverse_map, reverse_count, tac_buf, -1);
            }

            /* 如果找到有效映射，添加步骤 */
            if (step_type >= 0) {
                lvProofStep step;
                step.type = step_type;
                step.id = p->steps_da.count;
                /* 保存 tactic 名称作为描述 */
                lv_strlcpy_n(step.description, sizeof(step.description), tac_start, (size_t) (tac_end - tac_start));
                if (lv_darray_push(&p->steps_da, &step) < 0) {
                    lv_darray_free(&p->steps_da);
                    lv_free((void **) &p);
                    lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "failed to push step");
                }
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
    if (!lv_str_check_balanced(input, '{', '}'))
        return 0; /* 花括号不匹配 */
    /* 检查圆括号平衡 */
    if (!lv_str_check_balanced(input, '(', ')'))
        return 0; /* 圆括号不匹配 */

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
        "discriminate", "subst",      "symmetry",  "transitivity", "f_equal",    "congruence", NULL};

    int found_tactic = (lv_str_match_any(input, valid_tactics) >= 0) ? 1 : 0;
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
    /* 插件注册骨架收敛于公共 helper bridge_register */
    return bridge_register(mgr, "coq", "8.18", lv_EXT_COQ, coq_export_proof, coq_import_proof, coq_validate);
}
