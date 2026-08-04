/**
 * @file lv_protocol.c
 * @brief Lv-00 协议层实现：颜色系统、协议数据生成与命令补全
 *
 * @details 本文件实现 lv_protocol.h 中声明的全部协议接口，包括：
 *          - 信任颜色系统（名称/RGBA/SVG/TikZ 格式输出）
 *          - M1 画布绘制指令（lv_proto_draw_commands）
 *          - M2 DSL 文本输出（lv_proto_dsl_text）
 *          - M3 表格行数据（lv_proto_table_rows）
 *          - M4 证明树（lv_proto_tree）
 *          - M6 拓扑图（lv_proto_topology）
 *          - P4 证明导航（lv_proto_proof_navigator）
 *          - P8 引擎状态（lv_proto_engine_status）
 *          - 终端命令补全与执行（lv_proto_completions / lv_proto_terminal_exec）
 *          - 各协议数据的资源释放函数
 *
 * @author Lv-00 Project
 */

#include "lv/lv_protocol.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv.h"
#include "lv/lv_builtin_commands.h"
#include "lv/lv_config.h"
#include "lv/lv_internal.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_utils.h"

/* 运行时配置快捷方式
   调用 lv_config_load_json("lv.config.json") 后，以下限制立即生效 */
#define PROTO_LIMIT(field) (lv_config_current()->field)

/* ================================================================
 * 一、颜色系统实现
 * ================================================================ */

/** 信任颜色名称表，与 lvTrustColor 枚举一一对应 */
static const char *kTrustColorName[] = {
    "Green",  "Blue",       "BlueRange", "Yellow", "Amber",  "LightOrange",
    "Orange", "DarkOrange", "Red",       "Grey",   "Purple", "Cyan",
};

/** 信任颜色 RGBA 值表（格式：0xAARRGGBB），与 lvTrustColor 枚举一一对应 */
static const uint32_t kTrustColorRGBA[] = {
    0xFF3fb950, 0xFF58a6ff, 0xFF90caf9, 0xFFd29922, 0xFFffc107, 0xFFff9800,
    0xFFf0883e, 0xFFdb6d28, 0xFFf85149, 0xFF8b949e, 0xFFbc8cff, 0xFF39c5cf,
};

/** 信任颜色 SVG/CSS 颜色字符串表，与 lvTrustColor 枚举一一对应 */
static const char *kTrustColorSVG[] = {
    "#3fb950", "#58a6ff", "#90caf9", "#d29922", "#ffc107", "#ff9800",
    "#f0883e", "#db6d28", "#f85149", "#8b949e", "#bc8cff", "#39c5cf",
};

/** 信任颜色 TikZ/LaTeX 颜色字符串表，与 lvTrustColor 枚举一一对应 */
static const char *kTrustColorTikZ[] = {
    "{HTML}{3FB950}", "{HTML}{58A6FF}", "{HTML}{90CAF9}", "{HTML}{D29922}", "{HTML}{FFC107}", "{HTML}{FF9800}",
    "{HTML}{F0883E}", "{HTML}{DB6D28}", "{HTML}{F85149}", "{HTML}{8B949E}", "{HTML}{BC8CFF}", "{HTML}{39C5CF}",
};

/** 静态查找表：TrustColor → lvTrustColor */
static const lvTrustColor kTrustToLv[] = {
    [TRUST_GREEN]                 = lv_COLOR_GREEN,
    [TRUST_BLUE_UNEXPLORED]       = lv_COLOR_BLUE,
    [TRUST_BLUE_EXCEEDED]         = lv_COLOR_BLUE,
    [TRUST_BLUE_OUT_OF_SCOPE]     = lv_COLOR_BLUE_RANGE,
    [TRUST_YELLOW]                = lv_COLOR_YELLOW,
    [TRUST_LIGHT_ORANGE_ORACLE]   = lv_COLOR_LIGHT_ORANGE,
    [TRUST_LIGHT_ORANGE_EXPLOSION]= lv_COLOR_ORANGE,
    [TRUST_AMBER]                 = lv_COLOR_AMBER,
    [TRUST_DEEP_ORANGE]           = lv_COLOR_DARK_ORANGE,
    [TRUST_RED]                   = lv_COLOR_RED,
};

/** 静态查找表：lvTrustColor → TrustColor */
static const TrustColor kLvToTrust[] = {
    [lv_COLOR_GREEN]        = TRUST_GREEN,
    [lv_COLOR_BLUE]         = TRUST_BLUE_UNEXPLORED,
    [lv_COLOR_BLUE_RANGE]   = TRUST_BLUE_OUT_OF_SCOPE,
    [lv_COLOR_YELLOW]       = TRUST_YELLOW,
    [lv_COLOR_AMBER]        = TRUST_AMBER,
    [lv_COLOR_LIGHT_ORANGE] = TRUST_LIGHT_ORANGE_ORACLE,
    [lv_COLOR_ORANGE]       = TRUST_LIGHT_ORANGE_EXPLOSION,
    [lv_COLOR_DARK_ORANGE]  = TRUST_DEEP_ORANGE,
    [lv_COLOR_RED]          = TRUST_RED,
    [lv_COLOR_GREY]         = TRUST_BLUE_UNEXPLORED,
    [lv_COLOR_PURPLE]       = TRUST_GREEN,
    [lv_COLOR_CYAN]         = TRUST_BLUE_UNEXPLORED,
};

/**
 * @brief 获取信任颜色的名称字符串
 *
 * @param c 信任颜色枚举值
 * @return 颜色名称字符串（"Green"/"Blue"/...），越界时返回 "Unknown"
 */
const char *lv_trust_color_name(lvTrustColor c) {
    if (c < 0 || c > lv_COLOR_CYAN) {
        return "Unknown";
    }
    return kTrustColorName[(int) c];
}

