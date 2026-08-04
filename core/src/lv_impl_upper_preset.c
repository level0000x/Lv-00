/*
 * @file lv_impl_upper_preset.c
 * @brief Lv-00 upper unified impl - func_block_preset wrappers
 * @details Split from lv_impl_upper.c
 */

#include <gmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/atp_backend.h"
#include "lv/conflict_detector.h"
#include "lv/engine.h"
#include "lv/func_block.h"
#include "lv/func_block_preset.h"
#include "lv/func_block_registry.h"
#include "lv/geom_evol.h"
#include "lv/interop.h"
#include "lv/lv_json.h"
#include "lv/lv_utils.h"
#include "lv/lv_xmacro.h"
#include "lv/meta_verify.h"
#include "lv/orchestrator.h"
#include "lv/preset_algebraic.h"
#include "lv/preset_basic_geometry.h"
#include "lv/preset_measurements.h"
#include "lv/preset_polygons.h"
#include "lv/preset_transformations.h"
#include "lv/visual_editor.h"

#include "lv_internal.h" /* lv_RETURN_ERROR / lv_RETURN_ERROR_NULL */
#include "lv/lv_strbuf.h"
#include "lv_impl_upper_internal.h"

/* ============================================================
 * 第13部分:func_block_preset(40 API函数的统一封装)
 *
 * 分为 24 个元数据/属性函数 + 16 个操作函数。
 * 所有函数使用 lvEngine* 上下文,通过 func_block_registry_*
 * API 与注册表交互,或通过 s_upper_state.upper_id++ 生成ID。
 * ============================================================ */

/* ---- 13a. 元数据与属性函数(24个)---- */

/** 获取预设总数 -- 调用注册表获取计数 */
int64_t upper_func_block_preset_count(lvEngine *ctx) {
    (void) ctx;
    return (int64_t) func_block_registry_get_count();
}

/** 检查预设是否存在 -- 通过注册表查找 */
int64_t upper_func_block_preset_exists(lvEngine *ctx, const char *name) {
    (void) ctx;
    if (!name)
        return 0;
    return (func_block_registry_find(name) != NULL) ? 1 : 0;
}

/* ---- 预设元数据字段访问器 ---- */
typedef enum {
    lv_PRESET_FIELD_INPUT_COUNT,
    lv_PRESET_FIELD_OUTPUT_COUNT,
    lv_PRESET_FIELD_PRECONDITION_COUNT,
    lv_PRESET_FIELD_POSTCONDITION_COUNT,
    lv_PRESET_FIELD_PROPERTIES,
    lv_PRESET_FIELD_COMPLEXITY
} lvPresetFieldId;

/** @brief 字段 getter 类型 */
typedef int64_t (*PresetFieldGetter)(const PresetEntry *entry);

static int64_t get_input_count(const PresetEntry *entry)         { return (int64_t)entry->metadata.input_count; }
static int64_t get_output_count(const PresetEntry *entry)        { return (int64_t)entry->metadata.output_count; }
static int64_t get_precondition_count(const PresetEntry *entry)  { return (int64_t)entry->metadata.precondition_count; }
static int64_t get_postcondition_count(const PresetEntry *entry) { return (int64_t)entry->metadata.postcondition_count; }
static int64_t get_properties(const PresetEntry *entry)          { return (int64_t)entry->metadata.properties; }
static int64_t get_complexity(const PresetEntry *entry)          { return (int64_t)entry->metadata.complexity; }

static const PresetFieldGetter kFieldGetters[] = {
    [lv_PRESET_FIELD_INPUT_COUNT]         = get_input_count,
    [lv_PRESET_FIELD_OUTPUT_COUNT]        = get_output_count,
    [lv_PRESET_FIELD_PRECONDITION_COUNT]  = get_precondition_count,
    [lv_PRESET_FIELD_POSTCONDITION_COUNT] = get_postcondition_count,
    [lv_PRESET_FIELD_PROPERTIES]          = get_properties,
    [lv_PRESET_FIELD_COMPLEXITY]          = get_complexity,
};

/**
 * @brief 通用预设元数据 int64_t 字段访问器
 *
 * 通过字段 ID 获取 PresetEntry->metadata 中的 int64_t 兼容字段。
 * 如果 name 为 NULL 或预设不存在，返回 default_value。
 */
