/**
 * @file error_codes.h
 * @brief Lv-00 统一错误码系统
 *
 * @details 提供标准化的错误码定义、错误信息获取和错误处理宏。
 *          所有模块的错误码都应在此定义，确保错误处理的一致性和可追溯性。
 *
 * 【中文模块说明】
 * error_codes.h 定义了 Lv-00 系统的统一错误码体系，采用分层编号设计：
 * - 0: 成功 (lv_OK)
 * - 1-99: 通用系统错误（空指针、参数无效、超时、IO错误等）
 * - 100-199: 内存与资源错误（内存不足、分配失败、资源耗尽）
 * - 130-139: 解析器安全错误（输入过长、token超限、AST深度超限等）
 * - 200-299: 约束图相关错误（节点冲突、约束冲突、循环依赖等）
 * - 300-399: 符号坐标相关错误（坐标溢出、精度丢失、求值失败）
 * - 400-499: 求解器相关错误（无解、奇异矩阵、Groebner基计算失败）
 * - 500-599: 重写引擎相关错误（无匹配规则、重写循环、深度超限）
 * - 600-699: 合一检查相关错误（合一失败、发生检查失败、类型不匹配）
 * - 700-799: 函数块相关错误（无效函数块、非确定性、循环引用）
 * - 800-899: 类型系统相关错误（类型不匹配、推断失败、宇宙层级不一致）
 * - 900-999: 证明系统相关错误（无效证明、证明不完整、验证失败、熔断器跳闸）
 *
 * 此外提供：
 * - 错误信息获取函数（lv_error_string, lv_error_name, lv_error_category）
 * - 线程局部错误状态管理（lv_set_error, lv_get_last_error_code 等）
 * - 便捷错误处理宏（lv_CHECK_NULL, lv_CHECK, lv_CHECK_ALLOC 等）
 * - 错误码反向查找（lv_error_code_from_string）
 *
 * @author Lv-00 Project
 * @version 1.1.0
 */
#ifndef lv_ERROR_CODES_H
#define lv_ERROR_CODES_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stdbool.h>
#include <stddef.h>
#ifndef lv_PUBLIC_API
#define lv_PUBLIC_API
#endif
/* ============================================================
 * 错误码定义
 * ============================================================ */
/**
 * @brief Lv-00 系统错误码枚举
 *
 * 错误码采用分层设计：
 * - 0: 成功
 * - 1-99: 通用系统错误
 * - 100-199: 内存与资源错误
 * - 200-299: 约束图相关错误
 * - 300-399: 符号坐标相关错误
 * - 400-499: 求解器相关错误
 * - 500-599: 重写引擎相关错误
 * - 600-699: 合一检查相关错误
 * - 700-799: 函数块相关错误
 * - 800-899: 类型系统相关错误
 * - 900-999: 证明系统相关错误
 */
