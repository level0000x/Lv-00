/**
 * @file magic_spell.c
 * @brief 咒语系统/纯度阈值/咒语书/咏唱/禁术实现
 *
 * @details 从 magic.c 拆分的子模块（Lv-00 项目 v3.3.0+）。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "magic_internal.h"
#include "magic.h"
#include "lv/lv_lifecycle.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_json.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/lv_strbuf.h"

/* ============================================================
 * 咒语系统实现
 * ============================================================ */

/** 咒语结构体：包含咒语的所有属性和阶段配置 */
struct Spell {
    char *name;        /* 咒语名称 */
    char *description; /* 咒语描述 */
    int difficulty;    /* 难度等级（1-10） */
    int input_count;   /* 输入参数数量 */
    int output_count;  /* 输出参数数量 */

    RuneSequence *molding;              /* 开模阶段符文序列 */
    MagicElement purifying_element;     /* 提纯阶段元素 */
    double purifying_purity;            /* 提纯纯度要求（0.0 ~ 1.0） */
    EnergyThreshold infusing_threshold; /* 灌注阶段能量阈值 */
    int releasing_range;                /* 释放阶段作用范围 */
    int releasing_damage;               /* 释放阶段伤害值 */

    SpellStage current_stage; /* 当前施法阶段 */
    SpellStatus status;       /* 咒语状态 */
};

/**
 * @brief 创建咒语
 *
 * 使用给定名称创建一个新咒语，初始化默认参数：
 * - 难度：1，输入：0，输出：1
 * - 提纯元素：火，纯度：0.8
 * - 灌注阈值：T2，释放范围/伤害：10
 * - 初始阶段：开模，初始状态：空闲
 *
 * @param name 咒语名称，为 NULL 时使用 "Unnamed Spell"
 * @return 新创建的咒语指针，失败返回 NULL
 */
Spell *spell_create(const char *name) {
    Spell *spell = (Spell *) lv_calloc(1, sizeof(Spell));
    if (!spell)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "spell_create: spell calloc failed");

    /* 设置咒语名称（默认"Unnamed Spell"） */
    if (name) {
        spell->name = lv_strdup_safe(name);
    } else {
        spell->name = lv_strdup_safe("Unnamed Spell");
    }

    /* 检查名称分配是否成功 */
    if (!spell->name) {
        lv_free((void **) &spell);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "spell_create: name strdup failed");
    }

    /* 初始化描述为空字符串 */
    spell->description = lv_strdup_safe("");
    if (!spell->description) {
        lv_free((void **) &spell->name);
        lv_free((void **) &spell);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "spell_create: description strdup failed");
    }
    /* 设置咒语默认参数：难度1、输出1、开模阶段、空闲状态 */
    spell->difficulty = MAGIC_SPELL_DIFFICULTY_DEFAULT;
    spell->input_count = 0;
    spell->output_count = MAGIC_SPELL_OUTPUT_DEFAULT;
    spell->current_stage = SPELL_STAGE_MOLDING;
    spell->status = SPELL_STATUS_IDLE;

    /* 初始化各阶段默认配置 */
    spell->molding = rune_sequence_create();
    spell->purifying_element = ELEMENT_FIRE;
    spell->purifying_purity = MAGIC_SPELL_PURITY_DEFAULT;
    spell->infusing_threshold = THRESHOLD_T2;
    spell->releasing_range = MAGIC_SPELL_RANGE_DEFAULT;
    spell->releasing_damage = MAGIC_SPELL_DAMAGE_DEFAULT;

    return spell;
}

/**
 * @brief 销毁咒语并释放所有关联资源
 *
 * 释放咒语的名称、描述、开模符文序列以及咒语结构体本身。
 * 释放后指针会被自动置为 NULL。可安全传入 NULL。
 *
 * @param spell 待销毁的咒语指针
 */
/* ── spell_destroy 子资源销毁适配 ── */

static void destroy_spell_molding(void *obj) {
    rune_sequence_destroy((RuneSequence *) obj);
}

