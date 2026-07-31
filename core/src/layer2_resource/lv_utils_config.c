/**
 * @file lv_utils_config.c
 * @brief 配置管理器
 *
 * @details 从 lv_utils.c 拆分的子模块（Lv-00 项目 v3.3.0+）。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv_utils.h"

#include "lv/lv_file.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "error_codes.h"
#include "lv.h"
#include "debug.h"
#include "lv_internal.h"

/* ============================================================
 * 配置管理
 * ============================================================ */

/* 消除魔术数字，用宏定义替代字面量 */
#define CONFIG_LINE_BUFFER_SIZE 1024 /**< 配置文件每行读取缓冲区大小 */

static ConfigItem *config_item_create(const char *key, ConfigType type) {
    ConfigItem *item = lv_calloc(1, sizeof(ConfigItem));
    if (!item)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "config_item_create calloc 失败");

    item->key = lv_strdup_safe(key);
    if (!item->key) {
        lv_free((void **) &item);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "config_item_create strdup 失败");
    }
    item->type = type;
    return item;
}

static void config_item_destroy(ConfigItem *item) {
    if (!item)
        return;

    lv_free((void **) &item->key);

    switch (item->type) {
        case CONFIG_TYPE_STRING:
            lv_free((void **) &item->value.string_val);
            break;
        case CONFIG_TYPE_ARRAY:
            for (size_t i = 0; i < item->array_count; i++) {
                config_item_destroy(item->value.array_val[i]);
            }
            lv_free((void **) &item->value.array_val);
            break;
        default:
            break;
    }

    lv_free((void **) &item);
}

ConfigManager *config_manager_create(const char *config_file) {
    ConfigManager *mgr = lv_calloc(1, sizeof(ConfigManager));
    if (!mgr)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "config_manager_create calloc 失败");

    if (config_file) {
        mgr->config_file = lv_strdup_safe(config_file);
    }
    mgr->auto_save = false;

    return mgr;
}

void config_manager_destroy(ConfigManager *mgr) {
    if (!mgr)
        return;

    ConfigItem *item = mgr->items;
    while (item) {
        ConfigItem *next = item->next;
        config_item_destroy(item);
        item = next;
    }

    lv_free((void **) &mgr->config_file);
    lv_free((void **) &mgr);
}

/**
 * @brief 在配置管理器中查找指定键对应的配置项
 *
 * 遍历配置管理器的链表，通过字符串比较查找与 key 匹配的配置项。
 *
 * @param mgr 配置管理器指针，允许为 NULL。
 * @param key 要查找的配置键名，允许为 NULL。
 * @return 找到的配置项指针；若 mgr 或 key 为 NULL，或未找到匹配项，返回 NULL。
 * @note 此为内部静态函数，仅供配置管理模块内部使用。
 */
static ConfigItem *config_find_item(const ConfigManager *mgr, const char *key) {
    if (!mgr || !key)
        return NULL;

    ConfigItem *item = mgr->items;
    while (item) {
        if (strcmp(item->key, key) == 0)
            return item;
        item = item->next;
    }
    return NULL;
}

/**
 * @brief 生成标量类型配置设置函数的宏
 *
 * 用于 int、bool、double 等标量类型的 config_set_* 函数，
 * 避免重复编写"查找已有项 → 更新或创建 → 自动保存"的通用逻辑。
 *
 * 参数说明：
 *   func_name  - 要生成的函数名（如 config_set_int）
 *   cfg_type   - 对应的 ConfigType 枚举值（如 CONFIG_TYPE_INT）
 *   val_type   - 值参数的 C 类型（如 int）
 *   val_member - ConfigItem.value 联合体中的成员名（如 int_val）
 *
 * 注意：config_set_string 不使用此宏，因为字符串类型需要额外的
 * 内存管理（释放旧值、strdup 新值），逻辑与标量类型有本质区别。
 */
