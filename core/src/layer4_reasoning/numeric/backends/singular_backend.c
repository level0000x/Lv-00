/**
 * @file singular_backend.c
 * @brief Singular 计算机代数系统后端实现
 *
 * 提供 Gröbner 基计算、多项式理想操作、约束图→多项式理想转换等
 * 多项式代数功能。与 CUDA/HIP 等向量/矩阵运算后端不同，Singular
 * 后端专注于符号计算。
 *
 * 本后端不再依赖 libsingular 内核库，而是将全部代数计算委托给
 * 项目自研的内部 Gröbner 引擎（layer4_reasoning/backends/）：
 *   - ring_create / ideal_create / ideal_add_generator
 *   - groebner_compute / ideal_membership
 *   - ideal_intersection / ideal_quotient
 *   - constraint_graph_to_ideal
 *
 * 后端通过"句柄 → 内部引擎对象"的数据映射层与内部引擎对接：
 *   - 环句柄     = 内部引擎的 ring_id（int）
 *   - 多项式句柄 = SingularPoly { ring_id, poly_id }
 *   - 理想句柄   = SingularIdeal { ring_id, nvars, var_ids（poly_id 数组） }
 *
 * @author Lv-00 Project
 * @version v1.1.0
 * @date 2026-08-03
 *
 * @dependencies
 *   - singular_backend.h             : Singular 后端公共接口
 *   - lv/groebner_engine.h           : 内部 Gröbner 引擎公共 API
 *   - groebner_engine_internal.h     : 内部引擎池化数据结构（句柄映射层使用）
 *   - lv/lv_utils.h / debug.h / lv_internal.h
 */

/* ========================================================================
 * 包含头文件
 * ======================================================================== */

#include "lv/backends/singular_backend.h"

#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "lv/lv_utils.h"
#include "lv/lv_thread.h"
#include "debug.h"
#include "lv/lv_internal.h"

#include "lv/groebner_engine.h"
#include "layer4_reasoning/backends/groebner_engine_internal.h"

#include "singular_engine_guard.h"
#include "lv/lv_numeric.h" /* lv_index_in_range */

/* ========================================================================
 * 数据结构定义
 * ======================================================================== */

/**
 * @brief Singular 多项式句柄
 *
 * 表示环中的一个多项式，poly_id 为内部 Gröbner 引擎多项式池的 ID。
 */
typedef struct {
    int ring_id;    /**< 所属多项式环的标识符 */
    int poly_id;    /**< 内部引擎多项式池中的 poly_id */
} SingularPoly;

/**
 * @brief Singular 理想描述符
 *
 * 表示一个多项式理想，包含所属环的标识和生成元列表。
 * var_ids 中的每个元素为内部引擎多项式池的 poly_id（-1 表示跳过/零多项式）。
 */
typedef struct {
    int ring_id;    /**< 所属多项式环的标识符 */
    int nvars;      /**< 生成元个数 */
    int *var_ids;   /**< 生成元对应的多项式 ID 数组（内部引擎 poly_id） */
} SingularIdeal;

/* ========================================================================
 * 后端版本信息
 * ======================================================================== */

/** @brief Singular 后端版本字符串 */
#define SINGULAR_BACKEND_VERSION "1.1.0"

/* ========================================================================
 * Singular 内部数据结构与状态管理
 * ======================================================================== */

/**
 * @brief Singular 后端状态（单例）
 *
 * 管理内部 Gröbner 引擎的环注册表与初始化状态。
 * 环、多项式、理想对象全部由内部引擎的注册表统一管理。
 */
typedef struct SingularState {
    int initialized;            /**< 后端是否已初始化 */
    int n_rings;                /**< 已创建的多项式环数量（信息统计） */
    lvRingRegistry *registry;   /**< 内部 Gröbner 引擎的环注册表 */
    int current_ring;           /**< 最近创建（当前活跃）的环 ID */
    char version_str[128];      /**< 后端版本字符串 */
    lv_lazy_lock lock;          /**< 状态访问锁（惰性初始化，保护初始化/清理/计数器） */
} SingularState;

/** @brief 全局 Singular 状态（静态单例） */
static SingularState g_singular_state = {0};

/** @brief 状态互斥锁的一次性初始化回调（由 lv_lazy_lock 触发，线程安全） */
static void singular_state_lock_init_once(void) {
    lv_mutex_init(&g_singular_state.lock.mutex);
}

/**
 * @brief 锁内快照内部环注册表指针
 *
 * 计算入口函数在锁内读取 registry 指针，避免与并发初始化/清理
 * 竞争导致的撕裂读或悬垂使用；registry 指针在初始化后保持不变。
 */
