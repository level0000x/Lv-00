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
#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
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
/**
 * @brief Lv-00 错误码 X-macro 列表 —— 错误码体系的单一事实来源
 *
 * 条目形态：x(枚举名, 数值, 名称字符串, 中文消息, 类别)
 *
 * 由本列表自动生成：
 * - lvErrorCode 枚举（数值显式赋值，与历史值严格一致，含跳号与预留段）
 * - error_codes.c 的 g_error_table（生成顺序即列表顺序 = 枚举值升序，
 *   find_error_info() 二分查找的排序不变量由编译期宏展开保证，
 *   不再需要运行时排序校验 lv_error_table_validate 的扫描）
 *
 * 约束：
 * - 条目必须按数值升序排列（错误表生成依赖此顺序）
 * - 新增错误码只需在此追加一条，枚举与错误表自动同步
 * - lv_ERROR_COUNT 为哨兵成员（错误码总数），不属于错误码，不在此列出
 */
#define LV_ERROR_CODES_X(x) \
    /* 成功 */ \
    x(lv_OK, 0, "lv_OK", "操作成功", LV_CAT_OK) \
    /* 通用系统错误 (1-99)，16/19-69 为预留 */ \
    x(lv_ERROR_UNKNOWN, 1, "lv_ERROR_UNKNOWN", "未知错误", LV_CAT_SYSTEM) \
    x(lv_ERROR_INVALID_PARAM, 2, "lv_ERROR_INVALID_PARAM", "无效参数", LV_CAT_SYSTEM) \
    x(lv_ERROR_NULL_POINTER, 3, "lv_ERROR_NULL_POINTER", "空指针", LV_CAT_SYSTEM) \
    x(lv_ERROR_NOT_INITIALIZED, 4, "lv_ERROR_NOT_INITIALIZED", "未初始化", LV_CAT_SYSTEM) \
    x(lv_ERROR_ALREADY_EXISTS, 5, "lv_ERROR_ALREADY_EXISTS", "已存在", LV_CAT_SYSTEM) \
    x(lv_ERROR_NOT_FOUND, 6, "lv_ERROR_NOT_FOUND", "未找到", LV_CAT_SYSTEM) \
    x(lv_ERROR_UNSUPPORTED, 7, "lv_ERROR_UNSUPPORTED", "不支持的操作", LV_CAT_SYSTEM) \
    x(lv_ERROR_OVERFLOW, 8, "lv_ERROR_OVERFLOW", "数值溢出", LV_CAT_SYSTEM) \
    x(lv_ERROR_UNDERFLOW, 9, "lv_ERROR_UNDERFLOW", "数值下溢", LV_CAT_SYSTEM) \
    x(lv_ERROR_TIMEOUT, 10, "lv_ERROR_TIMEOUT", "操作超时", LV_CAT_SYSTEM) \
    x(lv_ERROR_CANCELLED, 11, "lv_ERROR_CANCELLED", "操作被取消", LV_CAT_SYSTEM) \
    x(lv_ERROR_IO, 12, "lv_ERROR_IO", "IO错误", LV_CAT_SYSTEM) \
    x(lv_ERROR_PARSE, 13, "lv_ERROR_PARSE", "解析错误", LV_CAT_SYSTEM) \
    x(lv_ERROR_INVALID_STATE, 14, "lv_ERROR_INVALID_STATE", "无效状态", LV_CAT_SYSTEM) \
    x(lv_ERROR_INDEX_OUT_OF_RANGE, 17, "lv_ERROR_INDEX_OUT_OF_RANGE", "索引越界", LV_CAT_SYSTEM) \
    x(lv_ERROR_VALUE_OUT_OF_RANGE, 18, "lv_ERROR_VALUE_OUT_OF_RANGE", "数值越界", LV_CAT_SYSTEM) \
    x(lv_ERROR_INTERNAL, 70, "lv_ERROR_INTERNAL", "内部错误", LV_CAT_SYSTEM) \
    /* 内存与资源错误 (100-199) */ \
    x(lv_ERROR_OUT_OF_MEMORY, 100, "lv_ERROR_OUT_OF_MEMORY", "内存不足", LV_CAT_MEMORY) \
    x(lv_ERROR_ALLOCATION_FAILED, 101, "lv_ERROR_ALLOCATION_FAILED", "内存分配失败", LV_CAT_MEMORY) \
    x(lv_ERROR_RESOURCE_EXHAUSTED, 102, "lv_ERROR_RESOURCE_EXHAUSTED", "资源耗尽", LV_CAT_MEMORY) \
    x(lv_ERROR_BUFFER_TOO_SMALL, 103, "lv_ERROR_BUFFER_TOO_SMALL", "缓冲区太小", LV_CAT_MEMORY) \
    /* 解析器安全错误 (130-139) */ \
    x(lv_ERROR_PARSER_NULL_INPUT, 130, "lv_ERROR_PARSER_NULL_INPUT", "解析器输入为NULL", LV_CAT_PARSER) \
    x(lv_ERROR_PARSER_EMPTY_INPUT, 131, "lv_ERROR_PARSER_EMPTY_INPUT", "解析器输入为空", LV_CAT_PARSER) \
    x(lv_ERROR_PARSER_INPUT_TOO_LONG, 132, "lv_ERROR_PARSER_INPUT_TOO_LONG", "解析器输入长度超限", LV_CAT_PARSER) \
    x(lv_ERROR_PARSER_ILLEGAL_CHARS, 133, "lv_ERROR_PARSER_ILLEGAL_CHARS", "解析器输入含非法字符", LV_CAT_PARSER) \
    x(lv_ERROR_PARSER_TOO_MANY_TOKENS, 134, "lv_ERROR_PARSER_TOO_MANY_TOKENS", "解析器token数量超限", LV_CAT_PARSER) \
    x(lv_ERROR_PARSER_DEPTH_EXCEEDED, 135, "lv_ERROR_PARSER_DEPTH_EXCEEDED", "AST深度超限", LV_CAT_PARSER) \
    x(lv_ERROR_PARSER_NODE_LIMIT, 136, "lv_ERROR_PARSER_NODE_LIMIT", "AST节点数超限", LV_CAT_PARSER) \
    x(lv_ERROR_PARSER_TOKEN_TOO_LONG, 137, "lv_ERROR_PARSER_TOKEN_TOO_LONG", "解析器token长度超限", LV_CAT_PARSER) \
    x(lv_ERROR_PARSER_POOL_EXHAUSTED, 138, "lv_ERROR_PARSER_POOL_EXHAUSTED", "解析器内存池耗尽", LV_CAT_PARSER) \
    /* 约束图错误 (200-299) */ \
    x(lv_ERROR_NODE_CONFLICT, 200, "lv_ERROR_NODE_CONFLICT", "节点冲突", LV_CAT_GRAPH) \
    x(lv_ERROR_NODE_NOT_FOUND, 201, "lv_ERROR_NODE_NOT_FOUND", "节点未找到", LV_CAT_GRAPH) \
    x(lv_ERROR_CONSTRAINT_CONFLICT, 202, "lv_ERROR_CONSTRAINT_CONFLICT", "约束冲突", LV_CAT_GRAPH) \
    x(lv_ERROR_CONSTRAINT_DUPLICATE, 203, "lv_ERROR_CONSTRAINT_DUPLICATE", "重复约束", LV_CAT_GRAPH) \
    x(lv_ERROR_INVALID_REGION, 204, "lv_ERROR_INVALID_REGION", "无效区域", LV_CAT_GRAPH) \
    x(lv_ERROR_INVALID_GEOM_TYPE, 205, "lv_ERROR_INVALID_GEOM_TYPE", "无效几何类型", LV_CAT_GRAPH) \
    x(lv_ERROR_CYCLIC_DEPENDENCY, 206, "lv_ERROR_CYCLIC_DEPENDENCY", "循环依赖", LV_CAT_GRAPH) \
    x(lv_ERROR_GRAPH_CORRUPTED, 207, "lv_ERROR_GRAPH_CORRUPTED", "图结构损坏", LV_CAT_GRAPH) \
    /* 符号坐标错误 (300-399) */ \
    x(lv_ERROR_COORD_INVALID, 300, "lv_ERROR_COORD_INVALID", "无效坐标", LV_CAT_COORD) \
    x(lv_ERROR_COORD_OVERFLOW, 301, "lv_ERROR_COORD_OVERFLOW", "坐标溢出", LV_CAT_COORD) \
    x(lv_ERROR_PRECISION_LOSS, 302, "lv_ERROR_PRECISION_LOSS", "精度丢失", LV_CAT_COORD) \
    x(lv_ERROR_SYMBOLIC_EVAL_FAILED, 303, "lv_ERROR_SYMBOLIC_EVAL_FAILED", "符号求值失败", LV_CAT_COORD) \
    /* 求解器错误 (400-499) */ \
    x(lv_ERROR_SOLVER_NO_SOLUTION, 400, "lv_ERROR_SOLVER_NO_SOLUTION", "方程无解", LV_CAT_SOLVER) \
    x(lv_ERROR_SOLVER_INFINITE, 401, "lv_ERROR_SOLVER_INFINITE", "无穷多解", LV_CAT_SOLVER) \
    x(lv_ERROR_SOLVER_NUMERIC, 402, "lv_ERROR_SOLVER_NUMERIC", "数值计算错误", LV_CAT_SOLVER) \
    x(lv_ERROR_SOLVER_SINGULAR, 403, "lv_ERROR_SOLVER_SINGULAR", "奇异矩阵", LV_CAT_SOLVER) \
    x(lv_ERROR_SOLVER_NOT_CONVERGED, 404, "lv_ERROR_SOLVER_NOT_CONVERGED", "求解未收敛", LV_CAT_SOLVER) \
    x(lv_ERROR_GROEBNER_FAILED, 405, "lv_ERROR_GROEBNER_FAILED", "Gröbner基计算失败", LV_CAT_SOLVER) \
    /* 重写引擎错误 (500-599) */ \
    x(lv_ERROR_REWRITE_NO_MATCH, 500, "lv_ERROR_REWRITE_NO_MATCH", "无匹配规则", LV_CAT_REWRITE) \
    x(lv_ERROR_REWRITE_CYCLE, 501, "lv_ERROR_REWRITE_CYCLE", "重写循环", LV_CAT_REWRITE) \
    x(lv_ERROR_REWRITE_DEPTH, 502, "lv_ERROR_REWRITE_DEPTH", "重写深度超限", LV_CAT_REWRITE) \
    /* 合一检查错误 (600-699) */ \
    x(lv_ERROR_UNIFY_FAILED, 600, "lv_ERROR_UNIFY_FAILED", "合一失败", LV_CAT_UNIFY) \
    x(lv_ERROR_UNIFY_OCCUR_CHECK, 601, "lv_ERROR_UNIFY_OCCUR_CHECK", "发生检查失败", LV_CAT_UNIFY) \
    x(lv_ERROR_UNIFY_TYPE_MISMATCH, 602, "lv_ERROR_UNIFY_TYPE_MISMATCH", "类型不匹配", LV_CAT_UNIFY) \
    /* 函数块错误 (700-799) */ \
    x(lv_ERROR_FUNC_BLOCK_INVALID, 700, "lv_ERROR_FUNC_BLOCK_INVALID", "无效函数块", LV_CAT_FBLOCK) \
    x(lv_ERROR_FUNC_BLOCK_NON_DETERMINISTIC, 701, "lv_ERROR_FUNC_BLOCK_NON_DETERMINISTIC", "非确定性函数块", LV_CAT_FBLOCK) \
    x(lv_ERROR_FUNC_BLOCK_CIRCULAR, 702, "lv_ERROR_FUNC_BLOCK_CIRCULAR", "循环函数块", LV_CAT_FBLOCK) \
    x(lv_ERROR_FUNC_BLOCK_TYPE_ERROR, 703, "lv_ERROR_FUNC_BLOCK_TYPE_ERROR", "函数块类型错误", LV_CAT_FBLOCK) \
    /* 预设系统错误 (750-799) */ \
    x(lv_ERROR_PRESET_REGISTRATION_FAILED, 750, "lv_ERROR_PRESET_REGISTRATION_FAILED", "预设注册失败", LV_CAT_PRESET) \
    x(lv_ERROR_PRESET_INSTANTIATION_FAILED, 751, "lv_ERROR_PRESET_INSTANTIATION_FAILED", "预设实例化失败", LV_CAT_PRESET) \
    /* 类型系统错误 (800-899) */ \
    x(lv_ERROR_TYPE_MISMATCH, 800, "lv_ERROR_TYPE_MISMATCH", "类型不匹配", LV_CAT_TYPESYS) \
    x(lv_ERROR_TYPE_INFERENCE_FAILED, 801, "lv_ERROR_TYPE_INFERENCE_FAILED", "类型推断失败", LV_CAT_TYPESYS) \
    x(lv_ERROR_UNIVERSE_INCONSISTENT, 802, "lv_ERROR_UNIVERSE_INCONSISTENT", "宇宙层级不一致", LV_CAT_TYPESYS) \
    /* 证明系统错误 (900-999) */ \
    x(lv_ERROR_PROOF_INVALID, 900, "lv_ERROR_PROOF_INVALID", "无效证明", LV_CAT_PROOF) \
    x(lv_ERROR_PROOF_INCOMPLETE, 901, "lv_ERROR_PROOF_INCOMPLETE", "证明不完整", LV_CAT_PROOF) \
    x(lv_ERROR_PROOF_VERIFICATION_FAILED, 902, "lv_ERROR_PROOF_VERIFICATION_FAILED", "证明验证失败", LV_CAT_PROOF) \
    x(lv_ERROR_CIRCUIT_OPEN, 903, "lv_ERROR_CIRCUIT_OPEN", "熔断器已跳闸（OPEN态）", LV_CAT_PROOF)

