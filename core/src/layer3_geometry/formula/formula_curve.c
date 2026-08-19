/**
 * @file formula_curve.c
 * @brief 代数方程到隐式曲线转换实现（从 formula_converter.c 拆分）
 *
 * @details Marching Squares 等值线采样，以及 AST 到二维多项式系数的
 *          扁平化展开（支持 + - * ^ 与 sin/cos/sqrt 等一元函数）。
 */

#include "formula_converter_internal.h"
#include "lv/formula_converter.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/error_codes.h"
#include "lv/constraint_graph.h"
#include "lv/geo_utils.h"
#include "lv/lv_xmacro.h" /* LV_DISPATCH */
#include "lv/lv_internal.h"
#include "lv/lv_numeric.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_utils.h"

/* ============================================================
 * 代数方程到曲线转换实现（新增）
 * ============================================================ */

/**
 * 销毁方程曲线转换结果
 */
void equation_curve_result_destroy(EquationCurveResult *result) {
    if (!result)
        return;

    if (result->points) {
        lv_free((void **) &result->points); /* 统一内存释放器 */
    }
    lv_free((void **) &result); /* 统一内存释放器 */
}

/**
 * 评估公式节点在特定点的值 (递归)
 * 用于计算方程在特定点的值
 */


/**
 * 使用行进正方形算法（Marching Squares）采样隐式曲线
 * 这是计算机图形学中用于提取等值线的标准算法
 */
EquationCurveResult *formula_convert_equation_to_curve(const FormulaNode *equation_node, int sample_count, double x_min,
                                                       double x_max, double y_min, double y_max) {
    EquationCurveResult *result =
        (EquationCurveResult *) lv_calloc(1, sizeof(EquationCurveResult)); /* 统一内存分配器 */
    if (!result) {
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "failed to allocate result");
    }

    /* 参数验证 */
    if (!equation_node || sample_count <= 0) {
        result->success = false;
        lv_snprintf(result->error_message, sizeof(result->error_message), "Invalid parameters");
        return result;
    }

    /* 分配采样点数组 */
    result->points = (CurveSamplePoint *) lv_calloc(sample_count, sizeof(CurveSamplePoint)); /* 统一内存分配器 */
    if (!result->points) {
        result->success = false;
        lv_snprintf(result->error_message, sizeof(result->error_message), "Memory allocation failed");
        return result;
    }

    /* 生成方程字符串表示 */
    node_to_string(equation_node, result->equation_str, sizeof(result->equation_str));

    /* 设置边界框 */
    result->bbox_min_x = x_min;
    result->bbox_min_y = y_min;
    result->bbox_max_x = x_max;
    result->bbox_max_y = y_max;

    /*
     * 使用自适应网格采样策略：
     * 1. 首先在整个区域进行粗采样
     * 2. 识别可能包含曲线的区域（函数值变号或接近零）
     * 3. 在这些区域进行精细采样
     */

    int grid_size = (int) sqrt((double) sample_count * 2);
    if (grid_size < 10)
        grid_size = 10;

    double dx = (x_max - x_min) / grid_size;
    double dy = (y_max - y_min) / grid_size;

    /* 第一阶段：粗采样，计算网格点上的函数值 */
    double *grid_values = (double *) lv_calloc((grid_size + 1) * (grid_size + 1), sizeof(double)); /* 统一内存分配器 */
    if (!grid_values) {
        result->success = false;
        lv_snprintf(result->error_message, sizeof(result->error_message), "Memory allocation failed for grid");
        return result;
    }

    for (int i = 0; i <= grid_size; i++) {
        for (int j = 0; j <= grid_size; j++) {
            double x = x_min + i * dx;
            double y = y_min + j * dy;
            grid_values[i * (grid_size + 1) + j] = eval_node(equation_node, x, y);
        }
    }

    /* 第二阶段：识别等值线穿越的边并插值 */
    int point_idx = 0;
    double threshold = 0.1; /* 接近零的阈值 */

    for (int i = 0; i < grid_size && point_idx < sample_count; i++) {
        for (int j = 0; j < grid_size && point_idx < sample_count; j++) {
            /* 获取当前单元格的四个角点值 */
            double v00 = grid_values[i * (grid_size + 1) + j];
            double v10 = grid_values[(i + 1) * (grid_size + 1) + j];
            double v01 = grid_values[i * (grid_size + 1) + (j + 1)];
            double v11 = grid_values[(i + 1) * (grid_size + 1) + (j + 1)];

            /* 检查是否穿越等值线（变号） */
            bool crosses = (v00 * v10 < 0) || (v00 * v01 < 0) || (v10 * v11 < 0) || (v01 * v11 < 0);

            /* 或者值接近零 */
            bool near_zero = (fabs(v00) < threshold) || (fabs(v10) < threshold) || (fabs(v01) < threshold) ||
                             (fabs(v11) < threshold);

            if (crosses || near_zero) {
                /* 在单元格中心添加一个采样点 */
                double cx = x_min + (i + 0.5) * dx;
                double cy = y_min + (j + 0.5) * dy;

                /* 使用牛顿迭代法细化到曲线上 */
                double x = cx, y = cy;
                double f = eval_node(equation_node, x, y);

                /* 简单的梯度下降细化 */
                for (int iter = 0; iter < 10 && fabs(f) > lv_EPSILON_LOW; iter++) {
                    double h = lv_FD_GRADIENT_STEP;
                    double fx = (eval_node(equation_node, x + h, y) - f) / h;
                    double fy = (eval_node(equation_node, x, y + h) - f) / h;

                    double grad_sq = geo_norm_sq_2d(fx, fy);
                    if (grad_sq < lv_EPSILON_ULTRA)
                        break;

                    x -= f * fx / grad_sq;
                    y -= f * fy / grad_sq;
                    f = eval_node(equation_node, x, y);
                }

                result->points[point_idx].x = x;
                result->points[point_idx].y = y;
                result->points[point_idx].is_valid = true;
                point_idx++;
            }
        }
    }

    lv_free((void **) &grid_values); /* 统一内存释放器 */

    result->point_count = point_idx;
    result->success = (point_idx > 0);

    if (!result->success) {
        lv_snprintf(result->error_message, sizeof(result->error_message), "No curve points found in the specified region");
    }

    return result;
}

