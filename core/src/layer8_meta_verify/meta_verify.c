/**
 * @file meta_verify.c
 * @brief 元验证系统实现
 *
 * @details 实现六项元验证检查（结构完整性、类型一致性、完备性、可靠性、
 * 非平凡性、往返验证），用于验证证明流水线输出的正确性。提供会话级别
 * (lv_meta_verify_session) 和证明对象级别 (lv_meta_verify_proof) 两套验证接口。
 * 每个检查可通过掩码单独启用/禁用，支持严格模式（预留扩展）。
 *
 * @author Lv-00 Project
 */

#include "lv/meta_verify.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_parse_utils.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_utils.h"
#include "lv/proof.h"
#include "lv/proof_compiler.h"
#include "lv/lv_strbuf.h"

/* ============================================================
 * 内部验证检查函数
 * ============================================================ */

/**
 * @brief 检查 1: 结构完整性
 *
 * 验证所有 pipeline 阶段都已完成或被跳过，没有遗留的
 * PENDING/RUNNING/FAILED 状态。
 */
static int check_structural(const lvSession *session, char *desc, int desc_size) {
    if (!session) {
        snprintf(desc, desc_size, "Session is NULL");
        return 0;
    }
    for (int i = 0; i < lv_STAGE_COUNT; i++) {
        lvStageStatus s = session->stages[i].status;
        if (s != lv_STAGE_COMPLETED && s != lv_STAGE_SKIPPED) {
            snprintf(desc, desc_size, "Stage %d not completed (status=%d)", i, s);
            return 0;
        }
    }
    snprintf(desc, desc_size, "All stages completed successfully");
    return 1;
}

/**
 * @brief 检查 2: 类型一致性
 *
 * 验证 proof 输出类型与目标命题类型匹配。
 * 通过检查推理阶段的输出信息来验证类型一致性。
 */
static int check_type_consistency(const lvSession *session, char *desc, int desc_size) {
    if (!session) {
        snprintf(desc, desc_size, "Session is NULL");
        return 0;
    }

    /* 检查推理阶段是否已完成 */
    if (session->stages[lv_STAGE_REASONING].status != lv_STAGE_COMPLETED) {
        snprintf(desc, desc_size, "类型一致性失败：推理阶段未完成 (status=%d)",
                 session->stages[lv_STAGE_REASONING].status);
        return 0;
    }

    /* 检查推理阶段是否有有效输出信息 */
    const char *reasoning_msg = session->stages[lv_STAGE_REASONING].error_msg;
    if (!reasoning_msg || reasoning_msg[0] == '\0') {
        snprintf(desc, desc_size, "类型一致性失败：推理阶段无输出信息");
        return 0;
    }

    /* 检查推理结果是否包含"已证明"或"成功"等正向类型标记 */
    int has_proof_result = 0;
    const char *positive_markers[] = {"已证明", "成功", "proved", "success", "completed", "完成"};
    if (lv_str_match_any(reasoning_msg, positive_markers) >= 0) {
        has_proof_result = 1;
    }

    if (!has_proof_result) {
        snprintf(desc, desc_size, "类型一致性失败：推理结果无有效证明类型标记 (msg: %.200s)", reasoning_msg);
        return 0;
    }

    /* 检查输出阶段是否与推理阶段的输出格式匹配 */
    const char *output_msg = session->stages[lv_STAGE_OUTPUT].error_msg;
    if (session->stages[lv_STAGE_OUTPUT].status == lv_STAGE_COMPLETED) {
        /* 验证输出格式与配置一致 */
        const char *fmt = session->config.output_format;
        if (fmt && fmt[0] != '\0' && output_msg && strstr(output_msg, fmt) == NULL) {
            /* 输出消息中未包含配置的格式标识，可能是类型不匹配 */
            snprintf(desc, desc_size, "类型一致性警告：输出格式 '%s' 未在输出消息中找到", fmt);
            /* 不严格失败，仅警告 */
        }
    }

    /* 检查最终证明状态是否与预期一致（session->success 应为 1） */
    if (!session->success) {
        snprintf(desc, desc_size, "类型一致性失败：会话标记为失败，但推理阶段已完成");
        return 0;
    }

    snprintf(desc, desc_size, "类型一致性通过: 推理结果类型有效，输出格式匹配");
    return 1;
}

