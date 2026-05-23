"""
Tests for Graph Python bindings.
"""

import pytest
from fractions import Fraction
import sys
import os

# Add parent directory to path
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from lv00 import Graph, Point, LineSegment, Lv00Error


class TestGraphCreation:
    """测试图的创建与基本表示"""

    def test_create_graph(self):
        """测试创建空的约束图对象"""
        g = Graph()
        assert g is not None
        assert repr(g) == "Graph(points=0, segments=0)"

    def test_graph_repr(self):
        """测试图的字符串表示包含 'Graph' 关键字"""
        g = Graph()
        assert "Graph" in repr(g)


class TestGraphAddPoint:
    """测试向图中添加几何点（支持整数和分数坐标）"""

    def test_add_point_int(self):
        """测试使用整数坐标添加点"""
        g = Graph()
        p = g.add_point(0, 0)
        assert isinstance(p, Point)
        assert p._id == 0

    def test_add_point_fraction(self):
        """测试使用分数坐标添加点"""
        g = Graph()
        p = g.add_point(Fraction(1, 2), Fraction(3, 4))
        assert isinstance(p, Point)

    def test_add_multiple_points(self):
        """测试连续添加多个点，验证 ID 递增和计数正确"""
        g = Graph()
        p1 = g.add_point(0, 0)
        p2 = g.add_point(1, 1)
        p3 = g.add_point(2, 2)

        assert p1._id == 0
        assert p2._id == 1
        assert p3._id == 2
        assert len(g._points) == 3

    def test_graph_tracks_points(self):
        """测试图正确跟踪已添加的点对象"""
        g = Graph()
        p = g.add_point(0, 0)
        assert p in g._points


class TestGraphAddLineSegment:
    """测试向图中添加线段（要求端点必须先添加到图中）"""

    def test_add_line_segment(self):
        """测试创建连接两点的线段"""
        g = Graph()
        p1 = g.add_point(0, 0)
        p2 = g.add_point(1, 1)
        seg = g.add_line_segment(p1, p2)

        assert isinstance(seg, LineSegment)
        assert seg.p1 == p1
        assert seg.p2 == p2

    def test_add_multiple_segments(self):
        """测试添加多条线段并验证计数"""
        g = Graph()
        p1 = g.add_point(0, 0)
        p2 = g.add_point(1, 1)
        p3 = g.add_point(2, 2)

        seg1 = g.add_line_segment(p1, p2)
        seg2 = g.add_line_segment(p2, p3)

        assert len(g._segments) == 2

    def test_add_segment_requires_points_in_graph(self):
        """测试使用未注册到图中的点创建线段时应抛出 Lv00Error"""
        g = Graph()
        p1 = Point(0, 0)
        p2 = Point(1, 1)

        # Points not added to graph yet
        with pytest.raises(Lv00Error):
            g.add_line_segment(p1, p2)


class TestGraphNormalization:
    """测试图的规范化操作（节点合并、等价类检测等）"""

    def test_normalize_empty_graph(self):
        """测试对空图执行规范化不报错"""
        g = Graph()
        result = g.normalize()
        assert result is not None

    def test_normalize_with_points(self):
        """测试对含有点的图执行规范化"""
        g = Graph()
        g.add_point(0, 0)
        g.add_point(1, 1)

        result = g.normalize()
        assert result is not None

    def test_normalize_with_segments(self):
        """测试对含有线段的图执行规范化"""
        g = Graph()
        p1 = g.add_point(0, 0)
        p2 = g.add_point(1, 1)
        g.add_line_segment(p1, p2)

        result = g.normalize()
        assert result is not None


class TestPointRepr:
    """测试 Point 对象的字符串表示"""

    def test_point_repr(self):
        """测试 Point 的 repr 输出包含 'Point' 关键字"""
        g = Graph()
        p = g.add_point(0, 0)
        r = repr(p)
        assert "Point" in r


class TestLineSegmentRepr:
    """测试 LineSegment 对象的字符串表示"""

    def test_segment_repr(self):
        """测试 LineSegment 的 repr 输出包含 'LineSegment' 关键字"""
        g = Graph()
        p1 = g.add_point(0, 0)
        p2 = g.add_point(1, 1)
        seg = g.add_line_segment(p1, p2)

        r = repr(seg)
        assert "LineSegment" in r


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
