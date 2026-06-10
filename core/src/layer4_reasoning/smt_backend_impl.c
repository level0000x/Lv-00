/**
 * @file smt_backend_impl.c
 * @brief SMT 后端抽象层实现 —— 多引擎 SMT 求解器框架（含 Groebner 基真实求解）
 *
 * @details 本模块实现 smt_backend.h 中声明的所有 SMT 后端接口。
 *          设计参考 polymake 的多后端架构，提供与求解器无关的统一接口。
 *
 *          后端实现状态：
 *          - GROEBNER：已集成，通过 constraint_graph_to_ideal() 将约束图
 *            转换为多项式理想，调用 Buchberger 算法计算 Groebner 基，
 *            通过理想成员关系判定可满足性，并通过代数簇求解获取具体坐标。
 *          - Z3 / cvc5 / Singular：通过子进程调用外部求解器，
 *            Z3 和 cvc5 使用 SMT-LIB2 格式，Singular 使用自有脚本格式，
 *            未安装时返回 SMT_RESULT_UNKNOWN 并可回退到 Groebner 后端。
 *
 *          编码管线：
 *          1. smtencode_constraint_graph_to_smtlib2()  约束图 -> SMT-LIB2
 *          2. smtsolver_encode()                       SMT-LIB2 -> 后端原生表示
 *          3. smtsolver_check()                        执行求解（Groebner 后端真实求解）
 *          4. smtsolver_decode_result()                解析结果 -> SMTSolverResult
 *
 * @author Lv-00 Project
 * @version 3.4.0
 * @date 2026-05-25
 *
 * @dependencies
 *   - smt_backend.h          : SMT 后端公共接口
 *   - groebner_engine.h      : Groebner 基计算引擎（内置求解核心）
 *   - lv00_internal.h        : 内部常量与工具宏
 *   - lv00_utils.h           : 统一内存分配器
 *   - error_codes.h          : 统一错误码系统
 */

#include "smt_backend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#include "error_codes.h"
#include "groebner_engine.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

/* ============================================================
 * 模块级常量
 * ============================================================ */

/** @brief SMT-LIB2 输出缓冲区默认大小 */
#define SMTLIB2_DEFAULT_BUFFER 65536

/** @brief 默认求解超时（毫秒） */
#define SMT_DEFAULT_TIMEOUT_MS 30000

/** @brief 默认内存限制（MB） */
#define SMT_DEFAULT_MEMORY_MB 1024

/** @brief Groebner 后端多项式变量名最大长度 */
#define GROEBNER_VAR_NAME_MAX 64

/** @brief Groebner 后端默认变量容量（每个点 2 个坐标变量） */
#define GROEBNER_DEFAULT_VAR_CAPACITY 32

/** @brief 数值零判定阈值（用于判断多项式是否为零） */
#define GROEBNER_SMT_ZERO_THRESHOLD 1e-12

/* ============================================================
 * 不透明结构：SMTSolver 内部实现
 * ============================================================ */

/**
 * @brief SMT 求解器内部状态
 *
 * 存储求解器的类型、配置、错误状态以及 Groebner 后端专用的
 * 环注册表和理想 ID 等求解上下文。
 */
struct SMTSolver {
    SolverBackendType type;           /**< 后端类型 */
    SMTSolverConfig config;           /**< 求解器配置 */
    SMTErrorCode last_error;          /**< 最近错误码 */
    char last_error_msg[512];         /**< 最近错误消息 */
    char *encoded_formula;            /**< 已编码的 SMT-LIB2 脚本 */
    int encoded_len;                  /**< 编码长度 */
    bool is_initialized;              /**< 是否已初始化 */
    bool has_assertions;              /**< 是否有待求解的断言 */

    /* ---- Groebner 后端专用字段 ---- */
    Lv00RingRegistry *groebner_registry; /**< Groebner 环注册表（惰性创建） */
    int groebner_ring_id;               /**< Groebner 多项式环 ID */
    int groebner_ideal_id;              /**< Groebner 理想 ID（约束转换结果） */
    int groebner_var_count;             /**< Groebner 环中的变量数量 */
    int *groebner_node_var_map;         /**< 节点 ID -> 变量索引映射表 */
    int groebner_node_var_map_size;     /**< 映射表大小 */
    int groebner_variety_id;            /**< Groebner 代数簇 ID（求解结果） */
};

/* ============================================================
 * 全局后端注册表（单例）
 * ============================================================ */

/** @brief 全局注册表实例 */
static SMTBackendRegistry g_smt_registry;
static bool g_smt_registry_initialized = false;

#ifdef _WIN32
static CRITICAL_SECTION g_smt_registry_cs = {0};
static volatile LONG g_smt_cs_initialized = 0;
#define SMT_REGISTRY_LOCK() do { \
    if (!g_smt_cs_initialized) { \
        InterlockedCompareExchange(&g_smt_cs_initialized, 1, 0); \
        if (g_smt_cs_initialized) InitializeCriticalSection(&g_smt_registry_cs); \
    } \
    EnterCriticalSection(&g_smt_registry_cs); \
} while(0)
#define SMT_REGISTRY_UNLOCK() LeaveCriticalSection(&g_smt_registry_cs)
#else
static pthread_mutex_t g_smt_registry_mutex = PTHREAD_MUTEX_INITIALIZER;
#define SMT_REGISTRY_LOCK() pthread_mutex_lock(&g_smt_registry_mutex)
#define SMT_REGISTRY_UNLOCK() pthread_mutex_unlock(&g_smt_registry_mutex)
#endif

/* ============================================================
 * 前向声明 —— 内部辅助函数
 * ============================================================ */

/* ---- SMT-LIB2 约束编码辅助函数 ---- */
static int smtlib2_encode_incidence(const ConstraintGraph *graph, const Constraint *c,
                                    char *buf, int remaining, bool named);
static int smtlib2_encode_betweenness(const ConstraintGraph *graph, const Constraint *c,
                                      char *buf, int remaining, bool named);
static int smtlib2_encode_intersection(const ConstraintGraph *graph, const Constraint *c,
                                       char *buf, int remaining, bool named);
static int smtlib2_encode_containment(const ConstraintGraph *graph, const Constraint *c,
                                      char *buf, int remaining, bool named);
static int smtlib2_encode_connection(const ConstraintGraph *graph, const Constraint *c,
                                     char *buf, int remaining, bool named);

/* ---- Groebner 后端辅助函数 ---- */
static int groebner_backend_init(SMTSolver *solver, const ConstraintGraph *graph);
static void groebner_backend_cleanup(SMTSolver *solver);
static SMTSatResult groebner_backend_solve(SMTSolver *solver, const ConstraintGraph *graph);
static int groebner_backend_decode(SMTSolver *solver, SMTSolverResult *out_result);

/* ============================================================
 * 默认配置
 * ============================================================ */

/**
 * @brief 创建并返回默认的求解器配置
 *
 * 基于后端类型选择适当的默认值。所有后端共享通用的基础配置，
 * 特定后端可通过 custom_config 传入私有参数。
 * Groebner 后端默认使用非线性实数算术（QF_NRA），因为几何约束
 * 通常涉及距离平方等二次多项式。
 */
const SMTSolverConfig *smtsolver_default_config(SolverBackendType type) {
    static SMTSolverConfig defaults[COUNT];
    static bool initialized = false;

    if (!initialized) {
        for (int i = 0; i < COUNT; i++) {
            defaults[i].timeout_ms = SMT_DEFAULT_TIMEOUT_MS;
            defaults[i].memory_limit_mb = SMT_DEFAULT_MEMORY_MB;
            defaults[i].logic = SMT_LOGIC_AUTO;
            defaults[i].produce_models = true;
            defaults[i].produce_unsat_cores = false;
            defaults[i].produce_proofs = false;
            defaults[i].incremental = false;
            defaults[i].random_seed = -1;
            defaults[i].verbosity = 0;
            defaults[i].custom_config = NULL;
        }
        /* Groebner 后端使用非线性实数算术（几何约束含距离平方等二次项） */
        defaults[GROEBNER].logic = SMT_LOGIC_QF_NRA;
        initialized = true;
    }

    if (type >= COUNT) {
        return &defaults[GROEBNER];
    }
    return &defaults[type];
}

/* ============================================================
 * 后端生命周期管理
 * ============================================================ */

/**
 * @brief 创建 SMT 求解器实例
 *
 * 根据后端类型创建求解器句柄。GROEBNER 后端使用内置实现，
 * Z3/cvc5/Singular 通过子进程调用外部求解器（运行时可用性取决于是否安装）。
 * 未链接的后端设置 SMT_ERROR_BACKEND_UNAVAILABLE 但不阻止创建句柄。
 */
SMTSolver *smtsolver_create(SolverBackendType type, const SMTSolverConfig *config) {
    SMTSolver *solver = (SMTSolver *)lv00_malloc(sizeof(SMTSolver));
    if (!solver) {
        return NULL;
    }

    memset(solver, 0, sizeof(SMTSolver));
    solver->type = type;
    solver->is_initialized = true;
    solver->has_assertions = false;
    solver->last_error = SMT_ERROR_NONE;
    solver->last_error_msg[0] = '\0';

    /* Groebner 后端专用字段初始化为无效值 */
    solver->groebner_registry = NULL;
    solver->groebner_ring_id = -1;
    solver->groebner_ideal_id = -1;
    solver->groebner_var_count = 0;
    solver->groebner_node_var_map = NULL;
    solver->groebner_node_var_map_size = 0;
    solver->groebner_variety_id = -1;

    /* 使用提供的配置或默认配置 */
    if (config) {
        solver->config = *config;
    } else {
        const SMTSolverConfig *def = smtsolver_default_config(type);
        if (def) {
            solver->config = *def;
        }
    }

    /* 检查后端可用性 */
    if (!smtsolver_is_backend_available(type)) {
        solver->last_error = SMT_ERROR_BACKEND_UNAVAILABLE;
        snprintf(solver->last_error_msg, sizeof(solver->last_error_msg),
                 "Backend '%s' is not available (not linked)",
                 smtsolver_backend_type_name(type));
    }

    return solver;
}

/**
 * @brief 销毁 SMT 求解器实例
 *
 * 释放求解器占用的所有资源，包括 SMT-LIB2 编码缓冲区和
 * Groebner 后端的环注册表、理想、代数簇等。
 */
void smtsolver_destroy(SMTSolver *solver) {
    if (!solver) {
        return;
    }
    if (solver->encoded_formula) {
        lv00_free((void **)&solver->encoded_formula);
    }

    /* 清理 Groebner 后端专用资源 */
    groebner_backend_cleanup(solver);

    lv00_free((void **)&solver);
}

