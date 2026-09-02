/**
 * @file dsl_extension.c
 * @brief 蓝图 DSL 扩展接口实现（TEN_LAYER_OPTIMIZED_PLAN §4.1.5 落地）
 *
 * 版本解析/比较（纯函数）+ 扩展注册表（名称 → 钩子副本）。
 * 归属 L2（依赖 lv_str_utils/lv_utils 字符串设施）。
 */

#include "lv/dsl_extension.h"

#include <ctype.h>
#include <string.h>

#include "lv/lv_str_utils.h"
#include "lv/lv_thread.h"
#include "lv/lv_utils.h"

/* ============================================================
 * 扩展注册表
 * ============================================================ */

#define LV_MAX_DSL_EXTENSIONS 32

typedef struct {
    char *name;                 /**< 扩展名副本 */
    char *version;              /**< 版本字符串副本（可为 NULL） */
    DslParseHook parse_hook;    /**< 解析钩子 */
    DslCodegenHook codegen_hook; /**< 代码生成钩子 */
    void *user_data;            /**< 用户数据 */
} DslExtensionEntry;

static DslExtensionEntry g_dsl_extensions[LV_MAX_DSL_EXTENSIONS];
static int g_dsl_extension_count = 0;

lv_LAZY_LOCK_DEFINE(g_dsl_lock);
#define DSL_LOCK() lv_lazy_lock_lock(&g_dsl_lock, g_dsl_lock_init_once)
#define DSL_UNLOCK() lv_lazy_lock_unlock(&g_dsl_lock)

/** @brief 按名查找扩展索引；未找到返回 -1 */
static int dsl_find(const char *name) {
    for (int i = 0; i < g_dsl_extension_count; i++) {
        if (lv_str_eq(g_dsl_extensions[i].name, name))
            return i;
    }
    return -1;
}

/** @brief 释放扩展项 */
static void dsl_entry_free(DslExtensionEntry *e) {
    lv_free((void **) &e->name);
    lv_free((void **) &e->version);
    memset(e, 0, sizeof(*e));
}

/* ============================================================
 * 公共接口
 * ============================================================ */

bool lv_dsl_register_extension(const DslExtensionRegistration *reg) {
    if (reg == NULL || reg->name == NULL) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_PARAM, "lv_dsl_register_extension: reg/name is NULL");
    }
    DSL_LOCK();
    if (dsl_find(reg->name) >= 0 || g_dsl_extension_count >= LV_MAX_DSL_EXTENSIONS) {
        DSL_UNLOCK();
        return false;
    }
    DslExtensionEntry *e = &g_dsl_extensions[g_dsl_extension_count];
    memset(e, 0, sizeof(*e));
    e->name = lv_strdup(reg->name);
    e->version = reg->version ? lv_strdup(reg->version) : NULL;
    if (e->name == NULL || (reg->version && e->version == NULL)) {
        dsl_entry_free(e);
        DSL_UNLOCK();
        lv_RETURN_ERROR_BOOL(lv_ERROR_OUT_OF_MEMORY, "lv_dsl_register_extension: strdup failed");
    }
    e->parse_hook = reg->parse_hook;
    e->codegen_hook = reg->codegen_hook;
    e->user_data = reg->user_data;
    g_dsl_extension_count++;
    DSL_UNLOCK();
    return true;
}

bool lv_dsl_unregister_extension(const char *name) {
    if (name == NULL)
        return false;
    DSL_LOCK();
    int idx = dsl_find(name);
    if (idx < 0) {
        DSL_UNLOCK();
        return false;
    }
    dsl_entry_free(&g_dsl_extensions[idx]);
    for (int j = idx; j < g_dsl_extension_count - 1; j++)
        g_dsl_extensions[j] = g_dsl_extensions[j + 1];
    g_dsl_extension_count--;
    DSL_UNLOCK();
    return true;
}

bool lv_dsl_version_parse(const char *version_str, DslVersion *out_version) {
    if (version_str == NULL || out_version == NULL)
        return false;
    const char *p = version_str;
    if (*p == 'v' || *p == 'V')
        p++;
    /* major */
    if (!isdigit((unsigned char) *p))
        return false;
    out_version->major = 0;
    while (isdigit((unsigned char) *p)) {
        out_version->major = out_version->major * 10 + (*p - '0');
        p++;
    }
    /* minor（可选） */
    out_version->minor = 0;
    out_version->patch = 0;
    if (*p == '.') {
        p++;
        if (!isdigit((unsigned char) *p))
            return false;
        while (isdigit((unsigned char) *p)) {
            out_version->minor = out_version->minor * 10 + (*p - '0');
            p++;
        }
        if (*p == '.') {
            p++;
            if (!isdigit((unsigned char) *p))
                return false;
            while (isdigit((unsigned char) *p)) {
                out_version->patch = out_version->patch * 10 + (*p - '0');
                p++;
            }
        }
    }
    return *p == '\0';
}

bool lv_dsl_version_compare(const DslVersion *a, const DslVersion *b, int *out_result) {
    if (a == NULL || b == NULL || out_result == NULL)
        return false;
    if (a->major != b->major) {
        *out_result = (a->major < b->major) ? -1 : 1;
    } else if (a->minor != b->minor) {
        *out_result = (a->minor < b->minor) ? -1 : 1;
    } else if (a->patch != b->patch) {
        *out_result = (a->patch < b->patch) ? -1 : 1;
    } else {
        *out_result = 0;
    }
    return true;
}

bool lv_dsl_syntax_transform(const char *source, const DslVersion *from_version, const DslVersion *to_version,
                             char **out_transformed) {
    if (source == NULL || out_transformed == NULL) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_PARAM, "lv_dsl_syntax_transform: NULL param");
    }
    *out_transformed = NULL;

    /* 同版本或无版本目标 → 原样复制（无迁移） */
    if (from_version == NULL || to_version == NULL) {
        *out_transformed = lv_strdup(source);
        return *out_transformed != NULL;
    }
    int cmp = 0;
    lv_dsl_version_compare(from_version, to_version, &cmp);
    if (cmp == 0) {
        *out_transformed = lv_strdup(source);
        return *out_transformed != NULL;
    }

    /* 遍历扩展：取首个 parse+codegen 钩子齐全者 */
    DSL_LOCK();
    for (int i = 0; i < g_dsl_extension_count; i++) {
        DslExtensionEntry *e = &g_dsl_extensions[i];
        if (e->parse_hook == NULL || e->codegen_hook == NULL)
            continue;
        /* 调钩子（在锁内调用——扩展为进程内注册，风险可接受） */
        void *ast = NULL;
        if (!e->parse_hook(source, strlen(source), &ast, e->user_data)) {
            continue;
        }
        char *output = NULL;
        size_t output_len = 0;
        bool ok = e->codegen_hook(ast, &output, &output_len, e->user_data);
        if (ok && output != NULL) {
            *out_transformed = output;
            DSL_UNLOCK();
            return true;
        }
        if (output != NULL)
            lv_free((void **) &output);
    }
    DSL_UNLOCK();
    lv_RETURN_ERROR_BOOL(lv_ERROR_NOT_FOUND, "lv_dsl_syntax_transform: no extension with parse+codegen hooks");
}
