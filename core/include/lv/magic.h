/**
 * @file magic.h
 * @brief 编程魔法系统 - 基于 Lv-00 的咒语编程模拟器
 *
 * 本模块将 Lv-00 的核心系统映射为魔法概念：
 * - 符号坐标 → 符文 (Runes)
 * - 约束图 → 魔法阵 (Magic Arrays)
 * - 函数块 → 咒语 (Spells)
 * - 约束类型 → 元素 (Elements)
 */

#ifndef lv_MAGIC_H
#define lv_MAGIC_H

#include <stdbool.h>
#include <stddef.h>

#include "constraint_graph.h"
#include "func_block.h"
#include "symbolic_coord.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 魔法元素系统 (基于轰界四元素体系)
 * ============================================================ */

typedef enum {
    ELEMENT_FIRE,  /* 火元素 - 释放与热效应 */
    ELEMENT_WATER, /* 水元素 - 流动与相态调节 */
    ELEMENT_AIR,   /* 风元素 - 运动与压差 */
    ELEMENT_EARTH, /* 土元素 - 结构与承载 */
    ELEMENT_ETHER, /* 以太 - 第五元素 */
    ELEMENT_NONE   /* 无属性 */
} MagicElement;

/* 元素反应矩阵 */
typedef enum {
    ELEMENT_REACTION_NONE,    /* 无反应 */
    ELEMENT_REACTION_ENHANCE, /* 协同增强 */
    ELEMENT_REACTION_WEAKEN,  /* 相互削弱 */
    ELEMENT_REACTION_CONFLICT /* 对立冲突 */
} ElementReaction;

/* ============================================================
 * 符文系统 (基于符号坐标)
 * ============================================================ */

typedef struct Rune Rune;
typedef struct RuneSequence RuneSequence;

/**
 * @brief 符文 - 魔法的基本构建块，对应符号坐标
 *
 * 符文代表魔法阵上的一个节点，可以是：
 * - 有理数符文：精确值
 * - 代数数符文：可开方表达的值
 * - 二次扩张符文：二次根号形式
 * - 超越数符文：π、e 等
 */
struct Rune {
    SymbolicCoord *coord; /* 符文位置/值 */
    MagicElement element; /* 符文元素属性 */
    char *name;           /* 符文名称 */
    char *symbol;         /* 符文符号 */
    int power_level;      /* 符文强度等级 1-10 */
};

/* 符文序列 - 多个符文的组合 */
struct RuneSequence {
    Rune **runes;
    int rune_count;
    int capacity;
};

/* 符文管理 API */
lv_PUBLIC_API Rune *rune_create_rational(int64_t num, uint64_t denom, MagicElement element);
lv_PUBLIC_API Rune *rune_create_algebraic(double value, MagicElement element);
lv_PUBLIC_API Rune *rune_create_transcendental(const char *name, MagicElement element);
lv_PUBLIC_API Rune *rune_copy(const Rune *src);
lv_PUBLIC_API void rune_destroy(Rune *rune);
lv_PUBLIC_API char *rune_serialize(const Rune *rune);
lv_PUBLIC_API int rune_serialize_to_buffer(const Rune *rune, char *buf, int buf_size);
lv_PUBLIC_API Rune *rune_parse(const char *str);

/* 符文序列管理 */
lv_PUBLIC_API RuneSequence *rune_sequence_create(void);
lv_PUBLIC_API bool rune_sequence_add(RuneSequence *seq, Rune *rune);
lv_PUBLIC_API Rune *rune_sequence_get(const RuneSequence *seq, int index);
lv_PUBLIC_API int rune_sequence_length(const RuneSequence *seq);
lv_PUBLIC_API void rune_sequence_destroy(RuneSequence *seq);

/* 符文操作 */
lv_PUBLIC_API SymbolicCoord *rune_get_value(const Rune *rune);
lv_PUBLIC_API MagicElement rune_get_element(const Rune *rune);
lv_PUBLIC_API int rune_get_power(const Rune *rune);
lv_PUBLIC_API void rune_set_power(Rune *rune, int power);

/* ============================================================
 * 魔法阵系统 (基于约束图)
 * ============================================================ */

/**
 * @brief 魔法阵 - 对应约束图
 *
 * 魔法阵由符文（节点）和符文约束（边）组成，
 * 支持以下约束类型：
 * - 连接约束：符文能量流动
 * - 增强约束：元素协同
 * - 冲突约束：元素对立
 * - 相交约束：能量汇聚点
 */

typedef struct MagicArray MagicArray;

/* 魔法阵约束类型 */
typedef enum {
    ARRAY_CONNECTION,   /* 连接：能量流动路径 */
    ARRAY_ENHANCEMENT,  /* 增强：元素协同 */
    ARRAY_CONFLICT,     /* 冲突：元素对立 */
    ARRAY_INTERSECTION, /* 相交：能量汇聚 */
    ARRAY_CONTAINMENT,  /* 包含：区域包围 */
    ARRAY_BOUNDARY,     /* 边界：结界边缘 */
    ARRAY_CHANNEL,      /* 通道：能量传输线 */
    ARRAY_FOCUS         /* 焦点：能量集中点 */
} ArrayConstraintType;

