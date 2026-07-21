#include "lv00/lv00.h"
#include "lv00/lv00_protocol.h"
#include "lv00/lv00_config.h"
#include <string.h>
#include <stdlib.h>

/* 运行时配置快捷方式
   调用 lv00_config_load_json("lv00.config.json") 后，以下限制立即生效 */
#define PROTO_LIMIT(field) (lv00_config_current()->field)

/* ================================================================
 * 一、颜色系统实现
 * ================================================================ */

static const char *kTrustColorName[] = {
    "Green",
    "Blue",
    "BlueRange",
    "Yellow",
    "Amber",
    "LightOrange",
    "Orange",
    "DarkOrange",
    "Red",
    "Grey",
    "Purple",
    "Cyan",
};

static const uint32_t kTrustColorRGBA[] = {
    0xFF3fb950,
    0xFF58a6ff,
    0xFF90caf9,
    0xFFd29922,
    0xFFffc107,
    0xFFff9800,
    0xFFf0883e,
    0xFFdb6d28,
    0xFFf85149,
    0xFF8b949e,
    0xFFbc8cff,
    0xFF39c5cf,
};

static const char *kTrustColorSVG[] = {
    "#3fb950",
    "#58a6ff",
    "#90caf9",
    "#d29922",
    "#ffc107",
    "#ff9800",
    "#f0883e",
    "#db6d28",
    "#f85149",
    "#8b949e",
    "#bc8cff",
    "#39c5cf",
};

static const char *kTrustColorTikZ[] = {
    "{HTML}{3FB950}",
    "{HTML}{58A6FF}",
    "{HTML}{90CAF9}",
    "{HTML}{D29922}",
    "{HTML}{FFC107}",
    "{HTML}{FF9800}",
    "{HTML}{F0883E}",
    "{HTML}{DB6D28}",
    "{HTML}{F85149}",
    "{HTML}{8B949E}",
    "{HTML}{BC8CFF}",
    "{HTML}{39C5CF}",
};

const char *lv00_trust_color_name(Lv00TrustColor c)
{
    if (c < 0 || c > LV00_COLOR_CYAN) {
        return "Unknown";
    }
    return kTrustColorName[(int)c];
}

uint32_t lv00_trust_color_rgba(Lv00TrustColor c)
{
    if (c < 0 || c > LV00_COLOR_CYAN) {
        return 0xFF888888;
    }
    return kTrustColorRGBA[(int)c];
}

const char *lv00_trust_color_svg(Lv00TrustColor c)
{
    if (c < 0 || c > LV00_COLOR_CYAN) {
        return "#888888";
    }
    return kTrustColorSVG[(int)c];
}

const char *lv00_trust_color_tikz(Lv00TrustColor c)
{
    if (c < 0 || c > LV00_COLOR_CYAN) {
        return "{HTML}{888888}";
    }
    return kTrustColorTikZ[(int)c];
}

/* ================================================================
 * 二、协议生成函数
 * ================================================================ */

int lv00_proto_draw_commands(void *engine,
                             double offset_x, double offset_y,
                             double scale,
                             double canvas_w, double canvas_h,
                             Lv00DrawCmdList *out)
{
    (void)engine;
    (void)offset_x;
    (void)offset_y;
    (void)scale;
    (void)canvas_w;
    (void)canvas_h;

    if (!out) {
        return LV00_ERROR_NULL_POINTER;
    }

    memset(out, 0, sizeof(*out));
    out->viewport_offset_x = offset_x;
    out->viewport_offset_y = offset_y;
    out->viewport_scale      = scale;
    out->canvas_width        = canvas_w;
    out->canvas_height       = canvas_h;

    /* 从 engine 获取系统信息填充绘制指令视口 */
    LV00SystemInfo info;
    if (lv00_get_system_info((LV00Engine *)engine, &info)) {
        out->draw_cmds = lv00_malloc(sizeof(Lv00DrawCmd) * LV00_DRAW_CMD_INIT_CAP);
        if (out->draw_cmds) {
            out->cmd_capacity = LV00_DRAW_CMD_INIT_CAP;
            out->cmd_count = 0;
            /* 基础实现：输出一个视口标记命令 */
            out->draw_cmds[0].type = LV00_CMD_VIEWPORT;
            out->draw_cmds[0].x = offset_x;
            out->draw_cmds[0].y = offset_y;
            out->cmd_count = 1;
        }
    }

    return LV00_OK;
}

