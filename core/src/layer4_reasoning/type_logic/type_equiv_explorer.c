/* ============================================================================
 * 交互式类型等价探索器引擎实现
 *
 * 核心算法：BFS 双向搜索
 *   1. 将左右两侧原始类型深拷贝后入队作为搜索根节点
 *   2. 每步出队一个节点，调用 type_check_equivalence 检查左右是否等价
 *   3. 等价 → 沿父链回溯构造 TypeRewritePath 证明路径，返回成功
 *   4. 不等价 → 枚举 TypeSystem 中所有匹配左侧或右侧类型种类的规则，
 *      各生成一个后继节点（替换对应侧为规则应用结果）并入队
 *   5. 重复直到找到等价证明或耗尽步数预算 / 节点上限
 *
 * 规则匹配：仅基于 TypeKind —— 若规则的 pattern.kind 等于当前类型区域
 * 的 kind 或为 TYPE_KIND_VARIABLE（通配），则认为该规则可应用。
 * 应用时通过 type_normalize 变换类型结构。
 *
 * 设计文档参考：§3.6 用户交互式路径探索 · 类型等价双向搜索
 *
 * ============================================================================ */

#include "lv/type_equiv_explorer.h"

#include <stdlib.h>
#include <string.h>

#include "lv/lv_utils.h"
#include "lv/rewrite.h"

/* ---- 外部函数声明（type_region 深拷贝/释放声明已收口到 type_system.h） ---- */
extern bool type_normalize(TypeSystem *ts, TypeRegion *type, TypeRegion **out_normalized);

/* ---- 内部常量 ------------------------------------------------------------- */

/** @brief BFS 搜索队列初始容量 */
#define INITIAL_QUEUE_CAPACITY 64
/** @brief 默认最大搜索深度 */
#define DEFAULT_MAX_DEPTH 20
/** @brief 默认最大探索节点数 */
#define DEFAULT_MAX_NODES 10000

/* ---- 类型局部别名 --------------------------------------------------------- */

/** 为外部深拷贝取的本地别名，避免每次写 extern 声明 */
static inline TypeRegion *type_copy(const TypeRegion *src) {
    return type_region_deep_copy(src);
}

/** 为外部深释放取的本地别名 */
static inline void type_free_op(TypeRegion *tr) {
    type_region_deep_free(tr);
}

#define type_free type_free_op

/* ============================================================================
 * 内部辅助函数
 * ============================================================================ */

/**
 * @brief 检查某条重写规则是否匹配给定类型区域
 *
 * 匹配条件：
 *   1. 规则模式的 kind 为 TYPE_KIND_VARIABLE → 通配，匹配一切
 *   2. 规则模式的 kind 等于类型的 kind → 精确匹配
 *
 * @param rule  待检查的重写规则
 * @param type  待匹配的类型区域
 * @return true 规则可应用于该类型
 */
static bool rule_matches_type(const RewriteRule *rule, const TypeRegion *type) {
    if (!rule || !rule->pattern || !type)
        return false;
    if (rule->pattern->kind == TYPE_KIND_VARIABLE)
        return true;
    return (TypeKind) rule->pattern->kind == type->kind;
}

/**
 * @brief 对类型区域应用一条匹配的规则
 *
 * 调用 type_normalize 尝试变换类型结构。
 * 若 type_normalize 返回了不同于 inout_type 的新对象，
 * 则将新对象内容移入 inout_type 后释放新对象。
 *
 * @param ts         类型系统上下文
 * @param inout_type 要变换的类型区域（原地修改）
 * @return true 变换成功
 */
static bool apply_rule_to_type(TypeSystem *ts, TypeRegion *inout_type) {
    if (!ts || !inout_type)
        return false;

    TypeRegion *normalized = NULL;
    if (!type_normalize(ts, inout_type, &normalized))
        return false;

    if (normalized && normalized != inout_type) {
        /*
         * type_normalize 分配了新对象 → 用深拷贝方式把新内容迁移到原地，
         * 避免 memcpy 浅拷贝破坏指针成员。
         */
        type_free(inout_type);
        *inout_type = *normalized; /* 浅拷贝结构体字段（kind、constraint_graph 指针等） */
        lv_free(normalized);       /* 只释放壳，不释放其内部资源（已转移） */
    }
    return true;
}

/**
 * @brief 扩展 BFS 搜索队列容量
 *
 * 统一扩容工具 lv_ensure_capacity：容量不足时翻倍扩容，等价于原
 * new_cap = queue_capacity * 2（失败时队列不变，与原 lv_realloc 失败语义一致）。
 *
 * @return true 扩展成功；false 内存不足，队列不变
 */
static bool expand_queue(TypeEquivExplorer *explorer) {
    return lv_ensure_capacity((void **) &explorer->queue, explorer->queue_tail,
                              &explorer->queue_capacity, sizeof(TypeEquivNode *), 1);
}