/**
 * @brief 辅助函数：将 AST 子树扁平化为多项式系数数组
 *
 * 将表达式 F(x,y) 展开为二维多项式，系数按字典序存储：
 *   coeffs[i] 对应 x^((i/MAX_DEG)%MAX_DEG) * y^(i%MAX_DEG)
 * 其中 MAX_DEG 为每维最大次数。
 *
 * @param[in]  node   AST 子树根节点
 * @param[out] coeffs 输出系数数组（调用者分配，至少 coeffs_size 个 double）
 * @param[in]  coeffs_size 系数数组大小
 * @param[in]  max_deg 每维最大次数
 * @return 成功返回 true，失败返回 false
 */
#define IMPLICIT_MAX_DEG 4
#define IMPLICIT_COEFFS_SIZE (IMPLICIT_MAX_DEG * IMPLICIT_MAX_DEG)

static bool flatten_to_polynomial(const FormulaNode *node, double *coeffs, int coeffs_size, int max_deg);
typedef bool (*FlattenFunc)(const FormulaNode *node, double *coeffs, int coeffs_size, int max_deg);
static bool flatten_number(const FormulaNode *node, double *coeffs, int coeffs_size, int max_deg) {
            double val;
            if (node->data.number.is_integer) {
                val = (double) node->data.number.numerator;
            } else {
                if (node->data.number.denominator == 0)
                    return false;
                val = (double) node->data.number.numerator / (double) node->data.number.denominator;
            }
            coeffs[0] = val;
            return true;
        }

static bool flatten_variable(const FormulaNode *node, double *coeffs, int coeffs_size, int max_deg) {
            if (node->data.variable.name) {
                if (lv_str_eq(node->data.variable.name, "x")) {
                    if (max_deg >= 1)
                        coeffs[1 * max_deg + 0] = 1.0; /* x^1 * y^0 */
                    return true;
                } else if (lv_str_eq(node->data.variable.name, "y")) {
                    if (max_deg >= 1)
                        coeffs[0 * max_deg + 1] = 1.0; /* x^0 * y^1 */
                    return true;
                }
            }
            /* 未知变量视为常量 0（或可扩展为参数） */
            return false;
        }

static bool flatten_b_add(const FormulaNode *node, double *coeffs, int coeffs_size, int max_deg) {
    double tmp[IMPLICIT_COEFFS_SIZE];
    double lhs[IMPLICIT_COEFFS_SIZE];
    double rhs[IMPLICIT_COEFFS_SIZE];
            if (!flatten_to_polynomial(node->data.binary_op.left, lhs, coeffs_size, max_deg))
                return false;
            if (!flatten_to_polynomial(node->data.binary_op.right, rhs, coeffs_size, max_deg))
                return false;
            for (int i = 0; i < coeffs_size; i++)
                coeffs[i] = lhs[i] + rhs[i];
            return true;
        }

static bool flatten_b_sub(const FormulaNode *node, double *coeffs, int coeffs_size, int max_deg) {
    double tmp[IMPLICIT_COEFFS_SIZE];
    double lhs[IMPLICIT_COEFFS_SIZE];
    double rhs[IMPLICIT_COEFFS_SIZE];
            if (!flatten_to_polynomial(node->data.binary_op.left, lhs, coeffs_size, max_deg))
                return false;
            if (!flatten_to_polynomial(node->data.binary_op.right, rhs, coeffs_size, max_deg))
                return false;
            for (int i = 0; i < coeffs_size; i++)
                coeffs[i] = lhs[i] - rhs[i];
            return true;
        }

