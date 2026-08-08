/**
 * @file smt_backend_internal.h
 * @brief SMT 后端内部共享常量/结构/函数声明（从 smt_backend_impl.c 拆分）
 *
 * @details 由 smt_backend_impl.c 与其拆分文件共享的常量宏、SMTSolver
 *          内部结构与 Groebner 后端钩子函数声明。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#ifndef lv_SMT_BACKEND_INTERNAL_H
#define lv_SMT_BACKEND_INTERNAL_H

#include <stdbool.h>

#include "smt_backend.h"
#include "groebner_engine.h"
#include "lv/lv_utils.h" /* GROEBNER_SMT_ZERO_THRESHOLD 语义别名 = lv_EPSILON_DOUBLE */

#ifdef __cplusplus
extern "C" {
#endif

/* ---- 模块级常量 ---- */
#define SMTLIB2_DEFAULT_BUFFER 65536          /* SMT-LIB2 输出缓冲区默认大小 */
#define SMT_DEFAULT_TIMEOUT_MS 30000          /* 默认求解超时（毫秒） */
#define SMT_DEFAULT_MEMORY_MB 1024            /* 默认内存限制（MB） */
#define GROEBNER_VAR_NAME_MAX 64              /* Groebner 后端多项式变量名最大长度 */
#define GROEBNER_DEFAULT_VAR_CAPACITY 32      /* Groebner 后端默认变量容量（每个点 2 个坐标变量） */
/* 数值零判定阈值（用于判断多项式是否为零）：语义别名 = lv_EPSILON_DOUBLE（1e-12） */
#define GROEBNER_SMT_ZERO_THRESHOLD lv_EPSILON_DOUBLE

/**
 * @brief SMT 求解器内部状态
 *
 * 存储求解器的类型、配置、错误状态以及 Groebner 后端专用的
 * 环注册表和理想 ID 等求解上下文。
 */
struct SMTSolver {
    SolverBackendType type;   /**< 后端类型 */
    SMTSolverConfig config;   /**< 求解器配置 */
    SMTErrorCode last_error;  /**< 最近错误码 */
    char last_error_msg[512]; /**< 最近错误消息 */
    char *encoded_formula;    /**< 已编码的 SMT-LIB2 脚本 */
    int encoded_len;          /**< 编码长度 */
    bool is_initialized;      /**< 是否已初始化 */
    bool has_assertions;      /**< 是否有待求解的断言 */

    /* ---- Groebner 后端专用字段 ---- */
    lvRingRegistry *groebner_registry; /**< Groebner 环注册表（惰性创建） */
    int groebner_ring_id;              /**< Groebner 多项式环 ID */
    int groebner_ideal_id;             /**< Groebner 理想 ID（约束转换结果） */
    int groebner_var_count;            /**< Groebner 环中的变量数量 */
    int *groebner_node_var_map;        /* 节点 ID -> 变量索引映射表 */
    int groebner_node_var_map_size;    /* 映射表大小 */
    int groebner_variety_id;           /* Groebner 代数簇 ID（求解结果） */
};

/* ---- Groebner 后端钩子（在 smt_backend_impl_groebner.c 实现） ---- */
int groebner_backend_init(SMTSolver *solver, const ConstraintGraph *graph);
void groebner_backend_cleanup(SMTSolver *solver);
SMTSatResult groebner_backend_solve(SMTSolver *solver, const ConstraintGraph *graph);
int groebner_backend_decode(SMTSolver *solver, SMTSolverResult *out_result);

#ifdef __cplusplus
}
#endif

#endif /* lv_SMT_BACKEND_INTERNAL_H */