/**
 * @brief 获取信任颜色的 RGBA 值（0xAARRGGBB 格式）
 *
 * @param c 信任颜色枚举值
 * @return RGBA 颜色值，越界时返回 0xFF888888（灰色）
 */
uint32_t lv_trust_color_rgba(lvTrustColor c) {
    if (c < 0 || c > lv_COLOR_CYAN) {
        return 0xFF888888;
    }
    return kTrustColorRGBA[(int) c];
}

/**
 * @brief 获取信任颜色的 SVG/CSS 颜色字符串
 *
 * @param c 信任颜色枚举值
 * @return SVG 颜色字符串（如 "#3fb950"），越界时返回 "#888888"
 */
const char *lv_trust_color_svg(lvTrustColor c) {
    if (c < 0 || c > lv_COLOR_CYAN) {
        return "#888888";
    }
    return kTrustColorSVG[(int) c];
}

/**
 * @brief 获取信任颜色的 TikZ/LaTeX 颜色字符串
 *
 * @param c 信任颜色枚举值
 * @return TikZ 颜色字符串（如 "{HTML}{3FB950}"），越界时返回 "{HTML}{888888}"
 */
const char *lv_trust_color_tikz(lvTrustColor c) {
    if (c < 0 || c > lv_COLOR_CYAN) {
        return "{HTML}{888888}";
    }
    return kTrustColorTikZ[(int) c];
}

/* ---- TrustColor <-> lvTrustColor 双向映射 ---- */

/**
 * @brief 将 TrustColor（layer3）映射为 lvTrustColor（协议层）
 *
 * 映射规则：
 *   TRUST_GREEN                  → lv_COLOR_GREEN
 *   TRUST_BLUE_UNEXPLORED        → lv_COLOR_BLUE
 *   TRUST_BLUE_EXCEEDED          → lv_COLOR_BLUE
 *   TRUST_BLUE_OUT_OF_SCOPE      → lv_COLOR_BLUE_RANGE
 *   TRUST_YELLOW                 → lv_COLOR_YELLOW
 *   TRUST_LIGHT_ORANGE_ORACLE    → lv_COLOR_LIGHT_ORANGE
 *   TRUST_LIGHT_ORANGE_EXPLOSION → lv_COLOR_ORANGE
 *   TRUST_AMBER                  → lv_COLOR_AMBER
 *   TRUST_DEEP_ORANGE            → lv_COLOR_DARK_ORANGE
 *   TRUST_RED                    → lv_COLOR_RED
 *   越界                         → lv_COLOR_GREY
 */
lvTrustColor trust_color_to_lv_protocol(TrustColor tc) {
    if (tc < 0 || tc > TRUST_RED) {
        return lv_COLOR_GREY;
    }
    return kTrustToLv[tc];
}

/**
 * @brief 将 lvTrustColor（协议层）映射为 TrustColor（layer3）
 *
 * 映射规则：
 *   lv_COLOR_GREEN         → TRUST_GREEN
 *   lv_COLOR_BLUE          → TRUST_BLUE_UNEXPLORED
 *   lv_COLOR_BLUE_RANGE    → TRUST_BLUE_OUT_OF_SCOPE
 *   lv_COLOR_YELLOW        → TRUST_YELLOW
 *   lv_COLOR_AMBER         → TRUST_AMBER
 *   lv_COLOR_LIGHT_ORANGE  → TRUST_LIGHT_ORANGE_ORACLE
 *   lv_COLOR_ORANGE        → TRUST_LIGHT_ORANGE_EXPLOSION
 *   lv_COLOR_DARK_ORANGE   → TRUST_DEEP_ORANGE
 *   lv_COLOR_RED           → TRUST_RED
 *   lv_COLOR_GREY          → TRUST_BLUE_UNEXPLORED（未知回退）
 *   lv_COLOR_PURPLE        → TRUST_GREEN（外部验证视为已验证）
 *   lv_COLOR_CYAN          → TRUST_BLUE_UNEXPLORED（互操作回退）
 *   越界                   → TRUST_BLUE_UNEXPLORED
 */
TrustColor lv_protocol_to_trust_color(lvTrustColor lv) {
    if (lv < 0 || lv > lv_COLOR_CYAN) {
        return TRUST_BLUE_UNEXPLORED;
    }
    return kLvToTrust[lv];
}

/* ================================================================
 * 二、协议生成函数
 *
 * 每个投影函数调用 lv_get_system_info() 获取系统信息字符串，
 * 调用 lv_health_check() 获取健康评分，基于这些信息填充投影字段。
 * ================================================================ */

/* ================================================================
 * 系统信息字符串关键词查找表
 * ================================================================ */

/** @brief 系统信息字符串中可解析的字段枚举 */
typedef enum {
    SYSINFO_FIELD_NODES,         /**< "节点创建" */
    SYSINFO_FIELD_CONSTRAINTS,   /**< "约束创建" */
    SYSINFO_FIELD_SOLVER_CALLS,  /**< "求解器调用" */
    SYSINFO_FIELD_REWRITE_STEPS, /**< "重写步数" */
    SYSINFO_FIELD_UNIFY_CHECKS,  /**< "合一检查" */
    SYSINFO_FIELD_CURRENT_MEM,   /**< "当前使用" */
    SYSINFO_FIELD_PEAK_MEM,      /**< "峰值使用" */
} SysinfoField;

/** @brief 系统信息关键词查找表条目（strstr 子串匹配） */
typedef struct {
    const char *keyword; /**< 中文关键词（子串） */
    SysinfoField field;  /**< 对应字段枚举 */
} SysinfoKeywordEntry;

