#ifndef LV_PARSER_H
#define LV_PARSER_H

#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
#include "lv/lv_ast.h"
#include "lv/lv_lexer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LvParser LvParser;

/** @brief 诊断严重级别（R4 三级诊断码，lv 家族 DSL 解析/语义诊断）
 *  0 = ERROR 为默认：LvParseResult 经 memset 清零后既有错误槽天然为 ERROR，
 *  兼容旧消费方（全部旧槽位语义即错误）；INFO/WARNING 为 R4 扩展显式设置。 */
typedef enum {
    LV_DIAG_ERROR = 0,   /**< 错误（解析/语义失败，默认级别，向后兼容 memset） */
    LV_DIAG_WARNING,     /**< 警告（可恢复但不推荐，如未使用声明） */
    LV_DIAG_INFO         /**< 信息（如 #!suppress 生效记录） */
} LvDiagSeverity;

/** @brief 单条诊断/错误记录（R4：severity 分级 + 修复方向提示） */
typedef struct {
    LvSourceLoc loc;
    LvDiagSeverity severity; /**< 诊断级别（ERROR 默认；扩展字段，旧消费方忽略） */
    char message[256];
    char fix_hint[128];      /**< 修复方向提示（空 = 无建议），如"缺 ';'：在语句末尾加分号" */
} LvParseError;

typedef struct {
    LvAstNode *ast;
    int error_count;
    LvParseError errors[64];
} LvParseResult;

LvParser *lv_parser_create(LvLexer *lexer);
lv_PUBLIC_API void lv_parser_destroy(LvParser *parser);
LvParseResult lv_parser_parse_program(LvParser *parser);

#ifdef __cplusplus
}
#endif

#endif /* LV_PARSER_H */
