/**
 * @file euclidean_geometry.h
 * @brief 欧几里得几何公理体系 —— Birkhoff/Tarski 双公理系统及其等价性验证
 *
 * 借鉴 mathlib4 EuclideanGeometry 的形式化设计：
 *   - 同时实现 Birkhoff 公理体系和 Tarski 公理体系并证明二者等价性
 *   - IncidenceGeometry 类型类：定义点/线关联公理
 *   - Betweenness（介于性）和 Congruence（全等性）的类型安全表示
 *   - SyntheticGeometry 免坐标风格 —— 所有推理不依赖坐标系
 *   - 双公理体系验证（Birkhoff vs Tarski），支持等价性证明链
 *
 * 设计目标：
 *   - 支持 Hilbert 五大公理组（关联、顺序、全等、平行、连续）
 *   - 提供可组合的谓词系统，支持约束图集成
 *   - 公理一致性检查与自动降级
 *   - 导出 Birkhoff/Tarski 形式化约束图以供外部求解器消费
 *
 * @version v3.3.0
 * @date 2026-05-24
 */
#ifndef LV00_EUCLIDEAN_GEOMETRY_H
#define LV00_EUCLIDEAN_GEOMETRY_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "constraint_graph.h"
#include "symbolic_coord.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ========================================================================
 * 第一部分：公理体系枚举
 *
 * 支持四种公理体系：
 *   - EUCLID_BIRKHOFF  — Birkhoff (1932)：基于实数度量和角度
 *   - EUCLID_TARSKI    — Tarski (1959)：一阶逻辑，仅点变量
 *   - EUCLID_HILBERT   — Hilbert (1899)：五大公理组的经典表述
 *   - EUCLID_CUSTOM    — 用户自定义公理体系
 *
 * 默认启用 EUCLID_HILBERT（最接近传统几何教材）。
 * ======================================================================== */
/**
 * @brief 公理体系标识枚举
 */
typedef enum {
    EUCLID_BIRKHOFF, /**< Birkhoff 公理体系：实数量度 + 角度，公理数量少但依赖实数完备性 */
    EUCLID_TARSKI,   /**< Tarski 公理体系：仅一阶逻辑，仅含点变量，11 条公理 + 连续性公理模式 */
    EUCLID_HILBERT,  /**< Hilbert 公理体系：五大公理组的经典表述，20 条公理 */
    EUCLID_CUSTOM    /**< 用户自定义公理体系 */
} EuclideanAxiomSystem;
/* ========================================================================
 * 第二部分：五大公理组的枚举常量
 *
 * 借鉴 Hilbert 的五大公理组分类：
 *   - I.   关联公理（Incidence）：点与线的从属关系
 *   - II.  顺序公理（Order/Betweenness）：点在线上的顺序
 *   - III. 全等公理（Congruence）：线段/角的相等关系
 *   - IV.  平行公理（Parallel）：平行线的唯一性
 *   - V.   连续公理（Continuity）：Archimedes 公理 + 完备性
 * ======================================================================== */
/**
 * @brief 关联公理枚举（Incidence Axioms, 对应 Hilbert I.1-I.8）
 */
typedef enum {
    INCIDENCE_TWO_POINTS_ONE_LINE,         /**< I.1: 任意两点确定唯一一条直线 */
    INCIDENCE_LINE_CONTAINS_TWO_POINTS,    /**< I.2: 每条直线至少含两点 */
    INCIDENCE_THREE_NONCOLLINEAR_POINTS,   /**< I.3: 存在至少三个不共线的点 */
    INCIDENCE_THREE_POINTS_ONE_PLANE,      /**< I.4: 任意不共线三点确定唯一平面 */
    INCIDENCE_PLANE_CONTAINS_LINE,         /**< I.5: 若直线两点在平面内，则整条线在平面内 */
    INCIDENCE_TWO_PLANES_INTERSECT_LINE,   /**< I.6: 两平面交于一直线 */
    INCIDENCE_PLANE_CONTAINS_THREE_POINTS, /**< I.7: 每个平面至少含三个不共线点 */
    INCIDENCE_FOUR_NONCOPLANAR_POINTS      /**< I.8: 存在至少四个不共面的点 */
} IncidenceAxiom;
/**
 * @brief 顺序公理枚举（Order/Betweenness Axioms, 对应 Hilbert II.1-II.4）
 */
