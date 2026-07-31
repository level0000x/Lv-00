/**
 * @file relation_model.h
 * @brief 关系模型层公共接口 —— Alloy 风格的"关系即一切"统一建模范式
 *
 * 提供：
 *   - 关系原子（RelAtom）/签名（RelSignature）的类型定义
 *   - 关系（Relation）数据结构：元组集合 + 域签名引用
 *   - 关系表达式（RelExpr）与 13 种关系运算符（RelOp）
 *   - 逻辑公式（RelFormula）与 12 种公式类型（RelFormulaType）
 *   - 关系模型（RelModel）：签名集合 + 关系集合 + 事实/断言
 *   - 关系实例（RelInstance）：模型的具体绑定
 *   - 有限范围配置（SmallScopeConfig）
 *   - SAT 编码相关类型（SatVarEntry / SatLiteral / SatEncoding / SatModel / SatResult）
 *
 * @version 1.1.0
 * @date 2026-05-24
 */

#ifndef lv_RELATION_MODEL_H
#define lv_RELATION_MODEL_H

#include "lv/lv_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 前向声明
 * ======================================================================== */

struct ConstraintGraph;
typedef struct ConstraintGraph ConstraintGraph;

/* ========================================================================
 * 关系原子类型枚举
 * ======================================================================== */

/** @brief 关系原子类型，与 GeomType 一一对应 */
typedef enum {
    REL_ATOM_POINT = 0,      /**< 几何点 */
    REL_ATOM_LINE = 1,       /**< 线段 */
    REL_ATOM_REGION = 2,     /**< 区域 */
    REL_ATOM_PORT = 3,       /**< 端口 */
    REL_ATOM_FUNC_BLOCK = 4, /**< 函数块 */
    REL_ATOM_UNKNOWN = 99    /**< 未知类型 */
} RelAtomType;

/* ========================================================================
 * 关系数据结构
 * ======================================================================== */

/** @brief 关系原子：关系模型中的基本个体 */
typedef struct RelAtom {
    int atom_id;       /**< 原子唯一标识符 */
    RelAtomType type;  /**< 原子类型 */
    char *label;       /**< 可选的人类可读标签 */
    int graph_node_id; /**< 对应约束图节点的 ID */
} RelAtom;

/** @brief 关系签名（Sig）：Alloy 中对应类型的抽象，包含一组同质原子 */
typedef struct RelSignature {
    char *name;                     /**< 签名名称（如 "Point", "LineSegment"） */
    RelAtomType atom_type;          /**< 签名对应的原子类型 */
    RelAtom **atoms;                /**< 签名包含的原子数组 */
    int atom_count;                 /**< 当前原子数量 */
    int atom_capacity;              /**< 原子数组容量 */
    bool is_abstract;               /**< 是否为抽象签名 */
    struct RelSignature **sub_sigs; /**< 子签名数组（继承用） */
    int sub_sig_count;              /**< 子签名数量 */
} RelSignature;

/**
 * @brief 关系：一组同元数元组的集合
 *
 * 每个元组由 int 数组表示，arity 指定元组长度。
 * domains 数组记录每列对应的域签名，用于补集等运算。
 */
typedef struct Relation {
    char *name;               /**< 关系名称 */
    int arity;                /**< 元数（每个元组的元素个数） */
    lvDArray tuples;          /**< 元组指针数组 (int *) */
    RelSignature *domains[8]; /**< 每列对应的域签名（最多 8 列） */
} Relation;

/* ========================================================================
 * 关系表达式类型与运算符
 * ======================================================================== */

/** @brief 关系表达式类型 */
typedef enum {
    REL_EXPR_ATOMIC,   /**< 原子表达式：直接引用一个关系 */
    REL_EXPR_COMPOSITE /**< 复合表达式：二元/一元运算 */
} RelExprType;

/** @brief 关系运算符（13 种） */
typedef enum {
    REL_OP_UNION = 0,          /**< 并集（R + S） */
    REL_OP_INTERSECTION,       /**< 交集（R & S） */
    REL_OP_DIFFERENCE,         /**< 差集（R - S） */
    REL_OP_JOIN,               /**< 关系连接（R.S） */
    REL_OP_PRODUCT,            /**< 笛卡尔积（R -> S） */
    REL_OP_TRANSPOSE,          /**< 转置（~R） */
    REL_OP_TRANSITIVE_CLOSURE, /**< 传递闭包（^R） */
    REL_OP_REFL_TRANS_CLOSURE, /**< 自反传递闭包（*R） */
    REL_OP_IDENTITY,           /**< 恒等关系（iden） */
    REL_OP_COMPLEMENT,         /**< 补集（~R 相对于全域） */
    REL_OP_RESTRICT_DOMAIN,    /**< 域约束（S <: R） */
    REL_OP_RESTRICT_RANGE,     /**< 值域约束（R :> S） */
    REL_OP_OVERRIDE            /**< 覆盖（R ++ S） */
} RelOp;

