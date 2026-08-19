/**
 * @file test_quantifier.c
 * @brief 量词系统（quantifier）零覆盖 API 契约测试
 *
 * 批次 C-㉚：补全 quantifier.h 中全部 17 个此前零测试覆盖的 API。
 * 覆盖域：
 * - 域管理：create / create_finite / add_element / add_elements / contains /
 *   size / destroy
 * - 量化表达式：expr_create / expr_destroy / expr_evaluate（空域/有限域/
 *   无限域三值语义 + 真值缓存）
 * - 量词运算：instantiate（∀E）/ generalize（∀I）/ exists_introduce（∃I）/
 *   exists_eliminate（∃E）
 * - 有限域消去：eliminate_forall_finite / eliminate_exists_finite /
 *   eliminate_exists_unique_finite / is_eliminable / count_satisfying
 * - 结果管理：result_destroy / 字符串映射
 *
 * 语义钉住（按实现契约）：
 * - 空域：∀→TRUE、∃→FALSE、∃!→FALSE（空合取/空析取的恒等元）
 * - 有限域：∀=AND 归约、∃=OR 归约（短路）、∃!=恰好一个满足
 * - 无限域（命名域）：evaluate→lv_UNKNOWN、is_eliminable→false、
 *   count_satisfying→-1
 * - 体命题评估：evaluate_body_for_element 命中 variable_node_id /
 *   precondition_region_ids / output_port_ids / 子命题 → TRUE
 *
 * @author Lv-00 Project
 * @date 2026-08-19
 */

#include <stdio.h>
#include <string.h>

#include "lv/quantifier.h"
#include "lv/proof.h"
#include "lv.h"
#include "test_helpers.h"

/* ============================================================
 * 全局测试计数器
 * ============================================================ */
int g_pass_count = 0;
int g_fail_count = 0;

/* ============================================================
 * 辅助：创建带体命题的量化表达式（体命题引用 variable_node_id → 评估 TRUE）
 * ============================================================ */
static lvQuantifiedExpr *mk_expr(int expr_id, lvQuantifier q, lvDomain *domain, int var_node_id, int body_id) {
    struct Proposition *body = proposition_create(body_id, PROPOSITION_TYPE_ATOMIC);
    if (!body)
        return NULL;
    lvQuantifiedExpr *expr = lv_quant_expr_create(expr_id, q, "x", var_node_id, domain, body);
    if (!expr) {
        proposition_unref(body);
        return NULL;
    }
    return expr;
}

/* ============================================================
 * 域管理
 * ============================================================ */
