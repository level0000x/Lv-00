/**
 * @file interop_command_export.c
 * @brief ExportGraph 命令族与导出辅助函数（由 interop_command.c 拆分子模块）
 *
 * @details ExportGraph 命令的格式分发（json/svg/tikz/json-pretty/coq/lean/
 *          html/pdf/geojson）、响应游标与信任颜色/类型名称辅助函数。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/lv_platform.h"
#include <float.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "lv/constraint_graph.h"
#include "lv/engine.h"
#include "lv/interop.h"
#include "lv/lv_json.h"
#include "lv/lv_parse_utils.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_xmacro.h"

#include "lv/debug.h"
#include "interop_export_internal.h" /* 公共信任颜色全字段表 kTrustColorEntries */
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/stream.h" /* LV_STREAM_CTX_DEFINE */
#include "interop_command_internal.h"

/* ── ExportGraph 导出格式分发（查找表，替代 4 分支 strcmp 链） ── */

/* ── ExportGraph 响应游标骨架：游标推进 + 越界防护 ──
 * 收敛 svg/tikz/json_pretty 三处重复的「snprintf → 越界守卫 → 游标推进」样板。
 * 语义与历史实现一致：vsnprintf 返回码 n<0 使游标进入错误态（后续不再追加）；
 * 截断（n 超过剩余容量）仍推进游标，使后续追加自然短路（等价原 offset < sizeof 守卫）。
 * 导出格式串为外部契约，内容不得修改。 */

typedef struct {
    char *out;   /* 输出缓冲区（resp->data） */
    size_t size; /* 缓冲区总容量 */
    int offset;  /* 当前游标；<0 表示错误态（不再追加） */
} RespCursor;

static void resp_cursor_init(RespCursor *c, char *out, size_t size) {
    c->out = out;
    c->size = size;
    c->offset = 0;
}

static void resp_cursor_printf(RespCursor *c, const char *fmt, ...) {
    if (c->offset < 0 || c->offset >= (int) c->size)
        return;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(c->out + c->offset, c->size - (size_t) c->offset, fmt, ap);
    va_end(ap);
    c->offset += n;
}

static void resp_cursor_char(RespCursor *c, char ch) {
    if (c->offset < 0 || c->offset >= (int) c->size - 1)
        return;
    c->out[c->offset++] = ch;
}

static void resp_cursor_spaces(RespCursor *c, int count) {
    if (c->offset < 0)
        return;
    for (int i = 0; i < count && c->offset < (int) c->size - 1; i++)
        c->out[c->offset++] = ' ';
}

static void resp_cursor_finish(RespCursor *c) {
    if (c->offset < 0)
        return;
    if (c->offset >= (int) c->size)
        c->offset = (int) c->size - 1;
    c->out[c->offset] = '\0';
}

static int interop_export_graph_json(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    (void) cmd;
    char *json_str = graph_serialize_to_json(engine->main_graph);
    if (json_str) {
        lv_strlcpy(resp->data, json_str, sizeof(resp->data));
        lv_free((void **) &json_str);
    } else {
        lv_strlcpy(resp->data, "{\"error\": \"Serialization failed\"}", sizeof(resp->data));
    }
    return lv_OK;
}

static int interop_export_graph_svg(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    (void) cmd;
    RespCursor cur;
    resp_cursor_init(&cur, resp->data, sizeof(resp->data));
    resp_cursor_printf(&cur,
                       "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"800\" height=\"600\">\n"
                       "  <rect width=\"100%%\" height=\"100%%\" fill=\"white\"/>\n");
    if (cur.offset < 0)
        cur.offset = 0;
    if (engine->main_graph) {
        for (int i = 0; i < engine->main_graph->node_count && cur.offset < (int) sizeof(resp->data) - 256; i++) {
            GeomNode *node = engine->main_graph->nodes[i];
            if (node->type == GEOM_POINT && node->coord_count >= 2) {
                double x = symbolic_coord_to_double(node->symbolic_coords[0]);
                double y = symbolic_coord_to_double(node->symbolic_coords[1]);
                resp_cursor_printf(&cur,
                                   "  <circle cx=\"%.2f\" cy=\"%.2f\" r=\"4\" fill=\"#3b82f6\"/>\n", x, y);
            } else if (node->type == GEOM_LINE_SEGMENT) {
                resp_cursor_printf(&cur,
                                   "  <line x1=\"0\" y1=\"0\" x2=\"100\" y2=\"100\" stroke=\"#22c55e\" "
                                   "stroke-width=\"2\"/>\n");
            }
        }
    }
    resp_cursor_printf(&cur, "</svg>");
    return lv_OK;
}

