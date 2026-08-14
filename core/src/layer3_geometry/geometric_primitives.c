/**
 * @file geometric_primitives.c
 * @brief 13 个几何原语统一包装层实现
 *
 * @details 本模块是外部调用者与约束图之间的门面（Facade）层，提供：
 *          - geo_create_node: 创建几何节点（点、线、区域、端口）
 *          - geo_add_constraint: 添加约束（关联、包含、相交、共面等）
 *          - geo_get_node / geo_set_node: 节点查询与属性修改
 *          - geo_delete_node / geo_add_point_line: 节点删除与点线构造
 *
 *          所有包装函数均处理 GeomNodeType ↔ GeoNodeType、
 *          ConstraintType ↔ GeoConstraintType 之间的枚举映射，
 *          并对 GeomNode 聚合体进行安全深拷贝。
 *
 *          内存安全：所有函数通过 CHECK_GRAPH/CHECK_ENGINE 宏进行
 *          NULL 指针检查，返回 GEO_STATUS_NULL_ARG 错误码。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date 2026-05-24
 */

#include "lv/geometric_primitives.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_utils.h"
#include "lv/lv_xmacro.h"

#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/engine.h"
#include "lv/func_block.h"
#include "lv/normalization.h"
#include "lv/proof.h"
#include "lv/rewrite.h"
#include "lv/symbolic_coord.h"
#include "lv/unify.h"

#define CHECK_GRAPH(g)                                                    \
    do {                                                                  \
        if (!(g))                                                         \
            return (GeoResult) {GEO_STATUS_NULL_ARG, NULL, "graph NULL"}; \
    } while (0)
#define CHECK_ENGINE(e)                                                    \
    do {                                                                   \
        if (!(e))                                                          \
            return (GeoResult) {GEO_STATUS_NULL_ARG, NULL, "engine NULL"}; \
    } while (0)

static const GeoResult s_ok = {GEO_STATUS_OK, NULL, NULL};
static inline GeoResult geo_err(GeoStatus s, const char *m) {
    GeoResult r = {s, NULL, m};
    return r;
}
static inline GeoResult geo_ok(void *d) {
    GeoResult r = {GEO_STATUS_OK, d, NULL};
    return r;
}
static int *geo_dup_int(int v) {
    int *p = (int *) lv_malloc(sizeof(int));
    if (p)
        *p = v;
    return p;
}

/* ── 节点创建辅助函数：将 AddNodeResult 包装为 GeoResult ── */
static GeoResult geo_create_node_finish(AddNodeResult res, ConstraintGraph *graph) {
    if (res != ADD_NODE_OK)
        return geo_err(GEO_STATUS_CONFLICT, "添加节点失败");
    int *out = geo_dup_int(graph_get_last_added_node_id(graph));
    return out ? geo_ok(out) : geo_err(GEO_STATUS_INTERNAL_ERROR, "内存分配失败");
}

/* ── 各类型节点创建处理函数 ── */
static GeoResult geo_create_node_point(ConstraintGraph *graph, const int *ids, int count) {
    if (!ids || count < 1)
        return geo_err(GEO_STATUS_INVALID_PARAM, "点至少需要一个坐标维度");
    SymbolicCoord **c = (SymbolicCoord **) lv_malloc((size_t) count * sizeof(SymbolicCoord *));
    if (!c)
        return geo_err(GEO_STATUS_INTERNAL_ERROR, "内存分配失败");
    for (int i = 0; i < count; i++)
        c[i] = symbolic_coord_create_rational((int64_t) ids[i], 1);
    AddNodeResult res = graph_add_point(graph, c, count);
    for (int i = 0; i < count; i++)
        symbolic_coord_destroy(c[i]);
    lv_free((void **)&(c));
    return geo_create_node_finish(res, graph);
}

