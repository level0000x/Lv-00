# 37. 解析层（Layer 1）：词法分析、语法解析、DSL 编译、AST 生成与语义分析

## 模块概述

本文档描述 Lv-00 十层架构中 **Layer 1 解析层**，覆盖从源码文本到可执行中间表示（IR）与约束图的完整编译前端：词法分析、递归下降语法解析、AST 生成、语义分析，以及面向几何构造的 DSL 编译管线。实现位于 `core/src/layer1_parser/`（`dsl_lexer.c`、`lv_lexer.c`、`lv_parser.c`、`lv_ast.c`、`lv_sema.c`、`lv_loader.c`）。

解析层提供两条并行管线：

1. **全量语言管线**：`lv_lexer.h` → `lv_parser.h` → `lv_ast.h` → `lv_sema.h`，面向 `lv_LANGUAGE_SPEC.md` 定义的语言（声明、约束、证明、导出语句）。
2. **几何 DSL 快速管线**：`dsl_compiler.h` 提供 `dsl_tokenize → dsl_parse → dsl_compile → dsl_ir_to_constraint_graph` 五阶段编译，借鉴 Ganja.js 的 AST 转译与 GCLC 几何构造语言，最终填充约束图。

**覆盖头文件**：
- `lv_lexer.h` —— 词法分析：Token 类型、源码位置、共享几何关键词表
- `lv_parser.h` —— 语法解析：解析器上下文与 `LvParseResult`
- `lv_ast.h` —— AST 节点（tagged union）与构建辅助
- `lv_sema.h` —— 语义分析：类型检查与错误收集
- `dsl_compiler.h` —— DSL 编译管线：Tokenizer → AST → IR → 约束图
- `formula_parser.h` —— 公式解析：LaTeX / Python / DSL 语法检测与引用计数 AST
- `lexer_shared.h` —— 共享词法分析器基础（`lvLexer`、转义字符串提取）
- `parser_safety.h` / `lv_parse_utils.h` —— 输入校验与安全数字解析

---

## 核心设计原则

1. **阶段分离**：词法、语法、语义严格分阶段，各阶段数据结构独立（Token / AST / IR），便于单测与错误定位。
2. **递归下降解析**：语法解析采用递归下降算法，语法结构直观映射到解析函数；`lv_parser_parse_program` 返回结构化错误数组（`LvParseError errors[64]`），不因首个错误中断。
3. **统一 AST、tagged union 编码**：`LvAstNode` 以节点类型区分语义（声明/语句/表达式/量词），通过 `next/child` 链表组织树结构，配合 `child_count` 支持 O(1) 遍历。
4. **语义分析独立成层**：`lv_sema` 对 AST 执行类型检查（几何实体类型、布尔/命题上下文），错误消息按索引查询，不污染语法层。
5. **DSL 声明式转过程化**：借鉴 Ganja.js 将声明式几何构造（`point A 10 20; line a A B;`）翻译为过程化 IR 操作序列（`IR_CREATE_POINT`/`IR_CREATE_LINE` 等），IR 符号表支持 O(1) 符号解析，最终解释填充约束图。
6. **多目标可移植**：DSL IR 支持生成 C/WASM/Python/JavaScript 目标代码（`DslCompileTarget`），语义分析与 IR 生成共用一个编译配置。
7. **输入安全**：解析入口前置 `lv_input_validate` 校验；数字解析一律走 `lv_parse_int`/`lv_parse_double(_strict)` 等带 errno 与整串消费检查的安全函数；`formula_parser` 以引用计数管理 AST 生命周期，并以 `node_count`/`current_depth` 限制解析深度与规模。

---

## 关键数据结构（C 代码块）