static lvRingRegistry *singular_registry_snapshot(void) {
    lv_lazy_lock_lock(&g_singular_state.lock, singular_state_lock_init_once);
    lvRingRegistry *reg = g_singular_state.registry;
    lv_lazy_lock_unlock(&g_singular_state.lock);
    return reg;
}

/* ========================================================================
 * 前向声明：内部辅助函数
 * ======================================================================== */

static int singular_kernel_init_locked(void);
static void singular_kernel_cleanup(void);
static int singular_create_ring(const char *var_names[], int nvars);
static int singular_create_ring_locked(const char *var_names[], int nvars);
static void *singular_compute_groebner(void *ideal_ptr);
static int singular_ideal_intersection(void *ideal_a, void *ideal_b, void **result);
static int singular_ideal_quotient(void *ideal_a, void *ideal_b, void **result);
static int singular_ideal_membership(void *poly, void *ideal);
static void *singular_constraint_graph_to_ideal(const int *constraint_ids,
                                                int n_constraints,
                                                const SingularVar *vars,
                                                int n_vars);

/* 数据映射层辅助函数（句柄 ↔ 内部引擎对象） */
static int singular_ideal_to_internal(const SingularIdeal *s_ideal);
static SingularIdeal *singular_polys_to_handle(lvPolynomialRing *ring, lvPolynomial **polys, int count);
static SingularIdeal *singular_ideal_from_internal(int ideal_id);
static SingularIdeal *singular_basis_from_internal(int ideal_id);
static void singular_internal_ideal_release(int ideal_id);

/* ========================================================================
 * 公共 API 实现
 * ======================================================================== */

int lv_singular_register_backend(void) {
    /* 持锁完成初始化检查与内核初始化，消除并发首次注册时的双重初始化竞态 */
    lv_lazy_lock_lock(&g_singular_state.lock, singular_state_lock_init_once);
    if (g_singular_state.initialized) {
        LOG_INFO("Singular", "后端已注册（内核已初始化，版本: %s）", g_singular_state.version_str);
        lv_lazy_lock_unlock(&g_singular_state.lock);
        return 0;
    }

    if (singular_kernel_init_locked() != 0) {
        lv_lazy_lock_unlock(&g_singular_state.lock);
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "Singular 内核初始化失败");
    }
    lv_lazy_lock_unlock(&g_singular_state.lock);

    lv_numerical_backend_register(lv_BACKEND_SINGULAR, NULL, NULL, NULL);

    LOG_INFO("Singular", "后端注册成功（版本: %s）", g_singular_state.version_str);
    return 0;
}

int lv_singular_available(void) {
    lv_lazy_lock_lock(&g_singular_state.lock, singular_state_lock_init_once);
    int available = g_singular_state.initialized ? 1 : 0;
    lv_lazy_lock_unlock(&g_singular_state.lock);
    return available;
}

const char *lv_singular_backend_version(void) {
    lv_lazy_lock_lock(&g_singular_state.lock, singular_state_lock_init_once);
    const char *version = g_singular_state.initialized
                              ? (const char *)g_singular_state.version_str
                              : SINGULAR_BACKEND_VERSION;
    lv_lazy_lock_unlock(&g_singular_state.lock);
    return version;
}

/* ========================================================================
 * Singular 内核初始化 / 销毁
 * ======================================================================== */

/**
 * @brief 初始化 Singular 后端（内部实现，调用方须持有 g_singular_state.lock）
 *
 * 创建内部 Gröbner 引擎的环注册表，并建立默认的 ℚ[x] 初始环
 * （与 Singular 内核"启动即存在默认环"的行为一致）。
 *
 * @return 成功返回 0，失败返回 -1
 */
static int singular_kernel_init_locked(void) {
    if (g_singular_state.initialized) {
        return 0;
    }

    /* 创建内部 Gröbner 引擎的环注册表（全局单例） */
    g_singular_state.registry = ring_registry_create(8);
    if (!g_singular_state.registry) {
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "Singular 引擎初始化失败：无法创建环注册表");
    }

    /* 创建默认的 ℚ[x] 环作为初始环 */
    const char *default_vars[] = {"x"};
    if (singular_create_ring_locked(default_vars, 1) < 0) {
        ring_registry_destroy(g_singular_state.registry);
        g_singular_state.registry = NULL;
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "Singular 默认环创建失败");
    }

    /* 记录版本信息 */
    snprintf(g_singular_state.version_str, sizeof(g_singular_state.version_str),
             "Singular %s (internal groebner engine)", SINGULAR_BACKEND_VERSION);

    g_singular_state.initialized = 1;
    return 0;
}

/**
 * @brief 清理 Singular 后端状态（线程安全）
 *
 * 销毁环注册表（内部引擎会级联释放所有环、多项式与理想对象）。
 * 通常在进程退出或后端注销时调用。
 */
