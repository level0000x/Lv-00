/**
 * @file relation_model.h
 * @brief 关系模型层 —— 借鉴 Alloy 的"关系即一切"统一建模范式
 *
 * 设计借鉴来源：
 * - Alloy (alloytools.org) — Daniel Jackson 的关系逻辑建模语言
 *   · 所有数据都是关系元组（Atom × ... × Atom → Bool）
 *   · 有限范围实例查找（bounded exhaustive search）
 *   · 关系组合算子（join/product/transpose/closure）
 *
 * 核心设计理念：
 * 将几何约束图重新解释为关系模型：
 *   - 点/线/区域是关系原子（Atom）
 *   - 约束边是关系上的逻辑公式（n 元关系）
 *   - 查询 = 关系表达式求值
 *   - 验证 = 有限范围反例查找
 *
 * @version v3.3.0
 * @date 2026-05-24
 */

#ifndef LV00_RELATION_MODEL_H
#define LV00_RELATION_MODEL_H

#include "lv00.h"
#include "constraint_graph.h"
#include "proof.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── 关系原子 ──
 *
 * 借鉴 Alloy 的 sig 声明：每个几何实体（点、线、圆）对应一个原子。
 * 原子是关系的最小不可分割单元。
 */

/**
 * @brief 关系原子类型
 *
 * 对应几何约束图中的节点类型，每个原子属于一个基础 sig。
 */
typedef enum {
    REL_ATOM_POINT,          /**< 点原子 */
    REL_ATOM_LINE,           /**< 线原子 */
    REL_ATOM_REGION,         /**< 区域原子 */
    REL_ATOM_PORT,           /**< 端口原子 */
    REL_ATOM_FUNC_BLOCK      /**< 函数块原子 */
} RelAtomType;

/**
 * @brief 关系原子
 *
 * Alloy 中 sig 的一个实例。每个原子有唯一标识符和类型标签。
 */
typedef struct RelAtom {
    int          atom_id;       /**< 原子唯一标识符 */
    RelAtomType  type;          /**< 原子类型 */
    char        *label;         /**< 原子标签（可空，用于可读性） */
    int          graph_node_id; /**< 对应的约束图节点 ID（-1 = 纯关系原子） */
} RelAtom;

/**
 * @brief 原子签名（sig）
 *
 * 一组同类型原子的集合。对应 Alloy 的 sig 声明。
 */
typedef struct RelSignature {
    char        *name;          /**< sig 名称（如 "Point", "Line"） */
    RelAtomType  atom_type;     /**< sig 对应的原子类型 */
    RelAtom    **atoms;         /**< sig 中的原子数组 */
    int          atom_count;    /**< 原子数量 */
    int          atom_capacity; /**< 原子数组容量 */
    bool         is_abstract;   /**< 是否为抽象 sig */
    struct RelSignature **sub_sigs; /**< 子 sig 数组 */
    int          sub_sig_count; /**< 子 sig 数量 */
} RelSignature;

/* ── 关系 ──
 *
 * 借鉴 Alloy 的核心理念：一切皆为关系。
 * 标量是 1 元关系，集合是 1 元关系，二元关系是边的集合。
 */

/**
 * @brief 关系元数
 */
typedef enum {
    REL_ARITY_UNARY  = 1,  /**< 一元关系（标量、集合） */
    REL_ARITY_BINARY = 2,  /**< 二元关系（边映射） */
    REL_ARITY_TERNARY = 3, /**< 三元关系 */
    REL_ARITY_NARY   = 4   /**< N 元关系（n >= 4） */
} RelationArity;

/**
 * @brief 关系
 *
 * 核心数据结构：n 元关系的笛卡尔积子集。
 * 例如：线段是点的二元关系，关联是点-线的二元关系。
 */
typedef struct Relation {
    char         *name;           /**< 关系名称 */
    int           arity;          /**< 关系元数（1~n） */
    RelSignature *domains[8];     /**< 各列的定义域签名 */
    int         **tuples;         /**< 元组数组，每个元组有 arity 个 atom_id */
    int           tuple_count;    /**< 元组当前数量 */
    int           tuple_capacity; /**< 元组数组容量 */
} Relation;

