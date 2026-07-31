/**
 * @file magic_rune.c
 * @brief 符文系统与符文序列实现
 *
 * @details 从 magic.c 拆分的子模块（Lv-00 项目 v3.3.0+）。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "magic_internal.h"
#include "magic.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_json.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_strbuf.h"

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
    Rune *rune = (Rune *) lv_calloc(1, sizeof(Rune));
    if (!rune)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "rune_create_rational: lv_calloc failed");

    /* 创建有理数类型的符号坐标 */
    rune->coord = symbolic_coord_create_rational(num, denom);
    if (!rune->coord) {
        lv_free((void **) &rune);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "rune_create_rational: symbolic_coord_create_rational failed");
    }

    /* 初始化符文属性：元素类型、名称、符号和威力等级 */
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
    Rune *rune = (Rune *) lv_calloc(1, sizeof(Rune));
    if (!rune)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "rune_create_algebraic: lv_calloc failed");

    /* 使用连分数近似创建代数数 */
    mpz_poly_t poly;
    poly.degree = MAGIC_POLY_DEGREE_QUADRATIC;
    poly.coeffs = (mpz_t *) lv_malloc(MAGIC_POLY_COEFF_COUNT * sizeof(mpz_t));
    if (!poly.coeffs) {
        lv_free((void **) &rune);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "rune_create_algebraic: poly.coeffs malloc failed");
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
    rune->coord = symbolic_coord_create_algebraic(&poly, value - tolerance, value + tolerance);
    if (!rune->coord) {
        /* 错误路径：必须先清理 mpz_t 内部状态，再释放数组内存 */
        for (int i = 0; i < MAGIC_POLY_COEFF_COUNT; i++) {
            mpz_clear(poly.coeffs[i]);
        }
        lv_free((void **) &poly.coeffs);
        lv_free((void **) &rune);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "rune_create_algebraic: symbolic_coord_create_algebraic failed");
    }

    /* 成功路径：同样需要先清理 mpz_t 内部状态 */
    for (int i = 0; i < MAGIC_POLY_COEFF_COUNT; i++) {
        mpz_clear(poly.coeffs[i]);
    }
    lv_free((void **) &poly.coeffs);
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
    Rune *rune = (Rune *) lv_calloc(1, sizeof(Rune));
    if (!rune)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "rune_create_transcendental: lv_calloc failed");

    /* 创建超越数类型的符号坐标（如 pi, e 等） */
    rune->coord = symbolic_coord_create_transcendental(name);
    if (!rune->coord) {
        lv_free((void **) &rune);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "rune_create_transcendental: symbolic_coord_create_transcendental failed");
    }

    /* 初始化符文属性 */
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
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "rune_copy: src is NULL");

    Rune *rune = (Rune *) lv_malloc(sizeof(Rune));
    if (!rune)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "rune_copy: lv_malloc failed");

    /* 深拷贝符号坐标 */
    rune->coord = symbolic_coord_copy(src->coord);
    if (!rune->coord) {
        lv_free((void **) &rune);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "rune_copy: symbolic_coord_copy failed");
    }
    /* 拷贝标量属性 */
    rune->element = src->element;
    rune->power_level = src->power_level;

    /* 深拷贝可空字符串字段：name */
    if (src->name) {
        rune->name = lv_strdup_safe(src->name);
    } else {
        rune->name = NULL;
    }

    /* 深拷贝可空字符串字段：symbol */
    if (src->symbol) {
        rune->symbol = lv_strdup_safe(src->symbol);
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

    /* 逆序释放：先释放内部子对象，再释放自身 */
    if (rune->coord) {
        symbolic_coord_destroy(rune->coord);
    }
    if (rune->name) {
        lv_free((void **) &rune->name);
    }
    if (rune->symbol) {
        lv_free((void **) &rune->symbol);
    }
    lv_free((void **) &rune);
}

