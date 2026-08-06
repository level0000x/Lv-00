#ifndef lv_XMACRO_H
#define lv_XMACRO_H

/**
 * @file lv_xmacro.h
 * @brief X-macro 辅助宏，用于生成枚举↔字符串双向映射
 *
 * 用法：
 *   1. 定义一个 X 列表宏（如 LV_MY_ENUM_X(x)）
 *   2. 用 lv_XMACRO_ENUM 生成枚举定义
 *   3. 用 lv_XMACRO_TO_STR 生成 switch 字符串函数
 *   4. 用 lv_XMACRO_TO_ENUM_TABLE 和 lv_str_to_enum 实现字符串→枚举查找
 *
 * 示例：
 *   #define LV_COLORS_X(x) \
 *       x(lv_RED, "red")   \
 *       x(lv_GREEN, "green")
 *
 *   enum lvColor { lv_XMACRO_ENUM(LV_COLORS_X) };
 *   const char *lv_color_to_str(enum lvColor c) {
 *       switch(c) { lv_XMACRO_TO_STR(LV_COLORS_X) default: return "?"; }
 *   }
 *
 *   static const lvStrToEnumEntry colors_map[] = {
 *       lv_XMACRO_TO_ENUM_TABLE(LV_COLORS_X)
 *   };
 *   lvCol c = (lvCol)lv_str_to_enum(colors_map, 2, "red", 0);
 */

/** @brief 从 X 列表生成枚举值 */
#define lv_XMACRO_ENUM(X_LIST) \
    X_LIST(LV_X_ENUM_ITEM)

/** @brief 从 X 列表生成 switch-case 字符串映射 */
#define lv_XMACRO_TO_STR(X_LIST) \
    X_LIST(LV_X_TO_STR_CASE)

/* 内部辅助宏 */
#define LV_X_ENUM_ITEM(name, str) name,
#define LV_X_TO_STR_CASE(name, str) case name: return str;

#include <string.h>
#include <stddef.h>

#include "lv_str_utils.h" /* lv_str_icmp */

/** @brief 字符串→枚举映射表条目 */
typedef struct {
    const char *name;
    int value;
} lvStrToEnumEntry;

/** @brief 从 X 列表生成映射表初始化器 */
#define lv_XMACRO_TO_ENUM_TABLE(X_LIST) \
    X_LIST(LV_X_TO_ENUM_ENTRY)

/* 内部辅助宏 */
#define LV_X_TO_ENUM_ENTRY(name, str) { str, name },

/**
 * @brief 从 X 列表生成「枚举值 → 字符串」指定初始化器名称数组
 *
 * 用法：static const char *const kLabels[] = { lv_XMACRO_TO_NAME_ARRAY(LV_MY_ENUM_X) };
 * 生成的数组按枚举值索引（设计指定初始化器），可配合 lv_ARRAY_SIZE 做边界检查，
 * 替代散落各文件的同名重复 char* 数组。
 */
#define lv_XMACRO_TO_NAME_ARRAY(X_LIST) \
    X_LIST(LV_X_TO_NAME_ENTRY)

/* 内部辅助宏 */
#define LV_X_TO_NAME_ENTRY(name, str) [name] = str,

/**
 * @brief 安全分发调用：key 越界或表中槽位为 NULL 时返回 fallback，否则调用 table[key](...)
 *
 * 收敛散落各模块的手写"边界检查 + NULL 槽检查 + 调用"三行样板
 * （此前 ~30 处逐字同构）。fallback 须与表项返回类型一致。
 *
 * @param table    函数指针数组（编译期数组，自动取大小）
 * @param key      索引（自动做 unsigned 边界检查）
 * @param fallback 越界/NULL 槽时的返回值
 * @param ...      传给 handler 的参数
 */
#define LV_DISPATCH(table, key, fallback, ...) \
    (((unsigned)(key) < (unsigned)(sizeof(table) / sizeof((table)[0]))) && (table)[(key)] \
         ? (table)[(key)](__VA_ARGS__) \
         : (fallback))

/**
 * @brief 安全分发调用（void 返回表）：key 越界或表中槽位为 NULL 时直接返回，否则调用 table[key](...)
 *
 * C 语言 void 不能作为三元运算符操作数，故提供 if 语句版，供 void 返回类型的
 * handler 表使用（LV_DISPATCH 无法处理）。边界/NULL 语义与 LV_DISPATCH 完全一致。
 *
 * @param table 函数指针数组（编译期数组，自动取大小）
 * @param key   索引（自动做 unsigned 边界检查）
 * @param ...   传给 handler 的参数
 */
#define LV_DISPATCH_VOID(table, key, ...)                         \
    do {                                                          \
        if ((unsigned)(key) < (unsigned)(sizeof(table) / sizeof((table)[0])) \
            && (table)[(key)]) {                                  \
            (table)[(key)](__VA_ARGS__);                          \
        }                                                         \
    } while (0)

/**
 * @brief 在映射表中查找字符串对应的枚举值（大小写敏感）
 * @param table     映射表
 * @param count     表大小
 * @param str       要查找的字符串
 * @param default_value 未找到时的默认返回值
 * @return 枚举值（int），未找到返回 default_value
 */
static inline int lv_str_to_enum(const lvStrToEnumEntry *table, size_t count,
                                  const char *str, int default_value) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(table[i].name, str) == 0)
            return table[i].value;
    }
    return default_value;
}

/**
 * @brief 在映射表中查找字符串对应的枚举值（大小写不敏感）
 * @param table     映射表
 * @param count     表大小
 * @param str       要查找的字符串
 * @param default_value 未找到时的默认返回值
 * @return 枚举值（int），未找到返回 default_value
 */
static inline int lv_str_to_enum_ci(const lvStrToEnumEntry *table, size_t count,
                                     const char *str, int default_value) {
    for (size_t i = 0; i < count; i++) {
        if (lv_str_icmp(table[i].name, str) == 0)
            return table[i].value;
    }
    return default_value;
}

/**
 * @brief 在映射表中按枚举值（value）二分查找对应字符串（反向的 lv_str_to_enum）
 * @param table       映射表（lvStrToEnumEntry 数组，须按 value 即枚举值升序排列）
 * @param count       表大小
 * @param code        要查找的枚举值
 * @param default_str 未命中时返回的默认字符串（可为 NULL）
 * @return 匹配的名称字符串；未命中返回 default_str
 */
static inline const char *lv_enum_to_str(const lvStrToEnumEntry *table, size_t count,
                                         int code, const char *default_str) {
    size_t lo = 0, hi = count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (table[mid].value == code)
            return table[mid].name;
        if (table[mid].value < code)
            lo = mid + 1;
        else
            hi = mid;
    }
    return default_str;
}

#endif /* lv_XMACRO_H */
