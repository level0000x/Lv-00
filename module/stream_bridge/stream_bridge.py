#!/usr/bin/env python3
"""
Lv-00 Stream Bridge - C 引擎流式事件到 JSON Lines 的桥接服务器模块
========================================================================

本模块是 Lv-00 几何引擎与前端/CLI 之间的流式事件桥接层。
将 C 引擎产生的流式事件转换为 JSON Lines 或 SSE 格式，供外部消费。

运行模式：
  1. stdio  : stdin JSON-RPC 命令 → stdout JSON Lines 事件流（子进程桥接）
  2. sse    : HTTP SSE (Server-Sent Events) 端点（浏览器直连）
  3. demo   : 演示模式，模拟引擎运行全流程事件流

架构：
  [C 引擎 stream 模块] → (ctypes 回调) → [Stream Bridge] → JSON Lines/SSE → [前端/CLI]

当 C 共享库不可用时，自动退化为纯 Python 模拟实现（demo 模式），
保证前端开发不依赖 C 编译环境。

核心组件：
  - StreamEventType: 事件类型枚举（与 C 头文件 stream.h 一致）
  - StreamEvent: 事件数据结构（与 C StreamEvent 结构体对应）
  - StreamContext: 流式上下文（模拟 C StreamContext API）
  - DemoEngine: 演示引擎（模拟完整求解流程）
  - JsonLineWriter: JSON Lines 序列化输出器

用法：
  python stream_bridge.py stdio          # stdin/stdout 模式
  python stream_bridge.py sse --port 5801  # SSE 服务器模式
  python stream_bridge.py demo --scenario triangle  # 演示模式

@author Lv-00 Project
@version 3.2.0
"""

import sys
import json
import time
import argparse
import threading
import queue
import logging
from typing import Optional, Callable, Dict, Any, List
from dataclasses import dataclass, field, asdict
from enum import IntEnum
import ctypes
import ctypes.util
import os

# ================================================================
# Logging Configuration (日志配置)
# ================================================================

logger = logging.getLogger("stream-bridge")
logger.setLevel(logging.DEBUG)
_handler = logging.StreamHandler(sys.stderr)
_handler.setFormatter(logging.Formatter("[%(name)s] %(levelname)s: %(message)s"))
logger.addHandler(_handler)

# ================================================================
# Event Types (mirror C stream.h StreamEventType enum)
# ================================================================

class StreamEventType(IntEnum):
    """30+ 种引擎流式事件类型，与 C 头文件 stream.h 完全一致"""
    # ---- 引擎生命周期 ----
    ENGINE_START        = 0
    ENGINE_DONE         = 1
    ENGINE_PAUSED       = 2
    # ---- 归一化 ----
    NORMALIZE_START     = 3
    NORMALIZE_MERGE     = 4
    NORMALIZE_DONE      = 5
    # ---- 重写引擎 ----
    REWRITE_START       = 6
    REWRITE_RULE_LOADED = 7
    REWRITE_MATCH_FOUND = 8
    REWRITE_APPLIED     = 9
    REWRITE_ROLLBACK    = 10
    REWRITE_DONE        = 11
    # ---- 代数求解 ----
    SOLVE_START             = 12
    SOLVE_EQUATION_EXTRACTED = 13
    SOLVE_GROEBNER_STEP     = 14
    SOLVE_VARIABLE_RESOLVED = 15
    SOLVE_DONE              = 16
    # ---- 证明系统 ----
    PROOF_STEP_ADDED      = 17
    PROOF_STEP_APPLIED    = 18
    PROOF_UNIFY           = 19
    PROOF_COLOR_UPDATE    = 20
    PROOF_DEPENDENCY_CHANGE = 21
    # ---- 函数块系统 ----
    FUNC_BLOCK_PACK_START        = 22
    FUNC_BLOCK_PACK_DONE         = 23
    FUNC_BLOCK_INSTANTIATE_START = 24
    FUNC_BLOCK_INSTANTIATE_DONE  = 25
    FUNC_BLOCK_PARTIAL_APPLY     = 26
    FUNC_BLOCK_DETERMINISM_CHECK = 27
    FUNC_BLOCK_CAPTURE_AVOID     = 28
    FUNC_BLOCK_CROSS_BOUNDARY    = 29
    # ---- 冲突与错误 ----
    CONFLICT_DETECTED  = 30
    CONSTRAINT_ADDED   = 31
    NODE_ADDED         = 32
    CIRCUIT_TRIP       = 33
    ERROR              = 34
    WARNING            = 35
    # ---- 信息 ----
    INFO        = 36
    PROGRESS    = 37
    GRAPH_SNAPSHOT = 38

    # 总数
    EVENT_COUNT = 40


# ================================================================
# Event Metadata (mirror C stream.c name/color/id tables)
# ================================================================

