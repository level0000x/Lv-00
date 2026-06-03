/**
 * @file rule_registry.h
 * @brief 动态定理/规则注册表 —— 推理引擎的可扩展规则管理
 *
 * @details 提供运行时可动态增删的规则注册表，支持：
 *          - 规则注册/注销（按名称唯一标识）
 *          - 优先级排序（数值越小优先级越高）
 *          - 运行时启用/禁用规则
 *          - 批量应用：对给定命题依次尝试所有已启用且适用的规则
 *
 *          注册表使用动态数组存储规则副本，支持 O(n) 查找和 O(n) 删除。
 *          适用于中小规模规则集（< 1000 条规则）。
 *
 * @author Lv-00 Project
 * @version 3.5.0
 */

#ifndef LV00_RULE_REGISTRY_H
#define LV00_RULE_REGISTRY_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 规则适用性检查回调
 *
 * @param context     用户上下文（由 Lv00Rule.user_data 传入）
 * @param proposition 待检查的命题
 * @return 非零值表示规则适用，0 表示不适用
 */
typedef int (*Lv00RuleApplicabilityFunc)(void *context, void *proposition);

/**
 * @brief 规则应用回调
 *
 * @param context      用户上下文
 * @param proposition  待处理的命题
 * @param results      输出：结果数组（由回调分配，调用者释放）
 * @param result_count 输出：结果数量
 * @return 0 成功，非零值失败
 */
typedef int (*Lv00RuleApplyFunc)(void *context, void *proposition, void **results, int *result_count);

/**
 * @brief 推理规则描述符
 *
 * 每条规则包含名称、描述、优先级、启用状态以及两个回调函数。
 * 规则注册到注册表时会进行深拷贝（字符串通过 strdup 复制）。
 */
typedef struct {
    const char *name;           /**< 规则名称（唯一标识符，如 "modus_ponens"） */
    const char *description;    /**< 人类可读描述 */
    int priority;               /**< 优先级（数值越小优先级越高，默认 0） */
    bool enabled;               /**< 是否启用（可在运行时切换） */
    Lv00RuleApplicabilityFunc can_apply; /**< 适用性检查函数 */
    Lv00RuleApplyFunc apply;            /**< 规则应用函数 */
    void *user_data;            /**< 透传给回调的用户数据指针 */
} Lv00Rule;

/**
 * @brief 规则注册表（不透明类型）
 *
 * 内部使用动态数组存储已注册的规则副本。
 * 通过 lv00_rule_registry_create() 创建，lv00_rule_registry_destroy() 销毁。
 */
typedef struct Lv00RuleRegistry Lv00RuleRegistry;

/* ============== 生命周期管理 ============== */

/**
 * @brief 创建规则注册表
 * @return 新分配的注册表实例，失败返回 NULL
 */
Lv00RuleRegistry *lv00_rule_registry_create(void);

/**
 * @brief 销毁规则注册表并释放所有已注册规则的资源
 * @param registry 注册表指针（可为 NULL，此时无操作）
 */
void lv00_rule_registry_destroy(Lv00RuleRegistry *registry);

/* ============== 规则注册/注销 ============== */

/**
 * @brief 向注册表添加一条规则（深拷贝）
 *
 * 如果已存在同名规则，则拒绝添加。
 *
 * @param registry 注册表
 * @param rule     规则描述符（会被深拷贝，调用者可随后释放原数据）
 * @return 0 成功，-1 参数无效，-2 同名规则已存在，-3 内存不足
 */
int lv00_rule_registry_add(Lv00RuleRegistry *registry, const Lv00Rule *rule);

/**
 * @brief 从注册表中移除指定名称的规则
 *
 * @param registry 注册表
 * @param name     要移除的规则名称
 * @return true 成功移除，false 未找到或参数无效
 */
bool lv00_rule_registry_remove(Lv00RuleRegistry *registry, const char *name);

/* ============== 规则查询 ============== */

/**
 * @brief 获取注册表中的规则数量
 * @param registry 注册表
 * @return 规则数量，registry 为 NULL 时返回 0
 */
int lv00_rule_registry_count(const Lv00RuleRegistry *registry);

/**
 * @brief 按索引获取规则（只读访问）
 *
 * @param registry 注册表
 * @param index    规则索引（0 <= index < count）
 * @return 规则指针（属于注册表内部存储，不可修改或释放），越界返回 NULL
 */
const Lv00Rule *lv00_rule_registry_get(const Lv00RuleRegistry *registry, int index);

/**
 * @brief 按名称查找规则（只读访问）
 *
 * @param registry 注册表
 * @param name     规则名称
 * @return 规则指针，未找到返回 NULL
 */
const Lv00Rule *lv00_rule_registry_find(const Lv00RuleRegistry *registry, const char *name);

/* ============== 规则启用/禁用 ============== */

/**
 * @brief 启用或禁用指定规则
 *
 * @param registry 注册表
 * @param name     规则名称
 * @param enabled  true 启用，false 禁用
 * @return true 成功，false 未找到规则
 */
bool lv00_rule_registry_enable(Lv00RuleRegistry *registry, const char *name, bool enabled);

/* ============== 批量应用 ============== */

/**
 * @brief 对给定命题应用所有适用的已启用规则
 *
 * 按优先级排序后依次检查每条已启用规则的 can_apply 回调。
 * 对适用的规则调用 apply 回调，收集结果到 results 数组。
 *
 * @param registry    注册表
 * @param context     用户上下文（透传给回调）
 * @param proposition 待处理的命题
 * @param results     输出：结果指针数组（调用者需逐个释放）
 * @param max_results results 数组的最大容量
 * @return 实际产生的结果数量，负值表示错误
 */
int lv00_rule_registry_apply_all(Lv00RuleRegistry *registry, void *context, void *proposition,
                                  void **results, int max_results);

#ifdef __cplusplus
}
#endif

#endif /* LV00_RULE_REGISTRY_H */
