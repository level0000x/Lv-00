# Idris 2 类型驱动开发范式与 QTT 线性类型借鉴设计

> **借鉴项目**：Idris 2（github.com/idris-lang/Idris2）
> **核心借鉴点**：类型驱动开发（TDD）范式、QTT 量词标记系统（0/1/omega）、多后端自举架构、编译期证明消除
> **分类**：P1 高优先级 / 证明编译期消除与类型驱动架构
> **日期**：2026-05-24

---

## 1. 概述

Idris 2 是由 Edwin Brady 开发的新一代依赖类型编程语言，在 Idris 1 的基础上进行了完整重写。其三个核心特性对 Lv-00 的证明系统设计有重要借鉴价值：

1. **类型驱动开发（Type-Driven Development）**：类型不仅是规范，更是交互式开发过程的驱动力。用户通过类型声明定义目标行为，然后使用编辑器命令（C-c C-s 等）让类型引导实现。这与 Lv-00 的"命题模式驱动构造"范式形成精确对应。

2. **QTT（Quantitative Type Theory）线性类型**：引入量词标记 0（erased/ghost，仅编译期存在）、1（恰好使用一次）、omega（无限制使用），使证明与计算的边界可以精确控制——标记为 0 的证明项在运行时被完全消除。

3. **多后端自举架构**：Idris 2 编译到 Chez Scheme、Racket、Gambit、Node.js、Reference 等多个后端，其自举策略和中间表示（TTImp）设计值得 Lv-00 的多目标代码生成借鉴。

---

## 2. 类型驱动开发范式映射到 Lv-00

### 2.1 Idris 2 的 TDD 核心循环

```
1. 写类型签名:  add : (x : Nat) -> (y : Nat) -> Nat
2. 写定义骨架:  add x y = ?body
3. 用 C-c C-s 生成模式匹配分支（由类型驱动）
4. 用 C-c C-a 自动搜索证明/实现
5. 用 C-c C-r 细化洞（类型主导的细化）
6. 重复直到所有洞填满
```

### 2.2 Lv-00 中的对应流程

在 Lv-00 中，"类型驱动"的对应物是"命题模式驱动构造"——用户声明一个命题（类型），然后由系统（或用户引导）通过几何构造填充该命题模式的约束骨架：

| Idris 2 TDD 步骤 | Lv-00 映射 | 说明 |
|:---|:---|:---|
| 声明类型签名 | 创建 `Proposition`，设置 `pattern`（约束骨架） | 命题模式 = 类型声明 |
| 写定义骨架 `?body` | 创建 `UNRESOLVED_REGION` 节点 | 洞 = 未确定的约束区域 |
| `C-c C-s` 生成分支 | `proof_guided_fill()` 生成填充建议 | 类型引导的策略推荐 |
| `C-c C-a` 自动搜索 | `proof_multi_strategy_try_all()` | 多策略自动证明尝试 |
| `C-c C-r` 细化 | `proof_interactive_step()` 逐步推进 | 交互式证明步骤构建 |

### 2.3 类型驱动 vs 构造驱动：Lv-00 的统一视角

Idris 2 的"类型驱动"和 Lv-00 的"构造驱动"在元语言层面是统一的：

```
类型声明 (Idris)  ≅  命题模式 (Lv-00)
     ↓                      ↓
类型检查            ≅  合一检查 (proof_unify)
     ↓                      ↓
项构造              ≅  几何构造 (添加节点/约束)
     ↓                      ↓
类型引导细化        ≅  约束传播 + 规范化遍
```

这种统一性的深层原因在于：两个系统的核心计算模型都是**命题即类型、证明即程序、构造即项**的 Curry-Howard 同构变体。

---

## 3. QTT 线性类型标记与 Lv-00 "证明仅编译期"概念

### 3.1 QTT 的三重量词体系

QTT 在函数类型箭头 `->` 上附加量词标记：

```
(0 x : A) -> B    -- x 被擦除（erased），运行时不存在，仅用于类型检查
(1 x : A) -> B    -- x 恰好使用一次（线性），运行时存在但不可复制
(x : A) -> B      -- x 无限制使用（omega），运行时存在且可自由复制
```

