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
 * @version v3.3.0
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

/**
 * @brief 证明构件（Widget）类型枚举
 *
 * 定义证明交互界面中可用的可视化组件类型。
 * 借鉴 ProofWidgets4 的组件体系，每种组件负责不同的交互职责：
 *
 * - WIDGET_GOAL_DISPLAY：渲染当前证明目标的 HTML 树，支持高亮当前子目标
 * - WIDGET_HYPOTHESIS_PANEL：显示当前可用的所有前提（hypotheses），支持点击选中
 * - WIDGET_APPLY_BUTTON：策略应用按钮，用户点击后将策略回传到证明引擎
 * - WIDGET_STEP_NAVIGATOR：证明步骤的导航面板，支持前进/后退/跳转
 * - WIDGET_SEARCH_TREE：搜索树可视化，展示自动证明搜索过程的分支与节点
 * - WIDGET_TIMELINE：证明时间线，按时间顺序展示已完成的证明步骤
 * - WIDGET_DEPENDENCY_GRAPH：依赖图，展示定理、引理、前提之间的依赖关系
 * - WIDGET_TACTIC_HISTORY：策略历史记录，展示已经应用的策略序列
 */
typedef enum {
    WIDGET_GOAL_DISPLAY = 0,     /**< 目标显示组件 */
    WIDGET_HYPOTHESIS_PANEL = 1, /**< 前提面板组件 */
    WIDGET_APPLY_BUTTON = 2,     /**< 策略按钮组件 */
    WIDGET_STEP_NAVIGATOR = 3,   /**< 步骤导航组件 */
    WIDGET_SEARCH_TREE = 4,      /**< 搜索树组件 */
    WIDGET_TIMELINE = 5,         /**< 证明时间线组件 */
    WIDGET_DEPENDENCY_GRAPH = 6, /**< 依赖图组件 */
    WIDGET_TACTIC_HISTORY = 7    /**< 策略历史组件 */
} ProofWidgetType;

/**
 * @brief 布局类型枚举
 *
 * 定义多个 Widget 在证明面板中的排列方式：
 * - GRID：网格布局（行列式）
 * - HORIZONTAL：水平排列（从左到右）
 * - VERTICAL：垂直排列（从上到下）
 * - TABBED：标签页布局（同一区域切换显示）
 */
typedef enum {
    LAYOUT_GRID = 0,       /**< 网格布局 */
    LAYOUT_HORIZONTAL = 1, /**< 水平排列 */
    LAYOUT_VERTICAL = 2,   /**< 垂直排列 */
    LAYOUT_TABBED = 3      /**< 标签页布局 */
} Lv00LayoutType;

/**
 * @brief 证明步骤高亮状态/颜色枚举
 *
 * 定义证明步骤在界面中的视觉状态：
 * - NORMAL：默认状态，未激活
 * - ACTIVE：当前正在执行的步骤
 * - COMPLETED：已成功完成的步骤
 * - FAILED：失败的步骤（尝试但未成功）
 * - SEARCHING：自动搜索中的步骤（动画效果）
 */
typedef enum {
    HIGHLIGHT_NORMAL = 0,    /**< 默认状态 */
    HIGHLIGHT_ACTIVE = 1,    /**< 激活（当前步骤） */
    HIGHLIGHT_COMPLETED = 2, /**< 已完成 */
    HIGHLIGHT_FAILED = 3,    /**< 失败 */
    HIGHLIGHT_SEARCHING = 4  /**< 搜索中（带动画） */
} Lv00HighlightState;

/* ================================================================
 *  第二部分：Widget 核心数据结构
 * ================================================================ */

/**
 * @brief 证明构件（Widget）状态结构体
 *
 * 描述一个证明交互 Widget 的完整运行时状态。
 * 每个 Widget 有唯一的 ID、绑定的证明步骤、启/禁用状态等。
 * 用户交互数据以 JSON 字符串形式存储，供前端 React 组件消费。
 */
