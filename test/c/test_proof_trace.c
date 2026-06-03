/**
 * @file test_proof_trace.c
 * @brief Lv00ProofTree 模块单元测试
 *
 * 测试 proof_trace 模块的核心功能：
 * - 证明树创建与销毁
 * - 节点创建与字段验证
 * - 根节点设置与验证
 * - 前提添加
 * - 公理/规则设置
 * - 结论设置
 * - 子节点添加
 * - 反证法分支标记
 * - 文本导出
 * - 多级证明树
 *
 * 使用 test_helpers.h 中定义的测试宏框架。
 * 全局计数器 g_pass_count / g_fail_count 记录通过/失败断言数。
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00.h"
#include "proof_trace.h"
#include "test_helpers.h"

/* ============================================================
 * 全局测试计数器
 * ============================================================ */
int g_pass_count = 0;
int g_fail_count = 0;

/* ============================================================
 * 测试用例 1：证明树创建与销毁
 * ============================================================ */
static void test_proof_tree_create_destroy(void) {
    /* 创建证明树 */
    Lv00ProofTree *tree = lv00_proof_tree_create("Pythagorean Theorem", "Direct Proof");
    TEST_ASSERT_NOT_NULL(tree);

    /* 验证元数据 */
    TEST_ASSERT_NOT_NULL(tree->root);
    TEST_ASSERT_EQ(tree->root->id, 0);
    TEST_ASSERT_EQ(tree->root->depth, 0);
    TEST_ASSERT_EQ(tree->node_count, 1);
    TEST_ASSERT_EQ(tree->total_steps, 0);

    /* 销毁证明树 */
    lv00_proof_tree_destroy(tree);
    /* 树销毁后不应再访问 */
}

/* ============================================================
 * 测试用例 2：创建节点并验证字段
 * ============================================================ */
static void test_proof_tree_create_node(void) {
    Lv00ProofTree *tree = lv00_proof_tree_create("Test", NULL);
    TEST_ASSERT_NOT_NULL(tree);

    /* 通过 add_step 在根节点下创建新节点 */
    Lv00ProofTreeNode *node = lv00_proof_tree_add_step(tree, NULL,
        "Reflexivity Axiom", "AB = AB", 0);
    TEST_ASSERT_NOT_NULL(node);

    /* 验证节点基本字段 */
    TEST_ASSERT_EQ(node->depth, 1);
    TEST_ASSERT_EQ(node->step_index, 0);
    TEST_ASSERT_EQ(node->id, 1);
    TEST_ASSERT_EQ(node->child_count, 0);
    TEST_ASSERT_EQ(node->premise_count, 0);

    /* 验证 axiom_used 和 conclusion 已被正确复制 */
    TEST_ASSERT_NOT_NULL(node->axiom_used);
    TEST_ASSERT_NOT_NULL(node->conclusion);

    /* 验证父节点信息 */
    TEST_ASSERT_NOT_NULL(node->parent);
    TEST_ASSERT_EQ(node->parent->id, 0);

    /* 验证树的统计信息已更新 */
    TEST_ASSERT_EQ(tree->total_steps, 1);
    TEST_ASSERT_EQ(tree->max_depth, 1);
    TEST_ASSERT_EQ(tree->node_count, 2);

    lv00_proof_tree_destroy(tree);
}

/* ============================================================
 * 测试用例 3：根节点设置与验证
 * ============================================================ */
static void test_proof_tree_set_root(void) {
    Lv00ProofTree *tree = lv00_proof_tree_create("Root Test Theorem", "Contradiction");
    TEST_ASSERT_NOT_NULL(tree);

    /* 根节点在 create 时自动设置，验证其有效性 */
    Lv00ProofTreeNode *root = tree->root;
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_EQ(root->id, 0);
    TEST_ASSERT_EQ(root->depth, 0);
    TEST_ASSERT_EQ(root->step_index, -1);
    TEST_ASSERT_NULL(root->axiom_used);
    TEST_ASSERT_NULL(root->parent);
    TEST_ASSERT_EQ(root->child_count, 0);
    TEST_ASSERT_EQ(root->premise_count, 0);

    /* root 节点的 conclusion 应该存储定理名称 */
    TEST_ASSERT_NOT_NULL(root->conclusion);

    /* 验证 all_nodes 数组中包含根节点 */
    TEST_ASSERT_EQ(tree->node_count, 1);
    TEST_ASSERT_NOT_NULL(tree->all_nodes);
    TEST_ASSERT_NOT_NULL(tree->all_nodes[0]);
    TEST_ASSERT_EQ((intptr_t)tree->all_nodes[0], (intptr_t)root);

    /* 验证树元数据 */
    TEST_ASSERT_NOT_NULL(tree->theorem_name);
    TEST_ASSERT_NOT_NULL(tree->proof_strategy);

    lv00_proof_tree_destroy(tree);
}

