/**
 * @file recursion.c
 * @brief 递归与条件系统实现 —— 良基递归与测度递减验证
 *
 * @details 实现递归深度监控、测度系统（Measures）、选择器块和互递归支持。
 *          包含内置测试套件和非符号测度验证。
 *
 *          核心功能模块：
 *          - 测度系统（MeasureSystem）：注册符号/自定义测度，管理递归终止条件
 *          - 符号测度：长度、角度、面积、深度等基于几何性质的测度计算
 *          - 自定义测度：用户提供的比较函数，用于非几何递归终止判定
 *          - 非符号测度验证：通过验证模板确认自定义测度的良基性
 *          - 选择器块：条件分支的图结构表示
 *          - 递归深度监控：防止无限递归，支持深度限制和反馈
 *          - 纯符号计算：使用代数运算而非浮点数进行测度值计算
 *            （如鞋带公式计算多边形面积）
 *
 *          递归终止保障：
 *          - 符号测度默认良基（well-founded），无需额外验证
 *          - 自定义测度需通过验证模板确认良基性
 *          - 测度递减原则：每次递归调用必须使测度值严格递减
 *
 * @author Lv-00 Project
 * @version 3.0.1
 *
 * @dependencies
 *   - recursion.h          : 递归系统公共接口定义
 *   - lv_internal.h      : 内部数据结构与常量
 *   - lv_utils.h         : 统一内存分配器
 *   - stream.h             : 流式事件输出
 *   - constraint_graph.h   : 约束图接口（通过 lv_internal.h 间接引用）
 */

#include "lv/lv_platform.h"
#include "lv/recursion.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_xmacro.h"
#include "lv/lv_utils.h"
#include "lv/stream.h"
#include "lv/stream.h"

LV_STREAM_CTX_DEFINE(recursion);