/**
 * @brief 错误类别体系 —— 类别键清单
 *
 * 类别键：编译期 token，作为宏参数传入 LV_ERROR_CODES_X（第 5 字段）与
 * LV_ERROR_CATEGORY_RANGES_X（第 3 字段）两侧。
 * 类别短名（每码类别，lv_error_category() 返回值）与长名（粗粒度区间类别，
 * lv_status_category() 返回值）在下方 LV_EC_CAT_*_<键> 映射宏中各自只定义
 * 一次，经 LV_EC_CAT_SHORT(键) / LV_EC_CAT_LONG(键) 拼接引用，无需手工同步
 * （取代此前"两侧必须同步"的约定）。
 */
#define LV_ERROR_CATEGORIES_X(x) \
    x(LV_CAT_OK) \
    x(LV_CAT_SYSTEM) \
    x(LV_CAT_MEMORY) \
    x(LV_CAT_PARSER) \
    x(LV_CAT_GRAPH) \
    x(LV_CAT_COORD) \
    x(LV_CAT_SOLVER) \
    x(LV_CAT_REWRITE) \
    x(LV_CAT_UNIFY) \
    x(LV_CAT_FBLOCK) \
    x(LV_CAT_PRESET) \
    x(LV_CAT_TYPESYS) \
    x(LV_CAT_PROOF)