static bool flatten_b_mul(const FormulaNode *node, double *coeffs, int coeffs_size, int max_deg) {
    double tmp[IMPLICIT_COEFFS_SIZE];
    double lhs[IMPLICIT_COEFFS_SIZE];
    double rhs[IMPLICIT_COEFFS_SIZE];
            if (!flatten_to_polynomial(node->data.binary_op.left, lhs, coeffs_size, max_deg))
                return false;
            if (!flatten_to_polynomial(node->data.binary_op.right, rhs, coeffs_size, max_deg))
                return false;
            /* 多项式乘法（卷积），截断到 max_deg */
            memset(coeffs, 0, sizeof(double) * coeffs_size);
            for (int i = 0; i < max_deg; i++) {
                for (int j = 0; j < max_deg; j++) {
                    int ai = i * max_deg + j;
                    for (int k = 0; k < max_deg; k++) {
                        for (int l = 0; l < max_deg; l++) {
                            int bi = k * max_deg + l;
                            int ci = (i + k) * max_deg + (j + l);
                            if (i + k < max_deg && j + l < max_deg) {
                                coeffs[ci] += lhs[ai] * rhs[bi];
                            }
                        }
                    }
                }
            }
            return true;
        }

static bool flatten_b_pow(const FormulaNode *node, double *coeffs, int coeffs_size, int max_deg) {
    double tmp[IMPLICIT_COEFFS_SIZE];
    double lhs[IMPLICIT_COEFFS_SIZE];
    double rhs[IMPLICIT_COEFFS_SIZE];
            if (!flatten_to_polynomial(node->data.binary_op.left, lhs, coeffs_size, max_deg))
                return false;
            /* 指数必须为非负整数 */
            int exp_val = 0;
            if (node->data.binary_op.right && node->data.binary_op.right->type == NODE_NUMBER) {
                if (node->data.binary_op.right->data.number.is_integer) {
                    exp_val = (int) node->data.binary_op.right->data.number.numerator;
                }
            }
            if (exp_val < 0)
                return false;

            /* 初始化结果为 1（即 x^0*y^0 = 1） */
            memset(coeffs, 0, sizeof(double) * coeffs_size);
            coeffs[0] = 1.0;

            for (int e = 0; e < exp_val; e++) {
                memset(tmp, 0, sizeof(double) * coeffs_size);
                for (int i = 0; i < max_deg; i++) {
                    for (int j = 0; j < max_deg; j++) {
                        int ai = i * max_deg + j;
                        for (int k = 0; k < max_deg; k++) {
                            for (int l = 0; l < max_deg; l++) {
                                int bi = k * max_deg + l;
                                int ci = (i + k) * max_deg + (j + l);
                                if (i + k < max_deg && j + l < max_deg) {
                                    tmp[ci] += coeffs[ai] * lhs[bi];
                                }
                            }
                        }
                    }
                }
                memcpy(coeffs, tmp, sizeof(double) * coeffs_size);
            }
            return true;
        }

static bool flatten_u_neg(const FormulaNode *node, double *coeffs, int coeffs_size, int max_deg) {
            if (!flatten_to_polynomial(node->data.unary_op.operand, coeffs, coeffs_size, max_deg))
                return false;
            for (int i = 0; i < coeffs_size; i++)
                coeffs[i] = -coeffs[i];
            return true;
        }

