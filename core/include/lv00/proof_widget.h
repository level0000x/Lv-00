/**
 * @file proof_widget.h
 * @brief 证明交互可视化组件 —— 借鉴 ProofWidgets4 的 React 组件嵌入证明环境设计
 *
 * @details 设计借鉴：
 * - ProofWidgets4 (github.com/leanprover-community/ProofWidgets4)
 *   - React 组件嵌入证明环境
 *   - 证明目标的 HTML 树渲染
 *   - 步骤高亮与导航
 *   - 前提选择器（智能推荐可用前提）
 *   - 组件可组合：多个 Widget 拼接为自定义证明面板
 *
 * 设计目标：为 Lv-00 的 Web GUI 前端 React 组件提供 C API 数据契约。
 *          本头文件定义的是 C 端的数据结构和接口——
 *          前端通过 JSON 序列化与本层通信，实现证明状态的双向同步。
 *
 * @version 1.1.0
 * @date 2026-05-24
 */
#ifndef LV00_PROOF_WIDGET_H
#define LV00_PROOF_WIDGET_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "constraint_graph.h"
#include "proof.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================================================
 *  前向声明
 * ================================================================ */
typedef struct Lv00WidgetLayout Lv00WidgetLayout;
typedef struct Lv00GoalDisplay Lv00GoalDisplay;
typedef struct Lv00HypothesisEntry Lv00HypothesisEntry;
typedef struct Lv00ProofStepHighlight Lv00ProofStepHighlight;
typedef struct ProofWidgetState ProofWidgetState;
typedef struct ProofNavigator ProofNavigator;
typedef struct ConstraintGraph ConstraintGraph;
/* ================================================================
 *  第一部分：Widget 类型与状态枚举
 * ================================================================ */
typedef enum {
    WIDGET_GOAL_DISPLAY = 0,
    WIDGET_HYPOTHESIS_PANEL = 1,
    WIDGET_APPLY_BUTTON = 2,
    WIDGET_STEP_NAVIGATOR = 3,
    WIDGET_SEARCH_TREE = 4,
    WIDGET_TIMELINE = 5,
    WIDGET_DEPENDENCY_GRAPH = 6,
    WIDGET_TACTIC_HISTORY = 7
} ProofWidgetType;

typedef enum {
    LAYOUT_GRID = 0,
    LAYOUT_HORIZONTAL = 1,
    LAYOUT_VERTICAL = 2,
    LAYOUT_TABBED = 3
} Lv00LayoutType;

typedef enum {
    HIGHLIGHT_NORMAL = 0,
    HIGHLIGHT_ACTIVE = 1,
    HIGHLIGHT_COMPLETED = 2,
    HIGHLIGHT_FAILED = 3,
    HIGHLIGHT_SEARCHING = 4
} Lv00HighlightState;
/* ================================================================
 *  第二部分：Widget 核心数据结构
 * ================================================================ */
typedef struct ProofWidgetState {
    int widget_id;
    ProofWidgetType widget_type;
    bool is_active;
    bool is_enabled;
    char *display_label;
    int bound_step_id;
    char *interaction_data;
} ProofWidgetState;

struct Lv00HypothesisEntry {
    int hyp_id;
    char *name;
    char *type_text;
    char *value_text;
    int source_step;
    bool is_selected;
};

struct Lv00GoalDisplay {
    Lv00HypothesisEntry *hypotheses;
    int hyp_count;
    char *goal_text;
    char **context_terms;
    int context_count;
    int depth;
    bool is_solved;
};

struct Lv00ProofStepHighlight {
    int step_id;
    Lv00HighlightState color;
    bool is_animated;
    float progress;
    char *tooltip_text;
};

struct Lv00WidgetLayout {
    ProofWidgetState *widgets;
    int widget_count;
    int widget_capacity;
    Lv00LayoutType layout_type;
    int columns;
    int rows;
    int *order_indices;
    char *persistence_key;
};
/* ================================================================
 *  第三部分：API —— 生命周期
 * ================================================================ */
Lv00WidgetLayout *proof_widget_init(int layout_capacity);
void proof_widget_destroy(Lv00WidgetLayout *layout);
/* ================================================================
 *  第四部分：API —— Widget 注册与更新
 * ================================================================ */
int proof_widget_register(Lv00WidgetLayout *layout, ProofWidgetType widget_type, const char *label, int bound_step);
int proof_widget_update(Lv00WidgetLayout *layout, int widget_id, bool is_active, bool is_enabled,
                        const char *display_label, int bound_step_id, const char *interaction_json);
/* ================================================================
 *  第五部分：API —— 证明状态查询
 * ================================================================ */
int proof_widget_get_goal(const ProofNavigator *navigator, Lv00GoalDisplay *out_goal);
int proof_widget_get_hypotheses(const ProofNavigator *navigator, Lv00HypothesisEntry *out_hypotheses, int max_count);
void goal_display_free(Lv00GoalDisplay *goal);
/* ================================================================
 *  第六部分：API —— 智能推荐与可视化
 * ================================================================ */
int proof_widget_suggest_tactic(const ProofNavigator *navigator, char **out_suggestions, double *out_confidences,
                                int max_count);
int proof_widget_get_step_highlights(const ProofNavigator *navigator, Lv00ProofStepHighlight *out_highlights,
                                     int max_count);
char *proof_widget_get_search_tree(const ProofNavigator *navigator);
char *proof_widget_get_dependency_graph(const ProofNavigator *navigator);
/* ================================================================
 *  第七部分：API —— 布局导出与策略回传
 * ================================================================ */
char *proof_widget_export_layout(const Lv00WidgetLayout *layout);
int proof_widget_apply_tactic(ProofNavigator *navigator, const char *tactic_name, const char *tactic_args,
                              bool *out_success, char **out_feedback);
/* ================================================================
 *  第八部分：API —— 布局管理
 * ================================================================ */
void proof_widget_set_layout_type(Lv00WidgetLayout *layout, Lv00LayoutType layout_type, int columns, int rows);
void proof_widget_set_persistence_key(Lv00WidgetLayout *layout, const char *persistence_key);
void proof_widget_set_order(Lv00WidgetLayout *layout, const int *order_indices, int count);
#ifdef __cplusplus
}
#endif
#endif /* LV00_PROOF_WIDGET_H */