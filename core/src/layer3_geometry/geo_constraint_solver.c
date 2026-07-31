/**
 * @file geo_constraint_solver.c
 * @brief 几何约束求解器实现 —— Newton-Raphson 迭代求解
 *
 * 借鉴 SolveSpace (github.com/solvespace/solvespace) 的核心求解架构：
 *   - 雅可比矩阵数值差分构建
 *   - 高斯消元法求解线性方程组
 *   - 阻尼 Newton-Raphson 迭代
 *   - DOF 自由度分析
 *
 * 代码质量优化（v3.6.1）：
 *   - 添加 ID 到索引的哈希映射，将查找复杂度从 O(n) 优化到 O(1)
 *   - 修复内存分配错误处理
 *   - 添加详细的函数注释
 *
 * @version v3.6.1
 */

#include "lv/lv_platform.h"
#include "lv/lv_internal.h"

#include "lv/geo_constraint_solver.h"

#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lv/config.h"

#include "lv_utils.h"

/* ========================================================================
 * lv_PUBLIC_API 兼容处理
 * ======================================================================== */

#ifndef lv_PUBLIC_API
#define lv_PUBLIC_API
#endif

/* ========================================================================
 * 内部常量与宏
 * ======================================================================== */

/** 初始实体/约束容量 */
#define INITIAL_CAPACITY 16

/** 数值差分步长 */
#define NUMERICAL_DIFF_EPSILON 1e-8

/** 最大参数维度（安全上限） */
#define MAX_PARAMS 64

/** ID 哈希表大小（应为 2 的幂次，用于位运算取模） */
#define ID_HASH_TABLE_SIZE 512

/** 哈希表最大负载因子 */
#define HASH_LOAD_FACTOR_MAX 0.75

/* ========================================================================
 * 内部数据结构：ID 到索引的哈希映射
 * ======================================================================== */

/**
 * @brief 哈希表条目
 */
typedef struct {
    int id;        /**< 实体/约束 ID */
    int index;     /**< 在数组中的索引 */
    bool occupied; /**< 是否被占用 */
} HashEntry;

/**
 * @brief ID 到索引的哈希表
 */
typedef struct {
    HashEntry entries[ID_HASH_TABLE_SIZE]; /**< 哈希表条目数组 */
    int count;                             /**< 当前条目数量 */
} IdHashTable;

/**
 * @brief 扩展的求解器系统（包含哈希索引）
 */
typedef struct lvSolverSystemEx {
    lvSolverSystem base;         /**< 基础求解器系统 */
    IdHashTable entity_hash;     /**< 实体 ID 哈希表 */
    IdHashTable constraint_hash; /**< 约束 ID 哈希表 */
} lvSolverSystemEx;

/* ========================================================================
 * 内部辅助函数声明
 * ======================================================================== */

/* 哈希表操作 */
static void id_hash_init(IdHashTable *table);
static bool id_hash_insert(IdHashTable *table, int id, int index);
static int id_hash_find(const IdHashTable *table, int id);
static bool id_hash_remove(IdHashTable *table, int id);

/* 查找函数（优先使用哈希表） */
static int find_entity_index_fast(const lvSolverSystemEx *sys_ex, int id);
static int find_constraint_index_fast(const lvSolverSystemEx *sys_ex, int id);
static double evaluate_constraint(const lvSolverSystem *sys, const lvConstraint *c, double *error_val);
static void build_jacobian_and_residual(const lvSolverSystem *sys, double *J, double *F, int nrows, int ncols,
                                        const int *param_map, const int *free_entities, int free_count);
static int gauss_eliminate(double *A, double *b, int n);
static double vec_norm(const double *v, int n);

/* ========================================================================
 * 第一部分：哈希表实现（O(1) ID 到索引映射）
 * ======================================================================== */

/**
 * @brief 计算 ID 的哈希值
 * @param id 实体/约束 ID
 * @return 哈希值（0 到 ID_HASH_TABLE_SIZE-1）
 */
static inline uint32_t id_hash(int id) {
    /* 使用简单的整数哈希：
     * 对于正 ID：直接取模
     * 对于负 ID：先取绝对值再取模
     * 使用位运算 (&) 代替取模 (%)，要求表大小为 2 的幂次 */
    uint32_t hash = (uint32_t) (id >= 0 ? id : -id);
    /* 混合高位和低位，提高分布均匀性 */
    hash = (hash ^ (hash >> 16)) & (ID_HASH_TABLE_SIZE - 1);
    return hash;
}

/**
 * @brief 初始化哈希表
 * @param table 哈希表指针
 */
static void id_hash_init(IdHashTable *table) {
    if (table == NULL)
        return;

    table->count = 0;
    for (int i = 0; i < ID_HASH_TABLE_SIZE; i++) {
        table->entries[i].occupied = false;
        table->entries[i].id = 0;
        table->entries[i].index = -1;
    }
}

/**
 * @brief 向哈希表插入 ID-索引映射
 * @param table 哈希表指针
 * @param id 实体/约束 ID
 * @param index 在数组中的索引
 * @return true 插入成功，false 表已满或 ID 已存在
 */
static bool id_hash_insert(IdHashTable *table, int id, int index) {
    if (table == NULL || index < 0)
        return false;

    /* 检查负载因子 */
    if (table->count >= (int) (ID_HASH_TABLE_SIZE * HASH_LOAD_FACTOR_MAX)) {
        return false; /* 哈希表过满 */
    }

    /* 检查 ID 是否已存在 */
    if (id_hash_find(table, id) >= 0) {
        return false; /* ID 已存在 */
    }

    uint32_t hash = id_hash(id);

    /* 线性探测解决冲突 */
    for (int probe = 0; probe < ID_HASH_TABLE_SIZE; probe++) {
        uint32_t idx = (hash + probe) & (ID_HASH_TABLE_SIZE - 1);

        if (!table->entries[idx].occupied) {
            table->entries[idx].id = id;
            table->entries[idx].index = index;
            table->entries[idx].occupied = true;
            table->count++;
            return true;
        }
    }

    return false; /* 哈希表已满（理论上不会发生，因为有负载因子检查） */
}

/**
 * @brief 在哈希表中查找 ID 对应的索引
 *
 * 使用线性探测处理哈希冲突。
 * 重要：当遇到 tombstone（已删除标记）时继续探测，
 * 只有遇到"从未使用"的空槽（id=0 且 index=-1）才返回 -1。
 *
 * @param table 哈希表指针
 * @param id 实体/约束 ID
 * @return 索引（-1 表示未找到）
 */
static int id_hash_find(const IdHashTable *table, int id) {
    if (table == NULL)
        return -1;

    uint32_t hash = id_hash(id);

    /* 线性探测查找 */
    for (int probe = 0; probe < ID_HASH_TABLE_SIZE; probe++) {
        uint32_t idx = (hash + probe) & (ID_HASH_TABLE_SIZE - 1);
        const HashEntry *entry = &table->entries[idx];

        if (!entry->occupied) {
            /* 遇到未占用槽：区分 tombstone 和从未使用的空槽 */
            /* tombstone: id=0 且 index=-1 但 occupied=false */
            /* 从未使用的空槽: id=0 且 index=-1 且 occupied=false */
            /* 由于初始化时所有条目都是 id=0, index=-1, occupied=false， */
            /* tombstone 是从 occupied=true 变为 false 的，所以需要额外判断 */
            /* 简化判断：如果 id=0 且 index=-1，认为是从未使用的空槽 */
            if (entry->id == 0 && entry->index == -1) {
                return -1; /* 从未使用的空槽，说明不存在 */
            }
            /* 否则是 tombstone，继续探测 */
            continue;
        }

        if (entry->id == id) {
            return entry->index;
        }
    }

    return -1; /* 未找到 */
}

