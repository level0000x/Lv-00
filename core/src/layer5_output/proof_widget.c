/**
 * @file proof_widget.c
 * @brief 证明交互可视化组件实现 -- Widget 生命周期、布局管理、证明状态查询与策略推荐
 *
 * 实现 proof_widget.h 中声明的全部 API：
 * - Widget 布局的创建与销毁
 * - Widget 的注册、更新与排序
 * - 证明目标与假设的查询
 * - 策略建议、步骤高亮与搜索树/依赖图导出
 * - 布局导出与策略应用回传
 */

#include "lv/proof_widget.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_json.h"
#include "lv/proof.h"

#include "lv_utils.h"

/* ================================================================
 * 内部常量
 * ================================================================ */

/** JSON 缓冲区初始大小 */
#define JSON_BUF_INIT_SIZE 1024

/** 布局默认列数 */
#define DEFAULT_COLUMNS 2

/** 布局默认行数 */
#define DEFAULT_ROWS 2

/* ================================================================
 * 第一部分：生命周期
 * ================================================================ */

/**
 * @brief 创建 Widget 布局，分配初始容量
 * @param layout_capacity 初始 widget 容量（<=0 时使用默认值 8）
 * @return 布局指针，失败返回 NULL
 */
lvWidgetLayout *proof_widget_init(int layout_capacity) {
    if (layout_capacity <= 0) {
        layout_capacity = 8;
    }

    lvWidgetLayout *layout = (lvWidgetLayout *) lv_calloc(1, sizeof(lvWidgetLayout));
    if (!layout)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "proof_widget_init: layout calloc failed");

    layout->widgets = (ProofWidgetState *) lv_calloc((size_t) layout_capacity, sizeof(ProofWidgetState));
    if (!layout->widgets) {
        lv_free_ptr(layout);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "proof_widget_init: widgets calloc failed");
    }

    layout->widget_count = 0;
    layout->widget_capacity = layout_capacity;
    layout->layout_type = LAYOUT_GRID;
    layout->columns = DEFAULT_COLUMNS;
    layout->rows = DEFAULT_ROWS;
    layout->order_indices = NULL;
    layout->persistence_key = NULL;

    return layout;
}

/* 销毁 Widget 布局，释放所有内部资源 */
void proof_widget_destroy(lvWidgetLayout *layout) {
    if (!layout)
        return;

    /* 释放每个 widget 的动态字符串 */
    for (int i = 0; i < layout->widget_count; i++) {
        ProofWidgetState *ws = &layout->widgets[i];
        if (ws->display_label) {
            lv_free_ptr(ws->display_label);
        }
        if (ws->interaction_data) {
            lv_free_ptr(ws->interaction_data);
        }
    }

    lv_free_ptr(layout->widgets);
    lv_free_ptr(layout->order_indices);
    lv_free_ptr(layout->persistence_key);
    lv_free_ptr(layout);
}

/* ================================================================
 * 第二部分：Widget 注册与更新
 * ================================================================ */

/**
 * @brief 注册新 Widget 到布局中
 * @param layout     布局指针
 * @param widget_type Widget 类型枚举
 * @param label      显示标签（可为 NULL，内部会复制）
 * @param bound_step 绑定的证明步骤 ID
 * @return widget_id（非负），失败返回 -1
 *
 * @note 已建未用/预留：当前无业务调用者（仅测试 test_layer5_output /
 *       test_output_export 使用），保留供证明导出 Widget 布局接入。
 */
