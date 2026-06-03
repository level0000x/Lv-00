/**
 * @file geo_dynamic.h
 * @brief 动态几何依赖图 —— 借鉴 GeoGebra 动态几何系统
 *
 * 借鉴来源：
 *   - GeoGebra (github.com/geogebra/geogebra)
 *     依赖图架构（父/子元素关系）、级联更新机制、代数-几何联动
 *
 * 设计目标：
 *   - 支持几何对象的父子依赖关系管理
 *   - 实现级联更新（当父对象变化时自动更新子对象）
 *   - 与现有 Layer 3 几何系统无缝集成
 *
 * 版本：v3.6.0（第十三梯队 GeoGebra 动态几何落地）
 */

#ifndef LV00_GEO_DYNAMIC_H
#define LV00_GEO_DYNAMIC_H

#include <stdbool.h>
#include <stdint.h>

#include "lv00.h"

/**
 * @brief 无效节点 ID 常量
 */
#define LV00_DYN_INVALID (-1)

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 第一部分：依赖图节点类型与状态
 * ======================================================================== */

/**
 * @brief 动态几何节点类型
 */
typedef enum {
    LV00_DYN_NODE_POINT,      /**< 自由点（无依赖） */
    LV00_DYN_NODE_LINE,       /**< 直线（依赖两点） */
    LV00_DYN_NODE_CIRCLE,     /**< 圆（依赖圆心和半径点） */
    LV00_DYN_NODE_POLYGON,    /**< 多边形（依赖多个顶点） */
    LV00_DYN_NODE_ANGLE,     /**< 角度（依赖三条边） */
    LV00_DYN_NODE_DISTANCE,   /**< 距离（依赖两个点） */
    LV00_DYN_NODE_MIDPOINT,   /**< 中点（依赖两个点） */
    LV00_DYN_NODE_INTERSECT,  /**< 交点（依赖两条线） */
    LV00_DYN_NODE_PARALLEL,   /**< 平行线（依赖基线和一点） */
    LV00_DYN_NODE_PERPENDICULAR, /**< 垂直线（依赖基线和一点） */
    LV00_DYN_NODE_TANGENT,    /**< 切线（依赖圆和点） */
    LV00_DYN_NODE_BISECTOR    /**< 角平分线（依赖角度） */
} Lv00DynNodeType;

/**
 * @brief 动态几何节点状态
 */
typedef enum {
    LV00_DYN_STATE_VALID,     /**< 有效，已更新 */
    LV00_DYN_STATE_DIRTY,     /**< 脏，需要重新计算 */
    LV00_DYN_STATE_COMPUTING, /**< 正在计算中（检测循环依赖） */
    LV00_DYN_STATE_ERROR      /**< 计算错误 */
} Lv00DynState;

/**
 * @brief 节点标记（用于批量操作）
 */
typedef enum {
    LV00_DYN_MARK_NONE     = 0,      /**< 无标记 */
    LV00_DYN_MARK_VISITED  = 1 << 0, /**< 已访问 */
    LV00_DYN_MARK_UPDATED  = 1 << 1, /**< 已更新 */
    LV00_DYN_MARK_TEMP     = 1 << 2  /**< 临时标记 */
} Lv00DynMark;

/**
 * @brief 动态几何节点
 */
typedef struct Lv00DynNode {
    int id;                         /**< 节点唯一 ID */
    Lv00DynNodeType type;           /**< 节点类型 */
    Lv00DynState state;              /**< 当前状态 */

    /* 依赖关系 */
    int parent_ids[4];              /**< 父节点 ID 数组（最多 4 个依赖） */
    int parent_count;               /**< 实际父节点数量 */

    int child_ids[16];             /**< 子节点 ID 数组（最多 16 个子节点） */
    int child_count;                /**< 实际子节点数量 */

    /* 几何数据（参数化存储） */
    double params[8];               /**< 几何参数 */
    int param_count;                /**< 参数数量 */

    /* 标记位 */
    uint8_t marks;                  /**< 节点标记 */

    /* 统计信息 */
    int update_count;               /**< 更新次数 */
    int64_t last_update_time;       /**< 上次更新时间戳 */
} Lv00DynNode;

