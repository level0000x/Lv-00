/*
 * @file prop_verifier_checks.c
 * @brief Proposition verifier module - smoke tests
 * @details Split from prop_verifier.c
 */

#include "lv/prop_verifier.h"
#include "lv/prop_formula_ops.h"
#include "prop_verifier_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/stream.h"
#include "lv/stream.h"

/* ============================================================
 * 内置烟测集
 * ============================================================ */

/* 检查 child 是否是 parent 的子节点（递归） */
static bool formula_is_descendant(const PropFormula *child, const PropFormula *parent) {
    if (!child || !parent)
        return false;
    if (child == parent)
        return true;
    const PropFormulaOps *ops = prop_formula_get_ops(parent->type);
    if (ops && ops->is_descendant)
        return ops->is_descendant(child, parent);
    return false;
}

/* 烟测辅助宏：构造原子命题 */
#define ATOM(name) prop_formula_create_atom(name)
#define AND(a, b) prop_formula_create_conjunction((a), (b))
#define OR(a, b) prop_formula_create_disjunction((a), (b))
#define IMPL(a, b) prop_formula_create_implication((a), (b))
#define NEG(a) prop_formula_create_negation(a)
#define BOT() prop_formula_create_bottom()
#define TOP() prop_formula_create_true()

int prop_verifier_builtin_smoke_test_count(void) {
    return PROP_SMOKE_TEST_COUNT;
}

