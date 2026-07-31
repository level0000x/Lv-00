/**
 * @file error_codes.c
 * @brief Lv-00 统一错误码系统实现
 *
 * @details 实现线程局部的错误码存储、错误消息格式化、错误上下文追踪
 *          和错误表验证功能。为整个 Lv-00 系统提供统一的错误报告机制，
 *          支持文件名、行号、函数名等上下文信息的自动捕获。
 *
 * @version 3.3.0
 * @author Lv-00 Team
 */

#include "error_codes.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "lv.h"
#include "lv_internal.h"

/* 命名常量 */
#define lv_ERROR_MSG_BUFFER_SIZE 512   /**< 线程局部错误消息缓冲区大小 */
#define lv_ERROR_CTX_FORMAT_SIZE 64    /**< 上下文格式前缀大小 */
#define lv_ERROR_FILE_BUFFER_SIZE 128  /**< 错误上下文（文件名/函数名）缓冲区大小 */
#define lv_ERROR_VALIDATE_BUF_SIZE 256 /**< 错误表验证消息缓冲区大小 */

/* ============================================================
 * 线程局部错误状态
 * ============================================================ */

/* 线程局部错误码 */
static lv_THREAD_LOCAL lvErrorCode g_last_error_code = lv_OK;

/* 线程局部错误消息缓冲区 */
static lv_THREAD_LOCAL char g_error_message[lv_ERROR_MSG_BUFFER_SIZE] = {0};

/* 线程局部错误上下文信息 */
static lv_THREAD_LOCAL char g_error_file[lv_ERROR_FILE_BUFFER_SIZE] = {0};
static lv_THREAD_LOCAL int g_error_line = 0;
static lv_THREAD_LOCAL char g_error_func[lv_ERROR_FILE_BUFFER_SIZE] = {0};

/* ============================================================
 * 错误信息表
 * ============================================================ */

/**
 * @brief 错误信息条目结构
 */
typedef struct {
    lvErrorCode code;
    const char *name;
    const char *message;
    const char *category;
} ErrorInfo;

/**
 * @brief 错误信息表
 *
 * **警告：此表必须按 lvErrorCode 枚举值升序排列！**
 * `find_error_info()` 使用二分查找定位错误码，若表未排序，
 * 二分查找将返回错误结果。
 *
 * 添加新错误码时，请确保：
 * 1. 在 error_codes.h 中按枚举值递增顺序添加新枚举值
 * 2. 在此表中按相同顺序插入对应的 {code, name, message} 条目
 * 3. 运行测试以验证二分查找的正确性
 */
