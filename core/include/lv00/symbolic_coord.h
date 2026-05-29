/* ========================================================================
 * 模块名称：符号坐标系统 (symbolic_coord)
 * 功能概述：提供四种坐标类型（有理数 Rational、代数数 Algebraic、
 *          二次扩张数 Quadratic、超越数 Transcendental）的表示与运算。
 *          包含创建、销毁、四则运算、序列化、比较、位电路溢出检测、
 *          信任颜色管理、A/B 计划切换机制和用户交互式跳闸回调。
 *
 * 主要 API：
 *   - symbolic_coord_create_rational / algebraic / ... — 创建符号坐标
 *   - symbolic_coord_add / subtract / multiply / divide — 四则运算
 *   - symbolic_coord_compare                          — 比较
 *   - symbolic_coord_serialize                        — 序列化
 *   - check_digit_circuit / circuit_handle_trip       — 位电路管理
 *   - symbolic_coord_downgrade_to_amber               — 信任降级
 *   - algebraic_get_plan / algebraic_set_plan         — A/B 计划切换
 *
 * 使用示例：
 *   SymbolicCoord *x = symbolic_coord_create_rational(3, 4);
 *   SymbolicCoord *y = symbolic_coord_create_transcendental("pi");
 *   SymbolicCoord *sum = symbolic_coord_add(x, y);
 *   CircuitStatus cs = check_digit_circuit(sum);
 *
 * ======================================================================== */

/**
 * @file symbolic_coord.h
 * @brief 符号坐标系统 —— 有理数、代数数、二次数、超越数的表示与运算
 */

#ifndef LV00_SYMBOLIC_COORD_H
#define LV00_SYMBOLIC_COORD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <gmp.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifndef LV00_PUBLIC_API
#define LV00_PUBLIC_API
#endif


/* LV00_PUBLIC_API 由 lv00.h 统一定义，此处不再重复。
 * 原因：lv00.h 中根据平台（Windows DLL / GCC/Clang visibility）和构建模式
 * （共享库 / 静态库）统一设置 LV00_PUBLIC_API。若各子模块头文件各自
 * #define LV00_PUBLIC_API 为空，在构建共享库时会导致符号不导出。
 * 因此要求使用者先包含 lv00.h，此处仅做守卫检查。 */
#ifndef LV00_PUBLIC_API
#error "请先包含 lv00.h 以获取 LV00_PUBLIC_API 定义"
#endif

/* MAX_MODULE_DEPTH —— 与 module.h 中的定义保持同步
 * 使用 #ifndef 守卫防止重复定义。若需修改此值，请同时修改
 * module.h 和所有引用此宏的 .c 文件。
 */
#ifndef MAX_MODULE_DEPTH
#define MAX_MODULE_DEPTH 32
#endif
#ifndef BIT_CUTOFF_THRESHOLD
#define BIT_CUTOFF_THRESHOLD 1000000
#endif
#ifndef MAX_PRECISION_BITS
#define MAX_PRECISION_BITS 100
#endif

#include "mpz_poly.h"

typedef enum {
    RATIONAL,      /* 有理数 */
    ALGEBRAIC,     /* 代数数 */
    QUADRATIC,     /* 二次扩张数 */
    TRANSCENDENTAL /* 超越数 */
} CoordType;

typedef enum {
    TRUST_GREEN,        /* 绿色：完全构造性 */
    TRUST_BLUE,         /* 蓝色：未确定 */
    TRUST_YELLOW,       /* 黄色：条件性 */
    TRUST_ORANGE,       /* 橙色：非构造性依赖 */
    TRUST_LIGHT_ORANGE, /* 浅橙色：oracle/爆炸原理 */
    TRUST_RED,          /* 红色：已证伪 */
    TRUST_AMBER         /* 橙黄色：含数值假设 */
} TrustColor;

typedef enum {
    LIGHT_ORANGE_ORACLE,   /* 浅橙色：oracle 依赖 */
    LIGHT_ORANGE_EXPLOSION /* 浅橙色：爆炸原理步骤 */
} LightOrangeSubtype;

/* Full struct definitions (not opaque) */
typedef struct Rational {
    mpq_t value;
} Rational;

typedef struct Algebraic {
    mpz_poly_t minimal_poly;
    double left_bound;
    double right_bound;
    int precision_bits;
    Rational *cached_rational;
} Algebraic;

