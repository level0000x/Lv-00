/**
 * @file fast_index.c
 * @brief 快速空间索引实现 —— 基于网格哈希的 2D 空间索引
 *
 * @details 实现均匀网格（Uniform Grid）空间索引，支持：
 *   - 以节点 ID 及其包围盒 (x, y, w, h) 插入
 *   - 以查询点 (x, y) 查询可能包含该点的所有节点 ID
 *
 * 算法复杂度：
 *   - 插入：O(1) 期望（哈希表 + 链表追加）
 *   - 查询：O(1 + k) 期望，其中 k 为命中网格单元内的候选节点数
 *
 * 设计要点：
 *   - 网格单元大小根据插入节点的包围盒自适应调整
 *   - 使用分离链表法（separate chaining）解决哈希冲突
 *   - 为简化实现，当前使用全局网格粒度，单元大小 = max(w, h) 的中位数
 *
 * 【lv_hashtable 收敛评估结论（不收敛，保留本实现）】
 * 本模块是"网格空间索引"而非通用键值哈希表，与 lv_hashtable 的
 * int/string 单键单值形态不兼容：
 *   1. 键是 2D 网格坐标对 (gx, gy)，cell_hash 用 Cantor 配对得到 64 位中间值，
 *      无法无损编码进 lv_hashtable 的 32 位 int 键（折叠会引入碰撞）。
 *   2. 值是"多值链"：同一网格单元挂多条 CellEntry（同一包围盒覆盖多单元时
 *      节点 ID 出现在多个单元），lv_hashtable 单值映射无法等价承载。
 *   3. "桶头插"顺序影响查询返回顺序（后插入的节点在前），若改存哈希表需
 *      在表外维护链表，等价于保留本实现。
 *   4. 扩容依赖条目的原始网格坐标 (gx, gy) 重算桶索引（cell_hash 的容量参数
 *      变化），lv_hashtable 自动扩容无法感知该几何语义。
 * 二次收敛评估（lv_hashtable 增加 64 位键形态 lv_hashtable_i64 的方案）：
 *   即使键升级为 64 位 Cantor 配对值，多值链（理由 2）与桶头插顺序（理由 3）
 *   仍要求表外维护链表并复刻重建逻辑 —— 迁移仅把"桶数组"换成"哈希表句柄"，
 *   多一次间接、无任何语义或性能收益，故仍不采用。
 * 强行收敛将引入 64 位键扩展、外部多值链与重建逻辑，收益为负，故保留。
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

#include "lv/fast_index.h"

#include <stdlib.h>
#include <string.h>

#include "lv/lv.h"
#include "lv/lv_internal.h"

/* ============== 内部常量 ============== */

/** 默认网格单元大小（在无插入数据时的回退值） */
#define DEFAULT_CELL_SIZE 1.0

/** 初始桶数量 */
#define INITIAL_BUCKET_COUNT 64

/** 桶扩容因子 */
#define BUCKET_GROWTH_FACTOR 2

/** 最小网格单元大小（避免除零和过度细分） */
#define MIN_CELL_SIZE 1e-6

/* ============== 内部数据结构 ============== */

/**
 * @brief 网格单元内的条目节点（单链表）
 */
typedef struct CellEntry {
    int node_id;            /**< 几何节点 ID */
    int gx;                 /**< 条目所在网格单元 X 坐标（扩容重哈希用） */
    int gy;                 /**< 条目所在网格单元 Y 坐标（扩容重哈希用） */
    struct CellEntry *next; /**< 链表下一节点 */
} CellEntry;

/**
 * @brief 网格单元
 */
typedef struct GridCell {
    CellEntry *head; /**< 条目链表头 */
    int count;       /**< 条目数量 */
} GridCell;

/**
 * @brief 快速空间索引结构
 */
struct lvFastIndex {
    int capacity;     /**< 当前桶数量 */
    int count;        /**< 已插入的节点总数 */
    double cell_size; /**< 网格单元大小（自适应） */
    GridCell *cells;  /**< 网格单元数组（哈希桶） */
};

/* ============== 内部辅助函数 ============== */

/**
 * @brief 将 2D 网格坐标映射到一维哈希桶索引
 *
 * 使用 Cantor 配对函数将 (gx, gy) 映射为 64 位整数，
 * 再取模映射到桶索引。Cantor 配对比简单线性拼接
 * 更能避免大量连续坐标导致的哈希聚集。
 *
 * @param gx      网格 X 坐标
 * @param gy      网格 Y 坐标
 * @param capacity 桶数量
 * @return 桶索引 [0, capacity)
 */
