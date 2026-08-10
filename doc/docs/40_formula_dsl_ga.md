# 40. 公式 DSL 与几何代数（GA）

## 模块概述

本模块覆盖 Lv-00 中公式表达式从输入到代码生成的完整链路，由四个子系统构成：

1. **公式 DSL 解析**（`formula_parser.h`）：将 LaTeX、Python、DSL 三种输入语法统一解析为同一棵 `FormulaNode` AST，覆盖数值、变量、运算、方程、坐标以及几何对象（点/线段/直线/圆/三角形/多边形/区域/弧/向量）与几何约束（垂直/平行/中点/角平分线/共线/切线/全等/角度）的表示。
2. **公式渲染**（`formula_renderer.h`）：把 AST 渲染为 LaTeX、Python、DSL、MathML、ASCII、HTML(MathJax) 六种输出格式，含几何点/线段/圆/分数的专用 LaTeX 渲染与希腊字母映射。
3. **表达式规范化**（`expr_canonical.h` + `expr_canon.h`）：双轨规范表示——树形符号表达式 `lvExpr`（变量/有理数/幂/积/和/函数，GMP 有理数）与稀疏多项式规范形 `lvExprCanonical`（按总次数降序、同次数字典序排序并合并同类项）。
4. **几何代数（GA）**（`ga_multivector.h`、`ga_interface.h`、`ga_codegen.h`、`gc_language.h`）：基于 Cl(3,0,1) 投影几何代数（PGA），16 维多重向量 `lvMultiVector` 统一编码点/向量/平面/线/转子/电机；`ga_interface.h` 提供标准几何量嵌入/提取；`ga_codegen.h` 将多重向量编译为 C/CPP/CUDA/LaTeX/Python/DOT 六种目标；`gc_language.h` 提供 Lv-00 GC 语言源级解析入口。

**覆盖头文件**：`core/include/lv/formula_parser.h`、`formula_renderer.h`、`math_input.h`、`expr_canonical.h`、`expr_canon.h`、`ga_multivector.h`、`ga_interface.h`、`ga_codegen.h`、`gc_language.h`。

## 核心设计原则

1. **多语法统一 AST**：`formula_detect_syntax` 检测输入语法（`latex`/`python`/`dsl`/`unknown`），三种解析入口（`parse_dsl_compound`、`parse_latex_expression`、`parse_python_expression`）共享同一 `ParserContext` 与 `FormulaNode` 表示，后续渲染与规范化不再区分来源语法。
2. **引用计数生命周期**：`FormulaNode.refcount` 大于 0 才存活，`formula_node_destroy` 仅在引用计数归零时释放并递归递减子节点；多位置共享同一节点时须显式 `formula_node_ref`。
3. **扁平化节点类型**：运算符直接编码在 `NodeType` 枚举中（每个二元/一元运算一个类型），AST 采用 tagged union，减少内存间接层级；约束类型同样一型一码。
4. **解析安全限制**：`ParserContext` 携带 `node_count`（AST 节点计数）与 `current_depth`（递归深度）安全计数；`lv_MAX_ARGUMENTS`(32)、`lv_MAX_COORDINATES`(16)、`lv_MAX_POLYGON_VERTICES`(64)、`lv_MAX_PARTICIPANTS`(16)、`lv_MAX_STATEMENTS`(256) 等编译期上限约束输入规模。
5. **规范表示唯一性**：`lvExprCanonical` 以稀疏多项式为唯一规范形，项按总次数降序、同次数字典序排列；`lv_expr_canonicalize` 合并同类项后，`lv_expr_canonical_equal` 即可判断代数等价。
6. **PGA 统一几何编码**：Cl(3,0,1) 以 16 个基元编码全部几何量——点=三矢（e123 分量）、向量=e1+e2·e03+e3·e03、平面=法向量（e1+e2+e3）、线=双矢（e12+e13+e23）、转子=偶阶（标量+双矢）、电机=偶阶+伪标量；几何构造（过两点直线 P∧Q、共线判定 P∧Q∧R=0 等）全部转化为多重向量外积。
7. **多目标代码生成**：仿 GAALOP 思路，`ga_codegen_compile` 将同一多重向量表达式编译为 C/CPP/CUDA/LaTeX/Python 数值代码或 DOT 表达式树图，`GACodegenOptions.optimize` 开关控制常量折叠等基础优化。

## 关键数据结构

