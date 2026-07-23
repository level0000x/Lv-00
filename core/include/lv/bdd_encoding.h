/**
 * @file bdd_encoding.h
 * @brief CUDD 二叉决策图（BDD/ADD）编码 —— 约束图符号化表示
 *
 * 借鉴 CUDD (github.com/ivmai/cudd) 的 BDD 库架构，
 * 为 Lv-00 提供约束图的布尔化编码和符号化操作。
 *
 * 设计借鉴：
 * - CUDD — Colorado University Decision Diagram package
 *   - BDD (Binary Decision Diagram)：布尔函数紧凑表示
 *   - ADD (Algebraic Decision Diagram)：叶子为实数的决策图
 *   - 变量序优化（sifting）与唯一表哈希
 *   - ITE (If-Then-Else) 递归算法
 *
 * 应用场景：
 * - 约束图 → BDD 编码 → 符号化模型计数
 * - BDD → CNF 转换（供 SAT 求解器使用）
 * - 坐标 bit-blasting（IEEE 754 位表示）
 *
 * @version 1.1.0
 * @date 2026-05-24
 */
#ifndef lv_BDD_ENCODING_H
#define lv_BDD_ENCODING_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stdbool.h>
#include <stdint.h>

#include "constraint_graph.h"
#include "symbolic_coord.h"
/* ========================================================================
 * BDD/ADD 基础类型
 * ======================================================================== */
/** BDD 变量在决策图中的类型 */
typedef enum {
    BDD_BOOLEAN = 0, /**< 布尔变量（0/1） */
    BDD_INT_BIT = 1, /**< 整数位变量（bit-blast 中的某一位） */
    BDD_ENUM = 2     /**< 枚举类型变量（多值编码为多位） */
} BDDVarType;
/** BDD 节点（二叉决策图节点） */
typedef struct BDDNode {
    int var_id;           /**< 决策变量 ID（终端节点为 -1） */
    struct BDDNode *low;  /**< 变量=0 时的子图 */
    struct BDDNode *high; /**< 变量=1 时的子图 */
    uint64_t ref_count;   /**< 引用计数（用于垃圾回收） */
    bool complemented;    /**< 是否为补边（CUDD 风格 complemented edges） */
} BDDNode;

/** ITE 计算表条目（缓存 ITE(f,g,h) 结果） */
typedef struct ITECacheEntry {
    BDDNode *f;      /**< ITE 第一个参数 */
    BDDNode *g;      /**< ITE 第二个参数 */
    BDDNode *h;      /**< ITE 第三个参数 */
    BDDNode *result; /**< 缓存的 ITE 结果 */
    bool occupied;   /**< 该槽位是否被占用 */
} ITECacheEntry;

/** BDD 管理器（唯一表 + 变量序 + 缓存） */
typedef struct BDDManager {
    BDDNode *true_node;            /**< 终端 T 节点（常量 1） */
    BDDNode *false_node;           /**< 终端 F 节点（常量 0） */
    BDDNode **unique_table;        /**< 唯一表（哈希桶数组，用于节点去重） */
    int unique_table_size;         /**< 唯一表哈希桶数 */
    int *var_order;                /**< 变量序数组（var_order[i] = 第 i 层的变量 ID） */
    int var_count;                 /**< 已注册变量总数 */
    int var_capacity;              /**< var_order 数组容量 */
    uint64_t node_count;           /**< 当前存活节点数（不含终端节点） */
    ITECacheEntry *computed_table; /**< ITE 计算表（缓存 ITE 结果，避免重复计算） */
    int computed_table_size;       /**< 计算表大小 */
    char **var_names;              /**< 变量名称表（var_names[i] = 第 i 个变量的名称） */
    BDDVarType *var_types;         /**< 变量类型表（var_types[i] = 第 i 个变量的类型） */
} BDDManager;
/* ========================================================================
 * ADD (Algebraic Decision Diagram) 类型
 * ======================================================================== */