_EVENT_META: Dict[StreamEventType, Dict[str, str]] = {
    StreamEventType.ENGINE_START:          {"id": "ENGINE_START",          "name": "引擎启动",   "color": "#3fb950", "category": "engine"},
    StreamEventType.ENGINE_DONE:           {"id": "ENGINE_DONE",           "name": "引擎完成",   "color": "#3fb950", "category": "engine"},
    StreamEventType.ENGINE_PAUSED:         {"id": "ENGINE_PAUSED",         "name": "引擎暂停",   "color": "#8b949e", "category": "engine"},
    StreamEventType.NORMALIZE_START:       {"id": "NORMALIZE_START",       "name": "归一化开始", "color": "#a371f7", "category": "normalize"},
    StreamEventType.NORMALIZE_MERGE:       {"id": "NORMALIZE_MERGE",       "name": "节点合并",   "color": "#a371f7", "category": "normalize"},
    StreamEventType.NORMALIZE_DONE:        {"id": "NORMALIZE_DONE",        "name": "归一化完成", "color": "#3fb950", "category": "normalize"},
    StreamEventType.REWRITE_START:         {"id": "REWRITE_START",         "name": "重写开始",   "color": "#a371f7", "category": "rewrite"},
    StreamEventType.REWRITE_RULE_LOADED:   {"id": "REWRITE_RULE_LOADED",   "name": "规则加载",   "color": "#a371f7", "category": "rewrite"},
    StreamEventType.REWRITE_MATCH_FOUND:   {"id": "REWRITE_MATCH_FOUND",   "name": "匹配找到",   "color": "#a371f7", "category": "rewrite"},
    StreamEventType.REWRITE_APPLIED:       {"id": "REWRITE_APPLIED",       "name": "规则应用",   "color": "#a371f7", "category": "rewrite"},
    StreamEventType.REWRITE_ROLLBACK:      {"id": "REWRITE_ROLLBACK",      "name": "规则回滚",   "color": "#d29922", "category": "rewrite"},
    StreamEventType.REWRITE_DONE:          {"id": "REWRITE_DONE",          "name": "重写完成",   "color": "#3fb950", "category": "rewrite"},
    StreamEventType.SOLVE_START:           {"id": "SOLVE_START",           "name": "求解开始",   "color": "#a371f7", "category": "solve"},
    StreamEventType.SOLVE_EQUATION_EXTRACTED: {"id": "SOLVE_EQUATION_EXTRACTED", "name": "方程提取", "color": "#58a6ff", "category": "solve"},
    StreamEventType.SOLVE_GROEBNER_STEP:   {"id": "SOLVE_GROEBNER_STEP",   "name": "Groebner基步骤", "color": "#58a6ff", "category": "solve"},
    StreamEventType.SOLVE_VARIABLE_RESOLVED: {"id": "SOLVE_VARIABLE_RESOLVED", "name": "变量解得", "color": "#a371f7", "category": "solve"},
    StreamEventType.SOLVE_DONE:            {"id": "SOLVE_DONE",            "name": "求解完成",   "color": "#3fb950", "category": "solve"},
    StreamEventType.PROOF_STEP_ADDED:      {"id": "PROOF_STEP_ADDED",      "name": "证明步骤添加", "color": "#a371f7", "category": "proof"},
    StreamEventType.PROOF_STEP_APPLIED:    {"id": "PROOF_STEP_APPLIED",    "name": "证明步骤应用", "color": "#a371f7", "category": "proof"},
    StreamEventType.PROOF_UNIFY:           {"id": "PROOF_UNIFY",           "name": "合一检查",   "color": "#58a6ff", "category": "proof"},
    StreamEventType.PROOF_COLOR_UPDATE:    {"id": "PROOF_COLOR_UPDATE",    "name": "颜色更新",   "color": "#d29922", "category": "proof"},
    StreamEventType.PROOF_DEPENDENCY_CHANGE: {"id": "PROOF_DEPENDENCY_CHANGE", "name": "依赖链变化", "color": "#d29922", "category": "proof"},
    StreamEventType.FUNC_BLOCK_PACK_START:       {"id": "FUNC_BLOCK_PACK_START",       "name": "函数打包开始", "color": "#39d353", "category": "func_block"},
    StreamEventType.FUNC_BLOCK_PACK_DONE:        {"id": "FUNC_BLOCK_PACK_DONE",        "name": "函数打包完成", "color": "#3fb950", "category": "func_block"},
    StreamEventType.FUNC_BLOCK_INSTANTIATE_START: {"id": "FUNC_BLOCK_INSTANTIATE_START", "name": "函数实例化开始", "color": "#39d353", "category": "func_block"},
    StreamEventType.FUNC_BLOCK_INSTANTIATE_DONE:  {"id": "FUNC_BLOCK_INSTANTIATE_DONE",  "name": "函数实例化完成", "color": "#3fb950", "category": "func_block"},
    StreamEventType.FUNC_BLOCK_PARTIAL_APPLY:    {"id": "FUNC_BLOCK_PARTIAL_APPLY",    "name": "部分应用",   "color": "#56d4dd", "category": "func_block"},
    StreamEventType.FUNC_BLOCK_DETERMINISM_CHECK: {"id": "FUNC_BLOCK_DETERMINISM_CHECK", "name": "确定性检查", "color": "#56d4dd", "category": "func_block"},
    StreamEventType.FUNC_BLOCK_CAPTURE_AVOID:     {"id": "FUNC_BLOCK_CAPTURE_AVOID",     "name": "捕获避免",  "color": "#56d4dd", "category": "func_block"},
    StreamEventType.FUNC_BLOCK_CROSS_BOUNDARY:    {"id": "FUNC_BLOCK_CROSS_BOUNDARY",    "name": "跨边界操作", "color": "#f778ba", "category": "func_block"},
    StreamEventType.CONFLICT_DETECTED:  {"id": "CONFLICT_DETECTED",  "name": "冲突检测",   "color": "#f85149", "category": "conflict"},
    StreamEventType.CONSTRAINT_ADDED:   {"id": "CONSTRAINT_ADDED",   "name": "约束添加",   "color": "#58a6ff", "category": "conflict"},
    StreamEventType.NODE_ADDED:         {"id": "NODE_ADDED",         "name": "节点添加",   "color": "#58a6ff", "category": "conflict"},
    StreamEventType.CIRCUIT_TRIP:       {"id": "CIRCUIT_TRIP",       "name": "位数熔断",   "color": "#f0883e", "category": "conflict"},
    StreamEventType.ERROR:              {"id": "ERROR",              "name": "错误",       "color": "#f85149", "category": "info"},
    StreamEventType.WARNING:            {"id": "WARNING",            "name": "警告",       "color": "#d29922", "category": "info"},
    StreamEventType.INFO:               {"id": "INFO",               "name": "信息",       "color": "#8b949e", "category": "info"},
    StreamEventType.PROGRESS:           {"id": "PROGRESS",           "name": "进度",       "color": "#58a6ff", "category": "info"},
    StreamEventType.GRAPH_SNAPSHOT:     {"id": "GRAPH_SNAPSHOT",     "name": "图快照",     "color": "#c9d1d9", "category": "info"},
}

# 未知事件回退
_UNKNOWN_META = {"id": "UNKNOWN_EVENT", "name": "未知事件", "color": "#c9d1d9", "category": "unknown"}


# ================================================================
# StreamEvent data class (mirrors C StreamEvent struct)
# ================================================================

@dataclass
class StreamEvent:
    """流式事件数据结构 —— 与 C 引擎 StreamEvent 结构体字段一一对应"""
    type: int
    timestamp_ms: int = 0
    step_number: int = -1
    total_steps: int = -1
    node_id: int = -1
    constraint_id: int = -1
    rule_id: int = -1
    var_id: int = -1
    description: str = ""
    detail_json: str = ""
    progress: float = -1.0
    numeric_value: float = 0.0
    graph_json: str = ""

    def __post_init__(self):
        """创建后验证事件类型是否合法"""
        try:
            StreamEventType(self.type)
        except ValueError:
            logger.warning("StreamEvent 创建时使用了未知的事件类型: %d", self.type)

    def to_json_dict(self) -> Dict[str, Any]:
        """序列化为与 C 引擎 stream_event_to_json() 兼容的 JSON 字典"""
        etype = StreamEventType(self.type)
        meta = _EVENT_META.get(etype, _UNKNOWN_META)
        return {
            "type": meta["id"],
            "type_name": meta["name"],
            "color": meta["color"],
            "category": meta["category"],
            "timestamp_ms": self.timestamp_ms,
            "step": self.step_number,
            "total_steps": self.total_steps,
            "node_id": self.node_id,
            "constraint_id": self.constraint_id,
            "rule_id": self.rule_id,
            "var_id": self.var_id,
            "description": self.description,
            "detail": self.detail_json if self.detail_json else None,
            "progress": self.progress,
            "numeric_value": self.numeric_value,
            "graph_snapshot": self.graph_json if self.graph_json else None,
        }

    def to_jsonrpc(self) -> Dict[str, Any]:
        """包装为 JSON-RPC 2.0 notification 格式"""
        return {
            "jsonrpc": "2.0",
            "method": "stream.event",
            "params": self.to_json_dict(),
        }


# ================================================================
# Stream Context (mirrors C stream.h StreamContext API)
# ================================================================

