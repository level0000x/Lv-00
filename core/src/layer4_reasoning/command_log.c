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

#include "lv/lv_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stddef.h>


#include "lv/constraint_graph.h"
#include "lv/engine.h"
#include "lv/lv.h"
#include "lv/lv_json.h"
#include "lv/lv_xmacro.h"
#include "lv/lv_internal.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_utils.h"

/* ════════════════════════════════════════════════════════════════
 *  内部数据结构
 * ════════════════════════════════════════════════════════════════ */

struct CommandLog {
    lvDArray entries; /**< 条目动态数组（元素类型：CommandEntry*） */
    int64_t next_seq; /**< 下一个可用序列号 */
};

/* ════════════════════════════════════════════════════════════════
 *  命令类型名称表
 * ════════════════════════════════════════════════════════════════ */

/* 由 LV_COMMAND_TYPE_X 生成（[枚举] = "序列化名" 指定初始化器，与枚举值一一对应） */
const char *g_command_type_names[CMD_COUNT] = {
    lv_XMACRO_TO_NAME_ARRAY(LV_COMMAND_TYPE_X)
};

/* ════════════════════════════════════════════════════════════════
 *  辅助函数
 * ════════════════════════════════════════════════════════════════ */

/* ── command_entry_cleanup 查找表 ── */
typedef void (*CleanupHandler)(CommandEntry *entry);

static void cleanup_add_node(CommandEntry *entry) {
    lv_free((void **) &entry->params.add_node.coords_num);
    lv_free((void **) &entry->params.add_node.coords_den);
}

static void cleanup_pack_function(CommandEntry *entry) {
    lv_free((void **) &entry->params.pack_function.internal_node_ids);
    lv_free((void **) &entry->params.pack_function.input_port_ids);
    lv_free((void **) &entry->params.pack_function.output_port_ids);
}

static const CleanupHandler cleanup_table[CMD_COUNT] = {
    [CMD_ADD_NODE]      = cleanup_add_node,
    [CMD_PACK_FUNCTION] = cleanup_pack_function,
};

/** 销毁命令条目的内部动态内存（不释放 entry 本身）*/
static void command_entry_cleanup(CommandEntry *entry) {
    if (!entry)
        return;
    /* 统一分发：边界/NULL 检查由 LV_DISPATCH_VOID 完成 */
    LV_DISPATCH_VOID(cleanup_table, entry->type, entry);
    command_entry_destroy(entry->inverse);
    entry->inverse = NULL;
}

/* ════════════════════════════════════════════════════════════════
 *  日志生命周期
 * ════════════════════════════════════════════════════════════════ */

CommandLog *command_log_create(int initial_capacity) {
    CommandLog *log = (CommandLog *) lv_calloc(1, sizeof(CommandLog));
    if (!log) {
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "command_log_create: lv_calloc failed");
    }

    int cap = (initial_capacity > 0) ? initial_capacity : 1024;
    lv_darray_init(&log->entries, sizeof(CommandEntry *));
    if (!lv_darray_reserve(&log->entries, cap)) {
        lv_free((void **) &log);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "command_log_create: lv_darray_reserve failed");
    }
    log->next_seq = 0;
    return log;
}

void command_log_destroy(CommandLog *log) {
    if (!log)
        return;
    command_log_clear(log);
    lv_darray_free(&log->entries);
    lv_free((void **) &log);
}

/* ════════════════════════════════════════════════════════════════
 *  日志操作
 * ════════════════════════════════════════════════════════════════ */

bool command_log_append(CommandLog *log, CommandEntry *entry) {
    if (!log || !entry)
        return false;

    /* 自动分配序列号和时间戳 */
    entry->seq = log->next_seq++;
    entry->timestamp_ms = (int64_t)lv_get_wallclock_ms();

    /* 追加条目（lv_darray_push 自动扩容） */
    return lv_darray_push(&log->entries, &entry) >= 0;
}

int command_log_count(const CommandLog *log) {
    return log ? log->entries.count : 0;
}

const CommandEntry *command_log_get(const CommandLog *log, int index) {
    if (!log) {
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "command_log_get: log is NULL");
    }
    CommandEntry *const *p = (CommandEntry *const *) lv_darray_get(&log->entries, index);
    return p ? *p : NULL;
}

void command_log_clear(CommandLog *log) {
    if (!log)
        return;
    CommandEntry **arr = (CommandEntry **) log->entries.data;
    for (int i = 0; i < log->entries.count; i++) {
        command_entry_cleanup(arr[i]);
        lv_free((void **) &arr[i]);
    }
    lv_darray_clear(&log->entries);
    log->next_seq = 0;
}

int64_t command_log_current_seq(const CommandLog *log) {
    return log ? log->next_seq : 0;
}

/* ════════════════════════════════════════════════════════════════
 *  便利构造函数
 * ════════════════════════════════════════════════════════════════ */

CommandEntry *command_entry_create_add_node(int geom_type, int node_id, int coord_count, const double *nums,
                                            const uint64_t *dens) {
    CommandEntry *entry = (CommandEntry *) lv_calloc(1, sizeof(CommandEntry));
    if (!entry) { lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "command_entry_create_add_node: calloc failed"); }
    entry->type = CMD_ADD_NODE;
    entry->params.add_node.geom_type = geom_type;
    entry->params.add_node.node_id = node_id;
    entry->params.add_node.coord_count = coord_count;
    entry->params.add_node.namespace_depth = 0;
    entry->params.add_node.parent_block_id = -1;
    entry->params.add_node.is_formal_param = false;

    if (coord_count > 0 && nums && dens) {
        entry->params.add_node.coords_num = (double *) lv_malloc((size_t) coord_count * sizeof(double));
        entry->params.add_node.coords_den = (uint64_t *) lv_malloc((size_t) coord_count * sizeof(uint64_t));
        if (!entry->params.add_node.coords_num || !entry->params.add_node.coords_den) {
            lv_free((void **) &entry->params.add_node.coords_num);
            lv_free((void **) &entry->params.add_node.coords_den);
            lv_free((void **) &entry);
            lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "command_entry_create_add_node: coord memory allocation failed");
        }
        memcpy(entry->params.add_node.coords_num, nums, (size_t) coord_count * sizeof(double));
        memcpy(entry->params.add_node.coords_den, dens, (size_t) coord_count * sizeof(uint64_t));
    }
    return entry;
}

