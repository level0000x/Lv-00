/**
 * @file proof_compiler.c
 * @brief 证明编译层实现
 *
 * @version 4.0.0
 */

#include "proof_compiler.h"

#include "lv/lv_file.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


#include "lv/lv_internal.h"


#include "circuit_breaker.h"
#include "lv.h"
#include "lv_utils.h"

/* ============== 共享缓冲区宏 ============== */

#define BUF_ENSURE(buf, buf_len, buf_cap, needed) do { \
    if ((buf_len) + (needed) + 1 > (buf_cap)) { \
        size_t _new_cap = (buf_cap) ? (buf_cap) * 2 : 4096; \
        while (_new_cap < (buf_len) + (needed) + 1) _new_cap *= 2; \
        char *_nb = realloc((buf), _new_cap); \
        if (!_nb) return NULL; \
        (buf) = _nb; \
        (buf_cap) = _new_cap; \
    } \
} while(0)

#define BUF_WRITE(buf, buf_len, buf_cap, ...) do { \
    int _n = snprintf((buf) + (buf_len), (buf_cap) - (buf_len), __VA_ARGS__); \
    if (_n < 0) return NULL; \
    (buf_len) += (size_t)_n; \
} while(0)

/* ============== 内部辅助函数 ============== */

/**
 * @brief 确保缓冲区容量
 */
static bool ensure_buffer_capacity(lvProofCompiler *compiler, size_t needed) {
    if (!compiler)
        return false;

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

    char *new_buffer = (char *) lv_realloc(compiler->output_buffer, new_size);
    if (!new_buffer)
        return false;

    compiler->output_buffer = new_buffer;
    compiler->buffer_size = new_size;
    return true;
}

/**
 * @brief 添加到缓冲区
 */
static void append_to_buffer(lvProofCompiler *compiler, const char *str) {
    if (!compiler || !str)
        return;
    size_t len = strlen(str);
    if (ensure_buffer_capacity(compiler, len + 1)) {
        snprintf(compiler->output_buffer + compiler->buffer_used, compiler->buffer_size - compiler->buffer_used, "%s",
                 str);
        compiler->buffer_used += len;
    }
}

/* ============== Proof Object 实现 ============== */

/**
 * @brief 创建证明对象
 */
lvProofObject *lv_proof_object_create(void) {
    lvProofObject *obj = (lvProofObject *) lv_calloc(1, sizeof(lvProofObject));
    if (!obj)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_proof_object_create: calloc failed");

    obj->step_capacity = 64;
    obj->steps = (lvProofStepRecord **) lv_malloc(obj->step_capacity * sizeof(lvProofStepRecord *));
    if (!obj->steps) {
        lv_free((void **) &obj);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_proof_object_create: steps malloc failed");
    }

    obj->axiom_ids = (int *) lv_malloc(32 * sizeof(int));
    if (!obj->axiom_ids) {
        lv_free((void **) &obj->steps);
        lv_free((void **) &obj);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_proof_object_create: axiom_ids malloc failed");
    }
    obj->axiom_capacity = 32;
    obj->assumption_ids = (int *) lv_malloc(32 * sizeof(int));
    if (!obj->assumption_ids) {
        lv_free((void **) &obj->axiom_ids);
        lv_free((void **) &obj->steps);
        lv_free((void **) &obj);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_proof_object_create: assumption_ids malloc failed");
    }
    obj->assumption_capacity = 32;

    return obj;
}

/**
 * @brief 销毁证明对象
 */
void lv_proof_object_destroy(lvProofObject *obj) {
    if (!obj)
        return;

    /* 释放所有步骤 */
    for (int i = 0; i < obj->step_count; i++) {
        if (obj->steps[i]) {
            lv_proof_step_record_destroy(obj->steps[i]);
        }
    }
    if (obj->steps)
        lv_free((void **) &obj->steps);
    if (obj->axiom_ids)
        lv_free((void **) &obj->axiom_ids);
    if (obj->assumption_ids)
        lv_free((void **) &obj->assumption_ids);
    if (obj->theorem_name)
        lv_free((void **) &obj->theorem_name);
    if (obj->goal)
        proposition_unref(obj->goal);

    lv_free((void **) &obj);
}

