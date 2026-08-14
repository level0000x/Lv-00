/**
 * @file engine_access.h
 * @brief 引擎跨域访问器（轻量头）—— 供 proof 等依赖域读取引擎数据，不引入完整 lvEngine 结构体
 *
 * @details 环 B（proof↔engine）破环产物：proof 域文件曾直接 include engine.h 访问
 * lvEngine 字段（如 axiom_package_count / axiom_packages），形成 proof→engine 目录级
 * include 边。本头仅前向声明 lvEngine 与 AxiomPackage，声明引擎域实现的访问器，
 * 使 proof 域可经访问器读取引擎数据而无需 lvEngine 完整定义（engine.h 也明确要求
 * 外部代码使用访问器而非直访字段）。
 *
 * 实现位于 core/src/layer4_reasoning/engine/engine.c。
 */

#ifndef lv_ENGINE_ACCESS_H
#define lv_ENGINE_ACCESS_H

typedef struct lvEngine lvEngine;
typedef struct AxiomPackage AxiomPackage;

#ifndef lv_PUBLIC_API
#define lv_PUBLIC_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 获取引擎已加载的公理包数量
 * @param engine 引擎实例（可为 NULL，返回 0）
 * @return 公理包数量
 */
lv_PUBLIC_API int engine_get_axiom_package_count(const lvEngine *engine);

/**
 * @brief 获取引擎第 index 个已加载公理包
 * @param engine 引擎实例（可为 NULL，返回 NULL）
 * @param index  索引（越界返回 NULL）
 * @return 公理包指针，失败返回 NULL
 */
lv_PUBLIC_API AxiomPackage *engine_get_axiom_package(const lvEngine *engine, int index);

#ifdef __cplusplus
}
#endif

#endif /* lv_ENGINE_ACCESS_H */
