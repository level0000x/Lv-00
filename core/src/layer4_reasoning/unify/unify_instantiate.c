/**
 * @file unify_instantiate.c
 * @brief proposition instantiation
 * @details Split from unify.c
 */

#include "lv/unify.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/geometric_primitives.h"

#include "lv/debug.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/normalization.h"
#include "lv/stream.h"
#include "lv/stream.h"
#include "lv/type_system.h"
#include "lv/lv_strbuf.h"
#include "unify_internal.h"

/* ---------------------------------------------------------------------------
 * 命题的实例化
 * ------------------------------------------------------------------------- */

/**
 * @brief 实例化命题图中的多态类型变量
 *
 * @details 对命题图进行深拷贝后，将指定节点（type_var_node_id）的端口类型区域
 *          设置为传入的具体类型 concrete_type。此函数用于将带有多态类型变量的
 *          命题模式实例化为具体类型版本，通常发生在合一检查确定类型绑定之后。
 *
 * 【类型区域生命周期管理规则 —— 调用者必读】
 *
 *   本函数对 concrete_type 使用引用语义（浅拷贝/指针赋值），不创建深拷贝。
 *   这意味着调用者必须承担以下生命周期责任：
 *
 *   1. concrete_type 指向的内存必须在 out_instantiated 指向的图存在期间
 *      始终保持有效。任何在实例化图销毁之前释放 concrete_type 的行为都将
 *      导致 inst 图中端口的 type_region 指针成为悬垂指针。
 *
 *   2. 禁止修改 concrete_type 的子字段（子类型、约束ID等），因为所有通过
 *      此函数实例化的端口共享同一份 concrete_type 引用，修改会导致
 *      不可预期的副作用。
 *
 *   3. 销毁规则：调用者应先销毁 out_instantiated（调用 graph_destroy），
 *      再销毁 concrete_type（调用 type_region_destroy）。反向操作将导致
 *      type_region 悬垂指针。注意 graph_destroy 不会释放 type_region，
 *      因为 type_region 的所有权属于调用者。
 *
 *   4. 如果调用者无法保证上述生命周期覆盖，应在调用本函数之前先通过
 *      其他方式创建 concrete_type 的深拷贝，再将拷贝传入。
 *      type_system 模块中已有 type_region_deep_copy() 静态函数提供此能力，
 *      但尚未导出为公共API。规划中将在后续版本中将其导出为
 *      type_system_deep_copy_type_region() 公共接口。
 *
 * 【内存所有权模型】
 *   - proposition: 输入，本函数不获取其所有权，不修改
 *   - concrete_type: 输入，本函数获取其引用（非所有权），调用者负责释放
 *   - out_instantiated: 输出，调用者获得所有权，使用完毕后需 graph_destroy
 *
 * @param proposition     含多态类型变量的命题图（输入，不修改）
 * @param type_var_node_id 待实例化的类型变量节点ID
 * @param concrete_type   具体类型区域（输入，生命周期由调用者管理）
 * @param out_instantiated 输出：实例化后的命题图（调用者获得所有权）
 * @return true 表示实例化成功，false 表示失败（参数无效或内存不足）
 */
bool unify_instantiate_proposition(ConstraintGraph *proposition, int type_var_node_id, const TypeRegion *concrete_type,
                                   ConstraintGraph **out_instantiated) {
    if (!proposition || !concrete_type || !out_instantiated)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "unify_instantiate_proposition: NULL parameter");

    *out_instantiated = NULL;

    /* 深拷贝命题图 */
    ConstraintGraph *inst = graph_copy(proposition);
    if (!inst)
        lv_RETURN_ERROR_BOOL(lv_ERROR_OUT_OF_MEMORY, "unify_instantiate_proposition: graph_copy failed");

    /* 查找类型变量节点 */
    GeomNode *type_var_node = NULL;
    for (int i = 0; i < inst->node_count; i++) {
        if (inst->nodes[i]->id == type_var_node_id) {
            type_var_node = inst->nodes[i];
            break;
        }
    }

    if (type_var_node && type_var_node->type == GEOM_PORT && type_var_node->data.port) {
        /*
         * 【类型区域赋值 —— 引用语义安全性说明】
         *
         * 此处将 concrete_type 指针直接赋值给 type_region，使用浅拷贝
         * （引用语义），而非深拷贝。这是有意为之的设计决策，原因如下：
         *
         * A. 为什么当前使用浅拷贝：
         *    - type_system 模块中已有 type_region_deep_copy() 静态函数，
         *      但该函数尚未导出为公共API（static 修饰，仅 type_system.c 内可见）。
         *    - 将 type_region_deep_copy 提升为公共API需要重新考虑：
         *      a) 深拷贝后谁负责释放（所有权转移语义）
         *      b) 递归子类型的生命周期管理（first_type/second_type 等）
         *      c) contained_node_ids / constraint_ids 在深拷贝后的有效性
         *    - 当前所有调用 unify_instantiate_proposition 的路径都确保
         *      concrete_type 来自 TypeSystem 的注册表（type_regions 数组），
         *      其生命周期由 TypeSystem 管理，通常覆盖整个引擎生命周期。
         *      因此浅拷贝在当前调用场景下实际上是安全的。
         *
         * B. 浅拷贝的风险（在非标准调用路径中）：
         *    1. 悬垂指针：若调用者传入栈上分配的临时 TypeRegion，或调用后
         *       立即销毁 concrete_type，则实例化图中的指针将失效。
         *    2. 别名修改：多个端口共享同一 concrete_type 时，任何一方通过
         *       指针修改其字段（如约束ID、子类型等）将影响所有引用者。
         *    3. 双重释放：若调用者在 graph_destroy 之后仍尝试 type_region_destroy
         *       concrete_type，因为 graph_destroy 中不会释放 type_region（端口
         *       不拥有 type_region 的所有权），不会双重释放；但若未来修改
         *       graph_destroy 实现，则需注意此问题。
         *
         * C. 规划中的修复方案：
         *    1. 短期：在 debug.h 中添加 TYPE_REGION_LIFECYCLE_CHECK 宏，
         *       在调试模式下对 concrete_type 添加哨兵标记，检测悬垂访问。
         *    2. 长期：将 type_region_deep_copy() 导出为公共API
         *       type_system_deep_copy_type_region()，并在本函数中使用它，
         *       同时明确文档化深拷贝后的所有权转移规则。
         *
         * D. 当前的安全保障（调用约定）：
         *    - 调用者约定：concrete_type 必须在 out_instantiated 的整个
         *      生命周期内有效（详见函数头部的生命周期管理规则文档）。
         *    - 调试模式断言：以下检查在调试模式下警告调用者注意生命周期：
          */
        if (debug_is_debug_mode()) {
            if (!concrete_type->alias_name && !concrete_type->variable_name && concrete_type->kind == 0 &&
                concrete_type->level == 0) {
                /* 如果 concrete_type 的所有可识别字段均为零/空，可能是已被销毁
                  * 或未初始化的对象。仅在调试模式下记录警告，不中断执行，
                  * 因为某些合法的类型区域可能确实全部为零值。 */
                debug_log(LOG_LEVEL_WARN, "unify",
                          "unify_instantiate_proposition: concrete_type at %p appears "
                          "to be zero-initialized or destroyed — possible dangling pointer "
                          "risk for node %d",
                          (const void *) concrete_type, type_var_node_id);
            }
        }
        type_var_node->data.port->type_region = (TypeRegion *) concrete_type;
        type_var_node->data.port->is_polymorphic = false;
    }

    *out_instantiated = inst;
    return true;
}
