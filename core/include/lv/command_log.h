/**
 * @file command_log.h
 * @brief 命令日志系统 —— 图操作的原子命令记录与重放
 *
 * @details 实现"唯一真相源"设计：所有图编辑操作（文本/图形模态）
 *          都转为同构的原子命令序列。命令日志是跨模态撤销/重做、
 *          版本控制和协作合并的统一基础。
 *
 *          支持 8 种原子命令（设计文档第 9 节定义）：
 *          - AddNode / AddConstraint / RemoveNode / RemoveConstraint
 *          - PackFunction / NormalizeGraph / Unify / SetNumericAssumption
 *
 * @version 1.1.0
 */

#ifndef lv_COMMAND_LOG_H
#define lv_COMMAND_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lv/lv_xmacro.h"

/* 前向声明 */
struct lvEngine;
typedef struct lvEngine lvEngine;

/* ════════════════════════════════════════════════════════════════
 *  命令类型
 * ════════════════════════════════════════════════════════════════ */

/**
 * @brief 原子命令类型 X 列表（命令族唯一真相源）
 *
 * 二元组 (枚举名, 序列化名称)，覆盖设计文档第 9 节定义的 8 种原子命令。
 * 由本列表生成：CommandType 枚举、g_command_type_names 名称表、
 * 字符串→枚举映射表。新增命令时在此追加一行即可同步上述三处，
 * 其余分发表（cleanup/execute/json_write/json_parse）按指定初始化器
 * 补一行，编译器自动校验缺项。
 *
 * 语义：
 * - CMD_ADD_NODE            添加节点（点/线段/区域/端口等）
 * - CMD_ADD_CONSTRAINT      添加约束
 * - CMD_REMOVE_NODE         移除节点
 * - CMD_REMOVE_CONSTRAINT   移除约束
 * - CMD_PACK_FUNCTION       打包函数块
 * - CMD_NORMALIZE_GRAPH     图规范化
 * - CMD_UNIFY               合一判定
 * - CMD_SET_NUMERIC_ASSUMPTION 设置数值假设降级
 */
#define LV_COMMAND_TYPE_X(x)                      \
    x(CMD_ADD_NODE, "ADD_NODE")                   \
    x(CMD_ADD_CONSTRAINT, "ADD_CONSTRAINT")       \
    x(CMD_REMOVE_NODE, "REMOVE_NODE")             \
    x(CMD_REMOVE_CONSTRAINT, "REMOVE_CONSTRAINT") \
    x(CMD_PACK_FUNCTION, "PACK_FUNCTION")         \
    x(CMD_NORMALIZE_GRAPH, "NORMALIZE_GRAPH")     \
    x(CMD_UNIFY, "UNIFY")                         \
    x(CMD_SET_NUMERIC_ASSUMPTION, "SET_NUMERIC_ASSUMPTION")

/**
 * @brief 原子命令类型枚举
 *
 * 由 LV_COMMAND_TYPE_X 生成，枚举值顺序即列表顺序（保持历史索引值不变）。
 * 所有图编辑操作必须通过内核 API 执行，每个调用对应一条命令日志条目。
 */
typedef enum {
    lv_XMACRO_ENUM(LV_COMMAND_TYPE_X)
    CMD_COUNT /**< 命令类型总数（用于数组大小） */
} CommandType;

/**
 * @brief 命令类型名称（调试/序列化用）
 */
extern const char *g_command_type_names[CMD_COUNT];

/* ════════════════════════════════════════════════════════════════
 *  命令日志条目
 * ════════════════════════════════════════════════════════════════ */

/**
 * @brief ADD_NODE 命令参数
 */
typedef struct {
    int geom_type;        /**< GeomType 枚举值 */
    int node_id;          /**< 节点 ID（-1 表示自动分配） */
    int coord_count;      /**< 符号坐标数量 */
    double *coords_num;   /**< 坐标分子数组（长度 coord_count） */
    uint64_t *coords_den; /**< 坐标分母数组（长度 coord_count） */
    int namespace_depth;
    int parent_block_id;
    bool is_formal_param;
} CmdAddNodeParams;

