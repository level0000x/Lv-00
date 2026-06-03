# 公式系统、DSL 编译器与几何代数 (Formula System, DSL Compiler & Geometric Algebra)

## 模块概述

本组模块构成 Lv-00 的形式化语言基础设施层，覆盖从公式输入、解析、渲染、格式转换，到 DSL 编译管线、表达式规范化，再到几何代数（PGA）嵌入/提取与多目标代码生成的完整链路。核心设计借鉴 Ganja.js 的 AST 转译与 GCLC 几何构造语言，结合 MathLive 的所见即所得编辑体验，最终实现从用户输入到约束图填充的端到端自动化流程。

## 核心设计原则

1. **多语法统一**：LaTeX / Python / DSL 三种输入语法共享同一 AST 结构，消除格式壁垒
2. **多格式输出**：六种渲染格式（LaTeX / Python / DSL / MathML / ASCII / HTML）覆盖排版、计算与 Web 场景
3. **编译管线解耦**：DSL 源码经词法分析、语法分析、AST、IR 到约束图五阶段编译，各阶段可独立调试
4. **纯整数符号运算**：表达式规范化禁止浮点运算，全部使用 GMP 多精度整数/有理数
5. **PGA 统一表示**：Cl(3,0,1) 几何代数统一表示点、向量、平面、射线、旋子和电机

## 覆盖模块总览

| 模块 | 职责 |
|------|------|
| formula_parser.h | 公式解析器，三语法 AST 构建 |
| formula_renderer.h | 公式渲染器，六种输出格式 |
| formula_converter.h | 公式与约束图双向转换 |
| expr_canonical.h | 表达式规范化（GMP 纯整数） |
| expr_canon.h | 规范多项式表示与排序 |
| dsl_compiler.h | DSL 编译器（五阶段管线） |
| lexer_shared.h | 共享词法分析器基础设施 |
| math_input.h | 所见即所得数学公式输入 |
| ga_interface.h | PGA 嵌入与提取接口 |
| ga_multivector.h | PGA 多向量运算 |
| ga_codegen.h | GA 多目标代码生成 |
| gc_language.h | GC 几何命令语言 |
| sym_expr.h | 符号表达式树 |
| math_protocol.h | 结构化数学中间表示 |

## 1. formula_parser.h -- 公式解析器

### 设计概述

公式解析器提供几何元语言系统的 AST 结构与解析 API。支持 LaTeX、Python、DSL 三种语法格式的输入，通过 `formula_detect_syntax()` 自动检测语法类型。AST 节点使用引用计数管理生命周期，支持深拷贝和安全共享。

### AST 节点类型枚举

```c
typedef enum {
    /* 基本类型 */
    NODE_NUMBER, NODE_VARIABLE, NODE_IDENTIFIER,

    /* 二元运算 */
    NODE_BINARY_OP_ADD, NODE_BINARY_OP_SUB,
    NODE_BINARY_OP_MUL, NODE_BINARY_OP_DIV, NODE_BINARY_OP_POW,

    /* 一元运算 */
    NODE_UNARY_OP_NEG, NODE_UNARY_OP_SQRT,
    NODE_UNARY_OP_SIN, NODE_UNARY_OP_COS, NODE_UNARY_OP_TAN,
    NODE_UNARY_OP_ABS, NODE_UNARY_OP_LN, NODE_UNARY_OP_LOG,

    /* 方程与坐标 */
    NODE_EQUATION, NODE_COORDINATE_LIST,

    /* 几何对象 */
    NODE_GEOM_POINT, NODE_GEOM_SEGMENT, NODE_GEOM_LINE,
    NODE_GEOM_CIRCLE, NODE_GEOM_TRIANGLE, NODE_GEOM_POLYGON,
    NODE_GEOM_REGION, NODE_GEOM_ARC, NODE_GEOM_VECTOR,

    /* 几何约束 */
    NODE_CONSTRAINT_PERPENDICULAR, NODE_CONSTRAINT_PARALLEL,
    NODE_CONSTRAINT_MIDPOINT, NODE_CONSTRAINT_BISECTOR,
    NODE_CONSTRAINT_COLLINEAR, NODE_CONSTRAINT_TANGENT,
    NODE_CONSTRAINT_CONGRUENT, NODE_CONSTRAINT_ANGLE,

    /* 复合语句 */
    NODE_COMPOUND
} NodeType;
```

### AST 节点结构

采用 tagged union 设计，运算符编码在 `NodeType` 中：

```c
struct FormulaNode {
    NodeType type;
    int line;       /* 源码行号 */
    int column;     /* 源码列号 */
    int refcount;   /* 引用计数 */
    union {
        struct { int64_t numerator; uint64_t denominator; bool is_integer; } number;
        struct { char *name; } variable;
        struct { FormulaNode *left; FormulaNode *right; } binary_op;
        struct { FormulaNode *operand; } unary_op;
        struct { FormulaNode *lhs; FormulaNode *rhs; } equation;
        struct { FormulaNode **coords; int coord_count; } coord_list;
        /* 几何对象与约束各含独立字段 */
        struct { FormulaNode **participants; int participant_count; } constraint;
        struct { FormulaNode **statements; int statement_count; } compound;
    } data;
};
```

### 核心操作

