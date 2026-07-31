import io

p = r"core\src\layer1_parser\dsl_lexer.c"
with io.open(p, "r", encoding="utf-8") as f:
    s = f.read()

macros = """/* ================================================================
 *  内部辅助宏
 * ================================================================ */

/** @brief 检查两 Token 类型是否匹配并前进 */
#define TOKEN_IS(tok, tp) ((tok).type == (tp))

/** @brief 安全扩容宏：通用动态数组扩容 */
#define ENSURE_CAP(arr, count, cap, elem_sz, ret_on_fail)          \\
    do {                                                           \\
        if ((count) >= (cap)) {                                    \\
            size_t _new_cap = (cap) == 0 ? 8 : (size_t) (cap) * 2; \\
            void *_np = lv_realloc((arr), _new_cap * (elem_sz));   \\
            if (!_np)                                              \\
                return (ret_on_fail);                              \\
            (arr) = _np;                                           \\
            (cap) = (int) _new_cap;                                \\
        }                                                          \\
    } while (0)

"""

anchor = "#include \"lv_internal.h\"\n\n"
assert s.count(anchor) == 1
s = s.replace(anchor, anchor + macros)
with io.open(p, "w", encoding="utf-8") as f:
    f.write(s)
print("DONE")
