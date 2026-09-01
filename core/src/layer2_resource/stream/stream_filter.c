/**
 * @file stream_filter.c
 * @brief 流式输出系统 —— 过滤掩码解析
 */

#include "stream_internal.h"
#include "lv/lv_str_utils.h"


/* ==================== 过滤掩码解析 ==================== */

/**
 * @brief 从英文标识符查找事件类型枚举值
 *
 * @param id_str 事件类型英文标识符（如 "ENGINE_START"）
 * @return 事件类型枚举值，未找到时返回 -1
 */
static int stream_find_event_type_by_id(const char *id_str) {
    if (!id_str)
        return -1;

    /* 遍历所有事件类型，通过 stream_event_type_id 反向查找 */
    for (int i = 0; i < STREAM_EVENT_TYPE_COUNT; i++) {
        const char *eid = stream_event_type_id((StreamEventType) i);
        if (eid && lv_str_eq(eid, id_str)) {
            return i;
        }
    }
    return -1;
}

/** @brief 类别名→掩码映射表条目 */
typedef struct {
    const char *name;
    uint64_t mask;
} CategoryMaskEntry;

/** @brief 类别名→掩码映射表（按名称长度降序排列，避免短名误匹配长名） */
static const CategoryMaskEntry s_category_masks[] = {
    {"func_block",
        STREAM_EVENT_MASK(STREAM_EVENT_FUNC_BLOCK_PACK_START) |
        STREAM_EVENT_MASK(STREAM_EVENT_FUNC_BLOCK_PACK_DONE) |
        STREAM_EVENT_MASK(STREAM_EVENT_FUNC_BLOCK_INSTANTIATE_START) |
        STREAM_EVENT_MASK(STREAM_EVENT_FUNC_BLOCK_INSTANTIATE_DONE) |
        STREAM_EVENT_MASK(STREAM_EVENT_FUNC_BLOCK_PARTIAL_APPLY) |
        STREAM_EVENT_MASK(STREAM_EVENT_FUNC_BLOCK_DETERMINISM_CHECK) |
        STREAM_EVENT_MASK(STREAM_EVENT_FUNC_BLOCK_CAPTURE_AVOID) |
        STREAM_EVENT_MASK(STREAM_EVENT_FUNC_BLOCK_CROSS_BOUNDARY)},
    {"normalize",
        STREAM_EVENT_MASK(STREAM_EVENT_NORMALIZE_START) |
        STREAM_EVENT_MASK(STREAM_EVENT_NORMALIZE_MERGE) |
        STREAM_EVENT_MASK(STREAM_EVENT_NORMALIZE_DONE)},
    {"rewrite",
        STREAM_EVENT_MASK(STREAM_EVENT_REWRITE_START) |
        STREAM_EVENT_MASK(STREAM_EVENT_REWRITE_RULE_LOADED) |
        STREAM_EVENT_MASK(STREAM_EVENT_REWRITE_MATCH_FOUND) |
        STREAM_EVENT_MASK(STREAM_EVENT_REWRITE_APPLIED) |
        STREAM_EVENT_MASK(STREAM_EVENT_REWRITE_ROLLBACK) |
        STREAM_EVENT_MASK(STREAM_EVENT_REWRITE_DONE)},
    {"conflict",
        STREAM_EVENT_MASK(STREAM_EVENT_CONFLICT_DETECTED)},
    {"engine",
        STREAM_EVENT_MASK(STREAM_EVENT_ENGINE_START) |
        STREAM_EVENT_MASK(STREAM_EVENT_ENGINE_DONE) |
        STREAM_EVENT_MASK(STREAM_EVENT_ENGINE_PAUSED)},
    {"solve",
        STREAM_EVENT_MASK(STREAM_EVENT_SOLVE_START) |
        STREAM_EVENT_MASK(STREAM_EVENT_SOLVE_EQUATION_EXTRACTED) |
        STREAM_EVENT_MASK(STREAM_EVENT_SOLVE_GROEBNER_STEP) |
        STREAM_EVENT_MASK(STREAM_EVENT_SOLVE_VARIABLE_RESOLVED) |
        STREAM_EVENT_MASK(STREAM_EVENT_SOLVE_DONE)},
    {"proof",
        STREAM_EVENT_MASK(STREAM_EVENT_PROOF_STEP_ADDED) |
        STREAM_EVENT_MASK(STREAM_EVENT_PROOF_STEP_APPLIED) |
        STREAM_EVENT_MASK(STREAM_EVENT_PROOF_UNIFY) |
        STREAM_EVENT_MASK(STREAM_EVENT_PROOF_COLOR_UPDATE) |
        STREAM_EVENT_MASK(STREAM_EVENT_PROOF_DEPENDENCY_CHANGE)},
    {"info",
        STREAM_EVENT_MASK(STREAM_EVENT_INFO) |
        STREAM_EVENT_MASK(STREAM_EVENT_PROGRESS) |
        STREAM_EVENT_MASK(STREAM_EVENT_GRAPH_SNAPSHOT)},
};

