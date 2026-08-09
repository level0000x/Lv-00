/**
 * @file stream_utils.c
 * @brief 流式输出系统 —— 工具函数与事件类型映射表
 */

#include "stream_internal.h"


/* ==================== 工具函数 ==================== */

/**
 * 获取当前时间戳（毫秒，单调时钟）。
 * 基于 lv_get_time_ms 获取高精度单调时间，
 * 返回值为自某参考点以来的毫秒数，
 * 仅用于计算相对时间差，绝对值无意义。
 * @return 毫秒级时间戳
 */
long stream_timestamp_ms(void) {
    return (long) lv_get_time_ms();
}

/* ============================================================
 * 事件类型映射表（数据驱动，替代 switch 语句）
 *
 * 统一管理事件类型的中文名称、英文标识符和前端颜色。
 * 表内容与枚举由 stream.h 的 LV_STREAM_EVENT_X 单一事实来源宏生成，
 * 新增事件类型时只需在 LV_STREAM_EVENT_X 追加一行，无需修改任何函数。
 * ============================================================ */

/** 事件类型映射表条目 */
typedef struct {
    StreamEventType type;   /**< 事件类型枚举值 */
    const char *name;       /**< 中文名称 */
    const char *id;         /**< 英文标识符 */
    const char *color;      /**< 前端颜色（十六进制） */
} StreamEventTypeEntry;

/** @brief 映射表条目生成辅助宏（LV_STREAM_EVENT_X → `{枚举, 中文名, 英文标识符, 颜色},`） */
#define LV_X_STREAM_TABLE_ENTRY(name, name_str, id_str, color) \
    { name, name_str, id_str, color },

/** 事件类型映射表（按枚举值顺序排列，支持 O(1) 直接索引） */
static const StreamEventTypeEntry s_event_type_table[STREAM_EVENT_TYPE_COUNT] = {
    LV_STREAM_EVENT_X(LV_X_STREAM_TABLE_ENTRY)
};

#undef LV_X_STREAM_TABLE_ENTRY

/**
 * 获取事件类型的中文名称。
 * 用于前端 UI 显示和日志输出，将枚举值映射为可读的中文字符串。
 * @param type 事件类型枚举值
 * @return 中文名称字符串（静态常量，无需释放）
 */
const char *stream_event_type_name(StreamEventType type) {
    if (type >= 0 && type < STREAM_EVENT_TYPE_COUNT) {
        return s_event_type_table[type].name;
    }
    return "未知事件";
}

/**
 * 获取事件类型的英文标识符。
 * 用于 JSON 序列化和前端事件路由，返回大写字母+下划线格式的字符串。
 * @param type 事件类型枚举值
 * @return 英文标识符字符串（静态常量，无需释放）
 */
const char *stream_event_type_id(StreamEventType type) {
    if (type >= 0 && type < STREAM_EVENT_TYPE_COUNT) {
        return s_event_type_table[type].id;
    }
    return "UNKNOWN_EVENT";
}

/**
 * 获取事件类型对应的前端显示颜色（十六进制格式）。
 * 根据事件类型返回对应的 CSS 颜色字符串，用于 Web 前端渲染事件节点。
 * 颜色常量统一定义在文件顶部的 STREAM_COLOR_* 宏中。
 * @param type 事件类型枚举值
 * @return 十六进制颜色字符串（如 "#3fb950"）
 */
const char *stream_event_color(StreamEventType type) {
    if (type >= 0 && type < STREAM_EVENT_TYPE_COUNT) {
        return s_event_type_table[type].color;
    }
    return STREAM_COLOR_LIGHT_GRAY;
}