@dataclass
class StreamContext:
    """流式上下文 —— Python 端模拟 C StreamContext 的完整 API"""
    callbacks: List[Callable] = field(default_factory=list)
    emit_mode: str = "immediate"  # "immediate" | "buffered" | "throttled"
    buffer: queue.Queue = field(default_factory=queue.Queue)
    event_counts: Dict[int, int] = field(default_factory=dict)
    total_count: int = 0
    dropped_count: int = 0

    def register_callback(self, callback: Callable) -> bool:
        """注册流式事件回调（无过滤）"""
        self.callbacks.append(callback)
        return True

    def unregister_callback(self, callback: Callable) -> bool:
        """注销流式事件回调"""
        try:
            self.callbacks.remove(callback)
            return True
        except ValueError:
            return False

    def emit(self, event: StreamEvent):
        """发射流式事件 —— 立即模式直接分发，缓冲模式入队"""
        if not event.timestamp_ms:
            event.timestamp_ms = int(time.time() * 1000)

        # 统计
        self.event_counts[event.type] = self.event_counts.get(event.type, 0) + 1
        self.total_count += 1

        if self.emit_mode == "buffered":
            self.buffer.put(event)
        else:
            # immediate: 直接分发
            for cb in self.callbacks:
                try:
                    cb(event)
                except Exception as e:
                    # 回调异常不传播到调用方，但记录日志以便排查
                    logger.exception("emit 回调执行异常: %s", e)

    def emit_simple(self, event_type: StreamEventType, description: str, step: int = -1):
        """便捷发射 —— 仅类型 + 描述"""
        self.emit(StreamEvent(
            type=int(event_type),
            step_number=step,
            description=description,
        ))

    def flush(self):
        """刷新缓冲区 —— 分发所有待处理事件"""
        while not self.buffer.empty():
            event = self.buffer.get_nowait()
            for cb in self.callbacks:
                try:
                    cb(event)
                except Exception as e:
                    # 回调异常不传播到调用方，但记录日志以便排查
                    logger.exception("flush 回调执行异常: %s", e)

    def reset_stats(self):
        """重置事件统计"""
        self.event_counts.clear()
        self.total_count = 0
        self.dropped_count = 0

    def get_event_count(self, event_type: StreamEventType) -> int:
        """获取指定事件类型的发射次数"""
        return self.event_counts.get(int(event_type), 0)


# ================================================================
# CTypes Stream Bridge (真实 C 库集成)
# ================================================================

# C StreamEvent 结构体布局（与 stream.h 中 StreamEvent 一致）
# 提取为模块级常量，避免嵌套类引用问题
_STREAM_EVENT_FIELDS = [
    ("type", ctypes.c_int),           # StreamEventType (enum = int)
    ("timestamp_ms", ctypes.c_long),  # long
    ("step_number", ctypes.c_int),    # int
    ("total_steps", ctypes.c_int),    # int
    ("node_id", ctypes.c_int),        # int
    ("constraint_id", ctypes.c_int),  # int
    ("rule_id", ctypes.c_int),        # int
    ("var_id", ctypes.c_int),         # int
    # merge_pairs: pointer (8 bytes on 64-bit) - skip for simplicity
    ("_merge_pad", ctypes.c_void_p),
    ("merge_count", ctypes.c_int),    # int
    # description: const char* (pointer)
    ("description", ctypes.c_char_p),
    # detail_json: const char* (pointer)
    ("detail_json", ctypes.c_char_p),
    ("progress", ctypes.c_double),    # double
    ("numeric_value", ctypes.c_double),  # double
    # graph_json: const char* (pointer)
    ("graph_json", ctypes.c_char_p),
]


class StreamEventCTypes(ctypes.Structure):
    """C StreamEvent 结构体的 ctypes 映射"""
    _fields_ = _STREAM_EVENT_FIELDS


