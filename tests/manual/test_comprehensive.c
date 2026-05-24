#include "lv00.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

void test_trust_colors() {
    printf("Testing trust color system...\n");
    
    ConstraintGraph *graph = graph_create();
    SymbolicCoord *c = symbolic_coord_create_rational(1, 1);
    graph_add_point(graph, &c, 1);
    
    GeomNode *node = graph_get_node(graph, 0);
    assert(node->trust == TRUST_GREEN);
    
    node->trust = TRUST_LIGHT_ORANGE;
    node->lo_subtype = LIGHT_ORANGE_ORACLE;
    assert(node->trust == TRUST_LIGHT_ORANGE);
    assert(node->lo_subtype == LIGHT_ORANGE_ORACLE);
    
    node->trust = TRUST_AMBER;
    node->numeric_precision = 1e-6;
    node->numeric_assumption_declaration = strdup("Testing precision");
    assert(node->trust == TRUST_AMBER);
    
    printf("  Trust colors: PASSED\n");
    
    lv00_free_ptr(node->numeric_assumption_declaration);
    graph_destroy(graph);
    printf("  PASSED\n");
}

void test_merge_candidates() {
    printf("Testing merge candidate detection...\n");
    
    ConstraintGraph *graph = graph_create();
    SymbolicCoord *c = symbolic_coord_create_rational(5, 2);
    
    graph_add_point(graph, &c, 1);
    graph_add_point(graph, &c, 1);
    assert(graph->node_count == 2);
    
    int count = 0;
    NodeMergeCandidate *candidates = find_merge_candidates(graph, &count);
    assert(count == 1);
    assert(candidates[0].node_a_id == 0);
    assert(candidates[0].node_b_id == 1);
    assert(candidates[0].scope_a == candidates[0].scope_b);
    
    printf("  Candidate detection: PASSED\n");
    
    lv00_free_ptr(candidates);
    graph_destroy(graph);
    printf("  PASSED\n");
}

int main() {
    printf("=== Lv-00 Comprehensive Test Suite ===\n\n");
    
    test_trust_colors();
    test_merge_candidates();
    
    printf("\n=== All comprehensive tests PASSED! ===\n");
    return 0;
}
