/**
 * @file proof_compiler.c
 * @brief 证明编译层实现
 *
 * @version 4.0.0
 */

#include "lv/proof_compiler.h"

#include "lv/lv_lifecycle.h"
#include "lv/lv_file.h"
#include "lv/lv_xmacro.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


#include "lv/lv_internal.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_dot_writer.h"
#include "lv/lv_str_utils.h"


#include "lv/circuit_breaker.h"
#include "lv/lv_utils.h"

/* ============== Proof Object 实现 ============== */

/* ProofObject 部分构建守卫：任一成员分配失败时统一释放已分配成员与外壳，
 * 替代递增回滚样板 */
typedef struct {
    lvProofObject *obj;
} ProofObjectGuard;

static void proof_object_guard_cleanup(void *p) {
    ProofObjectGuard *g = (ProofObjectGuard *) p;
    if (g->obj) {
        lv_free((void **) &g->obj->steps);
        lv_free((void **) &g->obj->axiom_ids);
        lv_free((void **) &g->obj->assumption_ids);
        lv_free((void **) &g->obj);
    }
}

/**
 * @brief 创建证明对象
 */
lvProofObject *lv_proof_object_create(void) {
    lvProofObject *obj = (lvProofObject *) lv_calloc(1, sizeof(lvProofObject));
    if (!obj)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_proof_object_create: calloc failed");

    /* 部分构建守卫：后续任一分配失败自动释放已分配成员；成功路径 guard.obj = NULL 解除 */
    ProofObjectGuard guard = {obj};
    lv_DEFER(proof_object_guard_cleanup, &guard);

    obj->step_capacity = 64;
    obj->steps = (lvProofStepRecord **) lv_malloc(obj->step_capacity * sizeof(lvProofStepRecord *));
    if (!obj->steps)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_proof_object_create: steps malloc failed");

    obj->axiom_ids = (int *) lv_malloc(32 * sizeof(int));
    if (!obj->axiom_ids)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_proof_object_create: axiom_ids malloc failed");
    obj->axiom_capacity = 32;
    obj->assumption_ids = (int *) lv_malloc(32 * sizeof(int));
    if (!obj->assumption_ids)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_proof_object_create: assumption_ids malloc failed");
    obj->assumption_capacity = 32;

    guard.obj = NULL; /* 守卫解除：结果移交调用方 */
    return obj;
}

/**
 * @brief 销毁证明对象
 */
/* ── proof object / step record / trace 子资源销毁适配 ── */

/* 步骤元素销毁：委托 lv_proof_step_record_destroy */
static void destroy_proof_step_elem(void *elem) {
    lv_proof_step_record_destroy((lvProofStepRecord *) elem);
}

/* 命题字段引用计数递减（proposition_unref） */
static void destroy_proposition_field(void *obj, void *field_ptr) {
    (void) obj;
    Proposition **pp = (Proposition **) field_ptr;
    if (*pp)
        proposition_unref(*pp);
}

/* 跟踪事件元素销毁：委托 lv_trace_event_destroy */
static void destroy_trace_event_elem(void *elem) {
    lv_trace_event_destroy((lvTraceEvent *) elem);
}

/* data 为联合体：仅按事件类型释放对应 union 成员中的字符串
 * （branch_name / lemma_name 与其它成员的 int 值重叠，须条件释放） */
static void destroy_trace_event_union_data(void *obj, void *field_ptr) {
    (void) field_ptr;
    lvTraceEvent *ev = (lvTraceEvent *) obj;
    if (ev->type == TRACE_EVENT_BRANCH && ev->data.branch.branch_name)
        lv_free((void **) &ev->data.branch.branch_name);
    else if (ev->type == TRACE_EVENT_LEMMA && ev->data.lemma.lemma_name)
        lv_free((void **) &ev->data.lemma.lemma_name);
}

/* lv_proof_object_destroy 字段描述表：steps 逐元素销毁后释放数组，
 * goal 引用计数递减（proposition_unref），其余纯指针字段 */
static const lvFieldDesc s_proof_object_destroy_fields[] = {
    lv_FIELD_ARRAY(lvProofObject, steps, step_count, destroy_proof_step_elem),
    lv_FIELD_PLAIN(lvProofObject, axiom_ids),
    lv_FIELD_PLAIN(lvProofObject, assumption_ids),
    lv_FIELD_PLAIN(lvProofObject, theorem_name),
    lv_FIELD_CUSTOM(lvProofObject, goal, destroy_proposition_field),
};

