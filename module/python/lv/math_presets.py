"""
Lv-00 理论数学研究预设函数块模块

提供理论数学研究中常用的预设函数块，包括：
    - 数论运算：最大公约数、素性检测、同余运算等
    - 群论运算：群操作、子群检测、同态同构等
    - 拓扑运算：开集判定、连续映射、紧致性等
    - 分析运算：极限、微分、积分、级数等

设计原则：
    1. 每个预设函数块都有明确的数学语义
    2. 支持符号计算，保持精确性
    3. 提供完整的中文文档和 LaTeX 数学描述
    4. 遵循局部最优解原则

版本：3.3.0
作者：Lv-00 开发团队
"""

from __future__ import annotations
from typing import TYPE_CHECKING, Any, Callable, Dict, List, Optional, Tuple, Union
from enum import Enum, auto
from dataclasses import dataclass, field
import math

# 版本号统一引用包级 __version__
from . import __version__

# 类型检查时导入，避免运行时循环依赖
if TYPE_CHECKING:
    from .core import Graph, Point, LineSegment, SymbolicCoord
    from .func_block import FuncBlock, SolutionSelector


# ============================================================
# 枚举和常量定义
# ============================================================

class MathPresetCategory(Enum):
    """
    理论数学预设函数块分类枚举。
    
    按照数学分支对预设函数块进行分类，
    便于用户查找和管理。
    
    枚举值：
        NUMBER_THEORY: 数论运算
        GROUP_THEORY: 群论运算
        TOPOLOGY: 拓扑构造
        ANALYSIS: 分析运算
        ALGEBRA: 代数运算
        LOGIC: 逻辑推导
    """
    NUMBER_THEORY = auto()    # 数论运算
    GROUP_THEORY = auto()     # 群论运算
    TOPOLOGY = auto()         # 拓扑构造
    ANALYSIS = auto()         # 分析运算
    ALGEBRA = auto()          # 代数运算
    LOGIC = auto()            # 逻辑推导


class ComplexityLevel(Enum):
    """
    算法复杂度级别枚举。
    
    描述函数块的计算复杂度等级。
    
    枚举值：
        O1: 常数时间
        OLOGN: 对数时间
        ON: 线性时间
        ONLOGN: 线性对数
        ON2: 平方时间
        ON3: 立方时间
        OEXP: 指数时间
        OINFINITE: 无穷（理论计算）
    """
    O1 = auto()           # O(1)
    OLOGN = auto()        # O(log n)
    ON = auto()           # O(n)
    ONLOGN = auto()       # O(n log n)
    ON2 = auto()          # O(n²)
    ON3 = auto()          # O(n³)
    OEXP = auto()         # O(2^n) 或更高
    OINFINITE = auto()    # 理论计算（无穷）
    OSQRTN = auto()       # O(√n)，供 euler_totient 和 mobius_function 使用


@dataclass
class MathFuncBlockSpec:
    """
    数学函数块规格说明数据类。
    
    描述一个数学预设函数块的完整规格，包括其输入输出、
    数学描述、前置条件和使用示例。
    
    属性：
        name: 函数块名称（英文标识符）
        chinese_name: 中文名称
        category: 所属分类
        description: 详细描述
        input_types: 输入类型列表
        output_type: 输出类型
        complexity: 时间复杂度
        mathematical_definition: 数学定义（LaTeX格式）
        preconditions: 前置条件列表
        properties: 数学性质
        example_usage: 使用示例代码
        notes: 额外注意事项
    """
    name: str
    chinese_name: str
    category: MathPresetCategory
    description: str
    input_types: List[str]
    output_type: str
    complexity: ComplexityLevel = ComplexityLevel.ON
    mathematical_definition: str = ""
    preconditions: List[str] = field(default_factory=list)
    properties: List[str] = field(default_factory=list)
    example_usage: str = ""
    notes: str = ""


# ============================================================
# 预设函数块规格注册表
# ============================================================

# 全局数学预设函数块规格注册表
_MATH_PRESET_SPECS: Dict[str, MathFuncBlockSpec] = {}


