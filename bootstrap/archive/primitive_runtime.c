/*
 * primitive_runtime.c -- 13原语IR运行时，全GMP mpq_t精确有理数实现
 * 绝无 double/float；所有几何计算以精确有理数完成。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gmp.h>
#include "primitive_runtime.h"

/* ================================================================
 * 生命周期：初始化 / 销毁图中全部 mpq_t 成员
 * ================================================================ */
void kg_init_graph(KGraph *g) {
    int i;
    g->node_count = g->constraint_count = 0;
    g->name[0] = '\0';
    for (i = 0; i < KN; i++) {
        g->nodes[i].id = -1; g->nodes[i].type = 0; g->nodes[i].name[0] = '\0';
        mpq_init(g->nodes[i].x);   /* GMP: 置 0/1 */
        mpq_init(g->nodes[i].y);
        mpq_init(g->nodes[i].z);
    }
    for (i = 0; i < KC; i++) {
        g->constraints[i].id = -1; g->constraints[i].type = 0;
        g->constraints[i].node_a = g->constraints[i].node_b = -1;
        g->constraints[i].relation = 0;
        mpq_init(g->constraints[i].value);  /* GMP: 置 0/1 */
    }
}

void kg_clear_graph(KGraph *g) {
    int i;
    for (i = 0; i < KN; i++) { mpq_clear(g->nodes[i].x); mpq_clear(g->nodes[i].y); mpq_clear(g->nodes[i].z); }
    for (i = 0; i < KC; i++) { mpq_clear(g->constraints[i].value); }
}

/* ================================================================
 * OP 0: kg_create_node -- 用 mpq_set_str 解析字符串为精确有理坐标
 * ================================================================ */
int kg_create_node(KGraph *g, int type, const char *name,
                   const char *xs, const char *ys, const char *zs) {
    int i;
    if (g->node_count >= KN) return -1;
    i = g->node_count;
    g->nodes[i].id = i; g->nodes[i].type = type;
    if (name) { strncpy(g->nodes[i].name, name, 31); g->nodes[i].name[31] = '\0'; }
    if (mpq_set_str(g->nodes[i].x, xs, 10) != 0) return -2;  /* GMP 解析有理字符串 */
    if (mpq_set_str(g->nodes[i].y, ys, 10) != 0) return -3;
    if (mpq_set_str(g->nodes[i].z, zs, 10) != 0) return -4;
    g->node_count++;
    return i;
}

/* ================================================================
 * OP 1: kg_create_constraint -- 用 mpq_set_str 解析精确有理约束值
 * ================================================================ */
int kg_create_constraint(KGraph *g, int type, int na, int nb,
                         const char *vs, int rel) {
    int i;
    if (g->constraint_count >= KC) return -1;
    if (na < 0 || na >= g->node_count) return -2;
    if (nb < 0 || nb >= g->node_count) return -3;
    i = g->constraint_count;
    g->constraints[i].id = i; g->constraints[i].type = type;
    g->constraints[i].node_a = na; g->constraints[i].node_b = nb;
    g->constraints[i].relation = rel;
    if (mpq_set_str(g->constraints[i].value, vs, 10) != 0) return -4;  /* GMP 解析 */
    g->constraint_count++;
    return i;
}

/* ================================================================
 * OP 2: kg_solve -- 全 GMP 精确验证 |dx^2+dy^2+dz^2 - value| < eps
 * ================================================================ */