/* 类别短名/长名映射宏（每个名称只定义一次，两侧共用） */
#define LV_EC_CAT_SHORT_LV_CAT_OK        "成功"
#define LV_EC_CAT_LONG_LV_CAT_OK         "成功"
#define LV_EC_CAT_SHORT_LV_CAT_SYSTEM    "系统"
#define LV_EC_CAT_LONG_LV_CAT_SYSTEM     "通用系统错误"
#define LV_EC_CAT_SHORT_LV_CAT_MEMORY    "内存"
#define LV_EC_CAT_LONG_LV_CAT_MEMORY     "内存与资源错误"
#define LV_EC_CAT_SHORT_LV_CAT_PARSER    "解析器"
#define LV_EC_CAT_LONG_LV_CAT_PARSER     "解析器安全错误"
#define LV_EC_CAT_SHORT_LV_CAT_GRAPH     "约束图"
#define LV_EC_CAT_LONG_LV_CAT_GRAPH      "约束图错误"
#define LV_EC_CAT_SHORT_LV_CAT_COORD     "符号坐标"
#define LV_EC_CAT_LONG_LV_CAT_COORD      "符号坐标错误"
#define LV_EC_CAT_SHORT_LV_CAT_SOLVER    "求解器"
#define LV_EC_CAT_LONG_LV_CAT_SOLVER     "求解器错误"
#define LV_EC_CAT_SHORT_LV_CAT_REWRITE   "重写引擎"
#define LV_EC_CAT_LONG_LV_CAT_REWRITE    "重写引擎错误"
#define LV_EC_CAT_SHORT_LV_CAT_UNIFY     "合一检查"
#define LV_EC_CAT_LONG_LV_CAT_UNIFY      "合一检查错误"
#define LV_EC_CAT_SHORT_LV_CAT_FBLOCK    "函数块"
#define LV_EC_CAT_LONG_LV_CAT_FBLOCK     "函数块错误"
#define LV_EC_CAT_SHORT_LV_CAT_PRESET    "预设系统"
#define LV_EC_CAT_LONG_LV_CAT_PRESET     "预设系统错误"
#define LV_EC_CAT_SHORT_LV_CAT_TYPESYS   "类型系统"
#define LV_EC_CAT_LONG_LV_CAT_TYPESYS    "类型系统错误"
#define LV_EC_CAT_SHORT_LV_CAT_PROOF     "证明系统"
#define LV_EC_CAT_LONG_LV_CAT_PROOF      "证明系统错误"

