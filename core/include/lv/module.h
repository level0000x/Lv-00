/* ========================================================================
 * 模块名称：模块系统 (module)
 * 功能概述：提供模块的创建/销毁、依赖管理、循环依赖检测、
 *          LVZ 文本格式与 MessagePack 二进制格式的序列化/反序列化、
 *          自动保存、崩溃恢复、增量快照以及 SVG/TikZ/PDF 可视化导出。
 *
 * 主要 API：
 *   - module_create / module_destroy                — 创建/销毁模块
 *   - module_add_dependency / validate_chain        — 依赖管理
 *   - module_load / module_save                     — LVZ 文件加载/保存
 *   - module_load_from_binary / save_to_binary      — MessagePack 格式
 *   - module_serialize_to_json / deserialize        — JSON 序列化
 *   - module_set_autosave_config / autosave         — 自动保存
 *   - module_compute_delta / apply_delta            — 增量快照
 *   - module_export_svg / tikz / pdf                — 可视化导出
 *
 * 使用示例：
 lv_PUBLIC_API *   Module *mod = module_create("euclidean", "1.0.0");
 lv_PUBLIC_API *   module_add_dependency(mod, "base", ">=1.0.0");
 lv_PUBLIC_API *   ModuleLoadStatus s = module_load(mod, "euclidean.lvz", NULL, 0);
 *
 * ======================================================================== */

/**
 * @file module.h
 * @brief 模块系统 —— 模块加载/保存、依赖管理、增量快照与崩溃恢复
 */

#ifndef lv_MODULE_H
#define lv_MODULE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "axiom_pkg.h"
#include "lv_utils.h"

/* MAX_MODULE_DEPTH —— 权威定义在 symbolic_coord.h 中
 * 此处使用 #ifndef 守卫防止重复定义。若需修改此值，请修改
 * symbolic_coord.h 中的权威定义，此处的值将自动同步。
 */
#ifndef MAX_MODULE_DEPTH
#define MAX_MODULE_DEPTH 32
#endif

/** 模块实例（不透明类型，内部实现隐藏） */
typedef struct Module Module;

typedef struct ModuleDependency {
    char *name;
    char *version_constraint;
    Module *module;
} ModuleDependency;

typedef struct ModuleExport {
    lvDArray function_block_ids; /**< 函数块 ID 数组（int 元素），count 即导出数量 */
    lvDArray type_region_ids;    /**< 类型区域 ID 数组（int 元素），count 即导出数量 */
} ModuleExport;

typedef enum {
    MODULE_LOAD_OK,                  /* 加载成功 */
    MODULE_LOAD_FILE_NOT_FOUND,      /* 文件未找到 */
    MODULE_LOAD_PARSE_ERROR,         /* 解析错误 */
    MODULE_LOAD_CIRCULAR_DEPENDENCY, /* 循环依赖 */
    MODULE_LOAD_DEPTH_EXCEEDED,      /* 依赖深度超限 */
    MODULE_LOAD_INVALID_LVZ,         /* 无效的 LVZ 格式 */
    MODULE_LOAD_ERROR_INVALID_PATH,  /* 无效的文件路径 */
    MODULE_LOAD_MEMORY_ERROR         /* 内存分配失败 */
} ModuleLoadStatus;

typedef enum {
    MODULE_SAVE_OK,         /* 保存成功 */
    MODULE_SAVE_FILE_ERROR, /* 文件错误 */
    MODULE_SAVE_WRITE_ERROR /* 写入错误 */
} ModuleSaveStatus;

lv_PUBLIC_API Module *module_create(const char *name, const char *version);
lv_PUBLIC_API void module_destroy(Module *mod);

/* ============== 属性访问器 ============== */

/**
 * @brief 获取模块名称
 * @return 模块名称字符串（不可修改，与模块同生命周期）
 */
lv_PUBLIC_API const char *module_get_name(const Module *mod);

/**
 * @brief 获取模块版本
 * @return 版本字符串（不可修改，与模块同生命周期）
 */
lv_PUBLIC_API const char *module_get_version(const Module *mod);

