/**
 * @file debug_trace.h
 * @brief 调试追踪系统 —— 证明状态快照、推导过程记录与性能分析计时器
 *
 * @details 本模块为 Lv-00 调试子系统提供三项核心能力：
 *   1. 证明状态快照（Lv00DebugSnapshot）：保存/恢复证明导航器的关键状态，
 *      用于分支推理时的状态回滚和调试断点恢复。
 *   2. 推导过程记录（Lv00DerivationLog）：记录证明引擎每一步推导的详细信息，
 *      包括应用的规则、输入输出、耗时等，支持结构化查询和导出。
 *   3. 性能分析计时器（Lv00PerfTimer）：轻量级区间计时器，支持嵌套计时和
 *      统计汇总，用于定位证明引擎的性能瓶颈。
 *
 * 【层级归属】
 * 本模块属于 Layer 2 (Resource Management)，被所有上层模块依赖。
 *
 * 【设计约束】
 * - 所有类型使用 Lv00 前缀，与项目命名规范一致
 * - 使用 LV00_PUBLIC_API 宏，支持共享库构建
 * - 使用项目内存管理宏（lv00_malloc / lv00_free 等）
 * - 不引入新的外部依赖
 * - 中文注释，Doxygen 风格
 *
 * @version 3.6.0
 * @author Lv-00 Project
 */

#ifndef LV00_DEBUG_TRACE_H
#define LV00_DEBUG_TRACE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* LV00_PUBLIC_API 由 lv00.h 统一定义，此处仅做守卫检查 */
#ifndef LV00_PUBLIC_API
#define LV00_PUBLIC_API
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 快照最大标签长度 */
#define LV00_SNAPSHOT_MAX_LABEL_LEN 128

/** @brief 推导日志条目描述最大长度 */
#define LV00_DERIVATION_MAX_DESC_LEN 512

/** @brief 推导日志条目规则名称最大长度 */
#define LV00_DERIVATION_MAX_RULE_LEN 64

/** @brief 性能计时器最大嵌套深度 */
#define LV00_PERF_TIMER_MAX_DEPTH 16

/** @brief 性能计时器最大标签长度 */
#define LV00_PERF_TIMER_MAX_LABEL_LEN 64

/** @brief 推导日志默认初始容量 */
#define LV00_DERIVATION_LOG_DEFAULT_CAPACITY 64

/** @brief 推导日志最大容量（防止内存无限增长） */
#define LV00_DERIVATION_LOG_MAX_CAPACITY 65536

/* ============================================================
 * 前向声明
 * ============================================================ */

struct ProofNavigator;
typedef struct ProofNavigator ProofNavigator;

/* ============================================================
 * 一、证明状态快照（Lv00DebugSnapshot）
 *
 * 保存证明导航器在某一时刻的关键状态，支持后续恢复。
 * 用于分支推理、断点调试和状态回滚。
 * ============================================================ */

/**
 * @brief 证明状态快照 —— 捕获证明导航器的关键状态
 *
 * 快照记录证明导航器在创建时刻的核心状态，包括：
 * - 当前步骤索引
 * - 证明完成状态
 * - 最终颜色
 * - 步骤数量
 * - 自定义标签（用于标识快照用途）
 *
 * 快照是轻量级的值类型，不持有对导航器的引用。
 * 适用于调试断点保存、分支推理回滚等场景。
 */
typedef struct {
    int    step_index;       /**< 快照时刻的当前步骤索引 */
    int    step_count;       /**< 快照时刻的步骤总数 */
    bool   is_complete;      /**< 快照时刻证明是否完成 */
    int    final_color;      /**< 快照时刻的最终颜色值（ProofColor 枚举值） */
    int    proof_state;      /**< 快照时刻的证明状态值（ProofState 枚举值） */
    int    breakpoint_count; /**< 快照时刻的断点数量 */
    char   label[LV00_SNAPSHOT_MAX_LABEL_LEN]; /**< 快照标签（用户自定义） */
    uint64_t timestamp_us;   /**< 快照时间戳（微秒） */
} Lv00DebugSnapshot;

/* ============================================================
 * 二、推导过程记录（Lv00DerivationLog）
 *
 * 记录证明引擎每一步推导的结构化信息，支持查询和导出。
 * ============================================================ */

/**
 * @brief 推导日志条目类型
 */
