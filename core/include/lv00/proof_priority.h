/**
 * @file proof_priority.h
 * @brief 推理优先级系统 —— 公理/规则的优先级标记与调度提示
 *
 * 提供定理和推理规则的优先级标记机制，用于在自动证明搜索中
 * 指导调度器优先使用高优先级规则，延迟低优先级规则。
 * 借鉴启发式搜索的思想，高优先级规则能更快地缩小搜索空间。
 */

#ifndef LV00_PROOF_PRIORITY_H
#define LV00_PROOF_PRIORITY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ============== 定理/规则优先级 ============== */

/**
 * @brief 定理/推理规则的优先级等级
 *
 * 四个优先级等级用于指导证明搜索的调度策略：
 * - PRIORITY_LOW:    低优先级 —— 可推迟到最后使用（如仅用于规范化的规则）
 * - PRIORITY_NORMAL:  普通优先级 —— 默认等级（大多数规则）
 * - PRIORITY_HIGH:    高优先级 —— 优先使用（如化简规则、强约束）
 * - PRIORITY_URGENT:  紧急优先级 —— 立即使用（如矛盾检测规则、安全问题）
 */
typedef enum {
    PRIORITY_LOW    = 0,  /**< 低优先级 —— 延迟执行 */
    PRIORITY_NORMAL = 1,  /**< 普通优先级 —— 默认等级 */
    PRIORITY_HIGH   = 2,  /**< 高优先级 —— 优先执行 */
    PRIORITY_URGENT = 3   /**< 紧急优先级 —— 立即执行 */
} Lv00TheoremPriority;

/* ============== 优先级标记结构 ============== */

/**
 * @brief 优先级标记 —— 附加在公理/规则上的调度元数据
 *
 * 通过 priority 字段标记规则的紧急程度，scheduler_hint 字段
 * 提供调度器的附加提示（如"先处理矛盾"、"可批量应用"等）。
 *
 * 典型使用场景：
 * - 矛盾检测规则设置为 PRIORITY_URGENT，尽快发现不可解分支
 * - 规范化规则设置为 PRIORITY_LOW，在主要推理完成后再执行
 * - 强约束生成规则设置为 PRIORITY_HIGH，缩小搜索空间
 */
typedef struct {
    int                rule_id;          /**< 规则/公理的唯一标识符 */
    char               rule_name[128];   /**< 规则/公理名称 */
    Lv00TheoremPriority priority;        /**< 优先级等级 */
    int                weight;           /**< 权重系数（0-100，用于精细化排序） */
    bool               is_exhausted;     /**< 是否已耗尽（优先级规则已被完全应用） */
    int                apply_count;      /**< 已应用次数（用于统计） */
    int                max_applications; /**< 最大允许应用次数（0=无限制） */
    char              *scheduler_hint;   /**< 调度器提示字符串（可为 NULL） */
} Lv00PriorityTag;

/* ============== 优先级调度器状态 ============== */

/**
 * @brief 优先级调度器 —— 管理多个优先级级别的规则队列
 *
 * 维护四个优先级的规则队列，按 URGENT > HIGH > NORMAL > LOW 顺序处理。
 * 支持规则注册、优先级动态调整、耗尽标记和统计查询。
 *
 * 调度策略：
 * 1. 首先处理所有 PRIORITY_URGENT 的规则，直到全部耗尽
 * 2. 然后处理 PRIORITY_HIGH 的规则
 * 3. 接着处理 PRIORITY_NORMAL 的规则
 * 4. 最后处理 PRIORITY_LOW 的规则
 *
 * 在每级内部，按 weight 降序排列（weight 越高越优先）。
 */
typedef struct {
    Lv00PriorityTag **urgent_queue;  /**< 紧急优先级规则队列 */
    int               urgent_count;
    int               urgent_capacity;

    Lv00PriorityTag **high_queue;    /**< 高优先级规则队列 */
    int               high_count;
    int               high_capacity;

    Lv00PriorityTag **normal_queue;  /**< 普通优先级规则队列 */
    int               normal_count;
    int               normal_capacity;

    Lv00PriorityTag **low_queue;     /**< 低优先级规则队列 */
    int               low_count;
    int               low_capacity;

    int  current_priority_level;     /**< 当前正在处理的优先级等级 */
    int  total_rules;                /**< 注册的规则总数 */
    bool sort_needed;                /**< 是否需要重新排序 */
} Lv00PriorityScheduler;

/* ============== API ============== */