```c
/* 词法单元（lv_lexer.h） */
typedef struct {
    LvTokenType type;        /* 枚举：字面量/关键字/运算符/分隔符/EOF/ERROR */
    LvSourceLoc loc;         /* { int line; int column; size_t offset; } */
    const char *start;       /* 指向源文本中的起始位置（非自有） */
    size_t length;           /* token 文本长度 */
} LvToken;

/* AST 节点（lv_ast.h，tagged union 核心骨架） */
struct LvAstNode {
    LvAstNodeType type;      /* 声明/语句/表达式/量词/比较/模块等 40+ 类 */
    LvSourceLoc loc;
    LvAstNode *next;         /* 兄弟节点链表 */
    LvAstNode *child;        /* 第一个子节点 */
    int child_count;
    union {                  /* 按 type 解释的载荷：decl/let_def/ident/ */
        /* ... literal / quantifier / call / binary / unary / compare / */
        /* ... stmt / field / export_stmt / module_import / theorem / ... */
    } data;
};

/* 解析结果（lv_parser.h） */
typedef struct {
    LvAstNode *ast;          /* 解析产生的 AST 根（可为 NULL） */
    int error_count;         /* 语法错误数量 */
    LvParseError errors[64]; /* 结构化错误：{ LvSourceLoc loc; char message[256]; } */
} LvParseResult;

/* DSL 编译中间表示（dsl_compiler.h） */
typedef struct DslIR {
    DslIROperation *operations; /* IR 操作数组（opcode/操作数/结果 ID/标签） */
    int op_count;
    int op_capacity;
    char **symbols;             /* 符号表（ID -> 名称） */
    int *symbol_to_ir_id;       /* 符号到结果 IR ID 的映射 */
    lvHashtable *symbol_index;  /* 符号名 → 下标+1 哈希索引（O(1) 查找） */
    int symbol_count;
    int symbol_capacity;
    int next_id;                /* 下一个可用结果实体 ID */
} DslIR;

/* 公式解析器上下文（formula_parser.h） */
typedef struct {
    const char *input;
    size_t pos;
    size_t length;
    char error_message[256];
    bool has_error;
    int line;
    int column;
    int node_count;      /* AST 节点计数（安全限制用） */
    int current_depth;   /* 当前解析递归深度 */
} ParserContext;
```

> 说明：`LvTokenType` 覆盖整数/有理数/小数/字符串/标识符字面量、40+ 关键字（`Point`/`Constraint`/`Theorem`/`Prove` 等）与 `|-`/`|=`/`=>` 等逻辑符号；`dsl_compiler.h` 的 `DslAST`/`DslIROperation`/`DslCompileConfig` 与 `formula_parser.h` 的 `FormulaNode`（`refcount` 引用计数，含 8 类约束节点）是另外两条管线的核心结构。

---

## 主要接口（表格）

### 词法分析（`lv_lexer.h`）

| 函数 | 功能 |
|------|------|
| `lv_lexer_create(source, len)` / `lv_lexer_destroy(lexer)` | 创建/销毁词法器 |
| `lv_lexer_next(lexer)` | 获取下一个 Token |
| `lv_lexer_peek(lexer, lookahead)` | 窥视未来 Token（不消费） |
| `lv_lexer_get_loc(lexer)` | 获取当前位置（错误报告用） |
| `lv_token_type_name(type)` | Token 类型转字符串（调试/错误消息） |
| `lv_token_text(token, buf, size)` | 安全提取 Token 文本到缓冲区 |
| `lv_geometry_relation_keywords[]` / `lv_measurement_keywords[]` | 共享几何关系/度量关键词表（parser/sema 单一事实源） |

### 语法解析（`lv_parser.h`）

| 函数 | 功能 |
|------|------|
| `lv_parser_create(lexer)` / `lv_parser_destroy(parser)` | 创建/销毁解析器 |
| `lv_parser_parse_program(parser)` | 解析完整程序，返回 `LvParseResult`（AST + 错误数组） |

### AST 生成与操作（`lv_ast.h`）