typedef struct Quadratic {
    Rational *a;
    Rational *b;
    unsigned int n;
} Quadratic;

/* 超越数符号表达式类型（design_v2.9.md 第1.4节） */
typedef enum {
    TRANS_EXPR_CONSTANT,      /* 独立常量（pi, e） */
    TRANS_EXPR_ADD_RATIONAL,  /* 常量 + 有理数 */
    TRANS_EXPR_MUL_RATIONAL,  /* 常量 * 有理数 */
    TRANS_EXPR_ADD_ALGEBRAIC, /* 常量 + 代数数（标记为超出范围） */
    TRANS_EXPR_MUL_ALGEBRAIC  /* 常量 * 代数数（标记为超出范围） */
} TransExprType;

typedef struct TranscendentalExpr {
    TransExprType expr_type;
    char base_name[16];         /* 基础常量名（如 "pi", "e"），最大15字符 */
    Rational *rational_operand; /* For ADD/MUL_RATIONAL */
    bool out_of_scope;          /* True if combined with algebraic/quadratic */
} TranscendentalExpr;

typedef struct Transcendental {
    char name[64];            /* 常量名（如 "pi", "pi/2", "3*pi/4"），最大63字符 */
    TranscendentalExpr *expr; /* Symbolic expression tree (NULL for bare constants) */
} Transcendental;

typedef struct SymbolicCoord {
    CoordType type;
    union {
        Rational *rational;
        Algebraic *algebraic;
        Quadratic *quadratic;
        Transcendental *transcendental;
    } data;
    TrustColor trust;
    double cached_value;      /* 几何节点数值缓存 */
    bool cache_valid;         /* 缓存是否有效 */

    /* ============================================================
     * 版本控制字段 (v3.5.0: 自举支持)
     * ============================================================ */
    uint16_t version_major;         /**< 主版本号 */
    uint16_t version_minor;         /**< 次版本号 */
    uint16_t version_patch;         /**< 补丁版本号 */
} SymbolicCoord;

LV00_PUBLIC_API Rational *rational_create(int64_t numerator, uint64_t denominator);

/**
 * @brief 从 GMP 整数创建有理数
 *
 * @param[in] numerator   分子（GMP 多精度整数）
 * @param[in] denominator 分母（GMP 多精度整数）
 * @return 新创建的有理数指针，失败返回 NULL
 */
LV00_PUBLIC_API Rational *rational_create_from_mpq(const mpq_t value);

/**
 * @brief 创建有理数（从 GMP 整数创建）
 *
 * GMP 只读参数加 const 修饰，遵循 GMP 最佳实践。
 *
 * @param[in] numerator   分子（GMP 多精度整数，只读）
 * @param[in] denominator 分母（GMP 多精度整数，只读）
 * @return 新创建的有理数指针，失败返回 NULL
 */
LV00_PUBLIC_API Rational *rational_create_from_mpz(const mpz_t numerator, const mpz_t denominator);

/**
 * @brief 复制有理数
 * @param[in] src 源有理数
 * @return 复制后的新有理数指针，失败返回 NULL
 */
LV00_PUBLIC_API Rational *rational_copy(const Rational *src);

/**
 * @brief 销毁有理数
 * @param r 要销毁的有理数
 */
LV00_PUBLIC_API void rational_destroy(Rational *r);

/**
 * @brief 比较两个有理数
 * @param[in] a 第一个有理数
 * @param[in] b 第二个有理数
 * @return a<b 返回负数，a=b 返回0，a>b 返回正数
 */
LV00_PUBLIC_API int rational_compare(const Rational *a, const Rational *b);

/**
 * @brief 有理数加法
 * @param[in] a 加数
 * @param[in] b 加数
 * @return 结果有理数（调用者负责释放），失败返回 NULL
 */
LV00_PUBLIC_API Rational *rational_add(const Rational *a, const Rational *b);

/**
 * @brief 有理数减法
 * @param[in] a 被减数
 * @param[in] b 减数
 * @return 结果有理数（调用者负责释放），失败返回 NULL
 */
LV00_PUBLIC_API Rational *rational_subtract(const Rational *a, const Rational *b);

/**
 * @brief 有理数乘法
 * @param[in] a 乘数
 * @param[in] b 乘数
 * @return 结果有理数（调用者负责释放），失败返回 NULL
 */
