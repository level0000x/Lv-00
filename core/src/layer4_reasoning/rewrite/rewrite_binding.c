/**
 * @file rewrite_binding.c
 * @brief 重写规则：匹配绑定解析与辅助工具
 *
 * 从 rewrite_match.c 拆分的模块之一：
 *   - rewrite_binding.c      匹配绑定解析与辅助工具
 *   - rewrite_snapshot.c     图快照（事务回滚）
 *   - rewrite_hash.c         图结构哈希与 WL 哈希
 *   - rewrite_rule.c         重写规则创建/销毁
 *   - rewrite_match_search.c 匹配查找与多匹配选择
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/rewrite.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "mpz_poly.h"

/* graph_index.c 实现：按约束类型分发到 typed graph_add_*（收敛三处平行分发） */
AddConstraintResult graph_add_constraint_dispatch(ConstraintGraph *graph, ConstraintType type,
                                                  const int *participants, int count, double numeric_value);
/**
 * @brief 从匹配绑定表中解析模式变量对应的实际图节点 ID
 *
 * 模式变量（负 ID）通过匹配绑定表映射到约束图中的实际节点。
 * 遍历绑定表查找匹配项，未找到返回 -1。
 *
 * @param bindings      节点绑定数组（[pattern_id, actual_id] 交错）
 * @param binding_count 绑定对数量
 * @param pattern_var_id 待解析的模式变量 ID（负值）
 * @return 对应的实际图节点 ID，未找到返回 -1
 */
int resolve_binding(const int *bindings, int binding_count, int pattern_var_id) {
    for (int i = 0; i < binding_count; i++) {
        if (bindings[i * 2] == pattern_var_id) {
            return bindings[i * 2 + 1];
        }
    }
    return -1;
}

/**
 * @brief 检查模式变量是否在替换约束中被引用
 *
 * 遍历替换约束数组中的每个约束及其参与者，
 * 判断给定的模式变量 ID 是否出现在替换中。
 *
 * @param repl            替换规则描述
 * @param pattern_var_id  待检查的模式变量 ID
 * @return true 如果该变量在替换约束中被引用，否则 false
 */
bool pattern_var_used_in_replacement(const RewriteReplacement *repl, int pattern_var_id) {
    for (int c = 0; c < repl->replacement_constraint_count; c++) {
        Constraint *rc = repl->replacement_constraints[c];
        for (int p = 0; p < rc->participant_count; p++) {
            if (rc->participants[p] == pattern_var_id) {
                return true;
            }
        }
    }
    return false;
}

/**
 * @brief 检查模式变量是否出现在替换的节点绑定表中
 *
 * 在替换的 node_bindings 表中搜索给定的模式变量 ID。
 * node_bindings 定义了替换后如何重新映射模式变量到新节点。
 *
 * @param repl            替换规则描述
 * @param pattern_var_id  待检查的模式变量 ID
 * @return true 如果该变量在绑定表中，否则 false
 */
bool pattern_var_in_replacement_bindings(const RewriteReplacement *repl, int pattern_var_id) {
    for (int b = 0; b < repl->binding_count; b++) {
        if (repl->node_bindings[b][0] == pattern_var_id) {
            return true;
        }
    }
    return false;
}

/**
 * @brief 解析替换约束中的参与者 ID 为实际图节点 ID
 *
 * 根据参与者 ID 的类型进行不同处理：
 * - 负值（模式变量）：在匹配绑定表中查找
 * - 正值但在 new_nodes 列表中：使用新创建节点的映射
 * - 正值且不在模式变量中：外部/边界节点，保持不变
 *
 * @param participant_id    参与者 ID
 * @param match_bindings    匹配绑定数组
 * @param match_binding_count 绑定数量
 * @param new_node_map      新节点映射数组
 * @param new_node_map_count 映射数量
 * @param new_nodes         新节点 ID 数组
 * @param new_node_count    新节点数量
 * @return 解析后的实际图节点 ID，失败返回 -1
 */