| 函数 | 功能 |
|------|------|
| `lv_ast_create(type, loc)` | 创建基础 AST 节点 |
| `lv_ast_create_ident/int/rational/decimal/string/bool(...)` | 创建字面量与标识符节点 |
| `lv_ast_create_call/call_typed(...)` | 创建函数/关系/度量/几何调用节点 |
| `lv_ast_create_binary/unary/compare/logic_binary(...)` | 创建运算节点 |
| `lv_ast_append_child(parent, child)` | 追加子节点到链表末尾 |
| `lv_ast_destroy(node)` | 递归销毁整棵 AST 树 |
| `lv_ast_print(node, indent)` | 打印 AST（调试用） |
| `lv_entity_type_name(type)` / `lv_entity_type_from_token(tok)` | 实体类型名称/关键字映射 |

### 语义分析（`lv_sema.h`）

| 函数 | 功能 |
|------|------|
| `lv_sema_create()` / `lv_sema_destroy(ctx)` | 创建/销毁语义分析上下文 |
| `lv_sema_analyze(ctx, ast)` | 对 AST 执行语义分析，返回是否无严重错误 |
| `lv_sema_error_count(ctx)` / `lv_sema_error_msg(ctx, index)` | 获取错误数量与单条错误消息 |

### DSL 编译管线（`dsl_compiler.h`）

| 函数 | 功能 |
|------|------|
| `dsl_tokenize(source, &tokens, &count)` / `dsl_tokens_destroy(tokens, count)` | 词法分析及数组释放（需携带 count） |
| `dsl_parse(tokens, count, &ast)` | 递归下降解析为 `DslAST` |
| `dsl_compile(ast, config, &ir)` | AST → IR（含符号解析与类型检查） |
| `dsl_ir_to_constraint_graph(ir, graph)` | IR → 约束图（解析符号引用） |
| `dsl_compile_and_load(source, config, graph)` | 一键编译并加载 |
| `dsl_compile_config_default(&cfg)` | 填充默认编译配置 |
| `dsl_ast_destroy(ast)` / `dsl_ir_destroy(ir)` | 释放 AST/IR |
| `dsl_ast_dump(ast, fd, indent)` / `dsl_ir_dump(ir, fd)` | 调试输出 |
| `dsl_ir_op_name(op)` / `dsl_ast_type_name(type)` | 名称查询（静态字符串） |

### 公式解析（`formula_parser.h`）

| 函数 | 功能 |
|------|------|
| `formula_detect_syntax(input)` | 检测语法类型（"latex"/"python"/"dsl"/"unknown"） |
| `formula_parse(input, syntax)` | 解析公式构建 `FormulaNode` AST |
| `formula_parser_get_last_error()` | 获取最后一次解析错误 |
| `formula_node_ref/refcount/destroy/copy` | 引用计数生命周期管理（refcount=0 才释放） |
| `formula_create_number/variable/identifier/binary_op/unary_op/equation/...` | AST 构建辅助 |
| `formula_create_geom_point/segment/circle/triangle/polygon/region/arc/...` | 几何对象节点 |
| `formula_create_constraint(type, participants, count)` | 创建约束节点 |
| `formula_create_compound(...)` / `formula_compound_add_statement(...)` | 复合语句 |

### 共享词法器基础与安全工具

| 函数 | 功能 |
|------|------|
| `lv_lexer_init(lex, source)` / `lv_lexer_clear(lex)` | 共享 `lvLexer` 初始化/重置（`lexer_shared.h`） |
| `lv_lexer_skip_whitespace_and_comments(lex)` | 跳过空白与 `#` 注释 |
| `lv_lexer_extract_string(lex)` | 提取字符串字面量（转义解码 `\n \t \r \" \\`） |
| `lv_input_validate(input, len)` | 解析前输入校验，返回 `lvErrorCode`（`parser_safety.h`） |
| `lv_parse_int` / `lv_parse_double` / `lv_parse_double_strict` / `lv_parse_int_default` | 安全数字解析（errno + 整串消费检查） |
| `lv_env_get_int` / `lv_env_get_bool` | 环境变量安全解析（范围钳制/布尔归一化） |

