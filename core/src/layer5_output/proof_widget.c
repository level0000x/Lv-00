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
 */
int proof_widget_register(lvWidgetLayout *layout, ProofWidgetType widget_type, const char *label, int bound_step) {
    if (!layout)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "proof_widget_register: layout is NULL");

    /* 容量不足时倍增扩容 */
    if (layout->widget_count >= layout->widget_capacity) {
        if (layout->widget_capacity > INT_MAX / lv_ARRAY_GROWTH_FACTOR)
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "proof_widget_register: capacity overflow");
        int new_cap = layout->widget_capacity * lv_ARRAY_GROWTH_FACTOR;
        ProofWidgetState *new_arr =
            (ProofWidgetState *) lv_realloc(layout->widgets, (size_t) new_cap * sizeof(ProofWidgetState));
        if (!new_arr)
            lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "proof_widget_register: realloc failed");

        /* 清零新增部分 */
        memset(new_arr + layout->widget_capacity, 0,
               (size_t) (new_cap - layout->widget_capacity) * sizeof(ProofWidgetState));
        layout->widgets = new_arr;
        layout->widget_capacity = new_cap;
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
    out_goal->is_solved = false;
    out_goal->depth = 0;

    /* 实际项目中应从 navigator 查询当前目标；
     * 此处分配默认文本作为桩实现 */
    out_goal->goal_text = lv_strdup("no goal available");
    if (!out_goal->goal_text)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "proof_widget_get_goal: strdup failed");

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

    /* 实际项目中应遍历 navigator 的假设集合；
     * 此处返回 0 条假设（由上层根据实际数据填充） */
    return 0;
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
 * @return 0 成功，-1 参数无效
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

    /* 实际项目中应调用策略推荐引擎；
     * 此处提供示例建议 */
    if (max_count >= 1) {
        out_suggestions[0] = lv_strdup("reflexivity");
        out_confidences[0] = 0.9;
    }
    if (max_count >= 2) {
        out_suggestions[1] = lv_strdup("congruence");
        out_confidences[1] = 0.7;
    }
    if (max_count >= 3) {
        out_suggestions[2] = lv_strdup("angle_bisector");
        out_confidences[2] = 0.5;
    }

    return 0;
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

    size_t cap = JSON_BUF_INIT_SIZE;
    char *buf = (char *) lv_malloc(cap);
    if (!buf)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "proof_widget_get_search_tree: malloc failed");

    int n = snprintf(buf, cap,
                     "{\"type\":\"search_tree\",\"root\":{\"id\":0,"
                     "\"tactic\":\"start\",\"children\":[]},\"status\":\"active\"}");
    if (n < 0 || (size_t) n >= cap) {
        cap = (size_t) n + 1;
        char *nb = (char *) lv_realloc(buf, cap);
        if (!nb) {
            lv_free_ptr(buf);
            lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "proof_widget_get_search_tree: realloc failed");
        }
        buf = nb;
        snprintf(buf, cap,
                 "{\"type\":\"search_tree\",\"root\":{\"id\":0,"
                 "\"tactic\":\"start\",\"children\":[]},\"status\":\"active\"}");
    }

    return buf;
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

    /* 估算缓冲区：基础 JSON + 每个 widget 约 160 字节 */
    size_t cap = (size_t) (JSON_BUF_INIT_SIZE + layout->widget_count * 160);
    char *buf = (char *) lv_malloc(cap);
    if (!buf)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "proof_widget_export_layout: malloc failed");

    size_t pos = 0;
    int written;

    /* JSON 头 */
    written = snprintf(buf + pos, cap - pos,
                       "{\"layout_type\":%d,\"columns\":%d,\"rows\":%d,"
                       "\"widget_count\":%d,\"persistence_key\":\"%s\",\"widgets\":[",
                       (int) layout->layout_type, layout->columns, layout->rows, layout->widget_count,
                       layout->persistence_key ? layout->persistence_key : "");
    if (written < 0) {
        lv_free_ptr(buf);
        lv_RETURN_ERROR_NULL(lv_ERROR_INTERNAL, "proof_widget_export_layout: snprintf failed");
    }
    if ((size_t) written >= cap - pos) {
        cap = pos + (size_t) written + 1;
        char *nb = (char *) lv_realloc(buf, cap);
        if (!nb) {
            lv_free_ptr(buf);
            lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "proof_widget_export_layout: realloc failed");
        }
        buf = nb;
    }
    pos += (size_t) written;

    /* 逐个 Widget 序列化 */
    for (int i = 0; i < layout->widget_count; i++) {
        const ProofWidgetState *ws = &layout->widgets[i];

        /* 确保容量充足 */
        if (pos + 256 > cap) {
            cap *= 2;
            char *nb = (char *) lv_realloc(buf, cap);
            if (!nb) {
                lv_free_ptr(buf);
                lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "proof_widget_export_layout: realloc in loop failed");
            }
            buf = nb;
        }

        if (i > 0) {
            buf[pos++] = ',';
        }
        written =
            snprintf(buf + pos, cap - pos,
                     "{\"id\":%d,\"type\":%d,\"active\":%s,\"enabled\":%s,"
                     "\"label\":\"%s\",\"step\":%d}",
                     ws->widget_id, (int) ws->widget_type, ws->is_active ? "true" : "false",
                     ws->is_enabled ? "true" : "false", ws->display_label ? ws->display_label : "", ws->bound_step_id);
        if (written > 0)
            pos += (size_t) written;
    }

    /* JSON 尾 */
    if (pos + 8 > cap) {
        cap += 8;
        char *nb = (char *) lv_realloc(buf, cap);
        if (!nb) {
            lv_free_ptr(buf);
            lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "proof_widget_export_layout: final realloc failed");
        }
        buf = nb;
    }
    snprintf(buf + pos, cap - pos, "]}");

    return buf;
}

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
    } else if (strcmp(tactic_name, "reflexivity") == 0 || strcmp(tactic_name, "assumption") == 0) {
        step_type = PROOF_STEP_UNIFY;
    } else if (strcmp(tactic_name, "exfalso") == 0) {
        step_type = PROOF_STEP_EX_FALSO;
    } else if (strcmp(tactic_name, "auto") == 0) {
        step_type = PROOF_STEP_NORMALIZATION;
    } else {
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
        int buf_size = (int) (strlen(tactic_name) + (tactic_args ? strlen(tactic_args) : 0) + 4);
        char *note = (char *) lv_malloc((size_t) buf_size);
        if (note) {
            snprintf(note, (size_t) buf_size, "%s %s", tactic_name, tactic_args);
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
