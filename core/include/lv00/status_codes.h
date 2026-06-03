/**
 * @file status_codes.h
 * @brief Lv-00 统一返回状态码
 *
 * 提供标准化的函数返回类型 Lv00Status 及对应的状态码宏。
 * 所有公共 API 函数应返回 Lv00Status 类型以保持接口一致性。
 *
 * 使用方式:
 *   Lv00Status status = lv00_engine_create(&engine);
 *   if (status != LV00_OK) { ... }
 *
 * 与 error_codes.h 的关系:
 *   - error_codes.h 定义了 Lv00ErrorCode 枚举（细粒度错误码，约 100+ 条目）
 *   - status_codes.h 定义了 Lv00Status 类型（统一的返回类型）和精简状态码宏
 *   - Lv00ErrorCode 可隐式转换为 Lv00Status（两者均为 int）
 *
 * @version 1.0.0
 * @date 2026-05-24
 */

#ifndef LV00_STATUS_CODES_H
#define LV00_STATUS_CODES_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 统一函数返回类型
 *
 * 所有公共 API 函数应返回此类型。值为 0 表示成功，
 * 非 0 表示错误。具体错误可使用 Lv00ErrorCode 获取详情。
 */
typedef int Lv00Status;

/* ====================================================================
 * 状态码定义
 * ==================================================================== */

/** @brief 操作成功完成（与 error_codes.h 中的枚举值 LV00_OK 保持一致） */
#ifndef LV00_OK
#define LV00_OK                  0
#endif

/* ---- 通用错误 (1-19) ---- */

/** @brief 内存分配失败 */
#define LV00_ERR_MEMORY          1

/** @brief 无效参数（空指针、越界等） */
#define LV00_ERR_INVALID_ARG     2

/** @brief 未找到指定资源 */
#define LV00_ERR_NOT_FOUND       3

/** @brief 资源已存在（重复创建） */
#define LV00_ERR_ALREADY_EXISTS  4

/** @brief 操作不支持 */
#define LV00_ERR_UNSUPPORTED     5

/** @brief 操作超时 */
#define LV00_ERR_TIMEOUT         6

/** @brief 内部错误 */
#define LV00_ERR_INTERNAL        7

/** @brief 无效状态 */
#define LV00_ERR_INVALID_STATE   8

/** @brief 数值越界/溢出 */
#define LV00_ERR_OVERFLOW        9

/** @brief IO 错误 */
#define LV00_ERR_IO              10

/** @brief 解析错误 */
#define LV00_ERR_PARSE           11

/* ---- 约束图错误 (20-29) ---- */

/** @brief 节点冲突 */
#define LV00_ERR_NODE_CONFLICT      20

/** @brief 约束冲突 */
#define LV00_ERR_CONSTRAINT_CONFLICT 21

/** @brief 重复约束 */
#define LV00_ERR_CONSTRAINT_DUPLICATE 22

/** @brief 无效区域 */
#define LV00_ERR_INVALID_REGION     23

/** @brief 循环依赖 */
#define LV00_ERR_CYCLIC_DEPENDENCY  24

/* ---- 求解器错误 (30-39) ---- */

/** @brief 无解 */
#define LV00_ERR_SOLVER_NO_SOLUTION  30

/** @brief 无穷多解 */
#define LV00_ERR_SOLVER_INFINITE     31

/** @brief 过度约束 */
#define LV00_ERR_SOLVER_OVERCONSTRAINED 32

/** @brief Groebner 基计算失败 */
#define LV00_ERR_GROEBNER_FAILED     33

/* ---- 合一检查错误 (40-49) ---- */

/** @brief 合一失败 */
#define LV00_ERR_UNIFY_FAILED        40

/** @brief 类型不匹配 */
#define LV00_ERR_UNIFY_TYPE_MISMATCH 41

/* ---- 证明系统错误 (50-59) ---- */

/** @brief 无效证明 */
#define LV00_ERR_PROOF_INVALID       50

/** @brief 证明不完整 */
#define LV00_ERR_PROOF_INCOMPLETE    51

/** @brief 证明验证失败 */
#define LV00_ERR_PROOF_VERIFY_FAILED 52

/* ---- 函数块错误(60-69) ---- */

/** @brief 无效函数块 */
#define LV00_ERR_FUNC_BLOCK_INVALID  60

/** @brief 非确定性函数块 */
#define LV00_ERR_FUNC_BLOCK_NON_DET  61

/* ---- 预设系统错误 (70-79) ---- */

/** @brief 预设注册失败 */
#define LV00_ERR_PRESET_REGISTER     70

/** @brief 预设实例化失败 */
#define LV00_ERR_PRESET_INSTANTIATE  71

/* ====================================================================
 * 辅助函数声明
 * ==================================================================== */

/**
 * @brief 获取状态码的描述字符串
 *
 * @param status  状态码
 * @return 描述字符串（静态存储，勿释放）
 */
const char *lv00_status_to_string(Lv00Status status);

/**
 * @brief 判断状态码是否表示成功
 *
 * @param status  状态码
 * @return true 如果 status == LV00_OK
 */
static inline bool lv00_status_is_ok(Lv00Status status) {
    return status == LV00_OK;
}

#ifdef __cplusplus
}
#endif

#endif /* LV00_STATUS_CODES_H */
