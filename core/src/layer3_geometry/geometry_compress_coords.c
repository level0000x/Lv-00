/*
 * @file geometry_compress_coords.c
 * @brief Geometry compression engine - public predictive coords interface
 * @details Split from geometry_compress.c
 */

#include "geometry_compress.h"
#include "geometry_compress_internal.h"

#include "lv/lv_file.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/lv_heap.h"

#include "lv_internal.h"
#include "lv_utils.h"
#include "node_deep_copy.h"
#include "symbolic_coord.h"

/* ── Prediction function pointer type ── */
typedef bool (*PredictFn)(ConstraintGraph *graph);

/* ── No-op predictor for PREDICT_NONE ── */
static bool predict_none(ConstraintGraph *graph) {
    (void)graph;
    return true;
}

/* ── Predictor dispatch table (PREDICT_NONE = -1 handled separately) ── */
static const PredictFn kPredictHandlers[] = {
    [PREDICT_PARALLELOGRAM] = predictive_encode_parallelogram,
    [PREDICT_MULTI_PARALLELOGRAM] = predictive_encode_multi_parallelogram,
    [PREDICT_DELTA] = predictive_encode_delta,
};

/* ========================================================================
 * Public predictive encoding interface
 * ======================================================================== */

bool predictive_encode_coords(ConstraintGraph *graph, PredictionMode mode) {
    if (!graph)
        return false;

    if (mode == PREDICT_NONE)
        return true;

    return LV_DISPATCH(kPredictHandlers, mode, false, graph);
}

