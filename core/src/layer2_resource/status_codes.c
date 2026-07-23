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

#include "lv/lv.h"
#include "lv/status_codes.h"
#include "lv/error_codes.h"

/* ==================== 状态码判断函数 ==================== */

int lv_status_is_success(int code)
{
    return code == 0 ? 1 : 0;
}

int lv_status_is_error(int code)
{
    return code != 0 ? 1 : 0;
}

/* ==================== 状态码描述映射 ==================== */

const char *lv_status_message(int code)
{
    switch (code) {
    /* --- 成功 --- */
    case 0:   return "操作成功";

    /* --- 通用系统错误 (1-99) --- */
    case 1:   return "未知错误";
    case 2:   return "无效参数";
    case 3:   return "空指针";
    case 4:   return "未初始化";
    case 5:   return "已存在";
    case 6:   return "未找到";
    case 7:   return "不支持的操作";
    case 8:   return "数值溢出";
    case 9:   return "数值下溢";
    case 10:  return "操作超时";
    case 11:  return "操作被取消";
    case 12:  return "IO错误";
    case 13:  return "解析错误";
    case 14:  return "无效状态";
    case 15:  return "无效参数（已废弃，请使用错误码2）";
    case 17:  return "索引越界";
    case 18:  return "数值越界";
    case 70:  return "内部错误";

    /* --- 解析器安全错误 (130-139) --- */
    case 130: return "解析器输入为NULL";
    case 131: return "解析器输入为空字符串";
    case 132: return "解析器输入长度超限";
    case 133: return "输入含非法控制字符或null字节";
    case 134: return "token数量超限";
    case 135: return "AST深度超限";
    case 136: return "AST节点数超限";
    case 137: return "token长度超限";
    case 138: return "内存池耗尽";

    /* --- 内存与资源错误 (100-199) --- */
    case 100: return "内存不足";
    case 101: return "内存分配失败";
    case 102: return "资源耗尽";
    case 103: return "缓冲区太小";

    /* --- 约束图错误 (200-299) --- */
    case 200: return "节点冲突";
    case 201: return "节点未找到";
    case 202: return "约束冲突";
    case 203: return "重复约束";
    case 204: return "无效区域";
    case 205: return "无效几何类型";
    case 206: return "循环依赖";
    case 207: return "图结构损坏";

    /* --- 符号坐标错误 (300-399) --- */
    case 300: return "无效坐标";
    case 301: return "坐标溢出";
    case 302: return "精度丢失";
    case 303: return "符号求值失败";

    /* --- 求解器错误 (400-499) --- */
    case 400: return "无解";
    case 401: return "无穷多解";
    case 402: return "数值计算错误";
    case 403: return "奇异矩阵";
    case 404: return "未收敛";
    case 405: return "Groebner基计算失败";

    /* --- 重写引擎错误 (500-599) --- */
    case 500: return "无匹配规则";
    case 501: return "重写循环";
    case 502: return "重写深度超限";

    /* --- 合一检查错误 (600-699) --- */
    case 600: return "合一失败";
    case 601: return "发生检查失败";
    case 602: return "类型不匹配";

    /* --- 函数块错误 (700-799) --- */
    case 700: return "无效函数块";
    case 701: return "非确定性函数块";
    case 702: return "循环函数块";
    case 703: return "函数块类型错误";

    /* --- 预设系统错误 (750-799) --- */
    case 750: return "预设注册失败";
    case 751: return "预设实例化失败";

    /* --- 类型系统错误 (800-899) --- */
    case 800: return "类型不匹配";
    case 801: return "类型推断失败";
    case 802: return "宇宙层级不一致";

    /* --- 证明系统错误 (900-999) --- */
    case 900: return "无效证明";
    case 901: return "证明不完整";
    case 902: return "证明验证失败";
    case 903: return "熔断器已跳闸（OPEN态）";

    default:  return "未知状态码";
    }
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
int lv_status_is_warning(int code)
{
    return (code < 0 && code >= -99) ? 1 : 0;
}

/**
 * @brief 获取状态码所属类别名称
 *
 * @param code 状态码
 * @return 类别名称字符串（中文，静态存储，无需释放）
 */
const char *lv_status_category(int code)
{
    if (code == 0)          return "成功";
    if (code >= 1 && code < 100)    return "通用系统错误";
    if (code >= 100 && code < 130)  return "内存与资源错误";
    if (code >= 130 && code < 140)  return "解析器安全错误";
    if (code >= 200 && code < 300)  return "约束图错误";
    if (code >= 300 && code < 400)  return "符号坐标错误";
    if (code >= 400 && code < 500)  return "求解器错误";
    if (code >= 500 && code < 600)  return "重写引擎错误";
    if (code >= 600 && code < 700)  return "合一检查错误";
    if (code >= 700 && code < 750)  return "函数块错误";
    if (code >= 750 && code < 800)  return "预设系统错误";
    if (code >= 800 && code < 900)  return "类型系统错误";
    if (code >= 900 && code < 1000) return "证明系统错误";
    if (code < 0)                   return "警告";
    return "未分类";
}

/**
 * @brief 获取状态码的简短名称
 *
 * @param code 状态码
 * @return 状态码名称字符串（如 "lv_OK"，静态存储，无需释放）
 */
const char *lv_status_name(int code)
{
    switch (code) {
    case 0:   return "lv_OK";
    case 1:   return "lv_ERROR_UNKNOWN";
    case 2:   return "lv_ERROR_INVALID_PARAM";
    case 3:   return "lv_ERROR_NULL_POINTER";
    case 4:   return "lv_ERROR_NOT_INITIALIZED";
    case 5:   return "lv_ERROR_ALREADY_EXISTS";
    case 6:   return "lv_ERROR_NOT_FOUND";
    case 7:   return "lv_ERROR_UNSUPPORTED";
    case 8:   return "lv_ERROR_OVERFLOW";
    case 9:   return "lv_ERROR_UNDERFLOW";
    case 10:  return "lv_ERROR_TIMEOUT";
    case 11:  return "lv_ERROR_CANCELLED";
    case 12:  return "lv_ERROR_IO";
    case 13:  return "lv_ERROR_PARSE";
    case 14:  return "lv_ERROR_INVALID_STATE";
    case 15:  return "lv_ERROR_INVALID_ARGUMENT";
    case 17:  return "lv_ERROR_INDEX_OUT_OF_RANGE";
    case 18:  return "lv_ERROR_VALUE_OUT_OF_RANGE";
    case 70:  return "lv_ERROR_INTERNAL";
    case 100: return "lv_ERROR_OUT_OF_MEMORY";
    case 101: return "lv_ERROR_ALLOCATION_FAILED";
    case 102: return "lv_ERROR_RESOURCE_EXHAUSTED";
    case 103: return "lv_ERROR_BUFFER_TOO_SMALL";
    case 130: return "lv_ERROR_PARSER_NULL_INPUT";
    case 131: return "lv_ERROR_PARSER_EMPTY_INPUT";
    case 132: return "lv_ERROR_PARSER_INPUT_TOO_LONG";
    case 133: return "lv_ERROR_PARSER_ILLEGAL_CHARS";
    case 134: return "lv_ERROR_PARSER_TOO_MANY_TOKENS";
    case 135: return "lv_ERROR_PARSER_DEPTH_EXCEEDED";
    case 136: return "lv_ERROR_PARSER_NODE_LIMIT";
    case 137: return "lv_ERROR_PARSER_TOKEN_TOO_LONG";
    case 138: return "lv_ERROR_PARSER_POOL_EXHAUSTED";
    case 200: return "lv_ERROR_NODE_CONFLICT";
    case 201: return "lv_ERROR_NODE_NOT_FOUND";
    case 202: return "lv_ERROR_CONSTRAINT_CONFLICT";
    case 203: return "lv_ERROR_CONSTRAINT_DUPLICATE";
    case 204: return "lv_ERROR_INVALID_REGION";
    case 205: return "lv_ERROR_INVALID_GEOM_TYPE";
    case 206: return "lv_ERROR_CYCLIC_DEPENDENCY";
    case 207: return "lv_ERROR_GRAPH_CORRUPTED";
    case 300: return "lv_ERROR_COORD_INVALID";
    case 301: return "lv_ERROR_COORD_OVERFLOW";
    case 302: return "lv_ERROR_PRECISION_LOSS";
    case 303: return "lv_ERROR_SYMBOLIC_EVAL_FAILED";
    case 400: return "lv_ERROR_SOLVER_NO_SOLUTION";
    case 401: return "lv_ERROR_SOLVER_INFINITE";
    case 402: return "lv_ERROR_SOLVER_NUMERIC";
    case 403: return "lv_ERROR_SOLVER_SINGULAR";
    case 404: return "lv_ERROR_SOLVER_NOT_CONVERGED";
    case 405: return "lv_ERROR_GROEBNER_FAILED";
    case 500: return "lv_ERROR_REWRITE_NO_MATCH";
    case 501: return "lv_ERROR_REWRITE_CYCLE";
    case 502: return "lv_ERROR_REWRITE_DEPTH";
    case 600: return "lv_ERROR_UNIFY_FAILED";
    case 601: return "lv_ERROR_UNIFY_OCCUR_CHECK";
    case 602: return "lv_ERROR_UNIFY_TYPE_MISMATCH";
    case 700: return "lv_ERROR_FUNC_BLOCK_INVALID";
    case 701: return "lv_ERROR_FUNC_BLOCK_NON_DETERMINISTIC";
    case 702: return "lv_ERROR_FUNC_BLOCK_CIRCULAR";
    case 703: return "lv_ERROR_FUNC_BLOCK_TYPE_ERROR";
    case 750: return "lv_ERROR_PRESET_REGISTRATION_FAILED";
    case 751: return "lv_ERROR_PRESET_INSTANTIATION_FAILED";
    case 800: return "lv_ERROR_TYPE_MISMATCH";
    case 801: return "lv_ERROR_TYPE_INFERENCE_FAILED";
    case 802: return "lv_ERROR_UNIVERSE_INCONSISTENT";
    case 900: return "lv_ERROR_PROOF_INVALID";
    case 901: return "lv_ERROR_PROOF_INCOMPLETE";
    case 902: return "lv_ERROR_PROOF_VERIFICATION_FAILED";
    case 903: return "lv_ERROR_CIRCUIT_OPEN";
    default:  return "lv_UNKNOWN";
    }
}
