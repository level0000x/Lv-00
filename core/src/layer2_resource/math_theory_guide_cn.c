/**
 * @file math_theory_guide_cn.c
 * @brief 理论数学研究中文指南实现
 *
 * @details 为理论数学研究者提供 Lv-00 系统的中文使用指南。
 *
 * @author Lv-00 Project
 * @version 3.5.0
 */

#include "math_theory_guide_cn.h"

#include <stdio.h>
#include <string.h>

/* ============================================================
 * 几何学研究接口实现
 * ============================================================ */

void guide_euclidean_geometry_cn(char *buf, size_t buf_size) {
    if (!buf || buf_size == 0)
        return;

    snprintf(buf, buf_size,
        "【欧几里得几何研究指南】\n"
        "\n"
        "■ 支持的几何构造：\n"
        "  • 点: lv00_add_point(engine, x_num, x_den, y_num, y_den)\n"
        "  • 线段: lv00_add_line_segment(engine, p1, p2)\n"
        "  • 直线: graph_add_line(engine->main_graph, p1, p2)\n"
        "  • 射线: graph_add_ray(engine->main_graph, origin, direction)\n"
        "  • 圆: graph_add_circle(engine->main_graph, center, radius_point)\n"
        "  • 圆弧: graph_add_arc(engine->main_graph, center, p1, p2)\n"
        "  • 椭圆: graph_add_ellipse(engine->main_graph, center, a, b)\n"
        "  • 多边形: graph_add_polygon(engine->main_graph, points, count)\n"
        "\n"
        "■ 支持的约束：\n"
        "  • 关联约束（点在线上）: lv00_add_constraint_incidence(engine, point, line)\n"
        "  • 距离约束: graph_add_distance_constraint(engine->main_graph, p1, p2, dist)\n"
        "  • 角度约束: graph_add_angle_constraint(engine->main_graph, l1, l2, angle)\n"
        "  • 垂直约束: graph_add_perpendicular_constraint(engine->main_graph, l1, l2)\n"
        "  • 平行约束: graph_add_parallel_constraint(engine->main_graph, l1, l2)\n"
        "  • 相切约束: graph_add_tangent_constraint(engine->main_graph, c, l)\n"
        "\n"
        "■ 常用预设：\n"
        "  • 中点构造: PRESET_MIDPOINT\n"
        "  • 垂直平分线: PRESET_PERPENDICULAR_BISECTOR\n"
        "  • 角平分线: PRESET_ANGLE_BISECTOR\n"
        "  • 外心构造: PRESET_CIRCUMCENTER\n"
        "  • 内心构造: PRESET_INCENTER\n"
        "  • 垂心构造: PRESET_ORTHOCENTER\n"
        "  • 重心构造: PRESET_CENTROID\n"
        "\n"
        "■ 示例：构造等边三角形并证明\n"
        "  1. 创建三个点 A(0,0), B(1,0), C(0.5, sqrt(3)/2)\n"
        "  2. 添加三条边 AB, BC, CA\n"
        "  3. 添加距离约束 AB=BC=CA\n"
        "  4. 调用 lv00_solve(engine) 验证\n"
    );
}

void guide_analytic_geometry_cn(char *buf, size_t buf_size) {
    if (!buf || buf_size == 0)
        return;

    snprintf(buf, buf_size,
        "【解析几何研究指南】\n"
        "\n"
        "■ 坐标系变换：\n"
        "  • 笛卡尔坐标 ↔ 极坐标转换\n"
        "  • 坐标平移: translate_coord(x, y, dx, dy)\n"
        "  • 坐标旋转: rotate_coord(x, y, angle)\n"
        "  • 缩放变换: scale_coord(x, y, sx, sy)\n"
        "\n"
        "■ 距离与角度计算：\n"
        "  • 两点距离: distance_point_to_point(p1, p2)\n"
        "  • 点到直线距离: distance_point_to_line(p, line)\n"
        "  • 点到圆距离: distance_point_to_circle(p, circle)\n"
        "  • 两直线夹角: angle_line_to_line(l1, l2)\n"
        "  • 向量夹角: angle_vector_to_vector(v1, v2)\n"
        "\n"
        "■ 面积与周长计算：\n"
        "  • 三角形面积: area_triangle(p1, p2, p3)\n"
        "  • 多边形面积: area_polygon(points, n)\n"
        "  • 圆面积: area_circle(radius)\n"
        "  • 椭圆面积: area_ellipse(a, b)\n"
        "\n"
        "■ 交点计算：\n"
        "  • 直线与直线交点: intersection_line_line(l1, l2)\n"
        "  • 直线与圆交点: intersection_line_circle(l, c)\n"
        "  • 圆与圆交点: intersection_circle_circle(c1, c2)\n"
    );
}