/**
 * @brief 获取模块依赖数量
 * @return 依赖数量
 */
lv_PUBLIC_API int module_get_dependency_count(const Module *mod);

/**
 * @brief 获取模块公理包数量
 * @return 公理包数量
 */
lv_PUBLIC_API int module_get_axiom_package_count(const Module *mod);

/**
 * @brief 获取模块的约束图
 * @return 约束图指针（所有权归模块所有，不可释放），无图时返回 NULL
 */
lv_PUBLIC_API const ConstraintGraph *module_get_graph(const Module *mod);

/**
 * @brief 设置模块的约束图
 * @param mod 模块
 * @param graph 约束图（所有权转移给模块，模块负责释放）
 */
lv_PUBLIC_API void module_set_graph(Module *mod, ConstraintGraph *graph);

/* 流式上下文设置 */
lv_PUBLIC_API void module_set_stream_context(StreamContext *ctx);
lv_PUBLIC_API bool module_add_dependency(Module *mod, const char *dep_name, const char *version_constraint);
lv_PUBLIC_API bool module_add_axiom_package(Module *mod, AxiomPackage *pkg);
lv_PUBLIC_API bool module_export_function_block(Module *mod, int func_block_id);
lv_PUBLIC_API bool module_export_type_region(Module *mod, int type_region_id);

lv_PUBLIC_API ModuleLoadStatus module_load(Module *mod, const char *filepath, Module **loaded_modules,
                                           int module_count);
lv_PUBLIC_API ModuleSaveStatus module_save(const Module *mod, const char *filepath);

lv_PUBLIC_API const char *module_get_last_error(void);

lv_PUBLIC_API char *module_compute_version_hash(const Module *mod);

/**
 * @brief 计算模块的内容 SHA-256 哈希（64 字符十六进制字符串）
 *
 * 哈希内容包括：模块名称、版本、依赖数量+名称+版本约束、
 * 导出数量+ID、公理包数量+名称+版本。
 * 返回值经 lv_calloc 分配，调用者须用 lv_free 释放（[take] 语义；
 * 原注释「free()」为错误——lv_calloc 配 free 是 UB，见
 * docs/architecture/memory-ownership.md K10/F39）。
 *
 * @param mod 模块
 * @return lv_calloc 分配的 65 字节字符串（64 hex + null），失败返回 NULL
 */
lv_PUBLIC_API char *module_compute_content_hash(const Module *mod);

lv_PUBLIC_API bool module_validate_dependency_chain(Module *mod, Module **all_modules, int module_count);
lv_PUBLIC_API bool module_detect_circular_dependency(Module *mod, Module **visited, int visited_count);
lv_PUBLIC_API bool module_full_cycle_detect(Module **modules, int count, int **out_path, int *out_path_len);

lv_PUBLIC_API bool module_parse_version_constraint(const char *constraint, const char *version);
lv_PUBLIC_API int module_compare_versions(const char *v1, const char *v2);

/* ============== 模块文件格式 ============== */

/**
 * @brief 模块文件格式
 */
typedef enum {
    MODULE_FORMAT_LVZ,    /* 文本格式（默认） */
    MODULE_FORMAT_MSGPACK /* MessagePack 二进制格式 */
} ModuleFormat;

/* ============== MessagePack 二进制格式支持 ============== */

/**
 * @brief 从二进制数据加载模块（MessagePack 格式）
 */
lv_PUBLIC_API ModuleLoadStatus module_load_from_binary(const uint8_t *data, size_t size, Module **out_module);

/**
 * @brief 将模块序列化为二进制数据（MessagePack 格式）
 */
lv_PUBLIC_API ModuleSaveStatus module_save_to_binary(const Module *mod, uint8_t **out_data, size_t *out_size);

/* ============== 完整序列化 ============== */

/**
 * @brief 将模块序列化为 JSON 字符串
 */
lv_PUBLIC_API char *module_serialize_to_json(const Module *mod);

/**
 * @brief 从 JSON 字符串反序列化模块
 */
lv_PUBLIC_API ModuleLoadStatus module_deserialize_from_json(const char *json, Module **out_module);