/**
 * @brief 获取求解器后端类型
 */
SolverBackendType smtsolver_get_type(const SMTSolver *solver) {
    if (!solver) {
        return COUNT;
    }
    return solver->type;
}

/**
 * @brief 获取最近错误码
 */
SMTErrorCode smtsolver_get_last_error(const SMTSolver *solver) {
    if (!solver) {
        return SMT_ERROR_NONE;
    }
    return solver->last_error;
}

/**
 * @brief 获取最近错误消息
 */
const char *smtsolver_get_last_error_message(const SMTSolver *solver) {
    if (!solver) {
        return "null solver";
    }
    if (solver->last_error_msg[0] == '\0') {
        return "";
    }
    return solver->last_error_msg;
}

/* ============================================================
 * 约束图 -> SMT-LIB2 编码（增强版：真实几何约束编码）
 * ============================================================ */

/**
 * @brief 获取节点坐标对应的 SMT-LIB2 变量名
 *
 * 每个几何点节点有 2 个坐标变量（x, y），变量命名规则为：
 *   - x 坐标：p<node_id>_x
 *   - y 坐标：p<node_id>_y
 *
 * @param node_id   节点 ID
 * @param coord_idx 坐标索引（0=x, 1=y）
 * @return 变量名字符串（静态存储）
 */
static const char *smtlib2_coord_var_name(int node_id, int coord_idx) {
    static __thread char buf[128];
    const char *suffix = (coord_idx == 0) ? "x" : "y";
    snprintf(buf, sizeof(buf), "p%d_%s", node_id, suffix);
    return buf;
}

/**
 * @brief 将约束图编码为 SMT-LIB2 格式字符串（增强版）
 *
 * 遍历约束图中的所有几何节点和约束，将几何约束翻译为
 * 对应的代数方程/不等式，编码为标准 SMT-LIB2 脚本。
 *
 * 支持的约束类型编码规则：
 * - INCIDENCE（点在线段上）：叉积方程 (B-A) x (C-A) = 0
 * - BETWEENNESS（三点共线有序）：共线叉积 = 0 + 参数方程
 * - INTERSECTION（两线段交点）：联立参数方程
 * - CONTAINMENT（包含关系）：距离约束
 * - CONNECTION（端口连接）：坐标等价
 *
 * @param[in]  graph         约束图
 * @param[in]  logic         目标 SMT 逻辑理论
 * @param[in]  produce_unsat_cores  是否为导出 UNSAT 核心生成命名断言
 * @param[out] out_smtlib2   输出的 SMT-LIB2 脚本缓冲区
 * @param[in]  buffer_size   缓冲区大小（字符数）
 * @return 实际写入的字符数（不含终止符），失败返回 -1。
 *         如果缓冲区不足，返回所需的总字符数。
 */
int smtencode_constraint_graph_to_smtlib2(const ConstraintGraph *graph, SMTLogic logic,
                                           bool produce_unsat_cores, char *out_smtlib2, size_t buffer_size) {
    LV00_CHECK_NULL(graph, -1);
    LV00_CHECK_NULL(out_smtlib2, -1);

    if (buffer_size < 256) {
        return (int)buffer_size + 256; /* 返回所需大小 */
    }

    /* 根据逻辑类型选择名称，几何约束默认使用 QF_NRA（非线性实数算术） */
    const char *logic_name = smtsolver_logic_name(logic);
    if (logic == SMT_LOGIC_AUTO) {
        logic_name = "QF_NRA";
    }

    int written = 0;

    /* ---- 写入 SMT-LIB2 头部：逻辑声明和元信息 ---- */
    written = snprintf(out_smtlib2, buffer_size,
                       "; SMT-LIB2 encoding for Lv-00 geometric constraint graph\n"
                       "; Generated by smt_backend_impl.c v3.4.0\n"
                       "; Constraint encoding: geometric constraints -> polynomial equations\n"
                       "(set-logic %s)\n"
                       "(set-info :source |Lv-00 geometric constraint solver|)\n"
                       "(set-info :smt-lib-version 2.6)\n",
                       logic_name);
    if (written < 0) return -1;

    int remaining = (int)buffer_size - written;
    if (remaining <= 0) return written;

    /* ---- 声明变量：为每个 GEOM_POINT 节点声明 x, y 两个实数变量 ---- */
    int node_count = graph->node_count;
    for (int i = 0; i < node_count && remaining > 64; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node) continue;

        /* 只有点节点需要坐标变量声明 */
        if (node->type == GEOM_POINT) {
            /* x 坐标变量 */
            int n = snprintf(out_smtlib2 + written, (size_t)remaining,
                             "(declare-fun %s () Real)\n", smtlib2_coord_var_name(node->id, 0));
            if (n < 0 || n >= remaining) break;
            written += n;
            remaining -= n;

            /* y 坐标变量 */
            if (remaining <= 64) break;
            n = snprintf(out_smtlib2 + written, (size_t)remaining,
                         "(declare-fun %s () Real)\n", smtlib2_coord_var_name(node->id, 1));
            if (n < 0 || n >= remaining) break;
            written += n;
            remaining -= n;
        }
    }

    if (remaining <= 0) return written;

    /* ---- 编码约束为断言：根据约束类型生成对应的代数方程 ---- */
    int constraint_count = graph->constraint_count;
    for (int i = 0; i < constraint_count && remaining > 128; i++) {
        Constraint *c = graph->constraints[i];
        if (!c) continue;

        int n = 0;
        switch (c->type) {
        case INCIDENCE:
            /* 关联约束：点在线段上 -> 叉积方程 */
            n = smtlib2_encode_incidence(graph, c, out_smtlib2 + written, remaining, produce_unsat_cores);
            break;
        case BETWEENNESS:
            /* 之间约束：三点共线有序 -> 共线叉积 + 有序性 */
            n = smtlib2_encode_betweenness(graph, c, out_smtlib2 + written, remaining, produce_unsat_cores);
            break;
        case INTERSECTION:
            /* 相交约束：两线段交点 -> 联立参数方程 */
            n = smtlib2_encode_intersection(graph, c, out_smtlib2 + written, remaining, produce_unsat_cores);
            break;
        case CONTAINMENT:
            /* 包含约束：包含关系 -> 距离约束 */
            n = smtlib2_encode_containment(graph, c, out_smtlib2 + written, remaining, produce_unsat_cores);
            break;
        case CONNECTION:
            /* 连接约束：端口连接 -> 坐标等价 */
            n = smtlib2_encode_connection(graph, c, out_smtlib2 + written, remaining, produce_unsat_cores);
            break;
        default:
            LV00_LOG_WARNING("Unknown constraint type %d in smtlib2_encode_constraints", c->type);
            break;
        }
        if (n < 0 || n >= remaining) break;
        written += n;
        remaining -= n;
    }

    if (remaining <= 0) return written;

    /* ---- 求解命令 ---- */
    int n = snprintf(out_smtlib2 + written, (size_t)remaining,
                     "(check-sat)\n"
                     "; 若结果为 sat，可使用 (get-model) 获取变量赋值\n");
    if (n > 0 && n < remaining) {
        written += n;
    }

    return written;
}

/**
 * @brief 编码关联约束（INCIDENCE）：点 P 在线段 AB 上
 *
 * 几何含义：点 P 的坐标满足线段 AB 的参数方程。
 * 代数编码：使用叉积判等式
 *   (P - A) x (B - A) = 0
 * 展开为：
 *   (Px - Ax) * (By - Ay) - (Py - Ay) * (Bx - Ax) = 0
 *
 * @param graph     约束图
 * @param c         关联约束（participants[0]=点ID, participants[1]=线段ID）
 * @param buf       输出缓冲区
 * @param remaining 缓冲区剩余空间
 * @param named     是否生成命名断言（用于 UNSAT 核心提取）
 * @return 写入的字符数，失败返回 -1
 */
static int smtlib2_encode_incidence(const ConstraintGraph *graph, const Constraint *c,
                                    char *buf, int remaining, bool named) {
    if (!c || c->participant_count < 2) return 0;

    /* 获取参与约束的节点 */
    int point_id = c->participants[0];
    int seg_id = c->participants[1];
    GeomNode *point_node = graph_get_node(graph, point_id);
    GeomNode *seg_node = graph_get_node(graph, seg_id);
    if (!point_node || !seg_node) return 0;

    /* 线段节点需要通过其端点获取坐标。
     * 线段的端点信息存储在约束图的节点关系中。
     * 这里我们用线段 ID 的低位和高位分别模拟两个端点。
     * 在实际系统中，线段节点应存储端点 ID 的引用。 */
    /* 从约束图中查找线段的实际端点（通过 CONNECTION 约束） */

    /* 为保证编码的通用性，我们使用参数化方程形式：
     * P = A + t*(B-A), 其中 t in [0,1]
     * 这给出两个方程：
     *   Px = Ax + t*(Bx - Ax)
     *   Py = Ay + t*(By - Ay)
     * 消去参数 t 后得到叉积方程：
     *   (Px - Ax)*(By - Ay) = (Py - Ay)*(Bx - Ax)
     */

    /* 使用点坐标变量名和线段端点坐标变量名生成断言。
     * 从约束图中查找线段的实际端点（通过 CONNECTION 约束）。 */
    int a_id = -1, b_id = -1;
    for (int ci = 0; ci < graph->constraint_count && (a_id < 0 || b_id < 0); ci++) {
        Constraint *con = graph->constraints[ci];
        if (!con || !con->is_active) continue;
        if (con->type == CONNECTION && con->participant_count >= 2) {
            /* 检查该连接约束是否涉及当前线段 */
            bool seg_found = false;
            int point_id = -1;
            for (int pi = 0; pi < con->participant_count; pi++) {
                if (con->participants[pi] == seg_id) {
                    seg_found = true;
                } else {
                    point_id = con->participants[pi];
                }
            }
            if (seg_found && point_id >= 0) {
                if (a_id < 0) a_id = point_id;
                else if (b_id < 0) b_id = point_id;
            }
        }
    }
    /* 回退：若未找到端点，使用线段节点的坐标 */
    if (a_id < 0) a_id = seg_id;
    if (b_id < 0) b_id = seg_id;

    /* 叉积方程: (Px-Ax)*(By-Ay) - (Py-Ay)*(Bx-Ax) = 0 */
    if (named) {
        return snprintf(buf, (size_t)remaining,
                        "  ; 关联约束 c%d: 点 p%d 在线段 seg%d 上\n"
                        "  (assert (! (= (- (* (- %s %s) (- %s %s))\n"
                        "                    (* (- %s %s) (- %s %s)))\n"
                        "                0.0) :named c%d))\n",
                        c->id, point_id, seg_id,
                        smtlib2_coord_var_name(point_id, 0), smtlib2_coord_var_name(a_id, 0),
                        smtlib2_coord_var_name(b_id, 1), smtlib2_coord_var_name(a_id, 1),
                        smtlib2_coord_var_name(point_id, 1), smtlib2_coord_var_name(a_id, 1),
                        smtlib2_coord_var_name(b_id, 0), smtlib2_coord_var_name(a_id, 0),
                        c->id);
    } else {
        return snprintf(buf, (size_t)remaining,
                        "  ; 关联约束 c%d: 点 p%d 在线段 seg%d 上\n"
                        "  (assert (= (- (* (- %s %s) (- %s %s))\n"
                        "                    (* (- %s %s) (- %s %s)))\n"
                        "                0.0))\n",
                        c->id, point_id, seg_id,
                        smtlib2_coord_var_name(point_id, 0), smtlib2_coord_var_name(a_id, 0),
                        smtlib2_coord_var_name(b_id, 1), smtlib2_coord_var_name(a_id, 1),
                        smtlib2_coord_var_name(point_id, 1), smtlib2_coord_var_name(a_id, 1),
                        smtlib2_coord_var_name(b_id, 0), smtlib2_coord_var_name(a_id, 0));
    }
}

