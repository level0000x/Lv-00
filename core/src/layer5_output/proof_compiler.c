/**
 * @file proof_compiler.c
 * @brief 证明编译层实现
 *
 * @version 4.0.0
 */

#include "proof_compiler.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

#include "lv.h"
#include "lv/lv_internal.h"
#include "circuit_breaker.h"

/* ============== 内部辅助函数 ============== */

/**
 * @brief 确保缓冲区容量
 */
static bool ensure_buffer_capacity(lvProofCompiler *compiler, size_t needed) {
    if (!compiler) return false;
    
    /* 溢出检查：buffer_used + needed 不能超过 SIZE_MAX */
    if (needed > 0 && compiler->buffer_used > SIZE_MAX - needed) {
        return false;
    }
    
    if (compiler->buffer_used + needed <= compiler->buffer_size) {
        return true;
    }
    
    size_t new_size = compiler->buffer_size == 0 ? 4096 : compiler->buffer_size * 2;
    /* 防止 new_size *= 2 无限循环或溢出 */
    while (new_size < compiler->buffer_used + needed) {
        if (new_size > SIZE_MAX / 2) {
            /* 再翻倍会溢出，直接使用最大值 */
            new_size = SIZE_MAX;
            break;
        }
        new_size *= 2;
    }
    
    if (new_size < compiler->buffer_used + needed) {
        return false;
    }
    
    char *new_buffer = (char *)lv_realloc(compiler->output_buffer, new_size);
    if (!new_buffer) return false;
    
    compiler->output_buffer = new_buffer;
    compiler->buffer_size = new_size;
    return true;
}

/**
 * @brief 添加到缓冲区
 */
static void append_to_buffer(lvProofCompiler *compiler, const char *str) {
    if (!compiler || !str) return;
    size_t len = strlen(str);
    if (ensure_buffer_capacity(compiler, len + 1)) {
        snprintf(compiler->output_buffer + compiler->buffer_used, compiler->buffer_size - compiler->buffer_used, "%s", str);
        compiler->buffer_used += len;
    }
}

/**
 * @brief 添加格式化字符串到缓冲区
 */
static void append_format(lvProofCompiler *compiler, const char *fmt, ...) {
    if (!compiler || !fmt) return;
    
    va_list args;
    va_start(args, fmt);
    
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    append_to_buffer(compiler, buffer);
}

/* ============== Proof Object 实现 ============== */

/**
 * @brief 创建证明对象
 */
lvProofObject *lv_proof_object_create(void) {
    lvProofObject *obj = (lvProofObject *)lv_calloc(1, sizeof(lvProofObject));
    if (!obj) return NULL;
    
    obj->step_capacity = 64;
    obj->steps = (lvProofStepRecord **)lv_malloc(
        obj->step_capacity * sizeof(lvProofStepRecord *));
    if (!obj->steps) {
        lv_free((void **)&obj);
        return NULL;
    }
    
    obj->axiom_ids = (int *)lv_malloc(32 * sizeof(int));
    if (!obj->axiom_ids) {
        lv_free((void **)&obj->steps);
        lv_free((void **)&obj);
        return NULL;
    }
    obj->axiom_capacity = 32;
    obj->assumption_ids = (int *)lv_malloc(32 * sizeof(int));
    if (!obj->assumption_ids) {
        lv_free((void **)&obj->axiom_ids);
        lv_free((void **)&obj->steps);
        lv_free((void **)&obj);
        return NULL;
    }
    obj->assumption_capacity = 32;
    
    return obj;
}

/**
 * @brief 销毁证明对象
 */
void lv_proof_object_destroy(lvProofObject *obj) {
    if (!obj) return;
    
    /* 释放所有步骤 */
    for (int i = 0; i < obj->step_count; i++) {
        if (obj->steps[i]) {
            lv_proof_step_record_destroy(obj->steps[i]);
        }
    }
    if (obj->steps) lv_free((void **)&obj->steps);
    if (obj->axiom_ids) lv_free((void **)&obj->axiom_ids);
    if (obj->assumption_ids) lv_free((void **)&obj->assumption_ids);
    if (obj->theorem_name) lv_free((void **)&obj->theorem_name);
    if (obj->goal) proposition_unref(obj->goal);
    
    lv_free((void **)&obj);
}

/**
 * @brief 添加证明步骤
 */