/* spell_destroy 字段描述表：name/description 纯指针，molding 对象销毁 */
static const lvFieldDesc s_spell_destroy_fields[] = {
    lv_FIELD_PLAIN(Spell, name),
    lv_FIELD_PLAIN(Spell, description),
    lv_FIELD_OBJECT(Spell, molding, destroy_spell_molding),
};

void spell_destroy(Spell *spell) {
    if (!spell)
        return;
    lv_obj_destroy_fields(spell, s_spell_destroy_fields,
                          sizeof(s_spell_destroy_fields) / sizeof(s_spell_destroy_fields[0]));
    lv_free((void **) &spell);
}

/**
 * @brief 设置咒语的输入参数数量
 *
 * @param spell 咒语指针
 * @param count 输入参数数量
 * @return 设置成功返回 true，spell 为 NULL 返回 false
 */
bool spell_set_input_count(Spell *spell, int count) {
    if (!spell)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "spell_set_input_count: spell is NULL");
    spell->input_count = count;
    return true;
}

/**
 * @brief 设置咒语的输出参数数量
 *
 * @param spell 咒语指针
 * @param count 输出参数数量
 * @return 设置成功返回 true，spell 为 NULL 返回 false
 */
bool spell_set_output_count(Spell *spell, int count) {
    if (!spell)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "spell_set_output_count: spell is NULL");
    if (count < 0)
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_PARAM, "spell_set_output_count: count < 0");
    spell->output_count = count;
    return true;
}

/**
 * @brief 设置咒语的描述文本
 *
 * 释放旧的描述字符串并创建新的副本。
 *
 * @param spell 咒语指针
 * @param desc  新的描述文本
 * @return 设置成功返回 true，参数无效返回 false
 */
bool spell_set_description(Spell *spell, const char *desc) {
    if (!spell || !desc)
        return false;
    lv_free((void **) &spell->description);
    spell->description = lv_strdup_safe(desc);
    return true;
}

/**
 * @brief 设置咒语的难度等级
 *
 * 难度等级会被限制在 [1, 10] 范围内。
 *
 * @param spell      咒语指针
 * @param difficulty 目标难度等级（超出范围会被截断）
 * @return 设置成功返回 true，spell 为 NULL 返回 false
 */
bool spell_set_difficulty(Spell *spell, int difficulty) {
    if (!spell)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "spell_set_difficulty: spell is NULL");
    spell->difficulty = difficulty > MAGIC_SPELL_DIFFICULTY_MAX
                            ? MAGIC_SPELL_DIFFICULTY_MAX
                            : (difficulty < MAGIC_SPELL_DIFFICULTY_MIN ? MAGIC_SPELL_DIFFICULTY_MIN : difficulty);
    return true;
}

/**
 * @brief 配置咒语的开模阶段符文序列
 *
 * 深拷贝给定的符文序列作为咒语开模阶段的符文配置。
 * 如果咒语已有开模配置，会先销毁旧的。
 *
 * @param spell 咒语指针
 * @param seq   符文序列模板
 * @return 配置成功返回 true，参数无效或内存不足返回 false
 */
bool spell_configure_molding(Spell *spell, const RuneSequence *seq) {
    if (!spell)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "spell_configure_molding: spell is NULL");
    if (!seq)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "spell_configure_molding: seq is NULL");

    /* 替换旧的符文序列 */
    if (spell->molding) {
        rune_sequence_destroy(spell->molding);
    }

    /* 创建新的符文序列 */
    spell->molding = rune_sequence_create();
    if (!spell->molding)
        return false;

    /* 深拷贝每个符文到咒语的开模序列 */
    for (int i = 0; i < seq->rune_count; i++) {
        Rune *copy = rune_copy(seq->runes[i]);
        if (!copy || !rune_sequence_add(spell->molding, copy)) {
            if (copy)
                rune_destroy(copy);
            rune_sequence_destroy(spell->molding);
            spell->molding = NULL;
            return false;
        }
    }

    return true;
}

