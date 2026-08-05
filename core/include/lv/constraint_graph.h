/* ========================================================================
 * 模块名称：约束图 (constraint_graph)
 * 功能概述：Lv-00 系统的核心数据结构，提供几何节点（点、线段、区域、
 *          端口、函数块）和约束（关联、之间、相交、包含、连接、角度）的
 *          创建/删除/查询接口，以及 O(1) 哈希索引加速查找，
 *          支持冗余检测与冲突分析、JSON 序列化、DOT 格式导出。
 *
 * 主要 API：
 *   - graph_create / graph_destroy                  — 创建/销毁约束图
 *   - graph_add_point / line_segment / region / ... — 添加几何节点
 *   - graph_add_incidence / betweenness / ...       — 添加约束
 *   - graph_get_node / graph_get_constraint         — O(1) 哈希查询
 *   - graph_serialize_to_json / deserialize         — JSON 序列化
 *   - graph_export_dot / export_dot_to_svg          — DOT 格式导出
 *   - graph_detect_redundant_constraints            — 冗余检测
 *   - graph_detect_conflicts                        — 冲突分析
 *
 * 使用示例：
 lv_PUBLIC_API *   ConstraintGraph *g = graph_create();
 lv_PUBLIC_API *   graph_add_point(g, coords, 2);
 lv_PUBLIC_API *   graph_add_line_segment(g, p1_id, p2_id);
 lv_PUBLIC_API *   graph_add_incidence(g, point_id, line_id);
 *
 * ======================================================================== */

/**
 * @file constraint_graph.h
 * @brief 约束图 —— 几何节点、约束与哈希索引的核心数据结构
 */

#ifndef lv_CONSTRAINT_GRAPH_H
#define lv_CONSTRAINT_GRAPH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdatomic.h> /* v3.4.1: 原子操作支持多线程安全 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "lv/cross_platform.h" /* lv_THREAD_LOCAL */
#include "symbolic_coord.h"

/* 前向声明 */
typedef struct StreamContext StreamContext;

/* ================================================================
 * v3.4.1: 多线程安全原子操作宏
 * ================================================================
 * 为 next_node_id 和 next_constraint_id 提供原子递增操作，
 * 确保多线程环境下的线程安全，避免数据竞争。
 *
 * 使用 _Atomic int 类型（在 C11 stdatomic.h 中定义），
 * 通过 atomic_fetch_add_explicit() 实现无锁原子递增，
 * 使用 memory_order_relaxed 优化性能（仅保证原子性，不保证内存顺序）。
 *
 * 对于需要严格内存顺序的场景，可使用 atomic_fetch_add() 默认的
 * memory_order_seq_cst，或显式使用 memory_order_acq_rel。
 */

/** @brief graph 模块全局流式上下文（由 constraint_graph.c 集中定义） */
extern lv_THREAD_LOCAL StreamContext *graph_stream_ctx;

/** @brief 原子递增节点ID并返回新值（线程安全） */
#define GRAPH_ATOMIC_NODE_ID_INCREMENT(graph) \
    atomic_fetch_add_explicit(&((graph)->next_node_id), 1, memory_order_relaxed)

/** @brief 原子递增约束ID并返回新值（线程安全） */
#define GRAPH_ATOMIC_CONSTRAINT_ID_INCREMENT(graph) \
    atomic_fetch_add_explicit(&((graph)->next_constraint_id), 1, memory_order_relaxed)

/* lv_DEPRECATED 宏统一由 lv.h 定义，此处不再重复声明。 */

/* 前向声明 - 用于 Port 的多态类型标记 */
typedef struct TypeRegion TypeRegion;

/* 前向声明 - lvContext (v3.4.0: 用于统一错误系统) */
struct lvContext;

/**
 * @brief 几何节点类型枚举
 *
 * 标识约束图中节点的几何类型，决定了节点的数据字段和可用的操作。
 */
typedef enum {
    GEOM_POINT,         /* 几何点：零维对象，用符号坐标表示位置 */
    GEOM_LINE_SEGMENT,  /* 线段：一维对象，由两个端点定义 */
    GEOM_REGION,        /* 区域：二维对象，由边界线段围成的封闭区域 */
    GEOM_CIRCLE,        /* 圆：由圆心和半径定义的二维几何对象 */
    GEOM_PORT,          /* 端口：函数块的输入/输出接口，支持多态类型 */
    GEOM_FUNCTION_BLOCK /* 函数块：封装的几何构造单元，含内部节点和端口 */
} GeomType;

/**
 * @brief 端口方向枚举
 *
 * 标识端口是函数块的输入还是输出。
 */
typedef enum {
    PORT_INPUT, /* 输入端口：接收外部数据 */
    PORT_OUTPUT /* 输出端口：输出计算结果 */
} PortType;

/**
 * @brief 约束类型枚举
 *
 * 定义几何对象之间的约束关系类型。
 * 约束是 Lv-00 系统的核心概念，用于表达几何构造的规则和条件。
 */
typedef enum {
    INCIDENCE,    /* 关联约束：点在线段上（点属于线段） */
    BETWEENNESS,  /* 之间约束：点B在点A和点C之间（共线有序） */
    INTERSECTION, /* 相交约束：两个几何对象在某点相交 */
    CONTAINMENT,  /* 包含约束：一个对象完全包含在另一个对象内 */
    CONNECTION,   /* 连接约束：端口之间的数据流连接 */
    ANGLE         /* 角度约束：两条线段之间的夹角 */
} ConstraintType;

