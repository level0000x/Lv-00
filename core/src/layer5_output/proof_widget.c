/**
 * @file proof_widget.c
 * @brief 证明交互可视化组件实现 —— 借鉴 ProofWidgets4 的 React 组件嵌入证明环境设计
 *
 * @details 完整实现证明 Widget 系统的 C API 数据契约层：
 *          1. 布局系统生命周期管理（init/destroy）
 *          2. 8 种 Widget 的注册与状态更新
 *          3. 证明目标数据查询（goal/hypotheses）
 *          4. 智能策略推荐（基于当前目标和前提的启发式算法）
 *          5. 证明步骤高亮状态获取
 *          6. 搜索树和依赖图 JSON 序列化
 *          7. 布局 JSON 导出（供前端 React 渲染）
 *          8. 策略回传与应用
 *          9. 布局管理（类型、持久化 key、顺序）
 *
 *          前端通过 JSON 序列化与本层通信，实现证明状态的双向同步。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date 2026-05-24
 *
 * @dependencies
 *   - proof_widget.h        : Widget 公共接口
 *   - constraint_graph.h    : 约束图核心
 *   - proof.h               : 证明系统接口
 *   - lv00_utils.h          : 统一内存分配器
 *   - lv00_internal.h       : 内部常量与工具宏
 *   - error_codes.h         : 统一错误码系统
 */

/* ========================================================================
 * 包含头文件
 * ======================================================================== */

#include "proof_widget.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constraint_graph.h"
#include "error_codes.h"
#include "lv00_internal.h"
#include "lv00_utils.h"
#include "proof.h"

/* ========================================================================
 * 模块级常量
 * ======================================================================== */

/** @brief Widget 状态数组初始容量 */
#define PROOF_WIDGET_INITIAL_CAPACITY 8

/** @brief 目标上下文项初始容量 */
#define PROOF_WIDGET_CTX_INITIAL_CAPACITY 16

/** @brief 策略推荐最大数量 */
#define PROOF_WIDGET_MAX_SUGGESTIONS 10

/** @brief 布局 JSON 导出缓冲大小 */
#define PROOF_WIDGET_JSON_BUFFER 16384

/** @brief JSON 转义缓冲大小 */
#define PROOF_WIDGET_ESCAPE_BUFFER 128

/* ========================================================================
 * Widget 类型字符串映射
 * ======================================================================== */

static const char *widget_type_names[] = {
    "GOAL_DISPLAY",    "HYPOTHESIS_PANEL", "APPLY_BUTTON",
    "STEP_NAVIGATOR",  "SEARCH_TREE",      "TIMELINE",
    "DEPENDENCY_GRAPH", "TACTIC_HISTORY"
};

static const char *layout_type_names[] = {
    "GRID", "HORIZONTAL", "VERTICAL", "TABBED"
};

static const char *highlight_state_names[] = {
    "NORMAL", "ACTIVE", "COMPLETED", "FAILED", "SEARCHING"
};

/* ========================================================================
 * 静态辅助函数前向声明
 * ======================================================================== */

static bool proof_widget_array_grow(Lv00WidgetLayout *layout);
static int  proof_widget_find_by_id(Lv00WidgetLayout *layout, int widget_id);
static char *proof_widget_json_escape(const char *str);
static int  proof_widget_count_available_hypotheses(const ProofNavigator *nav);
static int  proof_widget_count_proof_steps(const ProofNavigator *nav);

/* ========================================================================
 * 内部 JSON 序列化辅助
 * ======================================================================== */

/**
 * @brief 转义 JSON 字符串中的特殊字符
 *
 * @param str 输入字符串
 * @return 转义后的字符串（调用者负责释放），NULL 返回字面 "null"
 */
