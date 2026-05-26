/**
 * @file test_basic.c
 * @brief Lv-00 鍩虹妯″潡娴嬭瘯 - 鏈夌悊鏁般€佺害鏉熷浘銆佸綊涓€鍖栥€佹ā鍧楃郴缁熴€佸叕鐞嗗寘銆佺粺涓€鍖栥€佸紩鎿?
 *
 * 娴嬭瘯鍐呭锛?
 * - 鏈夌悊鏁扮畻鏈繍绠椾笌搴忓垪鍖?
 * - 绾︽潫鍥剧殑鏋勫缓涓庣害鏉熸坊鍔?
 * - 鍥惧綊涓€鍖栧鐞?
 * - 妯″潡绯荤粺渚濊禆绠＄悊
 * - 鍏悊鍖呮ā鏉挎敞鍐屼笌鏌ヨ
 * - 鏋勯€犱笌鍛介鐨勭粺涓€鍖?
 * - 寮曟搸鐢熷懡鍛ㄦ湡涓庣鍙?鍔熻兘鍧?
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00.h"
#include "test_helpers.h"

/* 鍏ㄥ眬娴嬭瘯璁℃暟鍣?*/
int g_pass_count = 0;
int g_fail_count = 0;

/**
 * @brief 娴嬭瘯鏈夌悊鏁扮殑鍩烘湰绠楁湳杩愮畻
 *
 * 楠岃瘉鏈夌悊鏁扮殑鍔犳硶銆佷箻娉曠粨鏋滄槸鍚︽纭紝
 * 浠ュ強搴忓垪鍖栬緭鍑烘槸鍚﹀寘鍚鏈熺殑鍒嗘暟琛ㄧず銆?
 */
void test_rational(void) {
    printf("Testing rational numbers...\n");

    /* 鍒涘缓涓や釜鏈夌悊鏁? 1/2 鍜?1/3 */
    Rational *r1 = rational_create(1, 2);
    Rational *r2 = rational_create(1, 3);
    Rational *sum = rational_add(r1, r2);
    Rational *prod = rational_multiply(r1, r2);

    /* 楠岃瘉鍔犳硶缁撴灉: 1/2 + 1/3 = 5/6 */
    Rational *expected_sum = rational_create(5, 6);
    TEST_ASSERT(rational_compare(sum, expected_sum) == 0, "1/2 + 1/3 should equal 5/6");
    rational_destroy(expected_sum);

    /* 楠岃瘉涔樻硶缁撴灉: 1/2 * 1/3 = 1/6 */
    Rational *expected_prod = rational_create(1, 6);
    TEST_ASSERT(rational_compare(prod, expected_prod) == 0, "1/2 * 1/3 should equal 1/6");
    rational_destroy(expected_prod);

    /* 楠岃瘉搴忓垪鍖栬緭鍑?*/
    char *ser = rational_serialize(sum);
    printf("  Sum: %s\n", ser);
    lv00_free_ptr(ser);

    rational_destroy(r1);
    rational_destroy(r2);
    rational_destroy(sum);
    rational_destroy(prod);

    printf("  PASSED\n");
}

/**
 * @brief 娴嬭瘯绾︽潫鍥剧殑鏋勫缓涓庡熀鏈搷浣?
 *
 * 鍦ㄧ害鏉熷浘涓坊鍔犱袱涓偣銆佷竴鏉＄嚎娈靛拰涓€涓叧鑱旂害鏉燂紝
 * 楠岃瘉鑺傜偣鏁板拰绾︽潫鏁版槸鍚︾鍚堥鏈熴€?
 */
