# -*- coding: utf-8 -*-
import io, os, sys

BASE = r"C:\Users\xingg\Desktop\知识体系化Wiki\Lv-00"

def apply(rel, old, new):
    fn = os.path.join(BASE, rel)
    with open(fn, "rb") as f:
        raw = f.read()
    crlf = b"\r\n" in raw
    data = raw.decode("utf-8")
    o = old.replace("\n", "\r\n") if crlf else old
    n = new.replace("\n", "\r\n") if crlf else new
    cnt = data.count(o)
    assert cnt == 1, "FAIL %s: expected 1 match, got %d" % (rel, cnt)
    data = data.replace(o, n)
    with open(fn, "wb") as f:
        f.write(data.encode("utf-8"))
    print("OK  ", rel)

EDITS = []

EDITS.append(("core/include/lv/context.h", '''    /** 已加载的模块引用（不拥有所有权，指向全局模块注册表） */
    void **module_refs;

    /** 模块引用数量 */
    int module_ref_count;

    /** 已加载的公理包引用（不拥有所有权） */
    void **axiom_pkg_refs;

    /** 公理包引用数量 */
    int axiom_pkg_ref_count;''', '''    /** 已加载的模块引用（不拥有所有权，指向全局模块注册表） */
    void **module_refs;

    /** 模块引用数量 */
    int module_ref_count;

    /** 模块引用数组容量（倍增扩容，lv_ensure_capacity 维护） */
    int module_ref_capacity;

    /** 已加载的公理包引用（不拥有所有权） */
    void **axiom_pkg_refs;

    /** 公理包引用数量 */
    int axiom_pkg_ref_count;

    /** 公理包引用数组容量（倍增扩容，lv_ensure_capacity 维护） */
    int axiom_pkg_ref_capacity;'''))

EDITS.append(("core/src/layer1_parser/dsl_compiler_load.c", '''    /* 线性 +1 扩容（ctx 无容量字段，改动最小：保持原样，不迁移到 lv_ensure_capacity） */
    void **np = lv_realloc(ctx->axiom_pkg_refs, sizeof(void *) * (size_t) (ctx->axiom_pkg_ref_count + 1));
    if (!np)
        return false;
    ctx->axiom_pkg_refs = np;
    ctx->axiom_pkg_refs[ctx->axiom_pkg_ref_count] = pkg;
    ctx->axiom_pkg_ref_count++;''', '''    /* 倍增扩容：委托 lv_ensure_capacity（初始 8，此后每次倍增；失败语义与原来一致：返回 false） */
    if (!lv_ensure_capacity((void **) &ctx->axiom_pkg_refs, ctx->axiom_pkg_ref_count, &ctx->axiom_pkg_ref_capacity,
                            sizeof(void *), 1))
        return false;
    ctx->axiom_pkg_refs[ctx->axiom_pkg_ref_count] = pkg;
    ctx->axiom_pkg_ref_count++;'''))

EDITS.append(("core/src/layer6_visual/runtime/block_scheduler.c", '''static int build_block_adjacency(BlockGraphView *bg, int n, int *in_degree, int ***out_adj, int **out_adj_count) {
    int **adj = *out_adj;
    int *adj_count = *out_adj_count;''', '''static int build_block_adjacency(BlockGraphView *bg, int n, int *in_degree, int ***out_adj, int **out_adj_count,
                                 int **out_adj_cap) {
    int **adj = *out_adj;
    int *adj_count = *out_adj_count;
    int *adj_cap = *out_adj_cap;'''))

EDITS.append(("core/src/layer6_visual/runtime/block_scheduler.c", '''                        /* 存在连接：i -> j */
                        /* 线性增长：每次 +1 个元素直接 realloc（adj 无 cap 字段，不引入 lv_ensure_capacity） */
                        adj_count[i]++;
                        int *new_adj = lv_realloc(adj[i], adj_count[i] * sizeof(int));
                        if (new_adj) {
                            adj[i] = new_adj;
                            adj[i][adj_count[i] - 1] = j;
                        }
                        if (in_degree)
                            in_degree[j]++;''', '''                        /* 存在连接：i -> j */
                        /* 倍增扩容：行容量委托 lv_ensure_capacity（初始 8，此后倍增；
                         * 失败时与原始语义一致：跳过该边写入，in_degree 计数仍照旧递增） */
                        if (lv_ensure_capacity((void **) &adj[i], adj_count[i], &adj_cap[i], sizeof(int), 1)) {
                            adj[i][adj_count[i]] = j;
                            adj_count[i]++;
                        }
                        if (in_degree)
                            in_degree[j]++;'''))

