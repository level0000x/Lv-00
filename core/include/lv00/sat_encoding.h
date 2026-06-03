/**
 * @file sat_encoding.h
 * @brief SAT 编码管道 —— 借鉴 Alloy Kodkod 的关系逻辑到 SAT 编码管道
 *
 * 设计借鉴来源：
 * - Kodkod (emina.github.io/kodkod) — Emina Torlak 的关系逻辑 SAT 编码器
 *   · 关系逻辑 → 命题逻辑的对称破缺编码
 *   · 逐层翻译管道：关系公式 → 布尔约束 → CNF 子句
 *   · 基于 MiniSat 的高效增量求解
 *
 * 核心设计理念：
 * 将几何约束图的关系视图映射到 SAT 变量空间：
 *   - 每个 (节点_i, 节点_j) 对映射为布尔变量
 *   - 几何约束（共线/平行/距离）编码为 CNF 子句
 *   - SAT 求解结果解码回关系模型实例
 *
 * @version v3.3.0
 * @date 2026-05-24
 */

#ifndef LV00_SAT_ENCODING_H
#define LV00_SAT_ENCODING_H

#include "constraint_graph.h"
#include "lv00.h"
#include "relation_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── SAT 求解结果 ── */

/**
 * @brief SAT 求解结果
 *
 * 以 Kodkod 的求解结果为基准，统一 SAT 求解器的输出语义。
 */
typedef enum {
    SAT_OK = 0,      /**< 可满足，已获得解 */
    SAT_UNSAT = 1,   /**< 不可满足，已获得证明 */
    SAT_UNKNOWN = 2, /**< 未知（超时/内存不足） */
    SAT_ERROR = 3    /**< 内部错误 */
} SatResult;

/**
 * @brief SAT 求解结果转字符串
 *
 * @param result  求解结果
 * @return 结果的人类可读字符串
 */
static inline const char *sat_result_to_string(SatResult result) {
    switch (result) {
        case SAT_OK:
            return "SAT_OK";
        case SAT_UNSAT:
            return "SAT_UNSAT";
        case SAT_UNKNOWN:
            return "SAT_UNKNOWN";
        case SAT_ERROR:
            return "SAT_ERROR";
        default:
            return "SAT_?";
    }
}

/* ── SAT 变量映射表 ──
 *
 * 将关系元组映射为布尔变量 ID。
 * 借鉴 Kodkod 的 tuple-to-literal 映射策略。
 */

/**
 * @brief SAT 变量
 *
 * 正文字（literal）为 var_id 的正负号表示。
 */
typedef int SatLiteral; /**< 正文字：正值为 var_id，负值为 ~var_id */

/**
 * @brief SAT 变量映射表条目
 *
 * 每条记录映射一个关系元组到唯一的 SAT 变量 ID。
 */
typedef struct SatVarEntry {
    int var_id;      /**< SAT 变量 ID（>= 1） */
    int arity;       /**< 元组的元数 */
    int atom_ids[8]; /**< 元组中的原子 ID */
} SatVarEntry;

/**
 * @brief SAT 编码上下文
 *
 * 维护关系元组→SAT变量映射、CNF子句缓冲区以及编码统计。
 */
typedef struct SatEncoding {
    SatVarEntry *var_map; /**< 变量映射表（动态数组） */
    int var_count;        /**< 当前变量数量 */
    int var_capacity;     /**< 映射表容量 */
    int next_var_id;      /**< 下一个可分配变量 ID */

    int **clauses;       /**< CNF 子句缓冲区（每个子句是 literal 数组） */
    int *clause_sizes;   /**< 每个子句的大小 */
    int clause_count;    /**< 当前子句数量 */
    int clause_capacity; /**< 子句缓冲区容量 */

    /* 编码统计 */
    int total_vars;        /**< 总变量数 */
    int total_clauses;     /**< 总子句数 */
    double encode_time_ms; /**< 编码耗时（毫秒） */

    /* 关联的约束图和关系模型引用（只读） */
    const ConstraintGraph *graph; /**< 源约束图 */
    const RelModel *rel_model;    /**< 关系模型（可空） */
} SatEncoding;

/**
 * @brief SAT 求解器模型（解码后的解）
 *
 * 从 SAT 解反译回约束图状态的中间结构。
 */
typedef struct SatModel {
    int *true_vars; /**< 赋值为真的变量 ID 数组 */
    int true_count; /**< 真变量数量 */
    int var_count;  /**< 变量总数 */

    /* 解码后的几何结构 */
    ConstraintGraph *decoded_graph; /**< 解码后的约束图（可空 = 未解码） */
    RelInstance *decoded_instance;  /**< 解码后的关系实例（可空 = 未解码） */
} SatModel;

/* ── 约束→CNF 编码规则 ──
 *
 * 每个几何约束类型对应一组 CNF 子句编码规则。
 */

