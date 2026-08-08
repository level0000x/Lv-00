/**
 * @file symbolic_coord_internal.h
 * @brief SymbolicCoord 内部共享头文件：VTable 定义与跨模块类型分发
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */
#ifndef LV_SYMBOLIC_COORD_INTERNAL_H
#define LV_SYMBOLIC_COORD_INTERNAL_H

#include "lv/symbolic_coord.h"

/* ── CoordOpsVTable: 虚函数表，消除类型分派 switch 反模式 ── */
typedef struct CoordOpsVTable {
    SymbolicCoord *(*add)(const SymbolicCoord *a, const SymbolicCoord *b);
    SymbolicCoord *(*subtract)(const SymbolicCoord *a, const SymbolicCoord *b);
    SymbolicCoord *(*multiply)(const SymbolicCoord *a, const SymbolicCoord *b);
    SymbolicCoord *(*divide)(const SymbolicCoord *a, const SymbolicCoord *b);
    int (*compare)(const SymbolicCoord *a, const SymbolicCoord *b);
    double (*to_double)(const SymbolicCoord *c);
    size_t (*bit_burning_bits)(const SymbolicCoord *c);

    /* ── 生命周期管理 ── */
    void (*destroy)(SymbolicCoord *coord);
    char *(*serialize)(const SymbolicCoord *coord);
    void (*copy_data)(const SymbolicCoord *src, SymbolicCoord *dst);
    bool (*copy_check)(const SymbolicCoord *coord);
    bool (*is_zero)(const SymbolicCoord *coord);
} CoordOpsVTable;

/* 由 symbolic_coord_ops.c 定义的全局 vtable 数组 */
extern const CoordOpsVTable kCoordOpsVTable[];

/* ── 代数数共享辅助（由 algebraic.c 定义，symbolics 层复用，
 *    rational.c 不再维护重复的 static 副本）── */
double sym_evaluate_poly_double(const mpz_poly_t *poly, double x);
void sym_evaluate_algebraic_at_rational(mpz_t result, const mpz_poly_t *poly, const Rational *r);

/* ── 生命周期 VTable handlers（由 symbolic_coord_lifecycle.c 定义）── */
void destroy_rational(SymbolicCoord *coord);
void destroy_algebraic(SymbolicCoord *coord);
void destroy_quadratic(SymbolicCoord *coord);
void destroy_transcendental(SymbolicCoord *coord);
char *serialize_rational(const SymbolicCoord *coord);
char *serialize_algebraic(const SymbolicCoord *coord);
char *serialize_quadratic(const SymbolicCoord *coord);
char *serialize_transcendental(const SymbolicCoord *coord);
void copy_data_rational(const SymbolicCoord *src, SymbolicCoord *dst);
void copy_data_algebraic(const SymbolicCoord *src, SymbolicCoord *dst);
void copy_data_quadratic(const SymbolicCoord *src, SymbolicCoord *dst);
void copy_data_transcendental(const SymbolicCoord *src, SymbolicCoord *dst);
bool copy_check_rational(const SymbolicCoord *coord);
bool copy_check_algebraic(const SymbolicCoord *coord);
bool copy_check_quadratic(const SymbolicCoord *coord);
bool copy_check_transcendental(const SymbolicCoord *coord);
bool is_zero_rational(const SymbolicCoord *coord);
bool is_zero_algebraic(const SymbolicCoord *coord);
bool is_zero_quadratic(const SymbolicCoord *coord);
bool is_zero_transcendental(const SymbolicCoord *coord);

#endif /* LV_SYMBOLIC_COORD_INTERNAL_H */