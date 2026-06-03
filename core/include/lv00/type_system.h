/**
 * @file type_system.h
 * @brief 类型系统 - 宇宙层级、类型等价检查、类型推断
 *
 * 根据 Lv-00 设计文档第12节实现：
 * - 宇宙层级机制
 * - 累积性
 * - 非良基模式
 * - 多态类型
 * - 依赖类型
 * - 类型等价检查
 * - 类型推断
 *
 * 【中文模块说明】
 * type_system.h 是 Lv-00 系统的类型理论实现模块，提供类似 Martin-Lof
 * 类型论的类型系统。主要功能包括：
 * - 宇宙层级：使用整数表示的无限层级体系（第0层：基本几何体，第1层：类型区域）
 * - 类型种类：点、线段、区域、函数、乘积、和、类型变量、依赖类型、底部类型
 * - 类型等价检查：支持直接比较和基于重写引擎的规范化等价检查
 * - 类型推断：规则表驱动的自动类型推断，支持自定义规则注册
 * - 类型变量实例化：多态类型的变量替换和绑定
 * - 非良基模式：循环类型检测和非良基相容性检查
 * - 依赖类型检查：对依赖函数类型 Π(x:A).B(x) 的兼容性验证
 * - 路径探索器：交互式类型重写路径搜索（查找、预览、应用、撤销规则）
 * - 重写路径记录：记录类型重写历史，支持回放到指定步骤
 * - 节点-类型映射：外部映射表将类型区域附加到几何节点
 */

#ifndef LV00_TYPE_SYSTEM_H
#define LV00_TYPE_SYSTEM_H

#include <stdbool.h>

#include "constraint_graph.h"
#include "rewrite.h"
#include "stream.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============== 流式输出上下文设置 ============== */

/**
 * @brief 设置类型系统的流式输出上下文
 * @param ctx  流式上下文（可为 NULL 以禁用流式输出）
 */
LV00_PUBLIC_API void type_system_set_stream_context(StreamContext *ctx);

/* ============== 前向声明 ============== */
typedef struct TypeRegion TypeRegion;
typedef struct TypeVariable TypeVariable;
typedef struct TypeSystem TypeSystem;

/* ============== 宇宙层级 ============== */
/* 宇宙层级使用整数表示，支持无限层级 */
typedef int UniverseLevel;

/* 常用层级常量 */
#define UNIVERSE_BASE 0   /* 第0层：基本几何体（点、线段） */
#define UNIVERSE_TYPE_1 1 /* 第1层：类型区域 */

/* ============== 类型种类 ============== */
typedef enum {
    TYPE_KIND_POINT,        /* 点类型 */
    TYPE_KIND_LINE_SEGMENT, /* 线段类型 */
    TYPE_KIND_REGION,       /* 区域类型 */
    TYPE_KIND_FUNCTION,     /* 函数类型 */
    TYPE_KIND_PRODUCT,      /* 乘积类型 */
    TYPE_KIND_SUM,          /* 和类型 */
    TYPE_KIND_VARIABLE,     /* 类型变量（多态） */
    TYPE_KIND_DEPENDENT,    /* 依赖类型 */
    TYPE_KIND_BOTTOM,       /* ⊥ 类型 */
    TYPE_KIND_PREDICATE_SUBTYPE /* PVS风格谓词子类型 {x:T | P(x)} */
} TypeKind;

