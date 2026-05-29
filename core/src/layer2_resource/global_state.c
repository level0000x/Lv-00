/**
 * @file global_state.c
 * @brief Lv-00 全局状态管理器实现
 * @details 统一管理所有全局参数和状态，提供版本迭代后的参数清理机制�? *          避免状态残留。所有分散的全局变量应迁移到此统一管理结构中�? *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 层级归属
 * 本模块属�?Layer 2 (Resource Management)�? */

#include "global_state.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00_internal.h"
#include "lv00_utils.h"

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

/**
 * @brief 查找参数索引
 */
static int global_state_find_param_index(const Lv00GlobalState *state, const char *name) {
    if (!state || !name) return -1;
    
    for (uint32_t i = 0; i < state->param_count; i++) {
        if (strcmp(state->params[i].name, name) == 0) {
            return (int)i;
        }
    }
    
    return -1;
}

/**
 * @brief 查找或创建参数索�? */
static int global_state_find_or_create_param_index(Lv00GlobalState *state, const char *name) {
    if (!state || !name) return -1;
    
    /* 先查�?*/
    int index = global_state_find_param_index(state, name);
    if (index >= 0) return index;
    
    /* 创建新参�?*/
    if (state->param_count >= LV00_GLOBAL_STATE_MAX_PARAMS) {
        return -1; /* 参数表已�?*/
    }
    
    index = (int)state->param_count;
    memset(&state->params[index], 0, sizeof(Lv00GlobalParam));
    strncpy(state->params[index].name, name, LV00_GLOBAL_STATE_MAX_PARAM_NAME_LEN - 1);
    state->params[index].name[LV00_GLOBAL_STATE_MAX_PARAM_NAME_LEN - 1] = '\0';
    state->params[index].type = GS_PARAM_TYPE_INVALID;
    state->param_count++;
    
    return index;
}

/**
 * @brief 获取当前时间戳（毫秒�? */
static uint64_t get_current_time_ms(void) {
    /* 使用简单的计数器，实际应用应使用平台特定的高精度计时器 */
    static uint64_t counter = 0;
    return ++counter;
}

/* ============================================================
 * 生命周期管理 API 实现
 * ============================================================ */

Lv00GlobalState *lv00_global_state_create(uint32_t system_version) {
    Lv00GlobalState *state = (Lv00GlobalState *)lv00_malloc(sizeof(Lv00GlobalState));
    if (!state) return NULL;
    
    memset(state, 0, sizeof(Lv00GlobalState));
    
    state->magic = LV00_GLOBAL_STATE_MAGIC;
    state->version = LV00_GLOBAL_STATE_VERSION;
    state->current_system_version = system_version;
    state->param_count = 0;
    state->access_count = 0;
    state->modification_count = 0;
    state->cleanup_callback = NULL;
    
    return state;
}

void lv00_global_state_destroy(Lv00GlobalState *state) {
    if (!state) return;
    
    /* 调用清理回调 */
    if (state->cleanup_callback) {
        state->cleanup_callback(state);
    }
    
    lv00_free((void **)&state);
}

bool lv00_global_state_is_valid(const Lv00GlobalState *state) {
    if (!state) return false;
    return state->magic == LV00_GLOBAL_STATE_MAGIC &&
           state->version == LV00_GLOBAL_STATE_VERSION;
}

Lv00ErrorCode lv00_global_state_cleanup_deprecated(Lv00GlobalState *state) {
    if (!lv00_global_state_is_valid(state)) {
        return LV00_ERROR_INVALID_PARAM;
    }
    
    uint32_t write_index = 0;
    
    for (uint32_t read_index = 0; read_index < state->param_count; read_index++) {
        Lv00GlobalParam *param = &state->params[read_index];
        
        /* 检查是否已废弃且版本已过期 */
        bool should_remove = false;
        if (param->is_deprecated && param->version_deprecated > 0) {
            /* 如果废弃版本小于当前系统版本，则移除 */
            if (param->version_deprecated < state->current_system_version) {
                should_remove = true;
            }
        }
        
        if (!should_remove) {
            /* 保留该参�?*/
            if (write_index != read_index) {
                state->params[write_index] = *param;
            }
            write_index++;
        }
    }
    
    state->param_count = write_index;
    
    return LV00_OK;
}

