/**
 * @file magic_array.c
 * @brief 元素反应矩阵与魔法阵系统实现
 *
 * @details 从 magic.c 拆分的子模块（Lv-00 项目 v3.3.0+）。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "magic_internal.h"
#include "magic.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_json.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/lv_strbuf.h"

/* ============================================================
 * 元素反应矩阵
 * ============================================================ */

/** 元素反应矩阵：定义两种元素之间的相互作用关系 */
static ElementReaction element_reaction_matrix[MAGIC_ELEMENT_TOTAL_COUNT][MAGIC_ELEMENT_TOTAL_COUNT] = {
    /*        NONE  FIRE  WATER AIR  EARTH ETHER */
    /*NONE*/ {ELEMENT_REACTION_NONE, ELEMENT_REACTION_NONE, ELEMENT_REACTION_NONE, ELEMENT_REACTION_NONE,
              ELEMENT_REACTION_NONE, ELEMENT_REACTION_NONE},
    /*FIRE*/
    {ELEMENT_REACTION_NONE, ELEMENT_REACTION_NONE, ELEMENT_REACTION_CONFLICT, ELEMENT_REACTION_ENHANCE,
     ELEMENT_REACTION_ENHANCE, ELEMENT_REACTION_NONE},
    /*WATER*/
    {ELEMENT_REACTION_NONE, ELEMENT_REACTION_CONFLICT, ELEMENT_REACTION_NONE, ELEMENT_REACTION_WEAKEN,
     ELEMENT_REACTION_ENHANCE, ELEMENT_REACTION_NONE},
    /*AIR*/
    {ELEMENT_REACTION_NONE, ELEMENT_REACTION_ENHANCE, ELEMENT_REACTION_WEAKEN, ELEMENT_REACTION_NONE,
     ELEMENT_REACTION_CONFLICT, ELEMENT_REACTION_NONE},
    /*EARTH*/
    {ELEMENT_REACTION_NONE, ELEMENT_REACTION_ENHANCE, ELEMENT_REACTION_ENHANCE, ELEMENT_REACTION_CONFLICT,
     ELEMENT_REACTION_NONE, ELEMENT_REACTION_NONE},
    /*ETHER*/
    {ELEMENT_REACTION_NONE, ELEMENT_REACTION_NONE, ELEMENT_REACTION_NONE, ELEMENT_REACTION_NONE, ELEMENT_REACTION_NONE,
     ELEMENT_REACTION_NONE}};

/**
 * @brief 查询两种魔法元素之间的反应关系
 *
 * 根据元素反应矩阵查询 e1 对 e2 的反应类型。
 * 超出有效范围的元素索引将返回 ELEMENT_REACTION_NONE。
 *
 * @param e1 第一种魔法元素
 * @param e2 第二种魔法元素
 * @return 元素反应类型
 */
ElementReaction array_check_element_reaction(MagicElement e1, MagicElement e2) {
    if (e1 < 0 || e1 > ELEMENT_ETHER || e2 < 0 || e2 > ELEMENT_ETHER) {
        return ELEMENT_REACTION_NONE;
    }
    return element_reaction_matrix[e1][e2];
}

/* ============================================================
 * 魔法阵系统实现
 * ============================================================ */

/** 魔法阵结构体：由符文序列、约束图和约束列表组成 */
struct MagicArray {
    char *name;                       /* 魔法阵名称 */
    RuneSequence *runes;              /* 符文序列 */
    ConstraintGraph *graph;           /* 底层约束图 */
    ArrayConstraintType *constraints; /* 约束类型数组 */
    int constraint_count;             /* 当前约束数量 */
    int constraint_capacity;          /* 约束数组容量 */
};

/**
 * @brief 创建空的魔法阵
 *
 * 初始化魔法阵的符文序列、底层约束图和约束数组。
 * 约束数组初始容量为 32。
 *
 * @return 新创建的魔法阵指针，失败返回 NULL
 */
