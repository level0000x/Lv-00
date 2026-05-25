# 符号坐标系统 (Symbolic Coordinate System)

## 模块概述

符号坐标系统是 Lv-00 的核心基础设施，负责精确表示和处理几何体的坐标值。系统支持四种类型的符号坐标：有理数、代数数、二次根式和超越常数，确保所有几何计算在符号层面精确执行，而非依赖数值近似。

## 核心设计原则

1. **精确性优先**：所有坐标操作在符号层面精确执行，数值近似仅用于显示
2. **可判定性**：坐标判等必须是可判定的算法
3. **位数熔断**：当计算复杂度超过阈值时提供优雅的降级路径
4. **A/B 计划切换**：支持从完整代数数到二次根式的降级方案

## 数据类型定义

### 坐标类型枚举

```c
typedef enum {
    RATIONAL,        // 有理数 (使用GMP mpq_t)
    ALGEBRAIC,       // 代数数 (极小多项式 + 隔离区间)
    QUADRATIC,       // 二次根式 (a + b√n)
    TRANSCENDENTAL   // 超越常数 (π, e)
} CoordType;
```

### 信任颜色枚举

```c
typedef enum {
    TRUST_GREEN,              // 全构造
    TRUST_BLUE,               // 待完成的证明义务
    TRUST_YELLOW,             // 条件性不可构造
    TRUST_ORANGE,             // 非构造性oracle
    TRUST_LIGHT_ORANGE,       // 爆炸原理
    TRUST_AMBER               // 数值假设
} TrustColor;
```

## 1. 有理数 (Rational)

### 内部表示

使用 GMP 库的 `mpq_t` 类型，即任意精度的分数（分子/分母均为大整数）。

```c
typedef struct Rational {
    mpq_t value;  // GMP 有理数类型
} Rational;
```

### 规范化规则

- 坐标创建时自动约分到最简形式
- 分母必须恒为正整数，分子可为负
- 零统一表示为 0/1

### 核心操作

| 操作 | 函数 | 说明 |
|------|------|------|
| 创建 | `rational_create(num, den)` | 从整数创建有理数 |
| 销毁 | `rational_destroy(r)` | 释放资源 |
| 加法 | `rational_add(a, b)` | 返回新的有理数 |
| 减法 | `rational_subtract(a, b)` | 返回新的有理数 |
| 乘法 | `rational_multiply(a, b)` | 返回新的有理数 |
| 除法 | `rational_divide(a, b)` | 检测除零错误 |
| 比较 | `rational_compare(a, b)` | 返回 -1, 0, 1 |
| 序列化 | `rational_serialize(r)` | 格式 "分子/分母" |
| 解析 | `rational_parse(str)` | 从字符串解析 |

### 判等算法

化为规范形式后，直接比较分子和分母是否完全相等。

### 位数熔断检测

```c
static CircuitStatus check_rational_circuit(const Rational *r) {
    size_t num_bits = mpz_sizeinbase(mpq_numref(r->value), 2);
    size_t den_bits = mpz_sizeinbase(mpq_denref(r->value), 2);
    if (num_bits + den_bits > BIT_CUTOFF_THRESHOLD) {
        return CIRCUIT_TRIPPED;
    }
    return CIRCUIT_OK;
}
```

## 2. 代数数 (Algebraic)

### 内部表示

```c
typedef struct Algebraic {
    mpz_poly_t minimal_poly;   // 整数系数极小多项式
    double left_bound;         // 隔离区间左端点
    double right_bound;        // 隔离区间右端点
    int precision_bits;        // 当前隔离区间的精度
    Rational *cached_rational; // 若有理化则缓存，否则 NULL
} Algebraic;
```

### 创建与验证

创建时必须验证该区间包含唯一实根：
1. 计算极小多项式的所有实根近似
2. 确保隔离区间与任何其他实根的区间均不重叠

### 隔离区间精度管理

- 初始精度仅需区分极小多项式的其他实根（通常 53 位双精度即可）
- 更高精度仅在以下情形触发：
  - 两个不同代数数判等时，精度加倍直到足以判定不相等
  - 达到硬上限（2^100 位 ≈ 1.27×10^30）

### 优先有理化通路

每次算术运算后自动运行：
1. 对结果代数数计算连分式逼近（精度取当前隔离区间宽度的 1/4）
2. 产生有理数候选值 r_approx
3. 在符号层将 r_approx 代入极小多项式精确求值
4. 若多项式值为零，则用有理数节点替换代数数节点

### 算术运算

加减乘除时，通过结式计算结果的极小多项式和新隔离区间。每次运算后自动触发优先有理化通路和位数检测。

### 判等算法

1. 先比较极小多项式（整数多项式逐系数判等）
2. 若多项式相同，再比较隔离区间是否相交且足够精确以区分根
3. 若多项式相同且区间重叠，则为同一代数数
4. 若多项式相同但区间不相交，则为不同代数数（多项式有多个实根）

### 平方根嵌套处理

若运算产生 √(a + b√n) 形式：
1. 检查 a² - b²n 是否为完全平方数
2. 若是，则结果可展开为规范的二次根式形式
3. 否则降级为代数数或标记超界

## 3. 二次根式 (Quadratic)

### 内部表示

```c
typedef struct Quadratic {
    Rational *a;       // 有理数系数
    Rational *b;       // 有理数系数
    unsigned int n;    // 无平方因子正整数
} Quadratic;
```

表示 a + b√n。

### 规范化规则

- n 必须无平方因子
- b = 0 时自动降级为有理数

### 核心操作

