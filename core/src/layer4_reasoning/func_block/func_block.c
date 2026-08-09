/**
 * @file func_block.c
 * @brief 函数块核心实现
 * @details 实现函数块的创建、销毁、打包、深拷贝等核心管理 API。
 *          确定性检查见 func_block_determinism.c，
 *          例化与捕获避免见 func_block_instantiate.c，
 *          序列化/反序列化见 func_block_serialize.c。
 *
 * INTERNAL NOTE: 本文件使用 goto 清理路径模式（14 处）。
 *   对于错误清理场景这是 C 语言惯用法，不做修改。
 *   若新增代码应考虑拆分超过 200 行的函数。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "func_block.h"
#include "lv/lv_xmacro.h"

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/solver.h"
#include "lv/stream.h"

#include "func_block_internal.h"
#include "lv_internal.h"
#include "lv/lv_lifecycle.h"
#include "lv_utils.h"
#include "stream_context_util.h"

/* ==================== 命名常量 ==================== */

/** 函数块默认容量（使用 lv_ARRAY_GROWTH_FACTOR 作为扩容增长因子） */
#define FUNC_BLOCK_DEFAULT_CAPACITY 8
/**
 * 最大函数块局部变量数量上限，用于防止 VLA / 栈分配数组溢出。
 * 当打包操作中 internal_count + input_count + output_count 的总和
 * 超过此上限时，拒绝分配，返回 PACK_RESULT_OUT_OF_MEMORY。
 */
#define lv_MAX_FB_LOCAL_VARS 1024

/*
 * 流式上下文定义（非 static，供其他子模块文件 extern 引用）
 *
 * 注意：此处无法使用 lv_DECLARE_STREAM_CTX 宏，原因如下：
 * 该宏展开后会生成 static lv_THREAD_LOCAL 变量，作用域仅限于当前编译单元。
 * 而函数块模块的流式上下文需要被 func_block_instantiate.c、
 * func_block_determinism.c、func_block_serialize.c 等子模块文件通过 extern 引用，
 * 因此必须使用非 static 的线程局部变量手动声明。
 */
LV_STREAM_CTX_DEFINE(func_block);

/* ============== 内部辅助函数 ============== */

/**
 * @brief 通用整数数组深拷贝设置函数
 * @param target 目标数组指针的地址
 * @param count 目标计数变量的地址
 * @param values 源数组
 * @param n 源数组元素个数
 * @return 成功返回 true，内存不足返回 false
 *
 * 该函数封装了函数块系统中常见的"释放旧数组、深拷贝新数组"模式，
 * 避免在多个设置函数中重复相同的逻辑。
 */
static bool func_block_set_int_array(int **target, int *count, const int *values, int n) {
    if (!target || !count)
        return false;

    /* 释放旧数组 */
    lv_free((void **) target);
    *count = 0;

    /* 空数组直接返回 */
    if (n <= 0 || !values)
        return true;

    /* 分配新数组并深拷贝 */
    int *copy = (int *) lv_malloc((size_t) n * sizeof(int));
    if (!copy)
        return false;

    for (int i = 0; i < n; i++) {
        copy[i] = values[i];
    }

    *target = copy;
    *count = n;
    return true;
}

/* ============== 函数块管理API ============== */

/**
 * @brief 创建函数块
 *
 * 分配并初始化一个 FuncBlock 结构体。新块的初始状态：
 * - 确定性状态设为 DETERMINISM_STATE_UNVERIFIED（未经检验）
 * - 内部节点数组、输入/输出端口数组、端口依赖数组均为空
 * - 选择器、名称、描述、测度比较函数均为 NULL
 * - 视图状态设为 FB_VIEW_STATE_EXPANDED（展开）
 *
 * @param id 函数块唯一标识符
 * @return 成功返回 FuncBlock 指针（调用方负责通过 func_block_destroy 释放），
 *         内存不足时返回 NULL
 */
/**
 * @brief 创建函数块（v3.4.2 增强版）
 *
 * 分配并初始化一个 FuncBlock 结构体。新块的初始状态：
 * - 确定性状态设为 DETERMINISM_STATE_UNVERIFIED（未经检验）
 * - 内部节点数组、输入/输出端口数组、端口依赖数组均为空
 * - 选择器、名称、描述、测度比较函数均为 NULL
 * - 视图状态设为 FB_VIEW_STATE_EXPANDED（展开）
 * - 版本号设为当前库版本（v3.4.2）
 *
 * @param id 函数块唯一标识符
 * @return 成功返回 FuncBlock 指针（调用方负责通过 func_block_destroy 释放），
 *         内存不足时返回 NULL
 */
FuncBlock *func_block_create(int id) {
    FuncBlock *fb = (FuncBlock *) lv_calloc(1, sizeof(FuncBlock));
    if (!fb) {
        lv_LOG_ERROR("func_block_create: 内存分配失败 (id=%d)", id);
        return NULL;
    }

    /* 设置基本属性 */
    fb->id = id;
    fb->determinism = DETERMINISM_STATE_UNVERIFIED;
    fb->view_state = FB_VIEW_STATE_EXPANDED;

    /* 初始化测度相关字段 */
    fb->has_measure = false;
    fb->measure_node_id = -1;
    fb->measure_compare = NULL;

    /* v3.4.2: 设置版本号 */
    fb->version_major = 3;
    fb->version_minor = 4;
    fb->version_patch = 2;

    /* v3.4.2: 初始化生命周期追踪字段 */
    fb->is_instantiated = false;

    /* 初始化容量字段 */
    fb->port_dep_capacity = 0;

    lv_LOG_DEBUG("func_block_create: 创建函数块 id=%d, version=%d.%d.%d", id, fb->version_major, fb->version_minor,
                 fb->version_patch);

    return fb;
}

/* selector_destroy() 实现在 func_block_selector.c 中，通过 func_block.h 声明可见 */

/**
 * @brief 销毁函数块
 *
 * 释放 FuncBlock 结构体及其所有动态分配的成员：
 * - internal_node_ids（内部节点ID数组）
 * - input_port_ids / output_port_ids（输入/输出端口ID数组）
 * - port_deps（端口依赖数组）
 * - precondition_region_ids（前置条件区域ID数组）
 * - selector（选择器对象，通过 selector_destroy 递归释放）
 * - name / description（名称和描述字符串）
 *
 * 参数为 NULL 时安全返回（no-op），无需调用方判空。
 *
 * @param fb 函数块指针，可为 NULL
 */
void func_block_destroy(FuncBlock *fb) {
    if (!fb)
        return;
    lv_free((void **) &fb->internal_node_ids);
    lv_free((void **) &fb->input_port_ids);
    lv_free((void **) &fb->output_port_ids);
    lv_free((void **) &fb->port_deps);
    lv_free((void **) &fb->precondition_region_ids);
    if (fb->selector) {
        selector_destroy(fb->selector);
        /* 修复：释放后置 NULL，防止悬空指针风险。
         * 虽然 fb 本身即将被释放，但防御性编程可避免未来重构引入 use-after-free */
        fb->selector = NULL;
    }
    lv_free((void **) &fb->name);
    lv_free((void **) &fb->description);
    lv_free((void **) &fb);
}