void guide_projective_geometry_cn(char *buf, size_t buf_size) {
    if (!buf || buf_size == 0)
        return;

    snprintf(buf, buf_size,
        "【射影几何研究指南】\n"
        "\n"
        "■ 射影平面基础：\n"
        "  • 齐次坐标: (x, y, w) 表示仿射点 (x/w, y/w)\n"
        "  • 无穷远直线: w = 0 的所有点\n"
        "  • 无穷远点: 斜率相同的平行线交点\n"
        "\n"
        "■ 射影变换：\n"
        "  • 透视映射: perspective_project(point, center, plane)\n"
        "  • 仿射变换: affine_transform(point, matrix)\n"
        "  • 单应变换: homography_transform(point, H)\n"
        "\n"
        "■ 射影不变量：\n"
        "  • 交比: cross_ratio(p1, p2, p3, p4)\n"
        "  • 调和共轭: harmonic_conjugate(p1, p2, p3)\n"
        "\n"
        "■ 二次曲线：\n"
        "  • 射影分类: conic_projective_classify(coeffs)\n"
        "  • 配谱变换: conic_diagonalize(conic)\n"
    );
}

/* ============================================================
 * 代数学研究接口实现
 * ============================================================ */

void guide_linear_algebra_cn(char *buf, size_t buf_size) {
    if (!buf || buf_size == 0)
        return;

    snprintf(buf, buf_size,
        "【线性代数研究指南】\n"
        "\n"
        "■ 矩阵运算：\n"
        "  • 矩阵加法: matrix_add(A, B)\n"
        "  • 矩阵乘法: matrix_multiply(A, B)\n"
        "  • 矩阵转置: matrix_transpose(A)\n"
        "  • 矩阵求逆: matrix_inverse(A)\n"
        "  • 矩阵行列式: matrix_determinant(A)\n"
        "\n"
        "■ 向量运算：\n"
        "  • 向量加法: vector_add(v1, v2)\n"
        "  • 向量数乘: vector_scale(v, scalar)\n"
        "  • 点积: vector_dot(v1, v2)\n"
        "  • 叉积: vector_cross(v1, v2)\n"
        "  • 向量范数: vector_norm(v)\n"
        "  • 投影: vector_project(v, basis)\n"
        "\n"
        "■ 线性方程组：\n"
        "  • 高斯消元: gaussian_elimination(A, b)\n"
        "  • LU分解: lu_decompose(A)\n"
        "  • QR分解: qr_decompose(A)\n"
        "  • 最小二乘: least_squares(A, b)\n"
        "\n"
        "■ 特征值与特征向量：\n"
        "  • 特征值求解: eigen_values(A)\n"
        "  • 特征向量求解: eigen_vectors(A)\n"
        "  • 特征多项式: characteristic_polynomial(A)\n"
    );
}

void guide_abstract_algebra_cn(char *buf, size_t buf_size) {
    if (!buf || buf_size == 0)
        return;

    snprintf(buf, buf_size,
        "【抽象代数研究指南】\n"
        "\n"
        "■ 群论：\n"
        "  • 群构造: group_create(elements, operation)\n"
        "  • 陪集计算: coset(G, H, element)\n"
        "  • 商群: quotient_group(G, N)\n"
        "  • 同态映射: group_homomorphism(G1, G2, mapping)\n"
        "  • 核与像: kernel_and_image(homomorphism)\n"
        "\n"
        "■ 环与域：\n"
        "  • 环构造: ring_create(elements, add, mul)\n"
        "  • 多项式环: polynomial_ring(R, variable)\n"
        "  • 商环: quotient_ring(R, ideal)\n"
        "  • 扩域: field_extension(F, irreducible_poly)\n"
        "\n"
        "■ 有限域：\n"
        "  • GF(p) 构造: finite_field_prime(p)\n"
        "  • GF(p^n) 构造: finite_field_extension(p, irreducible)\n"
        "  • 原根计算: primitive_root(field)\n"
        "  • 离散对数: discrete_log(field, a, g)\n"
    );
}