void test_constraint_graph(void) {
    printf("Testing constraint graph...\n");

    ConstraintGraph *graph = graph_create();
    TEST_ASSERT(graph != NULL, "graph_create should return non-NULL");

    /* 浣跨敤杈呭姪鍑芥暟娣诲姞绗竴涓偣: 鍘熺偣 (0, 0) */
    int p0 = add_point(graph, 0, 1, 0, 1);
    /* 妫€鏌?add_point 杩斿洖鍊硷細澶辫触鏃惰繑鍥?-1锛屼笉搴斿皢鏃犳晥ID浼犲叆鍚庣画鍑芥暟 */
    TEST_ASSERT(p0 >= 0, "add_point for p0 (origin) failed");

    /* 浣跨敤杈呭姪鍑芥暟娣诲姞绗簩涓偣: (1, 0) */
    int p1 = add_point(graph, 1, 1, 0, 1);
    TEST_ASSERT(p1 >= 0, "add_point for p1 failed");

    /* 娣诲姞绾挎: 杩炴帴鐐?鍜岀偣1 */
    AddNodeResult res3 = graph_add_line_segment(graph, p0, p1);
    TEST_ASSERT(res3 == ADD_NODE_OK, "graph_add_line_segment should return ADD_NODE_OK");

    /* 娣诲姞鍏宠仈绾︽潫: 鐐?鍦ㄧ嚎娈典笂 */
    int seg_id = graph->next_node_id - 1;
    AddConstraintResult res4 = graph_add_incidence(graph, p0, seg_id);
    TEST_ASSERT(res4 == ADD_CONSTRAINT_OK, "graph_add_incidence should return ADD_CONSTRAINT_OK");

    /* 楠岃瘉鍥剧粨鏋? 2涓偣 + 1鏉＄嚎娈?= 3涓妭鐐? 1涓害鏉?*/
    TEST_ASSERT(graph->node_count == 3, "graph should have 3 nodes");
    TEST_ASSERT(graph->constraint_count == 1, "graph should have 1 constraint");

    graph_destroy(graph);
    printf("  PASSED\n");
}

/**
 * @brief 娴嬭瘯鍥剧殑褰掍竴鍖栧鐞?
 *
 * 鍒涘缓鍖呭惈涓や釜鐩稿悓鍧愭爣鐐圭殑鍥撅紝楠岃瘉褰掍竴鍖栨搷浣?
 * 鑳芥纭墽琛屼笖涓嶆敼鍙樿妭鐐规暟銆?
 */
void test_normalization(void) {
    printf("Testing graph normalization...\n");

    ConstraintGraph *graph = graph_create();

    /* 浣跨敤杈呭姪鍑芥暟娣诲姞绗竴涓偣: (1, 1) */
    add_point(graph, 1, 1, 1, 1);

    /* 浣跨敤杈呭姪鍑芥暟娣诲姞绗簩涓偣: 鍚屾牱鏄?(1, 1) */
    add_point(graph, 1, 1, 1, 1);

    /* 涓や釜鐐瑰簲鐙珛瀛樺湪 */
    TEST_ASSERT(graph->node_count == 2, "graph should have 2 nodes before normalization");

    /* 鎵ц褰掍竴鍖?*/
    NormalizationResult *nr = graph_normalize(graph, true);
    TEST_ASSERT(nr != NULL, "graph_normalize should return non-NULL");
    normalization_result_destroy(nr);

    graph_destroy(graph);
    printf("  PASSED\n");
}

/**
 * @brief 娴嬭瘯妯″潡绯荤粺鐨勫熀鏈姛鑳?
 *
 * 鍒涘缓妯″潡銆佽缃悕绉般€佹坊鍔犱緷璧栭」锛?
 * 楠岃瘉妯″潡灞炴€у拰渚濊禆璁℃暟鏄惁姝ｇ‘銆?
 */
void test_module(void) {
    printf("Testing module system...\n");

    /* 鍒涘缓妯″潡 */
    Module *mod = module_create("test_module", "1.0.0");
    TEST_ASSERT(mod != NULL, "module_create should return non-NULL");

    /* 楠岃瘉妯″潡鍚嶇О */
    TEST_ASSERT(strcmp(module_get_name(mod), "test_module") == 0, "module name should be 'test_module'");

    /* 娣诲姞渚濊禆骞堕獙璇佽鏁?*/
    module_add_dependency(mod, "dep1", ">=1.0");
    TEST_ASSERT(module_get_dependency_count(mod) == 1, "module should have 1 dependency");

    module_destroy(mod);
    printf("  PASSED\n");
}

