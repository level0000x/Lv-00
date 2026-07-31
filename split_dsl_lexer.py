import io, re

p = r"core\src\layer1_parser\dsl_compiler.c"
with io.open(p, "r", encoding="utf-8") as f:
    src = f.read()

# 定位 lexer 段：从 "Tokenizer 内部辅助" 注释块开始
m_start = re.search(r"/\* =+\n \*  Tokenizer 内部辅助", src)
assert m_start, "lexer block start not found"

# 定位 dsl_tokens_destroy 函数体结束
m_destroy = re.search(r"void dsl_tokens_destroy\(DslToken \*tokens, int count\) \{", src)
assert m_destroy, "dsl_tokens_destroy not found"
depth = 0
i = m_destroy.end() - 1
while i < len(src):
    if src[i] == "{":
        depth += 1
    elif src[i] == "}":
        depth -= 1
        if depth == 0:
            end = i + 1
            break
    i += 1
else:
    raise RuntimeError("unbalanced dsl_tokens_destroy")

lexer_body = src[m_start.start():end]

# 组装 dsl_lexer.c
header = """/**
 * @file dsl_lexer.c
 * @brief Lv-00 DSL 词法分析器（从 dsl_compiler.c 拆分）
 *
 * @details 将 DSL 源文本转换为 Token 流。公共 API：dsl_tokenize / dsl_tokens_destroy。
 *
 * @author Lv-00 Project
 */

#include "dsl_compiler.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv_internal.h"

"""
lexer_file = header + lexer_body

new_path = r"core\src\layer1_parser\dsl_lexer.c"
with io.open(new_path, "w", encoding="utf-8") as f:
    f.write(lexer_file)
print("dsl_lexer.c written (%d bytes)" % len(lexer_file))

# dsl_compiler.c：删除 lexer 段
removed = src[m_start.start():end]
src = src[:m_start.start()] + src[end:]
# 清理：删除后可能残留多余空行（lexer 段前的空行与后的空行）
with io.open(p, "w", encoding="utf-8") as f:
    f.write(src)
print("dsl_compiler.c trimmed, removed %d bytes" % len(removed))

# CMakeLists 添加 dsl_lexer.c
cm = r"CMakeLists.txt"
with io.open(cm, "r", encoding="utf-8") as f:
    s = f.read()
old = "    core/src/layer1_parser/dsl_compiler.c\n"
assert s.count(old) == 1
s = s.replace(old, old + "    core/src/layer1_parser/dsl_lexer.c\n")
with io.open(cm, "w", encoding="utf-8") as f:
    f.write(s)
print("CMakeLists updated")