MagicArray *magic_array_create(void) {
    MagicArray *array = (MagicArray *) lv_calloc(1, sizeof(MagicArray));
    if (!array)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "magic_array_create: array calloc failed");

    /* 创建符文序列子组件 */
    array->runes = rune_sequence_create();
    if (!array->runes) {
        lv_free((void **) &array);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "magic_array_create: runes create failed");
    }

    /* 创建底层约束图子组件 */
    array->graph = graph_create();
    if (!array->graph) {
        rune_sequence_destroy(array->runes);
        lv_free((void **) &array);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "magic_array_create: graph create failed");
    }

    /* 初始化约束类型动态数组 */
    array->constraint_count = 0;
    array->constraint_capacity = MAGIC_ARRAY_CONSTRAINT_INIT_CAP;
    array->constraints = (ArrayConstraintType *) lv_malloc(array->constraint_capacity * sizeof(ArrayConstraintType));

    /* 子组件创建失败时逆序释放已创建的资源 */
    if (!array->constraints) {
        graph_destroy(array->graph);
        rune_sequence_destroy(array->runes);
        lv_free((void **) &array);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "magic_array_create: constraints malloc failed");
    }

    return array;
}

/**
 * @brief 销毁魔法阵并释放所有关联资源
 *
 * 释放符文序列、约束图、约束数组以及魔法阵结构体本身。
 * 释放后指针会被自动置为 NULL。可安全传入 NULL。
 *
 * @param array 待销毁的魔法阵指针
 */
void magic_array_destroy(MagicArray *array) {
    if (!array)
        return;

    /* 逆序释放各子组件：符文序列、约束图、约束数组、自身 */
    if (array->runes) {
        rune_sequence_destroy(array->runes);
    }
    if (array->graph) {
        graph_destroy(array->graph);
    }
    if (array->constraints) {
        lv_free((void **) &array->constraints);
    }
    lv_free((void **) &array);
}

/**
 * @brief 向魔法阵中添加符文
 *
 * 将符文添加到魔法阵的符文序列和底层约束图中。
 * 调用者保留原符文的所有权，魔法阵内部会创建副本。
 *
 * @param array 魔法阵指针
 * @param rune  待添加的符文指针
 * @return 符文在图中的节点索引，失败返回 -1
 */
int magic_array_add_rune(MagicArray *array, Rune *rune) {
    if (!array || !rune)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "magic_array_add_rune: array or rune is NULL");

    /* 将符文坐标添加到底层约束图中 */
    SymbolicCoord *coord = rune_get_value(rune);
    SymbolicCoord *coords[2] = {coord, coord};

    AddNodeResult result = graph_add_point(array->graph, coords, 1);
    if (result != ADD_NODE_OK) {
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "magic_array_add_rune: graph_add_point failed");
    }

    /* 获取新节点在约束图中的索引 */
    int index = array->graph->next_node_id - 1;

    /* 复制符文到序列（调用者保留原符文所有权） */
    Rune *rune_clone = rune_copy(rune);
    if (!rune_clone) {
        graph_remove_node(array->graph, index);
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "magic_array_add_rune: rune_copy failed");
    }

    /* 将符文副本追加到序列，失败时回滚图节点 */
    if (!rune_sequence_add(array->runes, rune_clone)) {
        graph_remove_node(array->graph, index);
        rune_destroy(rune_clone);
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "magic_array_add_rune: rune_sequence_add failed");
    }

    return index;
}

/**
 * @brief 从魔法阵中移除指定索引的符文
 *
 * 同时从约束图和符文序列中移除该符文，后续符文索引会前移。
 *
 * @param array      魔法阵指针
 * @param rune_index 待移除符文的索引
 * @return 移除成功返回 true，参数无效或索引越界返回 false
 */
bool magic_array_remove_rune(MagicArray *array, int rune_index) {
    if (!array || rune_index < 0 || rune_index >= rune_sequence_length(array->runes)) {
        return false;
    }

    /* 从底层约束图中移除对应节点 */
    GeomNode *node = graph_get_node(array->graph, rune_index);
    if (node) {
        graph_remove_node(array->graph, rune_index);
    }

    /* 从符文序列中移除并销毁符文，后续元素前移填补空缺 */
    rune_destroy(array->runes->runes[rune_index]);
    for (int i = rune_index; i < array->runes->rune_count - 1; i++) {
        array->runes->runes[i] = array->runes->runes[i + 1];
    }
    array->runes->rune_count--;

    return true;
}