static void test_quant_domain(void) {
    /* 命名域（无限） */
    lvDomain *d = lv_quant_domain_create(1, "Point");
    TEST_ASSERT_MSG(d != NULL, "命名域创建");
    TEST_ASSERT_MSG(d->id == 1, "域 ID");
    TEST_ASSERT_MSG(d->domain_name != NULL && strcmp(d->domain_name, "Point") == 0, "域名复制");
    TEST_ASSERT_MSG(!d->is_finite, "命名域默认无限");
    TEST_ASSERT_MSG(lv_quant_domain_size(d) == -1, "无限域 size 为 -1");
    TEST_ASSERT_MSG(lv_quant_domain_contains(d, 5) == false, "空命名域不含元素");

    /* 添加元素 → 转为有限域 */
    TEST_ASSERT_MSG(lv_quant_domain_add_element(d, 5), "添加元素 5");
    TEST_ASSERT_MSG(d->is_finite, "添加元素后有限");
    TEST_ASSERT_MSG(lv_quant_domain_contains(d, 5), "包含元素 5");
    TEST_ASSERT_MSG(!lv_quant_domain_contains(d, 6), "不含元素 6");
    TEST_ASSERT_MSG(lv_quant_domain_size(d) == 1, "size 1");

    /* 重复添加：幂等 */
    TEST_ASSERT_MSG(lv_quant_domain_add_element(d, 5), "重复添加返回 true（幂等）");
    TEST_ASSERT_MSG(lv_quant_domain_size(d) == 1, "重复添加不改变 size");

    /* 批量添加 */
    int more[] = {6, 7, 8};
    TEST_ASSERT_MSG(lv_quant_domain_add_elements(d, more, 3), "批量添加");
    TEST_ASSERT_MSG(lv_quant_domain_size(d) == 4, "批量后 size 4");
    TEST_ASSERT_MSG(lv_quant_domain_contains(d, 8), "包含批量元素");
    TEST_ASSERT_MSG(lv_quant_domain_add_elements(d, more, 3), "批量重复添加幂等");
    TEST_ASSERT_MSG(lv_quant_domain_size(d) == 4, "批量重复后 size 仍 4");

    /* 有限枚举域创建 */
    int elems[] = {10, 20, 30};
    lvDomain *df = lv_quant_domain_create_finite(2, elems, 3);
    TEST_ASSERT_MSG(df != NULL, "有限域创建");
    TEST_ASSERT_MSG(df->is_finite, "有限域标记");
    TEST_ASSERT_MSG(lv_quant_domain_size(df) == 3, "有限域 size 3");
    TEST_ASSERT_MSG(lv_quant_domain_contains(df, 20), "有限域含元素 20");
    TEST_ASSERT_MSG(!lv_quant_domain_contains(df, 99), "有限域不含 99");

    /* 空有限域 */
    lvDomain *de = lv_quant_domain_create_finite(3, NULL, 0);
    TEST_ASSERT_MSG(de != NULL, "空有限域创建");
    TEST_ASSERT_MSG(de->is_finite, "空有限域仍有限");
    TEST_ASSERT_MSG(lv_quant_domain_size(de) == 0, "空有限域 size 0");

    /* NULL 安全 */
    TEST_ASSERT_MSG(lv_quant_domain_create_finite(4, NULL, -1) == NULL, "负数 count 返回 NULL");
    TEST_ASSERT_MSG(!lv_quant_domain_add_element(NULL, 1), "NULL 域添加失败");
    TEST_ASSERT_MSG(!lv_quant_domain_add_elements(NULL, elems, 1), "NULL 域批量失败");
    TEST_ASSERT_MSG(!lv_quant_domain_add_elements(df, NULL, 1), "NULL 数组批量失败");
    TEST_ASSERT_MSG(!lv_quant_domain_contains(NULL, 1), "NULL 域 contains false");
    TEST_ASSERT_MSG(lv_quant_domain_size(NULL) == -1, "NULL 域 size -1");
    lv_quant_domain_destroy(NULL);

    lv_quant_domain_destroy(d);
    lv_quant_domain_destroy(df);
    lv_quant_domain_destroy(de);
}

/* ============================================================
 * 表达式创建/销毁 + 三值评估
 * ============================================================ */
