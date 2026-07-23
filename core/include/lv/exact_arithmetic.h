#ifndef lv_EXACT_ARITHMETIC_H
#define lv_EXACT_ARITHMETIC_H

#include <stdbool.h>
#include <stdint.h>

/* lv_TOLERATED_FLOAT: 标记浮点字段，用于精度容差相关成员
 * 当前仅作为字段名前缀，无额外语义 */
#define lv_TOLERATED_FLOAT(name) name

/**
 * @brief 高精度时间戳
 */
typedef struct {
    int64_t seconds;
    int64_t nanoseconds;
} lvTimestamp;

lvTimestamp lv_timestamp_now(void);
bool lv_safe_pow(int64_t a, int64_t b, int64_t *result);
bool lv_safe_mul_impl(int64_t a, int64_t b, int64_t *out);
bool lv_safe_add_check_impl(int64_t a, int64_t b, int64_t *out);
bool lv_safe_sub_impl(int64_t a, int64_t b, int64_t *out);

#endif /* lv_EXACT_ARITHMETIC_H */