#define LV_EC_CAT_CONCAT_IMPL(a, b) a##b
#define LV_EC_CAT_CONCAT(a, b) LV_EC_CAT_CONCAT_IMPL(a, b)
#define LV_EC_CAT_SHORT(key) LV_EC_CAT_CONCAT(LV_EC_CAT_SHORT_, key)
#define LV_EC_CAT_LONG(key) LV_EC_CAT_CONCAT(LV_EC_CAT_LONG_, key)

/**
 * @brief 状态码类别区间 X-macro —— 粗粒度类别区间的单一事实来源
 *
 * 条目形态：x(min, max, 类别键)
 *
 * 供 layer2_resource/status_codes.c 的 kStatusCategoryRanges 宏展开生成。
 * 类别名经 LV_EC_CAT_LONG(键) 从 LV_ERROR_CATEGORIES_X 单点派生，与
 * LV_ERROR_CODES_X 第 5 字段（每码类别短名）共用同一类别键，无需手工同步。
 *
 * 区间端点（min/max）由各模块预留码段的"手工分组"决定，故仍在此显式写出；
 * 仅类别名收敛为单一事实源（无法纯按每码类别自动推导区间端点的原因：例如
 * "解析器"上限 139 而相邻类别"约束图"起点 200，自动推导将得到 130-199）。
 *
 * 注：负码警告区间（INT_MIN..-1 → "警告"）是 status_codes 模块的语义，
 * 不属于错误码体系，不在本宏中定义。
 */
