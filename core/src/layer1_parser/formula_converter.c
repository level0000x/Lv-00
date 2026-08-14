/**
 * @file formula_converter.c
 * @brief 公式转换器实现（容器文件）
 *
 * @details 实现公式 AST 与约束图之间的双向转换。
 *          支持点、线段、圆等几何元素的解析和生成。
 *
 *          本文件已按功能边界拆分为以下模块：
 *          - formula_converter_util.c      共享状态与辅助函数
 *          - formula_converter_geom.c      基本几何对象转换（点/线段/圆/三角形）
 *          - formula_converter_complex.c   复合几何对象转换（多边形/区域/弧）
 *          - formula_converter_constraint.c 约束转换（垂直/平行/中点/角度）
 *          - formula_converter_stmt.c      语句分派与公式→图主入口
 *          - formula_converter_export.c    渲染辅助与图→公式导出
 *
 * @author Lv-00 Project
 * @version 3.0.1
 *
 * @dependencies
 *   - formula_converter.h : 转换器公共接口定义
 *   - formula_renderer.h  : 公式渲染器接口（渲染辅助）
 *   - lv_internal.h     : 内部数据结构和常量
 *   - lv_utils.h        : 统一内存分配器和工具函数
 */

#include "lv/lv_platform.h"
#include "lv/formula_converter.h"
#include "formula_converter_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/formula_renderer.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/stream.h"
#include "lv/stream_context_util.h"