static int interop_export_graph_tikz(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    (void) cmd;
    RespCursor cur;
    resp_cursor_init(&cur, resp->data, sizeof(resp->data));
    resp_cursor_printf(&cur, "\\begin{tikzpicture}\n");
    if (cur.offset < 0)
        cur.offset = 0;
    if (engine->main_graph) {
        for (int i = 0; i < engine->main_graph->node_count && cur.offset < (int) sizeof(resp->data) - 256; i++) {
            GeomNode *node = engine->main_graph->nodes[i];
            if (node->type == GEOM_POINT && node->coord_count >= 2) {
                double x = symbolic_coord_to_double(node->symbolic_coords[0]);
                double y = symbolic_coord_to_double(node->symbolic_coords[1]);
                resp_cursor_printf(&cur,
                                   "  \\coordinate (P%d) at (%.2f, %.2f);\n", node->id, x, y);
            } else if (node->type == GEOM_LINE_SEGMENT) {
                resp_cursor_printf(&cur, "  \\draw (0,0) -- (1,1);\n");
            }
        }
    }
    resp_cursor_printf(&cur, "\\end{tikzpicture}");
    return lv_OK;
}

static int interop_export_graph_json_pretty(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    (void) cmd;
    char *json_str = graph_serialize_to_json(engine->main_graph);
    if (json_str) {
        RespCursor cur;
        resp_cursor_init(&cur, resp->data, sizeof(resp->data));
        int indent = 0;
        for (size_t i = 0; json_str[i] && cur.offset < (int) sizeof(resp->data) - 4; i++) {
            char ch = json_str[i];
            if (ch == '{' || ch == '[') {
                resp_cursor_char(&cur, ch);
                resp_cursor_char(&cur, '\n');
                indent += 2;
                resp_cursor_spaces(&cur, indent);
            } else if (ch == '}' || ch == ']') {
                resp_cursor_char(&cur, '\n');
                indent -= 2;
                if (indent < 0)
                    indent = 0;
                resp_cursor_spaces(&cur, indent);
                resp_cursor_char(&cur, ch);
            } else if (ch == ',') {
                resp_cursor_char(&cur, ch);
                resp_cursor_char(&cur, '\n');
                resp_cursor_spaces(&cur, indent);
            } else {
                resp_cursor_char(&cur, ch);
            }
        }
        resp_cursor_finish(&cur);
        lv_free((void **) &json_str);
    } else {
        lv_strlcpy(resp->data, "{\"error\": \"Serialization failed\"}", sizeof(resp->data));
    }
    return lv_OK;
}

/* ── 文件型导出格式（coq/lean/html/pdf/geojson）：写入磁盘文件 ── */

/* interop.h 未声明 interop_export_pdf（其余导出函数已在 interop.h 声明），
 * 此处补充与 interop_export_pdf.c 定义一致的前向声明。 */
int interop_export_pdf(const ConstraintGraph *graph, const InteropExportConfig *config);

/**
 * @brief 将约束图导出为文件型格式
 *
 * ExportGraph 命令协议：`ExportGraph <fmt> [output_path]`，
 * 未提供 output_path 时使用默认路径 def_path。
 * coq/lean 需要 ProofNavigator（此处以空导航器导出框架）；
 * html/pdf/geojson 分别消费 engine / main_graph。
 */
