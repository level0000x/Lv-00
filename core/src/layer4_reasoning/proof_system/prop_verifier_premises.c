/*
 * @file prop_verifier_premises.c
 * @brief Proposition verifier module - premise search
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
 * 前提搜索
 * ============================================================ */

/* 在前提列表中查找公式 */
bool premise_contains(const PropFormula **premises, int count, const PropFormula *f) {
    for (int i = 0; i < count; i++) {
        if (formula_equal(premises[i], f))
            return true;
    }
    return false;
}

