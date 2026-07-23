/**
 * @file command_log.c
 * @brief 命令日志系统实现
 *
 * @details 实现"唯一真相源"设计：所有图编辑操作转为同构的原子命令序列。
 *          支持记录、重放、序列化和增量同步。
 *
 * @version 1.1.0
 */

#include "lv/command_log.h"
#include "lv/lv_internal.h"
#include "lv/constraint_graph.h"
#include "lv/engine.h"
#include "lv/lv.h"
#include "lv/lv_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* ════════════════════════════════════════════════════════════════
 *  内部数据结构
 * ════════════════════════════════════════════════════════════════ */

struct CommandLog {
    CommandEntry **entries;   /**< 条目指针数组 */
    int            count;     /**< 当前条目数 */
    int            capacity;  /**< 数组容量 */
    int64_t        next_seq;  /**< 下一个可用序列号 */
};

/* ════════════════════════════════════════════════════════════════
 *  命令类型名称表
 * ════════════════════════════════════════════════════════════════ */

const char *g_command_type_names[CMD_COUNT] = {
    "ADD_NODE",
    "ADD_CONSTRAINT",
    "REMOVE_NODE",
    "REMOVE_CONSTRAINT",
    "PACK_FUNCTION",
    "NORMALIZE_GRAPH",
    "UNIFY",
    "SET_NUMERIC_ASSUMPTION"
};

/* ════════════════════════════════════════════════════════════════
 *  辅助函数
 * ════════════════════════════════════════════════════════════════ */

/** 获取当前毫秒时间戳（跨平台） */
static int64_t get_current_ms(void) {
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER li;
    li.LowPart  = ft.dwLowDateTime;
    li.HighPart = ft.dwHighDateTime;
    /* Windows epoch 1601-01-01 → Unix epoch 1970-01-01 偏移 11644473600 秒 */
    return (int64_t)((li.QuadPart - 116444736000000000ULL) / 10000);
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000 + (int64_t)(ts.tv_nsec / 1000000);
#endif
}

/** 销毁命令条目的内部动态内存（不释放 entry 本身）*/
static void command_entry_cleanup(CommandEntry *entry) {
    if (!entry) return;
    switch (entry->type) {
    case CMD_ADD_NODE:
        lv_free((void **)&entry->params.add_node.coords_num);
        lv_free((void **)&entry->params.add_node.coords_den);
        break;
    case CMD_PACK_FUNCTION:
        lv_free((void **)&entry->params.pack_function.internal_node_ids);
        lv_free((void **)&entry->params.pack_function.input_port_ids);
        lv_free((void **)&entry->params.pack_function.output_port_ids);
        break;
    default:
        break;
    }
    command_entry_destroy(entry->inverse);
    entry->inverse = NULL;
}

/* ════════════════════════════════════════════════════════════════
 *  日志生命周期
 * ════════════════════════════════════════════════════════════════ */

CommandLog *command_log_create(int initial_capacity) {
    CommandLog *log = (CommandLog *)lv_calloc(1, sizeof(CommandLog));
    if (!log) return NULL;

    log->capacity = (initial_capacity > 0) ? initial_capacity : 1024;
    log->entries = (CommandEntry **)lv_calloc((size_t)log->capacity, sizeof(CommandEntry *));
    if (!log->entries) {
        lv_free((void **)&log);
        return NULL;
    }
    log->count = 0;
    log->next_seq = 0;
    return log;
}

void command_log_destroy(CommandLog *log) {
    if (!log) return;
    command_log_clear(log);
    lv_free((void **)&log->entries);
    lv_free((void **)&log);
}

/* ════════════════════════════════════════════════════════════════
 *  日志操作
 * ════════════════════════════════════════════════════════════════ */

bool command_log_append(CommandLog *log, CommandEntry *entry) {
    if (!log || !entry) return false;

    /* 自动分配序列号和时间戳 */
    entry->seq = log->next_seq++;
    entry->timestamp_ms = get_current_ms();

    /* 扩容 */
    if (log->count >= log->capacity) {
        int new_cap = log->capacity * 2;
        CommandEntry **new_entries = (CommandEntry **)lv_realloc(
            log->entries, (size_t)new_cap * sizeof(CommandEntry *));
        if (!new_entries) return false;
        log->entries = new_entries;
        log->capacity = new_cap;
    }

    log->entries[log->count++] = entry;
    return true;
}

int command_log_count(const CommandLog *log) {
    return log ? log->count : 0;
}

const CommandEntry *command_log_get(const CommandLog *log, int index) {
    if (!log || index < 0 || index >= log->count) return NULL;
    return log->entries[index];
}

void command_log_clear(CommandLog *log) {
    if (!log) return;
    for (int i = 0; i < log->count; i++) {
        command_entry_cleanup(log->entries[i]);
        lv_free((void **)&log->entries[i]);
    }
    log->count = 0;
    log->next_seq = 0;
}

int64_t command_log_current_seq(const CommandLog *log) {
    return log ? log->next_seq : 0;
}

/* ════════════════════════════════════════════════════════════════
 *  便利构造函数
 * ════════════════════════════════════════════════════════════════ */

