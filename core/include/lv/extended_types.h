#ifndef lv_EXTENDED_TYPES_H
#define lv_EXTENDED_TYPES_H

#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
#include "lv/effect_system.h"
#include "lv/type_system.h"

#ifdef __cplusplus
extern "C" {
#endif

/* List type region */
typedef struct lvListTypeRegion {
    void *base; /* TypeRegion base */
    void *elem_type;
} lvListTypeRegion;

/* Map type region */
typedef struct lvMapTypeRegion {
    void *base;
    void *key_type;
    void *value_type;
} lvMapTypeRegion;

/* Function type region (dependent) */
typedef struct lvFunctionTypeRegion {
    void *base;
    void *param_type;
    void *return_type;
    int is_dependent;
} lvFunctionTypeRegion;

/* Effect type region */
typedef struct lvEffectTypeRegion {
    void *base;
    lvEffectType *effects;
    int effect_count;
    void *result_type;
} lvEffectTypeRegion;

/* Factory */

/**
 * @brief 创建列表类型区域
 * @param elem_type 列表元素类型指针
 * @return 成功返回列表类型区域指针，失败返回 NULL
 */
lvListTypeRegion *lv_list_type_create(void *elem_type);

/**
 * @brief 销毁列表类型区域并释放资源
 * @param t 指向待销毁的列表类型区域的指针
 */
lv_PUBLIC_API void lv_list_type_destroy(lvListTypeRegion *t);

/**
 * @brief 创建映射（字典）类型区域
 * @param key_type 键类型指针
 * @param value_type 值类型指针
 * @return 成功返回映射类型区域指针，失败返回 NULL
 */
lvMapTypeRegion *lv_map_type_create(void *key_type, void *value_type);

/**
 * @brief 销毁映射类型区域并释放资源
 * @param t 指向待销毁的映射类型区域的指针
 */
lv_PUBLIC_API void lv_map_type_destroy(lvMapTypeRegion *t);

/**
 * @brief 创建函数类型区域
 * @param param 函数参数类型指针
 * @param ret 函数返回类型指针
 * @param dependent 是否为依赖类型（非零表示是依赖类型）
 * @return 成功返回函数类型区域指针，失败返回 NULL
 */
lvFunctionTypeRegion *lv_function_type_create(void *param, void *ret, int dependent);

/**
 * @brief 销毁函数类型区域并释放资源
 * @param t 指向待销毁的函数类型区域的指针
 */
lv_PUBLIC_API void lv_function_type_destroy(lvFunctionTypeRegion *t);

/**
 * @brief 创建副作用类型区域
 * @param effects 副作用类型数组指针
 * @param count 副作用类型数量
 * @param result 结果类型指针
 * @return 成功返回副作用类型区域指针，失败返回 NULL
 */
lvEffectTypeRegion *lv_effect_type_create(lvEffectType *effects, int count, void *result);

/**
 * @brief 销毁副作用类型区域并释放资源
 * @param t 指向待销毁的副作用类型区域的指针
 */
lv_PUBLIC_API void lv_effect_type_destroy(lvEffectTypeRegion *t);

/* Type compatibility check */

/**
 * @brief 检查两个扩展类型的兼容性
 * @param a 第一个类型指针
 * @param b 第二个类型指针
 * @return 非零表示兼容，0 表示不兼容
 */
lv_PUBLIC_API int lv_extended_type_compatible(void *a, void *b);

#ifdef __cplusplus
}
#endif

#endif /* lv_EXTENDED_TYPES_H */
