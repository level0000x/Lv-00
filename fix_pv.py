# -*- coding: utf-8 -*-
"""Unstatic cross-file helpers in prop_verifier split."""
import io, os
DIR = r"c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\core\src\layer4_reasoning\proof_system"
def read(p):
    with io.open(p, "rb") as f:
        return f.read().decode("utf-8-sig").replace("\r\n", "\n")
def write(p, s):
    with io.open(p, "w", encoding="utf-8", newline="\n") as f:
        f.write(s)
jobs = [
    ("prop_verifier_compare.c", "static bool formula_equal(const PropFormula *a, const PropFormula *b) {"),
    ("prop_verifier_context.c", "static uint64_t get_time_ms(void) {"),
    ("prop_verifier_hash.c", "static uint64_t formula_hash(const PropFormula *f) {"),
    ("prop_verifier_hash.c", "static uint64_t premises_hash(const PropFormula **premises, int count) {"),
    ("prop_verifier_memo.c", "static int memo_find(ProofContext *ctx, const PropFormula *goal, uint64_t phash) {"),
    ("prop_verifier_memo.c", "static void memo_add(ProofContext *ctx, const PropFormula *goal, uint64_t phash, bool proven) {"),
    ("prop_verifier_premises.c", "static bool premise_contains(const PropFormula **premises, int count, const PropFormula *f) {"),
    ("prop_verifier_engine.c", "static bool prove(ProofContext *ctx, const PropFormula **premises, int premise_count, const PropFormula *goal) {"),
]
for fname, sig in jobs:
    p = os.path.join(DIR, fname)
    t = read(p)
    assert sig in t, "%s: %s" % (fname, sig[:50])
    t = t.replace(sig, sig.replace("static ", "", 1), 1)
    write(p, t)
print("unstatic done (%d)" % len(jobs))