int resolve_replacement_participant(int participant_id, const int *match_bindings, int match_binding_count,
                                    const int *new_node_map, /* 将替换 new_node 索引映射到实际图节点 ID */
                                    int new_node_map_count, const int *new_nodes, /* replacement->new_nodes 数组 */
                                    int new_node_count) {
    if (participant_id < 0) {
        /* 模式变量：在匹配绑定表中查找 */
        return resolve_binding(match_bindings, match_binding_count, participant_id);
    }

    /* 检查是否是替换中的新节点引用。
       替换中的新节点通过 new_nodes 数组中的位置标识，
       替换约束通过 new_nodes[i] 中存储的相同值来引用它们。 */
    for (int i = 0; i < new_node_count; i++) {
        if (new_nodes[i] == participant_id) {
            /* 映射到新创建的图节点 ID */
            if (i < new_node_map_count) {
                return new_node_map[i];
            }
            return -1;
        }
    }

    /* 外部/边界节点：保持 ID 不变 */
    return participant_id;
}

/**
 * @brief 向约束图中添加通用约束
 *
 * 根据约束类型和已解析的参与者 ID 数组，
 * 经 graph_add_constraint_dispatch 统一分发到对应的 graph_add_* 函数。
 *
 * @param graph             目标约束图
 * @param type              约束类型
 * @param participants      参与者 ID 数组（已解析到实际图节点）
 * @param participant_count 参与者数量
 * @return true 添加成功，false 失败（类型不支持或参数不匹配）
 */
bool add_constraint_generic(ConstraintGraph *graph, ConstraintType type, const int *participants,
                            int participant_count) {
    return graph_add_constraint_dispatch(graph, type, participants, participant_count, 0.0) == ADD_CONSTRAINT_OK;
}

/**
 * @brief 检查约束 ID 是否在匹配的约束绑定列表中
 *
 * 遍历匹配对象中的 constraint_bindings 数组，
 * 判断给定约束是否已被匹配覆盖。
 *
 * @param match         重写匹配对象
 * @param constraint_id 待检查的约束 ID
 * @return true 如果该约束已被匹配，否则 false
 */
/**
 * @brief 检查节点 ID 是否已绑定到匹配中的某个模式变量
 *
 * 在匹配的 node_bindings 中查找该节点 ID。node_bindings
 * 以 [pattern_id, actual_id] 交错存储，此处匹配第二个元素。
 *
 * @param match   重写匹配对象
 * @param node_id 待检查的实际图节点 ID
 * @return true 如果该节点已被模式变量绑定，否则 false
 */
static bool is_pattern_bound_node(const RewriteMatch *match, int node_id) {
    for (int i = 0; i < match->binding_count; i++) {
        if (match->node_bindings[i * 2 + 1] == node_id) {
            return true;
        }
    }
    return false;
}

/**
 * @brief 检查约束 ID 是否在匹配的约束绑定列表中
 *
 * 遍历匹配对象中的约束绑定数组，判断给定约束是否已被匹配覆盖。
 *
 * @param match         重写匹配对象
 * @param constraint_id 待检查的约束 ID
 * @return true 如果该约束已被匹配，否则 false
 */
bool is_matched_constraint(const RewriteMatch *match, int constraint_id) {
    for (int i = 0; i < match->binding_count; i++) {
        if (match->constraint_bindings[i] == constraint_id) {
            return true;
        }
    }
    return false;
}

/**
 * @brief 快速一致性检查：在重写后检测明显的冲突
 *
 * 调用 graph_detect_conflicts 检测约束图中的冲突。
 *
 * @param graph 约束图指针
 * @return true 表示图看起来一致（无冲突），false 表示检测到冲突
 * @note graph_detect_conflicts 返回的 conflicts 数组和 conflict_sizes 数组
 *       需要分别释放；两者可能为 NULL（无冲突或分配失败时）。
 *       此函数不区分"无冲突"和"分配失败"两种情况，均视为通过。
 */
bool check_graph_consistency(ConstraintGraph *graph) {
    int conflict_count = 0;
    int *conflict_sizes = NULL;
    int **conflicts = graph_detect_conflicts(graph, &conflict_count, &conflict_sizes);
    if (conflicts && conflict_count > 0) {
        /* 存在冲突：释放冲突数组并返回 false */
        if (conflict_sizes)
            lv_free((void **) &conflict_sizes);
        if (conflicts)
            lv_free((void **) &conflicts);
        return false;
    }
    /* 无冲突或 conflicts 为 NULL（分配失败）：释放资源并返回 true */
    if (conflict_sizes)
        lv_free((void **) &conflict_sizes);
    if (conflicts)
        lv_free((void **) &conflicts);
    return true;
}