CommandEntry *command_entry_create_add_constraint(int constr_type, int constr_id, const int *participants,
                                                  int participant_count) {
    CommandEntry *entry = (CommandEntry *) lv_calloc(1, sizeof(CommandEntry));
    if (!entry) {
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "command_entry_create_add_constraint: calloc failed");
    }
    entry->type = CMD_ADD_CONSTRAINT;
    entry->params.add_constraint.constraint_type = constr_type;
    entry->params.add_constraint.constraint_id = constr_id;
    entry->params.add_constraint.participant_count = (participant_count > 8) ? 8 : participant_count;
    for (int i = 0; i < entry->params.add_constraint.participant_count; i++) {
        entry->params.add_constraint.participant_ids[i] = participants[i];
    }
    return entry;
}

CommandEntry *command_entry_create_remove_node(int node_id) {
    CommandEntry *entry = (CommandEntry *) lv_calloc(1, sizeof(CommandEntry));
    if (!entry) { lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "command_entry_create_remove_node: calloc failed"); }
    entry->type = CMD_REMOVE_NODE;
    entry->params.remove_node.node_id = node_id;
    return entry;
}

CommandEntry *command_entry_create_remove_constraint(int constraint_idx) {
    CommandEntry *entry = (CommandEntry *) lv_calloc(1, sizeof(CommandEntry));
    if (!entry) {
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "command_entry_create_remove_constraint: calloc failed");
    }
    entry->type = CMD_REMOVE_CONSTRAINT;
    entry->params.remove_constraint.constraint_index = constraint_idx;
    return entry;
}

CommandEntry *command_entry_create_pack_function(int internal_count, const int *internal_ids, int input_count,
                                                 const int *input_ports, int output_count, const int *output_ports) {
    CommandEntry *entry = (CommandEntry *) lv_calloc(1, sizeof(CommandEntry));
    if (!entry) {
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "command_entry_create_pack_function: calloc failed");
    }
    entry->type = CMD_PACK_FUNCTION;
    entry->params.pack_function.internal_count = internal_count;
    entry->params.pack_function.input_count = input_count;
    entry->params.pack_function.output_count = output_count;
    entry->params.pack_function.result_func_id = -1;

    if (internal_count > 0 && internal_ids) {
        entry->params.pack_function.internal_node_ids = (int *) lv_malloc((size_t) internal_count * sizeof(int));
        if (entry->params.pack_function.internal_node_ids)
            memcpy(entry->params.pack_function.internal_node_ids, internal_ids, (size_t) internal_count * sizeof(int));
    }
    if (input_count > 0 && input_ports) {
        entry->params.pack_function.input_port_ids = (int *) lv_malloc((size_t) input_count * sizeof(int));
        if (entry->params.pack_function.input_port_ids)
            memcpy(entry->params.pack_function.input_port_ids, input_ports, (size_t) input_count * sizeof(int));
    }
    if (output_count > 0 && output_ports) {
        entry->params.pack_function.output_port_ids = (int *) lv_malloc((size_t) output_count * sizeof(int));
        if (entry->params.pack_function.output_port_ids)
            memcpy(entry->params.pack_function.output_port_ids, output_ports, (size_t) output_count * sizeof(int));
    }
    return entry;
}

CommandEntry *command_entry_create_normalize_graph(bool scope_aware, int max_iterations) {
    CommandEntry *entry = (CommandEntry *) lv_calloc(1, sizeof(CommandEntry));
    if (!entry) {
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "command_entry_create_normalize_graph: calloc failed");
    }
    entry->type = CMD_NORMALIZE_GRAPH;
    entry->params.normalize_graph.scope_aware = scope_aware;
    entry->params.normalize_graph.max_iterations = max_iterations;
    return entry;
}

CommandEntry *command_entry_create_unify(int construction_graph_id, int proposition_graph_id) {
    CommandEntry *entry = (CommandEntry *) lv_calloc(1, sizeof(CommandEntry));
    if (!entry) {
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "command_entry_create_unify: calloc failed");
    }
    entry->type = CMD_UNIFY;
    entry->params.unify.construction_graph_id = construction_graph_id;
    entry->params.unify.proposition_graph_id = proposition_graph_id;
    entry->params.unify.result = false;
    return entry;
}

CommandEntry *command_entry_create_set_numeric_assumption(int node_id, double precision, const char *declaration) {
    CommandEntry *entry = (CommandEntry *) lv_calloc(1, sizeof(CommandEntry));
    if (!entry) {
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "command_entry_create_set_numeric_assumption: calloc failed");
    }
    entry->type = CMD_SET_NUMERIC_ASSUMPTION;
    entry->params.set_numeric_assumption.node_id = node_id;
    entry->params.set_numeric_assumption.precision = precision;
    if (declaration) {
        lv_strlcpy(entry->params.set_numeric_assumption.declaration, declaration,
                   sizeof(entry->params.set_numeric_assumption.declaration));
    }
    return entry;
}

void command_entry_destroy(CommandEntry *entry) {
    if (!entry)
        return;
    command_entry_cleanup(entry);
    lv_free((void **) &entry);
}

/* ════════════════════════════════════════════════════════════════
 *  命令执行
 * ════════════════════════════════════════════════════════════════ */

/* ── execute_command 查找表 ── */
typedef bool (*ExecuteHandler)(CommandEntry *entry, lvEngine *engine);