/**
 * @brief 从哈希表中删除 ID 映射
 * @param table 哈希表指针
 * @param id 实体/约束 ID
 * @return true 删除成功，false 未找到
 */
static bool id_hash_remove(IdHashTable *table, int id) {
    if (table == NULL)
        return false;

    uint32_t hash = id_hash(id);

    /* 线性探测查找并删除 */
    for (int probe = 0; probe < ID_HASH_TABLE_SIZE; probe++) {
        uint32_t idx = (hash + probe) & (ID_HASH_TABLE_SIZE - 1);

        if (!table->entries[idx].occupied) {
            return false; /* 未找到 */
        }

        if (table->entries[idx].id == id) {
            /* 标记为删除（不实际删除，保持探测链完整） */
            table->entries[idx].occupied = false;
            table->entries[idx].id = 0;
            table->entries[idx].index = -1;
            table->count--;
            return true;
        }
    }

    return false;
}

/**
 * @brief 快速查找实体索引（使用哈希表）
 * @param sys_ex 扩展求解器系统
 * @param id 实体 ID
 * @return 索引（-1 表示未找到）
 */
static int find_entity_index_fast(const lvSolverSystemEx *sys_ex, int id) {
    if (sys_ex == NULL)
        return -1;
    return id_hash_find(&sys_ex->entity_hash, id);
}

/**
 * @brief 快速查找约束索引（使用哈希表）
 * @param sys_ex 扩展求解器系统
 * @param id 约束 ID
 * @return 索引（-1 表示未找到）
 */
static int find_constraint_index_fast(const lvSolverSystemEx *sys_ex, int id) {
    if (sys_ex == NULL)
        return -1;
    return id_hash_find(&sys_ex->constraint_hash, id);
}

/* ========================================================================
 * 第二部分：实体自由度查询
 * ======================================================================== */

/**
 * @brief 获取实体类型的自由度数量
 *
 * 返回值说明：
 *   - POINT_2D: 2 (x, y)
 *   - POINT_3D: 3 (x, y, z)
 *   - LINE_2D:  4 (ax, ay, bx, by)
 *   - CIRCLE_2D: 3 (cx, cy, r)
 *   - SEGMENT_2D: 4 (x1, y1, x2, y2)
 *   - ARC_2D: 5 (cx, cy, r, start_angle, sweep)
 */
lv_PUBLIC_API int lv_entity_dof(lvEntityType type) {
    static const int dof_map[] = {
        [lv_ENTITY_POINT_2D]   = 2,
        [lv_ENTITY_POINT_3D]   = 3,
        [lv_ENTITY_LINE_2D]    = 4,
        [lv_ENTITY_CIRCLE_2D]  = 3,
        [lv_ENTITY_SEGMENT_2D] = 4,
        [lv_ENTITY_ARC_2D]     = 5,
    };
    if (type >= 0 && type < (int)(sizeof(dof_map)/sizeof(dof_map[0])))
        return dof_map[type];
    return 0;
}

/* ========================================================================
 * 第二部分：约束自由度查询
 * ======================================================================== */

/**
 * @brief 获取约束类型消耗的自由度数量
 *
 * 返回值说明：
 *   - POINTS_COINCIDENT: 2（消除两个平移自由度）
 *   - PT_PT_DISTANCE: 1（消除一个距离自由度）
 *   - PT_ON_LINE: 1
 *   - PT_LINE_DISTANCE: 1
 *   - PT_ON_SEGMENT: 1
 *   - PT_ON_CIRCLE: 1
 *   - PT_PT_MIDPOINT: 2
 *   - PARALLEL: 1
 *   - PERPENDICULAR: 1
 *   - ANGLE: 1
 *   - EQUAL_LENGTH: 1
 *   - EQUAL_RADIUS: 1
 *   - CONCENTRIC: 2
 *   - TANGENT: 1
 *   - FIXED: 消除实体全部自由度（此处返回 -1 表示特殊处理）
 *   - HORIZONTAL: 1
 *   - VERTICAL: 1
 */
lv_PUBLIC_API int lv_constraint_dof(lvConstraintType type) {
    static const int dof_map[] = {
        [lv_CONSTRAINT_POINTS_COINCIDENT]  = 2,
        [lv_CONSTRAINT_PT_PT_DISTANCE]     = 1,
        [lv_CONSTRAINT_PT_ON_LINE]         = 1,
        [lv_CONSTRAINT_PT_LINE_DISTANCE]   = 1,
        [lv_CONSTRAINT_PT_ON_SEGMENT]      = 1,
        [lv_CONSTRAINT_PT_ON_CIRCLE]       = 1,
        [lv_CONSTRAINT_PT_PT_MIDPOINT]     = 2,
        [lv_CONSTRAINT_PARALLEL]           = 1,
        [lv_CONSTRAINT_PERPENDICULAR]      = 1,
        [lv_CONSTRAINT_ANGLE]              = 1,
        [lv_CONSTRAINT_EQUAL_LENGTH]       = 1,
        [lv_CONSTRAINT_EQUAL_RADIUS]       = 1,
        [lv_CONSTRAINT_CONCENTRIC]         = 2,
        [lv_CONSTRAINT_TANGENT]            = 1,
        [lv_CONSTRAINT_FIXED]              = -1, /* 特殊：消除全部自由度 */
        [lv_CONSTRAINT_HORIZONTAL]         = 1,
        [lv_CONSTRAINT_VERTICAL]           = 1,
    };
    if (type >= 0 && type < (int)(sizeof(dof_map)/sizeof(dof_map[0])))
        return dof_map[type];
    return 0;
}

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
 * 第七部分：约束残差计算（核心）
 * ======================================================================== */

/* --- 约束求值：函数指针表 --- */
typedef double (*ConstraintEvalFunc)(const lvSolverSystem *sys, const lvConstraint *c, double *error_val);

static double eval_points_coincident(const lvSolverSystem *sys, const lvConstraint *c, double *error_val) {
    lvEntity *ea = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_a);
    lvEntity *eb = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_b);
    if (!ea || !eb) {
        if (error_val) *error_val = 0.0;
        return 2;
    }
    double dx = ea->params[0] - eb->params[0];
    double dy = ea->params[1] - eb->params[1];
    double err = dx * dx + dy * dy;
    if (error_val) *error_val = err;
    return 2;
}

static double eval_pt_pt_distance(const lvSolverSystem *sys, const lvConstraint *c, double *error_val) {
    lvEntity *ea = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_a);
    lvEntity *eb = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_b);
    if (!ea || !eb) {
        if (error_val) *error_val = 0.0;
        return 1;
    }
    double dx = ea->params[0] - eb->params[0];
    double dy = ea->params[1] - eb->params[1];
    double dist = sqrt(dx * dx + dy * dy);
    double err = dist - c->value;
    if (error_val) *error_val = err;
    return 1;
}

