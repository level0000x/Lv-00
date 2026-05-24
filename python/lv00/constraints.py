"""
Lv-00 几何约束类型模块

定义几何构造中使用的约束类型：
    - Constraint: 约束基类
    - IncidenceConstraint: 关联约束（点在线上或区域上）
    - BetweennessConstraint: 介子约束（点在两点之间）
    - IntersectionConstraint: 交点约束（两线交于一点）
    - ContainmentConstraint: 包含约束（区域包含关系）
    - ConnectionConstraint: 连接约束（端口连接）

版本：3.2.0
作者：Lv-00 开发团队
"""

from __future__ import annotations
from typing import TYPE_CHECKING, Any, Dict, List, Optional, Tuple, Union

# 使用 TYPE_CHECKING 避免循环导入，同时在类型检查时提供正确的类型提示
if TYPE_CHECKING:
    from .core import Point, LineSegment


class Constraint:
    """约束基类。

    所有几何约束的抽象父类，定义了约束的基本接口。

    属性：
        name: 约束类型名称，如 "incidence"、"betweenness" 等
    """

    __slots__ = ('name',)

    def __init__(self, name: str) -> None:
        """
        创建约束对象。

        参数：
            name: 约束类型的名称。建议使用 ConstraintType 中的枚举常量。

        异常：
            ValueError: name 为空字符串时抛出
        """
        if not name or not name.strip():
            raise ValueError("约束名称不能为空")
        self.name = name

    def __repr__(self) -> str:
        """
        返回约束的调试表示。

        返回：
            str: 格式为 "ConstraintType(name)" 的字符串
        """
        return f"{self.__class__.__name__}({self.name})"

    def to_dict(self) -> Dict[str, Any]:
        """
        将约束序列化为字典。

        返回：
            Dict[str, Any]: 包含约束类型名称的字典
        """
        return {"type": self.name}

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> 'Constraint':
        """
        从字典反序列化约束。

        参数：
            data: 包含 "type" 键的字典

        返回：
            Constraint: 对应类型的约束实例

        异常：
            ValueError: data 中缺少 "type" 键
        """
        constraint_type = data.get("type")
        if not constraint_type:
            raise ValueError("约束字典中缺少 'type' 键")
        return cls(constraint_type)


class IncidenceConstraint(Constraint):
    """关联约束 - 点位于线段或区域上。

    表示一个点必须落在某条线段或某个区域上。
    这是最基本的几何约束之一，用于定义点的位置关系。

    属性：
        point: 关联的点（Point 对象）
        target: 点所在的目标（LineSegment 或区域对象）
    """

    __slots__ = ('point', 'target')

    def __init__(self, point: Point, target: Union[Point, LineSegment]) -> None:
        """
        创建关联约束。

        参数：
            point: 被约束的点对象
            target: 点所关联的目标（线段或区域）

        异常：
            ValueError: point 或 target 为 None 时抛出
        """
        if point is None:
            raise ValueError("关联约束的点不能为 None")
        if target is None:
            raise ValueError("关联约束的目标不能为 None")
        super().__init__("incidence")
        self.point = point
        self.target = target

    def __repr__(self) -> str:
        """返回关联约束的调试表示。"""
        return f"IncidenceConstraint({self.point} on {self.target})"


class BetweennessConstraint(Constraint):
    """介子约束 - 一点位于另外两点之间。

    表示某点位于给定两点连线的中间位置，三点共线。
    这是欧几里得几何中的"介于"关系的约束化表示。

    属性：
        p1: 第一端点（Point 对象）
        p2: 第二端点（Point 对象）
        p3: 位于 p1 和 p2 之间的点（Point 对象）
    """

    __slots__ = ('p1', 'p2', 'p3')

    def __init__(self, p1: Point, p2: Point, p3: Point) -> None:
        """
        创建介子约束。

        参数：
            p1: 第一个端点
            p2: 第二个端点
            p3: 位于 p1 和 p2 连线中间的点

        异常：
            ValueError: 任意参数为 None 时抛出
        """
        if p1 is None:
            raise ValueError("介子约束的第一个端点不能为 None")
        if p2 is None:
            raise ValueError("介子约束的第二个端点不能为 None")
        if p3 is None:
            raise ValueError("介子约束的中间点不能为 None")
        if p1 is p2:
            raise ValueError("介子约束的两个端点不能相同")
        super().__init__("betweenness")
        self.p1 = p1
        self.p2 = p2
        self.p3 = p3

    def __repr__(self) -> str:
        """
        返回介子约束的调试表示。

        返回：
            str: 格式为 "BetweennessConstraint(p3 between p1 and p2)" 的字符串
        """
        return f"BetweennessConstraint({self.p3} between {self.p1} and {self.p2})"


class IntersectionConstraint(Constraint):
    """交点约束 - 两条线段交于一点。

    表示两条线段在给定点处相交。

    属性：
        line1: 第一条线段（LineSegment 对象）
        line2: 第二条线段（LineSegment 对象）
        point: 交点（Point 对象）
    """

    __slots__ = ('line1', 'line2', 'point')

    def __init__(self, line1: LineSegment, line2: LineSegment, point: Point) -> None:
        """
        创建交点约束。

        参数：
            line1: 第一条线段
            line2: 第二条线段
            point: 两条线段的交点

        异常：
            ValueError: 任意参数为 None 时抛出
        """
        if line1 is None:
            raise ValueError("交点约束的第一条线段不能为 None")
        if line2 is None:
            raise ValueError("交点约束的第二条线段不能为 None")
        if point is None:
            raise ValueError("交点约束的交点不能为 None")
        if line1 is line2:
            raise ValueError("交点约束的两条线段不能为同一对象")
        super().__init__("intersection")
        self.line1 = line1
        self.line2 = line2
        self.point = point

    def __repr__(self) -> str:
        """返回交点约束的调试表示。"""
        return f"IntersectionConstraint({self.line1} x {self.line2} = {self.point})"