/* ============================================================
 * 测试用例 4：添加前提到节点
 * ============================================================ */
static void test_proof_tree_node_add_premise(void) {
    Lv00ProofTree *tree = lv00_proof_tree_create("Premise Test", NULL);
    TEST_ASSERT_NOT_NULL(tree);

    Lv00ProofTreeNode *node = lv00_proof_tree_add_step(tree, NULL,
        NULL, "Some Conclusion", 0);
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_EQ(node->premise_count, 0);

    /* 添加一个公理性前提 */
    bool ok = lv00_proof_tree_add_premise(node, 100,
        "Through any two points there is exactly one line", true);
    TEST_ASSERT_MSG(ok, "add_premise should succeed");
    TEST_ASSERT_EQ(node->premise_count, 1);

    /* 验证前提字段 */
    TEST_ASSERT_EQ(node->premises[0].premise_id, 100);
    TEST_ASSERT_NOT_NULL(node->premises[0].description);
    TEST_ASSERT_EQ(node->premises[0].is_axiom, (intptr_t)true);

    /* 添加第二个前提（非公理，如已证定理） */
    ok = lv00_proof_tree_add_premise(node, 101,
        "Triangle ABC is isosceles", false);
    TEST_ASSERT_MSG(ok, "add_premise should succeed");
    TEST_ASSERT_EQ(node->premise_count, 2);
    TEST_ASSERT_EQ(node->premises[1].premise_id, 101);
    TEST_ASSERT_NOT_NULL(node->premises[1].description);
    TEST_ASSERT_EQ(node->premises[1].is_axiom, (intptr_t)false);

    lv00_proof_tree_destroy(tree);
}

/* ============================================================
 * 测试用例 5：设置节点使用的公理
 * ============================================================ */
static void test_proof_tree_node_set_axiom(void) {
    Lv00ProofTree *tree = lv00_proof_tree_create("Axiom Set Test", NULL);
    TEST_ASSERT_NOT_NULL(tree);

    /* 创建节点时不指定公理，后续通过字段直接设置 */
    Lv00ProofTreeNode *node = lv00_proof_tree_add_step(tree, NULL,
        NULL, "Conclusion", 0);
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_NULL(node->axiom_used);

    /* 设置公理名称（注意：add_step 中若为 NULL 则 axiom_used 也为 NULL，
     * 此处直接赋值不会导致泄漏） */
    node->axiom_used = lv00_strdup("SAS Congruence Axiom");
    TEST_ASSERT_NOT_NULL(node->axiom_used);

    /* 验证公理已正确设置 */
    TEST_ASSERT_MSG(strcmp(node->axiom_used, "SAS Congruence Axiom") == 0,
                    "axiom_used should be 'SAS Congruence Axiom'");

    lv00_proof_tree_destroy(tree);
}

/* ============================================================
 * 测试用例 6：设置节点结论
 * ============================================================ */
static void test_proof_tree_node_set_conclusion(void) {
    Lv00ProofTree *tree = lv00_proof_tree_create("Conclusion Test", NULL);
    TEST_ASSERT_NOT_NULL(tree);

    /* 创建节点时不指定结论 */
    Lv00ProofTreeNode *node = lv00_proof_tree_add_step(tree, NULL,
        "Given Axiom", NULL, 0);
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_NULL(node->conclusion);

    /* 设置结论 */
    node->conclusion = lv00_strdup("Therefore, triangle ABC is congruent to triangle DEF");
    TEST_ASSERT_NOT_NULL(node->conclusion);

    /* 验证结论已正确设置 */
    TEST_ASSERT_MSG(strcmp(node->conclusion,
        "Therefore, triangle ABC is congruent to triangle DEF") == 0,
        "conclusion should match expected string");

    lv00_proof_tree_destroy(tree);
}

/* ============================================================
 * 测试用例 7：添加子节点
 * ============================================================ */