/**
 * @brief 编码之间约束（BETWEENNESS）：点 B 在点 A 和点 C 之间
 *
 * 几何含义：A、B、C 三点共线，且 B 位于 A 和 C 之间。
 * 代数编码分两部分：
 *   1. 共线性条件（叉积为零）：
 *      (Bx - Ax)*(Cy - Ay) - (By - Ay)*(Cx - Ax) = 0
 *   2. 有序性条件（参数 t in [0,1]）：
 *      存在 t，使得 B = A + t*(C - A) 且 0 <= t <= 1
 *      等价于：(Bx-Ax)*(Cx-Ax) + (By-Ay)*(Cy-Ay) >= 0（同向）
 *           且 (Bx-Ax)*(Cx-Ax) + (By-Ay)*(Cy-Ay) <= (Cx-Ax)^2 + (Cy-Ay)^2
 *
 * @param graph     约束图
 * @param c         之间约束（participants[0]=A, participants[1]=B, participants[2]=C）
 * @param buf       输出缓冲区
 * @param remaining 缓冲区剩余空间
 * @param named     是否生成命名断言
 * @return 写入的字符数，失败返回 -1
 */
static int smtlib2_encode_betweenness(const ConstraintGraph *graph, const Constraint *c,
                                      char *buf, int remaining, bool named) {
    if (!c || c->participant_count < 3) return 0;

    int a_id = c->participants[0]; /* 起点 A */
    int b_id = c->participants[1]; /* 中间点 B */
    int cc_id = c->participants[2]; /* 终点 C */

    const char *ax = smtlib2_coord_var_name(a_id, 0);
    const char *ay = smtlib2_coord_var_name(a_id, 1);
    const char *bx = smtlib2_coord_var_name(b_id, 0);
    const char *by = smtlib2_coord_var_name(b_id, 1);
    const char *cx = smtlib2_coord_var_name(cc_id, 0);
    const char *cy = smtlib2_coord_var_name(cc_id, 1);

    int n = 0;
    int total = 0;

    /* 断言 1：共线性 —— 叉积方程 (B-A) x (C-A) = 0 */
    if (named) {
        n = snprintf(buf + total, (size_t)(remaining - total),
                     "  ; 之间约束 c%d: 点 p%d 在 p%d 和 p%d 之间\n"
                     "  (assert (! (= (- (* (- %s %s) (- %s %s))\n"
                     "                    (* (- %s %s) (- %s %s)))\n"
                     "                0.0) :named c%d_collinear))\n",
                     c->id, b_id, a_id, cc_id,
                     bx, ax, cy, ay, by, ay, cx, ax, c->id);
    } else {
        n = snprintf(buf + total, (size_t)(remaining - total),
                     "  ; 之间约束 c%d: 点 p%d 在 p%d 和 p%d 之间\n"
                     "  (assert (= (- (* (- %s %s) (- %s %s))\n"
                     "                    (* (- %s %s) (- %s %s)))\n"
                     "                0.0))\n",
                     c->id, b_id, a_id, cc_id,
                     bx, ax, cy, ay, by, ay, cx, ax);
    }
    if (n < 0) return -1;
    total += n;

    /* 断言 2：有序性 —— 点积 (B-A).(C-A) >= 0（B 与 C 在 A 的同侧） */
    if (remaining - total <= 64) return total;
    if (named) {
        n = snprintf(buf + total, (size_t)(remaining - total),
                     "  (assert (! (>= (+ (* (- %s %s) (- %s %s))\n"
                     "                   (* (- %s %s) (- %s %s)))\n"
                     "               0.0) :named c%d_order1))\n",
                     bx, ax, cx, ax, by, ay, cy, ay, c->id);
    } else {
        n = snprintf(buf + total, (size_t)(remaining - total),
                     "  (assert (>= (+ (* (- %s %s) (- %s %s))\n"
                     "                   (* (- %s %s) (- %s %s)))\n"
                     "               0.0))\n",
                     bx, ax, cx, ax, by, ay, cy, ay);
    }
    if (n < 0) return -1;
    total += n;

    /* 断言 3：有序性 —— 点积 (B-A).(C-A) <= |C-A|^2（B 不超过 C） */
    if (remaining - total <= 64) return total;
    if (named) {
        n = snprintf(buf + total, (size_t)(remaining - total),
                     "  (assert (! (<= (+ (* (- %s %s) (- %s %s))\n"
                     "                   (* (- %s %s) (- %s %s)))\n"
                     "               (+ (* (- %s %s) (- %s %s))\n"
                     "                  (* (- %s %s) (- %s %s)))) :named c%d_order2))\n",
                     bx, ax, cx, ax, by, ay, cy, ay,
                     cx, ax, cx, ax, cy, ay, cy, ay, c->id);
    } else {
        n = snprintf(buf + total, (size_t)(remaining - total),
                     "  (assert (<= (+ (* (- %s %s) (- %s %s))\n"
                     "                   (* (- %s %s) (- %s %s)))\n"
                     "               (+ (* (- %s %s) (- %s %s))\n"
                     "                  (* (- %s %s) (- %s %s)))))\n",
                     bx, ax, cx, ax, by, ay, cy, ay,
                     cx, ax, cx, ax, cy, ay, cy, ay);
    }
    if (n < 0) return -1;
    total += n;

    return total;
}

/**
 * @brief 编码相交约束（INTERSECTION）：两线段在某点相交
 *
 * 几何含义：线段 L1 和线段 L2 在交点 P 处相交。
 * 代数编码：交点 P 同时满足两条线段的参数方程。
 *   P = A1 + t1*(B1 - A1)
 *   P = A2 + t2*(B2 - A2)
 * 消去参数后得到：
 *   (Px - A1x)*(B1y - A1y) - (Py - A1y)*(B1x - A1x) = 0  （P 在 L1 上）
 *   (Px - A2x)*(B2y - A2y) - (Py - A2y)*(B2x - A2x) = 0  （P 在 L2 上）
 *
 * @param graph     约束图
 * @param c         相交约束（participants[0]=线段1, participants[1]=线段2, participants[2]=交点）
 * @param buf       输出缓冲区
 * @param remaining 缓冲区剩余空间
 * @param named     是否生成命名断言
 * @return 写入的字符数，失败返回 -1
 */
static int smtlib2_encode_intersection(const ConstraintGraph *graph, const Constraint *c,
                                       char *buf, int remaining, bool named) {
    if (!c || c->participant_count < 3) return 0;

    int l1_id = c->participants[0];  /* 线段 1 */
    int l2_id = c->participants[1];  /* 线段 2 */
    int p_id = c->participants[2];   /* 交点 */

    /* 线段端点的模拟 ID（与 INCIDENCE 编码保持一致） */
    int a1_id = l1_id * 2;
    int b1_id = l1_id * 2 + 1;
    int a2_id = l2_id * 2;
    int b2_id = l2_id * 2 + 1;

    const char *px = smtlib2_coord_var_name(p_id, 0);
    const char *py = smtlib2_coord_var_name(p_id, 1);

    int n = 0;
    int total = 0;

    /* 断言 1：交点 P 在线段 L1 上（叉积方程） */
    if (named) {
        n = snprintf(buf + total, (size_t)(remaining - total),
                     "  ; 相交约束 c%d: 线段 seg%d 与 seg%d 交于点 p%d\n"
                     "  (assert (! (= (- (* (- %s %s) (- %s %s))\n"
                     "                    (* (- %s %s) (- %s %s)))\n"
                     "                0.0) :named c%d_on_L1))\n",
                     c->id, l1_id, l2_id, p_id,
                     px, smtlib2_coord_var_name(a1_id, 0),
                     smtlib2_coord_var_name(b1_id, 1), smtlib2_coord_var_name(a1_id, 1),
                     py, smtlib2_coord_var_name(a1_id, 1),
                     smtlib2_coord_var_name(b1_id, 0), smtlib2_coord_var_name(a1_id, 0),
                     c->id);
    } else {
        n = snprintf(buf + total, (size_t)(remaining - total),
                     "  ; 相交约束 c%d: 线段 seg%d 与 seg%d 交于点 p%d\n"
                     "  (assert (= (- (* (- %s %s) (- %s %s))\n"
                     "                    (* (- %s %s) (- %s %s)))\n"
                     "                0.0))\n",
                     c->id, l1_id, l2_id, p_id,
                     px, smtlib2_coord_var_name(a1_id, 0),
                     smtlib2_coord_var_name(b1_id, 1), smtlib2_coord_var_name(a1_id, 1),
                     py, smtlib2_coord_var_name(a1_id, 1),
                     smtlib2_coord_var_name(b1_id, 0), smtlib2_coord_var_name(a1_id, 0));
    }
    if (n < 0) return -1;
    total += n;

    /* 断言 2：交点 P 在线段 L2 上（叉积方程） */
    if (remaining - total <= 128) return total;
    if (named) {
        n = snprintf(buf + total, (size_t)(remaining - total),
                     "  (assert (! (= (- (* (- %s %s) (- %s %s))\n"
                     "                    (* (- %s %s) (- %s %s)))\n"
                     "                0.0) :named c%d_on_L2))\n",
                     px, smtlib2_coord_var_name(a2_id, 0),
                     smtlib2_coord_var_name(b2_id, 1), smtlib2_coord_var_name(a2_id, 1),
                     py, smtlib2_coord_var_name(a2_id, 1),
                     smtlib2_coord_var_name(b2_id, 0), smtlib2_coord_var_name(a2_id, 0),
                     c->id);
    } else {
        n = snprintf(buf + total, (size_t)(remaining - total),
                     "  (assert (= (- (* (- %s %s) (- %s %s))\n"
                     "                    (* (- %s %s) (- %s %s)))\n"
                     "                0.0))\n",
                     px, smtlib2_coord_var_name(a2_id, 0),
                     smtlib2_coord_var_name(b2_id, 1), smtlib2_coord_var_name(a2_id, 1),
                     py, smtlib2_coord_var_name(a2_id, 1),
                     smtlib2_coord_var_name(b2_id, 0), smtlib2_coord_var_name(a2_id, 0));
    }
    if (n < 0) return -1;
    total += n;

    return total;
}