/**
 * @brief 共线性约束编码
 *
 * 三个点 P1, P2, P3 共线。
 * 编码规则：若 P1-P2 和 P2-P3 方向向量成比例，则共线。
 *
 * @param enc    编码上下文
 * @param p1_id  点1节点ID
 * @param p2_id  点2节点ID
 * @param p3_id  点3节点ID
 * @return 编码的子句数量，失败返回 -1
 */
int sat_encode_collinearity(SatEncoding *enc, int p1_id, int p2_id, int p3_id);

/**
 * @brief 平行性约束编码
 *
 * 四条线段 L1(P1P2) 和 L2(P3P4) 平行。
 * 编码规则：方向向量叉积为 0。
 *
 * @param enc    编码上下文
 * @param p1_id  线段1起点
 * @param p2_id  线段1终点
 * @param p3_id  线段2起点
 * @param p4_id  线段2终点
 * @return 编码的子句数量，失败返回 -1
 */
int sat_encode_parallelism(SatEncoding *enc, int p1_id, int p2_id, int p3_id, int p4_id);

/**
 * @brief 垂直性约束编码
 *
 * 两条线段 L1 和 L2 垂直。
 * 编码规则：方向向量点积为 0。
 *
 * @param enc    编码上下文
 * @param p1_id  线段1起点
 * @param p2_id  线段1终点
 * @param p3_id  线段2起点
 * @param p4_id  线段2终点
 * @return 编码的子句数量，失败返回 -1
 */
int sat_encode_perpendicularity(SatEncoding *enc, int p1_id, int p2_id, int p3_id, int p4_id);

/**
 * @brief 距离相等约束编码
 *
 * |P1P2| = |P3P4| 的编码。
 *
 * @param enc    编码上下文
 * @param p1_id  线段1起点
 * @param p2_id  线段1终点
 * @param p3_id  线段2起点
 * @param p4_id  线段2终点
 * @return 编码的子句数量，失败返回 -1
 */
int sat_encode_distance_eq(SatEncoding *enc, int p1_id, int p2_id, int p3_id, int p4_id);

/**
 * @brief 角度相等约束编码
 *
 * 角 P1P2P3 = 角 P4P5P6 的编码。
 *
 * @param enc    编码上下文
 * @param p1_id  角1顶点
 * @param p2_id  角1边端点
 * @param p3_id  角1边端点
 * @param p4_id  角2顶点
 * @param p5_id  角2边端点
 * @param p6_id  角2边端点
 * @return 编码的子句数量，失败返回 -1
 */
int sat_encode_angle_eq(SatEncoding *enc, int p1_id, int p2_id, int p3_id, int p4_id, int p5_id, int p6_id);

/**
 * @brief 包含关系约束编码
 *
 * 点 P 包含在区域 R 内的编码。
 *
 * @param enc    编码上下文
 * @param p_id   点节点ID
 * @param r_id   区域节点ID
 * @return 编码的子句数量，失败返回 -1
 */
int sat_encode_containment(SatEncoding *enc, int p_id, int r_id);

/**
 * @brief 通用约束编码
 *
 * 根据约束图中指定约束的类型，自动选择合适的编码规则。
 *
 * @param enc            编码上下文
 * @param constraint_id  约束的ID
 * @return 编码的子句数量，失败返回 -1
 */
int sat_encode_constraint(SatEncoding *enc, int constraint_id);

/* ── SAT 编码管道 API ── */

/**
 * @brief 创建 SAT 编码上下文
 *
 * @param initial_var_capacity     变量映射表初始容量
 * @param initial_clause_capacity  CNF 子句缓冲区初始容量
 * @return 新分配的编码上下文，失败返回 NULL
 */
SatEncoding *sat_encoding_create(int initial_var_capacity, int initial_clause_capacity);

/**
 * @brief 销毁 SAT 编码上下文
 *
 * 释放变量映射表、CNF 子句缓冲区及所有内部资源。
 *
 * @param enc  编码上下文（可为 NULL）
 */
void sat_encoding_destroy(SatEncoding *enc);

/**
 * @brief 注册一个变量映射
 *
 * 为给定的原子元组分配一个唯一的 SAT 变量 ID。
 * 若元组已有映射，返回已有 ID。
 *
 * @param enc       编码上下文
 * @param arity     元数
 * @param atom_ids  原子ID数组（长度为 arity）
 * @return SAT 变量 ID（>= 1），失败返回 -1
 */
int sat_encoding_register_var(SatEncoding *enc, int arity, const int *atom_ids);

/**
 * @brief 查找元组对应的 SAT 变量
 *
 * @param enc       编码上下文
 * @param arity     元数
 * @param atom_ids  原子ID数组
 * @return 变量 ID，未找到返回 -1
 */
int sat_encoding_lookup_var(const SatEncoding *enc, int arity, const int *atom_ids);

