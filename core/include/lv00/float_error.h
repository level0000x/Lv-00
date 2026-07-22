/**
 * @file float_error.h
 * @brief FPTaylor 风格浮点误差验证 —— 泰勒展开 + 区间算术误差界分析
 *
 * @details 借鉴 FPTaylor（FPBench 项目）的浮点误差分析方法论，为 Lv-00
 *          的几何约束系统提供严格的浮点误差验证。核心思路：
 *
 *          1. 将几何约束表达式转换为浮点表达式
 *          2. 对表达式进行一阶（或高阶）泰勒展开，分离中心值和导数项
 *          3. 用区间算术（Interval Arithmetic）对变量范围做误差传播分析
 *          4. 输出绝对误差界和相对误差界，按信任颜色分级
 *
 *          区间算术基于 Moore (1966) 的区间分析理论，所有基本运算
 *          （加减乘除、sqrt、sin、cos、exp、log）均在区间上做保守估计。
 *          可选集成 Z3 优化模式和 GELPIA（Guaranteed Error Local
 *          Polyhedral Interval Arithmetic）。
 *
 *          误差验证流程：
 *          - fptaylor_evaluate_expr：对任意浮点表达式做误差评估
 *          - fptaylor_evaluate_graph：对约束图中特定变量的约束系统做误差分析
 *          - fptaylor_verify_safety：判断误差是否在容差范围内
 *
 *          TrustColor 类型从 symbolic_coord.h 引入，用于将误差界映射到
 *          Lv-00 的信任颜色系统。
 *
 *          设计借鉴：
 *          - FPTaylor (github.com/soarlab/FPTaylor) — 浮点误差泰勒分析
 *          - Gappa (gappa.gitlabpages.inria.fr) — 形式化浮点证明
 *          - FLUCTUAT (CEA LIST) — 基于抽象解释的浮点误差分析
 *
 * @version 1.1.0
 * @date 2026-05-24
 */

#ifndef LV00_FLOAT_ERROR_H
#define LV00_FLOAT_ERROR_H

#include "constraint_graph.h"
#include "exact_arithmetic.h" /* LV00_TOLERATED_FLOAT for error-analysis double */
#include "lv00.h"
#include "symbolic_coord.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 泰勒形式结构体
 * ======================================================================== */

/**
 * @brief 泰勒形式 —— 一阶泰勒展开的符号表示
 *
 * 将浮点表达式 f(x1,...,xn) 展开为：
 *   f(center) + SUM_i (df/dxi) * (xi - center_i) + O(h^2)
 *
 * 其中 center_val = f(center) 是中心点处的值，
 * first_derivs[i] = df/dxi|center 是第 i 个偏导数值，
 * interval_lo/hi 是完整区间估计（含余项）。
 *
 * 用于误差界分析时将变量不确定性传播为结果不确定性。
 */
typedef struct {
    double LV00_TOLERATED_FLOAT(center_val);    /**< 中心点处的函数值 f(center) */
    double LV00_TOLERATED_FLOAT(*first_derivs); /**< 一阶偏导数数组 df/dxi|center */
    int *deriv_var_ids;   /**< 导数对应的变量 ID 数组 */
    int deriv_count;      /**< 偏导数数量（= 变量数） */
    double LV00_TOLERATED_FLOAT(interval_lo);   /**< 区间下界（含泰勒余项） */
    double LV00_TOLERATED_FLOAT(interval_hi);   /**< 区间上界（含泰勒余项） */
    int order;            /**< 泰勒展开阶数（1 = 一阶，2 = 二阶，...） */
} TaylorForm;

/* ========================================================================
 * 区间算术类型
 * ======================================================================== */

/**
 * @brief 浮点区间 —— 区间算术的基本数据类型
 *
 * 表示一个实数范围 [lo, hi]，所有区间运算保证结果区间
 * 包含所有可能的真实值（保守性）。is_exact 标记区间是否
 * 精确（即 lo == hi 且无舍入误差）。
 *
 * 区间算术遵循 IEEE 1788 标准的基本操作定义。
 */
typedef struct {
    double LV00_TOLERATED_FLOAT(lo);     /**< 区间下界 */
    double LV00_TOLERATED_FLOAT(hi);     /**< 区间上界 */
    bool is_exact; /**< 是否为精确值（lo == hi） */
} FloatInterval;