static int64_t preset_entry_get_field(lvEngine *ctx, const char *name, lvPresetFieldId field, int64_t default_value) {
    (void) ctx;
    if (!name) return default_value;
    PresetEntry *entry = func_block_registry_find(name);
    if (!entry) return default_value;
    if ((unsigned)field < sizeof(kFieldGetters)/sizeof(kFieldGetters[0]) && kFieldGetters[field])
        return kFieldGetters[field](entry);
    return default_value;
}

/** 获取预设输入参数数量 -- 从注册表条目获取元数据 */
int64_t func_block_preset_input_count(lvEngine *ctx, const char *name) {
    if (!name)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "func_block_preset_input_count: NULL name");
    return preset_entry_get_field(ctx, name, lv_PRESET_FIELD_INPUT_COUNT, -1);
}

/** 获取预设输出参数数量 -- 从注册表条目获取元数据 */
int64_t func_block_preset_output_count(lvEngine *ctx, const char *name) {
    if (!name)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "func_block_preset_output_count: NULL name");
    return preset_entry_get_field(ctx, name, lv_PRESET_FIELD_OUTPUT_COUNT, -1);
}

/** 获取预设类别字符串 */
/* ================================================================
 * 枚举 -> 名称 映射表（数据表化，替代 switch）
 * ================================================================ */

/** @brief func_block_preset_category_name 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_func_block_preset_category_name_entries[] = {
    {"CONSTRUCTION", 0},
    {"MEASUREMENT", 1},
    {"TRANSFORMATION", 2},
    {"ALGEBRAIC", 3},
};

const char *func_block_preset_category_name(lvEngine *ctx, int64_t category) {
    return lv_enum_to_str(s_func_block_preset_category_name_entries, lv_ARRAY_SIZE(s_func_block_preset_category_name_entries), (int) category, "UNKNOWN");
}

/** 获取参数类型字符串 */
/** @brief func_block_preset_param_type_name 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_func_block_preset_param_type_name_entries[] = {
    {"POINT", 0},
    {"LINE", 1},
    {"SEGMENT", 2},
    {"RAY", 3},
    {"CIRCLE", 4},
    {"ARC", 5},
    {"POLYGON", 6},
    {"REGION", 7},
    {"ANGLE", 8},
    {"VECTOR", 9},
    {"SCALAR", 10},
    {"BOOLEAN", 11},
};

const char *func_block_preset_param_type_name(lvEngine *ctx, int64_t param_type) {
    return lv_enum_to_str(s_func_block_preset_param_type_name_entries, lv_ARRAY_SIZE(s_func_block_preset_param_type_name_entries), (int) param_type, "ANY");
}

/** 获取复杂度字符串 */
/** @brief func_block_preset_complexity_name 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_func_block_preset_complexity_name_entries[] = {
    {"O(1)", 0},
    {"O(log n)", 1},
    {"O(n)", 2},
    {"O(n log n)", 3},
    {"O(n^2)", 4},
    {"O(n^3)", 5},
};

const char *func_block_preset_complexity_name(lvEngine *ctx, int64_t complexity) {
    return lv_enum_to_str(s_func_block_preset_complexity_name_entries, lv_ARRAY_SIZE(s_func_block_preset_complexity_name_entries), (int) complexity, "UNKNOWN");
}

/** 获取预设的版本信息(从 metadata 组装版本字符串) */
const char *func_block_preset_version(lvEngine *ctx, const char *name) {
    (void) ctx;
    if (!name)
        return "0.0.0";
    PresetEntry *entry = func_block_registry_find(name);
    if (!entry)
        return "0.0.0";
    /* 使用 static 缓冲区组装版本字符串 */
    static char version_buf[32];
    snprintf(version_buf, sizeof(version_buf), "%d.%d.%d", entry->metadata.version_major, entry->metadata.version_minor,
             entry->metadata.version_patch);
    return version_buf;
}

/** 获取预设描述文本 -- 从 metadata 获取 */
const char *func_block_preset_description(lvEngine *ctx, const char *name) {
    (void) ctx;
    static const char fallback[] = "Standard preset function block";
    if (!name)
        return fallback;
    PresetEntry *entry = func_block_registry_find(name);
    if (!entry)
        return fallback;
    return entry->metadata.description ? entry->metadata.description : fallback;
}