typedef enum {
    /* 成功 */
    lv_OK = 0,
    /* 通用系统错误 (1-99) */
    lv_ERROR_UNKNOWN = 1,             /**< 未知错误 */
    /* 16-69: 预留范围 / Reserved range */
    lv_ERROR_INVALID_PARAM = 2,       /**< 无效参数 */
    lv_ERROR_NULL_POINTER = 3,        /**< 空指针 */
    lv_ERROR_NOT_INITIALIZED = 4,     /**< 未初始化 */
    lv_ERROR_ALREADY_EXISTS = 5,      /**< 已存在 */
    lv_ERROR_NOT_FOUND = 6,           /**< 未找到 */
    lv_ERROR_UNSUPPORTED = 7,         /**< 不支持的操作 */
    lv_ERROR_OVERFLOW = 8,            /**< 数值溢出 */
    lv_ERROR_UNDERFLOW = 9,           /**< 数值下溢 */
    lv_ERROR_TIMEOUT = 10,            /**< 操作超时 */
    lv_ERROR_CANCELLED = 11,          /**< 操作被取消 */
    lv_ERROR_IO = 12,                 /**< IO错误 */
    lv_ERROR_PARSE = 13,              /**< 解析错误 */
    lv_ERROR_INVALID_STATE = 14,      /**< 无效状态 */
    lv_ERROR_INVALID_ARGUMENT = 15,   /**< 无效参数（字符串为空等）
                                              @deprecated 与 lv_ERROR_INVALID_PARAM 语义重叠，
                                              请使用 lv_ERROR_INVALID_PARAM 替代 */
    lv_ERROR_INDEX_OUT_OF_RANGE = 17, /**< 索引越界 */
    lv_ERROR_VALUE_OUT_OF_RANGE = 18, /**< 数值越界 */
    lv_ERROR_INTERNAL = 70,           /**< 内部错误 */
    /* 解析器安全错误 (130-139) */
    lv_ERROR_PARSER_NULL_INPUT = 130,     /**< 输入为NULL */
    lv_ERROR_PARSER_EMPTY_INPUT = 131,    /**< 输入为空字符串 */
    lv_ERROR_PARSER_INPUT_TOO_LONG = 132, /**< 输入长度超限 */
    lv_ERROR_PARSER_ILLEGAL_CHARS = 133,  /**< 输入含非法控制字符或null字节 */
    lv_ERROR_PARSER_TOO_MANY_TOKENS = 134,/**< token数量超限 */
    lv_ERROR_PARSER_DEPTH_EXCEEDED = 135, /**< AST深度超限 */
    lv_ERROR_PARSER_NODE_LIMIT = 136,     /**< AST节点数超限 */
    lv_ERROR_PARSER_TOKEN_TOO_LONG = 137, /**< token长度超限 */
    lv_ERROR_PARSER_POOL_EXHAUSTED = 138, /**< 内存池耗尽 */
    /* 内存与资源错误 (100-199) */
    lv_ERROR_OUT_OF_MEMORY = 100,      /**< 内存不足 */
    lv_ERROR_ALLOCATION_FAILED = 101,  /**< 内存分配失败 */
    lv_ERROR_RESOURCE_EXHAUSTED = 102, /**< 资源耗尽 */
    lv_ERROR_BUFFER_TOO_SMALL = 103,   /**< 缓冲区太小 */
    /* 约束图错误 (200-299) */
    lv_ERROR_NODE_CONFLICT = 200,        /**< 节点冲突 */
    lv_ERROR_NODE_NOT_FOUND = 201,       /**< 节点未找到 */
    lv_ERROR_CONSTRAINT_CONFLICT = 202,  /**< 约束冲突 */
    lv_ERROR_CONSTRAINT_DUPLICATE = 203, /**< 重复约束 */
    lv_ERROR_INVALID_REGION = 204,       /**< 无效区域 */
    lv_ERROR_INVALID_GEOM_TYPE = 205,    /**< 无效几何类型 */
    lv_ERROR_CYCLIC_DEPENDENCY = 206,    /**< 循环依赖 */
    lv_ERROR_GRAPH_CORRUPTED = 207,      /**< 图结构损坏 */
    /* 符号坐标错误 (300-399) */
    lv_ERROR_COORD_INVALID = 300,        /**< 无效坐标 */
    lv_ERROR_COORD_OVERFLOW = 301,       /**< 坐标溢出 */
    lv_ERROR_PRECISION_LOSS = 302,       /**< 精度丢失 */
    lv_ERROR_SYMBOLIC_EVAL_FAILED = 303, /**< 符号求值失败 */
    /* 求解器错误 (400-499) */
    lv_ERROR_SOLVER_NO_SOLUTION = 400,   /**< 无解 */
    lv_ERROR_SOLVER_INFINITE = 401,      /**< 无穷多解 */
    lv_ERROR_SOLVER_NUMERIC = 402,       /**< 数值计算错误 */
    lv_ERROR_SOLVER_SINGULAR = 403,      /**< 奇异矩阵 */
    lv_ERROR_SOLVER_NOT_CONVERGED = 404, /**< 未收敛 */
    lv_ERROR_GROEBNER_FAILED = 405,      /**< Gröbner基计算失败 */
    /* 重写引擎错误 (500-599) */
    lv_ERROR_REWRITE_NO_MATCH = 500, /**< 无匹配规则 */
    lv_ERROR_REWRITE_CYCLE = 501,    /**< 重写循环 */
    lv_ERROR_REWRITE_DEPTH = 502,    /**< 重写深度超限 */
    /* 合一检查错误 (600-699) */
    lv_ERROR_UNIFY_FAILED = 600,        /**< 合一失败 */
    lv_ERROR_UNIFY_OCCUR_CHECK = 601,   /**< 发生检查失败 */
    lv_ERROR_UNIFY_TYPE_MISMATCH = 602, /**< 类型不匹配 */
    /* 函数块错误 (700-799) */
    lv_ERROR_FUNC_BLOCK_INVALID = 700,           /**< 无效函数块 */
    lv_ERROR_FUNC_BLOCK_NON_DETERMINISTIC = 701, /**< 非确定性函数块 */
    lv_ERROR_FUNC_BLOCK_CIRCULAR = 702,          /**< 循环函数块 */
    lv_ERROR_FUNC_BLOCK_TYPE_ERROR = 703,        /**< 函数块类型错误 */
    /* 预设系统错误 (750-799) */
    lv_ERROR_PRESET_REGISTRATION_FAILED = 750,  /**< 预设注册失败 */
    lv_ERROR_PRESET_INSTANTIATION_FAILED = 751, /**< 预设实例化失败 */
    /* 类型系统错误 (800-899) */
    lv_ERROR_TYPE_MISMATCH = 800,         /**< 类型不匹配 */
    lv_ERROR_TYPE_INFERENCE_FAILED = 801, /**< 类型推断失败 */
    lv_ERROR_UNIVERSE_INCONSISTENT = 802, /**< 宇宙层级不一致 */
    /* 证明系统错误 (900-999) */
    lv_ERROR_PROOF_INVALID = 900,             /**< 无效证明 */
    lv_ERROR_PROOF_INCOMPLETE = 901,          /**< 证明不完整 */
    lv_ERROR_PROOF_VERIFICATION_FAILED = 902, /**< 证明验证失败 */
    lv_ERROR_CIRCUIT_OPEN = 903,              /**< 熔断器已跳闸（OPEN 态） */
    lv_ERROR_COUNT /**< 错误码总数，用于数组大小计算 */
} lvErrorCode;
/* ============================================================
 * 错误信息获取
 * ============================================================ */
