/**
 * @file dsl_compiler_internal.h
 * @brief DSL 编译器内部共享声明（dsl_compiler.c 拆分模块共用）
 *
 * @details 供 dsl_compiler_parse.c / dsl_compiler_ir.c / dsl_compiler_load.c
 *          三个编译阶段模块共享内部宏。
 */

#ifndef lv_DSL_COMPILER_INTERNAL_H
#define lv_DSL_COMPILER_INTERNAL_H

/* 注：动态数组扩容统一使用 lv/lv_utils.h 中的 lv_ENSURE_ARRAY_CAP，
 * 不再在此重复定义 ENSURE_CAP（原定义已移除）。 */

#endif /* lv_DSL_COMPILER_INTERNAL_H */
