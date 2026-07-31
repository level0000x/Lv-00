/**
 * @file geo_constraint_solver_system.c
 * @brief 几何约束求解器 —— 求解器系统创建/释放与实体/约束管理
 */

#include "geo_constraint_solver_internal.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * 第三部分：求解器配置
 * ======================================================================== */

/**
 * @brief 获取默认求解器配置
 */
lv_PUBLIC_API lvSolverConfig lv_solver_default_config(void) {
    lvSolverConfig cfg;
    cfg.max_iterations = 50;
    cfg.convergence_tol = lv_EPSILON_HIGH;
    cfg.damping_factor = 0.8;
    cfg.min_step = lv_EPSILON_SUPERTINY;
    cfg.verbose = false;
    return cfg;
}

/* ========================================================================
 * 第四部分：求解器系统创建与释放
 * ======================================================================== */

/**
 * @brief 创建约束求解系统
 *
 * 内部分配 lvSolverSystemEx（包含哈希索引），返回其 base 字段指针。
 * 由于 lvSolverSystemEx 的第一个字段是 lvSolverSystem base，
 * 返回的指针可以安全地作为 lvSolverSystem* 使用（向后兼容）。
 *
 * @param config 配置（NULL 使用默认配置）
 * @return 求解系统指针，失败返回 NULL
 */
lv_PUBLIC_API lvSolverSystem *lv_geo_solver_create(const lvSolverConfig *config) {
    lvSolverSystemEx *sys_ex = (lvSolverSystemEx *) lv_malloc(sizeof(lvSolverSystemEx));
    if (!sys_ex)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_geo_solver_create: sys_ex malloc failed");

    lvSolverSystem *sys = &sys_ex->base;

    sys->entities = (lvEntity *) lv_malloc(INITIAL_CAPACITY * sizeof(lvEntity));
    if (!sys->entities) {
        lv_free((void **) &(sys_ex));
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_geo_solver_create: entities malloc failed");
    }

    sys->constraints = (lvConstraint *) lv_calloc(INITIAL_CAPACITY, sizeof(lvConstraint));
    if (!sys->constraints) {
        lv_free((void **) &(sys->entities));
        lv_free((void **) &(sys_ex));
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_geo_solver_create: constraints calloc failed");
    }

    sys->entity_count = 0;
    sys->entity_capacity = INITIAL_CAPACITY;
    sys->constraint_count = 0;
    sys->constraint_capacity = INITIAL_CAPACITY;

    if (config) {
        sys->config = *config;
    } else {
        sys->config = lv_solver_default_config();
    }

    sys->last_result = lv_SOLVE_OK;
    sys->iteration_count = 0;

    /* 初始化哈希索引表 */
    id_hash_init(&sys_ex->entity_hash);
    id_hash_init(&sys_ex->constraint_hash);

    return sys;
}

/**
 * @brief 释放约束求解系统
 *
 * 释放时将 lvSolverSystem* 转换回 lvSolverSystemEx*，
 * 以释放哈希索引资源。由于 create 分配的是 lvSolverSystemEx，
 * 此处必须使用相同的指针进行释放。
 */
lv_PUBLIC_API void lv_geo_solver_destroy(lvSolverSystem *sys) {
    if (!sys)
        return;
    lv_free((void **) &(sys->entities));
    lv_free((void **) &(sys->constraints));
    /* sys 实际指向 lvSolverSystemEx.base，直接 free 即可释放整个 Ex 结构体 */
    lv_free((void **) &(sys));
}

/* ========================================================================
 * 第五部分：实体管理
 * ======================================================================== */

/**
 * @brief 在实体数组中查找指定 ID 的索引
 * @return 索引（-1 表示未找到）
 */
static int find_entity_index(const lvSolverSystem *sys, int id) {
    for (int i = 0; i < sys->entity_count; i++) {
        if (sys->entities[i].id == id)
            return i;
    }
    return -1;
}

/**
 * @brief 添加几何实体
 *
 * 添加实体后同步插入哈希索引表，实现 O(1) 后续查找。
 *
 * @return 新实体的 ID
 */
