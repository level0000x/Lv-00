/**
 * @file rel_atom_type.h
 * @brief 关系原子类型枚举（RelAtomType）的单一事实源
 *
 * 关系模型（relation_model.h）与 SAT 编码（sat_encoding.h）共用同一枚举，
 * 消除二者曾各自维护一份、且 99 号哨兵命名漂移（UNKNOWN vs CUSTOM）的隐患。
 * 本头文件只含枚举，不引入任何结构体，避免两套同名结构体（RelAtom 等）冲突。
 *
 * @version 3.3.0
 * @date 2026-08-13
 */

#ifndef lv_REL_ATOM_TYPE_H
#define lv_REL_ATOM_TYPE_H

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 关系原子类型。值 0-4 为具体类型；99 为未知/自定义哨兵。 */
typedef enum {
    REL_ATOM_POINT = 0,      /**< 几何点 */
    REL_ATOM_LINE = 1,       /**< 线段 */
    REL_ATOM_REGION = 2,     /**< 区域 */
    REL_ATOM_PORT = 3,       /**< 端口 */
    REL_ATOM_FUNC_BLOCK = 4, /**< 函数块 */
    REL_ATOM_UNKNOWN = 99    /**< 未知/自定义类型哨兵 */
} RelAtomType;

#ifdef __cplusplus
}
#endif

#endif /* lv_REL_ATOM_TYPE_H */
