/**
 * @file lv00_protocol.h
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
 *   ├── Lv00DrawCmd[]       (M1-Canvas)
 *   ├── Lv00TableRow[]      (M3-Table)
 *   ├── Lv00TreeNode*       (M4-Tree)
 *   ├── Lv00TopoGraph       (M6-Topology)
 *   ├── DSL text            (M2-Text)
 *   ├── Lv00ProofStep[]     (P4-Proof)
 *   ├── Lv00EngineStatus    (P8-Engine)
 *   ├── Lv00TerminalOutput  (M5-Terminal)
 *   └── Lv00Completion[]    (M5-Terminal Tab)
 *
 * Kernel ←──UserAction──  UI
 *   ├── Lv00CanvasEvent     (M1-Canvas 鼠标/触摸)
 *   ├── Lv00TerminalCmd     (M5-Terminal 命令)
 *   ├── Lv00TableSelect     (M3-Table 行点击)
 *   ├── Lv00TreeAction      (M4-Tree 展开/选择)
 *   └── Lv00BlockDrag       (M6-Topology 拖拽)
 * ```
 *
 * @author Lv-00 Project
 * @version 3.0.0
 */
#ifndef LV00_PROTOCOL_H
#define LV00_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>
#include "lv00/lv00_api_spec.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 一、常量
 * ================================================================ */

#define LV00_PROTO_MAX_DRAW_CMDS    4096
#define LV00_PROTO_MAX_TABLE_ROWS    512
#define LV00_PROTO_MAX_TREE_NODES    256
#define LV00_PROTO_MAX_TOPOLOGY      128
#define LV00_PROTO_MAX_PROOF_STEPS   512
#define LV00_PROTO_MAX_COMPLETIONS    64
#define LV00_PROTO_MAX_TERMINAL_LINES 512
#define LV00_PROTO_STR_LEN            64
#define LV00_PROTO_LABEL_LEN         128
#define LV00_PROTO_DESC_LEN          256
#define LV00_PROTO_BUFFER_LEN       4096

/* ================================================================
 * 二、颜色系统（领域概念，非 UI 样式）
 * ================================================================ */

typedef enum {
    LV00_COLOR_GREEN = 0,       /* 全构造 / 已验证 */
    LV00_COLOR_BLUE = 1,        /* 未探索 / 输入值 */
    LV00_COLOR_BLUE_RANGE = 2,  /* 超出输入范围 */
    LV00_COLOR_YELLOW = 3,      /* 求解器生成 */
    LV00_COLOR_AMBER = 4,       /* 数值假设 */
    LV00_COLOR_LIGHT_ORANGE = 5,/* 非构造 */
    LV00_COLOR_ORANGE = 6,      /* 爆炸原理 / 神谕 */
    LV00_COLOR_DARK_ORANGE = 7, /* 非构造 + 数值 */
    LV00_COLOR_RED = 8,         /* 矛盾 */
    LV00_COLOR_GREY = 9,        /* 未知 / 未确证 */
    LV00_COLOR_PURPLE = 10,     /* 外部验证 */
    LV00_COLOR_CYAN = 11,       /* 互操作 */
} Lv00TrustColor;

LV00_API const char *lv00_trust_color_name(Lv00TrustColor c);
LV00_API uint32_t    lv00_trust_color_rgba(Lv00TrustColor c);
LV00_API const char *lv00_trust_color_svg(Lv00TrustColor c);
LV00_API const char *lv00_trust_color_tikz(Lv00TrustColor c);

/* ================================================================
 * 三、内核→UI：DisplayData
 * ================================================================ */

/* ---- M1-Canvas：画布绘制指令 ---- */

typedef enum {
    LV00_DRAW_LINE = 0,
    LV00_DRAW_POINT = 1,
    LV00_DRAW_TEXT = 2,
    LV00_DRAW_HIGHLIGHT = 3,
} Lv00DrawCmdType;

typedef enum {
    LV00_LINE_SOLID = 0,
    LV00_LINE_DASHED = 1,
    LV00_LINE_DOTTED = 2,
} Lv00LineStyle;

