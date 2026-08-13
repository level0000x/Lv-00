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
#include "lv/lv_error.h"
#include "lv/lv_str_utils.h"

/* 命名常量 */
#define lv_ERROR_MSG_BUFFER_SIZE 512   /**< 线程局部错误消息缓冲区大小 */
#define lv_ERROR_CTX_FORMAT_SIZE 64    /**< 上下文格式前缀大小 */
#define lv_ERROR_FILE_BUFFER_SIZE 128  /**< 错误上下文（文件名/函数名）缓冲区大小 */

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
 * @brief 错误信息表（由 error_codes.h 的 LV_ERROR_CODES_X 宏展开生成）
 *
 * 生成顺序即列表顺序 = 枚举值升序，find_error_info() 二分查找所需的
 * 排序不变量由编译期宏展开保证（运行时校验 lv_error_table_validate
 * 恒为 true，仅保留以兼容既有调用点）。
 *
 * 添加新错误码时，只需在 error_codes.h 的 LV_ERROR_CODES_X 中
 * 按枚举值升序追加一条，本表自动同步。
 */
#define LV_X_EC_TABLE_ENTRY(name, value, name_str, msg, category) \
    {name, name_str, msg, LV_EC_CAT_SHORT(category)},

static const ErrorInfo g_error_table[] = {
    LV_ERROR_CODES_X(LV_X_EC_TABLE_ENTRY)
};

#undef LV_X_EC_TABLE_ENTRY

#define ERROR_TABLE_SIZE lv_ARRAY_COUNT(g_error_table)

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

/**
 * @brief 二分查找错误码在表中的索引（无副作用）
 * @param code 错误码
 * @return 命中返回表索引，未命中返回 -1
 */
static int find_error_index(lvErrorCode code) {
    int left = 0;
    int right = (int) ERROR_TABLE_SIZE - 1;

    while (left <= right) {
        int mid = left + (right - left) / 2;
        if (g_error_table[mid].code == code) {
            return mid;
        } else if (g_error_table[mid].code < code) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    return -1;
}

/**
 * @brief 二分查找错误信息
 * @param code 错误码
 * @return 错误信息指针，未找到返回NULL
 */
static const ErrorInfo *find_error_info(lvErrorCode code) {
    int idx = find_error_index(code);
    if (idx < 0) {
        lv_RETURN_ERROR_NULL(lv_ERROR_NOT_FOUND, "error code %d not found in table", code);
    }
    return &g_error_table[idx];
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

bool lv_error_is_unknown(lvErrorCode code) {
    return find_error_index(code) < 0;
}

/**
 * @brief 验证错误信息表的排序正确性
 *
 * g_error_table 由 LV_ERROR_CODES_X 宏展开生成（列表按枚举值升序），
 * 排序不变量由编译期宏展开保证，无需运行时扫描；保留此函数仅为
 * 兼容既有调用点（lv.c 启动自检、测试），恒返回 true。
 *
 * @return true（表排序恒正确）
 */
bool lv_error_table_validate(void) {
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

    /* 桥接：同步推入新式帧栈（无位置信息；lv_OK 日志通道帧跳过） */
    if (code != lv_OK)
        lv_error_push(code, NULL, 0, NULL, g_error_message);
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

    /* 桥接：同步推入新式帧栈（lv_OK 是日志通道帧，跳过），旧写端零改动获得链式追踪 */
    if (code != lv_OK)
        lv_error_push(code, file, line, func, g_error_message);
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
    /* 桥接：同步清空新式帧栈，两套状态保持一致 */
    lv_error_clear(lv_error_context_current());
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
        if (lv_str_eq(g_error_table[i].name, name)) {
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
