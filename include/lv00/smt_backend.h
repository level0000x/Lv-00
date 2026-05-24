/**
 * @file smt_backend.h
 * @brief SMT 后端抽象层 —— 多引擎 SMT 求解的接口、配置与编码
 *
 * @details 本模块提供与求解器无关的 SMT 后端抽象定义，参考 polymake 的
 *          多后端架构设计。它统一了 Z3、cvc5、Singular 等外部求解引擎的
 *          调用接口，并通过 SMT-LIB2 标准格式实现与各后端的互操作。
 *
 *          核心抽象层次：
 *          - SolverBackendType    —— 枚举所有可用的后端类型（含 Gröbner）
 *          - SMTSolverConfig      —— 每个后端实例的可配置参数
 *          - SMTSolverResult      —— 统一的求解结果结构
 *          - smtsolver_* 系列函数 —— 具有多态的创建/销毁/检查/编码接口
 *
 *          编码管线：
 *          1. smtencode_constraint_graph_to_smtlib2()  约束图 → SMT-LIB2
 *          2. smtsolver_encode()                       SMT-LIB2 → 后端原生表示
 *          3. smtsolver_check()                        执行求解
 *          4. smtsolver_decode_result()                解析结果 → SMTSolverResult
 *
 * @note 本模块不直接链接任何外部求解库。所有后端创建函数在其对应的
 *       编译单元中实现（如 smt_z3.c、smt_cvc5.c），通过工厂函数注册。
 *
 * @author Lv-00 Project
 * @version 3.2.0
 */

#ifndef LV00_SMT_BACKEND_H
#define LV00_SMT_BACKEND_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "constraint_graph.h"
#include "error_codes.h"

/* ============================================================
 * 前向声明
 * ============================================================ */

/** @brief 不透明的 SMT 求解器句柄，由后端实现持有内部状态 */
typedef struct SMTSolver SMTSolver;

/** @brief 不透明的 SMT 变量赋值映射表 */
typedef struct SMTAssignmentMap SMTAssignmentMap;

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief SMT-LIB2 编码输出的默认缓冲区大小（字符数） */
#define SMT_SMTLIB2_BUFFER_SIZE 1048576

/** @brief 单个变量赋值的名称最大长度 */
#define SMT_VAR_NAME_MAX_LEN 128

/** @brief UNSAT 核心中可容纳的最大约束数 */
#define SMT_UNSAT_CORE_MAX_SIZE 256

/** @brief 后端工厂注册表中可容纳的最大后端数量 */
#define SMT_BACKEND_REGISTRY_CAPACITY 16

/* ============================================================
 * 枚举定义
 * ============================================================ */

/**
 * @brief 求解器后端类型枚举
 *
 * 枚举所有 Lv-00 支持的形式化求解后端。每个后端类型对应一组
 * 创建函数，按需链接对应的外部库。
 *
 * @details 设计原则：
 *          - GROEBNER 保持为默认/遗留后端的引用
 *          - SMT_Z3 / SMT_CVC5 为主要的 SMT 后端
 *          - SMT_SINGULAR 用于需要符号代数+可满足性混合求解的场景
 *          - COUNT 用作数组大小标记，便于静态表查找
 */
typedef enum {
    GROEBNER = 0, /**< Gröbner 基方法（度数≤2，内置实现） */
    SMT_Z3,       /**< Z3 SMT 求解器（Microsoft Research） */
    SMT_CVC5,     /**< cvc5 SMT 求解器（Stanford/Waterloo） */
    SMT_SINGULAR, /**< Singular 代数系统（含 SMT 接口） */
    COUNT         /**< 后端类型计数（用于数组大小） */
} SolverBackendType;

/**
 * @brief SMT 求解器支持的理论逻辑
 *
 * SMT-LIB2 标准定义的逻辑片段（logic）。不同后端对不同逻辑
 * 的支持程度不同：Z3 全部支持，cvc5 支持 NRA/LRA，Singular
 * 偏向符号代数。
 */
