/**
 * @file magic_domain.c
 * @brief 领域系统与辅助工具实现
 *
 * @details 从 magic.c 拆分的子模块（Lv-00 项目 v3.3.0+）。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "magic_internal.h"
#include "magic.h"
#include "lv/lv_lifecycle.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_json.h"
#include "lv/lv_xmacro.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/lv_strbuf.h"

/* ============================================================
 * 领域系统
 * ============================================================ */

/** 领域规则结构体 */
typedef struct {
    char *pattern;   /* 规则匹配模式 */
    double priority; /* 规则优先级（数值越小优先级越高） */
    int action;      /* 规则动作类型 */
} DomainRule;

/** 领域结构体：定义一个魔法作用区域 */
struct Domain {
    char *name;            /* 领域名称 */
    int range;             /* 作用范围 */
    SymbolicCoord *center; /* 领域中心坐标 */
    bool active;           /* 是否激活 */
    double strength;       /* 领域强度 */

    /* 规则系统 */
    DomainRule *rules; /* 规则动态数组 */
    int rule_count;    /* 当前规则数量 */
    int rule_capacity; /* 规则数组容量 */
};

/**
 * @brief 创建领域
 *
 * 创建一个未激活的领域，初始强度为 0.0。
 *
 * @param name  领域名称，为 NULL 时使用 "Unnamed Domain"
 * @param range 领域作用范围
 * @return 新创建的领域指针，失败返回 NULL
 */
Domain *domain_create(const char *name, int range) {
    Domain *domain = (Domain *) lv_calloc(1, sizeof(Domain));
    if (!domain)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "domain_create: domain calloc failed");

    /* 分配领域名称，检查内存分配是否成功 */
    domain->name = name ? lv_strdup_safe(name) : lv_strdup_safe("Unnamed Domain");
    if (!domain->name) {
        lv_free((void **) &domain);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "domain_create: name strdup failed");
    }

    /* 初始化领域属性：范围、中心、激活状态、强度 */
    domain->range = range;
    domain->center = NULL;
    domain->active = false;
    domain->strength = 0.0;

    /* 初始化规则系统：初始为空，按需分配 */
    domain->rules = NULL;
    domain->rule_count = 0;
    domain->rule_capacity = 0;

    return domain;
}

/**
 * @brief 销毁领域并释放所有关联资源
 *
 * 释放领域的名称、中心坐标以及领域结构体本身。
 * 释放后指针会被自动置为 NULL。可安全传入 NULL。
 *
 * @param domain 待销毁的领域指针
 */
/* ── domain_destroy 子资源销毁适配 ── */

LV_DESTROY_SHIM(destroy_domain_center, SymbolicCoord, symbolic_coord_destroy)

/* DomainRule 元素：释放模式字符串（rules 为值数组，非指针数组，
 * 不适用 lv_FIELD_ARRAY 的指针数组语义） */
static void destroy_domain_rules(void *obj, void *field_ptr) {
    (void) field_ptr;
    Domain *domain = (Domain *) obj;
    if (domain->rules) {
        for (int i = 0; i < domain->rule_count; i++) {
            if (domain->rules[i].pattern)
                lv_free((void **) &domain->rules[i].pattern);
        }
        lv_free((void **) &domain->rules);
    }
}

/* domain_destroy 字段描述表：name 纯指针、center 对象销毁、
 * rules 逐元素销毁（CUSTOM，值数组语义） */
static const lvFieldDesc s_domain_destroy_fields[] = {
    lv_FIELD_PLAIN(Domain, name),
    lv_FIELD_OBJECT(Domain, center, destroy_domain_center),
    lv_FIELD_CUSTOM(Domain, rules, destroy_domain_rules),
};

void domain_destroy(Domain *domain) {
    if (!domain)
        return;
    lv_obj_destroy_fields(domain, s_domain_destroy_fields,
                          sizeof(s_domain_destroy_fields) / sizeof(s_domain_destroy_fields[0]));
    lv_free((void **) &domain);
}

/**
 * @brief 向领域添加规则
 *
 * 将规则添加到领域的规则数组中。如果存在相同 pattern 的规则则跳过。
 * 插入后按优先级排序（数值越小优先级越高）。
 *
 * @param domain     领域指针
 * @param rule_name  规则名称/模式
 * @param priority   规则优先级
 * @return 成功返回 true，domain 为 NULL 或内存分配失败返回 false
 */