int kg_solve(KGraph *g, const char *eps_str) {
    int i, v = 0;
    mpq_t eps, dx, dy, dz, ds, ds2, ds3, distSq, diff, ad;
    mpq_init(eps); mpq_set_str(eps, eps_str, 10);  /* GMP 解析容差 */
    mpq_init(dx); mpq_init(dy); mpq_init(dz);
    mpq_init(ds); mpq_init(ds2); mpq_init(ds3);
    mpq_init(distSq); mpq_init(diff); mpq_init(ad);

    for (i = 0; i < g->constraint_count; i++) {
        KConstraint *c = &g->constraints[i];
        KNode *A = &g->nodes[c->node_a], *B = &g->nodes[c->node_b];

        /* Step 1: 坐标差 = A - B   [mpq_sub 精确] */
        mpq_sub(dx, A->x, B->x);        /* dx = A.x - B.x */
        mpq_sub(dy, A->y, B->y);        /* dy = A.y - B.y */
        mpq_sub(dz, A->z, B->z);        /* dz = A.z - B.z */

        /* Step 2: 平方 = 差 * 差     [mpq_mul 精确] */
        mpq_mul(ds, dx, dx);            /* ds  = dx^2 */
        mpq_mul(ds2, dy, dy);           /* ds2 = dy^2 */
        mpq_mul(ds3, dz, dz);           /* ds3 = dz^2 */

        /* Step 3: 距离平方 = 累加   [mpq_add 精确] */
        mpq_set(distSq, ds);            /* distSq = dx^2 */
        mpq_add(distSq, distSq, ds2);   /* distSq = dx^2 + dy^2 */
        mpq_add(distSq, distSq, ds3);   /* distSq = dx^2 + dy^2 + dz^2 */

        /* Step 4: 差值 = 计算值 - 期望值  [mpq_sub 精确] */
        mpq_sub(diff, distSq, c->value); /* diff = distSq - value */

        /* Step 5: 绝对值 (GMP 无 mpq_abs，手动计算) */
        mpq_set(ad, diff);              /* ad = diff */
        if (mpq_sgn(diff) < 0) mpq_neg(ad, ad);  /* ad = -diff if diff < 0 */

        /* Step 6: 容差判断 [mpq_cmp 精确比较] */
        if (mpq_cmp(ad, eps) >= 0) v++;
    }
    mpq_clear(eps); mpq_clear(dx); mpq_clear(dy); mpq_clear(dz);
    mpq_clear(ds); mpq_clear(ds2); mpq_clear(ds3);
    mpq_clear(distSq); mpq_clear(diff); mpq_clear(ad);
    return v;
}

/* ================================================================
 * OP 3: kg_normalize -- GMP 比较+减法，平移坐标至非负象限
 * ================================================================ */
int kg_normalize(KGraph *g) {
    int i;
    mpq_t mx, my, mz;
    if (g->node_count <= 0) return 0;
    mpq_init(mx); mpq_init(my); mpq_init(mz);
    /* 初始化为首节点坐标 [mpq_set 精确] */
    mpq_set(mx, g->nodes[0].x); mpq_set(my, g->nodes[0].y); mpq_set(mz, g->nodes[0].z);
    /* 遍历找最小 [mpq_cmp 精确比较] */
    for (i = 1; i < g->node_count; i++) {
        if (mpq_cmp(g->nodes[i].x, mx) < 0) mpq_set(mx, g->nodes[i].x);
        if (mpq_cmp(g->nodes[i].y, my) < 0) mpq_set(my, g->nodes[i].y);
        if (mpq_cmp(g->nodes[i].z, mz) < 0) mpq_set(mz, g->nodes[i].z);
    }
    /* 平移：所有坐标减去最小值 [mpq_sub 精确] */
    for (i = 0; i < g->node_count; i++) {
        mpq_sub(g->nodes[i].x, g->nodes[i].x, mx);
        mpq_sub(g->nodes[i].y, g->nodes[i].y, my);
        mpq_sub(g->nodes[i].z, g->nodes[i].z, mz);
    }
    mpq_clear(mx); mpq_clear(my); mpq_clear(mz);
    return 0;
}

/* ================================================================
 * OP 4: kg_rewrite -- 应用指令块（扩展点）
 * ================================================================ */
int kg_rewrite(KGraph *g, KBlock *blk) {
    int i;
    if (!blk || blk->op_count <= 0) return 0;
    for (i = 0; i < blk->op_count; i++) {
        switch (blk->ops[i]) {
        case OP_NORMALIZE: kg_normalize(g); break;
        default: break;
        }
    }
    return 0;
}

/* ================================================================
 * OP 5: kg_unify -- 合一：node_a 取中点，约束引向 node_a，node_b 删除
 * ================================================================ */
int kg_unify(KGraph *g, int na, int nb) {
    int i;
    mpq_t sum, two;
    if (na < 0 || na >= g->node_count) return -1;
    if (nb < 0 || nb >= g->node_count) return -2;
    if (na == nb) return 0;
    mpq_init(sum); mpq_init(two);
    mpq_set_ui(two, 2, 1);   /* two = 2/1 [GMP] */

    /* 中点 = (A + B) / 2  [mpq_add + mpq_div 精确] */
    mpq_add(sum, g->nodes[na].x, g->nodes[nb].x);  /* sum = A.x+B.x */
    mpq_div(g->nodes[na].x, sum, two);             /* A.x = sum/2 */
    mpq_add(sum, g->nodes[na].y, g->nodes[nb].y);
    mpq_div(g->nodes[na].y, sum, two);
    mpq_add(sum, g->nodes[na].z, g->nodes[nb].z);
    mpq_div(g->nodes[na].z, sum, two);

    for (i = 0; i < g->constraint_count; i++) {
        if (g->constraints[i].node_a == nb) g->constraints[i].node_a = na;
        if (g->constraints[i].node_b == nb) g->constraints[i].node_b = na;
    }
    g->nodes[nb].id = -1; g->nodes[nb].type = 0;
    mpq_clear(sum); mpq_clear(two);
    return 0;
}

