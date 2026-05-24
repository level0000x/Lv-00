/**
 * @file proof_trace.c
 * @brief 证明追踪树实现 —— 树形证明步骤记录与文本导出
 *
 * @details 实现基于树结构的证明步骤追踪系统，每个节点记录推理过程
 *          （使用的公理、前提列表、结论），支持层次化嵌套子证明。
 *          提供人类可读的逐步证明文本导出功能。
 *
 *          核心功能模块：
 *          - 树创建与销毁：分配/释放证明树及其所有节点
 *          - 节点添加：向父节点下追加子节点，自动管理 ID 和深度
 *          - 前提管理：为节点附加前提引用列表
 *          - 文本导出：生成缩进格式的逐步证明文本
 *          - 标记系统：反证法分支、引理折叠标记
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

#include "proof_trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00_utils.h"

/* ============== 内部常量 ============== */

/** 初始子节点容量 */
#define PROOF_TRACE_CHILD_CAP   4

/** 初始前提容量 */
#define PROOF_TRACE_PREMISE_CAP 4

/** 初始节点数组容量 */
#define PROOF_TRACE_NODE_CAP    64

/** 导出文本每级缩进宽度（空格数） */
#define PROOF_TRACE_INDENT      2

/** 导出文本行缓冲区大小 */
#define PROOF_TRACE_LINE_BUF    512

/* ============== 证明树 API ============== */

/**
 * @brief 创建证明追踪树
 *
 * 分配证明树并创建根节点。根节点的 axiom_used 为 NULL，
 * conclusion 存储定理名称，depth 为 0。
 *
 * @param theorem_name   定理名称（可为 NULL，内部复制）
 * @param proof_strategy 证明策略描述（可为 NULL，内部复制）
 * @return 新分配的证明树指针，失败返回 NULL
 */
Lv00ProofTree *lv00_proof_tree_create(const char *theorem_name, const char *proof_strategy) {
    Lv00ProofTree *tree = lv00_calloc(1, sizeof(Lv00ProofTree));
    if (!tree)
        return NULL;

    /* 创建根节点 */
    Lv00ProofTreeNode *root = lv00_calloc(1, sizeof(Lv00ProofTreeNode));
    if (!root) {
        lv00_free((void **) &tree);
        return NULL;
    }

    root->id = 0;
    root->depth = 0;
    root->step_index = -1;
    root->conclusion = theorem_name ? lv00_strdup(theorem_name) : NULL;
    tree->root = root;

    /* 分配初始节点数组 */
    tree->node_capacity = PROOF_TRACE_NODE_CAP;
    tree->all_nodes = lv00_malloc(tree->node_capacity * sizeof(Lv00ProofTreeNode *));
    if (!tree->all_nodes) {
        lv00_free((void **) &root->conclusion);
        lv00_free((void **) &root);
        lv00_free((void **) &tree);
        return NULL;
    }
    tree->all_nodes[0] = root;
    tree->node_count = 1;

    /* 存储元数据 */
    tree->theorem_name = theorem_name ? lv00_strdup(theorem_name) : NULL;
    tree->proof_strategy = proof_strategy ? lv00_strdup(proof_strategy) : NULL;

    return tree;
}

/** 递归销毁最大深度限制（防止栈溢出） */
#define PROOF_TREE_DESTROY_MAX_DEPTH 10000

/**
 * @brief 递归销毁证明树节点（内部实现，带深度保护）
 *
 * @param node   要销毁的节点
 * @param depth  当前递归深度
 */
