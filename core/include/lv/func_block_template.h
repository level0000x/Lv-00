/**
 * @file func_block_template.h
 * @brief 蓝图函数块模板系统（TEN_LAYER_OPTIMIZED_PLAN §4.1.3 落地）
 *
 * 模板 = 名称/描述/参数/脚本/版本/依赖 的声明式定义；注册后可按名
 * 查询/实例化（实例化基于 func_block_create + func_block_instantiate，
 * 脚本/参数为元数据承载，不引入模板脚本解释器）。
 */

#ifndef lv_FUNC_BLOCK_TEMPLATE_H
#define lv_FUNC_BLOCK_TEMPLATE_H

#include <stdbool.h>
#include <stddef.h>

#include "constraint_graph.h"
#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 模板参数描述（蓝图 FuncBlockTemplateParam） */
typedef struct {
    char name[64];             /**< 参数名 */
    char type[64];             /**< 参数类型（如 "POINT"） */
    char default_value[256];   /**< 默认值字符串（可为空） */
    bool required;             /**< 是否必填 */
    const char *description;   /**< 描述（可为 NULL） */
} FuncBlockTemplateParam;

/** @brief 函数块模板（蓝图 FuncBlockTemplate，不透明句柄） */
typedef struct FuncBlockTemplate FuncBlockTemplate;

/**
 * @brief 创建模板（蓝图 lv_fb_template_create）
 *
 * 返回空模板句柄（名称/描述副本）；完成后需 lv_fb_template_destroy。
 *
 * @param name        模板名（唯一键，非 NULL）
 * @param description 描述（可为 NULL）
 * @return 模板句柄；失败返回 NULL
 */
lv_PUBLIC_API FuncBlockTemplate *lv_fb_template_create(const char *name, const char *description);

/**
 * @brief 销毁模板句柄（蓝图 lv_fb_template_destroy）
 *
 * 仅释放未注册的临时模板；已注册模板须先 lv_fb_template_unregister。
 * NULL 安全。
 */
lv_PUBLIC_API void lv_fb_template_destroy(FuncBlockTemplate *tmpl);

/**
 * @brief 添加参数（蓝图 lv_fb_template_add_param）
 *
 * 参数被复制；模板注册前可多次添加。
 *
 * @param tmpl  模板（非 NULL）
 * @param param 参数（非 NULL，name 非空）
 * @return true 成功；false 参数无效 / 内存不足
 */
lv_PUBLIC_API bool lv_fb_template_add_param(FuncBlockTemplate *tmpl, const FuncBlockTemplateParam *param);

/**
 * @brief 设置脚本（蓝图 lv_fb_template_set_script）
 *
 * 脚本作为元数据保存（本实现不解释脚本内容；实例化走 func_block_instantiate）。
 *
 * @param tmpl   模板（非 NULL）
 * @param script 脚本字符串（可为 NULL 清空）
 * @return true 成功
 */
lv_PUBLIC_API bool lv_fb_template_set_script(FuncBlockTemplate *tmpl, const char *script);

/**
 * @brief 设置版本（蓝图 lv_fb_template_set_version）
 * @param tmpl    模板（非 NULL）
 * @param version 版本字符串（可为 NULL 清空）
 * @return true 成功
 */
lv_PUBLIC_API bool lv_fb_template_set_version(FuncBlockTemplate *tmpl, const char *version);

/**
 * @brief 添加依赖（蓝图 lv_fb_template_add_dependency）
 *
 * 依赖名被复制；重复添加去重。
 *
 * @param tmpl    模板（非 NULL）
 * @param dep_name 依赖名（非 NULL）
 * @return true 成功
 */
lv_PUBLIC_API bool lv_fb_template_add_dependency(FuncBlockTemplate *tmpl, const char *dep_name);

/**
 * @brief 注册模板到全局注册表（蓝图 lv_fb_template_register）
 *
 * 注册后模板归注册表所有（句柄仍可查询字段，但不得再修改或单独 destroy）。
 * 同名已存在时返回 false。
 *
 * @param tmpl 模板（非 NULL）
 * @return true 注册成功
 */
lv_PUBLIC_API bool lv_fb_template_register(FuncBlockTemplate *tmpl);

/**
 * @brief 按名称查询模板（蓝图 lv_fb_template_query）
 *
 * 返回注册表内部模板指针（只读，勿修改/释放）。
 *
 * @param name 模板名
 * @return 模板指针；未找到返回 NULL
 */
lv_PUBLIC_API FuncBlockTemplate *lv_fb_template_query(const char *name);

/**
 * @brief 注销模板（蓝图 lv_fb_template_unregister）
 *
 * 从注册表移除并释放模板。
 *
 * @param name 模板名
 * @return true 注销成功；false 未注册
 */
lv_PUBLIC_API bool lv_fb_template_unregister(const char *name);

/** @brief 实例化参数（蓝图 FuncBlockInstantiationArgs） */
typedef struct {
    int *input_node_ids;   /**< 输入节点 ID 数组（可为 NULL 当 input_count 为 0） */
    int input_count;       /**< 输入数 */
    const char **param_values; /**< 参数值数组（可为 NULL） */
    int param_count;       /**< 参数值数 */
} FuncBlockInstantiationArgs;

/**
 * @brief 实例化模板（蓝图 lv_fb_template_instantiate）
 *
 * 按模板名创建 FuncBlock（名称/描述），以 args 的输入节点 ID 为
 * 实参调用 func_block_instantiate。返回新函数块在图中创建的节点 ID
 * （函数块节点）；失败返回 -1。
 *
 * @param template_name 模板名
 * @param graph         约束图（非 NULL）
 * @param args          实例化参数（可为 NULL 表示无输入）
 * @return 新函数块节点 ID；失败返回 -1
 */
lv_PUBLIC_API int lv_fb_template_instantiate(const char *template_name, ConstraintGraph *graph,
                                             const FuncBlockInstantiationArgs *args);

#ifdef __cplusplus
}
#endif

#endif /* lv_FUNC_BLOCK_TEMPLATE_H */