typedef enum {
    SMT_LOGIC_QF_NRA = 0, /**< 无量词非线性实数算术 */
    SMT_LOGIC_QF_LRA,     /**< 无量词线性实数算术 */
    SMT_LOGIC_QF_NIA,     /**< 无量词非线性整数算术 */
    SMT_LOGIC_QF_LIA,     /**< 无量词线性整数算术 */
    SMT_LOGIC_QF_UFLRA,   /**< 无量词未解释函数 + 线性实数算术 */
    SMT_LOGIC_QF_UFNRA,   /**< 无量词未解释函数 + 非线性实数算术 */
    SMT_LOGIC_QF_BV,      /**< 无量词位向量 */
    SMT_LOGIC_AUTO        /**< 由后端自动检测最合适的逻辑 */
} SMTLogic;

/**
 * @brief SMT 求解器的可满足性结果
 */
typedef enum {
    SMT_RESULT_SAT = 0, /**< 可满足（至少存在一个模型） */
    SMT_RESULT_UNSAT,   /**< 不可满足（无模型） */
    SMT_RESULT_UNKNOWN, /**< 未知（超时、内存不足或超出理论范围） */
    SMT_RESULT_ERROR    /**< 求解器内部错误 */
} SMTSatResult;

/**
 * @brief SMT 求解器错误码
 *
 * 从 Lv00 统一错误码系统扩展的后端专用错误码。
 */
typedef enum {
    SMT_ERROR_NONE = 0,            /**< 无错误 */
    SMT_ERROR_BACKEND_UNAVAILABLE, /**< 请求的后端不可用（未编译链接） */
    SMT_ERROR_ENCODING_FAILED,     /**< SMT-LIB2 编码失败 */
    SMT_ERROR_PARSE_FAILED,        /**< 求解器输出解析失败 */
    SMT_ERROR_SOLVER_CRASHED,      /**< 外部求解器进程崩溃 */
    SMT_ERROR_MEMORY_EXHAUSTED,    /**< 超出配置的内存限制 */
    SMT_ERROR_TIMEOUT_REACHED,     /**< 超出配置的时间限制 */
    SMT_ERROR_UNSUPPORTED_THEORY,  /**< 后端不支持请求的逻辑理论 */
    SMT_ERROR_INVALID_MODEL        /**< 返回的模型无效或不一致 */
} SMTErrorCode;

/* ============================================================
 * 结构体定义
 * ============================================================ */

/**
 * @brief SMT 求解器的运行配置
 *
 * 所有后端通用的可配置参数。特定后端可通过内部扩展数据
 * （custom_config）传递私有配置（如 Z3 的 tactics、cvc5 的
 * proof mode 等）。
 */
typedef struct SMTSolverConfig {
    int64_t timeout_ms;       /**< 求解超时时间（毫秒），0 表示无限制 */
    int64_t memory_limit_mb;  /**< 内存上限（MB），0 表示无限制 */
    SMTLogic logic;           /**< 使用的逻辑理论片段 */
    bool produce_models;      /**< 是否为 SAT 结果生成模型 */
    bool produce_unsat_cores; /**< 是否为 UNSAT 结果生成核心 */
    bool produce_proofs;      /**< 是否生成证明对象 */
    bool incremental;         /**< 是否启用增量求解模式 */
    int random_seed;          /**< 随机种子（-1 表示使用时间戳） */
    int verbosity;            /**< 日志详细级别（0=静默，3=最高） */
    void *custom_config;      /**< 后端特定的扩展配置（可为 NULL） */
} SMTSolverConfig;

/**
 * @brief 单个变量的符号赋值
 *
 * 存储 SMT 求解后某个约束图变量节点 ID 对应的解。
 * 支持有理数赋值（分子/分母）和布尔赋值两种模式。
 */
typedef struct SMTVariableAssignment {
    int var_node_id;                     /**< 约束图中的变量节点 ID */
    char var_name[SMT_VAR_NAME_MAX_LEN]; /**< 变量名（与 SMT-LIB2 编码一致） */
    bool is_boolean;                     /**< 是否为布尔变量 */
    union {
        struct {
            int64_t numerator;    /**< 有理数分子 */
            uint64_t denominator; /**< 有理数分母（0 表示十进制近似值） */
            bool is_approx;       /**< 分母=0 时，此值为近似十进制标记 */
            double approx_value;  /**< 近似十进制值 */
        } rational;
        bool bool_value; /**< 布尔值 */
    } value;
} SMTVariableAssignment;

/**
 * @brief SMT 求解器返回的统一结果
 *
 * 为所有后端类型提供一致的求解结果表示。
 * 对于 SAT 结果，可通过模型函数读取变量赋值；
 * 对于 UNSAT 结果，可查询 unsat_core。
 */
