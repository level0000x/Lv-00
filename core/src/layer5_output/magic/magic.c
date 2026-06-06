/**
 * @file magic.c
 * @brief 编程魔法系统实现 —— 基于 Lv-00 的咒语编程模拟器
 *
 * 本模块将 Lv-00 的核心系统映射为魔法概念，实现：
 * - 符文系统 (基于符号坐标)
 * - 魔法阵系统 (基于约束图)
 * - 咒语系统 (基于函数块)
 *
 * @author Lv-00 Project
 * @version 3.3.0（与项目主版本保持一致）
 */

#include "magic.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00_internal.h"
#include "lv00_utils.h"

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

/* ============================================================
 * 符文系统实现
 * ============================================================ */

/**
 * @brief 创建有理数符文
 *
 * 根据给定的分子、分母和魔法元素创建一个有理数类型的符文。
 * 符文的符号坐标由 symbolic_coord_create_rational 创建，
 * 初始威力等级为 1，名称和符号为空。
 *
 * @param num     分子（有符号 64 位整数）
 * @param denom   分母（无符号 64 位整数）
 * @param element 魔法元素类型
 * @return 新创建的符文指针，失败返回 NULL
 */
Rune *rune_create_rational(int64_t num, uint64_t denom, MagicElement element) {
    Rune *rune = (Rune *) lv00_malloc(sizeof(Rune));
    if (!rune)
        return NULL;

    rune->coord = symbolic_coord_create_rational(num, denom);
    if (!rune->coord) {
        lv00_free((void **) &rune);
        return NULL;
    }

    rune->element = element;
    rune->name = NULL;
    rune->symbol = NULL;
    rune->power_level = MAGIC_RUNE_POWER_MIN;

    return rune;
}

/**
 * @brief 创建代数数符文
 *
 * 根据给定的浮点数值和魔法元素创建一个代数数类型的符文。
 * 内部使用连分数近似方法构造二次多项式，再通过
 * symbolic_coord_create_algebraic 创建符号坐标。
 *
 * @param value   代数数的近似浮点值
 * @param element 魔法元素类型
 * @return 新创建的符文指针，失败返回 NULL
 */
Rune *rune_create_algebraic(double value, MagicElement element) {
    Rune *rune = (Rune *) lv00_malloc(sizeof(Rune));
    if (!rune)
        return NULL;

    /* 使用连分数近似创建代数数 */
    mpz_poly_t poly;
    poly.degree = MAGIC_POLY_DEGREE_QUADRATIC;
    poly.coeffs = (mpz_t *) lv00_malloc(MAGIC_POLY_COEFF_COUNT * sizeof(mpz_t));
    if (!poly.coeffs) {
        lv00_free((void **) &rune);
        return NULL;
    }

    /* 必须先初始化所有 mpz_t 元素，再设置值，避免 GMP 内部状态未定义 */
    for (int i = 0; i < MAGIC_POLY_COEFF_COUNT; i++) {
        mpz_init(poly.coeffs[i]);
    }

    /* 使用 mpz_set_d / mpz_set_si 安全设置 GMP 值，避免 double 到 long 的溢出风险 */
    double computed = value * value * MAGIC_POLY_APPROX_A - value * MAGIC_POLY_APPROX_B;
    mpz_set_d(poly.coeffs[0], computed);
    mpz_set_si(poly.coeffs[1], (long) (MAGIC_POLY_APPROX_B));
    mpz_set_si(poly.coeffs[2], MAGIC_POLY_APPROX_C);

    /* 使用相对容差，避免小数值时区间不合理 */
    double tolerance = fabs(value) * MAGIC_POLY_ROOT_TOLERANCE + MAGIC_POLY_ROOT_TOLERANCE;
    rune->coord =
        symbolic_coord_create_algebraic(&poly, value - tolerance, value + tolerance);
    if (!rune->coord) {
        /* 错误路径：必须先清理 mpz_t 内部状态，再释放数组内存 */
        for (int i = 0; i < MAGIC_POLY_COEFF_COUNT; i++) {
            mpz_clear(poly.coeffs[i]);
        }
        lv00_free((void **) &poly.coeffs);
        lv00_free((void **) &rune);
        return NULL;
    }

    /* 成功路径：同样需要先清理 mpz_t 内部状态 */
    for (int i = 0; i < MAGIC_POLY_COEFF_COUNT; i++) {
        mpz_clear(poly.coeffs[i]);
    }
    lv00_free((void **) &poly.coeffs);
    rune->element = element;
    rune->name = NULL;
    rune->symbol = NULL;
    rune->power_level = MAGIC_RUNE_POWER_MIN;

    return rune;
}

/**
 * @brief 创建超越数符文
 *
 * 根据给定的名称和魔法元素创建一个超越数类型的符文。
 * 符号坐标由 symbolic_coord_create_transcendental 创建。
 *
 * @param name    超越数的标识名称（如 "pi"、"e"）
 * @param element 魔法元素类型
 * @return 新创建的符文指针，失败返回 NULL
 */
Rune *rune_create_transcendental(const char *name, MagicElement element) {
    Rune *rune = (Rune *) lv00_malloc(sizeof(Rune));
    if (!rune)
        return NULL;

    rune->coord = symbolic_coord_create_transcendental(name);
    if (!rune->coord) {
        lv00_free((void **) &rune);
        return NULL;
    }

    rune->element = element;
    rune->name = NULL;
    rune->symbol = NULL;
    rune->power_level = MAGIC_RUNE_POWER_MIN;

    return rune;
}

/**
 * @brief 深拷贝符文
 *
 * 创建源符文的完整副本，包括符号坐标、名称、符号和威力等级。
 * 调用者负责释放返回的符文（通过 rune_destroy）。
 *
 * @param src 源符文指针
 * @return 新符文指针（深拷贝），失败或 src 为 NULL 时返回 NULL
 */
Rune *rune_copy(const Rune *src) {
    if (!src)
        return NULL;

    Rune *rune = (Rune *) lv00_malloc(sizeof(Rune));
    if (!rune)
        return NULL;

    rune->coord = symbolic_coord_copy(src->coord);
    if (!rune->coord) {
        lv00_free((void **)&rune);
        return NULL;
    }
    rune->element = src->element;
    rune->power_level = src->power_level;

    if (src->name) {
        rune->name = lv00_strdup_safe(src->name);
    } else {
        rune->name = NULL;
    }

    if (src->symbol) {
        rune->symbol = lv00_strdup_safe(src->symbol);
    } else {
        rune->symbol = NULL;
    }

    return rune;
}

/**
 * @brief 销毁符文并释放所有关联资源
 *
 * 释放符文的符号坐标、名称、符号字符串以及符文结构体本身。
 * 释放后指针会被自动置为 NULL。可安全传入 NULL。
 *
 * @param rune 待销毁的符文指针
 */
void rune_destroy(Rune *rune) {
    if (!rune)
        return;

    if (rune->coord) {
        symbolic_coord_destroy(rune->coord);
    }
    if (rune->name) {
        lv00_free((void **) &rune->name);
    }
    if (rune->symbol) {
        lv00_free((void **) &rune->symbol);
    }
    lv00_free((void **) &rune);
}

/**
 * @brief 将符文序列化为 JSON 格式字符串（动态分配版本）
 *
 * 将符文的元素类型、威力等级和符号坐标序列化为紧凑的 JSON 字符串。
 * 返回的字符串由调用者负责释放（使用 lv00_free）。
 *
 * **线程安全保证：**
 * 本函数使用 lv00_asprintf 动态分配缓冲区，每次调用返回独立的堆内存，
 * 不存在静态缓冲区共享问题，可在多线程环境中安全并发调用。
 * 符号坐标的序列化也通过 symbolic_coord_serialize 返回动态分配字符串。
 *
 * @param rune 待序列化的符文指针
 * @return JSON 格式字符串指针（需调用者释放），失败或 rune 为 NULL 时返回 NULL
 */
char *rune_serialize(const Rune *rune) {
    if (!rune)
        return NULL;

    char *coord_str = symbolic_coord_serialize(rune->coord);
    if (!coord_str)
        return NULL;

    /* 使用 lv00_asprintf 动态分配缓冲区，避免静态缓冲区的线程安全问题。
     * 不使用 static char buf[N] 模式，确保并发调用时不会互相覆盖。 */
    char *result =
        lv00_asprintf("{\"element\":%d,\"power\":%d,\"coord\":%s}", rune->element, rune->power_level, coord_str);

    /* 释放 coord_str，无论 result 是否成功都需要释放 */
    lv00_free((void **) &coord_str);
    return result;
}

