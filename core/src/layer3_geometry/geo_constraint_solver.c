/**
 * @file geo_constraint_solver.c
 * @brief 几何约束求解器实现 —— Newton-Raphson 迭代求解
 *
 * 借鉴 SolveSpace (github.com/solvespace/solvespace) 的核心求解架构：
 *   - 雅可比矩阵数值差分构建
 *   - 高斯消元法求解线性方程组
 *   - 阻尼 Newton-Raphson 迭代
 *   - DOF 自由度分析
 *
 * 代码质量优化（v3.6.1）：
 *   - 添加 ID 到索引的哈希映射，将查找复杂度从 O(n) 优化到 O(1)
 *   - 修复内存分配错误处理
 *   - 添加详细的函数注释
 *
 * @version v3.6.1
 */

#include "lv/lv_platform.h"
#include "lv/lv_internal.h"

#include "lv/geo_constraint_solver.h"

#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lv/config.h"

#include "lv_utils.h"

/* ========================================================================
 * lv_PUBLIC_API 兼容处理
 * ======================================================================== */

#ifndef lv_PUBLIC_API
#define lv_PUBLIC_API
#endif


#include "geo_constraint_solver_internal.h"
