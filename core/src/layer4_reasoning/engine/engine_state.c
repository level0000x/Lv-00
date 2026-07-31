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
#include "lv_utils.h"

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

/* ================================================================
 * 枚举 -> 名称 映射表（数据表化，替代 switch）
 * ================================================================ */

/** @brief 枚举值 -> 名称 映射项（表必须按 code 升序排列） */
typedef struct {
    int code;         /**< 枚举值 */
    const char *name; /**< 名称字符串 */
} engine_state_NameEntry;

/** @brief 二分查找枚举名称（表需按 code 升序） */
static const char *engine_state_name_lookup(const engine_state_NameEntry *table, size_t count, int code) {
    size_t lo = 0, hi = count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (table[mid].code == code)
            return table[mid].name;
        if (table[mid].code < code)
            lo = mid + 1;
        else
            hi = mid;
    }
    return NULL;
}

/** @brief engine_state_name 名称表（按枚举值升序） */
static const engine_state_NameEntry s_engine_state_name_entries[] = {
    {ENGINE_STATE_IDLE, "空闲"},
    {ENGINE_STATE_PARSING, "解析中"},
    {ENGINE_STATE_REASONING, "推理中"},
    {ENGINE_STATE_ERROR, "错误"},
    {ENGINE_STATE_COMPLETE, "完成"},
};

const char *engine_state_name(EngineState state) {
    const char *name = engine_state_name_lookup(s_engine_state_name_entries, lv_ARRAY_SIZE(s_engine_state_name_entries), (int) state);
    return name ? name : "未知状态";
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
