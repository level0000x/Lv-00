/* geometric_primitives.c -- 13 个 geo_* 原语统一包装层实现
 * C11 标准，NULL 指针安全，中文注释。 */

#include "lv/geometric_primitives.h"
#include <stdlib.h>
#include <string.h>
#include "constraint_graph.h"
#include "engine.h"
#include "func_block.h"
#include "normalization.h"
#include "proof.h"
#include "rewrite.h"
#include "symbolic_coord.h"
#include "unify.h"

#define CHECK_GRAPH(g)  do { if (!(g)) return (GeoResult){GEO_STATUS_NULL_ARG, NULL, "graph NULL"}; } while(0)
#define CHECK_ENGINE(e) do { if (!(e)) return (GeoResult){GEO_STATUS_NULL_ARG, NULL, "engine NULL"}; } while(0)

static const GeoResult s_ok = {GEO_STATUS_OK, NULL, NULL};
static inline GeoResult geo_err(GeoStatus s, const char *m) { GeoResult r = {s, NULL, m}; return r; }
static inline GeoResult geo_ok(void *d) { GeoResult r = {GEO_STATUS_OK, d, NULL}; return r; }
static int *geo_dup_int(int v) { int *p = (int *)malloc(sizeof(int)); if (p) *p = v; return p; }

/* 原语 1: geo_create_node -- 创建几何节点（POINT/LINE/REGION/PORT） */
GeoResult geo_create_node(ConstraintGraph *graph, GeoNodeType type,
                          const int *ids, int count)
{
    CHECK_GRAPH(graph);
    AddNodeResult res;

    switch (type) {
    case GEO_NODE_POINT: {
        if (!ids || count < 1)
            return geo_err(GEO_STATUS_INVALID_PARAM, "点至少需要一个坐标维度");
        SymbolicCoord **c = (SymbolicCoord **)malloc((size_t)count * sizeof(SymbolicCoord *));
        if (!c) return geo_err(GEO_STATUS_INTERNAL_ERROR, "内存分配失败");
        for (int i = 0; i < count; i++)
            c[i] = symbolic_coord_create_rational((int64_t)ids[i], 1);
        res = graph_add_point(graph, c, count);
        for (int i = 0; i < count; i++) symbolic_coord_destroy(c[i]);
        free(c);
        break;
    }
    case GEO_NODE_LINE_SEGMENT:
        if (!ids || count < 2) return geo_err(GEO_STATUS_INVALID_PARAM, "线段需2个端点");
        res = graph_add_line_segment(graph, ids[0], ids[1]);
        break;
    case GEO_NODE_REGION:
        if (!ids || count < 1) return geo_err(GEO_STATUS_INVALID_PARAM, "区域需边界线段");
        res = graph_add_region(graph, ids, count);
        break;
    case GEO_NODE_PORT:
        if (!ids || count < 3) return geo_err(GEO_STATUS_INVALID_PARAM, "端口需3个参数");
        res = graph_add_port(graph, (PortType)ids[0], ids[1], ids[2]);
        break;
    case GEO_NODE_FUNCTION_BLOCK:
        return geo_err(GEO_STATUS_UNSUPPORTED, "函数块请使用 geo_pack");
    default:
        return geo_err(GEO_STATUS_INVALID_TYPE, "未知节点类型");
    }

    if (res != ADD_NODE_OK)
        return geo_err(GEO_STATUS_CONFLICT, "添加节点失败");
    int *out = geo_dup_int(graph_get_last_added_node_id(graph));
    return out ? geo_ok(out) : geo_err(GEO_STATUS_INTERNAL_ERROR, "内存分配失败");
}