/* ========================================================================
 * 第二部分：依赖图数据结构
 * ======================================================================== */

/**
 * @brief 依赖图配置
 */
typedef struct Lv00DynGraphConfig {
    int max_nodes;          /**< 最大节点数 */
    int max_parents;        /**< 每个节点最大父节点数 */
    int max_children;       /**< 每个节点最大子节点数 */
    bool detect_cycles;     /**< 是否检测循环依赖 */
    int max_update_depth;   /**< 最大更新深度（防止无限递归） */
} Lv00DynGraphConfig;

/**
 * @brief 动态几何依赖图
 */
typedef struct Lv00DynGraph {
    Lv00DynNode *nodes;         /**< 节点数组 */
    int *id_to_index;           /**< ID 到索引的映射表 */
    int node_count;             /**< 当前节点数 */
    int node_capacity;           /**< 节点容量 */

    int *parent_adj;            /**< 父节点邻接表（压缩存储） */
    int *parent_adj_offsets;    /**< 父节点邻接表偏移 */
    int *child_adj;             /**< 子节点邻接表（压缩存储） */
    int *child_adj_offsets;     /**< 子节点邻接表偏移 */
    int adj_capacity;            /**< 邻接表容量 */

    Lv00DynGraphConfig config;  /**< 图配置 */

    /* 统计信息 */
    int64_t total_updates;       /**< 总更新次数 */
    int max_update_depth_seen;   /**< 历史最大更新深度 */
} Lv00DynGraph;

/* ========================================================================
 * 第三部分：依赖图操作 API
 * ======================================================================== */

/**
 * @brief 获取默认依赖图配置
 */
LV00_PUBLIC_API Lv00DynGraphConfig lv00_dyn_graph_default_config(void);

/**
 * @brief 创建依赖图
 * @param config 配置（NULL 使用默认配置）
 * @return 依赖图指针（失败返回 NULL）
 */
LV00_PUBLIC_API Lv00DynGraph *lv00_dyn_graph_create(const Lv00DynGraphConfig *config);

/**
 * @brief 释放依赖图
 * @param graph 依赖图指针
 */
LV00_PUBLIC_API void lv00_dyn_graph_free(Lv00DynGraph *graph);

/**
 * @brief 添加节点到依赖图
 * @param graph 依赖图
 * @param type 节点类型
 * @param parent_ids 父节点 ID 数组
 * @param parent_count 父节点数量
 * @param params 几何参数
 * @param param_count 参数数量
 * @return 新节点 ID（-1 表示失败）
 */
LV00_PUBLIC_API int lv00_dyn_graph_add_node(
    Lv00DynGraph *graph,
    Lv00DynNodeType type,
    const int *parent_ids,
    int parent_count,
    const double *params,
    int param_count);

/**
 * @brief 获取节点指针
 * @param graph 依赖图
 * @param node_id 节点 ID
 * @return 节点指针（不存在返回 NULL）
 */
LV00_PUBLIC_API Lv00DynNode *lv00_dyn_graph_get_node(Lv00DynGraph *graph, int node_id);

/**
 * @brief 删除节点及其依赖
 * @param graph 依赖图
 * @param node_id 节点 ID
 * @return 成功返回 true
 */
LV00_PUBLIC_API bool lv00_dyn_graph_remove_node(Lv00DynGraph *graph, int node_id);

/**
 * @brief 获取节点的父节点
 * @param graph 依赖图
 * @param node_id 节点 ID
 * @param out_parents 输出数组
 * @param max_count 最大输出数量
 * @return 实际父节点数量
 */
