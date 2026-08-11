/**
 * @file recursion_measure.c
 * @brief measure system API
 * @details Split from recursion.c
 */

#include "lv/lv_platform.h"
#include "recursion.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv_internal.h"
#include "lv/lv_lifecycle.h"
#include "lv/lv_xmacro.h"
#include "lv_utils.h"
#include "stream.h"
#include "stream_context_util.h"
#include "recursion_internal.h"

/* ============== 测度系统API ============== */

/**
 * @brief 创建测度系统实例
 *
 * 分配并初始化一个 MeasureSystem 结构体，默认无测度、无非符号测度、
 * 无验证模板。
 *
 * @return 新分配的测度系统指针，失败返回 NULL
 */
MeasureSystem *measure_system_create(void) {
    MeasureSystem *ms = lv_calloc(1, sizeof(MeasureSystem));
    if (!ms)
        return NULL;

    /* lv_calloc 已零初始化所有字段，无需逐字段赋零值 */
    return ms;
}

/**
 * @brief 销毁测度系统并释放所有资源
 *
 * 逐个销毁所有注册的测度，释放测度数组、非符号测度元数据、验证模板元数据。
 *
 * @param ms 测度系统指针（可为 NULL）
 */
void measure_system_destroy(MeasureSystem *ms) {
    if (!ms)
        return;

    for (int i = 0; i < ms->measure_count; i++) {
        measure_destroy(ms->measures[i]);
    }
    lv_free((void **) &ms->measures);

    /* 释放非符号测度元数据 */
    lv_free((void **) &ms->non_symbolic_metas);

    /* 释放验证模板元数据 */
    lv_free((void **) &ms->validation_metas);

    lv_free((void **) &ms);
}

/**
 * @brief 创建符号测度
 *
 * 符号测度基于预定义种类（长度、角度、面积、深度等）和参考节点进行计算，
 * 默认为良基（well-founded）。
 *
 * @param name         测度名称（可为 NULL）
 * @param kind         测度种类（MEASURE_KIND_LENGTH 等）
 * @param ref_node_id  参考节点 ID
 * @return 新分配的测度指针，失败返回 NULL
 */
Measure *measure_create_symbolic(const char *name, int kind, int ref_node_id) {
    Measure *m = lv_calloc(1, sizeof(Measure));
    if (!m)
        return NULL;

    m->type = MEASURE_SYMBOLIC;
    m->name = name ? lv_strdup(name) : NULL;
    m->kind = kind;
    m->reference_node_id = ref_node_id;
    m->is_well_founded = true; /* 符号测度默认良基 */

    return m;
}

/**
 * @brief 创建自定义测度
 *
 * 自定义测度通过用户提供的比较函数进行非符号化比较。
 * 默认为非良基（not well-founded），需要在验证阶段通过验证模板确认良基性。
 *
 * @param name         测度名称（可为 NULL）
 * @param compare_func 比较函数指针（比较两个几何节点）
 * @param user_data    用户数据指针（传递给比较函数）
 * @return 新分配的测度指针，失败返回 NULL
 */
Measure *measure_create_custom(const char *name, int (*compare_func)(GeomNode *a, GeomNode *b, void *user_data),
                               void *user_data) {
    Measure *m = lv_calloc(1, sizeof(Measure));
    if (!m)
        return NULL;

    m->type = MEASURE_CUSTOM;
    m->name = name ? lv_strdup(name) : NULL;
    m->kind = MEASURE_KIND_CUSTOM;
    m->compare_func = compare_func;
    m->user_data = user_data;
    m->is_well_founded = false; /* 非符号测度需要验证 */

    return m;
}

/**
 * @brief 销毁测度并释放资源
 *
 * 释放测度名称字符串和测度结构体本身。
 *
 * @param m 测度指针（可为 NULL）
 */
void measure_destroy(Measure *m) {
    if (!m)
        return;

    lv_free((void **) &m->name);
    lv_free((void **) &m);
}

/**
 * @brief 向测度系统添加测度
 *
 * 将测度追加到系统的测度数组中。如果测度为自定义类型，标记系统含非符号测度。
 *
 * @param ms 测度系统指针
 * @param m  测度指针
 * @return true 添加成功，false 参数无效或内存分配失败
 */
bool measure_system_add(MeasureSystem *ms, Measure *m) {
    if (!ms || !m)
        return false;

    /* 统一扩容：复用 lv_ensure_capacity（倍增 + 溢出检查），失败时内部已设置错误 */
    if (!lv_ensure_capacity((void **) &ms->measures, ms->measure_count, &ms->measure_capacity, sizeof(Measure *), 0))
        return false;

    ms->measures[ms->measure_count] = m;
    ms->measure_count++;

    if (m->type == MEASURE_CUSTOM) {
        ms->has_non_symbolic = true;
    }

    return true;
}

/**
 * @brief 设置测度系统的默认测度
 *
 * @param ms 测度系统指针（可为 NULL）
 * @param m  默认测度指针
 */