/* ── 关系表达式 ──
 *
 * 借鉴 Alloy 的关系表达式语法，支持 join/product/transpose/closure。
 */

/**
 * @brief 关系运算符
 */
typedef enum {
    REL_OP_UNION,            /**< 并集：R + S */
    REL_OP_INTERSECTION,     /**< 交集：R & S */
    REL_OP_DIFFERENCE,       /**< 差集：R - S */
    REL_OP_JOIN,             /**< 关系连接：R.S */
    REL_OP_PRODUCT,          /**< 笛卡尔积：R -> S */
    REL_OP_TRANSPOSE,        /**< 转置：~R */
    REL_OP_TRANSITIVE_CLOSURE,  /**< 传递闭包：^R */
    REL_OP_REFL_TRANS_CLOSURE,  /**< 自反传递闭包：*R */
    REL_OP_IDENTITY,         /**< 恒等关系：iden */
    REL_OP_COMPLEMENT,       /**< 补集：!R */
    REL_OP_RESTRICT_DOMAIN,  /**< 域约束：S <: R */
    REL_OP_RESTRICT_RANGE,   /**< 值域约束：R :> S */
    REL_OP_OVERRIDE          /**< 覆盖：R ++ S */
} RelOp;

/**
 * @brief 关系表达式节点类型
 */
typedef enum {
    REL_EXPR_ATOMIC,     /**< 原子关系（常量或引用） */
    REL_EXPR_COMPOSITE   /**< 复合关系表达式 */
} RelExprType;

/**
 * @brief 关系表达式
 *
 * Alloy 风格的关系表达式 AST 节点，叶子是原子关系引用，
 * 内部节点是关系运算符应用。
 */
typedef struct RelExpr {
    RelExprType type;            /**< 表达式类型 */
    union {
        struct {
            Relation *rel;       /**< 原子关系引用（REL_EXPR_ATOMIC） */
        } atomic;
        struct {
            RelOp      op;       /**< 关系运算符 */
            struct RelExpr *left;  /**< 左子表达式 */
            struct RelExpr *right; /**< 右子表达式（单目运算时为 NULL） */
        } composite;
    } data;
} RelExpr;

/**
 * @brief 关系公式（逻辑约束）
 *
 * Alloy 中事实（fact）和谓词（pred）的对应：
 * 量词语句和（关系上的）逻辑约束。
 */
typedef enum {
    REL_FORMULA_FORALL,      /**< 全称量词：all x: S | F */
    REL_FORMULA_EXISTS,      /**< 存在量词：some x: S | F */
    REL_FORMULA_NO,          /**< 不存在：no R */
    REL_FORMULA_SOME,        /**< 非空：some R */
    REL_FORMULA_LONE,        /**< 最多一个：lone R */
    REL_FORMULA_ONE,         /**< 恰好一个：one R */
    REL_FORMULA_EQ,          /**< 关系相等：R = S */
    REL_FORMULA_SUBSET,      /**< 子集：R in S */
    REL_FORMULA_AND,         /**< 合取：F && G */
    REL_FORMULA_OR,          /**< 析取：F || G */
    REL_FORMULA_NOT,         /**< 否定：!F */
    REL_FORMULA_IMPLIES      /**< 蕴含：F => G */
} RelFormulaType;

/**
 * @brief 关系逻辑公式
 */
typedef struct RelFormula {
    RelFormulaType   type;         /**< 公式类型 */
    char            *quant_var;    /**< 量词绑定变量名（量词公式用） */
    RelSignature    *quant_sig;    /**< 量词绑定 sig（量词公式用） */
    RelExpr         *expr;         /**< 关系表达式参数 */
    struct RelFormula *sub[2];     /**< 子公式（AND/OR/IMPLIES/NOT 用） */
} RelFormula;

/* ── 关系模型 ── */

/**
 * @brief 关系模型
 *
 * Alloy 模型的几何适配版本：包含签名声明、关系字段、事实公式和断言。
 */