/* 原语 2: geo_create_constraint -- 创建约束关系 */
GeoResult geo_create_constraint(ConstraintGraph *graph, GeoConstraintType type,
                                const int *p, int n)
{
    CHECK_GRAPH(graph);
    if (!p || n < 2) return geo_err(GEO_STATUS_INVALID_PARAM, "参与者不足");

    AddConstraintResult res;
    switch (type) {
    case GEO_CONSTRAINT_INCIDENCE:
        res = graph_add_incidence(graph, p[0], p[1]); break;
    case GEO_CONSTRAINT_BETWEENNESS:
        if (n < 3) return geo_err(GEO_STATUS_INVALID_PARAM, "之间需3个参与者");
        res = graph_add_betweenness(graph, p[0], p[1], p[2]); break;
    case GEO_CONSTRAINT_INTERSECTION:
        if (n < 3) return geo_err(GEO_STATUS_INVALID_PARAM, "相交需3个参与者");
        res = graph_add_intersection(graph, p[0], p[1], p[2]); break;
    case GEO_CONSTRAINT_CONTAINMENT:
        res = graph_add_containment(graph, p[0], p[1]); break;
    case GEO_CONSTRAINT_CONNECTION:
        res = graph_add_connection(graph, p[0], p[1]); break;
    default:
        return geo_err(GEO_STATUS_INVALID_TYPE, "未知约束类型");
    }
    if (res == ADD_CONSTRAINT_CONFLICT) return geo_err(GEO_STATUS_CONFLICT, "约束冲突");
    if (res == ADD_CONSTRAINT_DUPLICATE) return geo_err(GEO_STATUS_CONFLICT, "约束重复");
    return s_ok;
}

/* 原语 3: geo_solve -- 求解约束系统 */
GeoResult geo_solve(lvEngine *engine)
{
    CHECK_ENGINE(engine);
    switch (engine_solve(engine)) {
    case ENGINE_SOLVE_OK:       return s_ok;
    case ENGINE_SOLVE_CONFLICT: return geo_err(GEO_STATUS_CONFLICT, "求解冲突");
    case ENGINE_SOLVE_TIMEOUT:  return geo_err(GEO_STATUS_TIMEOUT, "求解超时");
    default:                    return geo_err(GEO_STATUS_INTERNAL_ERROR, "求解错误");
    }
}

/* 原语 4: geo_normalize -- 约束图归一化 */
GeoResult geo_normalize(ConstraintGraph *graph, bool scope_aware)
{
    CHECK_GRAPH(graph);
    NormalizationResult *nr = graph_normalize(graph, scope_aware);
    if (!nr) return geo_err(GEO_STATUS_INTERNAL_ERROR, "归一化失败");

    int *merged = geo_dup_int(nr->merged_count);
    normalization_result_destroy(nr);
    return merged ? geo_ok(merged) : geo_err(GEO_STATUS_INTERNAL_ERROR, "内存分配失败");
}

/* 原语 5: geo_rewrite -- 应用重写规则 */
GeoResult geo_rewrite(ConstraintGraph *graph, void **rules,
                      int rule_count, int step_limit)
{
    CHECK_GRAPH(graph);
    if (!rules || rule_count < 1)
        return geo_err(GEO_STATUS_INVALID_PARAM, "规则数组为空");
    if (step_limit <= 0) step_limit = 1000;

    RewriteStatus s = rewrite_with_rules(graph, (RewriteRule **)rules,
                                         rule_count, step_limit, true);
    switch (s) {
    case REWRITE_STATUS_OK:
    case REWRITE_STATUS_NO_MATCH:
    case REWRITE_STATUS_TERMINATED:
        return geo_ok(geo_dup_int((int)s));
    case REWRITE_STATUS_CONFLUENCE_ISSUE:
        return geo_err(GEO_STATUS_CONFLICT, "汇流性问题");
    default:
        return geo_err(GEO_STATUS_INTERNAL_ERROR, "重写错误");
    }
}