static const ErrorInfo g_error_table[] = {
    /* 成功 */
    {lv_OK, "lv_OK", "操作成功", "成功"},

    /* 通用系统错误 */
    {lv_ERROR_UNKNOWN, "lv_ERROR_UNKNOWN", "未知错误", "系统"},
    {lv_ERROR_INVALID_PARAM, "lv_ERROR_INVALID_PARAM", "无效参数", "系统"},
    {lv_ERROR_NULL_POINTER, "lv_ERROR_NULL_POINTER", "空指针", "系统"},
    {lv_ERROR_NOT_INITIALIZED, "lv_ERROR_NOT_INITIALIZED", "未初始化", "系统"},
    {lv_ERROR_ALREADY_EXISTS, "lv_ERROR_ALREADY_EXISTS", "已存在", "系统"},
    {lv_ERROR_NOT_FOUND, "lv_ERROR_NOT_FOUND", "未找到", "系统"},
    {lv_ERROR_UNSUPPORTED, "lv_ERROR_UNSUPPORTED", "不支持的操作", "系统"},
    {lv_ERROR_OVERFLOW, "lv_ERROR_OVERFLOW", "数值溢出", "系统"},
    {lv_ERROR_UNDERFLOW, "lv_ERROR_UNDERFLOW", "数值下溢", "系统"},
    {lv_ERROR_TIMEOUT, "lv_ERROR_TIMEOUT", "操作超时", "系统"},
    {lv_ERROR_CANCELLED, "lv_ERROR_CANCELLED", "操作被取消", "系统"},
    {lv_ERROR_IO, "lv_ERROR_IO", "IO错误", "系统"},
    {lv_ERROR_PARSE, "lv_ERROR_PARSE", "解析错误", "系统"},
    {lv_ERROR_INVALID_STATE, "lv_ERROR_INVALID_STATE", "无效状态", "系统"},
    {lv_ERROR_INVALID_ARGUMENT, "lv_ERROR_INVALID_ARGUMENT", "无效参数（字符串为空等）", "系统"},
    {lv_ERROR_INDEX_OUT_OF_RANGE, "lv_ERROR_INDEX_OUT_OF_RANGE", "索引越界", "系统"},
    {lv_ERROR_VALUE_OUT_OF_RANGE, "lv_ERROR_VALUE_OUT_OF_RANGE", "数值越界", "系统"},

    {lv_ERROR_INTERNAL, "lv_ERROR_INTERNAL", "内部错误", "系统"},

    /* 内存与资源错误 */
    {lv_ERROR_OUT_OF_MEMORY, "lv_ERROR_OUT_OF_MEMORY", "内存不足", "内存"},
    {lv_ERROR_ALLOCATION_FAILED, "lv_ERROR_ALLOCATION_FAILED", "内存分配失败", "内存"},
    {lv_ERROR_RESOURCE_EXHAUSTED, "lv_ERROR_RESOURCE_EXHAUSTED", "资源耗尽", "内存"},
    {lv_ERROR_BUFFER_TOO_SMALL, "lv_ERROR_BUFFER_TOO_SMALL", "缓冲区太小", "内存"},

    /* 解析器安全错误 */
    {lv_ERROR_PARSER_NULL_INPUT, "lv_ERROR_PARSER_NULL_INPUT", "解析器输入为NULL", "解析器"},
    {lv_ERROR_PARSER_EMPTY_INPUT, "lv_ERROR_PARSER_EMPTY_INPUT", "解析器输入为空", "解析器"},
    {lv_ERROR_PARSER_INPUT_TOO_LONG, "lv_ERROR_PARSER_INPUT_TOO_LONG", "解析器输入长度超限", "解析器"},
    {lv_ERROR_PARSER_ILLEGAL_CHARS, "lv_ERROR_PARSER_ILLEGAL_CHARS", "解析器输入含非法字符", "解析器"},
    {lv_ERROR_PARSER_TOO_MANY_TOKENS, "lv_ERROR_PARSER_TOO_MANY_TOKENS", "解析器token数量超限", "解析器"},
    {lv_ERROR_PARSER_DEPTH_EXCEEDED, "lv_ERROR_PARSER_DEPTH_EXCEEDED", "AST深度超限", "解析器"},
    {lv_ERROR_PARSER_NODE_LIMIT, "lv_ERROR_PARSER_NODE_LIMIT", "AST节点数超限", "解析器"},
    {lv_ERROR_PARSER_TOKEN_TOO_LONG, "lv_ERROR_PARSER_TOKEN_TOO_LONG", "解析器token长度超限", "解析器"},
    {lv_ERROR_PARSER_POOL_EXHAUSTED, "lv_ERROR_PARSER_POOL_EXHAUSTED", "解析器内存池耗尽", "解析器"},

    /* 约束图错误 */
    {lv_ERROR_NODE_CONFLICT, "lv_ERROR_NODE_CONFLICT", "节点冲突", "约束图"},
    {lv_ERROR_NODE_NOT_FOUND, "lv_ERROR_NODE_NOT_FOUND", "节点未找到", "约束图"},
    {lv_ERROR_CONSTRAINT_CONFLICT, "lv_ERROR_CONSTRAINT_CONFLICT", "约束冲突", "约束图"},
    {lv_ERROR_CONSTRAINT_DUPLICATE, "lv_ERROR_CONSTRAINT_DUPLICATE", "重复约束", "约束图"},
    {lv_ERROR_INVALID_REGION, "lv_ERROR_INVALID_REGION", "无效区域", "约束图"},
    {lv_ERROR_INVALID_GEOM_TYPE, "lv_ERROR_INVALID_GEOM_TYPE", "无效几何类型", "约束图"},
    {lv_ERROR_CYCLIC_DEPENDENCY, "lv_ERROR_CYCLIC_DEPENDENCY", "循环依赖", "约束图"},
    {lv_ERROR_GRAPH_CORRUPTED, "lv_ERROR_GRAPH_CORRUPTED", "图结构损坏", "约束图"},

    /* 符号坐标错误 */
    {lv_ERROR_COORD_INVALID, "lv_ERROR_COORD_INVALID", "无效坐标", "符号坐标"},
    {lv_ERROR_COORD_OVERFLOW, "lv_ERROR_COORD_OVERFLOW", "坐标溢出", "符号坐标"},
    {lv_ERROR_PRECISION_LOSS, "lv_ERROR_PRECISION_LOSS", "精度丢失", "符号坐标"},
    {lv_ERROR_SYMBOLIC_EVAL_FAILED, "lv_ERROR_SYMBOLIC_EVAL_FAILED", "符号求值失败", "符号坐标"},

    /* 求解器错误 */
    {lv_ERROR_SOLVER_NO_SOLUTION, "lv_ERROR_SOLVER_NO_SOLUTION", "方程无解", "求解器"},
    {lv_ERROR_SOLVER_INFINITE, "lv_ERROR_SOLVER_INFINITE", "无穷多解", "求解器"},
    {lv_ERROR_SOLVER_NUMERIC, "lv_ERROR_SOLVER_NUMERIC", "数值计算错误", "求解器"},
    {lv_ERROR_SOLVER_SINGULAR, "lv_ERROR_SOLVER_SINGULAR", "奇异矩阵", "求解器"},
    {lv_ERROR_SOLVER_NOT_CONVERGED, "lv_ERROR_SOLVER_NOT_CONVERGED", "求解未收敛", "求解器"},
    {lv_ERROR_GROEBNER_FAILED, "lv_ERROR_GROEBNER_FAILED", "Gröbner基计算失败", "求解器"},

    /* 重写引擎错误 */
    {lv_ERROR_REWRITE_NO_MATCH, "lv_ERROR_REWRITE_NO_MATCH", "无匹配规则", "重写引擎"},
    {lv_ERROR_REWRITE_CYCLE, "lv_ERROR_REWRITE_CYCLE", "重写循环", "重写引擎"},
    {lv_ERROR_REWRITE_DEPTH, "lv_ERROR_REWRITE_DEPTH", "重写深度超限", "重写引擎"},

    /* 合一检查错误 */
    {lv_ERROR_UNIFY_FAILED, "lv_ERROR_UNIFY_FAILED", "合一失败", "合一检查"},
    {lv_ERROR_UNIFY_OCCUR_CHECK, "lv_ERROR_UNIFY_OCCUR_CHECK", "发生检查失败", "合一检查"},
    {lv_ERROR_UNIFY_TYPE_MISMATCH, "lv_ERROR_UNIFY_TYPE_MISMATCH", "类型不匹配", "合一检查"},

    /* 函数块错误 */
    {lv_ERROR_FUNC_BLOCK_INVALID, "lv_ERROR_FUNC_BLOCK_INVALID", "无效函数块", "函数块"},
    {lv_ERROR_FUNC_BLOCK_NON_DETERMINISTIC, "lv_ERROR_FUNC_BLOCK_NON_DETERMINISTIC", "非确定性函数块", "函数块"},
    {lv_ERROR_FUNC_BLOCK_CIRCULAR, "lv_ERROR_FUNC_BLOCK_CIRCULAR", "循环函数块", "函数块"},
    {lv_ERROR_FUNC_BLOCK_TYPE_ERROR, "lv_ERROR_FUNC_BLOCK_TYPE_ERROR", "函数块类型错误", "函数块"},

    /* 预设系统错误 */
    {lv_ERROR_PRESET_REGISTRATION_FAILED, "lv_ERROR_PRESET_REGISTRATION_FAILED", "预设注册失败", "预设系统"},
    {lv_ERROR_PRESET_INSTANTIATION_FAILED, "lv_ERROR_PRESET_INSTANTIATION_FAILED", "预设实例化失败", "预设系统"},

    /* 类型系统错误 */
    {lv_ERROR_TYPE_MISMATCH, "lv_ERROR_TYPE_MISMATCH", "类型不匹配", "类型系统"},
    {lv_ERROR_TYPE_INFERENCE_FAILED, "lv_ERROR_TYPE_INFERENCE_FAILED", "类型推断失败", "类型系统"},
    {lv_ERROR_UNIVERSE_INCONSISTENT, "lv_ERROR_UNIVERSE_INCONSISTENT", "宇宙层级不一致", "类型系统"},

    /* 证明系统错误 */
    {lv_ERROR_PROOF_INVALID, "lv_ERROR_PROOF_INVALID", "无效证明", "证明系统"},
    {lv_ERROR_PROOF_INCOMPLETE, "lv_ERROR_PROOF_INCOMPLETE", "证明不完整", "证明系统"},
    {lv_ERROR_PROOF_VERIFICATION_FAILED, "lv_ERROR_PROOF_VERIFICATION_FAILED", "证明验证失败", "证明系统"},
    {lv_ERROR_CIRCUIT_OPEN, "lv_ERROR_CIRCUIT_OPEN", "熔断器已跳闸（OPEN态）", "证明系统"},
};

