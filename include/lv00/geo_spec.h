/**
 * @file geo_spec.h
 * @brief 几何构造规约层 —— 借鉴 TLA+ 的 Init/Next/Invariant 三段式框架
 *
 * 设计借鉴来源：
 * - TLA+ (github.com/tlaplus/tlaplus) — Leslie Lamport 的时序逻辑规约语言
 *   · Init/Next/Spec 三段式规约范式
 *   · TLC 模型检查器的穷举状态搜索
 *   · TLAPS 证明管理器的层次化证明
 *
 * 核心设计理念：
 * 将几何构造建模为状态变迁系统：
 *   Init: 初始几何体声明（点、线、圆的基本配置）
 *   Next: 构造步骤（作垂线、作平行线、作交点等）
 *   Spec: 完整的构造序列 + 所需验证的不变式
 *
 * @version v3.3.0
 * @date 2026-05-24
 */

#ifndef LV00_GEO_SPEC_H
#define LV00_GEO_SPEC_H

#include "lv00.h"
#include "constraint_graph.h"
#include "proof.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 几何构造状态
 *
 * 表示构造过程的一个快照：当前约束图状态、指纹和从 Init 到当前的步数。
 */
typedef struct GeoConstructionState {
    ConstraintGraph *graph;           /**< 当前约束图状态 */
    uint64_t         fingerprint;     /**< 状态指纹（哈希摘要） */
    int              depth;           /**< 从 Init 到当前状态的步数 */
} GeoConstructionState;

/**
 * @brief 几何构造步骤类型
 *
 * 枚举所有合法的几何构造操作，涵盖点、线、圆、交点、垂线、平行线、
 * 中点、角平分线、测量、约束施加和撤销。
 */
typedef enum {
    GEO_STEP_POINT,          /**< 定义点 */
    GEO_STEP_LINE,           /**< 定义线 */
    GEO_STEP_CIRCLE,         /**< 定义圆 */
    GEO_STEP_INTERSECTION,   /**< 求交点 */
    GEO_STEP_PERPENDICULAR,  /**< 作垂线 */
    GEO_STEP_PARALLEL,       /**< 作平行线 */
    GEO_STEP_MIDPOINT,       /**< 求中点 */
    GEO_STEP_BISECTOR,       /**< 作角平分线 */
    GEO_STEP_MEASURE,        /**< 测量（距离/角度） */
    GEO_STEP_CONSTRAINT,     /**< 施加约束 */
    GEO_STEP_UNDO            /**< 撤销 */
} GeoStepType;

/**
 * @brief 几何构造步骤
 *
 * 单个构造操作，包含类型、涉及的节点、人类可读描述。
 */
typedef struct GeoStep {
    int          step_id;       /**< 步骤唯一标识符 */
    GeoStepType  type;          /**< 步骤类型 */
    char        *label;         /**< 步骤标签（可空） */
    int         *node_ids;      /**< 涉及的节点 ID 数组 */
    int          node_count;    /**< 节点数量 */
    char        *description;   /**< 人类可读描述 */
} GeoStep;

/**
 * @brief 几何构造规约
 *
 * 完整构造规约 = Init（初始配置） + Next（步骤列表） + Invariant（不变式列表）。
 * 对应 TLA+ 的 Spec == Init /\ [][Next]_vars /\ Invariant。
 */
typedef struct GeoConstructionSpec {
    ConstraintGraph *initial;       /**< 初始几何配置 */
    GeoStep         *steps;         /**< 构造步骤列表 */
    int              step_count;    /**< 当前步骤数量 */
    int              step_capacity; /**< 步骤数组容量 */
    char           **invariants;    /**< 不变式（命题表达式字符串数组） */
    int              invariant_count; /**< 不变式数量 */
} GeoConstructionSpec;

/**
 * @brief 几何不变式类型
 *
 * 对应构造中需要保持不变的几何性质。
 */