static bool flatten_u_sin(const FormulaNode *node, double *coeffs, int coeffs_size, int max_deg) {
    double tmp[IMPLICIT_COEFFS_SIZE];
    double lhs[IMPLICIT_COEFFS_SIZE];
    double rhs[IMPLICIT_COEFFS_SIZE];
            /* 泰勒展开 sin(x) ≈ x - x^3/3! + x^5/5!
         * 先递归展开操作数为多项式 P(x)，然后计算 P - P^3/6 + P^5/120
         * 注意：这是近似多项式，仅在小范围 |x| < pi/2 内有效 */
            if (!flatten_to_polynomial(node->data.unary_op.operand, lhs, coeffs_size, max_deg))
                return false;
            /* coeffs = P(x) */
            memcpy(coeffs, lhs, sizeof(double) * coeffs_size);

            /* 计算 P^3 / 6 */
            memset(rhs, 0, sizeof(double) * coeffs_size);
            /* P^2 = P * P */
            memset(tmp, 0, sizeof(double) * coeffs_size);
            for (int i = 0; i < max_deg; i++) {
                for (int j = 0; j < max_deg; j++) {
                    int ai = i * max_deg + j;
                    for (int k = 0; k < max_deg; k++) {
                        for (int l = 0; l < max_deg; l++) {
                            int bi = k * max_deg + l;
                            int ci = (i + k) * max_deg + (j + l);
                            if (i + k < max_deg && j + l < max_deg) {
                                tmp[ci] += lhs[ai] * lhs[bi];
                            }
                        }
                    }
                }
            }
            /* P^3 = P^2 * P */
            double p3[IMPLICIT_COEFFS_SIZE];
            memset(p3, 0, sizeof(double) * coeffs_size);
            for (int i = 0; i < max_deg; i++) {
                for (int j = 0; j < max_deg; j++) {
                    int ai = i * max_deg + j;
                    for (int k = 0; k < max_deg; k++) {
                        for (int l = 0; l < max_deg; l++) {
                            int bi = k * max_deg + l;
                            int ci = (i + k) * max_deg + (j + l);
                            if (i + k < max_deg && j + l < max_deg) {
                                p3[ci] += tmp[ai] * lhs[bi];
                            }
                        }
                    }
                }
            }
            /* 保存 P^3 的副本用于后续 P^5 计算 */
            double p3_raw[IMPLICIT_COEFFS_SIZE];
            memcpy(p3_raw, p3, sizeof(double) * coeffs_size);
            /* P^3 / 6 */
            for (int i = 0; i < coeffs_size; i++)
                p3[i] /= 6.0;
            /* coeffs = P - P^3/6 */
            for (int i = 0; i < coeffs_size; i++)
                coeffs[i] -= p3[i];

            /* 计算 P^5 / 120 */
            /* P^4 = P^3 * P (使用未除以6的 P^3) */
            double p4[IMPLICIT_COEFFS_SIZE];
            memset(p4, 0, sizeof(double) * coeffs_size);
            for (int i = 0; i < max_deg; i++) {
                for (int j = 0; j < max_deg; j++) {
                    int ai = i * max_deg + j;
                    for (int k = 0; k < max_deg; k++) {
                        for (int l = 0; l < max_deg; l++) {
                            int bi = k * max_deg + l;
                            int ci = (i + k) * max_deg + (j + l);
                            if (i + k < max_deg && j + l < max_deg) {
                                p4[ci] += p3_raw[ai] * lhs[bi];
                            }
                        }
                    }
                }
            }
            /* P^5 = P^4 * P */
            double p5[IMPLICIT_COEFFS_SIZE];
            memset(p5, 0, sizeof(double) * coeffs_size);
            for (int i = 0; i < max_deg; i++) {
                for (int j = 0; j < max_deg; j++) {
                    int ai = i * max_deg + j;
                    for (int k = 0; k < max_deg; k++) {
                        for (int l = 0; l < max_deg; l++) {
                            int bi = k * max_deg + l;
                            int ci = (i + k) * max_deg + (j + l);
                            if (i + k < max_deg && j + l < max_deg) {
                                p5[ci] += p4[ai] * lhs[bi];
                            }
                        }
                    }
                }
            }
            /* P^5 / 120 */
            for (int i = 0; i < coeffs_size; i++)
                p5[i] /= 120.0;
            /* coeffs = P - P^3/6 + P^5/120 */
            for (int i = 0; i < coeffs_size; i++)
                coeffs[i] += p5[i];

            return true;
        }

