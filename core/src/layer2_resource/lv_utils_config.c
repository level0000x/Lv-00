/**
 * @file lv_utils_config.c
 * @brief 配置管理器
 *
 * @details 从 lv_utils.c 拆分的子模块（Lv-00 项目 v3.3.0+）。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/lv_utils.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_parse_utils.h"

#include "lv/lv_file.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lv/error_codes.h"
#include "lv/debug.h"
#include "lv/lv_internal.h"

/* ============================================================
 * 配置管理
 * ============================================================ */

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
        if (lv_str_eq(item->key, key))
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

/**
 * @brief 生成标量类型配置获取函数的宏
 *
 * 与 DEFINE_CONFIG_SET_SCALAR 对称，用于 int、bool、double 等标量类型的
 * config_get_* 函数，避免逐字重复"查找已有项 → 类型匹配则取值"的逻辑。
 *
 * 参数说明：
 *   func_name    - 要生成的函数名（如 config_get_int）
 *   cfg_type     - 对应的 ConfigType 枚举值（如 CONFIG_TYPE_INT）
 *   val_type     - 返回值及默认值参数的 C 类型（如 int）
 *   val_member   - ConfigItem.value 联合体中的成员名（如 int_val）
 *
 * 注意：config_get_string 不使用此宏，因为字符串返回值需要特殊处理
 * （返回内部指针而非拷贝），逻辑与标量类型有本质区别。
 */
#define DEFINE_CONFIG_GET_SCALAR(func_name, cfg_type, val_type, val_member) \
    val_type func_name(const ConfigManager *mgr, const char *key, val_type default_val) { \
        ConfigItem *item = config_find_item(mgr, key);                      \
        if (item && item->type == cfg_type) {                               \
            return item->value.val_member;                                  \
        }                                                                   \
        return default_val;                                                 \
    }