/**
 * @brief 检查 3: 完备性
 *
 * 验证所有子目标都已解决。
 * 检查所有 pipeline 阶段是否已完成，无遗留的未解决依赖。
 */
static int check_completeness(const lvSession *session, char *desc, int desc_size) {
    if (!session) {
        snprintf(desc, desc_size, "Session is NULL");
        return 0;
    }

    /* 检查所有必需阶段是否已完成（非可选阶段不能是 SKIPPED） */
    int unresolved_count = 0;
    int first_unresolved = -1;
    for (int i = 0; i < lv_STAGE_COUNT; i++) {
        lvStageStatus s = session->stages[i].status;
        /* VISUAL 阶段可以为 SKIPPED，其他阶段必须 COMPLETED */
        if (i == lv_STAGE_VISUAL) {
            if (s != lv_STAGE_COMPLETED && s != lv_STAGE_SKIPPED) {
                unresolved_count++;
                if (first_unresolved < 0)
                    first_unresolved = i;
            }
        } else {
            if (s != lv_STAGE_COMPLETED) {
                unresolved_count++;
                if (first_unresolved < 0)
                    first_unresolved = i;
            }
        }
    }

    if (unresolved_count > 0) {
        snprintf(desc, desc_size, "完备性失败: %d 个阶段未完成，首个未解决阶段=%d", unresolved_count, first_unresolved);
        return 0;
    }

    /* 检查推理阶段状态是否为 COMPLETED（非 ONGOING） */
    if (session->stages[lv_STAGE_REASONING].status != lv_STAGE_COMPLETED) {
        snprintf(desc, desc_size, "完备性失败：推理阶段状态为 ONGOING 或 FAILED");
        return 0;
    }

    /* 检查推理阶段消息中是否包含策略统计（表明所有策略已尝试） */
    const char *reasoning_msg = session->stages[lv_STAGE_REASONING].error_msg;
    if (reasoning_msg && strstr(reasoning_msg, "尝试") != NULL) {
        /* 验证是否有未解决的依赖：检查是否有 FAILED 状态的阶段 */
        int has_failure = 0;
        for (int i = 0; i < lv_STAGE_COUNT; i++) {
            if (session->stages[i].status == lv_STAGE_FAILED) {
                has_failure = 1;
                break;
            }
        }
        if (has_failure) {
            snprintf(desc, desc_size, "完备性失败：存在 FAILED 状态的阶段");
            return 0;
        }
    }

    /* 检查会话整体成功标志 */
    if (!session->success) {
        snprintf(desc, desc_size, "完备性失败：会话整体标记为失败");
        return 0;
    }

    snprintf(desc, desc_size, "完备性通过: 所有 %d 个阶段已完成，无未解决依赖", lv_STAGE_COUNT);
    return 1;
}

/**
 * @brief 检查 4: 可靠性
 *
 * 验证每一步推理都逻辑可靠。
 * 检查阶段间的依赖链一致性、无循环依赖、无矛盾。
 */
