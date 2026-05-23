#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Lv-00 流式桥接服务器 (Stream Bridge Server)

连接 C 引擎和 Web 前端的 WebSocket 服务器，提供：
1. 引擎流式事件实时推送（归一化、求解、证明等）
2. AI 助手流式响应代理（OpenAI/DeepSeek 兼容 API）
3. JSON-RPC 2.0 协议支持
4. 自动重连和心跳检测

使用方式:
    python stream_bridge.py --port 3456
    python stream_bridge.py --port 3456 --engine ../build/liblv00.so

前端连接:
    ws://localhost:3456

协议:
    - JSON-RPC 2.0 请求/响应
    - 事件通知: {"jsonrpc": "2.0", "method": "stream.event", "params": {...}}
    - 订阅: {"jsonrpc": "2.0", "method": "subscribe", "params": {"event_mask": -1}, "id": 1}
"""

import asyncio
import json
import logging
import os
import sys
import time
import argparse
from dataclasses import dataclass, field
from typing import Optional, Callable, Any, Dict, List, Set
from ctypes import CDLL, CFUNCTYPE, c_void_p, c_int, c_long, c_double, c_char_p, POINTER, Structure

# 配置日志
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(name)s: %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)
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
    C 引擎桥接器

    加载共享库并提供流式事件回调注册接口。
    """

    def __init__(self, lib_path: Optional[str] = None):
        self.lib: Optional[CDLL] = None
        self.ctx: Optional[c_void_p] = None
        self.callback_id: int = -1
        self._event_handlers: List[Callable[[Dict], None]] = []

        # 尝试加载引擎
        self._load_engine(lib_path)

    def _load_engine(self, lib_path: Optional[str]) -> bool:
        """加载 C 引擎共享库"""
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

    def _setup_functions(self):
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

    def destroy_context(self):
        """销毁流式上下文"""
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

    def add_event_handler(self, handler: Callable[[Dict], None]):
        """添加事件处理器"""
        self._event_handlers.append(handler)

    def remove_event_handler(self, handler: Callable[[Dict], None]):
        """移除事件处理器"""
        if handler in self._event_handlers:
            self._event_handlers.remove(handler)

    def emit_simulated_event(self, event_type: int, description: str, step: int = 0):
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
                'progress': -1.0,
                'numeric_value': 0.0,
            }
            for handler in self._event_handlers:
                handler(event_dict)


# ================================================================
# WebSocket 服务器 / WebSocket Server
# ================================================================

try:
    import websockets
    from websockets.server import serve, WebSocketServerProtocol
    WEBSOCKETS_AVAILABLE = True
except ImportError:
    WEBSOCKETS_AVAILABLE = False
    logger.warning("websockets 库未安装，WebSocket 功能不可用。安装: pip install websockets")


@dataclass
class ClientSession:
    """
    WebSocket 客户端会话数据类。

    存储与 WebSocket 客户端连接相关的会话信息。

    属性：
        websocket: WebSocket 连接对象
        subscriptions: 已订阅的事件类型集合
        event_mask: 事件掩码（-1 表示订阅所有事件）
        last_ping: 最后一次 ping 的时间戳
        last_pong: 最后一次 pong 的时间戳
    """

    websocket: Any
    subscriptions: Set[str] = field(default_factory=set)
    event_mask: int = -1  # -1 = 订阅所有
    last_ping: float = 0.0
    last_pong: float = 0.0


