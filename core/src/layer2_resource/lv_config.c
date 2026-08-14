/**
 * @file lv_config.c
 * @brief Lv-00 运行时配置系统实现
 *
 * @details 提供运行时配置的集中管理，包含以下功能：
 *          - 默认配置（lv_config_default）：所有子系统参数的默认值定义
 *          - 当前配置管理（lv_config_current / lv_config_apply）：全局状态读写
 *          - 类型安全的 setter 函数（lv_config_set_*）：直接修改全局配置
 *          - 通用 key-value setter（lv_config_set_int / lv_config_set_double）
 *          - JSON 配置文件加载（lv_config_load_json）和导出（lv_config_to_json）
 *
 * 配置覆盖范围：求解器、约束图、重写、流式、精度、MiniKernel、SAT、
 * 压力测试、解析器、类型系统、运行时防护、协议、交互几何、ODE、
 * 证明、递归/上下文、互操作、日志、监控、插件、后端、测试、内存、健康等。
 *
 * @author Lv-00 Project
 */

#include <ctype.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_file.h"

#include "lv/lv_internal.h"
#include "lv/lv_json.h"
#include "lv/lv_parse_utils.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_thread.h"
#include "lv/lv_utils.h"
#include "lv/geometry_config.h"


static lvConfig g_active_config;
static _Atomic int g_config_applied = 0;

static lvConfig def;
static lv_once_t g_default_config_once = lv_ONCE_INIT;

/* Lazy-init guard lock: shared by lv_config_current first-init and
 * lv_config_apply / lv_config_reset writes to avoid check-then-set
 * data races. 首次加锁时自动初始化（lv_lazy_lock，线程安全）。 */
lv_LAZY_LOCK_DEFINE(g_config_lock);

/** @brief 初始化默认配置（仅执行一次，由 lv_once 保证线程安全） */
static void lv_config_default_init(void) {
    memset(&def, 0, sizeof(def));

    /* 默认值由 LV_CONFIG_INT_KEYS / LV_CONFIG_DOUBLE_KEYS 四元组统一生成 */
#define DEFAULT_INT(key, type, field, dflt) def.field = dflt;
#define DEFAULT_DBL(key, type, field, dflt) def.field = dflt;
    LV_CONFIG_INT_KEYS(DEFAULT_INT)
    LV_CONFIG_DOUBLE_KEYS(DEFAULT_DBL)
#undef DEFAULT_INT
#undef DEFAULT_DBL
}

/**
 * @brief 获取默认配置
 *
 * 返回静态默认配置结构体。所有字段预置为安全的默认值。
 * 首次调用通过 lv_once 完成一次性初始化（线程安全）。
 *
 * @return 指向默认配置的常量指针
 */
const lvConfig *lv_config_default(void) {
    lv_once(&g_default_config_once, lv_config_default_init);
    return &def;
}

/**
 * @brief 获取当前生效的配置
 *
 * 首次调用时从默认配置初始化全局配置。
 *
 * @return 指向当前配置的常量指针
 */
const lvConfig *lv_config_current(void) {
    /* Fast path: atomic flag read avoids locking in the common case. */
    if (!g_config_applied) {
        /* Slow path: lock + double-check so concurrent first calls
         * cannot both run the lazy initialization. */
        lv_lazy_lock_lock(&g_config_lock, g_config_lock_init_once);
        if (!g_config_applied) {
            g_active_config = *lv_config_default();
            g_config_applied = 1;
        }
        lv_lazy_lock_unlock(&g_config_lock);
    }
    return &g_active_config;
}

/**
 * @brief 应用新的配置
 *
 * 用传入配置覆盖全局配置，立即生效。
 *
 * @param cfg 新配置指针，不能为 NULL
 * @return 0 成功，-1 失败（cfg 为 NULL）
 */
