/**
 * @file solver_eq_system.c
 * @brief 方程系统（初始化/添加/清空）
 *
 * @details 从 solver.c 拆分出的子模块（Lv-00 项目 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv00/solver.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00/constraint_graph.h"
#include "debug.h"
#include "lv00_internal.h"
#include "lv00_utils.h"
#include "mpz_poly.h"
#include "lv00/stream.h"
#include "stream_context_util.h"

/* --- 共享宏 --- */
#define LV00_SOLVER_DYNARRAY_INIT_CAP 16
#define LV00_SOLVER_LINEAR_COEFF_COUNT 2
#define LV00_SOLVER_QUADRATIC_COEFF_COUNT 3
#define LV00_ZERO_EPSILON 1e-12
#define SOLVER_DETAIL_BUF_SIZE 512
#define EQUATION_PUSH_OR_GOTO(sys, poly, vid, ci, label) \
    do { \
        if (equation_system_push((sys), (poly), (vid), (ci)) != 0) { \
            lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "push failed (OOM)"); \
            goto label; \
        } \
    } while (0)

/* ── PolyEquation + EquationSystem ── */

typedef struct {
    mpz_poly_t poly;
    int var_node_id;
    int coord_index;
} PolyEquation;

typedef struct EquationSystem {
    PolyEquation *eqs;
    int count;
    int capacity;
} EquationSystem;


LV00_DECLARE_STREAM_CTX(solver);

/**
 * @brief 初始化方程系统
 *
 * 将方程系统结构体清零，设置初始状态（无方程、无容量）。
 *
 * @param sys 方程系统指针（必须非空）
 */
void equation_system_init(EquationSystem *sys) {
    sys->eqs = NULL;
    sys->count = 0;
    sys->capacity = 0;
}

/**
 * @brief 检查 equation_system_push 返回值的辅助宏
 *
 * 当 push 失败（OOM）时，设置错误状态并跳转到指定的清理标签。
 * 用于避免在37个调用点重复相同的错误检查代码。
 */
#define EQUATION_PUSH_OR_GOTO(sys, poly, vid, ci, label) \
    do { \
        if (equation_system_push((sys), (poly), (vid), (ci)) != 0) { \
            lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "equation_system_push: 方程添加失败（内存不足）"); \
            goto label; \
        } \
    } while (0)

/**
 * @brief 向方程系统中添加一个多项式方程
 *
 * 如果当前容量不足，先扩容再插入。使用临时变量保存新容量，
 * 确保 realloc 失败时不会破坏原有状态。
 *
 * @param sys        方程系统指针
 * @param poly       多项式（通过值拷贝，内部会复制）
 * @param var_node_id 变量对应的几何节点 ID
 * @param coord_index 坐标索引（0 = x, 1 = y）
 * @return 0 表示成功，-1 表示失败（内存不足或容量溢出）
 */
int equation_system_push(EquationSystem *sys, mpz_poly_t poly, int var_node_id, int coord_index) {
    if (sys->count >= sys->capacity) {
        if (sys->capacity > INT_MAX / 2)
            return -1;
        /* 使用临时变量计算新容量，避免 realloc 失败时 capacity 已被修改 */
        int new_capacity = sys->capacity == 0 ? LV00_SOLVER_DYNARRAY_INIT_CAP : sys->capacity * LV00_ARRAY_GROWTH_FACTOR;
        /* 溢出检查：确保 new_capacity * sizeof(PolyEquation) 不超过 SIZE_MAX */
        if ((size_t)new_capacity > SIZE_MAX / sizeof(PolyEquation)) {
            return -1;
        }
        PolyEquation *new_eqs = lv00_realloc(sys->eqs, (size_t)new_capacity * sizeof(PolyEquation));
        if (!new_eqs) {
            lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "equation_system_push: 扩容失败");
            /* 注意：poly 按值传入（结构体副本，内部 coeffs 指针与调用者共享），
             * 不能在此处调用 mpz_poly_clear(&poly)，否则会导致调用者的
             * poly.coeffs 变成悬空指针。poly 的资源由调用者负责清理。 */
            return -1;
        }
        sys->eqs = new_eqs;
        sys->capacity = new_capacity; /* 仅在 realloc 成功后才更新容量 */
    }
    sys->eqs[sys->count].var_node_id = var_node_id;
    sys->eqs[sys->count].coord_index = coord_index;
    mpz_poly_init(&sys->eqs[sys->count].poly);
    if (!mpz_poly_set(&sys->eqs[sys->count].poly, &poly)) {
        mpz_poly_clear(&sys->eqs[sys->count].poly);
        lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "equation_system_push: mpz_poly_set 失败");
        return -1;
    }
    sys->count++;
    return 0;
}

/**
 * @brief 清空并释放方程系统内的所有资源
 *
 * 逐个清除每个多项式方程，释放方程数组，并将系统重置为空状态。
 *
 * @param sys 方程系统指针
 */
void equation_system_clear(EquationSystem *sys) {
    if (!sys) return;
    for (int i = 0; i < sys->count; i++) {
        mpz_poly_clear(&sys->eqs[i].poly);
    }
    lv00_free((void **) &sys->eqs);
    sys->eqs = NULL;
    sys->count = 0;
    sys->capacity = 0;
}