static void singular_kernel_cleanup(void) {
    lv_lazy_lock_lock(&g_singular_state.lock, singular_state_lock_init_once);
    if (!g_singular_state.initialized) {
        lv_lazy_lock_unlock(&g_singular_state.lock);
        return;
    }

    if (g_singular_state.registry) {
        ring_registry_destroy(g_singular_state.registry);
        g_singular_state.registry = NULL;
    }

    g_singular_state.n_rings = 0;
    g_singular_state.current_ring = 0;
    g_singular_state.initialized = 0;

    lv_lazy_lock_unlock(&g_singular_state.lock);
    LOG_INFO("Singular", "内核已清理");
}

/* ========================================================================
 * 多项式环操作
 * ======================================================================== */

/**
 * @brief 创建多项式环（内部实现，调用方须持有 g_singular_state.lock）
 *
 * 在内部 Gröbner 引擎中创建一个多项式环
 * K[var_0, ..., var_{nvars-1}]，其中 K 为 ℚ 系数域（与 Singular
 * 默认一致），单项式序为 grevlex（Singular 默认全局序）。
 *
 * @param var_names  变量名数组（每个名称以 null 结尾）
 * @param nvars      变量个数
 * @return 环标识符（≥0，内部引擎 ring_id），失败返回 -1
 */
static int singular_create_ring_locked(const char *var_names[], int nvars) {
    if (!var_names || nvars <= 0) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "singular_create_ring: 无效参数");
    }
    if (!singular_registry_has(g_singular_state.registry)) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "singular_create_ring: 引擎未初始化");
    }

    /* 内部引擎：ℚ 系数域 + grevlex 单项式序（与 Singular 默认设置一致） */
    int ring_id = ring_create(g_singular_state.registry, var_names, nvars,
                              RING_FIELD_RATIONAL, MONOMIAL_GREVLEX, "singular");
    if (ring_id < 0) {
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "singular_create_ring: 内部引擎建环失败");
    }

    g_singular_state.n_rings++;
    g_singular_state.current_ring = ring_id;

    LOG_INFO("Singular", "环创建: %d 个变量, ring_id=%d", nvars, ring_id);
    return ring_id;
}

/**
 * @brief 创建多项式环（线程安全入口）
 *
 * 持锁保护计数器（n_rings / current_ring）与注册表读取，
 * 避免多线程并发建环时丢失统计或读写竞争。
 */
static int singular_create_ring(const char *var_names[], int nvars) {
    lv_lazy_lock_lock(&g_singular_state.lock, singular_state_lock_init_once);
    int ring_id = singular_create_ring_locked(var_names, nvars);
    lv_lazy_lock_unlock(&g_singular_state.lock);
    return ring_id;
}

/* ========================================================================
 * 数据映射层：句柄 ↔ 内部引擎对象
 * ======================================================================== */

/**
 * @brief 将 SingularIdeal 句柄映射为内部引擎理想 ID
 *
 * 在内部引擎中新建理想（ring_id 取自句柄）并逐一添加生成元
 * （生成元为句柄中的 poly_id），返回内部 ideal_id。
 * 生成的临时理想仅引用句柄的生成元多项式，不拥有它们。
 *
 * @param s_ideal SingularIdeal 句柄
 * @return >= 0 内部 ideal_id，失败返回 -1
 */
static int singular_ideal_to_internal(const SingularIdeal *s_ideal) {
    if (!s_ideal || s_ideal->nvars < 0 || (s_ideal->nvars > 0 && !s_ideal->var_ids)) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "singular_ideal_to_internal: 无效理想句柄");
    }
    lvRingRegistry *reg = singular_registry_snapshot();
    if (!singular_registry_has(reg)) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "singular_ideal_to_internal: 引擎未初始化");
    }

    int ideal_id = ideal_create(reg, s_ideal->ring_id, "singular_ideal");
    if (ideal_id < 0) {
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "singular_ideal_to_internal: ideal_create 失败");
    }

    for (int i = 0; i < s_ideal->nvars; i++) {
        if (s_ideal->var_ids[i] < 0) {
            continue; /* 占位符（零多项式）跳过 */
        }
        if (ideal_add_generator(reg, ideal_id, s_ideal->var_ids[i]) != 0) {
            ideal_destroy(reg, ideal_id);
            lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM,
                            "singular_ideal_to_internal: 生成元 poly_id=%d 无效",
                            s_ideal->var_ids[i]);
        }
    }
    return ideal_id;
}

/**
 * @brief 将多项式数组包装为 SingularIdeal 句柄
 *
 * 对每个多项式深拷贝并注册进内部引擎多项式池（获得独立 poly_id），
 * 因此句柄与源理想/基之间不存在共享所有权，释放互不影响。
 *
 * @param ring  多项式所属的内部引擎环
 * @param polys 多项式指针数组
 * @param count 多项式个数
 * @return SingularIdeal 句柄，失败返回 NULL
 */
