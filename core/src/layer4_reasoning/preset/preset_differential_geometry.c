/**
 * @file preset_differential_geometry.c
 * @brief 微分几何预设函数块 - 实现
 *
 * 实现理论数学研究中常用的微分几何运算预设函数块。
 * 涵盖曲线论、曲面论、联络与曲率、测地线和张量分析五大领域。
 * 共25个预设函数块，均遵循模块化、确定性原则。
 *
 * 采用统一的 preset_blocks_register_simple 注册接口，
 * 使用 REGISTER_DG 宏模式简化注册代码。
 *
 * @module DifferentialGeometry
 * @category PRESET_CATEGORY_ANALYSIS
 * @version 5.0.0
 * @author Lv-00 开发团队
 */

#include "preset_differential_geometry.h"
#include "preset_blocks.h"
#include "preset_common.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

#include <string.h>

/* ==================== 预设函数块数量 ==================== */

/** 微分几何模块预设函数块总数（与头文件中 DIFFERENTIAL_GEOMETRY_PRESET_COUNT 一致） */
#define DG_PRESET_COUNT DIFFERENTIAL_GEOMETRY_PRESET_COUNT

/* ==================== REGISTER_DG 宏定义 ==================== */

/**
 * @brief 注册单个微分几何预设的便捷宏
 *
 * 封装 preset_blocks_register_simple 调用，简化注册代码。
 * 所有微分几何预设使用 PRESET_CATEGORY_ANALYSIS 类别。
 *
 * @param preset_name   预设名称常量（头文件中定义的宏）
 * @param desc          中文描述
 * @param inputs        输入类型数组（PresetType 复合字面量）
 * @param n_inputs      输入数量
 * @param output        输出类型
 * @param math          数学定义（LaTeX 格式）
 * @param comp          时间复杂度
 * @param constructive  是否构造性
 * @param reversible    是否可逆
 */
#define REGISTER_DG(preset_name, desc, n_inputs, output, math, comp, constructive, reversible, ...) \
    do { \
        PresetType _in[] = { __VA_ARGS__ }; \
        if (register_dg_preset(preset_name, desc, _in, n_inputs, output, math, comp, constructive, reversible)) { \
            success_count++; \
        } \
    } while (0)
/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个微分几何预设
 *
 * @param name 预设名称
 * @param description 中文描述
 * @param input_types 输入类型数组
 * @param input_count 输入数量
 * @param output_type 输出类型
 * @param math_def 数学定义（LaTeX格式）
 * @param complexity 时间复杂度
 * @param is_constructive 是否构造性
 * @param is_reversible 是否可逆
 * @return true 注册成功
 * @return false 注册失败
 */
static bool register_dg_preset(
    const char *name,
    const char *description,
    const PresetType *input_types,
    int input_count,
    PresetType output_type,
    const char *math_def,
    const char *complexity,
    bool is_constructive,
    bool is_reversible)
{
    return preset_blocks_register_simple(
        name, description,
        PRESET_CATEGORY_ANALYSIS,
        input_types, input_count, output_type,
        math_def, complexity,
        is_constructive, is_reversible);
}

/* ==================== 模块注册实现 ==================== */

