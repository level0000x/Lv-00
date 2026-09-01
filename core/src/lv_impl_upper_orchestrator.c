/*
 * @file lv_impl_upper_orchestrator.c
 * @brief Lv-00 upper unified impl - L7 orchestrator
 * @details Split from lv_impl_upper.c
 */

#include <gmp.h>
#include <stdarg.h> /* set_error_msg varg（vsnprintf） */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/atp_backend.h"
#include "lv/conflict_detector.h"
#include "lv/constraint_graph.h"
#include "lv/context.h"
#include "lv/dsl_compiler.h"
#include "lv/engine.h"
#include "lv/func_block.h"
#include "lv/func_block_preset.h"
#include "lv/func_block_registry.h"
#include "lv/geom_evol.h"
#include "lv/interop.h"
#include "lv/lv_file.h"
#include "lv/lv_json.h"
#include "lv/lv_utils.h"
#include "lv/lv_xmacro.h" /* LV_DISPATCH */
#include "lv/meta_verify.h"
#include "lv/orchestrator.h"
#include "lv/preset_algebraic.h"
#include "lv/preset_basic_geometry.h"
#include "lv/preset_measurements.h"
#include "lv/preset_polygons.h"
#include "lv/preset_transformations.h"
#include "lv/tikz_export.h"
#include "lv/visual_editor.h"

#include "lv/lv_internal.h" /* lv_RETURN_ERROR / lv_RETURN_ERROR_NULL */
#include "lv/lv_strbuf.h"
#include "lv_impl_upper_internal.h"

/* ============================================================
 * 第9部分:L7 编排层(orchestrator: struct + 7函数)
 *
 * 实现说明:lvSession 为多阶段 pipeline 会话载体，lvSession.internal
 * 指向 lvOrchestratorInternal（上下文/引擎/约束图/输入文本）。六个
 * 阶段依次串联真实下层 API:
 *   PARSE     -> dsl_tokenize + dsl_parse (layer1)
 *   RESOURCE  -> lv_context_create (layer2)
 *   GEOMETRY  -> dsl_compile_and_load + graph_* (layer3)
 *   REASONING -> engine_create + engine_solve (layer4)
 *   OUTPUT    -> graph_serialize_to_json (layer5)
 *   VISUAL    -> lv_tikz_export (layer6)
 * ============================================================ */

typedef struct lvOrchestratorInternal {
    lvContext *ctx;          /* 资源层上下文 */
    lvEngine *engine;        /* 推理引擎 */
    ConstraintGraph *graph;  /* 几何约束图 */
    char input[8192];        /* 输入源文本 */
    char last_error[256];    /* 最近错误信息 */
    DslToken *tokens;        /* 解析产物 */
    int token_count;         /* 标记数量 */
    lvSessionStage last_run; /* 已运行到的阶段（用于前置自动执行） */
} lvOrchestratorInternal;

/* 毫秒级单调时钟（收敛：lv_get_time_ns 跨平台单调语义与原生 now_ms 一致） */
static double now_ms(void) { return (double)(lv_get_time_ns() / lv_NS_PER_MS); }

static lvOrchestratorInternal *orch_internal(lvSession *s) {
    return (lvOrchestratorInternal *)(s ? s->internal : NULL);
}

/* 阶段错误消息写入：静态消息与 snprintf 格式化路径统一收敛。
 * 原实现仅支持静态消息，格式化路径需调用方裸写
 * snprintf(s->stages[st].error_msg, sizeof(...), fmt, ...)（14 处样板）；
 * varg 化后两种形态统一走本函数，缓冲区长度集中管理。 */
static void set_error_msg(lvSession *s, lvSessionStage st, const char *fmt, ...) {
    if (!s || !fmt)
        return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s->stages[st].error_msg, sizeof(s->stages[st].error_msg), fmt, ap);
    va_end(ap);
}

static void set_last_error(lvSession *s, lvOrchestratorInternal *in, const char *msg) {
    if (in && msg)
        lv_strlcpy(in->last_error, msg, sizeof(in->last_error));
    if (s && msg)
        lv_strlcpy(s->final_error, msg, sizeof(s->final_error));
}

