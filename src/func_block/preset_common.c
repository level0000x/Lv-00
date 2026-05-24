/**
 * @file preset_common.c
 * @brief 预设函数块系统 - 公共工具实现
 *
 * @details 实现预设系统公共头文件中声明的工具函数。
 *
 * @version 5.0.0
 * @author Lv-00 Project
 */

#include "preset_common.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "error_codes.h"

/* ============================================================
 * 字符串操作实现
 * ============================================================ */

size_t lv00_safe_strncpy(char *dest, const char *src, size_t dest_size) {
    PRESET_CHECK_NULL(dest, error);
    PRESET_CHECK_NULL(src, error);

    if (dest_size == 0) {
        return 0;
    }

    size_t i;
    for (i = 0; i < dest_size - 1 && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';

    /* 返回源字符串长度（不含终止符） */
    while (src[i] != '\0') {
        i++;
    }
    return i;

error:
    return 0;
}

size_t lv00_safe_strncat(char *dest, const char *src, size_t dest_size) {
    PRESET_CHECK_NULL(dest, error);
    PRESET_CHECK_NULL(src, error);

    if (dest_size == 0) {
        return 0;
    }

    /* 找到 dest 的末尾 */
    size_t dest_len = 0;
    while (dest_len < dest_size && dest[dest_len] != '\0') {
        dest_len++;
    }

    if (dest_len >= dest_size - 1) {
        return dest_len; /* 缓冲区已满 */
    }

    /* 追加 src */
    size_t i;
    for (i = 0; dest_len + i < dest_size - 1 && src[i] != '\0'; i++) {
        dest[dest_len + i] = src[i];
    }
    dest[dest_len + i] = '\0';

    return dest_len + i;

error:
    return 0;
}

int lv00_safe_snprintf(char *dest, size_t dest_size, const char *fmt, ...) {
    PRESET_CHECK_NULL(dest, error);
    PRESET_CHECK_NULL(fmt, error);

    if (dest_size == 0) {
        return 0;
    }

    va_list args;
    va_start(args, fmt);
    int result = vsnprintf(dest, dest_size, fmt, args);
    va_end(args);

    /* 确保终止 */
    dest[dest_size - 1] = '\0';

    return result;

error:
    return -1;
}

/* ============================================================
 * 数组工具实现
 * ============================================================ */

uint32_t lv00_hash_int_array(const int *arr, int count) {
    if (arr == NULL || count <= 0) {
        return 0;
    }

    /* FNV-1a 哈希算法 */
    uint32_t hash = 2166136261U;
    for (int i = 0; i < count; i++) {
        hash ^= (uint32_t) arr[i];
        hash *= 16777619U;
    }
    return hash;
}

bool lv00_int_arrays_equal(const int *a, int count_a, const int *b, int count_b) {
    if (a == NULL || b == NULL) {
        return a == b; /* 都为空则相等 */
    }

    if (count_a != count_b) {
        return false;
    }

    for (int i = 0; i < count_a; i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

int *lv00_dup_int_array(const int *src, int count) {
    if (src == NULL || count <= 0) {
        return NULL;
    }

    /* 检查整数溢出 */
    if (count > INT_MAX / (int) sizeof(int)) {
        LV00_ERROR_SET(LV00_ERROR_OVERFLOW, "数组大小溢出: count=%d", count);
        return NULL;
    }

    size_t size = (size_t) count * sizeof(int);
    int *dup = (int *) lv00_malloc(size);
    if (dup == NULL) {
        LV00_ERROR_SET(LV00_ERROR_ALLOCATION_FAILED, "数组内存分配失败");
        return NULL;
    }

    memcpy(dup, src, size);
    return dup;
}

/* ============================================================
 * 验证函数实现
 * ============================================================ */

bool preset_validate_name(const char *name) {
    /* NULL 指针保护：必须在任何解引用之前检查 */
    if (!name)
        return false;
    if (name[0] == '\0') {
        return false;
    }

    size_t len = strlen(name);
    if (len > PRESET_MAX_NAME_LENGTH) {
        return false;
    }

    /* 名称必须以字母开头 */
    if (!isalpha((unsigned char) name[0])) {
        return false;
    }

    /* 只能包含字母、数字、下划线和连字符 */
    for (size_t i = 1; i < len; i++) {
        char c = name[i];
        if (!isalnum((unsigned char) c) && c != '_' && c != '-') {
            return false;
        }
    }

    return true;
}

bool preset_validate_description(const char *description) {
    if (description == NULL) {
        return false;
    }

    size_t len = strlen(description);
    if (len > PRESET_MAX_DESC_LENGTH) {
        return false;
    }

    return true;
}

bool preset_validate_type_combination(const PresetType *input_types, int input_count, PresetType output_type) {
    if (input_count < 0 || input_count > PRESET_MAX_INPUTS) {
        return false;
    }

    if (input_count > 0 && input_types == NULL) {
        return false;
    }

    /* 验证输入类型 */
    for (int i = 0; i < input_count; i++) {
        if (input_types[i] < 0 || input_types[i] >= PRESET_TYPE_COUNT) {
            return false;
        }
    }

    /* 验证输出类型 */
    if (output_type < 0 || output_type >= PRESET_TYPE_COUNT) {
        return false;
    }

    return true;
}

/* ============================================================
 * 类型转换实现
 * ============================================================ */

/* 类别名称映射表 */
static const struct {
    PresetCategory category;
    const char *name;
} g_category_map[] = {
    {PRESET_CATEGORY_CONSTRUCTION, "construction"},
    {PRESET_CATEGORY_MEASUREMENT, "measurement"},
    {PRESET_CATEGORY_TRANSFORMATION, "transformation"},
    {PRESET_CATEGORY_MATH_LOGIC, "predicate"},
    {PRESET_CATEGORY_COMBINATORICS, "combinatorics"},
    {PRESET_CATEGORY_ALGEBRA, "algebra"},
    {PRESET_CATEGORY_TOPOLOGY, "topology"},
    {PRESET_CATEGORY_LOGIC, "logic"},
    {PRESET_CATEGORY_SET_THEORY, "set_theory"},
    {PRESET_CATEGORY_NUMBER_THEORY, "number_theory"},
    {PRESET_CATEGORY_GROUP_THEORY, "group_theory"},
    {PRESET_CATEGORY_ANALYSIS, "analysis"},
    {PRESET_CATEGORY_GEOMETRY, "geometry"},
    {PRESET_CATEGORY_CUSTOM, "custom"},
};

/*
 * preset_category_to_string() 和 preset_category_from_string()
 * 已在 func_block_registry.c 中实现（包含中文名称映射）。
 * 此处注释掉以避免重复符号错误。
 * 声明保留在 preset_common.h 中供外部调用。
 */
#if 0
const char* preset_category_to_string(PresetCategory category) {
    for (size_t i = 0; i < sizeof(g_category_map) / sizeof(g_category_map[0]); i++) {
        if (g_category_map[i].category == category) {
            return g_category_map[i].name;
        }
    }
    return "unknown";
}

bool preset_category_from_string(const char *str, PresetCategory *category) {
    if (str == NULL || category == NULL) {
        return false;
    }
    
    for (size_t i = 0; i < sizeof(g_category_map) / sizeof(g_category_map[0]); i++) {
        if (strcmp(g_category_map[i].name, str) == 0) {
            *category = g_category_map[i].category;
            return true;
        }
    }
    
    return false;
}
#endif

/* 类型名称映射表（完整版，覆盖全部 PresetType 枚举值）
 *
 * 每个类型条目包含 PresetType 枚举值和对应的英文/标识符名称。
 * preset_type_to_string() 和 preset_type_from_string() 依赖此表。
 * 若在此添加新类型，需确保 preset_blocks.h 中的 PresetType 枚举同步更新。
 */
static const struct {
    PresetType type;
    const char *name;
} g_type_map[] = {
    /* 基础几何类型 */
    {PRESET_TYPE_POINT, "point"},
    {PRESET_TYPE_LINE, "line"},
    {PRESET_TYPE_LINE_SEGMENT, "segment"},
    {PRESET_TYPE_RAY, "ray"},
    {PRESET_TYPE_CIRCLE, "circle"},
    {PRESET_TYPE_POLYGON, "polygon"},
    {PRESET_TYPE_ANGLE, "angle"},
    /* 数值类型 */
    {PRESET_TYPE_SCALAR, "scalar"},
    {PRESET_TYPE_INTEGER, "integer"},
    {PRESET_TYPE_BOOLEAN, "boolean"},
    {PRESET_TYPE_STRING, "string"},
    /* 代数类型 */
    {PRESET_TYPE_VECTOR, "vector"},
    {PRESET_TYPE_MATRIX, "matrix"},
    {PRESET_TYPE_COMPLEX, "complex"},
    {PRESET_TYPE_POLYNOMIAL, "polynomial"},
    {PRESET_TYPE_EQUATION, "equation"},
    /* 集合与函数类型 */
    {PRESET_TYPE_SET, "set"},
    {PRESET_TYPE_FUNCTION, "function"},
    {PRESET_TYPE_TUPLE, "tuple"},
    {PRESET_TYPE_LIST, "list"},
    {PRESET_TYPE_SEQUENCE, "sequence"},
    /* 高级几何类型 */
    {PRESET_TYPE_REGION, "region"},
    {PRESET_TYPE_PATH, "path"},
    {PRESET_TYPE_SURFACE, "surface"},
    {PRESET_TYPE_SPACE, "space"},
    {PRESET_TYPE_MANIFOLD, "manifold"},
    {PRESET_TYPE_DISTANCE, "distance"},
    {PRESET_TYPE_AREA, "area"},
    {PRESET_TYPE_LENGTH, "length"},
    {PRESET_TYPE_CURVATURE, "curvature"},
    /* 代数结构类型 */
    {PRESET_TYPE_GROUP, "group"},
    {PRESET_TYPE_GROUP_ELEMENT, "group_element"},
    {PRESET_TYPE_SUBGROUP, "subgroup"},
    {PRESET_TYPE_HOMOMORPHISM, "homomorphism"},
    {PRESET_TYPE_COSET, "coset"},
    {PRESET_TYPE_PERMUTATION, "permutation"},
    {PRESET_TYPE_AUTOMORPHISM, "automorphism"},
    {PRESET_TYPE_RING, "ring"},
    {PRESET_TYPE_IDEAL, "ideal"},
    {PRESET_TYPE_FIELD, "field"},
    {PRESET_TYPE_MODULE, "module"},
    {PRESET_TYPE_ALGEBRA, "algebra"},
    {PRESET_TYPE_EXTENSION, "extension"},
    /* 分析学类型 */
    {PRESET_TYPE_LIMIT, "limit"},
    {PRESET_TYPE_DERIVATIVE, "derivative"},
    {PRESET_TYPE_INTEGRAL, "integral"},
    {PRESET_TYPE_SERIES, "series"},
    {PRESET_TYPE_LIMIT_EXPRESSION, "limit_expression"},
    /* 拓扑类型 */
    {PRESET_TYPE_TOPOLOGY, "topology"},
    {PRESET_TYPE_OPEN_SET, "open_set"},
    {PRESET_TYPE_CLOSED_SET, "closed_set"},
    /* 数论类型 */
    {PRESET_TYPE_PRIME, "prime"},
    {PRESET_TYPE_RESIDUE, "residue"},
    /* 概率与统计类型 */
    {PRESET_TYPE_DISTRIBUTION, "distribution"},
    {PRESET_TYPE_PROBABILITY, "probability"},
    /* 图论类型 */
    {PRESET_TYPE_GRAPH, "graph"},
    {PRESET_TYPE_TREE, "tree"},
    /* 逻辑与结构类型 */
    {PRESET_TYPE_FORMULA, "formula"},
    {PRESET_TYPE_EXPRESSION, "expression"},
    {PRESET_TYPE_STRUCTURE, "structure"},
    /* 通用类型 */
    {PRESET_TYPE_ANY, "any"},
};

const char *preset_type_to_string(PresetType type) {
    for (size_t i = 0; i < sizeof(g_type_map) / sizeof(g_type_map[0]); i++) {
        if (g_type_map[i].type == type) {
            return g_type_map[i].name;
        }
    }
    return "unknown";
}

bool preset_type_from_string(const char *str, PresetType *type) {
    if (str == NULL || type == NULL) {
        return false;
    }

    for (size_t i = 0; i < sizeof(g_type_map) / sizeof(g_type_map[0]); i++) {
        if (strcmp(g_type_map[i].name, str) == 0) {
            *type = g_type_map[i].type;
            return true;
        }
    }

    return false;
}

const char *preset_complexity_to_string(const char *complexity) {
    /* 复杂度已经是字符串形式，直接返回 */
    return complexity ? complexity : "unknown";
}

/* 属性标志名称映射表条目类型（具名结构体，支持 sizeof 运算） */
typedef struct {
    PresetProperty flag;
    const char *name;
} PresetPropertyMapEntry;

static const PresetPropertyMapEntry g_property_map[] = {
    {PRESET_PROPERTY_DETERMINISTIC, "deterministic"}, {PRESET_PROPERTY_CONTINUOUS, "continuous"},
    {PRESET_PROPERTY_CONSTRUCTIVE, "constructive"},   {PRESET_PROPERTY_INVOLUTIVE, "reversible"},
    {PRESET_PROPERTY_IDEMPOTENT, "idempotent"},       {PRESET_PROPERTY_ASSOCIATIVE, "associative"},
    {PRESET_PROPERTY_COMMUTATIVE, "commutative"},
};

int preset_properties_to_string(PresetProperty properties, char *buffer, size_t buffer_size) {
    if (buffer == NULL || buffer_size == 0) {
        return -1;
    }

    buffer[0] = '\0';
    size_t pos = 0;
    bool first = true;

    for (size_t i = 0; i < sizeof(g_property_map) / sizeof(g_property_map[0]); i++) {
        if (properties & g_property_map[i].flag) {
            if (!first) {
                if (pos < buffer_size - 1) {
                    buffer[pos++] = '|';
                    buffer[pos] = '\0';
                }
            }
            first = false;

            const char *name = g_property_map[i].name;
            size_t name_len = strlen(name);

            if (pos + name_len < buffer_size - 1) {
                memcpy(buffer + pos, name, name_len);
                pos += name_len;
                buffer[pos] = '\0';
            }
        }
    }

    return (int) pos;
}

bool preset_properties_from_string(const char *str, PresetProperty *properties) {
    if (str == NULL || properties == NULL) {
        return false;
    }

    *properties = 0;

    /* 复制字符串以便分割 */
    char *copy = lv00_strdup(str);
    if (copy == NULL) {
        return false;
    }

    /* 跨平台字符串分割：Windows 使用 strtok_s，POSIX 使用 strtok_r */
#ifdef _WIN32
    char *context = NULL;
    char *token = strtok_s(copy, "|,& ", &context);
#else
    char *saveptr = NULL;
    char *token = strtok_r(copy, "|,& ", &saveptr);
#endif

    while (token != NULL) {
        bool found = false;
        for (size_t i = 0; i < sizeof(g_property_map) / sizeof(g_property_map[0]); i++) {
            if (strcmp(g_property_map[i].name, token) == 0) {
                *properties |= g_property_map[i].flag;
                found = true;
                break;
            }
        }

        if (!found) {
            free(copy);
            return false;
        }

#ifdef _WIN32
        token = strtok_s(NULL, "|,& ", &context);
#else
        token = strtok_r(NULL, "|,& ", &saveptr);
#endif
    }

    free(copy);
    return true;
}