#define DEFINE_CONFIG_SET_SCALAR(func_name, cfg_type, val_type, val_member) \
    bool func_name(ConfigManager *mgr, const char *key, val_type value) {   \
        if (!mgr || !key)                                                   \
            lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_PARAM, #func_name " 参数无效"); \
                                                                            \
        ConfigItem *item = config_find_item(mgr, key);                      \
        if (item) {                                                         \
            item->type = cfg_type;                                          \
            item->value.val_member = value;                                 \
        } else {                                                            \
            item = config_item_create(key, cfg_type);                       \
            if (!item)                                                      \
                lv_RETURN_ERROR_BOOL(lv_ERROR_ALLOCATION_FAILED, #func_name " 创建项失败"); \
            item->value.val_member = value;                                 \
            item->next = mgr->items;                                        \
            mgr->items = item;                                              \
        }                                                                   \
                                                                            \
        if (mgr->auto_save)                                                 \
            config_save(mgr);                                               \
        return true;                                                        \
    }

/* 使用宏生成 int、bool、double 三种标量类型的配置设置函数 */
DEFINE_CONFIG_SET_SCALAR(config_set_int, CONFIG_TYPE_INT, int, int_val)
DEFINE_CONFIG_SET_SCALAR(config_set_bool, CONFIG_TYPE_BOOL, bool, bool_val)
DEFINE_CONFIG_SET_SCALAR(config_set_double, CONFIG_TYPE_DOUBLE, double, double_val)

bool config_set_string(ConfigManager *mgr, const char *key, const char *value) {
    if (!mgr || !key)
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_PARAM, "config_set_string 参数无效");

    ConfigItem *item = config_find_item(mgr, key);
    if (item) {
        if (item->type == CONFIG_TYPE_STRING) {
            lv_free((void **) &item->value.string_val);
        }
        item->type = CONFIG_TYPE_STRING;
        item->value.string_val = lv_strdup_safe(value);
    } else {
        item = config_item_create(key, CONFIG_TYPE_STRING);
        if (!item)
            lv_RETURN_ERROR_BOOL(lv_ERROR_ALLOCATION_FAILED, "config_set_string 创建项失败");
        item->value.string_val = lv_strdup_safe(value);
        item->next = mgr->items;
        mgr->items = item;
    }

    if (mgr->auto_save)
        config_save(mgr);
    return true;
}

int config_get_int(const ConfigManager *mgr, const char *key, int default_val) {
    ConfigItem *item = config_find_item(mgr, key);
    if (item && item->type == CONFIG_TYPE_INT) {
        return item->value.int_val;
    }
    return default_val;
}

bool config_get_bool(const ConfigManager *mgr, const char *key, bool default_val) {
    ConfigItem *item = config_find_item(mgr, key);
    if (item && item->type == CONFIG_TYPE_BOOL) {
        return item->value.bool_val;
    }
    return default_val;
}

double config_get_double(const ConfigManager *mgr, const char *key, double default_val) {
    ConfigItem *item = config_find_item(mgr, key);
    if (item && item->type == CONFIG_TYPE_DOUBLE) {
        return item->value.double_val;
    }
    return default_val;
}

const char *config_get_string(const ConfigManager *mgr, const char *key, const char *default_val) {
    ConfigItem *item = config_find_item(mgr, key);
    if (item && item->type == CONFIG_TYPE_STRING) {
        return item->value.string_val;
    }
    return default_val;
}

/**
 * @brief 检查配置管理器中是否存在指定键
 *
 * @param mgr 配置管理器指针，允许为 NULL。
 * @param key 要检查的配置键名，允许为 NULL。
 * @return true  配置中存在该键；
 *         false mgr 或 key 为 NULL，或配置中不存在该键。
 */
bool config_has_key(const ConfigManager *mgr, const char *key) {
    return config_find_item(mgr, key) != NULL;
}

bool config_remove(ConfigManager *mgr, const char *key) {
    if (!mgr || !key)
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_PARAM, "config_remove 参数无效");

    ConfigItem **current = &mgr->items;
    while (*current) {
        if (strcmp((*current)->key, key) == 0) {
            ConfigItem *to_remove = *current;
            *current = to_remove->next;
            config_item_destroy(to_remove);
            if (mgr->auto_save)
                config_save(mgr);
            return true;
        }
        current = &(*current)->next;
    }
    lv_RETURN_ERROR_BOOL(lv_ERROR_NOT_FOUND, "config_remove 未找到 key");
}

/* 配置文件格式支持：
 *   - 注释行：以 '#' 开头
 *   - 节头：[section_name]   后续键自动加上 "section_name." 前缀
 *   - 键值对：key = value     在节内时存储为 "section.key"
 *   - 空行：忽略
 * 支持通过 dotted notation (如 "section.key") 查找配置项。
 */