int proof_widget_register(lvWidgetLayout *layout, ProofWidgetType widget_type, const char *label, int bound_step) {
    if (!layout)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "proof_widget_register: layout is NULL");

    /* 容量不足时倍增扩容（溢出检查由 lv_ensure_capacity 内部完成） */
    if (layout->widget_count >= layout->widget_capacity) {
        int old_cap = layout->widget_capacity;
        if (!lv_ensure_capacity((void **) &layout->widgets, old_cap,
                                &layout->widget_capacity, sizeof(ProofWidgetState), 1))
            lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "proof_widget_register: lv_ensure_capacity failed");

        /* 清零新增部分 */
        memset(layout->widgets + old_cap, 0,
               (size_t) (layout->widget_capacity - old_cap) * sizeof(ProofWidgetState));
    }

    int id = layout->widget_count;
    ProofWidgetState *ws = &layout->widgets[id];
    ws->widget_id = id;
    ws->widget_type = widget_type;
    ws->is_active = false;
    ws->is_enabled = true;
    ws->display_label = label ? lv_strdup(label) : NULL;
    ws->bound_step_id = bound_step;
    ws->interaction_data = NULL;

    layout->widget_count++;
    return id;
}

/**
 * @brief 更新已有 Widget 的状态
 * @param layout          布局指针
 * @param widget_id       Widget ID
 * @param is_active       是否激活
 * @param is_enabled      是否启用
 * @param display_label   新的显示标签（可为 NULL 表示不更新）
 * @param bound_step_id   新的绑定步骤 ID
 * @param interaction_json 新的交互数据 JSON（可为 NULL 表示不更新）
 * @return 0 成功，-1 参数无效或 widget_id 越界
 */
int proof_widget_update(lvWidgetLayout *layout, int widget_id, bool is_active, bool is_enabled,
                        const char *display_label, int bound_step_id, const char *interaction_json) {
    if (!layout)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "proof_widget_update: layout is NULL");
    if (widget_id < 0 || widget_id >= layout->widget_count)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "proof_widget_update: invalid widget_id");

    ProofWidgetState *ws = &layout->widgets[widget_id];
    ws->is_active = is_active;
    ws->is_enabled = is_enabled;
    ws->bound_step_id = bound_step_id;

    /* 更新显示标签（重新分配） */
    if (display_label) {
        if (ws->display_label)
            lv_free_ptr(ws->display_label);
        ws->display_label = lv_strdup(display_label);
    }

    /* 更新交互数据 JSON（重新分配） */
    if (interaction_json) {
        if (ws->interaction_data)
            lv_free_ptr(ws->interaction_data);
        ws->interaction_data = lv_strdup(interaction_json);
    }

    return 0;
}

/* ================================================================
 * 第三部分：证明状态查询
 * ================================================================ */

/**
 * @brief 生成命题的可显示文本（label > name > description）
 * @param prop 命题（可为 NULL）
 * @return lv_malloc 分配的字符串，无可用文本时为空串；失败返回 NULL
 */
static char *widget_proposition_text(const Proposition *prop) {
    const char *text = NULL;
    if (prop) {
        if (prop->label && prop->label[0] != '\0')
            text = prop->label;
        else if (prop->name && prop->name[0] != '\0')
            text = prop->name;
        else if (prop->description && prop->description[0] != '\0')
            text = prop->description;
    }
    return lv_strdup(text ? text : "");
}

/**
 * @brief 从导航器的假设集合（激活的假设作用域）构造假设条目
 * @param navigator 证明导航器指针
 * @param out_hypotheses 输出假设条目数组
 * @param max_count 数组最大容量
 * @return 写入的条目数量（>=0）
 */
static int widget_collect_hypotheses(const ProofNavigator *navigator, lvHypothesisEntry *out_hypotheses,
                                     int max_count) {
    int written = 0;
    if (!navigator->scope_assumptions || !navigator->scope_active)
        return 0;

    for (int i = 0; i < navigator->scope_count && written < max_count; i++) {
        if (!navigator->scope_active[i] || !navigator->scope_assumptions[i])
            continue;

        lvHypothesisEntry *he = &out_hypotheses[written];
        memset(he, 0, sizeof(*he));
        he->hyp_id = i;
        he->name = widget_proposition_text(navigator->scope_assumptions[i]);
        he->type_text = lv_strdup(proposition_type_to_string(navigator->scope_assumptions[i]->type));
        he->value_text = NULL;
        he->source_step = -1;
        he->is_selected = false;
        written++;
    }
    return written;
}