static SingularIdeal *singular_polys_to_handle(lvPolynomialRing *ring,
                                               lvPolynomial **polys,
                                               int count) {
    if (!ring || count < 0 || (count > 0 && !polys)) {
        return NULL;
    }

    SingularIdeal *h = (SingularIdeal *)lv_calloc(1, sizeof(SingularIdeal));
    if (!h) {
        return NULL;
    }
    h->ring_id = ring->ring_id;
    h->nvars = count;
    h->var_ids = count > 0 ? (int *)lv_calloc((size_t)count, sizeof(int)) : NULL;
    if (count > 0 && !h->var_ids) {
        lv_free((void **)&h);
        return NULL;
    }

    /* 期望入池的多项式个数 */
    int expect = 0;
    for (int k = 0; k < count; k++) {
        h->var_ids[k] = -1;
        if (polys[k]) expect++;
    }

    /* 深拷贝并注册进多项式池（poly_internal_store 不自带锁，需持有全局锁） */
    int stored = 0;
    lvLockGuard _lg;
    lv_lock_guard_init(&_lg, &g_data_mutex);
    if (!g_data) {
        lv_lock_guard_destroy(&_lg);
        lv_free((void **)&h->var_ids);
        lv_free((void **)&h);
        return NULL;
    }
    for (int k = 0; k < count; k++) {
        if (!polys[k]) {
            continue;
        }
        lvPolynomial *copy = poly_internal_copy(polys[k], ring);
        if (!copy) {
            break;
        }
        int pid = poly_internal_store(g_data, copy);
        if (pid < 0) {
            poly_internal_destroy(copy);
            break;
        }
        h->var_ids[k] = pid;
        stored++;
    }
    lv_lock_guard_destroy(&_lg);

    if (stored != expect) {
        /* 失败回滚：释放已入池的副本（poly_destroy 内部加锁，须在锁外调用） */
        lvRingRegistry *reg = singular_registry_snapshot();
        for (int k = 0; k < count; k++) {
            if (h->var_ids[k] >= 0 && singular_registry_has(reg)) {
                poly_destroy(reg, h->var_ids[k]);
            }
        }
        lv_free((void **)&h->var_ids);
        lv_free((void **)&h);
        return NULL;
    }
    return h;
}

/**
 * @brief 将内部引擎理想包装为 SingularIdeal 句柄
 *
 * 提取内部理想的全部生成元多项式并构建句柄。
 *
 * @param ideal_id 内部理想 ID
 * @return SingularIdeal 句柄，失败返回 NULL
 */
static SingularIdeal *singular_ideal_from_internal(int ideal_id) {
    lvRingRegistry *reg = singular_registry_snapshot();
    if (!singular_registry_has(reg)) {
        return NULL;
    }

    lvPolynomial **gens = NULL;
    int count = 0;
    int ring_id = -1;
    {
        lvLockGuard _lg;
        lv_lock_guard_init(&_lg, &g_data_mutex);
        if (!g_data || !lv_index_in_range(ideal_id, g_data->ideal_count) ||
            !g_data->ideals[ideal_id]) {
            lv_lock_guard_destroy(&_lg);
            return NULL;
        }
        lvIdeal *ideal = g_data->ideals[ideal_id];
        count = ideal->generator_count;
        ring_id = ideal->ring_id;
        if (count > 0) {
            gens = (lvPolynomial **)lv_calloc((size_t)count, sizeof(lvPolynomial *));
            if (gens) {
                for (int k = 0; k < count; k++) {
                    gens[k] = ideal->generators[k];
                }
            }
        }
        lv_lock_guard_destroy(&_lg);
    }

    if (ring_id < 0 || (count > 0 && !gens)) {
        lv_free((void **)&gens);
        return NULL;
    }
    lvPolynomialRing *ring = ring_find(reg, ring_id);
    if (!ring) {
        lv_free((void **)&gens);
        return NULL;
    }

    SingularIdeal *h = singular_polys_to_handle(ring, gens, count);
    lv_free((void **)&gens);
    return h;
}

/**
 * @brief 提取内部理想的缓存 Gröbner 基并包装为 SingularIdeal 句柄
 *
 * 要求目标理想的 Gröbner 基已计算（basis_valid 为真）。
 *
 * @param ideal_id 内部理想 ID
 * @return 以基多项式为生成元的 SingularIdeal 句柄，失败返回 NULL
 */