/**
 * @brief 将符文序列化为 JSON 格式字符串（动态分配版本）
 *
 * 将符文的元素类型、威力等级和符号坐标序列化为紧凑的 JSON 字符串。
 * 返回的字符串由调用者负责释放（使用 lv_free）。
 *
 * **线程安全保证：**
 * 本函数使用 lv_asprintf 动态分配缓冲区，每次调用返回独立的堆内存，
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
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "rune_serialize: coord_serialize failed");

    /* 使用 lv_asprintf 动态分配缓冲区，避免静态缓冲区的线程安全问题。
     * 不使用 static char buf[N] 模式，确保并发调用时不会互相覆盖。 */
    char *result =
        lv_asprintf("{\"element\":%d,\"power\":%d,\"coord\":%s}", rune->element, rune->power_level, coord_str);

    /* 释放 coord_str，无论 result 是否成功都需要释放 */
    lv_free((void **) &coord_str);
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
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "rune_serialize_to_buffer: invalid parameters");

    /* 序列化符号坐标为 JSON 子串 */
    char *coord_str = symbolic_coord_serialize(rune->coord);
    if (!coord_str)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "rune_serialize_to_buffer: serialize failed");

    /* 将符文信息写入调用者提供的缓冲区 */
    int written = snprintf(buf, (size_t) buf_size, "{\"element\":%d,\"power\":%d,\"coord\":%s}", rune->element,
                           rune->power_level, coord_str);

    lv_free((void **) &coord_str);

    if (written < 0)
        lv_RETURN_ERROR(lv_ERROR_IO, "rune_serialize_to_buffer: snprintf failed");
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
    if (lv_str_is_empty(str)) {
        lv_LOG_WARNING("rune_parse: 输入字符串为空");
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
        lv_LOG_WARNING("rune_parse: 无法解析代数数值 '%s'", value_start);
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
            lv_LOG_WARNING("rune_parse: 分子过长");
            return NULL;
        }
        /* [Bug修复] strncpy + 手动终止 → lv_strlcpy，更安全简洁 */
        lv_strlcpy(num_buf, num_start, num_len + 1);
        numerator = strtoll(num_buf, NULL, 10);

        /* 解析分母 */
        char denom_buf[64];
        size_t denom_len = (size_t) (num_end - slash - 1);
        if (denom_len >= sizeof(denom_buf)) {
            lv_LOG_WARNING("rune_parse: 分母过长");
            return NULL;
        }
        /* [Bug修复] strncpy + 手动终止 → lv_strlcpy，更安全简洁 */
        lv_strlcpy(denom_buf, slash + 1, denom_len + 1);
        denominator = strtoull(denom_buf, NULL, 10);

        if (denominator == 0) {
            lv_LOG_WARNING("rune_parse: 分母不能为零");
            return NULL;
        }
    } else {
        /* 整数格式 */
        char num_buf[64];
        size_t num_len = (size_t) (num_end - num_start);
        if (num_len >= sizeof(num_buf)) {
            lv_LOG_WARNING("rune_parse: 数值过长");
            return NULL;
        }
        /* [Bug修复] strncpy + 手动终止 → lv_strlcpy，更安全简洁 */
        lv_strlcpy(num_buf, num_start, num_len + 1);
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
    RuneSequence *seq = (RuneSequence *) lv_malloc(sizeof(RuneSequence));
    if (!seq)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "rune_sequence_create: lv_malloc failed");

    /* 初始化动态数组，预分配初始容量 */
    seq->capacity = MAGIC_RUNE_SEQUENCE_INIT_CAP;
    seq->rune_count = 0;
    seq->runes = (Rune **) lv_malloc(seq->capacity * sizeof(Rune *));

    if (!seq->runes) {
        lv_free((void **) &seq);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "rune_sequence_create: runes malloc failed");
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

    /* 容量不足时自动扩容 */
    if (!lv_ensure_capacity((void **)&seq->runes, seq->rune_count, &seq->capacity, sizeof(Rune *), 1))
        return false;

    /* 追加符文到序列尾部 */
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

    /* 销毁序列中的每个符文（序列拥有所有权） */
    for (int i = 0; i < seq->rune_count; i++) {
        rune_destroy(seq->runes[i]);
    }
    /* 释放动态数组和结构体本身 */
    lv_free((void **) &seq->runes);
    lv_free((void **) &seq);
}

