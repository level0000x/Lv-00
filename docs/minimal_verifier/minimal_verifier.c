/**
 * @file minimal_verifier.c
 * @brief Lv-00 最小验证器 —— 受 mm0/Metamath 启发
 *
 * 设计哲学（与 mm0 的 verifier 一致）：
 *   - 只做"替换检查"：验证器不内建任何逻辑或推理规则
 *   - 所有几何/代数知识以声明形式存在于约束图和公理包中
 *   - 极简即强壮：代码越少，出错越少
 *   - 独立验证器：仅依赖 C 标准库
 *
 * 验证范围（三遍扫描）：
 *   1. 节点声明-引用一致性 —— 约束/函数块中每个引用的节点 ID 必须已声明
 *   2. 约束类型参数数量检查 —— participant_count 必须匹配类型预期
 *   3. 公理包声明-使用一致性 —— 模板名非空无重复、参数描述数量匹配
 *
 * 显式不验证（留给上层）：
 *   - 几何正确性 / 约束冲突 / 类型推演 / 信任颜色变换
 *
 * 与 Lv-00 接口兼容：
 *   - 可以直接 #include "constraint_graph.h" 和 "axiom_pkg.h"，链接主引擎
 *   - 也可独立编译：本文件内联了必要的类型定义，二进制兼容
 *
 * 编译（独立模式）：
 *   gcc -std=c99 -Wall minimal_verifier.c -o minimal_verifier
 *   ./minimal_verifier --self-test
 *
 * 编译（与 Lv-00 链接）：
 *   gcc -std=c99 -I../../include minimal_verifier.c ../../src/core/constraint_graph.c \
 *       ../../src/axiom/axiom_pkg.c -o minimal_verifier -lgmp -lm
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * 内联类型定义（与 constraint_graph.h / axiom_pkg.h 二进制兼容）
 *
 * 若链接 Lv-00 主引擎，可删除此段，直接 #include 对应头文件。
 * 保留此段时，需确保字段顺序/类型与上游头文件完全一致。
 * ================================================================ */

typedef enum { TRUST_GREEN, TRUST_BLUE, TRUST_YELLOW, TRUST_ORANGE,
               TRUST_LIGHT_ORANGE, TRUST_RED, TRUST_AMBER } TrustColor;
typedef enum { LIGHT_ORANGE_ORACLE, LIGHT_ORANGE_EXPLOSION } LightOrangeSubtype;
typedef struct SymbolicCoord SymbolicCoord;
typedef struct TypeRegion TypeRegion;
typedef enum { GEOM_POINT, GEOM_LINE_SEGMENT, GEOM_REGION, GEOM_PORT,
               GEOM_FUNCTION_BLOCK } GeomType;
typedef enum { PORT_INPUT, PORT_OUTPUT } PortType;
typedef enum { INCIDENCE=0, BETWEENNESS=1, INTERSECTION=2,
               CONTAINMENT=3, CONNECTION=4 } ConstraintType;
typedef struct GeomNode GeomNode;
typedef struct Constraint Constraint;
typedef struct ConstraintGraph ConstraintGraph;

typedef struct Port {
    int id; PortType type; int namespace_depth; int parent_block_id;
    bool is_formal_param; bool is_polymorphic;
    TypeRegion *type_region; GeomNode *connected_to;
} Port;

struct GeomNode {
    int id; GeomType type;
    SymbolicCoord **symbolic_coords; int coord_count;
    TrustColor trust; LightOrangeSubtype lo_subtype;
    char *numeric_assumption_declaration; double numeric_precision;
    int namespace_depth; int parent_block_id;
    union {
        Port *port;
        struct { GeomNode **boundary_segments; int segment_count; } region;
        struct {
            GeomNode **internal_nodes;
            int *input_port_ids, *output_port_ids;
            int internal_node_count, input_count, output_count;
            int determinism_state;
        } func_block;
    } data;
};

struct Constraint {
    int id; ConstraintType type;
    int *participants; int participant_count; int template_id;
};

struct ConstraintGraph {
    GeomNode **nodes; int node_count, node_capacity;
    Constraint **constraints; int constraint_count, constraint_capacity;
    int next_node_id, next_constraint_id;
    GeomNode **node_index; int node_index_capacity;
    Constraint **constraint_index; int constraint_index_capacity;
};

/* axiom_pkg.h 兼容类型 */
typedef enum { PARAM_POINT, PARAM_LINE_SEGMENT, PARAM_REGION,
               PARAM_SCALAR } TemplateParamType;