```c
/* formula_parser.h —— AST 节点类型（节选）与 tagged union */
typedef enum {
    NODE_NUMBER, NODE_VARIABLE, NODE_IDENTIFIER,
    NODE_BINARY_OP_ADD, NODE_BINARY_OP_SUB, NODE_BINARY_OP_MUL,
    NODE_BINARY_OP_DIV, NODE_BINARY_OP_POW,
    NODE_UNARY_OP_NEG, NODE_UNARY_OP_SQRT, NODE_UNARY_OP_SIN,
    NODE_UNARY_OP_COS, NODE_UNARY_OP_TAN, NODE_UNARY_OP_ABS,
    NODE_UNARY_OP_LN, NODE_UNARY_OP_LOG,
    NODE_EQUATION, NODE_COORDINATE_LIST,
    NODE_GEOM_POINT, NODE_GEOM_SEGMENT, NODE_GEOM_LINE, NODE_GEOM_CIRCLE,
    NODE_GEOM_TRIANGLE, NODE_GEOM_POLYGON, NODE_GEOM_REGION,
    NODE_GEOM_ARC, NODE_GEOM_VECTOR,
    NODE_CONSTRAINT_PERPENDICULAR, NODE_CONSTRAINT_PARALLEL,
    NODE_CONSTRAINT_MIDPOINT, NODE_CONSTRAINT_BISECTOR,
    NODE_CONSTRAINT_COLLINEAR, NODE_CONSTRAINT_TANGENT,
    NODE_CONSTRAINT_CONGRUENT, NODE_CONSTRAINT_ANGLE,
    NODE_COMPOUND
} NodeType;

struct FormulaNode {
    NodeType type;
    int line, column;
    int refcount;
    union {
        struct { int64_t numerator; uint64_t denominator; bool is_integer; } number;
        struct { char *name; } variable;
        struct { FormulaNode *left; FormulaNode *right; } binary_op;
        struct { FormulaNode *operand; } unary_op;
        struct { FormulaNode *lhs; FormulaNode *rhs; } equation;
        struct { FormulaNode **coords; int coord_count; } coord_list;
        struct { char *name; FormulaNode *coords; } geom_point;
        struct { char *name; FormulaNode *endpoint1; FormulaNode *endpoint2; } geom_segment;
        struct { char *name; FormulaNode **vertices; int vertex_count; } geom_polygon;
        struct { FormulaNode **participants; int participant_count; } constraint;
        struct { FormulaNode **statements; int statement_count; int statement_capacity; } compound;
    } data;
};
```

```c
/* expr_canonical.h —— 树形符号表达式（GMP 有理数） */
typedef enum { EXPR_TYPE_VARIABLE, EXPR_TYPE_RATIONAL, EXPR_TYPE_POWER,
               EXPR_TYPE_PRODUCT, EXPR_TYPE_SUM, EXPR_TYPE_FUNCTION } lvExprType;

typedef struct lvExpr {
    lvExprType type;
    union {
        struct { struct lvExpr *base; struct lvExpr *exponent; } power;
        struct { uint32_t count; struct lvExpr **operands; } composite;
        struct { char *name; } variable;
        struct { mpq_t value; } rational;
        struct { char *func_name; struct lvExpr *argument; } function;
    } data;
    char *label;
} lvExpr;
```

```c
/* expr_canon.h —— 稀疏多项式规范形 */
typedef struct {
    lvRational *coeff; /* 系数 */
    int *exponents;    /* 各变量指数数组，长度 var_count */
    int var_count;
} lvExprTerm;

typedef struct {
    lvExprTerm *terms;  /* 项数组 */
    int term_count;
    int term_capacity;
    int var_count;
    char **var_names;   /* 变量名数组（可选） */
    bool canonicalized; /* 是否已规范化 */
} lvExprCanonical;
```

```c
/* ga_multivector.h —— Cl(3,0,1) 多重向量（16 基） */
#define GA_MV_DIM 16
typedef struct lvMultiVector lvMultiVector; /* 不透明类型 */

/* ga_codegen.h —— 代码生成选项与结果 */
typedef enum GACodegenTarget {
    GA_CODEGEN_C, GA_CODEGEN_CPP, GA_CODEGEN_CUDA,
    GA_CODEGEN_LATEX, GA_CODEGEN_PYTHON, GA_CODEGEN_DOT
} GACodegenTarget;

typedef struct GACodegenOptions {
    GACodegenTarget target;
    const char *variable_name; /* 生成代码中的输出变量名 */
    const char *indent;        /* 缩进串 */
    int include_header;        /* 是否包含头注释 */
    int optimize;              /* 是否启用常量折叠优化 */
} GACodegenOptions;

typedef struct GACodegenResult {
    char *code;        /* 生成的源码串 */
    char *error_msg;   /* 错误信息（成功为 NULL） */
    GACodegenTarget target;
    int line_count;
} GACodegenResult;
```

## 主要接口