typedef enum {
    GEO_INV_COLLINEARITY,     /**< 共线性 */
    GEO_INV_CONCURRENCY,      /**< 共点性 */
    GEO_INV_PARALLELISM,      /**< 平行性 */
    GEO_INV_PERPENDICULARITY, /**< 垂直性 */
    GEO_INV_DISTANCE_EQ,      /**< 距离相等 */
    GEO_INV_ANGLE_EQ,         /**< 角度相等 */
    GEO_INV_RATIO,            /**< 比例关系 */
    GEO_INV_CONTAINMENT,      /**< 包含关系 */
    GEO_INV_CUSTOM            /**< 自定义不变式 */
} GeoInvariantType;

/**
 * @brief 几何不变式
 *
 * 表示构造过程中必须恒为真的命题。
 */
typedef struct GeoInvariant {
    int              inv_id;      /**< 不变式唯一标识符 */
    GeoInvariantType type;        /**< 不变式类型 */
    char            *expression;  /**< 不变式表达式字符串 */
    int             *node_ids;    /**< 涉及的节点 ID 数组 */
    int              node_count;  /**< 节点数量 */
    bool             is_core;     /**< 是否为核心不变量 */
} GeoInvariant;

/**
 * @brief 状态搜索策略
 *
 * 模型检查器遍历状态空间的搜索顺序。
 */
typedef enum {
    GEO_SEARCH_BFS = 0,  /**< 广度优先搜索 */
    GEO_SEARCH_DFS = 1   /**< 深度优先搜索 */
} GeoSearchStrategy;

/**
 * @brief 状态空间搜索器
 *
 * 基于 TLC 模型检查器的穷举状态搜索，维护 BFS/DFS 队列和已见指纹集合。
 */
typedef struct StateSpaceExplorer {
    GeoConstructionState **queue;          /**< 状态队列（BFS/DFS） */
    int         queue_head;                /**< 队列头指针 */
    int         queue_tail;                /**< 队列尾指针 */
    int         queue_capacity;            /**< 队列容量 */
    uint64_t   *seen_fingerprints;         /**< 已见指纹数组 */
    int         seen_count;                /**< 已见状态数量 */
    int         seen_capacity;             /**< 指纹数组容量 */
    int         total_states_explored;     /**< 已探索状态总数 */
    int         max_depth;                 /**< 最大搜索深度 */
    GeoSearchStrategy strategy;            /**< 搜索策略 */
} StateSpaceExplorer;

/**
 * @brief 反例
 *
 * 当不变式被违反时，记录从初始状态到违规状态的完整路径。
 */
typedef struct CounterExample {
    GeoConstructionState *states;          /**< 从 Init 到违规状态的路径 */
    int  state_count;                      /**< 路径中的状态数量 */
    int  violated_invariant_id;            /**< 被违反的不变式 ID */
    char *description;                     /**< 人类可读描述 */
} CounterExample;

/* ── 构造规约 API ── */

/**
 * @brief 创建几何构造规约
 *
 * 分配并初始化一个 GeoConstructionSpec，以给定的约束图作为初始状态。
 *
 * @param initial  初始约束图（规约会取得所有权，调用者不应再修改它）
 * @return 新分配的规约指针，失败返回 NULL
 */
GeoConstructionSpec* geo_spec_create(ConstraintGraph *initial);

/**
 * @brief 销毁几何构造规约
 *
 * 释放规约及其所有步骤、不变式字符串和初始图。
 *
 * @param spec  规约指针（可为 NULL）
 */
void  geo_spec_destroy(GeoConstructionSpec *spec);

/**
 * @brief 向规约添加构造步骤
 *
 * 在步骤列表末尾追加一条构造步骤。步骤按添加顺序构成 Next 关系。
 *
 * @param spec       规约指针
 * @param type       步骤类型
 * @param label      步骤标签（可空）
 * @param node_ids   涉及的节点 ID 数组
 * @param count      节点数量
 * @return 新步骤的索引（>= 0），失败返回 -1
 */
int   geo_spec_add_step(GeoConstructionSpec *spec, GeoStepType type,
                         const char *label, const int *node_ids, int count);

/**
 * @brief 向规约添加不变式
 *
 * 将命题表达式字符串添加为规约需要验证的不变式。
 *
 * @param spec        规约指针
 * @param type        不变式类型
 * @param expression  命题表达式字符串（内部复制）
 * @return 新不变式的索引（>= 0），失败返回 -1
 */
