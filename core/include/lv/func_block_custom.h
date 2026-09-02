/**
 * @file func_block_custom.h
 * @brief 蓝图自定义函数注册接口（TEN_LAYER_OPTIMIZED_PLAN §4.1.2 落地）
 *
 * 提供运行时注册/注销/查询「自定义函数」（名称 → 回调映射），
 * 供插件扩展函数块语义。规划文档类型名 CustomFunctionMeta /
 * CustomFunctionRegistration 此处定义；函数名为 lv_func_block_*。
 */

#ifndef lv_FUNC_BLOCK_CUSTOM_H
#define lv_FUNC_BLOCK_CUSTOM_H

#include <stdbool.h>
#include <stddef.h>

#include "constraint_graph.h"
#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明（避免与 constraint_graph.h 重复） */
struct GeomNode;

/** @brief 自定义函数元数据（蓝图 CustomFunctionMeta） */
typedef struct {
    const char *name;         /**< 函数名（唯一键） */
    const char *description;  /**< 描述（可为 NULL） */
    const char *category;     /**< 类别（可为 NULL） */
    int min_inputs;           /**< 最小输入数 */
    int max_inputs;           /**< 最大输入数 */
    int output_count;         /**< 输出数 */
    const char **input_types;  /**< 输入类型名称数组（可为 NULL） */
    const char **output_types; /**< 输出类型名称数组（可为 NULL） */
    const char **param_names;  /**< 参数名称数组（可为 NULL） */
} CustomFunctionMeta;

/** @brief 自定义函数回调签名（蓝图 CustomFunctionCallback） */
typedef bool (*CustomFunctionCallback)(ConstraintGraph *graph, const int *input_node_ids, int input_count,
                                       int **output_node_ids, int *output_count, void *user_data);

/** @brief 自定义函数注册信息（蓝图 CustomFunctionRegistration） */
typedef struct {
    CustomFunctionMeta meta;     /**< 元数据 */
    CustomFunctionCallback callback; /**< 回调（非 NULL） */
    void *user_data;             /**< 用户数据 */
    void (*free_user_data)(void *); /**< user_data 释放回调（可为 NULL） */
} CustomFunctionRegistration;

/** @brief 批量注册表（蓝图 CustomFunctionRegistry） */
typedef struct {
    CustomFunctionRegistration *registrations; /**< 注册项数组 */
    size_t count;                              /**< 数组长度 */
} CustomFunctionRegistry;

/**
 * @brief 注册自定义函数（蓝图 lv_func_block_register_custom）
 *
 * 注册项被复制存储（meta 字符串 strdup），注册后调用方可释放原结构。
 * 同名已存在时返回 false（覆盖需先 unregister）。
 *
 * @param reg 注册信息（非 NULL，callback 非 NULL）
 * @return true 注册成功；false 参数无效 / 同名已存在 / 内存不足
 */
lv_PUBLIC_API bool lv_func_block_register_custom(const CustomFunctionRegistration *reg);

/**
 * @brief 注销自定义函数（蓝图 lv_func_block_unregister_custom）
 * @param name 函数名
 * @return true 注销成功；false 未注册过该名
 */
lv_PUBLIC_API bool lv_func_block_unregister_custom(const char *name);

/**
 * @brief 查询自定义函数是否已注册（蓝图 lv_func_block_is_custom_registered）
 * @param name 函数名
 * @return true 已注册
 */
lv_PUBLIC_API bool lv_func_block_is_custom_registered(const char *name);

/**
 * @brief 获取自定义函数元数据（蓝图 lv_func_block_get_custom_meta）
 *
 * 返回指向注册表内部存储的元数据，勿修改或释放。
 *
 * @param name 函数名
 * @return 元数据指针；未找到返回 NULL
 */
lv_PUBLIC_API const CustomFunctionMeta *lv_func_block_get_custom_meta(const char *name);

/**
 * @brief 批量注册自定义函数（蓝图 lv_func_block_register_custom_batch）
 *
 * 逐个调用 lv_func_block_register_custom；任一项失败即停止并返回 false
 * （已成功项保留）。
 *
 * @param registry 批量注册表（非 NULL）
 * @return true 全部注册成功
 */
lv_PUBLIC_API bool lv_func_block_register_custom_batch(const CustomFunctionRegistry *registry);

/**
 * @brief 批量注销自定义函数（蓝图 lv_func_block_unregister_custom_batch）
 *
 * 逐个注销；全部成功返回 true，任一未注册返回 false（其余已处理）。
 *
 * @param names 名称数组（非 NULL）
 * @param count 数量
 * @return true 全部注销成功
 */
lv_PUBLIC_API bool lv_func_block_unregister_custom_batch(const char **names, size_t count);

/**
 * @brief 执行自定义函数（内部/插件桥接用）
 *
 * 按名称查找并调用回调。此函数为注册表的能力出口（库内其他模块
 * 通过它驱动自定义函数），非规划文档命名，供桥接使用。
 *
 * @param name      函数名
 * @param graph     约束图
 * @param inputs    输入节点 ID 数组
 * @param input_count 输入数
 * @param outputs   输出节点 ID 数组（[take] 由回调分配，调用者 lv_free）
 * @param output_count 输出数
 * @return true 执行成功；false 未注册或回调失败
 */
lv_PUBLIC_API bool lv_func_block_call_custom(const char *name, ConstraintGraph *graph, const int *inputs,
                                             int input_count, int **outputs, int *output_count);

#ifdef __cplusplus
}
#endif

#endif /* lv_FUNC_BLOCK_CUSTOM_H */