/* 使用宏生成 int、bool、double 三种标量类型的配置获取函数 */
DEFINE_CONFIG_GET_SCALAR(config_get_int, CONFIG_TYPE_INT, int, int_val)
DEFINE_CONFIG_GET_SCALAR(config_get_bool, CONFIG_TYPE_BOOL, bool, bool_val)
DEFINE_CONFIG_GET_SCALAR(config_get_double, CONFIG_TYPE_DOUBLE, double, double_val)

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
        if (lv_str_eq((*current)->key, key)) {
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
/**
 * @brief lv_ini_parse 回调：解析单个键值对并写入 ConfigManager
 * @param ctx     ConfigManager 指针
 * @param section 当前节名（全局节为 NULL）
 * @param key     键名（已去除首尾空白）
 * @param value   值（'=' 之后原始内容，此处按原实现去除首尾空白）
 * @return true 继续解析；false 中止解析（分配失败等错误路径）
 */
static bool config_ini_visit(void *ctx, const char *section, const char *key, const char *value) {
    ConfigManager *mgr = (ConfigManager *) ctx;

    /* 构建带节前缀的完整键名：section.key 或直接 key（节名按原实现去除空白） */
    char full_key[512];
    if (section && section[0] != '\0') {
        char sec_buf[256];
        lv_strlcpy(sec_buf, section, sizeof(sec_buf));
        char *sec_trim = lv_str_trim(sec_buf);
        if (sec_trim[0] != '\0') {
            lv_snprintf(full_key, sizeof(full_key), "%s.%s", sec_trim, key);
        } else {
            lv_strlcpy(full_key, key, sizeof(full_key));
        }
    } else {
        lv_strlcpy(full_key, key, sizeof(full_key));
    }

    /* 值去除首尾空白（与原实现一致） */
    char val_buf[2048];
    lv_strlcpy(val_buf, value, sizeof(val_buf));
    char *trimmed_val = lv_str_trim(val_buf);

    /* 解析字符串数组：key = ["a", "b", ...]（与 config_serialize_array 对称） */
    if (*trimmed_val == '[') {
        ConfigItem *item = config_item_create(full_key, CONFIG_TYPE_ARRAY);
        if (!item)
            lv_RETURN_ERROR_BOOL(lv_ERROR_ALLOCATION_FAILED, "config_load 创建数组项失败");

        size_t cap = 4;
        size_t cnt = 0;
        ConfigItem **arr = lv_calloc(cap, sizeof(ConfigItem *));
        if (!arr) {
            config_item_destroy(item);
            lv_RETURN_ERROR_BOOL(lv_ERROR_ALLOCATION_FAILED, "config_load 分配数组失败");
        }

        const char *p = trimmed_val + 1;
        while (*p) {
            p = lv_str_skip_ws(p);
            if (*p == '\0' || *p == ']')
                break;
            if (*p == ',') {
                p++;
                continue;
            }
            if (*p == '"') {
                /* 引号提取统一走公共原语 lv_str_read_quoted（替代手写
                 * "跳引号 → 扫到闭引号 → 复制" 三行循环） */
                char *elem_str = NULL;
                if (!lv_str_read_quoted(&p, &elem_str))
                    break; /* 不可能走到（*p=='"' 保证），防御性 break */

                if (cnt >= cap) {
                    cap *= 2;
                    ConfigItem **na = (ConfigItem **) lv_realloc(arr, cap * sizeof(ConfigItem *));
                    if (!na) {
                        for (size_t i = 0; i < cnt; i++)
                            config_item_destroy(arr[i]);
                        lv_free((void **) &arr);
                        config_item_destroy(item);
                        lv_RETURN_ERROR_BOOL(lv_ERROR_ALLOCATION_FAILED, "config_load 数组扩容失败");
                    }
                    arr = na;
                }
                ConfigItem *elem = lv_calloc(1, sizeof(ConfigItem));
                if (!elem) {
                    for (size_t i = 0; i < cnt; i++)
                        config_item_destroy(arr[i]);
                    lv_free((void **) &arr);
                    config_item_destroy(item);
                    lv_RETURN_ERROR_BOOL(lv_ERROR_ALLOCATION_FAILED, "config_load 数组元素分配失败");
                }
                /* 元素值存于 key（与 config_serialize_array 写出形态对称）；
                 * elem_str 为 lv_str_read_quoted 堆分配结果，所有权直接转移 */
                elem->type = CONFIG_TYPE_STRING;
                elem->key = elem_str ? elem_str : lv_strdup_safe("");
                arr[cnt++] = elem;
            } else {
                /* 非字符串元素：跳过到下一个逗号或结束 */
                p = lv_str_skip_until(p, ",]");
            }
        }

        item->value.array_val = arr;
        item->array_count = cnt;
        item->next = mgr->items;
        mgr->items = item;
        return true;
    }

    /* 尝试解析为整数 */
    int int_val = 0;
    if (lv_parse_int(trimmed_val, &int_val) == 0) {
        config_set_int(mgr, full_key, int_val);
        return true;
    }

    /* 尝试解析为布尔值 */
    if (lv_str_eq(trimmed_val, "true") || lv_str_eq(trimmed_val, "yes")) {
        config_set_bool(mgr, full_key, true);
        return true;
    }
    if (lv_str_eq(trimmed_val, "false") || lv_str_eq(trimmed_val, "no")) {
        config_set_bool(mgr, full_key, false);
        return true;
    }

    /* 尝试解析为浮点数 */
    double double_val = 0.0;
    if (lv_parse_double_strict(trimmed_val, &double_val) == 0) {
        config_set_double(mgr, full_key, double_val);
        return true;
    }

    /* 否则作为字符串 */
    config_set_string(mgr, full_key, trimmed_val);
    return true;
}

bool config_load(ConfigManager *mgr) {
    if (!mgr || !mgr->config_file)
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_PARAM, "config_load 参数无效");

    /* 统一走公共 lv_ini_parse（收敛手写 fopen/fgets 行解析样板） */
    if (lv_ini_parse(mgr->config_file, config_ini_visit, mgr) != 0) {
        /* 打开失败或回调中止：具体错误码已由 lv_ini_parse / config_ini_visit 记录 */
        return false;
    }
    return true;
}

/* ============================================================
 * 配置序列化（类型 -> 格式化函数 查找表，替代 switch 分派）
 * ============================================================ */

/** @brief 配置项序列化函数签名（写出 key = value 一行；key 由调用方给出，
 *  节内键由 config_save 去掉节前缀，保证 save→load 往返键名一致） */
typedef void (*ConfigSerializeFn)(FILE *f, const char *key, const ConfigItem *item);

static void config_serialize_int(FILE *f, const char *key, const ConfigItem *item) {
    fprintf(f, "%s = %d\n", key, item->value.int_val);
}