typedef struct SMTSolverResult {
    SMTSatResult sat_result;        /**< 可满足性结论 */
    SolverBackendType backend_used; /**< 实际使用的后端类型 */
    int64_t solve_time_ms;          /**< 求解耗时（毫秒，含编码+检查+解码） */

    /* SAT 结果：变量赋值 */
    SMTVariableAssignment *assignments; /**< 变量赋值数组 */
    int assignment_count;               /**< 赋值数量 */

    /* UNSAT 结果：不可满足核心 */
    int *unsat_core_ids; /**< UNSAT 核心中的约束 ID 数组 */
    int unsat_core_size; /**< 核心大小 */

    /* 错误信息（SAT_RESULT_ERROR 时有效） */
    SMTErrorCode error_code; /**< 后端错误码 */
    char error_message[512]; /**< 人类可读的错误信息 */
} SMTSolverResult;

/* ============================================================
 * 默认配置
 * ============================================================ */

/**
 * @brief 获取后端类型的默认配置
 *
 * @param[in] type  后端类型
 * @return 指向静态存储的默认配置。调用者不得修改或释放。
 *         对于不支持的后端类型，返回所有字段归零的默认配置。
 */
const SMTSolverConfig *smtsolver_default_config(SolverBackendType type);

/* ============================================================
 * 后端生命周期管理
 * ============================================================ */

/**
 * @brief 创建一个 SMT 求解器实例
 *
 * 根据后端类型动态选择并调用对应的工厂函数创建求解器实例。
 * 如果目标后端的编译单元未被链接，则返回 NULL 并设置
 * SMT_ERROR_BACKEND_UNAVAILABLE。
 *
 * @param[in] type    后端类型
 * @param[in] config  求解器配置（可为 NULL，使用默认配置）
 * @return 新分配的 SMTSolver 句柄，失败返回 NULL。
 *         调用者需用 smtsolver_destroy() 释放。
 */
SMTSolver *smtsolver_create(SolverBackendType type, const SMTSolverConfig *config);

/**
 * @brief 销毁 SMT 求解器实例并释放所有后端资源
 *
 * 对于 NULL 输入，函数为空操作。
 *
 * @param[in,out] solver  要销毁的求解器（可为 NULL）
 */
void smtsolver_destroy(SMTSolver *solver);

/**
 * @brief 获取求解器的后端类型
 *
 * @param[in] solver  求解器句柄
 * @return 后端类型。solver 为 NULL 时返回 COUNT。
 */
SolverBackendType smtsolver_get_type(const SMTSolver *solver);

/**
 * @brief 获取求解器最近一次错误的错误码
 *
 * @param[in] solver  求解器句柄
 * @return 错误码。solver 为 NULL 时返回 SMT_ERROR_NONE。
 */
SMTErrorCode smtsolver_get_last_error(const SMTSolver *solver);

/**
 * @brief 获取求解器最近一次错误的描述信息
 *
 * @param[in] solver  求解器句柄
 * @return 错误描述字符串（内部存储，调用者不得释放）。
 *         无错误时返回空字符串。solver 为 NULL 时返回 "null solver"。
 */
const char *smtsolver_get_last_error_message(const SMTSolver *solver);

/* ============================================================
 * 求解流程
 * ============================================================ */

/**
 * @brief 将约束图编码为 SMT-LIB2 格式的字符串
 *
 * 遍历约束图中的所有几何约束，提取对应的代数方程/不等式，
 * 编码为标准 SMT-LIB2 脚本。
 *
 * 支持的约束类型编码规则：
 * - INCIDENCE：  叉积方程编码为多项式等式（= 0）
 * - BETWEENNESS：共线条件 + 分段排序编码为线性不等式
 * - INTERSECTION：参数化线性系统编码为等式组
 * - CONTAINMENT：卷绕数检测编码为分段不等式
 * - CONNECTION：  数据流等价关系编码为等式
 *
 * @param[in]  graph         约束图
 * @param[in]  logic         目标 SMT 逻辑理论
 * @param[in]  produce_unsat_cores  是否为导出 UNSAT 核心生成命名断言
 * @param[out] out_smtlib2   输出的 SMT-LIB2 脚本缓冲区
 * @param[in]  buffer_size   缓冲区大小（字符数）
 * @return 实际写入的字符数（不含终止符），失败返回 -1。
 *         如果缓冲区不足，返回所需的总字符数。
 */
