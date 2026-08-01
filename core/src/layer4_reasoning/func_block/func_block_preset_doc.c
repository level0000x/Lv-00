/**
 * @file func_block_preset_doc.c
 * @brief 预设文档生成
 *
 * 从 func_block_preset.c 拆分的模块之一。
 *
 * @version v5.0.0
 */

#include "func_block_preset_internal.h"
#include "lv/lv_xmacro.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error_codes.h"
#include "lv_internal.h"
#include "lv_utils.h"

/* ============================================================
 * 文档生成
 * ============================================================ */

size_t func_block_preset_generate_doc(const char *preset_name, char *out_buffer, size_t buffer_size) {
    if (!preset_name || !out_buffer || buffer_size == 0)
        return 0;

    const PresetMetadata *m = func_block_preset_get_metadata(preset_name);
    if (!m)
        return 0;

    /* 修复：动态分配属性字符串缓冲区，避免固定 256 字节栈缓冲区不够用的问题 */
    size_t props_buf_size = 512;
    char *props_buffer = (char *) lv_malloc(props_buf_size);
    if (!props_buffer)
        return 0;
    func_block_preset_properties_string(m->properties, props_buffer, props_buf_size);

    int written = snprintf(out_buffer, buffer_size,
                           "# %s\n\n"
                           "## 描述\n\n%s\n\n"
                           "## 数学定义\n\n`%s`\n\n"
                           "## 类别\n\n%s\n\n"
                           "## 性质\n\n%s\n\n"
                           "## 复杂度\n\n%s\n\n"
                           "## 参数\n\n"
                           "- 输入: %d个\n"
                           "- 输出: %d个\n\n"
                           "## 版本\n\n%d.%d.%d\n",
                           m->name, m->description, m->mathematical_def, func_block_preset_category_string(m->category),
                           props_buffer[0] ? props_buffer : "无", func_block_preset_complexity_string(m->complexity),
                           m->input_count, m->output_count, m->version_major, m->version_minor, m->version_patch);

    if (written < 0 || (size_t) written >= buffer_size) {
        lv_free((void **) &props_buffer); /* 释放动态分配的属性缓冲区 */
        return buffer_size + 1;           /* 指示缓冲区不足 */
    }

    lv_free((void **) &props_buffer); /* 释放动态分配的属性缓冲区 */
    return (size_t) written + 1;      /* 包含\0 */
}

size_t func_block_preset_generate_index(char *out_buffer, size_t buffer_size) {
    if (!out_buffer || buffer_size == 0)
        return 0;

    /* 修复：使用 size_t 类型的 written 避免累加时 int 溢出 */
    size_t written = 0;
    size_t remaining = buffer_size;

    int n = snprintf(out_buffer, remaining,
                     "# Lv-00 预设函数块库\n\n"
                     "## 概述\n\n"
                     "本库提供 %d 个标准化几何预设函数块，用于理论数学研究。\n\n"
                     "## 分类索引\n\n",
                     g_preset_library.count);

    /* 检查 snprintf 返回值：n < 0 表示编码错误，n >= remaining 表示截断 */
    if (n < 0)
        return buffer_size + 1;
    if ((size_t) n >= remaining)
        return buffer_size + 1;
    written += (size_t) n;
    remaining -= (size_t) n;

    /* 按类别分组 */
    const char *categories[] = {"几何构造", "度量计算", "几何变换", "代数运算", "逻辑推导"};
    PresetCategory cat_enums[] = {PRESET_CATEGORY_CONSTRUCTION, PRESET_CATEGORY_MEASUREMENT,
                                  PRESET_CATEGORY_TRANSFORMATION, PRESET_CATEGORY_ALGEBRAIC, PRESET_CATEGORY_LOGIC};

    for (int c = 0; c < 5; c++) {
        n = snprintf(out_buffer + written, remaining, "### %s\n\n", categories[c]);
        if (n < 0)
            return buffer_size + 1;
        if ((size_t) n >= remaining)
            return buffer_size + 1;
        written += (size_t) n;
        remaining -= (size_t) n;

        for (int i = 0; i < g_preset_library.count; i++) {
            if (!g_preset_library.entries[i].is_active)
                continue;
            if (g_preset_library.entries[i].metadata.category != cat_enums[c])
                continue;

            n = snprintf(out_buffer + written, remaining, "- **%s**: %s\n", g_preset_library.entries[i].metadata.name,
                         g_preset_library.entries[i].metadata.description);
            if (n < 0)
                return buffer_size + 1;
            if ((size_t) n >= remaining)
                return buffer_size + 1;
            written += (size_t) n;
            remaining -= (size_t) n;
        }

        n = snprintf(out_buffer + written, remaining, "\n");
        if (n < 0)
            return buffer_size + 1;
        if ((size_t) n >= remaining)
            return buffer_size + 1;
        written += (size_t) n;
        remaining -= (size_t) n;
    }

    return written + 1;
}