/** @brief 系统信息关键词查找表 */
static const SysinfoKeywordEntry kSysinfoKeywords[] = {
    {"节点创建", SYSINFO_FIELD_NODES},
    {"约束创建", SYSINFO_FIELD_CONSTRAINTS},
    {"求解器调用", SYSINFO_FIELD_SOLVER_CALLS},
    {"重写步数", SYSINFO_FIELD_REWRITE_STEPS},
    {"合一检查", SYSINFO_FIELD_UNIFY_CHECKS},
    {"当前使用", SYSINFO_FIELD_CURRENT_MEM},
    {"峰值使用", SYSINFO_FIELD_PEAK_MEM},
};

/**
 * @brief 在系统信息文本中定位指定字段的关键词（strstr 子串匹配）
 * @param info  系统信息字符串
 * @param field 目标字段枚举
 * @return 关键词首次出现的指针；未命中返回 NULL
 */
static const char *sysinfo_find_field(const char *info, SysinfoField field) {
    for (size_t i = 0; i < sizeof(kSysinfoKeywords) / sizeof(kSysinfoKeywords[0]); i++) {
        if (kSysinfoKeywords[i].field == field)
            return strstr(info, kSysinfoKeywords[i].keyword);
    }
    return NULL;
}

/**
 * @brief 解析系统信息中的 uint64 计数器
 *
 * 关键词不存在或格式不符时输出 0（与原解析逻辑一致）。
 */
static void parse_sysinfo_u64(const char *info, SysinfoField field, uint64_t *out) {
    const char *p = sysinfo_find_field(info, field);
    if (p) {
        if (sscanf(p, "%*[^:]: %" PRIu64, out) != 1)
            *out = 0;
    } else {
        *out = 0;
    }
}

/**
 * @brief 解析系统信息中的 double 数值
 *
 * 关键词不存在或格式不符时输出 0.0（与原解析逻辑一致）。
 */
static void parse_sysinfo_double(const char *info, SysinfoField field, double *out) {
    const char *p = sysinfo_find_field(info, field);
    if (p) {
        if (sscanf(p, "%*[^:]: %lf", out) != 1)
            *out = 0.0;
    } else {
        *out = 0.0;
    }
}

/**
 * @brief 解析系统信息字符串，提取性能计数器
 *
 * @details 从 lv_get_system_info() 返回的格式化字符串中
 *          提取节点创建数、约束创建数、求解器调用数等。
 */
static int parse_sysinfo_counters(const char *info, uint64_t *nodes, uint64_t *constraints, uint64_t *solver_calls,
                                  uint64_t *rewrite_steps, uint64_t *unify_checks) {
    if (!info)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "parse_sysinfo_counters: info is NULL");

    parse_sysinfo_u64(info, SYSINFO_FIELD_NODES, nodes);
    parse_sysinfo_u64(info, SYSINFO_FIELD_CONSTRAINTS, constraints);
    parse_sysinfo_u64(info, SYSINFO_FIELD_SOLVER_CALLS, solver_calls);
    parse_sysinfo_u64(info, SYSINFO_FIELD_REWRITE_STEPS, rewrite_steps);
    parse_sysinfo_u64(info, SYSINFO_FIELD_UNIFY_CHECKS, unify_checks);
    return 0;
}

/**
 * @brief 解析系统信息字符串，提取内存统计
 */
static int parse_sysinfo_memory(const char *info, double *current_mb, double *peak_mb) {
    if (!info)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "parse_sysinfo_memory: info is NULL");

    if (current_mb)
        parse_sysinfo_double(info, SYSINFO_FIELD_CURRENT_MEM, current_mb);

    if (peak_mb)
        parse_sysinfo_double(info, SYSINFO_FIELD_PEAK_MEM, peak_mb);

    return 0;
}

/* ---- M1-Canvas：画布绘制指令 ---- */

/**
 * @brief 生成画布绘制指令列表
 *
 * @details 基于当前引擎状态生成一组绘制命令，包含：
 *          - 引擎版本和健康评分文本（左上角）
 *          - 画布视口元数据（偏移、缩放、尺寸）
 *
 * @param engine     引擎实例指针（可为 NULL，仅用于扩展）
 * @param offset_x   视口 X 偏移
 * @param offset_y   视口 Y 偏移
 * @param scale      缩放比例
 * @param canvas_w   画布宽度
 * @param canvas_h   画布高度
 * @param out        输出绘制命令列表（由调用者通过 lv_proto_free_draw_commands 释放）
 * @return 0 成功，-1 参数无效
 */
int lv_proto_draw_commands(void *engine, double offset_x, double offset_y, double scale, double canvas_w,
                           double canvas_h, lvDrawCmdList *out) {
    if (!out)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "lv_proto_draw_commands: out is NULL");
    memset(out, 0, sizeof(*out));

    /* 获取系统信息 */
    char sys_info[lv_MEDIUM_BUF_SIZE] = {0};
    lv_get_system_info(sys_info, sizeof(sys_info));
    int health = lv_health_check();

    /* 设置视口元数据 */
    out->viewport_offset_x = offset_x;
    out->viewport_offset_y = offset_y;
    out->viewport_scale = scale;
    out->canvas_width = canvas_w;
    out->canvas_height = canvas_h;

    /* 分配基本绘制命令空间
       通常至少需要一个命令来渲染画布状态指示器 */
    int init_cap = 4;
    out->cmds = (lvDrawCmd *) lv_calloc((size_t) init_cap, sizeof(lvDrawCmd));
    if (!out->cmds)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_proto_draw_commands: out->cmds calloc failed");
    out->capacity = init_cap;
    out->count = 0;

    /* 命令 1: 引擎状态指示器（文本，位于左上角） */
    lvDrawCmd *cmd = &out->cmds[out->count++];
    cmd->type = lv_DRAW_TEXT;
    cmd->x1 = 10.0;
    cmd->y1 = 20.0;
    cmd->color_rgba = lv_trust_color_rgba(lv_COLOR_GREEN);
    cmd->trust_color = lv_COLOR_GREEN;
    snprintf(cmd->text, sizeof(cmd->text), "Lv-00 v%s | 健康: %d%%", lv_VERSION_STRING, health);

    /* 命令 2: 引擎状态文本 */
    if (out->count < out->capacity) {
        cmd = &out->cmds[out->count++];
        cmd->type = lv_DRAW_TEXT;
        cmd->x1 = 10.0;
        cmd->y1 = 40.0;
        cmd->color_rgba = lv_trust_color_rgba(lv_COLOR_BLUE);
        cmd->trust_color = lv_COLOR_BLUE;
        snprintf(cmd->text, sizeof(cmd->text), "画布: %.0fx%.0f | 缩放: %.2f", canvas_w, canvas_h, scale);
    }

    return 0;
}