**在证明场景中的应用**：
- `0` 标记证明参数：证明只用于编译期类型检查，不参与运行时计算
- `1` 标记资源：线性资源（如文件句柄、IO 状态）
- 默认（`omega`）：普通计算值

### 3.2 Lv-00 的对应概念：证明仅编译期（Proof-CompileTime-Only）

Lv-00 天然支持"证明仅编译期"的概念，因为几何证明的本质是**约束满足**，而非运行时计算。映射关系：

| QTT 量词 | Lv-00 映射 | 含义 |
|:---|:---|:---|
| `0`（erased） | `PROOF_COLOR_GREEN` 步骤 + ghost 标记 | 纯构造性证明步骤，编译后可消除 |
| `1`（linear） | 函数块端口唯一约束 | 端口输入不可被多处复用 |
| `omega`（unrestricted） | 默认模式 | 普通几何构造节点 |

**ghost 标记的设计**：在 Lv-00 中，`ghost` 不是一个独立类型，而是附加在证明步骤或函数块上的元数据标记：

```
FUNCTION_BLOCK {
    ...
    quantifier: GHOST  // 运行时消除，仅用于类型检查
}
```

当一个函数块被标记为 `GHOST` 时：
- 其全部证明步骤在编译后的目标语言代码中不生成任何运行时代码
- 其内部约束仅用于编译期的合一检查和类型推导
- 仍然出现在证明导航器和 Web GUI 中以供审阅

### 3.3 Ghost 标记在约束图中的生命周期

```
阶段 1：构造阶段
    Ghost 块参与约束图构建，与其他块无异
    约束图包含 ghost 节点和约束边

阶段 2：规范化阶段
    Ghost 约束参与图规范化遍
    规范化后的 ghost 节点可被消除（若已满足）

阶段 3：代码生成阶段
    Ghost 节点和约束被完全剥离
    仅保留非 ghost 的几何构造

阶段 4：运行时
    Ghost 证明完全不参与运行时计算
```

---

## 4. 多后端自举架构借鉴

### 4.1 Idris 2 的编译架构

```
Idris 2 源码 (.idr)
       ↓
  [Parser + Elaborator]
       ↓
  TTImp (Typed Implicit Core) ←—— 核心中间表示
       ↓
  [Compiler Backends]
   ├── Chez Scheme
   ├── Racket
   ├── Gambit
   ├── Node.js (JavaScript)
   └── Reference (C)
```

Idris 2 的 "自举" 是指 Idris 2 编译器本身由 Idris 2 编写，编译到 Chez Scheme 然后编译自己。

### 4.2 Lv-00 的多后端映射

Lv-00 的代码生成目标与 Idris 2 有本质不同：Lv-00 的目标是**几何证明的可视化和互操作**，而非通用计算。但多后端思想值得借鉴：

| Idris 2 后端 | Lv-00 输出目标 | 说明 |
|:---|:---|:---|
| Chez Scheme | C 代码（`proof_export_html` 等） | 核心证明引擎 |
| Node.js | WebAssembly（浏览器端证明检查） | Web GUI 端证明验证 |
| Reference (C) | 约束图序列化（JSON/DOT） | 跨工具互操作 |
| — | LaTeX（`proof_export_latex`） | 学术出版 |
| — | Coq 调用序列（`proof_export_coq`） | 外部证明助手互操作 |

### 4.3 TTImp 中间表示与 Lv-00 约束图中间表示

Idris 2 使用 TTImp（Typed Implicit）作为核心 IR。Lv-00 的类比是**约束图（ConstraintGraph）**：

| TTImp 特征 | Lv-00 ConstraintGraph 特征 |
|:---|:---|
| 显式类型标注 | 节点附带的 TypeRegion |
| 隐式参数（自动推断） | 规范化遍中的坐标消解 |
| Universe 检查 | 宇宙层级检查 |
| 模式匹配编译 | 约束图的分支匹配 |
| 线性性检查 | 端口连接唯一性检查 |

Lv-00 不需要自举（C 语言不需自举），但可以从 Idris 2 的编译阶段设计中提取以下模式应用：