CommandEntry *command_entry_create_add_node(int geom_type, int node_id,
    int coord_count, const double *nums, const uint64_t *dens)
{
    CommandEntry *entry = (CommandEntry *)lv_calloc(1, sizeof(CommandEntry));
    if (!entry) return NULL;
    entry->type = CMD_ADD_NODE;
    entry->params.add_node.geom_type      = geom_type;
    entry->params.add_node.node_id        = node_id;
    entry->params.add_node.coord_count    = coord_count;
    entry->params.add_node.namespace_depth = 0;
    entry->params.add_node.parent_block_id = -1;
    entry->params.add_node.is_formal_param = false;

    if (coord_count > 0 && nums && dens) {
        entry->params.add_node.coords_num = (double *)lv_malloc(
            (size_t)coord_count * sizeof(double));
        entry->params.add_node.coords_den = (uint64_t *)lv_malloc(
            (size_t)coord_count * sizeof(uint64_t));
        if (!entry->params.add_node.coords_num ||
            !entry->params.add_node.coords_den) {
            lv_free((void **)&entry->params.add_node.coords_num);
            lv_free((void **)&entry->params.add_node.coords_den);
            lv_free((void **)&entry);
            return NULL;
        }
        memcpy(entry->params.add_node.coords_num, nums,
               (size_t)coord_count * sizeof(double));
        memcpy(entry->params.add_node.coords_den, dens,
               (size_t)coord_count * sizeof(uint64_t));
    }
    return entry;
}

CommandEntry *command_entry_create_add_constraint(int constr_type,
    int constr_id, const int *participants, int participant_count)
{
    CommandEntry *entry = (CommandEntry *)lv_calloc(1, sizeof(CommandEntry));
    if (!entry) return NULL;
    entry->type = CMD_ADD_CONSTRAINT;
    entry->params.add_constraint.constraint_type  = constr_type;
    entry->params.add_constraint.constraint_id    = constr_id;
    entry->params.add_constraint.participant_count =
        (participant_count > 8) ? 8 : participant_count;
    for (int i = 0; i < entry->params.add_constraint.participant_count; i++) {
        entry->params.add_constraint.participant_ids[i] = participants[i];
    }
    return entry;
}

CommandEntry *command_entry_create_remove_node(int node_id) {
    CommandEntry *entry = (CommandEntry *)lv_calloc(1, sizeof(CommandEntry));
    if (!entry) return NULL;
    entry->type = CMD_REMOVE_NODE;
    entry->params.remove_node.node_id = node_id;
    return entry;
}

CommandEntry *command_entry_create_remove_constraint(int constraint_idx) {
    CommandEntry *entry = (CommandEntry *)lv_calloc(1, sizeof(CommandEntry));
    if (!entry) return NULL;
    entry->type = CMD_REMOVE_CONSTRAINT;
    entry->params.remove_constraint.constraint_index = constraint_idx;
    return entry;
}

CommandEntry *command_entry_create_pack_function(int internal_count,
    const int *internal_ids, int input_count, const int *input_ports,
    int output_count, const int *output_ports)
{
    CommandEntry *entry = (CommandEntry *)lv_calloc(1, sizeof(CommandEntry));
    if (!entry) return NULL;
    entry->type = CMD_PACK_FUNCTION;
    entry->params.pack_function.internal_count = internal_count;
    entry->params.pack_function.input_count   = input_count;
    entry->params.pack_function.output_count  = output_count;
    entry->params.pack_function.result_func_id = -1;

    if (internal_count > 0 && internal_ids) {
        entry->params.pack_function.internal_node_ids = (int *)lv_malloc(
            (size_t)internal_count * sizeof(int));
        if (entry->params.pack_function.internal_node_ids)
            memcpy(entry->params.pack_function.internal_node_ids, internal_ids,
                   (size_t)internal_count * sizeof(int));
    }
    if (input_count > 0 && input_ports) {
        entry->params.pack_function.input_port_ids = (int *)lv_malloc(
            (size_t)input_count * sizeof(int));
        if (entry->params.pack_function.input_port_ids)
            memcpy(entry->params.pack_function.input_port_ids, input_ports,
                   (size_t)input_count * sizeof(int));
    }
    if (output_count > 0 && output_ports) {
        entry->params.pack_function.output_port_ids = (int *)lv_malloc(
            (size_t)output_count * sizeof(int));
        if (entry->params.pack_function.output_port_ids)
            memcpy(entry->params.pack_function.output_port_ids, output_ports,
                   (size_t)output_count * sizeof(int));
    }
    return entry;
}

CommandEntry *command_entry_create_normalize_graph(bool scope_aware,
    int max_iterations)
{
    CommandEntry *entry = (CommandEntry *)lv_calloc(1, sizeof(CommandEntry));
    if (!entry) return NULL;
    entry->type = CMD_NORMALIZE_GRAPH;
    entry->params.normalize_graph.scope_aware    = scope_aware;
    entry->params.normalize_graph.max_iterations = max_iterations;
    return entry;
}

CommandEntry *command_entry_create_unify(int construction_graph_id,
    int proposition_graph_id)
{
    CommandEntry *entry = (CommandEntry *)lv_calloc(1, sizeof(CommandEntry));
    if (!entry) return NULL;
    entry->type = CMD_UNIFY;
    entry->params.unify.construction_graph_id = construction_graph_id;
    entry->params.unify.proposition_graph_id  = proposition_graph_id;
    entry->params.unify.result = false;
    return entry;
}

