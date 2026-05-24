/**
 * @file func_block.h
 * @brief 函数块系统 - 打包、例化、确定性检查、多解选择器
 *
 * 根据 Lv-00 设计文档第8节实现：
 * - 打包操作（PackFunction）
 * - 确定性检查（静态层 + 动态层）
 * - 多解选择器
 * - 部分应用（柯里化）
 * - 函数块组合子
 */

#ifndef LV00_FUNC_BLOCK_H
#define LV00_FUNC_BLOCK_H

#include "constraint_graph.h"
#include "symbolic_coord.h"
#include <stdbool.h>
#include "stream.h"
#include "func_block_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============== 前向声明 ============== */
typedef struct FuncBlock FuncBlock;
typedef struct PortDependency PortDependency;
typedef struct SolutionSelector SolutionSelector;

/* ============== 确定性状态机 ============== */
typedef enum {
    DETERMINISM_UNVERIFIED,        /* 打包完成，尚未进行静态分析 */
    DETERMINISM_VERIFIED,          /* 静态分析确认解唯一 */
    DETERMINISM_NON_DETERMINISTIC, /* 应用时出现过一次多解 */
    DETERMINISM_PARTIALLY_VERIFIED /* 静态分析未完成但未发现冲突 */
} DeterminismState;

/* ============== 端口依赖类型 ============== */
typedef enum {
    PORT_DEP_INCIDENCE,    /* 关联约束 */
    PORT_DEP_BETWEENNESS,  /* 之间约束 */
    PORT_DEP_CONTAINMENT,  /* 包含约束 */
    PORT_DEP_INTERSECTION  /* 相交约束 */
} PortDependencyType;

/* ============== 端口依赖结构 ============== */
struct PortDependency {
    PortDependencyType type;      /* 依赖类型 */
    int port_id;                  /* 相关端口ID */
    int external_node_id;         /* 外部节点ID */
    int internal_node_id;         /* 内部节点ID */
    void *constraint_data;        /* 约束数据 */
};

/* ============== 多解选择器 ============== */
typedef enum {
    SELECTOR_POSITIVE_ROOT,       /* 取正根 */
    SELECTOR_NEGATIVE_ROOT,       /* 取负根 */
    SELECTOR_IN_REGION,           /* 取位于区域内的解 */
    SELECTOR_NEAREST_TO_POINT,    /* 取距离某点最近的解 */
    SELECTOR_CUSTOM               /* 自定义选择器 */
} SelectorType;

typedef bool (*SelectorFunction)(GeomNode **candidates, int count, int *selected_index, void *user_data);

struct SolutionSelector {
    SelectorType type;
    int reference_node_id;        /* 参考节点ID（如区域、点等） */
    SelectorFunction custom_func; /* 自定义选择函数 */
    void *user_data;              /* 用户透传数据（自定义选择器使用） */
    ConstraintGraph *graph;       /* 显式的约束图引用 */
};

/* ============== 函数块跨边界约束（扩展版） ============== */
typedef enum {
    CROSS_BOUNDARY_PROMOTE,      /* 提升：成为端口依赖 */
    CROSS_BOUNDARY_DISCONNECT,   /* 断开：删除约束 */
    CROSS_BOUNDARY_CANCEL        /* 取消：放弃打包 */
} CrossBoundaryAction;

typedef struct FuncBlockCrossBoundary {
    int constraint_id;            /* 约束ID */
    int internal_node_id;         /* 内部节点ID */
    int external_node_id;         /* 外部节点ID */
    ConstraintType constraint_type; /* 约束类型 */
    CrossBoundaryAction action;   /* 用户选择的处理方式 */
} FuncBlockCrossBoundary;

/* ============== 函数块视图状态 ============== */
typedef enum {
    FB_VIEW_EXPANDED,    /* 展开显示内部构造 */
    FB_VIEW_COLLAPSED,   /* 折叠为单个盒子 */
    FB_VIEW_PINNED       /* 固定展开（用户锁定） */
} FuncBlockViewState;