/**
 * @brief 配置咒语的提纯阶段参数
 *
 * @param spell   咒语指针
 * @param element 提纯所需的魔法元素
 * @param purity  纯度要求（0.0 ~ 1.0，超出范围会被截断）
 * @return 配置成功返回 true，spell 为 NULL 返回 false
 */
bool spell_configure_purifying(Spell *spell, MagicElement element, double purity) {
    if (!spell)
        return false;
    spell->purifying_element = element;
    spell->purifying_purity = purity > MAGIC_SPELL_PURITY_MAX
                                  ? MAGIC_SPELL_PURITY_MAX
                                  : (purity < MAGIC_SPELL_PURITY_MIN ? MAGIC_SPELL_PURITY_MIN : purity);
    return true;
}

/**
 * @brief 配置咒语的灌注阶段能量阈值
 *
 * @param spell           咒语指针
 * @param threshold_level 阈值等级（1-6，对应 THRESHOLD_T1 ~ THRESHOLD_T6）
 * @return 配置成功返回 true，spell 为 NULL 返回 false
 */
bool spell_configure_infusing(Spell *spell, int threshold_level) {
    if (!spell)
        return false;
    spell->infusing_threshold = (threshold_level > 0 && threshold_level <= MAGIC_SPELL_THRESHOLD_COUNT)
                                    ? (EnergyThreshold) (threshold_level - 1)
                                    : THRESHOLD_T2;
    return true;
}

/**
 * @brief 配置咒语的释放阶段参数
 *
 * @param spell  咒语指针
 * @param range  释放作用范围
 * @param damage 释放伤害值
 * @return 配置成功返回 true，spell 为 NULL 返回 false
 */
bool spell_configure_releasing(Spell *spell, int range, int damage) {
    if (!spell)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "spell_configure_releasing: spell is NULL");
    spell->releasing_range = range;
    spell->releasing_damage = damage;
    return true;
}

/**
 * @brief 获取咒语名称
 *
 * @param spell 咒语指针
 * @return 咒语名称字符串，spell 为 NULL 时返回 NULL
 */
const char *spell_get_name(const Spell *spell) {
    return spell ? spell->name : NULL;
}

/**
 * @brief 获取咒语描述
 *
 * @param spell 咒语指针
 * @return 咒语描述字符串，spell 为 NULL 时返回 NULL
 */
const char *spell_get_description(const Spell *spell) {
    return spell ? spell->description : NULL;
}

/**
 * @brief 获取咒语难度等级
 *
 * @param spell 咒语指针
 * @return 难度等级（1-10），spell 为 NULL 时返回 0
 */
int spell_get_difficulty(const Spell *spell) {
    return spell ? spell->difficulty : 0;
}

/**
 * @brief 获取咒语输入参数数量
 *
 * @param spell 咒语指针
 * @return 输入参数数量，spell 为 NULL 时返回 0
 */
int spell_get_input_count(const Spell *spell) {
    return spell ? spell->input_count : 0;
}

/**
 * @brief 获取咒语输出参数数量
 *
 * @param spell 咒语指针
 * @return 输出参数数量，spell 为 NULL 时返回 0
 */
int spell_get_output_count(const Spell *spell) {
    return spell ? spell->output_count : 0;
}

/**
 * @brief 获取咒语当前施法阶段
 *
 * @param spell 咒语指针
 * @return 当前阶段，spell 为 NULL 时返回 SPELL_STAGE_MOLDING
 */
SpellStage spell_get_current_stage(const Spell *spell) {
    return spell ? spell->current_stage : SPELL_STAGE_MOLDING;
}

/**
 * @brief 获取咒语当前状态
 *
 * @param spell 咒语指针
 * @return 当前状态，spell 为 NULL 时返回 SPELL_STATUS_IDLE
 */
SpellStatus spell_get_status(const Spell *spell) {
    return spell ? spell->status : SPELL_STATUS_IDLE;
}