static void proof_tree_node_destroy_recursive_impl(Lv00ProofTreeNode *node, int depth) {
    if (!node)
        return;

    if (depth > PROOF_TREE_DESTROY_MAX_DEPTH) {
        /* 深度超限：仅释放当前节点，子节点可能泄漏但避免栈溢出 */
        fprintf(stderr,
                "proof_tree_node_destroy: 递归深度 %d 超过限制 %d，"
                "部分子节点可能未释放\n",
                depth, PROOF_TREE_DESTROY_MAX_DEPTH);
        /* 尝试释放子节点数组但不递归 */
        lv00_free((void **) &node->children);
        lv00_free((void **) &node);
        return;
    }

    /* 先递归销毁所有子节点 */
    for (int i = 0; i < node->child_count; i++) {
        proof_tree_node_destroy_recursive_impl(node->children[i], depth + 1);
    }

    /* 释放前提列表 */
    for (int i = 0; i < node->premise_count; i++) {
        lv00_free((void **) &node->premises[i].description);
    }
    lv00_free((void **) &node->premises);

    /* 释放自身资源 */
    lv00_free((void **) &node->axiom_used);
    lv00_free((void **) &node->conclusion);
    lv00_free((void **) &node->children);
    lv00_free((void **) &node);
}

/**
 * @brief 递归销毁证明树节点（包装函数）
 *
 * @param node  要销毁的节点
 */
static void proof_tree_node_destroy_recursive(Lv00ProofTreeNode *node) {
    proof_tree_node_destroy_recursive_impl(node, 0);
}

/**
 * @brief 销毁证明追踪树并释放所有资源
 *
 * @param tree  证明树指针（可为 NULL）
 */
void lv00_proof_tree_destroy(Lv00ProofTree *tree) {
    if (!tree)
        return;

    /* 递归销毁根节点及其子树 */
    proof_tree_node_destroy_recursive(tree->root);

    /* 释放线性节点数组（指针本身，不需要释放内容，因为已递归释放） */
    lv00_free((void **) &tree->all_nodes);

    lv00_free((void **) &tree->theorem_name);
    lv00_free((void **) &tree->proof_strategy);
    lv00_free((void **) &tree);
}

/* ============== 节点管理 ============== */

/**
 * @brief 注册节点到树的线性节点数组
 *
 * 将新创建的节点加入 all_nodes 数组，动态扩容。
 *
 * @param tree  证明树
 * @param node  新节点
 * @return true 成功，false 扩容失败
 */
static bool proof_tree_register_node(Lv00ProofTree *tree, Lv00ProofTreeNode *node) {
    if (tree->node_count >= tree->node_capacity) {
        int new_cap = tree->node_capacity * 2;
        Lv00ProofTreeNode **new_arr = lv00_realloc(tree->all_nodes,
                                                    new_cap * sizeof(Lv00ProofTreeNode *));
        if (!new_arr)
            return false;
        tree->all_nodes = new_arr;
        tree->node_capacity = new_cap;
    }

    node->id = tree->node_count;
    tree->all_nodes[tree->node_count] = node;
    tree->node_count++;
    return true;
}

/**
 * @brief 向证明树添加一个推理步骤
 *
 * 在指定父节点下创建新子节点。如果 parent 为 NULL，默认使用根节点。
 * 自动计算深度、分配 ID、更新树统计信息。
 *
 * @param tree           证明树
 * @param parent         父节点（NULL 表示根节点）
 * @param axiom_used     使用的公理/规则名称（内部复制）
 * @param conclusion     推导出的结论描述（内部复制）
 * @param step_index     对应的证明步骤索引
 * @return 新创建的节点指针，失败返回 NULL
 */