/** ADD 节点（代数决策图，叶子为 double） */
typedef struct ADDNode {
    int var_id;           /**< 决策变量 ID（常量叶子为 -1） */
    struct ADDNode *low;  /**< 变量=0 时的子图 */
    struct ADDNode *high; /**< 变量=1 时的子图 */
    double constant;      /**< 常量值（仅 is_constant=true 时有效） */
    bool is_constant;     /**< 是否为常量叶子节点 */
} ADDNode;
/** ADD 管理器 */
typedef struct ADDManager {
    ADDNode *zero_node;     /**< 常量 0 节点 */
    ADDNode *one_node;      /**< 常量 1 节点 */
    ADDNode **unique_table; /**< 唯一表 */
    int unique_table_size;  /**< 唯一表大小 */
    int *var_order;         /**< 变量序 */
    int var_count;          /**< 变量数 */
    uint64_t node_count;    /**< 节点数 */
} ADDManager;
/* ========================================================================
 * BDD 管理器生命周期
 * ======================================================================== */
/**
 * @brief 创建 BDD 管理器
 *
 * @param[in] var_count         最大变量数
 * @param[in] unique_table_size 唯一表哈希桶数（建议质数，如 65537）
 * @return 新管理器，失败返回 NULL
 */
BDDManager *bdd_manager_create(int var_count, int unique_table_size);
/**
 * @brief 销毁 BDD 管理器及其所有节点
 *
 * @param[in,out] mgr BDD 管理器
 */
void bdd_manager_destroy(BDDManager *mgr);
/**
 * @brief 注册新的 BDD 变量
 *
 * @param[in,out] mgr   BDD 管理器
 * @param[in]     name  变量名（调试用，可为 NULL）
 * @param[in]     type  变量类型
 * @return 新变量 ID（>=0），失败返回 -1
 */
int bdd_new_var(BDDManager *mgr, const char *name, BDDVarType type);
/* ========================================================================
 * BDD 基本节点创建
 * ======================================================================== */
/** 获取 T 节点（常量 1） */
BDDNode *bdd_true(BDDManager *mgr);
/** 获取 F 节点（常量 0） */
BDDNode *bdd_false(BDDManager *mgr);
/** 创建字面量节点：var_id 为正 = 正文字，var_id 为负 = 负文字 */
BDDNode *bdd_literal(BDDManager *mgr, int var_id);
/** 增加节点引用计数 */
void bdd_ref(BDDNode *node);
/** 减少节点引用计数（为 0 时从唯一表回收） */
void bdd_deref(BDDManager *mgr, BDDNode *node);
/* ========================================================================
 * BDD 布尔运算
 *
 * 所有运算使用递归 ITE (If-Then-Else) 算法实现：
 *   ite(F, G, H) = (F ∧ G) ∨ (¬F ∧ H)
 * 其他运算均可归约为 ITE。
 * ======================================================================== */
/** BDD AND：f ∧ g */
BDDNode *bdd_and(BDDManager *mgr, BDDNode *f, BDDNode *g);
/** BDD OR：f ∨ g */
BDDNode *bdd_or(BDDManager *mgr, BDDNode *f, BDDNode *g);
/** BDD NOT：¬f */
BDDNode *bdd_not(BDDManager *mgr, BDDNode *f);
/** BDD ITE：if(f) then g else h */
BDDNode *bdd_ite(BDDManager *mgr, BDDNode *f, BDDNode *g, BDDNode *h);
/** BDD XOR：f ⊕ g */
BDDNode *bdd_xor(BDDManager *mgr, BDDNode *f, BDDNode *g);
/** BDD NAND：¬(f ∧ g) */
BDDNode *bdd_nand(BDDManager *mgr, BDDNode *f, BDDNode *g);
/* ========================================================================
 * ADD 代数运算
 * ======================================================================== */
