/**
 * @file lv_upper_api.h
 * @brief Lv-00 上层统一接口（upper unified API）
 *
 * 为外部绑定（GUI/Python/CLI）与未来接入点提供以「ID 句柄 + 内部对象表」
 * 模式管理的跨层便捷接口。所有函数返回 int64_t 句柄/计数：
 * - >= 0：成功（句柄/计数/写入字符数）
 * - < 0：负错误码（lv_error 体系，见 lv/lv_error.h）
 *
 * 实现位于 core/src/lv_impl_upper_*.c（lv_core 聚合库）。
 */

#ifndef lv_UPPER_API_H
#define lv_UPPER_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "lv/engine.h"
#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */

/* ============================================================
 * L6 可视化层（visual_editor 5 + view_synchronizer 3 + text_code 3）
 * ============================================================ */

/** 创建可视化编辑器实例，返回编辑器句柄 */
lv_PUBLIC_API int64_t visual_editor_create(lvEngine *ctx);
/** 渲染当前约束图到画布（执行可视化编辑器） */
lv_PUBLIC_API int64_t visual_editor_render(lvEngine *ctx, int64_t editor_id);
/** 更新编辑器中的节点位置（更新节点图坐标并重置执行） */
lv_PUBLIC_API int64_t visual_editor_update(lvEngine *ctx, int64_t editor_id, int64_t node_id, int64_t x, int64_t y);
/** 缩放画布（zoom_level 0-3 切换视图类型；>3 适配画布） */
lv_PUBLIC_API int64_t visual_editor_zoom(lvEngine *ctx, int64_t editor_id, int64_t zoom_level);
/** 销毁可视化编辑器 */
lv_PUBLIC_API int64_t visual_editor_destroy(lvEngine *ctx, int64_t editor_id);
/** 创建视图同步器 */
lv_PUBLIC_API int64_t view_synchronizer_create(lvEngine *ctx);
/** 同步两个视图（如文本视图与图形视图） */
lv_PUBLIC_API int64_t view_synchronizer_sync(lvEngine *ctx, int64_t sync_id, int64_t src_view, int64_t dst_view);
/** 销毁视图同步器 */
lv_PUBLIC_API int64_t view_synchronizer_destroy(lvEngine *ctx, int64_t sync_id);
/** 创建文本代码视图 */
lv_PUBLIC_API int64_t text_code_create(lvEngine *ctx);
/** 销毁文本代码视图 */
lv_PUBLIC_API int64_t text_code_destroy(lvEngine *ctx, int64_t view_id);
/** 设置文本代码视图内容 */
lv_PUBLIC_API int64_t text_code_set_text(lvEngine *ctx, int64_t view_id, const char *text);
/** 获取文本代码视图内容（无效 view_id 返回空串） */
lv_PUBLIC_API const char *text_code_get_text(lvEngine *ctx, int64_t view_id);

/* ============================================================
 * L3 几何扩展（geom_evol 3 + atp_backend 4 + proof_tptp 2）
 * ============================================================ */

/** 创建几何演化引擎（dim 维参数向量），返回引擎句柄 */
lv_PUBLIC_API int64_t geom_evol_create(lvEngine *ctx, int64_t dim);
/** 执行 steps 次单步演化，返回实际执行步数 */
lv_PUBLIC_API int64_t geom_evol_step(lvEngine *ctx, int64_t evol_id, int64_t steps);
/** 销毁几何演化引擎 */
lv_PUBLIC_API int64_t geom_evol_destroy(lvEngine *ctx, int64_t evol_id);
/** 从求解器名创建 ATP 后端，返回后端句柄 */
lv_PUBLIC_API int64_t atp_backend_create(lvEngine *ctx, const char *solver_name);
/** 向 ATP 后端提交证明任务，返回任务 ID */
lv_PUBLIC_API int64_t atp_backend_submit(lvEngine *ctx, int64_t backend_id, const char *conjecture);
/** 获取 ATP 任务结果：0=待处理 1=已证明 -1=反例 -2=超时/错误 */
lv_PUBLIC_API int64_t atp_backend_result(lvEngine *ctx, int64_t task_id);
/** 销毁 ATP 后端实例 */
lv_PUBLIC_API int64_t atp_backend_destroy(lvEngine *ctx, int64_t backend_id);
/** 将证明导出为 TPTP 格式，返回写入 buf 的字符数 */
lv_PUBLIC_API int64_t proof_tptp_export(lvEngine *ctx, int64_t proof_id, char *buf, int64_t buf_size);
/** 从 TPTP 输入验证证明，返回验证报告 ID */
lv_PUBLIC_API int64_t proof_tptp_verify(lvEngine *ctx, const char *tptp_input);

