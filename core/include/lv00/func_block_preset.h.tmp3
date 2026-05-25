/* ========================================================================
 * 模块名称：预设函数块系统 (func_block_preset)
 * 功能概述：提供理论数学研究的标准化函数块库，包含完整的预设函数块
 *          定义、参数规范和实例化接口。所有预设都有明确的输入/输出
 *          类型签名，支持函数式组合和运行时注册自定义预设。
 *
 * 主要 API：
 *   - func_block_preset_library_init      — 初始化预设库
 *   - func_block_preset_instantiate        — 实例化预设函数块
 *   - func_block_preset_validate_types     — 验证参数类型
 *   - func_block_preset_compose            — 组合两个预设
 *   - func_block_preset_register_custom    — 注册自定义预设
 *   - func_block_preset_generate_doc       — 生成预设文档
 *
 * 使用示例：
 LV00_PUBLIC_API *   func_block_preset_library_init();
 *   FuncBlock *fb;
 *   InstantiateResult r = func_block_preset_instantiate(
 *       "midpoint", input_ids, 2, graph, &fb);
 *
 * @version 3.3.0
 * ======================================================================== */

/**
 * @file func_block_preset.h
 * @brief 预设函数块系统 - 理论数学研究的标准化函数块库
 */

#ifndef LV00_FUNC_BLOCK_PRESET_H
#define LV00_FUNC_BLOCK_PRESET_H

#include <stdbool.h>

#include "func_block.h"
#include "func_block_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设函数块参数类型系统
 * ============================================================ */

/**
 * @brief 几何参数类型枚举
 *
 * 定义预设函数块接受的参数类型，用于类型检查和验证。
 */
typedef enum {
    PARAM_TYPE_POINT,   /**< 点 */
    PARAM_TYPE_LINE,    /**< 直线（无限延伸） */
    PARAM_TYPE_SEGMENT, /**< 线段 */
    PARAM_TYPE_RAY,     /**< 射线 */
    PARAM_TYPE_CIRCLE,  /**< 圆 */
    PARAM_TYPE_ARC,     /**< 圆弧 */
    PARAM_TYPE_POLYGON, /**< 多边形 */
    PARAM_TYPE_REGION,  /**< 区域 */
    PARAM_TYPE_ANGLE,   /**< 角度 */
    PARAM_TYPE_VECTOR,  /**< 向量 */
    PARAM_TYPE_SCALAR,  /**< 标量（数值） */
    PARAM_TYPE_BOOLEAN, /**< 布尔值 */
    PARAM_TYPE_CURVE,   /**< 曲线 */
    PARAM_TYPE_SURFACE, /**< 曲面 */
    PARAM_TYPE_ANY,     /**< 任意类型（多态） */
    PARAM_TYPE_VARIADIC /**< 可变参数 */
} PresetParamType;

/**
 * @brief 参数约束类型
 */
typedef enum {
    PARAM_CONSTRAINT_NONE,          /**< 无约束 */
    PARAM_CONSTRAINT_NON_COLLINEAR, /**< 非共线 */
    PARAM_CONSTRAINT_NON_COPLANAR,  /**< 非共面 */
    PARAM_CONSTRAINT_DISTINCT,      /**< 互不相同 */
    PARAM_CONSTRAINT_POSITIVE,      /**< 正值 */
    PARAM_CONSTRAINT_NON_ZERO,      /**< 非零 */
    PARAM_CONSTRAINT_UNIT,          /**< 单位长度 */
    PARAM_CONSTRAINT_IN_RANGE       /**< 在范围内 */
} ParamConstraintType;

/**
 * @brief 参数约束定义
 */
typedef struct {
    ParamConstraintType type; /**< 约束类型 */
    double min_val;           /**< 最小值（用于IN_RANGE） */
    double max_val;           /**< 最大值（用于IN_RANGE） */
    const char *description;  /**< 约束描述 */
} ParamConstraint;

/**
 * @brief 预设函数块参数定义
 */
typedef struct {
    const char *name;             /**< 参数名称 */
    const char *description;      /**< 参数描述 */
    PresetParamType type;         /**< 参数类型 */
    bool is_optional;             /**< 是否可选 */
    bool is_output;               /**< 是否为输出参数 */
    int index;                    /**< 参数索引 */
    ParamConstraint *constraints; /**< 约束数组 */
    int constraint_count;         /**< 约束数量 */
} PresetParamDef;

