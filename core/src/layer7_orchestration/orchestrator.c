#include "lv00/orchestrator.h"
#include <stdlib.h>
#include <string.h>

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

    /* Stage 0: Parse */
    session->stages[LV00_STAGE_PARSE].status = LV00_STAGE_RUNNING;
    /* TODO: invoke parser with input */
    session->stages[LV00_STAGE_PARSE].status = LV00_STAGE_COMPLETED;
    session->stages[LV00_STAGE_PARSE].elapsed_ms = 1.0;

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
    /* TODO: execute single stage */
    session->stages[stage].status = LV00_STAGE_COMPLETED;
    return 0;
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