/**
 * @brief 获取魔法阵中指定索引的符文
 *
 * @param array      魔法阵指针
 * @param rune_index 符文索引
 * @return 符文指针，参数无效时返回 NULL
 */
Rune *magic_array_get_rune(const MagicArray *array, int rune_index) {
    if (!array)
        return NULL;
    return rune_sequence_get(array->runes, rune_index);
}

/**
 * @brief 获取魔法阵中的符文数量
 *
 * @param array 魔法阵指针
 * @return 符文数量，array 为 NULL 时返回 0
 */
int magic_array_get_rune_count(const MagicArray *array) {
    if (!array)
        return 0;
    return rune_sequence_length(array->runes);
}

/**
 * @brief 向魔法阵添加约束关系
 *
 * 在两个符文之间建立约束关系，同时更新底层约束图。
 * 约束类型会被映射为约束图的内部类型。
 * 如果约束数组容量不足，会自动扩容（容量翻倍）。
 *
 * @param array        魔法阵指针
 * @param type         约束类型
 * @param rune1_index 第一个符文的索引
 * @param rune2_index 第二个符文的索引
 * @return 约束在图中的 ID，失败返回 -1
 */
int magic_array_add_constraint(MagicArray *array, ArrayConstraintType type, int rune1_index, int rune2_index) {
    if (!array)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "magic_array_add_constraint: array is NULL");
    if (rune1_index < 0 || rune1_index >= array->runes->rune_count)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "magic_array_add_constraint: rune1_index out of range");
    if (rune2_index < 0 || rune2_index >= array->runes->rune_count)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "magic_array_add_constraint: rune2_index out of range");

    /* 约束数组容量不足时自动扩容 */
    if (!lv_ensure_capacity((void **)&array->constraints, array->constraint_count,
                            &array->constraint_capacity, sizeof(ArrayConstraintType), 1))
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "magic_array_add_constraint: ensure_capacity failed");

    /* 将魔法阵约束类型映射为底层约束图内部类型 */
    static const ConstraintType s_arr_to_ct[] = {
        CONNECTION,   /* ARRAY_CONNECTION = 0 */
        INCIDENCE,    /* ARRAY_ENHANCEMENT = 1 */
        INCIDENCE,    /* ARRAY_CONFLICT = 2 */
        INTERSECTION, /* ARRAY_INTERSECTION = 3 */
        CONTAINMENT,  /* ARRAY_CONTAINMENT = 4 */
    };
    ConstraintType graph_type;
    if ((int)type >= 0 && (int)type < (int)(sizeof(s_arr_to_ct) / sizeof(s_arr_to_ct[0])))
        graph_type = s_arr_to_ct[type];
    else
        graph_type = CONNECTION;

    int participants[2] = {rune1_index, rune2_index};
    AddConstraintResult result;
    switch (graph_type) {
        case CONTAINMENT:
            result = graph_add_containment(array->graph, participants[0], participants[1]);
            break;
        case CONNECTION:
            result = graph_add_connection(array->graph, participants[0], participants[1]);
            break;
        case INCIDENCE:
        default:
            result = graph_add_incidence(array->graph, participants[0], participants[1]);
            break;
    }

    if (result == ADD_CONSTRAINT_OK) {
        array->constraints[array->constraint_count++] = type;
        return array->graph->next_constraint_id - 1;
    }

    lv_RETURN_ERROR(lv_ERROR_INTERNAL, "magic_array_add_constraint: graph_add_* failed");
}

/**
 * @brief 从魔法阵中移除指定索引的约束
 *
 * 同时从约束图和约束数组中移除，后续约束索引会前移。
 *
 * @param array           魔法阵指针
 * @param constraint_index 约束索引
 * @return 移除成功返回 true，参数无效或索引越界返回 false
 */
