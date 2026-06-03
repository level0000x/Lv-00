/**
 * @file geo_metalogic.h
 * @brief 几何元逻辑层 - 基于 Isabelle Pure 元逻辑设计
 * 
 * 实现元逻辑-对象逻辑分层架构，支持多公理包系统
 * 
 * @author Lv-00 Project
 * @version 1.0
 */

#ifndef LV00_GEO_METALOGIC_H
#define LV00_GEO_METALOGIC_H

#include <lv00.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============ 类型系统 ============ */

/* 几何类型变量 */
typedef struct Lv00TypeVar Lv00TypeVar;
struct Lv00TypeVar {
    char* name;           /* 类型变量名，如 "'a" */
    Lv00TypeVar* next;    /* 链接下一个类型变量 */
};

/* 几何类型标签 */
typedef enum {
    LV00_GTYPE_POINT,     /* 点类型 */
    LV00_GTYPE_LINE,      /* 直线类型 */
    LV00_GTYPE_CIRCLE,   /* 圆类型 */
    LV00_GTYPE_PLANE,     /* 平面类型 */
    LV00_GTYPE_SEGMENT,   /* 线段类型 */
    LV00_GTYPE_ANGLE,    /* 角度类型 */
    LV00_GTYPE_REAL,     /* 实数类型（用于度量） */
    LV00_GTYPE_BOOL,     /* 布尔类型 */
    LV00_GTYPE_VAR       /* 类型变量 */
} Lv00GeoTypeTag;

/* 几何类型 */
typedef struct {
    Lv00GeoTypeTag tag;
    union {
        struct { int unused; } point;     /* 空结构占位 */
        struct { int unused; } line;
        struct { int unused; } circle;
        struct { Lv00GeoTypeTag elem; } set;  /* 集合类型 */
        struct { Lv00TypeVar* var; } var;     /* 类型变量 */
    } data;
} Lv00GeoType;

/* 类型上下文（类型环境） */
typedef struct Lv00TypeEnv Lv00TypeEnv;
struct Lv00TypeEnv {
    char** names;          /* 变量名列表 */
    Lv00GeoType* types;   /* 对应的类型 */
    size_t count;
    Lv00TypeEnv* parent;  /* 父作用域 */
};

/* ============ 几何项（Terms）============ */

typedef enum {
    LV00_TERM_VAR,        /* 几何变量 */
    LV00_TERM_CONST,       /* 几何常量 */
    LV00_TERM_APP,        /* 函数应用 */
    LV00_TERM_ABS,        /* λ-抽象 */
    LV00_TERM_GEOM        /* 几何对象构造 */
} Lv00TermTag;

/* 几何构造函数 */
typedef enum {
    LV00_CONSTRUCT_MK_POINT,      /* mk_point(x, y) */
    LV00_CONSTRUCT_MK_LINE,       /* mk_line(p1, p2) */
    LV00_CONSTRUCT_MK_CIRCLE,     /* mk_circle(center, radius) */
    LV00_CONSTRUCT_MK_SEGMENT,    /* mk_segment(p1, p2) */
    LV00_CONSTRUCT_MIDPOINT,      /* midpoint(p1, p2) */
    LV00_CONSTRUCT_INTERSECT      /* intersect(l1, l2) */
} Lv00GeoConstructor;

typedef struct Lv00Term Lv00Term;
struct Lv00Term {
    Lv00TermTag tag;
    Lv00GeoType type;     /* 项的类型 */
    union {
        struct { char* name; } var;
        struct { char* name; Lv00Term** args; size_t n; } const_;
        struct { Lv00Term* fn; Lv00Term* arg; } app;
        struct { char* vname; Lv00GeoType vtype; Lv00Term* body; } abs;
        struct { Lv00GeoConstructor ctor; Lv00Term** args; size_t n; } geom;
    } data;
};

/* ============ 几何命题（Propositions）============ */

typedef enum {
    LV00_PROP_EQ,           /* 几何相等 A ≡ B */
    LV00_PROP_CONG,         /* 全等 A ≅ B */
    LV00_PROP_PARALLEL,     /* 平行 A ∥ B */
    LV00_PROP_PERP,         /* 垂直 A ⟂ B */
    LV00_PROP_ON,           /* 在...上 P on L */
    LV00_PROP_BETWEEN,      /* 在...之间 Between(A,B,C) */
    LV00_PROP_CONVEX,       /* 凸 convex(S) */
    LV00_PROP_EXISTS,       /* 存在量词 ∃x. P(x) */
    LV00_PROP_FORALL,       /* 全称量词 ∀x. P(x) */
    LV00_PROP_IMP,          /* 蕴含 P ⇒ Q */
    LV00_PROP_AND,          /* 合取 P ∧ Q */
    LV00_PROP_OR            /* 析取 P ∨ Q */
} Lv00PropTag;