static int cell_hash(int gx, int gy, int capacity) {
    if (capacity <= 0)
        return 0;
    /* Cantor 配对函数：π(k1, k2) = (k1+k2)(k1+k2+1)/2 + k2 */
    /* 先处理符号，确保非负输入 */
    unsigned int ux = (unsigned int) (gx >= 0 ? gx : -gx);
    unsigned int uy = (unsigned int) (gy >= 0 ? gy : -gy);
    unsigned long long sum = (unsigned long long) ux + (unsigned long long) uy;
    unsigned long long paired = (sum * (sum + 1)) / 2 + uy;
    /* 若原始坐标为负，用符号位混合以避免 ⊕/- 对称冲突 */
    if (gx < 0)
        paired ^= 0x5555555555555555ULL;
    if (gy < 0)
        paired ^= 0xAAAAAAAAAAAAAAAAULL;
    return (int) (paired % (unsigned long long) capacity);
}

/**
 * @brief 获取点 (x, y) 所在的网格坐标
 *
 * 网格坐标通过向下取整计算：gx = floor(x / cell_size)
 * 对于负坐标，C 语言的整数除法向零舍入，因此显式处理
 * 以产生正确的向下取整语义。
 */
static int grid_coord(double val, double cell_size) {
    /* 显式 floor 以处理负值 */
    int c = (int) (val / cell_size);
    if (val < 0.0 && val != (double) c * cell_size) {
        c--; /* 修正向零舍入到向下取整 */
    }
    return c;
}

/* ============== 公开 API 实现 ============== */

/**
 * @brief 创建快速空间索引
 *
 * @param capacity 初始桶数量（建议值 >= 64），<= 0 时使用默认值
 * @return 新创建的索引指针，失败返回 NULL
 */
lvFastIndex *lv_fast_index_create(int capacity) {
    lvFastIndex *idx = (lvFastIndex *) lv_calloc(1, sizeof(lvFastIndex));
    if (!idx)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "failed to allocate lvFastIndex");

    idx->capacity = (capacity > 0) ? capacity : INITIAL_BUCKET_COUNT;
    idx->count = 0;
    idx->cell_size = DEFAULT_CELL_SIZE;

    /* 分配网格单元数组 */
    idx->cells = (GridCell *) lv_calloc((size_t) idx->capacity, sizeof(GridCell));
    if (!idx->cells) {
        lv_free((void **) &idx);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "failed to allocate grid cells");
    }

    return idx;
}

/**
 * @brief 销毁快速空间索引，释放所有内部资源
 *
 * @param idx 索引指针（可为 NULL）
 */
void lv_fast_index_destroy(lvFastIndex *idx) {
    if (!idx)
        return;

    /* 释放每个桶的条目链表 */
    for (int i = 0; i < idx->capacity; i++) {
        CellEntry *entry = idx->cells[i].head;
        while (entry) {
            CellEntry *next = entry->next;
            lv_free((void **) &entry);
            entry = next;
        }
    }
    lv_free((void **) &(idx->cells));
    lv_free((void **) &idx);
}

/**
 * @brief 向空间索引插入节点
 *
 * 根据节点的包围盒 (x, y, w, h) 计算其覆盖的所有网格单元，
 * 并在每个单元中记录该节点的 ID。包围盒覆盖多个单元时，
 * 节点 ID 会出现在所有被覆盖的单元中。
 *
 * 插入后自适应更新 cell_size：取当前所有节点包围盒尺寸的中位数。
 *
 * @param idx     空间索引
 * @param node_id 几何节点 ID
 * @param x       包围盒最小 X 坐标
 * @param y       包围盒最小 Y 坐标
 * @param w       包围盒宽度（必须 >= 0）
 * @param h       包围盒高度（必须 >= 0）
 * @return 0 成功，-1 失败（参数无效或内存不足）
 */