bool magic_array_remove_constraint(MagicArray *array, int constraint_index) {
    if (!array || constraint_index < 0 || constraint_index >= array->constraint_count) {
        return false;
    }

    /* 从底层约束图中移除指定约束 */
    graph_remove_constraint(array->graph, constraint_index);

    /* 从约束数组中移除，后续元素前移填补空缺 */
    for (int i = constraint_index; i < array->constraint_count - 1; i++) {
        array->constraints[i] = array->constraints[i + 1];
    }
    array->constraint_count--;

    return true;
}

/**
 * @brief 获取魔法阵中的约束数量
 *
 * @param array 魔法阵指针
 * @return 约束数量，array 为 NULL 时返回 0
 */
int magic_array_get_constraint_count(const MagicArray *array) {
    if (!array)
        return 0;
    return array->constraint_count;
}

/**
 * @brief 检查魔法阵的元素平衡性
 *
 * 通过计算五种魔法元素（不含 ELEMENT_NONE）分布的方差来判断平衡性。
 * 方差小于均值的两倍时视为平衡。
 *
 * @param array 魔法阵指针
 * @return 平衡返回 true，不平衡或参数无效返回 false
 */
bool magic_array_check_balance(const MagicArray *array) {
    if (!array)
        return false;

    /* 统计各魔法元素出现的次数 */
    int element_counts[MAGIC_ELEMENT_TOTAL_COUNT] = {0};
    for (int i = 0; i < array->runes->rune_count; i++) {
        Rune *rune = array->runes->runes[i];
        /* 边界检查：确保元素值在有效范围内，防止数组越界 */
        if (rune->element >= 0 && rune->element <= ELEMENT_ETHER) {
            element_counts[rune->element]++;
        }
    }

    /* 计算五种有效元素（不含NONE）的分布方差 */
    double mean = (double) array->runes->rune_count / (double) MAGIC_REAL_ELEMENT_COUNT;
    double variance = 0.0;

    for (int i = 1; i <= MAGIC_REAL_ELEMENT_COUNT; i++) { /* 跳过 ELEMENT_NONE */
        double diff = element_counts[i] - mean;
        variance += diff * diff;
    }
    variance /= (double) MAGIC_REAL_ELEMENT_COUNT;

    /* 方差小于（均值 × 平衡阈值）时视为元素分布平衡 */
    return variance < mean * MAGIC_ELEMENT_BALANCE_THRESHOLD;
}

/**
 * @brief 统计魔法阵中指定元素的符文数量
 *
 * @param array   魔法阵指针
 * @param element 要统计的魔法元素类型
 * @return 该元素的符文数量，array 为 NULL 时返回 0
 */
int array_count_elements(const MagicArray *array, MagicElement element) {
    if (!array)
        return 0;

    /* 遍历魔法阵中的所有符文，统计指定元素的出现次数 */
    int count = 0;
    for (int i = 0; i < array->runes->rune_count; i++) {
        if (array->runes->runes[i]->element == element) {
            count++;
        }
    }
    return count;
}

/**
 * @brief 计算魔法阵的稳定性评分
 *
 * 稳定性受冲突约束数量和符文数量影响：
 * - 每个冲突约束降低 0.1 稳定性
 * - 符文数量少于 3 时稳定性减半
 * - 最终结果限制在 [0.0, 1.0] 范围内
 *
 * @param array 魔法阵指针
 * @return 稳定性评分（0.0 ~ 1.0），array 为 NULL 或无符文时返回 0.0
 */
