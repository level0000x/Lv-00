/**
 * @file lv_block_utils.h
 * @brief 视效块统一 create/destroy 宏
 *
 * @details 提供用于统一生成视效块创建和销毁函数的便捷宏，
 *          减少重复样板代码。支持无参数和有参数两种模式。
 *
 * @author Lv-00 Project
 */

#ifndef lv_BLOCK_UTILS_H
#define lv_BLOCK_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 声明简单无参视效块的 create/destroy 函数
 *
 * 生成一个不带参数的 create 函数和一个简单的 destroy 函数（仅 lv_free）。
 * init_body 使用 ({ ... }) GNU 语句表达式写法。
 *
 * 用法：
 *   LV_SIMPLE_BLOCK(lvWhileBlock, lv_while_block, ({
 *       block->init_port = -1;
 *       block->condition_port = -1;
 *   }))
 *
 * @param type       结构体类型名
 * @param prefix     函数名前缀（如 lv_while_block）
 * @param init_body  初始化语句块（用 ({ ... }) 包裹）
 */
#define LV_SIMPLE_BLOCK(type, prefix, init_body) \
    type *prefix##_create(void) { \
        type *block = lv_calloc(1, sizeof(type)); \
        if (!block) \
            lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "failed to allocate " #type); \
        do { init_body; } while(0); \
        return block; \
    } \
    void prefix##_destroy(type *block) { \
        lv_free((void **)&block); \
    }

/**
 * @brief 声明带参简单视效块的 create/destroy 函数
 *
 * 与 LV_SIMPLE_BLOCK 类似，但 create 函数接受指定的参数列表。
 *
 * 用法：
 *   LV_SIMPLE_BLOCK_PARAM(lvListBlock, lv_list_block, (lvListOp op), ({
 *       block->operation = op;
 *   }))
 *
 * @param type       结构体类型名
 * @param prefix     函数名前缀（如 lv_list_block）
 * @param params     参数列表（用圆括号包裹，如 (lvListOp op)）
 * @param init_body  初始化语句块（用 ({ ... }) 包裹）
 */
#define LV_SIMPLE_BLOCK_PARAM(type, prefix, params, init_body) \
    type *prefix##_create params { \
        type *block = lv_calloc(1, sizeof(type)); \
        if (!block) \
            lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "failed to allocate " #type); \
        do { init_body; } while(0); \
        return block; \
    } \
    void prefix##_destroy(type *block) { \
        lv_free((void **)&block); \
    }

#ifdef __cplusplus
}
#endif

#endif /* lv_BLOCK_UTILS_H */
