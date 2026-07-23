/**
 * @file proof_demo.c
 * @brief 证明系统演示 —— 展示约束图构造、几何构造与证明引擎验证
 *
 * 本示例演示 Lv-00 证明系统的完整工作流程：
 * 1. 创建约束图并添加几何构造（点、线段）
 * 2. 构建命题模式（等腰三角形判定）
 * 3. 使用证明导航器管理证明步骤
 * 4. 执行合一检查验证构造是否满足命题
 * 5. 执行逻辑自检（一致性、循环性、完备性）
 * 6. 打印完整的证明结果
 *
 * 编译方式：
 *   gcc -o proof_demo proof_demo.c -llv -lgmp
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv.h"

/* ============================================================
 * 辅助函数：添加一个有理数坐标的点
 *
 * 使用分子/分母形式创建精确的有理数坐标点，
 * 避免浮点数带来的精度损失。
 *
 * @param g    约束图指针
 * @param xn   X 坐标分子
 * @param xd   X 坐标分母（必须 > 0）
 * @param yn   Y 坐标分子
 * @param yd   Y 坐标分母（必须 > 0）
 * @return     新节点的 ID，失败返回 -1
 * ============================================================ */
static int add_point(ConstraintGraph *g, int64_t xn, uint64_t xd,
                     int64_t yn, uint64_t yd) {
    if (!g || xd == 0 || yd == 0) {
        fprintf(stderr, "add_point: 无效参数\n");
        return -1;
    }

    /* 创建符号坐标 —— 使用精确有理数表示 */
    SymbolicCoord *cx = symbolic_coord_create_rational(xn, xd);
    SymbolicCoord *cy = symbolic_coord_create_rational(yn, yd);
    if (!cx || !cy) {
        fprintf(stderr, "add_point: 坐标创建失败\n");
        return -1;
    }

    /* 将坐标数组传入约束图，创建点节点 */
    SymbolicCoord *coords[] = {cx, cy};
    AddNodeResult res = graph_add_point(g, coords, 2);
    if (res != ADD_NODE_OK) {
        fprintf(stderr, "add_point: 添加节点失败 (错误码=%d)\n", res);
        return -1;
    }

    /* 返回新创建的节点 ID（next_node_id 在添加后自增） */
    return g->next_node_id - 1;
}

/* ============================================================
 * 步骤1：构造等腰三角形
 *
 * 创建一个等腰三角形 ABC，其中：
 * - A = (0, 0)    左下顶点
 * - B = (4, 0)    右下顶点
 * - C = (2, 3)    顶部顶点
 *
 * AC = BC = sqrt(4+9) = sqrt(13)，所以三角形 ABC 是等腰三角形。
 * ============================================================ */
static void construct_isosceles_triangle(ConstraintGraph *g,
                                          int *out_a, int *out_b, int *out_c) {
    printf("  创建顶点 A(0, 0)...\n");
    int a = add_point(g, 0, 1, 0, 1);

    printf("  创建顶点 B(4, 0)...\n");
    int b = add_point(g, 4, 1, 0, 1);

    printf("  创建顶点 C(2, 3)...\n");
    int c = add_point(g, 2, 1, 3, 1);

    *out_a = a;
    *out_b = b;
    *out_c = c;
}

/* ============================================================
 * 步骤2：添加边和几何约束
 *
 * 为三角形的三条边创建线段节点，并添加 betweenness 约束
 * 来表示顶点之间的空间关系。
 * ============================================================ */
static void add_edges_and_constraints(ConstraintGraph *g,
                                       int a, int b, int c) {
    /* 添加三条边 */
    printf("  添加边 AB...\n");
    graph_add_line_segment(g, a, b);

    printf("  添加边 BC...\n");
    graph_add_line_segment(g, b, c);

    printf("  添加边 CA...\n");
    graph_add_line_segment(g, c, a);

    /* 添加 betweenness 约束：C 在 A 和 B 的上方（确定三角形方向） */
    printf("  添加 betweenness 约束...\n");
    graph_add_betweenness(g, a, c, b);
}