LV00_PUBLIC_API Rational *rational_multiply(const Rational *a, const Rational *b);

/**
 * @brief 有理数除法
 * @param[in] a 被除数
 * @param[in] b 除数（不能为零）
 * @return 结果有理数（调用者负责释放），失败返回 NULL
 */
LV00_PUBLIC_API Rational *rational_divide(const Rational *a, const Rational *b);

/**
 * @brief 序列化有理数为字符串
 * @param[in] r 有理数
 * @return 序列化字符串（调用者负责 free），失败返回 NULL
 */
LV00_PUBLIC_API char *rational_serialize(const Rational *r);

/**
 * @brief 解析字符串创建有理数
 * @param[in] str 格式如 "3/4" 或 "1.5" 的字符串
 * @return 新创建的有理数指针，失败返回 NULL
 */
LV00_PUBLIC_API Rational *rational_parse(const char *str);

LV00_PUBLIC_API Algebraic *algebraic_create(mpz_poly_t *poly, double left, double right);

/**
 * @brief 销毁代数数
 * @param a 要销毁的代数数
 */
LV00_PUBLIC_API void algebraic_destroy(Algebraic *a);

/**
 * @brief 比较两个代数数
 * @param[in] a 第一个代数数
 * @param[in] b 第二个代数数
 * @return a<b 返回负数，a=b 返回0，a>b 返回正数
 */
LV00_PUBLIC_API int algebraic_compare(const Algebraic *a, const Algebraic *b);

/**
 * @brief 代数数加法
 * @param[in] a 加数
 * @param[in] b 加数
 * @return 结果代数数（调用者负责释放），失败返回 NULL
 */
LV00_PUBLIC_API Algebraic *algebraic_add(const Algebraic *a, const Algebraic *b);

/**
 * @brief 代数数减法
 * @param[in] a 被减数
 * @param[in] b 减数
 * @return 结果代数数（调用者负责释放），失败返回 NULL
 */
LV00_PUBLIC_API Algebraic *algebraic_subtract(const Algebraic *a, const Algebraic *b);

/**
 * @brief 代数数乘法
 * @param[in] a 乘数
 * @param[in] b 乘数
 * @return 结果代数数（调用者负责释放），失败返回 NULL
 */
LV00_PUBLIC_API Algebraic *algebraic_multiply(const Algebraic *a, const Algebraic *b);

/**
 * @brief 代数数除法
 * @param[in] a 被除数
 * @param[in] b 除数
 * @return 结果代数数（调用者负责释放），失败返回 NULL
 */
LV00_PUBLIC_API Algebraic *algebraic_divide(const Algebraic *a, const Algebraic *b);

/**
 * @brief 序列化代数数为字符串
 * @param[in] a 代数数
 * @return 序列化字符串（调用者负责 free），失败返回 NULL
 */
LV00_PUBLIC_API char *algebraic_serialize(const Algebraic *a);

LV00_PUBLIC_API Quadratic *quadratic_create(Rational *a, Rational *b, unsigned int n);

/**
 * @brief 销毁二次扩张数
 * @param q 要销毁的二次扩张数
 */
LV00_PUBLIC_API void quadratic_destroy(Quadratic *q);

/**
 * @brief 比较两个二次扩张数
 * @param[in] a 第一个二次扩张数
 * @param[in] b 第二个二次扩张数
 * @return a<b 返回负数，a=b 返回0，a>b 返回正数
 */
LV00_PUBLIC_API int quadratic_compare(const Quadratic *a, const Quadratic *b);

/**
 * @brief 二次扩张数加法
 * @param[in] a 加数
 * @param[in] b 加数
 * @return 结果二次扩张数（调用者负责释放），失败返回 NULL
 */
LV00_PUBLIC_API Quadratic *quadratic_add(const Quadratic *a, const Quadratic *b);

/**
 * @brief 二次扩张数减法
 * @param[in] a 被减数
 * @param[in] b 减数
 * @return 结果二次扩张数（调用者负责释放），失败返回 NULL
 */
LV00_PUBLIC_API Quadratic *quadratic_subtract(const Quadratic *a, const Quadratic *b);

/**
 * @brief 二次扩张数乘法
 * @param[in] a 乘数
 * @param[in] b 乘数
 * @return 结果二次扩张数（调用者负责释放），失败返回 NULL
 */