static bool exec_add_node(CommandEntry *entry, lvEngine *engine) {
    CmdAddNodeParams *p = &entry->params.add_node;
    if (p->coord_count >= 2 && p->coords_num && p->coords_den) {
        SymbolicCoord **coords = (SymbolicCoord **) lv_malloc((size_t) p->coord_count * sizeof(SymbolicCoord *));
        if (!coords) return false;
        for (int i = 0; i < p->coord_count; i++) {
            coords[i] = symbolic_coord_create_rational((int64_t) p->coords_num[i], p->coords_den[i]);
            if (!coords[i]) {
                for (int j = 0; j < i; j++) symbolic_coord_destroy(coords[j]);
                lv_free((void **) &coords);
                return false;
            }
        }
        int id = lv_add_point(engine, p->coords_num[0], p->coords_den[0], p->coords_num[1], p->coords_den[1]);
        for (int i = 0; i < p->coord_count; i++) symbolic_coord_destroy(coords[i]);
        lv_free((void **) &coords);
        return id >= 0;
    }
    return false;
}

static bool exec_add_constraint(CommandEntry *entry, lvEngine *engine) {
    CmdAddConstraintParams *p = &entry->params.add_constraint;
    if (p->participant_count >= 2) {
        for (int i = 1; i < p->participant_count; i++)
            lv_add_constraint_incidence(engine, p->participant_ids[i], p->participant_ids[0]);
        return true;
    }
    return false;
}

static bool exec_remove_node(CommandEntry *entry, lvEngine *engine) {
    RemoveNodeResult result = graph_remove_node(engine->main_graph, entry->params.remove_node.node_id);
    return result == REMOVE_NODE_OK;
}

static bool exec_remove_constraint(CommandEntry *entry, lvEngine *engine) {
    RemoveConstraintResult result = graph_remove_constraint(engine->main_graph, entry->params.remove_constraint.constraint_index);
    return result == REMOVE_CONSTRAINT_OK;
}

static bool exec_pack_function(CommandEntry *entry, lvEngine *engine) {
    CmdPackFunctionParams *p = &entry->params.pack_function;
    int func_id = -1;
    bool ok = engine_pack_function(engine, p->internal_node_ids, p->internal_count, p->input_port_ids,
                                   p->input_count, p->output_port_ids, p->output_count, &func_id);
    p->result_func_id = func_id;
    return ok;
}

static bool exec_normalize_graph(CommandEntry *entry, lvEngine *engine) {
    engine_rewrite_and_solve(engine, entry->params.normalize_graph.max_iterations, 0);
    return true;
}

static bool exec_unify(CommandEntry *entry, lvEngine *engine) {
    CmdUnifyParams *p = &entry->params.unify;
    UnifyStatus status = engine_unify(engine, engine->main_graph, engine->main_graph);
    p->result = (status == UNIFY_STATUS_OK);
    return p->result;
}

static bool exec_set_numeric_assumption(CommandEntry *entry, lvEngine *engine) {
    CmdSetNumericAssumptionParams *p = &entry->params.set_numeric_assumption;
    return lv_set_numeric_assumption(engine, p->node_id, p->precision, p->declaration) == 0;
}

static bool exec_default(CommandEntry *entry, lvEngine *engine) { (void)entry; (void)engine; return false; }

static const ExecuteHandler execute_table[CMD_COUNT] = {
    [CMD_ADD_NODE]             = exec_add_node,
    [CMD_ADD_CONSTRAINT]       = exec_add_constraint,
    [CMD_REMOVE_NODE]          = exec_remove_node,
    [CMD_REMOVE_CONSTRAINT]    = exec_remove_constraint,
    [CMD_PACK_FUNCTION]        = exec_pack_function,
    [CMD_NORMALIZE_GRAPH]      = exec_normalize_graph,
    [CMD_UNIFY]                = exec_unify,
    [CMD_SET_NUMERIC_ASSUMPTION] = exec_set_numeric_assumption,
};

/**
 * @brief 执行单条命令（不记录日志）
 */
static bool execute_command(CommandEntry *entry, lvEngine *engine) {
    if (!entry || !engine)
        return false;
    /* 统一分发：边界/NULL 槽回退 false（旧行为一致） */
    return LV_DISPATCH(execute_table, entry->type, false, entry, engine);
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
    if (!log || !engine)
        return false;

    CommandEntry **log_entries = (CommandEntry **) log->entries.data;
    for (int i = 0; i < log->entries.count; i++) {
        CommandEntry *entry = log_entries[i];
        if (entry->seq <= from_seq)
            continue;
        if (!execute_command(entry, engine)) {
            LOG_ERROR("command_log", "回放失败: seq=%lld type=%s", (long long) entry->seq,
                      g_command_type_names[entry->type]);
            return false;
        }
    }
    return true;
}

/* ════════════════════════════════════════════════════════════════
 *  JSON 序列化 —— 输出完整命令参数
 * ════════════════════════════════════════════════════════════════ */

/** JSON 数组元素写出器：将单个元素格式化为 JSON 数值并追加 */
typedef void (*JsonArrayElemWriter)(lvJsonBuf *buf, const void *elem);

static void json_buf_elem_int(lvJsonBuf *buf, const void *elem) {
    lv_json_buf_append_fmt(buf, "%d", *(const int *) elem);
}

static void json_buf_elem_double(lvJsonBuf *buf, const void *elem) {
    lv_json_buf_append_fmt(buf, "%.17g", *(const double *) elem);
}

static void json_buf_elem_uint64(lvJsonBuf *buf, const void *elem) {
    lv_json_buf_append_fmt(buf, "%llu", (unsigned long long) *(const uint64_t *) elem);
}

/** 泛型 JSON 数组写出器（元素大小 + 元素格式化函数指针），pretty 模式下自动多行缩进 */
static void json_buf_array(lvJsonBuf *buf, const void *arr, int count, size_t elem_size,
                           JsonArrayElemWriter writer) {
    lv_json_buf_begin_array(buf);
    for (int i = 0; i < count; i++)
        writer(buf, (const char *) arr + (size_t) i * elem_size);
    lv_json_buf_end_array(buf);
}

/* ── json_buf_write_params 查找表 ── */
typedef void (*JsonWriteHandler)(lvJsonBuf *buf, const CommandEntry *e);

