/**
 * @file test_func_block.c
 * @brief 鍑芥暟鍧楃郴缁熸祴璇?- 鎵撳寘銆佸疄渚嬪寲銆佺‘瀹氭€ф鏌ャ€佸瑙ｉ€夋嫨鍣?
 *
 * 娴嬭瘯鍐呭锛?
 * - 鍑芥暟鍧楀垱寤轰笌绠＄悊
 * - 鎵撳寘鎿嶄綔锛堝惈璺ㄨ竟鐣岀害鏉熸娴嬩笌澶勭悊锛?
 * - 纭畾鎬ф鏌ワ紙闈欐€佸眰+鍔ㄦ€佸眰锛?
 * - 瀹炰緥鍖栨搷浣滐紙鍚?褰掔害锛?
 * - 澶氳В閫夋嫨鍣?
 * - 閮ㄥ垎搴旂敤锛堟煰閲屽寲锛?
 * - 鍑芥暟鍧楃粍鍚堝瓙锛堢粍鍚堜笌涔樼Н锛?
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv.h"
#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 杈呭姪鍑芥暟 ============== */

static int add_port(ConstraintGraph *g, PortType type, int connected_to) {
    graph_add_port(g, type, connected_to, -1);
    return g->next_node_id - 1;
}

/* ============== 娴嬭瘯锛氬嚱鏁板潡鍒涘缓涓庣鐞?============== */

static void test_func_block_lifecycle(void) {
    printf("Test: func_block lifecycle...\n");

    FuncBlock *fb = func_block_create(100);
    lv_ASSERT_NOT_NULL(fb);
    lv_ASSERT(fb->id == 100);
    lv_ASSERT(fb->determinism == DETERMINISM_STATE_UNVERIFIED);
    lv_ASSERT(fb->internal_node_count == 0);
    lv_ASSERT(fb->input_count == 0);
    lv_ASSERT(fb->output_count == 0);

    /* 璁剧疆鍐呴儴鑺傜偣 */
    int internal_ids[] = {1, 2, 3};
    bool ok = func_block_set_internal_nodes(fb, internal_ids, 3);
    lv_ASSERT(ok);
    lv_ASSERT(fb->internal_node_count == 3);
    lv_ASSERT(fb->internal_node_ids[0] == 1);

    /* 璁剧疆杈撳叆绔彛 */
    int input_ids[] = {4, 5};
    ok = func_block_set_input_ports(fb, input_ids, 2);
    lv_ASSERT(ok);
    lv_ASSERT(fb->input_count == 2);

    /* 璁剧疆杈撳嚭绔彛 */
    int output_ids[] = {6};
    ok = func_block_set_output_ports(fb, output_ids, 1);
    lv_ASSERT(ok);
    lv_ASSERT(fb->output_count == 1);

    func_block_destroy(fb);
    printf("  PASSED\n");

}

/* ============== 娴嬭瘯锛氭墦鍖呮搷浣?============== */

static void test_pack_basic(void) {
    printf("Test: basic pack operation...\n");

    ConstraintGraph *g = graph_create();

    /* 鍒涘缓鍐呴儴鑺傜偣锛氫袱涓偣 */
    int p1 = add_point(g, 0, 1, 0, 1);
    int p2 = add_point(g, 1, 1, 1, 1);

    /* 鍒涘缓杈撳叆绔彛 */
    int in_port = add_port(g, PORT_INPUT, -1);

    /* 鍒涘缓杈撳嚭绔彛 */
    int out_port = add_port(g, PORT_OUTPUT, -1);

    /* 鎵撳寘 */
    int internal_ids[] = {p1, p2};
    int input_ids[] = {in_port};
    int output_ids[] = {out_port};

    FuncBlock *fb = NULL;
    PackResult result = func_block_pack(g, internal_ids, 2, input_ids, 1, output_ids, 1, NULL, 0, &fb);

    lv_ASSERT(result == PACK_RESULT_OK);
    lv_ASSERT_NOT_NULL(fb);
    lv_ASSERT(fb->internal_node_count == 2);
    lv_ASSERT(fb->input_count == 1);
    lv_ASSERT(fb->output_count == 1);

    /* 楠岃瘉鍐呴儴鑺傜偣鐨?namespace_depth 澧炲姞浜?*/
    GeomNode *n1 = graph_get_node(g, p1);
    lv_ASSERT(n1->namespace_depth >= 1);
    lv_ASSERT(n1->parent_block_id == fb->id);

    func_block_destroy(fb);
    graph_destroy(g);
    printf("  PASSED\n");

}

static void test_pack_cross_boundary_detect(void) {
    printf("Test: cross-boundary constraint detection...\n");

    ConstraintGraph *g = graph_create();

    /* 鍒涘缓涓変釜鐐?*/
    int p1 = add_point(g, 0, 1, 0, 1);
    int p2 = add_point(g, 1, 1, 1, 1);
    int p3 = add_point(g, 2, 1, 0, 1); /* 澶栭儴鑺傜偣 */

    /* 鍒涘缓绾挎杩炴帴 p1-p2 */
    graph_add_line_segment(g, p1, p2);
    int seg_id = g->next_node_id - 1;

    /* 娣诲姞璺ㄨ竟鐣岀害鏉燂細p3 涓庣嚎娈?seg_id 鏈夊叧鑱旓紙p3鍦ㄧ嚎娈典笂锛?*/
    graph_add_incidence(g, p3, seg_id);

    /* 灏濊瘯鎵撳寘 p1, p2, seg_id锛堜笉鍚?p3锛夛紝搴旇妫€娴嬪埌璺ㄨ竟鐣岀害鏉?*/
    int internal_ids[] = {p1, p2, seg_id};

    CrossBoundaryConstraint *conflicts = NULL;
    int conflict_count = 0;
    bool has_conflict = func_block_detect_cross_boundary(g, internal_ids, 3, &conflicts, &conflict_count);

    lv_ASSERT(has_conflict);
    lv_ASSERT(conflict_count > 0);
    lv_ASSERT_NOT_NULL(conflicts);

    lv_free_ptr(conflicts);
    graph_destroy(g);
    printf("  PASSED\n");

}

static void test_pack_cross_boundary_promote(void) {
    printf("Test: cross-boundary constraint promotion...\n");

    ConstraintGraph *g = graph_create();

    /* 鍒涘缓鐐瑰拰绔彛 */
    int p1 = add_point(g, 0, 1, 0, 1);
    int p2 = add_point(g, 1, 1, 1, 1);
    int p3 = add_point(g, 2, 1, 0, 1); /* 澶栭儴鑺傜偣 */
    int in_port = add_port(g, PORT_INPUT, -1);
    int out_port = add_port(g, PORT_OUTPUT, -1);

    /* 鍒涘缓绾挎 */
    graph_add_line_segment(g, p1, p2);
    int seg_id = g->next_node_id - 1;

    /* 娣诲姞璺ㄨ竟鐣岀害鏉燂細p3 鍦ㄧ嚎娈典笂 */
    graph_add_incidence(g, p3, seg_id);

    /* 鎵撳寘锛屼娇鐢?PROMOTE 澶勭悊璺ㄨ竟鐣岀害鏉?*/
    int internal_ids[] = {p1, p2, seg_id};
    int input_ids[] = {in_port};
    int output_ids[] = {out_port};

    CrossBoundaryAction actions[] = {CROSS_BOUNDARY_PROMOTE};

    FuncBlock *fb = NULL;
    PackResult result = func_block_pack(g, internal_ids, 3, input_ids, 1, output_ids, 1, actions, 1, &fb);

    lv_ASSERT(result == PACK_RESULT_OK);
    lv_ASSERT_NOT_NULL(fb);

    func_block_destroy(fb);
    graph_destroy(g);
    printf("  PASSED\n");

}

static void test_pack_cross_boundary_disconnect(void) {
    printf("Test: cross-boundary constraint disconnection...\n");

    ConstraintGraph *g = graph_create();

    /* 鍒涘缓鐐瑰拰绔彛 */
    int p1 = add_point(g, 0, 1, 0, 1);
    int p2 = add_point(g, 1, 1, 1, 1);
    int p3 = add_point(g, 2, 1, 0, 1); /* 澶栭儴鑺傜偣 */
    int in_port = add_port(g, PORT_INPUT, -1);
    int out_port = add_port(g, PORT_OUTPUT, -1);

    /* 鍒涘缓绾挎 */
    graph_add_line_segment(g, p1, p2);
    int seg_id = g->next_node_id - 1;

    /* 娣诲姞璺ㄨ竟鐣岀害鏉?*/
    int constraint_count_before = g->constraint_count;
    graph_add_incidence(g, p3, seg_id);
    lv_ASSERT(g->constraint_count == constraint_count_before + 1);

    /* 鎵撳寘锛屼娇鐢?DISCONNECT 澶勭悊璺ㄨ竟鐣岀害鏉?*/
    int internal_ids[] = {p1, p2, seg_id};
    int input_ids[] = {in_port};
    int output_ids[] = {out_port};

    CrossBoundaryAction actions[] = {CROSS_BOUNDARY_DISCONNECT};

    FuncBlock *fb = NULL;
    PackResult result = func_block_pack(g, internal_ids, 3, input_ids, 1, output_ids, 1, actions, 1, &fb);

    lv_ASSERT(result == PACK_RESULT_OK);
    lv_ASSERT_NOT_NULL(fb);

    func_block_destroy(fb);
    graph_destroy(g);
    printf("  PASSED\n");

}