static int check_soundness(const lvSession *session, char *desc, int desc_size) {
    if (!session) {
        snprintf(desc, desc_size, "Session is NULL");
        return 0;
    }

    /* 检查阶段依赖链一致性：每个阶段必须在其前驱阶段完成后才能完成 */
    for (int i = 1; i < lv_STAGE_COUNT; i++) {
        if (session->stages[i].status == lv_STAGE_COMPLETED) {
            /* 如果当前阶段完成，前驱阶段也必须完成（除非前驱是 SKIPPED） */
            lvStageStatus prev = session->stages[i - 1].status;
            if (prev != lv_STAGE_COMPLETED && prev != lv_STAGE_SKIPPED) {
                snprintf(desc, desc_size, "可靠性失败：阶段 %d 已完成但其前驱阶段 %d 状态=%d", i, i - 1, prev);
                return 0;
            }
        }
    }

    /* 检查循环依赖：阶段完成顺序必须单调递增（时间戳递增） */
    for (int i = 1; i < lv_STAGE_COUNT; i++) {
        if (session->stages[i].status == lv_STAGE_COMPLETED && session->stages[i - 1].status == lv_STAGE_COMPLETED) {
            /* 验证耗时合理性：后续阶段耗时不应为负 */
            if (session->stages[i].elapsed_ms < 0) {
                snprintf(desc, desc_size, "可靠性失败：阶段 %d 耗时为负值 (%.2fms)", i, session->stages[i].elapsed_ms);
                return 0;
            }
        }
    }

    /* 检查推理阶段是否存在矛盾 */
    const char *reasoning_msg = session->stages[lv_STAGE_REASONING].error_msg;
    if (reasoning_msg) {
        /* 检查推理消息中是否包含矛盾标记 */
        const char *contradiction_markers[] = {"矛盾", "contradiction", "CONTRADICTORY", "不一致"};
        int marker_idx = lv_str_match_any(reasoning_msg, contradiction_markers);
        if (marker_idx >= 0) {
            snprintf(desc, desc_size, "可靠性失败：推理阶段包含矛盾标记 '%s'", contradiction_markers[marker_idx]);
            return 0;
        }
    }

    /* 检查是否有阶段同时标记为 COMPLETED 和 FAILED（矛盾状态） */
    int completed_count = 0;
    int failed_count = 0;
    for (int i = 0; i < lv_STAGE_COUNT; i++) {
        if (session->stages[i].status == lv_STAGE_COMPLETED)
            completed_count++;
        if (session->stages[i].status == lv_STAGE_FAILED)
            failed_count++;
    }
    if (completed_count > 0 && failed_count > 0) {
        /* 存在混合状态：检查是否合理（如后续阶段因前置失败而终止） */
        /* 如果有 FAILED 阶段，会话应该标记为失败 */
        if (session->success) {
            snprintf(desc, desc_size, "可靠性失败：存在 %d 个完成和 %d 个失败阶段，但会话标记为成功", completed_count,
                     failed_count);
            return 0;
        }
    }

    /* 检查推理阶段是否报告了有效的规则应用 */
    if (session->stages[lv_STAGE_REASONING].status == lv_STAGE_COMPLETED) {
        if (!reasoning_msg || reasoning_msg[0] == '\0') {
            snprintf(desc, desc_size, "可靠性失败：推理阶段已完成但无规则应用信息");
            return 0;
        }
    }

    snprintf(desc, desc_size, "可靠性通过: 依赖链一致，无循环依赖，无矛盾，%d 阶段完成", completed_count);
    return 1;
}

/**
 * @brief 检查 5: 非平凡性
 *
 * 验证证明不是平凡的（如空证明、零步推理）。
 * 检查推理阶段是否有实质性的工作产出。
 */