EDITS.append(("core/src/layer6_visual/runtime/block_scheduler.c", '''    int *in_degree = lv_calloc(n, sizeof(int));
    int *adj_count = lv_calloc(n, sizeof(int));  /* 每个块的下游邻居数 */
    int **adj = lv_calloc(n, sizeof(int *));     /* 邻接表 */
    int *queue_buf = lv_calloc(n, sizeof(int));  /* 拓扑排序队列 */
    int *topo_order = lv_calloc(n, sizeof(int)); /* 拓扑排序结果 */

    if (!in_degree || !adj_count || !adj || !queue_buf || !topo_order) {
        lv_free_many(&in_degree, &adj_count, &adj, &queue_buf, &topo_order, NULL);''', '''    int *in_degree = lv_calloc(n, sizeof(int));
    int *adj_count = lv_calloc(n, sizeof(int));  /* 每个块的下游邻居数 */
    int *adj_cap = lv_calloc(n, sizeof(int));    /* 每行邻接表容量 */
    int **adj = lv_calloc(n, sizeof(int *));     /* 邻接表 */
    int *queue_buf = lv_calloc(n, sizeof(int));  /* 拓扑排序队列 */
    int *topo_order = lv_calloc(n, sizeof(int)); /* 拓扑排序结果 */

    if (!in_degree || !adj_count || !adj_cap || !adj || !queue_buf || !topo_order) {
        lv_free_many(&in_degree, &adj_count, &adj_cap, &adj, &queue_buf, &topo_order, NULL);'''))

EDITS.append(("core/src/layer6_visual/runtime/block_scheduler.c", '''    build_block_adjacency(bg, n, in_degree, &adj, &adj_count);''', '''    build_block_adjacency(bg, n, in_degree, &adj, &adj_count, &adj_cap);'''))

EDITS.append(("core/src/layer6_visual/runtime/block_scheduler.c", '''    /* 清理 */
    for (int i = 0; i < n; i++)
        lv_free((void **) &adj[i]);
    lv_free_many(&adj, &adj_count, &in_degree, &queue_buf, &topo_order, NULL);''', '''    /* 清理 */
    for (int i = 0; i < n; i++)
        lv_free((void **) &adj[i]);
    lv_free_many(&adj, &adj_cap, &adj_count, &in_degree, &queue_buf, &topo_order, NULL);'''))

EDITS.append(("core/src/layer6_visual/runtime/block_scheduler.c", '''    /* 构建邻接表（同 run 函数） */
    int *adj_count = lv_calloc(n, sizeof(int));
    int **adj = lv_calloc(n, sizeof(int *));
    if (!adj_count || !adj) {
        lv_free_many(&adj_count, &adj, NULL);''', '''    /* 构建邻接表（同 run 函数） */
    int *adj_count = lv_calloc(n, sizeof(int));
    int *adj_cap = lv_calloc(n, sizeof(int)); /* 每行邻接表容量 */
    int **adj = lv_calloc(n, sizeof(int *));
    if (!adj_count || !adj_cap || !adj) {
        lv_free_many(&adj_count, &adj_cap, &adj, NULL);'''))

EDITS.append(("core/src/layer6_visual/runtime/block_scheduler.c", '''    build_block_adjacency(bg, n, NULL, &adj, &adj_count);''', '''    build_block_adjacency(bg, n, NULL, &adj, &adj_count, &adj_cap);'''))

EDITS.append(("core/src/layer6_visual/runtime/block_scheduler.c", '''    if (!need_exec) {
        for (int i = 0; i < n; i++)
            lv_free((void **) &adj[i]);
        lv_free_many(&adj, &adj_count, NULL);''', '''    if (!need_exec) {
        for (int i = 0; i < n; i++)
            lv_free((void **) &adj[i]);
        lv_free_many(&adj, &adj_count, &adj_cap, NULL);'''))

EDITS.append(("core/src/layer6_visual/runtime/block_scheduler.c", '''    /* 清理 */
    lv_free((void **) &need_exec);
    for (int i = 0; i < n; i++)
        lv_free((void **) &adj[i]);
    lv_free((void **) &adj);
    lv_free((void **) &adj_count);''', '''    /* 清理 */
    lv_free((void **) &need_exec);
    for (int i = 0; i < n; i++)
        lv_free((void **) &adj[i]);
    lv_free((void **) &adj);
    lv_free((void **) &adj_count);
    lv_free((void **) &adj_cap);'''))