bool domain_add_rule(Domain *domain, const char *rule_name, double priority) {
    if (!domain || !rule_name)
        return false;

    /* 检查重复规则（相同 pattern 视为已存在，跳过） */
    for (int i = 0; i < domain->rule_count; i++) {
        if (domain->rules[i].pattern && strcmp(domain->rules[i].pattern, rule_name) == 0) {
            return true; /* 已存在，视为成功 */
        }
    }

    /* 规则数组容量不足时自动扩容（统一委托 lv_ensure_capacity，内部含溢出检查与倍增；0 → 8 与原一致） */
    if (!lv_ensure_capacity((void **) &domain->rules, domain->rule_count, &domain->rule_capacity,
                            sizeof(DomainRule), 0))
        return false;

    /* 添加新规则并设置默认动作 */
    int idx = domain->rule_count;
    domain->rules[idx].pattern = lv_strdup_safe(rule_name);
    if (!domain->rules[idx].pattern)
        return false;
    domain->rules[idx].priority = priority;
    domain->rules[idx].action = 0; /* 默认动作 */
    domain->rule_count++;

    /* 按优先级排序（数值越小优先级越高，使用简单冒泡排序） */
    for (int i = domain->rule_count - 1; i > 0; i--) {
        if (domain->rules[i].priority < domain->rules[i - 1].priority) {
            DomainRule tmp = domain->rules[i];
            domain->rules[i] = domain->rules[i - 1];
            domain->rules[i - 1] = tmp;
        } else {
            break; /* 尾部已排好序，提前退出 */
        }
    }

    return true;
}

/**
 * @brief 激活领域
 *
 * 设置领域中心坐标并激活领域，强度初始化为 1.0。
 * 如果领域已有中心坐标，会先销毁旧的。
 *
 * @param domain 领域指针
 * @param center 领域中心坐标
 * @return 激活成功返回 true，domain 为 NULL 返回 false
 */
bool domain_activate(Domain *domain, SymbolicCoord *center) {
    if (!domain)
        return false;

    /* 替换已有的中心坐标 */
    if (domain->center) {
        symbolic_coord_destroy(domain->center);
    }
    /* 深拷贝中心坐标并激活领域 */
    domain->center = symbolic_coord_copy(center);
    domain->active = true;
    domain->strength = MAGIC_DOMAIN_ACTIVATION_STRENGTH;

    return true;
}

/**
 * @brief 停用领域
 *
 * 将领域设为非激活状态，强度归零。
 *
 * @param domain 领域指针
 * @return 停用成功返回 true，domain 为 NULL 返回 false
 */
bool domain_deactivate(Domain *domain) {
    if (!domain)
        return false;
    /* 设置为非激活状态，强度归零 */
    domain->active = false;
    domain->strength = 0.0;
    return true;
}

/**
 * @brief 检查领域是否处于激活状态
 *
 * @param domain 领域指针
 * @return 激活返回 true，domain 为 NULL 返回 false
 */
bool domain_is_active(const Domain *domain) {
    return domain ? domain->active : false;
}

/**
 * @brief 获取领域强度
 *
 * @param domain 领域指针
 * @return 领域强度值，domain 为 NULL 时返回 0.0
 */
double domain_get_strength(const Domain *domain) {
    return domain ? domain->strength : 0.0;
}

/**
 * @brief 获取领域名称
 *
 * @param domain 领域指针
 * @return 领域名称字符串，domain 为 NULL 时返回 NULL
 */
const char *domain_get_name(const Domain *domain) {
    return domain ? domain->name : NULL;
}

/**
 * @brief 获取领域作用范围
 *
 * @param domain 领域指针
 * @return 领域作用范围值，domain 为 NULL 时返回 0
 */
int domain_get_range(const Domain *domain) {
    return domain ? domain->range : 0;
}

/**
 * @brief 获取领域中心坐标
 *
 * @param domain 领域指针
 * @return 领域中心坐标指针（所有权仍归领域），domain 为 NULL 或未设置时返回 NULL
 */
SymbolicCoord *domain_get_center(const Domain *domain) {
    return domain ? domain->center : NULL;
}

/* ============================================================
 * 辅助工具实现
 * ============================================================ */

/* ================================================================
 * 枚举 -> 名称 映射表（数据表化，替代 switch/索引数组）
 * 使用 lv_enum_to_str 二分查找（表须按枚举值升序排列）
 * ================================================================ */

/* ── 字符串↔枚举 X-macro 列表 ── */

#define MAGIC_ELEMENT_X(x) \
    x(ELEMENT_FIRE, "FIRE") \
    x(ELEMENT_WATER, "WATER") \
    x(ELEMENT_AIR, "AIR") \
    x(ELEMENT_EARTH, "EARTH") \
    x(ELEMENT_ETHER, "ETHER") \
    x(ELEMENT_NONE, "NONE")

#define MAGIC_STAGE_X(x) \
    x(SPELL_STAGE_MOLDING, "开模") \
    x(SPELL_STAGE_PURIFYING, "提纯") \
    x(SPELL_STAGE_INFUSING, "灌注") \
    x(SPELL_STAGE_RELEASING, "释放")

#define MAGIC_STATUS_X(x) \
    x(SPELL_STATUS_IDLE, "空闲") \
    x(SPELL_STATUS_CASTING, "施法中") \
    x(SPELL_STATUS_SUCCESS, "成功") \
    x(SPELL_STATUS_FAILED, "失败") \
    x(SPELL_STATUS_BACKLASH, "反噬")

#define MAGIC_REACTION_X(x) \
    x(ELEMENT_REACTION_NONE, "无反应") \
    x(ELEMENT_REACTION_ENHANCE, "增强") \
    x(ELEMENT_REACTION_WEAKEN, "削弱") \
    x(ELEMENT_REACTION_CONFLICT, "冲突")

