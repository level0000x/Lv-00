#include "lv00/application.h"
#include <stdlib.h>
#include <string.h>

Lv00AppConfig lv00_default_app_config(void) {
    Lv00AppConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.mode = LV00_APP_REPL;
    cfg.log_level = LV00_LOG_INFO;
    cfg.max_concurrent_sessions = 4;
    cfg.enable_meta_verify = 1;
    return cfg;
}

Lv00Application *lv00_app_create(const Lv00AppConfig *config) {
    Lv00Application *app = calloc(1, sizeof(Lv00Application));
    if (!app) return NULL;
    if (config) app->config = *config;
    else app->config = lv00_default_app_config();
    app->session_capacity = 16;
    app->sessions = calloc(app->session_capacity, sizeof(Lv00Session *));
    if (app->config.enable_meta_verify) {
        app->verifier = lv00_meta_verifier_create();
    }
    return app;
}

void lv00_app_destroy(Lv00Application *app) {
    if (!app) return;
    for (int i = 0; i < app->session_count; i++) {
        lv00_session_destroy(app->sessions[i]);
    }
    free(app->sessions);
    if (app->verifier) lv00_meta_verifier_destroy(app->verifier);
    free(app);
}

Lv00Session *lv00_app_create_session(Lv00Application *app, const char *name) {
    if (!app) return NULL;
    if (app->session_count >= app->session_capacity) return NULL;
    Lv00Session *session = lv00_session_create(name);
    if (session) {
        app->sessions[app->session_count++] = session;
    }
    return session;
}

int lv00_app_run_session(Lv00Application *app, Lv00Session *session, const char *input) {
    if (!app || !session || !input) return -1;
    int rc = lv00_session_run(session, input);
    app->total_sessions_run++;
    if (rc == 0 && lv00_session_success(session)) {
        app->total_sessions_passed++;
        if (app->verifier) {
            Lv00VerifyReport report = lv00_meta_verify_session(app->verifier, session);
            if (!lv00_verify_report_passed(&report)) {
                app->total_sessions_passed--;
                app->total_sessions_failed++;
            }
        }
    } else {
        app->total_sessions_failed++;
    }
    return rc;
}

int lv00_app_remove_session(Lv00Application *app, int session_id) {
    if (!app) return -1;
    for (int i = 0; i < app->session_count; i++) {
        if (app->sessions[i] && app->sessions[i]->session_id == session_id) {
            lv00_session_destroy(app->sessions[i]);
            app->sessions[i] = app->sessions[--app->session_count];
            return 0;
        }
    }
    return -1;
}

int lv00_app_run_batch(Lv00Application *app, const char **files, int file_count) {
    if (!app || !files || file_count <= 0) return -1;
    int passed = 0;
    for (int i = 0; i < file_count; i++) {
        Lv00Session *session = lv00_app_create_session(app, files[i]);
        if (!session) continue;
        /* TODO: read file content and pass as input */
        int rc = lv00_app_run_session(app, session, files[i]);
        if (rc == 0) passed++;
    }
    return passed;
}

int lv00_app_run_repl(Lv00Application *app) {
    if (!app) return -1;
    /* TODO: implement interactive REPL loop */
    return 0;
}

int lv00_app_stats(const Lv00Application *app, int *total, int *passed, int *failed) {
    if (!app) return -1;
    if (total) *total = app->total_sessions_run;
    if (passed) *passed = app->total_sessions_passed;
    if (failed) *failed = app->total_sessions_failed;
    return 0;
}