static void json_write_add_node(lvJsonBuf *buf, const CommandEntry *e) {
    const CmdAddNodeParams *p = &e->params.add_node;
    lv_json_buf_append_key(buf, "geom_type");
    lv_json_buf_append_int(buf, p->geom_type);
    lv_json_buf_append_key(buf, "node_id");
    lv_json_buf_append_int(buf, p->node_id);
    lv_json_buf_append_key(buf, "coord_count");
    lv_json_buf_append_int(buf, p->coord_count);
    lv_json_buf_append_key(buf, "namespace_depth");
    lv_json_buf_append_int(buf, p->namespace_depth);
    lv_json_buf_append_key(buf, "parent_block_id");
    lv_json_buf_append_int(buf, p->parent_block_id);
    lv_json_buf_append_key(buf, "is_formal_param");
    lv_json_buf_append_bool(buf, p->is_formal_param);
    lv_json_buf_append_key(buf, "coords_num");
    if (p->coords_num && p->coord_count > 0)
        json_buf_array(buf, p->coords_num, p->coord_count, sizeof(double), json_buf_elem_double);
    else
        lv_json_buf_append_null(buf);
    lv_json_buf_append_key(buf, "coords_den");
    if (p->coords_den && p->coord_count > 0)
        json_buf_array(buf, p->coords_den, p->coord_count, sizeof(uint64_t), json_buf_elem_uint64);
    else
        lv_json_buf_append_null(buf);
}

static void json_write_add_constraint(lvJsonBuf *buf, const CommandEntry *e) {
    const CmdAddConstraintParams *p = &e->params.add_constraint;
    lv_json_buf_append_key(buf, "constraint_type");
    lv_json_buf_append_int(buf, p->constraint_type);
    lv_json_buf_append_key(buf, "constraint_id");
    lv_json_buf_append_int(buf, p->constraint_id);
    lv_json_buf_append_key(buf, "participant_count");
    lv_json_buf_append_int(buf, p->participant_count);
    lv_json_buf_append_key(buf, "participant_ids");
    json_buf_array(buf, p->participant_ids, p->participant_count, sizeof(int), json_buf_elem_int);
}

static void json_write_remove_node(lvJsonBuf *buf, const CommandEntry *e) {
    lv_json_buf_append_key(buf, "node_id");
    lv_json_buf_append_int(buf, e->params.remove_node.node_id);
}

static void json_write_remove_constraint(lvJsonBuf *buf, const CommandEntry *e) {
    lv_json_buf_append_key(buf, "constraint_index");
    lv_json_buf_append_int(buf, e->params.remove_constraint.constraint_index);
}

static void json_write_pack_function(lvJsonBuf *buf, const CommandEntry *e) {
    const CmdPackFunctionParams *p = &e->params.pack_function;
    lv_json_buf_append_key(buf, "internal_count");
    lv_json_buf_append_int(buf, p->internal_count);
    lv_json_buf_append_key(buf, "input_count");
    lv_json_buf_append_int(buf, p->input_count);
    lv_json_buf_append_key(buf, "output_count");
    lv_json_buf_append_int(buf, p->output_count);
    lv_json_buf_append_key(buf, "result_func_id");
    lv_json_buf_append_int(buf, p->result_func_id);
    lv_json_buf_append_key(buf, "internal_node_ids");
    if (p->internal_node_ids && p->internal_count > 0)
        json_buf_array(buf, p->internal_node_ids, p->internal_count, sizeof(int), json_buf_elem_int);
    else
        lv_json_buf_append_null(buf);
    lv_json_buf_append_key(buf, "input_port_ids");
    if (p->input_port_ids && p->input_count > 0)
        json_buf_array(buf, p->input_port_ids, p->input_count, sizeof(int), json_buf_elem_int);
    else
        lv_json_buf_append_null(buf);
    lv_json_buf_append_key(buf, "output_port_ids");
    if (p->output_port_ids && p->output_count > 0)
        json_buf_array(buf, p->output_port_ids, p->output_count, sizeof(int), json_buf_elem_int);
    else
        lv_json_buf_append_null(buf);
}

static void json_write_normalize_graph(lvJsonBuf *buf, const CommandEntry *e) {
    lv_json_buf_append_key(buf, "scope_aware");
    lv_json_buf_append_bool(buf, e->params.normalize_graph.scope_aware);
    lv_json_buf_append_key(buf, "max_iterations");
    lv_json_buf_append_int(buf, e->params.normalize_graph.max_iterations);
}

static void json_write_unify(lvJsonBuf *buf, const CommandEntry *e) {
    lv_json_buf_append_key(buf, "construction_graph_id");
    lv_json_buf_append_int(buf, e->params.unify.construction_graph_id);
    lv_json_buf_append_key(buf, "proposition_graph_id");
    lv_json_buf_append_int(buf, e->params.unify.proposition_graph_id);
    lv_json_buf_append_key(buf, "result");
    lv_json_buf_append_bool(buf, e->params.unify.result);
}

static void json_write_set_numeric_assumption(lvJsonBuf *buf, const CommandEntry *e) {
    const CmdSetNumericAssumptionParams *p = &e->params.set_numeric_assumption;
    lv_json_buf_append_key(buf, "node_id");
    lv_json_buf_append_int(buf, p->node_id);
    lv_json_buf_append_key(buf, "precision");
    lv_json_buf_append_fmt(buf, "%.17g", p->precision);
    lv_json_buf_append_key(buf, "declaration");
    lv_json_buf_append_string(buf, p->declaration);
}

static const JsonWriteHandler json_write_table[CMD_COUNT] = {
    [CMD_ADD_NODE]             = json_write_add_node,
    [CMD_ADD_CONSTRAINT]       = json_write_add_constraint,
    [CMD_REMOVE_NODE]          = json_write_remove_node,
    [CMD_REMOVE_CONSTRAINT]    = json_write_remove_constraint,
    [CMD_PACK_FUNCTION]        = json_write_pack_function,
    [CMD_NORMALIZE_GRAPH]      = json_write_normalize_graph,
    [CMD_UNIFY]                = json_write_unify,
    [CMD_SET_NUMERIC_ASSUMPTION] = json_write_set_numeric_assumption,
};

