#ifndef LV00_EXTENDED_TYPES_H
#define LV00_EXTENDED_TYPES_H

#include "lv00/type_system.h"
#include "lv00/effect_system.h"

#ifdef __cplusplus
extern "C" {
#endif

/* List type region */
typedef struct Lv00ListTypeRegion {
    void *base;  /* TypeRegion base */
    void *elem_type;
} Lv00ListTypeRegion;

/* Map type region */
typedef struct Lv00MapTypeRegion {
    void *base;
    void *key_type;
    void *value_type;
} Lv00MapTypeRegion;

/* Function type region (dependent) */
typedef struct Lv00FunctionTypeRegion {
    void *base;
    void *param_type;
    void *return_type;
    int is_dependent;
} Lv00FunctionTypeRegion;

/* Effect type region */
typedef struct Lv00EffectTypeRegion {
    void *base;
    Lv00EffectType *effects;
    int effect_count;
    void *result_type;
} Lv00EffectTypeRegion;

/* Factory */

/**
 * @brief 创建列表类型区域
 * @param elem_type 列表元素类型指针
 * @return 成功返回列表类型区域指针，失败返回 NULL
 */
Lv00ListTypeRegion *lv00_list_type_create(void *elem_type);

/**
 * @brief 销毁列表类型区域并释放资源
 * @param t 指向待销毁的列表类型区域的指针
 */
void lv00_list_type_destroy(Lv00ListTypeRegion *t);

/**
 * @brief 创建映射（字典）类型区域
 * @param key_type 键类型指针
 * @param value_type 值类型指针
 * @return 成功返回映射类型区域指针，失败返回 NULL
 */
Lv00MapTypeRegion *lv00_map_type_create(void *key_type, void *value_type);

/**
 * @brief 销毁映射类型区域并释放资源
 * @param t 指向待销毁的映射类型区域的指针
 */
void lv00_map_type_destroy(Lv00MapTypeRegion *t);

/**
 * @brief 创建函数类型区域
 * @param param 函数参数类型指针
 * @param ret 函数返回类型指针
 * @param dependent 是否为依赖类型（非零表示是依赖类型）
 * @return 成功返回函数类型区域指针，失败返回 NULL
 */
Lv00FunctionTypeRegion *lv00_function_type_create(void *param, void *ret, int dependent);

/**
 * @brief 销毁函数类型区域并释放资源
 * @param t 指向待销毁的函数类型区域的指针
 */
void lv00_function_type_destroy(Lv00FunctionTypeRegion *t);

/**
 * @brief 创建副作用类型区域
 * @param effects 副作用类型数组指针
 * @param count 副作用类型数量
 * @param result 结果类型指针
 * @return 成功返回副作用类型区域指针，失败返回 NULL
 */
Lv00EffectTypeRegion *lv00_effect_type_create(Lv00EffectType *effects, int count, void *result);

/**
 * @brief 销毁副作用类型区域并释放资源
 * @param t 指向待销毁的副作用类型区域的指针
 */
void lv00_effect_type_destroy(Lv00EffectTypeRegion *t);

/* Type compatibility check */

/**
 * @brief 检查两个扩展类型的兼容性
 * @param a 第一个类型指针
 * @param b 第二个类型指针
 * @return 非零表示兼容，0 表示不兼容
 */
int lv00_extended_type_compatible(void *a, void *b);

#ifdef __cplusplus
}
#endif

#endif /* LV00_EXTENDED_TYPES_H */