Lv00ErrorCode lv00_global_state_reset(Lv00GlobalState *state) {
    if (!lv00_global_state_is_valid(state)) {
        return LV00_ERROR_INVALID_PARAM;
    }
    
    /* 清理所有参�?*/
    for (uint32_t i = 0; i < state->param_count; i++) {
        /* 如果参数包含动态分配的内存，需要在这里释放 */
        /* 当前实现中所有数据都是内联存储，无需额外释放 */
    }
    
    state->param_count = 0;
    state->access_count = 0;
    state->modification_count = 0;
    
    return LV00_OK;
}

/* ============================================================
 * 参数操作 API 实现 - 布尔类型
 * ============================================================ */

Lv00ErrorCode lv00_global_state_set_bool(Lv00GlobalState *state, const char *name, 
                                         bool value, uint32_t version_introduced) {
    if (!lv00_global_state_is_valid(state) || !name) {
        return LV00_ERROR_INVALID_PARAM;
    }
    
    int index = global_state_find_or_create_param_index(state, name);
    if (index < 0) return LV00_ERROR_OUT_OF_MEMORY;
    
    Lv00GlobalParam *param = &state->params[index];
    param->type = GS_PARAM_TYPE_BOOL;
    param->value.b = value;
    param->version_introduced = version_introduced;
    param->is_dirty = true;
    
    state->modification_count++;
    
    return LV00_OK;
}

bool lv00_global_state_get_bool(const Lv00GlobalState *state, const char *name, 
                                bool default_value) {
    if (!lv00_global_state_is_valid(state) || !name) {
        return default_value;
    }
    
    int index = global_state_find_param_index(state, name);
    if (index < 0 || state->params[index].type != GS_PARAM_TYPE_BOOL) {
        return default_value;
    }
    
    ((Lv00GlobalState *)state)->access_count++;
    return state->params[index].value.b;
}

/* ============================================================
 * 参数操作 API 实现 - 整数类型
 * ============================================================ */

Lv00ErrorCode lv00_global_state_set_int(Lv00GlobalState *state, const char *name, 
                                        int value, uint32_t version_introduced) {
    if (!lv00_global_state_is_valid(state) || !name) {
        return LV00_ERROR_INVALID_PARAM;
    }
    
    int index = global_state_find_or_create_param_index(state, name);
    if (index < 0) return LV00_ERROR_OUT_OF_MEMORY;
    
    Lv00GlobalParam *param = &state->params[index];
    param->type = GS_PARAM_TYPE_INT;
    param->value.i = value;
    param->version_introduced = version_introduced;
    param->is_dirty = true;
    
    state->modification_count++;
    
    return LV00_OK;
}

int lv00_global_state_get_int(const Lv00GlobalState *state, const char *name, 
                              int default_value) {
    if (!lv00_global_state_is_valid(state) || !name) {
        return default_value;
    }
    
    int index = global_state_find_param_index(state, name);
    if (index < 0 || state->params[index].type != GS_PARAM_TYPE_INT) {
        return default_value;
    }
    
    ((Lv00GlobalState *)state)->access_count++;
    return state->params[index].value.i;
}

/* ============================================================
 * 参数操作 API 实现 - 无符号整数类�? * ============================================================ */

Lv00ErrorCode lv00_global_state_set_uint(Lv00GlobalState *state, const char *name, 
                                         unsigned int value, uint32_t version_introduced) {
    if (!lv00_global_state_is_valid(state) || !name) {
        return LV00_ERROR_INVALID_PARAM;
    }
    
    int index = global_state_find_or_create_param_index(state, name);
    if (index < 0) return LV00_ERROR_OUT_OF_MEMORY;
    
    Lv00GlobalParam *param = &state->params[index];
    param->type = GS_PARAM_TYPE_UINT;
    param->value.u = value;
    param->version_introduced = version_introduced;
    param->is_dirty = true;
    
    state->modification_count++;
    
    return LV00_OK;
}

unsigned int lv00_global_state_get_uint(const Lv00GlobalState *state, const char *name, 
                                        unsigned int default_value) {
    if (!lv00_global_state_is_valid(state) || !name) {
        return default_value;
    }
    
    int index = global_state_find_param_index(state, name);
    if (index < 0 || state->params[index].type != GS_PARAM_TYPE_UINT) {
        return default_value;
    }
    
    ((Lv00GlobalState *)state)->access_count++;
    return state->params[index].value.u;
}

