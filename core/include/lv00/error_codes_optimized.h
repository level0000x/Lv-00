/**
 * @file error_codes_optimized.h
 * @brief Lv-00 错误码定义 (优化版 v3.5.1)
 * 
 * 本文件定义了 Lv-00 核心库使用的所有错误码、状态码和常量。
 * 错误码采用分层设计，便于错误处理和调试。
 * 
 * 错误码结构：
 *   - 通用错误 (0 ~ -99)
 *   - 内存错误 (-100 ~ -199)
 *   - 参数错误 (-200 ~ -299)
 *   - 图操作错误 (-300 ~ -399)
 *   - 约束错误 (-400 ~ -499)
 *   - 求解错误 (-500 ~ -599)
 *   - 证明错误 (-600 ~ -699)
 *   - 引擎错误 (-700 ~ -799)
 *   - 模块错误 (-800 ~ -899)
 *   - IO错误 (-900 ~ -999)
 * 
 * 版本：3.5.1 (优化版)
 * 作者：Lv-00 开发团队
 */

#ifndef LV00_ERROR_CODES_OPTIMIZED_H
#define LV00_ERROR_CODES_OPTIMIZED_H

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================
 * 通用错误码 / General Error Codes
 * ============================================================= */

/** 成功 */
#define LV00_OK                         0
/** 通用错误 */
#define LV00_ERROR                     -1
/** 未实现 */
#define LV00_NOT_IMPLEMENTED           -2
/** 内部错误 */
#define LV00_INTERNAL_ERROR            -3
/** 未知错误 */
#define LV00_UNKNOWN_ERROR             -4
/** 操作被取消 */
#define LV00_CANCELLED                 -5
/** 操作超时 */
#define LV00_TIMEOUT                   -6

/* =============================================================
 * 内存错误码 / Memory Error Codes (-100 ~ -199)
 * ============================================================= */

/** 内存不足 */
#define LV00_OUT_OF_MEMORY             -100
/** 内存分配失败 */
#define LV00_ALLOC_FAILED              -101
/** 内存释放失败 */
#define LV00_FREE_FAILED               -102
/** 内存越界访问 */
#define LV00_MEMORY_OVERFLOW           -103
/** 内存泄漏检测 */
#define LV00_MEMORY_LEAK               -104
/** 重复释放 */
#define LV00_DOUBLE_FREE               -105
/** 无效内存地址 */
#define LV00_INVALID_MEMORY            -106

/* =============================================================
 * 参数错误码 / Argument Error Codes (-200 ~ -299)
 * ============================================================= */

/** 无效参数 */
#define LV00_INVALID_ARGUMENT          -200
/** 空指针 */
#define LV00_NULL_POINTER              -201
/** 参数越界 */
#define LV00_ARGUMENT_OUT_OF_RANGE     -202
/** 类型不匹配 */
#define LV00_TYPE_MISMATCH             -203
/** 缓冲区过小 */
#define LV00_BUFFER_TOO_SMALL          -204
/** 字符串过长 */
#define LV00_STRING_TOO_LONG           -205
/** 无效句柄 */
#define LV00_INVALID_HANDLE            -206

/* =============================================================
 * 图操作错误码 / Graph Error Codes (-300 ~ -399)
 * ============================================================= */

/** 节点不存在 */
#define LV00_NODE_NOT_FOUND            -300
/** 节点已存在 */
#define LV00_NODE_ALREADY_EXISTS       -301
/** 无效节点类型 */
#define LV00_INVALID_NODE_TYPE         -302
/** 节点正在使用中 */
#define LV00_NODE_IN_USE               -303
/** 图循环依赖 */
#define LV00_GRAPH_CYCLE               -304
/** 图不连通 */
#define LV00_GRAPH_DISCONNECTED        -305
/** 图为空 */
#define LV00_GRAPH_EMPTY               -306
/** 图已满 */
#define LV00_GRAPH_FULL                -307

/* =============================================================
 * 约束错误码 / Constraint Error Codes (-400 ~ -499)
 * ============================================================= */

/** 约束不存在 */
#define LV00_CONSTRAINT_NOT_FOUND      -400
/** 约束已存在 */
#define LV00_CONSTRAINT_ALREADY_EXISTS -401
/** 无效约束类型 */
#define LV00_INVALID_CONSTRAINT_TYPE   -402
/** 约束冲突 */
#define LV00_CONSTRAINT_CONFLICT       -403
/** 约束冗余 */
#define LV00_CONSTRAINT_REDUNDANT      -404
/** 约束不足（欠约束） */
#define LV00_UNDER_CONSTRAINED         -405
/** 约束过度（过约束） */
#define LV00_OVER_CONSTRAINED          -406
/** 约束不可满足 */
#define LV00_CONSTRAINT_UNSATISFIABLE  -407
/** 约束传播失败 */
#define LV00_CONSTRAINT_PROPAGATION_FAILED -408

