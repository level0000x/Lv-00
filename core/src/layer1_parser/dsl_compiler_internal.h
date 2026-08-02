/**
 * @file dsl_compiler_internal.h
 * @brief DSL 编译器内部共享声明（dsl_compiler.c 拆分模块共用）
 *
 * @details 供 dsl_compiler_parse.c / dsl_compiler_ir.c / dsl_compiler_load.c
 *          三个编译阶段模块共享内部宏。
 */

#ifndef lv_DSL_COMPILER_INTERNAL_H
#define lv_DSL_COMPILER_INTERNAL_H

/* 安全扩容宏：通用动态数组扩容（parse 阶段与 IR 生成阶段共用） */
#define ENSURE_CAP(arr, count, cap, elem_sz, ret_on_fail)          \
    do {                                                           \
        if ((count) >= (cap)) {                                    \
            size_t _new_cap = (cap) == 0 ? 8 : (size_t) (cap) * 2; \
            void *_np = lv_realloc((arr), _new_cap * (elem_sz));   \
            if (!_np)                                              \
                return (ret_on_fail);                              \
            (arr) = _np;                                           \
            (cap) = (int) _new_cap;                                \
        }                                                          \
    } while (0)

#endif /* lv_DSL_COMPILER_INTERNAL_H */