static void test_quant_expr_evaluate(void) {
    /* 空域：∀→TRUE、∃→FALSE、∃!→FALSE */
    {
        lvDomain *de = lv_quant_domain_create_finite(10, NULL, 0);
        lvQuantifiedExpr *e1 = mk_expr(1, lv_FORALL, de, 1, 100);
        TEST_ASSERT_MSG(lv_quant_expr_evaluate(e1) == lv_TRUE, "空域 ∀ 为 TRUE");
        TEST_ASSERT_MSG(lv_quant_expr_evaluate(e1) == lv_TRUE, "真值缓存：重复评估一致");
        lv_quant_expr_destroy(e1);
    }
    {
        lvDomain *de = lv_quant_domain_create_finite(11, NULL, 0);
        lvQuantifiedExpr *e2 = mk_expr(2, lv_EXISTS, de, 1, 100);
        TEST_ASSERT_MSG(lv_quant_expr_evaluate(e2) == lv_FALSE, "空域 ∃ 为 FALSE");
        lv_quant_expr_destroy(e2);
    }
    {
        lvDomain *de = lv_quant_domain_create_finite(12, NULL, 0);
        lvQuantifiedExpr *e3 = mk_expr(3, lv_EXISTS_UNIQUE, de, 1, 100);
        TEST_ASSERT_MSG(lv_quant_expr_evaluate(e3) == lv_FALSE, "空域 ∃! 为 FALSE");
        lv_quant_expr_destroy(e3);
    }

    /* 有限域：体命题命中 variable_node_id → 全部 TRUE */
    {
        int elems[] = {7, 8, 9};
        lvDomain *df = lv_quant_domain_create_finite(13, elems, 3);
        /* variable_node_id=8：元素 7/9 评估 UNKNOWN，8 评估 TRUE */
        lvQuantifiedExpr *e4 = mk_expr(4, lv_FORALL, df, 8, 101);
        /* 三值 AND：TRUE ∧ UNKNOWN ∧ TRUE → UNKNOWN */
        TEST_ASSERT_MSG(lv_quant_expr_evaluate(e4) == lv_UNKNOWN, "∀ 含 UNKNOWN 元素 → UNKNOWN");
        lv_quant_expr_destroy(e4);
    }
    {
        int elems[] = {7, 8, 9};
        lvDomain *df = lv_quant_domain_create_finite(14, elems, 3);
        lvQuantifiedExpr *e5 = mk_expr(5, lv_EXISTS, df, 8, 102);
        /* 三值 OR：FALSE ∨ TRUE ∨ UNKNOWN → TRUE（短路于 8） */
        TEST_ASSERT_MSG(lv_quant_expr_evaluate(e5) == lv_TRUE, "∃ 含 TRUE 元素 → TRUE");
        lv_quant_expr_destroy(e5);
    }
    {
        int elems[] = {7, 8, 9};
        lvDomain *df = lv_quant_domain_create_finite(15, elems, 3);
        lvQuantifiedExpr *e6 = mk_expr(6, lv_EXISTS_UNIQUE, df, 8, 103);
        /* 恰好一个 TRUE（元素 8），7/9 为 UNKNOWN → has_unknown → UNKNOWN */
        TEST_ASSERT_MSG(lv_quant_expr_evaluate(e6) == lv_UNKNOWN, "∃! 含 UNKNOWN → UNKNOWN");
        lv_quant_expr_destroy(e6);
    }

    /* 体命题前置条件区域命中 → TRUE（精确满足计数）
     * 注：直接构造 alias 字段（见 M6 缺陷登记） */
    {
        int elems[] = {7, 8};
        lvDomain *df = lv_quant_domain_create_finite(16, elems, 2);
        struct Proposition *body = proposition_create(110, PROPOSITION_TYPE_ATOMIC);
        body->precondition_region_ids = (int *) lv_malloc(sizeof(int));
        body->precondition_region_ids[0] = 8;
        body->precondition_region_count = 1;
        lvQuantifiedExpr *e7 = lv_quant_expr_create(7, lv_EXISTS_UNIQUE, "p", -1, df, body);
        TEST_ASSERT_MSG(e7 != NULL, "表达式创建");
        /* 元素 7：precondition 不命中、var_node_id=-1 不命中 → UNKNOWN；元素 8 命中 → TRUE。
         * has_unknown → UNKNOWN */
        TEST_ASSERT_MSG(lv_quant_expr_evaluate(e7) == lv_UNKNOWN, "∃! precondition 单命中但含 UNKNOWN → UNKNOWN");
        lv_quant_expr_destroy(e7);
    }

    /* 无限域：UNKNOWN + 不可消去 + count -1 */
    {
        lvDomain *di = lv_quant_domain_create(17, "R");
        lvQuantifiedExpr *e8 = mk_expr(8, lv_FORALL, di, 1, 104);
        TEST_ASSERT_MSG(lv_quant_expr_evaluate(e8) == lv_UNKNOWN, "无限域评估 UNKNOWN");
        TEST_ASSERT_MSG(!lv_quant_is_eliminable(e8), "无限域不可消去");
        TEST_ASSERT_MSG(lv_quant_count_satisfying(e8) == -1, "无限域 count -1");
        lv_quant_expr_destroy(e8);
    }

    /* NULL 安全 */
    TEST_ASSERT_MSG(lv_quant_expr_evaluate(NULL) == lv_UNKNOWN, "NULL expr 评估 UNKNOWN");
    TEST_ASSERT_MSG(!lv_quant_is_eliminable(NULL), "NULL 不可消去");
    TEST_ASSERT_MSG(lv_quant_count_satisfying(NULL) == -1, "NULL count -1");
    TEST_ASSERT_MSG(lv_quant_expr_create(99, lv_FORALL, "x", 1, NULL, NULL) == NULL, "NULL 域创建失败");
    lv_quant_expr_destroy(NULL);
}

/* ============================================================
 * 量词实例化 / 泛化 / 存在引入 / 存在消去
 * ============================================================ */