/* ============================================================
 * 预设函数块元数据
 * ============================================================ */

/**
 * @brief 预设函数块数学性质
 */
typedef enum {
    PRESET_PROPERTY_NONE = 0,
    PRESET_PROPERTY_IDEMPOTENT = 1 << 0,    /**< 幂等性: f(f(x)) = f(x) */
    PRESET_PROPERTY_INVOLUTIVE = 1 << 1,    /**< 对合性: f(f(x)) = x */
    PRESET_PROPERTY_COMMUTATIVE = 1 << 2,   /**< 交换性: f(a,b) = f(b,a) */
    PRESET_PROPERTY_ASSOCIATIVE = 1 << 3,   /**< 结合性 */
    PRESET_PROPERTY_LINEAR = 1 << 4,        /**< 线性 */
    PRESET_PROPERTY_CONTINUOUS = 1 << 5,    /**< 连续性 */
    PRESET_PROPERTY_DETERMINISTIC = 1 << 6, /**< 确定性 */
    PRESET_PROPERTY_CONSTRUCTIVE = 1 << 7,  /**< 构造性 */
    PRESET_PROPERTY_REVERSIBLE = 1 << 8     /**< 可逆性 */
} PresetProperty;

/**
 * @brief 预设函数块复杂度等级
 */
typedef enum {
    COMPLEXITY_O1,     /**< 常数时间 */
    COMPLEXITY_OLOGN,  /**< 对数时间 */
    COMPLEXITY_ON,     /**< 线性时间 */
    COMPLEXITY_ONLOGN, /**< 线性对数 */
    COMPLEXITY_ON2,    /**< 平方时间 */
    COMPLEXITY_ON3,    /**< 立方时间 */
    COMPLEXITY_UNKNOWN /**< 未知 */
} PresetComplexity;

/**
 * @brief 预设函数块完整元数据
 */
typedef struct {
    const char *name;             /**< 预设名称 */
    const char *description;      /**< 描述 */
    const char *mathematical_def; /**< 数学定义（LaTeX格式） */
    PresetCategory category;      /**< 类别 */
    PresetProperty properties;    /**< 数学性质位掩码 */
    PresetComplexity complexity;  /**< 复杂度 */

    /* 参数定义 */
    PresetParamDef *input_params;  /**< 输入参数数组 */
    int input_count;               /**< 输入参数数量 */
    PresetParamDef *output_params; /**< 输出参数数组 */
    int output_count;              /**< 输出参数数量 */

    /* 前置条件 */
    const char **preconditions; /**< 前置条件描述数组 */
    int precondition_count;     /**< 前置条件数量 */

    /* 后置条件 */
    const char **postconditions; /**< 后置条件描述数组 */
    int postcondition_count;     /**< 后置条件数量 */

    /* 相关预设 */
    const char **related_presets; /**< 相关预设名称数组 */
    int related_count;            /**< 相关预设数量 */

    /* 版本信息 */
    int version_major; /**< 主版本 */
    int version_minor; /**< 次版本 */
    int version_patch; /**< 补丁版本 */
} PresetMetadata;

/* ============================================================
 * 预设函数块实例化上下文
 * ============================================================ */

/**
 * @brief 实例化选项
 */
typedef struct {
    bool auto_resolve_ambiguity;        /**< 自动解决歧义 */
    bool validate_constraints;          /**< 验证约束 */
    bool add_to_graph;                  /**< 自动添加到约束图 */
    int max_solutions;                  /**< 最大解数量 */
    SolutionSelector *default_selector; /**< 默认选择器 */
} InstantiateOptions;

/**
 * @brief 实例化结果详情
 */
typedef struct {
    InstantiateResult result; /**< 结果状态 */
    FuncBlock *func_block;    /**< 创建的函数块 */
    int *output_node_ids;     /**< 输出节点ID数组 */
    int output_count;         /**< 输出数量 */
    char **warnings;          /**< 警告信息数组 */
    int warning_count;        /**< 警告数量 */
    char *error_detail;       /**< 详细错误信息 */
} InstantiateDetails;

