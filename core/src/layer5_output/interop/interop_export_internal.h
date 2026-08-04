/**
 * @file interop_export_internal.h
 * @brief 互操作导出内部共享声明（SVG/TikZ/PDF 导出器共用）
 *
 * 集中定义曾经在三个导出器中各自维护、靠人肉同步的公共数据：
 *   1. 约束类型 → 绘图参数核心表（kConstraintVisuals），覆盖全部 6 种约束类型；
 *   2. 信任颜色 → 各导出器颜色字符串全字段表（kTrustColorEntries）；
 *   3. 公共几何工具函数：compute_bezier_control_points / segment_intersection。
 * 三个导出器只保留"核心表 → 本语法"的窄适配，颜色/线宽以核心表为唯一来源。
 */

#ifndef lv_INTEROP_EXPORT_INTERNAL_H
#define lv_INTEROP_EXPORT_INTERNAL_H

#include "lv/constraint_graph.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 计算约束图的包围盒（SVG 与 PDF 导出共用） */
void compute_bounding_box(const ConstraintGraph *graph, double *min_x, double *min_y, double *max_x,
                          double *max_y);

/* ==================== 约束类型 → 绘图参数核心表 ==================== */

/** @brief 语义线型枚举（各导出器再转换为自己的语法） */
typedef enum {
    CONSTRAINT_LINE_SOLID = 0,      /**< 实线 */
    CONSTRAINT_LINE_DASHED,         /**< 虚线 */
    CONSTRAINT_LINE_DOTTED,         /**< 点线 */
    CONSTRAINT_LINE_DASH_DOTTED     /**< 虚点线 */
} ConstraintLineStyle;

/** @brief 约束可视化参数（跨导出器统一的绘图参数核心条目） */
typedef struct {
    ConstraintType type;            /**< 约束类型 */
    uint8_t rgb[3];                 /**< 权威 RGB 颜色（0-255，唯一颜色来源） */
    ConstraintLineStyle line_style; /**< 语义线型 */
    float line_width;               /**< 权威线宽 */
} ConstraintVisual;

/**
 * @brief 约束类型 → 绘图参数核心表（覆盖全部 6 种约束类型）
 * @note 三个导出器（SVG/TikZ/PDF）均从此表取颜色与线宽，
 *       再经各自的窄适配函数转换为本语法输出，杜绝颜色人肉同步漂移。
 */
static const ConstraintVisual kConstraintVisuals[] = {
    [INCIDENCE]    = { INCIDENCE,    {107, 114, 128}, CONSTRAINT_LINE_DASHED,      1.0f },
    [BETWEENNESS]  = { BETWEENNESS,  { 99, 102, 241}, CONSTRAINT_LINE_DASHED,      1.0f },
    [INTERSECTION] = { INTERSECTION, {168,  85, 247}, CONSTRAINT_LINE_SOLID,       1.0f },
    [CONTAINMENT]  = { CONTAINMENT,  { 20, 184, 166}, CONSTRAINT_LINE_DOTTED,      1.0f },
    [CONNECTION]   = { CONNECTION,   {245, 158,  11}, CONSTRAINT_LINE_SOLID,       1.5f },
    [ANGLE]        = { ANGLE,        {168,  85, 247}, CONSTRAINT_LINE_DASH_DOTTED, 1.0f },
};

/** @brief 按约束类型查找核心表条目（未命中返回 NULL） */
static inline const ConstraintVisual *constraint_visual_find(ConstraintType type) {
    for (size_t i = 0; i < sizeof(kConstraintVisuals) / sizeof(kConstraintVisuals[0]); i++) {
        if (kConstraintVisuals[i].type == type)
            return &kConstraintVisuals[i];
    }
    return NULL;
}

/* ==================== 信任颜色 → 各导出器颜色字符串全字段表 ==================== */

/** @brief 信任颜色映射条目（SVG/TikZ/PDF 三套输出统一维护） */
typedef struct {
    TrustColor trust;      /**< 信任颜色枚举 */
    const char *svg_hex;   /**< SVG 十六进制颜色（如 "#22c55e"） */
    const char *tikz_expr; /**< TikZ/LaTeX 颜色表达式（如 "green!70!black"） */
    const char *pdf_rgba;  /**< PDF RG/rg 颜色三元组（如 "0.13 0.76 0.29"） */
} TrustColorEntry;

