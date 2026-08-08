/**
 * @file preset_graph_theory.c
 * @brief 图论预设函数块 - 实现
 *
 * H1 评估结论（2026-08-07）：注册样板已由 LV_PRESET_REGISTER 宏抽象（每条目 1 行），无需代码生成；运行时注册由 module/presets/graph_theory.lvz 数据驱动（convert_presets.py 生成）。
 *
 * 实现理论数学研究中常用的图论运算预设函数块。
 * 涵盖图基础、连通性、路径与环、图着色、匹配与覆盖、特殊图及图同构。
 *
 * @module GraphTheory
 * @category PRESET_CATEGORY_GRAPH_THEORY
 * @version 4.0.0
 */

#include "preset_graph_theory.h"

#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 图论模块预设函数块总数：31（与头文件中 GRAPH_THEORY_PRESET_COUNT 一致） */


/**
 * @brief 获取图论预设函数块数量
 *
 * @return int 图论模块预设函数块总数
 */
int preset_graph_theory_count(void) {
    return GRAPH_THEORY_PRESET_COUNT;
}

/**
 * @brief 获取图论预设名称列表
 */
bool preset_graph_theory_get_names(char ***out_names, int *out_count) {
    if (!out_names || !out_count)
        return false;

    char **names = (char **) lv_malloc(GRAPH_THEORY_PRESET_COUNT * sizeof(char *));
    if (!names)
        return false;

    const char *preset_names[] = {
        PRESET_GRAPH_CONSTRUCT,
        PRESET_GRAPH_ADJACENCY_MATRIX,
        PRESET_GRAPH_DEGREE_SEQUENCE,
        PRESET_GRAPH_SUBGRAPH_TEST,
        PRESET_GRAPH_COMPLEMENT,
        PRESET_GRAPH_CONNECTED_COMPONENTS,
        PRESET_GRAPH_CONNECTIVITY_TEST,
        PRESET_GRAPH_SCC,
        PRESET_GRAPH_ARTICULATION_POINTS,
        PRESET_GRAPH_BRIDGES,
        PRESET_GRAPH_SHORTEST_PATH,
        PRESET_GRAPH_MST,
        PRESET_GRAPH_EULER_PATH_TEST,
        PRESET_GRAPH_HAMILTONIAN_TEST,
        PRESET_GRAPH_CYCLE_DETECT,
        PRESET_GRAPH_CHROMATIC_NUMBER,
        PRESET_GRAPH_VERTEX_COLORING,
        PRESET_GRAPH_EDGE_COLORING,
        PRESET_GRAPH_PLANARITY_TEST,
        PRESET_GRAPH_FOUR_COLOR_VERIFY,
        PRESET_GRAPH_MAXIMUM_MATCHING,
        PRESET_GRAPH_PERFECT_MATCHING_TEST,
        PRESET_GRAPH_INDEPENDENT_SET,
        PRESET_GRAPH_VERTEX_COVER,
        PRESET_GRAPH_DOMINATING_SET,
        PRESET_GRAPH_COMPLETE,
        PRESET_GRAPH_BIPARTITE_TEST,
        PRESET_GRAPH_TREE_TEST,
        PRESET_GRAPH_DUAL,
        PRESET_GRAPH_ISOMORPHISM_TEST,
        PRESET_GRAPH_AUTOMORPHISM_GROUP,
    };

    int count = (int) (sizeof(preset_names) / sizeof(preset_names[0]));

    for (int i = 0; i < count; i++) {
        names[i] = lv_strdup(preset_names[i]);
        if (names[i] == NULL) {
            for (int j = 0; j < i; j++) {
                void *tmp = names[j];
                lv_free(&tmp);
            }
            {
                void *tmp = names;
                lv_free(&tmp);
            }
            return false;
        }
    }

    *out_names = names;
    *out_count = count;
    return true;
}

PresetCategory preset_graph_theory_category(void) {
    return PRESET_CATEGORY_GRAPH_THEORY;
}