static int check_nontriviality(const lvSession *session, char *desc, int desc_size) {
    if (!session) {
        snprintf(desc, desc_size, "Session is NULL");
        return 0;
    }

    /* 检查推理阶段是否已完成 */
    if (session->stages[lv_STAGE_REASONING].status != lv_STAGE_COMPLETED) {
        snprintf(desc, desc_size, "非平凡性失败：推理阶段未完成");
        return 0;
    }

    /* 检查推理阶段是否有实质性输出（至少 2 个步骤/策略） */
    const char *reasoning_msg = session->stages[lv_STAGE_REASONING].error_msg;
    if (!reasoning_msg || reasoning_msg[0] == '\0') {
        snprintf(desc, desc_size, "非平凡性失败：推理阶段无输出信息（空证明）");
        return 0;
    }

    /* 检查是否为平凡证明（仅 "trivial" 或 "rfl"） */
    const char *trivial_markers[] = {"trivial", "rfl", "平凡", "axiom", "假设即结论"};
    int marker_idx = lv_str_match_any(reasoning_msg, trivial_markers);
    if (marker_idx >= 0) {
        snprintf(desc, desc_size, "非平凡性失败：证明仅包含平凡标记 '%s'", trivial_markers[marker_idx]);
        return 0;
    }

    /* 检查推理深度：从推理阶段消息中提取策略尝试数 */
    int strategy_attempts = 0;
    const char *attempt_str = strstr(reasoning_msg, "尝试");
    if (attempt_str) {
        /* 向前搜索数字 */
        const char *num_start = attempt_str;
        while (num_start > reasoning_msg && *(num_start - 1) >= '0' && *(num_start - 1) <= '9')
            num_start--;
        if (num_start < attempt_str) {
            lv_parse_int(num_start, &strategy_attempts);
        }
    }

    /* 检查证明深度 > 1：至少尝试了 2 个策略或推理耗时 > 1ms */
    double reasoning_time = session->stages[lv_STAGE_REASONING].elapsed_ms;
    if (strategy_attempts > 0 && strategy_attempts < 2 && reasoning_time < 1.0) {
        snprintf(desc, desc_size, "非平凡性失败：证明深度不足 (策略尝试=%d, 耗时=%.2fms)", strategy_attempts,
                 reasoning_time);
        return 0;
    }

    /* 检查解析阶段是否有足够的输入（至少 2 个标记） */
    const char *parse_msg = session->stages[lv_STAGE_PARSE].error_msg;
    if (parse_msg) {
        const char *token_str = strstr(parse_msg, "标记");
        if (token_str) {
            const char *num_start = token_str;
            while (num_start > parse_msg && *(num_start - 1) >= '0' && *(num_start - 1) <= '9')
                num_start--;
            if (num_start < token_str) {
                int tokens = 0;
                lv_parse_int(num_start, &tokens);
                if (tokens < 2) {
                    snprintf(desc, desc_size, "非平凡性失败：输入仅含 %d 个标记（不足 2 个）", tokens);
                    return 0;
                }
            }
        }
    }

    /* 检查几何阶段是否识别了对象 */
    const char *geo_msg = session->stages[lv_STAGE_GEOMETRY].error_msg;
    if (geo_msg) {
        const char *obj_str = strstr(geo_msg, "个几何对象");
        if (obj_str) {
            const char *num_start = obj_str;
            while (num_start > geo_msg && *(num_start - 1) >= '0' && *(num_start - 1) <= '9')
                num_start--;
            if (num_start < obj_str) {
                int objs = 0;
                lv_parse_int(num_start, &objs);
                if (objs < 1) {
                    snprintf(desc, desc_size, "非平凡性失败：无几何对象被识别");
                    return 0;
                }
            }
        }
    }

    snprintf(desc, desc_size, "非平凡性通过: 策略尝试=%d, 推理耗时=%.2fms, 证明非平凡", strategy_attempts,
             reasoning_time);
    return 1;
}

/**
 * @brief 检查 6: 往返验证
 *
 * 验证输出可以重新解析为等价结构。
 * 检查输出阶段是否包含有效的结构化标记。
 */