#define MAGIC_RESTRICTION_X(x) \
    x(RESTRICTION_NONE, "无限制") \
    x(RESTRICTION_LIMITED, "限制级") \
    x(RESTRICTION_CONTROLLED, "管制级") \
    x(RESTRICTION_FORBIDDEN, "禁术级") \
    x(RESTRICTION_ABSOLUTE, "绝对禁术")

/** @brief element_to_string 名称表（按枚举值升序）
 *  @note 原实现为 names[] 索引表；此处按原表的运行时映射原样迁移，
 *        保持既有输出与边界行为（未命中返回 "未知"）不变。 */
static const lvStrToEnumEntry s_element_to_string_entries[] = {
    lv_XMACRO_TO_ENUM_TABLE(MAGIC_ELEMENT_X)
};

/** @brief stage_to_string 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_stage_to_string_entries[] = {
    lv_XMACRO_TO_ENUM_TABLE(MAGIC_STAGE_X)
};

/** @brief status_to_string 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_status_to_string_entries[] = {
    lv_XMACRO_TO_ENUM_TABLE(MAGIC_STATUS_X)
};

/** @brief reaction_to_string 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_reaction_to_string_entries[] = {
    lv_XMACRO_TO_ENUM_TABLE(MAGIC_REACTION_X)
};

/** @brief restriction_to_string 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_restriction_to_string_entries[] = {
    lv_XMACRO_TO_ENUM_TABLE(MAGIC_RESTRICTION_X)
};

/**
 * @brief 将魔法元素枚举值转换为中文字符串
 *
 * @param element 魔法元素类型
 * @return 元素的中文名称字符串，无效值时返回 "未知"
 */
const char *element_to_string(MagicElement element) {
    return lv_enum_to_str(s_element_to_string_entries, lv_ARRAY_SIZE(s_element_to_string_entries), (int) element,
                          "未知");
}

/**
 * @brief 将字符串转换为魔法元素枚举值
 *
 * 支持中文名称和英文名称两种格式。
 *
 * @param str 元素名称字符串（如 "FIRE"、"火"）
 * @return 对应的魔法元素类型，无法识别时返回 ELEMENT_NONE
 */
#define MAGIC_ELEMENT_STR_X(x) \
    x(ELEMENT_FIRE, "FIRE") \
    x(ELEMENT_FIRE, "火") \
    x(ELEMENT_WATER, "WATER") \
    x(ELEMENT_WATER, "水") \
    x(ELEMENT_AIR, "AIR") \
    x(ELEMENT_AIR, "风") \
    x(ELEMENT_EARTH, "EARTH") \
    x(ELEMENT_EARTH, "土") \
    x(ELEMENT_ETHER, "ETHER") \
    x(ELEMENT_ETHER, "以太")

/** @brief string_to_element 字符串→元素查找表（中文名 + 英文名双写，替代 5 分支 strcmp 链） */
static const lvStrToEnumEntry s_string_to_element_entries[] = {
    lv_XMACRO_TO_ENUM_TABLE(MAGIC_ELEMENT_STR_X)
};

MagicElement string_to_element(const char *str) {
    if (!str)
        return ELEMENT_NONE;

    /* 中英文名称查表（未命中回退 ELEMENT_NONE） */
    return (MagicElement) lv_str_to_enum(s_string_to_element_entries, lv_ARRAY_SIZE(s_string_to_element_entries),
                                         str, ELEMENT_NONE);
}

/**
 * @brief 将施法阶段枚举值转换为中文字符串
 *
 * @param stage 施法阶段
 * @return 阶段的中文名称字符串，无效值时返回 "未知"
 */
const char *stage_to_string(SpellStage stage) {
    return lv_enum_to_str(s_stage_to_string_entries, lv_ARRAY_SIZE(s_stage_to_string_entries), (int) stage, "未知");
}

/**
 * @brief 将咒语状态枚举值转换为中文字符串
 *
 * @param status 咒语状态
 * @return 状态的中文名称字符串，无效值时返回 "未知"
 */
const char *status_to_string(SpellStatus status) {
    return lv_enum_to_str(s_status_to_string_entries, lv_ARRAY_SIZE(s_status_to_string_entries), (int) status, "未知");
}

/**
 * @brief 将元素反应枚举值转换为中文字符串
 *
 * @param reaction 元素反应类型
 * @return 反应类型的中文名称字符串，无效值时返回 "未知"
 */
const char *reaction_to_string(ElementReaction reaction) {
    return lv_enum_to_str(s_reaction_to_string_entries, lv_ARRAY_SIZE(s_reaction_to_string_entries), (int) reaction,
                          "未知");
}

/**
 * @brief 将限制等级枚举值转换为中文字符串
 *
 * @param level 限制等级
 * @return 限制等级的中文名称字符串，无效值时返回 "未知"
 */
const char *restriction_to_string(RestrictionLevel level) {
    return lv_enum_to_str(s_restriction_to_string_entries, lv_ARRAY_SIZE(s_restriction_to_string_entries), (int) level,
                          "未知");
}