/**
 * @brief 设置函数块的内部节点列表
 *
 * 内部节点是函数块"封装"的几何节点——这些节点位于块内部，
 * 外部不可见。函数块作为这些节点的抽象边界。
 *
 * 操作逻辑：
 * - 释放旧的 internal_node_ids 数组（如果存在）
 * - 调用 dup_int_array 深拷贝 node_ids 到新数组
 * - 更新 internal_node_count
 *
 * 【设计模式】以下三个 setter 函数（set_internal_nodes / set_input_ports /
 * set_output_ports）遵循相同的"验证+委托"模式：
 *   1. 检查 fb 非空且 count >= 0
 *   2. 委托给 func_block_set_int_array 完成实际的深拷贝
 * 若未来字段增多，可考虑用宏或内联辅助函数消除重复，但当前三处
 * 的可读性和类型安全性已足够好，保持显式写法更利于调试。
 *
 * @param fb      函数块指针（不可为 NULL）
 * @param node_ids 内部节点 ID 数组（可为 NULL，当 count=0 时）
 * @param count   节点数量
 * @return true  设置成功
 * @return false fb 为 NULL 或 count<0 或内存分配失败
 */
bool func_block_set_internal_nodes(FuncBlock *fb, const int *node_ids, int count) {
    if (!fb || count < 0)
        return false;
    return func_block_set_int_array(&fb->internal_node_ids, &fb->internal_node_count, node_ids, count);
}

/**
 * @brief 设置函数块的输入端口列表
 *
 * 输入端口是函数块从外部接收数据/坐标的入口。当函数块被例化时，
 * 外部节点通过绑定到这些端口向块内传递几何信息。
 *
 * 操作逻辑：
 * - 释放旧的 input_port_ids 数组（如果存在）
 * - 调用 dup_int_array 深拷贝 port_ids 到新数组
 * - 更新 input_count
 *
 * @param fb       函数块指针（不可为 NULL）
 * @param port_ids 输入端口 ID 数组（可为 NULL，当 count=0 时）
 * @param count    端口数量
 * @return true  设置成功
 * @return false fb 为 NULL 或 count<0 或内存分配失败
 */
bool func_block_set_input_ports(FuncBlock *fb, const int *port_ids, int count) {
    if (!fb || count < 0)
        return false;
    return func_block_set_int_array(&fb->input_port_ids, &fb->input_count, port_ids, count);
}

/**
 * @brief 设置函数块的输出端口列表
 *
 * 输出端口是函数块向外部暴露计算结果的出口。当函数块被执行后，
 * 输出端口上的节点被解析/构造完成，供外部引用。
 *
 * 操作逻辑：
 * - 释放旧的 output_port_ids 数组（如果存在）
 * - 调用 dup_int_array 深拷贝 port_ids 到新数组
 * - 更新 output_count
 *
 * @param fb       函数块指针（不可为 NULL）
 * @param port_ids 输出端口 ID 数组（可为 NULL，当 count=0 时）
 * @param count    端口数量
 * @return true  设置成功
 * @return false fb 为 NULL 或 count<0 或内存分配失败
 */
bool func_block_set_output_ports(FuncBlock *fb, const int *port_ids, int count) {
    if (!fb || count < 0)
        return false;
    return func_block_set_int_array(&fb->output_port_ids, &fb->output_count, port_ids, count);
}

/**
 * @brief 设置函数块名称
 *
 * @param fb   函数块指针（不可为 NULL）
 * @param name 名称字符串（可为 NULL，表示清除名称）
 * @return true  设置成功
 * @return false 失败
 */
bool func_block_set_name(FuncBlock *fb, const char *name) {
    if (!fb)
        return false;
    lv_free((void **) &fb->name);
    if (name && name[0] != '\0') {
        fb->name = lv_strdup(name);
        if (!fb->name)
            return false;
    }
    return true;
}

/**
 * @brief 设置函数块描述
 *
 * @param fb          函数块指针（不可为 NULL）
 * @param description 描述字符串（可为 NULL，表示清除描述）
 * @return true  设置成功
 * @return false 失败
 */
bool func_block_set_description(FuncBlock *fb, const char *description) {
    if (!fb)
        return false;
    lv_free((void **) &fb->description);
    if (description && description[0] != '\0') {
        fb->description = lv_strdup(description);
        if (!fb->description)
            return false;
    }
    return true;
}

/* ============== Getter 函数实现 ============== */

/**
 * @brief 获取输入端口数量
 *
 * 安全访问函数块的输入端口计数。支持 NULL 安全检查。
 *
 * @param fb 函数块指针（可为 NULL）
 * @return 输入端口数量，fb 为 NULL 时返回 0
 */
int func_block_get_input_count(const FuncBlock *fb) {
    return fb ? fb->input_count : 0;
}

/**
 * @brief 获取输出端口数量
 *
 * 安全访问函数块的输出端口计数。支持 NULL 安全检查。
 *
 * @param fb 函数块指针（可为 NULL）
 * @return 输出端口数量，fb 为 NULL 时返回 0
 */
int func_block_get_output_count(const FuncBlock *fb) {
    return fb ? fb->output_count : 0;
}

/**
 * @brief 获取内部节点数量
 *
 * 安全访问函数块的内部节点计数。支持 NULL 安全检查。
 *
 * @param fb 函数块指针（可为 NULL）
 * @return 内部节点数量，fb 为 NULL 时返回 0
 */
int func_block_get_internal_count(const FuncBlock *fb) {
    return fb ? fb->internal_node_count : 0;
}

/**
 * @brief 获取函数块ID
 *
 * 安全访问函数块的唯一标识符。支持 NULL 安全检查。
 *
 * @param fb 函数块指针（可为 NULL）
 * @return 函数块ID，fb 为 NULL 时返回 -1
 */
int func_block_get_id(const FuncBlock *fb) {
    return fb ? fb->id : -1;
}

/**
 * @brief 获取确定性状态
 *
 * 安全访问函数块的确定性状态。支持 NULL 安全检查。
 * 确定性状态用于判断函数块是否产生唯一解。
 *
 * @param fb 函数块指针（可为 NULL）
 * @return 确定性状态，fb 为 NULL 时返回 DETERMINISM_STATE_UNVERIFIED
 */
DeterminismState func_block_get_determinism(const FuncBlock *fb) {
    return fb ? fb->determinism : DETERMINISM_STATE_UNVERIFIED;
}

/**
 * @brief 获取函数块名称
 *
 * 安全访问函数块的名称字符串。返回的字符串是只读的，
 * 调用者不应修改或释放。
 *
 * @param fb 函数块指针（可为 NULL）
 * @return 名称字符串（只读），fb 为 NULL 或无名称时返回 NULL
 */
const char *func_block_get_name(const FuncBlock *fb) {
    return fb ? fb->name : NULL;
}

/**
 * @brief 获取函数块描述
 *
 * 安全访问函数块的描述字符串。返回的字符串是只读的，
 * 调用者不应修改或释放。
 *
 * @param fb 函数块指针（可为 NULL）
 * @return 描述字符串（只读），fb 为 NULL 或无描述时返回 NULL
 */
