/* ========================================================================
 * 模块名称：公式解析器 (formula_parser)
 * 功能概述：提供几何元语言系统的 AST 结构与解析 API。支持多种语法格式
 *          （LaTeX、Python、DSL），包含数值、变量、运算符、方程以及
 *          几何对象（点、线段、圆、三角形、直线、多边形、向量、约束）
 *          的表示。使用引用计数管理 AST 节点生命周期。
 *
 * 主要 API：
 *   - formula_detect_syntax          — 检测输入语法类型
 *   - formula_parse                  — 解析公式字符串构建 AST
 *   - formula_node_destroy / ref     — AST 节点生命周期管理
 *   - formula_node_copy              — 深拷贝 AST
 *   - formula_create_number / variable / binary_op / ... — AST 构建辅助
 *   - formula_create_geom_point / segment / circle / ... — 几何对象节点
 *
 * 使用示例：
 *   FormulaNode *ast = formula_parse("A = (1, 2)", "auto");
 *   char *latex = formula_render_latex(ast);
 *   formula_node_destroy(ast);
 *
 * @version 1.1.0
 * ======================================================================== */

/**
 * @file formula_parser.h
 * @brief 公式解析器 —— 几何元语言系统的 AST 结构与解析 API
 */

#ifndef lv_FORMULA_PARSER_H
#define lv_FORMULA_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ============================================================
 * AST 节点类型枚举 (扁平设计，运算符编码在类型中)
 * ============================================================ */

typedef enum {
    /* 基本类型 */
    NODE_NUMBER,     /* 数值常量 (有理数) */
    NODE_VARIABLE,   /* 变量 */
    NODE_IDENTIFIER, /* 标识符 (函数名等) */

    /* 二元运算 (每种运算符一个类型) */
    NODE_BINARY_OP_ADD, /* 加法 + */
    NODE_BINARY_OP_SUB, /* 减法 - */
    NODE_BINARY_OP_MUL, /* 乘法 * */
    NODE_BINARY_OP_DIV, /* 除法 / */
    NODE_BINARY_OP_POW, /* 幂运算 ^ */

    /* 一元运算 (每种运算符一个类型) */
    NODE_UNARY_OP_NEG,  /* 取负 - */
    NODE_UNARY_OP_SQRT, /* 平方根 sqrt */
    NODE_UNARY_OP_SIN,  /* 正弦 sin */
    NODE_UNARY_OP_COS,  /* 余弦 cos */
    NODE_UNARY_OP_TAN,  /* 正切 tan */
    NODE_UNARY_OP_ABS,  /* 绝对值 abs */
    NODE_UNARY_OP_LN,   /* 自然对数 ln */
    NODE_UNARY_OP_LOG,  /* 常用对数 log */

    /* 方程与坐标 */
    NODE_EQUATION,        /* 方程 lhs = rhs */
    NODE_COORDINATE_LIST, /* 坐标列表 (x, y, ...) */

    /* 几何对象 */
    NODE_GEOM_POINT,    /* 几何点 point A(x, y) */
    NODE_GEOM_SEGMENT,  /* 几何线段 segment AB(A, B) */
    NODE_GEOM_LINE,     /* 几何直线 line l(A, B, eq) */
    NODE_GEOM_CIRCLE,   /* 几何圆 circle O(center, radius, eq) */
    NODE_GEOM_TRIANGLE, /* 几何三角形 triangle ABC(A, B, C) */
    NODE_GEOM_POLYGON,  /* 几何多边形 polygon P(v1, v2, ...) */
    NODE_GEOM_REGION,   /* 几何区域 region R(seg1, seg2, ...) */
    NODE_GEOM_ARC,      /* 几何弧 arc A(center, radius, start_angle, end_angle) */
    NODE_GEOM_VECTOR,   /* 几何向量 vector v(start, end) */

    /* 几何约束 (每种约束一个类型) */
    NODE_CONSTRAINT_PERPENDICULAR, /* 垂直约束 */
    NODE_CONSTRAINT_PARALLEL,      /* 平行约束 */
    NODE_CONSTRAINT_MIDPOINT,      /* 中点约束 */
    NODE_CONSTRAINT_BISECTOR,      /* 角平分线约束 */
    NODE_CONSTRAINT_COLLINEAR,     /* 共线约束 */
    NODE_CONSTRAINT_TANGENT,       /* 切线约束 */
    NODE_CONSTRAINT_CONGRUENT,     /* 全等约束 */
    NODE_CONSTRAINT_ANGLE,         /* 角度约束 ∠ABC = θ */

    /* 复合语句 */
    NODE_COMPOUND /* 复合语句 (分号分隔) */
} NodeType;