typedef struct GeomNode GeomNode;
typedef struct GeomNodeVTable GeomNodeVTable;
typedef struct Constraint Constraint;
typedef struct ConstraintGraph ConstraintGraph;
typedef struct Port Port;

struct Port {
    int id;
    PortType type;
    int namespace_depth;
    int parent_block_id;
    bool is_formal_param;
    bool is_polymorphic;     /* 是否为多态端口（如爆炸原理的输出） */
    TypeRegion *type_region; /* 端口类型区域（多态实例化后设置） */
    GeomNode *connected_to;
};

/**
 * @brief 几何节点虚函数表 (VTable)
 *
 * 为每种几何节点类型提供多态操作，消除 switch/if-else 类型代码反模式。
 * 每个 GeomNode 实例通过 vtable 指针关联其类型对应的操作表。
 */
struct GeomNodeVTable {
    /** 生命周期：类型特定的节点初始化（在公共分配之后调用，node 已分配） */
    void (*alloc)(GeomNode *node, ConstraintGraph *graph);
    /** 生命周期：释放类型特定的节点数据（在公共清理之后调用） */
    void (*free)(GeomNode *node);
    /** 生命周期：深拷贝类型特定的节点数据 */
    GeomNode *(*clone)(const GeomNode *node, ConstraintGraph *dst_graph);

    /** 标识：返回节点类型的字符串名称 */
    const char *(*type_name)(void);

    /** 序列化：将类型特定的节点数据追加到 JSON 缓冲区 */
    bool (*serialize)(const GeomNode *node, void *buf);

    /** 冲突检测：检查两个节点之间是否存在类型特定的冲突 */
    bool (*detect_conflict)(const GeomNode *a, const GeomNode *b);

    /** 哈希：计算节点类型特定数据的哈希值（用于去重和比较） */
    uint32_t (*hash)(const GeomNode *node);

    /** 比较：比较两个节点的类型特定数据是否相等 */
    int (*compare)(const GeomNode *a, const GeomNode *b);

    /** 反序列化：从二进制数据恢复类型特定的节点数据 */
    bool (*deserialize)(GeomNode *node, const uint8_t *data, size_t size);

    /** 交叉引用修复：深拷贝后修复节点内部的指针引用（如 region->boundary_segments 等） */
    void (*fixup_refs)(GeomNode *node, const int *id_map, int max_id, ConstraintGraph *dst_graph);

    /** 信任坐标计数：返回用于信任颜色传播的坐标数量（0 表示该类型不参与信任传播） */
    int (*get_trust_coord_count)(const GeomNode *node);
};

/**
 * @struct GeomNode
 * @brief 几何节点 —— 约束图中的核心数据单元
 *
 * 【union data 的使用方式与各变体含义】
 * GeomNode 使用 union data 字段存储与特定几何类型相关的数据。
 * 根据 type 字段的值，应使用 union 中对应的变体：
 *   - GEOM_POINT / GEOM_LINE_SEGMENT:
 *       不使用 union data（这些类型的所有信息已由通用字段表达）
 *   - GEOM_PORT:
 *       使用 data.port（Port* 指针），指向一个 Port 结构体，
 *       包含端口方向、命名空间深度、父函数块 ID、多态类型标记等
 *   - GEOM_REGION:
 *       使用 data.region（匿名结构体），包含：
 *         - boundary_segments: 边界线段数组（GeomNode**）
 *         - segment_count:    边界线段数量
 *   - GEOM_FUNCTION_BLOCK:
 *       使用 data.func_block（匿名结构体），包含：
 *         - internal_nodes:    内部节点数组（GeomNode**）
 *         - input_port_ids:    输入端口 ID 数组（int*）
 *         - output_port_ids:   输出端口 ID 数组（int*）
 *         - internal_node_count / input_count / output_count: 各数组长度
 *         - determinism_state: 确定性状态（见下方说明）
 *
 * 【data.func_block.determinism_state 的类型说明】
 * determinism_state 是一个匿名枚举类型，定义在 func_block 结构体内，
 * 取值为以下四种之一：
 *   - UNVERIFIED:        未验证 —— 尚未进行确定性分析
 *   - VERIFIED:          已验证 —— 函数块的行为是确定性的
 *   - NON_DETERMINISTIC: 非确定性 —— 函数块可能产生不同输出
 *   - PARTIALLY_VERIFIED: 部分验证 —— 仅部分路径已验证确定性
 * 该字段用于类型检查和约束求解时判断函数块的可替换性。
 *
 * 【numeric_precision 字段是近似值】
 * numeric_precision（double 类型）表示节点的数值精度，这是一个近似值。
 * 它通常由浮点运算或数值逼近算法得出，不保证精确。
 * 使用时应注意：
 *   - 不应依赖该值进行精确相等比较（应使用容差比较）
 *   - 该值可能受浮点舍入误差影响
 *   - 对于符号计算场景，应优先使用 symbolic_coords 而非 numeric_precision
 */
struct GeomNode {
    int id;
    GeomType type;
    const GeomNodeVTable *vtable; /**< 虚函数表指针，提供节点类型的多态操作 */
    SymbolicCoord **symbolic_coords;
    int coord_count;
    TrustColor trust;
    bool is_active; /**< 节点生命周期标记：true=活跃，false=已废弃 */
    LightOrangeSubtype lo_subtype;
    char *numeric_assumption_declaration;
    double numeric_precision;

    int namespace_depth;
    int parent_block_id;