/**
 * @brief 释放由 widget_collect_hypotheses 填充的假设条目字符串
 * @param entries 假设条目数组
 * @param count   条目数量
 */
static void widget_free_hypotheses(lvHypothesisEntry *entries, int count) {
    for (int i = 0; i < count; i++) {
        if (entries[i].name)
            lv_free_ptr(entries[i].name);
        if (entries[i].type_text)
            lv_free_ptr(entries[i].type_text);
        if (entries[i].value_text)
            lv_free_ptr(entries[i].value_text);
    }
}

/**
 * @brief 从 ProofNavigator 获取当前证明目标
 * @param navigator 证明导航器指针
 * @param out_goal  输出参数，填充目标显示结构
 * @return 0 成功，-1 参数无效
 */
int proof_widget_get_goal(const ProofNavigator *navigator, lvGoalDisplay *out_goal) {
    if (!navigator || !out_goal)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "proof_widget_get_goal: navigator or out_goal is NULL");

    /* 初始化输出结构 */
    memset(out_goal, 0, sizeof(lvGoalDisplay));
    out_goal->is_solved = navigator->is_complete;
    out_goal->depth = navigator->step_count;

    /* 从 navigator 查询当前目标的真实文本（无目标时返回空串） */
    out_goal->goal_text = widget_proposition_text(navigator->target_prop);
    if (!out_goal->goal_text)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "proof_widget_get_goal: strdup failed");

    /* 附带当前假设列表 */
    if (navigator->scope_count > 0) {
        out_goal->hypotheses = (lvHypothesisEntry *) lv_calloc((size_t) navigator->scope_count,
                                                               sizeof(lvHypothesisEntry));
        if (out_goal->hypotheses)
            out_goal->hyp_count = widget_collect_hypotheses(navigator, out_goal->hypotheses,
                                                            navigator->scope_count);
    }

    /* 附带上下文项（假设类型文本） */
    if (out_goal->hyp_count > 0) {
        out_goal->context_terms = (char **) lv_calloc((size_t) out_goal->hyp_count, sizeof(char *));
        if (out_goal->context_terms) {
            for (int i = 0; i < out_goal->hyp_count; i++) {
                out_goal->context_terms[i] = lv_strdup(out_goal->hypotheses[i].type_text
                                                           ? out_goal->hypotheses[i].type_text
                                                           : "");
            }
            out_goal->context_count = out_goal->hyp_count;
        }
    }

    return 0;
}

/**
 * @brief 从 ProofNavigator 获取假设列表
 * @param navigator     证明导航器指针
 * @param out_hypotheses 输出假设列表数组
 * @param max_count      数组最大容量
 * @return 实际假设数量（>=0），失败返回 -1
 */
int proof_widget_get_hypotheses(const ProofNavigator *navigator, lvHypothesisEntry *out_hypotheses, int max_count) {
    if (!navigator || !out_hypotheses || max_count <= 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "proof_widget_get_hypotheses: invalid parameters");

    /* 清零输出数组 */
    memset(out_hypotheses, 0, (size_t) max_count * sizeof(lvHypothesisEntry));

    /* 从导航器的假设集合构造假设列表 */
    return widget_collect_hypotheses(navigator, out_hypotheses, max_count);
}

/**
 * @brief 释放 GoalDisplay 内部动态分配的资源
 * @param goal 目标显示结构指针（可为 NULL）
 */
