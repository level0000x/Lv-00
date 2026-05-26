/* ========================================================================
 * 模块名称：函数块系统 (func_block)
 * 功能概述：提供函数块的打包、例化、确定性检查、多解选择器等核心功能。
 *          函数块是 Lv-00 系统中封装几何构造的基本单元，支持将一组
 *          内部节点和约束打包为可复用的"黑盒"，通过输入/输出端口
 *          与外部约束图交互。
 *
 * 主要 API：
 *   - func_block_create / func_block_destroy  — 创建/销毁函数块
 *   - func_block_pack / func_block_pack_ex    — 打包操作
 *   - func_block_instantiate                   — 例化操作
 *   - func_block_determinism_check_static     — 静态确定性检查
 *   - func_block_determinism_check_dynamic    — 动态确定性检查
 *   - func_block_compose / func_block_product — 组合子
 *   - selector_create / selector_apply        — 多解选择器
 *
 * 使用示例：
  *   FuncBlock *fb = func_block_create(1);
 *   func_block_set_internal_nodes(fb, node_ids, count);
 *   func_block_set_input_ports(fb, in_ports, in_count);
 *   func_block_set_output_ports(fb, out_ports, out_count);
 *   PackConfig config = { ... };
 *   FuncBlock *result;
  *   PackResult pr = func_block_pack_ex(graph, &config, &result);
 *
 * 根据 Lv-00 设计文档第8节实现：
 * - 打包操作（PackFunction）
 * - 确定性检查（静态层 + 动态层）
 * - 多解选择器
 * - 部分应用（柯里化）
 * - 函数块组合子
 * ======================================================================== */

/**
 * @file func_block.h
 * @brief 函数块系统 - 打包、例化、确定性检查、多解选择器
 */

#ifndef LV00_FUNC_BLOCK_H
#define LV00_FUNC_BLOCK_H

#include <stdbool.h>
#include <stdint.h>  /* v3.4.2: 添加 uint16_t 支持 */

#include "determinism_state.h"  /* 确定性状态枚举（解决循环依赖） */
#include "constraint_graph.h"
#include "func_block_utils.h"
#include "stream.h"
#include "symbolic_coord.h"
#ifndef LV00_PUBLIC_API
#define LV00_PUBLIC_API
#endif


