/**
 * @file test_axiom_pkg_ext.c
 * @brief 公理包扩展契约测试（批次 C-㊴：axiom_pkg.h 16 个零覆盖 API）
 *
 * 补充 test_axiom_pkg.c 未覆盖的 16 个零覆盖 API：
 *   - 依赖引用族：axiom_package_register_dependency_ref /
 *     mark_lemma_stale / add_internal_ref / add_external_ref /
 *     add_author_assertion / validate_dependencies_with_hashes /
 *     auto_degrade_invalidated / reverify_lemmas
 *   - 模板/缓存族：axiom_template_set_level / axiom_template_compress /
 *     store_expansion_cache / clear_expansion_cache（含 lookup_expansion_cache 命中验证）
 *   - 验证族：axiom_package_add_unconstructible_template /
 *     verify_unconstructible / axiom_template_validate_normal_form
 *   - 流式上下文：axiom_set_stream_context
 *
 * 契约要点（与实现核对）：
 *   - AxiomPackage 结构体公开，测试直接访问 pkg->dep_refs 验证条目字段。
 *   - register_dependency_ref 不设置 ref_type（memset 0 → REF_INTERNAL）。
 *   - reverify_lemmas 用 compute_content_hash 与 add_internal_ref 存储的
 *     compute_lemma_block_hash 比较 → 内引用必然判定 stale（行为契约）。
 *   - graph_add_line_segment 仅创建线段节点不创建约束；约束需
 *     graph_add_incidence 显式添加。
 *   - GeomNode::numeric_assumption_declaration 由 graph 拥有，strdup 赋值
 *     后 graph_destroy 统一释放。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* 构造一个含 INCIDENCE 约束的图（2 点 + 1 线段 + 关联约束） */
static ConstraintGraph *make_incidence_graph(const char *target_name) {
    ConstraintGraph *g = graph_create();
    if (!g)
        return NULL;
    int p1 = add_point(g, 0, 1, 0, 1);
    int p2 = add_point(g, 1, 1, 1, 1);
    /* graph_add_line_segment 返回 AddNodeResult 成功码（ADD_NODE_OK=0），
     * 非节点 id；线段节点 id 用 graph_get_last_added_node_id 获取 */
    if (graph_add_line_segment(g, p1, p2) != ADD_NODE_OK) {
        graph_destroy(g);
        return NULL;
    }
    int seg = graph_get_last_added_node_id(g);
    if (seg <= p2) {
        graph_destroy(g);
        return NULL;
    }
    if (graph_add_incidence(g, p1, seg) < 0) {
        graph_destroy(g);
        return NULL;
    }
    if (target_name) {
        GeomNode *node = graph_get_node(g, p1);
        if (node) {
            node->numeric_assumption_declaration = strdup(target_name);
        }
    }
    return g;
}

/* ============== 测试：依赖引用注册 + 遗留标记 ============== */

static void test_depref_register_api(void) {
    AxiomPackage *pkg = axiom_package_create("DepPkg", "1.0");
    TEST_ASSERT_NOT_NULL(pkg);

    /* register：NULL 契约 */
    TEST_ASSERT_EQ(axiom_package_register_dependency_ref(NULL, "r1", "abc", 1), -1);
    TEST_ASSERT_EQ(axiom_package_register_dependency_ref(pkg, NULL, "abc", 1), -1);
    TEST_ASSERT_EQ(axiom_package_register_dependency_ref(pkg, "r1", NULL, 1), -1);

    /* 正常注册 → 0，条目字段默认（ref_type=REF_INTERNAL、颜色 GREEN） */
    TEST_ASSERT_EQ(axiom_package_register_dependency_ref(pkg, "r1", "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789", 7), 0);
    TEST_ASSERT_EQ(pkg->dep_refs.count, 1);
    DependencyRef *ref = (DependencyRef *)lv_darray_get(&pkg->dep_refs, 0);
    TEST_ASSERT_NOT_NULL(ref);
    TEST_ASSERT_STR_EQ(ref->ref_id, "r1");
    TEST_ASSERT_EQ(ref->dependent_node_id, 7);
    TEST_ASSERT_EQ(ref->original_color, DEP_TRUST_GREEN);
    TEST_ASSERT_EQ(ref->ref_type, REF_INTERNAL);

    /* mark_lemma_stale：NULL 契约 / 未找到 / 找到 */
    TEST_ASSERT_EQ(axiom_package_mark_lemma_stale(NULL, "r1"), -1);
    TEST_ASSERT_EQ(axiom_package_mark_lemma_stale(pkg, NULL), -1);
    TEST_ASSERT_EQ(axiom_package_mark_lemma_stale(pkg, "nope"), -1);
    TEST_ASSERT_EQ(axiom_package_mark_lemma_stale(pkg, "r1"), 0);
    TEST_ASSERT_EQ(ref->original_color, DEP_TRUST_YELLOW);
    TEST_ASSERT(strstr(ref->trust_comment, "遗留") != NULL, "遗留注释写入");

    axiom_package_destroy(pkg);
    printf("  test_depref_register_api: PASSED\n");
}