static SingularIdeal *singular_basis_from_internal(int ideal_id) {
    lvRingRegistry *reg = singular_registry_snapshot();
    if (!reg) {
        return NULL;
    }

    lvPolynomial **basis_polys = NULL;
    int count = 0;
    int ring_id = -1;
    {
        lvLockGuard _lg;
        lv_lock_guard_init(&_lg, &g_data_mutex);
        if (!g_data || !lv_index_in_range(ideal_id, g_data->ideal_count) ||
            !g_data->ideals[ideal_id]) {
            lv_lock_guard_destroy(&_lg);
            return NULL;
        }
        lvIdeal *ideal = g_data->ideals[ideal_id];
        if (ideal->cached_basis && ideal->basis_valid) {
            count = ideal->cached_basis->bases_count;
            ring_id = ideal->ring_id;
            if (count > 0) {
                basis_polys = (lvPolynomial **)lv_calloc((size_t)count, sizeof(lvPolynomial *));
                if (basis_polys) {
                    for (int k = 0; k < count; k++) {
                        basis_polys[k] = ideal->cached_basis->basis_polys[k];
                    }
                }
            }
        }
        lv_lock_guard_destroy(&_lg);
    }

    if (ring_id < 0) {
        lv_free((void **)&basis_polys);
        return NULL;
    }
    lvPolynomialRing *ring = ring_find(reg, ring_id);
    if (!ring) {
        lv_free((void **)&basis_polys);
        return NULL;
    }

    SingularIdeal *h = singular_polys_to_handle(ring, basis_polys, count);
    lv_free((void **)&basis_polys);
    return h;
}

/**
 * @brief 释放内部引擎结果理想及其独占的生成元多项式
 *
 * 仅用于释放本后端自行创建的结果理想（交/商/约束图编码结果）。
 * 这些理想的生成元由理想独占持有；已注册进多项式池的用 poly_destroy
 * 释放，未注册的（如 constraint_graph_to_ideal 内部生成的临时多项式）
 * 用 poly_internal_destroy 释放。
 *
 * @param ideal_id 内部理想 ID
 */
static void singular_internal_ideal_release(int ideal_id) {
    lvRingRegistry *reg = singular_registry_snapshot();
    if (!reg) {
        return;
    }

    /* 先快照生成元指针，避免持锁期间执行销毁操作 */
    lvPolynomial **gens = NULL;
    int count = 0;
    {
        lvLockGuard _lg;
        lv_lock_guard_init(&_lg, &g_data_mutex);
        if (g_data && lv_index_in_range(ideal_id, g_data->ideal_count) &&
            g_data->ideals[ideal_id]) {
            lvIdeal *ideal = g_data->ideals[ideal_id];
            count = ideal->generator_count;
            if (count > 0) {
                gens = (lvPolynomial **)lv_calloc((size_t)count, sizeof(lvPolynomial *));
                if (gens) {
                    for (int k = 0; k < count; k++) {
                        gens[k] = ideal->generators[k];
                    }
                }
            }
        }
        lv_lock_guard_destroy(&_lg);
    }

    /* 销毁理想结构（释放缓存基与生成元数组，不释放生成元多项式本身） */
    ideal_destroy(reg, ideal_id);

    for (int k = 0; k < count && gens; k++) {
        lvPolynomial *g = gens[k];
        if (!g) {
            continue;
        }
        bool in_pool = false;
        {
            lvLockGuard _lg;
            lv_lock_guard_init(&_lg, &g_data_mutex);
            if (g_data && g->poly_id >= 0 && g->poly_id < g_data->poly_count) {
                in_pool = (g_data->polys[g->poly_id] == g);
            }
            lv_lock_guard_destroy(&_lg);
        }
        if (in_pool) {
            poly_destroy(reg, g->poly_id);
        } else {
            poly_internal_destroy(g);
        }
    }
    lv_free((void **)&gens);
}

/* ========================================================================
 * Gröbner 基计算
 * ======================================================================== */

/**
 * @brief 计算 Gröbner 基
 *
 * 对给定的多项式理想计算（关于 grevlex 单项式序的）Gröbner 基。
 * 内部调用内部 Gröbner 引擎的 groebner_compute（自研 Buchberger
 * 实现，等价于 Singular 的 kStd 语义）。
 *
 * @param ideal_ptr  SingularIdeal 句柄
 * @return 以基多项式为生成元的 SingularIdeal 句柄，失败返回 NULL
 */
