#include "lv00/application.h"
#include "lv00/lv00_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
    if (app->session_count >= app->session_capacity) {
        /* 动态扩容：容量翻倍 */
        int new_cap = app->session_capacity * 2;
        if (new_cap <= app->session_capacity) return NULL; /* 溢出保护 */
        Lv00Session **new_sessions = (Lv00Session **)realloc(
            app->sessions, (size_t)new_cap * sizeof(Lv00Session *));
        if (!new_sessions) return NULL;
        app->sessions = new_sessions;
        app->session_capacity = new_cap;
    }
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

        /* 读取文件内容 */
        FILE *fp = fopen(files[i], "rb");
        if (!fp) {
            LV00_LOG_ERROR("无法打开文件: %s", files[i]);
            continue;
        }
        fseek(fp, 0, SEEK_END);
        long fsize = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        if (fsize < 0) {
            fclose(fp);
            LV00_LOG_ERROR("无法获取文件大小: %s", files[i]);
            continue;
        }
        if (fsize > (long)(100 * 1024 * 1024)) {  /* 限制100MB */
            fclose(fp);
            LV00_LOG_ERROR("File too large: %s (%ld bytes)", files[i], fsize);
            continue;
        }
        char *content = lv00_malloc((size_t)fsize + 1);
        if (!content) {
            fclose(fp);
            LV00_LOG_ERROR("内存分配失败，文件: %s", files[i]);
            continue;
        }
        size_t nread = fread(content, 1, (size_t)fsize, fp);
        fclose(fp);
        content[nread] = '\0';

        /* 将文件内容（而非文件名）传入 session run */
        int rc = lv00_app_run_session(app, session, content);
        lv00_free((void **)&content);
        if (rc == 0) passed++;
    }
    return passed;
}

int lv00_app_run_repl(Lv00Application *app) {
    if (!app) return -1;

    /* 交互式 REPL 循环 */
    char linebuf[4096];
    printf("Lv-00 交互式证明系统 v1.0.0\n");
    printf("输入 \"quit\"、\"exit\" 或 \"q\" 退出\n\n");

    for (;;) {
        printf("lv00> ");
        fflush(stdout);

        /* 从 stdin 读取用户输入 */
        if (!fgets(linebuf, sizeof(linebuf), stdin)) {
            break; /* EOF 或读取错误 */
        }

        /* 去除末尾换行符 */
        size_t len = strlen(linebuf);
        if (len > 0 && linebuf[len - 1] == '\n') {
            linebuf[len - 1] = '\0';
            len--;
        }

        /* 跳过空行 */
        if (len == 0) continue;

        /* 检查退出命令 */
        if (strcmp(linebuf, "quit") == 0 ||
            strcmp(linebuf, "exit") == 0 ||
            strcmp(linebuf, "q") == 0) {
            break;
        }

        /* 创建临时会话并执行输入 */
        Lv00Session *session = lv00_app_create_session(app, "repl");
        if (!session) {
            fprintf(stderr, "错误：无法创建会话\n");
            continue;
        }

        int rc = lv00_app_run_session(app, session, linebuf);
        if (rc == 0 && lv00_session_success(session)) {
            printf("=> 成功\n");
        } else {
            const char *err = lv00_session_error(session);
            printf("=> 失败: %s\n", err ? err : "未知错误");
        }

        /* 销毁临时会话并从数组中移除，防止内存泄漏 */
        lv00_session_destroy(session);
        if (app->session_count > 0) {
            app->sessions[--app->session_count] = NULL;
        }
    }

    printf("\n再见！\n");
    return 0;
}

int lv00_app_stats(const Lv00Application *app, int *total, int *passed, int *failed) {
    if (!app) return -1;
    if (total) *total = app->total_sessions_run;
    if (passed) *passed = app->total_sessions_passed;
    if (failed) *failed = app->total_sessions_failed;
    return 0;
}
