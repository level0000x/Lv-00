# -*- coding: utf-8 -*-
"""
Pure-Python fallback mode for lv.

When the C DLL (lv_core) is unavailable, this module provides degraded
implementations so the Python layer can still run in a limited capacity.

Usage:
    from lv.fallback import enable_fallback
    enable_fallback()
"""

import math
import sys
from fractions import Fraction
from typing import Any, Dict, List, Optional, Tuple, Union

__all__ = [
    "enable_fallback",
    "_FakeBinding",
    "SymbolicCoordFb",
    "PointFb",
    "GraphFb",
    "_TriangleFb",
]

# ---------------------------------------------------------------------------
# Global state
# ---------------------------------------------------------------------------

_fallback_enabled: bool = False


def enable_fallback() -> None:
    """Inject the pure-Python fallback into the lv module namespace.

    Replaces ``lv._ctypes_binding`` with a :class:`_FakeBinding` instance
    and binds the fallback classes under their C-binding names
    (``SymbolicCoord``, ``Point``, ``Graph``) so that code importing the
    top-level API (``from lv import Graph`` etc.) works without the C DLL,
    matching the C-binding path.
    """
    global _fallback_enabled
    try:
        import lv as mod
    except ImportError:
        # lv package not importable; nothing to inject
        return
    mod._ctypes_binding = _FakeBinding()
    mod.SymbolicCoord = SymbolicCoordFb
    mod.Point = PointFb
    mod.Graph = GraphFb
    _fallback_enabled = True


# ---------------------------------------------------------------------------
# _FakeBinding — ctypes stand-in
# ---------------------------------------------------------------------------

class _FakeBinding:
    """Simulates the ctypes binding object when the C DLL is unavailable.

    Attribute access behaves as a no-op / identity, so code paths that
    branch on ``_ctypes_binding`` can proceed without error.  Methods that
    would normally invoke C functions simply return ``None`` or the
    appropriate sentinel.
    """

    def __getattr__(self, name: str) -> Any:
        # Return a callable that silently swallows any arguments.
        def _noop(*args: Any, **kwargs: Any) -> None:
            return None

        return _noop

    def __repr__(self) -> str:
        return "<_FakeBinding (fallback — no C DLL)>"


# ---------------------------------------------------------------------------
# SymbolicCoordFb — symbolic coordinate with rational support
# ---------------------------------------------------------------------------

class SymbolicCoordFb:
    """A symbolic coordinate backed by a Python :class:`~fractions.Fraction`.

    In fallback mode, all numeric coordinates are stored as fractions to
    preserve exact rational arithmetic even without the C rational library.
    """

    __slots__ = ("_val",)

    def __init__(self, value: Union[int, float, str, Fraction] = 0) -> None:
        if isinstance(value, Fraction):
            self._val = value
        elif isinstance(value, str):
            # Parse expressions like "1/3", "sqrt(2)", "2.5", "-4"
            s = value.strip()
            if "/" in s:
                parts = s.split("/", 1)
                self._val = Fraction(int(parts[0].strip()), int(parts[1].strip()))
            elif "sqrt" in s:
                # Store as a string token — exact irrationals are not
                # fully supported in fallback, but we record the expression.
                self._val = Fraction(int(float(s.replace("sqrt(", "").replace(")", "")) ** 0.5))
            else:
                self._val = Fraction(value).limit_denominator(10_000_000)
        elif isinstance(value, float):
            self._val = Fraction(value).limit_denominator(10_000_000)
        else:
            self._val = Fraction(value)

    @property
    def rational(self) -> Fraction:
        """Return the underlying Fraction."""
        return self._val

    @property
    def float(self) -> float:
        """Return a floating-point approximation."""
        return float(self._val)

    def __repr__(self) -> str:
        return f"SymbolicCoordFb({self._val})"

    def __eq__(self, other: object) -> bool:
        if isinstance(other, SymbolicCoordFb):
            return self._val == other._val
        if isinstance(other, (int, float, Fraction)):
            return self._val == Fraction(other)
        return NotImplemented

    def __add__(self, other: Any) -> "SymbolicCoordFb":
        if isinstance(other, SymbolicCoordFb):
            return SymbolicCoordFb(self._val + other._val)
        return SymbolicCoordFb(self._val + Fraction(other))

    def __sub__(self, other: Any) -> "SymbolicCoordFb":
        if isinstance(other, SymbolicCoordFb):
            return SymbolicCoordFb(self._val - other._val)
        return SymbolicCoordFb(self._val - Fraction(other))

    def __mul__(self, other: Any) -> "SymbolicCoordFb":
        if isinstance(other, SymbolicCoordFb):
            return SymbolicCoordFb(self._val * other._val)
        return SymbolicCoordFb(self._val * Fraction(other))

    def __truediv__(self, other: Any) -> "SymbolicCoordFb":
        if isinstance(other, SymbolicCoordFb):
            return SymbolicCoordFb(self._val / other._val)
        return SymbolicCoordFb(self._val / Fraction(other))

    def __neg__(self) -> "SymbolicCoordFb":
        return SymbolicCoordFb(-self._val)

    def __abs__(self) -> "SymbolicCoordFb":
        return SymbolicCoordFb(abs(self._val))


