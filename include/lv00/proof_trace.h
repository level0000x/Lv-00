/**
 * @file proof_trace.h
 * @brief 证明追踪树 —— 结构化证明步骤的可视化追踪与导出
 *
 * 提供以树形结构记录证明过程的每一步（使用的公理、前提、结论、
 * 子步骤等），并支持导出为人类可读的逐步证明文本。
 * 可用于生成类似于数学教科书风格的详细证明展示。
 */

#ifndef LV00_PROOF_TRACE_H
#define LV00_PROOF_TRACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ============== 前向声明 ============== */
typedef struct Lv00ProofTreeNode Lv00ProofTreeNode;
typedef struct Lv00ProofTree     Lv00ProofTree;

/* ============== 前提描述 ============== */

/**
 * @brief 前提描述 —— 证明步骤中引用的一个前提
 *
 * 可以是已知公理、已证定理、前提条件或中间结论。
 * 通过 premise_id 和 description 标识前提的来源。
 */
typedef struct {
    int   premise_id;    /**< 前提的唯一标识符 */
    char *description;   /**< 前提的文字描述（可为 NULL） */
    bool  is_axiom;      /**< 是否为公理（true=公理, false=定理/引理） */
} Lv00ProofPremise;

/* ============== 证明树节点 ============== */

/**
 * @brief 证明树节点 —— 表示证明过程中的一个推理步骤
 *
 * 每个节点记录：
 * - 应用了哪个公理/规则（axiom_used）
 * - 使用了哪些前提（premises[]）
 * - 推导出了什么结论（conclusion）
 * - 该步骤的子步骤（children[]）—— 用于支持分层证明
 * - 该节点在证明树中的深度（depth）
 */
struct Lv00ProofTreeNode {
    int    id;                /**< 节点唯一标识符（在树内自增） */
    int    step_index;        /**< 对应的证明步骤在 ProofNavigator 中的索引（-1=无关联） */

    /* 推理内容 */
    char  *axiom_used;        /**< 使用的公理/规则名称 */
    Lv00ProofPremise *premises; /**< 前提数组 */
    int    premise_count;     /**< 前提数量 */
    int    premise_capacity;  /**< 前提数组容量 */

    char  *conclusion;        /**< 推导出的结论描述 */

    /* 树结构 */
    Lv00ProofTreeNode *parent;   /**< 父节点（根节点为 NULL） */
    Lv00ProofTreeNode **children; /**< 子节点数组 */
    int    child_count;         /**< 子节点数量 */
    int    child_capacity;      /**< 子节点数组容量 */

    /* 追踪信息 */
    int    depth;             /**< 当前节点在树中的深度 */
    bool   is_contradiction_branch; /**< 是否为反证法分支 */
    bool   is_lemma;          /**< 是否为引理（可折叠的子证明） */
};

/* ============== 证明树 ============== */

/**
 * @brief 证明追踪树 —— 完整证明的树形记录容器
 *
 * 根容器管理整个证明追踪树，提供节点分配、ID 管理和导出功能。
 * 维护所有节点的线性数组以便于遍历，以及树统计信息。
 */
struct Lv00ProofTree {
    Lv00ProofTreeNode *root;           /**< 树根节点 */
    Lv00ProofTreeNode **all_nodes;     /**< 所有节点的线性数组（便于遍历） */
    int    node_count;                 /**< 节点总数 */
    int    node_capacity;              /**< 节点数组容量 */

    /* 统计信息 */
    int    total_steps;      /**< 总步骤数 */
    int    max_depth;        /**< 最大深度 */
    int    axiom_count;      /**< 使用的不同公理数量 */
    int    lemma_count;      /**< 引理数量 */
    int    contradiction_count; /**< 反证法分支数 */

    /* 元数据 */
    char  *theorem_name;     /**< 所证明的定理名称（可为 NULL） */
    char  *proof_strategy;   /**< 使用的证明策略描述（可为 NULL） */
};

/* ============== API ============== */

/**
 * @brief 创建证明追踪树
 *
 * 分配并初始化一个空的证明树，创建根节点。
 *
 * @param theorem_name   定理名称（可为 NULL）
 * @param proof_strategy 证明策略描述（可为 NULL）
 * @return 新分配的证明树指针，失败返回 NULL
 */
Lv00ProofTree *lv00_proof_tree_create(const char *theorem_name, const char *proof_strategy);

/**
 * @brief 销毁证明追踪树并释放所有资源
 *
 * 递归释放所有节点、前提描述、结论字符串等。
 *
 * @param tree  证明树指针（可为 NULL）
 */
void lv00_proof_tree_destroy(Lv00ProofTree *tree);

/**
 * @brief 向证明树添加一个推理步骤
 *
 * 在指定父节点下创建新的子节点，记录使用的公理、前提和结论。
 * 如果 parent 为 NULL，则添加到根节点下。
 *
 * @param tree           证明树
 * @param parent         父节点（NULL 表示添加到根节点下）
 * @param axiom_used     使用的公理/规则名称（内部复制）
 * @param conclusion     推导出的结论描述（内部复制）
 * @param step_index     对应的证明步骤索引（-1 = 无关联）
 * @return 新创建的节点指针，失败返回 NULL
 */
Lv00ProofTreeNode *lv00_proof_tree_add_step(Lv00ProofTree *tree, Lv00ProofTreeNode *parent,
                                             const char *axiom_used, const char *conclusion,
                                             int step_index);

/**
 * @brief 向证明树节点添加前提
 *
 * @param node          目标节点
 * @param premise_id    前提标识符
 * @param description   前提描述（内部复制，可为 NULL）
 * @param is_axiom      是否为公理
 * @return true 添加成功，false 参数无效或内存分配失败
 */
bool lv00_proof_tree_add_premise(Lv00ProofTreeNode *node, int premise_id,
                                  const char *description, bool is_axiom);

/**
 * @brief 将证明树导出为人类可读的逐步证明文本
 *
 * 生成类似数学教科书风格的详细证明文本，包括：
 * - 定理名称和整体策略
 * - 逐步推理（缩进表示嵌套层次）
 * - 前提引用和公理名称
 * - 反证法分支的特殊标注
 *
 * @param tree      证明树
 * @param filepath   输出文件路径（可为 NULL，此时写入内部缓冲区）
 * @return 新分配的证明文本字符串（调用者需用 lv00_free 释放），失败返回 NULL
 */
char *lv00_proof_tree_export_text(const Lv00ProofTree *tree, const char *filepath);

/**
 * @brief 标记节点为反证法分支
 *
 * 设置节点的 is_contradiction_branch 标志，
 * 更新树的 contradiction_count 统计。
 *
 * @param node  目标节点
 */
void lv00_proof_tree_mark_contradiction(Lv00ProofTreeNode *node);

/**
 * @brief 标记节点为引理（可折叠的子证明）
 *
 * @param node  目标节点
 */
void lv00_proof_tree_mark_lemma(Lv00ProofTreeNode *node);

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
                                int *out_axiom_count);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PROOF_TRACE_H */
