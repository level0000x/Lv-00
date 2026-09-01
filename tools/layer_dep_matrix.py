#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""F24/I5 层间依赖矩阵扫描（设计文档 §3.35 L1149）。

按「头→层归属表」检查 core/src 下每个 .c 的直接 include，
阻断低层对高层的引用（L1/L2 不得引用 L3+ 头或符号）。

规则：
- 源文件层：core/src/layerN_*/ → 层 N；core/src 根目录 → L0 便利层（lv.c / lv_impl_* 系统入口协调）；
- 头归属：显式登记表（L0 白名单/便利头）+ 同名实现 .c 推导（constraint_graph.h ↔ layer3_geometry/constraint_graph.c → L3）；
- 允许依赖表与 layer_validation.h 的 lv_LAYER_CAN_DEPEND 一致；
- lv.h 伞形头经白名单放行（传递引入由 P0-② lv_loader 只产 AST 重构解除）；
- 未登记头不参与判定（避免误报），必要时补登记表。

用法: python tools/layer_dep_matrix.py [--report]   # --report 打印全部头归属
退出码: 0 无违规；1 存在违规（CI 门禁）。
"""

import re
import sys
from pathlib import Path

# Windows 控制台 GBK 无法编码 ✓/✗ 等字符：强制 UTF-8 输出
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "core" / "src"

# ── 允许依赖表（与 core/include/lv/layer_validation.h lv_LAYER_CAN_DEPEND 一致）──
CAN_DEPEND = {
    0: set(range(11)),           # L0 便利层：允许所有
    1: {2},                      # L1 Parser → 仅 L2
    2: set(),                    # L2 Resource：不依赖任何层
    3: {2},                      # L3 Geometry → 仅 L2
    4: {2, 3},                   # L4 Reasoning → L2/L3
    5: {2, 3, 4},                # L5 Output → L2/L3/L4
    6: {2, 3, 4, 5},             # L6 Visual → L2..L5
    7: set(range(2, 7)),         # L7 Orchestration → L2..L6
    8: {2, 3, 4},                # L8 Meta → L2/L3/L4
    9: set(range(11)),           # L9 Application：允许所有
    10: {2, 4, 5},               # L10 Interop → L2/L4/L5
}

# ── L0 白名单头：任何层可 include（lv.h 伞形头经设计 L1149 白名单放行）──
WHITELIST = {
    "lv.h",            # 公共 API 伞形入口（协调各层）
    "lv_platform.h",   # 跨平台兼容
    "cross_platform.h",  # 跨平台类型
    "lv_internal.h",   # 内部基础（仅含 lv_platform.h）
    "lv_api_spec.h",   # 导出/废弃宏
    "engine.h",        # 引擎基础（lv_ALLOW_LAYER 宏定义处，L1 已含）
}

# 同名 .c 冲突时优先的实现层（geo_spec/math_input 等跨层重复，规范实现在高层）
NAME_OVERRIDE = {
    "geo_spec.h": 3,   # 规范实现 layer3_geometry/geo_spec.c
    "math_input.h": 1, # 规范实现 layer1_parser/math_input.c
    "parser_safety.h": 2, # F24/I5：自 L1 下沉 L2（输入安全设施 L1/L3 共享）
}

# ── 显式头→层登记表（纯头文件 / 内部头 / 子目录头，无同名 .c 可推导）──
EXPLICIT_HEADER_LAYER = {
    # L2 基础工具（纯 inline/宏头，全层共享；L1 可依赖 L2）
    "lv_xmacro.h": 2, "lv_thread.h": 2, "config.h": 2, "lv_check.h": 2,
    "lv_arith_safe.h": 2, "lv_parse_utils.h": 2, "lv_utils_internal.h": 2,
    "stream_internal.h": 2, "debug_internal.h": 2,
    # L3 几何拓扑（内部头/共享头）
    "mpz_poly.h": 3, "graph_node_internal.h": 3, "geometry_compress_internal.h": 3,
    "geometry_types.h": 3, "formula_converter_internal.h": 3,
    "euclidean_geometry_internal.h": 3, "geometry_csg_internal.h": 3,
    "geo_constraint_solver_internal.h": 3, "high_dim_internal.h": 3,
    "algebraic_number_internal.h": 3, "simd_ops_internal.h": 3,
    "graph_traversal_internal.h": 3, "aabb_internal.h": 3, "union_find_util.h": 3,
    "coeff_pool.h": 3, "determinism_state.h": 3, "symbolic_coord_internal.h": 3,
    "aabb_tree_impl.h": 3, "lv_vec3.h": 3, "trust_color_x.h": 3,
    "formula_dsl_internal.h": 3,
    # L3 内部 .c 互含（euclidean_geometry 家族）
    "euclidean_geometry_context.c": 3, "euclidean_geometry_axiom.c": 3,
    "euclidean_geometry_declare.c": 3, "euclidean_geometry_predicate.c": 3,
    "euclidean_geometry_consistency.c": 3, "euclidean_geometry_export.c": 3,
    "euclidean_geometry_equivalence.c": 3, "euclidean_geometry_helpers.c": 3,
    # L4 公理推理（内部头/共享头/后端子目录）
    "smt_backend.h": 4, "solver_common.h": 4, "prop_verifier_internal.h": 4,
    "proof_navigator_internal.h": 4, "proof_multi_strategy_internal.h": 4,
    "groebner_engine_internal.h": 4, "module_internal.h": 4, "preset_core.h": 4,
    "lv_constraint_guard.h": 4, "engine_internal.h": 4,
    "proof_engine_enhanced_internal.h": 4, "recursion_internal.h": 4,
    "inequality_reasoning_internal.h": 4, "unify_internal.h": 4, "module_helpers.h": 4,
    "axiom_pkg_internal.h": 4, "preset_manager_internal.h": 4,
    "bdd_encoding_internal.h": 4, "groebner_engine_guard.h": 4,
    "smt_backend_internal.h": 4, "dsl_compiler_internal.h": 4,
    "func_block_internal.h": 4, "sat_encoding_internal.h": 4,
    "quantifier_internal.h": 4, "preset_category.h": 4, "bootstrap_test_internal.h": 4,
    "engine_access.h": 4, "proof_classical.h": 4, "proof_step_strategy.h": 4,
    "proof_version_internal.h": 4, "rewrite_common.h": 4, "hash_history.h": 4,
    "expr_vtable.h": 4, "solver_types.h": 4, "preset_name_defs.h": 4,
    "autodiff_vtable.h": 4, "proof_rule_engine_internal.h": 4,
    "singular_engine_guard.h": 4,
    "cuda_backend.h": 4, "hip_backend.h": 4, "singular_backend.h": 4,
    # L5 结果输出（导出/互操作内部头）
    "plugin_system_internal.h": 5, "interop_export_internal.h": 5,
    "interop_import_internal.h": 5, "interop_command_internal.h": 5,
    "lv_builtin_commands.h": 5, "interop_server_internal.h": 5, "geo_visual.h": 5,
    # L6 可视化（块/图内部头）
    "block_graph_view.h": 6, "lv_block_utils.h": 6, "control_flow_blocks.h": 6,
    "data_structure_blocks.h": 6, "io_blocks.h": 6, "io_block.h": 6,
    "effect_system.h": 6,
    # L10 互操作桥接
    "interop_bridge_common.h": 10, "interop_step_type.h": 10,
    # L0 便利/协调（根目录 lv_impl_upper 家族 / orchestrator / application）
    "lv_impl_upper_internal.h": 0, "orchestrator.h": 0, "application.h": 0,
}


def src_layer(rel: str):
    """core/src 下的相对路径 → 层编号。"""
    rel = rel.replace("\\", "/")
    m = re.match(r"core/src/layer(\d+)_", rel)
    if m:
        return int(m.group(1))
    if rel.startswith("core/src/"):
        return 0  # 根目录：L0 便利层
    return None


def build_header_layer():
    """显式登记 > 同名覆盖 > 同名 .c 推导 > 去 _internal 后缀推导 → {头名: 层}。"""
    cache = dict(NAME_OVERRIDE)
    cache.update(EXPLICIT_HEADER_LAYER)
    for c in SRC.rglob("*.c"):
        l = src_layer(str(c.relative_to(ROOT)))
        if l is None:
            continue
        for key in (c.stem + ".h", re.sub(r"_internal$", "", c.stem) + ".h"):
            if key not in cache:
                cache[key] = l
    return cache


INCLUDE_RE = re.compile(r'^\s*#\s*include\s*"([^"]+)"')


def scan():
    header_layer = build_header_layer()
    violations = []
    stats = {"files": 0, "headers": len(header_layer), "whitelisted": 0, "unknown": 0}

    for c in sorted(SRC.rglob("*.c")):
        ls = src_layer(str(c.relative_to(ROOT)))
        if ls is None:
            continue
        stats["files"] += 1
        allowed = CAN_DEPEND[ls]
        for lineno, line in enumerate(c.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
            m = INCLUDE_RE.match(line)
            if not m:
                continue
            inc = m.group(1)
            name = inc.split("/")[-1]  # 统一取 basename（含 lv/backends/ 子目录头）
            if name in WHITELIST:
                stats["whitelisted"] += 1
                continue
            lh = header_layer.get(name)
            if lh is None:
                stats["unknown"] += 1
                continue
            if lh != ls and lh not in allowed:
                rel = str(c.relative_to(ROOT)).replace("\\", "/")
                violations.append((rel, lineno, name, ls, lh))
    return violations, stats


def main():
    report = "--report" in sys.argv
    violations, stats = scan()

    if report:
        hl = build_header_layer()
        print("=== 头→层归属表（同名实现推导，{} 个头）===".format(len(hl)))
        for name in sorted(hl):
            print("  {:40s} L{}".format(name, hl[name]))

    print("\n=== 依赖矩阵扫描结果 ===")
    print("文件数: {files}  头归属: {headers}  白名单 include: {whitelisted}  未登记 include: {unknown}".format(**stats))
    if not violations:
        print("✓ 无直接跨层 include 违规")
        return 0

    print("\n✗ {} 处直接跨层 include 违规:".format(len(violations)))
    for rel, lineno, name, ls, lh in violations:
        print("  {}:{}  include lv/{}  (L{} → L{} 违规)".format(rel, lineno, name, ls, lh))
    return 1


if __name__ == "__main__":
    sys.exit(main())
