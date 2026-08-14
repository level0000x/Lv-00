/**
 * @file geo_constraint_solver_internal.h
 * @brief Internal shared definitions for the geo constraint solver module.
 */

#ifndef lv_GEO_CONSTRAINT_SOLVER_INTERNAL_H
#define lv_GEO_CONSTRAINT_SOLVER_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv/lv_platform.h"
#include "lv/lv_internal.h"
#include "lv/lv_numeric.h" /* lv_NUMERICAL_DIFF_EPSILON（有限差分步长基准） */
#include "lv/geo_constraint_solver.h"
#include "lv/config.h"
#include "lv/lv_utils.h"
#include "lv/lv_hashtable.h"

#include <stdbool.h>
#include <stdint.h>

#ifndef lv_PUBLIC_API
#define lv_PUBLIC_API
#endif

/* ---- shared constants ---- */
#define INITIAL_CAPACITY 16
/* 统一引用公共数值差分步长基准（见 lv/lv_numeric.h 的 lv_NUMERICAL_DIFF_EPSILON，
 * 原独立定义 1e-8，与 geom_evol.c fd_eps / float_error.c sqrt(DBL_EPSILON) 量级一致） */
#define NUMERICAL_DIFF_EPSILON lv_NUMERICAL_DIFF_EPSILON
#define MAX_PARAMS 64
#define ID_HASH_TABLE_SIZE 512

/* ---- shared types ---- */
typedef struct {
    lvHashtable *ht; /**< 内部自动扩容哈希表（lv_hashtable_int：ID → 下标+1，自动扩容） */
} IdHashTable;

typedef struct lvSolverSystemEx {
    lvSolverSystem base;         /**< 基础求解器系统 */
    IdHashTable entity_hash;     /**< 实体 ID 哈希表 */
    IdHashTable constraint_hash; /**< 约束 ID 哈希表 */
} lvSolverSystemEx;

/* ---- hash.c ---- */
void id_hash_init(IdHashTable *table);
void id_hash_destroy(IdHashTable *table);
bool id_hash_insert(IdHashTable *table, int id, int index);
bool id_hash_remove(IdHashTable *table, int id);
int find_entity_index_fast(const lvSolverSystemEx *sys_ex, int id);
int find_constraint_index_fast(const lvSolverSystemEx *sys_ex, int id);

/* ---- residual.c ---- */
double evaluate_constraint(const lvSolverSystem *sys, const lvConstraint *c, double *error_val);

/* ---- linear.c ---- */
int gauss_eliminate(double *A, double *b, int n);
double vec_norm(const double *v, int n);

#ifdef __cplusplus
}
#endif

#endif /* lv_GEO_CONSTRAINT_SOLVER_INTERNAL_H */