/**
 * @brief 创建一个搜索子节点（深拷贝父节点的左右类型）
 *
 * @param parent        父搜索节点
 * @param parent_idx    父节点在队列中的索引
 * @param rule          应用的规则
 * @param rule_idx      规则在 TypeSystem 中的索引
 * @param apply_to_left 规则应用到左侧（true）还是右侧（false）
 * @return 新分配的搜索节点，调用者负责入队并在销毁时释放
 */
static TypeEquivNode *create_child_node(const TypeEquivNode *parent, int parent_idx, const RewriteRule *rule,
                                        int rule_idx, bool apply_to_left) {
    TypeEquivNode *child = lv_calloc(1, sizeof(TypeEquivNode));
    if (!child)
        return NULL;

    child->left = type_copy(parent->left);
    child->right = type_copy(parent->right);
    if (!child->left || !child->right) {
        type_free(child->left);
        type_free(child->right);
        lv_free(child);
        return NULL;
    }

    child->depth = parent->depth + 1;
    child->parent_index = parent_idx;
    child->applied_rule_index = rule_idx;
    child->applied_to_left = apply_to_left;
    snprintf(child->applied_rule_name, sizeof(child->applied_rule_name), "%s", rule->name);

    /* 规则应用由调用者在正确的 ts 上下文中执行 */
    return child;
}

/**
 * @brief 释放一个搜索节点及其持有的类型资源
 */
static void free_search_node(TypeEquivNode *node) {
    if (!node)
        return;
    type_free(node->left);
    type_free(node->right);
    lv_free(node);
}

/**
 * @brief 沿父链回溯，从解节点构造 TypeRewritePath 证明路径
 *
 * @param explorer  探索器
 * @param node_idx  解节点在队列中的索引
 */
static void reconstruct_path(TypeEquivExplorer *explorer, int node_idx) {
    int max_steps = explorer->queue[node_idx]->depth + 1;

    /* 分配回溯数组 */
    int *reverse_path = lv_malloc((size_t) max_steps * sizeof(int));
    int *reverse_side = lv_malloc((size_t) max_steps * sizeof(int));
    char **reverse_names = lv_malloc((size_t) max_steps * sizeof(char *));
    if (!reverse_path || !reverse_side || !reverse_names) {
        lv_free(reverse_path);
        lv_free(reverse_side);
        lv_free(reverse_names);
        return;
    }

    /* 从解节点回溯到根，收集每一步的规则信息 */
    int step = 0;
    int cur = node_idx;
    while (cur >= 0 && step < max_steps) {
        TypeEquivNode *node = explorer->queue[cur];
        reverse_path[step] = node->applied_rule_index;
        reverse_side[step] = node->applied_to_left ? 0 : 1;
        reverse_names[step] = node->applied_rule_name;
        cur = node->parent_index;
        step++;
    }

    /* 创建正向路径并依序记录步骤 */
    TypeRewritePath *path = type_rewrite_path_create();
    if (!path) {
        lv_free(reverse_path);
        lv_free(reverse_side);
        lv_free(reverse_names);
        return;
    }

    for (int i = step - 1; i >= 0; i--) {
        if (reverse_path[i] < 0)
            continue; /* 跳过根节点（无规则） */
        type_rewrite_path_record(path, reverse_names[i], NULL, NULL);
    }

    explorer->proved_path = path;
    explorer->found_equivalence = true;
    explorer->solution_node_index = node_idx;

    lv_free(reverse_path);
    lv_free(reverse_side);
    lv_free(reverse_names);
}

/* ============================================================================
 * 公共 API
 * ============================================================================ */

