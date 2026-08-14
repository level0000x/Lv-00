/**
 * @file graph_node.c
 * @brief ConstraintGraph 节点与约束生命周期管理（容器文件）
 *
 * @details 实现节点和约束的完整生命周期：
 *          - 节点创建：graph_create_point / graph_create_line /
 *            graph_create_circle 等各类型节点的构造与坐标初始化
 *          - 节点删除：graph_delete_node（级联删除关联约束）
 *          - 约束创建：graph_add_constraint（带类型验证与参与者兼容性检查）
 *          - 约束删除：graph_delete_constraint（更新邻接矩阵）
 *          - 安全扩容：动态数组的容量管理与 realloc 原子性保证
 *
 *          本文件已按功能域拆分为以下模块：
 *          - graph_node_alloc.c     节点/约束分配与生命周期创建
 *          - graph_node_hash.c      节点/约束哈希索引（O(1) 按 ID 查找）
 *          - graph_node_conflict.c  线段相交测试与增量代数冲突检测
 *          - graph_node_stub.c      区域/端口/函数块节点存根实现
 *
 *          共享内部函数声明见 graph_node_internal.h。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <assert.h>
#include <float.h>
#include <gmp.h>
#include <math.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/symbolic_coord.h"

#include "lv/config.h"
#include "lv/context.h"
#include "lv/debug.h"
#include "lv/error_codes.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/stream.h"
#include "lv/lv_strbuf.h"

#include "graph_node_internal.h"