void guide_number_theory_cn(char *buf, size_t buf_size) {
    if (!buf || buf_size == 0)
        return;

    snprintf(buf, buf_size,
        "【数论研究指南】\n"
        "\n"
        "■ 基础数论运算：\n"
        "  • 最大公约数: gcd(a, b) - 欧几里得算法\n"
        "  • 最小公倍数: lcm(a, b)\n"
        "  • 扩展欧几里得: extended_gcd(a, b) - 求逆元\n"
        "  • 素数判定: is_prime(n)\n"
        "  • 质因数分解: factorize(n)\n"
        "\n"
        "■ 模运算：\n"
        "  • 模加法: mod_add(a, b, m)\n"
        "  • 模乘法: mod_mul(a, b, m)\n"
        "  • 模幂: mod_pow(base, exp, m) - 快速幂\n"
        "  • 模逆元: mod_inverse(a, m) - 扩展欧几里得\n"
        "\n"
        "■ 中国剩余定理：\n"
        "  • crt(remainders, moduli) - 求解同余方程组\n"
        "\n"
        "■ 原根与离散对数：\n"
        "  • primitive_root(p) - 求原根\n"
        "  • discrete_log(g, a, p) - 求离散对数\n"
        "  • baby_step_giant_step - 离散对数算法\n"
        "\n"
        "■ 连分数：\n"
        "  • continued_fraction(x) - 展开连分数\n"
        "  • convergent(cf) - 求逼近有理数\n"
    );
}

void guide_polynomial_algebra_cn(char *buf, size_t buf_size) {
    if (!buf || buf_size == 0)
        return;

    snprintf(buf, buf_size,
        "【多项式代数研究指南】\n"
        "\n"
        "■ 基本运算：\n"
        "  • 多项式加减乘除: poly_add/sub/mul/div\n"
        "  • 多项式求导: poly_derivative(p)\n"
        "  • 多项式积分: poly_integral(p)\n"
        "  • 多项式最大公因式: poly_gcd(p1, p2)\n"
        "\n"
        "■ 因式分解：\n"
        "  • 有理根检验: rational_root_test(p)\n"
        "  • 平方-free分解: square_free_factor(p)\n"
        "  • 有限域分解: factor_ff(p, field)\n"
        "\n"
        "■ 结式与判别式：\n"
        "  • 结式计算: resultant(p, q, variable)\n"
        "  • 判别式: discriminant(p, variable)\n"
        "\n"
        "■ Groebner基：\n"
        "  • groebner_basis(polynomials, order)\n"
        "  • 多元多项式除法: poly_division(p, divisors)\n"
        "  • 消元理想: elimination_ideal(I, vars_to_eliminate)\n"
    );
}

/* ============================================================
 * 拓扑学研究接口实现
 * ============================================================ */

void guide_point_set_topology_cn(char *buf, size_t buf_size) {
    if (!buf || buf_size == 0)
        return;

    snprintf(buf, buf_size,
        "【点集拓扑研究指南】\n"
        "\n"
        "■ 拓扑空间构造：\n"
        "  • 度量空间: metric_space_create(distance_function)\n"
        "  • 开集构造: open_set_create(points, basis)\n"
        "  • 闭集计算: closure(A)\n"
        "  • 内部计算: interior(A)\n"
        "\n"
        "■ 拓扑性质判定：\n"
        "  • 紧致性: is_compact(space)\n"
        "  • 连通性: is_connected(space)\n"
        "  •道路连通: is_path_connected(space)\n"
        "  • Hausdorff性: is_hausdorff(space)\n"
        "\n"
        "■ 连续映射：\n"
        "  • 连续性检验: is_continuous(f, domain, codomain)\n"
        "  • 同胚判定: is_homeomorphism(f)\n"
        "  • 商空间: quotient_space(X, equivalence)\n"
    );
}

