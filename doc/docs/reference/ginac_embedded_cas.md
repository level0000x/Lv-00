# GiNaC 内嵌符号计算架构核心借鉴设计

> **借鉴项目**：GiNaC（www.ginac.de）
> **核心借鉴点**：C++内嵌符号计算架构、Pimpl+引用计数的 `ex` 句柄模式、自动规范化（canonicalization）、n 元 Add/Mul 容器、JIT 表达式编译
> **分类**：P1 高优先级 / 符号计算引擎架构
> **日期**：2026-05-24

---

## 1. 概述

GiNaC（GiNaC is Not a CAS）是一个用 C++ 实现的开源符号计算库，其最核心的设计哲学是**将符号计算嵌入宿主语言**——符号表达式作为 C++ 对象自然存在于 C++ 程序中，利用 C++ 的运算符重载、RAII 和模板机制，使得符号计算的 API 与数值计算一样自然。GiNaC 被广泛用于高能物理计算（如 xloops 和 nestedsums），并在性能上远超解释型 CAS（如 Maxima）。

GiNaC 的四个核心架构设计对 Lv-00 的 `normalization.h` 和 `SymbolicCoord` 系统有关键借鉴价值：

1. **`ex` 轻量句柄 + `basic` 重对象的 Pimpl 模式**：GiNaC 使用引用计数的 `ex`（expression handle）作为轻量级句柄，指向堆上的 `basic` 派生类对象。这种"值语义 + 引用实现"的 Pimpl（Pointer to Implementation）模式兼顾了 API 简洁性（用户看到的是值类型 `ex`）和内存管理便利性（引用计数自动回收）。Lv-00 的 `SymbolicCoord` 可完全借鉴这一模式。

2. **自动规范化（canonicalization）**：GiNaC 在构造表达式时**立即**进行代数规范化：`x + x` 自动规约为 `2*x`，`x * 1` 自动规约为 `x`，`0 * x` 自动规约为 `0`。规范化通过每个 `basic` 子类的虚函数 `eval()` 实现，在表达式构造时被调用以确保"构造即规范"。这一机制正是 Lv-00 的 `normalization.h` 需要的功能。

3. **n 元 `add` / `mul` 容器 vs 二叉树**：GiNaC 使用 n 元 `add` 和 `mul` 节点（而非严格的二叉树）存储和项与积项，每个 `add` 节点内部维护一组 `(系数, 项)` 对。这种扁平化的 n 元结构在符号计算中比二叉树更高效——不仅减少了内存分配次数，也使合并同类项、提取公因式等操作变为线性扫描。

4. **JIT 表达式编译**：GiNaC 支持将符号表达式编译为机器码（通过 `compile_ex` 函数），在数值求值时绕过解释性遍历，直接执行编译后的函数。这一特性对 Lv-00 的数值逼近阶段（将符号坐标转换为具体数值进行快速验证）有直接应用价值。

---

## 2. 核心借鉴 → Lv-00 映射

### 2.1 `ex` 轻量句柄 + Pimpl 模式 → Lv-00 SymbolicCoord

GiNaC 的核心类型 `ex` 本质上是一个轻量级句柄（类似 C++ 的 `std::shared_ptr`，但带有值语义）：

```
GiNaC 架构：
  ex (句柄)                  basic (基类)
  +-------------+          +------------------+
  | ptr (basic*) | ------->| refcount: int    |
  +-------------+          | tinfo: type_info |
                            | eval(): ex*      |
                            | subs(): ex*      |
                            | normal(): ex*    |
                            +------------------+
                                  ^
                                  | 继承
                     +------------+------------+
                     |            |            |
                  add           mul         symbol
              +-------+    +-------+    +----------+
              | terms |    |factors|    | name:str |
              +-------+    +-------+    +----------+
```