EDITS.append(("core/src/core/lv_convenience.c", '''    {
        void **np = lv_realloc(ctx->module_refs, sizeof(void *) * (size_t) (ctx->module_ref_count + 1));
        if (!np) {
            preset_release(entry);
            ctx->error_code = lv_ERROR_OUT_OF_MEMORY;
            snprintf(ctx->error_message, sizeof(ctx->error_message), "lv_preset_load: 模块引用数组扩容失败");
            return -4;
        }
        ctx->module_refs = np;
    }
    ctx->module_refs[ctx->module_ref_count] = (void *) entry;
    ctx->module_ref_count++;''', '''    {
        /* 倍增扩容：委托 lv_ensure_capacity（初始 8，此后每次倍增；失败语义与原来一致：返回 -4） */
        if (!lv_ensure_capacity((void **) &ctx->module_refs, ctx->module_ref_count, &ctx->module_ref_capacity,
                                sizeof(void *), 1)) {
            preset_release(entry);
            ctx->error_code = lv_ERROR_OUT_OF_MEMORY;
            snprintf(ctx->error_message, sizeof(ctx->error_message), "lv_preset_load: 模块引用数组扩容失败");
            return -4;
        }
    }
    ctx->module_refs[ctx->module_ref_count] = (void *) entry;
    ctx->module_ref_count++;'''))

EDITS.append(("core/src/layer4_reasoning/rewrite/rewrite_vf2.c", '''        pattern_graph->nodes =
            lv_realloc(pattern_graph->nodes, (size_t) (pattern_graph->node_count + 1) * sizeof(GeomNode *));
        pattern_graph->nodes[pattern_graph->node_count++] = node;''', '''        /* 倍增扩容：复用 ConstraintGraph 既有 node_capacity 字段（与 graph_node_alloc.c 同款模式） */
        if (!lv_ensure_capacity((void **) &pattern_graph->nodes, pattern_graph->node_count,
                                &pattern_graph->node_capacity, sizeof(GeomNode *), 1)) {
            lv_free((void **) &node);
            graph_destroy(pattern_graph);
            lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "vf2_find_match: pattern_graph nodes 扩容失败");
        }
        pattern_graph->nodes[pattern_graph->node_count++] = node;'''))

EDITS.append(("core/src/layer4_reasoning/rewrite/beta_reduce.c", '''    int *affected_target_ids = NULL;
    int affected_count = 0;''', '''    int *affected_target_ids = NULL;
    int affected_count = 0;
    int affected_cap = 0; /* affected_target_ids 容量（倍增扩容，lv_ensure_capacity 维护） */'''))

EDITS.append(("core/src/layer4_reasoning/rewrite/beta_reduce.c", '''        /* 记录受影响的约束 ID，以便后续移除旧约束 */
        int *tmp = lv_realloc(affected_target_ids, (size_t) (affected_count + 1) * sizeof(int));
        if (tmp) {
            affected_target_ids = tmp;
            affected_target_ids[affected_count++] = ci;
        }''', '''        /* 记录受影响的约束 ID，以便后续移除旧约束（倍增扩容；失败时与原始语义一致：跳过记录） */
        if (lv_ensure_capacity((void **) &affected_target_ids, affected_count, &affected_cap, sizeof(int), 1))
            affected_target_ids[affected_count++] = ci;'''))

EDITS.append(("core/include/lv/proof.h", '''    /* 策略信息 */
    char *current_strategy;      /**< 当前使用的搜索策略名称 */
    char **available_strategies; /**< 可用策略名称列表 */
    int strategy_count;          /**< 策略数量 */
} ProofSearchTree;''', '''    /* 策略信息 */
    char *current_strategy;      /**< 当前使用的搜索策略名称 */
    char **available_strategies; /**< 可用策略名称列表 */
    int strategy_count;          /**< 策略数量 */
    int strategy_capacity;       /**< 策略数组容量（倍增扩容，lv_ensure_capacity 维护） */
} ProofSearchTree;'''))

EDITS.append(("core/src/layer4_reasoning/proof/proof_dependency.c", '''    tree->available_strategies = NULL;
    tree->strategy_count = 0;''', '''    tree->available_strategies = NULL;
    tree->strategy_count = 0;
    tree->strategy_capacity = 0;'''))

EDITS.append(("core/src/layer4_reasoning/proof/proof_dependency.c", '''    /* 扩展策略数组 */
    char **new_strats = lv_realloc(tree->available_strategies, (tree->strategy_count + 1) * sizeof(char *));
    if (!new_strats)
        return;
    tree->available_strategies = new_strats;''', '''    /* 扩展策略数组（倍增扩容：初始 8，此后每次倍增；失败语义与原来一致：直接返回） */
    if (!lv_ensure_capacity((void **) &tree->available_strategies, tree->strategy_count, &tree->strategy_capacity,
                            sizeof(char *), 1))
        return;'''))