const char *func_block_get_description(const FuncBlock *fb) {
    return fb ? fb->description : NULL;
}

/**
 * @brief 设置函数块的选择器
 *
 * @param fb       函数块指针（不可为 NULL）
 * @param selector 选择器指针（可为 NULL，表示清除选择器）
 * @return true  设置成功
 * @return false 失败
 */
bool func_block_set_selector(FuncBlock *fb, SolutionSelector *selector) {
    if (!fb)
        return false;
    if (fb->selector) {
        selector_destroy(fb->selector);
    }
    fb->selector = selector;
    return true;
}

/**
 * @brief 添加端口依赖
 *
 * @param fb  函数块指针（不可为 NULL）
 * @param dep 端口依赖指针（不可为 NULL）
 * @return true  添加成功
 * @return false 失败
 */
/**
 * @brief 添加端口依赖（v3.4.2 安全增强版）
 *
 * @param fb  函数块指针（不可为 NULL）
 * @param dep 端口依赖指针（不可为 NULL）
 * @return true  添加成功
 * @return false 失败
 *
 * @note v3.4.2 改进：
 * - 使用安全整数运算防止溢出
 * - 改进错误处理和日志记录
 * - 添加 size_t 溢出检查
 */
bool func_block_add_port_dependency(FuncBlock *fb, PortDependency *dep) {
    if (!fb || !dep) {
        lv_LOG_ERROR("func_block_add_port_dependency: 无效参数 (fb=%p, dep=%p)", (void *) fb, (void *) dep);
        return false;
    }

    /* 使用指数扩容策略，避免每次添加依赖都触发 realloc */
    if (fb->port_dep_count >= fb->port_dep_capacity) {
        /* v3.4.2 起改用统一扩容设施（内部含 INT_MAX/SIZE_MAX 溢出检查与倍增策略） */
        if (!lv_ensure_capacity((void **) &fb->port_deps, fb->port_dep_count, &fb->port_dep_capacity,
                                sizeof(PortDependency), 1)) {
            lv_LOG_ERROR("func_block_add_port_dependency: 扩容失败 (count=%d)", fb->port_dep_count);
            return false;
        }
    }

    fb->port_deps[fb->port_dep_count] = *dep;
    fb->port_dep_count++;
    return true;
}

/**
 * @brief 设置函数块的前置条件
 *
 * @param fb        函数块指针（不可为 NULL）
 * @param region_ids 前置条件区域 ID 数组（可为 NULL，当 count=0 时）
 * @param count     区域数量
 * @return true  设置成功
 * @return false 失败
 */
bool func_block_set_preconditions(FuncBlock *fb, const int *region_ids, int count) {
    if (!fb || count < 0)
        return false;
    return func_block_set_int_array(&fb->precondition_region_ids, &fb->precondition_count, region_ids, count);
}

/* ============== 跨边界检测 ============== */

/**
 * @brief 检测跨边界约束
 *
 * @param graph               约束图
 * @param internal_node_ids   内部节点 ID 数组
 * @param internal_count     内部节点数量
 * @param out_conflicts      输出冲突数组
 * @param out_conflict_count 输出冲突数量
 * @return true  检测到冲突，false 未检测到或参数无效
 */
bool func_block_detect_cross_boundary(ConstraintGraph *graph, const int *internal_node_ids, int internal_count,
                                      CrossBoundaryConstraint **out_conflicts, int *out_conflict_count) {
    if (!graph || !internal_node_ids || !out_conflicts || !out_conflict_count) {
        if (out_conflict_count)
            *out_conflict_count = 0;
        if (out_conflicts)
            *out_conflicts = NULL;
        return false;
    }

    /* 使用 constraint_graph.h 中定义的 find_cross_boundary_constraints 函数 */
    /* 但我们需要合并端口节点到内部节点集合中 */
    CrossBoundaryConstraint *conflicts =
        find_cross_boundary_constraints(graph, internal_node_ids, internal_count, NULL, 0, out_conflict_count);

    *out_conflicts = conflicts;

    if (*out_conflict_count > 0 && func_block_stream_ctx) {
        stream_emit_simple(func_block_stream_ctx, STREAM_EVENT_FUNC_BLOCK_CROSS_BOUNDARY, "检测到跨边界端口依赖", -1);
    }

    return *out_conflict_count > 0;
}

/* ============== 跨边界约束处理 ============== */

/**
 * @brief 检查并处理跨边界约束
 *
 * 1. 调用 find_cross_boundary_constraints() 获取跨边界约束列表
 * 2. 对每条约束，检查其类型：
 *    - CONNECTION 类型：允许（端口连接在封装后自动由边界端口接管）
 *    - 其他类型（INCIDENCE/BETWEENNESS/INTERSECTION/CONTAINMENT）：
 *      通过 callback 询问用户处理方式
 * 3. 根据用户选择执行对应操作
 *
 * @param graph 约束图
 * @param internal_ids 内部节点 ID 数组
 * @param internal_count 内部节点数量
 * @param port_ids 端口 ID 数组
 * @param port_count 端口数量
 * @param error_msg 输出：错误消息缓冲区（可选）
 * @param error_size 错误消息缓冲区大小
 * @return lv_OK 成功（无跨边界约束或全部已处理），其他错误码
 */
static int handle_cross_boundary_constraints(ConstraintGraph *graph, const int *internal_ids, int internal_count,
                                             const int *port_ids, int port_count, char *error_msg, int error_size) {
    if (!graph || !internal_ids || internal_count <= 0)
        return lv_OK;

    /* 合并内部节点和端口 ID 用于跨边界检测 */
    int partial = lv_SAFE_ADD(internal_count, port_count > 0 ? port_count : 0, INT_MAX);
    if (partial == INT_MAX)
        return lv_ERROR_OVERFLOW;
    int total_bound = partial;

    int *bound_ids = NULL;
    if (total_bound > 0) {
        /* 检查数组大小乘法溢出 */
        if ((size_t) total_bound > SIZE_MAX / sizeof(int))
            return lv_ERROR_OVERFLOW;
        bound_ids = (int *) lv_malloc((size_t) total_bound * sizeof(int));
        if (!bound_ids)
            return lv_ERROR_OUT_OF_MEMORY;
        int bidx = 0;
        for (int i = 0; i < internal_count; i++)
            bound_ids[bidx++] = internal_ids[i];
        if (port_ids && port_count > 0) {
            for (int i = 0; i < port_count; i++)
                bound_ids[bidx++] = port_ids[i];
        }
    }

    int count = 0;
    CrossBoundaryConstraint *cbs = find_cross_boundary_constraints(graph, bound_ids, total_bound, NULL, 0, &count);

    if (bound_ids)
        lv_free((void **) &bound_ids);

    if (!cbs || count == 0)
        return lv_OK;

    int result = lv_OK;
    for (int i = 0; i < count; i++) {
        /* CONNECTION 约束自动允许（端口连接在封装后被边界端口接管） */
        if (cbs[i].type == CONNECTION)
            continue;

        /* 非连接约束需要用户决策 */
        if (error_msg && error_size > 0) {
            snprintf(error_msg, error_size,
                     "跨边界%d: 约束#%d (type=%d) 涉及外部节点#%d，"
                     "请选择: promote/disconnect/cancel",
                     i, cbs[i].constraint_id, (int) cbs[i].type, cbs[i].node_ids[0]);
        }
        result = lv_ERROR_UNKNOWN; /* 需要用户介入 */
    }

    lv_free((void **) &cbs);
    return result;
}