bool config_load(ConfigManager *mgr) {
    if (!mgr || !mgr->config_file)
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_PARAM, "config_load 参数无效");

    FILE *f = lv_file_open(mgr->config_file, "r");
    if (!f)
        lv_RETURN_ERROR_BOOL(lv_ERROR_IO, "config_load 打开文件失败");

    char current_section[256];
    current_section[0] = '\0';

    char line[CONFIG_LINE_BUFFER_SIZE];
    while (fgets(line, sizeof(line), f)) {
        char *trimmed = lv_str_trim(line);
        if (*trimmed == '\0' || *trimmed == '#')
            continue;

        /* 解析节头 [section_name] */
        if (*trimmed == '[') {
            char *close_bracket = strchr(trimmed, ']');
            if (close_bracket) {
                *close_bracket = '\0';
                char *section_name = lv_str_trim(trimmed + 1);
                snprintf(current_section, sizeof(current_section), "%s", section_name);
            }
            continue;
        }

        char *eq = strchr(trimmed, '=');
        if (!eq)
            continue;

        *eq = '\0';
        char *raw_key = lv_str_trim(trimmed);
        char *value = lv_str_trim(eq + 1);

        /* 构建带节前缀的完整键名：section.key 或直接 key */
        char full_key[512];
        if (current_section[0] != '\0') {
            snprintf(full_key, sizeof(full_key), "%s.%s", current_section, raw_key);
        } else {
            snprintf(full_key, sizeof(full_key), "%s", raw_key);
        }

        /* 尝试解析为整数 */
        char *endptr;
        long int_val = strtol(value, &endptr, 10);
        if (*endptr == '\0') {
            config_set_int(mgr, full_key, (int) int_val);
            continue;
        }

        /* 尝试解析为布尔值 */
        if (strcmp(value, "true") == 0 || strcmp(value, "yes") == 0) {
            config_set_bool(mgr, full_key, true);
            continue;
        }
        if (strcmp(value, "false") == 0 || strcmp(value, "no") == 0) {
            config_set_bool(mgr, full_key, false);
            continue;
        }

        /* 尝试解析为浮点数 */
        double double_val = strtod(value, &endptr);
        if (*endptr == '\0') {
            config_set_double(mgr, full_key, double_val);
            continue;
        }

        /* 否则作为字符串 */
        config_set_string(mgr, full_key, value);
    }

    lv_file_close(f);
    return true;
}

bool config_save(const ConfigManager *mgr) {
    if (!mgr || !mgr->config_file)
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_PARAM, "config_save 参数无效");

    FILE *f = lv_file_open(mgr->config_file, "w");
    if (!f)
        lv_RETURN_ERROR_BOOL(lv_ERROR_IO, "config_save 打开文件失败");

    fprintf(f, "# Lv-00 Configuration File\n");
    fprintf(f, "# Auto-generated\n\n");

    char last_section[256];
    last_section[0] = '\0';

    ConfigItem *item = mgr->items;
    while (item) {
        /* 检测节前缀：如果键包含 '.'，提取节名并在变化时输出节头 */
        const char *dot = strchr(item->key, '.');
        if (dot) {
            char section[256];
            size_t section_len = (size_t) (dot - item->key);
            if (section_len >= sizeof(section))
                section_len = sizeof(section) - 1;
            memcpy(section, item->key, section_len);
            section[section_len] = '\0';

            if (strcmp(section, last_section) != 0) {
                fprintf(f, "\n[%s]\n", section);
                snprintf(last_section, sizeof(last_section), "%s", section);
            }
        } else {
            /* 无节前缀的键：如果之前在某个节内，先输出空行退出节 */
            if (last_section[0] != '\0') {
                fprintf(f, "\n");
                last_section[0] = '\0';
            }
        }

        switch (item->type) {
            case CONFIG_TYPE_INT:
                fprintf(f, "%s = %d\n", item->key, item->value.int_val);
                break;
            case CONFIG_TYPE_BOOL:
                fprintf(f, "%s = %s\n", item->key, item->value.bool_val ? "true" : "false");
                break;
            case CONFIG_TYPE_DOUBLE:
                fprintf(f, "%s = %.6f\n", item->key, item->value.double_val);
                break;
            case CONFIG_TYPE_STRING:
                fprintf(f, "%s = %s\n", item->key, item->value.string_val);
                break;
            case CONFIG_TYPE_ARRAY:
                /* 数组类型：逐元素序列化 */
                fprintf(f, "%s = [", item->key);
                if (item->value.array_val && item->array_count > 0) {
                    for (size_t ai = 0; ai < item->array_count; ai++) {
                        if (ai > 0)
                            fprintf(f, ", ");
                        ConfigItem *elem_item = item->value.array_val[ai];
                        if (elem_item && elem_item->key) {
                            fprintf(f, "\"%s\"", elem_item->key);
                        } else {
                            fprintf(f, "\"\"");
                        }
                    }
                }
                fprintf(f, "]\n");
                break;
            default:
                break;
        }
        item = item->next;
    }

    lv_file_close(f);
    return true;
}