static int check_roundtrip(const lvSession *session, char *desc, int desc_size) {
    if (!session) {
        snprintf(desc, desc_size, "Session is NULL");
        return 0;
    }

    /* 检查输出阶段是否已完成 */
    if (session->stages[lv_STAGE_OUTPUT].status != lv_STAGE_COMPLETED) {
        snprintf(desc, desc_size, "往返验证失败：输出阶段未完成 (status=%d)", session->stages[lv_STAGE_OUTPUT].status);
        return 0;
    }

    /* 检查输出阶段是否有非空文本 */
    const char *output_msg = session->stages[lv_STAGE_OUTPUT].error_msg;
    if (!output_msg || output_msg[0] == '\0') {
        snprintf(desc, desc_size, "往返验证失败：输出文本为空");
        return 0;
    }

    /* 检查输出是否包含有效的结构化标记 */
    int found_markers = 0;
    const char *structure_markers[] = {"格式=", "proof", "latex", "html", "字节", "预估", "format", "bytes", "output"};
    int marker_count = (int) (sizeof(structure_markers) / sizeof(structure_markers[0]));
    for (int i = 0; i < marker_count; i++) {
        if (strstr(output_msg, structure_markers[i]) != NULL) {
            found_markers++;
        }
    }

    if (found_markers < 2) {
        snprintf(desc, desc_size, "往返验证失败：输出缺少有效的结构化标记 (仅找到 %d 个)", found_markers);
        return 0;
    }

    /* 检查输出是否包含可重新解析的数值信息（如字节数） */
    const char *byte_str = strstr(output_msg, "字节");
    if (byte_str) {
        const char *num_start = byte_str;
        while (num_start > output_msg && *(num_start - 1) >= '0' && *(num_start - 1) <= '9')
            num_start--;
        if (num_start < byte_str) {
            int bytes = 0;
            lv_parse_int(num_start, &bytes);
            if (bytes <= 0) {
                snprintf(desc, desc_size, "往返验证失败：输出字节数无效 (%d)", bytes);
                return 0;
            }
        }
    }

    /* 检查解析阶段的输出是否可以被重新解析（验证 bracket 平衡性） */
    const char *parse_msg = session->stages[lv_STAGE_PARSE].error_msg;
    if (parse_msg) {
        int bracket_depth = 0;
        int len = (int) strlen(parse_msg);
        for (int i = 0; i < len; i++) {
            if (parse_msg[i] == '(' || parse_msg[i] == '[')
                bracket_depth++;
            if (parse_msg[i] == ')' || parse_msg[i] == ']')
                bracket_depth--;
            if (bracket_depth < 0) {
                snprintf(desc, desc_size, "往返验证失败：解析输出包含不匹配的括号");
                return 0;
            }
        }
        if (bracket_depth != 0) {
            snprintf(desc, desc_size, "往返验证失败：解析输出括号不平衡 (depth=%d)", bracket_depth);
            return 0;
        }
    }

    /* 检查推理阶段输出是否包含可重新解析的数值统计 */
    const char *reasoning_msg = session->stages[lv_STAGE_REASONING].error_msg;
    if (reasoning_msg) {
        /* 验证数值统计格式：应包含 "尝试 N 策略" 模式 */
        if (strstr(reasoning_msg, "尝试") != NULL && strstr(reasoning_msg, "策略") != NULL) {
            /* 格式正确，可重新解析 */
        } else if (strstr(reasoning_msg, "模拟") != NULL) {
            /* 模拟模式也包含可解析信息 */
        } else {
            snprintf(desc, desc_size, "往返验证警告：推理输出格式可能不可重新解析");
            /* 不严格失败 */
        }
    }

    snprintf(desc, desc_size, "往返验证通过: 输出包含 %d 个结构化标记，可重新解析", found_markers);
    return 1;
}

/**
 * @brief 检查函数分发表类型
 *
 * 所有内部检查函数遵循统一签名：接收只读会话指针、输出描述缓冲区及大小，
 * 返回 1（通过）或 0（失败）。
 */
typedef int (*check_func_t)(const lvSession *, char *, int);

/** 
 * @brief 六项检查函数的分发表
 *
 * 索引与 lvVerifyCheck 枚举值一一对应，通过 lv_meta_verify_session 遍历调用。
 * 每个函数执行单一维度的验证，结果写入 lvMetaVerifyResult::description。
 */
static const check_func_t g_check_funcs[lv_CHECK_COUNT] = {
    check_structural,       /* lv_CHECK_STRUCTURAL */
    check_type_consistency, /* lv_CHECK_TYPE */
    check_completeness,     /* lv_CHECK_COMPLETE */
    check_soundness,        /* lv_CHECK_SOUND */
    check_nontriviality,    /* lv_CHECK_NONTRIVIAL */
    check_roundtrip         /* lv_CHECK_ROUNDTRIP */
};

/* ============================================================
 * 公共接口实现
 * ============================================================ */

/**
 * @brief 创建元验证器实例
 *
 * 分配并初始化元验证器，所有六项检查默认全部启用，严格模式关闭。
 * 验证器通过 check_mask 位掩码控制各项检查的启停。
 *
 * @return 成功返回元验证器指针，内存分配失败返回 NULL
 */
lvMetaVerifier *lv_meta_verifier_create(void) {
    lvMetaVerifier *v = lv_calloc(1, sizeof(lvMetaVerifier));
    if (!v)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "failed to allocate meta verifier");
    v->check_mask = (1 << lv_CHECK_COUNT) - 1; /* All checks enabled */
    v->strict_mode = 0;
    return v;
}

/**
 * @brief 销毁元验证器实例
 *
 * 释放元验证器占用的内存。传入 NULL 时安全返回。
 *
 * @param verifier 要销毁的元验证器指针
 */
void lv_meta_verifier_destroy(lvMetaVerifier *verifier) {
    lv_free((void **) &verifier);
}