/* 魔法阵创建/销毁 */
lv_PUBLIC_API MagicArray *magic_array_create(void);
lv_PUBLIC_API void magic_array_destroy(MagicArray *array);

/* 符文操作 */
lv_PUBLIC_API int magic_array_add_rune(MagicArray *array, Rune *rune);
lv_PUBLIC_API bool magic_array_remove_rune(MagicArray *array, int rune_index);
lv_PUBLIC_API Rune *magic_array_get_rune(const MagicArray *array, int rune_index);
lv_PUBLIC_API int magic_array_get_rune_count(const MagicArray *array);

/* 约束操作 */
lv_PUBLIC_API int magic_array_add_constraint(MagicArray *array, ArrayConstraintType type, int rune1_index, int rune2_index);
lv_PUBLIC_API bool magic_array_remove_constraint(MagicArray *array, int constraint_index);
lv_PUBLIC_API int magic_array_get_constraint_count(const MagicArray *array);

/* 魔法阵分析 */
bool magic_array_check_balance(const MagicArray *array); /* 检查元素平衡 */
lv_PUBLIC_API ElementReaction array_check_element_reaction(MagicElement e1, MagicElement e2);
lv_PUBLIC_API int array_count_elements(const MagicArray *array, MagicElement element);
double array_calculate_stability(const MagicArray *array); /* 稳定性评分 0-1 */

/* 魔法阵操作 */
lv_PUBLIC_API bool magic_array_merge(MagicArray *dest, const MagicArray *src);
lv_PUBLIC_API MagicArray *magic_array_copy(const MagicArray *src);
lv_PUBLIC_API char *magic_array_serialize(const MagicArray *array);
lv_PUBLIC_API MagicArray *magic_array_deserialize(const char *json);

/* ============================================================
 * 施法阶段系统 (基于轰界四阶段流程)
 * ============================================================ */

/* 施法四阶段 */
typedef enum {
    SPELL_STAGE_MOLDING,   /* 开模：定义法术形态 */
    SPELL_STAGE_PURIFYING, /* 提纯：元素纯化 */
    SPELL_STAGE_INFUSING,  /* 灌注：能量注入 */
    SPELL_STAGE_RELEASING  /* 释放：效果投射 */
} SpellStage;

/* 施法状态 */
typedef enum {
    SPELL_STATUS_IDLE,    /* 空闲 */
    SPELL_STATUS_CASTING, /* 施法中 */
    SPELL_STATUS_SUCCESS, /* 施法成功 */
    SPELL_STATUS_FAILED,  /* 施法失败 */
    SPELL_STATUS_BACKLASH /* 反噬 */
} SpellStatus;

/* ============================================================
 * 咒语系统 (基于函数块)
 * ============================================================ */

/**
 * @brief 咒语 - 对应函数块
 *
 * 咒语是封装好的施法流程，包含：
 * - 输入端口：施法参数
 * - 输出端口：施法效果
 * - 内部构造：符文序列和约束
 */

typedef struct Spell Spell;
typedef struct SpellBook SpellBook;

/* 咒语创建/销毁 */
lv_PUBLIC_API Spell *spell_create(const char *name);
lv_PUBLIC_API void spell_destroy(Spell *spell);

/* 咒语配置 */
lv_PUBLIC_API bool spell_set_input_count(Spell *spell, int count);
lv_PUBLIC_API bool spell_set_output_count(Spell *spell, int count);
lv_PUBLIC_API bool spell_set_description(Spell *spell, const char *desc);
bool spell_set_difficulty(Spell *spell, int difficulty); /* 1-10 */

/* 符文序列配置 */
lv_PUBLIC_API bool spell_configure_molding(Spell *spell, const RuneSequence *seq);
lv_PUBLIC_API bool spell_configure_purifying(Spell *spell, MagicElement element, double purity);
lv_PUBLIC_API bool spell_configure_infusing(Spell *spell, int threshold_level);
lv_PUBLIC_API bool spell_configure_releasing(Spell *spell, int range, int damage);

/* 咒语信息获取 */
lv_PUBLIC_API const char *spell_get_name(const Spell *spell);
lv_PUBLIC_API const char *spell_get_description(const Spell *spell);
lv_PUBLIC_API int spell_get_difficulty(const Spell *spell);
lv_PUBLIC_API int spell_get_input_count(const Spell *spell);
lv_PUBLIC_API int spell_get_output_count(const Spell *spell);
lv_PUBLIC_API SpellStage spell_get_current_stage(const Spell *spell);
lv_PUBLIC_API SpellStatus spell_get_status(const Spell *spell);

/* 咒语执行 */
lv_PUBLIC_API SpellStatus spell_cast(Spell *spell, MagicArray *array, SymbolicCoord **inputs, int input_count,
                       SymbolicCoord **outputs, int output_count);

/* 咒语验证 */
lv_PUBLIC_API bool spell_validate_structure(const Spell *spell);
lv_PUBLIC_API bool spell_check_element_compatibility(const Spell *spell, MagicElement element);

