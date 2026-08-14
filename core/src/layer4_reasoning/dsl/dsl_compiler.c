/**
 * @file dsl_compiler.c
 * @brief Lv-00 DSL 编译器 —— 词法分析 → 语法分析 → IR 生成 → 约束图加载（容器文件）
 *
 * @details 实现 .lv 源文件的完整编译流水线。支持 GCLC 风格几何构造语句。
 *          编译器管线：dsl_tokenize → dsl_parse → dsl_compile → dsl_ir_to_constraint_graph
 *
 *          本文件已按编译流水线阶段拆分为以下模块：
 *          - dsl_compiler_parse.c  Parser 阶段：Token 流 → DSL AST
 *          - dsl_compiler_ir.c     Compiler 阶段：DSL AST → IR 中间表示
 *          - dsl_compiler_load.c   IR Loader 阶段：IR → 约束图与销毁/转储
 *
 * @author Lv-00 Project
 * @version 3.0.1
 */

#include "lv/dsl_compiler.h"
#include "dsl_compiler_internal.h"

#include <ctype.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_xmacro.h"

#include "lv/lv_internal.h"
