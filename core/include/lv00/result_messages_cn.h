/**
 * @file result_messages_cn.h
 * @brief Lv-00 中文结果信息转换系统
 *
 * @details 提供枚举值到中文字符串的转换函数，
 *          涵盖函数块、求解器、证明系统等核心模块的返回结果。
 *
 * 【使用说明】
 * - determinism_state_to_string_cn() - 确定性状态转中文
 * - pack_result_to_string_cn() - 打包结果转中文
 * - instantiate_result_to_string_cn() - 实例化结果转中文
 * - solver_result_to_string_cn() - 求解器结果转中文
 * - normalize_result_to_string_cn() - 归一化结果转中文
 *
 * @author Lv-00 Project
 * @version 3.5.0
 */

#ifndef LV00_RESULT_MESSAGES_CN_H
#define LV00_RESULT_MESSAGES_CN_H

#include <stdbool.h>
#include <stdint.h>

#include "func_block.h"
#include "solver.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 函数块系统结果转中文
 * ============================================================ */

/**
 * @brief 将确定性状态转换为中文描述字符串
 *
 * @param state 确定性状态枚举值
 * @return 对应的中文描述字符串
 *
 * @note 返回静态字符串，无需释放
 *
 * 示例:
 * @code
 *   DeterminismState state = func_block_get_determinism(fb);
 *   printf("确定性状态：%s\n", determinism_state_to_string_cn(state));
 * @endcode
 */
const char *determinism_state_to_string_cn(int state);

/**
 * @brief 将确定性状态转换为简短的英文缩写（用于日志）
 *
 * @param state 确定性状态枚举值
 * @return 简短英文缩写（如 "UNVERIFIED", "VERIFIED" 等）
 */
const char *determinism_state_to_abbr(int state);

/**
 * @brief 将打包结果转换为中文描述字符串
 *
 * @param result 打包结果枚举值
 * @return 对应的中文描述字符串
 *
 * @note 返回静态字符串，无需释放
 */
const char *pack_result_to_string_cn(int result);

/**
 * @brief 将实例化结果转换为中文描述字符串
 *
 * @param result 实例化结果枚举值
 * @return 对应的中文描述字符串
 *
 * @note 返回静态字符串，无需释放
 */
const char *instantiate_result_to_string_cn(int result);

/* ============================================================
 * 求解器结果转中文
 * ============================================================ */

/**
 * @brief 将求解器结果转换为中文描述字符串
 *
 * @param result 求解器结果枚举值
 * @return 对应的中文描述字符串
 *
 * @note 返回静态字符串，无需释放
 */
const char *solver_result_to_string_cn(int result);

/**
 * @brief 将求解器状态转换为中文描述字符串
 *
 * @param status 求解器状态枚举值
 * @return 对应的中文描述字符串
 */
const char *solver_status_to_string_cn(int status);

/* ============================================================
 * 归一化结果转中文
 * ============================================================ */

/**
 * @brief 将归一化结果转换为中文描述字符串
 *
 * @param result 归一化结果枚举值
 * @return 对应的中文描述字符串
 */
const char *normalize_result_to_string_cn(int result);

/* ============================================================
 * 几何类型转中文
 * ============================================================ */

/**
 * @brief 将几何类型转换为中文描述字符串
 *
 * @param type 几何节点类型
 * @return 对应的中文描述字符串
 */
const char *geom_type_to_string_cn(int type);

/**
 * @brief 将约束类型转换为中文描述字符串
 *
 * @param type 约束类型
 * @return 对应的中文描述字符串
 */
const char *constraint_type_to_string_cn(int type);

/* ============================================================
 * 证明系统结果转中文
 * ============================================================ */

/**
 * @brief 将证明结果转换为中文描述字符串
 *
 * @param result 证明结果枚举值
 * @return 对应的中文描述字符串
 */
const char *proof_result_to_string_cn(int result);

/**
 * @brief 将证明状态转换为中文描述字符串
 *
 * @param status 证明状态枚举值
 * @return 对应的中文描述字符串
 */
const char *proof_status_to_string_cn(int status);

/* ============================================================
 * 便捷宏：格式化中文错误描述
 * ============================================================ */

/**
 * @brief 生成格式化的确定性状态描述
 * @param state 确定性状态
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return 写入的字符数
 */
int format_determinism_state_cn(int state, char *buf, size_t buf_size);

/**
 * @brief 生成格式化的打包结果描述
 * @param result 打包结果
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return 写入的字符数
 */
int format_pack_result_cn(int result, char *buf, size_t buf_size);

/**
 * @brief 生成格式化的求解器结果描述
 * @param result 求解器结果
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return 写入的字符数
 */
int format_solver_result_cn(int result, char *buf, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* LV00_RESULT_MESSAGES_CN_H */