    union {
        Port *port; /* 端口数据（GEOM_PORT 类型使用） */
        struct {
            GeomNode **boundary_segments; /* 边界线段数组 */
            int segment_count;            /* 边界线段数量 */
        } region;                         /* 区域数据（GEOM_REGION 类型使用） */
        struct {
            int center_node_id; /* 圆心节点 ID */
            int radius_node_id; /* 半径端点节点 ID（圆心到此点的距离为半径） */
        } circle;               /* 圆数据（GEOM_CIRCLE 类型使用） */
        struct {
            GeomNode **internal_nodes; /* 内部节点数组 */
            int *input_port_ids;       /* 输入端口 ID 数组 */
            int *output_port_ids;      /* 输出端口 ID 数组 */
            int internal_node_count;   /* 内部节点数量 */
            int input_count;           /* 输入端口数量 */
            int output_count;          /* 输出端口数量 */
            enum {
                UNVERIFIED,        /* 未验证 */
                VERIFIED,          /* 已验证 */
                NON_DETERMINISTIC, /* 非确定性 */
                PARTIALLY_VERIFIED /* 部分验证 */
            } determinism_state;   /* 确定性状态 */
        } func_block;              /* 函数块数据（GEOM_FUNCTION_BLOCK 类型使用） */
    } data;
};

struct Constraint {
    int id;
    ConstraintType type;
    int *participants;
    int participant_count;
    int template_id;
    bool is_active;       /**< 约束生命周期标记：true=活跃，false=已废弃 */
    double numeric_value; /**< 约束的数值参数（如距离、角度等），仅部分约束类型使用；ANGLE 类型使用此字段存储角度值（度） */
    double satisfaction;  /**< 约束满意度 (0.0~1.0)，用于概率推理 */
};

/**
 * 约束图 —— 理论数学研究系统的核心数据结构。
 *
 * 图以邻接表形式组织，节点和约束分别存储于动态数组中，
 * 并通过哈希索引支持 O(1) 的按 ID 查找。
 *
 * 【哈希索引的实现方式】
 * 哈希索引使用开放寻址法（open addressing）实现：
 *   - node_index 和 constraint_index 分别是 GeomNode** 和 Constraint** 数组
 *   - 索引容量（node_index_capacity / constraint_index_capacity）始终为 2 的幂
 *   - 哈希函数为 node_id % capacity（或 constraint_id % capacity）
 *   - 冲突时采用线性探测（linear probing）解决
 *   - 空槽位以 NULL 标记
 * 插入和查找操作的平均时间复杂度为 O(1)，最坏情况为 O(n)（当装载因子过高时）。
 * 哈希索引通过 graph_node_index_insert() 和 graph_constraint_index_insert()
 * 注册，通过 graph_get_node() 和 graph_get_constraint() 查询。
 *
 * 【error_buffer 和 serialize_buffer 的大小与溢出风险】
 * error_buffer 和 serialize_buffer 均为堆分配的 256 字节字符数组：
 *   - error_buffer:    存储图级内部错误消息（替代旧版 static 全局变量）
 *   - serialize_buffer: 存储图级序列化错误消息
 * 溢出风险：
 *   - 两个缓冲区大小固定为 256 字节，写入超过 255 个字符（含终止符）将导致
 *     缓冲区溢出，可能引发内存损坏或安全漏洞
 *   - 内部实现应使用 snprintf() 等带长度限制的函数，而非 sprintf()
 *   - 如果错误消息可能很长（如包含长路径名或复杂约束描述），应考虑截断处理
 *   - 调用 graph_get_serialize_error() 等函数获取错误信息时，返回的指针
 *     指向图内部存储，无需手动释放，但也不应在图销毁后继续使用
 */
struct ConstraintGraph {
    /** 节点数组 —— 动态扩容，按 node_id 顺序存储 */
    GeomNode **nodes;
    int node_count;    /**< 当前节点数量 */
    int node_capacity; /**< 节点数组容量 */

    /** 约束数组 —— 动态扩容，按 constraint_id 顺序存储 */
    Constraint **constraints;
    int constraint_count;    /**< 当前约束数量 */
    int constraint_capacity; /**< 约束数组容量 */

    /* ================================================================
     * v3.4.1: 原子类型确保多线程安全 ID 分配
     * ================================================================
     * next_node_id 和 next_constraint_id 使用 _Atomic int 类型，
     * 通过 atomic_fetch_add_explicit() 实现无锁原子递增。
     * 在多线程环境下，节点和约束的 ID 分配不会出现数据竞争。
     *
     * 注意：初始化时应使用 ATOMIC_INT_INIT 宏或在 graph_create 中
     * 使用 atomic_store(&graph->next_node_id, 0)。
     */
    _Atomic int next_node_id;       /**< 下一个可分配的节点 ID（原子操作，线程安全） */
    _Atomic int next_constraint_id; /**< 下一个可分配的约束 ID（原子操作，线程安全） */

    /** O(1) 节点哈希索引 —— node_id -> GeomNode* */
    GeomNode **node_index;
    int node_index_capacity; /**< 哈希表大小（2 的幂） */

    /** O(1) 约束哈希索引 —— constraint_id -> Constraint* */
    Constraint **constraint_index;
    int constraint_index_capacity; /**< 哈希表大小（2 的幂） */