lv_PUBLIC_API int lv_solver_add_entity(lvSolverSystem *sys, const lvEntity *entity) {
    if (!sys || !entity)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "lv_solver_add_entity: NULL sys or entity");

    /* 检查 ID 是否已存在 */
    if (find_entity_index(sys, entity->id) >= 0)
        lv_RETURN_ERROR(lv_ERROR_NODE_CONFLICT, "lv_solver_add_entity: entity ID already exists");

    /* 扩容 */
    if (sys->entity_count >= sys->entity_capacity) {
        int new_cap = sys->entity_capacity * 2;
        lvEntity *tmp = (lvEntity *) lv_realloc(sys->entities, new_cap * sizeof(lvEntity));
        if (!tmp)
            return -1;
        sys->entities = tmp;
        sys->entity_capacity = new_cap;
    }

    int new_index = sys->entity_count;
    sys->entities[new_index] = *entity;
    sys->entity_count++;

    /* 同步插入哈希表 */
    lvSolverSystemEx *sys_ex = (lvSolverSystemEx *) sys;
    if (!id_hash_insert(&sys_ex->entity_hash, entity->id, new_index)) {
        /* 哈希表插入失败（已满或 ID 冲突），线性扫描 fallback 仍然有效 */
        /* 此处仅记录警告，不影响功能 */
    }

    return entity->id;
}

/**
 * @brief 获取实体指针
 *
 * 优先使用哈希表 O(1) 查找，找不到时 fallback 到线性扫描。
 *
 * @return 实体指针（NULL 表示不存在）
 */
lv_PUBLIC_API lvEntity *lv_solver_get_entity(lvSolverSystem *sys, int id) {
    if (!sys)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_solver_get_entity: NULL sys");

    /* 优先使用哈希表查找 */
    lvSolverSystemEx *sys_ex = (lvSolverSystemEx *) sys;
    int idx = find_entity_index_fast(sys_ex, id);
    if (idx >= 0 && idx < sys->entity_count && sys->entities[idx].id == id) {
        return &sys->entities[idx];
    }

    /* Fallback：线性扫描（兼容非 Ex 分配的系统） */
    idx = find_entity_index(sys, id);
    if (idx < 0)
        return NULL;
    return &sys->entities[idx];
}

/* ========================================================================
 * 第六部分：约束管理
 * ======================================================================== */

/**
 * @brief 在约束数组中查找指定 ID 的索引
 * @return 索引（-1 表示未找到）
 */
static int find_constraint_index(const lvSolverSystem *sys, int id) {
    for (int i = 0; i < sys->constraint_count; i++) {
        if (sys->constraints[i].id == id)
            return i;
    }
    return -1;
}

/**
 * @brief 添加约束
 *
 * 添加约束后同步插入哈希索引表，实现 O(1) 后续查找。
 *
 * @return 新约束的 ID
 */
lv_PUBLIC_API int lv_geo_solver_add_constraint(lvSolverSystem *sys, const lvConstraint *c) {
    if (!sys || !c)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "lv_geo_solver_add_constraint: NULL sys or constraint");

    /* 检查 ID 是否已存在 */
    if (find_constraint_index(sys, c->id) >= 0)
        lv_RETURN_ERROR(lv_ERROR_NODE_CONFLICT, "lv_geo_solver_add_constraint: constraint ID already exists");

    /* 扩容 */
    if (sys->constraint_count >= sys->constraint_capacity) {
        int new_cap = sys->constraint_capacity * 2;
        lvConstraint *tmp = (lvConstraint *) lv_realloc(sys->constraints, new_cap * sizeof(lvConstraint));
        if (!tmp)
            lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_geo_solver_add_constraint: realloc failed");
        sys->constraints = tmp;
        sys->constraint_capacity = new_cap;
    }

    int new_index = sys->constraint_count;
    sys->constraints[new_index] = *c;
    sys->constraint_count++;

    /* 同步插入哈希表 */
    lvSolverSystemEx *sys_ex = (lvSolverSystemEx *) sys;
    if (!id_hash_insert(&sys_ex->constraint_hash, c->id, new_index)) {
        /* 哈希表插入失败（已满或 ID 冲突），线性扫描 fallback 仍然有效 */
        /* 此处仅记录警告，不影响功能 */
    }

    /* FIXED 约束：自动标记目标实体为固定 */
    if (c->type == lv_CONSTRAINT_FIXED && c->entity_a >= 0) {
        lvEntity *entity = lv_solver_get_entity(sys, c->entity_a);
        if (entity) {
            entity->is_fixed = true;
            memcpy(entity->initial_params, entity->params,
                   sizeof(double) * (entity->param_count > 8 ? 8 : entity->param_count));
        }
    }

    return c->id;
}

