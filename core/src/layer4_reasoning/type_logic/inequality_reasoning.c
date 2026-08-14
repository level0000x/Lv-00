/**
 * @file inequality_reasoning.c
 * @brief 不等式推理系统 - 真实实现
 *
 * @details 实现不等式创建/销毁、基本证明、经典不等式（AM-GM、Cauchy-Schwarz、
 * 排序不等式、Schur、Jensen、三角形不等式）、不等式变换（加减乘、传递、合并）、
 * 表达式符号判定、平方和分解、几何不等式和序列化。
 *
 * @version 3.3.0
 * @author Lv-00 Team
 */

#include "lv/inequality_reasoning.h"
#include "lv/lv_xmacro.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_utils.h"

#include "inequality_reasoning_internal.h"