static void test_quant_inst_rules(void) {
    /* instantiate（∀E）：实例必须在域中 */
    {
        int elems[] = {5, 6};
        lvDomain *df = lv_quant_domain_create_finite(20, elems, 2);
        lvQuantifiedExpr *e = mk_expr(20, lv_FORALL, df, 5, 200);

        lvQuantifiedResult res;
        memset(&res, 0, sizeof(res));
        /* 实例 5 在域中 → OK */
        lvQuantResult r = lv_quantifier_instantiate(e, 5, &res);
        TEST_ASSERT_MSG(r == lv_QUANT_OK, "∀E 实例化成功");
        TEST_ASSERT_MSG(res.status == lv_QUANT_OK, "结果状态 OK");
        TEST_ASSERT_MSG(res.witness_node_id == 5, "目击者 5");
        TEST_ASSERT_MSG(res.truth_value == lv_TRUE, "体命题命中 → TRUE");
        TEST_ASSERT_MSG(res.result_prop != NULL, "结果命题非空");
        lv_quant_result_destroy(&res);

        /* 实例不在域中 → INVALID_VARIABLE */
        memset(&res, 0, sizeof(res));
        r = lv_quantifier_instantiate(e, 99, &res);
        TEST_ASSERT_MSG(r == lv_QUANT_INVALID_VARIABLE, "域外实例 INVALID_VARIABLE");

        /* 非 ∀ → INSTANTIATE_FAILED */
        lvQuantifiedExpr *e_exists = mk_expr(21, lv_EXISTS, lv_quant_domain_create_finite(21, elems, 2), 5, 201);
        memset(&res, 0, sizeof(res));
        r = lv_quantifier_instantiate(e_exists, 5, &res);
        TEST_ASSERT_MSG(r == lv_QUANT_INSTANTIATE_FAILED, "非 ∀ 实例化失败");

        lv_quant_result_destroy(&res);
        lv_quant_expr_destroy(e);
        lv_quant_expr_destroy(e_exists);
    }

    /* generalize（∀I）：有限域全部 TRUE → OK；含 FALSE → COUNTEREXAMPLE */
    {
        int elems[] = {5, 6};
        lvDomain *df = lv_quant_domain_create_finite(22, elems, 2);
        /* variable_node_id=5：元素 6 评估 UNKNOWN → 泛化 UNKNOWN → GENERALIZE_FAILED */
        lvQuantifiedExpr *e = mk_expr(22, lv_FORALL, df, 5, 202);
        lvQuantifiedResult res;
        memset(&res, 0, sizeof(res));
        lvQuantResult r = lv_quantifier_generalize(e, &res);
        TEST_ASSERT_MSG(r == lv_QUANT_GENERALIZE_FAILED, "含 UNKNOWN 泛化失败");
        lv_quant_result_destroy(&res);
        lv_quant_expr_destroy(e);
    }
    {
        int elems[] = {5};
        lvDomain *df = lv_quant_domain_create_finite(23, elems, 1);
        lvQuantifiedExpr *e = mk_expr(23, lv_FORALL, df, 5, 203);
        lvQuantifiedResult res;
        memset(&res, 0, sizeof(res));
        lvQuantResult r = lv_quantifier_generalize(e, &res);
        TEST_ASSERT_MSG(r == lv_QUANT_OK, "单元素域全 TRUE 泛化成功");
        lv_quant_result_destroy(&res);
        lv_quant_expr_destroy(e);
    }

    /* exists_introduce（∃I）：目击者满足体命题 → OK */
    {
        int elems[] = {5, 6};
        lvDomain *df = lv_quant_domain_create_finite(24, elems, 2);
        lvQuantifiedExpr *e = mk_expr(24, lv_EXISTS, df, 5, 204);
        lvQuantifiedResult res;
        memset(&res, 0, sizeof(res));
        lvQuantResult r = lv_quant_exists_introduce(e, 5, &res);
        TEST_ASSERT_MSG(r == lv_QUANT_OK, "∃I 引入成功");
        TEST_ASSERT_MSG(res.witness_node_id == 5, "目击者记录");
        lv_quant_result_destroy(&res);

        /* 目击者不满足（6 未命中）→ INSTANTIATE_FAILED */
        memset(&res, 0, sizeof(res));
        r = lv_quant_exists_introduce(e, 6, &res);
        TEST_ASSERT_MSG(r == lv_QUANT_INSTANTIATE_FAILED, "不满足目击者引入失败");
        lv_quant_result_destroy(&res);
        lv_quant_expr_destroy(e);
    }

    /* exists_eliminate（∃E）：有限域找到目击者 → OK */
    {
        int elems[] = {5, 6};
        lvDomain *df = lv_quant_domain_create_finite(25, elems, 2);
        lvQuantifiedExpr *e = mk_expr(25, lv_EXISTS, df, 5, 205);
        struct Proposition *target = proposition_create(300, PROPOSITION_TYPE_ATOMIC);
        lvQuantifiedResult res;
        memset(&res, 0, sizeof(res));
        lvQuantResult r = lv_quant_exists_eliminate(e, target, &res);
        TEST_ASSERT_MSG(r == lv_QUANT_OK, "∃E 消去成功");
        TEST_ASSERT_MSG(res.witness_node_id == 5, "目击者 5");
        lv_quant_result_destroy(&res);
        proposition_unref(target);
        lv_quant_expr_destroy(e);
    }
    {
        /* 域中无满足元素 → INSTANTIATE_FAILED */
        int elems[] = {7};
        lvDomain *df = lv_quant_domain_create_finite(26, elems, 1);
        lvQuantifiedExpr *e = mk_expr(26, lv_EXISTS, df, 5, 206);
        lvQuantifiedResult res;
        memset(&res, 0, sizeof(res));
        lvQuantResult r = lv_quant_exists_eliminate(e, NULL, &res);
        TEST_ASSERT_MSG(r == lv_QUANT_INSTANTIATE_FAILED, "无目击者消去失败");
        lv_quant_result_destroy(&res);
        lv_quant_expr_destroy(e);
    }

    /* NULL 安全 */
    lvQuantifiedResult res;
    memset(&res, 0, sizeof(res));
    TEST_ASSERT_MSG(lv_quantifier_instantiate(NULL, 1, &res) == lv_QUANT_ERROR, "NULL instantiate 报错");
    TEST_ASSERT_MSG(lv_quantifier_generalize(NULL, &res) == lv_QUANT_ERROR, "NULL generalize 报错");
    TEST_ASSERT_MSG(lv_quant_exists_introduce(NULL, 1, &res) == lv_QUANT_ERROR, "NULL ∃I 报错");
    TEST_ASSERT_MSG(lv_quant_exists_eliminate(NULL, NULL, &res) == lv_QUANT_ERROR, "NULL ∃E 报错");
    lv_quant_result_destroy(&res);
    lv_quant_result_destroy(NULL);
}

