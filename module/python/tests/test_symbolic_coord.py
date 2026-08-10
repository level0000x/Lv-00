"""Test symbolic coordinates via lv.fallback (pure-Python path).

注意：fallback.py 仅提供纯 Python 的有理数/几何 API（SymbolicCoordFb、
PointFb、GraphFb），不支持 C 库的符号 sqrt 表示（无 SymCoord.sqrt/is_sqrt/
radicand），因此本文件中的测试均针对 fallback.py 实际导出的接口编写。
"""

import pytest
import os
import sys
from fractions import Fraction
from math import isclose

# Add parent directory to path so direct execution (python test_symbolic_coord.py) works
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import lv.fallback as fb


class TestFraction:
    """Exact fraction arithmetic: 1/3 + 1/3 = 2/3."""

    def test_fraction_exact(self):
        a = fb.SymbolicCoordFb(Fraction(1, 3))
        b = fb.SymbolicCoordFb(Fraction(1, 3))
        c = a + b
        assert c.rational == Fraction(2, 3)
        assert c.rational.numerator == 2
        assert c.rational.denominator == 3


class TestFractionString:
    """Fraction strings like '1/3' are parsed exactly."""

    def test_fraction_string(self):
        a = fb.SymbolicCoordFb("1/3")
        assert a.rational == Fraction(1, 3)


class TestDistance:
    """Pythagorean distance: 3-4-5 triangle."""

    def test_distance_pythagorean(self):
        p = fb.PointFb(0, 0)
        q = fb.PointFb(3, 4)
        d = p.distance_to(q)
        assert isclose(d, 5.0, rel_tol=1e-12)


class TestMidpoint:
    """Midpoint of two points."""

    def test_midpoint(self):
        p = fb.PointFb(1, 3)
        q = fb.PointFb(5, 7)
        m = p.midpoint(q)
        assert isclose(m.x.float, 3.0, rel_tol=1e-12)
        assert isclose(m.y.float, 5.0, rel_tol=1e-12)


class TestRegularHexagon:
    """Vertices of a regular hexagon inscribed in unit circle."""

    def test_regular_hexagon(self):
        graph = fb.GraphFb()
        verts = graph.regular_polygon("P", 6, 1.0)
        assert len(verts) == 6
        # First vertex at angle -90 degrees: (0, -1)
        assert isclose(verts[0].x.float, 0.0, abs_tol=1e-12)
        assert isclose(verts[0].y.float, -1.0, abs_tol=1e-12)
        # Adjacent vertices are 60 degrees apart: side length == radius == 1.0
        for i in range(6):
            d = verts[i].distance_to(verts[(i + 1) % 6])
            assert isclose(d, 1.0, rel_tol=1e-12)


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