void lv_proof_object_destroy(lvProofObject *obj) {
    if (!obj)
        return;
    lv_obj_destroy_fields(obj, s_proof_object_destroy_fields,
                          sizeof(s_proof_object_destroy_fields) / sizeof(s_proof_object_destroy_fields[0]));
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
 *
 * 前提数组不预分配，由 lv_proof_step_record_set_premises() 统一管理，
 * 保证 premise_step_ids 与 premise_capacity 始终一致（防止堆损坏）。
 */
lvProofStepRecord *lv_proof_step_record_create(void) {
    lvProofStepRecord *record = (lvProofStepRecord *) lv_calloc(1, sizeof(lvProofStepRecord));
    if (!record)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_proof_step_record_create: calloc failed");

    return record;
}

/**
 * @brief 销毁步骤记录
 */
/* lv_proof_step_record_destroy 字段描述表：rule_name/premise_step_ids/
 * justification 纯指针释放，conclusion 引用计数递减 */
static const lvFieldDesc s_step_record_destroy_fields[] = {
    lv_FIELD_PLAIN(lvProofStepRecord, rule_name),
    lv_FIELD_PLAIN(lvProofStepRecord, premise_step_ids),
    lv_FIELD_CUSTOM(lvProofStepRecord, conclusion, destroy_proposition_field),
    lv_FIELD_PLAIN(lvProofStepRecord, justification),
};

void lv_proof_step_record_destroy(lvProofStepRecord *record) {
    if (!record)
        return;
    lv_obj_destroy_fields(record, s_step_record_destroy_fields,
                          sizeof(s_step_record_destroy_fields) / sizeof(s_step_record_destroy_fields[0]));
    lv_free((void **) &record);
}

/**
 * @brief 设置步骤记录的前提步骤ID
 *
 * 统一管理 premise_step_ids 的分配与释放，并同步 premise_capacity，
 * 避免外部直接覆盖指针导致容量不一致（堆损坏隐患）。
 *
 * @param record 步骤记录
 * @param ids    前提ID数组（count 为 0 时可传 NULL）
 * @param count  前提数量（>= 0）
 * @return true 成功；false 参数非法或内存分配失败
 */
bool lv_proof_step_record_set_premises(lvProofStepRecord *record, const int *ids, int count) {
    if (!record || count < 0 || (count > 0 && !ids))
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_PARAM, "lv_proof_step_record_set_premises: invalid argument");

    if (count == 0) {
        lv_free((void **) &record->premise_step_ids);
        record->premise_count = 0;
        record->premise_capacity = 0;
        return true;
    }

    if (count > record->premise_capacity) {
        int *new_ids = (int *) lv_realloc(record->premise_step_ids, (size_t) count * sizeof(int));
        if (!new_ids)
            lv_RETURN_ERROR_BOOL(lv_ERROR_OUT_OF_MEMORY, "lv_proof_step_record_set_premises: realloc failed");
        record->premise_step_ids = new_ids;
        record->premise_capacity = count;
    }

    memcpy(record->premise_step_ids, ids, (size_t) count * sizeof(int));
    record->premise_count = count;
    return true;
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

    return compiler;
}

/**
 * @brief 销毁证明编译器
 */
