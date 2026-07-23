/**
 * @file groebner_engine.h
 * @brief Groebner 基计算引擎 —— 借鉴 Singular/Macaulay2 的多项式理想与 Gröbner 基计算
 *
 * @details 设计借鉴：
 * - Singular (singular.uni-kl.de)
 *   - "先声明环，再在环上做计算"的范式
 *   - 多环共存与对象归属：每个多项式都"知道"自己属于哪个环
 *   - 工业级 Gröbner 基实现（F4/F5 算法）
 *   - 理想操作：交、商、饱和、消除
 * - Macaulay2 (macaulay2.com)
 *   - 理想和簇（Variety）的统一视角
 *   - 单项式序的精确控制
 *   - 自由分辨率、正则序列等高级同调代数工具
 *
 * 设计目标：为 Lv-00 提供多项式方程组求解的代数引擎，
 *          将构造图约束转化为多项式理想，通过 Gröbner 基消去求解。
 *
 * @version 1.1.0
 * @date 2026-05-24
 */
#ifndef lv_GROEBNER_ENGINE_H
#define lv_GROEBNER_ENGINE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "constraint_graph.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================================================
 *  前向声明
 * ================================================================ */
typedef struct lvPolynomialRing lvPolynomialRing;
typedef struct lvPolynomial lvPolynomial;
typedef struct lvIdeal lvIdeal;
typedef struct lvGroebnerBasis lvGroebnerBasis;
typedef struct lvVariety lvVariety;
typedef struct lvRingRegistry lvRingRegistry;
typedef struct ConstraintGraph ConstraintGraph;
/* ================================================================
 *  第一部分：系数域与单项式序枚举
 * ================================================================ */
/**
 * @brief 系数域类型枚举
 *
 * 定义多项式环的系数所属的域（或环）：
 * - RING_FIELD_RATIONAL：有理数域 Q，最常用，精确计算
 * - RING_FIELD_REAL：实数域 R（通常以浮点近似）
 * - RING_FIELD_COMPLEX：复数域 C
 * - RING_FIELD_FINITE：有限域 GF(p)，p 为素数
 * - RING_FIELD_INTEGER：整数环 Z（非域，但可作为系数环）
 */
typedef enum {
    RING_FIELD_RATIONAL = 0, /**< 有理数域 Q */
    RING_FIELD_REAL = 1,     /**< 实数域 R */
    RING_FIELD_COMPLEX = 2,  /**< 复数域 C */
    RING_FIELD_FINITE = 3,   /**< 有限域 GF(p) */
    RING_FIELD_INTEGER = 4   /**< 整数环 Z（非域） */
} lvRingFieldType;
/**
 * @brief 单项式序类型枚举
 *
 * 定义多项式环中单项式的大小关系，直接影响 Gröbner 基计算：
 * - MONOMIAL_LEX：纯字典序（lex），适合消去理论
 * - MONOMIAL_GRLEX：分次字典序（grlex），先比较总次数再字典序
 * - MONOMIAL_GREVLEX：分次反字典序（grevlex），最常用，通常效率最优
 * - MONOMIAL_ELIM：消去序（elimination order），指定变量优先消去
 * - MONOMIAL_WEIGHT：权重序，用户自定义各变量权重
 */
typedef enum {
    MONOMIAL_LEX = 0,     /**< 纯字典序（lexicographic） */
    MONOMIAL_GRLEX = 1,   /**< 分次字典序（graded lex） */
    MONOMIAL_GREVLEX = 2, /**< 分次反字典序（graded reverse lex，默认推荐） */
    MONOMIAL_ELIM = 3,    /**< 消去序（elimination order） */
    MONOMIAL_WEIGHT = 4   /**< 权重序（用户自定义权重向量） */
} lvMonomialOrder;
/* ================================================================
 *  第二部分：多项式环（Polynomial Ring）
 * ================================================================ */
/**
 * @brief 多项式环结构体
 *
 * 借鉴 Singular 的环设计：先声明环，再在环上进行一切多项式运算。
 * 环包含了变量名、系数域、单项式序等全部元信息。
 *
 * 典型使用模式：
 * - ring r = ring_create(3, vars, RING_FIELD_RATIONAL, MONOMIAL_GREVLEX);
 * - lvPolynomial *f = poly_create(r, ...);
 * - 所有 f 上的操作都发生在 r 的上下文中
 */
