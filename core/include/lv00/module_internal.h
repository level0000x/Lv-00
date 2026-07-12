/**
 * @file module_internal.h
 * @brief 模块系统内部头文件 —— struct Module 完整定义及内部辅助函数声明
 *
 * 仅限模块子系统内部使用，外部代码不应包含此头文件。
 * 外部代码应使用 lv00/module.h 中的不透明指针 API。
 */

#ifndef LV00_MODULE_INTERNAL_H
#define LV00_MODULE_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv00/module.h"
#include "lv00/constraint_graph.h"

/* ============== 模块实例结构体完整定义 ============== */

struct Module {
    char *name;
    char *version;
    ModuleDependency *dependencies;
    int dependency_count;
    ModuleExport *exports;
    AxiomPackage **axiom_packages;
    int axiom_package_count;
    ConstraintGraph *graph;
};

/* ============== 内部辅助函数声明 ============== */

/**
 * @brief 按模块名查找已有的自动保存配置（不创建新条目）
 */
AutoSaveConfig *find_autosave_config(const char *module_name);

/**
 * @brief 按模块名查找或创建自动保存配置
 */
AutoSaveConfig *get_or_create_autosave_config(const char *module_name);

#ifdef __cplusplus
}
#endif

#endif /* LV00_MODULE_INTERNAL_H */
