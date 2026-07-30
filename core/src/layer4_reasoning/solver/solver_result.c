/**
 * @file solver_result.c
 * @brief 求解结果处理（GroebnerResult 生命周期）
 *
 * @details 从 solver.c 拆分出的子模块（Lv-00 项目 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "solver_common.h"
#include "lv/symbolic_coord.h"

/**
 * @brief 销毁 GroebnerResult 并释放所有资源
 */
void groebner_result_destroy(GroebnerResult *result) {
    if (!result)
        return;
    if (result->solutions) {
        for (int i = 0; i < result->solution_count; i++) {
            symbolic_coord_destroy(result->solutions[i]);
        }
        lv_free((void **) &result->solutions);
    }
    lv_free((void **) &result);
}