/* ================================================================
 * 查找表：CrossBoundaryAction → 处理函数（execute_boundary_action）
 * ================================================================ */

/** @brief 跨边界动作处理函数类型（用于 execute_boundary_action） */
typedef int (*BoundaryActionHandler)(ConstraintGraph *graph, int constraint_id);

/** @brief 处理 DISCONNECT 动作：删除约束 */
static int boundary_action_disconnect(ConstraintGraph *graph, int constraint_id) {
    return graph_deactivate_constraint(graph, constraint_id);
}
/** @brief 处理 PROMOTE 动作：保留约束 */
static int boundary_action_promote(ConstraintGraph *graph, int constraint_id) {
    (void) graph;
    (void) constraint_id;
    return lv_OK;
}
/** @brief 处理 CANCEL 动作：取消打包 */
static int boundary_action_cancel(ConstraintGraph *graph, int constraint_id) {
    (void) graph;
    (void) constraint_id;
    return lv_ERROR_CANCELLED;
}

/**
 * @brief CrossBoundaryAction 处理函数查找表（execute_boundary_action）
 *
 * 索引：CROSS_BOUNDARY_PROMOTE=0, CROSS_BOUNDARY_DISCONNECT=1, CROSS_BOUNDARY_CANCEL=2
 */
static const BoundaryActionHandler s_boundary_action_handlers[] = {
    boundary_action_promote,    /* CROSS_BOUNDARY_PROMOTE */
    boundary_action_disconnect, /* CROSS_BOUNDARY_DISCONNECT */
    boundary_action_cancel      /* CROSS_BOUNDARY_CANCEL */
};
#define lv_BOUNDARY_ACTION_HANDLER_COUNT lv_ARRAY_SIZE(s_boundary_action_handlers)

/**
 * @brief 执行跨边界约束的处理动作
 *
 * @param graph 约束图
 * @param constraint_id 约束 ID
 * @param action 处理动作
 * @return lv_OK 成功，其他错误码
 */
int execute_boundary_action(ConstraintGraph *graph, int constraint_id, CrossBoundaryAction action) {
    if (!graph)
        return lv_ERROR_INVALID_PARAM;

    if ((unsigned) action < lv_BOUNDARY_ACTION_HANDLER_COUNT) {
        return s_boundary_action_handlers[action](graph, constraint_id);
    }
    return lv_ERROR_CANCELLED;
}

/* ============== 跨边界回调上下文 ============== */

/** 跨边界回调上下文结构体：封装回调和用户数据，确保线程安全 */
typedef struct {
    CrossBoundaryCallback callback; /**< 回调函数指针 */
    void *user_data;                /**< 回调用户数据 */
} CrossBoundaryCallbackContext;

static lv_THREAD_LOCAL CrossBoundaryCallbackContext g_cross_boundary_ctx = {NULL, NULL};

/* ================================================================
 * 查找表：CrossBoundaryAction → 打包内部处理函数（文件作用域）
 * ================================================================ */

/** @brief 打包内部跨边界动作处理上下文 */
typedef struct {
    ConstraintGraph *graph;
    CrossBoundaryConstraint *conflicts;
    int *bound_ids;
    int constraint_idx;
} PackBoundaryContext;

/** @brief 打包内部处理函数类型 */
typedef bool (*PackBoundaryActionHandler)(PackBoundaryContext *ctx);

/** @brief 打包内部：处理 CANCEL */
static bool pack_boundary_cancel(PackBoundaryContext *ctx) {
    lv_free((void **) &ctx->conflicts);
    lv_free((void **) &ctx->bound_ids);
    return false; /* 调用者根据返回值判断中断 */
}
/** @brief 打包内部：处理 DISCONNECT */
static bool pack_boundary_disconnect(PackBoundaryContext *ctx) {
    for (int ci = 0; ci < ctx->graph->constraint_count; ci++) {
        if (ctx->graph->constraints[ci]->id == ctx->conflicts[ctx->constraint_idx].constraint_id) {
            graph_remove_constraint(ctx->graph, ci);
            break;
        }
    }
    return true; /* 继续处理 */
}
/** @brief 打包内部：处理 PROMOTE */
static bool pack_boundary_promote(PackBoundaryContext *ctx) {
    (void) ctx;
    return true; /* 继续处理 */
}

/**
 * @brief 打包内部跨边界动作处理函数查找表
 *
 * 索引：CROSS_BOUNDARY_PROMOTE=0, CROSS_BOUNDARY_DISCONNECT=1, CROSS_BOUNDARY_CANCEL=2
 */
static const PackBoundaryActionHandler s_pack_boundary_handlers[] = {
    pack_boundary_promote,    /* CROSS_BOUNDARY_PROMOTE */
    pack_boundary_disconnect, /* CROSS_BOUNDARY_DISCONNECT */
    pack_boundary_cancel      /* CROSS_BOUNDARY_CANCEL */
};
#define lv_PACK_BOUNDARY_HANDLER_COUNT lv_ARRAY_SIZE(s_pack_boundary_handlers)

/* ============== 打包操作 ============== */

/**
 * @brief 打包函数块
 *
 * 将内部节点、输入端口和输出端口打包为一个函数块。
 *
 * @param graph               约束图
 * @param internal_node_ids   内部节点 ID 数组
 * @param internal_count      内部节点数量
 * @param input_port_ids      输入端口 ID 数组
 * @param input_count         输入端口数量
 * @param output_port_ids     输出端口 ID 数组
 * @param output_count        输出端口数量
 * @param cross_boundary_actions 跨边界处理动作数组
 * @param cross_boundary_count   跨边界处理动作数量
 * @param out_func_block      输出参数，返回新创建的函数块
 * @return 打包结果状态码
 */
