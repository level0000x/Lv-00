#ifndef LV00_EXACT_ARITHMETIC_H
#define LV00_EXACT_ARITHMETIC_H

#include <stdbool.h>
#include <stdint.h>

/* LV00_TOLERATED_FLOAT: 标记浮点字段，用于精度容差相关成员
 * 当前仅作为字段名前缀，无额外语义 */
#define LV00_TOLERATED_FLOAT(name) name

/**
 * @brief 高精度时间戳
 */
typedef struct {
    int64_t seconds;
    int64_t nanoseconds;
} Lv00Timestamp;

Lv00Timestamp lv00_timestamp_now(void);
bool lv00_safe_pow(int64_t a, int64_t b, int64_t *result);
int lv00_safe_mul_impl(int64_t a, int64_t b, int64_t *out);
int lv00_safe_add_check_impl(int64_t a, int64_t b, int64_t *out);
int lv00_safe_sub_impl(int64_t a, int64_t b, int64_t *out);

#endif /* LV00_EXACT_ARITHMETIC_H */