/* ============== 类型区域 ============== */
/**
 * @struct TypeRegion
 * @brief 类型区域 —— 表示类型系统中的一个类型实体
 *
 * 【设计说明：联合体风格平铺字段】
 * TypeRegion 采用"联合体风格平铺字段"的设计，而非嵌套 union 或子结构体指针。
 * 所有类型种类共享同一结构体，不同种类使用不同的字段子集。这样设计的原因：
 *   1. 避免嵌套指针：减少间接寻址层级，提升缓存局部性
 *   2. 减少内存分配：无需为每种类型单独分配子结构体
 *   3. 简化内存管理：销毁时只需一个 type_region_destroy() 调用
 *   4. 统一布局：所有类型区域在内存中具有相同的布局，便于数组存储
 *
 * 【各类型种类使用的字段组合】
 *   - TYPE_KIND_POINT / TYPE_KIND_LINE_SEGMENT / TYPE_KIND_BOTTOM:
 *       仅使用 id, kind, level；其余字段均未使用
 *   - TYPE_KIND_REGION:
 *       id, kind, level + contained_node_ids, contained_count
 *   - TYPE_KIND_FUNCTION:
 *       id, kind, level + input_type, output_type
 *   - TYPE_KIND_PRODUCT:
 *       id, kind, level + left_type, right_type
 *   - TYPE_KIND_SUM:
 *       id, kind, level + first_type, second_type
 *   - TYPE_KIND_VARIABLE:
 *       id, kind, level + variable_id, variable_name
 *   - TYPE_KIND_DEPENDENT:
 *       id, kind, level + param_node_id, body_type
 *   - 通用字段（所有类型均可使用）:
 *       alias_name, aliased_type（类型别名）, constraint_ids, constraint_count（约束条件）
 *
 * 【安全访问建议】
 * 访问 TypeRegion 的特定字段前，必须先检查 kind 字段以确定当前类型种类。
 * 例如：
 *   if (tr->kind == TYPE_KIND_FUNCTION) {
 *       // 此时可以安全访问 tr->input_type 和 tr->output_type
 *   }
 * 不要对不匹配的种类访问对应字段，否则可能读到未初始化的值或悬空指针。
 *
 * 【警告：未使用字段为未初始化值】
 * 对于当前 kind 不涉及的字段，其值是未初始化的（创建时通常为零初始化，
 * 但后续操作可能不会维护这些字段）。依赖未使用字段的值会导致未定义行为。
 * 始终根据 kind 字段判断哪些字段是有效的。
 */
struct TypeRegion {
    int id;              /* 类型区域ID */
    TypeKind kind;       /* 类型种类 */
    UniverseLevel level; /* 宇宙层级 */

    /* 对于区域类型 */
    int *contained_node_ids; /* 包含的节点ID */
    int contained_count;     /* 包含的节点数量 */

    /* 对于函数类型 */
    TypeRegion *input_type;  /* 输入类型 */
    TypeRegion *output_type; /* 输出类型 */

    /* 对于乘积类型 */
    TypeRegion *left_type;  /* 左类型 */
    TypeRegion *right_type; /* 右类型 */

    /* 对于和类型 */
    TypeRegion *first_type;  /* 第一类型 */
    TypeRegion *second_type; /* 第二类型 */

    /* 对于类型变量 */
    int variable_id;     /* 变量ID */
    char *variable_name; /* 变量名称 */

    /* 对于依赖类型 */
    int param_node_id;     /* 参数节点ID */
    TypeRegion *body_type; /* 体类型 */

    /* 对于PVS风格谓词子类型 {x:base_type | predicate} */
    TypeRegion *base_type;      /* 基类型 T */
    char *predicate_name;       /* 谓词名称 P */
    char *predicate_expr;       /* 谓词表达式字符串 */
    int predicate_constraint_id; /* 关联的约束ID（用于验证） */

    /* 类型别名 */
    char *alias_name;         /* 别名名称 */
    TypeRegion *aliased_type; /* 被别名的类型 */

    /* 约束条件 */
    int *constraint_ids;  /* 约束ID数组 */
    int constraint_count; /* 约束数量 */
};

/* ============== 类型变量 ============== */
struct TypeVariable {
    int id;                 /* 变量ID */
    char *name;             /* 变量名称 */
    TypeRegion *bound_type; /* 绑定的类型（若已实例化） */
    bool is_polymorphic;    /* 是否为多态变量 */
};

/* ============== 类型等价检查结果 ============== */
typedef enum {
    TYPE_EQUIV_OK,               /* 类型等价 */
    TYPE_EQUIV_NOT_EQUIV,        /* 类型不等价 */
    TYPE_EQUIV_UNKNOWN,          /* 未能证明等价 */
    TYPE_EQUIV_ERROR,            /* 检查出错 */
    TYPE_EQUIV_NEEDS_INTERACTION /* 需要交互式证明（重写引擎无法归一化） */
} TypeEquivResult;

/* ============== 类型检查结果 ============== */
typedef enum {
    TYPE_CHECK_OK,           /* 类型检查通过 */
    TYPE_CHECK_MISMATCH,     /* 类型不匹配 */
    TYPE_CHECK_INCOMPATIBLE, /* 类型不兼容（无法证明等价） */
    TYPE_CHECK_LEVEL_ERROR,  /* 宇宙层级错误 */
    TYPE_CHECK_CYCLE,        /* 类型循环 */
    TYPE_CHECK_INFERRED,     /* 类型已推断 */
    TYPE_CHECK_ERROR         /* 检查出错 */
} TypeCheckResult;