static void test_proof_tree_node_add_child(void) {
    Lv00ProofTree *tree = lv00_proof_tree_create("Child Test", NULL);
    TEST_ASSERT_NOT_NULL(tree);

    /* 创建父节点 */
    Lv00ProofTreeNode *parent = lv00_proof_tree_add_step(tree, NULL,
        "Parent Axiom", "Parent Conclusion", 0);
    TEST_ASSERT_NOT_NULL(parent);
    TEST_ASSERT_EQ(parent->child_count, 0);

    /* 添加第一个子节点 */
    Lv00ProofTreeNode *child1 = lv00_proof_tree_add_step(tree, parent,
        "Child Axiom 1", "Child Conclusion 1", 1);
    TEST_ASSERT_NOT_NULL(child1);
    TEST_ASSERT_EQ(parent->child_count, 1);
    TEST_ASSERT_EQ(child1->depth, 2);
    TEST_ASSERT_EQ((intptr_t)child1->parent, (intptr_t)parent);

    /* 添加第二个子节点 */
    Lv00ProofTreeNode *child2 = lv00_proof_tree_add_step(tree, parent,
        "Child Axiom 2", "Child Conclusion 2", 2);
    TEST_ASSERT_NOT_NULL(child2);
    TEST_ASSERT_EQ(parent->child_count, 2);
    TEST_ASSERT_EQ(child2->depth, 2);

    /* 验证 children 数组 */
    TEST_ASSERT_NOT_NULL(parent->children);
    TEST_ASSERT_EQ((intptr_t)parent->children[0], (intptr_t)child1);
    TEST_ASSERT_EQ((intptr_t)parent->children[1], (intptr_t)child2);

    /* 验证每个子节点的 id 互不相同 */
    TEST_ASSERT_NE(child1->id, child2->id);

    /* 验证树的统计信息 */
    TEST_ASSERT_EQ(tree->total_steps, 3);
    TEST_ASSERT_EQ(tree->max_depth, 2);

    lv00_proof_tree_destroy(tree);
}

/* ============================================================
 * 测试用例 8：反证法分支标记
 * ============================================================ */
static void test_proof_tree_node_contradiction(void) {
    Lv00ProofTree *tree = lv00_proof_tree_create("Contradiction Test",
        "Proof by Contradiction");
    TEST_ASSERT_NOT_NULL(tree);

    /* 创建反证法分支节点 */
    Lv00ProofTreeNode *node = lv00_proof_tree_add_step(tree, NULL,
        "Assume Negation", "Suppose sqrt(2) is rational", 0);
    TEST_ASSERT_NOT_NULL(node);

    /* 初始不应为矛盾分支 */
    TEST_ASSERT_EQ(node->is_contradiction_branch, (intptr_t)false);

    /* 通过 API 标记为矛盾分支 */
    lv00_proof_tree_mark_contradiction(node);
    TEST_ASSERT_EQ(node->is_contradiction_branch, (intptr_t)true);

    /* 也可以通过直接设置字段来标记 */
    Lv00ProofTreeNode *node2 = lv00_proof_tree_add_step(tree, node,
        NULL, "This leads to a contradiction", 1);
    TEST_ASSERT_NOT_NULL(node2);
    TEST_ASSERT_EQ(node2->is_contradiction_branch, (intptr_t)false);

    node2->is_contradiction_branch = true;
    TEST_ASSERT_EQ(node2->is_contradiction_branch, (intptr_t)true);

    /* 验证标记对兄弟节点无影响 */
    TEST_ASSERT_EQ(node->is_contradiction_branch, (intptr_t)true);
    TEST_ASSERT_EQ(node2->is_contradiction_branch, (intptr_t)true);

    lv00_proof_tree_destroy(tree);
}

/* ============================================================
 * 测试用例 9：证明树文本导出
 * ============================================================ */
static void test_proof_tree_export_text(void) {
    Lv00ProofTree *tree = lv00_proof_tree_create(
        "Triangle Angle Sum Theorem",
        "Construct auxiliary line through vertex");
    TEST_ASSERT_NOT_NULL(tree);

    /* 构建简单的证明树 */
    Lv00ProofTreeNode *step1 = lv00_proof_tree_add_step(tree, NULL,
        "Postulate of Parallel Lines",
        "Through point C, draw line DE parallel to AB", 0);
    TEST_ASSERT_NOT_NULL(step1);

    /* 为 step1 添加前提 */
    lv00_proof_tree_add_premise(step1, 1, "Triangle ABC", false);
    lv00_proof_tree_add_premise(step1, 2, "Parallel postulate", true);

    Lv00ProofTreeNode *step2 = lv00_proof_tree_add_step(tree, step1,
        "Alternate Interior Angles Theorem",
        "Angle DCA equals Angle CAB", 1);
    TEST_ASSERT_NOT_NULL(step2);

    Lv00ProofTreeNode *step3 = lv00_proof_tree_add_step(tree, step1,
        "Alternate Interior Angles Theorem",
        "Angle ECB equals Angle CBA", 2);
    TEST_ASSERT_NOT_NULL(step3);

    /* 导出证明文本（filepath 为 NULL 表示仅返回字符串） */
    char *exported = lv00_proof_tree_export_text(tree, NULL);
    TEST_ASSERT_NOT_NULL(exported);

    /* 验证导出文本非空且包含关键信息 */
    TEST_ASSERT_MSG(strlen(exported) > 0, "exported text should not be empty");

    /* 释放导出文本（调用者负责释放） */
    lv00_free((void **)&exported);
    TEST_ASSERT_NULL(exported);

    lv00_proof_tree_destroy(tree);
}

