/**
 * @file application.c
 * @brief 应用层实现
 *
 * @details 实现 Lv-00 证明系统的主应用入口，提供会话管理（创建/运行/移除）、
 * REPL 交互式环境、文件批量处理、运行统计等功能。作为顶层编排器，
 * 协调 pipelines（解析→资源→几何→推理→输出→可视化）的完整生命周期。
 *
 * @author Lv-00 Project
 */

#include "lv/application.h"
#include "lv/orchestrator.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

/**
 * @brief 获取默认应用配置
 *
 * @return 填充了默认值的 lvAppConfig 结构体（REPL 模式、INFO 日志级别、最大 4 并发会话、启用元验证）
 */
lvAppConfig lv_default_app_config(void) {
    lvAppConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.mode = lv_APP_REPL;
    cfg.log_level = lv_LOG_INFO;
    cfg.max_concurrent_sessions = 4;
    cfg.enable_meta_verify = 1;
    return cfg;
}

/**
 * @brief 创建新的应用实例
 *
 * 分配并初始化应用结构体，为会话队列预分配内存。若 config 为 NULL 则采用默认配置。
 * 当配置启用元验证时，同时创建元验证器。
 *
 * @param config 应用配置（可为 NULL，将使用默认配置）
 * @return 成功返回应用实例指针，内存分配失败返回 NULL
 */
lvApplication *lv_app_create(const lvAppConfig *config) {
    /* 分配主结构体，用 calloc 确保所有字段初始为零 */
    lvApplication *app = lv_calloc(1, sizeof(lvApplication));
    if (!app) return NULL;
    /* 使用传入配置或默认配置 */
    if (config) app->config = *config;
    else app->config = lv_default_app_config();
    /* 预分配会话指针数组（初始容量 16），后续按需扩容 */
    app->session_capacity = 16;
    app->sessions = lv_calloc(app->session_capacity, sizeof(lvSession *));
    if (!app->sessions) {
        lv_free((void **)&app);
        return NULL;
    }
    /* 若启用元验证，初始化元验证器用于结果校验 */
    if (app->config.enable_meta_verify) {
        app->verifier = lv_meta_verifier_create();
    }
    return app;
}

/**
 * @brief 销毁应用实例并释放资源
 *
 * 释放所有关联的会话、会话数组、元验证器及应用结构体本身的内存。
 * 传入 NULL 时安全返回。
 *
 * @param app 要销毁的应用实例
 */
void lv_app_destroy(lvApplication *app) {
    if (!app) return;
    /* 释放所有注册的会话 */
    for (int i = 0; i < app->session_count; i++) {
        lv_session_destroy(app->sessions[i]);
    }
    /* 释放会话数组及元验证器 */
    lv_free((void **)&app->sessions);
    if (app->verifier) lv_meta_verifier_destroy(app->verifier);
    lv_free((void **)&app);
}

/**
 * @brief 创建新的计算会话
 *
 * 在应用实例中创建并注册一个新的会话。当会话数组满时会自动扩容（容量翻倍）。
 *
 * @param app  应用实例
 * @param name 会话名称
 * @return 成功返回会话指针，app 为 NULL 或内存分配失败返回 NULL
 */
lvSession *lv_app_create_session(lvApplication *app, const char *name) {
    if (!app) return NULL;
    /* 检查容量，不足时进行动态扩容（容量翻倍） */
    if (app->session_count >= app->session_capacity) {
        /* [安全] 乘法前做溢出检查：防止 INT_MAX/2 以下时 int * 2 溢出导致 UB */
        if (app->session_capacity > INT_MAX / 2) return NULL;
        int new_cap = app->session_capacity * 2;
        lvSession **_tmp = (lvSession **)lv_realloc(
            app->sessions, (size_t)new_cap * sizeof(lvSession *));
        if (!_tmp) return NULL;
        app->sessions = _tmp;
        app->session_capacity = new_cap;
    }
    /* 创建新会话并注册到应用实例中 */
    lvSession *session = lv_session_create(name);
    if (session) {
        app->sessions[app->session_count++] = session;
    }
    return session;
}

/**
 * @brief 运行指定的计算会话
 *
 * 以给定输入运行会话的完整流水线（解析→资源→几何→推理→输出→可视化），
 * 并更新应用的运行统计（总次数、通过次数、失败次数）。若启用元验证，还会对结果进行验证。
 *
 * @param app     应用实例
 * @param session 要运行的会话
 * @param input   输入字符串
 * @return 会话运行结果码：成功返回 0，参数无效返回 -1，流水线失败返回非零值
 */
int lv_app_run_session(lvApplication *app, lvSession *session, const char *input) {
    if (!app || !session || !input) return -1;
    int rc = lv_session_run(session, input);
    app->total_sessions_run++;
    /* 会话运行成功时，若启用元验证则进一步校验结果合法性 */
    if (rc == 0 && lv_session_success(session)) {
        app->total_sessions_passed++;
        if (app->verifier) {
            lvVerifyReport report = lv_meta_verify_session(app->verifier, session);
            /* 元验证未通过：撤回 passed 计数，计入 failed */
            if (!lv_verify_report_passed(&report)) {
                app->total_sessions_passed--;
                app->total_sessions_failed++;
            }
        }
    } else {
        app->total_sessions_failed++;
    }
    return rc;
}

