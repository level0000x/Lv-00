/**
 * @file status_codes.c
 * @brief Lv-00 状态码系统实现
 *
 * 实现 status_codes.h 中声明的状态码查询接口，
 * 以及 error_codes.h 中定义的错误码到中文描述的映射。
 *
 * @author Lv-00 Project
 * @version 2.0.0
 */

#include "lv/status_codes.h"

#include "lv/error_codes.h"
#include "lv/lv.h"

/* ==================== 状态码判断函数 ==================== */

int lv_status_is_success(int code) {
    return code == 0 ? 1 : 0;
}

int lv_status_is_error(int code) {
    return code != 0 ? 1 : 0;
}

/* ==================== 状态码描述映射 ==================== */

/* ==================== 状态码描述映射 ==================== */

/** 状态码 → 文本 映射表项 */
typedef struct {
    int code;
    const char *text;
} StatusCodeText;

/**
 * @brief 在升序映射表中二分查找状态码
 *
 * @param table 映射表（按 code 升序）
 * @param count 表项数量
 * @param code  状态码
 * @return 命中返回文本指针，未命中返回 NULL
 */
static const char *status_text_lookup(const StatusCodeText *table, size_t count, int code) {
    size_t lo = 0, hi = count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (table[mid].code == code)
            return table[mid].text;
        if (table[mid].code < code)
            lo = mid + 1;
        else
            hi = mid;
    }
    return NULL;
}

static const StatusCodeText s_status_messages[] = {
    {0, "操作成功"},
    {1, "未知错误"},
    {2, "无效参数"},
    {3, "空指针"},
    {4, "未初始化"},
    {5, "已存在"},
    {6, "未找到"},
    {7, "不支持的操作"},
    {8, "数值溢出"},
    {9, "数值下溢"},
    {10, "操作超时"},
    {11, "操作被取消"},
    {12, "IO错误"},
    {13, "解析错误"},
    {14, "无效状态"},
    {15, "无效参数（已废弃，请使用错误码2）"},
    {17, "索引越界"},
    {18, "数值越界"},
    {70, "内部错误"},
    {100, "内存不足"},
    {101, "内存分配失败"},
    {102, "资源耗尽"},
    {103, "缓冲区太小"},
    {130, "解析器输入为NULL"},
    {131, "解析器输入为空字符串"},
    {132, "解析器输入长度超限"},
    {133, "输入含非法控制字符或null字节"},
    {134, "token数量超限"},
    {135, "AST深度超限"},
    {136, "AST节点数超限"},
    {137, "token长度超限"},
    {138, "内存池耗尽"},
    {200, "节点冲突"},
    {201, "节点未找到"},
    {202, "约束冲突"},
    {203, "重复约束"},
    {204, "无效区域"},
    {205, "无效几何类型"},
    {206, "循环依赖"},
    {207, "图结构损坏"},
    {300, "无效坐标"},
    {301, "坐标溢出"},
    {302, "精度丢失"},
    {303, "符号求值失败"},
    {400, "无解"},
    {401, "无穷多解"},
    {402, "数值计算错误"},
    {403, "奇异矩阵"},
    {404, "未收敛"},
    {405, "Groebner基计算失败"},
    {500, "无匹配规则"},
    {501, "重写循环"},
    {502, "重写深度超限"},
    {600, "合一失败"},
    {601, "发生检查失败"},
    {602, "类型不匹配"},
    {700, "无效函数块"},
    {701, "非确定性函数块"},
    {702, "循环函数块"},
    {703, "函数块类型错误"},
    {750, "预设注册失败"},
    {751, "预设实例化失败"},
    {800, "类型不匹配"},
    {801, "类型推断失败"},
    {802, "宇宙层级不一致"},
    {900, "无效证明"},
    {901, "证明不完整"},
    {902, "证明验证失败"},
    {903, "熔断器已跳闸（OPEN态）"},
};

