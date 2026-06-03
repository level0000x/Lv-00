#include "lv00/orchestrator.h"
#include "lv00/lv00_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int session_counter = 0;

Lv00SessionConfig lv00_default_session_config(void) {
    Lv00SessionConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.max_reasoning_depth = 100;
    cfg.timeout_ms = 30000;
    cfg.enable_visualization = 0;
    strncpy(cfg.input_format, "lv00-dsl", sizeof(cfg.input_format) - 1);
    strncpy(cfg.output_format, "proof", sizeof(cfg.output_format) - 1);
    return cfg;
}

Lv00Session *lv00_session_create(const char *name) {
    Lv00Session *session = calloc(1, sizeof(Lv00Session));
    if (!session) return NULL;
    session->session_id = ++session_counter;
    if (name) strncpy(session->session_name, name, sizeof(session->session_name) - 1);
    session->config = lv00_default_session_config();
    for (int i = 0; i < LV00_STAGE_COUNT; i++) {
        session->stages[i].stage = (Lv00PipelineStage)i;
        session->stages[i].status = LV00_STAGE_PENDING;
    }
    return session;
}

void lv00_session_destroy(Lv00Session *session) {
    free(session);
}

int lv00_session_configure(Lv00Session *session, const Lv00SessionConfig *config) {
    if (!session || !config) return -1;
    session->config = *config;
    return 0;
}

int lv00_session_run(Lv00Session *session, const char *input) {
    if (!session || !input) return -1;
    session->success = 0;

    /* Stage 0: Parse —— 调用 Layer 1 解析器处理输入文本 */
    session->stages[LV00_STAGE_PARSE].status = LV00_STAGE_RUNNING;
    {
        /* 调用解析器处理输入（Layer 1 接口） */
        int parse_ok = 1; /* 解析成功标志 */
        /* TODO: 当 Layer 1 parser API 就绪后，替换为实际调用 */
        /* int parse_rc = lv00_parser_parse(input, &session->parsed_ast); */
        /* parse_ok = (parse_rc == 0); */
        (void)input; /* 暂时抑制未使用警告 */

        if (parse_ok) {
            session->stages[LV00_STAGE_PARSE].status = LV00_STAGE_COMPLETED;
            session->stages[LV00_STAGE_PARSE].elapsed_ms = 1.0;
        } else {
            session->stages[LV00_STAGE_PARSE].status = LV00_STAGE_FAILED;
            strncpy(session->stages[LV00_STAGE_PARSE].error_msg,
                    "解析失败：输入格式无效", sizeof(session->stages[LV00_STAGE_PARSE].error_msg) - 1);
            strncpy(session->final_error, "Stage 0 (Parse) 失败",
                    sizeof(session->final_error) - 1);
            session->success = 0;
            return -1;
        }
    }

    /* Stage 1: Resource */
    session->stages[LV00_STAGE_RESOURCE].status = LV00_STAGE_RUNNING;
    session->stages[LV00_STAGE_RESOURCE].status = LV00_STAGE_COMPLETED;
    session->stages[LV00_STAGE_RESOURCE].elapsed_ms = 0.5;

    /* Stage 2: Geometry */
    session->stages[LV00_STAGE_GEOMETRY].status = LV00_STAGE_RUNNING;
    session->stages[LV00_STAGE_GEOMETRY].status = LV00_STAGE_COMPLETED;
    session->stages[LV00_STAGE_GEOMETRY].elapsed_ms = 2.0;

    /* Stage 3: Reasoning */
    session->stages[LV00_STAGE_REASONING].status = LV00_STAGE_RUNNING;
    session->stages[LV00_STAGE_REASONING].status = LV00_STAGE_COMPLETED;
    session->stages[LV00_STAGE_REASONING].elapsed_ms = 10.0;

    /* Stage 4: Output */
    session->stages[LV00_STAGE_OUTPUT].status = LV00_STAGE_RUNNING;
    session->stages[LV00_STAGE_OUTPUT].status = LV00_STAGE_COMPLETED;
    session->stages[LV00_STAGE_OUTPUT].elapsed_ms = 1.0;

    /* Stage 5: Visual (optional) */
    if (session->config.enable_visualization) {
        session->stages[LV00_STAGE_VISUAL].status = LV00_STAGE_RUNNING;
        session->stages[LV00_STAGE_VISUAL].status = LV00_STAGE_COMPLETED;
        session->stages[LV00_STAGE_VISUAL].elapsed_ms = 5.0;
    } else {
        session->stages[LV00_STAGE_VISUAL].status = LV00_STAGE_SKIPPED;
    }

    session->success = 1;
    return 0;
}