static GeoResult geo_create_node_line_segment(ConstraintGraph *graph, const int *ids, int count) {
    if (!ids || count < 2)
        return geo_err(GEO_STATUS_INVALID_PARAM, "线段需2个端点");
    return geo_create_node_finish(graph_add_line_segment(graph, ids[0], ids[1]), graph);
}

static GeoResult geo_create_node_region(ConstraintGraph *graph, const int *ids, int count) {
    if (!ids || count < 1)
        return geo_err(GEO_STATUS_INVALID_PARAM, "区域需边界线段");
    return geo_create_node_finish(graph_add_region(graph, ids, count), graph);
}

static GeoResult geo_create_node_port(ConstraintGraph *graph, const int *ids, int count) {
    if (!ids || count < 3)
        return geo_err(GEO_STATUS_INVALID_PARAM, "端口需3个参数");
    return geo_create_node_finish(graph_add_port(graph, (PortType) ids[0], ids[1], ids[2]), graph);
}

static GeoResult geo_create_node_function_block(ConstraintGraph *graph, const int *ids, int count) {
    if (!ids || count < 1)
        return geo_err(GEO_STATUS_INVALID_PARAM, "函数块需至少1个内部节点");
    /* 与 geo_create_node_region 等一致的创建模式：委托图级 graph_add_function_block
     * 分配 GeoNode（graph_alloc_node(graph, GEOM_FUNCTION_BLOCK)）、设置类型并初始化
     * 函数块字段（内部节点数组由 ids 解析填充）；输入/输出端口数组以占位默认
     * （NULL/0）创建，后续可经 geo_pack 或 graph_add_* 补充端口信息。 */
    return geo_create_node_finish(graph_add_function_block(graph, ids, count, NULL, 0, NULL, 0), graph);
}

/** @brief 节点类型 → 创建处理函数 查找表 */
static GeoResult (*const s_node_handlers[])(ConstraintGraph *, const int *, int) = {
    [GEO_NODE_POINT] = geo_create_node_point,
    [GEO_NODE_LINE_SEGMENT] = geo_create_node_line_segment,
    [GEO_NODE_REGION] = geo_create_node_region,
    [GEO_NODE_PORT] = geo_create_node_port,
    [GEO_NODE_FUNCTION_BLOCK] = geo_create_node_function_block,
};

/* 原语 1: geo_create_node -- 创建几何节点（POINT/LINE/REGION/PORT） */
GeoResult geo_create_node(ConstraintGraph *graph, GeoNodeType type, const int *ids, int count) {
    CHECK_GRAPH(graph);
    if ((int) type >= 0 && (size_t) type < lv_ARRAY_SIZE(s_node_handlers) && s_node_handlers[(int) type]) {
        return s_node_handlers[(int) type](graph, ids, count);
    }
    return geo_err(GEO_STATUS_INVALID_TYPE, "未知节点类型");
}

/* ── 约束创建辅助函数：将 AddConstraintResult 包装为 GeoResult ── */
static GeoResult geo_create_constraint_finish(AddConstraintResult res) {
    if (res == ADD_CONSTRAINT_CONFLICT)
        return geo_err(GEO_STATUS_CONFLICT, "约束冲突");
    if (res == ADD_CONSTRAINT_DUPLICATE)
        return geo_err(GEO_STATUS_CONFLICT, "约束重复");
    return s_ok;
}

