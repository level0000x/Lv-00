#ifndef lv_GEO_DYNAMIC_H
#define lv_GEO_DYNAMIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

#define lv_DYN_INVALID (-1)

#define lv_DYN_MARK_VISITED 0x01
#define lv_DYN_MARK_UPDATED 0x02

typedef enum {
    lv_DYN_NODE_POINT = 0,
    lv_DYN_NODE_LINE,
    lv_DYN_NODE_CIRCLE,
    lv_DYN_NODE_MIDPOINT,
    lv_DYN_NODE_DISTANCE,
    lv_DYN_NODE_PARALLEL,
    lv_DYN_NODE_PERPENDICULAR
} lvDynNodeType;

typedef enum { lv_DYN_STATE_VALID = 0, lv_DYN_STATE_DIRTY, lv_DYN_STATE_ERROR } lvDynNodeState;

typedef struct {
    int max_nodes;
    int max_parents;
    int max_children;
    bool detect_cycles;
    int max_update_depth;
} lvDynGraphConfig;

typedef struct {
    int total_nodes;
    int free_nodes;
    int derived_nodes;
    int dirty_nodes;
    int max_children;
    int max_parents;
    int total_updates;
} lvDynGraphStats;

typedef struct lvDynNode {
    int id;
    lvDynNodeType type;
    lvDynNodeState state;
    int parent_count;
    int child_count;
    int param_count;
    int parent_ids[4];
    int child_ids[16];
    double params[8];
    int marks;
    int update_count;
} lvDynNode;

typedef struct lvDynGraph {
    int node_count;
    int node_capacity;
    lvDynNode *nodes;
    int *id_to_index;
    int *parent_adj;
    int *parent_adj_offsets;
    int *child_adj;
    int *child_adj_offsets;
    int adj_capacity;
    lvDynGraphConfig config;
    int total_updates;
} lvDynGraph;

typedef void (*lvDynUpdateFunc)(lvDynGraph *graph, int node_id);

lvDynGraphConfig lv_dyn_graph_default_config(void);

lvDynGraph *lv_dyn_graph_create(const lvDynGraphConfig *config);
void lv_dyn_graph_destroy(lvDynGraph *graph);

int lv_dyn_graph_add_node(lvDynGraph *graph, lvDynNodeType type, const int *parent_ids, int parent_count,
                          const double *params, int param_count);

lvDynNode *lv_dyn_graph_get_node(lvDynGraph *graph, int node_id);
bool lv_dyn_graph_remove_node(lvDynGraph *graph, int node_id);

int lv_dyn_graph_get_parents(const lvDynGraph *graph, int node_id, int *out_parents, int max_count);

int lv_dyn_graph_get_children(const lvDynGraph *graph, int node_id, int *out_children, int max_count);

int lv_dyn_graph_update_cascade(lvDynGraph *graph, int root_id, lvDynUpdateFunc update_func);

int lv_dyn_graph_update_chain(lvDynGraph *graph, int leaf_id);
void lv_dyn_graph_mark_dirty(lvDynGraph *graph, int node_id);
int lv_dyn_graph_update_all(lvDynGraph *graph);

bool lv_dyn_graph_has_path(const lvDynGraph *graph, int start_id, int target_id);

bool lv_dyn_graph_would_create_cycle(const lvDynGraph *graph, int parent_id, int child_id);

int lv_dyn_graph_topological_sort(const lvDynGraph *graph, int *out_order);

int lv_dyn_create_point(lvDynGraph *graph, double x, double y);
int lv_dyn_create_line(lvDynGraph *graph, int p1_id, int p2_id);
int lv_dyn_create_circle(lvDynGraph *graph, int center_id, int point_id);
int lv_dyn_create_midpoint(lvDynGraph *graph, int p1_id, int p2_id);
int lv_dyn_create_parallel(lvDynGraph *graph, int base_line_id, int through_point_id);
int lv_dyn_create_perpendicular(lvDynGraph *graph, int base_line_id, int through_point_id);
int lv_dyn_create_distance(lvDynGraph *graph, int p1_id, int p2_id);

void lv_dyn_graph_get_stats(const lvDynGraph *graph, lvDynGraphStats *out_stats);

void lv_dyn_graph_clear_dirty(lvDynGraph *graph);
void lv_dyn_graph_reset_states(lvDynGraph *graph);

typedef struct {
    double x, y, vx, vy;
} lvDynamicPoint;

void lv_geo_dynamic_step(lvDynamicPoint *points, size_t count, double dt);

#ifdef __cplusplus
}
#endif

#endif