/* =============================================================
 * 求解错误码 / Solver Error Codes (-500 ~ -599)
 * ============================================================= */

/** 求解失败 */
#define LV00_SOLVER_FAILED             -500
/** 求解发散 */
#define LV00_SOLVER_DIVERGED           -501
/** 求解收敛失败 */
#define LV00_SOLVER_NO_CONVERGENCE     -502
/** 矩阵奇异 */
#define LV00_MATRIX_SINGULAR           -503
/** 矩阵病态 */
#define LV00_MATRIX_ILL_CONDITIONED    -504
/** 数值溢出 */
#define LV00_NUMERIC_OVERFLOW          -505
/** 数值下溢 */
#define LV00_NUMERIC_UNDERFLOW         -506
/** 迭代次数超限 */
#define LV00_MAX_ITERATIONS_EXCEEDED   -507
/** 求解器未初始化 */
#define LV00_SOLVER_NOT_INITIALIZED    -508

/* =============================================================
 * 证明错误码 / Proof Error Codes (-600 ~ -699)
 * ============================================================= */

/** 证明验证失败 */
#define LV00_PROOF_VERIFICATION_FAILED -600
/** 证明不完整 */
#define LV00_PROOF_INCOMPLETE          -601
/** 证明步骤无效 */
#define LV00_INVALID_PROOF_STEP        -602
/** 合一失败 */
#define LV00_UNIFICATION_FAILED        -603
/** 模式不匹配 */
#define LV00_PATTERN_MISMATCH          -604
/** 公理不适用 */
#define LV00_AXIOM_NOT_APPLICABLE      -605
/** 定理不成立 */
#define LV00_THEOREM_FALSE             -606

/* =============================================================
 * 引擎错误码 / Engine Error Codes (-700 ~ -799)
 * ============================================================= */

/** 引擎未初始化 */
#define LV00_ENGINE_NOT_INITIALIZED    -700
/** 引擎已销毁 */
#define LV00_ENGINE_DESTROYED          -701
/** 引擎状态无效 */
#define LV00_INVALID_ENGINE_STATE      -702
/** 引擎忙 */
#define LV00_ENGINE_BUSY               -703
/** 重写失败 */
#define LV00_REWRITE_FAILED            -704
/** 重写规则无效 */
#define LV00_INVALID_REWRITE_RULE      -705
/** 重写步数超限 */
#define LV00_REWRITE_STEP_LIMIT        -706
/** 工作流执行失败 */
#define LV00_WORKFLOW_FAILED           -707

/* =============================================================
 * 模块错误码 / Module Error Codes (-800 ~ -899)
 * ============================================================= */

/** 模块加载失败 */
#define LV00_MODULE_LOAD_FAILED        -800
/** 模块未找到 */
#define LV00_MODULE_NOT_FOUND          -801
/** 模块版本不兼容 */
#define LV00_MODULE_VERSION_MISMATCH   -802
/** 模块符号未找到 */
#define LV00_MODULE_SYMBOL_NOT_FOUND   -803
/** 模块依赖缺失 */
#define LV00_MODULE_DEPENDENCY_MISSING -804
/** 模块循环依赖 */
#define LV00_MODULE_CIRCULAR_DEPENDENCY -805
/** 公理包加载失败 */
#define LV00_AXIOM_PACKAGE_LOAD_FAILED -810
/** 公理包无效 */
#define LV00_INVALID_AXIOM_PACKAGE     -811

/* =============================================================
 * IO错误码 / IO Error Codes (-900 ~ -999)
 * ============================================================= */

/** 文件未找到 */
#define LV00_FILE_NOT_FOUND            -900
/** 文件读取失败 */
#define LV00_FILE_READ_FAILED          -901
/** 文件写入失败 */
#define LV00_FILE_WRITE_FAILED         -902
/** 文件格式无效 */
#define LV00_INVALID_FILE_FORMAT       -903
/** 解析错误 */
#define LV00_PARSE_ERROR               -904
/** 序列化错误 */
#define LV00_SERIALIZATION_ERROR       -905
/** 网络错误 */
#define LV00_NETWORK_ERROR             -906

/* =============================================================
 * 状态码别名（向后兼容）/ Status Code Aliases (Backward Compatibility)
 * ============================================================= */