double array_calculate_stability(const MagicArray *array) {
    if (!array || array->runes->rune_count == 0)
        return 0.0;

    double stability = MAGIC_STABILITY_MAX;
    int conflicts = 0;

    /* 统计冲突约束的数量 */
    for (int i = 0; i < array->constraint_count; i++) {
        if (array->constraints[i] == ARRAY_CONFLICT) {
            conflicts++;
        }
    }

    /* 每个冲突约束降低固定比例的稳定性 */
    stability -= (double) conflicts * MAGIC_STABILITY_CONFLICT_PENALTY;

    /* 符文数量少于最低要求时稳定性折半 */
    if (array->runes->rune_count < MAGIC_STABILITY_MIN_RUNES) {
        stability *= MAGIC_STABILITY_TOO_FEW_MULTIPLIER;
    }

    /* 截断到 [0.0, 1.0] 范围内 */
    return stability < 0.0 ? 0.0 : stability;
}

/**
 * @brief 深拷贝魔法阵
 *
 * 创建魔法阵的完整副本，包括所有符文和约束关系。
 * 调用者负责通过 magic_array_destroy 释放返回的副本。
 *
 * @param src 源魔法阵指针
 * @return 新魔法阵指针（深拷贝），失败或 src 为 NULL 时返回 NULL
 */
MagicArray *magic_array_copy(const MagicArray *src) {
    if (!src)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "magic_array_copy: src is NULL");

    MagicArray *copy = magic_array_create();
    if (!copy)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "magic_array_copy: create failed");

    /* 逐个深拷贝符文，同步更新约束图 */
    for (int i = 0; i < src->runes->rune_count; i++) {
        Rune *rune = rune_copy(src->runes->runes[i]);
        if (!rune) {
            magic_array_destroy(copy);
            lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "magic_array_copy: rune_copy failed");
        }
        if (!rune_sequence_add(copy->runes, rune)) {
            rune_destroy(rune);
            magic_array_destroy(copy);
            lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "magic_array_copy: rune_sequence_add failed");
        }

        /* 将符文坐标加入副本的约束图 */
        SymbolicCoord *coords[] = {rune->coord};
        graph_add_point(copy->graph, coords, 1);
    }

    /* 深拷贝所有约束关系 */
    for (int i = 0; i < src->constraint_count; i++) {
        magic_array_add_constraint(copy, src->constraints[i], i, (i + 1) % src->runes->rune_count);
    }

    return copy;
}

/**
 * @brief 合并两个魔法阵
 *
 * 将源魔法阵 (src) 的所有符文和约束合并到目标魔法阵 (dest) 中。
 * 合并过程中会为每个符文和约束创建副本。
 *
 * @param dest 目标魔法阵（接收合并内容）
 * @param src  源魔法阵（提供合并内容）
 * @return 合并成功返回 true，参数无效或内存不足返回 false
 */
bool magic_array_merge(MagicArray *dest, const MagicArray *src) {
    if (!dest || !src)
        return false;

    /* 将源魔法阵的所有符文逐个复制到目标阵 */
    for (int i = 0; i < src->runes->rune_count; i++) {
        int idx = magic_array_add_rune(dest, src->runes->runes[i]);
        if (idx < 0) {
            lv_LOG_WARNING("magic_array_merge: 合并符文失败，索引 %d", i);
            return false;
        }
    }

    /* 将源魔法阵的所有约束关系映射到目标阵 */
    for (int i = 0; i < src->constraint_count; i++) {
        /* 约束索引需要映射到 dest 中的新索引 */
        int result = magic_array_add_constraint(dest, src->constraints[i], i, (i + 1) % src->runes->rune_count);
        if (result < 0) {
            lv_LOG_WARNING("magic_array_merge: 合并约束失败，索引 %d", i);
            return false;
        }
    }

    return true;
}

/**
 * @brief 将魔法阵序列化为 JSON 字符串
 *
 * 将魔法阵的符文数量、约束数量以及每个符文的基本信息
 * 序列化为 JSON 格式的字符串。
 *
 * @param array 魔法阵指针
 * @return 新分配的 JSON 字符串，失败返回 NULL（调用者需用 lv_free 释放）
 */
