/**
 * @file euclidean_geometry.c
 * @brief 欧几里得几何公理体系实现 —— Hilbert 五大公理组 + Birkhoff/Tarski 等价性
 *
 * @details 完整实现 Hilbert 五大公理组的几何推理框架：
 *          - I.   关联公理（Incidence）：点与线的从属关系
 *          - II.  顺序公理（Order/Betweenness）：点在线上的顺序
 *          - III. 全等公理（Congruence）：线段/角的相等关系
 *          - IV.  平行公理（Parallel）：平行线的唯一性
 *          - V.   连续公理（Continuity）：Archimedes 公理 + 完备性
 *
 *          同时实现 Birkhoff 和 Tarski 双公理体系的翻译映射，
 *          及其等价性验证框架（EquivalenceProofChain）。
 *
 *          借鉴 mathlib4 EuclideanGeometry 的形式化设计，
 *          提供免坐标风格（SyntheticGeometry）的谓词系统，
 *          与 ConstraintGraph 紧密集成。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date 2026-05-24
 *
 * @dependencies
 *   - euclidean_geometry.h    : 公理体系公共接口
 *   - constraint_graph.h      : 约束图核心数据结构
 *   - symbolic_coord.h        : 符号坐标系统
 *   - lv_utils.h            : 统一内存分配器
 *   - lv_internal.h         : 内部常量与工具宏
 *   - error_codes.h           : 统一错误码系统
 *   - debug.h                 : 调试断言
 */

/* ========================================================================
 * 包含头文件
 * ======================================================================== */

#include "euclidean_geometry.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/lv_check.h"

#include "debug.h"
#include "error_codes.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "symbolic_coord.h"
#include "euclidean_geometry_internal.h"

/* ========================================================================
 * 模块级常量在 euclidean_geometry_internal.h 中定义
 * ======================================================================== */

/* ========================================================================
 * 公理体系名称的字符串映射（调试用）
 * ======================================================================== */

static const char *euclidean_axiom_system_names[] = {"Birkhoff", "Tarski", "Hilbert", "Custom"};

static const char *euclidean_axiom_group_names[] = {"Incidence", "Order", "Congruence", "Parallel", "Continuity"};

#include "euclidean_geometry_context.c"
#include "euclidean_geometry_axiom.c"
#include "euclidean_geometry_declare.c"
#include "euclidean_geometry_predicate.c"
#include "euclidean_geometry_consistency.c"
#include "euclidean_geometry_export.c"
#include "euclidean_geometry_equivalence.c"
#include "euclidean_geometry_helpers.c"

/* ========================================================================
 * 拆分说明
 * ========================================================================
 *
 * 本文件已按功能域拆分为以下模块：
 * - euclidean_geometry_context.c                  上下文生命周期管理
 * - euclidean_geometry_axiom.c                    公理体系配置
 * - euclidean_geometry_declare.c                  几何实体声明
 * - euclidean_geometry_predicate.c                几何谓词断言
 * - euclidean_geometry_consistency.c              定理验证与一致性检查
 * - euclidean_geometry_export.c                   导出
 * - euclidean_geometry_equivalence.c              等价性证明框架
 * - euclidean_geometry_helpers.c                  内部辅助函数
 * ======================================================================== */
