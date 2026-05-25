/**
 * @file preset_polygons.h
 * @brief 多边形构造预设函数块
 *
 * 提供完整的多边形构造功能，包括正多边形、特殊多边形和多边形操作。
 * 支持理论数学研究中的多边形几何分析。
 *
 * 包含的预设函数块：
 * - 正多边形构造
 * - 三角形特殊构造
 * - 四边形构造
 * - 多边形分割与合并
 * - 多边形性质计算
 *
 * @module Polygons
 * @category PRESET_CATEGORY_CONSTRUCTION
 */

#ifndef LV00_PRESET_POLYGONS_H
#define LV00_PRESET_POLYGONS_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 正多边形 ==================== */

/**
 * @brief 构造正三角形
 *
 * 数学定义：给定边 $\overline{AB}$，构造正三角形的第三个顶点
 *
 * 输入：
 *   - A: 点 (PRESET_TYPE_POINT)
 *   - B: 点 (PRESET_TYPE_POINT)
 * 输出：
 *   - 第三个顶点 (PRESET_TYPE_POINT)
 *
 * 复杂度：O(1)
 * 注意：产生两个解（两侧各一个），使用选择器指定
 */
#define PRESET_EQUILATERAL_TRIANGLE "equilateral_triangle"

/**
 * @brief 构造正方形
 *
 * 数学定义：给定边 $\overline{AB}$，构造正方形的另外两个顶点
 *
 * 输入：
 *   - A: 点 (PRESET_TYPE_POINT)
 *   - B: 点 (PRESET_TYPE_POINT)
 * 输出：
 *   - C: 点 (PRESET_TYPE_POINT) - 第三个顶点
 *   - D: 点 (PRESET_TYPE_POINT) - 第四个顶点
 *
 * 复杂度：O(1)
 * 注意：产生两个解（两侧各一个）
 */
#define PRESET_SQUARE "square"

/**
 * @brief 构造正n边形
 *
 * 数学定义：给定边 $\overline{AB}$ 和边数 $n$，构造正 $n$ 边形
 *
 * 输入：
 *   - A: 点 (PRESET_TYPE_POINT)
 *   - B: 点 (PRESET_TYPE_POINT)
 *   - n: 整数 (PRESET_TYPE_INTEGER) - 边数（n >= 3）
 * 输出：
 *   - 顶点数组 (PRESET_TYPE_POINT, 可变输出)
 *
 * 复杂度：O(n)
 */
#define PRESET_REGULAR_POLYGON "regular_polygon"

/**
 * @brief 构造正五边形
 *
 * 数学定义：给定边 $\overline{AB}$，构造正五边形
 *
 * 输入：
 *   - A: 点 (PRESET_TYPE_POINT)
 *   - B: 点 (PRESET_TYPE_POINT)
 * 输出：
 *   - 顶点数组 (PRESET_TYPE_POINT, 5个点)
 *
 * 复杂度：O(1)
 */
#define PRESET_REGULAR_PENTAGON "regular_pentagon"

/**
 * @brief 构造正六边形
 *
 * 数学定义：给定边 $\overline{AB}$，构造正六边形
 *
 * 输入：
 *   - A: 点 (PRESET_TYPE_POINT)
 *   - B: 点 (PRESET_TYPE_POINT)
 * 输出：
 *   - 顶点数组 (PRESET_TYPE_POINT, 6个点)
 *
 * 复杂度：O(1)
 */
#define PRESET_REGULAR_HEXAGON "regular_hexagon"

/* ==================== 三角形特殊构造 ==================== */

/**
 * @brief 构造等腰三角形
 *
 * 数学定义：给定底边 $\overline{AB}$ 和顶点 $C$，构造等腰三角形
 * 要求 $|CA| = |CB|$
 *
 * 输入：
 *   - A: 点 (PRESET_TYPE_POINT) - 底边端点
 *   - B: 点 (PRESET_TYPE_POINT) - 底边端点
 *   - height: 标量 (PRESET_TYPE_SCALAR) - 高（带符号）
 * 输出：
 *   - 顶点 (PRESET_TYPE_POINT)
 *
 * 复杂度：O(1)
 */
#define PRESET_ISOSCELES_TRIANGLE "isosceles_triangle"

/**
 * @brief 构造直角三角形
 *
 * 数学定义：给定直角顶点 $C$ 和两直角边上的点 $A, B$
 *
 * 输入：
 *   - C: 点 (PRESET_TYPE_POINT) - 直角顶点
 *   - A: 点 (PRESET_TYPE_POINT) - 一条直角边上的点
 *   - leg_length: 标量 (PRESET_TYPE_SCALAR) - 另一直角边长度
 * 输出：
 *   - B: 点 (PRESET_TYPE_POINT) - 另一直角边上的点
 *
 * 复杂度：O(1)
 * 注意：产生两个解（两侧各一个）
 */
