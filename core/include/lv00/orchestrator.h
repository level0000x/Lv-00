#ifndef LV00_ORCHESTRATOR_H
#define LV00_ORCHESTRATOR_H

#include "lv00/visual_editor.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LV00_LAYER_ORCHESTRATION 7

/* Pipeline stage identifiers */
typedef enum {
    LV00_STAGE_PARSE,           /* Layer 1: Parse input */
    LV00_STAGE_RESOURCE,        /* Layer 2: Load resources */
    LV00_STAGE_GEOMETRY,        /* Layer 3: Build constraint graph */
    LV00_STAGE_REASONING,       /* Layer 4: Run reasoning strategies */
    LV00_STAGE_OUTPUT,           /* Layer 5: Generate proof output */
    LV00_STAGE_VISUAL,           /* Layer 6: Update visual editor */
    LV00_STAGE_COUNT
} Lv00PipelineStage;

/* Stage status */
typedef enum {
    LV00_STAGE_PENDING,
    LV00_STAGE_RUNNING,
    LV00_STAGE_COMPLETED,
    LV00_STAGE_FAILED,
    LV00_STAGE_SKIPPED
} Lv00StageStatus;

/* Pipeline stage result */
typedef struct Lv00StageResult {
    Lv00PipelineStage stage;
    Lv00StageStatus status;
    double elapsed_ms;
    char error_msg[512];
    void *output;  /* Stage-specific output data */
} Lv00StageResult;

/* Session configuration */
typedef struct Lv00SessionConfig {
    int max_reasoning_depth;
    int timeout_ms;
    int enable_visualization;
    char input_format[64];    /* "lv00-dsl", "json", "lean4", "coq" */
    char output_format[64];   /* "proof", "tikz", "coq", "lean4", "json" */
} Lv00SessionConfig;

/* Session (top-level orchestration unit) */
typedef struct Lv00Session {
    int session_id;
    char session_name[256];

    /* Configuration */
    Lv00SessionConfig config;

    /* Pipeline state */
    Lv00StageResult stages[LV00_STAGE_COUNT];

    /* Layer references */
    void *parser;        /* Layer 1 */
    void *resource_mgr;  /* Layer 2 */
    void *geometry;      /* Layer 3 */
    void *reasoning;     /* Layer 4 */
    void *output;        /* Layer 5 */
    void *visual;        /* Layer 6 */

    /* Aggregate result */
    int success;
    char final_error[1024];
} Lv00Session;

/* Lifecycle */
Lv00Session *lv00_session_create(const char *name);
void lv00_session_destroy(Lv00Session *session);
int lv00_session_configure(Lv00Session *session, const Lv00SessionConfig *config);

/* Pipeline execution */
int lv00_session_run(Lv00Session *session, const char *input);
int lv00_session_run_stage(Lv00Session *session, Lv00PipelineStage stage);
int lv00_session_run_from(Lv00Session *session, Lv00PipelineStage from_stage);

/* Query */
const Lv00StageResult *lv00_session_stage_result(const Lv00Session *session, Lv00PipelineStage stage);
int lv00_session_success(const Lv00Session *session);
const char *lv00_session_error(const Lv00Session *session);
double lv00_session_total_time(const Lv00Session *session);

/* Default configuration */
Lv00SessionConfig lv00_default_session_config(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_ORCHESTRATOR_H */
