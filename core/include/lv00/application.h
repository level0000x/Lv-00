#ifndef LV00_APPLICATION_H
#define LV00_APPLICATION_H

#include "lv00/meta_verify.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LV00_LAYER_APPLICATION 9

/* Application mode */
typedef enum {
    LV00_APP_REPL,          /* Interactive read-eval-print loop */
    LV00_APP_BATCH,         /* Batch file processing */
    LV00_APP_SERVER,         /* Long-running server mode */
    LV00_APP_LIBRARY,        /* Embedded library mode */
    LV00_APP_IDE             /* IDE integration mode */
} Lv00AppMode;

/* Log level */
typedef enum {
    LV00_LOG_ERROR,
    LV00_LOG_WARN,
    LV00_LOG_INFO,
    LV00_LOG_DEBUG,
    LV00_LOG_TRACE
} Lv00LogLevel;

/* Application configuration */
typedef struct Lv00AppConfig {
    Lv00AppMode mode;
    Lv00LogLevel log_level;
    int max_concurrent_sessions;
    int enable_meta_verify;
    char log_file[512];
    char workspace[512];
} Lv00AppConfig;

/* Application (top-level entry point) */
typedef struct Lv00Application {
    int app_id;
    Lv00AppConfig config;

    /* Core components */
    Lv00MetaVerifier *verifier;

    /* Session pool */
    Lv00Session **sessions;
    int session_count;
    int session_capacity;

    /* Statistics */
    int total_sessions_run;
    int total_sessions_passed;
    int total_sessions_failed;
} Lv00Application;

/* Lifecycle */
Lv00Application *lv00_app_create(const Lv00AppConfig *config);
void lv00_app_destroy(Lv00Application *app);

/* Session management */
Lv00Session *lv00_app_create_session(Lv00Application *app, const char *name);
int lv00_app_run_session(Lv00Application *app, Lv00Session *session, const char *input);
int lv00_app_remove_session(Lv00Application *app, int session_id);

/* Batch processing */
int lv00_app_run_batch(Lv00Application *app, const char **files, int file_count);
int lv00_app_run_directory(Lv00Application *app, const char *dir_path);

/* REPL */
int lv00_app_run_repl(Lv00Application *app);

/* Query */
int lv00_app_stats(const Lv00Application *app, int *total, int *passed, int *failed);

/* Default config */
Lv00AppConfig lv00_default_app_config(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_APPLICATION_H */
