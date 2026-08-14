/**
 * @file unify.c
 * @brief 合一检查实现
 * @details 实现构造与命题之间的合一检查，包括约束匹配、坐标判等、
 *          命题实例化和等价声明。支持哈希过滤和详细匹配结果输出。
 *
 * 【模块使用说明】
 *
 * 本模块是 Lv-00 几何元语言系统的核心合一引擎，负责验证构造图与命题图
 * 之间的结构等价性。主要功能模块如下：
 *
 * 1. 基础合一检查（unify_construction_with_proposition）：
 *    检查两个约束图在归一化后是否在拓扑和约束结构上等价。
 *
 * 2. 坐标级合一检查（unify_construction_with_proposition_coord）：
 *    在基础合一检查的基础上，增加对参与节点符号坐标的逐项判等，
 *    确保不仅拓扑匹配，几何语义也完全一致。
 *
 * 3. 哈希预过滤合一检查（unify_construction_with_proposition_hash）：
 *    使用坐标哈希值预分组，在详细匹配前快速排除不匹配的端口和约束对，
 *    显著提升大规模图的合一检查性能。
 *
 * 4. 命题实例化（unify_instantiate_proposition）：
 *    对含多态类型变量的命题图进行深拷贝，并将指定节点的类型区域
 *    替换为具体类型。使用引用语义（浅拷贝 type_region），调用者需
 *    确保 concrete_type 的生命周期覆盖实例化后的命题图。
 *
 * 5. 深层等价声明（unify_declare_equivalence）：
 *    声明两个节点在更深语义层上的等价性，用于桥接不同构造之间的
 *    语义联系。
 *
 * 6. 精细化匹配函数（端口匹配、约束匹配、坐标判等的独立封装）：
 *    将合一流程拆解为可独立调用的阶段函数，用于调试和自定义检查流程。
 *
 * 【典型调用流程】
 *
 *   // 创建 TypeSystem，用于端口类型等价检查
 *   TypeSystem *ts = type_system_create();
 *   ...
 *   // 执行合一检查
 *   UnifyStatus status = unify_construction_with_proposition_hash(constr, prop);
 *   if (status == UNIFY_STATUS_OK) {
 *       // 合一成功，可实例化类型变量
 *       unify_instantiate_proposition(prop, type_var_id, concrete_type, &inst);
 *   }
 *   ...
 *   // 清理
 *   type_system_destroy(ts);
 *
 * 【注意事项】
 *   - 所有合一函数内部创建 TypeSystem，使用者无需重复创建
 *   - 归一化结果（NormalizationResult）作为中间产物，函数内部管理生命周期
 *   - 哈希预过滤基于坐标哈希值，可能产生哈希碰撞导致误判，
 *     但不会导致正确性错误（最坏退化为全量比较）
 *   - type_region 使用引用语义，详见 unify_instantiate_proposition 文档
 *
 * @author Lv-00 Project
 * @version 3.0.1
 *
 * @dependencies
 *   - unify.h               : 合一检查器公共接口定义
 *   - constraint_graph.h    : 约束图接口
 *   - lv_internal.h       : 内部数据结构与常量
 *   - lv_utils.h          : 统一内存分配器和字符串工具
 *   - normalization.h       : 图规范化引擎
 *   - proof.h               : 证明系统接口（命题颜色更新）
 *   - type_system.h         : 类型系统（端口类型等价检查）
 *   - stream.h              : 流式事件输出
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
#include "lv/lv_utils.h" /* lv_strdup_safe, lv_malloc 等统一内存管理 */
#include "lv/normalization.h"
#include "lv/stream.h"
#include "lv/stream.h"
#include "lv/type_system.h"
#include "lv/lv_strbuf.h"

LV_STREAM_CTX_DEFINE(unify);

/* 哈希值到节点ID的掩码 —— 取哈希值低31位以确保结果为正整数 */
#define UNIFY_HASH_TO_ID_MASK 0x7FFFFFFF
