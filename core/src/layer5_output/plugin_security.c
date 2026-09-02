/**
 * @file plugin_security.c
 * @brief 蓝图插件安全层实现（TEN_LAYER_OPTIMIZED_PLAN §16.1/§16.2.1 落地）
 *
 * 详见 plugin_security.h 头注释的范围说明：签名=SHA-256 对称哈希校验；
 * 沙箱=配置记录模式（非 OS 隔离）；权限/信任/注入模式表进程级静态。
 */

#include "lv/plugin_security.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "lv/error_codes.h"
#include "lv/lv_file.h"
#include "lv/lv_hash.h"
#include "lv/lv_internal.h" /* lv_LOG_WARN 宏 */
#include "lv/lv_str_utils.h"
#include "lv/lv_thread.h"
#include "lv/lv_utils.h"

/* ============================================================
 * 进程级状态
 * ============================================================ */

lv_LAZY_LOCK_DEFINE(g_sec_lock);
#define SEC_LOCK() lv_lazy_lock_lock(&g_sec_lock, g_sec_lock_init_once)
#define SEC_UNLOCK() lv_lazy_lock_unlock(&g_sec_lock)

/* ① 描述符 */
static const lvPluginDescriptor *g_current_descriptor = NULL;

/* ② 签名信任表（key_id → 摘要 hex） */
#define LV_MAX_TRUSTED_KEYS 32
typedef struct {
    char *key_id;
    char *digest_hex;
} TrustedKey;
static TrustedKey g_trusted_keys[LV_MAX_TRUSTED_KEYS];
static int g_trusted_key_count = 0;
static bool g_enforce_signature = false;

/* ③ 沙箱配置记录 */
static bool g_sandbox_applied = false;
static lvSandboxConfig g_sandbox_config;

/* ④ 权限表（plugin_name → level） */
#define LV_MAX_PERM_ENTRIES 64
typedef struct {
    char *name;
    lvPermissionLevel level;
} PermEntry;
static PermEntry g_perm_entries[LV_MAX_PERM_ENTRIES];
static int g_perm_count = 0;

/* ⑤ 注入模式表（内置 + 扩展） */
#define LV_MAX_INJECTION_PATTERNS 64
static lvInjectionPattern g_injection_patterns[LV_MAX_INJECTION_PATTERNS];
static int g_injection_count = 0;

/** @brief 初始化内置注入模式（首次使用时惰性） */
static void ensure_injection_patterns(void) {
    if (g_injection_count > 0)
        return;
    static const lvInjectionPattern builtin[lv_DSL_BUILTIN_PATTERN_COUNT] = {
        {";rm", "可能的命令注入", 2},
        {"|sh", "可能的管道注入", 2},
        {"../", "路径遍历尝试", 2},
        {"%n", "格式化字符串攻击", 2},
        {"#include", "可疑的预处理指令", 2},
        {"#define", "可疑的预处理指令", 2},
    };
    for (int i = 0; i < lv_DSL_BUILTIN_PATTERN_COUNT; i++)
        g_injection_patterns[g_injection_count++] = builtin[i];
}

/* ============================================================
 * ① 插件描述符
 * ============================================================ */

const lvPluginDescriptor *lv_plugin_get_descriptor(void) {
    SEC_LOCK();
    const lvPluginDescriptor *d = g_current_descriptor;
    SEC_UNLOCK();
    return d;
}

void lv_plugin_security_register_descriptor(const lvPluginDescriptor *desc) {
    SEC_LOCK();
    g_current_descriptor = desc;
    SEC_UNLOCK();
}

/* ============================================================
 * ② 签名验证
 * ============================================================ */

const char *lv_signature_result_str(lvSignatureResult result) {
    switch (result) {
    case lv_SIG_OK:
        return "OK";
    case lv_SIG_NO_SIGNATURE:
        return "NO_SIGNATURE";
    case lv_SIG_INVALID_FORMAT:
        return "INVALID_FORMAT";
    case lv_SIG_HASH_MISMATCH:
        return "HASH_MISMATCH";
    case lv_SIG_KEY_UNTRUSTED:
        return "KEY_UNTRUSTED";
    case lv_SIG_EXPIRED:
        return "EXPIRED";
    default:
        return "INTERNAL_ERROR";
    }
}