int preset_differential_geometry_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 第一部分：曲线论（5个）
     * ============================================================ */

    /**
     * @brief 弧长参数化
     *
     * @details 将参数曲线 gamma(t) 转化为以弧长为参数的表示 gamma(s)，
     *          使得切向量的模长恒为1。弧长参数化是曲线论的基本工具，
     *          简化了Frenet标架和曲率的计算。
     * @param 输入的参数曲线（PRESET_TYPE_PATH）
     * @return 弧长参数化的曲线（PRESET_TYPE_PATH）
     * @math s = \int_{t_0}^t |\gamma'(\tau)|\,d\tau, \quad |\gamma'(s)| = 1
     * @complexity O(n)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_DG(PRESET_DG_ARC_LENGTH_PARAM,
        "弧长参数化：将参数曲线 gamma(t) 转化为弧长参数化 gamma(s)，满足 |gamma'(s)| = 1",
        1, PRESET_TYPE_PATH,
        "s = \\int_{t_0}^t |\\gamma'(\\tau)|\\,d\\tau, \\quad |\\gamma'(s)| = 1",
        "O(n)", true, false,
        PRESET_TYPE_PATH);

    /**
     * @brief Frenet标架
     *
     * @details 计算空间曲线在给定参数处的Frenet标架(T, N, B)。
     *          T是单位切向量，N是主法向量，B是副法向量。
     *          三者构成右手正交标架，是研究空间曲线局部几何的基本工具。
     * @param 曲线（PRESET_TYPE_PATH）和参数点（PRESET_TYPE_SCALAR）
     * @return 标架向量组（PRESET_TYPE_TUPLE）
     * @math T = \frac{\gamma'}{|\gamma'|}, \quad N = \frac{T'}{|T'|}, \quad B = T \times N
     * @complexity O(1)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_DG(PRESET_DG_FRENET_FRAME,
        "Frenet标架：计算空间曲线在给定参数处的Frenet标架(T, N, B)，构成右手正交标架",
        2, PRESET_TYPE_TUPLE,
        "T = \\frac{\\gamma'}{|\\gamma'|}, \\quad "
        "N = \\frac{T'}{|T'|}, \\quad "
        "B = T \\times N",
        "O(1)", true, false,
        PRESET_TYPE_PATH, PRESET_TYPE_SCALAR);

    /**
     * @brief 曲率计算
     *
     * @details 计算空间曲线在给定参数处的曲率 kappa。
     *          曲率度量了曲线偏离直线的程度，是Frenet公式的核心参数。
     * @param 曲线（PRESET_TYPE_PATH）和参数点（PRESET_TYPE_SCALAR）
     * @return 曲率值（PRESET_TYPE_SCALAR）
     * @math \kappa(t) = \frac{|\gamma'(t) \times \gamma''(t)|}{|\gamma'(t)|^3}
     * @complexity O(1)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_DG(PRESET_DG_CURVATURE,
        "曲率计算：计算空间曲线在给定参数处的曲率 kappa = |gamma' × gamma''| / |gamma'|^3",
        2, PRESET_TYPE_SCALAR,
        "\\kappa(t) = \\frac{|\\gamma'(t) \\times \\gamma''(t)|}{|\\gamma'(t)|^3}",
        "O(1)", true, false,
        PRESET_TYPE_PATH, PRESET_TYPE_SCALAR);

    /**
     * @brief 挠率计算
     *
     * @details 计算空间曲线在给定参数处的挠率 tau。
     *          挠率度量了曲线偏离密切平面的程度，
     *          挠率为零当且仅当曲线是平面曲线。
     * @param 曲线（PRESET_TYPE_PATH）和参数点（PRESET_TYPE_SCALAR）
     * @return 挠率值（PRESET_TYPE_SCALAR）
     * @math \tau(t) = \frac{(\gamma' \times \gamma'') \cdot \gamma'''}{|\gamma' \times \gamma''|^2}
     * @complexity O(1)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_DG(PRESET_DG_TORSION,
        "挠率计算：计算空间曲线在给定参数处的挠率 tau = (gamma' × gamma'') · gamma''' / |gamma' × gamma''|^2",
        2, PRESET_TYPE_SCALAR,
        "\\tau(t) = \\frac{(\\gamma' \\times \\gamma'') \\cdot \\gamma'''}{|\\gamma' \\times \\gamma''|^2}",
        "O(1)", true, false,
        PRESET_TYPE_PATH, PRESET_TYPE_SCALAR);

    /**
     * @brief Bertrand曲线
     *
     * @details 判定两条空间曲线是否构成Bertrand曲线对，
     *          即两者在对应点处有共同的主法线。
     *          Bertrand曲线是曲线论中的重要特例，
     *          存在Bertrand曲线对的充要条件是曲率和挠率满足线性关系。
     * @param 两条曲线（PRESET_TYPE_PATH, PRESET_TYPE_PATH）
     * @return 是否为Bertrand曲线对（PRESET_TYPE_BOOLEAN）
     * @math \exists \lambda, \mu \in \mathbb{R}: \lambda\kappa(t) + \mu\tau(t) = 1
     * @complexity O(n)
     * @constructive 否
     * @reversible 否
     */
    REGISTER_DG(PRESET_DG_BERTRAND_CURVE,
        "Bertrand曲线：判定两条空间曲线是否构成Bertrand曲线对（具有共同主法线）",
        2, PRESET_TYPE_BOOLEAN,
        "\\exists \\lambda, \\mu \\in \\mathbb{R}: \\lambda\\kappa(t) + \\mu\\tau(t) = 1",
        "O(n)", false, false,
        PRESET_TYPE_PATH, PRESET_TYPE_PATH);

    /* ============================================================
     * 第二部分：曲面论（6个）
     * ============================================================ */

    /**
     * @brief 第一基本形式
     *
     * @details 计算曲面参数表示的第一基本形式系数(E, F, G)。
     *          第一基本形式刻画了曲面的内蕴度量性质，
     *          决定了曲面上曲线的弧长、角度和面积。
     * @param 参数曲面（PRESET_TYPE_SURFACE）
     * @return 基本形式系数三元组 (E, F, G)（PRESET_TYPE_TUPLE）
     * @math I = E\,du^2 + 2F\,du\,dv + G\,dv^2, \quad E = \mathbf{r}_u \cdot \mathbf{r}_u, \quad F = \mathbf{r}_u \cdot \mathbf{r}_v, \quad G = \mathbf{r}_v \cdot \mathbf{r}_v
     * @complexity O(n)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_DG(PRESET_DG_FIRST_FUNDAMENTAL_FORM,
        "第一基本形式：计算曲面的第一基本形式系数(E, F, G)，刻画内蕴度量",
        1, PRESET_TYPE_TUPLE,
        "I = E\\,du^2 + 2F\\,du\\,dv + G\\,dv^2, \\quad "
        "E = \\mathbf{r}_u\\cdot\\mathbf{r}_u, \\quad "
        "F = \\mathbf{r}_u\\cdot\\mathbf{r}_v, \\quad "
        "G = \\mathbf{r}_v\\cdot\\mathbf{r}_v",
        "O(n)", true, false,
        PRESET_TYPE_SURFACE);

    /**
     * @brief 第二基本形式
     *
     * @details 计算曲面参数表示的第二基本形式系数(L, M, N)。
     *          第二基本形式刻画了曲面在空间中的弯曲程度，
     *          是计算各种曲率的基础。
     * @param 参数曲面（PRESET_TYPE_SURFACE）
     * @return 基本形式系数三元组 (L, M, N)（PRESET_TYPE_TUPLE）
     * @math II = L\,du^2 + 2M\,du\,dv + N\,dv^2, \quad L = \mathbf{r}_{uu}\cdot\mathbf{n}, \quad M = \mathbf{r}_{uv}\cdot\mathbf{n}, \quad N = \mathbf{r}_{vv}\cdot\mathbf{n}
     * @complexity O(n)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_DG(PRESET_DG_SECOND_FUNDAMENTAL_FORM,
        "第二基本形式：计算曲面的第二基本形式系数(L, M, N)，刻画外蕴弯曲",
        1, PRESET_TYPE_TUPLE,
        "II = L\\,du^2 + 2M\\,du\\,dv + N\\,dv^2, \\quad "
        "L = \\mathbf{r}_{uu}\\cdot\\mathbf{n}, \\quad "
        "M = \\mathbf{r}_{uv}\\cdot\\mathbf{n}, \\quad "
        "N = \\mathbf{r}_{vv}\\cdot\\mathbf{n}",
        "O(n)", true, false,
        PRESET_TYPE_SURFACE);

    /**
     * @brief Gauss曲率
     *
     * @details 计算曲面在给定点处的Gauss曲率 K。
     *          Gauss曲率是曲面最重要的内蕴几何量，
     *          由Gauss绝妙定理，它完全由第一基本形式决定。
     * @param 参数曲面（PRESET_TYPE_SURFACE）
     * @return Gauss曲率值（PRESET_TYPE_SCALAR）
     * @math K = \frac{LN - M^2}{EG - F^2} = \kappa_1 \kappa_2
     * @complexity O(1)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_DG(PRESET_DG_GAUSS_CURVATURE,
        "Gauss曲率：计算曲面在给定点处的Gauss曲率 K = (LN - M^2)/(EG - F^2)",
        1, PRESET_TYPE_SCALAR,
        "K = \\frac{LN - M^2}{EG - F^2} = \\kappa_1 \\kappa_2",
        "O(1)", true, false,
        PRESET_TYPE_SURFACE);

    /**
     * @brief 平均曲率
     *
     * @details 计算曲面在给定点处的平均曲率 H。
     *          平均曲率是曲面外蕴几何的重要量，
     *          极小曲面定义为平均曲率处处为零的曲面。
     * @param 参数曲面（PRESET_TYPE_SURFACE）
     * @return 平均曲率值（PRESET_TYPE_SCALAR）
     * @math H = \frac{EN - 2FM + GL}{2(EG - F^2)} = \frac{\kappa_1 + \kappa_2}{2}
     * @complexity O(1)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_DG(PRESET_DG_MEAN_CURVATURE,
        "平均曲率：计算曲面在给定点处的平均曲率 H = (kappa1 + kappa2)/2",
        1, PRESET_TYPE_SCALAR,
        "H = \\frac{EN - 2FM + GL}{2(EG - F^2)} = \\frac{\\kappa_1 + \\kappa_2}{2}",
        "O(1)", true, false,
        PRESET_TYPE_SURFACE);

    /**
     * @brief 主曲率
     *
     * @details 计算曲面在给定点处的主曲率(k1, k2)。
     *          主曲率是法曲率的最大值和最小值，
     *          对应的方向称为主方向。
     * @param 参数曲面（PRESET_TYPE_SURFACE）
     * @return 主曲率对 (k1, k2)（PRESET_TYPE_TUPLE）
     * @math \kappa_{1,2} = H \pm \sqrt{H^2 - K}
     * @complexity O(1)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_DG(PRESET_DG_PRINCIPAL_CURVATURES,
        "主曲率：计算曲面在给定点处的主曲率对 (k1, k2)，即法曲率的极值",
        1, PRESET_TYPE_TUPLE,
        "\\kappa_{1,2} = H \\pm \\sqrt{H^2 - K}",
        "O(1)", true, false,
        PRESET_TYPE_SURFACE);

    /**
     * @brief Weingarten映射
     *
     * @details 计算曲面的Weingarten映射（形状算子），
     *          它是切平面到自身的线性映射 S_p: T_pM -> T_pM，
     *          定义为 S_p(X) = -∇_X N（单位法向量沿切方向的负导数）。
     * @param 参数曲面（PRESET_TYPE_SURFACE）
     * @return Weingarten映射矩阵（PRESET_TYPE_MATRIX）
     * @math S = \begin{pmatrix} E & F \\ F & G \end{pmatrix}^{-1} \begin{pmatrix} L & M \\ M & N \end{pmatrix}
     * @complexity O(1)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_DG(PRESET_DG_WEINGARTEN_MAP,
        "Weingarten映射：计算曲面的形状算子 S = I^{-1}·II，特征值为主曲率",
        1, PRESET_TYPE_MATRIX,
        "S = \\begin{pmatrix} E & F \\\\ F & G \\end{pmatrix}^{-1}"
        "\\begin{pmatrix} L & M \\\\ M & N \\end{pmatrix}",
        "O(1)", true, false,
        PRESET_TYPE_SURFACE);

    /* ============================================================
     * 第三部分：联络与曲率（5个）
     * ============================================================ */

    /**
     * @brief Levi-Civita联络
     *
     * @details 从黎曼度量张量构造唯一的无挠、与度量相容的Levi-Civita联络。
     *          联络定义了流形上向量场的协变导数，
     *          Levi-Civita联络是黎曼几何中最基本的联络。
     * @param 度量张量矩阵（PRESET_TYPE_MATRIX）
     * @return Christoffel符号（PRESET_TYPE_MATRIX）
     * @math \Gamma_{ij}^k = \frac{1}{2}g^{kl}(\partial_i g_{lj} + \partial_j g_{li} - \partial_l g_{ij})
     * @complexity O(n^3)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_DG(PRESET_DG_LEVI_CIVITA_CONNECTION,
        "Levi-Civita联络：从度量张量构造无挠且与度量相容的唯一联络",
        1, PRESET_TYPE_MATRIX,
        "\\Gamma_{ij}^k = \\frac{1}{2}g^{kl}"
        "(\\partial_i g_{lj} + \\partial_j g_{li} - \\partial_l g_{ij})",
        "O(n^3)", true, false,
        PRESET_TYPE_MATRIX);

    /**
     * @brief Riemann曲率张量
     *
     * @details 计算黎曼流形的Riemann曲率张量 R^{i}_{jkl}。
     *          Riemann曲率张量是黎曼几何的核心对象，
     *          包含了流形弯曲的全部信息。
     * @param Christoffel符号矩阵（PRESET_TYPE_MATRIX）
     * @return Riemann曲率张量（PRESET_TYPE_MATRIX）
     * @math R^{i}_{jkl} = \partial_k \Gamma^{i}_{jl} - \partial_l \Gamma^{i}_{jk} + \Gamma^{i}_{mk}\Gamma^{m}_{jl} - \Gamma^{i}_{ml}\Gamma^{m}_{jk}
     * @complexity O(n^4)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_DG(PRESET_DG_RIEMANN_CURVATURE,
        "Riemann曲率张量：从Christoffel符号计算Riemann曲率张量 R^{i}_{jkl}",
        1, PRESET_TYPE_MATRIX,
        "R^{i}_{jkl} = \\partial_k \\Gamma^{i}_{jl} - "
        "\\partial_l \\Gamma^{i}_{jk} + "
        "\\Gamma^{i}_{mk}\\Gamma^{m}_{jl} - "
        "\\Gamma^{i}_{ml}\\Gamma^{m}_{jk}",
        "O(n^4)", true, false,
        PRESET_TYPE_MATRIX);

    /**
     * @brief Ricci曲率
     *
     * @details 由Riemann曲率张量缩并得到Ricci曲率张量 R_{ij}。
     *          Ricci曲率是Riemann张量的迹，
     *          在广义相对论中直接出现在Einstein场方程中。
     * @param Riemann曲率张量（PRESET_TYPE_MATRIX）
     * @return Ricci曲率张量（PRESET_TYPE_MATRIX）
     * @math R_{ij} = R^{k}_{ikj}
     * @complexity O(n^3)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_DG(PRESET_DG_RICCI_CURVATURE,
        "Ricci曲率：由Riemann曲率张量缩并得到Ricci曲率张量 R_{ij} = R^{k}_{ikj}",
        1, PRESET_TYPE_MATRIX,
        "R_{ij} = R^{k}_{ikj}",
        "O(n^3)", true, false,
        PRESET_TYPE_MATRIX);

    /**
     * @brief 截面曲率
     *
     * @details 计算流形在给定二维截面方向的截面曲率。
     *          截面曲率是Riemann曲率在二维子空间上的归一化投影，
     *          当截面曲率恒为常数时称为常曲率空间。
     * @param Riemann曲率张量（PRESET_TYPE_MATRIX）和二维截面（PRESET_TYPE_VECTOR, PRESET_TYPE_VECTOR）
     * @return 截面曲率值（PRESET_TYPE_SCALAR）
     * @math K(X, Y) = \frac{R(X,Y,Y,X)}{|X|^2|Y|^2 - \langle X,Y \rangle^2}
     * @complexity O(n^3)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_DG(PRESET_DG_SECTIONAL_CURVATURE,
        "截面曲率：计算流形在给定二维截面方向(X, Y)的截面曲率 K(X,Y)",
        3, PRESET_TYPE_SCALAR,
        "K(X, Y) = \\frac{R(X,Y,Y,X)}{|X|^2|Y|^2 - \\langle X,Y \\rangle^2}",
        "O(n^3)", true, false,
        PRESET_TYPE_MATRIX, PRESET_TYPE_VECTOR, PRESET_TYPE_VECTOR);

    /**
     * @brief 标量曲率
     *
     * @details 计算流形的标量曲率 R = g^{ij} R_{ij}（Ricci曲率的迹）。
     *          标量曲率是曲率的最简单标量不变量，
     *          在二维情况下等于Gauss曲率的两倍。
     * @param Ricci曲率张量和度量张量（PRESET_TYPE_MATRIX, PRESET_TYPE_MATRIX）
     * @return 标量曲率值（PRESET_TYPE_SCALAR）
     * @math R = g^{ij} R_{ij}
     * @complexity O(n^2)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_DG(PRESET_DG_SCALAR_CURVATURE,
        "标量曲率：计算标量曲率 R = g^{ij} R_{ij}（Ricci曲率的迹）",
        2, PRESET_TYPE_SCALAR,
        "R = g^{ij} R_{ij}",
        "O(n^2)", true, false,
        PRESET_TYPE_MATRIX, PRESET_TYPE_MATRIX);

    /* ============================================================
     * 第四部分：测地线（4个）
     * ============================================================ */

    /**
     * @brief 测地线方程
     *
     * @details 建立并求解曲面上给定初始点和初始方向的测地线微分方程。
     *          测地线是曲面上"最直"的曲线（测地曲率为零），
     *          局部上也是两点之间的最短路径。
     * @param 曲面（PRESET_TYPE_SURFACE）、初始点（PRESET_TYPE_POINT）、初始方向（PRESET_TYPE_VECTOR）
     * @return 测地线方程（PRESET_TYPE_EQUATION）
     * @math \frac{d^2 u^k}{dt^2} + \Gamma_{ij}^k \frac{du^i}{dt}\frac{du^j}{dt} = 0
     * @complexity O(n^2)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_DG(PRESET_DG_GEODESIC_EQUATION,
        "测地线方程：建立并求解曲面上给定初值条件的测地线微分方程",
        3, PRESET_TYPE_PATH,
        "\\frac{d^2 u^k}{dt^2} + "
        "\\Gamma_{ij}^k \\frac{du^i}{dt}\\frac{du^j}{dt} = 0",
        "O(n^2)", true, false,
        PRESET_TYPE_SURFACE, PRESET_TYPE_POINT, PRESET_TYPE_VECTOR);

    /**
     * @brief 指数映射
     *
     * @details 计算切向量在指数映射下的像。
     *          指数映射 exp_p: T_pM -> M 将切向量映射为
     *          沿该方向出发的测地线在时刻1到达的点。
     *          指数映射是黎曼几何中局部微分同胚的核心工具。
     * @param 点（PRESET_TYPE_POINT）和切向量（PRESET_TYPE_VECTOR）
     * @return 测地线终点（PRESET_TYPE_POINT）
     * @math \exp_p(v) = \gamma_v(1), \quad \gamma_v(0) = p, \quad \gamma_v'(0) = v
     * @complexity O(n^2)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_DG(PRESET_DG_EXPONENTIAL_MAP,
        "指数映射：计算切向量 v 在指数映射 exp_p(v) 下的像点",
        2, PRESET_TYPE_POINT,
        "\\exp_p(v) = \\gamma_v(1), \\quad "
        "\\gamma_v(0) = p, \\quad \\gamma_v'(0) = v",
        "O(n^2)", true, false,
        PRESET_TYPE_POINT, PRESET_TYPE_VECTOR);

    /**
     * @brief Jacobi场
     *
     * @details 计算沿测地线的Jacobi向量场。
     *          Jacobi场描述了邻近测地线的偏离行为，
     *          满足Jacobi方程，是研究共轭点和曲率的重要工具。
     * @param 测地线（PRESET_TYPE_PATH）和初始扰动（PRESET_TYPE_VECTOR）
     * @return Jacobi场沿测地线的值（PRESET_TYPE_PATH）
     * @math \frac{D^2 J}{dt^2} + R(J, \dot{\gamma})\dot{\gamma} = 0
     * @complexity O(n^2)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_DG(PRESET_DG_JACOBI_FIELD,
        "Jacobi场：计算沿测地线的Jacobi向量场，满足 Jacobi 方程 D^2J/dt^2 + R(J,dot(gamma))dot(gamma) = 0",
        2, PRESET_TYPE_VECTOR,
        "\\frac{D^2 J}{dt^2} + R(J, \\dot{\\gamma})\\dot{\\gamma} = 0",
        "O(n^2)", true, false,
        PRESET_TYPE_PATH, PRESET_TYPE_VECTOR);

    /**
     * @brief 共轭点
     *
     * @details 计算沿测地线的共轭点位置。
     *          共轭点是沿测地线Jacobi场消失的点，
     *          在共轭点之后测地线不再是最短的。
     * @param 测地线（PRESET_TYPE_PATH）
     * @return 共轭点参数值（PRESET_TYPE_SCALAR）
     * @math \exists J \neq 0: J(0) = 0, \quad J(t_0) = 0 \Rightarrow t_0 \text{ 是共轭点}
     * @complexity O(n^2)
     * @constructive 否
     * @reversible 否
     */
    REGISTER_DG(PRESET_DG_CONJUGATE_POINTS,
        "共轭点：计算沿测地线的共轭点位置（使Jacobi场首次消失的参数值）",
        1, PRESET_TYPE_SCALAR,
        "\\exists J \\neq 0: J(0) = 0, \\; J(t_0) = 0",
        "O(n^2)", false, false,
        PRESET_TYPE_PATH);

    /* ============================================================
     * 第五部分：张量分析（5个）
     * ============================================================ */

    /**
     * @brief 张量积
     *
     * @details 计算两个张量的张量积（Kronecker积推广）。
     *          张量积是构造高阶张量的基本运算，
     *          在微分几何中用于构造各种张量场。
     * @param 两个张量（PRESET_TYPE_MATRIX, PRESET_TYPE_MATRIX）
     * @return 张量积（PRESET_TYPE_MATRIX）
     * @math (T \otimes S)^{i_1\ldots i_p j_1\ldots j_q}_{k_1\ldots k_r l_1\ldots l_s} = T^{i_1\ldots i_p}_{k_1\ldots k_r} S^{j_1\ldots j_q}_{l_1\ldots l_s}
     * @complexity O(n^4)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_DG(PRESET_DG_TENSOR_PRODUCT,
        "张量积：计算两个张量的张量积 T ⊗ S，构造高阶张量",
        2, PRESET_TYPE_MATRIX,
        "(T \\otimes S)^{i_1\\ldots i_p j_1\\ldots j_q}_{k_1\\ldots k_r l_1\\ldots l_s} = "
        "T^{i_1\\ldots i_p}_{k_1\\ldots k_r} S^{j_1\\ldots j_q}_{l_1\\ldots l_s}",
        "O(n^4)", true, false,
        PRESET_TYPE_MATRIX, PRESET_TYPE_MATRIX);

    /**
     * @brief 协变导数
     *
     * @details 计算张量场的协变导数。
     *          协变导数是欧氏空间普通导数在流形上的推广，
     *          通过Christoffel符号对分量进行修正。
     * @param 张量场（PRESET_TYPE_MATRIX）和联络（PRESET_TYPE_MATRIX）
     * @return 协变导数（PRESET_TYPE_MATRIX）
     * @math \nabla_k T^{i_1\ldots i_p}_{j_1\ldots j_q} = \partial_k T^{i_1\ldots i_p}_{j_1\ldots j_q} + \sum \Gamma^{i_\alpha}_{kl}T^{\ldots l\ldots} - \sum \Gamma^{l}_{k j_\beta}T^{\ldots}_{l\ldots}
     * @complexity O(n^3)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_DG(PRESET_DG_COVARIANT_DERIVATIVE,
        "协变导数：计算张量场的协变导数 ∇_k T，联络修正偏导数",
        2, PRESET_TYPE_MATRIX,
        "\\nabla_k T^{i}_{j} = \\partial_k T^{i}_{j} + \\Gamma^{i}_{kl}T^{l}_{j} - \\Gamma^{l}_{kj}T^{i}_{l}",
        "O(n^3)", true, false,
        PRESET_TYPE_MATRIX, PRESET_TYPE_MATRIX);

    /**
     * @brief Lie导数
     *
     * @details 计算张量场沿向量场的Lie导数。
     *          Lie导数度量张量场沿向量场流的无穷小变化率，
     *          不依赖于联络，是微分拓扑中的基本工具。
     * @param 张量场（PRESET_TYPE_MATRIX）和向量场（PRESET_TYPE_VECTOR）
     * @return Lie导数（PRESET_TYPE_MATRIX）
     * @math \mathcal{L}_X T = \lim_{t\to 0}\frac{\phi_t^* T - T}{t}
     * @complexity O(n^2)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_DG(PRESET_DG_LIE_DERIVATIVE,
        "Lie导数：计算张量场沿向量场X的Lie导数 L_X T，度量无穷小变化率",
        2, PRESET_TYPE_MATRIX,
        "\\mathcal{L}_X T = \\lim_{t\\to 0}\\frac{\\phi_t^* T - T}{t}",
        "O(n^2)", true, false,
        PRESET_TYPE_MATRIX, PRESET_TYPE_VECTOR);

    /**
     * @brief 外微分
     *
     * @details 计算微分形式的外微分。
     *          外微分是de Rham上同调的核心运算，
     *          满足 d^2 = 0，将k-形式映射为(k+1)-形式。
     * @param 微分形式（PRESET_TYPE_MATRIX）
     * @return 外微分（PRESET_TYPE_MATRIX）
     * @math d\omega = \sum \frac{\partial \omega_{i_1\ldots i_k}}{\partial x^j} dx^j \wedge dx^{i_1} \wedge \cdots \wedge dx^{i_k}
     * @complexity O(n^2)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_DG(PRESET_DG_EXTERIOR_DERIVATIVE,
        "外微分：计算微分形式的外微分 dω，满足 d² = 0",
        1, PRESET_TYPE_MATRIX,
        "d\\omega = \\sum \\frac{\\partial \\omega_{i_1\\ldots i_k}}{\\partial x^j} "
        "dx^j \\wedge dx^{i_1} \\wedge \\cdots \\wedge dx^{i_k}",
        "O(n^2)", true, false,
        PRESET_TYPE_MATRIX);

    /**
     * @brief Hodge星算子
     *
     * @details 计算微分形式的Hodge对偶。
     *          Hodge星算子是n维定向黎曼流形上的线性映射
     *          *: Λ^k -> Λ^{n-k}，用于定义余微分和Laplace-de Rham算子。
     * @param 微分形式（PRESET_TYPE_MATRIX）
     * @return Hodge对偶形式（PRESET_TYPE_MATRIX）
     * @math *\omega = \frac{\sqrt{|\det g|}}{k!(n-k)!} \varepsilon_{i_1\ldots i_n} \omega^{i_1\ldots i_k} dx^{i_{k+1}} \wedge \cdots \wedge dx^{i_n}
     * @complexity O(n!)
     * @constructive 是
     * @reversible 是
     */
    REGISTER_DG(PRESET_DG_HODGE_STAR,
        "Hodge星算子：计算微分形式的Hodge对偶 *ω，将k-形式映射为(n-k)-形式",
        1, PRESET_TYPE_MATRIX,
        "*\\omega = \\frac{\\sqrt{|\\det g|}}{k!(n-k)!} "
        "\\varepsilon_{i_1\\ldots i_n} "
        "\\omega^{i_1\\ldots i_k} dx^{i_{k+1}} \\wedge \\cdots \\wedge dx^{i_n}",
        "O(n!)", true, true,
        PRESET_TYPE_MATRIX);

    /* 返回是否所有预设都注册成功 */
    if (success_count == DG_PRESET_COUNT) {
        /* lv00_log_info("微分几何模块注册成功：%d/%d 个预设", success_count, DG_PRESET_COUNT) */
        return true;
    }

    /* lv00_log_info("微分几何模块注册部分失败：%d/%d 个预设", success_count, DG_PRESET_COUNT) */
    return false;
}

