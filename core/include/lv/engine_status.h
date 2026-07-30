#ifndef lv_ENGINE_STATUS_H
#define lv_ENGINE_STATUS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ── 引擎状态码 ──
 * 【枚举值命名规范】所有枚举值使用 UPPER_SNAKE_CASE
 *
 * 独立头文件，供 engine.h 和 context.h 等共用，
 * 避免因类型统一而引入 engine.h 的大量传递依赖。
 */
typedef enum {
    ENGINE_STATUS_OK,                  /**< 操作成功完成 */
    ENGINE_STATUS_OUT_OF_MEMORY,       /**< 内存分配失败 */
    ENGINE_STATUS_INVALID_STATE,       /**< 引擎处于无效状态（如未初始化即调用） */
    ENGINE_STATUS_INVALID_ARGUMENT,    /**< 传入参数无效（空指针、越界等） */
    ENGINE_STATUS_CONSTRAINT_CONFLICT, /**< 约束冲突：无法满足的约束条件 */
    ENGINE_STATUS_MODULE_ERROR,        /**< 模块加载/执行错误 */
    ENGINE_STATUS_ERROR_INTERNAL       /**< 内部错误 */
} EngineStatus;

#ifdef __cplusplus
}
#endif

#endif /* lv_ENGINE_STATUS_H */