**核心设计原则**：
- `ex` 对象本身只包含一个指向 `basic` 的指针（通常 8 字节），可高效地在栈上传递
- `ex` 的拷贝只是增加引用计数，不复制底层表达式树
- `ex` 的析构只是减少引用计数，为 0 时自动释放
- `basic` 的子类通过虚函数 `eval()` 实现自动规范化

Lv-00 的 `SymbolicCoord` 可完全借鉴这一模式：

```c
/**
 * @file symbolic_coord.h — Lv-00 的符号坐标系统
 *
 * 借鉴 GiNaC 的 ex 句柄 + basic 重对象 Pimpl 模式，
 * 在 C 语言中实现符号表达式的轻量级值语义。
 */

/**
 * @brief 符号表达式句柄 —— 对应 GiNaC 的 ex
 *
 * SymbolicCoord 是 Lv-00 中表示符号几何坐标的核心类型。
 * 它是一个轻量级句柄（仅含一个指针 + 内联标志），
 * 指向堆上的 SymbolicCoordImpl 对象。
 *
 * 值语义：
 *  - 拷贝构造 = 增加引用计数
 *  - 赋值 = 释放旧引用 + 增加新引用
 *  - 销毁 = 减少引用计数（为 0 时自动释放）
 *
 * 内联优化：
 *  - 对于简单的数值常量，数据直接存储在句柄内部（is_inline = true）
 *  - 避免为常量表达式（如 SymbolicCoord(3.14)）分配堆内存
 */
typedef struct {
    union {
        SymbolicCoordImpl *ptr;     /**< 指向堆上实现对象的指针 */
        double             val;     /**< 内联数值（is_inline = true 时） */
    } data;
    uint32_t flags;                /**< 标志位：
                                        bit 0: is_inline
                                        bit 1-3: 表达式种类（KIND_*）
                                        bit 4-15: 保留 */
} SymbolicCoord;

/**
 * @brief 符号表达式实现基类 —— 对应 GiNaC 的 basic
 *
 * 所有具体的符号表达式类型（数值、符号变量、加法、乘法、函数应用）
 * 都继承自 SymbolicCoordImpl（通过包含作为首字段来实现 C 的"继承"）。
 */
typedef struct SymbolicCoordImpl {
    int32_t  refcount;             /**< 引用计数 */
    uint16_t kind;                 /**< 表达式种类 */
    uint16_t flags;                /**< 种类特定标志 */
    /**
     * @brief 自动规范化虚函数 —— 对应 GiNaC 的 basic::eval()
     *
     * 在表达式构造时立即调用，返回规范化后的等价表达式。
     * 例如：
     *   add(x, x)     → eval() → mul(2, x)
     *   mul(1, expr)  → eval() → expr
     *   add(x, 0)     → eval() → x
     *   mul(x, 0)     → eval() → 0
     *
     * 如果当前表达式已经是规范形式，返回 NULL（保持原样）。
     */
    SymbolicCoord   (*eval)(struct SymbolicCoordImpl *self);
    /**
     * @brief 打印/序列化虚函数
     */
    void            (*print)(struct SymbolicCoordImpl *self, char *buf, int bufsz);
    /**
     * @brief 哈希虚函数 —— 用于合并同类项（如 add 中的系数合并）
     */
    uint64_t        (*hash)(struct SymbolicCoordImpl *self);
    /**
     * @brief 比较虚函数 —— 判断两个表达式是否结构等价
     */
    bool            (*equals)(struct SymbolicCoordImpl *self, struct SymbolicCoordImpl *other);
    /**
     * @brief 替换虚函数 —— 将子表达式替换为新表达式
     */
    SymbolicCoord   (*subs)(struct SymbolicCoordImpl *self, SymbolicCoord from, SymbolicCoord to);
    /**
     * @brief 求值虚函数 —— 如果可以完全数值化，返回数值（对应 JIT 编译的 fallback）
     */
    int             (*to_double)(struct SymbolicCoordImpl *self, double *out);
} SymbolicCoordImpl;

/**
 * @brief 表达式种类枚举 —— 对应 GiNaC 的 basic 派生类层级
 */
typedef enum {
    EXPR_KIND_NUMERIC,       /**< 数值常量：3.14, -2.5 */
    EXPR_KIND_SYMBOL,        /**< 符号变量：x, y, radius */
    EXPR_KIND_ADD,           /**< n 元加法：a + b + c (扁平化表示) */
    EXPR_KIND_MUL,           /**< n 元乘法：a * b * c (扁平化表示) */
    EXPR_KIND_POW,           /**< 幂：x^2, x^(1/2) */
    EXPR_KIND_FUNC,          /**< 函数应用：sin(x), sqrt(y) */
    EXPR_KIND_RELATION,      /**< 关系：x = y, x < y */
} ExprKind;

#define SYMBOLIC_COORD_INLINE_FLAG   (1u << 0)  /**< 内联数据标志 */
#define SYMBOLIC_COORD_KIND_MASK     (0xEu)      /**< 种类掩码（bit 1-3） */
#define SYMBOLIC_COORD_KIND_SHIFT    1           /**< 种类偏移 */

/**
 * @brief 创建符号常量 —— Lv-00 中实现自动规范化的构造器
 *
 * 对应 GiNaC 的 numeric 类，但附加了自动规范化逻辑：
 *  - 对于简单的 double 常量，使用内联存储（避免堆分配）
 *  - 规范化检查：NaN/Inf 拒绝、-0 → +0 规范化
 *
 * @param value 数值
 * @return 规范化的 SymbolicCoord（永不失败）
 */
SymbolicCoord symbolic_coord_from_double(double value);

/**
 * @brief 创建符号变量
 *
 * 对应 GiNaC 的 symbol 类。
 * 同名符号变量共享同一 SymbolicCoordImpl 实例（通过符号表 intern）。
 *
 * @param name 变量名
 * @return SymbolicCoord，失败返回空句柄（flags = 0）
 */
SymbolicCoord symbolic_coord_from_symbol(const char *name);
```