#define ERROR_TABLE_SIZE lv_ARRAY_COUNT(g_error_table)

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

/**
 * @brief 二分查找错误信息
 * @param code 错误码
 * @return 错误信息指针，未找到返回NULL
 */
static const ErrorInfo *find_error_info(lvErrorCode code) {
    int left = 0;
    int right = (int) ERROR_TABLE_SIZE - 1;

    while (left <= right) {
        int mid = left + (right - left) / 2;
        if (g_error_table[mid].code == code) {
            return &g_error_table[mid];
        } else if (g_error_table[mid].code < code) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    lv_RETURN_ERROR_NULL(lv_ERROR_NOT_FOUND, "error code %d not found in table", code);
}

/* ============================================================
 * 公共接口实现
 * ============================================================ */

const char *lv_error_string(lvErrorCode code) {
    const ErrorInfo *info = find_error_info(code);
    if (info != NULL) {
        return info->message;
    }
    return "未知错误码";
}

const char *lv_error_name(lvErrorCode code) {
    const ErrorInfo *info = find_error_info(code);
    if (info != NULL) {
        return info->name;
    }
    return "UNKNOWN_ERROR";
}

const char *lv_error_category(lvErrorCode code) {
    const ErrorInfo *info = find_error_info(code);
    if (info != NULL) {
        return info->category;
    }
    return "未知";
}

/**
 * @brief 运行时验证错误信息表的排序正确性
 *
 * 在 debug 构建中调用此函数，验证 g_error_table 中的条目
 * 是否按 lvErrorCode 枚举值非降序排列。
 * 二分查找依赖此排序，若表未排序将导致错误结果。
 *
 * 注意：允许相邻条目具有相同的错误码（例如为同一错误码
 * 提供不同语言的描述或别名），因此使用 < 而非 <= 进行检查。
 * 仅当后续条目的错误码严格小于前一条目时才报告排序违规。
 *
 * @return true 表排序正确，false 检测到排序违规
 */
bool lv_error_table_validate(void) {
    for (size_t i = 1; i < ERROR_TABLE_SIZE; i++) {
        /* 修复：使用 < 而非 <=，允许重复错误码（某些场景可能需要
         * 同一错误码对应多个条目，如不同语言的描述或别名） */
        if (g_error_table[i].code < g_error_table[i - 1].code) {
            /* 检测到排序违规：记录详细错误信息 */
            char buf[lv_ERROR_VALIDATE_BUF_SIZE];
            snprintf(buf, sizeof(buf),
                     "错误表排序违规: 索引 %zu (code=%d, name=%s) "
                     "小于索引 %zu (code=%d, name=%s)",
                     i, (int) g_error_table[i].code, g_error_table[i].name, i - 1, (int) g_error_table[i - 1].code,
                     g_error_table[i - 1].name);
            lv_set_error(lv_ERROR_INVALID_STATE, "%s", buf);
            return false;
        }
    }
    return true;
}

/**
 * @brief 获取当前线程的最后错误码
 * @return 当前线程存储的最后错误码（lvErrorCode 枚举值）
 * @note 此函数返回的是线程局部存储的错误码，每个线程有独立的错误状态
 */
lvErrorCode lv_get_last_error_code(void) {
    return g_last_error_code;
}

/**
 * @brief 获取当前线程的最后错误消息
 * @return 错误消息字符串指针。如果未设置自定义消息，则返回错误码对应的默认描述
 * @note 返回的指针指向线程局部缓冲区，无需由调用者释放
 */
const char *lv_get_last_error_message(void) {
    if (g_error_message[0] == '\0') {
        return lv_error_string(g_last_error_code);
    }
    return g_error_message;
}

int lv_get_error_description(char *buf, size_t buf_size) {
    if (!buf || buf_size == 0) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "buf is NULL or buf_size is 0");
    }

    const char *name = lv_error_name(g_last_error_code);
    const char *message = lv_get_last_error_message();
    const char *category = lv_error_category(g_last_error_code);

    /* 修复：添加 g_error_file、g_error_line 和 g_error_func 的有效性检查。
     * 只有当文件名非空、行号大于0、且函数名非空时，才认为上下文信息完整，
     * 否则回退到无上下文的格式，避免输出中包含空的文件名或函数名。 */
    int written = -1;
    bool has_valid_context = (g_error_line > 0 && g_error_file[0] != '\0');
    bool has_valid_func = (g_error_func[0] != '\0');

    if (has_valid_context) {
        /* 有上下文信息 */
        if (has_valid_func) {
            /* 文件名、行号、函数名均有效：输出完整上下文 */
            lv_SAFE_SNPRINTF(written, buf, buf_size, "[%s] %s (0x%08X): %s\n  位置: %s:%d (%s)", category, name,
                             g_last_error_code, message, g_error_file, g_error_line, g_error_func);
        } else {
            /* 函数名无效：仅输出文件名和行号 */
            lv_SAFE_SNPRINTF(written, buf, buf_size, "[%s] %s (0x%08X): %s\n  位置: %s:%d", category, name,
                             g_last_error_code, message, g_error_file, g_error_line);
        }
    } else {
        /* 无上下文信息 */
        lv_SAFE_SNPRINTF(written, buf, buf_size, "[%s] %s (0x%08X): %s", category, name, g_last_error_code, message);
    }

    return written;
}

