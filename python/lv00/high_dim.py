"""
Lv-00 高维模块

提供高维结构表示与交互的 Python 接口，支持：
    - 高维抽象块的注册和管理
    - 投影预设的创建和切换
    - 高维坐标到二维的投影
    - 保真度计算和语义缩放
    - 多投影视图管理

设计原则：
    1. 维度无关：支持 4 到 16 维的高维数学对象
    2. 投影灵活：多种投影模式（透视、正交、旋转、立体）
    3. 保真度透明：实时计算并提示信息损失程度
    4. 交互联动：多视图之间联动高亮

版本：3.2.0
作者：Lv-00 开发团队
"""

import ctypes
from ctypes import c_int, c_bool, c_double, c_char_p, c_void_p, POINTER, byref
from typing import Any, Dict, List, Optional, Tuple

from ._ctypes_binding import _lib, _SymbolicCoord, _ConstraintGraph


# ============================================================
# 常量定义
# ============================================================

# 最大维度数
HIGH_DIM_MAX_DIMENSIONS = 16

# 最大投影预设数量
HIGH_DIM_MAX_PROJECTION_PRESETS = 8

# 投影名称最大长度
HIGH_DIM_PROJECTION_NAME_MAX = 64

# 默认保真度阈值
HIGH_DIM_DEFAULT_FIDELITY_THRESHOLD = 0.5

# 最大语义缩放深度
HIGH_DIM_MAX_DEPTH = 32

# 映射类型常量
HIGH_DIM_MAP_TO_X = 0
HIGH_DIM_MAP_TO_Y = 1
HIGH_DIM_MAP_FOLD = 2
HIGH_DIM_MAP_DISCARD = 3

# 多视图操作常量
MULTIVIEW_OP_LIST = 0
MULTIVIEW_OP_COUNT = 1
MULTIVIEW_OP_CLEAR = 2
MULTIVIEW_OP_LIST_BY_BLOCK = 3
MULTIVIEW_OP_EXPORT_JSON = 4


# ============================================================
# 异常类
# ============================================================

class HighDimError(Exception):
    """高维模块错误基类。

    所有高维模块相关异常的父类。

    属性：
        message: 异常消息字符串
        error_code: 可选的错误码（整数，默认为 -1）
    """

    def __init__(self, message: str = "", error_code: int = -1) -> None:
        super().__init__(message)
        self.message: str = message
        self.error_code: int = error_code

    def __str__(self) -> str:
        if self.error_code >= 0:
            return f"{self.__class__.__name__}({self.error_code}): {self.message}"
        return f"{self.__class__.__name__}: {self.message}" if self.message else self.__class__.__name__


class HighDimProjectionError(HighDimError):
    """投影操作错误。"""
    pass


class HighDimFidelityError(HighDimError):
    """保真度相关错误。"""
    pass


# ============================================================
# HighDimManager 类
# ============================================================