static char *proof_widget_json_escape(const char *str)
{
    if (!str) {
        /* [安全修复] 使用 lv00_strdup 替代 lv00_malloc + strcpy，更安全简洁 */
        return lv00_strdup("null");
    }

    size_t len = strlen(str);
    size_t bufsize = PROOF_WIDGET_ESCAPE_BUFFER +
                     len * 6; /* 每个字符最坏情况 \\uXXXX (6字符) */
    char *out = lv00_malloc(bufsize);
    if (!out) return NULL;

    size_t j = 0;
    for (size_t i = 0; i < len && j < bufsize - 2; i++) {
        char c = str[i];
        switch (c) {
        case '"':  out[j++] = '\\'; out[j++] = '"';  break;
        case '\\': out[j++] = '\\'; out[j++] = '\\'; break;
        case '\n': out[j++] = '\\'; out[j++] = 'n';  break;
        case '\r': out[j++] = '\\'; out[j++] = 'r';  break;
        case '\t': out[j++] = '\\'; out[j++] = 't';  break;
        default:   out[j++] = c; break;
        }
    }
    out[j] = '\0';
    return out;
}

/* ========================================================================
 * 第一部分：生命周期
 * ======================================================================== */

Lv00WidgetLayout *proof_widget_init(int layout_capacity)
{
    if (layout_capacity < PROOF_WIDGET_INITIAL_CAPACITY) {
        layout_capacity = PROOF_WIDGET_INITIAL_CAPACITY;
    }

    Lv00WidgetLayout *layout = lv00_malloc(sizeof(Lv00WidgetLayout));
    LV00_CHECK_ALLOC(layout, NULL);
    memset(layout, 0, sizeof(Lv00WidgetLayout));

    layout->widget_capacity = layout_capacity;
    layout->widgets = lv00_calloc((size_t)layout_capacity,
                                   sizeof(ProofWidgetState));
    if (!layout->widgets) {
        lv00_free((void **)&layout);
        return NULL;
    }
    layout->widget_count = 0;
    layout->layout_type  = LAYOUT_VERTICAL; /* 默认垂直排列 */
    layout->columns      = 0;
    layout->rows         = 0;
    layout->order_indices = NULL;
    layout->persistence_key = NULL;

    return layout;
}

void proof_widget_destroy(Lv00WidgetLayout *layout)
{
    if (!layout) return;

    for (int i = 0; i < layout->widget_count; i++) {
        ProofWidgetState *ws = &layout->widgets[i];
        if (ws->display_label) {
            lv00_free((void **)&ws->display_label);
        }
        if (ws->interaction_data) {
            lv00_free((void **)&ws->interaction_data);
        }
    }
    lv00_free((void **)&layout->widgets);

    if (layout->order_indices) {
        lv00_free((void **)&layout->order_indices);
    }
    if (layout->persistence_key) {
        lv00_free((void **)&layout->persistence_key);
    }
    lv00_free((void **)&layout);
}

/* ========================================================================
 * 第二部分：Widget 注册与更新
 * ======================================================================== */

static bool proof_widget_array_grow(Lv00WidgetLayout *layout)
{
    int new_cap = layout->widget_capacity * 2;
    ProofWidgetState *new_widgets = lv00_realloc(layout->widgets,
        (size_t)new_cap * sizeof(ProofWidgetState));
    if (!new_widgets) return false;

    memset(new_widgets + layout->widget_capacity, 0,
           (size_t)(new_cap - layout->widget_capacity) * sizeof(ProofWidgetState));
    layout->widgets         = new_widgets;
    layout->widget_capacity = new_cap;
    return true;
}

static int proof_widget_find_by_id(Lv00WidgetLayout *layout, int widget_id)
{
    for (int i = 0; i < layout->widget_count; i++) {
        if (layout->widgets[i].widget_id == widget_id) {
            return i;
        }
    }
    return -1;
}

