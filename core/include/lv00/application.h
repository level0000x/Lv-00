#ifndef LV00_APPLICATION_H
#define LV00_APPLICATION_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 应用模式与日志级别枚举
 * ======================================================================== */

typedef enum {
    LV00_APP_REPL = 0,
    LV00_APP_BATCH,
    LV00_APP_SERVER,
    LV00_APP_GUI
} Lv00AppMode;

typedef enum {
    LV00_LOG_DEBUG = 0,
    LV00_LOG_INFO,
    LV00_LOG_WARN,
    LV00_LOG_ERROR,
    LV00_LOG_FATAL
} Lv00LogLevel;

/* ========================================================================
 * 应用配置结构体
 * ======================================================================== */

#define LV00_MAX_SESSION_NAME 64

typedef struct Lv00AppConfig {
    Lv00AppMode mode;
    Lv00LogLevel log_level;
    int max_concurrent_sessions;
    int enable_meta_verify;
    char config_path[256];
    char log_path[256];
} Lv00AppConfig;

/* ========================================================================
 * 会话结构体（前向声明）
 * ======================================================================== */

typedef struct Lv00Session Lv00Session;

/* ========================================================================
 * 验证报告结构体
 * ======================================================================== */

typedef struct Lv00VerifyReport {
    int passed;
    int total;
    int warnings;
    char details[1024];
} Lv00VerifyReport;

/* ========================================================================
 * 应用结构体
 * ======================================================================== */

#define LV00_DEFAULT_SESSION_CAPACITY 16

typedef struct Lv00Application {
    Lv00AppConfig config;
    Lv00Session **sessions;
    int session_count;
    int session_capacity;
    void *verifier;  /* Lv00MetaVerifier* */
    int total_sessions_run;
    int total_sessions_passed;
    int total_sessions_failed;
} Lv00Application;

/* ========================================================================
 * 会话函数声明
 * ======================================================================== */

#include "orchestrator.h"

/* ========================================================================
 * 元验证器函数声明
 * ======================================================================== */

void *lv00_meta_verifier_create(void);
void lv00_meta_verifier_destroy(void *verifier);
Lv00VerifyReport lv00_meta_verify_session(void *verifier, Lv00Session *session);
bool lv00_verify_report_passed(const Lv00VerifyReport *report);

/* ========================================================================
 * 日志宏（向后兼容）
 * ======================================================================== */

#ifndef LV00_LOG_ERROR
#define LV00_LOG_ERROR(...) fprintf(stderr, __VA_ARGS__)
#endif

#ifndef LV00_LOG_WARN
#define LV00_LOG_WARN(...) fprintf(stderr, __VA_ARGS__)
#endif

#ifndef LV00_LOG_INFO
#define LV00_LOG_INFO(...) fprintf(stdout, __VA_ARGS__)
#endif

#ifndef LV00_LOG_DEBUG
#define LV00_LOG_DEBUG(...) ((void)0)
#endif

/* ========================================================================
 * 应用 API 函数声明
 * ======================================================================== */

Lv00AppConfig lv00_default_app_config(void);
Lv00Application *lv00_app_create(const Lv00AppConfig *config);
void lv00_app_destroy(Lv00Application *app);
Lv00Session *lv00_app_create_session(Lv00Application *app, const char *name);
int lv00_app_remove_session(Lv00Application *app, int session_id);
int lv00_app_run_session(Lv00Application *app, Lv00Session *session, const char *input);
int lv00_app_run_batch(Lv00Application *app, const char **files, int file_count);
int lv00_app_run_repl(Lv00Application *app);
int lv00_app_stats(const Lv00Application *app, int *total, int *passed, int *failed);

/* 初始化/运行/关闭 */
int lv00_application_init(void);
int lv00_application_run(void);
void lv00_application_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
