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
} CoordOpsVTable;

/* 由 symbolic_coord_ops.c 定义的全局 vtable 数组 */
extern const CoordOpsVTable kCoordOpsVTable[];

#endif /* LV_SYMBOLIC_COORD_INTERNAL_H */