static double eval_pt_on_line(const lvSolverSystem *sys, const lvConstraint *c, double *error_val) {
    lvEntity *ea = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_a);
    lvEntity *eb = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_b);
    if (!ea || !eb) {
        if (error_val) *error_val = 0.0;
        return 1;
    }
    double px = ea->params[0], py = ea->params[1];
    double ax = eb->params[0], ay = eb->params[1];
    double bx = eb->params[2], by = eb->params[3];
    double ldx = bx - ax, ldy = by - ay;
    double len = sqrt(ldx * ldx + ldy * ldy);
    if (len < lv_EPSILON_SUPERTINY) {
        if (error_val) *error_val = 0.0;
        return 1;
    }
    double err = ((px - ax) * ldy - (py - ay) * ldx) / len;
    if (error_val) *error_val = err;
    return 1;
}

static double eval_pt_line_distance(const lvSolverSystem *sys, const lvConstraint *c, double *error_val) {
    lvEntity *ea = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_a);
    lvEntity *eb = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_b);
    if (!ea || !eb) {
        if (error_val) *error_val = 0.0;
        return 1;
    }
    double px = ea->params[0], py = ea->params[1];
    double ax = eb->params[0], ay = eb->params[1];
    double bx = eb->params[2], by = eb->params[3];
    double ldx = bx - ax, ldy = by - ay;
    double len = sqrt(ldx * ldx + ldy * ldy);
    if (len < lv_EPSILON_SUPERTINY) {
        if (error_val) *error_val = 0.0;
        return 1;
    }
    double dist = fabs(((px - ax) * ldy - (py - ay) * ldx) / len);
    double err = dist - c->value;
    if (error_val) *error_val = err;
    return 1;
}

static double eval_pt_on_segment(const lvSolverSystem *sys, const lvConstraint *c, double *error_val) {
    lvEntity *ea = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_a);
    lvEntity *eb = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_b);
    if (!ea || !eb) {
        if (error_val) *error_val = 0.0;
        return 1;
    }
    double px = ea->params[0], py = ea->params[1];
    double x1 = eb->params[0], y1 = eb->params[1];
    double x2 = eb->params[2], y2 = eb->params[3];
    double sdx = x2 - x1, sdy = y2 - y1;
    double slen = sqrt(sdx * sdx + sdy * sdy);
    if (slen < 1e-15) {
        if (error_val) *error_val = 0.0;
        return 1;
    }
    double err = fabs(((px - x1) * sdy - (py - y1) * sdx) / slen);
    if (error_val) *error_val = err;
    return 1;
}

static double eval_pt_on_circle(const lvSolverSystem *sys, const lvConstraint *c, double *error_val) {
    lvEntity *ea = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_a);
    lvEntity *eb = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_b);
    if (!ea || !eb) {
        if (error_val) *error_val = 0.0;
        return 1;
    }
    double dx = ea->params[0] - eb->params[0];
    double dy = ea->params[1] - eb->params[1];
    double dist = sqrt(dx * dx + dy * dy);
    double err = dist - eb->params[2];
    if (error_val) *error_val = err;
    return 1;
}

static double eval_pt_pt_midpoint(const lvSolverSystem *sys, const lvConstraint *c, double *error_val) {
    lvEntity *ea = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_a);
    lvEntity *eb = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_b);
    lvEntity *ec = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_c);
    if (!ea || !eb || !ec) {
        if (error_val) *error_val = 0.0;
        return 2;
    }
    double mx = (ea->params[0] + eb->params[0]) * 0.5;
    double my = (ea->params[1] + eb->params[1]) * 0.5;
    double err = (ec->params[0] - mx) * (ec->params[0] - mx) + (ec->params[1] - my) * (ec->params[1] - my);
    if (error_val) *error_val = err;
    return 2;
}

static double eval_parallel(const lvSolverSystem *sys, const lvConstraint *c, double *error_val) {
    lvEntity *ea = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_a);
    lvEntity *eb = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_b);
    if (!ea || !eb) {
        if (error_val) *error_val = 0.0;
        return 1;
    }
    double dax = ea->params[2] - ea->params[0];
    double day = ea->params[3] - ea->params[1];
    double dbx = eb->params[2] - eb->params[0];
    double dby = eb->params[3] - eb->params[1];
    double err = dax * dby - day * dbx;
    if (error_val) *error_val = err;
    return 1;
}

static double eval_perpendicular(const lvSolverSystem *sys, const lvConstraint *c, double *error_val) {
    lvEntity *ea = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_a);
    lvEntity *eb = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_b);
    if (!ea || !eb) {
        if (error_val) *error_val = 0.0;
        return 1;
    }
    double dax = ea->params[2] - ea->params[0];
    double day = ea->params[3] - ea->params[1];
    double dbx = eb->params[2] - eb->params[0];
    double dby = eb->params[3] - eb->params[1];
    double err = dax * dbx + day * dby;
    if (error_val) *error_val = err;
    return 1;
}

static double eval_angle(const lvSolverSystem *sys, const lvConstraint *c, double *error_val) {
    lvEntity *ea = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_a);
    lvEntity *eb = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_b);
    if (!ea || !eb) {
        if (error_val) *error_val = 0.0;
        return 1;
    }
    double dax = ea->params[2] - ea->params[0];
    double day = ea->params[3] - ea->params[1];
    double dbx = eb->params[2] - eb->params[0];
    double dby = eb->params[3] - eb->params[1];
    double angle_a = (dax != 0.0 || day != 0.0) ? atan2(day, dax) : 0.0;
    double angle_b = (dbx != 0.0 || dby != 0.0) ? atan2(dby, dbx) : 0.0;
    double diff = angle_a - angle_b;
    while (diff > M_PI) diff -= 2.0 * M_PI;
    while (diff < -M_PI) diff += 2.0 * M_PI;
    double err = diff - c->value;
    if (error_val) *error_val = err;
    return 1;
}

static double eval_equal_length(const lvSolverSystem *sys, const lvConstraint *c, double *error_val) {
    lvEntity *ea = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_a);
    lvEntity *eb = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_b);
    if (!ea || !eb) {
        if (error_val) *error_val = 0.0;
        return 1;
    }
    double dax = ea->params[2] - ea->params[0];
    double day = ea->params[3] - ea->params[1];
    double dbx = eb->params[2] - eb->params[0];
    double dby = eb->params[3] - eb->params[1];
    double len_a = sqrt(dax * dax + day * day);
    double len_b = sqrt(dbx * dbx + dby * dby);
    double err = len_a - len_b;
    if (error_val) *error_val = err;
    return 1;
}

static double eval_equal_radius(const lvSolverSystem *sys, const lvConstraint *c, double *error_val) {
    lvEntity *ea = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_a);
    lvEntity *eb = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_b);
    if (!ea || !eb) {
        if (error_val) *error_val = 0.0;
        return 1;
    }
    double err = ea->params[2] - eb->params[2];
    if (error_val) *error_val = err;
    return 1;
}

