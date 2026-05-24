"""
Lv-00 归一化结果重导出模块

此文件为向后兼容而保留。NormalizationResult 类的真正定义位于
lv00.core 模块中。

背景：
    NormalizationResult 类原本在此文件中定义，但在 v3.2.0 中已合并到
    lv00.core 以避免与 core.py 中的类定义冲突。此模块现在仅作为薄层
    从 core.py 重新导出 NormalizationResult，以确保现有导入路径
    (from lv00.normalization import NormalizationResult) 继续工作。

新代码建议：
    新代码应直接从 lv00.core 导入：
        from lv00.core import NormalizationResult

版本：3.2.0
作者：Lv-00 开发团队
"""

from __future__ import annotations
from typing import Any, Callable, Dict, List, Optional, Set, Tuple, Union

from .core import NormalizationResult

# 重新导出以保持向后兼容
__all__ = ['NormalizationResult']


# ============================================================
# 类型别名
# ============================================================

#: 合并候选映射类型：规范化前的节点 ID -> 规范化后的代表节点 ID
MergeMapping = Dict[int, int]

#: 合并确认回调类型：接收 (源节点ID, 目标节点ID) -> 返回是否确认合并
MergeConfirmCallback = Callable[[int, int], bool]

#: 归一化选项字典类型
NormalizationOptions = Dict[str, Union[bool, int, MergeConfirmCallback]]


# ============================================================
# 类型化工具函数
# ============================================================

def normalize_merge_count(result: NormalizationResult) -> int:
    """
    获取归一化结果中的合并节点数量。

    参数：
        result: NormalizationResult 实例

    返回：
        int: 被合并的节点数量，结果无效时返回 0
    """
    if result is None:
        return 0
    try:
        return result.merged_count
    except AttributeError:
        return 0


def normalize_original_ids(result: NormalizationResult) -> List[int]:
    """
    提取归一化结果中被合并的原始节点 ID 列表。

    参数：
        result: NormalizationResult 实例

    返回：
        List[int]: 被合并的原始节点 ID 列表
    """
    if result is None:
        return []
    try:
        return list(result.original_ids) if result.original_ids else []
    except AttributeError:
        return []


def normalize_representative_ids(result: NormalizationResult) -> List[int]:
    """
    提取归一化结果中的代表节点 ID 列表。

    参数：
        result: NormalizationResult 实例

    返回：
        List[int]: 代表节点 ID 列表
    """
    if result is None:
        return []
    try:
        return list(result.representative_ids) if result.representative_ids else []
    except AttributeError:
        return []


def normalize_build_mapping(result: NormalizationResult) -> MergeMapping:
    """
    从归一化结果构建合并映射表。

    参数：
        result: NormalizationResult 实例

    返回：
        MergeMapping: 原始节点 ID 到代表节点 ID 的映射字典
    """
    mapping: MergeMapping = {}
    if result is None:
        return mapping
    try:
        orig = result.original_ids
        rep = result.representative_ids
        if orig and rep:
            for o, r in zip(orig, rep):
                mapping[o] = r
    except AttributeError:
        pass
    return mapping


def normalize_is_user_confirmed(result: NormalizationResult) -> bool:
    """
    检查归一化是否经过用户确认。

    参数：
        result: NormalizationResult 实例

    返回：
        bool: 用户已确认返回 True
    """
    if result is None:
        return False
    try:
        return bool(result.user_confirmed)
    except AttributeError:
        return False


def normalize_summary(result: NormalizationResult) -> Dict[str, Any]:
    """
    生成归一化结果的摘要信息。

    参数：
        result: NormalizationResult 实例

    返回：
        Dict[str, Any]: 包含合并数、用户确认状态等信息的字典
    """
    return {
        "merged_count": normalize_merge_count(result),
        "user_confirmed": normalize_is_user_confirmed(result),
        "original_ids": normalize_original_ids(result),
        "representative_ids": normalize_representative_ids(result),
    }