typedef enum {
    LV00_DERIVATION_RULE_APPLY,    /**< 规则应用 */
    LV00_DERIVATION_UNIFY_CHECK,   /**< 合一检查 */
    LV00_DERIVATION_REWRITE_STEP,  /**< 重写步骤 */
    LV00_DERIVATION_NORMALIZATION, /**< 归一化步骤 */
    LV00_DERIVATION_BRANCH,        /**< 分支点（多策略选择） */
    LV00_DERIVATION_BACKTRACK,     /**< 回溯 */
    LV00_DERIVATION_SUCCESS,       /**< 证明成功 */
    LV00_DERIVATION_FAILURE        /**< 证明失败 */
} Lv00DerivationEntryType;

/**
 * @brief 推导日志条目 —— 记录单步推导的详细信息
 *
 * 每个条目记录一步推导的完整上下文：
 * - 条目类型（规则应用、合一检查等）
 * - 应用的规则名称
 * - 步骤编号和深度
 * - 耗时（微秒）
 * - 描述信息
 */
typedef struct {
    Lv00DerivationEntryType type; /**< 条目类型 */
    int    step_number;           /**< 步骤编号（从 0 开始） */
    int    depth;                 /**< 推导深度（嵌套层级） */
    uint64_t timestamp_us;        /**< 时间戳（微秒） */
    uint64_t elapsed_us;          /**< 本步耗时（微秒） */
    char   rule_name[LV00_DERIVATION_MAX_RULE_LEN]; /**< 应用的规则名称 */
    char   description[LV00_DERIVATION_MAX_DESC_LEN]; /**< 描述信息 */
} Lv00DerivationEntry;

/**
 * @brief 推导日志 —— 推导过程的结构化记录容器
 *
 * 管理推导日志条目的动态数组，提供：
 * - 条目追加
 * - 按类型/步骤号查询
 * - 统计汇总
 * - 导出为文本
 */
typedef struct {
    Lv00DerivationEntry *entries; /**< 条目数组（动态分配） */
    int    count;                /**< 当前条目数 */
    int    capacity;             /**< 数组容量 */
    bool   active;               /**< 日志是否激活（false 时追加操作为空操作） */
    uint64_t total_elapsed_us;   /**< 总耗时（微秒） */
} Lv00DerivationLog;

/* ============================================================
 * 三、性能分析计时器（Lv00PerfTimer）
 *
 * 轻量级区间计时器，支持嵌套和统计汇总。
 * ============================================================ */

/**
 * @brief 计时器区间记录 —— 单次计时的结果
 *
 * 记录一次 begin/end 对的信息，用于性能分析。
 */
typedef struct {
    char   label[LV00_PERF_TIMER_MAX_LABEL_LEN]; /**< 区间标签 */
    uint64_t begin_us;       /**< 开始时间（微秒） */
    uint64_t end_us;         /**< 结束时间（微秒） */
    uint64_t elapsed_us;     /**< 耗时（微秒） */
    int    depth;            /**< 嵌套深度 */
    bool   completed;       /**< 是否已完成（end 已调用） */
} Lv00PerfTimerInterval;

/**
 * @brief 性能分析计时器 —— 区间计时与统计
 *
 * 提供嵌套的区间计时功能：
 * - lv00_perf_timer_begin() / lv00_perf_timer_end() 包裹待测代码
 * - 支持最多 LV00_PERF_TIMER_MAX_DEPTH 层嵌套
 * - 自动统计总耗时、调用次数、平均耗时等
 */
typedef struct {
    Lv00PerfTimerInterval stack[LV00_PERF_TIMER_MAX_DEPTH]; /**< 计时栈（嵌套支持） */
    int    current_depth;   /**< 当前嵌套深度 */
    uint64_t total_elapsed_us; /**< 总累计耗时（微秒） */
    uint64_t call_count;    /**< 总调用次数（begin 次数） */
    bool   active;          /**< 计时器是否激活 */
} Lv00PerfTimer;

/* ============================================================
 * 快照 API
 * ============================================================ */

/**
 * @brief 从证明导航器创建快照
 *
 * 捕获导航器当前的关键状态到快照结构中。
 * 快照是轻量级拷贝，不持有导航器的引用。
 *
 * @param[in] nav    证明导航器（不可为 NULL）
 * @param[in] label  快照标签（可为 NULL，默认为空字符串）
 * @return 填充完成的快照结构体
 *
 * @note 快照返回值类型为结构体（非指针），调用者可直接在栈上使用。
 */
LV00_PUBLIC_API Lv00DebugSnapshot lv00_debug_snapshot_create(const ProofNavigator *nav,
                                                              const char *label);