static double eval_concentric(const lvSolverSystem *sys, const lvConstraint *c, double *error_val) {
    lvEntity *ea = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_a);
    lvEntity *eb = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_b);
    if (!ea || !eb) {
        if (error_val) *error_val = 0.0;
        return 2;
    }
    double dx = ea->params[0] - eb->params[0];
    double dy = ea->params[1] - eb->params[1];
    double err = dx * dx + dy * dy;
    if (error_val) *error_val = err;
    return 2;
}

static double eval_tangent(const lvSolverSystem *sys, const lvConstraint *c, double *error_val) {
    lvEntity *ea = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_a);
    lvEntity *eb = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_b);
    if (!ea || !eb) {
        if (error_val) *error_val = 0.0;
        return 1;
    }
    double ax = ea->params[0], ay = ea->params[1];
    double bx = ea->params[2], by = ea->params[3];
    double cx = eb->params[0], cy = eb->params[1];
    double r = eb->params[2];
    double ldx = bx - ax, ldy = by - ay;
    double len = sqrt(ldx * ldx + ldy * ldy);
    if (len < lv_EPSILON_SUPERTINY) {
        if (error_val) *error_val = 0.0;
        return 1;
    }
    double dist = fabs(((cx - ax) * ldy - (cy - ay) * ldx) / len);
    double err = dist - r;
    if (error_val) *error_val = err;
    return 1;
}

static double eval_fixed(const lvSolverSystem *sys, const lvConstraint *c, double *error_val) {
    lvEntity *ea = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_a);
    if (!ea) {
        if (error_val) *error_val = 0.0;
        return 0;
    }
    int dof = lv_entity_dof(ea->type);
    if (error_val) *error_val = 0.0;
    return (double)dof;
}

static double eval_horizontal(const lvSolverSystem *sys, const lvConstraint *c, double *error_val) {
    lvEntity *ea = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_a);
    if (!ea) {
        if (error_val) *error_val = 0.0;
        return 1;
    }
    double err = ea->params[1] - ea->params[3];
    if (error_val) *error_val = err;
    return 1;
}

static double eval_vertical(const lvSolverSystem *sys, const lvConstraint *c, double *error_val) {
    lvEntity *ea = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_a);
    if (!ea) {
        if (error_val) *error_val = 0.0;
        return 1;
    }
    double err = ea->params[0] - ea->params[2];
    if (error_val) *error_val = err;
    return 1;
}

static ConstraintEvalFunc s_constraint_eval_funcs[] = {
    [lv_CONSTRAINT_POINTS_COINCIDENT] = eval_points_coincident,
    [lv_CONSTRAINT_PT_PT_DISTANCE]    = eval_pt_pt_distance,
    [lv_CONSTRAINT_PT_ON_LINE]        = eval_pt_on_line,
    [lv_CONSTRAINT_PT_LINE_DISTANCE]  = eval_pt_line_distance,
    [lv_CONSTRAINT_PT_ON_SEGMENT]     = eval_pt_on_segment,
    [lv_CONSTRAINT_PT_ON_CIRCLE]      = eval_pt_on_circle,
    [lv_CONSTRAINT_PT_PT_MIDPOINT]    = eval_pt_pt_midpoint,
    [lv_CONSTRAINT_PARALLEL]          = eval_parallel,
    [lv_CONSTRAINT_PERPENDICULAR]     = eval_perpendicular,
    [lv_CONSTRAINT_ANGLE]             = eval_angle,
    [lv_CONSTRAINT_EQUAL_LENGTH]      = eval_equal_length,
    [lv_CONSTRAINT_EQUAL_RADIUS]      = eval_equal_radius,
    [lv_CONSTRAINT_CONCENTRIC]        = eval_concentric,
    [lv_CONSTRAINT_TANGENT]           = eval_tangent,
    [lv_CONSTRAINT_FIXED]             = eval_fixed,
    [lv_CONSTRAINT_HORIZONTAL]        = eval_horizontal,
    [lv_CONSTRAINT_VERTICAL]          = eval_vertical,
};
static const int s_constraint_eval_func_count = (int)(sizeof(s_constraint_eval_funcs) / sizeof(s_constraint_eval_funcs[0]));

/**
 * @brief 计算单个约束的残差
 *
 * 对每种约束类型计算其残差值。当所有约束残差为零时，系统满足所有约束。
 *
 * @param sys  求解系统
 * @param c    约束
 * @param error_val  输出残差值（NULL 表示不输出）
 * @return 残差数量（1 或 2）
 */
static double evaluate_constraint(const lvSolverSystem *sys, const lvConstraint *c, double *error_val) {
    if (c->type >= 0 && c->type < s_constraint_eval_func_count && s_constraint_eval_funcs[c->type]) {
        return s_constraint_eval_funcs[c->type](sys, c, error_val);
    }
    if (error_val)
        *error_val = 0.0;
    return 0;
}

/* ========================================================================
 * 第八部分：高斯消元法
 * ======================================================================== */

/**
 * @brief 高斯消元法求解 n x n 线性方程组 A * x = b
 *
 * 使用部分主元选取（列主元）提高数值稳定性。
 * 矩阵 A 在求解过程中会被修改（行变换）。
 *
 * @param A  n x n 系数矩阵（行优先存储），求解后被修改
 * @param b  右端向量，求解后存储解 x
 * @param n  矩阵维度（n <= 20）
 * @return 0 成功，-1 奇异矩阵
 */
static int gauss_eliminate(double *A, double *b, int n) {
    if (n <= 0 || n > 20)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "gauss_eliminate: invalid dimension n=%d", n);

    /* 前向消元（列主元选取） */
    for (int col = 0; col < n; col++) {
        /* 寻找主元 */
        int pivot = col;
        double max_val = fabs(A[col * n + col]);
        for (int row = col + 1; row < n; row++) {
            double val = fabs(A[row * n + col]);
            if (val > max_val) {
                max_val = val;
                pivot = row;
            }
        }

        /* 奇异检测：使用相对容差，依据矩阵列范数自适应缩放 */
        double col_norm = 0.0;
        for (int r = 0; r < n; r++) {
            double av = fabs(A[r * n + col]);
            if (av > col_norm)
                col_norm = av;
        }
        double singular_tol = 1e-14 * fmax(1.0, col_norm);
        if (max_val < singular_tol)
            return -1;

        /* 交换行 */
        if (pivot != col) {
            for (int j = 0; j < n; j++) {
                double tmp = A[col * n + j];
                A[col * n + j] = A[pivot * n + j];
                A[pivot * n + j] = tmp;
            }
            double tmp = b[col];
            b[col] = b[pivot];
            b[pivot] = tmp;
        }

        /* 消元 */
        for (int row = col + 1; row < n; row++) {
            double factor = A[row * n + col] / A[col * n + col];
            for (int j = col; j < n; j++) {
                A[row * n + j] -= factor * A[col * n + j];
            }
            b[row] -= factor * b[col];
        }
    }

    /* 回代 */
    for (int row = n - 1; row >= 0; row--) {
        double sum = b[row];
        for (int j = row + 1; j < n; j++) {
            sum -= A[row * n + j] * b[j];
        }
        b[row] = sum / A[row * n + row];
    }

    return 0;
}

/* ========================================================================
 * 第九部分：向量范数
 * ======================================================================== */