/* ============================================================
 * 参数操作 API 实现 - 浮点类型
 * ============================================================ */

Lv00ErrorCode lv00_global_state_set_double(Lv00GlobalState *state, const char *name, 
                                           double value, uint32_t version_introduced) {
    if (!lv00_global_state_is_valid(state) || !name) {
        return LV00_ERROR_INVALID_PARAM;
    }
    
    int index = global_state_find_or_create_param_index(state, name);
    if (index < 0) return LV00_ERROR_OUT_OF_MEMORY;
    
    Lv00GlobalParam *param = &state->params[index];
    param->type = GS_PARAM_TYPE_DOUBLE;
    param->value.d = value;
    param->version_introduced = version_introduced;
    param->is_dirty = true;
    
    state->modification_count++;
    
    return LV00_OK;
}

double lv00_global_state_get_double(const Lv00GlobalState *state, const char *name, 
                                    double default_value) {
    if (!lv00_global_state_is_valid(state) || !name) {
        return default_value;
    }
    
    int index = global_state_find_param_index(state, name);
    if (index < 0 || state->params[index].type != GS_PARAM_TYPE_DOUBLE) {
        return default_value;
    }
    
    ((Lv00GlobalState *)state)->access_count++;
    return state->params[index].value.d;
}

/* ============================================================
 * 参数操作 API 实现 - 字符串类�? * ============================================================ */

Lv00ErrorCode lv00_global_state_set_string(Lv00GlobalState *state, const char *name, 
                                           const char *value, uint32_t version_introduced) {
    if (!lv00_global_state_is_valid(state) || !name || !value) {
        return LV00_ERROR_INVALID_PARAM;
    }
    
    int index = global_state_find_or_create_param_index(state, name);
    if (index < 0) return LV00_ERROR_OUT_OF_MEMORY;
    
    Lv00GlobalParam *param = &state->params[index];
    param->type = GS_PARAM_TYPE_STRING;
    strncpy(param->value.s, value, LV00_GLOBAL_STATE_MAX_PARAM_VALUE_LEN - 1);
    param->value.s[LV00_GLOBAL_STATE_MAX_PARAM_VALUE_LEN - 1] = '\0';
    param->version_introduced = version_introduced;
    param->is_dirty = true;
    
    state->modification_count++;
    
    return LV00_OK;
}

Lv00ErrorCode lv00_global_state_get_string(const Lv00GlobalState *state, const char *name, 
                                           const char *default_value,
                                           char *out_buffer, size_t buffer_size) {
    if (!lv00_global_state_is_valid(state) || !name || !out_buffer || buffer_size == 0) {
        return LV00_ERROR_INVALID_PARAM;
    }
    
    int index = global_state_find_param_index(state, name);
    if (index < 0 || state->params[index].type != GS_PARAM_TYPE_STRING) {
        if (default_value) {
            strncpy(out_buffer, default_value, buffer_size - 1);
            out_buffer[buffer_size - 1] = '\0';
        } else {
            out_buffer[0] = '\0';
        }
        return LV00_OK;
    }
    
    ((Lv00GlobalState *)state)->access_count++;
    strncpy(out_buffer, state->params[index].value.s, buffer_size - 1);
    out_buffer[buffer_size - 1] = '\0';
    
    return LV00_OK;
}

/* ============================================================
 * 参数操作 API 实现 - 指针类型
 * ============================================================ */

Lv00ErrorCode lv00_global_state_set_ptr(Lv00GlobalState *state, const char *name, 
                                        void *value, uint32_t version_introduced) {
    if (!lv00_global_state_is_valid(state) || !name) {
        return LV00_ERROR_INVALID_PARAM;
    }
    
    int index = global_state_find_or_create_param_index(state, name);
    if (index < 0) return LV00_ERROR_OUT_OF_MEMORY;
    
    Lv00GlobalParam *param = &state->params[index];
    param->type = GS_PARAM_TYPE_PTR;
    param->value.p = value;
    param->version_introduced = version_introduced;
    param->is_dirty = true;
    
    state->modification_count++;
    
    return LV00_OK;
}

