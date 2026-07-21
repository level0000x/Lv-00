/* ========================================================================
 * 交互式类型等价探索器引擎实现
 *
 * 核心算法：BFS 双向搜索
 *   1. 将左右两侧原始类型入队
 *   2. 每步出队一个节点，检查左右两侧是否通过 type_check_equivalence 等价
 *   3. 若等价 → 回溯路径，返回成功
 *   4. 若不等价 → 对所有匹配左/右类型种类的规则各生成一个后继节点入队
 *   5. 重复直到找到等价或达到步数/节点上限
 *
 * 规则匹配：仅基于类型种类（TypeKind）——若规则的 pattern.kind 等于
 * 当前类型区域的 kind，则认为该规则可应用。应用规则时通过
 * type_normalize 变换类型结构。
 *
 * ======================================================================== */

#include "lv00/type_equiv_explorer.h"
#include "lv00/rewrite.h"

#include <stdlib.h>
#include <string.h>

/* ---- 内部常量 --------------------------------------------------------- */

#define INITIAL_QUEUE_CAPACITY 64
#define DEFAULT_MAX_DEPTH      20
#define DEFAULT_MAX_NODES      10000

/* ---- 内部辅助 --------------------------------------------------------- */

/**
 * @brief 深拷贝一个 TypeRegion
 *
 * 封装 type_region_deep_copy（若存在）或回退到简单字段拷贝。
 */
static TypeRegion *type_copy(const TypeRegion *src)
{
    if (!src) return NULL;
    /* 使用 type_system.c 中定义的内部深拷贝 */
    extern TypeRegion *type_region_deep_copy(const TypeRegion *src);
    return type_region_deep_copy(src);
}

/**
 * @brief 释放深拷贝的 TypeRegion
 */
static void type_free(TypeRegion *tr)
{
    if (!tr) return;
    extern void type_region_deep_free(TypeRegion *tr);
    type_region_deep_free(tr);
}

/**
 * @brief 检查某条重写规则是否匹配给定类型
 *
 * 匹配条件：规则的 pattern.kind 等于类型的 kind，
 *          或规则模式为类型变量（匹配一切）。
 */
static bool rule_matches_type(const RewriteRule *rule, const TypeRegion *type)
{
    if (!rule || !rule->pattern || !type) return false;
    if (rule->pattern->kind == TYPE_KIND_VARIABLE) return true;
    return rule->pattern->kind == type->kind;
}

/**
 * @brief 应用指定的重写规则到类型区域
 *
 * 策略：尝试 type_normalize 变换类型结构。
 * 成功时 inout_type 被原地修改。
 *
 * @param ts         类型系统
 * @param inout_type 要变换的类型区域（原地修改）
 * @return true 变换成功
 */
static bool apply_rule_to_type(TypeSystem *ts, TypeRegion *inout_type)
{
    if (!ts || !inout_type) return false;

    /* 使用 type_normalize 尝试规范化类型 */
    extern bool type_normalize(TypeSystem *ts, TypeRegion *type, TypeRegion **out_normalized);
    TypeRegion *normalized = NULL;
    if (type_normalize(ts, inout_type, &normalized)) {
        if (normalized && normalized != inout_type) {
            /* 用规范化结果替换 */
            type_free(inout_type);
            /* 将 normalized 的内容移到 inout_type 的位置 */
            memcpy(inout_type, normalized, sizeof(TypeRegion));
            free(normalized);
        }
        return true;
    }
    return false;
}

/**
 * @brief 重建从根到解节点的路径
 *
 * @param explorer  探索器
 * @param node_idx  解节点在队列中的索引
 */
static void reconstruct_path(TypeEquivExplorer *explorer, int node_idx)
{
    /* 从解节点回溯到根，收集步骤 */
    int max_steps = explorer->queue[node_idx].depth + 1;
    int *reverse_path = lv00_malloc((size_t)max_steps * sizeof(int));
    int *reverse_side = lv00_malloc((size_t)max_steps * sizeof(int));
    char **reverse_names = lv00_malloc((size_t)max_steps * sizeof(char *));
    if (!reverse_path || !reverse_side || !reverse_names) {
        free(reverse_path);
        free(reverse_side);
        free(reverse_names);
        return;
    }

    int step = 0;
    int cur = node_idx;
    while (cur >= 0 && step < max_steps) {
        TypeEquivNode *node = &explorer->queue[cur];
        reverse_path[step] = node->applied_rule_index;
        reverse_side[step] = node->applied_to_left ? 0 : 1;
        reverse_names[step] = node->applied_rule_name;
        cur = node->parent_index;
        step++;
    }

    /* 从根到解正向构建 TypeRewritePath */
    TypeRewritePath *path = type_rewrite_path_create();
    if (!path) {
        free(reverse_path);
        free(reverse_side);
        free(reverse_names);
        return;
    }

    for (int i = step - 1; i >= 0; i--) {
        if (reverse_path[i] < 0) continue;
        /* 记录步骤：我们简化记录规则名称 */
        type_rewrite_path_record(path, reverse_names[i],
                                 NULL, NULL);
    }

    explorer->proved_path = path;
    explorer->found_equivalence = true;
    explorer->solution_node_index = node_idx;

    free(reverse_path);
    free(reverse_side);
    free(reverse_names);
}