/**
 * @brief 施放咒语
 *
 * 按照开模 -> 提纯 -> 灌注 -> 释放四个阶段依次执行咒语。
 * 每个阶段有独立的检查逻辑：
 * - 开模：检查符文序列是否非空
 * - 提纯：检查魔法阵中是否包含所需元素
 * - 灌注：检查魔法阵稳定性（低于 0.3 会触发反噬）
 * - 释放：生成输出结果
 *
 * @param spell       咒语指针
 * @param array       魔法阵指针
 * @param inputs      输入符号坐标数组
 * @param input_count 输入数量
 * @param outputs     输出符号坐标数组（调用者提供缓冲区）
 * @param output_count 输出数量
 * @return 咒语执行状态（成功、失败或反噬）
 */
SpellStatus spell_cast(Spell *spell, MagicArray *array, SymbolicCoord **inputs, int input_count,
                       SymbolicCoord **outputs, int output_count) {
    if (!spell || !array)
        return SPELL_STATUS_FAILED;

    /* 初始化施法状态 */
    spell->current_stage = SPELL_STAGE_MOLDING;
    spell->status = SPELL_STATUS_CASTING;

    /* 开模阶段：检查开模符文序列是否为空 */
    if (spell->molding->rune_count == 0) {
        spell->status = SPELL_STATUS_FAILED;
        return spell->status;
    }

    spell->current_stage = SPELL_STAGE_PURIFYING;

    /* 提纯阶段：检查魔法阵中是否存在所需的魔法元素 */
    bool has_matching_element = false;
    for (int i = 0; i < array->runes->rune_count; i++) {
        if (array->runes->runes[i]->element == spell->purifying_element) {
            has_matching_element = true;
            break;
        }
    }

    /* 需要提纯但缺少对应元素时，施法失败 */
    if (!has_matching_element && spell->purifying_purity > MAGIC_SPELL_PURITY_CHECK_THRESH) {
        spell->status = SPELL_STATUS_FAILED;
        return spell->status;
    }

    spell->current_stage = SPELL_STAGE_INFUSING;

    /* 灌注阶段：检查魔法阵稳定性，低于阈值则触发反噬 */
    double stability = array_calculate_stability(array);
    if (stability < MAGIC_STABILITY_BACKLASH_THRESHOLD) {
        spell->status = SPELL_STATUS_BACKLASH;
        return spell->status;
    }

    spell->current_stage = SPELL_STAGE_RELEASING;

    /* 释放阶段 - 生成输出
     * 基础算术咒语：根据输入坐标数量产生不同结果
     * - 0 个输入：返回有理数 0/1
     * - 1 个输入：直接复制返回该输入坐标
     * - 2+ 个输入：返回所有输入坐标之和 */
    if (outputs && output_count > 0) {
        if (input_count == 0 || !inputs) {
            outputs[0] = symbolic_coord_create_rational(0, 1);
        } else if (input_count == 1) {
            outputs[0] = symbolic_coord_copy(inputs[0]);
        } else {
            /* 累加所有输入坐标 */
            SymbolicCoord *sum = symbolic_coord_copy(inputs[0]);
            for (int i = 1; i < input_count; i++) {
                SymbolicCoord *tmp = symbolic_coord_add(sum, inputs[i]);
                symbolic_coord_destroy(sum);
                sum = tmp;
            }
            outputs[0] = sum;
        }
    }

    spell->status = SPELL_STATUS_SUCCESS;
    return spell->status;
}

/**
 * @brief 验证咒语结构的合法性
 *
 * 检查咒语的关键参数是否在有效范围内：
 * - 难度必须在 [1, 10] 范围内
 * - 开模符文序列不能为空
 * - 提纯纯度必须在 [0.0, 1.0] 范围内
 *
 * @param spell 咒语指针
 * @return 结构合法返回 true，参数无效或不合法返回 false
 */