class StreamBridgeServer:
    """
    流式桥接服务器。

    提供 WebSocket 接口，将引擎事件实时推送给前端。
    支持 JSON-RPC 2.0 协议的请求/响应和事件通知。

    属性：
        engine: 引擎桥接器实例
        host: 监听主机地址
        port: 监听端口
        clients: 客户端会话字典（client_id -> ClientSession）

    示例：
        >>> engine = EngineBridge()
        >>> server = StreamBridgeServer(engine, 'localhost', 3456)
        >>> asyncio.run(server.start())
    """

    def __init__(self, engine: EngineBridge, host: str = 'localhost', port: int = 3456):
        self.engine = engine
        self.host = host
        self.port = port
        self.clients: Dict[int, ClientSession] = {}
        self._next_client_id = 1
        self._running = False

        # 注册引擎事件处理器
        self.engine.add_event_handler(self._on_engine_event)

    def _on_engine_event(self, event: Dict):
        """
        引擎事件回调处理器。

        当引擎产生新事件时被调用，将事件封装为 JSON-RPC 通知
        并广播给所有订阅的客户端。

        参数：
            event: 事件字典，包含事件类型、描述等信息
        """
        if not self._running:
            return

        # 构建 JSON-RPC 通知
        notification = {
            'jsonrpc': '2.0',
            'method': 'stream.event',
            'params': event
        }

        # 广播给所有订阅的客户端
        asyncio.create_task(self._broadcast(notification))

    async def _broadcast(self, message: Dict):
        """广播消息给所有客户端"""
        if not self.clients:
            return

        message_json = json.dumps(message, ensure_ascii=False)
        disconnected = []

        for client_id, session in self.clients.items():
            try:
                await session.websocket.send(message_json)
            except Exception as e:
                logger.debug(f"发送给客户端 {client_id} 失败: {e}")
                disconnected.append(client_id)

        # 清理断开的客户端
        for client_id in disconnected:
            del self.clients[client_id]
            logger.info(f"客户端 {client_id} 已断开")

    async def _handle_client(self, websocket: WebSocketServerProtocol, path: str = ''):
        """处理 WebSocket 客户端连接"""
        client_id = self._next_client_id
        self._next_client_id += 1

        session = ClientSession(websocket=websocket)
        self.clients[client_id] = session

        client_addr = websocket.remote_address
        logger.info(f"客户端 {client_id} 已连接: {client_addr}")

        try:
            async for message in websocket:
                try:
                    await self._handle_message(client_id, message)
                except Exception as e:
                    logger.error(f"处理消息错误: {e}")
                    await self._send_error(websocket, None, -32603, f'内部错误: {e}')
        except websockets.exceptions.ConnectionClosed:
            pass
        finally:
            if client_id in self.clients:
                del self.clients[client_id]
            logger.info(f"客户端 {client_id} 已断开")

    async def _handle_message(self, client_id: int, message: str):
        """
        处理 JSON-RPC 消息。

        解析客户端发送的 JSON 消息，验证 JSON-RPC 版本，
        并将请求路由到相应的处理方法。

        参数：
            client_id: 客户端 ID
            message: 原始 JSON 字符串消息
        """
        session = self.clients.get(client_id)
        if not session:
            return

        try:
            request = json.loads(message)
        except json.JSONDecodeError:
            await self._send_error(session.websocket, None, -32700, '解析错误')
            return

        # 验证 JSON-RPC 版本
        if request.get('jsonrpc') != '2.0':
            await self._send_error(session.websocket, request.get('id'), -32600, '无效请求')
            return

        method = request.get('method')
        params = request.get('params', {})
        request_id = request.get('id')

        # 路由方法
        if method == 'subscribe':
            await self._handle_subscribe(session, params, request_id)
        elif method == 'unsubscribe':
            await self._handle_unsubscribe(session, params, request_id)
        elif method == 'ping':
            await self._handle_ping(session, request_id)
        elif method == 'get_stats':
            await self._handle_get_stats(session, request_id)
        elif method == 'emit_test_event':
            await self._handle_emit_test_event(session, params, request_id)
        else:
            await self._send_error(session.websocket, request_id, -32601, f'方法未找到: {method}')

    async def _handle_subscribe(self, session: ClientSession, params: Dict, request_id: Any):
        """
        处理订阅请求。

        根据请求参数设置客户端的事件掩码，实现选择性接收事件。

        参数：
            session: 客户端会话对象
            params: 请求参数字典（包含 event_mask）
            request_id: JSON-RPC 请求 ID
        """
        event_mask = params.get('event_mask', -1)
        session.event_mask = event_mask

        await self._send_result(session.websocket, request_id, {
            'status': 'subscribed',
            'event_mask': event_mask
        })
        logger.debug(f"客户端订阅事件掩码: {event_mask}")

    async def _handle_unsubscribe(self, session: ClientSession, params: Dict, request_id: Any):
        """
        处理取消订阅请求。

        将客户端的事件掩码设为 0，不再接收事件通知。

        参数：
            session: 客户端会话对象
            params: 请求参数字典
            request_id: JSON-RPC 请求 ID
        """
        session.event_mask = 0
        await self._send_result(session.websocket, request_id, {'status': 'unsubscribed'})

    async def _handle_ping(self, session: ClientSession, request_id: Any):
        """处理心跳请求"""
        session.last_pong = time.time()
        await self._send_result(session.websocket, request_id, 'pong')

    async def _handle_get_stats(self, session: ClientSession, request_id: Any):
        """处理统计请求"""
        stats = {
            'clients': len(self.clients),
            'engine_loaded': self.engine.lib is not None,
            'context_created': self.engine.ctx is not None,
        }
        await self._send_result(session.websocket, request_id, stats)

    async def _handle_emit_test_event(self, session: ClientSession, params: Dict, request_id: Any):
        """处理测试事件发射请求"""
        event_type = params.get('type', 0)
        description = params.get('description', '测试事件')
        step = params.get('step', 0)

        self.engine.emit_simulated_event(event_type, description, step)
        await self._send_result(session.websocket, request_id, {'status': 'emitted'})

    async def _send_result(self, websocket: WebSocketServerProtocol, request_id: Any, result: Any):
        """
        发送 JSON-RPC 成功响应。

        将请求处理结果封装为 JSON-RPC 响应格式并发送。

        参数：
            websocket: WebSocket 连接对象
            request_id: 请求 ID（用于匹配响应）
            result: 处理结果数据
        """
        response = {
            'jsonrpc': '2.0',
            'id': request_id,
            'result': result
        }
        await websocket.send(json.dumps(response, ensure_ascii=False))

    async def _send_error(self, websocket: WebSocketServerProtocol, request_id: Any, code: int, message: str):
        """
        发送 JSON-RPC 错误响应。

        将错误信息封装为 JSON-RPC 错误响应格式并发送。

        参数：
            websocket: WebSocket 连接对象
            request_id: 请求 ID（用于匹配响应）
            code: 错误码（符合 JSON-RPC 规范）
            message: 错误消息描述
        """
        response = {
            'jsonrpc': '2.0',
            'id': request_id,
            'error': {
                'code': code,
                'message': message
            }
        }
        await websocket.send(json.dumps(response, ensure_ascii=False))

    async def start(self):
        """启动服务器"""
        if not WEBSOCKETS_AVAILABLE:
            logger.error("websockets 库未安装，无法启动服务器")
            return

        self._running = True

        # 创建引擎上下文
        self.engine.create_context()

        logger.info(f"流式桥接服务器启动: ws://{self.host}:{self.port}")

        async with serve(self._handle_client, self.host, self.port):
            await asyncio.Future()  # 永久运行

    def stop(self):
        """
        停止流式桥接服务器。

        销毁引擎上下文并设置运行状态为 False。
        停止后服务器将不再接受新的连接或推送事件。
        """
        self._running = False
        self.engine.destroy_context()
        logger.info("流式桥接服务器已停止")


