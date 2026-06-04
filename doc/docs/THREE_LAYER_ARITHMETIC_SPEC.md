# Lv-00 代数计算内核三层数域优化规范

**版本**: v3.4-academic  
**状态**: 规范文档

---

## 1. 概述

本文档定义 Lv-00 代数计算内核的三层数域优化策略，确保在不同计算场景下正确平衡精度与性能。

### 1.1 设计原则

1. **证明路径严格精确**：所有参与证明构造的数值计算必须使用精确表示
2. **数值路径容忍近似**：数值计算后端允许使用浮点，但需显式标记
3. **审计可追溯**：所有浮点使用位置必须可被静态分析工具检测

---

## 2. 三层数域定义

### 2.1 层级 A：严格精确层 (Strict Exact Layer)

```
┌─────────────────────────────────────────────────────────────┐
│                  层级 A：严格精确层                           │
├─────────────────────────────────────────────────────────────┤
│  适用场景:                                                   │
│    - 证明构造与验证                                          │
│    - 认证路径计算                                            │
│    - 符号坐标比较                                            │
│    - 类型等价性判断                                          │
│                                                              │
│  允许类型:                                                   │
│    - 整数类型: int32_t, int64_t                              │
│    - 有理数类型: mpq_t (GMP)                                 │
│    - 代数数类型: 多项式根表示                                │
│    - 定点类型: Lv00Timestamp                                 │
│                                                              │
│  禁止类型:                                                   │
│    - float, double (编译时错误)                              │
│                                                              │
│  编译标志: LV00_NO_FLOAT                                     │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 层级 B：容忍近似层 (Tolerated Approximate Layer)

```
┌─────────────────────────────────────────────────────────────┐
│                  层级 B：容忍近似层                           │
├─────────────────────────────────────────────────────────────┤
│  适用场景:                                                   │
│    - 数值计算后端                                            │
│    - 迭代求解器                                              │
│    - 可视化渲染                                              │
│    - 用户界面显示                                            │
│                                                              │
│  允许类型:                                                   │
│    - float, double (需标记)                                  │
│    - 区间类型: Lv00Interval                                  │
│                                                              │
│  强制要求:                                                   │
│    - 所有浮点变量必须用 LV00_TOLERATED_FLOAT(var) 标记       │
│    - 精度损失转换必须用 LV00_LOSSY_TO_DOUBLE 标注            │
│                                                              │
│  编译标志: 无 (默认模式)                                      │
└─────────────────────────────────────────────────────────────┘
```

### 2.3 层级 C：审计模式层 (Audit Mode Layer)

```
┌─────────────────────────────────────────────────────────────┐
│                  层级 C：审计模式层                           │
├─────────────────────────────────────────────────────────────┤
│  适用场景:                                                   │
│    - 代码审查                                                │
│    - CI/CD 静态分析                                          │
│    - 安全关键验证                                            │
│                                                              │
│  行为:                                                       │
│    - 所有浮点使用产生编译时警告                              │
│    - 生成审计报告                                            │
│    - 标记所有 LV00_TOLERATED_FLOAT 位置                      │
│                                                              │
│  编译标志: LV00_STRICT_EXACT_MODE 或 LV00_FLOAT_AUDIT        │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. 类型系统

### 3.1 精确数值类型

| 类型 | 表示 | 精度 | 用途 |
|------|------|------|------|
| `int64_t` | 整数 | 精确 | 计数、索引 |
| `mpq_t` | 有理数 | 精确 | 符号计算 |
| `mpz_t` | 大整数 | 精确 | 大数运算 |
| `Lv00Timestamp` | 定点时间 | 精确 | 时间戳 |
| `Lv00Rational` | 有理数封装 | 精确 | 几何坐标 |

### 3.2 近似数值类型

| 类型 | 表示 | 精度 | 用途 |
|------|------|------|------|
| `double` | IEEE 754 | 53位尾数 | 数值计算 |
| `float` | IEEE 754 | 24位尾数 | 图形渲染 |
| `Lv00Interval` | 区间 [lo, hi] | 包含保证 | 区间算术 |

