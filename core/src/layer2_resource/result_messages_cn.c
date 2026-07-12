/**
 * @file result_messages_cn.c
 * @brief 中文结果消息映射
 *
 * 将 Lv-00 各模块的执行结果状态码映射为中文描述字符串。
 * 覆盖求解器、归一化、重写、证明等核心模块的结果状态。
 *
 * @version 1.0.0
 */

#include <stddef.h>

/* ========================================================================
 * 结果状态码（与引擎求解结果对应）
 * ======================================================================== */

typedef enum {
    LV00_RESULT_SUCCESS = 0,           /**< 求解成功 */
    LV00_RESULT_NO_SOLUTION = 1,       /**< 无解 */
    LV00_RESULT_INFINITE_SOLUTIONS = 2,/**< 无穷多解 */
    LV00_RESULT_TIMEOUT = 3,           /**< 求解超时 */
    LV00_RESULT_CANCELLED = 4,         /**< 被用户取消 */
    LV00_RESULT_NUMERIC_ERROR = 5,     /**< 数值错误 */
    LV00_RESULT_MEMORY_ERROR = 6,      /**< 内存错误 */
    LV00_RESULT_GRAPH_ERROR = 7,       /**< 约束图错误 */
    LV00_RESULT_NORMALIZE_SUCCESS = 10,/**< 归一化成功 */
    LV00_RESULT_NORMALIZE_PARTIAL = 11,/**< 归一化部分完成 */
    LV00_RESULT_REWRITE_SUCCESS = 20,  /**< 重写成功 */
    LV00_RESULT_REWRITE_NO_MATCH = 21, /**< 无匹配规则 */
    LV00_RESULT_PROOF_VERIFIED = 30,   /**< 证明已验证 */
    LV00_RESULT_PROOF_FAILED = 31,     /**< 证明失败 */
    LV00_RESULT_PROOF_INCOMPLETE = 32  /**< 证明不完整 */
} Lv00ResultCode;

/* ========================================================================
 * 结果消息条目
 * ======================================================================== */

typedef struct {
    int code;             /**< 结果码 */
    const char *message;  /**< 中文描述 */
    const char *detail;   /**< 详细说明 */
} ResultMessageEntry;

static const ResultMessageEntry g_result_table[] = {
    {LV00_RESULT_SUCCESS,
     "求解成功",
     "所有约束已满足，几何构造一致"},

    {LV00_RESULT_NO_SOLUTION,
     "无解",
     "约束系统矛盾，无法找到满足所有约束的几何构造"},

    {LV00_RESULT_INFINITE_SOLUTIONS,
     "无穷多解",
     "约束不足，存在无穷多个满足条件的几何构造"},

    {LV00_RESULT_TIMEOUT,
     "求解超时",
     "求解过程未能在规定时间内完成，请尝试简化问题或增加超时时间"},

    {LV00_RESULT_CANCELLED,
     "操作已取消",
     "求解过程被用户或系统取消"},

    {LV00_RESULT_NUMERIC_ERROR,
     "数值计算错误",
     "求解过程中遇到数值不稳定或精度溢出"},

    {LV00_RESULT_MEMORY_ERROR,
     "内存错误",
     "系统内存不足，无法完成求解"},

    {LV00_RESULT_GRAPH_ERROR,
     "约束图错误",
     "约束图结构异常，可能包含损坏的节点或约束"},

    {LV00_RESULT_NORMALIZE_SUCCESS,
     "归一化成功",
     "约束图已成功归一化，同构结构已合并"},

    {LV00_RESULT_NORMALIZE_PARTIAL,
     "归一化部分完成",
     "约束图部分归一化，某些子图可能需要手动处理"},

    {LV00_RESULT_REWRITE_SUCCESS,
     "重写成功",
     "所有匹配的重写规则已成功应用"},

    {LV00_RESULT_REWRITE_NO_MATCH,
     "无匹配规则",
     "当前约束图中未找到可匹配的重写规则"},

    {LV00_RESULT_PROOF_VERIFIED,
     "证明已验证",
     "命题证明已通过验证，结论成立"},

    {LV00_RESULT_PROOF_FAILED,
     "证明失败",
     "命题证明验证失败，可能存在推理错误"},

    {LV00_RESULT_PROOF_INCOMPLETE,
     "证明不完整",
     "证明存在未完成的分支或未解决的子目标"},
};

#define RESULT_TABLE_SIZE (sizeof(g_result_table) / sizeof(g_result_table[0]))

/* ========================================================================
 * 公共 API
 * ======================================================================== */

/**
 * @brief 获取结果码对应的中文消息
 * @param code 结果码
 * @return 中文消息字符串（静态存储，无需释放）
 */
const char *lv00_result_message_cn(int code)
{
    for (size_t i = 0; i < RESULT_TABLE_SIZE; i++) {
        if (g_result_table[i].code == code) {
            return g_result_table[i].message;
        }
    }
    return "未知结果";
}

/**
 * @brief 获取结果码对应的中文详细说明
 * @param code 结果码
 * @return 详细说明字符串（静态存储，无需释放）
 */
const char *lv00_result_detail_cn(int code)
{
    for (size_t i = 0; i < RESULT_TABLE_SIZE; i++) {
        if (g_result_table[i].code == code) {
            return g_result_table[i].detail;
        }
    }
    return "无详细说明";
}

/**
 * @brief 获取结果消息表的条目数量
 * @return 条目数量
 */
int lv00_result_message_cn_count(void)
{
    return (int)RESULT_TABLE_SIZE;
}