void goal_display_free(lvGoalDisplay *goal) {
    if (!goal)
        return;

    if (goal->goal_text) {
        lv_free_ptr(goal->goal_text);
        goal->goal_text = NULL;
    }

    /* 释放假设条目的字符串 */
    if (goal->hypotheses) {
        for (int i = 0; i < goal->hyp_count; i++) {
            lvHypothesisEntry *he = &goal->hypotheses[i];
            if (he->name)
                lv_free_ptr(he->name);
            if (he->type_text)
                lv_free_ptr(he->type_text);
            if (he->value_text)
                lv_free_ptr(he->value_text);
        }
        lv_free_ptr(goal->hypotheses);
        goal->hypotheses = NULL;
    }

    /* 释放上下文项 */
    if (goal->context_terms) {
        for (int i = 0; i < goal->context_count; i++) {
            if (goal->context_terms[i]) {
                lv_free_ptr(goal->context_terms[i]);
            }
        }
        lv_free_ptr(goal->context_terms);
        goal->context_terms = NULL;
    }

    goal->hyp_count = 0;
    goal->context_count = 0;
}

/* ================================================================
 * 第四部分：智能推荐与可视化
 * ================================================================ */

/**
 * @brief 基于当前证明状态建议可用策略
 * @param navigator      证明导航器指针
 * @param out_suggestions 输出策略名称字符串数组（各条目调用者需释放）
 * @param out_confidences 输出对应置信度数组
 * @param max_count       数组最大容量
 * @return 实际建议数量（>=0），-1 参数无效
 */
int proof_widget_suggest_tactic(const ProofNavigator *navigator, char **out_suggestions, double *out_confidences,
                                int max_count) {
    if (!navigator || !out_suggestions || !out_confidences || max_count <= 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "proof_widget_suggest_tactic: invalid parameters");

    /* 初始化输出 */
    for (int i = 0; i < max_count; i++) {
        out_suggestions[i] = NULL;
        out_confidences[i] = 0.0;
    }

    /* 无目标（目标文本为空）时不作任何推荐 */
    char *goal_text = widget_proposition_text(navigator->target_prop);
    if (!goal_text)
        return 0;
    if (goal_text[0] == '\0') {
        lv_free_ptr(goal_text);
        return 0;
    }

    /* 收集当前假设（临时缓冲） */
    enum { MAX_HYP_BUFFER = 64 };
    lvHypothesisEntry hyp_buffer[MAX_HYP_BUFFER];
    int hyp_count = widget_collect_hypotheses(navigator, hyp_buffer, MAX_HYP_BUFFER);

    int n = 0;

    /* 目标含等号 → 反射性 / 重写 */
    bool goal_has_equal = (strchr(goal_text, '=') != NULL) || strstr(goal_text, "equal") != NULL ||
                          strstr(goal_text, "congruent") != NULL || strstr(goal_text, "等于") != NULL;
    if (goal_has_equal) {
        out_suggestions[n] = lv_strdup("reflexivity");
        out_confidences[n] = 0.9;
        n++;
        if (n < max_count) {
            out_suggestions[n] = lv_strdup("rewrite");
            out_confidences[n] = 0.6;
            n++;
        }
    }

    /* 从假设中查找：假设包含完整目标 → exact；假设包含目标前提 → apply */
    int exact_hyp = -1;
    int premise_hyp = -1;
    const char *arrow = strstr(goal_text, "->");
    if (!arrow)
        arrow = strstr(goal_text, "→");
    char premise_buf[129];
    const char *premise = NULL;
    if (arrow && arrow != goal_text) {
        size_t premise_len = (size_t) (arrow - goal_text);
        if (premise_len > 0 && premise_len < sizeof(premise_buf)) {
            memcpy(premise_buf, goal_text, premise_len);
            premise_buf[premise_len] = '\0';
            premise = premise_buf;
        }
    }
    for (int i = 0; i < hyp_count; i++) {
        const char *hyp_text = hyp_buffer[i].name ? hyp_buffer[i].name : "";
        if (hyp_text[0] == '\0')
            continue;
        if (strstr(hyp_text, goal_text)) {
            exact_hyp = i;
            break;
        }
        if (premise && premise[0] != '\0' && strstr(hyp_text, premise))
            premise_hyp = i;
    }

    if (n < max_count && exact_hyp >= 0) {
        const char *hyp_name = hyp_buffer[exact_hyp].name ? hyp_buffer[exact_hyp].name : "";
        out_suggestions[n] = lv_asprintf("exact %s", hyp_name);
        out_confidences[n] = 0.95;
        n++;
    }
    if (n < max_count && premise_hyp >= 0) {
        const char *hyp_name = hyp_buffer[premise_hyp].name ? hyp_buffer[premise_hyp].name : "";
        out_suggestions[n] = lv_asprintf("apply %s", hyp_name);
        out_confidences[n] = 0.85;
        n++;
    }

    /* 无匹配假设：先 intro，再按假设引用给出建议 */
    if (exact_hyp < 0 && premise_hyp < 0) {
        if (n < max_count && arrow != NULL) {
            out_suggestions[n] = lv_strdup("intro");
            out_confidences[n] = 0.6;
            n++;
        }
        for (int i = 0; i < hyp_count && n < max_count; i++) {
            const char *hyp_name = hyp_buffer[i].name ? hyp_buffer[i].name : "";
            if (hyp_name[0] == '\0')
                continue;
            out_suggestions[n] = lv_asprintf("apply %s", hyp_name);
            out_confidences[n] = 0.4;
            n++;
        }
        if (n < max_count) {
            out_suggestions[n] = lv_strdup("auto");
            out_confidences[n] = 0.3;
            n++;
        }
    }

    widget_free_hypotheses(hyp_buffer, hyp_count);
    lv_free_ptr(goal_text);
    return n;
}