int lv_proof_object_add_step(lvProofObject *obj, lvProofStepRecord *step) {
    if (!obj || !step) return -1;
    
    /* 确保容量 */
    if (obj->step_count >= obj->step_capacity) {
        if (obj->step_capacity > INT_MAX / 2) return -1;
        int new_capacity = obj->step_capacity * 2;
        lvProofStepRecord **new_steps = (lvProofStepRecord **)lv_realloc(
            obj->steps, new_capacity * sizeof(lvProofStepRecord *));
        if (!new_steps) return -1;
        obj->steps = new_steps;
        obj->step_capacity = new_capacity;
    }
    
    step->step_id = obj->step_count;
    obj->steps[obj->step_count++] = step;
    
    if (step->depth > obj->max_depth) {
        obj->max_depth = step->depth;
    }
    
    return step->step_id;
}

/**
 * @brief 添加公理引用
 */
bool lv_proof_object_add_axiom(lvProofObject *obj, int axiom_id) {
    if (!obj) return false;
    if (obj->axiom_count >= obj->axiom_capacity) {
        if (obj->axiom_capacity > INT_MAX / 2) return false;
        int new_capacity = obj->axiom_capacity * 2;
        if ((size_t)new_capacity > SIZE_MAX / sizeof(int)) return false;
        int *new_ids = (int *)lv_realloc(obj->axiom_ids, (size_t)new_capacity * sizeof(int));
        if (!new_ids) return false;
        obj->axiom_ids = new_ids;
        obj->axiom_capacity = new_capacity;
    }
    obj->axiom_ids[obj->axiom_count++] = axiom_id;
    return true;
}

/**
 * @brief 添加假设引用
 */
bool lv_proof_object_add_assumption(lvProofObject *obj, int assumption_id) {
    if (!obj) return false;
    if (obj->assumption_count >= obj->assumption_capacity) {
        if (obj->assumption_capacity > INT_MAX / 2) return false;
        int new_capacity = obj->assumption_capacity * 2;
        if ((size_t)new_capacity > SIZE_MAX / sizeof(int)) return false;
        int *new_ids = (int *)lv_realloc(obj->assumption_ids, (size_t)new_capacity * sizeof(int));
        if (!new_ids) return false;
        obj->assumption_ids = new_ids;
        obj->assumption_capacity = new_capacity;
    }
    obj->assumption_ids[obj->assumption_count++] = assumption_id;
    return true;
}

/**
 * @brief 获取证明步骤数
 */
int lv_proof_object_get_step_count(const lvProofObject *obj) {
    return obj ? obj->step_count : 0;
}

/**
 * @brief 检查证明是否有效
 */
bool lv_proof_object_is_valid(const lvProofObject *obj) {
    if (!obj) return false;
    if (!obj->is_proved) return false;
    if (obj->step_count == 0) return false;
    if (!obj->goal) return false;
    
    /* 最后一步的结论应该是目标 */
    lvProofStepRecord *last = obj->steps[obj->step_count - 1];
    if (!last || !last->conclusion) return false;
    
    /* 检查最后一步是否与目标匹配 */
    return last->conclusion_id == obj->goal->id;
}

/**
 * @brief 验证证明链的每一步
 */
bool lv_proof_object_verify(const lvProofObject *obj) {
    if (!obj || !lv_proof_object_is_valid(obj)) return false;
    
    /* 验证每一步的前提都存在 */
    for (int i = 0; i < obj->step_count; i++) {
        lvProofStepRecord *step = obj->steps[i];
        
        /* 检查前提步骤是否有效 */
        for (int j = 0; j < step->premise_count; j++) {
            int premise_id = step->premise_step_ids[j];
            if (premise_id < 0 || premise_id >= i) {
                /* 前提不能是未来步骤 */
                return false;
            }
        }
    }
    
    return true;
}

/* ============== Proof Step Record 实现 ============== */

/**
 * @brief 创建步骤记录
 */
lvProofStepRecord *lv_proof_step_record_create(void) {
    lvProofStepRecord *record = (lvProofStepRecord *)lv_calloc(
        1, sizeof(lvProofStepRecord));
    if (!record) return NULL;
    record->premise_step_ids = (int *)lv_malloc(8 * sizeof(int));
    if (!record->premise_step_ids) {
        lv_free((void **) &record);
        return NULL;
    }
    record->premise_capacity = 8;

    return record;
}