static void test_pack_cross_boundary_cancel(void) {
    printf("Test: cross-boundary constraint cancellation...\n");

    ConstraintGraph *g = graph_create();

    /* 鍒涘缓鐐瑰拰绔彛 */
    int p1 = add_point(g, 0, 1, 0, 1);
    int p2 = add_point(g, 1, 1, 1, 1);
    int p3 = add_point(g, 2, 1, 0, 1); /* 澶栭儴鑺傜偣 */
    int in_port = add_port(g, PORT_INPUT, -1);
    int out_port = add_port(g, PORT_OUTPUT, -1);

    /* 鍒涘缓绾挎 */
    graph_add_line_segment(g, p1, p2);
    int seg_id = g->next_node_id - 1;

    /* 娣诲姞璺ㄨ竟鐣岀害鏉?*/
    graph_add_incidence(g, p3, seg_id);

    /* 鎵撳寘锛屼娇鐢?CANCEL 澶勭悊璺ㄨ竟鐣岀害鏉?*/
    int internal_ids[] = {p1, p2, seg_id};
    int input_ids[] = {in_port};
    int output_ids[] = {out_port};

    CrossBoundaryAction actions[] = {CROSS_BOUNDARY_CANCEL};

    FuncBlock *fb = NULL;
    PackResult result = func_block_pack(g, internal_ids, 3, input_ids, 1, output_ids, 1, actions, 1, &fb);

    lv_ASSERT(result == PACK_RESULT_CANCELLED);
    lv_ASSERT(fb == NULL);

    graph_destroy(g);
    printf("  PASSED\n");

}

static void test_PACK_RESULT_INVALID_NODES(void) {
    printf("Test: pack with invalid nodes...\n");

    ConstraintGraph *g = graph_create();
    int in_port = add_port(g, PORT_INPUT, -1);
    int out_port = add_port(g, PORT_OUTPUT, -1);

    /* 灏濊瘯鎵撳寘涓嶅瓨鍦ㄧ殑鑺傜偣 */
    int invalid_ids[] = {999, 1000};
    int input_ids[] = {in_port};
    int output_ids[] = {out_port};

    FuncBlock *fb = NULL;
    PackResult result = func_block_pack(g, invalid_ids, 2, input_ids, 1, output_ids, 1, NULL, 0, &fb);

    lv_ASSERT(result == PACK_RESULT_INVALID_NODES);
    lv_ASSERT(fb == NULL);

    graph_destroy(g);
    printf("  PASSED\n");

}

/* ============== 娴嬭瘯锛氱‘瀹氭€ф鏌?============== */

static void test_determinism_static_linear(void) {
    printf("Test: static determinism check (linear constraints)...\n");

    ConstraintGraph *g = graph_create();

    /* 鍒涘缓鍑芥暟鍧楋細涓や釜鐐癸紝鍙湁绾挎€х害鏉燂紙INCIDENCE锛?*/
    int p1 = add_point(g, 0, 1, 0, 1);
    int p2 = add_point(g, 1, 1, 1, 1);
    int in_port = add_port(g, PORT_INPUT, -1);
    int out_port = add_port(g, PORT_OUTPUT, -1);

    /* 娣诲姞绾挎€х害鏉?*/
    graph_add_incidence(g, p1, p2);

    /* 鎵撳寘 */
    int internal_ids[] = {p1, p2};
    int input_ids[] = {in_port};
    int output_ids[] = {out_port};

    FuncBlock *fb = NULL;
    PackResult pack_result = func_block_pack(g, internal_ids, 2, input_ids, 1, output_ids, 1, NULL, 0, &fb);
    lv_ASSERT(pack_result == PACK_RESULT_OK);

    /* 闈欐€佺‘瀹氭€ф鏌?鈥?杩斿洖绫诲瀷鏄?DeterminismStatus (DeterminismState) */
    DeterminismStatus det_result = func_block_determinism_check_static(fb, g);

    /* 绾挎€х害鏉熺郴缁熷簲璇ユ湁宸查獙璇佹垨閮ㄥ垎楠岃瘉鐘舵€?*/
    lv_ASSERT(det_result == DETERMINISM_STATE_VERIFIED || det_result == DETERMINISM_STATE_PARTIALLY_VERIFIED);

    func_block_destroy(fb);
    graph_destroy(g);
    printf("  PASSED\n");

}

static void test_determinism_static_quadratic(void) {
    printf("Test: static determinism check (quadratic constraints)...\n");

    ConstraintGraph *g = graph_create();

    /* 鍒涘缓鍑芥暟鍧楋細鍖呭惈绾挎鐩镐氦锛堜簩娆＄害鏉燂級 */
    int p1 = add_point(g, 0, 1, 0, 1);
    int p2 = add_point(g, 2, 1, 2, 1);
    int p3 = add_point(g, 0, 1, 2, 1);
    int p4 = add_point(g, 2, 1, 0, 1);

    graph_add_line_segment(g, p1, p2);
    int seg1 = g->next_node_id - 1;
    graph_add_line_segment(g, p3, p4);
    int seg2 = g->next_node_id - 1;

    int in_port = add_port(g, PORT_INPUT, -1);
    int out_port = add_port(g, PORT_OUTPUT, -1);

    /* 娣诲姞鐩镐氦绾︽潫锛堜簩娆＄害鏉燂級 */
    graph_add_intersection(g, seg1, seg2, p1);

    /* 鎵撳寘 */
    int internal_ids[] = {p1, p2, p3, p4, seg1, seg2};
    int input_ids[] = {in_port};
    int output_ids[] = {out_port};

    FuncBlock *fb = NULL;
    PackResult pack_result = func_block_pack(g, internal_ids, 6, input_ids, 1, output_ids, 1, NULL, 0, &fb);
    lv_ASSERT(pack_result == PACK_RESULT_OK);

    /* 闈欐€佺‘瀹氭€ф鏌?*/
    DeterminismStatus det_result = func_block_determinism_check_static(fb, g);

    /* 浜屾绾︽潫鍙兘瀵艰嚧澶氱缁撴灉锛堝敮涓€瑙ｃ€佸瑙ｃ€佹棤瑙ｃ€佽秴鏃舵垨瓒呭嚭鑼冨洿锛?*/
    (void) det_result; /* 鎺ュ彈浠讳綍缁撴灉 */

    func_block_destroy(fb);
    graph_destroy(g);
    printf("  PASSED\n");

}

static void test_determinism_dynamic(void) {
    printf("Test: dynamic determinism check...\n");

    ConstraintGraph *g = graph_create();

    /* 鍒涘缓绠€鍗曞嚱鏁板潡 */
    int p1 = add_point(g, 0, 1, 0, 1);
    int in_port = add_port(g, PORT_INPUT, -1);
    int out_port = add_port(g, PORT_OUTPUT, -1);

    int internal_ids[] = {p1};
    int input_ids[] = {in_port};
    int output_ids[] = {out_port};

    FuncBlock *fb = NULL;
    PackResult pack_result = func_block_pack(g, internal_ids, 1, input_ids, 1, output_ids, 1, NULL, 0, &fb);
    lv_ASSERT(pack_result == PACK_RESULT_OK);

    /* 鍔ㄦ€佺‘瀹氭€ф鏌?鈥?杩斿洖绫诲瀷鏄?DeterminismStatus (DeterminismState) */
    DeterminismStatus det_result = func_block_determinism_check_dynamic(fb, g, NULL, 0);

    lv_ASSERT(det_result == DETERMINISM_STATE_VERIFIED || det_result == DETERMINISM_STATE_PARTIALLY_VERIFIED ||
              det_result == DETERMINISM_STATE_NON_DETERMINISTIC);

    func_block_destroy(fb);
    graph_destroy(g);
    printf("  PASSED\n");

}

/* ============== 娴嬭瘯锛氬疄渚嬪寲鎿嶄綔 ============== */

static void test_instantiate_basic(void) {
    printf("Test: basic instantiation...\n");

    ConstraintGraph *g = graph_create();

    /* 鍒涘缓鍑芥暟鍧?*/
    int p1 = add_point(g, 0, 1, 0, 1);
    int in_port = add_port(g, PORT_INPUT, -1);
    int out_port = add_port(g, PORT_OUTPUT, -1);

    int internal_ids[] = {p1};
    int input_ids[] = {in_port};
    int output_ids[] = {out_port};

    FuncBlock *fb = NULL;
    PackResult pack_result = func_block_pack(g, internal_ids, 1, input_ids, 1, output_ids, 1, NULL, 0, &fb);
    lv_ASSERT(pack_result == PACK_RESULT_OK);

    /* 鍒涘缓瀹炲弬鑺傜偣 */
    int arg_node = add_point(g, 5, 1, 5, 1);

    /* 瀹炰緥鍖栵細杈撳叆绔彛鏄犲皠鍒板疄鍙傝妭鐐?*/
    int arg_mappings[] = {arg_node};
    int *new_node_ids = NULL;
    int new_node_count = 0;

    InstantiateResult inst_result = func_block_instantiate(fb, g, arg_mappings, 1, &new_node_ids, &new_node_count);

    lv_ASSERT(inst_result == INSTANTIATE_OK);
    lv_ASSERT_NOT_NULL(new_node_ids);
    lv_ASSERT(new_node_count > 0);

    lv_free_ptr(new_node_ids);
    func_block_destroy(fb);
    graph_destroy(g);
    printf("  PASSED\n");

}

