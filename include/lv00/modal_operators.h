/**
 * @file modal_operators.h
 * @brief 模态逻辑扩展 —— 必要性与可能性的基本操作符
 *
 * @details 为 Lv-00 的几何证明系统引入基本模态逻辑。
 *
 *          核心概念：
 *          - □ (NECESSARY / 框)：必然成立，在任何几何配置下都成立
 *          - ◇ (POSSIBLE / 钻)：可能成立，存在某个几何配置使命题成立
 *
 *          模态框架：
 *          采用基于几何约束的可达关系：
 *          世界 w' 从 w 可达，当且仅当从 w 到 w' 的几何变换是允许的
 *          （例如：平移、旋转、缩放等保持某种几何性质的变换）。
 *
 *          模态框架类型: <W, R, V>
 *          - W：所有可能的几何配置（世界集合）
 *          - R：可达关系（几何变换的可允许性）
 *          - V：命题赋值函数
 *
 *          应用场景：
 *          - "点 A 必须在直线 BC 上" = □(onLine(A, BC))  —— 必然
 *          - "点 A 可以在直线 BC 上" = ◇(onLine(A, BC)) —— 可能
 *          - "三角形的内角和必然是 180°" = □(sumAngles(triangle) = 180°)
 *          - "可能找到一个三等分角点" = ◇(trisection_possible(angle))
 *
 *          使用的逻辑系统: 基本模态逻辑 K（Kripke 语义）
 *          公理 K: □(A→B) → (□A → □B)
 *          必然化规则: 如果 A 是定理，则 □A 也是定理
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

#ifndef LV00_MODAL_OPERATORS_H
#define LV00_MODAL_OPERATORS_H

#include <stdbool.h>

#include "proof.h"
#include "three_valued_logic.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============== 前向声明 ============== */
typedef struct Lv00ModalFrame Lv00ModalFrame;
typedef struct Lv00ModalWorld Lv00ModalWorld;
typedef struct Lv00ModalFormula Lv00ModalFormula;
typedef struct Lv00ModalEvalResult Lv00ModalEvalResult;

/* ============== 模态操作符 ============== */

/**
 * @brief 模态操作符类型
 *
 * LV00_MODALOP_NECESSARY : □  必然算子  "在所有可达世界中成立"
 * LV00_MODALOP_POSSIBLE  : ◇  可能算子  "在某个可达世界中成立"
 *
 * 对偶关系: ◇A = ¬□¬A  (可能即非必然非)
 *          □A = ¬◇¬A  (必然即非可能非)
 */
typedef enum {
    LV00_MODALOP_NECESSARY = 0, /**< □ 必然 */
    LV00_MODALOP_POSSIBLE  = 1  /**< ◇ 可能 */
} Lv00ModalOperator;

/* ============== 模态可达关系 ============== */

/**
 * @brief 可达关系类型
 *
 * 定义世界 w' 从世界 w 可达的条件。
 * 在几何域中，可达关系对应几何变换的可允许性。
 */
typedef enum {
    LV00_REACH_GEOMETRIC_IDENTITY,   /**< 恒等变换：世界等于自身 */
    LV00_REACH_RIGID_TRANSFORM,      /**< 刚性变换：平移、旋转、反射 */
    LV00_REACH_SIMILARITY_TRANSFORM, /**< 相似变换：缩放 + 刚体 */
    LV00_REACH_AFFINE_TRANSFORM,     /**< 仿射变换：保持平行性 */
    LV00_REACH_PROJECTIVE_TRANSFORM, /**< 射影变换 */
    LV00_REACH_CONSTRAINT_INHERIT,   /**< 约束继承：子图可达 */
    LV00_REACH_CUSTOM                /**< 自定义可达关系 */
} Lv00ReachabilityType;

/* ============== 模态世界 ============== */

/**
 * @brief 模态世界（几何配置）
 *
 * 每个世界代表一种几何构造配置。
 * 世界之间通过可达关系连接。
 */
