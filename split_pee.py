# -*- coding: utf-8 -*-
import io, os

SRC = r"c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\core\src\layer4_reasoning\engine\proof_engine_enhanced.c"
OUT_DIR = r"c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\core\src\layer4_reasoning\engine"

with io.open(SRC, encoding="utf-8") as f:
    lines = f.read().split("\n")

def block(a, b):
    """行号 [a,b] (1-based, inclusive) -> list of lines"""
    return lines[a-1:b]

HEADER = u"""/**
 * @file {fname}
 * @brief {brief}
 *
 * @details 本文件从 proof_engine_enhanced.c 拆分子模块生成（Lv-00 v3.3.0+）。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "proof_engine_enhanced.h"
#include "proof_engine_enhanced_internal.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/proof.h"

#include "axiom_rule_engine.h"
#include "error_codes.h"
#include "lv.h"
#include "three_valued_logic.h"

#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/lv_strbuf.h"

"""

def write_file(fname, brief, body_lines, extra=u""):
    body = "\n".join(body_lines)
    if not body.endswith("\n"):
        body += "\n"
    content = HEADER.format(fname=fname, brief=brief) + extra + body
    with io.open(os.path.join(OUT_DIR, fname), "w", encoding="utf-8", newline="\n") as f:
        f.write(content)
    print("wrote", fname, len(body_lines), "lines")

# ---------- 1. proof_engine.c : 生命周期 L126-258 + get_stats L1089-1100 ----------
life = block(126, 258)
life += [u""] + block(1089, 1100)
# 移除策略区遗留的尾部注释
write_file("proof_engine.c", u"证明引擎生命周期管理", life)

# ---------- 2. proof_strategy.c : 策略内核 L259-1087 ----------
write_file("proof_strategy.c", u"证明策略执行内核", block(259, 1087))

# ---------- 3. proof_verify.c : L1102-1230 ----------
write_file("proof_verify.c", u"证明验证", block(1102, 1230))

# ---------- 4. proof_optimize.c : L1232-1422 ----------
write_file("proof_optimize.c", u"证明优化", block(1232, 1422))

# ---------- 5. proof_export.c : L1424-1878 ----------
write_file("proof_export.c", u"证明导出（自然语言/LaTeX/Coq/Isar）", block(1424, 1878))

# ---------- 6. 主文件：保留 L1-124 ----------
main_lines = lines[0:124]
with io.open(SRC, "w", encoding="utf-8", newline="\n") as f:
    f.write("\n".join(main_lines))
    f.write("\n")
print("main file rewritten with", len(main_lines), "lines")
