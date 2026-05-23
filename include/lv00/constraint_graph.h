/**
 * @file constraint_graph.h
 * @brief 约束图 —— 几何节点、约束与哈希索引的核心数据结构
 * @details 提供几何节点（点、线段、区域、端口、函数块）和约束（关联、
 * 之间、相交、包含、连接）的创建/删除/查询接口，以及 O(1) 哈希索引
 * 加速节点和约束的查找，支持冗余检测与冲突分析。
 */

#ifndef LV00_CONSTRAINT_GRAPH_H
#define LV00_CONSTRAINT_GRAPH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "symbolic_coord.h"
#include "stream.h"
#include "error_codes.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* LV00_DEPRECATED 宏统一由 lv00.h 定义，此处不再重复声明。 */

/* 前向声明 - 用于 Port 的多态类型标记 */
typedef struct TypeRegion TypeRegion;

/**
 * @brief 几何节点类型枚举
 *
 * 标识约束图中节点的几何类型，决定了节点的数据字段和可用的操作。
 */
typedef enum {
    GEOM_POINT,            /* 几何点：零维对象，用符号坐标表示位置 */
    GEOM_LINE_SEGMENT,     /* 线段：一维对象，由两个端点定义 */
    GEOM_REGION,           /* 区域：二维对象，由边界线段围成的封闭区域 */
    GEOM_PORT,             /* 端口：函数块的输入/输出接口，支持多态类型 */
    GEOM_FUNCTION_BLOCK    /* 函数块：封装的几何构造单元，含内部节点和端口 */
} GeomType;

/**
 * @brief 端口方向枚举
 *
 * 标识端口是函数块的输入还是输出。
 */
typedef enum {
    PORT_INPUT,            /* 输入端口：接收外部数据 */
    PORT_OUTPUT            /* 输出端口：输出计算结果 */
} PortType;

/**
 * @brief 约束类型枚举
 *
 * 定义几何对象之间的约束关系类型。
 * 约束是 Lv-00 系统的核心概念，用于表达几何构造的规则和条件。
 */
typedef enum {
    INCIDENCE,             /* 关联约束：点在线段上（点属于线段） */
    BETWEENNESS,           /* 之间约束：点B在点A和点C之间（共线有序） */
    INTERSECTION,          /* 相交约束：两个几何对象在某点相交 */
    CONTAINMENT,           /* 包含约束：一个对象完全包含在另一个对象内 */
    CONNECTION             /* 连接约束：端口之间的数据流连接 */
} ConstraintType;

typedef struct GeomNode GeomNode;
typedef struct Constraint Constraint;
typedef struct ConstraintGraph ConstraintGraph;
typedef struct Port Port;

struct Port {
    int id;
    PortType type;
    int namespace_depth;
    int parent_block_id;
    bool is_formal_param;
    bool is_polymorphic;       /* 是否为多态端口（如爆炸原理的输出） */
    TypeRegion *type_region;   /* 端口类型区域（多态实例化后设置） */
    GeomNode *connected_to;
};

struct GeomNode {
    int id;
    GeomType type;
    SymbolicCoord **symbolic_coords;
    int coord_count;
    TrustColor trust;
    LightOrangeSubtype lo_subtype;
    char *numeric_assumption_declaration;
    double numeric_precision;

    int namespace_depth;
    int parent_block_id;

    union {
        Port *port;
        struct {
            GeomNode **boundary_segments;
            int segment_count;
        } region;
        struct {
            GeomNode **internal_nodes;
            int *input_port_ids;
            int *output_port_ids;
            int internal_node_count;
            int input_count;
            int output_count;
            enum {
                UNVERIFIED,
                VERIFIED,
                NON_DETERMINISTIC,
                PARTIALLY_VERIFIED
            } determinism_state;
        } func_block;
    } data;
};

struct Constraint {
    int id;
    ConstraintType type;
    int *participants;
    int participant_count;
    int template_id;
};

/**
 * 约束图 —— 理论数学研究系统的核心数据结构。
 *
 * 图以邻接表形式组织，节点和约束分别存储于动态数组中，
 * 并通过哈希索引支持 O(1) 的按 ID 查找。
 */
struct ConstraintGraph {
    /** 节点数组 —— 动态扩容，按 node_id 顺序存储 */
    GeomNode **nodes;
    int node_count;             /**< 当前节点数量 */
    int node_capacity;          /**< 节点数组容量 */