static bool flatten_u_cos(const FormulaNode *node, double *coeffs, int coeffs_size, int max_deg) {
    double tmp[IMPLICIT_COEFFS_SIZE];
    double lhs[IMPLICIT_COEFFS_SIZE];
    double rhs[IMPLICIT_COEFFS_SIZE];
            /* 泰勒展开 cos(x) ≈ 1 - x^2/2! + x^4/4!
         * 先递归展开操作数为多项式 P(x)，然后计算 1 - P^2/2 + P^4/24 */
            if (!flatten_to_polynomial(node->data.unary_op.operand, lhs, coeffs_size, max_deg))
                return false;
            /* coeffs = 1 (常数项) */
            memset(coeffs, 0, sizeof(double) * coeffs_size);
            coeffs[0] = 1.0;

            /* 计算 P^2 / 2 */
            memset(tmp, 0, sizeof(double) * coeffs_size);
            for (int i = 0; i < max_deg; i++) {
                for (int j = 0; j < max_deg; j++) {
                    int ai = i * max_deg + j;
                    for (int k = 0; k < max_deg; k++) {
                        for (int l = 0; l < max_deg; l++) {
                            int bi = k * max_deg + l;
                            int ci = (i + k) * max_deg + (j + l);
                            if (i + k < max_deg && j + l < max_deg) {
                                tmp[ci] += lhs[ai] * lhs[bi];
                            }
                        }
                    }
                }
            }
            /* coeffs = 1 - P^2/2 */
            for (int i = 0; i < coeffs_size; i++)
                coeffs[i] -= tmp[i] / 2.0;

            /* 计算 P^4 / 24 */
            /* P^3 = P^2 * P */
            double p3[IMPLICIT_COEFFS_SIZE];
            memset(p3, 0, sizeof(double) * coeffs_size);
            for (int i = 0; i < max_deg; i++) {
                for (int j = 0; j < max_deg; j++) {
                    int ai = i * max_deg + j;
                    for (int k = 0; k < max_deg; k++) {
                        for (int l = 0; l < max_deg; l++) {
                            int bi = k * max_deg + l;
                            int ci = (i + k) * max_deg + (j + l);
                            if (i + k < max_deg && j + l < max_deg) {
                                p3[ci] += tmp[ai] * lhs[bi];
                            }
                        }
                    }
                }
            }
            /* P^4 = P^3 * P */
            double p4[IMPLICIT_COEFFS_SIZE];
            memset(p4, 0, sizeof(double) * coeffs_size);
            for (int i = 0; i < max_deg; i++) {
                for (int j = 0; j < max_deg; j++) {
                    int ai = i * max_deg + j;
                    for (int k = 0; k < max_deg; k++) {
                        for (int l = 0; l < max_deg; l++) {
                            int bi = k * max_deg + l;
                            int ci = (i + k) * max_deg + (j + l);
                            if (i + k < max_deg && j + l < max_deg) {
                                p4[ci] += p3[ai] * lhs[bi];
                            }
                        }
                    }
                }
            }
            /* coeffs = 1 - P^2/2 + P^4/24 */
            for (int i = 0; i < coeffs_size; i++)
                coeffs[i] += p4[i] / 24.0;

            return true;
        }

static bool flatten_u_sqrt(const FormulaNode *node, double *coeffs, int coeffs_size, int max_deg) {
            /* 如果参数是常数，直接计算平方根 */
            const FormulaNode *operand = node->data.unary_op.operand;
            if (operand && operand->type == NODE_NUMBER) {
                double val;
                if (operand->data.number.is_integer) {
                    val = (double) operand->data.number.numerator;
                } else {
                    if (operand->data.number.denominator == 0)
                        return false;
                    val = (double) operand->data.number.numerator / (double) operand->data.number.denominator;
                }
                if (val >= 0.0) {
                    memset(coeffs, 0, sizeof(double) * coeffs_size);
                    coeffs[0] = sqrt(val);
                    return true;
                }
            }
            /* 非常数参数的 sqrt 不支持多项式展开 */
            return false;
        }

static bool flatten_to_polynomial(const FormulaNode *node, double *coeffs, int coeffs_size, int max_deg) {
    if (!node)
        return false;

    memset(coeffs, 0, sizeof(double) * coeffs_size);

    /*
     * 递归求值辅助：将 AST 节点视为多项式并累加到 coeffs 中。
     * 使用简单的递归下降方法处理 +, -, *, ^ 等运算。
     *
     * 返回值含义：
     *   0 = 成功
     *  -1 = 遇到无法识别的节点类型
     */
    /* 使用栈式迭代避免深层递归 */
    /* 这里采用递归实现，方程深度通常有限 */

    /* 辅助：将一个单项式 c * x^a * y^b 累加到 coeffs */
    /* 辅助：从 AST 节点提取多项式系数 */

    /*
     * 简化的多项式提取：
     * 只处理常见模式：
     *   - 数字常量 -> c * x^0 * y^0
     *   - 变量 x -> 1 * x^1 * y^0
     *   - 变量 y -> 1 * x^0 * y^1
     *   - a + b -> 合并
     *   - a - b -> 合并
     *   - a * b -> 卷积
     *   - a^n -> 重复卷积
     */

    /* 临时缓冲区用于中间计算 */
    double tmp[IMPLICIT_COEFFS_SIZE];
    double lhs[IMPLICIT_COEFFS_SIZE];
    double rhs[IMPLICIT_COEFFS_SIZE];

    static const FlattenFunc s_funcs[] = {
        [NODE_NUMBER] = flatten_number,
        [NODE_VARIABLE] = flatten_variable,
        [NODE_BINARY_OP_ADD] = flatten_b_add,
        [NODE_BINARY_OP_SUB] = flatten_b_sub,
        [NODE_BINARY_OP_MUL] = flatten_b_mul,
        [NODE_BINARY_OP_POW] = flatten_b_pow,
        [NODE_UNARY_OP_NEG] = flatten_u_neg,
        [NODE_UNARY_OP_SIN] = flatten_u_sin,
        [NODE_UNARY_OP_COS] = flatten_u_cos,
        [NODE_UNARY_OP_SQRT] = flatten_u_sqrt,
    };
    return LV_DISPATCH(s_funcs, node->type, false, node, coeffs, coeffs_size, max_deg);
}
/**
 * @brief 辅助函数：尝试识别圆方程 (x-a)^2 + (y-b)^2 = r^2
 *
 * 将 F(x,y) = lhs - rhs 展开为多项式，检查是否符合圆方程模式。
 * 圆方程展开后：x^2 - 2ax + a^2 + y^2 - 2by + b^2 - r^2 = 0
 * 即：x^2 + y^2 + Dx + Ey + F = 0，其中 x^2 和 y^2 系数相同且无 xy 项。
 *
 * @param[in]  coeffs 多项式系数数组
 * @param[out] cx     圆心 x 坐标
 * @param[out] cy     圆心 y 坐标
 * @param[out] r      半径
 * @return 识别为圆返回 true，否则返回 false
 */
