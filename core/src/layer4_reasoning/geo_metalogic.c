/**
 * @file geo_metalogic.c
 * @brief 几何元逻辑层实现
 */

#include "lv00/geo_metalogic.h"
#include "lv00_utils.h"
#include <stdlib.h>
#include <string.h>

/* ============ 内部工具函数 ============ */

static char* lv00_strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* copy = (char*)lv00_malloc(len);
    if (copy) memcpy(copy, s, len);
    return copy;
}

/* ============ 类型操作 ============ */

Lv00GeoType lv00_gtype_point(void) {
    Lv00GeoType t;
    t.tag = LV00_GTYPE_POINT;
    t.data.point.unused = 0;
    return t;
}

Lv00GeoType lv00_gtype_line(void) {
    Lv00GeoType t;
    t.tag = LV00_GTYPE_LINE;
    t.data.line.unused = 0;
    return t;
}

Lv00GeoType lv00_gtype_circle(void) {
    Lv00GeoType t;
    t.tag = LV00_GTYPE_CIRCLE;
    t.data.circle.unused = 0;
    return t;
}

Lv00GeoType lv00_gtype_var(const char* name) {
    Lv00GeoType t;
    t.tag = LV00_GTYPE_VAR;
    t.data.var.var = (Lv00TypeVar*)lv00_malloc(sizeof(Lv00TypeVar));
    if (t.data.var.var) {
        t.data.var.var->name = lv00_strdup(name);
        t.data.var.var->next = NULL;
    }
    return t;
}

int lv00_gtype_equal(Lv00GeoType a, Lv00GeoType b) {
    if (a.tag != b.tag) return 0;
    
    switch (a.tag) {
        case LV00_GTYPE_VAR:
            if (!a.data.var.var || !b.data.var.var) return 0;
            return strcmp(a.data.var.var->name, b.data.var.var->name) == 0;
        default:
            return 1;
    }
}

/* ============ 类型环境操作 ============ */

Lv00TypeEnv* lv00_type_env_create(Lv00TypeEnv* parent) {
    Lv00TypeEnv* env = (Lv00TypeEnv*)lv00_malloc(sizeof(Lv00TypeEnv));
    if (!env) return NULL;
    
    env->names = NULL;
    env->types = NULL;
    env->count = 0;
    env->parent = parent;
    
    return env;
}

void lv00_type_env_add(Lv00TypeEnv* env, const char* name, Lv00GeoType type) {
    if (!env || !name) return;
    
    size_t new_count = env->count + 1;
    char** new_names = (char**)lv00_malloc(new_count * sizeof(char*));
    Lv00GeoType* new_types = (Lv00GeoType*)lv00_malloc(new_count * sizeof(Lv00GeoType));
    
    if (env->names) {
        memcpy(new_names, env->names, env->count * sizeof(char*));
        lv00_free(env->names);
    }
    if (env->types) {
        memcpy(new_types, env->types, env->count * sizeof(Lv00GeoType));
        lv00_free(env->types);
    }
    
    new_names[env->count] = lv00_strdup(name);
    new_types[env->count] = type;
    
    env->names = new_names;
    env->types = new_types;
    env->count = new_count;
}

Lv00GeoType lv00_type_env_lookup(Lv00TypeEnv* env, const char* name) {
    if (!env || !name) {
        Lv00GeoType t;
        t.tag = LV00_GTYPE_VAR;
        t.data.var.var = NULL;
        return t;
    }
    
    for (size_t i = 0; i < env->count; i++) {
        if (env->names[i] && strcmp(env->names[i], name) == 0) {
            return env->types[i];
        }
    }
    
    /* 递归查找父环境 */
    if (env->parent) {
        return lv00_type_env_lookup(env->parent, name);
    }
    
    Lv00GeoType t;
    t.tag = LV00_GTYPE_VAR;
    t.data.var.var = NULL;
    return t;
}

void lv00_type_env_destroy(Lv00TypeEnv* env) {
    if (!env) return;
    
    for (size_t i = 0; i < env->count; i++) {
        lv00_free(env->names[i]);
    }
    lv00_free(env->names);
    lv00_free(env->types);
    lv00_free(env);
}

/* ============ 项构造 ============ */

Lv00Term* lv00_term_var_create(const char* name) {
    Lv00Term* term = (Lv00Term*)lv00_malloc(sizeof(Lv00Term));
    if (!term) return NULL;
    
    term->tag = LV00_TERM_VAR;
    term->type = lv00_gtype_var("unknown");
    term->data.var.name = lv00_strdup(name);
    
    return term;
}

Lv00Term* lv00_term_const_create(const char* name, Lv00Term** args, size_t n) {
    Lv00Term* term = (Lv00Term*)lv00_malloc(sizeof(Lv00Term));
    if (!term) return NULL;
    
    term->tag = LV00_TERM_CONST;
    term->type = lv00_gtype_var("unknown");
    term->data.const_.name = lv00_strdup(name);
    
    if (n > 0 && args) {
        term->data.const_.args = (Lv00Term**)lv00_malloc(n * sizeof(Lv00Term*));
        memcpy(term->data.const_.args, args, n * sizeof(Lv00Term*));
        term->data.const_.n = n;
    } else {
        term->data.const_.args = NULL;
        term->data.const_.n = 0;
    }
    
    return term;
}

