#ifndef lv_SMT_BACKEND_H
#define lv_SMT_BACKEND_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

#include "constraint_graph.h"

/* ── Solver backend type ── */
typedef enum { GROEBNER = 0, SMT_Z3 = 1, SMT_CVC5 = 2, SMT_SINGULAR = 3, COUNT } SolverBackendType;

#define SMT_GROEBNER GROEBNER

/**
 * @brief SolverBackendType 显示名单一事实源（枚举 ↔ 名称映射）
 *
 * 每行 2 列：ENUM（枚举值）、NAME（显示名）。本宏是 SMT 后端显示名的
 * 唯一权威源：smt_backend_impl.c 的 name↔enum 表、内置后端注册表、
 * 插件注册表，以及 groebner 子模块的外部后端表均由此派生；禁止在其他
 * 文件重复硬编码后端显示名（新增后端只需改本列表一处）。
 */
#define LV_SMT_BACKEND_ENTRY(x) \
    x(GROEBNER, "Groebner") \
    x(SMT_Z3, "Z3") \
    x(SMT_CVC5, "cvc5") \
    x(SMT_SINGULAR, "Singular")

/* ── SMT Logic ── */
typedef enum {
    SMT_LOGIC_QF_NRA = 0,
    SMT_LOGIC_QF_LRA = 1,
    SMT_LOGIC_QF_NIA = 2,
    SMT_LOGIC_QF_LIA = 3,
    SMT_LOGIC_QF_UFLRA = 4,
    SMT_LOGIC_QF_UFNRA = 5,
    SMT_LOGIC_QF_BV = 6,
    SMT_LOGIC_AUTO = 7
} SMTLogic;

/* ── SMT Error Code ── */
typedef enum {
    SMT_ERROR_NONE = 0,
    SMT_ERROR_BACKEND_UNAVAILABLE = 1,
    SMT_ERROR_ENCODING_FAILED = 2,
    SMT_ERROR_PARSE_FAILED = 3,
    SMT_ERROR_SOLVER_CRASHED = 4,
    SMT_ERROR_MEMORY_EXHAUSTED = 5,
    SMT_ERROR_TIMEOUT_REACHED = 6,
    SMT_ERROR_UNSUPPORTED_THEORY = 7,
    SMT_ERROR_INVALID_MODEL = 8
} SMTErrorCode;

/* ── SMT Sat Result ── */
typedef enum { SMT_RESULT_SAT = 0, SMT_RESULT_UNSAT = 1, SMT_RESULT_UNKNOWN = 2, SMT_RESULT_ERROR = 3 } SMTSatResult;

/* ── SMT Solver Config ── */
#define SMT_VAR_NAME_MAX_LEN 128

typedef struct SMTSolverConfig {
    int timeout_ms;
    int memory_limit_mb;
    SMTLogic logic;
    bool produce_models;
    bool produce_unsat_cores;
    bool produce_proofs;
    bool incremental;
    int random_seed;
    int verbosity;
    void *custom_config;
} SMTSolverConfig;

/* ── SMT Variable Assignment ── */
typedef struct SMTRational {
    long long numerator;
    long long denominator;
    bool is_approx;
    double approx_value;
    struct { /* alias for code using .rational.xxx */
        long long numerator;
        long long denominator;
        bool is_approx;
        double approx_value;
    } rational;
} SMTRational;

typedef struct SMTVariableAssignment {
    int var_node_id;
    char variable_name[128];
    char var_name[128]; /* alias */
    SMTRational value;
    bool is_boolean;
} SMTVariableAssignment;

/* ── SMT Solver Result ── */
typedef struct SMTSolverResult {
    SMTSatResult sat_result;
    SolverBackendType backend_used;
    double solve_time_ms;
    SMTErrorCode error_code;
    char error_message[512];
    SMTVariableAssignment *assignments;
    int assignment_count;
    int *unsat_core_ids;
    int unsat_core_size;
} SMTSolverResult;

/* ── SMT Backend Entry ── */
typedef struct SMTBackendEntry {
    SolverBackendType type;
    char name[64];
    bool available;
    char path[512];
    char version[32];
    int priority;
} SMTBackendEntry;

/* ── SMT Backend Registry ── */
#define SMT_BACKEND_REGISTRY_CAPACITY 16

typedef struct SMTBackendRegistry {
    SMTBackendEntry entries[SMT_BACKEND_REGISTRY_CAPACITY];
    int count;
} SMTBackendRegistry;

/* ── SMTSolver opaque type (full struct in smt_backend_impl.c) ── */
typedef struct SMTSolver SMTSolver;

/* ── API ── */
const SMTSolverConfig *smtsolver_default_config(SolverBackendType type);
SMTSolver *smtsolver_create(SolverBackendType type, const SMTSolverConfig *config);
void smtsolver_destroy(SMTSolver *solver);
void smtsolver_set_error(SMTSolver *solver, SMTErrorCode code, const char *msg);
SolverBackendType smtsolver_get_type(const SMTSolver *solver);
SMTErrorCode smtsolver_get_last_error(const SMTSolver *solver);
const char *smtsolver_get_last_error_message(const SMTSolver *solver);

int smtencode_constraint_graph_to_smtlib2(const ConstraintGraph *graph, SMTLogic logic, bool produce_unsat_cores,
                                          char *out, size_t out_len);
int smtsolver_encode(SMTSolver *solver, const char *smtlib2, int len);
SMTSatResult smtsolver_check(SMTSolver *solver);

void smtsolver_result_init(SMTSolverResult *result);
int smtsolver_decode_groebner_variety(SMTSolver *solver, SMTSolverResult *out_result);
int smtsolver_decode_result(SMTSolver *solver, SMTSatResult sat_result, SMTSolverResult *out_result);
void smtsolver_result_clear(SMTSolverResult *result);
const SMTVariableAssignment *smtsolver_result_find_assignment(const SMTSolverResult *result, int var_node_id);
bool smtsolver_result_has_model(const SMTSolverResult *result);
int smtsolver_solve(SMTSolver *solver, const ConstraintGraph *graph, SMTSolverResult *out_result);
void smtsolver_result_free(SMTSolverResult *result);
int smtsolver_check_constraint_satisfied(SMTSolver *solver, const ConstraintGraph *graph, SMTSolverResult *out_result);

bool smtsolver_is_backend_available(SolverBackendType type);
const char *smtsolver_backend_type_name(SolverBackendType type);
SolverBackendType smtsolver_backend_type_from_name(const char *name);

const char *smtsolver_logic_name(SMTLogic logic);
const char *smtsolver_sat_result_name(SMTSatResult result);
const char *smtsolver_error_string(SMTErrorCode code);

SMTSatResult smt_external_solver_check(SMTSolver *solver, const char *executable, const char *smt2_input, int smt2_len,
                                       char *result_buf, int result_size);

SMTBackendRegistry *smtsolver_get_registry(void);
int smtsolver_register_backend(SMTBackendRegistry *registry, const SMTBackendEntry *entry);
const SMTBackendEntry *smtsolver_find_backend(const SMTBackendRegistry *registry, SolverBackendType type);

/**
 * @brief 将所有 SMT 后端注册到全局后端插件注册表
 *
 * 创建 lvBackendPlugin 包装器，使 SMT 后端可通过统一的
 * lv_backend_plugin_find() / lv_backend_plugin_find_by_type() 查找。
 * 可安全地多次调用（仅首次生效）。
 */
void smt_register_all_plugins(void);

#ifdef __cplusplus
}
#endif
#endif
