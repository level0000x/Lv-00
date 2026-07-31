# -*- coding: utf-8 -*-
"""Fix remaining cross-file issues in prop_verifier split."""
import io, os
DIR = r"c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\core\src\layer4_reasoning\proof_system"
def read(p):
    with io.open(p, "rb") as f:
        return f.read().decode("utf-8-sig").replace("\r\n", "\n")
def write(p, s):
    with io.open(p, "w", encoding="utf-8", newline="\n") as f:
        f.write(s)

# 1. engine.c: unstatic prove forward decl
p = os.path.join(DIR, "prop_verifier_engine.c")
t = read(p)
sig = "static bool prove(ProofContext *ctx, const PropFormula **premises, int premise_count, const PropFormula *goal);"
assert sig in t
t = t.replace(sig, sig.replace("static ", "", 1), 1)
write(p, t)
print("engine.c forward decl fixed")

# 2. forward.c: unstatic forward_chain_conjunctions
p = os.path.join(DIR, "prop_verifier_forward.c")
t = read(p)
sig = "static int forward_chain_conjunctions(const PropFormula **input, int input_count, const PropFormula **output,"
assert sig in t
t = t.replace(sig, sig.replace("static ", "", 1), 1)
write(p, t)
print("forward.c fixed")

# 3. analysis.c: unstatic collect_atoms + has_classical_pattern
p = os.path.join(DIR, "prop_verifier_analysis.c")
t = read(p)
for sig in ["static int collect_atoms(const PropFormula *f, char atoms[][PROP_ATOM_NAME_MAX_LEN], int max_atoms) {",
            "static bool has_classical_pattern(const PropFormula *f, char *pattern_desc, size_t desc_size) {"]:
    assert sig in t
    t = t.replace(sig, sig.replace("static ", "", 1), 1)
write(p, t)
print("analysis.c fixed")

# 4. internal.h: add declarations + lv_strbuf include
p = os.path.join(DIR, "prop_verifier_internal.h")
t = read(p)
t = t.replace('#include "lv/stream.h"', '#include "lv/stream.h"\n#include "lv/lv_strbuf.h"', 1)
anchor = "bool prove(ProofContext *ctx, const PropFormula **premises, int premise_count, const PropFormula *goal);"
decls = ("int forward_chain_conjunctions(const PropFormula **input, int input_count, const PropFormula **output,\n"
         "                             int output_capacity);\n"
         "int collect_atoms(const PropFormula *f, char atoms[][PROP_ATOM_NAME_MAX_LEN], int max_atoms);\n"
         "bool has_classical_pattern(const PropFormula *f, char *pattern_desc, size_t desc_size);\n")
assert anchor in t
t = t.replace(anchor, anchor + "\n" + decls)
write(p, t)
print("internal.h updated")