void lv_set_error(lvErrorCode code, const char *format, ...) {
    g_last_error_code = code;

    if (format != NULL) {
        va_list args;
        va_start(args, format);
        vsnprintf(g_error_message, lv_ERROR_MSG_BUFFER_SIZE, format, args);
        va_end(args);
    } else {
        lv_strlcpy(g_error_message, lv_error_string(code), lv_ERROR_MSG_BUFFER_SIZE);
    }

    /* 清除上下文信息 */
    g_error_file[0] = '\0';
    g_error_line = 0;
    g_error_func[0] = '\0';
}

void lv_set_error_ctx(lvErrorCode code, const char *file, int line, const char *func, const char *format, ...) {
    g_last_error_code = code;

    /* 保存上下文信息 */
    if (file) {
        lv_strlcpy(g_error_file, file, sizeof(g_error_file));
    }
    g_error_line = line;
    if (func) {
        lv_strlcpy(g_error_func, func, sizeof(g_error_func));
    }

    if (format != NULL) {
        va_list args;
        va_start(args, format);
        vsnprintf(g_error_message, lv_ERROR_MSG_BUFFER_SIZE, format, args);
        va_end(args);
    } else {
        lv_strlcpy(g_error_message, lv_error_string(code), lv_ERROR_MSG_BUFFER_SIZE);
    }
}

