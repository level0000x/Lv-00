/* ========================================================================
 * 模块名称：DSL 编译器 (dsl_compiler)
 * 功能概述：借鉴 Ganja.js 的 AST 转译和 GCLC 几何构造语言，提供从
 *          DSL 源码到约束图的完整编译管线。包含词法分析、语法解析、
 *          AST 构建、中间表示（IR）生成和约束图填充五个阶段。
 *          支持多种目标平台代码生成。
 *
 * 编译管线：DSL源码 -> Tokenizer -> AST -> IR -> 约束图
 *
 * 主要 API：
 *   - dsl_tokenize                    — 词法分析
 *   - dsl_parse                       — 语法解析（递归下降）
 *   - dsl_compile                     — AST -> IR 编译
 *   - dsl_ir_to_constraint_graph      — IR -> 约束图
 *   - dsl_compile_and_load            — 一键编译并加载
 *   - dsl_ast_dump / dsl_ir_dump      — 调试输出
 *
 * 使用示例：
 *   DslCompileConfig cfg;
 *   dsl_compile_config_default(&cfg);
 *   ConstraintGraph *g = graph_create();
 *   bool ok = dsl_compile_and_load("point A 10 20; line a A B;", &cfg, g);
 *
 * @version 1.1.0
 * ======================================================================== */

/**
 * @file dsl_compiler.h
 * @brief DSL 编译器 —— 借鉴 Ganja.js 的 AST 转译 + GCLC 几何构造语言
 */

#ifndef lv_DSL_COMPILER_H
#define lv_DSL_COMPILER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  前向声明
 * ================================================================ */

typedef struct ConstraintGraph ConstraintGraph;
typedef struct FuncBlock FuncBlock;
typedef struct lvHashtable lvHashtable;

/* ================================================================
 *  第一部分：DSL 词法分析（Tokenizer）
 * ================================================================ */

/**
 * @brief DSL 词法单元类型枚举
 *
 * 覆盖几何构造语言（借鉴 GCLC）和过程化约束 DSL 的全部词法类别。
 * 共 25+ 种类型，支持关键字、标识符、字面量、运算符和分隔符。
 */
typedef enum {
    /* ---- 几何构造原语（GCLC 风格）---- */
    DSL_TOK_POINT,    /**< 关键字 point */
    DSL_TOK_LINE,     /**< 关键字 line */
    DSL_TOK_CIRCLE,   /**< 关键字 circle */
    DSL_TOK_SEGMENT,  /**< 关键字 segment */
    DSL_TOK_RAY,      /**< 关键字 ray */
    DSL_TOK_POLYGON,  /**< 关键字 polygon */
    DSL_TOK_TRIANGLE, /**< 关键字 triangle */

    /* ---- 构造操作 ---- */
    DSL_TOK_INTERSECT,     /**< 关键字 intersect */
    DSL_TOK_PARALLEL,      /**< 关键字 parallel */
    DSL_TOK_PERPENDICULAR, /**< 关键字 perpendicular */
    DSL_TOK_MIDPOINT,      /**< 关键字 midpoint */
    DSL_TOK_CIRCUMCENTER,  /**< 关键字 circumcenter */
    DSL_TOK_ORTHOCENTER,   /**< 关键字 orthocenter */
    DSL_TOK_CENTROID,      /**< 关键字 centroid */
    DSL_TOK_INCENTER,      /**< 关键字 incenter */
    DSL_TOK_BISECTOR,      /**< 关键字 bisector（角平分线） */

    /* ---- 控制流与声明 ---- */
    DSL_TOK_LET,        /**< 关键字 let */
    DSL_TOK_LOAD,       /**< 关键字 load（加载公理包） */
    DSL_TOK_PROVE,      /**< 关键字 prove */
    DSL_TOK_CONSTRAINT, /**< 关键字 constraint */
    DSL_TOK_FIX,        /**< 关键字 fix（固定坐标点） */
    DSL_TOK_FREE,       /**< 关键字 free（自由点） */

    /* ---- 字面量 ---- */
    DSL_TOK_IDENT,  /**< 标识符 */
    DSL_TOK_NUMBER, /**< 数值字面量（整数/浮点） */

    /* ---- 运算符与分隔符 ---- */
    DSL_TOK_LPAREN,   /**< 左圆括号 ( */
    DSL_TOK_RPAREN,   /**< 右圆括号 ) */
    DSL_TOK_LBRACE,   /**< 左花括号 { */
    DSL_TOK_RBRACE,   /**< 右花括号 } */
    DSL_TOK_LBRACKET, /**< 左方括号 [ */
    DSL_TOK_RBRACKET, /**< 右方括号 ] */
    DSL_TOK_COMMA,    /**< 逗号 , */
    DSL_TOK_ASSIGN,   /**< 赋值 = */
    DSL_TOK_SEMI,     /**< 分号 ; */
    DSL_TOK_COLON,    /**< 冒号 : */
    DSL_TOK_ARROW,    /**< 箭头 -> */

    /* ---- 元信息 ---- */
    DSL_TOK_EOF,    /**< 文件结束 */
    DSL_TOK_ERROR,  /**< 错误词法单元 */
    DSL_TOK_COMMENT /**< 注释（行注释 // 和块注释） */
} DSLTokenType;