/* ========================================================================
 * 区间算术操作函数
 * ======================================================================== */

/**
 * @brief 区间加法：a + b = [a.lo + b.lo, a.hi + b.hi]
 *
 * 基于最小-最大原理，加法的精确界由端点的直接加法给出
 * （考虑舍入方向时需向外取整）。
 *
 * @param[in] a 区间 a
 * @param[in] b 区间 b
 * @return 结果区间
 */
FloatInterval float_interval_add(FloatInterval a, FloatInterval b);

/**
 * @brief 区间减法：a - b = [a.lo - b.hi, a.hi - b.lo]
 *
 * @param[in] a 区间 a
 * @param[in] b 区间 b
 * @return 结果区间
 */
FloatInterval float_interval_sub(FloatInterval a, FloatInterval b);

/**
 * @brief 区间乘法：a * b = [min(S), max(S)]
 *
 * 其中 S = {a.lo*b.lo, a.lo*b.hi, a.hi*b.lo, a.hi*b.hi}，
 * 取四个角点的最小-最大值。
 *
 * @param[in] a 区间 a
 * @param[in] b 区间 b
 * @return 结果区间
 */
FloatInterval float_interval_mul(FloatInterval a, FloatInterval b);

/**
 * @brief 区间除法：a / b
 *
 * 要求 0 不在分母区间内。若 b 跨越零点则返回 NaN 区间。
 *
 * @param[in] a 区间 a
 * @param[in] b 区间 b
 * @return 结果区间
 */
FloatInterval float_interval_div(FloatInterval a, FloatInterval b);

/**
 * @brief 区间平方根：sqrt(a)
 *
 * 要求 a.lo >= 0。使用单调递增性质：
 * sqrt([lo, hi]) = [sqrt(lo), sqrt(hi)]
 *
 * @param[in] a 区间 a
 * @return 结果区间
 */
FloatInterval float_interval_sqrt(FloatInterval a);

/**
 * @brief 区间正弦：sin(a)
 *
 * 处理正弦函数的非单调性，检查区间内是否包含拐点
 * （pi/2 + k*pi），取最大值 1 和最小值 -1。
 *
 * @param[in] a 区间 a
 * @return 结果区间
 */
FloatInterval float_interval_sin(FloatInterval a);

/**
 * @brief 区间余弦：cos(a)
 *
 * 类似正弦，检查区间内是否包含拐点（k*pi）。
 *
 * @param[in] a 区间 a
 * @return 结果区间
 */
FloatInterval float_interval_cos(FloatInterval a);

/**
 * @brief 区间指数：exp(a)
 *
 * 单调递增：exp([lo, hi]) = [exp(lo), exp(hi)]
 *
 * @param[in] a 区间 a
 * @return 结果区间
 */
FloatInterval float_interval_exp(FloatInterval a);

/**
 * @brief 区间对数：log(a)
 *
 * 要求 a.lo > 0。单调递增：
 * log([lo, hi]) = [log(lo), log(hi)]
 *
 * @param[in] a 区间 a
 * @return 结果区间
 */
FloatInterval float_interval_log(FloatInterval a);

/* ========================================================================
 * 误差界与信任颜色
 * ======================================================================== */

/**
 * @brief 浮点误差界
 *
 * 包含对某个表达式或变量的误差分析结果。
 * 绝对误差 = |computed - exact| 的上界
 * 相对误差 = |computed - exact| / |exact| 的上界
 *
 * proof_text 提供形式化的误差证明文本（如 FPTaylor 输出的证书）。
 */
typedef struct {
    double LV00_TOLERATED_FLOAT(absolute_error);  /**< 绝对误差上界 */
    double LV00_TOLERATED_FLOAT(relative_error);  /**< 相对误差上界 */
    TrustColor trust_level; /**< 信任颜色等级 */
    char *proof_text;       /**< 误差证明文本（调用者负责 free） */
} ErrorBound;

/* ========================================================================
 * 误差评估配置
 * ======================================================================== */

/**
 * @brief FPTaylor 误差评估配置
 *
 * 控制泰勒展开、优化、和分支切割行为的参数。
 */
typedef struct {
    bool use_optimization;         /**< 是否启用优化模式（减少误差过估） */
    int taylor_order;              /**< 泰勒展开阶数（默认 1，支持 1-3） */
    bool use_z3_opt;               /**< 是否使用 Z3 优化后端（精确区间收缩） */
    bool use_gelpia;               /**< 是否使用 GELPIA 多面体区间算术 */
    double branch_bound_threshold; /**< 分支切割阈值（区间宽度超过此值时进行二分） */
} FPTaylorConfig;