### 3.3 类型转换规则

```
┌─────────────────────────────────────────────────────────────┐
│                      类型转换规则                             │
├─────────────────────────────────────────────────────────────┤
│  精确 → 精确:                                                │
│    int64_t → mpq_t     : 隐式允许                            │
│    mpq_t → int64_t     : 需检查可整除性                      │
│                                                              │
│  精确 → 近似:                                                │
│    mpq_t → double      : 需 LV00_LOSSY_TO_DOUBLE 标注        │
│    int64_t → double    : 需 LV00_LOSSY_TO_DOUBLE 标注        │
│                                                              │
│  近似 → 精确:                                                │
│    double → mpq_t      : 需 LV00_DOUBLE_TO_RATIONAL_NOTE     │
│    double → int64_t    : 需显式舍入 + 溢出检查               │
│                                                              │
│  近似 → 近似:                                                │
│    double → float      : 需显式强制转换                      │
│    float → double      : 隐式允许                            │
└─────────────────────────────────────────────────────────────┘
```

---

## 4. 安全算术宏

### 4.1 溢出检测宏

```c
/* 安全乘法：返回 false 表示溢出 */
#define LV00_SAFE_MUL(a, b, result) \
    lv00_safe_mul_impl((int64_t)(a), (int64_t)(b), (int64_t *)(result))

/* 安全加法：返回 false 表示溢出 */
#define LV00_SAFE_ADD_CHECK(a, b, result) \
    lv00_safe_add_check_impl((int64_t)(a), (int64_t)(b), (int64_t *)(result))

/* 安全减法：返回 false 表示溢出 */
#define LV00_SAFE_SUB(a, b, result) \
    lv00_safe_sub_impl((int64_t)(a), (int64_t)(b), (int64_t *)(result))

/* 安全取幂：返回 false 表示溢出 */
bool lv00_safe_pow(int64_t a, int64_t b, int64_t *result);
```

### 4.2 使用示例

```c
int64_t a = 1000000000LL;
int64_t b = 1000000000LL;
int64_t result;

if (LV00_SAFE_MUL(a, b, &result)) {
    /* 安全：result = 10^18 */
} else {
    /* 溢出：处理错误 */
}
```

---

## 5. 区间算术

### 5.1 区间类型定义

```c
typedef struct {
    double lo;       /* 下界 */
    double hi;       /* 上界 */
    int is_exact;    /* 非零表示精确值 */
} Lv00Interval;
```

### 5.2 区间运算规则

```
┌─────────────────────────────────────────────────────────────┐
│                      区间运算规则                             │
├─────────────────────────────────────────────────────────────┤
│  加法: [a,b] + [c,d] = [a+c, b+d]                            │
│  减法: [a,b] - [c,d] = [a-d, b-c]                            │
│  乘法: [a,b] × [c,d] = [min(ac,ad,bc,bd), max(ac,ad,bc,bd)] │
│  除法: [a,b] / [c,d] = [a,b] × [1/d, 1/c]  (0 不在 [c,d])   │
│                                                              │
│  包含保证: 真值 ∈ 计算区间                                    │
└─────────────────────────────────────────────────────────────┘
```

### 5.3 区间应用场景

- 数值解验证：验证迭代解是否包含真值
- 误差传播：追踪浮点误差的累积
- 约束求解：确定变量范围

---

## 6. 编译配置

### 6.1 编译标志

| 标志 | 效果 | 使用场景 |
|------|------|----------|
| `LV00_NO_FLOAT` | 禁止所有浮点 | 证明核心模块 |
| `LV00_STRICT_EXACT_MODE` | 浮点使用警告 | 审计构建 |
| `LV00_FLOAT_AUDIT` | 生成审计标记 | CI/CD 分析 |

### 6.2 CMake 配置