/**
 * @brief 将符文序列化为 JSON 格式字符串（缓冲区版本）
 *
 * 将符文的元素类型、威力等级和符号坐标序列化为紧凑的 JSON 字符串，
 * 写入调用者提供的缓冲区中。线程安全：不使用任何静态或全局缓冲区。
 *
 * @param rune     待序列化的符文指针
 * @param buf      输出缓冲区
 * @param buf_size 缓冲区大小（字节）
 * @return 实际写入的字符数（不含终止空字符），失败或 rune 为 NULL 时返回 -1
 */
int rune_serialize_to_buffer(const Rune *rune, char *buf, int buf_size) {
    if (!rune || !buf || buf_size <= 0)
        return -1;

    char *coord_str = symbolic_coord_serialize(rune->coord);
    if (!coord_str)
        return -1;

    int written = snprintf(buf, (size_t) buf_size, "{\"element\":%d,\"power\":%d,\"coord\":%s}", rune->element,
                           rune->power_level, coord_str);

    lv00_free((void **) &coord_str);

    if (written < 0)
        return -1;
    /* 返回实际写入的字符数（截断时返回 buf_size - 1） */
    return (written >= buf_size) ? (buf_size - 1) : written;
}

/**
 * @brief 从字符串解析符文
 *
 * 支持以下格式：
 *   - "rational:num/denom:element"  例如 "rational:1/2:FIRE"
 *   - "rational:num:element"        例如 "rational:3:FIRE" (分母默认为1)
 *   - "algebraic:value:element"     例如 "algebraic:1.414:EARTH"
 *   - "num/denom:element"           简写格式
 *   - "num:element"                 简写格式（整数）
 *
 * @param str 符文的字符串表示
 * @return 解析成功返回新创建的符文，失败返回 NULL
 */
Rune *rune_parse(const char *str) {
    if (!str || str[0] == '\0') {
        LV00_LOG_WARNING("rune_parse: 输入字符串为空");
        return NULL;
    }

    /* 跳过前导空白 */
    while (*str == ' ' || *str == '\t')
        str++;

    /* 解析元素类型（默认为 NONE） */
    MagicElement element = ELEMENT_NONE;
    const char *colon = strrchr(str, ':');
    if (colon) {
        /* 解析元素类型 */
        const char *elem_str = colon + 1;
        if (strcmp(elem_str, "FIRE") == 0 || strcmp(elem_str, "fire") == 0) {
            element = ELEMENT_FIRE;
        } else if (strcmp(elem_str, "WATER") == 0 || strcmp(elem_str, "water") == 0) {
            element = ELEMENT_WATER;
        } else if (strcmp(elem_str, "EARTH") == 0 || strcmp(elem_str, "earth") == 0) {
            element = ELEMENT_EARTH;
        } else if (strcmp(elem_str, "AIR") == 0 || strcmp(elem_str, "air") == 0) {
            element = ELEMENT_AIR;
        } else if (strcmp(elem_str, "NONE") == 0 || strcmp(elem_str, "none") == 0) {
            element = ELEMENT_NONE;
        }
    }

    /* 检查是否为代数数格式 */
    if (strncmp(str, "algebraic:", 10) == 0) {
        const char *value_start = str + 10;
        char *end = NULL;
        double value = strtod(value_start, &end);
        if (end != value_start && value_start != colon) {
            return rune_create_algebraic(value, element);
        }
        LV00_LOG_WARNING("rune_parse: 无法解析代数数值 '%s'", value_start);
        return NULL;
    }

    /* 检查是否为有理数格式（带前缀） */
    const char *num_start = str;
    if (strncmp(str, "rational:", 9) == 0) {
        num_start = str + 9;
    }

    /* 解析分子 */
    char *slash = strchr(num_start, '/');
    char *elem_colon = colon ? (char *) colon : NULL;

    /* 确定数值部分的结束位置 */
    const char *num_end = elem_colon ? elem_colon : (strchr(num_start, '\0'));

    int64_t numerator = 0;
    uint64_t denominator = 1;

    if (slash && slash < num_end) {
        /* 有分数格式: num/denom */
        char num_buf[64];
        size_t num_len = (size_t) (slash - num_start);
        if (num_len >= sizeof(num_buf)) {
            LV00_LOG_WARNING("rune_parse: 分子过长");
            return NULL;
        }
        /* [Bug修复] strncpy + 手动终止 → lv00_strlcpy，更安全简洁 */
        lv00_strlcpy(num_buf, num_start, num_len + 1);
        numerator = strtoll(num_buf, NULL, 10);

        /* 解析分母 */
        char denom_buf[64];
        size_t denom_len = (size_t) (num_end - slash - 1);
        if (denom_len >= sizeof(denom_buf)) {
            LV00_LOG_WARNING("rune_parse: 分母过长");
            return NULL;
        }
        /* [Bug修复] strncpy + 手动终止 → lv00_strlcpy，更安全简洁 */
        lv00_strlcpy(denom_buf, slash + 1, denom_len + 1);
        denominator = strtoull(denom_buf, NULL, 10);

        if (denominator == 0) {
            LV00_LOG_WARNING("rune_parse: 分母不能为零");
            return NULL;
        }
    } else {
        /* 整数格式 */
        char num_buf[64];
        size_t num_len = (size_t) (num_end - num_start);
        if (num_len >= sizeof(num_buf)) {
            LV00_LOG_WARNING("rune_parse: 数值过长");
            return NULL;
        }
        /* [Bug修复] strncpy + 手动终止 → lv00_strlcpy，更安全简洁 */
        lv00_strlcpy(num_buf, num_start, num_len + 1);
        numerator = strtoll(num_buf, NULL, 10);
    }

    return rune_create_rational(numerator, denominator, element);
}

/**
 * @brief 获取符文的符号坐标值
 *
 * @param rune 符文指针
 * @return 符号坐标指针，rune 为 NULL 时返回 NULL
 */
SymbolicCoord *rune_get_value(const Rune *rune) {
    if (!rune)
        return NULL;
    return rune->coord;
}

/**
 * @brief 获取符文的魔法元素类型
 *
 * @param rune 符文指针
 * @return 魔法元素类型，rune 为 NULL 时返回 ELEMENT_NONE
 */
MagicElement rune_get_element(const Rune *rune) {
    if (!rune)
        return ELEMENT_NONE;
    return rune->element;
}

/**
 * @brief 获取符文的威力等级
 *
 * @param rune 符文指针
 * @return 威力等级（1-10），rune 为 NULL 时返回 0
 */
int rune_get_power(const Rune *rune) {
    if (!rune)
        return 0;
    return rune->power_level;
}

/**
 * @brief 设置符文的威力等级
 *
 * 威力等级会被限制在 [1, 10] 范围内。
 *
 * @param rune  符文指针
 * @param power 目标威力等级（超出范围会被截断）
 */
void rune_set_power(Rune *rune, int power) {
    if (!rune)
        return;
    rune->power_level = power > MAGIC_RUNE_POWER_MAX ? MAGIC_RUNE_POWER_MAX
                                                     : (power < MAGIC_RUNE_POWER_MIN ? MAGIC_RUNE_POWER_MIN : power);
}

/* ============================================================
 * 符文序列实现
 * ============================================================ */

/**
 * @brief 创建空的符文序列
 *
 * 创建一个初始容量为 16 的动态符文序列。
 * 调用者负责通过 rune_sequence_destroy 释放。
 *
 * @return 新创建的符文序列指针，失败返回 NULL
 */
RuneSequence *rune_sequence_create(void) {
    RuneSequence *seq = (RuneSequence *) lv00_malloc(sizeof(RuneSequence));
    if (!seq)
        return NULL;

    seq->capacity = MAGIC_RUNE_SEQUENCE_INIT_CAP;
    seq->rune_count = 0;
    seq->runes = (Rune **) lv00_malloc(seq->capacity * sizeof(Rune *));

    if (!seq->runes) {
        lv00_free((void **) &seq);
        return NULL;
    }

    return seq;
}

/**
 * @brief 向符文序列中添加一个符文
 *
 * 如果当前容量不足，会自动扩容（容量翻倍）。
 * 序列将接管符文的所有权，调用者不应再手动释放该符文。
 *
 * @param seq  符文序列指针
 * @param rune 待添加的符文指针
 * @return 添加成功返回 true，参数无效或内存不足返回 false
 */
bool rune_sequence_add(RuneSequence *seq, Rune *rune) {
    if (!seq || !rune)
        return false;

    if (seq->rune_count >= seq->capacity) {
        int new_capacity = seq->capacity * MAGIC_RUNE_SEQUENCE_GROWTH;
        Rune **new_runes = (Rune **) lv00_realloc(seq->runes, new_capacity * sizeof(Rune *));
        if (!new_runes)
            return false;
        seq->runes = new_runes;
        seq->capacity = new_capacity;
    }

    seq->runes[seq->rune_count++] = rune;
    return true;
}