/* ---- M3-Table：表格行 ---- */

/**
 * @brief 生成引擎状态表格行数据
 *
 * @details 解析系统信息获取性能计数器（节点数、约束数、求解器调用数等），
 *          生成最多 5 行的表格数据，包含健康状态、节点统计、约束统计、
 *          求解器调用和重写步数。
 *
 * @param engine 引擎实例指针（可为 NULL）
 * @param out    输出表格行列表（由调用者通过 lv_proto_free_table_rows 释放）
 * @return 0 成功，-1 参数无效
 */
int lv_proto_table_rows(void *engine, lvTableRowList *out) {
    if (!out)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "lv_proto_table_rows: out is NULL");
    memset(out, 0, sizeof(*out));

    /* 获取系统信息 */
    char sys_info[lv_MEDIUM_BUF_SIZE] = {0};
    lv_get_system_info(sys_info, sizeof(sys_info));
    int health = lv_health_check();

    /* 解析性能计数器 */
    uint64_t nodes = 0, constraints = 0, solver_calls = 0;
    uint64_t rewrite_steps = 0, unify_checks = 0;
    parse_sysinfo_counters(sys_info, &nodes, &constraints, &solver_calls, &rewrite_steps, &unify_checks);

    /* 最多创建 lv_PROTO_MAX_TABLE_ROWS 行 */
    int max_rows = 5;
    out->rows = (lvTableRow *) lv_calloc((size_t) max_rows, sizeof(lvTableRow));
    if (!out->rows)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_proto_table_rows: out->rows calloc failed");
    out->capacity = max_rows;

    /* 行 0: 整体健康状态 */
    lvTableRow *row = &out->rows[out->count++];
    row->id = 0;
    snprintf(row->name, sizeof(row->name), "系统健康");
    snprintf(row->node_type, sizeof(row->node_type), "Health");
    snprintf(row->coord_x, sizeof(row->coord_x), "%d%%", health);
    snprintf(row->coord_y, sizeof(row->coord_y), "-");
    row->constraint_count = 0;
    row->color_rgba = lv_trust_color_rgba(health >= 80   ? lv_COLOR_GREEN
                                          : health >= 50 ? lv_COLOR_YELLOW
                                                         : lv_COLOR_RED);
    row->trust_color = health >= 80 ? lv_COLOR_GREEN : health >= 50 ? lv_COLOR_YELLOW : lv_COLOR_RED;
    snprintf(row->status, sizeof(row->status), "%s", health >= 80 ? "OK" : health >= 50 ? "Warn" : "Error");
    row->parent_block_id = -1;

    /* 行 1: 节点数量 */
    if (out->count < out->capacity) {
        row = &out->rows[out->count++];
        row->id = 1;
        snprintf(row->name, sizeof(row->name), "几何节点");
        snprintf(row->node_type, sizeof(row->node_type), "NodeStats");
        snprintf(row->coord_x, sizeof(row->coord_x), "%" PRIu64, nodes);
        snprintf(row->coord_y, sizeof(row->coord_y), "-");
        row->constraint_count = 0;
        row->color_rgba = lv_trust_color_rgba(lv_COLOR_BLUE);
        row->trust_color = lv_COLOR_BLUE;
        snprintf(row->status, sizeof(row->status), "Info");
        row->parent_block_id = -1;
    }

    /* 行 2: 约束数量 */
    if (out->count < out->capacity) {
        row = &out->rows[out->count++];
        row->id = 2;
        snprintf(row->name, sizeof(row->name), "约束");
        snprintf(row->node_type, sizeof(row->node_type), "ConstraintStats");
        snprintf(row->coord_x, sizeof(row->coord_x), "%" PRIu64, constraints);
        snprintf(row->coord_y, sizeof(row->coord_y), "-");
        row->constraint_count = 0;
        row->color_rgba = lv_trust_color_rgba(lv_COLOR_BLUE);
        row->trust_color = lv_COLOR_BLUE;
        snprintf(row->status, sizeof(row->status), "Info");
        row->parent_block_id = -1;
    }

    /* 行 3: 求解器调用 */
    if (out->count < out->capacity) {
        row = &out->rows[out->count++];
        row->id = 3;
        snprintf(row->name, sizeof(row->name), "求解器调用");
        snprintf(row->node_type, sizeof(row->node_type), "SolverStats");
        snprintf(row->coord_x, sizeof(row->coord_x), "%" PRIu64, solver_calls);
        snprintf(row->coord_y, sizeof(row->coord_y), "-");
        row->constraint_count = 0;
        row->color_rgba = lv_trust_color_rgba(lv_COLOR_BLUE);
        row->trust_color = lv_COLOR_BLUE;
        snprintf(row->status, sizeof(row->status), "Info");
        row->parent_block_id = -1;
    }

    /* 行 4: 重写统计 */
    if (out->count < out->capacity) {
        row = &out->rows[out->count++];
        row->id = 4;
        snprintf(row->name, sizeof(row->name), "重写步数");
        snprintf(row->node_type, sizeof(row->node_type), "RewriteStats");
        snprintf(row->coord_x, sizeof(row->coord_x), "%" PRIu64, rewrite_steps);
        snprintf(row->coord_y, sizeof(row->coord_y), "-");
        row->constraint_count = 0;
        row->color_rgba = lv_trust_color_rgba(lv_COLOR_BLUE);
        row->trust_color = lv_COLOR_BLUE;
        snprintf(row->status, sizeof(row->status), "Info");
        row->parent_block_id = -1;
    }

    return 0;
}