char *magic_array_serialize(const MagicArray *array) {
    if (!array)
        return NULL;

    int rune_count = array->runes->rune_count;
    int constraint_count = array->constraint_count;

    lvJsonBuf buf;
    if (!lv_json_buf_init(&buf, MAGIC_SERIALIZE_JSON_BASE_SIZE + (size_t) rune_count * MAGIC_SERIALIZE_PER_RUNE_SIZE))
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "magic_array_serialize: json_buf_init failed");

    lv_json_buf_append_fmt(&buf, "{\"rune_count\":%d,\"constraint_count\":%d,\"runes\":[",
                           rune_count, constraint_count);

    for (int i = 0; i < rune_count; i++) {
        Rune *rune = array->runes->runes[i];
        const char *elem_str = element_to_string(rune->element);
        if (i > 0)
            lv_json_buf_append_char(&buf, ',');
        lv_json_buf_append_fmt(&buf, "{\"element\":\"%s\",\"power\":%d}", elem_str, rune->power_level);
    }

    lv_json_buf_append_raw(&buf, "]}");

    return lv_json_buf_finalize(&buf);
}

/**
 * @brief 从 JSON 字符串反序列化魔法阵
 *
 * 支持的 JSON 格式：
 *   {"name":"阵名","runes":[{"type":"rational","num":1,"denom":2,"element":"FIRE"},...]}
 *
 * JSON 解析器说明：
 *   本解析器采用手写实现，不依赖外部 JSON 库。使用 strstr 进行字段查找，
 *   并通过跳过字符串值内部内容来避免误匹配。
 *
 *   已知限制：
 *   - 不支持 JSON 字符串中的 unicode 转义（\uXXXX），遇到时将跳过
 *   - 不支持嵌套超过一层的对象/数组（runes 数组内的对象应为扁平结构）
 *   - 字段查找基于 strstr，如果字符串值中包含与关键字相同的文本可能误匹配
 *     （已通过跳过字符串值的机制缓解此问题）
 *
 *   对于复杂的 JSON 输入，建议使用标准 JSON 库（如 cJSON）替代。
 *
 * @param json JSON 格式字符串
 * @return 反序列化成功返回新创建的魔法阵，失败返回 NULL
 */

/**
 * @brief 在 JSON 文本中安全地查找键名（跳过字符串值内部）
 *
 * 从位置 start 开始向后搜索 "key" 模式，但跳过所有 JSON 字符串值
 * 的内部内容（包括转义字符），避免在字符串值中误匹配键名。
 *
 * @param start 搜索起始位置
 * @param key   要查找的键名（不含引号，如 "type"）
 * @return 找到返回键名起始位置的指针，未找到返回 NULL
 */
static const char *json_find_key_safe(const char *start, const char *key) {
    if (!start || !key)
        return NULL;

    /* 构造搜索模式: "key" */
    lvStrBuf sb = {0};
    lv_strbuf_printf(&sb, "\"%s\"", key);

    const char *p = start;
    while (*p) {
        /* 检查是否匹配目标键名 */
        if (strncmp(p, sb.data, strlen(sb.data)) == 0) {
            lv_strbuf_destroy(&sb);
            return p;
        }

        /* 如果当前字符是双引号，跳过整个字符串值 */
        if (*p == '"') {
            p++;
            while (*p && *p != '"') {
                if (*p == '\\' && *(p + 1)) {
                    /* 跳过转义字符：\", \\, \/, \b, \f, \n, \r, \t, \uXXXX */
                    p++;
                    if (*p == 'u' && *(p + 1) && *(p + 2) && *(p + 3) && *(p + 4)) {
                        /* 跳过 \uXXXX unicode 转义（4个十六进制数字） */
                        p += 5;
                    } else {
                        p++; /* 跳过转义后的单个字符 */
                    }
                } else {
                    p++;
                }
            }
            if (*p == '"')
                p++;
        } else {
            p++;
        }
    }

    return NULL;
}

/**
 * @brief 从 JSON 字符串值中提取解码后的文本
 *
 * 处理常见的 JSON 转义序列：\", \\, \/, \b, \f, \n, \r, \t。
 * 不处理 \uXXXX unicode 转义（遇到时保留原始转义文本）。
 *
 * @param src  指向字符串值第一个字符（引号后）的指针
 * @param dst  目标缓冲区
 * @param dst_cap 目标缓冲区容量
 * @return 写入的字符数（不含终止符），-1 表示错误
 */