/**
 * @brief 销毁步骤记录
 */
void lv_proof_step_record_destroy(lvProofStepRecord *record) {
    if (!record) return;
    if (record->rule_name) lv_free((void **)&record->rule_name);
    if (record->premise_step_ids) lv_free((void **)&record->premise_step_ids);
    if (record->conclusion) proposition_unref(record->conclusion);
    if (record->justification) lv_free((void **)&record->justification);
    lv_free((void **)&record);
}

/* ============== Proof Compiler 实现 ============== */

/**
 * @brief 创建证明编译器
 */
lvProofCompiler *lv_proof_compiler_create(const lvCompilerConfig *config) {
    lvProofCompiler *compiler = (lvProofCompiler *)lv_calloc(
        1, sizeof(lvProofCompiler));
    if (!compiler) return NULL;
    
    if (config) {
        compiler->config = *config;
    } else {
        compiler->config = lv_compiler_config_default();
    }
    
    compiler->output_buffer = (char *)lv_malloc(4096);
    if (!compiler->output_buffer) {
        lv_free((void **)&compiler);
        return NULL;
    }
    compiler->buffer_size = 4096;
    compiler->buffer_used = 0;
    compiler->output_buffer[0] = '\0';
    
    return compiler;
}

/**
 * @brief 销毁证明编译器
 */
void lv_proof_compiler_destroy(lvProofCompiler *compiler) {
    if (!compiler) return;
    if (compiler->output_buffer) lv_free((void **)&compiler->output_buffer);
    lv_free((void **)&compiler);
}

/**
 * @brief 设置编译配置
 */
void lv_proof_compiler_set_config(lvProofCompiler *compiler,
                                     const lvCompilerConfig *config) {
    if (!compiler || !config) return;
    compiler->config = *config;
}

/**
 * @brief 获取事件类型名称
 */
static const char *get_event_type_name(lvTraceEventType type) {
    switch (type) {
        case TRACE_EVENT_START: return "开始";
        case TRACE_EVENT_STEP: return "步骤";
        case TRACE_EVENT_BACKTRACK: return "回溯";
        case TRACE_EVENT_BRANCH: return "分支";
        case TRACE_EVENT_LEMMA: return "引理";
        case TRACE_EVENT_ORACLE: return "Oracle";
        case TRACE_EVENT_CONTRADICTION: return "矛盾";
        case TRACE_EVENT_COMPLETE: return "完成";
        case TRACE_EVENT_FAIL: return "失败";
        default: return "未知";
    }
}

/**
 * @brief 编译为JSON格式
 */
char *lv_proof_compiler_to_json(const lvProofObject *proof,
                                   const lvProofTrace *trace) {
    lv_UNUSED(trace);
    if (!proof) return NULL;

    /* 动态缓冲区：初始 4096，溢出时翻倍 */
    size_t buffer_size = 4096;
    size_t offset = 0;
    char *buffer = (char *)lv_malloc(buffer_size);
    if (!buffer) return NULL;

    /* 辅助宏：确保容量并写入 */
    #define JSON_ENSURE(needed) do { \
        while (offset + (needed) >= buffer_size) { \
            buffer_size *= 2; \
            char *_nb = (char *)lv_realloc(buffer, buffer_size); \
            if (!_nb) { lv_free((void **)&buffer); return NULL; } \
            buffer = _nb; \
        } \
    } while(0)

    #define JSON_WRITE(fmt, ...) do { \
        JSON_ENSURE(256); \
        offset += snprintf(buffer + offset, buffer_size - offset, fmt, ##__VA_ARGS__); \
    } while(0)

    JSON_WRITE("{\n");
    JSON_WRITE("  \"proof_id\": %d,\n", proof->proof_id);
    JSON_WRITE("  \"theorem_name\": \"%s\",\n",
        proof->theorem_name ? proof->theorem_name : "unknown");
    JSON_WRITE("  \"is_proved\": %s,\n", proof->is_proved ? "true" : "false");
    JSON_WRITE("  \"final_color\": %d,\n", proof->final_color);
    JSON_WRITE("  \"step_count\": %d,\n", proof->step_count);
    JSON_WRITE("  \"max_depth\": %d,\n", proof->max_depth);
    JSON_WRITE("  \"axiom_count\": %d,\n", proof->axiom_count);
    JSON_WRITE("  \"assumption_count\": %d,\n", proof->assumption_count);
    JSON_WRITE("  \"elapsed_us\": %lld,\n", (long long)proof->elapsed_us);

    /* 步骤数组 */
    JSON_WRITE("  \"steps\": [\n");
    for (int i = 0; i < proof->step_count; i++) {
        lvProofStepRecord *step = proof->steps[i];
        JSON_ENSURE(256);
        JSON_WRITE("    {\"id\": %d, \"type\": %d, \"depth\": %d}",
            step->step_id, step->type, step->depth);
        if (i < proof->step_count - 1) {
            JSON_WRITE(",");
        }
        JSON_WRITE("\n");
    }
    JSON_WRITE("  ]\n");

    JSON_WRITE("}\n");

    #undef JSON_ENSURE
    #undef JSON_WRITE

    return buffer;
}

