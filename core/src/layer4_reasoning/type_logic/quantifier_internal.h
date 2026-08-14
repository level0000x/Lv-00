#ifndef QUANTIFIER_INTERNAL_H
#define QUANTIFIER_INTERNAL_H

#include "lv/quantifier.h"
#include "lv/lv_lifecycle.h"
#include "lv/proof.h"
#include "lv/three_valued_logic.h"

/* 内部辅助宏（原定义于 quantifier.c，实例化/消去子模块共享） */
#define RESULT_NAME_BUF_SIZE 256

/* 以下定义于 quantifier.c（核心文件，原 static，C-⑭ 去 static）：
 * 量化表达式/实例化/消去子模块跨文件共享。签名以源文件实际定义为准。 */
struct Proposition *create_result_proposition(int id, const char *name);
lvTruthValue evaluate_body_for_element(const lvQuantifiedExpr *expr, int element_id);
void init_quant_result(lvQuantifiedResult *result);

/* 定义于 quantifier_expr.c（原 static const，C-⑭ 去 static）：结果命题字段销毁表 */
extern const lvFieldDesc kQuantBodyPropDestroyFields[5];

#endif /* QUANTIFIER_INTERNAL_H */