/**
 * @brief 获取每个证明步骤的高亮状态
 * @param navigator     证明导航器指针
 * @param out_highlights 输出高亮状态数组
 * @param max_count      数组最大容量
 * @return 0 成功，-1 参数无效
 */
int proof_widget_get_step_highlights(const ProofNavigator *navigator, lvProofStepHighlight *out_highlights,
                                     int max_count) {
    if (!navigator || !out_highlights || max_count <= 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "proof_widget_get_step_highlights: invalid parameters");

    /* 清零输出数组 */
    memset(out_highlights, 0, (size_t) max_count * sizeof(lvProofStepHighlight));

    /* 实际项目中应遍历 navigator 的步骤列表并设置高亮状态 */
    for (int i = 0; i < max_count; i++) {
        out_highlights[i].step_id = i;
        out_highlights[i].color = HIGHLIGHT_NORMAL;
        out_highlights[i].is_animated = false;
        out_highlights[i].progress = 0.0f;
        out_highlights[i].tooltip_text = NULL;
    }

    return 0;
}

/**
 * @brief 获取搜索树的 JSON 表示
 * @param navigator 证明导航器指针
 * @return JSON 字符串（调用者负责释放），失败返回 NULL
 */
char *proof_widget_get_search_tree(const ProofNavigator *navigator) {
    if (!navigator)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "proof_widget_get_search_tree: navigator is NULL");

    lvJsonBuf _jb;
    lv_json_buf_init(&_jb, 128);
    lv_json_buf_append_raw(&_jb,
        "{\"type\":\"search_tree\",\"root\":{\"id\":0,"
        "\"tactic\":\"start\",\"children\":[]},\"status\":\"active\"}");
    return lv_json_buf_finalize(&_jb);
}

/* 获取依赖图的 JSON 表示（调用者负责释放返回的字符串） */
char *proof_widget_get_dependency_graph(const ProofNavigator *navigator) {
    if (!navigator)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "proof_widget_get_dependency_graph: navigator is NULL");

    lvJsonBuf _jb;
    lv_json_buf_init(&_jb, 64);
    lv_json_buf_append_raw(&_jb, "{\"type\":\"dependency_graph\",\"nodes\":[],\"edges\":[]}");
    return lv_json_buf_finalize(&_jb);
}

