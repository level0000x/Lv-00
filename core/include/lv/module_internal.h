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