/**
 * @brief 编译为LaTeX格式
 */
char *lv_proof_compiler_to_latex(const lvProofObject *proof,
                                    const char *language) {
    if (!proof) return NULL;
    
    size_t buffer_size = 16384;
    char *buffer = (char *)lv_malloc(buffer_size);
    if (!buffer) return NULL;
    
    size_t offset = 0;

    /* 辅助宏：确保容量并写入 */
    #define LATEX_ENSURE(needed) do { \
        while (offset + (needed) >= buffer_size) { \
            buffer_size *= 2; \
            char *_nb = (char *)lv_realloc(buffer, buffer_size); \
            if (!_nb) { lv_free((void **)&buffer); return NULL; } \
            buffer = _nb; \
        } \
    } while(0)

    #define LATEX_WRITE(fmt, ...) do { \
        LATEX_ENSURE(256); \
        offset += snprintf(buffer + offset, buffer_size - offset, fmt, ##__VA_ARGS__); \
    } while(0)

    const char *lang = language ? language : "zh";
    const char *proof_begin = strcmp(lang, "en") == 0 ? "Proof" : "证明";
    const char *qed = strcmp(lang, "en") == 0 ? "\\qed" : "证毕";

    LATEX_WRITE("\\begin{Proof}\n");
    LATEX_WRITE("%s.\n\n", proof_begin);
    
    /* 生成步骤 */
    for (int i = 0; i < proof->step_count; i++) {
        lvProofStepRecord *step = proof->steps[i];
        
        /* 缩进 */
        for (int d = 0; d < step->depth; d++) {
            LATEX_WRITE("  ");
        }
        
        const char *rule_name = step->rule_name ? step->rule_name : "规则";
        LATEX_WRITE("由 %s 可得", rule_name);
        
        if (step->conclusion && step->conclusion->label) {
            LATEX_WRITE(" $%s$.\n", step->conclusion->label);
        } else {
            LATEX_WRITE("。\n");
        }
    }
    
    LATEX_WRITE("\n%s\n", qed);
    LATEX_WRITE("\\end{Proof}\n");

    #undef LATEX_ENSURE
    #undef LATEX_WRITE
    
    return buffer;
}

/**
 * @brief 编译为TikZ格式
 */
char *lv_proof_compiler_to_tikz(const lvProofObject *proof) {
    if (!proof) return NULL;
    
    size_t buffer_size = 16384;
    char *buffer = (char *)lv_malloc(buffer_size);
    if (!buffer) return NULL;
    
    size_t offset = 0;

    /* 辅助宏：确保容量并写入 */
    #define TIKZ_ENSURE(needed) do { \
        while (offset + (needed) >= buffer_size) { \
            buffer_size *= 2; \
            char *_nb = (char *)lv_realloc(buffer, buffer_size); \
            if (!_nb) { lv_free((void **)&buffer); return NULL; } \
            buffer = _nb; \
        } \
    } while(0)

    #define TIKZ_WRITE(fmt, ...) do { \
        TIKZ_ENSURE(256); \
        offset += snprintf(buffer + offset, buffer_size - offset, fmt, ##__VA_ARGS__); \
    } while(0)

    TIKZ_WRITE("\\begin{tikzpicture}[node distance=2cm]\n");
    TIKZ_WRITE("\\tikzstyle{step}=[circle,draw,minimum size=1cm]\n");
    TIKZ_WRITE("\\tikzstyle{arrow}=[->,>=stealth]\n");
    
    /* 生成节点 */
    for (int i = 0; i < proof->step_count; i++) {
        lvProofStepRecord *step = proof->steps[i];
        TIKZ_WRITE("\\node[step] (S%d) at (%d, %d) {$S_%d$};\n",
            step->step_id, step->step_id % 3, -step->depth, step->step_id);
    }
    
    /* 生成边 */
    for (int i = 0; i < proof->step_count; i++) {
        lvProofStepRecord *step = proof->steps[i];
        for (int j = 0; j < step->premise_count; j++) {
            int premise_id = step->premise_step_ids[j];
            TIKZ_WRITE("\\draw[arrow] (S%d) -- (S%d);\n",
                premise_id, step->step_id);
        }
    }
    
    TIKZ_WRITE("\\end{tikzpicture}\n");

    #undef TIKZ_ENSURE
    #undef TIKZ_WRITE
    
    return buffer;
}

