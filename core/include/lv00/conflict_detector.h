/**
 * @file conflict_detector.h
 * @brief 矛盾约束检测器 —— 几何约束系统的冲突分析与报告
 *
 * @details 提供多层次的矛盾检测能力：
 * - 基础约束冲突：点位置冲突、距离矛盾、角度冲突
 * - 约束组合冲突：共线vs夹角、垂直vs平行、包含vs分离
 * - 传递闭包冲突：通过约束传播推导的间接矛盾
 * - 代数冲突：方程组无解、过约束检测
 *
 * 检测流程：
 * 1. 快速扫描：O(n) 检测明显的局部冲突
 * 2. 组合分析：O(n^2) 检测约束对之间的逻辑冲突
 * 3. 传递闭包：基于图的约束传播检测间接冲突
 * 4. 代数验证：将几何约束转化为方程，检测代数矛盾
 *
 * @version 3.5.0
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
#include "error_codes.h"

/* ================================================================
 * 矛盾类型枚举
 * ================================================================ */

/**
 * @brief 矛盾类型分类
 *
 * 每种矛盾类型对应一种特定的几何约束冲突模式。
 * 用于生成针对性的错误报告和修复建议。
 */
typedef enum {
    /* 基础几何矛盾 */
    CONFLICT_POINT_POSITION,       /**< 同一点被约束到不同位置 */
    CONFLICT_DISTANCE_MISMATCH,    /**< 距离约束与实际距离不符 */
    CONFLICT_ANGLE_MISMATCH,       /**< 角度约束与实际角度不符 */
    
    /* 约束组合矛盾 */
    CONFLICT_COLLINEAR_VS_ANGLE,   /**< 点共线但与夹角约束矛盾 */
    CONFLICT_PERPENDICULAR_VS_PARALLEL, /**< 垂直与平行约束矛盾 */
    CONFLICT_CONTAINMENT_VS_SEPARATION, /**< 包含与分离约束矛盾 */
    CONFLICT_INTERSECTION_VS_PARALLEL,  /**< 相交与平行约束矛盾 */
    
    /* 传递性矛盾 */
    CONFLICT_TRANSITIVE_EQUALITY,  /**< 传递等式矛盾 (A=B, B=C, A≠C) */
    CONFLICT_TRANSITIVE_ORDER,     /**< 传递序关系矛盾 */
    CONFLICT_CYCLIC_DEPENDENCY,    /**< 循环依赖导致的矛盾 */
    
    /* 代数矛盾 */
    CONFLICT_ALGEBRAIC_NO_SOLUTION, /**< 方程组无解 */
    CONFLICT_ALGEBRAIC_OVERCONSTRAINED, /**< 过度约束 */
    CONFLICT_ALGEBRAIC_SINGULAR,    /**< 奇异矩阵 */
    
    /* 其他 */
    CONFLICT_UNKNOWN               /**< 未分类的矛盾 */
} ConflictType;

/**
 * @brief 矛盾严重程度
 */
typedef enum {
    CONFLICT_SEVERITY_WARNING,     /**< 警告：可能导致数值不稳定 */
    CONFLICT_SEVERITY_ERROR,       /**< 错误：约束系统可能无解 */
    CONFLICT_SEVERITY_CRITICAL     /**< 严重：确定存在矛盾 */
} ConflictSeverity;

/* ================================================================
 * 矛盾报告结构
 * ================================================================ */

/**
 * @brief 单个矛盾记录
 *
 * 描述一个检测到的具体矛盾，包括：
 * - 矛盾类型和严重程度
 * - 涉及的节点和约束
 * - 人类可读的描述
 * - 修复建议
 */
typedef struct {
    ConflictType type;              /**< 矛盾类型 */
    ConflictSeverity severity;      /**< 严重程度 */
    
    /* 涉及的实体 */
    int *node_ids;                  /**< 涉及的节点ID数组 */
    int node_count;                 /**< 节点数量 */
    int *constraint_ids;            /**< 涉及的约束ID数组 */
    int constraint_count;           /**< 约束数量 */
    
    /* 描述信息 */
    char *description;              /**< 人类可读的矛盾描述 */
    char *suggestion;               /**< 修复建议 */
    
    /* 数值详情（可选） */
    double expected_value;          /**< 期望值 */
    double actual_value;            /**< 实际值 */
    double tolerance;               /**< 允许的容差 */
} ConflictRecord;

/**
 * @brief 矛盾检测报告
 *
 * 包含对约束图的完整矛盾分析结果。
 */
typedef struct {
    ConflictRecord *conflicts;      /**< 矛盾记录数组 */
    int conflict_count;             /**< 矛盾数量 */
    int capacity;                   /**< 数组容量 */
    
    bool has_critical;              /**< 是否存在严重矛盾 */
    bool has_error;                 /**< 是否存在错误级矛盾 */
    bool has_warning;               /**< 是否存在警告级矛盾 */
    
    /* 统计信息 */
    int by_type[16];                /**< 各类型矛盾计数 */
} ConflictReport;