| 操作 | 函数 | 说明 |
|------|------|------|
| 语法检测 | `formula_detect_syntax(input)` | 返回 "latex"/"python"/"dsl"/"unknown" |
| 解析 | `formula_parse(input, syntax)` | 构建 AST，syntax 支持 "auto" |
| 销毁 | `formula_node_destroy(node)` | 引用计数安全释放 |
| 引用 | `formula_node_ref(node)` | 增加引用计数 |
| 深拷贝 | `formula_node_copy(node)` | 递归深拷贝 |
| 构建 | `formula_create_number/variable/binary_op/...` | 各类型 AST 节点工厂 |
| 几何构建 | `formula_create_geom_point/segment/circle/...` | 几何对象节点工厂 |
| 错误查询 | `formula_parser_get_last_error()` | 获取最后解析错误 |

### 错误恢复与位置追踪

每个 AST 节点记录 `line` 和 `column`，解析失败时通过 `formula_parser_get_last_error()` 返回错误信息。解析器在遇到语法错误时尝试同步恢复，尽可能构建部分 AST 以支持 IDE 场景下的增量解析。

## 2. formula_renderer.h -- 公式渲染器

### 设计概述

公式渲染器将 AST 渲染为多种输出格式的字符串，支持 LaTeX 数学排版、Python 数值计算代码生成、Lv-00 DSL、MathML、ASCII 艺术和 HTML MathJax 六种输出格式。

### 输出格式枚举

```c
typedef enum {
    OUTPUT_LATEX,    /* LaTeX 格式 */
    OUTPUT_PYTHON,   /* Python 代码格式 */
    OUTPUT_DSL,      /* Lv-00 DSL 格式 */
    OUTPUT_MATHML,   /* MathML 格式 */
    OUTPUT_ASCII,    /* ASCII 艺术格式 */
    OUTPUT_HTML      /* HTML MathJax 格式 */
} OutputFormat;
```

### 渲染选项

```c
typedef struct {
    bool implicit_multiplication; /* LaTeX: 隐式乘法 */
    bool display_mode;            /* LaTeX: 显示模式 */
    bool fraction_mode;           /* Python: 分数模式 */
    bool simplify_output;         /* 简化输出 */
    int precision;                /* 浮点数精度 */
} RenderOptions;
```

### 核心操作

| 操作 | 函数 | 说明 |
|------|------|------|
| 通用渲染 | `formula_render(node, format)` | 默认选项渲染 |
| 扩展渲染 | `formula_render_ex(node, format, options)` | 自定义选项渲染 |
| 缓冲区渲染 | `formula_render_to_buffer(node, format, buf, size)` | 渲染到已有缓冲区 |
| LaTeX 渲染 | `formula_render_latex(node)` | 格式特定渲染 |
| Python 渲染 | `formula_render_python(node)` | 格式特定渲染 |
| DSL 渲染 | `formula_render_dsl(node)` | 格式特定渲染 |
| 几何点 LaTeX | `formula_render_point_latex(name, coords, count)` | 如 "A = \left(1, 2\right)" |
| 分数 LaTeX | `formula_render_fraction_latex(num, den)` | 如 "\frac{3}{4}" |
| 希腊字母 | `formula_latex_greek_name(name)` | "theta" -> "\theta" |

## 3. formula_converter.h -- 公式格式转换器

### 设计概述

公式格式转换器是公式编辑器与图形系统之间的桥梁，提供 AST 与约束图的双向转换。正向转换将公式 AST 映射为约束图节点和约束，反向转换将约束图导出为多格式公式字符串。

### 转换结果结构

```c
/* 公式 -> 图 */
typedef struct {
    bool success;
    int *created_node_ids;
    int created_node_count;
    int *created_constraint_ids;
    int created_constraint_count;
    char error_message[256];
} FormulaToGraphResult;

/* 图 -> 公式 */
typedef struct {
    bool success;
    char *latex_output;
    char *python_output;
    char *dsl_output;
    char error_message[256];
} GraphToFormulaResult;
```

### 核心转换函数

| 方向 | 函数 | 说明 |
|------|------|------|
| AST -> 图 | `formula_to_graph(ast, graph)` | 将公式 AST 转换为约束图操作 |
| 图 -> 公式 | `graph_to_formula(graph)` | 将约束图转换为多格式公式 |
| 点转换 | `formula_convert_point(node, graph, out_id)` | 点定义到约束图 |
| 线段转换 | `formula_convert_segment(node, graph, out_id)` | 线段定义到约束图 |
| 圆转换 | `formula_convert_circle(node, graph, out_id)` | 圆定义到约束图 |
| 三角形转换 | `formula_convert_triangle(node, graph, out_ids, out_count)` | 三角形定义到约束图 |
| 垂直约束 | `formula_convert_perpendicular(node, graph, out_id)` | 垂直约束到约束图 |
| 平行约束 | `formula_convert_parallel(node, graph, out_id)` | 平行约束到约束图 |
| 角度约束 | `formula_convert_angle(node, graph, out_id)` | 角度约束（向量点积形式） |
| 方程曲线 | `formula_convert_equation_to_curve(node, ...)` | 隐式方程 F(x,y)=0 的行进正方形采样 |

### 变量名映射

通过 `formula_get_node_id()` / `formula_set_node_id()` / `formula_clear_var_map()` 管理变量名到约束图节点 ID 的双向映射，确保公式中的点名（如 A、B）与约束图节点正确关联。

## 4. expr_canonical.h -- 表达式规范化系统

### 设计概述