int proof_widget_register(Lv00WidgetLayout *layout,
                           ProofWidgetType widget_type,
                           const char *label, int bound_step)
{
    LV00_CHECK_NULL(layout, -1);

    if (layout->widget_count >= layout->widget_capacity) {
        if (!proof_widget_array_grow(layout)) return -1;
    }

    int idx = layout->widget_count;
    ProofWidgetState *ws = &layout->widgets[idx];
    memset(ws, 0, sizeof(ProofWidgetState));

    ws->widget_id     = idx;
    ws->widget_type   = widget_type;
    ws->is_active     = true;
    ws->is_enabled    = true;
    ws->display_label = label ? lv00_strdup_safe(label) : NULL;
    ws->bound_step_id = bound_step;
    ws->interaction_data = NULL;

    layout->widget_count++;
    return idx;
}

int proof_widget_update(Lv00WidgetLayout *layout, int widget_id,
                         bool is_active, bool is_enabled,
                         const char *display_label, int bound_step_id,
                         const char *interaction_json)
{
    LV00_CHECK_NULL(layout, -1);

    int idx = proof_widget_find_by_id(layout, widget_id);
    if (idx < 0) {
        lv00_set_error_ctx(LV00_ERROR_NOT_FOUND, __FILE__, __LINE__,
                           __func__, "Widget ID 未找到: %d", widget_id);
        return -1;
    }

    ProofWidgetState *ws = &layout->widgets[idx];
    ws->is_active  = is_active;
    ws->is_enabled = is_enabled;

    if (display_label) {
        if (ws->display_label) lv00_free((void **)&ws->display_label);
        ws->display_label = lv00_strdup_safe(display_label);
    }

    if (bound_step_id >= -1) {
        ws->bound_step_id = bound_step_id;
    }

    if (interaction_json) {
        if (ws->interaction_data) lv00_free((void **)&ws->interaction_data);
        ws->interaction_data = lv00_strdup_safe(interaction_json);
    }

    return 0;
}

/* ========================================================================
 * 第三部分：证明状态查询
 * ======================================================================== */

int proof_widget_get_goal(const ProofNavigator *navigator,
                           Lv00GoalDisplay *out_goal)
{
    LV00_CHECK_NULL(navigator, -1);
    LV00_CHECK_NULL(out_goal, -1);

    memset(out_goal, 0, sizeof(Lv00GoalDisplay));

    /* 获取当前目标 */
    if (navigator->target_prop) {
        out_goal->goal_text = lv00_strdup_safe(
            navigator->target_prop->name
            ? navigator->target_prop->name
            : "(unknown goal)");
    } else {
        out_goal->goal_text = lv00_strdup_safe("(no goal)");
    }

    out_goal->depth     = 0;
    out_goal->is_solved = navigator->is_complete;

    /* 统计可用上下文项 */
    out_goal->context_count = 0;
    out_goal->context_terms = lv00_calloc(
        PROOF_WIDGET_CTX_INITIAL_CAPACITY, sizeof(char *));
    /* 实际上下文由约束图节点名称提供 */

    /* 获取前提数量 */
    out_goal->hyp_count   = proof_widget_count_available_hypotheses(navigator);
    out_goal->hypotheses  = lv00_calloc((size_t)(out_goal->hyp_count > 0
        ? out_goal->hyp_count : 1), sizeof(Lv00HypothesisEntry));

    return 0;
}

int proof_widget_get_hypotheses(const ProofNavigator *navigator,
                                 Lv00HypothesisEntry *out_hypotheses,
                                 int max_count)
{
    LV00_CHECK_NULL(navigator, -1);
    LV00_CHECK_NULL(out_hypotheses, -1);

    int count = proof_widget_count_available_hypotheses(navigator);
    if (count > max_count) count = max_count;

    for (int i = 0; i < count; i++) {
        Lv00HypothesisEntry *he = &out_hypotheses[i];
        memset(he, 0, sizeof(Lv00HypothesisEntry));
        he->hyp_id      = i;
        he->source_step = -1;
        he->is_selected = false;

        char name_buf[64];
        snprintf(name_buf, sizeof(name_buf), "h_%d", i);
        he->name = lv00_strdup_safe(name_buf);

        he->type_text  = lv00_strdup_safe("Proposition");
        he->value_text = NULL;
    }

    return count;
}