| 子系统 | 函数签名 | 说明 |
|--------|----------|------|
| 解析 | `FormulaNode *formula_parse(const char *input, const char *syntax)` | 解析输入（syntax: auto/latex/python/dsl）构建 AST |
| 解析 | `const char *formula_detect_syntax(const char *input)` | 检测语法类型 |
| 解析 | `const char *formula_parser_get_last_error(void)` | 最近解析错误信息 |
| AST | `void formula_node_destroy(FormulaNode *node)`；`int formula_node_ref(FormulaNode *node)`；`FormulaNode *formula_node_copy(const FormulaNode *node)` | 引用计数生命周期与深拷贝 |
| AST 构造 | `FormulaNode *formula_create_number / _variable / _identifier / _binary_op / _unary_op / _equation / _coord_list(...)` | 基础节点构造 |
| AST 构造 | `FormulaNode *formula_create_geom_point / _segment / _circle / _triangle / _polygon / _region / _arc / _constraint / _compound(...)` | 几何对象与约束节点构造 |
| AST 构造 | `int formula_compound_add_statement(FormulaNode *compound, FormulaNode *statement)` | 向复合语句追加语句 |
| 渲染 | `char *formula_render_ex(const FormulaNode *node, OutputFormat format, const RenderOptions *options)` | 通用渲染（六格式） |
| 渲染 | `char *formula_render_latex / _python / _dsl(const FormulaNode *node)` | 格式特定快捷渲染 |
| 渲染 | `const char *formula_latex_greek_name(const char *name)` | 希腊字母 → LaTeX 命令 |
| 输入归一 | `int lv_math_input_parse(const char *input, char *normalized, size_t buf_size)` | 数学输入规范化 |
| 输入归一 | `int lv_math_input_detect_format(const char *input)` | 输入格式探测 |
| 规范形 | `lvExpr *lv_expr_create_variable / _rational / _rational_mpq / _power / _mul / _add / _sum_n / _product_n / _function(...)` | lvExpr 树形构造 |
| 规范形 | `void lv_expr_destroy(lvExpr **expr)`；`lvExpr *lv_expr_copy(const lvExpr *expr)`；`bool lv_expr_is_constant(const lvExpr *expr)` | lvExpr 生命周期与查询 |
| 规范形 | `lvExprCanonical *lv_expr_canonical_create(int var_count, const char **var_names)`；`bool lv_expr_canonical_add_term(lvExprCanonical *expr, const lvRational *coeff, const int *exponents)` | 稀疏多项式构造 |
| 规范形 | `bool lv_expr_canonicalize(lvExprCanonical *expr)`；`bool lv_expr_is_canonical(const lvExprCanonical *expr)`；`int lv_canonical_compare_terms(const int *a, const int *b, int var_count)` | 排序/合并/比较规则 |
| 规范形 | `lvExprCanonical *lv_expr_canonical_add / _sub / _mul / _scale / _neg(...)` | 多项式算术 |
| 规范形 | `bool lv_expr_canonical_equal(...)`；`bool lv_expr_canonical_is_zero(...)`；`int lv_expr_canonical_degree(...)`；`char *lv_expr_canonical_to_string(...)` | 查询与字符串化 |
| GA 基元 | `lvMultiVector *ga_mv_create(void)`；`void ga_mv_destroy(lvMultiVector *mv)`；`double ga_mv_get(const lvMultiVector *mv, int index)`；`void ga_mv_set(lvMultiVector *mv, int index, double value)` | 生命周期与系数访问（索引 0..15） |
| GA 运算 | `lvMultiVector *ga_mv_add / _sub / _scale / _negate / _geometric_product / _outer_product / _reverse / _normalize / _dual / _sandwich(...)` | 多重向量运算（调用者拥有结果） |
| GA 运算 | `double ga_mv_inner_product(...)`；`int ga_mv_grade(const lvMultiVector *mv)`；`lvMultiVector *ga_mv_grade_project(const lvMultiVector *mv, int grade)` | 内积与阶投影 |
| GA 嵌入 | `lvMultiVector *ga_embed_point(double x, double y, double z)`；`ga_embed_vector / _plane / _ray / _rotation / _translation(...)` | 标准几何量 → PGA 多重向量 |
| GA 提取 | `int ga_extract_point / _vector / _plane / _ray / _rotation(const lvMultiVector *mv, ...)` | PGA 多重向量 → 标准几何量 |
| GA 构造 | `lvMultiVector *ga_line_from_two_points(...)`；`lvMultiVector *ga_plane_from_three_points(...)`；`bool ga_three_points_collinear(...)`；`bool ga_four_points_coplanar(...)` | 点构造直线/平面与退化判定 |
| GA 代码生成 | `lv_PUBLIC_API GACodegenResult *ga_codegen_compile(const lvMultiVector *mv, const GACodegenOptions *options)` | 多重向量 → 目标语言代码 |
| GA 代码生成 | `lv_PUBLIC_API void ga_codegen_result_destroy(GACodegenResult *result)`；`char *ga_render_latex(const lvMultiVector *mv)`；`char *ga_render_dot(const lvMultiVector *mv)` | 结果释放与 LaTeX/DOT 渲染 |
| GC 语言 | `int lv_gc_parse(const char *source, void *engine)`；`const char *lv_gc_error(void)`；`int lv_gc_command_count(void)` | GC 源解析、错误与命令计数 |

