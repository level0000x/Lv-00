/* ========================================================================
 * 模块名称：合一检查 (unify)
 * 功能概述：提供构造图与命题图的匹配验证功能。包含基础版、坐标增强版、
 *          哈希预过滤版三种合一检查，以及详细失败报告、命题等价声明
 *          与查找、多态命题实例化等功能。
 *
 * 主要 API：
 *   - unify_construction_with_proposition        — 基础合一检查
 *   - unify_construction_with_proposition_coord  — 坐标增强合一
 *   - unify_construction_with_proposition_hash_filtered — 哈希预过滤合一
 *   - unify_construction_with_proposition_detailed — 带详细报告的合一
 *   - unify_match_ports / constraints / coords   — 精细化匹配
 *   - unify_declare_proposition_equivalence      — 声明命题等价
 *   - unify_instantiate_proposition              — 多态命题实例化
 *
 * 使用示例：
 lv_PUBLIC_API *   UnifyStatus s = unify_construction_with_proposition(construction, proposition);
 *   if (s != UNIFY_STATUS_OK) {
 *       UnifyFailureInfo info;
 lv_PUBLIC_API *       unify_construction_with_proposition_detailed(construction, pattern, &info);
 *   }
 *
 * ======================================================================== */

/**
 * @file unify.h
 * @brief 合一检查 —— 构造图与命题图的匹配验证
 */

#ifndef lv_UNIFY_H
#define lv_UNIFY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "constraint_graph.h"
#include "stream.h"
#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */

/* 前向声明：TypeRegion 完整定义在 type_system.h（L4 type_logic 域）。
 * 仅指针引用，打破 unify.h → type_system.h → rewrite.h 的头级依赖三角环 */
typedef struct TypeRegion TypeRegion;

/**
 * @brief 设置合一检查器的流式输出上下文
 * @param ctx  流式上下文（可为 NULL 以禁用流式输出）
 */
lv_PUBLIC_API void unify_set_stream_context(StreamContext *ctx);

/* 合一检查状态枚举 */
typedef enum {
    UNIFY_STATUS_OK,                  /* 合一成功 */
    UNIFY_STATUS_PORT_TYPE_MISMATCH,  /* 端口类型不匹配 */
    UNIFY_STATUS_CONSTRAINT_MISMATCH, /* 约束不匹配 */
    UNIFY_STATUS_COORD_MISMATCH,      /* 坐标不匹配 */
    UNIFY_STATUS_STRUCTURE_MISMATCH,  /* 结构不匹配 */
    UNIFY_STATUS_SCOPE_MISMATCH,      /* 作用域不匹配 */
    UNIFY_STATUS_FAILED               /* 合一失败 */
} UnifyStatus;

/**
 * @brief 不匹配原因枚举 —— 精确描述合一失败的具体原因
 *
 * 用于精细化匹配函数（unify_match_ports、unify_match_constraints、
 * unify_match_coords）的详细失败报告，帮助外部调用者快速定位
 * 不匹配的具体位置和原因。
 */
typedef enum {
    PORT_TYPE_MISMATCH,                    /* 端口类型不匹配 */
    PORT_NAMESPACE_MISMATCH,               /* 端口命名空间深度不匹配 */
    PORT_TYPE_REGION_MISMATCH,             /* 端口类型区域（TypeRegion）不匹配 */
    CONSTRAINT_TYPE_MISMATCH,              /* 约束类型不匹配 */
    CONSTRAINT_PARTICIPANT_COUNT_MISMATCH, /* 约束参与者数量不匹配 */
    CONSTRAINT_PARTICIPANT_ID_MISMATCH,    /* 约束参与者 ID 不匹配 */
    COORD_VALUE_MISMATCH,                  /* 坐标值不匹配 */
    COORD_TYPE_MISMATCH                    /* 坐标类型不匹配 */
} MismatchReason;

/**
 * @brief 获取 UnifyStatus 的人类可读中文原因
 *
 * 统一维护合一失败状态的中文文案（原 proof_proposition.c /
 * engine_function.c 私有表收敛于此），同一状态全项目仅此一种说法。
 *
 * @param status 合一状态
 * @return 静态字符串（如 "约束类型不匹配"），越界返回 "未知错误"
 */