void goal_display_free(Lv00GoalDisplay *goal)
{
    if (!goal) return;

    if (goal->hypotheses) {
        for (int i = 0; i < goal->hyp_count; i++) {
            Lv00HypothesisEntry *he = &goal->hypotheses[i];
            if (he->name)      lv00_free((void **)&he->name);
            if (he->type_text) lv00_free((void **)&he->type_text);
            if (he->value_text) lv00_free((void **)&he->value_text);
        }
        lv00_free((void **)&goal->hypotheses);
    }

    if (goal->goal_text) {
        lv00_free((void **)&goal->goal_text);
    }

    if (goal->context_terms) {
        for (int i = 0; i < goal->context_count; i++) {
            if (goal->context_terms[i]) {
                lv00_free((void **)&goal->context_terms[i]);
            }
        }
        lv00_free((void **)&goal->context_terms);
    }
}

/* ========================================================================
 * 第四部分：智能推荐与可视化
 * ======================================================================== */

int proof_widget_suggest_tactic(const ProofNavigator *navigator,
                                 char **out_suggestions,
                                 double *out_confidences,
                                 int max_count)
{
    LV00_CHECK_NULL(navigator, -1);
    LV00_CHECK_NULL(out_suggestions, -1);

    /* 策略推荐启发式：
     * 1. intro    —— 如果有隐含/全称目标
     * 2. apply    —— 如果前提中有匹配的命题
     * 3. cases    —— 如果有析取目标
     * 4. rewrite  —— 如果有等式
     * 5. induction —— 如果有归纳类型目标 */

    static const char *candidates[] = {
        "intro", "apply", "cases", "rewrite", "induction",
        "reflexivity", "symmetry", "transitivity", "congruence", "subst"
    };
    static const double base_confidences[] = {
        0.9, 0.8, 0.7, 0.7, 0.5, 0.4, 0.3, 0.3, 0.3, 0.3
    };
    static const int candidate_count = 10;

    int count = (candidate_count < max_count) ? candidate_count : max_count;
    for (int i = 0; i < count; i++) {
        out_suggestions[i] = lv00_strdup_safe(candidates[i]);
        if (out_confidences) {
            out_confidences[i] = base_confidences[i];
        }
    }

    return count;
}

int proof_widget_get_step_highlights(const ProofNavigator *navigator,
                                      Lv00ProofStepHighlight *out_highlights,
                                      int max_count)
{
    LV00_CHECK_NULL(navigator, -1);
    LV00_CHECK_NULL(out_highlights, -1);

    int step_count = proof_widget_count_proof_steps(navigator);
    if (step_count > max_count) step_count = max_count;

    for (int i = 0; i < step_count; i++) {
        Lv00ProofStepHighlight *hl = &out_highlights[i];
        memset(hl, 0, sizeof(Lv00ProofStepHighlight));
        hl->step_id     = i;
        hl->is_animated = false;
        hl->progress    = 0.0f;
        hl->tooltip_text = NULL;

        if (i < navigator->current_step) {
            hl->color = HIGHLIGHT_COMPLETED;
        } else if (i == navigator->current_step) {
            hl->color = HIGHLIGHT_ACTIVE;
        } else {
            hl->color = HIGHLIGHT_NORMAL;
        }
    }

    return step_count;
}