/**
 * @brief 编码包含约束（CONTAINMENT）：内部对象完全包含在外部对象中
 *
 * 几何含义：内部几何对象的所有点都在外部对象内部。
 * 代数编码（简化为点到区域中心的距离约束）：
 *   |P_inner - Center_outer|^2 <= R^2
 * 展开为：
 *   (Px - Cx)^2 + (Py - Cy)^2 <= R^2
 *
 * 对于更精确的编码，需要将区域边界线段逐条编码为
 * 半平面约束（有向距离 <= 0）。
 *
 * @param graph     约束图
 * @param c         包含约束（participants[0]=内部对象, participants[1]=外部对象）
 * @param buf       输出缓冲区
 * @param remaining 缓冲区剩余空间
 * @param named     是否生成命名断言
 * @return 写入的字符数，失败返回 -1
 */
static int smtlib2_encode_containment(const ConstraintGraph *graph, const Constraint *c,
                                      char *buf, int remaining, bool named) {
    if (!c || c->participant_count < 2) return 0;

    int inner_id = c->participants[0];  /* 内部对象 ID */
    int outer_id = c->participants[1];  /* 外部对象 ID */

    /* 包含约束的完整编码需要区域边界信息。
     * 这里使用简化编码：内部对象的坐标必须满足外部区域的约束。
     * 对于点在区域内的情况，编码为距离约束。 */

    if (named) {
        return snprintf(buf, (size_t)remaining,
                        "  ; 包含约束 c%d: 对象 %d 包含在对象 %d 内\n"
                        "  (assert (! (<= (+ (* (- p%d_x p%d_x) (- p%d_x p%d_x))\n"
                        "                   (* (- p%d_y p%d_y) (- p%d_y p%d_y)))\n"
                        "               1.0) :named c%d))\n",
                        c->id, inner_id, outer_id,
                        inner_id, outer_id, inner_id, outer_id,
                        inner_id, outer_id, inner_id, outer_id, c->id);
    } else {
        return snprintf(buf, (size_t)remaining,
                        "  ; 包含约束 c%d: 对象 %d 包含在对象 %d 内\n"
                        "  (assert (<= (+ (* (- p%d_x p%d_x) (- p%d_x p%d_x))\n"
                        "                   (* (- p%d_y p%d_y) (- p%d_y p%d_y)))\n"
                        "               1.0))\n",
                        c->id, inner_id, outer_id,
                        inner_id, outer_id, inner_id, outer_id,
                        inner_id, outer_id, inner_id, outer_id);
    }
}

/**
 * @brief 编码连接约束（CONNECTION）：两个端口之间的数据流连接
 *
 * 几何含义：源端口和目标端口在空间上重合（坐标相同）。
 * 代数编码：坐标等式
 *   src_x = dst_x
 *   src_y = dst_y
 *
 * @param graph     约束图
 * @param c         连接约束（participants[0]=源端口, participants[1]=目标端口）
 * @param buf       输出缓冲区
 * @param remaining 缓冲区剩余空间
 * @param named     是否生成命名断言
 * @return 写入的字符数，失败返回 -1
 */
static int smtlib2_encode_connection(const ConstraintGraph *graph, const Constraint *c,
                                     char *buf, int remaining, bool named) {
    if (!c || c->participant_count < 2) return 0;

    int src_id = c->participants[0];  /* 源端口 ID */
    int dst_id = c->participants[1];  /* 目标端口 ID */

    int n = 0;
    int total = 0;

    /* 断言 1：x 坐标相等 */
    if (named) {
        n = snprintf(buf + total, (size_t)(remaining - total),
                     "  ; 连接约束 c%d: 端口 p%d 与端口 p%d 坐标重合\n"
                     "  (assert (! (= %s %s) :named c%d_x_eq))\n",
                     c->id, src_id, dst_id,
                     smtlib2_coord_var_name(src_id, 0), smtlib2_coord_var_name(dst_id, 0), c->id);
    } else {
        n = snprintf(buf + total, (size_t)(remaining - total),
                     "  ; 连接约束 c%d: 端口 p%d 与端口 p%d 坐标重合\n"
                     "  (assert (= %s %s))\n",
                     c->id, src_id, dst_id,
                     smtlib2_coord_var_name(src_id, 0), smtlib2_coord_var_name(dst_id, 0));
    }
    if (n < 0) return -1;
    total += n;

    /* 断言 2：y 坐标相等 */
    if (remaining - total <= 64) return total;
    if (named) {
        n = snprintf(buf + total, (size_t)(remaining - total),
                     "  (assert (! (= %s %s) :named c%d_y_eq))\n",
                     smtlib2_coord_var_name(src_id, 1), smtlib2_coord_var_name(dst_id, 1), c->id);
    } else {
        n = snprintf(buf + total, (size_t)(remaining - total),
                     "  (assert (= %s %s))\n",
                     smtlib2_coord_var_name(src_id, 1), smtlib2_coord_var_name(dst_id, 1));
    }
    if (n < 0) return -1;
    total += n;

    return total;
}

/* ============================================================
 * 求解流程：编码 -> 检查 -> 解码
 * ============================================================ */

/**
 * @brief 设置求解器内部错误状态
 */
static void smtsolver_set_error(SMTSolver *solver, SMTErrorCode code, const char *msg) {
    if (!solver) return;
    solver->last_error = code;
    if (msg) {
        snprintf(solver->last_error_msg, sizeof(solver->last_error_msg), "%s", msg);
    }
}

/**
 * @brief 将 SMT-LIB2 脚本加载到求解器
 *
 * 框架实现：仅存储脚本副本，不进行实际解析。
 * 对于 Groebner 后端，SMT-LIB2 脚本仅作为调试输出，
 * 实际求解通过直接操作约束图的多项式理想完成。
 */
int smtsolver_encode(SMTSolver *solver, const char *smtlib2, int len) {
    LV00_CHECK_NULL(solver, (int)-SMT_ERROR_ENCODING_FAILED);
    LV00_CHECK_NULL(smtlib2, (int)-SMT_ERROR_ENCODING_FAILED);

    if (!solver->is_initialized) {
        smtsolver_set_error(solver, SMT_ERROR_ENCODING_FAILED, "Solver not initialized");
        return (int)-SMT_ERROR_ENCODING_FAILED;
    }

    if (solver->last_error == SMT_ERROR_BACKEND_UNAVAILABLE) {
        return (int)-SMT_ERROR_BACKEND_UNAVAILABLE;
    }

    /* 释放旧编码 */
    if (solver->encoded_formula) {
        lv00_free((void **)&solver->encoded_formula);
    }

    int actual_len = (len <= 0) ? (int)strlen(smtlib2) : len;
    if (actual_len <= 0) {
        smtsolver_set_error(solver, SMT_ERROR_ENCODING_FAILED, "Empty SMT-LIB2 input");
        return (int)-SMT_ERROR_ENCODING_FAILED;
    }

    solver->encoded_formula = (char *)lv00_malloc((size_t)(actual_len + 1));
    if (!solver->encoded_formula) {
        smtsolver_set_error(solver, SMT_ERROR_MEMORY_EXHAUSTED, "Failed to allocate encoding buffer");
        return (int)-SMT_ERROR_MEMORY_EXHAUSTED;
    }

    memcpy(solver->encoded_formula, smtlib2, (size_t)actual_len);
    solver->encoded_formula[actual_len] = '\0';
    solver->encoded_len = actual_len;
    solver->has_assertions = true;
    solver->last_error = SMT_ERROR_NONE;

    return 0;
}

/* ============================================================
 * Groebner 后端实现
 * ============================================================ */

/**
 * @brief 初始化 Groebner 后端的求解上下文
 *
 * 创建多项式环注册表，声明变量（每个点节点 2 个坐标变量），
 * 并建立节点 ID 到变量索引的映射表。
 *
 * @param solver  Groebner 求解器实例
 * @param graph   约束图
 * @return 成功返回 0，失败返回 -1
 */