Lv00ProofTreeNode *lv00_proof_tree_add_step(Lv00ProofTree *tree, Lv00ProofTreeNode *parent,
                                             const char *axiom_used, const char *conclusion,
                                             int step_index) {
    if (!tree)
        return NULL;

    /* 未指定父节点则默认使用根节点 */
    if (!parent)
        parent = tree->root;
    if (!parent)
        return NULL;

    /* 扩容父节点的子节点数组 */
    if (parent->child_count >= parent->child_capacity) {
        int new_cap = parent->child_capacity == 0 ? PROOF_TRACE_CHILD_CAP : parent->child_capacity * 2;
        Lv00ProofTreeNode **new_arr = lv00_realloc(parent->children,
                                                    new_cap * sizeof(Lv00ProofTreeNode *));
        if (!new_arr)
            return NULL;
        parent->children = new_arr;
        parent->child_capacity = new_cap;
    }

    /* 创建新节点 */
    Lv00ProofTreeNode *node = lv00_calloc(1, sizeof(Lv00ProofTreeNode));
    if (!node)
        return NULL;

    node->depth = parent->depth + 1;
    node->parent = parent;
    node->step_index = step_index;

    /* 复制推理内容 */
    node->axiom_used = axiom_used ? lv00_strdup(axiom_used) : NULL;
    node->conclusion = conclusion ? lv00_strdup(conclusion) : NULL;

    /* 追加到父节点 */
    parent->children[parent->child_count] = node;
    parent->child_count++;

    /* 注册到树 */
    if (!proof_tree_register_node(tree, node)) {
        parent->child_count--;
        lv00_free((void **) &node->axiom_used);
        lv00_free((void **) &node->conclusion);
        lv00_free((void **) &node);
        return NULL;
    }

    /* 更新统计信息 */
    tree->total_steps++;
    if (node->depth > tree->max_depth) {
        tree->max_depth = node->depth;
    }

    return node;
}

/* ============== 前提管理 ============== */

/**
 * @brief 向证明树节点添加前提
 *
 * @param node          目标节点
 * @param premise_id    前提标识符
 * @param description   前提描述（内部复制，可为 NULL）
 * @param is_axiom      是否为公理
 * @return true 添加成功
 */
bool lv00_proof_tree_add_premise(Lv00ProofTreeNode *node, int premise_id,
                                  const char *description, bool is_axiom) {
    if (!node)
        return false;

    /* 扩容前提数组 */
    if (node->premise_count >= node->premise_capacity) {
        int new_cap = node->premise_capacity == 0 ? PROOF_TRACE_PREMISE_CAP : node->premise_capacity * 2;
        Lv00ProofPremise *new_arr = lv00_realloc(node->premises,
                                                  new_cap * sizeof(Lv00ProofPremise));
        if (!new_arr)
            return false;
        node->premises = new_arr;
        node->premise_capacity = new_cap;
    }

    Lv00ProofPremise *p = &node->premises[node->premise_count];
    p->premise_id = premise_id;
    p->description = description ? lv00_strdup(description) : NULL;
    p->is_axiom = is_axiom;
    node->premise_count++;

    return true;
}

/* ============== 标记函数 ============== */

/**
 * @brief 标记节点为反证法分支
 *
 * @param node  目标节点
 */
void lv00_proof_tree_mark_contradiction(Lv00ProofTreeNode *node) {
    if (!node)
        return;
    node->is_contradiction_branch = true;
}

/**
 * @brief 标记节点为引理（可折叠的子证明）
 *
 * @param node  目标节点
 */
void lv00_proof_tree_mark_lemma(Lv00ProofTreeNode *node) {
    if (!node)
        return;
    node->is_lemma = true;
}

/* ============== 统计查询 ============== */

/**
 * @brief 获取证明树的统计摘要
 *
 * @param tree           证明树
 * @param out_total_steps  输出：总步骤数
 * @param out_max_depth    输出：最大深度
 * @param out_axiom_count  输出：使用的公理数量
 */
void lv00_proof_tree_get_stats(const Lv00ProofTree *tree,
                                int *out_total_steps, int *out_max_depth,
                                int *out_axiom_count) {
    if (!tree) {
        if (out_total_steps) *out_total_steps = 0;
        if (out_max_depth)  *out_max_depth = 0;
        if (out_axiom_count) *out_axiom_count = 0;
        return;
    }

    if (out_total_steps) *out_total_steps = tree->total_steps;
    if (out_max_depth)  *out_max_depth = tree->max_depth;

    /* 统计使用的不同公理数量 */
    if (out_axiom_count) {
        int count = 0;
        for (int i = 0; i < tree->node_count; i++) {
            if (tree->all_nodes[i]->axiom_used) {
                count++;
            }
        }
        *out_axiom_count = count;
    }
}

/* ============== 文本导出 ============== */