/* ============== 图序列化支持 ============== */

/**
 * @brief 将模块的图序列化为 JSON 字符串
 *
 * 独立序列化模块中的 ConstraintGraph，包含所有节点和约束。
 *
 * @param[in] mod 模块
 * @return JSON 字符串（[take] 调用者负责 lv_free），失败返回 NULL
 */
lv_PUBLIC_API char *module_serialize_graph_to_json(const Module *mod);

/**
 * @brief 从 JSON 字符串反序列化图并设置到模块
 *
 * @param[in] mod 模块
 * @param[in] json JSON 字符串
 * @return 成功返回 true，失败返回 false
 */
lv_PUBLIC_API bool module_deserialize_graph_from_json(Module *mod, const char *json);

/* ============== 自动保存与崩溃恢复 ============== */

/**
 * @brief 模块自动保存配置
 */
typedef struct {
    bool enabled;           /* 是否启用自动保存 */
    int interval_seconds;   /* 保存间隔（秒） */
    char *backup_directory; /* 备份目录路径 */
    int max_backups;        /* 最大备份数 */
} AutoSaveConfig;

/**
 * @brief 设置模块的自动保存配置
 */
lv_PUBLIC_API void module_set_autosave_config(Module *mod, const AutoSaveConfig *config);

/**
 * @brief 执行自动保存
 */
lv_PUBLIC_API ModuleSaveStatus module_autosave(const Module *mod);

/**
 * @brief 从最近的备份恢复模块
 */
lv_PUBLIC_API ModuleLoadStatus module_recover_from_backup(const char *module_name, Module **out_module);

/* ============== 增量存储 ============== */

/**
 * @brief 模块增量快照
 *
 * 与完整序列化不同，增量快照仅记录自上次快照以来的变化。
 */
typedef struct ModuleDelta {
    uint64_t base_version_hash; /* 基线版本哈希 */
    char *delta_data;           /* 增量数据（JSON 格式） */
    size_t delta_size;
} ModuleDelta;

/**
 * @brief 计算模块的增量快照
 */
lv_PUBLIC_API ModuleDelta *module_compute_delta(const Module *mod, uint64_t base_hash);

/**
 * @brief 应用增量快照到模块
 */
lv_PUBLIC_API bool module_apply_delta(Module *mod, const ModuleDelta *delta);

/**
 * @brief 销毁增量快照
 */
lv_PUBLIC_API void module_delta_destroy(ModuleDelta *delta);

/* ============== 可视化导出 ============== */

/**
 * @brief 将模块的约束图导出为 SVG 文件
 *
 * 生成包含节点（点/线段/区域/端口/函数块）和约束的可视化 SVG。
 * 节点使用不同形状和颜色区分类型，约束使用不同线型区分类型。
 *
 * @param mod      模块
 * @param filepath 输出文件路径（.svg）
 * @return true 成功，false 失败
 */
lv_PUBLIC_API bool module_export_svg(const Module *mod, const char *filepath);

/**
 * @brief 将模块的约束图导出为 TikZ 代码文件
 *
 * 生成 LaTeX TikZ 代码，可直接在 LaTeX 文档中 \input 引用。
 * 节点使用不同形状和颜色区分类型，约束使用不同线型区分类型。
 *
 * @param mod      模块
 * @param filepath 输出文件路径（.tex）
 * @return true 成功，false 失败
 */
lv_PUBLIC_API bool module_export_tikz(const Module *mod, const char *filepath);

/**
 * @brief 将模块的约束图导出为可编译的 LaTeX/PDF 文件
 *
 * 生成完整的 LaTeX 文档（包含 documentclass、导言区、TikZ 图），
 * 可通过 pdflatex/xelatex 直接编译为 PDF。
 *
 * @param mod      模块
 * @param filepath 输出文件路径（.tex）
 * @return true 成功，false 失败
 */
lv_PUBLIC_API bool module_export_pdf(const Module *mod, const char *filepath);

#ifdef __cplusplus
}
#endif

#endif /* lv_MODULE_H */