void guide_algebraic_topology_cn(char *buf, size_t buf_size) {
    if (!buf || buf_size == 0)
        return;

    snprintf(buf, buf_size,
        "【代数拓扑研究指南】\n"
        "\n"
        "■ 单纯复形：\n"
        "  • 复形构造: simplicial_complex_create(simplices)\n"
        "  • 边界算子: boundary_operator(complex, dimension)\n"
        "  • 链群: chain_group(complex, dimension)\n"
        "\n"
        "■ 同调群：\n"
        "  • 零维同调: homology_group_0(complex)\n"
        "  • 一维同调: homology_group_1(complex)\n"
        "  • Betti数: betti_numbers(complex)\n"
        "  • 欧拉示性数: euler_characteristic(complex)\n"
        "\n"
        "■ 基本群：\n"
        "  • 基本群计算: fundamental_group(space, base_point)\n"
        "  • 覆叠空间: covering_space(space)\n"
        "  • 提升: lift_path(path, covering)\n"
    );
}

/* ============================================================
 * 数理逻辑研究接口实现
 * ============================================================ */

void guide_propositional_logic_cn(char *buf, size_t buf_size) {
    if (!buf || buf_size == 0)
        return;

    snprintf(buf, buf_size,
        "【命题逻辑研究指南】\n"
        "\n"
        "■ 命题构造：\n"
        "  • 合取: proposition_conjunction(p, q) - P ∧ Q\n"
        "  • 析取: proposition_disjunction(p, q) - P ∨ Q\n"
        "  • 否定: proposition_negation(p) - ¬P\n"
        "  • 蕴含: proposition_implication(p, q) - P → Q\n"
        "  • 等价: proposition_biconditional(p, q) - P ↔ Q\n"
        "\n"
        "■ 语义分析：\n"
        "  • 真值表生成: truth_table(formula)\n"
        "  • 永真式判定: is_tautology(formula)\n"
        "  • 矛盾式判定: is_contradiction(formula)\n"
        "  • 可满足性: is_satisfiable(formula)\n"
        "\n"
        "■ 范式转换：\n"
        "  • 合取范式: conjunctive_normal_form(formula)\n"
        "  • 析取范式: disjunctive_normal_form(formula)\n"
        "  • 主合取范式: principal_cnf(formula)\n"
        "  • 主析取范式: principal_dnf(formula)\n"
    );
}

void guide_first_order_logic_cn(char *buf, size_t buf_size) {
    if (!buf || buf_size == 0)
        return;

    snprintf(buf, buf_size,
        "【一阶逻辑研究指南】\n"
        "\n"
        "■ 量词操作：\n"
        "  • 全称量词: universal_quantifier(variable, formula)\n"
        "  • 存在量词: existential_quantifier(variable, formula)\n"
        "  • 量词作用域: quantifier_scope(q)\n"
        "\n"
        "■ 规范化：\n"
        "  • 前束范式: prenex_normal_form(formula)\n"
        "  • Skolem化: skolemize(formula)\n"
        "  • Herbrand展开: herbrand_expansion(formula)\n"
        "\n"
        "■ 推理规则：\n"
        "  • 全称实例化: universal_instantiation(forall_x_Px, c)\n"
        "  • 存在实例化: existential_instantiation(exists_x_Px, c)\n"
        "  • 全称推广: universal_generalization(Pc)\n"
        "  • 存在推广: existential_generalization(Pc)\n"
    );
}

/* ============================================================
 * 范畴论研究接口实现
 * ============================================================ */

