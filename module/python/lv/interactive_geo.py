"""
Lv-00 交互几何模块

提供交互几何系统的 Python 接口，借鉴 Cinderella 与 Dr. Geo 的交互几何 UX 设计。

功能：
    - 几何画布状态管理（对象、选中、拖拽、视口）
    - 交互模式切换（点/线/圆/选择/拖拽/构造/测量/证明）
    - 拖拽交互与约束实时维护
    - 随机化定理验证（Cinderella 的 Randomized Theorem Checking）
    - 构造脚本生成（Dr. Geo 的 Smalltalk 代码生成思想）
    - 奇异配置检测（Cinderella Continuity 机制）
    - 状态快照与恢复（撤销/重做）

版本：3.3.0
作者：Lv-00 开发团队
"""

import ctypes
from typing import Any, List, Optional, Tuple

from ._ctypes_binding import _lib, c_int, c_double, c_char_p, c_void_p, c_bool, POINTER
from .core import lvBaseError
from ._ptr_owner import _PtrOwner, _str_enc

__all__ = [
    "InteractiveGeoMode", "ConfigClassification", "ScriptLanguage",
    "RandomizedCheckResult", "ConstraintMaintainStatus",
    "InteractiveGeo", "InteractiveGeoError",
    "interactive_geo_init",
]


# ============================================================
# 枚举常量
# ============================================================

class InteractiveGeoMode:
    """交互几何模式枚举。

    常量:
        POINT (0): 点模式——点击画布创建新点
        LINE (1): 线模式——拖拽创建直线
        CIRCLE (2): 圆模式——点击圆心后拖拽确定半径
        SEGMENT (3): 线段模式——点击两端点创建线段
        SELECT (4): 选择模式——点击选中/取消选中对象
        DRAG (5): 拖拽模式——自由拖拽移动对象
        CONSTRUCT (6): 构造模式——通过预设规则创建几何体
        MEASURE (7): 测量模式——点击显示距离/角度/面积
        PROVE (8): 证明模式——选择几何体并启动自动证明
    """
    POINT = 0
    LINE = 1
    CIRCLE = 2
    SEGMENT = 3
    SELECT = 4
    DRAG = 5
    CONSTRUCT = 6
    MEASURE = 7
    PROVE = 8


class ConfigClassification:
    """配置分类枚举（借鉴 Cinderella 连续性跟踪）。

    常量:
        NORMAL (0): 正常配置
        SINGULAR (1): 奇异配置——触发退化条件
        DEGENERATE (2): 退化配置——维度降低
    """
    NORMAL = 0
    SINGULAR = 1
    DEGENERATE = 2


class ScriptLanguage:
    """脚本语言类型枚举。

    常量:
        lv_DSL (0): Lv-00 原生 DSL
        PYTHON (1): Python 脚本
        LUA (2): Lua 脚本
    """
    lv_DSL = 0
    PYTHON = 1
    LUA = 2


class RandomizedCheckResult:
    """随机化验证结果枚举。

    常量:
        PASSED (0): 所有样本通过
        FAILED (1): 至少一个样本失败
        INCONCLUSIVE (2): 无法判定
        PROBABILISTICALLY_TRUE (3): 高概率成立
    """
    PASSED = 0
    FAILED = 1
    INCONCLUSIVE = 2
    PROBABILISTICALLY_TRUE = 3


class ConstraintMaintainStatus:
    """约束维护状态枚举。

    常量:
        OK (0): 约束维护成功
        OVER_CONSTRAINED (1): 过度约束
        UNDER_CONSTRAINED (2): 约束不足
        SINGULAR_AVOIDED (3): 奇异配置已避开
        FAILED (4): 约束维护失败
    """
    OK = 0
    OVER_CONSTRAINED = 1
    UNDER_CONSTRAINED = 2
    SINGULAR_AVOIDED = 3
    FAILED = 4


# ============================================================
# 异常类
# ============================================================

class InteractiveGeoError(lvBaseError):
    """交互几何错误基类。

    所有交互几何相关异常的父类。
    继承 lvBaseError，复用统一的 message、error_code 属性和 __str__ 格式化逻辑。

    属性:
        message: 异常消息字符串
        error_code: 可选的错误码（整数，默认为 -1）
    """
    pass