    /* ============================================================
     * 每图级的错误缓冲区（替代旧版静态全局变量，v3.3.0）
     *
     * 旧版使用静态 file-scope char[] 存储错误信息，限制了并发使用。
     * 现将错误缓冲区提升为堆分配的每图级字段：
     *   - error_buffer[256]：    替代旧版 static g_internal_error[256]
     *   - serialize_buffer[256]：替代旧版 static g_serialize_error[256]
     *
     * 在 graph_create() 中分配，在 graph_destroy() 中释放。
     * 每个 ConstraintGraph 实例拥有独立的错误轨道，
     * 多线程/多引擎场景下不再相互覆盖错误信息。
     * ============================================================ */
    char *error_buffer;     /**< 图级内部错误消息缓冲区（256 字节，堆分配） */
    char *serialize_buffer; /**< 图级序列化错误消息缓冲区（256 字节，堆分配） */

    /* ============================================================
     * lvContext 指针 (v3.4.0: 统一错误系统迁移)
     *
     * 当 graph 通过 engine 关联到 lvContext 时，此字段指向对应的上下文。
     * graph_set_error() 和 graph_get_error() 函数会优先使用 context
     * 中的错误存储，fallback 到 error_buffer。
     * ============================================================ */
    struct lvContext *context; /**< 关联的 lvContext 实例（可选，v3.4.0 新增） */
    bool dirty;                /**< 脏标记：约束被修改时置 true，需同步后置 false */
};

typedef enum {
    ADD_NODE_OK,             /* 添加成功 */
    ADD_NODE_CONFLICT,       /* 添加冲突 */
    ADD_NODE_INVALID_REGION, /* 无效区域 */
    ADD_NODE_ERROR           /* 添加错误 */
} AddNodeResult;

typedef enum {
    lv_CONSTRAINT_STATUS_CONSISTENT = 0,        /* 约束集合存在普通模型且未发现冗余 */
    lv_CONSTRAINT_STATUS_INCONSISTENT = 1,      /* 约束集合存在直接矛盾或非法退化 */
    lv_CONSTRAINT_STATUS_UNDER_CONSTRAINED = 2, /* 约束不足，无法形成确定几何结构 */
    lv_CONSTRAINT_STATUS_OVER_CONSTRAINED = 3,  /* 存在重复或冗余约束 */
    lv_CONSTRAINT_STATUS_INVALID = 4            /* 输入图或输出参数无效 */
} lvConstraintStatus;

typedef struct lvConstraintCompatibilityResult {
    lvConstraintStatus status;
    int conflicting_constraint_id;
    int redundant_constraint_count;
    int free_degree_count;
    const char *diagnostic;
} lvConstraintCompatibilityResult;

typedef enum { ADD_CONSTRAINT_OK, ADD_CONSTRAINT_DUPLICATE, ADD_CONSTRAINT_CONFLICT } AddConstraintResult;

typedef enum { REMOVE_NODE_OK, REMOVE_NODE_NOT_FOUND, REMOVE_NODE_ERROR } RemoveNodeResult;

typedef enum { REMOVE_CONSTRAINT_OK, REMOVE_CONSTRAINT_NOT_FOUND, REMOVE_CONSTRAINT_ERROR } RemoveConstraintResult;

/** @brief 将 AddNodeResult 转换为 lvErrorCode */
lv_PUBLIC_API lvErrorCode lv_add_node_result_to_error(AddNodeResult result);
/** @brief 将 AddConstraintResult 转换为 lvErrorCode */
lv_PUBLIC_API lvErrorCode lv_add_constraint_result_to_error(AddConstraintResult result);
/** @brief 将 RemoveNodeResult 转换为 lvErrorCode */
lv_PUBLIC_API lvErrorCode lv_remove_node_result_to_error(RemoveNodeResult result);

/**
 * @brief 向约束图添加几何点节点
 *
 * @param[in] graph       约束图
 * @param[in] coords      符号坐标数组
 * @param[in] coord_count 坐标数量
 * @return 操作结果状态码
 */
lv_PUBLIC_API AddNodeResult graph_add_point(ConstraintGraph *graph, SymbolicCoord *const *coords, int coord_count);

/**
 * @brief 向约束图添加线段节点
 *
 * @param[in] graph      约束图
 * @param[in] endpoint1_id 第一个端点的节点 ID
 * @param[in] endpoint2_id 第二个端点的节点 ID
 * @return 操作结果状态码
 */
lv_PUBLIC_API AddNodeResult graph_add_line_segment(ConstraintGraph *graph, int endpoint1_id, int endpoint2_id);

/**
 * @brief 向约束图添加区域节点
 *
 * @param[in] graph                约束图
 * @param[in] boundary_segment_ids 边界线段 ID 数组
 * @param[in] segment_count        边界线段数量
 * @return 操作结果状态码
 */
lv_PUBLIC_API AddNodeResult graph_add_region(ConstraintGraph *graph, const int *boundary_segment_ids,
                                             int segment_count);

/**
 * @brief 向约束图添加圆节点
 *
 * @param[in] graph          约束图
 * @param[in] center_node_id 圆心节点 ID
 * @param[in] radius_node_id 半径端点节点 ID（圆心到此点的距离为半径）
 * @return 操作结果状态码
 */
lv_PUBLIC_API AddNodeResult graph_add_circle(ConstraintGraph *graph, int center_node_id, int radius_node_id);

/**
 * @brief 向约束图添加端口节点
 *
 * @param[in] graph            约束图
 * @param[in] type             端口方向（输入/输出）
 * @param[in] namespace_depth  命名空间深度
 * @param[in] parent_block_id  父函数块 ID
 * @return 操作结果状态码
 */