/**
 * @brief 解析类别名为事件类型掩码
 *
 * 支持以下类别名（不区分大小写）：
 *   - "engine":    引擎生命周期事件
 *   - "normalize": 归一化事件
 *   - "rewrite":   重写事件
 *   - "solve":     求解事件
 *   - "proof":     证明事件
 *   - "func_block": 函数块事件
 *   - "conflict":  冲突事件
 *   - "info":      信息事件
 *
 * @param category 类别名
 * @return 对应的事件类型位掩码，未识别时返回 STREAM_FILTER_NONE
 */
static uint64_t stream_parse_category(const char *category) {
    if (!category)
        return STREAM_FILTER_NONE;

    for (size_t i = 0; i < lv_ARRAY_SIZE(s_category_masks); i++) {
        if (lv_str_icmp(category, s_category_masks[i].name) == 0)
            return s_category_masks[i].mask;
    }
    return STREAM_FILTER_NONE;
}

/**
 * 从字符串解析事件类型掩码。
 *
 * 支持以下格式：
 *   - "all" / "*" → STREAM_FILTER_ALL（接收所有事件）
 *   - "none" → STREAM_FILTER_NONE（不接收任何事件）
 *   - "ENGINE_START" → 单个事件类型
 *   - "ENGINE_START,ENGINE_DONE" → 多个事件类型用逗号分隔
 *   - "engine" → 按类别匹配（引擎生命周期事件）
 *   - "engine,rewrite" → 多个类别用逗号分隔
 *   - "ENGINE_START,engine" → 混合使用事件 ID 和类别名
 *
 * @param str 输入字符串
 * @return 解析后的位掩码，解析失败返回 STREAM_FILTER_NONE
 */
uint64_t stream_parse_filter_mask(const char *str) {
    if (!str)
        return STREAM_FILTER_NONE;

    /* 去除首尾空白 */
    str = lv_str_ltrim((char *) str); /* lv_str_ltrim 不修改原串，仅返回首非空白指针 */
    if (*str == '\0')
        return STREAM_FILTER_NONE;

    /* 检查特殊值 "all" 或 "*" */
    if (lv_str_eq(str, "all") || lv_str_eq(str, "*")) {
        return STREAM_FILTER_ALL;
    }
    /* 检查特殊值 "none" */
    if (lv_str_eq(str, "none")) {
        return STREAM_FILTER_NONE;
    }

    uint64_t mask = STREAM_FILTER_NONE;

    /* K71/D6：手写 strtok_r 逗号分词收敛到权威 lv_str_split（堆分配 items，
     * 不修改原 str，无需 buf 复制）；中间空段由下方空 token 检查兜底
     * （strtok 跳过空段 vs lv_str_split 保留，此处显式跳过语义一致） */
    lvStrSplitResult parts = lv_str_split(str, ",");
    for (size_t i = 0; i < parts.count; i++) {
        /* 去除 token 首尾空白 */
        char *token = lv_str_ltrim(parts.items[i]);
        if (*token == '\0') {
            continue; /* 空 token，跳过 */
        }
        lv_str_rtrim(token);

        if (*token != '\0') {
            /* 先尝试按类别名解析 */
            uint64_t cat_mask = stream_parse_category(token);
            if (cat_mask != STREAM_FILTER_NONE) {
                mask |= cat_mask;
            } else {
                /* 再尝试按事件 ID 解析 */
                int type_idx = stream_find_event_type_by_id(token);
                if (type_idx >= 0 && type_idx < STREAM_EVENT_TYPE_COUNT) {
                    mask |= STREAM_EVENT_MASK((StreamEventType) type_idx);
                }
                /* 无法识别的 token 静默忽略 */
            }
        }
    }

    lv_str_split_free(&parts);
    return mask;
}