CommandEntry *command_entry_create_set_numeric_assumption(int node_id,
    double precision, const char *declaration)
{
    CommandEntry *entry = (CommandEntry *)lv_calloc(1, sizeof(CommandEntry));
    if (!entry) return NULL;
    entry->type = CMD_SET_NUMERIC_ASSUMPTION;
    entry->params.set_numeric_assumption.node_id    = node_id;
    entry->params.set_numeric_assumption.precision  = precision;
    if (declaration) {
        lv_strncpy(entry->params.set_numeric_assumption.declaration,
                   declaration,
                   sizeof(entry->params.set_numeric_assumption.declaration));
    }
    return entry;
}

void command_entry_destroy(CommandEntry *entry) {
    if (!entry) return;
    command_entry_cleanup(entry);
    lv_free((void **)&entry);
}

/* ════════════════════════════════════════════════════════════════
 *  命令执行
 * ════════════════════════════════════════════════════════════════ */

/**
 * @brief 执行单条命令（不记录日志）
 */
static bool execute_command(CommandEntry *entry, lvEngine *engine) {
    if (!entry || !engine) return false;

    switch (entry->type) {
    case CMD_ADD_NODE: {
        CmdAddNodeParams *p = &entry->params.add_node;
        if (p->coord_count >= 2 && p->coords_num && p->coords_den) {
            /* 创建 SymbolicCoord 数组 */
            SymbolicCoord **coords = (SymbolicCoord **)lv_malloc(
                (size_t)p->coord_count * sizeof(SymbolicCoord *));
            if (!coords) return false;
            for (int i = 0; i < p->coord_count; i++) {
                coords[i] = symbolic_coord_create_rational(
                    (int64_t)p->coords_num[i], p->coords_den[i]);
                if (!coords[i]) {
                    for (int j = 0; j < i; j++)
                        symbolic_coord_destroy(coords[j]);
                    lv_free((void **)&coords);
                    return false;
                }
            }
            int id = lv_add_point(engine, p->coords_num[0], p->coords_den[0],
                                  p->coords_num[1], p->coords_den[1]);
            for (int i = 0; i < p->coord_count; i++)
                symbolic_coord_destroy(coords[i]);
            lv_free((void **)&coords);
            return id >= 0;
        }
        return false;
    }

    case CMD_ADD_CONSTRAINT: {
        CmdAddConstraintParams *p = &entry->params.add_constraint;
        /* 简化：通过引擎 API 添加约束 */
        if (p->participant_count >= 2) {
            /* 使用 incidence 约束作为通用 fallback */
            for (int i = 1; i < p->participant_count; i++) {
                lv_add_constraint_incidence(engine, p->participant_ids[i],
                                            p->participant_ids[0]);
            }
            return true;
        }
        return false;
    }

    case CMD_REMOVE_NODE: {
        /* 引擎层未暴露删除 API — 通过底层图操作 */
        /* constraint_graph.h: graph_remove_node */
        return false; /* 占位：需要引擎封装 */
    }

    case CMD_REMOVE_CONSTRAINT: {
        return false; /* 占位 */
    }

    case CMD_PACK_FUNCTION: {
        CmdPackFunctionParams *p = &entry->params.pack_function;
        int func_id = -1;
        bool ok = engine_pack_function(engine,
            p->internal_node_ids, p->internal_count,
            p->input_port_ids, p->input_count,
            p->output_port_ids, p->output_count, &func_id);
        p->result_func_id = func_id;
        return ok;
    }

    case CMD_NORMALIZE_GRAPH: {
        /* 规范化通过引擎重写管线 */
        engine_rewrite_and_solve(engine,
            entry->params.normalize_graph.max_iterations, 0);
        return true;
    }

    case CMD_UNIFY: {
        /* 合一操作需要两个图 — 当前简化 */
        return false; /* 占位 */
    }

    case CMD_SET_NUMERIC_ASSUMPTION: {
        CmdSetNumericAssumptionParams *p = &entry->params.set_numeric_assumption;
        int rc = lv_set_numeric_assumption(engine, p->node_id,
                                           p->precision, p->declaration);
        return rc == 0;
    }

    default:
        return false;
    }
}

bool command_log_execute(CommandLog *log, CommandEntry *entry, lvEngine *engine) {
    if (!log || !entry || !engine) {
        command_entry_destroy(entry);
        return false;
    }

    bool ok = execute_command(entry, engine);
    if (ok) {
        return command_log_append(log, entry);
    } else {
        command_entry_destroy(entry);
        return false;
    }
}

bool command_log_replay(CommandLog *log, lvEngine *engine) {
    return command_log_replay_from(log, engine, -1);
}

bool command_log_replay_from(CommandLog *log, lvEngine *engine, int64_t from_seq) {
    if (!log || !engine) return false;

    for (int i = 0; i < log->count; i++) {
        CommandEntry *entry = log->entries[i];
        if (entry->seq <= from_seq) continue;
        if (!execute_command(entry, engine)) {
            LOG_ERROR("command_log", "回放失败: seq=%lld type=%s",
                      (long long)entry->seq,
                      g_command_type_names[entry->type]);
            return false;
        }
    }
    return true;
}

/* ════════════════════════════════════════════════════════════════
 *  JSON 序列化 —— 输出完整命令参数
 * ════════════════════════════════════════════════════════════════ */

static void json_escape_string(FILE *fp, const char *s) {
    fputc('"', fp);
    while (s && *s) {
        switch (*s) {
        case '"':  fputs("\\\"", fp); break;
        case '\\': fputs("\\\\", fp); break;
        case '\n': fputs("\\n", fp); break;
        case '\r': fputs("\\r", fp); break;
        case '\t': fputs("\\t", fp); break;
        default:   fputc(*s, fp);
        }
        s++;
    }
    fputc('"', fp);
}