**自动规范化的实现原理**：借鉴 GiNaC 的 `eval()` 约定，每个 Constructor 在返回前调用规范化：

```c
/**
 * @brief 创建加法表达式 —— 带自动规范化（eval）
 *
 * 对应 GiNaC 的 add 类的构造 + eval 组合。
 * 规范化规则（借鉴 GiNaC 的 add::eval()）：
 *  1. 如果子表达式中有 add，扁平化（避免嵌套 add）
 *  2. 合并同类项（如 x + x → 2*x）
 *  3. 处理零消去（如 x + 0 → x）
 *  4. 如果结果只有一项，展开为单项（如 3*x → mul(3, x)）
 *
 * @param left   左操作数
 * @param right  右操作数
 * @return 规范化后的加法表达式
 */
SymbolicCoord symbolic_coord_add(SymbolicCoord left, SymbolicCoord right);

/**
 * @brief 创建乘法表达式 —— 带自动规范化
 *
 * 规范化规则（借鉴 GiNaC 的 mul::eval()）：
 *  1. 扁平化嵌套 mul
 *  2. 处理 0 乘（0 * x → 0）、1 乘（1 * x → x）
 *  3. 合并幂（x * x → x^2）
 *  4. 数值因子合并且前置
 *
 * @param left   左操作数
 * @param right  右操作数
 * @return 规范化后的乘法表达式
 */
SymbolicCoord symbolic_coord_mul(SymbolicCoord left, SymbolicCoord right);
```

### 2.2 自动规范化（Canonicalization）→ Lv-00 normalization.h

GiNaC 的自动规范化是"构造即规范"（construct-canonical form），这是符号计算系统中防止表达式膨胀的关键机制。Lv-00 的 `normalization.h` 可以内嵌同一套规范化规则：