int smtencode_constraint_graph_to_smtlib2(const ConstraintGraph *graph, SMTLogic logic, bool produce_unsat_cores,
                                          char *out_smtlib2, size_t buffer_size);

/**
 * @brief 将 SMT-LIB2 脚本加载到求解器内部表示中
 *
 * 子操作：
 * 1. 重置求解器内部状态（如启用增量模式则保留上下文）
 * 2. 解析 SMT-LIB2 脚本
 * 3. 构造后端特定的断言集
 *
 * @param[in,out] solver     求解器句柄
 * @param[in]     smtlib2    SMT-LIB2 脚本字符串
 * @param[in]     len        脚本长度（字符数），-1 表示自动检测
 * @return 成功返回 0，失败返回负的 SMTErrorCode。
 */
int smtsolver_encode(SMTSolver *solver, const char *smtlib2, int len);

/**
 * @brief 执行可满足性检查（求解）
 *
 * 对已编码的断言集执行 SAT/UNSAT 检查。
 * 调用前必须已经通过 smtsolver_encode() 加载断言。
 *
 * @param[in,out] solver  求解器句柄
 * @return SMT 可满足性结果。
 *         如果返回 SMT_RESULT_ERROR，调用
 *         smtsolver_get_last_error() 获取详情。
 */
SMTSatResult smtsolver_check(SMTSolver *solver);

/**
 * @brief 从求解器输出中解码结果
 *
 * 解析求解器的原始输出（如 Z3 的 model、cvc5 的 get-value 输出），
 * 构造统一的 SMTSolverResult 结构。
 *
 * @param[in]  solver       求解器句柄
 * @param[in]  sat_result   由 smtsolver_check() 返回的结果
 * @param[out] out_result   输出的结构化结果（调用者负责释放）
 *                          可为 NULL 以跳过结果构造
 * @return 成功返回 0，失败返回负的 SMTErrorCode。
 *         即使成功，如果 sat_result 为 SMT_RESULT_UNSAT 或
 *         SMT_RESULT_UNKNOWN，out_result->assignments 可能为空。
 */
int smtsolver_decode_result(SMTSolver *solver, SMTSatResult sat_result, SMTSolverResult *out_result);

/**
 * @brief 完整的求解管线：编码、检查、解码一条龙
 *
 * 等价于依次调用：
 * 1. smtencode_constraint_graph_to_smtlib2() - 编码
 * 2. smtsolver_encode()                     - 加载
 * 3. smtsolver_check()                      - 求解
 * 4. smtsolver_decode_result()              - 解码
 *
 * @param[in]  solver       求解器句柄
 * @param[in]  graph        约束图
 * @param[out] out_result   输出的结构化结果（调用者负责释放）
 * @return 求解器状态码（参考 SolverStatus）。
 *         成功时 out_result 已填充完整结果。
 */
int smtsolver_solve(SMTSolver *solver, const ConstraintGraph *graph, SMTSolverResult *out_result);

/* ============================================================
 * 结果管理
 * ============================================================ */

/**
 * @brief 初始化一个空的 SMTSolverResult 结构
 *
 * 将所有字段归零/置为空指针。
 * 在通过 smtsolver_decode_result() 或手动填充前调用。
 *
 * @param[out] result  要初始化的结果结构
 */
void smtsolver_result_init(SMTSolverResult *result);

/**
 * @brief 释放 SMTSolverResult 中动态分配的资源
 *
 * 释放 assignments 数组和 unsat_core_ids 数组（如果有）。
 * 安全接受 NULL 指针和已释放的结果。
 *
 * @param[in,out] result  要清理的结果结构
 */
void smtsolver_result_free(SMTSolverResult *result);

/**
 * @brief 在结果中按变量节点 ID 查找赋值
 *
 * 对 assignments 数组进行线性搜索。
 *
 * @param[in] result      求解结果
 * @param[in] var_node_id  变量节点 ID
 * @return 找到的赋值指针，未找到返回 NULL。
 */
const SMTVariableAssignment *smtsolver_result_find_assignment(const SMTSolverResult *result, int var_node_id);

/**
 * @brief 检查求解结果是否为有效解
 *
 * @param[in] result  求解结果
 * @return true 如果 sat_result 为 SMT_RESULT_SAT 且至少有一个赋值
 */