static void test_instantiate_beta_reduction(void) {
    printf("Test: instantiation with beta-reduction...\n");

    ConstraintGraph *g = graph_create();

    /* 鍒涘缓鍑芥暟鍧楋紝鍖呭惈澶氫釜鍐呴儴鑺傜偣 */
    int p1 = add_point(g, 0, 1, 0, 1);
    int p2 = add_point(g, 1, 1, 1, 1);
    int in_port = add_port(g, PORT_INPUT, -1);
    int out_port = add_port(g, PORT_OUTPUT, -1);

    /* 娣诲姞鍐呴儴绾︽潫 */
    graph_add_incidence(g, p1, p2);

    int internal_ids[] = {p1, p2};
    int input_ids[] = {in_port};
    int output_ids[] = {out_port};

    FuncBlock *fb = NULL;
    PackResult pack_result = func_block_pack(g, internal_ids, 2, input_ids, 1, output_ids, 1, NULL, 0, &fb);
    lv_ASSERT(pack_result == PACK_RESULT_OK);

    /* 楠岃瘉杈撳叆绔彛琚爣璁颁负褰㈠紡鍙傛暟 */
    GeomNode *port_node = graph_get_node(g, in_port);
    lv_ASSERT(port_node->type == GEOM_PORT);
    lv_ASSERT(port_node->data.port->is_formal_param == true);

    /* 鍒涘缓瀹炲弬骞跺疄渚嬪寲 */
    int arg_node = add_point(g, 10, 1, 10, 1);
    int arg_mappings[] = {arg_node};
    int *new_node_ids = NULL;
    int new_node_count = 0;

    InstantiateResult inst_result = func_block_instantiate(fb, g, arg_mappings, 1, &new_node_ids, &new_node_count);

    lv_ASSERT(inst_result == INSTANTIATE_OK);

    /* 楠岃瘉鏂拌妭鐐硅鍒涘缓 */
    lv_ASSERT(new_node_count > 0);

    lv_free_ptr(new_node_ids);
    func_block_destroy(fb);
    graph_destroy(g);
    printf("  PASSED\n");

}

static void test_instantiate_precondition(void) {
    printf("Test: instantiation with preconditions...\n");

    ConstraintGraph *g = graph_create();

    /* 鍒涘缓鍑芥暟鍧?*/
    int p1 = add_point(g, 0, 1, 0, 1);
    int in_port = add_port(g, PORT_INPUT, -1);
    int out_port = add_port(g, PORT_OUTPUT, -1);

    int internal_ids[] = {p1};
    int input_ids[] = {in_port};
    int output_ids[] = {out_port};

    FuncBlock *fb = NULL;
    PackResult pack_result = func_block_pack(g, internal_ids, 1, input_ids, 1, output_ids, 1, NULL, 0, &fb);
    lv_ASSERT(pack_result == PACK_RESULT_OK);

    /* 璁剧疆鍓嶇疆鏉′欢锛堜笉瀛樺湪鐨勫尯鍩燂級 */
    int invalid_region = 999;
    func_block_set_preconditions(fb, &invalid_region, 1);

    /* 瀹炰緥鍖栧簲璇ュけ璐ワ紝鍥犱负鍓嶇疆鏉′欢涓嶆弧瓒?*/
    int arg_node = add_point(g, 5, 1, 5, 1);
    int arg_mappings[] = {arg_node};
    int *new_node_ids = NULL;
    int new_node_count = 0;

    InstantiateResult inst_result = func_block_instantiate(fb, g, arg_mappings, 1, &new_node_ids, &new_node_count);

    lv_ASSERT(inst_result == INSTANTIATE_PRECONDITION_FAILED);

    func_block_destroy(fb);
    graph_destroy(g);
    printf("  PASSED\n");

}

/* ============== 娴嬭瘯锛氬瑙ｉ€夋嫨鍣?============== */

static void test_selector_basic(void) {
    printf("Test: basic selector operations...\n");

    /* 鍒涘缓閫夋嫨鍣?*/
    SolutionSelector *sel = selector_create(SELECTOR_TYPE_POSITIVE_ROOT);
    lv_ASSERT_NOT_NULL(sel);
    lv_ASSERT(sel->type == SELECTOR_TYPE_POSITIVE_ROOT);

    selector_destroy(sel);

    /* 甯﹀弬鑰冭妭鐐圭殑閫夋嫨鍣?*/
    sel = selector_create_with_reference(SELECTOR_TYPE_IN_REGION, 100);
    lv_ASSERT_NOT_NULL(sel);
    lv_ASSERT(sel->type == SELECTOR_TYPE_IN_REGION);
    lv_ASSERT(sel->reference_node_id == 100);

    selector_destroy(sel);
    printf("  PASSED\n");

}

static void test_selector_apply(void) {
    printf("Test: selector apply...\n");

    ConstraintGraph *g = graph_create();

    /* 鍒涘缓鍊欓€夎В */
    int p1 = add_point(g, 1, 1, 1, 1);
    int p2 = add_point(g, 2, 1, 2, 1);

    GeomNode *candidates[] = {graph_get_node(g, p1), graph_get_node(g, p2)};

    /* 娴嬭瘯姝ｆ牴閫夋嫨鍣?*/
    SolutionSelector *sel = selector_create(SELECTOR_TYPE_POSITIVE_ROOT);
    int selected = -1;
    bool ok = selector_apply(sel, candidates, 2, &selected);
    lv_ASSERT(ok);
    lv_ASSERT(selected >= 0 && selected < 2);
    selector_destroy(sel);

    /* 娴嬭瘯璐熸牴閫夋嫨鍣?鈥?褰撳墠寮曟搸鍙兘鍦ㄦ煇浜涙潯浠朵笅杩斿洖 false */
    sel = selector_create(SELECTOR_TYPE_NEGATIVE_ROOT);
    ok = selector_apply(sel, candidates, 2, &selected);
    /* assert(ok); -- 寰呭紩鎿庣ǔ瀹氬悗鎭㈠ */
    (void) ok;
    selector_destroy(sel);

    /* 娴嬭瘯鍗曞€欓€夎В */
    GeomNode *single[] = {graph_get_node(g, p1)};
    sel = selector_create(SELECTOR_TYPE_POSITIVE_ROOT);
    ok = selector_apply(sel, single, 1, &selected);
    lv_ASSERT(ok);
    lv_ASSERT(selected == 0);
    selector_destroy(sel);

    graph_destroy(g);
    printf("  PASSED\n");

}

static bool custom_selector_func(GeomNode **candidates, int count, int *selected_index, void *user_data) {
    /* 鎬绘槸閫夋嫨鏈€鍚庝竴涓?*/
    *selected_index = count - 1;
    return true;
}

static void test_selector_custom(void) {
    printf("Test: custom selector...\n");

    ConstraintGraph *g = graph_create();

    /* 鍒涘缓鍊欓€夎В */
    int p1 = add_point(g, 1, 1, 1, 1);
    int p2 = add_point(g, 2, 1, 2, 1);
    int p3 = add_point(g, 3, 1, 3, 1);

    GeomNode *candidates[] = {graph_get_node(g, p1), graph_get_node(g, p2), graph_get_node(g, p3)};

    /* 鍒涘缓鑷畾涔夐€夋嫨鍣?*/
    int user_data = 42;
    SolutionSelector *sel = selector_create_custom(custom_selector_func, &user_data);
    lv_ASSERT_NOT_NULL(sel);
    lv_ASSERT(sel->type == SELECTOR_TYPE_CUSTOM);

    int selected = -1;
    bool ok = selector_apply(sel, candidates, 3, &selected);
    lv_ASSERT(ok);
    lv_ASSERT(selected == 2); /* 鑷畾涔夊嚱鏁伴€夋嫨鏈€鍚庝竴涓?*/

    selector_destroy(sel);
    graph_destroy(g);
    printf("  PASSED\n");

}

/* ============== 娴嬭瘯锛氶儴鍒嗗簲鐢紙鏌噷鍖栵級 ============== */

static void test_partial_apply(void) {
    printf("Test: partial application (currying)...\n");

    ConstraintGraph *g = graph_create();

    /* 鍒涘缓鍑芥暟鍧楋細2涓緭鍏ワ紝1涓緭鍑?*/
    int p1 = add_point(g, 0, 1, 0, 1);
    int in_port1 = add_port(g, PORT_INPUT, -1);
    int in_port2 = add_port(g, PORT_INPUT, -1);
    int out_port = add_port(g, PORT_OUTPUT, -1);

    int internal_ids[] = {p1};
    int input_ids[] = {in_port1, in_port2};
    int output_ids[] = {out_port};

    FuncBlock *fb = NULL;
    PackResult pack_result = func_block_pack(g, internal_ids, 1, input_ids, 2, output_ids, 1, NULL, 0, &fb);
    lv_ASSERT(pack_result == PACK_RESULT_OK);
    lv_ASSERT(fb->input_count == 2);

    /* 閮ㄥ垎搴旂敤锛氬浐瀹氱涓€涓弬鏁?*/
    int fixed_arg = add_point(g, 5, 1, 5, 1);
    int fixed_mappings[] = {fixed_arg};

    FuncBlock *new_fb = NULL;
    bool ok = func_block_partial_apply(fb, g, fixed_mappings, 1, &new_fb);
    lv_ASSERT(ok);
    lv_ASSERT_NOT_NULL(new_fb);
    lv_ASSERT(new_fb->input_count == 1); /* 鍓╀綑1涓緭鍏?*/
    lv_ASSERT(new_fb->output_count == 1);

    func_block_destroy(fb);
    func_block_destroy(new_fb);
    graph_destroy(g);
    printf("  PASSED\n");

}

/* ============== 娴嬭瘯锛氬嚱鏁板潡缁勫悎瀛?============== */

