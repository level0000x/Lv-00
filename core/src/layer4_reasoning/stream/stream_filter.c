/**
 * @file stream_filter.c
 * @brief 流式输出系统 —— 过滤掩码解析
 */

#include "stream_internal.h"


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
        if (eid && strcmp(eid, id_str) == 0) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief 解析类别名为事件类型掩码
 *
 * 支持以下类别名（不区分大小写）：
 *   - "engine":    引擎生命周期事件（ENGINE_START/DONE/PAUSED）
 *   - "normalize": 归一化事件（NORMALIZE_START/MERGE/DONE）
 *   - "rewrite":   重写事件（REWRITE_START/RULE_LOADED/MATCH_FOUND/APPLIED/ROLLBACK/DONE）
 *   - "solve":     求解事件（SOLVE_START/EQUATION_EXTRACTED/GROEBNER_STEP/VARIABLE_RESOLVED/DONE）
 *   - "proof":     证明事件（PROOF_STEP_ADDED/APPLIED/UNIFY/COLOR_UPDATE/DEPENDENCY_CHANGE）
 *   - "conflict":  冲突事件（CONFLICT_DETECTED）
 *   - "info":      信息事件（INFO/PROGRESS/GRAPH_SNAPSHOT）
 *
 * @param category 类别名
 * @return 对应的事件类型位掩码，未识别时返回 STREAM_FILTER_NONE
 */
static uint64_t stream_parse_category(const char *category) {
    if (!category)
        return STREAM_FILTER_NONE;

    /* 不区分大小写比较 */
    if (strcasecmp(category, "engine") == 0) {
        return STREAM_EVENT_MASK(STREAM_EVENT_ENGINE_START) | STREAM_EVENT_MASK(STREAM_EVENT_ENGINE_DONE) |
               STREAM_EVENT_MASK(STREAM_EVENT_ENGINE_PAUSED);
    }
    if (strcasecmp(category, "normalize") == 0) {
        return STREAM_EVENT_MASK(STREAM_EVENT_NORMALIZE_START) | STREAM_EVENT_MASK(STREAM_EVENT_NORMALIZE_MERGE) |
               STREAM_EVENT_MASK(STREAM_EVENT_NORMALIZE_DONE);
    }
    if (strcasecmp(category, "rewrite") == 0) {
        return STREAM_EVENT_MASK(STREAM_EVENT_REWRITE_START) | STREAM_EVENT_MASK(STREAM_EVENT_REWRITE_RULE_LOADED) |
               STREAM_EVENT_MASK(STREAM_EVENT_REWRITE_MATCH_FOUND) | STREAM_EVENT_MASK(STREAM_EVENT_REWRITE_APPLIED) |
               STREAM_EVENT_MASK(STREAM_EVENT_REWRITE_ROLLBACK) | STREAM_EVENT_MASK(STREAM_EVENT_REWRITE_DONE);
    }
    if (strcasecmp(category, "solve") == 0) {
        return STREAM_EVENT_MASK(STREAM_EVENT_SOLVE_START) | STREAM_EVENT_MASK(STREAM_EVENT_SOLVE_EQUATION_EXTRACTED) |
               STREAM_EVENT_MASK(STREAM_EVENT_SOLVE_GROEBNER_STEP) |
               STREAM_EVENT_MASK(STREAM_EVENT_SOLVE_VARIABLE_RESOLVED) | STREAM_EVENT_MASK(STREAM_EVENT_SOLVE_DONE);
    }
    if (strcasecmp(category, "proof") == 0) {
        return STREAM_EVENT_MASK(STREAM_EVENT_PROOF_STEP_ADDED) | STREAM_EVENT_MASK(STREAM_EVENT_PROOF_STEP_APPLIED) |
               STREAM_EVENT_MASK(STREAM_EVENT_PROOF_UNIFY) | STREAM_EVENT_MASK(STREAM_EVENT_PROOF_COLOR_UPDATE) |
               STREAM_EVENT_MASK(STREAM_EVENT_PROOF_DEPENDENCY_CHANGE);
    }
    if (strcasecmp(category, "func_block") == 0) {
        return STREAM_EVENT_MASK(STREAM_EVENT_FUNC_BLOCK_PACK_START) |
               STREAM_EVENT_MASK(STREAM_EVENT_FUNC_BLOCK_PACK_DONE) |
               STREAM_EVENT_MASK(STREAM_EVENT_FUNC_BLOCK_INSTANTIATE_START) |
               STREAM_EVENT_MASK(STREAM_EVENT_FUNC_BLOCK_INSTANTIATE_DONE) |
               STREAM_EVENT_MASK(STREAM_EVENT_FUNC_BLOCK_PARTIAL_APPLY) |
               STREAM_EVENT_MASK(STREAM_EVENT_FUNC_BLOCK_DETERMINISM_CHECK) |
               STREAM_EVENT_MASK(STREAM_EVENT_FUNC_BLOCK_CAPTURE_AVOID) |
               STREAM_EVENT_MASK(STREAM_EVENT_FUNC_BLOCK_CROSS_BOUNDARY);
    }
    if (strcasecmp(category, "conflict") == 0) {
        return STREAM_EVENT_MASK(STREAM_EVENT_CONFLICT_DETECTED);
    }
    if (strcasecmp(category, "info") == 0) {
        return STREAM_EVENT_MASK(STREAM_EVENT_INFO) | STREAM_EVENT_MASK(STREAM_EVENT_PROGRESS) |
               STREAM_EVENT_MASK(STREAM_EVENT_GRAPH_SNAPSHOT);
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
    while (*str == ' ' || *str == '\t' || *str == '\r' || *str == '\n')
        str++;
    if (*str == '\0')
        return STREAM_FILTER_NONE;

    /* 检查特殊值 "all" 或 "*" */
    if (strcmp(str, "all") == 0 || strcmp(str, "*") == 0) {
        return STREAM_FILTER_ALL;
    }
    /* 检查特殊值 "none" */
    if (strcmp(str, "none") == 0) {
        return STREAM_FILTER_NONE;
    }

    uint64_t mask = STREAM_FILTER_NONE;

    /* 复制字符串用于分词（避免修改原始字符串） */
    size_t len = strlen(str);
    char *buf = (char *) lv_malloc(len + 1);
    if (!buf)
        return STREAM_FILTER_NONE;
    lv_strlcpy(buf, str, len + 1);

    /* 按逗号分词 */
    char *saveptr = NULL;
    char *token = lv_strtok_r(buf, ",", &saveptr);

    while (token) {
        /* 去除 token 首尾空白 */
        while (*token == ' ' || *token == '\t')
            token++;
        if (*token == '\0') {
            token = lv_strtok_r(NULL, ",", &saveptr);
            continue; /* 空 token，跳过 */
        }
        size_t tok_len = strlen(token);
        if (tok_len == 0) {
            token = lv_strtok_r(NULL, ",", &saveptr);
            continue;
        }
        char *end = token + tok_len - 1;
        while (end > token && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
            *end = '\0';
            end--;
        }

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

        token = lv_strtok_r(NULL, ",", &saveptr);
    }

    lv_free((void **) &buf);
    return mask;
}