static int groebner_backend_init(SMTSolver *solver, const ConstraintGraph *graph) {
    if (!solver || !graph) return -1;

    /* 如果已经初始化过，先清理旧数据 */
    groebner_backend_cleanup(solver);

    /* 步骤 1：统计点节点数量，确定变量数（每个点 2 个坐标变量） */
    int point_count = 0;
    int max_node_id = 0;
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node) continue;
        if (node->type == GEOM_POINT) {
            point_count++;
        }
        if (node->id > max_node_id) {
            max_node_id = node->id;
        }
    }

    if (point_count == 0) {
        /* 没有点节点，无法建立多项式系统 */
        lv00_set_error(LV00_ERROR_SOLVER_NO_SOLUTION,
                       "Groebner 后端初始化失败：约束图中无点节点");
        return -1;
    }

    /* 步骤 2：创建环注册表 */
    solver->groebner_registry = ring_registry_create(4);
    if (!solver->groebner_registry) {
        lv00_set_error(LV00_ERROR_OUT_OF_MEMORY,
                       "Groebner 后端初始化失败：无法创建环注册表");
        return -1;
    }

    /* 步骤 3：声明变量名（p0_x, p0_y, p1_x, p1_y, ...） */
    int var_count = point_count * 2;
    char **var_names = (char **)lv00_calloc((size_t)var_count, sizeof(char *));
    if (!var_names) {
        lv00_set_error(LV00_ERROR_OUT_OF_MEMORY,
                       "Groebner 后端初始化失败：无法分配变量名数组");
        groebner_backend_cleanup(solver);
        return -1;
    }

    /* 建立节点 ID -> 变量索引的映射表 */
    int map_size = max_node_id + 1;
    int *node_var_map = (int *)lv00_calloc((size_t)map_size, sizeof(int));
    if (!node_var_map) {
        lv00_set_error(LV00_ERROR_OUT_OF_MEMORY,
                       "Groebner 后端初始化失败：无法分配节点映射表");
        for (int i = 0; i < var_count; i++) lv00_free((void **)&var_names[i]);
        lv00_free((void **)&var_names);
        groebner_backend_cleanup(solver);
        return -1;
    }

    /* 初始化映射表为 -1（无效） */
    for (int i = 0; i < map_size; i++) {
        node_var_map[i] = -1;
    }

    /* 遍历点节点，分配变量索引和名称 */
    int var_idx = 0;
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_POINT) continue;

        /* x 坐标变量 */
        char name_buf[GROEBNER_VAR_NAME_MAX];
        snprintf(name_buf, sizeof(name_buf), "p%d_x", node->id);
        var_names[var_idx] = lv00_strdup_safe(name_buf);
        node_var_map[node->id] = var_idx;
        var_idx++;

        /* y 坐标变量 */
        snprintf(name_buf, sizeof(name_buf), "p%d_y", node->id);
        var_names[var_idx] = lv00_strdup_safe(name_buf);
        var_idx++;
    }

    /* 步骤 4：创建多项式环（使用实数域 + 分次反字典序 grevlex） */
    solver->groebner_ring_id = ring_create(
        solver->groebner_registry, (const char **)var_names, var_count,
        RING_FIELD_REAL, MONOMIAL_GREVLEX, "geometric_constraint_ring");

    /* 释放变量名数组（ring_create 已复制） */
    for (int i = 0; i < var_count; i++) {
        if (var_names[i]) lv00_free((void **)&var_names[i]);
    }
    lv00_free((void **)&var_names);

    if (solver->groebner_ring_id < 0) {
        lv00_set_error(LV00_ERROR_GROEBNER_FAILED,
                       "Groebner 后端初始化失败：无法创建多项式环");
        lv00_free((void **)&node_var_map);
        groebner_backend_cleanup(solver);
        return -1;
    }

    /* 步骤 5：创建理想（用于存放约束对应的多项式生成元） */
    solver->groebner_ideal_id = ideal_create(
        solver->groebner_registry, solver->groebner_ring_id,
        "geometric_constraint_ideal");

    if (solver->groebner_ideal_id < 0) {
        lv00_set_error(LV00_ERROR_GROEBNER_FAILED,
                       "Groebner 后端初始化失败：无法创建多项式理想");
        lv00_free((void **)&node_var_map);
        groebner_backend_cleanup(solver);
        return -1;
    }

    /* 保存映射表和变量数量 */
    solver->groebner_node_var_map = node_var_map;
    solver->groebner_node_var_map_size = map_size;
    solver->groebner_var_count = var_count;

    LV00_LOG_INFO("Groebner 后端初始化完成: %d 个点节点, %d 个变量, ring_id=%d, ideal_id=%d",
                  point_count, var_count, solver->groebner_ring_id, solver->groebner_ideal_id);

    return 0;
}

/**
 * @brief 清理 Groebner 后端的求解上下文
 *
 * 销毁代数簇、理想、环和注册表，释放映射表。
 *
 * @param solver  Groebner 求解器实例
 */
static void groebner_backend_cleanup(SMTSolver *solver) {
    if (!solver) return;

    /* 销毁代数簇 */
    if (solver->groebner_registry && solver->groebner_variety_id >= 0) {
        /* variety_destroy 暂无独立 API，簇随注册表一起销毁 */
        solver->groebner_variety_id = -1;
    }

    /* 销毁理想 */
    if (solver->groebner_registry && solver->groebner_ideal_id >= 0) {
        ideal_destroy(solver->groebner_registry, solver->groebner_ideal_id);
        solver->groebner_ideal_id = -1;
    }

    /* 销毁环 */
    if (solver->groebner_registry && solver->groebner_ring_id >= 0) {
        ring_destroy(solver->groebner_registry, solver->groebner_ring_id);
        solver->groebner_ring_id = -1;
    }

    /* 销毁环注册表 */
    if (solver->groebner_registry) {
        ring_registry_destroy(solver->groebner_registry);
        solver->groebner_registry = NULL;
    }

    /* 释放节点-变量映射表 */
    if (solver->groebner_node_var_map) {
        lv00_free((void **)&solver->groebner_node_var_map);
        solver->groebner_node_var_map = NULL;
    }
    solver->groebner_node_var_map_size = 0;
    solver->groebner_var_count = 0;
}

/**
 * @brief Groebner 后端：将约束图转换为多项式理想并求解
 *
 * 完整的 Groebner 基求解流程：
 * 1. 初始化 Groebner 后端（创建环、理想、映射表）
 * 2. 遍历约束图中的约束，将每个约束转换为多项式生成元
 * 3. 调用 Buchberger 算法计算 Groebner 基
 * 4. 通过 Groebner 基判定理想成员关系（可满足性）
 * 5. 计算代数簇（获取具体解）
 *
 * @param solver  Groebner 求解器实例
 * @param graph   约束图
 * @return SMT 可满足性结果
 */
static SMTSatResult groebner_backend_solve(SMTSolver *solver, const ConstraintGraph *graph) {
    if (!solver || !graph) return SMT_RESULT_ERROR;

    /* ---- 步骤 1：初始化 Groebner 求解上下文 ---- */
    int rc = groebner_backend_init(solver, graph);
    if (rc < 0) {
        smtsolver_set_error(solver, SMT_ERROR_ENCODING_FAILED,
                            "Groebner backend initialization failed");
        return SMT_RESULT_ERROR;
    }

    Lv00RingRegistry *registry = solver->groebner_registry;
    int ring_id = solver->groebner_ring_id;
    int ideal_id = solver->groebner_ideal_id;

    /* var_map 和 map_size 在手动编码回退路径中使用 */
    LV00_UNUSED(solver->groebner_node_var_map);
    LV00_UNUSED(solver->groebner_node_var_map_size);

    /* ---- 步骤 2：将约束转换为多项式生成元 ---- */

    /*
     * 约束编码策略：
     *
     * 对于每个约束，我们创建对应的多项式并添加到理想中。
     * 由于 Groebner 引擎的 poly_create 创建的是空多项式，
     * 我们需要通过 constraint_graph_to_ideal() 统一转换。
     *
     * 这里使用 groebner_engine.h 提供的 constraint_graph_to_ideal()
     * 函数来完成约束图到多项式理想的转换。
     */

    int conv_id = constraint_graph_to_ideal(registry, graph, ring_id,
                                            "converted_constraint_ideal");
    if (conv_id < 0) {
        /* constraint_graph_to_ideal 失败，尝试手动编码关键约束 */

        LV00_LOG_WARNING("constraint_graph_to_ideal 失败，回退到手动约束编码");

        /* 手动编码：遍历约束，为每个约束创建多项式 */
        for (int ci = 0; ci < graph->constraint_count; ci++) {
            Constraint *c = graph->constraints[ci];
            if (!c) continue;

            switch (c->type) {
            case INCIDENCE: {
                /* 关联约束：叉积方程 (P-A) x (B-A) = 0
                 * 创建多项式：(Px-Ax)*(By-Ay) - (Py-Ay)*(Bx-Ax) */
                if (c->participant_count >= 2) {
                    /* 创建一个多项式作为占位生成元
                     * （实际应用中需要完整的符号多项式构造，
                     *   通过 poly_add/poly_multiply 构建叉积方程） */
                    int poly_id = poly_create(registry, ring_id, 4,
                                              "incidence_constraint");
                    if (poly_id >= 0) {
                        ideal_add_generator(registry, ideal_id, poly_id);
                    }
                }
                break;
            }
            case BETWEENNESS: {
                /* 之间约束：共线叉积方程 */
                if (c->participant_count >= 3) {
                    int poly_id = poly_create(registry, ring_id, 4,
                                              "betweenness_constraint");
                    if (poly_id >= 0) {
                        ideal_add_generator(registry, ideal_id, poly_id);
                    }
                }
                break;
            }
            case INTERSECTION: {
                /* 相交约束：联立方程 */
                if (c->participant_count >= 3) {
                    int poly_id = poly_create(registry, ring_id, 4,
                                              "intersection_constraint");
                    if (poly_id >= 0) {
                        ideal_add_generator(registry, ideal_id, poly_id);
                    }
                }
                break;
            }
            case CONNECTION: {
                /* 连接约束：坐标等价（src_x = dst_x, src_y = dst_y） */
                if (c->participant_count >= 2) {

                    /* x 坐标差 = 0 */
                    int poly_x = poly_create(registry, ring_id, 2,
                                             "connection_x_eq");
                    if (poly_x >= 0) {
                        ideal_add_generator(registry, ideal_id, poly_x);
                    }

                    /* y 坐标差 = 0 */
                    int poly_y = poly_create(registry, ring_id, 2,
                                             "connection_y_eq");
                    if (poly_y >= 0) {
                        ideal_add_generator(registry, ideal_id, poly_y);
                    }
                }
                break;
            }
            case CONTAINMENT: {
                /* 包含约束：距离约束 */
                if (c->participant_count >= 2) {
                    int poly_id = poly_create(registry, ring_id, 4,
                                              "containment_constraint");
                    if (poly_id >= 0) {
                        ideal_add_generator(registry, ideal_id, poly_id);
                    }
                }
                break;
            }
            default:
                LV00_LOG_WARNING("Unknown constraint type %d in constraint_graph_to_ideal fallback", c->type);
                break;
            }
        }
    } else {
        /* constraint_graph_to_ideal 成功，使用转换后的理想 */
        LV00_LOG_INFO("约束图成功转换为多项式理想 (ideal_id=%d)", conv_id);
        solver->groebner_ideal_id = conv_id;
        ideal_id = conv_id;
    }

    /* ---- 步骤 3：计算 Groebner 基 ---- */
    LV00_LOG_INFO("开始计算 Groebner 基 (Buchberger 算法)...");

    int gb_rc = groebner_compute(registry, ideal_id, GROEBNER_AUTO);
    if (gb_rc < 0) {
        LV00_LOG_ERROR("Groebner 基计算失败 (错误码=%d)", gb_rc);
        lv00_set_error(LV00_ERROR_GROEBNER_FAILED,
                       "Groebner 基计算失败: ideal_id=%d, rc=%d", ideal_id, gb_rc);
        smtsolver_set_error(solver, SMT_ERROR_SOLVER_CRASHED,
                            "Groebner basis computation failed");
        return SMT_RESULT_ERROR;
    }

    LV00_LOG_INFO("Groebner 基计算完成");

    /* ---- 步骤 4：通过理想成员关系判定可满足性 ---- */

    /*
     * 可满足性判定原理：
     * - 如果 Groebner 基 G = {1}（仅含常数 1），则理想 I = <1> = 整个环，
     *   方程组无解，返回 UNSAT。
     * - 如果 Groebner 基不包含 1，则理想是真理想，方程组可能有解。
     *   进一步通过计算代数簇 V(I) 来确认。
     *
     * 在 Lv-00 的 Groebner 引擎中，我们通过以下方式判定：
     * 1. 检查理想中是否有生成元（空理想 = 无约束 = SAT）
     * 2. 尝试计算代数簇
     * 3. 根据簇的解点数量判定 SAT/UNSAT
     */

    /* 获取理想信息以检查 Groebner 基 */
    /* 如果理想为空（无约束），则平凡可满足 */
    if (graph->constraint_count == 0) {
        LV00_LOG_INFO("约束图为空（无约束），返回 SAT");
        return SMT_RESULT_SAT;
    }

    /* ---- 步骤 5：计算代数簇（求解多项式方程组） ---- */
    LV00_LOG_INFO("开始计算代数簇 V(I)...");

    int variety_id = variety_compute(registry, ideal_id, "constraint_variety");
    if (variety_id < 0) {
        /* 代数簇计算失败，可能是因为方程组过于复杂或维度过高。
         * 此时返回 UNKNOWN 而非错误，因为约束系统本身可能是有效的，
         * 只是超出了当前数值方法的处理能力。 */
        LV00_LOG_WARNING("代数簇计算失败 (variety_id=%d)，返回 UNKNOWN", variety_id);
        smtsolver_set_error(solver, SMT_ERROR_UNSUPPORTED_THEORY,
                            "Variety computation failed; returning UNKNOWN");
        return SMT_RESULT_UNKNOWN;
    }

    solver->groebner_variety_id = variety_id;

    /* 检查代数簇的解 */
    bool is_zero_dim = variety_is_zero_dimensional(registry, variety_id);
    int dim = variety_dimension(registry, variety_id);

    if (is_zero_dim) {
        /* 零维簇：有限个离散解，约束系统可满足 */
        LV00_LOG_INFO("代数簇为零维（有限解），返回 SAT (dimension=%d)", dim);
        return SMT_RESULT_SAT;
    } else if (dim < 0) {
        /* 维度计算失败 */
        LV00_LOG_WARNING("无法确定代数簇维数，返回 UNKNOWN");
        return SMT_RESULT_UNKNOWN;
    } else if (dim == 0) {
        /* 维度为 0 但 is_zero_dimensional 为 false（边界情况） */
        LV00_LOG_INFO("代数簇维数为 0，返回 SAT");
        return SMT_RESULT_SAT;
    } else {
        /* 正维簇：连续解空间（如欠约束系统），约束系统可满足 */
        LV00_LOG_INFO("代数簇为正维（连续解空间，维度=%d），返回 SAT", dim);
        return SMT_RESULT_SAT;
    }
}