/* ============================================================
 * 几何约束 枚举↔名称 单一事实源（判据 D）
 * ============================================================
 * 8 个约束名与 NODE_CONSTRAINT_* 枚举一一对应，顺序与上文枚举一致。
 * 各消费方（DSL 解析查表、字符串/ASCII 渲染）通过本 X-macro 派生，
 * 避免新增约束时名称列与枚举失步。
 *
 * X(枚举, "名称", 小写标识符)
 */
#define LV_CONSTRAINT_NAME_X(X) \
    X(NODE_CONSTRAINT_PERPENDICULAR, "perpendicular", perpendicular) \
    X(NODE_CONSTRAINT_PARALLEL, "parallel", parallel) \
    X(NODE_CONSTRAINT_MIDPOINT, "midpoint", midpoint) \
    X(NODE_CONSTRAINT_BISECTOR, "bisector", bisector) \
    X(NODE_CONSTRAINT_COLLINEAR, "collinear", collinear) \
    X(NODE_CONSTRAINT_TANGENT, "tangent", tangent) \
    X(NODE_CONSTRAINT_CONGRUENT, "congruent", congruent) \
    X(NODE_CONSTRAINT_ANGLE, "angle", angle)

/* ============================================================
 * AST 节点结构体 (tagged union)
 * ============================================================ */

typedef struct FormulaNode FormulaNode;

struct FormulaNode {
    NodeType type;
    int line;     /* 源码行号 */
    int column;   /* 源码列号 */
    int refcount; /* 引用计数：>0 表示存活，0 时方可释放 */
    union {
        /* NUMBER: 有理数 */
        struct {
            int64_t numerator;
            uint64_t denominator;
            bool is_integer;
        } number;

        /* VARIABLE: 变量名 (堆分配) */
        struct {
            char *name;
        } variable;

        /* IDENTIFIER: 标识符名 (堆分配) */
        struct {
            char *name;
        } identifier;

        /* 二元运算: left op right (op 编码在 NodeType 中) */
        struct {
            FormulaNode *left;
            FormulaNode *right;
        } binary_op;

        /* 一元运算: op operand (op 编码在 NodeType 中) */
        struct {
            FormulaNode *operand;
        } unary_op;

        /* EQUATION: lhs = rhs */
        struct {
            FormulaNode *lhs;
            FormulaNode *rhs;
        } equation;

        /* COORDINATE_LIST: 坐标列表 */
        struct {
            FormulaNode **coords;
            int coord_count;
        } coord_list;

        /* GEOM_POINT: 几何点 */
        struct {
            char *name;
            FormulaNode *coords;
        } geom_point;

        /* GEOM_SEGMENT: 几何线段 */
        struct {
            char *name;
            FormulaNode *endpoint1;
            FormulaNode *endpoint2;
        } geom_segment;

        /* GEOM_LINE: 几何直线 */
        struct {
            char *name;
            FormulaNode *point1;
            FormulaNode *point2;
            FormulaNode *equation;
        } geom_line;

        /* GEOM_CIRCLE: 几何圆 */
        struct {
            char *name;
            FormulaNode *center;
            FormulaNode *radius;
            FormulaNode *equation;
        } geom_circle;

        /* GEOM_TRIANGLE: 几何三角形 */
        struct {
            char *name;
            FormulaNode *vertex1;
            FormulaNode *vertex2;
            FormulaNode *vertex3;
        } geom_triangle;

        /* GEOM_POLYGON: 几何多边形 */
        struct {
            char *name;
            FormulaNode **vertices;
            int vertex_count;
        } geom_polygon;

        /* GEOM_REGION: 几何区域 */
        struct {
            char *name;
            FormulaNode **boundary_segments;
            int segment_count;
        } geom_region;

        /* GEOM_ARC: 几何弧 */
        struct {
            char *name;
            FormulaNode *center;
            FormulaNode *radius;
            FormulaNode *start_angle;
            FormulaNode *end_angle;
        } geom_arc;

        /* GEOM_VECTOR: 几何向量 */
        struct {
            char *name;
            FormulaNode *start;
            FormulaNode *end;
        } geom_vector;