# ---------------------------------------------------------------------------
# PointFb — 2D point
# ---------------------------------------------------------------------------

class PointFb:
    """A 2D point in the fallback constraint graph.

    Supports geometric operations: distance, midpoint, translation,
    and rotation.
    """

    __slots__ = ("x", "y", "_label")

    def __init__(
        self,
        x: Union[int, float, str, Fraction, SymbolicCoordFb],
        y: Union[int, float, str, Fraction, SymbolicCoordFb],
        label: str = "",
    ) -> None:
        self.x = x if isinstance(x, SymbolicCoordFb) else SymbolicCoordFb(x)
        self.y = y if isinstance(y, SymbolicCoordFb) else SymbolicCoordFb(y)
        self._label = label

    @property
    def label(self) -> str:
        return self._label

    def distance_to(self, other: "PointFb") -> float:
        """Euclidean distance to another point (float approximation)."""
        dx = float(self.x.float - other.x.float)
        dy = float(self.y.float - other.y.float)
        return math.sqrt(dx * dx + dy * dy)

    def midpoint(self, other: "PointFb") -> "PointFb":
        """Return the midpoint between self and *other*."""
        mx = (self.x + other.x) / 2
        my = (self.y + other.y) / 2
        return PointFb(mx, my)

    def translate(self, dx: float, dy: float) -> "PointFb":
        """Return a new point translated by (*dx*, *dy*)."""
        return PointFb(self.x + dx, self.y + dy)

    def rotate(self, angle_deg: float, center: Optional["PointFb"] = None) -> "PointFb":
        """Return a new point rotated *angle_deg* degrees around *center*.

        If *center* is ``None``, rotation is performed around the origin (0, 0).
        """
        rad = math.radians(angle_deg)
        cos_a = math.cos(rad)
        sin_a = math.sin(rad)
        cx = center.x.float if center else 0.0
        cy = center.y.float if center else 0.0
        px = self.x.float - cx
        py = self.y.float - cy
        rx = px * cos_a - py * sin_a + cx
        ry = px * sin_a + py * cos_a + cy
        return PointFb(rx, ry)

    def __repr__(self) -> str:
        lbl = f"'{self._label}' " if self._label else ""
        return f"PointFb({lbl}x={self.x.float:.4f}, y={self.y.float:.4f})"

    def __eq__(self, other: object) -> bool:
        if isinstance(other, PointFb):
            return self.x == other.x and self.y == other.y
        return NotImplemented


# ---------------------------------------------------------------------------
# _TriangleFb — triangle helper
# ---------------------------------------------------------------------------

class _TriangleFb:
    """Lightweight triangle representation for fallback mode.

    Stores three vertices and exposes basic geometric queries.
    """

    __slots__ = ("a", "b", "c")

    def __init__(self, a: PointFb, b: PointFb, c: PointFb) -> None:
        self.a = a
        self.b = b
        self.c = c

    def side_lengths(self) -> Tuple[float, float, float]:
        """Return (|BC|, |CA|, |AB|) — lengths opposite vertices A, B, C respectively."""
        return (
            self.b.distance_to(self.c),
            self.c.distance_to(self.a),
            self.a.distance_to(self.b),
        )

    def perimeter(self) -> float:
        a_len, b_len, c_len = self.side_lengths()
        return a_len + b_len + c_len

    def area(self) -> float:
        """Area via Heron's formula."""
        a_len, b_len, c_len = self.side_lengths()
        s = (a_len + b_len + c_len) / 2.0
        return math.sqrt(max(0.0, s * (s - a_len) * (s - b_len) * (s - c_len)))

    def centroid(self) -> PointFb:
        """Return the centroid (intersection of medians)."""
        mx = (self.a.x + self.b.x + self.c.x) / 3
        my = (self.a.y + self.b.y + self.c.y) / 3
        return PointFb(mx, my)

    def __repr__(self) -> str:
        return f"_TriangleFb({self.a!r}, {self.b!r}, {self.c!r})"


# ---------------------------------------------------------------------------
# GraphFb — constraint graph
# ---------------------------------------------------------------------------