/* ============================================================
 * 代数族（preset_polynomial 等 14 个）
 * 声明式语义：代数对象以约束图节点表示，结果经求解器计算
 * ============================================================ */

/** 以系数数组创建多项式（coord[0]=次数，coord[1..]=系数），返回节点 ID */
lv_PUBLIC_API int64_t preset_polynomial_create(lvEngine *ctx, int64_t *coeffs, int64_t degree);
/** 在 x = x_num/x_den 处求多项式值（创建求值节点），返回节点 ID */
lv_PUBLIC_API int64_t preset_polynomial_evaluate(lvEngine *ctx, int64_t poly_id, int64_t x_num, int64_t x_den);
/** 多项式求根（创建根节点组），返回结果节点 ID */
lv_PUBLIC_API int64_t preset_polynomial_roots(lvEngine *ctx, int64_t poly_id);
/** 多项式加法（创建结果节点），返回节点 ID */
lv_PUBLIC_API int64_t preset_polynomial_add(lvEngine *ctx, int64_t p1_id, int64_t p2_id);
/** 多项式乘法（创建结果节点），返回节点 ID */
lv_PUBLIC_API int64_t preset_polynomial_mul(lvEngine *ctx, int64_t p1_id, int64_t p2_id);
/** 解方程（创建解节点），返回节点 ID */
lv_PUBLIC_API int64_t preset_equation_solve(lvEngine *ctx, int64_t equation_id);
/** 不等式检查（创建检查节点），返回节点 ID */
lv_PUBLIC_API int64_t preset_inequality_check(lvEngine *ctx, int64_t expr_id);
/** 计算多项式组的 Gröbner 基（创建结果节点），返回节点 ID */
lv_PUBLIC_API int64_t preset_groebner_basis(lvEngine *ctx, int64_t *poly_ids, int64_t count);
/** 获取多项式次数（创建次数节点），返回节点 ID */
lv_PUBLIC_API int64_t preset_polynomial_degree(lvEngine *ctx, int64_t poly_id);
/** 多项式求导（创建导数节点），返回节点 ID */
lv_PUBLIC_API int64_t preset_polynomial_derivative(lvEngine *ctx, int64_t poly_id);
/** 多项式积分（创建积分节点），返回节点 ID */
lv_PUBLIC_API int64_t preset_polynomial_integral(lvEngine *ctx, int64_t poly_id);
/** 解方程组（创建解节点组），返回节点 ID */
lv_PUBLIC_API int64_t preset_system_solve(lvEngine *ctx, int64_t *equation_ids, int64_t count);
/** 有理式化简（创建化简节点），返回节点 ID */
lv_PUBLIC_API int64_t preset_rational_simplify(lvEngine *ctx, int64_t expr_id);
/** 表达式化简（创建化简节点），返回节点 ID */
lv_PUBLIC_API int64_t preset_expression_simplify(lvEngine *ctx, int64_t expr_id);

/* ============================================================
 * L10 互操作层（导出包装）
 * ============================================================ */

/** 导出为 Coq（经 proof_navigator_create 接线 interop_export_coq），返回写入 buf 的字符数 */
lv_PUBLIC_API int64_t upper_interop_export_coq(lvEngine *ctx, int64_t proof_id, char *buf, int64_t buf_size);
/** 导出为 OPML 大纲（经 lv_opml_export_navigator 内存导出），返回写入 buf 的字符数 */
lv_PUBLIC_API int64_t interop_export_opml(lvEngine *ctx, int64_t session_id, char *buf, int64_t buf_size);
/** 导出为 GeoJSON，返回写入 buf 的字符数 */
lv_PUBLIC_API int64_t upper_interop_export_geojson(lvEngine *ctx, int64_t graph_id, char *buf, int64_t buf_size);
/** 导出为 SVG，返回写入 buf 的字符数 */
lv_PUBLIC_API int64_t upper_interop_export_svg(lvEngine *ctx, int64_t graph_id, char *buf, int64_t buf_size);
/** 导出为 TikZ，返回写入 buf 的字符数 */
lv_PUBLIC_API int64_t upper_interop_export_tikz(lvEngine *ctx, int64_t graph_id, char *buf, int64_t buf_size);
/** 导出为 Lean4（经 proof_navigator_create 接线 interop_export_lean），返回写入 buf 的字符数 */
lv_PUBLIC_API int64_t interop_export_lean4(lvEngine *ctx, int64_t proof_id, char *buf, int64_t buf_size);