class CTypesStreamBridge:
    """
    通过 ctypes 加载 Lv-00 C 共享库，实现真实的流式事件桥接。
    
    当 C 共享库可用时，使用此类替代纯 Python 的 StreamContext + DemoEngine，
    直接从 C 引擎获取事件流。
    
    支持的平台库文件名：
      - Windows: lv00.dll
      - Linux: liblv00.so
      - macOS: liblv00.dylib
    """
    
    def __init__(self):
        self.lib = None
        self.ctx_ptr = None
        self._loaded = False
        self._load_error = None
        self._event_callback_type = None
        self._event_callback_fn = None
        self._event_callback_wrapper = None
        # 【修复 #2】持久化缓冲区列表，防止 C 侧使用期间被 Python GC 回收
        # 每次 emit_event 调用时创建的 ctypes 缓冲区会追加到此列表，
        # 确保在 C 库完成对 StreamEvent 结构体的处理之前，底层数据不会被释放。
        # 注意：此列表会持续增长，适合事件流场景；如果需要精确释放，
        # 可以在 stream_emit() 返回后手动清理旧条目。
        self._persistent_buffers: list = []
    
    def _find_library(self):
        """在常见路径中搜索 Lv-00 共享库"""
        # 候选库文件名（按平台）
        if os.name == 'nt':
            # Windows
            candidates = ['lv00.dll', 'liblv00.dll']
            search_paths = [
                os.path.dirname(os.path.dirname(os.path.abspath(__file__))),  # 项目根目录
                os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'build'),
                os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'build_mingw'),
            ]
        elif sys.platform == 'darwin':
            candidates = ['liblv00.dylib', 'liblv00.so']
            search_paths = [
                os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'build'),
            ]
        else:
            # Linux
            candidates = ['liblv00.so', 'liblv00.so.0']
            search_paths = [
                os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'build'),
            ]
        
        for search_dir in search_paths:
            for lib_name in candidates:
                lib_path = os.path.join(search_dir, lib_name)
                if os.path.isfile(lib_path):
                    return lib_path
        
        return None
    
    def load(self):
        """加载 C 共享库并初始化流式上下文"""
        if self._loaded:
            return True
        
        lib_path = self._find_library()
        if not lib_path:
            self._load_error = "C shared library not found. Searched common build directories."
            return False
        
        try:
            self.lib = ctypes.CDLL(lib_path)
        except OSError as e:
            self._load_error = f"Failed to load C library '{lib_path}': {e}"
            return False
        
        try:
            # 设置函数签名
            # StreamContext *stream_context_create(void);
            self.lib.stream_context_create.restype = ctypes.c_void_p
            self.lib.stream_context_create.argtypes = []
            
            # void stream_context_destroy(StreamContext *ctx);
            self.lib.stream_context_destroy.restype = None
            self.lib.stream_context_destroy.argtypes = [ctypes.c_void_p]
            
            # void stream_emit(StreamContext *ctx, const StreamEvent *event);
            self.lib.stream_emit.restype = None
            self.lib.stream_emit.argtypes = [ctypes.c_void_p, ctypes.POINTER(StreamEventCTypes)]
            
            # int stream_register_callback_ex(StreamContext *ctx, StreamCallback callback, void *user_data, uint64_t filter_mask);
            self.lib.stream_register_callback_ex.restype = ctypes.c_int
            self.lib.stream_register_callback_ex.argtypes = [
                ctypes.c_void_p,
                ctypes.CFUNCTYPE(None, ctypes.POINTER(StreamEventCTypes), ctypes.c_void_p),
                ctypes.c_void_p,
                ctypes.c_uint64,
            ]
            
            # void stream_flush(StreamContext *ctx);
            self.lib.stream_flush.restype = None
            self.lib.stream_flush.argtypes = [ctypes.c_void_p]
            
            # long stream_get_total_event_count(StreamContext *ctx);
            self.lib.stream_get_total_event_count.restype = ctypes.c_long
            self.lib.stream_get_total_event_count.argtypes = [ctypes.c_void_p]
            
            # long stream_get_dropped_count(StreamContext *ctx);
            self.lib.stream_get_dropped_count.restype = ctypes.c_long
            self.lib.stream_get_dropped_count.argtypes = [ctypes.c_void_p]
            
            # const char *stream_event_type_name(int type);
            self.lib.stream_event_type_name.restype = ctypes.c_char_p
            self.lib.stream_event_type_name.argtypes = [ctypes.c_int]
            
            # const char *stream_event_color(int type);
            self.lib.stream_event_color.restype = ctypes.c_char_p
            self.lib.stream_event_color.argtypes = [ctypes.c_int]
            
            # 创建流式上下文
            self.ctx_ptr = self.lib.stream_context_create()
            if not self.ctx_ptr:
                self._load_error = "stream_context_create() returned NULL"
                return False
            
            self._loaded = True
            return True
            
        except (AttributeError, OSError) as e:
            self._load_error = f"Failed to set up C library functions: {e}"
            return False
    
    def register_callback(self, callback):
        """注册 Python 回调，将 C 事件转换为 Python StreamEvent 对象"""
        if not self._loaded:
            return -1
        
        # 保存 Python 回调引用（防止垃圾回收）
        self._event_callback_fn = callback
        
        # 定义 ctypes 回调类型
        self._event_callback_type = ctypes.CFUNCTYPE(
            None,
            ctypes.POINTER(StreamEventCTypes),
            ctypes.c_void_p
        )
        
        def c_callback(c_event_ptr, user_data):
            """C 回调 → Python StreamEvent 转换"""
            if not c_event_ptr:
                return
            c_ev = c_event_ptr.contents
            
            # 转换为 Python StreamEvent
            py_event = StreamEvent(
                type=c_ev.type,
                timestamp_ms=c_ev.timestamp_ms,
                step_number=c_ev.step_number,
                total_steps=c_ev.total_steps,
                node_id=c_ev.node_id,
                constraint_id=c_ev.constraint_id,
                rule_id=c_ev.rule_id,
                var_id=c_ev.var_id,
                description=c_ev.description.decode('utf-8', errors='replace') if c_ev.description else "",
                detail_json=c_ev.detail_json.decode('utf-8', errors='replace') if c_ev.detail_json else "",
                progress=c_ev.progress,
                numeric_value=c_ev.numeric_value,
                graph_json=c_ev.graph_json.decode('utf-8', errors='replace') if c_ev.graph_json else "",
            )
            
            try:
                callback(py_event)
            except Exception as e:
                # 回调异常不传播到 C，但记录日志以便排查
                logger.exception("C 回调 → Python 回调执行异常: %s", e)
        
        # 创建持久的 ctypes 回调包装器
        self._event_callback_wrapper = self._event_callback_type(c_callback)
        
        # 注册到 C 库（STREAM_FILTER_ALL = 0xFFFFFFFFFFFFFFFF）
        cb_id = self.lib.stream_register_callback_ex(
            self.ctx_ptr,
            self._event_callback_wrapper,
            None,
            0xFFFFFFFFFFFFFFFF,
        )
        return cb_id
    
    def emit_event(self, event_type, description="", step_number=0, **kwargs):
        """通过 C 库发射事件（用于从 Python 端模拟引擎事件）

        【修复 #2】使用 ctypes.create_string_buffer 创建持久的 C 字符缓冲区，
        并将引用保存到 self._persistent_buffers 列表中，防止在 C 侧使用期间
        被 Python 垃圾回收器回收。直接赋值 bytes 给 c_char_p 字段时，
        ctypes 仅复制指针值而不持有对原始 bytes 对象的引用，
        一旦 Python 侧 bytes 对象被 GC 回收，C 侧将访问已释放的内存。
        """
        if not self._loaded:
            return

        c_event = StreamEventCTypes()
        c_event.type = int(event_type)
        c_event.timestamp_ms = int(time.time() * 1000)
        c_event.step_number = step_number
        c_event.total_steps = kwargs.get('total_steps', -1)
        c_event.node_id = kwargs.get('node_id', -1)
        c_event.constraint_id = kwargs.get('constraint_id', -1)
        c_event.rule_id = kwargs.get('rule_id', -1)
        c_event.var_id = kwargs.get('var_id', -1)
        c_event.merge_count = 0
        c_event.progress = kwargs.get('progress', -1.0)
        c_event.numeric_value = kwargs.get('numeric_value', 0.0)

        # 【修复 #2】使用 create_string_buffer 创建持久的 C 字符缓冲区
        # create_string_buffer 返回的 ctypes 数组对象拥有独立的内存块，
        # 赋值给 c_char_p 字段时 ctypes 会正确获取其内部指针。
        # 将缓冲区引用保存到 _persistent_buffers 防止 GC 回收。
        if isinstance(description, str):
            desc_buf = ctypes.create_string_buffer(description.encode('utf-8'))
        else:
            desc_buf = ctypes.create_string_buffer(description)
        c_event.description = desc_buf
        self._persistent_buffers.append(desc_buf)

        self.lib.stream_emit(self.ctx_ptr, ctypes.byref(c_event))
    
    def flush(self):
        """刷新缓冲区"""
        if self._loaded and self.ctx_ptr:
            self.lib.stream_flush(self.ctx_ptr)
    
    def get_stats(self):
        """获取事件统计"""
        if not self._loaded or not self.ctx_ptr:
            return {"total": 0, "dropped": 0}
        return {
            "total": self.lib.stream_get_total_event_count(self.ctx_ptr),
            "dropped": self.lib.stream_get_dropped_count(self.ctx_ptr),
        }
    
    def destroy(self):
        """销毁流式上下文和释放库引用"""
        if self.ctx_ptr and self.lib:
            self.lib.stream_context_destroy(self.ctx_ptr)
            self.ctx_ptr = None
        self._event_callback_wrapper = None
        self._event_callback_type = None
        self._event_callback_fn = None
        self._loaded = False
    
    @property
    def is_loaded(self):
        """C 库是否成功加载"""
        return self._loaded
    
    @property
    def load_error(self):
        """加载失败原因"""
        return self._load_error


def try_load_c_library():
    """
    尝试加载 C 共享库。
    成功返回 CTypesStreamBridge 实例，失败返回 None。
    """
    bridge = CTypesStreamBridge()
    if bridge.load():
        return bridge
    return None


# ================================================================
# Demo Engine Simulation (演示模式下模拟 C 引擎流程)
# ================================================================

# 【修复 #1】提取公共基类 _DemoEngineBase
# DemoEngine（纯 Python 模拟）和 DemoEngineCTypes（C 库桥接）有约 80% 重复代码，
# 主要是三个场景的事件序列定义完全一致。提取公共基类后：
#   - 场景事件序列定义在基类中，子类共享
#   - _emit_simple() 和 _emit_with_fields() 作为抽象接口，由子类实现
#   - 子类只需实现事件发射的具体方式（Python StreamContext vs C ctypes）
# 这样当需要新增场景或修改事件序列时，只需修改基类一处即可。