        /* 约束: 参与者列表 */
        struct {
            FormulaNode **participants;
            int participant_count;
        } constraint;

        /* COMPOUND: 复合语句 */
        struct {
            FormulaNode **statements;
            int statement_count;
            int statement_capacity;
        } compound;
    } data;
};

/* ============================================================
 * 公共 API —— 解析
 * ============================================================ */

/**
 * @brief 检测输入字符串的语法类型
 *
 * @param[in] input 输入的公式字符串
 * @return 语法类型字符串: "latex", "python", "dsl", "unknown"
 */
const char *formula_detect_syntax(const char *input);

/**
 * @brief 解析器上下文结构体
 *
 * 用于在解析过程中维护状态信息，包括输入位置、错误处理等。
 */
typedef struct {
    const char *input;       /* 输入字符串 */
    size_t pos;              /* 当前位置 */
    size_t length;           /* 输入长度 */
    char error_message[256]; /* 错误消息缓冲区 */
    bool has_error;          /* 是否有错误 */
    int line;                /* 当前行号 */
    int column;              /* 当前列号 */
    int node_count;          /* AST节点计数（安全限制用） */
    int current_depth;       /* 当前解析递归深度 */
} ParserContext;

/**
 * @brief 解析公式字符串，构建 AST
 *
 * @param[in] input  输入的公式字符串
 * @param[in] syntax 语法类型 ("auto", "latex", "python", "dsl")
 * @return AST 根节点，需要调用 formula_node_destroy 释放；失败返回 NULL
 */
FormulaNode *formula_parse(const char *input, const char *syntax);

/**
 * @brief 获取最后一次解析错误信息
 *
 * @return 错误信息字符串，无错误时返回 NULL
 */
const char *formula_parser_get_last_error(void);

/* ============================================================
 * 公共 API —— AST 节点管理
 * ============================================================ */

/**
 * @brief 销毁 AST 节点及其所有子节点
 *
 * 使用引用计数安全释放：递减引用计数，仅当 refcount=0 时才释放内存。
 * 递归释放所有子节点（子节点引用计数也相应递减）。
 *
 * @param[in] node 要销毁的 AST 节点
 */
void formula_node_destroy(FormulaNode *node);

/**
 * @brief 增加 AST 节点引用计数
 *
 * 当多个位置持有同一节点的指针时应调用此函数，
 * 以确保节点不会因某处的 formula_node_destroy 而提前释放。
 *
 * @param[in] node AST节点指针（不能为NULL）
 * @return 增加后的引用计数值
 */
int formula_node_ref(FormulaNode *node);

/**
 * @brief 获取 AST 节点当前引用计数
 *
 * @param[in] node AST节点指针
 * @return 当前引用计数，node为NULL时返回0
 */
int formula_node_refcount(const FormulaNode *node);

/**
 * @brief 深拷贝 AST 节点
 *
 * @param[in] node 要复制的 AST 节点
 * @return 新的 AST 节点，需要调用 formula_node_destroy 释放；失败返回 NULL
 */
FormulaNode *formula_node_copy(const FormulaNode *node);

/* ============================================================
 * AST 构建辅助函数
 * ============================================================ */

/**
 * @brief 创建数值常量节点
 *
 * @param[in] numerator   分子
 * @param[in] denominator 分母
 * @return 新的 AST 节点，失败返回 NULL
 */
FormulaNode *formula_create_number(int64_t numerator, uint64_t denominator);

/**
 * @brief 创建变量节点
 *
 * @param[in] name 变量名
 * @return 新的 AST 节点，失败返回 NULL
 */
FormulaNode *formula_create_variable(const char *name);

/**
 * @brief 创建标识符节点
 *
 * @param[in] name 标识符名
 * @return 新的 AST 节点，失败返回 NULL
 */
FormulaNode *formula_create_identifier(const char *name);

/**
 * @brief 创建二元运算节点
 *
 * @param[in] op    运算符类型 (NODE_BINARY_OP_ADD 等)
 * @param[in] left  左操作数
 * @param[in] right 右操作数
 * @return 新的 AST 节点，失败返回 NULL
 */
FormulaNode *formula_create_binary_op(NodeType op, FormulaNode *left, FormulaNode *right);

/**
 * @brief 创建一元运算节点
 *
 * @param[in] op      运算符类型 (NODE_UNARY_OP_NEG 等)
 * @param[in] operand 操作数
 * @return 新的 AST 节点，失败返回 NULL
 */