| GiNaC 规范化规则 | Lv-00 实现 | 示例 |
|:---|:---|:---|
| `x + x → 2*x` | `add::eval()` 合并同类项 | `symbolic_coord_add(A, A)` → `2*A` |
| `x * 1 → x` | `mul::eval()` 单位元消去 | `symbolic_coord_mul(expr, one)` → `expr` |
| `x * 0 → 0` | `mul::eval()` 零元消去 | `symbolic_coord_mul(expr, zero)` → `0` |
| `x + 0 → x` | `add::eval()` 零元消去 | `symbolic_coord_add(expr, zero)` → `expr` |
| `0 - x → -x` | `add::eval()` 负号归约 | `symbolic_coord_sub(zero, A)` → `-A` |
| `A - A → 0` | `add::eval()` 抵消 | `symbolic_coord_sub(A, A)` → `0` |
| `x^1 → x` | `pow::eval()` 一次幂展开 | `symbolic_coord_pow(A, one)` → `A` |
| `x^0 → 1` (x != 0) | `pow::eval()` 零次幂 | `symbolic_coord_pow(A, zero)` → `1` |
| 嵌套扁平化 | `add::eval()` / `mul::eval()` | `add(a, add(b,c))` → `add(a, b, c)` |
| 数值因子前置 | `mul::eval()` 排序 | `x*3*y` → `3*x*y` |

#### `normalization.h` 在 Lv-00 中的规范化遍

```c
/**
 * @file normalization.h — Lv-00 的表达式规范化系统
 *
 * 借鉴 GiNaC 的 canonicalization 策略，在构造和修改表达式时
 * 自动维持规范形式。规范化的目标是：
 *  1. 等价的表达式映射到相同的规范表示（便于比较和哈希）
 *  2. 消除无意义的冗余结构（如 1*x, x+0, x/x）
 *  3. 减少符号计算的中间膨胀
 *
 * 规范化发生在：
 *  - 表达式构造时（构造函数自动调用 normalize）
 *  - 替换操作后（subs 后自动调用 normalize）
 *  - 显式调用 normalize() 时（用于批量规范化）
 */

/**
 * @brief 对符号表达式执行完整的规范化
 *
 * 规范化阶段：
 *  1. 扁平化（Flatten）：将嵌套的 add/mul 展开为 n 元形式
 *  2. 合并同类项（Collect）：add 中相同底数的项合并
 *  3. 零/乘法单位消去（Absorb）：处理 0、1 的消去规则
 *  4. 排序（Sort）：将子表达式按规范顺序排序（用于唯一表示）
 *  5. 展开（Expand）：可选，将乘积展开为和的形式
 *
 * @param expr    要规范化的表达式
 * @param options 规范化选项（NORM_* 标志位）
 * @return 规范化后的表达式（可能等于输入，也可能不同）
 */
SymbolicCoord symbolic_coord_normalize(SymbolicCoord expr, int options);

/** 规范化选项 */
#define NORM_FLATTEN      (1 << 0)  /**< 扁平化嵌套 add/mul */
#define NORM_COLLECT      (1 << 1)  /**< 合并同类项 */
#define NORM_ABSORB       (1 << 2)  /**< 消去零元和单位元 */
#define NORM_SORT         (1 << 3)  /**< 按规范顺序排序 */
#define NORM_EXPAND       (1 << 4)  /**< 展开乘积为和 */
#define NORM_FULL         (0xFF)    /**< 全部规范化 */

/**
 * @brief 规范化约束图中的所有符号坐标
 *
 * 这是几何证明管道中的一个可选遍，
 * 在所有符号坐标被计算后、合一检查前调用。
 *
 * @param graph   约束图
 * @param options 规范化选项
 * @return 被规范化的节点数量
 */
int symbolic_coord_normalize_graph(ConstraintGraph *graph, int options);
```

### 2.3 n 元 Add/Mul 容器 vs 二叉树表示

GiNaC 最关键的数据结构决策之一是使用 **n 元容器**（flat container）而非二叉树来表示加法和乘法。这一决策在符号计算中影响深远：