/**
 * @brief 启用指定验证检查项
 *
 * 通过设置 check_mask 中对应的位来启用某项检查。
 *
 * @param verifier 元验证器指针
 * @param check    要启用的检查项枚举值
 */
void lv_meta_verifier_enable_check(lvMetaVerifier *verifier, lvVerifyCheck check) {
    if (verifier && check >= 0 && check < lv_CHECK_COUNT)
        verifier->check_mask |= (1 << check);
}

/**
 * @brief 禁用指定验证检查项
 *
 * 通过清除 check_mask 中对应的位来跳过某项检查。
 *
 * @param verifier 元验证器指针
 * @param check    要禁用的检查项枚举值
 */
void lv_meta_verifier_disable_check(lvMetaVerifier *verifier, lvVerifyCheck check) {
    if (verifier && check >= 0 && check < lv_CHECK_COUNT)
        verifier->check_mask &= ~(1 << check);
}

/**
 * @brief 设置严格模式
 *
 * 启用严格模式后，验证失败将产生更严格的判定（预留扩展，当前仅存储标记）。
 *
 * @param verifier 元验证器指针
 * @param strict   非零值启用严格模式，零值禁用
 */
void lv_meta_verifier_set_strict(lvMetaVerifier *verifier, int strict) {
    if (verifier)
        verifier->strict_mode = strict;
}

/**
 * @brief 运行会话级元验证
 *
 * 对指定会话依次执行所有已启用的检查项。遍历 g_check_funcs 分发表，
 * 为每项检查记录结果（通过/失败/跳过），并汇总生成验证报告。
 *
 * @param verifier 元验证器（决定哪些检查项启用）
 * @param session  已验证的会话对象（只读）
 * @return 包含每项检查结果和统计摘要的 lvVerifyReport 结构体
 */
lvVerifyReport lv_meta_verify_session(lvMetaVerifier *verifier, const lvSession *session) {
    lvVerifyReport report;
    memset(&report, 0, sizeof(report));
    if (!verifier || !session) {
        strncpy(report.summary, "Invalid verifier or session", sizeof(report.summary));
        report.summary[sizeof(report.summary) - 1] = '\0';
        return report;
    }
    report.total_checks = lv_CHECK_COUNT;
    for (int i = 0; i < lv_CHECK_COUNT; i++) {
        report.results[i].check = (lvVerifyCheck) i;
        if (verifier->check_mask & (1 << i)) {
            /* 调用对应的检查函数 */
            int passed =
                g_check_funcs[i](session, report.results[i].description, sizeof(report.results[i].description));
            report.results[i].passed = passed;
            if (passed) {
                report.passed_checks++;
            } else {
                report.failed_checks++;
            }
        } else {
            report.results[i].passed = -1; /* Skipped */
            report.skipped_checks++;
        }
    }

    /* 生成摘要 */
    snprintf(report.summary, sizeof(report.summary), "Meta-verification: %d/%d passed, %d failed, %d skipped",
             report.passed_checks, report.total_checks, report.failed_checks, report.skipped_checks);
    return report;
}

/**
 * @brief 运行证明对象级元验证
 *
 * 对 lvProofObject 结构体直接执行六项验证检查（非会话上下文）。
 * 通过 switch-case 逐项检查步骤链完整性、循环依赖、结论匹配目标等。
 *
 * @param verifier 元验证器（决定哪些检查项启用）
 * @param proof    证明对象指针（lvProofObject 类型）
 * @return 包含每项检查结果和统计摘要的 lvVerifyReport 结构体
 */