/* 通用状态 */
#define OK                              LV00_OK
#define ERROR                           LV00_ERROR

/* 引擎状态 */
#define ENGINE_OK                       LV00_OK
#define ENGINE_OUT_OF_MEMORY            LV00_OUT_OF_MEMORY
#define ENGINE_INVALID_STATE            LV00_INVALID_ENGINE_STATE
#define ENGINE_CONSTRAINT_CONFLICT      LV00_CONSTRAINT_CONFLICT
#define ENGINE_MODULE_ERROR             LV00_MODULE_LOAD_FAILED

/* 求解结果 */
#define ENGINE_SOLVE_OK                 LV00_OK
#define ENGINE_SOLVE_CONFLICT           LV00_CONSTRAINT_CONFLICT
#define ENGINE_SOLVE_TIMEOUT            LV00_TIMEOUT
#define ENGINE_SOLVE_ERROR              LV00_SOLVER_FAILED

/* 图操作状态 */
#define ADD_NODE_OK                     LV00_OK
#define REMOVE_NODE_OK                  LV00_OK
#define ADD_CONSTRAINT_OK               LV00_OK
#define REMOVE_CONSTRAINT_OK            LV00_OK

/* 合一结果 */
#define UNIFY_OK                        LV00_OK
#define UNIFY_FAILED                    LV00_UNIFICATION_FAILED
#define UNIFY_TYPE_MISMATCH             LV00_TYPE_MISMATCH

/* =============================================================
 * 几何类型常量 / Geometry Type Constants
 * ============================================================= */

/** 几何节点类型 */
#define GEOM_POINT                      0
#define GEOM_LINE_SEGMENT               1
#define GEOM_PORT                       2
#define GEOM_FUNCTION_BLOCK             3
#define GEOM_REGION                     4
#define GEOM_CIRCLE                     5
#define GEOM_ARC                        6

/** 端口方向 */
#define PORT_INPUT                      0
#define PORT_OUTPUT                     1
#define PORT_BIDIR                      2

/** 约束类型 */
#define CONSTRAINT_INCIDENCE            0
#define CONSTRAINT_BETWEENNESS          1
#define CONSTRAINT_INTERSECTION         2
#define CONSTRAINT_CONTAINMENT          3
#define CONSTRAINT_CONNECTION           4
#define CONSTRAINT_DISTANCE             5
#define CONSTRAINT_ANGLE                6
#define CONSTRAINT_PARALLEL             7
#define CONSTRAINT_PERPENDICULAR        8
#define CONSTRAINT_EQUAL_LENGTH         9
#define CONSTRAINT_COLLINEAR            10

/** 约束强度 */
#define CONSTRAINT_WEAK                 0
#define CONSTRAINT_NORMAL               1
#define CONSTRAINT_STRONG               2
#define CONSTRAINT_REQUIRED             3

/* =============================================================
 * 日志级别 / Log Levels
 * ============================================================= */

#define LOG_LEVEL_DEBUG                 0
#define LOG_LEVEL_INFO                  1
#define LOG_LEVEL_WARN                  2
#define LOG_LEVEL_ERROR                 3
#define LOG_LEVEL_FATAL                 4

/* =============================================================
 * 限制常量 / Limit Constants
 * ============================================================= */

/** 最大节点数 */
#define LV00_MAX_NODES                  100000
/** 最大约束数 */
#define LV00_MAX_CONSTRAINTS            500000
/** 最大图深度 */
#define LV00_MAX_GRAPH_DEPTH            1000
/** 默认重写步数限制 */
#define LV00_DEFAULT_REWRITE_STEPS      1000
/** 默认求解步数限制 */
#define LV00_DEFAULT_SOLVE_STEPS        1000
/** 默认求解超时（毫秒） */
#define LV00_DEFAULT_SOLVE_TIMEOUT_MS   30000

/* =============================================================
 * 工具宏 / Utility Macros
 * ============================================================= */

/**
 * @brief 检查状态码是否为成功
 */
#define LV00_IS_SUCCESS(status)         ((status) == LV00_OK)

/**
 * @brief 检查状态码是否为错误
 */
#define LV00_IS_ERROR(status)           ((status) < LV00_OK)

/**
 * @brief 获取错误类别
 */
#define LV00_ERROR_CATEGORY(status)     ((status) / 100 * 100)

/**
 * @brief 错误码是否为指定类别
 */
#define LV00_IS_ERROR_CATEGORY(status, category) \
    ((status) >= (category) && (status) > (category) - 100)

#ifdef __cplusplus
}
#endif

#endif /* LV00_ERROR_CODES_OPTIMIZED_H */
