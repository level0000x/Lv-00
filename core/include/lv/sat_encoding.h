#ifndef lv_SAT_ENCODING_H
#define lv_SAT_ENCODING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

#include "constraint_graph.h"
#include "lv/lv_utils.h"

/* ── SAT Literal ── */
typedef int SatLiteral;

/* ── SAT Result ── */
typedef enum { SAT_RESULT_UNSAT = 0, SAT_RESULT_SAT = 1, SAT_RESULT_UNKNOWN = 2, SAT_RESULT_ERROR = -1 } SatResult;

#define SAT_OK SAT_RESULT_SAT
#define SAT_UNSAT SAT_RESULT_UNSAT
#define SAT_UNKNOWN SAT_RESULT_UNKNOWN
#define SAT_ERROR SAT_RESULT_ERROR

/* ── SAT Variable Entry ── */
#define SAT_MAX_ARITY 8
typedef struct SatVarEntry {
    int var_id;
    int arity;
    int atom_ids[SAT_MAX_ARITY];
} SatVarEntry;

/* ── Rel Expr ── */
typedef enum {
    REL_EXPR_ATOMIC = 0,
    REL_EXPR_NOT,
    REL_EXPR_AND,
    REL_EXPR_OR,
    REL_EXPR_IMPLIES,
    REL_EXPR_IFF,
    REL_EXPR_FORALL,
    REL_EXPR_EXISTS
} RelExprType;

struct RelExpr {
    RelExprType type;
    union {
        struct {
            struct Relation *rel;
        } atomic;
        struct {
            struct RelExpr *child;
        } unary;
        struct {
            struct RelExpr *left;
            struct RelExpr *right;
        } binary;
        struct {
            int var_count;
            struct RelExpr *body;
        } quant;
    } data;
};

/* ── Rel Formula ── */
typedef enum {
    REL_FORMULA_FACT = 0,
    REL_FORMULA_ASSERT,
    REL_FORMULA_FORALL,
    REL_FORMULA_EXISTS,
    REL_FORMULA_NOT,
    REL_FORMULA_AND,
    REL_FORMULA_OR,
    REL_FORMULA_IMPLIES,
    REL_FORMULA_IFF,
    REL_FORMULA_SOME,  /* quantifier */
    REL_FORMULA_NO,    /* quantifier */
    REL_FORMULA_ONE,   /* quantifier */
    REL_FORMULA_LONE,  /* quantifier */
    REL_FORMULA_EQ,    /* comparison */
    REL_FORMULA_SUBSET /* comparison */
} RelFormulaType;

/* ── Rel Atom Type ── */
typedef enum {
    REL_ATOM_POINT = 0,
    REL_ATOM_LINE = 1,
    REL_ATOM_REGION = 2,
    REL_ATOM_PORT = 3,
    REL_ATOM_FUNC_BLOCK = 4,
    REL_ATOM_CUSTOM = 99
} RelAtomType;

/* ── Forward declarations ── */
typedef struct RelAtom RelAtom;
typedef struct RelSignature RelSignature;
typedef struct Relation Relation;
typedef struct RelFormula RelFormula;
typedef struct RelExpr RelExpr;
typedef struct RelModel RelModel;
typedef struct RelInstance RelInstance;
typedef struct SatEncoding SatEncoding;
typedef struct SatModel SatModel;
typedef struct SmallScopeConfig SmallScopeConfig;

/* ── Rel Atom ── */
struct RelAtom {
    int atom_id;
    RelAtomType type;
    char name[128];
};

/* ── Rel Signature ── */
struct RelSignature {
    RelAtom **atoms;
    int atom_count;
    int sig_id;
    char name[128];
    bool is_abstract;
};

/* ── Relation ── */
struct Relation {
    int sig_id;
    int relation_id;
    int arity;
    int **tuples;
    int tuple_count;
    int tuple_capacity;
    char name[128];
    int *domains;
    int domain_count;
};

/* ── Rel Formula ── */
struct RelFormula {
    RelFormulaType type;
    RelExpr *expr;
    RelFormula **sub;
    int sub_count;
    int formula_id;
    char label[128];
};

/* ── Rel Model ── */
struct RelModel {
    RelSignature **sigs;
    int sig_count;
    Relation **relations;
    int relation_count;
    RelFormula **facts;
    int fact_count;
    RelFormula **assertions;
    int assertion_count;
};

/* ── Rel Instance ── */
struct RelInstance {
    RelModel *model;
    RelAtom **atoms;
    int atom_count;
    Relation **rel_bindings;
    int binding_count;
    bool satisfies_assertions;
};

/* ── Small Scope Config ── */
struct SmallScopeConfig {
    int max_bitwidth;
    int max_scope;
    int max_sequence;
    bool enforce_integer_overflow;
};

/* ── SAT Encoding ── */
struct SatEncoding {
    /* Var map */
    lvDArray var_map; /**< lvDArray of SatVarEntry */
    int next_var_id;
    /* Clauses */
    int **clauses;
    int *clause_sizes;
    int clause_count;
    int clause_capacity;
    /* Stats */
    int total_vars;
    int total_clauses;
    double encode_time_ms;
    /* Graph */
    ConstraintGraph *graph;
    /* Rel model (for decode) */
    const RelModel *rel_model;
};

/* ── SAT Model ── */
struct SatModel {
    int var_count;
    int true_count;
    int *true_vars;
    RelInstance *decoded_instance;
    ConstraintGraph *decoded_graph;
};

/* ── API ── */
SatEncoding *sat_encoding_create(int initial_var_capacity, int initial_clause_capacity);
void sat_encoding_destroy(SatEncoding *enc);

int sat_encoding_register_var(SatEncoding *enc, int arity, const int *atom_ids);
int sat_encoding_lookup_var(const SatEncoding *enc, int arity, const int *atom_ids);
int sat_encoding_add_clause(SatEncoding *enc, const SatLiteral *literals, int count);
int sat_encoding_add_assumption(SatEncoding *enc, SatLiteral literal);

int sat_encode_collinearity(SatEncoding *enc, int p1, int p2, int p3);
int sat_encode_parallelism(SatEncoding *enc, int p1, int p2, int p3, int p4);
int sat_encode_perpendicularity(SatEncoding *enc, int p1, int p2, int p3, int p4);
int sat_encode_distance_eq(SatEncoding *enc, int p1, int p2, int p3, int p4);
int sat_encode_angle_eq(SatEncoding *enc, int p1, int p2, int p3, int p4, int p5, int p6);
int sat_encode_containment(SatEncoding *enc, int p_id, int r_id);
int sat_encode_constraint(SatEncoding *enc, int constraint_id);

SatResult constraint_graph_to_sat(const ConstraintGraph *graph, SatEncoding *enc);
SatResult relation_model_to_sat(const RelModel *model, const SmallScopeConfig *scope, SatEncoding *enc);

SatResult sat_solve_and_decode(SatEncoding *enc, SatModel **out_model);
SatResult sat_solve_incremental(SatEncoding *enc, const SatLiteral *literals, int count, SatModel **out_model);

ConstraintGraph *sat_model_to_graph(const SatModel *model);
RelInstance *sat_model_to_instance(const SatEncoding *enc, const SatModel *model);

void sat_model_destroy(SatModel *model);
void relation_instance_destroy(RelInstance *inst);
int *sat_get_unsat_core(const SatEncoding *enc, int *out_count);
bool sat_encoding_export_dimacs(const SatEncoding *enc, const char *filepath);
void sat_encoding_get_stats(const SatEncoding *enc, int *out_vars, int *out_clauses);

#ifdef __cplusplus
}
#endif
#endif