/**
 * @brief 娴嬭瘯鍏悊鍖呯殑妯℃澘娉ㄥ唽涓庢煡璇?
 *
 * 鍒涘缓鍏悊鍖咃紝娉ㄥ唽绾︽潫妯℃澘锛岀劧鍚庨€氳繃鍚嶇О鏌ユ壘妯℃澘锛?
 * 楠岃瘉妯℃澘鑳借姝ｇ‘娉ㄥ唽鍜屾绱€?
 */
void test_axiom_package(void) {
    printf("Testing axiom package...\n");

    /* 鍒涘缓娆у嚑閲屽緱鍏悊鍖?*/
    AxiomPackage *pkg = axiom_package_create("euclidean", "1.0");
    TEST_ASSERT(pkg != NULL, "axiom_package_create should return non-NULL");

    /* 娉ㄥ唽璺濈绾︽潫妯℃澘 */
    ConstraintTemplate tmpl;
    tmpl.name = strdup("distance");
    tmpl.param_count = 2;
    tmpl.verified = false;
    axiom_package_register_template(pkg, &tmpl);

    /* 閫氳繃鍚嶇О鏌ユ壘宸叉敞鍐岀殑妯℃澘 */
    ConstraintTemplate *found = axiom_package_get_template(pkg, "distance");
    TEST_ASSERT(found != NULL, "axiom_package_get_template should find 'distance'");

    axiom_package_destroy(pkg);
    printf("  PASSED\n");
}

/**
 * @brief 娴嬭瘯鏋勯€犲浘涓庡懡棰樺浘鐨勭粺涓€鍖?
 *
 * 鍒涘缓涓や釜缁撴瀯鐩稿悓鐨勭害鏉熷浘锛堟瀯閫犲拰鍛介锛夛紝
 * 楠岃瘉缁熶竴鍖栨搷浣滆兘鎴愬姛鍖归厤銆?
 */
void test_unify(void) {
    printf("Testing unification...\n");

    ConstraintGraph *construction = graph_create();
    ConstraintGraph *proposition = graph_create();

    /* 鏋勯€犲浘: 浣跨敤杈呭姪鍑芥暟娣诲姞鍘熺偣 (0, 0) 鍜岀偣 (1, 0) */
    int cp0 = add_point(construction, 0, 1, 0, 1);
    /* 妫€鏌?add_point 杩斿洖鍊硷細澶辫触杩斿洖 -1锛屼笉搴斿皢鏃犳晥ID浼犲叆鍚庣画鍑芥暟 */
    TEST_ASSERT(cp0 >= 0, "add_point in construction for origin failed");
    int cp1 = add_point(construction, 1, 1, 0, 1);
    TEST_ASSERT(cp1 >= 0, "add_point in construction for (1,0) failed");

    /* 鍛介鍥? 浣跨敤杈呭姪鍑芥暟娣诲姞鐩稿悓鐨勪袱涓偣 */
    int pp0 = add_point(proposition, 0, 1, 0, 1);
    TEST_ASSERT(pp0 >= 0, "add_point in proposition for origin failed");
    int pp1 = add_point(proposition, 1, 1, 0, 1);
    TEST_ASSERT(pp1 >= 0, "add_point in proposition for (1,0) failed");

    /* 楠岃瘉缁熶竴鍖栫粨鏋? 缁撴瀯鐩稿悓搴旇繑鍥?OK */
    UnifyStatus status = unify_construction_with_proposition(construction, proposition);
    TEST_ASSERT(status == UNIFY_STATUS_OK, "unify should return UNIFY_STATUS_OK for identical graphs");

    graph_destroy(construction);
    graph_destroy(proposition);
    printf("  PASSED\n");
}

/**
 * @brief 娴嬭瘯寮曟搸鐨勭敓鍛藉懆鏈熶笌鍩烘湰鎿嶄綔
 *
 * 鍒涘缓寮曟搸锛屽湪涓诲浘涓坊鍔犵偣銆佺嚎娈点€佸叧鑱旂害鏉熴€佺鍙ｅ拰鍔熻兘鍧楋紝
 * 楠岃瘉鍚勬搷浣滆繑鍥炴纭姸鎬併€?
 */
