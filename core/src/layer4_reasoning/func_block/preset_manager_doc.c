/**
 * @file preset_manager_doc.c
 * @brief 使用示例与文档生成
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
#include "lv/lv_json.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_strbuf.h"
#include "lv_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"
#include "preset_core.h"
#include "preset_manager_internal.h"

/* ============================================================
 * lv_hashtable 遍历辅助（foreach 回调）
 * ============================================================ */

/** foreach 回调：统计各类别数量（preset_generate_library_documentation 用） */
static void doc_cat_count_visitor(const char *key, void *value, void *ctx) {
    (void) key;
    InternalPresetEntry *entry = (InternalPresetEntry *) value;
    int *cat_counts = (int *) ctx;
    if (entry->is_active && entry->metadata.category >= 0 && entry->metadata.category < PRESET_CATEGORY_COUNT) {
        cat_counts[entry->metadata.category]++;
    }
}

/* ============================================================
 * 使用示例生成
 * ============================================================ */

/**
 * @brief 获取预设使用示例
 *
 * 根据预设元数据生成标准化的使用示例代码文本。
 *
 * @param name        预设名称
 * @param out_example 输出示例代码（调用者需使用 lv_free 释放）
 * @return true 成功
 * @return false 失败
 */
bool preset_get_usage_example(const char *name, char **out_example) {
    PRESET_CHECK_STRING(name, error);
    PRESET_CHECK_NULL(out_example, error);

    lock_library();

    if (!g_library.initialized) {
        unlock_library();
        set_error("预设库未初始化");
        return false;
    }

    InternalPresetEntry *entry = find_entry(name);
    if (!entry) {
        unlock_library();
        set_error("预设 '%s' 不存在", name);
        return false;
    }

    const PresetMetadata *meta = &entry->metadata;

    /* 使用 lv_asprintf 动态分配缓冲区构建示例文本 */
    char *example = NULL;

    /* 构建输入参数说明 */
    char inputs_desc[512] = {0};
    if (meta->input_count > 0 && meta->input_params) {
        int offset = 0;
        for (int i = 0; i < meta->input_count && offset < (int) sizeof(inputs_desc) - 1; i++) {
            int n = snprintf(inputs_desc + offset, sizeof(inputs_desc) - (size_t) offset,
                             "    node%d = graph_create_node(graph, \"point_%d\"); /* %s */\n", i + 1, i + 1,
                             func_block_preset_param_type_string(meta->input_params[i].type));
            if (n > 0)
                offset += n;
        }
    } else if (meta->input_count > 0) {
        snprintf(inputs_desc, sizeof(inputs_desc), "    /* 提供 %d 个输入节点 */\n", meta->input_count);
    }

    /* 构建输出说明 */
    char outputs_desc[256] = {0};
    if (meta->output_count > 0) {
        snprintf(outputs_desc, sizeof(outputs_desc), "    /* 预设产生 %d 个输出: %s */\n", meta->output_count,
                 meta->output_params && meta->output_count > 0
                     ? func_block_preset_param_type_string(meta->output_params[0].type)
                     : "未知类型");
    }

    /* 构建完整示例 */
    example = lv_asprintf(
        "/* ================================================================\n"
        " * 预设使用示例: %s\n"
        " * 类别: %s\n"
        " * 描述: %s\n"
        " * 复杂度: %s\n"
        " * ================================================================ */\n\n"
        "#include \"lv.h\"\n"
        "#include \"preset_core.h\"\n\n"
        "void example_%s(void)\n"
        "{\n"
        "    /* 1. 初始化系统 */\n"
        "    preset_library_init();\n"
        "    ConstraintGraph *graph = graph_create();\n\n"
        "    /* 2. 准备输入节点 */\n"
        "%s"
        "    int input_nodes[] = { /* 输入节点ID数组 */ };\n\n"
        "    /* 3. 实例化预设 */\n"
        "    PresetInstanceHandle instance = NULL;\n"
        "    PresetInstantiateOptions opts = { .validate_types = true,\n"
        "                                      .validate_constraints = true,\n"
        "                                      .auto_connect = true };\n"
        "    if (!preset_instantiate(\"%s\", input_nodes, %d, &opts, &instance)) {\n"
        "        fprintf(stderr, \"实例化失败: %%s\\n\", preset_get_last_error());\n"
        "        goto cleanup;\n"
        "    }\n\n"
        "    /* 4. 执行预设 */\n"
        "    if (!preset_instance_execute(instance, NULL)) {\n"
        "        fprintf(stderr, \"执行失败: %%s\\n\", preset_get_last_error());\n"
        "        goto cleanup;\n"
        "    }\n\n"
        "    /* 5. 获取结果 */\n"
        "%s"
        "    int *output_ids = NULL;\n"
        "    int output_count = 0;\n"
        "    preset_instance_get_outputs(instance, &output_ids, &output_count);\n"
        "    printf(\"输出节点数量: %%d\\n\", output_count);\n\n"
        "    /* 6. 清理 */\n"
        "    if (output_ids) lv_free((void **)&output_ids);\n"
        "cleanup:\n"
        "    preset_instance_destroy(instance);\n"
        "    graph_destroy(graph);\n"
        "    preset_library_shutdown();\n"
        "}\n",
        meta->name, func_block_preset_category_string(meta->category),
        meta->description ? meta->description : "（无描述）", func_block_preset_complexity_string(meta->complexity),
        meta->name, inputs_desc, meta->name, meta->input_count > 0 ? meta->input_count : 0, outputs_desc);

    unlock_library();

    if (!example) {
        set_error("示例生成失败：内存不足");
        return false;
    }

    *out_example = example;
    return true;

error:
    return false;
}

