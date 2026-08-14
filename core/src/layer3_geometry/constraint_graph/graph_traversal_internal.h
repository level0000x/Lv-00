#ifndef GRAPH_TRAVERSAL_INTERNAL_H
#define GRAPH_TRAVERSAL_INTERNAL_H

#include "lv/constraint_graph.h"
#include "lv/lv_graph_traversal.h"

/* 以下均定义于 graph_traversal_dfs.c（由 lv_graph_traversal.c 拆出，原 static，
 * C-⑭ 去 static）：图遍历 API 面（lv_graph_traversal.c）与便利函数段
 * （graph_traversal_util.c）复用。签名以源文件实际定义为准，逐字匹配。 */

int get_max_node_id(const ConstraintGraph *graph);

int find_neighbors(ConstraintGraph *graph, int node_id,
                   lvDArray *out_neighbors,
                   const lvGraphTraversalConfig *config);

int dfs_traverse_from(ConstraintGraph *graph, int start_id,
                      bool *visited, int visited_size,
                      lvGraphNodeVisitor visitor, void *user_data,
                      const lvGraphTraversalConfig *config,
                      int base_depth);

int bfs_traverse_from(ConstraintGraph *graph, int start_id,
                      bool *visited, int visited_size,
                      lvGraphNodeVisitor visitor, void *user_data,
                      const lvGraphTraversalConfig *config,
                      int base_depth);

int traverse_all_components(ConstraintGraph *graph,
                            bool *visited, int visited_size,
                            lvGraphNodeVisitor visitor, void *user_data,
                            const lvGraphTraversalConfig *config);

int topological_order_traverse(ConstraintGraph *graph, bool *visited, int visited_size,
                               lvGraphNodeVisitor visitor, void *user_data,
                               const lvGraphTraversalConfig *config);

int traverse_from_topological(ConstraintGraph *graph, int start_node_id,
                              bool *visited, int visited_size,
                              lvGraphNodeVisitor visitor, void *user_data,
                              const lvGraphTraversalConfig *config);

int mark_reachable_from(ConstraintGraph *graph, int start_id,
                        bool *reachable, int reachable_size,
                        const lvGraphTraversalConfig *config);

#endif /* GRAPH_TRAVERSAL_INTERNAL_H */