/* ============== 跨边界约束处理结果 ============== */
typedef struct {
    CrossBoundaryAction action;  /* 用户选择 */
    bool processed;              /* 是否已处理 */
} CrossBoundaryResolution;

/**
 * @brief 跨边界约束处理回调
 *
 * 当打包检测到跨边界约束时调用，让用户选择处理方式。
 */
typedef CrossBoundaryResolution (*CrossBoundaryCallback)(
    int constraint_id,
    ConstraintType constraint_type,
    int internal_node_id,
    int external_node_id,
    void *user_data);

/* ============== 函数块结构 ============== */
struct FuncBlock {
    int id;                       /* 函数块ID */
    int *internal_node_ids;       /* 内部节点ID数组 */
    int internal_node_count;      /* 内部节点数量 */
    int *input_port_ids;          /* 输入端口ID数组 */
    int input_count;              /* 输入端口数量 */
    int *output_port_ids;         /* 输出端口ID数组 */
    int output_count;             /* 输出端口数量 */

    DeterminismState determinism; /* 确定性状态 */
    SolutionSelector *selector;   /* 多解选择器 */

    PortDependency *port_deps;    /* 端口依赖数组 */
    int port_dep_count;           /* 端口依赖数量 */

    char *name;                   /* 函数块名称 */
    char *description;            /* 描述 */

    /* 前置条件区域 */
    int *precondition_region_ids; /* 前置条件区域ID数组 */
    int precondition_count;       /* 前置条件数量 */

    /* 测度（用于递归） */
    bool has_measure;             /* 是否声明了测度 */
    int measure_node_id;          /* 测度节点ID */
    int (*measure_compare)(GeomNode *a, GeomNode *b); /* 测度比较函数 */

    /* 视图状态 */
    FuncBlockViewState view_state; /* 视图折叠/展开状态 */
};

/* ============== 打包结果 ============== */
typedef enum {
    PACK_OK,                      /* 打包成功 */
    PACK_CROSS_BOUNDARY_CONFLICT, /* 存在跨边界约束 */
    PACK_INVALID_NODES,           /* 无效节点 */
    PACK_INVALID_PORTS,           /* 无效端口 */
    PACK_INVALID_GRAPH,           /* 无效图 */
    PACK_OUT_OF_MEMORY,           /* 内存不足 */
    PACK_CANCELLED                /* 用户取消 */
} PackResult;

/* ============== 例化结果 ============== */
typedef enum {
    INSTANTIATE_OK,                  /* 例化成功 */
    INSTANTIATE_NO_SOLUTION,         /* 无解 */
    INSTANTIATE_MULTIPLE_SOLUTIONS,  /* 多解 */
    INSTANTIATE_SELECTOR_NEEDED,     /* 需要选择器 */
    INSTANTIATE_PRECONDITION_FAILED, /* 前置条件不满足 */
    INSTANTIATE_OUT_OF_MEMORY        /* 内存不足 */
} InstantiateResult;

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

/* ============== 确定性检查结果（详细） ============== */
typedef enum {
    DETERMINISM_CHECK_UNIQUE,       /* 唯一解 */
    DETERMINISM_CHECK_MULTIPLE,     /* 多解 */
    DETERMINISM_CHECK_NO_SOLUTION,  /* 无解 */
    DETERMINISM_CHECK_TIMEOUT,      /* 超时 */
    DETERMINISM_CHECK_OUT_OF_RANGE  /* 超出范围 */
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
    const int *internal_node_ids;   /* 内部节点ID数组 */
    int internal_count;             /* 内部节点数量 */
    const int *input_port_ids;      /* 输入端口ID数组 */
    int input_count;                /* 输入端口数量 */
    const int *output_port_ids;     /* 输出端口ID数组 */
    int output_count;               /* 输出端口数量 */

    /* 可选参数（可为NULL） */
    const CrossBoundaryAction *cross_boundary_actions; /* 跨边界约束处理方式 */
    int cross_boundary_count;       /* 跨边界约束数量 */
    
    /* 可选配置 */
    const char *name;               /* 函数块名称 */
    const char *description;        /* 函数块描述 */
} PackConfig;

/* ============== 函数块管理API ============== */