int prop_verifier_run_builtin_smoke_tests(VerifyDetail *results) {
    SmokeTest tests[PROP_SMOKE_TEST_COUNT];
    memset(&tests, 0, sizeof(tests));

    /*
     * 内存清理策略：
     * 每个测试的复合公式（AND/OR/IMPL/NEG）会取子节点作为指针。
     * 为避免 double-free，每个测试库拥有的复合公式使用独立原子命题。
     * 清理时使用 formula_is_descendant 判断哪些是"根"公式。
     */

    /* 测试 1: P, P→Q ⊢ Q (modus ponens) */
    {
        PropFormula *p = ATOM("P");
        PropFormula *q = ATOM("Q");
        PropFormula *pimplq = IMPL(p, q);
        tests[0].premises[0] = p;
        tests[0].premises[1] = pimplq;
        tests[0].premise_count = 2;
        tests[0].goal = q;
        tests[0].expected_provable = true;
        tests[0].description = "P, P->Q |- Q (modus ponens)";
    }

    /* 测试 2: P∧Q ⊢ P (∧-elimination) */
    {
        PropFormula *p = ATOM("P");
        PropFormula *q = ATOM("Q");
        PropFormula *pq = AND(p, q);
        tests[1].premises[0] = pq;
        tests[1].premise_count = 1;
        tests[1].goal = p;
        tests[1].expected_provable = true;
        tests[1].description = "P/\\Q |- P (conjunction elimination)";
    }

    /* 测试 3: P ⊢ P∨Q (∨-intro left) */
    {
        PropFormula *p = ATOM("P");
        PropFormula *q = ATOM("Q");
        PropFormula *porq = OR(p, q);
        tests[2].premises[0] = p;
        tests[2].premise_count = 1;
        tests[2].goal = porq;
        tests[2].expected_provable = true;
        tests[2].description = "P |- P\\/Q (disjunction introduction left)";
    }

    /* 测试 4: P→Q, Q→R ⊢ P→R (hypothetical syllogism)
     * 每个蕴含使用独立的原子命题 */
    {
        PropFormula *pimplq = IMPL(ATOM("P"), ATOM("Q"));
        PropFormula *qimplr = IMPL(ATOM("Q"), ATOM("R"));
        PropFormula *pimplr = IMPL(ATOM("P"), ATOM("R"));
        tests[3].premises[0] = pimplq;
        tests[3].premises[1] = qimplr;
        tests[3].premise_count = 2;
        tests[3].goal = pimplr;
        tests[3].expected_provable = true;
        tests[3].description = "P->Q, Q->R |- P->R (hypothetical syllogism)";
    }

    /* 测试 5: P→(Q→R), P∧Q ⊢ R */
    {
        PropFormula *pimplqimplr = IMPL(ATOM("P"), IMPL(ATOM("Q"), ATOM("R")));
        PropFormula *pq = AND(ATOM("P"), ATOM("Q"));
        PropFormula *r = ATOM("R");
        tests[4].premises[0] = pimplqimplr;
        tests[4].premises[1] = pq;
        tests[4].premise_count = 2;
        tests[4].goal = r;
        tests[4].expected_provable = true;
        tests[4].description = "P->(Q->R), P/\\Q |- R";
    }

    /* 测试 6: ⊥ ⊢ ⊥ (trivial) */
    {
        PropFormula *bot = BOT();
        tests[5].premises[0] = bot;
        tests[5].premise_count = 1;
        tests[5].goal = bot;
        tests[5].expected_provable = true;
        tests[5].description = "_|_ |- _|_ (trivial)";
    }

    /* 测试 7: P, ¬P ⊢ ⊥ (¬-elimination) */
    {
        PropFormula *p = ATOM("P");
        PropFormula *notp = NEG(p);
        PropFormula *bot = BOT();
        tests[6].premises[0] = p;
        tests[6].premises[1] = notp;
        tests[6].premise_count = 2;
        tests[6].goal = bot;
        tests[6].expected_provable = true;
        tests[6].description = "P, ~P |- _|_ (negation elimination)";
    }

    /* 测试 8: (P→Q)→(¬Q→¬P) (contraposition - intuitionistic) */
    {
        PropFormula *contra = IMPL(IMPL(ATOM("P"), ATOM("Q")), IMPL(NEG(ATOM("Q")), NEG(ATOM("P"))));
        tests[7].premise_count = 0;
        tests[7].goal = contra;
        tests[7].expected_provable = true;
        tests[7].description = "|- (P->Q)->(~Q->~P) (contraposition)";
    }

    /* 测试 9: P∧(Q∨R) ⊢ (P∧Q)∨(P∧R) (distribution)
     * 所有子公式使用独立的原子命题 */
    {
        PropFormula *pqorr = AND(ATOM("P"), OR(ATOM("Q"), ATOM("R")));
        PropFormula *pqorpr = OR(AND(ATOM("P"), ATOM("Q")), AND(ATOM("P"), ATOM("R")));
        tests[8].premises[0] = pqorr;
        tests[8].premise_count = 1;
        tests[8].goal = pqorpr;
        tests[8].expected_provable = true;
        tests[8].description = "P/\\(Q\\/R) |- (P/\\Q)\\/(P/\\R) (distribution)";
    }

    /* 测试 10: ¬¬P ⊢ P (NOT provable intuitionistically) */
    {
        PropFormula *p = ATOM("P");
        PropFormula *notnotp = NEG(NEG(p));
        tests[9].premises[0] = notnotp;
        tests[9].premise_count = 1;
        tests[9].goal = p;
        tests[9].expected_provable = false;
        tests[9].description = "~~P |- P (double negation elimination - NOT intuitionistic)";
    }

    /* 测试 11: ⊢ P∨¬P (NOT provable intuitionistically) */
    {
        PropFormula *pnotp = OR(ATOM("P"), NEG(ATOM("P")));
        tests[10].premise_count = 0;
        tests[10].goal = pnotp;
        tests[10].expected_provable = false;
        tests[10].description = "|- P\\/~P (LEM - NOT intuitionistic)";
    }

    /* 测试 12: ⊢ ¬P∨P (NOT provable intuitionistically) */
    {
        PropFormula *notporp = OR(NEG(ATOM("P")), ATOM("P"));
        tests[11].premise_count = 0;
        tests[11].goal = notporp;
        tests[11].expected_provable = false;
        tests[11].description = "|- ~P\\/P (LEM variant - NOT intuitionistic)";
    }

    /* 测试 13: ⊥ ⊢ P (explosion - only with ex_falso) */
    {
        PropFormula *bot = BOT();
        PropFormula *p = ATOM("P");
        tests[12].premises[0] = bot;
        tests[12].premise_count = 1;
        tests[12].goal = p;
        tests[12].expected_provable = true;
        tests[12].description = "_|_ |- P (explosion - requires ex_falso)";
    }

    /* 执行测试 */
    int passed = prop_verifier_run_smoke_tests(tests, PROP_SMOKE_TEST_COUNT, results);

    /* 清理公式
     * 每个测试库拥有的公式可能被多个子节点指向相同指针。
     * 策略：先去重，识别哪些是"根"（未被其他复合公式作为子节点）。
     */
    for (int i = 0; i < PROP_SMOKE_TEST_COUNT; i++) {
        const PropFormula *ptrs[PROP_SMOKE_CLEANUP_MAX_PTRS];
        int ptr_count = 0;
        for (int j = 0; j < tests[i].premise_count && ptr_count < PROP_SMOKE_CLEANUP_MAX_PTRS; j++) {
            /* 去重：跳过已存在的指针 */
            bool dup = false;
            for (int d = 0; d < ptr_count; d++) {
                if (ptrs[d] == tests[i].premises[j]) {
                    dup = true;
                    break;
                }
            }
            if (!dup)
                ptrs[ptr_count++] = tests[i].premises[j];
        }
        if (tests[i].goal) {
            bool dup = false;
            for (int d = 0; d < ptr_count; d++) {
                if (ptrs[d] == tests[i].goal) {
                    dup = true;
                    break;
                }
            }
            if (!dup)
                ptrs[ptr_count++] = tests[i].goal;
        }

        /* 第一遍：识别哪些是"根"（未被其他复合公式作为子节点） */
        bool is_root[PROP_SMOKE_CLEANUP_MAX_PTRS];
        memset(is_root, true, sizeof(is_root));
        for (int k = 0; k < ptr_count; k++) {
            for (int m = 0; m < ptr_count; m++) {
                if (k != m && ptrs[k] != ptrs[m] && formula_is_descendant(ptrs[k], ptrs[m])) {
                    is_root[k] = false;
                    break;
                }
            }
        }

        /* 第二遍：只释放根公式 */
        for (int k = 0; k < ptr_count; k++) {
            if (is_root[k]) {
                prop_formula_destroy((PropFormula *) ptrs[k]);
            }
        }
    }

    return passed;
}
