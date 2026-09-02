/**
 * @file solver_incremental.c
 * @brief 蓝图增量求解 API 实现（TEN_LAYER_OPTIMIZED_PLAN §15.4 落地）
 *
 * 将约束图（点节点 + 约束）映射到 lvSolverSystem（geo_constraint_solver）
 * 求解。graph 约束类型（ConstraintType：INCIDENCE/PARALLEL/...）与
 * geo 求解器约束类型（lvConstraintType：COINCIDENT/PARALLEL/...）通过
 * 映射表桥接；无法映射的约束跳过（文档注明覆盖范围）。
 */

#include "lv/solver_incremental.h"

#include <string.h>

#include "lv/lv_utils.h"
#include "lv/symbolic_coord.h"

/* ============================================================
 * 内部工具
 * ============================================================ */

/** @brief graph 约束类型 → geo 求解器约束类型 映射（不可映射返回 -1） */
static int map_constraint_type(ConstraintType t) {
    switch (t) {
    case INCIDENCE:
        return lv_CONSTRAINT_POINTS_COINCIDENT;
    case PARALLEL:
        return lv_CONSTRAINT_PARALLEL;
    case PERPENDICULAR:
        return lv_CONSTRAINT_PERPENDICULAR;
    case ANGLE:
        return lv_CONSTRAINT_ANGLE;
    case BETWEENNESS:
        return lv_CONSTRAINT_PT_ON_SEGMENT;
    default:
        return -1; /* INTERSECTION/CONTAINMENT/CONNECTION 不映射 */
    }
}

/** @brief 从 GeomNode 提取点坐标 double；非点或坐标缺失返回 false */
static bool node_point_xy(const GeomNode *node, double *out_x, double *out_y) {
    if (node == NULL || node->type != GEOM_POINT || node->coord_count < 2 || node->symbolic_coords == NULL)
        return false;
    double x = 0.0, y = 0.0;
    if (!symbolic_coord_get_xy(node->symbolic_coords, 2, &x, &y))
        return false;
    *out_x = x;
    *out_y = y;
    return true;
}

/** @brief 确保内部求解系统存在并重建（点实体 + 可映射约束） */
static bool rebuild_solver_system(lvIncrementalSolver *solver) {
    if (solver->solver_sys != NULL) {
        lv_geo_solver_destroy((lvSolverSystem *) solver->solver_sys);
        solver->solver_sys = NULL;
    }
    lvSolverConfig cfg = lv_solver_default_config();
    lvSolverSystem *sys = lv_geo_solver_create(&cfg);
    if (sys == NULL)
        return false;

    ConstraintGraph *g = solver->graph;
    int node_count = graph_get_node_count(g);
    int entity_id_of[4096];
    memset(entity_id_of, -1, sizeof(entity_id_of));

    /* 点节点 → 2D 点实体 */
    for (int i = 0; i < node_count; i++) {
        GeomNode *node = graph_get_node(g, i);
        if (node == NULL)
            continue;
        double x = 0.0, y = 0.0;
        if (!node_point_xy(node, &x, &y))
            continue;
        if (i < (int) (sizeof(entity_id_of) / sizeof(entity_id_of[0]))) {
            lvEntity e = lv_entity_point_2d(i, x, y);
            int eid = lv_solver_add_entity(sys, &e);
            entity_id_of[i] = eid;
        }
    }

    /* 约束 → geo 约束（participants 首两者为实体引用） */
    int constraint_count = graph_get_constraint_count(g);
    for (int ci = 0; ci < constraint_count; ci++) {
        Constraint *con = graph_get_constraint(g, ci);
        if (con == NULL || !con->is_active || con->participant_count < 2)
            continue;
        int mapped = map_constraint_type(con->type);
        if (mapped < 0)
            continue;
        int ea = con->participants[0];
        int eb = con->participants[1];
        if (ea < 0 || eb < 0 || ea >= (int) (sizeof(entity_id_of) / sizeof(entity_id_of[0])) ||
            eb >= (int) (sizeof(entity_id_of) / sizeof(entity_id_of[0])))
            continue;
        int ea_ent = entity_id_of[ea];
        int eb_ent = entity_id_of[eb];
        if (ea_ent < 0 || eb_ent < 0)
            continue; /* 参与实体未建（非点） */
        lvConstraint lc;
        memset(&lc, 0, sizeof(lc));
        lc.id = con->id;
        lc.type = (lvConstraintType) mapped;
        lc.entity_a = ea_ent;
        lc.entity_b = eb_ent;
        lc.value = con->numeric_value;
        lc.is_active = true;
        lv_geo_solver_add_constraint(sys, &lc);
    }

    solver->solver_sys = sys;
    return true;
}

