/**
 * @file test_trust_color.c
 * @brief 信任颜色映射系统单元测试
 *
 * 覆盖：
 *   1. TrustColor ↔ ProofColor 双向映射
 *   2. TrustColor ↔ lvTrustColor 双向映射
 *   3. 颜色合并 (proof_color_combine)
 *   4. 颜色名称 (trust_color_name / proof_color_name)
 *   5. 边界值（越界回退）
 *
 * @author Lv-00 Project
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "lv/lv.h"
#include "lv/trust_color.h"
#include "lv/lv_protocol.h"

/* 测试通过/失败计数 */
static int g_pass = 0;
static int g_fail = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "  FAIL: %s (line %d)\n", msg, __LINE__); \
        g_fail++; \
    } else { \
        g_pass++; \
    } \
} while(0)

/* ================================================================
 * 测试 1: TrustColor → ProofColor 映射
 * ================================================================ */
static void test_trust_to_proof(void) {
    printf("[Test] TrustColor → ProofColor 映射\n");

    TEST_ASSERT(trust_color_to_proof(TRUST_GREEN) == PROOF_COLOR_GREEN,
                "TRUST_GREEN → PROOF_COLOR_GREEN");
    TEST_ASSERT(trust_color_to_proof(TRUST_BLUE_UNEXPLORED) == PROOF_COLOR_BLUE_UNEXPLORED,
                "TRUST_BLUE_UNEXPLORED → PROOF_COLOR_BLUE_UNEXPLORED");
    TEST_ASSERT(trust_color_to_proof(TRUST_BLUE_EXCEEDED) == PROOF_COLOR_BLUE_RESOURCE,
                "TRUST_BLUE_EXCEEDED → PROOF_COLOR_BLUE_RESOURCE");
    TEST_ASSERT(trust_color_to_proof(TRUST_BLUE_OUT_OF_SCOPE) == PROOF_COLOR_BLUE_OUT_OF_RANGE,
                "TRUST_BLUE_OUT_OF_SCOPE → PROOF_COLOR_BLUE_OUT_OF_RANGE");
    TEST_ASSERT(trust_color_to_proof(TRUST_YELLOW) == PROOF_COLOR_YELLOW,
                "TRUST_YELLOW → PROOF_COLOR_YELLOW");
    TEST_ASSERT(trust_color_to_proof(TRUST_LIGHT_ORANGE_ORACLE) == PROOF_COLOR_ORANGE_ORACLE,
                "TRUST_LIGHT_ORANGE_ORACLE → PROOF_COLOR_ORANGE_ORACLE");
    TEST_ASSERT(trust_color_to_proof(TRUST_LIGHT_ORANGE_EXPLOSION) == PROOF_COLOR_ORANGE_EX_FALSO,
                "TRUST_LIGHT_ORANGE_EXPLOSION → PROOF_COLOR_ORANGE_EX_FALSO");
    TEST_ASSERT(trust_color_to_proof(TRUST_AMBER) == PROOF_COLOR_AMBER,
                "TRUST_AMBER → PROOF_COLOR_AMBER");
    TEST_ASSERT(trust_color_to_proof(TRUST_DEEP_ORANGE) == PROOF_COLOR_DARK_ORANGE,
                "TRUST_DEEP_ORANGE → PROOF_COLOR_DARK_ORANGE");
    TEST_ASSERT(trust_color_to_proof(TRUST_RED) == PROOF_COLOR_RED_CONFLICT,
                "TRUST_RED → PROOF_COLOR_RED_CONFLICT");

    /* 越界测试 */
    TEST_ASSERT(trust_color_to_proof((TrustColor)99) == PROOF_COLOR_BLUE_UNEXPLORED,
                "越界 TrustColor 回退到 BLUE_UNEXPLORED");
}

/* ================================================================
 * 测试 2: ProofColor → TrustColor 映射
 * ================================================================ */