lv_PUBLIC_API AddNodeResult graph_add_port(ConstraintGraph *graph, PortType type, int namespace_depth,
                                           int parent_block_id);
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
lv_PUBLIC_API AddNodeResult graph_add_function_block(ConstraintGraph *graph, const int *internal_node_ids,
                                                     int internal_count, const int *input_port_ids, int input_count,
                                                     const int *output_port_ids, int output_count);

/**
 * @brief 获取最近一次通过 graph_add_* 成功添加的节点 ID
 *
 * 提供对 graph->next_node_id 内部细节的安全封装，消除调用者对
 * next_node_id 递增顺序的脆弱依赖。
 *
 * @param[in] graph 约束图指针
 * @return 最近添加的节点 ID；如果尚未添加任何节点则返回 -1
 */
lv_PUBLIC_API int graph_get_last_added_node_id(const ConstraintGraph *graph);

/**
 * @brief 添加关联约束（点在线段/区域上）
 *
 * @param[in] graph              约束图
 * @param[in] point_id           点节点 ID
 * @param[in] line_or_region_id  线段或区域节点 ID
 * @return 操作结果状态码
 */
lv_PUBLIC_API AddConstraintResult graph_add_incidence(ConstraintGraph *graph, int point_id, int line_or_region_id);

/**
 * @brief 添加之间约束（点 B 在点 A 和点 C 之间）
 *
 * @param[in] graph 约束图
 * @param[in] p1_id 第一个端点 ID
 * @param[in] p2_id 中间点 ID
 * @param[in] p3_id 第二个端点 ID
 * @return 操作结果状态码
 */
lv_PUBLIC_API AddConstraintResult graph_add_betweenness(ConstraintGraph *graph, int p1_id, int p2_id, int p3_id);

/**
 * @brief 添加相交约束（两线段在交点处相交）
 *
 * @param[in] graph          约束图
 * @param[in] line1_id       第一条线段 ID
 * @param[in] line2_id       第二条线段 ID
 * @param[in] result_point_id 交点节点 ID
 * @return 操作结果状态码
 */
lv_PUBLIC_API AddConstraintResult graph_add_intersection(ConstraintGraph *graph, int line1_id, int line2_id,
                                                         int result_point_id);

/**
 * @brief 添加包含约束（内部对象完全包含在外部对象内）
 *
 * @param[in] graph   约束图
 * @param[in] inner_id  内部对象节点 ID
 * @param[in] outer_id  外部对象节点 ID
 * @return 操作结果状态码
 */
lv_PUBLIC_API AddConstraintResult graph_add_containment(ConstraintGraph *graph, int inner_id, int outer_id);

/**
 * @brief 添加连接约束（端口之间的数据流连接）
 *
 * @param[in] graph       约束图
 * @param[in] src_port_id 源端口 ID
 * @param[in] dst_port_id 目标端口 ID
 * @return 操作结果状态码
 */
lv_PUBLIC_API AddConstraintResult graph_add_connection(ConstraintGraph *graph, int src_port_id, int dst_port_id);

/**
 * @brief 添加角度约束（两条线段之间的夹角）
 *
 * @param[in] graph     约束图
 * @param[in] line1_id  第一条线段 ID
 * @param[in] line2_id  第二条线段 ID
 * @param[in] angle_degrees 角度值（度）
 * @return 操作结果状态码
 */
lv_PUBLIC_API AddConstraintResult graph_add_angle(ConstraintGraph *graph, int line1_id, int line2_id, double angle_degrees);

/**
 * @brief 从约束图中移除节点
 *
 * @param[in] graph   约束图
 * @param[in] node_id 要移除的节点 ID
 * @return 操作结果状态码
 */
lv_PUBLIC_API RemoveNodeResult graph_remove_node(ConstraintGraph *graph, int node_id);

/**
 * @brief 从约束图中移除约束
 *
 * @param[in] graph           约束图
 * @param[in] constraint_index 约束索引
 * @return 操作结果状态码
 */
lv_PUBLIC_API RemoveConstraintResult graph_remove_constraint(ConstraintGraph *graph, int constraint_index);

/**
 * @brief 查找涉及指定节点的所有约束
 *
 * @param[in]  graph       约束图
 * @param[in]  node_id     节点 ID
 * @param[out] out_indices 输出约束索引数组
 * @param[in]  max_results 数组最大容量
 * @return 找到的约束数量
 */
lv_PUBLIC_API int graph_find_constraints_involving(const ConstraintGraph *graph, int node_id, int *out_indices,
                                                   int max_results);

/**
 * @brief 检查约束图的相容性状态
 *
 * 该接口属于约束拓扑规约层，只返回相容性诊断，不执行证明搜索，
 * 不生成证明输出。首批实现覆盖空图欠约束、普通线段相容、退化
 * 线段矛盾、重复线段过约束等基础情形。
 *
 * @param[in]  graph      约束图
 * @param[out] out_result 相容性诊断结果
 * @return true 表示成功写入诊断；false 表示输入无效
 */
lv_PUBLIC_API bool graph_check_compatibility(const ConstraintGraph *graph, lvConstraintCompatibilityResult *out_result);

/**
 * @brief 检测指定约束是否冗余
 *
 * @param[in] graph        约束图
 * @param[in] type         约束类型
 * @param[in] participants 参与者节点 ID 数组
 * @param[in] n_parts      参与者数量
 * @return 冗余约束的索引，无冗余返回 -1
 */
