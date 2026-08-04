#ifndef lv_NUMBER_H
#define lv_NUMBER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifndef lv_PUBLIC_API
#define lv_PUBLIC_API
#endif

// 数值类型枚举
typedef enum {
    lv_NUMBER_RATIONAL = 0,    // 有理数
    lv_NUMBER_ALGEBRAIC,       // 代数数
    lv_NUMBER_INTERVAL,        // 区间
    lv_NUMBER_FLOAT,           // 浮点
    lv_NUMBER_INTEGER,         // 整数
    lv_NUMBER_COUNT
} lvNumberType;

// 前向声明
typedef struct lvNumber lvNumber;

// 数值操作 vtable
typedef struct lvNumberOps {
    lvNumber *(*add)(const lvNumber *a, const lvNumber *b);
    lvNumber *(*sub)(const lvNumber *a, const lvNumber *b);
    lvNumber *(*mul)(const lvNumber *a, const lvNumber *b);
    lvNumber *(*div)(const lvNumber *a, const lvNumber *b);
    int (*compare)(const lvNumber *a, const lvNumber *b);   // -1/0/1
    double (*to_double)(const lvNumber *n);
    uint64_t (*hash)(const lvNumber *n);
    char *(*to_string)(const lvNumber *n);                  // 调用者 free
    bool (*is_zero)(const lvNumber *n);
    bool (*is_one)(const lvNumber *n);
    bool (*is_negative)(const lvNumber *n);
    lvNumber *(*clone)(const lvNumber *n);
    void (*destroy)(lvNumber *n);
    lvNumberType (*type)(const lvNumber *n);
} lvNumberOps;

// 数值句柄（所有数值操作通过此接口进行）
typedef struct lvNumber {
    const lvNumberOps *ops;
    void *impl;  // 指向具体实现（mpq_t, algebraic, interval 等）
} lvNumber;

// ---- 工厂函数 ----
lv_PUBLIC_API lvNumber *lv_number_from_rational(int64_t num, uint64_t den);
lv_PUBLIC_API lvNumber *lv_number_from_double(double val);
lv_PUBLIC_API lvNumber *lv_number_from_int(int64_t val);
lv_PUBLIC_API lvNumber *lv_number_from_string(const char *str);

// ---- 算术运算 ----
lv_PUBLIC_API lvNumber *lv_number_add(const lvNumber *a, const lvNumber *b);
lv_PUBLIC_API lvNumber *lv_number_sub(const lvNumber *a, const lvNumber *b);
lv_PUBLIC_API lvNumber *lv_number_mul(const lvNumber *a, const lvNumber *b);
lv_PUBLIC_API lvNumber *lv_number_div(const lvNumber *a, const lvNumber *b);
lv_PUBLIC_API lvNumber *lv_number_neg(const lvNumber *n);
lv_PUBLIC_API lvNumber *lv_number_abs(const lvNumber *n);
lv_PUBLIC_API lvNumber *lv_number_pow(const lvNumber *base, int exp);

// ---- 比较 ----
lv_PUBLIC_API int lv_number_compare(const lvNumber *a, const lvNumber *b);
lv_PUBLIC_API bool lv_number_eq(const lvNumber *a, const lvNumber *b);
lv_PUBLIC_API bool lv_number_lt(const lvNumber *a, const lvNumber *b);
lv_PUBLIC_API bool lv_number_gt(const lvNumber *a, const lvNumber *b);
lv_PUBLIC_API bool lv_number_lte(const lvNumber *a, const lvNumber *b);
lv_PUBLIC_API bool lv_number_gte(const lvNumber *a, const lvNumber *b);

// ---- 转换 ----
lv_PUBLIC_API double lv_number_to_double(const lvNumber *n);
lv_PUBLIC_API int64_t lv_number_to_int(const lvNumber *n);
lv_PUBLIC_API char *lv_number_to_string(const lvNumber *n);

// ---- 查询 ----
lv_PUBLIC_API bool lv_number_is_zero(const lvNumber *n);
lv_PUBLIC_API bool lv_number_is_one(const lvNumber *n);
lv_PUBLIC_API bool lv_number_is_negative(const lvNumber *n);
lv_PUBLIC_API bool lv_number_is_positive(const lvNumber *n);
lv_PUBLIC_API bool lv_number_is_integer(const lvNumber *n);
lv_PUBLIC_API lvNumberType lv_number_type(const lvNumber *n);
lv_PUBLIC_API uint64_t lv_number_hash(const lvNumber *n);
lv_PUBLIC_API lvNumber *lv_number_clone(const lvNumber *n);
lv_PUBLIC_API void lv_number_destroy(lvNumber *n);

// ---- 类型信息 ----
lv_PUBLIC_API const char *lv_number_type_name(lvNumberType type);

#ifdef __cplusplus
}
#endif

#endif