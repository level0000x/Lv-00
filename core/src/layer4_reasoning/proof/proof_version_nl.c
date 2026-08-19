/**
 * @file proof_version_nl.c
 * @brief 证明版本管理与序列化 —— 自然语言导出与策略注释
 *
 * @details 由 proof_version.c 按功能域拆分而来。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/proof.h"
#include "lv/smt_backend.h"
#include "lv/thread_pool.h"

#include "lv/debug.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/lv_str_utils.h"

#include "lv/lv_strbuf.h"

static char *format_proof_step_nl(ProofStep *step, ProofNaturalLanguage lang) {
    if (!step)
        return NULL;

    /* 用 lvStrBuf 累积输出（自动扩容，消除 4096 固定缓冲截断风险；
       lv_strbuf_to_string 返回 lv_malloc 分配的 NUL 结尾字符串） */
    lvStrBuf sb = {0};

    /* 步骤编号 */
    if (lang == PROOF_NL_LANG_ZH_CN) {
        lv_strbuf_printf(&sb, "步骤 %d", step->id);
    } else {
        lv_strbuf_printf(&sb, "Step %d", step->id);
    }

    /* 附加用户注释 */
    if (step->note && step->note[0] != '\0') {
        if (lang == PROOF_NL_LANG_ZH_CN) {
            lv_strbuf_printf(&sb, "  —— 注释：%s", step->note);
        } else {
            lv_strbuf_printf(&sb, "  -- Note: %s", step->note);
        }
    }

    /* 附加依赖信息 */
    if (step->dependency_count > 0) {
        if (lang == PROOF_NL_LANG_ZH_CN) {
            lv_strbuf_printf(&sb, "\n  —— 依赖步骤：");
        } else {
            lv_strbuf_printf(&sb, "\n  -- Depends on: ");
        }
        for (int d = 0; d < step->dependency_count && d < 8; d++) {
            if (d > 0) {
                lv_strbuf_printf(&sb, ", ");
            }
            lv_strbuf_printf(&sb, "Step %d", step->dependency_step_ids[d]);
        }
    }

    return lv_strbuf_to_string(&sb);
}

/**
 * @brief 导出完整证明为自然语言文本
 */
bool proof_export_natural_language(ProofNavigator *nav, const char *filepath, ProofNaturalLanguage lang) {
    if (!nav || !filepath)
        return false;

    FILE *f = fopen(filepath, "w");
    if (!f)
        return false;

    bool is_zh = (lang == PROOF_NL_LANG_ZH_CN);

    /* ===== 标题 ===== */
    if (is_zh) {
        fprintf(f, "========================================\n");
        fprintf(f, "  Lv-00 证明导出（自然语言格式）\n");
        fprintf(f, "========================================\n\n");
    } else {
        fprintf(f, "========================================\n");
        fprintf(f, "  Lv-00 Proof Export (Natural Language)\n");
        fprintf(f, "========================================\n\n");
    }

    /* ===== 总体策略（LeanGeo风格：先展示总体策略） ===== */
    const char *strategy = proof_navigator_get_strategy_note(nav);
    if (strategy && strategy[0] != '\0') {
        if (is_zh) {
            fprintf(f, "【证明策略】\n");
            fprintf(f, "%s\n\n", strategy);
            fprintf(f, "【证明步骤】\n");
        } else {
            fprintf(f, "[Proof Strategy]\n");
            fprintf(f, "%s\n\n", strategy);
            fprintf(f, "[Proof Steps]\n");
        }
    } else {
        if (is_zh) {
            fprintf(f, "【证明步骤】\n");
        } else {
            fprintf(f, "[Proof Steps]\n");
        }
    }
    fprintf(f, "----------------------------------------\n\n");

    /* ===== 逐步骤输出 ===== */
    for (int i = 0; i < nav->step_count; i++) {
        ProofStep *step = nav->steps[i];
        if (!step)
            continue;

        char *nl_desc = proof_step_get_natural_language(step, lang);
        if (nl_desc) {
            fprintf(f, "%s\n\n", nl_desc);
            lv_free((void **) &nl_desc);
        }
    }

    /* ===== 总结 ===== */
    fprintf(f, "----------------------------------------\n");
    if (is_zh) {
        fprintf(f, "\n【证明总结】\n");
        fprintf(f, "总步骤数：%d\n", nav->step_count);
        fprintf(f, "最终颜色：%s\n", proof_color_to_string(nav->final_color));
        fprintf(f, "证明状态：%s\n", nav->is_complete ? "已完成" : "进行中");
    } else {
        fprintf(f, "\n[Proof Summary]\n");
        fprintf(f, "Total steps: %d\n", nav->step_count);
        fprintf(f, "Final color: %s\n", proof_color_to_string(nav->final_color));
        fprintf(f, "Status: %s\n", nav->is_complete ? "Complete" : "In progress");
    }

    fclose(f);
    return true;
}

/* ============== 证明策略注释（LeanGeo风格） ============== */

/**
 * @brief 设置证明的总体策略描述
 */
bool proof_navigator_set_strategy_note(ProofNavigator *nav, const char *strategy_note) {
    if (!nav)
        return false;

    /* 释放旧值 */
    lv_free((void **) &nav->strategy_note);

    if (strategy_note && strategy_note[0] != '\0') {
        nav->strategy_note = lv_strdup_safe(strategy_note);
        if (!nav->strategy_note)
            return false;
    } else {
        nav->strategy_note = NULL;
    }

    if (proof_stream_ctx) {
        stream_emit_simple(proof_stream_ctx, STREAM_EVENT_INFO, strategy_note ? "策略注释已设置" : "策略注释已清除", 0);
    }

    return true;
}

/**
 * @brief 获取证明的总体策略描述
 */
const char *proof_navigator_get_strategy_note(const ProofNavigator *nav) {
    if (!nav)
        return NULL;
    return nav->strategy_note;
}

/**
 * @brief 为证明步骤设置自然语言注释
 */
bool proof_step_set_note(ProofStep *step, const char *note) {
    if (!step)
        return false;

    /* 释放旧值 */
    lv_free((void **) &step->note);

    if (note && note[0] != '\0') {
        step->note = lv_strdup_safe(note);
        if (!step->note)
            return false;
    } else {
        step->note = NULL;
    }

    return true;
}
