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

#include "lv00.h"
#include "lv00_internal.h"

/* 命名常量 */
#define LV00_ERROR_MSG_BUFFER_SIZE 512   /**< 线程局部错误消息缓冲区大小 */
#define LV00_ERROR_CTX_FORMAT_SIZE 64    /**< 上下文格式前缀大小 */
#define LV00_ERROR_FILE_BUFFER_SIZE 128  /**< 错误上下文（文件名/函数名）缓冲区大小 */
#define LV00_ERROR_VALIDATE_BUF_SIZE 256 /**< 错误表验证消息缓冲区大小 */

/* ============================================================
 * 线程局部错误状态
 * ============================================================ */

/* 线程局部错误码 */
static LV00_THREAD_LOCAL Lv00ErrorCode g_last_error_code = LV00_OK;

/* 线程局部错误消息缓冲区 */
static LV00_THREAD_LOCAL char g_error_message[LV00_ERROR_MSG_BUFFER_SIZE] = {0};

/* 线程局部错误上下文信息 */
static LV00_THREAD_LOCAL char g_error_file[LV00_ERROR_FILE_BUFFER_SIZE] = {0};
static LV00_THREAD_LOCAL int g_error_line = 0;
static LV00_THREAD_LOCAL char g_error_func[LV00_ERROR_FILE_BUFFER_SIZE] = {0};

/* ============================================================
 * 错误信息表
 * ============================================================ */

/**
 * @brief 错误信息条目结构
 */
typedef struct {
    Lv00ErrorCode code;
    const char *name;
    const char *message;
    const char *category;
} ErrorInfo;