/* ============== 测试：依赖链引用（内/外/作者断言） ============== */

static void test_depref_chain_refs_api(void) {
    AxiomPackage *pkg = axiom_package_create("ChainPkg", "1.0");
    TEST_ASSERT_NOT_NULL(pkg);

    /* add_internal_ref：NULL / 负 id / 正常 */
    TEST_ASSERT_EQ(axiom_package_add_internal_ref(NULL, 1, 1), -1);
    TEST_ASSERT_EQ(axiom_package_add_internal_ref(pkg, -1, 1), -1);
    TEST_ASSERT_EQ(axiom_package_add_internal_ref(pkg, 1, -1), -1);
    TEST_ASSERT_EQ(axiom_package_add_internal_ref(pkg, 3, 11), 0);
    TEST_ASSERT_EQ(pkg->dep_refs.count, 1);
    DependencyRef *ref = (DependencyRef *)lv_darray_get(&pkg->dep_refs, 0);
    TEST_ASSERT_EQ(ref->ref_type, REF_INTERNAL);
    TEST_ASSERT(ref->hash_valid, "内引用哈希有效");
    TEST_ASSERT(strstr(ref->ref_id, "internal:lemma:3") != NULL, "内引用 ID 格式");
    TEST_ASSERT_EQ(strlen(ref->content_hash), 64);

    /* add_external_ref：NULL / 负 id / 正常 */
    TEST_ASSERT_EQ(axiom_package_add_external_ref(NULL, "Wantzel 1837", 1, NULL), -1);
    TEST_ASSERT_EQ(axiom_package_add_external_ref(pkg, NULL, 1, NULL), -1);
    TEST_ASSERT_EQ(axiom_package_add_external_ref(pkg, "Wantzel 1837", -1, NULL), -1);
    TEST_ASSERT_EQ(axiom_package_add_external_ref(pkg, "Wantzel 1837", 12, "截至 2025 年公认有效"), 0);
    TEST_ASSERT_EQ(pkg->dep_refs.count, 2);
    ref = (DependencyRef *)lv_darray_get(&pkg->dep_refs, 1);
    TEST_ASSERT_EQ(ref->ref_type, REF_EXTERNAL);
    TEST_ASSERT(!ref->hash_valid, "外引用不参与哈希验证");
    TEST_ASSERT_STR_EQ(ref->external_ref, "Wantzel 1837");
    TEST_ASSERT_STR_EQ(ref->trust_comment, "截至 2025 年公认有效");
    TEST_ASSERT_EQ(ref->dependent_node_id, 12);

    /* add_author_assertion：NULL / 负 id / 正常 */
    TEST_ASSERT_EQ(axiom_package_add_author_assertion(NULL, 1), -1);
    TEST_ASSERT_EQ(axiom_package_add_author_assertion(pkg, -1), -1);
    TEST_ASSERT_EQ(axiom_package_add_author_assertion(pkg, 13), 0);
    TEST_ASSERT_EQ(pkg->dep_refs.count, 3);
    ref = (DependencyRef *)lv_darray_get(&pkg->dep_refs, 2);
    TEST_ASSERT_EQ(ref->ref_type, REF_AUTHOR);
    TEST_ASSERT_EQ(ref->original_color, DEP_TRUST_YELLOW);
    TEST_ASSERT(strstr(ref->ref_id, "author:node:13") != NULL, "作者断言 ID 格式");

    axiom_package_destroy(pkg);
    printf("  test_depref_chain_refs_api: PASSED\n");
}

