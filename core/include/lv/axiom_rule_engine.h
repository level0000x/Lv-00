/**
 * @file axiom_rule_engine.h
 * @brief 公理规则引擎 —— 可配置规则库与难度分级
 *
 * @details 提供灵活的公理规则管理系统：
 *   1. 规则定义：支持多种规则类型和触发条件
 *   2. 规则库管理：加载、保存、验证规则
 *   3. 难度分级：自动评估规则复杂度
 *   4. 规则匹配：高效的模式匹配算法
 *   5. 规则推荐：根据上下文推荐适用规则
 *
 * @author Lv-00 Project
 * @version 1.1.0
 */
#ifndef lv_AXIOM_RULE_ENGINE_H
#define lv_AXIOM_RULE_ENGINE_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "constraint_graph.h"
#include "proof.h"
/* ============== 配置常量 ============== */
/** 规则名称最大长度 */
#define lv_RULE_NAME_MAX_LEN 128
/** 规则描述最大长度 */
#define lv_RULE_DESC_MAX_LEN 512
/** 规则前提最大数量 */
#define lv_RULE_MAX_PREMISES 16
/** 规则结论最大数量 */
#define lv_RULE_MAX_CONCLUSIONS 8
/** 规则变量最大数量 */
#define lv_RULE_MAX_VARIABLES 32
/** 难度级别数量 */
#define lv_DIFFICULTY_LEVELS 10
/* ============== 前向声明 ============== */
typedef struct lvRule lvRule;
typedef struct lvRuleLibrary lvRuleLibrary;
typedef struct lvRuleMatch lvRuleMatch;
typedef struct lvDifficultyAssessment lvDifficultyAssessment;
/* ============== 规则类型 ============== */
/**
 * @brief 规则类型枚举
 */
typedef enum {
    RULE_TYPE_INFERENCE,  /**< 推理规则（前提 -> 结论） */
    RULE_TYPE_REWRITE,    /**< 重写规则（左部 -> 右部） */
    RULE_TYPE_AXIOM,      /**< 公理（无条件成立） */
    RULE_TYPE_DEFINITION, /**< 定义（展开/收起） */
    RULE_TYPE_THEOREM,    /**< 定理（需要证明） */
    RULE_TYPE_LEMMA,      /**< 引理 */
    RULE_TYPE_TACTIC,     /**< 策略（复合规则） */
    RULE_TYPE_CONSTRUCTOR /**< 构造规则 */
} lvRuleType;
/**
 * @brief 规则优先级
 */
typedef enum {
    RULE_PRIORITY_LOWEST = 0,
    RULE_PRIORITY_LOW = 25,
    RULE_PRIORITY_NORMAL = 50,
    RULE_PRIORITY_HIGH = 75,
    RULE_PRIORITY_HIGHEST = 100
} lvRulePriority;
/**
 * @brief 规则状态
 */
typedef enum {
    RULE_STATUS_DISABLED,    /**< 禁用 */
    RULE_STATUS_ENABLED,     /**< 启用 */
    RULE_STATUS_DEPRECATED,  /**< 已弃用 */
    RULE_STATUS_EXPERIMENTAL /**< 实验性 */
} lvRuleStatus;
/**
 * @brief 难度维度
 */
typedef enum {
    DIFF_DIM_STRUCTURAL,    /**< 结构复杂度 */
    DIFF_DIM_CONCEPTUAL,    /**< 概念难度 */
    DIFF_DIM_COMPUTATIONAL, /**< 计算复杂度 */
    DIFF_DIM_CREATIVE,      /**< 创造性要求 */
    DIFF_DIM_KNOWLEDGE,     /**< 知识依赖 */
    DIFF_DIM_COUNT          /**< 维度数量 */
} lvDifficultyDimension;
/* ============== 规则条件 ============== */
/**
 * @brief 条件类型
 */
typedef enum {
    COND_TYPE_PATTERN_MATCH, /**< 模式匹配 */
    COND_TYPE_TYPE_CHECK,    /**< 类型检查 */
    COND_TYPE_VALUE_COMPARE, /**< 值比较 */
    COND_TYPE_EXISTS,        /**< 存在性检查 */
    COND_TYPE_FORALL,        /**< 全称检查 */
    COND_TYPE_CUSTOM         /**< 自定义条件 */
} lvConditionType;
/**
 * @brief 规则条件
 */
typedef struct {
    lvConditionType type; /**< 条件类型 */
    char pattern[256];    /**< 模式字符串 */
    char variable[64];    /**< 相关变量 */
    int int_param;        /**< 整数参数 */
    double float_param;   /**< 浮点参数 */
    bool (*custom_check)(const ConstraintGraph *graph, const void *context);
} lvRuleCondition;
/* ============== 规则结构 ============== */
/**
 * @brief 规则变量
 */