/**
 * @brief 错误信息表
 *
 * **警告：此表必须按 Lv00ErrorCode 枚举值升序排列！**
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
    {LV00_OK, "LV00_OK", "操作成功", "成功"},

    /* 通用系统错误 */
    {LV00_ERROR_UNKNOWN, "LV00_ERROR_UNKNOWN", "未知错误", "系统"},
    {LV00_ERROR_INVALID_PARAM, "LV00_ERROR_INVALID_PARAM", "无效参数", "系统"},
    {LV00_ERROR_NULL_POINTER, "LV00_ERROR_NULL_POINTER", "空指针", "系统"},
    {LV00_ERROR_NOT_INITIALIZED, "LV00_ERROR_NOT_INITIALIZED", "未初始化", "系统"},
    {LV00_ERROR_ALREADY_EXISTS, "LV00_ERROR_ALREADY_EXISTS", "已存在", "系统"},
    {LV00_ERROR_NOT_FOUND, "LV00_ERROR_NOT_FOUND", "未找到", "系统"},
    {LV00_ERROR_UNSUPPORTED, "LV00_ERROR_UNSUPPORTED", "不支持的操作", "系统"},
    {LV00_ERROR_OVERFLOW, "LV00_ERROR_OVERFLOW", "数值溢出", "系统"},
    {LV00_ERROR_UNDERFLOW, "LV00_ERROR_UNDERFLOW", "数值下溢", "系统"},
    {LV00_ERROR_TIMEOUT, "LV00_ERROR_TIMEOUT", "操作超时", "系统"},
    {LV00_ERROR_CANCELLED, "LV00_ERROR_CANCELLED", "操作被取消", "系统"},
    {LV00_ERROR_IO, "LV00_ERROR_IO", "IO错误", "系统"},
    {LV00_ERROR_PARSE, "LV00_ERROR_PARSE", "解析错误", "系统"},
    {LV00_ERROR_INVALID_STATE, "LV00_ERROR_INVALID_STATE", "无效状态", "系统"},
    {LV00_ERROR_INVALID_ARGUMENT, "LV00_ERROR_INVALID_ARGUMENT", "无效参数（字符串为空等）", "系统"},
    {LV00_ERROR_INDEX_OUT_OF_RANGE, "LV00_ERROR_INDEX_OUT_OF_RANGE", "索引越界", "系统"},
    {LV00_ERROR_VALUE_OUT_OF_RANGE, "LV00_ERROR_VALUE_OUT_OF_RANGE", "数值越界", "系统"},

    {LV00_ERROR_INTERNAL, "LV00_ERROR_INTERNAL", "内部错误", "系统"},

    /* 内存与资源错误 */
    {LV00_ERROR_OUT_OF_MEMORY, "LV00_ERROR_OUT_OF_MEMORY", "内存不足", "内存"},
    {LV00_ERROR_ALLOCATION_FAILED, "LV00_ERROR_ALLOCATION_FAILED", "内存分配失败", "内存"},
    {LV00_ERROR_RESOURCE_EXHAUSTED, "LV00_ERROR_RESOURCE_EXHAUSTED", "资源耗尽", "内存"},
    {LV00_ERROR_BUFFER_TOO_SMALL, "LV00_ERROR_BUFFER_TOO_SMALL", "缓冲区太小", "内存"},

    /* 解析器安全错误 */
    {LV00_ERROR_PARSER_NULL_INPUT, "LV00_ERROR_PARSER_NULL_INPUT", "解析器输入为NULL", "解析器"},
    {LV00_ERROR_PARSER_EMPTY_INPUT, "LV00_ERROR_PARSER_EMPTY_INPUT", "解析器输入为空", "解析器"},
    {LV00_ERROR_PARSER_INPUT_TOO_LONG, "LV00_ERROR_PARSER_INPUT_TOO_LONG", "解析器输入长度超限", "解析器"},
    {LV00_ERROR_PARSER_ILLEGAL_CHARS, "LV00_ERROR_PARSER_ILLEGAL_CHARS", "解析器输入含非法字符", "解析器"},
    {LV00_ERROR_PARSER_TOO_MANY_TOKENS, "LV00_ERROR_PARSER_TOO_MANY_TOKENS", "解析器token数量超限", "解析器"},
    {LV00_ERROR_PARSER_DEPTH_EXCEEDED, "LV00_ERROR_PARSER_DEPTH_EXCEEDED", "AST深度超限", "解析器"},
    {LV00_ERROR_PARSER_NODE_LIMIT, "LV00_ERROR_PARSER_NODE_LIMIT", "AST节点数超限", "解析器"},
    {LV00_ERROR_PARSER_TOKEN_TOO_LONG, "LV00_ERROR_PARSER_TOKEN_TOO_LONG", "解析器token长度超限", "解析器"},
    {LV00_ERROR_PARSER_POOL_EXHAUSTED, "LV00_ERROR_PARSER_POOL_EXHAUSTED", "解析器内存池耗尽", "解析器"},

    /* 约束图错误 */
    {LV00_ERROR_NODE_CONFLICT, "LV00_ERROR_NODE_CONFLICT", "节点冲突", "约束图"},
    {LV00_ERROR_NODE_NOT_FOUND, "LV00_ERROR_NODE_NOT_FOUND", "节点未找到", "约束图"},
    {LV00_ERROR_CONSTRAINT_CONFLICT, "LV00_ERROR_CONSTRAINT_CONFLICT", "约束冲突", "约束图"},
    {LV00_ERROR_CONSTRAINT_DUPLICATE, "LV00_ERROR_CONSTRAINT_DUPLICATE", "重复约束", "约束图"},
    {LV00_ERROR_INVALID_REGION, "LV00_ERROR_INVALID_REGION", "无效区域", "约束图"},
    {LV00_ERROR_INVALID_GEOM_TYPE, "LV00_ERROR_INVALID_GEOM_TYPE", "无效几何类型", "约束图"},
    {LV00_ERROR_CYCLIC_DEPENDENCY, "LV00_ERROR_CYCLIC_DEPENDENCY", "循环依赖", "约束图"},
    {LV00_ERROR_GRAPH_CORRUPTED, "LV00_ERROR_GRAPH_CORRUPTED", "图结构损坏", "约束图"},

    /* 符号坐标错误 */
    {LV00_ERROR_COORD_INVALID, "LV00_ERROR_COORD_INVALID", "无效坐标", "符号坐标"},
    {LV00_ERROR_COORD_OVERFLOW, "LV00_ERROR_COORD_OVERFLOW", "坐标溢出", "符号坐标"},
    {LV00_ERROR_PRECISION_LOSS, "LV00_ERROR_PRECISION_LOSS", "精度丢失", "符号坐标"},
    {LV00_ERROR_SYMBOLIC_EVAL_FAILED, "LV00_ERROR_SYMBOLIC_EVAL_FAILED", "符号求值失败", "符号坐标"},

    /* 求解器错误 */
    {LV00_ERROR_SOLVER_NO_SOLUTION, "LV00_ERROR_SOLVER_NO_SOLUTION", "方程无解", "求解器"},
    {LV00_ERROR_SOLVER_INFINITE, "LV00_ERROR_SOLVER_INFINITE", "无穷多解", "求解器"},
    {LV00_ERROR_SOLVER_NUMERIC, "LV00_ERROR_SOLVER_NUMERIC", "数值计算错误", "求解器"},
    {LV00_ERROR_SOLVER_SINGULAR, "LV00_ERROR_SOLVER_SINGULAR", "奇异矩阵", "求解器"},
    {LV00_ERROR_SOLVER_NOT_CONVERGED, "LV00_ERROR_SOLVER_NOT_CONVERGED", "求解未收敛", "求解器"},
    {LV00_ERROR_GROEBNER_FAILED, "LV00_ERROR_GROEBNER_FAILED", "Gröbner基计算失败", "求解器"},

    /* 重写引擎错误 */
    {LV00_ERROR_REWRITE_NO_MATCH, "LV00_ERROR_REWRITE_NO_MATCH", "无匹配规则", "重写引擎"},
    {LV00_ERROR_REWRITE_CYCLE, "LV00_ERROR_REWRITE_CYCLE", "重写循环", "重写引擎"},
    {LV00_ERROR_REWRITE_DEPTH, "LV00_ERROR_REWRITE_DEPTH", "重写深度超限", "重写引擎"},

    /* 合一检查错误 */
    {LV00_ERROR_UNIFY_FAILED, "LV00_ERROR_UNIFY_FAILED", "合一失败", "合一检查"},
    {LV00_ERROR_UNIFY_OCCUR_CHECK, "LV00_ERROR_UNIFY_OCCUR_CHECK", "发生检查失败", "合一检查"},
    {LV00_ERROR_UNIFY_TYPE_MISMATCH, "LV00_ERROR_UNIFY_TYPE_MISMATCH", "类型不匹配", "合一检查"},

    /* 函数块错误 */
    {LV00_ERROR_FUNC_BLOCK_INVALID, "LV00_ERROR_FUNC_BLOCK_INVALID", "无效函数块", "函数块"},
    {LV00_ERROR_FUNC_BLOCK_NON_DETERMINISTIC, "LV00_ERROR_FUNC_BLOCK_NON_DETERMINISTIC", "非确定性函数块", "函数块"},
    {LV00_ERROR_FUNC_BLOCK_CIRCULAR, "LV00_ERROR_FUNC_BLOCK_CIRCULAR", "循环函数块", "函数块"},
    {LV00_ERROR_FUNC_BLOCK_TYPE_ERROR, "LV00_ERROR_FUNC_BLOCK_TYPE_ERROR", "函数块类型错误", "函数块"},

    /* 预设系统错误 */
    {LV00_ERROR_PRESET_REGISTRATION_FAILED, "LV00_ERROR_PRESET_REGISTRATION_FAILED", "预设注册失败", "预设系统"},
    {LV00_ERROR_PRESET_INSTANTIATION_FAILED, "LV00_ERROR_PRESET_INSTANTIATION_FAILED", "预设实例化失败", "预设系统"},

    /* 类型系统错误 */
    {LV00_ERROR_TYPE_MISMATCH, "LV00_ERROR_TYPE_MISMATCH", "类型不匹配", "类型系统"},
    {LV00_ERROR_TYPE_INFERENCE_FAILED, "LV00_ERROR_TYPE_INFERENCE_FAILED", "类型推断失败", "类型系统"},
    {LV00_ERROR_UNIVERSE_INCONSISTENT, "LV00_ERROR_UNIVERSE_INCONSISTENT", "宇宙层级不一致", "类型系统"},

    /* 证明系统错误 */
    {LV00_ERROR_PROOF_INVALID, "LV00_ERROR_PROOF_INVALID", "无效证明", "证明系统"},
    {LV00_ERROR_PROOF_INCOMPLETE, "LV00_ERROR_PROOF_INCOMPLETE", "证明不完整", "证明系统"},
    {LV00_ERROR_PROOF_VERIFICATION_FAILED, "LV00_ERROR_PROOF_VERIFICATION_FAILED", "证明验证失败", "证明系统"},
};