/* ============== 类型推断规则 ============== */

/**
 * @brief 类型推断规则
 *
 * 定义一条从几何节点类型到类型种类的推断规则。
 * 规则按优先级排序，优先级数值越小越优先。
 */
typedef struct {
    int source_node_type;    /* 源节点的几何类型（如 GEOM_POINT） */
    int target_type_kind;    /* 要推断的类型种类（如 TYPE_KIND_POINT） */
    int priority;            /* 规则优先级（数值越小优先级越高） */
    const char *description; /* 人类可读的规则描述 */
} TypeInferenceRule;

/* ============== 节点-类型映射条目 ============== */
typedef struct {
    int node_id;      /* 节点ID */
    TypeRegion *type; /* 类型区域 */
} NodeTypeMapping;

/* ============== 重写路径记录与回放 ============== */

/**
 * @brief 单步重写操作记录
 */
typedef struct {
    int step_number;    /* 步骤编号 */
    char *rule_name;    /* 应用的规则名称 */
    TypeRegion *before; /* 重写前的类型 */
    TypeRegion *after;  /* 重写后的类型 */
} TypeRewriteStep;

/**
 * @brief 重写路径——一系列重写步骤
 */
typedef struct {
    TypeRewriteStep *steps; /* 重写步骤数组 */
    int step_count;         /* 当前步骤数 */
    int capacity;           /* 数组容量 */
} TypeRewritePath;

/**
 * @brief 创建重写路径
 */
LV00_PUBLIC_API TypeRewritePath *type_rewrite_path_create(void);

/**
 * @brief 销毁重写路径
 */
LV00_PUBLIC_API void type_rewrite_path_destroy(TypeRewritePath *path);

/**
 * @brief 记录一步重写操作
 * @param path 重写路径
 * @param rule_name 应用的规则名称
 * @param before 重写前的类型
 * @param after 重写后的类型
 */
LV00_PUBLIC_API void type_rewrite_path_record(TypeRewritePath *path, const char *rule_name, const TypeRegion *before,
                              const TypeRegion *after);

/**
 * @brief 回放到指定步骤
 * @param path 重写路径
 * @param target_step 目标步骤编号（从0开始）
 * @return true 回放成功，false 失败
 */
LV00_PUBLIC_API bool type_rewrite_path_replay(TypeRewritePath *path, int target_step);

/**
 * @brief 获取类型系统的重写路径
 * @param ts 类型系统
 * @return 重写路径指针（只读），未设置返回NULL
 */
LV00_PUBLIC_API const TypeRewritePath *type_system_get_rewrite_path(const TypeSystem *ts);

/* ============== 类型系统上下文 ============== */
/**
 * @struct TypeSystem
 * @brief 类型系统 —— 管理所有类型区域、类型变量、重写规则和推断规则的核心上下文
 *
 * 【动态数组与容量同步】
 * TypeSystem 包含多个动态数组（type_regions, type_vars, node_type_mappings,
 * inference_rules 等），每个数组都有对应的 count 和 capacity 字段。
 * 修改 count 时必须同步更新 capacity：
 *   - 当 count 达到 capacity 时，必须先扩容（通常为 2 倍），再添加元素
 *   - 直接修改 type_region_count 而不检查/更新 type_region_capacity 会导致
 *     数组越界写入，引发内存损坏
 *   - 建议仅通过 type_system_create() / type_system_destroy() 及各
 *     type_create_*() 工厂函数操作这些数组，避免手动修改 count/capacity
 *
 * 【访问器函数建议】
 * 建议使用本头文件中声明的访问器函数（如 type_create_*(),
 * type_check_equivalence(), type_infer_node() 等）而非直接访问结构体内部数组。
 * 直接访问内部数组的风险：
 *   1. 可能跳过容量检查和边界验证
 *   2. 可能破坏类型系统的内部不变式（如良基性、累积性约束）
 *   3. 未来版本中内部布局可能变化，直接访问将导致不兼容
 */
struct TypeSystem {
    TypeRegion **type_regions; /* 类型区域数组 */
    int type_region_count;     /* 类型区域数量 */
    int type_region_capacity;  /* 类型区域数组容量（用于指数扩容） */