lv_PUBLIC_API const char *unify_status_reason_zh(UnifyStatus status);

/**
 * @brief 将构造图与命题图进行合一检查（基础版）
 *
 * 检查构造图是否满足命题图定义的结构模式，包括端口类型匹配和约束匹配。
 *
 * @param[in] construction  构造图（当前几何构造的状态）
 * @param[in] proposition   命题模式图（待匹配的目标模式）
 * @return 合一状态（UNIFY_STATUS_OK 表示匹配成功）
 */
lv_PUBLIC_API UnifyStatus unify_construction_with_proposition(const ConstraintGraph *construction,
                                                              const ConstraintGraph *proposition);

/**
 * @brief 带坐标级别相等检查的合一（增强版）
 *
 * 在约束匹配阶段，除了检查约束类型和参与者 ID，
 * 还验证对应参与者的符号坐标是否相等。
 * 返回 UNIFY_STATUS_COORD_MISMATCH 如果坐标不匹配。
 *
 * @param[in] construction  构造图
 * @param[in] proposition   命题模式图
 * @return 合一状态
 */
lv_PUBLIC_API UnifyStatus unify_construction_with_proposition_coord(const ConstraintGraph *construction,
                                                                    const ConstraintGraph *proposition);

/**
 * @brief 带哈希预过滤的合一（优化版）
 *
 * 在约束匹配前，使用 symbolic_coord_hash() 对节点分组，
 * 只比较相同哈希组的节点，加速匹配过程。
 *
 * @param[in] construction  构造图
 * @param[in] proposition   命题模式图
 * @return 合一状态
 */
lv_PUBLIC_API UnifyStatus unify_construction_with_proposition_hash_filtered(const ConstraintGraph *construction,
                                                                            const ConstraintGraph *proposition);

/* ============== 精细化匹配函数（供外部精细控制） ============== */

/**
 * @brief 单独执行端口类型匹配
 *
 * 独立于完整合一流程，仅执行命题图端口到构造图端口的匹配检查。
 * 返回匹配成功的端口对数量，-1 表示失败。
 *
 * @param[in]  construction       构造图
 * @param[in]  proposition        命题图（proposition）
 * @param[out] out_port_bindings  输出：端口绑定对数组 [prop_port_id, const_port_id] 交替排列，
 *                                大小至少为 proposition 端口数的 2 倍。
 *                                调用者需分配并传入（可为 NULL 以仅计数）。
 * @return 匹配成功的端口对数（>=0），或 -1 表示错误
 *
 * 【缓冲区大小要求 —— 重要】
 *   如果 out_port_bindings 非 NULL，调用者必须确保其分配的数组大小
 *   **至少为命题图端口总数的 2 倍**。这是因为每个匹配的端口对占用两个
 *   连续的 int 元素（prop_port_id, const_port_id）。
 *
 *   获取命题图端口数的方法：
 lv_PUBLIC_API *     int port_count = constraint_graph_port_count(proposition);
 lv_PUBLIC_API *     int *buf = malloc(port_count * 2 * sizeof(int));
 *
 *   如果缓冲区过小，函数行为未定义（可能越界写入）。
 *   建议在调用前检查端口数并分配足够的缓冲区，或传入 NULL 以仅获取匹配计数。
 */
lv_PUBLIC_API int unify_match_ports(const ConstraintGraph *construction, const ConstraintGraph *proposition,
                                    int *out_port_bindings, int max_bindings);

/**
 * @brief 单独执行约束匹配
 *
 * 独立于完整合一流程，仅执行命题图约束到构造图约束的匹配检查。
 * 返回匹配成功的约束数量，-1 表示失败。
 *
 * @param[in]  construction             构造图
 * @param[in]  proposition              命题图
 * @param[out] out_constraint_bindings  输出：约束 ID 对数组 [prop_constraint_id, const_constraint_id] 交替排列，
 *                                      大小至少为 proposition 约束数的 2 倍。
 *                                      调用者需分配并传入（可为 NULL 以仅计数）。
 * @return 匹配成功的约束对数（>=0），或 -1 表示错误
 */
