/**
 * @file modal_operators.c
 * @brief 模态逻辑扩展 —— 必要性与可能性的基本操作符实现
 *
 * @details 实现 Lv-00 的模态逻辑模块，基于 Kripke 语义的基本模态逻辑 K。
 *
 *          核心概念：
 *          - □ (NECESSARY / 框)：必然成立，在所有可达世界中成立
 *          - ◇ (POSSIBLE / 钻)：可能成立，在某个可达世界中成立
 *
 *          模态框架 <W, R, V>：
 *          - W：所有可能的几何配置（世界集合）
 *          - R：可达关系（几何变换的可允许性）
 *          - V：命题赋值函数
 *
 *          Kripke 语义：
 *          - □P 在世界 w 中为真 ⟺ P 在所有 w 可达的世界中为真
 *          - ◇P 在世界 w 中为真 ⟺ P 在某个 w 可达的世界中为真
 *
 *          对偶关系：
 *          - ◇A = ¬□¬A  (可能即非必然非)
 *          - □A = ¬◇¬A  (必然即非可能非)
 *
 *          使用的逻辑系统: 基本模态逻辑 K
 *          公理 K: □(A→B) → (□A → □B)
 *          必然化规则: 如果 A 是定理，则 □A 也是定理
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "modal_operators.h"
#include "three_valued_logic.h"
#include "lv00_utils.h"
#include "proof.h"
#include "error_codes.h"

#include <stdio.h>
#include <string.h>

/* ============== 内部常量 ============== */

/** 初始世界数组容量 */
#define MODAL_INITIAL_WORLD_CAPACITY 8

/** 初始命题数组容量 */
#define MODAL_INITIAL_PROP_CAPACITY 8

/** 公式字符串缓冲区初始大小 */
#define MODAL_FORMULA_STR_BUF_SIZE 256

/* ============== 内部辅助函数（前向声明） ============== */

/**
 * @brief 根据世界ID查找世界指针
 *
 * @param frame 模态框架
 * @param world_id 世界ID
 * @return 世界指针，未找到返回 NULL
 */
static Lv00ModalWorld *modal_frame_find_world(const Lv00ModalFrame *frame, int world_id);

/**
 * @brief 确保可达关系矩阵维度足够
 *
 * 当添加新世界时，需要扩展可达关系矩阵。
 *
 * @param frame 模态框架
 * @param required_dim 所需的矩阵维度
 * @return true 成功，false 失败
 */
static bool modal_frame_ensure_reach_matrix(Lv00ModalFrame *frame, int required_dim);

/**
 * @brief 递归评估模态公式的内部实现
 *
 * @param frame    模态框架
 * @param formula  模态公式
 * @param world_id 当前世界ID
 * @param result   输出评估结果
 * @return 0 成功，-1 参数错误
 */
static int modal_evaluate_recursive(const Lv00ModalFrame *frame, const Lv00ModalFormula *formula,
                                    int world_id, Lv00ModalEvalResult *result);

/* ============== 世界管理 ============== */

/**
 * @brief 创建模态世界
 *
 * 分配并初始化一个模态世界结构体。
 * 世界包含一个几何构造图（ConstraintGraph）和一组成立命题。
 *
 * @param id            世界ID
 * @param world_name    世界名称（内部复制）
 * @param configuration 几何构造图（所有权转移，可为 NULL）
 * @return 新分配的世界，失败返回 NULL
 */
Lv00ModalWorld *lv00_modal_world_create(int id, const char *world_name, ConstraintGraph *configuration)
{
    Lv00ModalWorld *world = (Lv00ModalWorld *)lv00_calloc(1, sizeof(Lv00ModalWorld));
    if (!world) {
        lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "无法分配模态世界");
        return NULL;
    }

    world->id = id;
    world->configuration = configuration; /* 所有权转移，可为 NULL */

    /* 复制世界名称 */
    if (world_name) {
        world->world_name = lv00_strdup(world_name);
        if (!world->world_name) {
            lv00_free((void **)&world);
            lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "无法复制世界名称");
            return NULL;
        }
    }

    /* 初始化命题数组 */
    world->true_props = (Proposition **)lv00_calloc(MODAL_INITIAL_PROP_CAPACITY, sizeof(Proposition *));
    if (!world->true_props) {
        LV00_FREE_AND_NULL(world->world_name);
        lv00_free((void **)&world);
        lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "无法分配命题数组");
        return NULL;
    }
    world->true_prop_capacity = MODAL_INITIAL_PROP_CAPACITY;
    world->true_prop_count = 0;

    return world;
}

/**
 * @brief 销毁模态世界
 *
 * 释放世界名称、命题数组（不释放命题本身，由调用者管理）、
 * 以及几何构造图。
 *
 * @param world 世界（可为 NULL，此时无操作）
 */
void lv00_modal_world_destroy(Lv00ModalWorld *world)
{
    if (!world) {
        return;
    }

    /* 释放世界名称 */
    LV00_FREE_AND_NULL(world->world_name);

    /* 释放命题数组（不释放命题本身，所有权属于外部管理器） */
    LV00_FREE_AND_NULL(world->true_props);

    /* 释放几何构造图 */
    if (world->configuration) {
        /* ConstraintGraph 的释放由 constraint_graph 模块管理 */
        /* 此处仅置空，实际释放由上层调用者或框架销毁时统一处理 */
        world->configuration = NULL;
    }

    lv00_free((void **)&world);
}