提供完全基于整数运算的表达式规范化系统，所有数值计算使用 GMP 多精度整数/有理数，禁止浮点运算。覆盖多项式、有理表达式和根式三种表达式的规范化。

### 规范化选项

```c
typedef struct {
    bool expand_products;        /* 展开乘积 */
    bool merge_like_terms;       /* 合并同类项 */
    bool order_terms;            /* 项排序（字典序） */
    bool rationalize_denom;      /* 分母有理化 */
    bool expand_nested_radicals; /* 展开嵌套根式 */
    bool simplify_fractions;     /* 约分 */
    int max_recursion_depth;     /* 最大递归深度 */
} Lv00CanonicalOptions;
```

### 表达式类型

```c
typedef enum {
    EXPR_TYPE_INTEGER, EXPR_TYPE_RATIONAL, EXPR_TYPE_VARIABLE,
    EXPR_TYPE_POLYNOMIAL, EXPR_TYPE_RATIONAL_EXPR, EXPR_TYPE_RADICAL,
    EXPR_TYPE_SUM, EXPR_TYPE_PRODUCT, EXPR_TYPE_POWER,
    EXPR_TYPE_FUNCTION, EXPR_TYPE_INVALID
} Lv00ExprType;
```

### 多项式操作

| 操作 | 函数 | 说明 |
|------|------|------|
| 创建 | `lv00_poly_create()` | 创建空多项式 |
| 添加项 | `lv00_poly_add_term(poly, coeff, vars, exponents, count)` | 添加单项式 |
| 合并同类项 | `lv00_poly_merge_like_terms(poly)` | O(n) 哈希分组合并 |
| 排序 | `lv00_poly_order_terms(poly)` | 按字典序排序 |
| 规范化 | `lv00_poly_normalize(poly)` | 合并 + 排序 |
| 四则运算 | `lv00_poly_add/sub/mul/div(a, b)` | 多项式算术 |
| 求值 | `lv00_poly_eval_int(poly, values, count, result)` | 整数点求值 |
| 序列化 | `lv00_poly_to_string(poly, var_names)` | 人类可读字符串 |

### 有理表达式操作

| 操作 | 函数 | 说明 |
|------|------|------|
| 创建 | `lv00_rat_expr_create(num, den)` | 分子/分母多项式 |
| 约分 | `lv00_rat_expr_simplify(expr)` | GCD 约分 |
| 加法 | `lv00_rat_expr_add(a, b)` | 通分后相加 |
| 乘法 | `lv00_rat_expr_mul(a, b)` | 分子/分母分别相乘 |

### 根式操作

| 操作 | 函数 | 说明 |
|------|------|------|
| 创建 | `lv00_radical_create(coeff, radicand, index)` | coeff * radicand^(1/index) |
| 嵌套展开 | `lv00_radical_try_expand(rad, out_expanded)` | sqrt(a + b*sqrt(c)) 展开 |
| 有理化 | `lv00_radical_rationalize(rad)` | 1/(a + b*sqrt(n)) 有理化 |
| 完全平方检测 | `lv00_is_perfect_square(n, out_root)` | GMP 整数完全平方检测 |

### Groebner 基计算

```c
/* Buchberger 算法，仅处理度数 <= 2 的多项式系统 */
bool lv00_compute_groebner_basis(Lv00Polynomial **polys, uint32_t poly_count,
                                  Lv00Polynomial ***out_basis, uint32_t *out_basis_count);
Lv00Polynomial *lv00_s_polynomial(const Lv00Polynomial *f, const Lv00Polynomial *g);
Lv00Polynomial *lv00_poly_reduce(const Lv00Polynomial *f, const Lv00Polynomial *g);
```

### 连分数近似

使用 Stern-Brocot 树搜索和连分数算法，完全基于整数运算：

```c
bool lv00_continued_fraction_approx(const mpz_t num, const mpz_t denom,
                                     mpz_t max_denom, mpz_t out_num, mpz_t out_denom);
void lv00_best_rational_approx(const mpz_t num, const mpz_t denom,
                                const mpz_t max_denom, mpz_t out_num, mpz_t out_denom);
```

## 5. expr_canon.h -- 规范多项式表示

### 设计概述

定义 Lv-00 代数表达式的规范形式及其排序规则，确保代数等价的两个表达式具有相同的规范形式。用于方程标准化、Groebner 基前的项排序固定化、等价性检查和缓存键生成。

### 规范形式不变式

规范多项式满足以下不变式：

1. 项按总次数降序排列
2. 同次数内按字典序排列变量名
3. 无零系数项
4. 首项系数为正

### 规范多项式结构

```c
typedef struct {
    Lv00ExprTerm *terms;    /* 项数组（按序排列） */
    int term_count;
    int term_capacity;
    int var_count;
    char **var_names;       /* 变量名数组 */
    bool canonicalized;     /* 是否已规范化 */
} Lv00ExprCanonical;
```

### 核心操作

| 操作 | 函数 | 说明 |
|------|------|------|
| 规范化 | `lv00_expr_canonicalize(expr)` | 合并同类项 + 消除零系数 + 排序 + 归一化符号 |
| 等价判定 | `lv00_expr_canonical_equal(a, b)` | 规范形式下线性逐项比较 |
| 算术运算 | `lv00_expr_canonical_add/sub/mul/scale/neg` | 产生规范结果 |
| 字符串解析 | `lv00_expr_canonical_from_string(str, names, count)` | 基本格式解析 |

### 排序规则