def register_math_preset(spec: MathFuncBlockSpec) -> None:
    """
    注册数学预设函数块规格到全局注册表。
    
    参数：
        spec: 数学函数块规格说明对象
    """
    _MATH_PRESET_SPECS[spec.name] = spec


def get_math_preset_spec(name: str) -> Optional[MathFuncBlockSpec]:
    """
    根据名称获取数学预设函数块规格。
    
    参数：
        name: 函数块名称（英文标识符）
        
    返回：
        MathFuncBlockSpec 或 None: 找到的规格对象
    """
    return _MATH_PRESET_SPECS.get(name)


def list_all_math_presets() -> List[str]:
    """
    列出所有已注册的数学预设函数块名称。
    
    返回：
        List[str]: 预设函数块名称列表
    """
    return list(_MATH_PRESET_SPECS.keys())


def list_math_presets_by_category(category: MathPresetCategory) -> List[MathFuncBlockSpec]:
    """
    按分类列出数学预设函数块规格。
    
    参数：
        category: 数学预设函数块分类枚举值
        
    返回：
        List[MathFuncBlockSpec]: 该分类下的所有规格列表
    """
    return [spec for spec in _MATH_PRESET_SPECS.values() 
            if spec.category == category]


# ============================================================
# 数论运算预设
# ============================================================

# ---- 最大公约数 ----
register_math_preset(MathFuncBlockSpec(
    name="gcd",
    chinese_name="最大公约数",
    category=MathPresetCategory.NUMBER_THEORY,
    description="""
    计算两个整数的最大公约数。
    
    给定两个整数 a 和 b，计算它们的最大公约数 gcd(a, b)。
    使用欧几里得算法（辗转相除法）。
    
    数学性质：
    - gcd(a, b) = gcd(b, a mod b)
    - gcd(a, 0) = |a|
    - gcd(a, b) · lcm(a, b) = |a · b|
    """,
    input_types=["integer", "integer"],
    output_type="integer",
    complexity=ComplexityLevel.OLOGN,
    mathematical_definition=r"\gcd(a, b) = \max\{d : d|a \land d|b\}",
    preconditions=[
        "a 和 b 不能同时为零"
    ],
    properties=["交换性", "结合性", "确定性"],
    example_usage="""
    >>> from lv.math_presets import number_theory
    >>> result = number_theory.gcd(48, 18)
    >>> print(result)  # 6
    """,
    notes="欧几里得算法是最古老的高效算法之一。"
))

# ---- 最小公倍数 ----
register_math_preset(MathFuncBlockSpec(
    name="lcm",
    chinese_name="最小公倍数",
    category=MathPresetCategory.NUMBER_THEORY,
    description="""
    计算两个整数的最小公倍数。
    
    给定两个整数 a 和 b，计算它们的最小公倍数 lcm(a, b)。
    利用公式：lcm(a, b) = |a · b| / gcd(a, b)。
    """,
    input_types=["integer", "integer"],
    output_type="integer",
    complexity=ComplexityLevel.OLOGN,
    mathematical_definition=r"\text{lcm}(a, b) = \min\{m > 0 : a|m \land b|m\}",
    properties=["交换性", "结合性", "确定性"],
    example_usage="""
    >>> from lv.math_presets import number_theory
    >>> result = number_theory.lcm(12, 18)
    >>> print(result)  # 36
    """
))

# ---- 扩展欧几里得 ----
register_math_preset(MathFuncBlockSpec(
    name="extended_gcd",
    chinese_name="扩展欧几里得",
    category=MathPresetCategory.NUMBER_THEORY,
    description="""
    扩展欧几里得算法：求解 ax + by = gcd(a, b) 的整数解。
    
    不仅计算 gcd(a, b)，还找到整数 x, y 使得：
    ax + by = gcd(a, b)
    
    这是求解线性丢番图方程的基础。
    """,
    input_types=["integer", "integer"],
    output_type="tuple",
    complexity=ComplexityLevel.OLOGN,
    mathematical_definition=r"\exists x, y \in \mathbb{Z}: ax + by = \gcd(a,b)",
    properties=["构造性", "确定性"],
    example_usage="""
    >>> from lv.math_presets import number_theory
    >>> g, x, y = number_theory.extended_gcd(35, 15)
    >>> print(g, x, y)  # 5, 1, -2 (因为 35*1 + 15*(-2) = 5)
    """
))