/* ============================================================
 * 预设函数块库 API
 * ============================================================ */

/**
 * @brief 初始化预设函数块库
 *
 * 初始化全局预设注册表，加载所有内置预设函数块。
 * 幂等操作：多次调用只执行一次初始化。
 *
 * @return true 初始化成功，false 失败
 */
LV00_PUBLIC_API bool func_block_preset_library_init(void);

/**
 * @brief 清理预设函数块库
 *
 * 释放所有预设函数块资源，清理全局注册表。
 */
LV00_PUBLIC_API void func_block_preset_library_cleanup(void);

/**
 * @brief 获取预设函数块元数据
 *
 * @param preset_name 预设名称
 * @return 元数据指针（只读，无需释放），未找到返回NULL
 */
LV00_PUBLIC_API const PresetMetadata *func_block_preset_get_metadata(const char *preset_name);

/**
 * @brief 实例化预设函数块（简化版）
 *
 * 根据预设名称和输入参数创建函数块实例。
 *
 * @param preset_name 预设名称
 * @param input_node_ids 输入节点ID数组
 * @param input_count 输入数量
 * @param graph 约束图
 * @param out_func_block 输出函数块
 * @return 实例化结果
 */
LV00_PUBLIC_API InstantiateResult func_block_preset_instantiate(const char *preset_name, const int *input_node_ids, int input_count,
                                                ConstraintGraph *graph, FuncBlock **out_func_block);

/**
 * @brief 实例化预设函数块（完整版）
 *
 * 支持完整选项和获取详细结果。
 *
 * @param preset_name 预设名称
 * @param input_node_ids 输入节点ID数组
 * @param input_count 输入数量
 * @param graph 约束图
 * @param options 实例化选项（可为NULL，使用默认）
 * @param out_details 输出详细信息（调用者负责释放warnings和error_detail）
 * @return 实例化结果状态
 */
LV00_PUBLIC_API InstantiateResult func_block_preset_instantiate_ex(const char *preset_name, const int *input_node_ids, int input_count,
                                                   ConstraintGraph *graph, const InstantiateOptions *options,
                                                   InstantiateDetails *out_details);

/**
 * @brief 释放实例化详情
 *
 * @param details 实例化详情
 */
LV00_PUBLIC_API void func_block_preset_free_details(InstantiateDetails *details);

/**
 * @brief 验证参数类型
 *
 * 检查输入节点类型是否与预设要求的参数类型匹配。
 *
 * @param preset_name 预设名称
 * @param input_nodes 输入节点数组
 * @param input_count 输入数量
 * @param out_mismatch_index 输出第一个不匹配的参数索引（可为NULL）
 * @return true 类型匹配，false 类型不匹配
 */
LV00_PUBLIC_API bool func_block_preset_validate_types(const char *preset_name, GeomNode **input_nodes, int input_count,
                                      int *out_mismatch_index);

/**
 * @brief 验证参数约束
 *
 * 检查输入参数是否满足预设定义的约束条件。
 *
 * @param preset_name 预设名称
 * @param graph 约束图
 * @param input_node_ids 输入节点ID数组
 * @param input_count 输入数量
 * @param out_violated_constraint 输出违反的约束描述（可为NULL）
 * @return true 约束满足，false 约束违反
 */
LV00_PUBLIC_API bool func_block_preset_validate_constraints(const char *preset_name, ConstraintGraph *graph, const int *input_node_ids,
                                            int input_count, const char **out_violated_constraint);

/**
 * @brief 获取预设输入参数数量
 *
 * @param preset_name 预设名称
 * @return 输入参数数量，未找到返回-1
 */
LV00_PUBLIC_API int func_block_preset_get_input_count(const char *preset_name);

/**
 * @brief 获取预设输出参数数量
 *
 * @param preset_name 预设名称
 * @return 输出参数数量，未找到返回-1
 */
LV00_PUBLIC_API int func_block_preset_get_output_count(const char *preset_name);

/**
 * @brief 列出所有可用预设
 *
 * @param out_names 输出名称数组（调用者分配，元素为const char*）
 * @param max_count 最大数量
 * @param category 类别筛选（PRESET_CATEGORY_CONSTRUCTION等，-1表示全部）
 * @return 实际预设数量（可能超过max_count）
 */