/**
 * @brief DSL 词法单元
 *
 * 每个词法单元记录其类型、词素文本和源代码位置信息（行列号），
 * 用于编译错误报告和 AST 节点溯源。
 */
typedef struct DslToken {
    DSLTokenType type;  /**< 词法单元类型 */
    const char *lexeme; /**< 词素文本指针（指向源码，非自有） */
    int line;           /**< 源码行号（从 1 开始） */
    int col;            /**< 源码列号（从 1 开始） */
} DslToken;

/* ================================================================
 *  第二部分：抽象语法树（AST）
 * ================================================================ */

/**
 * @brief DSL 抽象语法树节点类型枚举
 *
 * 借鉴 Ganja.js inline AST 分类方式——AST 节点类型直接对应语言构造的语义类别。
 * 共 20+ 种类型，覆盖声明、构造、约束和证明语句。
 */
typedef enum {
    DSL_AST_PROGRAM,       /**< 程序根节点（子节点列表为顶层语句） */
    DSL_AST_POINT_DECL,    /**< 点声明：point A = ... */
    DSL_AST_LINE_DECL,     /**< 线声明：line a = ... */
    DSL_AST_CIRCLE_DECL,   /**< 圆声明：circle k = ... */
    DSL_AST_SEGMENT_DECL,  /**< 线段声明：segment s = ... */
    DSL_AST_RAY_DECL,      /**< 射线声明：ray r = ... */
    DSL_AST_POLYGON_DECL,  /**< 多边形声明：polygon P = ... */
    DSL_AST_TRIANGLE_DECL, /**< 三角形声明：triangle T = ... */
    DSL_AST_INTERSECT,     /**< 求交表达式：intersect(A, B) */
    DSL_AST_PARALLEL,      /**< 平行线表达式：parallel(L, P) */
    DSL_AST_PERPENDICULAR, /**< 垂直线表达式：perpendicular(L, P) */
    DSL_AST_MIDPOINT,      /**< 中点表达式：midpoint(A, B) */
    DSL_AST_CIRCUMCENTER,  /**< 外心表达式：circumcenter(A, B, C) */
    DSL_AST_ORTHOCENTER,   /**< 垂心表达式：orthocenter(A, B, C) */
    DSL_AST_CENTROID,      /**< 重心表达式：centroid(A, B, C) */
    DSL_AST_INCENTER,      /**< 内心表达式：incenter(A, B, C) */
    DSL_AST_BISECTOR,      /**< 角平分线表达式：bisector(A, B, C) */
    DSL_AST_CONSTRAINT,    /**< 约束语句：constraint { ... } */
    DSL_AST_PROVE,         /**< 证明语句：prove { ... } */
    DSL_AST_LOAD,          /**< 加载语句：load "euclidean" */
    DSL_AST_FIX_POINT,     /**< 固定点：fix A 10 20 */
    DSL_AST_FREE_POINT,    /**< 自由点：free A */
    DSL_AST_BLOCK,         /**< 语句块：{ stmt1; stmt2; ... } */
    DSL_AST_IDENT,         /**< 标识符引用 */
    DSL_AST_NUMBER         /**< 数值字面量 */
} DslASTType;