void measure_system_set_default(MeasureSystem *ms, Measure *m) {
    if (ms)
        ms->default_measure = m;
}

/**
 * @brief 计算测度值（数值版本）
 *
 * 根据测度种类（长度/角度/面积/深度等）计算指定节点的测度值。
 * 仅支持符号测度，非符号测度返回 NULL。
 *
 * @param m     测度指针
 * @param node  几何节点指针
 * @param graph 约束图指针
 * @return 计算结果的符号坐标，失败返回 NULL
 */
/* 测度种类 → 计算函数（函数指针表分发；CUSTOM 未列出 → 走 default 返回 NULL） */
typedef SymbolicCoord *(*MeasureValueFn)(Measure *m, GeomNode *node, ConstraintGraph *graph);

/** @brief 作用域守卫清理回调：销毁 SymbolicCoord 指针变量（配合 lv_DEFER 使用） */
static void defer_symbolic_coord_destroy(void *arg) {
    symbolic_coord_destroy(*(SymbolicCoord **) arg);
}

static SymbolicCoord *measure_value_length(Measure *m, GeomNode *node, ConstraintGraph *graph) {
    (void) m;
    (void) graph;
    /*
     * 计算线段长度（返回平方距离）
     *
     * 计算过程：dx = x2 - x1, dy = y2 - y1
     *           result = dx^2 + dy^2
     *
     * 中间 SymbolicCoord 对象在创建前即注册 lv_DEFER 作用域守卫，
     * 任意一步失败直接 return，出口按注册逆序自动销毁，无需手写
     * goto cleanup 标签与同步维护的清理列表。
     */
    if (node->type == GEOM_LINE_SEGMENT && node->coord_count >= 4 && node->symbolic_coords) {
        /* 两端点坐标：(x1, y1), (x2, y2) */
        SymbolicCoord *x1 = node->symbolic_coords[0];
        SymbolicCoord *y1 = node->symbolic_coords[1];
        SymbolicCoord *x2 = node->symbolic_coords[2];
        SymbolicCoord *y2 = node->symbolic_coords[3];

        SymbolicCoord *dx = NULL;
        SymbolicCoord *dy = NULL;
        SymbolicCoord *dx2 = NULL;
        SymbolicCoord *dy2 = NULL;
        lv_DEFER(defer_symbolic_coord_destroy, &dx);
        lv_DEFER(defer_symbolic_coord_destroy, &dy);
        lv_DEFER(defer_symbolic_coord_destroy, &dx2);
        lv_DEFER(defer_symbolic_coord_destroy, &dy2);

        /* 计算距离：sqrt((x2-x1)^2 + (y2-y1)^2) */
        dx = symbolic_coord_subtract(x2, x1);
        dy = symbolic_coord_subtract(y2, y1);
        if (!dx || !dy)
            return NULL;

        dx2 = symbolic_coord_multiply(dx, dx);
        dy2 = symbolic_coord_multiply(dy, dy);
        if (!dx2 || !dy2)
            return NULL;

        SymbolicCoord *sum = symbolic_coord_add(dx2, dy2);
        if (!sum)
            return NULL;

        /* 计算成功：dx/dy/dx2/dy2 由作用域守卫自动释放，sum 返回给调用者 */
        return sum; /* 返回平方距离，避免开方 */
    }
    return NULL;
}

static SymbolicCoord *measure_value_delegate(Measure *m, GeomNode *node, ConstraintGraph *graph) {
    /* 区域面积 / 角度 —— 委托给纯符号版本 */
    return measure_compute_value_symbolic(m, node, graph);
}

static SymbolicCoord *measure_value_depth(Measure *m, GeomNode *node, ConstraintGraph *graph) {
    (void) m;
    (void) graph;
    /* 返回嵌套深度 */
    return symbolic_coord_create_rational(node->namespace_depth, 1);
}

static const MeasureValueFn kMeasureValueFns[] = {
    [MEASURE_KIND_LENGTH] = measure_value_length,
    [MEASURE_KIND_AREA]   = measure_value_delegate,
    [MEASURE_KIND_DEPTH]  = measure_value_depth,
    [MEASURE_KIND_ANGLE]  = measure_value_delegate,
};

SymbolicCoord *measure_compute_value(Measure *m, GeomNode *node, ConstraintGraph *graph) {
    if (!m || !node)
        return NULL;

    if (m->type != MEASURE_SYMBOLIC) {
        /* 非符号测度无法直接计算数值 */
        return NULL;
    }

    /* 测度种类 → 计算函数表分发（CUSTOM/越界走 default → NULL） */
    if ((unsigned) m->kind < sizeof(kMeasureValueFns) / sizeof(kMeasureValueFns[0]) &&
        kMeasureValueFns[m->kind]) {
        return kMeasureValueFns[m->kind](m, node, graph);
    }
    return NULL;
}
