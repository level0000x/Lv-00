/**
 * @file preset_differential_geometry.h
 * @brief 微分几何预设函数块 - 头文件
 *
 * 提供理论数学研究中常用的微分几何运算预设函数块，包括：
 *   - 曲线理论：参数曲线、弧长、曲率、挠率、Frenet标架
 *   - 曲面理论：参数曲面、基本形式、曲率、法向量、面积
 *   - 测地线：测地线方程、测地距离、测地曲率、指数映射、平行移动
 *   - 张量运算：张量创建、缩并、张量积、协变导数、克里斯托费尔符号
 *   - 流形理论：坐标卡、转移映射、切空间、余切空间
 *
 * @module DifferentialGeometry
 * @category PRESET_CATEGORY_GEOMETRY
 * @version 3.2.0
 * @author Lv-00 开发团队
 */

#ifndef LV00_PRESET_DIFFERENTIAL_GEOMETRY_H
#define LV00_PRESET_DIFFERENTIAL_GEOMETRY_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义
 * ============================================================ */

/* -------------------- 曲线理论 -------------------- */

/**
 * @brief 参数曲线定义 gamma(t) = (x(t), y(t), z(t))
 *
 * @details 数学定义：以参数 t 定义的空间曲线，gamma: I → R^3。
 *
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION
 *       输出: PRESET_TYPE_CURVE | 复杂度: O(1)
 */
#define PRESET_CURVE_PARAMETRIC        "curve_parametric"

/**
 * @brief 计算曲线弧长 s = integral |gamma'(t)| dt
 *
 * @details 数学定义：曲线的自然参数，弧长参数化使得 |gamma'(s)| = 1。
 *
 * @note 输入: PRESET_TYPE_CURVE | 输出: PRESET_TYPE_SCALAR | 复杂度: O(n)
 */
#define PRESET_CURVE_ARC_LENGTH        "curve_arc_length"

/**
 * @brief 计算曲线曲率 kappa = |gamma' × gamma''| / |gamma'|^3
 *
 * @details 数学定义：曲线弯曲程度的度量，密切圆半径的倒数。
 *
 * @note 输入: PRESET_TYPE_CURVE | 输出: PRESET_TYPE_FUNCTION | 复杂度: O(n)
 */
#define PRESET_CURVE_CURVATURE         "curve_curvature"

/**
 * @brief 计算曲线挠率 tau = (gamma' × gamma'') · gamma''' / |gamma' × gamma''|^2
 *
 * @details 数学定义：曲线偏离密切平面程度的度量，
 *          tau = 0 ⇔ 曲线为平面曲线。
 *
 * @note 输入: PRESET_TYPE_CURVE | 输出: PRESET_TYPE_FUNCTION | 复杂度: O(n)
 */
#define PRESET_CURVE_TORSION           "curve_torsion"

/**
 * @brief 计算 Frenet 标架 (T, N, B)
 *
 * @details 数学定义：曲线每点的局部正交坐标系，T 为切向量，
 *          N 为主法向量，B = T × N 为副法向量。
 *          Frenet 公式：T' = kappa*N, N' = -kappa*T + tau*B, B' = -tau*N。
 *
 * @note 输入: PRESET_TYPE_CURVE | 输出: PRESET_TYPE_TUPLE (T, N, B) | 复杂度: O(n)
 */
#define PRESET_CURVE_FRENET_FRAME      "curve_frenet_frame"

/**
 * @brief 计算曲线法向量 N（Frenet 标架的主法向量）
 *
 * @details 数学定义：N = (T'/kappa) = (gamma' × gamma'') × gamma' / (|gamma' × gamma''| · |gamma'|)
 *
 * @note 输入: PRESET_TYPE_CURVE | 输出: PRESET_TYPE_VECTOR | 复杂度: O(n)
 */
#define PRESET_CURVE_NORMAL_VECTOR     "curve_normal_vector"

/**
 * @brief 计算曲线副法向量 B = T × N
 *
 * @details 数学定义：B = (gamma' × gamma'') / |gamma' × gamma''|
 *
 * @note 输入: PRESET_TYPE_CURVE | 输出: PRESET_TYPE_VECTOR | 复杂度: O(n)
 */
#define PRESET_CURVE_BINORMAL_VECTOR   "curve_binormal_vector"

/**
 * @brief 计算密切圆（曲率圆）
 *
 * @details 数学定义：在给定点处最贴近曲线的圆，
 *          半径为 1/kappa，圆心在法向量方向距离 1/kappa 处。
 *
 * @note 输入: PRESET_TYPE_CURVE, PRESET_TYPE_SCALAR | 输出: PRESET_TYPE_CIRCLE | 复杂度: O(1)
 */