static void test_proof_to_trust(void) {
    printf("[Test] ProofColor → TrustColor 映射\n");

    TEST_ASSERT(proof_color_to_trust(PROOF_COLOR_GREEN) == TRUST_GREEN,
                "PROOF_COLOR_GREEN → TRUST_GREEN");
    TEST_ASSERT(proof_color_to_trust(PROOF_COLOR_BLUE_UNEXPLORED) == TRUST_BLUE_UNEXPLORED,
                "PROOF_COLOR_BLUE_UNEXPLORED → TRUST_BLUE_UNEXPLORED");
    TEST_ASSERT(proof_color_to_trust(PROOF_COLOR_BLUE_RESOURCE) == TRUST_BLUE_EXCEEDED,
                "PROOF_COLOR_BLUE_RESOURCE → TRUST_BLUE_EXCEEDED");
    TEST_ASSERT(proof_color_to_trust(PROOF_COLOR_AMBER) == TRUST_AMBER,
                "PROOF_COLOR_AMBER → TRUST_AMBER");
    TEST_ASSERT(proof_color_to_trust(PROOF_COLOR_RED_CONFLICT) == TRUST_RED,
                "PROOF_COLOR_RED_CONFLICT → TRUST_RED");
    TEST_ASSERT(proof_color_to_trust(PROOF_COLOR_DARK_ORANGE) == TRUST_DEEP_ORANGE,
                "PROOF_COLOR_DARK_ORANGE → TRUST_DEEP_ORANGE");

    /* 证明完成 → 回退到绿色 */
    TEST_ASSERT(proof_color_to_trust(PROOF_COLOR_GREEN_COMPLETE) == TRUST_GREEN,
                "PROOF_COLOR_GREEN_COMPLETE → TRUST_GREEN");
    TEST_ASSERT(proof_color_to_trust(PROOF_COLOR_GREEN_VERIFIED) == TRUST_GREEN,
                "PROOF_COLOR_GREEN_VERIFIED → TRUST_GREEN");

    /* 越界测试 */
    TEST_ASSERT(proof_color_to_trust((ProofColor)99) == TRUST_BLUE_UNEXPLORED,
                "越界 ProofColor 回退到 BLUE_UNEXPLORED");
}

/* ================================================================
 * 测试 3: 双向映射一致性
 * ================================================================ */
static void test_roundtrip(void) {
    printf("[Test] 双向映射一致性\n");

    /* TrustColor → ProofColor → TrustColor 应保持一致 */
    TrustColor trust_inputs[] = {
        TRUST_GREEN, TRUST_BLUE_UNEXPLORED, TRUST_BLUE_EXCEEDED,
        TRUST_BLUE_OUT_OF_SCOPE, TRUST_YELLOW, TRUST_LIGHT_ORANGE_ORACLE,
        TRUST_LIGHT_ORANGE_EXPLOSION, TRUST_AMBER, TRUST_DEEP_ORANGE, TRUST_RED
    };
    for (size_t i = 0; i < sizeof(trust_inputs)/sizeof(trust_inputs[0]); i++) {
        ProofColor mid = trust_color_to_proof(trust_inputs[i]);
        TrustColor back = proof_color_to_trust(mid);
        /* 不完全一致是允许的（如 VERIFIED → GREEN），但不应回退到未知 */
        if (back == TRUST_BLUE_UNEXPLORED && trust_inputs[i] != TRUST_BLUE_UNEXPLORED) {
            g_fail++;
            fprintf(stderr, "  FAIL: 往返后丢失语义 (input=%d)\n", (int)trust_inputs[i]);
        } else {
            g_pass++;
        }
    }
}

/* ================================================================
 * 测试 4: proof_color_combine 合并规则
 * ================================================================ */
static void test_proof_color_combine(void) {
    printf("[Test] proof_color_combine\n");

    /* 更高优先级胜出 */
    TEST_ASSERT(proof_color_combine(PROOF_COLOR_GREEN, PROOF_COLOR_YELLOW) == PROOF_COLOR_YELLOW,
                "GREEN + YELLOW = YELLOW");
    TEST_ASSERT(proof_color_combine(PROOF_COLOR_BLUE_UNEXPLORED, PROOF_COLOR_AMBER) == PROOF_COLOR_AMBER,
                "BLUE + AMBER = AMBER");

    /* LO + AMBER = DARK_ORANGE */
    TEST_ASSERT(proof_color_combine(PROOF_COLOR_ORANGE_ORACLE, PROOF_COLOR_AMBER) == PROOF_COLOR_DARK_ORANGE,
                "ORACLE + AMBER = DARK_ORANGE");
    TEST_ASSERT(proof_color_combine(PROOF_COLOR_AMBER, PROOF_COLOR_ORANGE_EX_FALSO) == PROOF_COLOR_DARK_ORANGE,
                "AMBER + EX_FALSO = DARK_ORANGE");

    /* 相同值合并 */
    TEST_ASSERT(proof_color_combine(PROOF_COLOR_GREEN, PROOF_COLOR_GREEN) == PROOF_COLOR_GREEN,
                "GREEN + GREEN = GREEN");
    TEST_ASSERT(proof_color_combine(PROOF_COLOR_RED_CONFLICT, PROOF_COLOR_RED_CONFLICT) == PROOF_COLOR_RED_CONFLICT,
                "RED + RED = RED");
}

