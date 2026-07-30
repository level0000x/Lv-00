#ifndef lv_XMACRO_H
#define lv_XMACRO_H

/**
 * @file lv_xmacro.h
 * @brief X-macro 辅助宏，用于生成枚举→字符串映射
 *
 * 用法：
 *   1. 定义一个 X 列表宏（如 LV_MY_ENUM_X(x)）
 *   2. 用 lv_XMACRO_ENUM 生成枚举定义
 *   3. 用 lv_XMACRO_TO_STR 生成 switch 字符串函数
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

#endif /* lv_XMACRO_H */