/** @brief 关系表达式（递归结构） */
typedef struct RelExpr {
    RelExprType type; /**< 表达式类型 */
    union {
        struct {
            Relation *rel; /**< 原子表达式引用的关系 */
        } atomic;
        struct {
            RelOp op;              /**< 运算符 */
            struct RelExpr *left;  /**< 左操作数 */
            struct RelExpr *right; /**< 右操作数（一元运算时为 NULL） */
        } composite;
    } data;
} RelExpr;

/* ========================================================================
 * 逻辑公式类型
 * ======================================================================== */

/** @brief 逻辑公式类型（12 种） */
typedef enum {
    REL_FORMULA_FORALL = 0, /**< 全称量化（all x | F） */
    REL_FORMULA_EXISTS,     /**< 存在量化（some x | F） */
    REL_FORMULA_NO,         /**< 不存在量化（no x | F） */
    REL_FORMULA_SOME,       /**< 至少一个（some R） */
    REL_FORMULA_LONE,       /**< 至多一个（lone R） */
    REL_FORMULA_ONE,        /**< 恰好一个（one R） */
    REL_FORMULA_EQ,         /**< 关系相等（R = S） */
    REL_FORMULA_SUBSET,     /**< 子集（R in S） */
    REL_FORMULA_AND,        /**< 合取（F1 && F2） */
    REL_FORMULA_OR,         /**< 析取（F1 || F2） */
    REL_FORMULA_NOT,        /**< 否定（!F） */
    REL_FORMULA_IMPLIES     /**< 蕴含（F1 => F2） */
} RelFormulaType;

/** @brief 逻辑公式（递归结构） */
typedef struct RelFormula {
    RelFormulaType type;     /**< 公式类型 */
    RelExpr *expr;           /**< 关联的关系表达式（量词/比较公式使用） */
    struct RelFormula **sub; /**< 子公式数组（AND/OR/NOT/IMPLIES 等使用） */
    int sub_count;           /**< 子公式数量 */
    RelSignature *quant_sig; /**< 量化签名（FORALL/EXISTS 使用） */
} RelFormula;

/* ========================================================================
 * 关系模型与实例
 * ======================================================================== */

/**
 * @brief 关系模型：Alloy 世界模型
 *
 * 包含一组签名（类型）、一组命名关系、一组事实和一组断言。
 */
typedef struct RelModel {
    lvDArray sigs;         /**< 签名指针数组 (RelSignature *) */

    Relation **relations;  /**< 命名关系数组 */
    int relation_count;    /**< 关系数量 */
    int relation_capacity; /**< 关系数组容量 */

    RelFormula **facts; /**< 事实公式数组 */
    int fact_count;     /**< 事实数量 */

    RelFormula **assertions; /**< 断言公式数组 */
    int assertion_count;     /**< 断言数量 */

    int max_point_count;      /**< 有限范围：最大点数 */
    int max_line_count;       /**< 有限范围：最大线段数 */
    int max_region_count;     /**< 有限范围：最大区域数 */
    int max_func_block_count; /**< 有限范围：最大函数块数 */
} RelModel;

/**
 * @brief 关系实例：模型的一个具体绑定
 *
 * 将模型中的每个关系绑定到具体的元组集合。
 */
typedef struct RelInstance {
    const RelModel *model;     /**< 所属的关系模型 */
    RelAtom **atoms;           /**< 实例中的所有原子 */
    int atom_count;            /**< 原子数量 */
    Relation **rel_bindings;   /**< 关系绑定数组 */
    int binding_count;         /**< 绑定数量 */
    bool satisfies_assertions; /**< 是否满足所有断言 */
} RelInstance;

/* ========================================================================
 * 有限范围配置
 * ======================================================================== */

/** @brief 有限范围配置：控制每个签名中允许的最大原子数量 */
typedef struct SmallScopeConfig {
    int max_points;      /**< 最大点数 */
    int max_lines;       /**< 最大线段数 */
    int max_regions;     /**< 最大区域数 */
    int max_ports;       /**< 最大端口数 */
    int max_func_blocks; /**< 最大函数块数 */
    int max_total_atoms; /**< 所有签名原子总数上限 */
} SmallScopeConfig;

/* ========================================================================
 * SAT 编码相关类型
 * ======================================================================== */

