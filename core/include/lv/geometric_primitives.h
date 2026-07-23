/**
 * @file geometric_primitives.h
 * @brief 几何原语统一包装层 -- 13 个 geo_* 原语的 DSL 汇编指令集
 *
 * 基于 doc/docs/geometric_primitives.md 定义的最小完备原语集，
 * 对 Lv-00 C 核的底层 API 提供统一的 geo_* 前缀包装。
 * 每个原语职责单一、正交、可组合，满足最小完备性要求。
 */

#ifndef lv_GEOMETRIC_PRIMITIVES_H
#define lv_GEOMETRIC_PRIMITIVES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

/* 前向声明 -- 避免引入过重的头文件依赖 */
struct ConstraintGraph;
struct lvEngine;
struct ProofNavigator;
typedef struct ConstraintGraph ConstraintGraph;
typedef struct lvEngine lvEngine;
typedef struct ProofNavigator ProofNavigator;

/* ================================================================
 * 统一结果类型
 * ================================================================ */

/**
 * @brief 几何原语统一状态码
 */
typedef enum {
    GEO_STATUS_OK = 0,        /**< 操作成功 */
    GEO_STATUS_NULL_ARG,      /**< 空指针参数 */
    GEO_STATUS_INVALID_TYPE,  /**< 无效的类型参数 */
    GEO_STATUS_INVALID_PARAM, /**< 无效的参数 */
    GEO_STATUS_NOT_FOUND,     /**< 未找到目标对象 */
    GEO_STATUS_CONFLICT,      /**< 约束冲突 */
    GEO_STATUS_NO_SOLUTION,   /**< 无解 */
    GEO_STATUS_TIMEOUT,       /**< 操作超时 */
    GEO_STATUS_IO_ERROR,      /**< I/O 错误 */
    GEO_STATUS_UNSUPPORTED,   /**< 不支持的格式/操作 */
    GEO_STATUS_INTERNAL_ERROR /**< 内部错误 */
} GeoStatus;

/**
 * @brief 几何原语统一返回结果
 *
 * status  表示操作结果状态码。
 * data    指向操作产生的数据（类型取决于具体原语），可为 NULL。
 * message 错误/状态描述信息，可为 NULL。
 *
 * data 和 message 的所有权语义由各原语函数文档说明。
 */
typedef struct {
    GeoStatus status;    /**< 操作状态码 */
    void *data;          /**< 结果数据（具体类型见各原语说明） */
    const char *message; /**< 状态/错误描述 */
} GeoResult;

/* ================================================================
 * 节点类型与约束类型枚举（与底层 GeomType / ConstraintType 对齐）
 * ================================================================ */

/** @brief 几何节点类型 */
typedef enum {
    GEO_NODE_POINT = 0,     /**< 几何点 */
    GEO_NODE_LINE_SEGMENT,  /**< 线段 */
    GEO_NODE_REGION,        /**< 区域 */
    GEO_NODE_PORT,          /**< 端口 */
    GEO_NODE_FUNCTION_BLOCK /**< 函数块 */
} GeoNodeType;

/** @brief 约束关系类型 */
typedef enum {
    GEO_CONSTRAINT_INCIDENCE = 0, /**< 关联：点在线段/区域上 */
    GEO_CONSTRAINT_BETWEENNESS,   /**< 之间：点B在点A和点C之间 */
    GEO_CONSTRAINT_INTERSECTION,  /**< 相交：两线段在交点处相交 */
    GEO_CONSTRAINT_CONTAINMENT,   /**< 包含：内部对象在外对象内 */
    GEO_CONSTRAINT_CONNECTION     /**< 连接：端口之间的数据流 */
} GeoConstraintType;

/* ================================================================
 * 13 个几何原语声明
 * ================================================================ */

/**
 * @brief 原语 1：创建几何节点
 * @param graph 约束图（不可为 NULL）
 * @param type  节点类型（GeoNodeType）
 * @param ids   节点 ID 数组（含义取决于类型，如端点 ID、坐标索引等）
 * @param count ids 数组长度
 * @return GeoResult，data 指向 int（新节点 ID，调用者需 free）
 */
GeoResult geo_create_node(ConstraintGraph *graph, GeoNodeType type, const int *ids, int count);

/**
 * @brief 原语 2：创建约束关系
 * @param graph        约束图（不可为 NULL）
 * @param type         约束类型（GeoConstraintType）
 * @param participants 参与节点 ID 数组
 * @param count        参与节点数量
 * @return GeoResult，data 为 NULL
 */
GeoResult geo_create_constraint(ConstraintGraph *graph, GeoConstraintType type, const int *participants, int count);