#define PRESET_CURVE_OSCULATING_CIRCLE "curve_osculating_circle"

/* -------------------- 曲面理论 -------------------- */

/**
 * @brief 参数曲面定义 r(u, v) = (x(u,v), y(u,v), z(u,v))
 *
 * @details 数学定义：以两参数 (u, v) 定义的二维嵌入曲面 r: R^2 → R^3。
 *
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION
 *       输出: PRESET_TYPE_SURFACE | 复杂度: O(1)
 */
#define PRESET_SURFACE_PARAMETRIC              "surface_parametric"

/**
 * @brief 计算第一基本形式 I = E*du^2 + 2F*du*dv + G*dv^2
 *
 * @details 数学定义：E = r_u·r_u, F = r_u·r_v, G = r_v·r_v，
 *          度量曲面上弧长和角度，由 Riemann 度量 g_{ij} 确定。
 *
 * @note 输入: PRESET_TYPE_SURFACE | 输出: PRESET_TYPE_TUPLE (E, F, G) | 复杂度: O(n)
 */
#define PRESET_SURFACE_FIRST_FUNDAMENTAL       "surface_first_fundamental"

/**
 * @brief 计算第二基本形式 II = L*du^2 + 2M*du*dv + N*dv^2
 *
 * @details 数学定义：L = r_uu·n, M = r_uv·n, N = r_vv·n（n 为单位法向量），
 *          度量曲面的弯曲程度。
 *
 * @note 输入: PRESET_TYPE_SURFACE | 输出: PRESET_TYPE_TUPLE (L, M, N) | 复杂度: O(n)
 */
#define PRESET_SURFACE_SECOND_FUNDAMENTAL      "surface_second_fundamental"

/**
 * @brief 计算高斯曲率 K = det(II)/det(I) = (LN-M^2)/(EG-F^2)
 *
 * @details 数学定义：曲面内蕴几何量，Gauss 绝妙定理：K 仅由第一基本形式决定。
 *          K > 0 椭圆点，K < 0 双曲点，K = 0 抛物点。
 *
 * @note 输入: PRESET_TYPE_SURFACE | 输出: PRESET_TYPE_FUNCTION | 复杂度: O(n)
 */
#define PRESET_SURFACE_GAUSS_CURVATURE         "surface_gauss_curvature"

/**
 * @brief 计算平均曲率 H = (EN - 2FM + GL)/(2(EG-F^2))
 *
 * @details 数学定义：H = (kappa_1 + kappa_2)/2，主曲率的算术平均。
 *          极小曲面满足 H ≡ 0。
 *
 * @note 输入: PRESET_TYPE_SURFACE | 输出: PRESET_TYPE_FUNCTION | 复杂度: O(n)
 */
#define PRESET_SURFACE_MEAN_CURVATURE          "surface_mean_curvature"

/**
 * @brief 计算主曲率 kappa_1, kappa_2
 *
 * @details 数学定义：Weingarten 映射的特征值，
 *          kappa = H ± sqrt(H^2 - K)，K = kappa_1 * kappa_2。
 *
 * @note 输入: PRESET_TYPE_SURFACE | 输出: PRESET_TYPE_TUPLE (kappa_1, kappa_2) | 复杂度: O(n)
 */
#define PRESET_SURFACE_PRINCIPAL_CURVATURES    "surface_principal_curvatures"

/**
 * @brief 计算曲面单位法向量 n = (r_u × r_v) / |r_u × r_v|
 *
 * @details 数学定义：曲面在每点的单位法向量，方向由定向决定。
 *
 * @note 输入: PRESET_TYPE_SURFACE | 输出: PRESET_TYPE_VECTOR | 复杂度: O(n)
 */
#define PRESET_SURFACE_NORMAL                  "surface_normal"

/**
 * @brief 计算曲面面积 A = integral over D |r_u × r_v| du dv
 *
 * @details 数学定义：参数区域 D 上曲面片的面积积分。
 *
 * @note 输入: PRESET_TYPE_SURFACE, PRESET_TYPE_REGION
 *       输出: PRESET_TYPE_SCALAR | 复杂度: O(n^2)
 */
#define PRESET_SURFACE_AREA                    "surface_area"

/* -------------------- 测地线 -------------------- */