static void config_serialize_bool(FILE *f, const char *key, const ConfigItem *item) {
    fprintf(f, "%s = %s\n", key, item->value.bool_val ? "true" : "false");
}

static void config_serialize_double(FILE *f, const char *key, const ConfigItem *item) {
    /* %.17g 保证 double 无损往返（如 1e-8 不会被 %.6f 截断成 0.000000） */
    fprintf(f, "%s = %.17g\n", key, item->value.double_val);
}

static void config_serialize_string(FILE *f, const char *key, const ConfigItem *item) {
    fprintf(f, "%s = %s\n", key, item->value.string_val);
}

static void config_serialize_array(FILE *f, const char *key, const ConfigItem *item) {
    /* 数组类型：逐元素序列化 */
    fprintf(f, "%s = [", key);
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
}

/** @brief 配置类型 -> 序列化函数 查找表（指定初始化器，编译器校验 ConfigType 对齐） */
static const ConfigSerializeFn kConfigSerializers[] = {
    [CONFIG_TYPE_INT] = config_serialize_int,
    [CONFIG_TYPE_BOOL] = config_serialize_bool,
    [CONFIG_TYPE_DOUBLE] = config_serialize_double,
    [CONFIG_TYPE_STRING] = config_serialize_string,
    [CONFIG_TYPE_ARRAY] = config_serialize_array,
};

bool config_save(const ConfigManager *mgr) {
    if (!mgr || !mgr->config_file)
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_PARAM, "config_save 参数无效");

    /* K62 写原子性：先写临时文件，成功后 rename 替换目标——中途崩溃/写入失败
     * 不损坏既有配置（原直接 "w" 覆盖，写一半失败即损坏原文件）。 */
    char tmp_path[1024];
    if (lv_snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", mgr->config_file) < 0)
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_PARAM, "config_save 临时路径过长");

    FILE *f = lv_file_open(tmp_path, "w");
    if (!f)
        lv_RETURN_ERROR_BOOL(lv_ERROR_IO, "config_save 打开临时文件失败");

    fprintf(f, "# Lv-00 Configuration File\n");
    fprintf(f, "# Auto-generated\n\n");

    char last_section[256];
    last_section[0] = '\0';

    ConfigItem *item = mgr->items;
    while (item) {
        /* 检测节前缀：如果键包含 '.'，提取节名并在变化时输出节头 */
        const char *out_key = item->key; /* 默认输出完整键 */
        size_t section_len = 0;
        if (lv_str_prefix_len(item->key, '.', &section_len)) {
            char section[256];
            if (section_len >= sizeof(section))
                section_len = sizeof(section) - 1;
            lv_strlcpy_n(section, sizeof(section), item->key, section_len);

            if (lv_str_ne(section, last_section)) {
                fprintf(f, "\n[%s]\n", section);
                lv_strlcpy(last_section, section, sizeof(last_section));
            }

            /* 节内键输出去前缀名（"geom.max_points" → "max_points"），
             * 与 config_ini_visit 的节前缀重建对称，保证 save→load 往返一致 */
            out_key = item->key + section_len + 1;
        } else {
            /* 无节前缀的键：如果之前在某个节内，先输出空行退出节 */
            if (last_section[0] != '\0') {
                fprintf(f, "\n");
                last_section[0] = '\0';
            }
        }

        /* 按类型查表输出键值对；未知类型不输出（保持原 default 空分支语义） */
        if ((unsigned) item->type < lv_ARRAY_SIZE(kConfigSerializers) && kConfigSerializers[item->type]) {
            kConfigSerializers[item->type](f, out_key, item);
        }
        item = item->next;
    }

    if (lv_file_close(f) != 0) {
        remove(tmp_path); /* 关闭失败：清理临时文件，保留原配置 */
        lv_RETURN_ERROR_BOOL(lv_ERROR_IO, "config_save 关闭临时文件失败");
    }

    /* 原子替换：rename 成功即新配置生效（POSIX rename 原子；Windows 上
     * rename 目标已存在时行为见实现——先 remove 目标再 rename 保证覆盖） */
    remove(mgr->config_file); /* 目标存在时 Windows rename 失败，先移除（同 debug_state 先例） */
    if (rename(tmp_path, mgr->config_file) != 0) {
        remove(tmp_path);
        lv_RETURN_ERROR_BOOL(lv_ERROR_IO, "config_save 替换配置文件失败");
    }
    return true;
}