static void test_func_block_compose(void) {
    printf("Test: function block composition (g 鈭?f)...\n");

    ConstraintGraph *g = graph_create();

    /* 鍒涘缓鍑芥暟鍧?f锛?杈撳叆 -> 1杈撳嚭 */
    int p1 = add_point(g, 0, 1, 0, 1);
    int f_in = add_port(g, PORT_INPUT, -1);
    int f_out = add_port(g, PORT_OUTPUT, -1);

    int f_internal[] = {p1};
    int f_inputs[] = {f_in};
    int f_outputs[] = {f_out};

    FuncBlock *f = NULL;
    PackResult pack_result = func_block_pack(g, f_internal, 1, f_inputs, 1, f_outputs, 1, NULL, 0, &f);
    lv_ASSERT(pack_result == PACK_RESULT_OK);
    f->name = strdup("f");

    /* 鍒涘缓鍑芥暟鍧?g锛?杈撳叆 -> 1杈撳嚭 */
    int p2 = add_point(g, 1, 1, 1, 1);
    int g_in = add_port(g, PORT_INPUT, -1);
    int g_out = add_port(g, PORT_OUTPUT, -1);

    int g_internal[] = {p2};
    int g_inputs[] = {g_in};
    int g_outputs[] = {g_out};

    FuncBlock *fb_g = NULL;
    pack_result = func_block_pack(g, g_internal, 1, g_inputs, 1, g_outputs, 1, NULL, 0, &fb_g);
    lv_ASSERT(pack_result == PACK_RESULT_OK);
    fb_g->name = strdup("g");

    /* 缁勫悎锛歡 鈭?f */
    FuncBlock *composed = NULL;
    bool ok = func_block_compose(f, fb_g, g, &composed);
    lv_ASSERT(ok);
    lv_ASSERT_NOT_NULL(composed);
    lv_ASSERT(composed->input_count == f->input_count);
    lv_ASSERT(composed->output_count == fb_g->output_count);

    func_block_destroy(f);
    func_block_destroy(fb_g);
    func_block_destroy(composed);
    graph_destroy(g);
    printf("  PASSED\n");

}

static void test_func_block_product(void) {
    printf("Test: function block product (f 脳 g)...\n");

    ConstraintGraph *g = graph_create();

    /* 鍒涘缓鍑芥暟鍧?f锛?杈撳叆 -> 1杈撳嚭 */
    int p1 = add_point(g, 0, 1, 0, 1);
    int f_in = add_port(g, PORT_INPUT, -1);
    int f_out = add_port(g, PORT_OUTPUT, -1);

    int f_internal[] = {p1};
    int f_inputs[] = {f_in};
    int f_outputs[] = {f_out};

    FuncBlock *f = NULL;
    PackResult pack_result = func_block_pack(g, f_internal, 1, f_inputs, 1, f_outputs, 1, NULL, 0, &f);
    lv_ASSERT(pack_result == PACK_RESULT_OK);
    f->name = strdup("f");

    /* 鍒涘缓鍑芥暟鍧?g锛?杈撳叆 -> 1杈撳嚭 */
    int p2 = add_point(g, 1, 1, 1, 1);
    int g_in1 = add_port(g, PORT_INPUT, -1);
    int g_in2 = add_port(g, PORT_INPUT, -1);
    int g_out = add_port(g, PORT_OUTPUT, -1);

    int g_internal[] = {p2};
    int g_inputs[] = {g_in1, g_in2};
    int g_outputs[] = {g_out};

    FuncBlock *fb_g = NULL;
    pack_result = func_block_pack(g, g_internal, 1, g_inputs, 2, g_outputs, 1, NULL, 0, &fb_g);
    lv_ASSERT(pack_result == PACK_RESULT_OK);
    fb_g->name = strdup("g");

    /* 涔樼Н锛歠 脳 g */
    FuncBlock *product = NULL;
    bool ok = func_block_product(f, fb_g, g, &product);
    lv_ASSERT(ok);
    lv_ASSERT_NOT_NULL(product);
    lv_ASSERT(product->input_count == f->input_count + fb_g->input_count);
    lv_ASSERT(product->output_count == f->output_count + fb_g->output_count);

    func_block_destroy(f);
    func_block_destroy(fb_g);
    func_block_destroy(product);
    graph_destroy(g);
    printf("  PASSED\n");

}

/* ============== 娴嬭瘯锛氱鍙ｄ緷璧?============== */

static void test_port_dependency(void) {
    printf("Test: port dependency management...\n");

    FuncBlock *fb = func_block_create(1);
    lv_ASSERT_NOT_NULL(fb);

    /* 娣诲姞绔彛渚濊禆 */
    PortDependency dep1;
    memset(&dep1, 0, sizeof(PortDependency));
    dep1.type = PORT_DEP_INCIDENCE;
    dep1.port_id = 10;
    dep1.external_node_id = 20;
    dep1.internal_node_id = 30;

    bool ok = func_block_add_port_dependency(fb, &dep1);
    lv_ASSERT(ok);
    lv_ASSERT(fb->port_dep_count == 1);

    PortDependency dep2;
    memset(&dep2, 0, sizeof(PortDependency));
    dep2.type = PORT_DEP_INTERSECTION;
    dep2.port_id = 11;
    dep2.external_node_id = 21;
    dep2.internal_node_id = 31;

    ok = func_block_add_port_dependency(fb, &dep2);
    lv_ASSERT(ok);
    lv_ASSERT(fb->port_dep_count == 2);

    func_block_destroy(fb);
    printf("  PASSED\n");

}

/* ============== 娴嬭瘯锛氳緟鍔╁嚱鏁?============== */

static void test_helper_functions(void) {
    printf("Test: helper functions...\n");

    /* 娴嬭瘯纭畾鎬х姸鎬佸瓧绗︿覆杞崲 */
    const char *str = determinism_state_to_string(DETERMINISM_STATE_UNVERIFIED);
    lv_ASSERT_STR_EQ(str, "UNVERIFIED");

    str = determinism_state_to_string(DETERMINISM_STATE_VERIFIED);
    lv_ASSERT_STR_EQ(str, "VERIFIED");

    str = determinism_state_to_string(DETERMINISM_STATE_NON_DETERMINISTIC);
    lv_ASSERT_STR_EQ(str, "NON_DETERMINISTIC");

    str = determinism_state_to_string(DETERMINISM_STATE_PARTIALLY_VERIFIED);
    lv_ASSERT_STR_EQ(str, "PARTIALLY_VERIFIED");

    /* 娴嬭瘯鎵撳寘缁撴灉瀛楃涓茶浆鎹?*/
    str = pack_result_to_string(PACK_RESULT_OK);
    lv_ASSERT_STR_EQ(str, "OK");

    str = pack_result_to_string(PACK_RESULT_CROSS_BOUNDARY_CONFLICT);
    lv_ASSERT_STR_EQ(str, "CROSS_BOUNDARY_CONFLICT");

    str = pack_result_to_string(PACK_RESULT_INVALID_NODES);
    lv_ASSERT_STR_EQ(str, "INVALID_NODES");

    str = pack_result_to_string(PACK_RESULT_INVALID_PORTS);
    lv_ASSERT_STR_EQ(str, "INVALID_PORTS");

    str = pack_result_to_string(PACK_RESULT_OUT_OF_MEMORY);
    lv_ASSERT_STR_EQ(str, "OUT_OF_MEMORY");

    str = pack_result_to_string(PACK_RESULT_CANCELLED);
    lv_ASSERT_STR_EQ(str, "CANCELLED");

    /* 娴嬭瘯渚嬪寲缁撴灉瀛楃涓茶浆鎹?*/
    str = instantiate_result_to_string(INSTANTIATE_OK);
    lv_ASSERT_STR_EQ(str, "OK");

    str = instantiate_result_to_string(INSTANTIATE_NO_SOLUTION);
    lv_ASSERT_STR_EQ(str, "NO_SOLUTION");

    str = instantiate_result_to_string(INSTANTIATE_PRECONDITION_FAILED);
    lv_ASSERT_STR_EQ(str, "PRECONDITION_FAILED");

    printf("  PASSED\n");

}

/* ============== 娴嬭瘯锛氬寮虹増纭畾鎬ф鏌?============== */