/**
 * @brief 获取错误码对应的错误信息
 * @param code 错误码
 * @return 错误信息字符串（静态存储，无需释放）
 */
lv_PUBLIC_API const char *lv_error_string(lvErrorCode code);
/**
 * @brief 获取错误码的简短名称
 * @param code 错误码
 * @return 错误名称字符串（如 "lv_OK"）
 */
lv_PUBLIC_API const char *lv_error_name(lvErrorCode code);
/**
 * @brief 判断错误码是否表示成功
 * @param code 错误码
 * @return 成功返回true，否则false
 */
static inline bool lv_is_success(lvErrorCode code) {
    return code == lv_OK;
}
/**
 * @brief 判断错误码是否表示错误
 * @param code 错误码
 * @return 错误返回true，否则false
 */
static inline bool lv_is_error(lvErrorCode code) {
    return code != lv_OK;
}
/**
 * @brief 获取错误码所属的错误类别
 * @param code 错误码
 * @return 错误类别名称字符串
 */
lv_PUBLIC_API const char *lv_error_category(lvErrorCode code);
/* ============================================================
 * 线程局部错误状态
 * ============================================================ */
/**
 * @brief 获取当前线程的最后错误码
 * @return 最后错误码
 */
lv_PUBLIC_API lvErrorCode lv_get_last_error_code(void);
/**
 * @brief 获取当前线程的最后错误信息（详细描述）
 * @return 错误信息字符串（线程局部存储，无需释放）
 */