/**
 * @brief 信任颜色全字段表（按枚举值升序，10 项）
 * @note SVG/TikZ 输出与历史行为完全一致；PDF 侧修复了 TRUST_AMBER 误输出红色
 *       （0.94 0.27 0.27）的问题，现与 SVG 语义一致输出橙色（0.96 0.62 0.04）。
 *       非绿/非琥珀色在 PDF 侧维持历史灰色（0.61 0.64 0.69），保证输出不变。
 */
static const TrustColorEntry kTrustColorEntries[] = {
    [TRUST_GREEN]               = { TRUST_GREEN,               "#22c55e", "green!70!black",  "0.13 0.76 0.29" },
    [TRUST_BLUE_UNEXPLORED]     = { TRUST_BLUE_UNEXPLORED,     "#3b82f6", "blue!70!black",   "0.61 0.64 0.69" },
    [TRUST_BLUE_EXCEEDED]       = { TRUST_BLUE_EXCEEDED,       "#6366f1", "blue!50!black",   "0.61 0.64 0.69" },
    [TRUST_BLUE_OUT_OF_SCOPE]   = { TRUST_BLUE_OUT_OF_SCOPE,   "#93c5fd", "blue!30!black",   "0.61 0.64 0.69" },
    [TRUST_YELLOW]              = { TRUST_YELLOW,              "#eab308", "yellow!70!black", "0.61 0.64 0.69" },
    [TRUST_LIGHT_ORANGE_ORACLE] = { TRUST_LIGHT_ORANGE_ORACLE, "#fb923c", "orange!40!black", "0.61 0.64 0.69" },
    [TRUST_LIGHT_ORANGE_EXPLOSION] = { TRUST_LIGHT_ORANGE_EXPLOSION, "#f97316", "orange!60!black", "0.61 0.64 0.69" },
    [TRUST_AMBER]               = { TRUST_AMBER,               "#f59e0b", "orange!80!black", "0.96 0.62 0.04" },
    [TRUST_DEEP_ORANGE]         = { TRUST_DEEP_ORANGE,         "#ea580c", "red!70!black",    "0.61 0.64 0.69" },
    [TRUST_RED]                 = { TRUST_RED,                 "#ef4444", "red!80!black",    "0.61 0.64 0.69" },
};

/** @brief 按信任颜色查找全字段表条目（未命中返回 NULL） */
static inline const TrustColorEntry *interop_trust_color_find(TrustColor trust) {
    for (size_t i = 0; i < sizeof(kTrustColorEntries) / sizeof(kTrustColorEntries[0]); i++) {
        if (kTrustColorEntries[i].trust == trust)
            return &kTrustColorEntries[i];
    }
    return NULL;
}

/* ==================== 公共几何工具函数 ==================== */

/**
 * @brief 计算贝塞尔曲线的两个控制点（SVG cubic 与 PDF c 操作符共用）
 * @details 控制点公式：
 *          CP1 = P1 + 0.3*(P2-P1) + 垂直法向偏移
 *          CP2 = P2 - 0.3*(P2-P1) + 垂直法向偏移
 *          其中垂直偏移 offset = 0.15*|P2-P1|（过短时固定为 5.0），
 *          法向 n = (-dy, dx) / (|P2-P1| + 0.001)。
 * @param p1x,p1y 起点坐标
 * @param p2x,p2y 终点坐标
 * @param cp1x,cp1y [out] 第一控制点
 * @param cp2x,cp2y [out] 第二控制点
 */
void compute_bezier_control_points(double p1x, double p1y, double p2x, double p2y,
                                   double *cp1x, double *cp1y, double *cp2x, double *cp2y);

/**
 * @brief 计算两条线段的交点（SVG 相交约束渲染共用）
 * @details 解线段参数方程：P1 + t*(P2-P1) = Q1 + s*(Q2-Q1)。
 *          平行/共线（|cross|<=1e-10）或交点超出 t∈[-0.05, 1.05] 范围时返回 false，
 *          此时不写 ix/iy。
 * @return true 交点有效；false 无有效交点
 */
bool segment_intersection(double p1x, double p1y, double p2x, double p2y,
                          double q1x, double q1y, double q2x, double q2y,
                          double *ix, double *iy);

#ifdef __cplusplus
}
#endif

#endif /* lv_INTEROP_EXPORT_INTERNAL_H */