`lv00_canonical_compare_terms()` 实现混合排序策略（grlex + monomial ordering）：
- 总次数降序优先
- 同次数按字典序比较指数数组（最后一个变量优先）

## 6. dsl_compiler.h -- DSL 编译器

### 设计概述

借鉴 Ganja.js 的 AST 转译和 GCLC 几何构造语言，提供从 DSL 源码到约束图的完整编译管线。包含词法分析、语法解析、AST 构建、中间表示（IR）生成和约束图填充五个阶段。

### 编译管线

```
DSL源码 -> Tokenizer -> AST -> IR -> 约束图
```

### 词法单元类型（25+ 种）

```c
typedef enum {
    /* 几何构造原语（GCLC 风格） */
    DSL_TOK_POINT, DSL_TOK_LINE, DSL_TOK_CIRCLE, DSL_TOK_SEGMENT,
    DSL_TOK_RAY, DSL_TOK_POLYGON, DSL_TOK_TRIANGLE,

    /* 构造操作 */
    DSL_TOK_INTERSECT, DSL_TOK_PARALLEL, DSL_TOK_PERPENDICULAR,
    DSL_TOK_MIDPOINT, DSL_TOK_CIRCUMCENTER, DSL_TOK_ORTHOCENTER,
    DSL_TOK_CENTROID, DSL_TOK_INCENTER, DSL_TOK_BISECTOR,

    /* 控制流与声明 */
    DSL_TOK_LET, DSL_TOK_LOAD, DSL_TOK_PROVE, DSL_TOK_CONSTRAINT,
    DSL_TOK_FIX, DSL_TOK_FREE,

    /* 字面量、运算符、分隔符、元信息 */
    DSL_TOK_IDENT, DSL_TOK_NUMBER, DSL_TOK_LPAREN, DSL_TOK_RPAREN,
    DSL_TOK_ASSIGN, DSL_TOK_SEMI, DSL_TOK_EOF, DSL_TOK_ERROR, DSL_TOK_COMMENT
} DSLTokenType;
```

### AST 节点类型（20+ 种）

借鉴 Ganja.js inline AST 分类方式，AST 节点类型直接对应语言构造的语义类别：

```c
typedef enum {
    DSL_AST_PROGRAM, DSL_AST_POINT_DECL, DSL_AST_LINE_DECL,
    DSL_AST_CIRCLE_DECL, DSL_AST_SEGMENT_DECL, DSL_AST_RAY_DECL,
    DSL_AST_POLYGON_DECL, DSL_AST_TRIANGLE_DECL,
    DSL_AST_INTERSECT, DSL_AST_PARALLEL, DSL_AST_PERPENDICULAR,
    DSL_AST_MIDPOINT, DSL_AST_CIRCUMCENTER, DSL_AST_ORTHOCENTER,
    DSL_AST_CENTROID, DSL_AST_INCENTER, DSL_AST_BISECTOR,
    DSL_AST_CONSTRAINT, DSL_AST_PROVE, DSL_AST_LOAD,
    DSL_AST_FIX_POINT, DSL_AST_FREE_POINT, DSL_AST_BLOCK,
    DSL_AST_IDENT, DSL_AST_NUMBER
} DslASTType;
```

### 中间表示（IR）

借鉴 Ganja.js 过程化 API 调用序列设计，将声明式 DSL 翻译为过程化操作序列：

```c
typedef enum {
    /* 实体创建 */
    IR_CREATE_POINT, IR_CREATE_POINT_FIXED, IR_CREATE_LINE,
    IR_CREATE_CIRCLE, IR_CREATE_SEGMENT, IR_CREATE_RAY,
    IR_CREATE_POLYGON, IR_CREATE_TRIANGLE,

    /* 构造操作 */
    IR_INTERSECT, IR_PARALLEL_THROUGH, IR_PERPENDICULAR_THROUGH,
    IR_MIDPOINT_OF, IR_CIRCUMCENTER_OF, IR_ORTHOCENTER_OF,
    IR_CENTROID_OF, IR_INCENTER_OF, IR_BISECTOR_OF,

    /* 约束操作 */
    IR_ADD_CONSTRAINT, IR_CONSTRAIN_EQUAL, IR_CONSTRAIN_PARALLEL,
    IR_CONSTRAIN_PERPENDICULAR, IR_CONSTRAIN_COLLINEAR, IR_CONSTRAIN_CONCYCLIC,

    /* 系统操作 */
    IR_LOAD_AXIOM, IR_PROVE, IR_CHECK_SAT, IR_LABEL, IR_NOOP
} DslIROp;
```

### 编译配置

```c
typedef struct {
    DslCompileTarget target;  /* NATIVE / WASM / PYTHON / JAVASCRIPT */
    int optimize_level;       /* 0-3 */
    bool debug_ast;
    bool validate_ir;
    bool generate_source_map;
    int max_iterations;
} DslCompileConfig;
```

### 核心 API

| 阶段 | 函数 | 说明 |
|------|------|------|
| 词法分析 | `dsl_tokenize(source, out_tokens, out_count)` | 字符串 -> Token 序列 |
| 语法分析 | `dsl_parse(tokens, count, out_ast)` | Token 序列 -> AST（递归下降） |
| IR 编译 | `dsl_compile(ast, config, out_ir)` | AST -> IR（语义分析） |
| 图填充 | `dsl_ir_to_constraint_graph(ir, graph)` | IR -> 约束图 |
| 一键编译 | `dsl_compile_and_load(source, config, graph)` | 全流程串联 |
| 调试输出 | `dsl_ast_dump(ast, fd, indent)` | AST 可视化 |
| 调试输出 | `dsl_ir_dump(ir, fd)` | IR 可视化 |