typedef struct ProofWidgetState {
    int widget_id;               /**< Widget 唯一标识符 */
    ProofWidgetType widget_type; /**< Widget 类型 */
    bool is_active;              /**< 是否处于活动状态 */
    bool is_enabled;             /**< 是否启用（禁用则灰显） */
    char *display_label;         /**< 显示标签（人类可读标题） */
    int bound_step_id;           /**< 绑定的证明步骤 ID（-1 表示无绑定） */
    char *interaction_data;      /**< 用户交互数据 JSON 字符串（可为 NULL） */
} ProofWidgetState;

/**
 * @brief 前提条目结构体
 *
 * 证明环境中的一个前提（hypothesis）。
 * 包含前提的标识、名称、类型文本、值文本、来源步骤和选中状态。
 *
 * 前端渲染为可选择的前提卡片，用户点击后可用于策略应用。
 */
struct Lv00HypothesisEntry {
    int hyp_id;       /**< 前提唯一标识符 */
    char *name;       /**< 前提名称（如 "h1", "h_AB_is_line"） */
    char *type_text;  /**< 前提类型文本（如 "Point A lies on Line l"） */
    char *value_text; /**< 前提值文本（具体命题内容，可为 NULL） */
    int source_step;  /**< 来源步骤 ID（此前提由哪个步骤引入，-1 为原始前提） */
    bool is_selected; /**< 是否被用户选中（前端交互状态） */
};

/**
 * @brief 证明目标显示结构体
 *
 * 封装当前证明目标的完整渲染数据。
 * 包括前提列表、目标文本和上下文可用项。
 * 前端根据此结构体渲染证明目标的 HTML 树。
 */
struct Lv00GoalDisplay {
    Lv00HypothesisEntry *hypotheses; /**< 前提条目数组 */
    int hyp_count;                   /**< 前提数量 */
    char *goal_text;                 /**< 目标文本（如 "Prove: triangle ABC is equilateral"） */
    char **context_terms;            /**< 上下文中可用的项名称数组 */
    int context_count;               /**< 上下文项数量 */
    int depth;                       /**< 目标嵌套深度（含嵌套中的证明为 0） */
    bool is_solved;                  /**< 目标是否已解决 */
};

/**
 * @brief 证明步骤高亮结构体
 *
 * 描述单个证明步骤在 UI 中的高亮/颜色/动画状态。
 * 前端根据此结构体为每个步骤渲染对应的视觉效果。
 */
struct Lv00ProofStepHighlight {
    int step_id;              /**< 证明步骤 ID */
    Lv00HighlightState color; /**< 高亮颜色/状态 */
    bool is_animated;         /**< 是否需要动画效果（如搜索中闪烁） */
    float progress;           /**< 动画进度（0.0 ~ 1.0，仅 SEARCHING 状态使用） */
    char *tooltip_text;       /**< 工具提示文本（鼠标悬停显示） */
};

/**
 * @brief 构件布局结构体
 *
 * 定义多个 Widget 在证明面板中的布局排列。
 * 布局支持网格、水平、垂直和标签页四种模式。
 * 持久化 key 用于保存/恢复布局状态。
 *
 * 前端 React 渲染器读取此布局定义，在面板中排列各 Widget。
 */
struct Lv00WidgetLayout {
    ProofWidgetState *widgets;  /**< Widget 状态数组 */
    int widget_count;           /**< Widget 数量 */
    int widget_capacity;        /**< Widget 容量 */
    Lv00LayoutType layout_type; /**< 布局类型 */
    int columns;                /**< 列数（仅 GRID 布局有效，其他布局忽略） */
    int rows;                   /**< 行数（仅 GRID 布局有效，其他布局忽略） */
    int *order_indices;         /**< Widget 顺序索引数组（可为 NULL 表示默认顺序） */
    char *persistence_key;      /**< 持久化 key（用于保存/恢复布局状态） */
};

/* ================================================================
 *  第三部分：API —— 生命周期
 * ================================================================ */

/**
 * @brief 初始化证明 Widget 系统
 *
 * 在证明会话开始时调用，分配内部状态。每个证明会话对应一个 Widget 系统实例。
 *
 * @param layout_capacity  布局中 Widget 的最大数量（建议 >= 8）
 * @return 成功返回 Widget 布局指针，失败返回 NULL
 */