/** 输出一个 int 数组作为 JSON 数组 */
static void json_int_array(FILE *fp, const int *arr, int count) {
    fputs("[", fp);
    for (int i = 0; i < count; i++) {
        if (i > 0) fputs(", ", fp);
        fprintf(fp, "%d", arr[i]);
    }
    fputs("]", fp);
}

/** 输出一个 double 数组作为 JSON 数组 */
static void json_double_array(FILE *fp, const double *arr, int count) {
    fputs("[", fp);
    for (int i = 0; i < count; i++) {
        if (i > 0) fputs(", ", fp);
        fprintf(fp, "%.17g", arr[i]);
    }
    fputs("]", fp);
}

/** 输出一个 uint64_t 数组作为 JSON 数组 */
static void json_uint64_array(FILE *fp, const uint64_t *arr, int count) {
    fputs("[", fp);
    for (int i = 0; i < count; i++) {
        if (i > 0) fputs(", ", fp);
        fprintf(fp, "%llu", (unsigned long long)arr[i]);
    }
    fputs("]", fp);
}

/** 输出命令参数的 JSON 对象体（不含外层花括号） */
static void json_write_params(FILE *fp, const CommandEntry *e) {
    switch (e->type) {
    case CMD_ADD_NODE: {
        const CmdAddNodeParams *p = &e->params.add_node;
        fprintf(fp, "\"geom_type\": %d,\n", p->geom_type);
        fprintf(fp, "      \"node_id\": %d,\n", p->node_id);
        fprintf(fp, "      \"coord_count\": %d,\n", p->coord_count);
        fprintf(fp, "      \"namespace_depth\": %d,\n", p->namespace_depth);
        fprintf(fp, "      \"parent_block_id\": %d,\n", p->parent_block_id);
        fprintf(fp, "      \"is_formal_param\": %s,\n",
                p->is_formal_param ? "true" : "false");
        fprintf(fp, "      \"coords_num\": ");
        if (p->coords_num && p->coord_count > 0) {
            json_double_array(fp, p->coords_num, p->coord_count);
        } else {
            fputs("null", fp);
        }
        fputs(",\n", fp);
        fprintf(fp, "      \"coords_den\": ");
        if (p->coords_den && p->coord_count > 0) {
            json_uint64_array(fp, p->coords_den, p->coord_count);
        } else {
            fputs("null", fp);
        }
        break;
    }
    case CMD_ADD_CONSTRAINT: {
        const CmdAddConstraintParams *p = &e->params.add_constraint;
        fprintf(fp, "\"constraint_type\": %d,\n", p->constraint_type);
        fprintf(fp, "      \"constraint_id\": %d,\n", p->constraint_id);
        fprintf(fp, "      \"participant_count\": %d,\n", p->participant_count);
        fprintf(fp, "      \"participant_ids\": ");
        json_int_array(fp, p->participant_ids, p->participant_count);
        break;
    }
    case CMD_REMOVE_NODE: {
        fprintf(fp, "\"node_id\": %d", e->params.remove_node.node_id);
        break;
    }
    case CMD_REMOVE_CONSTRAINT: {
        fprintf(fp, "\"constraint_index\": %d",
                e->params.remove_constraint.constraint_index);
        break;
    }
    case CMD_PACK_FUNCTION: {
        const CmdPackFunctionParams *p = &e->params.pack_function;
        fprintf(fp, "\"internal_count\": %d,\n", p->internal_count);
        fprintf(fp, "      \"input_count\": %d,\n", p->input_count);
        fprintf(fp, "      \"output_count\": %d,\n", p->output_count);
        fprintf(fp, "      \"result_func_id\": %d,\n", p->result_func_id);
        fprintf(fp, "      \"internal_node_ids\": ");
        if (p->internal_node_ids && p->internal_count > 0)
            json_int_array(fp, p->internal_node_ids, p->internal_count);
        else
            fputs("null", fp);
        fputs(",\n", fp);
        fprintf(fp, "      \"input_port_ids\": ");
        if (p->input_port_ids && p->input_count > 0)
            json_int_array(fp, p->input_port_ids, p->input_count);
        else
            fputs("null", fp);
        fputs(",\n", fp);
        fprintf(fp, "      \"output_port_ids\": ");
        if (p->output_port_ids && p->output_count > 0)
            json_int_array(fp, p->output_port_ids, p->output_count);
        else
            fputs("null", fp);
        break;
    }
    case CMD_NORMALIZE_GRAPH: {
        fprintf(fp, "\"scope_aware\": %s,\n",
                e->params.normalize_graph.scope_aware ? "true" : "false");
        fprintf(fp, "      \"max_iterations\": %d",
                e->params.normalize_graph.max_iterations);
        break;
    }
    case CMD_UNIFY: {
        fprintf(fp, "\"construction_graph_id\": %d,\n",
                e->params.unify.construction_graph_id);
        fprintf(fp, "      \"proposition_graph_id\": %d,\n",
                e->params.unify.proposition_graph_id);
        fprintf(fp, "      \"result\": %s",
                e->params.unify.result ? "true" : "false");
        break;
    }
    case CMD_SET_NUMERIC_ASSUMPTION: {
        const CmdSetNumericAssumptionParams *p = &e->params.set_numeric_assumption;
        fprintf(fp, "\"node_id\": %d,\n", p->node_id);
        fprintf(fp, "      \"precision\": %.17g,\n", p->precision);
        fprintf(fp, "      \"declaration\": ");
        json_escape_string(fp, p->declaration);
        break;
    }
    default:
        fputs("\"type\": \"unknown\"", fp);
        break;
    }
}