typedef struct Lv00Prop Lv00Prop;
struct Lv00Prop {
    Lv00PropTag tag;
    Lv00TypeEnv* type_env;  /* 类型环境 */
    
    union {
        struct { Lv00Term* lhs; Lv00Term* rhs; } eq;
        struct { Lv00Term* l1; Lv00Term* l2; } parallel;
        struct { Lv00Term* l1; Lv00Term* l2; } perp;
        struct { Lv00Term* p; Lv00Term* obj; } on;
        struct { Lv00Term* a; Lv00Term* b; Lv00Term* c; } between;
        struct { char* vname; Lv00GeoType vtype; Lv00Prop* body; } exists;
        struct { char* vname; Lv00GeoType vtype; Lv00Prop* body; } forall;
        struct { Lv00Prop* antecedent; Lv00Prop* consequent; } imp;
        struct { Lv00Prop* left; Lv00Prop* right; } and;
        struct { Lv00Prop* left; Lv00Prop* right; } or;
    } data;
};

/* ============ 证明上下文 ============ */

typedef struct Lv00AxiomPackage Lv00AxiomPackage;

typedef struct Lv00ProofContext Lv00ProofContext;
struct Lv00ProofContext {
    /* 类型环境 */
    Lv00TypeEnv* type_env;
    
    /* 假设（局部的几何事实） */
    Lv00Prop** assumptions;
    size_t assumption_count;
    
    /* 目标命题 */
    Lv00Prop* goal;
    
    /* 公理包（对象逻辑选择） */
    Lv00AxiomPackage* axiom_pkg;
    
    /* 证明状态 */
    void* proof_state;  /* 后端特定状态 */
};

/* ============ API 声明 ============ */

/* 类型操作 */
Lv00GeoType lv00_gtype_point(void);
Lv00GeoType lv00_gtype_line(void);
Lv00GeoType lv00_gtype_circle(void);
Lv00GeoType lv00_gtype_var(const char* name);
Lv00GeoType lv00_gtype_set(Lv00GeoType elem);
int lv00_gtype_equal(Lv00GeoType a, Lv00GeoType b);
int lv00_gtype_unify(Lv00GeoType a, Lv00GeoType b, void** subst);

/* 类型环境操作 */
Lv00TypeEnv* lv00_type_env_create(Lv00TypeEnv* parent);
void lv00_type_env_add(Lv00TypeEnv* env, const char* name, Lv00GeoType type);
Lv00GeoType lv00_type_env_lookup(Lv00TypeEnv* env, const char* name);
void lv00_type_env_destroy(Lv00TypeEnv* env);

/* 项构造 */
Lv00Term* lv00_term_var_create(const char* name);
Lv00Term* lv00_term_const_create(const char* name, Lv00Term** args, size_t n);
Lv00Term* lv00_term_geom_create(Lv00GeoConstructor ctor, Lv00Term** args, size_t n);

/* 命题构造 */
Lv00Prop* lv00_prop_parallel(Lv00Term* l1, Lv00Term* l2);
Lv00Prop* lv00_prop_perp(Lv00Term* l1, Lv00Term* l2);
Lv00Prop* lv00_prop_on(Lv00Term* p, Lv00Term* obj);
Lv00Prop* lv00_prop_congruent(Lv00Term* a, Lv00Term* b);
Lv00Prop* lv00_prop_between(Lv00Term* a, Lv00Term* b, Lv00Term* c);
Lv00Prop* lv00_prop_exists(const char* vname, Lv00GeoType vtype, Lv00Prop* body);
Lv00Prop* lv00_prop_forall(const char* vname, Lv00GeoType vtype, Lv00Prop* body);
Lv00Prop* lv00_prop_imp(Lv00Prop* ant, Lv00Prop* cons);
Lv00Prop* lv00_prop_and(Lv00Prop* left, Lv00Prop* right);
Lv00Prop* lv00_prop_or(Lv00Prop* left, Lv00Prop* right);

/* 证明上下文操作 */
Lv00ProofContext* lv00_pcontext_create(Lv00AxiomPackage* pkg);
void lv00_pcontext_assume(Lv00ProofContext* ctx, Lv00Prop* prop);
Lv00Prop* lv00_pcontext_goal(Lv00ProofContext* ctx);
void lv00_pcontext_destroy(Lv00ProofContext* ctx);

/* 清理 */
void lv00_term_destroy(Lv00Term* term);
void lv00_prop_destroy(Lv00Prop* prop);

#ifdef __cplusplus
}
#endif

#endif /* LV00_GEO_METALOGIC_H */