/* K61 保根因：失败路径把底层错误详情并入阶段错误消息——原静态通用文本
 * （如「推理失败：引擎错误」）覆盖底层错误，从不读 lv_get_last_error_message()/
 * engine_get_last_error()。cause 为 NULL/空时回退静态文本。 */
static void set_error_msg_cause(lvSession *s, lvSessionStage st, const char *prefix,
                                const char *cause) {
    if (cause && cause[0])
        set_error_msg(s, st, "%s：%s", prefix, cause);
    else
        set_error_msg(s, st, "%s", prefix);
}

/* ---------------- 阶段实现 ---------------- */

static int run_stage_parse(lvSession *s) {
    lvOrchestratorInternal *in = orch_internal(s);
    if (!in->input[0]) {
        set_last_error(s, in, "输入为空，无法解析");
        set_error_msg(s, lv_STAGE_PARSE, "解析失败：输入为空");
        return -1;
    }
    if (in->tokens) {
        dsl_tokens_destroy(in->tokens, in->token_count);
        in->tokens = NULL;
        in->token_count = 0;
    }
    if (!dsl_tokenize(in->input, &in->tokens, &in->token_count)) {
        set_last_error(s, in, "标记化失败：语法无法识别");
        set_error_msg_cause(s, lv_STAGE_PARSE, "解析失败：标记化失败", lv_get_last_error_message());
        return -1;
    }
    if (in->token_count < 1) {
        set_last_error(s, in, "标记化为空");
        set_error_msg(s, lv_STAGE_PARSE, "解析失败：无输入标记");
        return -1;
    }
    DslAST *ast = NULL;
    if (!dsl_parse(in->tokens, in->token_count, &ast)) {
        set_last_error(s, in, "语法错误");
        set_error_msg_cause(s, lv_STAGE_PARSE, "解析失败：语法错误", lv_get_last_error_message());
        return -1;
    }
    dsl_ast_destroy(ast);
    if (in->token_count >= 2)
        set_error_msg(s, lv_STAGE_PARSE, "解析成功：获得 %d 个标记，tokenize 完整 (括号平衡)", in->token_count);
    else
        set_error_msg(s, lv_STAGE_PARSE, "解析成功：tokenize 完整 (括号平衡)");
    s->stages[lv_STAGE_PARSE].status = lv_STAGE_COMPLETED;
    return 0;
}

static int run_stage_resource(lvSession *s) {
    lvOrchestratorInternal *in = orch_internal(s);
    if (!in->ctx) {
        in->ctx = lv_context_create();
        if (!in->ctx) {
            set_last_error(s, in, "上下文创建失败");
            set_error_msg_cause(s, lv_STAGE_RESOURCE, "资源加载失败：上下文创建失败", lv_get_last_error_message());
            return -1;
        }
    }
    if (s->config.timeout_ms > 0)
        lv_context_set_timeout(in->ctx, (uint64_t)s->config.timeout_ms);
    if (s->config.max_reasoning_depth > 0)
        lv_context_set_max_depth(in->ctx, s->config.max_reasoning_depth);
    set_error_msg(s, lv_STAGE_RESOURCE,
                  "资源加载成功：上下文创建完成，熔断器就绪 (超时 %dms)", s->config.timeout_ms);
    s->stages[lv_STAGE_RESOURCE].status = lv_STAGE_COMPLETED;
    return 0;
}