/**
 * @brief 获取快照的标签
 *
 * @param[in] snapshot 快照
 * @return 快照标签字符串（指向快照内部，无需释放）
 */
LV00_PUBLIC_API const char *lv00_debug_snapshot_get_label(const Lv00DebugSnapshot *snapshot);

/**
 * @brief 获取快照时间戳
 *
 * @param[in] snapshot 快照
 * @return 快照时间戳（微秒）
 */
LV00_PUBLIC_API uint64_t lv00_debug_snapshot_get_timestamp(const Lv00DebugSnapshot *snapshot);

/**
 * @brief 将快照信息格式化为字符串
 *
 * 生成人类可读的快照摘要，包含标签、时间戳、步骤信息等。
 *
 * @param[in] snapshot 快照
 * @param[out] buf     输出缓冲区（调用者分配）
 * @param[in] buf_size 缓冲区大小
 * @return 实际写入的字符数（不含终止符），缓冲区不足时返回所需大小
 */
LV00_PUBLIC_API int lv00_debug_snapshot_format(const Lv00DebugSnapshot *snapshot,
                                                char *buf, size_t buf_size);

/* ============================================================
 * 推导日志 API
 * ============================================================ */

/**
 * @brief 创建推导日志
 *
 * 分配并初始化一个空的推导日志实例。
 *
 * @return 新分配的推导日志指针，失败返回 NULL
 */
LV00_PUBLIC_API Lv00DerivationLog *lv00_derivation_log_create(void);

/**
 * @brief 销毁推导日志
 *
 * 释放推导日志及其所有条目。
 *
 * @param[in] log 推导日志指针（可为 NULL，此时函数无操作）
 */
LV00_PUBLIC_API void lv00_derivation_log_destroy(Lv00DerivationLog *log);

/**
 * @brief 清空推导日志
 *
 * 清除所有条目，重置统计信息，但不释放日志结构本身。
 *
 * @param[in] log 推导日志指针
 */
LV00_PUBLIC_API void lv00_derivation_log_clear(Lv00DerivationLog *log);

/**
 * @brief 激活/禁用推导日志
 *
 * 禁用时，所有追加操作为空操作（不分配内存）。
 *
 * @param[in] log    推导日志指针
 * @param[in] active true 激活，false 禁用
 */
LV00_PUBLIC_API void lv00_derivation_log_set_active(Lv00DerivationLog *log, bool active);

/**
 * @brief 检查推导日志是否激活
 *
 * @param[in] log 推导日志指针
 * @return true 激活，false 禁用
 */
LV00_PUBLIC_API bool lv00_derivation_log_is_active(const Lv00DerivationLog *log);

/**
 * @brief 追加一条推导日志条目
 *
 * 向日志中追加一条新的推导记录。如果日志未激活或已达到最大容量，
 * 则静默忽略（不报错）。
 *
 * @param[in] log         推导日志指针
 * @param[in] type        条目类型
 * @param[in] rule_name   规则名称（可为 NULL）
 * @param[in] description 描述信息（可为 NULL）
 * @param[in] elapsed_us  本步耗时（微秒）
 * @return true 成功追加，false 参数无效或容量已满
 */
LV00_PUBLIC_API bool lv00_derivation_log_append(Lv00DerivationLog *log,
                                                Lv00DerivationEntryType type,
                                                const char *rule_name,
                                                const char *description,
                                                uint64_t elapsed_us);

/**
 * @brief 获取指定索引的日志条目
 *
 * @param[in] log   推导日志指针
 * @param[in] index 条目索引（0-based）
 * @return 条目指针（指向日志内部存储，勿释放），索引越界返回 NULL
 */
LV00_PUBLIC_API const Lv00DerivationEntry *lv00_derivation_log_get_entry(
    const Lv00DerivationLog *log, int index);

/**
 * @brief 获取日志条目数量
 *
 * @param[in] log 推导日志指针
 * @return 条目数量
 */
LV00_PUBLIC_API int lv00_derivation_log_get_count(const Lv00DerivationLog *log);

/**
 * @brief 按类型统计日志条目数量
 *
 * @param[in] log  推导日志指针
 * @param[in] type 要统计的条目类型
 * @return 匹配类型的条目数量
 */
LV00_PUBLIC_API int lv00_derivation_log_count_by_type(const Lv00DerivationLog *log,
                                                      Lv00DerivationEntryType type);

/**
 * @brief 获取日志总耗时
 *
 * @param[in] log 推导日志指针
 * @return 总耗时（微秒）
 */
