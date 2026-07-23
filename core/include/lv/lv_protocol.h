/**
 * @file lv_protocol.h
 * @brief Lv-00 UI-Kernel 通信协议 —— UI 与内核的唯一接口
 *
 * ## 设计原则
 *
 * 1. **零 UI 渲染逻辑**：所有类型只描述数据，不包含 HTML/CSS/JS/像素/尺寸假设
 * 2. **单向数据流**：内核→UI 是 DisplayData，UI→内核 是 UserAction
 * 3. **无外部格式依赖**：不生成 HTML/CSS/JS，只输出结构化数据和纯文本（LaTeX 除外）
 * 4. **可扩展**：新增视图类型只需追加枚举值和结构体，不影响已有类型
 *
 * ## 数据流方向
 *
 * ```
 * Kernel ──DisplayData──→  UI
 *   ├── lvDrawCmd[]       (M1-Canvas)
 *   ├── lvTableRow[]      (M3-Table)
 *   ├── lvTreeNode*       (M4-Tree)
 *   ├── lvTopoGraph       (M6-Topology)
 *   ├── DSL text            (M2-Text)
 *   ├── lvProofStep[]     (P4-Proof)
 *   ├── lvEngineStatus    (P8-Engine)
 *   ├── lvTerminalOutput  (M5-Terminal)
 *   └── lvCompletion[]    (M5-Terminal Tab)
 *
 * Kernel ←──UserAction──  UI
 *   ├── lvCanvasEvent     (M1-Canvas 鼠标/触摸)
 *   ├── lvTerminalCmd     (M5-Terminal 命令)
 *   ├── lvTableSelect     (M3-Table 行点击)
 *   ├── lvTreeAction      (M4-Tree 展开/选择)
 *   └── lvBlockDrag       (M6-Topology 拖拽)
 * ```
 *
 * @author Lv-00 Project
 * @version 1.1.0
 */
#ifndef lv_PROTOCOL_H
#define lv_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>
#include "lv/lv_api_spec.h"
#include "lv/config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 二、颜色系统（领域概念，非 UI 样式）
 * ================================================================ */

typedef enum {
    lv_COLOR_GREEN = 0,       /* 全构造 / 已验证 */
    lv_COLOR_BLUE = 1,        /* 未探索 / 输入值 */
    lv_COLOR_BLUE_RANGE = 2,  /* 超出输入范围 */
    lv_COLOR_YELLOW = 3,      /* 求解器生成 */
    lv_COLOR_AMBER = 4,       /* 数值假设 */
    lv_COLOR_LIGHT_ORANGE = 5,/* 非构造 */
    lv_COLOR_ORANGE = 6,      /* 爆炸原理 / 神谕 */
    lv_COLOR_DARK_ORANGE = 7, /* 非构造 + 数值 */
    lv_COLOR_RED = 8,         /* 矛盾 */
    lv_COLOR_GREY = 9,        /* 未知 / 未确证 */
    lv_COLOR_PURPLE = 10,     /* 外部验证 */
    lv_COLOR_CYAN = 11,       /* 互操作 */
} lvTrustColor;

lv_API const char *lv_trust_color_name(lvTrustColor c);
lv_API uint32_t    lv_trust_color_rgba(lvTrustColor c);
lv_API const char *lv_trust_color_svg(lvTrustColor c);
lv_API const char *lv_trust_color_tikz(lvTrustColor c);

/* ================================================================
 * 三、内核→UI：DisplayData
 * ================================================================ */

/* ---- M1-Canvas：画布绘制指令 ---- */

typedef enum {
    lv_DRAW_LINE = 0,
    lv_DRAW_POINT = 1,
    lv_DRAW_TEXT = 2,
    lv_DRAW_HIGHLIGHT = 3,
} lvDrawCmdType;

typedef enum {
    lv_LINE_SOLID = 0,
    lv_LINE_DASHED = 1,
    lv_LINE_DOTTED = 2,
} lvLineStyle;