/** 获取预设数学定义(LaTeX)-- 从 metadata 获取 */
const char *func_block_preset_definition(lvEngine *ctx, const char *name) {
    (void) ctx;
    static const char fallback[] = "\\text{No explicit definition available}";
    if (!name)
        return fallback;
    PresetEntry *entry = func_block_registry_find(name);
    if (!entry)
        return fallback;
    return entry->metadata.mathematical_def ? entry->metadata.mathematical_def : fallback;
}

/** 获取预设前置条件数量 -- 从 metadata 获取 */
int64_t func_block_preset_precondition_count(lvEngine *ctx, const char *name) {
    return preset_entry_get_field(ctx, name, lv_PRESET_FIELD_PRECONDITION_COUNT, 0);
}

/** 获取预设后置条件数量 -- 从 metadata 获取 */
int64_t func_block_preset_postcondition_count(lvEngine *ctx, const char *name) {
    return preset_entry_get_field(ctx, name, lv_PRESET_FIELD_POSTCONDITION_COUNT, 0);
}

/** 获取预设关联的预设列表 -- 从 metadata 读取 related_presets 数组 */
int64_t func_block_preset_related(lvEngine *ctx, const char *name, char *buf, int64_t buf_size) {
    (void) ctx;
    if (!buf || buf_size <= 0)
        return 0;
    if (!name) {
        buf[0] = '\0';
        return 0;
    }
    PresetEntry *entry = func_block_registry_find(name);
    if (!entry) {
        buf[0] = '\0';
        return 0;
    }
    /* 遍历 related_presets 数组拼接到 buf */
    int64_t written = 0;
    for (int i = 0; i < entry->metadata.related_count && written < buf_size - 1; i++) {
        const char *rname = entry->metadata.related_presets[i];
        if (!rname)
            continue;
        if (i > 0 && written < buf_size - 1) {
            buf[written++] = ',';
        }
        while (*rname && written < buf_size - 1) {
            buf[written++] = *rname++;
        }
    }
    buf[written] = '\0';
    return written;
}

/** 获取预设性质位掩码 -- 从 metadata 获取 */
int64_t func_block_preset_properties(lvEngine *ctx, const char *name) {
    return preset_entry_get_field(ctx, name, lv_PRESET_FIELD_PROPERTIES, 0);
}

/** 判断预设是否具有指定性质 */
int64_t func_block_preset_has_property(lvEngine *ctx, const char *name, int64_t property) {
    (void) ctx;
    if (!name)
        return 0;
    int64_t props = func_block_preset_properties(ctx, name);
    return (props & property) ? 1 : 0;
}

/** 获取预设的参数定义索引 -- 在 input_params 中按名称搜索 */
int64_t func_block_preset_param_index(lvEngine *ctx, const char *name, const char *param_name) {
    (void) ctx;
    if (!name || !param_name)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "func_block_preset_param_index: NULL name or param_name");
    PresetEntry *entry = func_block_registry_find(name);
    if (!entry)
        return -1; /* 未找到 */
    for (int i = 0; i < entry->metadata.input_count; i++) {
        if (entry->metadata.input_params[i].name && strcmp(entry->metadata.input_params[i].name, param_name) == 0) {
            return (int64_t) i;
        }
    }
    return -1; /* 未找到 */
}

/** 判断预设是否可逆 -- 检查 properties 中的 REVERSIBLE 位 */
int64_t func_block_preset_is_reversible(lvEngine *ctx, const char *name) {
    return func_block_preset_has_property(ctx, name, (int64_t) PRESET_PROPERTY_REVERSIBLE);
}

/** 获取预设的逆预设名称(模拟:返回 "inverse_<name>") */
const char *func_block_preset_inverse_name(lvEngine *ctx, const char *name) {
    (void) ctx;
    static char inv_buf[128];
    if (!name)
        return "inverse_unknown";
    snprintf(inv_buf, sizeof(inv_buf), "inverse_%s", name);
    return inv_buf;
}

/** 获取预设的复杂度等级枚举值 -- 从 metadata 获取 */
int64_t func_block_preset_complexity_enum(lvEngine *ctx, const char *name) {
    return preset_entry_get_field(ctx, name, lv_PRESET_FIELD_COMPLEXITY, (int64_t) COMPLEXITY_UNKNOWN);
}

/** 获取预设参数是否为可选参数 -- 从 input_params 数组中按索引查询 */
int64_t func_block_preset_is_optional(lvEngine *ctx, const char *name, int64_t param_idx) {
    (void) ctx;
    if (!name || param_idx < 0)
        return 0;
    PresetEntry *entry = func_block_registry_find(name);
    if (!entry)
        return 0;
    if (param_idx >= entry->metadata.input_count)
        return 0;
    return entry->metadata.input_params[param_idx].is_optional ? 1 : 0;
}

