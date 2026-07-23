/**
 * @file proof.c
 * @brief 证明系统实现 —— 命题管理与证明工作流
 *
 * @details 实现命题和证明步骤的管理，包括证明导航器、爆炸原理、
 *          证明依赖链、引理块折叠、断点保存/恢复和交互式证明步骤。
 *          支持导出 HTML、LaTeX 和 Coq 格式的证明文档。
 *
 *          核心功能模块：
 *          - 命题管理：创建、销毁、端口设置、模式附加、子命题树
 *          - 合一检查：三层匹配（端口类型 -> 约束类型 -> 坐标等价）
 *          - 证明步骤：创建、依赖管理、颜色评估、断点标记
 *          - 证明导航器：步进、跳转、断点跳转、最终颜色计算
 *          - 依赖链：树形依赖传播与颜色叠加
 *          - 爆炸原理：ex falso quodlibet 函数块构造
 *          - 等价变换：命题间的图变换声明
 *          - 自底定义：bottom 公理包可定义性检查
 *          - 导出：HTML/LaTeX/Coq 格式的证明文档生成
 *          - 引理块：折叠与展开的视图状态管理
 *
 *          信任颜色系统（从低到高优先级）：
 *          - 绿色（GREEN）：纯构造性证明，无外部依赖
 *          - 黄色（YELLOW）：直接依赖绿色步骤，简单的演绎推理
 *          - 蓝色（BLUE）：未探索的证明路径
 *          - 琥珀色（AMBER）：依赖数值近似或非精确计算
 *          - 浅橙色（ORANGE_ORACLE）：依赖外部预言机
 *          - 浅橙色（ORANGE_EX_FALSO）：使用爆炸原理
 *          - 深橙色（DARK_ORANGE）：同时依赖预言机和爆炸原理
 *
 * @author Lv-00 Project
 * @version 3.3.0
 *
 * @dependencies
 *   - proof.h              : 证明系统公共接口定义
 *   - lv_internal.h      : 内部数据结构与常量
 *   - lv_utils.h         : 统一内存分配器
 *   - type_system.h        : 类型系统（端口类型等价检查）
 *   - unify.h              : 合一检查器
 *   - solver.h             : 代数求解器
 *   - axiom_pkg.h          : 公理包定义
 *   - engine.h             : 引擎实例上下文
 *   - stream.h             : 流式事件输出
 *   - constraint_graph.h   : 约束图接口
 *   - normalization.h      : 图规范化
 */

#include "lv/proof.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#include "axiom_pkg.h"
#include "lv/engine.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/node_deep_copy.h"
#include "lv/solver.h"
#include "lv/stream.h"
#include "stream_context_util.h"
#include "lv/thread_pool.h"
#include "lv/type_system.h"
#include "lv/unify.h"

lv_DECLARE_STREAM_CTX(proof);

void proof_set_stream_context(StreamContext *ctx) {
    proof_stream_ctx = ctx;
}

/* 证明树 API 的完整实现在 proof/proof_tree.c 中；
 * proof_trace.h 中提供了 lvProofTree / lvProofTreeNode 的完整定义 */
#define lv_DEFAULT_MAX_STEPS 10000

/** 命题销毁时迭代栈的初始容量 */
#define PROOF_DESTROY_STACK_INITIAL_CAPACITY 128

/* ============== 命题管理API ============== */

/**
 * @brief 创建命题实例
 *
 * 分配并初始化一个 Proposition 结构体，设置 ID 和类型。
 * 默认颜色状态为 PROOF_COLOR_BLUE_UNEXPLORED（蓝色未探索）。
 *
 * @param id   命题唯一标识符
 * @param type 命题类型（公理、定理、引理、推论、猜想、反例等）
 * @return 新分配的 Proposition 指针，失败返回 NULL
 */
Proposition *proposition_create(int id, PropositionType type) {
    if (proof_stream_ctx) {
        stream_emit_simple(proof_stream_ctx, STREAM_EVENT_INFO, "命题创建", 0);
    }

    Proposition *prop = lv_calloc(1, sizeof(Proposition));
    if (!prop)
        return NULL;

    prop->id = id;
    prop->type = type;
    prop->color = PROOF_COLOR_BLUE_UNEXPLORED; /* 默认蓝色未探索 */
    prop->ref_count = 1; /* 创建时引用计数为1 */

    /* 初始化时间戳 */
    prop->created_at = time(NULL);
    prop->last_modified = prop->created_at;

    return prop;
}


/**
 * 增加命题引用计数
 */
void proposition_ref(Proposition *prop) {
    if (prop)
        prop->ref_count++;
}

/**
 * 减少命题引用计数，当计数为0时销毁
 */
void proposition_unref(Proposition *prop) {
    if (!prop)
        return;
    if (prop->ref_count > 0)
        prop->ref_count--;
    if (prop->ref_count == 0)
        proposition_destroy(prop);
}

/** */

// ── 子模块已拆分至 proof/ ──
