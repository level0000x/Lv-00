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
 * @brief 从文本中定位指针前向回退并解析整数（关键词后置数字提取）
 *
 * 语义契约：从 pos 向 base 方向回退连续 ASCII 数字字符，定位数字串起点后
 * 按前缀语义解析（仅消费数字前缀，忽略后续非数字字符）；pos 处非数字、
 * pos 之前无数字、前缀溢出或前缀为空时返回 -1 且不写 out。
 *
 * 前置条件：base <= pos；base / pos / out 均非 NULL。
 * 失败/截断语义：纯查询，无资源分配；失败不修改 out。
 * 边界行为：pos == base 恒失败（无前驱字符可回退）；单个数字字符可正常解析；
 *            数字串起点位于 base 处（回退到 base 即停）仍可解析。
 * 扩展点：无（若未来需要「数字串后置 + 前向吸收」，另立新设施）。
 *
 * @note 与 lv_parse_int 严格整串语义相区别：调用点传入的是嵌入消息文本中的
 *       数字子串（如 "12尝试完成…"），整串消费恒失败；故按前缀语义解析，
 *       与 lv_parse_double 的前缀语义一致。原始 meta_verify.c 样板因整串
 *       解析恒失败而实际不可达，本设施为语义修复 + 收敛（见 ABSTRACTION_SPEC
 *       判据 I 决策登记）。
 *
 * 收敛对象（判据 I）：meta_verify.c 四处「strstr 定位关键词 → 回退数字起始 →
 * lv_parse_int」样板（策略尝试 / 标记数 / 几何对象数 / 字节数）。
 */
static inline int lv_parse_int_before(const char *base, const char *pos, int *out) {
    if (!base || !pos || pos < base || !out)
        return -1;
    const char *num_start = pos;
    while (num_start > base && *(num_start - 1) >= '0' && *(num_start - 1) <= '9')
        num_start--;
    if (num_start == pos)
        return -1;
    char *end = NULL;
    errno = 0;
    long val = strtol(num_start, &end, 10);
    if (errno != 0 || end == num_start || val > INT_MAX || val < INT_MIN)
        return -1;
    *out = (int) val;
    return 0;
}

static inline char lv_str_scan_peek(const char *p, const char *end) {
    if (!p || (end && p >= end))
        return '\0';
    return *p;
}

static inline bool lv_str_scan_digit(const char *p, const char *end) {
    char c = lv_str_scan_peek(p, end);
    return c >= '0' && c <= '9';
}

/**
 * @brief 数字字面量词法扫描器：返回数字字面量的结束位置
 *
 * 语义契约：从 p 扫描一个数字字面量（可选前导 '-'、整数部分、可选小数部分、
 * 可选科学计数法 e/E[±]数字），返回首个非数字字面量字符的位置；起始处不是
 * 数字 / '.' / '-' 加数字时返回 p 不动。仅做词法定位，不含数值累加。
 *
 * 前置条件：p 非 NULL；end 为 NULL 表示 NUL 终止，否则为扫描上界（不含）。
 * 失败/截断语义：纯查询，无资源分配；非数字起始返回 p。
 * 边界行为：'.' 后跟数字才消费（防 ".."）；e/E 后需 [±]数字才消费指数；
 *           '-' 后需数字才消费符号；有界场景下任何越界读均返回 '\0'。
 * 扩展点：无。
 *
 * @note 收敛对象（判据 I 变体）：gc_language.c / dsl_lexer.c 数字字面量词法
 *       骨架；module_lvz.c / axiom_pkg_parser.c / lv_lexer.c 因累加交错（豁免
 *       #9）或回退 token 契约豁免登记。科学计数法采用严格语义（e 后需数字），
 *       dsl_lexer 原无条件吞 e 的实现为缺陷修复。
 */
static inline const char *lv_str_scan_number(const char *p, const char *end) {
    if (!p)
        return NULL;

    if (lv_str_scan_peek(p, end) == '-') {
        if (!lv_str_scan_digit(p + 1, end))
            return p;
        p++;
    } else if (lv_str_scan_peek(p, end) == '.') {
        if (!lv_str_scan_digit(p + 1, end))
            return p;
    } else if (!lv_str_scan_digit(p, end)) {
        return p;
    }

    while (lv_str_scan_digit(p, end))
        p++;

    if (lv_str_scan_peek(p, end) == '.') {
        if (lv_str_scan_digit(p + 1, end)) {
            p++;
            while (lv_str_scan_digit(p, end))
                p++;
        }
    }

    if (lv_str_scan_peek(p, end) == 'e' || lv_str_scan_peek(p, end) == 'E') {
        const char *q = p + 1;
        if (lv_str_scan_peek(q, end) == '+' || lv_str_scan_peek(q, end) == '-')
            q++;
        if (lv_str_scan_digit(q, end)) {
            p = q;
            while (lv_str_scan_digit(p, end))
                p++;
        }
    }

    return p;
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