FormulaNode *formula_create_unary_op(NodeType op, FormulaNode *operand);

/**
 * @brief 创建方程节点
 *
 * @param[in] lhs 左侧表达式
 * @param[in] rhs 右侧表达式
 * @return 新的 AST 节点，失败返回 NULL
 */
FormulaNode *formula_create_equation(FormulaNode *lhs, FormulaNode *rhs);

/**
 * @brief 创建坐标列表节点
 *
 * @param[in] coords 坐标节点数组
 * @param[in] count  坐标数量
 * @return 新的 AST 节点，失败返回 NULL
 */
FormulaNode *formula_create_coord_list(FormulaNode **coords, int count);

/**
 * @brief 创建几何点节点
 *
 * @param[in] name   点名称
 * @param[in] coords 坐标列表节点 (NODE_COORDINATE_LIST)
 * @return 新的 AST 节点，失败返回 NULL
 */
FormulaNode *formula_create_geom_point(const char *name, FormulaNode *coords);

/**
 * @brief 创建几何线段节点
 *
 * @param[in] name 线段名称
 * @param[in] ep1  第一个端点
 * @param[in] ep2  第二个端点
 * @return 新的 AST 节点，失败返回 NULL
 */
FormulaNode *formula_create_geom_segment(const char *name, FormulaNode *ep1, FormulaNode *ep2);

/**
 * @brief 创建几何圆节点
 *
 * @param[in] name   圆名称
 * @param[in] center 圆心
 * @param[in] radius 半径
 * @return 新的 AST 节点，失败返回 NULL
 */
FormulaNode *formula_create_geom_circle(const char *name, FormulaNode *center, FormulaNode *radius);

/**
 * @brief 创建几何三角形节点
 *
 * @param[in] name 三角形名称
 * @param[in] v1   第一个顶点
 * @param[in] v2   第二个顶点
 * @param[in] v3   第三个顶点
 * @return 新的 AST 节点，失败返回 NULL
 */
FormulaNode *formula_create_geom_triangle(const char *name, FormulaNode *v1, FormulaNode *v2, FormulaNode *v3);

/**
 * @brief 创建几何多边形节点
 *
 * @param[in] name         多边形名称
 * @param[in] vertices     顶点节点数组
 * @param[in] vertex_count 顶点数量
 * @return 新的 AST 节点，失败返回 NULL
 */
FormulaNode *formula_create_geom_polygon(const char *name, FormulaNode **vertices, int vertex_count);

/**
 * @brief 创建几何区域节点
 *
 * @param[in] name          区域名称
 * @param[in] segments      边界线段节点数组
 * @param[in] segment_count 线段数量
 * @return 新的 AST 节点，失败返回 NULL
 */
FormulaNode *formula_create_geom_region(const char *name, FormulaNode **segments, int segment_count);

/**
 * @brief 创建几何弧节点
 *
 * @param[in] name        弧名称
 * @param[in] center      圆心
 * @param[in] radius      半径
 * @param[in] start_angle 起始角度
 * @param[in] end_angle   结束角度
 * @return 新的 AST 节点，失败返回 NULL
 */
FormulaNode *formula_create_geom_arc(const char *name, FormulaNode *center, FormulaNode *radius,
                                     FormulaNode *start_angle, FormulaNode *end_angle);

/**
 * @brief 创建约束节点
 *
 * @param[in] type         约束类型 (NODE_CONSTRAINT_PERPENDICULAR 等)
 * @param[in] participants 参与者节点数组
 * @param[in] count        参与者数量
 * @return 新的 AST 节点，失败返回 NULL
 */
FormulaNode *formula_create_constraint(NodeType type, FormulaNode **participants, int count);

/**
 * @brief 创建复合语句节点
 *
 * @param[in] statements      语句数组
 * @param[in] statement_count 语句数量
 * @return 新的 AST 节点，失败返回 NULL
 */
FormulaNode *formula_create_compound(FormulaNode **statements, int count);

/**
 * @brief 向复合语句添加一条语句
 *
 * @param[in] compound  复合语句节点
 * @param[in] statement 要添加的语句
 * @return 成功返回 0，失败返回 -1
 */
int formula_compound_add_statement(FormulaNode *compound, FormulaNode *statement);

/* ============================================================
 * 解析器内部辅助函数 —— 跨编译单元共享
 *
 * 以下函数在 formula_parser.c 中定义，供 formula_dsl.c、
 * formula_latex.c、formula_python.c 等子模块使用。
 * ============================================================ */