class _DemoEngineBase:
    """
    演示引擎公共基类 —— 定义场景事件序列和运行流程骨架。

    子类需要实现：
      - _emit_simple(event_type, description, step): 发射简单事件
      - _emit_with_fields(event_type, desc, node_id, constr_id, rule_id, var_id, progress, numeric): 发射带字段的事件

    场景定义（SCENARIOS）和三个场景运行方法（run_*_scenario）在基类中统一实现，
    子类通过 _emit_simple / _emit_with_fields 多态调用完成事件发射。
    """

    # 支持的场景定义（子类共享）
    SCENARIOS = {
        "triangle": "等边三角形构造 → 规范化 → 约束求解",
        "circle": "圆与线段相交 → 代数求解 → 坐标确定",
        "proof": "中点定理证明 → 合一检查 → 依赖链验证",
    }

    def __init__(self):
        self.step = 0
        self.total = 0

    def _emit_simple(self, event_type: StreamEventType, description: str, step: int = -1):
        """发射简单事件（仅类型 + 描述 + 步骤号）—— 子类必须实现"""
        raise NotImplementedError

    def _emit_with_fields(self, etype: StreamEventType, desc: str,
                          node_id: int = -1, constr_id: int = -1, rule_id: int = -1,
                          var_id: int = -1, progress: float = -1.0, numeric: float = 0.0):
        """发射带字段的事件 —— 子类必须实现"""
        raise NotImplementedError

    def _emit(self, etype: StreamEventType, desc: str,
              node_id: int = -1, constr_id: int = -1, rule_id: int = -1,
              var_id: int = -1, progress: float = -1.0, numeric: float = 0.0):
        """内部便捷方法：递增步骤计数并发射带字段的事件"""
        self.step += 1
        self._emit_with_fields(etype, desc, node_id, constr_id, rule_id, var_id, progress, numeric)
        # 模拟引擎计算延迟
        time.sleep(0.03 + (hash(desc) % 5) * 0.01)

    def _reset(self):
        """重置步骤计数器（每个场景开始前调用）"""
        self.step = 0

    # ---- 场景 1: 等边三角形构造 ----

    def run_triangle_scenario(self):
        """场景：等边三角形构造 → 规范化 → 求解"""
        self._reset()
        self.total = 18
        self._emit_simple(StreamEventType.ENGINE_START, "三角形场景引擎初始化完成", 0)

        # ---- 阶段 1: 添加节点 ----
        self._emit_simple(StreamEventType.INFO, "阶段 1/4: 添加构造节点")
        self._emit(StreamEventType.NODE_ADDED, "添加点 A(0, 0)", node_id=1)
        self._emit(StreamEventType.NODE_ADDED, "添加点 B(1, 0)", node_id=2)
        self._emit(StreamEventType.NODE_ADDED, "添加点 C 为自由变量", node_id=3)

        # ---- 阶段 2: 添加约束 ----
        self._emit_simple(StreamEventType.INFO, "阶段 2/4: 添加几何约束")
        self._emit(StreamEventType.CONSTRAINT_ADDED, "添加: dist(A,B) = 1 (约定)", constr_id=1)
        self._emit(StreamEventType.CONSTRAINT_ADDED, "添加: dist(A,C) = 1 (等边条件)", constr_id=2)
        self._emit(StreamEventType.CONSTRAINT_ADDED, "添加: dist(B,C) = 1 (等边条件)", constr_id=3)

        # ---- 阶段 3: 规范化 ----
        self._emit_simple(StreamEventType.NORMALIZE_START, "阶段 3/4: 图规范化", 5)
        self._emit(StreamEventType.NORMALIZE_MERGE, "并查集扫描: 3 节点 → 2 等价类", node_id=1)
        self._emit(StreamEventType.NORMALIZE_MERGE, "等价类合并: 点 B ≈ 点 A? — 跳过（不重合）", node_id=2)
        self._emit(StreamEventType.NORMALIZE_MERGE, "哈希预分组: 约束 1,2,3 归入 'dist' 组", constr_id=1)
        self._emit_simple(StreamEventType.NORMALIZE_DONE, "规范化完成: 0 合并, 3 节点保留, 3 约束", 9)

        # ---- 阶段 4: 代数求解 ----
        self._emit_simple(StreamEventType.SOLVE_START, "阶段 4/4: 符号代数求解", 10)
        self._emit(StreamEventType.SOLVE_EQUATION_EXTRACTED,
                   u"提取方程: (x_C - 0)^2 + (y_C - 0)^2 = 1", var_id=3)
        self._emit(StreamEventType.SOLVE_EQUATION_EXTRACTED,
                   u"提取方程: (x_C - 1)^2 + (y_C - 0)^2 = 1", var_id=3)
        self._emit(StreamEventType.SOLVE_GROEBNER_STEP,
                   u"Groebner 基: S-多项式约简 → x_C = 0.5", numeric=0.5)
        self._emit(StreamEventType.SOLVE_VARIABLE_RESOLVED,
                   "变量解得: x_C = 1/2 (精确有理数)", var_id=3, numeric=0.5)
        self._emit(StreamEventType.SOLVE_GROEBNER_STEP,
                   u"代入求解: y_C^2 = 3/4 → y_C = sqrt(3)/2", numeric=0.8660254)
        self._emit(StreamEventType.SOLVE_VARIABLE_RESOLVED,
                   u"变量解得: y_C = sqrt(3)/2 (二次扩域 Q[sqrt(3)])", var_id=3, numeric=0.8660254037844)
        self._emit_simple(StreamEventType.SOLVE_DONE, u"求解完成: 等边三角形 C(1/2, sqrt(3)/2)", 17)

        self._emit(StreamEventType.INFO, "C 坐标确定为 (0.5, 0.8660254)", progress=1.0)
        self._emit_simple(StreamEventType.ENGINE_DONE, "全部阶段完成", 18)

    # ---- 场景 2: 圆与线段相交 ----

    def run_circle_scenario(self):
        """场景：圆与线段相交 → 代数方程 → 两个交点"""
        self._reset()
        self.total = 16
        self._emit_simple(StreamEventType.ENGINE_START, "圆交点场景引擎初始化", 0)

        # 添加节点
        self._emit(StreamEventType.NODE_ADDED, "添加圆心 O(0, 0)", node_id=1)
        self._emit(StreamEventType.NODE_ADDED, "添加半径点 R(1, 0)", node_id=2)
        self._emit(StreamEventType.NODE_ADDED, "添加线段端点 P(0.5, -2)", node_id=3)
        self._emit(StreamEventType.NODE_ADDED, "添加线段端点 Q(0.5, 2)", node_id=4)
        self._emit(StreamEventType.NODE_ADDED, "添加交点 I 为自由变量", node_id=5)

        # 约束
        self._emit(StreamEventType.CONSTRAINT_ADDED, "圆约束: dist(O,I) = dist(O,R) = 1", constr_id=1)
        self._emit(StreamEventType.CONSTRAINT_ADDED, "关联约束: I 在线段 PQ 上", constr_id=2)

        # 规范化
        self._emit_simple(StreamEventType.NORMALIZE_START, "图规范化")
        self._emit(StreamEventType.NORMALIZE_MERGE, "节点哈希: 5 节点 → 5 独立等价类")
        self._emit_simple(StreamEventType.NORMALIZE_DONE, "规范化完成: 无合并")

        # 求解
        self._emit_simple(StreamEventType.SOLVE_START, "代数求解圆-线段交点")
        self._emit(StreamEventType.SOLVE_EQUATION_EXTRACTED,
                   u"x_I^2 + y_I^2 = 1  (圆方程)", var_id=5)
        self._emit(StreamEventType.SOLVE_EQUATION_EXTRACTED,
                   "x_I = 0.5  (线段垂线方程)", var_id=5)
        self._emit(StreamEventType.SOLVE_GROEBNER_STEP,
                   u"代入: 0.25 + y_I^2 = 1 → y_I^2 = 3/4")
        self._emit(StreamEventType.SOLVE_VARIABLE_RESOLVED,
                   u"多解分支 1: y_I = +sqrt(3)/2 ≈ 0.8660254", var_id=5, numeric=0.8660254)
        self._emit(StreamEventType.INFO, "多解分支: 2 个交点（上下对称）", progress=0.85)
        self._emit(StreamEventType.SOLVE_VARIABLE_RESOLVED,
                   u"多解分支 2: y_I = -sqrt(3)/2 ≈ -0.8660254", var_id=5, numeric=-0.8660254)
        self._emit_simple(StreamEventType.SOLVE_DONE,
                             u"求解完成: I1(0.5, 0.866), I2(0.5, -0.866)")
        self._emit(StreamEventType.GRAPH_SNAPSHOT, "图快照: 5 节点, 2 约束, 2 解分支", progress=1.0)
        self._emit_simple(StreamEventType.ENGINE_DONE, "圆交点场景完成")

    # ---- 场景 3: 中点定理证明 ----

    def run_proof_scenario(self):
        """场景：中点定理证明 → 合一检查 → 依赖链"""
        self._reset()
        self.total = 14
        self._emit_simple(StreamEventType.ENGINE_START, "中点定理证明引擎初始化", 0)

        self._emit(StreamEventType.NODE_ADDED, "构造三角形 ABC", node_id=1)
        self._emit(StreamEventType.CONSTRAINT_ADDED, "M 是 AB 中点", constr_id=1)
        self._emit(StreamEventType.CONSTRAINT_ADDED, "N 是 AC 中点", constr_id=2)
        self._emit(StreamEventType.CONSTRAINT_ADDED, "MN // BC 需要证明", constr_id=3)

        self._emit_simple(StreamEventType.NORMALIZE_START, "图规范化")
        self._emit(StreamEventType.NORMALIZE_MERGE, "等价类: AB 中点 M 语义检查")
        self._emit_simple(StreamEventType.NORMALIZE_DONE, "规范化完成")

        self._emit(StreamEventType.PROOF_STEP_ADDED, "证明步骤 1: 添加向量表示 AM = MB = AB/2")
        self._emit(StreamEventType.PROOF_STEP_ADDED, "证明步骤 2: 向量 AN = NC = AC/2")
        self._emit(StreamEventType.PROOF_UNIFY, "合一检查: MN 向量 ↔ (AN - AM) 模式匹配成功")
        self._emit(StreamEventType.PROOF_STEP_APPLIED, "应用: MN = AC/2 - AB/2 = (AC - AB)/2 = BC/2")
        self._emit(StreamEventType.PROOF_COLOR_UPDATE, "信任颜色: 中位线定理已验证 → 绿色")
        self._emit(StreamEventType.PROOF_DEPENDENCY_CHANGE, "依赖链: M,N 中点 → MN//BC (传递闭包)")
        self._emit(StreamEventType.INFO, "命题验证: MN // BC 成立 (中位线定理)", progress=1.0)
        self._emit_simple(StreamEventType.ENGINE_DONE, "中点定理证明完成")