/**
 * @brief 在此世界中声明命题为真
 *
 * 将命题添加到世界的成立命题列表中。
 * 如果数组容量不足，自动扩容。
 *
 * @param world 世界
 * @param prop  命题（所有权转移给世界）
 * @return true 成功，false 失败
 */
bool lv00_modal_world_assert(Lv00ModalWorld *world, Proposition *prop)
{
    LV00_CHECK_NULL(world, false);
    LV00_CHECK_NULL(prop, false);

    /* 检查是否需要扩容 */
    if (world->true_prop_count >= world->true_prop_capacity) {
        int new_capacity = world->true_prop_capacity * 2;
        Proposition **new_props = (Proposition **)lv00_realloc(
            world->true_props, (size_t)new_capacity * sizeof(Proposition *));
        if (!new_props) {
            lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "无法扩容命题数组");
            return false;
        }
        world->true_props = new_props;
        world->true_prop_capacity = new_capacity;
    }

    world->true_props[world->true_prop_count] = prop;
    world->true_prop_count++;

    return true;
}

/**
 * @brief 检查命题是否在此世界中成立
 *
 * 遍历世界的成立命题数组，通过命题 ID 进行匹配。
 * 如果找到 ID 相同的命题，则返回 TRUE；否则返回 UNKNOWN。
 *
 * 简化实现说明：
 * - 通过 prop->id 进行匹配比较
 * - 如果世界没有成立命题，返回 UNKNOWN
 * - 如果找到匹配的命题 ID，返回 TRUE
 *
 * @param world 世界
 * @param prop  命题
 * @return 真值（三值逻辑）
 */
Lv00TruthValue lv00_modal_world_holds(const Lv00ModalWorld *world, const Proposition *prop)
{
    if (!world || !prop) {
        return LV00_UNKNOWN;
    }

    /* 遍历成立命题数组，通过 ID 匹配 */
    for (int i = 0; i < world->true_prop_count; i++) {
        if (world->true_props[i] && world->true_props[i]->id == prop->id) {
            return LV00_TRUE;
        }
    }

    /* 未找到匹配的命题 */
    return LV00_UNKNOWN;
}

/* ============== 模态框架管理 ============== */

/**
 * @brief 创建模态框架
 *
 * 分配并初始化一个空的 Kripke 模态框架。
 * 初始状态不包含任何世界，可达关系矩阵维度为 0。
 *
 * @return 新分配的框架，失败返回 NULL
 */
Lv00ModalFrame *lv00_modal_frame_create(void)
{
    Lv00ModalFrame *frame = (Lv00ModalFrame *)lv00_calloc(1, sizeof(Lv00ModalFrame));
    if (!frame) {
        lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "无法分配模态框架");
        return NULL;
    }

    /* 初始化世界数组 */
    frame->worlds = (Lv00ModalWorld **)lv00_calloc(
        MODAL_INITIAL_WORLD_CAPACITY, sizeof(Lv00ModalWorld *));
    if (!frame->worlds) {
        lv00_free((void **)&frame);
        lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "无法分配世界数组");
        return NULL;
    }
    frame->world_capacity = MODAL_INITIAL_WORLD_CAPACITY;
    frame->world_count = 0;
    frame->current_world_id = 0;

    /* 可达关系矩阵初始为空 */
    frame->reach_matrix = NULL;
    frame->reach_dimension = 0;

    return frame;
}

/**
 * @brief 销毁模态框架
 *
 * 递归销毁框架中的所有世界，释放可达关系矩阵和框架自身。
 *
 * @param frame 框架（可为 NULL，此时无操作）
 */
void lv00_modal_frame_destroy(Lv00ModalFrame *frame)
{
    if (!frame) {
        return;
    }

    /* 销毁所有世界 */
    if (frame->worlds) {
        for (int i = 0; i < frame->world_count; i++) {
            if (frame->worlds[i]) {
                lv00_modal_world_destroy(frame->worlds[i]);
                frame->worlds[i] = NULL;
            }
        }
        lv00_free((void **)&frame->worlds);
    }

    /* 释放可达关系矩阵 */
    if (frame->reach_matrix) {
        for (int i = 0; i < frame->reach_dimension; i++) {
            LV00_FREE_AND_NULL(frame->reach_matrix[i]);
        }
        lv00_free((void **)&frame->reach_matrix);
    }

    lv00_free((void **)&frame);
}

/**
 * @brief 向框架添加世界
 *
 * 将世界添加到框架的世界集合中，同时扩展可达关系矩阵。
 * 添加后自动更新 current_world_id。
 *
 * @param frame 框架
 * @param world 世界（所有权转移）
 * @return true 成功，false 失败
 */
bool lv00_modal_frame_add_world(Lv00ModalFrame *frame, Lv00ModalWorld *world)
{
    LV00_CHECK_NULL(frame, false);
    LV00_CHECK_NULL(world, false);

    /* 检查是否需要扩容 */
    if (frame->world_count >= frame->world_capacity) {
        int new_capacity = frame->world_capacity * 2;
        Lv00ModalWorld **new_worlds = (Lv00ModalWorld **)lv00_realloc(
            frame->worlds, (size_t)new_capacity * sizeof(Lv00ModalWorld *));
        if (!new_worlds) {
            lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "无法扩容世界数组");
            return false;
        }
        frame->worlds = new_worlds;
        frame->world_capacity = new_capacity;
    }

    /* 扩展可达关系矩阵 */
    int new_dim = frame->world_count + 1;
    if (!modal_frame_ensure_reach_matrix(frame, new_dim)) {
        return false;
    }

    /* 添加世界 */
    frame->worlds[frame->world_count] = world;
    frame->world_count++;

    /* 更新当前世界ID计数器 */
    if (world->id >= frame->current_world_id) {
        frame->current_world_id = world->id + 1;
    }

    return true;
}

