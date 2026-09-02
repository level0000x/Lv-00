/**
 * @file plugin_security.h
 * @brief 蓝图插件安全层（TEN_LAYER_OPTIMIZED_PLAN §16.1/§16.2.1 + §4.1.1 落地）
 *
 * 插件安全层在既有 plugin_system（L5，52 个生命周期/接口 API）之上补充：
 *   ① 插件描述符查询（lv_plugin_get_descriptor 兼容入口）
 *   ② 签名验证（SHA-256 哈希校验 + 信任密钥表；对称哈希校验，非密码学强签名）
 *   ③ 沙箱配置（配置记录模式，非 OS 级隔离——本库嵌入式无子进程模型，诚实标注范围）
 *   ④ 权限模型（三级权限 + lv_REQUIRE_PERMISSION 宏 + 审计日志接线 lv_log）
 *   ⑤ DSL 注入检测（可扩展模式表，默认 6 条内置模式）
 *
 * 所有权：[take] 输出由调用方释放；权限/信任表为进程级静态（线程安全）。
 */

#ifndef lv_PLUGIN_SECURITY_H
#define lv_PLUGIN_SECURITY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
#include "plugin_system.h" /* lvPlugin 结构、lvPluginInfo */

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * ① 插件描述符（§4.1.1 兼容）
 * ============================================================ */

/** @brief 插件生命周期回调签名（蓝图 lv_plugin_on_load_fn 等） */
typedef int (*lv_plugin_on_load_fn)(void *ctx);
typedef int (*lv_plugin_on_unload_fn)(void *ctx);
typedef int (*lv_plugin_on_activate_fn)(void *ctx);
typedef int (*lv_plugin_on_deactivate_fn)(void *ctx);
typedef int (*lv_plugin_on_configure_fn)(void *ctx, void *config);
typedef int (*lv_plugin_on_event_fn)(void *ctx, void *event);

/** @brief 插件描述符（蓝图 lvPluginDescriptor；库内插件用 info_json 查询，本结构为兼容入口） */
typedef struct {
    const char *name;                     /**< 插件名 */
    int version;                          /**< 版本 */
    lv_plugin_on_load_fn on_load;         /**< 加载回调 */
    lv_plugin_on_unload_fn on_unload;     /**< 卸载回调 */
    lv_plugin_on_activate_fn on_activate; /**< 激活回调 */
    lv_plugin_on_deactivate_fn on_deactivate; /**< 停用回调 */
    lv_plugin_on_configure_fn on_configure;   /**< 配置回调 */
    lv_plugin_on_event_fn on_event;       /**< 事件回调 */
} lvPluginDescriptor;

/**
 * @brief 获取当前插件描述符（蓝图 lv_plugin_get_descriptor）
 *
 * 库内插件系统用 info_json（lv_plugin_get_info_json）查询插件信息，无
 * 「插件自身导出描述符」模型。本函数为蓝图兼容入口：返回进程级登记的
 * 当前插件描述符（若经 lv_plugin_security_register_descriptor 登记），
 * 未登记返回 NULL。插件系统内部不依赖本符号。
 *
 * @return 当前插件描述符；未登记返回 NULL
 */
lv_PUBLIC_API const lvPluginDescriptor *lv_plugin_get_descriptor(void);

/**
 * @brief 登记进程级插件描述符（G6 扩展：供 get_descriptor 返回）
 * @param desc 描述符（[borrow]，调用方保证存活）
 */
lv_PUBLIC_API void lv_plugin_security_register_descriptor(const lvPluginDescriptor *desc);

/* ============================================================
 * ② 签名验证（§16.1.1）
 * ============================================================ */

/** @brief 签名验证结果（蓝图 lvSignatureResult） */
typedef enum {
    lv_SIG_OK = 0,            /**< 校验通过 */
    lv_SIG_NO_SIGNATURE,      /**< 无签名文件 */
    lv_SIG_INVALID_FORMAT,    /**< 签名格式无效 */
    lv_SIG_HASH_MISMATCH,     /**< 哈希不匹配 */
    lv_SIG_KEY_UNTRUSTED,     /**< 密钥不在信任表 */
    lv_SIG_EXPIRED,           /**< 过期（预留） */
    lv_SIG_INTERNAL_ERROR     /**< 内部错误 */
} lvSignatureResult;

/** @brief 签名结果名称（调试/日志用） */
lv_PUBLIC_API const char *lv_signature_result_str(lvSignatureResult result);