bool spell_validate_structure(const Spell *spell) {
    if (!spell)
        return false;
    /* 检查难度是否在有效范围 [1, 10] 内 */
    if (spell->difficulty < MAGIC_SPELL_DIFFICULTY_MIN || spell->difficulty > MAGIC_SPELL_DIFFICULTY_MAX)
        return false;
    /* 检查开模符文序列是否非空 */
    if (spell->molding->rune_count == 0)
        return false;
    /* 检查提纯纯度是否在 [0.0, 1.0] 范围内 */
    if (spell->purifying_purity < MAGIC_SPELL_PURITY_MIN || spell->purifying_purity > MAGIC_SPELL_PURITY_MAX)
        return false;
    return true;
}

/**
 * @brief 检查咒语与指定元素的兼容性
 *
 * 通过查询元素反应矩阵，判断咒语的提纯元素与给定元素是否冲突。
 *
 * @param spell   咒语指针
 * @param element 待检查的魔法元素
 * @return 兼容返回 true（非冲突），不兼容或 spell 为 NULL 返回 false
 */
bool spell_check_element_compatibility(const Spell *spell, MagicElement element) {
    if (!spell)
        return false;

    ElementReaction reaction = array_check_element_reaction(spell->purifying_element, element);

    return reaction != ELEMENT_REACTION_CONFLICT;
}

/* ============================================================
 * 纯度与阈值转换
 * ============================================================ */

/**
 * @brief 将纯度等级转换为数值
 *
 * @param level 纯度等级
 * @return 对应的纯度数值，无效等级时返回 0.0
 * @warning 传入无效枚举值将触发边界检查并返回 0.0
 */
double purity_to_value(PurityLevel level) {
    static const double values[] = {MAGIC_PURITY_RAW_VALUE,  MAGIC_PURITY_COARSE_VALUE, MAGIC_PURITY_STANDARD_VALUE,
                                    MAGIC_PURITY_HIGH_VALUE, MAGIC_PURITY_ULTRA_VALUE,  MAGIC_PURITY_THEORETICAL_VALUE};
    /* 边界检查：防止数组越界 */
    if (level < 0 || level > PURITY_THEORETICAL) {
        return 0.0;
    }
    return values[level];
}

/**
 * @brief 将数值转换为纯度等级
 *
 * 根据数值所在区间映射到最近的纯度等级。
 *
 * @param value 纯度数值
 * @return 对应的纯度等级
 */
PurityLevel value_to_purity(double value) {
    if (value < MAGIC_PURITY_THRESH_COARSE)
        return PURITY_RAW;
    if (value < MAGIC_PURITY_THRESH_STANDARD)
        return PURITY_COARSE;
    if (value < MAGIC_PURITY_THRESH_HIGH)
        return PURITY_STANDARD;
    if (value < MAGIC_PURITY_THRESH_ULTRA)
        return PURITY_HIGH;
    if (value < MAGIC_PURITY_THRESH_THEORETICAL)
        return PURITY_ULTRA;
    return PURITY_THEORETICAL;
}

/**
 * @brief 将能量阈值等级转换为能量值
 *
 * @param level 能量阈值等级
 * @return 对应的能量值，无效等级时返回 0
 * @warning 传入无效枚举值将触发边界检查并返回 0
 */
int threshold_to_energy(EnergyThreshold level) {
    static const int energies[] = {MAGIC_ENERGY_T1, MAGIC_ENERGY_T2, MAGIC_ENERGY_T3,
                                   MAGIC_ENERGY_T4, MAGIC_ENERGY_T5, MAGIC_ENERGY_T6};
    /* 边界检查：防止数组越界 */
    if (level < 0 || level > THRESHOLD_T6) {
        return 0;
    }
    return energies[level];
}

/**
 * @brief 将能量值转换为能量阈值等级
 *
 * 根据能量值所在区间映射到最近的阈值等级。
 *
 * @param energy 能量值
 * @return 对应的能量阈值等级
 */