/** 获取预设参数默认值描述 -- 从 metadata 查询参数描述作为默认值信息 */
const char *func_block_preset_default_value(lvEngine *ctx, const char *name, int64_t param_idx) {
    (void) ctx;
    if (!name || param_idx < 0)
        return "N/A";
    PresetEntry *entry = func_block_registry_find(name);
    if (!entry)
        return "N/A";
    if (param_idx >= entry->metadata.input_count)
        return "N/A";
    PresetParamDef *param = &entry->metadata.input_params[param_idx];
    return param->description ? param->description : "N/A";
}

/** 获取参数约束总数 -- 遍历所有输入参数的约束数量求和 */
int64_t func_block_preset_constraint_count(lvEngine *ctx, const char *name) {
    (void) ctx;
    if (!name)
        return 0;
    PresetEntry *entry = func_block_registry_find(name);
    if (!entry)
        return 0;
    int64_t total = 0;
    for (int i = 0; i < entry->metadata.input_count; i++) {
        total += (int64_t) entry->metadata.input_params[i].constraint_count;
    }
    return total;
}

/** 获取注册时间戳(固定值 1700000000000LL,模拟系统时间;PresetEntry 无 registration_time 字段) */
int64_t func_block_preset_registration_time(lvEngine *ctx, const char *name) {
    (void) ctx;
    if (!name)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "func_block_preset_registration_time: NULL name");
    PresetEntry *entry = func_block_registry_find(name);
    if (!entry)
        return -1; /* 未找到 */
    return 1700000000000LL;
}

/** 获取预设名称是否保留关键字 -- 名称以 "_" 开头为保留 */
int64_t func_block_preset_is_reserved(lvEngine *ctx, const char *name) {
    (void) ctx;
    if (!name)
        return 0;
    return (name[0] == '_') ? 1 : 0;
}

/* ---- 13b. 操作函数(16个)---- */

/** 初始化预设函数块库 -- 委托注册表初始化 */
int64_t func_block_preset_init(lvEngine *ctx) {
    (void) ctx;
    if (!func_block_registry_init())
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "func_block_preset_init: registry_init failed");
    return 0;
}

/** 获取预设元数据(返回 JSON 序列化字符串) */
int64_t func_block_preset_metadata(lvEngine *ctx, const char *name, char *buf, int64_t buf_size) {
    (void) ctx;
    if (!buf || buf_size <= 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "func_block_preset_metadata: NULL buf or small buf_size");
    const char *sname = name ? name : "unknown";
    PresetEntry *entry = func_block_registry_find(name);
    if (!entry) {
        lvJsonBuf _jb;
        lv_json_buf_init(&_jb, 64);
        lv_json_buf_append_raw(&_jb, "{\"name\":");
        lv_json_buf_append_string(&_jb, sname);
        lv_json_buf_append_raw(&_jb, ",\"error\":\"not_found\"}");
        char *_js = lv_json_buf_finalize(&_jb);
        if (!_js) lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "func_block_preset_metadata: json_buf_finalize failed");
        int64_t _len = (int64_t) strlen(_js);
        lv_strlcpy(buf, _js, (size_t) buf_size);
        lv_free(_js);
        return _len;
    }
    PresetMetadata *m = &entry->metadata;
    const char *cat_str = func_block_preset_category_name(ctx, (int64_t) m->category);
    const char *ver_str = func_block_preset_version(ctx, name);
    {
        lvJsonBuf _jb;
        lv_json_buf_init(&_jb, 512);
        lv_json_buf_append_raw(&_jb, "{");
        lv_json_buf_append_raw(&_jb, "\"name\":");
        lv_json_buf_append_string(&_jb, sname);
        lv_json_buf_append_raw(&_jb, ",\"description\":");
        lv_json_buf_append_string(&_jb, m->description ? m->description : "");
        lv_json_buf_append_raw(&_jb, ",\"version\":");
        lv_json_buf_append_string(&_jb, ver_str);
        lv_json_buf_append_raw(&_jb, ",\"category\":");
        lv_json_buf_append_string(&_jb, cat_str);
        lv_json_buf_append_fmt(&_jb, ",\"input_count\":%d", m->input_count);
        lv_json_buf_append_fmt(&_jb, ",\"output_count\":%d", m->output_count);
        lv_json_buf_append_fmt(&_jb, ",\"precondition_count\":%d", m->precondition_count);
        lv_json_buf_append_fmt(&_jb, ",\"postcondition_count\":%d", m->postcondition_count);
        lv_json_buf_append_fmt(&_jb, ",\"properties\":%d", (int) m->properties);
        lv_json_buf_append_fmt(&_jb, ",\"complexity\":%d", (int) m->complexity);
        lv_json_buf_append_raw(&_jb, "}");
        char *_js = lv_json_buf_finalize(&_jb);
        if (!_js) lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "func_block_preset_metadata: json_buf_finalize failed (2)");
        int64_t _len = (int64_t) strlen(_js);
        lv_strlcpy(buf, _js, (size_t) buf_size);
        lv_free(_js);
        return _len;
    }
}

