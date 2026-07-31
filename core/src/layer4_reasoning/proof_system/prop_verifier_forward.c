/*
 * @file prop_verifier_forward.c
 * @brief Proposition verifier module - forward chaining
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
#include "lv/stream_context_util.h"

/* ============================================================
 * 前向链接：合取前提展开及信息
 * ============================================================ */

/**
 * @brief 前向链接展开合取前提
 *
 * 将前提集合中的合取公式（A /\ B）展开，
 * 将其子公式分别添加到前提列表中（去重），
 * 迭代执行直到没有新的合取可展开。
 *
 * @param input       输入前提公式数组
 * @param input_count 输入数量
 * @param output      输出前提公式数组（调用者预分配）
 * @param max_output  输出数组最大容量
 * @return 输出前提公式数量
 */
int forward_chain_conjunctions(const PropFormula **input, int input_count, const PropFormula **output,
                                      int max_output) {
    int out_count = 0;
    /* 先复制输入 */
    for (int i = 0; i < input_count && out_count < max_output; i++) {
        output[out_count++] = input[i];
    }
    /* 展开合取 */
    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = 0; i < out_count && out_count < max_output; i++) {
            const PropFormula *p = output[i];
            if (p->type == PROP_CONJUNCTION) {
                /* 检查是否已在列表中 */
                if (!premise_contains(output, out_count, p->data.binary.left)) {
                    output[out_count++] = p->data.binary.left;
                    changed = true;
                }
                if (!premise_contains(output, out_count, p->data.binary.right)) {
                    output[out_count++] = p->data.binary.right;
                    changed = true;
                }
            }
        }
    }
    return out_count;
}