#define ERROR_TABLE_SIZE LV00_ARRAY_COUNT(g_error_table)

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

/**
 * @brief 二分查找错误信息
 * @param code 错误码
 * @return 错误信息指针，未找到返回NULL
 */
static const ErrorInfo *find_error_info(Lv00ErrorCode code) {
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
    return NULL;
}

/* ============================================================
 * 公共接口实现
 * ============================================================ */

const char *lv00_error_string(Lv00ErrorCode code) {
    const ErrorInfo *info = find_error_info(code);
    if (info != NULL) {
        return info->message;
    }
    return "未知错误码";
}

const char *lv00_error_name(Lv00ErrorCode code) {
    const ErrorInfo *info = find_error_info(code);
    if (info != NULL) {
        return info->name;
    }
    return "UNKNOWN_ERROR";
}

const char *lv00_error_category(Lv00ErrorCode code) {
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
 * 是否按 Lv00ErrorCode 枚举值非降序排列。
 * 二分查找依赖此排序，若表未排序将导致错误结果。
 *
 * 注意：允许相邻条目具有相同的错误码（例如为同一错误码
 * 提供不同语言的描述或别名），因此使用 < 而非 <= 进行检查。
 * 仅当后续条目的错误码严格小于前一条目时才报告排序违规。
 *
 * @return true 表排序正确，false 检测到排序违规
 */
bool lv00_error_table_validate(void) {
    for (size_t i = 1; i < ERROR_TABLE_SIZE; i++) {
        /* 修复：使用 < 而非 <=，允许重复错误码（某些场景可能需要
         * 同一错误码对应多个条目，如不同语言的描述或别名） */
        if (g_error_table[i].code < g_error_table[i - 1].code) {
            /* 检测到排序违规：记录详细错误信息 */
            char buf[LV00_ERROR_VALIDATE_BUF_SIZE];
            snprintf(buf, sizeof(buf),
                     "错误表排序违规: 索引 %zu (code=%d, name=%s) "
                     "小于索引 %zu (code=%d, name=%s)",
                     i, (int) g_error_table[i].code, g_error_table[i].name, i - 1, (int) g_error_table[i - 1].code,
                     g_error_table[i - 1].name);
            lv00_set_error(LV00_ERROR_INVALID_STATE, "%s", buf);
            return false;
        }
    }
    return true;
}

/**
 * @brief 获取当前线程的最后错误码
 * @return 当前线程存储的最后错误码（Lv00ErrorCode 枚举值）
 * @note 此函数返回的是线程局部存储的错误码，每个线程有独立的错误状态
 */
Lv00ErrorCode lv00_get_last_error_code(void) {
    return g_last_error_code;
}

/**
 * @brief 获取当前线程的最后错误消息
 * @return 错误消息字符串指针。如果未设置自定义消息，则返回错误码对应的默认描述
 * @note 返回的指针指向线程局部缓冲区，无需由调用者释放
 */
const char *lv00_get_last_error_message(void) {
    if (g_error_message[0] == '\0') {
        return lv00_error_string(g_last_error_code);
    }
    return g_error_message;
}

int lv00_get_error_description(char *buf, size_t buf_size) {
    if (!buf || buf_size == 0) {
        return -1;
    }

    const char *name = lv00_error_name(g_last_error_code);
    const char *message = lv00_get_last_error_message();
    const char *category = lv00_error_category(g_last_error_code);

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
            LV00_SAFE_SNPRINTF(written, buf, buf_size, "[%s] %s (0x%08X): %s\n  位置: %s:%d (%s)", category, name,
                               g_last_error_code, message, g_error_file, g_error_line, g_error_func);
        } else {
            /* 函数名无效：仅输出文件名和行号 */
            LV00_SAFE_SNPRINTF(written, buf, buf_size, "[%s] %s (0x%08X): %s\n  位置: %s:%d", category, name,
                               g_last_error_code, message, g_error_file, g_error_line);
        }
    } else {
        /* 无上下文信息 */
        LV00_SAFE_SNPRINTF(written, buf, buf_size, "[%s] %s (0x%08X): %s", category, name, g_last_error_code, message);
    }

    return written;
}

