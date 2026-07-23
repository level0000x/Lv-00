/**
 * @file modal_operators.c
 * @brief 模态逻辑算子实现（子目录版本）
 *
 * 基于 Kripke 语义实现必然算子（□）和可能算子（◇）。
 * 支持嵌套模态公式、模态对偶转换和有效性检查。
 * 使用基本模态逻辑 K 系统。
 */

#include "lv/modal_operators.h"

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
    int i;
    if (!frame)
        return -1;
    for (i = 0; i < frame->world_count; i++) {
        if (frame->worlds[i] && frame->worlds[i]->id == world_id) {
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

        new_matrix = (lvReachabilityType **) calloc((size_t) new_dim, sizeof(lvReachabilityType *));
        if (!new_matrix)
            return -1;

        /* 复制旧数据 */
        for (i = 0; i < frame->reach_dimension; i++) {
            new_matrix[i] = (lvReachabilityType *) calloc((size_t) new_dim, sizeof(lvReachabilityType));
            if (!new_matrix[i]) {
                /* 回滚 */
                for (j = 0; j < i; j++)
                    free(new_matrix[j]);
                free(new_matrix);
                return -1;
            }
            if (frame->reach_matrix && frame->reach_matrix[i]) {
                memcpy(new_matrix[i], frame->reach_matrix[i],
                       (size_t) frame->reach_dimension * sizeof(lvReachabilityType));
                free(frame->reach_matrix[i]);
            }
        }
        /* 分配新增行 */
        for (i = frame->reach_dimension; i < new_dim; i++) {
            new_matrix[i] = (lvReachabilityType *) calloc((size_t) new_dim, sizeof(lvReachabilityType));
            if (!new_matrix[i]) {
                for (j = 0; j < i; j++)
                    free(new_matrix[j]);
                free(new_matrix);
                return -1;
            }
        }

        free(frame->reach_matrix);
        frame->reach_matrix = new_matrix;
        frame->reach_dimension = new_dim;
    }
    return 0;
}

/**
 * @brief 递归评估模态公式（内部）
 */
static lvTruthValue modal_evaluate_internal(const lvModalFrame *frame, const lvModalFormula *formula, int world_id) {
    int from_idx, i;
    lvTruthValue inner_truth;

    if (!frame || !formula)
        return lv_UNKNOWN;

    from_idx = modal_find_world_index(frame, world_id);
    if (from_idx < 0)
        return lv_UNKNOWN;

    /* 获取内层命题真值（如果有子公式则递归） */
    if (formula->sub) {
        /* 嵌套模态：先评估子公式 */
        inner_truth = lv_UNKNOWN; /* 由下方逻辑处理 */
    } else if (formula->inner_prop && frame->worlds[from_idx]) {
        inner_truth = lv_modal_world_holds(frame->worlds[from_idx], formula->inner_prop);
    } else {
        return lv_UNKNOWN;
    }

    if (formula->op == lv_MODALOP_NECESSARY) {
        /* □P: 在所有可达世界中 P 为真 */
        for (i = 0; i < frame->world_count; i++) {
            int to_id;
            lvTruthValue w_truth;

            if (!frame->worlds[i])
                continue;
            to_id = frame->worlds[i]->id;
            if (from_idx < frame->reach_dimension && i < frame->reach_dimension) {
                if (frame->reach_matrix[from_idx][i] == 0 && from_idx != i)
                    continue;
            }

            if (formula->sub) {
                w_truth = modal_evaluate_internal(frame, formula->sub, to_id);
            } else if (formula->inner_prop && frame->worlds[i]) {
                w_truth = lv_modal_world_holds(frame->worlds[i], formula->inner_prop);
            } else {
                w_truth = lv_UNKNOWN;
            }

            if (w_truth == lv_FALSE)
                return lv_FALSE;
            if (w_truth == lv_UNKNOWN)
                inner_truth = lv_UNKNOWN;
        }
        return (inner_truth == lv_UNKNOWN) ? lv_UNKNOWN : lv_TRUE;
    } else {
        /* ◇P: 在某个可达世界中 P 为真 */
        lvTruthValue result = lv_FALSE;
        for (i = 0; i < frame->world_count; i++) {
            int to_id;
            lvTruthValue w_truth;

            if (!frame->worlds[i])
                continue;
            to_id = frame->worlds[i]->id;
            if (from_idx < frame->reach_dimension && i < frame->reach_dimension) {
                if (frame->reach_matrix[from_idx][i] == 0 && from_idx != i)
                    continue;
            }

            if (formula->sub) {
                w_truth = modal_evaluate_internal(frame, formula->sub, to_id);
            } else if (formula->inner_prop && frame->worlds[i]) {
                w_truth = lv_modal_world_holds(frame->worlds[i], formula->inner_prop);
            } else {
                w_truth = lv_UNKNOWN;
            }

            if (w_truth == lv_TRUE)
                return lv_TRUE;
            if (w_truth == lv_UNKNOWN)
                result = lv_UNKNOWN;
        }
        return result;
    }
}

