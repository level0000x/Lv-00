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

    /* 使用统一 JSON 写入器 lvJsonBuf 生成与原有 snprintf 完全一致的缩进格式 */
    lvJsonBuf buf;
    if (!lv_json_buf_init(&buf, 1024))
        return lv_ERROR_OUT_OF_MEMORY;

    /* preset->name 经 append_string 自动 JSON 转义 */
    lv_json_buf_append_raw(&buf, "{\n  \"name\": ");
    lv_json_buf_append_string(&buf, preset->name);
    lv_json_buf_append_fmt(&buf,
                           ",\n"
                           "  \"dimension_count\": %d,\n"
                           "  \"mapping_count\": %d,\n"
                           "  \"mappings\": [\n",
                           preset->dimension_count, preset->mapping_count);

    /* 序列化映射配置。
     * 数值格式统一为 %.15g（与 lv_json_buf_append_double / graph_serialize
     * numeric_value 的序列化风格一致）。原 %.6f 定宽在 scale/offset 为
     * 1e-8 量级时会被截断为 0.000000，导致"序列化→反序列化"往返丢失精度
     * （真实精度 bug，已修复）。%.15g 可无损往返 double。 */
    for (int i = 0; i < preset->mapping_count; i++) {
        const HighDimAxisMapping *m = &preset->mappings[i];
        lv_json_buf_append_fmt(&buf,
                               "    {\n"
                               "      \"axis_index\": %d,\n"
                               "      \"mapping_type\": \"%s\",\n"
                               "      \"scale\": %.15g,\n"
                               "      \"offset\": %.15g\n"
                               "    }%s\n",
                               m->axis_index, high_dim_mapping_type_to_string(m->mapping_type), m->scale,
                               m->offset, (i < preset->mapping_count - 1) ? "," : "");
    }

    lv_json_buf_append_fmt(&buf,
                           "  ],\n"
                           "  \"transform\": {\n"
                           "    \"m00\": %.15g,\n"
                           "    \"m01\": %.15g,\n"
                           "    \"m10\": %.15g,\n"
                           "    \"m11\": %.15g\n"
                           "  },\n"
                           "  \"is_default\": %s\n"
                           "}",
                           preset->transform.m[0][0], preset->transform.m[0][1], preset->transform.m[1][0],
                           preset->transform.m[1][1], preset->is_default ? "true" : "false");

    char *json = lv_json_buf_finalize(&buf);
    size_t len = strlen(json);
    if (len >= buffer_size) {
        lv_free((void **) &json);
        return lv_ERROR_BUFFER_TOO_SMALL;
    }
    memcpy(buffer, json, len + 1);
    lv_free((void **) &json);
    return (int) len;
}


/* ==================== JSON 反序列化 ==================== */

/* hd_json_skip_ws / hd_json_skip_value / hd_json_extract_string / hd_json_extract_int /
 * hd_json_extract_bool 已统一迁移至 lv/lv_json.h 的 lvJsonParser / lv_json_get_* API */