## 工作流程

1. **公式 DSL 流水线**：`formula_parse(input, "auto")` 先经 `formula_detect_syntax` 判定语法，再路由到 `parse_dsl_compound` / `parse_latex_expression` / `parse_python_expression`；解析器通过 `formula_skip_whitespace` / `formula_peek` / `formula_consume` 等游标原语推进，`formula_track_node` 记录行列号并初始化引用计数；顶层 `formula_create_compound` 汇总语句后输出 AST。
2. **渲染**：AST 按 `OutputFormat` 分派到各渲染器；`RenderOptions` 控制隐式乘法、`display_mode`、分数模式与输出精度，`simplify_output` 复用规范化结果精简表达式；渲染错误经 `formula_render_get_last_error` 取回（线程局部）。
3. **表达式规范化**：`FormulaNode` 数值子节点（`int64_t numerator` + `uint64_t denominator`）先转为 `lvExpr` 树（GMP `mpq_t` 精确有理数），再映射为 `lvExprCanonical` 稀疏多项式；`lv_expr_canonical_add_term` 逐项注入后 `lv_expr_canonicalize` 完成排序（`lv_canonical_compare_terms` 按总次数降序 + 字典序）与同类项合并，供代数等价判定与字符串化使用。
4. **GA 构造与代码生成**：几何量经 `ga_embed_*` 嵌入 Cl(3,0,1) 多重向量 → `ga_mv_*` 运算（几何积/外积/夹心积）构造结果 → `ga_codegen_compile` 按 `GACodegenOptions.target` 生成目标语言源码（含头注释与缩进，`optimize` 开启常量折叠）；结果由 `ga_codegen_result_destroy` 释放，表达式树可经 `ga_render_dot` 输出为 Graphviz 图。
5. **GC 语言入口**：`lv_gc_parse(source, engine)` 逐条解析 GC 命令作用于引擎，错误经 `lv_gc_error` 报告，`lv_gc_command_count` 提供已解析命令计数，作为公式 DSL 之外的结构化命令入口。

## 模块关系

| 关联模块 | 编号文档 | 关系说明 |
|----------|----------|----------|
| 合一子系统 | [06_unify.md](06_unify.md) | 表达式等价判定以规范形（`lv_expr_canonical_equal`）为代数等价的判定基，合一过程复用规范化结果减少搜索空间 |
| 解析层 | [37_parsing_layer.md](37_parsing_layer.md) | 公式 DSL 解析器与主解析层（lv_parser）共享安全限制与 `ParserContext` 式游标设计，`dsl_lexer` 与 `formula_parser` 词法能力互补 |
| 规范化子系统 | [03_normalization.md](03_normalization.md) | `expr_canonical` 提供的是表达式级规范形，与引擎归一化（约束规范化、合并同义节点）在语义上分层、在流程上衔接 |
| 输出层 | [18_output_layer.md](18_output_layer.md) | 公式渲染（LaTeX/HTML MathJax）与 GA 的 LaTeX/DOT 渲染是输出层 TikZ/HTML/SVG 导出的内嵌片段来源 |
| Gappa 数值验证 | [33_gappa_verification.md](33_gappa_verification.md) | `ga_codegen` 生成的数值代码（C/Python）与公式渲染的 Python 输出可作为 Gappa 验证的候选证据，衔接符号-数值验证链 |
| 几何层 | [16_geometry_layer.md](16_geometry_layer.md) | `ga_embed_point` 等 PGA 嵌入接口与约束图/符号坐标（symbolic_coord）配合，为几何构造提供统一代数编码 |
| 预设注册表 | [20_preset_registry.md](20_preset_registry.md) | 预设几何对象的公式定义经 `formula_parse` 解析为 AST 后注册，规范化形供预设验证与去重 |

## 版本历史

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.0.0 | 2026-08-10 | 首版：整合公式 DSL 解析/渲染、数学输入、表达式规范化双轨与 PGA 代码生成四子系统，收录 `gc_language.h` 与 `math_input.h` 入口 |