/** @brief 计算文件 SHA-256 hex（返回 lv_strdup，调用方释放；失败 NULL） */
static char *file_sha256_hex(const char *path) {
    size_t len = 0;
    uint8_t *data = lv_file_read_all_limited(path, &len, 64u * 1024u * 1024u);
    if (data == NULL)
        return NULL;
    lvHashCtx ctx;
    lv_hash_init(&ctx, LV_HASH_SHA256);
    lv_hash_update(&ctx, data, len);
    lv_free((void **) &data);
    return lv_hash_to_hex_alloc(&ctx);
}

/** @brief 信任表查找 key_id；命中返回 digest_hex 副本指针（锁内调用） */
static const char *trusted_digest_of(const char *key_id) {
    for (int i = 0; i < g_trusted_key_count; i++) {
        if (lv_str_eq(g_trusted_keys[i].key_id, key_id))
            return g_trusted_keys[i].digest_hex;
    }
    return NULL;
}

lvSignatureResult lv_plugin_verify_signature(const char *plugin_path, const char *manifest_path) {
    if (plugin_path == NULL || manifest_path == NULL)
        return lv_SIG_INTERNAL_ERROR;

    /* 读签名文件（期望哈希，可带 key_id: 前缀） */
    char sig_buf[512];
    if (!lv_file_read_text(manifest_path, sig_buf, sizeof(sig_buf)))
        return lv_SIG_NO_SIGNATURE;

    /* 解析首行（去除首尾空白/换行） */
    char line[512];
    size_t n = 0;
    for (const char *p = sig_buf; *p && *p != '\n' && *p != '\r' && n + 1 < sizeof(line); p++)
        line[n++] = *p;
    line[n] = '\0';
    /* 去首尾空白 */
    while (n > 0 && (line[n - 1] == ' ' || line[n - 1] == '\t'))
        line[--n] = '\0';

    const char *expected_digest = line;
    const char *key_id = NULL;

    /* 支持 "key_id:hex" 形态：key_id 截断到 ':' */
    char *colon = strchr(line, ':');
    if (colon != NULL) {
        *colon = '\0';
        key_id = line;
        expected_digest = colon + 1;
    }

    char *file_digest = file_sha256_hex(plugin_path);
    if (file_digest == NULL)
        return lv_SIG_INTERNAL_ERROR;

    lvSignatureResult result;
    if (key_id != NULL) {
        SEC_LOCK();
        const char *trusted = trusted_digest_of(key_id);
        SEC_UNLOCK();
        if (trusted == NULL) {
            lv_free((void **) &file_digest);
            return lv_SIG_KEY_UNTRUSTED;
        }
        /* 信任表存的是期望摘要：比对 trusted 与文件哈希（签名文件仅存 key_id 时） */
        if (lv_str_icmp(trusted, file_digest) != 0) {
            lv_free((void **) &file_digest);
            return lv_SIG_HASH_MISMATCH;
        }
        result = lv_SIG_OK;
    } else {
        if (lv_str_icmp(expected_digest, file_digest) != 0) {
            lv_free((void **) &file_digest);
            return lv_SIG_HASH_MISMATCH;
        }
        result = lv_SIG_OK;
    }
    lv_free((void **) &file_digest);
    return result;
}

bool lv_plugin_add_trusted_key(const char *public_key_pem, const char *key_id) {
    if (public_key_pem == NULL || key_id == NULL) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_PARAM, "lv_plugin_add_trusted_key: NULL param");
    }
    SEC_LOCK();
    if (g_trusted_key_count >= LV_MAX_TRUSTED_KEYS) {
        SEC_UNLOCK();
        return false;
    }
    /* 覆盖同 key_id */
    int idx = -1;
    for (int i = 0; i < g_trusted_key_count; i++) {
        if (lv_str_eq(g_trusted_keys[i].key_id, key_id)) {
            idx = i;
            break;
        }
    }
    if (idx < 0)
        idx = g_trusted_key_count++;
    lv_free((void **) &g_trusted_keys[idx].key_id);
    lv_free((void **) &g_trusted_keys[idx].digest_hex);
    g_trusted_keys[idx].key_id = lv_strdup(key_id);
    /* public_key_pem 若为纯 hex 摘要（64 字符）则登记为摘要；否则存标记串 */
    g_trusted_keys[idx].digest_hex = lv_strdup(public_key_pem);
    SEC_UNLOCK();
    return g_trusted_keys[idx].key_id != NULL && g_trusted_keys[idx].digest_hex != NULL;
}