/**
 * @brief 创建函数块
 * @param id 函数块ID
 * @return 函数块指针，失败返回NULL
 */
FuncBlock *func_block_create(int id);

/**
 * @brief 销毁函数块
 * @param fb 函数块指针（可为NULL）
 */
void func_block_destroy(FuncBlock *fb);

/**
 * @brief 设置内部节点
 * @param fb 函数块
 * @param node_ids 节点ID数组
 * @param count 节点数量
 * @return true 成功，false 失败
 */
bool func_block_set_internal_nodes(FuncBlock *fb, const int *node_ids, int count);

/**
 * @brief 设置输入端口
 * @param fb 函数块
 * @param port_ids 端口ID数组
 * @param count 端口数量
 * @return true 成功，false 失败
 */
bool func_block_set_input_ports(FuncBlock *fb, const int *port_ids, int count);

/**
 * @brief 设置输出端口
 * @param fb 函数块
 * @param port_ids 端口ID数组
 * @param count 端口数量
 * @return true 成功，false 失败
 */
bool func_block_set_output_ports(FuncBlock *fb, const int *port_ids, int count);

/**
 * @brief 设置多解选择器
 * @param fb 函数块
 * @param selector 选择器（函数块接管所有权）
 * @return true 成功，false 失败
 */
bool func_block_set_selector(FuncBlock *fb, SolutionSelector *selector);

/**
 * @brief 添加端口依赖
 * @param fb 函数块
 * @param dep 端口依赖
 * @return true 成功，false 失败
 */
bool func_block_add_port_dependency(FuncBlock *fb, PortDependency *dep);

/**
 * @brief 设置前置条件
 * @param fb 函数块
 * @param region_ids 区域ID数组
 * @param count 区域数量
 * @return true 成功，false 失败
 */
bool func_block_set_preconditions(FuncBlock *fb, const int *region_ids, int count);

/**
 * @brief 设置函数块名称
 * @param fb 函数块
 * @param name 名称
 * @return true 成功，false 失败
 */
bool func_block_set_name(FuncBlock *fb, const char *name);

/**
 * @brief 设置函数块描述
 * @param fb 函数块
 * @param description 描述
 * @return true 成功，false 失败
 */
bool func_block_set_description(FuncBlock *fb, const char *description);

/**
 * @brief 获取输入端口数量
 * @param fb 函数块指针
 * @return 输入端口数量，fb 为 NULL 时返回 0
 */
int func_block_get_input_count(const FuncBlock *fb);

/**
 * @brief 获取输出端口数量
 * @param fb 函数块指针
 * @return 输出端口数量，fb 为 NULL 时返回 0
 */
int func_block_get_output_count(const FuncBlock *fb);

/**
 * @brief 获取内部节点数量
 * @param fb 函数块指针
 * @return 内部节点数量，fb 为 NULL 时返回 0
 */
int func_block_get_internal_count(const FuncBlock *fb);

/**
 * @brief 获取函数块ID
 * @param fb 函数块指针
 * @return 函数块ID，fb 为 NULL 时返回 -1
 */
int func_block_get_id(const FuncBlock *fb);

/**
 * @brief 获取确定性状态
 * @param fb 函数块指针
 * @return 确定性状态，fb 为 NULL 时返回 DETERMINISM_UNVERIFIED
 */
DeterminismState func_block_get_determinism(const FuncBlock *fb);

/**
 * @brief 获取函数块名称
 * @param fb 函数块指针
 * @return 名称字符串（只读），fb 为 NULL 或无名称时返回 NULL
 */
const char *func_block_get_name(const FuncBlock *fb);

/**
 * @brief 获取函数块描述
 * @param fb 函数块指针
 * @return 描述字符串（只读），fb 为 NULL 或无描述时返回 NULL
 */
const char *func_block_get_description(const FuncBlock *fb);

/**
 * @brief 深拷贝函数块
 *
 * 创建一个函数块的完整深拷贝，包括所有动态分配的成员。
 * 拷贝后的函数块与原始函数块完全独立，修改其中一个不会影响另一个。
 *
 * @param src 源函数块
 * @return 新创建的函数块副本，失败返回 NULL
 */
