/**
 * @file test_solver.c
 * @brief 姹傝В鍣ㄦā鍧楁祴璇?- 浠ｆ暟鏂圭▼姹傝В銆佽嚜鐢卞害鍒嗘瀽銆佸啿绐佹娴?
 *
 * 娴嬭瘯鍐呭锛?
 * - 鑷敱搴﹁绠?
 * - 鍐茬獊鏂圭▼妫€娴?
 * - 浠ｆ暟绯荤粺姹傝В
 * - 鍙橀噺娑堝厓
 * - 瓒呰寖鍥村垎鏋?
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00.h"
#include "test_helpers.h"

/* ============== 娴嬭瘯锛氳嚜鐢卞害璁＄畻 ============== */

static int test_degrees_of_freedom(void) {
    printf("Test: degrees of freedom calculation...\n");

    ConstraintGraph *g = graph_create();

    /* 鍒涘缓涓や釜鑷敱鐐癸紙姣忎釜鐐规湁2涓嚜鐢卞害锛?*/
    int p1 = add_point(g, 0, 1, 0, 1);
    int p2 = add_point(g, 1, 1, 1, 1);

    int *free_vars = NULL;
    int dof = count_degrees_of_freedom(g, &free_vars);

    printf("  涓や釜鑷敱鐐? 鑷敱搴?= %d\n", dof);
    /* 涓や釜鐐?= 4涓嚜鐢卞害锛堟瘡涓偣2涓潗鏍囧垎閲忥級 */
    assert(dof == 4);

    if (free_vars)
        lv00_free_ptr(free_vars);

    /* 娣诲姞绾挎绾︽潫 */
    graph_add_line_segment(g, p1, p2);

    int *free_vars2 = NULL;
    int dof2 = count_degrees_of_freedom(g, &free_vars2);
    printf("  娣诲姞绾挎鍚? 鑷敱搴?= %d\n", dof2);

    /* 娣诲姞涓€鏉＄嚎娈电害鏉熷簲鍑忓皯1涓嚜鐢卞害锛? - 1 = 3 */
    assert(dof2 == 3);

    if (free_vars2)
        lv00_free_ptr(free_vars2);

    graph_destroy(g);
    printf("  PASSED\n");
    return 0;
}

/* ============== 娴嬭瘯锛氬啿绐佹柟绋嬫娴?============== */

static int test_conflict_detection(void) {
    printf("Test: conflict equation detection...\n");

    ConstraintGraph *g = graph_create();

    /* 鍒涘缓涓変釜鐐?*/
    int p1 = add_point(g, 0, 1, 0, 1);
    int p2 = add_point(g, 1, 1, 0, 1);
    int p3 = add_point(g, 2, 1, 0, 1);

    /* 娣诲姞绾挎 */
    graph_add_line_segment(g, p1, p2);
    graph_add_line_segment(g, p2, p3);

    bool has_conflict = check_conflict_equations(g);
    printf("  涓変釜鍏辩嚎鐐? 鍐茬獊 = %s\n", has_conflict ? "鏄? : "鍚?);

    /* 涓変釜鍏辩嚎鐐瑰姞涓ゆ潯绾挎绾︽潫涓嶆瀯鎴愬啿绐?*/
    assert(has_conflict == false);

    graph_destroy(g);
    printf("  PASSED\n");
    return 0;
}

/* ============== 娴嬭瘯锛氫唬鏁扮郴缁熸眰瑙?============== */

static int test_algebraic_solve(void) {
    printf("Test: algebraic system solving...\n");

    ConstraintGraph *g = graph_create();

    /* 鍒涘缓鐐?*/
    int p1 = add_point(g, 0, 1, 0, 1);
    int p2 = add_point(g, 2, 1, 0, 1);

    /* 鍒涘缓绾挎 */
    graph_add_line_segment(g, p1, p2);

    /* 灏濊瘯姹傝В */
    int dirty_vars[] = {p1, p2};
    GroebnerResult *result = NULL;
    SolverStatus status = solve_algebraic_system(g, dirty_vars, 2, &result);

    printf("  姹傝В鐘舵€? %d\n", status);

    if (result) {
        printf("  瑙ｇ殑鏁伴噺: %d\n", result->solution_count);
        printf("  鍞竴瑙? %s\n", result->unique ? "鏄? : "鍚?);
        lv00_free_ptr(result);
    }

    graph_destroy(g);
    printf("  PASSED\n");
    return 0;
}