typedef enum {
    ORDER_BETWEENNESS_SYMMETRY,     /**< II.1: 若 B 在 A 和 C 之间，则 B 在 C 和 A 之间 */
    ORDER_TWO_POINTS_ONE_BETWEEN,   /**< II.2: 给定 A,C，存在 B 在 A 和 C 之间以及 D 使 C 在 A,D 之间 */
    ORDER_THREE_POINTS_ONE_BETWEEN, /**< II.3: 任意三个共线点，恰有一点在其余两点之间 */
    ORDER_PASCH_AXIOM               /**< II.4: Pasch 公理——直线与三角形一边相交则必与另一边相交 */
} OrderAxiom;
/**
 * @brief 全等公理枚举（Congruence Axioms, 对应 Hilbert III.1-III.5）
 */
typedef enum {
    CONGRUENCE_SEGMENT_TRANSFER, /**< III.1: 线段可转移——给定线段 AB 和射线从 A' 出发，可在射线上取 B' 使 AB≅A'B' */
    CONGRUENCE_TRANSITIVITY,     /**< III.2: 若 AB≅CD 且 AB≅EF，则 CD≅EF */
    CONGRUENCE_SEGMENT_ADDITION, /**< III.3: 若 AB≅A'B' 且 BC≅B'C' 且 B 介于 A,C; B' 介于 A',C'，则 AC≅A'C' */
    CONGRUENCE_ANGLE_TRANSFER,   /**< III.4: 角度可转移 */
    CONGRUENCE_SAS               /**< III.5: SAS 全等——两边及其夹角相等则三角形全等 */
} CongruenceAxiom;
/**
 * @brief 平行公理枚举（Parallel Axiom, 对应 Hilbert IV）
 */
typedef enum {
    PARALLEL_PLAYFAIR,     /**< Playfair 公理：过直线外一点有且仅有一条平行线 */
    PARALLEL_EUCLID_FIFTH, /**< Euclid 第五公设：同旁内角和小于两直角则两直线相交 */
    PARALLEL_PROCLUS       /**< Proclus 等价形式：若一直线与两平行线之一相交，则必与另一条相交 */
} ParallelAxiom;
/**
 * @brief 连续公理枚举（Continuity Axioms, 对应 Hilbert V.1-V.2）
 */
typedef enum {
    CONTINUITY_ARCHIMEDES,       /**< V.1: Archimedes 公理——给定线段 AB 和 CD，存在自然数 n 使 n*AB > CD */
    CONTINUITY_LINE_COMPLETENESS /**< V.2: 直线完备性——点集不能扩充而保持所有公理成立 */
} ContinuityAxiom;
/* ========================================================================
 * 第三部分：几何关系谓词（Predicate）
 *
 * 借鉴 mathlib4 SyntheticGeometry 的类型安全谓词设计。
 * 每个谓词封装了一种几何关系，支持符号和数值两种验证模式。
 * ======================================================================== */
/**
 * @brief 共线性谓词 —— 判断给定点集是否共线
 *
 * 符号模式：检查行列式恒为零（精确）
 * 数值模式：检查共线误差 < epsilon（近似）
 */
typedef struct CollinearityPredicate {
    int *point_ids;            /**< 点 ID 数组 */
    int point_count;           /**< 点数量（>= 3） */
    bool is_collinear;         /**< 验证结果：true = 共线 */
    double collinearity_error; /**< 共线误差（numerical mode） */
    bool verified_symbolic;    /**< 是否已通过符号验证 */
} CollinearityPredicate;
/**
 * @brief 介于性谓词 —— 判断点 B 是否在点 A 和点 C 之间
 *
 * Tarski 公理体系的基石，Hilbert 体系使用顺序关系。
 * 支持符号条件（坐标关系）和欧几里得度量（距离关系）双重验证。
 */