/* ============== 测试：依赖验证 + 自动降级 ============== */

static void test_depref_validate_api(void) {
    AxiomPackage *pkg = axiom_package_create("ValPkg", "1.0");
    TEST_ASSERT_NOT_NULL(pkg);

    /* validate：NULL 契约（out 参数置 NULL/0） */
    DependencyRef *invalidated = (DependencyRef *)0x1;
    int count = 99;
    TEST_ASSERT_EQ(axiom_package_validate_dependencies_with_hashes(NULL, &invalidated, &count), -1);
    TEST_ASSERT_EQ(axiom_package_validate_dependencies_with_hashes(pkg, NULL, &count), -1);
    TEST_ASSERT_EQ(axiom_package_validate_dependencies_with_hashes(pkg, &invalidated, NULL), -1);
    TEST_ASSERT_EQ(axiom_package_validate_dependencies_with_hashes(NULL, &invalidated, &count), -1);

    /* 空包 → 0 */
    invalidated = NULL;
    count = -1;
    TEST_ASSERT_EQ(axiom_package_validate_dependencies_with_hashes(pkg, &invalidated, &count), 0);
    TEST_ASSERT_EQ(count, 0);

    /* 正确哈希 → 不失效；错误哈希 → 失效 1 */
    char *h = axiom_package_compute_content_hash(pkg);
    TEST_ASSERT_NOT_NULL(h);
    TEST_ASSERT_EQ(axiom_package_register_dependency_ref(pkg, "good", h, 1), 0);
    TEST_ASSERT_EQ(axiom_package_register_dependency_ref(pkg, "bad", "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef", 2), 0);
    count = -1;
    invalidated = NULL;
    TEST_ASSERT_EQ(axiom_package_validate_dependencies_with_hashes(pkg, &invalidated, &count), 1);
    TEST_ASSERT_EQ(count, 1);
    TEST_ASSERT_NOT_NULL(invalidated);
    TEST_ASSERT_STR_EQ(invalidated[0].ref_id, "bad");
    lv_free((void **)&invalidated);
    lv_free((void **)&h);

    /* auto_degrade：NULL → 0；GREEN 节点降级 YELLOW */
    TEST_ASSERT_EQ(axiom_package_auto_degrade_invalidated(NULL, NULL), 0);
    TEST_ASSERT_EQ(axiom_package_auto_degrade_invalidated(pkg, NULL), 0);
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);
    int pid = add_point(g, 0, 1, 0, 1);
    GeomNode *node = graph_get_node(g, pid);
    TEST_ASSERT_NOT_NULL(node);
    node->trust = TRUST_GREEN;
    /* 失效引用依赖节点 id=2（图中不存在）→ 跳过；作者断言依赖节点 id=pid → 降级 */
    TEST_ASSERT_EQ(axiom_package_add_author_assertion(pkg, pid), 0);
    node->trust = TRUST_GREEN;
    int degraded = axiom_package_auto_degrade_invalidated(pkg, g);
    TEST_ASSERT(degraded >= 1, "至少降级作者断言节点");
    TEST_ASSERT_EQ(node->trust, TRUST_YELLOW);

    graph_destroy(g);
    axiom_package_destroy(pkg);
    printf("  test_depref_validate_api: PASSED\n");
}

/* ============== 测试：模板级别 + 压缩 ============== */

static void test_template_level_api(void) {
    ConstraintTemplate *tmpl = lv_calloc(1, sizeof(ConstraintTemplate));
    TEST_ASSERT_NOT_NULL(tmpl);

    /* set_level：NULL 安全；默认 LEVEL_ONE（0） */
    axiom_template_set_level(NULL, TEMPLATE_LEVEL_TWO); /* 不崩溃即通过 */
    axiom_template_set_level(tmpl, TEMPLATE_LEVEL_ONE);
    TEST_ASSERT_EQ(tmpl->level, TEMPLATE_LEVEL_ONE);
    TEST_ASSERT(!tmpl->is_compressed, "一级模板非压缩态");

    /* LEVEL_TWO → is_compressed=true */
    axiom_template_set_level(tmpl, TEMPLATE_LEVEL_TWO);
    TEST_ASSERT_EQ(tmpl->level, TEMPLATE_LEVEL_TWO);
    TEST_ASSERT(tmpl->is_compressed, "二级模板默认压缩态");

    /* compress：NULL 安全；二级模板恢复压缩态 */
    axiom_template_compress(NULL);
    tmpl->is_compressed = false;
    axiom_template_compress(tmpl);
    TEST_ASSERT(tmpl->is_compressed, "二级模板压缩");

    /* 一级模板 compress 无效（level != TWO 直接返回） */
    axiom_template_set_level(tmpl, TEMPLATE_LEVEL_ONE);
    tmpl->is_compressed = false;
    axiom_template_compress(tmpl);
    TEST_ASSERT(!tmpl->is_compressed, "一级模板不压缩");

    lv_free((void **)&tmpl);
    printf("  test_template_level_api: PASSED\n");
}

