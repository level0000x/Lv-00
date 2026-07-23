"""Test symbolic coordinates via lv.fallback (pure-Python path)."""

import pytest
from fractions import Fraction
from math import isclose, sqrt

import lv.fallback as fb


class TestFraction:
    """Exact fraction arithmetic: 1/3 + 1/3 = 2/3."""

    def test_fraction_exact(self):
        a = fb.SymCoord(Fraction(1, 3))
        b = fb.SymCoord(Fraction(1, 3))
        c = a + b
        assert c.value == Fraction(2, 3)
        assert c.value.numerator == 2
        assert c.value.denominator == 3


class TestSqrt:
    """Symbolic sqrt representation of sqrt(2)."""

    def test_sqrt_symbolic(self):
        s = fb.SymCoord.sqrt(2)
        assert s.is_sqrt
        assert s.radicand == 2
        # Numeric approximation for sanity
        assert isclose(float(s), sqrt(2), rel_tol=1e-12)


class TestDistance:
    """Pythagorean distance: 3-4-5 triangle."""

    def test_distance_pythagorean(self):
        p = fb.SymCoord(0, 0)
        q = fb.SymCoord(3, 4)
        d = p.distance(q)
        assert isclose(float(d), 5.0, rel_tol=1e-12)


class TestMidpoint:
    """Midpoint of two coordinates."""

    def test_midpoint(self):
        p = fb.SymCoord(1, 3)
        q = fb.SymCoord(5, 7)
        m = p.midpoint(q)
        assert isclose(float(m.x), 3.0, rel_tol=1e-12)
        assert isclose(float(m.y), 5.0, rel_tol=1e-12)


class TestRegularHexagon:
    """Vertices of a regular hexagon inscribed in unit circle."""

    def test_regular_hexagon(self):
        import math
        center = fb.SymCoord(0, 0)
        R = 1.0
        verts = fb.regular_polygon(center, R, 6)
        assert len(verts) == 6
        # First vertex at angle 0: (1, 0)
        assert isclose(float(verts[0].x), 1.0, rel_tol=1e-12)
        assert isclose(float(verts[0].y), 0.0, abs_tol=1e-12)
        # Adjacent vertices are 60 degrees apart
        for i in range(6):
            d = verts[i].distance(verts[(i + 1) % 6])
            assert isclose(float(d), 1.0, rel_tol=1e-12)