/**
 * @brief 设置两个世界之间的可达关系
 *
 * 在可达关系矩阵中设置从 from_world_id 到 to_world_id 的可达类型。
 * 世界 ID 会映射到矩阵索引。
 *
 * @param frame       框架
 * @param from_world_id 出发世界ID
 * @param to_world_id   目标世界ID
 * @param reach_type    可达关系类型
 * @return true 成功，false 失败
 */
bool lv00_modal_frame_set_reachability(Lv00ModalFrame *frame, int from_world_id, int to_world_id,
                                       Lv00ReachabilityType reach_type)
{
    LV00_CHECK_NULL(frame, false);

    /* 查找出发世界和目标世界的索引 */
    int from_idx = -1, to_idx = -1;
    for (int i = 0; i < frame->world_count; i++) {
        if (frame->worlds[i]->id == from_world_id) {
            from_idx = i;
        }
        if (frame->worlds[i]->id == to_world_id) {
            to_idx = i;
        }
        if (from_idx >= 0 && to_idx >= 0) {
            break;
        }
    }

    if (from_idx < 0) {
        lv00_set_error(LV00_ERROR_NOT_FOUND, "出发世界ID %d 未找到", from_world_id);
        return false;
    }
    if (to_idx < 0) {
        lv00_set_error(LV00_ERROR_NOT_FOUND, "目标世界ID %d 未找到", to_world_id);
        return false;
    }

    /* 确保矩阵维度足够 */
    int max_idx = (from_idx > to_idx) ? from_idx : to_idx;
    if (!modal_frame_ensure_reach_matrix(frame, max_idx + 1)) {
        return false;
    }

    /* 设置可达关系 */
    frame->reach_matrix[from_idx][to_idx] = reach_type;
    return true;
}

/**
 * @brief 检查世界 w_to 是否从 w_from 可达
 *
 * 查看可达关系矩阵，判断是否存在非恒等的可达关系。
 * LV00_REACH_GEOMETRIC_IDENTITY 表示恒等变换（自身到自身），
 * 其他类型表示存在有效的可达关系。
 *
 * @param frame       框架
 * @param from_world_id 出发世界ID
 * @param to_world_id   目标世界ID
 * @return true 可达，false 不可达
 */
bool lv00_modal_frame_is_reachable(const Lv00ModalFrame *frame, int from_world_id, int to_world_id)
{
    if (!frame) {
        return false;
    }

    /* 查找世界索引 */
    int from_idx = -1, to_idx = -1;
    for (int i = 0; i < frame->world_count; i++) {
        if (frame->worlds[i]->id == from_world_id) {
            from_idx = i;
        }
        if (frame->worlds[i]->id == to_world_id) {
            to_idx = i;
        }
        if (from_idx >= 0 && to_idx >= 0) {
            break;
        }
    }

    if (from_idx < 0 || to_idx < 0) {
        return false;
    }

    /* 检查矩阵维度 */
    if (from_idx >= frame->reach_dimension || to_idx >= frame->reach_dimension) {
        return false;
    }

    /* 检查可达关系是否存在（任何非零值都表示可达） */
    return frame->reach_matrix[from_idx][to_idx] != 0;
}

/**
 * @brief 获取从给定世界可达的所有世界ID列表
 *
 * 遍历可达关系矩阵中指定行，收集所有可达世界的ID。
 *
 * @param frame     框架
 * @param world_id  出发世界ID
 * @param out_ids   输出可达世界ID数组（调用者用 lv00_free 释放）
 * @param out_count 输出数量
 * @return true 成功，false 失败
 */
bool lv00_modal_frame_get_reachable_worlds(const Lv00ModalFrame *frame, int world_id,
                                           int **out_ids, int *out_count)
{
    if (!frame || !out_ids || !out_count) {
        lv00_set_error(LV00_ERROR_NULL_POINTER, "参数不能为 NULL");
        return false;
    }

    *out_ids = NULL;
    *out_count = 0;

    /* 查找出发世界索引 */
    int from_idx = -1;
    for (int i = 0; i < frame->world_count; i++) {
        if (frame->worlds[i]->id == world_id) {
            from_idx = i;
            break;
        }
    }

    if (from_idx < 0) {
        lv00_set_error(LV00_ERROR_NOT_FOUND, "世界ID %d 未找到", world_id);
        return false;
    }

    /* 第一遍：统计可达世界数量 */
    int count = 0;
    if (from_idx < frame->reach_dimension) {
        for (int j = 0; j < frame->world_count; j++) {
            if (j < frame->reach_dimension &&
                frame->reach_matrix[from_idx][j] != 0) {
                count++;
            }
        }
    }

    if (count == 0) {
        return true; /* 无可达世界，返回空数组 */
    }

    /* 分配输出数组 */
    int *ids = (int *)lv00_malloc((size_t)count * sizeof(int));
    if (!ids) {
        lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "无法分配可达世界ID数组");
        return false;
    }

    /* 第二遍：填充可达世界ID */
    int idx = 0;
    if (from_idx < frame->reach_dimension) {
        for (int j = 0; j < frame->world_count; j++) {
            if (j < frame->reach_dimension &&
                frame->reach_matrix[from_idx][j] != 0) {
                ids[idx++] = frame->worlds[j]->id;
            }
        }
    }

    *out_ids = ids;
    *out_count = count;
    return true;
}