int lv00_session_run_stage(Lv00Session *session, Lv00PipelineStage stage) {
    if (!session || stage < 0 || stage >= LV00_STAGE_COUNT) return -1;
    session->stages[stage].status = LV00_STAGE_RUNNING;

    /* 检查前置阶段是否已完成（除第一个阶段外） */
    if (stage > LV00_STAGE_PARSE) {
        if (session->stages[stage - 1].status != LV00_STAGE_COMPLETED) {
            session->stages[stage].status = LV00_STAGE_FAILED;
            snprintf(session->stages[stage].error_msg,
                     sizeof(session->stages[stage].error_msg),
                     "前置阶段 %d 未完成，无法执行阶段 %d", stage - 1, stage);
            return -1;
        }
    }

    /* 按阶段类型分发执行 */
    int rc = 0;
    switch (stage) {
    case LV00_STAGE_PARSE:
        /* 调用解析器处理输入 */
        /* TODO: 当 Layer 1 parser API 就绪后，替换为实际调用 */
        session->stages[stage].elapsed_ms = 1.0;
        session->stages[stage].status = LV00_STAGE_COMPLETED;
        break;

    case LV00_STAGE_RESOURCE:
        /* 调用资源管理器初始化 */
        /* TODO: 当 Layer 2 resource API 就绪后，替换为实际调用 */
        session->stages[stage].elapsed_ms = 0.5;
        session->stages[stage].status = LV00_STAGE_COMPLETED;
        break;

    case LV00_STAGE_GEOMETRY:
        /* 调用几何引擎初始化 */
        /* TODO: 当几何引擎 API 就绪后，替换为实际调用 */
        session->stages[stage].elapsed_ms = 2.0;
        session->stages[stage].status = LV00_STAGE_COMPLETED;
        break;

    case LV00_STAGE_REASONING:
        /* 调用证明引擎（多策略尝试） */
        /* TODO: 当推理引擎 API 就绪后，替换为实际调用 */
        session->stages[stage].elapsed_ms = 10.0;
        session->stages[stage].status = LV00_STAGE_COMPLETED;
        break;

    case LV00_STAGE_OUTPUT:
        /* 调用输出生成器 */
        /* TODO: 当输出 API 就绪后，替换为实际调用 */
        session->stages[stage].elapsed_ms = 1.0;
        session->stages[stage].status = LV00_STAGE_COMPLETED;
        break;

    case LV00_STAGE_VISUAL:
        /* 调用可视化编辑器（可选阶段） */
        if (session->config.enable_visualization) {
            /* TODO: 当可视化 API 就绪后，替换为实际调用 */
            session->stages[stage].elapsed_ms = 5.0;
            session->stages[stage].status = LV00_STAGE_COMPLETED;
        } else {
            session->stages[stage].status = LV00_STAGE_SKIPPED;
        }
        break;

    default:
        session->stages[stage].status = LV00_STAGE_FAILED;
        snprintf(session->stages[stage].error_msg,
                 sizeof(session->stages[stage].error_msg),
                 "未知阶段: %d", stage);
        rc = -1;
        break;
    }

    return rc;
}

int lv00_session_run_from(Lv00Session *session, Lv00PipelineStage from_stage) {
    if (!session || from_stage < 0 || from_stage >= LV00_STAGE_COUNT) return -1;
    for (int i = from_stage; i < LV00_STAGE_COUNT; i++) {
        int rc = lv00_session_run_stage(session, (Lv00PipelineStage)i);
        if (rc != 0) return rc;
    }
    return 0;
}

const Lv00StageResult *lv00_session_stage_result(const Lv00Session *session, Lv00PipelineStage stage) {
    return (session && stage >= 0 && stage < LV00_STAGE_COUNT) ? &session->stages[stage] : NULL;
}

int lv00_session_success(const Lv00Session *session) {
    return session ? session->success : 0;
}

const char *lv00_session_error(const Lv00Session *session) {
    return (session && !session->success) ? session->final_error : NULL;
}

double lv00_session_total_time(const Lv00Session *session) {
    if (!session) return 0.0;
    double total = 0.0;
    for (int i = 0; i < LV00_STAGE_COUNT; i++) {
        total += session->stages[i].elapsed_ms;
    }
    return total;
}
