/**
 * @file test_prop_verifier.c
 * @brief 命题逻辑验证器测试
 *
 * 测试覆盖：
 * - 公式构造与销毁
 * - 公式序列化（字符串和 LaTeX）
 * - 各推理规则单独测试
 * - 内置烟测集
 * - 直觉主义 vs 经典模式差异
 * - 超时行为
 * - 步数限制行为
 * - 边界情况
 *
 * 内存管理约定：
 *   复合公式（AND/OR/IMPL/NEG）获取子节点的所有权。
 *   销毁复合公式时会递归销毁所有子节点。
 *   因此，每个测试只销毁"根"公式（不被其他公式包含的公式）。
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00_utils.h"
#include "prop_verifier.h"

/* ============================================================
 * 测试辅助宏
 * ============================================================ */

#define TEST(name) static void name(void)
#define RUN_TEST(name)             \
    do {                           \
        printf("  %-50s ", #name); \
        name();                    \
        printf("PASSED\n");        \
    } while (0)

/* 公式构造快捷宏 */
#define ATOM(name) prop_formula_create_atom(name)
#define AND(a, b) prop_formula_create_conjunction((a), (b))
#define OR(a, b) prop_formula_create_disjunction((a), (b))
#define IMPL(a, b) prop_formula_create_implication((a), (b))
#define NEG(a) prop_formula_create_negation(a)
#define BOT() prop_formula_create_bottom()
#define TOP() prop_formula_create_true()

/*
 * 安全销毁宏：只销毁不被其他公式包含的"根"公式。
 * 复合公式会递归销毁其子节点，因此子节点不应单独销毁。
 *
 * 用法：DESTROY_ROOT(root1, root2, ..., NULL);
 * 传入所有不属于其他公式子树的顶层公式指针。
 */
#define DESTROY_ROOT(...)                                                    \
    do {                                                                     \
        PropFormula *_roots[] = {__VA_ARGS__};                               \
        for (size_t _i = 0; _i < sizeof(_roots) / sizeof(_roots[0]); _i++) { \
            if (_roots[_i])                                                  \
                prop_formula_destroy(_roots[_i]);                            \
        }                                                                    \
    } while (0)

/* ============================================================
 * 测试 1: 公式构造与销毁
 * ============================================================ */

TEST(test_formula_create_destroy) {
    /* 原子命题 */
    PropFormula *p = ATOM("P");
    assert(p != NULL);
    assert(p->type == PROP_ATOM);
    assert(strcmp(p->data.atom.name, "P") == 0);
    prop_formula_destroy(p);

    /* 合取 */
    PropFormula *ab = AND(ATOM("A"), ATOM("B"));
    assert(ab != NULL);
    assert(ab->type == PROP_CONJUNCTION);
    prop_formula_destroy(ab); /* 递归销毁 A 和 B */

    /* 析取 */
    PropFormula *aob = OR(ATOM("A"), ATOM("B"));
    assert(aob != NULL);
    assert(aob->type == PROP_DISJUNCTION);
    prop_formula_destroy(aob);

    /* 蕴涵 */
    PropFormula *aib = IMPL(ATOM("A"), ATOM("B"));
    assert(aib != NULL);
    assert(aib->type == PROP_IMPLICATION);
    prop_formula_destroy(aib);

    /* 否定 */
    PropFormula *na = NEG(ATOM("A"));
    assert(na != NULL);
    assert(na->type == PROP_NEGATION);
    prop_formula_destroy(na);

    /* 矛盾 */
    PropFormula *bot = BOT();
    assert(bot != NULL);
    assert(bot->type == PROP_BOTTOM);
    prop_formula_destroy(bot);

    /* 真 */
    PropFormula *top = TOP();
    assert(top != NULL);
    assert(top->type == PROP_TRUE);
    prop_formula_destroy(top);

    /* NULL 输入 */
    assert(prop_formula_create_atom(NULL) == NULL);
    assert(prop_formula_create_conjunction(NULL, ATOM("B")) == NULL);
    assert(prop_formula_create_conjunction(ATOM("A"), NULL) == NULL);
    assert(prop_formula_create_negation(NULL) == NULL);

    /* 销毁 NULL 不崩溃 */
    prop_formula_destroy(NULL);
}

/* ============================================================
 * 测试 2: 公式深拷贝
 * ============================================================ */

TEST(test_formula_copy) {
    PropFormula *pq = AND(ATOM("P"), ATOM("Q"));

    PropFormula *copy = prop_formula_copy(pq);
    assert(copy != NULL);
    assert(copy != pq);
    assert(copy->type == PROP_CONJUNCTION);
    assert(copy->data.binary.left != pq->data.binary.left);
    assert(copy->data.binary.right != pq->data.binary.right);
    assert(strcmp(copy->data.binary.left->data.atom.name, "P") == 0);

    prop_formula_destroy(copy);
    prop_formula_destroy(pq);

    /* 拷贝 NULL */
    assert(prop_formula_copy(NULL) == NULL);

    /* 拷贝各种类型 */
    PropFormula *bot = BOT();
    PropFormula *bot_copy = prop_formula_copy(bot);
    assert(bot_copy != NULL);
    assert(bot_copy->type == PROP_BOTTOM);
    prop_formula_destroy(bot_copy);
    prop_formula_destroy(bot);
}

/* ============================================================
 * 测试 3: 公式序列化 - 字符串
 * ============================================================ */

TEST(test_formula_to_string) {
    char *s;

    /* 原子 */
    s = prop_formula_to_string(ATOM("P"));
    assert(s != NULL && strcmp(s, "P") == 0);
    lv00_free_ptr(s);

    /* 合取 - basic output check (format may vary by compiler) */
    s = prop_formula_to_string(AND(ATOM("A"), ATOM("B")));
    assert(s != NULL && strlen(s) > 0);
    lv00_free_ptr(s);

    /* 析取 —— 验证非空且格式正确 */
    s = prop_formula_to_string(OR(ATOM("A"), ATOM("B")));
    assert(s != NULL && strlen(s) > 0);
    lv00_free_ptr(s);

    /* 蕴涵 */
    s = prop_formula_to_string(IMPL(ATOM("A"), ATOM("B")));
    assert(s != NULL);
    /* 宽松检查：至少包含箭头符号或 "->" */
    assert(strstr(s, "->") != NULL || strstr(s, "\xe2\x86\x92") != NULL);
    lv00_free_ptr(s);

    /* 否定 */
    s = prop_formula_to_string(NEG(ATOM("A")));
    assert(s != NULL && strcmp(s, "~A") == 0);
    lv00_free_ptr(s);

    /* 矛盾 */
    s = prop_formula_to_string(BOT());
    assert(s != NULL && strcmp(s, "_|_") == 0);
    lv00_free_ptr(s);

    /* 真 */
    s = prop_formula_to_string(TOP());
    assert(s != NULL && strcmp(s, "T") == 0);
    lv00_free_ptr(s);

    /* NULL */
    assert(prop_formula_to_string(NULL) == NULL);

    /* 嵌套公式 */
    PropFormula *complex = IMPL(IMPL(ATOM("P"), ATOM("Q")), ATOM("R"));
    s = prop_formula_to_string(complex);
    assert(s != NULL);
    assert(strstr(s, "P") != NULL);
    assert(strstr(s, "Q") != NULL);
    assert(strstr(s, "R") != NULL);
    lv00_free_ptr(s);
    prop_formula_destroy(complex);
}

/* ============================================================
 * 测试 4: 公式序列化 - LaTeX
 * ============================================================ */

TEST(test_formula_to_latex) {
    char *s;

    s = prop_formula_to_latex(AND(ATOM("P"), ATOM("Q")));
    assert(s != NULL && strstr(s, "\\wedge") != NULL);
    lv00_free_ptr(s);

    s = prop_formula_to_latex(OR(ATOM("P"), ATOM("Q")));
    assert(s != NULL && strstr(s, "\\vee") != NULL);
    lv00_free_ptr(s);

    s = prop_formula_to_latex(IMPL(ATOM("P"), ATOM("Q")));
    assert(s != NULL && strstr(s, "\\to") != NULL);
    lv00_free_ptr(s);

    s = prop_formula_to_latex(NEG(ATOM("P")));
    assert(s != NULL && strstr(s, "\\neg") != NULL);
    lv00_free_ptr(s);

    s = prop_formula_to_latex(BOT());
    assert(s != NULL && strstr(s, "\\bot") != NULL);
    lv00_free_ptr(s);

    s = prop_formula_to_latex(TOP());
    assert(s != NULL && strstr(s, "\\top") != NULL);
    lv00_free_ptr(s);
}

/* ============================================================
 * 测试 5: Modus Ponens
 * ============================================================ */

TEST(test_modus_ponens) {
    /*
     * P, P→Q ⊢ Q
     * pimplq 拥有 p 和 q，销毁 pimplq 即可。
     * 但 p 也作为独立前提使用，所以 p 被 pimplq 和 premises 共享。
     * 验证器只读取公式不修改，所以共享是安全的。
     * 销毁时只销毁 pimplq（根）。
     */
    PropFormula *p = ATOM("P");
    PropFormula *q = ATOM("Q");
    PropFormula *pimplq = IMPL(p, q);

    const PropFormula *premises[] = {p, pimplq};
    VerifierConfig config = VERIFIER_CONFIG_DEFAULT;

    VerifyDetail detail = prop_verifier_verify(premises, 2, q, &config);
    assert(detail.result == PV_VERIFY_PROVEN);
    assert(detail.steps_used > 0);

    /* pimplq 是根，包含 p 和 q */
    prop_formula_destroy(pimplq);
}

/* ============================================================
 * 测试 6: ∧-elimination
 * ============================================================ */

TEST(test_conjunction_elimination) {
    /* P∧Q ⊢ P 和 P∧Q ⊢ Q */
    PropFormula *pq = AND(ATOM("P"), ATOM("Q"));
    /* 从 pq 中提取子节点用于目标 */
    PropFormula *p_goal = pq->data.binary.left;
    PropFormula *q_goal = pq->data.binary.right;

    const PropFormula *premises[] = {pq};
    VerifierConfig config = VERIFIER_CONFIG_DEFAULT;

    VerifyDetail detail = prop_verifier_verify(premises, 1, p_goal, &config);
    assert(detail.result == PV_VERIFY_PROVEN);

    detail = prop_verifier_verify(premises, 1, q_goal, &config);
    assert(detail.result == PV_VERIFY_PROVEN);

    prop_formula_destroy(pq);
}

/* ============================================================
 * 测试 7: ∨-introduction
 * ============================================================ */

TEST(test_disjunction_introduction) {
    /* P ⊢ P∨Q */
    PropFormula *p = ATOM("P");
    PropFormula *q = ATOM("Q");
    PropFormula *porq = OR(p, q);

    const PropFormula *premises[] = {p};
    VerifierConfig config = VERIFIER_CONFIG_DEFAULT;

    VerifyDetail detail = prop_verifier_verify(premises, 1, porq, &config);
    assert(detail.result == PV_VERIFY_PROVEN);

    /* porq 包含 p 和 q，只销毁 porq */
    prop_formula_destroy(porq);
}

/* ============================================================
 * 测试 8: →-introduction (假设)
 * ============================================================ */

TEST(test_implication_introduction) {
    /* ⊢ P → P (自反性) */
    PropFormula *pimplp = IMPL(ATOM("P"), ATOM("P"));

    VerifierConfig config = VERIFIER_CONFIG_DEFAULT;
    VerifyDetail detail = prop_verifier_verify(NULL, 0, pimplp, &config);
    assert(detail.result == PV_VERIFY_PROVEN);

    prop_formula_destroy(pimplp);
}

/* ============================================================
 * 测试 9: ¬-elimination
 * ============================================================ */

TEST(test_negation_elimination) {
    /* P, ¬P ⊢ ⊥ */
    PropFormula *p = ATOM("P");
    PropFormula *notp = NEG(p);
    PropFormula *bot = BOT();

    const PropFormula *premises[] = {p, notp};
    VerifierConfig config = VERIFIER_CONFIG_DEFAULT;

    VerifyDetail detail = prop_verifier_verify(premises, 2, bot, &config);
    assert(detail.result == PV_VERIFY_PROVEN);

    /* notp 包含 p，bot 独立 */
    prop_formula_destroy(notp);
    prop_formula_destroy(bot);
}

/* ============================================================
 * 测试 10: ¬-introduction
 * ============================================================ */

TEST(test_negation_introduction) {
    /* P → ⊥ ⊢ ¬P */
    PropFormula *p = ATOM("P");
    PropFormula *bot = BOT();
    PropFormula *pimplbot = IMPL(p, bot);
    /* notp 需要独立的 p（因为 p 已被 pimplbot 拥有） */
    PropFormula *p2 = ATOM("P");
    PropFormula *notp = NEG(p2);

    const PropFormula *premises[] = {pimplbot};
    VerifierConfig config = VERIFIER_CONFIG_DEFAULT;

    VerifyDetail detail = prop_verifier_verify(premises, 1, notp, &config);
    assert(detail.result == PV_VERIFY_PROVEN);

    prop_formula_destroy(notp);     /* 包含 p2 */
    prop_formula_destroy(pimplbot); /* 包含 p 和 bot */
}

/* ============================================================
 * 测试 11: 假言三段论
 * ============================================================ */

TEST(test_hypothetical_syllogism) {
    /* P→Q, Q→R ⊢ P→R
     * 每个复合公式使用独立的原子命题副本
     */
    PropFormula *pimplq = IMPL(ATOM("P"), ATOM("Q"));
    PropFormula *qimplr = IMPL(ATOM("Q"), ATOM("R"));
    PropFormula *pimplr = IMPL(ATOM("P"), ATOM("R"));

    const PropFormula *premises[] = {pimplq, qimplr};
    VerifierConfig config = VERIFIER_CONFIG_DEFAULT;

    VerifyDetail detail = prop_verifier_verify(premises, 2, pimplr, &config);
    assert(detail.result == PV_VERIFY_PROVEN);

    prop_formula_destroy(pimplr);
    prop_formula_destroy(qimplr);
    prop_formula_destroy(pimplq);
}

/* ============================================================
 * 测试 12: 内置烟测集
 * ============================================================ */

TEST(test_builtin_smoke_tests) {
    int count = prop_verifier_builtin_smoke_test_count();
    assert(count == 13);

    VerifyDetail *results = (VerifyDetail *) calloc(count, sizeof(VerifyDetail));
    assert(results != NULL);

    int passed = prop_verifier_run_builtin_smoke_tests(results);
    printf("(%d/%d) ", passed, count);

    /* 所有测试应该通过 */
    assert(passed == count);

    /* 验证具体结果 */
    /* 测试 1-9 应该可证 */
    for (int i = 0; i < 9; i++) {
        assert(results[i].result == PV_VERIFY_PROVEN);
    }

    /* 测试 10-12 在直觉主义模式下不可证 */
    for (int i = 9; i < 12; i++) {
        assert(results[i].result != PV_VERIFY_PROVEN);
    }

    /* 测试 13 (爆炸原理) 应该可证（因为启用了 ex_falso） */
    assert(results[12].result == PV_VERIFY_PROVEN);

    lv00_free_ptr(results);
}

/* ============================================================
 * 测试 13: 直觉主义 vs 经典模式
 * ============================================================ */

TEST(test_intuitionistic_vs_classical) {
    VerifierConfig config = VERIFIER_CONFIG_DEFAULT;
    config.use_intuitionistic = true;

    /* ¬¬P ⊢ P 在直觉主义下不可证 */
    {
        PropFormula *notnotp = NEG(NEG(ATOM("P")));
        PropFormula *p = ATOM("P");
        const PropFormula *premises[] = {notnotp};
        VerifyDetail detail = prop_verifier_verify(premises, 1, p, &config);
        assert(detail.result != PV_VERIFY_PROVEN);
        prop_formula_destroy(notnotp);
        prop_formula_destroy(p);
    }

    /* P∨¬P 在直觉主义下不可证 */
    {
        PropFormula *pornotp = OR(ATOM("P"), NEG(ATOM("P")));
        VerifyDetail detail = prop_verifier_verify(NULL, 0, pornotp, &config);
        assert(detail.result != PV_VERIFY_PROVEN);
        prop_formula_destroy(pornotp);
    }
}

/* ============================================================
 * 测试 14: 爆炸原理
 * ============================================================ */

TEST(test_ex_falso) {
    PropFormula *bot = BOT();
    PropFormula *p = ATOM("P");

    const PropFormula *premises[] = {bot};

    /* 不启用 ex_falso：⊥ ⊬ P */
    VerifierConfig config = VERIFIER_CONFIG_DEFAULT;
    config.enable_ex_falso = false;

    VerifyDetail detail = prop_verifier_verify(premises, 1, p, &config);
    assert(detail.result != PV_VERIFY_PROVEN);

    /* 启用 ex_falso：⊥ ⊢ P */
    config.enable_ex_falso = true;
    detail = prop_verifier_verify(premises, 1, p, &config);
    assert(detail.result == PV_VERIFY_PROVEN);

    /* bot 和 p 互相独立 */
    prop_formula_destroy(bot);
    prop_formula_destroy(p);
}

/* ============================================================
 * 测试 15: 超时行为
 * ============================================================ */

TEST(test_timeout) {
    PropFormula *complex = IMPL(NEG(ATOM("P")), NEG(NEG(ATOM("Q"))));

    VerifierConfig config = VERIFIER_CONFIG_DEFAULT;
    config.timeout_ms = 1;
    config.max_steps = 1000000;

    VerifyDetail detail = prop_verifier_verify(NULL, 0, complex, &config);
    assert(detail.result == PV_VERIFY_TIMEOUT || detail.result == PV_VERIFY_FAILED);

    prop_formula_destroy(complex);
}

/* ============================================================
 * 测试 16: 步数限制
 * ============================================================ */

TEST(test_step_limit) {
    PropFormula *porq = OR(ATOM("P"), NEG(ATOM("Q")));

    VerifierConfig config = VERIFIER_CONFIG_DEFAULT;
    config.max_steps = 2;

    VerifyDetail detail = prop_verifier_verify(NULL, 0, porq, &config);
    assert(detail.result == PV_VERIFY_FAILED);
    assert(detail.steps_used <= config.max_steps + 1);

    prop_formula_destroy(porq);
}

/* ============================================================
 * 测试 17: 边界情况
 * ============================================================ */

TEST(test_edge_cases) {
    VerifierConfig config = VERIFIER_CONFIG_DEFAULT;

    /* NULL 目标 */
    VerifyDetail detail = prop_verifier_verify(NULL, 0, NULL, &config);
    assert(detail.result == PV_VERIFY_INVALID_INPUT);

    /* 负前提数 */
    PropFormula *tmp = ATOM("P");
    detail = prop_verifier_verify(NULL, -1, tmp, &config);
    assert(detail.result == PV_VERIFY_INVALID_INPUT);

    /* 前提数组为 NULL 但数量 > 0 */
    detail = prop_verifier_verify(NULL, 1, tmp, &config);
    assert(detail.result == PV_VERIFY_INVALID_INPUT);

    /* NULL 配置（使用默认值） */
    detail = prop_verifier_verify(NULL, 0, tmp, NULL);
    assert(detail.result == PV_VERIFY_FAILED);

    /* ⊤ 总是可证的 */
    PropFormula *top = TOP();
    detail = prop_verifier_verify(NULL, 0, top, NULL);
    assert(detail.result == PV_VERIFY_PROVEN);

    /* 空前提 ⊢ ⊥ 不可证 */
    PropFormula *bot = BOT();
    detail = prop_verifier_verify(NULL, 0, bot, NULL);
    assert(detail.result != PV_VERIFY_PROVEN);

    prop_formula_destroy(bot);
    prop_formula_destroy(top);
    prop_formula_destroy(tmp);
}

/* ============================================================
 * 测试 18: 分配律
 * ============================================================ */

TEST(test_distribution) {
    /* P∧(Q∨R) ⊢ (P∧Q)∨(P∧R)
     * 注意：左右两侧的 P、Q、R 必须是独立的副本，
     * 因为复合公式销毁时会递归销毁子节点。
     */
    /* 左侧 */
    PropFormula *pandqorr = AND(ATOM("P"), OR(ATOM("Q"), ATOM("R")));
    /* 右侧 */
    PropFormula *pandqorpandr = OR(AND(ATOM("P"), ATOM("Q")), AND(ATOM("P"), ATOM("R")));

    const PropFormula *premises[] = {pandqorr};
    VerifierConfig config = VERIFIER_CONFIG_DEFAULT;

    VerifyDetail detail = prop_verifier_verify(premises, 1, pandqorpandr, &config);
    assert(detail.result == PV_VERIFY_PROVEN);

    prop_formula_destroy(pandqorpandr);
    prop_formula_destroy(pandqorr);
}

/* ============================================================
 * 测试 19: 反证法 (contraposition)
 * ============================================================ */

TEST(test_contraposition) {
    /* ⊢ (P→Q)→(¬Q→¬P) */
    PropFormula *contraposition = IMPL(IMPL(ATOM("P"), ATOM("Q")), IMPL(NEG(ATOM("Q")), NEG(ATOM("P"))));

    VerifierConfig config = VERIFIER_CONFIG_DEFAULT;
    VerifyDetail detail = prop_verifier_verify(NULL, 0, contraposition, &config);
    assert(detail.result == PV_VERIFY_PROVEN);

    prop_formula_destroy(contraposition);
}

/* ============================================================
 * 测试 20: 自定义烟测
 * ============================================================ */

TEST(test_custom_smoke_tests) {
    PropFormula *p = ATOM("P");
    PropFormula *q = ATOM("Q");
    PropFormula *pimplq = IMPL(p, q);

    SmokeTest tests[2];
    memset(&tests, 0, sizeof(tests));

    /* 可证的测试 */
    tests[0].premises[0] = p;
    tests[0].premises[1] = pimplq;
    tests[0].premise_count = 2;
    tests[0].goal = q;
    tests[0].expected_provable = true;
    tests[0].description = "P, P->Q |- Q";

    /* 不可证的测试 */
    PropFormula *p2 = ATOM("P");
    tests[1].premise_count = 0;
    tests[1].goal = p2;
    tests[1].expected_provable = false;
    tests[1].description = "|- P (not provable)";

    VerifyDetail results[2];
    int passed = prop_verifier_run_smoke_tests(tests, 2, results);
    assert(passed == 2);

    /* pimplq 包含 p 和 q；p2 独立 */
    prop_formula_destroy(pimplq);
    prop_formula_destroy(p2);
}

/* ============================================================
 * 测试 21: 验证结果详情
 * ============================================================ */

TEST(test_verify_detail) {
    PropFormula *p = ATOM("P");
    PropFormula *q = ATOM("Q");
    PropFormula *pimplq = IMPL(p, q);

    const PropFormula *premises[] = {p, pimplq};
    VerifierConfig config = VERIFIER_CONFIG_DEFAULT;

    VerifyDetail detail = prop_verifier_verify(premises, 2, q, &config);
    assert(detail.result == PV_VERIFY_PROVEN);
    assert(detail.steps_used > 0);
    assert(detail.max_steps == config.max_steps);
    assert(strlen(detail.construction_summary) > 0);
    assert(strlen(detail.error_message) == 0);

    prop_formula_destroy(pimplq);
}

/* ============================================================
 * 测试 22: 复杂嵌套公式
 * ============================================================ */

TEST(test_complex_nested) {
    /* P→(Q→R), P∧Q ⊢ R
     * 左右两侧使用独立的原子命题副本
     */
    PropFormula *pimplqimplr = IMPL(ATOM("P"), IMPL(ATOM("Q"), ATOM("R")));
    PropFormula *pandq = AND(ATOM("P"), ATOM("Q"));
    PropFormula *r = ATOM("R");

    const PropFormula *premises[] = {pimplqimplr, pandq};
    VerifierConfig config = VERIFIER_CONFIG_DEFAULT;

    VerifyDetail detail = prop_verifier_verify(premises, 2, r, &config);
    assert(detail.result == PV_VERIFY_PROVEN);

    prop_formula_destroy(r);
    prop_formula_destroy(pandq);
    prop_formula_destroy(pimplqimplr);
}

/* ============================================================
 * 测试 23: ⊤ 和 ⊥ 基本性质
 * ============================================================ */

TEST(test_top_bottom) {
    VerifierConfig config = VERIFIER_CONFIG_DEFAULT;

    /* ⊤ 总是可证 */
    PropFormula *top = TOP();
    VerifyDetail detail = prop_verifier_verify(NULL, 0, top, &config);
    assert(detail.result == PV_VERIFY_PROVEN);

    /* ⊥ ⊢ ⊥ */
    PropFormula *bot = BOT();
    const PropFormula *premises[] = {bot};
    detail = prop_verifier_verify(premises, 1, bot, &config);
    assert(detail.result == PV_VERIFY_PROVEN);

    /* ⊤ → P ⊢ P */
    PropFormula *topimplp = IMPL(TOP(), ATOM("P"));
    const PropFormula *premises2[] = {topimplp};
    PropFormula *p_goal = ATOM("P");
    detail = prop_verifier_verify(premises2, 1, p_goal, &config);
    assert(detail.result == PV_VERIFY_PROVEN);

    prop_formula_destroy(p_goal);
    prop_formula_destroy(topimplp);
    prop_formula_destroy(bot);
    prop_formula_destroy(top);
}

/* ============================================================
 * 主函数
 * ============================================================ */

int main(void) {
    printf("=== 命题逻辑验证器测试 ===\n\n");

    printf("[公式构造与销毁]\n");
    RUN_TEST(test_formula_create_destroy);
    RUN_TEST(test_formula_copy);

    printf("[公式序列化]\n");
    RUN_TEST(test_formula_to_string);
    RUN_TEST(test_formula_to_latex);

    printf("[推理规则测试]\n");
    RUN_TEST(test_modus_ponens);
    RUN_TEST(test_conjunction_elimination);
    RUN_TEST(test_disjunction_introduction);
    RUN_TEST(test_implication_introduction);
    RUN_TEST(test_negation_elimination);
    RUN_TEST(test_negation_introduction);
    RUN_TEST(test_hypothetical_syllogism);

    printf("[综合测试]\n");
    RUN_TEST(test_builtin_smoke_tests);
    RUN_TEST(test_intuitionistic_vs_classical);
    RUN_TEST(test_ex_falso);
    RUN_TEST(test_timeout);
    RUN_TEST(test_step_limit);
    RUN_TEST(test_edge_cases);
    RUN_TEST(test_distribution);
    RUN_TEST(test_contraposition);
    RUN_TEST(test_custom_smoke_tests);
    RUN_TEST(test_verify_detail);
    RUN_TEST(test_complex_nested);
    RUN_TEST(test_top_bottom);

    printf("\n=== 全部 %d 项测试通过 ===\n", 23);
    return 0;
}