```
二叉树表示 (Lisp 风格):          n 元容器表示 (GiNaC 风格):
                                 
        +                              add
       / \                    +----+---+----+----+
      +   d                  | 3  | a | b  | c  | d  |
     / \                     +----+---+----+----+----+
    +   c                    系数  项1  项2  项3  项4
   / \
  a   b

a + b + c + d                系数 3 代表有 3 个 a 项
需要 3 个 add 节点             只需要 1 个 add 节点
合并同类项 = O(n^3)           合并同类项 = O(n log n)
合并 a 和 3a 需要遍历整棵树     合并 a 和 3a 是哈希查找
```

**效率对比（以 n 项的加法为例）**：

| 操作 | 二叉树 | n 元容器 (GiNaC) | 差距 |
|:---|:---|:---|:---|
| 构造 | O(n) 分配 n-1 个节点 | O(n) 分配 1 个节点 | n 倍内存 |
| 合并同类项 | O(n^2) 遍历 | O(n log n) 排序 + 线性扫描 | ~100x (n=100) |
| 判断相等 | O(n) 递归比较 | O(n) 排序后线性比较 | 相近 |
| 哈希 | O(n) 递归哈希 | O(n) 线性哈希 | 相近 |
| 输出 | O(n) 递归 | O(n) 线性 | 相近 |
| 常系数提取 | O(n) 遍历树 | O(1) 查系数数组 | ~n 倍 |

Lv-00 中的 n 元容器实现：

```c
/**
 * @brief n 元加法节点 —— 借鉴 GiNaC 的 add 类
 *
 * 存储一组 (系数, 项) 对，表示为两个平行数组。
 * 系数的数值部分和整体数值常量分离存储（overall_coeff）。
 */
typedef struct {
    SymbolicCoordImpl base;          /**< "基类"（首字段实现 C 继承） */
    SymbolicCoord     overall_coeff; /**< 数值常量项（如 "5"） */
    int              *coeffs;        /**< 系数数组（整数） */
    SymbolicCoord    *terms;         /**< 项数组 */
    int               term_count;    /**< 项数量 */
    int               capacity;      /**< 容量 */
    bool              is_expanded;   /**< 是否已经展开 */
} AddNodeImpl;

/**
 * @brief n 元乘法节点 —— 借鉴 GiNaC 的 mul 类
 *
 * 存储一组因子和整体数值系数。
 */
typedef struct {
    SymbolicCoordImpl base;          /**< "基类" */
    SymbolicCoord     overall_coeff; /**< 整体数值系数 */
    SymbolicCoord    *factors;       /**< 因子数组 */
    int               factor_count;  /**< 因子数量 */
    int               capacity;      /**< 容量 */
    bool              is_expanded;   /**< 是否已经展开 */
} MulNodeImpl;

/**
 * @brief 合并同类项 —— n 元 add 中的核心操作
 *
 * 借鉴 GiNaC 的 add::eval() 中的 collect 阶段：
 *  1. 对项按哈希分组（相同 term 表达式合并到同一组）
 *  2. 每组内累加系数
 *  3. 丢弃系数为 0 的项
 *  4. 如果只剩一项且系数为 1，返回该项本身
 *
 * @param add_node  加法节点
 * @return 合并后的结果（可能是简化后的表达式）
 */
SymbolicCoord symbolic_coord_add_collect(AddNodeImpl *add_node);
```

### 2.4 GiNaC 类型 → Lv-00 类型完整映射

