/**
 * @file preset_transformations.h
 * @brief 几何变换预设函数块
 *
 * 提供完整的几何变换操作，包括等距变换、相似变换和仿射变换。
 * 这些变换是几何学中的核心操作，支持群论结构的验证。
 *
 * 包含的预设函数块：
 * - 平移变换 (Translation)
 * - 旋转变换 (Rotation)
 * - 反射变换 (Reflection)
 * - 位似变换/缩放 (Homothety/Scaling)
 * - 仿射变换 (Affine Transformation)
 * - 变换的组合与逆
 *
 * @module Transformations
 * @category PRESET_CATEGORY_TRANSFORMATION
 */

#ifndef LV00_PRESET_TRANSFORMATIONS_H
#define LV00_PRESET_TRANSFORMATIONS_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 平移变换 ==================== */

/**
 * @brief 平移变换
 *
 * 数学定义：给定向量 $\vec{v}$，平移变换 $T_{\vec{v}}(P) = P + \vec{v}$
 *
 * 输入：
 *   - P: 点 (PRESET_TYPE_POINT) - 待平移的点
 *   - v_start: 点 (PRESET_TYPE_POINT) - 向量起点
 *   - v_end: 点 (PRESET_TYPE_POINT) - 向量终点
 * 输出：
 *   - 平移后的点 (PRESET_TYPE_POINT)
 *
 * 复杂度：O(1)
 * 群性质：平移群（阿贝尔群）
 */
#define PRESET_TRANSLATION "translation"

/**
 * @brief 批量平移
 *
 * 数学定义：对多个点应用相同的平移变换
 *
 * 输入：
 *   - points: 点数组 (PRESET_TYPE_POINT, 可变参数)
 *   - v_start: 点 (PRESET_TYPE_POINT) - 向量起点
 *   - v_end: 点 (PRESET_TYPE_POINT) - 向量终点
 * 输出：
 *   - 平移后的点数组 (PRESET_TYPE_POINT, 可变输出)
 *
 * 复杂度：O(n)，n为点的数量
 */
#define PRESET_TRANSLATION_BATCH "translation_batch"

/* ==================== 旋转变换 ==================== */

/**
 * @brief 绕点旋转
 *
 * 数学定义：给定中心 $O$ 和角度 $\theta$，旋转 $R_{O,\theta}(P)$
 *
 * 输入：
 *   - P: 点 (PRESET_TYPE_POINT) - 待旋转的点
 *   - O: 点 (PRESET_TYPE_POINT) - 旋转中心
 *   - angle: 标量 (PRESET_TYPE_SCALAR) - 旋转角度（弧度）
 * 输出：
 *   - 旋转后的点 (PRESET_TYPE_POINT)
 *
 * 复杂度：O(1)
 * 群性质：绕固定点的旋转群
 */
#define PRESET_ROTATION "rotation"

/**
 * @brief 绕两点确定的轴旋转（三维）
 *
 * 数学定义：给定轴（两点确定）和角度，旋转点
 *
 * 输入：
 *   - P: 点 (PRESET_TYPE_POINT) - 待旋转的点
 *   - axis_p1: 点 (PRESET_TYPE_POINT) - 轴上一点
 *   - axis_p2: 点 (PRESET_TYPE_POINT) - 轴上另一点
 *   - angle: 标量 (PRESET_TYPE_SCALAR) - 旋转角度
 * 输出：
 *   - 旋转后的点 (PRESET_TYPE_POINT)
 *
 * 复杂度：O(1)
 */
#define PRESET_ROTATION_3D "rotation_3d"

/**
 * @brief 通过参考点构造旋转
 *
 * 数学定义：给定中心 $O$、参考点 $A$ 和目标点 $B$，
 * 构造旋转使得 $A$ 映射到 $B$（绕 $O$ 旋转）
 *
 * 输入：
 *   - P: 点 (PRESET_TYPE_POINT) - 待旋转的点
 *   - O: 点 (PRESET_TYPE_POINT) - 旋转中心
 *   - A: 点 (PRESET_TYPE_POINT) - 参考起点
 *   - B: 点 (PRESET_TYPE_POINT) - 参考终点
 * 输出：
 *   - 旋转后的点 (PRESET_TYPE_POINT)
 *
 * 复杂度：O(1)
 */
#define PRESET_ROTATION_BY_REFERENCE "rotation_by_reference"