class ContainmentConstraint(Constraint):
    """包含约束 - 一个区域包含在另一个区域内。

    表示内部区域完全位于外部区域之内。
    用于表示几何图形的嵌套关系，如"圆A在三角形B内"。

    属性：
        inner: 内部区域对象
        outer: 外部区域对象
    """

    __slots__ = ('inner', 'outer')

    def __init__(self, inner: Any, outer: Any) -> None:
        """
        创建包含约束。

        参数：
            inner: 内部区域对象
            outer: 外部区域对象

        异常：
            ValueError: 任意参数为 None 或 inner 与 outer 相同时抛出
        """
        if inner is None:
            raise ValueError("包含约束的内部区域不能为 None")
        if outer is None:
            raise ValueError("包含约束的外部区域不能为 None")
        if inner is outer:
            raise ValueError("包含约束的内外区域不能相同")
        super().__init__("containment")
        self.inner = inner
        self.outer = outer

    def __repr__(self) -> str:
        """返回包含约束的调试表示。"""
        return f"ContainmentConstraint({self.inner} inside {self.outer})"


class ConnectionConstraint(Constraint):
    """连接约束 - 两个端口之间建立连接。

    表示源端口的输出连接到目标端口的输入。
    这是函数块系统中端口间数据流的约束表示。

    属性：
        src_port: 源端口对象（输出端口）
        dst_port: 目标端口对象（输入端口）
    """

    __slots__ = ('src_port', 'dst_port')

    def __init__(self, src_port: Any, dst_port: Any) -> None:
        """
        创建连接约束。

        参数：
            src_port: 源端口对象
            dst_port: 目标端口对象

        异常：
            ValueError: 任意参数为 None 或 src_port 与 dst_port 相同时抛出
        """
        if src_port is None:
            raise ValueError("连接约束的源端口不能为 None")
        if dst_port is None:
            raise ValueError("连接约束的目标端口不能为 None")
        if src_port is dst_port:
            raise ValueError("连接约束的源端口和目标端口不能相同")
        super().__init__("connection")
        self.src_port = src_port
        self.dst_port = dst_port

    def __repr__(self) -> str:
        """
        返回连接约束的调试表示。

        返回：
            str: 格式为 "ConnectionConstraint(src_port -> dst_port)" 的字符串
        """
        return f"ConnectionConstraint({self.src_port} -> {self.dst_port})"


class ConstraintType:
    """约束类型枚举常量。

    定义所有支持的约束类型标识符，用于统一引用约束类型名称，
    避免硬编码字符串导致的拼写错误。

    使用示例：
        >>> ct = ConstraintType
        >>> incidence = Constraint("incidence")  # 不推荐
        >>> incidence = Constraint(ct.INCIDENCE)  # 推荐
    """
    INCIDENCE = "incidence"
    BETWEENNESS = "betweenness"
    INTERSECTION = "intersection"
    CONTAINMENT = "containment"
    CONNECTION = "connection"

    @classmethod
    def all_types(cls) -> List[str]:
        """获取所有约束类型列表。

        返回：
            List[str]: 所有约束类型字符串的列表，按字母顺序排列
        """
        return [
            cls.INCIDENCE,
            cls.BETWEENNESS,
            cls.INTERSECTION,
            cls.CONTAINMENT,
            cls.CONNECTION,
        ]


# ============================================================
# 模块级类型化工具函数
# ============================================================

def constraints_to_dict(constraints: List[Constraint]) -> List[Dict[str, Any]]:
    """
    将约束列表序列化为字典列表。

    参数：
        constraints: 约束对象列表

    返回：
        List[Dict[str, Any]]: 每个约束的字典表示
    """
    return [c.to_dict() for c in constraints]


def find_constraint_by_type(
    constraints: List[Constraint],
    constraint_type: str
) -> Optional[Constraint]:
    """
    在约束列表中查找指定类型的第一个约束。

    参数：
        constraints: 约束对象列表
        constraint_type: 要查找的约束类型名称（如 "incidence"）

    返回：
        Optional[Constraint]: 找到的约束，未找到返回 None
    """
    for c in constraints:
        if c.name == constraint_type:
            return c
    return None


def count_constraints_by_type(
    constraints: List[Constraint]
) -> Dict[str, int]:
    """
    按类型统计约束数量。

    参数：
        constraints: 约束对象列表

    返回：
        Dict[str, int]: 约束类型名称到计数的映射
    """
    counts: Dict[str, int] = {}
    for c in constraints:
        counts[c.name] = counts.get(c.name, 0) + 1
    return counts


__all__ = [
    'Constraint',
    'IncidenceConstraint',
    'BetweennessConstraint',
    'IntersectionConstraint',
    'ContainmentConstraint',
    'ConnectionConstraint',
    'ConstraintType',
    'constraints_to_dict',
    'find_constraint_by_type',
    'count_constraints_by_type',
]
