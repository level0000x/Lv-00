#ifndef FORMULA_DSL_INTERNAL_H
#define FORMULA_DSL_INTERNAL_H

#include "lv/formula_parser.h"

/* 定义于 formula_dsl_lex.c（原 static，C-⑭ 去 static）：DSL 关键字判定，解析段复用 */
bool is_dsl_keyword(const char *str);

#endif /* FORMULA_DSL_INTERNAL_H */