/**
 * @brief 移除指定的计算会话
 *
 * 根据会话 ID 查找并销毁对应会话，将其从应用会话数组中移除。
 *
 * @param app        应用实例
 * @param session_id 要移除的会话 ID
 * @return 成功返回 0，app 为 NULL 或未找到匹配会话返回 -1
 */
int lv_app_remove_session(lvApplication *app, int session_id) {
    if (!app) return -1;
    for (int i = 0; i < app->session_count; i++) {
        if (app->sessions[i] && app->sessions[i]->session_id == session_id) {
            lv_session_destroy(app->sessions[i]);
            /* 交换删除：将数组最后一个元素移到被删除位置，避免数组移动开销 */
            app->sessions[i] = app->sessions[--app->session_count];
            return 0;
        }
    }
    return -1;
}

/**
 * @brief 批量运行多个会话
 *
 * 依次读取指定文件列表作为输入，为每个文件创建独立会话并执行完整流水线。
 * 限制单个文件不超过 100MB。
 *
 * @param app        应用实例
 * @param files      输入文件路径数组
 * @param file_count 文件数量
 * @return 成功完成的会话数量，参数无效返回 -1
 */
int lv_app_run_batch(lvApplication *app, const char **files, int file_count) {
    if (!app || !files || file_count <= 0) return -1;
    int passed = 0;
    /* 遍历每个输入文件，为之创建独立会话并执行完整流水线 */
    for (int i = 0; i < file_count; i++) {
        lvSession *session = lv_app_create_session(app, files[i]);
        if (!session) continue;

        /* 以二进制只读方式打开文件 */
        FILE *fp = fopen(files[i], "rb");
        if (!fp) {
            lv_LOG_ERROR("无法打开文件: %s", files[i]);
            continue;
        }
        /* 获取文件大小 */
        fseek(fp, 0, SEEK_END);
        long fsize = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        if (fsize < 0) {
            fclose(fp);
            lv_LOG_ERROR("无法获取文件大小: %s", files[i]);
            continue;
        }
        /* 限制单文件不超过 100MB，防止内存耗尽 */
        if (fsize > (long)(100 * 1024 * 1024)) {
            fclose(fp);
            lv_LOG_ERROR("文件过大: %s (%ld 字节)", files[i], fsize);
            continue;
        }
        /* 分配缓冲区并读取完整文件内容 */
        char *content = lv_malloc((size_t)fsize + 1);
        if (!content) {
            fclose(fp);
            lv_LOG_ERROR("内存分配失败，文件: %s", files[i]);
            continue;
        }
        size_t nread = fread(content, 1, (size_t)fsize, fp);
        if (nread != (size_t)fsize) {
            fclose(fp);
            lv_free((void **)&content);
            lv_LOG_ERROR("读取文件不完整: %s (期望 %ld, 实际 %zu)", files[i], fsize, nread);
            continue;
        }
        fclose(fp);
        content[nread] = '\0';

        /* 将文件内容传入会话流水线执行 */
        int rc = lv_app_run_session(app, session, content);
        lv_free((void **)&content);
        if (rc == 0) passed++;
    }
    return passed;
}

/**
 * @brief 启动交互式 REPL 环境
 *
 * 进入交互式命令行循环，逐行读取用户输入并执行证明流水线。
 * 输入 "quit"、"exit" 或 "q" 退出，EOF 也会退出。
 *
 * @param app 应用实例
 * @return 正常退出返回 0，app 为 NULL 返回 -1
 */
int lv_app_run_repl(lvApplication *app) {
    if (!app) return -1;

    /* 交互式 REPL 循环 */
    char linebuf[4096];
    printf("Lv-00 交互式证明系统 v1.0.0\n");
    printf("输入 \"quit\"、\"exit\" 或 \"q\" 退出\n\n");

    for (;;) {
        printf("lv> ");
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

        /* 为每次输入创建临时会话以隔离执行上下文 */
        lvSession *session = lv_app_create_session(app, "repl");
        if (!session) {
            fprintf(stderr, "错误：无法创建会话\n");
            continue;
        }

        /* 执行流水线并输出结果 */
        int rc = lv_app_run_session(app, session, linebuf);
        if (rc == 0 && lv_session_success(session)) {
            printf("=> 成功\n");
        } else {
            const char *err = lv_session_error(session);
            printf("=> 失败: %s\n", err ? err : "未知错误");
        }

        /*
         * REPL 模式下每个输入使用独立临时会话，执行完毕后立即销毁。
         * 同时手动维护应用内部的 sessions 数组：将最后一个有效指针置空并递减计数。
         */
        lv_session_destroy(session);
        if (app->session_count > 0) {
            app->sessions[--app->session_count] = NULL;
        }
    }

    printf("\n再见！\n");
    return 0;
}

/**
 * @brief 获取应用的运行统计信息
 *
 * 输出截至调用时的累计运行次数、通过次数和失败次数。
 *
 * @param app    应用实例（只读）
 * @param total  输出参数：总运行次数（可为 NULL 跳过）
 * @param passed 输出参数：通过次数（可为 NULL 跳过）
 * @param failed 输出参数：失败次数（可为 NULL 跳过）
 * @return 成功返回 0，app 为 NULL 返回 -1
 */
int lv_app_stats(const lvApplication *app, int *total, int *passed, int *failed) {
    if (!app) return -1;
    if (total) *total = app->total_sessions_run;
    if (passed) *passed = app->total_sessions_passed;
    if (failed) *failed = app->total_sessions_failed;
    return 0;
}