/**
 * @brief 添加证明步骤
 */
int lv_proof_object_add_step(lvProofObject *obj, lvProofStepRecord *step) {
    if (!obj || !step)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "lv_proof_object_add_step: obj or step is NULL");

    /* 确保容量 */
    if (!lv_ensure_capacity((void **)&obj->steps, obj->step_count,
                            &obj->step_capacity, sizeof(lvProofStepRecord *), 1))
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_proof_object_add_step: ensure_capacity failed");

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
    if (!obj)
        return false;
    if (!lv_ensure_capacity((void **)&obj->axiom_ids, obj->axiom_count,
                            &obj->axiom_capacity, sizeof(int), 1))
        return false;
    obj->axiom_ids[obj->axiom_count++] = axiom_id;
    return true;
}

/**
 * @brief 添加假设引用
 */
bool lv_proof_object_add_assumption(lvProofObject *obj, int assumption_id) {
    if (!obj)
        return false;
    if (!lv_ensure_capacity((void **)&obj->assumption_ids, obj->assumption_count,
                            &obj->assumption_capacity, sizeof(int), 1))
        return false;
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
    if (!obj)
        return false;
    if (!obj->is_proved)
        return false;
    if (obj->step_count == 0)
        return false;
    if (!obj->goal)
        return false;

    /* 最后一步的结论应该是目标 */
    lvProofStepRecord *last = obj->steps[obj->step_count - 1];
    if (!last || !last->conclusion)
        return false;

    /* 检查最后一步是否与目标匹配 */
    return last->conclusion_id == obj->goal->id;
}

/**
 * @brief 验证证明链的每一步
 */
bool lv_proof_object_verify(const lvProofObject *obj) {
    if (!obj || !lv_proof_object_is_valid(obj))
        return false;

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
    lvProofStepRecord *record = (lvProofStepRecord *) lv_calloc(1, sizeof(lvProofStepRecord));
    if (!record)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_proof_step_record_create: calloc failed");
    record->premise_step_ids = (int *) lv_malloc(8 * sizeof(int));
    if (!record->premise_step_ids) {
        lv_free((void **) &record);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_proof_step_record_create: premise_step_ids malloc failed");
    }
    record->premise_capacity = 8;

    return record;
}

/**
 * @brief 销毁步骤记录
 */
void lv_proof_step_record_destroy(lvProofStepRecord *record) {
    if (!record)
        return;
    if (record->rule_name)
        lv_free((void **) &record->rule_name);
    if (record->premise_step_ids)
        lv_free((void **) &record->premise_step_ids);
    if (record->conclusion)
        proposition_unref(record->conclusion);
    if (record->justification)
        lv_free((void **) &record->justification);
    lv_free((void **) &record);
}

/* ============== Proof Compiler 实现 ============== */

/**
 * @brief 创建证明编译器
 */
lvProofCompiler *lv_proof_compiler_create(const lvCompilerConfig *config) {
    lvProofCompiler *compiler = (lvProofCompiler *) lv_calloc(1, sizeof(lvProofCompiler));
    if (!compiler)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_proof_compiler_create: calloc failed");

    if (config) {
        compiler->config = *config;
    } else {
        compiler->config = lv_compiler_config_default();
    }

    compiler->output_buffer = (char *) lv_malloc(4096);
    if (!compiler->output_buffer) {
        lv_free((void **) &compiler);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_proof_compiler_create: output_buffer malloc failed");
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
    if (!compiler)
        return;
    if (compiler->output_buffer)
        lv_free((void **) &compiler->output_buffer);
    lv_free((void **) &compiler);
}

/**
 * @brief 设置编译配置
 */
void lv_proof_compiler_set_config(lvProofCompiler *compiler, const lvCompilerConfig *config) {
    if (!compiler || !config)
        return;
    compiler->config = *config;
}

/**
 * @brief 获取事件类型名称
 */
/* ================================================================
 * 枚举 -> 名称 映射表（数据表化，替代 switch）
 * ================================================================ */

/** @brief 枚举值 -> 名称 映射项（表必须按 code 升序排列） */
typedef struct {
    int code;         /**< 枚举值 */
    const char *name; /**< 名称字符串 */
} pcomp_NameEntry;

/** @brief 二分查找枚举名称（表需按 code 升序） */
static const char *pcomp_name_lookup(const pcomp_NameEntry *table, size_t count, int code) {
    size_t lo = 0, hi = count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (table[mid].code == code)
            return table[mid].name;
        if (table[mid].code < code)
            lo = mid + 1;
        else
            hi = mid;
    }
    return NULL;
}

