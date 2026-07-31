/**
 * @file solver_eq_system.c
 * @brief 方程系统（初始化/添加/清空）
 *
 * @details 从 solver.c 拆分出的子模块（Lv-00 项目 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "solver_common.h"

#ifndef EQUATION_PUSH_OR_GOTO
#define EQUATION_PUSH_OR_GOTO(sys, poly, vid, ci, label)               \
    do {                                                               \
        if (equation_system_push((sys), (poly), (vid), (ci)) != 0) {   \
            lv_set_error(lv_ERROR_OUT_OF_MEMORY, "push failed (OOM)"); \
            goto label;                                                \
        }                                                              \
    } while (0)
#endif

/* ── PolyEquation + EquationSystem（类型定义见 solver_types.h）── */




/**
 * @brief 初始化方程系统
 *
 * 将方程系统结构体清零，设置初始状态（无方程、无容量）。
 *
 * @param sys 方程系统指针（必须非空）
 */
void equation_system_init(EquationSystem *sys) {
    lv_darray_init(&sys->eqs, sizeof(PolyEquation));
}

/**
 * @brief 检查 equation_system_push 返回值的辅助宏
 *
 * 当 push 失败（OOM）时，设置错误状态并跳转到指定的清理标签。
 * 用于避免在37个调用点重复相同的错误检查代码。
 */
#ifndef EQUATION_PUSH_OR_GOTO
#define EQUATION_PUSH_OR_GOTO(sys, poly, vid, ci, label)                                            \
    do {                                                                                            \
        if (equation_system_push((sys), (poly), (vid), (ci)) != 0) {                                \
            lv_set_error(lv_ERROR_OUT_OF_MEMORY, "equation_system_push: 方程添加失败（内存不足）"); \
            goto label;                                                                             \
        }                                                                                           \
    } while (0)
#endif

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
    /* 确保容量：lv_darray_reserve 内部通过 lv_ensure_capacity 管理 */
    if (!lv_darray_reserve(&sys->eqs, sys->eqs.count + 1)) {
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "equation_system_push: 扩容失败");
    }
    /* 在数组末尾就地构造 PolyEquation（避免 memcpy GMP 内部指针） */
    PolyEquation *slot = (PolyEquation *)((char *)sys->eqs.data + (size_t)sys->eqs.count * sizeof(PolyEquation));
    slot->var_node_id = var_node_id;
    slot->coord_index = coord_index;
    mpz_poly_init(&slot->poly);
    if (!mpz_poly_set(&slot->poly, &poly)) {
        mpz_poly_clear(&slot->poly);
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "equation_system_push: mpz_poly_set 失败");
    }
    sys->eqs.count++;
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
    if (!sys)
        return;
    for (int i = 0; i < sys->eqs.count; i++) {
        PolyEquation *eq = (PolyEquation *)lv_darray_get(&sys->eqs, i);
        if (eq)
            mpz_poly_clear(&eq->poly);
    }
    lv_darray_free(&sys->eqs);
}

/* ================================================================== */
/*  PUBLIC API: Equation system lifecycle                              */
/* ================================================================== */

/**
 * @brief 创建并初始化一个空的方程系统
 *
 * @return 新分配的 EquationSystem 指针，失败返回 NULL
 */
EquationSystem *equation_system_create(void) {
    EquationSystem *sys = lv_calloc(1, sizeof(EquationSystem));
    if (!sys)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "equation_system_create: lv_calloc 分配失败");
    equation_system_init(sys);
    return sys;
}

/**
 * @brief 销毁方程系统并释放所有资源
 *
 * @param sys 方程系统指针（可为 NULL，内部会检查）
 */
void equation_system_destroy(EquationSystem *sys) {
    if (!sys)
        return;
    equation_system_clear(sys);
    lv_free((void **) &sys);
}

/**
 * @brief 获取方程系统中的方程数量
 *
 * @param sys 方程系统指针（可为 NULL）
 * @return 方程数量，sys 为 NULL 时返回 0
 */
int equation_system_count(const EquationSystem *sys) {
    if (!sys)
        return 0;
    return sys->eqs.count;
}

const mpz_poly_t *equation_system_get_poly(const EquationSystem *sys, int index) {
    if (!sys)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "equation_system_get_poly: sys 为 NULL");
    if (index < 0 || index >= sys->eqs.count)
        lv_RETURN_ERROR_NULL(lv_ERROR_INDEX_OUT_OF_RANGE, "equation_system_get_poly: index=%d 越界 (count=%d)", index, sys->eqs.count);
    PolyEquation *eq = (PolyEquation *)lv_darray_get(&sys->eqs, index);
    if (!eq)
        lv_RETURN_ERROR_NULL(lv_ERROR_INTERNAL, "equation_system_get_poly: lv_darray_get 返回 NULL (index=%d)", index);
    return &eq->poly;
}

int equation_system_get_var_id(const EquationSystem *sys, int index) {
    if (!sys || index < 0 || index >= sys->eqs.count)
        return -1;
    PolyEquation *eq = (PolyEquation *)lv_darray_get(&sys->eqs, index);
    return eq ? eq->var_node_id : -1;
}

int equation_system_get_coord_index(const EquationSystem *sys, int index) {
    if (!sys || index < 0 || index >= sys->eqs.count)
        return -1;
    PolyEquation *eq = (PolyEquation *)lv_darray_get(&sys->eqs, index);
    return eq ? eq->coord_index : -1;
}