int lv_config_apply(const lvConfig *cfg) {
    if (!cfg)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "cfg is NULL");
    lv_lazy_lock_lock(&g_config_lock, g_config_lock_init_once);
    g_active_config = *cfg;
    g_config_applied = 1;
    lv_lazy_lock_unlock(&g_config_lock);
    /* 配置应用后显式同步几何配置（lv_geometry_sync_config 读取
     * 当前 lvConfig 的几何键并写入全局几何配置；lv_config_load_json
     * 经 lv_config_apply 一并覆盖）。 */
    lv_geometry_sync_config();
    return 0;
}

/* ---- 类型安全 getter / setter（由四元组 X-macro 生成定义） ---- */

/** @brief 获取可变的全局配置指针（内部辅助） */
static lvConfig *cfg_mut(void) {
    lv_config_current(); /* ensure initialized */
    return &g_active_config;
}

/** @brief 类型安全 getter：读取当前生效配置 */
#define GETTER(key, type, field, dflt) \
    type lv_config_get_##key(void) { return lv_config_current()->field; }
LV_CONFIG_INT_KEYS(GETTER)
LV_CONFIG_DOUBLE_KEYS(GETTER)
#undef GETTER

/** @brief 类型安全 setter：直接修改全局配置，立即生效 */
#define SETTER(key, type, field, dflt) \
    void lv_config_set_##key(type val) { cfg_mut()->field = val; }
LV_CONFIG_INT_KEYS(SETTER)
LV_CONFIG_DOUBLE_KEYS(SETTER)
#undef SETTER

/* ---- 通用 key-value setter ---- */

/**
 * @brief 通过字符串键设置整型配置项
 *
 * 在全局配置中查找与 key 匹配的整型字段并设置值。
 * 若未找到匹配项，返回 false 但不报错。
 *
 * @param key 配置项名称（字符串）
 * @param val 新值
 * @return true 设置成功，false 未找到匹配项或 key 为 NULL
 */