PackResult func_block_pack(ConstraintGraph *graph, const int *internal_node_ids, int internal_count,
                           const int *input_port_ids, int input_count, const int *output_port_ids, int output_count,
                           CrossBoundaryAction *cross_boundary_actions, int cross_boundary_count,
                           FuncBlock **out_func_block) {
    if (!graph || !out_func_block)
        return PACK_RESULT_INVALID_NODES;

    /* 流式事件：函数块打包开始 */
    if (func_block_stream_ctx) {
        stream_emit_simple(func_block_stream_ctx, STREAM_EVENT_FUNC_BLOCK_PACK_START, "函数块打包开始", 0);
    }

    /* 参数基本验证 */
    if (internal_count > 0 && !internal_node_ids)
        return PACK_RESULT_INVALID_NODES;
    if (input_count > 0 && !input_port_ids)
        return PACK_RESULT_INVALID_PORTS;
    if (output_count > 0 && !output_port_ids)
        return PACK_RESULT_INVALID_PORTS;

    /* 验证所有内部节点存在 */
    for (int i = 0; i < internal_count; i++) {
        GeomNode *n = graph_get_node(graph, internal_node_ids[i]);
        if (!n)
            return PACK_RESULT_INVALID_NODES;
    }

    /* 验证输入端口 */
    for (int i = 0; i < input_count; i++) {
        GeomNode *n = graph_get_node(graph, input_port_ids[i]);
        if (!n || n->type != GEOM_PORT)
            return PACK_RESULT_INVALID_PORTS;
    }

    /* 验证输出端口 */
    for (int i = 0; i < output_count; i++) {
        GeomNode *n = graph_get_node(graph, output_port_ids[i]);
        if (!n || n->type != GEOM_PORT)
            return PACK_RESULT_INVALID_PORTS;
    }

    /* 检测跨边界约束 */
    CrossBoundaryConstraint *conflicts = NULL;
    int conflict_count = 0;

    /* 合并内部节点和端口用于跨边界检测 */
    /* 使用安全加法宏防止 total_bound 计算整数溢出。
     * 注意：嵌套调用 lv_SAFE_ADD 时，若第一次加法溢出返回 INT_MAX，
     * 第二次再加第三个值会再次溢出仍返回 INT_MAX，因此必须逐级检查。
     * 修复：拆分为两次独立的安全加法，每次都检查溢出结果。 */
    int partial = lv_SAFE_ADD(internal_count, input_count, INT_MAX);
    if (partial == INT_MAX)
        return PACK_RESULT_OUT_OF_MEMORY;
    int total_bound = lv_SAFE_ADD(partial, output_count, INT_MAX);
    if (total_bound == INT_MAX)
        return PACK_RESULT_OUT_OF_MEMORY;

    /* 【修复】局部变量栈空间保护：防止打包规模超出系统安全上限，
     * 避免在后续处理中因 VLA 或栈数组过大导致栈溢出。 */
    if (total_bound > lv_MAX_FB_LOCAL_VARS) {
        lv_LOG_ERROR("func_block_pack: 局部变量总数 %d 超过上限 %d", total_bound, lv_MAX_FB_LOCAL_VARS);
        return PACK_RESULT_OUT_OF_MEMORY;
    }
    int *bound_ids = NULL;
    if (total_bound > 0) {
        bound_ids = lv_malloc((size_t) total_bound * sizeof(int));
        if (!bound_ids)
            return PACK_RESULT_OUT_OF_MEMORY;
        int bidx = 0;
        for (int i = 0; i < internal_count; i++)
            bound_ids[bidx++] = internal_node_ids[i];
        for (int i = 0; i < input_count; i++)
            bound_ids[bidx++] = input_port_ids[i];
        for (int i = 0; i < output_count; i++)
            bound_ids[bidx++] = output_port_ids[i];
        total_bound = bidx;
    }

    /* 使用 constraint_graph.h 的函数检测跨边界约束 */
    conflicts = find_cross_boundary_constraints(graph, bound_ids ? bound_ids : internal_node_ids,
                                                bound_ids ? total_bound : internal_count, NULL, 0, &conflict_count);

    if (conflict_count > 0) {
        /* 流式事件：打包过程中检测到跨边界约束 */
        if (func_block_stream_ctx) {
            stream_emit_simple(func_block_stream_ctx, STREAM_EVENT_FUNC_BLOCK_CROSS_BOUNDARY,
                               "打包检测到跨边界约束，开始处理", 0);
        }

        /* 统计非 CONNECTION 类型的跨边界约束数量 */
        int non_connection_count = 0;
        for (int i = 0; i < conflict_count; i++) {
            if (conflicts[i].type != CONNECTION)
                non_connection_count++;
        }

        /* CONNECTION 类型约束自动允许（端口连接在封装后由边界端口接管）*/
        /* 如果存在非 CONNECTION 类型约束，需要用户决策 */
        if (non_connection_count > 0) {
            bool can_process = false;

            if (cross_boundary_actions && cross_boundary_count >= non_connection_count) {
                can_process = true;
            } else if (g_cross_boundary_ctx.callback) {
                /* 使用回调为每条非 CONNECTION 约束获取处理方式 */
                int action_idx = 0;
                int *cb_actions = (int *) lv_malloc((size_t) non_connection_count * sizeof(int));
                if (cb_actions) {
                    for (int i = 0; i < conflict_count; i++) {
                        if (conflicts[i].type == CONNECTION)
                            continue;
                        CrossBoundaryResolution res = g_cross_boundary_ctx.callback(
                            conflicts[i].constraint_id, conflicts[i].type, conflicts[i].node_ids[0],
                            conflicts[i].node_ids[1], g_cross_boundary_ctx.user_data);
                        if (res.action == CROSS_BOUNDARY_CANCEL) {
                            lv_free((void **) &cb_actions);
                            lv_free((void **) &conflicts);
                            lv_free((void **) &bound_ids);
                            return PACK_RESULT_CANCELLED;
                        }
                        cb_actions[action_idx++] = (int) res.action;
                    }
                    cross_boundary_actions = (CrossBoundaryAction *) cb_actions;
                    cross_boundary_count = non_connection_count;
                    can_process = true;
                }
            }

            if (!can_process) {
                /* 存在跨边界约束但未提供足够的处理方式 */
                lv_free((void **) &conflicts);
                lv_free((void **) &bound_ids);
                return PACK_RESULT_CROSS_BOUNDARY_CONFLICT;
            }
        }

        /* 使用文件作用域查找表 s_pack_boundary_handlers 处理跨边界约束 */
        /* 处理每条跨边界约束（CONNECTION 类型自动跳过） */
        int action_idx = 0;
        for (int i = 0; i < conflict_count; i++) {
            /* CONNECTION 约束自动允许 */
            if (conflicts[i].type == CONNECTION)
                continue;

            CrossBoundaryAction action = cross_boundary_actions[action_idx++];
            if ((unsigned) action < lv_PACK_BOUNDARY_HANDLER_COUNT) {
                PackBoundaryContext ctx;
                ctx.graph = graph;
                ctx.conflicts = conflicts;
                ctx.bound_ids = bound_ids;
                ctx.constraint_idx = i;
                if (!s_pack_boundary_handlers[action](&ctx)) {
                    /* CANCEL 已释放 conflicts 和 bound_ids，直接返回 */
                    return PACK_RESULT_CANCELLED;
                }
            }
        }
        lv_free((void **) &conflicts);
    }
    lv_free((void **) &bound_ids);

    /* 在图中创建 GEOM_FUNCTION_BLOCK 节点 */
    AddNodeResult add_result = graph_add_function_block(graph, internal_node_ids, internal_count, input_port_ids,
                                                        input_count, output_port_ids, output_count);
    if (add_result != ADD_NODE_OK) {
        return PACK_RESULT_OUT_OF_MEMORY;
    }

    /* 函数块ID = 新创建节点的ID
     *
     * v10.0 修复：使用 graph_get_last_added_node_id() 公共接口替代脆弱的
     * graph->next_node_id - 1 内部实现假设。该接口由 constraint_graph.c 提供，
     * 确保无论内部实现如何变化都能正确获取最后添加的节点 ID。
     */
    int fb_id = graph_get_last_added_node_id(graph);
    if (fb_id < 0) {
        lv_LOG_ERROR("func_block_pack: graph_get_last_added_node_id() 返回 %d，无法推断函数块ID", fb_id);
        return PACK_RESULT_OUT_OF_MEMORY;
    }

    /* 创建 FuncBlock 结构 */
    PackResult pack_result = PACK_RESULT_OK; /* 统一错误处理：记录最终返回值 */
    FuncBlock *fb = func_block_create(fb_id);
    if (!fb) {
        pack_result = PACK_RESULT_OUT_OF_MEMORY;
        goto pack_cleanup;
    }

    if (!func_block_set_internal_nodes(fb, internal_node_ids, internal_count)) {
        pack_result = PACK_RESULT_OUT_OF_MEMORY;
        goto pack_cleanup;
    }
    if (!func_block_set_input_ports(fb, input_port_ids, input_count)) {
        pack_result = PACK_RESULT_OUT_OF_MEMORY;
        goto pack_cleanup;
    }
    if (!func_block_set_output_ports(fb, output_port_ids, output_count)) {
        pack_result = PACK_RESULT_OUT_OF_MEMORY;
        goto pack_cleanup;
    }

pack_cleanup:
    if (pack_result != PACK_RESULT_OK) {
        /* 统一错误处理：从图中移除已添加的节点，销毁函数块 */
        if (graph_remove_node(graph, fb_id) != REMOVE_NODE_OK) {
            lv_LOG_WARNING("func_block_pack: graph_remove_node(%d) 失败，图中可能残留无主节点", fb_id);
        }
        if (fb) {
            func_block_destroy(fb);
            fb = NULL;
        }
        return pack_result;
    }

    /* ---- 设计文档 3.2: 更新内部节点 ---- */
    /* namespace_depth 重计算：新深度 = 原深度 - 原上下文深度 + 1 */
    /* 原上下文深度取第一个内部节点的 namespace_depth 作为基准 */
    /* 如果内部节点有不同的上下文深度，以最小值为准 */
    int context_depth = 0;
    if (internal_count > 0) {
        GeomNode *first_node = graph_get_node(graph, internal_node_ids[0]);
        if (first_node) {
            context_depth = first_node->namespace_depth;
            /* 取所有内部节点的最小 namespace_depth 作为上下文深度 */
            for (int i = 1; i < internal_count; i++) {
                GeomNode *n = graph_get_node(graph, internal_node_ids[i]);
                if (n && n->namespace_depth < context_depth) {
                    context_depth = n->namespace_depth;
                }
            }
        }
    }

    for (int i = 0; i < internal_count; i++) {
        GeomNode *n = graph_get_node(graph, internal_node_ids[i]);
        if (n) {
            n->namespace_depth = n->namespace_depth - context_depth + 1;
            n->parent_block_id = fb_id;
        }
    }

    /* 输入端口：标记 is_formal_param=true，更新 namespace_depth 和 parent_block_id */
    for (int i = 0; i < input_count; i++) {
        GeomNode *n = graph_get_node(graph, input_port_ids[i]);
        if (n && n->type == GEOM_PORT && n->data.port) {
            n->data.port->parent_block_id = fb_id;
            n->data.port->is_formal_param = true;
            n->data.port->namespace_depth = n->data.port->namespace_depth - context_depth + 1;
            n->parent_block_id = fb_id;
            n->namespace_depth = n->namespace_depth - context_depth + 1;
        }
    }

    /* 输出端口：is_formal_param=false（输出不是形式参数），更新归属 */
    for (int i = 0; i < output_count; i++) {
        GeomNode *n = graph_get_node(graph, output_port_ids[i]);
        if (n && n->type == GEOM_PORT && n->data.port) {
            n->data.port->parent_block_id = fb_id;
            n->data.port->is_formal_param = false;
            n->data.port->namespace_depth = n->data.port->namespace_depth - context_depth + 1;
            n->parent_block_id = fb_id;
            n->namespace_depth = n->namespace_depth - context_depth + 1;
        }
    }

    /* 流式事件：函数块打包完成 */
    if (func_block_stream_ctx) {
        stream_emit_simple(func_block_stream_ctx, STREAM_EVENT_FUNC_BLOCK_PACK_DONE, "函数块打包完成", 0);
    }
    *out_func_block = fb;
    return PACK_RESULT_OK;
}