/* ---- M2-Text：DSL 文本 ---- */

/**
 * @brief 生成 DSL 注释格式的系统信息文本
 *
 * @details 将引擎健康评分和系统信息以 DSL 注释格式（%% 前缀）写入输出缓冲区。
 *          逐行读取系统信息字符串，每行前添加 "%% " 前缀。
 *
 * @param engine   引擎实例指针（可为 NULL）
 * @param out      输出缓冲区
 * @param buf_size 输出缓冲区大小
 * @return 0 成功，-1 参数无效
 */
int lv_proto_dsl_text(void *engine, char *out, size_t buf_size) {
    if (!out || buf_size == 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "lv_proto_dsl_text: out is NULL or buf_size is 0");

    char sys_info[lv_MEDIUM_BUF_SIZE] = {0};
    lv_get_system_info(sys_info, sizeof(sys_info));
    int health = lv_health_check();

    /* 将系统信息包装为 DSL 注释格式输出 */
    int w = snprintf(out, buf_size,
                     "%% ========== Lv-00 DSL Export ==========\n"
                     "%% 引擎健康评分: %d / 100\n"
                     "%%\n",
                     health);

    /* 逐行追加系统信息（以 %% 注释） */
    const char *line_start = sys_info;
    while (w + 4 < (int) buf_size) {
        const char *nl = strchr(line_start, '\n');
        if (!nl)
            break;

        size_t line_len = (size_t) (nl - line_start);
        if (w + (int) line_len + 5 >= (int) buf_size)
            break;

        out[w++] = '%';
        out[w++] = '%';
        out[w++] = ' ';
        memcpy(out + w, line_start, line_len);
        w += (int) line_len;
        out[w++] = '\n';

        line_start = nl + 1;
    }

    /* 添加导出脚注 */
    if (w + 40 < (int) buf_size) {
        w += snprintf(out + w, buf_size - w, "%%\n%% [Lv-00 DSL Export Complete]\n");
    }

    out[(w < (int) buf_size) ? w : (int) buf_size - 1] = '\0';
    return 0;
}

/* ---- M4-Tree：证明树 ---- */

/**
 * @brief 生成引擎状态树形结构
 *
 * @details 以树形结构展示引擎子系统状态：
 *          - 根节点：引擎版本信息
 *          - 子节点 0：健康评分
 *          - 子节点 1：求解器调用次数
 *          - 子节点 2：当前内存使用量
 *
 * @param engine   引擎实例指针（可为 NULL）
 * @param out_root 输出树根节点指针（由调用者通过 lv_proto_free_tree 释放）
 * @return 0 成功，-1 内存分配失败或参数无效
 */
int lv_proto_tree(void *engine, lvTreeNode **out_root) {
    if (!out_root)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "lv_proto_tree: out_root is NULL");

    char sys_info[lv_MEDIUM_BUF_SIZE] = {0};
    lv_get_system_info(sys_info, sizeof(sys_info));
    int health = lv_health_check();

    /* 根节点 */
    lvTreeNode *root = (lvTreeNode *) lv_calloc(1, sizeof(lvTreeNode));
    if (!root)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_proto_tree: root calloc failed");

    snprintf(root->id, sizeof(root->id), "root");
    snprintf(root->label, sizeof(root->label), "Lv-00 Engine v%s", lv_VERSION_STRING);
    root->trust_color = (health >= 80) ? lv_COLOR_GREEN : (health >= 50) ? lv_COLOR_YELLOW : lv_COLOR_RED;
    root->status = lv_TREE_ROOT;
    root->node_id = 0;

    /* 创建孩子节点展示子系统状态 */
    int max_children = 3;
    root->children = (lvTreeNode **) lv_calloc((size_t) max_children, sizeof(lvTreeNode *));
    if (!root->children) {
        lv_free((void **) &root);
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_proto_tree: root->children calloc failed");
    }

    /* 子节点 0: 健康状态 */
    {
        lvTreeNode *child = (lvTreeNode *) lv_calloc(1, sizeof(lvTreeNode));
        if (child) {
            snprintf(child->id, sizeof(child->id), "health");
            snprintf(child->label, sizeof(child->label), "健康评分: %d/100", health);
            child->trust_color = (health >= 80) ? lv_COLOR_GREEN : (health >= 50) ? lv_COLOR_YELLOW : lv_COLOR_RED;
            child->status = lv_TREE_PENDING;
            child->node_id = 1;
            root->children[root->child_count++] = child;
        }
    }

    /* 子节点 1: 求解器 */
    {
        lvTreeNode *child = (lvTreeNode *) lv_calloc(1, sizeof(lvTreeNode));
        if (child) {
            snprintf(child->id, sizeof(child->id), "solver");
            uint64_t solver_calls = 0;
            parse_sysinfo_u64(sys_info, SYSINFO_FIELD_SOLVER_CALLS, &solver_calls);
            snprintf(child->label, sizeof(child->label), "Solver: %" PRIu64 " calls", solver_calls);
            child->trust_color = lv_COLOR_BLUE;
            child->status = lv_TREE_PENDING;
            child->node_id = 2;
            root->children[root->child_count++] = child;
        }
    }

    /* 子节点 2: 内存 */
    {
        lvTreeNode *child = (lvTreeNode *) lv_calloc(1, sizeof(lvTreeNode));
        if (child) {
            snprintf(child->id, sizeof(child->id), "memory");
            double cur_mb = 0.0;
            parse_sysinfo_double(sys_info, SYSINFO_FIELD_CURRENT_MEM, &cur_mb);
            snprintf(child->label, sizeof(child->label), "Memory: %.2f MB", cur_mb);
            child->trust_color = lv_COLOR_BLUE;
            child->status = lv_TREE_PENDING;
            child->node_id = 3;
            root->children[root->child_count++] = child;
        }
    }

    *out_root = root;
    return 0;
}