void lv_plugin_set_enforcement(bool enforce) {
    SEC_LOCK();
    g_enforce_signature = enforce;
    SEC_UNLOCK();
}

bool lv_plugin_enforcement_enabled(void) {
    SEC_LOCK();
    bool e = g_enforce_signature;
    SEC_UNLOCK();
    return e;
}

/* ============================================================
 * ③ 沙箱（配置记录模式）
 * ============================================================ */

bool lv_sandbox_check(const lvSandboxConfig *config, char *violation, size_t len) {
    if (config == NULL) {
        if (violation && len > 0)
            lv_strlcpy(violation, "config is NULL", len);
        return false;
    }
    const char *reason = NULL;
    if (config->cpu_time_limit_seconds == 0)
        reason = "cpu_time_limit_seconds 不能为 0";
    else if (config->max_rss_bytes == 0)
        reason = "max_rss_bytes 不能为 0";
    else if (config->max_open_fds < 0)
        reason = "max_open_fds 不能为负";
    else if (config->max_threads < 0)
        reason = "max_threads 不能为负";
    else if (config->allowed_path_count > 0 && config->allowed_paths == NULL)
        reason = "allowed_path_count>0 但 allowed_paths 为 NULL";
    if (reason != NULL) {
        if (violation && len > 0)
            lv_strlcpy(violation, reason, len);
        return false;
    }
    return true;
}

bool lv_sandbox_apply(const lvSandboxConfig *config) {
    char violation[128];
    if (!lv_sandbox_check(config, violation, sizeof(violation))) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_PARAM, "lv_sandbox_apply: %s", violation);
    }
    SEC_LOCK();
    g_sandbox_config = *config; /* 浅拷贝（路径数组 [borrow]） */
    g_sandbox_applied = true;
    SEC_UNLOCK();
    return true;
}

/* ============================================================
 * ④ 权限与审计
 * ============================================================ */

lvPermissionLevel lv_plugin_get_permission(const lvPlugin *plugin) {
    if (plugin == NULL)
        return lv_PERM_READONLY;
    const char *name = plugin->info.name;
    if (name == NULL || name[0] == '\0')
        return lv_PERM_READONLY;
    SEC_LOCK();
    lvPermissionLevel level = lv_PERM_READONLY;
    for (int i = 0; i < g_perm_count; i++) {
        if (lv_str_eq(g_perm_entries[i].name, name)) {
            level = g_perm_entries[i].level;
            break;
        }
    }
    SEC_UNLOCK();
    return level;
}

bool lv_plugin_set_permission(const char *plugin_name, lvPermissionLevel level) {
    if (plugin_name == NULL || level < lv_PERM_READONLY || level > lv_PERM_FULL) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_PARAM, "lv_plugin_set_permission: invalid");
    }
    SEC_LOCK();
    int idx = -1;
    for (int i = 0; i < g_perm_count; i++) {
        if (lv_str_eq(g_perm_entries[i].name, plugin_name)) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        if (g_perm_count >= LV_MAX_PERM_ENTRIES) {
            SEC_UNLOCK();
            return false;
        }
        idx = g_perm_count++;
        g_perm_entries[idx].name = lv_strdup(plugin_name);
    }
    g_perm_entries[idx].level = level;
    SEC_UNLOCK();
    return true;
}

const char *lv_perm_level_str(lvPermissionLevel level) {
    switch (level) {
    case lv_PERM_READONLY:
        return "readonly";
    case lv_PERM_CONSTRUCTION:
        return "construction";
    case lv_PERM_FULL:
        return "full";
    default:
        return "UNKNOWN";
    }
}

