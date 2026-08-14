/**
 * @file bootstrap_test.c
 * @brief Lv-00 自举差分测试框架（容器文件）
 *
 * @details 本文件提供自举测试框架的基础实现，包含：
 *          - 框架初始化/清理（bootstrap_test_framework_*）
 *          - 差分测试（BootstrapDiffTest）：DSL vs C API vs 几何层三路对比
 *          - 随机生成器（RandomGenerator）：随机约束图和 DSL 脚本生成
 *          - 图同构比较器（GraphIsomorphismComparator）：VF2 风格的同构检测
 *          - 原语包装器（Primitive Wrapper）：13 个几何原语的注册和测试
 *          - 测试预言机（TestOracle）：归一化幂等性、求解正确性、证明有效性验证
 *          - 报告生成（bootstrap_test_generate_report / write_report）
 *
 *          本文件已按功能组件拆分为以下模块：
 *          - bootstrap_test_init.c      框架初始化与全局状态
 *          - bootstrap_test_diff.c      差分测试
 *          - bootstrap_test_random.c    随机生成器
 *          - bootstrap_test_iso.c       图同构比较器
 *          - bootstrap_test_primitive.c 原语包装器
 *          - bootstrap_test_oracle.c    测试预言机
 *          - bootstrap_test_report.c    报告生成
 *
 *          共享兼容定义与框架状态见 bootstrap_test_internal.h。
 *
 * @author Lv-00 Project
 * @version 1.0.0
 * @date 2026-05-29
 */

#include "lv/bootstrap_test.h"
#include "lv/lv_log.h"

#include "lv/lv_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lv/constraint_graph.h"
#include "lv/cross_platform.h"
#include "lv/engine.h"
#include "lv/lv_utils.h"
#include "lv/proof_trace.h"
#include "lv/lv_internal.h"

#include "bootstrap_test_internal.h"