/**
 * @brief DSL 抽象语法树节点
 *
 * 表示编译管线中一步解析产生的语法树节点。
 * 每个节点包含类型标记、可选的标识符名、数值和子节点列表。
 * 借鉴 Ganja.js 的 AST 表示——统一节点结构，通过 type 区分语义。
 */
typedef struct DslAST {
    DslASTType type;          /**< 节点类型 */
    char *name;               /**< 标识符名称（可为 NULL） */
    double num_value;         /**< 数值（DSL_AST_NUMBER 时有效） */
    int line;                 /**< 源码行号 */
    int col;                  /**< 源码列号 */
    struct DslAST **children; /**< 子节点数组 */
    int child_count;          /**< 子节点数量 */
    int child_capacity;       /**< 子节点数组容量 */
} DslAST;

/* ================================================================
 *  第三部分：中间表示（IR）
 *
 * 借鉴 Ganja.js 过程化 API 调用序列：
 * Algebra(2,0,1).point(0,0).point(1,0).line(...) ...
 * ================================================================ */

/**
 * @brief DSL 中间表示操作码枚举
 *
 * 每条 IR 操作对应一个过程化几何构造 API 调用。
 * 借鉴 Ganja.js 的设计：将声明式 DSL 翻译为过程化操作序列，
 * 可进一步编译为 C++/Python/JavaScript/WASM 目标代码。
 * 共 25+ 种操作码。
 */
typedef enum {
    /* ---- 实体创建 ---- */
    IR_CREATE_POINT,       /**< 创建自由点 */
    IR_CREATE_POINT_FIXED, /**< 创建固定坐标点 */
    IR_CREATE_LINE,        /**< 通过两点创建直线 */
    IR_CREATE_CIRCLE,      /**< 通过圆心和半径创建圆 */
    IR_CREATE_SEGMENT,     /**< 通过两个端点创建线段 */
    IR_CREATE_RAY,         /**< 通过原点和方向点创建射线 */
    IR_CREATE_POLYGON,     /**< 通过顶点列表创建多边形 */
    IR_CREATE_TRIANGLE,    /**< 通过三点创建三角形 */

    /* ---- 构造操作（返回新实体）---- */
    IR_INTERSECT,             /**< 计算两几何对象交点 */
    IR_PARALLEL_THROUGH,      /**< 通过点作平行线 */
    IR_PERPENDICULAR_THROUGH, /**< 通过点作垂线 */
    IR_MIDPOINT_OF,           /**< 计算线段中点 */
    IR_CIRCUMCENTER_OF,       /**< 计算三角形外心 */
    IR_ORTHOCENTER_OF,        /**< 计算三角形垂心 */
    IR_CENTROID_OF,           /**< 计算三角形重心 */
    IR_INCENTER_OF,           /**< 计算三角形内心 */
    IR_BISECTOR_OF,           /**< 计算角平分线 */
    IR_ANGLE_BISECTOR,        /**< 计算角平分线交点 */

    /* ---- 约束操作 ---- */
    IR_ADD_CONSTRAINT,          /**< 添加约束（关联/之间/相交/包含） */
    IR_REMOVE_CONSTRAINT,       /**< 移除约束 */
    IR_CONSTRAIN_EQUAL,         /**< 等式约束 */
    IR_CONSTRAIN_PARALLEL,      /**< 平行约束 */
    IR_CONSTRAIN_PERPENDICULAR, /**< 垂直约束 */
    IR_CONSTRAIN_COLLINEAR,     /**< 共线约束 */
    IR_CONSTRAIN_CONCYCLIC,     /**< 共圆约束 */

    /* ---- 系统操作 ---- */
    IR_LOAD_AXIOM, /**< 加载公理包 */
    IR_PROVE,      /**< 启动证明 */
    IR_CHECK_SAT,  /**< 可满足性检查 */

    /* ---- 元操作 ---- */
    IR_LABEL, /**< 为实体添加标签 */
    IR_NOOP   /**< 空操作（占位符） */
} DslIROp;

/**
 * @brief DSL 中间表示操作
 *
 * 表示编译后的单条过程化 API 调用。借鉴 Ganja.js 的过程化操作序列——
 * 每条操作包含操作码、操作数引用和结果引用，
 * 形成从 IR 到可执行 API 调用的直接映射。
 */
