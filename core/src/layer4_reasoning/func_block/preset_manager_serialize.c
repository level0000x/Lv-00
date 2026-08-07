/**
 * @file preset_manager_serialize.c
 * @brief 序列化与文件导入导出
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

#include "error_codes.h"
#include "func_block_preset.h"
#include "func_block_registry.h"
#include "lv_internal.h"
#include "lv/lv_file.h"
#include "lv/lv_json.h"
#include "lv/lv_str_utils.h"
#include "lv_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"
#include "preset_core.h"
#include "preset_manager_internal.h"

/* ============================================================
 * 序列化与反序列化（JSON 格式）
 * ============================================================ */

/**
 * @brief 对 JSON 字符串中的特殊字符进行转义
 *
 * 转义逻辑统一走公共 API lv_str_json_escape（两遍法：先算长度再写出）。
 *
 * @param str    原始字符串
 * @param out_len 输出转义后长度（可选）
 * @return 新分配的转义后字符串，调用者需使用 lv_free 释放
 */
static char *json_escape_string(const char *str, size_t *out_len) {
    if (!str) {
        if (out_len)
            *out_len = 0;
        return lv_strdup_safe("");
    }

    return lv_str_json_escape_alloc(str, strlen(str), out_len);
}

/**
 * @brief 序列化预设
 *
 * 将预设条目序列化为 JSON 格式的字节数组。
 *
 * JSON 格式：
 * {
 *   "name": "...",
 *   "description": "...",
 *   "mathematical_def": "...",
 *   "category": "...",
 *   "complexity": "...",
 *   "input_count": N,
 *   "output_count": N,
 *   "version": "M.m.p"
 * }
 *
 * @param entry     预设条目句柄
 * @param out_data  输出数据（调用者需使用 lv_free 释放）
 * @param out_size  输出数据大小
 * @return true 成功
 * @return false 失败
 */
bool preset_serialize(PresetEntryHandle entry, uint8_t **out_data, size_t *out_size) {
    PRESET_CHECK_NULL(entry, error);
    PRESET_CHECK_NULL(out_data, error);
    PRESET_CHECK_NULL(out_size, error);

    InternalPresetEntry *internal = (InternalPresetEntry *) entry;
    const PresetMetadata *meta = &internal->metadata;

    /* 对字符串字段进行 JSON 转义 */
    char *esc_name = json_escape_string(meta->name, NULL);
    char *esc_desc = json_escape_string(meta->description ? meta->description : "", NULL);
    char *esc_math = json_escape_string(meta->mathematical_def ? meta->mathematical_def : "", NULL);

    if (!esc_name || !esc_desc || !esc_math) {
        if (esc_name)
            lv_free((void **) &esc_name);
        if (esc_desc)
            lv_free((void **) &esc_desc);
        if (esc_math)
            lv_free((void **) &esc_math);
        set_error("内存分配失败");
        return false;
    }

    /* 构建 JSON 字符串 */
    char *json = lv_asprintf(
        "{\n"
        "  \"name\": \"%s\",\n"
        "  \"description\": \"%s\",\n"
        "  \"mathematical_def\": \"%s\",\n"
        "  \"category\": \"%s\",\n"
        "  \"complexity\": \"%s\",\n"
        "  \"input_count\": %d,\n"
        "  \"output_count\": %d,\n"
        "  \"version\": \"%d.%d.%d\"\n"
        "}\n",
        esc_name, esc_desc, esc_math, func_block_preset_category_string(meta->category),
        func_block_preset_complexity_string(meta->complexity), meta->input_count, meta->output_count,
        meta->version_major, meta->version_minor, meta->version_patch);

    lv_free((void **) &esc_name);
    lv_free((void **) &esc_desc);
    lv_free((void **) &esc_math);

    if (!json) {
        set_error("JSON 序列化失败：内存不足");
        return false;
    }

    *out_data = (uint8_t *) json;
    *out_size = strlen(json);
    return true;

error:
    return false;
}

/* json_extract_string / json_extract_int
 * 已迁移至 lv/lv_json.h 的统一 API：lv_json_get_string / lv_json_get_int */

/**
 * @brief 反序列化预设
 *
 * 从 JSON 格式的字节数组还原预设条目。
 *
 * 注意：反序列化生成的预设条目注册到库中，
 * 返回的句柄由调用者通过 preset_release 管理。
 *
 * @param data      数据字节数组
 * @param size      数据大小
 * @param out_entry 输出条目句柄
 * @return true 成功
 * @return false 失败
 */