/* ==================== 反射变换 ==================== */

/**
 * @brief 关于直线的反射
 *
 * 数学定义：给定直线 $l$，反射 $Ref_l(P)$ 是 $P$ 关于 $l$ 的对称点
 *
 * 输入：
 *   - P: 点 (PRESET_TYPE_POINT) - 待反射的点
 *   - l_p1: 点 (PRESET_TYPE_POINT) - 直线上一点
 *   - l_p2: 点 (PRESET_TYPE_POINT) - 直线上另一点
 * 输出：
 *   - 反射后的点 (PRESET_TYPE_POINT)
 *
 * 复杂度：O(1)
 * 性质：对合（$Ref_l \circ Ref_l = Id$）
 */
#define PRESET_REFLECTION_LINE "reflection_line"

/**
 * @brief 关于点的中心反射（点反射）
 *
 * 数学定义：给定点 $O$，中心反射 $C_O(P) = 2O - P$
 *
 * 输入：
 *   - P: 点 (PRESET_TYPE_POINT) - 待反射的点
 *   - O: 点 (PRESET_TYPE_POINT) - 中心点
 * 输出：
 *   - 反射后的点 (PRESET_TYPE_POINT)
 *
 * 复杂度：O(1)
 * 性质：等价于绕 $O$ 旋转 180°
 */
#define PRESET_REFLECTION_POINT "reflection_point"

/**
 * @brief 滑移反射
 *
 * 数学定义：反射与平行于反射轴的平移的复合
 *
 * 输入：
 *   - P: 点 (PRESET_TYPE_POINT)
 *   - l_p1: 点 (PRESET_TYPE_POINT) - 反射轴上一点
 *   - l_p2: 点 (PRESET_TYPE_POINT) - 反射轴上另一点
 *   - distance: 标量 (PRESET_TYPE_SCALAR) - 滑移距离
 * 输出：
 *   - 变换后的点 (PRESET_TYPE_POINT)
 *
 * 复杂度：O(1)
 */
#define PRESET_GLIDE_REFLECTION "glide_reflection"

/* ==================== 位似/缩放变换 ==================== */

/**
 * @brief 位似变换（中心缩放）
 *
 * 数学定义：给定中心 $O$ 和比例 $k$，位似 $H_{O,k}(P) = O + k(P - O)$
 *
 * 输入：
 *   - P: 点 (PRESET_TYPE_POINT) - 待缩放的点
 *   - O: 点 (PRESET_TYPE_POINT) - 位似中心
 *   - k: 标量 (PRESET_TYPE_SCALAR) - 比例系数
 * 输出：
 *   - 缩放后的点 (PRESET_TYPE_POINT)
 *
 * 复杂度：O(1)
 * 群性质：位似群（同构于乘法群）
 */
#define PRESET_HOMOTHETY "homothety"

/**
 * @brief 通过参考点构造位似
 *
 * 数学定义：给定中心 $O$、参考点 $A$ 和 $B$，构造位似使得 $A$ 映射到 $B$
 *
 * 输入：
 *   - P: 点 (PRESET_TYPE_POINT) - 待缩放的点
 *   - O: 点 (PRESET_TYPE_POINT) - 位似中心
 *   - A: 点 (PRESET_TYPE_POINT) - 参考起点
 *   - B: 点 (PRESET_TYPE_POINT) - 参考终点
 * 输出：
 *   - 缩放后的点 (PRESET_TYPE_POINT)
 *
 * 复杂度：O(1)
 */
#define PRESET_HOMOTHETY_BY_REFERENCE "homothety_by_reference"

/**
 * @brief 均匀缩放
 *
 * 数学定义：以原点为中心的比例缩放
 *
 * 输入：
 *   - P: 点 (PRESET_TYPE_POINT)
 *   - k: 标量 (PRESET_TYPE_SCALAR) - 比例系数
 * 输出：
 *   - 缩放后的点 (PRESET_TYPE_POINT)
 *
 * 复杂度：O(1)
 */
#define PRESET_SCALE "scale"

/* ==================== 仿射变换 ==================== */

