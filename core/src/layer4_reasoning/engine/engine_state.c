/**
 * @file engine_state.c
 * @brief 引擎五状态机实现（状态转移表 + 状态查询）
 *
 * @details 从 engine.c 中提取，减少主引擎文件的耦合度。
 *          实现引擎的五状态机（IDLE / PARSING / REASONING / ERROR / COMPLETE），
 *          包括转移表、合法性检查、状态名称映射。
 *
 * @author Lv-00 Project
 */

#include "lv/engine.h"

#include <stdio.h>

#include "lv_utils.h"
#include "lv/lv_numeric.h"
#include "lv/lv_xmacro.h"

#include "engine_internal.h"

/**
 * @brief 引擎状态转移表
 *
 * 5x5 布尔矩阵，行 = 当前状态，列 = 目标状态。
 * true 表示状态转移合法。
 *
 * 合法转移：
 *   IDLE     → PARSING, ERROR
 *   PARSING  → IDLE, REASONING, ERROR
 *   REASONING→ IDLE, ERROR, COMPLETE
 *   ERROR    → IDLE
 *   COMPLETE → IDLE
 *
 * 所有其他组合均非法（如 IDLE → COMPLETE，COMPLETE → REASONING 等）。
 */
/* exempt: 1-A 状态机合流终审一 —— 与 context.c 的转移矩阵同构，
 * 但（a）返回错误码体系本质不同（EngineStatus vs lvErrorCode 两个独立枚举），
 * （b）self-transition 幂等 no-op（current==new_state 计数+1 返回 OK；context 无此分支），
 * （c）目标状态越界有独立错误消息"无效的目标状态"（context 并入"非法转移"），
 * （d）NULL 入参返回 ENGINE_STATUS_INVALID_ARGUMENT（context 返回 lv_ERROR_NULL_POINTER），
 * （e）无熔断强转路径（context 有 ctx_force_to_error）。
 * 合成共享骨架需体内 if(mode) 分支且返回类型不可统一 → 伪同构，保留各自纯函数。 */
static const bool engine_transition_table[5][5] = {
    /* from \ to →        IDLE  PARSING  REASONING  ERROR  COMPLETE */
    /* IDLE      */ {false, true, false, true, false},
    /* PARSING   */ {true, false, true, true, false},
    /* REASONING */ {true, false, false, true, true},
    /* ERROR     */ {true, false, false, false, false},
    /* COMPLETE  */ {true, false, false, false, false},
};

bool engine_is_valid_transition(EngineState from, EngineState to) {
    /* 边界检查：防止无效状态索引（收敛为 lv_index_in_range 双检查，覆盖负值与上界） */
    int state_count = (int) (sizeof(engine_transition_table) / sizeof(engine_transition_table[0]));
    if (!lv_index_in_range(from, state_count) || !lv_index_in_range(to, state_count)) {
        return false;
    }
    return engine_transition_table[from][to];
}

/* ================================================================
 * 枚举 -> 名称 映射表（X-macro 键表生成，替代手写枚举条目）
 * ================================================================ */

/** @brief EngineState 状态名键表（项序 = 枚举升序，供 lv_enum_to_str 二分） */
#define ENGINE_STATE_NAME_X(x)                                                                                        \
    x(ENGINE_STATE_IDLE, "空闲")                                                                                      \
    x(ENGINE_STATE_PARSING, "解析中")                                                                                 \
    x(ENGINE_STATE_REASONING, "推理中")                                                                               \
    x(ENGINE_STATE_ERROR, "错误")                                                                                     \
    x(ENGINE_STATE_COMPLETE, "完成")

/** @brief engine_state_name 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_engine_state_name_entries[] = {
    lv_XMACRO_TO_ENUM_TABLE(ENGINE_STATE_NAME_X)
};

const char *engine_state_name(EngineState state) {
    return lv_enum_to_str(s_engine_state_name_entries, lv_ARRAY_SIZE(s_engine_state_name_entries), (int) state, "未知状态");
}

EngineState engine_get_state(const lvEngine *engine) {
    if (!engine) {
        return ENGINE_STATE_IDLE;
    }
    return engine->state;
}

bool engine_is_busy(const lvEngine *engine) {
    if (!engine)
        return false;
    return engine->state == ENGINE_STATE_REASONING;
}

/**
 * @brief 尝试将引擎转移到指定状态
 *
 * 这是引擎状态机的核心 API。在执行任何可能改变引擎上下文的操作前，
 * 调用此函数来验证状态的合法性。
 *
 * 转移成功时：
 * - 记录 previous_state（用于审计和调试）
 * - 更新 state
 * - 递增 state_transition_count
 * - 返回 ENGINE_STATUS_OK
 *
 * 转移非法时：
 * - 不改变任何状态字段
 * - 设置 last_status = ENGINE_STATUS_INVALID_STATE
 * - 设置 last_error 描述尝试的非法转移
 * - 返回 ENGINE_STATUS_INVALID_STATE
 *
 * @param engine    引擎实例（非 NULL）
 * @param new_state 目标状态
 * @return ENGINE_STATUS_OK 成功，ENGINE_STATUS_INVALID_STATE 非法转移
 */
EngineStatus lv_engine_transition_state(lvEngine *engine, EngineState new_state) {
    /* 参数校验 */
    if (!engine) {
        return ENGINE_STATUS_INVALID_ARGUMENT;
    }

    EngineState current = engine->state;

    /* 边界检查：防止无效状态枚举值 */
    if (new_state < ENGINE_STATE_IDLE || new_state > ENGINE_STATE_COMPLETE) {
        engine_set_error(engine, ENGINE_STATUS_INVALID_STATE, "状态转移失败: 无效的目标状态 %d（合法范围: %d-%d）",
                         (int) new_state, ENGINE_STATE_IDLE, ENGINE_STATE_COMPLETE);
        return ENGINE_STATUS_INVALID_STATE;
    }

    /* 转移到相同状态 —— 允许（幂等），只记录日志不触发错误 */
    if (current == new_state) {
        /* 相同状态转移：这是一个 no-op，但计数仍然递增用于审计 */
        engine->state_transition_count++;
        return ENGINE_STATUS_OK;
    }

    /* 查转移表验证合法性 */
    if (!engine_is_valid_transition(current, new_state)) {
        engine_set_error(engine, ENGINE_STATUS_INVALID_STATE,
                         "状态转移非法: 不能从 \"%s\" 转移到 \"%s\"（转移次数: %d）", engine_state_name(current),
                         engine_state_name(new_state), engine->state_transition_count);
        return ENGINE_STATUS_INVALID_STATE;
    }

    /* 合法转移：记录并执行 */
    engine->previous_state = current;
    engine->state = new_state;
    engine->state_transition_count++;

    return ENGINE_STATUS_OK;
}