# ---- 模逆元 ----
register_math_preset(MathFuncBlockSpec(
    name="modular_inverse",
    chinese_name="模逆元",
    category=MathPresetCategory.NUMBER_THEORY,
    description="""
    计算 a 模 m 的乘法逆元。
    
    给定整数 a 和 m，找到整数 x 使得：
    ax ≡ 1 (mod m)
    
    逆元存在的充要条件是 gcd(a, m) = 1。
    """,
    input_types=["integer", "integer"],
    output_type="integer",
    complexity=ComplexityLevel.OLOGN,
    mathematical_definition=r"a^{-1} \mod m \text{ 满足 } a \cdot a^{-1} \equiv 1 \pmod{m}",
    preconditions=[
        "gcd(a, m) = 1（a 和 m 互质）"
    ],
    properties=["构造性", "确定性"],
    notes="模逆元在 RSA 加密中有重要应用。"
))

# ---- 素性检测 ----
register_math_preset(MathFuncBlockSpec(
    name="primality_test",
    chinese_name="素性检测",
    category=MathPresetCategory.NUMBER_THEORY,
    description="""
    Miller-Rabin 素性检测算法。
    
    判断一个整数 n 是否为素数。
    这是一个概率性算法，但可以通过多次测试降低错误率。
    """,
    input_types=["integer", "integer"],
    output_type="boolean",
    complexity=ComplexityLevel.ON3,
    mathematical_definition=r"n \text{ 是素数 } \Rightarrow \text{返回真}",
    properties=["概率性", "确定性（当返回假时）"],
    notes="参数 k 为测试轮数，错误概率不超过 4^(-k)。"
))

# ---- 欧拉函数 ----
register_math_preset(MathFuncBlockSpec(
    name="euler_totient",
    chinese_name="欧拉函数",
    category=MathPresetCategory.NUMBER_THEORY,
    description="""
    计算欧拉函数 φ(n)。
    
    φ(n) 表示不超过 n 且与 n 互质的正整数个数。
    
    性质：
    - 若 p 是素数，则 φ(p) = p - 1
    - 若 gcd(m, n) = 1，则 φ(mn) = φ(m)φ(n)
    - 欧拉定理：若 gcd(a, n) = 1，则 a^φ(n) ≡ 1 (mod n)
    """,
    input_types=["integer"],
    output_type="integer",
    complexity=ComplexityLevel.OSQRTN,
    mathematical_definition=r"\varphi(n) = |\{k : 1 \le k \le n, \gcd(k,n) = 1\}|",
    properties=["积性函数", "确定性"],
    example_usage="""
    >>> from lv.math_presets import number_theory
    >>> result = number_theory.euler_totient(12)
    >>> print(result)  # 4 (因为 1, 5, 7, 11 与 12 互质)
    """
))

# ---- 莫比乌斯函数 ----
register_math_preset(MathFuncBlockSpec(
    name="mobius_function",
    chinese_name="莫比乌斯函数",
    category=MathPresetCategory.NUMBER_THEORY,
    description="""
    计算莫比乌斯函数 μ(n)。
    
    定义：
    - μ(1) = 1
    - 若 n 有平方因子，μ(n) = 0
    - 若 n 是 k 个不同素数的乘积，μ(n) = (-1)^k
    
    莫比乌斯反演公式的基础。
    """,
    input_types=["integer"],
    output_type="integer",
    complexity=ComplexityLevel.OSQRTN,
    mathematical_definition=r"\mu(n) = \begin{cases} 1 & n=1 \\ 0 & \exists p: p^2|n \\ (-1)^k & n = p_1 \cdots p_k \end{cases}",
    properties=["积性函数", "确定性"],
    notes="莫比乌斯反演：f(n) = Σ_{d|n} g(d) ⟺ g(n) = Σ_{d|n} μ(d)f(n/d)"
))

