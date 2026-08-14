/**
 * @file proof_strategy_exec.c
 * @brief 多策略证明引擎策略执行函数（从 proof_multi_strategy.c 拆分）
 *
 * @details 直接构造、面积法、Groebner 基、向量法、全角法、演绎数据库、
 *          坐标法、HOL Light、Oracle 共九种策略的执行实现。
 *          仅依赖 ProofMultiStrategy / ProofNavigator 公共接口。
 *
 *          九种策略的执行实现已按语义拆分到独立模块：
 *          - proof_strategy_core.c        直接构造法 / 面积法 / Groebner 基法
 *          - proof_strategy_vector.c      向量法
 *          - proof_strategy_angle.c       全角法
 *          - proof_strategy_deductive.c   演绎数据库法
 *          - proof_strategy_coordinate.c  坐标法
 *          - proof_strategy_hol_oracle.c  HOL Light / Oracle
 */

#include "proof_multi_strategy_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/proof.h"
#include "lv/solver.h"

#include "lv/atp_backend.h"
#include "lv/debug.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/lambda_to_graph.h"
#include "lv/lambda_unify.h"
#include "lv/normalization.h"
#include "lv/type_system.h"
#include "lv/unify.h"
