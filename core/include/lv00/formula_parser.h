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
 * @version 3.3.0
 * ======================================================================== */

/**
 * @file formula_parser.h
 * @brief 公式解析器 —— 几何元语言系统的 AST 结构与解析 API
 */

#ifndef LV00_FORMULA_PARSER_H
#define LV00_FORMULA_PARSER_H

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

/**
 * @brief 预解析输入验证
 *
 * 在正式解析之前对输入进行结构级验证：NULL/空检查、括号匹配、
 * 过长输入警告（> 4096 字符）、非法字符检测。
 *
 * @param[in] input 输入的公式字符串
 * @return 0 验证通过，非零为错误码
 */
int formula_validate_input(const char *input);

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
 * @brief 递归销毁整棵 AST 树（便捷接口）
 *
 * 提供单次调用清理整棵解析树。不使用引用计数，
 * 直接递归释放所有子节点后释放根节点。
 * 适用于 formula_parse 返回的 AST 根节点的最终清理。
 *
 * @param[in] root AST 根节点，可以为 NULL
 */
void formula_ast_destroy(FormulaNode *root);

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

#ifdef __cplusplus
}
#endif

#endif /* LV00_FORMULA_PARSER_H */