/**
 * @brief 验证插件文件签名（蓝图 lv_plugin_verify_signature）
 *
 * 计算 plugin_path 文件 SHA-256；签名文件 = manifest_path 指向的文本文件，
 * 首行为期望哈希 hex（可含 key_id 前缀 "key_id:hex"）。签名中的 key_id
 * 若给出则须在信任表（lv_plugin_add_trusted_key）内，否则视为
 * lv_SIG_KEY_UNTRUSTED。对称哈希校验（非密码学强签名，文档注明）。
 *
 * @param plugin_path   插件文件路径
 * @param manifest_path 签名文件路径（期望哈希所在）
 * @return 验证结果
 */
lv_PUBLIC_API lvSignatureResult lv_plugin_verify_signature(const char *plugin_path, const char *manifest_path);

/**
 * @brief 添加信任密钥（蓝图 lv_plugin_add_trusted_key）
 *
 * 信任表存 key_id → 摘要哈希 hex（信任白名单）。public_key_pem 参数保留
 * （未解析；仅当其为纯 hex 摘要时登记——见实现）。重复 key_id 覆盖。
 *
 * @param public_key_pem 密钥（可为纯 hex 摘要或任意标记串）
 * @param key_id         密钥 ID（非 NULL）
 * @return true 登记成功
 */
lv_PUBLIC_API bool lv_plugin_add_trusted_key(const char *public_key_pem, const char *key_id);

/** @brief 设置签名验证强制策略（蓝图 lv_plugin_set_enforcement） */
lv_PUBLIC_API void lv_plugin_set_enforcement(bool enforce);

/** @brief 查询当前强制策略 */
lv_PUBLIC_API bool lv_plugin_enforcement_enabled(void);

/* ============================================================
 * ③ 沙箱（§16.1.2，配置记录模式）
 * ============================================================ */

/** @brief 沙箱配置（蓝图 lvSandboxConfig） */
typedef struct {
    uint32_t cpu_time_limit_seconds;  /**< CPU 时限（秒） */
    size_t max_rss_bytes;             /**< 最大 RSS（字节） */
    const char **allowed_paths;       /**< 允许访问路径数组（[borrow]） */
    size_t allowed_path_count;        /**< 路径数 */
    bool allow_network;               /**< 允许网络 */
    bool allow_fork;                  /**< 允许 fork */
    int max_open_fds;                 /**< 最大打开 fd */
    int max_threads;                  /**< 最大线程数 */
} lvSandboxConfig;

/** @brief 默认只读沙箱（蓝图 lv_sandbox_readonly）：30s / 64MB / 禁网络 / 禁 fork / 16 fd */
static inline lvSandboxConfig lv_sandbox_readonly(void) {
    lvSandboxConfig cfg;
    cfg.cpu_time_limit_seconds = 30;
    cfg.max_rss_bytes = 64u * 1024u * 1024u;
    cfg.allowed_paths = NULL;
    cfg.allowed_path_count = 0;
    cfg.allow_network = false;
    cfg.allow_fork = false;
    cfg.max_open_fds = 16;
    cfg.max_threads = 0;
    return cfg;
}

/**
 * @brief 应用沙箱配置（蓝图 lv_sandbox_apply）
 *
 * 本库嵌入式无子进程执行模型，不做 OS 级隔离。实现为「校验 + 记录」：
 * 配置字段合法（时限>0、字节数>0、fd>=0、路径数组非空时 count>0）则记录
 * 为进程级已应用沙箱配置（供 check/审计引用），返回 true；非法返回 false
 * 并设置错误。文档明确：配置记录模式，非强制隔离。
 *
 * @param config 沙箱配置（非 NULL）
 * @return true 配置合法并已记录
 */
lv_PUBLIC_API bool lv_sandbox_apply(const lvSandboxConfig *config);

/**
 * @brief 校验沙箱配置（蓝图 lv_sandbox_check）
 *
 * 检查配置字段合法性；违规写入 violation 描述（截断至 len）返回 false。
 *
 * @param config    沙箱配置（非 NULL）
 * @param violation 违规描述缓冲（可为 NULL）
 * @param len       缓冲长度
 * @return true 配置合法；false 违规（violation 描述原因）
 */
lv_PUBLIC_API bool lv_sandbox_check(const lvSandboxConfig *config, char *violation, size_t len);

/* ============================================================
 * ④ 权限模型与审计（§16.1.3 / §16.1.4）
 * ============================================================ */

/** @brief 权限级别（蓝图 lvPermissionLevel） */
typedef enum {
    lv_PERM_READONLY = 0,     /**< 只读 */
    lv_PERM_CONSTRUCTION = 1, /**< 可构造 */
    lv_PERM_FULL = 2          /**< 完全 */
} lvPermissionLevel;