| GiNaC C++ 类型 | 职责 | Lv-00 C 类型 | 对应 |
|:---|:---|:---|:---|
| `ex` | 轻量句柄（值语义 + 引用计数） | `SymbolicCoord` | 符号坐标值类型 |
| `basic` | 抽象基类（vtable） | `SymbolicCoordImpl` + 函数指针表 | 实现基类 |
| `numeric` | 数值类型（CLN 任意精度） | `EXPR_KIND_NUMERIC` + `double` / `mpfr_t` | 数值常量 |
| `symbol` | 符号变量（互名字符串） | `EXPR_KIND_SYMBOL` + `char*` (intern) | 符号变量 |
| `add` | n 元加法（平展 + 同类项合并） | `AddNodeImpl` (n 元) | 加法表达式 |
| `mul` | n 元乘法（平展 + 因子合并） | `MulNodeImpl` (n 元) | 乘法表达式 |
| `power` | 指数表达式 (base^exp) | `EXPR_KIND_POW` | 幂函数 |
| `function` | 函数 (sin, cos, sqrt 等) | `EXPR_KIND_FUNC` | 函数应用 |
| `relational` | 关系运算符 (=, <, > 等) | `EXPR_KIND_RELATION` | 等式/不等式 |
| `lst` | 表达式列表 | `SymbolicCoord*` 数组 | 表达式序列 |
| `matrix` | 符号矩阵 | `SymbolicMatrix` (待设计) | 坐标变换矩阵 |
| `archive` | 表达式存档 | `symbolic_coord_serialize()` | 序列化/反序列化 |
| `excompiler` | JIT 编译器 | `symbolic_coord_compile()` | JIT 数值求值 |

---

## 3. 实现方案

### 3.1 第一阶段：SymbolicCoord 核心类型（P2-1）

- [ ] 在 `normalization.h` 旁边新建 `symbolic_coord.h`，定义 `SymbolicCoord` 和 `SymbolicCoordImpl`
- [ ] 实现 `symbolic_coord_from_double()` —— 数值构造函数（含内联优化）
- [ ] 实现 `symbolic_coord_from_symbol()` —— 符号变量构造函数（含 intern 表）
- [ ] 实现引用计数管理（`symbolic_coord_retain()` / `symbolic_coord_release()`）
- [ ] 实现 `symbolic_coord_kind()` / `symbolic_coord_is_inline()` 等工具函数
- [ ] 编写 SymbolicCoord 的单元测试（值语义、引用计数、内联优化）

### 3.2 第二阶段：自动规范化引擎（P2-2）

- [ ] 实现 `symbolic_coord_add()` —— 加法构造 + 扁平化 + 合并同类项
- [ ] 实现 `symbolic_coord_mul()` —— 乘法构造 + 扁平化 + 单位元消去
- [ ] 实现 `symbolic_coord_sub()` / `symbolic_coord_div()` —— 减法/除法
- [ ] 实现 `symbolic_coord_pow()` —— 幂构造（含规范化：x^1→x, x^0→1）
- [ ] 实现 `symbolic_coord_normalize()` —— 完整规范化遍
- [ ] 实现 `symbolic_coord_normalize_graph()` —— 约束图全局规范化
- [ ] 编写规范化规则的正确性测试（每个 GiNaC 规范化规则对应一个测试用例）

### 3.3 第三阶段：n 元容器优化（P2-3）

- [ ] 实现 `AddNodeImpl` 的增删查操作（`add_append_term`, `add_remove_term`, `add_has_term`）
- [ ] 实现 `MulNodeImpl` 的增删查操作
- [ ] 实现 `symbolic_coord_add_collect()` —— n 元加法的同类项合并
- [ ] 实现 `symbolic_coord_mul_collect()` —— n 元乘法的幂合并
- [ ] 实现合并同类项的哈希加速（std-like hash-based grouping）
- [ ] 编写 n 元 vs 二叉树的性能基准测试（100 项加法的构造和合并时间）

### 3.4 第四阶段：JIT 编译数值求值（P2-4）

- [ ] 实现 `symbolic_coord_to_double()` —— 解释性数值求值（fallback 路径）
- [ ] 实现 `symbolic_coord_compile()` —— 将符号表达式编译为求值函数指针
- [ ] 实现 JIT 编译的缓存机制（相同表达式复用编译结果）
- [ ] 实现数值求值的自动向量化（一次求值多个坐标点）
- [ ] 编写 JIT vs 解释求值的性能基准测试

