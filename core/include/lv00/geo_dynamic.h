#ifndef LV00_GEO_DYNAMIC_H
#define LV00_GEO_DYNAMIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>

#define LV00_DYN_INVALID (-1)

#define LV00_DYN_MARK_VISITED 0x01
#define LV00_DYN_MARK_UPDATED 0x02

typedef enum {
    LV00_DYN_NODE_POINT = 0,
    LV00_DYN_NODE_LINE,
    LV00_DYN_NODE_CIRCLE,
    LV00_DYN_NODE_MIDPOINT,
    LV00_DYN_NODE_DISTANCE,
    LV00_DYN_NODE_PARALLEL,
    LV00_DYN_NODE_PERPENDICULAR
} Lv00DynNodeType;

typedef enum {
    LV00_DYN_STATE_VALID = 0,
    LV00_DYN_STATE_DIRTY,
    LV00_DYN_STATE_ERROR
} Lv00DynNodeState;

typedef struct {
    int max_nodes;
    int max_parents;
    int max_children;
    bool detect_cycles;
    int max_update_depth;
} Lv00DynGraphConfig;

typedef struct {
    int total_nodes;
    int free_nodes;
    int derived_nodes;
    int dirty_nodes;
    int max_children;
    int max_parents;
    int total_updates;
} Lv00DynGraphStats;

typedef struct Lv00DynNode {
    int id;
    Lv00DynNodeType type;
    Lv00DynNodeState state;
    int parent_count;
    int child_count;
    int param_count;
    int parent_ids[4];
    int child_ids[16];
    double params[8];
    int marks;
    int update_count;
} Lv00DynNode;

typedef struct Lv00DynGraph {
    int node_count;
    int node_capacity;
    Lv00DynNode *nodes;
    int *id_to_index;
    int *parent_adj;
    int *parent_adj_offsets;
    int *child_adj;
    int *child_adj_offsets;
    int adj_capacity;
    Lv00DynGraphConfig config;
    int total_updates;
} Lv00DynGraph;

typedef void (*Lv00DynUpdateFunc)(Lv00DynGraph *graph, int node_id);

Lv00DynGraphConfig lv00_dyn_graph_default_config(void);

Lv00DynGraph *lv00_dyn_graph_create(const Lv00DynGraphConfig *config);
void lv00_dyn_graph_destroy(Lv00DynGraph *graph);

int lv00_dyn_graph_add_node(
    Lv00DynGraph *graph,
    Lv00DynNodeType type,
    const int *parent_ids,
    int parent_count,
    const double *params,
    int param_count);

Lv00DynNode *lv00_dyn_graph_get_node(Lv00DynGraph *graph, int node_id);
bool lv00_dyn_graph_remove_node(Lv00DynGraph *graph, int node_id);

int lv00_dyn_graph_get_parents(
    const Lv00DynGraph *graph,
    int node_id,
    int *out_parents,
    int max_count);

int lv00_dyn_graph_get_children(
    const Lv00DynGraph *graph,
    int node_id,
    int *out_children,
    int max_count);

int lv00_dyn_graph_update_cascade(
    Lv00DynGraph *graph,
    int root_id,
    Lv00DynUpdateFunc update_func);

int lv00_dyn_graph_update_chain(Lv00DynGraph *graph, int leaf_id);
void lv00_dyn_graph_mark_dirty(Lv00DynGraph *graph, int node_id);
int lv00_dyn_graph_update_all(Lv00DynGraph *graph);

bool lv00_dyn_graph_has_path(
    const Lv00DynGraph *graph,
    int start_id,
    int target_id);

bool lv00_dyn_graph_would_create_cycle(
    const Lv00DynGraph *graph,
    int parent_id,
    int child_id);

int lv00_dyn_graph_topological_sort(
    const Lv00DynGraph *graph,
    int *out_order);

int lv00_dyn_create_point(Lv00DynGraph *graph, double x, double y);
int lv00_dyn_create_line(Lv00DynGraph *graph, int p1_id, int p2_id);
int lv00_dyn_create_circle(Lv00DynGraph *graph, int center_id, int point_id);
int lv00_dyn_create_midpoint(Lv00DynGraph *graph, int p1_id, int p2_id);
int lv00_dyn_create_parallel(Lv00DynGraph *graph, int base_line_id, int through_point_id);
int lv00_dyn_create_perpendicular(Lv00DynGraph *graph, int base_line_id, int through_point_id);
int lv00_dyn_create_distance(Lv00DynGraph *graph, int p1_id, int p2_id);

void lv00_dyn_graph_get_stats(
    const Lv00DynGraph *graph,
    Lv00DynGraphStats *out_stats);

void lv00_dyn_graph_clear_dirty(Lv00DynGraph *graph);
void lv00_dyn_graph_reset_states(Lv00DynGraph *graph);

typedef struct { double x, y, vx, vy; } Lv00DynamicPoint;

void lv00_geo_dynamic_step(Lv00DynamicPoint *points, size_t count, double dt);

#ifdef __cplusplus
}
#endif

#endif