#ifdef __cplusplus
extern "C" {
#endif

/* ============== 前向声明 ============== */
typedef struct FuncBlock FuncBlock;
typedef struct PortDependency PortDependency;
typedef struct SolutionSelector SolutionSelector;

/* ============== 确定性状态机 ==============
 * 【枚举值命名规范】所有枚举值使用 UPPER_SNAKE_CASE
 * DeterminismState 已在 determinism_state.h 中统一定义，
 * 此处通过 #include "determinism_state.h" 自动获得。
 * 原先此处的定义已移除，避免与 GeomNode 中的匿名枚举重复。
 */

/* ============== 端口依赖类型 ============== */
typedef enum {
    PORT_DEP_INCIDENCE,   /* 关联约束 */
    PORT_DEP_BETWEENNESS, /* 之间约束 */
    PORT_DEP_CONTAINMENT, /* 包含约束 */
    PORT_DEP_INTERSECTION /* 相交约束 */
} PortDependencyType;

/* ============== 端口依赖结构 ============== */
struct PortDependency {
    PortDependencyType type; /* 依赖类型 */
    int port_id;             /* 相关端口ID */
    int external_node_id;    /* 外部节点ID */
    int internal_node_id;    /* 内部节点ID */
    void *constraint_data;   /* 约束数据 */
};

/* ============== 多解选择器 ==============
 * 【枚举值命名规范】所有枚举值使用 UPPER_SNAKE_CASE
 */
typedef enum {
    SELECTOR_TYPE_POSITIVE_ROOT,    /**< 取正根 */
    SELECTOR_TYPE_NEGATIVE_ROOT,    /**< 取负根 */
    SELECTOR_TYPE_IN_REGION,        /**< 取位于区域内的解 */
    SELECTOR_TYPE_NEAREST_TO_POINT, /**< 取距离某点最近的解 */
    SELECTOR_TYPE_CUSTOM            /**< 自定义选择器 */
} SelectorType;

typedef bool (*SelectorFunction)(GeomNode **candidates, int count, int *selected_index, void *user_data);

/**
 * @brief 多解选择器结构 - 改进版 v3.4.1
 *
 * 支持深拷贝的选择器结构，添加了名称、候选解数组和 user_data 生命周期管理。
 * 用于在函数块例化产生多个候选解时，根据指定策略选择唯一解。
 */
struct SolutionSelector {
    /* === 基础配置 === */
    SelectorType type;            /**< 选择器类型 */
    int reference_node_id;        /**< 参考节点ID（如区域、点等） */
    
    /* === 回调函数 === */
    SelectorFunction custom_func; /**< 自定义选择函数 */
    int (*compare)(const void *a, const void *b); /**< 候选解比较函数 */
    void (*on_select)(int selected_index, void *user_data); /**< 选择回调 */
    void (*on_change)(int old_index, int new_index, void *user_data); /**< 切换回调 */
    
    /* === user_data 生命周期管理 === */
    void *user_data;              /**< 用户透传数据 */
    void (*free_user_data)(void *user_data);   /**< user_data 释放回调（可选） */
    void *(*copy_user_data)(const void *user_data); /**< user_data 深拷贝回调（可选） */
    
    /* === 约束图引用 === */
    ConstraintGraph *graph;       /**< 显式的约束图引用 */
    
    /* === 候选解管理 === */
    char *name;                   /**< 选择器名称（用于调试和UI显示） */
    double *solution_values;      /**< 候选解数值数组（用于排序和比较） */
    int solution_count;           /**< 候选解数量 */
    int current_index;            /**< 当前选中的候选解索引 */
};

/* ============== 函数块跨边界约束（扩展版） ============== */
typedef enum {
    CROSS_BOUNDARY_PROMOTE,    /* 提升：成为端口依赖 */
    CROSS_BOUNDARY_DISCONNECT, /* 断开：删除约束 */
    CROSS_BOUNDARY_CANCEL      /* 取消：放弃打包 */
} CrossBoundaryAction;

typedef struct FuncBlockCrossBoundary {
    int constraint_id;              /* 约束ID */
    int internal_node_id;           /* 内部节点ID */
    int external_node_id;           /* 外部节点ID */
    ConstraintType constraint_type; /* 约束类型 */
    CrossBoundaryAction action;     /* 用户选择的处理方式 */
} FuncBlockCrossBoundary;

/* ============== 函数块视图状态 ==============
 * 【枚举值命名规范】所有枚举值使用 UPPER_SNAKE_CASE
 */
typedef enum {
    FB_VIEW_STATE_EXPANDED,  /**< 展开显示内部构造 */
    FB_VIEW_STATE_COLLAPSED, /**< 折叠为单个盒子 */
    FB_VIEW_STATE_PINNED     /**< 固定展开（用户锁定） */
} FuncBlockViewState;

/* ============== 跨边界约束处理结果 ============== */
typedef struct {
    CrossBoundaryAction action; /* 用户选择 */
    bool processed;             /* 是否已处理 */
} CrossBoundaryResolution;

/**
 * @brief 跨边界约束处理回调
 *
 * 当打包检测到跨边界约束时调用，让用户选择处理方式。
 */
typedef CrossBoundaryResolution (*CrossBoundaryCallback)(int constraint_id, ConstraintType constraint_type,
                                                         int internal_node_id, int external_node_id, void *user_data);

/* ============== 函数块结构（v3.4.2 改进版） ==============
 *
 * 【重要说明】v3.4.2 改进：
 * 1. 添加版本字段用于序列化兼容性检查
 * 2. 添加容量字段（port_dep_capacity）用于安全边界检查
 * 3. 结构体定义保持公开以允许栈分配，但建议通过访问器函数访问字段
 *
 * 【内存布局】字段按类型大小降序排列，优化内存对齐：
 * - 指针字段（8字节）在前
 * - int 字段（4字节）其次
 * - bool/enum（1-4字节）最后
 */
struct FuncBlock {
    /* === 指针字段（8字节对齐）=== */
    int *internal_node_ids;         /**< 内部节点ID数组 */
    int *input_port_ids;            /**< 输入端口ID数组 */
    int *output_port_ids;           /**< 输出端口ID数组 */
    PortDependency *port_deps;      /**< 端口依赖数组 */
    char *name;                     /**< 函数块名称 */
    char *description;              /**< 函数块描述 */
    int *precondition_region_ids;   /**< 前置条件区域ID数组 */
    SolutionSelector *selector;     /**< 多解选择器 */

    /* === 函数指针 === */
    int (*measure_compare)(const GeomNode *a, const GeomNode *b); /**< 测度比较函数 */

    /* === int 字段（4字节对齐）=== */
    int id;                         /**< 函数块ID */
    int internal_node_count;        /**< 内部节点数量 */
    int input_count;                /**< 输入端口数量 */
    int output_count;               /**< 输出端口数量 */
    int port_dep_count;             /**< 端口依赖数量 */
    int port_dep_capacity;          /**< 端口依赖数组容量（v3.4.2 新增） */
    int precondition_count;         /**< 前置条件数量 */
    int measure_node_id;            /**< 测度节点ID */

    /* === 版本字段（v3.4.2 新增）=== */
    uint16_t version_major;         /**< 主版本号 */
    uint16_t version_minor;         /**< 次版本号 */
    uint16_t version_patch;         /**< 补丁版本号 */

    /* === 布尔和枚举字段 === */
    bool has_measure;               /**< 是否声明了测度 */
    bool is_instantiated;           /**< 是否已被例化（生命周期追踪，v3.4.2 新增） */
    DeterminismState determinism;   /**< 确定性状态 */
    FuncBlockViewState view_state;  /**< 视图折叠/展开状态 */
};

/* ============== 打包结果 ==============
 * 【枚举值命名规范】所有枚举值使用 UPPER_SNAKE_CASE
 */
typedef enum {
    PACK_RESULT_OK,                      /**< 打包成功 */
    PACK_RESULT_CROSS_BOUNDARY_CONFLICT, /**< 存在跨边界约束 */
    PACK_RESULT_INVALID_NODES,           /**< 无效节点 */
    PACK_RESULT_INVALID_PORTS,           /**< 无效端口 */
    PACK_RESULT_INVALID_GRAPH,           /**< 无效图 */
    PACK_RESULT_OUT_OF_MEMORY,           /**< 内存不足 */
    PACK_RESULT_CANCELLED                /**< 用户取消 */
} PackResult;

/* ============== 例化结果 ============== */
typedef enum {
    LV00_INSTANTIATE_OK,                  /* 例化成功 */
    LV00_INSTANTIATE_NO_SOLUTION,         /* 无解 */
    LV00_INSTANTIATE_MULTIPLE_SOLUTIONS,  /* 多解 */
    LV00_INSTANTIATE_SELECTOR_NEEDED,     /* 需要选择器 */
    LV00_INSTANTIATE_PRECONDITION_FAILED, /* 前置条件不满足 */
    LV00_INSTANTIATE_OUT_OF_MEMORY        /* 内存不足 */
} InstantiateResult;

/* 向后兼容别名：旧名称映射到新名称 */
#define INSTANTIATE_OK                  LV00_INSTANTIATE_OK
#define INSTANTIATE_NO_SOLUTION         LV00_INSTANTIATE_NO_SOLUTION
#define INSTANTIATE_MULTIPLE_SOLUTIONS  LV00_INSTANTIATE_MULTIPLE_SOLUTIONS
#define INSTANTIATE_SELECTOR_NEEDED     LV00_INSTANTIATE_SELECTOR_NEEDED
#define INSTANTIATE_PRECONDITION_FAILED LV00_INSTANTIATE_PRECONDITION_FAILED
#define INSTANTIATE_OUT_OF_MEMORY       LV00_INSTANTIATE_OUT_OF_MEMORY

/* ============== 确定性检查结果（设计文档 8.2 节） ============== */

/**
 * 确定性状态（DeterminismState）— 函数块的固有属性
 *   DETERMINISM_VERIFIED          — 唯一解已确认
 *   DETERMINISM_PARTIALLY_VERIFIED — 分析未完成但无冲突
 *   DETERMINISM_NON_DETERMINISTIC  — 存在冲突或多解
 *   DETERMINISM_UNVERIFIED         — 未进行分析
 */

/**
 * 确定性检查返回值（DeterminismStatus）— 确定性检查函数的返回类型
 * 与 DeterminismState 使用相同的枚举值，语义上表示检查结果。
 */
typedef DeterminismState DeterminismStatus;

/* ============== 确定性检查结果（详细） ==============
 * 【枚举值命名规范】所有枚举值使用 UPPER_SNAKE_CASE
 */
typedef enum {
    DETERMINISM_CHECK_RESULT_UNIQUE,      /**< 唯一解 */
    DETERMINISM_CHECK_RESULT_MULTIPLE,    /**< 多解 */
    DETERMINISM_CHECK_RESULT_NO_SOLUTION, /**< 无解 */
    DETERMINISM_CHECK_RESULT_TIMEOUT,     /**< 超时 */
    DETERMINISM_CHECK_RESULT_OUT_OF_RANGE /**< 超出范围 */
} DeterminismCheckResult;

/* ============== 打包配置结构（简化API参数） ============== */

/**
 * @brief 函数块打包配置
 *
 * 使用此结构体可以简化 func_block_pack 的调用，
 * 避免传递过多的独立参数。
 */
typedef struct {
    /* 必需参数 */
    const int *internal_node_ids; /* 内部节点ID数组 */
    int internal_count;           /* 内部节点数量 */
    const int *input_port_ids;    /* 输入端口ID数组 */
    int input_count;              /* 输入端口数量 */
    const int *output_port_ids;   /* 输出端口ID数组 */
    int output_count;             /* 输出端口数量 */

    /* 可选参数（可为NULL） */
    const CrossBoundaryAction *cross_boundary_actions; /* 跨边界约束处理方式 */
    int cross_boundary_count;                          /* 跨边界约束数量 */

    /* 可选配置 */
    const char *name;        /* 函数块名称 */
    const char *description; /* 函数块描述 */
} PackConfig;

/* ============== 函数块管理API ============== */

/**
 * @brief 创建函数块
 * @param id 函数块ID
 * @return 函数块指针，失败返回NULL
 *
 * @note 所有权：调用者获得函数块的所有权，负责在不再使用时
 *       调用 func_block_destroy() 释放。函数块内部所有动态分配的
 *       成员（节点数组、端口数组、名称、描述等）均由 func_block_destroy()
 *       统一释放，调用者不应单独释放这些成员。
 */
LV00_PUBLIC_API FuncBlock *func_block_create(int id);

/**
 * @brief 销毁函数块，释放所有内部资源
 * @param fb 函数块指针（可为NULL，NULL 时安全返回）
 *
 * @note 释放责任：此函数会递归释放函数块的所有动态成员，
 *       包括内部节点数组、输入/输出端口数组、端口依赖数组、
 *       选择器（通过 selector_destroy）、前置条件数组、名称和描述字符串。
 *       调用后 fb 指针不可再使用。
 */
LV00_PUBLIC_API void func_block_destroy(FuncBlock *fb);

/**
 * @brief 设置内部节点
 * @param fb 函数块
 * @param node_ids 节点ID数组
 * @param count 节点数量
 * @return true 成功，false 失败
 */
LV00_PUBLIC_API bool func_block_set_internal_nodes(FuncBlock *fb, const int *node_ids, int count);

/**
 * @brief 设置输入端口
 * @param fb 函数块
 * @param port_ids 端口ID数组
 * @param count 端口数量
 * @return true 成功，false 失败
 */
LV00_PUBLIC_API bool func_block_set_input_ports(FuncBlock *fb, const int *port_ids, int count);

/**
 * @brief 设置输出端口
 * @param fb 函数块
 * @param port_ids 端口ID数组
 * @param count 端口数量
 * @return true 成功，false 失败
 */
LV00_PUBLIC_API bool func_block_set_output_ports(FuncBlock *fb, const int *port_ids, int count);

/**
 * @brief 设置多解选择器
 * @param fb 函数块
 * @param selector 选择器（函数块接管所有权）
 * @return true 成功，false 失败
 *
 * @note 所有权转移：函数块接管 selector 的所有权。
 *       如果函数块已有关联的选择器，旧选择器会被自动销毁。
 *       调用者传入 selector 后不应再使用或释放该指针。
 *       函数块销毁时会自动释放关联的选择器。
 */
LV00_PUBLIC_API bool func_block_set_selector(FuncBlock *fb, SolutionSelector *selector);

/**
 * @brief 添加端口依赖
 * @param fb 函数块
 * @param dep 端口依赖
 * @return true 成功，false 失败
 */
LV00_PUBLIC_API bool func_block_add_port_dependency(FuncBlock *fb, PortDependency *dep);

/**
 * @brief 设置前置条件
 * @param fb 函数块
 * @param region_ids 区域ID数组
 * @param count 区域数量
 * @return true 成功，false 失败
 */
LV00_PUBLIC_API bool func_block_set_preconditions(FuncBlock *fb, const int *region_ids, int count);

/**
 * @brief 设置函数块名称
 * @param fb 函数块
 * @param name 名称
 * @return true 成功，false 失败
 */
LV00_PUBLIC_API bool func_block_set_name(FuncBlock *fb, const char *name);

/**
 * @brief 设置函数块描述
 * @param fb 函数块
 * @param description 描述
 * @return true 成功，false 失败
 */
LV00_PUBLIC_API bool func_block_set_description(FuncBlock *fb, const char *description);

/**
 * @brief 获取输入端口数量
 * @param fb 函数块指针
 * @return 输入端口数量，fb 为 NULL 时返回 0
 */
LV00_PUBLIC_API int func_block_get_input_count(const FuncBlock *fb);

/**
 * @brief 获取输出端口数量
 * @param fb 函数块指针
 * @return 输出端口数量，fb 为 NULL 时返回 0
 */
LV00_PUBLIC_API int func_block_get_output_count(const FuncBlock *fb);

/**
 * @brief 获取内部节点数量
 * @param fb 函数块指针
 * @return 内部节点数量，fb 为 NULL 时返回 0
 */
LV00_PUBLIC_API int func_block_get_internal_count(const FuncBlock *fb);

/**
 * @brief 获取函数块ID
 * @param fb 函数块指针
 * @return 函数块ID，fb 为 NULL 时返回 -1
 */
LV00_PUBLIC_API int func_block_get_id(const FuncBlock *fb);

/**
 * @brief 获取确定性状态
 * @param fb 函数块指针
 * @return 确定性状态，fb 为 NULL 时返回 DETERMINISM_UNVERIFIED
 */
LV00_PUBLIC_API DeterminismState func_block_get_determinism(const FuncBlock *fb);

/**
 * @brief 获取函数块名称
 * @param fb 函数块指针
 * @return 名称字符串（只读），fb 为 NULL 或无名称时返回 NULL
 */
LV00_PUBLIC_API const char *func_block_get_name(const FuncBlock *fb);

/**
 * @brief 获取函数块描述
 * @param fb 函数块指针
 * @return 描述字符串（只读），fb 为 NULL 或无描述时返回 NULL
 */
LV00_PUBLIC_API const char *func_block_get_description(const FuncBlock *fb);

/**
 * @brief 深拷贝函数块
 *
 * 创建一个函数块的完整深拷贝，包括所有动态分配的成员。
 * 拷贝后的函数块与原始函数块完全独立，修改其中一个不会影响另一个。
 *
 * @param src 源函数块
 * @return 新创建的函数块副本，失败返回 NULL
 *
 * @note 所有权：调用者获得新函数块的所有权，负责在不再使用时
 *       调用 func_block_destroy() 释放。源函数块的所有权不受影响。
 */
LV00_PUBLIC_API FuncBlock *func_block_copy(const FuncBlock *src);

/* ============== 打包操作 ============== */

/**
 * @brief 检测跨边界约束
 * @param graph 约束图
 * @param internal_node_ids 内部节点ID数组
 * @param internal_count 内部节点数量
 * @param out_conflicts 输出的跨边界约束数组
 * @param out_conflict_count 输出的跨边界约束数量
 * @return 是否存在跨边界约束
 *
 * @note 所有权：当返回 true 且存在跨边界约束时，通过 out_conflicts 输出
 *       新分配的约束数组，调用者负责使用 lv00_free() 释放该数组。
 *       当返回 false 时，*out_conflicts 设为 NULL，无需释放。
 */
LV00_PUBLIC_API bool func_block_detect_cross_boundary(ConstraintGraph *graph, const int *internal_node_ids, int internal_count,
                                      CrossBoundaryConstraint **out_conflicts, int *out_conflict_count);

/**
 * @brief 执行打包操作（简化版API）
 * @param graph 约束图
 * @param config 打包配置
 * @param out_func_block 输出的函数块
 * @return 打包结果
 *
 * @note 所有权：成功时通过 out_func_block 输出新创建的函数块，
 *       调用者获得其所有权，负责在不再使用时调用 func_block_destroy() 释放。
 *       失败时 *out_func_block 设为 NULL，无需释放。
 */
LV00_PUBLIC_API PackResult func_block_pack_ex(ConstraintGraph *graph, const PackConfig *config, FuncBlock **out_func_block);

/**
 * @brief 执行打包操作（传统API，保持向后兼容）
 * @param graph 约束图
 * @param internal_node_ids 内部节点ID数组
 * @param internal_count 内部节点数量
 * @param input_port_ids 输入端口ID数组
 * @param input_count 输入端口数量
 * @param output_port_ids 输出端口ID数组
 * @param output_count 输出端口数量
 * @param cross_boundary_actions 跨边界约束处理方式（可为NULL）
 * @param cross_boundary_count 跨边界约束数量
 * @param out_func_block 输出的函数块
 * @return 打包结果
 *
 * @note 所有权：成功时通过 out_func_block 输出新创建的函数块，
 *       调用者获得其所有权，负责在不再使用时调用 func_block_destroy() 释放。
 *       失败时 *out_func_block 设为 NULL，无需释放。
 */
LV00_PUBLIC_API PackResult func_block_pack(ConstraintGraph *graph, const int *internal_node_ids, int internal_count,
                           const int *input_port_ids, int input_count, const int *output_port_ids, int output_count,
                           CrossBoundaryAction *cross_boundary_actions, int cross_boundary_count,
                           FuncBlock **out_func_block);

/* ============== 确定性检查 ============== */


/**
 * @brief 静态确定性检查（增强版）
 * @param fb 函数块
 * @param graph 约束图（const，不修改图）
 * @return 确定性状态
 */
LV00_PUBLIC_API DeterminismStatus func_block_determinism_check_static(FuncBlock *fb, const ConstraintGraph *graph);

/**
 * @brief 动态确定性检查（增强版）
 * @param fb 函数块
 * @param graph 约束图（可修改）
 * @param input_values 实参的符号坐标值数组
 * @param n_inputs 实参数量
 * @return 确定性状态
 */
LV00_PUBLIC_API DeterminismStatus func_block_determinism_check_dynamic(FuncBlock *fb, ConstraintGraph *graph,
                                                       const SymbolicCoord **input_values, int n_inputs);

/**
 * @brief 完整的确定性验证流水线
 * @param fb 函数块
 * @param graph 约束图
 * @param step_limit 静态分析的最大步数
 * @return 最终的确定性状态
 */
LV00_PUBLIC_API DeterminismState func_block_verify_determinism(FuncBlock *fb, ConstraintGraph *graph, int step_limit);

/* ============== 例化操作 ============== */

/**
 * @brief 例化函数块
 * @param fb 函数块
 * @param graph 约束图
 * @param arg_mappings 实参映射（输入端口ID -> 外部节点ID）
 * @param arg_count 实参数量
 * @param out_new_node_ids 输出的新节点ID数组
 * @param out_new_node_count 输出的新节点数量
 * @return 例化结果
 *
 * @note 所有权：成功时通过 out_new_node_ids 输出新分配的节点ID数组，
 *       调用者负责使用 lv00_free() 释放该数组。
 *       失败时 *out_new_node_ids 设为 NULL，无需释放。
 */
LV00_PUBLIC_API InstantiateResult func_block_instantiate(FuncBlock *fb, ConstraintGraph *graph, const int *arg_mappings, int arg_count,
                                         int **out_new_node_ids, int *out_new_node_count);

/**
 * @brief 部分应用（柯里化）
 * @param fb 原函数块
 * @param graph 约束图
 * @param fixed_arg_mappings 固定的实参映射
 * @param fixed_count 固定的实参数量
 * @param out_new_fb 输出的新函数块
 * @return 是否成功
 *
 * @note 所有权：成功时通过 out_new_fb 输出新创建的函数块，
 *       调用者获得其所有权，负责在不再使用时调用 func_block_destroy() 释放。
 *       原函数块 fb 的所有权不受影响。
 */
LV00_PUBLIC_API bool func_block_partial_apply(FuncBlock *fb, ConstraintGraph *graph, const int *fixed_arg_mappings, int fixed_count,
                              FuncBlock **out_new_fb);

/**
 * @brief 执行捕获避免的例化
 * @param block 函数块
 * @param actual_arg_nodes 实参节点ID数组
 * @param arg_count 实参数量
 * @param target_graph 目标约束图
 * @param output_node_ids 输出：内部节点到外部节点ID的映射
 * @param output_node_count 输出：映射数量
 * @return 例化结果
 *
 * @note 所有权：成功时通过 output_node_ids 输出新分配的节点ID数组，
 *       调用者负责使用 lv00_free() 释放该数组。
 *       失败时 *output_node_ids 设为 NULL，无需释放。
 */
LV00_PUBLIC_API InstantiateResult func_block_instantiate_capture_avoiding(FuncBlock *block, const int *actual_arg_nodes, int arg_count,
                                                          ConstraintGraph *target_graph, int **output_node_ids,
                                                          int *output_node_count);

/* ============== 多解选择 ============== */

/**
 * @brief 创建选择器
 * @param type 选择器类型
 * @return 选择器指针，失败返回NULL
 *
 * @note 所有权：调用者获得选择器的所有权，负责在不再使用时
 *       调用 selector_destroy() 释放，或通过 func_block_set_selector()
 *       将所有权转移给函数块。
 */
LV00_PUBLIC_API SolutionSelector *selector_create(SelectorType type);

/**
 * @brief 创建带参考的选择器
 * @param type 选择器类型
 * @param reference_node_id 参考节点ID
 * @return 选择器指针，失败返回NULL
 *
 * @note 所有权：同 selector_create()，调用者获得所有权。
 */
LV00_PUBLIC_API SolutionSelector *selector_create_with_reference(SelectorType type, int reference_node_id);

/**
 * @brief 创建自定义选择器
 * @param func 自定义选择函数
 * @param user_data 用户数据
 * @return 选择器指针，失败返回NULL
 *
 * @note 所有权：同 selector_create()，调用者获得所有权。
 *       user_data 指针由调用者管理生命周期，选择器不会释放它。
 */
LV00_PUBLIC_API SolutionSelector *selector_create_custom(SelectorFunction func, void *user_data);

/**
 * @brief 销毁选择器，释放其内部资源
 * @param selector 选择器指针（可为NULL，NULL 时安全返回）
 *
 * @note 释放责任：释放选择器及其内部资源。
 *       如果选择器已通过 func_block_set_selector() 关联到函数块，
 *       函数块销毁时会自动调用此函数，调用者不应重复释放。
 */
LV00_PUBLIC_API void selector_destroy(SolutionSelector *selector);

/**
 * @brief 设置选择器关联的约束图
 * @param selector 选择器实例
 * @param graph 约束图指针
 */
LV00_PUBLIC_API void selector_set_graph(SolutionSelector *selector, ConstraintGraph *graph);

/**
 * @brief 应用选择器
 * @param selector 选择器
 * @param candidates 候选解数组
 * @param count 候选解数量
 * @param out_selected_index 输出的选中索引
 * @return 是否成功选择
 */
LV00_PUBLIC_API bool selector_apply(SolutionSelector *selector, GeomNode **candidates, int count, int *out_selected_index);

/* ============== 函数块组合子 ============== */

/**
 * @brief 组合两个函数块：g ∘ f
 * @param f 第一个函数块
 * @param g 第二个函数块
 * @param graph 约束图
 * @param out_composed 输出的组合函数块
 * @return 是否成功
 *
 * @note 所有权：成功时通过 out_composed 输出新创建的函数块，
 *       调用者获得其所有权，负责在不再使用时调用 func_block_destroy() 释放。
 *       原函数块 f 和 g 的所有权不受影响。
 */
LV00_PUBLIC_API bool func_block_compose(FuncBlock *f, FuncBlock *g, ConstraintGraph *graph, FuncBlock **out_composed);

/**
 * @brief 乘积两个函数块：f × g
 * @param f 第一个函数块
 * @param g 第二个函数块
 * @param graph 约束图
 * @param out_product 输出的乘积函数块
 * @return 是否成功
 *
 * @note 所有权：同 func_block_compose()，成功时调用者获得输出函数块的所有权。
 */
LV00_PUBLIC_API bool func_block_product(FuncBlock *f, FuncBlock *g, ConstraintGraph *graph, FuncBlock **out_product);

/* ============== 辅助函数 ============== */

/**
 * @brief 获取确定性状态字符串
 * @param state 确定性状态
 * @return 状态字符串
 */
LV00_PUBLIC_API const char *determinism_state_to_string(DeterminismState state);

/**
 * @brief 获取打包结果字符串
 * @param result 打包结果
 * @return 结果字符串
 */
LV00_PUBLIC_API const char *pack_result_to_string(PackResult result);

/**
 * @brief 获取例化结果字符串
 * @param result 例化结果
 * @return 结果字符串
 */
LV00_PUBLIC_API const char *instantiate_result_to_string(InstantiateResult result);

/* ============== 确定性状态持久化 ============== */

/**
 * @brief 序列化函数块的确定性状态
 * @param fb 函数块
 * @return 序列化字符串（调用者负责free），失败返回NULL
 */
LV00_PUBLIC_API char *func_block_serialize_state(const FuncBlock *fb);

/**
 * @brief 反序列化函数块的确定性状态
 * @param fb 函数块
 * @param data 序列化数据字符串
 * @return 是否成功
 */
LV00_PUBLIC_API bool func_block_deserialize_state(FuncBlock *fb, const char *data);

/* ============== 视图折叠/展开（API层） ============== */

/**
 * @brief 设置函数块的视图状态
 * @param fb 函数块
 * @param state 视图状态
 */
LV00_PUBLIC_API void func_block_set_view_state(FuncBlock *fb, FuncBlockViewState state);

/**
 * @brief 获取函数块的视图状态
 * @param fb 函数块
 * @return 视图状态
 */
LV00_PUBLIC_API FuncBlockViewState func_block_get_view_state(const FuncBlock *fb);

/* ============== 打包冲突对话框（API层） ============== */

/**
 * @brief 设置跨边界约束处理回调
 * @param cb 回调函数，传NULL取消回调
 * @param user_data 传递给回调的用户数据
 */
LV00_PUBLIC_API void func_block_set_cross_boundary_callback(CrossBoundaryCallback cb, void *user_data);

/**
 * @brief 设置函数块系统的流式输出上下文
 * @param ctx 流式上下文（可为NULL禁用流式输出）
 */
LV00_PUBLIC_API void func_block_set_stream_context(StreamContext *ctx);

#ifdef __cplusplus
}
#endif

#endif /* LV00_FUNC_BLOCK_H */