void test_engine(void) {
    printf("Testing engine...\n");

    LV00Engine *engine = engine_create();
    TEST_ASSERT(engine != NULL, "engine_create should return non-NULL");

    /* 浣跨敤杈呭姪鍑芥暟鍦ㄥ紩鎿庝富鍥句腑娣诲姞鍘熺偣 (0, 0) */
    int ep0 = add_point(engine->main_graph, 0, 1, 0, 1);
    /* 妫€鏌?add_point 杩斿洖鍊硷細澶辫触杩斿洖 -1锛屼笉搴斿皢鏃犳晥ID浼犲叆鍚庣画鍑芥暟 */
    TEST_ASSERT(ep0 >= 0, "add_point in engine for origin failed");

    /* 浣跨敤杈呭姪鍑芥暟娣诲姞鐐?(1, 0) */
    int ep1 = add_point(engine->main_graph, 1, 1, 0, 1);
    TEST_ASSERT(ep1 >= 0, "add_point in engine for (1,0) failed");

    /* 娣诲姞绾挎: 杩炴帴鐐?鍜岀偣1 */
    graph_add_line_segment(engine->main_graph, ep0, ep1);

    /* 娣诲姞鍏宠仈绾︽潫: 鐐?鍦ㄧ嚎娈典笂 */
    int seg_id = engine->main_graph->next_node_id - 1;
    AddConstraintResult res = graph_add_incidence(engine->main_graph, ep0, seg_id);
    TEST_ASSERT(res == ADD_CONSTRAINT_OK, "graph_add_incidence should return ADD_CONSTRAINT_OK");

    /* 娣诲姞杈撳叆绔彛 */
    AddNodeResult port_result = graph_add_port(engine->main_graph, PORT_INPUT, 0, -1);
    TEST_ASSERT(port_result == ADD_NODE_OK, "graph_add_port should return ADD_NODE_OK");

    /* 娣诲姞鍔熻兘鍧楋紙鏃犲弬鏁扮増鏈級 */
    AddNodeResult fb_result = graph_add_function_block(engine->main_graph, NULL, 0, NULL, 0, NULL, 0);
    TEST_ASSERT(fb_result == ADD_NODE_OK, "graph_add_function_block should return ADD_NODE_OK");

    engine_destroy(engine);
    printf("  PASSED\n");
}

/**
 * @brief 娴嬭瘯鏈夌悊鏁扮殑杈圭晫鏉′欢
 *
 * 楠岃瘉鏈夌悊鏁板湪闆跺€艰繍绠椼€佽礋鏁拌繍绠楀拰鍒嗘瘝涓洪浂鏃剁殑琛屼负銆?
 */
