/**
 * @file stream_utils.c
 * @brief 流式输出系统 —— 工具函数与事件类型映射表
 */

#include "stream_internal.h"


/* ==================== 工具函数 ==================== */

/**
 * 获取高精度时间戳（毫秒）。
 * Windows 平台使用 QueryPerformanceCounter 获取高精度时间，
 * 其他平台使用 gettimeofday。返回值为自某参考点以来的毫秒数，
 * 仅用于计算相对时间差，绝对值无意义。
 * @return 毫秒级时间戳
 */
long stream_timestamp_ms(void) {
    return (long) (lv_get_time_ns() / 1000000);
}

/* ============================================================
 * 事件类型映射表（数据驱动，替代 switch 语句）
 *
 * 统一管理事件类型的中文名称、英文标识符和前端颜色。
 * 新增事件类型时只需在此表添加一行，无需修改 3 个函数。
 * ============================================================ */

/** 事件类型映射表条目 */
typedef struct {
    StreamEventType type;   /**< 事件类型枚举值 */
    const char *name;       /**< 中文名称 */
    const char *id;         /**< 英文标识符 */
    const char *color;      /**< 前端颜色（十六进制） */
} StreamEventTypeEntry;

/** 事件类型映射表（按枚举值顺序排列，支持 O(1) 直接索引） */
static const StreamEventTypeEntry s_event_type_table[STREAM_EVENT_TYPE_COUNT] = {
    {STREAM_EVENT_ENGINE_START,              "引擎启动",       "ENGINE_START",              STREAM_COLOR_GREEN},
    {STREAM_EVENT_ENGINE_DONE,               "引擎完成",       "ENGINE_DONE",               STREAM_COLOR_GREEN},
    {STREAM_EVENT_ENGINE_PAUSED,             "引擎暂停",       "ENGINE_PAUSED",             STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_NORMALIZE_START,           "归一化开始",     "NORMALIZE_START",           STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_NORMALIZE_MERGE,           "节点合并",       "NORMALIZE_MERGE",           STREAM_COLOR_PURPLE},
    {STREAM_EVENT_NORMALIZE_DONE,            "归一化完成",     "NORMALIZE_DONE",            STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_REWRITE_START,             "重写开始",       "REWRITE_START",             STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_REWRITE_RULE_LOADED,       "规则加载",       "REWRITE_RULE_LOADED",       STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_REWRITE_MATCH_FOUND,       "匹配找到",       "REWRITE_MATCH_FOUND",       STREAM_COLOR_PURPLE},
    {STREAM_EVENT_REWRITE_APPLIED,           "规则应用",       "REWRITE_APPLIED",           STREAM_COLOR_PURPLE},
    {STREAM_EVENT_REWRITE_ROLLBACK,          "规则回滚",       "REWRITE_ROLLBACK",          STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_REWRITE_DONE,              "重写完成",       "REWRITE_DONE",              STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_SOLVE_START,               "求解开始",       "SOLVE_START",               STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_SOLVE_EQUATION_EXTRACTED,  "方程提取",       "SOLVE_EQUATION_EXTRACTED",  STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_SOLVE_GROEBNER_STEP,       "Gröbner基步骤",  "SOLVE_GROEBNER_STEP",       STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_SOLVE_VARIABLE_RESOLVED,   "变量解得",       "SOLVE_VARIABLE_RESOLVED",   STREAM_COLOR_PURPLE},
    {STREAM_EVENT_SOLVE_DONE,                "求解完成",       "SOLVE_DONE",                STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_PROOF_STEP_ADDED,          "证明步骤添加",   "PROOF_STEP_ADDED",          STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_PROOF_STEP_APPLIED,        "证明步骤应用",   "PROOF_STEP_APPLIED",        STREAM_COLOR_PURPLE},
    {STREAM_EVENT_PROOF_UNIFY,               "合一检查",       "PROOF_UNIFY",               STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_PROOF_COLOR_UPDATE,        "颜色更新",       "PROOF_COLOR_UPDATE",        STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_PROOF_DEPENDENCY_CHANGE,   "依赖链变化",     "PROOF_DEPENDENCY_CHANGE",   STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_FUNC_BLOCK_PACK_START,     "函数打包开始",   "FUNC_BLOCK_PACK_START",     STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_FUNC_BLOCK_PACK_DONE,      "函数打包完成",   "FUNC_BLOCK_PACK_DONE",      STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_FUNC_BLOCK_INSTANTIATE_START,"函数实例化开始","FUNC_BLOCK_INSTANTIATE_START",STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_FUNC_BLOCK_INSTANTIATE_DONE,"函数实例化完成","FUNC_BLOCK_INSTANTIATE_DONE",STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_FUNC_BLOCK_PARTIAL_APPLY,  "部分应用",       "FUNC_BLOCK_PARTIAL_APPLY",  STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_FUNC_BLOCK_DETERMINISM_CHECK,"确定性检查",   "FUNC_BLOCK_DETERMINISM_CHECK",STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_FUNC_BLOCK_CAPTURE_AVOID,  "捕获避免",       "FUNC_BLOCK_CAPTURE_AVOID",  STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_FUNC_BLOCK_CROSS_BOUNDARY, "跨边界操作",     "FUNC_BLOCK_CROSS_BOUNDARY", STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_PRESET_REGISTER_START,     "预设注册开始",   "PRESET_REGISTER_START",     STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_PRESET_REGISTER_DONE,      "预设注册完成",   "PRESET_REGISTER_DONE",      STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_PRESET_REGISTER_FAILED,    "预设注册失败",   "PRESET_REGISTER_FAILED",    STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_PRESET_LOOKUP,             "预设查找",       "PRESET_LOOKUP",             STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_PRESET_INSTANTIATE,        "预设实例化",     "PRESET_INSTANTIATE",        STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_PRESET_VALIDATE,           "预设验证",       "PRESET_VALIDATE",           STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_PRESET_CATEGORY_LOADED,    "预设类别加载",   "PRESET_CATEGORY_LOADED",    STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_PRESET_MODULE_LOADED,      "预设模块加载",   "PRESET_MODULE_LOADED",      STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_CONFLICT_DETECTED,         "冲突检测",       "CONFLICT_DETECTED",         STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_CONSTRAINT_ADDED,          "约束添加",       "CONSTRAINT_ADDED",          STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_NODE_ADDED,                "节点添加",       "NODE_ADDED",                STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_CIRCUIT_TRIP,              "位数熔断",       "CIRCUIT_TRIP",              STREAM_COLOR_ORANGE},
    {STREAM_EVENT_ERROR,                     "错误",           "ERROR",                     STREAM_COLOR_RED},
    {STREAM_EVENT_WARNING,                   "警告",           "WARNING",                   STREAM_COLOR_YELLOW},
    {STREAM_EVENT_INFO,                      "信息",           "INFO",                      STREAM_COLOR_GRAY},
    {STREAM_EVENT_PROGRESS,                  "进度",           "PROGRESS",                  STREAM_COLOR_BLUE},
    {STREAM_EVENT_GRAPH_SNAPSHOT,            "图快照",         "GRAPH_SNAPSHOT",            STREAM_COLOR_LIGHT_GRAY},
    {STREAM_EVENT_BUS_EVENT,                 "事件总线",       "BUS_EVENT",                 STREAM_COLOR_GRAY},
};

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