void guide_category_theory_cn(char *buf, size_t buf_size) {
    if (!buf || buf_size == 0)
        return;

    snprintf(buf, buf_size,
        "【范畴论研究指南】\n"
        "\n"
        "■ 范畴基础：\n"
        "  • 范畴构造: category_create(objects, morphisms)\n"
        "  • 对象操作: category_object(cat, name)\n"
        "  • 态射操作: category_morphism(cat, source, target, map)\n"
        "\n"
        "■ 函子：\n"
        "  • 函子构造: functor_create(C, D, object_map, morphism_map)\n"
        "  • 协变函子: covariant_functor(F, C, D)\n"
        "  • 反变函子: contravariant_functor(F, C, D)\n"
        "\n"
        "■ 自然变换：\n"
        "  • 自然变换: natural_transformation(F, G, components)\n"
        "  • 自然同构: natural_isomorphism(F, G)\n"
        "\n"
        "■ 泛构造：\n"
        "  • 始对象: initial_object(C)\n"
        "  • 终对象: terminal_object(C)\n"
        "  • 积对象: product_object(C, a, b)\n"
        "  • 余积对象: coproduct_object(C, a, b)\n"
    );
}

/* ============================================================
 * 同调代数研究接口实现
 * ============================================================ */

void guide_homological_algebra_cn(char *buf, size_t buf_size) {
    if (!buf || buf_size == 0)
        return;

    snprintf(buf, buf_size,
        "【同调代数研究指南】\n"
        "\n"
        "■ 链复形：\n"
        "  • 链复形构造: chain_complex_create(differentials)\n"
        "  • 边界算子: boundary_operator(complex, n)\n"
        "  • 边缘群: image(partial_n_plus_1)\n"
        "  • 闭链群: kernel(partial_n)\n"
        "\n"
        "■ 同调群：\n"
        "  • 同调群计算: homology_group(complex, n)\n"
        "  • 水平同调: homology(complex)\n"
        "  • 欧拉示性数: euler_characteristic(complex)\n"
        "\n"
        "■ 正合序列：\n"
        "  • 短正合列: short_exact_sequence(A, B, C)\n"
        "  • 长正合列: long_exact_sequence(ses)\n"
        "  • 五引理: five_lemma(comm_diagram)\n"
        "  • 九引理: nine_lemma(comm_cube)\n"
        "\n"
        "■ Ext与Tor：\n"
        "  • Ext群: ext_group(A, B, n)\n"
        "  • Tor函子: tor_module(A, B, n)\n"
    );
}

/* ============================================================
 * 微分几何研究接口实现
 * ============================================================ */

void guide_differential_geometry_cn(char *buf, size_t buf_size) {
    if (!buf || buf_size == 0)
        return;

    snprintf(buf, buf_size,
        "【微分几何研究指南】\n"
        "\n"
        "■ 流形基础：\n"
        "  • 流形构造: manifold_create(dim, charts)\n"
        "  • 坐标卡: chart_create(domain, coord_map)\n"
        "  • 光滑映射: smooth_map(f, M, N)\n"
        "\n"
        "■ 切向量与余切向量：\n"
        "  • 切空间: tangent_space(M, p)\n"
        "  • 切向量: tangent_vector(X, p)\n"
        "  • 余切空间: cotangent_space(M, p)\n"
        "  • 微分: differential(df, p)\n"
        "\n"
        "■ 度量与曲率：\n"
        "  • 度量张量: metric_tensor(g, M)\n"
        "  • Riemann曲率: riemann_curvature(R, X, Y, Z)\n"
        "  • Ricci曲率: ricci_curvature(Ric, X, Y)\n"
        "  • 数量曲率: scalar_curvature(R)\n"
        "\n"
        "■ 联络：\n"
        "  • Levi-Civita联络: levi_civita_connection(g)\n"
        "  • 协变导数: covariant_derivative(nabla, X, Y)\n"
        "  • 平行移动: parallel_transport(gamma, v, t)\n"
    );
}

/* ============================================================
 * 综合指南实现
 * ============================================================ */