TypeEquivExplorer *type_equiv_explore_create(TypeSystem *ts, const TypeRegion *left, const TypeRegion *right) {
    if (!ts || !left || !right)
        return NULL;

    TypeEquivExplorer *exp = lv_calloc(1, sizeof(TypeEquivExplorer));
    if (!exp)
        return NULL;

    exp->ts = ts;
    exp->left_original = type_copy(left);
    exp->right_original = type_copy(right);
    if (!exp->left_original || !exp->right_original) {
        type_free(exp->left_original);
        type_free(exp->right_original);
        lv_free(exp);
        return NULL;
    }

    /* 初始化搜索队列 */
    exp->queue_capacity = INITIAL_QUEUE_CAPACITY;
    exp->queue = lv_calloc((size_t) exp->queue_capacity, sizeof(TypeEquivNode *));
    if (!exp->queue) {
        type_free(exp->left_original);
        type_free(exp->right_original);
        lv_free(exp);
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

    /* 创建搜索根节点并入队 */
    TypeEquivNode *root = lv_calloc(1, sizeof(TypeEquivNode));
    if (!root) {
        type_equiv_explore_destroy(exp);
        return NULL;
    }
    root->left = type_copy(left);
    root->right = type_copy(right);
    root->depth = 0;
    root->parent_index = -1;
    root->applied_rule_index = -1;
    snprintf(root->applied_rule_name, sizeof(root->applied_rule_name), "(root)");

    exp->queue[exp->queue_tail++] = root;
    return exp;
}

bool type_equiv_explore_search(TypeEquivExplorer *explorer, int max_steps) {
    if (!explorer)
        return false;

    int steps = 0;
    while (explorer->queue_head < explorer->queue_tail && steps < max_steps) {
        /* 节点数达到上限 → 搜索空间耗尽 */
        if (explorer->nodes_explored >= explorer->max_nodes) {
            explorer->exhausted = true;
            return false;
        }

        /* 出队当前节点 */
        TypeEquivNode *node = explorer->queue[explorer->queue_head++];
        explorer->nodes_explored++;

        /* 深度超限 → 跳过（不生成更深的后继） */
        if (node->depth > explorer->max_depth) {
            free_search_node(node);
            continue;
        }

        /* 检查左右两侧是否等价 */
        if (type_check_equivalence(explorer->ts, node->left, node->right, true) == TYPE_EQUIV_OK) {
            reconstruct_path(explorer, (int) (explorer->queue_head - 1));
            free_search_node(node);
            return true;
        }

        /*
         * 生成后继：
         * 对左右两侧各枚举所有匹配规则，每个规则生成一个子节点。
         * 子节点继承父节点的类型副本并对其对应侧应用规则。
         */
        for (int side = 0; side < 2; side++) {
            bool apply_to_left = (side == 0);
            for (int r = 0; r < explorer->ts->rewrite_rule_count; r++) {
                RewriteRule *rule = explorer->ts->rewrite_rules[r];
                if (!rule || !rule->name)
                    continue;

                TypeRegion *target = apply_to_left ? node->left : node->right;
                if (!rule_matches_type(rule, target))
                    continue;

                /* 队列满 → 扩容 */
                if (explorer->queue_tail >= explorer->queue_capacity) {
                    if (!expand_queue(explorer)) {
                        explorer->exhausted = true;
                        free_search_node(node);
                        return false;
                    }
                }

                TypeEquivNode *child =
                    create_child_node(node, (int) (explorer->queue_head - 1), rule, r, apply_to_left);
                if (!child)
                    continue;

                /* 对子节点的目标侧应用规则 */
                apply_rule_to_type(explorer->ts, apply_to_left ? child->left : child->right);

                explorer->queue[explorer->queue_tail++] = child;
                steps++;
                if (steps >= max_steps)
                    goto exhausted;
            }
        }

        free_search_node(node);
        continue;

    exhausted:
        free_search_node(node);
        break;
    }

    explorer->exhausted = (explorer->queue_head >= explorer->queue_tail);
    return false;
}

const TypeRewritePath *type_equiv_explore_get_path(const TypeEquivExplorer *explorer) {
    return (explorer && explorer->found_equivalence) ? explorer->proved_path : NULL;
}

void type_equiv_explore_destroy(TypeEquivExplorer *explorer) {
    if (!explorer)
        return;

    /* 释放队列中剩余的所有搜索节点 */
    for (int i = explorer->queue_head; i < explorer->queue_tail; i++) {
        if (explorer->queue[i]) {
            type_free(explorer->queue[i]->left);
            type_free(explorer->queue[i]->right);
            lv_free(explorer->queue[i]);
        }
    }
    lv_free(explorer->queue);

    /* 释放原始类型副本和证明路径 */
    type_free(explorer->left_original);
    type_free(explorer->right_original);
    if (explorer->proved_path) {
        type_rewrite_path_destroy(explorer->proved_path);
    }

    lv_free(explorer);
}

void type_equiv_explore_get_stats(const TypeEquivExplorer *explorer, int *out_nodes, int *out_max_depth,
                                  bool *out_found, bool *out_exhausted) {
    /* 默认值：explorer 为 NULL 时各项均为零/false */
    if (out_nodes)
        *out_nodes = explorer ? explorer->nodes_explored : 0;
    if (out_found)
        *out_found = explorer ? explorer->found_equivalence : false;
    if (out_exhausted)
        *out_exhausted = explorer ? explorer->exhausted : false;

    /* 计算搜索队列中达到的最大深度 */
    if (out_max_depth) {
        *out_max_depth = 0;
        if (explorer) {
            int md = 0;
            for (int i = explorer->queue_head; i < explorer->queue_tail; i++) {
                if (explorer->queue[i] && explorer->queue[i]->depth > md) {
                    md = explorer->queue[i]->depth;
                }
            }
            *out_max_depth = md;
        }
    }
}