/* ============================================================
 * 元素纯度与阈值系统
 * ============================================================ */

typedef enum {
    PURITY_RAW,        /* 原始混合 < 30% */
    PURITY_COARSE,     /* 粗提纯 30-60% */
    PURITY_STANDARD,   /* 标准纯 60-85% */
    PURITY_HIGH,       /* 高纯 85-95% */
    PURITY_ULTRA,      /* 极纯 95-99% */
    PURITY_THEORETICAL /* 理论纯 > 99% */
} PurityLevel;

typedef enum {
    THRESHOLD_T1, /* 微效 1-10 E_u */
    THRESHOLD_T2, /* 弱效 10-100 E_u */
    THRESHOLD_T3, /* 中效 100-1k E_u */
    THRESHOLD_T4, /* 强效 1k-10k E_u */
    THRESHOLD_T5, /* 极效 10k-100k E_u */
    THRESHOLD_T6  /* 超限 > 100k E_u */
} EnergyThreshold;

/* 纯度转换 */
lv_PUBLIC_API double purity_to_value(PurityLevel level);
lv_PUBLIC_API PurityLevel value_to_purity(double value);

/* 阈值转换 */
lv_PUBLIC_API int threshold_to_energy(EnergyThreshold level);
lv_PUBLIC_API EnergyThreshold energy_to_threshold(int energy);

/* ============================================================
 * 咒语书系统 - 管理多个咒语
 * ============================================================ */

lv_PUBLIC_API SpellBook *spellbook_create(void);
lv_PUBLIC_API void spellbook_destroy(SpellBook *book);
lv_PUBLIC_API bool spellbook_add_spell(SpellBook *book, Spell *spell);
lv_PUBLIC_API bool spellbook_remove_spell(SpellBook *book, const char *spell_name);
lv_PUBLIC_API Spell *spellbook_get_spell(const SpellBook *book, const char *spell_name);
lv_PUBLIC_API int spellbook_get_count(const SpellBook *book);
lv_PUBLIC_API char **spellbook_list_spells(const SpellBook *book, int *count);

/* ============================================================
 * 咏唱系统
 * ============================================================ */

typedef enum {
    INCANTATION_INSTANT,  /* 瞬发 0词 */
    INCANTATION_SHORT,    /* 短咏 1-3词 */
    INCANTATION_STANDARD, /* 标准咏 4-7词 */
    INCANTATION_LONG,     /* 长咏 8-15词 */
    INCANTATION_RITUAL    /* 仪式咏 >15词 */
} IncantationLength;

typedef struct {
    IncantationLength length;
    double precision; /* 精度 0-1 */
    double speed;     /* 速度 0-1 */
    double stealth;   /* 隐蔽性 0-1 */
} IncantationProfile;

lv_PUBLIC_API IncantationProfile incantation_optimize(const char *goal, double target_value);
lv_PUBLIC_API double incantation_calculate_power(const IncantationProfile *profile);

/* ============================================================
 * 禁术与安全管理
 * ============================================================ */

typedef enum {
    RESTRICTION_NONE,       /* 无限制 */
    RESTRICTION_LIMITED,    /* 限制级 */
    RESTRICTION_CONTROLLED, /* 管制级 */
    RESTRICTION_FORBIDDEN,  /* 禁术级 */
    RESTRICTION_ABSOLUTE    /* 绝对禁术 */
} RestrictionLevel;

typedef struct {
    bool external_cost_unacceptable; /* 外部代价不可承受 */
    bool self_damage_too_high;       /* 施术者自损过高 */
    bool governance_uncontrollable;  /* 治理不可控 */
} ForbiddenSpellCriteria;

lv_PUBLIC_API RestrictionLevel spell_check_restriction(const Spell *spell, const ForbiddenSpellCriteria *criteria);
lv_PUBLIC_API const char *restriction_to_string(RestrictionLevel level);

/* ============================================================
 * 领域系统 (基于轰界领域概念)
 * ============================================================ */

typedef struct Domain Domain;

lv_PUBLIC_API Domain *domain_create(const char *name, int range);
lv_PUBLIC_API void domain_destroy(Domain *domain);
lv_PUBLIC_API bool domain_add_rule(Domain *domain, const char *rule_name, double priority);
lv_PUBLIC_API bool domain_activate(Domain *domain, SymbolicCoord *center);
lv_PUBLIC_API bool domain_deactivate(Domain *domain);
lv_PUBLIC_API bool domain_is_active(const Domain *domain);
lv_PUBLIC_API double domain_get_strength(const Domain *domain);

/* ============================================================
 * 辅助工具
 * ============================================================ */

lv_PUBLIC_API const char *element_to_string(MagicElement element);
lv_PUBLIC_API MagicElement string_to_element(const char *str);
lv_PUBLIC_API const char *stage_to_string(SpellStage stage);
lv_PUBLIC_API const char *status_to_string(SpellStatus status);
lv_PUBLIC_API const char *reaction_to_string(ElementReaction reaction);

#ifdef __cplusplus
}
#endif

#endif /* lv_MAGIC_H */