/* ============== 辅助函数 ============== */

/**
 * @brief 将确定性状态转换为字符串
 *
 * @param state 确定性状态枚举值
 * @return 对应的字符串表示
 */
/* ================================================================
 * 枚举 -> 名称 映射表（数据表化，替代 switch）
 * ================================================================ */

/** @brief determinism_state_to_string 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_determinism_state_to_string_entries[] = {
    {"UNVERIFIED", DETERMINISM_STATE_UNVERIFIED},
    {"VERIFIED", DETERMINISM_STATE_VERIFIED},
    {"NON_DETERMINISTIC", DETERMINISM_STATE_NON_DETERMINISTIC},
    {"PARTIALLY_VERIFIED", DETERMINISM_STATE_PARTIALLY_VERIFIED},
};

const char *determinism_state_to_string(DeterminismState state) {
    return lv_enum_to_str(s_determinism_state_to_string_entries, lv_ARRAY_SIZE(s_determinism_state_to_string_entries), (int) state, "UNKNOWN");
}

/**
 * @brief 将打包结果转换为字符串
 *
 * @param result 打包结果枚举值
 * @return 对应的字符串表示
 */
/** @brief pack_result_to_string 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_pack_result_to_string_entries[] = {
    {"OK", PACK_RESULT_OK},
    {"CROSS_BOUNDARY_CONFLICT", PACK_RESULT_CROSS_BOUNDARY_CONFLICT},
    {"INVALID_NODES", PACK_RESULT_INVALID_NODES},
    {"INVALID_PORTS", PACK_RESULT_INVALID_PORTS},
    {"OUT_OF_MEMORY", PACK_RESULT_OUT_OF_MEMORY},
    {"CANCELLED", PACK_RESULT_CANCELLED},
};

const char *pack_result_to_string(PackResult result) {
    return lv_enum_to_str(s_pack_result_to_string_entries, lv_ARRAY_SIZE(s_pack_result_to_string_entries), (int) result, "UNKNOWN");
}

/**
 * @brief 将例化结果转换为字符串
 *
 * @param result 例化结果枚举值
 * @return 对应的字符串表示
 */
/** @brief instantiate_result_to_string 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_instantiate_result_to_string_entries[] = {
    {"OK", INSTANTIATE_OK},
    {"NO_SOLUTION", INSTANTIATE_NO_SOLUTION},
    {"MULTIPLE_SOLUTIONS", INSTANTIATE_MULTIPLE_SOLUTIONS},
    {"SELECTOR_NEEDED", INSTANTIATE_SELECTOR_NEEDED},
    {"PRECONDITION_FAILED", INSTANTIATE_PRECONDITION_FAILED},
    {"OUT_OF_MEMORY", INSTANTIATE_OUT_OF_MEMORY},
};

const char *instantiate_result_to_string(InstantiateResult result) {
    return lv_enum_to_str(s_instantiate_result_to_string_entries, lv_ARRAY_SIZE(s_instantiate_result_to_string_entries), (int) result, "UNKNOWN");
}

/* ============== 视图折叠/展开 ============== */