/* ================================================================
 * OP 6: kg_pack -- 紧凑化，移除已删除节点并更新约束引用
 * ================================================================ */
int kg_pack(KGraph *g) {
    int i, w = 0, m[KN];
    for (i = 0; i < KN; i++) m[i] = -1;
    for (i = 0; i < g->node_count; i++) {
        if (g->nodes[i].id == -1) continue;
        if (i != w) {
            g->nodes[w].id = w; g->nodes[w].type = g->nodes[i].type;
            strncpy(g->nodes[w].name, g->nodes[i].name, 32);
            mpq_set(g->nodes[w].x, g->nodes[i].x);  /* [mpq_set 精确复制] */
            mpq_set(g->nodes[w].y, g->nodes[i].y);
            mpq_set(g->nodes[w].z, g->nodes[i].z);
        } else { g->nodes[i].id = i; }
        m[i] = w++;
    }
    g->node_count = w;
    for (i = 0; i < g->constraint_count; i++) {
        KConstraint *c = &g->constraints[i];
        if (c->node_a >= 0 && m[c->node_a] >= 0) c->node_a = m[c->node_a];
        if (c->node_b >= 0 && m[c->node_b] >= 0) c->node_b = m[c->node_b];
    }
    return 0;
}

/* ================================================================
 * OP 7: kg_instantiate -- 复制模板图并应用指令块
 * ================================================================ */
int kg_instantiate(KGraph *dst, const KGraph *src, KBlock *blk) {
    int i;
    kg_init_graph(dst);
    strncpy(dst->name, src->name, 63); dst->name[63] = '\0';
    for (i = 0; i < src->node_count && i < KN; i++) {
        dst->nodes[i].id = src->nodes[i].id; dst->nodes[i].type = src->nodes[i].type;
        strncpy(dst->nodes[i].name, src->nodes[i].name, 32);
        mpq_set(dst->nodes[i].x, src->nodes[i].x);  /* GMP 精确复制 */
        mpq_set(dst->nodes[i].y, src->nodes[i].y);
        mpq_set(dst->nodes[i].z, src->nodes[i].z);
    }
    dst->node_count = src->node_count;
    for (i = 0; i < src->constraint_count && i < KC; i++) {
        dst->constraints[i].id = src->constraints[i].id;
        dst->constraints[i].type = src->constraints[i].type;
        dst->constraints[i].node_a = src->constraints[i].node_a;
        dst->constraints[i].node_b = src->constraints[i].node_b;
        dst->constraints[i].relation = src->constraints[i].relation;
        mpq_set(dst->constraints[i].value, src->constraints[i].value);  /* GMP 精确复制 */
    }
    dst->constraint_count = src->constraint_count;
    if (blk && blk->op_count > 0) kg_rewrite(dst, blk);
    return 0;
}

/* ================================================================
 * OP 8: kg_prove -- 调用 kg_solve 验证全部约束，返回 0=通过 1=失败
 * ================================================================ */
int kg_prove(const KGraph *g, const char *stmt) {
    if (!stmt || !*stmt || g->node_count <= 0) return 1;
    return (kg_solve((KGraph *)g, "1/1000000") == 0) ? 0 : 1;
}

/* ================================================================
 * OP 9: kg_export -- 5 种格式导出，全部使用 mpq_get_str 输出精确有理数
 * ================================================================ */
