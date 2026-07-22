/**
 * @file conflict_detector.h
 * @brief 矛盾约束检测器 —— 多层次冲突检测 API
 *
 * @details 提供几何约束系统中矛盾检测的公共接口：
 * - 基础检测：O(n) 扫描明显的局部冲突（结构有效性、点位置、距离矛盾）
 * - 组合检测：O(n^2) 分析约束对之间的逻辑冲突（角度、平行/垂直等）
 * - 传递检测：基于图的约束传播推导间接矛盾（传递等式、循环依赖）
 * - 代数检测：方程组无解、过约束验证
 *
 * @version 1.1.0
 */

#ifndef LV00_CONFLICT_DETECTOR_H
#define LV00_CONFLICT_DETECTOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "constraint_graph.h"

#ifndef LV00_PUBLIC_API
#define LV00_PUBLIC_API
#endif

/* ================================================================
 * 扩展约束类型枚举
 *
 * 这些约束类型是对 ConstraintType 的扩展，用于表示度量约束和
 * 方向约束。在 conflict_detector 模块中统一管理。
 * ================================================================ */

/**
 * @brief 扩展约束类型枚举
 *
 * 包含基础约束类型（与 ConstraintType 重叠）以及度量/方向约束类型。
 * 值从 100 开始以避免与基础 ConstraintType 冲突。
 */
typedef enum {
    /* 度量约束 */
    CONSTRAINT_DISTANCE      = 100, /**< 距离约束：两实体间距离为指定值 */
    CONSTRAINT_ANGLE         = 101, /**< 角度约束：两实体间角度为指定值 */
    CONSTRAINT_COINCIDENT    = 102, /**< 重合约束：两实体位置相同 */

    /* 方向约束 */
    CONSTRAINT_PARALLEL      = 110, /**< 平行约束：两线段平行 */
    CONSTRAINT_PERPENDICULAR = 111, /**< 垂直约束：两线段垂直 */
    CONSTRAINT_HORIZONTAL    = 112, /**< 水平约束：线段水平 */
    CONSTRAINT_VERTICAL      = 113  /**< 垂直约束：线段垂直 */
} ExtendedConstraintType;

/* ================================================================
 * 冲突类型枚举
 * ================================================================ */

/**
 * @brief 冲突类型枚举
 *
 * 标识检测到的矛盾约束的具体类型。
 */
typedef enum {
    CONFLICT_POINT_POSITION            = 0,  /**< 点位置冲突 */
    CONFLICT_DISTANCE_MISMATCH        = 1,  /**< 距离不匹配 */
    CONFLICT_ANGLE_MISMATCH           = 2,  /**< 角度不匹配 */
    CONFLICT_COLLINEAR_VS_ANGLE       = 3,  /**< 共线与角度矛盾 */
    CONFLICT_PERPENDICULAR_VS_PARALLEL = 4, /**< 垂直与平行矛盾 */
    CONFLICT_CONTAINMENT_VS_SEPARATION = 5,  /**< 包含与分离矛盾 */
    CONFLICT_INTERSECTION_VS_PARALLEL  = 6,  /**< 相交与平行矛盾 */
    CONFLICT_TRANSITIVE_EQUALITY       = 7,  /**< 传递等式矛盾 */
    CONFLICT_TRANSITIVE_ORDER          = 8,  /**< 传递序矛盾 */
    CONFLICT_CYCLIC_DEPENDENCY         = 9,  /**< 循环依赖 */
    CONFLICT_ALGEBRAIC_NO_SOLUTION     = 10, /**< 代数无解 */
    CONFLICT_ALGEBRAIC_OVERCONSTRAINED = 11, /**< 代数过约束 */
    CONFLICT_ALGEBRAIC_SINGULAR        = 12, /**< 代数奇异矩阵 */
    CONFLICT_UNKNOWN                   = 15  /**< 未知冲突类型 */
} ConflictType;

/* ================================================================
 * 冲突严重程度枚举
 * ================================================================ */

/**
 * @brief 冲突严重程度枚举
 */
typedef enum {
    CONFLICT_SEVERITY_WARNING  = 0, /**< 警告：可能的问题，不阻止求解 */
    CONFLICT_SEVERITY_ERROR    = 1, /**< 错误：确定的矛盾，阻止正确求解 */
    CONFLICT_SEVERITY_CRITICAL = 2  /**< 严重：系统级矛盾，必须立即修复 */
} ConflictSeverity;

/* ================================================================
 * 冲突记录
 * ================================================================ */

/**
 * @brief 冲突记录 —— 描述单个检测到的矛盾
 */
typedef struct {
    ConflictType type;         /**< 冲突类型 */
    ConflictSeverity severity;  /**< 严重程度 */
    char *description;        /**< 冲突描述（动态分配） */
    char *suggestion;          /**< 修复建议（动态分配） */
    int *node_ids;             /**< 相关节点 ID 数组（动态分配） */
    int node_count;            /**< 相关节点数量 */
    int *constraint_ids;       /**< 相关约束 ID 数组（动态分配） */
    int constraint_count;     /**< 相关约束数量 */
} ConflictRecord;

/* ================================================================
 * 冲突报告
 * ================================================================ */

/**
 * @brief 冲突报告 —— 存储一次检测的所有冲突结果
 */
typedef struct {
    ConflictRecord *conflicts;      /**< 冲突记录数组（动态分配） */
    int conflict_count;             /**< 冲突总数 */
    int capacity;                   /**< 数组容量 */

    bool has_critical;              /**< 是否包含严重冲突 */
    bool has_error;                 /**< 是否包含错误级冲突 */
    bool has_warning;               /**< 是否包含警告级冲突 */

    int by_type[16];               /**< 按类型统计的冲突计数 */
} ConflictReport;