/* ============== 模态公式管理 ============== */

/**
 * @brief 创建模态公式
 *
 * 创建一个包含模态算子和内层命题的公式。
 *
 * @param op         最外层模态算子
 * @param inner_prop 内层命题（所有权转移，可为 NULL）
 * @return 新分配的模态公式，失败返回 NULL
 */
Lv00ModalFormula *lv00_modal_formula_create(Lv00ModalOperator op, Proposition *inner_prop)
{
    Lv00ModalFormula *formula = (Lv00ModalFormula *)lv00_calloc(1, sizeof(Lv00ModalFormula));
    if (!formula) {
        lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "无法分配模态公式");
        return NULL;
    }

    formula->op = op;
    formula->inner_prop = inner_prop;
    formula->sub = NULL;

    return formula;
}

/**
 * @brief 创建嵌套模态公式
 *
 * 创建一个包含模态算子和子模态公式的嵌套公式。
 * 例如：□◇P 中，外层 op 为 NECESSARY，sub 为 ◇P。
 *
 * @param op   最外层模态算子
 * @param sub  子模态公式（所有权转移）
 * @return 新分配的模态公式，失败返回 NULL
 */
Lv00ModalFormula *lv00_modal_formula_create_nested(Lv00ModalOperator op, Lv00ModalFormula *sub)
{
    LV00_CHECK_NULL(sub, NULL);

    Lv00ModalFormula *formula = (Lv00ModalFormula *)lv00_calloc(1, sizeof(Lv00ModalFormula));
    if (!formula) {
        lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "无法分配嵌套模态公式");
        return NULL;
    }

    formula->op = op;
    formula->inner_prop = NULL;
    formula->sub = sub;

    return formula;
}

/**
 * @brief 销毁模态公式
 *
 * 递归销毁模态公式及其子公式。
 * 不释放内层命题（Proposition），由外部管理。
 *
 * @param formula 模态公式（可为 NULL，此时无操作）
 */
void lv00_modal_formula_destroy(Lv00ModalFormula *formula)
{
    if (!formula) {
        return;
    }

    /* 递归销毁子公式 */
    if (formula->sub) {
        lv00_modal_formula_destroy(formula->sub);
        formula->sub = NULL;
    }

    /* 不释放 inner_prop，所有权属于外部管理器 */
    formula->inner_prop = NULL;

    lv00_free((void **)&formula);
}

/* ============== 模态评估 ============== */

/**
 * @brief 递归评估模态公式的内部实现
 *
 * 处理两种情况：
 * 1. 如果公式有子公式（嵌套），递归评估子公式
 * 2. 如果公式有内层命题（原子），在世界中直接检查
 *
 * 对于 □ 算子：检查所有可达世界中子公式/命题是否为真
 * 对于 ◇ 算子：检查某个可达世界中子公式/命题是否为真
 *
 * @param frame    模态框架
 * @param formula  模态公式
 * @param world_id 当前世界ID
 * @param result   输出评估结果
 * @return 0 成功，-1 参数错误
 */