/* ============================================================
 * 步骤3：创建等腰三角形判定命题
 *
 * 命题模式：如果三角形有两条边长度相等，则它是等腰三角形。
 * 我们通过创建一个抽象的命题模式图来表达这个命题。
 * ============================================================ */
static Proposition *create_isosceles_proposition(void) {
    printf("  创建等腰三角形判定命题...\n");

    /* 创建原子命题 */
    Proposition *prop = proposition_create(1, PROPOSITION_ATOMIC);
    if (!prop) {
        fprintf(stderr, "  命题创建失败\n");
        return NULL;
    }

    /* 设置命题名称和描述 */
    prop->name = lv_strdup_safe("IsoscelesTriangle");
    prop->description = lv_strdup_safe("等腰三角形判定：两条边相等的三角形是等腰三角形");

    /* 创建命题的模式约束图 */
    ConstraintGraph *pattern = graph_create();
    int p1 = add_point(pattern, 0, 1, 0, 1);
    int p2 = add_point(pattern, 1, 1, 0, 1);
    int p3 = add_point(pattern, 0, 1, 1, 1);

    /* 命题模式中的边 */
    graph_add_line_segment(pattern, p1, p2);
    graph_add_line_segment(pattern, p2, p3);
    graph_add_line_segment(pattern, p3, p1);

    /* 添加 betweenness 约束作为命题的结构要求 */
    graph_add_betweenness(pattern, p1, p3, p2);

    /* 将模式图关联到命题 */
    proposition_set_pattern(prop, pattern);

    printf("  命题创建成功 (ID=%d, 模式图节点数=%d)\n",
           prop->id, pattern->node_count);

    return prop;
}

/* ============================================================
 * 步骤4：使用证明导航器构建证明
 *
 * 证明导航器管理证明步骤的添加、导航和验证。
 * 我们将构造过程组织为一系列证明步骤。
 * ============================================================ */
static ProofNavigator *build_proof(ConstraintGraph *construction,
                                    Proposition *proposition) {
    printf("  创建证明导航器...\n");

    /* 创建证明导航器，关联目标命题 */
    ProofNavigator *nav = proof_navigator_create(proposition, NULL);
    if (!nav) {
        fprintf(stderr, "  证明导航器创建失败\n");
        return NULL;
    }

    /* 设置证明策略注释（LeanGeo 风格） */
    proof_navigator_set_strategy_note(nav,
        "通过构造等腰三角形 ABC（其中 AC=BC），验证构造满足等腰三角形的定义");

    /* ---- 证明步骤1：添加顶点 ---- */
    ProofStep *step1 = proof_step_create(PROOF_STEP_ADD_NODE);
    step1->note = lv_strdup_safe("构造三角形的三个顶点 A(0,0), B(4,0), C(2,3)");
    proof_navigator_add_step(nav, step1);

    /* ---- 证明步骤2：添加边 ---- */
    ProofStep *step2 = proof_step_create(PROOF_STEP_ADD_CONSTRAINT);
    step2->note = lv_strdup_safe("连接顶点形成三条边 AB, BC, CA");
    proof_step_add_dependency(step2, step1->id);  /* 步骤2依赖步骤1 */
    proof_navigator_add_step(nav, step2);

    /* ---- 证明步骤3：添加约束 ---- */
    ProofStep *step3 = proof_step_create(PROOF_STEP_ADD_CONSTRAINT);
    step3->note = lv_strdup_safe("添加 betweenness 约束确定三角形方向");
    proof_step_add_dependency(step3, step1->id);
    proof_navigator_add_step(nav, step3);

    /* ---- 证明步骤4：执行合一检查 ---- */
    ProofStep *step4 = proof_step_create(PROOF_STEP_UNIFY);
    step4->note = lv_strdup_safe("合一检查：验证构造是否满足命题模式");
    proof_step_add_dependency(step4, step2->id);
    proof_step_add_dependency(step4, step3->id);
    proof_navigator_add_step(nav, step4);

    printf("  证明导航器创建成功 (步骤数=%d)\n", nav->step_count);

    return nav;
}

/* ============================================================
 * 步骤5：执行逻辑自检
 *
 * 使用逻辑检查系统验证证明的质量：
 * - 一致性检查：检测证明中的矛盾
 * - 循环性检查：检测循环推理
 * - 完备性检查：验证所有断言都有合法来源
 * ============================================================ */