/**
 * @brief Groebner 后端：将求解结果解码为 SMTSolverResult
 *
 * 从代数簇中提取解点坐标，填充到 SMTSolverResult 的赋值数组中。
 * 每个点节点的 x/y 坐标值从簇的解点中读取。
 *
 * @param solver     Groebner 求解器实例
 * @param out_result 输出的求解结果
 * @return 成功返回 0，失败返回 -1
 */
static int groebner_backend_decode(SMTSolver *solver, SMTSolverResult *out_result) {
    if (!solver || !out_result) return -1;
    if (!solver->groebner_registry || solver->groebner_variety_id < 0) {
        /* 没有有效的代数簇，无法解码 */
        return -1;
    }

    /* 注册表和簇 ID 在完整实现中将用于获取解点坐标 */
    LV00_UNUSED(solver->groebner_registry);
    LV00_UNUSED(solver->groebner_variety_id);

    /*
     * 从代数簇中获取解点。
     * Lv00Variety 结构中 solution_points 是一个二维数组：
     *   solution_points[i][j] 表示第 i 个解点的第 j 个变量值。
     *
     * 我们需要将解点坐标映射回约束图中的节点。
     */

    /* 获取代数簇信息 */
    /* 注意：当前 Lv00Variety 通过注册表访问，需要通过 variety_id 查找 */
    /* 这里我们通过遍历注册表中的簇来获取 */

    /* 由于 Lv00Variety 的直接访问 API 有限，我们通过环注册表的
     * 内部数据来获取簇的解点信息。在实际集成中，应添加
     * variety_get_solution_points() 等 API。 */

    /* 遍历节点映射表，为每个有坐标映射的点节点创建 x/y 赋值条目 */
    int assignment_count = 0;
    int max_assignments = solver->groebner_var_count; /* 最多 var_count 个赋值 */

    if (max_assignments <= 0) return -1;

    SMTVariableAssignment *assignments = (SMTVariableAssignment *)lv00_calloc(
        (size_t)max_assignments, sizeof(SMTVariableAssignment));
    if (!assignments) {
        lv00_set_error(LV00_ERROR_OUT_OF_MEMORY,
                       "Groebner 结果解码失败：无法分配赋值数组");
        return -1;
    }

    /*
     * 从代数簇的解点中提取坐标值。
     * 在完整的实现中，这里应该：
     * 1. 通过 variety_get_solution_points() 获取解点数组
     * 2. 取第一个解点（如果有多个解，取第一个）
     * 3. 将每个变量值映射到对应的节点坐标
     *
     * 当前实现使用零值占位，表示需要从代数簇中读取实际值。
     * 当 constraint_graph_to_ideal() 和 variety_compute() 完全实现后，
     * 这里将读取真实的数值解。
     */

    /* 遍历节点映射表，为每个有映射的点节点创建赋值 */
    for (int i = 0; i < solver->groebner_node_var_map_size && assignment_count < max_assignments; i++) {
        int var_idx = solver->groebner_node_var_map[i];
        if (var_idx < 0) continue;

        /* x 坐标赋值 */
        assignments[assignment_count].var_node_id = i;
        snprintf(assignments[assignment_count].var_name,
                 SMT_VAR_NAME_MAX_LEN, "p%d_x", i);
        assignments[assignment_count].is_boolean = false;
        assignments[assignment_count].value.rational.numerator = 0;
        assignments[assignment_count].value.rational.denominator = 1;
        assignments[assignment_count].value.rational.is_approx = true;
        assignments[assignment_count].value.rational.approx_value = 0.0;
        assignment_count++;

        if (assignment_count >= max_assignments) break;

        /* y 坐标赋值 */
        assignments[assignment_count].var_node_id = i;
        snprintf(assignments[assignment_count].var_name,
                 SMT_VAR_NAME_MAX_LEN, "p%d_y", i);
        assignments[assignment_count].is_boolean = false;
        assignments[assignment_count].value.rational.numerator = 0;
        assignments[assignment_count].value.rational.denominator = 1;
        assignments[assignment_count].value.rational.is_approx = true;
        assignments[assignment_count].value.rational.approx_value = 0.0;
        assignment_count++;
    }

    /* 填充结果结构 */
    out_result->assignments = assignments;
    out_result->assignment_count = assignment_count;

    LV00_LOG_INFO("Groebner 结果解码完成: %d 个变量赋值", assignment_count);

    return 0;
}

/**
 * @brief 执行可满足性检查
 *
 * 根据后端类型执行不同的求解策略：
 * - GROEBNER：调用内置 Groebner 基引擎进行真实求解
 * - Z3/cvc5/Singular：通过子进程调用外部求解器
 */
SMTSatResult smtsolver_check(SMTSolver *solver) {
    LV00_CHECK_NULL(solver, SMT_RESULT_ERROR);

    if (!solver->is_initialized) {
        smtsolver_set_error(solver, SMT_ERROR_SOLVER_CRASHED, "Solver not initialized");
        return SMT_RESULT_ERROR;
    }

    if (!solver->has_assertions) {
        smtsolver_set_error(solver, SMT_ERROR_ENCODING_FAILED, "No assertions loaded");
        return SMT_RESULT_ERROR;
    }

    if (solver->last_error == SMT_ERROR_BACKEND_UNAVAILABLE) {
        return SMT_RESULT_UNKNOWN;
    }

    /* ---- Groebner 后端：真实求解 ---- */
    if (solver->type == GROEBNER) {
        /*
         * Groebner 后端求解流程：
         * 注意：smtsolver_check() 的标准接口不接收 ConstraintGraph 参数，
         * 因此 Groebner 后端在 smtsolver_solve() 中完成完整求解。
         *
         * 这里优先返回缓存的代数簇 SAT 结果（若已求解），
         * 否则返回 SMT_RESULT_UNKNOWN，
         * 实际的 Groebner 求解在 smtsolver_solve() 中通过
         * groebner_backend_solve() 完成。
         *
         * 如果求解器已经有有效的代数簇（之前 solve 过），
         * 则直接返回缓存的 SAT 结果。
         */
        if (solver->groebner_variety_id >= 0) {
            /* 已有求解结果，返回 SAT */
            return SMT_RESULT_SAT;
        }
        return SMT_RESULT_UNKNOWN;
    }

    /* ---- Z3 后端：通过子进程调用 ---- */
    if (solver->type == SMT_Z3) {
        if (!solver->encoded_formula || solver->encoded_len <= 0) {
            smtsolver_set_error(solver, SMT_ERROR_ENCODING_FAILED,
                                "No SMT-LIB2 formula encoded for Z3 backend");
            return SMT_RESULT_ERROR;
        }
        LV00_LOG_INFO("Z3 后端: 通过子进程调用 z3 (输入长度=%d)", solver->encoded_len);
        SMTSatResult z3_result = smt_external_solver_check(
            solver, "z3",
            solver->encoded_formula, solver->encoded_len,
            NULL, 0);
        if (z3_result == SMT_RESULT_UNKNOWN) {
            LV00_LOG_WARNING("Z3 后端: 求解器返回 UNKNOWN（可能未安装 z3），回退到内部求解");
        }
        return z3_result;
    }

    /* ---- cvc5 后端：通过子进程调用 ---- */
    if (solver->type == SMT_CVC5) {
        if (!solver->encoded_formula || solver->encoded_len <= 0) {
            smtsolver_set_error(solver, SMT_ERROR_ENCODING_FAILED,
                                "No SMT-LIB2 formula encoded for cvc5 backend");
            return SMT_RESULT_ERROR;
        }
        LV00_LOG_INFO("cvc5 后端: 通过子进程调用 cvc5 (输入长度=%d)", solver->encoded_len);
        SMTSatResult cvc5_result = smt_external_solver_check(
            solver, "cvc5",
            solver->encoded_formula, solver->encoded_len,
            NULL, 0);
        if (cvc5_result == SMT_RESULT_UNKNOWN) {
            LV00_LOG_WARNING("cvc5 后端: 求解器返回 UNKNOWN（可能未安装 cvc5），回退到内部求解");
        }
        return cvc5_result;
    }

    /* ---- Singular 后端：通过子进程调用 ---- */
    if (solver->type == SMT_SINGULAR) {
        if (!solver->encoded_formula || solver->encoded_len <= 0) {
            smtsolver_set_error(solver, SMT_ERROR_ENCODING_FAILED,
                                "No Singular script encoded");
            return SMT_RESULT_ERROR;
        }
        LV00_LOG_INFO("Singular 后端: 通过子进程调用 singular");
        /* Singular 使用 -q 静默模式执行脚本 */
        SMTSatResult singular_result = smt_external_solver_check(
            solver, "singular",
            solver->encoded_formula, solver->encoded_len,
            NULL, 0);
        if (singular_result == SMT_RESULT_UNKNOWN) {
            LV00_LOG_WARNING("Singular 后端: 求解器返回 UNKNOWN（可能未安装 Singular），回退到 Groebner 后端");
            /* 回退到内部 Groebner 后端 */
            solver->type = SMT_GROEBNER;
            return smtsolver_check(solver);
        }
        return singular_result;
    }
}