static int json_decode_string(const char *src, char *dst, size_t dst_cap) {
    if (!src || !dst || dst_cap == 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "json_decode_string: invalid parameters");

    size_t written = 0;
    const char *p = src;

    while (*p && *p != '"' && written < dst_cap - 1) {
        if (*p == '\\') {
            p++;
            /* JSON 标准转义字符映射表 */
            static const struct {
                char escape;
                char actual;
            } s_json_escape_table[] = {
                {'"', '"'},
                {'\\', '\\'},
                {'/', '/'},
                {'b', '\b'},
                {'f', '\f'},
                {'n', '\n'},
                {'r', '\r'},
                {'t', '\t'},
            };
            int found = 0;
            for (size_t i = 0; i < sizeof(s_json_escape_table) / sizeof(s_json_escape_table[0]); i++) {
                if (*p == s_json_escape_table[i].escape) {
                    dst[written++] = s_json_escape_table[i].actual;
                    found = 1;
                    break;
                }
            }
            if (!found) {
                if (*p == 'u') {
                    /* \uXXXX unicode 转义：当前不解码，保留为原始文本 */
                    if (written + 6 < dst_cap - 1) {
                        dst[written++] = '\\';
                        dst[written++] = 'u';
                        if (p[1])
                            dst[written++] = p[1];
                        if (p[2])
                            dst[written++] = p[2];
                        if (p[3])
                            dst[written++] = p[3];
                        if (p[4])
                            dst[written++] = p[4];
                        p += 4;
                    }
                } else {
                    /* 未知转义序列，保留原样 */
                    if (written + 1 < dst_cap - 1) {
                        dst[written++] = '\\';
                        dst[written++] = *p;
                    }
                }
            }
            p++;
        } else {
            dst[written++] = *p;
            p++;
        }
    }

    dst[written] = '\0';
    return (int) written;
}

/**
 * @brief 跳过 JSON 字符串值
 *
 * 从当前位置（应在引号上）跳过整个字符串值，包括转义字符。
 *
 * @param p 指向字符串起始引号的指针
 * @return 跳过字符串后的下一个字符位置
 */
static const char *json_skip_string(const char *p) {
    if (!p || *p != '"')
        return p;
    p++; /* 跳过起始引号 */
    while (*p && *p != '"') {
        if (*p == '\\' && *(p + 1)) {
            p += 2; /* 跳过转义序列 */
        } else {
            p++;
        }
    }
    if (*p == '"')
        p++; /* 跳过结束引号 */
    return p;
}

