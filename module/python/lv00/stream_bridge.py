#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Lv-00 流式桥接服务器 (Stream Bridge Server) - 兼容性重新导出层

连接 C 引擎和 Web 前端的 WebSocket 服务器，提供：
1. 引擎流式事件实时推送（归一化、求解、证明等）
2. AI 助手流式响应代理（OpenAI/DeepSeek 兼容 API）
3. JSON-RPC 2.0 协议支持
4. 自动重连和心跳检测

警告：此模块已拆分为两个文件：
    - stream_bridge.py: 引擎桥接器、事件持久化、多引擎管理、AI 代理
    - ws_server.py:     WebSocket 服务器、SSE 备选通道、客户端管理

所有导出名称保持不变，现有代码无需修改。

安全警告：
    本模块的 WebSocket 服务器不包含任何认证或授权机制。
    仅适用于受信任的内网开发环境，切勿直接暴露到公网。

使用方式:
    python stream_bridge.py --port 3456
    python stream_bridge.py --port 3456 --engine ../build/liblv00.so

前端连接:
    ws://localhost:3456
"""

import asyncio
import json
import logging
import os
import sys
import time
import argparse
from typing import Optional, Callable, Any, Dict, List
from ctypes import CDLL, CFUNCTYPE, c_void_p, c_int, c_long, c_double, c_char_p, POINTER, Structure

# 重新导出 WebSocket 服务器组件（供 main() 和外部使用）
from .ws_server import StreamBridgeServer, ClientSession, WEBSOCKETS_AVAILABLE
try:
    from .ws_server import serve
except ImportError:
    serve = None  # websockets 未安装时 serve 不可用

# 日志配置：库模块不应调用 logging.basicConfig()，由使用方（应用入口）负责全局日志配置。
logger = logging.getLogger('stream_bridge')

# ================================================================
# C 引擎绑定 / C Engine Bindings
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
        ('timestamp_ms', c_long),
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

# 回调函数类型
STREAM_CALLBACK = CFUNCTYPE(None, POINTER(StreamEvent), c_void_p)


class EngineBridge:
    """
    C 引擎桥接器。

    加载共享库并提供流式事件回调注册接口。
    负责管理 C 引擎的生命周期（加载、初始化、销毁），
    并将 C 层的流式事件转换为 Python 字典格式分发给事件处理器。

    属性：
        lib: 加载的 C 共享库对象（CDLL），未加载时为 None
        ctx: 流式上下文指针（c_void_p），未创建时为 None
        callback_id: 注册的回调 ID（-1 表示未注册）
        _event_handlers: 已注册的事件处理器列表
    """

    def __init__(self, lib_path: Optional[str] = None) -> None:
        self.lib: Optional[CDLL] = None
        self.ctx: Optional[c_void_p] = None
        self.callback_id: int = -1
        self._event_handlers: List[Callable[[Dict], None]] = []

        # 尝试加载引擎
        self._load_engine(lib_path)

    def _load_engine(self, lib_path: Optional[str]) -> bool:
        """加载 C 引擎共享库。

        搜索策略：
        1. 如果指定了 lib_path，优先尝试加载
        2. 否则按预定义的默认路径列表依次尝试

        参数：
            lib_path: 共享库路径（可选）

        返回：
            bool: 加载成功返回 True，否则返回 False
        """
        if lib_path and os.path.exists(lib_path):
            try:
                self.lib = CDLL(lib_path)
                self._setup_functions()
                logger.info(f"成功加载引擎: {lib_path}")
                return True
            except Exception as e:
                logger.warning(f"加载引擎失败 {lib_path}: {e}")

        # 尝试默认路径
        default_paths = [
            './build/liblv00.so',
            './build/liblv00.dylib',
            './build/lv00.dll',
            '../build/liblv00.so',
            '../build/liblv00.dylib',
            '../build/lv00.dll',
        ]

        for path in default_paths:
            if os.path.exists(path):
                try:
                    self.lib = CDLL(path)
                    self._setup_functions()
                    logger.info(f"成功加载引擎: {path}")
                    return True
                except Exception as e:
                    logger.debug(f"尝试加载 {path} 失败: {e}")

        logger.warning("未找到 C 引擎共享库，将使用模拟模式")
        return False

    def _setup_functions(self) -> None:
        """
        设置 C 函数的返回类型和参数类型。

        使用 ctypes 的 restype 和 argtypes 属性定义 C 库函数的签名，
        确保 Python 和 C 之间的数据传递正确。
        """
        if not self.lib:
            return

        # stream_context_create
        self.lib.stream_context_create.restype = c_void_p
        self.lib.stream_context_create.argtypes = []

        # stream_context_destroy
        self.lib.stream_context_destroy.restype = None
        self.lib.stream_context_destroy.argtypes = [c_void_p]

        # stream_register_callback_ex
        self.lib.stream_register_callback_ex.restype = c_int
        self.lib.stream_register_callback_ex.argtypes = [
            c_void_p, STREAM_CALLBACK, c_void_p, c_int  # 使用 uint64_t 但 Python 中简化为 int
        ]

        # stream_unregister_callback_by_id
        self.lib.stream_unregister_callback_by_id.restype = c_int
        self.lib.stream_unregister_callback_by_id.argtypes = [c_void_p, c_int]

        # stream_emit_simple
        self.lib.stream_emit_simple.restype = None
        self.lib.stream_emit_simple.argtypes = [c_void_p, c_int, c_char_p, c_int]

        # stream_event_to_json
        self.lib.stream_event_to_json.restype = c_int
        self.lib.stream_event_to_json.argtypes = [POINTER(StreamEvent), c_char_p, c_int]

        # stream_event_type_name
        self.lib.stream_event_type_name.restype = c_char_p
        self.lib.stream_event_type_name.argtypes = [c_int]

        # stream_event_type_id
        self.lib.stream_event_type_id.restype = c_char_p
        self.lib.stream_event_type_id.argtypes = [c_int]

        # stream_event_color
        self.lib.stream_event_color.restype = c_char_p
        self.lib.stream_event_color.argtypes = [c_int]

    def create_context(self) -> bool:
        """
        创建流式上下文。

        创建用于接收引擎事件的上下文对象，并注册默认回调处理器。
        每个 EngineBridge 实例只能创建一个上下文。

        返回：
            bool: 创建成功返回 True，失败返回 False
        """
        if not self.lib:
            return False

        self.ctx = self.lib.stream_context_create()
        if not self.ctx:
            logger.error("创建流式上下文失败")
            return False

        # 注册回调
        @STREAM_CALLBACK
        def callback(event_ptr: POINTER(StreamEvent), user_data: c_void_p):
            event = event_ptr.contents
            self._handle_event(event)

        self._callback = callback  # 保持引用
        self.callback_id = self.lib.stream_register_callback_ex(
            self.ctx, callback, None, 0xFFFFFFFFFFFFFFFF  # STREAM_FILTER_ALL
        )

        if self.callback_id < 0:
            logger.error("注册回调失败")
            return False

        logger.info(f"流式上下文已创建，回调 ID: {self.callback_id}")
        return True

    def destroy_context(self) -> None:
        """销毁流式上下文，释放关联的 C 资源。"""
        if self.lib and self.ctx:
            if self.callback_id >= 0:
                self.lib.stream_unregister_callback_by_id(self.ctx, self.callback_id)
            self.lib.stream_context_destroy(self.ctx)
            self.ctx = None
            self.callback_id = -1
            logger.info("流式上下文已销毁")

    def _handle_event(self, event: StreamEvent):
        """
        处理从 C 引擎接收到的原始事件。

        将原始事件结构体转换为字典格式，并分发给所有已注册的
        事件处理器。处理器异常会被捕获并记录，但不会中断其他处理器。

        参数：
            event: C 引擎发送的 StreamEvent 结构体实例
        """
        try:
            # 构建事件字典
            event_dict = {
                'type': self.lib.stream_event_type_id(event.type).decode('utf-8')
                        if self.lib else f'EVENT_{event.type}',
                'type_name': self.lib.stream_event_type_name(event.type).decode('utf-8')
                             if self.lib else f'Event {event.type}',
                'color': self.lib.stream_event_color(event.type).decode('utf-8')
                         if self.lib else '#888888',
                'timestamp_ms': event.timestamp_ms,
                'step': event.step_number,
                'total_steps': event.total_steps,
                'node_id': event.node_id,
                'constraint_id': event.constraint_id,
                'rule_id': event.rule_id,
                'var_id': event.var_id,
                'description': event.description.decode('utf-8')
                              if event.description else '',
                'detail_json': event.detail_json.decode('utf-8')
                              if event.detail_json else '',
                'progress': event.progress,
                'numeric_value': event.numeric_value,
            }

            # 通知所有处理器
            for handler in self._event_handlers:
                try:
                    handler(event_dict)
                except Exception as e:
                    logger.error(f"事件处理器错误: {e}")

        except Exception as e:
            logger.error(f"处理事件错误: {e}")

    def add_event_handler(self, handler: Callable[[Dict], None]) -> None:
        """添加事件处理器。

        参数：
            handler: 事件处理函数，接受事件字典作为参数
        """
        self._event_handlers.append(handler)

    def remove_event_handler(self, handler: Callable[[Dict], None]) -> None:
        """移除事件处理器。

        参数：
            handler: 要移除的事件处理函数
        """
        if handler in self._event_handlers:
            self._event_handlers.remove(handler)

    def emit_simulated_event(self, event_type: int, description: str, step: int = 0) -> None:
        """
        发射模拟事件（用于测试）。

        在模拟模式或测试场景下使用，生成一个模拟事件并分发给
        所有已注册的事件处理器。在实际模式下，会调用 C 库的
        stream_emit_simple 函数。

        参数：
            event_type: 模拟事件类型编码
            description: 事件描述文本
            step: 步骤编号（默认 0）
        """
        if self.lib and self.ctx:
            self.lib.stream_emit_simple(
                self.ctx, event_type, description.encode('utf-8'), step
            )
        else:
            # 模拟模式
            event_dict = {
                'type': f'SIMULATED_{event_type}',
                'type_name': f'模拟事件 {event_type}',
                'color': '#888888',
                'timestamp_ms': int(time.time() * 1000),
                'step': step,
                'total_steps': -1,
                'node_id': -1,
                'constraint_id': -1,
                'rule_id': -1,
                'var_id': -1,
                'description': description,
                'detail_json': '',
                'progress': -1.0,
                'numeric_value': 0.0,
            }
            for handler in self._event_handlers:
                try:
                    handler(event_dict)
                except Exception as e:
                    import logging
                    logging.getLogger('lv00.stream_bridge').error(
                        f"模拟事件处理器错误: {e}"
                    )



# 事件持久化 / Event Persistence
# ================================================================

class EventPersistence:
    """事件持久化：将流式事件保存到本地 JSONL 文件，支持回放。

    使用 JSONL 格式（每行一个 JSON 对象）存储事件记录。
    每个事件记录包含：timestamp, event_type, category, description,
    detail_json, session_id。

    功能：
        - append(event): 追加事件到 JSONL 文件
        - replay(callback): 从文件读取事件并逐个回调
        - clear(): 清空持久化文件
        - get_stats(): 返回持久化事件统计

    示例：
        >>> persistence = EventPersistence()
        >>> persistence.append({'type': 'normalization', 'description': '归一化完成'})
        >>> persistence.replay(print)
        >>> stats = persistence.get_stats()
    """

    def __init__(self, file_path: Optional[str] = None) -> None:
        """初始化事件持久化器。

        参数：
            file_path: JSONL 文件路径（默认 ~/.lv00/stream_events.jsonl）
        """
        if file_path:
            self.file_path = os.path.expanduser(file_path)
        else:
            self.file_path = os.path.expanduser('~/.lv00/stream_events.jsonl')

        # 确保目录存在
        file_dir = os.path.dirname(self.file_path)
        if file_dir:
            os.makedirs(file_dir, exist_ok=True)

        logger.info(f"事件持久化已初始化: {self.file_path}")

    def append(self, event: Dict, session_id: Optional[str] = None) -> None:
        """追加事件到 JSONL 文件。

        将事件封装为持久化记录并追加到文件末尾。每条记录包含
        时间戳、事件类型、分类、描述、详情和会话 ID。

        参数：
            event: 事件字典，至少包含 'type' 和 'description' 字段
            session_id: 会话标识符（可选）
        """
        record = {
            'timestamp': time.time(),
            'event_type': event.get('type', event.get('type_name', 'unknown')),
            'category': self._infer_category(event),
            'description': event.get('description', ''),
            'detail_json': event.get('detail_json', ''),
            'session_id': session_id or '',
        }

        try:
            with open(self.file_path, 'a', encoding='utf-8') as f:
                f.write(json.dumps(record, ensure_ascii=False) + '\n')
        except Exception as e:
            logger.error(f"写入持久化文件失败: {e}")

    def replay(self, callback: Callable[[Dict], None],
               session_id: Optional[str] = None,
               start_time: Optional[float] = None,
               end_time: Optional[float] = None) -> int:
        """从文件读取事件并逐个回调。

        按顺序读取 JSONL 文件中的事件记录，对每条记录调用回调函数。
        支持按 session_id 和时间范围过滤。

        参数：
            callback: 回调函数，接受事件记录字典作为参数
            session_id: 会话 ID 过滤（可选，为 None 时不过滤）
            start_time: 起始时间戳（可选，为 None 时不限制）
            end_time: 结束时间戳（可选，为 None 时不限制）

        返回：
            int: 回放的事件数量
        """
        if not os.path.exists(self.file_path):
            logger.warning(f"持久化文件不存在: {self.file_path}")
            return 0

        count = 0
        try:
            with open(self.file_path, 'r', encoding='utf-8') as f:
                for line in f:
                    line = line.strip()
                    if not line:
                        continue

                    try:
                        record = json.loads(line)
                    except json.JSONDecodeError:
                        logger.debug(f"跳过无效 JSON 行: {line[:50]}...")
                        continue

                    # 应用过滤条件
                    if session_id and record.get('session_id') != session_id:
                        continue
                    if start_time and record.get('timestamp', 0) < start_time:
                        continue
                    if end_time and record.get('timestamp', 0) > end_time:
                        continue

                    callback(record)
                    count += 1

        except Exception as e:
            logger.error(f"读取持久化文件失败: {e}")

        logger.info(f"回放完成，共 {count} 条事件")
        return count

    def clear(self) -> bool:
        """清空持久化文件。

        返回：
            bool: 清空成功返回 True，失败返回 False
        """
        try:
            with open(self.file_path, 'w', encoding='utf-8') as f:
                pass  # 清空文件内容
            logger.info("持久化文件已清空")
            return True
        except Exception as e:
            logger.error(f"清空持久化文件失败: {e}")
            return False

    def get_stats(self) -> Dict:
        """返回持久化事件统计。

        统计文件中的事件总数、各类型事件数量、时间范围等信息。

        返回：
            Dict: 统计信息字典，包含以下字段：
                - total: 事件总数
                - file_path: 文件路径
                - file_size_bytes: 文件大小（字节）
                - event_types: 各事件类型及其数量
                - sessions: 会话 ID 集合
                - time_range: [最早时间, 最晚时间]
        """
        stats = {
            'total': 0,
            'file_path': self.file_path,
            'file_size_bytes': 0,
            'event_types': {},
            'sessions': set(),
            'time_range': [None, None],
        }

        if not os.path.exists(self.file_path):
            return stats

        stats['file_size_bytes'] = os.path.getsize(self.file_path)

        try:
            with open(self.file_path, 'r', encoding='utf-8') as f:
                for line in f:
                    line = line.strip()
                    if not line:
                        continue

                    try:
                        record = json.loads(line)
                    except json.JSONDecodeError:
                        continue

                    stats['total'] += 1

                    # 统计事件类型
                    event_type = record.get('event_type', 'unknown')
                    stats['event_types'][event_type] = \
                        stats['event_types'].get(event_type, 0) + 1

                    # 收集会话 ID
                    sid = record.get('session_id', '')
                    if sid:
                        stats['sessions'].add(sid)

                    # 更新时间范围
                    ts = record.get('timestamp', 0)
                    if stats['time_range'][0] is None or ts < stats['time_range'][0]:
                        stats['time_range'][0] = ts
                    if stats['time_range'][1] is None or ts > stats['time_range'][1]:
                        stats['time_range'][1] = ts

        except Exception as e:
            logger.error(f"读取持久化文件失败: {e}")

        # 将 set 转为 list 以便 JSON 序列化
        stats['sessions'] = list(stats['sessions'])
        return stats

    @staticmethod
    def _infer_category(event: Dict) -> str:
        """根据事件类型推断分类。

        参数：
            event: 事件字典

        返回：
            str: 推断的事件分类
        """
        event_type = str(event.get('type', '')).lower()
        type_name = str(event.get('type_name', '')).lower()

        # 归一化相关
        if 'normali' in event_type or 'normali' in type_name:
            return 'normalization'
        # 求解相关
        if 'sol' in event_type or '求解' in type_name:
            return 'solving'
        # 证明相关
        if 'proof' in event_type or '证明' in type_name:
            return 'proof'
        # 合并相关
        if 'merge' in event_type or '合并' in type_name:
            return 'merge'
        # 错误相关
        if 'error' in event_type or '错误' in type_name:
            return 'error'
        # 进度相关
        if 'progress' in event_type or '进度' in type_name:
            return 'progress'
        # 模拟事件
        if 'simulated' in event_type or '模拟' in type_name:
            return 'simulation'

        return 'other'


# ================================================================
# 多引擎实例管理 / Multi-Engine Manager
# ================================================================

class MultiEngineManager:
    """多引擎实例管理器：支持同时连接多个 C 引擎实例。

    使用字典管理多个 EngineBridge 实例，每个实例有唯一名称。
    支持向所有实例广播事件和聚合统计信息。

    功能：
        - register(name, bridge): 注册引擎桥接实例
        - unregister(name): 注销引擎实例
        - get(name): 获取引擎实例
        - list_engines(): 列出所有注册的引擎
        - broadcast_event(event): 向所有引擎广播事件
        - aggregate_stats(): 聚合所有引擎的统计信息

    示例：
        >>> manager = MultiEngineManager()
        >>> engine_a = EngineBridge('./build/liblv00.so')
        >>> manager.register('primary', engine_a)
        >>> manager.list_engines()
        >>> manager.broadcast_event({'type': 'test', 'description': '测试'})
    """

    def __init__(self) -> None:
        """初始化多引擎管理器。"""
        self._engines: Dict[str, EngineBridge] = {}
        logger.info("多引擎管理器已初始化")

    def register(self, name: str, bridge: EngineBridge) -> bool:
        """注册引擎桥接实例。

        将一个 EngineBridge 实例注册到管理器中，使用唯一名称标识。

        参数：
            name: 引擎实例的唯一名称
            bridge: EngineBridge 实例

        返回：
            bool: 注册成功返回 True，名称已存在返回 False
        """
        if name in self._engines:
            logger.warning(f"引擎 '{name}' 已存在，注册失败")
            return False

        self._engines[name] = bridge
        logger.info(f"引擎已注册: {name}")
        return True

    def unregister(self, name: str) -> bool:
        """注销引擎实例。

        从管理器中移除指定名称的引擎实例，并销毁其上下文。

        参数：
            name: 要注销的引擎实例名称

        返回：
            bool: 注销成功返回 True，名称不存在返回 False
        """
        if name not in self._engines:
            logger.warning(f"引擎 '{name}' 不存在，注销失败")
            return False

        bridge = self._engines.pop(name)
        bridge.destroy_context()
        logger.info(f"引擎已注销: {name}")
        return True

    def get(self, name: str) -> Optional[EngineBridge]:
        """获取引擎实例。

        参数：
            name: 引擎实例名称

        返回：
            Optional[EngineBridge]: 对应的 EngineBridge 实例，不存在时返回 None
        """
        return self._engines.get(name)

    def list_engines(self) -> List[Dict]:
        """列出所有注册的引擎。

        返回：
            List[Dict]: 引擎信息列表，每个元素包含 name 和 loaded 状态
        """
        result = []
        for name, bridge in self._engines.items():
            result.append({
                'name': name,
                'loaded': bridge.lib is not None,
                'context_created': bridge.ctx is not None,
                'callback_id': bridge.callback_id,
            })
        return result

    def broadcast_event(self, event: Dict) -> int:
        """向所有引擎广播事件。

        将事件分发给所有已注册引擎实例的事件处理器。
        如果引擎处于模拟模式，则通过 emit_simulated_event 发送。

        参数：
            event: 事件字典，包含 type 和 description 字段

        返回：
            int: 成功接收事件的引擎数量
        """
        count = 0
        event_type = event.get('type', 0)
        description = event.get('description', '')
        step = event.get('step', 0)

        for name, bridge in self._engines.items():
            try:
                bridge.emit_simulated_event(event_type, description, step)
                count += 1
            except Exception as e:
                logger.error(f"向引擎 '{name}' 广播事件失败: {e}")

        if count > 0:
            logger.debug(f"事件已广播到 {count} 个引擎")
        return count

    def aggregate_stats(self) -> Dict:
        """聚合所有引擎的统计信息。

        收集所有已注册引擎的状态信息，汇总为整体统计。

        返回：
            Dict: 聚合统计信息，包含以下字段：
                - total_engines: 引擎总数
                - loaded_engines: 已加载的引擎数量
                - active_engines: 已创建上下文的引擎数量
                - engines: 各引擎的详细状态列表
        """
        total = len(self._engines)
        loaded = 0
        active = 0
        engine_details = []

        for name, bridge in self._engines.items():
            is_loaded = bridge.lib is not None
            is_active = bridge.ctx is not None
            if is_loaded:
                loaded += 1
            if is_active:
                active += 1

            engine_details.append({
                'name': name,
                'loaded': is_loaded,
                'active': is_active,
                'callback_id': bridge.callback_id,
                'handler_count': len(bridge._event_handlers),
            })

        return {
            'total_engines': total,
            'loaded_engines': loaded,
            'active_engines': active,
            'engines': engine_details,
        }


# ================================================================
# AI 助手代理 / AI Assistant Proxy
# ================================================================

class AIAssistantProxy:
    """
    AI 助手代理。

    支持 OpenAI/DeepSeek 兼容 API 的流式响应。
    用于将 Lv-00 引擎事件转发给 AI 助手进行自然语言解释。

    属性：
        endpoint: API 端点 URL（不含尾部斜杠）
        api_key: API 认证密钥
        model: 使用的模型名称（默认 'gpt-4o'）
    """

    def __init__(self, endpoint: str, api_key: str, model: Optional[str] = None) -> None:
        self.endpoint = endpoint.rstrip('/')
        self.api_key = api_key
        # 优先使用参数传入的模型名，其次从环境变量读取，最后回退到默认值
        self.model = model or os.environ.get('LV00_AI_MODEL', 'gpt-4o')

    async def chat_stream(self, messages: List[Dict], on_chunk: Callable[[str], None]) -> str:
        """
        流式聊天请求。

        向兼容 OpenAI 的 API 发送聊天请求，以流式方式接收响应，
        并通过 on_chunk 回调函数逐块返回内容。

        参数：
            messages: 消息列表，格式为 [{"role": "user", "content": "..."}]
            on_chunk: 分块回调函数，每收到一个文本块时调用

        返回：
            str: 完整的响应文本（所有块拼接后的结果）
        """
        try:
            import aiohttp
        except ImportError:
            logger.error("aiohttp 库未安装")
            return ""

        url = f"{self.endpoint}/v1/chat/completions"
        headers = {
            'Content-Type': 'application/json',
            'Authorization': f'Bearer {self.api_key}'
        }
        payload = {
            'model': self.model,
            'messages': messages,
            'stream': True
        }

        full_content = ''

        try:
            async with aiohttp.ClientSession() as session:
                async with session.post(url, headers=headers, json=payload) as response:
                    if response.status != 200:
                        error_text = await response.text()
                        logger.error(f"API 请求失败: {response.status} - {error_text}")
                        return ""

                    async for line in response.content:
                        line_text = line.decode('utf-8').strip()
                        if not line_text or not line_text.startswith('data: '):
                            continue

                        data_str = line_text[6:]  # 去掉 'data: '
                        if data_str == '[DONE]':
                            break

                        try:
                            data = json.loads(data_str)
                            choices = data.get('choices', [])
                            if choices:
                                delta = choices[0].get('delta', {})
                                content = delta.get('content', '')
                                if content:
                                    full_content += content
                                    on_chunk(content)
                        except json.JSONDecodeError:
                            continue

        except Exception as e:
            logger.error(f"流式请求错误: {e}")

        return full_content


# ================================================================
# 主入口 / Main Entry
# ================================================================

def main() -> None:
    """
    流式桥接服务器主入口函数。

    解析命令行参数，创建引擎桥接器和服务器实例，
    并启动异步事件循环。支持 SSE 备选通道和事件持久化。
    """
    parser = argparse.ArgumentParser(description='Lv-00 流式桥接服务器')
    parser.add_argument('--host', default='localhost', help='监听地址 (默认: localhost)')
    parser.add_argument('--port', type=int, default=3456, help='监听端口 (默认: 3456)')
    parser.add_argument('--engine', help='C 引擎共享库路径')
    parser.add_argument('--verbose', '-v', action='store_true', help='详细日志')

    # SSE 备选通道参数
    parser.add_argument('--sse', action='store_true', help='启用 SSE 备选通道')
    parser.add_argument('--sse-port', type=int, default=3457, help='SSE 端口 (默认: 3457)')

    # 事件持久化参数
    parser.add_argument('--persist', action='store_true', help='启用事件持久化')
    parser.add_argument('--persist-file', help='持久化文件路径 (默认: ~/.lv00/stream_events.jsonl)')
    parser.add_argument('--replay', action='store_true', help='从持久化文件回放事件')

    args = parser.parse_args()

    if args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)

    # 创建引擎桥接器
    engine = EngineBridge(args.engine)

    # 创建服务器
    server = StreamBridgeServer(engine, args.host, args.port)

    # 事件持久化
    persistence = None
    if args.persist or args.replay:
        persistence = EventPersistence(args.persist_file)

        if args.persist:
            # 注册持久化处理器：将引擎事件追加到文件
            engine.add_event_handler(
                lambda event: persistence.append(event)
            )
            logger.info("事件持久化已启用")

        if args.replay:
            # 回放持久化事件
            print("=== 回放持久化事件 ===")
            stats = persistence.get_stats()
            print(f"持久化文件: {stats['file_path']}")
            print(f"事件总数: {stats['total']}")
            if stats['time_range'][0] is not None:
                from datetime import datetime
                t0 = datetime.fromtimestamp(stats['time_range'][0])
                t1 = datetime.fromtimestamp(stats['time_range'][1])
                print(f"时间范围: {t0} ~ {t1}")
            print()

            def print_event(record: Dict) -> None:
                """打印回放事件。"""
                from datetime import datetime
                ts = datetime.fromtimestamp(record.get('timestamp', 0))
                etype = record.get('event_type', 'unknown')
                desc = record.get('description', '')
                print(f"  [{ts}] [{etype}] {desc}")

            persistence.replay(print_event)
            print("=== 回放完成 ===")

            # 如果仅回放，不启动服务器
            if not args.persist and not args.sse:
                return

    # 异步主函数
    async def async_main() -> None:
        """异步主函数，同时运行 WebSocket 和 SSE 服务器。"""
        # 创建引擎上下文
        engine.create_context()

        # 启动 WebSocket 服务器
        tasks = []
        if WEBSOCKETS_AVAILABLE:
            logger.info(f"流式桥接服务器启动: ws://{args.host}:{args.port}")
            ws_server = await serve(server._handle_client, args.host, args.port)
            tasks.append(asyncio.create_task(ws_server.wait_closed()))
        else:
            logger.warning("WebSocket 不可用，仅 SSE 通道可用")

        # 启动 SSE 服务器
        if args.sse:
            await server.start_sse_server(args.host, args.sse_port)

        server._running = True

        try:
            # 等待所有任务
            if tasks:
                await asyncio.gather(*tasks)
            else:
                await asyncio.Future()  # 永久运行
        except asyncio.CancelledError:
            pass
        finally:
            server._running = False
            server.stop_sse_server()
            engine.destroy_context()

    try:
        asyncio.run(async_main())
    except KeyboardInterrupt:
        logger.info("收到中断信号")
        server.stop()
        server.stop_sse_server()


if __name__ == '__main__':
    main()