void *lv00_global_state_get_ptr(const Lv00GlobalState *state, const char *name, 
                                void *default_value) {
    if (!lv00_global_state_is_valid(state) || !name) {
        return default_value;
    }
    
    int index = global_state_find_param_index(state, name);
    if (index < 0 || state->params[index].type != GS_PARAM_TYPE_PTR) {
        return default_value;
    }
    
    ((Lv00GlobalState *)state)->access_count++;
    return state->params[index].value.p;
}

/* ============================================================
 * 其他参数操作 API 实现
 * ============================================================ */

Lv00ErrorCode lv00_global_state_remove_param(Lv00GlobalState *state, const char *name) {
    if (!lv00_global_state_is_valid(state) || !name) {
        return LV00_ERROR_INVALID_PARAM;
    }
    
    int index = global_state_find_param_index(state, name);
    if (index < 0) return LV00_ERROR_NOT_FOUND;
    
    /* 移动后续参数填补空缺 */
    for (uint32_t i = index; i < state->param_count - 1; i++) {
        state->params[i] = state->params[i + 1];
    }
    
    state->param_count--;
    state->modification_count++;
    
    return LV00_OK;
}

bool lv00_global_state_has_param(const Lv00GlobalState *state, const char *name) {
    if (!lv00_global_state_is_valid(state) || !name) {
        return false;
    }
    
    return global_state_find_param_index(state, name) >= 0;
}

Lv00ErrorCode lv00_global_state_deprecate_param(Lv00GlobalState *state, const char *name,
                                                uint32_t version_deprecated) {
    if (!lv00_global_state_is_valid(state) || !name) {
        return LV00_ERROR_INVALID_PARAM;
    }
    
    int index = global_state_find_param_index(state, name);
    if (index < 0) return LV00_ERROR_NOT_FOUND;
    
    state->params[index].is_deprecated = true;
    state->params[index].version_deprecated = version_deprecated;
    state->modification_count++;
    
    return LV00_OK;
}

/* ============================================================
 * 批量操作 API 实现
 * ============================================================ */

Lv00ErrorCode lv00_global_state_load_from_string(Lv00GlobalState *state, const char *config) {
    if (!lv00_global_state_is_valid(state) || !config) {
        return LV00_ERROR_INVALID_PARAM;
    }
    
    /* 简单的配置解析：key=value，每行一�?*/
    const char *line = config;
    char key[LV00_GLOBAL_STATE_MAX_PARAM_NAME_LEN];
    char value[LV00_GLOBAL_STATE_MAX_PARAM_VALUE_LEN];
    
    while (*line) {
        /* 跳过空白行和注释 */
        while (*line == ' ' || *line == '\t' || *line == '\n' || *line == '\r') line++;
        if (*line == '#' || *line == ';') {
            while (*line && *line != '\n') line++;
            continue;
        }
        if (!*line) break;
        
        /* 解析 key=value */
        int key_len = 0;
        while (*line && *line != '=' && *line != '\n' && key_len < LV00_GLOBAL_STATE_MAX_PARAM_NAME_LEN - 1) {
            key[key_len++] = *line++;
        }
        key[key_len] = '\0';
        
        if (*line != '=') {
            /* 格式错误，跳�?*/
            while (*line && *line != '\n') line++;
            continue;
        }
        line++; /* 跳过 '=' */
        
        int value_len = 0;
        while (*line && *line != '\n' && value_len < LV00_GLOBAL_STATE_MAX_PARAM_VALUE_LEN - 1) {
            value[value_len++] = *line++;
        }
        value[value_len] = '\0';
        
        /* 去除尾部空白 */
        while (value_len > 0 && (value[value_len - 1] == ' ' || value[value_len - 1] == '\t' || value[value_len - 1] == '\r')) {
            value[--value_len] = '\0';
        }
        
        /* 尝试解析为整�?*/
        char *endptr;
        long int_val = strtol(value, &endptr, 10);
        if (*endptr == '\0') {
            lv00_global_state_set_int(state, key, (int)int_val, state->current_system_version);
        } else {
            /* 作为字符串存�?*/
            lv00_global_state_set_string(state, key, value, state->current_system_version);
        }
        
        /* 跳过换行 */
        if (*line == '\n') line++;
    }
    
    return LV00_OK;
}