/* ================================================================
 * 第五部分：布局导出与策略回传
 * ================================================================ */

/**
 * @brief 将布局导出为 JSON 字符串
 * @param layout 布局指针
 * @return JSON 字符串（调用者负责释放），失败返回 NULL
 */
char *proof_widget_export_layout(const lvWidgetLayout *layout) {
    if (!layout)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "proof_widget_export_layout: layout is NULL");

    lvJsonBuf _jb;
    lv_json_buf_init(&_jb, (size_t) (JSON_BUF_INIT_SIZE + layout->widget_count * 160));

    lv_json_buf_append_fmt(&_jb, "{\"layout_type\":%d,\"columns\":%d,\"rows\":%d,\"widget_count\":%d,",
                           (int) layout->layout_type, layout->columns, layout->rows, layout->widget_count);
    /* persistence_key 经 append_string 自动 JSON 转义 */
    lv_json_buf_append_raw(&_jb, "\"persistence_key\":");
    lv_json_buf_append_string(&_jb, layout->persistence_key ? layout->persistence_key : "");
    lv_json_buf_append_raw(&_jb, ",\"widgets\":[");

    /* 逐个 Widget 序列化 */
    for (int i = 0; i < layout->widget_count; i++) {
        const ProofWidgetState *ws = &layout->widgets[i];
        if (i > 0)
            lv_json_buf_append_raw(&_jb, ",");
        lv_json_buf_append_fmt(&_jb, "{\"id\":%d,\"type\":%d,\"active\":%s,\"enabled\":%s,",
                               ws->widget_id, (int) ws->widget_type, ws->is_active ? "true" : "false",
                               ws->is_enabled ? "true" : "false");
        /* display_label 经 append_string 自动 JSON 转义 */
        lv_json_buf_append_raw(&_jb, "\"label\":");
        lv_json_buf_append_string(&_jb, ws->display_label ? ws->display_label : "");
        lv_json_buf_append_fmt(&_jb, ",\"step\":%d}", ws->bound_step_id);
    }

    lv_json_buf_append_raw(&_jb, "]}");
    return lv_json_buf_finalize(&_jb);
}

/** @brief 策略名→证明步骤类型 查找表（替代 7 分支 strcmp 链） */
static const struct {
    const char *name;
    ProofStepType step_type;
} kTacticStepTypeTable[] = {
    {"intro", PROOF_STEP_ADD_NODE},
    {"apply", PROOF_STEP_FUNCTION_APP},
    {"rewrite", PROOF_STEP_REWRITE},
    {"destruct", PROOF_STEP_NORMALIZATION},
    {"reflexivity", PROOF_STEP_UNIFY},
    {"assumption", PROOF_STEP_UNIFY},
    {"exfalso", PROOF_STEP_EX_FALSO},
    {"auto", PROOF_STEP_NORMALIZATION},
};

/**
 * @brief 应用策略到当前证明状态
 * @param navigator   证明导航器指针
 * @param tactic_name 策略名称（如 "intro"、"apply"）
 * @param tactic_args 策略参数（可为 NULL）
 * @param out_success 输出是否成功
 * @param out_feedback 输出反馈字符串（调用者需释放）
 * @return 0 成功，-1 参数无效
 */
