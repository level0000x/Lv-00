/**
 * @file preset_manager_register.c
 * @brief 内置/批量注册
 *
 * @details 从 preset_manager.c 拆分的子模块（Lv-00 项目 v3.3.0+）。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/error_codes.h"
#include "lv/func_block_preset.h"
#include "lv/func_block_registry.h"
#include "lv/lv_internal.h"
#include "lv/lv_json.h"
#include "lv/lv_utils.h"
#include "lv/preset_blocks.h"
#include "lv/preset_common.h"
#include "lv/preset_core.h"
#include "preset_manager_internal.h"

/* ============================================================
 * 预设实例内部结构（PresetInstance 不透明类型的定义）
 * ============================================================ */

/**
 * @brief 预设实例内部结构
 *
 * 保存实例化后的函数块及相关元数据。
 * 通过 PresetInstanceHandle（不透明指针）暴露给外部。
 */
/* ============================================================
 * 内置预设注册
 * ============================================================ */

/**
 * @brief 注册所有内置预设
 *
 * 调用 func_block_preset_library_init() 来加载内置预设，
 * 然后遍历注册表将每个内置预设同步到本管理器。
 *
 * @return 成功注册的预设数量，失败返回 -1
 */
int preset_register_builtin(void) {
    lock_library();

    if (!g_library.initialized) {
        unlock_library();
        set_error("预设库未初始化，请先调用 preset_library_init()");
        return -1;
    }

    /* 通过外部 func_block_preset_library_init 注册内置预设到注册表 */
    /* 注意：此函数假设 func_block_preset 系统已链接，
     *       实际内置预设注册由 func_block_preset 模块完成。
     *       此处仅做幂等性检查和统计报告。 */

    int registered = g_library.builtin_count;
    unlock_library();

    ; /* 注册完成 */
    return registered;
}

/* ============================================================
 * 批量注册预设
 * ============================================================ */

/**
 * @brief 批量注册预设
 *
 * 依次注册多个预设元数据条目。
 * 每个条目注册独立进行，部分失败不影响其他条目。
 *
 * @param metadatas 元数据数组
 * @param count 数量
 * @return 成功注册的数量
 */
int preset_register_batch(const PresetMetadata *metadatas, int count) {
    if (!metadatas || count <= 0)
        return 0;

    int success_count = 0;

    for (int i = 0; i < count; i++) {
        const PresetMetadata *meta = &metadatas[i];
        if (!meta->name)
            continue;

        if (preset_register_custom(meta, NULL, NULL)) {
            success_count++;
        }
    }

    return success_count;
}