typedef struct {
    char name[64];     /**< 变量名 */
    char type[64];     /**< 类型约束 */
    bool is_bound;     /**< 是否已绑定 */
    int bound_node_id; /**< 绑定的节点 ID */
} lvRuleVariable;
/**
 * @brief 规则前提
 */
typedef struct {
    char pattern[256];           /**< 前提模式 */
    lvRuleCondition *conditions; /**< 额外条件 */
    uint32_t condition_count;    /**< 条件数量 */
    bool is_optional;            /**< 是否可选 */
} lvRulePremise;
/**
 * @brief 规则结论
 */
typedef struct {
    char pattern[256];       /**< 结论模式 */
    char justification[256]; /**< 证明理由 */
    TrustColor trust_color;  /**< 信任颜色 */
} lvRuleConclusion;
/**
 * @brief 规则结构
 */
struct lvRule {
    /* 基本信息 */
    uint32_t id;                            /**< 规则 ID */
    char name[lv_RULE_NAME_MAX_LEN];        /**< 规则名称 */
    char description[lv_RULE_DESC_MAX_LEN]; /**< 描述 */
    lvRuleType type;                        /**< 规则类型 */
    lvRuleStatus status;                    /**< 规则状态 */
    /* 规则内容 */
    lvRuleVariable variables[lv_RULE_MAX_VARIABLES];       /**< 变量 */
    uint32_t var_count;                                    /**< 变量数量 */
    lvRulePremise premises[lv_RULE_MAX_PREMISES];          /**< 前提 */
    uint32_t premise_count;                                /**< 前提数量 */
    lvRuleConclusion conclusions[lv_RULE_MAX_CONCLUSIONS]; /**< 结论 */
    uint32_t conclusion_count;                             /**< 结论数量 */
    /* 元数据 */
    lvRulePriority priority;                      /**< 优先级 */
    uint32_t difficulty_score;                    /**< 难度分数 (0-1000) */
    uint32_t difficulty_level;                    /**< 难度等级 (1-10) */
    double difficulty_dimensions[DIFF_DIM_COUNT]; /**< 各维度难度 */
    /* 统计信息 */
    uint64_t apply_count;     /**< 应用次数 */
    uint64_t success_count;   /**< 成功次数 */
    double avg_apply_time_ms; /**< 平均应用时间 */
    /* 依赖关系 */
    uint32_t *dependency_ids;  /**< 依赖规则 ID */
    uint32_t dependency_count; /**< 依赖数量 */
    /* 标签 */
    char **tags;            /**< 标签数组 */
    uint32_t tag_count;     /**< 标签数量 */
    uint32_t tag_capacity;  /**< 标签数组容量 */
    /* 所属包 */
    char package_name[64]; /**< 所属公理包名称 */
};
/* ============== 规则匹配 ============== */
/**
 * @brief 规则匹配结果
 */
struct lvRuleMatch {
    lvRule *rule;                                   /**< 匹配的规则 */
    lvRuleVariable bindings[lv_RULE_MAX_VARIABLES]; /**< 变量绑定 */
    uint32_t binding_count;                         /**< 绑定数量 */
    double confidence;                              /**< 匹配置信度 */
    uint32_t matched_premises;                      /**< 匹配的前提数量 */
    bool is_complete;                               /**< 是否完全匹配 */
};
/* ============== 难度评估 ============== */
/**
 * @brief 难度评估结果
 */
struct lvDifficultyAssessment {
    uint32_t overall_score;            /**< 总分 (0-1000) */
    uint32_t level;                    /**< 等级 (1-10) */
    double dimensions[DIFF_DIM_COUNT]; /**< 各维度分数 */
    char breakdown[1024];              /**< 详细分析 */
    char recommendation[512];          /**< 推荐建议 */
};
/* ============== 规则库 ============== */
/**
 * @brief 规则库配置
 */
typedef struct {
    uint32_t max_rules;          /**< 最大规则数 */
    bool auto_validate;          /**< 自动验证规则 */
    bool auto_difficulty;        /**< 自动评估难度 */
    bool enable_cache;           /**< 启用匹配缓存 */
    const char *default_package; /**< 默认公理包 */
} lvRuleLibraryConfig;
/**
 * @brief 规则库结构
 */