    TypeVariable **type_vars; /* 类型变量数组 */
    int type_var_count;       /* 类型变量数量 */

    RewriteRule **rewrite_rules; /* 重写规则数组（用于类型等价检查） */
    int rewrite_rule_count;      /* 重写规则数量 */

    bool well_founded; /* 是否启用良基模式 */
    bool cumulative;   /* 是否启用累积性 */

    int max_universe_level; /* 最大宇宙层级 */

    /* 节点-类型外部映射 */
    NodeTypeMapping *node_type_mappings; /* 节点-类型映射数组 */
    int node_type_mapping_count;         /* 映射条目数量 */
    int node_type_mapping_capacity;      /* 映射数组容量 */

    /* 重写路径记录 */
    TypeRewritePath *rewrite_path; /* 重写路径（用于记录类型重写历史） */

    /* 类型推断规则表 */
    TypeInferenceRule *inference_rules; /* 推断规则数组 */
    int inference_rule_count;           /* 推断规则数量 */
    int inference_rule_capacity;        /* 推断规则数组容量 */
};

/* ============== 类型系统管理API ============== */

/**
 * 创建类型系统
 */
LV00_PUBLIC_API TypeSystem *type_system_create(void);

/**
 * 销毁类型系统
 */
LV00_PUBLIC_API void type_system_destroy(TypeSystem *ts);

/**
 * 设置良基模式
 */
LV00_PUBLIC_API void type_system_set_well_founded(TypeSystem *ts, bool well_founded);

/**
 * 设置累积性
 */
LV00_PUBLIC_API void type_system_set_cumulative(TypeSystem *ts, bool cumulative);

/* ============== 类型区域管理 ============== */

/**
 * 创建点类型
 */
LV00_PUBLIC_API TypeRegion *type_create_point(TypeSystem *ts);

/**
 * 创建线段类型
 */
LV00_PUBLIC_API TypeRegion *type_create_line_segment(TypeSystem *ts);

/**
 * 创建区域类型
 */
LV00_PUBLIC_API TypeRegion *type_create_region(TypeSystem *ts, int *contained_ids, int count);

/**
 * 创建函数类型
 */
LV00_PUBLIC_API TypeRegion *type_create_function(TypeSystem *ts, TypeRegion *input, TypeRegion *output);

/**
 * 创建乘积类型
 */
LV00_PUBLIC_API TypeRegion *type_create_product(TypeSystem *ts, TypeRegion *left, TypeRegion *right);

/**
 * 创建和类型
 */
LV00_PUBLIC_API TypeRegion *type_create_sum(TypeSystem *ts, TypeRegion *first, TypeRegion *second);

/**
 * 创建类型变量
 */
LV00_PUBLIC_API TypeRegion *type_create_variable(TypeSystem *ts, const char *name);

/**
 * 创建依赖类型
 */
LV00_PUBLIC_API TypeRegion *type_create_dependent(TypeSystem *ts, int param_id, TypeRegion *body);

/**
 * 创建底部类型
 */
LV00_PUBLIC_API TypeRegion *type_create_bottom(TypeSystem *ts);

/**
 * 创建PVS风格谓词子类型 {x:base_type | predicate}
 * @param ts 类型系统
 * @param base_type 基类型 T
 * @param predicate_name 谓词名称 P
 * @param predicate_expr 谓词表达式（可选，用于显示）
 * @return 新创建的谓词子类型
 */
LV00_PUBLIC_API TypeRegion *type_create_predicate_subtype(TypeSystem *ts, TypeRegion *base_type,
                                          const char *predicate_name,
                                          const char *predicate_expr);

/**
 * 验证值是否满足谓词子类型的约束
 * @param ts 类型系统
 * @param subtype 谓词子类型
 * @param node_id 要验证的节点ID
 * @return true 满足谓词约束，false 不满足或出错
 */
LV00_PUBLIC_API bool type_check_predicate_subtype_value(TypeSystem *ts, TypeRegion *subtype, int node_id);

/**
 * 获取谓词子类型的基类型
 * @param subtype 谓词子类型
 * @return 基类型，如果不是谓词子类型则返回NULL
 */
LV00_PUBLIC_API TypeRegion *type_predicate_subtype_get_base(TypeRegion *subtype);

/**
 * 销毁类型区域
 */
LV00_PUBLIC_API void type_region_destroy(TypeRegion *tr);

/**
 * 添加类型别名
 */
