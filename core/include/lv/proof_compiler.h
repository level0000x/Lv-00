/**
 * @file proof_compiler.h
 * @brief 证明编译层
 *
 * 实现证明对象（Proof Object）、证明跟踪（Proof Trace）和
 * 证明编译器（Proof Compiler），支持多种输出格式。
 *
 * @version 1.1.0
 */
#ifndef lv_PROOF_COMPILER_H
#define lv_PROOF_COMPILER_H
#include <stdbool.h>

#include "proof.h"
/* ============== 前向声明 ============== */
typedef struct lvProofObject lvProofObject;
typedef struct lvProofTrace lvProofTrace;
typedef struct lvProofCompiler lvProofCompiler;
/* ============== 证明对象 ============== */
/**
 * @brief 证明步骤记录
 *
 * 记录证明中的每个步骤及其来源。
 */
typedef struct lvProofStepRecord {
    int step_id;        /**< 步骤ID */
    ProofStepType type; /**< 步骤类型 */
    ProofColor color;   /**< 证明颜色 */
    int rule_id;        /**< 使用的规则ID */
    char *rule_name;    /**< 规则名称 */

    /* 前提步骤 */
    int *premise_step_ids; /**< 前提步骤ID数组（须经 lv_proof_step_record_set_premises 管理） */
    int premise_count;     /**< 前提数量 */
    int premise_capacity;  /**< 前提数组容量 */

    /* 结论 */
    Proposition *conclusion; /**< 结论命题 */
    int conclusion_id;       /**< 结论ID */

    /* 元数据 */
    int depth;           /**< 证明深度 */
    char *justification; /**< 证明理由 */
    int64_t timestamp;   /**< 时间戳 */
} lvProofStepRecord;
/**
 * @brief 证明对象
 *
 * 机器可复核的证明链表示。
 */
struct lvProofObject {
    /* 元数据 */
    int proof_id;           /**< 证明ID */
    char *theorem_name;     /**< 定理名称 */
    Proposition *goal;      /**< 目标命题 */
    bool is_proved;         /**< 是否成功证明 */
    ProofColor final_color; /**< 最终颜色 */

    /* 证明步骤链 */
    lvProofStepRecord **steps; /**< 步骤数组 */
    int step_count;            /**< 步骤数量 */
    int step_capacity;         /**< 步骤容量 */

    /* 假设和公理 */
    int *axiom_ids;          /**< 使用的公理ID */
    int axiom_count;         /**< 公理数量 */
    int axiom_capacity;      /**< 公理数组容量 */
    int *assumption_ids;     /**< 假设ID数组 */
    int assumption_count;    /**< 假设数量 */
    int assumption_capacity; /**< 假设数组容量 */

    /* 统计信息 */
    int max_depth;      /**< 最大证明深度 */
    int64_t elapsed_us; /**< 耗时（微秒） */

    /* 附加数据 */
    void *extra_data; /**< 附加数据指针 */
};
/* ============== 证明跟踪 ============== */
/**
 * @brief 跟踪事件类型
 */
typedef enum {
    TRACE_EVENT_START,         /**< 证明开始 */
    TRACE_EVENT_STEP,          /**< 步骤执行 */
    TRACE_EVENT_BACKTRACK,     /**< 回溯 */
    TRACE_EVENT_BRANCH,        /**< 分支 */
    TRACE_EVENT_LEMMA,         /**< 引理引用 */
    TRACE_EVENT_ORACLE,        /**< Oracle调用 */
    TRACE_EVENT_CONTRADICTION, /**< 发现矛盾 */
    TRACE_EVENT_COMPLETE,      /**< 证明完成 */
    TRACE_EVENT_FAIL           /**< 证明失败 */
} lvTraceEventType;
/**
 * @brief 跟踪事件
 */
typedef struct lvTraceEvent {
    lvTraceEventType type; /**< 事件类型 */
    int step_id;           /**< 关联步骤ID */
    char *description;     /**< 事件描述 */
    char *details;         /**< 详细数据 */
    int depth;             /**< 当前深度 */
    int64_t timestamp;     /**< 时间戳 */

    /* 事件数据 */
    union {
        struct {
            int from_step;
            int to_step;
        } backtrack;
        struct {
            char *branch_name;
            int branch_id;
        } branch;
        struct {
            int lemma_id;
            char *lemma_name;
        } lemma;
        struct {
            lvProofScopeId scope_id;
            int assumption_count;
        } contradiction;
    } data;
} lvTraceEvent;
/**
 * @brief 证明跟踪
 *
 * 逻辑溯源存档，记录完整的证明过程。
 */
struct lvProofTrace {
    int trace_id; /**< 跟踪ID */
    int proof_id; /**< 关联证明ID */

    /* 事件流 */
    lvTraceEvent **events; /**< 事件数组 */
    int event_count;       /**< 事件数量 */
    int event_capacity;    /**< 事件容量 */

    /* 统计 */
    int total_steps;       /**< 总步骤数 */
    int total_backtracks;  /**< 总回溯次数 */
    int max_depth;         /**< 最大深度 */
    int64_t total_time_us; /**< 总耗时 */

    /* 快照 */
    void *snapshot_data; /**< 快照数据 */
};
/* ============== 输出格式 ============== */
/**
 * @brief 输出格式类型
 */