static bool identify_circle(const double *coeffs, double *cx, double *cy, double *r) {
    /*
     * 系数索引映射（max_deg=4）：
     *   coeffs[x*4+y] 对应 x^x * y^y
     *   coeffs[0] = 常数项
     *   coeffs[4] = x 系数
     *   coeffs[1] = y 系数
     *   coeffs[8] = x^2 系数
     *   coeffs[2] = y^2 系数
     *   coeffs[5] = xy 系数
     */
    double c_xy = coeffs[1 * IMPLICIT_MAX_DEG + 1]; /* xy 项 */
    double c_x2 = coeffs[2 * IMPLICIT_MAX_DEG + 0]; /* x^2 项 */
    double c_y2 = coeffs[0 * IMPLICIT_MAX_DEG + 2]; /* y^2 项 */
    double c_x = coeffs[1 * IMPLICIT_MAX_DEG + 0];  /* x 项 */
    double c_y = coeffs[0 * IMPLICIT_MAX_DEG + 1];  /* y 项 */
    double c_0 = coeffs[0 * IMPLICIT_MAX_DEG + 0];  /* 常数项 */

    /* 检查：x^2 和 y^2 系数相同且非零，xy 系数为零 */
    if (fabs(c_x2 - c_y2) > lv_GEO_COLLINEAR_EPSILON || fabs(c_x2) < lv_GEO_COLLINEAR_EPSILON ||
        fabs(c_xy) > lv_GEO_COLLINEAR_EPSILON) {
        return false;
    }

    /* 从 x^2 + y^2 + Dx + Ey + F = 0 提取参数 */
    /* 圆心：(-D/2, -E/2)，半径^2 = D^2/4 + E^2/4 - F */
    double D = c_x / c_x2;
    double E = c_y / c_x2;
    double F = c_0 / c_x2;

    *cx = -D / 2.0;
    *cy = -E / 2.0;
    double r_sq = (D * D + E * E) / 4.0 - F;

    if (r_sq < 0)
        return false; /* 半径为虚数，不是有效的圆 */
    *r = sqrt(r_sq);
    return true;
}

/**
 * @brief 辅助函数：尝试识别直线方程 Ax + By + C = 0
 *
 * 检查多项式是否为一次方程（无 x^2, y^2, xy 等高次项）。
 *
 * @param[in]  coeffs 多项式系数数组
 * @param[out] a      x 系数
 * @param[out] b      y 系数
 * @param[out] c      常数项
 * @return 识别为直线返回 true，否则返回 false
 */
static bool identify_line(const double *coeffs, double *a, double *b, double *c) {
    /* 检查所有二次及以上项是否为零 */
    for (int i = 0; i < IMPLICIT_COEFFS_SIZE; i++) {
        int deg_x = i / IMPLICIT_MAX_DEG;
        int deg_y = i % IMPLICIT_MAX_DEG;
        if (deg_x + deg_y >= 2 && fabs(coeffs[i]) > lv_GEO_COLLINEAR_EPSILON) {
            return false;
        }
    }

    *a = coeffs[1 * IMPLICIT_MAX_DEG + 0]; /* x 系数 */
    *b = coeffs[0 * IMPLICIT_MAX_DEG + 1]; /* y 系数 */
    *c = coeffs[0 * IMPLICIT_MAX_DEG + 0]; /* 常数项 */

    /* a 和 b 不能同时为零 */
    if (fabs(*a) < lv_GEO_COLLINEAR_EPSILON && fabs(*b) < lv_GEO_COLLINEAR_EPSILON) {
        return false;
    }

    return true;
}

/**
 * 将代数方程节点添加到约束图
 * 创建隐式曲线表示
 */