LV00_PUBLIC_API Quadratic *quadratic_multiply(const Quadratic *a, const Quadratic *b);

/**
 * @brief 二次扩张数除法
 * @param[in] a 被除数
 * @param[in] b 除数
 * @return 结果二次扩张数（调用者负责释放），失败返回 NULL
 */
LV00_PUBLIC_API Quadratic *quadratic_divide(const Quadratic *a, const Quadratic *b);

/**
 * @brief 序列化二次扩张数为字符串
 * @param[in] q 二次扩张数
 * @return 序列化字符串（调用者负责 free），失败返回 NULL
 */
LV00_PUBLIC_API char *quadratic_serialize(const Quadratic *q);

/**
 * @brief 创建超越数
 * @param[in] name 超越数名称（如 "pi"、"e"）
 * @return 新创建的超越数指针，失败返回 NULL
 */
LV00_PUBLIC_API Transcendental *transcendental_create(const char *name);

/**
 * @brief 销毁超越数
 * @param t 要销毁的超越数
 */
LV00_PUBLIC_API void transcendental_destroy(Transcendental *t);

/**
 * @brief 比较两个超越数
 * @param[in] a 第一个超越数
 * @param[in] b 第二个超越数
 * @return a<b 返回负数，a=b 返回0，a>b 返回正数
 */
LV00_PUBLIC_API int transcendental_compare(const Transcendental *a, const Transcendental *b);

/**
 * @brief 序列化超越数为字符串
 * @param[in] t 超越数
 * @return 序列化字符串（调用者负责 free），失败返回 NULL
 */
LV00_PUBLIC_API char *transcendental_serialize(const Transcendental *t);

/**
 * @brief 创建有理数型符号坐标
 * @param[in] num 分子
 * @param[in] denom 分母
 * @return 新创建的符号坐标（调用者负责释放），失败返回 NULL
 */
LV00_PUBLIC_API SymbolicCoord *symbolic_coord_create_rational(int64_t num, uint64_t denom);

/**
 * @brief 创建代数数型符号坐标
 * @param[in] poly 最小多项式
 * @param[in] left 区间左边界
 * @param[in] right 区间右边界
 * @return 新创建的符号坐标（调用者负责释放），失败返回 NULL
 */
LV00_PUBLIC_API SymbolicCoord *symbolic_coord_create_algebraic(mpz_poly_t *poly, double left, double right);

/**
 * @brief 创建二次扩张型符号坐标
 * @param[in] a 系数 a
 * @param[in] b 系数 b
 * @param[in] n 扩张次数
 * @return 新创建的符号坐标（调用者负责释放），失败返回 NULL
 */
LV00_PUBLIC_API SymbolicCoord *symbolic_coord_create_quadratic(Rational *a, Rational *b, unsigned int n);

/**
 * @brief 创建超越数型符号坐标
 * @param[in] name 超越数名称（如 "pi"、"e"）
 * @return 新创建的符号坐标（调用者负责释放），失败返回 NULL
 */
LV00_PUBLIC_API SymbolicCoord *symbolic_coord_create_transcendental(const char *name);

/**
 * @brief 销毁符号坐标
 * @param coord 要销毁的符号坐标
 */
LV00_PUBLIC_API void symbolic_coord_destroy(SymbolicCoord *coord);

/**
 * @brief 比较两个符号坐标
 * @param[in] a 第一个符号坐标
 * @param[in] b 第二个符号坐标
 * @return a<b 返回负数，a=b 返回0，a>b 返回正数
 */
LV00_PUBLIC_API int symbolic_coord_compare(const SymbolicCoord *a, const SymbolicCoord *b);

/**
 * @brief 符号坐标加法
 * @param[in] a 加数
 * @param[in] b 加数
 * @return 结果符号坐标（调用者负责释放），失败返回 NULL
 */
LV00_PUBLIC_API SymbolicCoord *symbolic_coord_add(const SymbolicCoord *a, const SymbolicCoord *b);

/**
 * @brief 符号坐标减法
 * @param[in] a 被减数
 * @param[in] b 减数
 * @return 结果符号坐标（调用者负责释放），失败返回 NULL
 */
LV00_PUBLIC_API SymbolicCoord *symbolic_coord_subtract(const SymbolicCoord *a, const SymbolicCoord *b);