typedef struct BetweennessPredicate {
    int point_a_id;   /**< 点 A 的 ID */
    int point_b_id;   /**< 点 B 的 ID（可能介于 A,C 之间） */
    int point_c_id;   /**< 点 C 的 ID */
    bool is_between;  /**< 验证结果：true = B 在 A 和 C 之间 */
    bool verified;    /**< 是否已验证 */
    double ratio;     /**< BA:BC 的比值（数值模式） */
    int axiom_source; /**< 推理依据的公理枚举值 */
} BetweennessPredicate;
/**
 * @brief 全等性谓词 —— 判断两线段或两角是否全等
 *
 * 线段全等：||AB|| == ||CD||
 * 角度全等：∠ABC == ∠DEF
 * 支持 SAS/SSS/ASA 等推理链的重放。
 */
typedef struct CongruencePredicate {
    int obj_type; /**< 0=线段全等, 1=角度全等 */
    union {
        struct {
            int seg_a1_id, seg_a2_id, seg_b1_id, seg_b2_id;
        } seg; /**< AB≅CD */
        struct {
            int ang_vertex_a, ang_side_a1, ang_side_a2, ang_vertex_b, ang_side_b1, ang_side_b2;
        } ang; /**< ∠ABC≅∠DEF */
    } args;
    bool is_congruent;    /**< 验证结果 */
    bool verified;        /**< 是否已验证 */
    double tolerance;     /**< 容差（数值模式） */
    int proof_step_count; /**< 证明步骤数量 */
    int *proof_step_ids;  /**< 证明步骤 ID 数组 */
} CongruencePredicate;
/**
 * @brief 平行谓词 —— 判断两直线是否平行
 *
 * 可基于 Playfair 公理或向量方向判定。
 */
typedef struct ParallelPredicate {
    int line_a_id;           /**< 直线 A 的 ID */
    int line_b_id;           /**< 直线 B 的 ID */
    bool is_parallel;        /**< 验证结果 */
    int parallel_axiom_used; /**< 使用的平行公理版本 */
} ParallelPredicate;
/**
 * @brief 垂直谓词 —— 判断两直线是否垂直
 */
typedef struct PerpendicularPredicate {
    int line_a_id;         /**< 直线 A 的 ID */
    int line_b_id;         /**< 直线 B 的 ID */
    bool is_perpendicular; /**< 验证结果 */
    double angle_degrees;  /**< 夹角（度） */
} PerpendicularPredicate;
/* ========================================================================
 * 第四部分：公理等价性验证框架
 *
 * 借鉴 mathlib4 中 Birkhoff 与 Tarski 等价性的形式化证明。
 *
 * 等价性证明链结构：
 *   EquivalenceProofChain
 *     ├── Birkhoff → Tarski 的翻译映射（axiom-level）
 *     ├── Tarski → Birkhoff 的翻译映射
 *     ├── 中间引理数组（每个引理带验证状态）
 *     └── 验证状态（pending / verified / failed / incomplete）
 * ======================================================================== */
/**
 * @brief 等价性验证状态
 */
typedef enum {
    EQUIV_STATUS_PENDING,   /**< 待验证 */
    EQUIV_STATUS_VERIFIED,  /**< 已验证等价 */
    EQUIV_STATUS_FAILED,    /**< 验证失败（不等价） */
    EQUIV_STATUS_INCOMPLETE /**< 不完全（缺少必要的引理） */
} EquivVerificationStatus;
/**
 * @brief 等价性证明链 —— 连接 Birkhoff 和 Tarski 的翻译映射
 */
