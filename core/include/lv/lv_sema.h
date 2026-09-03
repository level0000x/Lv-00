#ifndef LV_SEMA_H
#define LV_SEMA_H

#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
#include <stdbool.h>

#include "lv/lv_ast.h"
#include "lv/lv_parser.h" /* LvDiagSeverity（R4 三级诊断码共享） */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LV_TYPE_POINT,
    LV_TYPE_LINE,
    LV_TYPE_CIRCLE,
    LV_TYPE_SEGMENT,
    LV_TYPE_RAY,
    LV_TYPE_ANGLE,
    LV_TYPE_TRIANGLE,
    LV_TYPE_POLYGON,
    LV_TYPE_SCALAR,
    LV_TYPE_BOOL,
    LV_TYPE_PROPOSITION,
    LV_TYPE_PROOF,
    LV_TYPE_UNKNOWN,
    LV_TYPE_ERROR
} LvSemanticType;

typedef struct LvSemaContext LvSemaContext;

/** 创建语义分析上下文 */
LvSemaContext *lv_sema_create(void);

/** 销毁语义分析上下文 */
lv_PUBLIC_API void lv_sema_destroy(LvSemaContext *ctx);

/** 对 AST 执行语义分析，返回 true 表示无严重错误 */
lv_PUBLIC_API bool lv_sema_analyze(LvSemaContext *ctx, LvAstNode *ast);

/** 获取语义错误的数量 */
lv_PUBLIC_API int lv_sema_error_count(const LvSemaContext *ctx);

/** 获取第 index 条语义错误消息 */
lv_PUBLIC_API const char *lv_sema_error_msg(const LvSemaContext *ctx, int index);

/** 获取第 index 条语义错误的严重级别（R4 三级诊断码；越界返回 LV_DIAG_ERROR） */
lv_PUBLIC_API LvDiagSeverity lv_sema_error_severity(const LvSemaContext *ctx, int index);

#ifdef __cplusplus
}
#endif

#endif /* LV_SEMA_H */