/* ============================================================
 * func_block_preset 族（40 个）
 * ============================================================ */

/** 预设总数 */
lv_PUBLIC_API int64_t upper_func_block_preset_count(lvEngine *ctx);
/** 预设是否存在（1/0） */
lv_PUBLIC_API int64_t upper_func_block_preset_exists(lvEngine *ctx, const char *name);
/** 预设类别名（未知返回 "UNKNOWN"） */
lv_PUBLIC_API const char *func_block_preset_category_name(lvEngine *ctx, int64_t category);
/** 参数类型名（未知返回 "ANY"） */
lv_PUBLIC_API const char *func_block_preset_param_type_name(lvEngine *ctx, int64_t param_type);
/** 复杂度名（未知返回 "UNKNOWN"） */
lv_PUBLIC_API const char *func_block_preset_complexity_name(lvEngine *ctx, int64_t complexity);
/** 预设版本字符串（未找到返回 "0.0.0"） */
lv_PUBLIC_API const char *func_block_preset_version(lvEngine *ctx, const char *name);
/** 预设描述文本（未找到返回 fallback） */
lv_PUBLIC_API const char *func_block_preset_description(lvEngine *ctx, const char *name);
/** 预设数学定义（LaTeX，未找到返回 fallback） */
lv_PUBLIC_API const char *func_block_preset_definition(lvEngine *ctx, const char *name);
/** 预设的逆预设名称（无逆名返回 NULL） */
lv_PUBLIC_API const char *func_block_preset_inverse_name(lvEngine *ctx, const char *name);
/** 参数默认值描述（未找到返回 "N/A"） */
lv_PUBLIC_API const char *func_block_preset_default_value(lvEngine *ctx, const char *name, int64_t param_idx);
/** 预设输入参数数量 */
lv_PUBLIC_API int64_t func_block_preset_input_count(lvEngine *ctx, const char *name);
/** 预设输出参数数量 */
lv_PUBLIC_API int64_t func_block_preset_output_count(lvEngine *ctx, const char *name);
/** 预设前置条件数量 */
lv_PUBLIC_API int64_t func_block_preset_precondition_count(lvEngine *ctx, const char *name);
/** 预设后置条件数量 */
lv_PUBLIC_API int64_t func_block_preset_postcondition_count(lvEngine *ctx, const char *name);
/** 相关预设列表（写入 buf），返回写入字符数 */
lv_PUBLIC_API int64_t func_block_preset_related(lvEngine *ctx, const char *name, char *buf, int64_t buf_size);
/** 预设属性位掩码 */
lv_PUBLIC_API int64_t func_block_preset_properties(lvEngine *ctx, const char *name);
/** 预设是否具有指定属性（1/0） */
lv_PUBLIC_API int64_t func_block_preset_has_property(lvEngine *ctx, const char *name, int64_t property);
/** 参数名 → 参数下标（-1 未找到） */
lv_PUBLIC_API int64_t func_block_preset_param_index(lvEngine *ctx, const char *name, const char *param_name);
/** 预设是否可逆（1/0） */
lv_PUBLIC_API int64_t func_block_preset_is_reversible(lvEngine *ctx, const char *name);
/** 预设复杂度枚举值 */
lv_PUBLIC_API int64_t func_block_preset_complexity_enum(lvEngine *ctx, const char *name);
/** 参数是否可选（1/0） */
lv_PUBLIC_API int64_t func_block_preset_is_optional(lvEngine *ctx, const char *name, int64_t param_idx);
/** 预设约束数量 */
lv_PUBLIC_API int64_t func_block_preset_constraint_count(lvEngine *ctx, const char *name);
/** 预设注册时间戳 */
lv_PUBLIC_API int64_t func_block_preset_registration_time(lvEngine *ctx, const char *name);
/** 预设是否保留（1/0） */
lv_PUBLIC_API int64_t func_block_preset_is_reserved(lvEngine *ctx, const char *name);
/** 初始化预设库（1 成功） */
lv_PUBLIC_API int64_t func_block_preset_init(lvEngine *ctx);
/** 预设元数据 JSON（写入 buf），返回写入字符数 */
lv_PUBLIC_API int64_t func_block_preset_metadata(lvEngine *ctx, const char *name, char *buf, int64_t buf_size);
/** 实例化预设，返回实例句柄 */
lv_PUBLIC_API int64_t upper_func_block_preset_instantiate(lvEngine *ctx, const char *name, int64_t *input_ids, int64_t input_count);
/** 预设名列表（写入 buf），返回写入字符数 */
lv_PUBLIC_API int64_t upper_func_block_preset_list(lvEngine *ctx, char *buf, int64_t buf_size);
/** 组合两个预设，返回新预设句柄 */
lv_PUBLIC_API int64_t upper_func_block_preset_compose(lvEngine *ctx, const char *name_a, const char *name_b, const char *new_name);
/** 预设使用文档（写入 buf），返回写入字符数 */
lv_PUBLIC_API int64_t func_block_preset_doc(lvEngine *ctx, const char *name, char *buf, int64_t buf_size);
/** 链式执行预设序列，返回成功数量 */
lv_PUBLIC_API int64_t func_block_preset_chain(lvEngine *ctx, const char **names, int64_t count);
/** 批量实例化预设，结果写入 out_ids，返回成功数量 */
lv_PUBLIC_API int64_t func_block_preset_batch(lvEngine *ctx, const char **names, int64_t count, int64_t *out_ids);
/** 校验预设输入，返回 1 通过 */
lv_PUBLIC_API int64_t func_block_preset_validate(lvEngine *ctx, const char *name, int64_t *input_ids, int64_t input_count);
/** 实例绑定信息（写入 buf），返回写入字符数 */
lv_PUBLIC_API int64_t func_block_preset_bindings(lvEngine *ctx, int64_t instance_id, char *buf, int64_t buf_size);
/** 搜索预设（写入 buf），返回写入字符数 */
lv_PUBLIC_API int64_t func_block_preset_search(lvEngine *ctx, const char *query, char *buf, int64_t buf_size);
/** 递归展开预设，返回展开数量 */
lv_PUBLIC_API int64_t func_block_preset_recursive(lvEngine *ctx, int64_t preset_id, int64_t depth);
/** 注销预设（1 成功） */
lv_PUBLIC_API int64_t upper_func_block_preset_unregister(lvEngine *ctx, const char *name);
/** 注册预设（返回注册条目 ID） */
lv_PUBLIC_API int64_t func_block_preset_register(lvEngine *ctx, const char *name, int64_t input_count, int64_t output_count);
/** 预设库是否已初始化（1/0） */
lv_PUBLIC_API int64_t func_block_preset_initialized(lvEngine *ctx);
/** 清理预设库（1 成功） */
lv_PUBLIC_API int64_t func_block_preset_cleanup(lvEngine *ctx);

/* ============================================================
 * 综合工具（4 个）
 * ============================================================ */

/** 从引擎获取全局唯一 ID（每次调用递增） */
lv_PUBLIC_API int64_t lv_upper_alloc_id(lvEngine *ctx);
/** 获取当前全局 ID 计数器的值（只读） */
lv_PUBLIC_API int64_t lv_upper_get_id_counter(lvEngine *ctx);
/** 执行完整元验证流水线（completeness + soundness + differential），1=通过 */
lv_PUBLIC_API int64_t lv_upper_full_verify(lvEngine *ctx);
/** 综合导出 Coq/Lean4/SVG，返回成功导出的格式数量 */
int64_t lv_upper_export_all(lvEngine *ctx, int64_t proof_id, char *coq_buf, int64_t coq_sz, char *lean_buf,
                            int64_t lean_sz, char *svg_buf, int64_t svg_sz);

#ifdef __cplusplus
}
#endif

#endif /* lv_UPPER_API_H */