/**
 * @brief 测地线微分方程 nabla_{gamma'} gamma' = 0
 *
 * @details 数学定义：曲面上加速度可忽略的路径，是直线在曲面上的推广。
 *          满足测地线方程：gamma''^k + Gamma^k_{ij} * gamma'^i * gamma'^j = 0。
 *
 * @note 输入: PRESET_TYPE_SURFACE, PRESET_TYPE_TUPLE (起点, 方向)
 *       输出: PRESET_TYPE_CURVE | 复杂度: O(n)
 */
#define PRESET_GEODESIC_EQUATION       "geodesic_equation"

/**
 * @brief 计算测地线距离 d(p, q) = inf_γ length(γ)
 *
 * @details 数学定义：曲面上两点间沿曲面的最短路径长度。
 *
 * @note 输入: PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_SURFACE
 *       输出: PRESET_TYPE_SCALAR | 复杂度: O(n^2)
 */
#define PRESET_GEODESIC_DISTANCE       "geodesic_distance"

/**
 * @brief 计算测地曲率 kappa_g
 *
 * @details 数学定义：曲线在曲面切平面上的投影曲率。
 *          kappa^2 = kappa_g^2 + kappa_n^2（曲率和法曲率的勾股关系）。
 *
 * @note 输入: PRESET_TYPE_CURVE, PRESET_TYPE_SURFACE
 *       输出: PRESET_TYPE_FUNCTION | 复杂度: O(n)
 */
#define PRESET_GEODESIC_CURVATURE      "geodesic_curvature"

/**
 * @brief 指数映射 exp_p: T_pM → M
 *
 * @details 数学定义：从切向量出发沿测地线到达流形上的点。
 *          在 Riemann 流形上 exp_p 是局部微分同胚。
 *
 * @note 输入: PRESET_TYPE_POINT, PRESET_TYPE_VECTOR
 *       输出: PRESET_TYPE_POINT | 复杂度: O(n)
 */
#define PRESET_EXPONENTIAL_MAP         "exponential_map"

/**
 * @brief 平行移动：沿曲线保持切向量"平行"
 *
 * @details 数学定义：满足 nabla_{gamma'} v = 0 的向量场 v 沿曲线 gamma 的演化，
 *          在平坦空间中退化为常向量平移。
 *
 * @note 输入: PRESET_TYPE_CURVE, PRESET_TYPE_VECTOR
 *       输出: PRESET_TYPE_VECTOR | 复杂度: O(n)
 */
#define PRESET_PARALLEL_TRANSPORT      "parallel_transport"

/* -------------------- 张量运算 -------------------- */

/**
 * @brief 创建张量：给定阶数和分量，构造张量对象
 *
 * @details 数学定义：多重线性映射的数组表示，T^{i_1...i_p}_{j_1...j_q}。
 *
 * @note 输入: PRESET_TYPE_TUPLE, PRESET_TYPE_TUPLE
 *       输出: PRESET_TYPE_TENSOR | 复杂度: O(n^p+q)
 */
#define PRESET_TENSOR_CREATE           "tensor_create"

/**
 * @brief 张量缩并：对上/下指标求和的运算
 *
 * @details 数学定义：C^k_l(T)^{i_1...}_{j_1...} = sum_{alpha} T^{...i_{k-1} alpha i_{k+1}...}_{...j_{l-1} alpha j_{l+1}...}
 *
 * @note 输入: PRESET_TYPE_TENSOR, PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER
 *       输出: PRESET_TYPE_TENSOR | 复杂度: O(n^{p+q-2})
 */
#define PRESET_TENSOR_CONTRACT         "tensor_contract"

/**
 * @brief 张量积 (T ⊗ S)^{i_1...i_{p+r}}_{j_1...j_{q+s}} = T^{i_1...i_p}_{j_1...j_q} * S^{i_{p+1}...}_{j_{q+1}...}
 *
 * @details 数学定义：两个张量的外积，阶数相加。
 *
 * @note 输入: PRESET_TYPE_TENSOR, PRESET_TYPE_TENSOR
 *       输出: PRESET_TYPE_TENSOR | 复杂度: O(n^{p+q+r+s})
 */
#define PRESET_TENSOR_PRODUCT          "tensor_product"

/**
 * @brief 协变导数 nabla_k T^{i_1...}_{j_1...}
 *
 * @details 数学定义：张量场的协变微分，包含 Christoffel 符号修正项。
 *          与普通偏导不同，协变导数与坐标选取无关（张量性）。
 *
 * @note 输入: PRESET_TYPE_TENSOR | 输出: PRESET_TYPE_TENSOR | 复杂度: O(n^{p+q+1})
 */