lv_PUBLIC_API const char *lv_get_last_error_message(void);
/**
 * @brief 获取当前线程的完整错误描述（包含错误码名称和信息）
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return 实际写入的字符数，失败返回-1
 */
lv_PUBLIC_API int lv_get_error_description(char *buf, size_t buf_size);
/**
 * @brief 设置当前线程的错误状态
 * @param code 错误码
 * @param format 格式化字符串（类似printf）
 * @param ... 可变参数
 */
lv_PUBLIC_API void lv_set_error(lvErrorCode code, const char *format, ...);
/**
 * @brief 设置当前线程的错误状态（带上下文信息）
 * @param code 错误码
 * @param file 源文件名
 * @param line 行号
 * @param func 函数名
 * @param format 格式化字符串
 * @param ... 可变参数
 */
lv_PUBLIC_API void lv_set_error_ctx(lvErrorCode code, const char *file, int line, const char *func, const char *format, ...);
/**
 * @brief 清除当前线程的错误状态
 */
lv_PUBLIC_API void lv_clear_error(void);
/**
 * @brief 运行时验证错误信息表的排序正确性
 *
 * 在 debug 构建中调用此函数，验证错误码查找表（g_error_table）
 * 是否按 lvErrorCode 枚举值严格升序排列。
 * 二分查找依赖此排序不变量。
 *
 * @return true 表排序正确，false 检测到违规（已通过 lv_set_error 记录详情）
 */
lv_PUBLIC_API bool lv_error_table_validate(void);
/**
 * @brief 从错误名称字符串反向查找错误码
 *
 * @param name 错误名称（如 "lv_OK"、"lv_ERROR_OUT_OF_MEMORY"）
 * @return 对应的错误码枚举值，未找到时返回 lv_ERROR_UNKNOWN
 */
lv_PUBLIC_API lvErrorCode lv_error_code_from_string(const char *name);
/* ============================================================
 * 便捷错误处理宏
 * ============================================================ */
/**
 * @brief 设置错误状态（带完整上下文信息，不自动返回）
 * @param code 错误码
 * @param fmt  格式化字符串
 * @param ...  可变参数
 *
 * 此宏仅设置错误状态，不执行 return/goto 等控制流操作，
 * 适用于需要在设置错误后继续执行清理逻辑的场景。
 */
#define lv_ERROR_SET(code, fmt, ...) lv_set_error_ctx((lvErrorCode)(code), __FILE__, __LINE__, __func__, (fmt), ##__VA_ARGS__)
/**
 * @brief 检查指针是否为NULL，如果是则设置错误并返回
 * @param ptr 要检查的指针
 * @param ret 返回值
 */
