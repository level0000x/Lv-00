/**
 * @file test_axiom_grade_ext.c
 * @brief 公理分级系统契约测试（批次 C-㊺续26：axiom_grade.h 9 个零覆盖 API）
 *
 * 覆盖零覆盖 API：
 *   难度过滤：set_difficulty / get_filter / unlock_next_grade / grade_check
 *   元数据：grade_meta_create / grade_meta_destroy
 *   风格筛选：filter_by_style
 *   字符串：grade_to_string / proof_style_to_string
 *
 * 契约要点（与实现核对）：
 *   - 默认过滤器：min=BASIC、max=INTERMEDIATE、filter_enabled=true、
 *     current_level=1。
 *   - set_difficulty：设 max_grade + 启用过滤 + min 不超 max。
 *   - grade_check：NULL → false；未启用全过；is_required 恒过；
 *     grade ∈ [min,max] 通过。
 *   - unlock_next_grade：level+1 并同步 max_grade；EXPERt 不再变。
 *   - meta_create：name NULL → NULL；description 深拷贝。
 *   - filter_by_style：匹配索引写入，matched 可能 > max_out。
 *   - to_string：中文名 + 越界"未知"。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/axiom_grade.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：难度过滤 ============== */

static void test_axiom_difficulty_api(void) {
    /* 默认过滤器 */
    const lvAxiomGradeFilter *f = lv_axiom_get_filter();
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQ((int) f->min_grade, (int) GRADE_BASIC);
    TEST_ASSERT_EQ((int) f->max_grade, (int) GRADE_INTERMEDIATE);
    TEST_ASSERT(f->filter_enabled, "默认启用过滤");

    /* set_difficulty：max=ADVANCED */
    lv_axiom_set_difficulty(GRADE_ADVANCED);
    f = lv_axiom_get_filter();
    TEST_ASSERT_EQ((int) f->max_grade, (int) GRADE_ADVANCED);
    TEST_ASSERT(f->filter_enabled, "设置后启用");

    /* set_difficulty 低于当前 min：min 同步降低 */
    lv_axiom_set_difficulty(GRADE_BASIC);
    f = lv_axiom_get_filter();
    TEST_ASSERT_EQ((int) f->max_grade, (int) GRADE_BASIC);
    TEST_ASSERT_EQ((int) f->min_grade, (int) GRADE_BASIC);

    /* grade_check */
    lvAxiomGradeMeta basic = {"b", GRADE_BASIC, STYLE_FORWARD, 0, NULL, false};
    lvAxiomGradeMeta expert = {"e", GRADE_EXPERT, STYLE_INDUCTION, 0, NULL, false};
    lvAxiomGradeMeta required = {"r", GRADE_EXPERT, STYLE_BACKWARD, 0, NULL, true};

    TEST_ASSERT(lv_axiom_grade_check(&basic), "BASIC 通过");
    TEST_ASSERT(!lv_axiom_grade_check(&expert), "EXPERT 不通过");
    TEST_ASSERT(lv_axiom_grade_check(&required), "必修恒通过");
    TEST_ASSERT(!lv_axiom_grade_check(NULL), "NULL 不通过");

    printf("  test_axiom_difficulty_api: PASSED\n");
}

/* ============== 测试：元数据 ============== */