typedef struct EquivalenceProofChain {
    EuclideanAxiomSystem source_system; /**< 源公理体系 */
    EuclideanAxiomSystem target_system; /**< 目标公理体系 */
    EquivVerificationStatus status;     /**< 当前验证状态 */
    /* 翻译映射：源公理 ID → 目标公理 ID 的数组 */
    int *axiom_translation_map; /**< 公理翻译映射表 */
    int translation_count;      /**< 翻译映射条目数 */
    /* 中间引理 */
    int *lemma_ids;  /**< 所需引理的 ID 数组 */
    int lemma_count; /**< 引理数量 */
    /* 一致性证据 */
    bool birhoff_implies_tarski;  /**< Birkhoff ⇒ Tarski 方向已验证 */
    bool tarski_implies_birkhoff; /**< Tarski ⇒ Birkhoff 方向已验证 */
    /* 约束图快照：在验证过程中构建的几何构造 */
    ConstraintGraph *verification_graph; /**< 验证过程中构建的约束图（拥有所有权） */
} EquivalenceProofChain;
/* ========================================================================
 * 第五部分：欧几里得几何上下文
 *
 * 全局上下文，维护：
 *   - 当前活跃的公理体系（EUCLID_BIRKHOFF / TARSKI / HILBERT / CUSTOM）
 *   - 已注册的点/线/圆列表（用于公理验证）
 *   - 关联的约束图引用（用于几何构造）
 *   - 一致性状态
 * ======================================================================== */
/**
 * @brief 欧几里得几何操作上下文
 *
 * 维护活跃公理体系、注册的几何实体以及公理一致性状态。
 * 与 ConstraintGraph 紧密集成：所有几何声明和谓词断言
 * 都会同步到关联的约束图中。
 */
typedef struct EuclideanContext {
    EuclideanAxiomSystem active_axiom_system; /**< 当前活跃的公理体系 */
    /* 已注册的几何实体 */
    int *registered_points;  /**< 已注册点 ID 数组 */
    int point_count;         /**< 已注册点数量 */
    int point_capacity;      /**< 点数组容量 */
    int *registered_lines;   /**< 已注册线 ID 数组 */
    int line_count;          /**< 已注册线数量 */
    int line_capacity;       /**< 线数组容量 */
    int *registered_circles; /**< 已注册圆 ID 数组 */
    int circle_count;        /**< 已注册圆数量 */
    int circle_capacity;     /**< 圆数组容量 */
    /* 约束图关联 */
    ConstraintGraph *constraint_graph; /**< 关联的约束图（借引用，不拥有所有权） */
    /* 公理启用位掩码：
     *   bits 0-7:   IncidenceAxiom
     *   bits 8-11:  OrderAxiom
     *   bits 12-16: CongruenceAxiom
     *   bits 17-19: ParallelAxiom
     *   bits 20-21: ContinuityAxiom
     */
    uint32_t enabled_axioms_mask;
    /* 一致性状态 */
    bool is_consistent;              /**< 当前上下文是否一致 */
    int inconsistency_source;        /**< 导致不一致的公理/谓词 ID */
    char inconsistency_message[256]; /**< 不一致的详细描述 */
    /* 等价性验证链 */
    EquivalenceProofChain *equivalence_chain; /**< 等价性证明链（可为 NULL） */
} EuclideanContext;
/* ========================================================================
 * 第六部分：核心 API —— 初始化与配置
 * ======================================================================== */
/**
 * @brief 创建欧几里得几何上下文
 *
 * 初始化一个空的 EuclideanContext，默认使用 Hilbert 公理体系，
 * 启用全部五大公理组的公理。
 *
 * @param graph 关联的约束图（可为 NULL，后续通过 euclidean_bind_graph() 绑定）
 * @return 新分配的 EuclideanContext，失败返回 NULL
 */
EuclideanContext *euclidean_init(ConstraintGraph *graph);
/**
 * @brief 销毁欧几里得几何上下文
 *
 * 释放所有注册的实体列表和等价性证明链。
 * 注意：不释放关联的 ConstraintGraph（由调用者管理）。
 *
 * @param ctx 欧几里得上下文
 */
void euclidean_destroy(EuclideanContext *ctx);
/**
 * @brief 设置当前活跃的公理体系
 *
 * 切换到指定的公理体系。切换时会对已注册的实体执行一致性检查。
 * 如果新体系与现有构造不一致，返回 false 并设置 inconsistency_message。
 *
 * @param ctx    欧几里得上下文
 * @param system 目标公理体系
 * @return true 切换成功，false 存在不一致
 */
bool euclidean_set_axiom_system(EuclideanContext *ctx, EuclideanAxiomSystem system);
/**
 * @brief 获取当前活跃的公理体系
 *
 * @param ctx 欧几里得上下文
 * @return 当前活跃的公理体系枚举值（ctx 为 NULL 时返回 EUCLID_HILBERT）
 */