const char *lv_audit_event_type_str(lvAuditEventType type) {
    switch (type) {
    case lv_AUDIT_PLUGIN_LOAD:
        return "plugin_load";
    case lv_AUDIT_PLUGIN_UNLOAD:
        return "plugin_unload";
    case lv_AUDIT_PERMISSION_DENIED:
        return "permission_denied";
    case lv_AUDIT_RESOURCE_VIOLATION:
        return "resource_violation";
    case lv_AUDIT_SIGNATURE_FAILURE:
        return "signature_failure";
    case lv_AUDIT_API_CALL:
        return "api_call";
    case lv_AUDIT_SANDBOX_VIOLATION:
        return "sandbox_violation";
    default:
        return "UNKNOWN";
    }
}

void lv_audit_log(const lvPlugin *plugin, lvAuditEventType type, const char *fmt, ...) {
    char msg[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    const char *plugin_name =
        (plugin != NULL && plugin->info.name[0] != '\0') ? plugin->info.name : "<none>";
    char full[640];
    snprintf(full, sizeof(full), "plugin=\"%s\" event=%s msg=\"%s\"", plugin_name, lv_audit_event_type_str(type), msg);
    lv_LOG_WARN("%s", full); /* 审计经统一日志（无独立 tag；前缀已含 audit 语义） */
}

/* ============================================================
 * ⑤ DSL 注入检测
 * ============================================================ */

int lv_dsl_security_check(const char *input, size_t len, char *error, size_t err_len) {
    if (input == NULL) {
        if (error && err_len > 0)
            lv_strlcpy(error, "input is NULL", err_len);
        return (int) lv_ERROR_NULL_POINTER;
    }
    ensure_injection_patterns();
    for (int i = 0; i < g_injection_count; i++) {
        const lvInjectionPattern *pat = &g_injection_patterns[i];
        size_t pat_len = strlen(pat->pattern);
        if (pat_len == 0 || pat_len > len)
            continue;
        /* memmem 式扫描 */
        for (size_t off = 0; off + pat_len <= len; off++) {
            if (memcmp(input + off, pat->pattern, pat_len) == 0) {
                if (error && err_len > 0) {
                    snprintf(error, err_len, "%s (命中模式 \"%s\")", pat->description ? pat->description : "注入模式",
                             pat->pattern);
                }
                return lv_ERROR_INVALID_PARAM;
            }
        }
    }
    return lv_OK;
}

bool lv_dsl_add_injection_pattern(const char *pattern, const char *description, int severity) {
    if (pattern == NULL || severity < 1 || severity > 3) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_PARAM, "lv_dsl_add_injection_pattern: invalid");
    }
    ensure_injection_patterns();
    SEC_LOCK();
    if (g_injection_count >= LV_MAX_INJECTION_PATTERNS) {
        SEC_UNLOCK();
        return false;
    }
    g_injection_patterns[g_injection_count].pattern = lv_strdup(pattern);
    g_injection_patterns[g_injection_count].description =
        description ? lv_strdup(description) : lv_strdup(pattern);
    g_injection_patterns[g_injection_count].severity = severity;
    bool ok = g_injection_patterns[g_injection_count].pattern != NULL &&
              g_injection_patterns[g_injection_count].description != NULL;
    if (ok)
        g_injection_count++;
    SEC_UNLOCK();
    return ok;
}

/**
 * @brief 清理插件安全层进程级状态（信任表/权限表/注入扩展；内置模式表静态不释放）
 *
 * 供 lv.c 模块清理路径在进程退出时调用；测试也可调用以消除泄漏告警。
 * 清理后可再次使用（表重置为空）。
 */
void lv_plugin_security_cleanup(void) {
    SEC_LOCK();
    for (int i = 0; i < g_trusted_key_count; i++) {
        lv_free((void **) &g_trusted_keys[i].key_id);
        lv_free((void **) &g_trusted_keys[i].digest_hex);
    }
    g_trusted_key_count = 0;
    for (int i = 0; i < g_perm_count; i++)
        lv_free((void **) &g_perm_entries[i].name);
    g_perm_count = 0;
    /* 注入扩展：内置 6 条为静态字面量不释放；扩展（下标 >= 内置数）释放 */
    for (int i = lv_DSL_BUILTIN_PATTERN_COUNT; i < g_injection_count; i++) {
        lv_free((void **) &g_injection_patterns[i].pattern);
        lv_free((void **) &g_injection_patterns[i].description);
    }
    g_injection_count = 0;
    g_sandbox_applied = false;
    g_current_descriptor = NULL;
    SEC_UNLOCK();
}