void lv_proof_compiler_destroy(lvProofCompiler *compiler) {
    if (!compiler)
        return;
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

/** @brief get_event_type_name 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_get_event_type_name_entries[] = {
    {"开始", TRACE_EVENT_START},
    {"步骤", TRACE_EVENT_STEP},
    {"回溯", TRACE_EVENT_BACKTRACK},
    {"分支", TRACE_EVENT_BRANCH},
    {"引理", TRACE_EVENT_LEMMA},
    {"Oracle", TRACE_EVENT_ORACLE},
    {"矛盾", TRACE_EVENT_CONTRADICTION},
    {"完成", TRACE_EVENT_COMPLETE},
    {"失败", TRACE_EVENT_FAIL},
};

static const char *get_event_type_name(lvTraceEventType type) {
    return lv_enum_to_str(s_get_event_type_name_entries, lv_ARRAY_SIZE(s_get_event_type_name_entries), (int) type, "未知");
}

/**
 * @brief 编译为JSON格式
 */
char *lv_proof_compiler_to_json(const lvProofObject *proof, const lvProofTrace *trace) {
    lv_UNUSED(trace);
    if (!proof)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_proof_compiler_to_json: proof is NULL");

    lvStrBuf sb;
    lv_strbuf_init(&sb);

    lv_strbuf_printf(&sb, "{\n");
    lv_strbuf_printf(&sb, "  \"proof_id\": %d,\n", proof->proof_id);
    {
        /* theorem_name 经 JSON 转义后写入（两遍法，防止 JSON 注入/破坏） */
        const char *name = proof->theorem_name ? proof->theorem_name : "unknown";
        char *esc = lv_str_json_escape_alloc(name, strlen(name), NULL);
        if (!esc)
            lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_proof_compiler_to_json: theorem_name escape alloc failed");
        lv_strbuf_printf(&sb, "  \"theorem_name\": \"%s\",\n", esc);
        lv_free((void **) &esc);
    }
    lv_strbuf_printf(&sb, "  \"is_proved\": %s,\n", proof->is_proved ? "true" : "false");
    lv_strbuf_printf(&sb, "  \"final_color\": %d,\n", proof->final_color);
    lv_strbuf_printf(&sb, "  \"step_count\": %d,\n", proof->step_count);
    lv_strbuf_printf(&sb, "  \"max_depth\": %d,\n", proof->max_depth);
    lv_strbuf_printf(&sb, "  \"axiom_count\": %d,\n", proof->axiom_count);
    lv_strbuf_printf(&sb, "  \"assumption_count\": %d,\n", proof->assumption_count);
    lv_strbuf_printf(&sb, "  \"elapsed_us\": %lld,\n", (long long) proof->elapsed_us);

    /* 步骤数组 */
    lv_strbuf_printf(&sb, "  \"steps\": [\n");
    for (int i = 0; i < proof->step_count; i++) {
        lvProofStepRecord *step = proof->steps[i];
        lv_strbuf_printf(&sb, "    {\"id\": %d, \"type\": %d, \"depth\": %d}", step->step_id, step->type, step->depth);
        if (i < proof->step_count - 1) {
            lv_strbuf_printf(&sb, ",");
        }
        lv_strbuf_printf(&sb, "\n");
    }
    lv_strbuf_printf(&sb, "  ]\n");

    lv_strbuf_printf(&sb, "}\n");

    return lv_strbuf_to_string(&sb);
}

/**
 * @brief 编译为LaTeX格式
 */
char *lv_proof_compiler_to_latex(const lvProofObject *proof, const char *language) {
    if (!proof)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_proof_compiler_to_latex: proof is NULL");

    lvStrBuf sb;
    lv_strbuf_init(&sb);

    const char *lang = language ? language : "zh";
    const char *proof_begin = lv_str_eq(lang, "en") ? "Proof" : "证明";
    const char *qed = lv_str_eq(lang, "en") ? "\\qed" : "证毕";

    lv_strbuf_printf(&sb, "\\begin{Proof}\n");
    lv_strbuf_printf(&sb, "%s.\n\n", proof_begin);

    /* 生成步骤 */
    for (int i = 0; i < proof->step_count; i++) {
        lvProofStepRecord *step = proof->steps[i];

        /* 缩进 */
        if (step->depth > 0) {
            lv_strbuf_append_n(&sb, ' ', (size_t) step->depth * 2);
        }

        const char *rule_name = step->rule_name ? step->rule_name : "规则";
        lv_strbuf_printf(&sb, "由 %s 可得", rule_name);

        if (step->conclusion && step->conclusion->label) {
            lv_strbuf_printf(&sb, " $%s$.\n", step->conclusion->label);
        } else {
            lv_strbuf_printf(&sb, "。\n");
        }
    }

    lv_strbuf_printf(&sb, "\n%s\n", qed);
    lv_strbuf_printf(&sb, "\\end{Proof}\n");

    return lv_strbuf_to_string(&sb);
}

/**
 * @brief 编译为TikZ格式
 */
char *lv_proof_compiler_to_tikz(const lvProofObject *proof) {
    if (!proof)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_proof_compiler_to_tikz: proof is NULL");

    lvStrBuf sb;
    lv_strbuf_init(&sb);

    lv_strbuf_printf(&sb, "\\begin{tikzpicture}[node distance=2cm]\n");
    lv_strbuf_printf(&sb, "\\tikzstyle{step}=[circle,draw,minimum size=1cm]\n");
    lv_strbuf_printf(&sb, "\\tikzstyle{arrow}=[->,>=stealth]\n");

    /* 生成节点 */
    for (int i = 0; i < proof->step_count; i++) {
        lvProofStepRecord *step = proof->steps[i];
        lv_strbuf_printf(&sb, "\\node[step] (S%d) at (%d, %d) {$S_%d$};\n", step->step_id, step->step_id % 3, -step->depth,
                         step->step_id);
    }

    /* 生成边 */
    for (int i = 0; i < proof->step_count; i++) {
        lvProofStepRecord *step = proof->steps[i];
        for (int j = 0; j < step->premise_count; j++) {
            int premise_id = step->premise_step_ids[j];
            lv_strbuf_printf(&sb, "\\draw[arrow] (S%d) -- (S%d);\n", premise_id, step->step_id);
        }
    }

    lv_strbuf_printf(&sb, "\\end{tikzpicture}\n");

    return lv_strbuf_to_string(&sb);
}

/**
 * @brief 编译为纯文本格式
 */
char *lv_proof_compiler_to_text(const lvProofObject *proof, const char *language) {
    if (!proof)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_proof_compiler_to_text: proof is NULL");

    lvStrBuf sb;
    lv_strbuf_init(&sb);

    const char *lang = language ? language : "zh";
    const char *proof_begin = lv_str_eq(lang, "en") ? "Proof" : "证明";

    lv_strbuf_printf(&sb, "=== %s ===\n\n", proof_begin);

    if (proof->theorem_name) {
        lv_strbuf_printf(&sb, "定理: %s\n\n", proof->theorem_name);
    }

    /* 生成步骤 */
    for (int i = 0; i < proof->step_count; i++) {
        lvProofStepRecord *step = proof->steps[i];

        /* 缩进 */
        if (step->depth > 0) {
            lv_strbuf_append_n(&sb, ' ', (size_t) step->depth * 2);
        }

        /* 步骤编号 */
        lv_strbuf_printf(&sb, "[%d] ", step->step_id);

        /* 规则名称 */
        const char *rule_name = step->rule_name ? step->rule_name : "规则";
        lv_strbuf_printf(&sb, "由 %s", rule_name);

        /* 前提 */
        if (step->premise_count > 0) {
            lv_strbuf_printf(&sb, " (前提: ");
            for (int j = 0; j < step->premise_count; j++) {
                if (j > 0)
                    lv_strbuf_printf(&sb, ", ");
                lv_strbuf_printf(&sb, "%d", step->premise_step_ids[j]);
            }
            lv_strbuf_printf(&sb, ")");
        }

        /* 结论 */
        if (step->conclusion && step->conclusion->label) {
            lv_strbuf_printf(&sb, " 可得 %s", step->conclusion->label);
        }

        lv_strbuf_printf(&sb, "\n");
    }

    /* 统计 */
    lv_strbuf_printf(&sb, "\n--- 统计 ---\n");
    lv_strbuf_printf(&sb, "总步骤: %d\n", proof->step_count);
    lv_strbuf_printf(&sb, "最大深度: %d\n", proof->max_depth);
    lv_strbuf_printf(&sb, "使用公理: %d\n", proof->axiom_count);
    lv_strbuf_printf(&sb, "假设数量: %d\n", proof->assumption_count);

    return lv_strbuf_to_string(&sb);
}

/**
 * @brief 编译为Graphviz格式
 */
char *lv_proof_compiler_to_graphviz(const lvProofObject *proof, const lvProofTrace *trace) {
    lv_UNUSED(trace);
    if (!proof)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_proof_compiler_to_graphviz: proof is NULL");

    lvStrBuf sb;
    lv_strbuf_init(&sb);

    lv_dot_begin(&sb, "ProofTree", "TB", "shape=box", NULL);

    /* 生成节点 */
    for (int i = 0; i < proof->step_count; i++) {
        lvProofStepRecord *step = proof->steps[i];
        const char *label = step->conclusion && step->conclusion->label ? step->conclusion->label : "?";
        const char *color = step->color == PROOF_COLOR_GREEN             ? "lightgreen"
                            : step->color == PROOF_COLOR_ORANGE_EX_FALSO ? "orange"
                                                                         : "lightblue";

        char extra[128];
        snprintf(extra, sizeof(extra), "style=filled, fillcolor=%s", color);
        lv_dot_node_id(&sb, "S", step->step_id, label, extra);
    }

    /* 生成边 */
    for (int i = 0; i < proof->step_count; i++) {
        lvProofStepRecord *step = proof->steps[i];
        for (int j = 0; j < step->premise_count; j++) {
            int premise_id = step->premise_step_ids[j];
            const char *rule_name = step->rule_name ? step->rule_name : "";
            lv_dot_edge_id(&sb, "S", premise_id, step->step_id, rule_name, NULL);
        }
    }

    lv_dot_end(&sb);

    return lv_strbuf_to_string(&sb);
}

/* ================================================================
 * 函数指针类型 + 包装函数 + 查找表（替代 switch）
 * ================================================================ */

/** @brief 编译输出处理函数指针类型 */
typedef char *(*CompileHandlerFn)(lvProofCompiler *compiler, const lvProofObject *proof, const lvProofTrace *trace);

static char *compile_json(lvProofCompiler *compiler, const lvProofObject *proof, const lvProofTrace *trace) {
    (void)compiler;
    return lv_proof_compiler_to_json(proof, trace);
}
static char *compile_latex(lvProofCompiler *compiler, const lvProofObject *proof, const lvProofTrace *trace) {
    (void)trace;
    return lv_proof_compiler_to_latex(proof, compiler->config.language);
}
static char *compile_tikz(lvProofCompiler *compiler, const lvProofObject *proof, const lvProofTrace *trace) {
    (void)compiler; (void)trace;
    return lv_proof_compiler_to_tikz(proof);
}
static char *compile_text(lvProofCompiler *compiler, const lvProofObject *proof, const lvProofTrace *trace) {
    (void)trace;
    return lv_proof_compiler_to_text(proof, compiler->config.language);
}
static char *compile_graphviz(lvProofCompiler *compiler, const lvProofObject *proof, const lvProofTrace *trace) {
    (void)compiler;
    return lv_proof_compiler_to_graphviz(proof, trace);
}

/** @brief 编译输出格式查找表（按枚举值索引） */
static const CompileHandlerFn kCompileHandlers[] = {
    [OUTPUT_FORMAT_JSON] = compile_json,
    [OUTPUT_FORMAT_LATEX] = compile_latex,
    [OUTPUT_FORMAT_TIKZ] = compile_tikz,
    [OUTPUT_FORMAT_TEXT] = compile_text,
    [OUTPUT_FORMAT_GRAPHVIZ] = compile_graphviz,
};

/**
 * @brief 编译证明对象为字符串
 */
char *lv_proof_compiler_compile(lvProofCompiler *compiler, const lvProofObject *proof, const lvProofTrace *trace) {
    if (!compiler || !proof)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_proof_compiler_compile: compiler or proof is NULL");

    return LV_DISPATCH(kCompileHandlers, compiler->config.format, lv_proof_compiler_to_text(proof, compiler->config.language), compiler, proof, trace);
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

/* lv_proof_trace_destroy 字段描述表：events 逐元素销毁后释放数组，
 * snapshot_data 纯指针释放 */
static const lvFieldDesc s_proof_trace_destroy_fields[] = {
    lv_FIELD_ARRAY(lvProofTrace, events, event_count, destroy_trace_event_elem),
    lv_FIELD_PLAIN(lvProofTrace, snapshot_data),
};

lv_PUBLIC_API void lv_proof_trace_destroy(lvProofTrace *trace) {
    if (!trace) return;
    lv_obj_destroy_fields(trace, s_proof_trace_destroy_fields,
                          sizeof(s_proof_trace_destroy_fields) / sizeof(s_proof_trace_destroy_fields[0]));
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

/* lv_trace_event_destroy 字段描述表：description/details 纯指针释放，
 * data 联合体按事件类型条件释放（见 destroy_trace_event_union_data） */
static const lvFieldDesc s_trace_event_destroy_fields[] = {
    lv_FIELD_PLAIN(lvTraceEvent, description),
    lv_FIELD_PLAIN(lvTraceEvent, details),
    lv_FIELD_CUSTOM(lvTraceEvent, data, destroy_trace_event_union_data),
};

lv_PUBLIC_API void lv_trace_event_destroy(lvTraceEvent *event) {
    if (!event) return;
    lv_obj_destroy_fields(event, s_trace_event_destroy_fields,
                          sizeof(s_trace_event_destroy_fields) / sizeof(s_trace_event_destroy_fields[0]));
    lv_free((void **)&event);
}
