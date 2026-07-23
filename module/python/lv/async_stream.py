#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Lv-00 异步流式迭代器模块

提供 Pythonic 的异步迭代器接口，用于消费引擎流式事件。

主要类：
    - AsyncStreamIterator: 异步流迭代器
    - StreamEventQueue: 事件队列（线程安全）
    - AsyncStreamContext: 异步流上下文管理器

使用示例：
    >>> from lv.async_stream import AsyncStreamIterator
    >>>
    >>> # 异步迭代事件
    >>> async for event in AsyncStreamIterator(engine):
    ...     print(f"事件: {event['type']} - {event['description']}")
    >>>
    >>> # 使用上下文管理器
    >>> async with AsyncStreamContext(engine) as stream:
    ...     async for event in stream:
    ...         if event['type'] == 'normalization_complete':
    ...             break
    >>>
    >>> # 过滤特定事件类型
    >>> async for event in AsyncStreamIterator(engine, event_types=['normalization', 'solving']):
    ...     process(event)

版本：3.3.0
作者：Lv-00 开发团队
"""
[QA] Has broad except Exception: blocks — consider narrowing to specific types.

import asyncio
import sys
import threading
import time
from collections import deque
from dataclasses import dataclass, field
from typing import (
    Optional, Callable, Any, Dict, List, Set, AsyncIterator,
    TypeVar, Generic, Awaitable, Union
)
from enum import Enum, auto
import logging

# Python 版本兼容性：asyncio.timeout() 需要 Python 3.11+
# 在 3.10 上使用 asyncio.wait_for() 作为降级方案
if sys.version_info >= (3, 11):
    _async_timeout = asyncio.timeout
else:
    import contextlib

    @contextlib.asynccontextmanager
    async def _async_timeout(seconds):
        """Python 3.10 兼容的超时上下文管理器。"""
        # 使用 asyncio.wait_for 的包装器实现超时语义
        # 注意：此实现通过 CancelledError 传播超时
        loop = asyncio.get_running_loop()
        task = asyncio.current_task()

        def _on_timeout():
            if not task.done():
                task.cancel()

        handle = loop.call_later(seconds, _on_timeout)
        try:
            yield
        except asyncio.CancelledError:
            raise asyncio.TimeoutError()
        finally:
            handle.cancel()

logger = logging.getLogger('lv.async_stream')

T = TypeVar('T')


class StreamState(Enum):
    """流状态枚举。"""
    IDLE = auto()       # 空闲，未启动
    ACTIVE = auto()     # 活跃，正在接收事件
    PAUSED = auto()     # 暂停
    CLOSED = auto()     # 已关闭


@dataclass
class StreamEvent:
    """
    流事件数据类。

    封装引擎发送的事件数据，提供类型安全的访问接口。

    属性：
        type: 事件类型标识符
        type_name: 事件类型可读名称
        timestamp_ms: 事件时间戳（毫秒）
        step: 当前步骤编号
        total_steps: 总步骤数
        node_id: 相关节点 ID
        constraint_id: 相关约束 ID
        rule_id: 相关规则 ID
        description: 事件描述
        detail: 详细信息字典
        progress: 进度百分比
        numeric_value: 数值结果
        raw: 原始事件字典
    """
    type: str
    type_name: str = ""
    timestamp_ms: int = 0
    step: int = 0
    total_steps: int = 0
    node_id: int = -1
    constraint_id: int = -1
    rule_id: int = -1
    description: str = ""
    detail: Dict[str, Any] = field(default_factory=dict)
    progress: float = 0.0
    numeric_value: float = 0.0
    raw: Dict[str, Any] = field(default_factory=dict)

    @classmethod
    def from_dict(cls, data: Dict) -> 'StreamEvent':
        """从字典创建事件实例。"""
        return cls(
            type=data.get('type', 'unknown'),
            type_name=data.get('type_name', ''),
            timestamp_ms=data.get('timestamp_ms', 0),
            step=data.get('step', data.get('step_number', 0)),
            total_steps=data.get('total_steps', 0),
            node_id=data.get('node_id', -1),
            constraint_id=data.get('constraint_id', -1),
            rule_id=data.get('rule_id', -1),
            description=data.get('description', ''),
            detail=data.get('detail_json', {}) if isinstance(data.get('detail_json'), dict)
                   else {},
            progress=data.get('progress', 0.0),
            numeric_value=data.get('numeric_value', 0.0),
            raw=data,
        )

    def is_complete(self) -> bool:
        """检查是否为完成事件。"""
        return 'complete' in self.type.lower() or 'done' in self.type.lower()

    def is_error(self) -> bool:
        """检查是否为错误事件。"""
        return 'error' in self.type.lower() or 'fail' in self.type.lower()

    def __repr__(self) -> str:
        return f"StreamEvent(type={self.type!r}, step={self.step}/{self.total_steps})"


class StreamEventQueue(Generic[T]):
    """
    线程安全的异步事件队列。

    支持多生产者、单消费者模式。生产者（如 C 回调）从任意线程
    调用 put()，消费者通过异步迭代器 get() 获取事件。

    属性：
        maxsize: 队列最大容量（0 表示无限制）
        _queue: 内部队列（deque）
        _lock: 线程锁
        _not_empty: 异步条件变量
        _closed: 队列是否已关闭
    """

    def __init__(self, maxsize: int = 0) -> None:
        """
        初始化事件队列。

        参数：
            maxsize: 队列最大容量（0 表示无限制）
        """
        self.maxsize = maxsize
        self._queue: deque = deque()
        self._lock = threading.Lock()
        self._loop: Optional[asyncio.AbstractEventLoop] = None
        self._event: Optional[asyncio.Event] = None
        self._closed = False
        self._dropped_count = 0

    def _ensure_event(self) -> None:
        """确保异步事件对象已创建。"""
        if self._event is None:
            try:
                loop = asyncio.get_running_loop()
                self._loop = loop
                self._event = asyncio.Event()
            except RuntimeError:
                # 没有运行中的事件循环，延迟创建
                pass

    def put(self, item: T) -> bool:
        """
        将事件放入队列（线程安全）。

        参数：
            item: 要放入的事件

        返回：
            bool: 成功放入返回 True，队列已满或已关闭返回 False
        """
        if self._closed:
            return False

        with self._lock:
            if self.maxsize > 0 and len(self._queue) >= self.maxsize:
                # 队列已满，丢弃最旧的事件
                self._queue.popleft()
                self._dropped_count += 1
                logger.debug(f"队列已满，丢弃旧事件，累计丢弃: {self._dropped_count}")

            self._queue.append(item)

        # 通知等待的消费者
        if self._event is not None:
            try:
                self._loop.call_soon_threadsafe(self._event.set)
            except Exception:
                pass

        return True

    def put_nowait(self, item: T) -> bool:
        """非阻塞放入事件（同 put）。"""
        return self.put(item)

    async def get(self) -> T:
        """
        异步获取事件。

        如果队列为空，则等待直到有事件可用或队列关闭。

        返回：
            T: 队列中的事件

        异常：
            asyncio.CancelledError: 迭代被取消
            StopAsyncIteration: 队列已关闭且为空
        """
        self._ensure_event()

        while True:
            with self._lock:
                if self._queue:
                    return self._queue.popleft()
                if self._closed:
                    raise StopAsyncIteration

            # 等待新事件
            if self._event:
                self._event.clear()
                await self._event.wait()
            else:
                # 回退：短暂休眠
                await asyncio.sleep(0.01)

    def get_nowait(self) -> Optional[T]:
        """
        非阻塞获取事件。

        返回：
            Optional[T]: 队列中的事件，队列为空返回 None
        """
        with self._lock:
            if self._queue:
                return self._queue.popleft()
            return None

    def close(self) -> None:
        """关闭队列，通知所有等待的消费者。"""
        with self._lock:
            self._closed = True

        if self._event is not None:
            try:
                self._loop.call_soon_threadsafe(self._event.set)
            except Exception:
                pass

    def is_closed(self) -> bool:
        """检查队列是否已关闭。"""
        return self._closed

    def qsize(self) -> int:
        """返回队列当前大小。"""
        with self._lock:
            return len(self._queue)

    def empty(self) -> bool:
        """检查队列是否为空。"""
        with self._lock:
            return len(self._queue) == 0

    def clear(self) -> int:
        """
        清空队列。

        返回：
            int: 清除的事件数量
        """
        with self._lock:
            count = len(self._queue)
            self._queue.clear()
            return count


class AsyncStreamIterator(AsyncIterator[StreamEvent]):
    """
    异步流迭代器。

    提供 Pythonic 的异步迭代接口，用于消费引擎流式事件。
    支持事件类型过滤、超时控制和自动资源管理。

    使用示例：
        >>> async for event in AsyncStreamIterator(engine):
        ...     print(event)
        >>>
        >>> # 过滤事件类型
        >>> async for event in AsyncStreamIterator(
        ...     engine,
        ...     event_types=['normalization', 'solving']
        ... ):
        ...     process(event)
        >>>
        >>> # 设置超时
        >>> async for event in AsyncStreamIterator(engine, timeout=30.0):
        ...     if event.is_complete():
        ...         break
    """

    def __init__(
        self,
        engine_bridge: Any,
        event_types: Optional[List[str]] = None,
        event_mask: int = -1,
        timeout: Optional[float] = None,
        max_queue_size: int = 1024,
        auto_start: bool = True,
    ) -> None:
        """
        初始化异步流迭代器。

        参数：
            engine_bridge: 引擎桥接器实例（EngineBridge）
            event_types: 要接收的事件类型列表（None 表示所有类型）
            event_mask: 事件掩码（-1 表示所有事件）
            timeout: 单次获取超时（秒，None 表示无限制）
            max_queue_size: 内部队列最大容量
            auto_start: 是否自动启动流
        """
        self._engine = engine_bridge
        self._event_types: Optional[Set[str]] = set(event_types) if event_types else None
        self._event_mask = event_mask
        self._timeout = timeout
        self._queue = StreamEventQueue[StreamEvent](maxsize=max_queue_size)
        self._state = StreamState.IDLE
        self._handler_registered = False
        self._iteration_count = 0

        if auto_start:
            self._start()

    def _start(self) -> None:
        """启动流迭代器，注册事件处理器。"""
        if self._handler_registered:
            return

        # 注册事件处理器
        self._engine.add_event_handler(self._on_event)
        self._handler_registered = True
        self._state = StreamState.ACTIVE
        logger.debug("异步流迭代器已启动")

    def _on_event(self, event_dict: Dict) -> None:
        """
        事件处理器回调。

        将引擎事件转换为 StreamEvent 并放入队列。
        暂停状态下的事件会被丢弃（不放入队列）。

        参数：
            event_dict: 原始事件字典
        """
        # 暂停状态下丢弃事件，避免队列堆积
        if self._state == StreamState.PAUSED:
            return

        # 过滤事件类型
        if self._event_types:
            event_type = event_dict.get('type', '')
            if event_type not in self._event_types:
                return

        # 转换并放入队列
        event = StreamEvent.from_dict(event_dict)
        self._queue.put(event)

    def __aiter__(self) -> 'AsyncStreamIterator':
        """返回异步迭代器。"""
        return self

    async def __anext__(self) -> StreamEvent:
        """
        获取下一个事件。

        返回：
            StreamEvent: 下一个流事件

        异常：
            StopAsyncIteration: 流已关闭
            asyncio.TimeoutError: 获取超时
        """
        if self._state == StreamState.CLOSED:
            raise StopAsyncIteration

        try:
            if self._timeout:
                event = await asyncio.wait_for(
                    self._queue.get(),
                    timeout=self._timeout
                )
            else:
                event = await self._queue.get()

            self._iteration_count += 1
            return event

        except Exception as e:
            logger.error(f"获取事件错误: {e}")
            raise StopAsyncIteration

    async def __aenter__(self) -> 'AsyncStreamIterator':
        """异步上下文管理器入口。"""
        self._start()
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb) -> None:
        """异步上下文管理器出口，自动关闭流。"""
        self.close()
        return None

    def close(self) -> None:
        """关闭流迭代器，释放资源。"""
        if self._state == StreamState.CLOSED:
            return

        self._state = StreamState.CLOSED
        self._queue.close()

        # 注销事件处理器
        if self._handler_registered:
            try:
                self._engine.remove_event_handler(self._on_event)
            except Exception:
                pass
            self._handler_registered = False

        logger.debug(f"异步流迭代器已关闭，共处理 {self._iteration_count} 个事件")

    def pause(self) -> None:
        """暂停接收事件。"""
        if self._state == StreamState.ACTIVE:
            self._state = StreamState.PAUSED

    def resume(self) -> None:
        """恢复接收事件。"""
        if self._state == StreamState.PAUSED:
            self._state = StreamState.ACTIVE

    @property
    def state(self) -> StreamState:
        """返回当前流状态。"""
        return self._state

    @property
    def pending_count(self) -> int:
        """返回待处理事件数量。"""
        return self._queue.qsize()

    @property
    def iteration_count(self) -> int:
        """返回已迭代的事件数量。"""
        return self._iteration_count


class AsyncStreamContext:
    """
    异步流上下文管理器。

    提供更灵活的流生命周期管理，支持手动控制启动和停止。

    使用示例：
        >>> async with AsyncStreamContext(engine) as stream:
        ...     async for event in stream:
        ...         print(event)
        ...         if event.is_complete():
        ...             break
    """

    def __init__(
        self,
        engine_bridge: Any,
        event_types: Optional[List[str]] = None,
        **kwargs
    ) -> None:
        """
        初始化异步流上下文。

        参数：
            engine_bridge: 引擎桥接器实例
            event_types: 要接收的事件类型列表
            **kwargs: 传递给 AsyncStreamIterator 的其他参数
        """
        self._iterator = AsyncStreamIterator(
            engine_bridge,
            event_types=event_types,
            auto_start=False,
            **kwargs
        )

    async def __aenter__(self) -> AsyncStreamIterator:
        """进入上下文，启动流。"""
        return self._iterator

    async def __aexit__(self, exc_type, exc_val, exc_tb) -> None:
        """退出上下文，关闭流。"""
        self._iterator.close()
        return None


class BufferedStreamCollector:
    """
    缓冲流收集器。

    收集流事件到内存缓冲区，支持按时间窗口或数量批量获取。

    使用示例：
        >>> collector = BufferedStreamCollector(engine, buffer_size=100)
        >>> async for batch in collector.batches():
        ...     print(f"收到 {len(batch)} 个事件")
        ...     for event in batch:
        ...         process(event)
    """

    def __init__(
        self,
        engine_bridge: Any,
        buffer_size: int = 100,
        time_window: float = 1.0,
        event_types: Optional[List[str]] = None,
    ) -> None:
        """
        初始化缓冲收集器。

        参数：
            engine_bridge: 引擎桥接器实例
            buffer_size: 缓冲区大小（达到此数量触发批次）
            time_window: 时间窗口（秒，超过此时间触发批次）
            event_types: 要接收的事件类型列表
        """
        self._engine = engine_bridge
        self._buffer_size = buffer_size
        self._time_window = time_window
        self._event_types = event_types
        self._buffer: List[StreamEvent] = []
        self._lock = threading.Lock()
        self._last_flush_time = time.time()
        self._handler_registered = False

    def _start(self) -> None:
        """启动收集器。"""
        if self._handler_registered:
            return

        self._engine.add_event_handler(self._on_event)
        self._handler_registered = True

    def _on_event(self, event_dict: Dict) -> None:
        """事件处理器回调。"""
        if self._event_types:
            event_type = event_dict.get('type', '')
            if event_type not in self._event_types:
                return

        event = StreamEvent.from_dict(event_dict)

        with self._lock:
            self._buffer.append(event)

    def _flush(self) -> List[StreamEvent]:
        """刷新缓冲区，返回当前批次。"""
        with self._lock:
            batch = self._buffer.copy()
            self._buffer.clear()
            self._last_flush_time = time.time()
            return batch

    async def batches(self) -> AsyncIterator[List[StreamEvent]]:
        """
        异步迭代事件批次。

        根据缓冲区大小或时间窗口触发批次返回。

        返回：
            AsyncIterator[List[StreamEvent]]: 事件批次迭代器
        """
        self._start()

        try:
            while True:
                await asyncio.sleep(0.05)  # 检查间隔

                with self._lock:
                    should_flush = (
                        len(self._buffer) >= self._buffer_size or
                        (self._buffer and
                         time.time() - self._last_flush_time >= self._time_window)
                    )

                if should_flush:
                    batch = self._flush()
                    if batch:
                        yield batch

        except asyncio.CancelledError:
            # 返回剩余事件
            batch = self._flush()
            if batch:
                yield batch
            raise

    def close(self) -> None:
        """关闭收集器。"""
        if self._handler_registered:
            try:
                self._engine.remove_event_handler(self._on_event)
            except Exception:
                pass
            self._handler_registered = False

    async def __aenter__(self) -> 'BufferedStreamCollector':
        """异步上下文管理器入口。"""
        self._start()
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb) -> None:
        """异步上下文管理器出口。"""
        self.close()
        return None


# ================================================================
# 便捷函数 / Convenience Functions
# ================================================================

async def stream_events(
    engine_bridge: Any,
    event_types: Optional[List[str]] = None,
    timeout: Optional[float] = None,
) -> AsyncIterator[StreamEvent]:
    """
    便捷函数：创建异步流迭代器。

    参数：
        engine_bridge: 引擎桥接器实例
        event_types: 要接收的事件类型列表
        timeout: 获取超时

    返回：
        AsyncIterator[StreamEvent]: 流事件迭代器

    示例：
        >>> async for event in stream_events(engine, ['normalization']):
        ...     print(event)
    """
    async with AsyncStreamContext(
        engine_bridge,
        event_types=event_types,
        timeout=timeout
    ) as stream:
        async for event in stream:
            yield event


async def collect_events(
    engine_bridge: Any,
    count: int,
    event_types: Optional[List[str]] = None,
    timeout: float = 30.0,
) -> List[StreamEvent]:
    """
    便捷函数：收集指定数量的事件。

    参数：
        engine_bridge: 引擎桥接器实例
        count: 要收集的事件数量
        event_types: 要接收的事件类型列表
        timeout: 总超时时间

    返回：
        List[StreamEvent]: 收集的事件列表

    示例：
        >>> events = await collect_events(engine, 10, timeout=5.0)
        >>> print(f"收集到 {len(events)} 个事件")
    """
    events: List[StreamEvent] = []

    async with AsyncStreamContext(
        engine_bridge,
        event_types=event_types,
        timeout=timeout
    ) as stream:
        try:
            async with _async_timeout(timeout):
                async for event in stream:
                    events.append(event)
                    if len(events) >= count:
                        break
        except asyncio.TimeoutError:
            pass

    return events


async def wait_for_event(
    engine_bridge: Any,
    event_type: str,
    timeout: float = 30.0,
) -> Optional[StreamEvent]:
    """
    便捷函数：等待特定类型的事件。

    参数：
        engine_bridge: 引擎桥接器实例
        event_type: 目标事件类型
        timeout: 等待超时

    返回：
        Optional[StreamEvent]: 匹配的事件，超时返回 None

    示例：
        >>> event = await wait_for_event(engine, 'normalization_complete', timeout=10.0)
        >>> if event:
        ...     print("归一化完成")
    """
    async with AsyncStreamContext(
        engine_bridge,
        event_types=[event_type],
        timeout=timeout
    ) as stream:
        try:
            async with _async_timeout(timeout):
                async for event in stream:
                    if event.type == event_type:
                        return event
        except asyncio.TimeoutError:
            pass

    return None


# ================================================================
# 模块导出
# ================================================================

__all__ = [
    # 核心类
    'StreamEvent',
    'StreamEventQueue',
    'AsyncStreamIterator',
    'AsyncStreamContext',
    'BufferedStreamCollector',
    # 枚举
    'StreamState',
    # 便捷函数
    'stream_events',
    'collect_events',
    'wait_for_event',
]