static void *singular_compute_groebner(void *ideal_ptr) {
    SingularIdeal *s_ideal = (SingularIdeal *)ideal_ptr;
    if (!s_ideal) {
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "singular_compute_groebner: 理想指针为空");
    }
    lvRingRegistry *reg = singular_registry_snapshot();
    if (!reg) {
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_STATE, "singular_compute_groebner: 引擎未初始化");
    }

    int ideal_id = singular_ideal_to_internal(s_ideal);
    if (ideal_id < 0) {
        lv_RETURN_ERROR_NULL(lv_ERROR_INTERNAL, "singular_compute_groebner: 理想映射失败");
    }

    /* 委托内部引擎计算 Gröbner 基（结果缓存于理想对象中） */
    if (groebner_compute(reg, ideal_id, GROEBNER_AUTO) != 0) {
        ideal_destroy(reg, ideal_id);
        lv_RETURN_ERROR_NULL(lv_ERROR_INTERNAL, "singular_compute_groebner: groebner_compute 失败");
    }

    /* 提取基多项式并包装为句柄 */
    SingularIdeal *basis_handle = singular_basis_from_internal(ideal_id);

    /* 临时理想仅引用句柄的生成元多项式，释放时不得销毁生成元 */
    ideal_destroy(reg, ideal_id);

    if (!basis_handle) {
        lv_RETURN_ERROR_NULL(lv_ERROR_INTERNAL, "singular_compute_groebner: Gröbner 基提取失败");
    }
    LOG_INFO("Singular", "Gröbner 基计算完成");
    return basis_handle;
}

/* ========================================================================
 * 多项式理想操作
 * ======================================================================== */

/**
 * @brief 计算两个理想的交 I ∩ J
 *
 * @param ideal_a  理想 A（SingularIdeal 句柄）
 * @param ideal_b  理想 B（SingularIdeal 句柄）
 * @param result   输出参数：I ∩ J 的理想句柄
 * @return 成功返回 0，失败返回 -1
 */
static int singular_ideal_intersection(void *ideal_a, void *ideal_b, void **result) {
    SingularIdeal *ia = (SingularIdeal *)ideal_a;
    SingularIdeal *ib = (SingularIdeal *)ideal_b;
    if (!ia || !ib || !result) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "singular_ideal_intersection: 无效参数");
    }
    lvRingRegistry *reg = singular_registry_snapshot();
    if (!reg) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "singular_ideal_intersection: 引擎未初始化");
    }
    if (ia->ring_id != ib->ring_id) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "singular_ideal_intersection: 两个理想所属环不一致");
    }

    int ida = singular_ideal_to_internal(ia);
    if (ida < 0) {
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "singular_ideal_intersection: 理想 A 映射失败");
    }
    int idb = singular_ideal_to_internal(ib);
    if (idb < 0) {
        ideal_destroy(reg, ida);
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "singular_ideal_intersection: 理想 B 映射失败");
    }

    /* 委托内部引擎计算交（生成元注册进多项式池） */
    int res = ideal_intersection(reg, ida, idb);
    ideal_destroy(reg, ida);
    ideal_destroy(reg, idb);
    if (res < 0) {
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "singular_ideal_intersection: ideal_intersection 失败");
    }

    SingularIdeal *h = singular_ideal_from_internal(res);
    singular_internal_ideal_release(res);
    if (!h) {
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "singular_ideal_intersection: 结果句柄创建失败");
    }

    LOG_INFO("Singular", "理想交计算完成");
    *result = h;
    return 0;
}

/**
 * @brief 计算两个理想的商 I : J
 *
 * @param ideal_a  理想 A（SingularIdeal 句柄）
 * @param ideal_b  理想 B（SingularIdeal 句柄）
 * @param result   输出参数：I : J 的理想句柄
 * @return 成功返回 0，失败返回 -1
 */
static int singular_ideal_quotient(void *ideal_a, void *ideal_b, void **result) {
    SingularIdeal *ia = (SingularIdeal *)ideal_a;
    SingularIdeal *ib = (SingularIdeal *)ideal_b;
    if (!ia || !ib || !result) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "singular_ideal_quotient: 无效参数");
    }
    lvRingRegistry *reg = singular_registry_snapshot();
    if (!reg) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "singular_ideal_quotient: 引擎未初始化");
    }
    if (ia->ring_id != ib->ring_id) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "singular_ideal_quotient: 两个理想所属环不一致");
    }

    int ida = singular_ideal_to_internal(ia);
    if (ida < 0) {
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "singular_ideal_quotient: 理想 A 映射失败");
    }
    int idb = singular_ideal_to_internal(ib);
    if (idb < 0) {
        ideal_destroy(reg, ida);
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "singular_ideal_quotient: 理想 B 映射失败");
    }

    /* 委托内部引擎计算商（生成元注册进多项式池） */
    int res = ideal_quotient(reg, ida, idb, "singular_quotient");
    ideal_destroy(reg, ida);
    ideal_destroy(reg, idb);
    if (res < 0) {
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "singular_ideal_quotient: ideal_quotient 失败");
    }

    SingularIdeal *h = singular_ideal_from_internal(res);
    singular_internal_ideal_release(res);
    if (!h) {
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "singular_ideal_quotient: 结果句柄创建失败");
    }

    LOG_INFO("Singular", "理想商计算完成");
    *result = h;
    return 0;
}