/* ── 各类型约束创建处理函数 ── */
static GeoResult geo_create_constraint_incidence(ConstraintGraph *graph, const int *p, int n) {
    (void)n;
    return geo_create_constraint_finish(graph_add_incidence(graph, p[0], p[1]));
}
static GeoResult geo_create_constraint_betweenness(ConstraintGraph *graph, const int *p, int n) {
    if (n < 3)
        return geo_err(GEO_STATUS_INVALID_PARAM, "之间需3个参与者");
    return geo_create_constraint_finish(graph_add_betweenness(graph, p[0], p[1], p[2]));
}
static GeoResult geo_create_constraint_intersection(ConstraintGraph *graph, const int *p, int n) {
    if (n < 3)
        return geo_err(GEO_STATUS_INVALID_PARAM, "相交需3个参与者");
    return geo_create_constraint_finish(graph_add_intersection(graph, p[0], p[1], p[2]));
}
static GeoResult geo_create_constraint_containment(ConstraintGraph *graph, const int *p, int n) {
    (void)n;
    return geo_create_constraint_finish(graph_add_containment(graph, p[0], p[1]));
}
static GeoResult geo_create_constraint_connection(ConstraintGraph *graph, const int *p, int n) {
    (void)n;
    return geo_create_constraint_finish(graph_add_connection(graph, p[0], p[1]));
}
static GeoResult geo_create_constraint_angle(ConstraintGraph *graph, const int *p, int n) {
    if (n < 3)
        return geo_err(GEO_STATUS_INVALID_PARAM, "角度需2条线段和角度值");
    return geo_create_constraint_finish(graph_add_angle(graph, p[0], p[1], (double)p[2]));
}

/** @brief 约束类型 → 创建处理函数 查找表 */
static GeoResult (*const s_constraint_handlers[])(ConstraintGraph *, const int *, int) = {
    [GEO_CONSTRAINT_INCIDENCE] = geo_create_constraint_incidence,
    [GEO_CONSTRAINT_BETWEENNESS] = geo_create_constraint_betweenness,
    [GEO_CONSTRAINT_INTERSECTION] = geo_create_constraint_intersection,
    [GEO_CONSTRAINT_CONTAINMENT] = geo_create_constraint_containment,
    [GEO_CONSTRAINT_CONNECTION] = geo_create_constraint_connection,
    [GEO_CONSTRAINT_ANGLE] = geo_create_constraint_angle,
};

/* 原语 2: geo_create_constraint -- 创建约束关系 */
GeoResult geo_create_constraint(ConstraintGraph *graph, GeoConstraintType type, const int *p, int n) {
    CHECK_GRAPH(graph);
    if (!p || n < 2)
        return geo_err(GEO_STATUS_INVALID_PARAM, "参与者不足");
    return LV_DISPATCH(s_constraint_handlers, type, geo_err(GEO_STATUS_INVALID_TYPE, "未知约束类型"), graph, p, n);
}

/* ── geo_solve 状态映射处理函数 ── */
static GeoResult handle_solve_ok(void) { return s_ok; }
static GeoResult handle_solve_conflict(void) { return geo_err(GEO_STATUS_CONFLICT, "求解冲突"); }
static GeoResult handle_solve_timeout(void) { return geo_err(GEO_STATUS_TIMEOUT, "求解超时"); }

static GeoResult (*kSolveHandlers[])(void) = {
    [ENGINE_SOLVE_OK]       = handle_solve_ok,
    [ENGINE_SOLVE_CONFLICT] = handle_solve_conflict,
    [ENGINE_SOLVE_TIMEOUT]  = handle_solve_timeout,
};

/* 原语 3: geo_solve -- 求解约束系统 */
GeoResult geo_solve(lvEngine *engine) {
    CHECK_ENGINE(engine);
    EngineSolveResult esr = engine_solve(engine);
    return LV_DISPATCH(kSolveHandlers, esr, geo_err(GEO_STATUS_INTERNAL_ERROR, "求解错误"));
}

/* 原语 4: geo_normalize -- 约束图归一化 */
GeoResult geo_normalize(ConstraintGraph *graph, bool scope_aware) {
    CHECK_GRAPH(graph);
    NormalizationResult *nr = graph_normalize(graph, scope_aware);
    if (!nr)
        return geo_err(GEO_STATUS_INTERNAL_ERROR, "归一化失败");

    int *merged = geo_dup_int(nr->merged_count);
    normalization_result_destroy(nr);
    return merged ? geo_ok(merged) : geo_err(GEO_STATUS_INTERNAL_ERROR, "内存分配失败");
}

