/**
 * @file func_block_preset_ops.h
 * @brief 预设函数块操作接口 - 提供高级预设操作功能
 *
 * 本模块提供对预设函数块的高级操作，包括：
 * - 预设的组合与链式调用
 * - 预设的批量实例化
 * - 预设的参数绑定与部分应用
 * - 预设的验证与测试
 *
 * 设计原则：
 * - 所有操作都是函数式的，不修改原始预设
 * - 支持链式调用，便于构建复杂的数学构造
 * - 提供详细的错误信息和调试支持
 */

#ifndef LV00_FUNC_BLOCK_PRESET_OPS_H
#define LV00_FUNC_BLOCK_PRESET_OPS_H

#include <stdbool.h>

#include "func_block.h"
#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 预设链式调用
 * ================================================================ */

/**
 * @brief 预设链式调用结构
 *
 * 用于构建多个预设的顺序调用链。
 */
typedef struct PresetChain PresetChain;

/**
 * @brief 创建预设链
 *
 * @return 新的预设链，失败返回 NULL
 */
PresetChain *preset_chain_create(void);

/**
 * @brief 销毁预设链
 *
 * @param chain 预设链
 */
void preset_chain_destroy(PresetChain *chain);

/**
 * @brief 向链中添加预设
 *
 * @param chain 预设链
 * @param preset_name 预设名称
 * @param input_mapping 输入映射（指定前一输出到本输入的对应关系）
 * @return true 添加成功
 */
bool preset_chain_add(PresetChain *chain, const char *preset_name, const int *input_mapping);

/**
 * @brief 执行预设链
 *
 * @param chain 预设链
 * @param graph 约束图
 * @param initial_args 初始参数
 * @param arg_count 参数数量
 * @param out_final_outputs 输出最终结果
 * @param out_output_count 输出结果数量
 * @return true 执行成功
 */
bool preset_chain_execute(PresetChain *chain, ConstraintGraph *graph, const int *initial_args, int arg_count,
                          int **out_final_outputs, int *out_output_count);

/* ================================================================
 * 预设批量操作
 * ================================================================ */

/**
 * @brief 批量实例化预设
 *
 * 一次性实例化多个相同的预设。
 *
 * @param preset_name 预设名称
 * @param graph 约束图
 * @param arg_sets 参数集合数组（每个元素是一个参数数组）
 * @param set_count 集合数量
 * @param args_per_set 每个集合的参数数量
 * @param out_results 输出结果数组（每个实例的输出节点ID）
 * @return true 批量实例化成功
 */
bool preset_batch_instantiate(const char *preset_name, ConstraintGraph *graph, const int **arg_sets, int set_count,
                              int args_per_set, int ***out_results);

/**
 * @brief 批量应用预设到节点集合
 *
 * 将一个预设应用到多个不同的输入节点组合。
 *
 * @param preset_name 预设名称
 * @param graph 约束图
 * @param target_nodes 目标节点数组
 * @param node_count 节点数量
 * @param out_results 输出结果数组
 * @return true 批量应用成功
 */
bool preset_batch_apply(const char *preset_name, ConstraintGraph *graph, const int *target_nodes, int node_count,
                        int ***out_results);

/* ================================================================
 * 预设验证与测试
 * ================================================================ */

/**
 * @brief 预设验证结果
 */
typedef struct {
    bool is_valid;                 /* 预设是否有效 */
    bool has_valid_inputs;         /* 输入定义是否有效 */
    bool has_valid_outputs;        /* 输出定义是否有效 */
    bool determinism_check_passed; /* 确定性检查是否通过 */
    char *error_message;           /* 错误信息（如果有） */
} PresetValidationResult;

/**
 * @brief 验证预设的有效性
 *
 * @param preset_name 预设名称
 * @return 验证结果（调用者负责释放 error_message）
 */
PresetValidationResult preset_validate(const char *preset_name);

/**
 * @brief 释放验证结果
 *
 * @param result 验证结果
 */
void preset_validation_result_free(PresetValidationResult *result);

/**
 * @brief 测试预设的实例化
 *
 * 使用测试参数验证预设是否可以正确实例化。
 *
 * @param preset_name 预设名称
 * @param graph 约束图
 * @param test_args 测试参数
 * @param arg_count 参数数量
 * @return true 测试通过
 */