int   geo_spec_add_invariant(GeoConstructionSpec *spec, GeoInvariantType type,
                              const char *expression);

/* ── 状态空间搜索器 API ── */

/**
 * @brief 创建状态空间搜索器
 *
 * 分配并初始化搜索器，指定初始队列容量和搜索策略。
 *
 * @param capacity   初始队列和指纹集合容量
 * @param strategy   搜索策略（BFS 或 DFS）
 * @return 新分配的搜索器指针，失败返回 NULL
 */
StateSpaceExplorer* geo_explorer_create(int capacity, GeoSearchStrategy strategy);

/**
 * @brief 销毁状态空间搜索器
 *
 * 释放搜索器及其内部所有队列状态和指纹集合。
 *
 * @param explorer  搜索器指针（可为 NULL）
 */
void  geo_explorer_destroy(StateSpaceExplorer *explorer);

/* ── 模型检查 API ── */

/**
 * @brief 执行模型检查
 *
 * 借鉴 TLC 的穷举状态搜索：从 Init 出发，枚举所有可能的 Next 步骤，
 * 在每个状态下检查所有不变式。若发现违反，将反例填入 out_counter。
 *
 * @param explorer      状态空间搜索器
 * @param spec          构造规约
 * @param invariants    不变式数组
 * @param inv_count     不变式数量
 * @param out_counter   输出：反例（若检查通过则不修改）
 * @return true 所有不变式在所有可达状态下成立，false 找到反例
 */
bool  geo_model_check(StateSpaceExplorer *explorer, GeoConstructionSpec *spec,
                      GeoInvariant *invariants, int inv_count,
                      CounterExample *out_counter);

/**
 * @brief 创建空反例
 *
 * @return 新分配的反例指针，失败返回 NULL
 */
CounterExample* geo_counterexample_create(void);

/**
 * @brief 销毁反例
 *
 * 释放反例及其内部路径状态。
 *
 * @param ce  反例指针（可为 NULL）
 */
void  geo_counterexample_destroy(CounterExample *ce);

/**
 * @brief 检查单个不变式在给定图状态下是否成立
 *
 * @param inv   不变式
 * @param graph 约束图状态
 * @return true 成立，false 不成立
 */
bool  geo_invariant_check(GeoInvariant *inv, ConstraintGraph *graph);

/* ── 状态管理 API ── */

/**
 * @brief 创建几何构造状态
 *
 * 深拷贝约束图并计算初始指纹。
 *
 * @param graph  源约束图
 * @param depth  当前构造深度
 * @return 新分配的状态，失败返回 NULL
 */
GeoConstructionState* geo_state_create(ConstraintGraph *graph, int depth);

/**
 * @brief 销毁几何构造状态
 *
 * @param state  状态指针（可为 NULL）
 */
void  geo_state_destroy(GeoConstructionState *state);

/**
 * @brief 计算状态的指纹哈希
 *
 * 基于约束图的规范化形式生成确定性 64 位哈希值。
 *
 * @param state  状态
 * @return 64 位指纹值
 */
uint64_t geo_state_fingerprint(GeoConstructionState *state);

/**
 * @brief 在给定状态下应用构造步骤
 *
 * 执行步步骤对状态的变换，产生下一个状态（Next 关系）。
 *
 * @param step       构造步骤
 * @param state      当前状态
 * @param out_next   输出：下一个状态（调用者负责销毁）
 * @return true 步骤合法且成功应用，false 失败
 */
bool  geo_step_apply(GeoStep *step, GeoConstructionState *state,
                     GeoConstructionState *out_next);

/* ── 导出 API ── */

/**
 * @brief 将构造规约导出为 TLA+ 格式
 *
 * 生成可被 TLC 模型检查器直接验证的 TLA+ 模块字符串，
 * 包含 Init、Next、Spec 和所有不变式的完整定义。
 *
 * @param spec  构造规约
 * @return TLA+ 模块字符串（调用者负责 free），失败返回 NULL
 */
char* geo_spec_export_tlaplus(GeoConstructionSpec *spec);

#ifdef __cplusplus
}
#endif

#endif /* LV00_GEO_SPEC_H */