#define LV_ERROR_CATEGORY_RANGES_X(x) \
    x(0, 0, LV_CAT_OK) \
    x(1, 99, LV_CAT_SYSTEM) \
    x(100, 129, LV_CAT_MEMORY) \
    x(130, 139, LV_CAT_PARSER) \
    x(200, 299, LV_CAT_GRAPH) \
    x(300, 399, LV_CAT_COORD) \
    x(400, 499, LV_CAT_SOLVER) \
    x(500, 599, LV_CAT_REWRITE) \
    x(600, 699, LV_CAT_UNIFY) \
    x(700, 749, LV_CAT_FBLOCK) \
    x(750, 799, LV_CAT_PRESET) \
    x(800, 899, LV_CAT_TYPESYS) \
    x(900, 999, LV_CAT_PROOF)

/** @brief 枚举生成辅助宏（LV_ERROR_CODES_X → `枚举名 = 数值,`） */
#define LV_X_EC_ENUM_ITEM(name, value, name_str, msg, category) name = value,

/**
 * @brief Lv-00 系统错误码枚举（由 LV_ERROR_CODES_X 宏生成，数值与历史值严格一致）
 */
typedef enum {
    /* 成功 */
    LV_ERROR_CODES_X(LV_X_EC_ENUM_ITEM)
    lv_ERROR_COUNT /**< 错误码总数，用于数组大小计算 */
} lvErrorCode;