/**
 * @brief 获取符文序列中指定索引的符文
 *
 * @param seq   符文序列指针
 * @param index 符文索引（从 0 开始）
 * @return 符文指针，索引越界或参数无效时返回 NULL
 */
Rune *rune_sequence_get(const RuneSequence *seq, int index) {
    if (!seq || index < 0 || index >= seq->rune_count)
        return NULL;
    return seq->runes[index];
}

/**
 * @brief 获取符文序列中的符文数量
 *
 * @param seq 符文序列指针
 * @return 符文数量，seq 为 NULL 时返回 0
 */
int rune_sequence_length(const RuneSequence *seq) {
    if (!seq)
        return 0;
    return seq->rune_count;
}

/**
 * @brief 销毁符文序列及其包含的所有符文
 *
 * 依次销毁序列中的每个符文，然后释放序列结构本身。
 * 释放后指针会被自动置为 NULL。可安全传入 NULL。
 *
 * @param seq 待销毁的符文序列指针
 */
void rune_sequence_destroy(RuneSequence *seq) {
    if (!seq)
        return;

    for (int i = 0; i < seq->rune_count; i++) {
        rune_destroy(seq->runes[i]);
    }
    lv00_free((void **) &seq->runes);
    lv00_free((void **) &seq);
}

/* ============================================================
 * 元素反应矩阵
 * ============================================================ */

/** 元素反应矩阵：定义两种元素之间的相互作用关系 */
static ElementReaction element_reaction_matrix[MAGIC_ELEMENT_TOTAL_COUNT][MAGIC_ELEMENT_TOTAL_COUNT] = {
    /*        NONE  FIRE  WATER AIR  EARTH ETHER */
    /*NONE*/ {ELEMENT_REACTION_NONE, ELEMENT_REACTION_NONE, ELEMENT_REACTION_NONE, ELEMENT_REACTION_NONE, ELEMENT_REACTION_NONE, ELEMENT_REACTION_NONE},
    /*FIRE*/ {ELEMENT_REACTION_NONE, ELEMENT_REACTION_NONE, ELEMENT_REACTION_CONFLICT, ELEMENT_REACTION_ENHANCE, ELEMENT_REACTION_ENHANCE, ELEMENT_REACTION_NONE},
    /*WATER*/ {ELEMENT_REACTION_NONE, ELEMENT_REACTION_CONFLICT, ELEMENT_REACTION_NONE, ELEMENT_REACTION_WEAKEN, ELEMENT_REACTION_ENHANCE, ELEMENT_REACTION_NONE},
    /*AIR*/ {ELEMENT_REACTION_NONE, ELEMENT_REACTION_ENHANCE, ELEMENT_REACTION_WEAKEN, ELEMENT_REACTION_NONE, ELEMENT_REACTION_CONFLICT, ELEMENT_REACTION_NONE},
    /*EARTH*/ {ELEMENT_REACTION_NONE, ELEMENT_REACTION_ENHANCE, ELEMENT_REACTION_ENHANCE, ELEMENT_REACTION_CONFLICT, ELEMENT_REACTION_NONE, ELEMENT_REACTION_NONE},
    /*ETHER*/ {ELEMENT_REACTION_NONE, ELEMENT_REACTION_NONE, ELEMENT_REACTION_NONE, ELEMENT_REACTION_NONE, ELEMENT_REACTION_NONE, ELEMENT_REACTION_NONE}
};

/**
 * @brief 查询两种魔法元素之间的反应关系
 *
 * 根据元素反应矩阵查询 e1 对 e2 的反应类型。
 * 超出有效范围的元素索引将返回 ELEMENT_REACTION_NONE。
 *
 * @param e1 第一种魔法元素
 * @param e2 第二种魔法元素
 * @return 元素反应类型
 */
ElementReaction array_check_element_reaction(MagicElement e1, MagicElement e2) {
    if (e1 < 0 || e1 > ELEMENT_ETHER || e2 < 0 || e2 > ELEMENT_ETHER) {
        return ELEMENT_REACTION_NONE;
    }
    return element_reaction_matrix[e1][e2];
}

/* ============================================================
 * 魔法阵系统实现
 * ============================================================ */

/** 魔法阵结构体：由符文序列、约束图和约束列表组成 */
struct MagicArray {
    char *name;                        /* 魔法阵名称 */
    RuneSequence *runes;              /* 符文序列 */
    ConstraintGraph *graph;           /* 底层约束图 */
    ArrayConstraintType *constraints; /* 约束类型数组 */
    int constraint_count;             /* 当前约束数量 */
    int constraint_capacity;          /* 约束数组容量 */
};

/**
 * @brief 创建空的魔法阵
 *
 * 初始化魔法阵的符文序列、底层约束图和约束数组。
 * 约束数组初始容量为 32。
 *
 * @return 新创建的魔法阵指针，失败返回 NULL
 */
MagicArray *magic_array_create(void) {
    MagicArray *array = (MagicArray *) lv00_malloc(sizeof(MagicArray));
    if (!array)
        return NULL;

    array->runes = rune_sequence_create();
    if (!array->runes) {
        lv00_free((void **) &array);
        return NULL;
    }

    array->graph = graph_create();
    if (!array->graph) {
        rune_sequence_destroy(array->runes);
        lv00_free((void **) &array);
        return NULL;
    }

    array->constraint_count = 0;
    array->constraint_capacity = MAGIC_ARRAY_CONSTRAINT_INIT_CAP;
    array->constraints = (ArrayConstraintType *) lv00_malloc(array->constraint_capacity * sizeof(ArrayConstraintType));

    if (!array->constraints) {
        graph_destroy(array->graph);
        rune_sequence_destroy(array->runes);
        lv00_free((void **) &array);
        return NULL;
    }

    return array;
}

/**
 * @brief 销毁魔法阵并释放所有关联资源
 *
 * 释放符文序列、约束图、约束数组以及魔法阵结构体本身。
 * 释放后指针会被自动置为 NULL。可安全传入 NULL。
 *
 * @param array 待销毁的魔法阵指针
 */
void magic_array_destroy(MagicArray *array) {
    if (!array)
        return;

    if (array->runes) {
        rune_sequence_destroy(array->runes);
    }
    if (array->graph) {
        graph_destroy(array->graph);
    }
    if (array->constraints) {
        lv00_free((void **) &array->constraints);
    }
    lv00_free((void **) &array);
}

/**
 * @brief 向魔法阵中添加符文
 *
 * 将符文添加到魔法阵的符文序列和底层约束图中。
 * 调用者保留原符文的所有权，魔法阵内部会创建副本。
 *
 * @param array 魔法阵指针
 * @param rune  待添加的符文指针
 * @return 符文在图中的节点索引，失败返回 -1
 */
int magic_array_add_rune(MagicArray *array, Rune *rune) {
    if (!array || !rune)
        return -1;

    SymbolicCoord *coord = rune_get_value(rune);
    SymbolicCoord *coords[2] = {coord, coord};

    AddNodeResult result = graph_add_point(array->graph, coords, 1);
    if (result != ADD_NODE_OK) {
        return -1;
    }

    int index = array->graph->next_node_id - 1;

    /* 复制符文到序列（调用者保留原符文所有权） */
    Rune *rune_clone = rune_copy(rune);
    if (!rune_clone) {
        graph_remove_node(array->graph, index);
        return -1;
    }

    if (!rune_sequence_add(array->runes, rune_clone)) {
        graph_remove_node(array->graph, index);
        rune_destroy(rune_clone);
        return -1;
    }

    return index;
}

/**
 * @brief 从魔法阵中移除指定索引的符文
 *
 * 同时从约束图和符文序列中移除该符文，后续符文索引会前移。
 *
 * @param array      魔法阵指针
 * @param rune_index 待移除符文的索引
 * @return 移除成功返回 true，参数无效或索引越界返回 false
 */
bool magic_array_remove_rune(MagicArray *array, int rune_index) {
    if (!array || rune_index < 0 || rune_index >= rune_sequence_length(array->runes)) {
        return false;
    }

    /* 从图中移除节点 */
    GeomNode *node = graph_get_node(array->graph, rune_index);
    if (node) {
        graph_remove_node(array->graph, rune_index);
    }

    /* 从序列中移除符文 */
    rune_destroy(array->runes->runes[rune_index]);
    for (int i = rune_index; i < array->runes->rune_count - 1; i++) {
        array->runes->runes[i] = array->runes->runes[i + 1];
    }
    array->runes->rune_count--;

    return true;
}