EnergyThreshold energy_to_threshold(int energy) {
    if (energy < MAGIC_ENERGY_T2)
        return THRESHOLD_T1;
    if (energy < MAGIC_ENERGY_T3)
        return THRESHOLD_T2;
    if (energy < MAGIC_ENERGY_T4)
        return THRESHOLD_T3;
    if (energy < MAGIC_ENERGY_T5)
        return THRESHOLD_T4;
    if (energy < MAGIC_ENERGY_T6)
        return THRESHOLD_T5;
    return THRESHOLD_T6;
}

/* ============================================================
 * 咒语书系统
 * ============================================================ */

/** 咒语书结构体：管理多个咒语的集合 */
struct SpellBook {
    lvDArray spells; /* 咒语指针数组，元素为 Spell*（析构回调逐元素 spell_destroy） */
};

/** 咒语书元素析构回调：lv_darray_free 时逐个销毁咒语 */
static void spellbook_elem_destroy(void *elem) {
    spell_destroy(*(Spell **) elem);
}

/**
 * @brief 创建空的咒语书
 *
 * 创建一个初始容量为 64 的咒语集合。
 * 调用者负责通过 spellbook_destroy 释放。
 *
 * @return 新创建的咒语书指针，失败返回 NULL
 */
SpellBook *spellbook_create(void) {
    SpellBook *book = (SpellBook *) lv_calloc(1, sizeof(SpellBook));
    if (!book)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "spellbook_create: book calloc failed");

    /* 初始化带元素析构的 lvDArray，预分配初始容量 */
    lv_darray_init_with_dtor(&book->spells, sizeof(Spell *), spellbook_elem_destroy);
    if (!lv_darray_reserve(&book->spells, MAGIC_SPELLBOOK_INIT_CAP)) {
        lv_free((void **) &book);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "spellbook_create: spells reserve failed");
    }

    return book;
}

/**
 * @brief 销毁咒语书及其包含的所有咒语
 *
 * 依次销毁书中的每个咒语，然后释放咒语数组和咒语书结构体本身。
 * 释放后指针会被自动置为 NULL。可安全传入 NULL。
 *
 * @param book 待销毁的咒语书指针
 */
void spellbook_destroy(SpellBook *book) {
    if (!book)
        return;

    /* 逐元素调用 spell_destroy（析构回调）后释放动态数组 */
    lv_darray_free(&book->spells);
    lv_free((void **) &book);
}

/**
 * @brief 向咒语书中添加咒语
 *
 * 如果当前容量不足，会自动扩容（容量翻倍）。
 * 咒语书将接管咒语的所有权。
 *
 * @param book  咒语书指针
 * @param spell 待添加的咒语指针
 * @return 添加成功返回 true，参数无效或内存不足返回 false
 */
bool spellbook_add_spell(SpellBook *book, Spell *spell) {
    if (!book)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "spellbook_add_spell: book is NULL");
    if (!spell)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "spellbook_add_spell: spell is NULL");

    /* 追加咒语（自动扩容；咒语书获得所有权） */
    return lv_darray_push(&book->spells, &spell) >= 0;
}

/**
 * @brief 从咒语书中按名称移除咒语
 *
 * 查找并销毁指定名称的咒语，后续咒语索引会前移。
 *
 * @param book       咒语书指针
 * @param spell_name 要移除的咒语名称
 * @return 移除成功返回 true，未找到或参数无效返回 false
 */
bool spellbook_remove_spell(SpellBook *book, const char *spell_name) {
    if (!book || !spell_name)
        return false;

    /* 按名称查找并移除咒语 */
    for (int i = 0; i < book->spells.count; i++) {
        Spell *spell = *(Spell **) lv_darray_get(&book->spells, i);
        if (spell && strcmp(spell->name, spell_name) == 0) {
            spell_destroy(spell);
            /* 后续元素前移填补空缺（统一走 lv_shift_left 的 memmove 路径） */
            lv_shift_left(book->spells.data, sizeof(Spell *), (size_t) i, (size_t) book->spells.count);
            book->spells.count--;
            return true;
        }
    }

    return false;
}

/**
 * @brief 从咒语书中按名称查找咒语
 *
 * @param book       咒语书指针
 * @param spell_name 要查找的咒语名称
 * @return 找到的咒语指针（所有权仍归咒语书），未找到返回 NULL
 */