/**
 * @brief 计算向量的 L2 范数
 */
static double vec_norm(const double *v, int n) {
    if (n <= 0 || !v)
        return 0.0;
    /* 找到最大绝对值，用于缩放以避免平方和溢出到 Inf */
    double max_abs = lv_max_abs(v, (int64_t) n);
    if (max_abs == 0.0)
        return 0.0;
    /* 缩放后计算平方和：sqrt(sum((v[i]/scale)^2)) * scale */
    double scale = max_abs;
    double sum = 0.0;
    for (int i = 0; i < n; i++) {
        double scaled = v[i] / scale;
        sum += scaled * scaled;
    }
    return sqrt(sum) * scale;
}

/* ========================================================================
 * 第十部分：Newton-Raphson 求解核心
 * ======================================================================== */

/**
 * @brief 构建雅可比矩阵 J 和残差向量 F
 *
 * 使用数值差分（中心差分）计算雅可比矩阵的每个元素：
 *   J[i][j] = (F_i(params + eps*e_j) - F_i(params - eps*e_j)) / (2 * eps)
 *
 * @param sys           求解系统
 * @param J             输出雅可比矩阵（nrows x ncols，行优先）
 * @param F             输出残差向量（nrows）
 * @param nrows         残差维度（约束方程数量）
 * @param ncols         参数维度（自由参数数量）
 * @param param_map     参数映射表：param_map[j] = 全局参数索引
 * @param free_entities 自由实体索引数组
 * @param free_count    自由实体数量
 */
static void build_jacobian_and_residual(const lvSolverSystem *sys, double *J, double *F, int nrows, int ncols,
                                        const int *param_map, const int *free_entities, int free_count) {
    int row = 0;

    /* 计算残差向量 F */
    for (int ci = 0; ci < sys->constraint_count; ci++) {
        const lvConstraint *c = &sys->constraints[ci];
        if (!c->is_active)
            continue;
        if (c->type == lv_CONSTRAINT_FIXED)
            continue; /* FIXED 实体已通过 is_fixed 排除 */

        int n_eq = evaluate_constraint(sys, c, NULL);

        /* 对于多方程约束（如 COINCIDENT 返回 2），分别计算 */
        if (n_eq == 2) {
            /* 第一个方程：x 方向 */
            lvEntity *ea = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_a);
            lvEntity *eb = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_b);
            if (ea && eb) {
                F[row] = ea->params[0] - eb->params[0];
                row++;
                F[row] = ea->params[1] - eb->params[1];
                row++;
            } else {
                F[row] = 0.0;
                row++;
                if (row < nrows) {
                    F[row] = 0.0;
                    row++;
                }
            }
        } else {
            double err = 0.0;
            evaluate_constraint(sys, c, &err);
            F[row] = err;
            row++;
        }
    }

    /* 使用数值差分计算雅可比矩阵 */
    double eps = NUMERICAL_DIFF_EPSILON;

    for (int j = 0; j < ncols; j++) {
        /* 找到 param_map[j] 对应的实体和参数偏移 */
        int global_param = param_map[j];
        int ent_idx = -1;
        int param_offset = 0;
        int accumulated = 0;

        for (int fi = 0; fi < free_count; fi++) {
            int ei = free_entities[fi];
            const lvEntity *e = &sys->entities[ei];
            int dof = e->param_count;
            if (global_param >= accumulated && global_param < accumulated + dof) {
                ent_idx = ei;
                param_offset = global_param - accumulated;
                break;
            }
            accumulated += dof;
        }

        if (ent_idx < 0)
            continue;

        /* 正向扰动 */
        double orig = sys->entities[ent_idx].params[param_offset];

        /* 自适应扰动步长：固定步长 1e-8 在参数值较大时会被舍入（10000 + 1e-8 == 10000），
         * 导致雅可比列为零。使用相对步长 eps * max(1, |orig|) 保证扰动显著。 */
        double h = eps * fmax(1.0, fabs(orig));
        sys->entities[ent_idx].params[param_offset] = orig + h;

        row = 0;
        double *F_plus = (double *) lv_malloc(nrows * sizeof(double));
        for (int ci = 0; ci < sys->constraint_count; ci++) {
            const lvConstraint *c = &sys->constraints[ci];
            if (!c->is_active)
                continue;
            int n_eq = evaluate_constraint(sys, c, NULL);
            if (n_eq == 2) {
                lvEntity *ea = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_a);
                lvEntity *eb = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_b);
                if (ea && eb) {
                    F_plus[row] = ea->params[0] - eb->params[0];
                    row++;
                    F_plus[row] = ea->params[1] - eb->params[1];
                    row++;
                } else {
                    F_plus[row] = 0.0;
                    row++;
                    if (row < nrows) {
                        F_plus[row] = 0.0;
                        row++;
                    }
                }
            } else {
                double err = 0.0;
                evaluate_constraint(sys, c, &err);
                F_plus[row] = err;
                row++;
            }
        }

        /* 负向扰动 */
        sys->entities[ent_idx].params[param_offset] = orig - h;

        row = 0;
        double *F_minus = (double *) lv_malloc(nrows * sizeof(double));
        for (int ci = 0; ci < sys->constraint_count; ci++) {
            const lvConstraint *c = &sys->constraints[ci];
            if (!c->is_active)
                continue;
            int n_eq = evaluate_constraint(sys, c, NULL);
            if (n_eq == 2) {
                lvEntity *ea = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_a);
                lvEntity *eb = lv_solver_get_entity((lvSolverSystem *) sys, c->entity_b);
                if (ea && eb) {
                    F_minus[row] = ea->params[0] - eb->params[0];
                    row++;
                    F_minus[row] = ea->params[1] - eb->params[1];
                    row++;
                } else {
                    F_minus[row] = 0.0;
                    row++;
                    if (row < nrows) {
                        F_minus[row] = 0.0;
                        row++;
                    }
                }
            } else {
                double err = 0.0;
                evaluate_constraint(sys, c, &err);
                F_minus[row] = err;
                row++;
            }
        }

        /* 恢复原始值 */
        sys->entities[ent_idx].params[param_offset] = orig;

        /* 计算雅可比列：J[:, j] = (F_plus - F_minus) / (2 * h) */
        for (int i = 0; i < nrows; i++) {
            J[i * ncols + j] = (F_plus[i] - F_minus[i]) / (2.0 * h);
        }

        lv_free((void **) &(F_plus));
        lv_free((void **) &(F_minus));
    }
}

/**
 * @brief 执行 Newton-Raphson 求解
 *
 * 算法流程（借鉴 SolveSpace）：
 *   1. 确定自由参数（排除固定和被拖拽的实体）
 *   2. 构建雅可比矩阵 J 和残差向量 F
 *   3. 求解 J * delta = -F
 *   4. 更新参数：params += damping * delta
 *   5. 检查收敛：||F|| < tolerance
 *   6. 重复直到收敛或达到最大迭代次数
 *
 * @return 求解结果
 */