/** 输出命令参数的 JSON 对象体（不含外层花括号）（lvJsonBuf 版本，调用方已 begin_object/end_object） */
static void json_buf_write_params(lvJsonBuf *buf, const CommandEntry *e) {
    JsonWriteHandler h = json_write_table[e->type];
    if (h)
        h(buf, e);
    else {
        lv_json_buf_append_key(buf, "type");
        lv_json_buf_append_string(buf, "unknown");
    }
}

bool command_log_serialize_json(const CommandLog *log, const char *filepath) {
    if (!log || !filepath)
        return false;

    lvJsonBuf buf;
    if (!lv_json_buf_init(&buf, 4096))
        return false;
    lv_json_buf_set_pretty(&buf, true);
    lv_json_buf_set_key_space(&buf, true);

    lv_json_buf_begin_object(&buf);
    lv_json_buf_append_key(&buf, "version");
    lv_json_buf_append_int(&buf, 1);
    lv_json_buf_append_key(&buf, "entries");
    lv_json_buf_begin_array(&buf);

    CommandEntry **log_entries = (CommandEntry **) log->entries.data;
    for (int i = 0; i < log->entries.count; i++) {
        const CommandEntry *e = log_entries[i];
        lv_json_buf_begin_object(&buf);
        lv_json_buf_append_key(&buf, "type");
        lv_json_buf_append_string(&buf, g_command_type_names[e->type]);
        lv_json_buf_append_key(&buf, "seq");
        lv_json_buf_append_int(&buf, (long long) e->seq);
        lv_json_buf_append_key(&buf, "timestamp_ms");
        lv_json_buf_append_int(&buf, (long long) e->timestamp_ms);
        lv_json_buf_append_key(&buf, "params");
        lv_json_buf_begin_object(&buf);
        json_buf_write_params(&buf, e);
        lv_json_buf_end_object(&buf);
        lv_json_buf_end_object(&buf);
    }

    lv_json_buf_end_array(&buf);
    lv_json_buf_end_object(&buf);

    /* 写入文件 */
    FILE *fp = lv_file_open(filepath, "w");
    if (!fp) {
        lv_json_buf_free(&buf);
        return false;
    }
    fputs(buf.buffer, fp);
    lv_file_close(fp);

    lv_json_buf_free(&buf);
    return true;
}

/* ════════════════════════════════════════════════════════════════
 *  JSON 反序列化 —— 最小 JSON 解析器
 * ════════════════════════════════════════════════════════════════ */

/* ════════════════════════════════════════════════════════════════
 *  JSON 反序列化 —— 基于公共 lvJsonParser（lv_json.h）
 * ════════════════════════════════════════════════════════════════
 *
 * 基础原语（skip_ws/peek/next/expect/int/int64/uint64/double/bool/skip_value）
 * 统一委托公共 lvJsonParser，避免与 lv_json.c 重复；
 * 此处仅保留 command_log 特有的值形状解析：
 * 固定缓冲字符串、动态 int/double/uint64 数组、null 判定。
 */

/** 解析 JSON 字符串字面量，解码写入 dst 缓冲区（公共反转义 API 统一 \uXXXX 语义） */
static bool json_parse_string(lvJsonParser *p, char *dst, size_t dst_size) {
    if (lv_json_next(p) != '"')
        return false;

    size_t start = p->pos;
    size_t raw_len = 0;
    bool closed = false;

    /* 第一遍：定位结束引号，统计原始字节数（转义序列按原始长度计入） */
    while (p->pos < p->size) {
        char c = p->data[p->pos++];
        if (c == '"') {
            closed = true;
            break;
        }
        if (c == '\\' && p->pos < p->size) {
            p->pos++; /* 跳过转义字符（含 \uXXXX 由公共反转义函数统一解码） */
            raw_len++;
        }
        raw_len++;
    }

    /* 第二遍：公共反转义 API 解码（含 \uXXXX → UTF-8；缓冲区不足时安全截断） */
    lv_str_json_unescape(p->data + start, raw_len, dst, dst_size);

    if (!closed)
        return false; /* 未闭合的字符串（已尽力解码已读取内容） */
    return true;
}

/** 解析 JSON null */
static bool json_parse_null(lvJsonParser *p) {
    lv_json_skip_ws(p);
    if (p->pos + 4 <= p->size && memcmp(p->data + p->pos, "null", 4) == 0) {
        p->pos += 4;
        return true;
    }
    return false;
}

/** JSON 数组元素解析器：从解析器读取单个元素写入 dst */
typedef bool (*JsonArrayElemParser)(lvJsonParser *p, void *dst);

static bool json_parse_elem_int(lvJsonParser *p, void *dst) { return lv_json_parse_int(p, (int *) dst); }
static bool json_parse_elem_double(lvJsonParser *p, void *dst) { return lv_json_parse_double(p, (double *) dst); }
static bool json_parse_elem_uint64(lvJsonParser *p, void *dst) { return lv_json_parse_uint64(p, (uint64_t *) dst); }

/**
 * 泛型 JSON 数组解析器 [...]
 * 动态分配，返回元素个数；空数组返回 0 且 *out 为 NULL。
 * 解析失败返回 0 并释放已分配内存。语义与旧三函数逐位一致。
 */