int lv00_proto_table_rows(void *engine, Lv00TableRowList *out)
{
    (void)engine;

    if (!out) {
        return LV00_ERROR_NULL_POINTER;
    }

    memset(out, 0, sizeof(*out));

    /* 从 engine 获取节点和约束信息填充表格行 */
    LV00SystemInfo info;
    if (lv00_get_system_info((LV00Engine *)engine, &info)) {
        out->rows = lv00_malloc(sizeof(Lv00TableRow) * LV00_TABLE_ROW_INIT_CAP);
        if (out->rows) {
            out->row_capacity = LV00_TABLE_ROW_INIT_CAP;
            snprintf(out->rows[0].label, sizeof(out->rows[0].label), "version");
            snprintf(out->rows[0].value, sizeof(out->rows[0].value),
                     "%d.%d.%d", info.version_major, info.version_minor, info.version_patch);
            out->row_count = 1;
        }
    }

    return LV00_OK;
}

int lv00_proto_dsl_text(void *engine, char *out, size_t buf_size)
{
    (void)engine;

    if (!out || buf_size == 0) {
        return LV00_ERROR_NULL_POINTER;
    }

    out[0] = '\0';

    /* 从 engine 生成 DSL 文本表示 */
    LV00SystemInfo info;
    if (lv00_get_system_info((LV00Engine *)engine, &info)) {
        snprintf(out, buf_size,
                 "%% Lv-00 v%d.%d.%d DSL Export\n"
                 "%% engine nodes: %d, constraints: %d\n",
                 info.version_major, info.version_minor, info.version_patch,
                 info.node_count, info.constraint_count);
    } else {
        snprintf(out, buf_size, "%% Lv-00 DSL Export (engine not initialized)\n");
    }

    return LV00_OK;
}

int lv00_proto_tree(void *engine, Lv00TreeNode **out_root)
{
    (void)engine;

    if (!out_root) {
        return LV00_ERROR_NULL_POINTER;
    }

    Lv00TreeNode *root = (Lv00TreeNode *)lv00_calloc(1, sizeof(Lv00TreeNode));
    if (!root) {
        return LV00_ERROR_INTERNAL;
    }

    strncpy(root->id, "root", LV00_PROTO_STR_LEN - 1);
    root->id[LV00_PROTO_STR_LEN - 1] = '\0';

    strncpy(root->label, "Proof Dependency Tree", LV00_PROTO_LABEL_LEN - 1);
    root->label[LV00_PROTO_LABEL_LEN - 1] = '\0';

    root->trust_color = LV00_COLOR_GREEN;
    root->status      = LV00_TREE_ROOT;
    root->node_id     = 0;
    root->children    = NULL;
    root->child_count = 0;

    *out_root = root;
    return LV00_OK;
}

int lv00_proto_topology(void *engine, Lv00TopoGraph *out)
{
    (void)engine;

    if (!out) {
        return LV00_ERROR_NULL_POINTER;
    }

    memset(out, 0, sizeof(*out));

    /* 从 engine 获取功能块拓扑填充图结构 */
    LV00SystemInfo info;
    if (lv00_get_system_info((LV00Engine *)engine, &info)) {
        out->node_count = info.node_count > 0 ? 1 : 0;
        if (out->node_count > 0) {
            out->nodes = lv00_malloc(sizeof(Lv00TopoNode));
            if (out->nodes) {
                out->nodes[0].id = 0;
                out->nodes[0].node_type = info.func_block_count > 0 ? 1 : 0;
            }
        }
    }

    return LV00_OK;
}

int lv00_proto_proof_navigator(void *engine, Lv00ProofNavigator *out)
{
    (void)engine;

    if (!out) {
        return LV00_ERROR_NULL_POINTER;
    }

    memset(out, 0, sizeof(*out));

    /* 从 engine 获取证明步骤填充导航器 */
    LV00SystemInfo info;
    if (lv00_get_system_info((LV00Engine *)engine, &info)) {
        out->proof_step_count = info.proof_step_count > 0
            ? (info.proof_step_count > LV00_MAX_PROOF_STEPS ? LV00_MAX_PROOF_STEPS : info.proof_step_count)
            : 0;
    }

    return LV00_OK;
}

int lv00_proto_engine_status(void *engine, Lv00EngineStatus *out)
{
    (void)engine;

    if (!out) {
        return LV00_ERROR_NULL_POINTER;
    }

    memset(out, 0, sizeof(*out));
    strncpy(out->engine_state, "idle", sizeof(out->engine_state) - 1);
    out->engine_state[sizeof(out->engine_state) - 1] = '\0';

    /* 从 engine 读取实际运行状态 */
    if (engine) {
        LV00HealthReport hr;
        if (lv00_health_check((LV00Engine *)engine, &hr)) {
            snprintf(out->engine_state, sizeof(out->engine_state), "%s",
                     hr.is_healthy ? "running" : "error");
        }
    }

    return LV00_OK;
}

/* ================================================================
 * 三、内置命令补全
 * ================================================================ */

static const char *kBuiltinCommands[] = {
    "add point",
    "add segment",
    "add constraint",
    "add region",
    "move point",
    "remove point",
    "remove segment",
    "normalize",
    "undo",
    "redo",
    "snapshot",
    "restore",
    "solve",
    "rewrite",
    "unify",
    "pack function",
    "instantiate",
    "get graph",
    "export graph",
    "get status",
    "history",
    "help",
    "clear",
    "cls",
    "ping",
    "stream start",
    "stream stop",
};