bool smtsolver_result_is_valid(const SMTSolverResult *result);

/* ============================================================
 * 后端可用性查询
 * ============================================================ */

/**
 * @brief 检查指定后端是否可用
 *
 * 检查对应后端的编译单元是否被链接到当前二进制。
 *
 * @param[in] type  后端类型
 * @return true 可用，false 不可用或未链接。
 */
bool smtsolver_is_backend_available(SolverBackendType type);

/**
 * @brief 获取后端类型的名称字符串
 *
 * @param[in] type  后端类型
 * @return 名称字符串（如 "Gröbner"、"Z3"、"cvc5"、"Singular"）。
 *         无效类型返回 "Unknown"。
 */
const char *smtsolver_backend_type_name(SolverBackendType type);

/**
 * @brief 从名称字符串解析后端类型
 *
 * 大小写不敏感的匹配。
 *
 * @param[in] name  后端名称（如 "z3"、"groebner"、"cvc5"）
 * @return 对应的后端类型，无效名称返回 COUNT。
 */
SolverBackendType smtsolver_backend_type_from_name(const char *name);

/**
 * @brief 获取 SMT 逻辑的名称字符串
 *
 * @param[in] logic  SMT 逻辑
 * @return 名称字符串（如 "QF_NRA"、"QF_LRA"）。
 *         无效类型返回 "UNKNOWN"。
 */
const char *smtsolver_logic_name(SMTLogic logic);

/**
 * @brief 获取 SMT 可满足性结果的名称字符串
 *
 * @param[in] result  SMT 结果
 * @return 名称字符串（"SAT"、"UNSAT"、"UNKNOWN"、"ERROR"）
 */
const char *smtsolver_sat_result_name(SMTSatResult result);

/**
 * @brief 获取 SMT 错误码对应的描述字符串
 *
 * @param[in] code  SMT 错误码
 * @return 描述字符串（如 "Backend unavailable"）
 */
const char *smtsolver_error_string(SMTErrorCode code);

/* ============================================================
 * 后端注册表（供 engine_scheduler.h 使用）
 * ============================================================ */

/**
 * @brief 后端工厂函数签名
 *
 * 每个后端实现提供此签名的一个函数，用于创建求解器实例。
 *
 * @param[in] config  求解器配置（可为 NULL，使用默认值）
 * @return 新分配的 SMTSolver 句柄，失败返回 NULL。
 */
typedef SMTSolver *(*SMTSolverCreateFunc)(const SMTSolverConfig *config);

/**
 * @brief 后端注册表条目
 *
 * 存储已注册后端的元数据，供引擎调度器查询和路由。
 */
typedef struct SMTBackendEntry {
    SolverBackendType type;          /**< 后端类型 */
    bool available;                  /**< 是否已链接可用 */
    SMTSolverCreateFunc create_func; /**< 工厂创建函数 */
    int priority;                    /**< 调度优先级（数值越低越优先） */
    const char *description;         /**< 后端描述文本 */
} SMTBackendEntry;

/**
 * @brief 后端注册表
 *
 * 全局注册表，存储所有已链接的后端信息。
 * 由各后端的初始化函数填充，由调度器查询。
 */
typedef struct SMTBackendRegistry {
    SMTBackendEntry entries[SMT_BACKEND_REGISTRY_CAPACITY];
    int count; /**< 当前已注册的后端数量 */
} SMTBackendRegistry;

/**
 * @brief 获取全局后端注册表
 *
 * @return 指向全局静态注册表的指针。首次调用时自动初始化。
 */
SMTBackendRegistry *smtsolver_get_registry(void);

/**
 * @brief 向注册表注册一个后端
 *
 * @param[in,out] registry  注册表
 * @param[in]     entry     后端条目
 * @return 成功返回 0，注册表已满返回 -1。
 */
int smtsolver_register_backend(SMTBackendRegistry *registry, const SMTBackendEntry *entry);

/**
 * @brief 在注册表中查找指定类型的后端条目
 *
 * @param[in] registry  注册表
 * @param[in] type      后端类型
 * @return 找到的条目指针，未找到返回 NULL。
 */
const SMTBackendEntry *smtsolver_find_backend(const SMTBackendRegistry *registry, SolverBackendType type);

#ifdef __cplusplus
}
#endif

#endif /* LV00_SMT_BACKEND_H */