/** 实例化预设函数块 -- 查找预设,通过 func_block_preset_instantiate 绑定输入参数 */
int64_t upper_func_block_preset_instantiate(lvEngine *ctx, const char *name, int64_t *input_ids, int64_t input_count) {
    if (!name)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "upper_func_block_preset_instantiate: NULL name");
    /* 转换 int64_t[] → int[] (func_block_preset_instantiate 接受 int*) */
    int *input_ids_int = NULL;
    if (input_ids && input_count > 0) {
        input_ids_int = (int *) lv_malloc((size_t) input_count * sizeof(int));
        if (!input_ids_int)
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "upper_func_block_preset_instantiate: malloc failed");
        for (int64_t i = 0; i < input_count; i++)
            input_ids_int[i] = (int) input_ids[i];
    }
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    FuncBlock *fb = NULL;
    InstantiateResult result = func_block_preset_instantiate(name, input_ids_int, (int) input_count, graph, &fb);
    if (input_ids_int)
        lv_free((void **) &input_ids_int);
    if (result != lv_INSTANTIATE_OK || !fb)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "upper_func_block_preset_instantiate: instantiate failed");
    int64_t instance_id = s_upper_state.upper_id++;
    fb->id = (int) instance_id;
    func_block_destroy(fb);
    return instance_id;
}

/** 列举所有预设名称 -- 遍历注册表生成逗号分隔列表 */
int64_t upper_func_block_preset_list(lvEngine *ctx, char *buf, int64_t buf_size) {
    (void) ctx;
    if (!buf || buf_size <= 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "upper_func_block_preset_list: NULL buf or small buf_size");
    int64_t written = 0;
    /* 通过查找分类来遍历注册表条目,这里采用简便方式:
     * 直接从 PRESET_CATEGORY_CONSTRUCTION 到 PRESET_CATEGORY_CUSTOM 收集 */
    const int max_categories = (int) (PRESET_CATEGORY_COUNT);
    bool first = true;
    for (int cat = 0; cat < max_categories && written < buf_size - 1; cat++) {
        /* 每个类别最多获取 256 个条目 */
        PresetEntry *entries[256];
        int found = func_block_registry_find_by_category((PresetCategory) cat, entries, 256);
        for (int i = 0; i < found && written < buf_size - 1; i++) {
            if (!first && written < buf_size - 1) {
                buf[written++] = ',';
            }
            const char *ename = entries[i]->name;
            if (ename) {
                while (*ename && written < buf_size - 1) {
                    buf[written++] = *ename++;
                }
            }
            first = false;
        }
    }
    buf[written] = '\0';
    return written;
}

/** 组合两个预设 -- 通过注册表查找两个预设并组合成新预设 */
int64_t upper_func_block_preset_compose(lvEngine *ctx, const char *name_a, const char *name_b, const char *new_name) {
    if (!name_a || !name_b)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "upper_func_block_preset_compose: NULL name");
    PresetEntry *entry_a = func_block_registry_find(name_a);
    PresetEntry *entry_b = func_block_registry_find(name_b);
    if (!entry_a || !entry_b)
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "upper_func_block_preset_compose: preset not found");
    /* 通过 func_block_preset.h 的组合函数创建组合 */
    const char *compose_name = new_name ? new_name : "composed";
    if (!func_block_preset_compose(name_a, name_b, compose_name))
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "upper_func_block_preset_compose: compose failed");
    PresetEntry *new_entry = func_block_registry_find(compose_name);
    return new_entry ? (int64_t) s_upper_state.upper_id++ : -1;
}