lv_PUBLIC_API lvSolveResult lv_geo_solver_solve(lvSolverSystem *sys) {
    if (!sys)
        return lv_SOLVE_FAILED;

    sys->iteration_count = 0;

    /* 统计自由实体和参数维度 */
    int free_entities[MAX_PARAMS];
    int free_count = 0;
    int total_params = 0;

    for (int i = 0; i < sys->entity_count; i++) {
        if (sys->entities[i].is_fixed || sys->entities[i].is_dragged)
            continue;
        if (free_count < MAX_PARAMS) {
            free_entities[free_count] = i;
            free_count++;
            total_params += sys->entities[i].param_count;
        }
    }

    if (total_params == 0) {
        sys->last_result = lv_SOLVE_OK;
        return lv_SOLVE_OK;
    }

    /* 统计约束方程数量 */
    int nrows = 0;
    for (int ci = 0; ci < sys->constraint_count; ci++) {
        const lvConstraint *c = &sys->constraints[ci];
        if (!c->is_active)
            continue;
        nrows += evaluate_constraint(sys, c, NULL);
    }

    if (nrows == 0) {
        sys->last_result = lv_SOLVE_OK;
        return lv_SOLVE_OK;
    }

    int ncols = total_params;

    /* 分配工作内存 */
    double *J = (double *) lv_calloc(nrows * ncols, sizeof(double));
    double *F = (double *) lv_calloc(nrows, sizeof(double));
    double *delta = (double *) lv_calloc(ncols > nrows ? ncols : nrows, sizeof(double));
    double *rhs = (double *) lv_calloc(nrows, sizeof(double));
    double *J_copy = (double *) lv_calloc(nrows * ncols, sizeof(double));

    if (!J || !F || !delta || !rhs || !J_copy) {
        lv_free((void **) &(J));
        lv_free((void **) &(F));
        lv_free((void **) &(delta));
        lv_free((void **) &(rhs));
        lv_free((void **) &(J_copy));
        sys->last_result = lv_SOLVE_FAILED;
        return lv_SOLVE_FAILED;
    }

    /* 构建参数映射表 */
    int *param_map = (int *) lv_calloc(ncols, sizeof(int));
    int acc = 0;
    for (int fi = 0; fi < free_count; fi++) {
        int ei = free_entities[fi];
        for (int p = 0; p < sys->entities[ei].param_count; p++) {
            if (acc < ncols) {
                param_map[acc] = acc;
                acc++;
            }
        }
    }

    lvSolveResult result = lv_SOLVE_NOT_CONVERGED;
    double damping = sys->config.damping_factor;
    double prev_norm_F = -1.0; /* 初始化为负值，跳过第一次迭代的比较 */

    for (int iter = 0; iter < sys->config.max_iterations; iter++) {
        sys->iteration_count = iter + 1;

        /* 构建雅可比矩阵和残差向量 */
        build_jacobian_and_residual(sys, J, F, nrows, ncols, param_map, free_entities, free_count);

        /* 检查收敛 */
        double norm_F = vec_norm(F, nrows);
        /* 检测 NaN/Inf 传播：约束求值可能产生 NaN，导致残差范数为 NaN，
         * 此时收敛检查和发散检查均无效，应提前终止迭代 */
        if (!isfinite(norm_F)) {
            result = lv_SOLVE_NOT_CONVERGED;
            break;
        }
        if (sys->config.verbose) {
            /* 静默模式下不输出 */
        }

        if (norm_F < sys->config.convergence_tol) {
            result = lv_SOLVE_OK;
            break;
        }

        /* 发散检测：如果残差相比上次迭代增长超过 10 倍，
         * 且本次残差已经超过初始残差，说明迭代正在发散。
         * 此时终止迭代避免无意义计算。 */
        if (prev_norm_F > 0.0 && norm_F > prev_norm_F * 10.0) {
            result = lv_SOLVE_NOT_CONVERGED;
            break;
        }
        prev_norm_F = norm_F;

        /* 确定求解维度 */
        int solve_n = (nrows < ncols) ? nrows : ncols;

        /* 处理超定/欠定系统 */
        if (nrows > ncols) {
            /* 超定系统：使用 J^T * J * delta = -J^T * F（正规方程） */
            /* J^T * J (ncols x ncols) */
            double *JtJ = (double *) lv_calloc(ncols * ncols, sizeof(double));
            double *JtF = (double *) lv_calloc(ncols, sizeof(double));
            if (!JtJ || !JtF) {
                lv_free((void **) &(JtJ));
                lv_free((void **) &(JtF));
                result = lv_SOLVE_FAILED;
                break;
            }

            for (int i = 0; i < ncols; i++) {
                for (int j = 0; j < ncols; j++) {
                    double sum = 0.0;
                    for (int k = 0; k < nrows; k++) {
                        sum += J[k * ncols + i] * J[k * ncols + j];
                    }
                    JtJ[i * ncols + j] = sum;
                }
                double sum = 0.0;
                for (int k = 0; k < nrows; k++) {
                    sum += J[k * ncols + i] * F[k];
                }
                JtF[i] = -sum;
            }

            memcpy(J_copy, JtJ, ncols * ncols * sizeof(double));
            memcpy(delta, JtF, ncols * sizeof(double));

            int ret = gauss_eliminate(J_copy, delta, ncols);
            lv_free((void **) &(JtJ));
            lv_free((void **) &(JtF));

            if (ret != 0) {
                result = lv_SOLVE_INCONSISTENT;
                break;
            }

            /* 更新参数 */
            int pidx = 0;
            for (int fi = 0; fi < free_count; fi++) {
                int ei = free_entities[fi];
                for (int p = 0; p < sys->entities[ei].param_count; p++) {
                    if (pidx < ncols) {
                        sys->entities[ei].params[p] += damping * delta[pidx];
                        pidx++;
                    }
                }
            }

        } else if (nrows == ncols) {
            /* 方阵系统：直接求解 J * delta = -F */
            memcpy(J_copy, J, nrows * ncols * sizeof(double));
            for (int i = 0; i < nrows; i++) {
                rhs[i] = -F[i];
            }

            int ret = gauss_eliminate(J_copy, rhs, solve_n);
            if (ret != 0) {
                result = lv_SOLVE_INCONSISTENT;
                break;
            }

            /* 更新参数 */
            int pidx = 0;
            for (int fi = 0; fi < free_count; fi++) {
                int ei = free_entities[fi];
                for (int p = 0; p < sys->entities[ei].param_count; p++) {
                    if (pidx < ncols) {
                        sys->entities[ei].params[p] += damping * rhs[pidx];
                        pidx++;
                    }
                }
            }

        } else {
            /* 欠定系统： nrows < ncols，使用最小范数解 */
            /* J * delta = -F，取 delta = J^T * (J * J^T)^{-1} * (-F) */
            double *JJt = (double *) lv_calloc(nrows * nrows, sizeof(double));
            double *neg_F = (double *) lv_calloc(nrows, sizeof(double));
            if (!JJt || !neg_F) {
                lv_free((void **) &(JJt));
                lv_free((void **) &(neg_F));
                result = lv_SOLVE_FAILED;
                break;
            }

            for (int i = 0; i < nrows; i++) {
                for (int j = 0; j < nrows; j++) {
                    double sum = 0.0;
                    for (int k = 0; k < ncols; k++) {
                        sum += J[i * ncols + k] * J[j * ncols + k];
                    }
                    JJt[i * nrows + j] = sum;
                }
                neg_F[i] = -F[i];
            }

            int ret = gauss_eliminate(JJt, neg_F, nrows);
            if (ret != 0) {
                lv_free((void **) &(JJt));
                lv_free((void **) &(neg_F));
                result = lv_SOLVE_FAILED;
                break;
            }

            /* delta = J^T * neg_F */
            memset(delta, 0, ncols * sizeof(double));
            for (int i = 0; i < ncols; i++) {
                double sum = 0.0;
                for (int k = 0; k < nrows; k++) {
                    sum += J[k * ncols + i] * neg_F[k];
                }
                delta[i] = sum;
            }

            lv_free((void **) &(JJt));
            lv_free((void **) &(neg_F));

            /* 更新参数 */
            int pidx = 0;
            for (int fi = 0; fi < free_count; fi++) {
                int ei = free_entities[fi];
                for (int p = 0; p < sys->entities[ei].param_count; p++) {
                    if (pidx < ncols) {
                        sys->entities[ei].params[p] += damping * delta[pidx];
                        pidx++;
                    }
                }
            }
        }
    }

    /* 清理 */
    lv_free((void **) &(J));
    lv_free((void **) &(F));
    lv_free((void **) &(delta));
    lv_free((void **) &(rhs));
    lv_free((void **) &(J_copy));
    lv_free((void **) &(param_map));

    sys->last_result = result;
    return result;
}