bool formula_convert_equation(const FormulaNode *equation_node, ConstraintGraph *graph, int *out_node_id) {
    if (!equation_node || !graph || !out_node_id) {
        return false;
    }

    if (equation_node->type != NODE_EQUATION) {
        return false;
    }

    /*
     * 当前实现：将代数方程作为特殊的几何节点添加到图中
     * 这种节点类型可以用于后续求解和渲染
     *
     * 注意：这里我们创建一个表示隐式曲线的节点
     * 实际的几何意义是满足方程 F(x,y) = 0 的所有点集
     */

    /* 提取方程的左右两边 */
    const FormulaNode *lhs = equation_node->data.equation.lhs;
    const FormulaNode *rhs = equation_node->data.equation.rhs;

    if (!lhs) {
        return false;
    }

    /*
     * 对于简单情况，尝试识别曲线类型：
     * - 圆: (x-a)^2 + (y-b)^2 = r^2
     * - 直线: ax + by = c
     * - 抛物线: y = ax^2 + bx + c
     * - 椭圆: (x/a)^2 + (y/b)^2 = 1
     */

    /* 尝试将方程展开为多项式 F(x,y) = lhs - rhs = 0 */
    double coeffs[IMPLICIT_COEFFS_SIZE];
    bool poly_ok = false;

    if (rhs) {
        /* 计算 F = lhs - rhs */
        double lhs_coeffs[IMPLICIT_COEFFS_SIZE];
        double rhs_coeffs[IMPLICIT_COEFFS_SIZE];
        if (flatten_to_polynomial(lhs, lhs_coeffs, IMPLICIT_COEFFS_SIZE, IMPLICIT_MAX_DEG) &&
            flatten_to_polynomial(rhs, rhs_coeffs, IMPLICIT_COEFFS_SIZE, IMPLICIT_MAX_DEG)) {
            for (int i = 0; i < IMPLICIT_COEFFS_SIZE; i++) {
                coeffs[i] = lhs_coeffs[i] - rhs_coeffs[i];
            }
            poly_ok = true;
        }
    } else {
        /* F = lhs = 0 */
        poly_ok = flatten_to_polynomial(lhs, coeffs, IMPLICIT_COEFFS_SIZE, IMPLICIT_MAX_DEG);
    }

    if (poly_ok) {
        /* 尝试识别为圆方程 */
        double cx, cy, r;
        if (identify_circle(coeffs, &cx, &cy, &r)) {
            /* 创建圆心点 (cx, cy) */
            SymbolicCoord *center_coords[2];
            if (!symbolic_coord_pair_from_double_scaled(cx, cy, 1000, &center_coords[0], &center_coords[1]))
                return false;

            AddNodeResult add_result = graph_add_point(graph, center_coords, 2);
            symbolic_coord_pair_destroy(center_coords[0], center_coords[1]);

            if (add_result != ADD_NODE_OK) {
                return false;
            }

            *out_node_id = graph->next_node_id - 1;

            /* 创建圆周上的一个点表示半径 */
            SymbolicCoord *radius_coords[2];
            if (!symbolic_coord_pair_from_double_scaled(cx + r, cy, 1000, &radius_coords[0], &radius_coords[1]))
                return false;

            add_result = graph_add_point(graph, radius_coords, 2);
            symbolic_coord_pair_destroy(radius_coords[0], radius_coords[1]);

            if (add_result == ADD_NODE_OK) {
                int radius_pt_id = graph->next_node_id - 1;
                graph_add_line_segment(graph, *out_node_id, radius_pt_id);
            }

            /* 标记为圆类型 */
            GeomNode *node = graph_get_node(graph, *out_node_id);
            if (node && node->numeric_assumption_declaration) {
                lv_free((void **) &node->numeric_assumption_declaration); /* 统一内存释放器 */
                node->numeric_assumption_declaration = NULL;
            }
            if (node) {
                char buf[FORMULA_BUF_SIZE];
                int n = lv_snprintf(buf, sizeof(buf), "IMPLICIT_CURVE:CIRCLE:%.6f:%.6f:%.6f", cx, cy, r);
                /* 检查snprintf返回值：尺寸安全（CIRCLE格式最大约60字节），但防御性检查不可省略 */
                if (n < 0 || (size_t) n >= sizeof(buf)) {
                    buf[sizeof(buf) - 1] = '\0'; /* 确保零终止 */
                }
                node->numeric_assumption_declaration = lv_strdup_safe(buf);
            }

            return true;
        }

        /* 尝试识别为直线方程 Ax + By + C = 0 */
        double a, b, c;
        if (identify_line(coeffs, &a, &b, &c)) {
            /* 创建两个点表示直线 */
            SymbolicCoord *p1_coords[2];
            SymbolicCoord *p2_coords[2];

            /* exempt: 直线两端点为混合对（rational 与 double_scaled 混用），不适用 H2 pair helper */
            if (fabs(b) > lv_GEO_COLLINEAR_EPSILON) {
                /* y = (-Ax - C) / B，取 x=0 和 x=1 */
                double y0 = -c / b;
                double y1 = -(a + c) / b;
                p1_coords[0] = symbolic_coord_create_rational(0, 1);
                p1_coords[1] = symbolic_coord_from_double_scaled(y0, 1000);
                p2_coords[0] = symbolic_coord_create_rational(1000, 1000);
                p2_coords[1] = symbolic_coord_from_double_scaled(y1, 1000);
            } else {
                /* 垂直线 x = -C/A，取 y=0 和 y=1 */
                double x0 = -c / a;
                p1_coords[0] = symbolic_coord_from_double_scaled(x0, 1000);
                p1_coords[1] = symbolic_coord_create_rational(0, 1);
                p2_coords[0] = symbolic_coord_from_double_scaled(x0, 1000);
                p2_coords[1] = symbolic_coord_create_rational(1000, 1000);
            }

            AddNodeResult add_result = graph_add_point(graph, p1_coords, 2);
            symbolic_coord_pair_destroy(p1_coords[0], p1_coords[1]);

            if (add_result != ADD_NODE_OK) {
                return false;
            }
            int p1_id = graph->next_node_id - 1;

            add_result = graph_add_point(graph, p2_coords, 2);
            symbolic_coord_pair_destroy(p2_coords[0], p2_coords[1]);

            if (add_result != ADD_NODE_OK) {
                return false;
            }
            int p2_id = graph->next_node_id - 1;

            /* 创建线段 */
            graph_add_line_segment(graph, p1_id, p2_id);

            *out_node_id = p1_id;

            /* 标记为直线类型 */
            GeomNode *node = graph_get_node(graph, *out_node_id);
            if (node && node->numeric_assumption_declaration) {
                lv_free((void **) &node->numeric_assumption_declaration); /* 统一内存释放器 */
                node->numeric_assumption_declaration = NULL;
            }
            if (node) {
                char buf[FORMULA_BUF_SIZE];
                int n = lv_snprintf(buf, sizeof(buf), "IMPLICIT_CURVE:LINE:%.6f:%.6f:%.6f", a, b, c);
                /* 防御性检查：确保snprintf输出零终止 */
                if (n < 0 || (size_t) n >= sizeof(buf)) {
                    buf[sizeof(buf) - 1] = '\0';
                }
                node->numeric_assumption_declaration = lv_strdup_safe(buf);
            }

            return true;
        }
    }

    /*
     * 无法识别为特定曲线类型，回退到通用隐式曲线表示。
     * 将多项式系数存储到 numeric_assumption_declaration 中，
     * 格式为 "IMPLICIT_CURVE:degree:coeffs..."
     */
    SymbolicCoord *coords[2];
    if (!symbolic_coord_pair_create_rational(0, 1, 0, 1, &coords[0], &coords[1]))
        return false;

    AddNodeResult add_result = graph_add_point(graph, coords, 2);
    symbolic_coord_pair_destroy(coords[0], coords[1]);

    if (add_result != ADD_NODE_OK) {
        return false;
    }

    *out_node_id = graph->next_node_id - 1;

    GeomNode *node = graph_get_node(graph, *out_node_id);
    if (node) {
        if (node->numeric_assumption_declaration) {
            lv_free((void **) &node->numeric_assumption_declaration); /* 统一内存释放器 */
            node->numeric_assumption_declaration = NULL;
        }

        if (poly_ok) {
            /* 计算实际多项式次数 */
            int max_total_deg = 0;
            for (int i = 0; i < IMPLICIT_COEFFS_SIZE; i++) {
                int deg_x = i / IMPLICIT_MAX_DEG;
                int deg_y = i % IMPLICIT_MAX_DEG;
                if (fabs(coeffs[i]) > lv_EPSILON_ULTRA && (deg_x + deg_y) > max_total_deg) {
                    max_total_deg = deg_x + deg_y;
                }
            }

            /* 格式: IMPLICIT_CURVE:max_degree:c00:c01:c10:... */
            lvStrBuf sb = {0};
            lv_strbuf_printf(&sb, "IMPLICIT_CURVE:%d", max_total_deg);
            for (int i = 0; i < IMPLICIT_COEFFS_SIZE; i++) {
                if (fabs(coeffs[i]) > lv_EPSILON_ULTRA) {
                    lv_strbuf_printf(&sb, ":%d:%.10g", i, coeffs[i]);
                }
            }
            node->numeric_assumption_declaration = lv_strbuf_to_string(&sb);
        } else {
            node->numeric_assumption_declaration = lv_strdup_safe("IMPLICIT_CURVE:UNKNOWN");
        }
    }

    return true;
}