/** @brief 跳过空白字符和注释 */
void formula_skip_whitespace(ParserContext *ctx);
/** @brief 查看当前字符（不消费） */
char formula_peek(ParserContext *ctx);
/** @brief 查看下一个字符（不消费） */
char formula_peek_next(ParserContext *ctx);
/** @brief 消费当前字符 */
char formula_consume(ParserContext *ctx);
/** @brief 期望并消费指定字符 */
bool formula_expect_char(ParserContext *ctx, char c);
/** @brief 设置解析错误信息 */
void formula_set_error(ParserContext *ctx, const char *msg);
/** @brief 检查是否到达输入末尾 */
bool formula_is_at_end(ParserContext *ctx);
/** @brief 检查字符是否为字母或下划线 */
bool formula_is_alpha(char c);
/** @brief 检查字符是否为字母、数字或下划线 */
bool formula_is_alnum(char c);
/** @brief 检查字符是否为十进制数字 */
bool formula_is_digit(char c);
/** @brief 检查当前位置是否匹配字符串（不消费） */
bool formula_match_string(ParserContext *ctx, const char *str);
/** @brief 匹配并消费字符串 */
bool formula_match_and_consume(ParserContext *ctx, const char *str);
/** @brief 跟踪节点（设置行列号、引用计数） */
FormulaNode *formula_track_node(ParserContext *ctx, FormulaNode *node);
/** @brief 解析数字字面量 */
FormulaNode *formula_parse_number(ParserContext *ctx);
/** @brief 解析标识符字符串 */
char *formula_parse_identifier_str(ParserContext *ctx);

/** @brief 数学函数分发表条目（formula_dsl / formula_python 共享，替代两处重复 typedef） */
typedef struct {
    const char *name;   /**< 函数名 */
    int arg_count;      /**< 期望的参数个数 */
    NodeType op;        /**< 对应运算符节点类型 */
    bool is_binary;     /**< true=二元运算，false=一元运算 */
} MathFuncEntry;

/** @brief 按函数名查表创建数学函数节点（formula_dsl.c 定义，formula_dsl/formula_python 共享） */
FormulaNode *formula_apply_math_func(const char *ident, FormulaNode **args, int arg_count,
                                     const MathFuncEntry *table, size_t table_size);

/** @brief DSL 关键字表（NULL 终止） */
extern const char *formula_dsl_keywords[];
/** @brief LaTeX 命令表（NULL 终止） */
extern const char *formula_latex_commands[];
/** @brief Python 特征表（NULL 终止） */
extern const char *formula_python_features[];

/** @brief DSL 语法顶层解析入口 */
FormulaNode *parse_dsl_compound(ParserContext *ctx);
/** @brief LaTeX 语法顶层解析入口 */
FormulaNode *parse_latex_expression(ParserContext *ctx);
/** @brief Python 语法顶层解析入口 */
FormulaNode *parse_python_expression(ParserContext *ctx);

/** @brief 函数参数最大数量 */
#ifndef lv_MAX_ARGUMENTS
#define lv_MAX_ARGUMENTS 32
#endif

/** @brief 坐标最大维度（编译期数组维度，须 ≥ lvConfig.parser.parser_max_coordinates 默认值 16） */
#ifndef lv_MAX_COORDINATES
#define lv_MAX_COORDINATES 16
#endif

/** @brief 多边形最大顶点数 */
#ifndef lv_MAX_POLYGON_VERTICES
#define lv_MAX_POLYGON_VERTICES 64
#endif

/** @brief 临时消息缓冲区大小 */
#ifndef lv_MAX_TEMP_MSG_SIZE
#define lv_MAX_TEMP_MSG_SIZE 512
#endif

/** @brief 约束最大参与者数 */
#ifndef lv_MAX_PARTICIPANTS
#define lv_MAX_PARTICIPANTS 16
#endif

/** @brief 复合语句最大数量 */
#ifndef lv_MAX_STATEMENTS
#define lv_MAX_STATEMENTS 256
#endif

/* 注：解析器内部辅助函数统一使用 formula_ 前缀（formula_peek、formula_consume 等），
 * 不再提供无前缀兼容宏，避免污染包含本头文件的翻译单元。 */

#ifdef __cplusplus
}
#endif

#endif /* lv_FORMULA_PARSER_H */