Spell *spellbook_get_spell(const SpellBook *book, const char *spell_name) {
    if (!book || !spell_name)
        return NULL;

    for (int i = 0; i < book->spells.count; i++) {
        Spell *spell = *(Spell **) lv_darray_get(&book->spells, i);
        if (spell && strcmp(spell->name, spell_name) == 0) {
            return spell;
        }
    }

    return NULL;
}

/**
 * @brief 获取咒语书中的咒语数量
 *
 * @param book 咒语书指针
 * @return 咒语数量，book 为 NULL 时返回 0
 */
int spellbook_get_count(const SpellBook *book) {
    return book ? book->spells.count : 0;
}

/**
 * @brief 列出咒语书中所有咒语的名称
 *
 * 返回一个新分配的字符串数组，包含所有咒语名称的副本。
 * 调用者负责释放返回的数组及其中每个字符串。
 *
 * @param book  咒语书指针
 * @param count [out] 输出咒语数量
 * @return 咒语名称字符串数组，失败返回 NULL
 */
char **spellbook_list_spells(const SpellBook *book, int *count) {
    if (!book || !count)
        return NULL;

    *count = book->spells.count;
    /* 分配咒语名称指针数组 */
    char **names = (char **) lv_malloc(book->spells.count * sizeof(char *));

    if (!names) {
        *count = 0;
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "spellbook_list_spells: names malloc failed");
    }

    /* 逐个复制咒语名称到数组 */
    for (int i = 0; i < book->spells.count; i++) {
        Spell *spell = *(Spell **) lv_darray_get(&book->spells, i);
        names[i] = lv_strdup_safe(spell->name);
        /* 如果某个名称复制失败，释放已分配的内存并返回 NULL */
        if (!names[i]) {
            for (int j = 0; j < i; j++) {
                lv_free((void **) &names[j]);
            }
            lv_free((void **) &names);
            *count = 0;
            lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "spellbook_list_spells: strdup failed");
        }
    }

    return names;
}

/* ============================================================
 * 咏唱系统
 * ============================================================ */

/**
 * @brief 根据目标优化咏唱配置
 *
 * 根据施法目标（速度、精度、隐蔽）调整咏唱参数：
 * - "speed"：短咏唱，高速度和高隐蔽，低精度
 * - "precision"：长咏唱，高精度，低速度和隐蔽
 * - "stealth"：短咏唱，高隐蔽，中等速度和精度
 * - 其他：标准配置
 *
 * @param goal          施法目标字符串
 * @param target_value  目标数值（保留参数，当前未使用）
 * @return 优化后的咏唱配置
 */
IncantationProfile incantation_optimize(const char *goal, double target_value) {
    lv_UNUSED(target_value);
    /* 设置默认咏唱配置：标准长度 */
    IncantationProfile profile = {INCANTATION_STANDARD, MAGIC_INCANTATION_PRECISION_DEFAULT,
                                  MAGIC_INCANTATION_SPEED_DEFAULT, MAGIC_INCANTATION_STEALTH_DEFAULT};

    if (!goal)
        return profile;

    /* 根据施法目标调整咏唱参数 */
    if (strcmp(goal, "speed") == 0) {
        /* 速度优先：短咏唱，高速度高隐蔽，低精度 */
        profile.length = INCANTATION_SHORT;
        profile.speed = MAGIC_INCANTATION_SPEED_FAST;
        profile.precision = MAGIC_INCANTATION_PRECISION_LOW;
        profile.stealth = MAGIC_INCANTATION_STEALTH_HIGH;
    } else if (strcmp(goal, "precision") == 0) {
        /* 精度优先：长咏唱，高精度，低速度和隐蔽 */
        profile.length = INCANTATION_LONG;
        profile.speed = MAGIC_INCANTATION_SPEED_SLOW;
        profile.precision = MAGIC_INCANTATION_PRECISION_HIGH;
        profile.stealth = MAGIC_INCANTATION_STEALTH_LOW;
    } else if (strcmp(goal, "stealth") == 0) {
        /* 隐蔽优先：短咏唱，高隐蔽，中等速度和精度 */
        profile.length = INCANTATION_SHORT;
        profile.speed = MAGIC_INCANTATION_SPEED_MED;
        profile.precision = MAGIC_INCANTATION_PRECISION_MED;
        profile.stealth = MAGIC_INCANTATION_STEALTH_MAX;
    }

    return profile;
}