bool command_log_serialize_json(const CommandLog *log, const char *filepath) {
    if (!log || !filepath) return false;

    FILE *fp = fopen(filepath, "w");
    if (!fp) return false;

    fprintf(fp, "{\n  \"version\": 1,\n  \"entries\": [\n");

    for (int i = 0; i < log->count; i++) {
        const CommandEntry *e = log->entries[i];
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"type\": \"%s\",\n", g_command_type_names[e->type]);
        fprintf(fp, "      \"seq\": %lld,\n", (long long)e->seq);
        fprintf(fp, "      \"timestamp_ms\": %lld,\n", (long long)e->timestamp_ms);
        fprintf(fp, "      \"params\": {\n        ");
        json_write_params(fp, e);
        fprintf(fp, "\n      }\n");
        fprintf(fp, "    }");
        if (i < log->count - 1) fputs(",", fp);
        fputs("\n", fp);
    }

    fprintf(fp, "  ]\n}\n");
    fclose(fp);
    return true;
}

/* ════════════════════════════════════════════════════════════════
 *  JSON 反序列化 —— 最小 JSON 解析器
 * ════════════════════════════════════════════════════════════════ */

/** 轻量 JSON 解析上下文 */
typedef struct {
    const char *buf;   /* 输入缓冲区 */
    size_t pos;        /* 当前解析位置 */
    size_t len;        /* 缓冲区长度 */
} JsonCtx;