/**
 * @brief ADD_CONSTRAINT 命令参数
 */
typedef struct {
    int constraint_type;    /**< ConstraintType 枚举值 */
    int constraint_id;      /**< 约束 ID（-1 表示自动分配） */
    int participant_ids[8]; /**< 参与节点 ID 数组 */
    int participant_count;
} CmdAddConstraintParams;

/**
 * @brief REMOVE_NODE 命令参数
 */
typedef struct {
    int node_id;
} CmdRemoveNodeParams;

/**
 * @brief REMOVE_CONSTRAINT 命令参数
 */
typedef struct {
    int constraint_index;
} CmdRemoveConstraintParams;

/**
 * @brief PACK_FUNCTION 命令参数
 */
typedef struct {
    int internal_count;
    int *internal_node_ids;
    int input_count;
    int *input_port_ids;
    int output_count;
    int *output_port_ids;
    int result_func_id; /**< 输出：创建的函数块 ID */
} CmdPackFunctionParams;

/**
 * @brief NORMALIZE_GRAPH 命令参数
 */
typedef struct {
    bool scope_aware;
    int max_iterations;
} CmdNormalizeGraphParams;

/**
 * @brief UNIFY 命令参数
 */
typedef struct {
    int construction_graph_id; /**< 保留供将来多图使用 */
    int proposition_graph_id;
    bool result; /**< 输出：合一是否成功 */
} CmdUnifyParams;

/**
 * @brief SET_NUMERIC_ASSUMPTION 命令参数
 */
typedef struct {
    int node_id;
    double precision;
    char declaration[256];
} CmdSetNumericAssumptionParams;

/**
 * @brief 命令日志条目 —— 原子操作的完整记录
 *
 * 每条条目包含命令类型、序列号、时间戳和类型特定的参数联合体。
 * 序列号用于检测日志完整性（跳跃/乱序）。
 */
typedef struct CommandEntry {
    CommandType type;     /**< 命令类型 */
    int64_t seq;          /**< 全局递增序列号 */
    int64_t timestamp_ms; /**< 创建时间戳（毫秒） */
    union {
        CmdAddNodeParams add_node;
        CmdAddConstraintParams add_constraint;
        CmdRemoveNodeParams remove_node;
        CmdRemoveConstraintParams remove_constraint;
        CmdPackFunctionParams pack_function;
        CmdNormalizeGraphParams normalize_graph;
        CmdUnifyParams unify;
        CmdSetNumericAssumptionParams set_numeric_assumption;
    } params; /**< 类型特定参数 */

    /** 可选：逆操作类型和参数（用于撤销/重做，可为 NULL） */
    struct CommandEntry *inverse;
} CommandEntry;

/* ════════════════════════════════════════════════════════════════
 *  命令日志
 * ════════════════════════════════════════════════════════════════ */

/** @brief 命令日志不透明结构 */
typedef struct CommandLog CommandLog;

/**
 * @brief 创建命令日志
 *
 * @param initial_capacity 初始容量（<=0 使用默认值 1024）
 * @return 新创建的日志指针，失败返回 NULL
 */
CommandLog *command_log_create(int initial_capacity);

/**
 * @brief 销毁命令日志，释放所有条目
 *
 * @param log 日志指针（可为 NULL）
 */
void command_log_destroy(CommandLog *log);

/**
 * @brief 追加一条命令条目到日志尾部
 *
 * 自动分配序列号和当前时间戳。
 * 日志接过 entry 的所有权（调用后不应再访问 entry 指针）。
 *
 * @param log   日志指针
 * @param entry 命令条目指针（日志接过所有权）
 * @return true 成功，false 失败
 */
bool command_log_append(CommandLog *log, CommandEntry *entry);

/**
 * @brief 获取日志中的条目数量
 *
 * @param log 日志指针
 * @return 条目数量
 */
int command_log_count(const CommandLog *log);

/**
 * @brief 按索引获取日志条目（只读引用）
 *
 * @param log   日志指针
 * @param index 索引（0 ~ count-1）
 * @return 条目指针，无效索引返回 NULL
 */