/** @brief SAT 求解结果 */
typedef enum {
    SAT_OK = 0,      /**< 成功 */
    SAT_UNSAT = 1,   /**< 不可满足 */
    SAT_UNKNOWN = 2, /**< 未知（求解器无法确定） */
    SAT_ERROR = -1   /**< 错误 */
} SatResult;

/** @brief SAT 文字（literal）：带符号的变量 ID，正值表示正文字，负值表示负文字 */
typedef int SatLiteral;

/**
 * @brief SAT 变量映射条目
 *
 * 将关系元组（原子 ID 序列）映射到 SAT 变量 ID。
 */
typedef struct SatVarEntry {
    int var_id;      /**< SAT 变量 ID（>= 1） */
    int arity;       /**< 元组元数 */
    int atom_ids[8]; /**< 原子 ID 数组（最多 8 个） */
} SatVarEntry;

/**
 * @brief SAT 编码上下文
 *
 * 管理从关系模型/约束图到 CNF 子句的编码过程。
 */
typedef struct SatEncoding {
    SatVarEntry *var_map; /**< 变量映射表 */
    int var_count;        /**< 已注册变量数 */
    int var_capacity;     /**< 变量映射表容量 */
    int next_var_id;      /**< 下一个可用的变量 ID */

    int **clauses;       /**< CNF 子句数组 */
    int *clause_sizes;   /**< 每个子句的文字数量 */
    int clause_count;    /**< 子句数量 */
    int clause_capacity; /**< 子句数组容量 */

    int total_vars;        /**< 总变量数（统计用） */
    int total_clauses;     /**< 总子句数（统计用） */
    double encode_time_ms; /**< 编码耗时（毫秒） */

    const ConstraintGraph *graph; /**< 关联的约束图（可为 NULL） */
    const RelModel *rel_model;    /**< 关联的关系模型（可为 NULL） */
} SatEncoding;

/**
 * @brief SAT 模型：求解结果
 *
 * 包含赋值为真的变量列表，以及可选的解码结果。
 */
typedef struct SatModel {
    int var_count;                  /**< 总变量数 */
    int *true_vars;                 /**< 赋值为真的变量 ID 数组 */
    int true_count;                 /**< 赋值为真的变量数量 */
    ConstraintGraph *decoded_graph; /**< 解码后的约束图（可为 NULL） */
    RelInstance *decoded_instance;  /**< 解码后的关系实例（可为 NULL） */
} SatModel;

/* ========================================================================
 * 关系运算符 API
 * ======================================================================== */

/** @name 13 种关系运算符 */
/**@{*/

lv_PUBLIC_API Relation *rel_union(const Relation *a, const Relation *b);
lv_PUBLIC_API Relation *rel_intersection(const Relation *a, const Relation *b);
lv_PUBLIC_API Relation *rel_difference(const Relation *a, const Relation *b);
lv_PUBLIC_API Relation *rel_join(const Relation *a, const Relation *b);
lv_PUBLIC_API Relation *rel_product(const Relation *a, const Relation *b);
lv_PUBLIC_API Relation *rel_transpose(const Relation *r);
lv_PUBLIC_API Relation *rel_transitive_closure(const Relation *r);
lv_PUBLIC_API Relation *rel_reflexive_transitive_closure(const Relation *r);

/**@}*/

/* ========================================================================
 * 关系模型构建 API
 * ======================================================================== */

/**
 * @brief 从约束图构建关系模型
 *
 * 为每种 GeomType 创建对应的 RelSignature，为每个图节点创建 RelAtom。
 *
 * @param graph  源约束图
 * @return 新创建的关系模型，失败返回 NULL
 */
lv_PUBLIC_API RelModel *relation_model_from_graph(const ConstraintGraph *graph);

/**
 * @brief 销毁关系模型，释放所有关联内存
 */
lv_PUBLIC_API void relation_model_destroy(RelModel *model);

/**
 * @brief 向模型添加事实公式
 */
lv_PUBLIC_API bool relation_model_add_fact(RelModel *model, RelFormula *formula);

/**
 * @brief 向模型添加断言公式
 */
lv_PUBLIC_API bool relation_model_add_assertion(RelModel *model, RelFormula *formula);

/* ========================================================================
 * 可满足性检查与实例查找
 * ======================================================================== */

/**
 * @brief 检查关系模型在给定范围下是否可满足
 *
 * @param model  关系模型
 * @param scope  有限范围配置
 * @return true 可满足，false 不可满足或出错
 */
lv_PUBLIC_API bool relation_check_satisfiability(RelModel *model, const SmallScopeConfig *scope);

/**
 * @brief 在给定范围下查找关系模型的一个实例
 *
 * @param model       关系模型
 * @param scope       有限范围配置
 * @param assertions  是否验证断言
 * @return 实例指针（调用方需用 relation_instance_destroy 释放），失败返回 NULL
 */