#undef LV_X_EC_ENUM_ITEM

/* ============================================================
 * lvResult —— 统一错误传播结果类型
 *
 * 用于需要在成功/失败之间携带丰富错误信息的函数。
 * 配合 lv_RETURN_ERROR_* 宏使用，使错误路径代码更简洁。
 * ============================================================ */

/** 最大错误消息长度 */
#define lv_ERROR_MSG_MAX 256

/**
 * @brief 错误传播结果类型
 *
 * 成功时设置 success=true，error_code=lv_OK。
 * 失败时设置 success=false，error_code 为具体错误码，error_message 为描述。
 *
 * 使用示例：
 *   lvResult r = some_function();
 *   if (!r.success) { LOG_ERROR(...); return r; }
 */
typedef struct {
    bool success;                  /**< 操作是否成功 */
    lvErrorCode error_code;        /**< 错误码（成功时为 lv_OK） */
    char error_message[lv_ERROR_MSG_MAX]; /**< 错误描述 */
} lvResult;

/** @brief 生成成功结果 */
#define lv_RESULT_OK() ((lvResult){true, lv_OK, ""})

/** @brief 生成失败结果 */
#define lv_RESULT_ERROR(code, msg) ((lvResult){false, (code), (msg)})

/**
 * @brief 检查表达式，若失败则提前返回（用于 lvResult 传播链）
 *
 * 使用示例：
 *   lvResult r = lv_TRY(some_function());
 *   等价于：
 *   lvResult _r = some_function();
 *   if (!_r.success) return _r;
 */
#define lv_TRY(expr) \
    ({ lvResult _r = (expr); if (!_r.success) return _r; _r; })

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
/**
 * @brief 判断错误码是否已收录于规范错误表
 *
 * 未收录码的 lv_error_string/lv_error_name 将返回占位符文本
 * （"未知错误码"/"UNKNOWN_ERROR"）。本接口为纯查询，不修改线程错误状态。
 * @param code 错误码
 * @return true 表示未收录（占位符文本生效）；false 表示已收录
 */
lv_PUBLIC_API bool lv_error_is_unknown(lvErrorCode code);
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
 * @brief 获取当前线程的最后错误消息（兼容别名）
 *
 * 文档/示例中引用（lv.h 的 lv_get_last_error() 用法），本批补齐声明与实现
 * （C-㊺续36）：语义等价 lv_get_last_error_message()，返回线程局部错误
 * 消息或错误码默认描述，调用者无需释放。
 */