const CommandEntry *command_log_get(const CommandLog *log, int index);

/**
 * @brief 清除日志中的所有条目（重置到空状态）
 *
 * @param log 日志指针
 */
void command_log_clear(CommandLog *log);

/**
 * @brief 获取当前序列号（用于断点续传检测）
 *
 * @param log 日志指针
 * @return 当前序列号（最后一条条目的 seq + 1，空日志返回 0）
 */
int64_t command_log_current_seq(const CommandLog *log);

/* ════════════════════════════════════════════════════════════════
 *  命令执行与重放
 * ════════════════════════════════════════════════════════════════ */

/**
 * @brief 执行单条命令并记录到日志
 *
 * 执行命令操作（调用对应内核 API），然后将条目追加到日志。
 * 如果执行失败，条目不会被记录。
 *
 * @param log    命令日志
 * @param entry  命令条目（日志在成功后接过所有权）
 * @param engine 引擎实例
 * @return true 执行成功，false 执行失败
 */
bool command_log_execute(CommandLog *log, CommandEntry *entry, lvEngine *engine);

/**
 * @brief 重放日志中的所有命令到指定引擎
 *
 * 按顺序执行日志中的每条命令。用于从日志恢复状态。
 *
 * @param log    命令日志
 * @param engine 引擎实例（必须为空引擎）
 * @return true 全部重放成功，false 有失败的命令
 */
bool command_log_replay(CommandLog *log, lvEngine *engine);

/**
 * @brief 从指定位置开始增量重放
 *
 * 只重放 seq 大于 from_seq 的命令。
 * 用于断点续传和增量同步。
 *
 * @param log     命令日志
 * @param engine  引擎实例
 * @param from_seq 起始序列号（只重放 seq > from_seq 的条目）
 * @return true 全部重放成功，false 有失败的命令
 */
bool command_log_replay_from(CommandLog *log, lvEngine *engine, int64_t from_seq);

/* ════════════════════════════════════════════════════════════════
 *  序列化 / 反序列化
 * ════════════════════════════════════════════════════════════════ */

/**
 * @brief 将命令日志序列化为 JSON 文件
 *
 * 每条命令输出为 JSON 对象数组，包含类型、序列号、时间戳和参数。
 *
 * @param log      命令日志
 * @param filepath 输出文件路径
 * @return true 成功，false 失败
 */
bool command_log_serialize_json(const CommandLog *log, const char *filepath);

/**
 * @brief 从 JSON 文件反序列化命令日志
 *
 * @param filepath JSON 文件路径
 * @return 反序列化的日志指针，失败返回 NULL
 */
CommandLog *command_log_deserialize_json(const char *filepath);

/**
 * @brief 创建命令条目的便利函数
 *
 * 为每个命令类型提供便利创建函数，自动分配正确的 type 字段。
 * 返回的条目需要由调用者通过 command_entry_destroy 释放，
 * 或传给 command_log_append/execute。
 */

CommandEntry *command_entry_create_add_node(int geom_type, int node_id, int coord_count, const double *nums,
                                            const uint64_t *dens);

CommandEntry *command_entry_create_add_constraint(int constr_type, int constr_id, const int *participants,
                                                  int participant_count);

CommandEntry *command_entry_create_remove_node(int node_id);

CommandEntry *command_entry_create_remove_constraint(int constraint_idx);

CommandEntry *command_entry_create_pack_function(int internal_count, const int *internal_ids, int input_count,
                                                 const int *input_ports, int output_count, const int *output_ports);

CommandEntry *command_entry_create_normalize_graph(bool scope_aware, int max_iterations);

CommandEntry *command_entry_create_unify(int construction_graph_id, int proposition_graph_id);

CommandEntry *command_entry_create_set_numeric_assumption(int node_id, double precision, const char *declaration);

/**
 * @brief 销毁命令条目（释放内部动态内存）
 *
 * @param entry 条目指针（可为 NULL）
 */
void command_entry_destroy(CommandEntry *entry);

#ifdef __cplusplus
}
#endif

#endif /* lv_COMMAND_LOG_H */