#define PRESET_RIGHT_TRIANGLE "right_triangle"

/**
 * @brief 构造给定三边长的三角形（SSS）
 *
 * 数学定义：给定边长 $a, b, c$，构造三角形
 *
 * 输入：
 *   - A: 点 (PRESET_TYPE_POINT) - 第一个顶点
 *   - B: 点 (PRESET_TYPE_POINT) - 第二个顶点（确定边c）
 *   - a: 标量 (PRESET_TYPE_SCALAR) - 对边A的边长
 *   - b: 标量 (PRESET_TYPE_SCALAR) - 对边B的边长
 * 输出：
 *   - 第三个顶点 (PRESET_TYPE_POINT)
 *
 * 复杂度：O(1)
 * 注意：产生两个解（两侧各一个）
 */
#define PRESET_TRIANGLE_SSS "triangle_sss"

/**
 * @brief 构造给定两边及夹角的三角形（SAS）
 *
 * 数学定义：给定两边 $b, c$ 和夹角 $A$，构造三角形
 *
 * 输入：
 *   - A: 点 (PRESET_TYPE_POINT) - 顶点
 *   - B: 点 (PRESET_TYPE_POINT) - 确定边c
 *   - b: 标量 (PRESET_TYPE_SCALAR) - 边b长度
 *   - angle: 标量 (PRESET_TYPE_SCALAR) - 夹角（弧度）
 * 输出：
 *   - 第三个顶点 (PRESET_TYPE_POINT)
 *
 * 复杂度：O(1)
 * 注意：产生两个解
 */
#define PRESET_TRIANGLE_SAS "triangle_sas"

/**
 * @brief 构造给定两角及夹边的三角形（ASA）
 *
 * 数学定义：给定两角 $A, B$ 和夹边 $c$，构造三角形
 *
 * 输入：
 *   - A: 点 (PRESET_TYPE_POINT)
 *   - B: 点 (PRESET_TYPE_POINT) - 确定边c
 *   - angle_A: 标量 (PRESET_TYPE_SCALAR) - 角A
 *   - angle_B: 标量 (PRESET_TYPE_SCALAR) - 角B
 * 输出：
 *   - 第三个顶点 (PRESET_TYPE_POINT)
 *
 * 复杂度：O(1)
 */
#define PRESET_TRIANGLE_ASA "triangle_asa"

/* ==================== 四边形构造 ==================== */

/**
 * @brief 构造矩形
 *
 * 数学定义：给定两邻边 $\overline{AB}$ 和 $\overline{AD}$，构造矩形
 *
 * 输入：
 *   - A: 点 (PRESET_TYPE_POINT)
 *   - B: 点 (PRESET_TYPE_POINT)
 *   - D: 点 (PRESET_TYPE_POINT)
 * 输出：
 *   - C: 点 (PRESET_TYPE_POINT) - 第四个顶点
 *
 * 复杂度：O(1)
 */
#define PRESET_RECTANGLE "rectangle"

/**
 * @brief 构造平行四边形
 *
 * 数学定义：给定两邻边 $\overline{AB}$ 和 $\overline{AD}$，构造平行四边形
 *
 * 输入：
 *   - A: 点 (PRESET_TYPE_POINT)
 *   - B: 点 (PRESET_TYPE_POINT)
 *   - D: 点 (PRESET_TYPE_POINT)
 * 输出：
 *   - C: 点 (PRESET_TYPE_POINT) - 第四个顶点
 *
 * 复杂度：O(1)
 */
#define PRESET_PARALLELOGRAM "parallelogram"

/**
 * @brief 构造菱形
 *
 * 数学定义：给定对角线 $\overline{AC}$ 和 $\overline{BD}$，构造菱形
 *
 * 输入：
 *   - A: 点 (PRESET_TYPE_POINT) - 对角线1端点
 *   - C: 点 (PRESET_TYPE_POINT) - 对角线1端点
 *   - B: 点 (PRESET_TYPE_POINT) - 对角线2端点
 * 输出：
 *   - D: 点 (PRESET_TYPE_POINT) - 第四个顶点
 *
 * 复杂度：O(1)
 */
#define PRESET_RHOMBUS "rhombus"

/**
 * @brief 构造梯形
 *
 * 数学定义：给定底边 $\overline{AB}$、顶点 $D$ 和底角，构造梯形
 *
 * 输入：
 *   - A: 点 (PRESET_TYPE_POINT)
 *   - B: 点 (PRESET_TYPE_POINT) - 底边
 *   - D: 点 (PRESET_TYPE_POINT) - 上底顶点
 *   - angle: 标量 (PRESET_TYPE_SCALAR) - 底角
 * 输出：
 *   - C: 点 (PRESET_TYPE_POINT) - 第四个顶点
 *
 * 复杂度：O(1)
 */