/* 原语 6: geo_unify -- 统一构造与命题 */
GeoResult geo_unify(const ConstraintGraph *construction,
                    const ConstraintGraph *proposition)
{
    if (!construction) return geo_err(GEO_STATUS_NULL_ARG, "构造图 NULL");
    if (!proposition)  return geo_err(GEO_STATUS_NULL_ARG, "命题图 NULL");

    UnifyStatus s = unify_construction_with_proposition(construction, proposition);
    switch (s) {
    case UNIFY_STATUS_OK:                  return s_ok;
    case UNIFY_STATUS_PORT_TYPE_MISMATCH:  return geo_err(GEO_STATUS_CONFLICT, "端口类型不匹配");
    case UNIFY_STATUS_CONSTRAINT_MISMATCH: return geo_err(GEO_STATUS_CONFLICT, "约束不匹配");
    case UNIFY_STATUS_COORD_MISMATCH:      return geo_err(GEO_STATUS_CONFLICT, "坐标不匹配");
    case UNIFY_STATUS_STRUCTURE_MISMATCH:  return geo_err(GEO_STATUS_CONFLICT, "结构不匹配");
    case UNIFY_STATUS_SCOPE_MISMATCH:      return geo_err(GEO_STATUS_CONFLICT, "作用域不匹配");
    default:                               return geo_err(GEO_STATUS_INTERNAL_ERROR, "合一失败");
    }
}

/* 原语 7: geo_pack -- 打包为函数块 */
GeoResult geo_pack(ConstraintGraph *graph, const int *internal_ids,
                   int internal_count, const int *in_ports, int in_count,
                   const int *out_ports, int out_count)
{
    CHECK_GRAPH(graph);
    if (!internal_ids || internal_count < 1)
        return geo_err(GEO_STATUS_INVALID_PARAM, "内部节点为空");

    FuncBlock *fb = NULL;
    PackResult r = func_block_pack(graph, internal_ids, internal_count,
                                   in_ports, in_count, out_ports, out_count,
                                   NULL, 0, &fb);
    switch (r) {
    case PACK_RESULT_OK: {
        int *id = geo_dup_int(func_block_get_id(fb));
        func_block_destroy(fb);
        return id ? geo_ok(id) : geo_err(GEO_STATUS_INTERNAL_ERROR, "内存分配失败");
    }
    case PACK_RESULT_CROSS_BOUNDARY_CONFLICT:
        return geo_err(GEO_STATUS_CONFLICT, "跨边界约束冲突");
    case PACK_RESULT_INVALID_NODES:
        return geo_err(GEO_STATUS_INVALID_PARAM, "无效内部节点");
    case PACK_RESULT_INVALID_PORTS:
        return geo_err(GEO_STATUS_INVALID_PARAM, "无效端口");
    default:
        return geo_err(GEO_STATUS_INTERNAL_ERROR, "打包错误");
    }
}

/* 原语 8: geo_instantiate -- 实例化函数块 */
GeoResult geo_instantiate(ConstraintGraph *graph, lvEngine *engine,
                          int func_block_id, const int *args, int arg_count)
{
    CHECK_GRAPH(graph);
    CHECK_ENGINE(engine);
    if (!args || arg_count < 1)
        return geo_err(GEO_STATUS_INVALID_PARAM, "参数映射为空");

    /* 底层 API 要求非 const，需拷贝 */
    int *mutable_args = (int *)malloc((size_t)arg_count * sizeof(int));
    if (!mutable_args) return geo_err(GEO_STATUS_INTERNAL_ERROR, "内存分配失败");
    memcpy(mutable_args, args, (size_t)arg_count * sizeof(int));

    int result_count = 0;
    int *nodes = engine_instantiate_function(engine, func_block_id,
                                             mutable_args, arg_count,
                                             &result_count);
    free(mutable_args);
    if (!nodes) return geo_err(GEO_STATUS_NO_SOLUTION, "实例化失败");

    /* 打包为 [count, id0, id1, ...] */
    int *out = (int *)malloc((size_t)(result_count + 1) * sizeof(int));
    if (!out) { free(nodes); return geo_err(GEO_STATUS_INTERNAL_ERROR, "内存分配失败"); }
    out[0] = result_count;
    memcpy(&out[1], nodes, (size_t)result_count * sizeof(int));
    free(nodes);
    return geo_ok(out);
}

