/**
 * @file magic_internal.h
 * @brief 编程魔法系统内部共享常量（从 magic.c 拆分）
 *
 * @details 由 magic.c 各拆分模块共享的模块级常量定义。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#ifndef lv_MAGIC_INTERNAL_H
#define lv_MAGIC_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 模块级常量定义
 * ============================================================ */

/* ---- 符文系统常量 ---- */
#define MAGIC_RUNE_POWER_MIN 1          /* 符文最低威力等级 */
#define MAGIC_RUNE_POWER_MAX 10         /* 符文最高威力等级 */
#define MAGIC_RUNE_SEQUENCE_INIT_CAP 16 /* 符文序列初始容量 */
#define MAGIC_RUNE_SEQUENCE_GROWTH 2    /* 符文序列扩容倍数 */

/* ---- 多项式常量 ---- */
#define MAGIC_POLY_DEGREE_QUADRATIC 2  /* 二次多项式次数 */
#define MAGIC_POLY_COEFF_COUNT 3       /* 二次多项式系数个数 */
#define MAGIC_POLY_APPROX_A 1000000.0  /* 连分数近似二次项系数 */
#define MAGIC_POLY_APPROX_B 1000       /* 连分数近似线性项系数 */
#define MAGIC_POLY_APPROX_C 1          /* 连分数近似常数项系数 */
#define MAGIC_POLY_ROOT_TOLERANCE 0.01 /* 求根容忍区间半宽 */

/* ---- 元素系统常量 ---- */
#define MAGIC_ELEMENT_TOTAL_COUNT 6         /* 元素总数（含NONE） */
#define MAGIC_REAL_ELEMENT_COUNT 5          /* 实际有效元素数（不含NONE） */
#define MAGIC_ELEMENT_BALANCE_THRESHOLD 2.0 /* 平衡性判定因子 */

/* ---- 魔法阵系统常量 ---- */
#define MAGIC_ARRAY_CONSTRAINT_INIT_CAP 32 /* 约束数组初始容量 */
#define MAGIC_ARRAY_CONSTRAINT_GROWTH 2    /* 约束数组扩容倍数 */

/* ---- 稳定性计算常量 ---- */
#define MAGIC_STABILITY_CONFLICT_PENALTY 0.1   /* 每个冲突约束的稳定性惩罚 */
#define MAGIC_STABILITY_MIN_RUNES 3            /* 最低符文数量要求 */
#define MAGIC_STABILITY_TOO_FEW_MULTIPLIER 0.5 /* 符文太少时的稳定性倍率 */
#define MAGIC_STABILITY_BACKLASH_THRESHOLD 0.3 /* 灌注阶段反噬判定阈值 */

/* ---- 咒语系统常量 ---- */
#define MAGIC_SPELL_DIFFICULTY_MIN 1        /* 咒语最低难度 */
#define MAGIC_SPELL_DIFFICULTY_MAX 10       /* 咒语最高难度 */
#define MAGIC_SPELL_DIFFICULTY_DEFAULT 1    /* 咒语默认难度 */
#define MAGIC_SPELL_OUTPUT_DEFAULT 1        /* 咒语默认输出数 */
#define MAGIC_SPELL_RANGE_DEFAULT 10        /* 咒语默认释放范围 */
#define MAGIC_SPELL_DAMAGE_DEFAULT 10       /* 咒语默认伤害值 */
#define MAGIC_SPELL_PURITY_DEFAULT 0.8      /* 咒语默认提纯纯度 */
#define MAGIC_SPELL_PURITY_MIN 0.0          /* 提纯纯度下限 */
#define MAGIC_SPELL_PURITY_MAX 1.0          /* 提纯纯度上限 */
#define MAGIC_SPELL_PURITY_CHECK_THRESH 0.5 /* 提纯阶段元素存在性检查阈值 */
#define MAGIC_SPELL_THRESHOLD_COUNT 6       /* 能量阈值等级总数 */
#define MAGIC_SPELL_RESTRICTION_DIFF 8      /* 限制级禁术难度阈值 */