struct lvPolynomialRing {
    int ring_id;             /**< 环的唯一标识符 */
    char **var_names;        /**< 变量名字符串数组 */
    int var_count;           /**< 变量数量 */
    lvRingFieldType field; /**< 系数域类型 */
    lvMonomialOrder order; /**< 单项式序 */
    int *elim_vars;          /**< 消去序指定的优先消去变量索引（仅 MONOMIAL_ELIM） */
    int elim_var_count;      /**< 消去变量数量 */
    double *weights;         /**< 权重向量（仅 MONOMIAL_WEIGHT） */
    int finite_field_char;   /**< 有限域特征 p（仅 RING_FIELD_FINITE，0 表示不适用） */
    char *label;             /**< 环的标签（人类可读，可为 NULL） */
    bool is_commutative;     /**< 是否为交换环（当前版本固定为 true） */
};
/* ================================================================
 *  第三部分：多项式（Polynomial）
 * ================================================================ */
/**
 * @brief 多项式结构体
 *
 * 表示多项式环中的一个多项式元素。
 * 多项式以稀疏表示法存储：terms 数组存储每个单项式的系数和幂次。
 *
 * 项表示：terms[i] = { monomial: 变量幂次向量 [e_1,...,e_n], coefficient: 系数 }
 * 例如：3*x^2*y 表示为 monomial = [2, 1], coefficient = 3
 *
 * 每个多项式对象深知自己所属的环，操作时保证环一致性。
 */
struct lvPolynomial {
    int poly_id;         /**< 多项式唯一标识符 */
    int ring_id;         /**< 所属环 ID */
    int *powers;         /**< 幂次扁平数组：powers[i * var_count + j] 表示第 i 项第 j 变量的幂 */
    void *coeffs;        /**< 系数数组（类型依系数域而定：double* 或 mpq_t* 或 mpz_t*） */
    int term_count;      /**< 项数量 */
    int term_capacity;   /**< 项数组容量 */
    int total_degree;    /**< 多项式总次数（单项式次数最大值） */
    bool is_homogeneous; /**< 是否为齐次多项式 */
    char *label;         /**< 多项式标签（可为 NULL） */
};
/* ================================================================
 *  第四部分：理想（Ideal）、Gröbner 基与算法枚举
 * ================================================================ */
/**
 * @brief Gröbner 基算法枚举
 *
 * 支持从经典 Buchberger 到现代 F4/F5 的多种算法：
 * - GROEBNER_BUCHBERGER：经典 Buchberger 算法（教学/验证用）
 * - GROEBNER_F4：Faugere F4 算法（基于线性代数的矩阵方法，工业标准）
 * - GROEBNER_F5：Faugere F5 算法（基于签名的增量算法，避免零约化）
 * - GROEBNER_SIGNATURE：基于签名的 Gröbner 基（签名基，GVW 等变体）
 * - GROEBNER_AUTO：自动选择最优算法（默认）
 */
typedef enum {
    GROEBNER_BUCHBERGER = 0, /**< 经典 Buchberger 算法 */
    GROEBNER_F4 = 1,         /**< Faugere F4 算法（矩阵化） */
    GROEBNER_F5 = 2,         /**< Faugere F5 算法（签名基） */
    GROEBNER_SIGNATURE = 3,  /**< 基于签名的 Gröbner 基（GVW 等） */
    GROEBNER_AUTO = 4        /**< 自动选择最优算法（默认） */
} lvGroebnerAlgorithm;
/**
 * @brief 理想结构体
 *
 * 由一个或多个生成元多项式定义的多项式理想 I = <f_1, ..., f_k>。
 * 理想缓存了其 Gröbner 基（惰性计算），避免重复计算。
 *
 * 理想操作是 Gröbner 基理论的核心：
 * - 理想成员判定：f in I ?
 * - 理想交/商：I ∩ J, I : J
 * - 根理想判定
 */