/* ============================================================
 * 文档生成
 * ============================================================ */

/**
 * @brief 生成预设文档
 *
 * 为指定预设生成指定格式的文档字符串。
 *
 * @param name          预设名称
 * @param format        格式（"text", "html", "markdown"）
 * @param out_document  输出文档（调用者需使用 lv_free 释放）
 * @return true 生成成功
 * @return false 生成失败
 */
bool preset_generate_documentation(const char *name, const char *format, char **out_document) {
    PRESET_CHECK_STRING(name, error);
    PRESET_CHECK_NULL(out_document, error);

    lock_library();

    if (!g_library.initialized) {
        unlock_library();
        set_error("预设库未初始化");
        return false;
    }

    InternalPresetEntry *entry = find_entry(name);
    if (!entry) {
        unlock_library();
        set_error("预设 '%s' 不存在", name);
        return false;
    }

    const PresetMetadata *meta = &entry->metadata;
    const char *fmt = format ? format : "markdown";

    char *doc = NULL;

    if (lv_str_eq(fmt, "markdown")) {
        doc = lv_asprintf(
            "# %s\n\n"
            "## 描述\n\n%s\n\n"
            "## 数学定义\n\n`%s`\n\n"
            "## 基本信息\n\n"
            "- **类别**: %s\n"
            "- **复杂度**: %s\n"
            "- **输入数量**: %d\n"
            "- **输出数量**: %d\n"
            "- **版本**: %d.%d.%d\n\n"
            "## 前置条件 (%d)\n\n"
            "%s\n\n"
            "## 后置条件 (%d)\n\n"
            "%s\n",
            meta->name, meta->description ? meta->description : "（无描述）",
            meta->mathematical_def ? meta->mathematical_def : "（无定义）",
            func_block_preset_category_string(meta->category), func_block_preset_complexity_string(meta->complexity),
            meta->input_count, meta->output_count, meta->version_major, meta->version_minor, meta->version_patch,
            meta->precondition_count, meta->precondition_count > 0 ? "（已定义）" : "（无）", meta->postcondition_count,
            meta->postcondition_count > 0 ? "（已定义）" : "（无）");
    } else if (lv_str_eq(fmt, "text")) {
        doc = lv_asprintf(
            "预设: %s\n"
            "描述: %s\n"
            "数学定义: %s\n"
            "类别: %s\n"
            "复杂度: %s\n"
            "输入数量: %d\n"
            "输出数量: %d\n"
            "版本: %d.%d.%d\n",
            meta->name, meta->description ? meta->description : "（无描述）",
            meta->mathematical_def ? meta->mathematical_def : "（无定义）",
            func_block_preset_category_string(meta->category), func_block_preset_complexity_string(meta->complexity),
            meta->input_count, meta->output_count, meta->version_major, meta->version_minor, meta->version_patch);
    } else if (lv_str_eq(fmt, "html")) {
        doc = lv_asprintf(
            "<!DOCTYPE html>\n<html>\n<head>\n"
            "<meta charset=\"UTF-8\">\n"
            "<title>%s - 预设文档</title>\n"
            "</head>\n<body>\n"
            "<h1>%s</h1>\n"
            "<p><strong>描述:</strong> %s</p>\n"
            "<p><strong>数学定义:</strong> %s</p>\n"
            "<p><strong>类别:</strong> %s</p>\n"
            "<p><strong>复杂度:</strong> %s</p>\n"
            "<p><strong>输入:</strong> %d | <strong>输出:</strong> %d</p>\n"
            "<p><strong>版本:</strong> %d.%d.%d</p>\n"
            "</body>\n</html>\n",
            meta->name, meta->name, meta->description ? meta->description : "（无描述）",
            meta->mathematical_def ? meta->mathematical_def : "（无定义）",
            func_block_preset_category_string(meta->category), func_block_preset_complexity_string(meta->complexity),
            meta->input_count, meta->output_count, meta->version_major, meta->version_minor, meta->version_patch);
    } else {
        /* 未知格式，默认使用 markdown */
        doc = lv_asprintf(
            "# %s\n\n"
            "## 描述\n\n%s\n\n"
            "## 类别\n\n%s\n",
            meta->name, meta->description ? meta->description : "（无描述）",
            func_block_preset_category_string(meta->category));
    }

    unlock_library();

    if (!doc) {
        set_error("文档生成失败：内存不足");
        return false;
    }

    *out_document = doc;
    return true;

error:
    return false;
}

