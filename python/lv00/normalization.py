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

from .core import NormalizationResult

# 重新导出以保持向后兼容
__all__ = ['NormalizationResult']