/** @brief get_event_type_name 名称表（按枚举值升序） */
static const pcomp_NameEntry s_get_event_type_name_entries[] = {
    {TRACE_EVENT_START, "开始"},
    {TRACE_EVENT_STEP, "步骤"},
    {TRACE_EVENT_BACKTRACK, "回溯"},
    {TRACE_EVENT_BRANCH, "分支"},
    {TRACE_EVENT_LEMMA, "引理"},
    {TRACE_EVENT_ORACLE, "Oracle"},
    {TRACE_EVENT_CONTRADICTION, "矛盾"},
    {TRACE_EVENT_COMPLETE, "完成"},
    {TRACE_EVENT_FAIL, "失败"},
};

static const char *get_event_type_name(lvTraceEventType type) {
    const char *name = pcomp_name_lookup(s_get_event_type_name_entries, lv_ARRAY_SIZE(s_get_event_type_name_entries), (int) type);
    return name ? name : "未知";
}

/**
 * @brief 编译为JSON格式
 */
char *lv_proof_compiler_to_json(const lvProofObject *proof, const lvProofTrace *trace) {
    lv_UNUSED(trace);
    if (!proof)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_proof_compiler_to_json: proof is NULL");

    /* 动态缓冲区：初始 4096，溢出时翻倍 */
    size_t buf_cap = 4096;
    size_t buf_len = 0;
    char *buf = (char *) lv_malloc(buf_cap);
    if (!buf)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_proof_compiler_to_json: malloc failed");

    BUF_WRITE(buf, buf_len, buf_cap, "{\n");
    BUF_WRITE(buf, buf_len, buf_cap, "  \"proof_id\": %d,\n", proof->proof_id);
    BUF_WRITE(buf, buf_len, buf_cap, "  \"theorem_name\": \"%s\",\n", proof->theorem_name ? proof->theorem_name : "unknown");
    BUF_WRITE(buf, buf_len, buf_cap, "  \"is_proved\": %s,\n", proof->is_proved ? "true" : "false");
    BUF_WRITE(buf, buf_len, buf_cap, "  \"final_color\": %d,\n", proof->final_color);
    BUF_WRITE(buf, buf_len, buf_cap, "  \"step_count\": %d,\n", proof->step_count);
    BUF_WRITE(buf, buf_len, buf_cap, "  \"max_depth\": %d,\n", proof->max_depth);
    BUF_WRITE(buf, buf_len, buf_cap, "  \"axiom_count\": %d,\n", proof->axiom_count);
    BUF_WRITE(buf, buf_len, buf_cap, "  \"assumption_count\": %d,\n", proof->assumption_count);
    BUF_WRITE(buf, buf_len, buf_cap, "  \"elapsed_us\": %lld,\n", (long long) proof->elapsed_us);

    /* 步骤数组 */
    BUF_WRITE(buf, buf_len, buf_cap, "  \"steps\": [\n");
    for (int i = 0; i < proof->step_count; i++) {
        lvProofStepRecord *step = proof->steps[i];
        BUF_ENSURE(buf, buf_len, buf_cap, 256);
        BUF_WRITE(buf, buf_len, buf_cap, "    {\"id\": %d, \"type\": %d, \"depth\": %d}", step->step_id, step->type, step->depth);
        if (i < proof->step_count - 1) {
            BUF_WRITE(buf, buf_len, buf_cap, ",");
        }
        BUF_WRITE(buf, buf_len, buf_cap, "\n");
    }
    BUF_WRITE(buf, buf_len, buf_cap, "  ]\n");

    BUF_WRITE(buf, buf_len, buf_cap, "}\n");

    return buf;
}

