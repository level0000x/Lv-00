/**
 * @file lv_serialize_adapters.c
 * @brief 内置序列化适配器实现
 *
 * @details 把业务序列化对包装为序列化注册表的统一函数形态：
 *          - graph_serialize_to_json（返回 char*）→ lvSerializeFunc
 *          - graph_deserialize_from_json（返回新对象）→ lvDeserializeFunc
 *            （对象指针槽 T** 契约：成功时 *obj 指向新分配的对象）
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

#include "lv/lv_serialize_adapters.h"

#include <string.h>

#include "lv/lv_storage.h"
#include "lv/constraint_graph.h"
#include "lv/meta_repr.h" /* meta_repr_graph_equivalent：round-trip 验证内置比较 */
#include "lv/lv_utils.h"

/** @brief ConstraintGraph → JSON 字符串 → 存储（lvSerializeFunc 形态适配） */
static bool graph_json_ser_adapter(const void *obj, lvStorage *storage) {
    if (!obj || !storage) return false;

    char *json = graph_serialize_to_json((const ConstraintGraph *) obj);
    if (!json) return false;

    bool ok = lv_storage_write_all(storage, json, (int64_t) strlen(json));
    lv_free((void **) &json);
    return ok;
}

/** @brief 存储 → JSON 字符串 → ConstraintGraph（lvDeserializeFunc 形态适配）。
 *  graph_deserialize_from_json 返回新对象，此处将其接管写入对象指针槽。 */
static bool graph_json_deser_adapter(void *obj, lvStorage *storage) {
    if (!obj || !storage) return false;

    int64_t size = 0;
    char *json = lv_storage_read_all(storage, &size);
    if (!json) return false;

    ConstraintGraph *graph = graph_deserialize_from_json(json);
    lv_free((void **) &json);
    if (!graph) return false;

    *(ConstraintGraph **) obj = graph;
    return true;
}

bool lv_serialize_register_graph_adapters(void) {
    /* round-trip 验证的内置比较/释放回调注册（存储层 L2 不依赖上层类型） */
    lv_storage_register_verify("ConstraintGraph",
                               (lvCompareFn) meta_repr_graph_equivalent,
                               (lvStorageFreeFn) graph_destroy);

    return lv_serialize_register_format("ConstraintGraph", "json",
                                         graph_json_ser_adapter,
                                         graph_json_deser_adapter);
}

void lv_serialize_cleanup_adapters(void) {
    /* 完整释放序列化/验证/存储后端注册表（含 once 重置，允许再次 init）：
     * 修复 lv_cleanup 后 ConstraintGraph 序列化条目泄漏（~1162 字节） */
    lv_storage_system_cleanup();
}