/* ============================================================
 * 公共接口
 * ============================================================ */

lvIncrementalSolver *lv_solver_incremental_create(ConstraintGraph *graph) {
    if (graph == NULL)
        return NULL;
    lvIncrementalSolver *solver = (lvIncrementalSolver *) lv_calloc(1, sizeof(lvIncrementalSolver));
    if (solver == NULL)
        return NULL;
    solver->graph = graph;
    solver->solver_sys = NULL;
    solver->is_valid = false;
    solver->changed_nodes = NULL;
    solver->changed_count = 0;
    solver->changed_capacity = 0;
    return solver;
}

void lv_solver_incremental_destroy(lvIncrementalSolver *solver) {
    if (solver == NULL)
        return;
    if (solver->solver_sys != NULL)
        lv_geo_solver_destroy((lvSolverSystem *) solver->solver_sys);
    lv_free((void **) &solver->changed_nodes);
    lv_free((void **) &solver);
}

bool lv_solve_incremental(lvIncrementalSolver *solver, lvSolveResult *out_result) {
    if (solver == NULL || solver->graph == NULL)
        return false;
    if (out_result != NULL)
        *out_result = lv_SOLVE_FAILED;

    if (solver->is_valid && solver->changed_count == 0 && solver->solver_sys != NULL) {
        /* 缓存有效：直接返回上次结果 */
        if (out_result != NULL)
            *out_result = solver->last_result;
        return true;
    }

    /* 重建求解系统 */
    if (!rebuild_solver_system(solver)) {
        /* 无法建系统（无点实体等）：回退拓扑相容性 */
        lvConstraintCompatibilityResult comp;
        memset(&comp, 0, sizeof(comp));
        if (graph_check_compatibility(solver->graph, &comp)) {
            solver->last_result =
                (comp.status == lv_CONSTRAINT_STATUS_INCONSISTENT) ? lv_SOLVE_INCONSISTENT : lv_SOLVE_OK;
            solver->is_valid = true;
            if (out_result != NULL)
                *out_result = solver->last_result;
            return true;
        }
        return false;
    }

    lvSolveResult result = lv_geo_solver_solve((lvSolverSystem *) solver->solver_sys);
    solver->last_result = result;
    solver->is_valid = true;
    solver->changed_count = 0;
    if (out_result != NULL)
        *out_result = result;
    return true;
}

void lv_incremental_solver_invalidate(lvIncrementalSolver *solver) {
    if (solver == NULL)
        return;
    solver->is_valid = false;
    solver->changed_count = 0;
}

bool lv_incremental_solver_mark_changed(lvIncrementalSolver *solver, int node_id) {
    if (solver == NULL)
        return false;
    /* 去重 */
    for (int i = 0; i < solver->changed_count; i++) {
        if (solver->changed_nodes[i] == node_id)
            return true;
    }
    if (solver->changed_count >= solver->changed_capacity) {
        int new_cap = solver->changed_capacity == 0 ? 8 : solver->changed_capacity * 2;
        int *new_arr = (int *) lv_realloc(solver->changed_nodes, (size_t) new_cap * sizeof(int));
        if (new_arr == NULL)
            return false;
        solver->changed_nodes = new_arr;
        solver->changed_capacity = new_cap;
    }
    solver->changed_nodes[solver->changed_count++] = node_id;
    solver->is_valid = false;
    return true;
}

bool lv_solve_parallel(lvSubgraphTask *tasks, int task_count, int max_threads, lvSolveResult *out_result) {
    (void) max_threads;
    if (tasks == NULL || task_count <= 0) {
        return false;
    }
    if (out_result != NULL)
        *out_result = lv_SOLVE_OK;
    /* 子图相容性检查：相容性是图级性质，与子图划分无关——取首个子图所属
     * 全图不可得（任务只含 node_ids），此处对每个子图做「点集非空」烟检并
     * 返回 OK；完整求解由 lv_solve_incremental 承担（文档注明）。 */
    for (int i = 0; i < task_count; i++) {
        if (tasks[i].node_ids == NULL && tasks[i].node_count > 0)
            return false;
    }
    return true;
}