typedef struct {
    lvDrawCmdType type;
    double x1, y1;
    double x2, y2;
    double radius;
    char   text[lv_PROTO_STR_LEN];
    uint32_t    color_rgba;
    lvTrustColor trust_color;
    double line_width;
    lvLineStyle style;
} lvDrawCmd;

typedef struct {
    lvDrawCmd *cmds;
    int count;
    int capacity;
    /* 视口元数据 */
    double viewport_offset_x;
    double viewport_offset_y;
    double viewport_scale;
    double canvas_width;
    double canvas_height;
} lvDrawCmdList;

/* ---- M3-Table：表格行 ---- */

typedef struct {
    int    id;
    char   name[lv_PROTO_STR_LEN];
    char   node_type[32];
    char   coord_x[32];
    char   coord_y[32];
    int    constraint_count;
    uint32_t    color_rgba;
    lvTrustColor trust_color;
    char   status[16];
    int    parent_block_id;
} lvTableRow;

typedef struct {
    lvTableRow *rows;
    int count;
    int capacity;
} lvTableRowList;

/* ---- M4-Tree：证明树节点 ---- */

typedef enum {
    lv_TREE_PROVED = 0,
    lv_TREE_PENDING = 1,
    lv_TREE_FAILED = 2,
    lv_TREE_ASSUMED = 3,
    lv_TREE_ROOT = 4,
} lvTreeNodeStatus;

typedef struct lvTreeNode {
    char   id[lv_PROTO_STR_LEN];
    char   label[lv_PROTO_LABEL_LEN];
    lvTrustColor trust_color;
    lvTreeNodeStatus status;
    int    node_id;
    struct lvTreeNode **children;
    int    child_count;
} lvTreeNode;

/* ---- M6-Topology：拓扑图 ---- */

typedef struct {
    int  id;
    char name[lv_PROTO_STR_LEN];
} lvTopoPort;

typedef struct {
    int  id;
    char name[lv_PROTO_STR_LEN];
    lvTopoPort *inputs;
    int  input_count;
    lvTopoPort *outputs;
    int  output_count;
    double layout_x;
    double layout_y;
} lvTopoBlock;

typedef struct {
    int from_block;
    int from_port;
    int to_block;
    int to_port;
} lvTopoEdge;

typedef struct {
    lvTopoBlock *blocks;
    int block_count;
    lvTopoEdge *edges;
    int edge_count;
} lvTopoGraph;

/* ---- M2-Text：DSL 文本 ---- */

typedef struct {
    char *text;
    int   length;
} lvDslText;

/* ---- M5-Terminal：终端输出行 ---- */

typedef struct {
    int  id;
    char text[lv_PROTO_BUFFER_LEN];
    lvTrustColor color;
} lvTerminalLine;

typedef struct {
    lvTerminalLine *lines;
    int count;
} lvTerminalOutput;

/* ---- Tab 补全 ---- */

typedef struct {
    char *text;
} lvCompletion;

typedef struct {
    lvCompletion *items;
    int count;
} lvCompletionList;

/* ---- P4-Proof：证明步骤（替代 HTML 生成） ---- */

typedef enum {
    lv_PROOF_STEP_AXIOM = 0,
    lv_PROOF_STEP_THEOREM = 1,
    lv_PROOF_STEP_LEMMA = 2,
    lv_PROOF_STEP_COROLLARY = 3,
    lv_PROOF_STEP_TACTIC = 4,
    lv_PROOF_STEP_MERGE = 5,
    lv_PROOF_STEP_ROOT = 6,
} lvProofStepKind;

typedef struct {
    int    step_id;
    int    step_index;
    lvProofStepKind kind;
    char   label[lv_PROTO_LABEL_LEN];
    char   description[lv_PROTO_DESC_LEN];
    lvTrustColor color;
    int    dependency_count;
    int   *dependency_ids;
    int    is_backtrack_point;
    int    is_explored;
    char   strategy[lv_PROTO_STR_LEN];
    int    node_id;
    int    constraint_id;
} lvProofStep;