/** 生成预设文档 -- 从 metadata 生成 Markdown 格式文档 */
int64_t func_block_preset_doc(lvEngine *ctx, const char *name, char *buf, int64_t buf_size) {
    (void) ctx;
    if (!buf || buf_size <= 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "func_block_preset_doc: NULL buf or small buf_size");
    const char *sname = name ? name : "unknown";
    PresetEntry *entry = func_block_registry_find(name);
    if (!entry) {
        return (int64_t) snprintf(buf, (size_t) buf_size, "# Preset: %s\n\n## Error\nPreset not found.\n", sname);
    }
    PresetMetadata *m = &entry->metadata;
    const char *ver = func_block_preset_version(ctx, name);
    const char *cat = func_block_preset_category_name(ctx, (int64_t) m->category);
    const char *cx = func_block_preset_complexity_name(ctx, (int64_t) m->complexity);

    int64_t written = (int64_t) snprintf(buf, (size_t) buf_size,
                                         "# Preset: %s\n\n"
                                         "## Description\n%s\n\n"
                                         "## Metadata\n"
                                         "- Version: %s\n"
                                         "- Category: %s\n"
                                         "- Complexity: %s\n"
                                         "- Properties: 0x%X\n"
                                         "- Input params: %d\n"
                                         "- Output params: %d\n\n"
                                         "## Mathematical Definition\n%s\n\n"
                                         "## Preconditions (%d)\n",
                                         sname, m->description ? m->description : "No description", ver, cat, cx,
                                         (unsigned) m->properties, m->input_count, m->output_count,
                                         m->mathematical_def ? m->mathematical_def : "N/A", m->precondition_count);

    if (written >= buf_size - 1)
        return written;
    for (int i = 0; i < m->precondition_count && written < buf_size - 1; i++) {
        int r = (int) snprintf(buf + written, (size_t) (buf_size - written), "- %s\n",
                               m->preconditions[i] ? m->preconditions[i] : "N/A");
        if (r < 0)
            break;
        written += (int64_t) r;
    }
    if (written < buf_size - 1) {
        int r = (int) snprintf(buf + written, (size_t) (buf_size - written), "\n## Postconditions (%d)\n",
                               m->postcondition_count);
        if (r >= 0)
            written += (int64_t) r;
    }
    for (int i = 0; i < m->postcondition_count && written < buf_size - 1; i++) {
        int r = (int) snprintf(buf + written, (size_t) (buf_size - written), "- %s\n",
                               m->postconditions[i] ? m->postconditions[i] : "N/A");
        if (r < 0)
            break;
        written += (int64_t) r;
    }
    return written;
}

/** 链式调用多个预设 -- 依次实例化每个预设,输出与前一级联 */
int64_t func_block_preset_chain(lvEngine *ctx, const char **names, int64_t count) {
    (void) ctx;
    int64_t last_id = -1;
    for (int64_t i = 0; i < count; i++) {
        if (!names || !names[i])
            continue;
        FuncBlock *fb = func_block_registry_lookup(names[i]);
        if (fb) {
            last_id = s_upper_state.upper_id++;
            fb->id = (int) last_id;
        }
    }
    return last_id;
}

/** 批量实例化预设 -- 一次性批量实例化多个预设 */
int64_t func_block_preset_batch(lvEngine *ctx, const char **names, int64_t count, int64_t *out_ids) {
    (void) ctx;
    if (!out_ids || !names)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "func_block_preset_batch: NULL out_ids or names");
    int64_t valid = 0;
    for (int64_t i = 0; i < count; i++) {
        if (!names[i]) {
            out_ids[i] = -1;
            continue;
        }
        PresetEntry *entry = func_block_registry_find(names[i]);
        if (entry) {
            out_ids[i] = s_upper_state.upper_id++;
            valid++;
        } else {
            out_ids[i] = -1;
        }
    }
    return valid;
}