# ---- 中国剩余定理 ----
register_math_preset(MathFuncBlockSpec(
    name="chinese_remainder",
    chinese_name="中国剩余定理",
    category=MathPresetCategory.NUMBER_THEORY,
    description="""
    中国剩余定理：求解同余方程组。
    
    给定同余方程组：
    x ≡ a_i (mod m_i), i = 1, 2, ..., k
    
    当 m_i 两两互质时，存在唯一解（模 M = m_1·m_2·...·m_k）。
    """,
    input_types=["sequence", "sequence"],
    output_type="integer",
    complexity=ComplexityLevel.ON,
    mathematical_definition=r"x \equiv a_i \pmod{m_i}, \quad \gcd(m_i, m_j) = 1",
    preconditions=[
        "模数两两互质"
    ],
    properties=["构造性", "确定性"],
    notes="《孙子算经》中的'物不知数'问题是最早的应用。"
))


# ============================================================
# 群论运算预设
# ============================================================

# ---- 群运算 ----
register_math_preset(MathFuncBlockSpec(
    name="group_operation",
    chinese_name="群运算",
    category=MathPresetCategory.GROUP_THEORY,
    description="""
    群运算：计算群中两个元素的乘积。
    
    给定群 G 和两个元素 a, b ∈ G，计算 a · b。
    群运算满足：
    - 封闭性：a · b ∈ G
    - 结合性：(a · b) · c = a · (b · c)
    - 单位元：存在 e 使得 e · a = a · e = a
    - 逆元：对每个 a 存在 a^(-1) 使得 a · a^(-1) = e
    """,
    input_types=["group", "group_element", "group_element"],
    output_type="group_element",
    complexity=ComplexityLevel.O1,
    mathematical_definition=r"a \cdot b \in G",
    properties=["封闭性", "结合性", "确定性"],
    notes="群的运算通常用乘法或加法表示。"
))

# ---- 元素阶 ----
register_math_preset(MathFuncBlockSpec(
    name="element_order",
    chinese_name="元素阶",
    category=MathPresetCategory.GROUP_THEORY,
    description="""
    计算群元素的阶。
    
    元素 a 的阶 ord(a) 是使得 a^n = e 的最小正整数 n。
    如果不存在这样的 n，则称 a 有无穷阶。
    
    拉格朗日定理：ord(a) 整除 |G|。
    """,
    input_types=["group", "group_element"],
    output_type="integer",
    complexity=ComplexityLevel.ON,
    mathematical_definition=r"\text{ord}(a) = \min\{n > 0 : a^n = e\}",
    properties=["确定性"],
    notes="有限群中每个元素的阶都是有限的。"
))

# ---- 子群判定 ----
register_math_preset(MathFuncBlockSpec(
    name="subgroup_test",
    chinese_name="子群判定",
    category=MathPresetCategory.GROUP_THEORY,
    description="""
    判定子集是否构成子群。
    
    使用单步子群判定法：
    H 是 G 的子群 ⟺ 对所有 a, b ∈ H，有 ab^(-1) ∈ H。
    """,
    input_types=["group", "subgroup"],
    output_type="boolean",
    complexity=ComplexityLevel.ON2,
    mathematical_definition=r"H \le G \Leftrightarrow \forall a,b \in H: ab^{-1} \in H",
    properties=["确定性"],
    notes="子群判定是群论中的基本操作。"
))

# ---- 正规子群判定 ----
register_math_preset(MathFuncBlockSpec(
    name="normal_subgroup_test",
    chinese_name="正规子群判定",
    category=MathPresetCategory.GROUP_THEORY,
    description="""
    判定子群是否是正规子群。
    
    H 是 G 的正规子群 ⟺ 对所有 g ∈ G，有 gHg^(-1) = H。
    
    正规子群是构造商群的前提。
    """,
    input_types=["group", "subgroup"],
    output_type="boolean",
    complexity=ComplexityLevel.ON2,
    mathematical_definition=r"H \triangleleft G \Leftrightarrow gHg^{-1} = H, \forall g \in G",
    properties=["确定性"],
    notes="商群 G/H 只有在 H 是正规子群时才有意义。"
))