#define PRESET_COVARIANT_DERIVATIVE    "covariant_derivative"

/**
 * @brief 计算 Christoffel 符号 Gamma^k_{ij} = (1/2) * g^{kl} * (partial_i g_{jl} + partial_j g_{il} - partial_l g_{ij})
 *
 * @details 数学定义：Levi-Civita 联络的 Christoffel 符号，
 *          由度量张量的一阶导数组合而成，不是张量。
 *
 * @note 输入: PRESET_TYPE_SURFACE | 输出: PRESET_TYPE_TUPLE | 复杂度: O(n^3)
 */
#define PRESET_CHRISTOFFEL_SYMBOLS     "christoffel_symbols"

/* -------------------- 流形理论 -------------------- */

/**
 * @brief 定义流形坐标卡 (U, phi)，phi: U → R^n
 *
 * @details 数学定义：流形局部与欧氏空间同胚的映射，
 *          坐标卡族构成流形图册。
 *
 * @note 输入: PRESET_TYPE_REGION, PRESET_TYPE_MAPPING
 *       输出: PRESET_TYPE_CHART | 复杂度: O(1)
 */
#define PRESET_MANIFOLD_CHART          "manifold_chart"

/**
 * @brief 计算转移映射 phi_j ∘ phi_i^{-1}: phi_i(U_i ∩ U_j) → phi_j(U_i ∩ U_j)
 *
 * @details 数学定义：两坐标卡在重叠区域上的坐标变换，
 *          要求转移映射光滑（C^∞），确保微分结构相容。
 *
 * @note 输入: PRESET_TYPE_CHART, PRESET_TYPE_CHART
 *       输出: PRESET_TYPE_MAPPING | 复杂度: O(n)
 */
#define PRESET_MANIFOLD_TRANSITION_MAP "manifold_transition_map"

/**
 * @brief 计算切空间 T_pM = {所有过 p 点的光滑曲线切向量}
 *
 * @details 数学定义：流形在 p 点的切空间，维数等于流形维数。
 *          T_pM 是线性空间，由坐标基 {partial/partial x^i|_p} 张成。
 *
 * @note 输入: PRESET_TYPE_POINT, PRESET_TYPE_MANIFOLD
 *       输出: PRESET_TYPE_SET | 复杂度: O(n)
 */
#define PRESET_MANIFOLD_TANGENT_SPACE  "manifold_tangent_space"

/**
 * @brief 计算余切空间 T*_pM = (T_pM)^*（切空间的对偶空间）
 *
 * @details 数学定义：由线性泛函 {dx^i|_p} 张成，
 *          余切向量映射 T_pM → R。1-微分形式是余切向量场。
 *
 * @note 输入: PRESET_TYPE_POINT, PRESET_TYPE_MANIFOLD
 *       输出: PRESET_TYPE_SET | 复杂度: O(n)
 */
#define PRESET_MANIFOLD_COTANGENT_SPACE "manifold_cotangent_space"

/* ============================================================
 * 微分几何 v5.0 统一宏（PRESET_DG_ 前缀，与 .c 对齐）
 * ============================================================ */

/**
 * @brief 微分几何模块预设函数块总数
 */
#define DIFFERENTIAL_GEOMETRY_PRESET_COUNT 25

/* ── 曲线论 ── */

/** @brief 弧长参数化：将曲线转换为弧长参数 gamma(s) */
#define PRESET_DG_ARC_LENGTH_PARAM         "dg_arc_length_param"
/** @brief Frenet 标架：计算曲线的 Frenet-Serret 标架 (T, N, B) */
#define PRESET_DG_FRENET_FRAME             "dg_frenet_frame"
/** @brief 曲率计算：计算空间曲线的曲率 kappa(s) */
#define PRESET_DG_CURVATURE                "dg_curvature"
/** @brief 挠率计算：计算空间曲线的挠率 tau(s) */
#define PRESET_DG_TORSION                  "dg_torsion"
/** @brief Bertrand 曲线判定：判定一对曲线是否为 Bertrand 曲线 */
#define PRESET_DG_BERTRAND_CURVE           "dg_bertrand_curve"

/* ── 曲面论 ── */