    /** 约束数组 —— 动态扩容，按 constraint_id 顺序存储 */
    Constraint **constraints;
    int constraint_count;       /**< 当前约束数量 */
    int constraint_capacity;    /**< 约束数组容量 */

    int next_node_id;           /**< 下一个可分配的节点 ID */
    int next_constraint_id;     /**< 下一个可分配的约束 ID */

    /** O(1) 节点哈希索引 —— node_id -> GeomNode* */
    GeomNode **node_index;
    int node_index_capacity;    /**< 哈希表大小（2 的幂） */

    /** O(1) 约束哈希索引 —— constraint_id -> Constraint* */
    Constraint **constraint_index;
    int constraint_index_capacity; /**< 哈希表大小（2 的幂） */
};

typedef enum {
    ADD_NODE_OK,
    ADD_NODE_CONFLICT,
    ADD_NODE_INVALID_REGION
} AddNodeResult;

typedef enum {
    ADD_CONSTRAINT_OK,
    ADD_CONSTRAINT_DUPLICATE,
    ADD_CONSTRAINT_CONFLICT
} AddConstraintResult;

typedef enum {
    REMOVE_NODE_OK,
    REMOVE_NODE_NOT_FOUND,
    REMOVE_NODE_ERROR
} RemoveNodeResult;

typedef enum {
    REMOVE_CONSTRAINT_OK,
    REMOVE_CONSTRAINT_NOT_FOUND,
    REMOVE_CONSTRAINT_ERROR
} RemoveConstraintResult;

/** @brief 将 AddNodeResult 转换为 Lv00ErrorCode */
Lv00ErrorCode lv00_add_node_result_to_error(AddNodeResult result);
/** @brief 将 AddConstraintResult 转换为 Lv00ErrorCode */
Lv00ErrorCode lv00_add_constraint_result_to_error(AddConstraintResult result);
/** @brief 将 RemoveNodeResult 转换为 Lv00ErrorCode */
Lv00ErrorCode lv00_remove_node_result_to_error(RemoveNodeResult result);

AddNodeResult graph_add_point(ConstraintGraph *graph, SymbolicCoord **coords, int coord_count);
AddNodeResult graph_add_line_segment(ConstraintGraph *graph, int endpoint1_id, int endpoint2_id);
AddNodeResult graph_add_region(ConstraintGraph *graph, int *boundary_segment_ids, int segment_count);
AddNodeResult graph_add_port(ConstraintGraph *graph, PortType type, int namespace_depth, int parent_block_id);
/**
 * @brief 向约束图添加函数块节点
 *
 * @param[in] graph             约束图
 * @param[in] internal_node_ids 内部节点 ID 数组
 * @param[in] internal_count    内部节点数量
 * @param[in] input_port_ids    输入端口 ID 数组
 * @param[in] input_count       输入端口数量
 * @param[in] output_port_ids   输出端口 ID 数组
 * @param[in] output_count      输出端口数量
 * @return 操作结果状态码
 *
 */
AddNodeResult graph_add_function_block(ConstraintGraph *graph, int *internal_node_ids, int internal_count,
                                       int *input_port_ids, int input_count,
                                       int *output_port_ids, int output_count);

/**
 * @brief 获取最近一次通过 graph_add_* 成功添加的节点 ID
 *
 * 提供对 graph->next_node_id 内部细节的安全封装，消除调用者对
 * next_node_id 递增顺序的脆弱依赖。
 *
 * @param[in] graph 约束图指针
 * @return 最近添加的节点 ID；如果尚未添加任何节点则返回 -1
 */
int graph_get_last_added_node_id(const ConstraintGraph *graph);

AddConstraintResult graph_add_incidence(ConstraintGraph *graph, int point_id, int line_or_region_id);
AddConstraintResult graph_add_betweenness(ConstraintGraph *graph, int p1_id, int p2_id, int p3_id);
AddConstraintResult graph_add_intersection(ConstraintGraph *graph, int line1_id, int line2_id, int result_point_id);
AddConstraintResult graph_add_containment(ConstraintGraph *graph, int inner_id, int outer_id);
AddConstraintResult graph_add_connection(ConstraintGraph *graph, int src_port_id, int dst_port_id);