static void test_axiom_meta_api(void) {
    /* create：复制 name + description 深拷贝 */
    lvAxiomGradeMeta *m = lv_axiom_grade_meta_create("axiom_1", GRADE_ADVANCED, STYLE_CONTRADICTION, "教学描述");
    TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT(strcmp(m->axiom_name, "axiom_1") == 0, "名称复制");
    TEST_ASSERT_EQ((int) m->grade, (int) GRADE_ADVANCED);
    TEST_ASSERT_EQ((int) m->style, (int) STYLE_CONTRADICTION);
    TEST_ASSERT_EQ(m->prerequisite_count, 0);
    TEST_ASSERT(!m->is_required, "默认非必修");
    TEST_ASSERT_NOT_NULL(m->description);
    TEST_ASSERT(strcmp(m->description, "教学描述") == 0, "描述复制");

    /* description NULL 安全 */
    lvAxiomGradeMeta *m2 = lv_axiom_grade_meta_create("axiom_2", GRADE_BASIC, STYLE_FORWARD, NULL);
    TEST_ASSERT_NOT_NULL(m2);
    TEST_ASSERT_NULL(m2->description);
    lv_axiom_grade_meta_destroy(m2);

    /* name NULL → NULL */
    TEST_ASSERT_NULL(lv_axiom_grade_meta_create(NULL, GRADE_BASIC, STYLE_FORWARD, "d"));

    /* 长名称截断（<128） */
    char long_name[200];
    memset(long_name, 'A', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';
    lvAxiomGradeMeta *m3 = lv_axiom_grade_meta_create(long_name, GRADE_BASIC, STYLE_FORWARD, NULL);
    TEST_ASSERT_NOT_NULL(m3);
    TEST_ASSERT(strlen(m3->axiom_name) < sizeof(m3->axiom_name), "名称截断");
    lv_axiom_grade_meta_destroy(m3);

    /* destroy NULL 安全 */
    lv_axiom_grade_meta_destroy(NULL);
    lv_axiom_grade_meta_destroy(m);
    printf("  test_axiom_meta_api: PASSED\n");
}

/* ============== 测试：风格筛选与解锁 ============== */

static void test_axiom_style_unlock_api(void) {
    /* 构造 4 个元数据，风格分布 */
    lvAxiomGradeMeta metas[4];
    metas[0] = (lvAxiomGradeMeta) {"a0", GRADE_BASIC, STYLE_FORWARD, 0, NULL, false};
    metas[1] = (lvAxiomGradeMeta) {"a1", GRADE_BASIC, STYLE_BACKWARD, 0, NULL, false};
    metas[2] = (lvAxiomGradeMeta) {"a2", GRADE_BASIC, STYLE_FORWARD, 0, NULL, false};
    metas[3] = (lvAxiomGradeMeta) {"a3", GRADE_BASIC, STYLE_INDUCTION, 0, NULL, false};

    /* filter_by_style：FORWARD 匹配 0,2 */
    int indices[4];
    int n = lv_axiom_filter_by_style(metas, 4, STYLE_FORWARD, indices, 4);
    TEST_ASSERT_EQ(n, 2);
    TEST_ASSERT_EQ(indices[0], 0);
    TEST_ASSERT_EQ(indices[1], 2);

    /* INDUCTION 匹配 3 */
    n = lv_axiom_filter_by_style(metas, 4, STYLE_INDUCTION, indices, 4);
    TEST_ASSERT_EQ(n, 1);
    TEST_ASSERT_EQ(indices[0], 3);

    /* max_out 限制：只写前 max_out 个，但 matched 完整 */
    n = lv_axiom_filter_by_style(metas, 4, STYLE_FORWARD, indices, 1);
    TEST_ASSERT_EQ(n, 2); /* 返回完整匹配数 */
    TEST_ASSERT_EQ(indices[0], 0);

    /* 无效参数 → 0 */
    TEST_ASSERT_EQ(lv_axiom_filter_by_style(NULL, 4, STYLE_FORWARD, indices, 4), 0);
    TEST_ASSERT_EQ(lv_axiom_filter_by_style(metas, 0, STYLE_FORWARD, indices, 4), 0);
    TEST_ASSERT_EQ(lv_axiom_filter_by_style(metas, 4, STYLE_FORWARD, NULL, 4), 0);
    TEST_ASSERT_EQ(lv_axiom_filter_by_style(metas, 4, STYLE_FORWARD, indices, 0), 0);

    /* unlock_next_grade：默认 level=1 → ADVANCED(2) → EXPERT(3) → 不变 */
    lv_axiom_set_difficulty(GRADE_BASIC); /* 重置 max=BASIC，level 不变 */
    const lvAxiomGradeFilter *f = lv_axiom_get_filter();
    (void) f;
    /* 先把 level 推回 1（默认）——由于全局单例可能被前序测试改动，直接解锁到顶 */
    lvAxiomGrade g = lv_axiom_unlock_next_grade();
    TEST_ASSERT((int) g >= (int) GRADE_INTERMEDIATE, "解锁后至少中级");
    g = lv_axiom_unlock_next_grade();
    TEST_ASSERT((int) g >= (int) GRADE_ADVANCED, "二次解锁至少高级");
    g = lv_axiom_unlock_next_grade();
    TEST_ASSERT_EQ((int) g, (int) GRADE_EXPERT);
    g = lv_axiom_unlock_next_grade();
    TEST_ASSERT_EQ((int) g, (int) GRADE_EXPERT); /* 到顶不变 */
    /* 同步 max_grade */
    TEST_ASSERT_EQ((int) lv_axiom_get_filter()->max_grade, (int) GRADE_EXPERT);

    /* 解锁到 EXPERT 后 grade_check 全过 */
    TEST_ASSERT(lv_axiom_grade_check(&metas[0]), "解锁后 BASIC 通过");
    printf("  test_axiom_style_unlock_api: PASSED\n");
}

/* ============== 测试：字符串 ============== */

static void test_axiom_string_api(void) {
    /* grade_to_string */
    TEST_ASSERT(strcmp(lv_axiom_grade_to_string(GRADE_BASIC), "\xe5\x9f\xba\xe7\xa1\x80\xe7\xba\xa7") == 0, "基础级");
    TEST_ASSERT(strcmp(lv_axiom_grade_to_string(GRADE_EXPERT), "\xe4\xb8\x93\xe5\xae\xb6\xe7\xba\xa7") == 0, "专家级");
    TEST_ASSERT(strcmp(lv_axiom_grade_to_string((lvAxiomGrade) 99), "\xe6\x9c\xaa\xe7\x9f\xa5\xe7\xad\x89\xe7\xba\xa7") == 0,
                "越界未知等级");

    /* proof_style_to_string */
    TEST_ASSERT(strcmp(lv_proof_style_to_string(STYLE_FORWARD), "\xe6\xad\xa3\xe5\x90\x91\xe6\x8e\xa8\xe7\x90\x86") == 0,
                "正向推理");
    TEST_ASSERT(strcmp(lv_proof_style_to_string((lvProofStyle) 99), "\xe6\x9c\xaa\xe7\x9f\xa5\xe9\xa3\x8e\xe6\xa0\xbc") == 0,
                "越界未知风格");

    printf("  test_axiom_string_api: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Axiom Grade Ext Test Suite")
    printf("=== Lv-00 Axiom Grade Ext Test Suite (batch C-㊺续26) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_axiom_difficulty_api);
    TEST_MAIN_RUN(test_axiom_meta_api);
    TEST_MAIN_RUN(test_axiom_style_unlock_api);
    TEST_MAIN_RUN(test_axiom_string_api);

    lv_cleanup();
TEST_MAIN_END()