/**
 * @brief 多项式成员判定：检查多项式 p 是否属于理想 I
 *
 * @param poly   SingularPoly 句柄
 * @param ideal  SingularIdeal 句柄
 * @return 若 p ∈ I 返回 1，否则返回 0，出错返回 -1
 */
static int singular_ideal_membership(void *poly, void *ideal) {
    SingularPoly *sp = (SingularPoly *)poly;
    SingularIdeal *si = (SingularIdeal *)ideal;
    if (!sp || !si) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "singular_ideal_membership: 无效参数");
    }
    lvRingRegistry *reg = singular_registry_snapshot();
    if (!reg) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "singular_ideal_membership: 引擎未初始化");
    }
    if (sp->ring_id != si->ring_id) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "singular_ideal_membership: 多项式与理想所属环不一致");
    }

    int ideal_id = singular_ideal_to_internal(si);
    if (ideal_id < 0) {
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "singular_ideal_membership: 理想映射失败");
    }

    /* 委托内部引擎做成员判定（基于理想 Gröbner 基标准型归约） */
    bool belongs = ideal_membership(reg, ideal_id, sp->poly_id);
    ideal_destroy(reg, ideal_id);

    LOG_INFO("Singular", "成员判定完成");
    return belongs ? 1 : 0;
}

/* ========================================================================
 * 约束图 → 多项式理想转换
 * ======================================================================== */

/**
 * @brief 将约束图的结构信息转换为多项式理想
 *
 * 在约束求解和多项式优化场景中，约束图中的每个约束可以编码为
 * 多项式方程 fi(x) = 0。本函数将约束标识符集转换为对应的
 * 多项式理想 I = <f1, f2, ..., fk>。
 *
 * 转换完全委托给内部 Gröbner 引擎的 constraint_graph_to_ideal：
 *   1. 用 vars 中的变量名创建多项式环（ℚ[x0,...,x_{n-1}]）
 *   2. 构建约束图：每两个变量构成一个 POINT 节点的 (x, y)，
 *      坐标为对应 vars 的数值（经 1e6 缩放转为有理数）
 *   3. 约束 ID 按约定注册为 INCIDENCE 约束（参与者为相邻 POINT 节点）
 *   4. 调用 constraint_graph_to_ideal 完成"约束图 → 理想"编码
 *
 * @param constraint_ids  约束标识符数组
 * @param n_constraints   约束个数
 * @param vars            变量描述符数组（提供变量名和数值）
 * @param n_vars          变量个数
 * @return SingularIdeal 句柄，失败返回 NULL
 *
 * @note 编码规则与内部引擎 constraint_graph_to_ideal 保持一致：
 *       每个 POINT 节点的符号坐标编码为常量方程 (x - val = 0)；
 *       INCIDENCE 编码为叉积方程；BETWEENNESS 编码为共线性方程；
 *       其余约束类型由内部实现处理。
 */