**加法**：(a₁ + b₁√n) + (a₂ + b₂√n) = (a₁+a₂) + (b₁+b₂)√n
- 要求 n 相同，否则降级为代数数

**乘法**：(a₁ + b₁√n)(a₂ + b₂√n) = (a₁a₂ + b₁b₂n) + (a₁b₂ + a₂b₁)√n
- 归约到规范形式
- 检查是否可降级

**除法**：(a + b√n) / (c + d√n)
- 有理化分母，分子分母同乘 (c - d√n)
- 分母变为 c² - d²n
- 若分母为零则报错

### 判等算法

直接比较 a、b、n 三元组。三个值完全相等则为同一点。

### 序列化格式

"a,b,n" 格式，例如 "1/2,1/3,2" 表示 1/2 + (1/3)√2。

## 4. 超越常数 (Transcendental)

### 内部表示

```c
typedef struct Transcendental {
    char name[8];  // "pi" 或 "e"
} Transcendental;
```

### 支持列表

仅限于 π（圆周率）和 e（自然对数的底）。不允许用户自创超越常数。

### 判等规则

仅同名判等：
- π 等于 π
- e 等于 e
- π 不等于 e
- 超越常数不与任何其他类型坐标相等

### 算术限制

- 超越常数与有理数加减时保持为符号表达式
- 一旦超越常数与代数数或二次根式组合，结果标记为"可能超出符号覆盖范围"

## 5. 统一符号坐标接口

### SymbolicCoord 结构

```c
typedef struct SymbolicCoord {
    CoordType type;
    union {
        Rational *rational;
        Algebraic *algebraic;
        Quadratic *quadratic;
        Transcendental *transcendental;
    } data;
    TrustColor trust;
} SymbolicCoord;
```

### 统一操作接口

| 操作 | 函数 |
|------|------|
| 创建有理数 | `symbolic_coord_create_rational(num, denom)` |
| 创建代数数 | `symbolic_coord_create_algebraic(poly, left, right)` |
| 创建二次根式 | `symbolic_coord_create_quadratic(a, b, n)` |
| 创建超越常数 | `symbolic_coord_create_transcendental(name)` |
| 销毁 | `symbolic_coord_destroy(coord)` |
| 加法 | `symbolic_coord_add(a, b)` |
| 减法 | `symbolic_coord_subtract(a, b)` |
| 乘法 | `symbolic_coord_multiply(a, b)` |
| 除法 | `symbolic_coord_divide(a, b)` |
| 比较 | `symbolic_coord_compare(a, b)` |
| 序列化 | `symbolic_coord_serialize(coord)` |
| 转浮点 | `symbolic_coord_to_double(coord)` |

### 类型提升规则

运算时遵循以下类型提升顺序：

```
TRANSCENDENTAL > ALGEBRAIC > QUADRATIC > RATIONAL
```

即：
- 有理数 + 二次根式 → 二次根式
- 二次根式 + 代数数 → 代数数
- 任何类型 + 超越常数 → 超越常数（或标记超界）

## 6. 位数熔断系统

### 熔断阈值

```c
#define BIT_CUTOFF_THRESHOLD 1000000  // 100万比特
```

### 检测时机

每次算术运算后的规范化和优先有理化完成后立即检测。

### 熔断响应

1. **信号沿调用栈向上返回**
2. **内核调用者收到信号后**：
   - 暂停当前操作
   - 记录冻结点（当前约束图的完整快照）
   - 向用户界面报告

### 用户选项

| 选项 | 行为 |
|------|------|
| 忽略 | 接受当前节点为"数值辅助"，后续依赖该节点的构造步骤逐个阻塞 |
| 回退 | 恢复到冻结点状态，撤销所有中间操作 |
| 永久降级 | 连续触发 3 次熔断后提供，节点永久标记为 AMBER |

### CircuitStatus 枚举

```c
typedef enum {
    CIRCUIT_OK,      // 正常
    CIRCUIT_WARNED,  // 警告但未熔断
    CIRCUIT_TRIPPED  // 已熔断
} CircuitStatus;
```

## 7. A/B 计划切换

### A 计划（完整代数数）

- 使用 GMP + SymEngine/FLINT 提供完整代数数支持
- 阶段一早期进行隔离压力测试
- 监控隔离区间精度衰减和有理数位数增长

### 切换条件

连续两次压力测试中，任一以下条件未达标：
- **精度稳定性**：隔离区间在连续 100 次运算后精度衰减不超过 1 bit
- **性能稳定性**：单次运算链中有理数中间结果的最大位数不超过 10^6 比特

### B 计划（二次根式）

- 所有超出二次扩张范围的代数数节点降级为数值近似（AMBER 标记）
- 已存在的代数数节点若能表示为二次根式则转换，否则标记
- 切换过程需要序列化并重新加载所有已有工程

### 接口统一

两种计划的 SymbolicCoord 数据结构完全相同，仅内部 data 指向的具体类型不同。上层代码无需修改。

## 实现文件

- **头文件**：`include/lv00/symbolic_coord.h`
- **源文件**：`src/symbolic_coord.c`
- **多项式支持**：`include/lv00/mpz_poly.h`

## 依赖

- GMP 库 (GNU Multiple Precision Arithmetic Library)
- 可选：SymEngine 或 FLINT（A 计划）

## 测试要点

1. 有理数的基本运算和规范化
2. 代数数的隔离区间管理和判等
3. 二次根式的规范化和降级
4. 超越常数的判等限制
5. 位数熔断的触发和处理
6. 类型提升和混合运算