class DemoEngine(_DemoEngineBase):
    """
    演示引擎（纯 Python 模拟）—— 在无 C 库环境下模拟完整的求解/规范化/重写流程。

    继承 _DemoEngineBase 的场景定义，通过 StreamContext 发射事件。
    """

    def __init__(self, ctx: StreamContext):
        super().__init__()
        self.ctx = ctx

    def _emit_simple(self, event_type: StreamEventType, description: str, step: int = -1):
        """通过 StreamContext 发射简单事件"""
        self.ctx.emit_simple(event_type, description, step)

    def _emit_with_fields(self, etype: StreamEventType, desc: str,
                          node_id: int = -1, constr_id: int = -1, rule_id: int = -1,
                          var_id: int = -1, progress: float = -1.0, numeric: float = 0.0):
        """通过 StreamContext 发射带字段的事件"""
        self.ctx.emit(StreamEvent(
            type=int(etype),
            step_number=self.step,
            total_steps=self.total,
            node_id=node_id,
            constraint_id=constr_id,
            rule_id=rule_id,
            var_id=var_id,
            description=desc,
            progress=progress,
            numeric_value=numeric,
        ))


class DemoEngineCTypes(_DemoEngineBase):
    """
    演示引擎（C 库桥接）—— 通过 CTypesStreamBridge 发射事件。

    继承 _DemoEngineBase 的场景定义，通过 C 共享库发射事件。
    与 DemoEngine 接口完全兼容，可互换使用。
    """

    def __init__(self, c_bridge: CTypesStreamBridge):
        super().__init__()
        self.bridge = c_bridge

    def _emit_simple(self, event_type: StreamEventType, description: str, step: int = -1):
        """通过 C 库发射简单事件"""
        self.bridge.emit_event(event_type, description, step_number=step)

    def _emit_with_fields(self, etype: StreamEventType, desc: str,
                          node_id: int = -1, constr_id: int = -1, rule_id: int = -1,
                          var_id: int = -1, progress: float = -1.0, numeric: float = 0.0):
        """通过 C 库发射带字段的事件"""
        self.bridge.emit_event(
            event_type=etype,
            description=desc,
            step_number=self.step,
            total_steps=self.total,
            node_id=node_id,
            constraint_id=constr_id,
            rule_id=rule_id,
            var_id=var_id,
            progress=progress,
            numeric_value=numeric,
        )


# ================================================================
# JSON Lines Writer (stdio 模式输出)
# ================================================================

class JsonLineWriter:
    """将 StreamEvent 序列化为 JSON Lines 写入 stdout"""

    @staticmethod
    def write(event: StreamEvent):
        line = json.dumps(event.to_json_dict(), ensure_ascii=False)
        sys.stdout.write(line + "\n")
        sys.stdout.flush()

    @staticmethod
    def write_jsonrpc(event: StreamEvent):
        line = json.dumps(event.to_jsonrpc(), ensure_ascii=False)
        sys.stdout.write(line + "\n")
        sys.stdout.flush()


# ================================================================
# SSE Server (HTTP Server-Sent Events 模式)
# ================================================================