static int json_parse_array(lvJsonParser *p, void **out, size_t elem_size, JsonArrayElemParser parse_elem) {
    *out = NULL;
    if (lv_json_next(p) != '[')
        return 0;
    if (lv_json_peek(p) == ']') {
        lv_json_next(p);
        return 0;
    }
    int cap = 16, count = 0;
    void *arr = lv_malloc((size_t) cap * elem_size);
    if (!arr)
        return 0;
    while (1) {
        if (!lv_ensure_capacity(&arr, count, &cap, elem_size, 0)) {
            lv_free(&arr);
            return 0;
        }
        if (!parse_elem(p, (char *) arr + (size_t) count * elem_size)) {
            lv_free(&arr);
            return 0;
        }
        count++;
        if (lv_json_peek(p) == ']')
            break;
        if (!lv_json_expect(p, ',')) {
            lv_free(&arr);
            return 0;
        }
    }
    lv_json_next(p); /* 消费 ']' */
    *out = arr;
    return count;
}

/** 类型化薄包装：int 数组 */
static int json_parse_int_array(lvJsonParser *p, int **out) {
    return json_parse_array(p, (void **) out, sizeof(int), json_parse_elem_int);
}

/** 类型化薄包装：double 数组 */
static int json_parse_double_array(lvJsonParser *p, double **out) {
    return json_parse_array(p, (void **) out, sizeof(double), json_parse_elem_double);
}

/** 类型化薄包装：uint64_t 数组 */
static int json_parse_uint64_array(lvJsonParser *p, uint64_t **out) {
    return json_parse_array(p, (void **) out, sizeof(uint64_t), json_parse_elem_uint64);
}

/** 跳过 JSON 值：委托公共 lv_json_skip_value（递归跳过对象/数组/字符串/数字/布尔/null） */

/* ── json_parse_params 查找表 ── */
typedef void (*JsonParseHandler)(lvJsonParser *p, CommandEntry *e, const char *key);

/* 通用字段解析器：解析 JSON 值并写入目标内存（字段名→handler 表用） */
typedef void (*JsonFieldHandler)(lvJsonParser *p, void *dst);

static void json_field_int(lvJsonParser *p, void *dst) { lv_json_parse_int(p, (int *) dst); }
static void json_field_bool(lvJsonParser *p, void *dst) { lv_json_parse_bool(p, (bool *) dst); }
static void json_field_double(lvJsonParser *p, void *dst) { lv_json_parse_double(p, (double *) dst); }
static void json_field_string_decl(lvJsonParser *p, void *dst) { json_parse_string(p, (char *) dst, 256); }

/** @brief 可空 double 数组（null 或数组，数组分配并接管） */
static void json_field_nullable_double_array(lvJsonParser *p, void *dst) {
    double **out = (double **) dst;
    if (lv_json_peek(p) == 'n') json_parse_null(p);
    else { lv_free((void **) out); json_parse_double_array(p, out); }
}

/** @brief 可空 uint64_t 数组（null 或数组，数组分配并接管） */
static void json_field_nullable_u64_array(lvJsonParser *p, void *dst) {
    uint64_t **out = (uint64_t **) dst;
    if (lv_json_peek(p) == 'n') json_parse_null(p);
    else { lv_free((void **) out); json_parse_uint64_array(p, out); }
}

/** @brief 可空 int 数组（null 或数组，数组分配并接管） */
static void json_field_nullable_int_array(lvJsonParser *p, void *dst) {
    int **out = (int **) dst;
    if (lv_json_peek(p) == 'n') json_parse_null(p);
    else { int *arr = NULL; (void) json_parse_int_array(p, &arr); lv_free((void **) out); *out = arr; }
}

/** @brief 固定长度 int 数组（最多复制 8 个，如 participant_ids） */
static void json_field_participant_ids(lvJsonParser *p, void *dst) {
    int *arr = NULL;
    int cnt = json_parse_int_array(p, &arr);
    if (arr) {
        int n = (cnt > 8) ? 8 : cnt;
        memcpy(dst, arr, (size_t) n * sizeof(int));
        lv_free((void **) &arr);
    }
}

/** @brief 字段分发表条目：键名 + 目标字段偏移 + 解析器 */
typedef struct {
    const char *key;
    size_t offset;          /* 相对参数结构体基址的偏移 */
    JsonFieldHandler handler;
} JsonFieldEntry;

/** @brief 在字段表中查找键名对应的条目 */
static const JsonFieldEntry *json_field_lookup(const JsonFieldEntry *table, size_t count, const char *key) {
    for (size_t i = 0; i < count; i++) {
        if (lv_str_eq(table[i].key, key))
            return &table[i];
    }
    return NULL;
}

/** @brief ADD_NODE 参数字段表（替代 8 分支 strcmp 链） */
static const JsonFieldEntry kAddNodeFields[] = {
    {"geom_type", offsetof(CmdAddNodeParams, geom_type), json_field_int},
    {"node_id", offsetof(CmdAddNodeParams, node_id), json_field_int},
    {"coord_count", offsetof(CmdAddNodeParams, coord_count), json_field_int},
    {"namespace_depth", offsetof(CmdAddNodeParams, namespace_depth), json_field_int},
    {"parent_block_id", offsetof(CmdAddNodeParams, parent_block_id), json_field_int},
    {"is_formal_param", offsetof(CmdAddNodeParams, is_formal_param), json_field_bool},
    {"coords_num", offsetof(CmdAddNodeParams, coords_num), json_field_nullable_double_array},
    {"coords_den", offsetof(CmdAddNodeParams, coords_den), json_field_nullable_u64_array},
};

static void json_parse_add_node(lvJsonParser *j, CommandEntry *e, const char *key) {
    CmdAddNodeParams *p = &e->params.add_node;
    const JsonFieldEntry *entry = json_field_lookup(kAddNodeFields, lv_ARRAY_SIZE(kAddNodeFields), key);
    if (entry) {
        entry->handler(j, (char *) p + entry->offset);
        return;
    }
    lv_json_skip_value(j);
}

/** @brief ADD_CONSTRAINT 参数字段表（替代 4 分支 strcmp 链） */
static const JsonFieldEntry kAddConstraintFields[] = {
    {"constraint_type", offsetof(CmdAddConstraintParams, constraint_type), json_field_int},
    {"constraint_id", offsetof(CmdAddConstraintParams, constraint_id), json_field_int},
    {"participant_count", offsetof(CmdAddConstraintParams, participant_count), json_field_int},
    {"participant_ids", offsetof(CmdAddConstraintParams, participant_ids), json_field_participant_ids},
};

