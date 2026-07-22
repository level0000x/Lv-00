/**
 * @file three_layer_arithmetic.h
 * @brief 三层算术编译标志与安全算术宏
 *
 * @details 定义 Lv-00 三层算术系统的编译时控制标志和安全算术运算宏。
 *          三层架构：Layer 0 = 整数精确算术（安全）
 *                    Layer 1 = 有理数精确算术（GMP mpq_t）
 *                    Layer 2 = 浮点近似算术（审计/受限）
 *
 *          编译标志通过 -D 编译选项或在 CMakeLists.txt 中设置，
 *          控制浮点使用策略和安全算术行为。
 *
 * @see doc/docs/THREE_LAYER_ARITHMETIC_SPEC.md
 * @author Lv-00 Project
 * @version 1.1.0
 */

#ifndef LV00_THREE_LAYER_ARITHMETIC_H
#define LV00_THREE_LAYER_ARITHMETIC_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 第一部分：三层算术编译标志
 *
 * 以下标志控制浮点运算的全局行为。
 * 设为 1 时激活对应策略，0 或未定义时使用默认行为。
 * ============================================================ */

/**
 * @brief 禁用所有浮点运算
 *
 * 设为 1 时，整个系统禁止使用 float/double 类型。
 * 所有数值计算强制使用精确算术（整数或有理数）。
 * 编译时如果检测到浮点使用，会产生编译错误。
 *
 * 适用场景：纯符号计算、形式化验证、不允许近似的数学证明。
 */
#ifndef LV00_NO_FLOAT
#define LV00_NO_FLOAT 0          /* 1 = 禁用所有浮点运算 */
#endif

/**
 * @brief 严格精确模式
 *
 * 设为 1 时，禁止任何近似计算。所有数值结果必须是精确的。
 * 与 LV00_NO_FLOAT 不同，此标志允许浮点类型存在，
 * 但禁止任何可能产生精度损失的操作（如 sqrt、sin 等超越函数的近似）。
 *
 * 适用场景：需要绝对精确的代数计算，但允许浮点用于显示/日志。
 */
#ifndef LV00_STRICT_EXACT_MODE
#define LV00_STRICT_EXACT_MODE 0  /* 1 = 严格精确模式，禁用近似 */
#endif

/**
 * @brief 浮点审计模式
 *
 * 设为 1 时，记录系统中所有浮点运算的使用位置和次数。
 * 用于识别意外引入的浮点依赖，帮助迁移到精确算术。
 * 审计结果通过 lv00_float_audit_report() 导出。
 *
 * 适用场景：渐进式移除浮点依赖的迁移阶段。
 */
#ifndef LV00_FLOAT_AUDIT
#define LV00_FLOAT_AUDIT 0        /* 1 = 审计模式，记录所有浮点使用 */
#endif

/* ============================================================
 * 第二部分：安全算术宏
 *
 * 提供溢出检测的整数算术运算。
 * 每个宏调用底层 _impl 函数，返回 bool 指示是否溢出。
 * result 参数通过指针输出结果。
 *
 * 使用示例：
 *   long long a = 1000000000LL, b = 2000000000LL, r;
 *   if (LV00_SAFE_MUL(a, b, &r)) {
 *       // 成功，r 包含结果
 *   } else {
 *       // 溢出处理
 *   }
 * ============================================================ */

/**
 * @brief 安全乘法宏
 *
 * 计算 a * b，检测溢出。成功时 result 指向的变量存放结果，返回非零。
 * 溢出时返回 0，result 内容未定义。
 *
 * @param a      long long 类型的乘数
 * @param b      long long 类型的乘数
 * @param result long long* 类型的输出指针
 * @return int   1 = 成功，0 = 溢出
 */
#ifndef LV00_SAFE_MUL
#define LV00_SAFE_MUL(a, b, result) lv00_safe_mul_impl(a, b, result)
#endif

