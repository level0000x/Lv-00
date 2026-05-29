# Lv-00 参考设计：K Framework Cell 嵌套语义

> **版本**: 1.0.0  
> **日期**: 2026-05-24  
> **参考**: [K Framework](https://github.com/runtimeverification/k) —— 基于重写逻辑的可执行语义框架  
> **目标**: 借鉴 K Framework 的 Cell 嵌套语义上下文和语义规则热替换机制，映射到 Lv-00 的 `constraint_graph.h` 分层约束组织和公理包切换架构

---

## 目录

1. [项目概述](#1-项目概述)
2. [核心借鉴要点](#2-核心借鉴要点)
3. [Lv-00 映射方案](#3-lv-00-映射方案)
4. [实现路线图](#4-实现路线图)

---

## 1. 项目概述

### 1.1 K Framework 是什么

K Framework 是 Runtime Verification 公司开发的基于重写逻辑（Rewriting Logic）的可执行语义框架。其核心理念是：**一门编程语言的完整形式语义可以用一组重写规则来定义，而这些规则天然可以被执行**。K 的两大核心设计为：

1. **Cell 嵌套语义上下文（Configuration / Cell Nesting）**：K 将程序的状态建模为一棵嵌套的 Cell 树。例如一个简单命令式语言的状态配置可表示为：

```
<k> while (x > 0) { x = x - 1; } </k>
<env> x |-> 2 </env>
<store> 1 |-> 3 </store>
```

每个 `<cell>` 标签定义一个语义域（如 `<k>` 表示计算续体、`<env>` 表示变量环境、`<store>` 表示内存），Cell 可以任意嵌套，形成分层语义上下文。

2. **语义规则热替换（Rule Hot-Swapping）**：K 的语义规则本身就是一等数据。可以在运行时加载不同的语义定义文件（.k），实现语义规则集的动态切换。这在多语言互操作、语言扩展、语义变体实验中极为有用。

### 1.2 为什么借鉴 K Framework

Lv-00 的约束图已天然具备分层上下文的结构——函数块嵌套、命名空间深度、端口多态。但当前缺乏一种将"语义上下文"显式化的机制。K Framework 的 Cell 嵌套模型为 Lv-00 公理包的**分层组织**和**语义规则热替换**提供了可操作的参考路径。

---

## 2. 核心借鉴要点

### 2.1 Cell 嵌套语义上下文 → 约束图分层组织

| K Framework 概念 | Lv-00 对应概念 | 映射说明 |
|------------------|---------------|---------|
| `<T>`（顶层配置） | `ConstraintGraph` 自身 | 全局约束空间 |
| `<k>`（计算续体） | `ProofStep` 序列 / `ProofNavigator` | 证明步骤序列即计算续体 |
| `<env>`（变量环境） | 约束图中的 `GeomNode.id` → 名称映射 | 由 `formula_get_node_id()` 提供 |
| `<store>`（内存） | `symbolic_coords[]` + 解缓存 | 点坐标存储即为几何"内存" |
| `<thread>`（线程 Cell） | `FuncBlock` 实例（`parent_block_id` + `namespace_depth`） | 每个函数块即一个独立的执行上下文 |
| Cell 嵌套结构 | `FuncBlock` 的内层节点 + `namespace_depth` 字段 | `GEOM_FUNCTION_BLOCK` 的 `data.func_block.internal_node_ids[]` 定义嵌套 |
| Cell 的一等访问 | `GeomNode.id` 的全局可见性 | 任何节点可通过 ID 在 O(1) 时间内访问 |

关键洞察：K 的 Cell 嵌套本质上是在**状态空间中定义分层的作用域边界**。Lv-00 的 `FuncBlock` 嵌套和内层节点天然实现了这一模型——`parent_block_id` 和 `namespace_depth` 字段已经提供了与 Cell 嵌套等价的层次结构信息。

### 2.2 语义规则热替换 → 公理包切换

| K Framework 概念 | Lv-00 对应概念 | 映射说明 |
|------------------|---------------|---------|
| 语义规则文件 (.k) | 公理包（`axiom_pkg.h`） | 每个公理包是一组几何定理/引理 |
| `.k` 文件加载 | `axiom_pkg_load()` | 运行时加载公理包 |
| 语义规则优先级 | `ProofStrategyType` 枚举 | 不同策略引用不同公理子集 |
| 多语义变体切换 | `scheduler_select_backend()` 的自动路由 | 图特征分析决定使用哪组公理 |
| 规则条件 (side condition) | `ConstraintType` + `precondition_region_ids` | 约束类型和前置条件即为规则侧条件 |

K 的语义规则热替换的核心思想是：**规则集是可组合、可切换的一等模块**。Lv-00 的公理包已经具备了这一潜力——每个公理包是一组独立可加载的几何定理，`ProofMultiStrategy` 可以选择性地激活特定公理子集。当前缺失的是公理包之间的**形式化依赖管理**和**冲突检测**，这正是 K 的语义模块系统可以提供指导的地方。

---

## 3. Lv-00 映射方案

### 3.1 约束图的分层语义上下文

将 K 的 Cell 嵌套模型应用到 Lv-00，可将约束图的形式化语义上下文显式建模为三层结构：

```
ConstraintGraph (顶层配置 <T>)
├── global_constraints[]        —— 全局约束层 (<globals>)
│   ├── INCIDENCE / BETWEENNESS 等几何约束
│   └── 作用域: 所有函数块可见
│
├── FuncBlock_1 (parent_block_id=0, namespace_depth=0)  —— 模块层
│   ├── internal_nodes[]        —— 内部构造状态
│   ├── input_ports[]           —— 输入形式参数
│   ├── output_ports[]          —— 输出计算结果
│   ├── precondition_region_ids[] —— 前置条件
│   └── determinism_state       —— 确定性验证状态
│       ├── FuncBlock_1a        —— 嵌套函数块（namespace_depth=1）
│       │   ├── internal_nodes[]
│       │   └── ...
│       └── FuncBlock_1b
│
├── ProofNavigator             —— 证明层
│   ├── steps[]                 —— 证明步骤（当前续体 <k>）
│   ├── current_step            —— 执行位置
│   └── strategy_note           —— 策略标记
│
└── AxiomPackage[]              —— 公理库层
    ├── loaded_packages[]       —— 已加载公理包
    └── active_axiom_set        —— 当前激活的公理子集
```

#### 对应 C 结构扩展（示意）

```c
/**
 * @brief 语义上下文层类型（按 K Cell 模型分层）
 */
typedef enum {
    CTX_GLOBAL,          /* 全局约束层 —— 对应 <globals> cell */
    CTX_MODULE,          /* 函数块层 —— 对应模块 cell */
    CTX_PROOF,           /* 证明层 —— 对应 <proof> cell */
    CTX_AXIOM            /* 公理库层 —— 对应 <axioms> cell */
} SemanticContextLayer;

/**
 * @brief 分层语义上下文描述符
 *
 * 显式表达 K Framework 风格的语义层，提供：
 * - layer_id: 上下文层的唯一标识
 * - parent_layer_id: 嵌套父层 ID（-1 表示顶层）
 * - visibility_mask: 该层可访问的资源位掩码
 * - active_axiom_pkg_ids: 该层激活的公理包集合
 */
typedef struct SemanticLayer {
    SemanticContextLayer type;
    int layer_id;
    int parent_layer_id;
    uint64_t visibility_mask;
    int *active_axiom_pkg_ids;
    int axiom_pkg_count;
    int namespace_depth;         /* 与 FuncBlock.namespace_depth 同步 */
} SemanticLayer;
```

### 3.2 公理包热替换机制

参考 K 的语义规则热替换，Lv-00 可以形式化为以下公理包生命周期：

```
公理包生命周期（K-style）:
  1. axiom_pkg_register()     —— 注册公理包到 Lv-00 引擎
  2. axiom_pkg_load()          —— 将公理包约束加载到 ConstraintGraph
  3. axiom_pkg_activate()      —— 激活公理包到当前 SemanticLayer
  4. axiom_pkg_deactivate()    —— 停用公理包（从活跃集移除，约束保留但标记为 inactive）
  5. axiom_pkg_hotswap()       —— 热替换：停用旧包 + 激活新包，保留已证明步骤
  6. axiom_pkg_validate()      —— 验证新旧公理包的一致性（无矛盾定理）
```

**热替换的执行保证：**

```c
/**
 * @brief 热替换公理包（K Framework 风格）
 *
 * 在不重置证明状态的前提下，将活跃公理集从 old_pkg_ids 替换为 new_pkg_ids。
 * 只有在新旧公理集"可兼容"时才允许热替换，否则返回错误并保持不变。
 *
 * 兼容性条件：
 * 1. 已通过的公理包切换验证检查 (axiom_pkg_validate)
 * 2. 当前证明步骤不依赖即将停用的公理
 * 3. 新旧公理包覆盖的约束类型存在交叠（否则无意义）
 *
 * @return true 表示热替换成功，old→new 原子切换
 */
bool axiom_pkg_hotswap(const int *old_pkg_ids, int old_count,
                       const int *new_pkg_ids, int new_count,
                       ConstraintGraph *graph);
```

### 3.3 分层约束命名空间

利用 `GeomNode.parent_block_id` 和 `ConstraintGraph` 的哈希索引，可以实现分层命名空间查找：

```c
/**
 * @brief 在指定语义层内查找节点（按 Cell 风格分层作用域）
 *
 * 查找顺序（由内向外）：
 * 1. 当前函数块的 internal_nodes（最内层 cell）
 * 2. 递归父块的 internal_nodes（向上遍历 parent_block_id）
 * 3. 全局约束层（顶层 cell）
 *
 * 这模拟了 K 的 Cell 嵌套作用域解析规则。
 */
GeomNode *semantic_layer_lookup_node(ConstraintGraph *graph,
                                     int start_block_id,
                                     const char *node_name);
```

---

## 4. 实现路线图

### 4.1 第一阶段：语义上下文层建模（P2）

| 任务 | 文件 | 说明 |
|------|------|------|
| 定义 `SemanticLayer` 和 `SemanticContextLayer` | `include/lv00/semantic_layer.h`（新文件） | 分层语义上下文的核心数据结构 |
| 实现 `semantic_layer_create/destroy` | `src/semantic_layer.c`（新文件） | 创建/销毁语义层 |
| 为 `ConstraintGraph` 添加 `SemanticLayer *layers[]` 字段 | `constraint_graph.h` | 可选扩展，允许图关联多语义层 |
| 实现 `semantic_layer_lookup_node()` | `src/semantic_layer.c` | 按层作用域查找节点 |

**预估规模**：约 200 行 C 代码

### 4.2 第二阶段：公理包热替换（P2）

| 任务 | 文件 | 说明 |
|------|------|------|
| 实现 `axiom_pkg_hotswap()` | `src/axiom_pkg.c` | 热替换核心逻辑 |
| 实现 `axiom_pkg_validate()` | `src/axiom_pkg.c` | 新旧公理包兼容性验证 |
| 在 `engine_scheduler.cpp` 中集成热替换触发 | `src/engine_scheduler.cpp` | 当后端路由改变时自动触发公理包切换 |
| 添加公理包版本号管理 | `include/lv00/axiom_pkg.h` | 支持向后兼容性标记 |

**预估规模**：约 300 行 C 代码

### 4.3 第三阶段：语义规则的一等化（P3+）

| 任务 | 说明 |
|------|------|
| 将每个几何定理表示为独立可引用的语义规则对象 | 类似 K 的 `rule` 实体 |
| 支持规则的前/后条件检查（类似 K 的 `requires` / `ensures`） | 利用现有 `precondition_region_ids` |
| 实现多语义规则集的合一（类似 K 的 `configuration abstraction`） | 与现有 `proof_unify()` 集成 |

---

## 附录 A：K Cell 配置与 Lv-00 约束图的对应示例

### K 风格配置（假想的 Lv-00 语言语义描述）

```
configuration <T>
    <global>
        <nodes> ... </nodes>
        <constraints> ... </constraints>
    </global>
    <funcblock multiplicity="*">
        <internalNodes> ... </internalNodes>
        <inputPorts> ... </inputPorts>
        <outputPorts> ... </outputPorts>
        <funcblock multiplicity="*"> ... </funcblock>
    </funcblock>
    <proof>
        <k> $PGM:ProofSteps </k>
        <strategy> area_method </strategy>
    </proof>
    <axioms>
        <axiomPkg multiplicity="*">
            <name> ... </name>
            <rules> ... </rules>
        </axiomPkg>
    </axioms>
</T>
```

### 对应 Lv-00 C 结构

```c
// <global> cell → ConstraintGraph
ConstraintGraph *graph = constraint_graph_create();

// <funcblock> cell → FuncBlock 嵌套
FuncBlock *fb = func_block_create(id);
func_block_set_internal_nodes(fb, internals, count);

// <proof>/<k> → ProofNavigator
ProofNavigator *nav = proof_navigator_create(prop, engine);

// <axioms> → 公理包
AxiomPackage *pkg = axiom_pkg_load("euclidean_v1.ap");
axiom_pkg_activate(pkg->id);
```

---

## 附录 B：Cell 嵌套与 namespace_depth 的映射关系

| K Cell 嵌套示例 | `namespace_depth` | Lv-00 结构 |
|----------------|-------------------|-----------|
| `<T>` | `0` | `ConstraintGraph` |
| `<funcblock name="f">` | `0` | `FuncBlock`（顶层函数块） |
| `<funcblock name="g">`（在 f 内部） | `1` | 嵌套 `FuncBlock`，`parent_block_id = f.id` |
| `<funcblock name="h">`（在 g 内部） | `2` | 更深层嵌套 |

K 的 Cell 可嵌套性和 Lv-00 的 `FuncBlock` 可嵌套性本质上是等价的——都是**在状态空间中创建新的作用域边界**，区别仅在于 K 用 XML 标签标记、Lv-00 用 `parent_block_id` 整型指针表达。

---

> **文档结束**  
> 本文档详述了 K Framework 的 Cell 嵌套语义上下文如何映射到 Lv-00 的约束图分层组织，以及语义规则热替换如何对应公理包切换机制。核心结论：Lv-00 的 FuncBlock 嵌套、namespace_depth 字段、公理包加载体系已具备 K Framework 式的分层语义潜力，通过引入显式的 SemanticLayer 抽象和热替换 API 即可补齐。