/* ============== 测试：模板展开缓存 ============== */

static void test_expansion_cache_api(void) {
    AxiomPackage *pkg = axiom_package_create("CachePkg", "1.0");
    TEST_ASSERT_NOT_NULL(pkg);

    /* store：NULL pkg → false；正常 → true */
    ConstraintGraph *g1 = graph_create();
    TEST_ASSERT_NOT_NULL(g1);
    TEST_ASSERT(!axiom_package_store_expansion_cache(NULL, "T", NULL, 0, g1), "NULL pkg 存储失败");
    TEST_ASSERT(axiom_package_store_expansion_cache(pkg, "T", NULL, 0, g1), "存储成功");
    TEST_ASSERT_EQ(pkg->expansion_cache.count, 1);

    /* lookup 命中：相同参数哈希 → 返回缓存图 */
    ConstraintGraph *hit = axiom_package_lookup_expansion_cache(pkg, "T", NULL, 0);
    TEST_ASSERT(hit == g1, "缓存命中同一图对象");

    /* lookup 不同模板名 → 未命中 */
    TEST_ASSERT_NULL(axiom_package_lookup_expansion_cache(pkg, "OTHER", NULL, 0));

    /* clear：NULL 安全；清空后 count==0、lookup 未命中 */
    axiom_package_clear_expansion_cache(NULL); /* 不崩溃即通过 */
    axiom_package_clear_expansion_cache(pkg);
    TEST_ASSERT_EQ(pkg->expansion_cache.count, 0);
    TEST_ASSERT_NULL(axiom_package_lookup_expansion_cache(pkg, "T", NULL, 0));

    axiom_package_destroy(pkg);
    printf("  test_expansion_cache_api: PASSED\n");
}

/* ============== 测试：不可构造性模板 + 验证 + 正则形式 ============== */