lv_PUBLIC_API int unify_match_constraints(const ConstraintGraph *construction, const ConstraintGraph *proposition,
                                          int *out_constraint_bindings);

/**
 * @brief 比较两个几何节点的符号坐标数组是否完全相等
 *
 * 逐个比较两个节点的符号坐标序列。不关心节点的几何类型，
 * 仅检查 symbolic_coords 数组的内容一致性。
 *
 * 检查流程：
 *   1. NULL 检查：任一节点为 NULL 则判定为不相等
 *   2. 坐标数量一致性：coord_count 不同则不可能相等
 *   3. 数组指针检查：若 coord_count > 0 但 symbolic_coords 为 NULL，判定为不相等
 *   4. 逐坐标比较：对每个坐标槽位调用 symbolic_coord_compare
 *
 * @param[in] a 第一个节点
 * @param[in] b 第二个节点
 * @return 1 表示所有坐标均相等，0 表示存在差异或参数无效
 */
lv_PUBLIC_API int unify_coords_equal(const GeomNode *a, const GeomNode *b);

/**
 * @brief 单独执行符号坐标判等
 *
 * 比较两个符号坐标的结构是否相等。
 *
 * @param[in] c1  第一个符号坐标
 * @param[in] c2  第二个符号坐标
 * @return 0 表示相等，非 0 表示不相等或类型不同
 */
lv_PUBLIC_API int unify_match_coords(const SymbolicCoord *c1, const SymbolicCoord *c2);

/* ============== 不匹配位置的具体报告 ============== */

/**
 * @brief 合一失败详细信息
 *
 * 【混合内存管理策略 —— 重要】
 *   UnifyFailureInfo 的字段采用两种不同的内存管理策略：
 *
 *   1. 需要手动释放的字段：
 *      - description: 堆分配的字符串（通常通过 strdup 或 asprintf 分配）。
 *                     **必须**通过 unify_failure_info_destroy() 释放。
 *                     调用者不得直接 free() 此字段，因为 destroy 函数可能
 *                     执行额外的清理操作。
 *
 *   2. 固定大小、无需释放的字段：
 *      - reason_detail[256]: 结构体内嵌的固定大小字符数组。
 *                            不涉及堆分配，**不需要**也不应被释放。
 *                            其生命周期与 UnifyFailureInfo 结构体本身一致。
 *      - status / mismatch_reason: 枚举值，值类型，无需释放。
 *      - failed_constraint_id / failed_node_id / failed_port_index: int 值类型。
 *
 *   释放规则：始终通过 unify_failure_info_destroy() 释放整个结构体，
 *   不要单独释放 description 字段。
 */
typedef struct {
    UnifyStatus status;             /* 失败类型 */
    int failed_constraint_id;       /* 失败的约束 ID（-1 表示无） */
    int failed_node_id;             /* 失败的节点 ID（-1 表示无） */
    int failed_port_index;          /* 失败的端口索引（-1 表示无） */
    char *description;              /* 人类可读的失败描述 */
    MismatchReason mismatch_reason; /* 不匹配的具体原因（精细化分类） */
    char reason_detail[256];        /* 不匹配原因的详细说明文本 */
} UnifyFailureInfo;

/**
 * @brief 释放合一失败信息
 * @param info 失败信息指针
 */
lv_PUBLIC_API void unify_failure_info_destroy(UnifyFailureInfo *info);

/**
 * @brief 带详细失败报告的合一检查
 *
 * 与 unify_construction_with_proposition 功能相同，但在失败时
 * 填充 out_failure 结构体，提供具体的失败位置和原因。
 *
 * @param construction 构造图
 * @param pattern 命题模式图
 * @param out_failure 输出的失败信息（可为 NULL）
 * @return 合一状态
 */
lv_PUBLIC_API UnifyStatus unify_construction_with_proposition_detailed(const ConstraintGraph *construction,
                                                                       const ConstraintGraph *pattern,
                                                                       UnifyFailureInfo *out_failure);

/* ============== 命题的等价变换 ============== */

/**
 * @brief 命题等价声明
 *
 * 定义位于 unify.h（合一域单一事实来源）：unify_equivalence.c 消费，
 * proof.h 经 include 传递可见（proof.h 的 proof_declare_proposition_equivalence
 * 以 prop id 为参，无需完整类型）。
 */