/**
 * @brief 向编码添加上下文子句
 *
 * 添加一个 CNF 子句（literal 的析取式）。
 *
 * @param enc        编码上下文
 * @param literals   文字数组
 * @param count      文字数量
 * @return 添加的子句索引（>= 0），失败返回 -1
 */
int sat_encoding_add_clause(SatEncoding *enc, const SatLiteral *literals, int count);

/**
 * @brief 从单元子句添加假设
 *
 * 添加一个假设文字（用于增量求解的 under-assumptions 模式）。
 *
 * @param enc       编码上下文
 * @param literal   假设文字
 * @return 添加的子句索引，失败返回 -1
 */
int sat_encoding_add_assumption(SatEncoding *enc, SatLiteral literal);

/* ── 约束图→SAT 编码 API ── */

/**
 * @brief 将约束图编码为 SAT 问题
 *
 * 整个约束图的所有约束编码为 CNF 子句。这是主编码管道入口。
 *
 * @param graph  源约束图
 * @param enc    编码上下文（会将结果写入此处）
 * @return SAT_OK 编码成功，SAT_ERROR 编码失败
 *
 * @note 此函数通过 sat_encode_constraint() 对每个约束调用编码规则。
 */
SatResult constraint_graph_to_sat(const ConstraintGraph *graph, SatEncoding *enc);

/**
 * @brief 将关系模型编码为 SAT 问题
 *
 * 编码关系模型的事实公式和断言为 CNF 子句。
 *
 * @param model  关系模型
 * @param scope  有限范围配置
 * @param enc    编码上下文
 * @return SAT_OK 或错误码
 */
SatResult relation_model_to_sat(const RelModel *model, const SmallScopeConfig *scope, SatEncoding *enc);

/* ── SAT 求解与解码 API ── */

/**
 * @brief 求解编码后的 SAT 问题并解码结果
 *
 * 调用内部 SAT 求解器，将结果解码为 SatModel。
 *
 * @param enc        已编码的 SAT 上下文
 * @param out_model  输出：SAT 模型（调用者负责销毁），UNSAT 时为 NULL
 * @return SAT_OK / SAT_UNSAT / SAT_UNKNOWN / SAT_ERROR
 */
SatResult sat_solve_and_decode(SatEncoding *enc, SatModel **out_model);

/**
 * @brief 增量求解（保留已学子句）
 *
 * 在已有子句基础上追加新子句并重新求解。
 *
 * @param enc        编码上下文
 * @param literals   追加的假设文字数组
 * @param count      假设数量
 * @param out_model  输出：SAT 模型（可空 = 仅求解不解码）
 * @return 求解结果
 */
SatResult sat_solve_incremental(SatEncoding *enc, const SatLiteral *literals, int count, SatModel **out_model);

/**
 * @brief 将 SAT 模型解码回约束图
 *
 * 从布尔赋值重建几何约束图结构。
 *
 * @param model  SAT 模型
 * @return 解码后的约束图（调用者负责 graph_destroy），失败返回 NULL
 */
ConstraintGraph *sat_model_to_graph(const SatModel *model);

/**
 * @brief 将 SAT 模型解码回关系实例
 *
 * @param enc   编码上下文（含变量映射）
 * @param model SAT 模型
 * @return 解码后的关系实例（调用者负责销毁），失败返回 NULL
 */
RelInstance *sat_model_to_instance(const SatEncoding *enc, const SatModel *model);

/**
 * @brief 销毁 SAT 模型
 *
 * @param model  SAT 模型（可为 NULL）
 */
void sat_model_destroy(SatModel *model);

/* ── 诊断与调试 API ── */

/**
 * @brief 获取不可满足核心（UNSAT core）
 *
 * 返回构成矛盾的最小 CNF 子句子集（子句索引数组）。
 *
 * @param enc        编码上下文（必须是 UNSAT 状态）
 * @param out_count  输出：核心子句数量
 * @return 子句索引数组（调用者负责 free），失败返回 NULL
 */
int *sat_get_unsat_core(const SatEncoding *enc, int *out_count);

/**
 * @brief 导出 SAT 编码为 DIMACS CNF 格式
 *
 * 用于外部 SAT 求解器验证或调试。
 *
 * @param enc       编码上下文
 * @param filepath  输出文件路径
 * @return true 成功，false 失败
 */
bool sat_encoding_export_dimacs(const SatEncoding *enc, const char *filepath);

/**
 * @brief 获取编码统计信息
 *
 * @param enc         编码上下文
 * @param out_vars    输出：变量总数
 * @param out_clauses 输出：子句总数
 */
void sat_encoding_get_stats(const SatEncoding *enc, int *out_vars, int *out_clauses);

#ifdef __cplusplus
}
#endif

#endif /* LV00_SAT_ENCODING_H */
