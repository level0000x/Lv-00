$ErrorActionPreference = 'Stop'
$enc = New-Object System.Text.UTF8Encoding($false)

function Do-Replace {
    param([string]$path, [string]$old, [string]$new)
    $content = [System.IO.File]::ReadAllText($path)
    $idx = $content.IndexOf($old)
    if ($idx -lt 0) { Write-Output "NOT FOUND: $path"; return 1 }
    $idx2 = $content.IndexOf($old, $idx + 1)
    if ($idx2 -ge 0) { Write-Output "MULTIPLE MATCHES: $path"; return 2 }
    $content = $content.Remove($idx, $old.Length).Insert($idx, $new)
    [System.IO.File]::WriteAllText($path, $content, $enc)
    Write-Output "OK: $path"
    return 0
}

$g = 'c:\Users\xingg\Desktop\*\Lv-00\core\src\layer3_geometry\geo_dynamic.c'
$p = (Get-ChildItem $g | Select-Object -First 1).FullName

# ---------- geo_dynamic.c #1: ensure_id_map_capacity ----------
$old = @'
    int *new_map = (int *) lv_realloc(graph->id_to_index, new_capacity * sizeof(int));
    if (!new_map)
        return false;

    for (int i = graph->node_capacity; i < new_capacity; i++) {
        new_map[i] = lv_DYN_INVALID;
    }

    graph->id_to_index = new_map;
    graph->node_capacity = new_capacity;
    return true;
}
'@

$new = @'
    int old_capacity = graph->node_capacity;
    /* min_growth makes min_required = new_capacity, so new capacity >= target */
    if (!lv_ensure_capacity((void **) &graph->id_to_index, old_capacity,
                            &graph->node_capacity, sizeof(int),
                            new_capacity - old_capacity))
        return false;

    /* initialize new segment to lv_DYN_INVALID */
    for (int i = old_capacity; i < graph->node_capacity; i++) {
        graph->id_to_index[i] = lv_DYN_INVALID;
    }

    return true;
}
'@
Do-Replace $p $old $new

# ---------- geo_dynamic.c #2: ensure_adj_capacity ----------
$old = @'
    int new_cap = graph->adj_capacity * 2;
    while (new_cap < needed)
        new_cap *= 2;

    int *new_parent = (int *) lv_realloc(graph->parent_adj, new_cap * sizeof(int));
    int *new_child = (int *) lv_realloc(graph->child_adj, new_cap * sizeof(int));

    if (!new_parent || !new_child) {
        if (new_parent)
            lv_free((void **) &(new_parent));
        if (new_child)
            lv_free((void **) &(new_child));
        return false;
    }

    graph->parent_adj = new_parent;
    graph->child_adj = new_child;
    graph->adj_capacity = new_cap;
    return true;
}
'@

$new = @'
    int old_cap = graph->adj_capacity;

    /* first: grow parent_adj (adj_capacity updated to new capacity) */
    if (!lv_ensure_capacity((void **) &graph->parent_adj, old_cap,
                            &graph->adj_capacity, sizeof(int), needed - old_cap))
        return false;

    /* second: grow child_adj to the same capacity.  Temporarily rewind the
     * capacity pointer so the call really executes, keeping both arrays in
     * sync; on failure release BOTH arrays and restore the old capacity. */
    graph->adj_capacity = old_cap;
    if (!lv_ensure_capacity((void **) &graph->child_adj, old_cap,
                            &graph->adj_capacity, sizeof(int), needed - old_cap)) {
        lv_free((void **) &graph->parent_adj);
        lv_free((void **) &graph->child_adj);
        graph->adj_capacity = old_cap;
        return false;
    }
    return true;
}
'@
Do-Replace $p $old $new

# ---------- geo_dynamic.c #3: node arrays growth in lv_dyn_graph_add_node ----------
$old = @'
    if (graph->node_count >= graph->node_capacity) {
        int new_cap = graph->node_capacity * 2;
        lvDynNode *new_nodes = (lvDynNode *) lv_realloc(graph->nodes, new_cap * sizeof(lvDynNode));
        if (!new_nodes)
            return lv_DYN_INVALID;

        int *new_parent_offsets = (int *) lv_realloc(graph->parent_adj_offsets, (new_cap + 1) * sizeof(int));
        int *new_child_offsets = (int *) lv_realloc(graph->child_adj_offsets, (new_cap + 1) * sizeof(int));

        if (!new_parent_offsets || !new_child_offsets) {
            if (new_parent_offsets)
                lv_free((void **) &(new_parent_offsets));
            if (new_child_offsets)
                lv_free((void **) &(new_child_offsets));
            return lv_DYN_INVALID;
        }

        graph->nodes = new_nodes;
        graph->parent_adj_offsets = new_parent_offsets;
        graph->child_adj_offsets = new_child_offsets;
        graph->node_capacity = new_cap;
    }
'@

$new = @'
    if (graph->node_count >= graph->node_capacity) {
        int old_cap = graph->node_capacity;

        /* first: grow nodes (node_capacity updated to the new capacity) */
        if (!lv_ensure_capacity((void **) &graph->nodes, old_cap,
                                &graph->node_capacity, sizeof(lvDynNode), old_cap))
            return lv_DYN_INVALID;
        int new_cap = graph->node_capacity;

        /* offset arrays hold node_capacity + 1 elements and must grow in sync
         * with nodes.  Rewind the shared capacity pointer before each call so
         * the growth really executes; on any failure release ALL three arrays
         * and restore the old capacity. */
        for (int pass = 0; pass < 2; pass++) {
            graph->node_capacity = old_cap; /* temporary rewind */
            if (!lv_ensure_capacity((void **) (pass == 0 ? &graph->parent_adj_offsets
                                                         : &graph->child_adj_offsets),
                                    old_cap + 1, &graph->node_capacity, sizeof(int),
                                    new_cap - old_cap)) {
                lv_free((void **) &graph->nodes);
                lv_free((void **) &graph->parent_adj_offsets);
                lv_free((void **) &graph->child_adj_offsets);
                graph->node_capacity = old_cap;
                return lv_DYN_INVALID;
            }
        }
        graph->node_capacity = new_cap;
    }
'@
Do-Replace $p $old $new

Write-Output "DONE"
