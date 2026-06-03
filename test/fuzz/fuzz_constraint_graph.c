/**
 * @file fuzz_constraint_graph.c
 * @brief 约束图模糊测试 - 使用 libFuzzer
 *
 * 测试目标：
 * - 随机生成约束图结构
 * - 测试归一化的鲁棒性
 * - 发现内存安全问题
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "lv00.h"

/**
 * @brief libFuzzer 入口点：约束图模糊测试
 *
 * 使用 libFuzzer 提供的随机输入数据测试约束图模块的鲁棒性。
 * 根据输入数据动态创建随机数量的点和线段，添加 betweenness 约束，
 * 然后执行归一化操作，用于发现内存安全问题和崩溃缺陷。
 *
 * @param data 模糊测试输入数据指针
 * @param size 输入数据的字节长度，至少需要 8 字节
 * @return 固定返回 0（libFuzzer 约定）
 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 8)
        return 0;

    /* [P1 修复] 删除未使用的 seed 变量，原代码计算了 seed 但从未使用 */
    /* 如需引入随机性，应使用 data 数组直接驱动（当前已通过 data[] 实现） */

    /* 创建约束图 */
    ConstraintGraph *g = graph_create();
    if (!g)
        return 0;

    /* 根据输入数据决定创建多少节点 (1-20个) */
    int num_nodes = (data[4] % 20) + 1;
    int point_ids[20] = {0};
    int point_count = 0;

    /* 创建随机点 */
    for (int i = 0; i < num_nodes && i < (int) size / 2; i++) {
        /* 使用输入数据作为坐标 */
        int64_t xn = (int64_t) (data[i * 2] % 100);
        int64_t yn = (int64_t) (data[i * 2 + 1] % 100);

        SymbolicCoord *cx = symbolic_coord_create_rational(xn, 1);
        SymbolicCoord *cy = symbolic_coord_create_rational(yn, 1);
        if (!cx || !cy)
            continue;

        SymbolicCoord *coords[] = {cx, cy};
        AddNodeResult result = graph_add_point(g, coords, 2);

        if (result == ADD_NODE_OK && point_count < 20) {
            point_ids[point_count++] = g->next_node_id - 1;
        }
    }

    /* 如果至少有2个点，创建一些线段 */
    if (point_count >= 2) {
        for (int i = 0; i < point_count - 1 && i < 10; i++) {
            graph_add_line_segment(g, point_ids[i], point_ids[i + 1]);
        }
    }

    /* 如果至少有3个点，添加一些约束 */
    if (point_count >= 3) {
        for (int i = 0; i < point_count - 2 && i < 5; i++) {
            graph_add_betweenness(g, point_ids[i], point_ids[i + 1], point_ids[i + 2]);
        }
    }

    /* 测试归一化 - 这是最容易发现bug的地方 */
    NormalizationResult *norm = graph_normalize(g, false);
    if (norm) {
        normalization_result_destroy(norm);
    }

    /* 清理 */
    graph_destroy(g);

    return 0;
}
