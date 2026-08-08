/**
 * @file module_internal.h
 * @brief 模块系统内部头文件 —— struct Module 完整定义及内部辅助函数声明
 *
 * 仅限模块子系统内部使用，外部代码不应包含此头文件。
 * 外部代码应使用 lv/module.h 中的不透明指针 API。
 */

#ifndef lv_MODULE_INTERNAL_H
#define lv_MODULE_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv/constraint_graph.h"
#include "lv/module.h"

/* ============== 模块实例结构体完整定义 ============== */

struct Module {
    char *name;
    char *version;
    lvDArray dependencies;   /**< 依赖数组（ModuleDependency 元素） */
    lvDArray axiom_packages; /**< 公理包指针数组（AxiomPackage* 元素） */
    ModuleExport *exports;
    ConstraintGraph *graph;
};

/* ============== Module 序列化字段 X-macro（单一事实来源） ==============
 *
 * 统一描述 Module 的可序列化字段清单，供四种序列化路径共享，消除
 * "同一对象多处独立列举字段、新增字段需同步多处"的漂移风险：
 *   - LVZ 文本（module_serialize.c module_save / module_lvz.c lvz_parse）
 *   - MessagePack 二进制（module_serialize_msgpack.c）
 *   - JSON（module_serialize_json.c）
 *   - 版本哈希（module_serialize.c module_compute_version_hash）
 *
 * 各格式的字段处理函数表由本宏生成（用法见各模块文件）：
 *   #define LV_MODULE_XXX_ENTRY(field) { #field, xxx_##field },
 *   static const ... kTable[] = { LV_MODULE_FIELD_X(LV_MODULE_XXX_ENTRY) };
 *
 * 新增字段时的强制同步机制：在此登记字段名后，各格式的 handler 函数
 * （xxx_<field>）必须一并补齐，否则生成表的函数引用未定义 → 编译期报错，
 * 杜绝静默漂移。字段顺序即各格式输出顺序的基准。
 *
 * 取舍说明：字段含复杂类型（dependencies/exports/axiom_packages 为动态数组、
 * graph 为约束图指针），故各字段的编解码 handler 体保持手写（格式相关，
 * 无法宏化），但"字段枚举/键分发/哈希遍历"一律由宏生成；msgpack 二进制
 * 格式按历史约定不含 graph（槽位置 NULL 跳过），LVZ 节式格式在
 * module_save 中标注派生。
 */
#define LV_MODULE_FIELD_X(X) \
    X(name) \
    X(version) \
    X(dependencies) \
    X(exports) \
    X(axiom_packages) \
    X(graph)

/* ============== 内部辅助函数声明 ============== */

/**
 * @brief 按模块名查找已有的自动保存配置（不创建新条目）
 */
AutoSaveConfig *find_autosave_config(const char *module_name);

/**
 * @brief 按模块名查找或创建自动保存配置
 */
AutoSaveConfig *get_or_create_autosave_config(const char *module_name);

/**
 * @brief 释放全部自动保存配置（供程序退出或模块卸载时调用）
 */
void module_autosave_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* lv_MODULE_INTERNAL_H */