/** ADD 加法：a + b */
ADDNode *add_add(ADDManager *mgr, ADDNode *a, ADDNode *b);
/** ADD 减法：a - b */
ADDNode *add_sub(ADDManager *mgr, ADDNode *a, ADDNode *b);
/** ADD 乘法：a * b */
ADDNode *add_mul(ADDManager *mgr, ADDNode *a, ADDNode *b);
/** ADD 除法：a / b */
ADDNode *add_div(ADDManager *mgr, ADDNode *a, ADDNode *b);
/** ADD 最大值：max(a, b) */
ADDNode *add_max(ADDManager *mgr, ADDNode *a, ADDNode *b);
/** ADD 最小值：min(a, b) */
ADDNode *add_min(ADDManager *mgr, ADDNode *a, ADDNode *b);
/* ========================================================================
 * 变量序优化（Sifting 算法）
 * ======================================================================== */
/**
 * @brief 使用 sifting 算法重排 BDD 变量序
 *
 * Sifting 算法：
 * 1. 遍历每个变量
 * 2. 将该变量移动到每个可能的位置
 * 3. 记录使 BDD 节点数最少的位置
 * 4. 将变量固定在该位置
 *
 * 这是 CUDD 中 `Cudd_ReduceHeap` 的核心算法。
 *
 * @param[in,out] mgr BDD 管理器
 * @return 优化后的节点数（-1 表示失败）
 */
int bdd_reorder_sift(BDDManager *mgr);
/* ========================================================================
 * 约束图 → BDD 编码
 * ======================================================================== */
/**
 * @brief 将整个约束图编码为一个 BDD
 *
 * 枚举所有变量的布尔赋值组合，对每种赋值检查约束可满足性，
 * 将满足的赋值加入 BDD。这是基础的 Shannon 展开方法，
 * 适用于小规模约束图。大规模应使用符号化方法。
 *
 * @param[in] graph 约束图（非 NULL）
 * @param[in] mgr   BDD 管理器
 * @return 表示约束图可满足赋值的 BDD 节点，失败返回 NULL
 */
BDDNode *constraint_graph_to_bdd(const ConstraintGraph *graph, BDDManager *mgr);
/**
 * @brief 将符号坐标编码为 BDD 变量组
 *
 * 对有理数坐标做 bit-blasting 编码：
 * 使用 IEEE 754 双精度位表示，每位一个 BDD 变量。
 * 返回的变量 ID 数组对应 base_var, base_var+1, ..., base_var+63。
 *
 * @param[in]  coord    符号坐标（非 NULL）
 * @param[in]  mgr      BDD 管理器
 * @param[in]  base_var 起始变量 ID（编码占用 base_var ~ base_var+63 共 64 位）
 * @return 成功分配的位数（通常为 64），失败返回 -1
 */
int coord_to_bdd_var(const SymbolicCoord *coord, BDDManager *mgr, int base_var);
/* ========================================================================
 * BDD → CNF 转换
 * ======================================================================== */
/**
 * @brief 将 BDD 转换为 CNF (DIMACS 格式)
 *
 * 将对 BDD 的每个 ITE 节点使用 Tseitin 变换引入辅助变量，
 * 生成等可满足的 CNF 公式。
 *
 * @param[in]  bdd      BDD 根节点（非 NULL）
 * @param[out] out_cnf  输出的 CNF 字符串（DIMACS 格式，调用者负责 free）
 * @return true 成功，false 失败
 */
bool bdd_to_cnf(BDDNode *bdd, char **out_cnf);
/* ========================================================================
 * ADD 管理器生命周期
 * ======================================================================== */
/**
 * @brief 创建 ADD 管理器
 *
 * @param[in] var_count         最大变量数
 * @param[in] unique_table_size 唯一表大小
 * @return 新管理器，失败返回 NULL
 */
ADDManager *add_manager_create(int var_count, int unique_table_size);
/**
 * @brief 销毁 ADD 管理器
 */
void add_manager_destroy(ADDManager *mgr);
/** 创建 ADD 常量节点 */
ADDNode *add_constant(ADDManager *mgr, double value);
#ifdef __cplusplus
}
#endif
#endif /* lv_BDD_ENCODING_H */