/**
 * @brief 生成预设库完整文档
 *
 * 为整个预设库生成指定格式的索引文档。
 *
 * @param format        格式
 * @param out_document  输出文档（调用者需使用 lv_free 释放）
 * @return true 生成成功
 * @return false 生成失败
 */
bool preset_generate_library_documentation(const char *format, char **out_document) {
    PRESET_CHECK_NULL(out_document, error);

    lock_library();

    if (!g_library.initialized) {
        unlock_library();
        set_error("预设库未初始化");
        return false;
    }

    char *doc = NULL;

    /* 使用 lvStrBuf 动态构建文档 */
    lvStrBuf sb = {0};

    /* 标题 */
    lv_strbuf_printf(&sb,
                     "# Lv-00 预设函数块库\n\n"
                     "## 概述\n\n"
                     "本库包含 %d 个预设函数块，涵盖多个数学领域。\n\n"
                     "| 类别 | 数量 |\n|------|------|\n",
                     g_library.entry_count);

    /* 按类别统计（复用 lv_hashtable string 形态） */
    int cat_counts[PRESET_CATEGORY_COUNT] = {0};
    lv_hashtable_str_foreach(g_library.hash_table, doc_cat_count_visitor, cat_counts);

    for (int c = 0; c < PRESET_CATEGORY_COUNT; c++) {
        if (cat_counts[c] > 0) {
            lv_strbuf_printf(&sb, "| %s | %d |\n",
                             func_block_preset_category_string((PresetCategory) c), cat_counts[c]);
        }
    }

    doc = lv_strbuf_to_string(&sb);
    if (!doc) {
        unlock_library();
        set_error("文档生成失败：内存不足");
        return false;
    }

    unlock_library();

    *out_document = doc;
    return true;

error:
    return false;
}