/** @brief 审计事件类型（蓝图 lvAuditEventType） */
typedef enum {
    lv_AUDIT_PLUGIN_LOAD = 0,
    lv_AUDIT_PLUGIN_UNLOAD,
    lv_AUDIT_PERMISSION_DENIED,
    lv_AUDIT_RESOURCE_VIOLATION,
    lv_AUDIT_SIGNATURE_FAILURE,
    lv_AUDIT_API_CALL,
    lv_AUDIT_SANDBOX_VIOLATION,
} lvAuditEventType;

/**
 * @brief 获取插件权限级别（蓝图 lv_plugin_get_permission）
 *
 * 查进程级插件权限表（plugin->info.name 为键）；未登记返回 lv_PERM_READONLY。
 *
 * @param plugin 插件（非 NULL）
 * @return 权限级别
 */
lv_PUBLIC_API lvPermissionLevel lv_plugin_get_permission(const lvPlugin *plugin);

/** @brief 设置插件权限级别（G6 扩展：登记表填充入口） */
lv_PUBLIC_API bool lv_plugin_set_permission(const char *plugin_name, lvPermissionLevel level);

/** @brief 权限级别名称（蓝图 lv_perm_level_str；越界返回 "UNKNOWN"） */
lv_PUBLIC_API const char *lv_perm_level_str(lvPermissionLevel level);

/** @brief 审计事件类型名称（调试用） */
lv_PUBLIC_API const char *lv_audit_event_type_str(lvAuditEventType type);

/**
 * @brief 审计日志（蓝图 lv_audit_log）
 *
 * 格式化后经 lv_log（tag="audit"）输出。plugin 可为 NULL。
 *
 * @param plugin 插件（可为 NULL）
 * @param type   事件类型
 * @param fmt    printf 风格格式串
 * @param ...    参数
 */
lv_PUBLIC_API void lv_audit_log(const lvPlugin *plugin, lvAuditEventType type, const char *fmt, ...);

/** @brief 权限检查宏（蓝图 lv_REQUIRE_PERMISSION）：权限不足记审计并 return retval */
#define lv_REQUIRE_PERMISSION(plugin, required_level, retval) \
    do {                                                      \
        if ((plugin) == NULL)                                 \
            return (retval);                                  \
        lvPermissionLevel _current = lv_plugin_get_permission(plugin); \
        if (_current < (required_level)) {                    \
            lv_audit_log(plugin, lv_AUDIT_PERMISSION_DENIED,  \
                         "权限不足: 需要 %s, 当前 %s",         \
                         lv_perm_level_str(required_level),    \
                         lv_perm_level_str(_current));         \
            return (retval);                                  \
        }                                                     \
    } while (0)

/* ============================================================
 * ⑤ DSL 注入检测（§16.2.1）
 * ============================================================ */

/** @brief 注入检测模式（蓝图 lvInjectionPattern） */
typedef struct {
    const char *pattern;     /**< 匹配子串 */
    const char *description; /**< 风险描述 */
    int severity;            /**< 严重度（1-3） */
} lvInjectionPattern;

/**
 * @brief DSL 安全检查（蓝图 lv_dsl_security_check）
 *
 * 扫描 input[0..len) 中的注入模式（内置 6 条 + 扩展），命中写入 error
 * 描述并返回 lv_ERROR_INVALID_PARAM；干净返回 lv_OK。NULL 输入视为
 * lv_ERROR_NULL_POINTER。
 *
 * @param input   DSL 输入
 * @param len     输入长度
 * @param error   违规描述缓冲（可为 NULL）
 * @param err_len 缓冲长度
 * @return lv_OK 干净；lv_ERROR_INVALID_PARAM 命中注入模式；其它错误码
 */
lv_PUBLIC_API int lv_dsl_security_check(const char *input, size_t len, char *error, size_t err_len);

/**
 * @brief 添加注入检测模式（G6 扩展：原则 8 可配置，默认内置 6 条）
 * @param pattern 模式子串（非 NULL）
 * @param description 描述（可为 NULL）
 * @param severity 严重度（1-3）
 * @return true 添加成功；false 表满或参数无效
 */
lv_PUBLIC_API bool lv_dsl_add_injection_pattern(const char *pattern, const char *description, int severity);

/** @brief 内置注入模式数（§16.2.1 默认表） */
#define lv_DSL_BUILTIN_PATTERN_COUNT 6

/**
 * @brief 清理插件安全层进程级状态（信任表/权限表/注入扩展）
 *
 * 供 lv.c 模块清理路径调用；测试亦可调用消除泄漏告警。清理后可再次使用。
 */
lv_PUBLIC_API void lv_plugin_security_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* lv_PLUGIN_SECURITY_H */