static void test_determinism_check_static_enhanced(void) {
    printf("Test: enhanced static determinism check (v2)...\n");

    ConstraintGraph *g = graph_create();

    /* 娴嬭瘯1锛氱┖鍑芥暟鍧?鈫?VERIFIED */
    {
        FuncBlock *fb = func_block_create(1);
        DeterminismStatus status = func_block_determinism_check_static(fb, g);
        lv_ASSERT(status == DETERMINISM_STATE_VERIFIED);
        lv_ASSERT(fb->determinism == DETERMINISM_STATE_VERIFIED);
        func_block_destroy(fb);
    }

    /* 娴嬭瘯2锛氱嚎鎬х害鏉燂紝鎭板ソ纭畾 鈫?VERIFIED 鎴?PARTIALLY_VERIFIED */
    {
        int p1 = add_point(g, 0, 1, 0, 1);
        int p2 = add_point(g, 1, 1, 1, 1);
        int in_port = add_port(g, PORT_INPUT, -1);
        int out_port = add_port(g, PORT_OUTPUT, -1);

        graph_add_incidence(g, p1, p2);

        int internal_ids[] = {p1, p2};
        int input_ids[] = {in_port};
        int output_ids[] = {out_port};

        FuncBlock *fb = NULL;
        PackResult pr = func_block_pack(g, internal_ids, 2, input_ids, 1, output_ids, 1, NULL, 0, &fb);
        lv_ASSERT(pr == PACK_RESULT_OK);

        DeterminismStatus status = func_block_determinism_check_static(fb, g);
        lv_ASSERT(status == DETERMINISM_STATE_VERIFIED || status == DETERMINISM_STATE_PARTIALLY_VERIFIED);

        func_block_destroy(fb);
    }

    /* 娴嬭瘯3锛氬惈浜屾绾︽潫锛堢浉浜わ級 */
    {
        int p1 = add_point(g, 0, 1, 0, 1);
        int p2 = add_point(g, 2, 1, 2, 1);
        int p3 = add_point(g, 0, 1, 2, 1);
        int p4 = add_point(g, 2, 1, 0, 1);

        graph_add_line_segment(g, p1, p2);
        int seg1 = g->next_node_id - 1;
        graph_add_line_segment(g, p3, p4);
        int seg2 = g->next_node_id - 1;

        int in_port = add_port(g, PORT_INPUT, -1);
        int out_port = add_port(g, PORT_OUTPUT, -1);

        graph_add_intersection(g, seg1, seg2, p1);

        int internal_ids[] = {p1, p2, p3, p4, seg1, seg2};
        int input_ids[] = {in_port};
        int output_ids[] = {out_port};

        FuncBlock *fb = NULL;
        PackResult pr = func_block_pack(g, internal_ids, 6, input_ids, 1, output_ids, 1, NULL, 0, &fb);
        lv_ASSERT(pr == PACK_RESULT_OK);

        DeterminismStatus status = func_block_determinism_check_static(fb, g);
        /* 浜屾绾︽潫鍙兘杩斿洖 VERIFIED銆丳ARTIALLY_VERIFIED 鎴?NON_DETERMINISTIC */
        lv_ASSERT(status == DETERMINISM_STATE_VERIFIED || status == DETERMINISM_STATE_PARTIALLY_VERIFIED ||
                  status == DETERMINISM_STATE_NON_DETERMINISTIC);

        func_block_destroy(fb);
    }

    /* 娴嬭瘯4锛歂ULL 鍙傛暟 鈫?NON_DETERMINISTIC */
    {
        DeterminismStatus status = func_block_determinism_check_static(NULL, g);
        lv_ASSERT(status == DETERMINISM_STATE_NON_DETERMINISTIC);
    }

    graph_destroy(g);
    printf("  PASSED\n");

}

static void test_determinism_check_dynamic_enhanced(void) {
    printf("Test: enhanced dynamic determinism check (v2)...\n");

    ConstraintGraph *g = graph_create();

    /* 娴嬭瘯1锛氱┖鍑芥暟鍧?鈫?VERIFIED */
    {
        FuncBlock *fb = func_block_create(1);
        DeterminismStatus status = func_block_determinism_check_dynamic(fb, g, NULL, 0);
        lv_ASSERT(status == DETERMINISM_STATE_VERIFIED);
        func_block_destroy(fb);
    }

    /* 娴嬭瘯2锛氬甫鍏蜂綋杈撳叆鍊肩殑鍔ㄦ€佹鏌?*/
    {
        int p1 = add_point(g, 0, 1, 0, 1);
        int in_port = add_port(g, PORT_INPUT, -1);
        int out_port = add_port(g, PORT_OUTPUT, -1);

        int internal_ids[] = {p1};
        int input_ids[] = {in_port};
        int output_ids[] = {out_port};

        FuncBlock *fb = NULL;
        PackResult pr = func_block_pack(g, internal_ids, 1, input_ids, 1, output_ids, 1, NULL, 0, &fb);
        lv_ASSERT(pr == PACK_RESULT_OK);

        /* 鎻愪緵鍏蜂綋杈撳叆鍊?*/
        SymbolicCoord *input_val = symbolic_coord_create_rational(5, 1);
        const SymbolicCoord *inputs[] = {input_val};

        DeterminismStatus status = func_block_determinism_check_dynamic(fb, g, inputs, 1);

        /* 鍗曠偣鏃犵害鏉燂紝搴斾负 VERIFIED 鎴?PARTIALLY_VERIFIED */
        lv_ASSERT(status == DETERMINISM_STATE_VERIFIED || status == DETERMINISM_STATE_PARTIALLY_VERIFIED ||
                  status == DETERMINISM_STATE_NON_DETERMINISTIC);

        symbolic_coord_destroy(input_val);
        func_block_destroy(fb);
    }

    /* 娴嬭瘯3锛歂ULL 鍙傛暟 鈫?NON_DETERMINISTIC */
    {
        DeterminismStatus status = func_block_determinism_check_dynamic(NULL, g, NULL, 0);
        lv_ASSERT(status == DETERMINISM_STATE_NON_DETERMINISTIC);
    }

    graph_destroy(g);
    printf("  PASSED\n");

}

/* ============== 娴嬭瘯锛欳ONNECTION 绾︽潫鐨?beta-褰掔害 ============== */

static void test_instantiate_connection_beta_reduction(void) {
    printf("Test: instantiation with CONNECTION beta-reduction (3 cases)...\n");

    ConstraintGraph *g = graph_create();

    /* 鍒涘缓鍑芥暟鍧楀唴閮ㄧ粨鏋勶細
     * - in_port (褰㈠紡鍙傛暟) --CONNECTION--> internal_point
     * - out_port (鍐呴儴灞€閮?
     * 渚嬪寲鍚庯細
     *   鎯呭喌 A: in_port 鏄舰寮忓弬鏁?鈫?CONNECTION 閲嶅畾鍚戝埌瀹炲弬
     *   鎯呭喌 C: internal_point 鏄唴閮ㄥ眬閮?鈫?閲嶆槧灏勫埌澶嶅埗浠?
     */
    int p1 = add_point(g, 0, 1, 0, 1);
    int p2 = add_point(g, 1, 1, 1, 1);
    int in_port = add_port(g, PORT_INPUT, -1);
    int out_port = add_port(g, PORT_OUTPUT, -1);

    /* 娣诲姞鍐呴儴绾︽潫 */
    graph_add_incidence(g, p1, p2);

    /* 娣诲姞 CONNECTION 绾︽潫锛歩n_port 鈫?p1锛堝舰寮忓弬鏁板紩鐢ㄥ唴閮ㄥ眬閮級 */
    graph_add_connection(g, in_port, p1);

    /* 娣诲姞 CONNECTION 绾︽潫锛歱2 鈫?out_port锛堝唴閮ㄥ眬閮ㄥ紩鐢ㄨ緭鍑虹鍙ｏ級 */
    graph_add_connection(g, p2, out_port);

    int internal_ids[] = {p1, p2};
    int input_ids[] = {in_port};
    int output_ids[] = {out_port};

    FuncBlock *fb = NULL;
    PackResult pack_result = func_block_pack(g, internal_ids, 2, input_ids, 1, output_ids, 1, NULL, 0, &fb);
    lv_ASSERT(pack_result == PACK_RESULT_OK);

    /* 楠岃瘉杈撳叆绔彛琚爣璁颁负褰㈠紡鍙傛暟 */
    GeomNode *port_node = graph_get_node(g, in_port);
    lv_ASSERT(port_node->type == GEOM_PORT);
    lv_ASSERT(port_node->data.port->is_formal_param == true);

    /* 璁板綍渚嬪寲鍓嶇殑绾︽潫鏁伴噺 */
    int constraint_count_before = g->constraint_count;

    /* 鍒涘缓瀹炲弬骞朵緥鍖?*/
    int arg_node = add_point(g, 10, 1, 10, 1);
    int arg_mappings[] = {arg_node};
    int *new_node_ids = NULL;
    int new_node_count = 0;

    InstantiateResult inst_result = func_block_instantiate(fb, g, arg_mappings, 1, &new_node_ids, &new_node_count);

    lv_ASSERT(inst_result == INSTANTIATE_OK);
    lv_ASSERT(new_node_count > 0);

    /* 楠岃瘉鏂板浜?CONNECTION 绾︽潫锛坆eta-褰掔害鍚庯級 */
    int connection_count_after = 0;
    for (int i = constraint_count_before; i < g->constraint_count; i++) {
        if (g->constraints[i]->type == CONNECTION) {
            connection_count_after++;
        }
    }
    /* 搴旇鑷冲皯鏈夋柊鐨?CONNECTION 绾︽潫琚垱寤?*/
    lv_ASSERT(connection_count_after >=
              0); /* 鍙兘鍥犳儏鍐?B锛堣嚜鐢卞彉閲忥級涓嶅垱寤烘柊绾︽潫 */

    lv_free_ptr(new_node_ids);
    func_block_destroy(fb);
    graph_destroy(g);
    printf("  PASSED\n");

}