LV00_PUBLIC_API bool type_add_alias(TypeRegion *tr, const char *alias);

/* ============== 宇宙层级检查 ============== */

/**
 * 获取类型的宇宙层级
 */
LV00_PUBLIC_API UniverseLevel type_get_level(TypeRegion *tr);

/**
 * 检查层级有效性
 * @param container 包含者类型
 * @param contained 被包含者类型
 * @return 是否有效
 */
LV00_PUBLIC_API bool type_check_level_validity(TypeSystem *ts, TypeRegion *container, TypeRegion *contained);

/**
 * 检查累积性
 * @param lower 较低层级类型
 * @param higher 较高层级类型
 * @return lower是否自动属于higher
 */
LV00_PUBLIC_API bool type_check_cumulative(TypeSystem *ts, TypeRegion *lower, TypeRegion *higher);

/* ============== 类型等价检查 ============== */

/**
 * 检查两个类型是否等价
 * @param ts 类型系统
 * @param type1 第一个类型
 * @param type2 第二个类型
 * @param use_rewrite 是否使用重写引擎
 * @return 等价检查结果
 */
LV00_PUBLIC_API TypeEquivResult type_check_equivalence(TypeSystem *ts, TypeRegion *type1, TypeRegion *type2, bool use_rewrite);

/**
 * 检查端口类型兼容性
 * @param ts 类型系统
 * @param source_type 源端口类型
 * @param target_type 目标端口类型
 * @return 类型检查结果
 */
LV00_PUBLIC_API TypeCheckResult type_check_port_compatibility(TypeSystem *ts, TypeRegion *source_type, TypeRegion *target_type);

/* ============== 类型推断 ============== */

/**
 * 推断节点类型
 * @param ts 类型系统
 * @param graph 约束图
 * @param node_id 节点ID
 * @param out_type 输出的推断类型
 * @return 是否成功推断
 */
LV00_PUBLIC_API bool type_infer_node(TypeSystem *ts, ConstraintGraph *graph, int node_id, TypeRegion **out_type);

/**
 * 推断端口类型
 * @param ts 类型系统
 * @param graph 约束图
 * @param port_id 端口ID
 * @param out_type 输出的推断类型
 * @return 是否成功推断
 */
LV00_PUBLIC_API bool type_infer_port(TypeSystem *ts, ConstraintGraph *graph, int port_id, TypeRegion **out_type);

/* ============== 类型变量实例化 ============== */

/**
 * 实例化类型变量
 * @param ts 类型系统
 * @param var_id 变量ID
 * @param concrete_type 具体类型
 * @return 是否成功
 */
LV00_PUBLIC_API bool type_instantiate_variable(TypeSystem *ts, int var_id, TypeRegion *concrete_type);

/**
 * 替换类型中的变量
 * @param ts 类型系统
 * @param type 类型
 * @param var_id 变量ID
 * @param replacement 替换类型
 * @param out_result 输出的结果类型
 * @return 是否成功
 */
LV00_PUBLIC_API bool type_substitute_variable(TypeSystem *ts, TypeRegion *type, int var_id, TypeRegion *replacement,
                              TypeRegion **out_result);

/* ============== 非良基模式 ============== */

/**
 * 检测类型循环
 * @param ts 类型系统
 * @param type 类型
 * @return 是否存在循环
 */
LV00_PUBLIC_API bool type_detect_cycle(TypeSystem *ts, TypeRegion *type);

/**
 * 检查非良基相容性
 * @param ts 类型系统
 * @param type 类型
 * @return 是否相容
 */
LV00_PUBLIC_API bool type_check_non_well_founded_compatibility(TypeSystem *ts, TypeRegion *type);

/* ============== 类型规范化 ============== */

/**
 * 规范化类型
 * @param ts 类型系统
 * @param type 类型
 * @param out_normalized 输出的规范化类型
 * @return 是否成功
 */
LV00_PUBLIC_API bool type_normalize(TypeSystem *ts, TypeRegion *type, TypeRegion **out_normalized);

/* ============== 类型附加到节点 ============== */

/**
 * 将类型区域附加到节点
 * 使用外部映射表，不修改 GeomNode 结构
 * @param ts 类型系统
 * @param node_id 节点ID
 * @param type 要附加的类型区域
 * @return true 成功，false 失败
 */
LV00_PUBLIC_API bool type_attach_to_node(TypeSystem *ts, int node_id, TypeRegion *type);