typedef struct RelModel {
    RelSignature **sigs;           /**< 签名数组 */
    int            sig_count;      /**< 签名数量 */
    int            sig_capacity;   /**< 签名数组容量 */

    Relation     **relations;      /**< 关系数组 */
    int            relation_count; /**< 关系数量 */
    int            relation_capacity; /**< 关系数组容量 */

    RelFormula   **facts;          /**< 事实（恒真约束）数组 */
    int            fact_count;     /**< 事实数量 */

    RelFormula   **assertions;     /**< 断言（待验证）数组 */
    int            assertion_count; /**< 断言数量 */

    /* 有限范围配置 */
    int            max_point_count;   /**< 点原子的最大数量 */
    int            max_line_count;    /**< 线原子的最大数量 */
    int            max_region_count;  /**< 区域原子的最大数量 */
    int            max_func_block_count; /**< 函数块原子的最大数量 */
} RelModel;

/**
 * @brief 有限范围配置
 *
 * 借鉴 Alloy 的 bounded exhaustive checking：
 * 限制每种 sig 的最大实例数，在小范围内穷举所有可能。
 */
typedef struct SmallScopeConfig {
    int scope_point;      /**< Point 的实例上限（0 = 不限） */
    int scope_line;       /**< Line 的实例上限（0 = 不限） */
    int scope_region;     /**< Region 的实例上限（0 = 不限） */
    int scope_block;      /**< FuncBlock 的实例上限（0 = 不限） */
    int bitwidth;         /**< 整数位宽（默认 4） */
} SmallScopeConfig;

/**
 * @brief 关系模型实例（模型的一个具体解）
 */
typedef struct RelInstance {
    RelModel    *model;           /**< 所属模型 */
    RelAtom    **atoms;           /**< 实例中的所有原子 */
    int          atom_count;      /**< 原子数量 */
    Relation   **rel_bindings;    /**< 关系绑定（关系的具体值） */
    int          binding_count;   /**< 绑定数量 */
    bool         satisfies_assertions; /**< 是否满足所有断言 */
} RelInstance;

/**
 * @brief 默认有限范围配置
 *
 * 返回默认配置：point=8, line=8, region=4, block=2, bitwidth=4。
 *
 * @return 默认配置
 */
static inline SmallScopeConfig rel_small_scope_default(void) {
    SmallScopeConfig cfg;
    cfg.scope_point  = 8;
    cfg.scope_line   = 8;
    cfg.scope_region = 4;
    cfg.scope_block  = 2;
    cfg.bitwidth     = 4;
    return cfg;
}

/* ── 关系操作 API ── */

/**
 * @brief 计算两个关系的并集（R + S）
 *
 * @param a  关系 A
 * @param b  关系 B（必须同元数）
 * @return 新关系（调用者负责销毁），失败返回 NULL
 */
Relation* rel_union(const Relation *a, const Relation *b);

/**
 * @brief 计算两个关系的交集（R & S）
 *
 * @param a  关系 A
 * @param b  关系 B（必须同元数）
 * @return 新关系（调用者负责销毁），失败返回 NULL
 */
Relation* rel_intersection(const Relation *a, const Relation *b);

/**
 * @brief 计算两个关系的差集（R - S）
 *
 * @param a  关系 A
 * @param b  关系 B（必须同元数）
 * @return 新关系（调用者负责销毁），失败返回 NULL
 */
Relation* rel_difference(const Relation *a, const Relation *b);

/**
 * @brief 计算关系连接（R.S，join 运算）
 *
 * 二元关系 R: A→B 和 S: B→C 的 join 得到 A→C。
 *
 * @param a  关系 A
 * @param b  关系 B
 * @return 新关系（调用者负责销毁），失败返回 NULL
 */
Relation* rel_join(const Relation *a, const Relation *b);

/**
 * @brief 计算笛卡尔积（R -> S）
 *
 * @param a  关系 A（m 元）
 * @param b  关系 B（n 元）
 * @return m+n 元新关系（调用者负责销毁），失败返回 NULL
 */
Relation* rel_product(const Relation *a, const Relation *b);

/**
 * @brief 计算关系转置（~R）
 *
 * 交换二元关系的列顺序。
 *
 * @param r  关系（必须为二元）
 * @return 转置关系（调用者负责销毁），失败返回 NULL
 */
Relation* rel_transpose(const Relation *r);