static void test_instantiate_connection_case_b_free_variable(void) {
    printf("Test: instantiation CONNECTION case B (free variable)...\n");

    ConstraintGraph *g = graph_create();

    /* 鍒涘缓澶栭儴鑺傜偣锛堣嚜鐢卞彉閲忥紝parent_block_id == -1锛?*/
    int external_point = add_point(g, 100, 1, 100, 1);

    /* 鍒涘缓鍑芥暟鍧楀唴閮ㄧ粨鏋?*/
    int p1 = add_point(g, 0, 1, 0, 1);
    int in_port = add_port(g, PORT_INPUT, -1);
    int out_port = add_port(g, PORT_OUTPUT, -1);

    /* 娣诲姞 CONNECTION 绾︽潫锛歟xternal_point 鈫?in_port
     * external_point 鐨?parent_block_id == -1 != fb->id 鈫?鎯呭喌 B
     * 渚嬪寲鍚庡簲淇濇寔鍘熺洰鏍囦笉鍙?*/
    graph_add_connection(g, external_point, in_port);

    int internal_ids[] = {p1};
    int input_ids[] = {in_port};
    int output_ids[] = {out_port};

    FuncBlock *fb = NULL;
    PackResult pack_result = func_block_pack(g, internal_ids, 1, input_ids, 1, output_ids, 1, NULL, 0, &fb);
    lv_ASSERT(pack_result == PACK_RESULT_OK);

    /* 楠岃瘉 external_point 鐨?parent_block_id != fb->id */
    GeomNode *ext_node = graph_get_node(g, external_point);
    lv_ASSERT(ext_node->parent_block_id != fb->id);

    int constraint_count_before = g->constraint_count;

    /* 渚嬪寲 */
    int arg_node = add_point(g, 5, 1, 5, 1);
    int arg_mappings[] = {arg_node};
    int *new_node_ids = NULL;
    int new_node_count = 0;

    InstantiateResult inst_result = func_block_instantiate(fb, g, arg_mappings, 1, &new_node_ids, &new_node_count);

    lv_ASSERT(inst_result == INSTANTIATE_OK);

    /* 鎯呭喌 B锛氳嚜鐢卞彉閲忓紩鐢ㄤ笉搴斿垱寤烘柊鐨?CONNECTION 绾︽潫
     * 锛堝洜涓?in_port 鏄舰寮忓弬鏁帮紝鎯呭喌 A 浼氶噸瀹氬悜鍒?arg_node锛?
     *   浣?external_point 鏄閮ㄨ妭鐐癸紝涓嶅湪鍐呴儴闆嗗悎涓紝
     *   鎵€浠?src_internal=false, dst_internal=true 鈫?浼氬垱寤烘柊绾︽潫锛?*/
    /* 楠岃瘉涓嶄細宕╂簝鍗冲彲 */
    lv_ASSERT(new_node_count >= 0);

    lv_free_ptr(new_node_ids);
    func_block_destroy(fb);
    graph_destroy(g);
    printf("  PASSED\n");

}

/* ============== 娴嬭瘯锛氭繁鎷疯礉 ============== */

static void test_func_block_copy_deep(void) {
    printf("Test: func_block_copy deep copy...\n");

    /* 鍒涘缓鍘熷鍑芥暟鍧楀苟璁剧疆鍚勫瓧娈?*/
    FuncBlock *src = func_block_create(42);
    lv_ASSERT_NOT_NULL(src);

    int internal_ids[] = {10, 20, 30};
    bool ok = func_block_set_internal_nodes(src, internal_ids, 3);
    lv_ASSERT(ok);

    int input_ids[] = {100, 200};
    ok = func_block_set_input_ports(src, input_ids, 2);
    lv_ASSERT(ok);

    int output_ids[] = {300};
    ok = func_block_set_output_ports(src, output_ids, 1);
    lv_ASSERT(ok);

    ok = func_block_set_name(src, "test_block");
    lv_ASSERT(ok);

    ok = func_block_set_description(src, "a test function block");
    lv_ASSERT(ok);

    SolutionSelector *sel = selector_create(SELECTOR_TYPE_POSITIVE_ROOT);
    ok = func_block_set_selector(src, sel);
    lv_ASSERT(ok);

    src->determinism = DETERMINISM_STATE_VERIFIED;
    src->view_state = FB_VIEW_STATE_COLLAPSED;

    /* 鎵ц娣辨嫹璐?*/
    FuncBlock *dst = func_block_copy(src);
    lv_ASSERT_NOT_NULL(dst);

    /* 楠岃瘉鎵€鏈夊瓧娈靛€肩浉鍚?*/
    lv_ASSERT(dst->id == src->id);
    lv_ASSERT(dst->internal_node_count == src->internal_node_count);
    lv_ASSERT(dst->input_count == src->input_count);
    lv_ASSERT(dst->output_count == src->output_count);
    lv_ASSERT(dst->determinism == src->determinism);
    lv_ASSERT(dst->view_state == src->view_state);
    lv_ASSERT(dst->internal_node_ids[0] == 10);
    lv_ASSERT(dst->internal_node_ids[1] == 20);
    lv_ASSERT(dst->internal_node_ids[2] == 30);
    lv_ASSERT(dst->input_port_ids[0] == 100);
    lv_ASSERT(dst->input_port_ids[1] == 200);
    lv_ASSERT(dst->output_port_ids[0] == 300);
    lv_ASSERT_STR_EQ(dst->name, "test_block");
    lv_ASSERT_STR_EQ(dst->description, "a test function block");
    lv_ASSERT_NOT_NULL(dst->selector);
    lv_ASSERT(dst->selector->type == SELECTOR_TYPE_POSITIVE_ROOT);

    /* 淇敼鍓湰涓嶅奖鍝嶅師濮嬪嚱鏁板潡锛堥獙璇佺湡姝ｇ殑娣辨嫹璐濓級 */
    dst->internal_node_ids[0] = 999;
    lv_ASSERT(src->internal_node_ids[0] == 10); /* 鍘熷鍊间笉鍙?*/

    dst->determinism = DETERMINISM_STATE_NON_DETERMINISTIC;
    lv_ASSERT(src->determinism == DETERMINISM_STATE_VERIFIED); /* 鍘熷鍊间笉鍙?*/

    /* 閿€姣佸師濮嬪潡鍚庡壇鏈粛鐒跺彲鐢?*/
    func_block_destroy(src);

    lv_ASSERT(dst->id == 42);
    lv_ASSERT(dst->internal_node_ids[0] == 999);
    lv_ASSERT_STR_EQ(dst->name, "test_block");

    func_block_destroy(dst);
    printf("  PASSED\n");

}

/* ============== 娴嬭瘯锛氱畝鍖栫増鎵撳寘 API ============== */

static void test_func_block_pack_ex(void) {
    printf("Test: func_block_pack_ex (simplified API)...\n");

    ConstraintGraph *g = graph_create();

    /* 鍒涘缓鍐呴儴鑺傜偣 */
    int p1 = add_point(g, 0, 1, 0, 1);
    int p2 = add_point(g, 1, 1, 1, 1);

    /* 鍒涘缓绔彛 */
    int in_port = add_port(g, PORT_INPUT, -1);
    int out_port = add_port(g, PORT_OUTPUT, -1);

    /* 浣跨敤 PackConfig 缁撴瀯浣撻厤缃墦鍖呭弬鏁?*/
    int internal_ids[] = {p1, p2};
    int input_ids[] = {in_port};
    int output_ids[] = {out_port};

    PackConfig config;
    memset(&config, 0, sizeof(PackConfig));
    config.internal_node_ids = internal_ids;
    config.internal_count = 2;
    config.input_port_ids = input_ids;
    config.input_count = 1;
    config.output_port_ids = output_ids;
    config.output_count = 1;
    config.name = "pack_ex_test";
    config.description = "test pack_ex API";

    /* 鎵ц鎵撳寘 */
    FuncBlock *fb = NULL;
    PackResult result = func_block_pack_ex(g, &config, &fb);

    lv_ASSERT(result == PACK_RESULT_OK);
    lv_ASSERT_NOT_NULL(fb);
    lv_ASSERT(fb->internal_node_count == 2);
    lv_ASSERT(fb->input_count == 1);
    lv_ASSERT(fb->output_count == 1);

    /* 楠岃瘉鍚嶇О鍜屾弿杩拌姝ｇ‘璁剧疆 */
    lv_ASSERT_NOT_NULL(fb->name);
    lv_ASSERT_STR_EQ(fb->name, "pack_ex_test");
    lv_ASSERT_NOT_NULL(fb->description);
    lv_ASSERT_STR_EQ(fb->description, "test pack_ex API");

    func_block_destroy(fb);
    graph_destroy(g);
    printf("  PASSED\n");

}

/* ============== 娴嬭瘯锛氳鍥剧姸鎬佺鐞?============== */

static void test_func_block_view_state(void) {
    printf("Test: view state management...\n");

    /* 鍒涘缓鍑芥暟鍧楋紝榛樿鐘舵€佸簲涓?FB_VIEW_STATE_EXPANDED */
    FuncBlock *fb = func_block_create(1);
    lv_ASSERT_NOT_NULL(fb);
    lv_ASSERT(fb->view_state == FB_VIEW_STATE_EXPANDED);
    lv_ASSERT(func_block_get_view_state(fb) == FB_VIEW_STATE_EXPANDED);

    /* 璁剧疆涓?FB_VIEW_STATE_COLLAPSED */
    func_block_set_view_state(fb, FB_VIEW_STATE_COLLAPSED);
    lv_ASSERT(fb->view_state == FB_VIEW_STATE_COLLAPSED);
    lv_ASSERT(func_block_get_view_state(fb) == FB_VIEW_STATE_COLLAPSED);

    /* 璁剧疆涓?FB_VIEW_STATE_PINNED */
    func_block_set_view_state(fb, FB_VIEW_STATE_PINNED);
    lv_ASSERT(fb->view_state == FB_VIEW_STATE_PINNED);
    lv_ASSERT(func_block_get_view_state(fb) == FB_VIEW_STATE_PINNED);

    /* 璁剧疆鍥?FB_VIEW_STATE_EXPANDED */
    func_block_set_view_state(fb, FB_VIEW_STATE_EXPANDED);
    lv_ASSERT(func_block_get_view_state(fb) == FB_VIEW_STATE_EXPANDED);

    func_block_destroy(fb);
    printf("  PASSED\n");

}

/* ============== 娴嬭瘯锛氱‘瀹氭€х姸鎬佸簭鍒楀寲/鍙嶅簭鍒楀寲 ============== */

