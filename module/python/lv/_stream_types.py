#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Lv-00 共享类型定义模块

包含 ws_server.py 和 stream_bridge.py 双方引用的公共类型和标志，
用于解除这两个模块之间的循环导入问题。

此模块不依赖 ws_server.py 或 stream_bridge.py 中的任何内容。
"""

import logging
from ctypes import c_int, c_long, c_double, c_char_p, c_void_p, POINTER, Structure

logger = logging.getLogger('stream_bridge')

# ================================================================
# WebSocket 依赖检查
# ================================================================

try:
    import websockets  # noqa: F401
    WEBSOCKETS_AVAILABLE = True
except ImportError:
    WEBSOCKETS_AVAILABLE = False
    logger.warning(
        "websockets 库未安装，WebSocket 功能不可用。"
        "安装: pip install websockets"
    )


# ================================================================
# C 引擎 StreamEvent 结构体
# ================================================================

class StreamEvent(Structure):
    """
    C 引擎 StreamEvent 结构体的 Python 映射。

    该结构体用于接收底层 C 引擎发送的流式事件数据，
    包括事件类型、时间戳、节点/约束/规则 ID、描述文本等信息。

    属性：
        type: 事件类型编码
        timestamp_ms: 事件时间戳（毫秒）
        step_number: 当前步骤编号
        total_steps: 总步骤数
        node_id: 相关节点 ID
        constraint_id: 相关约束 ID
        rule_id: 相关规则 ID
        var_id: 相关变量 ID
        merge_pairs: 合并对指针（StreamMergePair*）
        merge_count: 合并对数量
        description: 事件描述文本
        detail_json: 详细信息（JSON 格式）
        progress: 进度百分比
        numeric_value: 数值结果
        graph_json: 图数据（JSON 格式）
    """

    _fields_ = [
        ('type', c_int),
        # K66/F96 修复：timestamp_ms 为 C 侧 int64_t（8B）——原 c_long 在
        # Windows（LLP64）下是 4B，导致结构错位（其后字段全部偏移错误）；
        # Linux LP64 下 c_long 恰为 8B 故未暴露（设计 L756 判定）。
        ('timestamp_ms', c_int64),
        ('step_number', c_int),
        ('total_steps', c_int),
        ('node_id', c_int),
        ('constraint_id', c_int),
        ('rule_id', c_int),
        ('var_id', c_int),
        ('merge_pairs', c_void_p),  # StreamMergePair*
        ('merge_count', c_int),
        ('description', c_char_p),
        ('detail_json', c_char_p),
        ('progress', c_double),
        ('numeric_value', c_double),
        ('graph_json', c_char_p),
    ]