struct lvIdeal {
    int ideal_id;                    /**< 理想唯一标识符 */
    int ring_id;                     /**< 所属环 ID */
    lvPolynomial **generators;     /**< 生成元多项式数组 */
    int generator_count;             /**< 生成元数量 */
    int generator_capacity;          /**< 生成元数组容量 */
    lvGroebnerBasis *cached_basis; /**< 缓存的 Gröbner 基（惰性计算，可为 NULL） */
    bool basis_valid;                /**< 缓存基是否有效 */
    char *label;                     /**< 理想标签（可为 NULL） */
};
/**
 * @brief Gröbner 基结构体
 *
 * 一个理想 I 的 Gröbner 基是由有限个多项式组成的集合 G，
 * 满足 LT(I) = <LT(g) : g in G>（前项理想由 G 的前项生成）。
 *
 * 属性：
 * - is_minimal：是否是最小 Gröbner 基（前项互不整除）
 * - is_reduced：是否是约化 Gröbner 基（前项系数为 1 且各项不可被其他前项约化）
 */
struct lvGroebnerBasis {
    lvPolynomial **basis_polys;         /**< 基多项式数组 */
    int bases_count;                      /**< 基多项式数量 */
    int bases_capacity;                   /**< 基数组容量 */
    int reducing_degree;                  /**< 约化后的最大次数 */
    lvGroebnerAlgorithm algorithm_used; /**< 使用的算法 */
    int64_t computation_time_us;          /**< 计算耗时（微秒） */
    bool is_minimal;                      /**< 是否为最小 Gröbner 基 */
    bool is_reduced;                      /**< 是否为约化 Gröbner 基 */
};
/* ================================================================
 *  第五部分：代数簇（Variety）
 * ================================================================ */
/**
 * @brief 代数簇结构体
 *
 * 由理想 I 定义的代数簇 V(I) = { x | f(x) = 0 for all f in I }。
 * 即理想中所有多项式的公共零点集合。
 *
 * 此结构与 Lv-00 的几何构造系统联动：
 * - 构造图的约束条件转化为多项式理想
 * - 该理想的簇就是满足所有约束的点集
 * - 零维簇对应有限个离散解
 * - 正维簇对应连续的解空间（曲线、曲面等）
 */
struct lvVariety {
    int variety_id;           /**< 簇的唯一标识符 */
    int ideal_id;             /**< 定义理想的 ID */
    double **solution_points; /**< 解点数组（每个点是一个坐标向量） */
    int solution_count;       /**< 解点数量 */
    int solution_capacity;    /**< 解点数组容量 */
    int variety_dimension;    /**< 簇的维数（Krull 维数） */
    int degree_of_freedom;    /**< 自由度（参数化维度） */
    bool is_zero_dimensional; /**< 是否为有限点集（零维簇） */
    char *label;              /**< 簇标签（可为 NULL） */
};
/* ================================================================
 *  第六部分：环注册表（Ring Registry）
 * ================================================================ */
/**
 * @brief 环注册表
 *
 * 管理多个多项式环的全局注册表。
 * 借鉴 Singular 的"多环共存"范式：每个几何对象知道自己属于哪个环。
 *
 * 在 Lv-00 中，不同几何问题可能需要不同的多项式环
 * （例如一个在 Q[x,y] 上，一个在 C[u,v,w] 上），
 * 环注册表确保对象的环归属始终正确。
 */
struct lvRingRegistry {
    lvPolynomialRing **rings; /**< 已注册的环数组 */
    int ring_count;             /**< 当前环数量 */
    int ring_capacity;          /**< 环数组容量 */
    int active_ring_id;         /**< 当前活动环 ID（-1 表示无） */
    bool is_initialized;        /**< 注册表初始化状态 */
};
/* ================================================================
 *  第七部分：API —— 环管理
 * ================================================================ */
/**
 * @brief 创建环注册表
 *
 * @param capacity  环容量（建议 >= 8）
 * @return 成功返回注册表指针，失败返回 NULL
 */
lvRingRegistry *ring_registry_create(int capacity);
/**
 * @brief 销毁环注册表及其中所有环
 *
 * @param registry  环注册表
 */