int guide_generate_full_cn(char *buf, size_t buf_size) {
    if (!buf || buf_size == 0)
        return -1;

    int offset = 0;
    int remaining = (int)buf_size;

    offset += snprintf(buf + offset, (size_t)remaining,
        "╔══════════════════════════════════════════════════════════════╗\n"
        "║            Lv-00 理论数学研究系统 - 完整指南                    ║\n"
        "╚══════════════════════════════════════════════════════════════╝\n"
        "\n"
        "■ 版本: %s\n"
        "■ 架构: 五层单向依赖 (Parser → Resource → Geometry → Reasoning → Output)\n"
        "\n",
        "3.5.0"
    );

    if (offset < 0 || offset >= (int)buf_size)
        return offset;

    remaining = (int)buf_size - offset;

    /* 几何学 */
    char geom_buf[2048];
    guide_euclidean_geometry_cn(geom_buf, sizeof(geom_buf));
    offset += snprintf(buf + offset, (size_t)remaining, "%s\n", geom_buf);
    remaining = (int)buf_size - offset;
    if (remaining <= 0) return offset;

    /* 代数学 */
    char alg_buf[2048];
    guide_linear_algebra_cn(alg_buf, sizeof(alg_buf));
    offset += snprintf(buf + offset, (size_t)remaining, "%s\n", alg_buf);
    remaining = (int)buf_size - offset;
    if (remaining <= 0) return offset;

    /* 拓扑学 */
    char topo_buf[2048];
    guide_point_set_topology_cn(topo_buf, sizeof(topo_buf));
    offset += snprintf(buf + offset, (size_t)remaining, "%s\n", topo_buf);
    remaining = (int)buf_size - offset;
    if (remaining <= 0) return offset;

    /* 数理逻辑 */
    char logic_buf[2048];
    guide_propositional_logic_cn(logic_buf, sizeof(logic_buf));
    offset += snprintf(buf + offset, (size_t)remaining, "%s\n", logic_buf);
    remaining = (int)buf_size - offset;
    if (remaining <= 0) return offset;

    /* 范畴论 */
    char cat_buf[1024];
    guide_category_theory_cn(cat_buf, sizeof(cat_buf));
    offset += snprintf(buf + offset, (size_t)remaining, "%s\n", cat_buf);
    remaining = (int)buf_size - offset;
    if (remaining <= 0) return offset;

    /* 同调代数 */
    char homo_buf[1024];
    guide_homological_algebra_cn(homo_buf, sizeof(homo_buf));
    offset += snprintf(buf + offset, (size_t)remaining, "%s\n", homo_buf);

    return offset;
}

int guide_quick_start_cn(char *buf, size_t buf_size) {
    if (!buf || buf_size == 0)
        return -1;

    return snprintf(buf, buf_size,
        "╔══════════════════════════════════════════════════════════════╗\n"
        "║                  Lv-00 快速开始指南                           ║\n"
        "╚══════════════════════════════════════════════════════════════╝\n"
        "\n"
        "【第一步：初始化系统】\n"
        "  if (!lv00_init()) {\n"
        "      fprintf(stderr, \"系统初始化失败\\n\");\n"
        "      return 1;\n"
        "  }\n"
        "\n"
        "【第二步：创建引擎】\n"
        "  LV00Engine *engine = lv00_engine_create();\n"
        "\n"
        "【第三步：构造几何问题】\n"
        "  // 创建点\n"
        "  int p1 = lv00_add_point(engine, 0, 1, 0, 1);  // (0, 0)\n"
        "  int p2 = lv00_add_point(engine, 3, 1, 0, 1);  // (3, 0)\n"
        "  int p3 = lv00_add_point(engine, 0, 1, 4, 1);  // (0, 4)\n"
        "\n"
        "  // 创建线段（直角三角形）\n"
        "  lv00_add_line_segment(engine, p1, p2);  // AB\n"
        "  lv00_add_line_segment(engine, p2, p3);  // BC\n"
        "  lv00_add_line_segment(engine, p3, p1);  // CA\n"
        "\n"
        "【第四步：归一化与求解】\n"
        "  NormalizationResult *nr = lv00_normalize(engine, true);\n"
        "  EngineSolveResult result = lv00_solve(engine);\n"
        "\n"
        "【第五步：检查结果】\n"
        "  printf(\"求解结果: %s\\n\", solver_result_to_string_cn(result));\n"
        "\n"
        "【第六步：清理资源】\n"
        "  lv00_engine_destroy(engine);\n"
        "  lv00_cleanup();\n"
        "\n"
        "更多示例请参考: test/examples/ 目录\n"
    );
}

