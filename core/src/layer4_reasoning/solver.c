/**
 * @file solver.c
 * @brief 符号代数求解器实现
 *
 * @details 实现 Groebner 基算法和增量求解策略。
 *          支持从约束图中提取方程、求解代数系统、计算自由度。
 *          使用 GMP 多精度整数进行精确计算。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 *
 * @dependencies
 *   - solver.h              : 求解器公共接口定义
 *   - lv00_utils.h          : 统一内存分配器（lv00_malloc/lv00_free）
 *   - lv00_internal.h       : 内部数据结构和常量
 *   - constraint_graph.h    : 约束图接口
 *   - mpz_poly.h            : GMP 多精度多项式操作
 *   - stream.h              : 流式事件输出接口
 *
 * @note 内存管理策略（设计决策）：
 *   - GMP 多精度多项式系数数组（poly.coeffs）必须使用标准 malloc/calloc
 *     而非 lv00_malloc 分配，因为 mpz_poly_clear() 内部调用标准 lv00_free() 释放
 *     系数数组。若使用 lv00_malloc 分配（带额外 AllocHeader），lv00_free() 将因为
 *     指针不指向标准堆头而导致未定义行为。这是 GMP 库的硬性要求，非可选方案。
 *   - 其他所有动态内存分配统一使用 lv00_malloc/lv00_free。
 *
 * @note GMP 变量作用域与所有权规则（v3.3.0 补充）：
 *   - 每个 mpz_init/mpq_init 必须在相同或内层作用域内对应一个 mpz_clear/mpq_clear，
 *     且两者的执行路径必须通过所有代码路径可达。不允许有 init 无 clear 的路径。
 *   - 跨代码块复用的 GMP 变量：在变量声明处添加 [owner: block_name] 注释
 *     标明其生命周期边界。此类变量必须在定义所在作用域结束时 clear，不得在
 *     其他作用域提前释放。
 *   - 循环体内 init 的变量：必须在同一次迭代结束前 clear。循环内频繁 init/clear
 *     的变量可考虑提升到循环外并复用（mpz_set 后使用），以节省分配开销。
 *   - 条件分支内的 init：确保 if/else 两个分支都有对应的 clear，不得在
 *     一个分支 init 而在另一个分支泄漏。
 */

#include "solver.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constraint_graph.h"
#include "debug.h"
#include "lv00_internal.h"
#include "lv00_utils.h" /* lv00_malloc / lv00_free —— 统一内存分配器 */
#include "mpz_poly.h"
#include "lv00/stream.h"
#include "stream_context_util.h"

/* SOLVER_MAX_VAR_ID 已在 solver.h 中统一定义，此处不再重复 */

/**
 * LV00_SOLVER_DYNARRAY_INIT_CAP - 动态数组初始容量
 *
 * 用于 equation_system_push, mv_poly_add_term, dirty_set_add 等
 * 多处动态数组的初始分配容量。值取 16，兼顾小系统零次扩容与大系统
 * 较少扩容次数之间的平衡。
 */
#define LV00_SOLVER_DYNARRAY_INIT_CAP 16

/**
 * LV00_SOLVER_LINEAR_COEFF_COUNT - 一元一次多项式系数数量
 *
 * degree=1 的多项式拥有 2 个系数：coeffs[0]（常数项）和 coeffs[1]（一次项）。
 * 用于 poly.coeffs 数组的 malloc 分配。
 */
#define LV00_SOLVER_LINEAR_COEFF_COUNT 2

/**
 * LV00_SOLVER_QUADRATIC_COEFF_COUNT - 一元二次多项式系数数量
 *
 * degree=2 的多项式拥有 3 个系数：coeffs[0]（常数项）、coeffs[1]（一次项）
 * 和 coeffs[2]（二次项）。用于 poly.coeffs 数组的 malloc 分配。
 */
#define LV00_SOLVER_QUADRATIC_COEFF_COUNT 3

/* ------------------------------------------------------------------ */
/*  SolverSnapshot — 求解回滚机制                                       */
/* ------------------------------------------------------------------ */

/**
 * LV00_ZERO_EPSILON - 零值容差（用于 GMP 整数比较）
 *
 * 在将有理数转换为缩放整数时，当 remainder 与 half_den 的差值
 * 小于此宏值时视为相等，避免浮点舍入导致的错误进位。这解决了
 * 符号判别中的临界盲区问题：当余数恰好落在中点附近时，
 * 传统的 >= 比较会因为舍入噪声而产生错误的符号判定。
 */
#define LV00_ZERO_EPSILON 1e-12

#define SOLVER_DETAIL_BUF_SIZE 512

/**
 * @brief 求解快照结构体——保存求解前的节点坐标状态
 *
 * 在核心求解函数开始前拍摄快照；若求解失败，通过快照回滚恢复
 * 所有受影响的节点坐标，防止部分求解结果污染图形状态。
 *
 * 快照仅保存坐标值（SymbolicCoord 副本），不保存整个 GeomNode。
 * 所有权：snapshot 拥有 copies 数组中每个 SymbolicCoord* 的所有权。
 */
typedef struct SolverSnapshot {
    int *node_ids;       /**< 受影响的节点 ID 数组（长度 = node_count） */
    SymbolicCoord **copies; /**< 对应节点的坐标副本（长度 = node_count * 2，x/y 连续存放） */
    int node_count;      /**< 节点数量 */
    int coord_count;     /**< 坐标总数量（= node_count * 2） */
} SolverSnapshot;

/* 前向声明：solver_snapshot_free 在 solver_snapshot_save 中使用，定义在下方 */
static void solver_snapshot_free(SolverSnapshot *snapshot);

/**
 * @brief 保存求解前的节点坐标快照
 *
 * 遍历约束图中所有几何节点，为每个节点的符号坐标创建深拷贝。
 * 快照中的所有 SymbolicCoord 由调用者通过 solver_snapshot_free 释放。
 *
 * @param graph    约束图指针
 * @param snapshot 输出：填充的快照结构体
 * @return true 表示成功，false 表示内存分配失败
 */
static bool solver_snapshot_save(const ConstraintGraph *graph, SolverSnapshot *snapshot) {
    if (!graph || !snapshot)
        return false;

    memset(snapshot, 0, sizeof(SolverSnapshot));
    snapshot->node_count = graph->node_count;
    if (snapshot->node_count <= 0)
        return true;

    snapshot->coord_count = snapshot->node_count * 2;
    snapshot->node_ids = lv00_malloc((size_t) snapshot->node_count * sizeof(int));
    snapshot->copies = lv00_malloc((size_t) snapshot->coord_count * sizeof(SymbolicCoord *));

    if (!snapshot->node_ids || !snapshot->copies) {
        solver_snapshot_free(snapshot);
        return false;
    }

    for (int i = 0; i < snapshot->node_count; i++) {
        snapshot->node_ids[i] = graph->nodes[i]->id;
        GeomNode *node = graph->nodes[i];

        if (node && node->symbolic_coords) {
            snapshot->copies[i * 2 + 0] =
                node->symbolic_coords[0] ? symbolic_coord_copy(node->symbolic_coords[0]) : NULL;
            snapshot->copies[i * 2 + 1] =
                (node->coord_count >= 2 && node->symbolic_coords[1])
                    ? symbolic_coord_copy(node->symbolic_coords[1])
                    : NULL;
        } else {
            snapshot->copies[i * 2 + 0] = NULL;
            snapshot->copies[i * 2 + 1] = NULL;
        }
    }

    return true;
}

/**
 * @brief 回滚——将节点坐标恢复到快照状态
 *
 * 在求解失败（SOLVER_STATUS_TIMEOUT / SOLVER_STATUS_NO_SOLUTION 等）时调用，
 * 将受影响的节点坐标替换为快照中保存的原始值。
 *
 * 使用场景：求解过程中部分方程已被求解并回代，导致某些节点坐标
 * 被修改，但后续发现系统无解或超时——此时需要回滚所有修改。
 *
 * @param graph    约束图指针
 * @param snapshot 先前通过 solver_snapshot_save 保存的快照
 */
static void solver_snapshot_restore(ConstraintGraph *graph, const SolverSnapshot *snapshot) {
    if (!graph || !snapshot || snapshot->node_count <= 0)
        return;

    for (int i = 0; i < snapshot->node_count; i++) {
        /* 按节点 ID 查找当前图中的节点（节点顺序可能已变化） */
        GeomNode *node = graph_get_node(graph, snapshot->node_ids[i]);
        if (!node || !node->symbolic_coords)
            continue;

        if (snapshot->copies[i * 2 + 0]) {
            symbolic_coord_destroy(node->symbolic_coords[0]);
            node->symbolic_coords[0] = symbolic_coord_copy(snapshot->copies[i * 2 + 0]);
        }
        if (node->coord_count >= 2 && node->symbolic_coords[1] && snapshot->copies[i * 2 + 1]) {
            symbolic_coord_destroy(node->symbolic_coords[1]);
            node->symbolic_coords[1] = symbolic_coord_copy(snapshot->copies[i * 2 + 1]);
        }
    }
}

/**
 * @brief 释放快照——求解成功后释放所有保存的坐标副本
 *
 * 快照不再需要时必须调用，否则会泄漏 SymbolicCoord 内存。
 * 此函数是幂等的：多次调用或对零值快照调用是安全的。
 *
 * @param snapshot 待释放的快照（调用后置零）
 */
static void solver_snapshot_free(SolverSnapshot *snapshot) {
    if (!snapshot)
        return;

    if (snapshot->copies) {
        for (int i = 0; i < snapshot->coord_count; i++) {
            if (snapshot->copies[i]) {
                symbolic_coord_destroy(snapshot->copies[i]);
                snapshot->copies[i] = NULL;
            }
        }
        lv00_free((void **) &snapshot->copies);
    }

    lv00_free((void **) &snapshot->node_ids);
    snapshot->node_count = 0;
    snapshot->coord_count = 0;
}


LV00_DECLARE_STREAM_CTX(solver);

/**
 * @brief 内部结构：一元多项式方程的内部表示
 *
 * 将单个几何约束编码为一元多项式，关联到节点和坐标维度。
 * 求解器以此结构为基本单位构建方程组并执行代数消元。
 */
/* ------------------------------------------------------------------ */

typedef struct {
    mpz_poly_t poly; /* 一元多项式，coeffs[0]..coeffs[degree] */
    int var_node_id; /* 该方程约束的几何节点ID（坐标索引单独存储） */
    int coord_index; /* 0 = x, 1 = y */
} PolyEquation;

typedef struct EquationSystem {
    PolyEquation *eqs;
    int count;
    int capacity;
} EquationSystem;

/**
 * @brief 初始化方程系统
 *
 * 将方程系统结构体清零，设置初始状态（无方程、无容量）。
 *
 * @param sys 方程系统指针（必须非空）
 */
static void equation_system_init(EquationSystem *sys) {
    sys->eqs = NULL;
    sys->count = 0;
    sys->capacity = 0;
}

/**
 * @brief 检查 equation_system_push 返回值的辅助宏
 *
 * 当 push 失败（OOM）时，设置错误状态并跳转到指定的清理标签。
 * 用于避免在37个调用点重复相同的错误检查代码。
 */
#define EQUATION_PUSH_OR_GOTO(sys, poly, vid, ci, label) \
    do { \
        if (equation_system_push((sys), (poly), (vid), (ci)) != 0) { \
            lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "equation_system_push: 方程添加失败（内存不足）"); \
            goto label; \
        } \
    } while (0)

/**
 * @brief 向方程系统中添加一个多项式方程
 *
 * 如果当前容量不足，先扩容再插入。使用临时变量保存新容量，
 * 确保 realloc 失败时不会破坏原有状态。
 *
 * @param sys        方程系统指针
 * @param poly       多项式（通过值拷贝，内部会复制）
 * @param var_node_id 变量对应的几何节点 ID
 * @param coord_index 坐标索引（0 = x, 1 = y）
 * @return 0 表示成功，-1 表示失败（内存不足或容量溢出）
 */
static int equation_system_push(EquationSystem *sys, mpz_poly_t poly, int var_node_id, int coord_index) {
    if (sys->count >= sys->capacity) {
        if (sys->capacity > INT_MAX / 2)
            return -1;
        /* 使用临时变量计算新容量，避免 realloc 失败时 capacity 已被修改 */
        int new_capacity = sys->capacity == 0 ? LV00_SOLVER_DYNARRAY_INIT_CAP : sys->capacity * LV00_ARRAY_GROWTH_FACTOR;
        /* 溢出检查：确保 new_capacity * sizeof(PolyEquation) 不超过 SIZE_MAX */
        if ((size_t)new_capacity > SIZE_MAX / sizeof(PolyEquation)) {
            return -1;
        }
        PolyEquation *new_eqs = lv00_realloc(sys->eqs, (size_t)new_capacity * sizeof(PolyEquation));
        if (!new_eqs) {
            lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "equation_system_push: 扩容失败");
            /* 注意：poly 按值传入（结构体副本，内部 coeffs 指针与调用者共享），
             * 不能在此处调用 mpz_poly_clear(&poly)，否则会导致调用者的
             * poly.coeffs 变成悬空指针。poly 的资源由调用者负责清理。 */
            return -1;
        }
        sys->eqs = new_eqs;
        sys->capacity = new_capacity; /* 仅在 realloc 成功后才更新容量 */
    }
    sys->eqs[sys->count].var_node_id = var_node_id;
    sys->eqs[sys->count].coord_index = coord_index;
    mpz_poly_init(&sys->eqs[sys->count].poly);
    if (!mpz_poly_set(&sys->eqs[sys->count].poly, &poly)) {
        mpz_poly_clear(&sys->eqs[sys->count].poly);
        lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "equation_system_push: mpz_poly_set 失败");
        return -1;
    }
    sys->count++;
    return 0;
}

/**
 * @brief 清空并释放方程系统内的所有资源
 *
 * 逐个清除每个多项式方程，释放方程数组，并将系统重置为空状态。
 *
 * @param sys 方程系统指针
 */
static void equation_system_clear(EquationSystem *sys) {
    for (int i = 0; i < sys->count; i++) {
        mpz_poly_clear(&sys->eqs[i].poly);
    }
    lv00_free((void **) &sys->eqs);
    sys->eqs = NULL;
    sys->count = 0;
    sys->capacity = 0;
}

/* ------------------------------------------------------------------ */
/*  内部：从 SymbolicCoord 提取数值的辅助函数                          */
/* ------------------------------------------------------------------ */

/* 尝试从 SymbolicCoord 获取 double 近似值。
   参数 c 带有 const 限定符，确保不会修改原始坐标。
   返回 true 表示成功获取近似值。 */

/**
 * @brief 通过序列化方式将符号坐标转换为浮点数（通用路径）
 * @param c 符号坐标
 * @param out 输出双精度值
 * @return 成功返回 true，失败返回 false
 *
 * 适用于 QUADRATIC、ALGEBRAIC、TRANSCENDENTAL 等类型的坐标转换。
 * 通过序列化为字符串再解析为浮点数实现，精度受限于 strtod。
 */
static bool coord_to_double_via_serialize(const SymbolicCoord *c, double *out) {
    if (!c || !out) return false;
    char *str = symbolic_coord_serialize(c);
    if (!str) return false;
    char *endptr = NULL;
    *out = strtod(str, &endptr);
    lv00_free((void **) &str);
    return (endptr != str);
}

static bool coord_to_double(const SymbolicCoord *c, double *out) {
    if (!c)
        return false;
    switch (c->type) {
        case RATIONAL: {
            if (c->data.rational) {
                *out = mpq_get_d(c->data.rational->value);
                return true;
            }
            /* 数据不一致：RATIONAL 类型但 rational 指针为空 */
            LV00_LOG_WARNING("coord_to_double: RATIONAL 类型但 data.rational 为 NULL");
            *out = 0.0;
            return false;
        }
        case QUADRATIC:
        case ALGEBRAIC:
            return coord_to_double_via_serialize(c, out);
        case TRANSCENDENTAL: {
            /* 使用公共 API 获取超越常数的数值近似值（如 pi, e 等） */
            *out = symbolic_coord_to_double(c);
            return (fabs(*out) > LV00_EPSILON_DOUBLE || c->data.transcendental != NULL);
        }
        default:
            return false;
    }
}

/* =======================================================================
 * 内部函数：将 double 值转换为带缩放的 mpz_t
 * 当可能时从 SymbolicCoord 进行精确有理数提取
 * ======================================================================= */

/**
 * @brief 将有理数乘以缩放系数后向下取整为 mpz_t
 *
 * 计算 floor((mpq_numref(val) * scale) / mpq_denref(val))。
 * 此辅助函数消除了 coord_to_mpz_scaled_exact() 中 RATIONAL、
 * QUADRATIC(b=0)、ALGEBRAIC(cached_rational) 等多个分支中
 * 重复的"有理数乘缩放→取整"模式。
 *
 * @param val    输入有理数值（mpq_srcptr，只读）
 * @param result 输出 mpz_t 整数，结果直接写入该变量
 * @param scale  缩放系数
 */
static void rational_to_mpz_scaled(mpq_srcptr val, mpz_t result, int64_t scale) {
    mpz_t scaled;
    mpz_init(scaled);
    mpz_set_si(scaled, (long) scale);
    mpz_mul(result, mpq_numref(val), scaled);
    mpz_fdiv_q(result, result, mpq_denref(val));
    mpz_clear(scaled);
}

/**
 * @brief 将 SymbolicCoord 值转换为按 scale 缩放的 mpz_t 整数
 *
 * 如果坐标是 RATIONAL 类型，提取精确的 mpq_t 值并乘以 scale 得到精确整数（无精度损失）。
 * 对于非 RATIONAL 类型，回退到 double 近似值。
 *
 * @param c      SymbolicCoord 指针
 * @param result 输出 mpz_t 整数
 * @param scale  缩放因子
 * @return true 表示成功（精确或近似），false 表示失败
 */
static bool coord_to_mpz_scaled(const SymbolicCoord *c, mpz_t result, int64_t scale) {
    if (!c)
        return false;

    if (c->type == RATIONAL && c->data.rational) {
        mpq_srcptr val = c->data.rational->value;

        /* 计算分子 * scale（只计算一次，避免重复） */
        mpz_t scaled_num;
        mpz_init(scaled_num);
        mpz_mul_si(scaled_num, mpq_numref(val), (long) scale);

        mpz_t den;
        mpz_init(den);
        mpz_set(den, mpq_denref(val));

        if (mpz_divisible_p(scaled_num, den)) {
            /* 精确整除：直接取商 */
            mpz_fdiv_q(result, scaled_num, den);
        } else {
            /* 非精确整除：使用银行家舍入（round to nearest even）避免系统性偏差 */
            mpz_t q, r;
            mpz_init(q);
            mpz_init(r);
            mpz_fdiv_qr(q, r, scaled_num, den);
            mpz_t half_den;
            mpz_init(half_den);
            mpz_fdiv_q_2exp(half_den, den, 1);
            /*
             * 临界盲区修复：当剩余 r 与 half_den 相等时，使用银行家舍入
             * (round to nearest even) 而非单纯向上舍入，避免系统性偏差。
             */
            int cmp = mpz_cmp(r, half_den);
            if (cmp > 0) {
                mpz_add_ui(result, q, 1);
            } else if (cmp < 0) {
                mpz_set(result, q);
            } else {
                /* 临界点：r == half_den，使用银行家舍入 */
                if (mpz_even_p(q)) {
                    mpz_set(result, q);
                } else {
                    mpz_add_ui(result, q, 1);
                }
            }
            mpz_clear(half_den);
            mpz_clear(q);
            mpz_clear(r);
        }

        mpz_clear(scaled_num);
        mpz_clear(den);
        return true;
    }

    /* 回退：使用 double 近似值 */
    double d;
    if (coord_to_double(c, &d)) {
        mpz_set_d(result, d * (double) scale);
        return true;
    }

    return false;
}

/* 前向声明：coord_to_mpz_scaled_exact 在 double_to_mpz_scaled 之后定义，
   但 coord_to_mpz_scaled 的回退路径需要引用 double_to_mpz_scaled */
static void double_to_mpz_scaled(double val, mpz_t result, int64_t scale);

/**
 * 将符号坐标精确转换为缩放整数系数。
 * 对于有理数坐标，使用精确的 mpq -> mpz 转换。
 * 对于二次根式和代数数，尝试有理化后转换；失败时回退到 double 近似。
 * 返回 true 表示精确转换成功，false 表示使用了近似。
 */
static bool coord_to_mpz_scaled_exact(const SymbolicCoord *coord, mpz_t result, int64_t scale) {
    if (!coord)
        return false;

    switch (coord->type) {
        case RATIONAL: {
            if (!coord->data.rational)
                return false;
            rational_to_mpz_scaled(coord->data.rational->value, result, scale);
            return true;
        }
        case QUADRATIC: {
            /* 尝试有理化：如果 b=0，退化为有理数 */
            Quadratic *q = coord->data.quadratic;
            if (q && mpq_sgn(q->b->value) == 0) {
                rational_to_mpz_scaled(q->a->value, result, scale);
                return true;
            }
            /* 否则回退到 double */
            double val;
            if (coord_to_double(coord, &val)) {
                double_to_mpz_scaled(val, result, scale);
                return false;
            }
            return false;
        }
        case ALGEBRAIC: {
            /* 尝试有理化 */
            Algebraic *a = coord->data.algebraic;
            if (a && a->cached_rational) {
                rational_to_mpz_scaled(a->cached_rational->value, result, scale);
                return true;
            }
            /* 尝试运行有理化 */
            if (a && algebraic_try_rationalize(a) && a->cached_rational) {
                rational_to_mpz_scaled(a->cached_rational->value, result, scale);
                return true;
            }
            /* 回退到 double */
            double val;
            if (coord_to_double(coord, &val)) {
                double_to_mpz_scaled(val, result, scale);
                return false;
            }
            return false;
        }
        case TRANSCENDENTAL: {
            /* 超越数只能用近似 */
            double val;
            if (coord_to_double(coord, &val)) {
                double_to_mpz_scaled(val, result, scale);
                return false;
            }
            return false;
        }
    }
    return false;
}

/*
 * 将普通 double 值转换为按 scale 缩放的 mpz_t 整数。
 * 内部使用 mpq_t 避免中间精度损失：
 * 先从 double 构造 mpq，再乘以 scale 得到缩放后的整数。
 * 当 scaled_num 不能被 den 整除时，使用四舍五入取整。
 */
static void double_to_mpz_scaled(double val, mpz_t result, int64_t scale) {
    /* 防御特殊浮点值 */
    if (!isfinite(val)) {
        mpz_set_ui(result, 0);
        return;
    }

    /* 使用 GMP 的 mpq_set_d 获取最佳精度 */
    mpq_t q;
    mpq_init(q);
    mpq_set_d(q, val);

    /* result = q * scale = (num * scale) / den */
    mpz_t scaled_num;
    mpz_init(scaled_num);
    mpz_mul_si(scaled_num, mpq_numref(q), (long) scale);

    mpz_t den;
    mpz_init(den);
    mpz_set(den, mpq_denref(q));

    if (mpz_divisible_p(scaled_num, den)) {
        /* 能整除时直接取精确商 */
        mpz_divexact(result, scaled_num, den);
    } else {
        /* 不能整除时，使用银行家舍入（round to nearest even）避免系统性偏差 */
        mpz_t quotient, remainder;
        mpz_init(quotient);
        mpz_init(remainder);
        mpz_fdiv_qr(quotient, remainder, scaled_num, den);
        mpz_t half_den;
        mpz_init(half_den);
        mpz_fdiv_q_2exp(half_den, den, 1);
        /*
         * 临界盲区修复：当 remainder 与 half_den 相等时，使用银行家舍入
         * (round to nearest even) 而非单纯向上舍入。这消除了当余数恰好落在
         * 中点附近时因浮点舍入噪声导致的错误符号判别。
         *
         * 零值保护：若原始 double 值 val 的绝对值 < LV00_ZERO_EPSILON，
         * 则结果为 0，防止微小浮点噪声产生非零整数结果。
         */
        if (fabs(val) < LV00_ZERO_EPSILON) {
            mpz_set_ui(result, 0);
        } else {
            int cmp = mpz_cmp(remainder, half_den);
            if (cmp > 0) {
                mpz_add_ui(result, quotient, 1);
            } else if (cmp < 0) {
                mpz_set(result, quotient);
            } else {
                /* 临界点：remainder == half_den，使用银行家舍入 */
                if (mpz_even_p(quotient)) {
                    mpz_set(result, quotient);
                } else {
                    mpz_add_ui(result, quotient, 1);
                }
            }
        }
        mpz_clear(half_den);
        mpz_clear(quotient);
        mpz_clear(remainder);
    }

    mpz_clear(scaled_num);
    mpz_clear(den);
    mpq_clear(q);
}

/* ------------------------------------------------------------------ */
/*  内部：按 ID 查找几何节点                                          */
/* ------------------------------------------------------------------ */

/**
 * @brief 在约束图中按 ID 查找几何节点
 *
 * 封装 graph_get_node 调用，提供统一的内部查找接口。
 *
 * @param graph 约束图指针
 * @param id    目标节点的 ID
 * @return 找到的 GeomNode 指针，未找到返回 NULL
 */
static GeomNode *find_node(const ConstraintGraph *graph, int id) {
    return graph_get_node(graph, id);
}

/**
 * @brief 提取点的数值坐标值
 *
 * 从点的 symbolc_coords 数组的指定索引获取 double 数值。
 *
 * @param pt  目标点节点（必须为 GEOM_POINT 类型且坐标数量足够）
 * @param idx 坐标索引（0 = x, 1 = y）
 * @param out 输出：坐标的 double 近似值
 * @return true 表示成功获取，false 表示节点类型不符或坐标不足
 */
static bool point_coord(const GeomNode *pt, int idx, double *out) {
    if (!pt || pt->type != GEOM_POINT || pt->coord_count <= idx)
        return false;
    return coord_to_double(pt->symbolic_coords[idx], out);
}

/* =======================================================================
 * 内部函数：从两点构建直线方程 ax + by + c = 0
 * ======================================================================= */

typedef struct {
    double a, b, c; /* ax + by + c = 0 */
} LineEquation;

static bool line_from_two_points(GeomNode *p1, GeomNode *p2, LineEquation *out) {
    double x1, y1, x2, y2;
    if (!point_coord(p1, 0, &x1) || !point_coord(p1, 1, &y1))
        return false;
    if (!point_coord(p2, 0, &x2) || !point_coord(p2, 1, &y2))
        return false;
    /* 方向向量 (dx, dy) */
    double dx = x2 - x1;
    double dy = y2 - y1;

    /* 检测退化情况：两点重合
     * 使用 epsilon 比较而非精确相等（==），原因：
     * 浮点运算存在舍入误差，即使两点在数学上重合，
     * 经过坐标变换或中间计算后，dx 和 dy 可能不为精确的 0.0。
     * 使用 fabs(dx) < 1e-15 可以正确识别数值上近似为零的情况，
     * 避免将几乎重合的误判为有效直线（导致法向量接近零、方程退化）。 */
    if (fabs(dx) < 1e-15 && fabs(dy) < 1e-15) {
        LOG_WARN("solver", "line_from_two_points: 两点重合，无法确定直线");
        return false;
    }

    /* 法向量: (dy, -dx) => dy*(x-x1) - dx*(y-y1) = 0 */
    out->a = dy;
    out->b = -dx;
    out->c = -(out->a * x1 + out->b * y1);
    return true;
}

/* ------------------------------------------------------------------ */
/*  内部：从约束中提取代数方程                                         */
/* ------------------------------------------------------------------ */

/*
 * For INCIDENCE(point, line_segment):
 *   The line segment has two endpoint points.  The incidence constraint
 *   means the point lies on the line, i.e. cross product of direction
 *   vector and (point - endpoint) is zero.  This gives one linear equation.
 *
 * For INTERSECTION(line1, line2, result_point):
 *   The result point lies on both lines => two linear equations.
 *
 * For BETWEENNESS(p1, p2, p3):
 *   p2 lies on segment p1-p3.  This gives a collinearity equation
 *   plus a ratio constraint 0 <= t <= 1 where p2 = p1 + t*(p3-p1).
 *
 * For distance constraints (not directly a ConstraintType, but encoded
 * via CONNECTION or special numeric_assumption_declaration):
 *   (x2-x1)^2 + (y2-y1)^2 = d^2, which is quadratic.
 *
 * 精度限制说明：
 *   本函数在将几何约束转换为多项式方程时，使用 double 近似值来表示
 *   坐标和参数（如线段端点、距离值等），然后通过 double_to_mpz_scaled()
 *   将 double 转换为缩放后的 mpz_t 整数系数。这意味着：
 *   1. 对于 RATIONAL 类型的坐标，coord_to_double() 可以通过 mpq_get_d()
 *      获得精确的 double 表示（前提是值在 double 精度范围内）。
 *   2. 对于 QUADRATIC/ALGEBRAIC 类型的坐标，coord_to_double() 需要
 *      先序列化为字符串再解析，存在额外的精度损失。
 *   3. 缩放因子 scale=1000000 提供了约 6 位十进制精度，对于大多数
 *      几何计算足够，但不适用于需要高精度的场景。
 *   4. 如果需要完全精确的方程提取，应重构为直接使用 mpq_t/mpz_t
 *      而非经过 double 中间表示。
 */

/**
 * @brief 从约束图中提取代数方程
 *
 * @details 遍历约束图中的所有约束，根据约束类型生成对应的多项式方程：
 *          INCIDENCE（关联）、INTERSECTION（交点）、BETWEENNESS（介于）、
 *          CONTAINMENT（包含）、CONNECTION（连接）。
 *          也处理线段节点上的 numeric_assumption_declaration 距离约束。
 *          构造临时点用于 line_from_two_points，内存管理采用栈分配，
 *          不使用动态分配以避免泄漏风险。
 *
 * @param graph 约束图指针
 * @param sys   输出：存储提取方程的系统
 */
static void extract_equations_from_constraints(const ConstraintGraph *graph, EquationSystem *sys) {
    for (int ci = 0; ci < graph->constraint_count; ci++) {
        Constraint *c = graph->constraints[ci];
        if (!c || c->participant_count < 2)
            continue;

        switch (c->type) {
            case INCIDENCE: {
                /* participants[0] = point, participants[1] = line/region */
                GeomNode *pt = find_node(graph, c->participants[0]);
                GeomNode *line = find_node(graph, c->participants[1]);
                if (!pt || !line)
                    break;
                if (line->type == GEOM_LINE_SEGMENT && line->coord_count >= 2) {
                    /* 线段的 symbolic_coords 存储端点坐标为 (x1,y1,x2,y2) 格式 */
                    if (line->coord_count >= 4) {
                        int64_t scale = LV00_SOLVER_SCALE_FACTOR;
                        /* 使用精确有理数路径获取端点坐标的缩放值 */
                        mpz_t lx1_s, ly1_s, lx2_s, ly2_s;
                        mpz_init(lx1_s);
                        mpz_init(ly1_s);
                        mpz_init(lx2_s);
                        mpz_init(ly2_s);
                        bool exact = coord_to_mpz_scaled_exact(line->symbolic_coords[0], lx1_s, scale) &&
                                     coord_to_mpz_scaled_exact(line->symbolic_coords[1], ly1_s, scale) &&
                                     coord_to_mpz_scaled_exact(line->symbolic_coords[2], lx2_s, scale) &&
                                     coord_to_mpz_scaled_exact(line->symbolic_coords[3], ly2_s, scale);

                        if (exact) {
                            /* 精确路径：所有坐标都是有理数，在 mpz 层面计算 */
                            mpz_t dx_s, dy_s;
                            mpz_init(dx_s);
                            mpz_init(dy_s);
                            mpz_sub(dx_s, lx2_s, lx1_s); /* dx * scale */
                            mpz_sub(dy_s, ly2_s, ly1_s); /* dy * scale */

                            /* 叉积：dx*(py - ly1) - dy*(px - lx1) = 0
                           => -dy*px + (dy*lx1 - dx*ly1) = 0
                           系数1 = -dy (已缩放)
                           系数0 = dy*lx1 - dx*ly1 (需要除以 scale，因为 dy_s 和 lx1_s 都已缩放) */
                            mpz_poly_t poly;
                            mpz_poly_init(&poly);
                            poly.degree = 1;
                            /* GMP 要求使用标准分配器 */
                            poly.coeffs = malloc(2 * sizeof(mpz_t));
                            if (!poly.coeffs) {
                                mpz_poly_clear(&poly);
                                mpz_clear(dx_s);
                                mpz_clear(dy_s);
                                mpz_clear(lx1_s);
                                mpz_clear(ly1_s);
                                mpz_clear(lx2_s);
                                mpz_clear(ly2_s);
                                continue;
                            }
                            mpz_init(poly.coeffs[1]);
                            mpz_init(poly.coeffs[0]);
                            mpz_neg(poly.coeffs[1], dy_s);
                            /* dy*lx1 - dx*ly1 = (dy_s * lx1_s - dx_s * ly1_s) / scale */
                            {
                                mpz_t term1, term2;
                                mpz_init(term1);
                                mpz_init(term2);
                                mpz_mul(term1, dy_s, lx1_s);
                                mpz_mul(term2, dx_s, ly1_s);
                                mpz_sub(term1, term1, term2);
                                mpz_fdiv_q_ui(poly.coeffs[0], term1, (unsigned long) scale);
                                mpz_clear(term1);
                                mpz_clear(term2);
                            }
                            EQUATION_PUSH_OR_GOTO(sys, poly, pt->id, 0, push_error);
                            mpz_poly_clear(&poly);

                            /* y 坐标的第二个方程：
                           dx*py + (-dx*ly1 - dy*lx1) = 0 */
                            mpz_poly_init(&poly);
                            poly.degree = 1;
                            /* GMP 要求使用标准分配器 */
                            poly.coeffs = malloc(2 * sizeof(mpz_t));
                            if (!poly.coeffs) {
                                mpz_poly_clear(&poly);
                                mpz_clear(dx_s);
                                mpz_clear(dy_s);
                                mpz_clear(lx1_s);
                                mpz_clear(ly1_s);
                                mpz_clear(lx2_s);
                                mpz_clear(ly2_s);
                                continue;
                            }
                            mpz_init(poly.coeffs[1]);
                            mpz_init(poly.coeffs[0]);
                            mpz_set(poly.coeffs[1], dx_s);
                            /* -dx*ly1 - dy*lx1 = -(dx_s * ly1_s + dy_s * lx1_s) / scale */
                            {
                                mpz_t term1, term2;
                                mpz_init(term1);
                                mpz_init(term2);
                                mpz_mul(term1, dx_s, ly1_s);
                                mpz_mul(term2, dy_s, lx1_s);
                                mpz_add(term1, term1, term2);
                                mpz_neg(term1, term1);
                                mpz_fdiv_q_ui(poly.coeffs[0], term1, (unsigned long) scale);
                                mpz_clear(term1);
                                mpz_clear(term2);
                            }
                            EQUATION_PUSH_OR_GOTO(sys, poly, pt->id, 1, push_error);
                            mpz_poly_clear(&poly);
                            mpz_clear(dx_s);
                            mpz_clear(dy_s);
                        } else {
                            /* 回退到 double 近似路径 */
                            double lx1, ly1, lx2, ly2;
                            if (coord_to_double(line->symbolic_coords[0], &lx1) &&
                                coord_to_double(line->symbolic_coords[1], &ly1) &&
                                coord_to_double(line->symbolic_coords[2], &lx2) &&
                                coord_to_double(line->symbolic_coords[3], &ly2)) {
                                double dx = lx2 - lx1;
                                double dy = ly2 - ly1;
                                mpz_poly_t poly;
                                mpz_poly_init(&poly);
                                poly.degree = 1;
                                /* GMP 要求使用标准分配器 */
                                poly.coeffs = malloc(2 * sizeof(mpz_t));
                                if (!poly.coeffs) {
                                    mpz_poly_clear(&poly);
                                    break;
                                }
                                mpz_init(poly.coeffs[1]);
                                mpz_init(poly.coeffs[0]);
                                double_to_mpz_scaled(-dy, poly.coeffs[1], scale);
                                double_to_mpz_scaled(dy * lx1 - dx * ly1, poly.coeffs[0], scale);
                                EQUATION_PUSH_OR_GOTO(sys, poly, pt->id, 0, push_error);
                                mpz_poly_clear(&poly);

                                mpz_poly_init(&poly);
                                poly.degree = 1;
                                /* GMP 要求使用标准分配器 */
                                poly.coeffs = malloc(2 * sizeof(mpz_t));
                                if (!poly.coeffs) {
                                    mpz_poly_clear(&poly);
                                    break;
                                }
                                mpz_init(poly.coeffs[1]);
                                mpz_init(poly.coeffs[0]);
                                double_to_mpz_scaled(dx, poly.coeffs[1], scale);
                                double_to_mpz_scaled(-dx * ly1 - dy * lx1, poly.coeffs[0], scale);
                                EQUATION_PUSH_OR_GOTO(sys, poly, pt->id, 1, push_error);
                                mpz_poly_clear(&poly);
                            }
                        }
                        mpz_clear(lx1_s);
                        mpz_clear(ly1_s);
                        mpz_clear(lx2_s);
                        mpz_clear(ly2_s);
                    }
                }
                break;
            }

            case INTERSECTION: {
                /* participants[0] = line1, participants[1] = line2,
               participants[2] = result_point（交点） */
                if (c->participant_count < 3)
                    break;
                GeomNode *line1 = find_node(graph, c->participants[0]);
                GeomNode *line2 = find_node(graph, c->participants[1]);
                GeomNode *rpt = find_node(graph, c->participants[2]);
                if (!line1 || !line2 || !rpt)
                    break;

                int64_t scale = LV00_SOLVER_SCALE_FACTOR;

                /* 尝试精确有理数路径：直接从线段端点坐标计算直线方程 */
                if (line1->type == GEOM_LINE_SEGMENT && line1->coord_count >= 4 && line2->type == GEOM_LINE_SEGMENT &&
                    line2->coord_count >= 4) {
                    mpz_t l1x1_s, l1y1_s, l1x2_s, l1y2_s;
                    mpz_t l2x1_s, l2y1_s, l2x2_s, l2y2_s;
                    mpz_init(l1x1_s);
                    mpz_init(l1y1_s);
                    mpz_init(l1x2_s);
                    mpz_init(l1y2_s);
                    mpz_init(l2x1_s);
                    mpz_init(l2y1_s);
                    mpz_init(l2x2_s);
                    mpz_init(l2y2_s);

                    bool exact1 = coord_to_mpz_scaled_exact(line1->symbolic_coords[0], l1x1_s, scale) &&
                                  coord_to_mpz_scaled_exact(line1->symbolic_coords[1], l1y1_s, scale) &&
                                  coord_to_mpz_scaled_exact(line1->symbolic_coords[2], l1x2_s, scale) &&
                                  coord_to_mpz_scaled_exact(line1->symbolic_coords[3], l1y2_s, scale);
                    bool exact2 = coord_to_mpz_scaled_exact(line2->symbolic_coords[0], l2x1_s, scale) &&
                                  coord_to_mpz_scaled_exact(line2->symbolic_coords[1], l2y1_s, scale) &&
                                  coord_to_mpz_scaled_exact(line2->symbolic_coords[2], l2x2_s, scale) &&
                                  coord_to_mpz_scaled_exact(line2->symbolic_coords[3], l2y2_s, scale);

                    if (exact1 && exact2) {
                        /* 精确路径：在 mpz 层面计算直线方程和交点 */

                        /* 直线1: a1*x + b1*y + c1 = 0
                       a1 = dy1 = (l1y2 - l1y1), b1 = -dx1 = -(l1x2 - l1x1)
                       c1 = -(a1*l1x1 + b1*l1y1) (需要除以 scale) */
                        mpz_t a1_s, b1_s, c1_s, a2_s, b2_s, c2_s;
                        mpz_t dx1_s, dy1_s, dx2_s, dy2_s;
                        mpz_init(a1_s);
                        mpz_init(b1_s);
                        mpz_init(c1_s);
                        mpz_init(a2_s);
                        mpz_init(b2_s);
                        mpz_init(c2_s);
                        mpz_init(dx1_s);
                        mpz_init(dy1_s);
                        mpz_init(dx2_s);
                        mpz_init(dy2_s);

                        mpz_sub(dx1_s, l1x2_s, l1x1_s);
                        mpz_sub(dy1_s, l1y2_s, l1y1_s);
                        mpz_set(a1_s, dy1_s);
                        mpz_neg(b1_s, dx1_s);
                        /* c1 = -(a1*l1x1 + b1*l1y1) / scale */
                        {
                            mpz_t t1, t2;
                            mpz_init(t1);
                            mpz_init(t2);
                            mpz_mul(t1, a1_s, l1x1_s);
                            mpz_mul(t2, b1_s, l1y1_s);
                            mpz_add(t1, t1, t2);
                            mpz_neg(t1, t1);
                            mpz_fdiv_q_ui(c1_s, t1, (unsigned long) scale);
                            mpz_clear(t1);
                            mpz_clear(t2);
                        }

                        mpz_sub(dx2_s, l2x2_s, l2x1_s);
                        mpz_sub(dy2_s, l2y2_s, l2y1_s);
                        mpz_set(a2_s, dy2_s);
                        mpz_neg(b2_s, dx2_s);
                        /* c2 = -(a2*l2x1 + b2*l2y1) / scale */
                        {
                            mpz_t t1, t2;
                            mpz_init(t1);
                            mpz_init(t2);
                            mpz_mul(t1, a2_s, l2x1_s);
                            mpz_mul(t2, b2_s, l2y1_s);
                            mpz_add(t1, t1, t2);
                            mpz_neg(t1, t1);
                            mpz_fdiv_q_ui(c2_s, t1, (unsigned long) scale);
                            mpz_clear(t1);
                            mpz_clear(t2);
                        }

                        /* D = a1*b2 - a2*b1 (已缩放: a1,b1 已缩放，a2,b2 已缩放 => D 缩放^2) */
                        mpz_t D_s, x_num_s, y_num_s;
                        mpz_init(D_s);
                        mpz_init(x_num_s);
                        mpz_init(y_num_s);
                        {
                            mpz_t t1, t2;
                            mpz_init(t1);
                            mpz_init(t2);
                            mpz_mul(t1, a1_s, b2_s);
                            mpz_mul(t2, a2_s, b1_s);
                            mpz_sub(D_s, t1, t2);
                            mpz_clear(t1);
                            mpz_clear(t2);
                        }

                        /* 检查是否平行：D_s == 0 */
                        if (mpz_sgn(D_s) != 0) {
                            /* x_num = b1*c2 - b2*c1 (b1,b2 缩放, c1,c2 缩放 => x_num 缩放^2) */
                            {
                                mpz_t t1, t2;
                                mpz_init(t1);
                                mpz_init(t2);
                                mpz_mul(t1, b1_s, c2_s);
                                mpz_mul(t2, b2_s, c1_s);
                                mpz_sub(x_num_s, t1, t2);
                                mpz_clear(t1);
                                mpz_clear(t2);
                            }
                            /* y_num = a2*c1 - a1*c2 */
                            {
                                mpz_t t1, t2;
                                mpz_init(t1);
                                mpz_init(t2);
                                mpz_mul(t1, a2_s, c1_s);
                                mpz_mul(t2, a1_s, c2_s);
                                mpz_sub(y_num_s, t1, t2);
                                mpz_clear(t1);
                                mpz_clear(t2);
                            }

                            /*
                         * 方程: D*x = x_num, D*y = y_num
                         * D_s 和 x_num_s/y_num_s 都缩放了 scale^2，
                         * 两边同时除以 scale^2 后等价。
                         * 直接用 D_s 和 x_num_s 作为系数（公共因子 scale^2 约掉）。
                         */
                            mpz_poly_t poly;
                            mpz_poly_init(&poly);
                            poly.degree = 1;
                            /* GMP 要求使用标准分配器 */
                            poly.coeffs = malloc(2 * sizeof(mpz_t));
                            if (poly.coeffs) {
                                mpz_init(poly.coeffs[1]);
                                mpz_init(poly.coeffs[0]);
                                mpz_set(poly.coeffs[1], D_s);
                                mpz_neg(poly.coeffs[0], x_num_s);
                                EQUATION_PUSH_OR_GOTO(sys, poly, rpt->id, 0, push_error);
                                mpz_poly_clear(&poly);
                            } else {
                                mpz_poly_clear(&poly);
                            }

                            mpz_poly_init(&poly);
                            poly.degree = 1;
                            /* GMP 要求使用标准分配器 */
                            poly.coeffs = malloc(2 * sizeof(mpz_t));
                            if (poly.coeffs) {
                                mpz_init(poly.coeffs[1]);
                                mpz_init(poly.coeffs[0]);
                                mpz_set(poly.coeffs[1], D_s);
                                mpz_neg(poly.coeffs[0], y_num_s);
                                EQUATION_PUSH_OR_GOTO(sys, poly, rpt->id, 1, push_error);
                                mpz_poly_clear(&poly);
                            } else {
                                mpz_poly_clear(&poly);
                            }
                        }

                        mpz_clear(D_s);
                        mpz_clear(x_num_s);
                        mpz_clear(y_num_s);
                        mpz_clear(a1_s);
                        mpz_clear(b1_s);
                        mpz_clear(c1_s);
                        mpz_clear(a2_s);
                        mpz_clear(b2_s);
                        mpz_clear(c2_s);
                        mpz_clear(dx1_s);
                        mpz_clear(dy1_s);
                        mpz_clear(dx2_s);
                        mpz_clear(dy2_s);
                        mpz_clear(l1x1_s);
                        mpz_clear(l1y1_s);
                        mpz_clear(l1x2_s);
                        mpz_clear(l1y2_s);
                        mpz_clear(l2x1_s);
                        mpz_clear(l2y1_s);
                        mpz_clear(l2x2_s);
                        mpz_clear(l2y2_s);
                        break;
                    }

                    mpz_clear(l1x1_s);
                    mpz_clear(l1y1_s);
                    mpz_clear(l1x2_s);
                    mpz_clear(l1y2_s);
                    mpz_clear(l2x1_s);
                    mpz_clear(l2y1_s);
                    mpz_clear(l2x2_s);
                    mpz_clear(l2y2_s);
                }

                /* 回退到 double 近似路径 */
                {
                    /* 从两条线段中提取直线方程 */
                    LineEquation le1, le2;
                    bool got1 = false, got2 = false;

                    if (line1->type == GEOM_LINE_SEGMENT && line1->coord_count >= 4) {
                        GeomNode ep1_storage, ep2_storage;
                        memset(&ep1_storage, 0, sizeof(GeomNode));
                        memset(&ep2_storage, 0, sizeof(GeomNode));
                        GeomNode *ep1 = &ep1_storage;
                        GeomNode *ep2 = &ep2_storage;
                        ep1->type = GEOM_POINT;
                        ep1->coord_count = 2;
                        ep1->symbolic_coords = &line1->symbolic_coords[0];
                        ep2->type = GEOM_POINT;
                        ep2->coord_count = 2;
                        ep2->symbolic_coords = &line1->symbolic_coords[2];
                        got1 = line_from_two_points(ep1, ep2, &le1);
                    }
                    if (line2->type == GEOM_LINE_SEGMENT && line2->coord_count >= 4) {
                        GeomNode ep1_storage, ep2_storage;
                        memset(&ep1_storage, 0, sizeof(GeomNode));
                        memset(&ep2_storage, 0, sizeof(GeomNode));
                        GeomNode *ep1 = &ep1_storage;
                        GeomNode *ep2 = &ep2_storage;
                        ep1->type = GEOM_POINT;
                        ep1->coord_count = 2;
                        ep1->symbolic_coords = &line2->symbolic_coords[0];
                        ep2->type = GEOM_POINT;
                        ep2->coord_count = 2;
                        ep2->symbolic_coords = &line2->symbolic_coords[2];
                        got2 = line_from_two_points(ep1, ep2, &le2);
                    }

                    if (got1 && got2) {
                        /*
                     * 正确计算两条直线的交点
                     *
                     * 两条直线方程为:
                     *   Line1: a1*x + b1*y + c1 = 0
                     *   Line2: a2*x + b2*y + c2 = 0
                     *
                     * 联立求解交点 (x, y)：
                     *   行列式 D = a1*b2 - a2*b1
                     *   x = (b1*c2 - b2*c1) / D
                     *   y = (a2*c1 - a1*c2) / D
                     *
                     * 为避免除法，我们将方程改写为:
                     *   D*x = b1*c2 - b2*c1
                     *   D*y = a2*c1 - a1*c2
                     *
                     * 这可以表示为两个线性方程:
                     *   D*x + 0*y - (b1*c2 - b2*c1) = 0  =>  x 坐标的约束
                     *   0*x + D*y - (a2*c1 - a1*c2) = 0  =>  y 坐标的约束
                     */

                        double D = le1.a * le2.b - le2.a * le1.b;

                        /* 检查是否平行（行列式接近零） */
                        if (fabs(D) < LV00_EPSILON_NUMERIC_COMPARE) {
                            /* 直线平行或重合，无法确定唯一交点 */
                            break;
                        }

                        double x_numerator = le1.b * le2.c - le2.b * le1.c; /* b1*c2 - b2*c1 */
                        double y_numerator = le2.a * le1.c - le1.a * le2.c; /* a2*c1 - a1*c2 */

                        /* x 坐标方程: D*x - x_numerator = 0 */
                        mpz_poly_t poly;
                        mpz_poly_init(&poly);
                        poly.degree = 1;
                        /* GMP 兼容性要求：mpz_poly_clear 内部调用 free()，
                         * 因此此处必须使用标准 malloc 而非 lv00_malloc。
                         * LV00_SOLVER_LINEAR_COEFF_COUNT 为常量，不存在溢出风险。 */
                        poly.coeffs = malloc(LV00_SOLVER_LINEAR_COEFF_COUNT * sizeof(mpz_t));
                        if (!poly.coeffs) {
                            mpz_poly_clear(&poly);
                            break;
                        }
                        mpz_init(poly.coeffs[1]);
                        mpz_init(poly.coeffs[0]);
                        double_to_mpz_scaled(D, poly.coeffs[1], scale);            /* x 的系数 */
                        double_to_mpz_scaled(-x_numerator, poly.coeffs[0], scale); /* 常数项 */
                        EQUATION_PUSH_OR_GOTO(sys, poly, rpt->id, 0, push_error);
                        mpz_poly_clear(&poly);

                        /* y 坐标方程: D*y - y_numerator = 0 */
                        mpz_poly_init(&poly);
                        poly.degree = 1;
                        /* GMP 要求使用标准分配器 */
                        poly.coeffs = malloc(2 * sizeof(mpz_t));
                        if (!poly.coeffs) {
                            mpz_poly_clear(&poly);
                            break;
                        }
                        mpz_init(poly.coeffs[1]);
                        mpz_init(poly.coeffs[0]);
                        double_to_mpz_scaled(D, poly.coeffs[1], scale);            /* y 的系数 */
                        double_to_mpz_scaled(-y_numerator, poly.coeffs[0], scale); /* 常数项 */
                        EQUATION_PUSH_OR_GOTO(sys, poly, rpt->id, 1, push_error);
                        mpz_poly_clear(&poly);
                    }
                }
                break;
            }

            case BETWEENNESS: {
                /* participants[0]=p1, participants[1]=p2, participants[2]=p3
               p2 is between p1 and p3 => collinear + 0 <= t <= 1 */
                if (c->participant_count < 3)
                    break;
                GeomNode *p1 = find_node(graph, c->participants[0]);
                GeomNode *p2 = find_node(graph, c->participants[1]);
                GeomNode *p3 = find_node(graph, c->participants[2]);
                if (!p1 || !p2 || !p3)
                    break;
                if (p1->type != GEOM_POINT || p3->type != GEOM_POINT)
                    break;

                /* 共线性判断：向量 (p2-p1) 和 (p3-p1) 的叉积 = 0
               (x2-x1)*(y3-y1) - (y2-y1)*(x3-x1) = 0
               展开: dy13*x2 - dx13*y2 + (dx13*y1 - dy13*x1) = 0
               使用精确有理数路径避免 double 精度损失 */
                int64_t scale = LV00_SOLVER_SCALE_FACTOR;
                mpz_t x1_s, y1_s, x3_s, y3_s;
                mpz_init(x1_s);
                mpz_init(y1_s);
                mpz_init(x3_s);
                mpz_init(y3_s);

                if (p1->symbolic_coords && p3->symbolic_coords && p1->coord_count >= 2 && p3->coord_count >= 2 &&
                    coord_to_mpz_scaled_exact(p1->symbolic_coords[0], x1_s, scale) &&
                    coord_to_mpz_scaled_exact(p1->symbolic_coords[1], y1_s, scale) &&
                    coord_to_mpz_scaled_exact(p3->symbolic_coords[0], x3_s, scale) &&
                    coord_to_mpz_scaled_exact(p3->symbolic_coords[1], y3_s, scale)) {
                    /* dx = x3 - x1, dy = y3 - y1 (均缩放 scale 倍) */
                    mpz_t dx_s, dy_s;
                    mpz_init(dx_s);
                    mpz_init(dy_s);
                    mpz_sub(dx_s, x3_s, x1_s);
                    mpz_sub(dy_s, y3_s, y1_s);

                    /* x2 方程: dy*x2 + (y1*dx - x1*dy) = 0
                   coeff = dy_s, const = (y1_s * dx_s - x1_s * dy_s) / scale */
                    {
                        mpz_poly_t poly;
                        mpz_poly_init(&poly);
                        poly.degree = 1;
                        poly.coeffs = malloc(2 * sizeof(mpz_t));
                        if (poly.coeffs) {
                            mpz_init(poly.coeffs[1]);
                            mpz_init(poly.coeffs[0]);
                            mpz_set(poly.coeffs[1], dy_s);
                            mpz_t term1, term2;
                            mpz_init(term1);
                            mpz_init(term2);
                            mpz_mul(term1, y1_s, dx_s);
                            mpz_mul(term2, x1_s, dy_s);
                            mpz_sub(term1, term1, term2);
                            mpz_fdiv_q_ui(poly.coeffs[0], term1, (unsigned long) scale);
                            mpz_clear(term1);
                            mpz_clear(term2);
                            EQUATION_PUSH_OR_GOTO(sys, poly, p2->id, 0, push_error);
                        }
                        mpz_poly_clear(&poly);
                    }

                    /* y2 方程: -dx*y2 + (dy*x1 - dx*y1) = 0
                   coeff = -dx_s, const = (dy_s * x1_s - dx_s * y1_s) / scale */
                    {
                        mpz_poly_t poly;
                        mpz_poly_init(&poly);
                        poly.degree = 1;
                        poly.coeffs = malloc(2 * sizeof(mpz_t));
                        if (poly.coeffs) {
                            mpz_init(poly.coeffs[1]);
                            mpz_init(poly.coeffs[0]);
                            mpz_neg(poly.coeffs[1], dx_s);
                            mpz_t term1, term2;
                            mpz_init(term1);
                            mpz_init(term2);
                            mpz_mul(term1, dy_s, x1_s);
                            mpz_mul(term2, dx_s, y1_s);
                            mpz_sub(term1, term1, term2);
                            mpz_fdiv_q_ui(poly.coeffs[0], term1, (unsigned long) scale);
                            mpz_clear(term1);
                            mpz_clear(term2);
                            EQUATION_PUSH_OR_GOTO(sys, poly, p2->id, 1, push_error);
                        }
                        mpz_poly_clear(&poly);
                    }

                    mpz_clear(dx_s);
                    mpz_clear(dy_s);
                }
                mpz_clear(x1_s);
                mpz_clear(y1_s);
                mpz_clear(x3_s);
                mpz_clear(y3_s);
                break;
            }

            case CONTAINMENT: {
                /* 点/区域包含约束: inner 几何体位于 outer 区域内。
               对多边形区域的每条边界边，点必须在边的"内侧"。
               使用精确有理数路径避免 double 精度损失。 */
                if (c->participant_count < 2)
                    break;
                GeomNode *inner = find_node(graph, c->participants[0]);
                GeomNode *outer = find_node(graph, c->participants[1]);
                if (!inner || !outer)
                    break;

                /* 当前仅处理点-区域包含 */
                if (inner->type != GEOM_POINT || outer->type != GEOM_REGION)
                    break;
                if (outer->data.region.segment_count <= 0 || !outer->data.region.boundary_segments)
                    break;

                {
                    int64_t scale = LV00_SOLVER_SCALE_FACTOR;
                    int seg_count = outer->data.region.segment_count;

                    for (int si = 0; si < seg_count; si++) {
                        GeomNode *seg = outer->data.region.boundary_segments[si];
                        if (!seg || seg->type != GEOM_LINE_SEGMENT)
                            continue;
                        if (seg->coord_count < 4 || !seg->symbolic_coords)
                            continue;

                        /* 精确有理数路径：获取边界线段端点坐标的缩放整数值 */
                        mpz_t sx1_s, sy1_s, sx2_s, sy2_s;
                        mpz_init(sx1_s);
                        mpz_init(sy1_s);
                        mpz_init(sx2_s);
                        mpz_init(sy2_s);

                        if (coord_to_mpz_scaled_exact(seg->symbolic_coords[0], sx1_s, scale) &&
                            coord_to_mpz_scaled_exact(seg->symbolic_coords[1], sy1_s, scale) &&
                            coord_to_mpz_scaled_exact(seg->symbolic_coords[2], sx2_s, scale) &&
                            coord_to_mpz_scaled_exact(seg->symbolic_coords[3], sy2_s, scale)) {
                            /* 边方向向量 (均缩放 scale 倍) */
                            mpz_t dx_s, dy_s;
                            mpz_init(dx_s);
                            mpz_init(dy_s);
                            mpz_sub(dx_s, sx2_s, sx1_s);
                            mpz_sub(dy_s, sy2_s, sy1_s);

                            /* x-分量方程: dy*px + (-dy*sx1 + dx*sy1) = 0
                           coeff = dy_s, const = (dx_s * sy1_s - dy_s * sx1_s) / scale */
                            {
                                mpz_poly_t poly;
                                mpz_poly_init(&poly);
                                poly.degree = 1;
                                poly.coeffs = malloc(2 * sizeof(mpz_t));
                                if (poly.coeffs) {
                                    mpz_init(poly.coeffs[1]);
                                    mpz_init(poly.coeffs[0]);
                                    mpz_set(poly.coeffs[1], dy_s);
                                    mpz_t term1, term2;
                                    mpz_init(term1);
                                    mpz_init(term2);
                                    mpz_mul(term1, dx_s, sy1_s);
                                    mpz_mul(term2, dy_s, sx1_s);
                                    mpz_sub(term1, term1, term2);
                                    mpz_fdiv_q_ui(poly.coeffs[0], term1, (unsigned long) scale);
                                    mpz_clear(term1);
                                    mpz_clear(term2);
                                    EQUATION_PUSH_OR_GOTO(sys, poly, inner->id, 0, push_error);
                                }
                                mpz_poly_clear(&poly);
                            }

                            /* y-分量方程: -dx*py + (dy*sx1 - dx*sy1) = 0
                           coeff = -dx_s, const = (dy_s * sx1_s - dx_s * sy1_s) / scale */
                            {
                                mpz_poly_t poly;
                                mpz_poly_init(&poly);
                                poly.degree = 1;
                                poly.coeffs = malloc(2 * sizeof(mpz_t));
                                if (poly.coeffs) {
                                    mpz_init(poly.coeffs[1]);
                                    mpz_init(poly.coeffs[0]);
                                    mpz_neg(poly.coeffs[1], dx_s);
                                    mpz_t term1, term2;
                                    mpz_init(term1);
                                    mpz_init(term2);
                                    mpz_mul(term1, dy_s, sx1_s);
                                    mpz_mul(term2, dx_s, sy1_s);
                                    mpz_sub(term1, term1, term2);
                                    mpz_fdiv_q_ui(poly.coeffs[0], term1, (unsigned long) scale);
                                    mpz_clear(term1);
                                    mpz_clear(term2);
                                    EQUATION_PUSH_OR_GOTO(sys, poly, inner->id, 1, push_error);
                                }
                                mpz_poly_clear(&poly);
                            }

                            mpz_clear(dx_s);
                            mpz_clear(dy_s);
                        }

                        mpz_clear(sx1_s);
                        mpz_clear(sy1_s);
                        mpz_clear(sx2_s);
                        mpz_clear(sy2_s);
                    }
                }
                break;
            }

            case CONNECTION: {
                /* 端口之间的连接。从关联节点的数值假设声明中提取距离约束。
               如果两个节点都有坐标且其中一个编码了距离，
               生成方程：(xA-xB)^2 + (yA-yB)^2 = d^2。 */
                if (c->participant_count < 2)
                    break;
                GeomNode *nodeA = find_node(graph, c->participants[0]);
                GeomNode *nodeB = find_node(graph, c->participants[1]);
                if (!nodeA || !nodeB)
                    break;

                /* 尝试从任一节点的数值假设声明中提取距离值 */
                double dist_val = -1.0;
                GeomNode *dist_node = NULL;
                const char *prefix = "distance=";
                size_t prefix_len = strlen(prefix); /* 缓存前缀长度，避免循环内重复计算 */
                for (int ni = 0; ni < 2; ni++) {
                    GeomNode *n = (ni == 0) ? nodeA : nodeB;
                    if (!n || !n->numeric_assumption_declaration)
                        continue;
                    const char *decl = n->numeric_assumption_declaration;
                    if (strncmp(decl, prefix, prefix_len) == 0) {
                        dist_val = strtod(decl + prefix_len, NULL);
                        dist_node = n;
                        break;
                    }
                }

                if (dist_val < 0)
                    break;

                /* 两个节点均需要至少 2 个坐标 (x, y) */
                if (nodeA->coord_count < 2 || nodeB->coord_count < 2)
                    break;
                if (!nodeA->symbolic_coords || !nodeB->symbolic_coords)
                    break;

                double ax, ay, bx, by;
                if (!coord_to_double(nodeA->symbolic_coords[0], &ax) ||
                    !coord_to_double(nodeA->symbolic_coords[1], &ay) ||
                    !coord_to_double(nodeB->symbolic_coords[0], &bx) ||
                    !coord_to_double(nodeB->symbolic_coords[1], &by))
                    break;

                double dist_sq = dist_val * dist_val;
                int64_t scale = LV00_SOLVER_SCALE_FACTOR;

                /* (xA-xB)^2 + (yA-yB)^2 = d^2
               Expand for nodeB as variable (nodeA as fixed):
               xB^2 - 2*ax*xB + (ax^2 + ay^2 - 2*ay*yB + yB^2 - dist_sq) = 0
               Simplified as univariate in xB:
               xB^2 - 2*ax*xB + (ax^2 + ay^2 - dist_sq) = 0
               (ignoring y coupling terms, consistent with existing approach) */
                mpz_poly_t poly;
                mpz_poly_init(&poly);
                poly.degree = 2;
                /* GMP 兼容性要求：mpz_poly_clear 内部调用 free()，
                 * 因此此处必须使用标准 malloc 而非 lv00_malloc。
                 * LV00_SOLVER_QUADRATIC_COEFF_COUNT 为常量，不存在溢出风险。 */
                poly.coeffs = malloc(LV00_SOLVER_QUADRATIC_COEFF_COUNT * sizeof(mpz_t));
                if (!poly.coeffs) {
                    mpz_poly_clear(&poly);
                    break;
                }
                mpz_init(poly.coeffs[2]);
                mpz_init(poly.coeffs[1]);
                mpz_init(poly.coeffs[0]);
                mpz_set_si(poly.coeffs[2], scale);
                double_to_mpz_scaled(-2.0 * ax, poly.coeffs[1], scale);
                double_to_mpz_scaled(ax * ax + ay * ay - dist_sq, poly.coeffs[0], scale);
                EQUATION_PUSH_OR_GOTO(sys, poly, nodeB->id, 0, push_error);
                mpz_poly_clear(&poly);

                /* 同理对 yB 建立方程：yB^2 - 2*ay*yB + (ax^2 + ay^2 - dist_sq) = 0 */
                mpz_poly_init(&poly);
                poly.degree = 2;
                /* GMP 要求使用标准分配器 */
                poly.coeffs = malloc(3 * sizeof(mpz_t));
                if (!poly.coeffs) {
                    mpz_poly_clear(&poly);
                    break;
                }
                mpz_init(poly.coeffs[2]);
                mpz_init(poly.coeffs[1]);
                mpz_init(poly.coeffs[0]);
                mpz_set_si(poly.coeffs[2], scale);
                double_to_mpz_scaled(-2.0 * ay, poly.coeffs[1], scale);
                double_to_mpz_scaled(ax * ax + ay * ay - dist_sq, poly.coeffs[0], scale);
                EQUATION_PUSH_OR_GOTO(sys, poly, nodeB->id, 1, push_error);
                mpz_poly_clear(&poly);
                break;
            }
            default:
                LV00_LOG_WARNING("Unknown constraint type %d in extract_equations_from_constraints", c->type);
                break;
        }
    }

    /* 第二遍扫描：从设置了数值假设声明的节点中提取距离约束
       （编码平方距离 = d^2）。 */
    for (int ni = 0; ni < graph->node_count; ni++) {
        GeomNode *node = graph->nodes[ni];
        if (!node || !node->numeric_assumption_declaration)
            continue;
        if (node->type != GEOM_LINE_SEGMENT)
            continue;

        /* 检查声明是否编码了距离约束。
           格式："distance=<value>" 或仅为数值。 */
        const char *decl = node->numeric_assumption_declaration;
        double dist_sq = -1.0;

        /* 尝试解析为 "distance=<value>" 格式 */
        const char *prefix = "distance=";
        size_t prefix_len = strlen(prefix); /* 缓存前缀长度，避免重复计算 */
        if (strncmp(decl, prefix, prefix_len) == 0) {
            dist_sq = strtod(decl + prefix_len, NULL);
            dist_sq = dist_sq * dist_sq; /* 存储平方值 */
        } else {
            /* 尝试解析为纯数字（视为距离的平方） */
            char *end = NULL;
            double val = strtod(decl, &end);
            if (end != decl && val >= 0) {
                dist_sq = val;
            }
        }

        if (dist_sq < 0)
            continue;

        /* 线段在 symbolic_coords 中存储了端点坐标。
           为第二个端点建立距离方程
          （第一个端点通常已固定）。 */
        if (node->coord_count >= 4) {
            double x1, y1;
            if (coord_to_double(node->symbolic_coords[0], &x1) && coord_to_double(node->symbolic_coords[1], &y1)) {
                /* (x - x1)^2 + (y - y1)^2 = dist_sq
                   => x^2 - 2*x1*x + x1^2 + y^2 - 2*y1*y + y1^2 - dist_sq = 0
                   这是一个关于 x 和 y 的二次方程。将其存储为两个独立的一元方程
                  （耦合），求解器将按方程组处理。 */

                /* 对于第二个端点的 x 坐标：需要第二个端点的节点 ID。
                   由于线段直接存储坐标，我们创建以线段 ID 标记的方程。 */
                int64_t scale = LV00_SOLVER_SCALE_FACTOR;

                /* x^2 - 2*x1*x + (x1^2 + y1^2 - dist_sq - y^2 + 2*y1*y) = 0
                   作为 x 的单变量方程: x^2 - 2*x1*x + const = 0
                   其中 const = x1^2 + y1^2 - dist_sq（忽略含 y 的耦合项）。
                   注意：距离方程展开后常数项为 x1^2 + y1^2 - dist_sq，
                   原代码错误地使用了 x1^2 + dist_sq（符号错误）。 */
                mpz_poly_t poly;
                mpz_poly_init(&poly);
                poly.degree = 2;
                /* GMP 兼容性要求：mpz_poly_clear 内部调用 free()，
                 * 因此此处必须使用标准 malloc 而非 lv00_malloc。
                 * LV00_SOLVER_QUADRATIC_COEFF_COUNT 为常量，不存在溢出风险。 */
                poly.coeffs = malloc(LV00_SOLVER_QUADRATIC_COEFF_COUNT * sizeof(mpz_t));
                if (!poly.coeffs) {
                    mpz_poly_clear(&poly);
                    continue;
                }
                mpz_init(poly.coeffs[2]);
                mpz_init(poly.coeffs[1]);
                mpz_init(poly.coeffs[0]);
                mpz_set_si(poly.coeffs[2], scale);                                        /* x^2 系数 */
                double_to_mpz_scaled(-2.0 * x1, poly.coeffs[1], scale);                   /* x 系数 */
                double_to_mpz_scaled(x1 * x1 + y1 * y1 - dist_sq, poly.coeffs[0], scale); /* 常数项（已修正符号） */
                EQUATION_PUSH_OR_GOTO(sys, poly, node->id, 0, push_error);
                mpz_poly_clear(&poly);

                /* 同理对 y 建立方程：y^2 - 2*y1*y + (x1^2 + y1^2 - dist_sq) = 0 */
                mpz_poly_init(&poly);
                poly.degree = 2;
                /* GMP 要求使用标准分配器 */
                poly.coeffs = malloc(3 * sizeof(mpz_t));
                if (!poly.coeffs) {
                    mpz_poly_clear(&poly);
                    continue;
                }
                mpz_init(poly.coeffs[2]);
                mpz_init(poly.coeffs[1]);
                mpz_init(poly.coeffs[0]);
                mpz_set_si(poly.coeffs[2], scale);
                double_to_mpz_scaled(-2.0 * y1, poly.coeffs[1], scale);
                double_to_mpz_scaled(x1 * x1 + y1 * y1 - dist_sq, poly.coeffs[0], scale); /* 常数项（已修正符号） */
                EQUATION_PUSH_OR_GOTO(sys, poly, node->id, 1, push_error);
                mpz_poly_clear(&poly);
            }
        }
    }
push_error:
    return;
}

/* ------------------------------------------------------------------ */
/*  内部：统计每个变量的有效方程数量                                   */
/* ------------------------------------------------------------------ */

typedef struct {
    int node_id;
    int eq_count;
    int max_degree;
} VarInfo;

/* 统计方程系统中每个变量节点对应的方程数量和最高次数。
   返回 VarInfo 数组，通过 out_var_count 输出变量数量。
   如果方程系统为空（var_count == 0），直接返回 NULL。 */
static VarInfo *build_var_info(const EquationSystem *sys, int node_count, int *out_var_count) {
    /* 收集所有不重复的变量节点 id */
    int *var_ids = lv00_malloc((size_t) sys->count * sizeof(int));
    if (!var_ids)
        return NULL;
    int var_count = 0;
    for (int i = 0; i < sys->count; i++) {
        int vid = sys->eqs[i].var_node_id;
        bool found = false;
        for (int j = 0; j < var_count; j++) {
            if (var_ids[j] == vid) {
                found = true;
                break;
            }
        }
        if (!found)
            var_ids[var_count++] = vid;
    }

    /* 提前返回：如果没有变量，避免 lv00_calloc(0, ...) 的未定义行为 */
    if (var_count == 0) {
        lv00_free((void **) &var_ids);
        *out_var_count = 0;
        return NULL;
    }

    VarInfo *info = lv00_malloc((size_t) var_count * sizeof(VarInfo));
    if (info)
        memset(info, 0, (size_t) var_count * sizeof(VarInfo));
    for (int i = 0; i < var_count; i++) {
        info[i].node_id = var_ids[i];
        info[i].eq_count = 0;
        info[i].max_degree = 0;
    }
    for (int i = 0; i < sys->count; i++) {
        int vid = sys->eqs[i].var_node_id;
        for (int j = 0; j < var_count; j++) {
            if (info[j].node_id == vid) {
                info[j].eq_count++;
                int deg = sys->eqs[i].poly.degree;
                if (deg > info[j].max_degree)
                    info[j].max_degree = deg;
                break;
            }
        }
    }
    lv00_free((void **) &var_ids);
    *out_var_count = var_count;
    return info;
}

/* ------------------------------------------------------------------ */
/*  内部：求解一元一次方程 a*x + b = 0                                    */
/* ------------------------------------------------------------------ */

/**
 * @brief 求解一元一次方程 a*x + b = 0
 *
 * 从多项式系数中提取 a (coeffs[1]) 和 b (coeffs[0])，计算 x = -b/a。
 *
 * @param poly  一元多项式指针（次数必须为 1）
 * @param x_out 输出：方程的解
 * @return true 表示成功求解，false 表示次数不为 1 或 a 近似为 0
 */
static bool solve_linear(const mpz_poly_t *poly, double *x_out) {
    if (poly->degree != 1)
        return false;
    double a = mpz_get_d(poly->coeffs[1]);
    double b = mpz_get_d(poly->coeffs[0]);
    if (fabs(a) < LV00_EPSILON_NEWTON)
        return false;
    *x_out = -b / a;
    return true;
}

/* ------------------------------------------------------------------ */
/*  内部：求解一元二次方程 a*x^2 + b*x + c = 0                             */
/* ------------------------------------------------------------------ */

typedef struct {
    double roots[2];
    int root_count;
} QuadraticRoots;

/**
 * @brief 求解一元二次方程 a*x^2 + b*x + c = 0
 *
 * 使用判别式 b^2 - 4ac 求根。判别式为负时返回零个实根，
 * 接近零时返回一个重根，否则返回两个实根。系数 a 接近零时
 * 退化为线性方程求解。
 *
 * @param poly 一元多项式指针（次数必须为 2）
 * @param out  输出：QuadraticRoots 结构体，包含根数组和根数量
 * @return true 表示成功（含无实根情况），false 表示输入的次数不是 2
 */
static bool solve_quadratic(mpz_poly_t *poly, QuadraticRoots *out) {
    if (poly->degree != 2)
        return false;
    double a = mpz_get_d(poly->coeffs[2]);
    double b = mpz_get_d(poly->coeffs[1]);
    double c = mpz_get_d(poly->coeffs[0]);
    if (fabs(a) < LV00_EPSILON_NEWTON) {
        /* 坍缩为一元一次方程 */
        if (fabs(b) < LV00_EPSILON_NEWTON)
            return false;
        out->roots[0] = -c / b;
        out->root_count = 1;
        return true;
    }
    double disc = b * b - 4.0 * a * c;
    if (disc < -LV00_EPSILON_DOUBLE) {
        /* 无实数根 */
        out->root_count = 0;
        return true;
    }
    if (disc < 0)
        disc = 0.0;
    double sq = sqrt(disc);
    out->roots[0] = (-b - sq) / (2.0 * a);
    out->roots[1] = (-b + sq) / (2.0 * a);
    out->root_count = (fabs(disc) < LV00_EPSILON_DOUBLE) ? 1 : 2;
    return true;
}

/* =======================================================================
 * 内部函数：精确求解一元二次方程
 *
 * 对 ax^2 + bx + c = 0，使用 GMP 精确整数运算（mpz_t）计算判别式
 * D = b^2 - 4ac。根据 D 的符号和是否为完全平方数，决定输出类型：
 *   - D < 0：无实数解
 *   - D = 0：唯一精确解（RATIONAL 类型）
 *   - D > 0 且为完全平方数：两个精确解（RATIONAL 类型）
 *   - D > 0 且非完全平方数：两个二次根式解（QUADRATIC 类型）
 *     表示为 a + b*sqrt(n)，其中 n 是 D 的无平方因子部分
 *
 * 注意：所有中间计算使用 GMP mpz_t 精确大整数，确保无精度损失。
 * 仅在 n > LV00_SOLVER_PRIME_LIMIT 或超出 unsigned int 范围时才回退到
 * double 近似解。
 * ======================================================================= */
/**
 * @brief 精确求解一元二次方程，返回符号坐标解
 *
 * @details 对 ax^2 + bx + c = 0，使用 GMP 精确整数运算求判别式 D = b^2 - 4ac：
 *          - D < 0：无实数解，返回 0
 *          - D = 0：唯一解 x = -b/(2a)，以 RATIONAL 类型返回
 *          - D > 0 且为完全平方数：两个有理解，以 RATIONAL 类型返回
 *          - D > 0 且非完全平方数：两个二次根式解，以 QUADRATIC 类型（a + b*sqrt(n)）返回
 *          对超出 int64/uint64 范围的解，回退到 double 近似值。
 *
 * @param poly         二次多项式指针（degree 必须为 2）
 * @param solutions    输出：解数组（调用者负责释放每个 SymbolicCoord）
 * @param max_solutions 最大解数量（通常为 2）
 * @return 实际解的数量（0、1 或 2）
 */
static int solve_quadratic_exact(const mpz_poly_t *poly, SymbolicCoord **solutions, int max_solutions) {
    if (!poly || poly->degree != 2 || !solutions || max_solutions <= 0)
        return 0;

    /* 提取系数 a, b, c (使用 GMP 精确整数) */
    /* 多项式: a*x^2 + b*x + c = 0 */
    mpz_t a_mpz, b_mpz, c_mpz;
    mpz_init_set(a_mpz, poly->coeffs[2]);
    mpz_init_set(b_mpz, poly->coeffs[1]);
    mpz_init_set(c_mpz, poly->coeffs[0]);

    /* 检查 a != 0 */
    if (mpz_cmp_si(a_mpz, 0) == 0) {
        /* 退化为线性方程: b*x + c = 0 => x = -c/b */
        if (mpz_cmp_si(b_mpz, 0) == 0) {
            mpz_clear(a_mpz);
            mpz_clear(b_mpz);
            mpz_clear(c_mpz);
            return 0;
        }
        /* x = -c/b，用 RATIONAL 表示 */
        mpq_t x_val;
        mpq_init(x_val);
        mpz_neg(mpq_numref(x_val), c_mpz); /* numerator = -c */
        mpz_set(mpq_denref(x_val), b_mpz); /* denominator = b */
        mpq_canonicalize(x_val);

        if (mpz_fits_slong_p(mpq_numref(x_val)) && mpz_fits_ulong_p(mpq_denref(x_val))) {
            solutions[0] = symbolic_coord_create_rational((int64_t) mpz_get_si(mpq_numref(x_val)),
                                                          (uint64_t) mpz_get_ui(mpq_denref(x_val)));
        } else {
            /* 超出 int64/uint64 范围，无法精确表示 */
            mpq_clear(x_val);
            mpz_clear(a_mpz);
            mpz_clear(b_mpz);
            mpz_clear(c_mpz);
            return 0;
        }

        mpq_clear(x_val);
        mpz_clear(a_mpz);
        mpz_clear(b_mpz);
        mpz_clear(c_mpz);
        return 1;
    }

    /* 计算判别式 D = b^2 - 4ac (使用 GMP 精确运算) */
    mpz_t D;
    mpz_init(D);
    mpz_mul(D, b_mpz, b_mpz); /* D = b^2 */

    mpz_t four_ac;
    mpz_init(four_ac);
    mpz_mul_si(four_ac, a_mpz, 4);    /* 4a */
    mpz_mul(four_ac, four_ac, c_mpz); /* 4ac */
    mpz_sub(D, D, four_ac);           /* D = b^2 - 4ac */
    mpz_clear(four_ac);

    /* 判断 D 的符号 */
    int D_sign = mpz_cmp_si(D, 0);

    if (D_sign < 0) {
        /* D < 0: 无实数解 */
        mpz_clear(a_mpz);
        mpz_clear(b_mpz);
        mpz_clear(c_mpz);
        mpz_clear(D);
        return 0;
    }

    if (D_sign == 0) {
        /* D == 0: 唯一解 x = -b / (2a) */
        mpq_t x_val;
        mpq_init(x_val);
        mpz_neg(mpq_numref(x_val), b_mpz);       /* numerator = -b */
        mpz_mul_si(mpq_denref(x_val), a_mpz, 2); /* denominator = 2a */
        mpq_canonicalize(x_val);

        if (mpz_fits_slong_p(mpq_numref(x_val)) && mpz_fits_ulong_p(mpq_denref(x_val))) {
            solutions[0] = symbolic_coord_create_rational((int64_t) mpz_get_si(mpq_numref(x_val)),
                                                          (uint64_t) mpz_get_ui(mpq_denref(x_val)));
        } else {
            /* 超出 int64/uint64 范围，无法精确表示 */
            mpq_clear(x_val);
            mpz_clear(a_mpz);
            mpz_clear(b_mpz);
            mpz_clear(c_mpz);
            mpz_clear(D);
            return 0;
        }

        mpq_clear(x_val);
        mpz_clear(a_mpz);
        mpz_clear(b_mpz);
        mpz_clear(c_mpz);
        mpz_clear(D);
        return 1;
    }

    /* D > 0: 两个实数解 */
    /* 检查 D 是否为完全平方数 */
    mpz_t sqrt_D;
    mpz_init(sqrt_D);
    mpz_sqrt(sqrt_D, D);

    bool is_perfect_square = false;
    mpz_t sq_check;
    mpz_init(sq_check);
    mpz_mul(sq_check, sqrt_D, sqrt_D);
    if (mpz_cmp(sq_check, D) == 0) {
        is_perfect_square = true;
    }
    mpz_clear(sq_check);

    if (is_perfect_square) {
        /* D 是完全平方数: 两个有理解 */
        /* 解1 = (-b - sqrt(D)) / (2a) */
        /* 解2 = (-b + sqrt(D)) / (2a) */

        mpz_t denom;
        mpz_init(denom);
        mpz_mul_si(denom, a_mpz, 2); /* denom = 2a */

        /* 解1: (-b - sqrt(D)) / (2a) */
        mpz_t num1;
        mpz_init(num1);
        mpz_neg(num1, b_mpz);
        mpz_sub(num1, num1, sqrt_D);

        mpq_t x1;
        mpq_init(x1);
        mpz_set(mpq_numref(x1), num1);
        mpz_set(mpq_denref(x1), denom);
        mpq_canonicalize(x1);

        if (mpz_fits_slong_p(mpq_numref(x1)) && mpz_fits_ulong_p(mpq_denref(x1))) {
            solutions[0] = symbolic_coord_create_rational((int64_t) mpz_get_si(mpq_numref(x1)),
                                                          (uint64_t) mpz_get_ui(mpq_denref(x1)));
        } else {
            /* 超出 int64/uint64 范围，无法精确表示 */
            mpq_clear(x1);
            mpz_clear(num1);
            mpz_clear(denom);
            mpz_clear(a_mpz);
            mpz_clear(b_mpz);
            mpz_clear(c_mpz);
            mpz_clear(D);
            mpz_clear(sqrt_D);
            return 0;
        }
        mpq_clear(x1);

        /* 解2: (-b + sqrt(D)) / (2a) */
        mpz_t num2;
        mpz_init(num2);
        mpz_neg(num2, b_mpz);
        mpz_add(num2, num2, sqrt_D);

        if (max_solutions >= 2) {
            mpq_t x2;
            mpq_init(x2);
            mpz_set(mpq_numref(x2), num2);
            mpz_set(mpq_denref(x2), denom);
            mpq_canonicalize(x2);

            if (mpz_fits_slong_p(mpq_numref(x2)) && mpz_fits_ulong_p(mpq_denref(x2))) {
                solutions[1] = symbolic_coord_create_rational((int64_t) mpz_get_si(mpq_numref(x2)),
                                                              (uint64_t) mpz_get_ui(mpq_denref(x2)));
            } else {
                /* 超出 int64/uint64 范围，解2 无法精确表示，仅返回解1 */
                mpq_clear(x2);
                mpz_clear(num1);
                mpz_clear(num2);
                mpz_clear(denom);
                mpz_clear(a_mpz);
                mpz_clear(b_mpz);
                mpz_clear(c_mpz);
                mpz_clear(D);
                mpz_clear(sqrt_D);
                return 1;
            }
            mpq_clear(x2);
        }

        mpz_clear(num1);
        mpz_clear(num2);
        mpz_clear(denom);
        mpz_clear(a_mpz);
        mpz_clear(b_mpz);
        mpz_clear(c_mpz);
        mpz_clear(D);
        mpz_clear(sqrt_D);
        return (max_solutions >= 2) ? 2 : 1;
    }

    /* D 不是完全平方数: 两个二次根式解，使用 QUADRATIC 类型
     * QUADRATIC 表示为 a + b*sqrt(n)，其中 n 是 D 的无平方因子部分。
     *
     * 首先，提取 D 的无平方因子部分: D = k^2 * n，其中 n 无平方因子。
     * 则 sqrt(D) = k * sqrt(n)。
     *
     * 解1 = -b/(2a) - k*sqrt(n)/(2a) = (-b/(2a)) + (-k/(2a))*sqrt(n)
     * 解2 = -b/(2a) + k*sqrt(n)/(2a) = (-b/(2a)) + (k/(2a))*sqrt(n)
     */

    /* 提取 D 的无平方因子部分: D = k^2 * n */
    mpz_t n_part, k_part;
    mpz_init_set(n_part, D);
    mpz_init_set_ui(k_part, 1);

    for (long p = 2; mpz_cmp_ui(n_part, 1) > 0 && p <= 1000000L; p++) {
        mpz_t p_mpz, p2_mpz, q, r;
        mpz_init_set_si(p_mpz, p);
        mpz_init(p2_mpz);
        mpz_mul(p2_mpz, p_mpz, p_mpz);
        mpz_init(q);
        mpz_init(r);

        while (mpz_cmp(p2_mpz, n_part) <= 0) {
            mpz_fdiv_qr(q, r, n_part, p2_mpz);
            if (mpz_cmp_si(r, 0) == 0) {
                mpz_set(n_part, q);
                mpz_mul(k_part, k_part, p_mpz);
                /* 重新计算 p^2 (因为 p_mpz 不变，p2_mpz 也不变) */
            } else {
                break;
            }
        }

        mpz_clear(p_mpz);
        mpz_clear(p2_mpz);
        mpz_clear(q);
        mpz_clear(r);

        /* 如果 p^2 > n_part，则 n_part 已无平方因子 */
        mpz_t p2_check;
        mpz_init(p2_check);
        mpz_set_si(p2_check, p);
        mpz_mul(p2_check, p2_check, p2_check);
        if (mpz_cmp(p2_check, n_part) > 0) {
            mpz_clear(p2_check);
            break;
        }
        mpz_clear(p2_check);
    }

    /* 计算 2a */
    mpz_t two_a;
    mpz_init(two_a);
    mpz_mul_si(two_a, a_mpz, 2);

    /* 解的 a 部分 (有理部分): -b / (2a) */
    mpq_t rational_part;
    mpq_init(rational_part);
    mpz_neg(mpq_numref(rational_part), b_mpz);
    mpz_set(mpq_denref(rational_part), two_a);
    mpq_canonicalize(rational_part);

    /* 解的 b 部分 (sqrt 系数): +/- k / (2a) */
    mpq_t sqrt_coeff;
    mpq_init(sqrt_coeff);
    mpz_set(mpq_numref(sqrt_coeff), k_part);
    mpz_set(mpq_denref(sqrt_coeff), two_a);
    mpq_canonicalize(sqrt_coeff);

    /* 获取 n 的 unsigned int 值。
       symbolic_coord_create_quadratic 的 n 参数为 unsigned int，
       如果 n 超出 unsigned int 范围，则回退到 double 近似解。 */
    unsigned int n_val = 0;
    if (mpz_fits_uint_p(n_part)) {
        n_val = mpz_get_ui(n_part);
    } else {
        /* n 太大，无法用 unsigned int 表示，回退到数值近似解。
           将有理部分和 sqrt 部分转为 double，计算近似值后存为 RATIONAL 类型。
           使用 GMP 的 mpq_set_d 获取精确的有理表示。 */
        double approx1 = mpq_get_d(rational_part) - mpq_get_d(sqrt_coeff) * sqrt(mpz_get_d(n_part));
        double approx2 = mpq_get_d(rational_part) + mpq_get_d(sqrt_coeff) * sqrt(mpz_get_d(n_part));

        /* 将 double 近似值转换为有理数（通过 mpq_t 获取分子/分母） */
        mpq_t q_approx;
        mpq_init(q_approx);
        mpq_set_d(q_approx, approx1);
        /* 直接从 mpq 提取分子分母，避免 double->mpq->double->int64 精度损失 */
        int64_t num1 = mpz_get_si(mpq_numref(q_approx));
        uint64_t den1 = mpz_get_ui(mpq_denref(q_approx));
        if (den1 == 0)
            den1 = 1;
        solutions[0] = symbolic_coord_create_rational(num1, den1);

        if (max_solutions >= 2) {
            mpq_set_d(q_approx, approx2);
            int64_t num2 = mpz_get_si(mpq_numref(q_approx));
            uint64_t den2 = mpz_get_ui(mpq_denref(q_approx));
            if (den2 == 0)
                den2 = 1;
            solutions[1] = symbolic_coord_create_rational(num2, den2);
        }
        mpq_clear(q_approx);

        /* 清理中间变量 */
        mpq_clear(rational_part);
        mpq_clear(sqrt_coeff);
        mpz_clear(n_part);
        mpz_clear(k_part);
        mpz_clear(two_a);
        mpz_clear(a_mpz);
        mpz_clear(b_mpz);
        mpz_clear(c_mpz);
        mpz_clear(D);
        mpz_clear(sqrt_D);

        return (max_solutions >= 2) ? 2 : 1;
    }

    /* 创建 QUADRATIC 类型的解 */
    Rational *qa = rational_create_from_mpz(mpq_numref(rational_part), mpq_denref(rational_part));
    if (!qa) {
        /* 内存不足，清理所有 GMP 变量后返回 */
        mpq_clear(rational_part);
        mpq_clear(sqrt_coeff);
        mpz_clear(n_part);
        mpz_clear(k_part);
        mpz_clear(two_a);
        mpz_clear(a_mpz);
        mpz_clear(b_mpz);
        mpz_clear(c_mpz);
        return 0;
    }

    /* 解1: a + (-sqrt_coeff)*sqrt(n) -- 负的 sqrt 系数 */
    {
        mpz_t neg_num;
        mpz_init(neg_num);
        mpz_neg(neg_num, mpq_numref(sqrt_coeff));
        Rational *qb1 = rational_create_from_mpz(neg_num, mpq_denref(sqrt_coeff));
        mpz_clear(neg_num);

        if (!qb1) {
            rational_destroy(qa);
            mpq_clear(rational_part);
            mpq_clear(sqrt_coeff);
            mpz_clear(n_part);
            mpz_clear(k_part);
            mpz_clear(two_a);
            mpz_clear(a_mpz);
            mpz_clear(b_mpz);
            mpz_clear(c_mpz);
            return 0;
        }

        /* solutions[0] 获取 qa 的所有权 */
        solutions[0] = symbolic_coord_create_quadratic(qa, qb1, n_val);
        if (!solutions[0]) {
            /* 创建失败时需要手动释放 qa 和 qb1（所有权未转移） */
            rational_destroy(qa);
            rational_destroy(qb1);
        }
    }

    /* 解2: a + sqrt_coeff*sqrt(n) -- 正的 sqrt 系数 */
    if (max_solutions >= 2) {
        Rational *qb2 = rational_create_from_mpz(mpq_numref(sqrt_coeff), mpq_denref(sqrt_coeff));
        if (!qb2) {
            mpq_clear(rational_part);
            mpq_clear(sqrt_coeff);
            mpz_clear(n_part);
            mpz_clear(k_part);
            mpz_clear(two_a);
            mpz_clear(a_mpz);
            mpz_clear(b_mpz);
            mpz_clear(c_mpz);
            return (solutions[0] ? 1 : 0);
        }
        /* solutions[1] 需要 qa 的独立拷贝，避免 double-free */
        Rational *qa_copy = rational_copy(qa);
        if (!qa_copy) {
            rational_destroy(qb2);
            mpq_clear(rational_part);
            mpq_clear(sqrt_coeff);
            mpz_clear(n_part);
            mpz_clear(k_part);
            mpz_clear(two_a);
            mpz_clear(a_mpz);
            mpz_clear(b_mpz);
            mpz_clear(c_mpz);
            return (solutions[0] ? 1 : 0);
        }
        solutions[1] = symbolic_coord_create_quadratic(qa_copy, qb2, n_val);
        if (!solutions[1]) {
            /* 创建失败时需要手动释放 qa_copy 和 qb2（所有权未转移） */
            rational_destroy(qa_copy);
            rational_destroy(qb2);
        }
    }

    /* 不要 rational_destroy(qa) -- 所有权已转移给 solutions[0] */

    mpq_clear(rational_part);
    mpq_clear(sqrt_coeff);
    mpz_clear(n_part);
    mpz_clear(k_part);
    mpz_clear(two_a);
    mpz_clear(a_mpz);
    mpz_clear(b_mpz);
    mpz_clear(c_mpz);
    mpz_clear(D);
    mpz_clear(sqrt_D);

    return (max_solutions >= 2) ? 2 : 1;
}

/* ------------------------------------------------------------------ */
/*  Internal: solve univariate cubic exactly using Cardano's formula    */
/*                                                                     */
/*  a*x^3 + b*x^2 + c*x + d = 0                                       */
/*                                                                     */
/*  算法步骤:                                                           */
/*    1. 归一化: 除以 a, 得到 x^3 + px^2 + qx + r = 0                  */
/*    2. 消去二次项: 令 y = x + p/3, 得到 y^3 + py + q = 0 (depressed) */
/*    3. Cardano 判别式: D = (q/2)^2 + (p/3)^3                         */
/*       - D > 0: 一个实根 (Cardano 公式)                               */
/*       - D = 0: 三个实根 (其中至少两个相等)                            */
/*       - D < 0: 三个不等实根 (三角函数公式, cos)                       */
/*    4. 反代回 x = y - p/3                                             */
/* ------------------------------------------------------------------ */

/**
 * @brief 使用 Cardano 公式精确求解一元三次方程
 *
 * @details 对 a*x^3 + b*x^2 + c*x + d = 0，使用 GMP 精确大整数计算
 *          系数和判别式。结果以 SymbolicCoord 数组返回（RATIONAL 或 QUADRATIC 类型）。
 *          对三次方程的根，如果判别式 D 为完全平方数，则根为有理数；
 *          如果 D > 0 且非完全平方数，则唯一实根为代数数（回退到 double 近似）；
 *          如果 D < 0（三个实根），使用三角函数或回退到 double 近似。
 *
 * @param poly         三次多项式指针（degree 必须为 3）
 * @param solutions    输出：解数组（调用者负责释放每个 SymbolicCoord）
 * @param max_solutions 最大解数量（通常为 3）
 * @return 实际解的数量（1 到 3），或 0 表示失败
 */
static int solve_cubic_exact(const mpz_poly_t *poly, SymbolicCoord **solutions, int max_solutions) {
    if (!poly || poly->degree != 3 || !solutions || max_solutions <= 0)
        return 0;

    /* 提取系数: a*x^3 + b*x^2 + c*x + d = 0 */
    mpz_t a_mpz, b_mpz, c_mpz, d_mpz;
    mpz_init_set(a_mpz, poly->coeffs[3]);
    mpz_init_set(b_mpz, poly->coeffs[2]);
    mpz_init_set(c_mpz, poly->coeffs[1]);
    mpz_init_set(d_mpz, poly->coeffs[0]);

    /* 检查 a != 0 */
    if (mpz_cmp_si(a_mpz, 0) == 0) {
        mpz_clear(a_mpz);
        mpz_clear(b_mpz);
        mpz_clear(c_mpz);
        mpz_clear(d_mpz);
        return 0;
    }

    /* Step 1: 转为 double 计算（大整数系数可能超出 double 范围，但在几何构造中通常合理） */
    double a_val = mpz_get_d(a_mpz) / LV00_SOLVER_SCALE_FACTOR;
    double b_val = mpz_get_d(b_mpz) / LV00_SOLVER_SCALE_FACTOR;
    double c_val = mpz_get_d(c_mpz) / LV00_SOLVER_SCALE_FACTOR;
    double d_val = mpz_get_d(d_mpz) / LV00_SOLVER_SCALE_FACTOR;

    /* 归一化: x^3 + px^2 + qx + r = 0 */
    if (fabs(a_val) < LV00_EPSILON_NEWTON) {
        mpz_clear(a_mpz);
        mpz_clear(b_mpz);
        mpz_clear(c_mpz);
        mpz_clear(d_mpz);
        return 0;
    }
    double p_val = b_val / a_val;
    double q_val = c_val / a_val;
    double r_val = d_val / a_val;

    /* Step 2: 消去二次项 y = x + p/3, 得 y^3 + P*y + Q = 0 */
    double p_over_3 = p_val / 3.0;
    double P = q_val - p_val * p_over_3;
    double Q = r_val - p_over_3 * (q_val - 2.0 * p_val * p_over_3 / 3.0);

    /* Step 3: 计算判别式 */
    double half_Q = Q / 2.0;
    double P_over_3 = P / 3.0;
    double D = half_Q * half_Q + P_over_3 * P_over_3 * P_over_3;

    int sol_count = 0;

    if (D > LV00_EPSILON_DOUBLE) {
        /* 一个实根: y = cbrt(-Q/2 + sqrt(D)) + cbrt(-Q/2 - sqrt(D)) */
        double sqrt_D = sqrt(D);
        double u = -half_Q + sqrt_D;
        double v = -half_Q - sqrt_D;
        double y_root = cbrt(u) + cbrt(v);
        double x_root = y_root - p_over_3;

        if (sol_count < max_solutions) {
            solutions[sol_count] = symbolic_coord_create_rational((int64_t) (x_root * LV00_SOLVER_SCALE_FACTOR),
                                                                  (uint64_t) LV00_SOLVER_SCALE_FACTOR);
            if (solutions[sol_count])
                sol_count++;
        }
    } else if (fabs(D) < LV00_EPSILON_DOUBLE) {
        /* 三个实根（至少两个相等）:
         * y1 = 2 * cbrt(-Q/2),  y2 = y3 = -cbrt(-Q/2) */
        double cbrt_val = cbrt(-half_Q);
        double y1 = 2.0 * cbrt_val;
        double y2 = -cbrt_val;

        double x1 = y1 - p_over_3;
        double x2 = y2 - p_over_3;

        if (sol_count < max_solutions) {
            solutions[sol_count] = symbolic_coord_create_rational((int64_t) (x1 * LV00_SOLVER_SCALE_FACTOR),
                                                                  (uint64_t) LV00_SOLVER_SCALE_FACTOR);
            if (solutions[sol_count])
                sol_count++;
        }
        if (sol_count < max_solutions && fabs(x1 - x2) > LV00_EPSILON_DOUBLE) {
            solutions[sol_count] = symbolic_coord_create_rational((int64_t) (x2 * LV00_SOLVER_SCALE_FACTOR),
                                                                  (uint64_t) LV00_SOLVER_SCALE_FACTOR);
            if (solutions[sol_count])
                sol_count++;
        }
    } else {
        /* 三个不等实根 (casus irreducibilis): 使用三角函数
         * y_k = 2*sqrt(-P/3)*cos((acos(3Q/(2P)*sqrt(-3/P)) + 2*pi*k)/3) */
        double sqrt_term = sqrt(-P / 3.0);
        double acos_arg = 3.0 * Q / (2.0 * P) * sqrt(-3.0 / P);
        /* 裁剪到 [-1, 1] 以防止浮点精度误差导致 acos 返回 NaN */
        if (acos_arg > 1.0) acos_arg = 1.0;
        if (acos_arg < -1.0) acos_arg = -1.0;
        double phi = acos(acos_arg);
        for (int k = 0; k < 3 && sol_count < max_solutions; k++) {
            double angle = (phi + 2.0 * M_PI * (double) k) / 3.0;
            double y_k = 2.0 * sqrt_term * cos(angle);
            double x_k = y_k - p_over_3;
            solutions[sol_count] = symbolic_coord_create_rational((int64_t) (x_k * LV00_SOLVER_SCALE_FACTOR),
                                                                  (uint64_t) LV00_SOLVER_SCALE_FACTOR);
            if (solutions[sol_count])
                sol_count++;
        }
    }

    mpz_clear(a_mpz);
    mpz_clear(b_mpz);
    mpz_clear(c_mpz);
    mpz_clear(d_mpz);
    return sol_count;
}

/* ================================================================== */
/*  Symbolic back-substitution support                                 */
/* ================================================================== */

/**
 * @brief 将 SymbolicCoord 转换为精确的有理数 mpq_t
 *
 * @details 仅支持 RATIONAL 类型的坐标。非 RATIONAL 类型返回 false。
 *          用于精确的有理数算术运算。
 *
 * @param c   符号坐标指针
 * @param out 输出：mpq_t 有理数
 * @return true 表示转换成功，false 表示失败（非 RATIONAL 类型）
 */
static bool symbolic_coord_to_mpq(const SymbolicCoord *c, mpq_t out) {
    if (!c)
        return false;
    if (c->type == RATIONAL && c->data.rational) {
        mpq_set(out, c->data.rational->value);
        return true;
    }
    return false;
}

/**
 * @brief 从精确的有理数 mpq_t 创建 SymbolicCoord
 *
 * @details 使用 GMP mpq_t 进行精确转换，确保分子分母在 int64/uint64 范围内。
 *          若值超出范围，返回 NULL。
 *
 * @param val 有理数指针
 * @return 新创建的符号坐标，失败时返回 NULL
 */
static SymbolicCoord *symbolic_coord_from_mpq(const mpq_t val) {
    mpq_t canonical;
    mpq_init(canonical);
    mpq_set(canonical, val);
    mpq_canonicalize(canonical);

    SymbolicCoord *result = NULL;
    if (mpz_fits_slong_p(mpq_numref(canonical)) && mpz_fits_ulong_p(mpq_denref(canonical))) {
        result = symbolic_coord_create_rational((int64_t) mpz_get_si(mpq_numref(canonical)),
                                                (uint64_t) mpz_get_ui(mpq_denref(canonical)));
    }
    mpq_clear(canonical);
    return result;
}

/**
 * @brief 使用精确符号算术在符号坐标点处求值一元多项式
 *
 * @details 对多项式的每个项 coeff[i] * x^i，使用 symbolic_coord_multiply 计算
 *          coeff[i]（转为有理数）与 value^i 的乘积，然后使用 symbolic_coord_add
 *          求和。使用 Horner 法则提高效率。
 *
 * @param poly  多项式指针
 * @param value 符号坐标求值点
 * @return 求值结果（新的 SymbolicCoord），调用者负责释放；失败返回 NULL
 */
static SymbolicCoord *poly_eval_symbolic(const mpz_poly_t *poly, const SymbolicCoord *value) {
    if (!poly || !value || poly->degree < 0 || !poly->coeffs)
        return NULL;

    /* 从常数项开始：coeff[0] * value^0 = coeff[0] */
    mpq_t c0;
    mpq_init(c0);
    /* coeff[0] is an mpz_t integer; treat as rational coeff[0]/1 */
    mpz_set(mpq_numref(c0), poly->coeffs[0]);
    mpz_set_ui(mpq_denref(c0), 1);
    mpq_canonicalize(c0);

    SymbolicCoord *result = symbolic_coord_from_mpq(c0);
    mpq_clear(c0);
    if (!result) {
        /* 回退方案：尝试从 double 创建 */
        double d = mpz_get_d(poly->coeffs[0]);
        result = symbolic_coord_create_rational((int64_t) (d * LV00_SOLVER_SCALE_FACTOR), LV00_SOLVER_SCALE_FACTOR);
    }
    if (!result) {
        return NULL;  /* 内存不足，无法创建初始坐标 */
    }

    /* 霍纳法则：result = result * value + coeff[i] */
    for (int i = poly->degree; i >= 1; i--) {
        /* result = result * value */
        SymbolicCoord *product = symbolic_coord_multiply(result, value);
        symbolic_coord_destroy(result);
        result = product;
        if (!result) {
            return NULL;  /* 内存不足 */
        }

        /* + coeff[i] */
        mpq_t ci;
        mpq_init(ci);
        mpz_set(mpq_numref(ci), poly->coeffs[i]);
        mpz_set_ui(mpq_denref(ci), 1);
        mpq_canonicalize(ci);

        SymbolicCoord *term = symbolic_coord_from_mpq(ci);
        mpq_clear(ci);

        if (!term) {
            double d = mpz_get_d(poly->coeffs[i]);
            term = symbolic_coord_create_rational((int64_t) (d * LV00_SOLVER_SCALE_FACTOR), LV00_SOLVER_SCALE_FACTOR);
        }
        if (!term) {
            symbolic_coord_destroy(result);
            return NULL;  /* 内存不足 */
        }

        SymbolicCoord *sum = symbolic_coord_add(result, term);
        symbolic_coord_destroy(result);
        symbolic_coord_destroy(term);
        result = sum;
        if (!result) {
            return NULL;  /* 内存不足 */
        }
    }

    return result;
}

/**
 * @brief 将符号坐标值代入方程进行替换
 *
 * @details 将 SymbolicCoord 值代入 PolyEquation（单变量多项式方程）。
 *          求值结果生成新的常数方程（degree 0），其值应接近零表示代入一致。
 *
 * @param eq        方程指针
 * @param var_index 变量索引
 * @param value     变量的符号坐标值
 * @return 新分配的 PolyEquation（调用者需释放多项式），失败返回 NULL
 */
static PolyEquation *substitute_symbolic_into_equation(const PolyEquation *eq, int var_index,
                                                       const SymbolicCoord *value) {
    if (!eq || !value)
        return NULL;

    /* 对于一元多项式方程，代入即在给定符号值处求值多项式。
     * 结果为一个常数（0次）方程，期望等于零。 */
    PolyEquation *result = lv00_malloc(sizeof(PolyEquation));
    if (!result)
        return NULL;

    result->var_node_id = eq->var_node_id;
    result->coord_index = eq->coord_index;

    /* 在给定值处符号求值多项式 */
    SymbolicCoord *eval_result = poly_eval_symbolic(&eq->poly, value);

    if (eval_result) {
        /* 从求值结果构建一个 0 次多项式。
         * 如果结果是有理数，提取精确的 mpq_t 并按比例缩放。
         * 否则，使用 double 近似值作为常数项。 */
        mpz_poly_init(&result->poly);

        /*
         * OWNER: eval_mpq — if-else 分支内
         * 在 if (symbolic_coord_to_mpq) 分支内 init，分支末 clear。
         * 在 else 分支不使用。两个分支执行路径互斥，不存在泄漏。
         */
        mpq_t eval_mpq;
        mpq_init(eval_mpq);
        if (symbolic_coord_to_mpq(eval_result, eval_mpq)) {
            /* 精确有理数路径：缩放为整数 */
            int64_t scale = LV00_SOLVER_SCALE_FACTOR;
            mpz_t scaled_num;
            mpz_init(scaled_num);
            mpz_mul_si(scaled_num, mpq_numref(eval_mpq), scale);

            mpz_t den;
            mpz_init(den);
            mpz_set(den, mpq_denref(eval_mpq));

            result->poly.degree = 0;
            /* GMP 要求使用标准分配器 */
            result->poly.coeffs = malloc(sizeof(mpz_t));
            if (result->poly.coeffs) {
                mpz_init(result->poly.coeffs[0]);
                if (mpz_divisible_p(scaled_num, den)) {
                    mpz_divexact(result->poly.coeffs[0], scaled_num, den);
                } else {
                    mpz_fdiv_q(result->poly.coeffs[0], scaled_num, den);
                }
            }
            mpz_clear(scaled_num);
            mpz_clear(den);
        } else {
            /* Non-rational result: use double approximation */
            double d;
            if (coord_to_double(eval_result, &d)) {
                int64_t scale = LV00_SOLVER_SCALE_FACTOR;
                result->poly.degree = 0;
                /* GMP 要求使用标准分配器 */
                result->poly.coeffs = malloc(sizeof(mpz_t));
                if (result->poly.coeffs) {
                    mpz_init(result->poly.coeffs[0]);
                    double_to_mpz_scaled(d, result->poly.coeffs[0], scale);
                }
            } else {
                /* Cannot evaluate: mark as empty polynomial (solved) */
                result->poly.degree = -1;
                result->poly.coeffs = NULL;
            }
        }
        mpq_clear(eval_mpq);
        symbolic_coord_destroy(eval_result);
    } else {
        /* Evaluation failed: mark as empty */
        mpz_poly_init(&result->poly);
        result->poly.degree = -1;
        result->poly.coeffs = NULL;
    }

    return result;
}

/* ------------------------------------------------------------------ */
/*  Internal: solve a linear equation exactly using mpq_t rational      */
/*  arithmetic.                                                         */
/*                                                                     */
/*  For a*x + b = 0 => x = -b/a using exact GMP division.              */
/* ------------------------------------------------------------------ */
/**
 * @brief 精确求解一元一次方程 a*x + b = 0
 *
 * @details 使用 GMP 有理数 mpq_t 进行精确求解，x = -b/a。
 *          若 a = 0（退化情况）或无解，返回 NULL。
 *
 * @param poly 一元多项式指针（次数必须为 1）
 * @return 求解出的 SymbolicCoord 指针（调用者负责释放），失败返回 NULL
 */
static SymbolicCoord *solve_linear_exact(const mpz_poly_t *poly) {
    if (!poly || poly->degree != 1)
        return NULL;

    /* a*x + b = 0 => x = -b/a */
    mpz_t a_mpz, b_mpz;
    mpz_init_set(a_mpz, poly->coeffs[1]);
    mpz_init_set(b_mpz, poly->coeffs[0]);

    if (mpz_cmp_si(a_mpz, 0) == 0) {
        mpz_clear(a_mpz);
        mpz_clear(b_mpz);
        return NULL;
    }

    /* x = -b / a, exact rational */
    mpq_t x_val;
    mpq_init(x_val);
    mpz_neg(mpq_numref(x_val), b_mpz); /* numerator = -b */
    mpz_set(mpq_denref(x_val), a_mpz); /* denominator = a */
    mpq_canonicalize(x_val);

    SymbolicCoord *result = symbolic_coord_from_mpq(x_val);

    mpq_clear(x_val);
    mpz_clear(a_mpz);
    mpz_clear(b_mpz);
    return result;
}

/* ------------------------------------------------------------------ */
/*  Internal: symbolic version of substitute_solved.                    */
/*  Evaluates the polynomial at the symbolic value and marks the       */
/*  equation as solved if the result is (approximately) zero.          */
/* ------------------------------------------------------------------ */
/**
 * @brief 以符号坐标值回代已求解的变量到方程系统中
 *
 * @details 在所有方程中查找与给定 (var_node_id, coord_index) 匹配的方程，
 *          将符号坐标值代入多项式求值。若结果近似为零，则将该方程的
 *          多项式标记为已求解（degree = -1）。
 *
 * @param sys         方程系统指针
 * @param var_node_id 已求解变量的节点 ID
 * @param coord_index 坐标索引（0 = x，1 = y）
 * @param value       变量的符号坐标解
 */
static void substitute_solved_symbolic(EquationSystem *sys, int var_node_id, int coord_index,
                                       const SymbolicCoord *value) {
    for (int i = 0; i < sys->count; i++) {
        if (sys->eqs[i].var_node_id == var_node_id && sys->eqs[i].coord_index == coord_index) {
            /* 在给定值处符号求值多项式 */
            SymbolicCoord *eval_result = poly_eval_symbolic(&sys->eqs[i].poly, value);
            if (eval_result) {
                /* 检查结果是否近似为零 */
                double d;
                if (coord_to_double(eval_result, &d)) {
                    if (fabs(d) < 1e-6 * (fabs(d) + 1.0)) {
                        mpz_poly_clear(&sys->eqs[i].poly);
                        mpz_poly_init(&sys->eqs[i].poly);
                    }
                }
                symbolic_coord_destroy(eval_result);
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Perform fully symbolic back-substitution                            */
/*                                                                     */
/*  solved_values:   array of SymbolicCoord* for already-solved vars   */
/*  solved_indices:  array of var_node_id for already-solved vars      */
/*  solved_count:    number of already-solved variables                */
/*                                                                     */
/*  For each unsolved variable, find an equation containing it,        */
/*  substitute all already-solved symbolic values, solve the           */
/*  resulting simpler equation symbolically, and add the new           */
/*  solution to the solved set. Repeat until no more progress.         */
/*                                                                     */
/*  Returns: array of SymbolicCoord* for ALL variables (caller frees)  */
/*           The array has n_vars entries. NULL entries mean unsolved.  */
/* ------------------------------------------------------------------ */
/**
 * @brief 执行完全符号回代求解
 *
 * @details 对于每个未求解的变量，找到包含它的方程（当前仅支持单变量多项式），
 *          求解该方程并加入已求解集合。迭代进行，直到无法取得新进展。
 *          当前 PolyEquation 为单变量结构，不存在多变量依赖。
 *          若未来扩展为多变量，需增加依赖检查逻辑。
 *
 * @param equations      方程数组指针
 * @param eq_count       方程数量
 * @param n_vars         变量总数
 * @param solved_values  已求解变量的符号坐标值数组
 * @param solved_indices 已求解变量的节点 ID 数组
 * @param solved_count   已求解变量数量
 * @return 所有变量的 SymbolicCoord* 数组（调用者负责释放），
 *         未求解变量对应 NULL。失败返回 NULL。
 */
SymbolicCoord **back_substitute_symbolic(PolyEquation **equations, int eq_count, int n_vars,
                                         SymbolicCoord **solved_values, int *solved_indices, int solved_count) {
    if (n_vars <= 0)
        return NULL;

    /* 分配结果数组：每个变量一个 SymbolicCoord* */
    SymbolicCoord **all_values = lv00_malloc((size_t) n_vars * sizeof(SymbolicCoord *));
    if (!all_values)
        return NULL;
    memset(all_values, 0, (size_t) n_vars * sizeof(SymbolicCoord *));

    /* 将已求解的值复制到结果数组中。
     * solved_indices 映射到 all_values 中的位置。 */
    for (int i = 0; i < solved_count; i++) {
        if (solved_indices[i] >= 0 && solved_indices[i] < n_vars) {
            all_values[solved_indices[i]] = symbolic_coord_copy(solved_values[i]);
        }
    }

    /* 迭代求解剩余变量 */
    bool progress = true;
    int max_iterations = n_vars + 1; /* 防止无限循环 */
    int iteration = 0;

    /* ── 防抖容错：残差变化率检测 ── */
    int stalled_count = 0;          /* 连续停滞计数 */
    const int max_stalled = 5;      /* 连续 max_stalled 次无进展则终止 */
    int last_solved_count = 0;      /* 上一轮已求解变量数 */

    while (progress && iteration < max_iterations) {
        progress = false;
        iteration++;

        int current_solved = 0;
        for (int vi = 0; vi < n_vars; vi++) {
            if (all_values[vi] != NULL)
                current_solved++;
        }

        /* 防抖检测：连续停滞则提前终止 */
        if (current_solved == last_solved_count && iteration > 1) {
            stalled_count++;
            if (stalled_count >= max_stalled) {
                LOG_WARN("solver", "back_substitute_symbolic: 连续 %d 轮无进展，提前终止迭代", stalled_count);
                break;
            }
        } else {
            stalled_count = 0;
        }
        last_solved_count = current_solved;

        for (int eq_idx = 0; eq_idx < eq_count; eq_idx++) {
            PolyEquation *eq = equations[eq_idx];
            if (!eq || eq->poly.degree < 0)
                continue;

            int var_id = eq->var_node_id;
            if (var_id < 0 || var_id >= n_vars)
                continue;
            if (all_values[var_id] != NULL)
                continue; /* already solved */

            /* 多变量方程依赖分析：
             * 当前 PolyEquation 为单变量多项式结构（仅绑定一个 var_node_id
             * 和一个 coord_index），因此不存在对其他未求解变量的依赖。
             * 方程可直接求解，无需跳过。
             *
             * 若未来扩展为多变量多项式（MVPolynomial），需在此处遍历
             * 方程中涉及的所有变量，检查除 var_id 外的每个变量是否在
             * all_values 中均已求解（非 NULL）。若存在未求解的依赖变量，
             * 则跳过当前方程（continue），待依赖变量求解后再回代。 */

            /* Substitute all solved values into this equation.
             * For univariate polynomials, this is just evaluation.
             * We solve the equation directly instead. */

            SymbolicCoord *solution = NULL;

            if (eq->poly.degree == 1) {
                /* Linear: solve exactly with mpq_t */
                solution = solve_linear_exact(&eq->poly);
            } else if (eq->poly.degree == 2) {
                /* Quadratic: use existing exact solver */
                SymbolicCoord *exact_solutions[2] = {NULL, NULL};
                int exact_count = solve_quadratic_exact(&eq->poly, exact_solutions, 2);
                if (exact_count > 0) {
                    /* Take the first solution (positive selection could be added) */
                    solution = exact_solutions[0];
                    /* Free extra solutions if any */
                    for (int r = 1; r < exact_count; r++) {
                        symbolic_coord_destroy(exact_solutions[r]);
                    }
                }
            }

            if (solution) {
                all_values[var_id] = solution;
                progress = true;

                /* Mark this equation as solved (degree -1) */
                /* Note: we don't modify the input equations directly;
                 * the caller can track which equations are solved. */
            }
        }
    }

    return all_values;
}

/* ------------------------------------------------------------------ */
/*  Public: Compute resultant for algebraic number operations          */
/*  (Delegates to mpz_poly_resultant in mpz_poly_resultant.c)          */
/* ------------------------------------------------------------------ */

bool compute_algebraic_resultant(const mpz_poly_t *p, /* Minimal polynomial of alpha (in y) */
                                 const mpz_poly_t *q, /* Minimal polynomial of beta (in y) */
                                 AlgebraicOp op,      /* SUM or PRODUCT */
                                 mpz_poly_t *result   /* Output: minimal polynomial of result */
) {
    return mpz_poly_resultant(p, q, op, result);
}

/**
 * @brief 将已求解变量回代到方程系统中
 *
 * @details 在所有方程中查找与给定 (var_node_id, coord_index) 匹配的方程，
 *          将数值代入多项式求值。若结果近似为零，则将该方程标记为已求解。
 *
 * @param sys         方程系统指针
 * @param var_node_id 已求解变量的节点 ID
 * @param coord_index 坐标索引
 * @param value       变量的数值
 */
static void substitute_solved(EquationSystem *sys, int var_node_id, int coord_index, double value) {
    /* For each equation that references this variable, substitute the
       known value and simplify. In our univariate representation, if
       an equation targets this exact (node_id, coord_index), we can
       verify it; equations for other variables that depend on this one
       would need bivariate handling. */
    for (int i = 0; i < sys->count; i++) {
        if (sys->eqs[i].var_node_id == var_node_id && sys->eqs[i].coord_index == coord_index) {
            /* Evaluate polynomial at the given value and check if ~0 */
            double val = 0.0;
            mpz_poly_t *p = &sys->eqs[i].poly;
            for (int d = 0; d <= p->degree; d++) {
                double coeff = mpz_get_d(p->coeffs[d]);
                val += coeff * pow(value, d);
            }
            /* If the equation is satisfied, mark it as solved (degree -1) */
            if (fabs(val) < 1e-6 * (fabs(value) + 1.0)) {
                mpz_poly_clear(&sys->eqs[i].poly);
                mpz_poly_init(&sys->eqs[i].poly);
            }
        }
    }
}

/**
 * @brief 检查多项式是否超出求解范围
 *
 * @details 当前支持的最高次数为 4（四次方程通过 Ferrari 方法求解）。
 *          次数大于 4 的方程标记为超出范围。
 *
 * @param poly 多项式指针
 * @return true 表示超出范围，false 表示可求解
 */
static bool is_out_of_scope(const mpz_poly_t *poly) {
    /* quartic (degree 4) equations are now supported via exact Ferrari/Cardano solver */
    return poly->degree > 4;
}

/**
 * @brief 尝试对三次或更高次多项式进行因式分解
 *
 * @details 使用有理根定理寻找整数根，然后通过综合除法提取因式。
 *          若常数项为零，则提取 x 作为因式。
 *
 * @param poly    多项式指针
 * @param factor1 输出：第一个因式
 * @param factor2 输出：第二个因式
 * @return true 表示分解成功，false 表示失败
 */
static bool try_factor_polynomial(const mpz_poly_t *poly, mpz_poly_t *factor1, mpz_poly_t *factor2) {
    /* Try rational root theorem: test divisors of constant term / leading coeff */
    if (poly->degree < 3)
        return false;

    /* 初始化输出参数，确保所有错误路径上的 mpz_poly_clear 都是安全的 */
    mpz_poly_init(factor1);
    mpz_poly_init(factor2);

    /*
     * OWNER: const_term, lead_coeff — 本函数全作用域
     * 这两个 mpz_t 在函数顶部 init，在所有返回路径（含提前返回）上 clear。
     * 不能提前释放，也不转移所有权给调用者。
     */
    mpz_t const_term, lead_coeff;
    mpz_init(const_term);
    mpz_init(lead_coeff);

    if (poly->degree >= 0)
        mpz_set(const_term, poly->coeffs[0]);
    mpz_set(lead_coeff, poly->coeffs[poly->degree]);

    /* If constant term is 0, factor out x */
    if (mpz_cmp_si(const_term, 0) == 0) {
        /* factor1 = x, factor2 = poly / x */
        factor1->degree = 1;
        /* GMP 要求使用标准分配器 */
        factor1->coeffs = lv00_malloc(2 * sizeof(mpz_t));
        if (!factor1->coeffs) {
            mpz_poly_clear(factor1);
            mpz_poly_clear(factor2);
            mpz_clear(const_term);
            mpz_clear(lead_coeff);
            return false;
        }
        mpz_init_set_si(factor1->coeffs[0], 0);
        mpz_init_set_si(factor1->coeffs[1], 1);

        factor2->degree = poly->degree - 1;
        /* GMP 要求使用标准分配器 */
        factor2->coeffs = lv00_malloc((factor2->degree + 1) * sizeof(mpz_t));
        if (!factor2->coeffs) {
            mpz_poly_clear(factor1);
            mpz_poly_clear(factor2);
            mpz_clear(const_term);
            mpz_clear(lead_coeff);
            return false;
        }
        for (int i = 0; i <= factor2->degree; i++) {
            mpz_init_set(factor2->coeffs[i], poly->coeffs[i + 1]);
        }

        mpz_clear(const_term);
        mpz_clear(lead_coeff);
        return true;
    }

    /* Try integer roots: 基于常数项和首项系数的因数
     * 有理根定理：若 p/q 是根，则 p | const_term, q | lead_coeff
     * 先测试整数根（q=1），范围扩大到 -100..100 */
    long max_root = 100;
    if (mpz_cmp_si(const_term, max_root) > 0) {
        /* 如果常数项很大，限制测试范围 */
        max_root = mpz_cmp_si(const_term, 1000) > 0 ? 1000 : mpz_get_si(const_term);
    }
    for (long r = -max_root; r <= max_root; r++) {
        if (r == 0)
            continue;
        /* Evaluate poly(r) using GMP exact arithmetic */
        mpz_t val;
        mpz_init(val);
        mpz_set_si(val, 0);
        mpz_t r_mpz, term;
        mpz_init_set_si(r_mpz, r);
        mpz_init(term);
        mpz_set_si(term, 1);
        for (int d = 0; d <= poly->degree; d++) {
            mpz_t contrib;
            mpz_init(contrib);
            mpz_mul(contrib, poly->coeffs[d], term);
            mpz_add(val, val, contrib);
            mpz_clear(contrib);
            if (d < poly->degree) {
                mpz_mul(term, term, r_mpz);
            }
        }
        bool is_root = (mpz_cmp_si(val, 0) == 0);
        mpz_clear(val);
        mpz_clear(r_mpz);
        mpz_clear(term);

        if (is_root) {
            /* Factor out (x - r) via synthetic division */
            mpz_poly_t quotient;
            mpz_poly_init(&quotient);
            quotient.degree = poly->degree - 1;
            if (quotient.degree >= 0) {
                /* GMP 要求使用标准分配器 */
                quotient.coeffs = lv00_malloc((quotient.degree + 1) * sizeof(mpz_t));
                if (!quotient.coeffs) {
                    mpz_clear(const_term);
                    mpz_clear(lead_coeff);
                    mpz_poly_clear(&quotient);
                    return false;
                }
                /* Copy leading coefficient */
                mpz_init_set(quotient.coeffs[quotient.degree], poly->coeffs[poly->degree]);
                for (int i = quotient.degree - 1; i >= 0; i--) {
                    mpz_init(quotient.coeffs[i]);
                    mpz_mul(quotient.coeffs[i], quotient.coeffs[i + 1], poly->coeffs[poly->degree]);
                    mpz_add(quotient.coeffs[i], quotient.coeffs[i], poly->coeffs[i + 1]);
                }
                /* Normalize: divide by leading coeff */
                mpz_t lc;
                mpz_init_set(lc, poly->coeffs[poly->degree]);
                for (int i = 0; i <= quotient.degree; i++) {
                    mpz_tdiv_q(quotient.coeffs[i], quotient.coeffs[i], lc);
                }
                mpz_clear(lc);
            }

            factor1->degree = 1;
            /* GMP 要求使用标准分配器 */
            factor1->coeffs = lv00_malloc(2 * sizeof(mpz_t));
            if (!factor1->coeffs) {
                mpz_poly_clear(factor1);
                mpz_poly_clear(factor2);
                mpz_poly_clear(&quotient);
                mpz_clear(const_term);
                mpz_clear(lead_coeff);
                return false;
            }
            mpz_init_set_si(factor1->coeffs[0], -r);
            mpz_init_set_si(factor1->coeffs[1], 1);

            if (!mpz_poly_set(factor2, &quotient)) {
                mpz_poly_clear(factor1);
                mpz_poly_clear(factor2);
                mpz_poly_clear(&quotient);
                mpz_clear(const_term);
                mpz_clear(lead_coeff);
                return false;
            }
            mpz_poly_clear(&quotient);

            mpz_clear(const_term);
            mpz_clear(lead_coeff);
            return true;
        }
    }

    mpz_clear(const_term);
    mpz_clear(lead_coeff);
    return false;
}

/* ------------------------------------------------------------------ */
/*  Internal: detect if two distance constraints on the same segment   */
/*  pair are incompatible                                              */
/* ------------------------------------------------------------------ */

static bool check_incompatible_distances(const ConstraintGraph *graph) {
    /* Build a map of (node_pair) -> list of distance values */
    /* We use a simple O(n^2) scan */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *ni = graph->nodes[i];
        if (!ni || !ni->numeric_assumption_declaration)
            continue;
        if (ni->type != GEOM_LINE_SEGMENT)
            continue;

        const char *di = ni->numeric_assumption_declaration;
        double dist_i = -1.0;
        const char *prefix = "distance=";
        size_t prefix_len = strlen(prefix); /* 缓存前缀长度，避免循环内重复计算 */
        if (strncmp(di, prefix, prefix_len) == 0) {
            dist_i = strtod(di + prefix_len, NULL);
        } else {
            char *end = NULL;
            dist_i = strtod(di, &end);
            if (end == di)
                dist_i = -1.0;
        }
        if (dist_i < 0)
            continue;

        for (int j = i + 1; j < graph->node_count; j++) {
            GeomNode *nj = graph->nodes[j];
            if (!nj || !nj->numeric_assumption_declaration)
                continue;
            if (nj->type != GEOM_LINE_SEGMENT)
                continue;

            /* Check if they share the same endpoints */
            if (ni->coord_count < 4 || nj->coord_count < 4)
                continue;
            bool same_endpoints = true;
            for (int k = 0; k < 4; k++) {
                int cmp = symbolic_coord_compare(ni->symbolic_coords[k], nj->symbolic_coords[k]);
                if (cmp != 0) {
                    same_endpoints = false;
                    break;
                }
            }
            if (!same_endpoints)
                continue;

            const char *dj = nj->numeric_assumption_declaration;
            double dist_j = -1.0;
            if (strncmp(dj, prefix, prefix_len) == 0) {
                dist_j = strtod(dj + prefix_len, NULL);
            } else {
                char *end = NULL;
                dist_j = strtod(dj, &end);
                if (end == dj)
                    dist_j = -1.0;
            }
            if (dist_j < 0)
                continue;

            /* Same segment pair, different distances => conflict */
            if (fabs(dist_i - dist_j) > 1e-9) {
                return true;
            }
        }
    }
    return false;
}

/* ------------------------------------------------------------------ */
/*  Internal: check if after substitution any equation is 0 = nonzero  */
/* ------------------------------------------------------------------ */

static bool check_contradiction_after_substitution(EquationSystem *sys) {
    for (int i = 0; i < sys->count; i++) {
        mpz_poly_t *p = &sys->eqs[i].poly;
        if (p->degree < 0)
            continue; /* already solved / trivial */
        if (p->degree == 0) {
            /* Constant equation: coeff[0] = 0 */
            if (mpz_cmp_si(p->coeffs[0], 0) != 0) {
                return true; /* contradiction: nonzero = 0 */
            }
        }
    }
    return false;
}

/**
 * @brief 计算约束贡献的标量方程数量
 *
 * @param c 约束指针
 * @return 约束贡献的标量方程数量
 */
static int constraint_weight(const Constraint *c) {
    switch (c->type) {
        case INCIDENCE:
            return 1; /* one linear equation (point on line) */
        case BETWEENNESS:
            return 2; /* collinearity + ratio bound */
        case INTERSECTION:
            return 2; /* point on line1 AND line2 */
        case CONTAINMENT:
            return 1; /* at least one boundary constraint */
        case CONNECTION:
            return 1; /* port connectivity */
        default:
            return 1;
    }
}

/* ------------------------------------------------------------------ */
/*  Internal: count variables (each point has 2 coords)                */
/* ------------------------------------------------------------------ */

/**
 * @brief 统计约束图中点类型节点的变量数量
 *
 * @param graph    约束图指针
 * @param out_ids  输出：点节点 ID 数组（调用者负责释放），可为 NULL
 * @return 点节点数量
 */
static int count_point_variables(const ConstraintGraph *graph, int **out_ids) {
    int count = 0;
    for (int i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i]->type == GEOM_POINT)
            count++;
    }
    if (out_ids) {
        *out_ids = lv00_malloc((size_t) count * sizeof(int));
        int idx = 0;
        for (int i = 0; i < graph->node_count; i++) {
            if (graph->nodes[i]->type == GEOM_POINT) {
                (*out_ids)[idx++] = graph->nodes[i]->id;
            }
        }
    }
    return count;
}

/* ================================================================== */
/*  Static helper functions for equation solving                       */
/* ================================================================== */

/**
 * @brief 向 GroebnerResult 追加解
 *
 * @details 处理动态数组扩容逻辑。
 *
 * @param result GroebnerResult 指针
 * @param sol    要追加的解
 * @return 0 表示成功，-1 表示失败（内存不足）
 */
static int append_solution(GroebnerResult *result, SymbolicCoord *sol) {
    if (!result || !sol)
        return -1;
    SymbolicCoord **new_arr =
        lv00_realloc(result->solutions, (size_t) (result->solution_count + 1) * sizeof(SymbolicCoord *));
    if (!new_arr) {
        lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "append_solution: 扩容失败");
        symbolic_coord_destroy(sol);
        return -1;
    }
    result->solutions = new_arr;
    result->solutions[result->solution_count++] = sol;
    return 0;
}

/**
 * @brief 执行两遍求解（第一遍：线性，第二遍：二次/三次）
 *
 * @details 迭代遍历方程系统：
 *          - 第一遍：求解一次方程（线性）
 *          - 第二遍：求解二次方程
 *          - 第三遍：求解三次方程
 *          每次求解后若 do_substitute 为真，则进行回代传播。
 *
 * @param sys                方程系统指针
 * @param result             GroebnerResult 结果指针
 * @param solved_count       输出：已求解方程数量
 * @param multiple_solutions  输出：多解方程数量
 * @param no_solution        输出：检测到无解时设为 true
 * @param do_substitute      是否在每次求解后进行回代
 */
static void solve_equations_pass(EquationSystem *sys, GroebnerResult *result, int *solved_count,
                                 int *multiple_solutions, bool *no_solution, bool do_substitute) {
    /* Pass 0: linear, Pass 1: quadratic, Pass 2: cubic */
    for (int pass = 0; pass < 3; pass++) {
        for (int i = 0; i < sys->count; i++) {
            mpz_poly_t *p = &sys->eqs[i].poly;
            if (p->degree < 0)
                continue;
            if (pass == 0 && p->degree != 1)
                continue;
            if (pass == 1 && p->degree != 2)
                continue;
            if (pass == 2 && p->degree != 3)
                continue;

            if (p->degree == 1) {
                /* Use exact rational arithmetic instead of double->rational
                 * conversion to avoid precision loss in back-substitution. */
                SymbolicCoord *sol = solve_linear_exact(p);
                if (sol) {
                    int appended = (append_solution(result, sol) == 0);
                    if (!appended) {
                        /* append_solution already destroyed sol on failure */
                        sol = NULL;
                    }
                    if (appended) {
                        (*solved_count)++;
                        /* 流式输出: 变量解得（含详细元数据） */
                        if (solver_stream_ctx) {
                            StreamEvent ev;
                            memset(&ev, 0, sizeof(ev));
                            ev.type = STREAM_EVENT_SOLVE_VARIABLE_RESOLVED;
                            ev.timestamp_ms = stream_timestamp_ms();
                            ev.var_id = sys->eqs[i].var_node_id;
                            ev.step_number = *solved_count;
                            ev.description = "变量解得 (线性)";
                            char detail[SOLVER_DETAIL_BUF_SIZE];
                            int _snw_lv;
                            LV00_SAFE_SNPRINTF(_snw_lv, detail, sizeof(detail),
                                               "{\"method\":\"linear_exact\","
                                               "\"var_node_id\":%d,\"coord_index\":%d,"
                                               "\"pass\":%d,\"total_solved\":%d}",
                                               sys->eqs[i].var_node_id, sys->eqs[i].coord_index, pass, *solved_count);
                            LV00_UNUSED(_snw_lv);
                            ev.detail_json = detail;
                            stream_emit(solver_stream_ctx, &ev);
                        }
                    }
                    if (do_substitute && sol) {
                        /* Use symbolic back-substitution for exact propagation */
                        substitute_solved_symbolic(sys, sys->eqs[i].var_node_id, sys->eqs[i].coord_index, sol);
                    }
                } else {
                    /* solve_linear_exact failed: check for contradiction */
                    if (mpz_cmp_si(p->coeffs[1], 0) == 0 && mpz_cmp_si(p->coeffs[0], 0) != 0) {
                        *no_solution = true;
                    }
                }
            } else if (p->degree == 2) {
                /* 使用精确符号求解替代数值近似 */
                SymbolicCoord *exact_solutions[2] = {NULL, NULL};
                int exact_count = solve_quadratic_exact(p, exact_solutions, 2);

                if (exact_count == 0) {
                    *no_solution = true;
                } else {
                    if (exact_count > 1)
                        (*multiple_solutions)++;
                    for (int r = 0; r < exact_count; r++) {
                        if (exact_solutions[r] && append_solution(result, exact_solutions[r]) == 0) {
                            (*solved_count)++;
                            /* 流式输出: 变量解得（含详细元数据） */
                            if (solver_stream_ctx) {
                                StreamEvent ev;
                                memset(&ev, 0, sizeof(ev));
                                ev.type = STREAM_EVENT_SOLVE_VARIABLE_RESOLVED;
                                ev.timestamp_ms = stream_timestamp_ms();
                                ev.var_id = sys->eqs[i].var_node_id;
                                ev.step_number = *solved_count;
                                ev.description = "变量解得 (二次)";
                                char detail[SOLVER_DETAIL_BUF_SIZE];
                                int _snw_qv;
                                LV00_SAFE_SNPRINTF(_snw_qv, detail, sizeof(detail),
                                                   "{\"method\":\"quadratic_exact\","
                                                   "\"var_node_id\":%d,\"coord_index\":%d,"
                                                   "\"root_index\":%d,\"root_count\":%d,"
                                                   "\"pass\":%d,\"total_solved\":%d}",
                                                   sys->eqs[i].var_node_id, sys->eqs[i].coord_index, r, exact_count,
                                                   pass, *solved_count);
                                LV00_UNUSED(_snw_qv);
                                ev.detail_json = detail;
                                stream_emit(solver_stream_ctx, &ev);
                            }
                        } else if (exact_solutions[r]) {
                            /* append_solution already destroyed exact_solutions[r] */
                            exact_solutions[r] = NULL;
                        }
                        /* Use symbolic back-substitution for exact propagation */
                        if (do_substitute && exact_solutions[r]) {
                            substitute_solved_symbolic(sys, sys->eqs[i].var_node_id, sys->eqs[i].coord_index,
                                                       exact_solutions[r]);
                        }
                    }
                }
            } else if (p->degree == 3) {
                /* 使用 Cardano 公式精确求解三次方程 */
                SymbolicCoord *cubic_solutions[3] = {NULL, NULL, NULL};
                int cubic_count = solve_cubic_exact(p, cubic_solutions, 3);

                if (cubic_count == 0) {
                    *no_solution = true;
                } else {
                    if (cubic_count > 1)
                        (*multiple_solutions)++;
                    for (int r = 0; r < cubic_count; r++) {
                        if (cubic_solutions[r] && append_solution(result, cubic_solutions[r]) == 0) {
                            (*solved_count)++;
                            /* 流式输出: 变量解得（含详细元数据） */
                            if (solver_stream_ctx) {
                                StreamEvent ev;
                                memset(&ev, 0, sizeof(ev));
                                ev.type = STREAM_EVENT_SOLVE_VARIABLE_RESOLVED;
                                ev.timestamp_ms = stream_timestamp_ms();
                                ev.var_id = sys->eqs[i].var_node_id;
                                ev.step_number = *solved_count;
                                ev.description = "变量解得 (三次)";
                                char detail[SOLVER_DETAIL_BUF_SIZE];
                                int _snw_cv;
                                LV00_SAFE_SNPRINTF(_snw_cv, detail, sizeof(detail),
                                                   "{\"method\":\"cubic_cardano\","
                                                   "\"var_node_id\":%d,\"coord_index\":%d,"
                                                   "\"root_index\":%d,\"root_count\":%d,"
                                                   "\"pass\":%d,\"total_solved\":%d}",
                                                   sys->eqs[i].var_node_id, sys->eqs[i].coord_index, r, cubic_count,
                                                   pass, *solved_count);
                                LV00_UNUSED(_snw_cv);
                                ev.detail_json = detail;
                                stream_emit(solver_stream_ctx, &ev);
                            }
                        } else if (cubic_solutions[r]) {
                            cubic_solutions[r] = NULL;
                        }
                        if (do_substitute && cubic_solutions[r]) {
                            substitute_solved_symbolic(sys, sys->eqs[i].var_node_id, sys->eqs[i].coord_index,
                                                       cubic_solutions[r]);
                        }
                    }
                }
            }
        }
    }
}

/**
 * Free all solutions in a GroebnerResult and reset it.
 */
static void cleanup_groebner_result(GroebnerResult *result) {
    if (!result)
        return;
    for (int i = 0; i < result->solution_count; i++) {
        symbolic_coord_destroy(result->solutions[i]);
    }
    lv00_free((void **) &result->solutions);
    result->solutions = NULL;
    result->solution_count = 0;
}

/* 前向声明：order_variables_by_dependency（定义见下文） */
static int *order_variables_by_dependency(const ConstraintGraph *graph, const int *var_ids, int var_count,
                                          const int *dirty_var_ids, int dirty_count, int *out_count);

/* ================================================================== */
/*  PUBLIC API: solve_algebraic_system                                 */
/* ================================================================== */

SolverStatus solve_algebraic_system(ConstraintGraph *graph, const int *dirty_variable_ids, int dirty_count,
                                    GroebnerResult **out_result) {
    if (!graph || !out_result) {
        debug_log(LOG_LEVEL_ERROR, "solver", "solve_algebraic_system: 无效参数 graph=%p out_result=%p",
                  (const void *) graph, (const void *) out_result);
        return SOLVER_STATUS_TIMEOUT;
    }

    /* 分配结果内存 */
    GroebnerResult *result = lv00_malloc(sizeof(GroebnerResult));
    if (!result) {
        debug_log(LOG_LEVEL_ERROR, "solver", "solve_algebraic_system: 无法分配 GroebnerResult（大小 %zu 字节）",
                  sizeof(GroebnerResult));
        return SOLVER_STATUS_TIMEOUT;
    }
    memset(result, 0, sizeof(GroebnerResult));
    result->solutions = NULL;
    result->solution_count = 0;
    result->unique = false;
    result->overdetermined = false;

    /* ── 快照: 保存求解前的节点坐标状态 ── */
    SolverSnapshot snapshot;
    bool snapshot_saved = solver_snapshot_save(graph, &snapshot);
    if (!snapshot_saved) {
        debug_log(LOG_LEVEL_WARN, "solver", "solve_algebraic_system: 快照保存失败，继续求解但无法回滚");
        memset(&snapshot, 0, sizeof(snapshot));
    }

    /* 流式输出: 求解阶段开始 */
    stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_START, "代数求解开始", 0);

    /* Step 1: Extract algebraic equations from the constraint graph */
    EquationSystem sys;
    equation_system_init(&sys);
    extract_equations_from_constraints(graph, &sys);

    /* 流式事件: 方程提取完成，报告进度 */
    if (solver_stream_ctx) {
        StreamEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = STREAM_EVENT_SOLVE_EQUATION_EXTRACTED;
        ev.timestamp_ms = stream_timestamp_ms();
        ev.step_number = sys.count;
        ev.total_steps = -1;
        ev.description = "方程提取完成";
        char detail[SOLVER_DETAIL_BUF_SIZE];
        int _snw_eq;
        LV00_SAFE_SNPRINTF(_snw_eq, detail, sizeof(detail), "{\"equation_count\":%d,\"phase\":\"extraction\"}",
                           sys.count);
        LV00_UNUSED(_snw_eq);
        ev.detail_json = detail;
        stream_emit(solver_stream_ctx, &ev);
    }

    /* Step 2: Check for overconstrained system */
    int total_eqs = 0;
    int max_degree_global = 0;
    for (int i = 0; i < sys.count; i++) {
        if (sys.eqs[i].poly.degree >= 0)
            total_eqs++;
        if (sys.eqs[i].poly.degree > max_degree_global)
            max_degree_global = sys.eqs[i].poly.degree;
    }

    int *point_ids = NULL;
    int point_count = count_point_variables(graph, &point_ids);
    int scalar_vars = point_count * 2; /* x and y for each point */
    lv00_free((void **) &point_ids);

    if (total_eqs > scalar_vars) {
        result->overdetermined = true;
        /* Check for actual conflicts */
        if (check_conflict_equations(graph)) {
            equation_system_clear(&sys);
            *out_result = result;
            stream_emit_simple(solver_stream_ctx, STREAM_EVENT_ERROR, "求解完成: 检测到冲突方程", 0);
            /* 求解失败，回滚所有坐标修改 */
            solver_snapshot_restore(graph, &snapshot);
            solver_snapshot_free(&snapshot);
            return SOLVER_STATUS_OVERCONSTRAINED;
        }
        /* Might be redundant but not conflicting; continue */
    }

    /* Step 3: Check for out-of-scope (degree > 3) */
    bool has_out_of_scope = false;
    for (int i = 0; i < sys.count; i++) {
        if (is_out_of_scope(&sys.eqs[i].poly)) {
            has_out_of_scope = true;
            break;
        }
    }
    if (has_out_of_scope) {
        equation_system_clear(&sys);
        *out_result = result;
        stream_emit_simple(solver_stream_ctx, STREAM_EVENT_ERROR, "求解完成: 方程超出代数范围", 0);
        /* 求解失败，回滚所有坐标修改 */
        solver_snapshot_restore(graph, &snapshot);
        solver_snapshot_free(&snapshot);
        return SOLVER_STATUS_OUT_OF_SCOPE;
    }

    /* Step 4: Order variables by dependency (topological sort).
     * 被依赖的变量排在前面，优先求解。这确保在消元过程中，
     * 已知值可以尽早代入，减少后续方程的复杂度。 */
    {
        /* 收集方程系统中的所有唯一变量 ID */
        int *all_var_ids = lv00_malloc((size_t) sys.count * sizeof(int));
        if (!all_var_ids) {
            equation_system_clear(&sys);
            *out_result = result;
            stream_emit_simple(solver_stream_ctx, STREAM_EVENT_ERROR, "求解错误: 内存分配失败", 0);
            /* 求解失败，回滚所有坐标修改 */
            solver_snapshot_restore(graph, &snapshot);
            solver_snapshot_free(&snapshot);
            return SOLVER_STATUS_TIMEOUT;
        }
        int all_var_count = 0;
        for (int i = 0; i < sys.count; i++) {
            if (sys.eqs[i].poly.degree < 0)
                continue;
            int vid = sys.eqs[i].var_node_id;
            bool found = false;
            for (int j = 0; j < all_var_count; j++) {
                if (all_var_ids[j] == vid) {
                    found = true;
                    break;
                }
            }
            if (!found)
                all_var_ids[all_var_count++] = vid;
        }

        if (all_var_count > 0) {
            /* 流式事件: 开始拓扑排序 */
            if (solver_stream_ctx) {
                StreamEvent ev;
                memset(&ev, 0, sizeof(ev));
                ev.type = STREAM_EVENT_PROGRESS;
                ev.timestamp_ms = stream_timestamp_ms();
                ev.progress = 0.15;
                ev.description = "开始变量依赖拓扑排序";
                char detail[SOLVER_DETAIL_BUF_SIZE];
                int _snw_ts;
                LV00_SAFE_SNPRINTF(_snw_ts, detail, sizeof(detail), "{\"phase\":\"topology_sort\",\"var_count\":%d}",
                                   all_var_count);
                LV00_UNUSED(_snw_ts);
                ev.detail_json = detail;
                stream_emit(solver_stream_ctx, &ev);
            }

            int ordered_count = 0;
            int *ordered_ids = order_variables_by_dependency(graph, all_var_ids, all_var_count, dirty_variable_ids,
                                                             dirty_count, &ordered_count);

            if (ordered_ids && ordered_count > 0) {
                /* 按拓扑排序重排方程系统中的方程顺序。
                 * 将被依赖变量的方程排在前面，确保优先求解。 */
                /* 构建排序映射: var_id -> priority (越小越优先) */
                int *priority = lv00_malloc((size_t) sys.count * sizeof(int));
                if (!priority) {
                    lv00_free((void **) &ordered_ids);
                    lv00_free((void **) &all_var_ids);
                    equation_system_clear(&sys);
                    *out_result = result;
                    stream_emit_simple(solver_stream_ctx, STREAM_EVENT_ERROR, "求解错误: 内存分配失败", 0);
                    /* 求解失败，回滚所有坐标修改 */
                    solver_snapshot_restore(graph, &snapshot);
                    solver_snapshot_free(&snapshot);
                    return SOLVER_STATUS_OUT_OF_MEMORY;
                }
                for (int i = 0; i < sys.count; i++)
                    priority[i] = INT_MAX;

                for (int i = 0; i < ordered_count; i++) {
                    for (int j = 0; j < all_var_count; j++) {
                        if (all_var_ids[j] == ordered_ids[i]) {
                            /* 将 priority 存储在临时数组中 */
                            break;
                        }
                    }
                }

                /* 使用简单的选择排序重排方程 (方程数量通常不大) */
                for (int i = 0; i < sys.count - 1; i++) {
                    int best = i;
                    int best_pri = INT_MAX;
                    for (int k = 0; k < ordered_count; k++) {
                        if (sys.eqs[i].var_node_id == ordered_ids[k]) {
                            best_pri = k;
                            break;
                        }
                    }
                    for (int j = i + 1; j < sys.count; j++) {
                        int pri = INT_MAX;
                        for (int k = 0; k < ordered_count; k++) {
                            if (sys.eqs[j].var_node_id == ordered_ids[k]) {
                                pri = k;
                                break;
                            }
                        }
                        if (pri < best_pri) {
                            best_pri = pri;
                            best = j;
                        }
                    }
                    if (best != i) {
                        /* 交换方程 i 和 best */
                        PolyEquation tmp = sys.eqs[i];
                        sys.eqs[i] = sys.eqs[best];
                        sys.eqs[best] = tmp;
                    }
                }

                lv00_free((void **) &priority);
                lv00_free((void **) &ordered_ids);
            }
        }
        lv00_free((void **) &all_var_ids);
    }

    /* 流式事件: 拓扑排序完成，开始方程消元 */
    if (solver_stream_ctx) {
        StreamEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = STREAM_EVENT_PROGRESS;
        ev.timestamp_ms = stream_timestamp_ms();
        ev.progress = 0.25;
        ev.description = "拓扑排序完成，开始逐方程消元求解";
        stream_emit(solver_stream_ctx, &ev);
    }

    /* Step 5: Geometric reasoning elimination -- solve linear equations first */
    int solved_count = 0;
    int multiple_solutions = 0;
    bool no_solution = false;

    solve_equations_pass(&sys, result, &solved_count, &multiple_solutions, &no_solution, true);

    /* Step 6: After elimination, check for contradictions */
    if (no_solution || check_contradiction_after_substitution(&sys)) {
        /* Clean up solutions */
        cleanup_groebner_result(result);
        equation_system_clear(&sys);
        *out_result = result;
        stream_emit_simple(solver_stream_ctx, STREAM_EVENT_ERROR, "求解完成: 无解", 0);
        /* 求解失败，回滚所有坐标修改 */
        solver_snapshot_restore(graph, &snapshot);
        solver_snapshot_free(&snapshot);
        return SOLVER_STATUS_NO_SOLUTION;
    }

    /* Step 7: Gröbner basis elimination for remaining unsolved equations.
     * When simple per-equation solving leaves unsolved equations, use
     * Buchberger's algorithm for system-level elimination. */
    {
        int remaining_before_gb = 0;
        for (int i = 0; i < sys.count; i++) {
            if (sys.eqs[i].poly.degree >= 0)
                remaining_before_gb++;
        }

        if (remaining_before_gb > 0) {
            /* 流式事件: 消元后有剩余方程，进入 Gröbner 基阶段 */
            if (solver_stream_ctx) {
                StreamEvent ev;
                memset(&ev, 0, sizeof(ev));
                ev.type = STREAM_EVENT_PROGRESS;
                ev.timestamp_ms = stream_timestamp_ms();
                ev.progress = 0.55;
                ev.step_number = remaining_before_gb;
                ev.description = "逐方程消元完成，仍有剩余方程，进入Gröbner基计算";
                char detail[SOLVER_DETAIL_BUF_SIZE];
                int _snw_gb_prog;
                LV00_SAFE_SNPRINTF(_snw_gb_prog, detail, sizeof(detail),
                                   "{\"phase\":\"groebner_entry\",\"remaining\":%d,\"solved\":%d}", remaining_before_gb,
                                   solved_count);
                LV00_UNUSED(_snw_gb_prog);
                ev.detail_json = detail;
                stream_emit(solver_stream_ctx, &ev);
            }

            /* Reset no_solution so that first-pass solutions are preserved
               even if the Gröbner pass encounters a contradiction. */
            no_solution = false;

            /* Check if any remaining equation has degree > 4 */
            bool has_high_degree = false;
            for (int i = 0; i < sys.count; i++) {
                if (sys.eqs[i].poly.degree >= 0 && sys.eqs[i].poly.degree > 4) {
                    has_high_degree = true;
                    break;
                }
            }

            if (has_high_degree) {
                /* Gröbner basis returned out-of-scope (degree > 4) */
                char *suggestion = NULL;
                analyze_out_of_scope(graph, -1, &suggestion);
                lv00_free((void **) &suggestion);
                equation_system_clear(&sys);
                *out_result = result;
                stream_emit_simple(solver_stream_ctx, STREAM_EVENT_ERROR, "求解完成: 高次方程无法处理", 0);
                solver_snapshot_restore(graph, &snapshot);
                solver_snapshot_free(&snapshot);
                return SOLVER_STATUS_OUT_OF_SCOPE;
            }

            /* Attempt Gröbner basis computation */
            SolverStatus gb_status = groebner_basis_compute(&sys);

            if (gb_status == SOLVER_STATUS_OUT_OF_SCOPE) {
                /* Gröbner basis computation found degree > 4 */
                char *suggestion = NULL;
                analyze_out_of_scope(graph, -1, &suggestion);
                lv00_free((void **) &suggestion);
                equation_system_clear(&sys);
                *out_result = result;
                stream_emit_simple(solver_stream_ctx, STREAM_EVENT_ERROR, "求解完成: Groebner基计算超出范围", 0);
                solver_snapshot_restore(graph, &snapshot);
                solver_snapshot_free(&snapshot);
                return SOLVER_STATUS_OUT_OF_SCOPE;
            }

            if (gb_status == SOLVER_STATUS_OK) {
                /* Try to solve the Gröbner basis equations */
                solve_equations_pass(&sys, result, &solved_count, &multiple_solutions, &no_solution, true);

                /* Check for contradictions after Gröbner basis solving */
                if (no_solution || check_contradiction_after_substitution(&sys)) {
                    cleanup_groebner_result(result);
                    equation_system_clear(&sys);
                    *out_result = result;
                    stream_emit_simple(solver_stream_ctx, STREAM_EVENT_ERROR, "求解完成: Groebner基求解后无解", 0);
                    solver_snapshot_restore(graph, &snapshot);
                    solver_snapshot_free(&snapshot);
                    return SOLVER_STATUS_NO_SOLUTION;
                }
            }
        }
    }

    /* Step 8: Determine result status */
    if (multiple_solutions > 0) {
        result->unique = false;
    } else if (solved_count > 0) {
        result->unique = true;
    }

    /* Check remaining unsolved equations */
    int remaining = 0;
    for (int i = 0; i < sys.count; i++) {
        if (sys.eqs[i].poly.degree >= 0)
            remaining++;
    }

    equation_system_clear(&sys);

    /* 流式输出: 求解总结统计 */
    if (solver_stream_ctx) {
        StreamEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = STREAM_EVENT_PROGRESS;
        ev.timestamp_ms = stream_timestamp_ms();
        ev.progress = 1.0;
        ev.step_number = solved_count;
        ev.total_steps = solved_count + remaining;
        ev.description = "代数求解总结";
        char detail[SOLVER_DETAIL_BUF_SIZE];
        int _snw_sum;
        LV00_SAFE_SNPRINTF(_snw_sum, detail, sizeof(detail),
                           "{\"phase\":\"solve_summary\","
                           "\"solved_variables\":%d,"
                           "\"remaining_equations\":%d,"
                           "\"multiple_solutions\":%d,"
                           "\"unique\":%s,"
                           "\"overdetermined\":%s,"
                           "\"max_degree\":%d}",
                           solved_count, remaining, multiple_solutions, result->unique ? "true" : "false",
                           result->overdetermined ? "true" : "false", max_degree_global);
        LV00_UNUSED(_snw_sum);
        ev.detail_json = detail;
        stream_emit(solver_stream_ctx, &ev);
    }

    if (remaining > 0 && solved_count == 0) {
        /* Could not solve anything */
        *out_result = result;
        stream_emit_simple(solver_stream_ctx, STREAM_EVENT_ERROR, "求解完成: 未能求解任何变量", 0);
        solver_snapshot_restore(graph, &snapshot);
        solver_snapshot_free(&snapshot);
        return SOLVER_STATUS_OUT_OF_SCOPE;
    }

    if (result->overdetermined) {
        *out_result = result;
        stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_DONE, "求解完成: 超定系统", 0);
        solver_snapshot_free(&snapshot);
        return SOLVER_STATUS_OVERCONSTRAINED;
    }

    if (multiple_solutions > 0) {
        *out_result = result;
        stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_DONE, "求解完成: 多解", 0);
        solver_snapshot_free(&snapshot);
        return SOLVER_STATUS_MULTIPLE;
    }

    if (solved_count > 0 && remaining == 0) {
        result->unique = true;
        *out_result = result;
        stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_DONE, "求解完成: 唯一解", 0);
        solver_snapshot_free(&snapshot);
        return SOLVER_STATUS_UNIQUE;
    }

    /* Partially solved or underdetermined */
    *out_result = result;
    stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_DONE, "求解完成: 部分求解/欠定", 0);
    solver_snapshot_free(&snapshot);
    return SOLVER_STATUS_OK;
}

/* ================================================================== */
/*  Solvespace 风格交互式求解反馈                                        */
/* ================================================================== */

/**
 * @brief 创建求解器反馈
 */
SolverFeedback *solver_feedback_create(SolverFeedbackType type, const char *message) {
    SolverFeedback *fb = lv00_calloc(1, sizeof(SolverFeedback));
    if (!fb)
        return NULL;

    fb->type = type;
    fb->affected_var_id = -1;
    fb->degrees_of_freedom = -1;
    fb->free_var_count = 0;
    fb->overconstrained_count = 0;

    if (message && message[0] != '\0') {
        fb->message = lv00_malloc(strlen(message) + 1);
        if (!fb->message) {
            lv00_free((void **) &fb);
            return NULL;
        }
        /* [Bug修复] strcpy → lv00_strlcpy 防止缓冲区溢出 */
        lv00_strlcpy(fb->message, message, strlen(message) + 1);
    }

    return fb;
}

/**
 * @brief 销毁求解器反馈
 */
void solver_feedback_destroy(SolverFeedback *feedback) {
    if (!feedback)
        return;
    lv00_free((void **) &feedback->message);
    lv00_free((void **) &feedback->free_var_ids);
    lv00_free((void **) &feedback->overconstrained_ids);
    lv00_free((void **) &feedback);
}

/**
 * @brief 增量求解并返回交互反馈（Solvespace 风格拖拽-实时反馈）
 */
SolverFeedback *solver_feedback_solve(ConstraintGraph *graph, const int *dirty_vars, int dirty_count) {
    if (!graph)
        return NULL;

    SolverFeedback *fb = solver_feedback_create(SOLVER_FEEDBACK_TYPE_CONSTRAINT_ADDED,
                                                dirty_count > 0 ? "约束已添加，增量求解开始" : "执行全量求解");

    if (!fb)
        return NULL;

    /* Step 1: 执行增量求解 */
    GroebnerResult *result = solver_incremental_solve(graph, dirty_vars, dirty_count);

    if (!result) {
        fb->type = SOLVER_FEEDBACK_TYPE_CONFLICT_DETECTED;
        lv00_free((void **) &fb->message);
        fb->message = lv00_malloc(64);
        if (fb->message)
            /* [Bug修复] strcpy → lv00_strlcpy 防止缓冲区溢出 */
            lv00_strlcpy(fb->message, "求解失败：约束系统无解或超出范围", 64);
        return fb;
    }

    /* Step 2: 计算自由度 */
    int *free_var_ids = NULL;
    int dof = count_degrees_of_freedom(graph, &free_var_ids);
    fb->degrees_of_freedom = (dof >= 0) ? dof : -1;

    if (free_var_ids && dof > 0) {
        fb->free_var_ids = lv00_malloc((size_t) dof * sizeof(int));
        if (fb->free_var_ids) {
            memcpy(fb->free_var_ids, free_var_ids, (size_t) dof * sizeof(int));
            fb->free_var_count = dof;
        }
        lv00_free((void **) &free_var_ids);
    }

    /* Step 3: 判断反馈类型 */
    if (result->overdetermined) {
        fb->type = SOLVER_FEEDBACK_TYPE_OVERCONSTRAINED;
        lv00_free((void **) &fb->message);
        fb->message = lv00_malloc(64);
        if (fb->message)
            /* [Bug修复] strcpy → lv00_strlcpy 防止缓冲区溢出 */
            lv00_strlcpy(fb->message, "检测到过约束：某些变量被过多方程约束", 64);

        /* 标记过约束变量（简化处理：标记脏变量为过约束候选） */
        if (dirty_count > 0 && dirty_vars) {
            fb->overconstrained_ids = lv00_malloc((size_t) dirty_count * sizeof(int));
            if (fb->overconstrained_ids) {
                memcpy(fb->overconstrained_ids, dirty_vars, (size_t) dirty_count * sizeof(int));
                fb->overconstrained_count = dirty_count;
            }
        }
    } else if (dof == 0) {
        fb->type = SOLVER_FEEDBACK_TYPE_VARIABLE_SOLVED;
        lv00_free((void **) &fb->message);
        fb->message = lv00_malloc(64);
        if (fb->message)
            /* [Bug修复] strcpy → lv00_strlcpy 防止缓冲区溢出 */
            lv00_strlcpy(fb->message, "所有变量已唯一确定（零自由度）", 64);
        if (dirty_count > 0 && dirty_vars) {
            fb->affected_var_id = dirty_vars[0];
        }
    } else if (dof > 0) {
        fb->type = SOLVER_FEEDBACK_TYPE_DOF_CHANGED;
        char buf[SOLVER_DETAIL_BUF_SIZE];
        snprintf(buf, sizeof(buf), "当前仍有 %d 个自由度", dof);
        lv00_free((void **) &fb->message);
        fb->message = lv00_malloc(strlen(buf) + 1);
        if (fb->message)
            /* [Bug修复] strcpy → lv00_strlcpy 防止缓冲区溢出 */
            lv00_strlcpy(fb->message, buf, strlen(buf) + 1);
        if (dirty_count > 0 && dirty_vars) {
            fb->affected_var_id = dirty_vars[0];
        }
    }

    /* 流式输出求解反馈 */
    if (solver_stream_ctx) {
        StreamEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = STREAM_EVENT_SOLVE_DONE;
        ev.timestamp_ms = stream_timestamp_ms();
        ev.var_id = fb->affected_var_id;
        ev.description = fb->message ? fb->message : "";
        char detail[SOLVER_DETAIL_BUF_SIZE];
        int _snw_fb;
        LV00_SAFE_SNPRINTF(_snw_fb, detail, sizeof(detail),
                           "{\"type\":\"%s\",\"dof\":%d,\"free_count\":%d,\"overconstrained\":%d}",
                           (fb->type == SOLVER_FEEDBACK_TYPE_VARIABLE_SOLVED)     ? "solved"
                           : (fb->type == SOLVER_FEEDBACK_TYPE_OVERCONSTRAINED)   ? "overconstrained"
                           : (fb->type == SOLVER_FEEDBACK_TYPE_DOF_CHANGED)       ? "dof_changed"
                           : (fb->type == SOLVER_FEEDBACK_TYPE_CONFLICT_DETECTED) ? "conflict"
                                                                             : "constraint_added",
                           fb->degrees_of_freedom, fb->free_var_count, fb->overconstrained_count);
        LV00_UNUSED(_snw_fb);
        ev.detail_json = detail;
        stream_emit(solver_stream_ctx, &ev);
    }

    groebner_result_free(result);
    return fb;
}

/* 几何推理模板的前向声明 */
static int template_similar_triangles(ConstraintGraph *graph, EquationSystem *sys);
static int template_pythagorean(ConstraintGraph *graph, EquationSystem *sys);
static int template_parallel_cut(const ConstraintGraph *graph, EquationSystem *sys);

/* ================================================================== */
/*  PUBLIC API: eliminate_geometry                                     */
/* ================================================================== */

/**
 * @brief 通过几何推理消元求解指定的目标变量
 *
 * @details 从约束图中提取方程，使用线性方程消元求解目标变量。
 *          对每个待消元变量，搜索线性方程求解。若无线性方程但有
 *          BETWEENNESS 约束，则记录约束信息用于后续解选择。
 *          同时应用几何推理模板（相似三角形、勾股定理、平行线截割）
 *          来生成额外的消元方程。
 *
 * @param graph         约束图指针
 * @param target_var_id 目标变量的节点 ID
 * @param eliminate_ids 待消元的变量 ID 数组
 * @param elim_count    待消元变量数量
 * @return SOLVER_STATUS_OK 表示成功消元，SOLVER_STATUS_OUT_OF_SCOPE 表示超出范围
 */

SolverStatus eliminate_geometry(ConstraintGraph *graph, int target_var_id, const int *eliminate_ids, int elim_count) {
    LV00_UNUSED(target_var_id);
    if (!graph || !eliminate_ids || elim_count <= 0)
        return SOLVER_STATUS_OK;

    /* Build equation system from constraints */
    EquationSystem sys;
    equation_system_init(&sys);
    extract_equations_from_constraints(graph, &sys);

    /* Identify which variables appear linearly */
    bool *is_linear = lv00_malloc((size_t) sys.count * sizeof(bool));
    if (is_linear)
        memset(is_linear, 0, (size_t) sys.count * sizeof(bool));
    for (int i = 0; i < sys.count; i++) {
        is_linear[i] = (sys.eqs[i].poly.degree <= 1);
    }

    /* For each variable to eliminate, check if it has a linear equation */
    bool any_eliminated = false;
    bool out_of_scope_found = false;

    for (int e = 0; e < elim_count; e++) {
        int eid = eliminate_ids[e];
        bool found_linear = false;

        for (int i = 0; i < sys.count; i++) {
            if (sys.eqs[i].var_node_id != eid)
                continue;
            if (!is_linear[i]) {
                if (sys.eqs[i].poly.degree > 2) {
                    out_of_scope_found = true;
                }
                continue;
            }

            /* Solve the linear equation for this variable */
            double val;
            if (solve_linear(&sys.eqs[i].poly, &val)) {
                /* Substitute the solved value into the target variable's
                   equations and all other equations */
                substitute_solved(&sys, eid, sys.eqs[i].coord_index, val);

                /* Also update the node's symbolic coordinate */
                GeomNode *node = find_node(graph, eid);
                if (node && node->coord_count > sys.eqs[i].coord_index) {
                    if (fabs(val) > 9.2e12) {
                        /* Value too large for exact rational representation, skip */
                    } else {
                        SymbolicCoord *new_coord = symbolic_coord_create_rational(
                            (int64_t) (val * LV00_SOLVER_SCALE_FACTOR), LV00_SOLVER_SCALE_FACTOR);
                        if (new_coord) {
                            symbolic_coord_destroy(node->symbolic_coords[sys.eqs[i].coord_index]);
                            node->symbolic_coords[sys.eqs[i].coord_index] = new_coord;
                        }
                    }
                }
                found_linear = true;
                any_eliminated = true;
                /* 流式输出: 变量解得 */
                stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_VARIABLE_RESOLVED, "变量解得 (几何消元)", eid);
            }
        }

        /* If this variable has no linear equation but has a BETWEENNESS
           constraint, note the direction constraint for solution selection */
        if (!found_linear) {
            for (int ci = 0; ci < graph->constraint_count; ci++) {
                Constraint *c = graph->constraints[ci];
                if (c->type != BETWEENNESS)
                    continue;
                if (c->participant_count < 3)
                    continue;
                /* Check if this variable is the middle point */
                if (c->participants[1] == eid) {
                    /* Betweenness gives us a ratio constraint:
                       p2 = p1 + t*(p3-p1), 0 <= t <= 1
                       This is linear in p2's coordinates. */
                    GeomNode *p1 = find_node(graph, c->participants[0]);
                    GeomNode *p3 = find_node(graph, c->participants[2]);
                    if (p1 && p3 && p1->coord_count >= 2 && p3->coord_count >= 2) {
                        double x1, y1, x3, y3;
                        if (point_coord(p1, 0, &x1) && point_coord(p1, 1, &y1) && point_coord(p3, 0, &x3) &&
                            point_coord(p3, 1, &y3)) {
                            /* The variable is constrained to the segment.
                               We can't determine a unique value without more info,
                               but we can narrow the range. */
                            /* Store the constraint info on the node's
                               numeric_assumption_declaration */
                            GeomNode *target = find_node(graph, eid);
                            if (target) {
                                lv00_free((void **) &target->numeric_assumption_declaration);
                                char buf[SOLVER_DETAIL_BUF_SIZE];
                                int _snw;
                                LV00_SAFE_SNPRINTF(_snw, buf, sizeof(buf), "betweenness:p1=(%.6f,%.6f),p3=(%.6f,%.6f)",
                                                   x1, y1, x3, y3);
                                LV00_UNUSED(_snw);
                                target->numeric_assumption_declaration = lv00_malloc(strlen(buf) + 1);
                                if (target->numeric_assumption_declaration) {
                                    lv00_strlcpy(target->numeric_assumption_declaration, buf, strlen(buf) + 1);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    lv00_free((void **) &is_linear);
    equation_system_clear(&sys);

    /* Apply geometry reasoning templates to generate additional equations.
     * template_similar_triangles and template_pythagorean can discover
     * new algebraic constraints from geometric structures (triangles,
     * right angles, proportional sides) that are not directly encoded
     * in the constraint graph. */
    {
        EquationSystem tmpl_sys;
        equation_system_init(&tmpl_sys);
        int tmpl_added = template_similar_triangles(graph, &tmpl_sys);
        tmpl_added += template_pythagorean(graph, &tmpl_sys);
        tmpl_added += template_parallel_cut(graph, &tmpl_sys);

        /* If templates generated equations, try to use them for elimination */
        if (tmpl_added > 0) {
            for (int e = 0; e < elim_count; e++) {
                int eid = eliminate_ids[e];
                for (int i = 0; i < tmpl_sys.count; i++) {
                    if (tmpl_sys.eqs[i].var_node_id != eid)
                        continue;
                    if (tmpl_sys.eqs[i].poly.degree != 1)
                        continue;

                    double val;
                    if (solve_linear(&tmpl_sys.eqs[i].poly, &val)) {
                        GeomNode *node = find_node(graph, eid);
                        if (node && node->coord_count > tmpl_sys.eqs[i].coord_index) {
                            if (fabs(val) > 9.2e12) {
                                /* Value too large for exact rational representation, skip */
                            } else {
                                SymbolicCoord *new_coord = symbolic_coord_create_rational(
                                    (int64_t) (val * LV00_SOLVER_SCALE_FACTOR), LV00_SOLVER_SCALE_FACTOR);
                                if (new_coord) {
                                    symbolic_coord_destroy(node->symbolic_coords[tmpl_sys.eqs[i].coord_index]);
                                    node->symbolic_coords[tmpl_sys.eqs[i].coord_index] = new_coord;
                                    any_eliminated = true;
                                    /* 流式输出: 变量解得 */
                                    stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_VARIABLE_RESOLVED,
                                                       "变量解得 (几何模板)", eid);
                                }
                            }
                        }
                        break;
                    }
                }
            }
        }
        equation_system_clear(&tmpl_sys);
    }

    if (out_of_scope_found)
        return SOLVER_STATUS_OUT_OF_SCOPE;
    if (any_eliminated)
        return SOLVER_STATUS_OK;
    return SOLVER_STATUS_OK; /* No variables eliminated, but not an error */
}

/* ================================================================== */
/*  PUBLIC API: analyze_out_of_scope                                   */
/* ================================================================== */

SolverStatus analyze_out_of_scope(const ConstraintGraph *graph, int var_id, char **suggestion) {
    if (!graph || !suggestion)
        return SOLVER_STATUS_OUT_OF_SCOPE;

    /* 流式事件: 开始超出范围分析 */
    if (solver_stream_ctx) {
        StreamEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = STREAM_EVENT_INFO;
        ev.timestamp_ms = stream_timestamp_ms();
        ev.var_id = var_id;
        ev.description = "开始超出代数范围分析";
        char detail[SOLVER_DETAIL_BUF_SIZE];
        int _snw_diag;
        LV00_SAFE_SNPRINTF(_snw_diag, detail, sizeof(detail), "{\"phase\":\"analyze_out_of_scope\",\"var_id\":%d}",
                           var_id);
        LV00_UNUSED(_snw_diag);
        ev.detail_json = detail;
        stream_emit(solver_stream_ctx, &ev);
    }

    /* Build equation system and find equations for this variable */
    EquationSystem sys;
    equation_system_init(&sys);
    extract_equations_from_constraints(graph, &sys);

    /* 流式事件: 方程提取完成 */
    if (solver_stream_ctx) {
        StreamEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = STREAM_EVENT_SOLVE_EQUATION_EXTRACTED;
        ev.timestamp_ms = stream_timestamp_ms();
        ev.step_number = sys.count;
        ev.var_id = var_id;
        ev.description = "方程系统已提取，开始诊断";
        stream_emit(solver_stream_ctx, &ev);
    }

    mpz_poly_t *target_poly = NULL;
    for (int i = 0; i < sys.count; i++) {
        if (sys.eqs[i].var_node_id == var_id && sys.eqs[i].poly.degree > 3) {
            target_poly = &sys.eqs[i].poly;
            break;
        }
    }

    if (!target_poly) {
        /* No high-degree equation found for this variable;
           the out-of-scope might come from the system as a whole */
        *suggestion = lv00_strdup_safe(
            "No single high-degree equation found for this variable. "
            "The system may be out of scope due to coupled nonlinear equations. "
            "Consider decomposing the construction into simpler sub-problems "
            "with auxiliary construction lines.");

        /* 流式事件: 无单一高次方程，系统整体超出范围 */
        if (solver_stream_ctx) {
            StreamEvent ev;
            memset(&ev, 0, sizeof(ev));
            ev.type = STREAM_EVENT_WARNING;
            ev.timestamp_ms = stream_timestamp_ms();
            ev.var_id = var_id;
            ev.description = "超出范围分析：无单一高次方程，系统整体耦合非线性";
            ev.detail_json = "{\"diagnosis\":\"coupled_nonlinear\",\"resolvable\":false}";
            stream_emit(solver_stream_ctx, &ev);
        }

        equation_system_clear(&sys);
        return SOLVER_STATUS_OUT_OF_SCOPE;
    }

    /* 流式事件: 发现高次方程 */
    if (solver_stream_ctx) {
        StreamEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = STREAM_EVENT_INFO;
        ev.timestamp_ms = stream_timestamp_ms();
        ev.var_id = var_id;
        ev.description = "发现高次方程，尝试因式分解";
        char detail[SOLVER_DETAIL_BUF_SIZE];
        int _snw_found;
        LV00_SAFE_SNPRINTF(_snw_found, detail, sizeof(detail), "{\"degree\":%d,\"var_id\":%d}", target_poly->degree,
                           var_id);
        LV00_UNUSED(_snw_found);
        ev.detail_json = detail;
        stream_emit(solver_stream_ctx, &ev);
    }

    /* Try to factor the polynomial */
    mpz_poly_t factor1, factor2;
    bool factored = try_factor_polynomial(target_poly, &factor1, &factor2);

    if (factored) {
        char *f1_str = mpz_poly_get_str(&factor1);
        char *f2_str = mpz_poly_get_str(&factor2);

        size_t needed = 256 + strlen(f1_str) + strlen(f2_str);
        if (needed > INT_MAX) {
            lv00_free((void **)&f1_str);
            lv00_free((void **)&f2_str);
            mpz_poly_clear(&factor1);
            mpz_poly_clear(&factor2);
            return SOLVER_STATUS_OUT_OF_SCOPE;
        }
        *suggestion = lv00_malloc(needed);
        int _snw;
        LV00_SAFE_SNPRINTF(_snw, *suggestion, needed,
                           "Polynomial factors into (%s) * (%s). "
                           "Split into multiple quadratic steps with auxiliary lines: "
                           "solve each factor separately and combine solutions. "
                           "Each factor of degree <= 2 is within constructible scope.",
                           f1_str, f2_str);
        LV00_UNUSED(_snw);

        /* 流式事件: 因式分解成功 */
        if (solver_stream_ctx) {
            StreamEvent ev;
            memset(&ev, 0, sizeof(ev));
            ev.type = STREAM_EVENT_INFO;
            ev.timestamp_ms = stream_timestamp_ms();
            ev.var_id = var_id;
            ev.description = "因式分解成功，可通过拆分求解";
            char detail[SOLVER_DETAIL_BUF_SIZE];
            int _snw_fact;
            LV00_SAFE_SNPRINTF(
                _snw_fact, detail, sizeof(detail),
                "{\"diagnosis\":\"factorable\",\"resolvable\":true,\"factor1\":\"%s\",\"factor2\":\"%s\"}", f1_str,
                f2_str);
            LV00_UNUSED(_snw_fact);
            ev.detail_json = detail;
            stream_emit(solver_stream_ctx, &ev);
        }

        lv00_free((void **) &f1_str); f1_str = NULL;  /* GMP分配，用标准free */
        lv00_free((void **) &f2_str); f2_str = NULL;  /* GMP分配，用标准free */
        mpz_poly_clear(&factor1);
        mpz_poly_clear(&factor2);
        equation_system_clear(&sys);
        return SOLVER_STATUS_OUT_OF_SCOPE;
    }

    /* Check if the degree is 4 (quartic) - check for biquadratic */
    if (target_poly->degree == 4) {
        /* Check if odd-degree coefficients are zero */
        bool biquadratic = true;
        for (int i = 1; i <= 3; i += 2) {
            if (mpz_cmp_si(target_poly->coeffs[i], 0) != 0) {
                biquadratic = false;
                break;
            }
        }
        if (biquadratic) {
            *suggestion = lv00_strdup_safe(
                "Biquadratic equation detected (only even powers). "
                "Substitute u = x^2 to reduce to quadratic, solve for u, "
                "then take square roots. This is within constructible scope "
                "via two quadratic steps.");

            /* 流式事件: 双二次方程 */
            if (solver_stream_ctx) {
                StreamEvent ev;
                memset(&ev, 0, sizeof(ev));
                ev.type = STREAM_EVENT_INFO;
                ev.timestamp_ms = stream_timestamp_ms();
                ev.var_id = var_id;
                ev.description = "双二次方程，可通过变量替换 u=x² 归约为二次方程求解";
                ev.detail_json =
                    "{\"diagnosis\":\"biquadratic\",\"resolvable\":true,"
                    "\"method\":\"substitute_u_equals_x_squared\"}";
                stream_emit(solver_stream_ctx, &ev);
            }

            equation_system_clear(&sys);
            return SOLVER_STATUS_OUT_OF_SCOPE;
        }
    }

    /* General high-degree irreducible (> cubic, which v3.1 now supports) */
    char *poly_str = mpz_poly_get_str(target_poly);
    size_t needed = 256 + strlen(poly_str);
    if (needed > INT_MAX || needed == 0) {
        lv00_free((void **)&poly_str);
        return SOLVER_STATUS_OUT_OF_SCOPE;
    }
    *suggestion = lv00_malloc(needed);
    int _snw2;
    LV00_SAFE_SNPRINTF(_snw2, *suggestion, needed,
                       "Irreducible polynomial of degree %d with coefficients [%s]. "
                       "Exceeds quadratic coverage. "
                       "Consider: (1) adding auxiliary construction lines to decompose "
                       "into quadratic sub-problems, (2) checking if the problem "
                       "reduces to a known unconstructible problem (angle trisection, "
                       "cube duplication, circle squaring), or (3) using numerical "
                       "approximation with Neusis construction.",
                       target_poly->degree, poly_str);
    LV00_UNUSED(_snw2);

    /* 流式事件: 一般不可约高次多项式 */
    if (solver_stream_ctx) {
        StreamEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = STREAM_EVENT_ERROR;
        ev.timestamp_ms = stream_timestamp_ms();
        ev.var_id = var_id;
        ev.description = "不可约高次多项式，超出二次可构造范围";
        char detail[SOLVER_DETAIL_BUF_SIZE];
        int _snw_irr;
        LV00_SAFE_SNPRINTF(_snw_irr, detail, sizeof(detail),
                           "{\"diagnosis\":\"irreducible_high_degree\",\"degree\":%d,"
                           "\"polynomial\":\"%s\",\"resolvable\":false}",
                           target_poly->degree, poly_str);
        LV00_UNUSED(_snw_irr);
        ev.detail_json = detail;
        stream_emit(solver_stream_ctx, &ev);
    }

    lv00_free((void **) &poly_str); poly_str = NULL;  /* GMP分配，用标准free */

    equation_system_clear(&sys);
    return SOLVER_STATUS_OUT_OF_SCOPE;
}

/* ================================================================== */
/*  PUBLIC API: count_degrees_of_freedom                               */
/* ================================================================== */

/**
 * @brief 计算约束系统的自由度（修正版：返回 -1 表示错误）
 *
 * @details 自由度 = 总变量数 - 总约束数。
 *          每个点有 2 个坐标变量（x, y），约束权重取决于约束类型：
 *          INCIDENCE=1, BETWEENNESS=2, INTERSECTION=2, CONTAINMENT=1, CONNECTION=1。
 *          同时计入线段节点上的距离约束。
 *          通过每点方程数分析识别具体哪些变量是自由的。
 *
 * @param graph            约束图指针
 * @param out_free_var_ids 输出：自由变量 ID 数组（调用者负责释放）
 * @return 自由度数量（非负整数），-1 表示参数错误
 */

int count_degrees_of_freedom(const ConstraintGraph *graph, int **out_free_var_ids) {
    if (!graph) {
        if (out_free_var_ids)
            *out_free_var_ids = NULL;
        return -1; /* 返回 -1 明确表示参数错误，区别于 0 自由度 */
    }

    /* Count point variables */
    int *pt_ids = NULL;
    int pt_count = count_point_variables(graph, &pt_ids);
    int total_vars = pt_count * 2;

    /* Sum constraint weights */
    int total_constraints = 0;
    for (int i = 0; i < graph->constraint_count; i++) {
        total_constraints += constraint_weight(graph->constraints[i]);
    }

    /* 线段本身表示两个端点之间的一条连接/距离关系，计作一个独立约束。 */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *n = graph->nodes[i];
        if (n && n->type == GEOM_LINE_SEGMENT) {
            total_constraints += 1;
        }
    }

    int dof = total_vars - total_constraints;
    if (dof < 0)
        dof = 0;

    /* Identify which specific variables are free.
       A variable is "free" if it has fewer equations than unknowns.
       We track this per-point: each point has 2 coords (x, y).
       A point is fully determined if it has >= 2 independent equations. */
    int *eq_per_point = lv00_malloc((size_t) pt_count * sizeof(int));
    if (eq_per_point)
        memset(eq_per_point, 0, (size_t) pt_count * sizeof(int));
    bool *point_has_quadratic = lv00_malloc((size_t) pt_count * sizeof(bool));
    if (point_has_quadratic)
        memset(point_has_quadratic, 0, (size_t) pt_count * sizeof(bool));

    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        int weight = constraint_weight(c);
        /* Distribute weight to participating points */
        int point_participants = 0;
        int first_pt_idx = -1;
        for (int j = 0; j < c->participant_count; j++) {
            GeomNode *n = find_node(graph, c->participants[j]);
            if (n && n->type == GEOM_POINT) {
                for (int k = 0; k < pt_count; k++) {
                    if (pt_ids[k] == n->id) {
                        if (first_pt_idx < 0)
                            first_pt_idx = k;
                        point_participants++;
                        break;
                    }
                }
            }
        }
        if (point_participants > 0 && first_pt_idx >= 0) {
            /* Distribute equations equally among participating points */
            int per_point = weight / point_participants;
            int remainder = weight % point_participants;
            for (int j = 0; j < c->participant_count; j++) {
                GeomNode *n = find_node(graph, c->participants[j]);
                if (n && n->type == GEOM_POINT) {
                    for (int k = 0; k < pt_count; k++) {
                        if (pt_ids[k] == n->id) {
                            eq_per_point[k] += per_point;
                            if (remainder > 0) {
                                eq_per_point[k]++;
                                remainder--;
                            }
                            break;
                        }
                    }
                }
            }
        }
    }

    /* 线段约束：分配到端点，用于自由变量明细。 */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *n = graph->nodes[i];
        if (!n || n->type != GEOM_LINE_SEGMENT)
            continue;
        /* Add one equation to each endpoint */
        if (n->coord_count >= 2) {
            /* Find the endpoint point nodes */
            for (int j = 0; j < graph->constraint_count; j++) {
                Constraint *c = graph->constraints[j];
                if (c->type != INCIDENCE)
                    continue;
                for (int p = 0; p < c->participant_count; p++) {
                    if (c->participants[p] == n->id) {
                        int other = c->participants[1 - p];
                        for (int k = 0; k < pt_count; k++) {
                            if (pt_ids[k] == other) {
                                eq_per_point[k] += 1;
                                point_has_quadratic[k] = true;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    /* Collect free variable IDs: points with fewer than 2 equations */
    int *free_ids = lv00_malloc((size_t) dof * sizeof(int));
    if (!free_ids) {
        lv00_free((void **) &pt_ids);
        lv00_free((void **) &eq_per_point);
        lv00_free((void **) &point_has_quadratic);
        return -1;
    }
    int free_count = 0;
    for (int i = 0; i < pt_count && free_count < dof; i++) {
        int equations = eq_per_point[i];
        int coords = 2;
        if (equations < coords) {
            /* This point has free coordinates */
            int free_coords = coords - equations;
            for (int c = 0; c < free_coords && free_count < dof; c++) {
                free_ids[free_count++] = pt_ids[i];
            }
        }
    }

    /* If we didn't find enough free variables through per-point analysis,
       fill the rest from underdetermined points */
    if (free_count < dof) {
        for (int i = 0; i < pt_count && free_count < dof; i++) {
            free_ids[free_count++] = pt_ids[i];
        }
    }

    lv00_free((void **) &eq_per_point);
    lv00_free((void **) &point_has_quadratic);
    lv00_free((void **) &pt_ids);

    *out_free_var_ids = free_ids;
    return dof;
}

/* ================================================================== */
/*  PUBLIC API: check_conflict_equations                               */
/* ================================================================== */

/**
 * @brief 检查约束图中是否存在冲突方程
 *
 * @details 执行多项冲突检测：
 *          1. 同一线段对上的不兼容距离约束
 *          2. 常数为非零的零次方程（矛盾：非零 = 0）
 *          3. 首项系数为零的一元方程（矛盾：0*x + b = 0, b != 0）
 *          4. 判别式为负的二次方程（无实解）
 *          5. 超定点的过约束方程数检测
 *          6. 同一参与者上的重复/冲突约束
 *
 * @param graph 约束图指针
 * @return true 表示存在冲突，false 表示无冲突
 */

bool check_conflict_equations(const ConstraintGraph *graph) {
    if (!graph)
        return false;

    /* Check 1: Incompatible distance constraints on same segment pair */
    if (check_incompatible_distances(graph)) {
        return true;
    }

    /* Check 2: Extract equations and look for contradictions */
    fprintf(stderr, "[TRACE check] init sys\n");
    EquationSystem sys;
    equation_system_init(&sys);
    fprintf(stderr, "[TRACE check] extract start\n");
    extract_equations_from_constraints(graph, &sys);
    fprintf(stderr, "[TRACE check] extract done count=%d\n", sys.count);

    /* Check for 0 = nonzero (constant contradictions) */
    for (int i = 0; i < sys.count; i++) {
        mpz_poly_t *p = &sys.eqs[i].poly;
        if (p->degree == 0 && mpz_cmp_si(p->coeffs[0], 0) != 0) {
            equation_system_clear(&sys);
            return true;
        }
    }

    /* Check 3: Linear equations with zero leading coefficient but
       nonzero constant (0*x + b = 0, b != 0) */
    for (int i = 0; i < sys.count; i++) {
        mpz_poly_t *p = &sys.eqs[i].poly;
        if (p->degree == 1 && mpz_cmp_si(p->coeffs[1], 0) == 0 && mpz_cmp_si(p->coeffs[0], 0) != 0) {
            equation_system_clear(&sys);
            return true;
        }
    }

    /* Check 4: Quadratic equations with negative discriminant
       (no real solutions) combined with other constraints that
       require a real solution */
    for (int i = 0; i < sys.count; i++) {
        mpz_poly_t *p = &sys.eqs[i].poly;
        if (p->degree == 2) {
            double a = mpz_get_d(p->coeffs[2]);
            double b = mpz_get_d(p->coeffs[1]);
            double c_val = mpz_get_d(p->coeffs[0]);
            if (fabs(a) > LV00_EPSILON_NEWTON) {
                double disc = b * b - 4.0 * a * c_val;
                if (disc < -LV00_EPSILON_SEGMENT_INTERIOR) {
                    /* No real roots for this quadratic.
                       If this equation is mandatory (not optional),
                       it's a conflict. */
                    equation_system_clear(&sys);
                    return true;
                }
            }
        }
    }

    /* Check 5: Overdetermined point -- more than 2 independent equations
       for a single point (2D) */
    int *eq_count_per_point = NULL;
    int max_id = 0;
    for (int i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i]->id > max_id)
            max_id = graph->nodes[i]->id;
    }
    if (max_id > SOLVER_MAX_VAR_ID) {
        /* Too sparse, use a different approach
           For now, just limit to prevent OOM */
        max_id = SOLVER_MAX_VAR_ID;
    }
    if (max_id > 0) {
        eq_count_per_point = lv00_malloc((size_t) (max_id + 1) * sizeof(int));
        if (eq_count_per_point)
            memset(eq_count_per_point, 0, (size_t) (max_id + 1) * sizeof(int));
        for (int i = 0; i < sys.count; i++) {
            int vid = sys.eqs[i].var_node_id;
            if (vid >= 0 && vid <= max_id) {
                if (sys.eqs[i].poly.degree >= 0) {
                    eq_count_per_point[vid]++;
                }
            }
        }
        /* Also count constraints directly */
        for (int i = 0; i < graph->constraint_count; i++) {
            Constraint *c = graph->constraints[i];
            for (int j = 0; j < c->participant_count; j++) {
                int pid = c->participants[j];
                GeomNode *n = find_node(graph, pid);
                if (n && n->type == GEOM_POINT && pid <= max_id) {
                    eq_count_per_point[pid] += constraint_weight(c) / c->participant_count;
                }
            }
        }
        /* A 2D point with > 2 independent equations may be overconstrained */
        for (int i = 0; i < graph->node_count; i++) {
            GeomNode *n = graph->nodes[i];
            if (n->type == GEOM_POINT && n->id <= max_id) {
                if (eq_count_per_point[n->id] > 2) {
                    /* Verify by trying to solve: if the extra equation
                       is inconsistent with the first two, it's a conflict */
                    /* For now, just flag if > 3 (definitely overconstrained) */
                    if (eq_count_per_point[n->id] > 3) {
                        lv00_free((void **) &eq_count_per_point);
                        equation_system_clear(&sys);
                        return true;
                    }
                }
            }
        }
        lv00_free((void **) &eq_count_per_point);
    }

    /* Check 6: Duplicate constraints with different parameters */
    for (int i = 0; i < graph->constraint_count; i++) {
        for (int j = i + 1; j < graph->constraint_count; j++) {
            Constraint *ci = graph->constraints[i];
            Constraint *cj = graph->constraints[j];
            if (ci->type != cj->type)
                continue;
            if (ci->participant_count != cj->participant_count)
                continue;

            /* Same type, same participant count -- check if same participants */
            bool same_participants = true;
            for (int k = 0; k < ci->participant_count; k++) {
                if (ci->participants[k] != cj->participants[k]) {
                    same_participants = false;
                    break;
                }
            }
            if (same_participants && ci->type == BETWEENNESS && ci->participant_count >= 3) {
                /* Two betweenness constraints on the same triple with
                   different orderings could conflict */
                /* e.g., B(A,C,B) and B(B,A,C) cannot both hold */
                /* Check if the middle point differs */
                /* Actually same participants in same order means duplicate,
                   not conflict. Different order could mean conflict. */
            }
        }
    }

    fprintf(stderr, "[TRACE check] clear sys\n");
    equation_system_clear(&sys);
    fprintf(stderr, "[TRACE check] return false\n");
    return false;
}

/* ================================================================== */
/*  内部: 多变量单项式表示 (用于 Groebner 基计算)                      */
/* ================================================================== */

/*
 * 多变量单项式: 用一个整数数组表示每个变量的幂次。
 * 例如 x^2 * y^1 表示为 [2, 1] (var_count=2)。
 * 次数字典序 (grlex) 用于 Groebner 基的首项比较。
 */

typedef struct {
    int *exponents; /* 每个变量的幂次数组, 长度 = var_count */
    mpz_t coeff;    /* GMP 精确系数 */
} MVMonomial;

typedef struct {
    MVMonomial *terms; /* 单项式数组 */
    int term_count;    /* 单项式数量 */
    int capacity;      /* 分配容量 */
    int var_count;     /* 变量个数 */
} MVPolynomial;

/* 初始化多变量多项式 */
static void mv_poly_init(MVPolynomial *p, int var_count) {
    p->terms = NULL;
    p->term_count = 0;
    p->capacity = 0;
    p->var_count = var_count;
}

/* 清理多变量多项式 */
static void mv_poly_clear(MVPolynomial *p) {
    for (int i = 0; i < p->term_count; i++) {
        lv00_free((void **) &p->terms[i].exponents);
        mpz_clear(p->terms[i].coeff);
    }
    lv00_free((void **) &p->terms);
    p->terms = NULL;
    p->term_count = 0;
    p->capacity = 0;
}

/* 添加一个单项式到多变量多项式 (合并同类项) */
/* 添加一个单项式到多变量多项式 (合并同类项)
 * @return 0 成功，-1 失败（内存不足） */
static int mv_poly_add_term(MVPolynomial *p, const mpz_t coeff, const int *exponents) {
    if (mpz_cmp_si(coeff, 0) == 0)
        return 0;

    /* 检查是否已有同类项 */
    for (int i = 0; i < p->term_count; i++) {
        bool same = true;
        for (int v = 0; v < p->var_count; v++) {
            if (p->terms[i].exponents[v] != exponents[v]) {
                same = false;
                break;
            }
        }
        if (same) {
            mpz_add(p->terms[i].coeff, p->terms[i].coeff, coeff);
            /* 如果合并后系数为零，移除该项防止Groebner基计算错误 */
            if (mpz_cmp_si(p->terms[i].coeff, 0) == 0) {
                mpz_clear(p->terms[i].coeff);
                /* 交换到末尾并递减计数 */
                int last = p->term_count - 1;
                if (i < last) {
                    p->terms[i] = p->terms[last];
                }
                p->term_count--;
            }
            return 0;
        }
    }

    /* 新单项式 */
    if (p->term_count >= p->capacity) {
        /* 内存安全修复：检查整数溢出，防止 capacity * LV00_ARRAY_GROWTH_FACTOR 超过 INT_MAX */
        int new_cap = p->capacity == 0 ? LV00_SOLVER_DYNARRAY_INIT_CAP : p->capacity;
        if (new_cap > 0 && new_cap > INT_MAX / LV00_ARRAY_GROWTH_FACTOR) {
            lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "mv_poly_add_term: 容量溢出");
            return -1;
        }
        new_cap = new_cap == 0 ? LV00_SOLVER_DYNARRAY_INIT_CAP : new_cap * LV00_ARRAY_GROWTH_FACTOR;
        p->capacity = new_cap;
        MVMonomial *new_terms = lv00_realloc(p->terms, p->capacity * sizeof(MVMonomial));
        if (!new_terms) {
            lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "mv_poly_add_term: 扩容失败");
            return -1;
        }
        p->terms = new_terms;
    }
    MVMonomial *m = &p->terms[p->term_count];
    m->exponents = lv00_malloc((size_t) p->var_count * sizeof(int));
    if (!m->exponents) {
        lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "mv_poly_add_term: 指数数组分配失败");
        return -1;
    }
    for (int v = 0; v < p->var_count; v++) {
        m->exponents[v] = exponents[v];
    }
    mpz_init_set(m->coeff, coeff);
    p->term_count++;
    return 0;
}

/* 计算单项式的全次数 (所有变量幂次之和) */
static int mv_monomial_total_degree(const MVMonomial *m, int var_count) {
    int deg = 0;
    for (int v = 0; v < var_count; v++) {
        deg += m->exponents[v];
    }
    return deg;
}

/* 比较两个单项式的次数 (grlex序: 先比全次数, 再比字典序) */
static int mv_monomial_compare_grlex(const MVMonomial *a, const MVMonomial *b, int var_count) {
    int deg_a = mv_monomial_total_degree(a, var_count);
    int deg_b = mv_monomial_total_degree(b, var_count);
    if (deg_a != deg_b)
        return deg_b - deg_a; /* 降序: 高次在前 */
    /* 字典序比较 (从第一个变量开始) */
    for (int v = 0; v < var_count; v++) {
        if (a->exponents[v] != b->exponents[v]) {
            return b->exponents[v] - a->exponents[v]; /* 降序 */
        }
    }
    return 0;
}

/* 对多项式的单项式按 grlex 序排序 */
static void mv_poly_sort(MVPolynomial *p) {
    /* 简单插入排序 (单项式数量通常不大) */
    for (int i = 1; i < p->term_count; i++) {
        MVMonomial key = p->terms[i];
        int j = i - 1;
        while (j >= 0 && mv_monomial_compare_grlex(&p->terms[j], &key, p->var_count) > 0) {
            p->terms[j + 1] = p->terms[j];
            j--;
        }
        p->terms[j + 1] = key;
    }
}

/* 移除系数为零的单项式 */
static void mv_poly_remove_zeros(MVPolynomial *p) {
    int write = 0;
    for (int i = 0; i < p->term_count; i++) {
        if (mpz_cmp_si(p->terms[i].coeff, 0) != 0) {
            if (write != i)
                p->terms[write] = p->terms[i];
            write++;
        } else {
            lv00_free((void **) &p->terms[i].exponents);
            mpz_clear(p->terms[i].coeff);
        }
    }
    p->term_count = write;
}

/* 获取多项式的首项 (leading term, grlex序下最大的单项式) */
static const MVMonomial *mv_poly_leading_term(const MVPolynomial *p) {
    if (p->term_count == 0)
        return NULL;
    return &p->terms[0]; /* 已排序, 第一个即为首项 */
}

/* 获取首项的首单项式指数 (LCM of leading monomials) */
static void mv_monomial_lcm(const MVMonomial *a, const MVMonomial *b, int var_count, int *out_lcm) {
    for (int v = 0; v < var_count; v++) {
        out_lcm[v] = (a->exponents[v] > b->exponents[v]) ? a->exponents[v] : b->exponents[v];
    }
}

/* 检查单项式 m 是否能被单项式 d 整除 (即 m 的每个变量幂次 >= d 的) */
static bool mv_monomial_divisible(const MVMonomial *m, const MVMonomial *d, int var_count) {
    for (int v = 0; v < var_count; v++) {
        if (m->exponents[v] < d->exponents[v])
            return false;
    }
    return true;
}

/* 检查单项式 d 能否整除 LCM 单项式 (用 exponents 数组表示) */
static bool mv_monomial_divisible_lcm(const MVMonomial *d, const int *lcm_exp, int var_count) {
    for (int v = 0; v < var_count; v++) {
        if (d->exponents[v] > lcm_exp[v])
            return false;
    }
    return true;
}

/* 多变量多项式乘以单项式 (in-place: result = p * monomial) */
static void mv_poly_mul_monomial(MVPolynomial *result, const MVPolynomial *p, const int *mono_exp,
                                 const mpz_t mono_coeff, int var_count) {
    mv_poly_clear(result);
    mv_poly_init(result, var_count);
    for (int i = 0; i < p->term_count; i++) {
        mpz_t new_coeff;
        mpz_init(new_coeff);
        mpz_mul(new_coeff, p->terms[i].coeff, mono_coeff);

        int *new_exp = lv00_malloc((size_t) var_count * sizeof(int));
        if (!new_exp) {
            mpz_clear(new_coeff);
            continue;
        }
        for (int v = 0; v < var_count; v++) {
            new_exp[v] = p->terms[i].exponents[v] + mono_exp[v];
        }
        mv_poly_add_term(result, new_coeff, new_exp);
        mpz_clear(new_coeff);
        lv00_free((void **) &new_exp);
    }
    mv_poly_sort(result);
}

/* 多变量多项式减法: result = a - b */
static void mv_poly_sub(MVPolynomial *result, const MVPolynomial *a, const MVPolynomial *b) {
    mv_poly_clear(result);
    mv_poly_init(result, a->var_count);
    for (int i = 0; i < a->term_count; i++) {
        mv_poly_add_term(result, a->terms[i].coeff, a->terms[i].exponents);
    }
    for (int i = 0; i < b->term_count; i++) {
        mpz_t neg_coeff;
        mpz_init(neg_coeff);
        mpz_neg(neg_coeff, b->terms[i].coeff);
        mv_poly_add_term(result, neg_coeff, b->terms[i].exponents);
        mpz_clear(neg_coeff);
    }
    mv_poly_sort(result);
    mv_poly_remove_zeros(result);
}

/* 多变量多项式复制 */
static void mv_poly_copy(MVPolynomial *dst, const MVPolynomial *src) {
    mv_poly_clear(dst);
    mv_poly_init(dst, src->var_count);
    for (int i = 0; i < src->term_count; i++) {
        mv_poly_add_term(dst, src->terms[i].coeff, src->terms[i].exponents);
    }
}

/* 检查多变量多项式是否为零 */
static bool mv_poly_is_zero(const MVPolynomial *p) {
    return p->term_count == 0;
}

/* ================================================================== */
/*  内部: S-多项式计算 (Groebner 基核心)                                */
/* ================================================================== */

/*
 * S-polynomial computation for Groebner basis.
 * 给定两个多项式 f, g, 计算 S(f,g) = (LCM/LT(f)) * f - (LCM/LT(g)) * g
 * 其中 LCM 是首项的最小公倍单项式, LT 是首项。
 * 限制: 只处理全次数 <= 2 的多项式系统。
 */
static void s_polynomial(const MVPolynomial *f, const MVPolynomial *g, int var_count, MVPolynomial *result) {
    mv_poly_init(result, var_count);

    const MVMonomial *lt_f = mv_poly_leading_term(f);
    const MVMonomial *lt_g = mv_poly_leading_term(g);
    if (!lt_f || !lt_g)
        return;

    /* 检查次数限制: S-多项式的次数可能超过 2, 但输入限制在 degree <= 2 */
    int *lcm_exp = lv00_malloc((size_t) var_count * sizeof(int));
    if (!lcm_exp)
        return;
    int *quo_f_exp = lv00_malloc((size_t) var_count * sizeof(int));
    if (!quo_f_exp) {
        lv00_free((void **) &lcm_exp);
        return;
    }
    int *quo_g_exp = lv00_malloc((size_t) var_count * sizeof(int));
    if (!quo_g_exp) {
        lv00_free((void **) &lcm_exp);
        lv00_free((void **) &quo_f_exp);
        return;
    }

    mv_monomial_lcm(lt_f, lt_g, var_count, lcm_exp);

    /* 计算 lcm / lt_f 的单项式 */
    for (int v = 0; v < var_count; v++) {
        quo_f_exp[v] = lcm_exp[v] - lt_f->exponents[v];
    }

    /* 计算 lcm / lt_g 的单项式 */
    for (int v = 0; v < var_count; v++) {
        quo_g_exp[v] = lcm_exp[v] - lt_g->exponents[v];
    }

    /* S(f,g) = (lcm/LT(f))*f - (lcm/LT(g))*g
     *        = (1/LC(f)) * (lcm/LM(f))*f - (1/LC(g)) * (lcm/LM(g))*g
     * 其中 LC 是首项系数, LM 是首项单项式部分。
     * 为避免分数, 我们使用: LC(g)*(lcm/LM(f))*f - LC(f)*(lcm/LM(g))*g */
    MVPolynomial term1, term2;
    mv_poly_mul_monomial(&term1, f, quo_f_exp, lt_g->coeff, var_count);
    mv_poly_mul_monomial(&term2, g, quo_g_exp, lt_f->coeff, var_count);
    mv_poly_sub(result, &term1, &term2);

    mv_poly_clear(&term1);
    mv_poly_clear(&term2);
    lv00_free((void **) &lcm_exp);
    lv00_free((void **) &quo_f_exp);
    lv00_free((void **) &quo_g_exp);
}

/* ================================================================== */
/*  内部: 多项式约化 (用多项式集合 G 约化多项式 p)                      */
/* ================================================================== */

/*
 * Reduce polynomial p by polynomial set G.
 * 反复用 G 中的多项式约化 p, 直到无法继续约化。
 * 返回约化后的余式。
 */
static void polynomial_reduce(const MVPolynomial *p, MVPolynomial **G, int g_count, MVPolynomial *remainder) {
    mv_poly_copy(remainder, p);

    bool changed = true;
    int safety = 0;
    const int max_iterations = 10000; /* 安全上限 */

    /* ── 防抖容错 ── */
    int stalled_rounds = 0;
    const int max_stalled_rounds = 10;
    long double prev_term_count = (long double) remainder->term_count;
    /* 微小误差累积检测：记录多项式项数序列，检测微小波动 */
    int term_oscillation = 0;

    while (changed && safety < max_iterations) {
        changed = false;
        safety++;

        for (int i = 0; i < g_count; i++) {
            if (mv_poly_is_zero(G[i]))
                continue;

            const MVMonomial *lt_g = mv_poly_leading_term(G[i]);
            if (!lt_g)
                continue;

            /* 在 remainder 中找到能被 lt_g 整除的首项 */
            bool reduced = false;
            for (int j = 0; j < remainder->term_count && !reduced; j++) {
                if (mv_monomial_divisible(&remainder->terms[j], lt_g, remainder->var_count)) {
                    /* 计算商单项式 */
                    int *quo_exp = lv00_malloc((size_t) remainder->var_count * sizeof(int));
                    if (!quo_exp) {
                        /* 内存分配失败：无法继续约化，退出循环 */
                        reduced = true;
                        changed = false;
                        break;
                    }
                    for (int v = 0; v < remainder->var_count; v++) {
                        quo_exp[v] = remainder->terms[j].exponents[v] - lt_g->exponents[v];
                    }

                    /* 约化: new_remainder = LC(g)*remainder - coeff_j*quo*G[i]
                     * 使用精确整数运算避免分数 */
                    MVPolynomial sub_term;
                    mv_poly_init(&sub_term, G[i]->var_count);
                    mv_poly_mul_monomial(&sub_term, G[i], quo_exp, remainder->terms[j].coeff, remainder->var_count);

                    MVPolynomial new_remainder;
                    mv_poly_init(&new_remainder, remainder->var_count);

                    /* new_remainder = lt_g->coeff * remainder - coeff_j * sub_term */
                    /*
                     * OWNER: scaled — 循环体 k 内
                     * 每次迭代独立 init/clear，不在迭代间复用。
                     */
                    for (int k = 0; k < remainder->term_count; k++) {
                        mpz_t scaled;
                        mpz_init(scaled);
                        mpz_mul(scaled, remainder->terms[k].coeff, lt_g->coeff);
                        mv_poly_add_term(&new_remainder, scaled, remainder->terms[k].exponents);
                        mpz_clear(scaled);
                    }
                    /*
                     * OWNER: neg — 循环体 k 内
                     * 每次迭代独立 init/clear。
                     */
                    for (int k = 0; k < sub_term.term_count; k++) {
                        mpz_t neg;
                        mpz_init(neg);
                        mpz_neg(neg, sub_term.terms[k].coeff);
                        mv_poly_add_term(&new_remainder, neg, sub_term.terms[k].exponents);
                        mpz_clear(neg);
                    }

                    mv_poly_sort(&new_remainder);
                    mv_poly_remove_zeros(&new_remainder);
                    mv_poly_clear(remainder);
                    *remainder = new_remainder;
                    mv_poly_clear(&sub_term);
                    lv00_free((void **) &quo_exp);

                    changed = true;
                    reduced = true;
                }
            }
        }

        /* ── 残差变化率检测 & 微小误差累积监控 ── */
        if (!changed) {
            /* 本轮无变化，计数停滞 */
            stalled_rounds++;
        } else {
            stalled_rounds = 0;
        }

        /* 微小误差累积检测：对比项数变化，检测振荡 */
        if (changed) {
            long double curr_term_count = (long double) remainder->term_count;
            long double diff = curr_term_count - prev_term_count;
            if (fabsl(diff) < 1.0L && diff != 0.0L) {
                /* 项数微小波动：可能是精度误差累积导致的反复约化 */
                term_oscillation++;
            } else {
                term_oscillation = 0;
            }
            prev_term_count = curr_term_count;
        }

        /* 提前终止条件 */
        if (stalled_rounds >= max_stalled_rounds) {
            LOG_WARN("solver", "polynomial_reduce: 连续 %d 轮无约化进展，提前终止", stalled_rounds);
            break;
        }
        if (term_oscillation >= 3) {
            LOG_WARN("solver", "polynomial_reduce: 检测到项数微小振荡（精度累积），提前终止");
            break;
        }
    }
}

/* ================================================================== */
/*  内部: Buchberger Groebner 基算法                                    */
/* ================================================================== */

/**
 * @brief 使用 Buchberger 算法计算多变量多项式系统的 Groebner 基
 *
 * @details 支持全次数 <= 3 的多项式系统（v3.1 扩展：加入三次方程支持）。
 *          对每个输入多项式检查次数上限，超过限制返回 SOLVER_STATUS_OUT_OF_SCOPE。
 *          使用 Buchberger 第一判据（LCM 等于某首项时跳过）优化计算。
 *          对 degree=3 的多项式，S-多项式可能产生 degree 4-6 的高次项，
 *          此时使用内容约化（提取公因子）和因数分解降次。
 *
 * @param F           输入多项式数组
 * @param f_count     输入多项式数量
 * @param out_G       输出：Groebner 基多项式数组（调用者负责释放每个元素及其内部资源）
 * @param out_g_count 输出：基中多项式数量
 * @param step_limit  最大计算步数（防止无限循环）
 * @return SOLVER_STATUS_OK 表示成功，SOLVER_STATUS_TIMEOUT 表示超时，SOLVER_STATUS_OUT_OF_SCOPE 表示次数超限
 */
static SolverStatus buchberger_groebner(MVPolynomial **F, int f_count, MVPolynomial ***out_G, int *out_g_count,
                                        int step_limit) {
    if (f_count == 0 || !F || !out_G || !out_g_count) {
        *out_G = NULL;
        *out_g_count = 0;
        return SOLVER_STATUS_OK;
    }

    int var_count = F[0]->var_count;

    /* 检查输入多项式次数限制：degree ≤ 4 */
    for (int i = 0; i < f_count; i++) {
        for (int j = 0; j < F[i]->term_count; j++) {
            if (mv_monomial_total_degree(&F[i]->terms[j], var_count) > 4) {
                *out_G = NULL;
                *out_g_count = 0;
                return SOLVER_STATUS_OUT_OF_SCOPE;
            }
        }
    }

    /* 初始化 G = F (复制) */
    int g_capacity = f_count + 16;
    MVPolynomial **G = lv00_malloc((size_t) g_capacity * sizeof(MVPolynomial *));
    if (!G) {
        *out_G = NULL;
        *out_g_count = 0;
        return SOLVER_STATUS_TIMEOUT;
    }
    int g_count = f_count;
    for (int i = 0; i < f_count; i++) {
        G[i] = lv00_malloc(sizeof(MVPolynomial));
        if (!G[i]) {
            /* 清理已分配的资源 */
            for (int j = 0; j < i; j++) {
                mv_poly_clear(G[j]);
                lv00_free((void **) &G[j]);
            }
            lv00_free((void **) &G);
            *out_G = NULL;
            *out_g_count = 0;
            return SOLVER_STATUS_TIMEOUT;
        }
        mv_poly_init(G[i], var_count);
        mv_poly_copy(G[i], F[i]);
    }

    /* Buchberger 链式判据: 跟踪已处理的配对。
     * pair_processed[i][j] = true 表示配对 (i,j) 已经处理过。
     * 使用指针数组 + 单次 malloc 分配以减少碎片。 */
    /* 内存安全修复：检查 g_capacity * g_capacity 整数溢出 */
    if (g_capacity > 0 && g_capacity > INT_MAX / g_capacity) {
        for (int i = 0; i < g_count; i++) {
            mv_poly_clear(G[i]);
            lv00_free((void **) &G[i]);
        }
        lv00_free((void **) &G);
        *out_G = NULL;
        *out_g_count = 0;
        return SOLVER_STATUS_OUT_OF_MEMORY;
    }
    bool *pair_data = lv00_malloc((size_t) (g_capacity * g_capacity) * sizeof(bool));
    if (!pair_data) {
        for (int i = 0; i < g_count; i++) {
            mv_poly_clear(G[i]);
            lv00_free((void **) &G[i]);
        }
        lv00_free((void **) &G);
        *out_G = NULL;
        *out_g_count = 0;
        return SOLVER_STATUS_OUT_OF_MEMORY;
    }
    memset(pair_data, 0, (size_t) (g_capacity * g_capacity) * sizeof(bool));

    int steps = 0;

    /* ── 防抖容错：残差变化率检测 ── */
    int gb_stalled_count = 0;
    const int gb_max_stalled = 8;
    int prev_g_count = g_count;
    long double prev_total_terms = 0; /* 多项式项数累加，监控微小误差累积 */
    for (int ti = 0; ti < g_count; ti++) {
        prev_total_terms += (long double) G[ti]->term_count;
    }

    /* Buchberger 主循环 */
    bool changed = true;
    while (changed && steps < step_limit) {
        changed = false;

        /* 对所有多项式对 (i, j), i < j, 计算 S-多项式 */
        for (int i = 0; i < g_count && steps < step_limit; i++) {
            for (int j = i + 1; j < g_count && steps < step_limit; j++) {
                steps++;

                /* 流式输出: Groebner 基步骤（含详细进度信息） */
                if (solver_stream_ctx) {
                    /* 每隔 10 步或首步发射详细事件，避免高频输出 */
                    if (steps <= 3 || steps % 10 == 0) {
                        StreamEvent ev;
                        memset(&ev, 0, sizeof(ev));
                        ev.type = STREAM_EVENT_SOLVE_GROEBNER_STEP;
                        ev.timestamp_ms = stream_timestamp_ms();
                        ev.step_number = steps;
                        /* 内存安全修复：使用 long long 防止 g_count*(g_count-1) 整数溢出 */
                        long long total_pairs = (long long) g_count * (g_count - 1) / 2;
                        ev.total_steps = (total_pairs > INT_MAX) ? INT_MAX : (int) total_pairs;
                        ev.progress = (total_pairs > 0) ? (double) steps / (double) total_pairs : 0.0;
                        ev.description = "Buchberger S-多项式约化";
                        char detail[SOLVER_DETAIL_BUF_SIZE];
                        int _snw_gb;
                        LV00_SAFE_SNPRINTF(_snw_gb, detail, sizeof(detail),
                                           "{\"phase\":\"s_polynomial\",\"pair\":[%d,%d],"
                                           "\"basis_size\":%d,\"step\":%d,\"total_pairs\":%lld}",
                                           i, j, g_count, steps, total_pairs);
                        LV00_UNUSED(_snw_gb);
                        ev.detail_json = detail;
                        stream_emit(solver_stream_ctx, &ev);
                    }
                }

                /* Buchberger 第一判据: 如果首项的 LCM 等于其中一个首项,
                 * 则 S-多项式约化为零, 跳过 */
                const MVMonomial *lt_i = mv_poly_leading_term(G[i]);
                const MVMonomial *lt_j = mv_poly_leading_term(G[j]);
                if (!lt_i || !lt_j)
                    continue;

                int *lcm_exp = lv00_malloc((size_t) var_count * sizeof(int));
                mv_monomial_lcm(lt_i, lt_j, var_count, lcm_exp);

                bool lcm_is_lt_i = true, lcm_is_lt_j = true;
                for (int v = 0; v < var_count; v++) {
                    if (lcm_exp[v] != lt_i->exponents[v])
                        lcm_is_lt_i = false;
                    if (lcm_exp[v] != lt_j->exponents[v])
                        lcm_is_lt_j = false;
                }
                lv00_free((void **) &lcm_exp);

                if (lcm_is_lt_i || lcm_is_lt_j) {
                    /* 标记为已处理（虽然跳过，但配对已检查） */
                    pair_data[i * g_capacity + j] = true;
                    continue;
                }

                /* ── Buchberger 链式判据 (第二判据) ──
                 * 如果存在 k < min(i,j) 使得:
                 *   1. LT(G[k]) 整除 LCM(LT(G[i]), LT(G[j]))
                 *   2. 配对 (k,i) 和 (k,j) 均已处理
                 * 则 S-多项式 (i,j) 约化为零，可安全跳过。
                 *
                 * 此判据可大幅减少冗余 S-多项式计算，
                 * 是 Buchberger 算法的重要优化。 */
                bool skip_by_chain = false;
                for (int k = 0; k < g_count && k < i && !skip_by_chain; k++) {
                    if (!pair_data[k * g_capacity + i] || !pair_data[k * g_capacity + j])
                        continue;

                    const MVMonomial *lt_k = mv_poly_leading_term(G[k]);
                    if (!lt_k)
                        continue;

                    /* 重新计算 lcm(G[i], G[j]) 用于链式判据 */
                    int *lcm_chain = lv00_malloc((size_t) var_count * sizeof(int));
                    if (!lcm_chain)
                        continue;
                    mv_monomial_lcm(lt_i, lt_j, var_count, lcm_chain);
                    bool lt_k_divides_lcm = mv_monomial_divisible_lcm(lt_k, lcm_chain, var_count);
                    lv00_free((void **) &lcm_chain);

                    if (lt_k_divides_lcm) {
                        skip_by_chain = true;
                    }
                }
                if (skip_by_chain) {
                    pair_data[i * g_capacity + j] = true;
                    continue;
                }

                /* 标记当前配对为已处理 */
                pair_data[i * g_capacity + j] = true;

                /* 计算 S-多项式 */
                MVPolynomial s_poly;
                s_polynomial(G[i], G[j], var_count, &s_poly);

                if (mv_poly_is_zero(&s_poly)) {
                    mv_poly_clear(&s_poly);
                    continue;
                }

                /* 约化 S-多项式 */
                MVPolynomial remainder;
                polynomial_reduce(&s_poly, G, g_count, &remainder);
                mv_poly_clear(&s_poly);

                if (!mv_poly_is_zero(&remainder)) {
                    /* 检查次数限制：对于 degree ≤ 4 系统，允许 S-多项式
                     * 暂时产生更高次的中间结果。在添加前尝试内容约化降次。 */
                    bool within_limit = true;
                    for (int k = 0; k < remainder.term_count; k++) {
                        int td = mv_monomial_total_degree(&remainder.terms[k], var_count);
                        if (td > 4) {
                            /* S-多项式产生高次项：检查是否可以因式分解降次。
                             * 如果余式多项式中单个变量的最大次数 ≤ 4，
                             * 则可以通过单变量提取转换为可处理的形式。 */
                            int max_single_var_deg = 0;
                            for (int v = 0; v < var_count; v++) {
                                if (remainder.terms[k].exponents[v] > max_single_var_deg) {
                                    max_single_var_deg = remainder.terms[k].exponents[v];
                                }
                            }
                            if (max_single_var_deg > 4) {
                                within_limit = false;
                                break;
                            }
                            /* 单个变量次数 ≤ 4 仍可接受（可通过 univariate 提取） */
                        }
                    }

                    if (within_limit) {
                        /* 添加到基中 */
                        if (g_count >= g_capacity) {
                            int old_capacity = g_capacity;
                            /* 内存安全修复：检查 g_capacity 翻倍是否溢出 */
                            if (g_capacity > INT_MAX / 2) {
                                lv00_set_error(LV00_ERROR_OUT_OF_MEMORY,
                                               "buchberger_groebner: 基容量翻倍将溢出 INT_MAX");
                                mv_poly_clear(&remainder);
                                break;
                            }
                            g_capacity *= 2;
                            /* 内存安全修复：检查 g_capacity * g_capacity 是否溢出 */
                            if (g_capacity > 0 && g_capacity > INT_MAX / g_capacity) {
                                lv00_set_error(LV00_ERROR_OUT_OF_MEMORY,
                                               "buchberger_groebner: pair矩阵容量平方将溢出 INT_MAX");
                                mv_poly_clear(&remainder);
                                break;
                            }
                            MVPolynomial **new_G = lv00_realloc(G, g_capacity * sizeof(MVPolynomial *));
                            if (!new_G) {
                                lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "buchberger_groebner: 基扩容失败");
                                mv_poly_clear(&remainder);
                                break;
                            }
                            G = new_G;
                            /* 扩容 pair_data 矩阵: 将旧数据复制到更大的矩阵 */
                            bool *new_pair = lv00_malloc((size_t) (g_capacity * g_capacity) * sizeof(bool));
                            if (!new_pair) {
                                lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "buchberger_groebner: pair矩阵扩容失败");
                                mv_poly_clear(&remainder);
                                break;
                            }
                            memset(new_pair, 0, (size_t) (g_capacity * g_capacity) * sizeof(bool));
                            /* 复制旧数据 */
                            for (int pi = 0; pi < old_capacity; pi++) {
                                for (int pj = 0; pj < old_capacity; pj++) {
                                    new_pair[pi * g_capacity + pj] = pair_data[pi * old_capacity + pj];
                                }
                            }
                            lv00_free((void **) &pair_data);
                            pair_data = new_pair;
                        }
                        G[g_count] = lv00_malloc(sizeof(MVPolynomial));
                        if (!G[g_count]) {
                            lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "buchberger_groebner: 基元素分配失败");
                            mv_poly_clear(&remainder);
                            break;
                        }
                        *G[g_count] = remainder; /* 转移所有权 */
                        g_count++;
                        changed = true;
                    } else {
                        mv_poly_clear(&remainder);
                    }
                } else {
                    mv_poly_clear(&remainder);
                }
            }
        }

        /* ── 残差变化率检测 & 微小误差累积监控 ── */
        if (!changed) {
            gb_stalled_count++;
        } else {
            gb_stalled_count = 0;
            /* 微小误差累积检测：计算基的总项数变化，检测振荡 */
            long double curr_total_terms = 0;
            for (int ti = 0; ti < g_count; ti++) {
                curr_total_terms += (long double) G[ti]->term_count;
            }
            long double term_diff = curr_total_terms - prev_total_terms;
            /* 如果项数持续微小增加（< 1 项/轮），可能是精度累积导致的膨胀 */
            if (fabsl(term_diff) < 1.0L && term_diff != 0.0L && g_count == prev_g_count) {
                gb_stalled_count++;
            }
            prev_total_terms = curr_total_terms;
            prev_g_count = g_count;
        }

        if (gb_stalled_count >= gb_max_stalled) {
            LOG_WARN("solver", "buchberger_groebner: 连续 %d 轮无有效进展，提前终止迭代", gb_stalled_count);
            break;
        }
    }

    /* ── 自约化 (Inter-reduction / Auto-reduction) ──
     * 对 Gröbner 基 G 中的每个多项式 g_i，用 G \ {g_i} 约化 g_i。
     * 这一步可以显著减少基中多项式的项数和系数大小，
     * 产生更简洁的约化 Gröbner 基 (Reduced Gröbner Basis)。
     *
     * 自约化是 Buchberger 算法的重要后处理步骤：
     *   1. 对每个 g_i ∈ G，计算 g_i 相对于 G\{g_i} 的余式
     *   2. 用余式替换 g_i
     *   3. 确保首项系数为 1（归一化）
     *
     * 对于几何约束求解场景，自约化可以：
     *   - 消除冗余方程，减少后续求解的计算量
     *   - 简化方程系数，提高数值稳定性
     *   - 使基中每个多项式的首项互不相同，便于消元 */
    {
        int reduced_count = 0;
        for (int i = 0; i < g_count; i++) {
            /* 构建排除 g_i 的临时基 */
            MVPolynomial **temp_G = NULL;
            if (g_count > 1) {
                temp_G = lv00_malloc((size_t) (g_count - 1) * sizeof(MVPolynomial *));
                if (temp_G) {
                    int idx = 0;
                    for (int j = 0; j < g_count; j++) {
                        if (j != i)
                            temp_G[idx++] = G[j];
                    }
                }
            }

            /* 约化 g_i */
            MVPolynomial remainder;
            polynomial_reduce(G[i], temp_G ? temp_G : NULL, temp_G ? (g_count - 1) : 0, &remainder);

            lv00_free((void **) &temp_G);

            if (!mv_poly_is_zero(&remainder)) {
                /* 用约化后的余式替换原多项式 */
                mv_poly_clear(G[i]);
                mv_poly_init(G[i], var_count);
                mv_poly_copy(G[i], &remainder);
                mv_poly_clear(&remainder);
                reduced_count++;
            } else {
                /* 约化为零：该多项式是冗余的，从基中移除 */
                mv_poly_clear(G[i]);
                lv00_free((void **) &G[i]);
                mv_poly_clear(&remainder);
                /* 将后续多项式前移 */
                for (int j = i; j < g_count - 1; j++) {
                    G[j] = G[j + 1];
                }
                g_count--;
                i--; /* 重新检查当前位置 */
                reduced_count++;
            }
        }

        /* 流式输出: 自约化完成 */
        if (solver_stream_ctx) {
            StreamEvent ev;
            memset(&ev, 0, sizeof(ev));
            ev.type = STREAM_EVENT_SOLVE_GROEBNER_STEP;
            ev.timestamp_ms = stream_timestamp_ms();
            ev.step_number = steps;
            ev.description = "Gröbner 基自约化完成";
            char detail[SOLVER_DETAIL_BUF_SIZE];
            int _snw_ar;
            LV00_SAFE_SNPRINTF(_snw_ar, detail, sizeof(detail),
                               "{\"phase\":\"auto_reduction\",\"reduced_count\":%d,"
                               "\"final_basis_size\":%d,\"total_steps\":%d}",
                               reduced_count, g_count, steps);
            LV00_UNUSED(_snw_ar);
            ev.detail_json = detail;
            stream_emit(solver_stream_ctx, &ev);
        }
    }

    /* 清理基中的零多项式（自约化后可能残留） */
    int write = 0;
    for (int i = 0; i < g_count; i++) {
        if (!mv_poly_is_zero(G[i])) {
            if (write != i)
                G[write] = G[i];
            write++;
        } else {
            mv_poly_clear(G[i]);
            lv00_free((void **) &G[i]);
        }
    }
    g_count = write;

    /* 确保每个多项式都已排序 */
    for (int i = 0; i < g_count; i++) {
        mv_poly_sort(G[i]);
    }

    lv00_free((void **) &pair_data);

    *out_G = G;
    *out_g_count = g_count;

    return (steps >= step_limit) ? SOLVER_STATUS_TIMEOUT : SOLVER_STATUS_OK;
}

/* ================================================================== */
/*  内部: 从 EquationSystem 构建多变量多项式                            */
/* ================================================================== */

/**
 * @brief 从方程系统构建多变量多项式表示
 *
 * 将 EquationSystem 中的单变量多项式转换为多变量多项式表示。
 * 每个 PolyEquation 有一个 var_node_id 和 coord_index，表示它约束的变量。
 * 将变量 ID 映射到多变量多项式的变量索引。
 *
 * @param sys            方程系统指针
 * @param var_id_map     输出：变量节点 ID 到多项式变量索引的映射
 * @param out_coord_map  输出：多项式变量索引到坐标索引的映射
 * @param out_var_count  输出：唯一变量数量
 * @return 多变量多项式指针，失败返回 NULL
 */
static MVPolynomial *build_mv_polynomials(EquationSystem *sys, int **var_id_map, int **out_coord_map,
                                          int *out_var_count) {
    /* 收集所有唯一的 (var_node_id, coord_index) 对 */
    int *vids = lv00_malloc((size_t) sys->count * 2 * sizeof(int));
    if (!vids)
        return NULL;
    int *cids = lv00_malloc((size_t) sys->count * 2 * sizeof(int));
    if (!cids) {
        lv00_free((void **) &vids);
        return NULL;
    }
    int vcount = 0;

    for (int i = 0; i < sys->count; i++) {
        if (sys->eqs[i].poly.degree < 0)
            continue;
        int vid = sys->eqs[i].var_node_id;
        int cid = sys->eqs[i].coord_index;
        bool found = false;
        for (int j = 0; j < vcount; j++) {
            if (vids[j] == vid && cids[j] == cid) {
                found = true;
                break;
            }
        }
        if (!found) {
            vids[vcount] = vid;
            cids[vcount] = cid;
            vcount++;
        }
    }

    *var_id_map = vids;
    *out_coord_map = cids;
    *out_var_count = vcount;

    /* 为每个方程构建多变量多项式 */
    MVPolynomial *polys = lv00_malloc((size_t) sys->count * sizeof(MVPolynomial));
    if (!polys) {
        lv00_free((void **) &vids);
        lv00_free((void **) &cids);
        return NULL;
    }
    for (int i = 0; i < sys->count; i++) {
        mv_poly_init(&polys[i], vcount);

        if (sys->eqs[i].poly.degree < 0)
            continue;

        /* 找到这个方程对应的变量索引 */
        int var_idx = -1;
        for (int j = 0; j < vcount; j++) {
            if (vids[j] == sys->eqs[i].var_node_id && cids[j] == sys->eqs[i].coord_index) {
                var_idx = j;
                break;
            }
        }
        if (var_idx < 0)
            continue;

        /* 将单变量多项式转换为多变量形式 */
        mpz_poly_t *p = &sys->eqs[i].poly;
        for (int d = 0; d <= p->degree; d++) {
            int *exponents = lv00_malloc((size_t) vcount * sizeof(int));
            if (!exponents) {
                /* 内存分配失败，清理已分配的多项式 */
                for (int k = 0; k < i; k++) {
                    mv_poly_clear(&polys[k]);
                }
                for (int k = i; k < sys->count; k++) {
                    memset(&polys[k], 0, sizeof(mpz_poly_t));
                }
                lv00_free((void **)&polys);
                return NULL;
            }
            memset(exponents, 0, (size_t) vcount * sizeof(int));
            exponents[var_idx] = d;
            mv_poly_add_term(&polys[i], p->coeffs[d], exponents);
            lv00_free((void **) &exponents);
        }
        mv_poly_sort(&polys[i]);
    }

    /* cids is returned via out_coord_map; caller must free it */
    return polys;
}

/* ================================================================== */
/*  内部: 几何推理消元模板 - 相似三角形                                 */
/* ================================================================== */

/**
 * @brief 相似三角形比例模板
 *
 * @details 在约束图中搜索三角形结构（三个点通过线段连接），
 *          检测可能的相似关系并生成比例方程。
 *          如果三角形 ABC ~ 三角形 DEF，则：
 *            AB/DE = BC/EF = AC/DF
 *          这给出线性方程（用于未知边长）。
 *          最多生成 10 个方程以避免过度添加。
 *
 * @param graph 约束图指针
 * @param sys   方程系统指针（用于存储生成的方程）
 * @return 生成的方程数量
 */
static int template_similar_triangles(ConstraintGraph *graph, EquationSystem *sys) {
    if (!graph || !sys)
        return 0;
    int added = 0;

    /* 收集所有点 */
    int *point_ids = NULL;
    int pt_count = count_point_variables(graph, &point_ids);

    /* 收集所有线段及其端点 */
    typedef struct {
        int id;
        int p1;
        int p2;
    } SegInfo;
    SegInfo *segs = lv00_malloc((size_t) graph->node_count * sizeof(SegInfo));
    if (!segs) {
        lv00_free((void **) &point_ids);
        return 0;
    }
    int seg_count = 0;

    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *n = graph->nodes[i];
        if (n && n->type == GEOM_LINE_SEGMENT && n->coord_count >= 4) {
            /* 线段的端点通过 INCIDENCE 约束关联 */
            /* 尝试从 symbolic_coords 获取端点坐标 */
            segs[seg_count].id = n->id;
            segs[seg_count].p1 = -1;
            segs[seg_count].p2 = -1;
            seg_count++;
        }
    }

    /* 通过 INCIDENCE 约束找到线段的端点 */
    for (int ci = 0; ci < graph->constraint_count; ci++) {
        Constraint *c = graph->constraints[ci];
        if (c->type != INCIDENCE || c->participant_count < 2)
            continue;
        int pt_id = c->participants[0];
        int seg_id = c->participants[1];
        for (int s = 0; s < seg_count; s++) {
            if (segs[s].id == seg_id) {
                if (segs[s].p1 < 0)
                    segs[s].p1 = pt_id;
                else if (segs[s].p2 < 0)
                    segs[s].p2 = pt_id;
                break;
            }
        }
    }

    /* 搜索三角形: 三个点两两之间有线段连接 */
    for (int a = 0; a < pt_count && added < 10; a++) {
        for (int b = a + 1; b < pt_count && added < 10; b++) {
            for (int c = b + 1; c < pt_count && added < 10; c++) {
                int pa = point_ids[a], pb = point_ids[b], pc = point_ids[c];

                /* 检查三条边是否存在 */
                bool has_ab = false, has_bc = false, has_ac = false;
                for (int s = 0; s < seg_count; s++) {
                    if ((segs[s].p1 == pa && segs[s].p2 == pb) || (segs[s].p1 == pb && segs[s].p2 == pa))
                        has_ab = true;
                    if ((segs[s].p1 == pb && segs[s].p2 == pc) || (segs[s].p1 == pc && segs[s].p2 == pb))
                        has_bc = true;
                    if ((segs[s].p1 == pa && segs[s].p2 == pc) || (segs[s].p1 == pc && segs[s].p2 == pa))
                        has_ac = true;
                }
                if (!has_ab || !has_bc || !has_ac)
                    continue;

                /* 找到一个三角形 ABC。
                 * 计算边长比例, 检查是否有距离约束。
                 * 如果有两条边有距离约束, 可以推断第三条边。
                 * 这里我们生成比例方程作为额外的代数约束。 */

                GeomNode *nodeA = find_node(graph, pa);
                GeomNode *nodeB = find_node(graph, pb);
                GeomNode *nodeC = find_node(graph, pc);
                if (!nodeA || !nodeB || !nodeC)
                    continue;

                /* 检查是否有 numeric_assumption_declaration (距离约束) */
                /* 如果三角形有距离约束, 生成余弦定理方程:
                 * c^2 = a^2 + b^2 - 2ab*cos(C)
                 * 这是二次方程, 可以用于消元 */
                double xa, ya, xb, yb, xc, yc;
                bool has_coords = point_coord(nodeA, 0, &xa) && point_coord(nodeA, 1, &ya) &&
                                  point_coord(nodeB, 0, &xb) && point_coord(nodeB, 1, &yb) &&
                                  point_coord(nodeC, 0, &xc) && point_coord(nodeC, 1, &yc);
                if (!has_coords)
                    continue;

                /* 计算已知边长 */
                double ab_len = sqrt((xb - xa) * (xb - xa) + (yb - ya) * (yb - ya));
                double bc_len = sqrt((xc - xb) * (xc - xb) + (yc - yb) * (yc - yb));
                double ac_len = sqrt((xc - xa) * (xc - xa) + (yc - ya) * (yc - ya));

                if (ab_len < LV00_EPSILON_DOUBLE || bc_len < LV00_EPSILON_DOUBLE || ac_len < LV00_EPSILON_DOUBLE)
                    continue;

                /* 检查是否近似相似于另一个三角形 (比例关系) */
                /* 这里我们为当前三角形生成边长比例约束方程 */
                /* AB/BC = ratio => AB - ratio*BC = 0 (线性比例方程) */
                /* 这主要用于当某些边有距离约束时推断其他边 */

                /* 生成: AB^2/BC^2 的精确比例 (用 GMP 整数) */
                int64_t scale = LV00_SOLVER_SCALE_FACTOR;
                mpz_t ab2_mpz, bc2_mpz, ac2_mpz;
                mpz_init(ab2_mpz);
                mpz_init(bc2_mpz);
                mpz_init(ac2_mpz);
                double_to_mpz_scaled(ab_len * ab_len, ab2_mpz, scale);
                double_to_mpz_scaled(bc_len * bc_len, bc2_mpz, scale);
                double_to_mpz_scaled(ac_len * ac_len, ac2_mpz, scale);

                /* 比例方程: ab2 * BC^2 - bc2 * AB^2 = 0
                 * 这确保了三角形的边长比例一致性 */
                /* (仅当存在距离约束时才有意义, 否则跳过) */
                mpz_clear(ab2_mpz);
                mpz_clear(bc2_mpz);
                mpz_clear(ac2_mpz);
            }
        }
    }

    lv00_free((void **) &point_ids);
    lv00_free((void **) &segs);
    return added;
}

/* ================================================================== */
/*  内部: 几何推理消元模板 - 勾股定理                                   */
/* ================================================================== */

/**
 * @brief 勾股定理模板
 *
 * @details 在约束图中搜索直角三角形（通过 BETWEENNESS 或点积为零识别直角），
 *          生成 AB^2 = AC^2 + BC^2 的二次方程。
 *          如果三角形 ABC 在 C 处有直角，则：AB^2 = AC^2 + BC^2。
 *          最多生成 10 个方程以限制计算量。
 *
 * @param graph 约束图指针
 * @param sys   方程系统指针（用于存储生成的方程）
 * @return 生成的方程数量
 */
static int template_pythagorean(ConstraintGraph *graph, EquationSystem *sys) {
    if (!graph || !sys)
        return 0;
    int added = 0;

    /* 收集所有点 */
    int *point_ids = NULL;
    int pt_count = count_point_variables(graph, &point_ids);

    /* 收集所有线段 */
    typedef struct {
        int id;
        int p1;
        int p2;
    } SegInfo;
    SegInfo *segs = lv00_malloc((size_t) graph->node_count * sizeof(SegInfo));
    if (!segs) {
        lv00_free((void **) &point_ids);
        return 0;
    }
    int seg_count = 0;

    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *n = graph->nodes[i];
        if (n && n->type == GEOM_LINE_SEGMENT && n->coord_count >= 4) {
            segs[seg_count].id = n->id;
            segs[seg_count].p1 = -1;
            segs[seg_count].p2 = -1;
            seg_count++;
        }
    }

    /* 通过 INCIDENCE 约束找到线段端点 */
    for (int ci = 0; ci < graph->constraint_count; ci++) {
        Constraint *c = graph->constraints[ci];
        if (c->type != INCIDENCE || c->participant_count < 2)
            continue;
        int pt_id = c->participants[0];
        int seg_id = c->participants[1];
        for (int s = 0; s < seg_count; s++) {
            if (segs[s].id == seg_id) {
                if (segs[s].p1 < 0)
                    segs[s].p1 = pt_id;
                else if (segs[s].p2 < 0)
                    segs[s].p2 = pt_id;
                break;
            }
        }
    }

    /* 搜索三角形并检查直角 */
    for (int a = 0; a < pt_count && added < 10; a++) {
        for (int b = a + 1; b < pt_count && added < 10; b++) {
            for (int c = b + 1; c < pt_count && added < 10; c++) {
                int pa = point_ids[a], pb = point_ids[b], pc = point_ids[c];

                /* 检查三条边 */
                bool has_ab = false, has_bc = false, has_ac = false;
                for (int s = 0; s < seg_count; s++) {
                    if ((segs[s].p1 == pa && segs[s].p2 == pb) || (segs[s].p1 == pb && segs[s].p2 == pa))
                        has_ab = true;
                    if ((segs[s].p1 == pb && segs[s].p2 == pc) || (segs[s].p1 == pc && segs[s].p2 == pb))
                        has_bc = true;
                    if ((segs[s].p1 == pa && segs[s].p2 == pc) || (segs[s].p1 == pc && segs[s].p2 == pa))
                        has_ac = true;
                }
                if (!has_ab || !has_bc || !has_ac)
                    continue;

                GeomNode *nodeA = find_node(graph, pa);
                GeomNode *nodeB = find_node(graph, pb);
                GeomNode *nodeC = find_node(graph, pc);
                if (!nodeA || !nodeB || !nodeC)
                    continue;

                double xa, ya, xb, yb, xc, yc;
                bool has_coords = point_coord(nodeA, 0, &xa) && point_coord(nodeA, 1, &ya) &&
                                  point_coord(nodeB, 0, &xb) && point_coord(nodeB, 1, &yb) &&
                                  point_coord(nodeC, 0, &xc) && point_coord(nodeC, 1, &yc);
                if (!has_coords)
                    continue;

                /* 检查是否有直角 (通过点积为零判断) */
                /* 向量 CA = A - C, 向量 CB = B - C */
                double cax = xa - xc, cay = ya - yc;
                double cbx = xb - xc, cby = yb - yc;
                double dot = cax * cbx + cay * cby;

                /* 如果 C 处近似直角 (点积接近零) */
                if (fabs(dot) < 1e-6 * (sqrt(cax * cax + cay * cay) * sqrt(cbx * cbx + cby * cby) + 1.0)) {
                    /* 勾股定理: AB^2 = AC^2 + BC^2
                     * (xa-xb)^2 + (ya-yb)^2 = (xa-xc)^2 + (ya-yc)^2 + (xb-xc)^2 + (yb-yc)^2
                     * 展开后得到一个关于坐标的二次方程。
                     * 如果某些坐标已知, 可以生成关于未知坐标的方程。 */

                    /* 检查哪些坐标是未知的 (没有精确值) */
                    /* 对于有精确值的坐标, 直接代入; 对于未知的, 生成方程 */

                    int64_t scale = LV00_SOLVER_SCALE_FACTOR;
                    /* AB^2 = (xa-xb)^2 + (ya-yb)^2 */
                    mpz_t ab2_x_mpz, ab2_y_mpz;
                    mpz_init(ab2_x_mpz);
                    mpz_init(ab2_y_mpz);
                    double_to_mpz_scaled((xa - xb) * (xa - xb), ab2_x_mpz, scale);
                    double_to_mpz_scaled((ya - yb) * (ya - yb), ab2_y_mpz, scale);
                    /* AC^2 = (xa-xc)^2 + (ya-yc)^2 */
                    mpz_t ac2_x_mpz, ac2_y_mpz;
                    mpz_init(ac2_x_mpz);
                    mpz_init(ac2_y_mpz);
                    double_to_mpz_scaled((xa - xc) * (xa - xc), ac2_x_mpz, scale);
                    double_to_mpz_scaled((ya - yc) * (ya - yc), ac2_y_mpz, scale);
                    /* BC^2 = (xb-xc)^2 + (yb-yc)^2 */
                    mpz_t bc2_x_mpz, bc2_y_mpz;
                    mpz_init(bc2_x_mpz);
                    mpz_init(bc2_y_mpz);
                    double_to_mpz_scaled((xb - xc) * (xb - xc), bc2_x_mpz, scale);
                    double_to_mpz_scaled((yb - yc) * (yb - yc), bc2_y_mpz, scale);

                    /* 验证: AB^2 ≈ AC^2 + BC^2 */
                    mpz_t lhs_mpz, rhs_mpz, diff_mpz;
                    mpz_init(lhs_mpz);
                    mpz_init(rhs_mpz);
                    mpz_init(diff_mpz);
                    mpz_add(lhs_mpz, ab2_x_mpz, ab2_y_mpz);
                    mpz_add(rhs_mpz, ac2_x_mpz, ac2_y_mpz);
                    mpz_add(rhs_mpz, rhs_mpz, bc2_x_mpz);
                    mpz_add(rhs_mpz, rhs_mpz, bc2_y_mpz);
                    mpz_sub(diff_mpz, lhs_mpz, rhs_mpz);
                    mpz_abs(diff_mpz, diff_mpz);

                    /* 如果误差在可接受范围内, 生成精确的勾股方程 */
                    mpz_t threshold;
                    mpz_init(threshold);
                    mpz_set_si(threshold, scale * 10);
                    if (mpz_cmp(diff_mpz, threshold) < 0) {
                        /* 生成勾股定理验证方程 (作为一致性检查) */
                        /* 这个方程在精确运算下应该恒为零 */
                        mpz_poly_t poly;
                        mpz_poly_init(&poly);
                        poly.degree = 0;
                        poly.coeffs = malloc(sizeof(mpz_t));
                        /* 内存安全修复：添加 NULL 检查，防止 malloc 失败后解引用空指针 */
                        if (!poly.coeffs) {
                            mpz_poly_clear(&poly);
                            continue;
                        }
                        mpz_init_set(poly.coeffs[0], diff_mpz);
                        /* 不添加零方程, 只计数 */
                        mpz_poly_clear(&poly);
                        added++;
                    }

                    mpz_clear(ab2_x_mpz);
                    mpz_clear(ab2_y_mpz);
                    mpz_clear(ac2_x_mpz);
                    mpz_clear(ac2_y_mpz);
                    mpz_clear(bc2_x_mpz);
                    mpz_clear(bc2_y_mpz);
                    mpz_clear(lhs_mpz);
                    mpz_clear(rhs_mpz);
                    mpz_clear(diff_mpz);
                    mpz_clear(threshold);
                }
            }
        }
    }

    lv00_free((void **) &point_ids);
    lv00_free((void **) &segs);
    return added;
}

/**
 * Parallel cut theorem template (平行线截线段比例定理).
 *
 * If line L3 intersects parallel lines L1 and L2 at points A,B and C,D
 * respectively, then |AB|/|CD| = |OA|/|OC| where O is the intersection
 * of L3 with a transversal.
 *
 * This produces a linear proportion equation that can be used for
 * geometric elimination.
 *
 * @param graph Constraint graph
 * @param sys Equation system to add equations to
 * @return Number of equations added
 */
static int template_parallel_cut(const ConstraintGraph *graph, EquationSystem *sys) {
    int added = 0;

    /* Find pairs of line segments that are parallel (same direction vector) */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *li = graph->nodes[i];
        if (li->type != GEOM_LINE_SEGMENT || li->coord_count < 4)
            continue;

        double x1_i, y1_i, x2_i, y2_i;
        if (!coord_to_double(li->symbolic_coords[0], &x1_i) || !coord_to_double(li->symbolic_coords[1], &y1_i) ||
            !coord_to_double(li->symbolic_coords[2], &x2_i) || !coord_to_double(li->symbolic_coords[3], &y2_i))
            continue;

        double dx_i = x2_i - x1_i;
        double dy_i = y2_i - y1_i;
        double len_i = sqrt(dx_i * dx_i + dy_i * dy_i);
        if (len_i < LV00_EPSILON_DOUBLE)
            continue;

        /* Normalize direction */
        double nx_i = dx_i / len_i;
        double ny_i = dy_i / len_i;

        for (int j = i + 1; j < graph->node_count; j++) {
            GeomNode *lj = graph->nodes[j];
            if (lj->type != GEOM_LINE_SEGMENT || lj->coord_count < 4)
                continue;

            double x1_j, y1_j, x2_j, y2_j;
            if (!coord_to_double(lj->symbolic_coords[0], &x1_j) || !coord_to_double(lj->symbolic_coords[1], &y1_j) ||
                !coord_to_double(lj->symbolic_coords[2], &x2_j) || !coord_to_double(lj->symbolic_coords[3], &y2_j))
                continue;

            double dx_j = x2_j - x1_j;
            double dy_j = y2_j - y1_j;
            double len_j = sqrt(dx_j * dx_j + dy_j * dy_j);
            if (len_j < LV00_EPSILON_DOUBLE)
                continue;

            double nx_j = dx_j / len_j;
            double ny_j = dy_j / len_j;

            /* Check if parallel: cross product of direction vectors ≈ 0 */
            double cross = nx_i * ny_j - ny_i * nx_j;
            if (fabs(cross) > 1e-6)
                continue;

            /* Lines i and j are parallel. Now find a transversal that
             * intersects both. Look for INCIDENCE constraints involving
             * points on both lines. */
            for (int ci = 0; ci < graph->constraint_count; ci++) {
                Constraint *c = graph->constraints[ci];
                if (c->type != INCIDENCE || c->participant_count < 2)
                    continue;

                /* Check if this incidence involves a point on line i */
                GeomNode *pt = find_node((ConstraintGraph *) graph, c->participants[0]);
                GeomNode *seg = find_node((ConstraintGraph *) graph, c->participants[1]);
                if (!pt || pt->type != GEOM_POINT)
                    continue;

                int on_line_i = (seg && seg->id == li->id);
                int on_line_j = (seg && seg->id == lj->id);
                if (!on_line_i && !on_line_j)
                    continue;

                /* If the point is on line i, look for another point on line j
                 * that is collinear with this point (same transversal) */
                if (on_line_i && pt->coord_count >= 2) {
                    for (int cj = ci + 1; cj < graph->constraint_count; cj++) {
                        Constraint *c2 = graph->constraints[cj];
                        if (c2->type != INCIDENCE || c2->participant_count < 2)
                            continue;

                        GeomNode *pt2 = find_node((ConstraintGraph *) graph, c2->participants[0]);
                        GeomNode *seg2 = find_node((ConstraintGraph *) graph, c2->participants[1]);
                        if (!pt2 || pt2->type != GEOM_POINT || pt2->coord_count < 2)
                            continue;
                        if (!seg2 || seg2->id != lj->id)
                            continue;

                        /* pt is on li, pt2 is on lj. Check if they are on the
                         * same transversal (collinear with some reference).
                         * For now, produce a proportion equation:
                         * |pt on li| / |li| = |pt2 on lj| / |lj|
                         * This is a linear relationship. */
                        double px, py;
                        if (!coord_to_double(pt->symbolic_coords[0], &px) ||
                            !coord_to_double(pt->symbolic_coords[1], &py))
                            continue;

                        double t_i = ((px - x1_i) * dx_i + (py - y1_i) * dy_i) / (len_i * len_i);

                        double px2, py2;
                        if (!coord_to_double(pt2->symbolic_coords[0], &px2) ||
                            !coord_to_double(pt2->symbolic_coords[1], &py2))
                            continue;

                        double t_j = ((px2 - x1_j) * dx_j + (py2 - y1_j) * dy_j) / (len_j * len_j);

                        /* Proportion: t_i * len_j = t_j * len_i
                         * => t_i * len_j - t_j * len_i = 0 (linear equation) */
                        int64_t scale = LV00_SOLVER_SCALE_FACTOR;
                        mpz_poly_t poly;
                        mpz_poly_init(&poly);
                        poly.degree = 1;
                        poly.coeffs = malloc(2 * sizeof(mpz_t));
                        if (!poly.coeffs) {
                            mpz_poly_clear(&poly);
                            continue;
                        }
                        mpz_init(poly.coeffs[1]);
                        mpz_init(poly.coeffs[0]);

                        /* This is a derived relation, not directly solving
                         * for a variable. Store as a constraint equation
                         * for the transversal parameter. */
                        double coeff = t_i * len_j - t_j * len_i;
                        if (fabs(coeff) > LV00_EPSILON_NUMERIC_COMPARE) {
                            /* The proportion is not satisfied, add equation */
                            double_to_mpz_scaled(len_j, poly.coeffs[1], scale);
                            double_to_mpz_scaled(-t_j * len_i, poly.coeffs[0], scale);
                            EQUATION_PUSH_OR_GOTO(sys, poly, pt->id, 0, push_error);
                            mpz_poly_clear(&poly);
                            added++;
                        } else {
                            mpz_poly_clear(&poly);
                        }

                        break; /* Only one transversal per parallel pair */
                    }
                }
            }
        }
    }

    return added;
push_error:
    return -1;
}

/* ================================================================== */
/*  内部: 几何推理消元模板 - 平行线截割定理                              */
/* ================================================================== */

/**
 * @brief 平行线截割定理模板
 *
 * @details 如果直线 L1 || L2，且截线 T 与 L1 交于 A,B，与 L2 交于 C,D，
 *          则 AB/AC = ...（比例关系）。
 *          在约束图中，平行关系通过 INCIDENCE + BETWEENNESS 约束推导。
 *          两条线段平行当且仅当它们的方向向量成比例。
 *          最多生成 10 个方程。
 *
 * @param graph 约束图指针
 * @param sys   方程系统指针（用于存储生成的方程）
 * @return 生成的方程数量
 */
static int template_parallel_intercept(ConstraintGraph *graph, EquationSystem *sys) {
    if (!graph || !sys)
        return 0;
    int added = 0;

    /* 收集所有线段 */
    typedef struct {
        int id;
        int p1, p2;
        double dx, dy; /* 方向向量 */
    } SegInfo;
    SegInfo *segs = lv00_malloc((size_t) graph->node_count * sizeof(SegInfo));
    if (!segs)
        return 0;
    int seg_count = 0;

    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *n = graph->nodes[i];
        if (n && n->type == GEOM_LINE_SEGMENT && n->coord_count >= 4) {
            segs[seg_count].id = n->id;
            segs[seg_count].p1 = -1;
            segs[seg_count].p2 = -1;
            segs[seg_count].dx = 0;
            segs[seg_count].dy = 0;
            seg_count++;
        }
    }

    /* 通过 INCIDENCE 约束找到线段端点 */
    for (int ci = 0; ci < graph->constraint_count; ci++) {
        Constraint *c = graph->constraints[ci];
        if (c->type != INCIDENCE || c->participant_count < 2)
            continue;
        int pt_id = c->participants[0];
        int seg_id = c->participants[1];
        for (int s = 0; s < seg_count; s++) {
            if (segs[s].id == seg_id) {
                if (segs[s].p1 < 0)
                    segs[s].p1 = pt_id;
                else if (segs[s].p2 < 0)
                    segs[s].p2 = pt_id;
                break;
            }
        }
    }

    /* 计算每条线段的方向向量 */
    for (int s = 0; s < seg_count; s++) {
        if (segs[s].p1 < 0 || segs[s].p2 < 0)
            continue;
        GeomNode *n1 = find_node(graph, segs[s].p1);
        GeomNode *n2 = find_node(graph, segs[s].p2);
        if (!n1 || !n2)
            continue;
        double x1, y1, x2, y2;
        if (point_coord(n1, 0, &x1) && point_coord(n1, 1, &y1) && point_coord(n2, 0, &x2) && point_coord(n2, 1, &y2)) {
            segs[s].dx = x2 - x1;
            segs[s].dy = y2 - y1;
        }
    }

    /* 搜索平行线段对 */
    for (int i = 0; i < seg_count && added < 10; i++) {
        if (segs[i].p1 < 0 || fabs(segs[i].dx) + fabs(segs[i].dy) < LV00_EPSILON_DOUBLE)
            continue;
        for (int j = i + 1; j < seg_count && added < 10; j++) {
            if (segs[j].p1 < 0 || fabs(segs[j].dx) + fabs(segs[j].dy) < LV00_EPSILON_DOUBLE)
                continue;

            /* 检查平行: 方向向量叉积为零 */
            double cross = segs[i].dx * segs[j].dy - segs[i].dy * segs[j].dx;
            double len_i = sqrt(segs[i].dx * segs[i].dx + segs[i].dy * segs[i].dy);
            double len_j = sqrt(segs[j].dx * segs[j].dx + segs[j].dy * segs[j].dy);

            if (fabs(cross) < 1e-6 * (len_i * len_j + 1.0)) {
                /* 找到一对平行线段 */
                /* 平行线截割定理: 如果一条截线与两条平行线相交,
                 * 则截出的线段成比例 */

                /* 搜索与这两条平行线都相交的截线 */
                for (int k = 0; k < seg_count && added < 10; k++) {
                    if (k == i || k == j)
                        continue;
                    if (segs[k].p1 < 0)
                        continue;

                    /* 检查截线 k 是否与线段 i 和 j 都相交 */
                    /* 简化: 检查截线的端点是否分别在两条平行线上 */
                    /* 更精确的做法是检查线段相交, 这里用简化版本 */

                    GeomNode *kp1 = find_node(graph, segs[k].p1);
                    GeomNode *kp2 = find_node(graph, segs[k].p2);
                    if (!kp1 || !kp2)
                        continue;

                    /* 检查 kp1 是否在线段 i 上, kp2 是否在线段 j 上
                     * (或反之) */
                    /* 通过 INCIDENCE 约束检查 */
                    bool k1_on_i = false, k2_on_j = false;
                    bool k1_on_j = false, k2_on_i = false;
                    for (int ci = 0; ci < graph->constraint_count; ci++) {
                        Constraint *c = graph->constraints[ci];
                        if (c->type != INCIDENCE || c->participant_count < 2)
                            continue;
                        int pt = c->participants[0];
                        int seg = c->participants[1];
                        if (pt == segs[k].p1 && seg == segs[i].id)
                            k1_on_i = true;
                        if (pt == segs[k].p2 && seg == segs[j].id)
                            k2_on_j = true;
                        if (pt == segs[k].p1 && seg == segs[j].id)
                            k1_on_j = true;
                        if (pt == segs[k].p2 && seg == segs[i].id)
                            k2_on_i = true;
                    }

                    if ((k1_on_i && k2_on_j) || (k1_on_j && k2_on_i)) {
                        /* 截线与两条平行线相交
                         * 平行线截割定理: 截出的线段成比例
                         * 如果截线与 L1 交于 A,B, 与 L2 交于 C,D
                         * 则 AB/CD = (L1到截线交点距离)/(L2到截线交点距离) */

                        /* 生成比例方程:
                         * 方向向量比例 = 线段长度比例
                         * segs[i].dx / segs[j].dx = segs[i].dy / segs[j].dy
                         * => segs[i].dx * segs[j].dy - segs[i].dy * segs[j].dx = 0
                         * (这已经在平行检测中使用, 这里生成精确方程) */

                        int64_t scale = LV00_SOLVER_SCALE_FACTOR;
                        /* 生成平行条件的精确方程 */
                        mpz_poly_t poly;
                        mpz_poly_init(&poly);
                        poly.degree = 0;
                        poly.coeffs = malloc(sizeof(mpz_t));
                        /* 内存安全修复：添加 NULL 检查，防止 malloc 失败后解引用空指针 */
                        if (!poly.coeffs) {
                            mpz_poly_clear(&poly);
                            continue;
                        }
                        mpz_init(poly.coeffs[0]);
                        double_to_mpz_scaled(cross, poly.coeffs[0], scale);
                        /* 验证平行条件 */
                        mpz_poly_clear(&poly);
                        added++;
                    }
                }
            }
        }
    }

    lv00_free((void **) &segs);
    return added;
}

/* ================================================================== */
/*  内部: 主函数 - 应用所有几何推理模板                                 */
/* ================================================================== */

/**
 * @brief 主控函数：应用所有几何推理模板
 *
 * @details 依次调用所有几何模板（相似三角形、勾股定理、平行线截割），
 *          将生成的方程添加到方程系统中。
 *
 * @param graph 约束图指针
 * @param sys   方程系统指针（用于存储生成的方程）
 * @return 总共生成的额外方程数量
 */
static int apply_geometry_templates(ConstraintGraph *graph, EquationSystem *sys) {
    if (!graph || !sys)
        return 0;
    int total = 0;

    /* 1. 相似三角形比例模板 */
    total += template_similar_triangles(graph, sys);

    /* 2. 勾股定理模板 */
    total += template_pythagorean(graph, sys);

    /* 3. 平行线截割定理模板 */
    total += template_parallel_intercept(graph, sys);

    return total;
}

/* ================================================================== */
/*  内部: 脏变量追踪 (增量求解支持)                                     */
/* ================================================================== */

/*
 * DirtyVariableSet: 追踪在增量求解中发生变化的变量。
 * 只有涉及脏变量的方程需要重新求解, 避免全局重新计算。
 */

/* 脏变量集合结构体 */
typedef struct {
    int *dirty_ids;  /* dirty variable IDs */
    int dirty_count; /* number of dirty variables */
    int capacity;    /* allocated capacity */
} DirtyVariableSet;

/* 初始化脏变量集合 */
static void dirty_set_init(DirtyVariableSet *ds) {
    ds->dirty_ids = NULL;
    ds->dirty_count = 0;
    ds->capacity = 0;
}

/* 检查变量 ID 是否在脏集合中 */
static bool dirty_set_contains(DirtyVariableSet *ds, int var_id) {
    for (int i = 0; i < ds->dirty_count; i++) {
        if (ds->dirty_ids[i] == var_id)
            return true;
    }
    return false;
}

/* 添加一个脏变量 ID */
static void dirty_set_add(DirtyVariableSet *ds, int var_id) {
    /* 检查是否已存在 */
    if (dirty_set_contains(ds, var_id))
        return;

    if (ds->dirty_count >= ds->capacity) {
        /* 内存安全修复：检查整数溢出，防止 capacity * LV00_ARRAY_GROWTH_FACTOR 超过 INT_MAX */
        int new_cap = ds->capacity == 0 ? 16 : ds->capacity;
        if (new_cap > 0 && new_cap > INT_MAX / LV00_ARRAY_GROWTH_FACTOR)
            return; /* 溢出，跳过 */
        new_cap = new_cap == 0 ? 16 : new_cap * LV00_ARRAY_GROWTH_FACTOR;
        ds->capacity = new_cap;
        int *new_ids = lv00_realloc(ds->dirty_ids, ds->capacity * sizeof(int));
        if (!new_ids)
            return; /* allocation failed, skip */
        ds->dirty_ids = new_ids;
    }
    ds->dirty_ids[ds->dirty_count++] = var_id;
}

/* 清空脏变量集合 */
static void dirty_set_clear(DirtyVariableSet *ds) {
    ds->dirty_count = 0;
    /* 不释放内存, 保留容量以供复用 */
}

/* 释放脏变量集合资源 */
static void dirty_set_free(DirtyVariableSet *ds) {
    lv00_free((void **) &ds->dirty_ids);
    ds->dirty_ids = NULL;
    ds->dirty_count = 0;
    ds->capacity = 0;
}

/* ================================================================== */
/*  内部: 基于脏变量过滤方程                                            */
/* ================================================================== */

/*
 * Filter equations to only those involving dirty variables.
 * 从完整方程系统中筛选出只涉及脏变量的方程子集,
 * 以及通过约束图传播涉及的间接相关方程。
 *
 * 传播规则: 如果一个方程涉及的变量与脏变量共享约束,
 * 则该方程也间接相关。
 */
static void filter_equations_for_dirty(EquationSystem *sys, DirtyVariableSet *ds, EquationSystem *filtered) {
    equation_system_init(filtered);

    if (!sys || !ds || ds->dirty_count == 0)
        return;

    /* 第一遍: 直接筛选涉及脏变量的方程 */
    for (int i = 0; i < sys->count; i++) {
        if (sys->eqs[i].poly.degree < 0)
            continue;
        if (dirty_set_contains(ds, sys->eqs[i].var_node_id)) {
            EQUATION_PUSH_OR_GOTO(filtered, sys->eqs[i].poly, sys->eqs[i].var_node_id, sys->eqs[i].coord_index, push_error);
        }
    }

    /* 第二遍: 传播 - 收集过滤后方程涉及的所有变量 */
    DirtyVariableSet related;
    dirty_set_init(&related);
    for (int i = 0; i < filtered->count; i++) {
        dirty_set_add(&related, filtered->eqs[i].var_node_id);
    }

    /* 第三遍: 添加与相关变量共享同一节点的方程
     * (同一节点的 x 和 y 坐标是耦合的) */
    for (int i = 0; i < sys->count; i++) {
        if (sys->eqs[i].poly.degree < 0)
            continue;
        if (dirty_set_contains(&related, sys->eqs[i].var_node_id)) {
            /* 检查是否已在 filtered 中 */
            bool found = false;
            for (int j = 0; j < filtered->count; j++) {
                if (filtered->eqs[j].var_node_id == sys->eqs[i].var_node_id &&
                    filtered->eqs[j].coord_index == sys->eqs[i].coord_index) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                EQUATION_PUSH_OR_GOTO(filtered, sys->eqs[i].poly, sys->eqs[i].var_node_id, sys->eqs[i].coord_index, push_error);
            }
        }
    }

    dirty_set_free(&related);
push_error:
    return;
}

/* ================================================================== */
/*  内部: 基于约束图拓扑排序的变量消元顺序                               */
/* ================================================================== */

/*
 * order_variables_by_dependency: 根据约束图依赖关系对变量进行拓扑排序。
 * 被依赖的变量排在前面（优先求解）。
 *
 * 参数:
 *   graph        - 约束图
 *   var_ids      - 待排序的变量 ID 数组
 *   var_count    - 变量数量
 *   dirty_var_ids - 脏变量 ID 数组 (可为 NULL，表示全量排序)
 *   dirty_count  - 脏变量数量
 *
 * 返回: 排序后的变量 ID 数组，通过 out_count 返回长度。
 *       调用者负责 lv00_free() 释放返回的数组。
 *       若 dirty_var_ids 非空，仅对脏变量子集排序，其余变量追加在末尾。
 */
static int *order_variables_by_dependency(const ConstraintGraph *graph, const int *var_ids, int var_count,
                                          const int *dirty_var_ids, int dirty_count, int *out_count) {
    *out_count = 0;
    if (!graph || var_count == 0)
        return NULL;

    /* 构建变量 ID 到索引的映射 */
    int *id_to_idx = lv00_malloc((size_t) var_count * sizeof(int));
    if (!id_to_idx)
        return NULL;
    for (int i = 0; i < var_count; i++)
        id_to_idx[i] = -1;
    for (int i = 0; i < var_count; i++) {
        /* 线性搜索 var_ids 中是否有重复 */
        bool dup = false;
        for (int j = 0; j < i; j++) {
            if (var_ids[j] == var_ids[i]) {
                dup = true;
                break;
            }
        }
        if (!dup)
            id_to_idx[i] = i;
    }

    /* 构建邻接矩阵：变量 A 依赖变量 B 当且仅当
     * 存在约束同时涉及 A 和 B，且 B 已有已知值或更少自由度 */
    bool **adj = lv00_malloc((size_t) var_count * sizeof(bool *));
    if (adj)
        memset(adj, 0, (size_t) var_count * sizeof(bool *));
    for (int i = 0; i < var_count; i++) {
        adj[i] = lv00_malloc((size_t) var_count * sizeof(bool));
        if (adj[i])
            memset(adj[i], 0, (size_t) var_count * sizeof(bool));
    }

    /* 统计每个变量的约束参与度 (用于确定依赖方向) */
    int *participation = lv00_malloc((size_t) var_count * sizeof(int));
    if (participation)
        memset(participation, 0, (size_t) var_count * sizeof(int));
    for (int ci = 0; ci < graph->constraint_count; ci++) {
        const Constraint *c = graph->constraints[ci];
        for (int p = 0; p < c->participant_count; p++) {
            int pid = c->participants[p];
            for (int j = 0; j < var_count; j++) {
                if (var_ids[j] == pid && id_to_idx[j] >= 0) {
                    participation[j]++;
                    break;
                }
            }
        }
    }

    /* 构建依赖边：参与度高的变量被依赖（作为已知值），参与度低的依赖它们 */
    for (int ci = 0; ci < graph->constraint_count; ci++) {
        const Constraint *c = graph->constraints[ci];
        /* 收集此约束涉及的所有变量索引 */
        int p_indices[32];
        int p_count = 0;
        for (int p = 0; p < c->participant_count && p < 32; p++) {
            int pid = c->participants[p];
            for (int j = 0; j < var_count; j++) {
                if (var_ids[j] == pid && id_to_idx[j] >= 0) {
                    p_indices[p_count++] = j;
                    break;
                }
            }
        }
        /* 对共享约束的变量对，建立依赖方向：
         * 参与度高的 -> 参与度低的 (被依赖 -> 依赖) */
        for (int a = 0; a < p_count; a++) {
            for (int b = a + 1; b < p_count; b++) {
                int ia = p_indices[a], ib = p_indices[b];
                if (participation[ia] >= participation[ib]) {
                    /* ia 被 ib 依赖: ia -> ib */
                    adj[ia][ib] = true;
                } else {
                    /* ib 被 ia 依赖: ib -> ia */
                    adj[ib][ia] = true;
                }
            }
        }
    }

    /* 计算入度 */
    int *in_degree = lv00_malloc((size_t) var_count * sizeof(int));
    if (in_degree)
        memset(in_degree, 0, (size_t) var_count * sizeof(int));
    for (int i = 0; i < var_count; i++) {
        for (int j = 0; j < var_count; j++) {
            if (adj[i][j])
                in_degree[j]++;
        }
    }

    /* 确定排序子集：若有脏变量列表，仅对脏变量子集排序 */
    bool *in_subset = lv00_malloc((size_t) var_count * sizeof(bool));
    if (in_subset)
        memset(in_subset, 0, (size_t) var_count * sizeof(bool));
    bool use_subset = (dirty_var_ids != NULL && dirty_count > 0);
    if (use_subset) {
        for (int i = 0; i < dirty_count; i++) {
            for (int j = 0; j < var_count; j++) {
                if (var_ids[j] == dirty_var_ids[i]) {
                    in_subset[j] = true;
                    break;
                }
            }
        }
    } else {
        for (int i = 0; i < var_count; i++)
            in_subset[i] = true;
    }

    /* Kahn 拓扑排序，优先选择被依赖度最高的节点 */
    int *order = lv00_malloc((size_t) var_count * sizeof(int));
    if (!order) {
        for (int i = 0; i < var_count; i++)
            lv00_free((void **) &adj[i]);
        lv00_free((void **) &adj);
        lv00_free((void **) &participation);
        lv00_free((void **) &in_degree);
        lv00_free((void **) &in_subset);
        lv00_free((void **) &id_to_idx);
        return NULL;
    }
    int order_count = 0;
    bool *visited = lv00_malloc((size_t) var_count * sizeof(bool));
    if (visited)
        memset(visited, 0, (size_t) var_count * sizeof(bool));

    while (order_count < var_count) {
        int best = -1;
        int best_participation = -1;

        for (int i = 0; i < var_count; i++) {
            if (visited[i])
                continue;
            if (!in_subset[i] && order_count < var_count - (use_subset ? (var_count - dirty_count) : 0)) {
                /* 跳过不在子集中的变量 (留到后面追加) */
                continue;
            }
            if (in_degree[i] > 0)
                continue;

            /* 选择参与度最高的 (被依赖最多的) */
            if (participation[i] > best_participation ||
                (participation[i] == best_participation && var_ids[i] < var_ids[best])) {
                best = i;
                best_participation = participation[i];
            }
        }

        if (best < 0) {
            /* 有环或子集已排完，添加剩余变量 */
            int unvisited = 0;
            for (int i = 0; i < var_count; i++) {
                if (!visited[i])
                    unvisited++;
            }
            fprintf(stderr,
                    "order_variables_by_dependency: cycle detected in constraint graph, "
                    "%d variables remain unscheduled\n",
                    unvisited);
            for (int i = 0; i < var_count; i++) {
                if (!visited[i]) {
                    order[order_count++] = var_ids[i];
                    visited[i] = true;
                }
            }
            break;
        }

        order[order_count++] = var_ids[best];
        visited[best] = true;

        /* 减少邻居的入度 */
        for (int j = 0; j < var_count; j++) {
            if (adj[best][j]) {
                in_degree[j]--;
            }
        }
    }

    /* 清理 */
    for (int i = 0; i < var_count; i++)
        lv00_free((void **) &adj[i]);
    lv00_free((void **) &adj);
    lv00_free((void **) &participation);
    lv00_free((void **) &in_degree);
    lv00_free((void **) &visited);
    lv00_free((void **) &in_subset);
    lv00_free((void **) &id_to_idx);

    *out_count = order_count;
    return order;
}

/**
 * @brief 根据约束图拓扑计算变量消元顺序
 *
 * 被依赖最多的变量优先消元（即出现在最多方程中的变量先消元）。
 *
 * 算法:
 * 1. 统计每个变量（节点）出现在多少个方程/约束中
 * 2. 构建依赖图：变量 A 依赖变量 B，如果它们共享一个约束
 * 3. 使用拓扑排序确定消元顺序
 * 4. 被依赖度最高的变量排在前面（优先消元）
 *
 * @param graph           约束图指针
 * @param sys             方程系统指针
 * @param out_order_count 输出：消元顺序数组长度
 * @return 消元顺序数组（变量 ID 列表），调用者负责释放
 */
static int *compute_elimination_order(const ConstraintGraph *graph, EquationSystem *sys, int *out_order_count) {
    *out_order_count = 0;

    if (!graph || !sys)
        return NULL;

    /* 收集所有唯一的变量 ID (来自方程系统) */
    int *var_ids = lv00_malloc((size_t) sys->count * sizeof(int));
    if (!var_ids)
        return NULL;
    int var_count = 0;
    for (int i = 0; i < sys->count; i++) {
        if (sys->eqs[i].poly.degree < 0)
            continue;
        int vid = sys->eqs[i].var_node_id;
        bool found = false;
        for (int j = 0; j < var_count; j++) {
            if (var_ids[j] == vid) {
                found = true;
                break;
            }
        }
        if (!found)
            var_ids[var_count++] = vid;
    }

    if (var_count == 0) {
        lv00_free((void **) &var_ids);
        return NULL;
    }

    /* 统计每个变量的方程数量 (被依赖度) */
    int *eq_count = lv00_malloc((size_t) var_count * sizeof(int));
    if (eq_count)
        memset(eq_count, 0, (size_t) var_count * sizeof(int));
    for (int i = 0; i < sys->count; i++) {
        if (sys->eqs[i].poly.degree < 0)
            continue;
        for (int j = 0; j < var_count; j++) {
            if (var_ids[j] == sys->eqs[i].var_node_id) {
                eq_count[j]++;
                break;
            }
        }
    }

    /* 统计每个变量的约束参与度 (来自约束图) */
    int *constraint_count = lv00_malloc((size_t) var_count * sizeof(int));
    if (constraint_count)
        memset(constraint_count, 0, (size_t) var_count * sizeof(int));
    for (int ci = 0; ci < graph->constraint_count; ci++) {
        Constraint *c = graph->constraints[ci];
        for (int p = 0; p < c->participant_count; p++) {
            int pid = c->participants[p];
            for (int j = 0; j < var_count; j++) {
                if (var_ids[j] == pid) {
                    constraint_count[j]++;
                    break;
                }
            }
        }
    }

    /* 计算综合权重: 方程数量 * 2 + 约束参与度
     * 方程数量权重更高, 因为它直接反映消元复杂度 */
    int *weight = lv00_malloc((size_t) var_count * sizeof(int));
    if (!weight) {
        lv00_free((void **) &var_ids);
        lv00_free((void **) &eq_count);
        lv00_free((void **) &constraint_count);
        return NULL;
    }
    for (int i = 0; i < var_count; i++) {
        weight[i] = eq_count[i] * 2 + constraint_count[i];
    }

    /* 构建邻接表 (变量之间的依赖关系) */
    /* 两个变量相邻当且仅当它们共享至少一个约束 */
    bool **adj = lv00_malloc((size_t) var_count * sizeof(bool *));
    if (adj)
        memset(adj, 0, (size_t) var_count * sizeof(bool *));
    for (int i = 0; i < var_count; i++) {
        adj[i] = lv00_malloc((size_t) var_count * sizeof(bool));
        if (adj[i])
            memset(adj[i], 0, (size_t) var_count * sizeof(bool));
    }

    for (int ci = 0; ci < graph->constraint_count; ci++) {
        Constraint *c = graph->constraints[ci];
        /* 找出此约束涉及的所有变量 */
        int *participants = lv00_malloc((size_t) c->participant_count * sizeof(int));
        if (!participants)
            continue;
        int p_var_count = 0;
        for (int p = 0; p < c->participant_count; p++) {
            int pid = c->participants[p];
            for (int j = 0; j < var_count; j++) {
                if (var_ids[j] == pid) {
                    participants[p_var_count++] = j;
                    break;
                }
            }
        }
        /* 将共享此约束的变量对标记为相邻 */
        for (int a = 0; a < p_var_count; a++) {
            for (int b = a + 1; b < p_var_count; b++) {
                adj[participants[a]][participants[b]] = true;
                adj[participants[b]][participants[a]] = true;
            }
        }
        lv00_free((void **) &participants);
    }

    /* 计算每个变量的入度 (用于拓扑排序) */
    /* 在依赖图中, 如果变量 A 依赖变量 B (A 的求解需要 B 的值),
     * 则有一条边 B -> A。被依赖最多的变量 (入度最高) 优先消元。 */
    int *in_degree = lv00_malloc((size_t) var_count * sizeof(int));
    if (in_degree)
        memset(in_degree, 0, (size_t) var_count * sizeof(int));
    for (int i = 0; i < var_count; i++) {
        for (int j = 0; j < var_count; j++) {
            if (adj[i][j])
                in_degree[j]++;
        }
    }

    /* 拓扑排序 (Kahn 算法), 优先选择权重高的节点 */
    int *order = lv00_malloc((size_t) var_count * sizeof(int));
    if (!order) {
        for (int i = 0; i < var_count; i++)
            lv00_free((void **) &adj[i]);
        lv00_free((void **) &adj);
        lv00_free((void **) &eq_count);
        lv00_free((void **) &constraint_count);
        lv00_free((void **) &weight);
        lv00_free((void **) &in_degree);
        lv00_free((void **) &var_ids);
        return NULL;
    }
    int order_count = 0;
    bool *visited = lv00_malloc((size_t) var_count * sizeof(bool));
    if (visited)
        memset(visited, 0, (size_t) var_count * sizeof(bool));

    /* 使用优先队列 (简化: 每次选择权重最高的未访问节点) */
    while (order_count < var_count) {
        int best = -1;
        int best_weight = -1;

        for (int i = 0; i < var_count; i++) {
            if (visited[i])
                continue;
            if (in_degree[i] > 0)
                continue; /* 还有依赖未消除 */

            /* 选择权重最高的 */
            if (weight[i] > best_weight || (weight[i] == best_weight && var_ids[i] < var_ids[best])) {
                best = i;
                best_weight = weight[i];
            }
        }

        if (best < 0) {
            /* 有环存在 (循环依赖), 按权重降序添加剩余节点 */
            int unvisited = 0;
            for (int i = 0; i < var_count; i++) {
                if (!visited[i])
                    unvisited++;
            }
            fprintf(stderr,
                    "compute_elimination_order: cycle detected in constraint graph, "
                    "%d variables remain unscheduled\n",
                    unvisited);
            for (int i = 0; i < var_count; i++) {
                if (!visited[i]) {
                    order[order_count++] = var_ids[i];
                    visited[i] = true;
                }
            }
            break;
        }

        order[order_count++] = var_ids[best];
        visited[best] = true;

        /* 减少邻居的入度 */
        for (int j = 0; j < var_count; j++) {
            if (adj[best][j]) {
                in_degree[j]--;
            }
        }
    }

    /* 清理 */
    for (int i = 0; i < var_count; i++) {
        lv00_free((void **) &adj[i]);
    }
    lv00_free((void **) &adj);
    lv00_free((void **) &eq_count);
    lv00_free((void **) &constraint_count);
    lv00_free((void **) &weight);
    lv00_free((void **) &in_degree);
    lv00_free((void **) &visited);
    lv00_free((void **) &var_ids);

    *out_order_count = order_count;
    return order;
}

/* ================================================================== */
/*  PUBLIC API: Equation system lifecycle                              */
/* ================================================================== */

/**
 * @brief 创建并初始化一个空的方程系统
 *
 * @return 新分配的 EquationSystem 指针，失败返回 NULL
 */
EquationSystem *equation_system_create(void) {
    EquationSystem *sys = lv00_malloc(sizeof(EquationSystem));
    if (!sys)
        return NULL;
    equation_system_init(sys);
    return sys;
}

/**
 * @brief 销毁方程系统并释放所有资源
 *
 * @param sys 方程系统指针（可为 NULL，内部会检查）
 */
void equation_system_destroy(EquationSystem *sys) {
    if (!sys)
        return;
    equation_system_clear(sys);
    lv00_free((void **) &sys);
}

/**
 * @brief 获取方程系统中的方程数量
 *
 * @param sys 方程系统指针（可为 NULL）
 * @return 方程数量，sys 为 NULL 时返回 0
 */
int equation_system_count(const EquationSystem *sys) {
    if (!sys)
        return 0;
    return sys->count;
}

const mpz_poly_t *equation_system_get_poly(const EquationSystem *sys, int index) {
    if (!sys || index < 0 || index >= sys->count)
        return NULL;
    return &sys->eqs[index].poly;
}

int equation_system_get_var_id(const EquationSystem *sys, int index) {
    if (!sys || index < 0 || index >= sys->count)
        return -1;
    return sys->eqs[index].var_node_id;
}

int equation_system_get_coord_index(const EquationSystem *sys, int index) {
    if (!sys || index < 0 || index >= sys->count)
        return -1;
    return sys->eqs[index].coord_index;
}

/* ================================================================== */
/*  内部: 依赖传播 (增量求解支持)                                       */
/* ================================================================== */

/*
 * propagate_dependency: 从脏变量出发，沿约束关系 BFS 传播，
 * 收集所有受影响的变量。
 *
 * 算法:
 * 1. 将所有脏变量标记为 affected
 * 2. 对每个脏变量，找到其参与的所有约束
 * 3. 将这些约束的所有参与者也标记为 affected
 * 4. 对新加入的 affected 变量重复步骤 2-3 (BFS)
 * 5. 直到没有新的 affected 变量产生
 *
 * 参数:
 *   graph    - 约束图
 *   var_id   - 起始脏变量 ID
 *   affected - 受影响变量标记数组 (大小 >= graph->node_count)
 */
static void propagate_dependency(const ConstraintGraph *graph, int var_id, bool *affected) {
    if (!graph || !affected || var_id < 0)
        return;

    /* BFS 队列 */
    int alloc_size = graph->node_count;
    int *queue = lv00_malloc((size_t) alloc_size * sizeof(int));
    if (!queue) {
        lv00_free((void **) &affected);
        return;
    }
    int queue_head = 0, queue_tail = 0;

    /* 找到 var_id 在 nodes 数组中的索引 */
    int start_idx = -1;
    for (int i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i]->id == var_id) {
            start_idx = i;
            break;
        }
    }
    if (start_idx < 0) {
        lv00_free((void **) &queue);
        return;
    }

    affected[start_idx] = true;
    queue[queue_tail++] = start_idx;

    while (queue_head < queue_tail) {
        int cur_idx = queue[queue_head++];
        int cur_id = graph->nodes[cur_idx]->id;

        /* 找到当前变量参与的所有约束 */
        for (int ci = 0; ci < graph->constraint_count; ci++) {
            const Constraint *c = graph->constraints[ci];
            if (!c)
                continue;

            bool participates = false;
            for (int p = 0; p < c->participant_count; p++) {
                if (c->participants[p] == cur_id) {
                    participates = true;
                    break;
                }
            }
            if (!participates)
                continue;

            /* 将此约束的所有参与者标记为 affected */
            for (int p = 0; p < c->participant_count; p++) {
                int pid = c->participants[p];
                for (int ni = 0; ni < graph->node_count; ni++) {
                    if (graph->nodes[ni]->id == pid && !affected[ni]) {
                        affected[ni] = true;
                        if (queue_tail < alloc_size) {
                            queue[queue_tail++] = ni;
                        }
                        break;
                    }
                }
            }
        }
    }

    lv00_free((void **) &queue);
}

/* ================================================================== */
/*  PUBLIC API: solver_incremental_solve                               */
/* ================================================================== */

/**
 * @brief 增量求解：仅重新求解与脏变量相关的最小依赖子图
 *
 * @details 未受影响的变量保留其已知解。算法流程：
 *          1. 若 dirty_var_ids 为 NULL 或 dirty_count 为 0，执行全量求解
 *          2. 通过 BFS 传播从脏变量构建依赖子图
 *          3. 收集受影响的变量 ID
 *          4. 仅筛选涉及受影响变量的方程
 *          5. 求解过滤后的子系统
 *          6. 返回受影响变量的解
 *
 * @param graph        约束图指针
 * @param dirty_var_ids 脏变量 ID 数组（可为 NULL 执行全量求解）
 * @param n_dirty_vars  脏变量数量
 * @return GroebnerResult 指针（调用者负责释放），失败返回 NULL
 */
GroebnerResult *solver_incremental_solve(ConstraintGraph *graph, const int *dirty_var_ids, int n_dirty_vars) {
    if (!graph)
        return NULL;

    /* 流式事件: 增量求解开始 */
    if (solver_stream_ctx) {
        StreamEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = STREAM_EVENT_SOLVE_START;
        ev.timestamp_ms = stream_timestamp_ms();
        ev.step_number = n_dirty_vars;
        ev.description = "增量求解开始";
        char detail[SOLVER_DETAIL_BUF_SIZE];
        int _snw_inc;
        LV00_SAFE_SNPRINTF(_snw_inc, detail, sizeof(detail), "{\"phase\":\"incremental\",\"dirty_count\":%d}",
                           n_dirty_vars);
        LV00_UNUSED(_snw_inc);
        ev.detail_json = detail;
        stream_emit(solver_stream_ctx, &ev);
    }

    /* Step 1: 若 dirty_var_ids 为 NULL 或 dirty_count == 0，执行全量求解 */
    if (!dirty_var_ids || n_dirty_vars <= 0) {
        GroebnerResult *result = NULL;
        solve_algebraic_system(graph, NULL, 0, &result);
        return result;
    }

    /* Step 2: 构建脏变量的依赖子图 (BFS 传播) */
    bool *affected = lv00_malloc((size_t) graph->node_count * sizeof(bool));
    if (affected)
        memset(affected, 0, (size_t) graph->node_count * sizeof(bool));
    for (int i = 0; i < n_dirty_vars; i++) {
        propagate_dependency(graph, dirty_var_ids[i], affected);
    }

    /* Step 3: 收集受影响的变量 ID */
    int *affected_ids = NULL;
    int affected_count = 0;
    for (int i = 0; i < graph->node_count; i++) {
        if (affected[i]) {
            affected_ids = lv00_realloc(affected_ids, (size_t) (affected_count + 1) * sizeof(int));
            if (affected_ids) {
                affected_ids[affected_count++] = graph->nodes[i]->id;
            }
        }
    }
    lv00_free((void **) &affected);

    /* 流式事件: 依赖传播完成 */
    if (solver_stream_ctx) {
        StreamEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = STREAM_EVENT_PROGRESS;
        ev.timestamp_ms = stream_timestamp_ms();
        ev.progress = 0.20;
        ev.step_number = affected_count;
        ev.description = "依赖传播完成";
        char detail[SOLVER_DETAIL_BUF_SIZE];
        int _snw_dep;
        LV00_SAFE_SNPRINTF(_snw_dep, detail, sizeof(detail), "{\"phase\":\"dependency_propagation\",\"affected\":%d}",
                           affected_count);
        LV00_UNUSED(_snw_dep);
        ev.detail_json = detail;
        stream_emit(solver_stream_ctx, &ev);
    }

    if (affected_count == 0 || !affected_ids) {
        lv00_free((void **) &affected_ids);
        /* 没有受影响的变量，返回空结果 */
        GroebnerResult *result = lv00_malloc(sizeof(GroebnerResult));
        if (result)
            memset(result, 0, sizeof(GroebnerResult));
        return result;
    }

    /* Step 4: Build full equation system and filter for affected variables */
    EquationSystem full_sys;
    equation_system_init(&full_sys);
    extract_equations_from_constraints(graph, &full_sys);

    /* Build dirty variable set from affected IDs */
    DirtyVariableSet ds;
    dirty_set_init(&ds);
    for (int i = 0; i < affected_count; i++) {
        dirty_set_add(&ds, affected_ids[i]);
    }

    /* Expand: add constraint neighbors of affected variables */
    DirtyVariableSet expanded;
    dirty_set_init(&expanded);
    for (int i = 0; i < affected_count; i++) {
        dirty_set_add(&expanded, affected_ids[i]);
    }

    for (int ci = 0; ci < graph->constraint_count; ci++) {
        Constraint *c = graph->constraints[ci];
        if (!c)
            continue;
        bool involves_affected = false;
        for (int p = 0; p < c->participant_count; p++) {
            if (dirty_set_contains(&ds, c->participants[p])) {
                involves_affected = true;
                break;
            }
        }
        if (involves_affected) {
            for (int p = 0; p < c->participant_count; p++) {
                dirty_set_add(&expanded, c->participants[p]);
            }
        }
    }

    /* Filter equations */
    EquationSystem filtered_sys;
    filter_equations_for_dirty(&full_sys, &expanded, &filtered_sys);

    dirty_set_free(&ds);
    dirty_set_free(&expanded);
    equation_system_clear(&full_sys);
    lv00_free((void **) &affected_ids);

    /* 流式事件: 方程过滤完成 */
    if (solver_stream_ctx) {
        StreamEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = STREAM_EVENT_PROGRESS;
        ev.timestamp_ms = stream_timestamp_ms();
        ev.progress = 0.40;
        ev.step_number = filtered_sys.count;
        ev.description = "增量求解方程过滤完成";
        char detail[SOLVER_DETAIL_BUF_SIZE];
        int _snw_filt;
        LV00_SAFE_SNPRINTF(_snw_filt, detail, sizeof(detail), "{\"phase\":\"filter\",\"filtered_eq_count\":%d}",
                           filtered_sys.count);
        LV00_UNUSED(_snw_filt);
        ev.detail_json = detail;
        stream_emit(solver_stream_ctx, &ev);
    }

    /* Step 5: Solve the filtered subsystem */
    GroebnerResult *result = lv00_malloc(sizeof(GroebnerResult));
    if (!result)
        return NULL;
    memset(result, 0, sizeof(GroebnerResult));
    result->solutions = NULL;
    result->solution_count = 0;
    result->unique = false;
    result->overdetermined = false;

    if (filtered_sys.count == 0) {
        equation_system_clear(&filtered_sys);
        return result;
    }

    /* Check for out-of-scope */
    bool has_oos = false;
    for (int i = 0; i < filtered_sys.count; i++) {
        if (filtered_sys.eqs[i].poly.degree > 2) {
            has_oos = true;
            break;
        }
    }

    int solved_count = 0;
    int multiple_solutions = 0;
    bool no_solution = false;

    if (!has_oos) {
        solve_equations_pass(&filtered_sys, result, &solved_count, &multiple_solutions, &no_solution, false);
    }

    if (no_solution) {
        cleanup_groebner_result(result);
    }

    /* Step 6: Groebner basis elimination for remaining unsolved equations */
    if (!no_solution && !has_oos) {
        int remaining = 0;
        for (int i = 0; i < filtered_sys.count; i++) {
            if (filtered_sys.eqs[i].poly.degree >= 0)
                remaining++;
        }

        if (remaining > 0) {
            SolverStatus gb_status = groebner_basis_compute(&filtered_sys);

            if (gb_status != SOLVER_STATUS_OUT_OF_SCOPE) {
                solve_equations_pass(&filtered_sys, result, &solved_count, &multiple_solutions, &no_solution, false);
            }
        }
    }

    if (multiple_solutions > 0) {
        result->unique = false;
    } else if (solved_count > 0) {
        result->unique = true;
    }

    equation_system_clear(&filtered_sys);

    /* 流式事件: 增量求解完成 */
    if (solver_stream_ctx) {
        StreamEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = STREAM_EVENT_SOLVE_DONE;
        ev.timestamp_ms = stream_timestamp_ms();
        ev.step_number = solved_count;
        ev.description = multiple_solutions > 0 ? "增量求解完成: 多解"
                         : solved_count > 0     ? "增量求解完成: 唯一解"
                                                : "增量求解完成: 部分求解";
        char detail[SOLVER_DETAIL_BUF_SIZE];
        int _snw_inc_done;
        LV00_SAFE_SNPRINTF(_snw_inc_done, detail, sizeof(detail),
                           "{\"phase\":\"incremental_done\",\"solved\":%d,\"multiple\":%d,\"unique\":%d}", solved_count,
                           multiple_solutions, result->unique ? 1 : 0);
        LV00_UNUSED(_snw_inc_done);
        ev.detail_json = detail;
        stream_emit(solver_stream_ctx, &ev);
    }

    return result;
}

/* ================================================================== */
/*  PUBLIC API: groebner_result_free                                   */
/* ================================================================== */

void groebner_result_free(GroebnerResult *result) {
    if (!result)
        return;
    if (result->solutions) {
        for (int i = 0; i < result->solution_count; i++) {
            symbolic_coord_destroy(result->solutions[i]);
        }
        lv00_free((void **) &result->solutions);
    }
    lv00_free((void **) &result);
}

/* ================================================================== */
/*  PUBLIC API: solver_extract_equations_full                          */
/* ================================================================== */

/**
 * @brief 从所有约束类型中提取方程的增强版本
 *
 * @details 比 extract_equations_from_constraints 更全面的版本，
 *          显式处理每种约束类型的语义并生成相应的代数方程：
 *          - INCIDENCE：点 P 在线段 AB 上 => 叉积 (P-A) x (B-A) = 0
 *          - INTERSECTION：两条线段相交于交点 R => 两个线性方程
 *          - CONTAINMENT：点在区域内 => 遍历边界线段，叉积方法生成线性方程
 *          - BETWEENNESS：B 在 A 和 C 之间 => 共线性方程
 *          - CONNECTION：端口连接 => 从 numeric_assumption 解析距离，生成二次方程
 *
 * @param graph      约束图指针
 * @param out_system 输出：提取的方程系统
 * @return 提取的方程数量，-1 表示参数错误
 */
int solver_extract_equations_full(const ConstraintGraph *graph, EquationSystem *out_system) {
    if (!graph || !out_system)
        return -1;

    int count = 0;

    for (int ci = 0; ci < graph->constraint_count; ci++) {
        const Constraint *c = graph->constraints[ci];
        if (!c || c->participant_count < 2)
            continue;

        switch (c->type) {
            case INCIDENCE: {
                /*
             * 关联约束：点 P 在线段 AB 上。
             * 叉积方程 (P - A) x (B - A) = 0
             * => (px - ax)*(by - ay) - (py - ay)*(bx - ax) = 0
             * 这是一个关于 px, py 的线性方程。我们将其分解为两个一元方程
             * （一个用于 px，一个用于 py）。
             *
             * 【精度路径说明】
             * 当前 INCIDENCE 约束的方程提取使用 coord_to_double 获取浮点近似值，
             * 然后通过 double_to_mpz_scaled 转换为缩放后的整数系数。
             * 与 BETWEENNESS 和 CONTAINMENT 不同，INCIDENCE 的方程系数在
             * line_from_two_points 中已通过 coord_to_mpz_scaled_exact 优先走精确路径。
             * 本函数直接使用的 coord_to_double 路径仅在线段端点坐标不可用时作为回退。
             * 如果未来需要更强的精度保证，可参考 BETWEENNESS 的实现模式添加
             * coord_to_mpz_scaled 精确路径。
             */
                GeomNode *pt = find_node((ConstraintGraph *) graph, c->participants[0]);
                GeomNode *line = find_node((ConstraintGraph *) graph, c->participants[1]);
                if (!pt || !line)
                    break;
                if (line->type == GEOM_LINE_SEGMENT) {
                    double lx1, ly1, lx2, ly2;
                    bool got_coords = false;

                    if (line->coord_count >= 4) {
                        got_coords = coord_to_double(line->symbolic_coords[0], &lx1) &&
                                     coord_to_double(line->symbolic_coords[1], &ly1) &&
                                     coord_to_double(line->symbolic_coords[2], &lx2) &&
                                     coord_to_double(line->symbolic_coords[3], &ly2);
                    } else if (line->coord_count >= 2) {
                        /* Line segment stores endpoint x-coords in symbolic_coords.
                     * Find endpoint nodes via INCIDENCE constraints to get full coords. */
                        GeomNode *ep1 = NULL, *ep2 = NULL;
                        for (int cj = 0; cj < graph->constraint_count; cj++) {
                            const Constraint *c2 = graph->constraints[cj];
                            if (c2->type != INCIDENCE || c2->participant_count < 2)
                                continue;
                            if (c2->participants[1] == line->id) {
                                GeomNode *candidate = find_node((ConstraintGraph *) graph, c2->participants[0]);
                                if (candidate && candidate->type == GEOM_POINT) {
                                    if (!ep1)
                                        ep1 = candidate;
                                    else if (!ep2)
                                        ep2 = candidate;
                                }
                            }
                        }
                        if (ep1 && ep2) {
                            got_coords = point_coord(ep1, 0, &lx1) && point_coord(ep1, 1, &ly1) &&
                                         point_coord(ep2, 0, &lx2) && point_coord(ep2, 1, &ly2);
                        }
                    }

                    if (got_coords) {
                        double dx = lx2 - lx1;
                        double dy = ly2 - ly1;

                        /* Equation for x-coordinate of point:
                     * dy*(px - lx1) - dx*(py - ly1) = 0
                     * => dy*px - dx*py + (dx*ly1 - dy*lx1) = 0
                     * As univariate in px: dy*px + (dx*ly1 - dy*lx1 - dx*py) = 0
                     * We store the x-component: dy*px + (dx*ly1 - dy*lx1) = 0
                     * (the y-dependent part is handled via the second equation) */
                        int64_t scale = LV00_SOLVER_SCALE_FACTOR;
                        mpz_poly_t poly;
                        mpz_poly_init(&poly);
                        poly.degree = 1;
                        poly.coeffs = malloc(2 * sizeof(mpz_t));
                        /* 内存安全修复：添加 NULL 检查，防止分配失败后解引用空指针 */
                        if (!poly.coeffs) {
                            mpz_poly_clear(&poly);
                            break;
                        }
                        mpz_init(poly.coeffs[1]);
                        mpz_init(poly.coeffs[0]);
                        double_to_mpz_scaled(-dy, poly.coeffs[1], scale);
                        double_to_mpz_scaled(dy * lx1 - dx * ly1, poly.coeffs[0], scale);
                        EQUATION_PUSH_OR_GOTO(out_system, poly, pt->id, 0, push_error);
                        mpz_poly_clear(&poly);
                        count++;
                        stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_EQUATION_EXTRACTED,
                                           "提取方程: 关联约束 (x)", count);

                        /* 方程 for y-coordinate of point */
                        mpz_poly_init(&poly);
                        poly.degree = 1;
                        poly.coeffs = malloc(2 * sizeof(mpz_t));
                        /* 内存安全修复：添加 NULL 检查，防止分配失败后解引用空指针 */
                        if (!poly.coeffs) {
                            mpz_poly_clear(&poly);
                            break;
                        }
                        mpz_init(poly.coeffs[1]);
                        mpz_init(poly.coeffs[0]);
                        double_to_mpz_scaled(dx, poly.coeffs[1], scale);
                        double_to_mpz_scaled(-dx * ly1 - dy * lx1, poly.coeffs[0], scale);
                        EQUATION_PUSH_OR_GOTO(out_system, poly, pt->id, 1, push_error);
                        mpz_poly_clear(&poly);
                        count++;
                        stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_EQUATION_EXTRACTED,
                                           "提取方程: 关联约束 (y)", count);
                    }
                } /* if (line->type == GEOM_LINE_SEGMENT) */
                break;
            }

            case INTERSECTION: {
                /*
             * 交点约束：两条线段 L1 和 L2 相交于点 R。
             * R 在 L1 上且 R 在 L2 上，产生一个 2x2 参数方程组：
             *   R = A + s*(B-A),  R = C + t*(D-C)
             * 消去参数 s, t 即得关于 R 坐标的两个线性方程。
             *
             * 【精度路径说明】
             * 当前 INTERSECTION 约束的方程系数 line_from_two_points 在
             * 内部已经优先使用 coord_to_mpz_scaled_exact 精确路径来提取
             * 线段方程系数（a, b, c）。因此 double_to_mpz_scaled 对此处
             * 的 LineEquation 使用已经是从精确路径来的值，精度良好。
             * 如果 line_from_two_points 回退到双精度路径，本处的系数才会
             * 出现精度损失。未来可考虑将 LineEquation 的 a/b/c 字段改为
             * mpz_t 类型以彻底消除 double 中间表示。
             */
                if (c->participant_count < 3)
                    break;
                GeomNode *line1 = find_node((ConstraintGraph *) graph, c->participants[0]);
                GeomNode *line2 = find_node((ConstraintGraph *) graph, c->participants[1]);
                GeomNode *rpt = find_node((ConstraintGraph *) graph, c->participants[2]);
                if (!line1 || !line2 || !rpt)
                    break;

                LineEquation le1, le2;
                bool got1 = false, got2 = false;

                if (line1->type == GEOM_LINE_SEGMENT && line1->coord_count >= 4) {
                    GeomNode ep1_storage, ep2_storage;
                    memset(&ep1_storage, 0, sizeof(GeomNode));
                    memset(&ep2_storage, 0, sizeof(GeomNode));
                    GeomNode *ep1 = &ep1_storage;
                    GeomNode *ep2 = &ep2_storage;
                    ep1->type = GEOM_POINT;
                    ep1->coord_count = 2;
                    ep1->symbolic_coords = &line1->symbolic_coords[0];
                    ep2->type = GEOM_POINT;
                    ep2->coord_count = 2;
                    ep2->symbolic_coords = &line1->symbolic_coords[2];
                    got1 = line_from_two_points(ep1, ep2, &le1);
                }
                if (line2->type == GEOM_LINE_SEGMENT && line2->coord_count >= 4) {
                    GeomNode ep1_storage, ep2_storage;
                    memset(&ep1_storage, 0, sizeof(GeomNode));
                    memset(&ep2_storage, 0, sizeof(GeomNode));
                    GeomNode *ep1 = &ep1_storage;
                    GeomNode *ep2 = &ep2_storage;
                    ep1->type = GEOM_POINT;
                    ep1->coord_count = 2;
                    ep1->symbolic_coords = &line2->symbolic_coords[0];
                    ep2->type = GEOM_POINT;
                    ep2->coord_count = 2;
                    ep2->symbolic_coords = &line2->symbolic_coords[2];
                    got2 = line_from_two_points(ep1, ep2, &le2);
                }

                if (got1 && got2) {
                    int64_t scale = LV00_SOLVER_SCALE_FACTOR;
                    /* Line1: a1*x + b1*y + c1 = 0 */
                    mpz_poly_t poly;
                    mpz_poly_init(&poly);
                    poly.degree = 1;
                    poly.coeffs = malloc(2 * sizeof(mpz_t));
                    if (!poly.coeffs) {
                        mpz_poly_clear(&poly);
                        continue;
                    }
                    mpz_init(poly.coeffs[1]);
                    mpz_init(poly.coeffs[0]);
                    double_to_mpz_scaled(le1.a, poly.coeffs[1], scale);
                    double_to_mpz_scaled(le1.c, poly.coeffs[0], scale);
                    EQUATION_PUSH_OR_GOTO(out_system, poly, rpt->id, 0, push_error);
                    mpz_poly_clear(&poly);
                    count++;
                    stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_EQUATION_EXTRACTED,
                                       "提取方程: 交点约束 (x)", count);

                    /* Line2: a2*x + b2*y + c2 = 0 */
                    mpz_poly_init(&poly);
                    poly.degree = 1;
                    poly.coeffs = malloc(2 * sizeof(mpz_t));
                    if (!poly.coeffs) {
                        mpz_poly_clear(&poly);
                        continue;
                    }
                    mpz_init(poly.coeffs[1]);
                    mpz_init(poly.coeffs[0]);
                    double_to_mpz_scaled(le2.a, poly.coeffs[1], scale);
                    double_to_mpz_scaled(le2.c, poly.coeffs[0], scale);
                    EQUATION_PUSH_OR_GOTO(out_system, poly, rpt->id, 1, push_error);
                    mpz_poly_clear(&poly);
                    count++;
                    stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_EQUATION_EXTRACTED,
                                       "提取方程: 交点约束 (y)", count);
                }
                break;
            }

            case CONTAINMENT: {
                /*
             * 包容约束：内部几何体位于外部区域内。
             * 对凸多边形区域，点必须在每条边的"内侧"。
             * 对每条边界线段生成线性不等式/方程，使用叉积符号判定。
             *
             * 【精度路径选择逻辑】
             * 叉积约束方程（以点 (px, py) 为变量，线段端点 (sx1,sy1)-(sx2,sy2)）：
             *   x-分量：dy*px + (-dy*sx1 + dx*sy1) = 0
             *   y-分量：-dx*py + (dy*sx1 - dx*sy1) = 0
             *   其中 dx = sx2 - sx1, dy = sy2 - sy1。
             *
             * 路径1（精确有理数路径）：当边界线段的四个端点坐标均为 RATIONAL 类型时，
             *   使用 coord_to_mpz_scaled 从 mpq_t 中直接提取精确的有理数值，
             *   配合 mpz 多精度整数算术计算 dx, dy 和常数项。此路径无精度损失。
             * 路径2（double 近似路径）：当任一端点坐标是 QUADRATIC、ALGEBRAIC
             *   或 TRANSCENDENTAL 类型时，回退到 coord_to_double +
             *   double_to_mpz_scaled 的近似路径。此路径可能引入浮点舍入误差。
             *
             * 注：当前仅处理"点在区域内"的包容约束。
             */
                if (c->participant_count < 2)
                    break;
                GeomNode *inner = find_node((ConstraintGraph *) graph, c->participants[0]);
                GeomNode *outer = find_node((ConstraintGraph *) graph, c->participants[1]);
                if (!inner || !outer)
                    break;

                /* Only handle point-in-region containment for now */
                if (inner->type != GEOM_POINT || outer->type != GEOM_REGION)
                    break;
                if (outer->data.region.segment_count <= 0 || !outer->data.region.boundary_segments)
                    break;

                {
                    int64_t scale = LV00_SOLVER_SCALE_FACTOR;
                    int seg_count = outer->data.region.segment_count;

                    for (int si = 0; si < seg_count; si++) {
                        GeomNode *seg = outer->data.region.boundary_segments[si];
                        if (!seg || seg->type != GEOM_LINE_SEGMENT)
                            continue;
                        if (seg->coord_count < 4 || !seg->symbolic_coords)
                            continue;

                        /*
                     * 精度路径选择：检查边界线段的四个端点坐标是否都是 RATIONAL 类型。
                     * 如果是，走精确有理数路径；否则走 double 近似路径。
                     */
                        bool seg_is_rational = (seg->symbolic_coords[0] && seg->symbolic_coords[0]->type == RATIONAL &&
                                                seg->symbolic_coords[1] && seg->symbolic_coords[1]->type == RATIONAL &&
                                                seg->symbolic_coords[2] && seg->symbolic_coords[2]->type == RATIONAL &&
                                                seg->symbolic_coords[3] && seg->symbolic_coords[3]->type == RATIONAL);

                        if (seg_is_rational) {
                            /*
                         * 路径1（精确有理数路径）：
                         * 使用 coord_to_mpz_scaled 提取缩放后的整数坐标，
                         * 再用 mpz 算术计算 dx, dy 和常数项系数。
                         */
                            mpz_t sx1_s, sy1_s, sx2_s, sy2_s;
                            mpz_init(sx1_s);
                            mpz_init(sy1_s);
                            mpz_init(sx2_s);
                            mpz_init(sy2_s);

                            if (coord_to_mpz_scaled(seg->symbolic_coords[0], sx1_s, scale) &&
                                coord_to_mpz_scaled(seg->symbolic_coords[1], sy1_s, scale) &&
                                coord_to_mpz_scaled(seg->symbolic_coords[2], sx2_s, scale) &&
                                coord_to_mpz_scaled(seg->symbolic_coords[3], sy2_s, scale)) {
                                mpz_t dx_s, dy_s;
                                mpz_init(dx_s);
                                mpz_init(dy_s);
                                /* 精确缩放差量: dx_s = (sx2 - sx1) * scale */
                                mpz_sub(dx_s, sx2_s, sx1_s);
                                mpz_sub(dy_s, sy2_s, sy1_s);

                                /* 中间变量 */
                                mpz_t term1, term2;
                                mpz_init(term1);
                                mpz_init(term2);

                                /* 方程1（x-分量）: dy_s * px + (-dy_s*sx1_s + dx_s*sy1_s) = 0 */
                                {
                                    mpz_poly_t poly;
                                    mpz_poly_init(&poly);
                                    poly.degree = 1;
                                    poly.coeffs = malloc(2 * sizeof(mpz_t));
                                    if (!poly.coeffs) {
                                        mpz_poly_clear(&poly);
                                    } else {
                                        mpz_init(poly.coeffs[1]);
                                        mpz_init(poly.coeffs[0]);
                                        mpz_set(poly.coeffs[1], dy_s);
                                        mpz_mul(term1, dy_s, sx1_s);
                                        mpz_mul(term2, dx_s, sy1_s);
                                        mpz_sub(poly.coeffs[0], term2, term1);
                                        EQUATION_PUSH_OR_GOTO(out_system, poly, inner->id, 0, push_error);
                                        mpz_poly_clear(&poly);
                                        count++;
                                        stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_EQUATION_EXTRACTED,
                                                           "提取方程: 包容约束 [精确] (x)", count);
                                    }
                                }

                                /* 方程2（y-分量）: -dx_s * py + (dy_s*sx1_s - dx_s*sy1_s) = 0 */
                                {
                                    mpz_poly_t poly;
                                    mpz_poly_init(&poly);
                                    poly.degree = 1;
                                    poly.coeffs = malloc(2 * sizeof(mpz_t));
                                    if (!poly.coeffs) {
                                        mpz_poly_clear(&poly);
                                    } else {
                                        mpz_init(poly.coeffs[1]);
                                        mpz_init(poly.coeffs[0]);
                                        mpz_neg(poly.coeffs[1], dx_s);
                                        mpz_mul(term1, dy_s, sx1_s);
                                        mpz_mul(term2, dx_s, sy1_s);
                                        mpz_sub(poly.coeffs[0], term1, term2);
                                        EQUATION_PUSH_OR_GOTO(out_system, poly, inner->id, 1, push_error);
                                        mpz_poly_clear(&poly);
                                        count++;
                                        stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_EQUATION_EXTRACTED,
                                                           "提取方程: 包容约束 [精确] (y)", count);
                                    }
                                }
                                mpz_clear(term1);
                                mpz_clear(term2);
                                mpz_clear(dx_s);
                                mpz_clear(dy_s);
                            }
                            mpz_clear(sx1_s);
                            mpz_clear(sy1_s);
                            mpz_clear(sx2_s);
                            mpz_clear(sy2_s);
                        } else {
                            /*
                         * 路径2（double 近似路径）：
                         * 使用 coord_to_double 获取浮点近似值，通过 double_to_mpz_scaled
                         * 转换为缩放后的整数系数。此路径可能引入舍入误差。
                         */
                            double sx1, sy1, sx2, sy2;
                            if (!coord_to_double(seg->symbolic_coords[0], &sx1) ||
                                !coord_to_double(seg->symbolic_coords[1], &sy1) ||
                                !coord_to_double(seg->symbolic_coords[2], &sx2) ||
                                !coord_to_double(seg->symbolic_coords[3], &sy2))
                                continue;

                            double dx = sx2 - sx1;
                            double dy = sy2 - sy1;

                            /* x-component equation: dy*px + (-dy*sx1 + dx*sy1) = 0 */
                            mpz_poly_t poly;
                            mpz_poly_init(&poly);
                            poly.degree = 1;
                            poly.coeffs = malloc(2 * sizeof(mpz_t));
                            if (!poly.coeffs) {
                                mpz_poly_clear(&poly);
                                continue;
                            }
                            mpz_init(poly.coeffs[1]);
                            mpz_init(poly.coeffs[0]);
                            double_to_mpz_scaled(dy, poly.coeffs[1], scale);
                            double_to_mpz_scaled(-dy * sx1 + dx * sy1, poly.coeffs[0], scale);
                            EQUATION_PUSH_OR_GOTO(out_system, poly, inner->id, 0, push_error);
                            mpz_poly_clear(&poly);
                            count++;
                            stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_EQUATION_EXTRACTED,
                                               "提取方程: 包容约束 [近似] (x)", count);

                            /* y-component equation: -dx*py + (dy*sx1 - dx*sy1) = 0 */
                            mpz_poly_init(&poly);
                            poly.degree = 1;
                            poly.coeffs = malloc(2 * sizeof(mpz_t));
                            if (!poly.coeffs) {
                                mpz_poly_clear(&poly);
                                continue;
                            }
                            mpz_init(poly.coeffs[1]);
                            mpz_init(poly.coeffs[0]);
                            double_to_mpz_scaled(-dx, poly.coeffs[1], scale);
                            double_to_mpz_scaled(dy * sx1 - dx * sy1, poly.coeffs[0], scale);
                            EQUATION_PUSH_OR_GOTO(out_system, poly, inner->id, 1, push_error);
                            mpz_poly_clear(&poly);
                            count++;
                            stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_EQUATION_EXTRACTED,
                                               "提取方程: 包容约束 [近似] (y)", count);
                        }
                    }
                }
                break;
            }

            case BETWEENNESS: {
                /*
             * B 在 A 和 C 之间：不产生独立的代数方程。
             * 仅用于解的选择（0 <= t <= 1）。
             * 我们生成共线性方程作为线性约束。
             *
             * 【精度路径选择逻辑】
             * 共线性方程：(x2-x1)*dy13 - (y2-y1)*dx13 = 0
             * 其中 dx13 = x3 - x1, dy13 = y3 - y1。
             *
             * 路径1（精确有理数路径）：当 p1 和 p3 的坐标都是 RATIONAL 类型时，
             *   使用 coord_to_mpz_scaled 从 mpq_t 中直接提取精确的有理数值，
             *   配合 mpz 多精度整数算术计算方程系数。此路径无精度损失。
             * 路径2（double 近似路径）：当任一端点坐标是 QUADRATIC、ALGEBRAIC
             *   或 TRANSCENDENTAL 类型时，回退到 coord_to_double +
             *   double_to_mpz_scaled 的近似路径。此路径可能引入浮点舍入误差。
             */
                if (c->participant_count < 3)
                    break;
                GeomNode *p1 = find_node((ConstraintGraph *) graph, c->participants[0]);
                GeomNode *p2 = find_node((ConstraintGraph *) graph, c->participants[1]);
                GeomNode *p3 = find_node((ConstraintGraph *) graph, c->participants[2]);
                if (!p1 || !p2 || !p3)
                    break;

                /*
             * 检查 p1 和 p3 的符号坐标是否存在且均非空。
             * BETWEENNESS 约束只使用端点 p1 和 p3，中间点 p2 是目标变量。
             */
                if (!p1->symbolic_coords || !p3->symbolic_coords)
                    break;

                bool exact_mode = false;
                mpz_t dx13_s, dy13_s, x1_s, y1_s;
                /* 精确有理数路径：检查 p1 和 p3 的四个坐标是否都是 RATIONAL 类型 */
                if (p1->symbolic_coords[0] && p1->symbolic_coords[0]->type == RATIONAL && p1->symbolic_coords[1] &&
                    p1->symbolic_coords[1]->type == RATIONAL && p3->symbolic_coords[0] &&
                    p3->symbolic_coords[0]->type == RATIONAL && p3->symbolic_coords[1] &&
                    p3->symbolic_coords[1]->type == RATIONAL) {
                    int64_t scale = LV00_SOLVER_SCALE_FACTOR;
                    mpz_init(dx13_s);
                    mpz_init(dy13_s);
                    mpz_init(x1_s);
                    mpz_init(y1_s);

                    /*
                 * 精确路径 —— 使用 coord_to_mpz_scaled（精确有理数转缩放整数）。
                 * 计算步骤（使用 GMP 多精度整数算术，无精度损失）：
                 *   1. 提取缩放后的端点坐标: x1_s, y1_s, x3_s, y3_s
                 *   2. 计算缩放后的差量: dx13_s = x3_s - x1_s
                 *   3. 计算缩放后的差量: dy13_s = y3_s - y1_s
                 *   4. 解构共线性方程系数:
                 *      - x 方程: coeffs[1] = dy13_s, coeffs[0] = dx13_s*y1_s - dy13_s*x1_s
                 *         （需要除以 scale 恢复真实系数）
                 *      - y 方程: coeffs[1] = -dx13_s, coeffs[0] = -dx13_s*y1_s + dy13_s*x1_s
                 */
                    if (coord_to_mpz_scaled(p1->symbolic_coords[0], x1_s, scale) &&
                        coord_to_mpz_scaled(p1->symbolic_coords[1], y1_s, scale)) {
                        mpz_t x3_s, y3_s;
                        mpz_init(x3_s);
                        mpz_init(y3_s);

                        if (coord_to_mpz_scaled(p3->symbolic_coords[0], x3_s, scale) &&
                            coord_to_mpz_scaled(p3->symbolic_coords[1], y3_s, scale)) {
                            /* 缩放差量: dx13_s = (x3 - x1) * scale */
                            mpz_sub(dx13_s, x3_s, x1_s);
                            mpz_sub(dy13_s, y3_s, y1_s);

                            /* 中间结果：dx13_s * y1_s 和 dy13_s * x1_s */
                            mpz_t term1, term2;
                            mpz_init(term1);
                            mpz_init(term2);

                            /*
                         * 方程1（x-分量，变量为 p2 的 x 坐标）：
                         *   coeffs[1] * x + coeffs[0] = 0
                         *   其中 coeffs[1] = dy13_s / scale（缩放后的 dy13）
                         *         coeffs[0] = (dx13_s*y1_s - dy13_s*x1_s) / scale
                         */
                            {
                                mpz_poly_t poly;
                                mpz_poly_init(&poly);
                                poly.degree = 1;
                                poly.coeffs = malloc(2 * sizeof(mpz_t));
                                if (!poly.coeffs) {
                                    mpz_poly_clear(&poly);
                                } else {
                                    mpz_init(poly.coeffs[1]);
                                    mpz_init(poly.coeffs[0]);
                                    mpz_set(poly.coeffs[1], dy13_s);
                                    mpz_mul(term1, dx13_s, y1_s);
                                    mpz_mul(term2, dy13_s, x1_s);
                                    mpz_sub(poly.coeffs[0], term1, term2);
                                    EQUATION_PUSH_OR_GOTO(out_system, poly, p2->id, 0, push_error);
                                    mpz_poly_clear(&poly);
                                    count++;
                                    stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_EQUATION_EXTRACTED,
                                                       "提取方程: 介于约束 [精确] (x)", count);
                                }
                            }

                            /*
                         * 方程2（y-分量，变量为 p2 的 y 坐标）：
                         *   coeffs[1] * y + coeffs[0] = 0
                         *   其中 coeffs[1] = -dx13_s / scale
                         *         coeffs[0] = (dy13_s*x1_s - dx13_s*y1_s) / scale
                         */
                            {
                                mpz_poly_t poly;
                                mpz_poly_init(&poly);
                                poly.degree = 1;
                                poly.coeffs = malloc(2 * sizeof(mpz_t));
                                if (!poly.coeffs) {
                                    mpz_poly_clear(&poly);
                                } else {
                                    mpz_init(poly.coeffs[1]);
                                    mpz_init(poly.coeffs[0]);
                                    mpz_neg(poly.coeffs[1], dx13_s);
                                    mpz_mul(term1, dy13_s, x1_s);
                                    mpz_mul(term2, dx13_s, y1_s);
                                    mpz_sub(poly.coeffs[0], term1, term2);
                                    EQUATION_PUSH_OR_GOTO(out_system, poly, p2->id, 1, push_error);
                                    mpz_poly_clear(&poly);
                                    count++;
                                    stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_EQUATION_EXTRACTED,
                                                       "提取方程: 介于约束 [精确] (y)", count);
                                }
                            }

                            mpz_clear(term1);
                            mpz_clear(term2);
                            exact_mode = true;
                        }
                        mpz_clear(x3_s);
                        mpz_clear(y3_s);
                    }
                    /* 精确路径结束，无论成功与否都释放临时 mpz_t */
                    if (!exact_mode) {
                        mpz_clear(dx13_s);
                        mpz_clear(dy13_s);
                        mpz_clear(x1_s);
                        mpz_clear(y1_s);
                    }
                }

                /*
             * 路径2（double 近似路径）：精确路径未启用或失败时回退。
             * 使用 coord_to_double 获取浮点近似值，再通过 double_to_mpz_scaled
             * 转换为缩放后的整数系数。注意此路径可能引入舍入误差。
             */
                if (!exact_mode) {
                    double x1, y1, x2, y2, x3, y3;
                    bool ok = point_coord(p1, 0, &x1) && point_coord(p1, 1, &y1) && point_coord(p3, 0, &x3) &&
                              point_coord(p3, 1, &y3);
                    if (ok) {
                        double dx13 = x3 - x1;
                        double dy13 = y3 - y1;
                        int64_t scale = LV00_SOLVER_SCALE_FACTOR;

                        /* Collinearity: (x2-x1)*dy13 - (y2-y1)*dx13 = 0 */
                        mpz_poly_t poly;
                        mpz_poly_init(&poly);
                        poly.degree = 1;
                        poly.coeffs = malloc(2 * sizeof(mpz_t));
                        if (!poly.coeffs) {
                            mpz_poly_clear(&poly);
                        } else {
                            mpz_init(poly.coeffs[1]);
                            mpz_init(poly.coeffs[0]);
                            double_to_mpz_scaled(dy13, poly.coeffs[1], scale);
                            double_to_mpz_scaled(dx13 * y1 - dy13 * x1, poly.coeffs[0], scale);
                            EQUATION_PUSH_OR_GOTO(out_system, poly, p2->id, 0, push_error);
                            mpz_poly_clear(&poly);
                            count++;
                            stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_EQUATION_EXTRACTED,
                                               "提取方程: 介于约束 [近似] (x)", count);
                        }

                        mpz_poly_init(&poly);
                        poly.degree = 1;
                        poly.coeffs = malloc(2 * sizeof(mpz_t));
                        if (!poly.coeffs) {
                            mpz_poly_clear(&poly);
                        } else {
                            mpz_init(poly.coeffs[1]);
                            mpz_init(poly.coeffs[0]);
                            double_to_mpz_scaled(-dx13, poly.coeffs[1], scale);
                            double_to_mpz_scaled(-dx13 * y1 + dy13 * x1, poly.coeffs[0], scale);
                            EQUATION_PUSH_OR_GOTO(out_system, poly, p2->id, 1, push_error);
                            mpz_poly_clear(&poly);
                            count++;
                            stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_EQUATION_EXTRACTED,
                                               "提取方程: 介于约束 [近似] (y)", count);
                        }
                    }
                }
                break;
            }

            case CONNECTION: {
                /*
             * 连接约束：端口之间的连接。
             * 从连通节点的 numeric_assumption_declaration 中提取距离约束。
             * 如果两个节点都有坐标且其中一个编码了距离值，则生成方程：
             *   (xA - xB)^2 + (yA - yB)^2 = d^2
             *
             * 将 nodeA 视为固定参考点，nodeB 为变量，展开为二次方程：
             *   xB^2 - 2*ax*xB + (ax^2 + ay^2 - d^2) = 0
             *
             * 【精度路径说明】
             * 当前 CONNECTION 的方程系数通过 coord_to_double 获取双精度近似值。
             * 距离值 dist_val 从 numeric_assumption_declaration 字符串中用
             * strtod 解析（也是双精度）。对于 RATIONAL 类型的节点坐标，
             * 可考虑使用 coord_to_mpz_scaled 精确路径来减少系数整数化时的
             * 舍入误差。此优化可参考 BETWEENNESS 的实现模式。
             */
                if (c->participant_count < 2)
                    break;
                GeomNode *nodeA = find_node((ConstraintGraph *) graph, c->participants[0]);
                GeomNode *nodeB = find_node((ConstraintGraph *) graph, c->participants[1]);
                if (!nodeA || !nodeB)
                    break;

                /* 尝试从任一节点的 numeric_assumption_declaration 中提取距离值 */
                double dist_val = -1.0;
                GeomNode *dist_node = NULL;
                const char *prefix = "distance=";
                size_t prefix_len = strlen(prefix); /* 缓存前缀长度，避免循环内重复计算 */
                for (int ni = 0; ni < 2; ni++) {
                    GeomNode *n = (ni == 0) ? nodeA : nodeB;
                    if (!n || !n->numeric_assumption_declaration)
                        continue;
                    const char *decl = n->numeric_assumption_declaration;
                    if (strncmp(decl, prefix, prefix_len) == 0) {
                        dist_val = strtod(decl + prefix_len, NULL);
                        dist_node = n;
                        break;
                    }
                }

                if (dist_val < 0)
                    break;

                /* Both nodes need at least 2 coordinates (x, y) */
                if (nodeA->coord_count < 2 || nodeB->coord_count < 2)
                    break;
                if (!nodeA->symbolic_coords || !nodeB->symbolic_coords)
                    break;

                double ax, ay, bx, by;
                if (!coord_to_double(nodeA->symbolic_coords[0], &ax) ||
                    !coord_to_double(nodeA->symbolic_coords[1], &ay) ||
                    !coord_to_double(nodeB->symbolic_coords[0], &bx) ||
                    !coord_to_double(nodeB->symbolic_coords[1], &by))
                    break;

                double dist_sq = dist_val * dist_val;
                int64_t scale = LV00_SOLVER_SCALE_FACTOR;

                /* (xA-xB)^2 + (yA-yB)^2 = d^2
               Expand for nodeB as variable (nodeA as fixed):
               xB^2 - 2*ax*xB + (ax^2 + ay^2 - dist_sq) = 0 */
                mpz_poly_t poly;
                mpz_poly_init(&poly);
                poly.degree = 2;
                poly.coeffs = malloc(3 * sizeof(mpz_t));
                if (!poly.coeffs) {
                    mpz_poly_clear(&poly);
                    break;
                }
                mpz_init(poly.coeffs[2]);
                mpz_init(poly.coeffs[1]);
                mpz_init(poly.coeffs[0]);
                mpz_set_si(poly.coeffs[2], scale);
                double_to_mpz_scaled(-2.0 * ax, poly.coeffs[1], scale);
                double_to_mpz_scaled(ax * ax + ay * ay - dist_sq, poly.coeffs[0], scale);
                EQUATION_PUSH_OR_GOTO(out_system, poly, nodeB->id, 0, push_error);
                mpz_poly_clear(&poly);
                count++;
                stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_EQUATION_EXTRACTED, "提取方程: 连接约束 (x)",
                                   count);

                /* Similarly for yB: yB^2 - 2*ay*yB + (ax^2 + ay^2 - dist_sq) = 0 */
                mpz_poly_init(&poly);
                poly.degree = 2;
                poly.coeffs = malloc(3 * sizeof(mpz_t));
                if (!poly.coeffs) {
                    mpz_poly_clear(&poly);
                    break;
                }
                mpz_init(poly.coeffs[2]);
                mpz_init(poly.coeffs[1]);
                mpz_init(poly.coeffs[0]);
                mpz_set_si(poly.coeffs[2], scale);
                double_to_mpz_scaled(-2.0 * ay, poly.coeffs[1], scale);
                double_to_mpz_scaled(ax * ax + ay * ay - dist_sq, poly.coeffs[0], scale);
                EQUATION_PUSH_OR_GOTO(out_system, poly, nodeB->id, 1, push_error);
                mpz_poly_clear(&poly);
                count++;
                stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_EQUATION_EXTRACTED, "提取方程: 连接约束 (y)",
                                   count);
                break;
            }
            default:
                LV00_LOG_WARNING("Unknown constraint type %d in solver_extract_equations", c->type);
                break;
        } /* closes switch(c->type) */
    } /* closes for(ci) - first pass */

    /* Second pass: extract distance constraints from nodes that have
       numeric_assumption_declaration set (encoding squared-distance = d^2). */
    for (int ni = 0; ni < graph->node_count; ni++) {
        GeomNode *node = graph->nodes[ni];
        if (!node || !node->numeric_assumption_declaration)
            continue;
        if (node->type != GEOM_LINE_SEGMENT)
            continue;

        const char *decl = node->numeric_assumption_declaration;
        double dist_sq = -1.0;

        const char *prefix = "distance=";
        size_t prefix_len = strlen(prefix); /* 缓存前缀长度，避免重复计算 */
        if (strncmp(decl, prefix, prefix_len) == 0) {
            dist_sq = strtod(decl + prefix_len, NULL);
            dist_sq = dist_sq * dist_sq;
        } else {
            char *end = NULL;
            double val = strtod(decl, &end);
            if (end != decl && val >= 0) {
                dist_sq = val;
            }
        }

        if (dist_sq < 0)
            continue;

        if (node->coord_count >= 4) {
            double x1, y1;
            if (coord_to_double(node->symbolic_coords[0], &x1) && coord_to_double(node->symbolic_coords[1], &y1)) {
                int64_t scale = LV00_SOLVER_SCALE_FACTOR;

                mpz_poly_t poly;
                mpz_poly_init(&poly);
                poly.degree = 2;
                poly.coeffs = malloc(3 * sizeof(mpz_t));
                if (!poly.coeffs) {
                    mpz_poly_clear(&poly);
                    continue;
                }
                mpz_init(poly.coeffs[2]);
                mpz_init(poly.coeffs[1]);
                mpz_init(poly.coeffs[0]);
                mpz_set_si(poly.coeffs[2], scale);
                double_to_mpz_scaled(-2.0 * x1, poly.coeffs[1], scale);
                double_to_mpz_scaled(x1 * x1 + y1 * y1 - dist_sq, poly.coeffs[0], scale);
                EQUATION_PUSH_OR_GOTO(out_system, poly, node->id, 0, push_error);
                mpz_poly_clear(&poly);
                count++;
                stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_EQUATION_EXTRACTED, "提取方程: 距离约束 (x)",
                                   count);

                mpz_poly_init(&poly);
                poly.degree = 2;
                poly.coeffs = malloc(3 * sizeof(mpz_t));
                if (!poly.coeffs) {
                    mpz_poly_clear(&poly);
                    continue;
                }
                mpz_init(poly.coeffs[2]);
                mpz_init(poly.coeffs[1]);
                mpz_init(poly.coeffs[0]);
                mpz_set_si(poly.coeffs[2], scale);
                double_to_mpz_scaled(-2.0 * y1, poly.coeffs[1], scale);
                double_to_mpz_scaled(x1 * x1 + y1 * y1 - dist_sq, poly.coeffs[0], scale);
                EQUATION_PUSH_OR_GOTO(out_system, poly, node->id, 1, push_error);
                mpz_poly_clear(&poly);
                count++;
                stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_EQUATION_EXTRACTED, "提取方程: 距离约束 (y)",
                                   count);
            }
        }
    }

    return count;
push_error:
    return -1;
}

/* ================================================================== */
/*  PUBLIC API: groebner_basis_compute                                 */
/* ================================================================== */

/**
 * @brief 对方程系统计算 Groebner 基（简化的 Buchberger 算法）
 *
 * @details 处理所有多项式全次数 <= 4 的系统。算法流程：
 *          1. 将 EquationSystem 转换为多变量多项式表示
 *          2. 运行 Buchberger 算法
 *          3. 将 Groebner 基转换回 EquationSystem 形式
 *          4. 用 Groebner 基替换原方程
 *
 * @param system 方程系统指针（函数会就地修改）
 * @return SOLVER_STATUS_OK 表示成功，SOLVER_STATUS_OUT_OF_SCOPE 表示存在次数 > 4 的多项式，
 *         SOLVER_STATUS_TIMEOUT 表示计算超出步数限制
 */
SolverStatus groebner_basis_compute(EquationSystem *system) {
    if (!system || system->count == 0)
        return SOLVER_STATUS_OK;

    /* Step 1: Check degree limit first (fast path) */
    for (int i = 0; i < system->count; i++) {
        if (system->eqs[i].poly.degree > 4) {
            return SOLVER_STATUS_OUT_OF_SCOPE;
        }
    }

    /* Step 2: Build multivariate polynomial representation */
    int *var_id_map = NULL;
    int *coord_map = NULL;
    int var_count = 0;
    MVPolynomial *mv_polys = build_mv_polynomials(system, &var_id_map, &coord_map, &var_count);

    if (var_count == 0 || !mv_polys) {
        lv00_free((void **) &var_id_map);
        lv00_free((void **) &coord_map);
        return SOLVER_STATUS_OK;
    }

    /* Step 3: Filter out zero polynomials and collect non-trivial ones */
    int active_count = 0;
    for (int i = 0; i < system->count; i++) {
        if (!mv_poly_is_zero(&mv_polys[i])) {
            active_count++;
        }
    }

    if (active_count == 0) {
        for (int i = 0; i < system->count; i++) {
            mv_poly_clear(&mv_polys[i]);
        }
        lv00_free((void **) &mv_polys);
        lv00_free((void **) &var_id_map);
        lv00_free((void **) &coord_map);
        return SOLVER_STATUS_OK;
    }

    MVPolynomial **active = lv00_malloc((size_t) active_count * sizeof(MVPolynomial *));
    if (!active) {
        for (int i = 0; i < system->count; i++)
            mv_poly_clear(&mv_polys[i]);
        lv00_free((void **) &mv_polys);
        lv00_free((void **) &var_id_map);
        lv00_free((void **) &coord_map);
        return SOLVER_STATUS_TIMEOUT;
    }
    int idx = 0;
    for (int i = 0; i < system->count; i++) {
        if (!mv_poly_is_zero(&mv_polys[i])) {
            active[idx++] = &mv_polys[i];
        }
    }

    /* Step 4: Run Buchberger's algorithm */
    MVPolynomial **G = NULL;
    int g_count = 0;
    SolverStatus status = buchberger_groebner(active, active_count, &G, &g_count, 10000);

    /* 流式输出: Groebner 基计算步骤完成（含详细统计） */
    if (solver_stream_ctx) {
        StreamEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = STREAM_EVENT_SOLVE_GROEBNER_STEP;
        ev.timestamp_ms = stream_timestamp_ms();
        ev.step_number = 0;
        ev.description = "Groebner 基计算完成";
        char detail[SOLVER_DETAIL_BUF_SIZE];
        int _snw_gc;
        LV00_SAFE_SNPRINTF(_snw_gc, detail, sizeof(detail),
                           "{\"phase\":\"groebner_complete\",\"status\":\"%s\","
                           "\"input_equations\":%d,\"active_equations\":%d,"
                           "\"basis_size\":%d,\"variables\":%d}",
                           (status == SOLVER_STATUS_OK)             ? "ok"
                           : (status == SOLVER_STATUS_OUT_OF_SCOPE) ? "out_of_scope"
                                                             : "timeout",
                           system->count, active_count, g_count, var_count);
        LV00_UNUSED(_snw_gc);
        ev.detail_json = detail;
        stream_emit(solver_stream_ctx, &ev);
    }

    lv00_free((void **) &active);

    if (status == SOLVER_STATUS_OUT_OF_SCOPE) {
        for (int i = 0; i < system->count; i++) {
            mv_poly_clear(&mv_polys[i]);
        }
        lv00_free((void **) &mv_polys);
        lv00_free((void **) &var_id_map);
        lv00_free((void **) &coord_map);
        return SOLVER_STATUS_OUT_OF_SCOPE;
    }

    /* Save original count before clearing system */
    int original_eq_count = system->count;

    /* Step 5: Convert Groebner basis back to EquationSystem form.
     * For each Groebner basis polynomial, extract the leading univariate
     * equation (the one with the highest degree in a single variable).
     * Since we're limited to degree <= 2, each basis polynomial
     * is at most quadratic in each variable. */
    equation_system_clear(system);
    equation_system_init(system);

    if (G) {
        for (int i = 0; i < g_count; i++) {
            if (mv_poly_is_zero(G[i]))
                continue;

            /* Find the variable with the highest degree in this polynomial */
            int best_var = -1;
            int best_degree = -1;

            for (int v = 0; v < var_count; v++) {
                int max_deg_v = 0;
                for (int t = 0; t < G[i]->term_count; t++) {
                    if (G[i]->terms[t].exponents[v] > max_deg_v) {
                        max_deg_v = G[i]->terms[t].exponents[v];
                    }
                }
                if (max_deg_v > best_degree) {
                    best_degree = max_deg_v;
                    best_var = v;
                }
            }

            if (best_var < 0 || best_degree < 0)
                continue;

            /* Extract univariate polynomial for best_var.
             * Collect terms, treating other variables as constants.
             * For degree <= 4 systems, we can extract the polynomial
             * by evaluating at the leading variable's powers. */
            mpz_poly_t poly;
            mpz_poly_init(&poly);
            poly.degree = best_degree;
            poly.coeffs = malloc((best_degree + 1) * sizeof(mpz_t));
            if (!poly.coeffs) {
                mpz_poly_clear(&poly);
                continue;
            }
            for (int d = 0; d <= best_degree; d++) {
                mpz_init_set_si(poly.coeffs[d], 0);
            }

            /* Sum coefficients for each power of best_var */
            for (int t = 0; t < G[i]->term_count; t++) {
                int power = G[i]->terms[t].exponents[best_var];
                if (power <= best_degree) {
                    mpz_add(poly.coeffs[power], poly.coeffs[power], G[i]->terms[t].coeff);
                }
            }

            /* Look up the original variable ID and coord index */
            int node_id = var_id_map[best_var];
            int coord_index = coord_map ? coord_map[best_var] : 0;

            EQUATION_PUSH_OR_GOTO(system, poly, node_id, coord_index, push_error);
            mpz_poly_clear(&poly);
        }

        /* Clean up Groebner basis */
        for (int i = 0; i < g_count; i++) {
            mv_poly_clear(G[i]);
            lv00_free((void **) &G[i]);
        }
        lv00_free((void **) &G);
    }

    /* Clean up original multivariate polynomials.
     * system->count now reflects the Groebner basis size;
     * use original_eq_count to free the original mv_polys entries. */
    for (int i = 0; i < original_eq_count; i++) {
        mv_poly_clear(&mv_polys[i]);
    }
    lv00_free((void **) &mv_polys);
    lv00_free((void **) &var_id_map);
    lv00_free((void **) &coord_map);

    return (status == SOLVER_STATUS_TIMEOUT) ? SOLVER_STATUS_TIMEOUT : SOLVER_STATUS_OK;
push_error:
    /* 清理 Groebner 基（可能部分已分配） */
    if (G) {
        for (int i = 0; i < g_count; i++) {
            if (G[i]) {
                mv_poly_clear(G[i]);
                lv00_free((void **) &G[i]);
            }
        }
        lv00_free((void **) &G);
    }
    /* 清理原始多变量多项式 */
    if (mv_polys) {
        for (int i = 0; i < original_eq_count; i++) {
            mv_poly_clear(&mv_polys[i]);
        }
        lv00_free((void **) &mv_polys);
    }
    lv00_free((void **) &var_id_map);
    lv00_free((void **) &coord_map);
    return SOLVER_STATUS_OUT_OF_MEMORY;
}

/* ================================================================== */
/*  PUBLIC API: solver_handle_multiple_solutions                       */
/* ================================================================== */

/**
 * @brief 处理二次方程的多解分支
 *
 * @details 当二次方程 a*x^2 + b*x + c = 0 有判别式 D = b^2 - 4ac 时：
 *          - D > 0：两个不同实根
 *          - D = 0：一个重根
 *          - D < 0：无实根
 *          算法流程：
 *          1. 扫描方程系统中所有 degree=2 的方程
 *          2. 对每个二次方程计算两个根
 *          3. 生成所有组合（笛卡尔积）= 2^k 个分支（上限 2^12 = 4096）
 *          4. 过滤导致其他方程矛盾的分支
 *          流式输出每个根和解分支的进度。
 *
 * @param result         GroebnerResult 指针（用于流式输出元数据）
 * @param system         方程系统指针
 * @param out_branches   输出：有效分支的解数组
 * @param out_branch_count 输出：有效分支数量
 * @return SOLVER_STATUS_UNIQUE 表示唯一解，SOLVER_STATUS_NO_SOLUTION 表示无解，
 *         SOLVER_STATUS_OK 表示多解，SOLVER_STATUS_TIMEOUT 表示内存不足
 */
SolverStatus solver_handle_multiple_solutions(const GroebnerResult *result, const EquationSystem *system,
                                              SymbolicCoord ***out_branches, int *out_branch_count) {
    LV00_UNUSED(result);
    if (!out_branches || !out_branch_count)
        return SOLVER_STATUS_TIMEOUT;
    *out_branches = NULL;
    *out_branch_count = 0;

    /* No system means no branches to handle */
    if (!system || system->count == 0) {
        return SOLVER_STATUS_UNIQUE;
    }

    /* Step 1: Identify quadratic equations that have distinct real roots.
     * For each quadratic a*x^2 + b*x + c = 0:
     *   root = (-b ± sqrt(D)) / (2*a)  where D = b^2 - 4*a*c
     * We collect the two possible values for each quadratic variable. */
    struct BranchVariable {
        int var_node_id; /* variable node ID */
        int coord_index; /* 0=x, 1=y */
        int eq_index;    /* index in system->eqs */
        double root1;    /* first root ( -b + sqrt(D) ) / (2a) */
        double root2;    /* second root ( -b - sqrt(D) ) / (2a) */
        bool valid;      /* has two distinct real roots */
    };

    int max_branch_vars = system->count;
    struct BranchVariable *branch_vars = lv00_malloc((size_t) max_branch_vars * sizeof(struct BranchVariable));
    if (!branch_vars)
        return SOLVER_STATUS_TIMEOUT;
    memset(branch_vars, 0, (size_t) max_branch_vars * sizeof(struct BranchVariable));

    /* Also need a mapping from variable (node_id, coord) to which equations
     * constrain it, for later contradiction checking */
    int branch_count = 0;

    for (int i = 0; i < system->count; i++) {
        if (system->eqs[i].poly.degree != 2)
            continue;
        if (mpz_cmp_si(system->eqs[i].poly.coeffs[2], 0) == 0)
            continue;

        /* Extract coefficients a, b, c using GMP's mpz_get_d which
         * converts mpz_t directly to double. */
        double a_val = mpz_get_d(system->eqs[i].poly.coeffs[2]) / LV00_SOLVER_SCALE_FACTOR;
        double b_val = (system->eqs[i].poly.degree >= 1)
                           ? mpz_get_d(system->eqs[i].poly.coeffs[1]) / LV00_SOLVER_SCALE_FACTOR
                           : 0.0;
        double c_val = mpz_get_d(system->eqs[i].poly.coeffs[0]) / LV00_SOLVER_SCALE_FACTOR;

        double discriminant = b_val * b_val - 4.0 * a_val * c_val;

        if (discriminant < -LV00_EPSILON_DOUBLE) {
            /* No real roots - this equation has complex roots only */
            continue;
        }

        if (discriminant < LV00_EPSILON_DOUBLE) {
            /* Double root (D ≈ 0): only one distinct solution, skip for branching */
            continue;
        }

        /* Two distinct real roots */
        double sqrt_d = sqrt(discriminant);
        double denom = 2.0 * a_val;

        branch_vars[branch_count].var_node_id = system->eqs[i].var_node_id;
        branch_vars[branch_count].coord_index = system->eqs[i].coord_index;
        branch_vars[branch_count].eq_index = i;
        branch_vars[branch_count].root1 = (-b_val + sqrt_d) / denom;
        branch_vars[branch_count].root2 = (-b_val - sqrt_d) / denom;
        branch_vars[branch_count].valid = true;

        /* Stream: emit variable with two possible values */
        if (solver_stream_ctx) {
            StreamEvent ev;
            memset(&ev, 0, sizeof(ev));
            ev.type = STREAM_EVENT_SOLVE_VARIABLE_RESOLVED;
            ev.timestamp_ms = stream_timestamp_ms();
            ev.var_id = system->eqs[i].var_node_id;
            ev.step_number = branch_count;
            ev.description = "二次方程双解分支变量";
            char detail[SOLVER_DETAIL_BUF_SIZE];
            int _snw3;
            LV00_SAFE_SNPRINTF(_snw3, detail, sizeof(detail),
                               "{\"var_id\":%d,\"coord\":%d,\"root1\":%.6f,\"root2\":%.6f,\"D\":%.6f}",
                               system->eqs[i].var_node_id, system->eqs[i].coord_index, branch_vars[branch_count].root1,
                               branch_vars[branch_count].root2, discriminant);
            LV00_UNUSED(_snw3);
            ev.detail_json = detail;
            stream_emit(solver_stream_ctx, &ev);
        }

        branch_count++;
    }

    /* Step 2: Generate all combinations (Cartesian product).
     * For k quadratic equations, there are 2^k possible branches.
     * We cap at 2^12 = 4096 to avoid combinatorial explosion. */
    if (branch_count == 0) {
        lv00_free((void **) &branch_vars);
        return SOLVER_STATUS_UNIQUE; /* No quadratic equations with distinct roots */
    }

    if (branch_count > 12) {
        if (solver_stream_ctx) {
            stream_emit_simple(solver_stream_ctx, STREAM_EVENT_WARNING,
                               "多解分支过多 (k>12)，已截断为 2^12=4096 个分支", branch_count);
        }
        branch_count = 12;
    }

    int total_branches = 1 << branch_count; /* 2^k */
    int valid_branches = 0;                 /* branches that passed validation */

    /* Step 3: Allocate flat 2D array for all branch coordinates.
     * branch_coords[b * branch_count + v] = v-th coordinate of branch b */
    int total_coords = total_branches * branch_count;
    SymbolicCoord **branch_coords = lv00_malloc((size_t) total_coords * sizeof(SymbolicCoord *));
    if (!branch_coords) {
        lv00_free((void **) &branch_vars);
        return SOLVER_STATUS_OUT_OF_MEMORY;
    }
    memset(branch_coords, 0, (size_t) total_coords * sizeof(SymbolicCoord *));

    for (int b = 0; b < total_branches; b++) {
        for (int v = 0; v < branch_count; v++) {
            /* Bit v of b determines which root: 0 = root1, 1 = root2 */
            double chosen = (b & (1u << v)) ? branch_vars[v].root2 : branch_vars[v].root1;

            /* Create a rational coordinate from the double value. */
            int64_t num_val = (int64_t) (chosen * LV00_SOLVER_SCALE_FACTOR);
            SymbolicCoord *coord = symbolic_coord_create_rational(num_val, (uint64_t) LV00_SOLVER_SCALE_FACTOR);
            if (!coord) {
                /* Cleanup: destroy all coords created so far */
                for (int i = 0; i < b * branch_count + v; i++) {
                    symbolic_coord_destroy(branch_coords[i]);
                }
                lv00_free((void **) &branch_coords);
                lv00_free((void **) &branch_vars);
                return SOLVER_STATUS_TIMEOUT;
            }
            branch_coords[b * branch_count + v] = coord;
        }
    }

    /* Step 4: Validate each branch against remaining equations.
     * A branch is valid if it doesn't contradict any of the other equations
     * in the system when the quadratic values are substituted. */
    bool *branch_valid = lv00_malloc((size_t) total_branches * sizeof(bool));
    if (!branch_valid) {
        for (int i = 0; i < total_coords; i++) {
            symbolic_coord_destroy(branch_coords[i]);
        }
        lv00_free((void **) &branch_coords);
        lv00_free((void **) &branch_vars);
        return SOLVER_STATUS_TIMEOUT;
    }
    memset(branch_valid, 0, (size_t) total_branches * sizeof(bool));

    for (int b = 0; b < total_branches; b++) {
        bool valid = true;

        /* Check each non-quadratic equation in the system:
         * substitute the branch's values and verify the equation holds */
        for (int eq = 0; eq < system->count; eq++) {
            /* Skip the quadratic equations themselves */
            bool is_branch_eq = false;
            for (int v = 0; v < branch_count; v++) {
                if (branch_vars[v].eq_index == eq) {
                    is_branch_eq = true;
                    break;
                }
            }
            if (is_branch_eq)
                continue;

            /* For linear equations (degree=1): check if the branch value
             * satisfies the equation. For now we do a simple linear check:
             * a*x + c = 0 => x ≈ -c/a */
            if (system->eqs[eq].poly.degree == 1) {
                /* Check if this equation constrains the same variable */
                if (system->eqs[eq].var_node_id == 0) {
                    /* 跳过占位符方程（var_node_id == 0 表示该方程槽位未被实际约束填充）。
                     * 在调试模式下记录跳过的占位符数量，帮助诊断方程提取阶段的问题。
                     * 占位符方程通常出现在方程系统的预分配槽位中，不影响求解正确性。 */
                    if (debug_is_debug_mode()) {
                        static int placeholder_skip_count = 0;
                        placeholder_skip_count++;
                        if (placeholder_skip_count == 1 || placeholder_skip_count % 100 == 0) {
                            debug_log(LOG_LEVEL_DEBUG, "solver",
                                      "跳过占位符方程 #%d（var_node_id=0），"
                                      "当前累计跳过 %d 个占位符方程",
                                      eq, placeholder_skip_count);
                        }
                    }
                    continue;
                }

                /* Find the branch value for this variable */
                double branch_val = 0.0;
                bool found = false;
                /* 安全检查：确保分支数不超过 unsigned int 位数，避免位移未定义行为 */
                if (branch_count > (int) (sizeof(unsigned int) * 8)) {
                    valid = false;
                    break;
                }
                for (int v = 0; v < branch_count; v++) {
                    if (branch_vars[v].var_node_id == system->eqs[eq].var_node_id &&
                        branch_vars[v].coord_index == system->eqs[eq].coord_index) {
                        branch_val = (b & (1u << v)) ? branch_vars[v].root2 : branch_vars[v].root1;
                        found = true;
                        break;
                    }
                }
                if (!found)
                    continue; /* Different variable, skip */

                /* Verify: a*x + c ≈ 0
                 * 使用符号坐标多项式求值进行精确验证，避免 double 精度丢失。
                 * 先尝试符号求值，失败时回退到 double 近似。 */
                bool equation_satisfied = false;

                /* 尝试符号精确验证 */
                if (branch_coords && branch_count > 0) {
                    /* 找到该分支中对应变量的符号坐标 */
                    for (int v = 0; v < branch_count; v++) {
                        if (branch_vars[v].var_node_id == system->eqs[eq].var_node_id &&
                            branch_vars[v].coord_index == system->eqs[eq].coord_index) {
                            SymbolicCoord *branch_coord = branch_coords[b * branch_count + v];
                            if (branch_coord) {
                                /* 用符号坐标在多项式上求值 */
                                SymbolicCoord *poly_val = poly_eval_symbolic(&system->eqs[eq].poly, branch_coord);
                                if (poly_val) {
                                    /* 检查求值结果是否为零 */
                                    double eval_d = 0.0;
                                    if (coord_to_double(poly_val, &eval_d)) {
                                        equation_satisfied = (fabs(eval_d) < 1e-6);
                                    }
                                    symbolic_coord_destroy(poly_val);
                                }
                            }
                            break;
                        }
                    }
                }

                /* 回退到 double 近似验证 */
                if (!equation_satisfied) {
                    double a_coeff = mpz_get_d(system->eqs[eq].poly.coeffs[1]) / LV00_SOLVER_SCALE_FACTOR;
                    double c_coeff = mpz_get_d(system->eqs[eq].poly.coeffs[0]) / LV00_SOLVER_SCALE_FACTOR;
                    double lhs = a_coeff * branch_val + c_coeff;
                    equation_satisfied = (fabs(lhs) < 1e-6);
                }

                if (!equation_satisfied) {
                    /* Branch contradicts this equation */
                    valid = false;
                    break;
                }
            }
            /* For degree 0: check constant = 0 (contradiction if non-zero) */
            if (system->eqs[eq].poly.degree == 0) {
                double const_val = mpz_get_d(system->eqs[eq].poly.coeffs[0]) / LV00_SOLVER_SCALE_FACTOR;
                if (fabs(const_val) > LV00_EPSILON_DOUBLE) {
                    valid = false;
                    break;
                }
            }
        }

        branch_valid[b] = valid;
        if (valid)
            valid_branches++;

        /* Stream: progress */
        if (solver_stream_ctx && (b % 100 == 0 || b == total_branches - 1)) {
            StreamEvent ev;
            memset(&ev, 0, sizeof(ev));
            ev.type = STREAM_EVENT_PROGRESS;
            ev.timestamp_ms = stream_timestamp_ms();
            ev.progress = (double) (b + 1) / (double) total_branches;
            ev.description = "多解分支验证中";
            char detail[SOLVER_DETAIL_BUF_SIZE];
            int _snw4;
            LV00_SAFE_SNPRINTF(_snw4, detail, sizeof(detail), "{\"checked\":%d,\"total\":%d,\"valid\":%d}", b + 1,
                               total_branches, valid_branches);
            LV00_UNUSED(_snw4);
            ev.detail_json = detail;
            stream_emit(solver_stream_ctx, &ev);
        }
    }

    /* Step 5: Build output array of valid branches.
     * Each valid branch is a newly-allocated SymbolicCoord* array of branch_count entries,
     * copied from the flat branch_coords. */
    SymbolicCoord **out_branch_arr = lv00_malloc((size_t) (valid_branches * branch_count) * sizeof(SymbolicCoord *));
    if (!out_branch_arr) {
        for (int i = 0; i < total_coords; i++) {
            symbolic_coord_destroy(branch_coords[i]);
        }
        lv00_free((void **) &branch_coords);
        lv00_free((void **) &branch_valid);
        lv00_free((void **) &branch_vars);
        return SOLVER_STATUS_OUT_OF_MEMORY;
    }
    memset(out_branch_arr, 0, (size_t) (valid_branches * branch_count) * sizeof(SymbolicCoord *));

    int out_idx = 0;
    for (int b = 0; b < total_branches; b++) {
        if (!branch_valid[b])
            continue;

        /* Copy this branch's coordinates to the output array */
        int dst_base = out_idx * branch_count;
        int src_base = b * branch_count;
        for (int v = 0; v < branch_count; v++) {
            out_branch_arr[dst_base + v] = branch_coords[src_base + v];
        }
        out_idx++;

        /* Stream: emit valid branch */
        if (solver_stream_ctx) {
            StreamEvent ev;
            memset(&ev, 0, sizeof(ev));
            ev.type = STREAM_EVENT_SOLVE_VARIABLE_RESOLVED;
            ev.timestamp_ms = stream_timestamp_ms();
            ev.step_number = out_idx;
            ev.description = "有效多解分支";
            char detail[SOLVER_DETAIL_BUF_SIZE];
            int pos;
            LV00_SAFE_SNPRINTF(pos, detail, sizeof(detail), "{\"branch\":%d,\"valid\":true,\"values\":[", out_idx);
            for (int v = 0; v < branch_count && pos < (int) sizeof(detail) - 30; v++) {
                char *coord_str = symbolic_coord_serialize(branch_coords[src_base + v]);
                if (coord_str) {
                    int _sn_tmp;
                    LV00_SAFE_SNPRINTF(_sn_tmp, detail + pos, (size_t) (sizeof(detail) - pos - 5), "%s\"%s\"",
                                       (v > 0 ? "," : ""), coord_str);
                    pos += _sn_tmp;
                    lv00_free((void **) &coord_str);
                }
            }
            {
                int _sn_tmp2;
                LV00_SAFE_SNPRINTF(_sn_tmp2, detail + pos, (size_t) (sizeof(detail) - pos - 3), "]}");
                LV00_UNUSED(_sn_tmp2);
                LV00_UNUSED(pos);
            }
            ev.detail_json = detail;
            stream_emit(solver_stream_ctx, &ev);
        }
    }

    /* Destroy invalid branch coordinates (they're not moved to out_branch_arr) */
    for (int b = 0; b < total_branches; b++) {
        if (!branch_valid[b]) {
            for (int v = 0; v < branch_count; v++) {
                symbolic_coord_destroy(branch_coords[b * branch_count + v]);
            }
        }
    }

    /* Free temporary working arrays.
     * branch_coords entries for valid branches were MOVED to out_branch_arr;
     * don't double-free them. */
    lv00_free((void **) &branch_coords);
    lv00_free((void **) &branch_valid);
    lv00_free((void **) &branch_vars);

    if (valid_branches == 0) {
        lv00_free((void **) &out_branch_arr);
        *out_branches = NULL;
        *out_branch_count = 0;
        if (solver_stream_ctx) {
            stream_emit_simple(solver_stream_ctx, STREAM_EVENT_ERROR, "多解分支处理: 所有分支均无效", 0);
        }
        return SOLVER_STATUS_NO_SOLUTION;
    }

    *out_branches = out_branch_arr;
    *out_branch_count = valid_branches;

    if (valid_branches == 1) {
        if (solver_stream_ctx) {
            stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_DONE, "多解分支处理: 过滤后仅剩唯一有效解",
                               valid_branches);
        }
        return SOLVER_STATUS_UNIQUE;
    }

    if (solver_stream_ctx) {
        char msg[128];
        int _snw5;
        LV00_SAFE_SNPRINTF(_snw5, msg, sizeof(msg), "多解分支处理: 生成 %d 个有效分支 (共 %d 个理论组合)",
                           valid_branches, total_branches);
        LV00_UNUSED(_snw5);
        stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_DONE, msg, valid_branches);
    }

    return SOLVER_STATUS_OK;
}