/**
 * @brief 编译为纯文本格式
 */
char *lv_proof_compiler_to_text(const lvProofObject *proof,
                                   const char *language) {
    if (!proof) return NULL;
    
    size_t buffer_size = 16384;
    char *buffer = (char *)lv_malloc(buffer_size);
    if (!buffer) return NULL;
    
    size_t offset = 0;

    /* 辅助宏：确保容量并写入 */
    #define TEXT_ENSURE(needed) do { \
        while (offset + (needed) >= buffer_size) { \
            buffer_size *= 2; \
            char *_nb = (char *)lv_realloc(buffer, buffer_size); \
            if (!_nb) { lv_free((void **)&buffer); return NULL; } \
            buffer = _nb; \
        } \
    } while(0)

    #define TEXT_WRITE(fmt, ...) do { \
        TEXT_ENSURE(256); \
        offset += snprintf(buffer + offset, buffer_size - offset, fmt, ##__VA_ARGS__); \
    } while(0)

    const char *lang = language ? language : "zh";
    const char *proof_begin = strcmp(lang, "en") == 0 ? "Proof" : "证明";
    
    TEXT_WRITE("=== %s ===\n\n", proof_begin);
    
    if (proof->theorem_name) {
        TEXT_WRITE("定理: %s\n\n", proof->theorem_name);
    }
    
    /* 生成步骤 */
    for (int i = 0; i < proof->step_count; i++) {
        lvProofStepRecord *step = proof->steps[i];
        
        /* 缩进 */
        for (int d = 0; d < step->depth; d++) {
            TEXT_WRITE("  ");
        }
        
        /* 步骤编号 */
        TEXT_WRITE("[%d] ", step->step_id);
        
        /* 规则名称 */
        const char *rule_name = step->rule_name ? step->rule_name : "规则";
        TEXT_WRITE("由 %s", rule_name);
        
        /* 前提 */
        if (step->premise_count > 0) {
            TEXT_WRITE(" (前提: ");
            for (int j = 0; j < step->premise_count; j++) {
                if (j > 0) TEXT_WRITE(", ");
                TEXT_WRITE("%d", step->premise_step_ids[j]);
            }
            TEXT_WRITE(")");
        }
        
        /* 结论 */
        if (step->conclusion && step->conclusion->label) {
            TEXT_WRITE(" 可得 %s", step->conclusion->label);
        }
        
        TEXT_WRITE("\n");
    }
    
    /* 统计 */
    TEXT_WRITE("\n--- 统计 ---\n");
    TEXT_WRITE("总步骤: %d\n", proof->step_count);
    TEXT_WRITE("最大深度: %d\n", proof->max_depth);
    TEXT_WRITE("使用公理: %d\n", proof->axiom_count);
    TEXT_WRITE("假设数量: %d\n", proof->assumption_count);

    #undef TEXT_ENSURE
    #undef TEXT_WRITE
    
    return buffer;
}

/**
 * @brief 编译为Graphviz格式
 */
