# -*- coding: utf-8 -*-
"""Move ProofContext/MemoEntry types to internal.h."""
import io, os
DIR = r"c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\core\src\layer4_reasoning\proof_system"
def read(p):
    with io.open(p, "rb") as f:
        return f.read().decode("utf-8-sig").replace("\r\n", "\n")
def write(p, s):
    with io.open(p, "w", encoding="utf-8", newline="\n") as f:
        f.write(s)

# 1. context.c: 删除 24-47 的类型定义
p = os.path.join(DIR, "prop_verifier_context.c")
t = read(p)
start = t.find("/* \u8bb0\u5fc6\u5316\u6761\u76ee\uff1a")
end = t.find("/**\n * @brief \u83b7\u53d6\u5899\u4e0a\u65f6\u949f\u65f6\u95f4")
assert start >= 0 and end > start, (start, end)
t = t[:start] + t[end:]
write(p, t)
print("context.c types removed")

# 2. internal.h: 添加类型
p = os.path.join(DIR, "prop_verifier_internal.h")
t = read(p)
anchor = "/* ---- thread-local stream context (defined in prop_verifier.c) ---- */"
types = '''/* ---- internal data structures ---- */
typedef struct {
    const PropFormula *goal;
    uint64_t premises_hash;
    bool proven;
    bool searched;
} MemoEntry;

typedef struct {
    const PropFormula **premises;
    int premise_count;
    const VerifierConfig *config;
    int steps;
    bool timed_out;
    uint64_t start_time_ms;
    MemoEntry memo[MAX_MEMO_ENTRIES];
    int memo_count;
    int recursion_depth;
} ProofContext;

'''
assert anchor in t
t = t.replace(anchor, types + anchor)
write(p, t)
print("internal.h types added")
