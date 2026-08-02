/**
 * @file formula_converter_util.c
 * @brief 公式转换器实现 —— 共享状态与辅助函数：流式上下文、变量映射、坐标/名称辅助
 *
 * @details 由 formula_converter.c 按功能边界拆分而来，
 *          属于公式 AST 与约束图双向转换的一部分。
 *
 * @author Lv-00 Project
 * @version 3.0.1
 */

#include "lv/lv_platform.h"
#include "formula_converter.h"
#include "formula_converter_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "formula_renderer.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "stream.h"
#include "stream_context_util.h"

lv_THREAD_LOCAL StreamContext *formula_converter_stream_ctx = NULL;

/* ============================================================
 * 变量名到节点ID映射
 * ============================================================ */

typedef struct {
    char name[MAX_NAME_LENGTH];
    int node_id;
} VarMapEntry;

/* 注意：此全局变量已使用线程本地存储，每线程独立副本。
 * 若需跨线程共享变量映射，需额外使用互斥锁保护。 */
static lv_THREAD_LOCAL VarMapEntry g_var_map[MAX_VAR_MAP_SIZE];
static lv_THREAD_LOCAL int g_var_map_count = 0;

/**
 * @brief 根据变量名查询节点 ID
 *
 * @param var_name 变量名称
 * @return 节点 ID，未找到返回 -1
 */
int formula_get_node_id(const char *var_name) {
    if (!var_name)
        return -1;

    for (int i = 0; i < g_var_map_count; i++) {
        if (strcmp(g_var_map[i].name, var_name) == 0) {
            return g_var_map[i].node_id;
        }
    }
    return -1;
}

/**
 * @brief 设置变量名到节点 ID 的映射
 *
 * @param var_name 变量名称
 * @param node_id  节点 ID
 */
void formula_set_node_id(const char *var_name, int node_id) {
    if (!var_name)
        return;

    /* 检查是否已存在 */
    for (int i = 0; i < g_var_map_count; i++) {
        if (strcmp(g_var_map[i].name, var_name) == 0) {
            g_var_map[i].node_id = node_id;
            return;
        }
    }

    /* 添加新条目 */
    if (g_var_map_count < MAX_VAR_MAP_SIZE) {
        /* 使用 lv_strlcpy 替代不安全的 strncpy，自动保证零终止 */
        lv_strlcpy(g_var_map[g_var_map_count].name, var_name, MAX_NAME_LENGTH);
        g_var_map[g_var_map_count].node_id = node_id;
        g_var_map_count++;
    }
}

/**
 * @brief 清空变量映射表
 */
void formula_clear_var_map(void) {
    g_var_map_count = 0;
}

/* ============================================================
 * 辅助函数
 * ============================================================ */

/**
 * @brief 将数值 AST 节点转换为符号坐标
 *
 * @param node 数值节点
 * @return 新分配的符号坐标数组（2 个元素：x, y），失败返回 NULL
 */
SymbolicCoord *formula_number_to_coord(const FormulaNode *node) {
    if (!node || node->type != NODE_NUMBER) {
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "invalid node or not a number");
    }

    if (node->data.number.is_integer) {
        return symbolic_coord_create_rational(node->data.number.numerator, 1);
    } else {
        /* 简化分数 */
        int64_t num = node->data.number.numerator;
        uint64_t denom = node->data.number.denominator;

        /* 计算 GCD（最大公约数）
         * 修复 INT64_MIN 取反溢出：当 num == INT64_MIN 时，
         * -num 会导致有符号整数溢出（未定义行为）。
         * 解决方案：使用 uint64_t 接收绝对值。
         * INT64_MIN 的绝对值 = -(INT64_MIN) = 2^63，
         * 在 uint64_t 中安全表示为 (uint64_t)INT64_MAX + 1。 */
        uint64_t a = (num == INT64_MIN) ? ((uint64_t) INT64_MAX + 1) : (uint64_t) (num < 0 ? -num : num);
        uint64_t b = denom;
        while (b != 0) {
            uint64_t t = b;
            b = a % b;
            a = t;
        }
        /* 当 GCD > 1 时约分分子和分母 */
        if (a > 1) {
            num /= (int64_t) a;
            denom /= a;
        }

        return symbolic_coord_create_rational(num, denom);
    }
}

SymbolicCoord **formula_coords_to_symbolic(const FormulaNode *coord_list, int *out_count) {
    if (!out_count || !coord_list || coord_list->type != NODE_COORDINATE_LIST) {
        if (out_count)
            *out_count = 0;
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "invalid coord_list or out_count");
    }

    int count = coord_list->data.coord_list.coord_count;
    SymbolicCoord **coords = (SymbolicCoord **) lv_calloc(count, sizeof(SymbolicCoord *)); /* 统一内存分配器 */
    if (!coords) {
        *out_count = 0;
        return NULL;
    }

    for (int i = 0; i < count; i++) {
        coords[i] = formula_number_to_coord(coord_list->data.coord_list.coords[i]);
        if (!coords[i]) {
            /* 创建默认坐标 0 */
            coords[i] = symbolic_coord_create_rational(0, 1);
        }
    }

    *out_count = count;
    return coords;
}

/**
 * @brief 从几何节点提取名称
 *
 * @param node     几何节点指针
 * @param out_name 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return true 成功，false 失败
 */
bool formula_node_to_name(const GeomNode *node, char *out_name, size_t buf_size) {
    if (!node || !out_name || buf_size == 0) {
        return false;
    }

    /* 根据节点类型生成名称 */
    static const char *s_node_prefixes[] = {
    [GEOM_POINT]          = "P",
    [GEOM_LINE_SEGMENT]   = "S",
    [GEOM_REGION]         = "R",
    [GEOM_CIRCLE]         = "C",
    [GEOM_PORT]           = "Port",
    [GEOM_FUNCTION_BLOCK] = "FB",
};

#define NODE_PREFIX_COUNT (sizeof(s_node_prefixes) / sizeof(s_node_prefixes[0]))
    {
    const char *prefix = ((unsigned)node->type < NODE_PREFIX_COUNT && s_node_prefixes[node->type])
                             ? s_node_prefixes[node->type] : "N";
    snprintf(out_name, buf_size, "%s%d", prefix, node->id);
    }

    return true;
}