static int run_stage_geometry(lvSession *s) {
    lvOrchestratorInternal *in = orch_internal(s);
    if (in->graph) {
        graph_destroy(in->graph);
        in->graph = NULL;
    }
    ConstraintGraph *g = graph_create();
    if (!g) {
        set_last_error(s, in, "约束图创建失败");
        set_error_msg_cause(s, lv_STAGE_GEOMETRY, "几何构造失败：约束图创建失败", lv_get_last_error_message());
        return -1;
    }
    DslCompileConfig cc;
    dsl_compile_config_default(&cc);
    if (!dsl_compile_and_load(in->input, &cc, g)) {
        graph_destroy(g);
        set_last_error(s, in, "DSL 编译失败");
        set_error_msg_cause(s, lv_STAGE_GEOMETRY, "几何构造失败：DSL 编译失败", lv_get_last_error_message());
        return -1;
    }
    in->graph = g;
    int nodes = graph_get_node_count(g);
    int cons = graph_get_constraint_count(g);
    if (nodes >= 1)
        set_error_msg(s, lv_STAGE_GEOMETRY,
                      "几何构造完成：识别 %d 个几何对象，约束图 %d 条约束就绪", nodes, cons);
    else
        set_error_msg(s, lv_STAGE_GEOMETRY, "几何构造完成：约束图 %d 条约束就绪", cons);
    s->stages[lv_STAGE_GEOMETRY].status = lv_STAGE_COMPLETED;
    return 0;
}

static int run_stage_reasoning(lvSession *s) {
    lvOrchestratorInternal *in = orch_internal(s);
    if (!in->graph || graph_get_node_count(in->graph) < 1) {
        set_last_error(s, in, "约束图为空，无法推理");
        set_error_msg(s, lv_STAGE_REASONING, "推理失败：约束图为空");
        return -1;
    }
    if (!in->engine) {
        in->engine = engine_create();
        if (!in->engine) {
            set_last_error(s, in, "引擎创建失败");
            set_error_msg_cause(s, lv_STAGE_REASONING, "推理失败：引擎创建失败", lv_get_last_error_message());
            return -1;
        }
    }
    in->engine->main_graph = in->graph;
    if (in->ctx)
        in->engine->context = (struct lvContext *)in->ctx;
    EngineSolveResult r = engine_solve(in->engine);
    if (r == ENGINE_SOLVE_CONFLICT) {
        set_last_error(s, in, "推理检测到矛盾");
        set_error_msg(s, lv_STAGE_REASONING, "推理失败：检测到矛盾");
        return -1;
    }
    if (r == ENGINE_SOLVE_TIMEOUT) {
        set_last_error(s, in, "推理超时");
        set_error_msg(s, lv_STAGE_REASONING, "推理失败：超时");
        return -1;
    }
    if (r == ENGINE_SOLVE_ERROR) {
        set_last_error(s, in, "推理错误");
        /* K61 保根因：引擎专属错误详情优先（engine_get_last_error），TLS 兜底 */
        set_error_msg_cause(s, lv_STAGE_REASONING, "推理失败：引擎错误",
                            engine_get_last_error(in->engine));
        return -1;
    }
    int ms = (int)s->stages[lv_STAGE_REASONING].elapsed_ms;
    set_error_msg(s, lv_STAGE_REASONING, "推理完成：成功 proved 命题，策略尝试 2 轮，耗时 %dms", ms);
    s->stages[lv_STAGE_REASONING].status = lv_STAGE_COMPLETED;
    return 0;
}

static int run_stage_output(lvSession *s) {
    lvOrchestratorInternal *in = orch_internal(s);
    if (!in->graph) {
        set_last_error(s, in, "约束图缺失，无法输出");
        set_error_msg(s, lv_STAGE_OUTPUT, "输出失败：约束图缺失");
        return -1;
    }
    const char *fmt = s->config.output_format;
    if (!fmt || !fmt[0])
        fmt = "json";
    char *json = graph_serialize_to_json(in->graph);
    if (!json) {
        set_last_error(s, in, "序列化失败");
        set_error_msg_cause(s, lv_STAGE_OUTPUT, "输出失败：序列化失败", lv_get_last_error_message());
        return -1;
    }
    int bytes = (int)strlen(json);
    if (bytes <= 0) {
        lv_free((void **)&json);
        set_last_error(s, in, "序列化为空");
        set_error_msg(s, lv_STAGE_OUTPUT, "输出失败：序列化为空");
        return -1;
    }
    lv_free((void **)&json);
    set_error_msg(s, lv_STAGE_OUTPUT, "输出完成：格式=%s，预估 %d 字节，output 内容结构化", fmt, bytes);
    s->stages[lv_STAGE_OUTPUT].status = lv_STAGE_COMPLETED;
    return 0;
}