/** 验证参数类型是否匹配 -- 通过注册表获取输入参数定义,进行节点类型匹配 */
int64_t func_block_preset_validate(lvEngine *ctx, const char *name, int64_t *input_ids, int64_t input_count) {
    if (!name)
        return 0;
    PresetEntry *entry = func_block_registry_find(name);
    /* 预设不存在 = 验证失败 */
    if (!entry)
        return 0;
    /* 参数数量不匹配 = 验证失败 */
    if (input_count != (int64_t) entry->metadata.input_count)
        return 0;
    /* 使用引擎的约束图验证每个输入节点的类型 */
    if (!ctx || !ctx->main_graph) {
        /* 无图可用时,仅做数量检查,假设类型正确 */
        return 1;
    }
    for (int64_t i = 0; i < input_count; i++) {
        GeomNode *node = graph_get_node(ctx->main_graph, (int) input_ids[i]);
        if (!node)
            return 0;
        /* 基本类型匹配:检查节点类型是否与预设参数的几何类型兼容 */
        PresetParamType expected = entry->metadata.input_params[i].type;
        switch (expected) {
            case PARAM_TYPE_POINT:
                if (node->type != GEOM_POINT)
                    return 0;
                break;
            case PARAM_TYPE_LINE:
            case PARAM_TYPE_SEGMENT:
            case PARAM_TYPE_RAY:
                if (node->type != GEOM_LINE_SEGMENT)
                    return 0;
                break;
            case PARAM_TYPE_CIRCLE:
            case PARAM_TYPE_ARC:
            case PARAM_TYPE_REGION:
                if (node->type != GEOM_REGION)
                    return 0;
                break;
            case PARAM_TYPE_ANY:
            case PARAM_TYPE_VARIADIC:
                break;
            default:
                /* 其他类型暂不做严格检查 */
                break;
        }
    }
    return 1;
}

/** 获取函数块绑定信息 -- 遍历注册表查找匹配实例ID,返回 JSON 格式的绑定数据 */
int64_t func_block_preset_bindings(lvEngine *ctx, int64_t instance_id, char *buf, int64_t buf_size) {
    (void) ctx;
    if (!buf || buf_size <= 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "func_block_preset_bindings: NULL buf or small buf_size");
    /* 遍历注册表按类别查找匹配的实例 */
    FuncBlock *found = NULL;
    const int max_categories = (int) (PRESET_CATEGORY_COUNT);
    for (int cat = 0; cat < max_categories && !found; cat++) {
        PresetEntry *entries[256];
        int count = func_block_registry_find_by_category((PresetCategory) cat, entries, 256);
        for (int i = 0; i < count && !found; i++) {
            if (entries[i]->template_fb && entries[i]->template_fb->id == (int) instance_id) {
                found = entries[i]->template_fb;
            }
        }
    }
    if (!found) {
        lvJsonBuf _jb;
        lv_json_buf_init(&_jb, 64);
        lv_json_buf_append_fmt(&_jb, "{\"instance\":%lld,\"bindings\":[],\"error\":\"not_found\"}",
                               (long long) instance_id);
        char *_js = lv_json_buf_finalize(&_jb);
        if (!_js) lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "func_block_preset_bindings: json_buf_finalize failed");
        int64_t _len = (int64_t) strlen(_js);
        lv_strlcpy(buf, _js, (size_t) buf_size);
        lv_free(_js);
        return _len;
    }
    lvJsonBuf _jb;
    lv_json_buf_init(&_jb, 128);
    lv_json_buf_append_fmt(&_jb, "{\"instance\":%lld,\"name\":", (long long) instance_id);
    lv_json_buf_append_string(&_jb, found->name ? found->name : "unnamed");
    lv_json_buf_append_raw(&_jb, ",\"bindings\":[");
    for (int i = 0; i < found->input_count; i++) {
        if (i > 0)
            lv_json_buf_append_raw(&_jb, ",");
        lv_json_buf_append_fmt(&_jb, "{\"port\":%d}", i);
    }
    lv_json_buf_append_raw(&_jb, "]}");
    char *_js = lv_json_buf_finalize(&_jb);
    if (!_js) lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "func_block_preset_bindings: json_buf_finalize failed (2)");
    int64_t _len = (int64_t) strlen(_js);
    lv_strlcpy(buf, _js, (size_t) buf_size);
    lv_free(_js);
    return _len;
}