/**
 * @brief 递归导出节点子树为文本
 *
 * 使用缩进表示嵌套层次，添加反证法/引理标记。
 *
 * @param node      当前节点
 * @param depth     当前缩进深度
 * @param buf       输出缓冲区（动态扩容）
 * @param buf_size  缓冲区当前大小
 * @param buf_used  已使用的字节数
 * @return 更新后的缓冲区，NULL 表示内存分配失败
 */
static char *proof_tree_export_node(const Lv00ProofTreeNode *node, int depth,
                                     char *buf, size_t *buf_size, size_t *buf_used) {
    if (!node || !buf)
        return buf;

    char line[PROOF_TRACE_LINE_BUF];
    int indent = depth * PROOF_TRACE_INDENT;

    /* 跳过没有任何推理内容的根节点 */
    if (!(depth == 0 && !node->axiom_used && !node->conclusion)) {
        /* 生成缩进前缀 */
        char prefix[64];
        int prefix_len = 0;
        for (int i = 0; i < indent && prefix_len < (int) sizeof(prefix) - 1; i++) {
            prefix[prefix_len++] = ' ';
        }
        prefix[prefix_len] = '\0';

        /* 生成步骤编号或标记 */
        if (node->is_contradiction_branch) {
            /* 反证法分支：特殊标记 */
            snprintf(line, sizeof(line), "%s[反证法分支] ", prefix);
        } else if (node->is_lemma) {
            /* 引理：标记为可折叠 */
            snprintf(line, sizeof(line), "%s[引理] ", prefix);
        } else if (depth > 0) {
            /* 普通步骤：用数字编号 */
            snprintf(line, sizeof(line), "%s步骤 %d: ", prefix, node->id);
        } else {
            /* 根节点：输出定理名称 */
            if (node->conclusion) {
                snprintf(line, sizeof(line), "定理: %s\n", node->conclusion);
            } else {
                line[0] = '\0';
            }
        }

        /* 追加步骤行 */
        size_t line_len = strlen(line);
        if (*buf_used + line_len + 1 > *buf_size) {
            *buf_size = (*buf_size) * 2 + line_len + 1;
            char *new_buf = lv00_realloc(buf, *buf_size);
            if (!new_buf) {
                lv00_free((void **) &buf);
                return NULL;
            }
            buf = new_buf;
        }
        memcpy(buf + *buf_used, line, line_len);
        *buf_used += line_len;

        /* 输出推理内容（非根节点） */
        if (depth > 0) {
            /* 公理/规则 */
            if (node->axiom_used) {
                char axiom_line[PROOF_TRACE_LINE_BUF];
                snprintf(axiom_line, sizeof(axiom_line), "使用: %s", node->axiom_used);
                size_t alen = strlen(axiom_line);
                if (*buf_used + alen + 1 > *buf_size) {
                    *buf_size = (*buf_size) * 2 + alen + 1;
                    char *new_buf = lv00_realloc(buf, *buf_size);
                    if (!new_buf) { lv00_free((void **) &buf); return NULL; }
                    buf = new_buf;
                }
                memcpy(buf + *buf_used, axiom_line, alen);
                *buf_used += alen;
            }

            /* 前提 */
            if (node->premise_count > 0) {
                char prem_line[PROOF_TRACE_LINE_BUF];
                int plen = snprintf(prem_line, sizeof(prem_line), " [前提: ");
                for (int j = 0; j < node->premise_count && plen < (int) sizeof(prem_line) - 1; j++) {
                    const char *desc = node->premises[j].description;
                    if (desc) {
                        plen += snprintf(prem_line + plen, sizeof(prem_line) - plen,
                                        "%s%s", j > 0 ? ", " : "", desc);
                    } else {
                        plen += snprintf(prem_line + plen, sizeof(prem_line) - plen,
                                        "%s#%d", j > 0 ? ", " : "", node->premises[j].premise_id);
                    }
                }
                snprintf(prem_line + plen, sizeof(prem_line) - plen, "]");

                size_t actual_len = strlen(prem_line);
                if (*buf_used + actual_len + 1 > *buf_size) {
                    *buf_size = (*buf_size) * 2 + actual_len + 1;
                    char *new_buf = lv00_realloc(buf, *buf_size);
                    if (!new_buf) { lv00_free((void **) &buf); return NULL; }
                    buf = new_buf;
                }
                memcpy(buf + *buf_used, prem_line, actual_len);
                *buf_used += actual_len;
            }

            /* 结论 */
            if (node->conclusion) {
                char conc_line[PROOF_TRACE_LINE_BUF];
                snprintf(conc_line, sizeof(conc_line), " => %s", node->conclusion);
                size_t clen = strlen(conc_line);
                if (*buf_used + clen + 1 > *buf_size) {
                    *buf_size = (*buf_size) * 2 + clen + 1;
                    char *new_buf = lv00_realloc(buf, *buf_size);
                    if (!new_buf) { lv00_free((void **) &buf); return NULL; }
                    buf = new_buf;
                }
                memcpy(buf + *buf_used, conc_line, clen);
                *buf_used += clen;
            }

            /* 反证法结果标记 */
            if (node->is_contradiction_branch && node->conclusion) {
                char contr_mark[] = " [矛盾!]";
                size_t cmlen = strlen(contr_mark);
                if (*buf_used + cmlen + 1 > *buf_size) {
                    *buf_size = (*buf_size) * 2 + cmlen + 1;
                    char *new_buf = lv00_realloc(buf, *buf_size);
                    if (!new_buf) { lv00_free((void **) &buf); return NULL; }
                    buf = new_buf;
                }
                memcpy(buf + *buf_used, contr_mark, cmlen);
                *buf_used += cmlen;
            }
        }

        /* 换行 */
        buf[*buf_used] = '\n';
        (*buf_used)++;
        if (*buf_used >= *buf_size) {
            *buf_size = *buf_used + 1;
            char *new_buf = lv00_realloc(buf, *buf_size);
            if (!new_buf) { lv00_free((void **) &buf); return NULL; }
            buf = new_buf;
        }
    }

    /* 递归处理子节点 */
    for (int i = 0; i < node->child_count; i++) {
        buf = proof_tree_export_node(node->children[i], depth + (depth > 0 ? 1 : 1),
                                      buf, buf_size, buf_used);
        if (!buf)
            return NULL;
    }

    return buf;
}