void ring_registry_destroy(lvRingRegistry *registry);
/**
 * @brief 创建一个多项式环
 *
 * 借鉴 Singular 的 "先声明环" 范式。
 * 环定义了变量集、系数域和单项式序，后续一切多项式运算都在环的上下文中进行。
 *
 * @param registry    环注册表
 * @param var_names   变量名字符串数组
 * @param var_count   变量数量
 * @param field       系数域
 * @param order       单项式序
 * @param label       环标签（可为 NULL）
 * @return 成功返回环 ID（>= 0），失败返回 -1
 */
int ring_create(lvRingRegistry *registry, const char *var_names[], int var_count, lvRingFieldType field,
                lvMonomialOrder order, const char *label);
/**
 * @brief 销毁一个多项式环及其所有关联对象
 *
 * 注意：此操作也会销毁属于该环的所有多项式和理想。
 *
 * @param registry  环注册表
 * @param ring_id   环 ID
 */
void ring_destroy(lvRingRegistry *registry, int ring_id);
/**
 * @brief 注册一个外部创建的环
 *
 * @param registry  环注册表
 * @param ring      环指针
 * @return 成功返回分配的环 ID（>= 0），失败返回 -1
 */
int ring_register(lvRingRegistry *registry, lvPolynomialRing *ring);
/**
 * @brief 按 ID 查找环
 *
 * @param registry  环注册表
 * @param ring_id   环 ID
 * @return 环指针，若不存在返回 NULL
 */
lvPolynomialRing *ring_find(const lvRingRegistry *registry, int ring_id);
/* ================================================================
 *  第八部分：API —— 多项式操作
 * ================================================================ */
/**
 * @brief 创建多项式
 *
 * 在指定环中创建一个多项式。初始化为零多项式。
 *
 * @param registry  环注册表
 * @param ring_id   所属环 ID
 * @param capacity  项容量预分配
 * @param label     多项式标签（可为 NULL）
 * @return 成功返回多项式 ID（>= 0），失败返回 -1
 */
int poly_create(lvRingRegistry *registry, int ring_id, int capacity, const char *label);
/**
 * @brief 销毁多项式
 *
 * @param registry  环注册表
 * @param poly_id   多项式 ID
 */
void poly_destroy(lvRingRegistry *registry, int poly_id);
/**
 * @brief 多项式加法：h = f + g
 *
 * @param registry     环注册表
 * @param poly_id_f    被加多项式 ID
 * @param poly_id_g    加多项式 ID
 * @param result_label 结果标签（可为 NULL）
 * @return 成功返回结果多项式 ID（>= 0），失败返回 -1
 */
int poly_add(lvRingRegistry *registry, int poly_id_f, int poly_id_g, const char *result_label);
/**
 * @brief 多项式乘法：h = f * g
 *
 * @param registry     环注册表
 * @param poly_id_f    被乘多项式 ID
 * @param poly_id_g    乘多项式 ID
 * @param result_label 结果标签（可为 NULL）
 * @return 成功返回结果多项式 ID（>= 0），失败返回 -1
 */
int poly_multiply(lvRingRegistry *registry, int poly_id_f, int poly_id_g, const char *result_label);
/**
 * @brief 多项式代入：将指定变量替换为另一个多项式
 *
 * 实现 f(var_idx := g)，即 f(x_1, ..., x_i, ..., x_n) 中的 x_i 代入 g。
 *
 * @param registry   环注册表
 * @param poly_id    待代入多项式 ID
 * @param var_index  被替换的变量索引（0-based）
 * @param subst_poly_id 代入的多项式 ID
 * @param result_label 结果标签（可为 NULL）
 * @return 成功返回结果多项式 ID（>= 0），失败返回 -1
 */
int poly_substitute(lvRingRegistry *registry, int poly_id, int var_index, int subst_poly_id,
                    const char *result_label);
/**
 * @brief 获取多项式实例
 *
 * @param registry  环注册表
 * @param poly_id   多项式 ID
 * @return 多项式指针，若不存在返回 NULL
 */
const lvPolynomial *poly_get(const lvRingRegistry *registry, int poly_id);
/* ================================================================
 *  第九部分：API —— 理想与 Gröbner 基
 * ================================================================ */
