#ifndef LV_SEMA_H
#define LV_SEMA_H

#include "lv/lv_ast.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LV_TYPE_POINT, LV_TYPE_LINE, LV_TYPE_CIRCLE, LV_TYPE_SEGMENT,
    LV_TYPE_RAY, LV_TYPE_ANGLE, LV_TYPE_TRIANGLE, LV_TYPE_POLYGON,
    LV_TYPE_SCALAR, LV_TYPE_BOOL, LV_TYPE_PROPOSITION, LV_TYPE_PROOF,
    LV_TYPE_UNKNOWN, LV_TYPE_ERROR
} LvSemanticType;

typedef struct LvSemaContext LvSemaContext;

/** 创建语义分析上下文 */
LvSemaContext *lv_sema_create(void);

/** 销毁语义分析上下文 */
void lv_sema_destroy(LvSemaContext *ctx);

/** 对 AST 执行语义分析，返回 true 表示无严重错误 */
bool lv_sema_analyze(LvSemaContext *ctx, LvAstNode *ast);

/** 获取语义错误的数量 */
int lv_sema_error_count(const LvSemaContext *ctx);

/** 获取第 index 条语义错误消息 */
const char *lv_sema_error_msg(const LvSemaContext *ctx, int index);

#ifdef __cplusplus
}
#endif

#endif /* LV_SEMA_H */
