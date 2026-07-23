"""
Lv-00 Demo — 6 场景纯 Python 演示
使用 fallback 模块路径，无外部依赖。
"""
import sys

# ---------------------------------------------------------------------------
# Fallback 模块路径
# ---------------------------------------------------------------------------
try:
    from lv.lang import lvLang
    from lv.ir import lvIR
    from lv.compiler import Compiler
    from lv.theorem import TheoremDB
except ImportError:
    # Fallback — 演示模式下使用内联桩
    class lvLang:
        @staticmethod
        def eval(expr):
            return expr

    class lvIR:
        @staticmethod
        def lower(ast):
            return ast

    class Compiler:
        @staticmethod
        def compile(src):
            return src

    class TheoremDB:
        theorems = {
            "determinism": "eval 是确定性的",
            "type_safety": "良类型程序不会 stuck",
            "preservation": "编译保持语义",
        }
        @classmethod
        def check(cls, name):
            return cls.theorems.get(name, "未找到")

# ---------------------------------------------------------------------------
# 场景 1: 几何构造
# ---------------------------------------------------------------------------
def scene_geometry():
    print("=== 场景 1: 几何构造 ===")
    expr = ("Point", 0, 0)
    result = lvLang.eval(expr)
    print(f"  输入: {expr}")
    print(f"  求值: {result}")

# ---------------------------------------------------------------------------
# 场景 2: 三角形
# ---------------------------------------------------------------------------
def scene_triangle():
    print("=== 场景 2: 三角形 ===")
    a = ("Point", 0, 0)
    b = ("Point", 4, 0)
    c = ("Point", 2, 3)
    triangle = ("Triangle", a, b, c)
    result = lvLang.eval(triangle)
    print(f"  三角形: {result}")

# ---------------------------------------------------------------------------
# 场景 3: 正多边形
# ---------------------------------------------------------------------------
def scene_regular_polygon():
    print("=== 场景 3: 正多边形 ===")
    center = ("Point", 0, 0)
    poly = ("RegularPolygon", center, 5, 1.0)
    result = lvLang.eval(poly)
    print(f"  正五边形: {result}")

# ---------------------------------------------------------------------------
# 场景 4: 平移与旋转
# ---------------------------------------------------------------------------
def scene_transform():
    print("=== 场景 4: 平移与旋转 ===")
    p = ("Point", 1, 2)
    translated = ("Translate", p, ("Vector", 3, 4))
    rotated = ("Rotate", p, 90)
    results = [lvLang.eval(x) for x in (translated, rotated)]
    print(f"  平移: {results[0]}")
    print(f"  旋转: {results[1]}")

# ---------------------------------------------------------------------------
# 场景 5: 符号坐标
# ---------------------------------------------------------------------------
def scene_symbolic():
    print("=== 场景 5: 符号坐标 ===")
    expr = ("Point", "a", "b")
    lowered = lvIR.lower(expr)
    compiled = Compiler.compile(lowered)
    print(f"  AST:  {expr}")
    print(f"  IR:   {lowered}")
    print(f"  目标: {compiled}")

# ---------------------------------------------------------------------------
# 场景 6: 项目统计
# ---------------------------------------------------------------------------
def scene_stats():
    print("=== 场景 6: 项目统计 ===")
    checks = ["determinism", "type_safety", "preservation"]
    for name in checks:
        status = TheoremDB.check(name)
        print(f"  {name}: {status}")

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
if __name__ == "__main__":
    scenes = [
        scene_geometry,
        scene_triangle,
        scene_regular_polygon,
        scene_transform,
        scene_symbolic,
        scene_stats,
    ]
    for fn in scenes:
        fn()
        print()
    print("Lv-00 Demo 完成。")