/* 原语 9: geo_prove -- 执行证明搜索 */
GeoResult geo_prove(ProofNavigator *nav, int strategy, int max_steps)
{
    if (!nav) return geo_err(GEO_STATUS_NULL_ARG, "导航器 NULL");
    if (max_steps <= 0) max_steps = 1000;

    return proof_search_with_strategy(nav, (ProofStrategyType)strategy, max_steps)
        ? s_ok : geo_err(GEO_STATUS_NO_SOLUTION, "证明搜索失败");
}

/* 原语 10: geo_export -- 导出结果 (html/latex/coq) */
GeoResult geo_export(ProofNavigator *nav, const char *format, const char *path)
{
    if (!nav)    return geo_err(GEO_STATUS_NULL_ARG, "导航器 NULL");
    if (!format) return geo_err(GEO_STATUS_NULL_ARG, "格式 NULL");
    if (!path)   return geo_err(GEO_STATUS_NULL_ARG, "路径 NULL");

    bool ok;
    if      (strcmp(format, "html")  == 0) ok = proof_export_html(nav, path);
    else if (strcmp(format, "latex") == 0) ok = proof_export_latex(nav, path);
    else if (strcmp(format, "coq")   == 0) ok = proof_export_coq(nav, path);
    else return geo_err(GEO_STATUS_UNSUPPORTED, "不支持的导出格式");

    return ok ? s_ok : geo_err(GEO_STATUS_IO_ERROR, "导出写入失败");
}

/* 原语 11: geo_serialize -- 序列化约束图为 JSON */
GeoResult geo_serialize(const ConstraintGraph *graph)
{
    if (!graph) return geo_err(GEO_STATUS_NULL_ARG, "约束图 NULL");

    char *json = graph_serialize_to_json(graph);
    if (!json) return geo_err(GEO_STATUS_INTERNAL_ERROR,
                              graph_get_serialize_error(graph));
    return geo_ok(json);
}

/* 原语 12: geo_deserialize -- 从 JSON 反序列化约束图 */
GeoResult geo_deserialize(const char *json)
{
    if (!json) return geo_err(GEO_STATUS_NULL_ARG, "JSON NULL");

    ConstraintGraph *g = graph_deserialize_from_json(json);
    if (!g) return geo_err(GEO_STATUS_INVALID_PARAM, "JSON 解析失败");
    return geo_ok(g);
}

/* 原语 13: geo_query -- 查询约束图 (node/constraint/count) */
GeoResult geo_query(const ConstraintGraph *graph, const char *query,
                    int target_id)
{
    if (!graph) return geo_err(GEO_STATUS_NULL_ARG, "约束图 NULL");
    if (!query) return geo_err(GEO_STATUS_NULL_ARG, "查询类型 NULL");

    if (strcmp(query, "node") == 0) {
        GeomNode *n = graph_get_node(graph, target_id);
        if (!n) return geo_err(GEO_STATUS_NOT_FOUND, "节点未找到");
        GeoResult r = {GEO_STATUS_OK, (void *)n, NULL};
        return r;
    }
    if (strcmp(query, "constraint") == 0) {
        Constraint *c = graph_get_constraint(graph, target_id);
        if (!c) return geo_err(GEO_STATUS_NOT_FOUND, "约束未找到");
        GeoResult r = {GEO_STATUS_OK, (void *)c, NULL};
        return r;
    }
    if (strcmp(query, "count") == 0) {
        int *cnt = (int *)malloc(2 * sizeof(int));
        if (!cnt) return geo_err(GEO_STATUS_INTERNAL_ERROR, "内存分配失败");
        cnt[0] = graph_get_node_count(graph);
        cnt[1] = graph_get_constraint_count(graph);
        return geo_ok(cnt);
    }
    return geo_err(GEO_STATUS_INVALID_PARAM, "未知查询类型");
}