/**
 * @brief 符号坐标乘法
 * @param[in] a 乘数
 * @param[in] b 乘数
 * @return 结果符号坐标（调用者负责释放），失败返回 NULL
 */
LV00_PUBLIC_API SymbolicCoord *symbolic_coord_multiply(const SymbolicCoord *a, const SymbolicCoord *b);

/**
 * @brief 符号坐标除法
 * @param[in] a 被除数
 * @param[in] b 除数
 * @return 结果符号坐标（调用者负责释放），失败返回 NULL
 */
LV00_PUBLIC_API SymbolicCoord *symbolic_coord_divide(const SymbolicCoord *a, const SymbolicCoord *b);

/**
 * @brief 序列化符号坐标为字符串
 * @param[in] coord 符号坐标
 * @return 序列化字符串（调用者负责 free），失败返回 NULL
 */
LV00_PUBLIC_API char *symbolic_coord_serialize(const SymbolicCoord *coord);

/**
 * @brief 将符号坐标转换为双精度浮点数
 * @param[in] coord 符号坐标
 * @return 双精度浮点数值
 */
LV00_PUBLIC_API double symbolic_coord_to_double(const SymbolicCoord *coord);

/**
 * @brief 使符号坐标的数值缓存失效
 *
 * 当坐标被修改（如算术运算、类型转换、信任颜色变更）时，
 * 调用此函数清除缓存的数值近似值，确保下次调用
 * symbolic_coord_to_double() 时重新计算。
 *
 * @param[in,out] coord 符号坐标（可为 NULL，空操作）
 */
LV00_PUBLIC_API void symbolic_coord_invalidate_cache(SymbolicCoord *coord);

/**
 * @brief 位电路状态枚举
 *
 * 用于位电路（位数熔断）系统的状态管理。
 */
typedef enum {
    CIRCUIT_STATUS_OK,     /**< 正常：数值未溢出 */
    CIRCUIT_STATUS_WARNED, /**< 警告：数值接近溢出阈值 */
    CIRCUIT_STATUS_TRIPPED /**< 跳闸：数值溢出，触发熔断 */
} CircuitStatus;

/* 位电路（digit cutoff）函数 - Section 1.5 of design_v2.9.md */

/**
 * @brief 检查符号坐标是否触发位电路
 * @param[in] coord 符号坐标
 * @return 位电路状态
 */
LV00_PUBLIC_API CircuitStatus check_digit_circuit(const SymbolicCoord *coord);

/**
 * @brief 处理位电路溢出
 */
LV00_PUBLIC_API void circuit_handle_overflow(void);

/**
 * @brief 重置位电路上下文
 */
LV00_PUBLIC_API void circuit_reset_context(void);

/**
 * @brief 设置冻结点快照
 * @param snapshot 快照数据
 */
LV00_PUBLIC_API void circuit_set_frozen_point(void *snapshot);

/**
 * @brief 获取溢出计数
 * @return 溢出次数
 */
LV00_PUBLIC_API int circuit_get_overflow_count(void);

/* 信任颜色转换（用于溢出处理） */

/**
 * @brief 将符号坐标降级为 AMBER 信任级别
 * @param[in] coord 符号坐标
 * @param[in] precision 精度
 * @param[in] declaration 数值假设声明
 * @return 降级后的符号坐标（调用者负责释放）
 */
LV00_PUBLIC_API SymbolicCoord *symbolic_coord_downgrade_to_amber(const SymbolicCoord *coord, double precision, const char *declaration);

/**
 * @brief 检查符号坐标是否为 AMBER 信任级别
 * @param[in] coord 符号坐标
 * @return true 如果为 AMBER，否则 false
 */
LV00_PUBLIC_API bool symbolic_coord_is_amber(const SymbolicCoord *coord);

/**
 * @brief 获取符号坐标的信任颜色
 * @param[in] coord 符号坐标
 * @return 信任颜色
 */
LV00_PUBLIC_API TrustColor symbolic_coord_get_trust(const SymbolicCoord *coord);

/**
 * @brief 设置符号坐标的信任颜色
 * @param[in,out] coord 符号坐标
 * @param[in] trust 信任颜色
 */
LV00_PUBLIC_API void symbolic_coord_set_trust(SymbolicCoord *coord, TrustColor trust);

