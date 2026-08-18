/*
 * @file prop_verifier_trust.c
 * @brief Proposition verifier module - trust color mapping
 * @details Split from prop_verifier.c
 */

#include "lv/prop_verifier.h"
#include "prop_verifier_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/stream.h"
#include "lv/stream.h"

/* ============================================================
 * 信任颜色映射 —— BHK 验证映射到 约束图 TrustColor
 * ============================================================ */

/**
 * @brief 将 BHK 验证结果映射为 TrustColor
 *
 * 将验证结果映射为合适的信任颜色：
 *   - verified + 0 missing → TRUST_GREEN
 *   - verified + 1-2 missing → TRUST_YELLOW
 *   - verified + 3+ missing → TRUST_AMBER
 *   - 未验证（VERIFY_FAILED）→ TRUST_BLUE
 *   - 验证伪（VERIFY_DISPROVEN）→ TRUST_RED
 *   - 超时/错误 → TRUST_BLUE
 */
/* VerifyResult → 基准 TrustColor 静态查找表
 * VERIFY_PROVEN 因需依据 bhk 缺构数细分颜色，单独处理，不列入本表 */
static const TrustColor kVerifyBaseColorTable[] = {
    [VERIFY_DISPROVEN]     = TRUST_RED,              /* 验证伪 → 红 */
    [VERIFY_FAILED]        = TRUST_BLUE_UNEXPLORED,  /* 未验证 → 蓝-未探索 */
    [VERIFY_TIMEOUT]       = TRUST_BLUE_UNEXPLORED,  /* 超时 → 蓝-未探索 */
    [VERIFY_INVALID_INPUT] = TRUST_BLUE_UNEXPLORED,  /* 非法输入 → 蓝-未探索 */
};

/**
 * @brief 根据 BHK 验证结果与详细结果映射信任颜色
 */
static TrustColor map_bhk_to_trust_color(const BHKVerificationResult *bhk, VerifyResult verify_result) {
    if (bhk->verified) {
        /* 验证成功：按缺构数细分 */
        if (bhk->missing_constructions == 0)
            return TRUST_GREEN;
        if (bhk->missing_constructions <= 2)
            return TRUST_YELLOW;
        return TRUST_AMBER;
    }

    /* 其余结果查基准色表，越界保守回退为蓝-未探索 */
    if ((unsigned)verify_result < sizeof(kVerifyBaseColorTable) / sizeof(kVerifyBaseColorTable[0]))
        return kVerifyBaseColorTable[verify_result];
    return TRUST_BLUE_UNEXPLORED;
}

/**
 * @brief 获取 TrustColor 的中文名称描述
 */
/** @brief TrustColor→中文名称静态查找表 */
/* Chinese log names for stream events (not authoritative); authoritative English names: trust_color.c trust_color_name() */
static const char *s_trust_color_names[] = {
    [TRUST_GREEN]                = "绿色：完全可信",
    [TRUST_BLUE_UNEXPLORED]      = "蓝色-未探索",
    [TRUST_BLUE_EXCEEDED]        = "蓝色-资源受限",
    [TRUST_BLUE_OUT_OF_SCOPE]    = "蓝色-超出范围",
    [TRUST_YELLOW]               = "黄色：条件性可信",
    [TRUST_LIGHT_ORANGE_ORACLE]  = "浅橙色-oracle",
    [TRUST_LIGHT_ORANGE_EXPLOSION] = "浅橙色-爆炸",
    [TRUST_AMBER]                = "琥珀色：精度缺失",
    [TRUST_DEEP_ORANGE]          = "深橙色：叠加",
    [TRUST_RED]                  = "红色：矛盾/验证伪",
};

static const char *trust_color_name(TrustColor color) {
    if ((unsigned)color >= sizeof(s_trust_color_names) / sizeof(s_trust_color_names[0]))
        return "未知";
    return s_trust_color_names[color];
}