Lv00ErrorCode lv00_global_state_export_to_string(const Lv00GlobalState *state, 
                                                 char *out_buffer, size_t buffer_size) {
    if (!lv00_global_state_is_valid(state) || !out_buffer || buffer_size == 0) {
        return LV00_ERROR_INVALID_PARAM;
    }
    
    size_t offset = 0;
    int written;
    
    written = snprintf(out_buffer + offset, buffer_size - offset, 
                       "# Lv-00 Global State Export\n");
    if (written < 0 || (size_t)written >= buffer_size - offset) {
        return LV00_ERROR_BUFFER_TOO_SMALL;
    }
    offset += written;
    
    written = snprintf(out_buffer + offset, buffer_size - offset,
                       "# Version: %u\n", state->current_system_version);
    if (written < 0 || (size_t)written >= buffer_size - offset) {
        return LV00_ERROR_BUFFER_TOO_SMALL;
    }
    offset += written;
    
    for (uint32_t i = 0; i < state->param_count; i++) {
        const Lv00GlobalParam *param = &state->params[i];
        
        if (param->is_deprecated) {
            written = snprintf(out_buffer + offset, buffer_size - offset,
                               "# [DEPRECATED v%u] ", param->version_deprecated);
            if (written < 0 || (size_t)written >= buffer_size - offset) {
                return LV00_ERROR_BUFFER_TOO_SMALL;
            }
            offset += written;
        }
        
        switch (param->type) {
            case GS_PARAM_TYPE_BOOL:
                written = snprintf(out_buffer + offset, buffer_size - offset,
                                   "%s=%s\n", param->name, param->value.b ? "true" : "false");
                break;
            case GS_PARAM_TYPE_INT:
                written = snprintf(out_buffer + offset, buffer_size - offset,
                                   "%s=%d\n", param->name, param->value.i);
                break;
            case GS_PARAM_TYPE_UINT:
                written = snprintf(out_buffer + offset, buffer_size - offset,
                                   "%s=%u\n", param->name, param->value.u);
                break;
            case GS_PARAM_TYPE_DOUBLE:
                written = snprintf(out_buffer + offset, buffer_size - offset,
                                   "%s=%.15g\n", param->name, param->value.d);
                break;
            case GS_PARAM_TYPE_STRING:
                written = snprintf(out_buffer + offset, buffer_size - offset,
                                   "%s=%s\n", param->name, param->value.s);
                break;
            case GS_PARAM_TYPE_PTR:
                written = snprintf(out_buffer + offset, buffer_size - offset,
                                   "%s=%p\n", param->name, param->value.p);
                break;
            default:
                written = snprintf(out_buffer + offset, buffer_size - offset,
                                   "# %s=<unknown type>\n", param->name);
                break;
        }
        
        if (written < 0 || (size_t)written >= buffer_size - offset) {
            return LV00_ERROR_BUFFER_TOO_SMALL;
        }
        offset += written;
    }
    
    return LV00_OK;
}

Lv00GlobalState *lv00_global_state_clone(const Lv00GlobalState *src) {
    if (!lv00_global_state_is_valid(src)) return NULL;
    
    Lv00GlobalState *dst = lv00_global_state_create(src->current_system_version);
    if (!dst) return NULL;
    
    /* 复制所有参�?*/
    dst->param_count = src->param_count;
    for (uint32_t i = 0; i < src->param_count; i++) {
        dst->params[i] = src->params[i];
        dst->params[i].is_dirty = false; /* 重置脏标�?*/
    }
    
    return dst;
}

/* ============================================================
 * 统计与诊�?API 实现
 * ============================================================ */

uint32_t lv00_global_state_get_param_count(const Lv00GlobalState *state) {
    if (!lv00_global_state_is_valid(state)) return 0;
    return state->param_count;
}

uint32_t lv00_global_state_get_deprecated_count(const Lv00GlobalState *state) {
    if (!lv00_global_state_is_valid(state)) return 0;
    
    uint32_t count = 0;
    for (uint32_t i = 0; i < state->param_count; i++) {
        if (state->params[i].is_deprecated) count++;
    }
    return count;
}

uint64_t lv00_global_state_get_access_count(const Lv00GlobalState *state) {
    if (!lv00_global_state_is_valid(state)) return 0;
    return state->access_count;
}

uint64_t lv00_global_state_get_modification_count(const Lv00GlobalState *state) {
    if (!lv00_global_state_is_valid(state)) return 0;
    return state->modification_count;
}