1. **多遍编译流水线**：约束图 → 规范化 → 合一 → 类型检查 → 代码生成
2. **中间表示稳定性**：约束图是稳定的 IR，各遍只读或局部修改
3. **后端插件化**：每种输出格式作为独立后端（HTML/LaTeX/Coq/Graphviz），共享同一约束图 IR

---

## 5. proof_mark_ghost() API 设计

### 5.1 函数声明（追加到 proof.h）

```c
/**
 * @brief 标记证明步骤为 Ghost（编译期消除）—— 借鉴 Idris 2 QTT 量词 0 标记
 *
 * 将指定的证明步骤（以及可选的所有子步骤）标记为 Ghost 模式。
 * Ghost 步骤在编译/代码生成时被完全消除，仅保留其类型检查结果。
 *
 * 这对应于 Idris 2 QTT 中的量词 0（erased）：证明仅用于编译期验证，
 * 不需要在运行时保留任何代码。
 *
 * 典型使用场景：
 *  - 几何构造中的辅助线：证明完成后可消除，只保留目标几何对象
 *  - 凸包计算的证明步骤：凸包由目标点唯一确定，证明过程可消除
 *  - 等价变换的中间步骤：变换前后的几何对象等价，中间步骤可消除
 *
 * Ghost 标记的传播规则：
 *  - 若步骤 A 依赖 Ghost 步骤 B，且 A 本身是构造性的，A 不受影响
 *  - 若步骤 A 的类型检查需要 Ghost 步骤 B 的约束，B 的约束仍参与检查
 *  - Ghost 步骤的可视化：在 ProofPanel 中以半透明/虚线样式渲染
 *
 * @param nav              证明导航器
 * @param step_id          要标记的步骤 ID
 * @param recursive        是否递归标记所有子步骤
 * @param out_affected_count 输出：受影响的步骤数量（包括递归标记的）
 * @return true 成功，false 步骤不存在或已标记
 *
 * @note 与 Lv00_PROOF_COLOR 的关系：
 *        标记为 Ghost 的步骤显示为 PROOF_COLOR_GREEN_VERIFIED（绿实框），
 *        表示该步骤是构造性的且不参与运行时计算。
 *
 * @see idris_type_driven_design.md —— QTT 量词设计参考
 * @see proof_guided_fill() —— 洞填充可生成 Ghost 建议
 */
bool proof_mark_ghost(
    ProofNavigator *nav,
    int step_id,
    bool recursive,
    int *out_affected_count
);
```

### 5.2 辅助：检查 Ghost 依赖冲突

```c
/**
 * @brief 检查 Ghost 标记是否会产生依赖冲突
 *
 * 当非 Ghost 的运行时计算依赖 Ghost（即已被消除的）步骤的约束时，
 * 会产生依赖冲突。此函数预检查冲突并提供诊断信息。
 *
 * @param nav       证明导航器
 * @param step_id   候选 Ghost 步骤 ID
 * @param out_conflicts 输出：冲突描述字符串数组（调用者需用 lv00_free 释放每个元素）
 * @param out_count 输出：冲突数量
 * @return true 存在冲突，false 无冲突（安全标记）
 */
bool proof_check_ghost_conflicts(
    const ProofNavigator *nav,
    int step_id,
    char ***out_conflicts,
    int *out_count
);
```

### 5.3 Ghost 量词枚举

```c
/**
 * @brief 量词标记（借鉴 QTT 三重量词）
 *
 * Lv-00 的量词标记附加在证明步骤/函数块上，用于控制
 * 证明与计算的分离程度。
 */
typedef enum {
    QUANTIFIER_OMEGA,   /**< 无限制（默认）：证明参与运行时计算 */
    QUANTIFIER_ONE,     /**< 线性：证明恰好使用一次，运行时存在 */
    QUANTIFIER_ZERO     /**< Ghost/Erased：证明仅编译期存在，运行时完全消除 */
} ProofQuantifier;
```

---

## 6. 实现路线图

### 6.1 第一阶段：Ghost 标记基础设施（P1-1）

- [ ] 在 `ConstraintGraph` 节点上增加 `is_ghost` 标志位
- [ ] 在 `ProofStep` 上增加 `quantifier` 字段
- [ ] 实现 `proof_mark_ghost()` 核心逻辑
- [ ] 实现 `proof_check_ghost_conflicts()` 冲突检测
- [ ] 在证明导出函数中跳过 Ghost 步骤（HTML 以虚线样式渲染）