/**
 * @brief 编译为LaTeX格式
 */
char *lv_proof_compiler_to_latex(const lvProofObject *proof, const char *language) {
    if (!proof)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_proof_compiler_to_latex: proof is NULL");

    size_t buf_cap = 16384;
    char *buf = (char *) lv_malloc(buf_cap);
    if (!buf)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_proof_compiler_to_latex: malloc failed");

    size_t buf_len = 0;

    const char *lang = language ? language : "zh";
    const char *proof_begin = strcmp(lang, "en") == 0 ? "Proof" : "证明";
    const char *qed = strcmp(lang, "en") == 0 ? "\\qed" : "证毕";

    BUF_WRITE(buf, buf_len, buf_cap, "\\begin{Proof}\n");
    BUF_WRITE(buf, buf_len, buf_cap, "%s.\n\n", proof_begin);

    /* 生成步骤 */
    for (int i = 0; i < proof->step_count; i++) {
        lvProofStepRecord *step = proof->steps[i];

        /* 缩进 */
        for (int d = 0; d < step->depth; d++) {
            BUF_WRITE(buf, buf_len, buf_cap, "  ");
        }

        const char *rule_name = step->rule_name ? step->rule_name : "规则";
        BUF_WRITE(buf, buf_len, buf_cap, "由 %s 可得", rule_name);

        if (step->conclusion && step->conclusion->label) {
            BUF_WRITE(buf, buf_len, buf_cap, " $%s$.\n", step->conclusion->label);
        } else {
            BUF_WRITE(buf, buf_len, buf_cap, "。\n");
        }
    }

    BUF_WRITE(buf, buf_len, buf_cap, "\n%s\n", qed);
    BUF_WRITE(buf, buf_len, buf_cap, "\\end{Proof}\n");

    return buf;
}

/**
 * @brief 编译为TikZ格式
 */
char *lv_proof_compiler_to_tikz(const lvProofObject *proof) {
    if (!proof)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_proof_compiler_to_tikz: proof is NULL");

    size_t buf_cap = 16384;
    char *buf = (char *) lv_malloc(buf_cap);
    if (!buf)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_proof_compiler_to_tikz: malloc failed");

    size_t buf_len = 0;

    BUF_WRITE(buf, buf_len, buf_cap, "\\begin{tikzpicture}[node distance=2cm]\n");
    BUF_WRITE(buf, buf_len, buf_cap, "\\tikzstyle{step}=[circle,draw,minimum size=1cm]\n");
    BUF_WRITE(buf, buf_len, buf_cap, "\\tikzstyle{arrow}=[->,>=stealth]\n");

    /* 生成节点 */
    for (int i = 0; i < proof->step_count; i++) {
        lvProofStepRecord *step = proof->steps[i];
        BUF_WRITE(buf, buf_len, buf_cap, "\\node[step] (S%d) at (%d, %d) {$S_%d$};\n", step->step_id, step->step_id % 3, -step->depth,
                   step->step_id);
    }

    /* 生成边 */
    for (int i = 0; i < proof->step_count; i++) {
        lvProofStepRecord *step = proof->steps[i];
        for (int j = 0; j < step->premise_count; j++) {
            int premise_id = step->premise_step_ids[j];
            BUF_WRITE(buf, buf_len, buf_cap, "\\draw[arrow] (S%d) -- (S%d);\n", premise_id, step->step_id);
        }
    }

    BUF_WRITE(buf, buf_len, buf_cap, "\\end{tikzpicture}\n");

    return buf;
}

/**
 * @brief 编译为纯文本格式
 */