/**
 * @brief 仿射变换（一般形式）
 *
 * 数学定义：$f(P) = A \cdot P + \vec{b}$，其中 $A$ 是线性变换矩阵
 *
 * 输入：
 *   - P: 点 (PRESET_TYPE_POINT)
 *   - matrix: 矩阵 (PRESET_TYPE_MATRIX) - 2×2变换矩阵
 *   - translation: 向量 (PRESET_TYPE_VECTOR) - 平移向量
 * 输出：
 *   - 变换后的点 (PRESET_TYPE_POINT)
 *
 * 复杂度：O(1)
 */
#define PRESET_AFFINE_TRANSFORM "affine_transform"

/**
 * @brief 错切变换
 *
 * 数学定义：沿某一方向的错切
 *
 * 输入：
 *   - P: 点 (PRESET_TYPE_POINT)
 *   - direction: 点 (PRESET_TYPE_POINT) - 错切方向
 *   - factor: 标量 (PRESET_TYPE_SCALAR) - 错切因子
 * 输出：
 *   - 变换后的点 (PRESET_TYPE_POINT)
 *
 * 复杂度：O(1)
 */
#define PRESET_SHEAR "shear"

/* ==================== 变换组合 ==================== */

/**
 * @brief 变换的复合
 *
 * 数学定义：给定变换 $f$ 和 $g$，构造复合变换 $g \circ f$
 *
 * 输入：
 *   - f: 变换 (PRESET_TYPE_FUNCTION) - 先应用的变换
 *   - g: 变换 (PRESET_TYPE_FUNCTION) - 后应用的变换
 * 输出：
 *   - 复合变换 (PRESET_TYPE_FUNCTION)
 *
 * 复杂度：取决于变换类型
 */
#define PRESET_TRANSFORM_COMPOSE "transform_compose"

/**
 * @brief 变换的逆
 *
 * 数学定义：给定可逆变换 $f$，构造其逆变换 $f^{-1}$
 *
 * 输入：
 *   - transform: 变换 (PRESET_TYPE_FUNCTION)
 * 输出：
 *   - 逆变换 (PRESET_TYPE_FUNCTION)
 *
 * 复杂度：取决于变换类型
 */
#define PRESET_TRANSFORM_INVERSE "transform_inverse"

/**
 * @brief 恒等变换
 *
 * 数学定义：$Id(P) = P$
 *
 * 输入：
 *   - P: 点 (PRESET_TYPE_POINT)
 * 输出：
 *   - 点 (PRESET_TYPE_POINT) - 不变的点
 *
 * 复杂度：O(1)
 */
#define PRESET_IDENTITY_TRANSFORM "identity_transform"

/* ==================== 特殊变换 ==================== */

/**
 * @brief 反演变换（关于圆）
 *
 * 数学定义：给定圆 $C(O, r)$，点 $P$ 的反演点 $P'$ 满足 $|OP| \cdot |OP'| = r^2$
 *
 * 输入：
 *   - P: 点 (PRESET_TYPE_POINT)
 *   - O: 点 (PRESET_TYPE_POINT) - 反演中心
 *   - R: 点 (PRESET_TYPE_POINT) - 反演圆半径点
 * 输出：
 *   - 反演点 (PRESET_TYPE_POINT)
 *
 * 复杂度：O(1)
 * 注意：中心点无反演（映射到无穷远）
 */
#define PRESET_INVERSION "inversion"

/**
 * @brief 螺旋相似
 *
 * 数学定义：旋转与位似的复合，$S_{O,\theta,k}(P) = O + k \cdot R_{\theta}(P - O)$
 *
 * 输入：
 *   - P: 点 (PRESET_TYPE_POINT)
 *   - O: 点 (PRESET_TYPE_POINT) - 中心
 *   - angle: 标量 (PRESET_TYPE_SCALAR) - 旋转角度
 *   - k: 标量 (PRESET_TYPE_SCALAR) - 比例
 * 输出：
 *   - 变换后的点 (PRESET_TYPE_POINT)
 *
 * 复杂度：O(1)
 */
#define PRESET_SPIRAL_SIMILARITY "spiral_similarity"

/* ==================== 模块初始化 ==================== */

/**
 * @brief 注册几何变换预设函数块
 *
 * 此函数由 preset_blocks_init() 自动调用。
 *
 * @return true 注册成功
 * @return false 注册失败
 */
bool preset_transformations_register(void);

/**
 * @brief 获取变换模块的预设数量
 *
 * @return 预设函数块数量
 */
int preset_transformations_count(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_TRANSFORMATIONS_H */
