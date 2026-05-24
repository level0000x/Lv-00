/**
 * @file unify.h
 * @brief 合一检查 —— 构造图与命题图的匹配验证
 * @details 提供构造图与命题图的合一检查（基础版、坐标增强版、哈希预过滤版）、
 * 详细失败报告、命题等价声明与查找、多态命题实例化以及简化命题/证明结构。
 */

#ifndef LV00_UNIFY_H
#define LV00_UNIFY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "constraint_graph.h"
#include "stream.h"
#include "type_system.h"

/**
 * @brief 设置合一检查器的流式输出上下文
 * @param ctx  流式上下文（可为 NULL 以禁用流式输出）
 */
void unify_set_stream_context(StreamContext *ctx);

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

UnifyStatus unify_construction_with_proposition(ConstraintGraph *construction, ConstraintGraph *proposition);

/* 带坐标级别相等检查的合一（增强版）
 * 在约束匹配阶段，除了检查约束类型和参与者 ID，
 * 还验证对应参与者的符号坐标是否相等。
 * 返回 UNIFY_STATUS_COORD_MISMATCH 如果坐标不匹配。 */
UnifyStatus unify_construction_with_proposition_coord(ConstraintGraph *construction, ConstraintGraph *proposition);

/* 带哈希预过滤的合一（优化版）
 * 在约束匹配前，使用 symbolic_coord_hash() 对节点分组，
 * 只比较相同哈希组的节点，加速匹配过程。 */
UnifyStatus unify_construction_with_proposition_hash_filtered(ConstraintGraph *construction,
                                                              ConstraintGraph *proposition);

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
 */
int unify_match_ports(const ConstraintGraph *construction, const ConstraintGraph *proposition, int *out_port_bindings);

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
int unify_match_constraints(const ConstraintGraph *construction, const ConstraintGraph *proposition,
                            int *out_constraint_bindings);

/**
 * @brief 单独执行符号坐标判等
 *
 * 比较两个符号坐标的结构是否相等。
 *
 * @param[in] c1  第一个符号坐标
 * @param[in] c2  第二个符号坐标
 * @return 0 表示相等，非 0 表示不相等或类型不同
 */
int unify_match_coords(const SymbolicCoord *c1, const SymbolicCoord *c2);

/* ============== 不匹配位置的具体报告 ============== */

/**
 * @brief 合一失败详细信息
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
void unify_failure_info_destroy(UnifyFailureInfo *info);

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
UnifyStatus unify_construction_with_proposition_detailed(ConstraintGraph *construction, ConstraintGraph *pattern,
                                                         UnifyFailureInfo *out_failure);

/* ============== 命题的等价变换 ============== */

/* PropositionEquivalence 在 proof.h 中定义 */

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
bool unify_declare_proposition_equivalence(int prop_a_id, int prop_b_id, ConstraintGraph *transformation_rule);

/**
 * @brief 查找命题的等价命题
 *
 * @param prop_id 命题 ID
 * @param equivalent_ids 输出的等价命题 ID 数组
 * @param max_count 数组最大容量
 * @return 找到的等价命题数量
 */
int unify_find_equivalent_proposition(int prop_id, int *equivalent_ids, int max_count);

/**
 * @brief 清除所有等价声明
 */
void unify_clear_equivalences(void);

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
bool unify_instantiate_proposition(ConstraintGraph *proposition, int type_var_node_id, const TypeRegion *concrete_type,
                                   ConstraintGraph **out_instantiated);

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

SimpleProposition *simple_proposition_create(const char *name, int *input_port_ids, int input_count,
                                             int *output_port_ids, int output_count);
void simple_proposition_destroy(SimpleProposition *prop);
SimpleProof *simple_proof_create(SimpleProposition *prop, ConstraintGraph *construction);
void simple_proof_destroy(SimpleProof *proof);

bool simple_proof_check(SimpleProof *proof);
void simple_proof_normalize(SimpleProof *proof);

#ifdef __cplusplus
}
#endif

#endif /* LV00_UNIFY_H */