lv_PUBLIC_API RelInstance *relation_find_instance(RelModel *model, const SmallScopeConfig *scope, bool assertions);

/**
 * @brief 销毁关系实例
 */
lv_PUBLIC_API void relation_instance_destroy(RelInstance *inst);

/* ========================================================================
 * 关系表达式求值
 * ======================================================================== */

/**
 * @brief 在给定实例下求值关系表达式
 *
 * @param model  关系模型
 * @param inst   关系实例
 * @param expr   关系表达式
 * @return 求值结果关系（调用方需释放），失败返回 NULL
 */
lv_PUBLIC_API Relation *relation_evaluate_expr(const RelModel *model, const RelInstance *inst, const RelExpr *expr);

/* ========================================================================
 * 公式评估
 * ======================================================================== */

/**
 * @brief 在给定实例下评估逻辑公式
 *
 * @param model   关系模型
 * @param inst    关系实例
 * @param formula 逻辑公式
 * @return true 公式成立，false 不成立或出错
 */
lv_PUBLIC_API bool relation_evaluate_formula(const RelModel *model, const RelInstance *inst, const RelFormula *formula);

/* ========================================================================
 * 导出 API
 * ======================================================================== */

/**
 * @brief 将关系模型导出为 Alloy 源码格式的字符串
 *
 * @param model  关系模型
 * @return Alloy 源码字符串（调用方需用 lv_free 释放），失败返回 NULL
 */
lv_PUBLIC_API char *relation_model_export_alloy(const RelModel *model);

/**
 * @brief 将关系实例导出为 XML 格式的字符串
 *
 * @param inst  关系实例
 * @return XML 字符串（调用方需用 lv_free 释放），失败返回 NULL
 */
lv_PUBLIC_API char *relation_instance_export_xml(const RelInstance *inst);

/* ========================================================================
 * SAT 编码 API
 * ======================================================================== */

/**
 * @brief 创建 SAT 编码上下文
 */
lv_PUBLIC_API SatEncoding *sat_encoding_create(int initial_var_capacity, int initial_clause_capacity);

/**
 * @brief 销毁 SAT 编码上下文
 */
lv_PUBLIC_API void sat_encoding_destroy(SatEncoding *enc);

/**
 * @brief 注册变量：将关系元组映射到 SAT 变量
 *
 * @return 变量 ID（>= 1），失败返回 -1
 */
lv_PUBLIC_API int sat_encoding_register_var(SatEncoding *enc, int arity, const int *atom_ids);

/**
 * @brief 查找变量：根据元组查找对应的 SAT 变量 ID
 *
 * @return 变量 ID（>= 1），未找到返回 -1
 */
lv_PUBLIC_API int sat_encoding_lookup_var(const SatEncoding *enc, int arity, const int *atom_ids);

/**
 * @brief 添加 CNF 子句
 *
 * @return 子句索引（>= 0），失败返回 -1
 */
lv_PUBLIC_API int sat_encoding_add_clause(SatEncoding *enc, const SatLiteral *literals, int count);

/**
 * @brief 添加假设（单元子句）
 */
lv_PUBLIC_API int sat_encoding_add_assumption(SatEncoding *enc, SatLiteral literal);

/* ========================================================================
 * SAT 编码管道
 * ======================================================================== */

/**
 * @brief 将约束图编码为 SAT
 */
lv_PUBLIC_API SatResult constraint_graph_to_sat(const ConstraintGraph *graph, SatEncoding *enc);

/**
 * @brief 将关系模型编码为 SAT
 */
lv_PUBLIC_API SatResult relation_model_to_sat(const RelModel *model, const SmallScopeConfig *scope, SatEncoding *enc);

/* ========================================================================
 * SAT 求解与解码
 * ======================================================================== */

/**
 * @brief 求解 SAT 编码并解码结果
 */
lv_PUBLIC_API SatResult sat_solve_and_decode(SatEncoding *enc, SatModel **out_model);

/**
 * @brief 增量求解：追加假设后求解
 */
lv_PUBLIC_API SatResult sat_solve_incremental(SatEncoding *enc, const SatLiteral *literals, int count,
                                              SatModel **out_model);

/**
 * @brief 将 SAT 模型解码为约束图
 */
lv_PUBLIC_API ConstraintGraph *sat_model_to_graph(const SatModel *model);

/**
 * @brief 将 SAT 模型解码为关系实例
 */
lv_PUBLIC_API RelInstance *sat_model_to_instance(const SatEncoding *enc, const SatModel *model);

#ifdef __cplusplus
}
#endif

#endif /* lv_RELATION_MODEL_H */