struct Lv00ModalWorld {
    int id;                         /**< 世界ID */
    char *world_name;               /**< 世界名称（如 "原始配置", "经平移后的配置"） */
    ConstraintGraph *configuration; /**< 该世界的几何构造图（所有权） */
    Proposition **true_props;       /**< 在该世界中成立的命题数组 */
    int true_prop_count;            /**< 成立命题数量 */
    int true_prop_capacity;         /**< 命题数组容量 */
};

/* ============== 模态框架 ============== */

/**
 * @brief Kripke 模态框架 <W, R>
 *
 * W: 世界集合
 * R: 世界间的可达关系
 */
struct Lv00ModalFrame {
    Lv00ModalWorld **worlds;        /**< 世界数组 */
    int world_count;                /**< 世界数量 */
    int world_capacity;             /**< 世界数组容量 */
    int current_world_id;           /**< 当前世界ID（1-based） */

    /* 可达关系：reachability[w_from][w_to] */
    Lv00ReachabilityType **reach_matrix; /**< 可达关系类型矩阵 */
    int reach_dimension;                 /**< 可达矩阵维度 */
};

/* ============== 模态公式 ============== */

/**
 * @brief 模态命题公式
 *
 * 支持嵌套模态算子，例如：□◇P（必然可能P）
 */
struct Lv00ModalFormula {
    Lv00ModalOperator op;           /**< 最外层模态算子 */
    struct Proposition *inner_prop; /**< 内层命题（不含模态算子） */
    struct Lv00ModalFormula *sub;   /**< 子模态公式（嵌套时使用，如 □◇P 时 sub = ◇P） */
};

/* ============== 模态评估结果 ============== */

/**
 * @brief 模态公式评估结果
 */