/* ── geo_rewrite 状态映射处理函数 ── */
static GeoResult handle_rewrite_ok(void) { return geo_ok(geo_dup_int(REWRITE_STATUS_OK)); }
static GeoResult handle_rewrite_no_match(void) { return geo_ok(geo_dup_int(REWRITE_STATUS_NO_MATCH)); }
static GeoResult handle_rewrite_terminated(void) { return geo_ok(geo_dup_int(REWRITE_STATUS_TERMINATED)); }
static GeoResult handle_rewrite_confluence(void) { return geo_err(GEO_STATUS_CONFLICT, "汇流性问题"); }

static GeoResult (*kRewriteHandlers[])(void) = {
    [REWRITE_STATUS_OK]               = handle_rewrite_ok,
    [REWRITE_STATUS_NO_MATCH]         = handle_rewrite_no_match,
    [REWRITE_STATUS_TERMINATED]       = handle_rewrite_terminated,
    [REWRITE_STATUS_CONFLUENCE_ISSUE] = handle_rewrite_confluence,
};

/* 原语 5: geo_rewrite -- 应用重写规则 */
GeoResult geo_rewrite(ConstraintGraph *graph, void **rules, int rule_count, int step_limit) {
    CHECK_GRAPH(graph);
    if (!rules || rule_count < 1)
        return geo_err(GEO_STATUS_INVALID_PARAM, "规则数组为空");
    if (step_limit <= 0)
        step_limit = 1000;

    RewriteStatus s = rewrite_with_rules(graph, (RewriteRule **) rules, rule_count, step_limit, true);
    return LV_DISPATCH(kRewriteHandlers, s, geo_err(GEO_STATUS_INTERNAL_ERROR, "重写错误"));
}

/* ── geo_unify 状态映射处理函数 ── */
static GeoResult handle_unify_ok(void) { return s_ok; }
static GeoResult handle_unify_port_type_mismatch(void) { return geo_err(GEO_STATUS_CONFLICT, "端口类型不匹配"); }
static GeoResult handle_unify_constraint_mismatch(void) { return geo_err(GEO_STATUS_CONFLICT, "约束不匹配"); }
static GeoResult handle_unify_coord_mismatch(void) { return geo_err(GEO_STATUS_CONFLICT, "坐标不匹配"); }
static GeoResult handle_unify_structure_mismatch(void) { return geo_err(GEO_STATUS_CONFLICT, "结构不匹配"); }
static GeoResult handle_unify_scope_mismatch(void) { return geo_err(GEO_STATUS_CONFLICT, "作用域不匹配"); }

static GeoResult (*kUnifyHandlers[])(void) = {
    [UNIFY_STATUS_OK]                  = handle_unify_ok,
    [UNIFY_STATUS_PORT_TYPE_MISMATCH]  = handle_unify_port_type_mismatch,
    [UNIFY_STATUS_CONSTRAINT_MISMATCH] = handle_unify_constraint_mismatch,
    [UNIFY_STATUS_COORD_MISMATCH]      = handle_unify_coord_mismatch,
    [UNIFY_STATUS_STRUCTURE_MISMATCH]  = handle_unify_structure_mismatch,
    [UNIFY_STATUS_SCOPE_MISMATCH]      = handle_unify_scope_mismatch,
};

/* 原语 6: geo_unify -- 统一构造与命题 */
GeoResult geo_unify(const ConstraintGraph *construction, const ConstraintGraph *proposition) {
    if (!construction)
        return geo_err(GEO_STATUS_NULL_ARG, "构造图 NULL");
    if (!proposition)
        return geo_err(GEO_STATUS_NULL_ARG, "命题图 NULL");

    UnifyStatus s = unify_construction_with_proposition(construction, proposition);
    return LV_DISPATCH(kUnifyHandlers, s, geo_err(GEO_STATUS_INTERNAL_ERROR, "合一失败"));
}