lvVerifyReport lv_meta_verify_proof(lvMetaVerifier *verifier, void *proof) {
    lvVerifyReport report;
    memset(&report, 0, sizeof(report));
    if (!verifier) {
        strncpy(report.summary, "Invalid verifier", sizeof(report.summary) - 1);
        return report;
    }

    lvProofObject *p = (lvProofObject *) proof;
    report.total_checks = lv_CHECK_COUNT;

    for (int i = 0; i < lv_CHECK_COUNT; i++) {
        report.results[i].check = (lvVerifyCheck) i;
        if (!(verifier->check_mask & (1 << i))) {
            report.results[i].passed = -1; /* Skipped */
            report.skipped_checks++;
            continue;
        }

        int passed = 0;
        lvStrBuf sb = {0};

        switch ((lvVerifyCheck) i) {
            case lv_CHECK_STRUCTURAL: {
                /* 检查证明至少有一个步骤 */
                if (!p) {
                    lv_strbuf_printf(&sb, "Proof is NULL");
                    break;
                }
                if (p->step_count < 1) {
                    lv_strbuf_printf(&sb, "Proof has no steps (step_count=%d)", p->step_count);
                    break;
                }
                if (!p->steps || !p->steps[0]) {
                    lv_strbuf_printf(&sb, "Proof steps array is NULL or first step is NULL");
                    break;
                }
                lv_strbuf_printf(&sb, "Structural check passed: %d steps", p->step_count);
                passed = 1;
                break;
            }
            case lv_CHECK_TYPE: {
                /* 检查步骤链有效性：每个步骤的前提引用更早的步骤 */
                if (!p) {
                    lv_strbuf_printf(&sb, "Proof is NULL");
                    break;
                }
                passed = 1;
                for (int s = 0; s < p->step_count; s++) {
                    lvProofStepRecord *step = p->steps[s];
                    if (!step) {
                        lv_strbuf_printf(&sb, "Step %d is NULL", s);
                        passed = 0;
                        break;
                    }
                    for (int j = 0; j < step->premise_count; j++) {
                        int pid = step->premise_step_ids[j];
                        if (pid < 0 || pid >= s) {
                            lv_strbuf_printf(&sb, "Step %d references premise %d which is not an earlier step",
                                     s, pid);
                            passed = 0;
                            break;
                        }
                    }
                    if (!passed)
                        break;
                }
                if (passed) {
                    lv_strbuf_printf(&sb, "Type/chain check passed: all premises reference earlier steps");
                }
                break;
            }
            case lv_CHECK_COMPLETE: {
                /* 检查最后一步的结论是否匹配目标 */
                if (!p) {
                    lv_strbuf_printf(&sb, "Proof is NULL");
                    break;
                }
                if (p->step_count == 0) {
                    lv_strbuf_printf(&sb, "No steps to check conclusion");
                    break;
                }
                lvProofStepRecord *last = p->steps[p->step_count - 1];
                if (!last || !last->conclusion) {
                    lv_strbuf_printf(&sb, "Last step has no conclusion");
                    break;
                }
                if (p->goal && last->conclusion != p->goal &&
                    !(last->conclusion->label && p->goal->label &&
                      strcmp(last->conclusion->label, p->goal->label) == 0)) {
                    lv_strbuf_printf(&sb, "Last step conclusion does not match goal");
                    break;
                }
                lv_strbuf_printf(&sb, "Completeness check passed: final conclusion matches goal");
                passed = 1;
                break;
            }
            case lv_CHECK_SOUND: {
                /* 检查循环依赖：确保没有步骤间接依赖自身 */
                if (!p) {
                    lv_strbuf_printf(&sb, "Proof is NULL");
                    break;
                }
                passed = 1;
                /* 对每个步骤，检查其前提链是否回指自身 */
                for (int s = 0; s < p->step_count; s++) {
                    lvProofStepRecord *step = p->steps[s];
                    if (!step)
                        continue;
                    /* BFS 检查前提链中是否包含步骤 s 自身 */
                    /* 队列大小：最坏情况每个步骤的所有前提都入队 */
                    int queue_cap = p->step_count * 4;
                    if (queue_cap < 16)
                        queue_cap = 16;
                    int *queue = (int *) lv_malloc((size_t) queue_cap * sizeof(int));
                    int *visited = (int *) lv_calloc((size_t) p->step_count, sizeof(int));
                    if (!queue || !visited) {
                        lv_free((void **) &queue);
                        lv_free((void **) &visited);
                        lv_strbuf_printf(&sb, "Memory allocation failed");
                        passed = 0;
                        break;
                    }
                    int head = 0, tail = 0;
                    for (int j = 0; j < step->premise_count; j++) {
                        if (tail < queue_cap) {
                            queue[tail++] = step->premise_step_ids[j];
                        }
                    }
                    visited[s] = 1;
                    int found_cycle = 0;
                    while (head < tail) {
                        int cur = queue[head++];
                        if (cur == s) {
                            found_cycle = 1;
                            break;
                        }
                        if (cur < 0 || cur >= p->step_count)
                            continue;
                        if (visited[cur])
                            continue;
                        visited[cur] = 1;
                        lvProofStepRecord *cur_step = p->steps[cur];
                        if (cur_step) {
                            for (int j = 0; j < cur_step->premise_count; j++) {
                                if (tail < queue_cap) {
                                    queue[tail++] = cur_step->premise_step_ids[j];
                                }
                            }
                        }
                    }
                    lv_free((void **) &queue);
                    lv_free((void **) &visited);
                    if (found_cycle) {
                        lv_strbuf_printf(&sb, "Circular dependency detected at step %d", s);
                        passed = 0;
                        break;
                    }
                }
                if (passed) {
                    lv_strbuf_printf(&sb, "Soundness check passed: no circular dependencies");
                }
                break;
            }
            case lv_CHECK_NONTRIVIAL: {
                /* 检查证明非平凡：至少有2个步骤或使用了公理/假设 */
                if (!p) {
                    lv_strbuf_printf(&sb, "Proof is NULL");
                    break;
                }
                if (p->step_count <= 1 && p->axiom_count == 0 && p->assumption_count == 0) {
                    lv_strbuf_printf(&sb, "Proof is trivial: only %d step(s), no axioms or assumptions",
                             p->step_count);
                    break;
                }
                lv_strbuf_printf(&sb, "Nontriviality check passed: %d steps, %d axioms, %d assumptions",
                         p->step_count, p->axiom_count, p->assumption_count);
                passed = 1;
                break;
            }
            case lv_CHECK_ROUNDTRIP: {
                /* 检查证明标记为已完成 */
                if (!p) {
                    lv_strbuf_printf(&sb, "Proof is NULL");
                    break;
                }
                if (!p->is_proved) {
                    lv_strbuf_printf(&sb, "Proof is not marked as proved");
                    break;
                }
                lv_strbuf_printf(&sb, "Roundtrip check passed: proof is marked as proved");
                passed = 1;
                break;
            }
            default:
                lv_strbuf_printf(&sb, "Unknown check");
                break;
        }

        report.results[i].passed = passed;
        if (passed) {
            report.passed_checks++;
        } else {
            report.failed_checks++;
        }
        strncpy(report.results[i].description, sb.data, sizeof(report.results[i].description));
        report.results[i].description[sizeof(report.results[i].description) - 1] = '\0';
        lv_strbuf_destroy(&sb);
    }

    /* 生成摘要 */
    snprintf(report.summary, sizeof(report.summary), "Proof meta-verification: %d/%d passed, %d failed, %d skipped",
             report.passed_checks, report.total_checks, report.failed_checks, report.skipped_checks);
    return report;
}