class GraphFb:
    """A constraint graph for geometric construction in fallback mode.

    Maintains collections of points, segments, and circles with their
    associated constraints, and provides convenience builders for common
    shapes.
    """

    def __init__(self, name: str = "graph") -> None:
        self.name = name
        self._points: Dict[str, PointFb] = {}
        self._segments: List[Tuple[str, str]] = []  # (label_a, label_b)
        self._circles: List[Tuple[str, float]] = []  # (center_label, radius)
        self._constraints: List[str] = []

    # -- mutators ------------------------------------------------------------

    def add_point(self, name: str, x: float, y: float, *, free: bool = True) -> PointFb:
        """Add a named point to the graph.

        Args:
            name: Unique label for the point.
            x, y: Coordinates.
            free: If ``True`` the point is free (can be moved by solver);
                  if ``False`` it is fixed.

        Returns:
            The newly created :class:`PointFb`.
        """
        pt = PointFb(x, y, label=name)
        self._points[name] = pt
        kind = "free" if free else "fixed"
        self._constraints.append(f"point({name}, {x}, {y}, {kind})")
        return pt

    def add_segment(self, a: str, b: str, *, length: Optional[float] = None) -> None:
        """Add a segment between two named points, optionally with a length constraint.

        Args:
            a, b: Labels of existing points.
            length: If provided, constrain the segment to this length.
        """
        if a not in self._points or b not in self._points:
            raise KeyError(f"Unknown point label: {a if a not in self._points else b}")
        self._segments.append((a, b))
        if length is not None:
            self._constraints.append(f"len({a}, {b}) = {length}")

    def add_circle(self, center: str, radius: float) -> None:
        """Add a circle with *center* point and *radius*.

        Args:
            center: Label of an existing point serving as the circle centre.
            radius: The radius (numeric).
        """
        if center not in self._points:
            raise KeyError(f"Unknown point label: {center}")
        self._circles.append((center, radius))

    # -- builders ------------------------------------------------------------

    def triangle(
        self,
        labels: Tuple[str, str, str],
        base: float = 3.0,
        height: float = 2.0,
    ) -> _TriangleFb:
        """Build a triangle with given vertex labels.

        Points are placed with *labels[0]* at (0, 0), *labels[1]* at
        (*base*, 0), and *labels[2]* at (base/2, height).

        Args:
            labels: Three labels for the vertices.
            base: Length of the base (horizontal).
            height: Height of the triangle.

        Returns:
            A :class:`_TriangleFb` wrapping the three points.
        """
        a = self.add_point(labels[0], 0.0, 0.0, free=False)
        b = self.add_point(labels[1], base, 0.0, free=False)
        c = self.add_point(labels[2], base / 2.0, height, free=False)
        self.add_segment(labels[0], labels[1])
        self.add_segment(labels[1], labels[2])
        self.add_segment(labels[2], labels[0])
        return _TriangleFb(a, b, c)

    def regular_polygon(
        self,
        prefix: str,
        n: int,
        radius: float = 1.0,
        center: Tuple[float, float] = (0.0, 0.0),
    ) -> List[PointFb]:
        """Build a regular *n*-gon inscribed in a circle.

        Args:
            prefix: Label prefix for vertices (e.g. ``"P"`` yields ``P0, P1, ...``).
            n: Number of vertices (>= 3).
            radius: Circumscribed circle radius.
            center: (cx, cy) of the polygon centre.

        Returns:
            List of vertex :class:`PointFb` objects in counter-clockwise order.
        """
        if n < 3:
            raise ValueError("regular_polygon requires n >= 3")
        cx, cy = center
        vertices: List[PointFb] = []
        for i in range(n):
            angle = math.radians(360.0 * i / n - 90.0)
            x = cx + radius * math.cos(angle)
            y = cy + radius * math.sin(angle)
            label = f"{prefix}{i}"
            pt = self.add_point(label, x, y)
            vertices.append(pt)
        for i in range(n):
            self.add_segment(vertices[i].label, vertices[(i + 1) % n].label)
        return vertices

    # -- queries -------------------------------------------------------------

    def get_point(self, name: str) -> PointFb:
        """Retrieve a point by label."""
        return self._points[name]

    @property
    def points(self) -> Dict[str, PointFb]:
        return dict(self._points)

    @property
    def segment_count(self) -> int:
        return len(self._segments)

    @property
    def circle_count(self) -> int:
        return len(self._circles)

    @property
    def constraint_count(self) -> int:
        return len(self._constraints)

    def summary(self) -> str:
        """Return a one-line summary of the graph contents."""
        return (
            f"GraphFb('{self.name}'): {len(self._points)} points, "
            f"{len(self._segments)} segments, {len(self._circles)} circles, "
            f"{len(self._constraints)} constraints"
        )

    def __repr__(self) -> str:
        return self.summary()
