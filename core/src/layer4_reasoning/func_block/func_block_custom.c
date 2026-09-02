/**
 * @file func_block_custom.c
 * @brief 蓝图自定义函数注册接口实现（TEN_LAYER_OPTIMIZED_PLAN §4.1.2 落地）
 *
 * 独立注册表（名称 → 注册项副本），互斥保护（复用 lv_lazy_lock 风格——
 * 本项目 func_block_registry 用 lv_once + 静态锁；此处用简单静态数组 +
 * 进程级锁 lv_lazy_lock，见 lv_thread.h）。
 */

#include "lv/func_block_custom.h"

#include <string.h>

#include "lv/lv_str_utils.h"
#include "lv/lv_thread.h"
#include "lv/lv_utils.h"

/* ============================================================
 * 注册表存储
 * ============================================================ */

#define LV_MAX_CUSTOM_FUNCTIONS 64

typedef struct {
    char *name;                  /**< 函数名副本 */
    CustomFunctionRegistration reg; /**< 注册信息（字符串指针指向下方 strdup 副本） */
    char *description;           /**< 描述副本 */
    char *category;              /**< 类别副本 */
} CustomFunctionEntry;

static CustomFunctionEntry g_custom_functions[LV_MAX_CUSTOM_FUNCTIONS];
static int g_custom_function_count = 0;

/* 静态初始化锁（进程级；lv_LAZY_LOCK_DEFINE 生成 once 回调自动 init mutex） */
lv_LAZY_LOCK_DEFINE(g_custom_lock);
#define CUSTOM_LOCK() lv_lazy_lock_lock(&g_custom_lock, g_custom_lock_init_once)
#define CUSTOM_UNLOCK() lv_lazy_lock_unlock(&g_custom_lock)

/** @brief 查找函数名索引；未找到返回 -1 */
static int custom_find(const char *name) {
    for (int i = 0; i < g_custom_function_count; i++) {
        if (lv_str_eq(g_custom_functions[i].name, name))
            return i;
    }
    return -1;
}

/** @brief 释放注册项的全部 strdup 副本 */
static void custom_entry_free(CustomFunctionEntry *e) {
    lv_free((void **) &e->name);
    lv_free((void **) &e->description);
    lv_free((void **) &e->category);
    /* meta 的指针数组（input_types/output_types/param_names）为调用方所有，
     * 注册表不深拷贝数组本身（元素指向的字符串也由调用方管理——契约：
     * 注册后调用方须保持这些数组存活，或置 NULL）。文档在头文件中注明。 */
    if (e->reg.free_user_data != NULL && e->reg.user_data != NULL)
        e->reg.free_user_data(e->reg.user_data);
    memset(e, 0, sizeof(*e));
}

/* ============================================================
 * 公共接口
 * ============================================================ */

bool lv_func_block_register_custom(const CustomFunctionRegistration *reg) {
    if (reg == NULL || reg->callback == NULL || reg->meta.name == NULL) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_PARAM, "lv_func_block_register_custom: reg/callback/name is NULL");
    }
    if (reg->meta.min_inputs < 0 || reg->meta.max_inputs < reg->meta.min_inputs) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_PARAM, "lv_func_block_register_custom: invalid input range");
    }

    CUSTOM_LOCK();

    if (custom_find(reg->meta.name) >= 0) {
        CUSTOM_UNLOCK();
        return false; /* 同名已存在 */
    }
    if (g_custom_function_count >= LV_MAX_CUSTOM_FUNCTIONS) {
        CUSTOM_UNLOCK();
        return false;
    }

    CustomFunctionEntry *e = &g_custom_functions[g_custom_function_count];
    memset(e, 0, sizeof(*e));
    e->name = lv_strdup(reg->meta.name);
    e->description = reg->meta.description ? lv_strdup(reg->meta.description) : NULL;
    e->category = reg->meta.category ? lv_strdup(reg->meta.category) : NULL;
    if (e->name == NULL || (reg->meta.description && e->description == NULL) ||
        (reg->meta.category && e->category == NULL)) {
        custom_entry_free(e);
        CUSTOM_UNLOCK();
        lv_RETURN_ERROR_BOOL(lv_ERROR_OUT_OF_MEMORY, "lv_func_block_register_custom: strdup failed");
    }
    e->reg = *reg;
    e->reg.meta.name = e->name;
    e->reg.meta.description = e->description;
    e->reg.meta.category = e->category;
    g_custom_function_count++;

    CUSTOM_UNLOCK();
    return true;
}

bool lv_func_block_unregister_custom(const char *name) {
    if (name == NULL)
        return false;
    CUSTOM_LOCK();
    int idx = custom_find(name);
    if (idx < 0) {
        CUSTOM_UNLOCK();
        return false;
    }
    custom_entry_free(&g_custom_functions[idx]);
    for (int j = idx; j < g_custom_function_count - 1; j++)
        g_custom_functions[j] = g_custom_functions[j + 1];
    g_custom_function_count--;
    CUSTOM_UNLOCK();
    return true;
}

bool lv_func_block_is_custom_registered(const char *name) {
    if (name == NULL)
        return false;
    CUSTOM_LOCK();
    bool found = custom_find(name) >= 0;
    CUSTOM_UNLOCK();
    return found;
}

const CustomFunctionMeta *lv_func_block_get_custom_meta(const char *name) {
    if (name == NULL)
        return NULL;
    CUSTOM_LOCK();
    int idx = custom_find(name);
    const CustomFunctionMeta *meta = (idx >= 0) ? &g_custom_functions[idx].reg.meta : NULL;
    CUSTOM_UNLOCK();
    return meta;
}

bool lv_func_block_register_custom_batch(const CustomFunctionRegistry *registry) {
    if (registry == NULL || (registry->count > 0 && registry->registrations == NULL)) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_PARAM, "lv_func_block_register_custom_batch: invalid registry");
    }
    for (size_t i = 0; i < registry->count; i++) {
        if (!lv_func_block_register_custom(&registry->registrations[i]))
            return false;
    }
    return true;
}

bool lv_func_block_unregister_custom_batch(const char **names, size_t count) {
    if (names == NULL && count > 0) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_PARAM, "lv_func_block_unregister_custom_batch: names is NULL");
    }
    bool all_ok = true;
    for (size_t i = 0; i < count; i++) {
        if (!lv_func_block_unregister_custom(names[i]))
            all_ok = false;
    }
    return all_ok;
}

bool lv_func_block_call_custom(const char *name, ConstraintGraph *graph, const int *inputs, int input_count,
                               int **outputs, int *output_count) {
    if (name == NULL || outputs == NULL || output_count == NULL) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_PARAM, "lv_func_block_call_custom: NULL param");
    }
    CUSTOM_LOCK();
    int idx = custom_find(name);
    CustomFunctionCallback cb = (idx >= 0) ? g_custom_functions[idx].reg.callback : NULL;
    void *user_data = (idx >= 0) ? g_custom_functions[idx].reg.user_data : NULL;
    CUSTOM_UNLOCK();
    if (cb == NULL)
        return false;
    return cb(graph, inputs, input_count, outputs, output_count, user_data);
}