def run_sse_server(port: int = 5801, cors_origins: Optional[List[str]] = None) -> None:
    """
    启动 HTTP SSE 服务器 —— 供浏览器前端直连（支持多连接并发）

    创建 HTTP 服务器，提供以下端点：
      - /events: SSE 事件流端点
      - /: 仪表盘 HTML 页面
      - /health: 健康检查端点

    Args:
        port: 监听端口号
        cors_origins: 【修复 #7】允许的 CORS 来源列表。
                      默认为 ["http://localhost:*", "http://127.0.0.1:*"]，
                      仅允许本地开发来源。传入 ["*"] 可恢复通配符行为（不推荐用于生产环境）。
    """
    # 【修复 #7】CORS 来源列表：默认仅允许本地来源，不再使用通配符 '*'
    # 生产环境应通过参数显式指定允许的域名列表
    if cors_origins is None:
        cors_origins = ["http://localhost", "http://127.0.0.1"]

    def _is_cors_allowed(origin: str) -> bool:
        """检查请求来源是否在允许的 CORS 列表中（支持通配符端口匹配）"""
        for allowed in cors_origins:
            if allowed == "*":
                return True
            # 支持通配符端口匹配，如 "http://localhost" 匹配 "http://localhost:5801"
            if origin == allowed or origin.startswith(allowed + ":") or origin.startswith(allowed + "/"):
                return True
        return False
    try:
        from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
    except ImportError:
        # Python 3.6 不支持 ThreadingHTTPServer，回退到单线程 HTTPServer
        from http.server import HTTPServer as ThreadingHTTPServer, BaseHTTPRequestHandler
        logger.warning("Python 版本过低，回退到单线程 HTTPServer（不支持多连接并发）")

    class SSEHandler(BaseHTTPRequestHandler):
        def do_GET(self):
            if self.path == "/events":
                self.send_response(200)
                self.send_header("Content-Type", "text/event-stream")
                self.send_header("Cache-Control", "no-cache")
                self.send_header("Connection", "keep-alive")
                # 【修复 #7】使用可配置的 CORS 来源列表替代通配符 '*'
                origin = self.headers.get("Origin", "")
                if _is_cors_allowed(origin):
                    self.send_header("Access-Control-Allow-Origin", origin)
                    self.send_header("Vary", "Origin")
                self.end_headers()

                ctx = StreamContext()
                # 注册 SSE 输出回调
                def sse_callback(event: StreamEvent):
                    data = json.dumps(event.to_json_dict(), ensure_ascii=False)
                    self.wfile.write(f"data: {data}\n\n".encode("utf-8"))
                    self.wfile.flush()

                # 尝试使用 C 库，回退到 Python 模拟
                c_bridge = try_load_c_library()
                use_c_lib = False
                if c_bridge and c_bridge.is_loaded:
                    c_bridge.register_callback(sse_callback)
                    engine = DemoEngineCTypes(c_bridge)
                    use_c_lib = True
                    print(f"[stream-bridge] SSE: Using C library (ctypes)", file=sys.stderr)
                else:
                    if c_bridge:
                        print(f"[stream-bridge] SSE: C library unavailable: {c_bridge.load_error}", file=sys.stderr)
                    ctx.register_callback(sse_callback)
                    engine = DemoEngine(ctx)
                    print(f"[stream-bridge] SSE: Using Python simulation", file=sys.stderr)

                try:
                    # 持续运行演示场景循环
                    scenarios = ["triangle", "circle", "proof"]
                    for scenario in scenarios:
                        getattr(engine, f"run_{scenario}_scenario")()
                        time.sleep(0.5)
                except (BrokenPipeError, ConnectionResetError):
                    pass
            elif self.path == "/":
                self.send_response(200)
                self.send_header("Content-Type", "text/html")
                self.end_headers()
                self.wfile.write(b"""<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>Lv-00 Stream Events</title>
<style>body{font-family:monospace;background:#0d1117;color:#c9d1d9;padding:16px}
.event{display:flex;gap:8px;padding:4px 0;border-bottom:1px solid #21262d;animation:fadeIn .3s}
.badge{padding:1px 6px;border-radius:4px;font-size:11px;font-weight:600;white-space:nowrap}
@keyframes fadeIn{from{opacity:0;transform:translateY(4px)}to{opacity:1}}</style></head>
<body><h2>Lv-00 Stream Events (SSE)</h2><div id="events"></div>
<script>const es=new EventSource("/events");es.onmessage=e=>{
const d=JSON.parse(e.data);const el=document.getElementById("events");
const div=document.createElement("div");div.className="event";
div.innerHTML=`<span class="badge" style="background:${d.color}20;color:${d.color}">${d.type_name}</span>
<span>${d.description||''}</span><span style="color:#8b949e;margin-left:auto">step ${d.step}</span>`;
el.prepend(div);if(el.children.length>100)el.lastChild.remove()}</script></body></html>""")
            elif self.path == "/health":
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(b'{"status":"ok","service":"lv00-stream-bridge"}')
            else:
                self.send_response(404)
                self.end_headers()

        def log_message(self, format, *args):
            pass  # 静默日志

    server = ThreadingHTTPServer(("127.0.0.1", port), SSEHandler)
    logger.info("SSE server listening on http://localhost:%d/events (ThreadingHTTPServer)", port)
    logger.info("Dashboard: http://localhost:%d/", port)

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[stream-bridge] Shutting down SSE server", file=sys.stderr)
        server.shutdown()


# ================================================================
# Stdio JSON-RPC Handler (stdio 模式)
# ================================================================

def handle_stdio_command(cmd: Dict[str, Any], ctx: StreamContext, engine: DemoEngine) -> Optional[Dict[str, Any]]:
    """处理 stdin JSON-RPC 命令并返回响应（可选）"""
    method = cmd.get("method", "")
    params = cmd.get("params", {})
    req_id = cmd.get("id")

    try:
        if method == "stream.start":
            scenario = params.get("scenario", "triangle")
            if scenario in DemoEngine.SCENARIOS:
                getattr(engine, f"run_{scenario}_scenario")()
                return {"jsonrpc": "2.0", "id": req_id, "result": {"status": "ok", "scenario": scenario}}
            else:
                return {"jsonrpc": "2.0", "id": req_id, "error": {"code": -1, "message": f"Unknown scenario: {scenario}"}}

        elif method == "stream.stop":
            return {"jsonrpc": "2.0", "id": req_id, "result": {"status": "stopped"}}

        elif method == "stream.stats":
            return {
                "jsonrpc": "2.0", "id": req_id,
                "result": {
                    "total": ctx.total_count,
                    "counts": {StreamEventType(k).name: v for k, v in ctx.event_counts.items()},
                }
            }

        elif method == "stream.filter":
            mask_str = params.get("mask", "all")
            # 解析过滤掩码（此处做简化处理，完整实现见 C 版 stream_parse_filter_mask）
            return {"jsonrpc": "2.0", "id": req_id, "result": {"filter": mask_str, "status": "applied"}}

        elif method == "ping":
            return {"jsonrpc": "2.0", "id": req_id, "result": "pong"}

        elif method == "shutdown":
            return {"jsonrpc": "2.0", "id": req_id, "result": "shutting_down"}

        else:
            return {"jsonrpc": "2.0", "id": req_id, "error": {"code": -32601, "message": f"Unknown method: {method}"}}

    except Exception as e:
        return {"jsonrpc": "2.0", "id": req_id, "error": {"code": -32000, "message": str(e)}}