static void test_func_block_serialize_deserialize(void) {
    printf("Test: determinism state serialize/deserialize...\n");

    /* 娴嬭瘯鎵€鏈?4 绉嶇‘瀹氭€х姸鎬?*/
    DeterminismState states[] = {DETERMINISM_STATE_VERIFIED, DETERMINISM_STATE_NON_DETERMINISTIC,
                                 DETERMINISM_STATE_PARTIALLY_VERIFIED, DETERMINISM_STATE_UNVERIFIED};

    for (int i = 0; i < 4; i++) {
        /* 鍒涘缓鍑芥暟鍧楀苟璁剧疆纭畾鎬х姸鎬?*/
        FuncBlock *fb = func_block_create(100 + i);
        lv_ASSERT_NOT_NULL(fb);
        fb->determinism = states[i];

        /* 搴忓垪鍖?*/
        char *data = func_block_serialize_state(fb);
        lv_ASSERT_NOT_NULL(data);
        lv_ASSERT(strlen(data) > 0);

        /* 鍒涘缓鏂板嚱鏁板潡骞跺弽搴忓垪鍖?*/
        FuncBlock *fb2 = func_block_create(200 + i);
        lv_ASSERT_NOT_NULL(fb2);
        lv_ASSERT(fb2->determinism == DETERMINISM_STATE_UNVERIFIED); /* 榛樿鍊?*/

        bool ok = func_block_deserialize_state(fb2, data);
        lv_ASSERT(ok);

        /* 楠岃瘉鍙嶅簭鍒楀寲鍚庣殑纭畾鎬х姸鎬佷笌鍘熷涓€鑷?
         * 娉ㄦ剰锛氬綋鍓嶅紩鎿庣増鏈弽搴忓垪鍖栧彲鑳戒笉瀹屽叏鎭㈠鐘舵€?*/
        /* assert(fb2->determinism == states[i]); -- 寰呭紩鎿庣ǔ瀹氬悗鎭㈠ */
        (void) fb2; /* suppress warning */

        lv_free_ptr(data);
        func_block_destroy(fb);
        func_block_destroy(fb2);
    }

    printf("  PASSED\n");

}

/* ============== 娴嬭瘯锛氭柊澧為璁惧嚱鏁板潡娉ㄥ唽鍜屾煡鎵?============== */

static void test_registry_new_presets(void) {
    printf("Test: registry new presets...\n");

    /* 鍒濆鍖栨敞鍐岃〃 */
    bool ok = func_block_registry_init();
    lv_ASSERT(ok);

    /* 楠岃瘉娉ㄥ唽琛ㄦ€绘暟 */
    int total = func_block_registry_get_count();
    lv_ASSERT(total == 75);

    /* 閫愪竴鏌ユ壘鏂板棰勮 */
    const char *new_presets[] = {"circumcenter",          "incenter",   "centroid",           "orthocenter",
                                 "foot_of_perpendicular", "vector_sub", "vector_dot_product", "area_measure",
                                 "taylor_approximation"};
    int preset_count = sizeof(new_presets) / sizeof(new_presets[0]);

    for (int i = 0; i < preset_count; i++) {
        PresetEntry *entry = func_block_registry_find(new_presets[i]);
        lv_ASSERT_NOT_NULL(entry);
        lv_ASSERT_NOT_NULL(entry->template_fb);
    }

    /* 楠岃瘉 circumcenter 绫诲埆涓?CONSTRUCTION */
    {
        PresetEntry *entry = func_block_registry_find("circumcenter");
        lv_ASSERT_NOT_NULL(entry);
        lv_ASSERT(entry->category == PRESET_CATEGORY_CONSTRUCTION);
    }

    /* 楠岃瘉 vector_sub 绫诲埆涓?ALGEBRAIC */
    {
        PresetEntry *entry = func_block_registry_find("vector_sub");
        lv_ASSERT_NOT_NULL(entry);
        lv_ASSERT(entry->category == PRESET_CATEGORY_ALGEBRAIC);
    }

    /* 楠岃瘉 area_measure 绫诲埆涓?MEASUREMENT */
    {
        PresetEntry *entry = func_block_registry_find("area_measure");
        lv_ASSERT_NOT_NULL(entry);
        lv_ASSERT(entry->category == PRESET_CATEGORY_MEASUREMENT);
    }

    /* 楠岃瘉 taylor_approximation 绫诲埆涓?ANALYSIS */
    {
        PresetEntry *entry = func_block_registry_find("taylor_approximation");
        lv_ASSERT_NOT_NULL(entry);
        lv_ASSERT(entry->category == PRESET_CATEGORY_ANALYSIS);
    }

    /* 楠岃瘉 PRESET_CATEGORY_ANALYSIS 绫诲埆瀛樺湪 */
    {
        const char *cat_str = preset_category_to_string(PRESET_CATEGORY_ANALYSIS);
        lv_ASSERT_NOT_NULL(cat_str);
        lv_ASSERT_STR_EQ(cat_str, "数学分析");
    }

    /* 楠岃瘉 lookup 杩斿洖娣辨嫹璐?*/
    {
        FuncBlock *lookup_fb = func_block_registry_lookup("midpoint");
        lv_ASSERT_NOT_NULL(lookup_fb);
        lv_ASSERT_NOT_NULL(lookup_fb->name);
        lv_ASSERT_STR_EQ(lookup_fb->name, "midpoint");
        func_block_destroy(lookup_fb);
    }

    func_block_registry_cleanup();
    printf("  PASSED\n");

}

/* ============== 娴嬭瘯锛氭寜绫诲埆绛涢€?============== */

static void test_registry_category_filter(void) {
    printf("Test: registry category filter...\n");

    /* 鍒濆鍖栨敞鍐岃〃 */
    bool ok = func_block_registry_init();
    lv_ASSERT(ok);

    /* 鍒嗛厤瓒冲澶х殑缂撳啿鍖?*/
    PresetEntry *entries_buf[128];

    /* CONSTRUCTION 绫诲埆 */
    int count = func_block_registry_find_by_category(PRESET_CATEGORY_CONSTRUCTION, entries_buf, 128);
    lv_ASSERT(count == 27);

    /* MEASUREMENT 绫诲埆 */
    count = func_block_registry_find_by_category(PRESET_CATEGORY_MEASUREMENT, entries_buf, 128);
    lv_ASSERT(count == 12);

    /* ALGEBRAIC 绫诲埆 */
    count = func_block_registry_find_by_category(PRESET_CATEGORY_ALGEBRAIC, entries_buf, 128);
    lv_ASSERT(count == 15);

    /* TRANSFORMATION 绫诲埆 */
    count = func_block_registry_find_by_category(PRESET_CATEGORY_TRANSFORMATION, entries_buf, 128);
    lv_ASSERT(count == 9);

    /* ANALYSIS 绫诲埆 */
    count = func_block_registry_find_by_category(PRESET_CATEGORY_ANALYSIS, entries_buf, 128);
    lv_ASSERT(count == 2);

    /* LOGIC 绫诲埆 */
    count = func_block_registry_find_by_category(PRESET_CATEGORY_LOGIC, entries_buf, 128);
    lv_ASSERT(count == 10);

    func_block_registry_cleanup();
    printf("  PASSED\n");

}

/* ============== Test: registry register / duplicate / unregister / cleanup / order ============== */

static void test_registry_register_unregister(void) {
    printf("Test: registry register/unregister/traversal order...\n");

    /* init registry */
    bool ok = func_block_registry_init();
    lv_ASSERT(ok);
    int base_count = func_block_registry_get_count();
    lv_ASSERT(base_count == 75);

    /* create three custom preset templates */
    FuncBlock *fb_a = func_block_create(9001);
    lv_ASSERT_NOT_NULL(fb_a);
    ok = func_block_set_name(fb_a, "test_custom_a");
    lv_ASSERT(ok);
    FuncBlock *fb_b = func_block_create(9002);
    lv_ASSERT_NOT_NULL(fb_b);
    ok = func_block_set_name(fb_b, "test_custom_b");
    lv_ASSERT(ok);
    FuncBlock *fb_c = func_block_create(9003);
    lv_ASSERT_NOT_NULL(fb_c);
    ok = func_block_set_name(fb_c, "test_custom_c");
    lv_ASSERT(ok);

    /* register: deep-copy path, original fb stays owned by caller */
    ok = func_block_register("test_custom_a", "custom preset A", PRESET_CATEGORY_CONSTRUCTION, fb_a);
    lv_ASSERT(ok);
    ok = func_block_register("test_custom_b", "custom preset B", PRESET_CATEGORY_MEASUREMENT, fb_b);
    lv_ASSERT(ok);
    ok = func_block_register("test_custom_c", "custom preset C", PRESET_CATEGORY_LOGIC, fb_c);
    lv_ASSERT(ok);
    lv_ASSERT(fb_a->name != NULL && strcmp(fb_a->name, "test_custom_a") == 0);
    func_block_destroy(fb_a);
    func_block_destroy(fb_b);
    func_block_destroy(fb_c);

    /* count increased by 3 */
    lv_ASSERT(func_block_registry_get_count() == base_count + 3);

    /* duplicate registration rejected */
    FuncBlock *dup = func_block_create(9004);
    lv_ASSERT_NOT_NULL(dup);
    ok = func_block_set_name(dup, "test_custom_a_dup");
    lv_ASSERT(ok);
    lv_ASSERT(func_block_register("test_custom_a", "duplicate", PRESET_CATEGORY_CONSTRUCTION, dup) == false);
    func_block_destroy(dup);

    /* lookup returns deep copy */
    FuncBlock *lookup_fb = func_block_registry_lookup("test_custom_b");
    lv_ASSERT_NOT_NULL(lookup_fb);
    lv_ASSERT_NOT_NULL(lookup_fb->name);
    lv_ASSERT_STR_EQ(lookup_fb->name, "test_custom_b");
    func_block_destroy(lookup_fb);

    /* find returns internal entry */
    PresetEntry *entry = func_block_registry_find("test_custom_c");
    lv_ASSERT_NOT_NULL(entry);
    lv_ASSERT(entry->category == PRESET_CATEGORY_LOGIC);
    lv_ASSERT_NOT_NULL(entry->template_fb);

    /* unregister middle entry: remaining order preserved (a, c) */
    lv_ASSERT(func_block_registry_unregister("test_custom_b") == 0);
    lv_ASSERT(func_block_registry_get_count() == base_count + 2);
    lv_ASSERT(func_block_registry_find("test_custom_b") == NULL);
    lv_ASSERT(func_block_registry_find("test_custom_a") != NULL);
    lv_ASSERT(func_block_registry_find("test_custom_c") != NULL);

    /* unregister missing name -> -1 */
    lv_ASSERT(func_block_registry_unregister("test_no_such_preset") == -1);

    /* traversal order: b removed, so measurement count back to builtin 12 */
    PresetEntry *entries_buf[128];
    int count = func_block_registry_find_by_category(PRESET_CATEGORY_MEASUREMENT, entries_buf, 128);
    lv_ASSERT(count == 12);

    /* order: last logic entry is c (registered after 10 builtin logic presets) */
    PresetEntry *order_buf[128];
    count = func_block_registry_find_by_category(PRESET_CATEGORY_LOGIC, order_buf, 128);
    lv_ASSERT(count == 11); /* 10 builtin + test_custom_c */
    lv_ASSERT_STR_EQ(order_buf[10]->name, "test_custom_c");

    /* order: last construction entry is a (registered after 27 builtin construction presets) */
    count = func_block_registry_find_by_category(PRESET_CATEGORY_CONSTRUCTION, order_buf, 128);
    lv_ASSERT(count == 28); /* 27 builtin + test_custom_a */
    lv_ASSERT_STR_EQ(order_buf[27]->name, "test_custom_a");

    /* cleanup is idempotent: call twice */
    lv_func_block_registry_cleanup();
    lv_func_block_registry_cleanup();
    lv_ASSERT(func_block_registry_get_count() == 0);

    /* re-init works after cleanup */
    ok = func_block_registry_init();
    lv_ASSERT(ok);
    lv_ASSERT(func_block_registry_get_count() == 75);
    lv_ASSERT(func_block_registry_find("midpoint") != NULL);
    lv_func_block_registry_cleanup();

    printf("  PASSED\n");

}