typedef struct DslIROperation {
    DslIROp op;        /**< 操作码 */
    int *operands;     /**< 操作数 IR 索引数组 */
    int operand_count; /**< 操作数数量 */
    int result_id;     /**< 结果实体 ID（-1 表示无结果） */
    const char *label; /**< 结果标签（IR_LABEL 时使用，可为 NULL） */
    int source_line;   /**< 对应的源码行号（用于调试） */
} DslIROperation;

/**
 * @brief DSL 编译中间表示
 *
 * 完整的 IR 程序表示，包含有序的操作序列和符号表。
 * 可直接解释执行或进一步编译为目标代码（C++/Python/JS/WASM）。
 */
typedef struct DslIR {
    DslIROperation *operations; /**< IR 操作数组 */
    int op_count;               /**< 操作数量 */
    int op_capacity;            /**< 操作数组容量 */
    char **symbols;             /**< 符号表（ID -> 名称映射） */
    int *symbol_to_ir_id;       /**< 符号到结果 IR ID 的映射 */
    lvHashtable *symbol_index;  /**< 符号名 → 符号下标+1 的哈希索引（O(1) 查找，可为 NULL 回退线性） */
    int symbol_count;           /**< 符号数量 */
    int symbol_capacity;        /**< 符号表容量 */
    int next_id;                /**< 下一个可用的结果实体 ID */
} DslIR;

/* ================================================================
 *  第四部分：编译配置
 * ================================================================ */

/**
 * @brief 目标平台枚举
 *
 * 指定 DSL 编译的输出目标。借鉴 Ganja.js 跨语言代码生成管道，
 * 支持多种后端目标代码生成。
 */
typedef enum {
    TARGET_NATIVE,    /**< 编译为原生 C 代码 */
    TARGET_WASM,      /**< 编译为 WebAssembly */
    TARGET_PYTHON,    /**< 生成 Python 过程化脚本 */
    TARGET_JAVASCRIPT /**< 生成 JavaScript 过程化脚本 */
} DslCompileTarget;

/**
 * @brief DSL 编译配置
 *
 * 控制编译管线的行为，包括优化级别、目标平台和调试选项。
 */
typedef struct DslCompileConfig {
    DslCompileTarget target;  /**< 目标平台 */
    int optimize_level;       /**< 优化级别（0-3, 0=无优化, 3=最大优化） */
    bool debug_ast;           /**< 是否输出 AST 调试信息 */
    bool validate_ir;         /**< 是否对生成的 IR 进行验证 */
    bool generate_source_map; /**< 是否生成源码映射（IR->源码行） */
    int max_iterations;       /**< 约束求解最大迭代次数（0=默认） */
} DslCompileConfig;

/* ================================================================
 *  第五部分：DSL 编译器 API
 *
 *  编译管线：dsl_tokenize -> dsl_parse -> dsl_compile -> dsl_ir_to_constraint_graph
 *  捷径：dsl_compile_and_load（一键完成全部流程）
 * ================================================================ */

/**
 * @brief 对 DSL 源码进行词法分析
 *
 * 将输入字符串拆分为 DslToken 序列。空格和注释被跳过，
 * 错误的字符序列产生 DSL_TOK_ERROR 词法单元。
 *
 * @param source   DSL 源码字符串（以 null 结尾）
 * @param out_tokens 输出：DslToken 数组（[take] 调用者负责释放每一个 token 和数组本身）
 * @param out_count   输出：token 数量
 * @return true 成功，false 遇到致命词法错误
 */
lv_PUBLIC_API bool dsl_tokenize(const char *source, DslToken **out_tokens, int *out_count);

/**
 * @brief 释放词法分析产生的 token 数组
 *
 * @param tokens token 数组
 * @param count  token 数量
 * @note 数组销毁必须携带 count（token 数组不以哨兵终止），
 *       与主流单对象约定 void xxx_destroy(T*) 不同，属数组释放变体。
 */
lv_PUBLIC_API void dsl_tokens_destroy(DslToken *tokens, int count);

/**
 * @brief 将 token 序列解析为抽象语法树
 *
 * 采用递归下降解析算法。解析器将 token 流转换为层次化的 AST 结构。
 *
 * @param tokens   token 数组
 * @param count    token 数量
 * @param out_ast  输出：AST 根节点（调用者通过 dsl_ast_destroy 释放）
 * @return true 成功，false 语法错误
 */