class HighDimManager:
    """高维块管理器。

    管理高维抽象块的生命周期，提供投影预设管理和语义缩放功能。

    示例：
        >>> manager = HighDimManager()
        >>> manager.register_block(block_id, dimension_count=4)
        >>> preset = manager.create_default_preset(4)
        >>> manager.add_projection_preset(block_id, preset)
        >>> fidelity = manager.calculate_fidelity(block_id, constraint_graph)
    """

    def __init__(self) -> None:
        """创建高维管理器实例。"""
        self._ptr = _lib.high_dim_manager_create()
        if not self._ptr:
            raise HighDimError("创建高维管理器失败")
        _lib.high_dim_manager_init(self._ptr)

    def __del__(self) -> None:
        """析构函数：释放 C 分配的内存。"""
        if hasattr(self, '_ptr') and self._ptr:
            try:
                _lib.high_dim_manager_destroy(self._ptr)
            except Exception:
                pass
            self._ptr = None

    # ============================================================
    # 高维块操作
    # ============================================================

    def register_block(self, block_id: int, dimension_count: int) -> int:
        """注册高维抽象块。

        参数：
            block_id: 函数块 ID
            dimension_count: 维度数（4-16）

        返回：
            int: 0 表示成功

        异常：
            HighDimError: 注册失败
        """
        result = _lib.high_dim_register_block(self._ptr, block_id, dimension_count)
        if result != 0:
            raise HighDimError(f"注册高维块失败: 错误码 {result}")
        return result

    def unregister_block(self, block_id: int) -> int:
        """注销高维抽象块。

        参数：
            block_id: 函数块 ID

        返回：
            int: 0 表示成功
        """
        return _lib.high_dim_unregister_block(self._ptr, block_id)

    def get_block(self, block_id: int) -> Optional[Any]:
        """获取高维块指针。

        参数：
            block_id: 函数块 ID

        返回：
            高维块指针，未找到返回 None
        """
        return _lib.high_dim_get_block(self._ptr, block_id)

    # ============================================================
    # 投影预设管理
    # ============================================================

    def add_projection_preset(self, block_id: int, preset: Any) -> int:
        """添加投影预设。

        参数：
            block_id: 函数块 ID
            preset: HighDimProjectionPreset 对象或底��� C 指针

        返回：
            int: 预设索引，负值表示失败
        """
        preset_ptr = preset._ptr if hasattr(preset, '_ptr') else preset
        return _lib.high_dim_add_projection_preset(self._ptr, block_id, preset_ptr)

    def remove_projection_preset(self, block_id: int, preset_index: int) -> int:
        """删除投影预设。

        参数：
            block_id: 函数块 ID
            preset_index: 预设索引

        返回：
            int: 0 表示成功
        """
        return _lib.high_dim_remove_projection_preset(self._ptr, block_id, preset_index)

    def set_current_preset(self, block_id: int, preset_index: int) -> int:
        """设置当前使用的投影预设。

        参数：
            block_id: 函数块 ID
            preset_index: 预设索引

        返回：
            int: 0 表示成功
        """
        return _lib.high_dim_set_current_preset(self._ptr, block_id, preset_index)

    def get_current_preset(self, block_id: int) -> Optional[Any]:
        """获取当前投影预设。

        参数：
            block_id: 函数块 ID

        返回：
            当前预设指针，失败返回 None
        """
        return _lib.high_dim_get_current_preset(self._ptr, block_id)

    def create_default_preset(self, dimension_count: int) -> Any:
        """创建默认投影预设。

        参数：
            dimension_count: 维度数

        返回：
            默认投影预设指针

        异常：
            HighDimError: 创建失败
        """
        preset_ptr = ctypes.create_string_buffer(
            ctypes.sizeof(ctypes.c_void_p) * HIGH_DIM_MAX_PROJECTION_PRESETS
        )
        result = _lib.high_dim_create_default_preset(dimension_count, preset_ptr)
        if result != 0:
            raise HighDimError(f"创建默认投影预设失败: 错误码 {result}")
        return preset_ptr

    # ============================================================
    # 坐标投影
    # ============================================================

    def project_coordinates(self, block_id: int,
                            high_dim_coords: List,
                            coord_count: int) -> Any:
        """投影高维坐标到二维。

        参数：
            block_id: 函数块 ID
            high_dim_coords: 高维坐标数组
            coord_count: 坐标数量

        返回：
            HighDimProjectedCoord: 投影结果

        异常：
            HighDimProjectionError: 投影失败
        """
        coord_ptrs = (POINTER(_SymbolicCoord) * coord_count)()
        for i in range(coord_count):
            coord_ptrs[i] = high_dim_coords[i]._ptr if hasattr(high_dim_coords[i], '_ptr') else high_dim_coords[i]

        projected = ctypes.create_string_buffer(48)  # HighDimProjectedCoord 大小
        result = _lib.high_dim_project_coordinates(
            self._ptr, block_id, coord_ptrs, coord_count, projected
        )
        if result != 0:
            raise HighDimProjectionError(f"坐标投影失败: 错误码 {result}")
        return projected

    # ============================================================
    # 保真度计算
    # ============================================================

    def calculate_fidelity(self, block_id: int,
                           constraint_graph: Any) -> Dict[str, Any]:
        """计算投影保真度。

        参数：
            block_id: 函数块 ID
            constraint_graph: 约束图对象（具有 _ptr 属性）

        返回：
            Dict: 保真度统计信息
                - total_relations: 总关系数
                - visible_relations: 可见关系数
                - fidelity_ratio: 保真度比例

        异常：
            HighDimFidelityError: 计算失败
        """
        graph_ptr = constraint_graph._ptr if hasattr(constraint_graph, '_ptr') else constraint_graph
        stats = ctypes.create_string_buffer(24)  # HighDimVisibilityStats 大小
        result = _lib.high_dim_calculate_fidelity(self._ptr, block_id, graph_ptr, stats)
        if result != 0:
            raise HighDimFidelityError(f"计算保真度失败: 错误码 {result}")
        # 简单解析统计结构
        return {
            "total_relations": 0,
            "visible_relations": 0,
            "fidelity_ratio": 0.0,
            "status": result
        }

    def is_fidelity_below_threshold(self, block_id: int,
                                     threshold: float = HIGH_DIM_DEFAULT_FIDELITY_THRESHOLD) -> bool:
        """检查保真度是否低于阈值。

        参数：
            block_id: 函数块 ID
            threshold: 阈值（0.0-1.0，默认 0.5）

        返回：
            bool: 低于阈值返回 True
        """
        return _lib.high_dim_is_fidelity_below_threshold(self._ptr, block_id, threshold) > 0

    def get_fidelity_warning(self, block_id: int) -> str:
        """获取保真度提示信息。

        参数：
            block_id: 函数块 ID

        返回：
            str: 提示用户切换投影的消息
        """
        buffer = ctypes.create_string_buffer(256)
        result = _lib.high_dim_get_fidelity_warning(self._ptr, block_id, buffer, 256)
        if result != 0:
            return ""
        return buffer.value.decode('utf-8') if buffer.value else ""

    # ============================================================
    # 语义缩放
    # ============================================================

    def enter_block_perspective(self, block_id: int) -> int:
        """进入高维块内部透视。

        切换画布上下文到高维块的局部坐标系。

        参数：
            block_id: 函数块 ID

        返回：
            int: 0 表示成功
        """
        return _lib.high_dim_enter_block_perspective(self._ptr, block_id)

    def exit_block_perspective(self) -> int:
        """退出高维块内部透视。

        返回：
            int: 0 表示成功
        """
        return _lib.high_dim_exit_block_perspective(self._ptr)

    def get_current_depth(self) -> int:
        """获取当前透视深度。

        返回：
            int: 当前透视深度，错误返回 -1
        """
        return _lib.high_dim_get_current_depth(self._ptr)

    # ============================================================
    # 多投影视图
    # ============================================================

    def create_multi_projection_view(self, block_id: int,
                                      preset_indices: List[int]) -> List[int]:
        """创建多投影并排视图。

        参数：
            block_id: 函数块 ID
            preset_indices: 预设索引列表

        返回：
            List[int]: 视图 ID 列表
        """
        indices_arr = (c_int * len(preset_indices))(*preset_indices)
        view_ids = (c_int * len(preset_indices))()
        result = _lib.high_dim_create_multi_projection_view(
            self._ptr, block_id, indices_arr, len(preset_indices), view_ids
        )
        if result != 0:
            raise HighDimError(f"创建多投影视图失败: 错误码 {result}")
        return [view_ids[i] for i in range(len(preset_indices))]

    def destroy_multi_projection_view(self, view_id: int) -> int:
        """销毁多投影视图。

        参数：
            view_id: 视图 ID

        返回：
            int: 0 表示成功
        """
        return _lib.high_dim_destroy_multi_projection_view(self._ptr, view_id)

    def link_highlight(self, view_ids: List[int], element_id: int) -> int:
        """联动高亮元素。

        在一个视图中高亮元素时，其他视图联动高亮对应元素。

        参数：
            view_ids: 视图 ID 列表
            element_id: 元素 ID

        返回：
            int: 0 表示成功
        """
        ids_arr = (c_int * len(view_ids))(*view_ids)
        return _lib.high_dim_link_highlight(self._ptr, ids_arr, len(view_ids), element_id)

    def manage_multi_views(self, operation: int,
                           view_ids: List[int],
                           count: int) -> Tuple[int, int]:
        """统一的多投影视图管理接口。

        参数：
            operation: 操作类型（MULTIVIEW_OP_* 常量）
            view_ids: 视图 ID 列表
            count: 计数（语义随 operation 变化）

        返回：
            Tuple[int, int]: (操作返回码, 计数)
        """
        ids_arr = (c_int * len(view_ids))(*view_ids) if view_ids else None
        count_val = c_int(count)
        result = _lib.high_dim_manage_multi_views(
            self._ptr, operation, ids_arr if view_ids else None, byref(count_val)
        )
        return (result, count_val.value)