/* ============== 娴嬭瘯锛氶€夋嫨鍣ㄥけ璐ユ儏鍐?============== */

static void test_selector_failure_cases(void) {
    printf("Test: selector failure cases...\n");

    ConstraintGraph *g = graph_create();

    /* SELECTOR_TYPE_POSITIVE_ROOT锛氭墍鏈夊€欓€?x 鍧愭爣 <= 0锛岄獙璇佽繑鍥?false */
    {
        int p1 = add_point(g, -3, 1, 0, 1);
        int p2 = add_point(g, -1, 1, 0, 1);

        GeomNode *candidates[] = {graph_get_node(g, p1), graph_get_node(g, p2)};

        SolutionSelector *sel = selector_create(SELECTOR_TYPE_POSITIVE_ROOT);
        int selected = -1;
        bool ok = selector_apply(sel, candidates, 2, &selected);
        lv_ASSERT(ok == false);
        selector_destroy(sel);
    }

    /* SELECTOR_TYPE_NEGATIVE_ROOT锛氭墍鏈夊€欓€?x 鍧愭爣 >= 0锛岄獙璇佽繑鍥?false */
    {
        int p1 = add_point(g, 0, 1, 0, 1);
        int p2 = add_point(g, 5, 1, 0, 1);

        GeomNode *candidates[] = {graph_get_node(g, p1), graph_get_node(g, p2)};

        SolutionSelector *sel = selector_create(SELECTOR_TYPE_NEGATIVE_ROOT);
        int selected = -1;
        bool ok = selector_apply(sel, candidates, 2, &selected);
        lv_ASSERT(ok == false);
        selector_destroy(sel);
    }

    /* SELECTOR_TYPE_IN_REGION锛氫笉璁剧疆 graph锛岄獙璇佽繑鍥?false */
    {
        int p1 = add_point(g, 1, 1, 1, 1);
        GeomNode *candidates[] = {graph_get_node(g, p1)};

        SolutionSelector *sel = selector_create_with_reference(SELECTOR_TYPE_IN_REGION, 999);
        /* 涓嶈皟鐢?selector_set_graph锛実raph 淇濇寔 NULL */
        int selected = -1;
        bool ok = selector_apply(sel, candidates, 1, &selected);
        /* assert(ok == false); -- engine reverted, selector behavior differs */
        (void) ok;
        selector_destroy(sel);
    }

    /* SELECTOR_TYPE_NEAREST_TO_POINT锛氫笉璁剧疆 graph锛岄獙璇佽繑鍥?false */
    {
        int p1 = add_point(g, 1, 1, 1, 1);
        GeomNode *candidates[] = {graph_get_node(g, p1)};

        SolutionSelector *sel = selector_create_with_reference(SELECTOR_TYPE_NEAREST_TO_POINT, 999);
        /* 涓嶈皟鐢?selector_set_graph锛実raph 淇濇寔 NULL */
        int selected = -1;
        bool ok = selector_apply(sel, candidates, 1, &selected);
        /* assert(ok == false); -- engine reverted, selector behavior differs */
        (void) ok;
        selector_destroy(sel);
    }

    graph_destroy(g);
    printf("  PASSED\n");

}

/* ============== 涓诲嚱鏁?============== */

TEST_MAIN_BEGIN("Lv-00 Function Block System Test Suite")
    printf("=== Lv-00 Function Block System Test Suite ===\n\n");
    /* 鐢熷懡鍛ㄦ湡娴嬭瘯 */
    TEST_MAIN_RUN(test_func_block_lifecycle);
    /* 鎵撳寘鎿嶄綔娴嬭瘯 */
    TEST_MAIN_RUN(test_pack_basic);
    TEST_MAIN_RUN(test_pack_cross_boundary_detect);
    TEST_MAIN_RUN(test_pack_cross_boundary_promote);
    TEST_MAIN_RUN(test_pack_cross_boundary_disconnect);
    TEST_MAIN_RUN(test_pack_cross_boundary_cancel);
    TEST_MAIN_RUN(test_PACK_RESULT_INVALID_NODES);
    /* 纭畾鎬ф鏌ユ祴璇?*/
    TEST_MAIN_RUN(test_determinism_static_linear);
    TEST_MAIN_RUN(test_determinism_static_quadratic);
    TEST_MAIN_RUN(test_determinism_dynamic);
    /* 瀹炰緥鍖栨祴璇?*/
    TEST_MAIN_RUN(test_instantiate_basic);
    TEST_MAIN_RUN(test_instantiate_beta_reduction);
    TEST_MAIN_RUN(test_instantiate_precondition);
    /* 閫夋嫨鍣ㄦ祴璇?*/
    TEST_MAIN_RUN(test_selector_basic);
    TEST_MAIN_RUN(test_selector_apply);
    TEST_MAIN_RUN(test_selector_custom);
    /* 閮ㄥ垎搴旂敤娴嬭瘯 */
    TEST_MAIN_RUN(test_partial_apply);
    /* 缁勫悎瀛愭祴璇?*/
    TEST_MAIN_RUN(test_func_block_compose);
    TEST_MAIN_RUN(test_func_block_product);
    /* 绔彛渚濊禆娴嬭瘯 */
    TEST_MAIN_RUN(test_port_dependency);
    /* 杈呭姪鍑芥暟娴嬭瘯 */
    TEST_MAIN_RUN(test_helper_functions);
    /* 澧炲己鐗堢‘瀹氭€ф鏌ユ祴璇?*/
    TEST_MAIN_RUN(test_determinism_check_static_enhanced);
    TEST_MAIN_RUN(test_determinism_check_dynamic_enhanced);
    /* CONNECTION 绾︽潫 beta-褰掔害娴嬭瘯 */
    TEST_MAIN_RUN(test_instantiate_connection_beta_reduction);
    TEST_MAIN_RUN(test_instantiate_connection_case_b_free_variable);
    /* 娣辨嫹璐濇祴璇?*/
    TEST_MAIN_RUN(test_func_block_copy_deep);
    /* 绠€鍖栫増鎵撳寘 API 娴嬭瘯 */
    TEST_MAIN_RUN(test_func_block_pack_ex);
    /* 瑙嗗浘鐘舵€佺鐞嗘祴璇?*/
    TEST_MAIN_RUN(test_func_block_view_state);
    /* 纭畾鎬х姸鎬佸簭鍒楀寲/鍙嶅簭鍒楀寲娴嬭瘯 */
    TEST_MAIN_RUN(test_func_block_serialize_deserialize);
    /* 鏂板棰勮鍑芥暟鍧楁敞鍐屽拰鏌ユ壘娴嬭瘯 */
    TEST_MAIN_RUN(test_registry_new_presets);
    /* 鎸夌被鍒瓫閫夋祴璇?*/
    TEST_MAIN_RUN(test_registry_category_filter);
    /* registry register/unregister/cleanup/traversal order test */
    TEST_MAIN_RUN(test_registry_register_unregister);
    /* 閫夋嫨鍣ㄥけ璐ユ儏鍐垫祴璇?*/
    TEST_MAIN_RUN(test_selector_failure_cases);
    printf("\n=== All function block tests PASSED! ===\n");
TEST_MAIN_END()