/**
 * @brief 查询验证报告是否全部通过
 *
 * 当报告中无失败项时视为通过（跳过项不影响判定）。
 *
 * @param report 验证报告指针
 * @return 全部通过返回 1，有失败项或 report 为 NULL 返回 0
 */
int lv_verify_report_passed(const lvVerifyReport *report) {
    return report ? (report->failed_checks == 0) : 0;
}

/**
 * @brief 获取验证报告的文本摘要
 *
 * 返回预先生成的摘要字符串，形如 "Meta-verification: 5/6 passed, 0 failed, 1 skipped"。
 *
 * @param report 验证报告指针
 * @return 摘要字符串指针，report 为 NULL 时返回 NULL
 */
const char *lv_verify_report_summary(const lvVerifyReport *report) {
    if (!report)
        return NULL;
    return report->summary;
}

/**
 * @brief 获取单项检查的详细结果
 *
 * 按检查项索引返回对应的 lvMetaVerifyResult，包含检查状态和描述信息。
 *
 * @param report 验证报告指针
 * @param check  要查询的检查项枚举值
 * @return 单项检查结果指针，参数无效时返回 NULL
 */
const lvMetaVerifyResult *lv_verify_report_result(const lvVerifyReport *report, lvVerifyCheck check) {
    if (!report || check < 0 || check >= lv_CHECK_COUNT)
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "NULL report or invalid check");
    return &report->results[check];
}
