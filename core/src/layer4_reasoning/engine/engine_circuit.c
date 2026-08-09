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
#include "lv/lv_internal.h" /* lv_LOG_WARNING */
#include "lv/symbolic_coord.h"

#include "engine_internal.h"

/** @brief 建议永久降级统一消息（有无冻结点两条分支共用，防改一漏一） */
static const char kMsgSuggestDowngrade[] = "engine_handle_circuit_trip: 溢出计数 %d >= %d，建议永久降级为琥珀色";

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
            engine_set_error(engine, ENGINE_STATUS_CONSTRAINT_CONFLICT, kMsgSuggestDowngrade, overflow, threshold);
            return ENGINE_CIRCUIT_DOWNGRADE;
        }

        /* 存在冻结点但溢出计数可控，建议回滚（成功说明文字：ENGINE_STATUS_OK 语义下不落错误日志） */
        engine_set_error(engine, ENGINE_STATUS_OK, "engine_handle_circuit_trip: 存在冻结点，溢出计数 %d，建议回滚",
                         overflow);
        return ENGINE_CIRCUIT_ROLLBACK;
    }

    /* 步骤3：无冻结点，仅报告警告 */
    int overflow = circuit_get_overflow_count();
    int threshold = lv_config_get_int(LV_CFG_CIRCUIT_OVERFLOW_THRESHOLD, 3);
    if (overflow >= threshold) {
        /* 即使没有冻结点，反复溢出也建议降级 */
        engine_set_error(engine, ENGINE_STATUS_CONSTRAINT_CONFLICT, kMsgSuggestDowngrade, overflow, threshold);
        return ENGINE_CIRCUIT_DOWNGRADE;
    }

    engine_set_error(engine, ENGINE_STATUS_OK, "engine_handle_circuit_trip: 溢出计数 %d，已处理（忽略）", overflow);
    return ENGINE_CIRCUIT_IGNORE;
}

/* ================================================================
 * 查找表：CoordType → 内部数据销毁函数
 * ================================================================ */

/** @brief CoordType 内部数据销毁函数类型 */
typedef void (*CoordTypeDestroyFunc)(SymbolicCoord *coord);

/** @brief 销毁有理数内部数据 */
static void coord_type_destroy_rational(SymbolicCoord *coord) { rational_destroy(coord->data.rational); }
/** @brief 销毁代数数内部数据 */
static void coord_type_destroy_algebraic(SymbolicCoord *coord) { algebraic_destroy(coord->data.algebraic); }
/** @brief 销毁二次域内部数据 */
static void coord_type_destroy_quadratic(SymbolicCoord *coord) { quadratic_destroy(coord->data.quadratic); }
/** @brief 销毁超越数内部数据 */
static void coord_type_destroy_transcendental(SymbolicCoord *coord) { transcendental_destroy(coord->data.transcendental); }

/**
 * @brief CoordType 内部数据销毁函数查找表（按枚举值升序）
 *
 * 索引：RATIONAL=0, ALGEBRAIC=1, QUADRATIC=2, TRANSCENDENTAL=3
 */
static const CoordTypeDestroyFunc s_coord_type_destroy_handlers[] = {
    coord_type_destroy_rational,       /* RATIONAL */
    coord_type_destroy_algebraic,      /* ALGEBRAIC */
    coord_type_destroy_quadratic,      /* QUADRATIC */
    coord_type_destroy_transcendental, /* TRANSCENDENTAL */
};

/**
 * @brief 通过查找表销毁 SymbolicCoord 的内部数据
 */
static void coord_type_destroy(SymbolicCoord *coord) {
    if (!coord)
        return;
    if ((unsigned) coord->type < lv_ARRAY_SIZE(s_coord_type_destroy_handlers)) {
        s_coord_type_destroy_handlers[coord->type](coord);
    }
}

/* ================================================================
 * 查找表：EngineCircuitAction → 处理函数
 * ================================================================ */

/** @brief 电路跳闸动作处理函数类型 */
typedef EngineCircuitResult (*CircuitActionHandler)(lvEngine *engine, SymbolicCoord *overflow_coord);