/**
 * 获取节点附加的类型区域
 * @param ts 类型系统
 * @param node_id 节点ID
 * @return 类型区域指针，未找到返回NULL
 */
LV00_PUBLIC_API TypeRegion *type_get_node_type(const TypeSystem *ts, int node_id);

/**
 * 从节点分离类型区域
 * @param ts 类型系统
 * @param node_id 节点ID
 * @return true 成功，false 未找到
 */
LV00_PUBLIC_API bool type_detach_node_type(TypeSystem *ts, int node_id);

/* ============== 依赖类型检查 ============== */

/**
 * 依赖类型检查
 * 对于依赖类型 Π(x:A).B(x)，检查将输入值代入B后是否产生期望的输出类型
 * 简化实现：检查 output_type 和 input_type 是否具有兼容的结构
 * @param ts 类型系统
 * @param output_type 输出类型（期望的返回类型）
 * @param input_type 输入类型（依赖类型的参数类型）
 * @param input_values 输入值的符号坐标数组（可为NULL）
 * @return true 兼容，false 不兼容
 */
LV00_PUBLIC_API bool type_check_dependent(const TypeSystem *ts, const TypeRegion *output_type, const TypeRegion *input_type,
                          const SymbolicCoord **input_values);

/* ============== 辅助函数 ============== */

/**
 * 类型种类转字符串
 */
LV00_PUBLIC_API const char *type_kind_to_string(TypeKind kind);

/**
 * 宇宙层级转字符串
 */
LV00_PUBLIC_API const char *universe_level_to_string(UniverseLevel level);

/**
 * 类型等价结果转字符串
 */
LV00_PUBLIC_API const char *type_equiv_result_to_string(TypeEquivResult result);

/**
 * 类型检查结果转字符串
 */
LV00_PUBLIC_API const char *type_check_result_to_string(TypeCheckResult result);

/**
 * 打印类型
 */
LV00_PUBLIC_API void type_print(const TypeRegion *tr, int indent);

/* ============== 规则表驱动的类型推断 ============== */

/**
 * 注册一条类型推断规则
 * @param ts 类型系统
 * @param source_node_type 源节点几何类型 (GeomType, e.g., GEOM_POINT)
 * @param target_type_kind 目标类型种类 (TypeKind, e.g., TYPE_KIND_POINT)
 * @param priority 规则优先级（数值越小优先级越高）
 * @param description 人类可读的规则描述
 * @return 0 成功，-1 失败
 */
LV00_PUBLIC_API int type_system_register_inference_rule(TypeSystem *ts, int source_node_type, int target_type_kind, int priority,
                                        const char *description);

/**
 * 获取所有已注册的类型推断规则
 * @param ts 类型系统
 * @param rule_count 输出规则数量
 * @return 规则数组指针（只读），无规则时返回NULL
 */
LV00_PUBLIC_API const TypeInferenceRule *type_system_get_inference_rules(TypeSystem *ts, int *rule_count);

/**
 * 清除所有自定义推断规则（恢复为默认规则集）
 * @param ts 类型系统
 */
LV00_PUBLIC_API void type_system_clear_inference_rules(TypeSystem *ts);

/**
 * 使用已注册的规则链执行类型推断
 * 遍历规则表，找到第一条匹配的规则，创建对应类型并附加到节点。
 * @param ts 类型系统
 * @param graph 约束图
 * @param node_id 节点ID
 * @return TYPE_EQUIV_OK 规则匹配成功，TYPE_EQUIV_NOT_EQUIV 无规则匹配
 */
LV00_PUBLIC_API TypeEquivResult type_infer_by_rules(TypeSystem *ts, ConstraintGraph *graph, int node_id);

/* ============== 类型系统路径探索器 ============== */

/**
 * @brief 路径探索器——交互式类型重写路径搜索
 *
 * 提供在 TypeSystem 中从当前类型区域探索到目标类型区域的
 * 交互式路径搜索功能。支持：
 * - 查找可应用的重写规则
 * - 预览规则应用效果
 * - 应用规则并记录历史
 * - 撤销操作（基于 GraphSnapshot 事务回滚）
 * - 目标检查（基于 type_check_equivalence）
 * - 导出探索路径为 TypeRewritePath
 */

/* 前向声明 */
typedef struct PathExplorer PathExplorer;

/**
 * @brief 探索器单步记录
 */