static void json_parse_add_constraint(lvJsonParser *j, CommandEntry *e, const char *key) {
    CmdAddConstraintParams *p = &e->params.add_constraint;
    const JsonFieldEntry *entry = json_field_lookup(kAddConstraintFields, lv_ARRAY_SIZE(kAddConstraintFields), key);
    if (entry) {
        entry->handler(j, (char *) p + entry->offset);
        return;
    }
    lv_json_skip_value(j);
}

/** @brief REMOVE_NODE 参数字段表 */
static const JsonFieldEntry kRemoveNodeFields[] = {
    {"node_id", offsetof(CmdRemoveNodeParams, node_id), json_field_int},
};

static void json_parse_remove_node(lvJsonParser *j, CommandEntry *e, const char *key) {
    const JsonFieldEntry *entry = json_field_lookup(kRemoveNodeFields, lv_ARRAY_SIZE(kRemoveNodeFields), key);
    if (entry) {
        entry->handler(j, (char *) &e->params.remove_node + entry->offset);
        return;
    }
    lv_json_skip_value(j);
}

/** @brief REMOVE_CONSTRAINT 参数字段表 */
static const JsonFieldEntry kRemoveConstraintFields[] = {
    {"constraint_index", offsetof(CmdRemoveConstraintParams, constraint_index), json_field_int},
};

static void json_parse_remove_constraint(lvJsonParser *j, CommandEntry *e, const char *key) {
    const JsonFieldEntry *entry =
        json_field_lookup(kRemoveConstraintFields, lv_ARRAY_SIZE(kRemoveConstraintFields), key);
    if (entry) {
        entry->handler(j, (char *) &e->params.remove_constraint + entry->offset);
        return;
    }
    lv_json_skip_value(j);
}

/** @brief PACK_FUNCTION 参数字段表（替代 7 分支 strcmp 链） */
static const JsonFieldEntry kPackFunctionFields[] = {
    {"internal_count", offsetof(CmdPackFunctionParams, internal_count), json_field_int},
    {"input_count", offsetof(CmdPackFunctionParams, input_count), json_field_int},
    {"output_count", offsetof(CmdPackFunctionParams, output_count), json_field_int},
    {"result_func_id", offsetof(CmdPackFunctionParams, result_func_id), json_field_int},
    {"internal_node_ids", offsetof(CmdPackFunctionParams, internal_node_ids), json_field_nullable_int_array},
    {"input_port_ids", offsetof(CmdPackFunctionParams, input_port_ids), json_field_nullable_int_array},
    {"output_port_ids", offsetof(CmdPackFunctionParams, output_port_ids), json_field_nullable_int_array},
};

static void json_parse_pack_function(lvJsonParser *j, CommandEntry *e, const char *key) {
    CmdPackFunctionParams *p = &e->params.pack_function;
    const JsonFieldEntry *entry =
        json_field_lookup(kPackFunctionFields, lv_ARRAY_SIZE(kPackFunctionFields), key);
    if (entry) {
        entry->handler(j, (char *) p + entry->offset);
        return;
    }
    lv_json_skip_value(j);
}

/** @brief NORMALIZE_GRAPH 参数字段表（替代 2 分支 strcmp 链） */
static const JsonFieldEntry kNormalizeGraphFields[] = {
    {"scope_aware", offsetof(CmdNormalizeGraphParams, scope_aware), json_field_bool},
    {"max_iterations", offsetof(CmdNormalizeGraphParams, max_iterations), json_field_int},
};

static void json_parse_normalize_graph(lvJsonParser *j, CommandEntry *e, const char *key) {
    CmdNormalizeGraphParams *p = &e->params.normalize_graph;
    const JsonFieldEntry *entry =
        json_field_lookup(kNormalizeGraphFields, lv_ARRAY_SIZE(kNormalizeGraphFields), key);
    if (entry) {
        entry->handler(j, (char *) p + entry->offset);
        return;
    }
    lv_json_skip_value(j);
}

/** @brief UNIFY 参数字段表（替代 3 分支 strcmp 链） */
static const JsonFieldEntry kUnifyFields[] = {
    {"construction_graph_id", offsetof(CmdUnifyParams, construction_graph_id), json_field_int},
    {"proposition_graph_id", offsetof(CmdUnifyParams, proposition_graph_id), json_field_int},
    {"result", offsetof(CmdUnifyParams, result), json_field_bool},
};

static void json_parse_unify(lvJsonParser *j, CommandEntry *e, const char *key) {
    CmdUnifyParams *p = &e->params.unify;
    const JsonFieldEntry *entry = json_field_lookup(kUnifyFields, lv_ARRAY_SIZE(kUnifyFields), key);
    if (entry) {
        entry->handler(j, (char *) p + entry->offset);
        return;
    }
    lv_json_skip_value(j);
}

/** @brief SET_NUMERIC_ASSUMPTION 参数字段表（替代 3 分支 strcmp 链） */
static const JsonFieldEntry kSetNumericAssumptionFields[] = {
    {"node_id", offsetof(CmdSetNumericAssumptionParams, node_id), json_field_int},
    {"precision", offsetof(CmdSetNumericAssumptionParams, precision), json_field_double},
    {"declaration", offsetof(CmdSetNumericAssumptionParams, declaration), json_field_string_decl},
};

static void json_parse_set_numeric_assumption(lvJsonParser *j, CommandEntry *e, const char *key) {
    CmdSetNumericAssumptionParams *p = &e->params.set_numeric_assumption;
    const JsonFieldEntry *entry =
        json_field_lookup(kSetNumericAssumptionFields, lv_ARRAY_SIZE(kSetNumericAssumptionFields), key);
    if (entry) {
        entry->handler(j, (char *) p + entry->offset);
        return;
    }
    lv_json_skip_value(j);
}