/**
 * @brief 获取微分几何预设函数块数量
 *
 * @return int 微分几何模块预设函数块总数（25）
 */
int preset_differential_geometry_count(void)
{
    return DG_PRESET_COUNT;
}

/**
 * @brief 获取微分几何预设的类别
 *
 * @return PresetCategory 预设类别（PRESET_CATEGORY_ANALYSIS）
 */
PresetCategory preset_differential_geometry_category(void)
{
    return PRESET_CATEGORY_ANALYSIS;
}

/**
 * @brief 获取微分几何预设名称列表
 *
 * @param out_names 输出名称数组（调用者需释放每个元素和数组本身）
 * @param out_count 输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_differential_geometry_get_names(char ***out_names, int *out_count)
{
    if (!out_names || !out_count) return false;

    char **names = (char**)lv00_malloc(DG_PRESET_COUNT * sizeof(char*));
    if (!names) return false;

    const char *preset_names[] = {
        /* 曲线论 */
        PRESET_DG_ARC_LENGTH_PARAM,
        PRESET_DG_FRENET_FRAME,
        PRESET_DG_CURVATURE,
        PRESET_DG_TORSION,
        PRESET_DG_BERTRAND_CURVE,
        /* 曲面论 */
        PRESET_DG_FIRST_FUNDAMENTAL_FORM,
        PRESET_DG_SECOND_FUNDAMENTAL_FORM,
        PRESET_DG_GAUSS_CURVATURE,
        PRESET_DG_MEAN_CURVATURE,
        PRESET_DG_PRINCIPAL_CURVATURES,
        PRESET_DG_WEINGARTEN_MAP,
        /* 联络与曲率 */
        PRESET_DG_LEVI_CIVITA_CONNECTION,
        PRESET_DG_RIEMANN_CURVATURE,
        PRESET_DG_RICCI_CURVATURE,
        PRESET_DG_SECTIONAL_CURVATURE,
        PRESET_DG_SCALAR_CURVATURE,
        /* 测地线 */
        PRESET_DG_GEODESIC_EQUATION,
        PRESET_DG_EXPONENTIAL_MAP,
        PRESET_DG_JACOBI_FIELD,
        PRESET_DG_CONJUGATE_POINTS,
        /* 张量分析 */
        PRESET_DG_TENSOR_PRODUCT,
        PRESET_DG_COVARIANT_DERIVATIVE,
        PRESET_DG_LIE_DERIVATIVE,
        PRESET_DG_EXTERIOR_DERIVATIVE,
        PRESET_DG_HODGE_STAR,
    };

    int count = (int)(sizeof(preset_names) / sizeof(preset_names[0]));

    for (int i = 0; i < count; i++) {
        names[i] = lv00_strdup(preset_names[i]);
        if (names[i] == NULL) {
            for (int j = 0; j < i; j++) { void *tmp = names[j]; lv00_free(&tmp); }
            { void *tmp = names; lv00_free(&tmp); }
            return false;
        }
    }

    *out_names = names;
    *out_count = count;
    return true;
}