lv_PUBLIC_API const char *lv_get_last_error(void);
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
lv_PUBLIC_API void lv_set_error_ctx(lvErrorCode code, const char *file, int line, const char *func, const char *format,
                                    ...);
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
#define lv_ERROR_SET(code, fmt, ...) \
    lv_set_error_ctx((lvErrorCode) (code), __FILE__, __LINE__, __func__, (fmt), ##__VA_ARGS__)
/**
 * @brief 检查指针是否为NULL，如果是则设置错误并返回
 * @param ptr 要检查的指针
 * @param ret 返回值
 */
#define lv_CHECK_NULL(ptr, ret)                                                                        \
    do {                                                                                               \
        if ((ptr) == NULL) {                                                                           \
            lv_set_error_ctx(lv_ERROR_NULL_POINTER, __FILE__, __LINE__, __func__, "空指针: %s", #ptr); \
            return (ret);                                                                              \
        }                                                                                              \
    } while (0)
/**
 * @brief 检查指针是否为NULL，如果是则设置错误并无返回值返回（用于 void 函数）
 * @param ptr 要检查的指针
 */
#define lv_CHECK_NULL_VOID(ptr)                                                                        \
    do {                                                                                               \
        if ((ptr) == NULL) {                                                                           \
            lv_set_error_ctx(lv_ERROR_NULL_POINTER, __FILE__, __LINE__, __func__, "空指针: %s", #ptr); \
            return;                                                                                    \
        }                                                                                              \
    } while (0)
/**
 * @brief 检查条件，如果不满足则设置错误并返回
 * @param cond 条件表达式
 * @param err_code 错误码
 * @param ret 返回值
 * @param msg 错误消息
 */
#define lv_CHECK(cond, err_code, ret, msg)                                                      \
    do {                                                                                        \
        if (!(cond)) {                                                                          \
            lv_set_error_ctx((err_code), __FILE__, __LINE__, __func__, "%s: %s", (msg), #cond); \
            return (ret);                                                                       \
        }                                                                                       \
    } while (0)
/**
 * @brief 检查内存分配是否成功
 * @param ptr 分配的指针
 * @param ret 返回值
 */
#define lv_CHECK_ALLOC(ptr, ret)                                                                              \
    do {                                                                                                      \
        if ((ptr) == NULL) {                                                                                  \
            lv_set_error_ctx(lv_ERROR_OUT_OF_MEMORY, __FILE__, __LINE__, __func__, "内存分配失败: %s", #ptr); \
            return (ret);                                                                                     \
        }                                                                                                     \
    } while (0)
/**
 * @brief 检查索引是否在有效范围内
 * @param idx 索引值
 * @param max 最大值（不包含）
 * @param ret 返回值
 */
#define lv_CHECK_INDEX(idx, max, ret)                                                          \
    do {                                                                                       \
        if ((idx) < 0 || (idx) >= (max)) {                                                     \
            lv_set_error_ctx(lv_ERROR_INVALID_PARAM, __FILE__, __LINE__, __func__,             \
                             "索引越界: %s=%d, 有效范围[0, %s=%d]", #idx, (idx), #max, (max)); \
            return (ret);                                                                      \
        }                                                                                      \
    } while (0)
/**
 * @brief 检查范围是否有效（min <= val <= max）
 * @param val 要检查的值
 * @param min 最小值
 * @param max 最大值
 * @param ret 返回值
 */
#define lv_CHECK_RANGE(val, min, max, ret)                                                                        \
    do {                                                                                                          \
        if ((val) < (min) || (val) > (max)) {                                                                     \
            lv_set_error_ctx(lv_ERROR_INVALID_PARAM, __FILE__, __LINE__, __func__,                                \
                             "值超出范围: %s=%d, 有效范围[%s=%d, %s=%d]", #val, (val), #min, (min), #max, (max)); \
            return (ret);                                                                                         \
        }                                                                                                         \
    } while (0)

/* ============================================================
 * 便捷错误返回宏（lv_ERROR_RETURN 及其便捷别名）
 *
 * 将 lv_set_error_ctx 和 return 合并为一步，减少遗漏 return 的风险。
 * 适用于函数错误路径中的快速退出。
 * - lv_ERROR_RETURN:    设置错误并返回指定值
 * - lv_RETURN_ERROR:    返回 -1（int 函数常用）
 * - lv_RETURN_ERROR_NULL: 返回 NULL（指针函数常用）
 * - lv_RETURN_ERROR_BOOL: 返回 false（bool 函数常用）
 * - lv_RETURN_ERROR_VAL:  返回任意值
 * ============================================================ */
#define lv_ERROR_RETURN(err_code, ret_val, fmt, ...)                                      \
    do {                                                                                  \
        lv_set_error_ctx((err_code), __FILE__, __LINE__, __func__, (fmt), ##__VA_ARGS__); \
        return (ret_val);                                                                 \
    } while (0)

#define lv_RETURN_ERROR(code, fmt, ...) \
    lv_ERROR_RETURN((code), -1, (fmt), ##__VA_ARGS__)

#define lv_RETURN_ERROR_NULL(code, fmt, ...) \
    lv_ERROR_RETURN((code), NULL, (fmt), ##__VA_ARGS__)

#define lv_RETURN_ERROR_BOOL(code, fmt, ...) \
    lv_ERROR_RETURN((code), false, (fmt), ##__VA_ARGS__)

#define lv_RETURN_ERROR_VAL(code, val, fmt, ...) \
    lv_ERROR_RETURN((code), (val), (fmt), ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* lv_ERROR_CODES_H */