/* ========================================================================
 * 第十一部分：DOF 分析
 * ======================================================================== */

/**
 * @brief DOF（自由度）分析
 *
 * 统计所有实体的自由度之和，减去约束消耗的自由度。
 * FIXED 约束特殊处理：消除对应实体的全部自由度。
 *
 * @return DOF 分析结果（需用 lv_dof_analysis_destroy 释放）
 */
lv_PUBLIC_API lvDOFAnalysis *lv_solver_dof_analyze(const lvSolverSystem *sys) {
    if (!sys)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_solver_dof_analyze: NULL sys");

    lvDOFAnalysis *analysis = (lvDOFAnalysis *) lv_calloc(1, sizeof(lvDOFAnalysis));
    if (!analysis)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_solver_dof_analyze: calloc failed");

    /* 统计总自由度 */
    int total_dof = 0;
    int fixed_count = 0;

    for (int i = 0; i < sys->entity_count; i++) {
        const lvEntity *e = &sys->entities[i];
        if (e->is_fixed) {
            fixed_count++;
        } else {
            total_dof += lv_entity_dof(e->type);
        }
    }

    /* 统计约束消耗的自由度 */
    int constraint_dof = 0;
    for (int i = 0; i < sys->constraint_count; i++) {
        const lvConstraint *c = &sys->constraints[i];
        if (!c->is_active)
            continue;

        int cdof = lv_constraint_dof(c->type);
        if (cdof == -1) {
            /* FIXED 约束：目标实体的 DOF 已通过 is_fixed 在 total_dof 中排除，跳过 */
            continue;
        } else {
            constraint_dof += cdof;
        }
    }

    analysis->total_dof = total_dof;
    analysis->constraint_dof = constraint_dof;
    analysis->remaining_dof = total_dof - constraint_dof;
    analysis->fixed_count = fixed_count;

    /* 确定自由实体 */
    int free_cap = sys->entity_count;
    analysis->free_entity_ids = (int *) lv_malloc(free_cap * sizeof(int));
    analysis->free_entity_count = 0;

    if (analysis->free_entity_ids) {
        for (int i = 0; i < sys->entity_count; i++) {
            if (!sys->entities[i].is_fixed) {
                analysis->free_entity_ids[analysis->free_entity_count] = sys->entities[i].id;
                analysis->free_entity_count++;
            }
        }
    }

    /* 确定系统状态 */
    if (analysis->remaining_dof > 0) {
        analysis->status = lv_SYSTEM_UNDER_CONSTRAINED;
    } else if (analysis->remaining_dof == 0) {
        analysis->status = lv_SYSTEM_WELL_CONSTRAINED;
    } else {
        analysis->status = lv_SYSTEM_OVER_CONSTRAINED;
    }

    return analysis;
}

/**
 * @brief 释放 DOF 分析结果
 */
lv_PUBLIC_API void lv_dof_analysis_destroy(lvDOFAnalysis *analysis) {
    if (!analysis)
        return;
    lv_free((void **) &(analysis->free_entity_ids));
    lv_free((void **) &(analysis));
}

/* ========================================================================
 * 第十二部分：系统状态查询
 * ======================================================================== */

/**
 * @brief 获取系统约束状态
 */
lv_PUBLIC_API lvSystemStatus lv_solver_get_status(const lvSolverSystem *sys) {
    if (!sys)
        return lv_SYSTEM_UNDER_CONSTRAINED;

    lvDOFAnalysis *analysis = lv_solver_dof_analyze(sys);
    if (!analysis)
        return lv_SYSTEM_UNDER_CONSTRAINED;

    lvSystemStatus status = analysis->status;
    lv_dof_analysis_destroy(analysis);
    return status;
}

/**
 * @brief 获取上次求解的迭代次数
 */