static int interop_export_graph_to_file(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp,
                                        InteropExportFormat format, const char *def_path, bool needs_navigator) {
    char path[INTEROP_MAX_PATH_LEN];
    if (cmd->param_count > 1 && cmd->params[1][0] != '\0') {
        lv_strlcpy(path, cmd->params[1], sizeof(path));
    } else {
        lv_strlcpy(path, def_path, sizeof(path));
    }
    InteropExportConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.format = format;
    cfg.include_proofs = true;
    cfg.include_metadata = true;
    cfg.pretty_print = true;
    lv_strlcpy(cfg.output_path, path, sizeof(cfg.output_path));

    int rc = lv_ERROR_UNKNOWN;
    if (needs_navigator) {
        ProofNavigator *nav = proof_navigator_create(NULL, engine);
        if (!nav) {
            resp->status_code = lv_ERROR_OUT_OF_MEMORY;
            lv_strlcpy(resp->data,
                       "{\"result\": \"failed\", \"reason\": \"Out of memory creating proof navigator\"}",
                       sizeof(resp->data));
            return lv_OK;
        }
        if (format == INTEROP_EXPORT_COQ)
            rc = interop_export_coq(nav, &cfg);
        else if (format == INTEROP_EXPORT_LEAN)
            rc = interop_export_lean(nav, &cfg);
        proof_navigator_destroy(nav);
    } else {
        if (format == INTEROP_EXPORT_HTML)
            rc = interop_export_html(engine, &cfg);
        else if (format == INTEROP_EXPORT_PDF)
            rc = interop_export_pdf(engine->main_graph, &cfg);
        else if (format == INTEROP_EXPORT_GEOJSON)
            rc = interop_export_geojson(engine->main_graph, &cfg);
    }
    const char *fmt_name = interop_export_format_name(format);
    if (rc == lv_OK) {
        snprintf(resp->data, sizeof(resp->data), "{\"result\": \"ok\", \"format\": \"%s\", \"path\": \"%s\"}",
                 fmt_name, path);
    } else {
        resp->status_code = rc;
        snprintf(resp->data, sizeof(resp->data),
                 "{\"result\": \"failed\", \"format\": \"%s\", \"path\": \"%s\", \"code\": %d}", fmt_name, path, rc);
    }
    return lv_OK;
}

static int interop_export_graph_coq(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    return interop_export_graph_to_file(engine, cmd, resp, INTEROP_EXPORT_COQ, "export_graph.v", true);
}

static int interop_export_graph_lean(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    return interop_export_graph_to_file(engine, cmd, resp, INTEROP_EXPORT_LEAN, "export_graph.lean", true);
}

static int interop_export_graph_html(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    return interop_export_graph_to_file(engine, cmd, resp, INTEROP_EXPORT_HTML, "export_graph.html", false);
}

static int interop_export_graph_pdf(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    return interop_export_graph_to_file(engine, cmd, resp, INTEROP_EXPORT_PDF, "export_graph.pdf", false);
}

static int interop_export_graph_geojson(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    return interop_export_graph_to_file(engine, cmd, resp, INTEROP_EXPORT_GEOJSON, "export_graph.geojson", false);
}

/** @brief ExportGraph 导出格式名→处理函数 查找表（覆盖 interop_theorem.c 注册的全部格式） */
static const struct {
    const char *name;
    InteropCmdHandler handler;
} kExportFormatHandlers[] = {
    {"json", interop_export_graph_json},
    {"canonical", interop_export_graph_json},
    {"svg", interop_export_graph_svg},
    {"tikz", interop_export_graph_tikz},
    {"json-pretty", interop_export_graph_json_pretty},
    {"coq", interop_export_graph_coq},
    {"lean", interop_export_graph_lean},
    {"html", interop_export_graph_html},
    {"pdf", interop_export_graph_pdf},
    {"geojson", interop_export_graph_geojson},
};