static void run_logic_check(ProofNavigator *nav) {
    printf("\n[逻辑自检]\n");

    /* 创建逻辑检查上下文 */
    lvLogicContext *ctx = lv_logic_check_context_create(nav);
    if (!ctx) {
        fprintf(stderr, "  逻辑检查上下文创建失败\n");
        return;
    }
    ctx->verbose = true;

    /* 创建报告 */
    lvLogicReport *report = lv_logic_report_create();
    if (!report) {
        fprintf(stderr, "  逻辑检查报告创建失败\n");
        lv_logic_check_context_destroy(ctx);
        return;
    }

    /* 执行综合逻辑检查（一致性 + 循环性 + 完备性） */
    int issues = lv_logic_check_all(ctx, report);
    printf("  综合检查完成，发现问题数: %d\n", issues);

    /* 打印检查结果摘要 */
    printf("  一致性: %s\n", report->is_consistent ? "通过" : "存在问题");
    printf("  无循环: %s\n", report->is_non_circular ? "通过" : "存在循环");
    printf("  完备性: %s\n", report->is_complete ? "通过" : "存在问题");
    printf("  总问题数: %d (错误=%d, 警告=%d, 信息=%d)\n",
           report->total_issues, report->error_count,
           report->warning_count, report->info_count);

    /* 导出文本报告 */
    char *text_report = lv_logic_report_to_text(report, false);
    if (text_report) {
        printf("\n  --- 逻辑检查报告 ---\n");
        printf("%s\n", text_report);
        lv_free(text_report);
    }

    /* 清理 */
    lv_logic_report_destroy(report);
    lv_logic_check_context_destroy(ctx);
}

/* ============================================================
 * 主函数
 * ============================================================ */
int main(void) {
    printf("========================================\n");
    printf("  Lv-00 证明系统演示\n");
    printf("  等腰三角形构造与验证\n");
    printf("========================================\n\n");

    /* ---- 初始化系统 ---- */
    printf("[初始化] Lv-00 系统版本: %s\n\n", lv_get_version_string());

    /* ======== 步骤1：构造等腰三角形 ======== */
    printf("[1/5] 构造等腰三角形...\n");
    ConstraintGraph *construction = graph_create();
    int a, b, c;
    construct_isosceles_triangle(construction, &a, &b, &c);
    printf("  三角形顶点: A=%d, B=%d, C=%d\n\n", a, b, c);

    /* ======== 步骤2：添加边和约束 ======== */
    printf("[2/5] 添加边和几何约束...\n");
    add_edges_and_constraints(construction, a, b, c);
    printf("  节点数: %d, 约束数: %d\n\n",
           construction->node_count, construction->constraint_count);

    /* ======== 步骤3：创建命题 ======== */
    printf("[3/5] 创建等腰三角形判定命题...\n");
    Proposition *proposition = create_isosceles_proposition();
    printf("\n");

    /* ======== 步骤4：构建证明并执行合一检查 ======== */
    printf("[4/5] 构建证明并执行合一检查...\n");
    ProofNavigator *nav = build_proof(construction, proposition);
    if (nav) {
        /* 执行合一检查：验证构造图是否满足命题模式 */
        UnifyStatus status = proof_unify(construction, proposition, true);
        printf("  合一检查结果: %s\n\n", unify_result_to_string(status));

        /* 计算最终证明颜色 */
        ProofColor color = proof_navigator_compute_final_color(nav);
        printf("  最终证明颜色: %s\n\n", proof_color_to_string(color));
    }

    /* ======== 步骤5：逻辑自检 ======== */
    printf("[5/5] 执行逻辑自检...\n");
    if (nav) {
        run_logic_check(nav);
    }

    /* ---- 清理资源 ---- */
    printf("\n[清理] 释放资源...\n");
    if (nav) {
        proof_navigator_destroy(nav);
    }
    if (proposition) {
        proposition_destroy(proposition);
    }
    graph_destroy(construction);

    printf("\n示例完成！\n");
    return 0;
}