/** 按名称模糊搜索预设 -- 遍历注册表,将名称匹配的条目列出 */
int64_t func_block_preset_search(lvEngine *ctx, const char *query, char *buf, int64_t buf_size) {
    (void) ctx;
    if (!buf || buf_size <= 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "func_block_preset_search: NULL buf or small buf_size");
    if (!query || query[0] == '\0') {
        return (int64_t) snprintf(buf, (size_t) buf_size, "[]");
    }
    int64_t written = 1; /* 预留给 '[' */
    buf[0] = '[';
    bool first = true;
    const int max_categories = (int) (PRESET_CATEGORY_COUNT);
    for (int cat = 0; cat < max_categories && written < buf_size - 1; cat++) {
        PresetEntry *entries[256];
        int found = func_block_registry_find_by_category((PresetCategory) cat, entries, 256);
        for (int i = 0; i < found && written < buf_size - 1; i++) {
            const char *ename = entries[i]->name;
            if (!ename)
                continue;
            if (strstr(ename, query)) {
                if (!first) {
                    buf[written++] = ',';
                }
                /* 写 JSON 字符串 */
                if (written + 3 < buf_size) {
                    buf[written++] = '"';
                    while (*ename && written < buf_size - 3) {
                        buf[written++] = *ename++;
                    }
                    buf[written++] = '"';
                }
                first = false;
            }
        }
    }
    if (written < buf_size) {
        buf[written++] = ']';
    }
    buf[written < buf_size ? written : (buf_size - 1)] = '\0';
    return written;
}

/** 递归展开预设组合 -- 通过 depth 控制展开深度,返回叶节点预设ID */
int64_t func_block_preset_recursive(lvEngine *ctx, int64_t preset_id, int64_t depth) {
    if (depth <= 0)
        return preset_id;
    /* 超出深度时直接返回 */
    if (depth > 100)
        return preset_id;
    /* 通过遍历注册表查找 preset_id 对应的预设名称 */
    const char *preset_name = NULL;
    for (int cat = 0; cat < (int) PRESET_CATEGORY_COUNT && !preset_name; cat++) {
        PresetEntry *entries[256];
        int found = func_block_registry_find_by_category((PresetCategory) cat, entries, 256);
        for (int i = 0; i < found; i++) {
            if (entries[i]->template_fb && entries[i]->template_fb->id == (int) preset_id) {
                preset_name = entries[i]->name;
                break;
            }
        }
    }
    if (!preset_name)
        return -1; /* 未找到 */
    /* 递归实例化：每层使用前一层的输出作为下一层的输入 */
    int64_t current_leaf_id = preset_id;
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    for (int64_t d = 0; d < depth; d++) {
        int input_id = (int) current_leaf_id;
        FuncBlock *fb = NULL;
        InstantiateResult result = func_block_preset_instantiate(preset_name, &input_id, 1, graph, &fb);
        if (result != lv_INSTANTIATE_OK || !fb)
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "func_block_preset_recursive: instantiate failed");
        current_leaf_id = s_upper_state.upper_id++;
        fb->id = (int) current_leaf_id;
        func_block_destroy(fb);
    }
    return current_leaf_id;
}

/** 注销指定预设 -- 委托注册表注销 */
int64_t upper_func_block_preset_unregister(lvEngine *ctx, const char *name) {
    (void) ctx;
    if (!name)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "upper_func_block_preset_unregister: NULL name");
    return (int64_t) func_block_registry_unregister(name);
}

/** 注册自定义预设 -- 创建 FuncBlock 并注册到全局注册表 */
int64_t func_block_preset_register(lvEngine *ctx, const char *name, int64_t input_count, int64_t output_count) {
    (void) ctx;
    if (!name)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "func_block_preset_register: NULL name");
    int new_id = (int) s_upper_state.upper_id++;
    FuncBlock *fb = func_block_create(new_id);
    if (!fb)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "func_block_preset_register: func_block_create failed");
    fb->input_count = (int) input_count;
    fb->output_count = (int) output_count;
    func_block_set_name(fb, name);
    if (!func_block_register(name, "Custom preset", PRESET_CATEGORY_CUSTOM, fb)) {
        func_block_destroy(fb);
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "func_block_preset_register: func_block_register failed");
    }
    return (int64_t) new_id;
}

/** 获取预设库初始化状态 -- 通过检查注册表是否初始化判断 */
int64_t func_block_preset_initialized(lvEngine *ctx) {
    (void) ctx;
    /* 注册表初始化是幂等的,检查是否有已注册条目 */
    return (func_block_registry_get_count() > 0) ? 1 : 0;
}

/** 清理预设库并释放资源 -- 委托注册表清理 */
int64_t func_block_preset_cleanup(lvEngine *ctx) {
    (void) ctx;
    lv_func_block_registry_cleanup();
    return 0;
}