FuncBlock *func_block_copy(const FuncBlock *src);

/* ============== 打包操作 ============== */

/**
 * @brief 检测跨边界约束
 * @param graph 约束图
 * @param internal_node_ids 内部节点ID数组
 * @param internal_count 内部节点数量
 * @param out_conflicts 输出的跨边界约束数组
 * @param out_conflict_count 输出的跨边界约束数量
 * @return 是否存在跨边界约束
 */
bool func_block_detect_cross_boundary(
    ConstraintGraph *graph,
    const int *internal_node_ids,
    int internal_count,
    CrossBoundaryConstraint **out_conflicts,
    int *out_conflict_count
);

/**
 * @brief 执行打包操作（简化版API）
 * @param graph 约束图
 * @param config 打包配置
 * @param out_func_block 输出的函数块
 * @return 打包结果
 */
PackResult func_block_pack_ex(
    ConstraintGraph *graph,
    const PackConfig *config,
    FuncBlock **out_func_block
);

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
 */
PackResult func_block_pack(
    ConstraintGraph *graph,
    const int *internal_node_ids,
    int internal_count,
    const int *input_port_ids,
    int input_count,
    const int *output_port_ids,
    int output_count,
    CrossBoundaryAction *cross_boundary_actions,
    int cross_boundary_count,
    FuncBlock **out_func_block
);

/* ============== 确定性检查 ============== */



/**
 * @brief 静态确定性检查（增强版）
 * @param fb 函数块
 * @param graph 约束图（const，不修改图）
 * @return 确定性状态
 */
DeterminismStatus func_block_determinism_check_static(
    FuncBlock *fb,
    const ConstraintGraph *graph
);

/**
 * @brief 动态确定性检查（增强版）
 * @param fb 函数块
 * @param graph 约束图（可修改）
 * @param input_values 实参的符号坐标值数组
 * @param n_inputs 实参数量
 * @return 确定性状态
 */
DeterminismStatus func_block_determinism_check_dynamic(
    FuncBlock *fb,
    ConstraintGraph *graph,
    const SymbolicCoord **input_values,
    int n_inputs
);

/**
 * @brief 完整的确定性验证流水线
 * @param fb 函数块
 * @param graph 约束图
 * @param step_limit 静态分析的最大步数
 * @return 最终的确定性状态
 */
DeterminismState func_block_verify_determinism(
    FuncBlock *fb, 
    ConstraintGraph *graph, 
    int step_limit
);

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
 */
InstantiateResult func_block_instantiate(
    FuncBlock *fb,
    ConstraintGraph *graph,
    int *arg_mappings,
    int arg_count,
    int **out_new_node_ids,
    int *out_new_node_count
);

/**
 * @brief 部分应用（柯里化）
 * @param fb 原函数块
 * @param graph 约束图
 * @param fixed_arg_mappings 固定的实参映射
 * @param fixed_count 固定的实参数量
 * @param out_new_fb 输出的新函数块
 * @return 是否成功
 */
bool func_block_partial_apply(
    FuncBlock *fb,
    ConstraintGraph *graph,
    int *fixed_arg_mappings,
    int fixed_count,
    FuncBlock **out_new_fb
);

/**
 * @brief 执行捕获避免的例化
 * @param block 函数块
 * @param actual_arg_nodes 实参节点ID数组
 * @param arg_count 实参数量
 * @param target_graph 目标约束图
 * @param output_node_ids 输出：内部节点到外部节点ID的映射
 * @param output_node_count 输出：映射数量
 * @return 例化结果
 */
InstantiateResult func_block_instantiate_capture_avoiding(
    FuncBlock *block,
    const int *actual_arg_nodes,
    int arg_count,
    ConstraintGraph *target_graph,
    int **output_node_ids,
    int *output_node_count
);

/* ============== 多解选择 ============== */

/**
 * @brief 创建选择器
 * @param type 选择器类型
 * @return 选择器指针，失败返回NULL
 */
SolutionSelector *selector_create(SelectorType type);