/**
 * @brief 原语 3：求解约束系统
 * @param engine 引擎实例（不可为 NULL）
 * @return GeoResult，data 为 NULL（成功时）
 */
GeoResult geo_solve(lvEngine *engine);

/**
 * @brief 原语 4：约束图归一化
 * @param graph       约束图（不可为 NULL）
 * @param scope_aware 是否考虑作用域边界
 * @return GeoResult，data 指向 int（合并节点数，调用者需 free）
 */
GeoResult geo_normalize(ConstraintGraph *graph, bool scope_aware);

/**
 * @brief 原语 5：应用重写规则
 * @param graph      约束图（不可为 NULL）
 * @param rules      重写规则数组（RewriteRule**，以 void** 传递）
 * @param rule_count 规则数量
 * @param step_limit 最大重写步数（0 使用默认值）
 * @return GeoResult，data 指向 int（实际应用步数，调用者需 free）
 */
GeoResult geo_rewrite(ConstraintGraph *graph, void **rules, int rule_count, int step_limit);

/**
 * @brief 原语 6：统一构造与命题
 * @param construction 构造图（不可为 NULL）
 * @param proposition  命题图（不可为 NULL）
 * @return GeoResult，data 为 NULL（成功时）
 */
GeoResult geo_unify(const ConstraintGraph *construction, const ConstraintGraph *proposition);

/**
 * @brief 原语 7：打包为函数块
 * @param graph            约束图（不可为 NULL）
 * @param internal_ids     内部节点 ID 数组
 * @param internal_count   内部节点数量
 * @param input_port_ids   输入端口 ID 数组
 * @param input_count      输入端口数量
 * @param output_port_ids  输出端口 ID 数组
 * @param output_count     输出端口数量
 * @return GeoResult，data 指向 int（函数块 ID，调用者需 free）
 */
GeoResult geo_pack(ConstraintGraph *graph, const int *internal_ids, int internal_count, const int *input_port_ids,
                   int input_count, const int *output_port_ids, int output_count);

/**
 * @brief 原语 8：实例化函数块
 * @param graph         约束图（不可为 NULL）
 * @param engine        引擎实例（不可为 NULL，用于访问函数块）
 * @param func_block_id 函数块 ID
 * @param arg_mappings  输入端口映射到实际节点 ID 的数组
 * @param arg_count     映射数量
 * @return GeoResult，data 指向 int*（首元素为长度，后续为新节点 ID；调用者需 free）
 */
GeoResult geo_instantiate(ConstraintGraph *graph, lvEngine *engine, int func_block_id, const int *arg_mappings,
                          int arg_count);

/**
 * @brief 原语 9：执行证明搜索
 * @param nav       证明导航器（不可为 NULL）
 * @param strategy  证明策略索引（参见 ProofStrategyType）
 * @param max_steps 最大搜索步数（0 使用默认值 1000）
 * @return GeoResult，data 为 NULL
 */
GeoResult geo_prove(ProofNavigator *nav, int strategy, int max_steps);

/**
 * @brief 原语 10：导出结果
 * @param nav      证明导航器（不可为 NULL）
 * @param format   导出格式字符串："html", "latex", "coq"
 * @param filepath 输出文件路径
 * @return GeoResult，data 为 NULL
 */
GeoResult geo_export(ProofNavigator *nav, const char *format, const char *filepath);

/**
 * @brief 原语 11：序列化约束图
 * @param graph 约束图（不可为 NULL）
 * @return GeoResult，data 指向 JSON 字符串（调用者需 free）
 */
GeoResult geo_serialize(const ConstraintGraph *graph);

/**
 * @brief 原语 12：反序列化约束图
 * @param json JSON 字符串（不可为 NULL）
 * @return GeoResult，data 指向 ConstraintGraph*（调用者需 graph_destroy）
 */
GeoResult geo_deserialize(const char *json);

/**
 * @brief 原语 13：查询约束图
 * @param graph     约束图（不可为 NULL）
 * @param query     查询类型："node", "constraint", "count"
 * @param target_id 查询目标 ID（节点 ID 或约束 ID；"count" 模式忽略此参数）
 * @return GeoResult，data 指向查询结果（见各查询类型说明）
 *
 * 查询类型说明：
 *   "node"       -> data 指向 GeomNode*（只读，不可 free）
 *   "constraint" -> data 指向 Constraint*（只读，不可 free）
 *   "count"      -> data 指向 int（节点数和约束数，调用者需 free；首元素为节点数，次元素为约束数）
 */
GeoResult geo_query(const ConstraintGraph *graph, const char *query, int target_id);

#ifdef __cplusplus
}
#endif

#endif /* lv_GEOMETRIC_PRIMITIVES_H */