/* 工具函数 */

/**
 * @brief 复制符号坐标
 * @param[in] src 源符号坐标
 * @return 复制后的新符号坐标（调用者负责释放），失败返回 NULL
 */
LV00_PUBLIC_API SymbolicCoord *symbolic_coord_copy(const SymbolicCoord *src);

/**
 * @brief 检查符号坐标是否为零
 * @param[in] coord 符号坐标
 * @return true 如果为零，否则 false
 */
LV00_PUBLIC_API bool symbolic_coord_is_zero(const SymbolicCoord *coord);

/**
 * @brief 检查符号坐标是否为正数
 * @param[in] coord 符号坐标
 * @return true 如果为正数，否则 false
 */
LV00_PUBLIC_API bool symbolic_coord_is_positive(const SymbolicCoord *coord);

/**
 * @brief 检查符号坐标是否为负数
 * @param[in] coord 符号坐标
 * @return true 如果为负数，否则 false
 */
LV00_PUBLIC_API bool symbolic_coord_is_negative(const SymbolicCoord *coord);

/**
 * @brief 取符号坐标的相反数
 * @param[in] coord 符号坐标
 * @return 结果符号坐标（调用者负责释放），失败返回 NULL
 */
LV00_PUBLIC_API SymbolicCoord *symbolic_coord_negate(const SymbolicCoord *coord);

/**
 * @brief 符号坐标的幂运算
 * @param[in] base 底数
 * @param[in] exponent 指数
 * @return 结果符号坐标（调用者负责释放），失败返回 NULL
 */
LV00_PUBLIC_API SymbolicCoord *symbolic_coord_pow(const SymbolicCoord *base, unsigned int exponent);

/**
 * @brief 符号坐标的平方根
 * @param[in] coord 符号坐标
 * @return 结果符号坐标（调用者负责释放），失败返回 NULL
 */
LV00_PUBLIC_API SymbolicCoord *symbolic_coord_sqrt(const SymbolicCoord *coord);

/**
 * @brief 计算符号坐标的哈希值
 * @param[in] coord 符号坐标
 * @return 哈希值
 */
LV00_PUBLIC_API uint64_t symbolic_coord_hash(const SymbolicCoord *coord);

/* 增强优先级有理化 (Section 1.2 of design_v2.9.md) */
/* 使用连分数近似实现更好的有理化 */

/**
 * @brief 尝试将代数数有理化
 * @param[in,out] a 代数数
 * @return true 成功有理化，false 无法有理化
 */
LV00_PUBLIC_API bool algebraic_try_rationalize(Algebraic *a);

/* 代数数相等性的惰性精度细化 (Section 1.2) */
/* 倍增精度直到两个代数数可区分 */

/**
 * @brief 细化代数数以判断相等性
 * @param[in,out] a 代数数
 * @param[in,out] b 代数数
 * @param[in] max_iterations 最大迭代次数
 * @return 0 相等，-1 不等，其他值表示无法确定
 */
LV00_PUBLIC_API int algebraic_refine_for_equality(Algebraic *a, Algebraic *b, int max_iterations);

/* 嵌套平方根展开检查 (Section 1.2) */
/* 检查 sqrt(a + b*sqrt(n)) 是否可展开为二次形式 */

/**
 * @brief 尝试展开嵌套平方根
 * @param[in] coord 符号坐标
 * @return 展开后的符号坐标（调用者负责释放），失败返回 NULL
 */
LV00_PUBLIC_API SymbolicCoord *symbolic_coord_try_expand_nested_sqrt(const SymbolicCoord *coord);

/* 位电路上下文管理 */

/**
 * @brief 设置位电路上下文
 * @param[in,out] result 结果坐标
 * @param[in] operation 操作名称
 * @param[in] left_type 左操作数类型
 * @param[in] right_type 右操作数类型
 */
LV00_PUBLIC_API void circuit_set_context(SymbolicCoord *result, const char *operation, CoordType left_type, CoordType right_type);

/**
 * @brief 获取上一次的位电路结果
 * @return 上一次的符号坐标结果
 */
LV00_PUBLIC_API SymbolicCoord *circuit_get_last_result(void);

/**
 * @brief 获取上一次的位电路操作名称
 * @return 操作名称字符串
 */