typedef struct { TemplateParamType type; char name[64]; } TemplateParam;
typedef struct {
    int expected_node_types[8], expected_constraint_types[8];
    int node_type_count, constraint_type_count;
} NormalFormDesc;
typedef struct ConstraintTemplate {
    char *name; int param_count;
    void (*expand)(SymbolicCoord **, ConstraintGraph *);
    bool verified; TemplateParam *params; int param_desc_count;
    NormalFormDesc normal_form;
} ConstraintTemplate;
typedef struct DependencyRef {
    char ref_id[64], content_hash[65];
    int dependent_node_id, original_color;
} DependencyRef;
typedef struct AxiomPackage {
    char *name, *version;
    ConstraintTemplate *templates; int template_count;
    void *known_unconstructibles; int unconstructible_count;
    char *bottom_geometry, *negation_encoding;
    int contradiction_behavior;
    void *expansion_cache; int expansion_cache_count,
        expansion_cache_capacity, max_expansion_depth;
    DependencyRef *dep_refs; int dep_ref_count, dep_ref_capacity;
} AxiomPackage;

/* ================================================================
 * 约束类型参数数量表（验证器唯一的"内建知识"）
 * ================================================================ */

#define CT_COUNT 5
static const char *g_ct_names[CT_COUNT] =
    {"INCIDENCE","BETWEENNESS","INTERSECTION","CONTAINMENT","CONNECTION"};
static const int g_ct_arity[CT_COUNT] = {2,3,3,2,2};

/* ================================================================
 * 验证报告
 * ================================================================ */

#define MV_MAX_ENTRIES 128
#define MV_MSG_LEN 256

typedef struct {
    int err_c, warn_c;
    char errs[MV_MAX_ENTRIES][MV_MSG_LEN];
    char warns[MV_MAX_ENTRIES][MV_MSG_LEN];
    bool passed;
} MvReport;

static void mv_rpt_init(MvReport *r) {
    r->err_c = r->warn_c = 0; r->passed = true;
}
static void mv_err(MvReport *r, const char *fmt, ...) {
    if (r->err_c >= MV_MAX_ENTRIES) return;
    va_list ap; va_start(ap, fmt);
    vsnprintf(r->errs[r->err_c], MV_MSG_LEN, fmt, ap);
    va_end(ap); r->err_c++; r->passed = false;
}
static void mv_warn(MvReport *r, const char *fmt, ...) {
    if (r->warn_c >= MV_MAX_ENTRIES) return;
    va_list ap; va_start(ap, fmt);
    vsnprintf(r->warns[r->warn_c], MV_MSG_LEN, fmt, ap);
    va_end(ap); r->warn_c++;
}

/* ================================================================
 * 辅助：节点存在性检查（O(n) 线性扫描，mm0 风格）
 * ================================================================ */

static bool mv_node_exists(const ConstraintGraph *g, int nid) {
    if (!g || nid < 0) return false;
    for (int i = 0; i < g->node_count; i++)
        if (g->nodes[i] && g->nodes[i]->id == nid) return true;
    return false;
}

/* ================================================================
 * 第一遍：节点声明-引用一致性
 * ================================================================ */

static void mv_pass1_node_refs(const ConstraintGraph *g, MvReport *r) {
    if (!g) { mv_err(r, "node-refs: NULL graph"); return; }

    /* 约束参与者引用 */
    for (int i = 0; i < g->constraint_count; i++) {
        const Constraint *c = g->constraints[i];
        if (!c) continue;
        for (int j = 0; j < c->participant_count; j++) {
            int pid = c->participants[j];
            if (!mv_node_exists(g, pid))
                mv_err(r, "node-refs: constraint #%d (%s) refs undeclared "
                       "node %d [participant[%d]]",
                       c->id, c->type<CT_COUNT?g_ct_names[c->type]:"?",
                       pid, j);
        }
    }

    /* 函数块内部引用（internal_nodes / input_ports / output_ports） */
    for (int i = 0; i < g->node_count; i++) {
        const GeomNode *n = g->nodes[i];
        if (!n || n->type != GEOM_FUNCTION_BLOCK) continue;
        int ic = n->data.func_block.internal_node_count;
        int *ids = n->data.func_block.internal_node_ids;
        for (int j = 0; j < ic; j++)
            if (ids && ids[j] >= 0 && !mv_node_exists(g, ids[j]))
                mv_err(r, "node-refs: fb #%d refs undeclared internal node %d",
                       n->id, ids[j]);
        ic = n->data.func_block.input_count;
        ids = n->data.func_block.input_port_ids;
        for (int j = 0; j < ic; j++)
            if (ids && ids[j] >= 0 && !mv_node_exists(g, ids[j]))
                mv_err(r, "node-refs: fb #%d refs undeclared input port %d",
                       n->id, ids[j]);
        ic = n->data.func_block.output_count;
        ids = n->data.func_block.output_port_ids;
        for (int j = 0; j < ic; j++)
            if (ids && ids[j] >= 0 && !mv_node_exists(g, ids[j]))
                mv_err(r, "node-refs: fb #%d refs undeclared output port %d",
                       n->id, ids[j]);
    }
}