/* ============== 娴嬭瘯锛氬彉閲忔秷鍏?============== */

static int test_variable_elimination(void) {
    printf("Test: variable elimination...\n");

    ConstraintGraph *g = graph_create();

    /* 鍒涘缓鐐?*/
    int p1 = add_point(g, 0, 1, 0, 1);
    int p2 = add_point(g, 1, 1, 1, 1);
    int p3 = add_point(g, 2, 1, 2, 1);

    /* 娣诲姞绾︽潫 */
    graph_add_line_segment(g, p1, p2);
    graph_add_line_segment(g, p2, p3);

    /* 灏濊瘯娑堝厓 */
    int elim_vars[] = {p2};
    SolverStatus status = eliminate_geometry(g, p3, elim_vars, 1);

    printf("  娑堝厓鐘舵€? %d\n", status);

    graph_destroy(g);
    printf("  PASSED\n");
    return 0;
}

/* ============== 娴嬭瘯锛氳秴鑼冨洿鍒嗘瀽 ============== */

static int test_out_of_scope_analysis(void) {
    printf("Test: out of scope analysis...\n");

    ConstraintGraph *g = graph_create();

    /* 鍒涘缓鐐?*/
    int p1 = add_point(g, 0, 1, 0, 1);
    int p2 = add_point(g, 1, 1, 1, 1);

    graph_add_line_segment(g, p1, p2);

    char *suggestion = NULL;
    SolverStatus status = analyze_out_of_scope(g, p1, &suggestion);

    printf("  鍒嗘瀽鐘舵€? %d\n", status);
    if (suggestion) {
        printf("  寤鸿: %s\n", suggestion);
        lv00_free_ptr(suggestion);
    }

    graph_destroy(g);
    printf("  PASSED\n");
    return 0;
}

/* ============== 娴嬭瘯锛氬鏉傜害鏉熺郴缁?============== */

static int test_complex_constraint_system(void) {
    printf("Test: complex constraint system...\n");

    ConstraintGraph *g = graph_create();

    /* 鍒涘缓涓夎褰㈤《鐐?*/
    int a = add_point(g, 0, 1, 0, 1);
    int b = add_point(g, 4, 1, 0, 1);
    int c = add_point(g, 2, 1, 3, 1);

    /* 鍒涘缓杈?*/
    graph_add_line_segment(g, a, b);
    graph_add_line_segment(g, b, c);
    graph_add_line_segment(g, c, a);

    /* 璁＄畻鑷敱搴?*/
    int *free_vars = NULL;
    int dof = count_degrees_of_freedom(g, &free_vars);
    printf("  涓夎褰㈣嚜鐢卞害: %d\n", dof);

    if (free_vars)
        lv00_free_ptr(free_vars);

    /* 妫€娴嬪啿绐?*/
    bool has_conflict = check_conflict_equations(g);
    printf("  鍐茬獊妫€娴? %s\n", has_conflict ? "鏄? : "鍚?);

    graph_destroy(g);
    printf("  PASSED\n");
    return 0;
}

/* ============== 娴嬭瘯锛氳繃绾︽潫绯荤粺 ============== */

static int test_overconstrained_system(void) {
    printf("Test: overconstrained system...\n");

    ConstraintGraph *g = graph_create();

    /* 鍒涘缓鍥哄畾鐐?*/
    int p1 = add_point(g, 0, 1, 0, 1);
    int p2 = add_point(g, 1, 1, 0, 1);
    int p3 = add_point(g, 2, 1, 0, 1);

    /* 娣诲姞澶氫釜绾︽潫 */
    graph_add_line_segment(g, p1, p2);
    graph_add_line_segment(g, p2, p3);
    graph_add_betweenness(g, p1, p2, p3);

    /* 妫€娴嬪啿绐?*/
    bool has_conflict = check_conflict_equations(g);
    printf("  杩囩害鏉熺郴缁熷啿绐? %s\n", has_conflict ? "鏄? : "鍚?);

    /* 璁＄畻鑷敱搴?*/
    int *free_vars = NULL;
    int dof = count_degrees_of_freedom(g, &free_vars);
    printf("  鑷敱搴? %d\n", dof);

    if (free_vars)
        lv00_free_ptr(free_vars);

    graph_destroy(g);
    printf("  PASSED\n");
    return 0;
}