EuclideanAxiomSystem euclidean_get_axiom_system(const EuclideanContext *ctx);
/**
 * @brief 将上下文绑定到新的约束图
 *
 * 所有后续的几何声明和谓词断言都会作用到此约束图上。
 *
 * @param ctx   欧几里得上下文
 * @param graph 约束图（可为 NULL 以解除绑定）
 */
void euclidean_bind_graph(EuclideanContext *ctx, ConstraintGraph *graph);
/* ========================================================================
 * 第七部分：核心 API —— 几何实体声明
 * ======================================================================== */
/**
 * @brief 声明一个点
 *
 * 在上下文中注册一个新点。该点会被添加到已注册点列表
 * 并同步到关联的约束图中（创建 GEOM_POINT 节点）。
 *
 * @param ctx 欧几里得上下文
 * @param x   X 坐标（可为 NULL 表示未定坐标）
 * @param y   Y 坐标（可为 NULL 表示未定坐标）
 * @param name 可选的名称（可为 NULL）
 * @return 新注册的点 ID（>= 0），失败返回 -1
 */
int euclidean_declare_point(EuclideanContext *ctx, SymbolicCoord *x, SymbolicCoord *y, const char *name);
/**
 * @brief 声明一条直线
 *
 * 由两个不同的点确定一条直线。
 * 两点必须已在上下文中注册。
 *
 * @param ctx  欧几里得上下文
 * @param p1_id 第一个点的 ID
 * @param p2_id 第二个点的 ID
 * @return 新注册的线 ID（>= 0），失败返回 -1（点不存在或两点相同）
 */
int euclidean_declare_line(EuclideanContext *ctx, int p1_id, int p2_id);
/**
 * @brief 声明一个圆
 *
 * 由圆心和半径确定一个圆。
 *
 * @param ctx     欧几里得上下文
 * @param center_id 圆心点 ID
 * @param radius   半径（符号坐标）
 * @return 新注册的圆 ID（>= 0），失败返回 -1
 */
int euclidean_declare_circle(EuclideanContext *ctx, int center_id, SymbolicCoord *radius);
/* ========================================================================
 * 第八部分：核心 API —— 几何谓词断言
 * ======================================================================== */
/**
 * @brief 断言一组点共线
 *
 * 在约束图中添加 COLLINEAR 约束，并验证是否满足当前公理体系。
 *
 * @param ctx       欧几里得上下文
 * @param point_ids 点 ID 数组
 * @param count     点数量（必须 >= 3）
 * @return true 断言成功且一致，false 冲突
 */
bool euclidean_assert_collinear(EuclideanContext *ctx, const int *point_ids, int count);
/**
 * @brief 断言点 B 在点 A 和点 C 之间
 *
 * 添加 Betweenness 约束到约束图中。
 *
 * @param ctx 欧几里得上下文
 * @param a_id 点 A 的 ID
 * @param b_id 点 B 的 ID
 * @param c_id 点 C 的 ID
 * @return true 断言成功且一致，false 冲突
 */
bool euclidean_assert_between(EuclideanContext *ctx, int a_id, int b_id, int c_id);
/**
 * @brief 断言两条线段全等
 *
 * 在约束图中添加 CONGRUENCE 约束。
 *
 * @param ctx    欧几里得上下文
 * @param a1_id  第一条线段的第一个端点 ID
 * @param a2_id  第一条线段的第二个端点 ID
 * @param b1_id  第二条线段的第一个端点 ID
 * @param b2_id  第二条线段的第二个端点 ID
 * @return true 断言成功且一致，false 冲突
 */
bool euclidean_assert_congruent(EuclideanContext *ctx, int a1_id, int a2_id, int b1_id, int b2_id);

/* ── forward decls for .c internal functions ── */
void euclidean_destroy_equivalence_chain(EquivalenceProofChain *chain);
bool euclidean_check_consistency(EuclideanContext *ctx);

#ifdef __cplusplus
}
#endif
#endif