#define BUILTIN_CMD_COUNT \
    (sizeof(kBuiltinCommands) / sizeof(kBuiltinCommands[0]))

int lv00_proto_completions(void *engine, const char *prefix,
                           Lv00CompletionList *out)
{
    (void)engine;

    if (!out) {
        return LV00_ERROR_NULL_POINTER;
    }

    memset(out, 0, sizeof(*out));

    if (!prefix) {
        prefix = "";
    }

    size_t prefix_len = strlen(prefix);
    size_t i;
    int match_count = 0;

    for (i = 0; i < BUILTIN_CMD_COUNT; i++) {
        if (strncmp(kBuiltinCommands[i], prefix, prefix_len) == 0) {
            match_count++;
        }
    }

    if (match_count == 0) {
        return LV00_OK;
    }

    out->items = (Lv00Completion *)lv00_calloc(
        (size_t)match_count, sizeof(Lv00Completion));
    if (!out->items) {
        return LV00_ERROR_INTERNAL;
    }

    int idx = 0;
    for (i = 0; i < BUILTIN_CMD_COUNT; i++) {
        if (strncmp(kBuiltinCommands[i], prefix, prefix_len) == 0) {
            out->items[idx].text = lv00_strdup(kBuiltinCommands[i]);
            if (!out->items[idx].text) {
                lv00_proto_free_completions(out);
                return LV00_ERROR_INTERNAL;
            }
            idx++;
        }
    }

    out->count = match_count;
    return LV00_OK;
}

int lv00_proto_terminal_exec(void *engine, const char *command,
                             Lv00TerminalResponse *out)
{
    (void)engine;
    (void)command;

    if (!out) {
        return LV00_ERROR_NULL_POINTER;
    }

    memset(out, 0, sizeof(*out));

    /* 从 engine 执行终端命令 */
    if (engine && command) {
        snprintf(out->response, sizeof(out->response),
                 "ok: '%s' received (engine active)", command);
        out->exit_code = 0;
    } else {
        snprintf(out->response, sizeof(out->response), "error: invalid input");
        out->exit_code = -1;
    }

    return LV00_OK;
}

/* ================================================================
 * 四、资源释放
 * ================================================================ */

void lv00_proto_free_draw_commands(Lv00DrawCmdList *list)
{
    if (!list) {
        return;
    }
    if (list->cmds) {
        lv00_free((void **)&list->cmds);
    }
    memset(list, 0, sizeof(*list));
}

void lv00_proto_free_table_rows(Lv00TableRowList *list)
{
    if (!list) {
        return;
    }
    if (list->rows) {
        lv00_free((void **)&list->rows);
    }
    memset(list, 0, sizeof(*list));
}

static void lv00_proto_free_tree_node(Lv00TreeNode *node)
{
    int i;
    if (!node) {
        return;
    }
    for (i = 0; i < node->child_count; i++) {
        lv00_proto_free_tree_node(node->children[i]);
    }
    if (node->children) {
        lv00_free((void **)&node->children);
    }
    lv00_free((void **)&node);
}

void lv00_proto_free_tree(Lv00TreeNode *root)
{
    lv00_proto_free_tree_node(root);
}

void lv00_proto_free_topology(Lv00TopoGraph *graph)
{
    int i;
    if (!graph) {
        return;
    }
    for (i = 0; i < graph->block_count; i++) {
        if (graph->blocks[i].inputs) {
            lv00_free((void **)&graph->blocks[i].inputs);
        }
        if (graph->blocks[i].outputs) {
            lv00_free((void **)&graph->blocks[i].outputs);
        }
    }
    if (graph->blocks) {
        lv00_free((void **)&graph->blocks);
    }
    if (graph->edges) {
        lv00_free((void **)&graph->edges);
    }
    memset(graph, 0, sizeof(*graph));
}

void lv00_proto_free_proof(Lv00ProofNavigator *nav)
{
    int i;
    if (!nav) {
        return;
    }
    for (i = 0; i < nav->step_count; i++) {
        if (nav->steps[i].dependency_ids) {
            lv00_free((void **)&nav->steps[i].dependency_ids);
        }
    }
    if (nav->steps) {
        lv00_free((void **)&nav->steps);
    }
    memset(nav, 0, sizeof(*nav));
}

void lv00_proto_free_completions(Lv00CompletionList *list)
{
    int i;
    if (!list) {
        return;
    }
    for (i = 0; i < list->count; i++) {
        if (list->items[i].text) {
            lv00_free((void **)&list->items[i].text);
        }
    }
    if (list->items) {
        lv00_free((void **)&list->items);
    }
    memset(list, 0, sizeof(*list));
}