Lv00WidgetLayout *proof_widget_init(int layout_capacity);

/**
 * @brief 销毁证明 Widget 系统并释放所有资源
 *
 * 释放布局中所有 Widget 状态、前提条目、上下文项等内存。
 *
 * @param layout  布局指针（销毁后置为悬空）
 */
void proof_widget_destroy(Lv00WidgetLayout *layout);

/* ================================================================
 *  第四部分：API —— Widget 注册与更新
 * ================================================================ */

/**
 * @brief 向布局中注册一个新 Widget
 *
 * @param layout       布局
 * @param widget_type  Widget 类型
 * @param label        显示标签
 * @param bound_step   绑定的证明步骤 ID（-1 表示无绑定）
 * @return 成功返回分配的 Widget ID（>= 0），失败返回 -1
 */
int proof_widget_register(Lv00WidgetLayout *layout, ProofWidgetType widget_type, const char *label, int bound_step);

/**
 * @brief 更新 Widget 状态
 *
 * 修改现有 Widget 的启用状态、显示标签、绑定步骤或交互数据。
 * 只更新非默认值字段（传入 -1/NULL/false 表示保持原值）。
 *
 * @param layout           布局
 * @param widget_id        Widget ID
 * @param is_active        新的活动状态
 * @param is_enabled       新的启用状态
 * @param display_label    新的显示标签（NULL 表示不变）
 * @param bound_step_id    新的绑定步骤 ID（-2 表示不变，-1 表示解绑）
 * @param interaction_json 新的交互数据 JSON（NULL 表示不变）
 * @return 成功返回 0，失败返回负值错误码
 */
int proof_widget_update(Lv00WidgetLayout *layout, int widget_id, bool is_active, bool is_enabled,
                        const char *display_label, int bound_step_id, const char *interaction_json);

/* ================================================================
 *  第五部分：API —— 证明状态查询
 * ================================================================ */

/**
 * @brief 获取当前证明目标数据
 *
 * 从证明导航器提取当前目标的渲染数据，填充到 GoalDisplay 结构体。
 * 前端调用此函数获取目标树的最新状态用于渲染。
 *
 * @param navigator  证明导航器
 * @param out_goal   输出：目标显示数据（调用者负责通过 goal_display_free 释放）
 * @return 成功返回 0，失败返回负值错误码
 */
int proof_widget_get_goal(const ProofNavigator *navigator, Lv00GoalDisplay *out_goal);

/**
 * @brief 获取当前前提数据
 *
 * 提取当前证明状态中所有可用的前提（hypotheses）。
 *
 * @param navigator      证明导航器
 * @param out_hypotheses 输出：前提条目数组（调用者分配，大小 >= max_count）
 * @param max_count      数组最大容量
 * @return 实际填充的前提数量（>= 0），或 -1 表示错误
 */
int proof_widget_get_hypotheses(const ProofNavigator *navigator, Lv00HypothesisEntry *out_hypotheses, int max_count);

/**
 * @brief 释放目标显示数据的内存
 *
 * @param goal  目标显示数据指针
 */
void goal_display_free(Lv00GoalDisplay *goal);

/* ================================================================
 *  第六部分：API —— 智能推荐与可视化
 * ================================================================ */

/**
 * @brief 智能推荐证明策略
 *
 * 基于当前目标和可用前提，使用启发式算法推荐最有可能成功的证明策略。
 * 借鉴 ProofWidgets4 的 tactic suggestion 功能。
 *
 * @param navigator         证明导航器
 * @param out_suggestions   输出：推荐策略名称数组（调用者分配，大小 >= max_count）
 * @param out_confidences   输出：每个策略的置信度（0.0~1.0，调用者分配，可为 NULL）
 * @param max_count         最大推荐数量
 * @return 实际推荐的策略数量（>= 0），或 -1 表示错误
 */
int proof_widget_suggest_tactic(const ProofNavigator *navigator, char **out_suggestions, double *out_confidences,
                                int max_count);