typedef struct {
    Lv00DrawCmdType type;
    double x1, y1;
    double x2, y2;
    double radius;
    char   text[LV00_PROTO_STR_LEN];
    uint32_t    color_rgba;
    Lv00TrustColor trust_color;
    double line_width;
    Lv00LineStyle style;
} Lv00DrawCmd;

typedef struct {
    Lv00DrawCmd *cmds;
    int count;
    int capacity;
    /* 视口元数据 */
    double viewport_offset_x;
    double viewport_offset_y;
    double viewport_scale;
    double canvas_width;
    double canvas_height;
} Lv00DrawCmdList;

/* ---- M3-Table：表格行 ---- */

typedef struct {
    int    id;
    char   name[LV00_PROTO_STR_LEN];
    char   node_type[32];
    char   coord_x[32];
    char   coord_y[32];
    int    constraint_count;
    uint32_t    color_rgba;
    Lv00TrustColor trust_color;
    char   status[16];
    int    parent_block_id;
} Lv00TableRow;

typedef struct {
    Lv00TableRow *rows;
    int count;
    int capacity;
} Lv00TableRowList;

/* ---- M4-Tree：证明树节点 ---- */

typedef enum {
    LV00_TREE_PROVED = 0,
    LV00_TREE_PENDING = 1,
    LV00_TREE_FAILED = 2,
    LV00_TREE_ASSUMED = 3,
    LV00_TREE_ROOT = 4,
} Lv00TreeNodeStatus;

typedef struct Lv00TreeNode {
    char   id[LV00_PROTO_STR_LEN];
    char   label[LV00_PROTO_LABEL_LEN];
    Lv00TrustColor trust_color;
    Lv00TreeNodeStatus status;
    int    node_id;
    struct Lv00TreeNode **children;
    int    child_count;
} Lv00TreeNode;

/* ---- M6-Topology：拓扑图 ---- */

typedef struct {
    int  id;
    char name[LV00_PROTO_STR_LEN];
} Lv00TopoPort;

typedef struct {
    int  id;
    char name[LV00_PROTO_STR_LEN];
    Lv00TopoPort *inputs;
    int  input_count;
    Lv00TopoPort *outputs;
    int  output_count;
    double layout_x;
    double layout_y;
} Lv00TopoBlock;

typedef struct {
    int from_block;
    int from_port;
    int to_block;
    int to_port;
} Lv00TopoEdge;

typedef struct {
    Lv00TopoBlock *blocks;
    int block_count;
    Lv00TopoEdge *edges;
    int edge_count;
} Lv00TopoGraph;

/* ---- M2-Text：DSL 文本 ---- */

typedef struct {
    char *text;
    int   length;
} Lv00DslText;

/* ---- M5-Terminal：终端输出行 ---- */

typedef struct {
    int  id;
    char text[LV00_PROTO_BUFFER_LEN];
    Lv00TrustColor color;
} Lv00TerminalLine;

typedef struct {
    Lv00TerminalLine *lines;
    int count;
} Lv00TerminalOutput;

/* ---- Tab 补全 ---- */

typedef struct {
    char *text;
} Lv00Completion;

typedef struct {
    Lv00Completion *items;
    int count;
} Lv00CompletionList;

/* ---- P4-Proof：证明步骤（替代 HTML 生成） ---- */

typedef enum {
    LV00_PROOF_STEP_AXIOM = 0,
    LV00_PROOF_STEP_THEOREM = 1,
    LV00_PROOF_STEP_LEMMA = 2,
    LV00_PROOF_STEP_COROLLARY = 3,
    LV00_PROOF_STEP_TACTIC = 4,
    LV00_PROOF_STEP_MERGE = 5,
    LV00_PROOF_STEP_ROOT = 6,
} Lv00ProofStepKind;

typedef struct {
    int    step_id;
    int    step_index;
    Lv00ProofStepKind kind;
    char   label[LV00_PROTO_LABEL_LEN];
    char   description[LV00_PROTO_DESC_LEN];
    Lv00TrustColor color;
    int    dependency_count;
    int   *dependency_ids;
    int    is_backtrack_point;
    int    is_explored;
    char   strategy[LV00_PROTO_STR_LEN];
    int    node_id;
    int    constraint_id;
} Lv00ProofStep;

