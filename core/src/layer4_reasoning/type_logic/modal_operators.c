/**
 * @file modal_operators.c
 * @brief 模态逻辑算子实现（子目录版本）
 *
 * 基于 Kripke 语义实现必然算子（□）和可能算子（◇）。
 * 支持嵌套模态公式、模态对偶转换和有效性检查。
 * 使用基本模态逻辑 K 系统。
 */

#include "lv/modal_operators.h"
#include "lv/lv_xmacro.h"
#include "lv/lv_utils.h"
#include "lv/constraint_graph.h" /* graph_add_incidence：几何断言关联真实约束 */
#include "lv/error_codes.h" /* lv_RETURN_ERROR / lv_ERROR_UNSUPPORTED */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 *  内部常量
 * ================================================================ */

#define MODAL_MAX_WORLDS 64       /**< 最大世界数量 */
#define MODAL_INITIAL_CAPACITY 16 /**< 初始数组容量 */

/* ================================================================
 *  内部辅助函数
 * ================================================================ */

/**
 * @brief 在世界数组中按 ID 查找世界索引
 * @return 索引 (0-based)，未找到返回 -1
 */
static int modal_find_world_index(const lvModalFrame *frame, int world_id) {
    if (!frame)
        return -1;
    for (int i = 0; i < frame->worlds.count; i++) {
        lvModalWorld **wp = (lvModalWorld **)lv_darray_get(&frame->worlds, i);
        if (wp && *wp && (*wp)->id == world_id) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief 确保可达矩阵足够大
 * @return 0 成功，-1 失败
 */
static int modal_ensure_reach_matrix(lvModalFrame *frame, int needed_dim) {
    int i, j;
    lvReachabilityType **new_matrix;

    if (!frame)
        return -1;
    if (frame->reach_dimension >= needed_dim)
        return 0;

    {
        int new_dim = frame->reach_dimension > 0 ? frame->reach_dimension : MODAL_INITIAL_CAPACITY;
        while (new_dim < needed_dim) {
            new_dim *= 2;
        }

        new_matrix = (lvReachabilityType **) lv_calloc((size_t) new_dim, sizeof(lvReachabilityType *));
        if (!new_matrix)
            return -1;

        /* 复制旧数据 */
        for (i = 0; i < frame->reach_dimension; i++) {
            new_matrix[i] = (lvReachabilityType *) lv_calloc((size_t) new_dim, sizeof(lvReachabilityType));
            if (!new_matrix[i]) {
                /* 回滚 */
                for (j = 0; j < i; j++)
                    lv_free((void **) &new_matrix[j]);
                lv_free((void **) &new_matrix);
                return -1;
            }
            if (frame->reach_matrix && frame->reach_matrix[i]) {
                memcpy(new_matrix[i], frame->reach_matrix[i],
                       (size_t) frame->reach_dimension * sizeof(lvReachabilityType));
                lv_free((void **) &frame->reach_matrix[i]);
            }
        }
        /* 分配新增行 */
        for (i = frame->reach_dimension; i < new_dim; i++) {
            new_matrix[i] = (lvReachabilityType *) lv_calloc((size_t) new_dim, sizeof(lvReachabilityType));
            if (!new_matrix[i]) {
                for (j = 0; j < i; j++)
                    lv_free((void **) &new_matrix[j]);
                lv_free((void **) &new_matrix);
                return -1;
            }
        }

        lv_free((void **) &frame->reach_matrix);
        frame->reach_matrix = new_matrix;
        frame->reach_dimension = new_dim;
    }
    return 0;
}

/**
 * @brief 递归评估模态公式（内部）
 *
 * @param frame         模态框架
 * @param formula       模态公式
 * @param world_id      当前评估世界ID
 * @param out_witness_id 输出：目击世界ID（◇为真时的证明世界，□为假时的反例世界，¬ 时透传子式），可为NULL
 * @return 真值
 *
 * [重构] □ 与 ◇ 的遍历逻辑原为两份近乎相同的循环（各 ~30 行），仅聚合
 * 语义不同（□：全真才真、任一假即假；◇：任一真即真、全假才假），
 * 收敛为单一循环 + is_necessary 分支；并新增 ¬ 否定节点支持（对偶转换
 * 真实实现的基础）。
 */
static lvTruthValue modal_evaluate_internal(const lvModalFrame *frame, const lvModalFormula *formula, int world_id,
                                            int *out_witness_id) {
    if (!frame || !formula)
        return lv_UNKNOWN;

    /* 否定节点：¬A 真值 = A 真值取反（三值：TRUE↔FALSE，UNKNOWN 保持）；
     * A 可为子公式（sub）或命题（inner_prop，对偶转换的 ¬P 形式）；
     * witness 透传（子式 □/◇ 已填充反例/目击世界，外层 ¬ 无需改动）。 */
    if (formula->op == lv_MODALOP_NEGATION) {
        lvTruthValue sub_truth;
        if (formula->sub) {
            sub_truth = modal_evaluate_internal(frame, formula->sub, world_id, out_witness_id);
        } else if (formula->inner_prop) {
            /* ¬P 形式：在给定世界评估 P 的 holds */
            int idx = modal_find_world_index(frame, world_id);
            if (idx < 0)
                return lv_UNKNOWN;
            lvModalWorld **wp = (lvModalWorld **) lv_darray_get(&frame->worlds, idx);
            sub_truth = (wp && *wp) ? lv_modal_world_holds(*wp, formula->inner_prop) : lv_UNKNOWN;
        } else {
            return lv_UNKNOWN;
        }
        if (sub_truth == lv_TRUE)
            return lv_FALSE;
        if (sub_truth == lv_FALSE)
            return lv_TRUE;
        return lv_UNKNOWN;
    }

    int from_idx = modal_find_world_index(frame, world_id);
    if (from_idx < 0)
        return lv_UNKNOWN;

    /* 验证公式结构有效 */
    if (!formula->sub && !formula->inner_prop)
        return lv_UNKNOWN;

    bool is_necessary = (formula->op == lv_MODALOP_NECESSARY);
    lvTruthValue result = is_necessary ? lv_TRUE : lv_FALSE;

    for (int i = 0; i < frame->worlds.count; i++) {
        lvModalWorld **wp = (lvModalWorld **)lv_darray_get(&frame->worlds, i);
        if (!wp || !*wp)
            continue;
        int to_id = (*wp)->id;
        if (from_idx < frame->reach_dimension && i < frame->reach_dimension) {
            if (frame->reach_matrix[from_idx][i] == 0 && from_idx != i)
                continue;
        }

        lvTruthValue w_truth;
        if (formula->sub) {
            w_truth = modal_evaluate_internal(frame, formula->sub, to_id, NULL);
        } else if (formula->inner_prop) {
            w_truth = lv_modal_world_holds(*wp, formula->inner_prop);
        } else {
            w_truth = lv_UNKNOWN;
        }

        if (is_necessary) {
            /* □P：任一可达世界为假 → 假（记录反例世界） */
            if (w_truth == lv_FALSE) {
                if (out_witness_id)
                    *out_witness_id = to_id;
                return lv_FALSE;
            }
            if (w_truth == lv_UNKNOWN)
                result = lv_UNKNOWN;
        } else {
            /* ◇P：任一可达世界为真 → 真（记录目击世界） */
            if (w_truth == lv_TRUE) {
                if (out_witness_id)
                    *out_witness_id = to_id;
                return lv_TRUE;
            }
            if (w_truth == lv_UNKNOWN)
                result = lv_UNKNOWN;
        }
    }
    return result;
}

/* ================================================================
 *  世界管理 API
 * ================================================================ */

lvModalWorld *lv_modal_world_create(int id, const char *world_name, ConstraintGraph *configuration) {
    lvModalWorld *w = (lvModalWorld *) lv_calloc(1, sizeof(lvModalWorld));
    if (!w)
        return NULL;
    w->id = id;
    if (world_name) {
        w->world_name = lv_strdup(world_name);
    }
    w->configuration = configuration;
    lv_darray_init(&w->true_props, sizeof(Proposition *));
    return w;
}

void lv_modal_world_destroy(lvModalWorld *world) {
    if (!world)
        return;
    lv_free((void **) &world->world_name);
    /* 注意：configuration 所有权由调用者管理 */
    lv_darray_free(&world->true_props);
    lv_free((void **) &world);
}

bool lv_modal_world_assert(lvModalWorld *world, Proposition *prop) {
    if (!world || !prop)
        return false;

    if (lv_darray_push(&world->true_props, &prop) < 0)
        return false;
    return true;
}

lvTruthValue lv_modal_world_holds(const lvModalWorld *world, const Proposition *prop) {
    if (!world || !prop)
        return lv_UNKNOWN;
    for (int i = 0; i < world->true_props.count; i++) {
        Proposition **pp = (Proposition **)lv_darray_get(&world->true_props, i);
        if (pp && *pp == prop) {
            return lv_TRUE;
        }
    }
    return lv_FALSE;
}

/* ================================================================
 *  模态框架 API
 * ================================================================ */

lvModalFrame *lv_modal_frame_create(void) {
    lvModalFrame *f = (lvModalFrame *) lv_calloc(1, sizeof(lvModalFrame));
    if (!f)
        return NULL;
    lv_darray_init(&f->worlds, sizeof(lvModalWorld *));
    f->current_world_id = 1;
    f->reach_matrix = NULL;
    f->reach_dimension = 0;
    return f;
}

void lv_modal_frame_destroy(lvModalFrame *frame) {
    if (!frame)
        return;
    for (int i = 0; i < frame->worlds.count; i++) {
        lvModalWorld **wp = (lvModalWorld **)lv_darray_get(&frame->worlds, i);
        if (wp && *wp)
            lv_modal_world_destroy(*wp);
    }
    lv_darray_free(&frame->worlds);
    for (int i = 0; i < frame->reach_dimension; i++) {
        lv_free((void **) &frame->reach_matrix[i]);
    }
    lv_free((void **) &frame->reach_matrix);
    lv_free((void **) &frame);
}

bool lv_modal_frame_add_world(lvModalFrame *frame, lvModalWorld *world) {
    if (!frame || !world)
        return false;

    if (lv_darray_push(&frame->worlds, &world) < 0)
        return false;
    return true;
}

bool lv_modal_frame_set_reachability(lvModalFrame *frame, int from_world_id, int to_world_id,
                                     lvReachabilityType reach_type) {
    int from_idx, to_idx;
    if (!frame)
        return false;

    from_idx = modal_find_world_index(frame, from_world_id);
    to_idx = modal_find_world_index(frame, to_world_id);
    if (from_idx < 0 || to_idx < 0)
        return false;

    {
        int needed = (from_idx > to_idx ? from_idx : to_idx) + 1;
        if (modal_ensure_reach_matrix(frame, needed) != 0)
            return false;
    }

    frame->reach_matrix[from_idx][to_idx] = reach_type;
    return true;
}

bool lv_modal_frame_is_reachable(const lvModalFrame *frame, int from_world_id, int to_world_id) {
    int from_idx, to_idx;
    if (!frame)
        return false;

    from_idx = modal_find_world_index(frame, from_world_id);
    to_idx = modal_find_world_index(frame, to_world_id);
    if (from_idx < 0 || to_idx < 0)
        return false;
    /* 自可达（from == to）恒成立，不依赖可达矩阵（与评估语义一致：
     * modal_evaluate_internal 中 from_idx == i 时跳过矩阵检查）。
     * [修复] 原实现先查 reach_dimension 再判 from==to，导致未调用
     * set_reachability 时（reach_dimension == 0）自可达误判为 false。 */
    if (from_idx == to_idx)
        return true;
    if (from_idx >= frame->reach_dimension || to_idx >= frame->reach_dimension)
        return false;

    return frame->reach_matrix[from_idx][to_idx] != 0;
}

bool lv_modal_frame_get_reachable_worlds(const lvModalFrame *frame, int world_id, int **out_ids, int *out_count) {
    if (!frame || !out_ids || !out_count)
        return false;

    int from_idx = modal_find_world_index(frame, world_id);
    if (from_idx < 0)
        return false;

    int count = 0;
    int *ids = (int *) lv_malloc((size_t) frame->worlds.count * sizeof(int));
    if (!ids)
        return false;

    for (int i = 0; i < frame->worlds.count; i++) {
        if (i == from_idx)
            continue;
        if (from_idx < frame->reach_dimension && i < frame->reach_dimension) {
            if (frame->reach_matrix[from_idx][i] != 0) {
                lvModalWorld **wp = (lvModalWorld **)lv_darray_get(&frame->worlds, i);
                if (wp && *wp)
                    ids[count++] = (*wp)->id;
            }
        }
    }

    *out_ids = ids;
    *out_count = count;
    return true;
}

/* ================================================================
 *  模态公式 API
 * ================================================================ */

lvModalFormula *lv_modal_formula_create(lvModalOperator op, Proposition *inner_prop) {
    lvModalFormula *f = (lvModalFormula *) lv_calloc(1, sizeof(lvModalFormula));
    if (!f)
        return NULL;
    f->op = op;
    f->inner_prop = inner_prop;
    f->sub = NULL;
    return f;
}

lvModalFormula *lv_modal_formula_create_nested(lvModalOperator op, lvModalFormula *sub) {
    lvModalFormula *f = (lvModalFormula *) lv_calloc(1, sizeof(lvModalFormula));
    if (!f)
        return NULL;
    f->op = op;
    f->inner_prop = NULL;
    f->sub = sub;
    return f;
}

void lv_modal_formula_destroy(lvModalFormula *formula) {
    if (!formula)
        return;
    lv_modal_formula_destroy(formula->sub);
    lv_free((void **) &formula);
}

/* ================================================================
 *  模态评估
 * ================================================================ */

int lv_modal_evaluate(const lvModalFrame *frame, const lvModalFormula *formula, int world_id,
                      lvModalEvalResult *result) {
    int witness_id = -1;
    const char *op_str;
    char buf[256];

    if (!frame || !formula || !result)
        return -1;

    memset(result, 0, sizeof(lvModalEvalResult));

    op_str = lv_modal_op_to_string(formula->op);
    result->truth_value = modal_evaluate_internal(frame, formula, world_id, &witness_id);
    result->witness_world_id = witness_id;

    /* 生成解释字符串 */
    switch (result->truth_value) {
        case lv_TRUE:
            if (formula->op == lv_MODALOP_POSSIBLE && witness_id >= 0) {
                lv_snprintf(buf, sizeof(buf), "%sP 在世界 %d 中为真，目击世界: %d",
                            op_str, world_id, witness_id);
            } else {
                lv_snprintf(buf, sizeof(buf), "%sP 在世界 %d 中为真", op_str, world_id);
            }
            break;
        case lv_FALSE:
            if (formula->op == lv_MODALOP_NECESSARY && witness_id >= 0) {
                lv_snprintf(buf, sizeof(buf), "%sP 在世界 %d 中为假，反例世界: %d",
                            op_str, world_id, witness_id);
            } else {
                lv_snprintf(buf, sizeof(buf), "%sP 在世界 %d 中为假", op_str, world_id);
            }
            break;
        default:
            lv_snprintf(buf, sizeof(buf), "%sP 在世界 %d 中的真值未知", op_str, world_id);
            break;
    }

    result->explanation = lv_strdup(buf);
    return 0;
}

lvTruthValue lv_modal_check_validity(const lvModalFrame *frame, const lvModalFormula *formula) {
    if (!frame || !formula)
        return lv_UNKNOWN;

    for (int i = 0; i < frame->worlds.count; i++) {
        lvModalWorld **wp = (lvModalWorld **)lv_darray_get(&frame->worlds, i);
        if (!wp || !*wp)
            continue;
        lvModalEvalResult result;
        if (lv_modal_evaluate(frame, formula, (*wp)->id, &result) != 0) {
            return lv_UNKNOWN;
        }
        if (result.truth_value != lv_TRUE) {
            return result.truth_value;
        }
    }
    return lv_TRUE;
}

/* ================================================================
 *  模态算子转换（对偶）
 * ================================================================
 *
 * 模态对偶：◇A ≡ ¬□¬A、□A ≡ ¬◇¬A。
 * v1.1.0 起通过 lv_MODALOP_NEGATION 否定节点真实实现：
 *   - 转换结果 = ¬(□(¬A)) / ¬(◇(¬A))，A 可为命题（inner_prop）或
 *     嵌套公式（sub）；
 *   - 输入公式的 sub 树被深拷贝（克隆），inner_prop 指针共享
 *     （lv_modal_formula_destroy 不销毁 inner_prop，无双重释放）；
 *   - 返回新公式归调用者销毁。
 */

/**
 * @brief 深拷贝模态公式树（仅 sub 树克隆；inner_prop 指针共享）
 */
static lvModalFormula *modal_formula_clone(const lvModalFormula *src) {
    if (!src)
        return NULL;
    lvModalFormula *copy = (lvModalFormula *) lv_calloc(1, sizeof(lvModalFormula));
    if (!copy)
        return NULL;
    copy->op = src->op;
    copy->inner_prop = src->inner_prop;
    copy->sub = src->sub ? modal_formula_clone(src->sub) : NULL;
    return copy;
}

lvModalFormula *lv_modal_possible_to_necessary_not(const lvModalFormula *formula) {
    if (!formula || formula->op != lv_MODALOP_POSSIBLE)
        return NULL;
    if (!formula->sub && !formula->inner_prop)
        return NULL;

    /* ¬A：A 为公式时克隆 sub，为命题时共享 inner_prop */
    lvModalFormula *not_a = NULL;
    if (formula->sub) {
        lvModalFormula *a_copy = modal_formula_clone(formula->sub);
        if (!a_copy)
            return NULL;
        not_a = lv_modal_formula_create_nested(lv_MODALOP_NEGATION, a_copy);
    } else {
        not_a = lv_modal_formula_create(lv_MODALOP_NEGATION, formula->inner_prop);
    }
    if (!not_a)
        return NULL;

    /* □(¬A) */
    lvModalFormula *box_not_a = lv_modal_formula_create_nested(lv_MODALOP_NECESSARY, not_a);
    if (!box_not_a) {
        lv_modal_formula_destroy(not_a);
        return NULL;
    }

    /* ¬(□(¬A)) */
    lvModalFormula *result = lv_modal_formula_create_nested(lv_MODALOP_NEGATION, box_not_a);
    if (!result) {
        lv_modal_formula_destroy(box_not_a);
        return NULL;
    }
    return result;
}

lvModalFormula *lv_modal_necessary_to_not_possible(const lvModalFormula *formula) {
    if (!formula || formula->op != lv_MODALOP_NECESSARY)
        return NULL;
    if (!formula->sub && !formula->inner_prop)
        return NULL;

    lvModalFormula *not_a = NULL;
    if (formula->sub) {
        lvModalFormula *a_copy = modal_formula_clone(formula->sub);
        if (!a_copy)
            return NULL;
        not_a = lv_modal_formula_create_nested(lv_MODALOP_NEGATION, a_copy);
    } else {
        not_a = lv_modal_formula_create(lv_MODALOP_NEGATION, formula->inner_prop);
    }
    if (!not_a)
        return NULL;

    lvModalFormula *diamond_not_a = lv_modal_formula_create_nested(lv_MODALOP_POSSIBLE, not_a);
    if (!diamond_not_a) {
        lv_modal_formula_destroy(not_a);
        return NULL;
    }

    lvModalFormula *result = lv_modal_formula_create_nested(lv_MODALOP_NEGATION, diamond_not_a);
    if (!result) {
        lv_modal_formula_destroy(diamond_not_a);
        return NULL;
    }
    return result;
}

/* ================================================================
 *  几何应用辅助
 * ================================================================ */

lvModalFrame *lv_modal_frame_create_geometric_default(void) {
    lvModalFrame *frame = lv_modal_frame_create();
    lvModalWorld *w1;
    if (!frame)
        return NULL;

    w1 = lv_modal_world_create(1, "原始几何配置", NULL);
    if (!w1) {
        lv_modal_frame_destroy(frame);
        return NULL;
    }
    if (!lv_modal_frame_add_world(frame, w1)) {
        lv_modal_world_destroy(w1);
        lv_modal_frame_destroy(frame);
        return NULL;
    }

    /* 默认自可达（恒等变换） */
    lv_modal_frame_set_reachability(frame, 1, 1, lv_REACH_GEOMETRIC_IDENTITY);
    return frame;
}

/**
 * @brief 内部：检查配置图是否已含 point_id 与 line_id 的 INCIDENCE 约束
 *
 * 重复断言同一 onLine 时 graph_add_incidence 因查重返回非 OK，但约束已
 * 存在（幂等语义：第二次断言仍应把约束写入配置图）。
 */
static bool modal_graph_has_incidence(ConstraintGraph *g, int point_id, int line_id) {
    if (!g)
        return false;
    int indices[16];
    int n = graph_find_constraints_involving(g, point_id, indices, 16);
    for (int i = 0; i < n; i++) {
        Constraint *c = graph_get_constraint(g, indices[i]);
        if (!c || c->type != INCIDENCE || c->participant_count != 2)
            continue;
        if ((c->participants[0] == point_id && c->participants[1] == line_id) ||
            (c->participants[0] == line_id && c->participants[1] == point_id))
            return true;
    }
    return false;
}

/**
 * @brief 内部：创建 onLine(point, line) 原子命题
 *
 * 行为补全（C-㊺续18）：若 frame 的当前世界携带配置图
 * （world->configuration），则：
 *   1. 副作用：graph_add_incidence 把真实 INCIDENCE 约束写入配置图
 *      （幂等查重），供几何工具消费；
 *   2. 命题 pattern 持有 graph_copy 独立快照副本——proposition_destroy
 *      会销毁 pattern（destroy_proposition_pattern → graph_destroy），
 *      故必须为副本而非共享引用，避免与 world 配置图所有权冲突。
 * 节点缺失/图缺失时回退为纯符号命题（仅命名，不崩溃）。
 */
static Proposition *modal_make_on_line_prop(lvModalFrame *frame, int point_id, int line_id) {
    Proposition *prop = proposition_create(0, PROPOSITION_TYPE_ATOMIC);
    if (!prop)
        return NULL;

    char buf[128];
    lv_snprintf(buf, sizeof(buf), "onLine(p%d, s%d)", point_id, line_id);
    prop->name = lv_strdup(buf);

    /* 关联真实几何约束：取当前世界（current_world_id 优先，否则首个）的配置图 */
    ConstraintGraph *config = NULL;
    if (frame) {
        int target_idx = modal_find_world_index(frame, frame->current_world_id);
        if (target_idx < 0 && frame->worlds.count > 0)
            target_idx = 0;
        if (target_idx >= 0) {
            lvModalWorld **wp = (lvModalWorld **) lv_darray_get(&frame->worlds, target_idx);
            if (wp && *wp)
                config = (*wp)->configuration;
        }
    }
    if (config) {
        /* 1. 真实约束写入配置图（幂等）；无效节点（如 999）时失败且不存在
         *    约束 → 回退纯符号（不做快照）。 */
        bool linked = false;
        if (graph_add_incidence(config, point_id, line_id) == ADD_CONSTRAINT_OK) {
            linked = true;
        } else if (modal_graph_has_incidence(config, point_id, line_id)) {
            linked = true; /* 重复断言：约束已存在 */
        }
        if (linked) {
            /* 2. 命题 pattern = 独立快照副本（所有权归命题，proposition_destroy 销毁） */
            ConstraintGraph *snapshot = graph_copy(config);
            if (snapshot)
                prop->pattern = snapshot;
        }
    }
    return prop;
}

lvModalFormula *lv_modal_assert_point_must_on_line(lvModalFrame *frame, int point_id, int line_id) {
    /* 创建原子命题 "point lies on line"（关联真实约束） */
    Proposition *prop = modal_make_on_line_prop(frame, point_id, line_id);
    if (!prop)
        return NULL;

    /* 返回 □(onLine(point, line)) */
    return lv_modal_formula_create(lv_MODALOP_NECESSARY, prop);
}

lvModalFormula *lv_modal_assert_point_can_on_line(lvModalFrame *frame, int point_id, int line_id) {
    /* 创建原子命题 "point lies on line"（关联真实约束） */
    Proposition *prop = modal_make_on_line_prop(frame, point_id, line_id);
    if (!prop)
        return NULL;

    /* 返回 ◇(onLine(point, line)) */
    return lv_modal_formula_create(lv_MODALOP_POSSIBLE, prop);
}

/* ================================================================
 *  释放评估结果
 * ================================================================ */

void lv_modal_eval_result_destroy(lvModalEvalResult *result) {
    if (!result)
        return;
    lv_free((void **) &result->explanation);
}

/* ================================================================
 *  辅助函数
 * ================================================================ */

/** @brief 模态算子 → Unicode 字符串查找表，按枚举值索引 */
static const char *kModalOpNames[] = {
    "\xe2\x96\xa1", /* lv_MODALOP_NECESSARY = 0 → "□" */
    "\xe2\x9a\xa7", /* lv_MODALOP_POSSIBLE  = 1 → "◇" */
    "\xc2\xac",     /* lv_MODALOP_NEGATION  = 2 → "¬" */
};

const char *lv_modal_op_to_string(lvModalOperator op) {
    if ((int)op >= 0 && (size_t)op < lv_ARRAY_SIZE(kModalOpNames))
        return kModalOpNames[(size_t)op];
    return "?";
}

/* ================================================================
 * 枚举 -> 名称 映射表（数据表化，替代 switch）
 * ================================================================ */

/** @brief lv_reachability_type_to_string 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_lv_reachability_type_to_string_entries[] = {
    {"identity", lv_REACH_GEOMETRIC_IDENTITY},
    {"rigid", lv_REACH_RIGID_TRANSFORM},
    {"similarity", lv_REACH_SIMILARITY_TRANSFORM},
    {"affine", lv_REACH_AFFINE_TRANSFORM},
    {"projective", lv_REACH_PROJECTIVE_TRANSFORM},
    {"inherit", lv_REACH_CONSTRAINT_INHERIT},
    {"custom", lv_REACH_CUSTOM},
};

const char *lv_reachability_type_to_string(lvReachabilityType type) {
    return lv_enum_to_str(s_lv_reachability_type_to_string_entries, lv_ARRAY_SIZE(s_lv_reachability_type_to_string_entries), (int) type, "unknown");
}

char *lv_modal_formula_to_string(const lvModalFormula *formula) {
    char *buf;
    const char *op_str;

    if (!formula)
        return NULL;

    op_str = lv_modal_op_to_string(formula->op);
    buf = (char *) lv_malloc(256);
    if (!buf)
        return NULL;

    if (formula->sub) {
        char *sub_str = lv_modal_formula_to_string(formula->sub);
        if (sub_str) {
            lv_snprintf(buf, 256, "%s(%s)", op_str, sub_str);
            lv_free((void **) &sub_str);
        } else {
            lv_snprintf(buf, 256, "%s(?)", op_str);
        }
    } else {
        lv_snprintf(buf, 256, "%s(P)", op_str);
    }
    return buf;
}