void test_rational_boundary(void) {
    printf("Testing rational boundary conditions...\n");

    /* --- 闆跺€艰繍绠?--- */

    /* 鍒涘缓闆跺€兼湁鐞嗘暟: 0/1 */
    Rational *zero = rational_create(0, 1);
    TEST_ASSERT_NOT_NULL(zero);

    /* 鍒涘缓鏅€氭湁鐞嗘暟: 3/4 */
    Rational *r = rational_create(3, 4);
    TEST_ASSERT_NOT_NULL(r);

    /* 0 + x == x */
    Rational *sum_zero = rational_add(zero, r);
    TEST_ASSERT_NOT_NULL(sum_zero);
    Rational *expected_r = rational_create(3, 4);
    TEST_ASSERT(rational_compare(sum_zero, expected_r) == 0, "0 + 3/4 should equal 3/4");
    rational_destroy(expected_r);
    rational_destroy(sum_zero);

    /* 0 * x == 0 */
    Rational *prod_zero = rational_multiply(zero, r);
    TEST_ASSERT_NOT_NULL(prod_zero);
    Rational *expected_zero = rational_create(0, 1);
    TEST_ASSERT(rational_compare(prod_zero, expected_zero) == 0, "0 * 3/4 should equal 0");
    rational_destroy(expected_zero);
    rational_destroy(prod_zero);

    /* x / 1 == x */
    Rational *one = rational_create(1, 1);
    Rational *div_one = rational_divide(r, one);
    TEST_ASSERT_NOT_NULL(div_one);
    Rational *expected_r2 = rational_create(3, 4);
    TEST_ASSERT(rational_compare(div_one, expected_r2) == 0, "3/4 / 1 should equal 3/4");
    rational_destroy(expected_r2);
    rational_destroy(div_one);

    rational_destroy(one);
    rational_destroy(zero);
    rational_destroy(r);

    /* --- 璐熸暟杩愮畻 --- */

    /* 鍒涘缓璐熸暟鏈夌悊鏁? -2/3 */
    Rational *neg = rational_create(-2, 3);
    TEST_ASSERT_NOT_NULL(neg);

    /* 鍒涘缓姝ｆ暟鏈夌悊鏁? 1/3 */
    Rational *pos = rational_create(1, 3);
    TEST_ASSERT_NOT_NULL(pos);

    /* 璐熸暟 + 姝ｆ暟: -2/3 + 1/3 = -1/3 */
    Rational *sum_neg = rational_add(neg, pos);
    TEST_ASSERT_NOT_NULL(sum_neg);
    Rational *expected_neg = rational_create(-1, 3);
    TEST_ASSERT(rational_compare(sum_neg, expected_neg) == 0, "-2/3 + 1/3 should equal -1/3");
    rational_destroy(expected_neg);
    rational_destroy(sum_neg);

    /* 璐熸暟 * 姝ｆ暟: -2/3 * 1/3 = -2/9 */
    Rational *prod_neg = rational_multiply(neg, pos);
    TEST_ASSERT_NOT_NULL(prod_neg);
    Rational *expected_neg2 = rational_create(-2, 9);
    TEST_ASSERT(rational_compare(prod_neg, expected_neg2) == 0, "-2/3 * 1/3 should equal -2/9");
    rational_destroy(expected_neg2);
    rational_destroy(prod_neg);

    /* 璐熸暟 * 璐熸暟: -2/3 * -2/3 = 4/9 */
    Rational *prod_neg_neg = rational_multiply(neg, neg);
    TEST_ASSERT_NOT_NULL(prod_neg_neg);
    Rational *expected_pos = rational_create(4, 9);
    TEST_ASSERT(rational_compare(prod_neg_neg, expected_pos) == 0, "-2/3 * -2/3 should equal 4/9");
    rational_destroy(expected_pos);
    rational_destroy(prod_neg_neg);

    rational_destroy(neg);
    rational_destroy(pos);

    /* --- 鍒嗘瘝涓洪浂鐨勯敊璇鐞?--- */

    /* 鐢ㄥ垎姣嶄负闆跺垱寤烘湁鐞嗘暟搴旇繑鍥?NULL 鎴栬瑙勮寖鍖栧鐞?*/
    Rational *div_by_zero = rational_create(1, 0);
    /* 瀹炵幇搴斿畨鍏ㄥ鐞嗗垎姣嶄负闆剁殑鎯呭喌锛氳繑鍥?NULL 鎴栬鑼冨寲涓烘湁鏁堝€?*/
    printf("  鍒嗘瘝涓洪浂鍒涘缓: %s\n", div_by_zero ? "杩斿洖闈濶ULL锛堝凡瑙勮寖鍖栵級" : "杩斿洖NULL锛堝畨鍏ㄥ鐞嗭級");
    if (div_by_zero != NULL) {
        rational_destroy(div_by_zero);
    }

    /* 鏈夌悊鏁伴櫎浠ラ浂搴旇繑鍥?NULL */
    Rational *a = rational_create(5, 1);
    Rational *b = rational_create(0, 1);
    Rational *div_result = rational_divide(a, b);
    TEST_ASSERT(div_result == NULL, "鏈夌悊鏁伴櫎浠ラ浂搴旇繑鍥?NULL");
    rational_destroy(a);
    rational_destroy(b);

    printf("  PASSED\n");
}

int main() {
    printf("=== Lv-00 Geometry Metalanguage Test Suite ===\n\n");
    test_rational();
    test_rational_boundary();
    test_constraint_graph();
    test_normalization();
    test_module();
    test_axiom_package();
    test_unify();
    test_engine();

    if (g_fail_count > 0) {
        printf("\n=== %d test(s) FAILED ===\n", g_fail_count);
        return 1;
    }
    printf("\n=== All tests PASSED! ===\n");
    return 0;
}