/**
 * @brief 计算咏唱配置的综合威力值
 *
 * 威力由精度（40%）、速度（30%）和隐蔽（30%）加权计算，
 * 再乘以咏唱长度的系数（瞬发 0.5x ~ 仪式 1.5x）。
 *
 * @param profile 咏唱配置指针
 * @return 综合威力值，profile 为 NULL 时返回 0.0
 */
double incantation_calculate_power(const IncantationProfile *profile) {
    if (!profile)
        return 0.0;

    /* 加权计算基础威力：精度40% + 速度30% + 隐蔽30% */
    double power = profile->precision * MAGIC_INCANTATION_WEIGHT_PRECISION +
                   profile->speed * MAGIC_INCANTATION_WEIGHT_SPEED +
                   profile->stealth * MAGIC_INCANTATION_WEIGHT_STEALTH;

    /* 根据咏唱长度施加倍率（瞬发0.5x ~ 仪式1.5x） */
    static const double s_power_multipliers[] = {
        MAGIC_INCANTATION_MULT_INSTANT,   /* INCANTATION_INSTANT = 0 */
        MAGIC_INCANTATION_MULT_SHORT,     /* INCANTATION_SHORT = 1 */
        MAGIC_INCANTATION_MULT_STANDARD,  /* INCANTATION_STANDARD = 2 */
        MAGIC_INCANTATION_MULT_LONG,      /* INCANTATION_LONG = 3 */
        MAGIC_INCANTATION_MULT_RITUAL,    /* INCANTATION_RITUAL = 4 */
    };
    if ((int)profile->length >= 0 && (int)profile->length < (int)(sizeof(s_power_multipliers) / sizeof(s_power_multipliers[0])))
        power *= s_power_multipliers[profile->length];

    return power;
}

/* ============================================================
 * 禁术判定
 * ============================================================ */

/**
 * @brief 检查咒语的禁术等级
 *
 * 根据禁术判定标准评估咒语的限制级别：
 * - 3 项标准全部满足：绝对禁术
 * - 2 项标准满足：禁术级
 * - 1 项标准满足：管制级
 * - 难度 > 8：限制级
 * - 其他：无限制
 *
 * @param spell   咒语指针
 * @param criteria 禁术判定标准
 * @return 限制等级
 */
RestrictionLevel spell_check_restriction(const Spell *spell, const ForbiddenSpellCriteria *criteria) {
    if (!spell || !criteria)
        return RESTRICTION_NONE;

    /* 统计满足的禁术判定标准数量 */
    int criteria_count = 0;
    if (criteria->external_cost_unacceptable)
        criteria_count++;
    if (criteria->self_damage_too_high)
        criteria_count++;
    if (criteria->governance_uncontrollable)
        criteria_count++;

    /* 根据满足的标准数量确定限制等级 */
    if (criteria_count >= MAGIC_RESTRICTION_CRITERIA_ABSOLUTE)
        return RESTRICTION_ABSOLUTE;
    if (criteria_count == MAGIC_RESTRICTION_CRITERIA_FORBID)
        return RESTRICTION_FORBIDDEN;
    if (criteria_count == MAGIC_RESTRICTION_CRITERIA_CONTROL)
        return RESTRICTION_CONTROLLED;
    /* 难度超过阈值也视为限制级 */
    if (spell->difficulty > MAGIC_SPELL_RESTRICTION_DIFF)
        return RESTRICTION_LIMITED;

    return RESTRICTION_NONE;
}

