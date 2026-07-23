/**
 * @file lv_api_spec.h
 * @brief Lv-00 API 版式规范 —— 命名 / 可见性 / 生命周期 约定
 *
 * @details 本文件定义 Lv-00 内核 API 的强制性编码规范。
 *          所有新增代码必须遵守本规范；存量代码逐步迁移。
 *
 * @author Lv-00 Project
 * @version 1.1.0
 */
#ifndef lv_API_SPEC_H
#define lv_API_SPEC_H

#ifdef __cplusplus
extern "C" {
#endif

/* lv_PUBLIC_API 兜底定义：如果未包含 lv.h，则使用空定义 */
#ifndef lv_PUBLIC_API
#define lv_PUBLIC_API
#endif

/* ============================================================
 * 一、可见性修饰符
 * ============================================================ */

/** 公开 API（跨层或外部可调用） */
#define lv_API        lv_PUBLIC_API

/** 模块内部 API（同层内可见，通过 static 限制链接作用域） */
#define lv_INTERNAL   static

/** 已废弃 API（编译器会发出警告） */
#ifdef __GNUC__
  #define lv_DEPRECATED(msg) __attribute__((deprecated(msg)))
#elif defined(_MSC_VER)
  #define lv_DEPRECATED(msg) __declspec(deprecated(msg))
#else
  #define lv_DEPRECATED(msg)
#endif

/* ============================================================
 * 二、命名规则（新增代码强制）
 * ============================================================
 *
 * 格式: lv_{layer_prefix}_{module}_{verb}_{noun}
 *
 * ┌─────────────┬──────────┬──────────────┐
 * │ 层级         │ 前缀     │ 领域          │
 * ├─────────────┼──────────┼──────────────┤
 * │ Layer 1     │ _l1_    │ Parser       │
 * │ Layer 2     │ _l2_    │ Resource     │
 * │ Layer 3     │ _l3_    │ Geometry     │
 * │ Layer 4     │ _l4_    │ Reasoning    │
 * │ Layer 5     │ _l5_    │ Output       │
 * │ Layer 6     │ _l6_    │ Visual       │
 * │ Layer 7     │ _l7_    │ Orchestration│
 * │ Layer 8     │ _l8_    │ MetaVerify   │
 * │ Layer 9     │ _l9_    │ Application  │
 * │ Layer 10    │ _l0_    │ Interop      │
 * └─────────────┴──────────┴──────────────┘
 *
 * 跨层通用模块（工具函数/内存/错误/协议）保留 lv_ 前缀：
 *   lv_malloc / lv_free / lv_init / lv_cleanup
 *   lv_proto_draw_commands / lv_trust_color_name 等
 *
 * 示例：
 *   旧: graph_get_node_count(graph)
 *   新: lv_l3_graph_node_count(graph)
 *
 *   旧: proof_navigator_add_step(nav, step)
 *   新: lv_l4_proof_add_step(nav, step)
 *
 *   旧: type_system_create()
 *   新: lv_l4_typesys_create()
 *
 *   旧: interop_export_geojson(graph, config)
 *   新: lv_l0_export_geojson(graph, config)
 *
 *   旧: func_block_create(name)
 *   新: lv_l4_funcblock_create(name)
 *
 * 遵守要点:
 *   - 全部使用 snake_case
 *   - 禁止 camelCase / PascalCase 函数名
 *   - 类型名（typedef）使用 PascalCase: lvLayerMessage
 *   - 宏和常量使用 UPPER_SNAKE_CASE: lv_MAX_NODES
 */

/* ============================================================
 * 三、生命周期规则
 * ============================================================
 *
 * create → destroy（堆分配对象，返回指针）
 *   示例: lvSolver *lv_l4_solver_create() → lv_l4_solver_destroy(lvSolver *)
 *
 * init → cleanup（嵌入结构体初始化，接收已分配内存的指针）
 *   示例: lv_l4_proof_breakpoint_init(Storage *) → lv_l4_proof_breakpoint_cleanup(Storage *)
 *
 * 禁止使用 _free 后缀:
 *   groebner_result_destroy() 应改为 groebner_result_destroy()
 *
 * destroy / cleanup 必须接收它所操作的指针所属类型，
 * 并对 NULL 输入保持安全（no-op）。
 */

/* ============================================================
 * 四、返回值约定
 * ============================================================
 *
 * 跨层 API:           统一返回 lvResult
 * 同层内部查询:       返回指针 (NULL = 不存在)
 * 同层内部成功/失败:   返回 int (0 = 成功) 或 bool
 *
 * 错误传播:           使用 lv_PROPAGATE(result) 宏
 *                     参见 lv_error_codes.h → lvResult 定义
 */

#ifdef __cplusplus
}
#endif
#endif /* lv_API_SPEC_H */