LV00_PUBLIC_API int lv00_dyn_graph_get_parents(
    const Lv00DynGraph *graph,
    int node_id,
    int *out_parents,
    int max_count);

/**
 * @brief 获取节点的子节点
 * @param graph 依赖图
 * @param node_id 节点 ID
 * @param out_children 输出数组
 * @param max_count 最大输出数量
 * @return 实际子节点数量
 */
LV00_PUBLIC_API int lv00_dyn_graph_get_children(
    const Lv00DynGraph *graph,
    int node_id,
    int *out_children,
    int max_count);

/* ========================================================================
 * 第四部分：级联更新机制
 * ======================================================================== */

/**
 * @brief 更新单个节点
 *
 * 根据节点类型重新计算几何参数。
 * 子类实现应调用此函数前更新父节点。
 *
 * @param graph 依赖图
 * @param node_id 节点 ID
 * @return 成功返回 true
 */
typedef bool (*Lv00DynUpdateFunc)(Lv00DynGraph *graph, int node_id);

/**
 * @brief 级联更新（自顶向下）
 *
 * 从指定节点开始，递归更新所有子孙节点。
 * 用于父节点变化时更新所有依赖它的子节点。
 *
 * @param graph 依赖图
 * @param root_id 根节点 ID
 * @param update_func 更新函数（可为 NULL 使用默认实现）
 * @return 实际更新的节点数（-1 表示循环依赖）
 */
LV00_PUBLIC_API int lv00_dyn_graph_update_cascade(
    Lv00DynGraph *graph,
    int root_id,
    Lv00DynUpdateFunc update_func);

/**
 * @brief 更新单个节点及其父节点链（自底向上）
 *
 * 递归更新到根节点，确保所有依赖都是最新的。
 *
 * @param graph 依赖图
 * @param leaf_id 叶节点 ID
 * @return 实际更新的节点数
 */
LV00_PUBLIC_API int lv00_dyn_graph_update_chain(Lv00DynGraph *graph, int leaf_id);

/**
 * @brief 标记节点为脏
 *
 * 将节点状态设置为 DIRTY，并递归标记所有子节点为 DIRTY。
 *
 * @param graph 依赖图
 * @param node_id 节点 ID
 */
LV00_PUBLIC_API void lv00_dyn_graph_mark_dirty(Lv00DynGraph *graph, int node_id);

/**
 * @brief 批量更新所有脏节点
 *
 * 遍历所有节点，找出 DIRTY 状态的节点并进行更新。
 *
 * @param graph 依赖图
 * @return 更新的节点数
 */
LV00_PUBLIC_API int lv00_dyn_graph_update_all(Lv00DynGraph *graph);

/* ========================================================================
 * 第五部分：循环依赖检测
 * ======================================================================== */

/**
 * @brief 检测是否存在从 start 到 target 的路径
 *
 * 使用 DFS 检测是否有从 start 到 target 的路径（间接依赖）。
 *
 * @param graph 依赖图
 * @param start_id 起始节点
 * @param target_id 目标节点
 * @return 存在路径返回 true
 */
LV00_PUBLIC_API bool lv00_dyn_graph_has_path(
    const Lv00DynGraph *graph,
    int start_id,
    int target_id);

/**
 * @brief 检测添加边是否会形成循环
 *
 * 在添加 parent -> child 边之前检测是否会形成循环。
 *
 * @param graph 依赖图
 * @param parent_id 父节点 ID
 * @param child_id 子节点 ID
 * @return 会形成循环返回 true
 */
LV00_PUBLIC_API bool lv00_dyn_graph_would_create_cycle(
    const Lv00DynGraph *graph,
    int parent_id,
    int child_id);

/**
 * @brief 拓扑排序
 *
 * 对图中所有节点进行拓扑排序。
 * 返回的数组按依赖顺序排列（父节点在子节点之前）。
 *
 * @param graph 依赖图
 * @param out_order 输出数组（需 pre-allocated，大小 >= node_count）
 * @return 排序后的节点数（-1 表示存在循环依赖）
 */