def run_stdio_mode() -> None:
    """stdin JSON-RPC 命令 → stdout JSON Lines 事件流模式

    协议: 每行一个 JSON-RPC 请求，响应行也是 JSON-RPC，事件行是 JSON Lines。
    前端/spawner 可以通过管道与此进程通信。

    示例输入:
      {"jsonrpc":"2.0","method":"stream.start","params":{"scenario":"triangle"},"id":1}
      {"jsonrpc":"2.0","method":"stream.stats","id":2}
      {"jsonrpc":"2.0","method":"shutdown","id":3}
    """
    ctx = StreamContext()
    engine = DemoEngine(ctx)

    # 将事件回调绑定到 stdout JSON Lines 输出
    def output_callback(event: StreamEvent):
        JsonLineWriter.write(event)

    ctx.register_callback(output_callback)

    print("[stream-bridge] stdio mode ready, waiting for JSON-RPC commands...", file=sys.stderr)

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue

        try:
            cmd = json.loads(line)
        except json.JSONDecodeError:
            resp = {"jsonrpc": "2.0", "error": {"code": -32700, "message": "Parse error"}, "id": None}
            sys.stdout.write(json.dumps(resp) + "\n")
            sys.stdout.flush()
            continue

        resp = handle_stdio_command(cmd, ctx, engine)
        if resp:
            sys.stdout.write(json.dumps(resp, ensure_ascii=False) + "\n")
            sys.stdout.flush()

        # shutdown 命令退出
        if cmd.get("method") == "shutdown":
            print("[stream-bridge] Shutdown requested, exiting", file=sys.stderr)
            break


# ================================================================
# Demo Mode (直接输出事件流到 stdout)
# ================================================================

def run_demo_mode(scenario: str = "triangle") -> int:
    """
    演示模式 —— 模拟引擎运行并输出 JSON Lines 事件流到 stdout

    Args:
        scenario: 场景名称（triangle/circle/proof/all）

    Returns:
        int: 退出码（0 表示成功，1 表示失败）
    """
    # 尝试使用 C 库
    c_bridge = try_load_c_library()
    use_c_lib = False
    # 【修复 #3】确保 ctx 在所有代码路径中都被正确初始化
    # 原代码中 ctx 仅在 else 分支中赋值，当 use_c_lib=True 时
    # 后续的 else 分支引用 ctx.total_count 会导致 NameError
    ctx = StreamContext()  # 默认初始化，C 库模式下不会被使用
    if c_bridge and c_bridge.is_loaded:
        c_bridge.register_callback(lambda ev: JsonLineWriter.write(ev))
        engine = DemoEngineCTypes(c_bridge)
        use_c_lib = True
        print(f"[stream-bridge] Demo: Using C library (ctypes)", file=sys.stderr)
    else:
        if c_bridge:
            print(f"[stream-bridge] Demo: C library unavailable: {c_bridge.load_error}", file=sys.stderr)
        engine = DemoEngine(ctx)
        ctx.register_callback(JsonLineWriter.write)
        print(f"[stream-bridge] Demo: Using Python simulation", file=sys.stderr)

    print(f"[stream-bridge] Demo mode: scenario={scenario}", file=sys.stderr)

    if scenario == "all":
        for s in ["triangle", "circle", "proof"]:
            print(f"[stream-bridge] Running: {s}", file=sys.stderr)
            getattr(engine, f"run_{s}_scenario")()
            time.sleep(0.5)
    elif scenario in DemoEngine.SCENARIOS:
        getattr(engine, f"run_{scenario}_scenario")()
    else:
        print(f"[stream-bridge] Unknown scenario: {scenario}", file=sys.stderr)
        print(f"  Available: {', '.join(DemoEngine.SCENARIOS.keys())}, all", file=sys.stderr)
        return 1

    if use_c_lib:
        stats = c_bridge.get_stats()
        stats["mode"] = "c_library"
    else:
        stats = {
            "total_events": ctx.total_count,
            "counts": {StreamEventType(k).name: v for k, v in ctx.event_counts.items()},
            "mode": "python_simulation",
        }
    print(f"[stream-bridge] Demo complete: {stats.get('total', stats.get('total_events', 0))} events emitted", file=sys.stderr)
    return 0


# ================================================================
# CLI Entry Point
# ================================================================

def main() -> int:
    """
    CLI 入口函数

    解析命令行参数，根据指定的运行模式启动对应的处理函数。

    Returns:
        int: 退出码（0 表示成功）
    """
    parser = argparse.ArgumentParser(
        description="Lv-00 Stream Bridge — C engine stream events to JSON Lines / SSE bridge",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python stream_bridge.py demo --scenario triangle   # 演示等边三角形场景
  python stream_bridge.py demo --scenario all        # 演示全部场景
  python stream_bridge.py stdio                      # stdin/stdout JSON-RPC 模式
  python stream_bridge.py sse --port 5801            # HTTP SSE 服务器模式
  python stream_bridge.py sse --port 5801 --open     # SSE 模式并打开浏览器
        """,
    )
    subparsers = parser.add_subparsers(dest="mode", help="运行模式")

    # stdio 子命令
    subparsers.add_parser("stdio", help="stdin JSON-RPC → stdout JSON Lines 模式")

    # sse 子命令
    sse_parser = subparsers.add_parser("sse", help="HTTP SSE 服务器模式")
    sse_parser.add_argument("--port", type=int, default=5801, help="监听端口 (默认: 5801)")
    sse_parser.add_argument("--open", action="store_true", help="自动打开浏览器仪表盘")
    # 【修复 #7】添加 --cors-origins 参数，支持配置允许的跨域来源
    sse_parser.add_argument("--cors-origins", type=str, default=None,
                            help="允许的 CORS 来源，多个用逗号分隔 (默认: localhost, 127.0.0.1)")

    # demo 子命令
    demo_parser = subparsers.add_parser("demo", help="演示模式")
    demo_parser.add_argument("--scenario", type=str, default="triangle",
                             choices=["triangle", "circle", "proof", "all"],
                             help="模拟场景 (默认: triangle)")

    args = parser.parse_args()

    if args.mode == "stdio":
        return run_stdio_mode() or 0
    elif args.mode == "sse":
        if args.open:
            import webbrowser
            threading.Timer(1.0, lambda: webbrowser.open(f"http://localhost:{args.port}")).start()
        # 【修复 #7】解析 CORS 来源列表并传递给 SSE 服务器
        cors_list = None
        if args.cors_origins:
            cors_list = [s.strip() for s in args.cors_origins.split(",") if s.strip()]
        return run_sse_server(args.port, cors_origins=cors_list) or 0
    elif args.mode == "demo":
        return run_demo_mode(args.scenario)
    else:
        # 默认: demo triangle
        print("[stream-bridge] No mode specified, running demo mode (triangle)", file=sys.stderr)
        return run_demo_mode("triangle")


if __name__ == "__main__":
    sys.exit(main())