## 7. lexer_shared.h -- 共享词法分析器

### 设计概述

为 axiom_pkg 和 module 提供公共的词法分析器基础类型和辅助函数。两个模块各自维护自己的 Token 类型和 `lexer_next_token` 实现（因数字解析规则和标识符规则不同），但共享 Lexer 结构体和空白/注释跳过、字符串字面量提取等公共操作。

### 共享范围

- `Lv00Lexer` 结构体（source, pos, line, col, error_msg）
- `lv00_lexer_init()`：初始化词法分析器
- `lv00_lexer_clear()`：重置/清除状态
- `lv00_lexer_skip_whitespace_and_comments()`：跳过空白和 `#` 到行尾的注释
- `lv00_lexer_extract_string()`：提取字符串字面量（含 `\n` `\t` `\r` `\"` `\\` 转义处理）

### 不共享范围

- Token 类型定义（字段差异：int vs double，有无 bool）
- `lexer_next_token()`（数字解析和标识符规则不同）

### Lexer 结构体

```c
typedef struct {
    const char *source; /* 输入源字符串 */
    const char *pos;    /* 当前解析位置 */
    int line;           /* 当前行号（从 1 开始） */
    int col;            /* 当前列号（从 1 开始） */
    char *error_msg;    /* 错误消息 */
} Lv00Lexer;
```

## 8. math_input.h -- 所见即所得数学公式输入

### 设计概述

借鉴 MathLive 的交互式编辑器设计，提供所见即所得的数学公式输入系统。支持 LaTeX / 纯文本 / 可视化三种输入模式，预置 20+ 几何专用宏，可自定义宏库和虚拟键盘布局，支持命令补全、undo/redo 和双向绑定。

### 三种输入模式

```c
typedef enum {
    INPUT_MODE_LATEX,     /* LaTeX 输入模式：实时渲染 */
    INPUT_MODE_PLAINTEXT, /* 纯文本输入模式：类似 ASCIIMath */
    INPUT_MODE_VISUAL     /* 可视化输入模式：点选虚拟键盘构建 */
} MathInputMode;
```

### 五种键盘布局

```c
typedef enum {
    KEYBOARD_STANDARD, /* 标准数学键盘 */
    KEYBOARD_GEOMETRY, /* 几何键盘：\point, \line, \circle 等 */
    KEYBOARD_PROOF,    /* 证明键盘：\implies, \forall, \exists 等 */
    KEYBOARD_GREEK,    /* 希腊字母键盘 */
    KEYBOARD_CUSTOM    /* 用户自定义 */
} MathKeyboardLayout;
```

### 数学宏系统

借鉴 MathLive 的自定义宏机制，每个宏包含名称、展开模板、分类、参数数量和键盘提示：

```c
typedef struct MathMacro {
    char *macro_name;           /* 宏名称（不含反斜杠） */
    char *expansion;            /* 展开后的 LaTeX 模板 */
    MathMacroCategory category; /* 宏分类 */
    int arg_count;              /* 参数数量 */
    char *keyboard_hint;        /* 虚拟键盘提示 */
    char *description;          /* 描述文本 */
    int id;
} MathMacro;
```

### 预置 20+ 几何宏

`\point`, `\line`, `\circle`, `\segment`, `\angle`, `\triangle`, `\quadrilateral`, `\parallel`, `\perp`, `\cong`, `\similar`, `\intersect`, `\tangent`, `\bisector`, `\midpoint`, `\distance`, `\area`, `\perimeter`, `\circumcircle`, `\incircle`, `\centroid`, `\orthocenter`, `\collinear`, `\concurrent`

### 命令补全系统

当用户输入前缀（如 `\ang`）时，系统从宏库中匹配候选列表，按匹配分数降序排列：

```c
typedef struct MathAutocomplete {
    char *prefix;                        /* 用户已输入的前缀 */
    MathCompletionCandidate *candidates; /* 匹配的候选列表 */
    int candidate_count;
    int max_candidates;                  /* 最大候选数（默认 10） */
    char trigger_char;                   /* 触发字符（默认 '\\'） */
    int selected_index;                  /* 当前选中索引 */
} MathAutocomplete;
```

### 输入状态

```c
typedef struct MathInputState {
    char *input_text;           /* 当前输入缓冲区 */
    int cursor_position;        /* 光标位置 */
    int selection_start, selection_end; /* 选区 */
    MathInputMode current_mode; /* 当前输入模式 */
    MathKeyboardLayout active_keyboard;
    MathMacroLibrary *macro_library;
    char *history[64];          /* undo 栈 */
    int history_index;
    bool dirty;                 /* 未同步更改标记 */
    MathExpr *parsed_expr;      /* 缓存解析结果 */
    char *rendered_output;      /* 渲染输出 */
} MathInputState;
```

### 核心 API