# ---- 循环群判定 ----
register_math_preset(MathFuncBlockSpec(
    name="cyclic_group_test",
    chinese_name="循环群判定",
    category=MathPresetCategory.GROUP_THEORY,
    description="""
    判定群是否是循环群。
    
    G 是循环群 ⟺ 存在 g ∈ G 使得 G = ⟨g⟩。
    
    循环群是最简单的群结构，所有循环群都是阿贝尔群。
    """,
    input_types=["group"],
    output_type="boolean",
    complexity=ComplexityLevel.ON,
    mathematical_definition=r"G \text{ 循环} \Leftrightarrow \exists g: G = \langle g \rangle",
    properties=["确定性"],
    notes="有限循环群同构于 Z_n，无限循环群同构于 Z。"
))

# ---- 群中心 ----
register_math_preset(MathFuncBlockSpec(
    name="group_center",
    chinese_name="群中心",
    category=MathPresetCategory.GROUP_THEORY,
    description="""
    计算群的中心 Z(G)。
    
    中心是与所有元素交换的元素集合：
    Z(G) = {z ∈ G : zg = gz, ∀g ∈ G}
    
    中心是群的正规子群。
    """,
    input_types=["group"],
    output_type="subgroup",
    complexity=ComplexityLevel.ON2,
    mathematical_definition=r"Z(G) = \{z \in G : zg = gz, \forall g \in G\}",
    properties=["构造性", "确定性"],
    notes="阿贝尔群的中心就是群本身。"
))


# ============================================================
# 拓扑运算预设
# ============================================================

# ---- 拓扑判定 ----
register_math_preset(MathFuncBlockSpec(
    name="topology_test",
    chinese_name="拓扑判定",
    category=MathPresetCategory.TOPOLOGY,
    description="""
    判定集合族是否构成拓扑。
    
    τ 是 X 上的拓扑当且仅当：
    1. ∅, X ∈ τ
    2. τ 对任意并封闭
    3. τ 对有限交封闭
    """,
    input_types=["set", "set_family"],
    output_type="boolean",
    complexity=ComplexityLevel.ON2,
    mathematical_definition=r"\mathcal{T} \text{ 是拓扑} \Leftrightarrow \emptyset, X \in \mathcal{T}, "
                           r"\mathcal{T} \text{ 对任意并、有限交封闭}",
    properties=["确定性"],
    notes="拓扑是拓扑学的基本概念。"
))

# ---- 开集判定 ----
register_math_preset(MathFuncBlockSpec(
    name="open_set_test",
    chinese_name="开集判定",
    category=MathPresetCategory.TOPOLOGY,
    description="""
    判定集合是否是拓扑空间中的开集。
    
    U 是开集 ⟺ U ∈ τ。
    """,
    input_types=["topological_space", "set"],
    output_type="boolean",
    complexity=ComplexityLevel.ON,
    mathematical_definition=r"U \in \mathcal{T}",
    properties=["确定性"],
    notes="开集是拓扑空间的基本概念。"
))

# ---- 闭包计算 ----
register_math_preset(MathFuncBlockSpec(
    name="closure",
    chinese_name="闭包计算",
    category=MathPresetCategory.TOPOLOGY,
    description="""
    计算集合的闭包。
    
    集合 A 的闭包是包含 A 的最小闭集：
    Ā = ∩{F : A ⊆ F, F 是闭集}
    """,
    input_types=["topological_space", "set"],
    output_type="closed_set",
    complexity=ComplexityLevel.ON,
    mathematical_definition=r"\bar{A} = \bigcap \{F : A \subseteq F, F \text{ 闭}\}",
    properties=["构造性", "确定性", "幂等性"],
    notes="闭包算子满足 Kuratowski 闭包公理。"
))