bool preset_test_instantiation(const char *preset_name, ConstraintGraph *graph, const int *test_args, int arg_count);

/* ================================================================
 * 预设参数操作
 * ================================================================ */

/**
 * @brief 预设参数绑定结构
 */
typedef struct {
    int param_index;   /* 参数索引 */
    int bound_node_id; /* 绑定的节点ID */
    bool is_bound;     /* 是否已绑定 */
} PresetParamBinding;

/**
 * @brief 创建参数绑定数组
 *
 * @param preset_name 预设名称
 * @param out_bindings 输出绑定数组
 * @param out_count 输出绑定数量
 * @return true 创建成功
 */
bool preset_create_bindings(const char *preset_name, PresetParamBinding **out_bindings, int *out_count);

/**
 * @brief 释放参数绑定数组
 *
 * @param bindings 绑定数组
 */
void preset_bindings_free(PresetParamBinding *bindings);

/**
 * @brief 应用部分绑定创建新预设
 *
 * @param preset_name 原预设名称
 * @param bindings 参数绑定数组
 * @param binding_count 绑定数量
 * @param out_new_preset_name 输出新预设名称（自动生成）
 * @return true 创建成功
 */
bool preset_partial_bind(const char *preset_name, const PresetParamBinding *bindings, int binding_count,
                         char **out_new_preset_name);

/* ================================================================
 * 预设搜索与推荐
 * ================================================================ */

/**
 * @brief 预设搜索结果
 */
typedef struct {
    char **names;             /* 预设名称数组 */
    double *relevance_scores; /* 相关度分数 */
    int count;                /* 结果数量 */
} PresetSearchResult;

/**
 * @brief 基于输入输出数量的预设搜索
 *
 * 查找具有特定输入输出端口数量的预设。
 *
 * @param input_count 输入端口数量（-1表示任意）
 * @param output_count 输出端口数量（-1表示任意）
 * @param out_result 搜索结果
 * @return 找到的结果数量
 */
int preset_search_by_signature(int input_count, int output_count, PresetSearchResult *out_result);

/**
 * @brief 基于类别的预设推荐
 *
 * 推荐与给定预设同类别或相关类别的其他预设。
 *
 * @param preset_name 参考预设名称
 * @param out_result 推荐结果
 * @return 推荐数量
 */
int preset_recommend_related(const char *preset_name, PresetSearchResult *out_result);

/**
 * @brief 释放搜索结果
 *
 * @param result 搜索结果
 */
void preset_search_result_free(PresetSearchResult *result);

/* ================================================================
 * 预设组合操作
 * ================================================================ */

/**
 * @brief 预设组合模式
 */
#ifndef LV00_PRESET_COMPOSE_MODE_DEFINED
#define LV00_PRESET_COMPOSE_MODE_DEFINED
typedef enum {
    PRESET_COMPOSE_SEQUENCE, /* 顺序组合：f -> g */
    PRESET_COMPOSE_PARALLEL, /* 并行组合：f | g */
    PRESET_COMPOSE_FEEDBACK, /* 反馈组合：f 的输出反馈到输入 */
    PRESET_COMPOSE_BRANCH,   /* 分支组合：条件选择 f 或 g */
    PRESET_COMPOSE_PIPE      /* 管道组合：数据流管道 */
} PresetComposeMode;
#endif /* LV00_PRESET_COMPOSE_MODE_DEFINED */

/**
 * @brief 创建组合预设
 *
 * 将两个预设按指定模式组合成新预设。
 *
 * @param preset_a 第一个预设
 * @param preset_b 第二个预设
 * @param mode 组合模式
 * @param out_composed_name 输出组合预设名称
 * @return true 组合成功
 */
/* preset_compose 已在 preset_manager.c 中声明，
 * func_block_preset_ops.c 中的版本为 static 内部实现 */

/**
 * @brief 创建递归预设
 *
 * 创建一个递归调用的预设（用于迭代构造）。
 *
 * @param base_preset 基础预设
 * @param max_iterations 最大迭代次数
 * @param out_recursive_name 输出递归预设名称
 * @return true 创建成功
 */
bool preset_make_recursive(const char *base_preset, int max_iterations, char **out_recursive_name);

#ifdef __cplusplus
}
#endif

#endif /* LV00_FUNC_BLOCK_PRESET_OPS_H */