static int modal_evaluate_recursive(const Lv00ModalFrame *frame, const Lv00ModalFormula *formula,
                                    int world_id, Lv00ModalEvalResult *result)
{
    if (!frame || !formula || !result) {
        return -1;
    }

    result->truth_value = LV00_UNKNOWN;
    result->witness_world_id = -1;
    LV00_FREE_AND_NULL(result->explanation);

    /* 获取可达世界列表 */
    int *reachable_ids = NULL;
    int reachable_count = 0;

    if (!lv00_modal_frame_get_reachable_worlds(frame, world_id, &reachable_ids, &reachable_count)) {
        result->explanation = lv00_strdup("获取可达世界失败");
        return -1;
    }

    if (reachable_count == 0) {
        /* 无可达世界：
         * □P 在空可达集下为真（空真，vacuous truth）
         * ◇P 在空可达集下为假 */
        if (formula->op == LV00_MODALOP_NECESSARY) {
            result->truth_value = LV00_TRUE;
            result->explanation = lv00_strdup("空真：无可达世界，必然命题平凡成立");
        } else {
            result->truth_value = LV00_FALSE;
            result->explanation = lv00_strdup("无可达世界，可能命题不成立");
        }
        LV00_FREE_AND_NULL(reachable_ids);
        return 0;
    }

    if (formula->op == LV00_MODALOP_NECESSARY) {
        /*
         * □P 在世界 w 中为真 ⟺ P 在所有 w 可达的世界中为真
         *
         * 使用三值逻辑 AND 归约：
         * - 任何 FALSE 导致整体为 FALSE
         * - 全 TRUE 才为 TRUE
         * - 否则为 UNKNOWN
         */
        Lv00TruthValue overall = LV00_TRUE;

        for (int i = 0; i < reachable_count; i++) {
            Lv00TruthValue world_val;

            if (formula->sub) {
                /* 嵌套模态公式：递归评估子公式 */
                Lv00ModalEvalResult sub_result;
                memset(&sub_result, 0, sizeof(sub_result));
                int rc = modal_evaluate_recursive(frame, formula->sub, reachable_ids[i], &sub_result);
                if (rc != 0) {
                    overall = LV00_UNKNOWN;
                    lv00_modal_eval_result_destroy(&sub_result);
                    continue;
                }
                world_val = sub_result.truth_value;
                lv00_modal_eval_result_destroy(&sub_result);
            } else if (formula->inner_prop) {
                /* 原子命题：在世界中直接检查 */
                Lv00ModalWorld *w = modal_frame_find_world(frame, reachable_ids[i]);
                if (w) {
                    world_val = lv00_modal_world_holds(w, formula->inner_prop);
                } else {
                    world_val = LV00_UNKNOWN;
                }
            } else {
                /* 无内层内容，视为未知 */
                world_val = LV00_UNKNOWN;
            }

            /* AND 归约 */
            overall = lv00_tvl_and(overall, world_val);

            /* 短路：遇到 FALSE 立即停止 */
            if (overall == LV00_FALSE) {
                break;
            }
        }

        result->truth_value = overall;
        if (overall == LV00_TRUE) {
            result->explanation = lv00_asprintf(
                "□: 命题在所有 %d 个可达世界中成立", reachable_count);
        } else if (overall == LV00_FALSE) {
            result->explanation = lv00_strdup(
                "□: 命题在某个可达世界中不成立");
        } else {
            result->explanation = lv00_strdup(
                "□: 命题在部分可达世界中真值未知");
        }

    } else {
        /* LV00_MODALOP_POSSIBLE */
        /*
         * ◇P 在世界 w 中为真 ⟺ P 在某个 w 可达的世界中为真
         *
         * 使用三值逻辑 OR 归约：
         * - 任何 TRUE 导致整体为 TRUE
         * - 全 FALSE 才为 FALSE
         * - 否则为 UNKNOWN
         */
        Lv00TruthValue overall = LV00_FALSE;

        for (int i = 0; i < reachable_count; i++) {
            Lv00TruthValue world_val;

            if (formula->sub) {
                /* 嵌套模态公式：递归评估子公式 */
                Lv00ModalEvalResult sub_result;
                memset(&sub_result, 0, sizeof(sub_result));
                int rc = modal_evaluate_recursive(frame, formula->sub, reachable_ids[i], &sub_result);
                if (rc != 0) {
                    overall = lv00_tvl_or(overall, LV00_UNKNOWN);
                    lv00_modal_eval_result_destroy(&sub_result);
                    continue;
                }
                world_val = sub_result.truth_value;
                lv00_modal_eval_result_destroy(&sub_result);
            } else if (formula->inner_prop) {
                /* 原子命题：在世界中直接检查 */
                Lv00ModalWorld *w = modal_frame_find_world(frame, reachable_ids[i]);
                if (w) {
                    world_val = lv00_modal_world_holds(w, formula->inner_prop);
                } else {
                    world_val = LV00_UNKNOWN;
                }
            } else {
                world_val = LV00_UNKNOWN;
            }

            /* OR 归约 */
            overall = lv00_tvl_or(overall, world_val);

            /* 记录目击世界 */
            if (world_val == LV00_TRUE && result->witness_world_id < 0) {
                result->witness_world_id = reachable_ids[i];
            }

            /* 短路：遇到 TRUE 立即停止 */
            if (overall == LV00_TRUE) {
                break;
            }
        }

        result->truth_value = overall;
        if (overall == LV00_TRUE) {
            result->explanation = lv00_asprintf(
                "◇: 命题在可达世界 %d 中成立（目击世界）", result->witness_world_id);
        } else if (overall == LV00_FALSE) {
            result->explanation = lv00_strdup(
                "◇: 命题在所有可达世界中均不成立");
        } else {
            result->explanation = lv00_strdup(
                "◇: 命题在部分可达世界中真值未知");
        }
    }

    LV00_FREE_AND_NULL(reachable_ids);
    return 0;
}

/**
 * @brief 评估模态公式在给定框架和世界中的真值
 *
 * 基于 Kripke 语义评估模态公式：
 * - □P 在世界 w 中为真 ⟺ P 在所有 w 可达的世界中为真
 * - ◇P 在世界 w 中为真 ⟺ P 在某个 w 可达的世界中为真
 *
 * 支持嵌套模态公式的递归评估。
 * 使用三值逻辑（Lv00TruthValue）进行评估。
 *
 * @param frame    模态框架
 * @param formula  模态公式
 * @param world_id 当前世界ID
 * @param result   输出评估结果
 * @return 0 成功，-1 参数错误
 */
int lv00_modal_evaluate(const Lv00ModalFrame *frame, const Lv00ModalFormula *formula,
                        int world_id, Lv00ModalEvalResult *result)
{
    if (!frame || !formula || !result) {
        lv00_set_error(LV00_ERROR_NULL_POINTER, "模态评估参数不能为 NULL");
        return -1;
    }

    /* 验证世界ID存在 */
    Lv00ModalWorld *world = modal_frame_find_world(frame, world_id);
    if (!world) {
        lv00_set_error(LV00_ERROR_NOT_FOUND, "世界ID %d 在框架中未找到", world_id);
        return -1;
    }

    /* 初始化结果 */
    memset(result, 0, sizeof(Lv00ModalEvalResult));
    result->witness_world_id = -1;

    /* 委托给递归评估函数 */
    return modal_evaluate_recursive(frame, formula, world_id, result);
}

/**
 * @brief 检查公式是否为模态框架中的有效式
 *
 * 有效式：在框架的所有世界中评估都为 TRUE 的公式。
 * 遍历框架中的每个世界，对公式进行评估，
 * 如果所有世界都返回 TRUE，则公式有效。
 *
 * @param frame   模态框架
 * @param formula 模态公式
 * @return 真值
 */