LV00_PUBLIC_API uint64_t lv00_derivation_log_get_total_elapsed(const Lv00DerivationLog *log);

/**
 * @brief 将推导日志导出为文本
 *
 * 生成人类可读的推导过程文本，每行一个条目，包含步骤号、类型、规则、描述和耗时。
 *
 * @param[in] log      推导日志指针
 * @param[out] buf     输出缓冲区（调用者分配）
 * @param[in] buf_size 缓冲区大小
 * @return 实际写入的字符数（不含终止符），缓冲区不足时返回所需大小
 */
LV00_PUBLIC_API int lv00_derivation_log_format(const Lv00DerivationLog *log,
                                                char *buf, size_t buf_size);

/**
 * @brief 条目类型转字符串
 *
 * @param[in] type 条目类型
 * @return 类型名称字符串（静态存储，无需释放）
 */
LV00_PUBLIC_API const char *lv00_derivation_entry_type_to_string(Lv00DerivationEntryType type);

/* ============================================================
 * 性能计时器 API
 * ============================================================ */

/**
 * @brief 创建性能计时器
 *
 * 分配并初始化一个性能计时器实例。
 *
 * @return 新分配的计时器指针，失败返回 NULL
 */
LV00_PUBLIC_API Lv00PerfTimer *lv00_perf_timer_create(void);

/**
 * @brief 销毁性能计时器
 *
 * @param[in] timer 计时器指针（可为 NULL，此时函数无操作）
 */
LV00_PUBLIC_API void lv00_perf_timer_destroy(Lv00PerfTimer *timer);

/**
 * @brief 重置性能计时器
 *
 * 清除所有计时区间和统计信息，恢复到初始状态。
 *
 * @param[in] timer 计时器指针
 */
LV00_PUBLIC_API void lv00_perf_timer_reset(Lv00PerfTimer *timer);

/**
 * @brief 开始一次计时区间
 *
 * 将当前时间压入计时栈，开始一个新区间。
 * 支持嵌套调用（最多 LV00_PERF_TIMER_MAX_DEPTH 层）。
 *
 * @param[in] timer 计时器指针
 * @param[in] label 区间标签（可为 NULL，默认为空字符串）
 * @return true 成功开始，false 栈溢出或参数无效
 */
LV00_PUBLIC_API bool lv00_perf_timer_begin(Lv00PerfTimer *timer, const char *label);

/**
 * @brief 结束最近一次计时区间
 *
 * 弹出计时栈顶区间，记录结束时间和耗时。
 * 必须与 lv00_perf_timer_begin 配对使用。
 *
 * @param[in] timer 计时器指针
 * @return true 成功结束，false 栈为空或参数无效
 */
LV00_PUBLIC_API bool lv00_perf_timer_end(Lv00PerfTimer *timer);

/**
 * @brief 获取当前嵌套深度
 *
 * @param[in] timer 计时器指针
 * @return 当前嵌套深度（0 表示未在计时中）
 */
LV00_PUBLIC_API int lv00_perf_timer_get_depth(const Lv00PerfTimer *timer);

/**
 * @brief 获取总累计耗时
 *
 * @param[in] timer 计时器指针
 * @return 总累计耗时（微秒）
 */
LV00_PUBLIC_API uint64_t lv00_perf_timer_get_total_elapsed(const Lv00PerfTimer *timer);

/**
 * @brief 获取总调用次数
 *
 * @param[in] timer 计时器指针
 * @return begin 被调用的总次数
 */
LV00_PUBLIC_API uint64_t lv00_perf_timer_get_call_count(const Lv00PerfTimer *timer);

/**
 * @brief 获取最近一次完成的区间耗时
 *
 * @param[in] timer 计时器指针
 * @return 最近一次完成区间的耗时（微秒），无已完成区间返回 0
 */
LV00_PUBLIC_API uint64_t lv00_perf_timer_get_last_elapsed(const Lv00PerfTimer *timer);

/**
 * @brief 将计时器统计信息格式化为字符串
 *
 * 生成人类可读的性能摘要，包含总耗时、调用次数、平均耗时等。
 *
 * @param[in] timer    计时器指针
 * @param[out] buf     输出缓冲区（调用者分配）
 * @param[in] buf_size 缓冲区大小
 * @return 实际写入的字符数（不含终止符），缓冲区不足时返回所需大小
 */
LV00_PUBLIC_API int lv00_perf_timer_format(const Lv00PerfTimer *timer,
                                            char *buf, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* LV00_DEBUG_TRACE_H */
