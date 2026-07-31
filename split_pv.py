# -*- coding: utf-8 -*-
"""Split prop_verifier.c (2305 lines) into 15 files + internal header."""
import io
import os

SRC = r"c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\core\src\layer4_reasoning\proof_system\prop_verifier.c"
DIR = os.path.dirname(SRC)

with io.open(SRC, "rb") as f:
    raw = f.read()
lines = raw.decode("utf-8-sig").splitlines(keepends=True)
assert len(lines) == 2305, "line count mismatch: %d" % len(lines)

def seg(a, b):
    return lines[a - 1:b]

main_parts = [seg(1, 84)]
fml_parts = [seg(85, 380)]
cmp_parts = [seg(381, 421)]
ser_parts = [seg(422, 595)]
ctx_parts = [seg(596, 639)]
hsh_parts = [seg(640, 709)]
mem_parts = [seg(710, 734)]
pre_parts = [seg(735, 747)]
fwd_parts = [seg(748, 793)]
eng_parts = [seg(794, 1173)]
api_parts = [seg(1174, 1239)]
chk_parts = [seg(1240, 1491)]
anl_parts = [seg(1492, 1734)]
bhk_parts = [seg(1735, 1948)]
trs_parts = [seg(1949, 2110)]
eql_parts = [seg(2111, 2305)]

all_parts = [main_parts, fml_parts, cmp_parts, ser_parts, ctx_parts, hsh_parts,
             mem_parts, pre_parts, fwd_parts, eng_parts, api_parts, chk_parts,
             anl_parts, bhk_parts, trs_parts, eql_parts]
covered = sum(len(p[0]) for p in all_parts)
assert covered == 2305, "coverage: %d" % covered

main_text = "".join(main_parts[0])
main_text = main_text.replace(
    "lv_DECLARE_STREAM_CTX(prop_verifier);",
    "lv_THREAD_LOCAL StreamContext *prop_verifier_stream_ctx = NULL;")
main_text = main_text.replace(
    '#include "lv/stream_context_util.h"',
    '#include "lv/stream_context_util.h"\n#include "prop_verifier_internal.h"', 1)

internal_h = '''/**
 * @file prop_verifier_internal.h
 * @brief Internal shared definitions for proposition verifier module.
 */

#ifndef lv_PROP_VERIFIER_INTERNAL_H
#define lv_PROP_VERIFIER_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv/prop_verifier.h"
#include "lv/lv_internal.h"
#include "lv/stream.h"

/* ---- constants ---- */
#define MAX_PREMISES 64
#define MAX_GOALS 64
#define MAX_MEMO_ENTRIES 1024
#define MAX_FORMULA_STR 2048
#define MAX_COPY_DEPTH 200
#define MAX_DESTROY_DEPTH 200

#define PROP_DESTROY_STACK_INIT_CAP 64
#define PROP_DESTROY_STACK_GROWTH 2

#define PROP_PREC_ATOM 100
#define PROP_PREC_NEGATION 80
#define PROP_PREC_CONJUNCTION 60
#define PROP_PREC_DISJUNCTION 50
#define PROP_PREC_IMPLICATION 40
#define PROP_PREC_DEFAULT 0

#define PROP_HASH_TYPE_MULTIPLIER 2654435761U
#define PROP_HASH_STRING_MULTIPLIER 31
#define PROP_HASH_LEFT_MULTIPLIER 0x9e3779b9U
#define PROP_HASH_RIGHT_MULTIPLIER 0x517cc1b7U
#define PROP_HASH_PTR_MULTIPLIER 0x45d9f3bU
#define PROP_HASH_BIT_SHIFT 16
#define PROP_HASH_PREMISES_MULTIPLIER 31

#define PROP_TIME_MS_PER_SEC 1000

#define PROP_SMOKE_TEST_COUNT 13
#define PROP_SMOKE_MAX_PREM_PTRS 8
#define PROP_SMOKE_CLEANUP_MAX_PTRS 16
#define PROP_ATOM_NAME_MAX_LEN 64
#define PROP_ATOM_COLLECT_MAX 32
#define PROP_PATTERN_DESC_BUFSIZE 256
#define PROP_ANALYSIS_DESC_BUFSIZE 512
#define PROP_MISSING_LIST_BUFSIZE 512
#define PROP_STREAM_EVENT_BUFSIZE 256
#define PROP_JSON_DETAIL_BUFSIZE 192

#define PROP_TRUST_YELLOW_THRESHOLD 2
#define PROP_TRUST_AMBER_MIN 3

/* ---- thread-local stream context (defined in prop_verifier.c) ---- */
extern lv_THREAD_LOCAL StreamContext *prop_verifier_stream_ctx;

/* ---- cross-section helpers ---- */
bool formula_equal(const PropFormula *a, const PropFormula *b);
uint64_t get_time_ms(void);
uint64_t formula_hash(const PropFormula *f);
uint64_t premises_hash(const PropFormula **premises, int count);
int memo_find(ProofContext *ctx, const PropFormula *goal, uint64_t phash);
void memo_add(ProofContext *ctx, const PropFormula *goal, uint64_t phash, bool proven);
bool premise_contains(const PropFormula **premises, int count, const PropFormula *f);
bool prove(ProofContext *ctx, const PropFormula **premises, int premise_count, const PropFormula *goal);

#ifdef __cplusplus
}
#endif

#endif /* lv_PROP_VERIFIER_INTERNAL_H */
'''