Lv00TruthValue lv00_modal_check_validity(const Lv00ModalFrame *frame, const Lv00ModalFormula *formula)
{
    if (!frame || !formula) {
        return LV00_UNKNOWN;
    }

    if (frame->world_count == 0) {
        /* 空框架中任何公式都平凡有效 */
        return LV00_TRUE;
    }

    Lv00TruthValue overall = LV00_TRUE;

    for (int i = 0; i < frame->world_count; i++) {
        int world_id = frame->worlds[i]->id;
        Lv00ModalEvalResult eval_result;
        memset(&eval_result, 0, sizeof(eval_result));

        int rc = lv00_modal_evaluate(frame, formula, world_id, &eval_result);
        if (rc != 0) {
            overall = lv00_tvl_and(overall, LV00_UNKNOWN);
            lv00_modal_eval_result_destroy(&eval_result);
            continue;
        }

        overall = lv00_tvl_and(overall, eval_result.truth_value);
        lv00_modal_eval_result_destroy(&eval_result);

        /* 短路：遇到 FALSE 立即停止 */
        if (overall == LV00_FALSE) {
            break;
        }
    }

    return overall;
}

/* ============== 模态算子转换（对偶性） ============== */

/**
 * @brief 模态对偶转换：◇A → ¬□¬A
 *
 * 将可能算子转换为必然算子和否定的组合。
 *
 * 由于当前公式结构不直接支持否定节点，
 * 此函数创建一个 □ 形式的公式作为语义等价的表示。
 * 生成的公式不拥有内层命题的所有权（共享引用），
 * 调用者需确保原始公式和转换结果不会同时被销毁，
 * 或者在使用完毕后分别管理内层命题的生命周期。
 *
 * @param formula 原始模态公式（应为 ◇ 形式）
 * @return 转换后的模态公式（新分配，不拥有 inner_prop 所有权），失败返回 NULL
 */
Lv00ModalFormula *lv00_modal_possible_to_necessary_not(const Lv00ModalFormula *formula)
{
    LV00_CHECK_NULL(formula, NULL);

    /*
     * 对偶转换：◇A → ¬□¬A
     *
     * 由于公式结构不支持否定节点，我们创建 □ 形式的公式。
     * 实际的否定语义需要在评估层处理。
     * inner_prop 为共享引用（不转移所有权），调用者需注意生命周期管理。
     */
    Lv00ModalFormula *result = (Lv00ModalFormula *)lv00_calloc(1, sizeof(Lv00ModalFormula));
    if (!result) {
        lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "无法分配对偶转换结果");
        return NULL;
    }

    result->op = LV00_MODALOP_NECESSARY;

    /* 转移内层命题或子公式 */
    if (formula->sub) {
        /* 嵌套情况：递归转换子公式 */
        result->sub = lv00_modal_possible_to_necessary_not(formula->sub);
        if (!result->sub) {
            lv00_free((void **)&result);
            return NULL;
        }
        result->inner_prop = NULL;
    } else {
        /*
         * 原子命题：共享引用（不转移所有权）。
         * 警告：销毁 result 时不会释放 inner_prop。
         * 调用者需确保原始公式和转换结果不会同时被销毁导致悬垂指针。
         */
        result->inner_prop = formula->inner_prop;
        result->sub = NULL;
    }

    return result;
}

/**
 * @brief 模态对偶转换：□A → ¬◇¬A
 *
 * 将必然算子转换为可能算子和否定的组合。
 *
 * 由于当前公式结构不直接支持否定节点，
 * 此函数创建一个 ◇ 形式的公式作为语义等价的表示。
 * 生成的公式不拥有内层命题的所有权（共享引用），
 * 调用者需确保原始公式和转换结果不会同时被销毁，
 * 或者在使用完毕后分别管理内层命题的生命周期。
 *
 * @param formula 原始模态公式（应为 □ 形式）
 * @return 转换后的模态公式（新分配，不拥有 inner_prop 所有权），失败返回 NULL
 */
Lv00ModalFormula *lv00_modal_necessary_to_not_possible(const Lv00ModalFormula *formula)
{
    LV00_CHECK_NULL(formula, NULL);

    /*
     * 对偶转换：□A → ¬◇¬A
     *
     * 由于公式结构不支持否定节点，我们创建 ◇ 形式的公式。
     * 实际的否定语义需要在评估层处理。
     * inner_prop 为共享引用（不转移所有权），调用者需注意生命周期管理。
     */
    Lv00ModalFormula *result = (Lv00ModalFormula *)lv00_calloc(1, sizeof(Lv00ModalFormula));
    if (!result) {
        lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "无法分配对偶转换结果");
        return NULL;
    }

    result->op = LV00_MODALOP_POSSIBLE;

    /* 转移内层命题或子公式 */
    if (formula->sub) {
        /* 嵌套情况：递归转换子公式 */
        result->sub = lv00_modal_necessary_to_not_possible(formula->sub);
        if (!result->sub) {
            lv00_free((void **)&result);
            return NULL;
        }
        result->inner_prop = NULL;
    } else {
        /*
         * 原子命题：共享引用（不转移所有权）。
         * 警告：销毁 result 时不会释放 inner_prop。
         * 调用者需确保原始公式和转换结果不会同时被销毁导致悬垂指针。
         */
        result->inner_prop = formula->inner_prop;
        result->sub = NULL;
    }

    return result;
}