int prop_verifier_apply_trust_colors(ConstraintGraph *graph, const PropFormula **premises, int premise_count,
                                     const PropFormula *goal, const VerifierConfig *config,
                                     BHKVerificationResult *out_result) {
    lv_CHECK_NULL(graph, -1);

    /* 步骤1: 执行 BHK 验证 */
    BHKVerificationResult bhk = prop_verifier_bhk_verify(premises, premise_count, goal, config);

    /* 同时获取原始验证结果以判断 DISPROVEN 等状态 */
    VerifierConfig default_cfg = VERIFIER_CONFIG_DEFAULT;
    if (!config)
        config = &default_cfg;
    VerifyDetail detail = prop_verifier_verify(premises, premise_count, goal, config);

    /* 步骤2: 映射信任颜色 */
    TrustColor target_color = map_bhk_to_trust_color(&bhk, detail.result);

    /* 格式事件: 验证开始 */
    if (prop_verifier_stream_ctx) {
        lvStrBuf sb_4 = {0};
        lv_strbuf_printf(&sb_4, "信任颜色映射: BHK验证=%s, 缺失构造=%d, 目标颜色=%s", bhk.verified ? "通过" : "未通过",
                 bhk.missing_constructions, trust_color_name(target_color));
        stream_emit_simple(prop_verifier_stream_ctx, STREAM_EVENT_PROOF_COLOR_UPDATE, sb_4.data, 0);
        lv_strbuf_destroy(&sb_4);
    }

    /* 步骤3: 遍历约束图中的所有节点，应用目标颜色 */
    int updated_count = 0;
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node)
            continue;

        bool node_updated = false;

        /* 对节点的每个坐标应用目标颜色 */
        for (int c = 0; c < node->coord_count; c++) {
            SymbolicCoord *coord = node->symbolic_coords[c];
            if (!coord)
                continue;

            TrustColor old_color = symbolic_coord_get_trust(coord);
            if (old_color != target_color) {
                symbolic_coord_set_trust(coord, target_color);
                node_updated = true;
            }
        }

        if (node_updated) {
            updated_count++;

            /* 格式事件: 输出节点颜色更新 */
            if (prop_verifier_stream_ctx) {
                StreamEvent ev;
                memset(&ev, 0, sizeof(ev));
                ev.type = STREAM_EVENT_PROOF_COLOR_UPDATE;
                ev.timestamp_ms = stream_timestamp_ms();
                ev.node_id = node->id;
                ev.step_number = i;
                ev.total_steps = graph->node_count;
                ev.description = trust_color_name(target_color);
                lvStrBuf sb_5 = {0};
                lv_strbuf_printf(&sb_5,
                         "{\"node_id\":%d,\"type\":%d,\"old_color\":%d,\"new_color\":%d,"
                         "\"verified\":%s,\"missing\":%d}",
                         node->id, (int) node->type,
                         (int) symbolic_coord_get_trust(
                             node->coord_count > 0 && node->symbolic_coords[0] ? node->symbolic_coords[0] : NULL),
                         (int) target_color, bhk.verified ? "true" : "false", bhk.missing_constructions);
                ev.detail_json = sb_5.data;
                stream_emit(prop_verifier_stream_ctx, &ev);
                lv_strbuf_destroy(&sb_5);
            }
        }
    }

    /* 格式事件: 结果统计 */
    if (prop_verifier_stream_ctx) {
        lvStrBuf sb_6 = {0};
        lv_strbuf_printf(&sb_6, "信任颜色应用完成: 已更新 %d/%d 个节点", updated_count, graph->node_count);
        stream_emit_simple(prop_verifier_stream_ctx, STREAM_EVENT_PROOF_COLOR_UPDATE, sb_6.data, 0);
        lv_strbuf_destroy(&sb_6);
    }

    /* 步骤4: 输出 BHK 结果（若调用者需要） */
    if (out_result) {
        memcpy(out_result, &bhk, sizeof(BHKVerificationResult));
        /* 注意: missing_descriptions 所有权转移给调用者 */
        /* 故在此处不释放 bhk.missing_descriptions */
    } else {
        /* 调用者不需要结果：则由本函数释放 */
        prop_verifier_free_bhk_result(&bhk);
    }

    return updated_count;
}