typedef struct {
    int rule_index;  /* 应用的规则在 available_rules 中的索引 */
    char *rule_name; /* 规则名称（用于显示） */
    int step_number; /* 步骤编号（从0开始） */
} ExplorerStep;

/**
 * @brief 探索器操作结果
 */
typedef enum {
    EXPLORER_OK,           /* 操作成功 */
    EXPLORER_GOAL_REACHED, /* 当前类型已匹配目标 */
    EXPLORER_NO_RULES,     /* 无可应用规则 */
    EXPLORER_INVALID_RULE, /* 指定规则不可应用 */
    EXPLORER_UNDO_EMPTY,   /* 无历史可撤销 */
    EXPLORER_ERROR         /* 一般性错误 */
} ExplorerResult;

/**
 * @brief 创建路径探索器
 * @param ts 类型系统
 * @param current 当前类型区域（探索起点）
 * @param target 目标类型区域（探索终点）
 * @return 探索器指针，失败返回 NULL
 */
LV00_PUBLIC_API PathExplorer *path_explorer_create(TypeSystem *ts, TypeRegion *current, TypeRegion *target);

/**
 * @brief 销毁路径探索器
 * @param explorer 探索器指针
 */
LV00_PUBLIC_API void path_explorer_destroy(PathExplorer *explorer);

/**
 * @brief 获取当前状态下可应用的规则列表
 * @param explorer 探索器
 * @param rule_indices 输出可应用规则索引数组（调用者需 free）
 * @param count 输出可应用规则数量
 * @return EXPLORER_OK 成功，EXPLORER_GOAL_REACHED 已达目标
 */
LV00_PUBLIC_API ExplorerResult path_explorer_get_applicable_rules(const PathExplorer *explorer, int **rule_indices, int *count);

/**
 * @brief 预览规则应用效果（不修改状态）
 * @param explorer 探索器
 * @param rule_index 规则索引
 * @param preview_result 输出预览结果类型区域（调用者需 type_region_destroy）
 * @return EXPLORER_OK 预览成功，EXPLORER_INVALID_RULE 规则不可应用
 */
LV00_PUBLIC_API ExplorerResult path_explorer_preview_rule(PathExplorer *explorer, int rule_index, TypeRegion **preview_result);

/**
 * @brief 应用规则（修改当前状态）
 * @param explorer 探索器
 * @param rule_index 规则索引
 * @return EXPLORER_OK 成功，EXPLORER_INVALID_RULE 规则不可应用
 */
LV00_PUBLIC_API ExplorerResult path_explorer_apply_rule(PathExplorer *explorer, int rule_index);

/**
 * @brief 撤销上一步操作
 * @param explorer 探索器
 * @return EXPLORER_OK 成功，EXPLORER_UNDO_EMPTY 无历史可撤销
 */
LV00_PUBLIC_API ExplorerResult path_explorer_undo(PathExplorer *explorer);

/**
 * @brief 检查当前类型是否已达到目标
 * @param explorer 探索器
 * @param reached 输出是否达到目标
 * @return EXPLORER_OK 检查成功
 */
LV00_PUBLIC_API ExplorerResult path_explorer_check_goal(const PathExplorer *explorer, bool *reached);

/**
 * @brief 将探索路径导出为 TypeRewritePath
 * @param explorer 探索器
 * @param out_path 输出重写路径（调用者需 type_rewrite_path_destroy）
 * @return EXPLORER_OK 成功
 */
LV00_PUBLIC_API ExplorerResult path_explorer_save_path(const PathExplorer *explorer, TypeRewritePath **out_path);

/**
 * @brief 获取已执行步骤数
 * @param explorer 探索器
 * @return 步骤数量
 */
LV00_PUBLIC_API int path_explorer_get_step_count(const PathExplorer *explorer);

/**
 * @brief 获取所有步骤记录
 * @param explorer 探索器
 * @return 步骤数组指针（只读），无步骤返回 NULL
 */
LV00_PUBLIC_API const ExplorerStep *path_explorer_get_steps(const PathExplorer *explorer);

/**
 * @brief 获取当前类型区域
 * @param explorer 探索器
 * @return 当前类型区域指针（只读），失败返回 NULL
 */
LV00_PUBLIC_API const TypeRegion *path_explorer_get_current(const PathExplorer *explorer);

#ifdef __cplusplus
}
#endif

#endif /* LV00_TYPE_SYSTEM_H */
