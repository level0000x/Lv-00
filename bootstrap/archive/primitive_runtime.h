/*
 * primitive_runtime.h -- 13原语IR运行时头文件
 *
 * 使用 GMP (GNU Multiple Precision) mpq_t 精确有理数
 * 替代所有 double 浮点数，确保几何计算零误差。
 */

#ifndef PRIMITIVE_RUNTIME_H
#define PRIMITIVE_RUNTIME_H

#include <gmp.h>
#include <stddef.h>  /* size_t */

/* ================================================================
 * 硬编码容量限制
 * ================================================================ */
#define KN  2048   /* 最大节点数 */
#define KC  8192   /* 最大约束数 */
#define KB  256    /* 最大块指令数 */

/* ================================================================
 * 13 条原语 IR 操作码枚举
 * ================================================================ */
typedef enum {
    OP_CREATE_NODE      = 0,   /* 创建节点（含精确有理坐标） */
    OP_CREATE_CONSTRAINT = 1,  /* 创建约束（含精确有理数值） */
    OP_SOLVE            = 2,   /* 求解 / 验证约束满足 */
    OP_NORMALIZE        = 3,   /* 归一化坐标 */
    OP_REWRITE          = 4,   /* 重写 / 变换图 */
    OP_UNIFY            = 5,   /* 合一两个节点 */
    OP_PACK             = 6,   /* 紧凑化图 */
    OP_INSTANTIATE      = 7,   /* 实例化模板 */
    OP_PROVE            = 8,   /* 证明命题 */
    OP_EXPORT           = 9,   /* 导出图到文件 */
    OP_SERIALIZE        = 10,  /* 序列化为字符串 */
    OP_DESERIALIZE      = 11,  /* 从字符串反序列化 */
    OP_QUERY            = 12   /* 查询图信息 */
} KPrimitive;

/* ================================================================
 * 导出格式枚举
 * ================================================================ */
typedef enum {
    FORMAT_JSON  = 0,   /* JSON 格式（含精确有理数字符串） */
    FORMAT_LATEX = 1,   /* LaTeX (tikz/pgf) 格式 */
    FORMAT_LEAN4 = 2,   /* Lean 4 定理声明 */
    FORMAT_COQ   = 3,   /* Coq 定理声明 */
    FORMAT_DOT   = 4    /* Graphviz DOT 格式 */
} KExportFormat;

/* ================================================================
 * 数据结构
 * ================================================================ */

/*
 * KNode -- 几何节点
 *
 * 使用 mpq_t 存储精确有理坐标 (x, y, z)。
 * mpq_t 是 GMP 有理数类型，内部以既约分数形式存储。
 */
typedef struct {
    int    id;            /* 节点唯一标识 */
    int    type;          /* 节点类型（用户自定义） */
    char   name[32];      /* 节点名称（可选标签） */
    mpq_t  x, y, z;       /* 精确有理坐标（GMP mpq_t） */
} KNode;

/*
 * KConstraint -- 几何约束
 *
 * 约束表示节点之间的几何关系。
 * 如距离约束: |dx^2 + dy^2 + dz^2 - value| < eps
 * relation: 0=EQ(相等), 1=LE(小于等于), 2=GE(大于等于)
 */
typedef struct {
    int    id;            /* 约束唯一标识 */
    int    type;          /* 约束类型（用户自定义） */
    int    node_a;        /* 首节点索引 */
    int    node_b;        /* 次节点索引 */
    mpq_t  value;         /* 约束精确有理数值（GMP mpq_t） */
    int    relation;      /* 关系: 0=EQ, 1=LE, 2=GE */
} KConstraint;

/*
 * KGraph -- 知识图
 *
 * 包含节点数组和约束数组，以及当前计数。
 * 数组大小由硬编码常量 KN, KC 确定。
 */
typedef struct {
    KNode       nodes[KN];            /* 节点数组 */
    KConstraint constraints[KC];      /* 约束数组 */
    int         node_count;           /* 当前有效节点数 */
    int         constraint_count;     /* 当前有效约束数 */
    char        name[64];             /* 图名称 */
} KGraph;

/*
 * KBlock -- 指令块
 *
 * 包含一系列原语操作码索引，用于 REWRITE 和 INSTANTIATE。
 */
typedef struct {
    int ops[KB];      /* 操作码序列 */
    int op_count;     /* 当前操作数 */
} KBlock;

/* ================================================================
 * 13 原语函数声明
 * 所有数值参数以十进制字符串形式传入，内部用 mpq_set_str 解析。
 * ================================================================ */

/* OP 0: 创建节点 */
int kg_create_node(KGraph *g, int type, const char *name,
                   const char *x_str, const char *y_str, const char *z_str);

/* OP 1: 创建约束 */
int kg_create_constraint(KGraph *g, int type, int node_a, int node_b,
                         const char *value_str, int relation);

/* OP 2: 求解 / 验证约束 */
int kg_solve(KGraph *g, const char *eps_str);

/* OP 3: 归一化坐标 */
int kg_normalize(KGraph *g);

/* OP 4: 重写图 */
int kg_rewrite(KGraph *g, KBlock *block);

/* OP 5: 合一节点 */
int kg_unify(KGraph *g, int node_a, int node_b);

/* OP 6: 紧凑化图 */
int kg_pack(KGraph *g);

/* OP 7: 实例化模板 */
int kg_instantiate(KGraph *dst, const KGraph *src, KBlock *block);

/* OP 8: 证明命题 */
int kg_prove(const KGraph *g, const char *statement);

/* OP 9: 导出图到文件 */
int kg_export(const KGraph *g, KExportFormat fmt, const char *filename);

/* OP 10: 序列化为字符串 */
int kg_serialize(const KGraph *g, char *buf, size_t buf_size);

/* OP 11: 从字符串反序列化 */
int kg_deserialize(KGraph *g, const char *buf);

/* OP 12: 查询图信息 */
int kg_query(const KGraph *g, char *buf, size_t buf_size);

/* -- 生命周期辅助 -- */

/* 初始化 KGraph：对所有 KN 个节点的 mpq_t 成员及 KC 个约束的 mpq_t value 调用 mpq_init */
void kg_init_graph(KGraph *g);

/* 销毁 KGraph：对所有已初始化的 mpq_t 成员调用 mpq_clear */
void kg_clear_graph(KGraph *g);

#endif /* PRIMITIVE_RUNTIME_H */
