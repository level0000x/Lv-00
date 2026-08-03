/*
 * @file high_dim_serialize.c
 * @brief High-dim module - serialization and json
 * @details Split from high_dim.c
 */

#include "high_dim.h"
#include "high_dim_internal.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "lv/config.h"
#include "lv/lv_json.h"
#include "lv/lv_parse_utils.h"

#include "debug.h"
#include "error_codes.h"
#include "lv_internal.h"
#include "lv/lv_str_utils.h"
#include "lv_utils.h"
#include "stream.h"
#include "stream_context_util.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_xmacro.h"

/* ==================== 序列化 ==================== */

int high_dim_preset_serialize_json(const HighDimProjectionPreset *preset, char *buffer, size_t buffer_size) {
    if (!preset || !buffer || buffer_size == 0) {
        return lv_ERROR_INVALID_PARAM;
    }

    int written = high_dim_snprintf(buffer, buffer_size,
                                    "{\n"
                                    "  \"name\": \"%s\",\n"
                                    "  \"dimension_count\": %d,\n"
                                    "  \"mapping_count\": %d,\n"
                                    "  \"mappings\": [\n",
                                    preset->name, preset->dimension_count, preset->mapping_count);

    if (written >= (int) buffer_size) {
        return lv_ERROR_BUFFER_TOO_SMALL;
    }

    size_t offset = written;

    /* 序列化映射配置 */
    for (int i = 0; i < preset->mapping_count && offset < buffer_size; i++) {
        const HighDimAxisMapping *m = &preset->mappings[i];
        written = high_dim_snprintf(buffer + offset, buffer_size - offset,
                                    "    {\n"
                                    "      \"axis_index\": %d,\n"
                                    "      \"mapping_type\": \"%s\",\n"
                                    "      \"scale\": %.6f,\n"
                                    "      \"offset\": %.6f\n"
                                    "    }%s\n",
                                    m->axis_index, high_dim_mapping_type_to_string(m->mapping_type), m->scale,
                                    m->offset, (i < preset->mapping_count - 1) ? "," : "");
        offset += written;
    }

    if (offset < buffer_size) {
        written = high_dim_snprintf(buffer + offset, buffer_size - offset,
                                    "  ],\n"
                                    "  \"transform\": {\n"
                                    "    \"m00\": %.6f,\n"
                                    "    \"m01\": %.6f,\n"
                                    "    \"m10\": %.6f,\n"
                                    "    \"m11\": %.6f\n"
                                    "  },\n"
                                    "  \"is_default\": %s\n"
                                    "}",
                                    preset->transform.m[0][0], preset->transform.m[0][1], preset->transform.m[1][0],
                                    preset->transform.m[1][1], preset->is_default ? "true" : "false");
        offset += written;
    }

    return (offset >= buffer_size) ? lv_ERROR_BUFFER_TOO_SMALL : (int) offset;
}


/* ==================== JSON 反序列化辅助函数 ==================== */

/** 跳过空白字符 */
static const char *hd_json_skip_ws(const char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    return p;
}

/** 跳过一个 JSON 值（字符串、数字、对象、数组、true/false/null） */
static const char *hd_json_skip_value(const char *p) {
    p = hd_json_skip_ws(p);
    if (*p == '"') {
        p++;
        while (*p && *p != '"') {
            if (*p == '\\')
                p++;
            p++;
        }
        if (*p == '"')
            p++;
    } else if (*p == '{') {
        int depth = 1;
        p++;
        while (*p && depth > 0) {
            if (*p == '{')
                depth++;
            else if (*p == '}')
                depth--;
            else if (*p == '"') {
                p++;
                while (*p && *p != '"') {
                    if (*p == '\\')
                        p++;
                    p++;
                }
            }
            p++;
        }
    } else if (*p == '[') {
        int depth = 1;
        p++;
        while (*p && depth > 0) {
            if (*p == '[')
                depth++;
            else if (*p == ']')
                depth--;
            else if (*p == '"') {
                p++;
                while (*p && *p != '"') {
                    if (*p == '\\')
                        p++;
                    p++;
                }
            }
            p++;
        }
    } else {
        while (*p && *p != ',' && *p != '}' && *p != ']' && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
            p++;
        }
    }
    return p;
}

/* hd_json_extract_string / hd_json_extract_int / hd_json_extract_bool
 * 已迁移至 lv/lv_json.h 的统一 API：lv_json_get_string / lv_json_get_int / lv_json_get_bool */