| 类别 | 函数 | 说明 |
|------|------|------|
| 初始化 | `math_input_init()` | 创建输入状态 |
| 模式切换 | `math_input_set_mode(state, mode)` | 三种模式切换 |
| 宏注册 | `math_input_register_macro(state, name, expansion, ...)` | 注册自定义宏 |
| 几何宏库 | `math_input_create_default_geometry_macros()` | 创建 20+ 预设宏 |
| 命令补全 | `math_input_autocomplete(state, prefix, out)` | 前缀匹配候选 |
| 渲染 | `math_input_render(state)` | 渲染为可显示字符串 |
| 解析 | `math_input_parse(state)` | 解析为 MathExpr 树 |
| LaTeX 导出 | `math_input_export_latex(state)` | 导出 LaTeX 字符串 |
| LaTeX 导入 | `math_input_import_latex(state, str)` | 从 LaTeX 导入 |
| 键盘布局 | `math_input_create_keyboard_layout(layout)` | 创建虚拟键盘 |
| 文本编辑 | `math_input_insert_text/delete/undo/redo/clear` | 编辑操作 |

## 9. ga_interface.h -- 几何代数接口

### 设计概述

提供标准 3D 几何表示与 Projective Geometric Algebra (PGA) Cl(3,0,1) 多向量编码之间的双向转换。PGA 统一表示点、向量、平面、射线、旋转和平移等几何对象。

### Cl(3,0,1) 嵌入约定

| 几何对象 | PGA 表示 | 分量 |
|----------|----------|------|
| 点 (x, y, z) | 三向量 | x*e023 + y*e013 + z*e012 + e123 |
| 向量 (vx, vy, vz) | 向量 | vx*e1 + vy*e2 + vz*e3 |
| 平面 (nx, ny, nz, d) | 向量 | nx*e1 + ny*e2 + nz*e3 + d*e0 |
| 直线 | 二向量 | e12 + e13 + e23 分量 |
| 旋子 | 偶数级 | 标量 + 二向量 |
| 电机 | 偶数级 | 标量 + 二向量 + 伪标量 |

### 嵌入/提取 API

| 操作 | 函数 | 说明 |
|------|------|------|
| 点嵌入 | `ga_embed_point(x, y, z)` | 3D 点 -> PGA 三向量 |
| 点提取 | `ga_extract_point(mv, out_x, out_y, out_z)` | PGA -> 3D 坐标 |
| 向量嵌入 | `ga_embed_vector(vx, vy, vz)` | 方向向量 -> PGA |
| 向量提取 | `ga_extract_vector(mv, out_vx, out_vy, out_vz)` | PGA -> 方向向量 |
| 平面嵌入 | `ga_embed_plane(nx, ny, nz, d)` | 法向量+距离 -> PGA |
| 平面提取 | `ga_extract_plane(mv, out_nx, out_ny, out_nz, out_d)` | PGA -> 平面参数 |
| 射线嵌入 | `ga_embed_ray(origin, dir)` | 外积 P ^ v |
| 射线提取 | `ga_extract_ray(mv, out_origin, out_dir)` | PGA -> 原点+方向 |
| 旋转嵌入 | `ga_embed_rotation(ax, ay, az, angle)` | cos(theta/2) + sin(theta/2)*B |
| 旋转提取 | `ga_extract_rotation(rotor, out_ax, out_ay, out_az, out_angle)` | PGA -> 轴+角度 |
| 平移嵌入 | `ga_embed_translation(tx, ty, tz)` | 1 + 0.5*(tx*e01 + ty*e02 + tz*e03) |

### 几何构造函数

| 操作 | 函数 | 说明 |
|------|------|------|
| 两点直线 | `ga_line_from_two_points(p1, p2)` | L = P ^ Q |
| 三点共线 | `ga_three_points_collinear(p1, p2, p3)` | P ^ Q ^ R = 0 |
| 四点共面 | `ga_four_points_coplanar(p1, p2, p3, p4)` | P ^ Q ^ R ^ S = 0 |
| 三点平面 | `ga_plane_from_three_points(p1, p2, p3)` | pi = P ^ Q ^ R |

## 10. ga_multivector.h -- PGA 多向量运算

### 设计概述

借鉴 GATr（扁平数组存储、基于级的操作）和 GAALOP（乘法表驱动计算、代码生成）的设计模式。默认代数为 Cl(3,0,1)，包含 16 个基刃（grades 0..4）。

### 基刃排序（Cl(3,0,1)）

| 级 | 基刃 |
|----|------|
| 0 | 1 |
| 1 | e1, e2, e3, e0 |
| 2 | e12, e13, e03, e23, e023 |
| 3 | e123, e0123, e013, e0234 |
| 4 | e01234, e1234 |

### 多向量结构

```c
#define GA_MV_DIM 16

typedef struct Lv00MultiVector {
    double components[GA_MV_DIM];           /* 数值刃系数 */
    char  *symbolic_components[GA_MV_DIM];  /* 符号刃表达式（可空） */
    int    is_symbolic;                      /* 符号模式标志 */
    double trust;                            /* 信任度 */
} Lv00MultiVector;
```

### 乘法表

```c
typedef struct GAMultEntry {
    int result_index; /* 结果刃索引 */
    int sign;         /* 符号因子: +1 或 -1 */
} GAMultEntry;

typedef struct GAMultTable {
    GASignature sig;          /* 代数签名 Cl(p,q,r) */
    int dim;                  /* 基刃数量 */
    GAMultEntry *table;       /* 16x16 乘法表 */
    char **blade_names;       /* 基刃名称 */
} GAMultTable;
```

### 预定义代数签名