/**
 * @brief 设置函数块的视图状态
 *
 * @param fb    函数块
 * @param state 视图状态
 */
void func_block_set_view_state(FuncBlock *fb, FuncBlockViewState state) {
    if (fb) {
        fb->view_state = state;
    }
}

/**
 * @brief 获取函数块的视图状态
 *
 * @param fb 函数块
 * @return 视图状态
 */
FuncBlockViewState func_block_get_view_state(const FuncBlock *fb) {
    if (!fb)
        return FB_VIEW_STATE_EXPANDED;
    return fb->view_state;
}

/* ============== 简化版打包API ============== */

/**
 * @brief 执行打包操作（简化版API）
 *
 * 使用 PackConfig 结构体简化参数传递，避免传递过多的独立参数。
 * 这是 func_block_pack 的包装函数，提供更友好的API接口。
 *
 * @param graph 约束图
 * @param config 打包配置
 * @param out_func_block 输出的函数块
 * @return 打包结果
 */
PackResult func_block_pack_ex(ConstraintGraph *graph, const PackConfig *config, FuncBlock **out_func_block) {
    /* 参数验证 */
    if (!graph || !config || !out_func_block) {
        lv_ERROR_RETURN(lv_ERROR_INVALID_PARAM, PACK_RESULT_INVALID_NODES,
                        "无效参数: graph=%p, config=%p, out_func_block=%p", (void *) graph, (void *) config,
                        (void *) out_func_block);
    }

    /* 验证必需参数 */
    if (!config->internal_node_ids || config->internal_count <= 0) {
        lv_ERROR_RETURN(lv_ERROR_INVALID_PARAM, PACK_RESULT_INVALID_NODES, "无效的内部节点: ids=%p, count=%d",
                        (void *) config->internal_node_ids, config->internal_count);
    }

    if (!config->input_port_ids || config->input_count < 0) {
        lv_ERROR_RETURN(lv_ERROR_INVALID_PARAM, PACK_RESULT_INVALID_PORTS, "无效的输入端口: ids=%p, count=%d",
                        (void *) config->input_port_ids, config->input_count);
    }

    if (!config->output_port_ids || config->output_count < 0) {
        lv_ERROR_RETURN(lv_ERROR_INVALID_PARAM, PACK_RESULT_INVALID_PORTS, "无效的输出端口: ids=%p, count=%d",
                        (void *) config->output_port_ids, config->output_count);
    }

    /* 准备跨边界约束处理参数 */
    CrossBoundaryAction *actions = (CrossBoundaryAction *) config->cross_boundary_actions;
    int action_count = config->cross_boundary_count;
    bool actions_allocated = false;

    /* ========== 跨边界约束预检查 ==========
     *
     * 在调用 func_block_pack 之前，先检查是否存在非 CONNECTION 类型的
     * 跨边界约束。CONNECTION 类型约束自动允许（由端口机制处理），
     * 其他类型需要通过回调询问用户处理方式（promote/disconnect/cancel）。
     *
     * 如果已存在 cross_boundary_actions 数组，则直接使用（跳过回调）。
     */
    if (!actions || action_count <= 0) {
        char error_buf[256];
        int cb_status = handle_cross_boundary_constraints(graph, config->internal_node_ids, config->internal_count,
                                                          config->input_port_ids, config->input_count, error_buf,
                                                          sizeof(error_buf));

        if (cb_status != lv_OK) {
            /* 存在非 CONNECTION 类型的跨边界约束，需要用户决策 */
            if (g_cross_boundary_ctx.callback) {
                /* 使用回调获取每条约束的处理方式 */
                int count = 0;
                CrossBoundaryConstraint *cbs =
                    find_cross_boundary_constraints(graph, config->internal_node_ids, config->internal_count,
                                                    config->input_port_ids, config->input_count, &count);

                if (cbs && count > 0) {
                    int non_conn_count = 0;
                    for (int i = 0; i < count; i++) {
                        if (cbs[i].type != CONNECTION)
                            non_conn_count++;
                    }

                    if (non_conn_count > 0) {
                        CrossBoundaryAction *cb_actions =
                            (CrossBoundaryAction *) lv_malloc((size_t) non_conn_count * sizeof(CrossBoundaryAction));
                        if (cb_actions) {
                            int act_idx = 0;
                            bool cancelled = false;
                            for (int i = 0; i < count; i++) {
                                if (cbs[i].type == CONNECTION)
                                    continue;
                                CrossBoundaryResolution res =
                                    g_cross_boundary_ctx.callback(cbs[i].constraint_id, cbs[i].type, cbs[i].node_ids[0],
                                                                  cbs[i].node_ids[1], g_cross_boundary_ctx.user_data);
                                if (res.action == CROSS_BOUNDARY_CANCEL) {
                                    cancelled = true;
                                    break;
                                }
                                cb_actions[act_idx++] = res.action;
                            }
                            if (cancelled) {
                                lv_free((void **) &cb_actions);
                                lv_free((void **) &cbs);
                                return PACK_RESULT_CANCELLED;
                            }
                            actions = cb_actions;
                            action_count = non_conn_count;
                            actions_allocated = true;
                        }
                    }
                    lv_free((void **) &cbs);
                }
            } else {
                /* 无回调，返回错误消息让调用者处理 */
                lv_ERROR_RETURN(lv_ERROR_UNKNOWN, PACK_RESULT_CROSS_BOUNDARY_CONFLICT, "跨边界约束检查失败: %s",
                                error_buf);
            }
        }
    }

    /* 调用传统API */
    PackResult result = func_block_pack(graph, config->internal_node_ids, config->internal_count,
                                        config->input_port_ids, config->input_count, config->output_port_ids,
                                        config->output_count, actions, action_count, out_func_block);

    /* 如果 actions 是本函数分配的，需要释放 */
    if (actions_allocated) {
        lv_free((void **) &actions);
    }

    /* 设置名称和描述（如果提供了） */
    if (result == PACK_RESULT_OK && *out_func_block) {
        if (config->name && config->name[0] != '\0') {
            func_block_set_name(*out_func_block, config->name);
        }
        if (config->description && config->description[0] != '\0') {
            func_block_set_description(*out_func_block, config->description);
        }
    }

    return result;
}


/* ============== 深拷贝 ============== */

/**
 * @brief 深拷贝函数块
 *
 * 创建一个函数块的完整深拷贝，包括所有动态分配的成员。
 * 拷贝后的函数块与原始函数块完全独立，修改其中一个不会影响另一个。
 *
 * 深拷贝内容包括：
 * - internal_node_ids（深拷贝数组）
 * - input_port_ids / output_port_ids（深拷贝数组）
 * - port_deps（深拷贝数组）
 * - selector（创建新的选择器并复制字段）
 * - name / description（lv_strdup）
 * - precondition_region_ids（深拷贝数组）
 * - has_measure / measure_node_id / measure_compare（直接复制）
 * - view_state（直接复制）
 * - determinism（直接复制）
 *
 * @param src 源函数块
 * @return 新创建的函数块副本，失败返回 NULL
 */