int guide_symbol_reference_cn(char *buf, size_t buf_size) {
    if (!buf || buf_size == 0)
        return -1;

    return snprintf(buf, buf_size,
        "╔══════════════════════════════════════════════════════════════╗\n"
        "║                  数学符号对照表                                 ║\n"
        "╚══════════════════════════════════════════════════════════════╝\n"
        "\n"
        "【几何符号】\n"
        "  • A, B, C, ... - 点\n"
        "  • l, m, n, ... - 直线\n"
        "  • s, t, u, ... - 线段\n"
        "  • C(O, r)      - 圆（圆心O，半径r）\n"
        "  • ∠ABC         - 角ABC\n"
        "  • ⊥            - 垂直\n"
        "  • ∥            - 平行\n"
        "  • ≅            - 全等\n"
        "  • ∼            - 相似\n"
        "\n"
        "【代数符号】\n"
        "  • det(A)       - 行列式\n"
        "  • tr(A)        - 矩阵的迹\n"
        "  • rank(A)      - 矩阵的秩\n"
        "  • ker(φ)       - 核\n"
        "  • im(φ)        - 像\n"
        "  • dim(V)       - 向量空间维度\n"
        "\n"
        "【逻辑符号】\n"
        "  • ∧            - 合取（且）\n"
        "  • ∨            - 析取（或）\n"
        "  • ¬            - 否定（非）\n"
        "  • →            - 蕴含\n"
        "  • ↔            - 等价\n"
        "  • ∀            - 全称量词\n"
        "  • ∃            - 存在量词\n"
        "\n"
        "【拓扑符号】\n"
        "  • ∂X           - 边界\n"
        "  • int(X)       - 内部\n"
        "  • cl(X)        - 闭包\n"
        "  • X°           - 内部\n"
        "  • ≅            - 同胚\n"
        "\n"
        "【范畴论符号】\n"
        "  • Obj(C)       - 范畴C的对象类\n"
        "  • Mor(C)       - 范畴C的态射类\n"
        "  • dom(f)       - 态射的定义域\n"
        "  • cod(f)       - 态射的余域\n"
        "  • F ○ G        - 函子复合\n"
        "  • α : F ⇒ G   - 自然变换\n"
    );
}

int guide_preset_math_definition_cn(const char *preset_name, char *buf, size_t buf_size) {
    if (!buf || buf_size == 0 || !preset_name)
        return -1;

    /* 常用预设的数学定义表 */
    typedef struct {
        const char *name;
        const char *cn_name;
        const char *definition;
        const char *formula;
    } PresetMathDef;

    static const PresetMathDef defs[] = {
        {"midpoint", "中点", "两点连线段的中点", "M = (A + B) / 2"},
        {"centroid", "重心", "三角形三条中线的交点", "G = (A + B + C) / 3"},
        {"circumcenter", "外心", "三角形外接圆的圆心", "OA = OB = OC"},
        {"incenter", "内心", "三角形内切圆的圆心", "到三边距离相等"},
        {"orthocenter", "垂心", "三角形三条高的交点", "AH ⊥ BC, BH ⊥ AC, CH ⊥ AB"},
        {"perpendicular_bisector", "垂直平分线", "线段的垂直平分线", "MA = MB, MA ⊥ AB"},
        {"angle_bisector", "角平分线", "角的平分线", "∠BAD = ∠DAC"},
        {"median", "中线", "顶点到对边中点的连线", "M 为 BC 中点, AM 为中线"},
    };

    for (size_t i = 0; i < sizeof(defs) / sizeof(defs[0]); i++) {
        if (strcmp(preset_name, defs[i].name) == 0) {
            return snprintf(buf, buf_size,
                "【%s - %s】\n"
                "定义: %s\n"
                "公式: %s\n",
                defs[i].name, defs[i].cn_name,
                defs[i].definition, defs[i].formula
            );
        }
    }

    return snprintf(buf, buf_size,
        "【%s】\n"
        "该预设的数学定义请参考 preset 模块文档\n",
        preset_name
    );
}
