/**
 * @file proof_compiler.h
 * @brief 证明编译层
 *
 * 实现证明对象（Proof Object）、证明跟踪（Proof Trace）和
 * 证明编译器（Proof Compiler），支持多种输出格式。
 *
 * @version 4.0.0
 */
#ifndef LV00_PROOF_COMPILER_H
#define LV00_PROOF_COMPILER_H
#include <stdbool.h>
#include "proof.h"
/* ============== 前向声明 ============== */
typedef struct Lv00ProofObject Lv00ProofObject;
typedef struct Lv00ProofTrace Lv00ProofTrace;
typedef struct Lv00ProofCompiler Lv00ProofCompiler;
/* ============== 证明对象 ============== */
/**
 * @brief 证明步骤记录
 *
 * 记录证明中的每个步骤及其来源。
 */
typedef struct Lv00ProofStepRecord {
    int step_id;                 /**< 步骤ID */
    ProofStepType type;          /**< 步骤类型 */
    ProofColor color;            /**< 证明颜色 */
    int rule_id;                /**< 使用的规则ID */
    char *rule_name;            /**< 规则名称 */

    /* 前提步骤 */
    int *premise_step_ids;      /**< 前提步骤ID数组 */
    int premise_count;          /**< 前提数量 */

    /* 结论 */
    Proposition *conclusion;    /**< 结论命题 */
    int conclusion_id;          /**< 结论ID */

    /* 元数据 */
    int depth;                  /**< 证明深度 */
    char *justification;       /**< 证明理由 */
    int64_t timestamp;         /**< 时间戳 */
} Lv00ProofStepRecord;
/**
 * @brief 证明对象
 *
 * 机器可复核的证明链表示。
 */
struct Lv00ProofObject {
    /* 元数据 */
    int proof_id;               /**< 证明ID */
    char *theorem_name;         /**< 定理名称 */
    Proposition *goal;         /**< 目标命题 */
    bool is_proved;            /**< 是否成功证明 */
    ProofColor final_color;    /**< 最终颜色 */

    /* 证明步骤链 */
    Lv00ProofStepRecord **steps; /**< 步骤数组 */
    int step_count;            /**< 步骤数量 */
    int step_capacity;         /**< 步骤容量 */

    /* 假设和公理 */
    int *axiom_ids;            /**< 使用的公理ID */
    int axiom_count;           /**< 公理数量 */
    int *assumption_ids;       /**< 假设ID数组 */
    int assumption_count;      /**< 假设数量 */

    /* 统计信息 */
    int max_depth;             /**< 最大证明深度 */
    int64_t elapsed_us;       /**< 耗时（微秒） */

    /* 附加数据 */
    void *extra_data;          /**< 附加数据指针 */
};
/* ============== 证明跟踪 ============== */
/**
 * @brief 跟踪事件类型
 */
typedef enum {
    TRACE_EVENT_START,          /**< 证明开始 */
    TRACE_EVENT_STEP,          /**< 步骤执行 */
    TRACE_EVENT_BACKTRACK,      /**< 回溯 */
    TRACE_EVENT_BRANCH,         /**< 分支 */
    TRACE_EVENT_LEMMA,          /**< 引理引用 */
    TRACE_EVENT_ORACLE,         /**< Oracle调用 */
    TRACE_EVENT_CONTRADICTION,  /**< 发现矛盾 */
    TRACE_EVENT_COMPLETE,      /**< 证明完成 */
    TRACE_EVENT_FAIL           /**< 证明失败 */
} Lv00TraceEventType;
/**
 * @brief 跟踪事件
 */
typedef struct Lv00TraceEvent {
    Lv00TraceEventType type;   /**< 事件类型 */
    int step_id;               /**< 关联步骤ID */
    char *description;         /**< 事件描述 */
    char *details;              /**< 详细数据 */
    int depth;                 /**< 当前深度 */
    int64_t timestamp;         /**< 时间戳 */

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
            Lv00ProofScopeId scope_id;
            int assumption_count;
        } contradiction;
    } data;
} Lv00TraceEvent;
/**
 * @brief 证明跟踪
 *
 * 逻辑溯源存档，记录完整的证明过程。
 */
struct Lv00ProofTrace {
    int trace_id;              /**< 跟踪ID */
    int proof_id;              /**< 关联证明ID */

    /* 事件流 */
    Lv00TraceEvent **events;   /**< 事件数组 */
    int event_count;           /**< 事件数量 */
    int event_capacity;        /**< 事件容量 */

    /* 统计 */
    int total_steps;          /**< 总步骤数 */
    int total_backtracks;     /**< 总回溯次数 */
    int max_depth;            /**< 最大深度 */
    int64_t total_time_us;   /**< 总耗时 */

    /* 快照 */
    void *snapshot_data;       /**< 快照数据 */
};
/* ============== 输出格式 ============== */
/**
 * @brief 输出格式类型
 */
typedef enum {
    OUTPUT_FORMAT_JSON,        /**< JSON格式 */
    OUTPUT_FORMAT_LATEX,       /**< LaTeX格式 */
    OUTPUT_FORMAT_TIKZ,        /**< TikZ格式 */
    OUTPUT_FORMAT_TEXT,        /**< 纯文本格式 */
    OUTPUT_FORMAT_XML,         /**< XML格式 */
    OUTPUT_FORMAT_GRAPHVIZ    /**< Graphviz格式 */
} Lv00OutputFormat;
/* ============== 证明编译器 ============== */
/**
 * @brief 编译器配置
 */