```c
static const GASignature GA_CL_3_0_1 = {3, 0, 1}; /* PGA 3D 欧氏 */
static const GASignature GA_CL_2_0_1 = {2, 0, 1}; /* PGA 2D 欧氏 */
static const GASignature GA_CL_3_0_0 = {3, 0, 0}; /* 标准 3D 欧氏 */
```

### 核心运算

| 操作 | 函数 | 说明 |
|------|------|------|
| 创建 | `ga_mv_zero/scalar/copy/free` | 生命周期管理 |
| 几何积 | `ga_geometric_product(a, b)` | 基础乘积，查表计算 |
| 外积 | `ga_outer_product(a, b)` | 最高级分量，表示 join |
| 内积 | `ga_inner_product(a, b)` | 最低级分量，表示 meet |
| 逆 | `ga_reverse(mv)` | 奇数级取反 |
| 级对合 | `ga_grade_involute(mv)` | 奇数级取反 |
| 范数 | `ga_norm_squared(mv)` | ||mv||^2 = <mv * ~mv>_0 |
| 级投影 | `ga_mv_grade_projection(mv, grade)` | 投影到指定级 |
| 算术 | `ga_mv_add/sub/scale` | 加减缩放 |

## 11. ga_codegen.h -- GA 代码生成器

### 设计概述

借鉴 GAALOP 的编译符号 GA 表达式为优化数值代码的方法，将多向量表达式翻译为多种目标语言。

### 支持的目标语言

```c
typedef enum GACodegenTarget {
    GA_CODEGEN_C,      /* 标准 C 代码 */
    GA_CODEGEN_CPP,    /* C++ 代码 */
    GA_CODEGEN_CUDA,   /* CUDA kernel 代码 */
    GA_CODEGEN_LATEX,  /* LaTeX 数学表示 */
    GA_CODEGEN_PYTHON, /* Python / NumPy 代码 */
    GA_CODEGEN_DOT     /* Graphviz DOT 图 */
} GACodegenTarget;
```

### 代码生成选项

```c
typedef struct GACodegenOptions {
    GACodegenTarget target;
    const char *variable_name;  /* 输出变量名 */
    const char *indent;         /* 缩进字符串 */
    int include_header;         /* 包含头注释 */
    int optimize;               /* 常量折叠优化 */
} GACodegenOptions;
```

### 核心 API

| 操作 | 函数 | 说明 |
|------|------|------|
| 编译 | `ga_codegen_compile(mv, options)` | 多向量 -> 目标代码 |
| LaTeX 渲染 | `ga_render_latex(mv)` | 多向量 -> LaTeX 字符串 |
| DOT 渲染 | `ga_render_dot(mv)` | 多向量 -> Graphviz DOT 图 |

## 12. gc_language.h -- GC 几何命令语言

### 设计概述

借鉴 GCLC（Geometry Constructions Language）的声明式语法和 WASM 移植管道，提供 42 种几何命令和 5 种证明方法的完整几何构造语言。

### 证明方法（5 种）

```c
typedef enum {
    GCL_PROOF_AREA = 0,       /* 面积法 —— 面积关系和消点法 */
    GCL_PROOF_WU = 1,         /* 吴方法 —— 代数消元 */
    GCL_PROOF_GROEBNER = 2,   /* Groebner 基法 —— 代数方程系统 */
    GCL_PROOF_FULL_ANGLE = 3, /* 全角法 —— 全角关系角度推理 */
    GCL_PROOF_VECTOR = 4      /* 向量法 —— 矢量代数推导 */
} GCLProofMethod;
```

### 命令类型（42 种）

涵盖基本声明（point/line/circle/segment/ray/arc/polygon/triangle）、构造命令（intersect/midpoint/bisector/perpendicular/parallel/mediatrix/orthocenter/centroid/circumcenter/incenter/foot/reflection/rotation/translation/scale）、测量命令（measure/angle/calc/distance/area）、证明命令（prove/assume/lemma/conjecture/counterexample）、模块命令（load/include/export/save）和元命令（comment/set/echo/dump）。

### GCL 上下文

```c
struct GCLContext {
    GCLCommand **commands;       /* 命令列表 */
    int command_count;
    GCLProofMethod proof_method; /* 当前证明方法 */
    char **symbol_names;         /* 符号表 */
    int *symbol_node_ids;
    ConstraintGraph *graph;      /* 关联约束图 */
    WasmExportConfig wasm_config; /* WASM 编译配置 */
};
```

### WASM 编译管道

借鉴 GCLC 的 C++ -> Emscripten -> WASM -> TypeScript Web GUI 管道，支持三种导出级别：

```c
typedef enum {
    WASM_GCL_DEFAULT = 0, /* 解析+执行 */
    WASM_GCL_MINIMAL = 1, /* 仅解析 */
    WASM_GCL_FULL = 2     /* 解析+执行+可视化+证明 */
} WasmExportFormat;
```

### 核心 API

| 类别 | 函数 | 说明 |
|------|------|------|
| 上下文管理 | `gcl_context_create/destroy` | 生命周期 |
| 解析 | `gcl_parse(ctx, line)` / `gcl_parse_file(ctx, path)` | 单行/文件解析 |
| 执行 | `gcl_execute(ctx)` / `gcl_execute_command(ctx, cmd)` | 命令执行 |
| 证明方法 | `gcl_set/get_proof_method(ctx, method)` | 运行时切换 |
| 证明执行 | `gcl_prove(ctx, proposition, timeout_ms)` | 定理证明 |
| 导出 | `gcl_export_latex/html(ctx, path)` | LaTeX/HTML 导出 |
| WASM | `gcl_compile_wasm(ctx, format, mem)` | WASM 编译 |
| 图转换 | `gcl_to_constraint_graph(ctx, graph)` | 命令 -> 约束图 |