/* ============================================================
 * 有限域消去 + 满足计数 + 字符串映射
 * ============================================================ */
static void test_quant_eliminate(void) {
    /* forall 消去：有限域 AND 归约 */
    {
        int elems[] = {5, 6};
        lvDomain *df = lv_quant_domain_create_finite(30, elems, 2);
        lvQuantifiedExpr *e = mk_expr(30, lv_FORALL, df, 5, 300);
        lvQuantifiedResult res;
        memset(&res, 0, sizeof(res));
        lvQuantResult r = lv_quant_eliminate_forall_finite(e, &res);
        TEST_ASSERT_MSG(r == lv_QUANT_OK, "∀ 消去成功");
        TEST_ASSERT_MSG(res.result_prop != NULL, "消去结果命题");
        TEST_ASSERT_MSG(res.truth_value == lv_UNKNOWN, "∀ 含 UNKNOWN → UNKNOWN");
        lv_quant_result_destroy(&res);
        lv_quant_expr_destroy(e);
    }
    {
        /* 单元素域命中 → TRUE */
        int elems[] = {5};
        lvDomain *df = lv_quant_domain_create_finite(31, elems, 1);
        lvQuantifiedExpr *e = mk_expr(31, lv_FORALL, df, 5, 301);
        lvQuantifiedResult res;
        memset(&res, 0, sizeof(res));
        lvQuantResult r = lv_quant_eliminate_forall_finite(e, &res);
        TEST_ASSERT_MSG(r == lv_QUANT_OK, "∀ 单元素消去成功");
        TEST_ASSERT_MSG(res.truth_value == lv_TRUE, "∀ 单元素命中 → TRUE");
        lv_quant_result_destroy(&res);
        lv_quant_expr_destroy(e);
    }
    {
        /* 空域 ∀ → DOMAIN_EMPTY 且 truth TRUE */
        lvDomain *de = lv_quant_domain_create_finite(32, NULL, 0);
        lvQuantifiedExpr *e = mk_expr(32, lv_FORALL, de, 5, 302);
        lvQuantifiedResult res;
        memset(&res, 0, sizeof(res));
        lvQuantResult r = lv_quant_eliminate_forall_finite(e, &res);
        TEST_ASSERT_MSG(r == lv_QUANT_DOMAIN_EMPTY, "空域 ∀ 消去 DOMAIN_EMPTY");
        TEST_ASSERT_MSG(res.truth_value == lv_TRUE, "空域 ∀ 真值为 TRUE");
        lv_quant_result_destroy(&res);
        lv_quant_expr_destroy(e);
    }

    /* exists 消去：有限域 OR 归约 */
    {
        int elems[] = {5, 6};
        lvDomain *df = lv_quant_domain_create_finite(33, elems, 2);
        lvQuantifiedExpr *e = mk_expr(33, lv_EXISTS, df, 5, 303);
        lvQuantifiedResult res;
        memset(&res, 0, sizeof(res));
        lvQuantResult r = lv_quant_eliminate_exists_finite(e, &res);
        TEST_ASSERT_MSG(r == lv_QUANT_OK, "∃ 消去成功");
        TEST_ASSERT_MSG(res.truth_value == lv_TRUE, "∃ 含命中 → TRUE");
        TEST_ASSERT_MSG(res.witness_node_id == 5, "∃ 目击者 5");
        lv_quant_result_destroy(&res);
        lv_quant_expr_destroy(e);
    }
    {
        int elems[] = {7};
        lvDomain *df = lv_quant_domain_create_finite(34, elems, 1);
        lvQuantifiedExpr *e = mk_expr(34, lv_EXISTS, df, 5, 304);
        lvQuantifiedResult res;
        memset(&res, 0, sizeof(res));
        lvQuantResult r = lv_quant_eliminate_exists_finite(e, &res);
        TEST_ASSERT_MSG(r == lv_QUANT_OK, "∃ 无命中元素消去仍 OK");
        TEST_ASSERT_MSG(res.truth_value == lv_UNKNOWN, "∃ 无命中 → UNKNOWN");
        lv_quant_result_destroy(&res);
        lv_quant_expr_destroy(e);
    }

    /* exists_unique 消去 */
    {
        /* 单元素域恰好命中（元素 5 命中）→ TRUE
         * 注：未命中元素 evaluate 返回 UNKNOWN 而非 FALSE，多元素域必含 UNKNOWN → 结果 UNKNOWN */
        int elems[] = {5};
        lvDomain *df = lv_quant_domain_create_finite(35, elems, 1);
        lvQuantifiedExpr *e = mk_expr(35, lv_EXISTS_UNIQUE, df, 5, 305);
        lvQuantifiedResult res;
        memset(&res, 0, sizeof(res));
        lvQuantResult r = lv_quant_eliminate_exists_unique_finite(e, &res);
        TEST_ASSERT_MSG(r == lv_QUANT_OK, "∃! 消去成功");
        TEST_ASSERT_MSG(res.truth_value == lv_TRUE, "单元素命中 → TRUE");
        TEST_ASSERT_MSG(res.witness_node_id == 5, "∃! 目击者 5");
        lv_quant_result_destroy(&res);
        lv_quant_expr_destroy(e);
    }
    {
        /* 两个命中（元素 5/6 均命中）→ FALSE
         * 注：proposition_set_preconditions 只写 precondition_count，
         * 而 evaluate_body_for_element 读 precondition_region_count（alias，
         * 全库无赋值点 → M6 缺陷登记），故此处直接构造 alias 字段绕过。 */
        int elems[] = {5, 6};
        lvDomain *df = lv_quant_domain_create_finite(36, elems, 2);
        struct Proposition *body = proposition_create(306, PROPOSITION_TYPE_ATOMIC);
        int pre[] = {6};
        body->precondition_region_ids = (int *) lv_malloc(sizeof(int));
        body->precondition_region_ids[0] = 6;
        body->precondition_region_count = 1;
        lvQuantifiedExpr *e = lv_quant_expr_create(36, lv_EXISTS_UNIQUE, "x", 5, df, body);
        TEST_ASSERT_MSG(e != NULL, "表达式创建");
        lvQuantifiedResult res;
        memset(&res, 0, sizeof(res));
        lvQuantResult r = lv_quant_eliminate_exists_unique_finite(e, &res);
        TEST_ASSERT_MSG(r == lv_QUANT_OK, "∃! 双命中消去仍 OK");
        TEST_ASSERT_MSG(res.truth_value == lv_FALSE, "两个命中 → FALSE");
        lv_quant_result_destroy(&res);
        lv_quant_expr_destroy(e);
    }

    /* 无限域消去 → DOMAIN_INFINITE（每表达式独立域，避免所有权转移双 free） */
    {
        lvDomain *di = lv_quant_domain_create(37, "R");
        lvQuantifiedExpr *e = mk_expr(37, lv_FORALL, di, 5, 307);
        lvQuantifiedResult res;
        memset(&res, 0, sizeof(res));
        TEST_ASSERT_MSG(lv_quant_eliminate_forall_finite(e, &res) == lv_QUANT_DOMAIN_INFINITE, "无限域 ∀ 消去失败");
        lv_quant_result_destroy(&res);

        lvDomain *di2 = lv_quant_domain_create(38, "R");
        lvQuantifiedExpr *e2 = mk_expr(38, lv_EXISTS, di2, 5, 308);
        memset(&res, 0, sizeof(res));
        TEST_ASSERT_MSG(lv_quant_eliminate_exists_finite(e2, &res) == lv_QUANT_DOMAIN_INFINITE, "无限域 ∃ 消去失败");
        lv_quant_result_destroy(&res);
        lv_quant_expr_destroy(e);
        lv_quant_expr_destroy(e2);
    }

    /* 量词不匹配 → ERROR */
    {
        int elems[] = {5};
        lvDomain *df = lv_quant_domain_create_finite(39, elems, 1);
        lvQuantifiedExpr *e = mk_expr(39, lv_EXISTS, df, 5, 309);
        lvQuantifiedResult res;
        memset(&res, 0, sizeof(res));
        TEST_ASSERT_MSG(lv_quant_eliminate_forall_finite(e, &res) == lv_QUANT_ERROR, "∃ 用 ∀ 消去 → ERROR");
        lv_quant_result_destroy(&res);
        lv_quant_expr_destroy(e);
    }

    /* count_satisfying：有限域精确统计 */
    {
        int elems[] = {5, 6, 7};
        lvDomain *df = lv_quant_domain_create_finite(40, elems, 3);
        lvQuantifiedExpr *e = mk_expr(40, lv_FORALL, df, 5, 310);
        /* 命中：5；未命中：6、7（UNKNOWN 不计） → count 1 */
        TEST_ASSERT_MSG(lv_quant_count_satisfying(e) == 1, "满足计数 1");
        lv_quant_expr_destroy(e);
    }

    /* 字符串映射 */
    TEST_ASSERT_MSG(strcmp(lv_quant_to_string(lv_FORALL), "\xe2\x88\x80") == 0, "∀ 符号");
    TEST_ASSERT_MSG(strcmp(lv_quant_to_string(lv_EXISTS), "\xe2\x88\x83") == 0, "∃ 符号");
    TEST_ASSERT_MSG(strcmp(lv_quant_to_string(lv_EXISTS_UNIQUE), "\xe2\x88\x83!") == 0, "∃! 符号");
    TEST_ASSERT_MSG(lv_quant_to_string((lvQuantifier) 99) != NULL, "越界量词回退");
    TEST_ASSERT_MSG(lv_quant_result_to_string(lv_QUANT_OK) != NULL, "结果字符串");
    TEST_ASSERT_MSG(lv_quant_result_to_string((lvQuantResult) 99) != NULL, "越界结果回退");

    /* NULL 安全 */
    lvQuantifiedResult res;
    memset(&res, 0, sizeof(res));
    TEST_ASSERT_MSG(lv_quant_eliminate_forall_finite(NULL, &res) == lv_QUANT_ERROR, "NULL ∀ 消去报错");
    TEST_ASSERT_MSG(lv_quant_eliminate_exists_finite(NULL, &res) == lv_QUANT_ERROR, "NULL ∃ 消去报错");
    TEST_ASSERT_MSG(lv_quant_eliminate_exists_unique_finite(NULL, &res) == lv_QUANT_ERROR, "NULL ∃! 消去报错");
    lv_quant_result_destroy(&res);
}

/* ============================================================
 * 主入口
 * ============================================================ */
TEST_MAIN_BEGIN("Quantifier System")

    TEST_MAIN_RUN(test_quant_domain);
    TEST_MAIN_RUN(test_quant_expr_evaluate);
    TEST_MAIN_RUN(test_quant_inst_rules);
    TEST_MAIN_RUN(test_quant_eliminate);

TEST_MAIN_END()
