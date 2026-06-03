"""
Lv-00 预设函数块基础架构模块
============================

模块功能概述:
    提供预设函数块系统的基础架构，包括:
    - 枚举类型定义 (分类、确定性级别、输出格式)
    - 数据类定义 (函数块规格、参数规格、输出规格)
    - 注册表系统 (全局预设函数块管理)
    - 工具函数 (输入验证、格式化等)

设计原则:
    1. 类型安全: 使用 Python 类型注解确保代码健壮性
    2. 可扩展性: 注册表系统支持动态添加预设
    3. 文档完整: 所有公共 API 都有详细的中文文档
    4. 数学严谨: 数学定义与实现严格对应

版本: 4.0.0
作者: Lv-00 开发团队
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from enum import Enum, auto
from typing import TYPE_CHECKING, Any, Callable, Dict, Generic, List, Optional, Set, Tuple, TypeVar, Union

# 类型检查时导入，避免运行时循环依赖
if TYPE_CHECKING:
    from ..core import Graph, Point, LineSegment, SymbolicCoord
    from ..func_block import FuncBlock, SolutionSelector

# ============================================================
# 枚举类型定义
# ============================================================

class PresetCategory(Enum):
    """
    预设函数块分类枚举。
    
    按照数学领域对预设函数块进行分类，便于用户查找和管理。
    
    Attributes:
        BASIC: 基础几何构造（点、线、距离等）
        CIRCLE: 圆相关构造
        TRIANGLE: 三角形构造
        POLYGON: 多边形构造
        TRANSFORMATION: 几何变换
        ALGEBRA: 代数运算
        TOPOLOGY: 拓扑分析
        ANALYSIS: 数学分析
        LOGIC: 逻辑推导
        ADVANCED: 高级构造
    """
    BASIC = auto()
    CIRCLE = auto()
    TRIANGLE = auto()
    POLYGON = auto()
    TRANSFORMATION = auto()
    ALGEBRA = auto()
    TOPOLOGY = auto()
    ANALYSIS = auto()
    LOGIC = auto()
    ADVANCED = auto()
    
    def __str__(self) -> str:
        """返回分类的中文名称。"""
        names = {
            PresetCategory.BASIC: "基础几何",
            PresetCategory.CIRCLE: "圆相关",
            PresetCategory.TRIANGLE: "三角形",
            PresetCategory.POLYGON: "多边形",
            PresetCategory.TRANSFORMATION: "几何变换",
            PresetCategory.ALGEBRA: "代数运算",
            PresetCategory.TOPOLOGY: "拓扑分析",
            PresetCategory.ANALYSIS: "数学分析",
            PresetCategory.LOGIC: "逻辑推导",
            PresetCategory.ADVANCED: "高级构造",
        }
        return names.get(self, "未知分类")


class DeterminismLevel(Enum):
    """
    确定性级别枚举。
    
    描述函数块在不同输入条件下的解的唯一性保证。
    
    Attributes:
        ALWAYS_UNIQUE: 对于所有有效输入，解总是唯一的
        CONDITIONALLY_UNIQUE: 在满足特定条件时解唯一
        MULTIPLE_SOLUTIONS: 可能产生多个解，需要选择器
    """
    ALWAYS_UNIQUE = auto()
    CONDITIONALLY_UNIQUE = auto()
    MULTIPLE_SOLUTIONS = auto()
    
    def __str__(self) -> str:
        """返回确定性级别的中文描述。"""
        names = {
            DeterminismLevel.ALWAYS_UNIQUE: "总是唯一解",
            DeterminismLevel.CONDITIONALLY_UNIQUE: "条件唯一解",
            DeterminismLevel.MULTIPLE_SOLUTIONS: "多解情况",
        }
        return names.get(self, "未知级别")


class OutputFormat(Enum):
    """
    输出格式枚举。
    
    定义函数块输出的数据格式。
    
    Attributes:
        POINT: 点坐标
        LINE: 直线
        SEGMENT: 线段
        CIRCLE: 圆
        SCALAR: 标量值
        VECTOR: 向量
        BOOLEAN: 布尔值
        COLLECTION: 集合
    """
    POINT = auto()
    LINE = auto()
    SEGMENT = auto()
    CIRCLE = auto()
    SCALAR = auto()
    VECTOR = auto()
    BOOLEAN = auto()
    COLLECTION = auto()
    
    def __str__(self) -> str:
        """返回输出格式的中文名称。"""
        names = {
            OutputFormat.POINT: "点",
            OutputFormat.LINE: "直线",
            OutputFormat.SEGMENT: "线段",
            OutputFormat.CIRCLE: "圆",
            OutputFormat.SCALAR: "标量",
            OutputFormat.VECTOR: "向量",
            OutputFormat.BOOLEAN: "布尔值",
            OutputFormat.COLLECTION: "集合",
        }
        return names.get(self, "未知格式")


# ============================================================
# 数据类定义
# ============================================================

@dataclass(frozen=True)
class ParamSpec:
    """
    参数规格数据类。
    
    描述函数块输入参数的详细规格。
    
    Attributes:
        name: 参数名称（英文标识符）
        chinese_name: 参数中文名称
        description: 参数描述
        param_type: 参数类型
        required: 是否为必需参数
        default_value: 默认值（可选）
    
    Example:
        >>> param = ParamSpec(
        ...     name="point_a",
        ...     chinese_name="点A",
        ...     description="线段的第一个端点",
        ...     param_type="Point",
        ...     required=True
        ... )
    """
    name: str
    chinese_name: str
    description: str
    param_type: str
    required: bool = True
    default_value: Any = None
    
    def __post_init__(self) -> None:
        """验证参数规格的有效性。"""
        if not self.name or not isinstance(self.name, str):
            raise ValueError("参数名称必须是非空字符串")
        if not self.chinese_name or not isinstance(self.chinese_name, str):
            raise ValueError("参数中文名称必须是非空字符串")


@dataclass(frozen=True)
class OutputSpec:
    """
    输出规格数据类。
    
    描述函数块输出的详细规格。
    
    Attributes:
        name: 输出名称
        chinese_name: 输出中文名称
        description: 输出描述
        output_format: 输出格式
        count: 输出数量（默认为1）
    
    Example:
        >>> output = OutputSpec(
        ...     name="midpoint",
        ...     chinese_name="中点",
        ...     description="线段的中点",
        ...     output_format=OutputFormat.POINT
        ... )
    """
    name: str
    chinese_name: str
    description: str
    output_format: OutputFormat
    count: int = 1
    
    def __post_init__(self) -> None:
        """验证输出规格的有效性。"""
        if not self.name or not isinstance(self.name, str):
            raise ValueError("输出名称必须是非空字符串")
        if self.count < 1:
            raise ValueError("输出数量必须至少为1")


@dataclass
class FuncBlockSpec:
    """
    函数块规格说明数据类。
    
    描述一个预设函数块的完整规格，包括其输入输出、
    数学描述、前置条件和使用示例。
    
    Attributes:
        name: 函数块名称（英文标识符）
        chinese_name: 中文名称
        category: 所属分类
        description: 详细描述
        params: 输入参数规格列表
        outputs: 输出规格列表
        determinism: 确定性级别
        preconditions: 前置条件列表
        mathematical_definition: 数学定义描述
        example_usage: 使用示例代码
        notes: 额外注意事项
        version: 版本号
        author: 作者信息
    
    Example:
        >>> spec = FuncBlockSpec(
        ...     name="midpoint",
        ...     chinese_name="中点",
        ...     category=PresetCategory.BASIC,
        ...     description="计算两点的中点",
        ...     params=[
        ...         ParamSpec("p1", "点1", "第一个点", "Point"),
        ...         ParamSpec("p2", "点2", "第二个点", "Point"),
        ...     ],
        ...     outputs=[OutputSpec("mid", "中点", "中点", OutputFormat.POINT)],
        ...     determinism=DeterminismLevel.ALWAYS_UNIQUE,
        ... )
    """
    name: str
    chinese_name: str
    category: PresetCategory
    description: str
    params: List[ParamSpec] = field(default_factory=list)
    outputs: List[OutputSpec] = field(default_factory=list)
    determinism: DeterminismLevel = DeterminismLevel.ALWAYS_UNIQUE
    preconditions: List[str] = field(default_factory=list)
    mathematical_definition: str = ""
    example_usage: str = ""
    notes: str = ""
    version: str = "4.0.0"
    author: str = "Lv-00 开发团队"
    
    def __post_init__(self) -> None:
        """验证函数块规格的有效性。"""
        if not self.name or not isinstance(self.name, str):
            raise ValueError("函数块名称必须是非空字符串")
        if not re.match(r'^[a-z][a-z0-9_]*$', self.name):
            raise ValueError(f"函数块名称 '{self.name}' 必须符合 snake_case 命名规范")
    
    @property
    def input_count(self) -> int:
        """获取输入参数数量。"""
        return len(self.params)
    
    @property
    def output_count(self) -> int:
        """获取输出数量。"""
        return sum(o.count for o in self.outputs)
    
    def get_param(self, name: str) -> Optional[ParamSpec]:
        """根据名称获取参数规格。"""
        for param in self.params:
            if param.name == name:
                return param
        return None


@dataclass
class ValidationResult:
    """
    验证结果数据类。
    
    封装输入参数验证的结果。
    
    Attributes:
        valid: 验证是否通过
        errors: 错误消息列表
        warnings: 警告消息列表
    """
    valid: bool
    errors: List[str] = field(default_factory=list)
    warnings: List[str] = field(default_factory=list)
    
    def __bool__(self) -> bool:
        """使验证结果可直接用于布尔判断。"""
        return self.valid
    
    def add_error(self, message: str) -> None:
        """添加错误消息。"""
        self.errors.append(message)
        self.valid = False
    
    def add_warning(self, message: str) -> None:
        """添加警告消息。"""
        self.warnings.append(message)


# ============================================================
# 注册表系统
# ============================================================

class PresetRegistry:
    """
    预设函数块注册表。
    
    管理所有预设函数块规格的全局注册表，提供注册、查询、
    分类检索等功能。
    
    本类采用单例模式，确保全局只有一个注册表实例。
    
    Attributes:
        _specs: 存储所有预设函数块规格的字典
    
    Example:
        >>> registry = get_registry()
        >>> spec = registry.get("midpoint")
        >>> specs = registry.list_by_category(PresetCategory.BASIC)
    """
    
    _instance: Optional['PresetRegistry'] = None
    
    def __new__(cls) -> 'PresetRegistry':
        """确保单例模式。"""
        if cls._instance is None:
            cls._instance = super().__new__(cls)
            cls._instance._specs: Dict[str, FuncBlockSpec] = {}
        return cls._instance
    
    def register(self, spec: FuncBlockSpec) -> None:
        """
        注册预设函数块规格。
        
        将给定的函数块规格添加到注册表中。如果同名规格已存在，
        将覆盖旧规格。
        
        Args:
            spec: 函数块规格说明对象
        
        Raises:
            TypeError: spec 不是 FuncBlockSpec 类型
        
        Example:
            >>> registry = get_registry()
            >>> registry.register(spec)
        """
        if not isinstance(spec, FuncBlockSpec):
            raise TypeError(f"期望 FuncBlockSpec 类型，但收到 {type(spec).__name__}")
        self._specs[spec.name] = spec
    
    def get(self, name: str) -> Optional[FuncBlockSpec]:
        """
        根据名称获取预设函数块规格。
        
        Args:
            name: 函数块名称（英文标识符）
        
        Returns:
            FuncBlockSpec 或 None: 找到的规格对象，若不存在则返回 None
        
        Example:
            >>> spec = registry.get("midpoint")
            >>> if spec:
            ...     print(spec.chinese_name)
        """
        return self._specs.get(name)
    
    def list_all(self) -> List[str]:
        """
        列出所有已注册的预设函数块名称。
        
        Returns:
            List[str]: 预设函数块名称列表，按字母顺序排列
        
        Example:
            >>> names = registry.list_all()
            >>> print(names)
        """
        return sorted(self._specs.keys())
    
    def list_by_category(self, category: PresetCategory) -> List[FuncBlockSpec]:
        """
        按分类列出预设函数块规格。
        
        Args:
            category: 预设函数块分类枚举值
        
        Returns:
            List[FuncBlockSpec]: 该分类下的所有规格列表
        
        Example:
            >>> specs = registry.list_by_category(PresetCategory.BASIC)
            >>> for spec in specs:
            ...     print(spec.chinese_name)
        """
        return [spec for spec in self._specs.values() if spec.category == category]
    
    def search(self, keyword: str) -> List[FuncBlockSpec]:
        """
        根据关键词搜索预设函数块。
        
        在名称、中文名称和描述中搜索匹配的关键词。
        
        Args:
            keyword: 搜索关键词
        
        Returns:
            List[FuncBlockSpec]: 匹配的规格列表
        
        Example:
            >>> specs = registry.search("中点")
        """
        keyword_lower = keyword.lower()
        results = []
        for spec in self._specs.values():
            if (keyword_lower in spec.name.lower() or
                keyword_lower in spec.chinese_name.lower() or
                keyword_lower in spec.description.lower()):
                results.append(spec)
        return results
    
    def clear(self) -> None:
        """清空注册表（主要用于测试）。"""
        self._specs.clear()
    
    def __len__(self) -> int:
        """返回注册表中预设函数块的数量。"""
        return len(self._specs)
    
    def __contains__(self, name: str) -> bool:
        """检查指定名称的预设是否存在。"""
        return name in self._specs


# 全局注册表实例
_registry: Optional[PresetRegistry] = None


def get_registry() -> PresetRegistry:
    """
    获取全局预设函数块注册表实例。
    
    Returns:
        PresetRegistry: 全局注册表实例
    
    Example:
        >>> registry = get_registry()
        >>> spec = registry.get("midpoint")
    """
    return PresetRegistry()


def register_preset(spec: FuncBlockSpec) -> None:
    """
    注册预设函数块规格到全局注册表。
    
    便捷函数，等价于 get_registry().register(spec)。
    
    Args:
        spec: 函数块规格说明对象
    
    Example:
        >>> register_preset(spec)
    """
    get_registry().register(spec)


def get_preset_spec(name: str) -> Optional[FuncBlockSpec]:
    """
    根据名称从全局注册表获取预设函数块规格。
    
    便捷函数，等价于 get_registry().get(name)。
    
    Args:
        name: 函数块名称
    
    Returns:
        FuncBlockSpec 或 None: 找到的规格对象
    
    Example:
        >>> spec = get_preset_spec("midpoint")
    """
    return get_registry().get(name)


def list_all_presets() -> List[str]:
    """
    列出全局注册表中所有预设函数块名称。
    
    便捷函数，等价于 get_registry().list_all()。
    
    Returns:
        List[str]: 预设函数块名称列表
    
    Example:
        >>> names = list_all_presets()
    """
    return get_registry().list_all()


def list_presets_by_category(category: PresetCategory) -> List[FuncBlockSpec]:
    """
    按分类列出全局注册表中的预设函数块规格。
    
    便捷函数，等价于 get_registry().list_by_category(category)。
    
    Args:
        category: 预设函数块分类
    
    Returns:
        List[FuncBlockSpec]: 该分类下的所有规格列表
    
    Example:
        >>> specs = list_presets_by_category(PresetCategory.BASIC)
    """
    return get_registry().list_by_category(category)


# ============================================================
# 工具函数
# ============================================================

def validate_inputs(spec: FuncBlockSpec, inputs: List[Any]) -> ValidationResult:
    """
    验证输入参数是否符合函数块规格。
    
    检查输入参数的数量和基本有效性。
    
    Args:
        spec: 函数块规格
        inputs: 输入参数列表
    
    Returns:
        ValidationResult: 验证结果
    
    Example:
        >>> result = validate_inputs(spec, [p1, p2])
        >>> if not result.valid:
        ...     print(result.errors)
    """
    result = ValidationResult(valid=True)
    
    # 检查参数数量
    required_count = sum(1 for p in spec.params if p.required)
    if len(inputs) < required_count:
        result.add_error(
            f"输入参数数量不足: 至少需要 {required_count} 个必需参数，"
            f"实际提供了 {len(inputs)} 个"
        )
        return result
    
    if len(inputs) > len(spec.params):
        result.add_warning(
            f"输入参数数量过多: 期望最多 {len(spec.params)} 个参数，"
            f"实际提供了 {len(inputs)} 个，多余的参数将被忽略"
        )
    
    # 检查 None 值
    for i, inp in enumerate(inputs):
        if inp is None:
            result.add_error(f"第 {i+1} 个输入参数为 None")
    
    return result


def format_math_definition(definition: str, indent: int = 4) -> str:
    """
    格式化数学定义字符串。
    
    清理多余的空白字符，统一缩进格式。
    
    Args:
        definition: 原始数学定义字符串
        indent: 缩进空格数
    
    Returns:
        str: 格式化后的字符串
    
    Example:
        >>> formatted = format_math_definition("  设 P = (x, y)  \n  则...")
    """
    lines = definition.strip().split('\n')
    formatted_lines = []
    for line in lines:
        stripped = line.strip()
        if stripped:
            formatted_lines.append(' ' * indent + stripped)
    return '\n'.join(formatted_lines)


def get_preset_info(name: str) -> str:
    """
    获取预设函数块的详细信息字符串。
    
    格式化输出预设函数块的完整规格说明。
    
    Args:
        name: 预设函数块名称
    
    Returns:
        str: 格式化的信息字符串
    
    Example:
        >>> info = get_preset_info("midpoint")
        >>> print(info)
    """
    spec = get_preset_spec(name)
    if spec is None:
        return f"未找到预设函数块: {name}"
    
    lines = [
        f"{'=' * 50}",
        f"  {spec.chinese_name} ({spec.name})",
        f"{'=' * 50}",
        f"分类: {spec.category}",
        f"确定性: {spec.determinism}",
        f"版本: {spec.version}",
        f"",
        f"描述:",
        f"  {spec.description.strip()}",
    ]
    
    if spec.params:
        lines.extend([
            f"",
            f"输入参数:",
        ])
        for param in spec.params:
            req_mark = " [必需]" if param.required else " [可选]"
            lines.append(f"  - {param.chinese_name} ({param.name}): {param.description}{req_mark}")
    
    if spec.outputs:
        lines.extend([
            f"",
            f"输出:",
        ])
        for output in spec.outputs:
            lines.append(f"  - {output.chinese_name} ({output.name}): {output.description}")
    
    if spec.preconditions:
        lines.extend([
            f"",
            f"前置条件:",
        ])
        for cond in spec.preconditions:
            lines.append(f"  - {cond}")
    
    if spec.mathematical_definition:
        lines.extend([
            f"",
            f"数学定义:",
            format_math_definition(spec.mathematical_definition),
        ])
    
    if spec.notes:
        lines.extend([
            f"",
            f"注意事项:",
            f"  {spec.notes}",
        ])
    
    if spec.example_usage:
        lines.extend([
            f"",
            f"使用示例:",
            format_math_definition(spec.example_usage),
        ])
    
    lines.append(f"{'=' * 50}")
    
    return '\n'.join(lines)


# ============================================================
# 模块导出
# ============================================================

__all__ = [
    # 枚举类型
    "PresetCategory",
    "DeterminismLevel",
    "OutputFormat",
    
    # 数据类
    "ParamSpec",
    "OutputSpec",
    "FuncBlockSpec",
    "ValidationResult",
    
    # 注册表
    "PresetRegistry",
    "get_registry",
    "register_preset",
    "get_preset_spec",
    "list_all_presets",
    "list_presets_by_category",
    
    # 工具函数
    "validate_inputs",
    "format_math_definition",
    "get_preset_info",
]