MagicArray *magic_array_deserialize(const char *json) {
    if (!json || json[0] == '\0') {
        lv_LOG_WARNING("magic_array_deserialize: 输入 JSON 为空");
        return NULL;
    }

    /* 跳过前导空白 */
    while (*json == ' ' || *json == '\t' || *json == '\n' || *json == '\r')
        json++;

    /* 检查 JSON 对象起始 */
    if (json[0] != '{') {
        lv_LOG_WARNING("magic_array_deserialize: JSON 格式无效，期望 '{'");
        return NULL;
    }

    /* 创建空的魔法阵 */
    MagicArray *array = magic_array_create();
    if (!array) {
        lv_LOG_WARNING("magic_array_deserialize: 无法创建魔法阵");
        return NULL;
    }

    /* 查找 runes 数组（使用安全查找，避免误匹配字符串值内的 "runes"） */
    const char *runes_key = json_find_key_safe(json, "runes");
    if (!runes_key) {
        /* 没有 runes 字段，返回空魔法阵 */
        return array;
    }

    /* 查找数组起始 */
    const char *array_start = strchr(runes_key, '[');
    if (!array_start) {
        lv_LOG_WARNING("magic_array_deserialize: runes 不是数组格式");
        return array;
    }

    /* 遍历数组元素 */
    const char *ptr = array_start + 1;
    while (*ptr && *ptr != ']') {
        /* 跳过空白和逗号 */
        while (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == ',' || *ptr == '\r')
            ptr++;
        if (*ptr == ']')
            break;

        /* 查找对象起始 */
        if (*ptr != '{') {
            ptr++;
            continue;
        }

        /* 使用安全查找提取字段，避免嵌套对象/字符串值中的误匹配 */
        const char *obj_end = strchr(ptr, '}');
        if (!obj_end)
            break;

        /* 计算当前对象的范围，限制字段搜索在此范围内 */
        const char *type_key = json_find_key_safe(ptr, "type");
        const char *num_key = json_find_key_safe(ptr, "num");
        const char *denom_key = json_find_key_safe(ptr, "denom");
        const char *value_key = json_find_key_safe(ptr, "value");
        const char *element_key = json_find_key_safe(ptr, "element");

        /* 确保找到的键在当前对象范围内 */
        if (type_key && type_key > obj_end)
            type_key = NULL;
        if (num_key && num_key > obj_end)
            num_key = NULL;
        if (denom_key && denom_key > obj_end)
            denom_key = NULL;
        if (value_key && value_key > obj_end)
            value_key = NULL;
        if (element_key && element_key > obj_end)
            element_key = NULL;

        Rune *rune = NULL;
        MagicElement element = ELEMENT_NONE;

        /* 解析元素类型 */
        if (element_key) {
            const char *elem_val_start = strchr(element_key + 8, ':');
            if (elem_val_start) {
                elem_val_start++;
                while (*elem_val_start == ' ' || *elem_val_start == '"')
                    elem_val_start++;
                if (strncmp(elem_val_start, "FIRE", 4) == 0)
                    element = ELEMENT_FIRE;
                else if (strncmp(elem_val_start, "WATER", 5) == 0)
                    element = ELEMENT_WATER;
                else if (strncmp(elem_val_start, "EARTH", 5) == 0)
                    element = ELEMENT_EARTH;
                else if (strncmp(elem_val_start, "AIR", 3) == 0)
                    element = ELEMENT_AIR;
            }
        }

        /* 根据类型创建符文 */
        if (type_key && strstr(type_key, "\"rational\"")) {
            /* 有理数类型 */
            int64_t num = 0;
            uint64_t denom = 1;

            if (num_key) {
                const char *num_val = strchr(num_key + 5, ':');
                if (num_val)
                    num = strtoll(num_val + 1, NULL, 10);
            }
            if (denom_key) {
                const char *denom_val = strchr(denom_key + 7, ':');
                if (denom_val)
                    denom = strtoull(denom_val + 1, NULL, 10);
            }
            if (denom == 0)
                denom = 1;

            rune = rune_create_rational(num, denom, element);
        } else if (type_key && strstr(type_key, "\"algebraic\"")) {
            /* 代数数类型 */
            double value = 0.0;
            if (value_key) {
                const char *val_start = strchr(value_key + 7, ':');
                if (val_start)
                    value = strtod(val_start + 1, NULL);
            }
            rune = rune_create_algebraic(value, element);
        }

        if (rune) {
            magic_array_add_rune(array, rune);
        }

        ptr = obj_end + 1;
    }

    /* 尝试解析名称字段（使用安全查找） */
    const char *name_key = json_find_key_safe(json, "name");
    if (name_key) {
        const char *name_start = strchr(name_key + 6, ':');
        if (name_start) {
            name_start++;
            while (*name_start == ' ')
                name_start++;
            if (*name_start == '"') {
                name_start++; /* 跳过起始引号 */
                char name_buf[256];
                int name_len = json_decode_string(name_start, name_buf, sizeof(name_buf));
                if (name_len > 0) {
                    char *name_copy = (char *) lv_malloc((size_t) name_len + 1);
                    if (name_copy) {
                        lv_strlcpy(name_copy, name_buf, (size_t) name_len + 1);
                        if (array->name)
                            lv_free((void **) &array->name);
                        array->name = name_copy;
                    }
                }
            }
        }
    }

    return array;
}