int kg_export(const KGraph *g, KExportFormat fmt, const char *fn) {
    FILE *fp = fopen(fn, "w");
    int i, r = 0; char *xs, *ys, *zs, *vs;
    if (!fp) return -2;

    switch (fmt) {
    case FORMAT_JSON:
        fprintf(fp, "{\"name\":\"%s\",\"nodes\":[", g->name);
        for (i = 0; i < g->node_count; i++) {
            KNode *n = &g->nodes[i];
            xs = mpq_get_str(NULL, 10, n->x); ys = mpq_get_str(NULL, 10, n->y); zs = mpq_get_str(NULL, 10, n->z);
            fprintf(fp, "%s{\"id\":%d,\"type\":%d,\"name\":\"%s\",\"x\":\"%s\",\"y\":\"%s\",\"z\":\"%s\"}",
                    i?",":"", n->id, n->type, n->name, xs, ys, zs);
            free(xs); free(ys); free(zs);
        }
        fprintf(fp, "],\"constraints\":[");
        for (i = 0; i < g->constraint_count; i++) {
            KConstraint *c = &g->constraints[i];
            vs = mpq_get_str(NULL, 10, c->value);
            fprintf(fp, "%s{\"id\":%d,\"type\":%d,\"na\":%d,\"nb\":%d,\"value\":\"%s\",\"rel\":%d}",
                    i?",":"", c->id, c->type, c->node_a, c->node_b, vs, c->relation);
            free(vs);
        }
        fprintf(fp, "]}\n"); break;

    case FORMAT_LATEX:
        fprintf(fp, "%% LaTeX/TikZ export: %s\n\\begin{tikzpicture}\n", g->name);
        for (i = 0; i < g->node_count; i++) {
            xs = mpq_get_str(NULL, 10, g->nodes[i].x); ys = mpq_get_str(NULL, 10, g->nodes[i].y);
            fprintf(fp, "  \\coordinate (%s) at (%s,%s);\n", g->nodes[i].name[0]?g->nodes[i].name:"N", xs, ys);
            free(xs); free(ys);
        }
        for (i = 0; i < g->constraint_count; i++)
            fprintf(fp, "  %% Constraint %d: n%d--n%d rel=%d\n",
                    g->constraints[i].id, g->constraints[i].node_a, g->constraints[i].node_b, g->constraints[i].relation);
        fprintf(fp, "\\end{tikzpicture}\n"); break;

    case FORMAT_LEAN4:
        fprintf(fp, "-- Lean 4 export: %s\nimport Mathlib\n\n", g->name);
        for (i = 0; i < g->node_count; i++) {
            xs = mpq_get_str(NULL, 10, g->nodes[i].x); ys = mpq_get_str(NULL, 10, g->nodes[i].y); zs = mpq_get_str(NULL, 10, g->nodes[i].z);
            fprintf(fp, "def %s_pt : Point := ⟨(%s : ℚ), (%s : ℚ), (%s : ℚ)⟩\n",
                    g->nodes[i].name[0]?g->nodes[i].name:"u", xs, ys, zs);
            free(xs); free(ys); free(zs);
        }
        for (i = 0; i < g->constraint_count; i++) {
            vs = mpq_get_str(NULL, 10, g->constraints[i].value);
            fprintf(fp, "theorem c%d : distSq n%d_pt n%d_pt = (%s : ℚ) := by sorry\n",
                    g->constraints[i].id, g->constraints[i].node_a, g->constraints[i].node_b, vs);
            free(vs);
        } break;

    case FORMAT_COQ:
        fprintf(fp, "(* Coq export: %s *)\nRequire Import QArith.\n\n", g->name);
        for (i = 0; i < g->node_count; i++) {
            xs = mpq_get_str(NULL, 10, g->nodes[i].x); ys = mpq_get_str(NULL, 10, g->nodes[i].y); zs = mpq_get_str(NULL, 10, g->nodes[i].z);
            fprintf(fp, "Definition %s_pt := ((%s)%%Q, (%s)%%Q, (%s)%%Q).\n",
                    g->nodes[i].name[0]?g->nodes[i].name:"u", xs, ys, zs);
            free(xs); free(ys); free(zs);
        }
        for (i = 0; i < g->constraint_count; i++) {
            vs = mpq_get_str(NULL, 10, g->constraints[i].value);
            fprintf(fp, "Theorem c%d : dist_sq n%d_pt n%d_pt = %s%%Q. Proof. Admitted.\n",
                    g->constraints[i].id, g->constraints[i].node_a, g->constraints[i].node_b, vs);
            free(vs);
        } break;

    case FORMAT_DOT:
        fprintf(fp, "// Graphviz DOT: %s\ngraph \"%s\" {\n  node [shape=circle];\n", g->name, g->name);
        for (i = 0; i < g->node_count; i++) {
            xs = mpq_get_str(NULL, 10, g->nodes[i].x); ys = mpq_get_str(NULL, 10, g->nodes[i].y); zs = mpq_get_str(NULL, 10, g->nodes[i].z);
            fprintf(fp, "  n%d [label=\"%s\\n(%s,%s,%s)\"];\n",
                    g->nodes[i].id, g->nodes[i].name[0]?g->nodes[i].name:"?", xs, ys, zs);
            free(xs); free(ys); free(zs);
        }
        for (i = 0; i < g->constraint_count; i++) {
            vs = mpq_get_str(NULL, 10, g->constraints[i].value);
            fprintf(fp, "  n%d -- n%d [label=\"%s\"];\n",
                    g->constraints[i].node_a, g->constraints[i].node_b, vs);
            free(vs);
        }
        fprintf(fp, "}\n"); break;

    default: r = -3; break;
    }
    fclose(fp); return r;
}

