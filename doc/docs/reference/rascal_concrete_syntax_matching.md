# Lv-00 参考设计：Rascal 具体语法模式匹配

> **版本**: 1.0.0  
> **日期**: 2026-05-24  
> **参考**: [Rascal](https://github.com/usethesource/rascal) —— 用于元编程和源码分析/转换的领域特定语言  
> **目标**: 借鉴 Rascal 的"具体语法模式匹配"范式，应用于 Lv-00 DSL 编译器的 AST 模式匹配和代码生成，映射到现有的 `formula_parser.h` / `formula_converter.h` 架构

---

## 目录

1. [项目概述](#1-项目概述)
2. [核心借鉴要点](#2-核心借鉴要点)
3. [Lv-00 映射方案](#3-lv-00-映射方案)
4. [实现路线图](#4-实现路线图)

---

## 1. 项目概述

### 1.1 Rascal 是什么

Rascal 是 CWI（荷兰国家数学与计算机科学研究所）和 SWAT 团队开发的元编程语言，专为源码分析、软件仓库挖掘、代码转换和领域特定语言（DSL）工程而设计。其最核心的武器是**具体语法模式匹配（Concrete Syntax Pattern Matching）**：

```rascal
// Rascal 示例：匹配一个 for 循环并提取其变量、条件和体
visit (ast) {
    case (Statement) `for (<Expr init>; <Expr cond>; <Expr update>) <Statement body>`
        => rewriteWith(body, optimize(cond))
}
```

在反引号 `` `...` `` 之间的语法片段是**目标语言的原始语法**（不是 AST 构造器），Rascal 自动将其解析为模式，解构出 `init`、`cond`、`update`、`body` 等子模式变量。

### 1.2 为什么借鉴 Rascal

Lv-00 DSL 编译器（`formula_parser.h` + `formula_converter.h`）当前通过手写递归下降解析器处理 DSL 文本到 AST 再转换到 `ConstraintGraph`。随着 DSL 语法的扩展（函数块、证明语法、选择器、导出语句），手写解析器面临维护成本非线性增长的问题。Rascal 的具体语法匹配范式为**用模式直接描述 DSL 语法转换规则**提供了优雅的解决方案。

---

## 2. 核心借鉴要点

### 2.1 具体语法模式匹配的核心思想

| Rascal 概念 | Lv-00 对应概念 | 映射说明 |
|-------------|---------------|---------|
| 具体语法模式（Concrete Syntax） | `FormulaNode` AST 节点 + DSL 文本模式 | 以 DSL 文本片段的"形状"描述模式 |
| 模式变量 `<Expr e>` | 子节点绑定到 `FormulaNode*` 变量 | 通过 `type` 字段约束节点类型 |
| `visit` 遍历器 | AST 递归遍历（已有） | `formula_node_foreach()` 风格迭代 |
| `case` 分支 | 模式匹配的 `if-switch` 分派 | 节点类型 + 子结构匹配合一 |
| 具体语法构造（Concrete Syntax Construction） | 模式→AST 的反向构造 | 用 DSL 模板文本生成 AST 子树 |
| 带洞语法（Syntax with holes） | 通过 DSL 片段 + 占位符构建 `FormulaNode` | 例如 `"point <name>(<x>, <y>)"` |

### 2.2 Rascal 模式匹配的 5 个层次

| 层次 | Rascal 表达 | Lv-00 对应 |
|------|------------|-----------|
| **类型匹配** | `(Statement) ...` | `NodeType` 枚举检查 |
| **结构匹配** | `for (<E>; <E>; <E>) <Stmt>` | 子节点数量和类型验证 |
| **深度匹配** | 嵌套语法模式 | 递归遍历子节点 |
| **条件匹配** | `when size(body) > 0` | 额外的断言条件（如 `ConstraintType` 检查） |
| **列表匹配** | `[<Expr>*]` | 可变参数子节点列表（如多边形顶点列表） |

---

## 3. Lv-00 映射方案

### 3.1 DSL 模式匹配的抽象定义

借鉴 Rascal 的具体语法模式，为 Lv-00 DSL 编译器定义模式匹配原语：

```c
/**
 * @brief DSL 模式变量 —— 对应 Rascal 的 <Type var>
 *
 * 在 DSL 文本片段中标记一个待匹配的子结构位置。
 * 模式变量在解析时被识别，匹配成功后绑定到具体的 FormulaNode 子树。
 */
typedef struct DslPatternVar {
    char *name;                 /* 变量名（如 "x", "body"） */
    NodeType expected_type;     /* 期望的节点类型（用于类型约束） */
    bool is_list;               /* 是否为列表模式（匹配零或多个节点） */
    FormulaNode *binding;       /* 匹配成功后绑定的 AST 子树 */
} DslPatternVar;

/**
 * @brief DSL 具体语法模式 —— 对应 Rascal 的 `...` 引号模式
 *
 * 将一个 DSL 文本片段编译为一个"带洞"的 AST 树，
 * 其中 DslPatternVar 标记的位置是模式的洞（holes），
 * 匹配时将被具体 AST 子树填充。
 */
typedef struct DslPattern {
    char *source;               /* DSL 模式文本（如 "point <name>(<x>, <y>)"） */
    FormulaNode *pattern_ast;   /* 解析后的模式 AST（洞标记为特殊的 NODE_PATTERN_HOLE） */
    DslPatternVar *vars;        /* 模式变量数组 */
    int var_count;              /* 模式变量数量 */
} DslPattern;
```

### 3.2 模式编译管线

将 Rascal 的模式匹配编译为 Lv-00 可执行的匹配操作：

```
DSL 模式文本                         匹配目标 AST
     │                                    │
     ▼                                    ▼
┌──────────────┐                  ┌──────────────┐
│ DslPattern   │                  │ FormulaNode  │
│ Compilation  │                  │ (待匹配的AST) │
│              │                  │              │
│ "point       │                  │ NODE_GEOM_POINT│
│  <name>(     │                  │ ├── name: "A" │
│   <x>,<y>)"  │                  │ └── coord: (10,20)│
└──────┬───────┘                  └──────┬───────┘
       │                                 │
       ▼                                 ▼
┌──────────────────────────────────────────┐
│         dsl_pattern_match()              │
│                                          │
│  1. 根节点类型检查: NODE_GEOM_POINT      │
│  2. 子节点数量检查: 2 (name + coord)     │
│  3. 子结构匹配:                          │
│     name  → .name = "A" (绑定到 var 0)   │
│     coord → (10, 20)  (绑定到 var 1,2)   │
│  4. 条件检查（若定义 when 子句）         │
│                                          │
│  返回: {matched=true, bindings=[...]}    │
└──────────────────────────────────────────┘
```

#### 核心匹配 API

```c
/**
 * @brief DSL 模式匹配结果
 */
typedef struct DslMatchResult {
    bool success;
    DslPatternVar *bindings;    /* 模式变量绑定结果数组 */
    int binding_count;
    char error_message[256];
} DslMatchResult;

/**
 * @brief 将 DSL 文本片段编译为具体语法模式
 *
 * 输入:  "point <name>(<x>, <y>)"
 * 输出:  DslPattern 结构，其中 pattern_ast 为带洞的 AST，
 *        并提取出 {name, x, y} 三个模式变量。
 *
 * 尖括号内的标识符自动识别为模式变量，非尖括号的 DSL 语法作为字面结构匹配。
 */
DslPattern *dsl_pattern_compile(const char *pattern_source);

/**
 * @brief 将具体语法模式与目标 AST 进行匹配
 *
 * 对每一个 NODE_GEOM_* 类型的 AST 节点调用此函数，
 * 以模式 pattern 的 pattern_ast 为模板，递归验证目标 AST 的同构性，
 * 并绑定模式变量。
 */
DslMatchResult dsl_pattern_match(const DslPattern *pattern,
                                 const FormulaNode *target);

/**
 * @brief 使用具体语法模板构造 AST
 *
 * 类似 Rascal 的具体语法构造（Concrete Syntax Construction）。
 * 输入为 DSL 模板文本 + 变量绑定表，输出为新构造的 FormulaNode。
 *
 * 例如:
 *   dsl_pattern_construct("point <name>(<x>, <y>)",
 *       {"name"→A_node, "x"→10_node, "y"→20_node})
 *   生成: NODE_GEOM_POINT {name: "A", coord: (10, 20)}
 */
FormulaNode *dsl_pattern_construct(const char *template_source,
                                   DslPatternVar *bindings,
                                   int binding_count);
```

### 3.3 映射到现有 formula_parser.h / formula_converter.h

| 现有组件 | 在 Rascal 模式中的角色 |
|---------|----------------------|
| `FormulaNode` / `NodeType` | 模式的"目标语言"AST（相当于 Rascal 中要被匹配的 Java/C 程序的 AST） |
| `formula_to_graph()` | 模式匹配成功后 → 约束图构造的 **rewrite 动作** |
| `graph_to_formula()` | 逆向转换（图 → 文本），用于代码生成/导出 |
| `formula_get_node_id()` / `formula_set_node_id()` | 模式变量绑定的名称→ID 映射 |
| `NODE_GEOM_POINT` / `NODE_GEOM_SEGMENT` 等类型 | 模式的类型约束（类似 Rascal 的 `(Statement)` 类型标注） |

### 3.4 DSL 编译器中的模式匹配应用场景

```
场景 1: 点构造识别
  ─────────────────────────────────────
  输入 DSL: "point A 10 20;"
  模式:     "point <name:IDENTIFIER> <x:NUMBER> <y:NUMBER>"
  匹配后:   name→"A", x→10, y→20
  动作:     geom_node_create(GEOM_POINT) + symbolic_coord_from_double()

场景 2: 函数块声明识别
  ─────────────────────────────────────
  输入 DSL: "funcblock equilateral(p1:Point, p2:Point) -> Point { ... }"
  模式:     "funcblock <name>(<params:PARAM_LIST>) -> <ret_type:TYPE> { <body> }"
  匹配后:   name→equilateral, params→[p1,p2], ret_type→Point, body→AST_block
  动作:     func_block_create() + 设置端口 + 设置内部节点

场景 3: 命题声明识别
  ─────────────────────────────────────
  输入 DSL: "proposition <name> { given: <given>; prove: <goal>; }"
  模式:     "proposition <name> { given: <given>; prove: <goal>; }"
  匹配后:   name→median_concurrency, given→三角区域, goal→concurrent约束
  动作:     proposition_create() + 设置前置/后置条件
```

### 3.5 代码生成端（模式→DSL输出）

Rascal 的"具体语法构造"同样可以应用到 Lv-00 的代码生成端。当需要将 `ConstraintGraph` 导出为 DSL 文本时：

```c
/**
 * @brief 使用模板生成 DSL 代码
 *
 * 将约束图中的一个函数块用模板渲染为 DSL 文本。
 * 模板中的 <var> 从 func_block 的元数据中填充。
 */
char *dsl_render_funcblock(const FuncBlock *fb,
                           const DslPatternVar *bindings);

// 使用示例:
// dsl_render_funcblock(fb) 输出:
//   "funcblock equilateral(p1:Point, p2:Point) -> Point {\n"
//   "    point _M = midpoint(p1, p2);\n"
//   "    ...\n"
//   "}"
```

---

## 4. 实现路线图

### 4.1 第一阶段：模式匹配核心（P2）

| 任务 | 文件 | 说明 |
|------|------|------|
| 定义 `DslPattern`、`DslPatternVar`、`DslMatchResult` | `include/lv00/dsl_pattern.h`（新文件） | 核心模式数据结构 |
| 扩展 `NodeType` 增加 `NODE_PATTERN_HOLE` | `include/lv00/formula_parser.h` | 标记 AST 中的洞位置 |
| 实现 `dsl_pattern_compile()` | `src/parser/dsl_pattern.c`（新文件） | 将 DSL 文本→带洞 AST 模式 |
| 实现 `dsl_pattern_match()` | `src/parser/dsl_pattern.c` | 模式与目标 AST 的递归匹配 |
| 实现 `dsl_pattern_construct()` | `src/parser/dsl_pattern.c` | 用模板+绑定变量构造新 AST |

**预估规模**：约 350 行 C 代码

### 4.2 第二阶段：DSL 编译器重构（P2）

| 任务 | 文件 | 说明 |
|------|------|------|
| 将 `公式→图转换` 重写为模式驱动 | `src/parser/formula_converter.c` | 用 `dsl_pattern_match` 替代部分手写解析 |
| 将 DSL 语法规则定义为模式表 | `src/parser/dsl_rules.c`（新文件） | 集中管理所有 DSL 语法模式 |
| 实现 `dsl_render_funcblock()` 等模板渲染 | `src/parser/dsl_pattern.c` | DSL→文本的代码生成 |
| 为 `GraphToFormulaResult` 增加模板模式支持 | `include/lv00/formula_converter.h` | 让导出的 DSL 文本遵循模板格式 |

**预估规模**：约 250 行 C 代码 + 重构现有 100 行

### 4.3 第三阶段：性能优化（P3+）

| 任务 | 说明 |
|------|------|
| 模式预编译缓存 | 将 `dsl_pattern_compile()` 的结果缓存在哈希表中，避免重复编译 |
| 索引加速 | 根据根节点类型建立模式索引，O(1) 定位候选模式 |
| 增量匹配 | 当约束图局部更新时，仅重匹配受影响的模式 |

---

## 附录 A：Rascal 模式匹配与 Lv-00 DSL 编译器的对照

| Rascal 代码 | Lv-00 对应实现 |
|------------|---------------|
| `visit(ast) { }` | `formula_node_foreach(root, callback)` 遍历 |
| `case (Statement) \`for (...)\`` | `dsl_pattern_match(pattern, node)` 检查 |
| `<Expr e>` | `DslPatternVar { .name="e", .expected_type=NODE_* }` |
| `when ...` | 匹配后的附加断言（如 `node->coord_count == 2`） |
| `=> newExpression` | 模式匹配成功后执行的重写动作（调用 `formula_to_graph` 等） |
| `\`return <Expr value>;\`` (构造) | `dsl_pattern_construct("return <value>;", bindings)` |

---

## 附录 B：DSL 模式预编译表示例

```c
/* dsl_rules.c —— DSL 语法模式表（Rascal 风格） */

static DslPattern dsl_grammar_rules[] = {
    /* 点构造 */
    { "point  <name>(<x>, <y>)",         .target=NODE_GEOM_POINT    },
    { "point  <name> <x> <y>",           .target=NODE_GEOM_POINT    }, /* GCLC兼容 */
    
    /* 线段构造 */
    { "segment <name>(<A>, <B>)",         .target=NODE_GEOM_SEGMENT  },
    
    /* 函数块 */
    { "funcblock <name>(<params>)-><ret> { <body> }",
                                          .target=NODE_GEOM_FUNCTION_BLOCK },
    
    /* 证明 */
    { "prove <name> using strategy=<strat>",
                                          .target=NODE_PROOF         },
    
    /* 导出 (可变参数) */
    { "export_<fmt> \"<filename>\"",      .target=NODE_EXPORT        },
};
```

---

> **文档结束**  
> 本文档详述了 Rascal 的"具体语法模式匹配"范式如何应用于 Lv-00 DSL 编译器的 AST 模式匹配和代码生成。核心结论：通过引入 `DslPattern` / `DslMatchResult` / `dsl_pattern_match()` 三层抽象，可以将 `formula_parser.h` 和 `formula_converter.h` 的手写解析逻辑逐步迁移为声明式的模式驱动架构，降低 DSL 语法扩展的维护成本。