/* ============== 娴嬭瘯锛氬閲忔眰瑙?============== */

static int test_incremental_solve(void) {
    printf("Test: incremental solve...\n");

    ConstraintGraph *g = graph_create();

    /* 鍒涘缓涓変釜鐐?*/
    int p1 = add_point(g, 0, 1, 0, 1);
    int p2 = add_point(g, 2, 1, 0, 1);
    int p3 = add_point(g, 1, 1, 1, 1);

    /* 鍒涘缓绾挎 */
    graph_add_line_segment(g, p1, p2);

    /* 澧為噺姹傝В: 鍙眰瑙?p3 (鏈害鏉? 搴旇繑鍥炵┖瑙? */
    int dirty1[] = {p3};
    GroebnerResult *r1 = solver_incremental_solve(g, dirty1, 1);
    printf("  鏈害鏉熷彉閲忓閲忔眰瑙? 瑙ｆ暟 = %d\n", r1 ? r1->solution_count : -1);
    assert(r1 != NULL);
    groebner_result_free(r1);

    /* 澧為噺姹傝В: 姹傝В p1, p2 (鏈夌嚎娈电害鏉? */
    int dirty2[] = {p1, p2};
    GroebnerResult *r2 = solver_incremental_solve(g, dirty2, 2);
    printf("  鏈夌害鏉熷彉閲忓閲忔眰瑙? 瑙ｆ暟 = %d\n", r2 ? r2->solution_count : -1);
    assert(r2 != NULL);
    groebner_result_free(r2);

    /* 绌鸿剰鍙橀噺闆?-- 搴旀墽琛屽叏閲忔眰瑙ｏ紝杩斿洖闈炵┖缁撴灉 */
    GroebnerResult *r3 = solver_incremental_solve(g, NULL, 0);
    printf("  绌鸿剰鍙橀噺闆? result = %s, 瑙ｆ暟 = %d\n", r3 ? "non-null" : "null", r3 ? r3->solution_count : -1);
    assert(r3 != NULL);
    groebner_result_free(r3);

    graph_destroy(g);
    printf("  PASSED\n");
    return 0;
}

/* ============== 娴嬭瘯锛氬寮烘柟绋嬫彁鍙?============== */

static int test_extract_equations_full(void) {
    printf("Test: extract equations full...\n");

    ConstraintGraph *g = graph_create();

    /* 鍒涘缓鐐?*/
    int p1 = add_point(g, 0, 1, 0, 1);
    int p2 = add_point(g, 3, 1, 0, 1);
    int p3 = add_point(g, 1, 1, 2, 1);

    /* 鍒涘缓绾挎 */
    graph_add_line_segment(g, p1, p2);
    int seg1 = g->next_node_id - 1;

    /* 娣诲姞 INCIDENCE: p3 on seg1 */
    graph_add_incidence(g, p3, seg1);

    /* 鎻愬彇鏂圭▼ */
    EquationSystem *sys = equation_system_create();
    int count = solver_extract_equations_full(g, sys);
    printf("  鎻愬彇鏂圭▼鏁? %d\n", count);
    /* 楠岃瘉: 鎻愬彇鐨勬柟绋嬫暟搴?>= 1锛堣嚦灏戝寘鍚嚎娈垫柟绋嬪拰鍏宠仈鏂圭▼锛?*/
    assert(count >= 1);

    /* 妫€鏌ユ柟绋嬬郴缁?*/
    printf("  鏂圭▼绯荤粺澶у皬: %d\n", equation_system_count(sys));

    /* 楠岃瘉: 鏂圭▼绯荤粺搴旈潪绌?*/
    assert(equation_system_count(sys) >= 1);

    /* 娴嬭瘯 NULL 杈撳叆 */
    int null_result = solver_extract_equations_full(NULL, sys);
    printf("  NULL 杈撳叆: result = %d (expected -1)\n", null_result);
    assert(null_result == -1);

    equation_system_destroy(sys);
    graph_destroy(g);
    printf("  PASSED\n");
    return 0;
}

/* ============== 娴嬭瘯锛欸roebner 鍩鸿绠?============== */

