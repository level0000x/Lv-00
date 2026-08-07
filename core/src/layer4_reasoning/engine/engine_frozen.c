/**
 * @file engine_frozen.c
 * @brief 引擎冻结点快照机制（从 engine.c 拆分）
 *
 * @details 冻结点是对引擎约束图的深拷贝，在执行有风险的符号操作前创建。
 *          如果位电路跳闸，引擎可以回滚到这个快照。
 *          实现完整的图深拷贝（节点/约束/哈希索引重建）与快照管理。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/engine.h"

#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/lv.h"

/** @brief 创建引擎状态冻结点 @details 保存当前引擎状态，用于后续回滚。 @param engine 引擎实例 @return 冻结点句柄，失败返回 NULL */
void *engine_create_frozen_point(lvEngine *engine) {
    if (!engine || !engine->main_graph)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "engine_create_frozen_point: NULL engine or main_graph");

    /* 通过唯一公共图级深拷贝入口 graph_copy() 创建快照（恒等 ID 方案，
     * 顺带保留 is_active/dirty 语义，与旧的本地新-ID 深拷贝实现等价）。 */
    ConstraintGraph *snapshot = graph_copy(engine->main_graph);
    return (void *) snapshot;
}

/** @brief 恢复引擎状态到指定冻结点 @param engine 引擎实例 @param frozen_point 冻结点句柄 @return true 成功 */
bool engine_restore_frozen_point(lvEngine *engine, void *frozen_point) {
    if (!engine || !frozen_point)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "engine_restore_frozen_point: NULL engine or frozen_point");

    ConstraintGraph *snapshot = (ConstraintGraph *) frozen_point;

    /* 销毁当前图 */
    if (engine->main_graph) {
        graph_destroy(engine->main_graph);
    }

    /* 用快照替换（所有权转移给引擎） */
    engine->main_graph = snapshot;

    /* 同步电路系统的冻结点状态：本次快照已消耗（所有权已转给引擎主图） */
    circuit_set_frozen_point(NULL);

    /* 引擎若此前另持有旧冻结点（非本次被消耗的快照），先释放，
     * 避免"失败回滚 → 后续成功命令"交替时旧快照泄漏 */
    if (engine->frozen_point && engine->frozen_point != frozen_point) {
        engine_destroy_frozen_point(engine->frozen_point);
    }

    /* 语义：回滚后引擎继续使用（电路跳闸处理器 engine_circuit.c 与
     * interop 命令路径均会在回滚后继续处理后续操作），因此立即重新打点，
     * 保证下一次电路跳闸仍可回滚，不再被静默跳过。
     * 注意：engine_create_frozen_point 仅执行 graph_copy，不触发 restore，
     * 此处无递归/死循环风险。 */
    engine->frozen_point = engine_create_frozen_point(engine);

    return true;
}

/** @brief 销毁冻结点并释放关联资源 @param frozen_point 冻结点句柄 */
void engine_destroy_frozen_point(void *frozen_point) {
    if (!frozen_point)
        return;
    ConstraintGraph *snapshot = (ConstraintGraph *) frozen_point;
    graph_destroy(snapshot);
}