char *lv_proof_compiler_to_graphviz(const lvProofObject *proof,
                                       const lvProofTrace *trace) {
    lv_UNUSED(trace);
    if (!proof) return NULL;
    
    size_t buffer_size = 16384;
    char *buffer = (char *)lv_malloc(buffer_size);
    if (!buffer) return NULL;
    
    size_t offset = 0;

    /* 辅助宏：确保容量并写入 */
    #define GRAPHVIZ_ENSURE(needed) do { \
        while (offset + (needed) >= buffer_size) { \
            buffer_size *= 2; \
            char *_nb = (char *)lv_realloc(buffer, buffer_size); \
            if (!_nb) { lv_free((void **)&buffer); return NULL; } \
            buffer = _nb; \
        } \
    } while(0)

    #define GRAPHVIZ_WRITE(fmt, ...) do { \
        GRAPHVIZ_ENSURE(256); \
        offset += snprintf(buffer + offset, buffer_size - offset, fmt, ##__VA_ARGS__); \
    } while(0)

    GRAPHVIZ_WRITE("digraph ProofTree {\n");
    GRAPHVIZ_WRITE("  rankdir=TB;\n");
    GRAPHVIZ_WRITE("  node [shape=box];\n");
    
    /* 生成节点 */
    for (int i = 0; i < proof->step_count; i++) {
        lvProofStepRecord *step = proof->steps[i];
        const char *label = step->conclusion && step->conclusion->label ?
            step->conclusion->label : "?";
        const char *color = step->color == PROOF_COLOR_GREEN ? "lightgreen" :
            step->color == PROOF_COLOR_ORANGE_EX_FALSO ? "orange" : "lightblue";
        
        GRAPHVIZ_WRITE("  S%d [label=\"%s\", style=filled, fillcolor=%s];\n",
            step->step_id, label, color);
    }
    
    /* 生成边 */
    for (int i = 0; i < proof->step_count; i++) {
        lvProofStepRecord *step = proof->steps[i];
        for (int j = 0; j < step->premise_count; j++) {
            int premise_id = step->premise_step_ids[j];
            const char *rule_name = step->rule_name ? step->rule_name : "";
            GRAPHVIZ_WRITE("  S%d -> S%d [label=\"%s\"];\n",
                premise_id, step->step_id, rule_name);
        }
    }
    
    GRAPHVIZ_WRITE("}\n");

    #undef GRAPHVIZ_ENSURE
    #undef GRAPHVIZ_WRITE
    
    return buffer;
}

/**
 * @brief 编译证明对象为字符串
 */
char *lv_proof_compiler_compile(lvProofCompiler *compiler,
                                   const lvProofObject *proof,
                                   const lvProofTrace *trace) {
    if (!compiler || !proof) return NULL;
    
    /* 清空缓冲区 */
    compiler->buffer_used = 0;
    
    switch (compiler->config.format) {
        case OUTPUT_FORMAT_JSON:
            return lv_proof_compiler_to_json(proof, trace);
        case OUTPUT_FORMAT_LATEX:
            return lv_proof_compiler_to_latex(proof, compiler->config.language);
        case OUTPUT_FORMAT_TIKZ:
            return lv_proof_compiler_to_tikz(proof);
        case OUTPUT_FORMAT_TEXT:
            return lv_proof_compiler_to_text(proof, compiler->config.language);
        case OUTPUT_FORMAT_GRAPHVIZ:
            return lv_proof_compiler_to_graphviz(proof, trace);
        default:
            return lv_proof_compiler_to_text(proof, compiler->config.language);
    }
}

/**
 * @brief 创建默认编译器配置
 */
lvCompilerConfig lv_compiler_config_default(void) {
    lvCompilerConfig config;
    config.format = OUTPUT_FORMAT_TEXT;
    config.include_metadata = true;
    config.include_trace = true;
    config.verbose = false;
    config.max_depth = lv_DEFAULT_MAX_DEPTH;
    config.language = "zh";
    return config;
}

/**
 * @brief 导出证明对象到文件
 */
bool lv_proof_export_to_file(const lvProofObject *proof,
                                const lvProofTrace *trace,
                                lvOutputFormat format,
                                const char *filename) {
    if (!proof || !filename) return false;
    
    lvCompilerConfig config = lv_compiler_config_default();
    config.format = format;
    
    lvProofCompiler *compiler = lv_proof_compiler_create(&config);
    if (!compiler) return false;
    
    char *content = lv_proof_compiler_compile(compiler, proof, trace);
    lv_proof_compiler_destroy(compiler);
    
    if (!content) return false;
    
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        lv_free((void **)&content);
        return false;
    }
    
    fputs(content, fp);
    fclose(fp);
    lv_free((void **)&content);
    
    return true;
}