/* ---- 公共 API ---------------------------------------------------------- */

TypeEquivExplorer *type_equiv_explore_create(TypeSystem *ts,
                                              const TypeRegion *left,
                                              const TypeRegion *right)
{
    if (!ts || !left || !right) return NULL;

    TypeEquivExplorer *exp = lv00_calloc(1, sizeof(TypeEquivExplorer));
    if (!exp) return NULL;

    exp->ts = ts;
    exp->left_original = type_copy(left);
    exp->right_original = type_copy(right);
    if (!exp->left_original || !exp->right_original) {
        type_free(exp->left_original);
        type_free(exp->right_original);
        free(exp);
        return NULL;
    }

    exp->queue_capacity = INITIAL_QUEUE_CAPACITY;
    exp->queue = lv00_calloc((size_t)exp->queue_capacity, sizeof(TypeEquivNode *));
    if (!exp->queue) {
        type_free(exp->left_original);
        type_free(exp->right_original);
        free(exp);
        return NULL;
    }
    exp->queue_head = 0;
    exp->queue_tail = 0;
    exp->max_depth = DEFAULT_MAX_DEPTH;
    exp->max_nodes = DEFAULT_MAX_NODES;
    exp->nodes_explored = 0;
    exp->found_equivalence = false;
    exp->solution_node_index = -1;
    exp->proved_path = NULL;
    exp->exhausted = false;

    /* 创建根节点并入队 */
    TypeEquivNode *root = lv00_calloc(1, sizeof(TypeEquivNode));
    if (!root) {
        type_equiv_explore_destroy(exp);
        return NULL;
    }
    root->left = type_copy(left);
    root->right = type_copy(right);
    root->depth = 0;
    root->parent_index = -1;
    root->applied_rule_index = -1;
    root->applied_to_left = false;
    snprintf(root->applied_rule_name, sizeof(root->applied_rule_name), "(root)");

    exp->queue[exp->queue_tail++] = root;

    return exp;
}