/* ============== 几何应用辅助 ============== */

/**
 * @brief 创建默认的几何模态框架
 *
 * 创建一个包含基本可达关系的框架：
 * - 世界 1："原始几何配置"（configuration 为 NULL 占位）
 * - 世界间默认可达关系为刚性变换（LV00_REACH_RIGID_TRANSFORM）
 *
 * 此函数创建一个最小化的框架，适用于基本的模态推理演示。
 *
 * @return 新分配的框架，失败返回 NULL
 */
Lv00ModalFrame *lv00_modal_frame_create_geometric_default(void)
{
    Lv00ModalFrame *frame = lv00_modal_frame_create();
    if (!frame) {
        return NULL;
    }

    /* 创建默认世界：原始几何配置 */
    Lv00ModalWorld *world = lv00_modal_world_create(1, "原始几何配置", NULL);
    if (!world) {
        lv00_modal_frame_destroy(frame);
        return NULL;
    }

    /* 添加世界到框架 */
    if (!lv00_modal_frame_add_world(frame, world)) {
        lv00_modal_world_destroy(world);
        lv00_modal_frame_destroy(frame);
        return NULL;
    }

    /* 设置自身到自身的恒等变换可达关系 */
    if (!lv00_modal_frame_set_reachability(frame, 1, 1, LV00_REACH_GEOMETRIC_IDENTITY)) {
        /* 恒等关系设置失败不影响框架创建 */
    }

    return frame;
}

/**
 * @brief 创建"点必须在线上"的模态断言
 *
 * 生成模态公式: □(onLine(point, line))
 * 表示在所有可达世界中，指定点都在指定线上。
 *
 * inner_prop 设置为 NULL（占位），实际命题由调用者在外部关联。
 * 公式仅作为模态结构的模板使用。
 *
 * @param frame    模态框架（用于验证，可为 NULL）
 * @param point_id 点节点ID
 * @param line_id  线节点ID
 * @return 新分配的模态公式，失败返回 NULL
 */
Lv00ModalFormula *lv00_modal_assert_point_must_on_line(Lv00ModalFrame *frame, int point_id, int line_id)
{
    (void)frame; /* 框架参数保留用于未来扩展 */

    /*
     * 创建 □(onLine(point, line)) 公式
     * inner_prop 为 NULL 占位，实际几何语义由外部命题系统管理
     */
    Lv00ModalFormula *formula = lv00_modal_formula_create(LV00_MODALOP_NECESSARY, NULL);
    if (!formula) {
        return NULL;
    }

    (void)point_id; /* 保留用于未来扩展：关联到具体命题 */
    (void)line_id;  /* 保留用于未来扩展：关联到具体命题 */

    return formula;
}

/**
 * @brief 创建"点可以在线上"的模态断言
 *
 * 生成模态公式: ◇(onLine(point, line))
 * 表示在某个可达世界中，指定点在指定线上。
 *
 * inner_prop 设置为 NULL（占位），实际命题由调用者在外部关联。
 * 公式仅作为模态结构的模板使用。
 *
 * @param frame    模态框架（用于验证，可为 NULL）
 * @param point_id 点节点ID
 * @param line_id  线节点ID
 * @return 新分配的模态公式，失败返回 NULL
 */
Lv00ModalFormula *lv00_modal_assert_point_can_on_line(Lv00ModalFrame *frame, int point_id, int line_id)
{
    (void)frame; /* 框架参数保留用于未来扩展 */

    /*
     * 创建 ◇(onLine(point, line)) 公式
     * inner_prop 为 NULL 占位，实际几何语义由外部命题系统管理
     */
    Lv00ModalFormula *formula = lv00_modal_formula_create(LV00_MODALOP_POSSIBLE, NULL);
    if (!formula) {
        return NULL;
    }

    (void)point_id; /* 保留用于未来扩展：关联到具体命题 */
    (void)line_id;  /* 保留用于未来扩展：关联到具体命题 */

    return formula;
}

/* ============== 释放评估结果 ============== */

/**
 * @brief 释放模态评估结果
 *
 * 释放评估结果中动态分配的解释字符串。
 *
 * @param result 评估结果指针
 */
void lv00_modal_eval_result_destroy(Lv00ModalEvalResult *result)
{
    if (!result) {
        return;
    }

    LV00_FREE_AND_NULL(result->explanation);
    result->witness_world_id = -1;
    result->truth_value = LV00_UNKNOWN;
}

/* ============== 辅助函数 ============== */

/**
 * @brief 模态算子转字符串
 *
 * @param op 模态算子
 * @return 静态字符串（"□" / "◇"），请勿释放
 */
const char *lv00_modal_op_to_string(Lv00ModalOperator op)
{
    switch (op) {
        case LV00_MODALOP_NECESSARY:
            return "\xe2\x96\xa1"; /* □ UTF-8 编码 */
        case LV00_MODALOP_POSSIBLE:
            return "\xe2\x97\x87"; /* ◇ UTF-8 编码 */
        default:
            return "?";
    }
}

/**
 * @brief 可达关系类型转字符串
 *
 * @param type 可达关系类型
 * @return 静态字符串，请勿释放
 */