static void *singular_constraint_graph_to_ideal(const int *constraint_ids,
                                                int n_constraints,
                                                const SingularVar *vars,
                                                int n_vars) {
    if (!constraint_ids || n_constraints <= 0 || !vars || n_vars <= 0) {
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM,
                             "singular_constraint_graph_to_ideal: 无效参数");
    }
    lvRingRegistry *reg = singular_registry_snapshot();
    if (!reg) {
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_STATE,
                             "singular_constraint_graph_to_ideal: 引擎未初始化");
    }

    /* 1. 创建包含全部变量的多项式环 */
    const char **names = (const char **)lv_malloc((size_t)n_vars * sizeof(char *));
    if (!names) {
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY,
                             "singular_constraint_graph_to_ideal: 变量名数组分配失败");
    }
    for (int i = 0; i < n_vars; i++) {
        names[i] = vars[i].name;
    }
    int ring_id = singular_create_ring(names, n_vars);
    lv_free((void **)&names);
    if (ring_id < 0) {
        return NULL;
    }

    /* 2. 构建约束图：每两个变量构成一个 POINT 节点的 (x, y) */
    ConstraintGraph *graph = graph_create();
    if (!graph) {
        lv_RETURN_ERROR_NULL(lv_ERROR_INTERNAL,
                             "singular_constraint_graph_to_ideal: graph_create 失败");
    }
    int point_count = n_vars / 2;
    for (int i = 0; i < point_count; i++) {
        SymbolicCoord *coords[2] = {
            symbolic_coord_from_double_scaled(vars[2 * i].val, 1000000),
            symbolic_coord_from_double_scaled(vars[2 * i + 1].val, 1000000)
        };
        if (!coords[0] || !coords[1]) {
            if (coords[0]) {
                symbolic_coord_destroy(coords[0]);
            }
            if (coords[1]) {
                symbolic_coord_destroy(coords[1]);
            }
            graph_destroy(graph);
            lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY,
                                 "singular_constraint_graph_to_ideal: 符号坐标创建失败");
        }
        GeomNode *node = graph_add_node_with_id(graph, i, GEOM_POINT, coords, 2);
        symbolic_coord_destroy(coords[0]);
        symbolic_coord_destroy(coords[1]);
        if (!node) {
            graph_destroy(graph);
            lv_RETURN_ERROR_NULL(lv_ERROR_INTERNAL,
                                 "singular_constraint_graph_to_ideal: POINT 节点创建失败");
        }
    }

    /* 3. 注册约束：函数签名未提供约束类型信息，按约定注册为
     *    INCIDENCE（参与者为相邻 POINT 节点），具体编码由
     *    constraint_graph_to_ideal 内部实现完成。 */
    if (point_count > 0) {
        for (int i = 0; i < n_constraints; i++) {
            int p1 = constraint_ids[i] % point_count;
            if (p1 < 0) {
                p1 += point_count;
            }
            int p2 = (p1 + 1) % point_count;
            int participants[2] = {p1, p2};
            if (!graph_add_constraint_with_id(graph, constraint_ids[i],
                                              INCIDENCE, participants, 2)) {
                graph_destroy(graph);
                lv_RETURN_ERROR_NULL(lv_ERROR_INTERNAL,
                                     "singular_constraint_graph_to_ideal: 约束注册失败");
            }
        }
    }

    /* 4. 委托内部引擎完成"约束图 → 多项式理想"编码 */
    int ideal_id = constraint_graph_to_ideal(reg, graph, ring_id,
                                             "singular_constraint_ideal");
    graph_destroy(graph);
    if (ideal_id < 0) {
        lv_RETURN_ERROR_NULL(lv_ERROR_INTERNAL,
                             "singular_constraint_graph_to_ideal: 编码失败");
    }

    /* 5. 包装为理想句柄并释放临时理想 */
    SingularIdeal *h = singular_ideal_from_internal(ideal_id);
    singular_internal_ideal_release(ideal_id);
    if (!h) {
        lv_RETURN_ERROR_NULL(lv_ERROR_INTERNAL,
                             "singular_constraint_graph_to_ideal: 结果句柄创建失败");
    }

    LOG_INFO("Singular", "约束图→理想转换: %d 个约束, %d 个变量", n_constraints, n_vars);
    return h;
}

/* ========================================================================
 * 高层窗口函数（供 layer4 重写器调用）
 * ======================================================================== */

/**
 * @brief 计算给定理想的 Gröbner 基并返回标准型
 *
 * @param ideal_ptr  输入理想
 * @return Gröbner 基（以 ideal_t* 形式），失败返回 NULL
 */
void *lv_singular_groebner_basis(void *ideal_ptr) {
    return singular_compute_groebner(ideal_ptr);
}

/**
 * @brief 计算两个理想的交
 *
 * @param ideal_a  理想 A
 * @param ideal_b  理想 B
 * @return I ∩ J 的理想指针，失败返回 NULL
 */
void *lv_singular_ideal_intersect(void *ideal_a, void *ideal_b) {
    void *result = NULL;
    if (singular_ideal_intersection(ideal_a, ideal_b, &result) != 0) {
        lv_RETURN_ERROR_NULL(lv_ERROR_INTERNAL, "lv_singular_ideal_intersect: 理想交运算失败");
    }
    return result;
}

/**
 * @brief 计算两个理想的商
 *
 * @param ideal_a  理想 A
 * @param ideal_b  理想 B
 * @return I : J 的理想指针，失败返回 NULL
 */
void *lv_singular_ideal_quotient_op(void *ideal_a, void *ideal_b) {
    void *result = NULL;
    if (singular_ideal_quotient(ideal_a, ideal_b, &result) != 0) {
        lv_RETURN_ERROR_NULL(lv_ERROR_INTERNAL, "lv_singular_ideal_quotient_op: 理想商运算失败");
    }
    return result;
}

/**
 * @brief 检查多项式是否属于理想
 *
 * @param poly   多项式
 * @param ideal  理想
 * @return 1 表示属于，0 表示不属于，-1 表示出错
 */
int lv_singular_membership_test(void *poly, void *ideal) {
    return singular_ideal_membership(poly, ideal);
}

/**
 * @brief 从约束图构建多项式理想
 *
 * @param constraint_ids  约束 ID 数组
 * @param n_constraints   约束个数
 * @param vars            变量描述符数组
 * @param n_vars          变量个数
 * @return 理想指针，失败返回 NULL
 */
void *lv_singular_graph_to_ideal(const int *constraint_ids,
                                 int n_constraints,
                                 const SingularVar *vars,
                                 int n_vars) {
    return singular_constraint_graph_to_ideal(constraint_ids, n_constraints,
                                              vars, n_vars);
}
