#ifndef LV_LOADER_H
#define LV_LOADER_H

#include "lv/lv_parser.h"
#include "lv/lv_sema.h"
#include "engine.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 从文件路径加载 .lv 文件：
 *   读取文件 → lex → parse → sema analyze
 * 返回解析结果（包含 AST 和解析错误）
 */
LvParseResult lv_load_file(const char *filepath);

/**
 * 将解析结果应用到引擎：
 *   - Declaration Point A, B, C → lv_add_point (默认坐标 0,1,0,1)
 *   - Declaration Line/Segment 等 → 存储名称映射，待后续处理
 *   - Constraint 语句 → 添加约束到约束图
 *   - Prove 语句 → 注册为证明目标
 * 返回 true 表示成功
 */
bool lv_apply_parse_result(lvEngine *engine, const LvParseResult *result, LvSemaContext *sema);

#ifdef __cplusplus
}
#endif

#endif /* LV_LOADER_H */
