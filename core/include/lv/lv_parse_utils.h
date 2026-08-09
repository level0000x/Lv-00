#ifndef lv_PARSE_UTILS_H
#define lv_PARSE_UTILS_H

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>

/**
 * @brief 安全地将字符串解析为 int，替代不安全的 atoi()
 * @param str 输入字符串
 * @param out 输出整数
 * @return 成功返回 0，失败返回 -1
 */
static inline int lv_parse_int(const char *str, int *out) {
    if (!str || !*str || !out)
        return -1;
    char *end = NULL;
    errno = 0;
    long val = strtol(str, &end, 10);
    if (errno != 0 || end == str || *end != '\0')
        return -1;
    if (val > INT_MAX || val < INT_MIN)
        return -1;
    *out = (int) val;
    return 0;
}

/**
 * @brief 安全地将字符串解析为 double，替代不安全的 atof()
 * @param str 输入字符串
 * @param out 输出双精度浮点数
 * @return 成功返回 0，失败返回 -1
 *
 * @note 前缀解析语义：strtod 停止处之后的尾部字符被忽略（不要求整串消费），
 *       与 lv_parse_double_strict 的严格整串语义相区别。
 *       runtime_monitor.c 等调用方依赖此前缀语义（如 " 12345 kB" 解析出 12345）。
 */
static inline int lv_parse_double(const char *str, double *out) {
    if (!str || !*str || !out)
        return -1;
    char *end = NULL;
    errno = 0;
    double val = strtod(str, &end);
    if (errno != 0 || end == str)
        return -1;
    *out = val;
    return 0;
}

/**
 * @brief 严格整串解析 double：整串消费 + errno 检查
 * @param str 输入字符串
 * @param out 输出双精度浮点数
 * @return 成功返回 0，失败返回 -1
 *
 * @note 与 lv_parse_double 的区别：要求整串均为数字（*end == '\0'），
 *       尾部有任何非空白残留字符均判定失败。
 */
static inline int lv_parse_double_strict(const char *str, double *out) {
    if (!str || !*str || !out)
        return -1;
    char *end = NULL;
    errno = 0;
    double val = strtod(str, &end);
    if (errno != 0 || end == str || *end != '\0')
        return -1;
    *out = val;
    return 0;
}

/**
 * @brief 安全解析整数，失败时返回默认值
 * @param str 输入字符串
 * @param default_value 解析失败时返回的默认值
 * @return 解析后的整数，或默认值
 */
static inline int lv_parse_int_default(const char *str, int default_value) {
    int result = 0;
    if (lv_parse_int(str, &result) != 0)
        return default_value;
    return result;
}

/**
 * @brief 读取环境变量并安全解析为整数（严格解析 + 范围钳制）
 * @param name 环境变量名
 * @param dflt 变量缺失 / 空串 / 解析失败时返回的默认值
 * @param min  钳制下限
 * @param max  钳制上限
 * @return 解析成功则返回钳制到 [min, max] 的整数值，否则返回 dflt
 * @note 收敛 runtime_monitor.c 手写 getenv + strtol + clamp 样板：
 *       与 lv_parse_int 一致采用严格整串解析（errno + 尾字符检查），
 *       非法输入回落 dflt，不再静默截断为 0。
 */
static inline int lv_env_get_int(const char *name, int dflt, int min, int max) {
    if (!name) {
        return dflt;
    }
    const char *val = getenv(name);
    if (!val || !val[0]) {
        return dflt;
    }
    int parsed = 0;
    if (lv_parse_int(val, &parsed) != 0) {
        return dflt;
    }
    if (parsed < min) {
        parsed = min;
    }
    if (parsed > max) {
        parsed = max;
    }
    return parsed;
}

/**
 * @brief 读取环境变量并解析为布尔值
 * @param name 环境变量名
 * @param dflt 变量缺失 / 空串 / 解析失败时返回的默认值
 * @return 解析成功时按整数值非零归一化（"1"/"0"/其他整数），否则返回 dflt
 * @note 收敛 solver_groebner.c 手写 getenv + 仅认 '1' 判断样板；
 *       只接受整数字符串（lv_parse_int 严格解析），非数字回落 dflt。
 */
static inline bool lv_env_get_bool(const char *name, bool dflt) {
    if (!name) {
        return dflt;
    }
    const char *val = getenv(name);
    if (!val || !val[0]) {
        return dflt;
    }
    int parsed = 0;
    if (lv_parse_int(val, &parsed) != 0) {
        return dflt;
    }
    return parsed != 0;
}

#endif /* lv_PARSE_UTILS_H */