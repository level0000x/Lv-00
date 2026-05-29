/**
 * @file proof.h
 * @brief 命题与证明系统 - 合一检查、证明导航器、证明步骤
 *
 * 根据 Lv-00 设计文档第10节实现：
 * - 命题模式定义
 * - 合一检查（Unify）
 * - 命题的等价变换
 * - 命题的实例化
 * - ⊥的公理包可定义性
 * - 爆炸原理
 * - 证明导航器
 *
 * 【中文模块说明】
 * proof.h 是 Lv-00 系统的证明引擎核心模块，提供完整的几何证明框架。
 * 主要功能包括：
 * - 命题管理：创建、销毁、设置端口/模式/前置条件/后置条件
 * - 合一检查：将构造图与命题模式进行匹配验证
 * - 证明导航器：管理证明步骤的添加、导航、断点管理
 * - 证明依赖链：追踪证明步骤间的依赖关系和信任颜色
 * - 爆炸原理（Ex Falso）：从矛盾推导任意命题
 * - 反证法证明：假设目标否定，推导矛盾以证明原命题
 * - 自然语言输出：AlphaGeometry 风格的人类可读证明文本
 * - 策略注释：LeanGeo 风格的"先展示总体策略，再展开细节"
 * - 回溯搜索树：Newclid 风格的证明搜索可视化
 * - 多策略引擎：JGEX 风格的多证明方法共存（面积法、Groebner基法、向量法等）
 * - 不可构造性证明：三等分角、倍立方等经典不可构造问题的检测
 * - 参考项目 API：借鉴 Agda（洞填充）、Idris 2（QTT）、Isabelle（Sledgehammer）、
 *   HOL Light（微内核验证）、F*（精化类型）的证明功能
 *
 * 本文件为聚合头文件，按逻辑拆分为以下子文件：
 *   proof_proposition.h — 命题结构体、类型定义、基础 API
 *   proof_navigator.h   — 证明导航器、步骤管理、断点管理
 *   proof_search.h      — 证明搜索、策略引擎、回溯搜索树
 *   proof_output.h      — 证明输出、格式化、可视化与验证
 */

#ifndef LV00_PROOF_H
#define LV00_PROOF_H

#include "proof_proposition.h"
#include "proof_navigator.h"
#include "proof_search.h"
#include "proof_output.h"

#endif /* LV00_PROOF_H */