# ---- 连续映射判定 ----
register_math_preset(MathFuncBlockSpec(
    name="continuous_map_test",
    chinese_name="连续映射判定",
    category=MathPresetCategory.TOPOLOGY,
    description="""
    判定映射是否连续。
    
    f: X → Y 连续 ⟺ 对每个开集 V ⊆ Y，f^(-1)(V) 是 X 中的开集。
    """,
    input_types=["topological_space", "topological_space", "function"],
    output_type="boolean",
    complexity=ComplexityLevel.ON,
    mathematical_definition=r"f \text{ 连续} \Leftrightarrow f^{-1}(U) \in \mathcal{T}_X, \forall U \in \mathcal{T}_Y",
    properties=["确定性"],
    notes="连续性是拓扑学的核心概念。"
))

# ---- 紧致空间判定 ----
register_math_preset(MathFuncBlockSpec(
    name="compact_space_test",
    chinese_name="紧致空间判定",
    category=MathPresetCategory.TOPOLOGY,
    description="""
    判定拓扑空间是否紧致。
    
    X 紧致 ⟺ X 的每个开覆盖都有有限子覆盖。
    
    Heine-Borel 定理：R^n 中的有界闭集是紧致的。
    """,
    input_types=["topological_space"],
    output_type="boolean",
    complexity=ComplexityLevel.OINFINITE,
    mathematical_definition=r"X \text{ 紧致} \Leftrightarrow \text{每个开覆盖有有限子覆盖}",
    properties=["确定性"],
    notes="紧致性是拓扑学中最重要的概念之一。"
))

# ---- 连通空间判定 ----
register_math_preset(MathFuncBlockSpec(
    name="connected_space_test",
    chinese_name="连通空间判定",
    category=MathPresetCategory.TOPOLOGY,
    description="""
    判定拓扑空间是否连通。
    
    X 连通 ⟺ X 不能表示为两个非空不相交开集的并。
    """,
    input_types=["topological_space"],
    output_type="boolean",
    complexity=ComplexityLevel.ON2,
    mathematical_definition=r"X \text{ 连通} \Leftrightarrow \nexists U, V \in \mathcal{T} \setminus \{\emptyset\}: "
                          r"U \cup V = X, U \cap V = \emptyset",
    properties=["确定性"],
    notes="连通性是拓扑不变量。"
))


# ============================================================
# 分析运算预设
# ============================================================

# ---- 数列极限 ----
register_math_preset(MathFuncBlockSpec(
    name="sequence_limit",
    chinese_name="数列极限",
    category=MathPresetCategory.ANALYSIS,
    description="""
    计算数列的极限。
    
    lim(n→∞) a_n = L ⟺ 对任意 ε > 0，存在 N 使得 n > N 时 |a_n - L| < ε。
    
    收敛数列的性质：
    - 唯一性：极限若存在则唯一
    - 有界性：收敛数列必有界
    - 保号性：若 a_n ≥ 0 且极限存在，则极限 ≥ 0
    """,
    input_types=["sequence"],
    output_type="limit",
    complexity=ComplexityLevel.OINFINITE,
    mathematical_definition=r"\lim_{n \to \infty} a_n = L \Leftrightarrow "
                           r"\forall \epsilon > 0, \exists N: n > N \Rightarrow |a_n - L| < \epsilon",
    properties=["确定性（若存在）"],
    notes="极限是分析学的基础概念。"
))

# ---- 导数计算 ----
register_math_preset(MathFuncBlockSpec(
    name="derivative",
    chinese_name="导数计算",
    category=MathPresetCategory.ANALYSIS,
    description="""
    计算函数在某点的导数。
    
    f'(a) = lim(h→0) [f(a+h) - f(a)] / h
    
    导数的几何意义：切线斜率
    导数的物理意义：瞬时变化率
    """,
    input_types=["function", "scalar"],
    output_type="derivative",
    complexity=ComplexityLevel.OINFINITE,
    mathematical_definition=r"f'(a) = \lim_{h \to 0} \frac{f(a+h) - f(a)}{h}",
    properties=["线性性", "确定性（若可微）"],
    notes="导数是微积分的核心概念。"
))

