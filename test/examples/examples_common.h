/**
 * @file examples_common.h
 * @brief test/examples 下示例程序的公共辅助函数
 *
 * 集中存放各示例重复使用的辅助函数（如 add_point），
 * 避免在多个示例文件中复制粘贴。
 */
#ifndef lv_EXAMPLES_COMMON_H
#define lv_EXAMPLES_COMMON_H

#include "lv.h"

/**
 * @brief 在约束图中添加一个有理数坐标点
 *
 * 使用分子/分母形式创建精确的有理数坐标点，避免浮点数带来的精度损失。
 *
 * @param g   约束图指针（不允许为 NULL）
 * @param xn  X 坐标分子
 * @param xd  X 坐标分母（不允许为 0）
 * @param yn  Y 坐标分子
 * @param yd  Y 坐标分母（不允许为 0）
 * @return    新节点的 ID（>= 0），失败返回 -1
 */
static inline int add_point(ConstraintGraph *g, int64_t xn, uint64_t xd, int64_t yn, uint64_t yd) {
    if (g == NULL || xd == 0 || yd == 0) {
        fprintf(stderr, "add_point: 无效参数\n");
        return -1;
    }

    /* 创建符号坐标 —— 使用精确有理数表示 */
    SymbolicCoord *cx = symbolic_coord_create_rational(xn, xd);
    SymbolicCoord *cy = symbolic_coord_create_rational(yn, yd);
    if (cx == NULL || cy == NULL) {
        fprintf(stderr, "add_point: 坐标创建失败\n");
        if (cx)
            symbolic_coord_destroy(cx);
        if (cy)
            symbolic_coord_destroy(cy);
        return -1;
    }

    /* 将坐标数组传入约束图，创建点节点 */
    SymbolicCoord *coords[] = {cx, cy};
    AddNodeResult res = graph_add_point(g, coords, 2);
    if (res != ADD_NODE_OK) {
        fprintf(stderr, "add_point: 添加节点失败 (错误码=%d)\n", res);
        symbolic_coord_destroy(cx);
        symbolic_coord_destroy(cy);
        return -1;
    }

    /* 返回新创建的节点 ID（next_node_id 在添加后自增） */
    return g->next_node_id - 1;
}

#endif /* lv_EXAMPLES_COMMON_H */