/**
 * @brief 创建带参考的选择器
 * @param type 选择器类型
 * @param reference_node_id 参考节点ID
 * @return 选择器指针，失败返回NULL
 */
SolutionSelector *selector_create_with_reference(SelectorType type, int reference_node_id);

/**
 * @brief 创建自定义选择器
 * @param func 自定义选择函数
 * @param user_data 用户数据
 * @return 选择器指针，失败返回NULL
 */
SolutionSelector *selector_create_custom(SelectorFunction func, void *user_data);

/**
 * @brief 销毁选择器
 * @param selector 选择器指针（可为NULL）
 */
void selector_destroy(SolutionSelector *selector);

/**
 * @brief 设置选择器关联的约束图
 * @param selector 选择器实例
 * @param graph 约束图指针
 */
void selector_set_graph(SolutionSelector *selector, ConstraintGraph *graph);

/**
 * @brief 应用选择器
 * @param selector 选择器
 * @param candidates 候选解数组
 * @param count 候选解数量
 * @param out_selected_index 输出的选中索引
 * @return 是否成功选择
 */
bool selector_apply(
    SolutionSelector *selector,
    GeomNode **candidates,
    int count,
    int *out_selected_index
);

/* ============== 函数块组合子 ============== */

/**
 * @brief 组合两个函数块：g ∘ f
 * @param f 第一个函数块
 * @param g 第二个函数块
 * @param graph 约束图
 * @param out_composed 输出的组合函数块
 * @return 是否成功
 */
bool func_block_compose(
    FuncBlock *f,
    FuncBlock *g,
    ConstraintGraph *graph,
    FuncBlock **out_composed
);

/**
 * @brief 乘积两个函数块：f × g
 * @param f 第一个函数块
 * @param g 第二个函数块
 * @param graph 约束图
 * @param out_product 输出的乘积函数块
 * @return 是否成功
 */
bool func_block_product(
    FuncBlock *f,
    FuncBlock *g,
    ConstraintGraph *graph,
    FuncBlock **out_product
);

/* ============== 辅助函数 ============== */

/**
 * @brief 获取确定性状态字符串
 * @param state 确定性状态
 * @return 状态字符串
 */
const char *determinism_state_to_string(DeterminismState state);

/**
 * @brief 获取打包结果字符串
 * @param result 打包结果
 * @return 结果字符串
 */
const char *pack_result_to_string(PackResult result);

/**
 * @brief 获取例化结果字符串
 * @param result 例化结果
 * @return 结果字符串
 */
const char *instantiate_result_to_string(InstantiateResult result);

/* ============== 确定性状态持久化 ============== */

/**
 * @brief 序列化函数块的确定性状态
 * @param fb 函数块
 * @return 序列化字符串（调用者负责free），失败返回NULL
 */
char *func_block_serialize_state(const FuncBlock *fb);

/**
 * @brief 反序列化函数块的确定性状态
 * @param fb 函数块
 * @param data 序列化数据字符串
 * @return 是否成功
 */
bool func_block_deserialize_state(FuncBlock *fb, const char *data);

/* ============== 视图折叠/展开（API层） ============== */

/**
 * @brief 设置函数块的视图状态
 * @param fb 函数块
 * @param state 视图状态
 */
void func_block_set_view_state(FuncBlock *fb, FuncBlockViewState state);

/**
 * @brief 获取函数块的视图状态
 * @param fb 函数块
 * @return 视图状态
 */
FuncBlockViewState func_block_get_view_state(const FuncBlock *fb);

/* ============== 打包冲突对话框（API层） ============== */

/**
 * @brief 设置跨边界约束处理回调
 * @param cb 回调函数，传NULL取消回调
 * @param user_data 传递给回调的用户数据
 */
void func_block_set_cross_boundary_callback(CrossBoundaryCallback cb, void *user_data);

/**
 * @brief 设置函数块系统的流式输出上下文
 * @param ctx 流式上下文（可为NULL禁用流式输出）
 */
void func_block_set_stream_context(StreamContext *ctx);

#ifdef __cplusplus
}
#endif

#endif /* LV00_FUNC_BLOCK_H */