lv_PUBLIC_API bool dsl_parse(const DslToken *tokens, int count, DslAST **out_ast);

/**
 * @brief 将 AST 编译为中间表示（IR）
 *
 * 借鉴 Ganja.js 的 AST 重写技术——遍历 AST 并生成过程化 API 调用序列。
 * 此阶段进行语义分析：符号解析、类型检查和作用域管理。
 *
 * @param ast       输入 AST 根节点
 * @param config    编译配置
 * @param out_ir    输出：编译生成的 IR（调用者通过 dsl_ir_destroy 释放）
 * @return true 成功，false 语义错误
 */
lv_PUBLIC_API bool dsl_compile(const DslAST *ast, const DslCompileConfig *config, DslIR **out_ir);

/**
 * @brief 将 IR 转换为约束图
 *
 * 遍历 IR 操作序列，逐条解释执行，将几何实体和约束填充到约束图中。
 * 这是编译管线的最后一步，IR 中的符号引用被解析为约束图节点。
 *
 * @param ir    编译生成的中间表示
 * @param graph 输出：填充好的约束图（调用者已有所有权）
 * @return true 成功，false IR 中存在无法解析的引用
 */
lv_PUBLIC_API bool dsl_ir_to_constraint_graph(const DslIR *ir, ConstraintGraph *graph);

/**
 * @brief 一键编译并加载：DSL 源码直接生成约束图
 *
 * 等价于 dsl_tokenize + dsl_parse + dsl_compile + dsl_ir_to_constraint_graph 的串联调用。
 * 适合交互式应用场景。
 *
 * @param source  DSL 源码字符串
 * @param config  编译配置
 * @param graph   输出：填充好的约束图
 * @return true 成功，false 编译或加载过程中出错
 */
lv_PUBLIC_API bool dsl_compile_and_load(const char *source, const DslCompileConfig *config, ConstraintGraph *graph);

/**
 * @brief 获取默认编译配置
 *
 * 默认配置：TARGET_NATIVE / optimize_level=1 / debug_ast=false /
 * validate_ir=true / generate_source_map=true / max_iterations=1000
 *
 * @param out_config 输出：填充默认值的编译配置
 */
lv_PUBLIC_API void dsl_compile_config_default(DslCompileConfig *out_config);

/**
 * @brief 释放抽象语法树
 *
 * 递归释放 AST 节点及其所有子节点。
 *
 * @param ast AST 根节点（可为 NULL）
 */
lv_PUBLIC_API void dsl_ast_destroy(DslAST *ast);

/**
 * @brief 释放中间表示
 *
 * 释放 IR 中的操作数组、符号表和所有内部资源。
 *
 * @param ir IR 对象（可为 NULL）
 */
lv_PUBLIC_API void dsl_ir_destroy(DslIR *ir);

/**
 * @brief 将 AST 以可读格式输出到文件描述符
 *
 * 递归打印 AST 树，用于调试和可视化。
 *
 * @param ast AST 根节点
 * @param fd   输出文件描述符（使用 stdout/stderr 或文件句柄）
 * @param indent 缩进级别（根节点使用 0）
 */
lv_PUBLIC_API void dsl_ast_dump(const DslAST *ast, void *fd, int indent);

/**
 * @brief 将 IR 以可读格式输出到文件描述符
 *
 * 逐条打印 IR 操作序列，包含操作码名称、操作数 ID 和结果 ID。
 *
 * @param ir IR 对象
 * @param fd 输出文件描述符
 */
lv_PUBLIC_API void dsl_ir_dump(const DslIR *ir, void *fd);

/**
 * @brief 获取 IR 操作码的字符串名称
 *
 * @param op 操作码
 * @return 静态字符串（不需要释放）
 */
lv_PUBLIC_API const char *dsl_ir_op_name(DslIROp op);

/**
 * @brief 获取 DSL AST 节点类型的字符串名称
 *
 * @param type AST 节点类型
 * @return 静态字符串（不需要释放）
 */
lv_PUBLIC_API const char *dsl_ast_type_name(DslASTType type);

#ifdef __cplusplus
}
#endif

#endif /* lv_DSL_COMPILER_H */