/**
 * @brief 将证明树导出为人类可读的逐步证明文本
 *
 * @param tree      证明树
 * @param filepath   输出文件路径（可为 NULL）
 * @return 新分配的证明文本字符串（调用者需用 lv00_free 释放），失败返回 NULL
 */
char *lv00_proof_tree_export_text(const Lv00ProofTree *tree, const char *filepath) {
    if (!tree || !tree->root)
        return NULL;

    size_t buf_size = 4096;
    size_t buf_used = 0;
    char *buf = lv00_malloc(buf_size);
    if (!buf)
        return NULL;
    buf[0] = '\0';

    /* 输出头部信息 */
    if (tree->theorem_name) {
        int n = snprintf(buf + buf_used, buf_size - buf_used,
                        "========================================\n"
                        "  定理证明: %s\n"
                        "========================================\n\n",
                        tree->theorem_name);
        buf_used += n;
    }

    if (tree->proof_strategy) {
        int n = snprintf(buf + buf_used, buf_size - buf_used,
                        "证明策略: %s\n\n", tree->proof_strategy);
        buf_used += n;
    }

    /* 递归导出证明树 */
    char *result = proof_tree_export_node(tree->root, 0, buf, &buf_size, &buf_used);
    if (!result) {
        return NULL;
    }

    /* 输出尾部统计 */
    {
        int n = snprintf(result + buf_used, buf_size - buf_used,
                        "\n========================================\n"
                        "证明共 %d 步，最大深度 %d\n", tree->total_steps, tree->max_depth);
        buf_used += n;
    }

    /* 如果指定了文件路径，写入文件 */
    if (filepath) {
        FILE *fp = fopen(filepath, "w");
        if (fp) {
            fwrite(result, 1, buf_used, fp);
            fclose(fp);
        }
    }

    return result;
}