int lv_fast_index_insert(lvFastIndex *idx, int node_id, double x, double y, double w, double h) {
    if (!idx || w < 0.0 || h < 0.0)
        return -1;

    /* 自适应调整网格单元大小：
     * 取包围盒尺寸作为候选 cell_size，取最大值以避免过度细分。
     * 简化策略：cell_size = max(current, max(w, h)) */
    double candidate = (w > h) ? w : h;
    if (candidate > idx->cell_size && candidate >= MIN_CELL_SIZE) {
        idx->cell_size = candidate;
    }

    /* 计算包围盒覆盖的网格单元范围 */
    double cs = idx->cell_size;
    int gx_min = grid_coord(x, cs);
    int gy_min = grid_coord(y, cs);
    int gx_max = grid_coord(x + w, cs);
    int gy_max = grid_coord(y + h, cs);

    /* 限制单次插入覆盖的单元数，防止包围盒过大时性能退化 */
    int gx_range = gx_max - gx_min + 1;
    int gy_range = gy_max - gy_min + 1;
    if (gx_range > 100 || gy_range > 100) {
        /* 包围盒过大：仅插入最小单元（退化处理） */
        gx_max = gx_min;
        gy_max = gy_min;
    }

    /* 可能需要扩容 */
    int total_cells = gx_range * gy_range;
    if (total_cells > idx->capacity / 4 && idx->capacity < 4096) {
        /* 扩容：加倍桶数量 */
        int new_cap = idx->capacity * BUCKET_GROWTH_FACTOR;
        GridCell *new_cells = (GridCell *) lv_calloc((size_t) new_cap, sizeof(GridCell));
        if (!new_cells)
            return -1;

        /* 重新哈希所有现有条目 */
        for (int i = 0; i < idx->capacity; i++) {
            CellEntry *entry = idx->cells[i].head;
            while (entry) {
                CellEntry *next = entry->next;
                /* 按条目原始网格坐标 (gx, gy) 重新哈希到新桶（加到新桶头部），
                 * 避免用旧桶索引冒充坐标导致扩容后条目落错桶、查询丢失 */
                int new_idx = cell_hash(entry->gx, entry->gy, new_cap);
                entry->next = new_cells[new_idx].head;
                new_cells[new_idx].head = entry;
                new_cells[new_idx].count++;
                entry = next;
            }
        }
        lv_free((void **) &(idx->cells));
        idx->cells = new_cells;
        idx->capacity = new_cap;
    }

    /* 在覆盖的每个网格单元中记录该节点 */
    for (int gx = gx_min; gx <= gx_max; gx++) {
        for (int gy = gy_min; gy <= gy_max; gy++) {
            int bucket = cell_hash(gx, gy, idx->capacity);
            CellEntry *entry = (CellEntry *) lv_malloc(sizeof(CellEntry));
            if (!entry)
                continue; /* 尽力而为：内存不足时跳过此单元 */

            entry->node_id = node_id;
            entry->gx = gx;
            entry->gy = gy;
            entry->next = idx->cells[bucket].head;
            idx->cells[bucket].head = entry;
            idx->cells[bucket].count++;
        }
    }

    idx->count++;
    return 0;
}

/**
 * @brief 查询包含点 (x, y) 的所有节点 ID
 *
 * 计算点所在的网格单元，遍历该单元中记录的所有节点 ID，
 * 去除重复后写入输出数组。
 *
 * @param idx     空间索引
 * @param x       查询点 X 坐标
 * @param y       查询点 Y 坐标
 * @param out_ids 输出数组（调用者分配，大小 = max_out）
 * @param max_out 输出数组最大容量
 * @return >= 0 命中的节点数量，-1 参数无效
 *
 * @note 返回的节点 ID 可能有重复（当节点包围盒覆盖多个单元时），
 *       当前实现不做去重。调用者如需唯一结果，需自行去重。
 * @note 仅返回网格单元内的候选节点，不保证候选节点确实包含该点
 *       （需要调用者进一步做精确的包围盒或几何包含检查）。
 */
int lv_fast_index_query(lvFastIndex *idx, double x, double y, int *out_ids, int max_out) {
    if (!idx || !out_ids || max_out <= 0)
        return -1;

    int gx = grid_coord(x, idx->cell_size);
    int gy = grid_coord(y, idx->cell_size);
    int bucket = cell_hash(gx, gy, idx->capacity);

    int out_count = 0;
    CellEntry *entry = idx->cells[bucket].head;
    while (entry && out_count < max_out) {
        out_ids[out_count++] = entry->node_id;
        entry = entry->next;
    }

    return out_count;
}