void lv00_set_error(Lv00ErrorCode code, const char *format, ...) {
    g_last_error_code = code;

    if (format != NULL) {
        va_list args;
        va_start(args, format);
        vsnprintf(g_error_message, LV00_ERROR_MSG_BUFFER_SIZE, format, args);
        va_end(args);
    } else {
        lv00_strlcpy(g_error_message, lv00_error_string(code), LV00_ERROR_MSG_BUFFER_SIZE);
    }

    /* 清除上下文信息 */
    g_error_file[0] = '\0';
    g_error_line = 0;
    g_error_func[0] = '\0';
}

void lv00_set_error_ctx(Lv00ErrorCode code, const char *file, int line, const char *func, const char *format, ...) {
    g_last_error_code = code;

    /* 保存上下文信息 */
    if (file) {
        lv00_strlcpy(g_error_file, file, sizeof(g_error_file));
    }
    g_error_line = line;
    if (func) {
        lv00_strlcpy(g_error_func, func, sizeof(g_error_func));
    }

    if (format != NULL) {
        va_list args;
        va_start(args, format);
        vsnprintf(g_error_message, LV00_ERROR_MSG_BUFFER_SIZE, format, args);
        va_end(args);
    } else {
        lv00_strlcpy(g_error_message, lv00_error_string(code), LV00_ERROR_MSG_BUFFER_SIZE);
    }
}

/**
 * @brief 清除当前线程的错误状态
 * @note 将错误码重置为 LV00_OK，并清空错误消息及上下文信息（文件名、行号、函数名）
 */
void lv00_clear_error(void) {
    g_last_error_code = LV00_OK;
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
 * @param name 错误名称（如 "LV00_OK"、"LV00_ERROR_OUT_OF_MEMORY"）
 * @return 对应的错误码枚举值，未找到时返回 LV00_ERROR_UNKNOWN
 */
Lv00ErrorCode lv00_error_code_from_string(const char *name) {
    if (!name)
        return LV00_ERROR_UNKNOWN;

    for (size_t i = 0; i < ERROR_TABLE_SIZE; i++) {
        if (strcmp(g_error_table[i].name, name) == 0) {
            return g_error_table[i].code;
        }
    }
    return LV00_ERROR_UNKNOWN;
}