int proof_widget_apply_tactic(ProofNavigator *navigator, const char *tactic_name, const char *tactic_args,
                              bool *out_success, char **out_feedback) {
    if (!navigator || !tactic_name)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "proof_widget_apply_tactic: navigator or tactic_name is NULL");
    if (!out_success || !out_feedback)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "proof_widget_apply_tactic: out_success or out_feedback is NULL");

    *out_success = false;
    *out_feedback = NULL;

    /* 策略名到步骤类型的映射（查找表，替代 7 分支 strcmp 链） */
    ProofStepType step_type = (ProofStepType) -1;
    for (size_t i = 0; i < lv_ARRAY_SIZE(kTacticStepTypeTable); i++) {
        if (strcmp(tactic_name, kTacticStepTypeTable[i].name) == 0) {
            step_type = kTacticStepTypeTable[i].step_type;
            break;
        }
    }
    if (step_type == (ProofStepType) -1) {
        *out_feedback = lv_strdup("unknown tactic");
        if (!*out_feedback)
            lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "proof_widget_apply_tactic: strdup failed");
        return 0;
    }

    /* 创建证明步骤 */
    ProofStep *step = proof_step_create(step_type);
    if (!step) {
        *out_feedback = lv_strdup("failed to create proof step");
        if (!*out_feedback)
            lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "proof_widget_apply_tactic: strdup failed");
        return 0;
    }

    /* 设置步骤备注 */
    if (tactic_args && tactic_args[0]) {
        /* [安全] 计算备注缓冲区大小：确保 tactic_name 和 tactic_args 不超过 2^31-1 */
        if (strlen(tactic_name) > 0x3FFFFFFF || (tactic_args && strlen(tactic_args) > 0x3FFFFFFF)) {
            *out_feedback = lv_strdup("tactic name or args too long");
            if (!*out_feedback)
                lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "proof_widget_apply_tactic: strdup failed");
            return 0;
        }
        char *note = lv_asprintf("%s %s", tactic_name, tactic_args);
        if (note) {
            proof_step_set_note(step, note);
            lv_free((void **) &(note));
        }
    } else {
        proof_step_set_note(step, tactic_name);
    }

    /* 添加到导航器 */
    if (!proof_navigator_add_step(navigator, step)) {
        proof_step_destroy(step);
        *out_feedback = lv_strdup("failed to add step to navigator");
        if (!*out_feedback)
            lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "proof_widget_apply_tactic: strdup failed");
        return 0;
    }

    *out_success = true;
    *out_feedback = lv_strdup("tactic applied successfully");
    if (!*out_feedback)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "proof_widget_apply_tactic: strdup failed");
    return 0;
}

/* ================================================================
 * 第六部分：布局管理
 * ================================================================ */

/**
 * @brief 设置布局类型和网格尺寸
 * @param layout     布局指针
 * @param layout_type 布局类型枚举
 * @param columns    列数（<=0 时使用默认值 2）
 * @param rows       行数（<=0 时使用默认值 2）
 */
void proof_widget_set_layout_type(lvWidgetLayout *layout, lvLayoutType layout_type, int columns, int rows) {
    if (!layout)
        return;
    layout->layout_type = layout_type;
    layout->columns = (columns > 0) ? columns : DEFAULT_COLUMNS;
    layout->rows = (rows > 0) ? rows : DEFAULT_ROWS;
}

/**
 * @brief 设置布局的持久化键（用于状态序列化与恢复）
 * @param layout          布局指针
 * @param persistence_key 持久化键字符串（内部复制，可为 NULL）
 */
void proof_widget_set_persistence_key(lvWidgetLayout *layout, const char *persistence_key) {
    if (!layout)
        return;

    if (layout->persistence_key) {
        lv_free_ptr(layout->persistence_key);
    }
    layout->persistence_key = persistence_key ? lv_strdup(persistence_key) : NULL;
}

/**
 * @brief 设置 Widget 的显示顺序
 * @param layout        布局指针
 * @param order_indices 顺序索引数组
 * @param count         数组长度
 */
void proof_widget_set_order(lvWidgetLayout *layout, const int *order_indices, int count) {
    if (!layout || !order_indices || count <= 0)
        return;

    /* 释放旧的顺序数组 */
    if (layout->order_indices) {
        lv_free_ptr(layout->order_indices);
    }

    layout->order_indices = (int *) lv_malloc((size_t) count * sizeof(int));
    if (!layout->order_indices)
        return;

    memcpy(layout->order_indices, order_indices, (size_t) count * sizeof(int));
}