EDITS.append(("core/include/lv/solver.h", '''typedef struct GroebnerResult {
    SymbolicCoord **solutions; /**< 解数组 */
    int solution_count;        /**< 解的数量 */''', '''typedef struct GroebnerResult {
    SymbolicCoord **solutions; /**< 解数组 */
    int solution_count;        /**< 解的数量 */
    int solution_capacity;     /**< 解数组容量（倍增扩容，lv_ensure_capacity 维护） */'''))

EDITS.append(("core/src/layer4_reasoning/solver/solver_symbolic.c", '''    SymbolicCoord **new_arr =
        lv_realloc(result->solutions, (size_t) (result->solution_count + 1) * sizeof(SymbolicCoord *));
    if (!new_arr) {
        symbolic_coord_destroy(sol);
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "append_solution: 扩容失败");
    }
    result->solutions = new_arr;
    result->solutions[result->solution_count++] = sol;
    return 0;''', '''    /* 倍增扩容：委托 lv_ensure_capacity（初始 8，此后每次倍增；失败语义与原来一致：销毁 sol 并返回错误） */
    if (!lv_ensure_capacity((void **) &result->solutions, result->solution_count, &result->solution_capacity,
                            sizeof(SymbolicCoord *), 1)) {
        symbolic_coord_destroy(sol);
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "append_solution: 扩容失败");
    }
    result->solutions[result->solution_count++] = sol;
    return 0;'''))

EDITS.append(("core/src/layer4_reasoning/solver/solver_symbolic.c", '''    lv_free((void **) &result->solutions);
    result->solutions = NULL;
    result->solution_count = 0;''', '''    lv_free((void **) &result->solutions);
    result->solutions = NULL;
    result->solution_count = 0;
    result->solution_capacity = 0; /* 保持 容量=0 无分配 的不变量，支持结果复用 */'''))

EDITS.append(("core/include/lv/relation_model.h", '''    RelFormula **facts; /**< 事实公式数组 */
    int fact_count;     /**< 事实数量 */

    RelFormula **assertions; /**< 断言公式数组 */
    int assertion_count;     /**< 断言数量 */''', '''    RelFormula **facts; /**< 事实公式数组 */
    int fact_count;     /**< 事实数量 */
    int fact_capacity;  /**< 事实数组容量（倍增扩容，lv_ensure_capacity 维护） */

    RelFormula **assertions; /**< 断言公式数组 */
    int assertion_count;     /**< 断言数量 */
    int assertion_capacity;  /**< 断言数组容量（倍增扩容，lv_ensure_capacity 维护） */'''))

EDITS.append(("core/src/layer4_reasoning/model/relation_model.c", '''    /* 线性 +1 扩容（facts 数量通常很小，保持最小改动；如需倍增可引入 fact_capacity 字段） */
    RelFormula **new_facts =
        (RelFormula **) lv_realloc(model->facts, (size_t) (model->fact_count + 1) * sizeof(RelFormula *));
    if (!new_facts)
        return false;
    model->facts = new_facts;
    model->facts[model->fact_count++] = formula;
    return true;''', '''    /* 倍增扩容：委托 lv_ensure_capacity（初始 8，此后每次倍增；失败语义与原来一致：返回 false） */
    if (!lv_ensure_capacity((void **) &model->facts, model->fact_count, &model->fact_capacity,
                            sizeof(RelFormula *), 1))
        return false;
    model->facts[model->fact_count++] = formula;
    return true;'''))

EDITS.append(("core/src/layer4_reasoning/model/relation_model.c", '''    /* 线性 +1 扩容（assertions 数量通常很小，保持最小改动；如需倍增可引入 assertion_capacity 字段） */
    RelFormula **new_asserts =
        (RelFormula **) lv_realloc(model->assertions, (size_t) (model->assertion_count + 1) * sizeof(RelFormula *));
    if (!new_asserts)
        return false;
    model->assertions = new_asserts;
    model->assertions[model->assertion_count++] = formula;
    return true;''', '''    /* 倍增扩容：委托 lv_ensure_capacity（初始 8，此后每次倍增；失败语义与原来一致：返回 false） */
    if (!lv_ensure_capacity((void **) &model->assertions, model->assertion_count, &model->assertion_capacity,
                            sizeof(RelFormula *), 1))
        return false;
    model->assertions[model->assertion_count++] = formula;
    return true;'''))

for rel, old, new in EDITS:
    apply(rel, old, new)
print("ALL EDITS APPLIED:", len(EDITS))