/* ================================================================
 * 检测器配置
 * ================================================================ */

/**
 * @brief 矛盾检测器配置
 *
 * 控制检测的粒度、性能和精度。
 */
typedef struct {
    /* 检测级别 */
    bool enable_basic_checks;       /**< 启用基础几何检查 */
    bool enable_combination_checks; /**< 启用约束组合检查 */
    bool enable_transitive_checks;  /**< 启用传递闭包检查 */
    bool enable_algebraic_checks;   /**< 启用代数验证 */
    
    /* 性能参数 */
    int max_conflicts;              /**< 最大报告矛盾数（0=无限制） */
    int max_check_time_ms;          /**< 最大检测时间（毫秒，0=无限制） */
    
    /* 精度参数 */
    double position_tolerance;      /**< 位置容差 */
    double distance_tolerance;      /**< 距离容差 */
    double angle_tolerance;         /**< 角度容差（弧度） */
} ConflictDetectorConfig;

/* ================================================================
 * 核心 API
 * ================================================================ */

/**
 * @brief 获取默认检测器配置
 * @return 默认配置（静态常量，不可修改）
 */
const ConflictDetectorConfig *lv00_conflict_detector_default_config(void);

/**
 * @brief 创建矛盾检测报告
 * @return 新分配的报告，失败返回 NULL
 */
ConflictReport *lv00_conflict_report_create(void);

/**
 * @brief 销毁矛盾检测报告
 * @param report 报告指针（可为 NULL）
 */
void lv00_conflict_report_destroy(ConflictReport *report);

/**
 * @brief 清空报告内容（保留容量）
 * @param report 报告指针
 */
void lv00_conflict_report_clear(ConflictReport *report);

/**
 * @brief 执行完整矛盾检测
 *
 * 对约束图执行所有启用的检测级别，收集发现的矛盾。
 *
 * @param graph 约束图
 * @param config 检测配置（NULL 使用默认配置）
 * @param report 输出报告（必须已创建）
 * @return 0 表示检测完成，负数为错误码
 *
 * @note 即使返回 0，report 中也可能包含矛盾记录。
 *       需要通过 report->conflict_count 判断是否存在矛盾。
 */
int lv00_conflict_detect_all(const ConstraintGraph *graph,
                              const ConflictDetectorConfig *config,
                              ConflictReport *report);

/**
 * @brief 快速矛盾检测
 *
 * 仅执行 O(n) 的基础检查，适合频繁调用。
 *
 * @param graph 约束图
 * @return true 表示发现矛盾，false 表示未发现明显矛盾
 */
bool lv00_conflict_detect_quick(const ConstraintGraph *graph);

/**
 * @brief 检测特定节点的约束矛盾
 *
 * @param graph 约束图
 * @param node_id 节点ID
 * @param report 输出报告
 * @return 0 表示检测完成
 */
int lv00_conflict_detect_for_node(const ConstraintGraph *graph,
                                   int node_id,
                                   ConflictReport *report);

/**
 * @brief 检测特定约束是否与其他约束矛盾
 *
 * @param graph 约束图
 * @param constraint_id 约束ID
 * @param report 输出报告
 * @return 0 表示检测完成
 */
int lv00_conflict_detect_for_constraint(const ConstraintGraph *graph,
                                         int constraint_id,
                                         ConflictReport *report);

/* ================================================================
 * 报告输出
 * ================================================================ */

/**
 * @brief 将矛盾报告输出为文本格式
 *
 * @param report 矛盾报告
 * @param output 输出流（如 stdout）
 * @param verbose 是否输出详细信息
 */
void lv00_conflict_report_print(const ConflictReport *report,
                                 void *output,
                                 bool verbose);

/**
 * @brief 将矛盾报告序列化为 JSON
 *
 * @param report 矛盾报告
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 实际写入的字节数（含终止符），失败返回负值
 */
int lv00_conflict_report_to_json(const ConflictReport *report,
                                  char *buffer,
                                  size_t buffer_size);

/* ================================================================
 * 便捷函数
 * ================================================================ */

/**
 * @brief 检查约束图是否存在任何矛盾
 *
 * @param graph 约束图
 * @return true 表示存在矛盾
 */
bool lv00_conflict_graph_has_conflicts(const ConstraintGraph *graph);

/**
 * @brief 获取最严重的矛盾类型
 *
 * @param report 矛盾报告
 * @return 最严重的矛盾类型，无矛盾返回 CONFLICT_UNKNOWN
 */
ConflictType lv00_conflict_get_worst_type(const ConflictReport *report);

/**
 * @brief 获取矛盾的英文类型名称
 *
 * @param type 矛盾类型
 * @return 类型名称字符串（静态常量）
 */
const char *lv00_conflict_type_name(ConflictType type);

/**
 * @brief 获取矛盾严重程度的英文名称
 *
 * @param severity 严重程度
 * @return 严重程度名称（静态常量）
 */
const char *lv00_conflict_severity_name(ConflictSeverity severity);

#ifdef __cplusplus
}
#endif

#endif /* LV00_CONFLICT_DETECTOR_H */