typedef struct {
    Lv00ProofStep *steps;
    int   step_count;
    int   total_steps;
    int   green_count;       /* 已验证步骤数 */
    Lv00TrustColor final_color; /* 整体证明颜色 */
    char  strategy_label[LV00_PROTO_LABEL_LEN];
    char  nl_summary[LV00_PROTO_DESC_LEN]; /* 自然语言摘要 */
    int   is_complete;
} Lv00ProofNavigator;

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
    char  backend_info[LV00_PROTO_STR_LEN];
} Lv00EngineStatus;

/* ================================================================
 * 四、UI→内核：UserAction
 * ================================================================ */

/* ---- M1-Canvas：画布事件 ---- */

typedef enum {
    LV00_UI_MOUSE_DOWN = 0,
    LV00_UI_MOUSE_MOVE = 1,
    LV00_UI_MOUSE_UP = 2,
    LV00_UI_MOUSE_WHEEL = 3,
    LV00_UI_KEY_DOWN = 4,
} Lv00CanvasEventType;

typedef struct {
    Lv00CanvasEventType type;
    double screen_x;
    double screen_y;
    int    button;       /* 0=left, 1=middle, 2=right */
    int    shift_down;
    int    ctrl_down;
    double wheel_delta;  /* >0=zoom in, <0=zoom out */
    char   key[8];
} Lv00CanvasEvent;

/* ---- M5-Terminal：命令 ---- */

typedef struct {
    char command[LV00_PROTO_BUFFER_LEN];
} Lv00TerminalCmd;

typedef struct {
    int    request_id;
    int    success;
    int    error_code;
    char   output[LV00_PROTO_BUFFER_LEN];
} Lv00TerminalResponse;

/* ---- M3-Table：行选择 ---- */

typedef struct {
    int row_id;
    int ctrl_down;
} Lv00TableSelect;

/* ---- M4-Tree：树操作 ---- */

typedef enum {
    LV00_TREE_TOGGLE = 0,
    LV00_TREE_SELECT = 1,
} Lv00TreeActionType;

typedef struct {
    Lv00TreeActionType type;
    char node_id[LV00_PROTO_STR_LEN];
    int  node_id_int; /* 关联的几何节点 ID */
} Lv00TreeAction;

/* ---- M6-Topology：块拖拽 ---- */

typedef struct {
    int    block_id;
    double new_x;
    double new_y;
} Lv00BlockDrag;

/* ================================================================
 * 五、协议生成函数（内核→UI 数据投影）
 * ================================================================ */

LV00_API int lv00_proto_draw_commands(void *engine,
                             double offset_x, double offset_y,
                             double scale,
                             double canvas_w, double canvas_h,
                             Lv00DrawCmdList *out);

LV00_API int lv00_proto_table_rows(void *engine, Lv00TableRowList *out);

LV00_API int lv00_proto_dsl_text(void *engine, char *out, size_t buf_size);

LV00_API int lv00_proto_tree(void *engine, Lv00TreeNode **out_root);

LV00_API int lv00_proto_topology(void *engine, Lv00TopoGraph *out);

LV00_API int lv00_proto_proof_navigator(void *engine, Lv00ProofNavigator *out);

LV00_API int lv00_proto_engine_status(void *engine, Lv00EngineStatus *out);

LV00_API int lv00_proto_completions(void *engine, const char *prefix,
                           Lv00CompletionList *out);

LV00_API int lv00_proto_terminal_exec(void *engine, const char *command,
                             Lv00TerminalResponse *out);

/* ---- 资源释放 ---- */

LV00_API void lv00_proto_free_draw_commands(Lv00DrawCmdList *list);
LV00_API void lv00_proto_free_table_rows(Lv00TableRowList *list);
LV00_API void lv00_proto_free_tree(Lv00TreeNode *root);
LV00_API void lv00_proto_free_topology(Lv00TopoGraph *graph);
LV00_API void lv00_proto_free_proof(Lv00ProofNavigator *nav);
LV00_API void lv00_proto_free_completions(Lv00CompletionList *list);

#ifdef __cplusplus
}
#endif
#endif /* LV00_PROTOCOL_H */