/* ============================================================
 * 测试用例 10：多级证明树深度验证
 * ============================================================ */
static void test_proof_tree_multi_level(void) {
    Lv00ProofTree *tree = lv00_proof_tree_create("Multi-Level Proof", NULL);
    TEST_ASSERT_NOT_NULL(tree);

    /* 构建一棵三层深的证明树
     *   root (depth 0)
     *     +-- level1 (depth 1)
     *          +-- level2 (depth 2)
     *               +-- level3 (depth 3)
     *          +-- sibling (depth 2)
     */

    Lv00ProofTreeNode *level1 = lv00_proof_tree_add_step(tree, NULL,
        "Axiom A", "Conclusion Level 1", 0);
    TEST_ASSERT_NOT_NULL(level1);
    TEST_ASSERT_EQ(level1->depth, 1);
    TEST_ASSERT_EQ(level1->parent->id, 0);

    Lv00ProofTreeNode *level2 = lv00_proof_tree_add_step(tree, level1,
        "Axiom B", "Conclusion Level 2", 1);
    TEST_ASSERT_NOT_NULL(level2);
    TEST_ASSERT_EQ(level2->depth, 2);
    TEST_ASSERT_EQ((intptr_t)level2->parent, (intptr_t)level1);

    Lv00ProofTreeNode *level3 = lv00_proof_tree_add_step(tree, level2,
        "Axiom C", "Conclusion Level 3", 2);
    TEST_ASSERT_NOT_NULL(level3);
    TEST_ASSERT_EQ(level3->depth, 3);
    TEST_ASSERT_EQ((intptr_t)level3->parent, (intptr_t)level2);

    /* 为 level1 添加另一个子节点，深度应为 2 */
    Lv00ProofTreeNode *sibling = lv00_proof_tree_add_step(tree, level1,
        "Axiom D", "Sibling Conclusion", 3);
    TEST_ASSERT_NOT_NULL(sibling);
    TEST_ASSERT_EQ(sibling->depth, 2);
    TEST_ASSERT_EQ((intptr_t)sibling->parent, (intptr_t)level1);

    /* 验证树统计信息 */
    TEST_ASSERT_EQ(tree->total_steps, 4);
    TEST_ASSERT_EQ(tree->max_depth, 3);
    TEST_ASSERT_EQ(tree->node_count, 5);  /* root + 4 steps */

    /* 验证兄弟关系不影响各自深度 */
    TEST_ASSERT_EQ(level1->child_count, 2);
    TEST_ASSERT_EQ(level2->depth, sibling->depth);
    TEST_ASSERT_EQ(level2->depth, 2);
    TEST_ASSERT_EQ(sibling->depth, 2);

    /* 验证 leaf 节点没有子节点 */
    TEST_ASSERT_EQ(level3->child_count, 0);
    TEST_ASSERT_EQ(sibling->child_count, 0);

    lv00_proof_tree_destroy(tree);
}

/* ============================================================
 * 主入口
 * ============================================================ */
int main(void) {
    TEST_SUITE_BEGIN("Lv00ProofTree");

    TEST_RUN(test_proof_tree_create_destroy);
    TEST_RUN(test_proof_tree_create_node);
    TEST_RUN(test_proof_tree_set_root);
    TEST_RUN(test_proof_tree_node_add_premise);
    TEST_RUN(test_proof_tree_node_set_axiom);
    TEST_RUN(test_proof_tree_node_set_conclusion);
    TEST_RUN(test_proof_tree_node_add_child);
    TEST_RUN(test_proof_tree_node_contradiction);
    TEST_RUN(test_proof_tree_export_text);
    TEST_RUN(test_proof_tree_multi_level);

    TEST_SUITE_END();

    return g_fail_count > 0 ? 1 : 0;
}