lv_PUBLIC_API int graph_detect_redundancy(const ConstraintGraph *graph, ConstraintType type, const int *participants,
                                          int n_parts);

/**
 * @brief 获取约束图中的节点总数
 *
 * @param[in] graph 约束图
 * @return 节点数量
 */
lv_PUBLIC_API int graph_get_node_count(const ConstraintGraph *graph);

/**
 * @brief 获取约束图中的约束总数
 *
 * @param[in] graph 约束图
 * @return 约束数量
 */
lv_PUBLIC_API int graph_get_constraint_count(const ConstraintGraph *graph);

/**
 * @deprecated 使用 graph_get_node() 代替
 *
 * @param[in] graph   约束图
 * @param[in] node_id 节点 ID
 * @return 节点指针，未找到返回 NULL
 */
lv_PUBLIC_API GeomNode *graph_get_node_by_id(const ConstraintGraph *graph, int node_id);

/**
 * @brief 通过节点 ID 在 O(1) 时间内获取节点指针（推荐）
 *
 * @param[in] graph   约束图
 * @param[in] node_id 节点 ID
 * @return 节点指针，未找到返回 NULL
 */
lv_PUBLIC_API GeomNode *graph_get_node(const ConstraintGraph *graph, int node_id);

/**
 * @brief 通过约束 ID 获取约束指针
 *
 * @param[in] graph         约束图
 * @param[in] constraint_id 约束 ID
 * @return 约束指针，未找到返回 NULL
 */
lv_PUBLIC_API Constraint *graph_get_constraint(const ConstraintGraph *graph, int constraint_id);

/**
 * @brief 向哈希索引注册节点（供 func_block.c 例化时使用）
 *
 * @param[in] graph 约束图
 * @param[in] node  要注册的节点
 */
lv_PUBLIC_API void graph_node_index_insert(ConstraintGraph *graph, GeomNode *node);

/**
 * @brief 向哈希索引注册约束（供 func_block.c 例化时使用）
 *
 * @param[in] graph 约束图
 * @param[in] con   要注册的约束
 */
lv_PUBLIC_API void graph_constraint_index_insert(ConstraintGraph *graph, Constraint *con);

/**
 * @brief 使用指定 ID 添加节点（用于反序列化）
 *
 * @param[in] graph       约束图
 * @param[in] node_id     指定的节点 ID
 * @param[in] type        几何类型
 * @param[in] coords      符号坐标数组
 * @param[in] coord_count 坐标数量
 * @return 新创建的节点指针，失败返回 NULL
 */
lv_PUBLIC_API GeomNode *graph_add_node_with_id(ConstraintGraph *graph, int node_id, GeomType type,
                                               SymbolicCoord **coords, int coord_count);

/**
 * @brief 使用指定 ID 添加约束（用于反序列化）
 *
 * @param[in] graph            约束图
 * @param[in] constraint_id    指定的约束 ID
 * @param[in] type             约束类型
 * @param[in] participants     参与者节点 ID 数组
 * @param[in] participant_count 参与者数量
 * @return 新创建的约束指针，失败返回 NULL
 */
lv_PUBLIC_API Constraint *graph_add_constraint_with_id(ConstraintGraph *graph, int constraint_id, ConstraintType type,
                                                       const int *participants, int participant_count);

/**
 * @brief 创建空的约束图
 *
 * @return 新创建的约束图，失败返回 NULL
 *
 * @note 调用者获得所有权，需在不再使用时调用 graph_destroy() 释放。
 */
lv_PUBLIC_API ConstraintGraph *graph_create(void);

/**
 * @brief 深拷贝约束图
 *
 * 遍历源图中的所有节点和约束，在新图中创建完全独立的副本。
 * 高级类型（Region/Circle/Port/FunctionBlock）的类型特定数据
 * （boundary_segments、center/radius_node_id、data.port、
 * internal_nodes/input/output_port_ids）通过 vtable->clone 深拷贝，
 * 内部指针引用通过 vtable->fixup_refs 重映射到新图。
 * 调用者负责对返回的图调用 graph_destroy() 释放。
 *
 * @param graph 源图（非 NULL）
 * @return 新分配的图副本，失败返回 NULL
 */
lv_PUBLIC_API ConstraintGraph *graph_copy(const ConstraintGraph *graph);

/**
 * @brief 销毁约束图，释放所有内部资源
 *
 * @param graph 约束图指针（可为 NULL，NULL 时安全返回）
 *
 * @note 释放后 graph 指针不可再使用。
 */
lv_PUBLIC_API void graph_destroy(ConstraintGraph *graph);

/**
 * @brief 设置约束图的流式输出上下文
 *
 * @param ctx 流式上下文（可为 NULL 以禁用流式输出）
 */
lv_PUBLIC_API void graph_set_stream_context(StreamContext *ctx);

/**
 * @brief 跨边界约束结构
 *
 * 描述一个跨越函数块边界的约束，包含约束 ID、类型和涉及的节点。
 */
typedef struct {
    int constraint_id;   /**< 约束 ID */
    ConstraintType type; /**< 约束类型 */
    int node_ids[2];     /**< 涉及的节点 ID（内部节点和外部节点） */
    int node_count;      /**< 涉及的节点数量 */
} CrossBoundaryConstraint;