char *lv_proof_compiler_to_text(const lvProofObject *proof, const char *language) {
    if (!proof)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_proof_compiler_to_text: proof is NULL");

    size_t buf_cap = 16384;
    char *buf = (char *) lv_malloc(buf_cap);
    if (!buf)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_proof_compiler_to_text: malloc failed");

    size_t buf_len = 0;

    const char *lang = language ? language : "zh";
    const char *proof_begin = strcmp(lang, "en") == 0 ? "Proof" : "证明";

    BUF_WRITE(buf, buf_len, buf_cap, "=== %s ===\n\n", proof_begin);

    if (proof->theorem_name) {
        BUF_WRITE(buf, buf_len, buf_cap, "定理: %s\n\n", proof->theorem_name);
    }

    /* 生成步骤 */
    for (int i = 0; i < proof->step_count; i++) {
        lvProofStepRecord *step = proof->steps[i];

        /* 缩进 */
        for (int d = 0; d < step->depth; d++) {
            BUF_WRITE(buf, buf_len, buf_cap, "  ");
        }

        /* 步骤编号 */
        BUF_WRITE(buf, buf_len, buf_cap, "[%d] ", step->step_id);

        /* 规则名称 */
        const char *rule_name = step->rule_name ? step->rule_name : "规则";
        BUF_WRITE(buf, buf_len, buf_cap, "由 %s", rule_name);

        /* 前提 */
        if (step->premise_count > 0) {
            BUF_WRITE(buf, buf_len, buf_cap, " (前提: ");
            for (int j = 0; j < step->premise_count; j++) {
                if (j > 0)
                    BUF_WRITE(buf, buf_len, buf_cap, ", ");
                BUF_WRITE(buf, buf_len, buf_cap, "%d", step->premise_step_ids[j]);
            }
            BUF_WRITE(buf, buf_len, buf_cap, ")");
        }

        /* 结论 */
        if (step->conclusion && step->conclusion->label) {
            BUF_WRITE(buf, buf_len, buf_cap, " 可得 %s", step->conclusion->label);
        }

        BUF_WRITE(buf, buf_len, buf_cap, "\n");
    }

    /* 统计 */
    BUF_WRITE(buf, buf_len, buf_cap, "\n--- 统计 ---\n");
    BUF_WRITE(buf, buf_len, buf_cap, "总步骤: %d\n", proof->step_count);
    BUF_WRITE(buf, buf_len, buf_cap, "最大深度: %d\n", proof->max_depth);
    BUF_WRITE(buf, buf_len, buf_cap, "使用公理: %d\n", proof->axiom_count);
    BUF_WRITE(buf, buf_len, buf_cap, "假设数量: %d\n", proof->assumption_count);

    return buf;
}

/**
 * @brief 编译为Graphviz格式
 */
char *lv_proof_compiler_to_graphviz(const lvProofObject *proof, const lvProofTrace *trace) {
    lv_UNUSED(trace);
    if (!proof)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_proof_compiler_to_graphviz: proof is NULL");

    size_t buf_cap = 16384;
    char *buf = (char *) lv_malloc(buf_cap);
    if (!buf)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_proof_compiler_to_graphviz: malloc failed");

    size_t buf_len = 0;

    BUF_WRITE(buf, buf_len, buf_cap, "digraph ProofTree {\n");
    BUF_WRITE(buf, buf_len, buf_cap, "  rankdir=TB;\n");
    BUF_WRITE(buf, buf_len, buf_cap, "  node [shape=box];\n");

    /* 生成节点 */
    for (int i = 0; i < proof->step_count; i++) {
        lvProofStepRecord *step = proof->steps[i];
        const char *label = step->conclusion && step->conclusion->label ? step->conclusion->label : "?";
        const char *color = step->color == PROOF_COLOR_GREEN             ? "lightgreen"
                            : step->color == PROOF_COLOR_ORANGE_EX_FALSO ? "orange"
                                                                         : "lightblue";

        BUF_WRITE(buf, buf_len, buf_cap, "  S%d [label=\"%s\", style=filled, fillcolor=%s];\n", step->step_id, label, color);
    }

    /* 生成边 */
    for (int i = 0; i < proof->step_count; i++) {
        lvProofStepRecord *step = proof->steps[i];
        for (int j = 0; j < step->premise_count; j++) {
            int premise_id = step->premise_step_ids[j];
            const char *rule_name = step->rule_name ? step->rule_name : "";
            BUF_WRITE(buf, buf_len, buf_cap, "  S%d -> S%d [label=\"%s\"];\n", premise_id, step->step_id, rule_name);
        }
    }

    BUF_WRITE(buf, buf_len, buf_cap, "}\n");

    return buf;
}