/**
 * @brief 获取魔法阵中指定索引的符文
 *
 * @param array      魔法阵指针
 * @param rune_index 符文索引
 * @return 符文指针，参数无效时返回 NULL
 */
Rune *magic_array_get_rune(const MagicArray *array, int rune_index) {
    if (!array)
        return NULL;
    return rune_sequence_get(array->runes, rune_index);
}

/**
 * @brief 获取魔法阵中的符文数量
 *
 * @param array 魔法阵指针
 * @return 符文数量，array 为 NULL 时返回 0
 */
int magic_array_get_rune_count(const MagicArray *array) {
    if (!array)
        return 0;
    return rune_sequence_length(array->runes);
}

/**
 * @brief 向魔法阵添加约束关系
 *
 * 在两个符文之间建立约束关系，同时更新底层约束图。
 * 约束类型会被映射为约束图的内部类型。
 * 如果约束数组容量不足，会自动扩容（容量翻倍）。
 *
 * @param array        魔法阵指针
 * @param type         约束类型
 * @param rune1_index 第一个符文的索引
 * @param rune2_index 第二个符文的索引
 * @return 约束在图中的 ID，失败返回 -1
 */
int magic_array_add_constraint(MagicArray *array, ArrayConstraintType type, int rune1_index, int rune2_index) {
    if (!array)
        return -1;
    if (rune1_index < 0 || rune1_index >= array->runes->rune_count)
        return -1;
    if (rune2_index < 0 || rune2_index >= array->runes->rune_count)
        return -1;

    if (array->constraint_count >= array->constraint_capacity) {
        int new_capacity = array->constraint_capacity * MAGIC_ARRAY_CONSTRAINT_GROWTH;
        ArrayConstraintType *new_constraints =
            (ArrayConstraintType *) lv00_realloc(array->constraints, new_capacity * sizeof(ArrayConstraintType));
        if (!new_constraints)
            return -1;
        array->constraints = new_constraints;
        array->constraint_capacity = new_capacity;
    }

    /* 转换为底层约束类型并添加到图 */
    ConstraintType graph_type;
    switch (type) {
        case ARRAY_CONNECTION:
            graph_type = CONNECTION;
            break;
        case ARRAY_ENHANCEMENT:
        case ARRAY_CONFLICT:
            graph_type = INCIDENCE;
            break;
        case ARRAY_INTERSECTION:
            graph_type = INTERSECTION;
            break;
        case ARRAY_CONTAINMENT:
            graph_type = CONTAINMENT;
            break;
        default:
            graph_type = CONNECTION;
    }

    int participants[2] = {rune1_index, rune2_index};
    AddConstraintResult result = graph_add_incidence(array->graph, participants[0], participants[1]);

    if (result == ADD_CONSTRAINT_OK) {
        array->constraints[array->constraint_count++] = type;
        return array->graph->next_constraint_id - 1;
    }

    return -1;
}

/**
 * @brief 从魔法阵中移除指定索引的约束
 *
 * 同时从约束图和约束数组中移除，后续约束索引会前移。
 *
 * @param array           魔法阵指针
 * @param constraint_index 约束索引
 * @return 移除成功返回 true，参数无效或索引越界返回 false
 */
bool magic_array_remove_constraint(MagicArray *array, int constraint_index) {
    if (!array || constraint_index < 0 || constraint_index >= array->constraint_count) {
        return false;
    }

    /* 从图中移除约束 */
    graph_remove_constraint(array->graph, constraint_index);

    /* 从数组中移除 */
    for (int i = constraint_index; i < array->constraint_count - 1; i++) {
        array->constraints[i] = array->constraints[i + 1];
    }
    array->constraint_count--;

    return true;
}

/**
 * @brief 获取魔法阵中的约束数量
 *
 * @param array 魔法阵指针
 * @return 约束数量，array 为 NULL 时返回 0
 */
int magic_array_get_constraint_count(const MagicArray *array) {
    if (!array)
        return 0;
    return array->constraint_count;
}

/**
 * @brief 检查魔法阵的元素平衡性
 *
 * 通过计算五种魔法元素（不含 ELEMENT_NONE）分布的方差来判断平衡性。
 * 方差小于均值的两倍时视为平衡。
 *
 * @param array 魔法阵指针
 * @return 平衡返回 true，不平衡或参数无效返回 false
 */
bool magic_array_check_balance(const MagicArray *array) {
    if (!array)
        return false;

    int element_counts[MAGIC_ELEMENT_TOTAL_COUNT] = {0};
    for (int i = 0; i < array->runes->rune_count; i++) {
        Rune *rune = array->runes->runes[i];
        /* 边界检查：确保元素值在有效范围内，防止数组越界 */
        if (rune->element >= 0 && rune->element <= ELEMENT_ETHER) {
            element_counts[rune->element]++;
        }
    }

    /* 计算元素分布的方差 */
    double mean = (double) array->runes->rune_count / (double) MAGIC_REAL_ELEMENT_COUNT;
    double variance = 0.0;

    for (int i = 1; i <= MAGIC_REAL_ELEMENT_COUNT; i++) { /* 跳过 ELEMENT_NONE */
        double diff = element_counts[i] - mean;
        variance += diff * diff;
    }
    variance /= (double) MAGIC_REAL_ELEMENT_COUNT;

    /* 方差小于阈值表示平衡 */
    return variance < mean * MAGIC_ELEMENT_BALANCE_THRESHOLD;
}

/**
 * @brief 统计魔法阵中指定元素的符文数量
 *
 * @param array   魔法阵指针
 * @param element 要统计的魔法元素类型
 * @return 该元素的符文数量，array 为 NULL 时返回 0
 */
int array_count_elements(const MagicArray *array, MagicElement element) {
    if (!array)
        return 0;

    int count = 0;
    for (int i = 0; i < array->runes->rune_count; i++) {
        if (array->runes->runes[i]->element == element) {
            count++;
        }
    }
    return count;
}

/**
 * @brief 计算魔法阵的稳定性评分
 *
 * 稳定性受冲突约束数量和符文数量影响：
 * - 每个冲突约束降低 0.1 稳定性
 * - 符文数量少于 3 时稳定性减半
 * - 最终结果限制在 [0.0, 1.0] 范围内
 *
 * @param array 魔法阵指针
 * @return 稳定性评分（0.0 ~ 1.0），array 为 NULL 或无符文时返回 0.0
 */
double array_calculate_stability(const MagicArray *array) {
    if (!array || array->runes->rune_count == 0)
        return 0.0;

    double stability = MAGIC_STABILITY_MAX;
    int conflicts = 0;

    for (int i = 0; i < array->constraint_count; i++) {
        if (array->constraints[i] == ARRAY_CONFLICT) {
            conflicts++;
        }
    }

    /* 每有一个冲突约束，稳定性降低 */
    stability -= (double) conflicts * MAGIC_STABILITY_CONFLICT_PENALTY;

    /* 符文数量过少也不稳定 */
    if (array->runes->rune_count < MAGIC_STABILITY_MIN_RUNES) {
        stability *= MAGIC_STABILITY_TOO_FEW_MULTIPLIER;
    }

    return stability < 0.0 ? 0.0 : stability;
}

/**
 * @brief 深拷贝魔法阵
 *
 * 创建魔法阵的完整副本，包括所有符文和约束关系。
 * 调用者负责通过 magic_array_destroy 释放返回的副本。
 *
 * @param src 源魔法阵指针
 * @return 新魔法阵指针（深拷贝），失败或 src 为 NULL 时返回 NULL
 */
MagicArray *magic_array_copy(const MagicArray *src) {
    if (!src)
        return NULL;

    MagicArray *copy = magic_array_create();
    if (!copy)
        return NULL;

    /* 复制符文 */
    for (int i = 0; i < src->runes->rune_count; i++) {
        Rune *rune = rune_copy(src->runes->runes[i]);
        if (!rune) {
            magic_array_destroy(copy);
            return NULL;
        }
        if (!rune_sequence_add(copy->runes, rune)) {
            rune_destroy(rune);
            magic_array_destroy(copy);
            return NULL;
        }

        /* 添加到图 */
        SymbolicCoord *coords[] = { rune->coord };
        graph_add_point(copy->graph, coords, 1);
    }

    /* 复制约束 */
    for (int i = 0; i < src->constraint_count; i++) {
        magic_array_add_constraint(copy, src->constraints[i], i, (i + 1) % src->runes->rune_count);
    }

    return copy;
}

/**
 * @brief 合并两个魔法阵
 *
 * 将源魔法阵 (src) 的所有符文和约束合并到目标魔法阵 (dest) 中。
 * 合并过程中会为每个符文和约束创建副本。
 *
 * @param dest 目标魔法阵（接收合并内容）
 * @param src  源魔法阵（提供合并内容）
 * @return 合并成功返回 true，参数无效或内存不足返回 false
 */