/** 跳过空白字符 */
static void json_skip_ws(JsonCtx *j) {
    while (j->pos < j->len) {
        char c = j->buf[j->pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
            j->pos++;
        else
            break;
    }
}

/** 查看当前字符（不推进） */
static char json_peek(JsonCtx *j) {
    json_skip_ws(j);
    return (j->pos < j->len) ? j->buf[j->pos] : '\0';
}

/** 消费一个字符 */
static char json_next(JsonCtx *j) {
    json_skip_ws(j);
    return (j->pos < j->len) ? j->buf[j->pos++] : '\0';
}

/** 期望特定字符，返回 true 成功 */
static bool json_expect(JsonCtx *j, char expected) {
    char c = json_next(j);
    if (c != expected) return false;
    return true;
}

/** 解析 JSON 字符串字面量，写入 dst 缓冲区 */
static bool json_parse_string(JsonCtx *j, char *dst, size_t dst_size) {
    if (json_next(j) != '"') return false;
    size_t i = 0;
    while (j->pos < j->len) {
        char c = j->buf[j->pos++];
        if (c == '"') {
            dst[i] = '\0';
            return true;
        }
        if (c == '\\' && j->pos < j->len) {
            char esc = j->buf[j->pos++];
            switch (esc) {
            case '"':  c = '"';  break;
            case '\\': c = '\\'; break;
            case 'n':  c = '\n'; break;
            case 'r':  c = '\r'; break;
            case 't':  c = '\t'; break;
            default:   c = esc;  break;
            }
        }
        if (i < dst_size - 1) dst[i++] = c;
    }
    dst[i] = '\0';
    return false; /* 未闭合的字符串 */
}

/** 解析 JSON 整数 */
static bool json_parse_int(JsonCtx *j, int *out) {
    json_skip_ws(j);
    if (j->pos >= j->len) return false;
    long val = 0;
    int sign = 1;
    size_t start = j->pos;
    if (j->buf[j->pos] == '-') { sign = -1; j->pos++; }
    if (j->pos >= j->len || j->buf[j->pos] < '0' || j->buf[j->pos] > '9')
        return false;
    while (j->pos < j->len && j->buf[j->pos] >= '0' && j->buf[j->pos] <= '9') {
        val = val * 10 + (j->buf[j->pos] - '0');
        j->pos++;
    }
    *out = (int)(sign * val);
    return j->pos > start + (sign == -1 ? 1 : 0);
}

/** 解析 JSON 长整数 */
static bool json_parse_int64(JsonCtx *j, int64_t *out) {
    json_skip_ws(j);
    if (j->pos >= j->len) return false;
    long long val = 0;
    int sign = 1;
    size_t start = j->pos;
    if (j->buf[j->pos] == '-') { sign = -1; j->pos++; }
    if (j->pos >= j->len || j->buf[j->pos] < '0' || j->buf[j->pos] > '9')
        return false;
    while (j->pos < j->len && j->buf[j->pos] >= '0' && j->buf[j->pos] <= '9') {
        val = val * 10 + (j->buf[j->pos] - '0');
        j->pos++;
    }
    *out = (int64_t)(sign * val);
    return true;
}

/** 解析 JSON 浮点数 */
static bool json_parse_double(JsonCtx *j, double *out) {
    json_skip_ws(j);
    if (j->pos >= j->len) return false;
    size_t start = j->pos;
    /* 处理可能的负号 */
    if (j->buf[j->pos] == '-') j->pos++;
    /* 数字部分 */
    bool has_digit = false;
    while (j->pos < j->len && j->buf[j->pos] >= '0' && j->buf[j->pos] <= '9') {
        has_digit = true;
        j->pos++;
    }
    if (j->pos < j->len && j->buf[j->pos] == '.') {
        j->pos++;
        while (j->pos < j->len && j->buf[j->pos] >= '0' && j->buf[j->pos] <= '9') {
            has_digit = true;
            j->pos++;
        }
    }
    /* 科学计数法 */
    if (has_digit && j->pos < j->len &&
        (j->buf[j->pos] == 'e' || j->buf[j->pos] == 'E')) {
        j->pos++;
        if (j->pos < j->len && (j->buf[j->pos] == '+' || j->buf[j->pos] == '-'))
            j->pos++;
        while (j->pos < j->len && j->buf[j->pos] >= '0' && j->buf[j->pos] <= '9')
            j->pos++;
    }
    if (!has_digit) return false;
    /* 用 sscanf 转换，需要临时缓冲区 */
    size_t len = j->pos - start;
    char tmp[128];
    if (len >= sizeof(tmp)) return false;
    memcpy(tmp, j->buf + start, len);
    tmp[len] = '\0';
    *out = strtod(tmp, NULL);
    return true;
}

/** 解析 JSON true/false */
static bool json_parse_bool(JsonCtx *j, bool *out) {
    json_skip_ws(j);
    if (j->pos + 4 <= j->len && memcmp(j->buf + j->pos, "true", 4) == 0) {
        *out = true; j->pos += 4; return true;
    }
    if (j->pos + 5 <= j->len && memcmp(j->buf + j->pos, "false", 5) == 0) {
        *out = false; j->pos += 5; return true;
    }
    return false;
}

/** 解析 JSON null */
static bool json_parse_null(JsonCtx *j) {
    json_skip_ws(j);
    if (j->pos + 4 <= j->len && memcmp(j->buf + j->pos, "null", 4) == 0) {
        j->pos += 4; return true;
    }
    return false;
}

/** 解析 JSON int 数组 [...] */
static int json_parse_int_array(JsonCtx *j, int **out) {
    *out = NULL;
    if (json_next(j) != '[') return 0;
    /* 空数组 */
    if (json_peek(j) == ']') { json_next(j); return 0; }
    int cap = 16, count = 0;
    int *arr = (int *)lv_malloc((size_t)cap * sizeof(int));
    if (!arr) return 0;
    while (1) {
        if (count >= cap) {
            cap *= 2;
            int *tmp = (int *)lv_realloc(arr, (size_t)cap * sizeof(int));
            if (!tmp) { lv_free((void **)&arr); return 0; }
            arr = tmp;
        }
        if (!json_parse_int(j, &arr[count])) {
            lv_free((void **)&arr); return 0;
        }
        count++;
        if (json_peek(j) == ']') break;
        if (!json_expect(j, ',')) { lv_free((void **)&arr); return 0; }
    }
    json_next(j); /* 消费 ']' */
    *out = arr;
    return count;
}

/** 解析 JSON double 数组 [...] */
static int json_parse_double_array(JsonCtx *j, double **out) {
    *out = NULL;
    if (json_next(j) != '[') return 0;
    if (json_peek(j) == ']') { json_next(j); return 0; }
    int cap = 16, count = 0;
    double *arr = (double *)lv_malloc((size_t)cap * sizeof(double));
    if (!arr) return 0;
    while (1) {
        if (count >= cap) {
            cap *= 2;
            double *tmp = (double *)lv_realloc(arr, (size_t)cap * sizeof(double));
            if (!tmp) { lv_free((void **)&arr); return 0; }
            arr = tmp;
        }
        if (!json_parse_double(j, &arr[count])) {
            lv_free((void **)&arr); return 0;
        }
        count++;
        if (json_peek(j) == ']') break;
        if (!json_expect(j, ',')) { lv_free((void **)&arr); return 0; }
    }
    json_next(j); /* 消费 ']' */
    *out = arr;
    return count;
}

/** 解析 JSON uint64_t 数组 [...] */
static int json_parse_uint64_array(JsonCtx *j, uint64_t **out) {
    *out = NULL;
    if (json_next(j) != '[') return 0;
    if (json_peek(j) == ']') { json_next(j); return 0; }
    int cap = 16, count = 0;
    uint64_t *arr = (uint64_t *)lv_malloc((size_t)cap * sizeof(uint64_t));
    if (!arr) return 0;
    while (1) {
        if (count >= cap) {
            cap *= 2;
            uint64_t *tmp = (uint64_t *)lv_realloc(arr, (size_t)cap * sizeof(uint64_t));
            if (!tmp) { lv_free((void **)&arr); return 0; }
            arr = tmp;
        }
        /* JSON 无符号 → 解析为 int64 再转换 */
        int64_t v = 0;
        if (!json_parse_int64(j, &v)) {
            lv_free((void **)&arr); return 0;
        }
        arr[count++] = (uint64_t)v;
        if (json_peek(j) == ']') break;
        if (!json_expect(j, ',')) { lv_free((void **)&arr); return 0; }
    }
    json_next(j); /* 消费 ']' */
    *out = arr;
    return count;
}

/** 跳过 JSON 值（对象/数组/字符串/数字/布尔/null） */
static void json_skip_value(JsonCtx *j) {
    json_skip_ws(j);
    if (j->pos >= j->len) return;
    char c = j->buf[j->pos];
    if (c == '{') {
        /* 跳过整个对象 */
        int depth = 0;
        do {
            if (c == '{') depth++;
            if (c == '}') depth--;
            j->pos++;
            if (j->pos >= j->len) return;
            c = j->buf[j->pos];
        } while (depth > 0);
    } else if (c == '[') {
        int depth = 0;
        do {
            if (c == '[') depth++;
            if (c == ']') depth--;
            j->pos++;
            if (j->pos >= j->len) return;
            c = j->buf[j->pos];
        } while (depth > 0);
    } else if (c == '"') {
        j->pos++;
        while (j->pos < j->len) {
            if (j->buf[j->pos] == '"') { j->pos++; return; }
            if (j->buf[j->pos] == '\\' && j->pos + 1 < j->len) j->pos++;
            j->pos++;
        }
    } else if (c == 't' || c == 'f') {
        if (memcmp(j->buf + j->pos, "true", 4) == 0) j->pos += 4;
        else if (memcmp(j->buf + j->pos, "false", 5) == 0) j->pos += 5;
        else j->pos++;
    } else if (c == 'n') {
        j->pos += 4; /* null */
    } else {
        /* 数字 */
        while (j->pos < j->len && (j->buf[j->pos] == '-' || j->buf[j->pos] == '+' ||
               j->buf[j->pos] == '.' || j->buf[j->pos] == 'e' || j->buf[j->pos] == 'E' ||
               (j->buf[j->pos] >= '0' && j->buf[j->pos] <= '9')))
            j->pos++;
    }
}

/** 查找并解析命令参数字段 */
static void json_parse_params(JsonCtx *j, CommandEntry *e) {
    /* 期望 "params": { ... } */
    char key[64];
    if (!json_parse_string(j, key, sizeof(key))) return;
    if (json_expect(j, ':') && json_peek(j) == '{') {
        json_next(j); /* 消费 '{' */

        while (json_peek(j) != '}') {
            char k[64];
            if (!json_parse_string(j, k, sizeof(k))) break;
            if (!json_expect(j, ':')) break;

            switch (e->type) {
            case CMD_ADD_NODE: {
                CmdAddNodeParams *p = &e->params.add_node;
                if (strcmp(k, "geom_type") == 0) json_parse_int(j, &p->geom_type);
                else if (strcmp(k, "node_id") == 0) json_parse_int(j, &p->node_id);
                else if (strcmp(k, "coord_count") == 0) json_parse_int(j, &p->coord_count);
                else if (strcmp(k, "namespace_depth") == 0) json_parse_int(j, &p->namespace_depth);
                else if (strcmp(k, "parent_block_id") == 0) json_parse_int(j, &p->parent_block_id);
                else if (strcmp(k, "is_formal_param") == 0) json_parse_bool(j, &p->is_formal_param);
                else if (strcmp(k, "coords_num") == 0) {
                    if (json_peek(j) == 'n') { json_parse_null(j); }
                    else { lv_free((void **)&p->coords_num);
                           json_parse_double_array(j, &p->coords_num); }
                }
                else if (strcmp(k, "coords_den") == 0) {
                    if (json_peek(j) == 'n') { json_parse_null(j); }
                    else { lv_free((void **)&p->coords_den);
                           json_parse_uint64_array(j, &p->coords_den); }
                }
                else json_skip_value(j);
                break;
            }
            case CMD_ADD_CONSTRAINT: {
                CmdAddConstraintParams *p = &e->params.add_constraint;
                if (strcmp(k, "constraint_type") == 0) json_parse_int(j, &p->constraint_type);
                else if (strcmp(k, "constraint_id") == 0) json_parse_int(j, &p->constraint_id);
                else if (strcmp(k, "participant_count") == 0) json_parse_int(j, &p->participant_count);
                else if (strcmp(k, "participant_ids") == 0) {
                    int *arr = NULL;
                    int cnt = json_parse_int_array(j, &arr);
                    if (arr) {
                        int n = (cnt > 8) ? 8 : cnt;
                        memcpy(p->participant_ids, arr, (size_t)n * sizeof(int));
                        lv_free((void **)&arr);
                    }
                }
                else json_skip_value(j);
                break;
            }
            case CMD_REMOVE_NODE: {
                if (strcmp(k, "node_id") == 0)
                    json_parse_int(j, &e->params.remove_node.node_id);
                else json_skip_value(j);
                break;
            }
            case CMD_REMOVE_CONSTRAINT: {
                if (strcmp(k, "constraint_index") == 0)
                    json_parse_int(j, &e->params.remove_constraint.constraint_index);
                else json_skip_value(j);
                break;
            }
            case CMD_PACK_FUNCTION: {
                CmdPackFunctionParams *p = &e->params.pack_function;
                if (strcmp(k, "internal_count") == 0) json_parse_int(j, &p->internal_count);
                else if (strcmp(k, "input_count") == 0) json_parse_int(j, &p->input_count);
                else if (strcmp(k, "output_count") == 0) json_parse_int(j, &p->output_count);
                else if (strcmp(k, "result_func_id") == 0) json_parse_int(j, &p->result_func_id);
                else if (strcmp(k, "internal_node_ids") == 0) {
                    if (json_peek(j) == 'n') json_parse_null(j);
                    else { int *arr = NULL; (void)json_parse_int_array(j, &arr);
                           lv_free((void **)&p->internal_node_ids);
                           p->internal_node_ids = arr; }
                }
                else if (strcmp(k, "input_port_ids") == 0) {
                    if (json_peek(j) == 'n') json_parse_null(j);
                    else { int *arr = NULL; (void)json_parse_int_array(j, &arr);
                           lv_free((void **)&p->input_port_ids);
                           p->input_port_ids = arr; }
                }
                else if (strcmp(k, "output_port_ids") == 0) {
                    if (json_peek(j) == 'n') json_parse_null(j);
                    else { int *arr = NULL; (void)json_parse_int_array(j, &arr);
                           lv_free((void **)&p->output_port_ids);
                           p->output_port_ids = arr; }
                }
                else json_skip_value(j);
                break;
            }
            case CMD_NORMALIZE_GRAPH: {
                if (strcmp(k, "scope_aware") == 0)
                    json_parse_bool(j, &e->params.normalize_graph.scope_aware);
                else if (strcmp(k, "max_iterations") == 0)
                    json_parse_int(j, &e->params.normalize_graph.max_iterations);
                else json_skip_value(j);
                break;
            }
            case CMD_UNIFY: {
                if (strcmp(k, "construction_graph_id") == 0)
                    json_parse_int(j, &e->params.unify.construction_graph_id);
                else if (strcmp(k, "proposition_graph_id") == 0)
                    json_parse_int(j, &e->params.unify.proposition_graph_id);
                else if (strcmp(k, "result") == 0)
                    json_parse_bool(j, &e->params.unify.result);
                else json_skip_value(j);
                break;
            }
            case CMD_SET_NUMERIC_ASSUMPTION: {
                CmdSetNumericAssumptionParams *p = &e->params.set_numeric_assumption;
                if (strcmp(k, "node_id") == 0) json_parse_int(j, &p->node_id);
                else if (strcmp(k, "precision") == 0) json_parse_double(j, &p->precision);
                else if (strcmp(k, "declaration") == 0)
                    json_parse_string(j, p->declaration, sizeof(p->declaration));
                else json_skip_value(j);
                break;
            }
            default:
                json_skip_value(j);
                break;
            }

            if (json_peek(j) == ',') json_next(j);
        }
        json_expect(j, '}'); /* 消费 '}' */
    }
}

CommandLog *command_log_deserialize_json(const char *filepath) {
    if (!filepath) return NULL;

    FILE *fp = fopen(filepath, "rb");
    if (!fp) return NULL;

    /* 读取整个文件 */
    fseek(fp, 0, SEEK_END);
    long flen = ftell(fp);
    if (flen <= 0) { fclose(fp); return NULL; }
    fseek(fp, 0, SEEK_SET);

    char *buf = (char *)lv_malloc((size_t)(flen + 1));
    if (!buf) { fclose(fp); return NULL; }
    size_t nread = fread(buf, 1, (size_t)flen, fp);
    fclose(fp);
    buf[nread] = '\0';

    JsonCtx j;
    j.buf = buf;
    j.pos = 0;
    j.len = nread;

    CommandLog *log = command_log_create(1024);
    if (!log) { lv_free((void **)&buf); return NULL; }

    /* 解析顶层对象 */
    if (!json_expect(&j, '{')) { lv_free((void **)&buf); return log; }

    while (json_peek(&j) != '}') {
        char key[64];
        if (!json_parse_string(&j, key, sizeof(key))) break;
        if (!json_expect(&j, ':')) break;

        if (strcmp(key, "version") == 0) {
            int ver; json_parse_int(&j, &ver);
        } else if (strcmp(key, "entries") == 0) {
            /* 解析 entries 数组 */
            if (!json_expect(&j, '[')) break;

            while (json_peek(&j) != ']') {
                if (!json_expect(&j, '{')) break;

                CommandEntry *e = (CommandEntry *)lv_calloc(1, sizeof(CommandEntry));
                if (!e) break;

                while (json_peek(&j) != '}') {
                    char k[64];
                    if (!json_parse_string(&j, k, sizeof(k))) break;
                    if (!json_expect(&j, ':')) break;

                    if (strcmp(k, "type") == 0) {
                        char t[32];
                        if (json_parse_string(&j, t, sizeof(t))) {
                            for (int ti = 0; ti < CMD_COUNT; ti++) {
                                if (strcmp(t, g_command_type_names[ti]) == 0) {
                                    e->type = (CommandType)ti;
                                    break;
                                }
                            }
                        }
                    } else if (strcmp(k, "seq") == 0) {
                        int64_t seq_val;
                        if (json_parse_int64(&j, &seq_val)) e->seq = seq_val;
                    } else if (strcmp(k, "timestamp_ms") == 0) {
                        int64_t ts;
                        if (json_parse_int64(&j, &ts)) e->timestamp_ms = ts;
                    } else if (strcmp(k, "params") == 0) {
                        json_parse_params(&j, e);
                    } else {
                        json_skip_value(&j);
                    }

                    if (json_peek(&j) == ',') json_next(&j);
                }
                json_expect(&j, '}'); /* 消费 '}' */

                /* 用序列号方式恢复 next_seq */
                if (!command_log_append(log, e)) {
                    command_entry_destroy(e);
                }

                if (json_peek(&j) == ',') json_next(&j);
            }
            json_expect(&j, ']'); /* 消费 ']' */
        } else {
            json_skip_value(&j);
        }

        if (json_peek(&j) == ',') json_next(&j);
    }

    lv_free((void **)&buf);
    return log;
}