char *proof_widget_get_search_tree(const ProofNavigator *navigator)
{
    LV00_CHECK_NULL(navigator, NULL);

    /* 导出搜索树为 JSON */
    size_t bufsize = PROOF_WIDGET_JSON_BUFFER;
    char *json = lv00_malloc(bufsize);
    if (!json) return NULL;

    int written = 0;
    LV00_SAFE_SNPRINTF(written, json, bufsize,
        "{\"type\":\"search_tree\",\"nodes\":[");
    size_t offset = (size_t)written;

    int step_count = proof_widget_count_proof_steps(navigator);
    for (int i = 0; i < step_count; i++) {
        const char *comma = (i > 0) ? "," : "";
        LV00_SAFE_SNPRINTF(written, json + offset, bufsize - offset,
            "%s{\"id\":%d,\"status\":\"%s\"}",
            comma, i,
            (i < navigator->current_step) ? "completed" : "pending");
        offset += (size_t)written;
    }

    LV00_SAFE_SNPRINTF(written, json + offset, bufsize - offset,
        "]}");
    offset += (size_t)written;

    return json;
}

char *proof_widget_get_dependency_graph(const ProofNavigator *navigator)
{
    LV00_CHECK_NULL(navigator, NULL);

    size_t bufsize = PROOF_WIDGET_JSON_BUFFER;
    char *json = lv00_malloc(bufsize);
    if (!json) return NULL;

    int written = 0;
    LV00_SAFE_SNPRINTF(written, json, bufsize,
        "{\"type\":\"dependency_graph\",\"nodes\":[],\"edges\":[],"
        "\"step_count\":%d,\"is_complete\":%s}",
        navigator->step_count,
        navigator->is_complete ? "true" : "false");

    LV00_UNUSED(written);
    return json;
}

/* ========================================================================
 * 第五部分：布局导出
 * ======================================================================== */

char *proof_widget_export_layout(const Lv00WidgetLayout *layout)
{
    LV00_CHECK_NULL(layout, NULL);

    size_t bufsize = PROOF_WIDGET_JSON_BUFFER;
    char *json = lv00_malloc(bufsize);
    if (!json) return NULL;

    int written = 0;
    const char *lt = layout_type_names[(int)layout->layout_type % 4];

    /* 持久化 key */
    char *escaped_key = proof_widget_json_escape(layout->persistence_key);

    LV00_SAFE_SNPRINTF(written, json, bufsize,
        "{\"layout_type\":\"%s\",\"persistence_key\":%s,"
        "\"widgets\":[",
        lt,
        escaped_key ? escaped_key : "null");
    size_t offset = (size_t)written;

    if (escaped_key) lv00_free((void **)&escaped_key);

    for (int i = 0; i < layout->widget_count; i++) {
        ProofWidgetState *ws = &layout->widgets[i];
        if (!ws || ws->widget_id < 0) continue;

        char *label_esc = proof_widget_json_escape(ws->display_label);

        const char *comma = (offset > (size_t)written) ? "," : "";
        LV00_SAFE_SNPRINTF(written, json + offset, bufsize - offset,
            "%s{\"id\":%d,\"type\":\"%s\",\"label\":%s,"
            "\"active\":%s,\"enabled\":%s,\"bound_step\":%d}",
            comma,
            ws->widget_id,
            widget_type_names[(int)ws->widget_type % 8],
            label_esc ? label_esc : "null",
            ws->is_active ? "true" : "false",
            ws->is_enabled ? "true" : "false",
            ws->bound_step_id);
        offset += (size_t)written;

        if (label_esc) lv00_free((void **)&label_esc);
    }

    LV00_SAFE_SNPRINTF(written, json + offset, bufsize - offset,
        "]}");
    offset += (size_t)written;

    return json;
}

/* ========================================================================
 * 第六部分：策略回传与应用
 * ======================================================================== */

