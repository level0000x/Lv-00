#ifndef lv_PRESET_COMMON_H
#define lv_PRESET_COMMON_H
#include <stdbool.h>
#include <stdint.h>

#include "func_block_preset.h"
#include "func_block_registry.h"
#include "preset_blocks.h"
#ifdef __cplusplus
extern "C" {
#endif

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wjump-misses-init"
#endif

/* ========================================================================
 * 预设系统公共宏定义（从 preset_core.h 重导出）
 * ======================================================================== */

#ifndef PRESET_BUFFER_SIZE
#define PRESET_BUFFER_SIZE 1024
#endif

#ifndef PRESET_ID_OFFSET
#define PRESET_ID_OFFSET 1000
#endif

/* lv 兼容宏 */
#ifndef lv_PRESET_MAX_COUNT
#define lv_PRESET_MAX_COUNT PRESET_MAX_COUNT
#endif

#ifndef lv_PRESET_ID_OFFSET
#define lv_PRESET_ID_OFFSET PRESET_ID_OFFSET
#endif

#ifndef lv_PRESET_MAX_PARAMS
#define lv_PRESET_MAX_PARAMS 32
#endif

#ifndef PRESET_MAX_COUNT
#define PRESET_MAX_COUNT 10000
#endif

#ifndef PRESET_SYSTEM_VERSION_MAJOR
#define PRESET_SYSTEM_VERSION_MAJOR 4
#endif

#ifndef PRESET_SYSTEM_VERSION_MINOR
#define PRESET_SYSTEM_VERSION_MINOR 0
#endif

#ifndef PRESET_SYSTEM_VERSION_PATCH
#define PRESET_SYSTEM_VERSION_PATCH 0
#endif

#ifndef PRESET_MAX_NAME_LENGTH
#define PRESET_MAX_NAME_LENGTH 128
#endif

#ifndef PRESET_MAX_DESC_LENGTH
#define PRESET_MAX_DESC_LENGTH 512
#endif

#ifndef PRESET_MAX_INPUTS
#define PRESET_MAX_INPUTS 32
#endif

/* ========================================================================
 * 空指针检查宏（配合 goto error 模式使用）
 *
 * 注：本宏为 goto 错误处理模式的公共骨架，全项目 30+ 调用点依赖；
 *     C11 宏清理任务（B7）决定保守保留——函数化需同步改造所有调用点
 *     与 goto label 语义，风险大于收益。
 * ======================================================================== */

#ifndef PRESET_CHECK_NULL
#define PRESET_CHECK_NULL(ptr, label) \
    do {                              \
        if (!(ptr))                   \
            goto label;               \
    } while (0)
#endif

/* ========================================================================
 * 公共预设注册
 * ======================================================================== */

#define COMMON_PRESET_COUNT 1
bool preset_common_register(void);

/**
 * @brief 声明预设注册辅助函数（消除每个 preset 文件头部的重复实现）
 *
 * 用法：在 preset_xxx.c 文件头部使用：
 *   LV_DECLARE_PRESET_REGISTER(PRESET_CATEGORY_XXX)
 *
 * 这会生成一个 static helper 函数，用于向 preset_blocks_register_simple 注册预设。
 */
#define LV_DECLARE_PRESET_REGISTER(category)                                                          \
    static bool lv_preset_register_helper(const char *name, const char *desc,                         \
                                          const PresetType *in, int in_cnt, PresetType out,           \
                                          const char *math, const char *comp, bool cons, bool rev) {  \
        return preset_blocks_register_simple(name, desc, (category), in, in_cnt, out, math, comp,     \
                                             cons, rev);                                              \
    }

/**
 * @brief 单个预设注册块（消除每个注册块的重复结构）
 *
 * 用法：在 xxx_register() 函数内部使用：
 *   LV_PRESET_REGISTER(success, "name", "desc", 2, TYPE_C, "math", "comp", true, false, TYPE_A, TYPE_B);
 *
 * @param success_counter 成功计数器（递增的变量名）
 * @param name_      预设名称字符串
 * @param desc_      预设描述字符串
 * @param in_cnt_    输入数量
 * @param output_    输出类型
 * @param math_def_  数学定义字符串
 * @param comp_      复杂度字符串
 * @param cons_      bool 是否构造性
 * @param rev_       bool 是否可逆
 * @param ...        输入类型列表（可变参数，1 个或多个 PresetType 枚举）
 */
#define LV_PRESET_REGISTER(success_counter, name_, desc_, in_cnt_, output_, math_def_, comp_, cons_, rev_, ...) \
    do {                                                                                                            \
        static const PresetType _inputs[] = { __VA_ARGS__ };                                                        \
        if (lv_preset_register_helper((name_), (desc_), _inputs, (in_cnt_), (output_), (math_def_), (comp_),        \
                                      (cons_), (rev_))) {                                                          \
            (success_counter)++;                                                                                    \
        }                                                                                                           \
    } while (0)

size_t lv_safe_strncpy(char *dest, const char *src, size_t dest_size);
int lv_safe_snprintf(char *dest, size_t dest_size, const char *fmt, ...);
int preset_properties_to_string(PresetProperty properties, char *buffer, size_t buffer_size);
bool preset_properties_from_string(const char *str, PresetProperty *properties);

#ifdef __cplusplus
}
#endif
#endif