/* ================================================================
 * OP 10: kg_serialize -- 序列化，mpq_get_str 输出精确有理字符串
 * 格式: # name \n N id type name x y z \n C id type na nb value rel
 * ================================================================ */
int kg_serialize(const KGraph *g, char *buf, size_t sz) {
    int i, w = 0, rem = (int)sz, n;
    char *xs, *ys, *zs, *vs;
    if (!g || !buf || sz == 0) return -1;
    n = snprintf(buf, rem, "# %s\n", g->name); if (n < 0 || n >= rem) return -2; buf += n; rem -= n; w += n;
    for (i = 0; i < g->node_count; i++) {
        xs = mpq_get_str(NULL, 10, g->nodes[i].x); ys = mpq_get_str(NULL, 10, g->nodes[i].y); zs = mpq_get_str(NULL, 10, g->nodes[i].z);
        n = snprintf(buf, rem, "N %d %d %s %s %s %s\n", g->nodes[i].id, g->nodes[i].type, g->nodes[i].name, xs, ys, zs);
        free(xs); free(ys); free(zs);
        if (n < 0 || n >= rem) return -2; buf += n; rem -= n; w += n;
    }
    for (i = 0; i < g->constraint_count; i++) {
        vs = mpq_get_str(NULL, 10, g->constraints[i].value);
        n = snprintf(buf, rem, "C %d %d %d %d %s %d\n", g->constraints[i].id, g->constraints[i].type,
                     g->constraints[i].node_a, g->constraints[i].node_b, vs, g->constraints[i].relation);
        free(vs);
        if (n < 0 || n >= rem) return -2; buf += n; rem -= n; w += n;
    }
    return 0;
}

/* ================================================================
 * OP 11: kg_deserialize -- 反序列化，mpq_set_str 解析精确有理数
 * ================================================================ */
int kg_deserialize(KGraph *g, const char *buf) {
    char line[512], nm[32], xs[128], ys[128], zs[128], vs[128];
    int nid, nt, cid, ct, na, nb, rel;
    const char *p = buf;
    if (!g || !buf) return -1;
    kg_clear_graph(g); kg_init_graph(g);
    while (*p) {
        const char *nl = strchr(p, '\n'); size_t len = nl ? (size_t)(nl - p) : strlen(p);
        if (len >= sizeof(line)) len = sizeof(line) - 1;
        memcpy(line, p, len); line[len] = '\0'; p += len; if (*p == '\n') p++;
        if (len == 0) continue;
        if (line[0] == '#') { strncpy(g->name, line + 2, 63); g->name[63] = '\0'; }
        else if (line[0] == 'N' && sscanf(line, "N %d %d %31s %127s %127s %127s", &nid, &nt, nm, xs, ys, zs) == 6)
            kg_create_node(g, nt, nm, xs, ys, zs);
        else if (line[0] == 'C' && sscanf(line, "C %d %d %d %d %127s %d", &cid, &ct, &na, &nb, vs, &rel) == 6)
            kg_create_constraint(g, ct, na, nb, vs, rel);
    }
    return 0;
}

/* ================================================================
 * OP 12: kg_query -- 查询图摘要
 * ================================================================ */
int kg_query(const KGraph *g, char *buf, size_t sz) {
    int i, an = 0, dn = 0;
    if (!g || !buf || sz == 0) return -1;
    for (i = 0; i < g->node_count; i++) { if (g->nodes[i].id >= 0) an++; else dn++; }
    snprintf(buf, sz, "KGraph \"%s\"\n  nodes: %d (active %d, deleted %d) cap=%d\n  constraints: %d cap=%d\n",
             g->name[0]?g->name:"(unnamed)", g->node_count, an, dn, KN, g->constraint_count, KC);
    return 0;
}

