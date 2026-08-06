/**
 * @file interval_arith.h
 * @brief Lv-00 公共区间算术库 —— 数值验证策略的统一区间底座
 *
 * @details 以 float_error.c 的 float_interval_* 实现为语义基准（最完整：
 *          含 nextafter 方向舍入与定义域外处理），收敛本项目多份历史区间实现：
 *          - gappa_propagate.c 的 ia_*（表达式 -> double [lo,hi] 正/反向传播）
 *          - float_error.c 的 float_interval_*（FPTaylor 一阶泰勒误差界）
 *          - interval_arithmetic.c 的 interval_*（IEEE 1788 空区间语义，
 *            保留不动，供符号求值/自适应验证使用，与本库不冲突）
 *
 *          区间类型复用 interval_arithmetic.h 的 lvInterval（lo/hi/is_exact），
 *          避免重复定义同名结构。本库统一以 float_error 语义处理定义域外情形：
 *          除数跨零、sqrt/log/asin/acos 定义域外返回保守的全实数区间
 *          [-HUGE_VAL, HUGE_VAL]（而非空区间），保证 FPTaylor 的误差界
 *          half_width 不会因空区间（lo>hi）变为负值而误判 TrustColor。
 *
 * @version 1.0.0
 * @date 2026-08-06
 */

#ifndef lv_INTERVAL_ARITH_H
#define lv_INTERVAL_ARITH_H

#include <math.h>
#include <stdbool.h>
#include <stddef.h>

#include "lv/interval_arithmetic.h" /* lvInterval { double lo, hi; int is_exact; } */

#ifdef __cplusplus
extern "C" {
#endif

/* ── 构造 ── */

/** @brief 构造区间 [lo, hi]，is_exact 标记是否精确 */
lvInterval lv_interval_make(double lo, double hi, int is_exact);

/* ── 基础运算（语义基准：float_error.c 的 float_interval_*）── */

/** @brief 区间加法：[a.lo+b.lo, a.hi+b.hi]，端点向外取整 */
lvInterval lv_interval_add(lvInterval a, lvInterval b);

/** @brief 区间减法：[a.lo-b.hi, a.hi-b.lo]，端点向外取整 */
lvInterval lv_interval_sub(lvInterval a, lvInterval b);

/** @brief 区间乘法：四个角点 min/max，端点向外取整 */
lvInterval lv_interval_mul(lvInterval a, lvInterval b);

/** @brief 区间除法：除数跨零 -> [-HUGE_VAL, HUGE_VAL]；否则倒数乘法 */
lvInterval lv_interval_div(lvInterval a, lvInterval b);

/** @brief 区间平方根：负下界截断到 0（与 float_error 一致）；is_exact = a.is_exact && (a.lo == a.hi) */
lvInterval lv_interval_sqrt(lvInterval a);

/** @brief 区间正弦（弧度）：处理非单调区间与极值点，结果范围 [-1, 1] */
lvInterval lv_interval_sin(lvInterval a);

/** @brief 区间余弦（弧度）：处理非单调区间与极值点，结果范围 [-1, 1] */
lvInterval lv_interval_cos(lvInterval a);

/** @brief 区间指数：单调递增，端点向外取整 */
lvInterval lv_interval_exp(lvInterval a);

/** @brief 区间自然对数：非正下界 -> [-HUGE_VAL, ...]（与 float_error 一致） */
lvInterval lv_interval_log(lvInterval a);

/** @brief 区间绝对值 */
lvInterval lv_interval_abs(lvInterval a);

/** @brief 区间取反：[-a.hi, -a.lo] */
lvInterval lv_interval_neg(lvInterval a);

/* ── 扩展函数（保守端点法，奇点/定义域处理见 interval_arith.c 注释）── */

/** @brief 区间正切（弧度）：区间含 π/2+kπ 奇点 -> 全实数；否则端点 min/max */
lvInterval lv_interval_tan(lvInterval a);

/** @brief 区间反正切：单调递增，端点向外取整 */
lvInterval lv_interval_atan(lvInterval a);

/** @brief 区间幂 a^b：正底 4 角点法；负底仅整数幂端点法；否则全实数 */
lvInterval lv_interval_pow(lvInterval a, lvInterval b);

/** @brief 区间反正弦：与定义域 [-1,1] 求交后按单调递增求值 */
lvInterval lv_interval_asin(lvInterval a);

/** @brief 区间反余弦：与定义域 [-1,1] 求交后按单调递减求值 */
lvInterval lv_interval_acos(lvInterval a);

/** @brief 区间向下取整：单调不减，结果精确整数 */
lvInterval lv_interval_floor(lvInterval a);

/** @brief 区间向上取整：单调不减，结果精确整数 */
lvInterval lv_interval_ceil(lvInterval a);

#ifdef __cplusplus
}
#endif

#endif /* lv_INTERVAL_ARITH_H */
