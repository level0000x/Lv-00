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
 * 二、协议生成函数（简单桩实现）
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
        return -1;
    }

    memset(out, 0, sizeof(*out));
    return 0;
}

int lv00_proto_table_rows(void *engine, Lv00TableRowList *out)
{
    (void)engine;

    if (!out) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    return 0;
}

int lv00_proto_dsl_text(void *engine, char *out, size_t buf_size)
{
    (void)engine;

    if (!out || buf_size == 0) {
        return -1;
    }

    snprintf(out, buf_size, "%% Lv-00 DSL Export (stub)\n");
    return 0;
}

int lv00_proto_tree(void *engine, Lv00TreeNode **out_root)
{
    (void)engine;

    if (!out_root) {
        return -1;
    }

    Lv00TreeNode *root = (Lv00TreeNode *)calloc(1, sizeof(Lv00TreeNode));
    if (!root) {
        return -1;
    }

    strncpy(root->id, "root", LV00_PROTO_STR_LEN - 1);
    root->id[LV00_PROTO_STR_LEN - 1] = '\0';

    strncpy(root->label, "Proof Tree", LV00_PROTO_LABEL_LEN - 1);
    root->label[LV00_PROTO_LABEL_LEN - 1] = '\0';

    root->trust_color = LV00_COLOR_GREEN;
    root->status      = LV00_TREE_ROOT;
    root->node_id     = 0;
    root->children    = NULL;
    root->child_count = 0;

    *out_root = root;
    return 0;
}

int lv00_proto_topology(void *engine, Lv00TopoGraph *out)
{
    (void)engine;

    if (!out) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    return 0;
}

int lv00_proto_proof_navigator(void *engine, Lv00ProofNavigator *out)
{
    (void)engine;

    if (!out) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    return 0;
}

int lv00_proto_engine_status(void *engine, Lv00EngineStatus *out)
{
    (void)engine;

    if (!out) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    strncpy(out->engine_state, "idle", sizeof(out->engine_state) - 1);
    out->engine_state[sizeof(out->engine_state) - 1] = '\0';
    return 0;
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
        return -1;
    }

    memset(out, 0, sizeof(*out));

    if (!prefix) {
        prefix = "";
    }

    size_t prefix_len = strlen(prefix);
    int match_count = 0;

    for (size_t i = 0; i < BUILTIN_CMD_COUNT; i++) {
        if (strncmp(kBuiltinCommands[i], prefix, prefix_len) == 0) {
            match_count++;
        }
    }

    if (match_count == 0) {
        return 0;
    }

    out->items = (Lv00Completion *)calloc((size_t)match_count, sizeof(Lv00Completion));
    if (!out->items) {
        return -1;
    }

    int idx = 0;
    for (size_t i = 0; i < BUILTIN_CMD_COUNT; i++) {
        if (strncmp(kBuiltinCommands[i], prefix, prefix_len) == 0) {
            out->items[idx].text = _strdup(kBuiltinCommands[i]);
            if (!out->items[idx].text) {
                lv00_proto_free_completions(out);
                return -1;
            }
            idx++;
        }
    }

    out->count = match_count;
    return 0;
}

int lv00_proto_terminal_exec(void *engine, const char *command,
                             Lv00TerminalResponse *out)
{
    (void)engine;

    if (!out) {
        return -1;
    }

    memset(out, 0, sizeof(*out));

    if (engine && command) {
        snprintf(out->output, sizeof(out->output),
                 "ok: '%s' received", command);
        out->success = 1;
        out->error_code = 0;
    } else {
        snprintf(out->output, sizeof(out->output), "error: invalid input");
        out->success = 0;
        out->error_code = -1;
    }

    return 0;
}

/* ================================================================
 * 四、资源释放
 * ================================================================ */

void lv00_proto_free_draw_commands(Lv00DrawCmdList *list)
{
    if (!list) {
        return;
    }
    free(list->cmds);
    memset(list, 0, sizeof(*list));
}

void lv00_proto_free_table_rows(Lv00TableRowList *list)
{
    if (!list) {
        return;
    }
    free(list->rows);
    memset(list, 0, sizeof(*list));
}

static void lv00_proto_free_tree_node(Lv00TreeNode *node)
{
    if (!node) {
        return;
    }
    for (int i = 0; i < node->child_count; i++) {
        lv00_proto_free_tree_node(node->children[i]);
    }
    free(node->children);
    free(node);
}

void lv00_proto_free_tree(Lv00TreeNode *root)
{
    lv00_proto_free_tree_node(root);
}

void lv00_proto_free_topology(Lv00TopoGraph *graph)
{
    if (!graph) {
        return;
    }
    for (int i = 0; i < graph->block_count; i++) {
        free(graph->blocks[i].inputs);
        free(graph->blocks[i].outputs);
    }
    free(graph->blocks);
    free(graph->edges);
    memset(graph, 0, sizeof(*graph));
}

void lv00_proto_free_proof(Lv00ProofNavigator *nav)
{
    if (!nav) {
        return;
    }
    for (int i = 0; i < nav->step_count; i++) {
        free(nav->steps[i].dependency_ids);
    }
    free(nav->steps);
    memset(nav, 0, sizeof(*nav));
}

void lv00_proto_free_completions(Lv00CompletionList *list)
{
    if (!list) {
        return;
    }
    for (int i = 0; i < list->count; i++) {
        free(list->items[i].text);
    }
    free(list->items);
    memset(list, 0, sizeof(*list));
}
