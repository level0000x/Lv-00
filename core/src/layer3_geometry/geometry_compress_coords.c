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

/* ========================================================================
 * Public predictive encoding interface
 * ======================================================================== */

bool predictive_encode_coords(ConstraintGraph *graph, PredictionMode mode) {
    if (!graph)
        return false;

    switch (mode) {
        case PREDICT_PARALLELOGRAM:
            return predictive_encode_parallelogram(graph);

        case PREDICT_MULTI_PARALLELOGRAM:
            return predictive_encode_multi_parallelogram(graph);

        case PREDICT_DELTA:
            return predictive_encode_delta(graph);

        case PREDICT_NONE:
            /* No prediction: keep original coordinates */
            return true;

        default:
            return false;
    }
}