typedef struct Lv00CompilerConfig {
    Lv00OutputFormat format;  /**< 输出格式 */
    bool include_metadata;    /**< 包含元数据 */
    bool include_trace;       /**< 包含跟踪信息 */
    bool verbose;             /**< 详细输出 */
    int max_depth;            /**< 最大输出深度 */
    char *language;           /**< 输出语言（zh/en） */
} Lv00CompilerConfig;
/**
 * @brief 证明编译器
 *
 * 将证明对象编译为多种输出格式。
 */
struct Lv00ProofCompiler {
    Lv00CompilerConfig config; /**< 编译配置 */

    /* 输出缓冲区 */
    char *output_buffer;       /**< 输出缓冲区 */
    size_t buffer_size;        /**< 缓冲区大小 */
    size_t buffer_used;        /**< 已使用大小 */
};
/* ============== API 函数声明 ============== */
/* ---- Proof Object API ---- */
LV00_PUBLIC_API Lv00ProofObject *lv00_proof_object_create(void);
LV00_PUBLIC_API void lv00_proof_object_destroy(Lv00ProofObject *obj);
LV00_PUBLIC_API int lv00_proof_object_add_step(Lv00ProofObject *obj, Lv00ProofStepRecord *step);
LV00_PUBLIC_API bool lv00_proof_object_add_axiom(Lv00ProofObject *obj, int axiom_id);
LV00_PUBLIC_API bool lv00_proof_object_add_assumption(Lv00ProofObject *obj, int assumption_id);
LV00_PUBLIC_API int lv00_proof_object_get_step_count(const Lv00ProofObject *obj);
LV00_PUBLIC_API bool lv00_proof_object_is_valid(const Lv00ProofObject *obj);
LV00_PUBLIC_API bool lv00_proof_object_verify(const Lv00ProofObject *obj);
/* ---- Proof Trace API ---- */
LV00_PUBLIC_API Lv00ProofTrace *lv00_proof_trace_create(void);
LV00_PUBLIC_API void lv00_proof_trace_destroy(Lv00ProofTrace *trace);
LV00_PUBLIC_API int lv00_proof_trace_add_event(Lv00ProofTrace *trace, Lv00TraceEvent *event);
LV00_PUBLIC_API void lv00_proof_trace_start(Lv00ProofTrace *trace, int proof_id);
LV00_PUBLIC_API void lv00_proof_trace_step(Lv00ProofTrace *trace, int step_id, const char *description, int depth);
LV00_PUBLIC_API void lv00_proof_trace_backtrack(Lv00ProofTrace *trace, int from_step, int to_step);
LV00_PUBLIC_API void lv00_proof_trace_branch(Lv00ProofTrace *trace, const char *branch_name, int branch_id, int depth);
LV00_PUBLIC_API void lv00_proof_trace_lemma(Lv00ProofTrace *trace, int lemma_id, const char *lemma_name);
LV00_PUBLIC_API void lv00_proof_trace_contradiction(Lv00ProofTrace *trace, Lv00ProofScopeId scope_id, int assumption_count);
LV00_PUBLIC_API void lv00_proof_trace_complete(Lv00ProofTrace *trace, bool success);
/* ---- Proof Compiler API ---- */
LV00_PUBLIC_API Lv00ProofCompiler *lv00_proof_compiler_create(const Lv00CompilerConfig *config);
LV00_PUBLIC_API void lv00_proof_compiler_destroy(Lv00ProofCompiler *compiler);
LV00_PUBLIC_API void lv00_proof_compiler_set_config(Lv00ProofCompiler *compiler, const Lv00CompilerConfig *config);
LV00_PUBLIC_API char *lv00_proof_compiler_compile(Lv00ProofCompiler *compiler, const Lv00ProofObject *proof, const Lv00ProofTrace *trace);
LV00_PUBLIC_API char *lv00_proof_compiler_to_json(const Lv00ProofObject *proof, const Lv00ProofTrace *trace);
LV00_PUBLIC_API char *lv00_proof_compiler_to_latex(const Lv00ProofObject *proof, const char *language);
LV00_PUBLIC_API char *lv00_proof_compiler_to_tikz(const Lv00ProofObject *proof);
LV00_PUBLIC_API char *lv00_proof_compiler_to_text(const Lv00ProofObject *proof, const char *language);
LV00_PUBLIC_API char *lv00_proof_compiler_to_graphviz(const Lv00ProofObject *proof, const Lv00ProofTrace *trace);
/* ---- 辅助函数 ---- */
LV00_PUBLIC_API Lv00ProofStepRecord *lv00_proof_step_record_create(void);
LV00_PUBLIC_API void lv00_proof_step_record_destroy(Lv00ProofStepRecord *record);
LV00_PUBLIC_API Lv00TraceEvent *lv00_trace_event_create(Lv00TraceEventType type);
LV00_PUBLIC_API void lv00_trace_event_destroy(Lv00TraceEvent *event);
LV00_PUBLIC_API Lv00CompilerConfig lv00_compiler_config_default(void);
LV00_PUBLIC_API bool lv00_proof_export_to_file(const Lv00ProofObject *proof, const Lv00ProofTrace *trace, Lv00OutputFormat format, const char *filename);
#endif /* LV00_PROOF_COMPILER_H */