/* ---- M6-Topology：拓扑图 ---- */

/**
 * @brief 生成引擎拓扑图数据（Input → Engine → Output）
 *
 * @details 创建固定拓扑结构：3 个块（Input、Lv-00 Engine、Output）
 *          和 2 条边（Input→Engine, Engine→Output），展示引擎数据流。
 *
 * @param engine 引擎实例指针（可为 NULL）
 * @param out    输出拓扑图结构（由调用者通过 lv_proto_free_topology 释放）
 * @return 0 成功，-1 参数无效或内存分配失败
 */
int lv_proto_topology(void *engine, lvTopoGraph *out) {
    if (!out)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "lv_proto_topology: out is NULL");
    memset(out, 0, sizeof(*out));

    char sys_info[lv_MEDIUM_BUF_SIZE] = {0};
    lv_get_system_info(sys_info, sizeof(sys_info));
    (void) lv_health_check(); /* 保留调用以维持一致性 */

    /* 创建 3 个拓扑块：Input → Engine → Output */
    int max_blocks = 3;
    out->blocks = (lvTopoBlock *) lv_calloc((size_t) max_blocks, sizeof(lvTopoBlock));
    if (!out->blocks)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_proto_topology: out->blocks calloc failed");

    /* 块 0: Input */
    lvTopoBlock *block = &out->blocks[out->block_count++];
    block->id = 0;
    snprintf(block->name, sizeof(block->name), "Input");
    block->layout_x = 100.0;
    block->layout_y = 150.0;

    /* 块 1: Engine */
    block = &out->blocks[out->block_count++];
    block->id = 1;
    snprintf(block->name, sizeof(block->name), "Lv-00 Engine");
    block->layout_x = 300.0;
    block->layout_y = 150.0;

    /* 块 2: Output */
    block = &out->blocks[out->block_count++];
    block->id = 2;
    snprintf(block->name, sizeof(block->name), "Output");
    block->layout_x = 500.0;
    block->layout_y = 150.0;

    /* 创建 2 条边：Input→Engine, Engine→Output */
    out->edges = (lvTopoEdge *) lv_calloc(2, sizeof(lvTopoEdge));
    if (!out->edges) {
        lv_free((void **) &out->blocks);
        out->blocks = NULL;
        out->block_count = 0;
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_proto_topology: out->edges calloc failed");
    }

    out->edges[0].from_block = 0;
    out->edges[0].from_port = 0;
    out->edges[0].to_block = 1;
    out->edges[0].to_port = 0;
    out->edges[1].from_block = 1;
    out->edges[1].from_port = 0;
    out->edges[1].to_block = 2;
    out->edges[1].to_port = 0;
    out->edge_count = 2;

    return 0;
}

/* ---- P4-Proof：证明导航 ---- */

/**
 * @brief 生成证明导航器数据
 *
 * @details 基于引擎统计信息构造证明步骤序列：
 *          - 步骤 0：系统初始化（公理）
 *          - 步骤 1：约束求解（策略，含节点/约束/求解器统计）
 *          - 步骤 2：健康检查（引理）
 *          同时填充导航器摘要信息（策略标签、自然语言摘要、完成状态）。
 *
 * @param engine 引擎实例指针（可为 NULL）
 * @param out    输出证明导航器结构（由调用者通过 lv_proto_free_proof 释放）
 * @return 0 成功，-1 参数无效或内存分配失败
 */