def file_header(name, desc):
    return '/*\n' \
           ' * @file %(name)s\n' \
           ' * @brief Proposition verifier module - %(desc)s\n' \
           ' * @details Split from prop_verifier.c\n' \
           ' */\n\n' \
           '#include "lv/prop_verifier.h"\n' \
           '#include "prop_verifier_internal.h"\n\n' \
           '#include <stdarg.h>\n' \
           '#include <stdio.h>\n' \
           '#include <stdlib.h>\n' \
           '#include <string.h>\n' \
           '#include <time.h>\n\n' \
           '#include "lv/lv_internal.h"\n' \
           '#include "lv/lv_utils.h"\n' \
           '#include "lv/stream.h"\n' \
           '#include "lv/stream_context_util.h"\n\n' % {"name": name, "desc": desc}


def join_parts(parts):
    out = []
    for i, p in enumerate(parts):
        out.append("".join(p))
        if i < len(parts) - 1:
            out.append("\n")
    return "".join(out)


files = {
    "prop_verifier_formula.c": (file_header("prop_verifier_formula.c", "formula create/destroy"), fml_parts),
    "prop_verifier_compare.c": (file_header("prop_verifier_compare.c", "formula comparison"), cmp_parts),
    "prop_verifier_serialize.c": (file_header("prop_verifier_serialize.c", "formula serialization"), ser_parts),
    "prop_verifier_context.c": (file_header("prop_verifier_context.c", "proof context"), ctx_parts),
    "prop_verifier_hash.c": (file_header("prop_verifier_hash.c", "hashing"), hsh_parts),
    "prop_verifier_memo.c": (file_header("prop_verifier_memo.c", "memoization"), mem_parts),
    "prop_verifier_premises.c": (file_header("prop_verifier_premises.c", "premise search"), pre_parts),
    "prop_verifier_forward.c": (file_header("prop_verifier_forward.c", "forward chaining"), fwd_parts),
    "prop_verifier_engine.c": (file_header("prop_verifier_engine.c", "core proof engine"), eng_parts),
    "prop_verifier_api.c": (file_header("prop_verifier_api.c", "public verify API"), api_parts),
    "prop_verifier_checks.c": (file_header("prop_verifier_checks.c", "smoke tests"), chk_parts),
    "prop_verifier_analysis.c": (file_header("prop_verifier_analysis.c", "inconstructibility analysis"), anl_parts),
    "prop_verifier_bhk.c": (file_header("prop_verifier_bhk.c", "BHK semantics"), bhk_parts),
    "prop_verifier_trust.c": (file_header("prop_verifier_trust.c", "trust color mapping"), trs_parts),
    "prop_verifier_equivalence.c": (file_header("prop_verifier_equivalence.c", "equivalence and legacy API"), eql_parts),
}

for fname, (header, parts) in files.items():
    body = header + join_parts(parts)
    with io.open(os.path.join(DIR, fname), "w", encoding="utf-8", newline="\n") as f:
        f.write(body)
    print("written %s (%d lines)" % (fname, body.count("\n") + 1))

with io.open(SRC, "w", encoding="utf-8", newline="\n") as f:
    f.write(main_text)
print("rewritten prop_verifier.c (%d lines)" % (main_text.count("\n") + 1))

with io.open(os.path.join(DIR, "prop_verifier_internal.h"), "w", encoding="utf-8", newline="\n") as f:
    f.write(internal_h)
print("written prop_verifier_internal.h (%d lines)" % (internal_h.count("\n") + 1))

print("DONE")