/**
 * @brief 计算传递闭包（^R）
 *
 * R^+ = R U R.R U R.R.R U ...，直到不动点。
 *
 * @param r  关系（必须为二元且定义域=值域）
 * @return 闭包关系（调用者负责销毁），失败返回 NULL
 */
Relation* rel_transitive_closure(const Relation *r);

/**
 * @brief 计算自反传递闭包（*R）
 *
 * R^* = iden U ^R。
 *
 * @param r  关系（必须为二元且定义域=值域）
 * @return 闭包关系（调用者负责销毁），失败返回 NULL
 */
Relation* rel_reflexive_transitive_closure(const Relation *r);

/* ── 关系模型构建 API ── */

/**
 * @brief 从约束图构建关系模型
 *
 * 将几何约束图自动转换为 Alloy 风格的关系模型：
 * - 每种 GeomType 对应一个 RelSignature
 * - 每个节点对应一个 RelAtom
 * - 每个约束边对应关系元组
 * - 推导出的几何不变式作为事实（fact）
 *
 * @param graph  源约束图
 * @return 新分配的关系模型，失败返回 NULL
 */
RelModel* relation_model_from_graph(const ConstraintGraph *graph);

/**
 * @brief 销毁关系模型
 *
 * 递归释放模型中的所有签名、关系、事实和断言。
 *
 * @param model  模型指针（可为 NULL）
 */
void relation_model_destroy(RelModel *model);

/**
 * @brief 为关系模型添加事实
 *
 * @param model   模型
 * @param formula 事实公式（模型取得所有权）
 * @return true 成功
 */
bool relation_model_add_fact(RelModel *model, RelFormula *formula);

/**
 * @brief 为关系模型添加断言
 *
 * @param model   模型
 * @param formula 断言公式（模型取得所有权）
 * @return true 成功
 */
bool relation_model_add_assertion(RelModel *model, RelFormula *formula);

/* ── 可满足性检查 API ── */

/**
 * @brief 检查关系模型的可满足性
 *
 * 在给定有限范围内穷举搜索模型实例，检查所有事实是否可满足。
 *
 * @param model   关系模型
 * @param scope   有限范围配置
 * @return true 存在满足所有事实的实例，false 无解
 */
bool relation_check_satisfiability(RelModel *model, const SmallScopeConfig *scope);

/**
 * @brief 查找关系模型的一个实例
 *
 * @param model      关系模型
 * @param scope      有限范围配置
 * @param assertions 是否验证断言（true = 找断言反例，false = 仅满足事实）
 * @return 找到的实例（调用者负责销毁），无解返回 NULL
 */
RelInstance* relation_find_instance(RelModel *model, const SmallScopeConfig *scope, bool assertions);

/**
 * @brief 销毁关系模型实例
 *
 * @param inst  实例指针（可为 NULL）
 */
void relation_instance_destroy(RelInstance *inst);

/**
 * @brief 计算关系表达式的值
 *
 * 在给定模型和实例中求值关系表达式。
 *
 * @param model  关系模型
 * @param inst   关系实例（可为 NULL = 在所有可能元组上求值）
 * @param expr   关系表达式
 * @return 求值结果关系（调用者负责销毁），失败返回 NULL
 */
Relation* relation_evaluate_expr(const RelModel *model, const RelInstance *inst, const RelExpr *expr);

/**
 * @brief 评估公式在模型实例中的真值
 *
 * @param model  关系模型
 * @param inst   关系实例
 * @param formula 公式
 * @return true 公式在实例中成立
 */
bool relation_evaluate_formula(const RelModel *model, const RelInstance *inst, const RelFormula *formula);

/* ── 导出 API ── */

/**
 * @brief 将关系模型导出为 Alloy 语言格式（.als 文件内容）
 *
 * @param model  关系模型
 * @return Alloy 语言字符串（调用者负责 free），失败返回 NULL
 */
char* relation_model_export_alloy(const RelModel *model);

/**
 * @brief 将关系模型实例导出为 Alloy 格式的实例文件
 *
 * @param inst   关系实例
 * @return XML 格式的实例字符串（调用者负责 free），失败返回 NULL
 */
char* relation_instance_export_xml(const RelInstance *inst);

#ifdef __cplusplus
}
#endif

#endif /* LV00_RELATION_MODEL_H */
