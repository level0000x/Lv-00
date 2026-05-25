# Langium DSL 语言工程框架参考文档

> 参考项目：Eclipse Langium（[github.com/eclipse-langium/langium](https://github.com/eclipse-langium/langium)）
> 版本：主分支（截至 2026-05-24）
> 许可证：MIT
> 编制日期：2026-05-24

---

## 目录

1. [项目概述](#1-项目概述)
2. [核心借鉴点](#2-核心借鉴点)
3. [Lv-00 映射方案](#3-lv-00-映射方案)
4. [实现路线图](#4-实现路线图)
5. [附录](#5-附录)

---

## 1. 项目概述

### 1.1 简介

Langium 是 Eclipse 基金会旗下的开源语言工程框架，使用 TypeScript 编写，专为构建领域特定语言（DSL）及其配套工具链而设计。它被定位为 Xtext 框架（Java/Eclipse 生态中经典的 DSL 开发框架）在 Web 和 VS Code 时代的"精神继承者"，继承了 Xtext 的声明式语法驱动理念，同时拥抱了现代前端和 Node.js 生态。

Langium 的核心理念可概括为一句话：**一份语法声明，生成全栈语言工具**。开发者只需编写一份 `.langium` 语法定义文件，Langium CLI 即可自动生成：

- 基于 Chevrotain 的 LL(k) 解析器（Parser）
- 完整的 Language Server Protocol（LSP）服务器，支持代码补全、悬停提示、跳转定义、诊断（错误/警告）、文档符号、代码格式化等功能
- 类型检查器和验证器基础设施
- 代码生成器骨架
- VS Code 扩展项目模板

Langium 目前已发布 v3.x 系列版本，API 趋于稳定，社区活跃度较高，被多个商业和开源 DSL 项目采用。

### 1.2 技术栈

| 层级 | 技术选型 | 说明 |
|------|----------|------|
| 核心语言 | TypeScript | 类型安全，适合构建复杂编译器基础设施 |
| 运行时 | Node.js / 浏览器 | 同构设计，一套代码同时支持服务器端和 Web Worker |
| 解析器引擎 | Chevrotain | 高性能 LL(k) 解析器生成器，内置自动错误恢复 |
| LSP 协议 | vscode-languageserver (LSP 3.17) | 标准语言服务器协议，兼容所有 LSP 客户端 |
| 依赖注入 | 自研 IoC 容器 | 模块化解耦，各组件可独立替换 |
| 构建工具 | Langium CLI (`langium generate`) | 语法文件 → 代码生成，项目脚手架 |
| 测试框架 | Vitest | 单元测试和集成测试 |

### 1.3 许可证

Langium 采用 **MIT 许可证**，与 Lv-00 项目的许可证完全兼容。这意味着 Lv-00 可以直接引用 Langium 的设计理念和 API 风格，甚至在需要时直接复用其部分代码（如 Chevrotain 语法定义模式、LSP 处理器接口设计），无任何法律风险。

### 1.4 与 Lv-00 的关系定位

Lv-00 当前拥有一个手写递归下降的 DSL 解析器（位于 `formula_parser.h` / `formula_parser.c` 及相关模块），其语法涵盖几何约束、代数方程、公理声明等域概念。Langium 的参考价值在于：它展示了一套从语法声明到 LSP 集成的**完整方法论**，Lv-00 可以借鉴这一方法论来提升 DSL 编辑体验、系统化解析器的错误处理能力，并建立统一的语言服务层。

---

## 2. 核心借鉴点

### 2.1 借鉴点总览

| 编号 | 借鉴点 | Langium 做法 | Lv-00 对应 | 优先级 |
|------|--------|-------------|-----------|--------|
| A | **语法声明即全栈** | 一份 `.langium` 语法文件自动生成 Parser、LSP、类型检查器、代码生成器 | `formula_parser.h` 可参考声明式方法，将语法规则集中管理 | 高 |
| B | **LSP 深度集成** | 内置完整 LSP 支持（补全、悬停、跳转、诊断、符号） | Web GUI 可引入 LSP-like 的 DSL 编辑体验 | 高 |
| C | **依赖注入架构** | IoC 容器模式，允许灵活替换各层组件 | 对应 Lv-00 的"公理中立"模块替换思想 | 中 |
| D | **Chevrotain 解析器生成** | 基于 Chevrotain 的 LL(k) 解析，内置错误恢复 | 手写递归下降解析器可参考其错误处理和恢复机制 | 中 |
| E | **多语言项目支持** | 一个 workspace 内多个 DSL 共存，互操作 | 多个 `.lvz` 公理包的共存和互操作 | 中 |

### 2.2 借鉴点 A：语法声明即全栈

**Langium 做法**

Langium 的核心工作流以 `.langium` 语法文件为单一事实来源（Single Source of Truth）。语法文件采用类 EBNF 的简洁语法，同时支持在规则上标注类型信息、验证约束和代码生成模板。示例如下：

```langium
grammar HelloWorld

entry Model:
    (persons+=Person | greetings+=Greeting)*;

Person:
    'person' name=ID;

Greeting:
    'Hello' person=[Person] '!';

hidden terminal WS: /\s+/;
terminal ID: /[_a-zA-Z][\w_]*/;
```

运行 `langium generate` 后，Langium 自动生成：

1. **AST 类型定义**（TypeScript 接口，基于语法规则推导）
2. **解析器**（Chevrotain 语法配置 + Token 定义 + 解析逻辑）
3. **LSP 服务骨架**（补全提供器、悬停提供器、跳转定义提供器、诊断提供器）
4. **代码生成器接口**（`GeneratorNode` 抽象，支持链式生成）
5. **序列化器**（JSON 导入/导出 AST 的能力）

**Lv-00 对应**

Lv-00 当前的 DSL 解析器采用手写递归下降方式，语法规则分散在 `formula_parser.h`、`tokenizer.h` 等多个文件中。借鉴 Langium 的声明式方法论后，可以：

- 将 Lv-00 DSL 的语法规则整理为一份独立的语法规范文件（如 `lv_dsl.langium` 原型或等价的 YAML/JSON Schema 格式）
- 基于该规范自动生成：
  - C 语言解析器骨架（token 定义、语法规则函数签名、AST 节点结构体）
  - Web GUI 的语法高亮规则（TextMate 语法或 Monaco 语言定义）
  - 诊断规则框架（每个语法规则的验证钩子）
- 保持手写解析器的性能优势，同时在**架构层面**引入声明式的可维护性

**关键收益**：语法修改时只需更改一处声明，所有下游工具同步更新，避免手工同步 `formula_parser.c`、`tokenizer.c`、Web 高亮规则、自动补全规则之间的不一致。

---

### 2.3 借鉴点 B：LSP 深度集成

**Langium 做法**

Langium 的 LSP 实现是其最具参考价值的部分。它不仅仅是"支持 LSP"，而是将 LSP 协议**内化**为框架的一等公民：

```
LangiumDocument
  │
  ├── parse()        → AST
  ├── validate()     → Diagnostic[]
  ├── scope()        → ScopeProvider（跨文件引用解析）
  └── lsp/
       ├── CompletionProvider     （补全：语法驱动 + 作用域感知）
       ├── HoverProvider          （悬停：类型信息、文档注释）
       ├── GoToDefinitionProvider （跳转：跨引用解析）
       ├── ReferencesProvider     （查找引用）
       ├── FoldingProvider        （代码折叠）
       ├── DocumentSymbolProvider （文档符号大纲）
       ├── Formatter              （代码格式化）
       └── RenameProvider         （重命名重构）
```

每个 LSP 处理器都是一个独立的服务类，通过 DI 容器组装，可独立替换或扩展。这使得 Langium 的 LSP 语义极其丰富：补全能感知作用域（只提示当前上下文可见的符号），悬停能显示类型推导结果，诊断能跨文件传播错误。

**Lv-00 对应**

Lv-00 的 Web GUI（`lv_gui.html`）当前使用基础的 `<textarea>` 或简单的代码编辑器组件，缺少语法感知的编辑体验。借鉴 Langium 的 LSP 设计后，Lv-00 可以：

- 在 Web GUI 中集成 Monaco Editor（VS Code 内核），并为其注册自定义语言模式
- 以 Web Worker 形式运行 DSL 解析器，通过 LSP 消息协议与编辑器通信
- 实现以下编辑体验增强：

| LSP 功能 | Lv-00 具体效果 | 实现复杂度 |
|----------|---------------|-----------|
| **代码补全** | 输入 `axiom` 后自动提示所有可用公理名和参数签名 | 中 |
| **悬停提示** | 鼠标悬停在函数块名上，显示其输入/输出/前置条件 | 低 |
| **跳转定义** | 从公理引用跳转到 `.lvz` 文件中的声明处 | 中 |
| **实时诊断** | 红色波浪线标记语法错误、类型不匹配、未定义引用 | 高 |
| **文档符号** | 侧边栏大纲显示当前文件的公理/函数块/约束层级 | 低 |
| **代码折叠** | 折叠公理体、函数块体、大型约束表达式 | 低 |

**关键收益**：从"文本编辑器 + 手动编译"升级为"智能 IDE 体验"，大幅降低 DSL 编写门槛，减少语法错误。

---

### 2.4 借鉴点 C：依赖注入架构

**Langium 做法**

Langium 实现了自己的轻量级 IoC（Inversion of Control）容器，核心原则是：

```typescript
// Langium DI 容器示例（简化）
const services = createHelloWorldServices();

// 各层组件通过 DI 注册和解析
services.index.AstNodeLocator;          // AST 节点定位器
services.references.ScopeProvider;      // 作用域提供器
services.validation.ValidationRegistry; // 验证规则注册表
services.lsp.CompletionProvider;        // 补全提供器
```

这种架构的核心理念是：**任何组件都可以被替换**。例如，如果要改变作用域解析逻辑（如从"词法作用域"切换到"依赖序作用域"），只需：

1. 实现 `ScopeProvider` 接口
2. 在 DI 容器中覆盖注册

无需修改补全、跳转、诊断等其他依赖作用域提供器的组件——它们通过 DI 自动获得新实现。

**Lv-00 对应**

Lv-00 的核心设计哲学之一是"公理中立"：解析器和求解器不预设任何特定公理系统的语义，公理系统作为一个**可替换模块**注入。这一思想与 Langium 的 DI 架构高度契合。

映射到 Lv-00 中：

| Langium DI 概念 | Lv-00 对应模块 | 可替换性示例 |
|-----------------|---------------|-------------|
| `ScopeProvider` | 公理包加载器 / 符号表 | 切换 `.lvz` 公理包时，符号可见性随之变化 |
| `ValidationRegistry` | 类型检查规则集 | 不同公理系统（欧几里得几何 vs 射影几何）有不同的类型规则 |
| `Parser` | `formula_parser` 模块 | 可替换为 LL(k) 表驱动解析器或 PEG 解析器 |
| `Linker` | 跨文件引用解析器 | 公理包间的依赖引用解析 |
| `IndexManager` | 公理索引数据库 | 全局公理注册表和查找服务 |

**关键收益**：强化 Lv-00 的模块化设计，使公理包替换、解析器切换、验证规则定制等操作更加系统化和规范化。

---

### 2.5 借鉴点 D：Chevrotain 解析器生成与错误恢复

**Langium 做法**

Langium 底层使用 [Chevrotain](https://chevrotain.io/) 作为解析器引擎。Chevrotain 的核心优势在于：

1. **LL(k) 解析**：支持任意 k 级别的前瞻（lookahead），足以处理大多数 DSL 语法
2. **自动错误恢复**：解析器遇到语法错误时，不立即崩溃，而是尝试跳过错误 token 继续解析，从而在一次解析中报告多个错误
3. **嵌入式语法**：在 JavaScript/TypeScript 代码中直接定义语法规则，支持条件分支、动态规则等高级模式
4. **语法诊断**：自动检测左递归、歧义等语法问题

错误恢复的具体机制（简化）：

```
输入: "point A = (1, "   ← 缺少右括号
        ─────────┬─────
                 └── 解析失败
Chevrotain: 尝试"单 token 删除"→ 成功匹配右括号 → 继续解析后续规则
            记录 Diagnostic{ severity: ERROR, message: "缺少 ')'" }
```

**Lv-00 对应**

Lv-00 当前的 `formula_parser` 采用手写递归下降解析器，错误处理通常只在第一个语法错误处中止，返回简单的错误信息。借鉴 Chevrotain 的错误恢复策略后，可以：

1. **实现 Panic Mode 恢复**：遇到错误时，跳过输入 token 直到找到同步 token（如分号 `;`、换行符、`}` 等），然后继续解析
2. **实现单 Token 插入/删除**：尝试小范围的"修复"（假设用户遗漏或多余了一个 token），继续解析并验证修复成本
3. **累积诊断**：在一次解析中收集所有语法错误，而非在第一个错误处停止

Lv-00 手写解析器的错误恢复可参考伪代码：

```c
// Lv-00 formula_parser 错误恢复参考（伪代码）
typedef struct {
    Token* tokens;
    int pos;
    DiagnosticList* diagnostics;  // 累积诊断
} ParserState;

bool parse_expression(ParserState* s, ASTNode** out) {
    ASTNode* left;
    if (!parse_term(s, &left)) {
        // 错误恢复：跳过直到同步 token
        skip_to_sync(s, TOKEN_SEMICOLON, TOKEN_NEWLINE);
        return false;  // 继续解析，不崩溃
    }
    // ... 继续解析
}
```

**关键收益**：用户在一次编译中看到所有语法错误，而非逐个修复 → 重新编译的循环，显著提升 DSL 编写效率。

---

### 2.6 借鉴点 E：多语言项目支持

**Langium 做法**

Langium 原生支持在一个 workspace 中同时存在多个 DSL 定义，每个 DSL 有独立的语法、验证规则和代码生成器，但共享 LSP 基础设施。用户在一个文件中引用另一个 DSL 的符号时，Langium 的 ScopeProvider 能跨语言解析引用。

典型场景：一个项目同时包含 `.entity`（实体定义 DSL）、`.workflow`（工作流 DSL）和 `.ui`（UI 布局 DSL），`.workflow` 中的活动引用 `.entity` 中定义的数据类型。

**Lv-00 对应**

Lv-00 的公理系统天然具有"多语言"特征：每个 `.lvz` 公理包定义了一套领域概念（如欧几里得几何公理、仿射几何公理、代数恒等式公理），它们可以在同一个工程中共存和互操作。

| Langium 多语言概念 | Lv-00 多公理包对应 |
|-------------------|-------------------|
| 独立 DSL 语法文件 | 独立 `.lvz` 公理包文件 |
| 跨 DSL 符号引用 | 公理包 A 中的函数块引用公理包 B 中的公理 |
| 共享 LSP 基础设施 | 共享解析器和 LSP 服务 |
| 命名空间隔离 | 公理包的命名空间前缀（如 `euclid::point`） |
| 交叉引用验证 | 跨公理包的类型兼容性检查 |

**关键收益**：为 Lv-00 建立正式的"多公理包工程模型"，支持公理包的模块化发布、版本管理和依赖解析。

---

## 3. Lv-00 映射方案

### 3.1 总体方案：Lv-00 DSL 编译器的 LSP 化

借鉴 Langium 的方法论，Lv-00 的 DSL 编译管线将从当前的"命令行编译"模式演进为"LSP 驱动的交互式编辑"模式。核心变化如下：

```
当前架构:
┌─────────────┐     ┌──────────────┐     ┌──────────────┐
│ .lvz 源文件  │ ──▶ │ formula_parser │ ──▶ │ C 编译/执行   │
└─────────────┘     └──────────────┘     └──────────────┘
                         (一次性，报第一个错误后停止)

目标架构:
┌─────────────┐     ┌──────────────┐     ┌──────────────┐
│ Monaco       │     │  LSP Server  │     │  C 编译/执行  │
│ Editor      │◀───▶│  (Web Worker) │ ──▶ │  (后端)      │
│ (Web GUI)   │     │              │     │              │
└─────────────┘     │ - Parser     │     └──────────────┘
                     │ - Validator  │
                     │ - Completer  │
                     │ - Hover      │
                     │ - Diagnostics│
                     └──────────────┘
```

### 3.2 语法规范层映射

用 Langium 风格的语法定义来描述 Lv-00 几何 DSL 的核心语法：

```langium
// Lv-00 DSL 语法定义（Langium 风格，仅供参考）
grammar Lv00GeometricDSL

entry LvzFile:
    (axioms+=AxiomDeclaration | funcBlocks+=FuncBlockDefinition | imports+=ImportStatement)*;

AxiomDeclaration:
    'axiom' name=ID '(' params+=ParameterDecl (',' params+=ParameterDecl)* ')' '{'
        (premises+=Premise ';')*
        conclusion=Conclusion
    '}';

FuncBlockDefinition:
    'funcblock' name=ID '(' inputs+=TypedParam (',' inputs+=TypedParam)* ')'
    ':' outputs+=TypedParam (',' outputs+=TypedParam)* '{'
        (steps+=ConstraintStep)*
    '}';

ConstraintStep:
    'let' var=ID '=' expr=Expression
    | 'constrain' constraint=ConstraintExpression
    | 'solve' target=ID;

Expression:
    Literal | VariableRef | BinaryOp | UnaryOp | FuncCall;

// JSON 序列化
Expression returns json:
    Literal | VariableRef | BinaryOp | UnaryOp | FuncCall;

// 坐标类型
PointLiteral:
    '(' x=NUMBER ',' y=NUMBER ')';

// 诊断标注
Expression:
    Literal | VariableRef | BinaryOp | UnaryOp | FuncCall;

// ... 更多语法规则
```

**映射要点**：

- `entry LvzFile` 对应 Lv-00 的一个 `.lvz` 源文件
- `AxiomDeclaration` 对应 `formula_parser` 中的公理解析规则
- `FuncBlockDefinition` 对应预设函数块的 DSL 描述（若采用 DSL 而非 C 常量定义）
- `ConstraintStep` 对应 Lv-00 约束图中的求解步骤
- Lv-00 实际使用 C 语言实现解析器，此语法定义主要起到**规范文档**和**代码生成模板**的作用

### 3.3 LSP 功能映射表

| LSP 功能 | Langium 实现入口 | Lv-00 实现方案 | 数据来源 |
|----------|-----------------|---------------|---------|
| 代码补全 | `DefaultCompletionProvider` | Web GUI 中调用解析器获取当前作用域符号，返回补全列表 | 解析后的符号表 + 公理索引 |
| 悬停提示 | `DefaultHoverProvider` | 解析光标位置的 AST 节点，提取类型和文档信息 | AST 节点属性 + 内嵌文档注释 |
| 跳转定义 | `DefaultGoToDefinitionProvider` | 解析引用 token，查询公理索引，返回目标位置 | 符号表 + 公理索引 |
| 实时诊断 | `DefaultDocumentValidator` | 随输入增量解析，收集语法和语义错误 | 解析器输出 + 类型检查器 |
| 文档符号 | `DefaultDocumentSymbolProvider` | 解析文件获取顶层声明列表 | AST 顶层节点 |
| 查找引用 | `DefaultReferencesProvider` | 在公理索引中反向查找所有引用 | 公理索引 |
| 代码格式化 | `DefaultFormatter` | 基于语法规则的缩进和换行 | 语法规则元信息 |
| 语法高亮 | TextMate 语法 | Monaco Editor 的 Monarch tokenizer | token 类型定义 |

### 3.4 代码示例：Web Worker LSP 服务

以下展示 Lv-00 如何在 Web GUI 中集成一个 LSP-like 的解析服务（概念代码）：

```c
// lsp_bridge.h -- Lv-00 LSP 桥接层（C 侧接口）
#ifndef LV_LSP_BRIDGE_H
#define LV_LSP_BRIDGE_H

// LSP 消息类型枚举
typedef enum {
    LSP_DID_OPEN,           // 文件打开
    LSP_DID_CHANGE,         // 文件内容变更
    LSP_COMPLETION,         // 补全请求
    LSP_HOVER,              // 悬停请求
    LSP_DEFINITION,         // 跳转定义请求
    LSP_DIAGNOSTIC          // 诊断刷新
} LspMessageType;

// 诊断严重级别
typedef enum {
    DIAG_ERROR   = 1,
    DIAG_WARNING = 2,
    DIAG_INFO    = 3,
    DIAG_HINT    = 4
} DiagnosticSeverity;

// 诊断项
typedef struct {
    int line, col;
    int end_line, end_col;
    char message[256];
    DiagnosticSeverity severity;
} Diagnostic;

// 补全项
typedef struct {
    char label[128];
    char detail[256];
    char insert_text[256];
    int kind;  // LSP CompletionItemKind
} CompletionItem;

// 核心接口：增量解析
void lsp_did_change(const char* uri, const char* content);

// 核心接口：获取诊断
int lsp_get_diagnostics(const char* uri, Diagnostic* out, int max_count);

// 核心接口：获取补全
int lsp_get_completions(const char* uri, int line, int col,
                        CompletionItem* out, int max_count);

// 核心接口：获取悬停信息
const char* lsp_get_hover(const char* uri, int line, int col);

// 核心接口：跳转定义
int lsp_get_definition(const char* uri, int line, int col,
                       char* target_uri, int uri_len,
                       int* target_line, int* target_col);

#endif /* LV_LSP_BRIDGE_H */
```

```javascript
// lsp_worker.js -- Web Worker 中的 LSP 消息处理（JavaScript 侧）
// 通过 WASM 调用 C 侧的 lsp_bridge 函数

const lvLspModule = await initLvLspWasm();

self.onmessage = async (event) => {
    const { method, params } = JSON.parse(event.data);

    switch (method) {
        case 'textDocument/didChange': {
            const uri = params.textDocument.uri;
            const content = params.contentChanges[0].text;
            // 调用 C 侧增量解析
            lvLspModule._lsp_did_change(uri, content);
            // 获取诊断结果
            const diagnostics = lvLspModule._lsp_get_diagnostics(uri, 100);
            self.postMessage(JSON.stringify({
                method: 'textDocument/publishDiagnostics',
                params: { uri, diagnostics }
            }));
            break;
        }

        case 'textDocument/completion': {
            const { uri, position } = params;
            const items = lvLspModule._lsp_get_completions(
                uri, position.line, position.character, 50
            );
            self.postMessage(JSON.stringify({
                id: params.id,
                result: items
            }));
            break;
        }

        case 'textDocument/hover': {
            const { uri, position } = params;
            const hover = lvLspModule._lsp_get_hover(
                uri, position.line, position.character
            );
            self.postMessage(JSON.stringify({
                id: params.id,
                result: hover ? { contents: hover } : null
            }));
            break;
        }

        case 'textDocument/definition': {
            const { uri, position } = params;
            const def = lvLspModule._lsp_get_definition(
                uri, position.line, position.character
            );
            self.postMessage(JSON.stringify({
                id: params.id,
                result: def
            }));
            break;
        }
    }
};
```

---

## 4. 实现路线图

### 4.1 总体阶段划分

整合 Langium 核心借鉴点到 Lv-00 中，分为三个主要阶段，每个阶段产出可独立交付的增量价值。

### 4.2 分阶段详细表

| 阶段 | 内容 | 具体任务 | 预计工期 | 前置依赖 |
|------|------|---------|---------|---------|
| **Phase 1** | **语法规范定义** | 1. 用 Langium 风格语法描述 Lv-00 DSL 全部语法规则<br>2. 编写 `lv_dsl.langium` 原型文件作为语法规范文档<br>3. 校验语法规则与 `formula_parser.c` 的一致性<br>4. 生成语法高亮的 Monarch tokenizer 定义<br>5. 定义 AST 节点类型枚举表（与 `ast.h` 对齐） | 2-3 周 | 当前 `formula_parser` 模块完整可用 |
| **Phase 2** | **LSP 集成** | 1. 实现 `lsp_bridge.h` 的 C 侧 LSP 接口层<br>2. 编译 C 代码为 WASM，部署至 Web GUI<br>3. 编写 `lsp_worker.js` Web Worker 消息处理<br>4. 集成 Monaco Editor 到 `lv_gui.html`<br>5. 实现补全、悬停、诊断三个核心功能<br>6. 编写端到端测试（模拟编辑操作，验证 LSP 响应） | 4-6 周 | Phase 1 语法规范完成；Emscripten/WASM 编译链就绪 |
| **Phase 3** | **多语言工作区** | 1. 实现公理包的命名空间机制（`namespace euclid { ... }`）<br>2. 实现跨公理包的符号引用解析<br>3. LSP 服务支持多文件 workspace<br>4. 实现公理包依赖图的可视化和版本管理<br>5. 性能优化：大型公理包的增量解析和索引缓存 | 3-4 周 | Phase 2 LSP 基础功能稳定；公理包加载模块完善 |

### 4.3 里程碑定义

| 里程碑 | 触发条件 | 验收标准 |
|--------|---------|---------|
| M1：语法规范冻结 | Phase 1 完成 | `lv_dsl.langium` 语法文件与所有现有 `.lvz` 示例文件完全兼容 |
| M2：LSP MVP | Phase 2 核心功能完成 | Web GUI 中编辑 `.lvz` 文件时，语法高亮、自动补全、错误诊断三项均可正常使用 |
| M3：多公理包互操作 | Phase 3 完成 | 在一个工程中加载多个 `.lvz` 公理包，跨包引用可被 LSP 正确解析 |

### 4.4 风险与缓解

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|---------|
| WASM 编译 `formula_parser` 时出现兼容性问题 | 中 | 高 | Phase 1 期间进行 WASM 编译预研（PoC），提前识别问题 |
| Monaco Editor 集成复杂度超出预期 | 中 | 中 | 保留备选方案：使用 CodeMirror 6（更轻量的编辑器组件） |
| LSP 实时诊断引入性能瓶颈 | 低 | 中 | 采用增量解析 + 去抖（debounce 300ms），大文件降级为非实时模式 |
| 手写解析器无法实现错误恢复 | 中 | 中 | Phase 2 中评估引入 Chevrotain 风格错误恢复的改造工作量，必要时保持"首错即停" |

---

## 5. 附录

### 5.1 参考链接

| 资源 | 链接 | 说明 |
|------|------|------|
| Langium 官方仓库 | https://github.com/eclipse-langium/langium | 项目主页，源码、Issues、Discussions |
| Langium 官方文档 | https://langium.org/docs/ | 入门指南、语法参考、LSP 开发指南 |
| Langium Playground | https://langium.org/playground/ | 在线语法验证和效果预览 |
| Chevrotain 官网 | https://chevrotain.io/ | Langium 底层解析器引擎 |
| LSP 协议规范 | https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/ | LSP 3.17 完整标准 |
| Monaco Editor | https://microsoft.github.io/monaco-editor/ | VS Code 核心编辑器组件（浏览器端） |
| Emscripten | https://emscripten.org/ | C/C++ → WASM 编译工具链 |
| Xtext 框架 | https://www.eclipse.org/Xtext/ | Langium 的前身和设计原型 |

### 5.2 Lv-00 现有 DSL 解析器文件清单

| 文件 | 用途 |
|------|------|
| `src/formula_parser.h` | 公式解析器公共接口声明 |
| `src/formula_parser.c` | 公式解析器核心实现（递归下降） |
| `src/tokenizer.h` | 词法分析器接口声明 |
| `src/tokenizer.c` | 词法分析器核心实现 |
| `src/ast.h` | AST 节点类型定义 |
| `src/ast.c` | AST 节点构造和释放函数 |
| `src/type_checker.h` | 类型检查器接口 |
| `src/type_checker.c` | 类型检查器实现 |
| `src/symbol_table.h` | 符号表接口 |
| `src/symbol_table.c` | 符号表实现 |
| `web/lv_gui.html` | Web GUI 主页面 |
| `web/lv_editor.js` | Web 端 DSL 编辑器脚本 |

### 5.3 术语对照表

| 中文术语 | 英文术语 | Langium 对应 | 说明 |
|---------|---------|-------------|------|
| 语法声明 | Grammar Declaration | `.langium` 文件 | 声明式定义 DSL 语法规则 |
| 解析器 | Parser | Chevrotain 生成 | 将 token 流转换为 AST |
| 词法分析器 | Tokenizer/Lexer | Chevrotain Lexer | 将字符流转换为 token 流 |
| 语言服务器协议 | LSP | Langium LSP Services | IDE 和语言工具的通信协议 |
| 代码补全 | Completion | CompletionProvider | 根据上下文提示可输入内容 |
| 悬停提示 | Hover | HoverProvider | 鼠标悬停显示类型和文档 |
| 诊断 | Diagnostics | ValidationRegistry | 错误、警告、提示信息 |
| 作用域 | Scope | ScopeProvider | 符号可见性规则 |
| 依赖注入 | DI / IoC | Langium DI Container | 组件解耦和替换机制 |
| AST | Abstract Syntax Tree | Langium AST | 代码的抽象语法树表示 |
| 多语言项目 | Multi-Language | Langium Workspace | 同一工程中多个 DSL 共存 |
| 公理包 | Axiom Package | — | Lv-00 特有概念：逻辑公理的集合 |

### 5.4 变更记录

| 日期 | 版本 | 变更内容 | 作者 |
|------|------|---------|------|
| 2026-05-24 | v1.0 | 初始版本：完整参考文档编制 | SOLO Agent |

---

> **文档说明**：本文档属于 Lv-00 `/docs/reference/` 系列的参考研究文档，旨在系统梳理外部开源项目的设计理念和最佳实践，为 Lv-00 的架构演进和技术选型提供依据。本文档不构成正式的技术规范，所有映射方案和实施建议仅供参考。
