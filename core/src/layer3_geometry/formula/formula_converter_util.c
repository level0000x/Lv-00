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
#include "lv/lv_arith_safe.h"
#include "lv/lv_str_utils.h"
#include "lv/formula_converter.h"
#include "formula_converter_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/formula_renderer.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/stream.h"
#include "lv/stream.h"

/* 注：formula_converter 模块无 setter 函数（变量被 formula_converter_constraint.c 等直接 extern 引用），
 * 不适用 LV_STREAM_CTX_DEFINE 宏，保留手写。 */
lv_THREAD_LOCAL StreamContext *formula_converter_stream_ctx = NULL;

/* ============================================================
 * 变量名到节点ID映射
 * ============================================================ */

typedef struct {
    char name[MAX_NAME_LENGTH];
    int node_id;
} VarMapEntry;

/* 注意：此全局变量已使用线程本地存储，每线程独立副本。
 * 若需跨线程共享变量映射，需额外使用互斥锁保护。
 * 动态扩容（lvTlsVector：TLS 指针 + 计数 + 容量，惰性分配），消除 MAX_VAR_MAP_SIZE 定长上限。 */
static lv_THREAD_LOCAL lvTlsVector g_var_map = {0};

/**
 * @brief 根据变量名查询节点 ID
 *
 * @param var_name 变量名称
 * @return 节点 ID，未找到返回 -1
 */
int formula_get_node_id(const char *var_name) {
    if (!var_name)
        return -1;

    const VarMapEntry *arr = (const VarMapEntry *) g_var_map.ptr;
    for (int i = 0; i < g_var_map.count; i++) {
        if (lv_str_eq(arr[i].name, var_name)) {
            return arr[i].node_id;
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

    VarMapEntry *arr = (VarMapEntry *) g_var_map.ptr;
    /* 检查是否已存在 */
    for (int i = 0; i < g_var_map.count; i++) {
        if (lv_str_eq(arr[i].name, var_name)) {
            arr[i].node_id = node_id;
            return;
        }
    }

    /* 添加新条目（动态扩容，替代原 MAX_VAR_MAP_SIZE 满表静默丢弃） */
    if (!lv_tls_vector_ensure(&g_var_map, g_var_map.count + 1, sizeof(VarMapEntry)))
        return;
    /* 扩容可能 realloc，重新取指针 */
    arr = (VarMapEntry *) g_var_map.ptr;
    /* 使用 lv_strlcpy 替代不安全的 strncpy，自动保证零终止 */
    lv_strlcpy(arr[g_var_map.count].name, var_name, MAX_NAME_LENGTH);
    arr[g_var_map.count].node_id = node_id;
    g_var_map.count++;
}

/**
 * @brief 清空变量映射表（保留缓冲区供复用）
 */
void formula_clear_var_map(void) {
    lv_tls_vector_clear(&g_var_map);
}

/**
 * @brief 释放变量映射表的堆缓冲区（lv_cleanup 时调用，防 TLS 堆表泄漏）
 * @note 池化工作线程退出时无 TLS 析构钩子，其副本由线程生命周期持有；
 *       主线程副本在此函数中回收。
 */
void formula_converter_util_cleanup(void) {
    lv_tls_vector_cleanup(&g_var_map);
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
        /* 简化分数
         * 复用公共设施 lv_rational_simplify_i64：gcd 采用 uint64 安全语义，
         * INT64_MIN 的绝对值 2^63 以 (uint64_t)INT64_MAX + 1 表示，
         * 与原实现的 uint64 约分逻辑行为完全一致。
         * 解析器生成的分母恒 <= INT64_MAX，int64 中转安全。 */
        int64_t num = node->data.number.numerator;
        uint64_t denom = node->data.number.denominator;
        int64_t denom_i64 = (int64_t) denom;
        lv_rational_simplify_i64(&num, &denom_i64);

        return symbolic_coord_create_rational(num, (uint64_t) denom_i64);
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

    /* 根据节点类型生成名称（前缀表自 LV_GEOM_TYPE_ENTRY 生成，单一事实来源） */
#define LV_GEOM_PREFIX_ROW(ENUM, NAME, ALIAS, SHAPE, PREFIX, COLOR) [ENUM] = PREFIX,
    static const char *const s_node_prefixes[] = {
        LV_GEOM_TYPE_ENTRY(LV_GEOM_PREFIX_ROW)
    };
#undef LV_GEOM_PREFIX_ROW

#define NODE_PREFIX_COUNT (sizeof(s_node_prefixes) / sizeof(s_node_prefixes[0]))
    {
    const char *prefix = ((unsigned)node->type < NODE_PREFIX_COUNT && s_node_prefixes[node->type])
                             ? s_node_prefixes[node->type] : "N";
    snprintf(out_name, buf_size, "%s%d", prefix, node->id);
    }

    return true;
}