### 6.2 第二阶段：TDD 证明循环（P1-2）

- [ ] 实现类型签名 → 命题模式的自动转换
- [ ] 实现构造骨架自动生成（从命题模式的类型自动生成未确定的约束区域）
- [ ] 实现类型引导的策略推荐（分析命题类型选择合适的证明策略）
- [ ] 实现自动搜索的终止条件检测

### 6.3 第三阶段：多后端输出增强（P1-3）

- [ ] 实现约束图 → WebAssembly 代码生成（浏览器端验证）
- [ ] 实现约束图 → JSON 序列化（Editor 互操作）
- [ ] 实现约束图 → DOT 格式增强（含 Ghost 标记和量词可视化）
- [ ] 编写多后端输出的一致性测试

### 6.4 第四阶段：线性性检查（P1-4）

- [ ] 实现端口连接的线性性分析（`QUANTIFIER_ONE` 的运行时语义）
- [ ] 实现资源使用次数的编译期计数
- [ ] 实现线性性违规的友好错误提示
- [ ] 编写线性性检查的单元测试

---

## 7. 设计决策与权衡

### 7.1 Ghost 标记与 Coq 的 Prop/Set 的区别

Lv-00 的 Ghost 标记不同于 Coq 的 Prop/Set 二分：
- Coq 的 Prop/Set 是类型层面的区分（Prop 中的证明不可被 Set 中的程序使用）
- Lv-00 的 Ghost 是步骤级别的标记（每一步可独立决定是否消除）
- 这提供了更细粒度的控制：同一条证明路径中，核心构造步骤保留，辅助证明步骤消除

### 7.2 不完全实现 QTT

Lv-00 不实现完整的 QTT 线性类型系统，仅借鉴其三重量词的思想：
- 原因 1：Lv-00 的约束图已提供比线性类型更丰富的资源追踪（端口连接关系）
- 原因 2：完整的 QTT 需要资源语义的类型检查器，约 3000+ 行核心代码
- 原因 3：Lv-00 的目标用户（几何学家）不需要理解线性类型的全部细节

### 7.3 自举目标的选择

Lv-00 的核心是 C 语言实现，短期内不自举。但保留以下未来方向：
- ProofPanel.tsx 端证明检查器：编译 Lv-00 核心检查逻辑到 WebAssembly
- 这使得用户在浏览器端即可验证证明，无需服务端
- WebAssembly 后端借鉴 Idris 2 的 Node.js 后端思路

---

## 8. 补充：Idris 2 TTImp 中间表示的深入分析

### 8.1 TTImp 的语法结构

Idris 2 的核心中间表示 TTImp（Typed Implicit）使用以下语法结构：

```
data RawImp : Type where
  IVar      : Name -> RawImp                          -- 变量引用
  IPi       : ... -> PiInfo RawImp -> RawImp -> RawImp -- 依赖函数类型
  ILam      : ... -> PiInfo RawImp -> RawImp -> RawImp -- lambda 抽象
  IApp      : RawImp -> RawImp -> RawImp               -- 函数应用
  ICase     : ... -> RawImp -> List Clause -> RawImp   -- 模式匹配
  ...
```

这个 IR 在完整类型检查前存在，隐式参数尚未填充。在 Lv-00 中，约束图（ConstraintGraph）承担了类似 TTImp 的角色——它是尚未完全验证的中间表示，在规范化、合一检查、类型检查等遍中逐步精化。

### 8.2 QTT 线性性检查的几何类比

QTT 的线性性检查（确保 `1 x` 恰好使用一次）在几何构造中有自然的类比：
- 每个几何节点在约束分析中必须被恰好使用一次（不被复制/不被遗漏）
- 线性端口：每个函数块的输入端口在约束传播中恰好被引用一次
- 违反线性性的错误类比：同一个端口被两处不同的约束作为一个对象引用，导致坐标不一致

### 8.3 自举策略的启发