/** @brief 处理 IGNORE 动作：将坐标标记为琥珀色并继续 */
static EngineCircuitResult handle_action_ignore(lvEngine *engine, SymbolicCoord *overflow_coord) {
    if (overflow_coord) {
        symbolic_coord_set_trust(overflow_coord, TRUST_AMBER);
    }
    circuit_handle_overflow();
    engine_set_error(engine, ENGINE_STATUS_OK, "");
    return ENGINE_CIRCUIT_IGNORE;
}

/** @brief 处理 ROLLBACK 动作：恢复到冻结点 */
static EngineCircuitResult handle_action_rollback(lvEngine *engine, SymbolicCoord *overflow_coord) {
    (void) overflow_coord;
    if (engine->frozen_point) {
        if (!engine_restore_frozen_point(engine, engine->frozen_point)) {
            lv_LOG_WARNING("engine: 回滚到冻结点失败，引擎状态可能不一致");
        }
        /* 注意：engine_restore_frozen_point 成功后会自动重新打点
         * （engine->frozen_point = 新快照），保证下一次跳闸仍可回滚。 */
    } else {
        /* engine->frozen_point 为 NULL：从未打点或上次打点失败，
         * 无可用回滚目标 —— 显式告警，避免静默跳过导致状态不一致 */
        lv_LOG_WARNING("engine: 电路跳闸但无可用冻结点，跳过回滚，引擎状态可能不一致");
    }
    circuit_reset_context();
    /* 成功说明文字：写入 ROLLBACK 已处理说明（ENGINE_STATUS_OK 语义下不落错误日志） */
    engine_set_error(engine, ENGINE_STATUS_OK, "engine: 通过回滚到冻结点处理了电路跳闸");
    return ENGINE_CIRCUIT_ROLLBACK;
}

/** @brief 处理 DOWNGRADE 动作：替换为高精度浮点数近似值 */
static EngineCircuitResult handle_action_downgrade(lvEngine *engine, SymbolicCoord *overflow_coord) {
    if (overflow_coord) {
        double approx = symbolic_coord_to_double(overflow_coord);
        if (fabs(approx) > lv_VALUE_TOO_LARGE) {
            symbolic_coord_set_trust(overflow_coord, TRUST_AMBER);
        } else {
            SymbolicCoord *new_coord = symbolic_coord_create_rational(
                (int64_t) (approx * lv_DOWNGRADE_DENOMINATOR), lv_DOWNGRADE_DENOMINATOR);
            if (new_coord) {
                symbolic_coord_set_trust(new_coord, TRUST_AMBER);
                coord_type_destroy(overflow_coord);
                overflow_coord->type = new_coord->type;
                overflow_coord->data = new_coord->data;
                overflow_coord->trust = new_coord->trust;
                lv_free((void **) &new_coord);
            } else {
                symbolic_coord_set_trust(overflow_coord, TRUST_AMBER);
            }
        }
    }
    circuit_handle_overflow();
    /* 成功说明文字：写入 DOWNGRADE 已处理说明（ENGINE_STATUS_OK 语义下不落错误日志） */
    engine_set_error(engine, ENGINE_STATUS_OK, "engine: 通过永久降级为琥珀色处理了电路跳闸");
    return ENGINE_CIRCUIT_DOWNGRADE;
}

/**
 * @brief EngineCircuitAction 处理函数查找表（按枚举值升序）
 *
 * 索引：ENGINE_CIRCUIT_ACTION_IGNORE=0,
 *       ENGINE_CIRCUIT_ACTION_ROLLBACK=1,
 *       ENGINE_CIRCUIT_ACTION_DOWNGRADE=2
 */
static const CircuitActionHandler s_circuit_action_handlers[] = {
    handle_action_ignore,   /* ENGINE_CIRCUIT_ACTION_IGNORE */
    handle_action_rollback, /* ENGINE_CIRCUIT_ACTION_ROLLBACK */
    handle_action_downgrade /* ENGINE_CIRCUIT_ACTION_DOWNGRADE */
};
#define lv_CIRCUIT_ACTION_HANDLER_COUNT lv_ARRAY_SIZE(s_circuit_action_handlers)

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

    if ((unsigned) action < lv_CIRCUIT_ACTION_HANDLER_COUNT) {
        return s_circuit_action_handlers[action](engine, overflow_coord);
    }

    engine_set_error(engine, ENGINE_STATUS_INVALID_STATE, "engine_handle_circuit_trip_with_action: 无效的动作 %d",
                     action);
    return ENGINE_CIRCUIT_ERROR;
}