/** @brief 作用域守卫清理回调：销毁 FuncBlock 指针变量（配合 lv_DEFER 使用） */
static void defer_func_block_destroy(void *arg) {
    func_block_destroy(*(FuncBlock **) arg);
}

FuncBlock *func_block_copy(const FuncBlock *src) {
    if (!src)
        return NULL;

    /* 创建新函数块并复制基本字段 */
    FuncBlock *dst = func_block_create(src->id);
    if (!dst)
        return NULL;

    /* dst 创建后立即注册作用域守卫：任何一步失败直接 return，
     * 出口按注册逆序自动销毁 dst，无需手写 goto fail 标签。 */
    lv_DEFER(defer_func_block_destroy, &dst);

    /* 深拷贝内部节点 ID 数组 —— 收敛至公共 lv_copy_int_array */
    if (src->internal_node_count > 0 && src->internal_node_ids) {
        dst->internal_node_ids = lv_copy_int_array(src->internal_node_ids, src->internal_node_count);
        if (!dst->internal_node_ids)
            return NULL;
    }
    dst->internal_node_count = src->internal_node_count;

    /* 深拷贝输入端口 ID 数组 */
    if (src->input_count > 0 && src->input_port_ids) {
        dst->input_port_ids = lv_copy_int_array(src->input_port_ids, src->input_count);
        if (!dst->input_port_ids)
            return NULL;
    }
    dst->input_count = src->input_count;

    /* 深拷贝输出端口 ID 数组 */
    if (src->output_count > 0 && src->output_port_ids) {
        dst->output_port_ids = lv_copy_int_array(src->output_port_ids, src->output_count);
        if (!dst->output_port_ids)
            return NULL;
    }
    dst->output_count = src->output_count;

    /* 深拷贝端口依赖数组 */
    if (src->port_dep_count > 0 && src->port_deps) {
        dst->port_deps = lv_calloc((size_t) src->port_dep_count, sizeof(PortDependency));
        if (!dst->port_deps)
            return NULL;
        memcpy(dst->port_deps, src->port_deps, (size_t) src->port_dep_count * sizeof(PortDependency));
    }
    dst->port_dep_count = src->port_dep_count;
    dst->port_dep_capacity = src->port_dep_capacity;

    /* 深拷贝选择器 - 改进版：正确处理回调函数和 user_data */
    if (src->selector) {
        dst->selector = lv_calloc(1, sizeof(SolutionSelector));
        if (!dst->selector)
            return NULL;

        /* 复制基本字段 */
        dst->selector->type = src->selector->type;
        dst->selector->reference_node_id = src->selector->reference_node_id;
        dst->selector->custom_func = src->selector->custom_func;
        dst->selector->solution_count = src->selector->solution_count;
        dst->selector->current_index = src->selector->current_index;
        dst->selector->compare = src->selector->compare;
        dst->selector->on_select = src->selector->on_select;
        dst->selector->on_change = src->selector->on_change;
        dst->selector->graph = src->selector->graph;

        /* 深拷贝选择器名称 */
        if (src->selector->name) {
            dst->selector->name = lv_strdup(src->selector->name);
            if (!dst->selector->name)
                return NULL; /* 统一清理：func_block_destroy → selector_destroy 释放子字段 */
        }

        /* 深拷贝 solution_values 数组 */
        if (src->selector->solution_count > 0 && src->selector->solution_values) {
            dst->selector->solution_values = lv_malloc((size_t) src->selector->solution_count * sizeof(double));
            if (!dst->selector->solution_values)
                return NULL; /* 统一清理：selector_destroy 释放 name/solution_values/外壳 */
            memcpy(dst->selector->solution_values, src->selector->solution_values,
                   (size_t) src->selector->solution_count * sizeof(double));
        } else {
            dst->selector->solution_count = 0;
            dst->selector->solution_values = NULL;
        }

        /* user_data 处理：使用深拷贝回调（如果提供）否则浅拷贝 */
        if (src->selector->copy_user_data && src->selector->user_data) {
            /* 使用自定义深拷贝函数 */
            dst->selector->user_data = src->selector->copy_user_data(src->selector->user_data);
            dst->selector->free_user_data = src->selector->free_user_data;
            dst->selector->copy_user_data = src->selector->copy_user_data;
        } else {
            /* 浅拷贝 user_data，但记录原始指针用于追踪 */
            dst->selector->user_data = src->selector->user_data;
            dst->selector->free_user_data = NULL;
            dst->selector->copy_user_data = NULL;
        }
    }

    /* 深拷贝名称字符串 */
    if (src->name) {
        dst->name = lv_strdup(src->name);
        if (!dst->name)
            return NULL;
    }

    /* 深拷贝描述字符串 */
    if (src->description) {
        dst->description = lv_strdup(src->description);
        if (!dst->description)
            return NULL;
    }

    /* 深拷贝前置条件区域 ID 数组 —— 收敛至公共 lv_copy_int_array */
    if (src->precondition_count > 0 && src->precondition_region_ids) {
        dst->precondition_region_ids = lv_copy_int_array(src->precondition_region_ids, src->precondition_count);
        if (!dst->precondition_region_ids)
            return NULL;
    }
    dst->precondition_count = src->precondition_count;

    /* 直接复制值类型字段 */
    dst->determinism = src->determinism;
    dst->has_measure = src->has_measure;
    dst->measure_node_id = src->measure_node_id;
    dst->measure_compare = src->measure_compare;
    dst->view_state = src->view_state;

    /* 成功路径：dst 交由调用者接管，置 NULL 使作用域守卫不再销毁 */
    FuncBlock *result = dst;
    dst = NULL;
    return result;
}

/* ==================== 内部共享函数 ==================== */

bool collect_all_block_ids(const FuncBlock *fb, int **out_ids, int *out_count) {
    if (!fb || !out_ids || !out_count)
        return false;

    /* 计算总数：内部节点 + 输入端口 + 输出端口 */
    int total = fb->internal_node_count + fb->input_count + fb->output_count;
    if (total <= 0)
        return false;

    /* 检查整数溢出 */
    if (total > INT_MAX / (int) sizeof(int))
        return false;

    int *ids = lv_malloc((size_t) total * sizeof(int));
    if (!ids)
        return false;

    int count = 0;

    /* 收集内部节点ID */
    for (int i = 0; i < fb->internal_node_count; i++) {
        ids[count++] = fb->internal_node_ids[i];
    }

    /* 收集输入端口ID */
    for (int i = 0; i < fb->input_count; i++) {
        ids[count++] = fb->input_port_ids[i];
    }

    /* 收集输出端口ID */
    for (int i = 0; i < fb->output_count; i++) {
        ids[count++] = fb->output_port_ids[i];
    }

    *out_ids = ids;
    *out_count = count;
    return true;
}