/**
 * @brief 编译证明对象为字符串
 */
char *lv_proof_compiler_compile(lvProofCompiler *compiler, const lvProofObject *proof, const lvProofTrace *trace) {
    if (!compiler || !proof)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_proof_compiler_compile: compiler or proof is NULL");

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
bool lv_proof_export_to_file(const lvProofObject *proof, const lvProofTrace *trace, lvOutputFormat format,
                             const char *filename) {
    if (!proof || !filename)
        return false;

    lvCompilerConfig config = lv_compiler_config_default();
    config.format = format;

    lvProofCompiler *compiler = lv_proof_compiler_create(&config);
    if (!compiler)
        return false;

    char *content = lv_proof_compiler_compile(compiler, proof, trace);
    lv_proof_compiler_destroy(compiler);

    if (!content)
        return false;

    FILE *fp = lv_file_open(filename, "w");
    if (!fp) {
        lv_free((void **) &content);
        return false;
    }

    fputs(content, fp);
    lv_file_close(fp);
    lv_free((void **) &content);

    return true;
}

/* ============== Proof Trace 实现 ============== */

static void lv_trace_event_set_description(lvTraceEvent *ev, const char *desc) {
    if (!ev || !desc) return;
    if (ev->description) lv_free((void **)&ev->description);
    ev->description = lv_strdup(desc);
}

lv_PUBLIC_API lvProofTrace *lv_proof_trace_create(void) {
    lvProofTrace *trace = (lvProofTrace *)lv_calloc(1, sizeof(lvProofTrace));
    if (!trace)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_proof_trace_create: calloc failed");

    trace->event_capacity = 64;
    trace->events = (lvTraceEvent **)lv_malloc((size_t)trace->event_capacity * sizeof(lvTraceEvent *));
    if (!trace->events) {
        lv_free((void **)&trace);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_proof_trace_create: events malloc failed");
    }
    trace->event_count = 0;
    trace->total_steps = 0;
    trace->total_backtracks = 0;
    trace->max_depth = 0;
    trace->snapshot_data = NULL;
    return trace;
}

lv_PUBLIC_API void lv_proof_trace_destroy(lvProofTrace *trace) {
    if (!trace) return;
    for (int i = 0; i < trace->event_count; i++) {
        if (trace->events[i]) {
            lv_trace_event_destroy(trace->events[i]);
        }
    }
    lv_free((void **)&trace->events);
    if (trace->snapshot_data) lv_free((void **)&trace->snapshot_data);
    lv_free((void **)&trace);
}

lv_PUBLIC_API int lv_proof_trace_add_event(lvProofTrace *trace, lvTraceEvent *event) {
    if (!trace || !event)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "lv_proof_trace_add_event: trace or event is NULL");
    if (!lv_ensure_capacity((void **)&trace->events, trace->event_count,
                            &trace->event_capacity, sizeof(lvTraceEvent *), 1))
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_proof_trace_add_event: ensure_capacity failed");
    trace->events[trace->event_count++] = event;
    return 0;
}

lv_PUBLIC_API void lv_proof_trace_start(lvProofTrace *trace, int proof_id) {
    if (!trace) return;
    trace->proof_id = proof_id;
    lvTraceEvent *ev = lv_trace_event_create(TRACE_EVENT_START);
    if (ev) {
        ev->step_id = proof_id;
        ev->depth = 0;
        lv_proof_trace_add_event(trace, ev);
    }
}