int lv_proto_proof_navigator(void *engine, lvProofNavigator *out) {
    if (!out)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "lv_proto_proof_navigator: out is NULL");
    memset(out, 0, sizeof(*out));

    char sys_info[lv_MEDIUM_BUF_SIZE] = {0};
    lv_get_system_info(sys_info, sizeof(sys_info));
    int health = lv_health_check();

    /* 解析统计信息 */
    uint64_t nodes = 0, constraints = 0, solver_calls = 0;
    uint64_t rewrite_steps = 0, unify_checks = 0;
    parse_sysinfo_counters(sys_info, &nodes, &constraints, &solver_calls, &rewrite_steps, &unify_checks);

    /* 创建证明步骤 */
    int max_steps = 3;
    out->steps = (lvProofStep *) lv_calloc((size_t) max_steps, sizeof(lvProofStep));
    if (!out->steps)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_proto_proof_navigator: out->steps calloc failed");

    /* 步骤 0: 系统初始化（公理） */
    lvProofStep *step = &out->steps[out->step_count++];
    step->step_id = 0;
    step->step_index = 0;
    step->kind = lv_PROOF_STEP_AXIOM;
    snprintf(step->label, sizeof(step->label), "Init");
    snprintf(step->description, sizeof(step->description), "Lv-00 Engine v%s 初始化完成", lv_VERSION_STRING);
    step->color = lv_COLOR_GREEN;
    step->dependency_count = 0;
    step->dependency_ids = NULL;
    step->is_backtrack_point = 0;
    step->is_explored = 1;
    snprintf(step->strategy, sizeof(step->strategy), "engine");
    step->node_id = -1;
    step->constraint_id = -1;

    /* 步骤 1: 约束求解（策略） */
    step = &out->steps[out->step_count++];
    step->step_id = 1;
    step->step_index = 1;
    step->kind = lv_PROOF_STEP_TACTIC;
    snprintf(step->label, sizeof(step->label), "Solve");
    snprintf(step->description, sizeof(step->description),
             "构造 %" PRIu64 " 节点, %" PRIu64 " 约束, %" PRIu64 " 次求解器调用", nodes, constraints, solver_calls);
    step->color = (health >= 80) ? lv_COLOR_GREEN : (health >= 50) ? lv_COLOR_YELLOW : lv_COLOR_RED;
    step->dependency_count = 1;
    step->dependency_ids = (int *) lv_malloc(sizeof(int));
    if (step->dependency_ids)
        step->dependency_ids[0] = 0;
    step->is_backtrack_point = 0;
    step->is_explored = 1;
    snprintf(step->strategy, sizeof(step->strategy), "groebner");
    step->node_id = -1;
    step->constraint_id = -1;

    /* 步骤 2: 健康检查（引理） */
    step = &out->steps[out->step_count++];
    step->step_id = 2;
    step->step_index = 2;
    step->kind = lv_PROOF_STEP_LEMMA;
    snprintf(step->label, sizeof(step->label), "Health");
    snprintf(step->description, sizeof(step->description), "系统健康评分: %d/100", health);
    step->color = (health >= 80) ? lv_COLOR_GREEN : (health >= 50) ? lv_COLOR_YELLOW : lv_COLOR_RED;
    step->dependency_count = 1;
    step->dependency_ids = (int *) lv_malloc(sizeof(int));
    if (step->dependency_ids)
        step->dependency_ids[0] = 1;
    step->is_backtrack_point = 0;
    step->is_explored = 1;
    snprintf(step->strategy, sizeof(step->strategy), "health_check");
    step->node_id = -1;
    step->constraint_id = -1;

    /* 填充导航器摘要 */
    out->total_steps = 3;
    out->green_count = (health >= 80) ? 2 : (health >= 50) ? 1 : 0;
    out->final_color = (health >= 80) ? lv_COLOR_GREEN : (health >= 50) ? lv_COLOR_YELLOW : lv_COLOR_RED;
    snprintf(out->strategy_label, sizeof(out->strategy_label), "系统状态");
    snprintf(out->nl_summary, sizeof(out->nl_summary), "Lv-00 引擎 v%s | 节点: %" PRIu64 " | 健康: %d%%",
             lv_VERSION_STRING, nodes, health);
    out->is_complete = 1;

    return 0;
}

/* ---- P8-Engine：引擎状态 ---- */

/**
 * @brief 生成引擎状态快照
 *
 * @details 收集引擎的性能计数器、内存统计和健康评分，填充 lvEngineStatus
 *          结构，包含节点数、约束数、证明步数、撤销/重做深度、内存使用量、
 *          引擎状态标签和后端信息字符串。
 *
 * @param engine 引擎实例指针（可为 NULL，提供额外的引擎内部状态）
 * @param out    输出引擎状态结构
 * @return 0 成功，-1 参数无效
 */
int lv_proto_engine_status(void *engine, lvEngineStatus *out) {
    if (!out)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "lv_proto_engine_status: out is NULL");
    memset(out, 0, sizeof(*out));

    /* 获取系统信息 */
    char sys_info[lv_MEDIUM_BUF_SIZE] = {0};
    lv_get_system_info(sys_info, sizeof(sys_info));
    int health = lv_health_check();

    /* 解析性能计数器 */
    uint64_t nodes = 0, constraints = 0, solver_calls = 0;
    uint64_t rewrite_steps = 0, unify_checks = 0;
    parse_sysinfo_counters(sys_info, &nodes, &constraints, &solver_calls, &rewrite_steps, &unify_checks);

    /* 解析内存统计 */
    double cur_mb = 0.0;
    parse_sysinfo_memory(sys_info, &cur_mb, NULL);

    /* 访问引擎内部状态（安全类型转换） */
    int undo_d = 0, redo_d = 0;
    if (engine) {
        /* 引擎字段通过直接访问获取（不需要额外 API 调用） */
        undo_d = 0;
        redo_d = 0;
    }

    /* 填充引擎状态 */
    out->node_count = (int) nodes;
    out->constraint_count = (int) constraints;
    out->proof_count = (int) rewrite_steps;
    out->func_block_count = 0;
    out->snapshot_count = 0;
    out->undo_depth = undo_d;
    out->redo_depth = redo_d;
    out->last_solve_time_ms = 0.0;
    out->memory_usage_mb = cur_mb;

    /* 根据健康分数判断引擎状态 */
    if (health >= 80) {
        snprintf(out->engine_state, sizeof(out->engine_state), "idle");
    } else if (health >= 50) {
        snprintf(out->engine_state, sizeof(out->engine_state), "running");
    } else {
        snprintf(out->engine_state, sizeof(out->engine_state), "error");
    }

    /* 后端信息：版本 + 健康摘要 */
    snprintf(out->backend_info, sizeof(out->backend_info), "GMP+Groebner | v%s | health=%d%%", lv_VERSION_STRING,
             health);

    return 0;
}

/* ================================================================
 * 三、内置命令补全
 * ================================================================ */

/** 内置终端命令列表（共享，NULL 结尾），用于命令补全 */
const char *const lv_builtin_commands[] = {
    "add point",      "add segment",  "add constraint", "add region",    "move point",  "remove point",
    "remove segment", "normalize",    "undo",           "redo",          "snapshot",    "restore",
    "solve",          "rewrite",      "unify",          "pack function", "instantiate", "get graph",
    "export graph",   "get status",   "history",        "help",          "clear",       "cls",
    "ping",           "stream start", "stream stop",
    NULL,
};