bool preset_deserialize(const uint8_t *data, size_t size, PresetEntryHandle *out_entry) {
    PRESET_CHECK_NULL(data, error);
    PRESET_CHECK_NULL(out_entry, error);

    /* 确保数据以空字符结尾 */
    char *json_copy = (char *) lv_malloc(size + 1);
    if (!json_copy) {
        set_error("内存分配失败");
        return false;
    }
    memcpy(json_copy, data, size);
    json_copy[size] = '\0';

    /* 提取各字段 */
    PresetMetadata meta;
    memset(&meta, 0, sizeof(PresetMetadata));

#define PRESET_JSON_BUF_SIZE 1024
    char name_buf[PRESET_JSON_BUF_SIZE] = {0};
    char desc_buf[PRESET_JSON_BUF_SIZE] = {0};
    char math_buf[PRESET_JSON_BUF_SIZE] = {0};
    char cat_buf[PRESET_JSON_BUF_SIZE] = {0};

    bool ok = true;
    ok = ok && lv_json_get_string(json_copy, "name", name_buf, sizeof(name_buf));
    ok = ok && lv_json_get_string(json_copy, "description", desc_buf, sizeof(desc_buf));
    ok = ok && lv_json_get_string(json_copy, "mathematical_def", math_buf, sizeof(math_buf));
    ok = ok && lv_json_get_string(json_copy, "category", cat_buf, sizeof(cat_buf));
    ok = ok && lv_json_get_int(json_copy, "input_count", &meta.input_count);
    ok = ok && lv_json_get_int(json_copy, "output_count", &meta.output_count);

    if (!ok) {
        lv_free((void **) &json_copy);
        set_error("JSON 解析失败：缺少必要字段");
        return false;
    }

    char *name = lv_strdup(name_buf);
    char *desc = lv_strdup(desc_buf);
    char *math_def = lv_strdup(math_buf);
    char *cat_str = lv_strdup(cat_buf);

    meta.name = name;
    meta.description = desc;
    meta.mathematical_def = math_def;

    /* 类别解析：默认使用 CUSTOM */
    meta.category = PRESET_CATEGORY_CUSTOM;
    if (cat_str) {
        for (int c = 0; c < PRESET_CATEGORY_COUNT; c++) {
            const char *cat_name = func_block_preset_category_string((PresetCategory) c);
            if (cat_name && strcmp(cat_name, cat_str) == 0) {
                meta.category = (PresetCategory) c;
                break;
            }
        }
    }

    /* 复杂度解析：从字符串反查枚举，字段缺失时回退默认 COMPLEXITY_O1 */
    meta.complexity = COMPLEXITY_O1;
    char complexity_buf[128] = {0};
    if (lv_json_get_string(json_copy, "complexity", complexity_buf, sizeof(complexity_buf)) && complexity_buf[0]) {
        for (int c = 0; c <= COMPLEXITY_UNKNOWN; c++) {
            const char *cname = func_block_preset_complexity_string((PresetComplexity) c);
            if (cname && strcmp(cname, complexity_buf) == 0) {
                meta.complexity = (PresetComplexity) c;
                break;
            }
        }
    }

    /* 版本号：解析 "M.m.p" 格式，字段缺失时回退 1.0.0 */
    meta.version_major = 1;
    meta.version_minor = 0;
    meta.version_patch = 0;
    char version_buf[64] = {0};
    if (lv_json_get_string(json_copy, "version", version_buf, sizeof(version_buf)) && version_buf[0]) {
        if (sscanf(version_buf, "%d.%d.%d", &meta.version_major, &meta.version_minor, &meta.version_patch) != 3) {
            meta.version_major = 1;
            meta.version_minor = 0;
            meta.version_patch = 0;
        }
    }

    lv_free((void **) &cat_str);
    lv_free((void **) &json_copy);

    /* 注册到库中 */
    bool success = preset_register_custom(&meta, NULL, out_entry);

    /* 注意：preset_register_custom 会通过 lv_strdup 深拷贝 name/desc/math_def，
     * 因此可以安全释放临时分配的字符串 */
    lv_free((void **) &name);
    lv_free((void **) &desc);
    lv_free((void **) &math_def);

    if (!success) {
        return false;
    }

    return true;

error:
    return false;
}

/* ============================================================
 * 文件导入导出
 * ============================================================ */

/**
 * @brief 导出预设到文件
 *
 * 将指定预设序列化为 JSON 格式并写入文件。
 *
 * @param name     预设名称
 * @param filepath 文件路径
 * @return true 成功
 * @return false 失败
 */
bool preset_export_to_file(const char *name, const char *filepath) {
    PRESET_CHECK_STRING(name, error);
    PRESET_CHECK_STRING(filepath, error);

    /* 查找预设 */
    PresetEntryHandle entry = preset_find(name);
    if (!entry) {
        set_error("预设 '%s' 不存在", name);
        return false;
    }

    /* 序列化 */
    uint8_t *data = NULL;
    size_t size = 0;
    bool ok = preset_serialize(entry, &data, &size);

    preset_release(entry);

    if (!ok || !data) {
        return false;
    }

    /* 写入文件（统一 lv_file_write_all：打开/写入失败均返回非零） */
    if (lv_file_write_all(filepath, data, size) != 0) {
        lv_free((void **) &data);
        set_error("无法写入文件 '%s'", filepath);
        return false;
    }

    lv_free((void **) &data);

    ; /* 注册完成 */
    return true;

error:
    return false;
}

/**
 * @brief 从文件导入预设
 *
 * 读取 JSON 格式的文件并反序列化为预设条目。
 *
 * @param filepath 文件路径
 * @param out_name 输出预设名称（可选，调用者需使用 lv_free 释放）
 * @return true 成功
 * @return false 失败
 */
bool preset_import_from_file(const char *filepath, char **out_name) {
    PRESET_CHECK_STRING(filepath, error);

    /* 读取文件（统一 lv_file_read_all_limited；上限 PRESET_BUFFER_SIZE*10 与原实现一致，
     * 不存在/为空/超限/短读均返回 NULL） */
    size_t file_size = 0;
    uint8_t *data = lv_file_read_all_limited(filepath, &file_size, (size_t) PRESET_BUFFER_SIZE * 10);
    if (!data) {
        set_error("无法读取文件 '%s'（不存在、为空、超出大小上限或读取不完整）", filepath);
        return false;
    }

    /* 反序列化 */
    PresetEntryHandle entry = NULL;
    bool ok = preset_deserialize(data, file_size, &entry);

    lv_free((void **) &data);

    if (!ok || !entry) {
        return false;
    }

    /* 获取导入的预设名称 */
    const PresetMetadata *meta = preset_get_metadata(entry);
    if (meta && out_name) {
        *out_name = lv_strdup_safe(meta->name);
    }

    preset_release(entry);

    ; /* 注册完成 */
    return true;

error:
    return false;
}