# ---- 定积分 ----
register_math_preset(MathFuncBlockSpec(
    name="definite_integral",
    chinese_name="定积分",
    category=MathPresetCategory.ANALYSIS,
    description="""
    计算定积分 ∫ₐᵇ f(x)dx。
    
    定积分的几何意义：曲线下的面积。
    
    微积分基本定理：
    ∫ₐᵇ f(x)dx = F(b) - F(a)，其中 F' = f。
    """,
    input_types=["function", "scalar", "scalar"],
    output_type="scalar",
    complexity=ComplexityLevel.OINFINITE,
    mathematical_definition=r"\int_a^b f(x) \, dx = F(b) - F(a)",
    properties=["线性性", "确定性"],
    notes="黎曼积分和勒贝格积分是两种主要的积分理论。"
))

# ---- 泰勒展开 ----
register_math_preset(MathFuncBlockSpec(
    name="taylor_expansion",
    chinese_name="泰勒展开",
    category=MathPresetCategory.ANALYSIS,
    description="""
    计算函数在 a 点的 n 阶泰勒展开。
    
    f(x) = Σ_{k=0}^n [f^(k)(a) / k!] (x-a)^k + R_n(x)
    
    其中 R_n(x) 是余项。
    """,
    input_types=["function", "scalar", "integer"],
    output_type="polynomial",
    complexity=ComplexityLevel.ON,
    mathematical_definition=r"f(x) = \sum_{k=0}^{n} \frac{f^{(k)}(a)}{k!}(x-a)^k + R_n(x)",
    properties=["构造性", "确定性"],
    notes="泰勒级数是函数逼近的重要工具。"
))

# ---- 级数收敛判定 ----
register_math_preset(MathFuncBlockSpec(
    name="series_convergence_test",
    chinese_name="级数收敛判定",
    category=MathPresetCategory.ANALYSIS,
    description="""
    判定数项级数是否收敛。
    
    使用多种判别法：
    - 比较判别法
    - 比值判别法（d'Alembert）
    - 根值判别法（Cauchy）
    - 积分判别法
    """,
    input_types=["sequence"],
    output_type="boolean",
    complexity=ComplexityLevel.OINFINITE,
    mathematical_definition=r"\sum a_n \text{ 收敛} \Leftrightarrow \{S_n\} \text{ 收敛}",
    properties=["确定性"],
    notes="收敛性分析是级数理论的核心。"
))


# ============================================================
# 实现函数
# ============================================================

