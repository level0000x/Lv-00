/**
 * @file dsl_extension.h
 * @brief 蓝图 DSL 扩展接口（TEN_LAYER_OPTIMIZED_PLAN §4.1.5 落地）
 *
 * DSL 版本语义化解析/比较 + 扩展注册表（parse/codegen 钩子）
 * + 语法转换（按版本区间调用扩展）。
 */

#ifndef lv_DSL_EXTENSION_H
#define lv_DSL_EXTENSION_H

#include <stdbool.h>
#include <stddef.h>

#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief DSL 版本（蓝图 DslVersion） */
typedef struct {
    int major;
    int minor;
    int patch;
} DslVersion;

/** @brief 解析钩子（蓝图 DslParseHook） */
typedef bool (*DslParseHook)(const char *source, size_t source_len, void *ast_out, void *user_data);

/** @brief 代码生成钩子（蓝图 DslCodegenHook） */
typedef bool (*DslCodegenHook)(void *ast, char **output, size_t *output_len, void *user_data);

/** @brief DSL 扩展注册信息（蓝图 DslExtensionRegistration） */
typedef struct {
    const char *name;          /**< 扩展名（唯一键） */
    const char *version;       /**< 版本字符串（semver，可为 NULL） */
    DslParseHook parse_hook;   /**< 解析钩子（可为 NULL） */
    DslCodegenHook codegen_hook; /**< 代码生成钩子（可为 NULL） */
    void *user_data;           /**< 用户数据 */
} DslExtensionRegistration;

/**
 * @brief 注册 DSL 扩展（蓝图 lv_dsl_register_extension）
 *
 * 注册项被复制存储（name/version strdup）；同名已存在返回 false。
 *
 * @param reg 注册信息（非 NULL，name 非空）
 * @return true 注册成功
 */
lv_PUBLIC_API bool lv_dsl_register_extension(const DslExtensionRegistration *reg);

/**
 * @brief 注销 DSL 扩展（蓝图 lv_dsl_unregister_extension）
 * @param name 扩展名
 * @return true 注销成功；false 未注册
 */
lv_PUBLIC_API bool lv_dsl_unregister_extension(const char *name);

/**
 * @brief 解析语义化版本字符串（蓝图 lv_dsl_version_parse）
 *
 * 支持 "major.minor.patch" 与 "major.minor"（patch 缺省为 0），
 * 允许前缀 "v"/"V"。解析失败返回 false。
 *
 * @param version_str 版本字符串（非 NULL）
 * @param out_version 输出版本（非 NULL）
 * @return true 解析成功
 */
lv_PUBLIC_API bool lv_dsl_version_parse(const char *version_str, DslVersion *out_version);

/**
 * @brief 比较两个版本（蓝图 lv_dsl_version_compare）
 *
 * @param a          版本 a（非 NULL）
 * @param b          版本 b（非 NULL）
 * @param out_result 输出比较结果：a<b → -1，a==b → 0，a>b → 1
 * @return true 成功；false 参数无效
 */
lv_PUBLIC_API bool lv_dsl_version_compare(const DslVersion *a, const DslVersion *b, int *out_result);

/**
 * @brief 语法转换（蓝图 lv_dsl_syntax_transform）
 *
 * 若 from_version 与 to_version 相等：源串原样复制（无迁移）；
 * 否则遍历已注册扩展，调用首个 parse+codegen 钩子齐全者做转换。
 * 转换输出 [take] 调用者负责 lv_free。
 *
 * @param source        源 DSL 文本（非 NULL）
 * @param from_version  源版本（可为 NULL 视为不迁移）
 * @param to_version    目标版本（可为 NULL）
 * @param out_transformed 输出转换后文本（[take]）
 * @return true 成功（含同版本无迁移）；false 参数无效 / 无可用扩展
 */
lv_PUBLIC_API bool lv_dsl_syntax_transform(const char *source, const DslVersion *from_version,
                                           const DslVersion *to_version, char **out_transformed);

#ifdef __cplusplus
}
#endif

#endif /* lv_DSL_EXTENSION_H */