LV00_PUBLIC_API const char *circuit_get_last_operation(void);

/**
 * @brief 检查是否存在冻结点
 * @return true 存在冻结点，false 不存在
 */
LV00_PUBLIC_API bool circuit_has_frozen_point(void);

/**
 * @brief 获取冻结点
 * @return 冻结点快照数据
 */
LV00_PUBLIC_API void *circuit_get_frozen_point(void);

/* Global overflow context (defined in symbolic_coord.c) */
struct OverflowContext {
    SymbolicCoord *last_result;
    const char *last_operation;
    CoordType left_type;
    CoordType right_type;
    int overflow_count;
    void *frozen_point;
    bool has_frozen_point;
};

/* ============================================================
 * A/B Plan switching (Section 1.6 of design_v2.9.md)
 * ============================================================ */

/**
 * @brief 代数计划枚举
 *
 * 用于控制代数数的处理策略。
 */
typedef enum {
    PLAN_A_FULL_ALGEBRAIC, /**< 完整代数数支持 */
    PLAN_B_QUADRATIC_ONLY  /**< 仅二次扩张，高次降级处理 */
} AlgebraicPlan;

/**
 * @brief 获取当前代数计划
 * @return 当前代数计划
 */
LV00_PUBLIC_API AlgebraicPlan algebraic_get_plan(void);

/**
 * @brief 设置代数计划
 * @param[in] plan 要设置的代数计划
 */
LV00_PUBLIC_API void algebraic_set_plan(AlgebraicPlan plan);

/* 代数数压力测试结果结构体 */

/**
 * @brief 代数数压力测试结果
 *
 * 用于记录代数数压力测试的各项指标。
 */
typedef struct {
    bool precision_stable;   /**< 精度衰减是否稳定（100次操作后 <=1 bit） */
    bool performance_stable; /**< 性能是否稳定（100次操作后最大位数 <= 10^6） */
    int max_precision_decay; /**< 观察到的最大精度衰减 */
    int max_bits_observed;   /**< 观察到的最大位数 */
} StressTestResult;

/**
 * @brief 运行代数数压力测试
 *
 * 验证 A 计划在给定链长度和多项式度数下的稳定性。
 *
 * @param[in] chain_length 链长度
 * @param[in] max_poly_degree 最大多项式度数
 * @return 压力测试结果
 */
LV00_PUBLIC_API StressTestResult algebraic_stress_test(int chain_length, int max_poly_degree);

/* ============================================================
 * 位电路用户交互机制 (Section 1.5 of design_v2.9.md)
 * ============================================================ */

/* 位电路跳闸时的用户响应类型 */

/**
 * @brief 位电路跳闸用户响应类型
 */
typedef enum {
    CIRCUIT_RESPONSE_IGNORE,   /**< 忽略：接受为数值辅助 */
    CIRCUIT_RESPONSE_ROLLBACK, /**< 回滚：恢复到冻结点 */
    CIRCUIT_RESPONSE_DOWNGRADE /**< 降级：永久降级为 AMBER */
} CircuitResponse;

/**
 * @brief 位电路跳闸回调函数类型
 *
 * 当检测到位电路跳闸时调用此回调，让用户选择处理方式。
 *
 * @param[in] coord           触发跳闸的符号坐标
 * @param[in] overflow_count  跳闸次数
 * @param[in] user_data       用户数据
 * @return 用户选择的响应类型
 */
typedef CircuitResponse (*CircuitTripCallback)(const SymbolicCoord *coord, /**< 触发跳闸的符号坐标 */
                                               int overflow_count,         /**< 跳闸次数 */
                                               void *user_data             /**< 用户数据 */
);

/**
 * @brief 设置位电路跳闸回调函数
 * @param[in] cb 回调函数（为 NULL 则取消回调）
 * @param[in] user_data 用户数据
 */
LV00_PUBLIC_API void circuit_set_trip_callback(CircuitTripCallback cb, void *user_data);

/**
 * @brief 处理位电路跳闸（带用户交互）
 * @param[in] coord 触发跳闸的符号坐标
 * @return 用户选择的响应类型
 */
LV00_PUBLIC_API CircuitResponse circuit_handle_trip_interactive(const SymbolicCoord *coord);

#ifdef __cplusplus
}
#endif

#endif /* LV00_SYMBOLIC_COORD_H */