/**
 * @brief 获取约束指针
 *
 * 优先使用哈希表 O(1) 查找，找不到时 fallback 到线性扫描。
 */
lv_PUBLIC_API lvConstraint *lv_solver_get_constraint(lvSolverSystem *sys, int id) {
    if (!sys)
        return NULL;

    /* 优先使用哈希表查找 */
    lvSolverSystemEx *sys_ex = (lvSolverSystemEx *) sys;
    int idx = find_constraint_index_fast(sys_ex, id);
    if (idx >= 0 && idx < sys->constraint_count && sys->constraints[idx].id == id) {
        return &sys->constraints[idx];
    }

    /* Fallback：线性扫描（兼容非 Ex 分配的系统） */
    idx = find_constraint_index(sys, id);
    if (idx < 0)
        return NULL;
    return &sys->constraints[idx];
}

/**
 * @brief 移除约束
 *
 * 使用 swap-and-pop 策略删除约束，同时更新哈希表中
 * 被移动元素的索引映射。
 */
lv_PUBLIC_API bool lv_geo_solver_remove_constraint(lvSolverSystem *sys, int id) {
    if (!sys)
        return false;
    int idx = find_constraint_index(sys, id);
    if (idx < 0)
        return false;

    lvSolverSystemEx *sys_ex = (lvSolverSystemEx *) sys;

    /* 如果被删除的不是最后一个元素，需要更新被移动元素的哈希索引 */
    int last_index = sys->constraint_count - 1;
    if (idx != last_index) {
        int moved_id = sys->constraints[last_index].id;
        /* 先从哈希表移除被移动元素的旧索引 */
        id_hash_remove(&sys_ex->constraint_hash, moved_id);
        /* 重新插入到新位置 */
        id_hash_insert(&sys_ex->constraint_hash, moved_id, idx);
    }

    /* 从哈希表移除被删除的约束 */
    id_hash_remove(&sys_ex->constraint_hash, id);

    /* 将最后一个元素移到被删除的位置 */
    sys->constraints[idx] = sys->constraints[last_index];
    sys->constraint_count--;
    return true;
}


/* ========================================================================
 * 第十三部分：交互支持
 * ======================================================================== */

/**
 * @brief 设置实体固定状态
 *
 * 固定实体不参与求解，其参数保持不变。
 */
lv_PUBLIC_API void lv_solver_set_fixed(lvSolverSystem *sys, int entity_id, bool fixed) {
    if (!sys)
        return;
    lvEntity *e = lv_solver_get_entity(sys, entity_id);
    if (e) {
        e->is_fixed = fixed;
    }
}

/**
 * @brief 设置实体拖拽状态（用于实时反馈）
 *
 * 被拖拽的实体不参与求解，其位置由外部控制。
 */
lv_PUBLIC_API void lv_solver_set_dragged(lvSolverSystem *sys, int entity_id, bool dragged) {
    if (!sys)
        return;
    lvEntity *e = lv_solver_get_entity(sys, entity_id);
    if (e) {
        e->is_dragged = dragged;
    }
}

/**
 * @brief 设置实体拖拽位置（用于实时反馈）
 *
 * 仅对 2D 点实体有效，设置其 (x, y) 坐标。
 */
lv_PUBLIC_API void lv_solver_set_drag_position(lvSolverSystem *sys, int entity_id, double x, double y) {
    if (!sys)
        return;
    lvEntity *e = lv_solver_get_entity(sys, entity_id);
    if (e && (e->type == lv_ENTITY_POINT_2D || e->param_count >= 2)) {
        e->params[0] = x;
        e->params[1] = y;
        e->is_dragged = true;
    }
}