/* ================================================================
 * 第二遍：约束类型参数数量检查
 * ================================================================ */

static void mv_pass2_arity(const ConstraintGraph *g, MvReport *r) {
    if (!g) { mv_err(r, "arity: NULL graph"); return; }
    for (int i = 0; i < g->constraint_count; i++) {
        const Constraint *c = g->constraints[i];
        if (!c) continue;
        if (c->type < 0 || c->type >= CT_COUNT) {
            mv_err(r, "arity: constraint #%d unknown type %d", c->id, c->type);
            continue;
        }
        int exp = g_ct_arity[c->type];
        if (c->participant_count != exp)
            mv_err(r, "arity: constraint #%d (%s) expects %d participants, "
                   "got %d", c->id, g_ct_names[c->type], exp,
                   c->participant_count);
    }
}

/* ================================================================
 * 第三遍：公理包声明-使用一致性
 * ================================================================ */

static void mv_pass3_axiom(const AxiomPackage *pkg,
                            const ConstraintGraph *g, MvReport *r) {
    if (!pkg) { mv_warn(r, "axiom: no package, skipping"); return; }

    /* 模板名非空 + 参数数量一致 + 无重复 */
    if (pkg->templates && pkg->template_count > 0) {
        for (int i = 0; i < pkg->template_count; i++) {
            const ConstraintTemplate *t = &pkg->templates[i];
            if (!t->name || !strlen(t->name))
                mv_err(r, "axiom: template[%d] empty name", i);
            if (t->param_desc_count > 0 && t->param_desc_count != t->param_count)
                mv_err(r, "axiom: tmpl '%s' param_count=%d desc_count=%d",
                       t->name ? t->name : "?", t->param_count,
                       t->param_desc_count);
            for (int j = i + 1; j < pkg->template_count; j++)
                if (pkg->templates[j].name && t->name &&
                    !strcmp(t->name, pkg->templates[j].name))
                    mv_err(r, "axiom: duplicate template '%s' [%d,%d]",
                           t->name, i, j);
        }
    }

    /* 依赖引用节点存在性（仅作警告，可能引用外部包） */
    if (g && pkg->dep_refs && pkg->dep_ref_count > 0) {
        for (int i = 0; i < pkg->dep_ref_count; i++) {
            const DependencyRef *dr = &pkg->dep_refs[i];
            if (dr->dependent_node_id >= 0 &&
                !mv_node_exists(g, dr->dependent_node_id))
                mv_warn(r, "axiom: dep '%s' refs node %d (may be external)",
                        dr->ref_id, dr->dependent_node_id);
        }
    }
}

/* ================================================================
 * 综合验证入口
 * ================================================================ */

/**
 * @brief 对约束图执行最小验证（mm0 风格，三遍扫描）
 * @param graph 约束图（NULL 则仅报告错误）
 * @param pkg   公理包（NULL 则跳过公理包检查）
 * @param rpt   输出报告（调用者分配，不可为 NULL）
 */
void minimal_verifier_validate(const ConstraintGraph *graph,
                                const AxiomPackage *pkg,
                                MvReport *rpt) {
    mv_rpt_init(rpt);
    if (!graph) { mv_err(rpt, "validate: NULL graph"); return; }
    if (graph->node_count == 0 && graph->constraint_count == 0)
        mv_warn(rpt, "validate: empty graph (0 nodes, 0 constraints)");
    mv_pass1_node_refs(graph, rpt);
    mv_pass2_arity(graph, rpt);
    mv_pass3_axiom(pkg, graph, rpt);
}

/* ================================================================
 * 报告输出
 * ================================================================ */

void minimal_verifier_report_print(const MvReport *rpt) {
    if (!rpt) { printf("[mv] NULL report\n"); return; }
    printf("===== Minimal Verifier Report =====\n");
    printf("Status: %s | Errors: %d Warnings: %d\n",
           rpt->passed ? "PASSED" : "FAILED", rpt->err_c, rpt->warn_c);
    for (int i = 0; i < rpt->err_c; i++)
        printf("  [ERR] %s\n", rpt->errs[i]);
    for (int i = 0; i < rpt->warn_c; i++)
        printf("  [WARN] %s\n", rpt->warns[i]);
    printf("===================================\n");
}

/* ================================================================
 * 自测（不依赖 Lv-00 主引擎，手工构造测试数据结构）
 * ================================================================ */