# ================================================================
# AI 助手代理 / AI Assistant Proxy
# ================================================================

class AIAssistantProxy:
    """
    AI 助手代理

    支持 OpenAI/DeepSeek 兼容 API 的流式响应。
    """

    def __init__(self, endpoint: str, api_key: str, model: str = 'gpt-4o'):
        self.endpoint = endpoint.rstrip('/')
        self.api_key = api_key
        self.model = model

    async def chat_stream(self, messages: List[Dict], on_chunk: Callable[[str], None]) -> str:
        """
        流式聊天请求

        Args:
            messages: 消息列表 [{"role": "user", "content": "..."}]
            on_chunk: 分块回调函数

        Returns:
            完整响应文本
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

def main():
    """
    流式桥接服务器主入口函数。

    解析命令行参数，创建引擎桥接器和服务器实例，
    并启动异步事件循环。
    """
    parser = argparse.ArgumentParser(description='Lv-00 流式桥接服务器')
    parser.add_argument('--host', default='localhost', help='监听地址 (默认: localhost)')
    parser.add_argument('--port', type=int, default=3456, help='监听端口 (默认: 3456)')
    parser.add_argument('--engine', help='C 引擎共享库路径')
    parser.add_argument('--verbose', '-v', action='store_true', help='详细日志')

    args = parser.parse_args()

    if args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)

    # 创建引擎桥接器
    engine = EngineBridge(args.engine)

    # 创建服务器
    server = StreamBridgeServer(engine, args.host, args.port)

    try:
        asyncio.run(server.start())
    except KeyboardInterrupt:
        logger.info("收到中断信号")
        server.stop()


if __name__ == '__main__':
    main()