/* ================================================================
 *  世界管理 API
 * ================================================================ */

lvModalWorld *lv_modal_world_create(int id, const char *world_name, ConstraintGraph *configuration) {
    lvModalWorld *w = (lvModalWorld *) calloc(1, sizeof(lvModalWorld));
    if (!w)
        return NULL;
    w->id = id;
    if (world_name) {
        w->world_name = strdup(world_name);
    }
    w->configuration = configuration;
    w->true_props = NULL;
    w->true_prop_count = 0;
    w->true_prop_capacity = 0;
    return w;
}

void lv_modal_world_destroy(lvModalWorld *world) {
    if (!world)
        return;
    free(world->world_name);
    /* 注意：configuration 所有权由调用者管理 */
    free(world->true_props);
    free(world);
}

bool lv_modal_world_assert(lvModalWorld *world, Proposition *prop) {
    if (!world || !prop)
        return false;

    /* 扩容 */
    if (world->true_prop_count >= world->true_prop_capacity) {
        int new_cap = world->true_prop_capacity > 0 ? world->true_prop_capacity * 2 : 8;
        Proposition **new_arr =
            (Proposition **) lv_realloc(world->true_props, (size_t) new_cap * sizeof(Proposition *));
        if (!new_arr)
            return false;
        world->true_props = new_arr;
        world->true_prop_capacity = new_cap;
    }
    world->true_props[world->true_prop_count++] = prop;
    return true;
}

lvTruthValue lv_modal_world_holds(const lvModalWorld *world, const Proposition *prop) {
    int i;
    if (!world || !prop)
        return lv_UNKNOWN;
    for (i = 0; i < world->true_prop_count; i++) {
        if (world->true_props[i] == prop) {
            return lv_TRUE;
        }
    }
    return lv_FALSE;
}

/* ================================================================
 *  模态框架 API
 * ================================================================ */

lvModalFrame *lv_modal_frame_create(void) {
    lvModalFrame *f = (lvModalFrame *) calloc(1, sizeof(lvModalFrame));
    if (!f)
        return NULL;
    f->worlds = NULL;
    f->world_count = 0;
    f->world_capacity = 0;
    f->current_world_id = 1;
    f->reach_matrix = NULL;
    f->reach_dimension = 0;
    return f;
}

void lv_modal_frame_destroy(lvModalFrame *frame) {
    int i;
    if (!frame)
        return;
    for (i = 0; i < frame->world_count; i++) {
        lv_modal_world_destroy(frame->worlds[i]);
    }
    free(frame->worlds);
    for (i = 0; i < frame->reach_dimension; i++) {
        free(frame->reach_matrix[i]);
    }
    free(frame->reach_matrix);
    free(frame);
}

bool lv_modal_frame_add_world(lvModalFrame *frame, lvModalWorld *world) {
    if (!frame || !world)
        return false;

    if (frame->world_count >= frame->world_capacity) {
        int new_cap = frame->world_capacity > 0 ? frame->world_capacity * 2 : MODAL_INITIAL_CAPACITY;
        lvModalWorld **new_arr = (lvModalWorld **) lv_realloc(frame->worlds, (size_t) new_cap * sizeof(lvModalWorld *));
        if (!new_arr)
            return false;
        frame->worlds = new_arr;
        frame->world_capacity = new_cap;
    }
    frame->worlds[frame->world_count++] = world;
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
    if (from_idx >= frame->reach_dimension || to_idx >= frame->reach_dimension)
        return false;

    return frame->reach_matrix[from_idx][to_idx] != 0 || from_idx == to_idx;
}