/* ================================================================
 * main() -- 命令行入口，状态文件持久化
 * ================================================================ */
int main(int argc, char *argv[]) {
    KGraph g;
    const char *sf = "kg_state.dat";
    int op;

    kg_init_graph(&g);
    /* 加载持久化状态 */
    {   FILE *fp = fopen(sf, "r");
        if (fp) { fseek(fp, 0, SEEK_END); long fs = ftell(fp);
            if (fs > 0 && fs < 1024*1024) { char *b = malloc((size_t)fs+1);
                if (b) { fseek(fp, 0, SEEK_SET); size_t nr = fread(b, 1, (size_t)fs, fp); b[nr] = '\0'; kg_deserialize(&g, b); free(b); }
            } fclose(fp); }
    }

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <op> [args]\n", argv[0]);
        fprintf(stderr, "  0 create_node      <type> <name> <x> <y> <z>\n");
        fprintf(stderr, "  1 create_constraint <type> <na> <nb> <value> <rel>\n");
        fprintf(stderr, "  2 solve            [eps]\n");
        fprintf(stderr, "  3 normalize\n");
        fprintf(stderr, "  4 rewrite          (block)\n");
        fprintf(stderr, "  5 unify            <na> <nb>\n");
        fprintf(stderr, "  6 pack\n");
        fprintf(stderr, "  7 instantiate      (template)\n");
        fprintf(stderr, "  8 prove            <statement>\n");
        fprintf(stderr, "  9 export           <fmt:0-4> <filename>\n");
        fprintf(stderr, "  10 serialize       (to stdout)\n");
        fprintf(stderr, "  11 deserialize     (from stdin)\n");
        fprintf(stderr, "  12 query\n");
        kg_clear_graph(&g); return 1;
    }

    op = atoi(argv[1]);
    switch (op) {
    case 0: if (argc >= 7) { int r = kg_create_node(&g, atoi(argv[2]), argv[3], argv[4], argv[5], argv[6]); printf("create_node -> %d\n", r); } break;
    case 1: if (argc >= 8) { int r = kg_create_constraint(&g, atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), argv[5], atoi(argv[6])); printf("create_constraint -> %d\n", r); } break;
    case 2: { const char *e = (argc >= 3) ? argv[2] : "1/1000000"; printf("solve -> %d violations (eps=%s)\n", kg_solve(&g, e), e); } break;
    case 3: printf("normalize -> %d\n", kg_normalize(&g)); break;
    case 4: printf("rewrite -> 0\n"); break;
    case 5: if (argc >= 4) printf("unify -> %d\n", kg_unify(&g, atoi(argv[2]), atoi(argv[3]))); break;
    case 6: printf("pack -> %d\n", kg_pack(&g)); break;
    case 7: { KGraph d; KBlock b = {{0}, 0}; kg_instantiate(&d, &g, &b); printf("instantiate -> %d nodes, %d constraints\n", d.node_count, d.constraint_count); kg_clear_graph(&d); } break;
    case 8: { const char *s = (argc >= 3) ? argv[2] : "ok"; printf("prove -> %s\n", kg_prove(&g, s) == 0 ? "PASS" : "FAIL"); } break;
    case 9: if (argc >= 4) { int r = kg_export(&g, (KExportFormat)atoi(argv[2]), argv[3]); printf("export -> %d (%s)\n", r, argv[3]); } break;
    case 10: { char b[64*1024]; if (kg_serialize(&g, b, sizeof(b)) == 0) fputs(b, stdout); } break;
    case 11: { char b[64*1024]; size_t t = 0; while (t < sizeof(b)-1) { int c = getchar(); if (c == EOF) break; b[t++] = (char)c; } b[t] = '\0'; printf("deserialize -> %d\n", kg_deserialize(&g, b)); } break;
    case 12: { char b[1024]; kg_query(&g, b, sizeof(b)); fputs(b, stdout); } break;
    default: fprintf(stderr, "Unknown opcode: %d\n", op); break;
    }

    /* 持久化 */
    {   char b[64*1024];
        if (kg_serialize(&g, b, sizeof(b)) == 0) { FILE *fp = fopen(sf, "w"); if (fp) { fputs(b, fp); fclose(fp); } }
    }
    kg_clear_graph(&g);
    return 0;
}