int high_dim_preset_deserialize_json(const char *json, HighDimProjectionPreset *preset) {
    /*
     * 基于统一 JSON 解析器 lvJsonParser 的实现。
     * 正确处理嵌套数组和数值，包括 mappings 数组和 transform 矩阵。
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
        const char *mappings_val = lv_json_find_key(json, "mappings", strlen("mappings"));
        if (mappings_val) {
            lvJsonParser p;
            lv_json_parser_init(&p, mappings_val, strlen(mappings_val));
            if (lv_json_peek(&p) == '[') {
                lv_json_next(&p); /* 跳过 '[' */
                int idx = 0;

                while (idx < HIGH_DIM_MAX_DIMENSIONS) {
                    lv_json_skip_ws(&p);
                    if (lv_json_peek(&p) == ']')
                        break;
                    if (lv_json_peek(&p) == ',') {
                        lv_json_next(&p);
                        continue;
                    }
                    if (lv_json_peek(&p) != '{') {
                        /* 跳过异常 token；若指针未推进（如到达字符串结尾 \0），
                         * 继续循环将陷入死循环，必须终止 */
                        size_t prev = p.pos;
                        lv_json_skip_value(&p);
                        if (p.pos == prev)
                            break;
                        continue;
                    }

                    /* 解析单个 mapping 对象 */
                    lv_json_next(&p); /* 跳过 '{' */

                    int axis_index = 0;
                    double scale = 1.0;
                    double offset = 0.0;
                    char type_str[32] = "";

                    /* 遍历对象字段，逐一提取 */
                    char *k = NULL;
                    while (lv_json_parse_field(&p, &k)) {
                        if (strcmp(k, "axis_index") == 0) {
                            lv_json_parse_int(&p, &axis_index);
                        } else if (strcmp(k, "mapping_type") == 0) {
                            char *s = lv_json_parse_string(&p);
                            if (s) {
                                lv_strlcpy(type_str, s, sizeof(type_str));
                                lv_free((void **) &s);
                            }
                        } else if (strcmp(k, "scale") == 0) {
                            lv_json_parse_double(&p, &scale);
                        } else if (strcmp(k, "offset") == 0) {
                            lv_json_parse_double(&p, &offset);
                        } else {
                            /* 跳过未知字段 */
                            lv_json_skip_value(&p);
                        }
                        lv_free((void **) &k);
                    }
                    if (lv_json_peek(&p) == '}')
                        lv_json_next(&p);

                    /* 填充映射结构 */
                    preset->mappings[idx].axis_index = axis_index;
                    preset->mappings[idx].mapping_type = high_dim_mapping_type_from_string(type_str);
                    preset->mappings[idx].scale = scale;
                    preset->mappings[idx].offset = offset;
                    idx++;
                }

                /* 更新 mapping_count（如果 JSON 中未指定或指定值偏小） */
                if (idx > preset->mapping_count) {
                    preset->mapping_count = idx;
                }
            }
        }
    }

    /* ---- 解析 transform 矩阵 ---- */
    {
        const char *transform_val = lv_json_find_key(json, "transform", strlen("transform"));
        if (transform_val) {
            lvJsonParser p;
            lv_json_parser_init(&p, transform_val, strlen(transform_val));
            if (lv_json_peek(&p) == '{') {
                lv_json_next(&p); /* 跳过 '{' */

                /* 遍历对象字段，查找 "m" 字段 */
                char *k = NULL;
                while (lv_json_parse_field(&p, &k)) {
                    bool is_m = (strcmp(k, "m") == 0);
                    lv_free((void **) &k);
                    if (!is_m) {
                        lv_json_skip_value(&p);
                        continue;
                    }

                    if (lv_json_peek(&p) == '[') {
                        lv_json_next(&p); /* 跳过 '[' */

                        /* 解析 2x2 矩阵 [[m00, m01], [m10, m11]] */
                        for (int row = 0; row < 2; row++) {
                            lv_json_skip_ws(&p);
                            if (lv_json_peek(&p) == ',') {
                                lv_json_next(&p);
                                lv_json_skip_ws(&p);
                            }
                            if (lv_json_peek(&p) == '[') {
                                lv_json_next(&p); /* 跳过行 '[' */

                                for (int col = 0; col < 2; col++) {
                                    lv_json_skip_ws(&p);
                                    if (lv_json_peek(&p) == ',') {
                                        lv_json_next(&p);
                                        lv_json_skip_ws(&p);
                                    }
                                    lv_json_parse_double(&p, &preset->transform.m[row][col]);
                                    /* 跳过数值 */
                                    lv_json_skip_value(&p);
                                }

                                lv_json_skip_ws(&p);
                                if (lv_json_peek(&p) == ']')
                                    lv_json_next(&p); /* 跳过行 ']' */
                            }
                        }
                    }
                    break; /* "m" 字段已处理完毕 */
                }
            }
        }
    }

    return lv_OK;
}