bool magic_array_merge(MagicArray *dest, const MagicArray *src) {
    if (!dest || !src)
        return false;

    /* 合并所有符文 */
    for (int i = 0; i < src->runes->rune_count; i++) {
        int idx = magic_array_add_rune(dest, src->runes->runes[i]);
        if (idx < 0) {
            LV00_LOG_WARNING("magic_array_merge: 合并符文失败，索引 %d", i);
            return false;
        }
    }

    /* 合并所有约束 */
    for (int i = 0; i < src->constraint_count; i++) {
        /* 约束索引需要映射到 dest 中的新索引 */
        int result = magic_array_add_constraint(dest, src->constraints[i], i, (i + 1) % src->runes->rune_count);
        if (result < 0) {
            LV00_LOG_WARNING("magic_array_merge: 合并约束失败，索引 %d", i);
            return false;
        }
    }

    return true;
}

/**
 * @brief 将魔法阵序列化为 JSON 字符串
 *
 * 将魔法阵的符文数量、约束数量以及每个符文的基本信息
 * 序列化为 JSON 格式的字符串。
 *
 * @param array 魔法阵指针
 * @return 新分配的 JSON 字符串，失败返回 NULL（调用者需用 lv00_free 释放）
 */
char *magic_array_serialize(const MagicArray *array) {
    if (!array)
        return NULL;

    /* 计算所需缓冲区大小 */
    int rune_count = array->runes->rune_count;
    int constraint_count = array->constraint_count;

    /* 基础 JSON 结构 + 每个符文预估大小 */
    size_t buf_size = MAGIC_SERIALIZE_JSON_BASE_SIZE + (size_t) rune_count * MAGIC_SERIALIZE_PER_RUNE_SIZE;
    char *json = (char *) lv00_malloc(buf_size);
    if (!json)
        return NULL;

    int offset = 0;
    offset += snprintf(json + offset, buf_size - offset, "{\"rune_count\":%d,\"constraint_count\":%d,\"runes\":[",
                       rune_count, constraint_count);

    /* 序列化每个符文 */
    for (int i = 0; i < rune_count; i++) {
        Rune *rune = array->runes->runes[i];
        const char *elem_str = element_to_string(rune->element);
        if (i > 0) {
            offset += snprintf(json + offset, buf_size - offset, ",");
        }
        offset += snprintf(json + offset, buf_size - offset, "{\"element\":\"%s\",\"power\":%d}", elem_str,
                           rune->power_level);
    }

    offset += snprintf(json + offset, buf_size - offset, "]}");

    return json;
}

/**
 * @brief 从 JSON 字符串反序列化魔法阵
 *
 * 支持的 JSON 格式：
 *   {"name":"阵名","runes":[{"type":"rational","num":1,"denom":2,"element":"FIRE"},...]}
 *
 * JSON 解析器说明：
 *   本解析器采用手写实现，不依赖外部 JSON 库。使用 strstr 进行字段查找，
 *   并通过跳过字符串值内部内容来避免误匹配。
 *
 *   已知限制：
 *   - 不支持 JSON 字符串中的 unicode 转义（\uXXXX），遇到时将跳过
 *   - 不支持嵌套超过一层的对象/数组（runes 数组内的对象应为扁平结构）
 *   - 字段查找基于 strstr，如果字符串值中包含与关键字相同的文本可能误匹配
 *     （已通过跳过字符串值的机制缓解此问题）
 *
 *   对于复杂的 JSON 输入，建议使用标准 JSON 库（如 cJSON）替代。
 *
 * @param json JSON 格式字符串
 * @return 反序列化成功返回新创建的魔法阵，失败返回 NULL
 */

/**
 * @brief 在 JSON 文本中安全地查找键名（跳过字符串值内部）
 *
 * 从位置 start 开始向后搜索 "key" 模式，但跳过所有 JSON 字符串值
 * 的内部内容（包括转义字符），避免在字符串值中误匹配键名。
 *
 * @param start 搜索起始位置
 * @param key   要查找的键名（不含引号，如 "type"）
 * @return 找到返回键名起始位置的指针，未找到返回 NULL
 */
static const char *json_find_key_safe(const char *start, const char *key) {
    if (!start || !key) return NULL;

    /* 构造搜索模式: "key" */
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char *p = start;
    while (*p) {
        /* 检查是否匹配目标键名 */
        if (strncmp(p, pattern, strlen(pattern)) == 0) {
            return p;
        }

        /* 如果当前字符是双引号，跳过整个字符串值 */
        if (*p == '"') {
            p++;
            while (*p && *p != '"') {
                if (*p == '\\' && *(p + 1)) {
                    /* 跳过转义字符：\", \\, \/, \b, \f, \n, \r, \t, \uXXXX */
                    p++;
                    if (*p == 'u' && *(p + 1) && *(p + 2) && *(p + 3) && *(p + 4)) {
                        /* 跳过 \uXXXX unicode 转义（4个十六进制数字） */
                        p += 5;
                    } else {
                        p++; /* 跳过转义后的单个字符 */
                    }
                } else {
                    p++;
                }
            }
            if (*p == '"') p++;
        } else {
            p++;
        }
    }

    return NULL;
}

/**
 * @brief 从 JSON 字符串值中提取解码后的文本
 *
 * 处理常见的 JSON 转义序列：\", \\, \/, \b, \f, \n, \r, \t。
 * 不处理 \uXXXX unicode 转义（遇到时保留原始转义文本）。
 *
 * @param src  指向字符串值第一个字符（引号后）的指针
 * @param dst  目标缓冲区
 * @param dst_cap 目标缓冲区容量
 * @return 写入的字符数（不含终止符），-1 表示错误
 */
static int json_decode_string(const char *src, char *dst, size_t dst_cap) {
    if (!src || !dst || dst_cap == 0) return -1;

    size_t written = 0;
    const char *p = src;

    while (*p && *p != '"' && written < dst_cap - 1) {
        if (*p == '\\') {
            p++;
            switch (*p) {
                case '"':  dst[written++] = '"';  break;
                case '\\': dst[written++] = '\\'; break;
                case '/':  dst[written++] = '/';  break;
                case 'b':  dst[written++] = '\b'; break;
                case 'f':  dst[written++] = '\f'; break;
                case 'n':  dst[written++] = '\n'; break;
                case 'r':  dst[written++] = '\r'; break;
                case 't':  dst[written++] = '\t'; break;
                case 'u':
                    /* \uXXXX unicode 转义：当前不解码，保留为原始文本 */
                    if (written + 6 < dst_cap - 1) {
                        dst[written++] = '\\';
                        dst[written++] = 'u';
                        if (p[1]) dst[written++] = p[1];
                        if (p[2]) dst[written++] = p[2];
                        if (p[3]) dst[written++] = p[3];
                        if (p[4]) dst[written++] = p[4];
                        p += 4;
                    }
                    break;
                default:
                    /* 未知转义序列，保留原样 */
                    if (written + 1 < dst_cap - 1) {
                        dst[written++] = '\\';
                        dst[written++] = *p;
                    }
                    break;
            }
            p++;
        } else {
            dst[written++] = *p;
            p++;
        }
    }

    dst[written] = '\0';
    return (int) written;
}

/**
 * @brief 跳过 JSON 字符串值
 *
 * 从当前位置（应在引号上）跳过整个字符串值，包括转义字符。
 *
 * @param p 指向字符串起始引号的指针
 * @return 跳过字符串后的下一个字符位置
 */
static const char *json_skip_string(const char *p) {
    if (!p || *p != '"') return p;
    p++; /* 跳过起始引号 */
    while (*p && *p != '"') {
        if (*p == '\\' && *(p + 1)) {
            p += 2; /* 跳过转义序列 */
        } else {
            p++;
        }
    }
    if (*p == '"') p++; /* 跳过结束引号 */
    return p;
}

/**
 * @brief 跳过 JSON 值（字符串、数字、对象、数组、布尔、null）
 *
 * @param p 指向值起始位置的指针
 * @return 跳过值后的下一个字符位置
 */
