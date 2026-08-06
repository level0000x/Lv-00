/**
 * @file lv_convenience.h
 * @brief Lv-00 高层便捷 API 声明
 *
 * @details 声明 core/src/core/lv_convenience.c 中实现的高层便捷函数：
 *   - lv_prove():          执行证明
 *   - lv_preset_load():    加载预设
 *   - lv_preset_unload():  卸载预设
 *   - lv_preset_apply():   应用预设
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

#ifndef lv_CONVENIENCE_H
#define lv_CONVENIENCE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv/context.h"

/**
 * @brief 执行证明 -- 高层便捷接口
 *
 * 将 goal 文本解析为约束并调用引擎的重写-求解流水线完成证明。
 *
 * @param ctx   上下文指针（非 NULL，必须处于 IDLE 或 COMPLETE 状态）
 * @param goal  证明目标的文本描述（如 "triangle ABC is equilateral"）
 *              如果为 NULL，使用上下文中已有的约束图进行证明
 * @return 0    证明成功
 * @return -1   参数无效（ctx 为 NULL）
 * @return -2   上下文状态不合法（不在 IDLE/COMPLETE 状态）
 * @return -3   解析阶段失败
 * @return -4   推理阶段失败（矛盾、超时或熔断）
 */
int lv_prove(lvContext *ctx, const char *goal);

/**
 * @brief 加载预设 -- 将指定名称的预设注册到上下文
 *
 * @param ctx  上下文指针（非 NULL）
 * @param name 预设名称（如 "midpoint", "circumcenter", "rotate" 等）
 * @return 0   加载成功
 * @return -1  参数无效（ctx 或 name 为 NULL）
 * @return -2  预设库未初始化
 * @return -3  指定名称的预设不存在
 * @return -4  内存分配失败
 */
int lv_preset_load(lvContext *ctx, const char *name);

/**
 * @brief 卸载预设 -- 从上下文中移除指定预设的加载标记
 *
 * @param ctx  上下文指针（非 NULL）
 * @param name 预设名称
 * @return 0   卸载成功
 * @return -1  参数无效（ctx 或 name 为 NULL）
 * @return -3  上下文中未找到该预设的加载记录
 */
int lv_preset_unload(lvContext *ctx, const char *name);

/**
 * @brief 应用预设 -- 将指定预设实例化并应用到当前约束图
 *
 * @param ctx  上下文指针（非 NULL，应处于 IDLE 或 PARSING 状态）
 * @param name 预设名称（必须已通过 lv_preset_load 加载）
 * @return 0   应用成功
 * @return -1  参数无效（ctx 或 name 为 NULL）
 * @return -2  上下文状态不允许应用预设
 * @return -3  指定预设未加载
 * @return -4  实例化失败
 */
int lv_preset_apply(lvContext *ctx, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* lv_CONVENIENCE_H */