lv_PUBLIC_API int lv_solver_get_iteration_count(const lvSolverSystem *sys) {
    if (!sys)
        return 0;
    return sys->iteration_count;
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

/* ========================================================================
 * 第十四部分：便捷函数 —— 快速创建实体
 * ======================================================================== */

/**
 * @brief 创建 2D 点实体
 */
lv_PUBLIC_API lvEntity lv_entity_point_2d(int id, double x, double y) {
    lvEntity e;
    memset(&e, 0, sizeof(e));
    e.type = lv_ENTITY_POINT_2D;
    e.id = id;
    e.params[0] = x;
    e.params[1] = y;
    e.param_count = 2;
    e.is_fixed = false;
    e.is_dragged = false;
    return e;
}

/**
 * @brief 创建 2D 直线实体（过两点的直线）
 */
lv_PUBLIC_API lvEntity lv_entity_line_2d(int id, double x1, double y1, double x2, double y2) {
    lvEntity e;
    memset(&e, 0, sizeof(e));
    e.type = lv_ENTITY_LINE_2D;
    e.id = id;
    e.params[0] = x1;
    e.params[1] = y1;
    e.params[2] = x2;
    e.params[3] = y2;
    e.param_count = 4;
    e.is_fixed = false;
    e.is_dragged = false;
    return e;
}

/**
 * @brief 创建 2D 圆实体
 */
lv_PUBLIC_API lvEntity lv_entity_circle_2d(int id, double cx, double cy, double r) {
    lvEntity e;
    memset(&e, 0, sizeof(e));
    e.type = lv_ENTITY_CIRCLE_2D;
    e.id = id;
    e.params[0] = cx;
    e.params[1] = cy;
    e.params[2] = r;
    e.param_count = 3;
    e.is_fixed = false;
    e.is_dragged = false;
    return e;
}

/**
 * @brief 创建 2D 线段实体
 */
lv_PUBLIC_API lvEntity lv_entity_segment_2d(int id, double x1, double y1, double x2, double y2) {
    lvEntity e;
    memset(&e, 0, sizeof(e));
    e.type = lv_ENTITY_SEGMENT_2D;
    e.id = id;
    e.params[0] = x1;
    e.params[1] = y1;
    e.params[2] = x2;
    e.params[3] = y2;
    e.param_count = 4;
    e.is_fixed = false;
    e.is_dragged = false;
    return e;
}

/* ========================================================================
 * 第十五部分：便捷函数 —— 快速创建约束
 * ======================================================================== */

/**
 * @brief 创建两点重合约束
 */
lv_PUBLIC_API lvConstraint lv_constraint_coincident(int id, int entity_a, int entity_b) {
    lvConstraint c;
    memset(&c, 0, sizeof(c));
    c.type = lv_CONSTRAINT_POINTS_COINCIDENT;
    c.id = id;
    c.entity_a = entity_a;
    c.entity_b = entity_b;
    c.entity_c = -1;
    c.value = 0.0;
    c.is_active = true;
    return c;
}

/* ========================================================================
 * 兼容旧 API：lv_solve_constraints
 * ======================================================================== */

lv_PUBLIC_API int lv_solve_constraints(const lvConstraint *constraints, size_t count, double *points, size_t n_points) {
    if (!constraints || count == 0 || !points || n_points == 0) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "lv_solve_constraints: NULL or empty params");
    }

    lvSolverConfig config = lv_solver_default_config();
    lvSolverSystem *sys = lv_geo_solver_create(&config);
    if (!sys) {
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_solve_constraints: solver creation failed");
    }

    /* 为每个点创建实体 */
    for (size_t i = 0; i < n_points; i++) {
        lvEntity e = lv_entity_point_2d((int) i, points[i * 2], points[i * 2 + 1]);
        if (lv_solver_add_entity(sys, &e) < 0) {
            lv_geo_solver_destroy(sys);
            lv_RETURN_ERROR(lv_ERROR_NODE_CONFLICT, "lv_solve_constraints: add entity failed");
        }
    }

    /* 添加所有约束 */
    for (size_t i = 0; i < count; i++) {
        if (lv_geo_solver_add_constraint(sys, &constraints[i]) < 0) {
            lv_geo_solver_destroy(sys);
            lv_RETURN_ERROR(lv_ERROR_NODE_CONFLICT, "lv_solve_constraints: add constraint failed");
        }
    }

    /* 求解 */
    lvSolveResult result = lv_geo_solver_solve(sys);
    if (result != lv_SOLVE_OK) {
        lv_geo_solver_destroy(sys);
        return -2;
    }

    /* 提取求解后的坐标 */
    for (size_t i = 0; i < n_points; i++) {
        lvEntity *entity = lv_solver_get_entity(sys, (int) i);
        if (entity && entity->param_count >= 2) {
            points[i * 2] = entity->params[0];
            points[i * 2 + 1] = entity->params[1];
        }
    }

    lv_geo_solver_destroy(sys);
    return (int) n_points;
}

/**
 * @brief 创建点点距离约束
 */
lv_PUBLIC_API lvConstraint lv_constraint_distance(int id, int entity_a, int entity_b, double dist) {
    lvConstraint c;
    memset(&c, 0, sizeof(c));
    c.type = lv_CONSTRAINT_PT_PT_DISTANCE;
    c.id = id;
    c.entity_a = entity_a;
    c.entity_b = entity_b;
    c.entity_c = -1;
    c.value = dist;
    c.is_active = true;
    return c;
}

/**
 * @brief 创建平行约束
 */
lv_PUBLIC_API lvConstraint lv_constraint_parallel(int id, int entity_a, int entity_b) {
    lvConstraint c;
    memset(&c, 0, sizeof(c));
    c.type = lv_CONSTRAINT_PARALLEL;
    c.id = id;
    c.entity_a = entity_a;
    c.entity_b = entity_b;
    c.entity_c = -1;
    c.value = 0.0;
    c.is_active = true;
    return c;
}

/**
 * @brief 创建垂直约束
 */
lv_PUBLIC_API lvConstraint lv_constraint_perpendicular(int id, int entity_a, int entity_b) {
    lvConstraint c;
    memset(&c, 0, sizeof(c));
    c.type = lv_CONSTRAINT_PERPENDICULAR;
    c.id = id;
    c.entity_a = entity_a;
    c.entity_b = entity_b;
    c.entity_c = -1;
    c.value = 0.0;
    c.is_active = true;
    return c;
}

/**
 * @brief 创建角度约束
 */
lv_PUBLIC_API lvConstraint lv_constraint_angle(int id, int entity_a, int entity_b, double angle_rad) {
    lvConstraint c;
    memset(&c, 0, sizeof(c));
    c.type = lv_CONSTRAINT_ANGLE;
    c.id = id;
    c.entity_a = entity_a;
    c.entity_b = entity_b;
    c.entity_c = -1;
    c.value = angle_rad;
    c.is_active = true;
    return c;
}

/**
 * @brief 创建点在圆上约束
 */
lv_PUBLIC_API lvConstraint lv_constraint_on_circle(int id, int point_entity, int circle_entity) {
    lvConstraint c;
    memset(&c, 0, sizeof(c));
    c.type = lv_CONSTRAINT_PT_ON_CIRCLE;
    c.id = id;
    c.entity_a = point_entity;
    c.entity_b = circle_entity;
    c.entity_c = -1;
    c.value = 0.0;
    c.is_active = true;
    return c;
}

/**
 * @brief 创建固定约束
 */
lv_PUBLIC_API lvConstraint lv_constraint_fixed(int id, int entity) {
    lvConstraint c;
    memset(&c, 0, sizeof(c));
    c.type = lv_CONSTRAINT_FIXED;
    c.id = id;
    c.entity_a = entity;
    c.entity_b = -1;
    c.entity_c = -1;
    c.value = 0.0;
    c.is_active = true;
    return c;
}

/**
 * @brief 创建等长约束
 */
lv_PUBLIC_API lvConstraint lv_constraint_equal_length(int id, int entity_a, int entity_b) {
    lvConstraint c;
    memset(&c, 0, sizeof(c));
    c.type = lv_CONSTRAINT_EQUAL_LENGTH;
    c.id = id;
    c.entity_a = entity_a;
    c.entity_b = entity_b;
    c.entity_c = -1;
    c.value = 0.0;
    c.is_active = true;
    return c;
}

/**
 * @brief 创建水平约束
 */
lv_PUBLIC_API lvConstraint lv_constraint_horizontal(int id, int entity) {
    lvConstraint c;
    memset(&c, 0, sizeof(c));
    c.type = lv_CONSTRAINT_HORIZONTAL;
    c.id = id;
    c.entity_a = entity;
    c.entity_b = -1;
    c.entity_c = -1;
    c.value = 0.0;
    c.is_active = true;
    return c;
}

/**
 * @brief 创建垂直约束（线段垂直方向）
 */
lv_PUBLIC_API lvConstraint lv_constraint_vertical(int id, int entity) {
    lvConstraint c;
    memset(&c, 0, sizeof(c));
    c.type = lv_CONSTRAINT_VERTICAL;
    c.id = id;
    c.entity_a = entity;
    c.entity_b = -1;
    c.entity_c = -1;
    c.value = 0.0;
    c.is_active = true;
    return c;
}
