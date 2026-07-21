#ifndef LV00_PARSE_UTILS_H
#define LV00_PARSE_UTILS_H

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

/**
 * @brief 安全地将字符串解析为 int，替代不安全的 atoi()
 * @param str 输入字符串
 * @param out 输出整数
 * @return 成功返回 0，失败返回 -1
 */
static inline int lv00_parse_int(const char *str, int *out)
{
    if (!str || !*str || !out) return -1;
    char *end = NULL;
    errno = 0;
    long val = strtol(str, &end, 10);
    if (errno != 0 || end == str || *end != '\0') return -1;
    if (val > INT_MAX || val < INT_MIN) return -1;
    *out = (int)val;
    return 0;
}

/**
 * @brief 安全地将字符串解析为 double，替代不安全的 atof()
 * @param str 输入字符串
 * @param out 输出双精度浮点数
 * @return 成功返回 0，失败返回 -1
 */
static inline int lv00_parse_double(const char *str, double *out)
{
    if (!str || !*str || !out) return -1;
    char *end = NULL;
    errno = 0;
    double val = strtod(str, &end);
    if (errno != 0 || end == str) return -1;
    *out = val;
    return 0;
}

/**
 * @brief 安全解析整数，失败时返回默认值
 * @param str 输入字符串
 * @param default_value 解析失败时返回的默认值
 * @return 解析后的整数，或默认值
 */
static inline int lv00_parse_int_default(const char *str, int default_value)
{
    int result = 0;
    if (lv00_parse_int(str, &result) != 0) return default_value;
    return result;
}

#endif /* LV00_PARSE_UTILS_H */