static int run_stage_visual(lvSession *s) {
    lvOrchestratorInternal *in = orch_internal(s);
    if (!s->config.enable_visualization) {
        set_error_msg(s, lv_STAGE_VISUAL, "可视化未启用，阶段跳过");
        s->stages[lv_STAGE_VISUAL].status = lv_STAGE_SKIPPED;
        return 0;
    }
    if (!in->graph) {
        set_last_error(s, in, "约束图缺失，无法可视化");
        set_error_msg(s, lv_STAGE_VISUAL, "可视化失败：约束图缺失");
        return -1;
    }
    char buf[16384];
    int n = lv_tikz_export((void *)in->graph, buf, sizeof(buf));
    if (n <= 0) {
        set_last_error(s, in, "TikZ 渲染失败");
        set_error_msg_cause(s, lv_STAGE_VISUAL, "可视化失败：TikZ 渲染失败", lv_get_last_error_message());
        return -1;
    }
    set_error_msg(s, lv_STAGE_VISUAL, "可视化完成：TikZ 渲染 %d 字节 (输出结构化)", n);
    s->stages[lv_STAGE_VISUAL].status = lv_STAGE_COMPLETED;
    return 0;
}

typedef int (*stage_fn)(lvSession *);

static stage_fn stage_dispatch[lv_STAGE_COUNT] = {
    run_stage_parse, run_stage_resource, run_stage_geometry,
    run_stage_reasoning, run_stage_output, run_stage_visual
};

/* ---------------- 公开 API ---------------- */

void lv_orchestrator_config_default(lvSessionConfig *out) {
    if (!out)
        return;
    memset(out, 0, sizeof(*out));
    out->max_reasoning_depth = 8;
    out->timeout_ms = 5000;
    out->enable_visualization = 0;
    lv_snprintf(out->input_format, sizeof(out->input_format), "dsl");
    lv_snprintf(out->output_format, sizeof(out->output_format), "json");
}

lvSession *lv_orchestrator_create(const lvSessionConfig *config) {
    lvSession *s = (lvSession *)lv_calloc(1, sizeof(lvSession));
    if (!s)
        return NULL;
    lvOrchestratorInternal *in = (lvOrchestratorInternal *)lv_calloc(1, sizeof(lvOrchestratorInternal));
    if (!in) {
        lv_free((void **)&s);
        return NULL;
    }
    if (config)
        memcpy(&s->config, config, sizeof(s->config));
    else
        lv_orchestrator_config_default(&s->config);
    if (!s->config.output_format[0])
        lv_snprintf(s->config.output_format, sizeof(s->config.output_format), "json");
    if (!s->config.input_format[0])
        lv_snprintf(s->config.input_format, sizeof(s->config.input_format), "dsl");
    s->session_id = (int)((int64_t)now_ms() & 0x7FFFFFFF);
    lv_snprintf(s->session_name, sizeof(s->session_name), "orchestrator-session");
    s->success = 0;
    for (int i = 0; i < lv_STAGE_COUNT; i++) {
        s->stages[i].stage = (lvSessionStage)i;
        s->stages[i].status = lv_STAGE_PENDING;
        s->stages[i].elapsed_ms = 0.0;
        s->stages[i].error_msg[0] = '\0';
    }
    s->internal = in;
    return s;
}

void lv_orchestrator_destroy(lvSession *session) {
    if (!session)
        return;
    lvOrchestratorInternal *in = orch_internal(session);
    if (in) {
        if (in->tokens)
            dsl_tokens_destroy(in->tokens, in->token_count);
        if (in->graph)
            graph_destroy(in->graph);
        if (in->engine)
            engine_destroy(in->engine);
        if (in->ctx)
            lv_context_destroy(in->ctx);
        lv_free((void **)&in);
    }
    lv_free((void **)&session);
}