#define PRESET_TRAPEZOID "trapezoid"

/**
 * @brief 构造筝形（风筝形）
 *
 * 数学定义：给定对角线 $\overline{AC}$ 和顶点 $B$，构造筝形
 *
 * 输入：
 *   - A: 点 (PRESET_TYPE_POINT)
 *   - C: 点 (PRESET_TYPE_POINT) - 对称轴对角线
 *   - B: 点 (PRESET_TYPE_POINT)
 * 输出：
 *   - D: 点 (PRESET_TYPE_POINT) - 第四个顶点
 *
 * 复杂度：O(1)
 */
#define PRESET_KITE "kite"

/* ==================== 多边形操作 ==================== */

/**
 * @brief 多边形三角剖分
 *
 * 数学定义：将多边形分割为三角形
 *
 * 输入：
 *   - vertices: 点数组 (PRESET_TYPE_POINT, 可变参数) - 多边形顶点
 * 输出：
 *   - 三角形数组 (PRESET_TYPE_POLYGON, 可变输出)
 *
 * 复杂度：O(n)
 */
#define PRESET_POLYGON_TRIANGULATION "polygon_triangulation"

/**
 * @brief 构造凸包
 *
 * 数学定义：给定点集，构造其凸包
 *
 * 输入：
 *   - points: 点数组 (PRESET_TYPE_POINT, 可变参数)
 * 输出：
 *   - 凸包顶点 (PRESET_TYPE_POINT, 可变输出)
 *
 * 复杂度：O(n log n)
 */
#define PRESET_CONVEX_HULL "convex_hull"

/**
 * @brief 多边形偏移（向外/向内）
 *
 * 数学定义：构造与原多边形各边平行且距离为 $d$ 的多边形
 *
 * 输入：
 *   - vertices: 点数组 (PRESET_TYPE_POINT, 可变参数)
 *   - offset: 标量 (PRESET_TYPE_SCALAR) - 偏移距离（正为外扩，负为内缩）
 * 输出：
 *   - 偏移后的顶点 (PRESET_TYPE_POINT, 可变输出)
 *
 * 复杂度：O(n)
 */
#define PRESET_POLYGON_OFFSET "polygon_offset"

/**
 * @brief 多边形布尔运算（并、交、差）
 *
 * 数学定义：两个多边形的布尔运算
 *
 * 输入：
 *   - poly1: 点数组 (PRESET_TYPE_POINT, 可变参数)
 *   - poly2: 点数组 (PRESET_TYPE_POINT, 可变参数)
 *   - operation: 整数 (PRESET_TYPE_INTEGER) - 0:并, 1:交, 2:差
 * 输出：
 *   - 结果多边形 (PRESET_TYPE_POINT, 可变输出)
 *
 * 复杂度：O(n*m)
 */
#define PRESET_POLYGON_BOOLEAN "polygon_boolean"

/* ==================== 特殊多边形 ==================== */

/**
 * @brief 构造星形多边形
 *
 * 数学定义：构造 $\{n/k\}$ 星形多边形
 *
 * 输入：
 *   - center: 点 (PRESET_TYPE_POINT) - 中心
 *   - radius: 标量 (PRESET_TYPE_SCALAR) - 外接圆半径
 *   - n: 整数 (PRESET_TYPE_INTEGER) - 顶点数
 *   - k: 整数 (PRESET_TYPE_INTEGER) - 密度参数
 * 输出：
 *   - 顶点数组 (PRESET_TYPE_POINT, 可变输出)
 *
 * 复杂度：O(n)
 */
#define PRESET_STAR_POLYGON "star_polygon"

/**
 * @brief 构造正星形五角星
 *
 * 数学定义：构造正五角星
 *
 * 输入：
 *   - A: 点 (PRESET_TYPE_POINT)
 *   - B: 点 (PRESET_TYPE_POINT) - 确定外接圆
 * 输出：
 *   - 顶点数组 (PRESET_TYPE_POINT, 5个点)
 *
 * 复杂度：O(1)
 */
#define PRESET_PENTAGRAM "pentagram"

/* ==================== 模块初始化 ==================== */

/**
 * @brief 注册多边形构造预设函数块
 *
 * 此函数由 preset_blocks_init() 自动调用。
 *
 * @return true 注册成功
 * @return false 注册失败
 */
bool preset_polygons_register(void);

/**
 * @brief 获取多边形模块的预设数量
 *
 * @return 预设函数块数量
 */
int preset_polygons_count(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_POLYGONS_H */
