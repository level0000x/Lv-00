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

#include "lv00/proof_widget.h"
#include "lv00/proof.h"
#include "lv00/lv00_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "lv00/lv00_utils.h"

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

/* 创建 Widget 布局，分配初始容量 */
Lv00WidgetLayout *proof_widget_init(int layout_capacity) {
    if (layout_capacity <= 0) {
        layout_capacity = 8;
    }

    Lv00WidgetLayout *layout = (Lv00WidgetLayout *)lv00_malloc(sizeof(Lv00WidgetLayout));
    if (!layout) return NULL;

    layout->widgets = (ProofWidgetState *)lv00_calloc((size_t)layout_capacity,
                                                       sizeof(ProofWidgetState));
    if (!layout->widgets) {
        lv00_free_ptr(layout);
        return NULL;
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
void proof_widget_destroy(Lv00WidgetLayout *layout) {
    if (!layout) return;

    /* 释放每个 widget 的动态字符串 */
    for (int i = 0; i < layout->widget_count; i++) {
        ProofWidgetState *ws = &layout->widgets[i];
        if (ws->display_label) {
            lv00_free_ptr(ws->display_label);
        }
        if (ws->interaction_data) {
            lv00_free_ptr(ws->interaction_data);
        }
    }

    lv00_free_ptr(layout->widgets);
    lv00_free_ptr(layout->order_indices);
    lv00_free_ptr(layout->persistence_key);
    lv00_free_ptr(layout);
}

/* ================================================================
 * 第二部分：Widget 注册与更新
 * ================================================================ */

/* 注册新 Widget 到布局中，返回 widget_id；失败返回 -1 */
int proof_widget_register(Lv00WidgetLayout *layout, ProofWidgetType widget_type,
                          const char *label, int bound_step) {
    if (!layout) return -1;

    /* 容量不足时倍增扩容 */
    if (layout->widget_count >= layout->widget_capacity) {
        int new_cap = layout->widget_capacity * LV00_ARRAY_GROWTH_FACTOR;
        ProofWidgetState *new_arr = (ProofWidgetState *)lv00_realloc(
            layout->widgets, (size_t)new_cap * sizeof(ProofWidgetState));
        if (!new_arr) return -1;

        /* 清零新增部分 */
        memset(new_arr + layout->widget_capacity, 0,
               (size_t)(new_cap - layout->widget_capacity) * sizeof(ProofWidgetState));
        layout->widgets = new_arr;
        layout->widget_capacity = new_cap;
    }

    int id = layout->widget_count;
    ProofWidgetState *ws = &layout->widgets[id];
    ws->widget_id = id;
    ws->widget_type = widget_type;
    ws->is_active = false;
    ws->is_enabled = true;
    ws->display_label = label ? lv00_strdup(label) : NULL;
    ws->bound_step_id = bound_step;
    ws->interaction_data = NULL;

    layout->widget_count++;
    return id;
}

/* 更新已有 Widget 的状态 */
int proof_widget_update(Lv00WidgetLayout *layout, int widget_id, bool is_active,
                        bool is_enabled, const char *display_label,
                        int bound_step_id, const char *interaction_json) {
    if (!layout) return -1;
    if (widget_id < 0 || widget_id >= layout->widget_count) return -1;

    ProofWidgetState *ws = &layout->widgets[widget_id];
    ws->is_active = is_active;
    ws->is_enabled = is_enabled;
    ws->bound_step_id = bound_step_id;

    /* 更新显示标签（重新分配） */
    if (display_label) {
        if (ws->display_label) lv00_free_ptr(ws->display_label);
        ws->display_label = lv00_strdup(display_label);
    }

    /* 更新交互数据 JSON（重新分配） */
    if (interaction_json) {
        if (ws->interaction_data) lv00_free_ptr(ws->interaction_data);
        ws->interaction_data = lv00_strdup(interaction_json);
    }

    return 0;
}

/* ================================================================
 * 第三部分：证明状态查询
 * ================================================================ */

/* 从 ProofNavigator 获取当前证明目标，填充 out_goal */
int proof_widget_get_goal(const ProofNavigator *navigator, Lv00GoalDisplay *out_goal) {
    if (!navigator || !out_goal) return -1;

    /* 初始化输出结构 */
    memset(out_goal, 0, sizeof(Lv00GoalDisplay));
    out_goal->is_solved = false;
    out_goal->depth = 0;

    /* 实际项目中应从 navigator 查询当前目标；
     * 此处分配默认文本作为桩实现 */
    out_goal->goal_text = lv00_strdup("no goal available");
    if (!out_goal->goal_text) return -1;

    return 0;
}

/* 从 ProofNavigator 获取假设列表 */
int proof_widget_get_hypotheses(const ProofNavigator *navigator,
                                Lv00HypothesisEntry *out_hypotheses, int max_count) {
    if (!navigator || !out_hypotheses || max_count <= 0) return -1;

    /* 清零输出数组 */
    memset(out_hypotheses, 0, (size_t)max_count * sizeof(Lv00HypothesisEntry));

    /* 实际项目中应遍历 navigator 的假设集合；
     * 此处返回 0 条假设（由上层根据实际数据填充） */
    return 0;
}

/* 释放 GoalDisplay 内部动态分配的资源 */
void goal_display_free(Lv00GoalDisplay *goal) {
    if (!goal) return;

    if (goal->goal_text) {
        lv00_free_ptr(goal->goal_text);
        goal->goal_text = NULL;
    }

    /* 释放假设条目的字符串 */
    if (goal->hypotheses) {
        for (int i = 0; i < goal->hyp_count; i++) {
            Lv00HypothesisEntry *he = &goal->hypotheses[i];
            if (he->name) lv00_free_ptr(he->name);
            if (he->type_text) lv00_free_ptr(he->type_text);
            if (he->value_text) lv00_free_ptr(he->value_text);
        }
        lv00_free_ptr(goal->hypotheses);
        goal->hypotheses = NULL;
    }

    /* 释放上下文项 */
    if (goal->context_terms) {
        for (int i = 0; i < goal->context_count; i++) {
            if (goal->context_terms[i]) {
                lv00_free_ptr(goal->context_terms[i]);
            }
        }
        lv00_free_ptr(goal->context_terms);
        goal->context_terms = NULL;
    }

    goal->hyp_count = 0;
    goal->context_count = 0;
}

/* ================================================================
 * 第四部分：智能推荐与可视化
 * ================================================================ */

/* 基于当前证明状态建议可用策略 */
int proof_widget_suggest_tactic(const ProofNavigator *navigator,
                                char **out_suggestions, double *out_confidences,
                                int max_count) {
    if (!navigator || !out_suggestions || !out_confidences || max_count <= 0) return -1;

    /* 初始化输出 */
    for (int i = 0; i < max_count; i++) {
        out_suggestions[i] = NULL;
        out_confidences[i] = 0.0;
    }

    /* 实际项目中应调用策略推荐引擎；
     * 此处提供示例建议 */
    if (max_count >= 1) {
        out_suggestions[0] = lv00_strdup("reflexivity");
        out_confidences[0] = 0.9;
    }
    if (max_count >= 2) {
        out_suggestions[1] = lv00_strdup("congruence");
        out_confidences[1] = 0.7;
    }
    if (max_count >= 3) {
        out_suggestions[2] = lv00_strdup("angle_bisector");
        out_confidences[2] = 0.5;
    }

    return 0;
}

/* 获取每个证明步骤的高亮状态 */
int proof_widget_get_step_highlights(const ProofNavigator *navigator,
                                     Lv00ProofStepHighlight *out_highlights,
                                     int max_count) {
    if (!navigator || !out_highlights || max_count <= 0) return -1;

    /* 清零输出数组 */
    memset(out_highlights, 0, (size_t)max_count * sizeof(Lv00ProofStepHighlight));

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

/* 获取搜索树的 JSON 表示（调用者负责释放返回的字符串） */
char *proof_widget_get_search_tree(const ProofNavigator *navigator) {
    if (!navigator) return NULL;

    size_t cap = JSON_BUF_INIT_SIZE;
    char *buf = (char *)lv00_malloc(cap);
    if (!buf) return NULL;

    int n = snprintf(buf, cap,
        "{\"type\":\"search_tree\",\"root\":{\"id\":0,"
        "\"tactic\":\"start\",\"children\":[]},\"status\":\"active\"}");
    if (n < 0 || (size_t)n >= cap) {
        cap = (size_t)n + 1;
        char *nb = (char *)lv00_realloc(buf, cap);
        if (!nb) { lv00_free_ptr(buf); return NULL; }
        buf = nb;
        snprintf(buf, cap,
            "{\"type\":\"search_tree\",\"root\":{\"id\":0,"
            "\"tactic\":\"start\",\"children\":[]},\"status\":\"active\"}");
    }

    return buf;
}

/* 获取依赖图的 JSON 表示（调用者负责释放返回的字符串） */
char *proof_widget_get_dependency_graph(const ProofNavigator *navigator) {
    if (!navigator) return NULL;

    size_t cap = JSON_BUF_INIT_SIZE;
    char *buf = (char *)lv00_malloc(cap);
    if (!buf) return NULL;

    int n = snprintf(buf, cap,
        "{\"type\":\"dependency_graph\",\"nodes\":[],\"edges\":[]}");
    if (n < 0 || (size_t)n >= cap) {
        cap = (size_t)n + 1;
        char *nb = (char *)lv00_realloc(buf, cap);
        if (!nb) { lv00_free_ptr(buf); return NULL; }
        buf = nb;
        snprintf(buf, cap,
            "{\"type\":\"dependency_graph\",\"nodes\":[],\"edges\":[]}");
    }

    return buf;
}

/* ================================================================
 * 第五部分：布局导出与策略回传
 * ================================================================ */

/* 将布局导出为 JSON 字符串（调用者负责释放） */
char *proof_widget_export_layout(const Lv00WidgetLayout *layout) {
    if (!layout) return NULL;

    /* 估算缓冲区：基础 JSON + 每个 widget 约 160 字节 */
    size_t cap = (size_t)(JSON_BUF_INIT_SIZE + layout->widget_count * 160);
    char *buf = (char *)lv00_malloc(cap);
    if (!buf) return NULL;

    size_t pos = 0;
    int written;

    /* JSON 头 */
    written = snprintf(buf + pos, cap - pos,
        "{\"layout_type\":%d,\"columns\":%d,\"rows\":%d,"
        "\"widget_count\":%d,\"persistence_key\":\"%s\",\"widgets\":[",
        (int)layout->layout_type, layout->columns, layout->rows,
        layout->widget_count,
        layout->persistence_key ? layout->persistence_key : "");
    if (written > 0) pos += (size_t)written;

    /* 逐个 Widget 序列化 */
    for (int i = 0; i < layout->widget_count; i++) {
        const ProofWidgetState *ws = &layout->widgets[i];

        /* 确保容量充足 */
        if (pos + 256 > cap) {
            cap *= 2;
            char *nb = (char *)lv00_realloc(buf, cap);
            if (!nb) { lv00_free_ptr(buf); return NULL; }
            buf = nb;
        }

        if (i > 0) {
            buf[pos++] = ',';
        }
        written = snprintf(buf + pos, cap - pos,
            "{\"id\":%d,\"type\":%d,\"active\":%s,\"enabled\":%s,"
            "\"label\":\"%s\",\"step\":%d}",
            ws->widget_id, (int)ws->widget_type,
            ws->is_active ? "true" : "false",
            ws->is_enabled ? "true" : "false",
            ws->display_label ? ws->display_label : "",
            ws->bound_step_id);
        if (written > 0) pos += (size_t)written;
    }

    /* JSON 尾 */
    if (pos + 8 > cap) {
        cap += 8;
        char *nb = (char *)lv00_realloc(buf, cap);
        if (!nb) { lv00_free_ptr(buf); return NULL; }
        buf = nb;
    }
    snprintf(buf + pos, cap - pos, "]}");

    return buf;
}

/* 应用策略到当前证明状态 */
int proof_widget_apply_tactic(ProofNavigator *navigator, const char *tactic_name,
                              const char *tactic_args, bool *out_success,
                              char **out_feedback) {
    if (!navigator || !tactic_name) return -1;
    if (!out_success || !out_feedback) return -1;

    *out_success = false;
    *out_feedback = NULL;

    /* 策略名到步骤类型的映射 */
    ProofStepType step_type;
    if (strcmp(tactic_name, "intro") == 0) {
        step_type = PROOF_STEP_ADD_NODE;
    } else if (strcmp(tactic_name, "apply") == 0) {
        step_type = PROOF_STEP_FUNCTION_APP;
    } else if (strcmp(tactic_name, "rewrite") == 0) {
        step_type = PROOF_STEP_REWRITE;
    } else if (strcmp(tactic_name, "destruct") == 0) {
        step_type = PROOF_STEP_NORMALIZATION;
    } else if (strcmp(tactic_name, "reflexivity") == 0 ||
               strcmp(tactic_name, "assumption") == 0) {
        step_type = PROOF_STEP_UNIFY;
    } else if (strcmp(tactic_name, "exfalso") == 0) {
        step_type = PROOF_STEP_EX_FALSO;
    } else if (strcmp(tactic_name, "auto") == 0) {
        step_type = PROOF_STEP_NORMALIZATION;
    } else {
        *out_feedback = lv00_strdup("unknown tactic");
        return (*out_feedback) ? 0 : -1;
    }

    /* 创建证明步骤 */
    ProofStep *step = proof_step_create(step_type);
    if (!step) {
        *out_feedback = lv00_strdup("failed to create proof step");
        return (*out_feedback) ? 0 : -1;
    }

    /* 设置步骤备注 */
    if (tactic_args && tactic_args[0]) {
        int buf_size = (int)strlen(tactic_name) + (int)strlen(tactic_args) + 4;
        char *note = (char *)lv00_malloc((size_t)buf_size);
        if (note) {
            snprintf(note, (size_t)buf_size, "%s %s", tactic_name, tactic_args);
            proof_step_set_note(step, note);
            lv00_free((void **)&note);
        }
    } else {
        proof_step_set_note(step, tactic_name);
    }

    /* 添加到导航器 */
    if (!proof_navigator_add_step(navigator, step)) {
        proof_step_destroy(step);
        *out_feedback = lv00_strdup("failed to add step to navigator");
        return (*out_feedback) ? 0 : -1;
    }

    *out_success = true;
    *out_feedback = lv00_strdup("tactic applied successfully");
    return (*out_feedback) ? 0 : -1;
}

/* ================================================================
 * 第六部分：布局管理
 * ================================================================ */

/* 设置布局类型和网格尺寸 */
void proof_widget_set_layout_type(Lv00WidgetLayout *layout, Lv00LayoutType layout_type,
                                  int columns, int rows) {
    if (!layout) return;
    layout->layout_type = layout_type;
    layout->columns = (columns > 0) ? columns : DEFAULT_COLUMNS;
    layout->rows = (rows > 0) ? rows : DEFAULT_ROWS;
}

/* 设置布局的持久化键（用于状态序列化与恢复） */
void proof_widget_set_persistence_key(Lv00WidgetLayout *layout,
                                      const char *persistence_key) {
    if (!layout) return;

    if (layout->persistence_key) {
        lv00_free_ptr(layout->persistence_key);
    }
    layout->persistence_key = persistence_key ? lv00_strdup(persistence_key) : NULL;
}

/* 设置 Widget 的显示顺序 */
void proof_widget_set_order(Lv00WidgetLayout *layout, const int *order_indices,
                            int count) {
    if (!layout || !order_indices || count <= 0) return;

    /* 释放旧的顺序数组 */
    if (layout->order_indices) {
        lv00_free_ptr(layout->order_indices);
    }

    layout->order_indices = (int *)lv00_malloc((size_t)count * sizeof(int));
    if (!layout->order_indices) return;

    memcpy(layout->order_indices, order_indices, (size_t)count * sizeof(int));
}
