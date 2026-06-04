/**
 * @file math_theory_guide_cn.h
 * @brief 理论数学研究中文指南
 *
 * @details 为理论数学研究者提供 Lv-00 系统的中文使用指南。
 *          涵盖几何、代数、拓扑、逻辑等数学分支的研究接口。
 *
 * 【适用场景】
 * - 几何定理机器证明
 * - 代数结构可视化
 * - 拓扑空间构造
 * - 数理逻辑验证
 * - 范畴论形式化
 * - 同调代数计算
 *
 * @author Lv-00 Project
 * @version 3.5.0
 */
#ifndef LV00_MATH_THEORY_GUIDE_CN_H
#define LV00_MATH_THEORY_GUIDE_CN_H
#include <stdbool.h>
#include <stddef.h>  /* size_t */
#ifdef __cplusplus
extern "C" {
#endif
/* ============================================================
 * 几何学研究接口
 * ============================================================ */
/**
 * @brief 欧几里得几何研究接口
 *
 * 支持的构造：
 * - 点、线段、直线、射线
 * - 圆、圆弧、椭圆
 * - 多边形（三角形、四边形等）
 * - 角平分线、垂直平分线
 *
 * 示例代码：
 * @code
 *   // 创建等边三角形
 *   LV00Engine *engine = lv00_engine_create();
 *   int p1 = lv00_add_point(engine, 0, 1, 0, 1);
 *   int p2 = lv00_add_point(engine, 3, 1, 0, 1);
 *   int p3 = lv00_add_point(engine, 0, 1, 4, 1);
 *   lv00_add_line_segment(engine, p1, p2);
 *   lv00_add_line_segment(engine, p2, p3);
 *   lv00_add_line_segment(engine, p3, p1);
 * @endcode
 */
void guide_euclidean_geometry_cn(char *buf, size_t buf_size);
/**
 * @brief 解析几何研究接口
 *
 * 支持的运算：
 * - 坐标系变换
 * - 距离计算
 * - 角度计算
 * - 面积计算
 */
void guide_analytic_geometry_cn(char *buf, size_t buf_size);
/**
 * @brief 射影几何研究接口
 *
 * 支持的构造：
 * - 射影平面点
 * - 直线无穷远点
 * - 透视对应
 * - 交比计算
 */
void guide_projective_geometry_cn(char *buf, size_t buf_size);
/* ============================================================
 * 代数学研究接口
 * ============================================================ */
/**
 * @brief 线性代数研究接口
 *
 * 支持的运算：
 * - 矩阵运算（加法、乘法、转置、逆）
 * - 向量运算（点积、叉积、投影）
 * - 特征值与特征向量
 * - 行列式计算
 * - 线性方程组求解
 */
void guide_linear_algebra_cn(char *buf, size_t buf_size);
/**
 * @brief 抽象代数研究接口
 *
 * 支持的结构：
 * - 群（阿贝尔群、循环群、对称群）
 * - 环（多项式环、商环）
 * - 域（有限域、扩域）
 * - 模与向量空间
 */
void guide_abstract_algebra_cn(char *buf, size_t buf_size);
/**
 * @brief 数论研究接口
 *
 * 支持的运算：
 * - 整除性与素数判定
 * - 最大公约数与最小公倍数
 * - 模运算与同余
 * - 原根与离散对数
 * - 连分数展开
 */
void guide_number_theory_cn(char *buf, size_t buf_size);
/**
 * @brief 多项式代数研究接口
 *
 * 支持的运算：
 * - 多项式加减乘除
 * - 多项式因式分解
 * - 多项式求导与积分
 * - 多项式结式计算
 * - Groebner基计算
 */
void guide_polynomial_algebra_cn(char *buf, size_t buf_size);
/* ============================================================
 * 拓扑学研究接口
 * ============================================================ */
/**
 * @brief 点集拓扑研究接口
 *
 * 支持的概念：
 * - 开集与闭集
 * - 连续映射
 * - 同胚
 * - 紧致性与连通性
 */
void guide_point_set_topology_cn(char *buf, size_t buf_size);
/**
 * @brief 代数拓扑研究接口
 *
 * 支持的结构：
 * - 基本群
 * - 单纯复形
 * - 同调群
 * - 欧拉示性数
 */
void guide_algebraic_topology_cn(char *buf, size_t buf_size);
/* ============================================================
 * 数理逻辑研究接口
 * ============================================================ */
/**
 * @brief 命题逻辑研究接口
 *
 * 支持的运算：
 * - 合取、析取、否定
 * - 蕴含、等价
 * - 永真式判定
 * - 可满足性判定
 */
void guide_propositional_logic_cn(char *buf, size_t buf_size);
/**
 * @brief 一阶逻辑研究接口
 *
 * 支持的运算：
 * - 全称量词与存在量词
 * - 量词消去
 * - 前束范式转换
 * - Skolem化
 */
void guide_first_order_logic_cn(char *buf, size_t buf_size);
/* ============================================================
 * 范畴论研究接口
 * ============================================================ */
/**
 * @brief 范畴论基础研究接口
 *
 * 支持的概念：
 * - 范畴与对象
 * - 态射与复合
 * - 函子与自然变换
 * - 米田引理
 */
void guide_category_theory_cn(char *buf, size_t buf_size);
/* ============================================================
 * 同调代数研究接口
 * ============================================================ */
/**
 * @brief 同调代数研究接口
 *
 * 支持的结构：
 * - 链复形
 * - 边界算子
 * - 同调群计算
 * - 短正合列
 */
void guide_homological_algebra_cn(char *buf, size_t buf_size);
/* ============================================================
 * 微分几何研究接口
 * ============================================================ */
/**
 * @brief 微分几何研究接口
 *
 * 支持的概念：
 * - 切向量与余切向量
 * - 度量张量
 * - 曲率与挠率
 * - Levi-Civita联络
 */
void guide_differential_geometry_cn(char *buf, size_t buf_size);
/* ============================================================
 * 综合指南
 * ============================================================ */
/**
 * @brief 生成完整的研究指南
 *
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return 写入的字符数
 */
int guide_generate_full_cn(char *buf, size_t buf_size);
/**
 * @brief 生成快速开始指南
 *
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return 写入的字符数
 */
int guide_quick_start_cn(char *buf, size_t buf_size);
/**
 * @brief 生成数学符号对照表
 *
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return 写入的字符数
 */
int guide_symbol_reference_cn(char *buf, size_t buf_size);
/**
 * @brief 获取预设模块的数学定义
 *
 * @param preset_name 预设名称
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return 写入的字符数，失败返回-1
 */
int guide_preset_math_definition_cn(const char *preset_name, char *buf, size_t buf_size);
#ifdef __cplusplus
}
#endif
#endif /* LV00_MATH_THEORY_GUIDE_CN_H */
