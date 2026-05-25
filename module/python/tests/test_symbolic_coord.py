"""
Tests for SymbolicCoord Python bindings.
"""

import pytest
from fractions import Fraction
import sys
import os

# Add parent directory to path
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from lv00 import SymbolicCoord, Lv00Error


class TestSymbolicCoordCreation:
    """测试符号坐标的创建（支持整数、分数、字符串等多种输入形式）"""

    def test_create_from_int(self):
        """测试从整数创建符号坐标"""
        c = SymbolicCoord(5)
        assert str(c) == "5"

    def test_create_from_fraction(self):
        """测试从 Fraction 对象创建符号坐标"""
        c = SymbolicCoord(Fraction(3, 4))
        assert "3" in str(c)
        assert "4" in str(c)

    def test_create_from_string(self):
        """测试从字符串（如 '1/2'）创建符号坐标"""
        c = SymbolicCoord("1/2")
        assert "1" in str(c)
        assert "2" in str(c)

    def test_create_zero(self):
        """测试创建零坐标"""
        c = SymbolicCoord(0)
        assert str(c) == "0"

    def test_create_negative(self):
        """测试创建负数坐标"""
        c = SymbolicCoord(-5)
        assert "-5" in str(c) or "5" in str(c)


class TestSymbolicCoordArithmetic:
    """测试符号坐标的四则运算（加、减、乘、除）"""

    def test_addition(self):
        """测试加法运算"""
        c1 = SymbolicCoord(3)
        c2 = SymbolicCoord(2)
        result = c1 + c2
        assert result == SymbolicCoord(5)

    def test_subtraction(self):
        """测试减法运算"""
        c1 = SymbolicCoord(5)
        c2 = SymbolicCoord(3)
        result = c1 - c2
        assert result == SymbolicCoord(2)

    def test_multiplication(self):
        """测试乘法运算"""
        c1 = SymbolicCoord(3)
        c2 = SymbolicCoord(4)
        result = c1 * c2
        assert result == SymbolicCoord(12)

    def test_division(self):
        """测试除法运算"""
        c1 = SymbolicCoord(6)
        c2 = SymbolicCoord(2)
        result = c1 / c2
        assert result == SymbolicCoord(3)

    def test_addition_with_int(self):
        """测试符号坐标与普通整数的混合加法"""
        c1 = SymbolicCoord(3)
        result = c1 + 2
        assert result == SymbolicCoord(5)

    def test_chained_operations(self):
        """测试链式运算（多个符号坐标连续相加）"""
        c1 = SymbolicCoord(1)
        c2 = SymbolicCoord(2)
        c3 = SymbolicCoord(3)
        result = c1 + c2 + c3
        assert result == SymbolicCoord(6)


class TestSymbolicCoordComparison:
    """测试符号坐标的比较操作（等于、不等于、大小比较）"""

    def test_equality(self):
        """测试相等比较"""
        c1 = SymbolicCoord(5)
        c2 = SymbolicCoord(5)
        assert c1 == c2

    def test_inequality(self):
        """测试不等比较"""
        c1 = SymbolicCoord(5)
        c2 = SymbolicCoord(3)
        assert c1 != c2

    def test_less_than(self):
        """测试小于比较"""
        c1 = SymbolicCoord(3)
        c2 = SymbolicCoord(5)
        assert c1 < c2

    def test_greater_than(self):
        """测试大于比较"""
        c1 = SymbolicCoord(5)
        c2 = SymbolicCoord(3)
        assert c1 > c2

    def test_less_than_or_equal(self):
        """测试小于等于比较（含等于的情况）"""
        c1 = SymbolicCoord(3)
        c2 = SymbolicCoord(3)
        c3 = SymbolicCoord(5)
        assert c1 <= c2
        assert c1 <= c3

    def test_greater_than_or_equal(self):
        """测试大于等于比较（含等于的情况）"""
        c1 = SymbolicCoord(5)
        c2 = SymbolicCoord(5)
        c3 = SymbolicCoord(3)
        assert c1 >= c2
        assert c1 >= c3


class TestSymbolicCoordSerialization:
    """测试符号坐标的序列化与反序列化（字符串 <-> SymbolicCoord）"""

    def test_serialize(self):
        """测试将符号坐标序列化为字符串"""
        c = SymbolicCoord(Fraction(3, 4))
        s = str(c)
        assert len(s) > 0

    def test_deserialize(self):
        """测试从字符串反序列化恢复符号坐标，验证往返一致性"""
        original = SymbolicCoord(Fraction(3, 4))
        s = str(original)
        restored = SymbolicCoord(s)
        assert original == restored


class TestSymbolicCoordRepr:
    """测试符号坐标的 Python 表示（repr 和 str）"""

    def test_repr(self):
        """测试 repr 输出包含 'SymbolicCoord' 类名"""
        c = SymbolicCoord(5)
        r = repr(c)
        assert "SymbolicCoord" in r

    def test_str(self):
        """测试 str 输出为非空字符串"""
        c = SymbolicCoord(5)
        s = str(c)
        assert len(s) > 0


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