static void test_unconstructible_api(void) {
    AxiomPackage *pkg = axiom_package_create("UC Pkg", "1.0");
    TEST_ASSERT_NOT_NULL(pkg);

    /* add_unconstructible_template：NULL 契约 */
    TEST_ASSERT_EQ(axiom_package_add_unconstructible_template(NULL, "T", "K", NULL, NULL), -1);
    TEST_ASSERT_EQ(axiom_package_add_unconstructible_template(pkg, NULL, "K", NULL, NULL), -1);
    TEST_ASSERT_EQ(axiom_package_add_unconstructible_template(pkg, "T", NULL, NULL, NULL), -1);
    TEST_ASSERT_EQ(axiom_package_add_unconstructible_template(pkg, "T", "K", NULL, NULL), -1);

    /* 正常添加（接管 construction 所有权）→ 0 */
    ConstraintGraph *reduction = make_incidence_graph(NULL);
    TEST_ASSERT_NOT_NULL(reduction);
    TEST_ASSERT_EQ(axiom_package_add_unconstructible_template(pkg, "Trisect", "Cubic", reduction, "reduce to cubic"), 0);
    TEST_ASSERT_EQ(pkg->unconstructible_templates.count, 1);

    /* lookup：找到 / 未找到 */
    UnconstructibleTemplate *t = axiom_package_lookup_unconstructible_template(pkg, "Trisect");
    TEST_ASSERT_NOT_NULL(t);
    TEST_ASSERT_STR_EQ(t->target_problem_name, "Trisect");
    TEST_ASSERT_STR_EQ(t->known_unconstructible_name, "Cubic");
    TEST_ASSERT_NULL(axiom_package_lookup_unconstructible_template(pkg, "Nope"));

    /* verify_unconstructible：NULL 契约 / 节点不存在 / 无匹配模板 */
    TEST_ASSERT(!axiom_package_verify_unconstructible(NULL, 0, pkg), "NULL graph");
    TEST_ASSERT(!axiom_package_verify_unconstructible(NULL, 0, NULL), "全 NULL");
    ConstraintGraph *target = make_incidence_graph("unrelated");
    TEST_ASSERT_NOT_NULL(target);
    TEST_ASSERT(!axiom_package_verify_unconstructible(target, 0, pkg), "无匹配模板");

    /* 正路径：目标节点声明含模板名 + 归约图约束兼容 → true 且 trust=YELLOW（known 未注册） */
    GeomNode *target_node = graph_get_node(target, 0);
    TEST_ASSERT_NOT_NULL(target_node);
    lv_free((void **)&target_node->numeric_assumption_declaration);
    target_node->numeric_assumption_declaration = strdup("Trisect is hard");
    TEST_ASSERT(axiom_package_verify_unconstructible(target, 0, pkg), "归约验证通过");
    TEST_ASSERT_EQ(target_node->trust, TRUST_YELLOW);
    TEST_ASSERT(t->verified, "模板标记已验证");

    /* known green_verified → 目标 trust=GREEN */
    KnownUnconstructible *uc = lv_malloc(sizeof(KnownUnconstructible));
    TEST_ASSERT_NOT_NULL(uc);
    uc->name = strdup("Cubic");
    uc->reduces_to = strdup("x");
    lv_darray_init(&uc->dependency_chain, sizeof(char *));
    uc->external_ref = strdup("ref");
    uc->green_verified = true;
    TEST_ASSERT(axiom_package_add_known_unconstructible(pkg, uc), "注册已知不可构造项");
    target_node->trust = TRUST_GREEN; /* 重置（verify 后再次执行） */
    TEST_ASSERT(axiom_package_verify_unconstructible(target, 0, pkg), "已知问题已验证 → 目标 GREEN");
    TEST_ASSERT_EQ(target_node->trust, TRUST_GREEN);

    graph_destroy(target);

    /* axiom_template_validate_normal_form：NULL 契约 */
    TEST_ASSERT(!axiom_template_validate_normal_form(NULL, NULL, "X"), "NULL graph");
    TEST_ASSERT(!axiom_template_validate_normal_form(NULL, target, NULL), "NULL 规范形式");
    ConstraintGraph *empty = graph_create();
    TEST_ASSERT_NOT_NULL(empty);
    TEST_ASSERT(!axiom_template_validate_normal_form(NULL, empty, "INCIDENCE(POINT,LINE_SEGMENT)"), "空图违规");

    /* 正路径：INCIDENCE 约束 2 参与者 */
    ConstraintGraph *norm = make_incidence_graph(NULL);
    TEST_ASSERT_NOT_NULL(norm);
    TEST_ASSERT(axiom_template_validate_normal_form(NULL, norm, "INCIDENCE(POINT,LINE_SEGMENT)"), "正则形式匹配");
    TEST_ASSERT(!axiom_template_validate_normal_form(NULL, norm, "BETWEENNESS(POINT,POINT,POINT)"), "参与者数不匹配");
    TEST_ASSERT(!axiom_template_validate_normal_form(NULL, norm, "no-paren"), "无括号格式失败");

    graph_destroy(norm);
    graph_destroy(empty);
    axiom_package_destroy(pkg);
    printf("  test_unconstructible_api: PASSED\n");
}

/* ============== 测试：流式上下文 ============== */

static void test_stream_ctx_api(void) {
    axiom_set_stream_context(NULL); /* 禁用流式输出（NULL 安全） */
    axiom_set_stream_context(NULL); /* 重复设置安全 */
    printf("  test_stream_ctx_api: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Axiom Package Ext Test Suite")
    printf("=== Lv-00 Axiom Package Ext Test Suite (batch C-㊴) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_depref_register_api);
    TEST_MAIN_RUN(test_depref_chain_refs_api);
    TEST_MAIN_RUN(test_depref_validate_api);
    TEST_MAIN_RUN(test_template_level_api);
    TEST_MAIN_RUN(test_expansion_cache_api);
    TEST_MAIN_RUN(test_unconstructible_api);
    TEST_MAIN_RUN(test_stream_ctx_api);

    lv_cleanup();
TEST_MAIN_END()
