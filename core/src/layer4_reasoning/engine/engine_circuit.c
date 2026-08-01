/**
 * @file engine_circuit.c
 * @brief 引擎位电路跳闸处理（从 engine.c 拆分）
 *
 * @details 处理符号坐标运算触发的位电路跳闸：
 *          检查冻结点、溢出计数与降级阈值，
 *          支持忽略/回滚/永久降级三种用户动作。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/engine.h"

#include <math.h>
#include <stdio.h>

#include "lv/lv.h"
#include "lv/lv_config.h"
#include "lv/symbolic_coord.h"

/**
 * @brief 位电路跳闸处理器
 *
 * 当位电路跳闸时（来自 symbolic_coord 操作）：
 *   1. 检查是否存在冻结点
 *   2. 若 overflow_count >= 3，建议永久降级
 *   3. 否则，报告警告
 *
 * @param engine 引擎实例
 * @return ENGINE_CIRCUIT_IGNORE 已处理（忽略），
 *         ENGINE_CIRCUIT_ROLLBACK 需要回滚，ENGINE_CIRCUIT_DOWNGRADE 建议降级
 */
EngineCircuitResult engine_handle_circuit_trip(lvEngine *engine) {
    if (!engine) {
        return ENGINE_CIRCUIT_IGNORE;
    }

    /* 步骤1：检查是否存在冻结点 */
    if (circuit_has_frozen_point()) {
        /* 存在冻结点，调用方可能需要回滚到该点 */
        int overflow = circuit_get_overflow_count();

        /* 步骤2：若 overflow_count >= 阈值，建议永久降级 */
        int threshold = lv_config_get_int(LV_CFG_CIRCUIT_OVERFLOW_THRESHOLD, 3);
        if (overflow >= threshold) {
            snprintf(engine->last_error, sizeof(engine->last_error),
                     "engine_handle_circuit_trip: 溢出计数 %d >= %d，"
                     "建议永久降级为琥珀色",
                     overflow, threshold);
            engine->last_status = ENGINE_STATUS_CONSTRAINT_CONFLICT;
            return ENGINE_CIRCUIT_DOWNGRADE;
        }

        /* 存在冻结点但溢出计数可控，建议回滚 */
        snprintf(engine->last_error, sizeof(engine->last_error),
                 "engine_handle_circuit_trip: 存在冻结点，"
                 "溢出计数 %d，建议回滚",
                 overflow);
        engine->last_status = ENGINE_STATUS_OK;
        return ENGINE_CIRCUIT_ROLLBACK;
    }

    /* 步骤3：无冻结点，仅报告警告 */
    int overflow = circuit_get_overflow_count();
    int threshold = lv_config_get_int(LV_CFG_CIRCUIT_OVERFLOW_THRESHOLD, 3);
    if (overflow >= threshold) {
        /* 即使没有冻结点，反复溢出也建议降级 */
        snprintf(engine->last_error, sizeof(engine->last_error),
                 "engine_handle_circuit_trip: 溢出计数 %d >= %d，"
                 "建议永久降级为琥珀色",
                 overflow, threshold);
        engine->last_status = ENGINE_STATUS_CONSTRAINT_CONFLICT;
        return ENGINE_CIRCUIT_DOWNGRADE;
    }

    snprintf(engine->last_error, sizeof(engine->last_error), "engine_handle_circuit_trip: 溢出计数 %d，已处理（忽略）",
             overflow);
    engine->last_status = ENGINE_STATUS_OK;
    return ENGINE_CIRCUIT_IGNORE;
}

/**
 * @brief 使用显式用户动作处理电路跳闸
 *
 * 根据 design_v2.9.md Section 1.5：
 * - action 0 (忽略)：将坐标标记为琥珀色，继续执行
 * - action 1 (回滚)：恢复到冻结点快照
 * - action 2 (永久降级)：替换为高精度浮点数，标记琥珀色
 *
 * @param engine 引擎实例
 * @param action 用户选择的动作（0=忽略，1=回滚，2=降级）
 * @return 引擎电路结果码
 */