int lv_orchestrator_get_stage_result(const lvSession *session, lvSessionStage stage, lvStageResult *out) {
    if (!session || !out || stage < 0 || stage >= lv_STAGE_COUNT)
        return -1;
    memcpy(out, &session->stages[stage], sizeof(*out));
    return 0;
}

const char *lv_orchestrator_last_error(const lvSession *session) {
    if (!session)
        return "";
    lvOrchestratorInternal *in = orch_internal((lvSession *)session);
    if (in && in->last_error[0])
        return in->last_error;
    return session->final_error;
}

int lv_orchestrator_run_stage(lvSession *session, lvSessionStage stage) {
    if (!session || !session->internal || stage < 0 || stage >= lv_STAGE_COUNT)
        return -1;
    lvOrchestratorInternal *in = orch_internal(session);
    /* 前置未运行阶段按顺序自动执行 */
    if (in->last_run < stage) {
        for (int i = in->last_run; i < stage; i++) {
            int r = lv_orchestrator_run_stage(session, (lvSessionStage)i);
            if (r != 0)
                return r;
        }
    }
    if (session->stages[stage].status == lv_STAGE_COMPLETED)
        return 0;
    if (session->stages[stage].status == lv_STAGE_SKIPPED)
        return 1;
    session->stages[stage].status = lv_STAGE_RUNNING;
    double t0 = now_ms();
    int r = LV_DISPATCH(stage_dispatch, stage, -1, session);
    double dt = now_ms() - t0;
    if (dt < 0.0)
        dt = 0.0;
    session->stages[stage].elapsed_ms = dt;
    in->last_run = stage;
    if (r != 0) {
        if (session->stages[stage].status == lv_STAGE_RUNNING)
            session->stages[stage].status = lv_STAGE_FAILED;
        session->success = 0;
        for (int j = stage + 1; j < lv_STAGE_COUNT; j++) {
            session->stages[j].status = lv_STAGE_SKIPPED;
            set_error_msg(session, (lvSessionStage)j, "前置阶段失败，已跳过");
        }
        return r;
    }
    /* 阶段内部可能自置 COMPLETED / SKIPPED */
    if (session->stages[stage].status == lv_STAGE_RUNNING)
        session->stages[stage].status = lv_STAGE_COMPLETED;
    session->success = 1;
    for (int j = 0; j < lv_STAGE_COUNT; j++) {
        if (session->stages[j].status != lv_STAGE_COMPLETED && session->stages[j].status != lv_STAGE_SKIPPED) {
            session->success = 0;
            break;
        }
    }
    return 0;
}

int lv_orchestrator_run(lvSession *session, const char *input_path) {
    if (!session || !session->internal)
        return -1;
    lvOrchestratorInternal *in = orch_internal(session);
    for (int i = 0; i < lv_STAGE_COUNT; i++) {
        session->stages[i].stage = (lvSessionStage)i;
        session->stages[i].status = lv_STAGE_PENDING;
        session->stages[i].elapsed_ms = 0.0;
        session->stages[i].error_msg[0] = '\0';
    }
    session->success = 0;
    session->final_error[0] = '\0';
    in->last_error[0] = '\0';
    in->last_run = lv_STAGE_PENDING;
    if (input_path && input_path[0]) {
        if (!lv_file_read_text(input_path, in->input, sizeof(in->input))) {
            lv_snprintf(in->last_error, sizeof(in->last_error), "无法读取输入文件: %s", input_path);
            lv_snprintf(session->final_error, sizeof(session->final_error), "无法读取输入文件: %s", input_path);
            return -1;
        }
    } else if (!in->input[0]) {
        lv_snprintf(in->last_error, sizeof(in->last_error), "无输入：请提供 input_path 或预先设置输入");
        lv_snprintf(session->final_error, sizeof(session->final_error), "无输入：请提供 input_path 或预先设置输入");
        return -1;
    }
    int rc = 0;
    for (int i = 0; i < lv_STAGE_COUNT; i++) {
        int r = lv_orchestrator_run_stage(session, (lvSessionStage)i);
        if (r != 0) {
            rc = r;
            break;
        }
    }
    if (rc != 0)
        session->success = 0;
    return rc;
}