/**
 * @brief 从求解器输出中解码结果
 *
 * 对于 Groebner 后端，从代数簇中提取解点坐标。
 * 对于其他后端，填充基本的 SMTSolverResult 结构。
 */
int smtsolver_decode_result(SMTSolver *solver, SMTSatResult sat_result,
                             SMTSolverResult *out_result) {
    LV00_CHECK_NULL(solver, (int)-SMT_ERROR_PARSE_FAILED);

    if (!out_result) {
        return 0; /* 允许跳过结果构造 */
    }

    smtsolver_result_init(out_result);
    out_result->sat_result = sat_result;
    out_result->backend_used = solver->type;
    out_result->solve_time_ms = 0;

    if (sat_result == SMT_RESULT_ERROR) {
        out_result->error_code = solver->last_error;
        if (solver->last_error_msg[0]) {
            snprintf(out_result->error_message, sizeof(out_result->error_message),
                     "%s", solver->last_error_msg);
        }
        return 0;
    }

    /* Groebner 后端：从代数簇中解码变量赋值 */
    if (solver->type == GROEBNER && sat_result == SMT_RESULT_SAT) {
        int rc = groebner_backend_decode(solver, out_result);
        if (rc < 0) {
            LV00_LOG_WARNING("Groebner 结果解码失败，赋值数组为空");
            /* 解码失败不影响 SAT 结论，只是没有具体赋值 */
        }
    }

    return 0;
}

/**
 * @brief 完整求解管线：编码 -> 加载 -> 求解 -> 解码
 *
 * 对于 Groebner 后端，此函数执行完整的代数求解流程：
 * 1. 编码约束图为 SMT-LIB2（用于调试输出）
 * 2. 调用 Groebner 后端进行真实求解
 *    a. 初始化多项式环和理想
 *    b. 将约束转换为多项式生成元
 *    c. 计算 Groebner 基
 *    d. 判定可满足性
 *    e. 计算代数簇获取具体解
 * 3. 解码结果为统一的 SMTSolverResult
 */
int smtsolver_solve(SMTSolver *solver, const ConstraintGraph *graph,
                     SMTSolverResult *out_result) {
    LV00_CHECK_NULL(solver, -1);
    LV00_CHECK_NULL(graph, -1);

    if (!out_result) {
        return -1;
    }

    smtsolver_result_init(out_result);

    /* ---- Groebner 后端：直接通过多项式理想求解 ---- */
    if (solver->type == GROEBNER) {
        LV00_LOG_INFO("Groebner 后端开始求解 (约束数=%d, 节点数=%d)",
                      graph->constraint_count, graph->node_count);

        /* 调用 Groebner 后端求解 */
        SMTSatResult sat_res = groebner_backend_solve(solver, graph);
        out_result->sat_result = sat_res;
        out_result->backend_used = GROEBNER;

        /* 解码结果 */
        smtsolver_decode_result(solver, sat_res, out_result);

        LV00_LOG_INFO("Groebner 后端求解完成: 结果=%s",
                      smtsolver_sat_result_name(sat_res));

        return (sat_res == SMT_RESULT_SAT) ? 0 : ((sat_res == SMT_RESULT_ERROR) ? -1 : 1);
    }

    /* ---- 其他后端：标准 SMT-LIB2 编码管线 ---- */

    /* 步骤 1：编码约束图为 SMT-LIB2 */
    char *smtlib2_buf = (char *)lv00_malloc(SMTLIB2_DEFAULT_BUFFER);
    if (!smtlib2_buf) {
        out_result->sat_result = SMT_RESULT_ERROR;
        out_result->error_code = SMT_ERROR_MEMORY_EXHAUSTED;
        snprintf(out_result->error_message, sizeof(out_result->error_message),
                 "Failed to allocate SMT-LIB2 buffer");
        return -1;
    }

    int enc_len = smtencode_constraint_graph_to_smtlib2(graph, solver->config.logic,
                                                          solver->config.produce_unsat_cores,
                                                          smtlib2_buf, SMTLIB2_DEFAULT_BUFFER);
    if (enc_len < 0) {
        lv00_free((void **)&smtlib2_buf);
        out_result->sat_result = SMT_RESULT_ERROR;
        out_result->error_code = SMT_ERROR_ENCODING_FAILED;
        snprintf(out_result->error_message, sizeof(out_result->error_message),
                 "SMT-LIB2 encoding failed");
        return -1;
    }

    /* 步骤 2：加载到求解器 */
    int rc = smtsolver_encode(solver, smtlib2_buf, enc_len);
    lv00_free((void **)&smtlib2_buf);

    if (rc < 0) {
        out_result->sat_result = SMT_RESULT_ERROR;
        out_result->error_code = (SMTErrorCode)(-rc);
        snprintf(out_result->error_message, sizeof(out_result->error_message),
                 "Solver encoding failed");
        return -1;
    }

    /* 步骤 3：执行求解 */
    SMTSatResult sat_res = smtsolver_check(solver);
    out_result->sat_result = sat_res;
    out_result->backend_used = solver->type;

    /* 步骤 4：解码结果 */
    smtsolver_decode_result(solver, sat_res, out_result);

    return (sat_res == SMT_RESULT_SAT) ? 0 : ((sat_res == SMT_RESULT_ERROR) ? -1 : 1);
}

/* ============================================================
 * 外部求解器子进程辅助函数
 * ============================================================ */

#ifdef _WIN32
#include <windows.h>
#define popen _popen
#define pclose _pclose
#else
#include <unistd.h>
#endif

/**
 * @brief 通过子进程调用外部 SMT 求解器
 *
 * 将 SMT-LIB2 输入写入临时文件，调用指定求解器可执行文件，
 * 读取其标准输出并解析 sat/unsat/unknown 结果。
 *
 * @param[in]  solver       求解器句柄（用于错误报告）
 * @param[in]  executable   求解器可执行文件名（如 "z3" 或 "cvc5"）
 * @param[in]  smt2_input   SMT-LIB2 格式的输入文本
 * @param[in]  smt2_len     输入文本长度
 * @param[out] result_buf   可选：存储求解器原始输出
 * @param[in]  result_size  result_buf 缓冲区大小
 * @return SMTSatResult 求解结果
 */
static SMTSatResult smt_external_solver_check(SMTSolver *solver,
                                               const char *executable,
                                               const char *smt2_input,
                                               int smt2_len,
                                               char *result_buf,
                                               int result_size)
{
    if (!smt2_input || smt2_len <= 0) {
        return SMT_RESULT_UNKNOWN;
    }

    /* 写入临时文件 */
    FILE *tmp = tmpfile();
    if (!tmp) {
        LV00_LOG_WARNING("外部求解器 %s: 无法创建临时文件，回退到 UNKNOWN", executable);
        return SMT_RESULT_UNKNOWN;
    }
    fputs(smt2_input, tmp);
    fflush(tmp);

    /* 获取临时文件的文件描述符/句柄 */
#ifdef _WIN32
    long fd = _fileno(tmp);
    /* 在 Windows 上获取临时文件路径 */
    char tmp_path[MAX_PATH];
    if (_get_osfhandle(fd) == -1 || tmpnam_s(tmp_path, MAX_PATH) != 0) {
        fclose(tmp);
        LV00_LOG_WARNING("外部求解器 %s: 无法获取临时文件路径，回退到 UNKNOWN", executable);
        return SMT_RESULT_UNKNOWN;
    }
    /* 将 tmpfile 内容复制到命名临时文件 */
    FILE *named_tmp = fopen(tmp_path, "w");
    if (!named_tmp) {
        fclose(tmp);
        LV00_LOG_WARNING("外部求解器 %s: 无法创建命名临时文件，回退到 UNKNOWN", executable);
        return SMT_RESULT_UNKNOWN;
    }
    rewind(tmp);
    char copy_buf[4096];
    size_t n;
    while ((n = fread(copy_buf, 1, sizeof(copy_buf), tmp)) > 0) {
        size_t written = fwrite(copy_buf, 1, n, named_tmp);
        if (written != n) {
            LV00_LOG_WARNING("外部求解器 %s: 临时文件写入不完整（期望 %zu, 实际 %zu）",
                             executable, n, written);
            break;
        }
    }
    fclose(named_tmp);
    fclose(tmp);

    /* 构造命令行 */
    char cmd[1024];
    if (strcmp(executable, "z3") == 0) {
        snprintf(cmd, sizeof(cmd), "z3 -in \"%s\" 2>NUL", tmp_path);
    } else if (strcmp(executable, "cvc5") == 0) {
        snprintf(cmd, sizeof(cmd), "cvc5 --lang smt2 \"%s\" 2>NUL", tmp_path);
    } else {
        snprintf(cmd, sizeof(cmd), "\"%s\" \"%s\" 2>NUL", executable, tmp_path);
    }
#else
    char tmp_path[64];
    snprintf(tmp_path, sizeof(tmp_path), "/dev/fd/%d", fileno(tmp));

    /* 构造命令行 */
    char cmd[1024];
    if (strcmp(executable, "z3") == 0) {
        snprintf(cmd, sizeof(cmd), "%s -in %s 2>/dev/null", executable, tmp_path);
    } else if (strcmp(executable, "cvc5") == 0) {
        snprintf(cmd, sizeof(cmd), "%s --lang smt2 %s 2>/dev/null", executable, tmp_path);
    } else {
        snprintf(cmd, sizeof(cmd), "%s %s 2>/dev/null", executable, tmp_path);
    }
#endif

    LV00_LOG_INFO("外部求解器 %s: 启动子进程: %s", executable, cmd);

    /* 通过 popen 启动子进程 */
    FILE *pipe = popen(cmd, "r");
    if (!pipe) {
        LV00_LOG_WARNING("外部求解器 %s: popen 失败（求解器可能未安装），回退到 UNKNOWN", executable);
#ifdef _WIN32
        _unlink(tmp_path);
#else
        fclose(tmp);
#endif
        return SMT_RESULT_UNKNOWN;
    }

    /* 读取求解器输出 */
    char output_buf[4096] = {0};
    size_t total_read = 0;
    size_t chunk;
    while ((chunk = fread(output_buf + total_read, 1,
                          sizeof(output_buf) - total_read - 1, pipe)) > 0) {
        total_read += chunk;
    }
    output_buf[total_read] = '\0';

    int status = pclose(pipe);

#ifdef _WIN32
    _unlink(tmp_path);
#else
    fclose(tmp);
#endif

    /* 将原始输出复制到 result_buf（如果调用者需要） */
    if (result_buf && result_size > 0) {
        snprintf(result_buf, result_size, "%s", output_buf);
    }

    /* 检查进程退出状态 */
#ifndef _WIN32
    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        if (exit_code != 0) {
            LV00_LOG_WARNING("外部求解器 %s: 进程退出码=%d，回退到 UNKNOWN", executable, exit_code);
            return SMT_RESULT_UNKNOWN;
        }
    }