LV00_PUBLIC_API int func_block_preset_list(const char **out_names, int max_count, PresetCategory category);

/**
 * @brief 检查预设是否存在
 *
 * @param preset_name 预设名称
 * @return true 存在，false 不存在
 */
LV00_PUBLIC_API bool func_block_preset_exists(const char *preset_name);

/**
 * @brief 获取当前已注册的预设函数块总数
 * @return 已注册的预设数量
 */
LV00_PUBLIC_API int func_block_preset_count(void);

/**
 * @brief 注销指定名称的预设函数块
 * @param name 要注销的预设名称
 * @return 0 成功，-1 未找到或内置预设不可注销
 */
LV00_PUBLIC_API int func_block_preset_unregister(const char *name);

/**
 * @brief 获取预设类别字符串
 *
 * @param category 类别枚举
 * @return 类别名称字符串（静态存储）
 */
LV00_PUBLIC_API const char *func_block_preset_category_string(PresetCategory category);

/**
 * @brief 获取参数类型字符串
 *
 * @param type 参数类型
 * @return 类型名称字符串（静态存储）
 */
LV00_PUBLIC_API const char *func_block_preset_param_type_string(PresetParamType type);

/**
 * @brief 获取复杂度字符串
 *
 * @param complexity 复杂度等级
 * @return 复杂度描述字符串（静态存储）
 */
LV00_PUBLIC_API const char *func_block_preset_complexity_string(PresetComplexity complexity);

/**
 * @brief 获取性质字符串列表
 *
 * @param properties 性质位掩码
 * @param out_buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 实际写入的字符数
 */
LV00_PUBLIC_API int func_block_preset_properties_string(PresetProperty properties, char *out_buffer, size_t buffer_size);

/* ============================================================
 * 高级预设操作
 * ============================================================ */

/**
 * @brief 组合两个预设（函数复合）
 *
 * 创建一个新的预设，表示 f ∘ g（先执行g，再执行f）。
 *
 * @param f_name 第一个预设名称
 * @param g_name 第二个预设名称
 * @param new_preset_name 新预设名称
 * @return true 组合成功，false 失败
 */
LV00_PUBLIC_API bool func_block_preset_compose(const char *f_name, const char *g_name, const char *new_preset_name);

/**
 * @brief 创建预设的偏应用
 *
 * 固定部分参数，创建一个新的预设。
 *
 * @param preset_name 原预设名称
 * @param fixed_param_indices 固定参数索引数组
 * @param fixed_count 固定参数数量
 * @param new_preset_name 新预设名称
 * @return true 成功，false 失败
 */
LV00_PUBLIC_API bool func_block_preset_partial(const char *preset_name, const int *fixed_param_indices, int fixed_count,
                               const char *new_preset_name);

/**
 * @brief 获取预设的逆（如果存在）
 *
 * @param preset_name 预设名称
 * @return 逆预设名称，不存在返回NULL
 */
LV00_PUBLIC_API const char *func_block_preset_get_inverse(const char *preset_name);

/**
 * @brief 注册自定义预设
 *
 * 将用户定义的函数块注册为新的预设。
 *
 * @param metadata 预设元数据（将被复制）
 * @param template_fb 模板函数块（将被复制）
 * @return true 注册成功，false 失败
 */
LV00_PUBLIC_API bool func_block_preset_register_custom(const PresetMetadata *metadata, const FuncBlock *template_fb);

/* ============================================================
 * 预设函数块文档生成
 * ============================================================ */

/**
 * @brief 生成预设的Markdown文档
 *
 * @param preset_name 预设名称
 * @param out_buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 实际需要的缓冲区大小（包括\0），失败返回0
 */
LV00_PUBLIC_API size_t func_block_preset_generate_doc(const char *preset_name, char *out_buffer, size_t buffer_size);

/**
 * @brief 生成所有预设的文档索引
 *
 * @param out_buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 实际需要的缓冲区大小（包括\0），失败返回0
 */
LV00_PUBLIC_API size_t func_block_preset_generate_index(char *out_buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* LV00_FUNC_BLOCK_PRESET_H */