static bool mv_self_test(void) {
    printf("[self-test] running...\n");
    bool ok = true;
    int passed = 0, failed = 0;

    /* T1: NULL graph */
    { MvReport r; minimal_verifier_validate(NULL, NULL, &r);
      if (r.passed) { printf("  FAIL T1\n"); ok=false; failed++; }
      else { printf("  OK   T1 (NULL)\n"); passed++; } }

    /* T2: empty graph */
    { ConstraintGraph g; memset(&g,0,sizeof(g));
      MvReport r; minimal_verifier_validate(&g, NULL, &r);
      if (!r.passed) { printf("  FAIL T2\n"); ok=false; failed++; }
      else { printf("  OK   T2 (empty)\n"); passed++; } }

    /* T3: undeclared node reference */
    { GeomNode n; memset(&n,0,sizeof(n)); n.id=1; n.type=GEOM_POINT;
      int p=999; Constraint c; memset(&c,0,sizeof(c));
      c.id=10; c.type=INCIDENCE; c.participants=&p; c.participant_count=2;
      GeomNode *ns[]={&n}; Constraint *cs[]={&c};
      ConstraintGraph g; memset(&g,0,sizeof(g));
      g.nodes=ns; g.node_count=1; g.constraints=cs; g.constraint_count=1;
      MvReport r; minimal_verifier_validate(&g, NULL, &r);
      if (r.passed) { printf("  FAIL T3\n"); ok=false; failed++; }
      else { printf("  OK   T3 (undeclared) [%d errs]\n",r.err_c); passed++; } }

    /* T4: wrong arity */
    { GeomNode n1,n2; memset(&n1,0,sizeof(n1)); n1.id=1; n1.type=GEOM_POINT;
      memset(&n2,0,sizeof(n2)); n2.id=2; n2.type=GEOM_LINE_SEGMENT;
      int ps[]={1,2,3,4,5}; Constraint c; memset(&c,0,sizeof(c));
      c.id=20; c.type=BETWEENNESS; c.participants=ps; c.participant_count=5;
      GeomNode *ns[]={&n1,&n2}; Constraint *cs[]={&c};
      ConstraintGraph g; memset(&g,0,sizeof(g));
      g.nodes=ns; g.node_count=2; g.constraints=cs; g.constraint_count=1;
      MvReport r; minimal_verifier_validate(&g, NULL, &r);
      if (r.passed) { printf("  FAIL T4\n"); ok=false; failed++; }
      else { printf("  OK   T4 (arity) [%d errs]\n",r.err_c); passed++; } }

    /* T5: correct graph */
    { GeomNode n1,n2,n3; memset(&n1,0,sizeof(n1)); n1.id=1; n1.type=GEOM_POINT;
      memset(&n2,0,sizeof(n2)); n2.id=2; n2.type=GEOM_POINT;
      memset(&n3,0,sizeof(n3)); n3.id=3; n3.type=GEOM_LINE_SEGMENT;
      int ps[]={1,3}; Constraint c; memset(&c,0,sizeof(c));
      c.id=100; c.type=INCIDENCE; c.participants=ps; c.participant_count=2;
      GeomNode *ns[]={&n1,&n2,&n3}; Constraint *cs[]={&c};
      ConstraintGraph g; memset(&g,0,sizeof(g));
      g.nodes=ns; g.node_count=3; g.constraints=cs; g.constraint_count=1;
      MvReport r; minimal_verifier_validate(&g, NULL, &r);
      if (!r.passed) { printf("  FAIL T5 (%d errs)\n",r.err_c); ok=false; failed++; }
      else { printf("  OK   T5 (correct)\n"); passed++; } }

    /* T6: duplicate template */
    { ConstraintTemplate ts[2]; memset(ts,0,sizeof(ts));
      ts[0].name="dup"; ts[0].param_count=2; ts[0].param_desc_count=2;
      ts[1].name="dup"; ts[1].param_count=1; ts[1].param_desc_count=1;
      AxiomPackage p; memset(&p,0,sizeof(p));
      p.templates=ts; p.template_count=2;
      MvReport r; minimal_verifier_validate(NULL, &p, &r);
      if (r.passed) { printf("  FAIL T6\n"); ok=false; failed++; }
      else { printf("  OK   T6 (dup tmpl) [%d errs]\n",r.err_c); passed++; } }

    printf("[self-test] %d passed, %d failed => %s\n",
           passed, failed, ok?"ALL PASSED":"SOME FAILED");
    return ok;
}

/* ================================================================
 * main()
 * ================================================================ */

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--self-test"))
        return mv_self_test() ? 0 : 1;

    printf("Lv-00 Minimal Verifier (mm0-inspired)\n\n");
    printf("Validates structural consistency:\n");
    printf("  1. Node declaration-reference consistency\n");
    printf("  2. Constraint type arity (participant count)\n");
    printf("  3. Axiom package declaration-use consistency\n\n");
    printf("No logic or inference rules built-in.\n");
    printf("Usage: %s [--self-test]\n", argv[0]);
    return 0;
}