bool lv_config_set_int(const char *key, int val) {
    if (!key)
        return false;
    lvConfig *c = cfg_mut();

#define SET_IF(k, t, f, d)     \
    if (lv_str_eq(key, #k)) { \
        c->f = val;            \
        return true;           \
    }
    LV_CONFIG_INT_KEYS(SET_IF)
#undef SET_IF
    return false;
}

/**
 * @brief 通过字符串键设置浮点型配置项
 *
 * @param key 配置项名称
 * @param val 新值
 * @return true 设置成功，false 未找到匹配项或 key 为 NULL
 */
bool lv_config_set_double(const char *key, double val) {
    if (!key)
        return false;
    lvConfig *c = cfg_mut();

#define SET_IF(k, t, f, d)     \
    if (lv_str_eq(key, #k)) { \
        c->f = val;            \
        return true;           \
    }
    LV_CONFIG_DOUBLE_KEYS(SET_IF)
#undef SET_IF
    return false;
}

/* ---- 重置 ---- */

/**
 * @brief 重置配置为默认值
 */
void lv_config_reset(void) {
    lv_lazy_lock_lock(&g_config_lock, g_config_lock_init_once);
    g_active_config = *lv_config_default();
    g_config_applied = 1;
    lv_lazy_lock_unlock(&g_config_lock);
}

/* ---- JSON 配置加载辅助 —— 基于 lv_json.h ---- */

/**
 * @brief 从 JSON 字符串中解析指定键的整数值
 * @param json JSON 字符串
 * @param key  键名
 * @param out  输出值
 */
static void json_config_int(const char *json, const char *key, int *out) {
    const char *val = lv_json_find_key(json, key, strlen(key));
    if (!val) return;
    size_t remaining = strlen(val);
    lvJsonParser p;
    lv_json_parser_init(&p, val, remaining);
    lv_json_parse_int(&p, out);
}

/**
 * @brief 从 JSON 字符串中解析指定键的浮点数值
 * @param json JSON 字符串
 * @param key  键名
 * @param out  输出值
 */
static void json_config_double(const char *json, const char *key, double *out) {
    const char *val = lv_json_find_key(json, key, strlen(key));
    if (!val) return;
    size_t remaining = strlen(val);
    lvJsonParser p;
    lv_json_parser_init(&p, val, remaining);
    lv_json_parse_double(&p, out);
}

/* JLD_INT / JLD_DBL 已迁移至 X-macro LV_CONFIG_INT_KEYS / LV_CONFIG_DOUBLE_KEYS */

/**
 * @brief 从 JSON 文件加载配置
 *
 * 读取 JSON 格式的配置文件，解析其中的键值对并应用到全局配置。
 * 仅解析预定义的配置项，未知键将被忽略。
 * 文件大小限制为 1MB。
 *
 * @param json_path JSON 文件路径
 * @return 0 成功，-1 失败（文件无法打开、过大或解析错误）
 */
int lv_config_load_json(const char *json_path) {
    if (!json_path)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "json_path is NULL");
    size_t len = 0;
    char *buf = (char *) lv_file_read_all_limited(json_path, &len, (size_t) lv_MB_I);
    if (!buf)
        lv_RETURN_ERROR(lv_ERROR_IO, "failed to read config file (open/size/alloc/read error)");
    /* buf 已由 lv_file_read_all_limited 保证以 '\0' 结尾 */

    lvConfig cfg = *lv_config_default();
    const char *json_data = buf;

    /* 使用 X-macro 一次性展开所有整型 JSON 键 */
#define JLD(key, type, field, dflt) json_config_int(json_data, #key, &cfg.field);
    LV_CONFIG_INT_KEYS(JLD)
#undef JLD
#define JLD(key, type, field, dflt) json_config_double(json_data, #key, &cfg.field);
    LV_CONFIG_DOUBLE_KEYS(JLD)
#undef JLD

    lv_free((void **) &buf);
    return lv_config_apply(&cfg);
}

/**
 * @brief 将当前配置导出为 JSON 字符串
 *
 * 与 lv_config_load_json 对称：通过 X-macro 全量导出
 * LV_CONFIG_INT_KEYS / LV_CONFIG_DOUBLE_KEYS 覆盖的全部配置键。
 * double 使用 %.17g 输出，保证 save→load 无损往返。
 * 输出结构保持原有风格：外层对象、每行 "  \"key\": value," 缩进。
 *
 * @param buf      输出缓冲区
 * @param buf_size 缓冲区大小
 * @return 写入的字符数（不含结尾 null），失败返回 -1
 */
int lv_config_to_json(char *buf, size_t buf_size) {
    if (!buf)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "buf is NULL");
    if (buf_size < 64)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "buf_size < 64");
    const lvConfig *c = lv_config_current();

    lvJsonBuf jb;
    if (!lv_json_buf_init(&jb, 4096))
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "failed to init json buf");
    lv_json_buf_append_raw(&jb, "{\n");
#define TOJSON_INT(key, type, field, dflt) lv_json_buf_append_fmt(&jb, "  \"" #key "\": %d,\n", c->field);
#define TOJSON_DBL(key, type, field, dflt) lv_json_buf_append_fmt(&jb, "  \"" #key "\": %.17g,\n", c->field);
    LV_CONFIG_INT_KEYS(TOJSON_INT)
    LV_CONFIG_DOUBLE_KEYS(TOJSON_DBL)
#undef TOJSON_INT
#undef TOJSON_DBL
    /* 去掉最后一个键的尾部逗号，保证输出为合法 JSON（可被 lv_config_load_json 往返加载） */
    if (jb.pos >= 2 && jb.buffer[jb.pos - 2] == ',' && jb.buffer[jb.pos - 1] == '\n') {
        jb.buffer[jb.pos - 2] = '\n';
        jb.pos -= 1;
    }
    lv_json_buf_append_raw(&jb, "}\n");

    char *json = lv_json_buf_finalize(&jb);
    if (!json)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "failed to finalize json buf");
    size_t len = strlen(json);
    if (len >= buf_size) {
        lv_free((void **) &json);
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "buf_size too small for config JSON");
    }
    memcpy(buf, json, len + 1);
    lv_free((void **) &json);
    return (int) len; /* 不含结尾 '\0' */
}
