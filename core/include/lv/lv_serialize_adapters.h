/**
 * @file lv_serialize_adapters.h
 * @brief 内置序列化适配器注册
 *
 * @details 把业务序列化函数（graph_serialize_to_json 返回 char*、
 *          graph_deserialize_from_json 返回新对象）包装为序列化注册表的
 *          lvSerializeFunc / lvDeserializeFunc 形态，接入统一注册表。
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

#ifndef lv_SERIALIZE_ADAPTERS_H
#define lv_SERIALIZE_ADAPTERS_H

#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 注册内置序列化适配器（幂等，可重复调用）
 *
 * 当前注册：
 *   - "ConstraintGraph" + "json"
 *     序列化：graph_serialize_to_json（写入 lvStorage）
 *     反序列化：graph_deserialize_from_json（obj 为 ConstraintGraph**，
 *               成功时 *obj 指向新分配的对象）
 *
 * 可在 lv_init() 模块初始化路径（lv_module_register 批量注册模式）调用，
 * 也可由调用方直接调用（注册表支持重复注册覆盖，幂等）。
 *
 * @return true 注册成功，false 失败
 */
lv_PUBLIC_API bool lv_serialize_register_graph_adapters(void);

/**
 * @brief 清理内置序列化适配器（模块 cleanup 回调）
 *
 * 完整释放序列化/验证/存储后端注册表结构（entries 数组 / 哈希索引 /
 * 互斥锁）并重置 once 守卫，消除 lv_init 注册的 ConstraintGraph 序列化
 * 条目在 lv_cleanup 后的内存泄漏（~1162 字节）。幂等，可重复调用。
 */
lv_PUBLIC_API void lv_serialize_cleanup_adapters(void);

#ifdef __cplusplus
}
#endif

#endif /* lv_SERIALIZE_ADAPTERS_H */
