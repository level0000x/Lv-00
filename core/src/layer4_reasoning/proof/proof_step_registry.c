/**
 * @file proof_step_registry.c
 * @brief 证明步骤类型（ProofStepType）统一注册表实现
 *
 * 单张注册表承载 9 种步骤类型的全部展示文案与 HOL 验证规则映射，
 * 供 proof_navigator_utils / proof_dependency / proof_strategy_hol_oracle
 * 等模块查询，消除跨文件文案漂移。
 *
 * @author Lv-00 Project
 */

#include "proof_step_registry.h"

#include "lv/lv_xmacro.h"

/** @brief 证明步骤类型注册表（按枚举值升序，数组下标即枚举值） */
static const ProofStepInfo s_proof_step_info_table[] = {
    [PROOF_STEP_ADD_NODE] = {
        PROOF_STEP_ADD_NODE,
        "Add Node",
        "构造",
        "Construct",
        "根据已知条件和构造规则，该几何对象可以合法构造。",
        "Based on the known conditions and construction rules, this geometric object is validly constructible.",
        PROOF_STEP_HOL_RULE_NONE,
    },
    [PROOF_STEP_ADD_CONSTRAINT] = {
        PROOF_STEP_ADD_CONSTRAINT,
        "Add Constraint",
        "添加约束",
        "Add constraint",
        "根据已构造的几何对象之间的关系，该约束成立。",
        "Based on the relationships between constructed geometric objects, this constraint holds.",
        PROOF_STEP_HOL_RULE_NONE,
    },
    [PROOF_STEP_REWRITE] = {
        PROOF_STEP_REWRITE,
        "Rewrite",
        "应用重写规则",
        "Apply rewrite rule",
        "模式匹配成功，重写规则的前提条件已满足。",
        "Pattern matching succeeded; the preconditions of the rewrite rule are satisfied.",
        VERIFY_TRANS,
    },
    [PROOF_STEP_FUNCTION_APP] = {
        PROOF_STEP_FUNCTION_APP,
        "Function Application",
        "应用函数块",
        "Apply function block",
        "函数块的输入端口类型与实参类型匹配。",
        "The input port types of the function block match the argument types.",
        VERIFY_MK_COMB,
    },
    [PROOF_STEP_PACK_FUNCTION] = {
        PROOF_STEP_PACK_FUNCTION,
        "Pack Function",
        "打包函数块",
        "Package function block",
        "",
        "",
        PROOF_STEP_HOL_RULE_NONE,
    },
    [PROOF_STEP_NORMALIZATION] = {
        PROOF_STEP_NORMALIZATION,
        "Normalization",
        "执行规范化",
        "Perform normalization",
        "检测到坐标等价的节点，执行合并以保持图的一致性。",
        "Coordinate-equivalent nodes detected; merging to maintain graph consistency.",
        VERIFY_BETA_CONV,
    },
    [PROOF_STEP_UNIFY] = {
        PROOF_STEP_UNIFY,
        "Unify",
        "执行合一检查",
        "Perform unification check",
        "构造图与命题模式在所有层级完成匹配。",
        "The construction graph matches the proposition pattern at all levels.",
        PROOF_STEP_HOL_RULE_NONE,
    },
    [PROOF_STEP_EX_FALSO] = {
        PROOF_STEP_EX_FALSO,
        "Ex Falso",
        "应用爆炸原理",
        "Apply ex falso quodlibet",
        "由矛盾 ⊥ 出发，根据爆炸原理可以推出任意命题。",
        "From contradiction ⊥, any proposition follows by the principle of explosion.",
        PROOF_STEP_HOL_RULE_NONE,
    },
    [PROOF_STEP_ORACLE] = {
        PROOF_STEP_ORACLE,
        "Oracle",
        "引用外部预言机",
        "Reference external oracle",
        "此步骤依赖外部知识源，其正确性需要独立验证。",
        "This step depends on an external knowledge source whose correctness requires independent verification.",
        PROOF_STEP_HOL_RULE_NONE,
    },
};

const ProofStepInfo *proof_step_info(ProofStepType type) {
    if ((unsigned) type >= lv_ARRAY_SIZE(s_proof_step_info_table))
        return NULL;
    return &s_proof_step_info_table[(unsigned) type];
}