/* ── geo_pack 状态映射处理函数 ── */
static GeoResult handle_pack_ok(FuncBlock *fb) {
    int *id = geo_dup_int(func_block_get_id(fb));
    func_block_destroy(fb);
    return id ? geo_ok(id) : geo_err(GEO_STATUS_INTERNAL_ERROR, "内存分配失败");
}
static GeoResult handle_pack_cross_boundary(void) { return geo_err(GEO_STATUS_CONFLICT, "跨边界约束冲突"); }
static GeoResult handle_pack_invalid_nodes(void) { return geo_err(GEO_STATUS_INVALID_PARAM, "无效内部节点"); }
static GeoResult handle_pack_invalid_ports(void) { return geo_err(GEO_STATUS_INVALID_PARAM, "无效端口"); }

static GeoResult (*kPackHandlers[])(void) = {
    [PACK_RESULT_CROSS_BOUNDARY_CONFLICT] = handle_pack_cross_boundary,
    [PACK_RESULT_INVALID_NODES]           = handle_pack_invalid_nodes,
    [PACK_RESULT_INVALID_PORTS]           = handle_pack_invalid_ports,
};

/* 原语 7: geo_pack -- 打包为函数块 */
GeoResult geo_pack(ConstraintGraph *graph, const int *internal_ids, int internal_count, const int *in_ports,
                   int in_count, const int *out_ports, int out_count) {
    CHECK_GRAPH(graph);
    if (!internal_ids || internal_count < 1)
        return geo_err(GEO_STATUS_INVALID_PARAM, "内部节点为空");

    FuncBlock *fb = NULL;
    PackResult r =
        func_block_pack(graph, internal_ids, internal_count, in_ports, in_count, out_ports, out_count, NULL, 0, &fb);
    if (r == PACK_RESULT_OK)
        return handle_pack_ok(fb);
    return LV_DISPATCH(kPackHandlers, r, geo_err(GEO_STATUS_INTERNAL_ERROR, "打包错误"));
}

/* 原语 8: geo_instantiate -- 实例化函数块 */
GeoResult geo_instantiate(ConstraintGraph *graph, lvEngine *engine, int func_block_id, const int *args, int arg_count) {
    CHECK_GRAPH(graph);
    CHECK_ENGINE(engine);
    if (!args || arg_count < 1)
        return geo_err(GEO_STATUS_INVALID_PARAM, "参数映射为空");

    /* 底层 API 要求非 const，需拷贝 */
    int *mutable_args = (int *) lv_malloc((size_t) arg_count * sizeof(int));
    if (!mutable_args)
        return geo_err(GEO_STATUS_INTERNAL_ERROR, "内存分配失败");
    memcpy(mutable_args, args, (size_t) arg_count * sizeof(int));

    int result_count = 0;
    int *nodes = engine_instantiate_function(engine, func_block_id, mutable_args, arg_count, &result_count);
    lv_free((void **)&(mutable_args));
    if (!nodes)
        return geo_err(GEO_STATUS_NO_SOLUTION, "实例化失败");

    /* 打包为 [count, id0, id1, ...] */
    int *out = (int *) lv_malloc((size_t) (result_count + 1) * sizeof(int));
    if (!out) {
        lv_free((void **)&(nodes));
        return geo_err(GEO_STATUS_INTERNAL_ERROR, "内存分配失败");
    }
    out[0] = result_count;
    memcpy(&out[1], nodes, (size_t) result_count * sizeof(int));
    lv_free((void **)&(nodes));
    return geo_ok(out);
}

/* 原语 9: geo_prove -- 执行证明搜索 */
GeoResult geo_prove(ProofNavigator *nav, int strategy, int max_steps) {
    if (!nav)
        return geo_err(GEO_STATUS_NULL_ARG, "导航器 NULL");
    if (max_steps <= 0)
        max_steps = 1000;

    return proof_search_with_strategy(nav, (ProofStrategyType) strategy, max_steps)
               ? s_ok
               : geo_err(GEO_STATUS_NO_SOLUTION, "证明搜索失败");
}