/**
 * @brief 创建理想
 *
 * 由一组生成元定义理想 I = <f_1, ..., f_k>。
 *
 * @param registry  环注册表
 * @param ring_id   所属环 ID
 * @param label     理想标签（可为 NULL）
 * @return 成功返回理想 ID（>= 0），失败返回 -1
 */
int ideal_create(lvRingRegistry *registry, int ring_id, const char *label);
/**
 * @brief 销毁理想
 *
 * @param registry  环注册表
 * @param ideal_id  理想 ID
 */
void ideal_destroy(lvRingRegistry *registry, int ideal_id);
/**
 * @brief 向理想添加生成元
 *
 * @param registry  环注册表
 * @param ideal_id  理想 ID
 * @param poly_id   生成元多项式 ID
 * @return 成功返回 0，失败返回负值错误码
 */
int ideal_add_generator(lvRingRegistry *registry, int ideal_id, int poly_id);
/**
 * @brief 计算 Gröbner 基（核心函数）
 *
 * 为理想 I 计算 Gröbner 基。结果缓存在理想内部。
 * 支持多种算法，默认为 GROEBNER_AUTO 自动选择。
 *
 * @param registry  环注册表
 * @param ideal_id  理想 ID
 * @param algorithm 算法选择
 * @return 成功返回 0，失败返回负值错误码
 */
int groebner_compute(lvRingRegistry *registry, int ideal_id, lvGroebnerAlgorithm algorithm);
/**
 * @brief 增量式 Gröbner 基计算
 *
 * 向已有 Gröbner 基的理想添加新生成元后，增量更新基。
 * 比重新计算更高效（利用 F5 的增量性质）。
 *
 * @param registry      环注册表
 * @param ideal_id      理想 ID
 * @param new_poly_id   新增的生成元多项式 ID
 * @return 成功返回 0，失败返回负值错误码
 */
int groebner_compute_incremental(lvRingRegistry *registry, int ideal_id, int new_poly_id);
/**
 * @brief 理想成员判定
 *
 * 检查多项式 f 是否属于理想 I。
 * 等价于检查 f 的 Gröbner 基约化余式是否为 0。
 *
 * @param registry  环注册表
 * @param ideal_id  理想 ID
 * @param poly_id   待判定多项式 ID
 * @return 属于理想返回 true，否则返回 false
 */
bool ideal_membership(lvRingRegistry *registry, int ideal_id, int poly_id);
/**
 * @brief 理想交：计算 I ∩ J
 *
 * @param registry     环注册表
 * @param ideal_id_a   理想 I 的 ID
 * @param ideal_id_b   理想 J 的 ID
 * @return >= 0 新理想 ID, < 0 错误码
 */
int ideal_intersection(lvRingRegistry *registry, int ideal_id_a, int ideal_id_b);

/* Forward declarations for smt_backend_impl.c */
int constraint_graph_to_ideal(lvRingRegistry *registry, const ConstraintGraph *graph,
                               int ring_id, const char *ideal_name);
int variety_compute(lvRingRegistry *registry, int ideal_id, const char *variety_name);
bool variety_is_zero_dimensional(lvRingRegistry *registry, int variety_id);
int variety_dimension(lvRingRegistry *registry, int variety_id);

/**
 * @brief 从代数簇中获取指定索引的解点坐标
 *
 * 从 variety_compute() 计算得到的代数簇中提取第 point_idx 个解点的坐标值。
 * 仅对零维簇（有限解）有效；对正维簇返回 false。
 *
 * @param registry  环注册表
 * @param variety_id 代数簇 ID（来自 variety_compute()）
 * @param point_idx  解点索引（0-based）
 * @param out_coords 输出缓冲区，用于存储坐标值（大小需 >= coord_count）
 * @param coord_count 要读取的坐标数量
 * @return 成功返回 true，簇不存在或索引越界返回 false
 */
bool variety_get_solution_point(lvRingRegistry *registry, int variety_id,
                                int point_idx, double *out_coords, int coord_count);

#ifdef __cplusplus
}
#endif

#endif /* lv_GROEBNER_ENGINE_H */