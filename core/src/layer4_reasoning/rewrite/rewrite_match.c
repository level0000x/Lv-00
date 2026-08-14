/**
 * @file rewrite_match.c
 * @brief 匹配查找（WL散列 + 图快照）—— 已拆分为独立模块
 *
 * @details 原始单体实现已按职责拆分为以下模块（Lv-00 v3.3.0+）：
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

#include "lv/debug.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/mpz_poly.h"

/* ===========================================================================
 * 规则热加载/卸载
 *
 * 支持从 .lvz 格式文件动态加载重写规则，以及按名称卸载指定规则。
 * =========================================================================== */

/* .lvz 文件解析辅助结构 */
typedef struct {
    char name[256];
    int priority;
    /* 模式变量节点 ID 列表 */
    int *pattern_var_ids;
    int pattern_var_count;
    /* 模式约束：每条约束由 type + participant_count + participants 组成 */
    struct {
        ConstraintType type;
        int participant_count;
        int participants[8]; /* 最多 8 个参与者（BETWEENNESS/INTERSECTION 为 3） */
    } *pattern_constraints;
    int pattern_constraint_count;
    /* 替换约束 */
    struct {
        ConstraintType type;
        int participant_count;
        int participants[8];
    } *replacement_constraints;
    int replacement_constraint_count;
    /* 替换节点绑定 */
    struct {
        int pattern_var_id;
        int target_id;
    } *node_bindings;
    int node_binding_count;
    /* 新节点 */
    int *new_nodes;
    int new_node_count;
} LvzRewriteRule;