/* ================================================================
 * 检测器配置
 * ================================================================ */

/**
 * @brief 冲突检测器配置
 *
 * 控制检测行为、容差和资源限制。
 */
typedef struct {
    bool enable_basic_checks;       /**< 启用基础检测（结构、点位置、距离） */
    bool enable_combination_checks; /**< 启用组合检测（约束对逻辑冲突） */
    bool enable_transitive_checks;  /**< 启用传递检测（传递等式、循环依赖） */
    bool enable_algebraic_checks;   /**< 启用代数检测（方程组无解、过约束） */

    int max_conflicts;              /**< 最大报告冲突数（0=无限制） */
    int max_check_time_ms;          /**< 最大检测时间（毫秒，0=无限制） */

    double position_tolerance;      /**< 位置容差（默认 1e-9） */
    double distance_tolerance;      /**< 距离容差（默认 1e-9） */
    double angle_tolerance;         /**< 角度容差（弧度，默认 1e-6） */
} ConflictDetectorConfig;

/* ================================================================
 * 默认配置
 * ================================================================ */

/**
 * @brief 获取默认检测器配置
 * @return 指向静态默认配置的指针（只读，不要修改）
 */
LV00_PUBLIC_API const ConflictDetectorConfig *lv00_conflict_detector_default_config(void);

/* ================================================================
 * 报告管理
 * ================================================================ */

/**
 * @brief 创建冲突报告
 * @return 新创建的报告指针，失败返回 NULL
 */
LV00_PUBLIC_API ConflictReport *lv00_conflict_report_create(void);

/**
 * @brief 销毁冲突报告及所有关联的动态内存
 * @param report 要销毁的报告
 */
LV00_PUBLIC_API void lv00_conflict_report_destroy(ConflictReport *report);

/**
 * @brief 清空报告内容（保留数组容量）
 * @param report 要清空的报告
 */
LV00_PUBLIC_API void lv00_conflict_report_clear(ConflictReport *report);

/* ================================================================
 * 检测 API
 * ================================================================ */

/**
 * @brief 执行全量冲突检测
 *
 * 依次执行基础、组合、传递和代数检测（根据配置启用/禁用）。
 *
 * @param graph  约束图
 * @param config 检测配置（NULL 使用默认配置）
 * @param report 输出报告（会被清空后填充）
 * @return 0 成功，非零错误码
 */
LV00_PUBLIC_API int lv00_conflict_detect_all(const ConstraintGraph *graph,
                                              const ConflictDetectorConfig *config,
                                              ConflictReport *report);

/**
 * @brief 快速冲突检测（仅基础检测）
 * @param graph 约束图
 * @return true 表示存在冲突
 */
LV00_PUBLIC_API bool lv00_conflict_detect_quick(const ConstraintGraph *graph);

/**
 * @brief 针对特定节点检测冲突
 * @param graph   约束图
 * @param node_id 目标节点 ID
 * @param report  输出报告
 * @return 0 成功，非零错误码
 */
LV00_PUBLIC_API int lv00_conflict_detect_for_node(const ConstraintGraph *graph,
                                                   int node_id,
                                                   ConflictReport *report);

/**
 * @brief 针对特定约束检测冲突
 * @param graph         约束图
 * @param constraint_id 目标约束 ID
 * @param report        输出报告
 * @return 0 成功，非零错误码
 */
LV00_PUBLIC_API int lv00_conflict_detect_for_constraint(const ConstraintGraph *graph,
                                                         int constraint_id,
                                                         ConflictReport *report);

/* ================================================================
 * 类型名称查询
 * ================================================================ */

/**
 * @brief 获取冲突类型名称
 * @param type 冲突类型
 * @return 类型名称字符串
 */
LV00_PUBLIC_API const char *lv00_conflict_type_name(ConflictType type);

/**
 * @brief 获取冲突严重程度名称
 * @param severity 严重程度
 * @return 严重程度名称字符串
 */
LV00_PUBLIC_API const char *lv00_conflict_severity_name(ConflictSeverity severity);

/* ================================================================
 * 报告输出
 * ================================================================ */

/**
 * @brief 打印冲突报告到输出流
 * @param report  冲突报告
 * @param output 输出流（NULL 使用 stdout）
 * @param verbose 是否输出详细信息（建议、节点列表）
 */
LV00_PUBLIC_API void lv00_conflict_report_print(const ConflictReport *report,
                                                 void *output,
                                                 bool verbose);

/**
 * @brief 将冲突报告序列化为 JSON
 * @param report      冲突报告
 * @param buffer      输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 写入的字节数（不含终止符），失败返回 -1
 */
LV00_PUBLIC_API int lv00_conflict_report_to_json(const ConflictReport *report,
                                                  char *buffer,
                                                  size_t buffer_size);

/* ================================================================
 * 便捷函数
 * ================================================================ */

/**
 * @brief 检查约束图中是否存在冲突（快速检测的便捷包装）
 * @param graph 约束图
 * @return true 表示存在冲突
 */
LV00_PUBLIC_API bool lv00_conflict_graph_has_conflicts(const ConstraintGraph *graph);

/**
 * @brief 获取报告中最高严重程度的冲突类型
 * @param report 冲突报告
 * @return 最严重的冲突类型，无冲突返回 CONFLICT_UNKNOWN
 */
LV00_PUBLIC_API ConflictType lv00_conflict_get_worst_type(const ConflictReport *report);

#ifdef __cplusplus
}
#endif

#endif /* LV00_CONFLICT_DETECTOR_H */