## 13. sym_expr.h -- 符号表达式树

### 设计概述

借鉴 SymEngine 和 GiNaC 的设计，提供树形符号表达式系统，支持常量、变量、二元运算、一元运算的构建、化简、求值、渲染、微分和变量替换。

### 表达式类型

```c
typedef enum Lv00SymExprKind {
    LV00_SYM_CONST = 0,  /* 数值常量 */
    LV00_SYM_VAR,        /* 变量 */
    LV00_SYM_ADD,        /* 加法 */
    LV00_SYM_MUL,        /* 乘法 */
    LV00_SYM_POW,        /* 幂 */
    LV00_SYM_NEG,        /* 取负 */
    LV00_SYM_SIN,        /* 正弦 */
    LV00_SYM_COS,        /* 余弦 */
    LV00_SYM_SQRT,       /* 平方根 */
    LV00_SYM_LOG         /* 自然对数 */
} Lv00SymExprKind;
```

### 表达式树节点

```c
typedef struct Lv00SymExpr {
    Lv00SymExprKind kind;
    double value;              /* SYM_CONST */
    char *var_name;            /* SYM_VAR */
    struct Lv00SymExpr **children;
    int child_count;
} Lv00SymExpr;
```

### 核心 API

| 操作 | 函数 | 说明 |
|------|------|------|
| 构建 | `sym_expr_create_const/var/binary/unary` | 各类型节点工厂 |
| 销毁 | `sym_expr_destroy(expr)` | 递归释放 |
| 化简 | `sym_expr_simplify(expr)` | 常数折叠、幺元消除、零元吸收 |
| 求值 | `sym_expr_eval_double(expr, names, values, count)` | 数值求值 |
| 字符串 | `sym_expr_to_string(expr)` | 中缀表示 |
| 微分 | `sym_expr_diff(expr, var_name)` | 符号微分 |
| 替换 | `sym_expr_substitute(expr, var_name, replacement)` | 变量替换 |

## 14. math_protocol.h -- 结构化数学中间表示

### 设计概述

借鉴 CortexJS / MathJSON 的语义化表达式设计，提供树形 AST 表达任意数学表达式，支持 MathJSON 兼容的序列化/反序列化、可扩展符号字典和前后端通信协议。

### 表达式类型（32 种）

覆盖原子类型（NUMBER/SYMBOL/STRING）、基本算术（ADD/SUBTRACT/MULTIPLY/DIVIDE/POWER/NEGATE/SQRT/ABS）、三角函数（SIN/COS/TAN/ARCSIN/ARCCOS/ARCTAN）、关系运算（EQUAL/LESS/GREATER/LESS_EQUAL/GREATER_EQUAL/NOT_EQUAL）、通用函数（FUNCTION）和几何专用（GEOM_POINT/GEOM_LINE/GEOM_CIRCLE/DISTANCE/MIDPOINT/COLLINEAR/PARALLEL/PERPENDICULAR）。

### MathExpr 节点

```c
typedef struct MathExpr {
    MathExprType type;
    struct MathExpr **args;  /* 子表达式数组 */
    int arg_count;
    double number_value;     /* NUMBER */
    char *symbol_name;       /* SYMBOL */
    char *string_value;      /* STRING */
    char *function_name;     /* FUNCTION */
    int id;                  /* 唯一 ID */
    bool is_simplified;
    bool is_evaluated;
    double evaluated_value;
} MathExpr;
```

### 可扩展数学字典

借鉴 CortexJS Compute Engine 的可扩展字典机制：

```c
typedef struct MathDictEntry {
    char *name;                   /* 条目名称 */
    MathDictEntryType entry_type; /* FUNCTION / SYMBOL / PREDICATE */
    int arity;                    /* 元数（-1 = 可变参数） */
    char *description;
    int id;
} MathDictEntry;

typedef struct MathDictionary {
    MathDictEntry *entries;
    int entry_count;
    char *name;         /* 字典名称 */
    bool is_readonly;
} MathDictionary;
```

### 序列化格式

```c
typedef enum {
    MATH_FORMAT_JSON,   /* MathJSON: ["Add", "x", 2] */
    MATH_FORMAT_BINARY, /* 二进制紧凑格式 */
    MATH_FORMAT_S_EXPR  /* S-表达式: (Add x 2) */
} MathSerializationFormat;
```

### 核心 API

| 类别 | 函数 | 说明 |
|------|------|------|
| 表达式创建 | `math_expr_create_number/symbol/create/add_arg/destroy/clone` | 生命周期 |
| 序列化 | `math_expr_serialize(expr, format)` | MathExpr -> JSON/S-expr |
| 反序列化 | `math_expr_deserialize(json, format)` | JSON/S-expr -> MathExpr |
| 化简 | `math_expr_simplify(expr)` | 代数化简 |
| 求值 | `math_expr_evaluate(expr, names, values, count)` | 变量绑定求值 |
| 字典管理 | `math_dict_create/register/lookup/unregister/destroy` | 可扩展字典 |
| LaTeX 导出 | `math_expr_to_latex(expr)` | MathExpr -> LaTeX |
| 通信协议 | `math_protocol_create/send/receive/destroy_message` | 前后端消息帧 |