static void json_parse_default(lvJsonParser *j, CommandEntry *e, const char *key) { (void)e; lv_json_skip_value(j); }

static const JsonParseHandler json_parse_table[CMD_COUNT] = {
    [CMD_ADD_NODE]             = json_parse_add_node,
    [CMD_ADD_CONSTRAINT]       = json_parse_add_constraint,
    [CMD_REMOVE_NODE]          = json_parse_remove_node,
    [CMD_REMOVE_CONSTRAINT]    = json_parse_remove_constraint,
    [CMD_PACK_FUNCTION]        = json_parse_pack_function,
    [CMD_NORMALIZE_GRAPH]      = json_parse_normalize_graph,
    [CMD_UNIFY]                = json_parse_unify,
    [CMD_SET_NUMERIC_ASSUMPTION] = json_parse_set_numeric_assumption,
};

/** 查找并解析命令参数字段 */
static void json_parse_params(lvJsonParser *j, CommandEntry *e) {
    /* 调用方已消费 "params": 并定位到 '{'，直接检查并消费 */
    if (lv_json_peek(j) == '{') {
        lv_json_next(j); /* 消费 '{' */

        char *k = NULL;
        while (lv_json_parse_field(j, &k)) {
            JsonParseHandler h = json_parse_table[e->type];
            if (h) h(j, e, k);
            else lv_json_skip_value(j);

            lv_free((void **) &k);
        }
        lv_json_expect(j, '}'); /* 消费 '}' */
    }
}

/* ── 日志条目字段分发（查找表，替代 4 分支 strcmp 链） ── */

typedef void (*EntryFieldHandler)(lvJsonParser *j, CommandEntry *e);

/* 命令类型字符串→枚举映射表（由 LV_COMMAND_TYPE_X 生成，供 lv_str_to_enum 线性查找） */
static const lvStrToEnumEntry kCommandTypeMap[] = {
    lv_XMACRO_TO_ENUM_TABLE(LV_COMMAND_TYPE_X)
};

static void entry_field_type(lvJsonParser *j, CommandEntry *e) {
    char t[32];
    if (json_parse_string(j, t, sizeof(t))) {
        /* 未匹配时保持 0（CMD_ADD_NODE），与旧实现未匹配不赋值（calloc 后为 0）行为一致 */
        e->type = (CommandType) lv_str_to_enum(kCommandTypeMap, lv_ARRAY_SIZE(kCommandTypeMap), t, 0);
    }
}

static void entry_field_seq(lvJsonParser *j, CommandEntry *e) {
    int64_t seq_val;
    if (lv_json_parse_int64(j, &seq_val))
        e->seq = seq_val;
}

static void entry_field_timestamp_ms(lvJsonParser *j, CommandEntry *e) {
    int64_t ts;
    if (lv_json_parse_int64(j, &ts))
        e->timestamp_ms = ts;
}

static void entry_field_params(lvJsonParser *j, CommandEntry *e) {
    json_parse_params(j, e);
}

/** @brief 日志条目字段名→处理函数 查找表（替代 4 分支 strcmp 链） */
static const struct {
    const char *key;
    EntryFieldHandler handler;
} kEntryFieldTable[] = {
    {"type", entry_field_type},
    {"seq", entry_field_seq},
    {"timestamp_ms", entry_field_timestamp_ms},
    {"params", entry_field_params},
};

CommandLog *command_log_deserialize_json(const char *filepath) {
    if (!filepath) {
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "command_log_deserialize_json: filepath is NULL");
    }

    /* 读取整个文件（lv_file_read_all：失败/空文件返回 NULL，成功时缓冲以 NUL 结尾） */
    size_t flen = 0;
    char *buf = (char *) lv_file_read_all(filepath, &flen);
    if (!buf) {
        lv_RETURN_ERROR_NULL(lv_ERROR_PARSE, "command_log_deserialize_json: cannot read file (not found, empty or read error)");
    }

    lvJsonParser j;
    lv_json_parser_init(&j, buf, flen);

    CommandLog *log = command_log_create(1024);
    if (!log) {
        lv_free((void **) &buf);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "command_log_deserialize_json: command_log_create failed");
    }

    /* 解析顶层对象 */
    if (!lv_json_expect(&j, '{')) {
        lv_free((void **) &buf);
        return log;
    }

    char *key = NULL;
    while (lv_json_parse_field(&j, &key)) {
        if (lv_str_eq(key, "version")) {
            int ver;
            lv_json_parse_int(&j, &ver);
        } else if (lv_str_eq(key, "entries")) {
            /* 解析 entries 数组 */
            if (!lv_json_expect(&j, '[')) {
                lv_free((void **) &key);
                break;
            }

            while (lv_json_peek(&j) != ']') {
                if (!lv_json_expect(&j, '{'))
                    break;

                CommandEntry *e = (CommandEntry *) lv_calloc(1, sizeof(CommandEntry));
                if (!e)
                    break;

                char *k = NULL;
                while (lv_json_parse_field(&j, &k)) {
                    /* 条目字段查表分发（替代 4 分支 strcmp 链） */
                    EntryFieldHandler eh = NULL;
                    for (size_t fi = 0; fi < lv_ARRAY_SIZE(kEntryFieldTable); fi++) {
                        if (lv_str_eq(k, kEntryFieldTable[fi].key)) {
                            eh = kEntryFieldTable[fi].handler;
                            break;
                        }
                    }
                    if (eh)
                        eh(&j, e);
                    else
                        lv_json_skip_value(&j);

                    lv_free((void **) &k);
                }
                lv_json_expect(&j, '}'); /* 消费 '}' */

                /* 用序列号方式恢复 next_seq */
                if (!command_log_append(log, e)) {
                    command_entry_destroy(e);
                }

                if (lv_json_peek(&j) == ',')
                    lv_json_next(&j);
            }
            lv_json_expect(&j, ']'); /* 消费 ']' */
        } else {
            lv_json_skip_value(&j);
        }

        lv_free((void **) &key);
    }

    lv_free((void **) &buf);
    return log;
}