typedef struct PropositionEquivalence {
    int prop_a_id;
    int prop_b_id;
    ConstraintGraph *transformation; /* 双向变换规则 */
} PropositionEquivalence;

/**
 * @brief 声明两个命题等价
 *
 * 等价声明被存储为双向重写规则，在合一前自动应用。
 *
 * @param prop_a_id 命题 A 的 ID
 * @param prop_b_id 命题 B 的 ID
 * @param transformation_rule 变换规则（可为 NULL，表示纯等价声明）
 * @return 是否成功声明
 */
lv_PUBLIC_API bool unify_declare_proposition_equivalence(int prop_a_id, int prop_b_id,
                                                         ConstraintGraph *transformation_rule);

/**
 * @brief 查找命题的等价命题
 *
 * @param prop_id 命题 ID
 * @param equivalent_ids 输出的等价命题 ID 数组
 * @param max_count 数组最大容量
 * @return 找到的等价命题数量
 */
lv_PUBLIC_API int unify_find_equivalent_proposition(int prop_id, int *equivalent_ids, int max_count);

/**
 * @brief 清除所有等价声明
 */
lv_PUBLIC_API void unify_clear_equivalences(void);

/* ============== 等价声明存储管理（v3.4.1 新增） ============== */

/**
 * @brief 初始化等价声明存储系统
 *
 * 线程安全：每个线程有独立的存储实例。
 * 可重复调用，后续调用会重置存储状态。
 */
lv_PUBLIC_API void unify_equivalence_storage_init(void);

/**
 * @brief 清理等价声明存储系统
 *
 * 释放所有等价声明相关资源，重置存储状态。
 *
 * 线程安全：每个线程有独立的存储实例。
 */
lv_PUBLIC_API void lv_unify_equivalence_storage_cleanup(void);

/**
 * @brief 获取当前等价声明数量
 *
 * @return 当前存储的等价声明数量
 *
 * @note 线程安全：返回当前线程存储中的等价声明数量。
 */
lv_PUBLIC_API int unify_equivalence_count(void);

/* ============== 命题的实例化 ============== */

/**
 * @brief 实例化多态命题
 *
 * 将类型变量替换为具体类型区域，生成一个新的实例化命题。
 *
 * @param proposition 原始命题（不修改）
 * @param type_var_node_id 类型变量节点 ID
 * @param concrete_type 具体类型区域
 * @param out_instantiated 输出的实例化命题（调用者负责销毁）
 * @return 是否成功实例化
 */
lv_PUBLIC_API bool unify_instantiate_proposition(ConstraintGraph *proposition, int type_var_node_id,
                                                 const TypeRegion *concrete_type, ConstraintGraph **out_instantiated);

/* 简化的命题结构（用于底层合一检查） */
typedef struct SimpleProposition {
    int id;
    char *name;
    ConstraintGraph *pattern;
    int *input_port_ids;
    int *output_port_ids;
    int input_count;
    int output_count;
} SimpleProposition;

typedef struct SimpleProof {
    int id;
    SimpleProposition *proposition;
    ConstraintGraph *construction;
    bool normalized;
    bool passed;
} SimpleProof;

lv_PUBLIC_API SimpleProposition *simple_proposition_create(const char *name, int *input_port_ids, int input_count,
                                                           int *output_port_ids, int output_count);
lv_PUBLIC_API void simple_proposition_destroy(SimpleProposition *prop);
lv_PUBLIC_API SimpleProof *simple_proof_create(SimpleProposition *prop, ConstraintGraph *construction);
lv_PUBLIC_API void simple_proof_destroy(SimpleProof *proof);

lv_PUBLIC_API bool simple_proof_check(SimpleProof *proof);
lv_PUBLIC_API void simple_proof_normalize(SimpleProof *proof);

#ifdef __cplusplus
}
#endif

/* ============================================================
 * 向后兼容别名（旧名称 → lv_ 前缀新名称）
 * ============================================================ */
#define unify_equivalence_storage_cleanup lv_unify_equivalence_storage_cleanup
#endif /* lv_UNIFY_H */