int handle_cmd_export_graph(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp) {
    const char *fmt = (cmd->param_count > 0) ? cmd->params[0] : "json";
    if (!engine->main_graph) {
        resp->status_code = lv_ERROR_INVALID_STATE;
        lv_strlcpy(resp->data, "No graph to export", sizeof(resp->data));
        return lv_OK;
    }
    /* 导出格式→处理函数 查表（替代 4 分支 strcmp 链） */
    for (size_t i = 0; i < lv_ARRAY_SIZE(kExportFormatHandlers); i++) {
        if (lv_str_eq(fmt, kExportFormatHandlers[i].name))
            return kExportFormatHandlers[i].handler(engine, cmd, resp);
    }
    resp->status_code = lv_ERROR_UNSUPPORTED;
    snprintf(resp->data, sizeof(resp->data), "Unsupported export format: %s", fmt);
    return lv_OK;
}

/* StreamStart 命令注册的流式回调 ID（-1 = 未注册）。
 * 与 interop_server.c 的 server->stream_callback_id 字段同语义：
 * StreamStop 按此 ID 注销，保证「注册/注销」配对，防止回调列表无限增长。 */

/* ==================== 导出辅助函数 ==================== */

/**
 * @brief 获取信任颜色对应的SVG颜色字符串
 *
 * 将内部 TrustColor 枚举值映射为 SVG 可用的十六进制颜色代码。
 *
 * @param trust 信任颜色枚举值
 * @return 对应的 SVG 颜色字符串（如 "#22c55e"），未知颜色返回 "#9ca3af"
 */
/* ================================================================
 * 枚举 -> 名称 映射表（数据表化，替代 switch）
 * ================================================================ */

const char *interop_trust_color_to_svg(TrustColor trust) {
    /* 查公共信任颜色全字段表（interop_export_internal.h），未命中返回默认灰色 */
    const TrustColorEntry *e = interop_trust_color_find(trust);
    return e ? e->svg_hex : "#9ca3af";
}

/**
 * @brief 获取信任颜色对应的TikZ颜色字符串
 *
 * 将内部 TrustColor 枚举值映射为 TikZ/LaTeX 可用的颜色表达式。
 *
 * @param trust 信任颜色枚举值
 * @return 对应的 TikZ 颜色字符串（如 "green!70!black"），未知颜色返回 "gray"
 */
const char *interop_trust_color_to_tikz(TrustColor trust) {
    /* 查公共信任颜色全字段表（interop_export_internal.h），未命中返回默认 gray */
    const TrustColorEntry *e = interop_trust_color_find(trust);
    return e ? e->tikz_expr : "gray";
}

/**
 * @brief 获取几何类型名称字符串
 *
 * 将 GeomType 枚举值映射为可读的英文名称字符串。
 *
 * @param type 几何类型枚举值
 * @return 对应的类型名称字符串（如 "point"、"line_segment"），未知类型返回 "unknown"
 */
/** @brief interop_geom_type_name 由共享条目宏 API 提供
 *  @note 对外命令协议要求小写名（"point"），由 constraint_graph.h 的
 *        LV_GEOM_TYPE_ENTRY 别名列生成（lv_geom_type_alias），
 *        原手写小写表 s_interop_geom_type_name_entries 已删除。 */
const char *interop_geom_type_name(GeomType type) {
    const char *alias = lv_geom_type_alias((int) type);
    /* lv_geom_type_alias 越界返回 "UNKNOWN"，此处保持本接口既有的 "unknown" 回退语义 */
    return (alias && lv_str_ne(alias, "UNKNOWN")) ? alias : "unknown";
}

/**
 * @brief 获取约束类型名称字符串
 *
 * 将 ConstraintType 枚举值映射为可读的英文名称字符串。
 *
 * @param type 约束类型枚举值
 * @return 对应的类型名称字符串（如 "incidence"、"betweenness"），未知类型返回 "unknown"
 */
/** @brief interop_constraint_type_name 由共享条目宏 API 提供
 *  @note 小写别名由 constraint_graph.h 的 LV_CONSTRAINT_TYPE_ENTRY 别名列生成
 *        （lv_constraint_type_alias），原手写小写表已删除。 */
const char *interop_constraint_type_name(ConstraintType type) {
    const char *alias = lv_constraint_type_alias((int) type);
    return (alias && lv_str_ne(alias, "UNKNOWN")) ? alias : "unknown";
}