static const StatusCodeText s_status_names[] = {
    {0, "lv_OK"},
    {1, "lv_ERROR_UNKNOWN"},
    {2, "lv_ERROR_INVALID_PARAM"},
    {3, "lv_ERROR_NULL_POINTER"},
    {4, "lv_ERROR_NOT_INITIALIZED"},
    {5, "lv_ERROR_ALREADY_EXISTS"},
    {6, "lv_ERROR_NOT_FOUND"},
    {7, "lv_ERROR_UNSUPPORTED"},
    {8, "lv_ERROR_OVERFLOW"},
    {9, "lv_ERROR_UNDERFLOW"},
    {10, "lv_ERROR_TIMEOUT"},
    {11, "lv_ERROR_CANCELLED"},
    {12, "lv_ERROR_IO"},
    {13, "lv_ERROR_PARSE"},
    {14, "lv_ERROR_INVALID_STATE"},
    {15, "lv_ERROR_INVALID_ARGUMENT"},
    {17, "lv_ERROR_INDEX_OUT_OF_RANGE"},
    {18, "lv_ERROR_VALUE_OUT_OF_RANGE"},
    {70, "lv_ERROR_INTERNAL"},
    {100, "lv_ERROR_OUT_OF_MEMORY"},
    {101, "lv_ERROR_ALLOCATION_FAILED"},
    {102, "lv_ERROR_RESOURCE_EXHAUSTED"},
    {103, "lv_ERROR_BUFFER_TOO_SMALL"},
    {130, "lv_ERROR_PARSER_NULL_INPUT"},
    {131, "lv_ERROR_PARSER_EMPTY_INPUT"},
    {132, "lv_ERROR_PARSER_INPUT_TOO_LONG"},
    {133, "lv_ERROR_PARSER_ILLEGAL_CHARS"},
    {134, "lv_ERROR_PARSER_TOO_MANY_TOKENS"},
    {135, "lv_ERROR_PARSER_DEPTH_EXCEEDED"},
    {136, "lv_ERROR_PARSER_NODE_LIMIT"},
    {137, "lv_ERROR_PARSER_TOKEN_TOO_LONG"},
    {138, "lv_ERROR_PARSER_POOL_EXHAUSTED"},
    {200, "lv_ERROR_NODE_CONFLICT"},
    {201, "lv_ERROR_NODE_NOT_FOUND"},
    {202, "lv_ERROR_CONSTRAINT_CONFLICT"},
    {203, "lv_ERROR_CONSTRAINT_DUPLICATE"},
    {204, "lv_ERROR_INVALID_REGION"},
    {205, "lv_ERROR_INVALID_GEOM_TYPE"},
    {206, "lv_ERROR_CYCLIC_DEPENDENCY"},
    {207, "lv_ERROR_GRAPH_CORRUPTED"},
    {300, "lv_ERROR_COORD_INVALID"},
    {301, "lv_ERROR_COORD_OVERFLOW"},
    {302, "lv_ERROR_PRECISION_LOSS"},
    {303, "lv_ERROR_SYMBOLIC_EVAL_FAILED"},
    {400, "lv_ERROR_SOLVER_NO_SOLUTION"},
    {401, "lv_ERROR_SOLVER_INFINITE"},
    {402, "lv_ERROR_SOLVER_NUMERIC"},
    {403, "lv_ERROR_SOLVER_SINGULAR"},
    {404, "lv_ERROR_SOLVER_NOT_CONVERGED"},
    {405, "lv_ERROR_GROEBNER_FAILED"},
    {500, "lv_ERROR_REWRITE_NO_MATCH"},
    {501, "lv_ERROR_REWRITE_CYCLE"},
    {502, "lv_ERROR_REWRITE_DEPTH"},
    {600, "lv_ERROR_UNIFY_FAILED"},
    {601, "lv_ERROR_UNIFY_OCCUR_CHECK"},
    {602, "lv_ERROR_UNIFY_TYPE_MISMATCH"},
    {700, "lv_ERROR_FUNC_BLOCK_INVALID"},
    {701, "lv_ERROR_FUNC_BLOCK_NON_DETERMINISTIC"},
    {702, "lv_ERROR_FUNC_BLOCK_CIRCULAR"},
    {703, "lv_ERROR_FUNC_BLOCK_TYPE_ERROR"},
    {750, "lv_ERROR_PRESET_REGISTRATION_FAILED"},
    {751, "lv_ERROR_PRESET_INSTANTIATION_FAILED"},
    {800, "lv_ERROR_TYPE_MISMATCH"},
    {801, "lv_ERROR_TYPE_INFERENCE_FAILED"},
    {802, "lv_ERROR_UNIVERSE_INCONSISTENT"},
    {900, "lv_ERROR_PROOF_INVALID"},
    {901, "lv_ERROR_PROOF_INCOMPLETE"},
    {902, "lv_ERROR_PROOF_VERIFICATION_FAILED"},
    {903, "lv_ERROR_CIRCUIT_OPEN"},
};

const char *lv_status_message(int code) {
    const char *msg = status_text_lookup(s_status_messages, lv_ARRAY_SIZE(s_status_messages), code);
    return msg ? msg : "未知状态码";
}


/* ==================== 辅助函数 ==================== */

/**
 * @brief 判断状态码是否为警告
 *
 * 警告状态码保留为 -1 到 -99 范围。
 *
 * @param code 状态码
 * @return 是警告返回 1，否则返回 0
 */
int lv_status_is_warning(int code) {
    return (code < 0 && code >= -99) ? 1 : 0;
}

/**
 * @brief 获取状态码所属类别名称
 *
 * @param code 状态码
 * @return 类别名称字符串（中文，静态存储，无需释放）
 */
const char *lv_status_category(int code) {
    if (code == 0)
        return "成功";
    if (code >= 1 && code < 100)
        return "通用系统错误";
    if (code >= 100 && code < 130)
        return "内存与资源错误";
    if (code >= 130 && code < 140)
        return "解析器安全错误";
    if (code >= 200 && code < 300)
        return "约束图错误";
    if (code >= 300 && code < 400)
        return "符号坐标错误";
    if (code >= 400 && code < 500)
        return "求解器错误";
    if (code >= 500 && code < 600)
        return "重写引擎错误";
    if (code >= 600 && code < 700)
        return "合一检查错误";
    if (code >= 700 && code < 750)
        return "函数块错误";
    if (code >= 750 && code < 800)
        return "预设系统错误";
    if (code >= 800 && code < 900)
        return "类型系统错误";
    if (code >= 900 && code < 1000)
        return "证明系统错误";
    if (code < 0)
        return "警告";
    return "未分类";
}


/**
 * @brief 获取状态码的简短名称
 *
 * @param code 状态码
 * @return 状态码名称字符串（如 "lv_OK"，静态存储，无需释放）
 */
const char *lv_status_name(int code) {
    const char *name = status_text_lookup(s_status_names, lv_ARRAY_SIZE(s_status_names), code);
    return name ? name : "lv_UNKNOWN";
}