class NumberTheory:
    """
    数论运算模块。
    
    提供数论相关的预设函数块实现。
    """
    
    @staticmethod
    def gcd(a: int, b: int) -> int:
        """
        计算最大公约数（欧几里得算法）。
        
        参数：
            a: 第一个整数
            b: 第二个整数
            
        返回：
            int: gcd(a, b)
            
        示例：
            >>> NumberTheory.gcd(48, 18)
            6
        """
        a, b = abs(a), abs(b)
        while b:
            a, b = b, a % b
        return a
    
    @staticmethod
    def lcm(a: int, b: int) -> int:
        """
        计算最小公倍数。
        
        参数：
            a: 第一个整数
            b: 第二个整数
            
        返回：
            int: lcm(a, b)
        """
        if a == 0 or b == 0:
            return 0
        return abs(a * b) // NumberTheory.gcd(a, b)
    
    @staticmethod
    def extended_gcd(a: int, b: int) -> Tuple[int, int, int]:
        """
        扩展欧几里得算法。
        
        参数：
            a: 第一个整数
            b: 第二个整数
            
        返回：
            Tuple[int, int, int]: (gcd, x, y) 使得 ax + by = gcd
        """
        if b == 0:
            return a, 1, 0
        g, x1, y1 = NumberTheory.extended_gcd(b, a % b)
        x = y1
        y = x1 - (a // b) * y1
        return g, x, y
    
    @staticmethod
    def modular_inverse(a: int, m: int) -> Optional[int]:
        """
        计算模逆元。
        
        参数：
            a: 整数
            m: 模数
            
        返回：
            Optional[int]: a 模 m 的逆元，若不存在则返回 None
        """
        g, x, _ = NumberTheory.extended_gcd(a % m, m)
        if g != 1:
            return None
        return x % m
    
    @staticmethod
    def euler_totient(n: int) -> int:
        """
        计算欧拉函数 φ(n)。
        
        参数：
            n: 正整数
            
        返回：
            int: φ(n)
        """
        if n <= 0:
            return 0
        result = n
        p = 2
        while p * p <= n:
            if n % p == 0:
                while n % p == 0:
                    n //= p
                result -= result // p
            p += 1
        if n > 1:
            result -= result // n
        return result
    
    @staticmethod
    def mobius(n: int) -> int:
        """
        计算莫比乌斯函数 μ(n)。
        
        参数：
            n: 正整数
            
        返回：
            int: μ(n) ∈ {-1, 0, 1}
        """
        if n == 1:
            return 1
        prime_count = 0
        p = 2
        temp = n
        while p * p <= temp:
            if temp % p == 0:
                temp //= p
                if temp % p == 0:
                    return 0
                prime_count += 1
            p += 1
        if temp > 1:
            prime_count += 1
        return -1 if prime_count % 2 else 1
    
    @staticmethod
    def is_prime_miller_rabin(n: int, k: int = 5) -> bool:
        """
        Miller-Rabin 素性检测。
        
        参数：
            n: 待检测的整数
            k: 测试轮数
            
        返回：
            bool: n 可能是素数（真）或 n 是合数（假）
        """
        import random
        
        if n < 2:
            return False
        if n == 2 or n == 3:
            return True
        if n % 2 == 0:
            return False
        
        # 分解 n-1 = 2^r * d
        r, d = 0, n - 1
        while d % 2 == 0:
            r += 1
            d //= 2
        
        # 进行 k 轮测试
        for _ in range(k):
            a = random.randrange(2, n - 1)
            x = pow(a, d, n)
            
            if x == 1 or x == n - 1:
                continue
            
            for _ in range(r - 1):
                x = pow(x, 2, n)
                if x == n - 1:
                    break
            else:
                return False
        
        return True


class GroupTheory:
    """
    群论运算模块。
    
    提供群论相关的预设函数块实现。
    """
    
    @staticmethod
    def permutation_multiply(p1: List[int], p2: List[int]) -> List[int]:
        """
        置换乘法（复合）。
        
        参数：
            p1: 第一个置换
            p2: 第二个置换
            
        返回：
            List[int]: p1 ∘ p2
        """
        n = len(p1)
        return [p1[p2[i] - 1] if min(p2) == 1 else p1[p2[i]] for i in range(n)]
    
    @staticmethod
    def permutation_inverse(p: List[int]) -> List[int]:
        """
        置换的逆。
        
        参数：
            p: 置换
            
        返回：
            List[int]: p^(-1)
        """
        n = len(p)
        inv = [0] * n
        for i in range(n):
            inv[p[i] - 1 if min(p) == 1 else p[i]] = i + 1 if min(p) == 1 else i
        return inv
    
    @staticmethod
    def permutation_decompose(p: List[int]) -> List[List[int]]:
        """
        将置换分解为不相交轮换的乘积。
        
        参数：
            p: 置换
            
        返回：
            List[List[int]]: 轮换列表
        """
        n = len(p)
        visited = [False] * n
        cycles = []
        
        for i in range(n):
            if not visited[i]:
                cycle = []
                j = i
                while not visited[j]:
                    visited[j] = True
                    cycle.append(j + 1 if min(p) == 1 else j)
                    j = p[j] - 1 if min(p) == 1 else p[j]
                if len(cycle) > 1:
                    cycles.append(cycle)
        
        return cycles


# ============================================================
# 模块导出
# ============================================================

__all__ = [
    # 枚举和常量
    'MathPresetCategory',
    'ComplexityLevel',
    
    # 数据类
    'MathFuncBlockSpec',
    
    # 注册表函数
    'register_math_preset',
    'get_math_preset_spec',
    'list_all_math_presets',
    'list_math_presets_by_category',
    
    # 实现模块
    'NumberTheory',
    'GroupTheory',
]