Lv00Term* lv00_term_geom_create(Lv00GeoConstructor ctor, Lv00Term** args, size_t n) {
    Lv00Term* term = (Lv00Term*)lv00_malloc(sizeof(Lv00Term));
    if (!term) return NULL;
    
    term->tag = LV00_TERM_GEOM;
    term->type = lv00_gtype_var("geom");
    term->data.geom.ctor = ctor;
    
    if (n > 0 && args) {
        term->data.geom.args = (Lv00Term**)lv00_malloc(n * sizeof(Lv00Term*));
        memcpy(term->data.geom.args, args, n * sizeof(Lv00Term*));
        term->data.geom.n = n;
    } else {
        term->data.geom.args = NULL;
        term->data.geom.n = 0;
    }
    
    return term;
}

/* ============ 命题构造 ============ */

Lv00Prop* lv00_prop_parallel(Lv00Term* l1, Lv00Term* l2) {
    Lv00Prop* prop = (Lv00Prop*)lv00_malloc(sizeof(Lv00Prop));
    if (!prop) return NULL;
    
    prop->tag = LV00_PROP_PARALLEL;
    prop->type_env = NULL;
    prop->data.parallel.l1 = l1;
    prop->data.parallel.l2 = l2;
    
    return prop;
}

Lv00Prop* lv00_prop_perp(Lv00Term* l1, Lv00Term* l2) {
    Lv00Prop* prop = (Lv00Prop*)lv00_malloc(sizeof(Lv00Prop));
    if (!prop) return NULL;
    
    prop->tag = LV00_PROP_PERP;
    prop->type_env = NULL;
    prop->data.perp.l1 = l1;
    prop->data.perp.l2 = l2;
    
    return prop;
}

Lv00Prop* lv00_prop_between(Lv00Term* a, Lv00Term* b, Lv00Term* c) {
    Lv00Prop* prop = (Lv00Prop*)lv00_malloc(sizeof(Lv00Prop));
    if (!prop) return NULL;
    
    prop->tag = LV00_PROP_BETWEEN;
    prop->type_env = NULL;
    prop->data.between.a = a;
    prop->data.between.b = b;
    prop->data.between.c = c;
    
    return prop;
}

Lv00Prop* lv00_prop_imp(Lv00Prop* ant, Lv00Prop* cons) {
    Lv00Prop* prop = (Lv00Prop*)lv00_malloc(sizeof(Lv00Prop));
    if (!prop) return NULL;
    
    prop->tag = LV00_PROP_IMP;
    prop->type_env = NULL;
    prop->data.imp.antecedent = ant;
    prop->data.imp.consequent = cons;
    
    return prop;
}

Lv00Prop* lv00_prop_and(Lv00Prop* left, Lv00Prop* right) {
    Lv00Prop* prop = (Lv00Prop*)lv00_malloc(sizeof(Lv00Prop));
    if (!prop) return NULL;
    
    prop->tag = LV00_PROP_AND;
    prop->type_env = NULL;
    prop->data.and.left = left;
    prop->data.and.right = right;
    
    return prop;
}

/* ============ 证明上下文操作 ============ */

Lv00ProofContext* lv00_pcontext_create(Lv00AxiomPackage* pkg) {
    Lv00ProofContext* ctx = (Lv00ProofContext*)lv00_malloc(sizeof(Lv00ProofContext));
    if (!ctx) return NULL;
    
    ctx->type_env = lv00_type_env_create(NULL);
    ctx->assumptions = NULL;
    ctx->assumption_count = 0;
    ctx->goal = NULL;
    ctx->axiom_pkg = pkg;
    ctx->proof_state = NULL;
    
    return ctx;
}

void lv00_pcontext_assume(Lv00ProofContext* ctx, Lv00Prop* prop) {
    if (!ctx || !prop) return;
    
    size_t new_count = ctx->assumption_count + 1;
    Lv00Prop** new_assumptions = (Lv00Prop**)lv00_malloc(new_count * sizeof(Lv00Prop*));
    
    if (ctx->assumptions) {
        memcpy(new_assumptions, ctx->assumptions, ctx->assumption_count * sizeof(Lv00Prop*));
        lv00_free(ctx->assumptions);
    }
    
    new_assumptions[ctx->assumption_count] = prop;
    ctx->assumptions = new_assumptions;
    ctx->assumption_count = new_count;
}

void lv00_pcontext_destroy(Lv00ProofContext* ctx) {
    if (!ctx) return;
    
    lv00_type_env_destroy(ctx->type_env);
    lv00_free(ctx->assumptions);
    lv00_free(ctx);
}

/* ============ 清理 ============ */

void lv00_term_destroy(Lv00Term* term) {
    if (!term) return;
    
    switch (term->tag) {
        case LV00_TERM_VAR:
            lv00_free(term->data.var.name);
            break;
        case LV00_TERM_CONST:
            lv00_free(term->data.const_.name);
            for (size_t i = 0; i < term->data.const_.n; i++) {
                lv00_term_destroy(term->data.const_.args[i]);
            }
            lv00_free(term->data.const_.args);
            break;
        case LV00_TERM_GEOM:
            for (size_t i = 0; i < term->data.geom.n; i++) {
                lv00_term_destroy(term->data.geom.args[i]);
            }
            lv00_free(term->data.geom.args);
            break;
        default:
            break;
    }
    
    lv00_free(term);
}

void lv00_prop_destroy(Lv00Prop* prop) {
    if (!prop) return;
    
    switch (prop->tag) {
        case LV00_PROP_IMP:
            lv00_prop_destroy(prop->data.imp.antecedent);
            lv00_prop_destroy(prop->data.imp.consequent);
            break;
        case LV00_PROP_AND:
        case LV00_PROP_OR:
            lv00_prop_destroy(prop->data.and.left);
            lv00_prop_destroy(prop->data.and.right);
            break;
        default:
            break;
    }
    
    lv00_free(prop);
}
