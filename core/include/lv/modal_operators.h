/* ========================================================================
 * 模块名称：模态逻辑扩展 (modal_operators)
 * 功能概述：为 Lv-00 几何证明系统引入基本模态逻辑。提供必然算子（□）
 *          和可能算子（◇），基于 Kripke 语义的几何约束可达关系框架。
 *          支持嵌套模态公式、模态对偶转换和有效性检查。
 *          使用基本模态逻辑 K 系统。
 *
 * 主要 API：
 *   - lv_modal_world_create / destroy / assert  — 世界管理
 *   - lv_modal_frame_create / destroy / add_world — 框架管理
 *   - lv_modal_frame_set_reachability / is_reachable — 可达关系
 *   - lv_modal_formula_create / destroy          — 模态公式
 *   - lv_modal_evaluate / check_validity         — 模态评估
 *   - lv_modal_possible_to_necessary_not         — 对偶转换
 *   - lv_modal_frame_create_geometric_default    — 默认几何框架
 *
 * 使用示例：
 lv_PUBLIC_API *   lvModalFrame *frame = lv_modal_frame_create_geometric_default();
 lv_PUBLIC_API *   lvModalFormula *f = lv_modal_assert_point_must_on_line(frame, pt, ln);
 *   lvModalEvalResult result;
 lv_PUBLIC_API *   lv_modal_evaluate(frame, f, 1, &result);
 *
 * @version 1.1.0
 * ======================================================================== */

/**
 * @file modal_operators.h
 * @brief 模态逻辑扩展 —— 必要性与可能性的基本操作符
 */

#ifndef lv_MODAL_OPERATORS_H
#define lv_MODAL_OPERATORS_H

#include <stdbool.h>

#include "proof.h"
#include "three_valued_logic.h"
#include "lv/lv_utils.h"
#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */

#ifdef __cplusplus
extern "C" {
#endif

/* ============== 前向声明 ============== */
typedef struct lvModalFrame lvModalFrame;
typedef struct lvModalWorld lvModalWorld;
typedef struct lvModalFormula lvModalFormula;
typedef struct lvModalEvalResult lvModalEvalResult;

/* ============== 模态操作符 ============== */

/**
 * @brief 模态操作符类型
 *
 * lv_MODALOP_NECESSARY : □  必然算子  "在所有可达世界中成立"
 * lv_MODALOP_POSSIBLE  : ◇  可能算子  "在某个可达世界中成立"
 *
 * 对偶关系: ◇A = ¬□¬A  (可能即非必然非)
 *          □A = ¬◇¬A  (必然即非可能非)
 */
typedef enum {
    lv_MODALOP_NECESSARY = 0, /**< □ 必然 */
    lv_MODALOP_POSSIBLE = 1,  /**< ◇ 可能 */
    lv_MODALOP_NEGATION = 2   /**< ¬ 否定（对偶转换内部节点） */
} lvModalOperator;

/* ============== 模态可达关系 ============== */

/**
 * @brief 可达关系类型
 *
 * 定义世界 w' 从世界 w 可达的条件。
 * 在几何域中，可达关系对应几何变换的可允许性。
 */
typedef enum {
    lv_REACH_GEOMETRIC_IDENTITY,   /**< 恒等变换：世界等于自身 */
    lv_REACH_RIGID_TRANSFORM,      /**< 刚性变换：平移、旋转、反射 */
    lv_REACH_SIMILARITY_TRANSFORM, /**< 相似变换：缩放 + 刚体 */
    lv_REACH_AFFINE_TRANSFORM,     /**< 仿射变换：保持平行性 */
    lv_REACH_PROJECTIVE_TRANSFORM, /**< 射影变换 */
    lv_REACH_CONSTRAINT_INHERIT,   /**< 约束继承：子图可达 */
    lv_REACH_CUSTOM                /**< 自定义可达关系 */
} lvReachabilityType;

/* ============== 模态世界 ============== */

/**
 * @brief 模态世界（几何配置）
 *
 * 每个世界代表一种几何构造配置。
 * 世界之间通过可达关系连接。
 */
struct lvModalWorld {
    int id;                         /**< 世界ID */
    char *world_name;               /**< 世界名称（如 "原始配置", "经平移后的配置"） */
    ConstraintGraph *configuration; /**< 该世界的几何构造图（所有权） */
    lvDArray true_props;            /**< 在该世界中成立的命题数组（lvDArray of Proposition*） */
};

/* ============== 模态框架 ============== */

/**
 * @brief Kripke 模态框架 <W, R>
 *
 * W: 世界集合
 * R: 世界间的可达关系
 */
struct lvModalFrame {
    lvDArray worlds; /**< 世界数组（lvDArray of lvModalWorld*） */
    int current_world_id;  /**< 当前世界ID（1-based） */

    /* 可达关系：reachability[w_from][w_to] */
    lvReachabilityType **reach_matrix; /**< 可达关系类型矩阵 */
    int reach_dimension;               /**< 可达矩阵维度 */
};

/* ============== 模态公式 ============== */

/**
 * @brief 模态命题公式
 *
 * 支持嵌套模态算子，例如：□◇P（必然可能P）
 */
struct lvModalFormula {
    lvModalOperator op;             /**< 最外层模态算子 */
    struct Proposition *inner_prop; /**< 内层命题（不含模态算子） */
    struct lvModalFormula *sub;     /**< 子模态公式（嵌套时使用，如 □◇P 时 sub = ◇P） */
};

/* ============== 模态评估结果 ============== */

/**
 * @brief 模态公式评估结果
 */
struct lvModalEvalResult {
    lvTruthValue truth_value; /**< 评估真值 */
    int witness_world_id;     /**< 目击世界ID（◇ 算子时有效，-1 表示无） */
    char *explanation;        /**< 解释字符串 */
};

/* ============== 世界管理 API ============== */

/**
 * @brief 创建模态世界
 *
 * @param id            世界ID
 * @param world_name    世界名称（内部复制）
 * @param configuration 几何构造图（所有权转移）
 * @return 新分配的世界，失败返回 NULL
 */
lv_PUBLIC_API lvModalWorld *lv_modal_world_create(int id, const char *world_name, ConstraintGraph *configuration);

/**
 * @brief 销毁模态世界
 *
 * @param world 世界（可为 NULL）
 */
lv_PUBLIC_API void lv_modal_world_destroy(lvModalWorld *world);

/**
 * @brief 在此世界中声明命题为真
 *
 * @param world 世界
 * @param prop  命题（所有权转移给世界）
 * @return true 成功，false 失败
 */
lv_PUBLIC_API bool lv_modal_world_assert(lvModalWorld *world, Proposition *prop);

/**
 * @brief 检查命题是否在此世界中成立
 *
 * @param world 世界
 * @param prop  命题
 * @return 真值（三值逻辑）
 */
lv_PUBLIC_API lvTruthValue lv_modal_world_holds(const lvModalWorld *world, const Proposition *prop);

/* ============== 模态框架 API ============== */

/**
 * @brief 创建模态框架
 *
 * @return 新分配的框架，失败返回 NULL
 */
lv_PUBLIC_API lvModalFrame *lv_modal_frame_create(void);

/**
 * @brief 销毁模态框架
 *
 * @param frame 框架（可为 NULL）
 */
lv_PUBLIC_API void lv_modal_frame_destroy(lvModalFrame *frame);

/**
 * @brief 向框架添加世界
 *
 * @param frame 框架
 * @param world 世界（所有权转移）
 * @return true 成功，false 失败
 */
lv_PUBLIC_API bool lv_modal_frame_add_world(lvModalFrame *frame, lvModalWorld *world);

/**
 * @brief 设置两个世界之间的可达关系
 *
 * @param frame       框架
 * @param from_world_id 出发世界ID
 * @param to_world_id   目标世界ID
 * @param reach_type    可达关系类型
 * @return true 成功，false 失败
 */
lv_PUBLIC_API bool lv_modal_frame_set_reachability(lvModalFrame *frame, int from_world_id, int to_world_id,
                                                   lvReachabilityType reach_type);

/**
 * @brief 检查世界 w_to 是否从 w_from 可达
 *
 * @param frame       框架
 * @param from_world_id 出发世界ID
 * @param to_world_id   目标世界ID
 * @return true 可达，false 不可达
 */
lv_PUBLIC_API bool lv_modal_frame_is_reachable(const lvModalFrame *frame, int from_world_id, int to_world_id);

/**
 * @brief 获取从给定世界可达的所有世界ID列表
 *
 * @param frame     框架
 * @param world_id  出发世界ID
 * @param out_ids   输出可达世界ID数组（调用者释放）
 * @param out_count 输出数量
 * @return true 成功，false 失败
 */
lv_PUBLIC_API bool lv_modal_frame_get_reachable_worlds(const lvModalFrame *frame, int world_id, int **out_ids,
                                                       int *out_count);

/* ============== 模态公式 API ============== */

/**
 * @brief 创建模态公式
 *
 * @param op         最外层模态算子
 * @param inner_prop 内层命题（所有权转移）
 * @return 新分配的模态公式，失败返回 NULL
 */
lv_PUBLIC_API lvModalFormula *lv_modal_formula_create(lvModalOperator op, Proposition *inner_prop);

/**
 * @brief 创建嵌套模态公式
 *
 * @param op   最外层模态算子
 * @param sub  子模态公式（所有权转移）
 * @return 新分配的模态公式，失败返回 NULL
 */
lv_PUBLIC_API lvModalFormula *lv_modal_formula_create_nested(lvModalOperator op, lvModalFormula *sub);

/**
 * @brief 销毁模态公式
 *
 * @param formula 模态公式（可为 NULL）
 */
lv_PUBLIC_API void lv_modal_formula_destroy(lvModalFormula *formula);

/* ============== 模态评估 ============== */

/**
 * @brief 评估模态公式在给定框架和世界中的真值
 *
 * 评估语义（Kripke）：
 * - □P 在世界 w 中为真，当且仅当 P 在所有 w 可达的世界中为真
 * - ◇P 在世界 w 中为真，当且仅当 P 在某个 w 可达的世界中为真
 *
 * @param frame    模态框架
 * @param formula  模态公式
 * @param world_id 当前世界ID
 * @param result   输出评估结果
 * @return 0 成功，-1 参数错误
 */
lv_PUBLIC_API int lv_modal_evaluate(const lvModalFrame *frame, const lvModalFormula *formula, int world_id,
                                    lvModalEvalResult *result);

/**
 * @brief 检查公式是否为模态框架中的有效式
 *
 * 有效式：在所有世界的所有赋值下都为真的公式。
 * 例如：K 公理 □(A→B) → (□A → □B) 在所有框架中都有效。
 *
 * @param frame   模态框架
 * @param formula 模态公式
 * @return 真值
 */
lv_PUBLIC_API lvTruthValue lv_modal_check_validity(const lvModalFrame *frame, const lvModalFormula *formula);

/* ============== 模态算子转换 ============== */

/**
 * @brief 模态对偶转换：◇A → ¬□¬A
 *
 * 将可能算子转换为必然算子和否定的组合。
 * 通过 lv_MODALOP_NEGATION 否定节点表达对偶等式中的两处否定：
 * 结果公式结构为 ¬(□(¬A))。
 *
 * @param formula 原始模态公式
 * @return 转换后的模态公式（新分配，调用者 lv_modal_formula_destroy）；
 *         非 ◇ 公式或结构无效时返回 NULL
 */
lv_PUBLIC_API lvModalFormula *lv_modal_possible_to_necessary_not(const lvModalFormula *formula);

/**
 * @brief 模态对偶转换：□A → ¬◇¬A
 *
 * 将必然算子转换为可能算子和否定的组合。
 * 通过 lv_MODALOP_NEGATION 否定节点表达对偶等式中的两处否定：
 * 结果公式结构为 ¬(◇(¬A))。
 *
 * @param formula 原始模态公式
 * @return 转换后的模态公式（新分配，调用者 lv_modal_formula_destroy）；
 *         非 □ 公式或结构无效时返回 NULL
 */
lv_PUBLIC_API lvModalFormula *lv_modal_necessary_to_not_possible(const lvModalFormula *formula);

/* ============== 几何应用辅助 ============== */

/**
 * @brief 创建默认的几何模态框架
 *
 * 创建一个包含基本可达关系的框架：
 * - 世界 1："原始几何配置"
 * - 世界间默认可达关系为刚性变换（平移、旋转、反射）
 *
 * @return 新分配的框架，失败返回 NULL
 */
lv_PUBLIC_API lvModalFrame *lv_modal_frame_create_geometric_default(void);

/**
 * @brief 创建"点必须在线上"的模态断言
 *
 * 生成模态公式: □(onLine(point, line))
 *
 * @param frame    模态框架
 * @param point_id 点节点ID
 * @param line_id  线节点ID
 * @return 新分配的模态公式，失败返回 NULL
 */
lv_PUBLIC_API lvModalFormula *lv_modal_assert_point_must_on_line(lvModalFrame *frame, int point_id, int line_id);

/**
 * @brief 创建"点可以在线上"的模态断言
 *
 * 生成模态公式: ◇(onLine(point, line))
 *
 * @param frame    模态框架
 * @param point_id 点节点ID
 * @param line_id  线节点ID
 * @return 新分配的模态公式，失败返回 NULL
 */
lv_PUBLIC_API lvModalFormula *lv_modal_assert_point_can_on_line(lvModalFrame *frame, int point_id, int line_id);

/* ============== 释放评估结果 ============== */

/**
 * @brief 释放模态评估结果
 *
 * @param result 评估结果指针
 */
lv_PUBLIC_API void lv_modal_eval_result_destroy(lvModalEvalResult *result);

/* ============== 辅助函数 ============== */

/**
 * @brief 模态算子转字符串
 *
 * @param op 模态算子
 * @return 静态字符串（"□" / "◇"），请勿释放
 */
lv_PUBLIC_API const char *lv_modal_op_to_string(lvModalOperator op);

/**
 * @brief 可达关系类型转字符串
 *
 * @param type 可达关系类型
 * @return 静态字符串，请勿释放
 */
lv_PUBLIC_API const char *lv_reachability_type_to_string(lvReachabilityType type);

/**
 * @brief 模态公式转字符串
 *
 * @param formula 模态公式
 * @return 新分配的字符串（调用者需释放），失败返回 NULL
 */
lv_PUBLIC_API char *lv_modal_formula_to_string(const lvModalFormula *formula);

#ifdef __cplusplus
}
#endif

#endif /* lv_MODAL_OPERATORS_H */