/**
 * @brief 查找跨越函数块边界的约束
 *
 * 检测内部节点与外部节点之间的所有约束连接。
 *
 * @param[in]  graph           约束图
 * @param[in]  internal_node_ids 内部节点 ID 数组
 * @param[in]  internal_count  内部节点数量
 * @param[in]  port_ids        端口 ID 数组
 * @param[in]  port_count      端口数量
 * @param[out] out_count       输出：找到的跨边界约束数量
 * @return 跨边界约束数组（调用者需 free），无跨边界约束返回 NULL
 */
lv_PUBLIC_API CrossBoundaryConstraint *find_cross_boundary_constraints(ConstraintGraph *graph,
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
lv_PUBLIC_API int *graph_detect_redundant_constraints(const ConstraintGraph *graph, int *out_count);

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
lv_PUBLIC_API int **graph_detect_conflicts(const ConstraintGraph *graph, int *out_conflict_count,
                                           int **out_conflict_sizes);

/**
 * @brief 验证区域的闭合性（边界线段是否形成封闭环路）
 *
 * @param[in] graph     约束图
 * @param[in] region_id 区域节点 ID
 * @return true 区域闭合，false 区域不闭合或出错
 */
lv_PUBLIC_API bool graph_validate_region_closure(const ConstraintGraph *graph, int region_id);

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
lv_PUBLIC_API char *graph_serialize_to_json(const ConstraintGraph *graph);

/**
 * @brief 从 JSON 字符串反序列化图
 *
 * @param[in] json JSON 字符串
 * @return 反序列化的图（调用者负责 graph_destroy），失败返回 NULL
 */
lv_PUBLIC_API ConstraintGraph *graph_deserialize_from_json(const char *json);

/**
 * @brief 序列化节点为 JSON 字符串
 *
 * @param[in] node 几何节点
 * @return JSON 字符串（调用者负责 free），失败返回 NULL
 */
lv_PUBLIC_API char *graph_node_serialize_to_json(const GeomNode *node);

/**
 * @brief 序列化约束为 JSON 字符串
 *
 * @param[in] constraint 约束
 * @return JSON 字符串（调用者负责 free），失败返回 NULL
 */
lv_PUBLIC_API char *graph_constraint_serialize_to_json(const Constraint *constraint);

/**
 * @brief 获取图级序列化错误信息（v3.3.0：需传入图指针）
 *
 * 每个 ConstraintGraph 实例拥有独立的序列化错误缓冲区，
 * 替代旧版全局静态变量 g_serialize_error。
 *
 * @param graph 约束图（不可为 NULL）
 * @return 错误信息字符串（内部存储，勿 free），graph 为 NULL 时返回空字符串
 */
lv_PUBLIC_API const char *graph_get_serialize_error(const ConstraintGraph *graph);

/* ============================================================
 * 统一错误系统 (v3.4.0: 迁移到 lvContext)
 *
 * graph_set_error() 和 graph_get_error() 函数优先使用
 * graph->context 中的错误存储（如果有），fallback 到
 * graph->error_buffer。
 *
 * 设计原则：
 *   - 优先使用 context 错误存储（支持隔离上下文）
 *   - context 为 NULL 时 fallback 到 error_buffer
 *   - 保持向后兼容，现有调用无需修改
 * ============================================================ */

/**
 * @brief 设置约束图的错误信息 (v3.4.0: 支持 lvContext)
 *
 * 优先将错误信息存储到 graph->context->error_message 中
 * (如果有 context)，fallback 到 graph->error_buffer。
 *
 * @param graph 约束图（可以为 NULL，但错误信息不会被存储）
 * @param fmt   printf 风格的格式字符串
 * @param ...   可变参数列表
 */
lv_PUBLIC_API void graph_set_error(ConstraintGraph *graph, const char *fmt, ...);

/**
 * @brief 获取约束图的错误信息 (v3.4.0: 支持 lvContext)
 *
 * 优先从 graph->context->error_message 读取错误信息
 * (如果有 context 且有错误)，fallback 到 graph->error_buffer。
 *
 * @param graph 约束图（可以为 NULL，返回 "NULL graph"）
 * @return 错误信息字符串（内部存储，勿 free）
 */
lv_PUBLIC_API const char *graph_get_error(const ConstraintGraph *graph);

/* ========================================================================
 * DOT 格式导出（借鉴 Graphviz DOT 声明式图描述语言，v3.3.0）
 *
 * 将约束图导出为 DOT 格式字符串，直接可由 Graphviz
 * (dot/neato/fdp/sfdp/circo/twopi) 渲染为 SVG/PNG/PDF。
 *
 * 节点渲染为矩形，标注类型颜色（点=蓝、线段=绿、区域=橙、
 * 端口=灰、函数块=紫）和维度信息。
 * 约束渲染为有向边，标注约束类型（INCIDENCE/BETWEENNESS/
 * INTERSECTION/CONTAINMENT/CONNECTION/ANGLE）。
 *
 * 用途：
 * - 约束图可视化（替代手写 Canvas 布局）
 * - 文档生成（Markdown 嵌入 DOT → 自动渲染）
 * - 调试（快速查看约束图结构）
 * ======================================================================== */

/** DOT 导出选项 */
typedef enum {
    DOT_LAYOUT_HIERARCHY = 0, /**< dot 层级布局（适合树形约束） */
    DOT_LAYOUT_SPRING = 1,    /**< neato 弹簧模型（适合一般图） */
    DOT_LAYOUT_FORCE = 2,     /**< fdp 力导向（适合稠密图） */
    DOT_LAYOUT_SCALABLE = 3,  /**< sfdp 大规模力导向 */
    DOT_LAYOUT_CIRCULAR = 4,  /**< circo 环形布局 */
    DOT_LAYOUT_RADIAL = 5,    /**< twopi 径向布局 */
} DOTLayoutEngine;

/** DOT 导出配置 */
typedef struct {
    DOTLayoutEngine layout;      /**< 布局引擎 */
    bool show_node_ids;          /**< 是否显示节点 ID */
    bool show_coords;            /**< 是否显示坐标信息 */
    bool show_dimensions;        /**< 是否显示维度信息 */
    bool show_trust_colors;      /**< 是否显示信任颜色（GREEN/BLUE/YELLOW/...） */
    bool show_namespace_depth;   /**< 是否显示命名空间深度 */
    bool show_constraint_labels; /**< 是否显示约束标签 */
    bool cluster_by_namespace;   /**< 是否按命名空间分簇（subgraph cluster） */
    bool html_labels;            /**< 使用 HTML-like labels（丰富样式） */
    const char *graph_label;     /**< 图标题（NULL = 无标题） */
    const char *font_name;       /**< 字体名（NULL = 默认） */
    int font_size;               /**< 字体大小（0 = 默认 12） */
    double node_margin;          /**< 节点边距（0 = 默认 0.1） */
    double edge_len;             /**< 理想边长（0 = 默认，仅 neato/fdp） */
} DOTExportConfig;

/**
 * @brief 创建默认 DOT 导出配置
 *
 * 默认：LAYOUT_SPRING, show_node_ids=true, show_coords=true,
 * show_constraint_labels=true, 其余 false
 *
 * @return 默认配置
 */
lv_PUBLIC_API DOTExportConfig dot_export_config_default(void);

/**
 * @brief 将约束图导出为 Graphviz DOT 格式字符串
 *
 * 导出的 DOT 文本可直接保存为 .dot 文件，用 `dot -Tsvg graph.dot -o graph.svg`
 * 渲染。也可在 Markdown 中嵌入：
 *
 * ```dot
 * ... DOT 内容 ...
 * ```
 *
 * @param[in] graph   约束图（非 NULL）
 * @param[in] config  DOT 导出配置
 * @return DOT 格式字符串（调用者负责 free），失败返回 NULL
 *
 * @note 借鉴 Graphviz (graphviz.org) — AT&T 30+ 年稳定维护的图可视化标准
 */
lv_PUBLIC_API char *graph_export_dot(const ConstraintGraph *graph, const DOTExportConfig *config);

/**
 * @brief 将约束图导出为 DOT 文件
 *
 * @param[in] graph      约束图（非 NULL）
 * @param[in] config     DOT 导出配置
 * @param[in] filepath   输出文件路径
 * @return lv_OK 成功，lv_ERROR_INVALID_ARG 参数无效，其他错误码
 *
 * @note 内部调用 graph_export_dot() 后写入文件
 */
lv_PUBLIC_API int graph_export_dot_file(const ConstraintGraph *graph, const DOTExportConfig *config,
                                        const char *filepath);

/**
 * @brief 快捷导出：约束图 → DOT → 渲染为 SVG（需系统安装 graphviz）
 *
 * 内部流程：
 * 1. graph_export_dot() 生成 DOT 文本
 * 2. 写入临时文件
 * 3. 调用 `dot -Tsvg temp.dot -o output.svg`
 * 4. 清理临时文件
 *
 * @param[in] graph      约束图（非 NULL）
 * @param[in] config     DOT 导出配置
 * @param[in] output_svg 输出 SVG 文件路径
 * @return lv_OK 成功，失败返回错误码
 *
 * @note 需系统 PATH 中有 graphviz 的 dot 命令
 */
lv_PUBLIC_API int graph_export_dot_to_svg(const ConstraintGraph *graph, const DOTExportConfig *config,
                                          const char *output_svg);

/* ============================================================
 * 约束生命周期管理与脏标记同步 (v3.5.0)
 *
 * 提供约束的惰性废弃机制和 dirty 标记传播机制，
 * 用于在约束被修改后同步所有受影响节点的属性。
 * ============================================================ */

/**
 * @brief 标记约束图为脏状态（约束被修改时调用）
 *
 * 设置 graph->dirty = true，表示图中节点属性需要刷新。
 * 应在每次约束添加、修改或删除后调用。
 *
 * @param graph 约束图（非 NULL）
 */
lv_PUBLIC_API void graph_mark_dirty(ConstraintGraph *graph);

/**
 * @brief 同步约束图中所有受影响节点的属性
 *
 * 遍历所有受影响的节点并刷新其属性。
 * 同步完成后将 dirty 标记重置为 false。
 * 通常在调用 graph_mark_dirty() 后、求解前调用。
 *
 * @param graph 约束图（非 NULL）
 */
lv_PUBLIC_API void graph_sync_nodes(ConstraintGraph *graph);

/**
 * @brief 废弃约束（惰性删除，保留审计跟踪）
 *
 * 将约束标记为不活跃（is_active = false），从活跃约束索引中移除，
 * 但保留其数据以便审计跟踪。遍历约束时自动跳过不活跃约束。
 *
 * @param graph         约束图（非 NULL）
 * @param constraint_id 要废弃的约束 ID
 * @return lv_OK 成功，其他错误码表示失败
 */
lv_PUBLIC_API int graph_deactivate_constraint(ConstraintGraph *graph, int constraint_id);

#ifdef __cplusplus
}
#endif

#endif /* lv_CONSTRAINT_GRAPH_H */