/* ========================================================================
 * 主评估 API
 * ======================================================================== */

/**
 * @brief 对浮点表达式进行误差评估
 *
 * 解析给定表达式字符串，在变量区间边界约束下进行一阶泰勒
 * 展开和区间误差传播，输出误差界。
 *
 * 表达式语法支持：
 *   - 基本算术：+ - * /
 *   - 函数：sqrt, sin, cos, exp, log, abs
 *   - 变量名：x0, x1, ..., y, z 等
 *
 * 示例：
 *   fptaylor_evaluate_expr("sqrt(x0*x0 + x1*x1)", bounds, 2, &cfg, &err);
 *
 * @param[in]  expr       浮点表达式字符串
 * @param[in]  var_bounds 各变量的区间边界数组
 * @param[in]  var_count  变量数量
 * @param[in]  cfg        误差评估配置（NULL = 使用默认配置）
 * @param[out] out        输出的误差界
 * @return true 成功，false 失败
 */
bool fptaylor_evaluate_expr(const char *expr, const FloatInterval *var_bounds, int var_count, const FPTaylorConfig *cfg,
                            ErrorBound *out);

/**
 * @brief 对约束图中指定变量进行浮点误差分析
 *
 * 这是一个将 FPTaylor 方法论应用于 Lv-00 几何约束系统的
 * 独特函数。其工作流程：
 *
 *   1. 从约束图中提取所有涉及 var_id 的约束方程
 *   2. 将几何约束转换为浮点表达式字符串
 *   3. 对每个约束方程进行泰勒展开分析
 *   4. 用区间算术聚合所有约束的误差贡献
 *   5. 输出综合误差界
 *
 * 适用于评估几何构造中关键节点的数值稳定性，
 * 例如验证某个交点坐标是否因浮点误差偏离了理论位置。
 *
 * @param[in]  graph   约束图
 * @param[in]  var_id  待分析的变量节点 ID
 * @param[in]  cfg     误差评估配置
 * @param[out] out     输出的误差界
 * @return true 成功，false 失败
 */
bool fptaylor_evaluate_graph(const ConstraintGraph *graph, int var_id, const FPTaylorConfig *cfg, ErrorBound *out);

/**
 * @brief 验证误差界是否在安全容差范围内
 *
 * 根据绝对误差与容差的比较，返回信任颜色等级：
 *   - TRUST_GREEN:  absolute_error <= 1e-12（高精度安全）
 *   - TRUST_BLUE:   absolute_error <= 1e-10（一般安全）
 *   - TRUST_AMBER:  absolute_error <= tolerance（边界安全，含数值假设）
 *   - TRUST_RED:    absolute_error > tolerance（不安全，已证伪）
 *   - TRUST_YELLOW: 无法确定（评估失败或信息不足）
 *
 * 此函数是 Lv-00 信任颜色系统与浮点误差分析的桥梁，
 * 确保几何证明链中的数值步骤可以被信任颜色机制追踪。
 *
 * @param[in] bound     误差界
 * @param[in] tolerance 用户指定的容差阈值
 * @return 信任颜色等级
 */
TrustColor fptaylor_verify_safety(const ErrorBound *bound, double tolerance);

/* ========================================================================
 * 便捷工厂函数
 * ======================================================================== */

/**
 * @brief 创建默认 FPTaylor 配置
 *
 * 默认：taylor_order=1, use_optimization=true, use_z3_opt=false,
 * use_gelpia=false, branch_bound_threshold=1e-6
 *
 * @return 默认配置
 */
FPTaylorConfig fptaylor_config_default(void);

/**
 * @brief 从区间边界创建 FloatInterval
 *
 * @param[in] lo      下界
 * @param[in] hi      上界
 * @param[in] is_exact 是否精确
 * @return FloatInterval
 */
FloatInterval interval_make(double lo, double hi, bool is_exact);

/**
 * @brief 释放 ErrorBound 内部资源
 *
 * @param[in,out] bound 误差界
 */
void error_bound_destroy(ErrorBound *bound);

#ifdef __cplusplus
}
#endif

#endif /* LV00_FLOAT_ERROR_H */