```cmake
# 证明核心模块：禁止浮点
target_compile_definitions(lv00_layer4_reasoning PRIVATE LV00_NO_FLOAT)

# 数值后端：允许浮点
target_compile_definitions(lv00_numerical_backend PRIVATE)

# 审计构建
if(LV00_ENABLE_AUDIT)
    target_compile_definitions(lv00_core PRIVATE LV00_FLOAT_AUDIT)
endif()
```

---

## 7. 代码示例

### 7.1 层级 A 代码（严格精确）

```c
/* proof_construct.c - 证明构造模块 */
#define LV00_NO_FLOAT

#include "exact_arithmetic.h"
#include <gmp.h>

void compute_proof_value(mpq_t result, const mpq_t a, const mpq_t b) {
    /* 所有计算使用精确有理数 */
    mpq_add(result, a, b);
    
    /* 以下代码会触发编译错误：
     * double x = mpq_get_d(a);  // 错误：double 被禁止
     */
}
```

### 7.2 层级 B 代码（容忍近似）

```c
/* numerical_backend.c - 数值后端模块 */
#include "exact_arithmetic.h"
#include "interval_arithmetic.h"

double LV00_TOLERATED_FLOAT(solve_iterative)(double initial) {
    LV00_FLOAT_WARNING;  /* 标记浮点使用 */
    
    double LV00_TOLERATED_FLOAT(x) = initial;
    for (int i = 0; i < 100; i++) {
        double LV00_TOLERATED_FLOAT(next) = x * x - 2.0;
        if (fabs(next - x) < 1e-10) break;
        x = next;
    }
    return x;
}
```

### 7.3 精度损失标注

```c
/* 精确有理数转浮点 */
mpq_t rational;
double approx;
LV00_LOSSY_TO_DOUBLE(mpq_numref(rational), mpq_denref(rational), approx);

/* 浮点转有理数 */
double value = 3.14159;
mpq_t recovered;
mpq_init(recovered);
mpq_set_d(recovered, value);
LV00_DOUBLE_TO_RATIONAL_NOTE("recovered from approximation");
```

---

## 8. 审计工具

### 8.1 静态分析脚本

```bash
#!/bin/bash
# scan_float_usage.sh

echo "=== Lv-00 浮点使用审计 ==="

# 查找所有未标记的 double 声明
grep -rn "double\s\+\w\+\s*=" core/src/ | \
    grep -v "LV00_TOLERATED_FLOAT" | \
    grep -v "interval_arithmetic" | \
    head -20

# 查找所有 LV00_TOLERATED_FLOAT 标记
echo -e "\n=== 已标记的浮点使用 ==="
grep -rn "LV00_TOLERATED_FLOAT" core/src/ | head -20
```

### 8.2 CI/CD 集成

```yaml
# .github/workflows/float_audit.yml
name: Float Usage Audit

on: [push, pull_request]

jobs:
  audit:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Build with audit mode
        run: |
          cmake -DLV00_FLOAT_AUDIT=ON -B build .
          cmake --build build 2>&1 | grep -E "LV00_TOLERATED_FLOAT|warning.*double"
```

---

## 9. 性能考量

### 9.1 精确 vs 近似性能对比

| 操作 | 精确 (mpq_t) | 近似 (double) | 比值 |
|------|-------------|---------------|------|
| 加法 | ~100ns | ~1ns | 100x |
| 乘法 | ~200ns | ~1ns | 200x |
| 比较 | ~50ns | ~1ns | 50x |
| 开方 | 不支持 | ~10ns | N/A |

### 9.2 优化策略

1. **延迟精确化**：先用近似计算，最后一步转精确
2. **分层计算**：核心路径精确，辅助路径近似
3. **缓存精确结果**：避免重复精确计算

---

## 10. 参考文献

1. GMP (GNU Multiple Precision Arithmetic Library)
2. MPFI (Multiple Precision Floating-point Interval library)
3. IEEE 1788 - Standard for Interval Arithmetic
4. FLINT/Arb - Ball/Interval Arithmetic Library

---

**文档状态**: 已完成  
**下一步**: 实现数值后端的区间算术集成