typedef struct {
    lvProofStep *steps;
    int   step_count;
    int   total_steps;
    int   green_count;       /* 已验证步骤数 */
    lvTrustColor final_color; /* 整体证明颜色 */
    char  strategy_label[lv_PROTO_LABEL_LEN];
    char  nl_summary[lv_PROTO_DESC_LEN]; /* 自然语言摘要 */
    int   is_complete;
} lvProofNavigator;

/* ---- P8-Engine：引擎状态 ---- */

typedef struct {
    int   node_count;
    int   constraint_count;
    int   proof_count;
    int   func_block_count;
    int   snapshot_count;
    int   undo_depth;
    int   redo_depth;
    double last_solve_time_ms;
    double memory_usage_mb;
    char  engine_state[32];    /* "idle" | "running" | "error" */
    char  backend_info[lv_PROTO_STR_LEN];
} lvEngineStatus;

/* ================================================================
 * 四、UI→内核：UserAction
 * ================================================================ */

/* ---- M1-Canvas：画布事件 ---- */

typedef enum {
    lv_UI_MOUSE_DOWN = 0,
    lv_UI_MOUSE_MOVE = 1,
    lv_UI_MOUSE_UP = 2,
    lv_UI_MOUSE_WHEEL = 3,
    lv_UI_KEY_DOWN = 4,
} lvCanvasEventType;

typedef struct {
    lvCanvasEventType type;
    double screen_x;
    double screen_y;
    int    button;       /* 0=left, 1=middle, 2=right */
    int    shift_down;
    int    ctrl_down;
    double wheel_delta;  /* >0=zoom in, <0=zoom out */
    char   key[8];
} lvCanvasEvent;

/* ---- M5-Terminal：命令 ---- */

typedef struct {
    char command[lv_PROTO_BUFFER_LEN];
} lvTerminalCmd;

typedef struct {
    int    request_id;
    int    success;
    int    error_code;
    char   output[lv_PROTO_BUFFER_LEN];
} lvTerminalResponse;

/* ---- M3-Table：行选择 ---- */

typedef struct {
    int row_id;
    int ctrl_down;
} lvTableSelect;

/* ---- M4-Tree：树操作 ---- */

typedef enum {
    lv_TREE_TOGGLE = 0,
    lv_TREE_SELECT = 1,
} lvTreeActionType;

typedef struct {
    lvTreeActionType type;
    char node_id[lv_PROTO_STR_LEN];
    int  node_id_int; /* 关联的几何节点 ID */
} lvTreeAction;

/* ---- M6-Topology：块拖拽 ---- */

typedef struct {
    int    block_id;
    double new_x;
    double new_y;
} lvBlockDrag;

/* ================================================================
 * 五、协议生成函数（内核→UI 数据投影）
 * ================================================================ */

lv_API int lv_proto_draw_commands(void *engine,
                             double offset_x, double offset_y,
                             double scale,
                             double canvas_w, double canvas_h,
                             lvDrawCmdList *out);

lv_API int lv_proto_table_rows(void *engine, lvTableRowList *out);

lv_API int lv_proto_dsl_text(void *engine, char *out, size_t buf_size);

lv_API int lv_proto_tree(void *engine, lvTreeNode **out_root);

lv_API int lv_proto_topology(void *engine, lvTopoGraph *out);

lv_API int lv_proto_proof_navigator(void *engine, lvProofNavigator *out);

lv_API int lv_proto_engine_status(void *engine, lvEngineStatus *out);

lv_API int lv_proto_completions(void *engine, const char *prefix,
                           lvCompletionList *out);

lv_API int lv_proto_terminal_exec(void *engine, const char *command,
                             lvTerminalResponse *out);

/* ---- 资源释放 ---- */

lv_API void lv_proto_free_draw_commands(lvDrawCmdList *list);
lv_API void lv_proto_free_table_rows(lvTableRowList *list);
lv_API void lv_proto_free_tree(lvTreeNode *root);
lv_API void lv_proto_free_topology(lvTopoGraph *graph);
lv_API void lv_proto_free_proof(lvProofNavigator *nav);
lv_API void lv_proto_free_completions(lvCompletionList *list);

#ifdef __cplusplus
}
#endif
#endif /* lv_PROTOCOL_H */