struct lvRuleLibrary {
    lvRule **rules;         /**< 规则数组 */
    uint32_t rule_count;    /**< 规则数量 */
    uint32_t rule_capacity; /**< 规则容量 */
    /* 索引 */
    uint32_t *id_index;   /**< ID 索引 */
    char **name_index;    /**< 名称索引 */
    uint32_t *type_index; /**< 类型索引 */
    /* 缓存 */
    void *match_cache; /**< 匹配缓存 */
    /* 配置 */
    lvRuleLibraryConfig config;
    /* 统计 */
    uint64_t total_matches; /**< 总匹配次数 */
    uint64_t cache_hits;    /**< 缓存命中次数 */
};
/* ============== 规则库管理 ============== */
/**
 * @brief 创建规则库
 * @param config 配置（NULL 使用默认）
 * @return 新规则库
 */
lvRuleLibrary *lv_rule_library_create(const lvRuleLibraryConfig *config);
/**
 * @brief 销毁规则库
 * @param library 规则库指针
 */
void lv_rule_library_destroy(lvRuleLibrary *library);
/**
 * @brief 添加规则到规则库
 * @param library 规则库
 * @param rule 规则
 * @return 是否成功
 */
bool lv_rule_library_add(lvRuleLibrary *library, lvRule *rule);
/**
 * @brief 从规则库移除规则
 * @param library 规则库
 * @param rule_id 规则 ID
 * @return 是否成功
 */
bool lv_rule_library_remove(lvRuleLibrary *library, uint32_t rule_id);
/**
 * @brief 根据 ID 获取规则
 * @param library 规则库
 * @param rule_id 规则 ID
 * @return 规则指针（不存在返回 NULL）
 */
lvRule *lv_rule_library_get_by_id(const lvRuleLibrary *library, uint32_t rule_id);
/**
 * @brief 根据名称获取规则
 * @param library 规则库
 * @param name 规则名称
 * @return 规则指针（不存在返回 NULL）
 */
lvRule *lv_rule_library_get_by_name(const lvRuleLibrary *library, const char *name);
/**
 * @brief 获取指定类型的所有规则
 * @param library 规则库
 * @param type 规则类型
 * @param out_rules 输出规则数组
 * @param max_count 最大数量
 * @return 实际数量
 */
uint32_t lv_rule_library_get_by_type(const lvRuleLibrary *library, lvRuleType type, lvRule **out_rules,
                                     uint32_t max_count);
/**
 * @brief 获取指定难度范围的规则
 * @param library 规则库
 * @param min_level 最小难度等级
 * @param max_level 最大难度等级
 * @param out_rules 输出规则数组
 * @param max_count 最大数量
 * @return 实际数量
 */
uint32_t lv_rule_library_get_by_difficulty(const lvRuleLibrary *library, uint32_t min_level, uint32_t max_level,
                                           lvRule **out_rules, uint32_t max_count);
/**
 * @brief 按标签搜索规则
 * @param library 规则库
 * @param tag 标签
 * @param out_rules 输出规则数组
 * @param max_count 最大数量
 * @return 实际数量
 */
uint32_t lv_rule_library_search_by_tag(const lvRuleLibrary *library, const char *tag, lvRule **out_rules,
                                       uint32_t max_count);
/* ============== 规则创建 ============== */
/**
 * @brief 创建规则
 * @param name 规则名称
 * @param type 规则类型
 * @return 新规则
 */
lvRule *lv_rule_create(const char *name, lvRuleType type);
/**
 * @brief 销毁规则
 * @param rule 规则指针
 */
void lv_rule_destroy(lvRule *rule);
/**
 * @brief 复制规则
 * @param rule 源规则
 * @return 新规则副本
 */
lvRule *lv_rule_copy(const lvRule *rule);
/**
 * @brief 设置规则描述
 * @param rule 规则
 * @param description 描述
 * @return 是否成功
 */
bool lv_rule_set_description(lvRule *rule, const char *description);
/**
 * @brief 添加变量
 * @param rule 规则
 * @param name 变量名
 * @param type 类型约束
 * @return 是否成功
 */
bool lv_rule_add_variable(lvRule *rule, const char *name, const char *type);
/**
 * @brief 添加前提
 * @param rule 规则
 * @param pattern 前提模式
 * @param is_optional 是否可选
 * @return 是否成功
 */
bool lv_rule_add_premise(lvRule *rule, const char *pattern, bool is_optional);
/**
 * @brief 添加结论
 * @param rule 规则
 * @param pattern 结论模式
 * @param trust_color 信任颜色
 * @return 是否成功
 */
bool lv_rule_add_conclusion(lvRule *rule, const char *pattern, TrustColor trust_color);
/**
 * @brief 添加标签
 * @param rule 规则
 * @param tag 标签
 * @return 是否成功
 */