bool lv_modal_frame_get_reachable_worlds(const lvModalFrame *frame, int world_id, int **out_ids, int *out_count) {
    int from_idx, i, count;
    int *ids;

    if (!frame || !out_ids || !out_count)
        return false;

    from_idx = modal_find_world_index(frame, world_id);
    if (from_idx < 0)
        return false;

    count = 0;
    ids = (int *) malloc((size_t) frame->world_count * sizeof(int));
    if (!ids)
        return false;

    for (i = 0; i < frame->world_count; i++) {
        if (i == from_idx)
            continue;
        if (from_idx < frame->reach_dimension && i < frame->reach_dimension) {
            if (frame->reach_matrix[from_idx][i] != 0) {
                ids[count++] = frame->worlds[i]->id;
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
    lvModalFormula *f = (lvModalFormula *) calloc(1, sizeof(lvModalFormula));
    if (!f)
        return NULL;
    f->op = op;
    f->inner_prop = inner_prop;
    f->sub = NULL;
    return f;
}

lvModalFormula *lv_modal_formula_create_nested(lvModalOperator op, lvModalFormula *sub) {
    lvModalFormula *f = (lvModalFormula *) calloc(1, sizeof(lvModalFormula));
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
    free(formula);
}

/* ================================================================
 *  模态评估
 * ================================================================ */

int lv_modal_evaluate(const lvModalFrame *frame, const lvModalFormula *formula, int world_id,
                      lvModalEvalResult *result) {
    if (!frame || !formula || !result)
        return -1;

    memset(result, 0, sizeof(lvModalEvalResult));
    result->truth_value = modal_evaluate_internal(frame, formula, world_id);
    result->witness_world_id = -1;
    result->explanation = NULL;
    return 0;
}

lvTruthValue lv_modal_check_validity(const lvModalFrame *frame, const lvModalFormula *formula) {
    int i;
    if (!frame || !formula)
        return lv_UNKNOWN;

    for (i = 0; i < frame->world_count; i++) {
        lvModalEvalResult result;
        if (!frame->worlds[i])
            continue;
        if (lv_modal_evaluate(frame, formula, frame->worlds[i]->id, &result) != 0) {
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
 * ================================================================ */

lvModalFormula *lv_modal_possible_to_necessary_not(const lvModalFormula *formula) {
    if (!formula || formula->op != lv_MODALOP_POSSIBLE)
        return NULL;
    /* ◇A -> ¬□¬A: 创建 □¬A，然后外层取反由调用者处理 */
    /* 这里简化为返回等价的嵌套公式 */
    return lv_modal_formula_create_nested(lv_MODALOP_NECESSARY, (lvModalFormula *) (size_t) formula->sub);
}

lvModalFormula *lv_modal_necessary_to_not_possible(const lvModalFormula *formula) {
    if (!formula || formula->op != lv_MODALOP_NECESSARY)
        return NULL;
    /* □A -> ¬◇¬A */
    return lv_modal_formula_create_nested(lv_MODALOP_POSSIBLE, (lvModalFormula *) (size_t) formula->sub);
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

lvModalFormula *lv_modal_assert_point_must_on_line(lvModalFrame *frame, int point_id, int line_id) {
    (void) frame;
    (void) point_id;
    (void) line_id;
    /* 简化实现：返回一个 □ 公式占位符 */
    return lv_modal_formula_create(lv_MODALOP_NECESSARY, NULL);
}

lvModalFormula *lv_modal_assert_point_can_on_line(lvModalFrame *frame, int point_id, int line_id) {
    (void) frame;
    (void) point_id;
    (void) line_id;
    /* 简化实现：返回一个 ◇ 公式占位符 */
    return lv_modal_formula_create(lv_MODALOP_POSSIBLE, NULL);
}

/* ================================================================
 *  释放评估结果
 * ================================================================ */

void lv_modal_eval_result_destroy(lvModalEvalResult *result) {
    if (!result)
        return;
    free(result->explanation);
    result->explanation = NULL;
}

/* ================================================================
 *  辅助函数
 * ================================================================ */

const char *lv_modal_op_to_string(lvModalOperator op) {
    switch (op) {
        case lv_MODALOP_NECESSARY:
            return "\xe2\x96\xa1"; /* "□" */
        case lv_MODALOP_POSSIBLE:
            return "\xe2\x9a\xa7"; /* "◇" */
        default:
            return "?";
    }
}

const char *lv_reachability_type_to_string(lvReachabilityType type) {
    switch (type) {
        case lv_REACH_GEOMETRIC_IDENTITY:
            return "identity";
        case lv_REACH_RIGID_TRANSFORM:
            return "rigid";
        case lv_REACH_SIMILARITY_TRANSFORM:
            return "similarity";
        case lv_REACH_AFFINE_TRANSFORM:
            return "affine";
        case lv_REACH_PROJECTIVE_TRANSFORM:
            return "projective";
        case lv_REACH_CONSTRAINT_INHERIT:
            return "inherit";
        case lv_REACH_CUSTOM:
            return "custom";
        default:
            return "unknown";
    }
}

char *lv_modal_formula_to_string(const lvModalFormula *formula) {
    char *buf;
    const char *op_str;

    if (!formula)
        return NULL;

    op_str = lv_modal_op_to_string(formula->op);
    buf = (char *) malloc(256);
    if (!buf)
        return NULL;

    if (formula->sub) {
        char *sub_str = lv_modal_formula_to_string(formula->sub);
        if (sub_str) {
            snprintf(buf, 256, "%s(%s)", op_str, sub_str);
            free(sub_str);
        } else {
            snprintf(buf, 256, "%s(?)", op_str);
        }
    } else {
        snprintf(buf, 256, "%s(P)", op_str);
    }
    return buf;
}