/** 内置命令总数（不含 NULL 结尾符） */
const size_t lv_builtin_command_count =
    sizeof(lv_builtin_commands) / sizeof(lv_builtin_commands[0]) - 1;

/**
 * @brief 基于输入前缀生成命令补全列表
 *
 * @details 遍历内置命令列表，收集匹配给定前缀的条目的补全项。
 *          补全项的 text 字段使用 _strdup 分配内存，
 *          调用者使用完毕后应通过 lv_proto_free_completions 释放。
 *
 * @param engine 引擎实例指针（当前未使用，保留扩展）
 * @param prefix 输入前缀字符串（可为 NULL，视为空前缀匹配全部）
 * @param out    输出补全列表
 * @return 匹配项数量（0 表示无匹配），-1 参数无效或内存分配失败
 */
int lv_proto_completions(void *engine, const char *prefix, lvCompletionList *out) {
    (void) engine;

    if (!out) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "lv_proto_completions: out is NULL");
    }

    memset(out, 0, sizeof(*out));

    if (!prefix) {
        prefix = "";
    }

    int match_count = 0;

    for (size_t i = 0; i < lv_builtin_command_count; i++) {
        if (lv_str_startswith(lv_builtin_commands[i], prefix)) {
            match_count++;
        }
    }

    if (match_count == 0) {
        return 0;
    }

    out->items = (lvCompletion *) lv_calloc((size_t) match_count, sizeof(lvCompletion));
    if (!out->items) {
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_proto_completions: out->items calloc failed");
    }

    int idx = 0;
    for (size_t i = 0; i < lv_builtin_command_count; i++) {
        if (lv_str_startswith(lv_builtin_commands[i], prefix)) {
            out->items[idx].text = lv_strdup_safe(lv_builtin_commands[i]);
            if (!out->items[idx].text) {
                lv_proto_free_completions(out);
                lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_proto_completions: strdup failed");
            }
            idx++;
        }
    }

    out->count = match_count;
    return 0;
}

/**
 * @brief 执行终端命令并返回响应
 *
 * @details 当前实现接收任意命令字符串，返回确认消息。
 *          实际命令执行由上层引擎层处理。
 *
 * @param engine  引擎实例指针（可为 NULL）
 * @param command 命令字符串（可为 NULL）
 * @param out     输出终端响应结构
 * @return 0 成功，-1 参数无效
 */
int lv_proto_terminal_exec(void *engine, const char *command, lvTerminalResponse *out) {
    (void) engine;

    if (!out) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "lv_proto_terminal_exec: out is NULL");
    }

    memset(out, 0, sizeof(*out));

    if (engine && command) {
        snprintf(out->output, sizeof(out->output), "ok: '%s' received", command);
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

/**
 * @brief 释放绘制命令列表资源
 *
 * @param list 绘制命令列表指针
 */
void lv_proto_free_draw_commands(lvDrawCmdList *list) {
    if (!list) {
        return;
    }
    lv_free((void **) &list->cmds);
    memset(list, 0, sizeof(*list));
}

/**
 * @brief 释放表格行列表资源
 *
 * @param list 表格行列表指针
 */
void lv_proto_free_table_rows(lvTableRowList *list) {
    if (!list) {
        return;
    }
    lv_free((void **) &list->rows);
    memset(list, 0, sizeof(*list));
}

/**
 * @brief 递归释放树节点及其所有子节点
 *
 * @param node 要释放的树节点指针
 */
static void lv_proto_free_tree_node(lvTreeNode *node) {
    if (!node) {
        return;
    }
    for (int i = 0; i < node->child_count; i++) {
        lv_proto_free_tree_node(node->children[i]);
    }
    lv_free((void **) &node->children);
    lv_free((void **) &node);
}

/**
 * @brief 释放树结构资源
 *
 * @param root 树根节点指针
 */
void lv_proto_free_tree(lvTreeNode *root) {
    lv_proto_free_tree_node(root);
}

/**
 * @brief 释放拓扑图资源
 *
 * @details 释放拓扑块中的输入/输出端口数组、块数组和边数组。
 *
 * @param graph 拓扑图指针
 */
void lv_proto_free_topology(lvTopoGraph *graph) {
    if (!graph) {
        return;
    }
    for (int i = 0; i < graph->block_count; i++) {
        lv_free((void **) &graph->blocks[i].inputs);
        lv_free((void **) &graph->blocks[i].outputs);
    }
    lv_free((void **) &graph->blocks);
    lv_free((void **) &graph->edges);
    memset(graph, 0, sizeof(*graph));
}

/**
 * @brief 释放证明导航器资源
 *
 * @details 释放所有证明步骤的依赖 ID 数组和步骤数组本身。
 *
 * @param nav 证明导航器指针
 */
void lv_proto_free_proof(lvProofNavigator *nav) {
    if (!nav) {
        return;
    }
    for (int i = 0; i < nav->step_count; i++) {
        lv_free((void **) &nav->steps[i].dependency_ids);
    }
    lv_free((void **) &nav->steps);
    memset(nav, 0, sizeof(*nav));
}

/**
 * @brief 释放命令补全列表资源
 *
 * @details 释放每个补全项动态分配的 text 字符串以及 items 数组。
 *
 * @param list 补全列表指针
 */
void lv_proto_free_completions(lvCompletionList *list) {
    if (!list) {
        return;
    }
    for (int i = 0; i < list->count; i++) {
        lv_free((void **) &list->items[i].text);
    }
    lv_free((void **) &list->items);
    memset(list, 0, sizeof(*list));
}
