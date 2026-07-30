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
static const bool engine_transition_table[5][5] = {
    /* from \ to →        IDLE  PARSING  REASONING  ERROR  COMPLETE */
    /* IDLE      */ {false, true, false, true, false},
    /* PARSING   */ {true, false, true, true, false},
    /* REASONING */ {true, false, false, true, true},
    /* ERROR     */ {true, false, false, false, false},
    /* COMPLETE  */ {true, false, false, false, false},
};

bool engine_is_valid_transition(EngineState from, EngineState to) {
    /* 边界检查：防止无效状态索引 */
    if (from > ENGINE_STATE_COMPLETE || to > ENGINE_STATE_COMPLETE) {
        return false;
    }
    return engine_transition_table[from][to];
}

const char *engine_state_name(EngineState state) {
    switch (state) {
        case ENGINE_STATE_IDLE:
            return "空闲"; /* IDLE: 等待输入 */
        case ENGINE_STATE_PARSING:
            return "解析中"; /* PARSING: 解析输入文本 */
        case ENGINE_STATE_REASONING:
            return "推理中"; /* REASONING: 执行重写/求解/证明 */
        case ENGINE_STATE_ERROR:
            return "错误"; /* ERROR: 发生不可恢复错误 */
        case ENGINE_STATE_COMPLETE:
            return "完成"; /* COMPLETE: 推理成功完成 */
        default:
            return "未知状态";
    }
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