---

## 工作流程

**全量语言管线**：

```
source → lv_input_validate → lv_lexer_create → lv_lexer_next/peek 逐 Token
      → lv_parser_create(lexer) → lv_parser_parse_program → LvParseResult{ast, errors[]}
      → lv_ast_destroy 释放 → lv_sema_create → lv_sema_analyze(ast)
      → lv_sema_error_count / lv_sema_error_msg 收集语义错误 → lv_sema_destroy
```

**几何 DSL 编译管线（五阶段）**：

```
DSL 源码 → dsl_tokenize（DslToken[]）
        → dsl_parse（递归下降，DslAST）
        → dsl_compile（AST → DslIR，符号表 + 类型检查）
        → dsl_ir_to_constraint_graph（IR 逐条解释，填充 ConstraintGraph）
捷径：dsl_compile_and_load（一次完成全部阶段，适合交互式应用）
```

**公式解析管线**：`formula_detect_syntax` 检测语法 → `formula_parse(input, syntax)` 构建引用计数 AST；`ParserContext.node_count/current_depth` 防止超限递归；多持有方通过 `formula_node_ref` 增加引用，`formula_node_destroy` 仅在 `refcount=0` 时递归释放。

**错误与安全**：语法错误在 `LvParseResult.errors[64]` 中批量返回（带行列号）；语义错误经 `lv_sema_error_msg` 按索引查询；所有外部输入先过 `lv_input_validate`，数字字面量解析统一走 `lv_parse_utils.h` 安全函数，杜绝 `atoi`/`atof` 未定义行为。

---

## 模块关系（表格）

| 上游/调用方 | 关系说明 | 参考文档 |
|-------------|----------|----------|
| 核心基础设施 | `error_codes.h`（`lvErrorCode`）支撑 `lv_input_validate`；`config.h` 提供解析深度/坐标维度上限 | [23_core_infrastructure.md](23_core_infrastructure.md) |
| 语言规范 | 解析层实现 `lv_LANGUAGE_SPEC.md` 定义的语法与语义 | [lv_LANGUAGE_SPEC.md](lv_LANGUAGE_SPEC.md) |
| 约束传播 | `dsl_ir_to_constraint_graph` 填充的 `ConstraintGraph` 由约束传播消费 | [24_constraint_propagation.md](24_constraint_propagation.md) |
| 上下文与生命周期 | `lv_loader.c` 负责脚本加载，AST/IR 生命周期挂靠上下文 | [12_context_and_lifecycle.md](12_context_and_lifecycle.md) |
| 引擎调度器 | 解析产物（AST/IR/约束图）作为调度任务输入 | [25_engine_scheduler.md](25_engine_scheduler.md) |
| 函数块系统 | DSL IR 编译为原生/过程化目标代码，与函数块执行模型对接 | [07_func_block.md](07_func_block.md) |
| 元证明缓存 / 图形化编程 | `prove`/`Theorem` 语句生成证明任务；`35` 文档的文本代码视图消费 AST | [34_meta_proof_cache.md](34_meta_proof_cache.md)、[35_layer6_visual_programming.md](35_layer6_visual_programming.md) |
| 流式互操作 | 流式输入中的文本先经本层解析再进入推理流水线 | [31_stream_interop.md](31_stream_interop.md) |

---

## 版本历史

| 版本 | 日期 | 变更内容 |
|------|------|----------|
| 1.0.0 | 2026-06 | 建立 Layer 1：`lv_lexer`/`lv_parser`/`lv_ast`/`lv_sema` 全量语言管线 |
| 1.1.0 | 2026-07 | 引入 `dsl_compiler.h` 五阶段 DSL 编译管线与 `formula_parser.h` 多语法公式解析；`lexer_shared.h` 收敛共享词法器基础 |
| 1.2.0 | 2026-08 | 本次成文：固化解析层边界、安全输入校验（`parser_safety.h`）与语义分析接口约定 |