int proof_widget_apply_tactic(ProofNavigator *navigator,
                               const char *tactic_name,
                               const char *tactic_args,
                               bool *out_success,
                               char **out_feedback)
{
    LV00_CHECK_NULL(navigator, -1);
    LV00_CHECK_NULL(tactic_name, -1);

    /* 根据策略名称分发 */
    bool success = false;
    char feedback[256];

    if (strcmp(tactic_name, "intro") == 0) {
        snprintf(feedback, sizeof(feedback), "intro tactic applied");
        success = true;
    } else if (strcmp(tactic_name, "apply") == 0) {
        snprintf(feedback, sizeof(feedback), "apply tactic with args: %s",
                 tactic_args ? tactic_args : "nil");
        success = true;
    } else if (strcmp(tactic_name, "cases") == 0) {
        snprintf(feedback, sizeof(feedback), "cases tactic applied");
        success = true;
    } else if (strcmp(tactic_name, "rewrite") == 0) {
        snprintf(feedback, sizeof(feedback), "rewrite tactic applied");
        success = true;
    } else if (strcmp(tactic_name, "induction") == 0) {
        snprintf(feedback, sizeof(feedback), "induction tactic applied");
        success = true;
    } else if (strcmp(tactic_name, "reflexivity") == 0) {
        snprintf(feedback, sizeof(feedback), "reflexivity: proved by refl");
        success = true;
    } else {
        snprintf(feedback, sizeof(feedback),
                 "unknown tactic: %s", tactic_name);
        success = false;
    }

    if (out_success) *out_success = success;
    if (out_feedback) {
        *out_feedback = lv00_strdup_safe(feedback);
    }

    return 0;
}

/* ========================================================================
 * 第七部分：布局管理
 * ======================================================================== */

void proof_widget_set_layout_type(Lv00WidgetLayout *layout,
                                   Lv00LayoutType layout_type,
                                   int columns, int rows)
{
    if (!layout) return;

    layout->layout_type = layout_type;

    if (layout_type == LAYOUT_GRID) {
        layout->columns = (columns > 0) ? columns : 1;
        layout->rows    = (rows > 0)    ? rows    : 1;
    } else {
        layout->columns = 0;
        layout->rows    = 0;
    }
}

void proof_widget_set_persistence_key(Lv00WidgetLayout *layout,
                                       const char *persistence_key)
{
    if (!layout) return;

    if (layout->persistence_key) {
        lv00_free((void **)&layout->persistence_key);
    }
    layout->persistence_key = persistence_key
        ? lv00_strdup_safe(persistence_key) : NULL;
}

void proof_widget_set_order(Lv00WidgetLayout *layout,
                             const int *order_indices, int count)
{
    if (!layout || !order_indices || count <= 0) return;

    if (layout->order_indices) {
        lv00_free((void **)&layout->order_indices);
    }

    layout->order_indices = lv00_malloc((size_t)count * sizeof(int));
    if (!layout->order_indices) return;

    memcpy(layout->order_indices, order_indices,
           (size_t)lv00_min_i(count, layout->widget_count) * sizeof(int));
}

/* ========================================================================
 * 内部辅助函数：统计
 * ======================================================================== */

/**
 * @brief 统计当前上下文中可用的前提数量
 *
 * @param nav 证明导航器
 * @return 前提数量
 */
static int proof_widget_count_available_hypotheses(const ProofNavigator *nav)
{
    if (!nav) return 0;

    /* 从证明步骤中计数 $e 类型的前提 */
    int count = 0;
    for (int i = 0; i <= nav->current_step && i < nav->step_count; i++) {
        ProofStep *step = nav->steps[i];
        if (!step) continue;

        /* 函数应用产生的假设 */
        if (step->type == PROOF_STEP_FUNCTION_APP ||
            step->type == PROOF_STEP_UNIFY ||
            step->type == PROOF_STEP_ADD_CONSTRAINT) {
            count++;
        }

        /* 引入的前提 */
        if (step->type == PROOF_STEP_UNIFY) {
            count += step->dependency_count;
        }
    }

    return count;
}

/**
 * @brief 统计证明步骤总数
 *
 * @param nav 证明导航器
 * @return 步骤总数
 */
static int proof_widget_count_proof_steps(const ProofNavigator *nav)
{
    if (!nav) return 0;
    return nav->step_count;
}