bool lv_rule_add_tag(lvRule *rule, const char *tag);
/**
 * @brief 设置规则优先级
 * @param rule 规则
 * @param priority 优先级
 */
void lv_rule_set_priority(lvRule *rule, lvRulePriority priority);
/**
 * @brief 设置规则状态
 * @param rule 规则
 * @param status 状态
 */
void lv_rule_set_status(lvRule *rule, lvRuleStatus status);
/* ============== 难度评估 ============== */
/**
 * @brief 评估规则难度
 * @param rule 规则
 * @return 难度评估结果（调用者负责释放）
 */
lvDifficultyAssessment *lv_rule_assess_difficulty(const lvRule *rule);
/**
 * @brief 销毁难度评估结果
 * @param assessment 评估结果
 */
void lv_difficulty_assessment_destroy(lvDifficultyAssessment *assessment);
/**
 * @brief 评估证明步骤难度
 * @param step 证明步骤
 * @param graph 约束图
 * @return 难度评估结果
 */
lvDifficultyAssessment *lv_proof_step_assess_difficulty(const ProofStep *step, const ConstraintGraph *graph);
/**
 * @brief 评估命题难度
 * @param prop 命题
 * @return 难度评估结果
 */
lvDifficultyAssessment *lv_proposition_assess_difficulty(const Proposition *prop);
/**
 * @brief 获取难度等级描述
 * @param level 难度等级 (1-10)
 * @return 描述字符串
 */
const char *lv_difficulty_level_to_string(uint32_t level);
/**
 * @brief 获取难度维度名称
 * @param dimension 维度
 * @return 名称字符串
 */
const char *lv_difficulty_dimension_to_string(lvDifficultyDimension dimension);
/* ============== 规则匹配 ============== */
/**
 * @brief 在约束图中查找匹配的规则
 * @param library 规则库
 * @param graph 约束图
 * @param context 证明上下文
 * @param out_matches 输出匹配数组
 * @param max_count 最大数量
 * @return 实际匹配数量
 */
uint32_t lv_rule_find_matches(const lvRuleLibrary *library, const ConstraintGraph *graph, const ProofNavigator *context,
                              lvRuleMatch **out_matches, uint32_t max_count);
/**
 * @brief 应用规则匹配
 * @param match 规则匹配
 * @param graph 约束图
 * @param context 证明上下文
 * @param out_steps 输出生成的证明步骤
 * @param max_steps 最大步骤数
 * @return 实际生成步骤数
 */
uint32_t lv_rule_apply_match(const lvRuleMatch *match, ConstraintGraph *graph, ProofNavigator *context,
                             ProofStep **out_steps, uint32_t max_steps);
/**
 * @brief 销毁规则匹配
 * @param match 匹配指针
 */
void lv_rule_match_destroy(lvRuleMatch *match);
/**
 * @brief 检查规则是否适用于约束图
 * @param rule 规则
 * @param graph 约束图
 * @param context 证明上下文
 * @return 是否适用
 */
bool lv_rule_is_applicable(const lvRule *rule, const ConstraintGraph *graph, const ProofNavigator *context);
/* ============== 规则推荐 ============== */
/**
 * @brief 规则推荐结果
 */
typedef struct {
    lvRule **rules; /**< 推荐规则数组 */
    double *scores; /**< 推荐分数 */
    uint32_t count; /**< 推荐数量 */
    char *reason;   /**< 推荐理由 */
} lvRuleRecommendation;
/**
 * @brief 根据上下文推荐规则
 * @param library 规则库
 * @param graph 约束图
 * @param context 证明上下文
 * @param max_count 最大推荐数量
 * @return 推荐结果（调用者负责释放）
 */
lvRuleRecommendation *lv_rule_recommend(const lvRuleLibrary *library, const ConstraintGraph *graph,
                                        const ProofNavigator *context, uint32_t max_count);
/**
 * @brief 销毁规则推荐
 * @param rec 推荐指针
 */
void lv_rule_recommendation_destroy(lvRuleRecommendation *rec);
/* ============== 规则序列化 ============== */
/**
 * @brief 规则序列化为 JSON
 * @param rule 规则
 * @return JSON 字符串
 */
char *lv_rule_to_json(const lvRule *rule);
/**
 * @brief 从 JSON 解析规则
 * @param json JSON 字符串
 * @return 新规则
 */
lvRule *lv_rule_from_json(const char *json);
/**
 * @brief 规则库保存到文件
 * @param library 规则库
 * @param path 文件路径
 * @return 是否成功
 */
bool lv_rule_library_save(const lvRuleLibrary *library, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* lv_AXIOM_RULE_ENGINE_H */
