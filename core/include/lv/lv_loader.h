#ifndef LV_LOADER_H
#define LV_LOADER_H

#include "lv/lv_parser.h"
#include "lv/lv_sema.h"

/* 前向声明：本头仅以指针使用 lvEngine，不引入 engine.h（L4）依赖 */
typedef struct lvEngine lvEngine;

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

/* ================================================================
 * 微自举 B —— 证明验证（lv 系统验证自身证明，路线图步骤 5）
 *
 * 语义：加载 .lv 文件时对文件中的 Prove 断言执行真实验证（C 内实现
 * verify 语义），构成"lv 验证自身证明"的第一闭环。配套规格文件：
 * bootstrap/src/proofs/proof_verifier.lv（用 lv 语言描述验证器自身）。
 *
 * 可验证的断言形式：
 *   - λ-演算验证：Church 编码函数应用（add/sub/mul/pow/succ/pred/
 *     eq/leq/gt/iszero/xor/if/pair 等）编译为 λ-项，De Bruijn β-归约，
 *     与断言值比较（参照 test_lambda_eval 的归约验证能力）；
 *   - 算术验证：整数表达式（+ - * / ^ 与括号）求值比较；
 *   - 布尔验证：Prove true / Prove false、逻辑运算（and/or/not/->/iff）、
 *     纯布尔 λ 目标（如 Prove eq(2, 2);）；
 *   - 反射律：全同名参数的关系调用（如 collinear(A, A, A)）恒真。
 *   - 命题逻辑验证（首次自举，路线图步骤 6）：对纯命题公式（and/or/
 *     not/->/iff + 原子命题）穷举全真值表（2^n）验证恒真；恒真→PASS，
 *     存在反例→FAIL。配套规格文件：bootstrap/src/proofs/propositional_verifier.lv。
 *
 * 无法机械判定的形式（量词、未知函数、除零等）标记为 SKIP，不误报。
 * ================================================================ */

/** @brief Prove 语句验证判定 */
typedef enum {
    LV_PROVE_PASS, /**< 断言验证通过（结论在验证语义下成立） */
    LV_PROVE_FAIL, /**< 断言验证失败（结论不成立） */
    LV_PROVE_SKIP  /**< 无法机械验证（量词/未知函数/非闭合），不判定 */
} LvProveVerdict;

/** @brief 单条 Prove 语句的验证报告 */
typedef struct {
    char name[64];           /**< 报告名称："prove#<序号>" */
    LvProveVerdict verdict;  /**< 判定结果 */
    int line;                /**< 源文件行号 */
    int column;              /**< 源文件列号 */
    char detail[160];        /**< 验证细节（所用规则与结论） */
} LvProveReport;

/** @brief 证明验证汇总（一次加载中全部 Prove 语句的验证结果） */
typedef struct {
    int prove_count;           /**< 文件中 Prove 语句总数 */
    int pass_count;            /**< 验证通过数 */
    int fail_count;            /**< 验证失败数 */
    int skip_count;            /**< 无法判定数 */
    LvProveReport reports[64]; /**< 逐条报告（上限 64 条） */
} LvProveSummary;

/**
 * @brief 对解析结果中的 Prove 语句执行证明验证（微自举 B 核心）
 *
 * 扫描 AST 中的 Prove 语句，逐条按上述验证规则判定并填充 summary。
 * 不修改 AST，可独立于 lv_apply_parse_result 调用。
 *
 * @param result  解析结果（含 AST）
 * @param summary 输出：验证汇总（逐条报告 + 计数）；判定流程完成返回 true
 * @return 验证流程成功执行返回 true（具体判定见 summary），参数错误返回 false
 */
bool lv_verify_proofs(const LvParseResult *result, LvProveSummary *summary);

/**
 * @brief 加载 .lv 文件并执行证明验证（微自举 B 便捷入口）
 *
 * 等价于 lv_load_file + lv_verify_proofs：读取/解析文件后验证其中的
 * Prove 断言，随后释放 AST。验证结果写入 summary（可为 NULL）。
 *
 * @param filepath .lv 文件路径
 * @param summary  输出：证明验证汇总（可为 NULL）
 * @return 文件有效且验证流程执行成功返回 true；读取/解析失败返回 false
 */
bool lv_load_file_verified(const char *filepath, LvProveSummary *summary);

#ifdef __cplusplus
}
#endif

#endif /* LV_LOADER_H */
