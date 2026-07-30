#ifndef lv_APPLICATION_H
#define lv_APPLICATION_H

#include <stdbool.h>
#include <stddef.h>
#include "lv/lv_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 应用模式与日志级别枚举
 * ======================================================================== */

typedef enum { lv_APP_REPL = 0, lv_APP_BATCH, lv_APP_SERVER, lv_APP_GUI } lvAppMode;

typedef enum { lv_LOG_DEBUG = 0, lv_LOG_INFO, lv_LOG_WARN, lv_LOG_ERROR, lv_LOG_FATAL } lvLogLevel;

/* ========================================================================
 * 应用配置结构体
 * ======================================================================== */

#define lv_MAX_SESSION_NAME 64

typedef struct lvAppConfig {
    lvAppMode mode;
    lvLogLevel log_level;
    int max_concurrent_sessions;
    int enable_meta_verify;
    char config_path[256];
    char log_path[256];
} lvAppConfig;

/* ========================================================================
 * 会话结构体（前向声明）
 * ======================================================================== */

typedef struct lvSession lvSession;

/* ========================================================================
 * 验证报告结构体
 * ======================================================================== */

typedef struct lvVerifyReport {
    int passed;
    int total;
    int warnings;
    char details[1024];
} lvVerifyReport;

/* ========================================================================
 * 应用结构体
 * ======================================================================== */

#define lv_DEFAULT_SESSION_CAPACITY 16

typedef struct lvApplication {
    lvAppConfig config;
    lvDArray sessions; /* lvDArray<lvSession*> */
    void *verifier; /* lvMetaVerifier* */
    int total_sessions_run;
    int total_sessions_passed;
    int total_sessions_failed;
} lvApplication;

/* ========================================================================
 * 会话函数声明
 * ======================================================================== */

#include "orchestrator.h"

/* ========================================================================
 * 元验证器函数声明
 * ======================================================================== */

void *lv_meta_verifier_create(void);
void lv_meta_verifier_destroy(void *verifier);
lvVerifyReport lv_meta_verify_session(void *verifier, lvSession *session);
bool lv_verify_report_passed(const lvVerifyReport *report);

/* ========================================================================
 * 日志宏（向后兼容）
 * ======================================================================== */

#ifndef lv_LOG_ERROR
#define lv_LOG_ERROR(...) fprintf(stderr, __VA_ARGS__)
#endif

#ifndef lv_LOG_WARN
#define lv_LOG_WARN(...) fprintf(stderr, __VA_ARGS__)
#endif

#ifndef lv_LOG_INFO
#define lv_LOG_INFO(...) fprintf(stdout, __VA_ARGS__)
#endif

#ifndef lv_LOG_DEBUG
#define lv_LOG_DEBUG(...) ((void) 0)
#endif

/* ========================================================================
 * 应用 API 函数声明
 * ======================================================================== */

lvAppConfig lv_default_app_config(void);
lvApplication *lv_app_create(const lvAppConfig *config);
void lv_app_destroy(lvApplication *app);
lvSession *lv_app_create_session(lvApplication *app, const char *name);
int lv_app_remove_session(lvApplication *app, int session_id);
int lv_app_run_session(lvApplication *app, lvSession *session, const char *input);
int lv_app_run_batch(lvApplication *app, const char **files, int file_count);
int lv_app_run_repl(lvApplication *app);
int lv_app_stats(const lvApplication *app, int *total, int *passed, int *failed);

/* 初始化/运行/关闭 */
int lv_app_init(void);
int lv_app_run(void);
void lv_app_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