# ============================================================
# InteractiveGeo 类
# ============================================================

class InteractiveGeo(_PtrOwner):
    """交互几何主上下文类。

    聚合所有交互几何子系统的顶层结构，是交互几何模块的入口。
    包含画布状态、随机化验证、脚本绑定、连续性跟踪和约束保持。

    属性:
        _ptr: 底层 C lvInteractiveGeo 指针
    """

    def __init__(self, engine_handle: Any = None) -> None:
        """初始化交互几何系统。

        参数:
            engine_handle: 关联的 lvEngine 句柄（可为 None 延迟绑定）

        异常:
            InteractiveGeoError: 初始化失败
        """
        eh = engine_handle._ptr if hasattr(engine_handle, '_ptr') else engine_handle
        self._ptr = _lib.interactive_geo_init(eh if eh else None)
        if not self._ptr:
            raise InteractiveGeoError("初始化交互几何系统失败")
        # 登记生命周期管理（析构由 _PtrOwner 统一处理）
        _PtrOwner.__init__(self, self._ptr, _lib.interactive_geo_destroy, True)

    # ---- 模式管理 ----

    def set_mode(self, mode: int) -> None:
        """设置当前交互模式。

        切换工具模式并触发 on_mode_changed 回调。

        参数:
            mode: 目标交互模式（InteractiveGeoMode 枚举值）
        """
        _lib.interactive_geo_set_mode(self._ptr, mode)

    def get_mode(self) -> int:
        """获取当前交互模式。

        返回:
            int: 当前交互模式（InteractiveGeoMode 枚举值）
        """
        return _lib.interactive_geo_get_mode(self._ptr)

    # ---- 选择管理 ----

    def select(self, object_id: int) -> int:
        """选中一个几何对象。

        参数:
            object_id: 要选中的对象 ID

        返回:
            int: 0 成功，-1 无效 ID
        """
        return _lib.interactive_geo_select(self._ptr, object_id)

    def deselect(self, object_id: int = -1) -> None:
        """取消选中几何对象。

        参数:
            object_id: 要取消选中的对象 ID（-1 表示取消全部选中）
        """
        _lib.interactive_geo_deselect(self._ptr, object_id)

    # ---- 拖拽交互 ----

    def drag_start(self, object_id: int, x: float, y: float) -> int:
        """开始拖拽操作。

        参数:
            object_id: 被拖拽的对象 ID（-1 = 画布平移）
            x: 拖拽起始 X 坐标（世界坐标）
            y: 拖拽起始 Y 坐标（世界坐标）

        返回:
            int: 0 成功，-1 对象不存在
        """
        return _lib.interactive_geo_drag_start(self._ptr, object_id, x, y)

    def drag_move(self, x: float, y: float) -> int:
        """拖拽移动。

        更新拖拽位置并触发约束维护。

        参数:
            x: 当前拖拽 X 坐标（世界坐标）
            y: 当前拖拽 Y 坐标（世界坐标）

        返回:
            int: 约束维护状态码（ConstraintMaintainStatus 枚举值）
        """
        return _lib.interactive_geo_drag_move(self._ptr, x, y)

    def drag_end(self, x: float, y: float) -> int:
        """结束拖拽操作。

        参数:
            x: 最终 X 坐标（世界坐标）
            y: 最终 Y 坐标（世界坐标）

        返回:
            int: 约束维护状态码（ConstraintMaintainStatus 枚举值）
        """
        return _lib.interactive_geo_drag_end(self._ptr, x, y)

    # ---- 随机化定理验证 ----

    def randomized_check(self, sample_count: int = 0, tolerance: float = 0.0,
                         theorem_expr: Optional[str] = None) -> Tuple[int, Any]:
        """执行随机化定理验证。

        借鉴 Cinderella 的 Randomized Theorem Checking。

        参数:
            sample_count: 采样次数（0 = 使用默认值 10000）
            tolerance: 数值容差（0 = 使用默认值 1e-9）
            theorem_expr: 定理表达式（Lv-00 DSL 格式，None = 使用当前选中构造）

        返回:
            Tuple[int, Any]: (RandomizedCheckResult 枚举值, 结果详情指针)
        """
        c_expr = _str_enc(theorem_expr) if theorem_expr else None
        result = _lib.interactive_geo_randomized_check(
            self._ptr, sample_count, tolerance, c_expr, None)
        return (result, None)

    # ---- 构造脚本生成 ----

    def generate_script(self, language: int = ScriptLanguage.PYTHON) -> Optional[str]:
        """生成构造脚本。

        借鉴 Dr. Geo 的 Smalltalk 代码生成思想。

        参数:
            language: 目标脚本语言（ScriptLanguage 枚举值）

        返回:
            Optional[str]: 生成的脚本代码字符串，失败返回 None
        """
        output = c_char_p()
        result = _lib.interactive_geo_generate_script(self._ptr, language, ctypes.byref(output))
        if result > 0 and output.value:
            return output.value.decode('utf-8')
        return None

    # ---- 奇异配置检测 ----

    def detect_singularity(self) -> Tuple[bool, int]:
        """检测当前几何配置的奇异性。

        借鉴 Cinderella Continuity 机制。

        返回:
            Tuple[bool, int]: (是否奇异, 配置分类 ConfigClassification 枚举值)
        """
        classification = c_int()
        is_singular = _lib.interactive_geo_detect_singularity(self._ptr, ctypes.byref(classification))
        return (is_singular, classification.value)

    # ---- 约束实时维护 ----

    def maintain_constraints(self, moved_id: int, new_x: float, new_y: float) -> int:
        """维护几何约束（Cinderella 核心功能）。

        当用户拖拽点时，实时计算所有受影响的约束并更新位置。

        参数:
            moved_id: 被移动的对象 ID
            new_x: 新 X 坐标
            new_y: 新 Y 坐标

        返回:
            int: 约束维护状态码（ConstraintMaintainStatus 枚举值）
        """
        return _lib.interactive_geo_maintain_constraints(self._ptr, moved_id, new_x, new_y)

    # ---- 状态导入/导出 ----

    def export_state(self) -> Optional[str]:
        """导出当前交互几何状态为 JSON 字符串。

        返回:
            Optional[str]: JSON 字符串，失败返回 None
        """
        s = _lib.interactive_geo_export_state(self._ptr)
        if s:
            result = ctypes.string_at(s).decode('utf-8')
            _lib.lv_free_ptr(s)
            return result
        return None

    def import_state(self, json_str: str) -> int:
        """从 JSON 字符串导入交互几何状态。

        参数:
            json_str: JSON 状态字符串

        返回:
            int: 0 成功，-1 格式错误，-2 数据不一致
        """
        return _lib.interactive_geo_import_state(self._ptr, _str_enc(json_str))

    # ---- 对象查询 ----

    def get_all_objects(self) -> List[int]:
        """获取所有活跃几何对象 ID 列表。

        返回:
            List[int]: 对象 ID 列表
        """
        count = c_int()
        ids_ptr = _lib.interactive_geo_get_all_objects(self._ptr, ctypes.byref(count))
        if not ids_ptr or count.value == 0:
            return []
        return [ids_ptr[i] for i in range(count.value)]

    # ---- 快照/恢复 ----

    def snapshot(self) -> int:
        """创建当前状态的快照。

        快照存储在内部环形缓冲区（最多 32 个）。

        返回:
            int: 快照索引（0-based），失败返回 -1
        """
        return _lib.interactive_geo_snapshot(self._ptr)

    def restore(self, snapshot_index: int) -> int:
        """恢复到指定快照。

        参数:
            snapshot_index: 快照索引

        返回:
            int: 0 成功，-1 无效索引
        """
        return _lib.interactive_geo_restore(self._ptr, snapshot_index)


# ============================================================
# 便捷函数
# ============================================================

def interactive_geo_init(engine_handle: Any = None) -> InteractiveGeo:
    """初始化交互几何系统（便捷函数）。

    参数:
        engine_handle: 关联的引擎句柄

    返回:
        InteractiveGeo: 交互几何上下文
    """
    return InteractiveGeo(engine_handle)