struct Lv00ModalEvalResult {
    Lv00TruthValue truth_value;     /**< 评估真值 */
    int witness_world_id;           /**< 目击世界ID（◇ 算子时有效，-1 表示无） */
    char *explanation;              /**< 解释字符串 */
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
Lv00ModalWorld *lv00_modal_world_create(int id, const char *world_name, ConstraintGraph *configuration);

/**
 * @brief 销毁模态世界
 *
 * @param world 世界（可为 NULL）
 */
void lv00_modal_world_destroy(Lv00ModalWorld *world);

/**
 * @brief 在此世界中声明命题为真
 *
 * @param world 世界
 * @param prop  命题（所有权转移给世界）
 * @return true 成功，false 失败
 */
bool lv00_modal_world_assert(Lv00ModalWorld *world, Proposition *prop);

/**
 * @brief 检查命题是否在此世界中成立
 *
 * @param world 世界
 * @param prop  命题
 * @return 真值（三值逻辑）
 */
Lv00TruthValue lv00_modal_world_holds(const Lv00ModalWorld *world, const Proposition *prop);

/* ============== 模态框架 API ============== */

/**
 * @brief 创建模态框架
 *
 * @return 新分配的框架，失败返回 NULL
 */
Lv00ModalFrame *lv00_modal_frame_create(void);

/**
 * @brief 销毁模态框架
 *
 * @param frame 框架（可为 NULL）
 */
void lv00_modal_frame_destroy(Lv00ModalFrame *frame);

/**
 * @brief 向框架添加世界
 *
 * @param frame 框架
 * @param world 世界（所有权转移）
 * @return true 成功，false 失败
 */
bool lv00_modal_frame_add_world(Lv00ModalFrame *frame, Lv00ModalWorld *world);

/**
 * @brief 设置两个世界之间的可达关系
 *
 * @param frame       框架
 * @param from_world_id 出发世界ID
 * @param to_world_id   目标世界ID
 * @param reach_type    可达关系类型
 * @return true 成功，false 失败
 */
bool lv00_modal_frame_set_reachability(Lv00ModalFrame *frame, int from_world_id, int to_world_id,
                                       Lv00ReachabilityType reach_type);

/**
 * @brief 检查世界 w_to 是否从 w_from 可达
 *
 * @param frame       框架
 * @param from_world_id 出发世界ID
 * @param to_world_id   目标世界ID
 * @return true 可达，false 不可达
 */
bool lv00_modal_frame_is_reachable(const Lv00ModalFrame *frame, int from_world_id, int to_world_id);

/**
 * @brief 获取从给定世界可达的所有世界ID列表
 *
 * @param frame     框架
 * @param world_id  出发世界ID
 * @param out_ids   输出可达世界ID数组（调用者释放）
 * @param out_count 输出数量
 * @return true 成功，false 失败
 */
bool lv00_modal_frame_get_reachable_worlds(const Lv00ModalFrame *frame, int world_id,
                                           int **out_ids, int *out_count);

/* ============== 模态公式 API ============== */

/**
 * @brief 创建模态公式
 *
 * @param op         最外层模态算子
 * @param inner_prop 内层命题（所有权转移）
 * @return 新分配的模态公式，失败返回 NULL
 */
Lv00ModalFormula *lv00_modal_formula_create(Lv00ModalOperator op, Proposition *inner_prop);

/**
 * @brief 创建嵌套模态公式
 *
 * @param op   最外层模态算子
 * @param sub  子模态公式（所有权转移）
 * @return 新分配的模态公式，失败返回 NULL
 */
Lv00ModalFormula *lv00_modal_formula_create_nested(Lv00ModalOperator op, Lv00ModalFormula *sub);

/**
 * @brief 销毁模态公式
 *
 * @param formula 模态公式（可为 NULL）
 */
void lv00_modal_formula_destroy(Lv00ModalFormula *formula);

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
int lv00_modal_evaluate(const Lv00ModalFrame *frame, const Lv00ModalFormula *formula,
                        int world_id, Lv00ModalEvalResult *result);

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
Lv00TruthValue lv00_modal_check_validity(const Lv00ModalFrame *frame, const Lv00ModalFormula *formula);

/* ============== 模态算子转换 ============== */

/**
 * @brief 模态对偶转换：◇A → ¬□¬A
 *
 * 将可能算子转换为必然算子和否定的组合。
 *
 * @param formula 原始模态公式
 * @return 转换后的模态公式（新分配），失败返回 NULL
 */
Lv00ModalFormula *lv00_modal_possible_to_necessary_not(const Lv00ModalFormula *formula);

/**
 * @brief 模态对偶转换：□A → ¬◇¬A
 *
 * 将必然算子转换为可能算子和否定的组合。
 *
 * @param formula 原始模态公式
 * @return 转换后的模态公式（新分配），失败返回 NULL
 */
Lv00ModalFormula *lv00_modal_necessary_to_not_possible(const Lv00ModalFormula *formula);

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
Lv00ModalFrame *lv00_modal_frame_create_geometric_default(void);

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
Lv00ModalFormula *lv00_modal_assert_point_must_on_line(Lv00ModalFrame *frame, int point_id, int line_id);

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
Lv00ModalFormula *lv00_modal_assert_point_can_on_line(Lv00ModalFrame *frame, int point_id, int line_id);

/* ============== 释放评估结果 ============== */

/**
 * @brief 释放模态评估结果
 *
 * @param result 评估结果指针
 */
void lv00_modal_eval_result_destroy(Lv00ModalEvalResult *result);

/* ============== 辅助函数 ============== */

/**
 * @brief 模态算子转字符串
 *
 * @param op 模态算子
 * @return 静态字符串（"□" / "◇"），请勿释放
 */
const char *lv00_modal_op_to_string(Lv00ModalOperator op);

/**
 * @brief 可达关系类型转字符串
 *
 * @param type 可达关系类型
 * @return 静态字符串，请勿释放
 */
const char *lv00_reachability_type_to_string(Lv00ReachabilityType type);

/**
 * @brief 模态公式转字符串
 *
 * @param formula 模态公式
 * @return 新分配的字符串（调用者需释放），失败返回 NULL
 */
char *lv00_modal_formula_to_string(const Lv00ModalFormula *formula);

#ifdef __cplusplus
}
#endif

#endif /* LV00_MODAL_OPERATORS_H */