lv_PUBLIC_API void lv_proof_trace_step(lvProofTrace *trace, int step_id, const char *description, int depth) {
    if (!trace) return;
    lvTraceEvent *ev = lv_trace_event_create(TRACE_EVENT_STEP);
    if (!ev) return;
    ev->step_id = step_id;
    ev->depth = depth;
    lv_trace_event_set_description(ev, description ? description : "");
    lv_proof_trace_add_event(trace, ev);
    trace->total_steps++;
    if (depth > trace->max_depth) trace->max_depth = depth;
}

lv_PUBLIC_API void lv_proof_trace_backtrack(lvProofTrace *trace, int from_step, int to_step) {
    if (!trace) return;
    lvTraceEvent *ev = lv_trace_event_create(TRACE_EVENT_BACKTRACK);
    if (!ev) return;
    ev->step_id = from_step;
    ev->data.backtrack.from_step = from_step;
    ev->data.backtrack.to_step = to_step;
    lv_proof_trace_add_event(trace, ev);
    trace->total_backtracks++;
}

lv_PUBLIC_API void lv_proof_trace_branch(lvProofTrace *trace, const char *branch_name, int branch_id, int depth) {
    if (!trace) return;
    lvTraceEvent *ev = lv_trace_event_create(TRACE_EVENT_BRANCH);
    if (!ev) return;
    ev->step_id = branch_id;
    ev->depth = depth;
    if (branch_name) {
        ev->data.branch.branch_name = lv_strdup(branch_name);
    }
    ev->data.branch.branch_id = branch_id;
    lv_proof_trace_add_event(trace, ev);
}

lv_PUBLIC_API void lv_proof_trace_lemma(lvProofTrace *trace, int lemma_id, const char *lemma_name) {
    if (!trace) return;
    lvTraceEvent *ev = lv_trace_event_create(TRACE_EVENT_LEMMA);
    if (!ev) return;
    ev->step_id = lemma_id;
    ev->data.lemma.lemma_id = lemma_id;
    if (lemma_name) {
        ev->data.lemma.lemma_name = lv_strdup(lemma_name);
    }
    lv_proof_trace_add_event(trace, ev);
}

lv_PUBLIC_API void lv_proof_trace_contradiction(lvProofTrace *trace, lvProofScopeId scope_id, int assumption_count) {
    if (!trace) return;
    lvTraceEvent *ev = lv_trace_event_create(TRACE_EVENT_CONTRADICTION);
    if (!ev) return;
    ev->data.contradiction.scope_id = scope_id;
    ev->data.contradiction.assumption_count = assumption_count;
    lv_proof_trace_add_event(trace, ev);
}

lv_PUBLIC_API void lv_proof_trace_complete(lvProofTrace *trace, bool success) {
    if (!trace) return;
    lvTraceEvent *ev = lv_trace_event_create(success ? TRACE_EVENT_COMPLETE : TRACE_EVENT_FAIL);
    if (!ev) return;
    ev->step_id = trace->proof_id;
    lv_proof_trace_add_event(trace, ev);
}

/* ============== Trace Event 实现 ============== */

lv_PUBLIC_API lvTraceEvent *lv_trace_event_create(lvTraceEventType type) {
    lvTraceEvent *ev = (lvTraceEvent *)lv_calloc(1, sizeof(lvTraceEvent));
    if (!ev)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_trace_event_create: calloc failed");
    ev->type = type;
    ev->step_id = -1;
    ev->description = NULL;
    ev->details = NULL;
    ev->depth = 0;
    ev->timestamp = (int64_t)time(NULL);
    return ev;
}

lv_PUBLIC_API void lv_trace_event_destroy(lvTraceEvent *event) {
    if (!event) return;
    if (event->description) lv_free((void **)&event->description);
    if (event->details) lv_free((void **)&event->details);
    if (event->type == TRACE_EVENT_BRANCH && event->data.branch.branch_name) {
        lv_free((void **)&event->data.branch.branch_name);
    }
    if (event->type == TRACE_EVENT_LEMMA && event->data.lemma.lemma_name) {
        lv_free((void **)&event->data.lemma.lemma_name);
    }
    lv_free((void **)&event);
}