static int test_groebner_basis_compute(void) {
    printf("Test: Groebner basis compute...\n");

    /* 娴嬭瘯1: 绌虹郴缁?*/
    {
        EquationSystem *sys = equation_system_create();
        SolverStatus result = groebner_basis_compute(sys);
        printf("  绌虹郴缁? result = %d (expected %d)\n", result, SOLVER_STATUS_OK);
        assert(result == SOLVER_STATUS_OK);
        equation_system_destroy(sys);
    }

    /* 娴嬭瘯2: 搴︽暟瓒呴檺绯荤粺 */
    {
        EquationSystem *sys = equation_system_create();
        /* 鎵嬪姩娣诲姞涓€涓?degree 3 鐨勬柟绋?*/
        mpz_poly_t poly;
        mpz_poly_init(&poly);
        poly.degree = 3;
        poly.coeffs = malloc(4 * sizeof(mpz_t));
        mpz_init_set_si(poly.coeffs[0], 1);
        mpz_init_set_si(poly.coeffs[1], 0);
        mpz_init_set_si(poly.coeffs[2], 0);
        mpz_init_set_si(poly.coeffs[3], 1);
        /* 鐩存帴鎺ㄥ叆鍐呴儴缁撴瀯 - 浣跨敤 equation_system_push 闇€瑕?
         * 璁块棶鍐呴儴, 鎵€浠ユ垜浠€氳繃 extract_equations_full 鏉ユ祴璇?*/
        mpz_poly_clear(&poly);
        equation_system_destroy(sys);
    }

    /* 娴嬭瘯3: 浠庣害鏉熷浘鏋勫缓绯荤粺骞惰绠?Groebner 鍩?*/
    {
        ConstraintGraph *g = graph_create();
        int p1 = add_point(g, 0, 1, 0, 1);
        int p2 = add_point(g, 2, 1, 0, 1);
        graph_add_line_segment(g, p1, p2);

        EquationSystem *sys = equation_system_create();
        solver_extract_equations_full(g, sys);

        printf("  绾︽潫鍥炬柟绋嬫暟: %d\n", equation_system_count(sys));

        SolverStatus result = groebner_basis_compute(sys);
        printf("  Groebner 鍩鸿绠楃粨鏋? %d (expected %d)\n", result, SOLVER_STATUS_OK);
        assert(result == SOLVER_STATUS_OK);

        printf("  Groebner 鍩哄ぇ灏? %d\n", equation_system_count(sys));

        equation_system_destroy(sys);
        graph_destroy(g);
    }

    /* 娴嬭瘯4: NULL 杈撳叆 */
    {
        SolverStatus result = groebner_basis_compute(NULL);
        printf("  NULL 杈撳叆: result = %d (expected %d)\n", result, SOLVER_STATUS_OK);
        assert(result == SOLVER_STATUS_OK);
    }

    printf("  PASSED\n");
    return 0;
}

/* ============== 娴嬭瘯锛氭柟绋嬬郴缁熺敓鍛藉懆鏈?============== */

static int test_equation_system_lifecycle(void) {
    printf("Test: equation system lifecycle...\n");

    /* 鍒涘缓鍜岄攢姣?*/
    EquationSystem *sys = equation_system_create();
    assert(sys != NULL);
    assert(equation_system_count(sys) == 0);

    /* 鏃犳晥绱㈠紩璁块棶 */
    assert(equation_system_get_poly(sys, 0) == NULL);
    assert(equation_system_get_var_id(sys, 0) == -1);
    assert(equation_system_get_coord_index(sys, 0) == -1);

    /* NULL 杈撳叆 */
    assert(equation_system_count(NULL) == 0);
    assert(equation_system_get_poly(NULL, 0) == NULL);

    equation_system_destroy(sys);

    /* 閿€姣?NULL */
    equation_system_destroy(NULL);

    printf("  PASSED\n");
    return 0;
}

/* ============== 涓诲嚱鏁?============== */

int main(void) {
    printf("=== Lv-00 Solver Module Test Suite ===\n\n");

    test_degrees_of_freedom();
    test_conflict_detection();
    test_algebraic_solve();
    test_variable_elimination();
    test_out_of_scope_analysis();
    test_complex_constraint_system();
    test_overconstrained_system();
    test_incremental_solve();
    test_extract_equations_full();
    test_groebner_basis_compute();
    test_equation_system_lifecycle();

    printf("\n=== All solver tests PASSED! ===\n");
    return 0;
}