EngineCircuitResult engine_handle_circuit_trip_with_action(lvEngine *engine, EngineCircuitAction action) {
    if (!engine)
        return ENGINE_CIRCUIT_ERROR;

    SymbolicCoord *overflow_coord = circuit_get_last_result();

    switch (action) {
        case ENGINE_CIRCUIT_ACTION_IGNORE: /* 忽略：将坐标标记为琥珀色并继续 */
            if (overflow_coord) {
                symbolic_coord_set_trust(overflow_coord, TRUST_AMBER);
            }
            circuit_handle_overflow();
            engine->last_status = ENGINE_STATUS_OK;
            engine->last_error[0] = '\0';
            return ENGINE_CIRCUIT_IGNORE;

        case ENGINE_CIRCUIT_ACTION_ROLLBACK: /* 回滚：恢复到冻结点 */
            if (engine->frozen_point) {
                if (!engine_restore_frozen_point(engine, engine->frozen_point)) {
                    /* lv_LOG_WARNING("engine: 回滚到冻结点失败，引擎状态可能不一致"); */
                }
                /* engine_restore_frozen_point 消费了 frozen_point，已置 NULL */
            }
            circuit_reset_context();
            engine->last_status = ENGINE_STATUS_OK;
            snprintf(engine->last_error, sizeof(engine->last_error), "engine: 通过回滚到冻结点处理了电路跳闸");
            return ENGINE_CIRCUIT_ROLLBACK;

        case ENGINE_CIRCUIT_ACTION_DOWNGRADE: /* 永久降级：替换为高精度浮点数近似值 */
            if (overflow_coord) {
                /* 将溢出坐标永久降级为高精度有理数近似值。
                 * 策略：创建新的有理数坐标，然后将其内容原地替换到
                 * overflow_coord 中，使得所有引用该坐标的节点都能
                 * 看到降级后的值。 */
                double approx = symbolic_coord_to_double(overflow_coord);
                if (fabs(approx) > lv_VALUE_TOO_LARGE) {
                    /* 值过大，无法用 int64_t 分子精确表示，仅标记琥珀色 */
                    symbolic_coord_set_trust(overflow_coord, TRUST_AMBER);
                } else {
                    SymbolicCoord *new_coord = symbolic_coord_create_rational(
                        (int64_t) (approx * lv_DOWNGRADE_DENOMINATOR), lv_DOWNGRADE_DENOMINATOR);
                    if (new_coord) {
                        symbolic_coord_set_trust(new_coord, TRUST_AMBER);
                        /* 原地替换：先释放 overflow_coord 的内部数据（但不释放
                         * overflow_coord 结构体本身，因为外部仍持有其指针），
                         * 再将 new_coord 的数据转移过来，最后仅释放 new_coord 外壳。
                         * 注意：不能调用 symbolic_coord_destroy(overflow_coord)，
                         * 因为它会 lv_free((void **) &coord) 整个结构体，导致后续写入为 use-after-free。 */
                        switch (overflow_coord->type) {
                            case RATIONAL:
                                rational_destroy(overflow_coord->data.rational);
                                break;
                            case ALGEBRAIC:
                                algebraic_destroy(overflow_coord->data.algebraic);
                                break;
                            case QUADRATIC:
                                quadratic_destroy(overflow_coord->data.quadratic);
                                break;
                            case TRANSCENDENTAL:
                                transcendental_destroy(overflow_coord->data.transcendental);
                                break;
                            default:
                                break;
                        }
                        overflow_coord->type = new_coord->type;
                        overflow_coord->data = new_coord->data;
                        overflow_coord->trust = new_coord->trust;
                        /* 仅释放 new_coord 的外壳，不释放其内部数据（已转移至 overflow_coord） */
                        lv_free((void **) &new_coord);
                    } else {
                        /* new_coord 创建失败，仅标记琥珀色作为降级 */
                        symbolic_coord_set_trust(overflow_coord, TRUST_AMBER);
                    }
                }
            }
            circuit_handle_overflow();
            engine->last_status = ENGINE_STATUS_OK;
            snprintf(engine->last_error, sizeof(engine->last_error), "engine: 通过永久降级为琥珀色处理了电路跳闸");
            return ENGINE_CIRCUIT_DOWNGRADE;

        default:
            engine->last_status = ENGINE_STATUS_INVALID_STATE;
            snprintf(engine->last_error, sizeof(engine->last_error),
                     "engine_handle_circuit_trip_with_action: 无效的动作 %d", action);
            return ENGINE_CIRCUIT_ERROR;
    }
}