int high_dim_preset_deserialize_json(const char *json, HighDimProjectionPreset *preset) {
    /*
     * 手工 JSON 解析实现。
     * 正确处理嵌套数组和数值，包括 mappings 数组和 transform 矩阵。
     * 不依赖外部 JSON 库。
     */
    if (!json || !preset) {
        return lv_ERROR_INVALID_PARAM;
    }

    /* 初始化 */
    memset(preset, 0, sizeof(HighDimProjectionPreset));

    /* ---- 提取顶层标量字段 ---- */

    lv_json_get_string(json, "name", preset->name, HIGH_DIM_PROJECTION_NAME_MAX);
    lv_json_get_int(json, "dimension_count", &preset->dimension_count);
    lv_json_get_int(json, "mapping_count", &preset->mapping_count);
    lv_json_get_bool(json, "is_default", &preset->is_default);

    /* ---- 解析 mappings 数组 ---- */
    {
        const char *mappings_key = strstr(json, "\"mappings\"");
        if (mappings_key) {
            const char *p = mappings_key + strlen("\"mappings\"");
            p = hd_json_skip_ws(p);
            if (*p == ':') {
                p++;
                p = hd_json_skip_ws(p);
                if (*p == '[') {
                    p++; /* 跳过 '[' */
                    int idx = 0;

                    int loop_count = 0;
                    while (idx < HIGH_DIM_MAX_DIMENSIONS) {
                        loop_count++;
                        if (loop_count > 100) {
                            break;
                        }
                        p = hd_json_skip_ws(p);
                        if (*p == ']')
                            break;
                        if (*p == ',') {
                            p++;
                            continue;
                        }
                        if (*p != '{') {
                            /* 跳过异常 token；若指针未推进（如到达字符串结尾 \0），
                             * 继续循环将陷入死循环，必须终止 */
                            const char *prev = p;
                            p = hd_json_skip_value(p);
                            if (p == prev)
                                break;
                            continue;
                        }

                        /* 解析单个 mapping 对象 */
                        p++; /* 跳过 '{' */

                        int axis_index = 0;
                        double scale = 1.0;
                        double offset = 0.0;
                        char type_str[32] = "";

                        /* 提取 axis_index */
                        {
                            const char *ai_key = strstr(p, "\"axis_index\"");
                            if (ai_key && ai_key < strchr(p, '}')) {
                                const char *ai_val = ai_key + strlen("\"axis_index\"");
                                ai_val = hd_json_skip_ws(ai_val);
                                if (*ai_val == ':') {
                                    ai_val++;
                                    ai_val = hd_json_skip_ws(ai_val);
                                    axis_index = 0;
                                    lv_parse_int(ai_val, &axis_index);
                                }
                            }
                        }

                        /* 提取 mapping_type */
                        {
                            const char *mt_key = strstr(p, "\"mapping_type\"");
                            if (mt_key && mt_key < strchr(p, '}')) {
                                const char *mt_val = mt_key + strlen("\"mapping_type\"");
                                mt_val = hd_json_skip_ws(mt_val);
                                if (*mt_val == ':') {
                                    mt_val++;
                                    mt_val = hd_json_skip_ws(mt_val);
                                    if (*mt_val == '"') {
                                        mt_val++;
                                        size_t ti = 0;
                                        while (*mt_val && *mt_val != '"' && ti < sizeof(type_str) - 1) {
                                            type_str[ti++] = *mt_val;
                                            mt_val++;
                                        }
                                        type_str[ti] = '\0';
                                    }
                                }
                            }
                        }

                        /* 提取 scale */
                        {
                            const char *sc_key = strstr(p, "\"scale\"");
                            if (sc_key && sc_key < strchr(p, '}')) {
                                const char *sc_val = sc_key + strlen("\"scale\"");
                                sc_val = hd_json_skip_ws(sc_val);
                                if (*sc_val == ':') {
                                    sc_val++;
                                    sc_val = hd_json_skip_ws(sc_val);
                                    scale = strtod(sc_val, NULL);
                                }
                            }
                        }

                        /* 提取 offset */
                        {
                            const char *of_key = strstr(p, "\"offset\"");
                            if (of_key && of_key < strchr(p, '}')) {
                                const char *of_val = of_key + strlen("\"offset\"");
                                of_val = hd_json_skip_ws(of_val);
                                if (*of_val == ':') {
                                    of_val++;
                                    of_val = hd_json_skip_ws(of_val);
                                    offset = strtod(of_val, NULL);
                                }
                            }
                        }

                        /* 填充映射结构 */
                        preset->mappings[idx].axis_index = axis_index;
                        preset->mappings[idx].mapping_type = high_dim_mapping_type_from_string(type_str);
                        preset->mappings[idx].scale = scale;
                        preset->mappings[idx].offset = offset;
                        idx++;

                        /* 跳到对象结束 */
                        p = strchr(p, '}');
                        if (p)
                            p++;
                    }

                    /* 更新 mapping_count（如果 JSON 中未指定或指定值偏小） */
                    if (idx > preset->mapping_count) {
                        preset->mapping_count = idx;
                    }
                }
            }
        }
    }

    /* ---- 解析 transform 矩阵 ---- */
    {
        const char *transform_key = strstr(json, "\"transform\"");
        if (transform_key) {
            const char *p = transform_key + strlen("\"transform\"");
            p = hd_json_skip_ws(p);
            if (*p == ':') {
                p++;
                p = hd_json_skip_ws(p);
                if (*p == '{') {
                    p++; /* 跳过 '{' */

                    /* 查找 "m" 字段 */
                    const char *m_key = strstr(p, "\"m\"");
                    if (m_key) {
                        const char *m_val = m_key + strlen("\"m\"");
                        m_val = hd_json_skip_ws(m_val);
                        if (*m_val == ':') {
                            m_val++;
                            m_val = hd_json_skip_ws(m_val);
                            if (*m_val == '[') {
                                m_val++; /* 跳过 '[' */

                                /* 解析 2x2 矩阵 [[m00, m01], [m10, m11]] */
                                for (int row = 0; row < 2; row++) {
                                    m_val = hd_json_skip_ws(m_val);
                                    if (*m_val == ',') {
                                        m_val++;
                                        m_val = hd_json_skip_ws(m_val);
                                    }
                                    if (*m_val == '[') {
                                        m_val++; /* 跳过行 '[' */

                                        for (int col = 0; col < 2; col++) {
                                            m_val = hd_json_skip_ws(m_val);
                                            if (*m_val == ',') {
                                                m_val++;
                                                m_val = hd_json_skip_ws(m_val);
                                            }
                                            preset->transform.m[row][col] = strtod(m_val, NULL);
                                            /* 跳过数值 */
                                            m_val = hd_json_skip_value(m_val);
                                        }

                                        m_val = hd_json_skip_ws(m_val);
                                        if (*m_val == ']')
                                            m_val++; /* 跳过行 ']' */
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return lv_OK;
}