/* ================================================================
 * 测试 5: 颜色名称
 * ================================================================ */
static void test_color_names(void) {
    printf("[Test] 颜色名称\n");

    TEST_ASSERT(strcmp(trust_color_name(TRUST_GREEN), "Green") == 0,
                "trust_color_name(TRUST_GREEN) == \"Green\"");
    TEST_ASSERT(strcmp(trust_color_name(TRUST_RED), "Red") == 0,
                "trust_color_name(TRUST_RED) == \"Red\"");
    TEST_ASSERT(strcmp(trust_color_name((TrustColor)99), "Unknown") == 0,
                "trust_color_name(越界) == \"Unknown\"");

    TEST_ASSERT(strcmp(proof_color_name(PROOF_COLOR_GREEN), "Green (fully constructed)") == 0,
                "proof_color_name(PROOF_COLOR_GREEN) 正确");
    TEST_ASSERT(strcmp(proof_color_name(PROOF_COLOR_DARK_ORANGE), "Dark orange") == 0,
                "proof_color_name(DARK_ORANGE) 正确");
    TEST_ASSERT(strcmp(proof_color_name((ProofColor)99), "Unknown") == 0,
                "proof_color_name(越界) == \"Unknown\"");
}

/* ================================================================
 * 测试 6: TrustColor ↔ lvTrustColor 映射
 * ================================================================ */
static void test_trust_to_lv(void) {
    printf("[Test] TrustColor ↔ lvTrustColor 映射\n");

    TEST_ASSERT(trust_color_to_lv_protocol(TRUST_GREEN) == lv_COLOR_GREEN,
                "TRUST_GREEN → lv_COLOR_GREEN");
    TEST_ASSERT(trust_color_to_lv_protocol(TRUST_BLUE_UNEXPLORED) == lv_COLOR_BLUE,
                "TRUST_BLUE_UNEXPLORED → lv_COLOR_BLUE");
    TEST_ASSERT(trust_color_to_lv_protocol(TRUST_AMBER) == lv_COLOR_AMBER,
                "TRUST_AMBER → lv_COLOR_AMBER");
    TEST_ASSERT(trust_color_to_lv_protocol(TRUST_RED) == lv_COLOR_RED,
                "TRUST_RED → lv_COLOR_RED");

    /* 互逆映射 */
    TEST_ASSERT(lv_protocol_to_trust_color(lv_COLOR_GREEN) == TRUST_GREEN,
                "lv_COLOR_GREEN → TRUST_GREEN");
    TEST_ASSERT(lv_protocol_to_trust_color(lv_COLOR_AMBER) == TRUST_AMBER,
                "lv_COLOR_AMBER → TRUST_AMBER");
    TEST_ASSERT(lv_protocol_to_trust_color(lv_COLOR_RED) == TRUST_RED,
                "lv_COLOR_RED → TRUST_RED");

    /* lvTrustColor 特有颜色回退 */
    TEST_ASSERT(lv_protocol_to_trust_color(lv_COLOR_GREY) == TRUST_BLUE_UNEXPLORED,
                "lv_COLOR_GREY → TRUST_BLUE_UNEXPLORED（回退）");
    TEST_ASSERT(lv_protocol_to_trust_color(lv_COLOR_PURPLE) == TRUST_GREEN,
                "lv_COLOR_PURPLE → TRUST_GREEN");
}

/* ================================================================
 * 主函数
 * ================================================================ */
int main(void) {
    printf("===== 信任颜色映射系统测试 =====\n\n");

    test_trust_to_proof();
    test_proof_to_trust();
    test_roundtrip();
    test_proof_color_combine();
    test_color_names();
    test_trust_to_lv();

    printf("\n===== 结果: %d 通过, %d 失败 =====\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