const char *lv00_reachability_type_to_string(Lv00ReachabilityType type)
{
    switch (type) {
        case LV00_REACH_GEOMETRIC_IDENTITY:
            return "恒等变换";
        case LV00_REACH_RIGID_TRANSFORM:
            return "刚性变换";
        case LV00_REACH_SIMILARITY_TRANSFORM:
            return "相似变换";
        case LV00_REACH_AFFINE_TRANSFORM:
            return "仿射变换";
        case LV00_REACH_PROJECTIVE_TRANSFORM:
            return "射影变换";
        case LV00_REACH_CONSTRAINT_INHERIT:
            return "约束继承";
        case LV00_REACH_CUSTOM:
            return "自定义";
        default:
            return "未知";
    }
}

/**
 * @brief 模态公式转字符串
 *
 * 递归构建模态公式的字符串表示。
 * 格式示例：
 * - □P（带命题名称）
 * - ◇□P（嵌套）
 * - □(onLine)（带命题标签）
 *
 * @param formula 模态公式
 * @return 新分配的字符串（调用者需用 lv00_free 释放），失败返回 NULL
 */
char *lv00_modal_formula_to_string(const Lv00ModalFormula *formula)
{
    if (!formula) {
        return lv00_strdup("null");
    }

    /* 递归获取子公式字符串 */
    char *sub_str = NULL;
    if (formula->sub) {
        sub_str = lv00_modal_formula_to_string(formula->sub);
        if (!sub_str) {
            return NULL;
        }
    }

    /* 获取内层命题描述 */
    char prop_desc_buf[64];
    const char *prop_desc = "P"; /* 默认占位符 */
    if (formula->inner_prop) {
        if (formula->inner_prop->name) {
            prop_desc = formula->inner_prop->name;
        } else if (formula->inner_prop->label) {
            prop_desc = formula->inner_prop->label;
        } else {
            /* 使用命题 ID 作为描述（栈上缓冲区，线程安全） */
            snprintf(prop_desc_buf, sizeof(prop_desc_buf), "prop_%d", formula->inner_prop->id);
            prop_desc = prop_desc_buf;
        }
    }

    const char *op_str = lv00_modal_op_to_string(formula->op);

    /* 构建结果字符串 */
    char *result = NULL;

    if (sub_str) {
        /* 嵌套公式：□(sub_formula) */
        result = lv00_asprintf("%s(%s)", op_str, sub_str);
    } else {
        /* 原子命题：□prop_desc */
        result = lv00_asprintf("%s%s", op_str, prop_desc);
    }

    /* 释放子公式字符串 */
    LV00_FREE_AND_NULL(sub_str);

    return result;
}

/* ============== 内部辅助函数实现 ============== */

/**
 * @brief 根据世界ID查找世界指针
 *
 * 在框架的世界数组中线性搜索指定ID的世界。
 *
 * @param frame 模态框架
 * @param world_id 世界ID
 * @return 世界指针，未找到返回 NULL
 */
static Lv00ModalWorld *modal_frame_find_world(const Lv00ModalFrame *frame, int world_id)
{
    if (!frame) {
        return NULL;
    }

    for (int i = 0; i < frame->world_count; i++) {
        if (frame->worlds[i] && frame->worlds[i]->id == world_id) {
            return frame->worlds[i];
        }
    }

    return NULL;
}

/**
 * @brief 确保可达关系矩阵维度足够
 *
 * 当添加新世界时，需要扩展可达关系矩阵。
 * 新矩阵的行和列都会初始化为 0（表示不可达）。
 *
 * @param frame 模态框架
 * @param required_dim 所需的矩阵维度
 * @return true 成功，false 失败
 */
static bool modal_frame_ensure_reach_matrix(Lv00ModalFrame *frame, int required_dim)
{
    if (!frame) {
        return false;
    }

    if (required_dim <= frame->reach_dimension) {
        return true; /* 当前维度已足够 */
    }

    int old_dim = frame->reach_dimension;
    int new_dim = required_dim;

    /* 扩展行指针数组 */
    Lv00ReachabilityType **new_matrix = (Lv00ReachabilityType **)lv00_realloc(
        frame->reach_matrix, (size_t)new_dim * sizeof(Lv00ReachabilityType *));
    if (!new_matrix) {
        lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "无法扩展可达关系矩阵行");
        return false;
    }
    frame->reach_matrix = new_matrix;

    /* 初始化新增的行 */
    for (int i = old_dim; i < new_dim; i++) {
        frame->reach_matrix[i] = (Lv00ReachabilityType *)lv00_calloc(
            (size_t)new_dim, sizeof(Lv00ReachabilityType));
        if (!frame->reach_matrix[i]) {
            lv00_set_error(LV00_ERROR_OUT_OF_MEMORY,
                           "无法分配可达关系矩阵第 %d 行", i);
            return false;
        }
    }

    /* 扩展已有行的列数 */
    if (old_dim > 0) {
        for (int i = 0; i < old_dim; i++) {
            Lv00ReachabilityType *expanded_row = (Lv00ReachabilityType *)lv00_realloc(
                frame->reach_matrix[i], (size_t)new_dim * sizeof(Lv00ReachabilityType));
            if (!expanded_row) {
                lv00_set_error(LV00_ERROR_OUT_OF_MEMORY,
                               "无法扩展可达关系矩阵第 %d 行的列", i);
                return false;
            }
            frame->reach_matrix[i] = expanded_row;

            /* 将新增的列初始化为 0（不可达） */
            for (int j = old_dim; j < new_dim; j++) {
                frame->reach_matrix[i][j] = 0;
            }
        }
    }

    frame->reach_dimension = new_dim;
    return true;
}