static const char *json_skip_value(const char *p) {
    if (!p) return NULL;

    /* 跳过空白 */
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;

    if (*p == '"') {
        return json_skip_string(p);
    } else if (*p == '{') {
        /* 跳过对象 */
        p++; /* 跳过 '{' */
        while (*p && *p != '}') {
            if (*p == '"') {
                p = json_skip_string(p); /* 跳过键 */
                while (*p == ' ' || *p == '\t' || *p == ':' || *p == '\n' || *p == '\r') p++;
                p = json_skip_value(p); /* 跳过值 */
            } else {
                p++;
            }
            while (*p == ' ' || *p == '\t' || *p == ',' || *p == '\n' || *p == '\r') p++;
        }
        if (*p == '}') p++;
        return p;
    } else if (*p == '[') {
        /* 跳过数组 */
        p++; /* 跳过 '[' */
        while (*p && *p != ']') {
            p = json_skip_value(p);
            while (*p == ' ' || *p == '\t' || *p == ',' || *p == '\n' || *p == '\r') p++;
        }
        if (*p == ']') p++;
        return p;
    } else if (*p == 't') {
        return p + 4; /* true */
    } else if (*p == 'f') {
        return p + 5; /* false */
    } else if (*p == 'n') {
        return p + 4; /* null */
    } else {
        /* 数字 */
        while (*p && (*p == '-' || *p == '+' || *p == '.' || *p == 'e' || *p == 'E' ||
               (*p >= '0' && *p <= '9'))) {
            p++;
        }
        return p;
    }
}

MagicArray *magic_array_deserialize(const char *json) {
    if (!json || json[0] == '\0') {
        LV00_LOG_WARNING("magic_array_deserialize: 输入 JSON 为空");
        return NULL;
    }

    /* 跳过前导空白 */
    while (*json == ' ' || *json == '\t' || *json == '\n' || *json == '\r')
        json++;

    /* 检查 JSON 对象起始 */
    if (json[0] != '{') {
        LV00_LOG_WARNING("magic_array_deserialize: JSON 格式无效，期望 '{'");
        return NULL;
    }

    /* 创建空的魔法阵 */
    MagicArray *array = magic_array_create();
    if (!array) {
        LV00_LOG_WARNING("magic_array_deserialize: 无法创建魔法阵");
        return NULL;
    }

    /* 查找 runes 数组（使用安全查找，避免误匹配字符串值内的 "runes"） */
    const char *runes_key = json_find_key_safe(json, "runes");
    if (!runes_key) {
        /* 没有 runes 字段，返回空魔法阵 */
        return array;
    }

    /* 查找数组起始 */
    const char *array_start = strchr(runes_key, '[');
    if (!array_start) {
        LV00_LOG_WARNING("magic_array_deserialize: runes 不是数组格式");
        return array;
    }

    /* 遍历数组元素 */
    const char *ptr = array_start + 1;
    while (*ptr && *ptr != ']') {
        /* 跳过空白和逗号 */
        while (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == ',' || *ptr == '\r')
            ptr++;
        if (*ptr == ']')
            break;

        /* 查找对象起始 */
        if (*ptr != '{') {
            ptr++;
            continue;
        }

        /* 使用安全查找提取字段，避免嵌套对象/字符串值中的误匹配 */
        const char *obj_end = strchr(ptr, '}');
        if (!obj_end)
            break;

        /* 计算当前对象的范围，限制字段搜索在此范围内 */
        const char *type_key = json_find_key_safe(ptr, "type");
        const char *num_key = json_find_key_safe(ptr, "num");
        const char *denom_key = json_find_key_safe(ptr, "denom");
        const char *value_key = json_find_key_safe(ptr, "value");
        const char *element_key = json_find_key_safe(ptr, "element");

        /* 确保找到的键在当前对象范围内 */
        if (type_key && type_key > obj_end) type_key = NULL;
        if (num_key && num_key > obj_end) num_key = NULL;
        if (denom_key && denom_key > obj_end) denom_key = NULL;
        if (value_key && value_key > obj_end) value_key = NULL;
        if (element_key && element_key > obj_end) element_key = NULL;

        Rune *rune = NULL;
        MagicElement element = ELEMENT_NONE;

        /* 解析元素类型 */
        if (element_key) {
            const char *elem_val_start = strchr(element_key + 8, ':');
            if (elem_val_start) {
                elem_val_start++;
                while (*elem_val_start == ' ' || *elem_val_start == '"')
                    elem_val_start++;
                if (strncmp(elem_val_start, "FIRE", 4) == 0)
                    element = ELEMENT_FIRE;
                else if (strncmp(elem_val_start, "WATER", 5) == 0)
                    element = ELEMENT_WATER;
                else if (strncmp(elem_val_start, "EARTH", 5) == 0)
                    element = ELEMENT_EARTH;
                else if (strncmp(elem_val_start, "AIR", 3) == 0)
                    element = ELEMENT_AIR;
            }
        }

        /* 根据类型创建符文 */
        if (type_key && strstr(type_key, "\"rational\"")) {
            /* 有理数类型 */
            int64_t num = 0;
            uint64_t denom = 1;

            if (num_key) {
                const char *num_val = strchr(num_key + 5, ':');
                if (num_val)
                    num = strtoll(num_val + 1, NULL, 10);
            }
            if (denom_key) {
                const char *denom_val = strchr(denom_key + 7, ':');
                if (denom_val)
                    denom = strtoull(denom_val + 1, NULL, 10);
            }
            if (denom == 0)
                denom = 1;

            rune = rune_create_rational(num, denom, element);
        } else if (type_key && strstr(type_key, "\"algebraic\"")) {
            /* 代数数类型 */
            double value = 0.0;
            if (value_key) {
                const char *val_start = strchr(value_key + 7, ':');
                if (val_start)
                    value = strtod(val_start + 1, NULL);
            }
            rune = rune_create_algebraic(value, element);
        }

        if (rune) {
            magic_array_add_rune(array, rune);
        }

        ptr = obj_end + 1;
    }

    /* 尝试解析名称字段（使用安全查找） */
    const char *name_key = json_find_key_safe(json, "name");
    if (name_key) {
        const char *name_start = strchr(name_key + 6, ':');
        if (name_start) {
            name_start++;
            while (*name_start == ' ')
                name_start++;
            if (*name_start == '"') {
                name_start++; /* 跳过起始引号 */
                char name_buf[256];
                int name_len = json_decode_string(name_start, name_buf, sizeof(name_buf));
                if (name_len > 0) {
                    char *name_copy = (char *) lv00_malloc((size_t) name_len + 1);
                    if (name_copy) {
                        lv00_strlcpy(name_copy, name_buf, (size_t) name_len + 1);
                        if (array->name)
                            lv00_free((void **) &array->name);
                        array->name = name_copy;
                    }
                }
            }
        }
    }

    return array;
}

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
    Spell *spell = (Spell *) lv00_malloc(sizeof(Spell));
    if (!spell)
        return NULL;

    memset(spell, 0, sizeof(Spell));

    if (name) {
        spell->name = lv00_strdup_safe(name);
    } else {
        spell->name = lv00_strdup_safe("Unnamed Spell");
    }

    /* 检查名称分配是否成功 */
    if (!spell->name) {
        lv00_free((void **) &spell);
        return NULL;
    }

    spell->description = lv00_strdup_safe("");
    if (!spell->description) {
        lv00_free((void **) &spell->name);
        lv00_free((void **) &spell);
        return NULL;
    }
    spell->difficulty = MAGIC_SPELL_DIFFICULTY_DEFAULT;
    spell->input_count = 0;
    spell->output_count = MAGIC_SPELL_OUTPUT_DEFAULT;
    spell->current_stage = SPELL_STAGE_MOLDING;
    spell->status = SPELL_STATUS_IDLE;

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
void spell_destroy(Spell *spell) {
    if (!spell)
        return;

    if (spell->name)
        lv00_free((void **) &spell->name);
    if (spell->description)
        lv00_free((void **) &spell->description);
    if (spell->molding)
        rune_sequence_destroy(spell->molding);
    lv00_free((void **) &spell);
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
        return false;
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
        return false;
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
    lv00_free((void **) &spell->description);
    spell->description = lv00_strdup_safe(desc);
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
        return false;
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
    if (!spell || !seq)
        return false;

    if (spell->molding) {
        rune_sequence_destroy(spell->molding);
    }

    spell->molding = rune_sequence_create();
    if (!spell->molding)
        return false;

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
        return false;
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

    spell->current_stage = SPELL_STAGE_MOLDING;
    spell->status = SPELL_STATUS_CASTING;

    /* 开模阶段 */
    if (spell->molding->rune_count == 0) {
        spell->status = SPELL_STATUS_FAILED;
        return spell->status;
    }

    spell->current_stage = SPELL_STAGE_PURIFYING;

    /* 提纯阶段 - 检查元素 */
    bool has_matching_element = false;
    for (int i = 0; i < array->runes->rune_count; i++) {
        if (array->runes->runes[i]->element == spell->purifying_element) {
            has_matching_element = true;
            break;
        }
    }

    if (!has_matching_element && spell->purifying_purity > MAGIC_SPELL_PURITY_CHECK_THRESH) {
        spell->status = SPELL_STATUS_FAILED;
        return spell->status;
    }

    spell->current_stage = SPELL_STAGE_INFUSING;

    /* 灌注阶段 - 能量检查 */
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
    if (spell->difficulty < MAGIC_SPELL_DIFFICULTY_MIN || spell->difficulty > MAGIC_SPELL_DIFFICULTY_MAX)
        return false;
    if (spell->molding->rune_count == 0)
        return false;
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
    if (energy <= MAGIC_ENERGY_T1)
        return THRESHOLD_T1;
    if (energy <= MAGIC_ENERGY_T2)
        return THRESHOLD_T2;
    if (energy <= MAGIC_ENERGY_T3)
        return THRESHOLD_T3;
    if (energy <= MAGIC_ENERGY_T4)
        return THRESHOLD_T4;
    if (energy <= MAGIC_ENERGY_T5)
        return THRESHOLD_T5;
    return THRESHOLD_T6;
}