RemoveNodeResult graph_remove_node(ConstraintGraph *graph, int node_id);
RemoveConstraintResult graph_remove_constraint(ConstraintGraph *graph, int constraint_index);
int graph_find_constraints_involving(const ConstraintGraph *graph, int node_id, int *out_indices, int max_results);
int graph_detect_redundancy(const ConstraintGraph *graph, ConstraintType type, const int *participants, int n_parts);
int graph_get_node_count(const ConstraintGraph *graph);
int graph_get_constraint_count(const ConstraintGraph *graph);

/* @deprecated Use graph_get_node() instead */
GeomNode* graph_get_node_by_id(const ConstraintGraph *graph, int node_id);

/* 推荐的节点查询 API：通过节点 ID 在 O(1) 时间内获取节点指针。 */
GeomNode *graph_get_node(const ConstraintGraph *graph, int node_id);
Constraint *graph_get_constraint(const ConstraintGraph *graph, int constraint_id);

/* 哈希索引注册接口（供 func_block.c 例化时使用） */
void graph_node_index_insert(ConstraintGraph *graph, GeomNode *node);
void graph_constraint_index_insert(ConstraintGraph *graph, Constraint *con);

/* 带指定ID添加节点和约束（用于反序列化） */
GeomNode *graph_add_node_with_id(ConstraintGraph *graph, int node_id, GeomType type,
                                  SymbolicCoord **coords, int coord_count);
Constraint *graph_add_constraint_with_id(ConstraintGraph *graph, int constraint_id,
                                          ConstraintType type, int *participants, int participant_count);

ConstraintGraph *graph_create(void);
void graph_destroy(ConstraintGraph *graph);

/* 流式上下文设置 */
void graph_set_stream_context(StreamContext *ctx);

typedef struct {
    int constraint_id;
    ConstraintType type;
    int node_ids[2];
    int node_count;
} CrossBoundaryConstraint;

CrossBoundaryConstraint* find_cross_boundary_constraints(ConstraintGraph *graph,
                                                         const int *internal_node_ids, int internal_count,
                                                         const int *port_ids, int port_count,
                                                         int *out_count);

/**
 * @brief 检测图中的冗余约束
 *
 * @param[in]  graph      约束图
 * @param[out] out_count 冗余约束数量
 * @return 冗余约束 ID 数组（调用者需 free）。
 *         出错返回 NULL。
 */
int *graph_detect_redundant_constraints(ConstraintGraph *graph, int *out_count);

/**
 * @brief 检测图中的冲突约束组
 *
 * @param[in]  graph              约束图
 * @param[out] out_conflict_count  冲突组数量
 * @param[out] out_conflict_sizes 每组的大小数组（调用者需 free）
 * @return 冲突组数组（每个组是一个约束/节点 ID 数组）。
 *         调用者需逐组释放，然后释放数组本身。
 *         无冲突或出错返回 NULL。
 */
int **graph_detect_conflicts(ConstraintGraph *graph, int *out_conflict_count, int **out_conflict_sizes);

bool graph_validate_region_closure(ConstraintGraph *graph, int region_id);

/* ============== 图序列化与反序列化 ============== */

/**
 * @brief 将图序列化为 JSON 字符串
 *
 * 序列化整个约束图，包括所有节点（点、线段、区域、端口、函数块）
 * 和约束（关联、之间、相交、包含、连接）。
 *
 * @param[in] graph 约束图
 * @return JSON 字符串（调用者负责 free），失败返回 NULL
 */
char *graph_serialize_to_json(const ConstraintGraph *graph);

/**
 * @brief 从 JSON 字符串反序列化图
 *
 * @param[in] json JSON 字符串
 * @return 反序列化的图（调用者负责 graph_destroy），失败返回 NULL
 */
ConstraintGraph *graph_deserialize_from_json(const char *json);

/**
 * @brief 序列化节点为 JSON 字符串
 *
 * @param[in] node 几何节点
 * @return JSON 字符串（调用者负责 free），失败返回 NULL
 */
char *graph_node_serialize_to_json(const GeomNode *node);

/**
 * @brief 序列化约束为 JSON 字符串
 *
 * @param[in] constraint 约束
 * @return JSON 字符串（调用者负责 free），失败返回 NULL
 */
char *graph_constraint_serialize_to_json(const Constraint *constraint);

/**
 * @brief 获取序列化错误信息
 *
 * @return 错误信息字符串（内部存储，勿 free）
 */
const char *graph_get_serialize_error(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_CONSTRAINT_GRAPH_H */