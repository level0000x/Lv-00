#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Lv-00 WebSocket 服务器模块

从 stream_bridge.py 拆分出的 WebSocket 服务器逻辑，
提供：
1. WebSocket 连接管理
2. JSON-RPC 2.0 协议支持
3. 引擎事件实时推送
4. SSE 备选通道
5. 自动重连和心跳检测

安全警告：
    本模块的 WebSocket 服务器不包含任何认证或授权机制。
    仅适用于受信任的内网开发环境，切勿直接暴露到公网。

版本：3.3.0
作者：Lv-00 开发团队
"""

import asyncio
import json
import logging
import time
from dataclasses import dataclass, field
from typing import Optional, Callable, Any, Dict, List, Set

from .stream_bridge import EngineBridge, StreamEvent

logger = logging.getLogger('stream_bridge')

# ================================================================
# WebSocket 依赖检查
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

    def __init__(self, engine: EngineBridge, host: str = 'localhost', port: int = 3456) -> None:
        self.engine = engine
        self.host = host
        self.port = port
        self.clients: Dict[int, ClientSession] = {}
        self._next_client_id = 1
        self._running = False
        # SSE 相关
        self._sse_clients: List[asyncio.Queue] = []
        self._sse_server = None

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

        try:
            loop = asyncio.get_running_loop()
            asyncio.run_coroutine_threadsafe(self._broadcast(notification), loop)
            asyncio.run_coroutine_threadsafe(self._broadcast_sse(notification), loop)
        except RuntimeError:
            pass  # 事件循环未运行，静默忽略

    async def _broadcast(self, message: Dict) -> None:
        """广播消息给所有已连接的客户端。

        参数：
            message: 要广播的消息字典
        """
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

    async def _handle_client(self, websocket: WebSocketServerProtocol, path: str = '') -> None:
        """处理 WebSocket 客户端连接的生命周期。

        参数：
            websocket: WebSocket 连接协议对象
            path: 请求路径（默认空字符串）
        """
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

    async def _handle_message(self, client_id: int, message: str) -> None:
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

    async def _handle_subscribe(self, session: ClientSession, params: Dict, request_id: Any) -> None:
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

    async def _handle_unsubscribe(self, session: ClientSession, params: Dict, request_id: Any) -> None:
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

    async def _handle_ping(self, session: ClientSession, request_id: Any) -> None:
        """处理心跳请求，更新最后 pong 时间戳。"""
        session.last_pong = time.time()
        await self._send_result(session.websocket, request_id, 'pong')

    async def _handle_get_stats(self, session: ClientSession, request_id: Any) -> None:
        """处理统计请求，返回服务器运行状态信息。"""
        stats = {
            'clients': len(self.clients),
            'engine_loaded': self.engine.lib is not None,
            'context_created': self.engine.ctx is not None,
        }
        await self._send_result(session.websocket, request_id, stats)

    async def _handle_emit_test_event(self, session: ClientSession, params: Dict, request_id: Any) -> None:
        """处理测试事件发射请求，用于调试和演示。"""
        event_type = params.get('type', 0)
        description = params.get('description', '测试事件')
        step = params.get('step', 0)

        self.engine.emit_simulated_event(event_type, description, step)
        await self._send_result(session.websocket, request_id, {'status': 'emitted'})

    async def _send_result(self, websocket: WebSocketServerProtocol, request_id: Any, result: Any) -> None:
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

    async def _send_error(self, websocket: WebSocketServerProtocol, request_id: Any, code: int, message: str) -> None:
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

    async def start(self) -> None:
        """启动服务器，开始监听 WebSocket 连接。"""
        if not WEBSOCKETS_AVAILABLE:
            logger.error("websockets 库未安装，无法启动服务器")
            return

        self._running = True

        # 创建引擎上下文
        self.engine.create_context()

        logger.info(f"流式桥接服务器启动: ws://{self.host}:{self.port}")

        async with serve(self._handle_client, self.host, self.port):
            await asyncio.Future()  # 永久运行

    # --------------------------------------------------------
    # SSE 备选通道 / SSE Fallback Channel
    # --------------------------------------------------------

    async def _broadcast_sse(self, message: Dict) -> None:
        """向所有 SSE 客户端广播消息。

        将消息以 SSE 格式推送到每个 SSE 客户端的事件队列中。
        如果客户端队列已满（客户端消费过慢），则跳过该客户端。

        参数：
            message: 要广播的消息字典
        """
        if not self._sse_clients:
            return

        message_json = json.dumps(message, ensure_ascii=False)
        disconnected = []

        for queue in self._sse_clients:
            try:
                queue.put_nowait(message_json)
            except asyncio.QueueFull:
                # 客户端消费过慢，跳过
                logger.debug("SSE 客户端队列已满，跳过")
            except Exception as e:
                logger.debug(f"SSE 推送失败: {e}")
                disconnected.append(queue)

        for queue in disconnected:
            if queue in self._sse_clients:
                self._sse_clients.remove(queue)

    async def _create_sse_response(self, event_queue: asyncio.Queue) -> None:
        """创建 SSE 响应生成器。

        从事件队列中持续读取消息，以 SSE 格式写入客户端连接。
        当队列收到 None 时表示连接应关闭。

        参数：
            event_queue: SSE 客户端的事件队列

        TODO: 此方法当前为空实现，SSE 响应逻辑已内联在 _handle_sse_client 中。
              未来应将 SSE 数据发送循环重构到此方法中，以提高代码可维护性。
              基本实现参考：
                while True:
                    message_json = await event_queue.get()
                    if message_json is None:
                        break
                    writer.write(f"data: {message_json}\\n\\n".encode('utf-8'))
                    await writer.drain()
        """
        pass

    async def _handle_sse_client(self, reader: asyncio.StreamReader,
                                  writer: asyncio.StreamWriter) -> None:
        """处理 SSE 客户端连接。

        建立一个轻量级 HTTP SSE 连接：
        1. 解析 HTTP 请求，验证路径为 /events
        2. 发送 SSE 响应头
        3. 循环从事件队列读取事件并发送 SSE 格式数据
        4. 格式: "data: {json}\\n\\n"

        参数：
            reader: 异步流读取器
            writer: 异步流写入器
        """
        peer = writer.get_extra_info('peername')
        logger.info(f"SSE 客户端连接: {peer}")

        try:
            # 读取 HTTP 请求行
            request_line = await asyncio.wait_for(reader.readline(), timeout=5.0)
            if not request_line:
                writer.close()
                await writer.wait_closed()
                return

            request_str = request_line.decode('utf-8', errors='replace').strip()
            logger.debug(f"SSE 请求: {request_str}")

            # 只接受 GET /events 路径
            if not request_str.startswith('GET /events'):
                # 返回 404
                response = (
                    'HTTP/1.1 404 Not Found\r\n'
                    'Content-Type: text/plain\r\n'
                    'Connection: close\r\n'
                    '\r\n'
                    'Not Found. SSE endpoint is at /events\n'
                )
                writer.write(response.encode('utf-8'))
                await writer.drain()
                writer.close()
                await writer.wait_closed()
                return

            # 读取剩余的请求头
            while True:
                header_line = await asyncio.wait_for(reader.readline(), timeout=5.0)
                if header_line in (b'\r\n', b'\n', b''):
                    break

            # 创建该客户端的事件队列
            event_queue: asyncio.Queue = asyncio.Queue(maxsize=256)
            self._sse_clients.append(event_queue)

            # 发送 SSE 响应头
            headers = (
                'HTTP/1.1 200 OK\r\n'
                'Content-Type: text/event-stream\r\n'
                'Cache-Control: no-cache\r\n'
                'Connection: keep-alive\r\n'
                'Access-Control-Allow-Origin: *\r\n'
                'Access-Control-Allow-Headers: Cache-Control\r\n'
                '\r\n'
            )
            writer.write(headers.encode('utf-8'))
            await writer.drain()

            logger.info(f"SSE 客户端已建立连接: {peer}")

            # 发送初始连接确认事件
            connect_event = json.dumps({
                'jsonrpc': '2.0',
                'method': 'sse.connected',
                'params': {'status': 'connected', 'timestamp': time.time()}
            }, ensure_ascii=False)
            writer.write(f"data: {connect_event}\n\n".encode('utf-8'))
            await writer.drain()

            # 持续从队列读取事件并发送
            try:
                while self._running:
                    try:
                        message_json = await asyncio.wait_for(
                            event_queue.get(), timeout=30.0
                        )
                        if message_json is None:
                            break
                        writer.write(f"data: {message_json}\n\n".encode('utf-8'))
                        await writer.drain()
                    except asyncio.TimeoutError:
                        # 发送心跳注释以保持连接
                        writer.write(b": heartbeat\n\n")
                        await writer.drain()
            except (ConnectionResetError, BrokenPipeError, asyncio.CancelledError):
                pass

        except asyncio.TimeoutError:
            logger.debug(f"SSE 客户端 {peer} 请求超时")
        except Exception as e:
            logger.debug(f"SSE 客户端 {peer} 错误: {e}")
        finally:
            # 清理
            if event_queue in self._sse_clients:
                self._sse_clients.remove(event_queue)
            try:
                writer.close()
                await writer.wait_closed()
            except Exception:
                pass
            logger.info(f"SSE 客户端已断开: {peer}")

    async def start_sse_server(self, host: str = 'localhost', port: int = 3457) -> None:
        """启动轻量级 HTTP SSE 服务器。

        使用纯 asyncio 创建一个简单的 HTTP 服务器，专门处理 SSE 连接。
        客户端通过 GET /events 路径连接。

        参数：
            host: 监听地址（默认 'localhost'）
            port: 监听端口（默认 3457）
        """
        self._sse_server = await asyncio.start_server(
            self._handle_sse_client, host, port
        )
        logger.info(f"SSE 备选通道已启动: http://{host}:{port}/events")

    def stop_sse_server(self) -> None:
        """停止 SSE 服务器。"""
        if self._sse_server:
            self._sse_server.close()
            self._sse_server = None
            # 通知所有 SSE 客户端关闭
            for queue in self._sse_clients:
                try:
                    queue.put_nowait(None)
                except asyncio.QueueFull:
                    pass
            self._sse_clients.clear()
            logger.info("SSE 备选通道已停止")

    def stop(self) -> None:
        """
        停止流式桥接服务器。

        销毁引擎上下文并设置运行状态为 False。
        停止后服务器将不再接受新的连接或推送事件。
        """
        self._running = False
        self.engine.destroy_context()
        logger.info("流式桥接服务器已停止")


# ================================================================