/**
 * @brief 安全加法宏
 *
 * 计算 a + b，检测溢出。成功时 result 指向的变量存放结果，返回非零。
 * 溢出时返回 0，result 内容未定义。
 *
 * @param a      long long 类型的加数
 * @param b      long long 类型的加数
 * @param result long long* 类型的输出指针
 * @return int   1 = 成功，0 = 溢出
 */
#ifndef LV00_SAFE_ADD_CHECK
#define LV00_SAFE_ADD_CHECK(a, b, result) lv00_safe_add_check_impl(a, b, result)
#endif

/**
 * @brief 安全减法宏
 *
 * 计算 a - b，检测溢出。成功时 result 指向的变量存放结果，返回非零。
 * 溢出时返回 0，result 内容未定义。
 *
 * @param a      long long 类型的被减数
 * @param b      long long 类型的减数
 * @param result long long* 类型的输出指针
 * @return int   1 = 成功，0 = 溢出
 */
#ifndef LV00_SAFE_SUB
#define LV00_SAFE_SUB(a, b, result) lv00_safe_sub_impl(a, b, result)
#endif

/* ============================================================
 * 第三部分：浮点标注宏
 *
 * 用于在代码中标记有意识的浮点使用，便于审计和审查。
 * 这些宏不影响运行时行为，但提供文档化和静态分析支持。
 * ============================================================ */

/**
 * @brief 标记有损类型转换（整数/有理数 -> double）
 *
 * 在将精确值转换为浮点数时使用此宏，标记该转换是有损的。
 * 编译器/静态分析器可据此发出警告。
 *
 * @param x 要转换的值（类型不限）
 * @return double 转换后的浮点值
 *
 * @note 当 LV00_NO_FLOAT=1 时，使用此宏将产生编译错误。
 */
#ifndef LV00_LOSSY_TO_DOUBLE
#define LV00_LOSSY_TO_DOUBLE(x) ((double)(x))  /* 标记有损转换 */
#endif

/**
 * @brief 标记有注释的 double->有理数 转换
 *
 * 在将浮点值转换回精确类型时使用此宏，要求附带说明原因。
 * x 是待转换的值，note 是转换原因的字符串字面量（仅用于文档/审计）。
 *
 * @param x    要转换的浮点值
 * @param note 转换原因的字符串字面量
 *
 * @note 当 LV00_NO_FLOAT=1 时，使用此宏将产生编译错误。
 */
#ifndef LV00_DOUBLE_TO_RATIONAL_NOTE
#define LV00_DOUBLE_TO_RATIONAL_NOTE(x, note) /* 标记有注释的转换 */
#endif

/* ============================================================
 * 第四部分：底层安全函数声明
 *
 * 这些函数在 exact_arithmetic.c 中实现。
 * 用户通常通过上方的 SAFE_* 宏调用，而非直接调用。
 * ============================================================ */

/**
 * @brief 安全乘法实现
 *
 * @param a      乘数 a
 * @param b      乘数 b
 * @param result 输出：a * b 的结果
 * @return int   1 = 成功（无溢出），0 = 溢出或 result 为 NULL
 */
bool lv00_safe_mul_impl(int64_t a, int64_t b, int64_t *out);

/**
 * @brief 安全加法实现（带溢出检测）
 *
 * @param a      加数 a
 * @param b      加数 b
 * @param result 输出：a + b 的结果
 * @return int   1 = 成功（无溢出），0 = 溢出或 result 为 NULL
 */
bool lv00_safe_add_check_impl(int64_t a, int64_t b, int64_t *out);

/**
 * @brief 安全减法实现（带溢出检测）
 *
 * @param a      被减数 a
 * @param b      减数 b
 * @param result 输出：a - b 的结果
 * @return int   1 = 成功（无溢出），0 = 溢出或 result 为 NULL
 */
bool lv00_safe_sub_impl(int64_t a, int64_t b, int64_t *out);

#ifdef __cplusplus
}
#endif

#endif /* LV00_THREE_LAYER_ARITHMETIC_H */