LV00_PUBLIC_API int lv00_dyn_graph_topological_sort(
    const Lv00DynGraph *graph,
    int *out_order);

/* ========================================================================
 * 第六部分：便捷构造函数
 * ======================================================================== */

/**
 * @brief 创建自由点
 * @param graph 依赖图
 * @param x X 坐标
 * @param y Y 坐标
 * @return 节点 ID
 */
LV00_PUBLIC_API int lv00_dyn_create_point(Lv00DynGraph *graph, double x, double y);

/**
 * @brief 创建直线（依赖两个点）
 * @param graph 依赖图
 * @param p1_id 第一个点 ID
 * @param p2_id 第二个点 ID
 * @return 节点 ID
 */
LV00_PUBLIC_API int lv00_dyn_create_line(Lv00DynGraph *graph, int p1_id, int p2_id);

/**
 * @brief 创建圆（依赖圆心和圆上一点）
 * @param graph 依赖图
 * @param center_id 圆心 ID
 * @param point_id 圆上一点 ID
 * @return 节点 ID
 */
LV00_PUBLIC_API int lv00_dyn_create_circle(Lv00DynGraph *graph, int center_id, int point_id);

/**
 * @brief 创建中点
 * @param graph 依赖图
 * @param p1_id 第一个点 ID
 * @param p2_id 第二个点 ID
 * @return 节点 ID
 */
LV00_PUBLIC_API int lv00_dyn_create_midpoint(Lv00DynGraph *graph, int p1_id, int p2_id);

/**
 * @brief 创建平行线
 * @param graph 依赖图
 * @param base_line_id 基线 ID
 * @param through_point_id 通过的点 ID
 * @return 节点 ID
 */
LV00_PUBLIC_API int lv00_dyn_create_parallel(Lv00DynGraph *graph, int base_line_id, int through_point_id);

/**
 * @brief 创建垂直线
 * @param graph 依赖图
 * @param base_line_id 基线 ID
 * @param through_point_id 通过的点 ID
 * @return 节点 ID
 */
LV00_PUBLIC_API int lv00_dyn_create_perpendicular(Lv00DynGraph *graph, int base_line_id, int through_point_id);

/**
 * @brief 创建两点距离
 * @param graph 依赖图
 * @param p1_id 第一个点 ID
 * @param p2_id 第二个点 ID
 * @return 节点 ID
 */
LV00_PUBLIC_API int lv00_dyn_create_distance(Lv00DynGraph *graph, int p1_id, int p2_id);

/* ========================================================================
 * 第七部分：统计与调试
 * ======================================================================== */

/**
 * @brief 获取图统计信息
 */
typedef struct Lv00DynGraphStats {
    int total_nodes;
    int free_nodes;           /**< 无父节点的节点数 */
    int derived_nodes;         /**< 有父节点的节点数 */
    int dirty_nodes;          /**< 脏节点数 */
    int max_children;         /**< 最多子节点的度 */
    int max_parents;          /**< 最多父节点的度 */
    int64_t total_updates;    /**< 总更新次数 */
} Lv00DynGraphStats;

/**
 * @brief 获取图统计信息
 * @param graph 依赖图
 * @param out_stats 输出统计
 */
LV00_PUBLIC_API void lv00_dyn_graph_get_stats(
    const Lv00DynGraph *graph,
    Lv00DynGraphStats *out_stats);

/**
 * @brief 清空所有脏标记
 * @param graph 依赖图
 */
LV00_PUBLIC_API void lv00_dyn_graph_clear_dirty(Lv00DynGraph *graph);

/**
 * @brief 重置所有节点状态为 VALID
 * @param graph 依赖图
 */
LV00_PUBLIC_API void lv00_dyn_graph_reset_states(Lv00DynGraph *graph);

#ifdef __cplusplus
}
#endif

#endif /* LV00_GEO_DYNAMIC_H */