/** @brief 第一基本形式：计算曲面 Riemann 度量矩阵 (E, F, G) */
#define PRESET_DG_FIRST_FUNDAMENTAL_FORM   "dg_first_fundamental_form"
/** @brief 第二基本形式：计算曲面弯曲度量矩阵 (L, M, N) */
#define PRESET_DG_SECOND_FUNDAMENTAL_FORM  "dg_second_fundamental_form"
/** @brief 高斯曲率：计算曲面 Gauss 曲率 K = kappa_1 * kappa_2 */
#define PRESET_DG_GAUSS_CURVATURE          "dg_gauss_curvature"
/** @brief 平均曲率：计算平均曲率 H = (kappa_1 + kappa_2) / 2 */
#define PRESET_DG_MEAN_CURVATURE           "dg_mean_curvature"
/** @brief 主曲率：计算曲面的两个主曲率 kappa_1, kappa_2 */
#define PRESET_DG_PRINCIPAL_CURVATURES     "dg_principal_curvatures"
/** @brief Weingarten 映射：计算曲面形状算子 S = -(第一基本形式)^{-1} * (第二基本形式) */
#define PRESET_DG_WEINGARTEN_MAP           "dg_weingarten_map"

/* ── 联络与曲率 ── */

/** @brief Levi-Civita 联络：计算 Riemann 流形上唯一无挠度量相容联络 */
#define PRESET_DG_LEVI_CIVITA_CONNECTION   "dg_levi_civita_connection"
/** @brief Riemann 曲率张量：计算 (3,1) 型曲率张量 R^l_{ijk} */
#define PRESET_DG_RIEMANN_CURVATURE        "dg_riemann_curvature"
/** @brief Ricci 曲率张量：R_{ij} = R^k_{ikj}，Riemann 张量的缩并 */
#define PRESET_DG_RICCI_CURVATURE          "dg_ricci_curvature"
/** @brief 截面曲率：二维截面的截面曲率 K(X,Y) */
#define PRESET_DG_SECTIONAL_CURVATURE      "dg_sectional_curvature"
/** @brief 标量曲率：R = g^{ij} * R_{ij}，Ricci 曲率的进一步缩并 */
#define PRESET_DG_SCALAR_CURVATURE         "dg_scalar_curvature"

/* ── 测地线 ── */

/** @brief 测地线方程：求解 nabla_{gamma'} gamma' = 0 的微分方程 */
#define PRESET_DG_GEODESIC_EQUATION        "dg_geodesic_equation"
/** @brief 指数映射 exp_p: T_pM → M，沿测地线映射切向量到流形 */
#define PRESET_DG_EXPONENTIAL_MAP          "dg_exponential_map"
/** @brief Jacobi 场：沿测地线的变分向量场，满足 Jacobi 方程 */
#define PRESET_DG_JACOBI_FIELD             "dg_jacobi_field"
/** @brief 共轭点：沿测地线存在非平凡 Jacobi 场的点 */
#define PRESET_DG_CONJUGATE_POINTS         "dg_conjugate_points"

/* ── 张量分析 ── */

/** @brief 张量积：(T ⊗ S)^{i_1...i_p i_{p+1}...}_{j_1...j_q j_{q+1}...} */
#define PRESET_DG_TENSOR_PRODUCT           "dg_tensor_product"
/** @brief 协变导数：nabla_k T^{i_1...}_{j_1...}，保持张量性的求导运算 */
#define PRESET_DG_COVARIANT_DERIVATIVE     "dg_covariant_derivative"
/** @brief Lie 导数：L_X Y = [X, Y]，沿向量场的李导数 */
#define PRESET_DG_LIE_DERIVATIVE           "dg_lie_derivative"
/** @brief 外微分：d(omega)，微分形式的反对称化外导算子 */
#define PRESET_DG_EXTERIOR_DERIVATIVE      "dg_exterior_derivative"
/** @brief Hodge 星算子：*omega，微分形式的对偶变换 */
#define PRESET_DG_HODGE_STAR               "dg_hodge_star"

/* ============================================================
 * 模块注册函数
 * ============================================================ */

/**
 * @brief 注册所有微分几何预设函数块
 *
 * @return true 全部注册成功
 * @return false 部分注册失败
 */
bool preset_differential_geometry_register(void);

/**
 * @brief 获取微分几何预设函数块数量
 *
 * @return int 微分几何模块预设函数块总数
 */
int preset_differential_geometry_count(void);

/**
 * @brief 获取微分几何预设的类别
 *
 * @return 预设类别
 */
PresetCategory preset_differential_geometry_category(void);

/**
 * @brief 获取微分几何模块的预设名称列表
 *
 * @param out_names 输出名称数组（调用者需释放每个元素和数组本身）
 * @param out_count 输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_differential_geometry_get_names(char ***out_names, int *out_count);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_DIFFERENTIAL_GEOMETRY_H */
