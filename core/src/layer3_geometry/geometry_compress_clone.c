/*
 * @file geometry_compress_clone.c
 * @brief Geometry compression engine - graph deep copy
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
 * graph_clone for deep copy (T-010/011)
 * ======================================================================== */

/**
 * @brief Deep copy a ConstraintGraph
 *
 * Creates a new graph with deep copies of all nodes (including coordinates),
 * all constraints, and all edges.
 *
 * @param[in] graph Source constraint graph
 * @return Deep-copied graph, NULL on failure
 */
ConstraintGraph *graph_clone(const ConstraintGraph *graph) {
    if (!graph)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "graph_clone: graph is NULL");

    ConstraintGraph *cloned = graph_create();
    if (!cloned)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "graph_clone: graph_create failed");

    /* Deep copy all nodes */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *orig = graph->nodes[i];
        if (!orig)
            continue;

        GeomNode *copy = node_deep_copy_geom_node(orig, NULL);
        if (!copy) {
            graph_destroy(cloned);
            lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "graph_clone: node_deep_copy failed");
        }

        /* Add node to cloned graph using the same ID */
        GeomNode *added =
            graph_add_node_with_id(cloned, copy->id, copy->type, copy->symbolic_coords, copy->coord_count);
        if (!added) {
            /* Clean up the copy we made */
            if (copy->symbolic_coords) {
                for (int d = 0; d < copy->coord_count; d++) {
                    if (copy->symbolic_coords[d])
                        symbolic_coord_destroy(copy->symbolic_coords[d]);
                }
                lv_free((void **) &copy->symbolic_coords);
            }
            if (copy->numeric_assumption_declaration)
                lv_free((void **) &copy->numeric_assumption_declaration);
            lv_free((void **) &copy);
            graph_destroy(cloned);
            lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "graph_clone: graph_add_node_with_id failed");
        }

        /* Copy additional fields that graph_add_node_with_id may not set */
        added->trust = copy->trust;
        added->lo_subtype = copy->lo_subtype;
        added->numeric_precision = copy->numeric_precision;
        added->namespace_depth = copy->namespace_depth;
        added->parent_block_id = copy->parent_block_id;

        /* Free the intermediate copy (graph_add_node_with_id made its own deep copy) */
        if (copy->numeric_assumption_declaration)
            lv_free((void **) &copy->numeric_assumption_declaration);
        lv_free((void **) &copy);
    }

    /* Deep copy all constraints */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *orig = graph->constraints[i];
        if (!orig)
            continue;

        /* Allocate participant array copy */
        int *parts_copy = (int *) lv_malloc(orig->participant_count * sizeof(int));
        if (!parts_copy) {
            graph_destroy(cloned);
            lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "graph_clone: malloc parts_copy failed");
        }
        memcpy(parts_copy, orig->participants, (size_t) orig->participant_count * sizeof(int));

        Constraint *added =
            graph_add_constraint_with_id(cloned, orig->id, orig->type, parts_copy, orig->participant_count);
        if (!added) {
            lv_free((void **) &parts_copy);
            graph_destroy(cloned);
            lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "graph_clone: graph_add_constraint_with_id failed");
        }
        lv_free((void **) &parts_copy); /* graph_add_constraint_with_id makes its own copy */
    }

    return cloned;
}