---

## 4. 设计决策与权衡

### 4.1 内联优化 vs 堆分配

GiNaC 本身不做内联优化——所有 `ex` 对象都指向堆上的 `basic`。Lv-00 选择加入内联优化（`is_inline` 标志 + `union { ptr; val; }`）的原因是几何计算中大量使用简单数值常量（如坐标值 `(3.0, 0, 0)`、距离值 `5.0`），为每个常量分配堆内存会造成显著的性能损失。

**代价**：`SymbolicCoord` 的大小从 8 字节增加到 16 字节（8 字节 union + 4 字节 flags + 4 字节 padding），但节省了频繁的堆分配/释放开销。

### 4.2 C 语言的"虚函数"实现

GiNaC 利用 C++ 的虚函数实现多态分发。在 C 语言中，使用显式的函数指针表（vtable）来模拟。每个 `SymbolicCoordImpl` 的"派生"类型（如 `AddNodeImpl`）在构造时填充自己的虚函数指针：

```c
// "虚函数表" 手动构造示例
static void add_node_print(SymbolicCoordImpl *self, char *buf, int bufsz) {
    AddNodeImpl *add = (AddNodeImpl *)self;
    // ... 打印 n 元加法 ...
}
// 在 AddNodeImpl 构造函数中：
impl->base.print = add_node_print;
impl->base.eval  = add_node_eval;
impl->base.hash  = add_node_hash;
```

这种方案虽然代码较冗长，但无运行时开销，且与 Lv-00 的 C 语言技术栈完全兼容。

### 4.3 n 元容器 vs 二叉树的适用范围

GiNaC 只在 `add` 和 `mul` 上使用 n 元容器，因为这两种运算是交换结合律的（commutative + associative），天然适合扁平化。对于非交换/非结合运算（如 `pow`、`function`），使用传统的树形表示。Lv-00 遵循相同的原则：n 元容器仅用于 `add` 和 `mul`。

---

## 5. 参考资源

- GiNaC 官方网站：https://www.ginac.de
- GiNaC 源代码仓库：https://www.ginac.de/ginac.git/
- GiNaC 教程与参考手册：https://www.ginac.de/tutorial/
- "GiNaC: An Open Source C++ Computer Algebra Library" — Bauer, Frink, Kreckel (2002)
- "Design Patterns in GiNaC" — the `ex` handle/body idiom (Pimpl pattern)
- "Symbolic Computation in C++" — comparison of C++ CAS libraries
- CLN (Class Library for Numbers) — GiNaC 依赖的任意精度数值库
- Lv-00 已有借鉴文档：`symengine_cpp_symbolic.md`（SymEngine C++ 符号库）、`normalization.h`（规范化 API 定义）

---

## 6. 总结

GiNaC 为 Lv-00 的符号坐标系统提供了四个关键的 C 语言适配架构模式：（1）**ex 句柄模式**——通过 `SymbolicCoord` 轻量句柄 + `SymbolicCoordImpl` 重对象的 Pimpl 设计，在 C 语言中实现符号表达式的值语义和自动内存管理；（2）**自动规范化**——借鉴 GiNaC 的 `eval()` 约定，在每个构造器返回前执行规范化（x+x→2x, x*1→x），确保"构造即规范"；（3）**n 元 Add/Mul 容器**——用扁平化 n 元容器替代二叉树表示加法/乘法，将合并同类项从 O(n^2) 降至 O(n log n)，内存分配从 n-1 个节点降至 1 个节点；（4）**JIT 编译数值求值**——将符号表达式编译为原生函数指针，在数值逼近验证（如坐标法反例搜索）中提供 10-100x 的加速。这四者共同构成了 Lv-00 符号计算引擎在 C 语言层面的高效实现基础。