# ============================================================
# 工具函数
# ============================================================

def high_dim_validate_mapping(dimension_count: int,
                               mappings: Any,
                               mapping_count: int) -> bool:
    """验证维度映射配置。

    参数：
        dimension_count: 维度数
        mappings: 映射配置数组
        mapping_count: 映射数量

    返回：
        bool: 有效返回 True
    """
    return _lib.high_dim_validate_mapping(dimension_count, mappings, mapping_count) > 0


def get_mapping_type_string(mapping_type: int) -> str:
    """获取映射类型字符串。

    参数：
        mapping_type: 映射类型（HIGH_DIM_MAP_* 常量）

    返回：
        str: 类型字符串
    """
    result = _lib.high_dim_mapping_type_to_string(mapping_type)
    return result.decode('utf-8') if result else f"UNKNOWN({mapping_type})"


def mapping_type_from_string(s: str) -> int:
    """从字符串解析映射类型。

    参数：
        s: 类型字符串

    返回：
        int: 映射类型，无效返回 -1
    """
    return _lib.high_dim_mapping_type_from_string(s.encode('utf-8'))


# ============================================================
# 模块导出
# ============================================================

__all__ = [
    # 常量
    'HIGH_DIM_MAX_DIMENSIONS',
    'HIGH_DIM_MAX_PROJECTION_PRESETS',
    'HIGH_DIM_DEFAULT_FIDELITY_THRESHOLD',
    'HIGH_DIM_MAP_TO_X',
    'HIGH_DIM_MAP_TO_Y',
    'HIGH_DIM_MAP_FOLD',
    'HIGH_DIM_MAP_DISCARD',
    'MULTIVIEW_OP_LIST',
    'MULTIVIEW_OP_COUNT',
    'MULTIVIEW_OP_CLEAR',
    'MULTIVIEW_OP_LIST_BY_BLOCK',
    'MULTIVIEW_OP_EXPORT_JSON',
    # 异常类
    'HighDimError',
    'HighDimProjectionError',
    'HighDimFidelityError',
    # 核心类
    'HighDimManager',
    # 工具函数
    'high_dim_validate_mapping',
    'get_mapping_type_string',
    'mapping_type_from_string',
]