bool type_equiv_explore_search(TypeEquivExplorer *explorer, int max_steps)
{
    if (!explorer) return false;

    int steps = 0;
    while (explorer->queue_head < explorer->queue_tail && steps < max_steps) {
        /* 检查节点上限 */
        if (explorer->nodes_explored >= explorer->max_nodes) {
            explorer->exhausted = true;
            return false;
        }

        TypeEquivNode *node = explorer->queue[explorer->queue_head++];
        explorer->nodes_explored++;

        /* 检查深度限制 */
        if (node->depth > explorer->max_depth) {
            type_free(node->left);
            type_free(node->right);
            free(node);
            continue;
        }

        /* 检查当前左右两侧是否等价 */
        TypeEquivResult eq = type_check_equivalence(explorer->ts,
                                                     node->left, node->right, true);
        if (eq == TYPE_EQUIV_OK) {
            /* 找到等价！重建路径 */
            reconstruct_path(explorer, (int)(explorer->queue_head - 1));
            type_free(node->left);
            type_free(node->right);
            free(node);
            return true;
        }

        /* 生成后继：对左侧应用规则 */
        for (int r = 0; r < explorer->ts->rewrite_rule_count; r++) {
            RewriteRule *rule = explorer->ts->rewrite_rules[r];
            if (!rule || !rule->name) continue;
            if (!rule_matches_type(rule, node->left)) continue;

            /* 扩展队列容量 */
            if (explorer->queue_tail >= explorer->queue_capacity) {
                int new_cap = explorer->queue_capacity * 2;
                TypeEquivNode **new_q = lv00_realloc(explorer->queue,
                    (size_t)new_cap * sizeof(TypeEquivNode *));
                if (!new_q) {
                    explorer->exhausted = true;
                    type_free(node->left);
                    type_free(node->right);
                    free(node);
                    return false;
                }
                explorer->queue = new_q;
                explorer->queue_capacity = new_cap;
            }

            TypeEquivNode *child = lv00_calloc(1, sizeof(TypeEquivNode));
            if (!child) continue;

            child->left = type_copy(node->left);
            child->right = type_copy(node->right);
            child->depth = node->depth + 1;
            child->parent_index = (int)(explorer->queue_head - 1);
            child->applied_rule_index = r;
            child->applied_to_left = true;
            snprintf(child->applied_rule_name, sizeof(child->applied_rule_name),
                     "%s", rule->name);

            /* 对左侧应用规则 */
            (void)apply_rule_to_type(explorer->ts, child->left);
            explorer->queue[explorer->queue_tail++] = child;
            steps++;
            if (steps >= max_steps) break;
        }

        if (steps >= max_steps) {
            type_free(node->left);
            type_free(node->right);
            free(node);
            break;
        }

        /* 生成后继：对右侧应用规则 */
        for (int r = 0; r < explorer->ts->rewrite_rule_count; r++) {
            RewriteRule *rule = explorer->ts->rewrite_rules[r];
            if (!rule || !rule->name) continue;
            if (!rule_matches_type(rule, node->right)) continue;

            /* 扩展队列容量 */
            if (explorer->queue_tail >= explorer->queue_capacity) {
                int new_cap = explorer->queue_capacity * 2;
                TypeEquivNode **new_q = lv00_realloc(explorer->queue,
                    (size_t)new_cap * sizeof(TypeEquivNode *));
                if (!new_q) {
                    explorer->exhausted = true;
                    type_free(node->left);
                    type_free(node->right);
                    free(node);
                    return false;
                }
                explorer->queue = new_q;
                explorer->queue_capacity = new_cap;
            }

            TypeEquivNode *child = lv00_calloc(1, sizeof(TypeEquivNode));
            if (!child) continue;

            child->left = type_copy(node->left);
            child->right = type_copy(node->right);
            child->depth = node->depth + 1;
            child->parent_index = (int)(explorer->queue_head - 1);
            child->applied_rule_index = r;
            child->applied_to_left = false;
            snprintf(child->applied_rule_name, sizeof(child->applied_rule_name),
                     "%s", rule->name);

            /* 对右侧应用规则 */
            (void)apply_rule_to_type(explorer->ts, child->right);
            explorer->queue[explorer->queue_tail++] = child;
            steps++;
            if (steps >= max_steps) break;
        }

        /* 当前节点不再需要（其副本已在子节点中） */
        type_free(node->left);
        type_free(node->right);
        free(node);
    }

    explorer->exhausted = (explorer->queue_head >= explorer->queue_tail);
    return false;
}

const TypeRewritePath *type_equiv_explore_get_path(const TypeEquivExplorer *explorer)
{
    if (!explorer || !explorer->found_equivalence) return NULL;
    return explorer->proved_path;
}

void type_equiv_explore_destroy(TypeEquivExplorer *explorer)
{
    if (!explorer) return;

    /* 释放队列中所有剩余节点 */
    for (int i = explorer->queue_head; i < explorer->queue_tail; i++) {
        if (explorer->queue[i]) {
            type_free(explorer->queue[i]->left);
            type_free(explorer->queue[i]->right);
            free(explorer->queue[i]);
        }
    }
    free(explorer->queue);

    type_free(explorer->left_original);
    type_free(explorer->right_original);

    if (explorer->proved_path) {
        type_rewrite_path_destroy(explorer->proved_path);
    }

    free(explorer);
}

void type_equiv_explore_get_stats(const TypeEquivExplorer *explorer,
                                  int *out_nodes,
                                  int *out_max_depth,
                                  bool *out_found,
                                  bool *out_exhausted)
{
    if (out_nodes)     *out_nodes     = explorer ? explorer->nodes_explored : 0;
    if (out_max_depth) *out_max_depth = 0;
    if (out_found)     *out_found     = explorer ? explorer->found_equivalence : false;
    if (out_exhausted) *out_exhausted = explorer ? explorer->exhausted : false;

    if (explorer && out_max_depth) {
        int md = 0;
        for (int i = explorer->queue_head; i < explorer->queue_tail; i++) {
            if (explorer->queue[i] && explorer->queue[i]->depth > md) {
                md = explorer->queue[i]->depth;
            }
        }
        *out_max_depth = md;
    }
}