typedef enum {
    OUTPUT_FORMAT_JSON,    /**< JSON格式 */
    OUTPUT_FORMAT_LATEX,   /**< LaTeX格式 */
    OUTPUT_FORMAT_TIKZ,    /**< TikZ格式 */
    OUTPUT_FORMAT_TEXT,    /**< 纯文本格式 */
    OUTPUT_FORMAT_XML,     /**< XML格式 */
    OUTPUT_FORMAT_GRAPHVIZ /**< Graphviz格式 */
} lvOutputFormat;
/* ============== 证明编译器 ============== */
/**
 * @brief 编译器配置
 */
typedef struct lvCompilerConfig {
    lvOutputFormat format; /**< 输出格式 */
    bool include_metadata; /**< 包含元数据 */
    bool include_trace;    /**< 包含跟踪信息 */
    bool verbose;          /**< 详细输出 */
    int max_depth;         /**< 最大输出深度 */
    char *language;        /**< 输出语言（zh/en） */
} lvCompilerConfig;
/**
 * @brief 证明编译器
 *
 * 将证明对象编译为多种输出格式。
 */
struct lvProofCompiler {
    lvCompilerConfig config; /**< 编译配置 */
};
/* ============== API 函数声明 ============== */
/* ---- Proof Object API ---- */
lv_PUBLIC_API lvProofObject *lv_proof_object_create(void);
lv_PUBLIC_API void lv_proof_object_destroy(lvProofObject *obj);
lv_PUBLIC_API int lv_proof_object_add_step(lvProofObject *obj, lvProofStepRecord *step);
lv_PUBLIC_API bool lv_proof_object_add_axiom(lvProofObject *obj, int axiom_id);
lv_PUBLIC_API bool lv_proof_object_add_assumption(lvProofObject *obj, int assumption_id);
lv_PUBLIC_API int lv_proof_object_get_step_count(const lvProofObject *obj);
lv_PUBLIC_API bool lv_proof_object_is_valid(const lvProofObject *obj);
lv_PUBLIC_API bool lv_proof_object_verify(const lvProofObject *obj);
/* ---- Proof Trace API ---- */
lv_PUBLIC_API lvProofTrace *lv_proof_trace_create(void);
lv_PUBLIC_API void lv_proof_trace_destroy(lvProofTrace *trace);
lv_PUBLIC_API int lv_proof_trace_add_event(lvProofTrace *trace, lvTraceEvent *event);
lv_PUBLIC_API void lv_proof_trace_start(lvProofTrace *trace, int proof_id);
lv_PUBLIC_API void lv_proof_trace_step(lvProofTrace *trace, int step_id, const char *description, int depth);
lv_PUBLIC_API void lv_proof_trace_backtrack(lvProofTrace *trace, int from_step, int to_step);
lv_PUBLIC_API void lv_proof_trace_branch(lvProofTrace *trace, const char *branch_name, int branch_id, int depth);
lv_PUBLIC_API void lv_proof_trace_lemma(lvProofTrace *trace, int lemma_id, const char *lemma_name);
lv_PUBLIC_API void lv_proof_trace_contradiction(lvProofTrace *trace, lvProofScopeId scope_id, int assumption_count);
lv_PUBLIC_API void lv_proof_trace_complete(lvProofTrace *trace, bool success);
/* ---- Proof Compiler API ---- */
lv_PUBLIC_API lvProofCompiler *lv_proof_compiler_create(const lvCompilerConfig *config);
lv_PUBLIC_API void lv_proof_compiler_destroy(lvProofCompiler *compiler);
lv_PUBLIC_API void lv_proof_compiler_set_config(lvProofCompiler *compiler, const lvCompilerConfig *config);
lv_PUBLIC_API char *lv_proof_compiler_compile(lvProofCompiler *compiler, const lvProofObject *proof,
                                              const lvProofTrace *trace);
lv_PUBLIC_API char *lv_proof_compiler_to_json(const lvProofObject *proof, const lvProofTrace *trace);
lv_PUBLIC_API char *lv_proof_compiler_to_latex(const lvProofObject *proof, const char *language);
lv_PUBLIC_API char *lv_proof_compiler_to_tikz(const lvProofObject *proof);
lv_PUBLIC_API char *lv_proof_compiler_to_text(const lvProofObject *proof, const char *language);
lv_PUBLIC_API char *lv_proof_compiler_to_graphviz(const lvProofObject *proof, const lvProofTrace *trace);
/* ---- 辅助函数 ---- */
lv_PUBLIC_API lvProofStepRecord *lv_proof_step_record_create(void);
lv_PUBLIC_API void lv_proof_step_record_destroy(lvProofStepRecord *record);
lv_PUBLIC_API bool lv_proof_step_record_set_premises(lvProofStepRecord *record, const int *ids, int count);
lv_PUBLIC_API lvTraceEvent *lv_trace_event_create(lvTraceEventType type);
lv_PUBLIC_API void lv_trace_event_destroy(lvTraceEvent *event);
lv_PUBLIC_API lvCompilerConfig lv_compiler_config_default(void);
lv_PUBLIC_API bool lv_proof_export_to_file(const lvProofObject *proof, const lvProofTrace *trace, lvOutputFormat format,
                                           const char *filename);
#endif /* lv_PROOF_COMPILER_H */
