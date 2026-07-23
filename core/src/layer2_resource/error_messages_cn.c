/**
 * @file error_messages_cn.c
 * @brief 中文错误消息映射
 *
 * 将 Lv-00 统一错误码映射为中文描述字符串。
 * 遵循 error_codes.h 中的分层编号设计。
 *
 * @version 1.0.0
 */

#include <stddef.h>

#include "lv/error_codes.h"

/* ========================================================================
 * 错误条目结构
 * ======================================================================== */

typedef struct {
    lvErrorCode code;     /**< 错误码 */
    const char *message;  /**< 中文描述 */
    const char *category; /**< 错误类别 */
} ErrorMessageEntry;

/* ========================================================================
 * 中文错误消息表
 * ======================================================================== */

static const ErrorMessageEntry g_error_table[] = {
    /* 成功 */
    {lv_OK, "操作成功", "成功"},

    /* 通用系统错误 (1-99) */
    {lv_ERROR_UNKNOWN, "未知错误", "系统"},
    {lv_ERROR_INVALID_PARAM, "无效参数", "系统"},
    {lv_ERROR_NULL_POINTER, "空指针错误", "系统"},
    {lv_ERROR_NOT_INITIALIZED, "系统未初始化", "系统"},
    {lv_ERROR_ALREADY_EXISTS, "资源已存在", "系统"},
    {lv_ERROR_NOT_FOUND, "资源未找到", "系统"},
    {lv_ERROR_UNSUPPORTED, "不支持的操作", "系统"},
    {lv_ERROR_OVERFLOW, "数值溢出", "系统"},
    {lv_ERROR_UNDERFLOW, "数值下溢", "系统"},
    {lv_ERROR_TIMEOUT, "操作超时", "系统"},
    {lv_ERROR_CANCELLED, "操作被取消", "系统"},
    {lv_ERROR_IO, "输入输出错误", "系统"},
    {lv_ERROR_PARSE, "解析错误", "系统"},
    {lv_ERROR_INVALID_STATE, "无效状态", "系统"},
    {lv_ERROR_INDEX_OUT_OF_RANGE, "索引越界", "系统"},
    {lv_ERROR_VALUE_OUT_OF_RANGE, "数值越界", "系统"},
    {lv_ERROR_INTERNAL, "内部错误", "系统"},

    /* 解析器安全错误 (130-139) */
    {lv_ERROR_PARSER_NULL_INPUT, "解析器输入为 NULL", "解析器"},
    {lv_ERROR_PARSER_EMPTY_INPUT, "解析器输入为空字符串", "解析器"},
    {lv_ERROR_PARSER_INPUT_TOO_LONG, "输入长度超出限制", "解析器"},
    {lv_ERROR_PARSER_ILLEGAL_CHARS, "输入含非法字符", "解析器"},
    {lv_ERROR_PARSER_TOO_MANY_TOKENS, "词法单元数量超出限制", "解析器"},
    {lv_ERROR_PARSER_DEPTH_EXCEEDED, "语法树深度超出限制", "解析器"},
    {lv_ERROR_PARSER_NODE_LIMIT, "语法树节点数超出限制", "解析器"},
    {lv_ERROR_PARSER_TOKEN_TOO_LONG, "词法单元长度超出限制", "解析器"},
    {lv_ERROR_PARSER_POOL_EXHAUSTED, "解析器内存池耗尽", "解析器"},

    /* 内存与资源错误 (100-199) */
    {lv_ERROR_OUT_OF_MEMORY, "内存不足", "内存"},
    {lv_ERROR_ALLOCATION_FAILED, "内存分配失败", "内存"},
    {lv_ERROR_RESOURCE_EXHAUSTED, "资源耗尽", "内存"},
    {lv_ERROR_BUFFER_TOO_SMALL, "缓冲区太小", "内存"},

    /* 约束图错误 (200-299) */
    {lv_ERROR_NODE_CONFLICT, "节点冲突", "约束图"},
    {lv_ERROR_NODE_NOT_FOUND, "节点未找到", "约束图"},
    {lv_ERROR_CONSTRAINT_CONFLICT, "约束冲突", "约束图"},
    {lv_ERROR_CONSTRAINT_DUPLICATE, "重复约束", "约束图"},
    {lv_ERROR_INVALID_REGION, "无效区域", "约束图"},
    {lv_ERROR_INVALID_GEOM_TYPE, "无效几何类型", "约束图"},
    {lv_ERROR_CYCLIC_DEPENDENCY, "循环依赖", "约束图"},
    {lv_ERROR_GRAPH_CORRUPTED, "图结构损坏", "约束图"},

    /* 符号坐标错误 (300-399) */
    {lv_ERROR_COORD_INVALID, "无效坐标", "符号坐标"},
    {lv_ERROR_COORD_OVERFLOW, "坐标溢出", "符号坐标"},
    {lv_ERROR_PRECISION_LOSS, "精度丢失", "符号坐标"},
    {lv_ERROR_SYMBOLIC_EVAL_FAILED, "符号求值失败", "符号坐标"},

    /* 求解器错误 (400-499) */
    {lv_ERROR_SOLVER_NO_SOLUTION, "方程无解", "求解器"},
    {lv_ERROR_SOLVER_INFINITE, "方程有无穷多解", "求解器"},
    {lv_ERROR_SOLVER_NUMERIC, "数值计算错误", "求解器"},
    {lv_ERROR_SOLVER_SINGULAR, "奇异矩阵", "求解器"},
    {lv_ERROR_SOLVER_NOT_CONVERGED, "迭代未收敛", "求解器"},
    {lv_ERROR_GROEBNER_FAILED, "Gr\u00f6bner 基计算失败", "求解器"},

    /* 重写引擎错误 (500-599) */
    {lv_ERROR_REWRITE_NO_MATCH, "无匹配规则", "重写"},
    {lv_ERROR_REWRITE_CYCLE, "重写循环", "重写"},
    {lv_ERROR_REWRITE_DEPTH, "重写深度超出限制", "重写"},

    /* 合一检查错误 (600-699) */
    {lv_ERROR_UNIFY_FAILED, "合一失败", "合一"},
    {lv_ERROR_UNIFY_OCCUR_CHECK, "发生检查失败", "合一"},
    {lv_ERROR_UNIFY_TYPE_MISMATCH, "类型不匹配", "合一"},

    /* 函数块错误 (700-799) */
    {lv_ERROR_FUNC_BLOCK_INVALID, "无效函数块", "函数块"},
    {lv_ERROR_FUNC_BLOCK_NON_DETERMINISTIC, "非确定性函数块", "函数块"},
    {lv_ERROR_FUNC_BLOCK_CIRCULAR, "循环函数块", "函数块"},
    {lv_ERROR_FUNC_BLOCK_TYPE_ERROR, "函数块类型错误", "函数块"},
    {lv_ERROR_PRESET_REGISTRATION_FAILED, "预设注册失败", "预设"},
    {lv_ERROR_PRESET_INSTANTIATION_FAILED, "预设实例化失败", "预设"},

    /* 类型系统错误 (800-899) */
    {lv_ERROR_TYPE_MISMATCH, "类型不匹配", "类型"},
    {lv_ERROR_TYPE_INFERENCE_FAILED, "类型推断失败", "类型"},
    {lv_ERROR_UNIVERSE_INCONSISTENT, "宇宙层级不一致", "类型"},

    /* 证明系统错误 (900-999) */
    {lv_ERROR_PROOF_INVALID, "无效证明", "证明"},
    {lv_ERROR_PROOF_INCOMPLETE, "证明不完整", "证明"},
    {lv_ERROR_PROOF_VERIFICATION_FAILED, "证明验证失败", "证明"},
    {lv_ERROR_CIRCUIT_OPEN, "熔断器已跳闸", "证明"},
};

#define ERROR_TABLE_SIZE (sizeof(g_error_table) / sizeof(g_error_table[0]))

/* ========================================================================
 * 公共 API
 * ======================================================================== */

/**
 * @brief 获取错误码对应的中文描述
 * @param code 错误码
 * @return 中文描述字符串（静态存储，无需释放）
 */
const char *lv_error_message_cn(lvErrorCode code) {
    for (size_t i = 0; i < ERROR_TABLE_SIZE; i++) {
        if (g_error_table[i].code == code) {
            return g_error_table[i].message;
        }
    }
    return "未知错误码";
}

/**
 * @brief 获取错误码所属的中文类别
 * @param code 错误码
 * @return 中文类别字符串
 */
const char *lv_error_category_cn(lvErrorCode code) {
    for (size_t i = 0; i < ERROR_TABLE_SIZE; i++) {
        if (g_error_table[i].code == code) {
            return g_error_table[i].category;
        }
    }
    return "未知";
}

/**
 * @brief 获取中文错误消息表的条目数量
 * @return 条目数量
 */
int lv_error_message_cn_count(void) {
    return (int) ERROR_TABLE_SIZE;
}