/**
 * @brief 清除当前线程的错误状态
 * @note 将错误码重置为 lv_OK，并清空错误消息及上下文信息（文件名、行号、函数名）
 */
void lv_clear_error(void) {
    g_last_error_code = lv_OK;
    g_error_message[0] = '\0';
    g_error_file[0] = '\0';
    g_error_line = 0;
    g_error_func[0] = '\0';
}

/**
 * @brief 从错误名称字符串反向查找错误码
 *
 * 线性遍历 g_error_table，逐条比对 name 字段。
 * 虽然时间复杂度为 O(n)，但错误表规模较小（约 50 条），
 * 且此函数通常仅在日志/调试场景调用，性能不敏感。
 *
 * @param name 错误名称（如 "lv_OK"、"lv_ERROR_OUT_OF_MEMORY"）
 * @return 对应的错误码枚举值，未找到时返回 lv_ERROR_UNKNOWN
 */
lvErrorCode lv_error_code_from_string(const char *name) {
    if (!name)
        return lv_ERROR_UNKNOWN;

    for (size_t i = 0; i < ERROR_TABLE_SIZE; i++) {
        if (strcmp(g_error_table[i].name, name) == 0) {
            return g_error_table[i].code;
        }
    }
    return lv_ERROR_UNKNOWN;
}

/**
 * @brief 获取规范错误信息表的条目数量
 *
 * 供同库内其他模块（如 status_codes、error_messages_cn）查询
 * 统一错误表的大小，避免各自维护重复的错误码映射表。
 *
 * @return 错误信息表条目数量
 */
int lv_error_table_size(void) {
    return (int) ERROR_TABLE_SIZE;
}