/* ============================================================
 * 咒语书系统
 * ============================================================ */

/** 咒语书结构体：管理多个咒语的集合 */
struct SpellBook {
    Spell **spells;  /* 咒语指针数组 */
    int spell_count; /* 当前咒语数量 */
    int capacity;    /* 数组容量 */
};

/**
 * @brief 创建空的咒语书
 *
 * 创建一个初始容量为 64 的咒语集合。
 * 调用者负责通过 spellbook_destroy 释放。
 *
 * @return 新创建的咒语书指针，失败返回 NULL
 */
SpellBook *spellbook_create(void) {
    SpellBook *book = (SpellBook *) lv00_malloc(sizeof(SpellBook));
    if (!book)
        return NULL;

    book->capacity = MAGIC_SPELLBOOK_INIT_CAP;
    book->spell_count = 0;
    book->spells = (Spell **) lv00_malloc(book->capacity * sizeof(Spell *));

    if (!book->spells) {
        lv00_free((void **) &book);
        return NULL;
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

    for (int i = 0; i < book->spell_count; i++) {
        spell_destroy(book->spells[i]);
    }
    lv00_free((void **) &book->spells);
    lv00_free((void **) &book);
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
    if (!book || !spell)
        return false;

    if (book->spell_count >= book->capacity) {
        int new_capacity = book->capacity * MAGIC_SPELLBOOK_GROWTH;
        Spell **new_spells = (Spell **) lv00_realloc(book->spells, new_capacity * sizeof(Spell *));
        if (!new_spells)
            return false;
        book->spells = new_spells;
        book->capacity = new_capacity;
    }

    book->spells[book->spell_count++] = spell;
    return true;
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

    for (int i = 0; i < book->spell_count; i++) {
        if (strcmp(book->spells[i]->name, spell_name) == 0) {
            spell_destroy(book->spells[i]);
            for (int j = i; j < book->spell_count - 1; j++) {
                book->spells[j] = book->spells[j + 1];
            }
            book->spell_count--;
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

    for (int i = 0; i < book->spell_count; i++) {
        if (strcmp(book->spells[i]->name, spell_name) == 0) {
            return book->spells[i];
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
    return book ? book->spell_count : 0;
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

    *count = book->spell_count;
    char **names = (char **) lv00_malloc(book->spell_count * sizeof(char *));

    if (!names) {
        *count = 0;
        return NULL;
    }

    for (int i = 0; i < book->spell_count; i++) {
        names[i] = lv00_strdup_safe(book->spells[i]->name);
        /* 如果某个名称复制失败，释放已分配的内存并返回 NULL */
        if (!names[i]) {
            for (int j = 0; j < i; j++) {
                lv00_free((void **) &names[j]);
            }
            lv00_free((void **) &names);
            *count = 0;
            return NULL;
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
    IncantationProfile profile = {INCANTATION_STANDARD, MAGIC_INCANTATION_PRECISION_DEFAULT,
                                  MAGIC_INCANTATION_SPEED_DEFAULT, MAGIC_INCANTATION_STEALTH_DEFAULT};

    if (!goal)
        return profile;

    if (strcmp(goal, "speed") == 0) {
        profile.length = INCANTATION_SHORT;
        profile.speed = MAGIC_INCANTATION_SPEED_FAST;
        profile.precision = MAGIC_INCANTATION_PRECISION_LOW;
        profile.stealth = MAGIC_INCANTATION_STEALTH_HIGH;
    } else if (strcmp(goal, "precision") == 0) {
        profile.length = INCANTATION_LONG;
        profile.speed = MAGIC_INCANTATION_SPEED_SLOW;
        profile.precision = MAGIC_INCANTATION_PRECISION_HIGH;
        profile.stealth = MAGIC_INCANTATION_STEALTH_LOW;
    } else if (strcmp(goal, "stealth") == 0) {
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

    double power = profile->precision * MAGIC_INCANTATION_WEIGHT_PRECISION +
                   profile->speed * MAGIC_INCANTATION_WEIGHT_SPEED +
                   profile->stealth * MAGIC_INCANTATION_WEIGHT_STEALTH;

    switch (profile->length) {
        case INCANTATION_INSTANT:
            power *= MAGIC_INCANTATION_MULT_INSTANT;
            break;
        case INCANTATION_SHORT:
            power *= MAGIC_INCANTATION_MULT_SHORT;
            break;
        case INCANTATION_STANDARD:
            power *= MAGIC_INCANTATION_MULT_STANDARD;
            break;
        case INCANTATION_LONG:
            power *= MAGIC_INCANTATION_MULT_LONG;
            break;
        case INCANTATION_RITUAL:
            power *= MAGIC_INCANTATION_MULT_RITUAL;
            break;
        default:
            break;
    }

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

    int criteria_count = 0;
    if (criteria->external_cost_unacceptable)
        criteria_count++;
    if (criteria->self_damage_too_high)
        criteria_count++;
    if (criteria->governance_uncontrollable)
        criteria_count++;

    if (criteria_count >= MAGIC_RESTRICTION_CRITERIA_ABSOLUTE)
        return RESTRICTION_ABSOLUTE;
    if (criteria_count == MAGIC_RESTRICTION_CRITERIA_FORBID)
        return RESTRICTION_FORBIDDEN;
    if (criteria_count == MAGIC_RESTRICTION_CRITERIA_CONTROL)
        return RESTRICTION_CONTROLLED;
    if (spell->difficulty > MAGIC_SPELL_RESTRICTION_DIFF)
        return RESTRICTION_LIMITED;

    return RESTRICTION_NONE;
}

/* ============================================================
 * 领域系统
 * ============================================================ */

/** 领域规则结构体 */
typedef struct {
    char *pattern;    /* 规则匹配模式 */
    double priority;  /* 规则优先级（数值越小优先级越高） */
    int action;       /* 规则动作类型 */
} DomainRule;

/** 领域结构体：定义一个魔法作用区域 */
struct Domain {
    char *name;            /* 领域名称 */
    int range;             /* 作用范围 */
    SymbolicCoord *center; /* 领域中心坐标 */
    bool active;           /* 是否激活 */
    double strength;       /* 领域强度 */

    /* 规则系统 */
    DomainRule *rules;     /* 规则动态数组 */
    int rule_count;         /* 当前规则数量 */
    int rule_capacity;      /* 规则数组容量 */
};

/**
 * @brief 创建领域
 *
 * 创建一个未激活的领域，初始强度为 0.0。
 *
 * @param name  领域名称，为 NULL 时使用 "Unnamed Domain"
 * @param range 领域作用范围
 * @return 新创建的领域指针，失败返回 NULL
 */
Domain *domain_create(const char *name, int range) {
    Domain *domain = (Domain *) lv00_malloc(sizeof(Domain));
    if (!domain)
        return NULL;

    /* 分配领域名称，检查内存分配是否成功 */
    domain->name = name ? lv00_strdup_safe(name) : lv00_strdup_safe("Unnamed Domain");
    if (!domain->name) {
        lv00_free((void **) &domain);
        return NULL;
    }

    domain->range = range;
    domain->center = NULL;
    domain->active = false;
    domain->strength = 0.0;

    /* 初始化规则系统 */
    domain->rules = NULL;
    domain->rule_count = 0;
    domain->rule_capacity = 0;

    return domain;
}

/**
 * @brief 销毁领域并释放所有关联资源
 *
 * 释放领域的名称、中心坐标以及领域结构体本身。
 * 释放后指针会被自动置为 NULL。可安全传入 NULL。
 *
 * @param domain 待销毁的领域指针
 */
void domain_destroy(Domain *domain) {
    if (!domain)
        return;
    if (domain->name)
        lv00_free((void **) &domain->name);
    if (domain->center)
        symbolic_coord_destroy(domain->center);
    /* 释放所有规则 */
    if (domain->rules) {
        for (int i = 0; i < domain->rule_count; i++) {
            if (domain->rules[i].pattern)
                lv00_free((void **) &domain->rules[i].pattern);
        }
        lv00_free((void **) &domain->rules);
    }
    lv00_free((void **) &domain);
}

/**
 * @brief 向领域添加规则
 *
 * 将规则添加到领域的规则数组中。如果存在相同 pattern 的规则则跳过。
 * 插入后按优先级排序（数值越小优先级越高）。
 *
 * @param domain     领域指针
 * @param rule_name  规则名称/模式
 * @param priority   规则优先级
 * @return 成功返回 true，domain 为 NULL 或内存分配失败返回 false
 */
bool domain_add_rule(Domain *domain, const char *rule_name, double priority) {
    if (!domain || !rule_name)
        return false;

    /* 检查重复规则（相同 pattern） */
    for (int i = 0; i < domain->rule_count; i++) {
        if (domain->rules[i].pattern &&
            strcmp(domain->rules[i].pattern, rule_name) == 0) {
            return true; /* 已存在，视为成功 */
        }
    }

    /* 扩容检查 */
    if (domain->rule_count >= domain->rule_capacity) {
        int new_cap = domain->rule_capacity == 0 ? 8 : domain->rule_capacity * 2;
        DomainRule *new_rules = (DomainRule *) lv00_realloc(
            domain->rules, new_cap * sizeof(DomainRule));
        if (!new_rules)
            return false;
        domain->rules = new_rules;
        domain->rule_capacity = new_cap;
    }

    /* 添加新规则 */
    int idx = domain->rule_count;
    domain->rules[idx].pattern = lv00_strdup_safe(rule_name);
    if (!domain->rules[idx].pattern)
        return false;
    domain->rules[idx].priority = priority;
    domain->rules[idx].action = 0; /* 默认动作 */
    domain->rule_count++;

    /* 按优先级排序（数值越小优先级越高，使用简单冒泡排序） */
    for (int i = domain->rule_count - 1; i > 0; i--) {
        if (domain->rules[i].priority < domain->rules[i - 1].priority) {
            DomainRule tmp = domain->rules[i];
            domain->rules[i] = domain->rules[i - 1];
            domain->rules[i - 1] = tmp;
        } else {
            break; /* 已排好序 */
        }
    }

    return true;
}

/**
 * @brief 激活领域
 *
 * 设置领域中心坐标并激活领域，强度初始化为 1.0。
 * 如果领域已有中心坐标，会先销毁旧的。
 *
 * @param domain 领域指针
 * @param center 领域中心坐标
 * @return 激活成功返回 true，domain 为 NULL 返回 false
 */
bool domain_activate(Domain *domain, SymbolicCoord *center) {
    if (!domain)
        return false;

    if (domain->center) {
        symbolic_coord_destroy(domain->center);
    }
    domain->center = symbolic_coord_copy(center);
    domain->active = true;
    domain->strength = MAGIC_DOMAIN_ACTIVATION_STRENGTH;

    return true;
}

/**
 * @brief 停用领域
 *
 * 将领域设为非激活状态，强度归零。
 *
 * @param domain 领域指针
 * @return 停用成功返回 true，domain 为 NULL 返回 false
 */
bool domain_deactivate(Domain *domain) {
    if (!domain)
        return false;
    domain->active = false;
    domain->strength = 0.0;
    return true;
}

/**
 * @brief 检查领域是否处于激活状态
 *
 * @param domain 领域指针
 * @return 激活返回 true，domain 为 NULL 返回 false
 */
bool domain_is_active(const Domain *domain) {
    return domain ? domain->active : false;
}

/**
 * @brief 获取领域强度
 *
 * @param domain 领域指针
 * @return 领域强度值，domain 为 NULL 时返回 0.0
 */
double domain_get_strength(const Domain *domain) {
    return domain ? domain->strength : 0.0;
}

/**
 * @brief 获取领域名称
 *
 * @param domain 领域指针
 * @return 领域名称字符串，domain 为 NULL 时返回 NULL
 */
const char *domain_get_name(const Domain *domain) {
    return domain ? domain->name : NULL;
}

/**
 * @brief 获取领域作用范围
 *
 * @param domain 领域指针
 * @return 领域作用范围值，domain 为 NULL 时返回 0
 */
int domain_get_range(const Domain *domain) {
    return domain ? domain->range : 0;
}

/**
 * @brief 获取领域中心坐标
 *
 * @param domain 领域指针
 * @return 领域中心坐标指针（所有权仍归领域），domain 为 NULL 或未设置时返回 NULL
 */
SymbolicCoord *domain_get_center(const Domain *domain) {
    return domain ? domain->center : NULL;
}

/* ============================================================
 * 辅助工具实现
 * ============================================================ */

/**
 * @brief 将魔法元素枚举值转换为中文字符串
 *
 * @param element 魔法元素类型
 * @return 元素的中文名称字符串，无效值时返回 "未知"
 * @warning 传入无效枚举值将触发边界检查并返回 "未知"
 */
const char *element_to_string(MagicElement element) {
    static const char *names[] = {"无属性", "火", "水", "风", "土", "以太"};
    /* 边界检查：防止数组越界 */
    if (element < 0 || element > ELEMENT_ETHER) {
        return "未知";
    }
    return names[element];
}

/**
 * @brief 将字符串转换为魔法元素枚举值
 *
 * 支持中文名称和英文名称两种格式。
 *
 * @param str 元素名称字符串（如 "FIRE"、"火"）
 * @return 对应的魔法元素类型，无法识别时返回 ELEMENT_NONE
 */
MagicElement string_to_element(const char *str) {
    if (!str)
        return ELEMENT_NONE;

    if (strcmp(str, "FIRE") == 0 || strcmp(str, "火") == 0)
        return ELEMENT_FIRE;
    if (strcmp(str, "WATER") == 0 || strcmp(str, "水") == 0)
        return ELEMENT_WATER;
    if (strcmp(str, "AIR") == 0 || strcmp(str, "风") == 0)
        return ELEMENT_AIR;
    if (strcmp(str, "EARTH") == 0 || strcmp(str, "土") == 0)
        return ELEMENT_EARTH;
    if (strcmp(str, "ETHER") == 0 || strcmp(str, "以太") == 0)
        return ELEMENT_ETHER;

    return ELEMENT_NONE;
}

/**
 * @brief 将施法阶段枚举值转换为中文字符串
 *
 * @param stage 施法阶段
 * @return 阶段的中文名称字符串，无效值时返回 "未知"
 * @warning 传入无效枚举值将触发边界检查并返回 "未知"
 */
const char *stage_to_string(SpellStage stage) {
    static const char *names[] = {"开模", "提纯", "灌注", "释放"};
    /* 边界检查：防止数组越界 */
    if (stage < 0 || stage > SPELL_STAGE_RELEASING) {
        return "未知";
    }
    return names[stage];
}

/**
 * @brief 将咒语状态枚举值转换为中文字符串
 *
 * @param status 咒语状态
 * @return 状态的中文名称字符串，无效值时返回 "未知"
 * @warning 传入无效枚举值将触发边界检查并返回 "未知"
 */
const char *status_to_string(SpellStatus status) {
    static const char *names[] = {"空闲", "施法中", "成功", "失败", "反噬"};
    /* 边界检查：防止数组越界 */
    if (status < 0 || status > SPELL_STATUS_BACKLASH) {
        return "未知";
    }
    return names[status];
}

/**
 * @brief 将元素反应枚举值转换为中文字符串
 *
 * @param reaction 元素反应类型
 * @return 反应类型的中文名称字符串，无效值时返回 "未知"
 * @warning 传入无效枚举值将触发边界检查并返回 "未知"
 */
const char *reaction_to_string(ElementReaction reaction) {
    static const char *names[] = {"无反应", "增强", "削弱", "冲突"};
    /* 边界检查：防止数组越界 */
    if (reaction < 0 || reaction > ELEMENT_REACTION_CONFLICT) {
        return "未知";
    }
    return names[reaction];
}

/**
 * @brief 将限制等级枚举值转换为中文字符串
 *
 * @param level 限制等级
 * @return 限制等级的中文名称字符串，无效值时返回 "未知"
 * @warning 传入无效枚举值将触发边界检查并返回 "未知"
 */
const char *restriction_to_string(RestrictionLevel level) {
    static const char *names[] = {"无限制", "限制级", "管制级", "禁术级", "绝对禁术"};
    /* 边界检查：防止数组越界 */
    if (level < 0 || level > RESTRICTION_ABSOLUTE) {
        return "未知";
    }
    return names[level];
}