Idris 2 的自举过程经历了两个阶段：
1. **Idris 1 编译 Idris 2 源码** → 得到 Idris 2 的第一个可运行版本
2. **Idris 2 编译自身** → 真正的自举，脱离对 Idris 1 的依赖

对应到 Lv-00，自举的类比是：
1. **C 实现编译 Lv-00 验证核心** → 得到验证器的第一个版本
2. **验证核心验证自身** → Lv-00 的约束图检查 function block 的正确性时，验证核心自身的规范也由同一验证核心检查

虽然 Lv-00 不需要真正的自举（因为它是 C 语言），但这个"自我指涉的验证"概念对于构建可信证明系统至关重要——例如，用 Lv-00 的验证核心来检查它自身的正确性规范的满足性。

### 8.4 Idris 2 的 Elaborator（精化器）与 Lv-00 规范化遍的对应

Idris 2 的核心精化器（Elaborator）负责将隐式的 TTImp 转换为显式的 TT（完全类型标注的核心语言）。这个过程包括：
- 隐式参数推导（implicit argument inference）
- 类型类实例搜索（type class resolution）
- 证明搜索（proof search via auto implicit）

在 Lv-00 中，对应的精化过程由多个规范化遍承担：
- **隐式坐标消解**：`normalization.h` 中的坐标传播算法——自动确定未指定的符号坐标
- **类型变量实例化**：`type_instantiate_variable()` 和 `proof_instantiate_proposition()`——将多态类型自动绑定到具体几何类型
- **约束传播**：`constraint_graph.h` 中的约束传播——从前提到结论自动推导约束

这种"精化 = 多遍规范化"的对应表明，Lv-00 的约束图处理管道在概念上是 Idris 2 精化器的几何元语言版本。

### 8.5 Idris 2 的覆盖率检查（Coverage Checking）与 Lv-00 的命题完备性

Idris 2 的模式匹配覆盖率检查确保函数定义的所有可能输入情况都被处理。在 Lv-00 的几何证明中，类似的概念是**命题覆盖性**——证明步骤是否覆盖了命题模式的所有约束骨架：

```c
/**
 * @brief 检查证明步骤是否覆盖了命题模式的所有约束
 *
 * 对照 Idris 2 的覆盖率检查，验证 proof steps 中是否对
 * proposition pattern 中的每一条约束都有对应的推理步骤。
 *
 * @param nav  证明导航器
 * @param prop 目标命题（其 pattern 中的约束作为覆盖目标）
 * @return 覆盖率 (0.0 ~ 1.0)，1.0 表示完全覆盖
 */
double proof_check_coverage(const ProofNavigator *nav, const Proposition *prop);
```

覆盖率不足时，ProofPanel 可以高亮未覆盖的约束区域（类比 IDEs 中"Missing case"的高亮）。这种反馈使用户明确知道证明还需要在哪些方面继续工作。

覆盖率检查还支持自动补全功能——系统可以在未覆盖的约束区域自动建议可能的证明策略（类似 Idris 2 的 `C-c C-s` 生成模式匹配分支），加速证明的完成速度。这一特性与 proof_guided_fill 的洞填充建议形成协同：覆盖率检查定位"哪里还没被覆盖"，洞填充建议提供"如何覆盖未覆盖区域"。

覆盖率检查的结果可以通过 `proof_search_tree` 可视化，使用户在证明搜索树的上下文中直观地理解哪些分支已被充分探索、哪些分支需要更多关注。这种覆盖率可视化在 ProofPanel 中以进度条 + 热力图的形式呈现，类似代码覆盖工具（如 gcov）但对几何证明做了领域适配。

---

## 9. 总结

Idris 2 的类型驱动开发范式与 Lv-00 的命题模式驱动构造高度同构。QTT 量词系统为 Lv-00 提供了精确控制"证明仅编译期"的理论基础——通过 Ghost 标记实现证明步骤的编译期消除，确保运行时没有任何冗余的证明开销。多后端自举架构启发 Lv-00 将约束图作为稳定中间表示，前端证明编辑和后端代码生成共享同一 IR，实现证明工作流的全链路优化。TTImp 的中间表示设计和 QTT 线性性检查进一步为 Lv-00 的类型系统和资源追踪提供了形式化的参考框架。