/* ---------- 导出格式名 -> 导出函数 查找表（替代 strcmp 分支链） ---------- */

/** @brief 导出函数签名：成功返回 true，失败返回 false */
typedef bool (*ProofExportFn)(ProofNavigator *nav, const char *filepath);

static const struct {
    const char *name;
    ProofExportFn fn;
} kProofExportFns[] = {
    {"html", proof_export_html},
    {"latex", proof_export_latex},
    {"coq", proof_export_coq},
};

/* 原语 10: geo_export -- 导出结果 (html/latex/coq) */
GeoResult geo_export(ProofNavigator *nav, const char *format, const char *path) {
    if (!nav)
        return geo_err(GEO_STATUS_NULL_ARG, "导航器 NULL");
    if (!format)
        return geo_err(GEO_STATUS_NULL_ARG, "格式 NULL");
    if (!path)
        return geo_err(GEO_STATUS_NULL_ARG, "路径 NULL");

    /* 按导出格式名查表分发（替代 strcmp 分支链） */
    bool ok = false;
    size_t i;
    for (i = 0; i < lv_ARRAY_SIZE(kProofExportFns); i++) {
        if (lv_str_eq(format, kProofExportFns[i].name)) {
            ok = kProofExportFns[i].fn(nav, path);
            break;
        }
    }
    if (i >= lv_ARRAY_SIZE(kProofExportFns))
        return geo_err(GEO_STATUS_UNSUPPORTED, "不支持的导出格式");

    return ok ? s_ok : geo_err(GEO_STATUS_IO_ERROR, "导出写入失败");
}

/* 原语 11: geo_serialize -- 序列化约束图为 JSON */
GeoResult geo_serialize(const ConstraintGraph *graph) {
    if (!graph)
        return geo_err(GEO_STATUS_NULL_ARG, "约束图 NULL");

    char *json = graph_serialize_to_json(graph);
    if (!json)
        return geo_err(GEO_STATUS_INTERNAL_ERROR, graph_get_serialize_error(graph));
    return geo_ok(json);
}

/* 原语 12: geo_deserialize -- 从 JSON 反序列化约束图 */
GeoResult geo_deserialize(const char *json) {
    if (!json)
        return geo_err(GEO_STATUS_NULL_ARG, "JSON NULL");

    ConstraintGraph *g = graph_deserialize_from_json(json);
    if (!g)
        return geo_err(GEO_STATUS_INVALID_PARAM, "JSON 解析失败");
    return geo_ok(g);
}

/* 原语 13: geo_query -- 查询约束图 (node/constraint/count) */
GeoResult geo_query(const ConstraintGraph *graph, const char *query, int target_id) {
    if (!graph)
        return geo_err(GEO_STATUS_NULL_ARG, "约束图 NULL");
    if (!query)
        return geo_err(GEO_STATUS_NULL_ARG, "查询类型 NULL");

    if (lv_str_eq(query, "node")) {
        GeomNode *n = graph_get_node(graph, target_id);
        if (!n)
            return geo_err(GEO_STATUS_NOT_FOUND, "节点未找到");
        GeoResult r = {GEO_STATUS_OK, (void *) n, NULL};
        return r;
    }
    if (lv_str_eq(query, "constraint")) {
        Constraint *c = graph_get_constraint(graph, target_id);
        if (!c)
            return geo_err(GEO_STATUS_NOT_FOUND, "约束未找到");
        GeoResult r = {GEO_STATUS_OK, (void *) c, NULL};
        return r;
    }
    if (lv_str_eq(query, "count")) {
        int *cnt = (int *) lv_malloc(2 * sizeof(int));
        if (!cnt)
            return geo_err(GEO_STATUS_INTERNAL_ERROR, "内存分配失败");
        cnt[0] = graph_get_node_count(graph);
        cnt[1] = graph_get_constraint_count(graph);
        return geo_ok(cnt);
    }
    return geo_err(GEO_STATUS_INVALID_PARAM, "未知查询类型");
}