/**
 * @brief 获取所有证明步骤的高亮状态
 *
 * 返回证明树中所有步骤的当前高亮/颜色状态。
 * 前端根据此信息为每一步渲染对应的视觉效果。
 *
 * @param navigator       证明导航器
 * @param out_highlights  输出：高亮状态数组（调用者分配，大小 >= max_count）
 * @param max_count       最大步骤数量
 * @return 实际填充的步骤数量（>= 0），或 -1 表示错误
 */
int proof_widget_get_step_highlights(const ProofNavigator *navigator, Lv00ProofStepHighlight *out_highlights,
                                     int max_count);

/**
 * @brief 获取搜索树数据（JSON 格式）
 *
 * 将证明搜索树序列化为 JSON 字符串，供前端渲染交互式搜索树。
 * JSON 结构包含节点、边、分支状态等信息。
 *
 * @param navigator  证明导航器
 * @return 搜索树 JSON 字符串（调用者负责 free），失败返回 NULL
 */
char *proof_widget_get_search_tree(const ProofNavigator *navigator);

/**
 * @brief 获取依赖图数据（JSON 格式）
 *
 * 将定理/引理/前提的依赖关系序列化为 JSON 图结构。
 * 节点为定理/前提，边为依赖关系。
 *
 * @param navigator  证明导航器
 * @return 依赖图 JSON 字符串（调用者负责 free），失败返回 NULL
 */
char *proof_widget_get_dependency_graph(const ProofNavigator *navigator);

/* ================================================================
 *  第七部分：API —— 布局导出与策略回传
 * ================================================================ */

/**
 * @brief 导出布局 JSON 供前端 React 渲染
 *
 * 将当前 Widget 布局序列化为 JSON 字符串。
 * 前端 React 渲染器读取此 JSON，按布局类型排列各 Widget 组件。
 *
 * 导出的 JSON 结构示例：
 * {
 *   "layout_type": "VERTICAL",
 *   "persistence_key": "proof_panel_main",
 *   "widgets": [
 *     { "id": 0, "type": "GOAL_DISPLAY", "label": "Current Goal", "active": true },
 *     ...
 *   ]
 * }
 *
 * @param layout  布局
 * @return JSON 字符串（调用者负责 free），失败返回 NULL
 */
char *proof_widget_export_layout(const Lv00WidgetLayout *layout);

/**
 * @brief 从用户交互回传策略应用
 *
 * 前端用户点击策略按钮后，通过此接口将策略名称和参数回传到证明引擎。
 * 引擎执行策略后更新证明状态，前端再通过查询接口获取更新后的渲染数据。
 *
 * @param navigator     证明导航器
 * @param tactic_name   策略名称（如 "intro", "apply", "cases", "induction"）
 * @param tactic_args   策略参数 JSON 字符串（如 {"target":"h1", "term":"refl"}）
 * @param out_success   输出：策略是否成功应用
 * @param out_feedback  输出：反馈信息 JSON 字符串
 * @return 成功返回 0（即使策略本身失败），系统错误返回负值错误码
 */
int proof_widget_apply_tactic(ProofNavigator *navigator, const char *tactic_name, const char *tactic_args,
                              bool *out_success, char **out_feedback);

/* ================================================================
 *  第八部分：API —— 布局管理
 * ================================================================ */

/**
 * @brief 设置布局类型
 *
 * @param layout      布局
 * @param layout_type 布局类型
 * @param columns     列数（仅 GRID 有效，其他布局忽略）
 * @param rows        行数（仅 GRID 有效，其他布局忽略）
 */
void proof_widget_set_layout_type(Lv00WidgetLayout *layout, Lv00LayoutType layout_type, int columns, int rows);

/**
 * @brief 设置布局持久化 key
 *
 * @param layout          布局
 * @param persistence_key 持久化 key（用于恢复布局状态）
 */
void proof_widget_set_persistence_key(Lv00WidgetLayout *layout, const char *persistence_key);

/**
 * @brief 设置 Widget 显示顺序
 *
 * @param layout        布局
 * @param order_indices 顺序索引数组（widget_count 长度，index 为位置，值为 widget_id）
 * @param count         索引数量
 */
void proof_widget_set_order(Lv00WidgetLayout *layout, const int *order_indices, int count);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PROOF_WIDGET_H */
