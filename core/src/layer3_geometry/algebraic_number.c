/**
 * @file algebraic_number.c
 * @brief 代数数域封装 —— 有理数、二次代数数、区间运算、多项式系统（容器文件）
 *
 * @details 所有运算基于 int64_t，不依赖 GMP 等外部库。
 *
 *          本文件已按数域类型分层拆分为以下模块：
 *          - algebraic_number_util.c       内部整数工具：gcd/lcm/溢出检测/开方/化简
 *          - algebraic_number_rational.c   第一层：有理数域 Q
 *          - algebraic_number_quadratic.c  第二层：二次代数数域 Q(sqrt(d))
 *          - algebraic_number_interval.c   第三层：区间运算
 *          - algebraic_number_poly.c       第四层：多项式系统（简化版）
 *          - algebraic_number_io.c         跨层数域转换与误差字符串
 *
 *          共享内部工具函数声明见 algebraic_number_internal.h。
 *
 * @version 3.5.0
 * @copyright Copyright (c) 2024-2026 Lv-00 Project
 */

#include "lv/algebraic_number.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_xmacro.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "algebraic_number_internal.h"
#include "lv/lv_internal.h"