#define lv_CHECK_NULL(ptr, ret)                                                                          \
    do {                                                                                                   \
        if ((ptr) == NULL) {                                                                               \
            lv_set_error_ctx(lv_ERROR_NULL_POINTER, __FILE__, __LINE__, __func__, "空指针: %s", #ptr); \
            return (ret);                                                                                  \
        }                                                                                                  \
    } while (0)
/**
 * @brief 检查指针是否为NULL，如果是则设置错误并无返回值返回（用于 void 函数）
 * @param ptr 要检查的指针
 */
#define lv_CHECK_NULL_VOID(ptr)                                                                          \
    do {                                                                                                   \
        if ((ptr) == NULL) {                                                                               \
            lv_set_error_ctx(lv_ERROR_NULL_POINTER, __FILE__, __LINE__, __func__, "空指针: %s", #ptr); \
            return;                                                                                        \
        }                                                                                                  \
    } while (0)
/**
 * @brief 检查条件，如果不满足则设置错误并返回
 * @param cond 条件表达式
 * @param err_code 错误码
 * @param ret 返回值
 * @param msg 错误消息
 */
#define lv_CHECK(cond, err_code, ret, msg)                                                      \
    do {                                                                                          \
        if (!(cond)) {                                                                            \
            lv_set_error_ctx((err_code), __FILE__, __LINE__, __func__, "%s: %s", (msg), #cond); \
            return (ret);                                                                         \
        }                                                                                         \
    } while (0)
/**
 * @brief 检查内存分配是否成功
 * @param ptr 分配的指针
 * @param ret 返回值
 */
#define lv_CHECK_ALLOC(ptr, ret)                                                                                \
    do {                                                                                                          \
        if ((ptr) == NULL) {                                                                                      \
            lv_set_error_ctx(lv_ERROR_OUT_OF_MEMORY, __FILE__, __LINE__, __func__, "内存分配失败: %s", #ptr); \
            return (ret);                                                                                         \
        }                                                                                                         \
    } while (0)
/**
 * @brief 检查索引是否在有效范围内
 * @param idx 索引值
 * @param max 最大值（不包含）
 * @param ret 返回值
 */
#define lv_CHECK_INDEX(idx, max, ret)                                                          \
    do {                                                                                         \
        if ((idx) < 0 || (idx) >= (max)) {                                                       \
            lv_set_error_ctx(lv_ERROR_INVALID_PARAM, __FILE__, __LINE__, __func__,           \
                               "索引越界: %s=%d, 有效范围[0, %s=%d]", #idx, (idx), #max, (max)); \
            return (ret);                                                                        \
        }                                                                                        \
    } while (0)
/**
 * @brief 检查范围是否有效（min <= val <= max）
 * @param val 要检查的值
 * @param min 最小值
 * @param max 最大值
 * @param ret 返回值
 */
#define lv_CHECK_RANGE(val, min, max, ret)                                                                        \
    do {                                                                                                            \
        if ((val) < (min) || (val) > (max)) {                                                                       \
            lv_set_error_ctx(lv_ERROR_INVALID_PARAM, __FILE__, __LINE__, __func__,                              \
                               "值超出范围: %s=%d, 有效范围[%s=%d, %s=%d]", #val, (val), #min, (min), #max, (max)); \
            return (ret);                                                                                           \
        }                                                                                                           \
    } while (0)

/* ============================================================
 * lvResult 结果类型
 * ============================================================ */
/**
 * @brief lvResult 结构体 - 统一的结果返回类型
 *
 * 封装错误码和返回值，用于需要同时返回成功/失败状态和数据的场景。
 * 可选的 value 字段可用于传递计算结果。
 */
typedef struct {
    lvErrorCode code;      /**< 错误码 */
    int           value;     /**< 返回值（可选，仅 code == lv_OK 时有效） */
} lvResult;

/* ============================================================
 * 便捷宏定义
 * ============================================================ */
/**
 * @brief 创建成功的 lvResult
 * @param val 返回值
 */
#define lv_OK(val) ((lvResult){ .code = lv_OK, .value = (val) })

/**
 * @brief 创建失败的 lvResult
 * @param err 错误码
 */
#define lv_ERR(err) ((lvResult){ .code = (err), .value = 0 })

/**
 * @brief 传播错误 - 如果 result 包含错误，则立即返回该错误结果
 *
 * 用法示例：
 *   lvResult result = some_operation();
 *   lv_PROPAGATE(result);
 *
 * @param result_expr 要检查的 lvResult 表达式
 */
#define lv_PROPAGATE(result_expr)                                                \
    do {                                                                           \
        lvResult _lv_tmp_res = (result_expr);                                  \
        if ((_lv_tmp_res).code != lv_OK) {                                     \
            return _lv_tmp_res;                                                  \
        }                                                                          \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* lv_ERROR_CODES_H */