/**
 * @brief 创建优先级标记
 *
 * @param rule_name       规则名称（不可为空，内部复制）
 * @param rule_id         规则ID
 * @param priority        优先级等级
 * @param weight          权重系数（0-100）
 * @param max_applications 最大应用次数（0=无限制）
 * @param scheduler_hint  调度提示（可为 NULL，内部复制）
 * @return 新分配的优先级标记，失败返回 NULL
 */
Lv00PriorityTag *lv00_priority_tag_create(const char *rule_name, int rule_id,
                                           Lv00TheoremPriority priority, int weight,
                                           int max_applications, const char *scheduler_hint);

/**
 * @brief 销毁优先级标记
 *
 * @param tag  优先级标记指针（可为 NULL）
 */
void lv00_priority_tag_destroy(Lv00PriorityTag *tag);

/**
 * @brief 创建优先级调度器
 *
 * @return 新分配的调度器指针，失败返回 NULL
 */
Lv00PriorityScheduler *lv00_priority_scheduler_create(void);

/**
 * @brief 销毁优先级调度器
 *
 * 释放所有已注册的优先级标记和调度器自身。
 *
 * @param scheduler  调度器指针（可为 NULL）
 */
void lv00_priority_scheduler_destroy(Lv00PriorityScheduler *scheduler);

/**
 * @brief 向调度器注册一个优先级规则
 *
 * 根据规则的优先级等级，将其加入对应的队列。
 * 注册后自动标记 sort_needed = true。
 *
 * @param scheduler  调度器
 * @param tag        优先级标记
 * @return true 注册成功，false 参数无效或内存分配失败
 */
bool lv00_priority_scheduler_register(Lv00PriorityScheduler *scheduler, Lv00PriorityTag *tag);

/**
 * @brief 获取下一个应执行的规则
 *
 * 按 URGENT > HIGH > NORMAL > LOW 的顺序查找下一个未耗尽的规则。
 * 同级别内部按 weight 降序返回。
 * 如果所有规则都已耗尽，返回 NULL。
 *
 * @param scheduler  调度器
 * @return 下一个应执行的规则标记，无可用规则返回 NULL
 */
Lv00PriorityTag *lv00_priority_scheduler_next(Lv00PriorityScheduler *scheduler);

/**
 * @brief 标记一个规则为已耗尽
 *
 * 当规则的 apply_count 达到 max_applications 时自动标记。
 * 也可手动调用此函数提前耗尽某个规则。
 *
 * @param tag  优先级标记
 */
void lv00_priority_tag_exhaust(Lv00PriorityTag *tag);

/**
 * @brief 动态调整规则的优先级
 *
 * 允许在运行时将规则从一个优先级级别移动到另一个。
 * 自动从旧队列移除并加入新队列。
 *
 * @param scheduler   调度器
 * @param rule_id     规则ID
 * @param new_priority 新的优先级等级
 * @return true 调整成功，false 未找到规则或参数无效
 */
bool lv00_priority_scheduler_reprioritize(Lv00PriorityScheduler *scheduler,
                                           int rule_id, Lv00TheoremPriority new_priority);

/**
 * @brief 记录规则被应用了一次
 *
 * 递增规则的 apply_count，如果达到 max_applications 则自动标记耗尽。
 *
 * @param tag  优先级标记
 */
void lv00_priority_tag_record_application(Lv00PriorityTag *tag);

/**
 * @brief 重置调度器中的所有规则状态
 *
 * 将所有规则的 is_exhausted 重新设为 false，apply_count 清零。
 * 用于新一轮证明搜索时恢复初始状态。
 *
 * @param scheduler  调度器
 */
void lv00_priority_scheduler_reset(Lv00PriorityScheduler *scheduler);

/**
 * @brief 获取调度器的统计信息
 *
 * @param scheduler            调度器
 * @param out_total_rules      输出：注册的规则总数
 * @param out_remaining_rules  输出：尚未耗尽的规则数量
 * @param out_current_level    输出：当前正在处理的优先级等级
 */
void lv00_priority_scheduler_get_stats(const Lv00PriorityScheduler *scheduler,
                                        int *out_total_rules, int *out_remaining_rules,
                                        int *out_current_level);

/**
 * @brief 将优先级等级转换为中文字符串
 *
 * @param priority  优先级等级
 * @return 中文描述字符串（静态内存）
 */
const char *lv00_priority_to_string(Lv00TheoremPriority priority);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PROOF_PRIORITY_H */