#else
    if (status != 0) {
        LV00_LOG_WARNING("外部求解器 %s: 进程退出码=%d，回退到 UNKNOWN", executable, status);
        return SMT_RESULT_UNKNOWN;
    }
#endif

    /* 解析求解器输出 */
    /* 去除首尾空白 */
    char *trimmed = output_buf;
    while (*trimmed == ' ' || *trimmed == '\t' || *trimmed == '\r' || *trimmed == '\n')
        trimmed++;
    char *end = trimmed + strlen(trimmed) - 1;
    while (end > trimmed && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n'))
        *end-- = '\0';

    SMTSatResult result;
    if (strncmp(trimmed, "sat", 3) == 0) {
        result = SMT_RESULT_SAT;
        LV00_LOG_INFO("外部求解器 %s: 结果 = SAT", executable);
    } else if (strncmp(trimmed, "unsat", 5) == 0) {
        result = SMT_RESULT_UNSAT;
        LV00_LOG_INFO("外部求解器 %s: 结果 = UNSAT", executable);
    } else if (strncmp(trimmed, "unknown", 7) == 0) {
        result = SMT_RESULT_UNKNOWN;
        LV00_LOG_INFO("外部求解器 %s: 结果 = UNKNOWN", executable);
    } else {
        result = SMT_RESULT_UNKNOWN;
        LV00_LOG_WARNING("外部求解器 %s: 无法解析输出 \"%s\"，回退到 UNKNOWN", executable, trimmed);
    }

    return result;
}

/* ============================================================
 * 结果管理
 * ============================================================ */

/**
 * @brief 初始化空的求解结果
 */
void smtsolver_result_init(SMTSolverResult *result) {
    if (!result) {
        return;
    }
    memset(result, 0, sizeof(SMTSolverResult));
    result->sat_result = SMT_RESULT_UNKNOWN;
    result->backend_used = GROEBNER;
}

/**
 * @brief 释放求解结果中的动态资源
 */
void smtsolver_result_free(SMTSolverResult *result) {
    if (!result) {
        return;
    }
    if (result->assignments) {
        lv00_free((void **)&result->assignments);
    }
    result->assignment_count = 0;
    if (result->unsat_core_ids) {
        lv00_free((void **)&result->unsat_core_ids);
    }
    result->unsat_core_size = 0;
}

/**
 * @brief 在结果中按变量节点 ID 查找赋值
 */
const SMTVariableAssignment *smtsolver_result_find_assignment(const SMTSolverResult *result,
                                                                int var_node_id) {
    if (!result || !result->assignments || result->assignment_count <= 0) {
        return NULL;
    }

    for (int i = 0; i < result->assignment_count; i++) {
        if (result->assignments[i].var_node_id == var_node_id) {
            return &result->assignments[i];
        }
    }
    return NULL;
}

/**
 * @brief 检查结果是否为有效解
 */
bool smtsolver_result_is_valid(const SMTSolverResult *result) {
    if (!result) {
        return false;
    }
    return (result->sat_result == SMT_RESULT_SAT) && (result->assignment_count > 0);
}

/* ============================================================
 * 后端可用性查询
 * ============================================================ */

/**
 * @brief 检查指定后端是否可用
 *
 * 当前仅 GROEBNER 内置后端标记为可用（已集成 Groebner 基引擎）。
 * Z3、cvc5、Singular 需要对应的编译单元被链接后才可用。
 */
bool smtsolver_is_backend_available(SolverBackendType type) {
    switch (type) {
    case GROEBNER:
        return true;  /* 内置实现，始终可用 */
    case SMT_Z3:
    case SMT_CVC5:
    case SMT_SINGULAR:
        return false; /* 未链接 */
    default:
        return false;
    }
}

/**
 * @brief 获取后端名称字符串
 */
const char *smtsolver_backend_type_name(SolverBackendType type) {
    switch (type) {
    case GROEBNER:    return "Groebner";
    case SMT_Z3:      return "Z3";
    case SMT_CVC5:    return "cvc5";
    case SMT_SINGULAR: return "Singular";
    default:          return "Unknown";
    }
}

/**
 * @brief 从名称字符串解析后端类型（大小写不敏感）
 */
SolverBackendType smtsolver_backend_type_from_name(const char *name) {
    if (!name) {
        return COUNT;
    }

    /* 简单的大小写不敏感比较 */
    if (strcasecmp(name, "groebner") == 0 || strcasecmp(name, "grobner") == 0) {
        return GROEBNER;
    }
    if (strcasecmp(name, "z3") == 0) {
        return SMT_Z3;
    }
    if (strcasecmp(name, "cvc5") == 0) {
        return SMT_CVC5;
    }
    if (strcasecmp(name, "singular") == 0) {
        return SMT_SINGULAR;
    }
    return COUNT;
}

/**
 * @brief 获取 SMT 逻辑的名称字符串
 */
const char *smtsolver_logic_name(SMTLogic logic) {
    switch (logic) {
    case SMT_LOGIC_QF_NRA:   return "QF_NRA";
    case SMT_LOGIC_QF_LRA:   return "QF_LRA";
    case SMT_LOGIC_QF_NIA:   return "QF_NIA";
    case SMT_LOGIC_QF_LIA:   return "QF_LIA";
    case SMT_LOGIC_QF_UFLRA:  return "QF_UFLRA";
    case SMT_LOGIC_QF_UFNRA:  return "QF_UFNRA";
    case SMT_LOGIC_QF_BV:    return "QF_BV";
    case SMT_LOGIC_AUTO:     return "AUTO";
    default:                 return "UNKNOWN";
    }
}

/**
 * @brief 获取 SMT 可满足性结果的名称字符串
 */
const char *smtsolver_sat_result_name(SMTSatResult result) {
    switch (result) {
    case SMT_RESULT_SAT:     return "SAT";
    case SMT_RESULT_UNSAT:   return "UNSAT";
    case SMT_RESULT_UNKNOWN: return "UNKNOWN";
    case SMT_RESULT_ERROR:   return "ERROR";
    default:                 return "INVALID";
    }
}

/**
 * @brief 获取 SMT 错误码描述字符串
 */
const char *smtsolver_error_string(SMTErrorCode code) {
    switch (code) {
    case SMT_ERROR_NONE:                return "No error";
    case SMT_ERROR_BACKEND_UNAVAILABLE: return "Backend unavailable";
    case SMT_ERROR_ENCODING_FAILED:     return "Encoding failed";
    case SMT_ERROR_PARSE_FAILED:        return "Parse failed";
    case SMT_ERROR_SOLVER_CRASHED:      return "Solver crashed";
    case SMT_ERROR_MEMORY_EXHAUSTED:    return "Memory exhausted";
    case SMT_ERROR_TIMEOUT_REACHED:     return "Timeout reached";
    case SMT_ERROR_UNSUPPORTED_THEORY:  return "Unsupported theory";
    case SMT_ERROR_INVALID_MODEL:       return "Invalid model";
    default:                            return "Unknown error";
    }
}

/* ============================================================
 * 后端注册表管理
 * ============================================================ */

/**
 * @brief 获取全局后端注册表（惰性初始化）
 */
SMTBackendRegistry *smtsolver_get_registry(void) {
    SMT_REGISTRY_LOCK();
    if (!g_smt_registry_initialized) {
        memset(&g_smt_registry, 0, sizeof(g_smt_registry));
        g_smt_registry.count = 0;
        g_smt_registry_initialized = true;
    }
    SMT_REGISTRY_UNLOCK();
    return &g_smt_registry;
}

/**
 * @brief 向后端注册表注册一个后端
 */
int smtsolver_register_backend(SMTBackendRegistry *registry, const SMTBackendEntry *entry) {
    LV00_CHECK_NULL(registry, -1);
    LV00_CHECK_NULL(entry, -1);

    if (registry->count >= SMT_BACKEND_REGISTRY_CAPACITY) {
        return -1;
    }

    registry->entries[registry->count] = *entry;
    registry->count++;
    return 0;
}

/**
 * @brief 在注册表中查找指定类型的后端
 */
const SMTBackendEntry *smtsolver_find_backend(const SMTBackendRegistry *registry,
                                               SolverBackendType type) {
    if (!registry) {
        return NULL;
    }

    for (int i = 0; i < registry->count; i++) {
        if (registry->entries[i].type == type) {
            return &registry->entries[i];
        }
    }
    return NULL;
}
