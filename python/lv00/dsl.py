"""
Lv-00 DSL 模块 — PyEuclid 风格链式几何构造语言（兼容性重新导出层）

汲取 PyEuclid 的设计哲学——"让用户用最简单语法表达复杂几何关系"——
本模块在 lv00 现有核心基础上实现了极致简洁的链式几何 DSL。

警告：此模块已拆分为两个文件：
    - dsl_wrappers.py: 几何对象包装类（PointWrapper, SegmentWrapper 等）
    - dsl_context.py:  G 上下文类 + 模块级便捷函数

所有导出名称保持不变，现有代码无需修改。

用法示例：
    >>> from lv00.dsl import G
    >>>
    >>> with G() as g:
    ...     A = g.point(0, 0, "A")
    ...     B = g.point(4, 0, "B")
    ...     C = g.point(0, 3, "C")
    ...     d = g.distance(A, B)  # → 4.0

版本：3.3.0
作者：Lv-00 开发团队
"""

# 重新导出包装器类
from .dsl_wrappers import (
    PointWrapper,
    SegmentWrapper,
    LineWrapper,
    CircleWrapper,
    TriangleWrapper,
    PolygonWrapper,
    DSLError,
)

# 重新导出 G 上下文和便捷函数
from .dsl_context import (
    G,
    P,
    line,
    circle,
)

# 重新导出代数模式（v3.2.0 新增，借鉴 CadQuery + build123d + GAlgebra）
from .dsl_algebra import (
    # 变换系统（build123d 风格）
    Transform,
    Pos,
    Rot,
    Scale,
    # 工作平面系统（CadQuery 风格）
    Plane,
    TransformedPlane,
    Workplane,
    # 代数模式（build123d 风格）
    AlgebraMode,
)


__all__ = [
    # 原有（v3.0.0+）
    'G',
    'PointWrapper',
    'SegmentWrapper',
    'LineWrapper',
    'CircleWrapper',
    'TriangleWrapper',
    'PolygonWrapper',
    'P',
    'line',
    'circle',
    'DSLError',

    # 代数模式（v3.2.0，借鉴 CadQuery/build123d/GAlgebra）
    'Transform',
    'Pos',
    'Rot',
    'Scale',
    'Plane',
    'TransformedPlane',
    'Workplane',
    'AlgebraMode',
]