/* ---- 咒语书系统常量 ---- */
#define MAGIC_SPELLBOOK_INIT_CAP 64 /* 咒语书初始容量 */
#define MAGIC_SPELLBOOK_GROWTH 2    /* 咒语书扩容倍数 */

/* ---- 纯度等级数值定义 ---- */
#define MAGIC_PURITY_RAW_VALUE 0.15
#define MAGIC_PURITY_COARSE_VALUE 0.45
#define MAGIC_PURITY_STANDARD_VALUE 0.725
#define MAGIC_PURITY_HIGH_VALUE 0.9
#define MAGIC_PURITY_ULTRA_VALUE 0.97
#define MAGIC_PURITY_THEORETICAL_VALUE 0.995

/* ---- 纯度区间判定阈值 ---- */
#define MAGIC_PURITY_THRESH_COARSE 0.3
#define MAGIC_PURITY_THRESH_STANDARD 0.6
#define MAGIC_PURITY_THRESH_HIGH 0.85
#define MAGIC_PURITY_THRESH_ULTRA 0.95
#define MAGIC_PURITY_THRESH_THEORETICAL 0.99

/* ---- 能量阈值等级数值定义 ---- */
#define MAGIC_ENERGY_T1 1
#define MAGIC_ENERGY_T2 10
#define MAGIC_ENERGY_T3 100
#define MAGIC_ENERGY_T4 1000
#define MAGIC_ENERGY_T5 10000
#define MAGIC_ENERGY_T6 100000

/* ---- 咏唱系统常量 ---- */
#define MAGIC_INCANTATION_SPEED_DEFAULT 0.8
#define MAGIC_INCANTATION_PRECISION_DEFAULT 0.8
#define MAGIC_INCANTATION_STEALTH_DEFAULT 0.5
#define MAGIC_INCANTATION_SPEED_FAST 0.95
#define MAGIC_INCANTATION_PRECISION_LOW 0.5
#define MAGIC_INCANTATION_STEALTH_HIGH 0.9
#define MAGIC_INCANTATION_SPEED_SLOW 0.4
#define MAGIC_INCANTATION_PRECISION_HIGH 0.95
#define MAGIC_INCANTATION_STEALTH_LOW 0.3
#define MAGIC_INCANTATION_SPEED_MED 0.8
#define MAGIC_INCANTATION_PRECISION_MED 0.6
#define MAGIC_INCANTATION_STEALTH_MAX 0.95
#define MAGIC_INCANTATION_WEIGHT_PRECISION 0.4
#define MAGIC_INCANTATION_WEIGHT_SPEED 0.3
#define MAGIC_INCANTATION_WEIGHT_STEALTH 0.3
#define MAGIC_INCANTATION_MULT_INSTANT 0.5
#define MAGIC_INCANTATION_MULT_SHORT 0.7
#define MAGIC_INCANTATION_MULT_STANDARD 1.0
#define MAGIC_INCANTATION_MULT_LONG 1.2
#define MAGIC_INCANTATION_MULT_RITUAL 1.5

/* ---- 禁术判定常量 ---- */
#define MAGIC_RESTRICTION_CRITERIA_ABSOLUTE 3
#define MAGIC_RESTRICTION_CRITERIA_FORBID 2
#define MAGIC_RESTRICTION_CRITERIA_CONTROL 1

/* ---- 稳定性与领域常量 ---- */
#define MAGIC_STABILITY_MAX 1.0              /* 最大稳定性（初始值） */
#define MAGIC_DOMAIN_ACTIVATION_STRENGTH 1.0 /* 领域激活时的初始强度 */

/* ---- 序列化缓冲区常量 ---- */
#define MAGIC_SERIALIZE_JSON_BASE_SIZE 256 /* JSON序列化基础结构大小 */
#define MAGIC_SERIALIZE_PER_RUNE_SIZE 128  /* 每个符文JSON序列化预估大小 */


#ifdef __cplusplus
}
#endif

#endif /* lv_MAGIC_INTERNAL_H */
