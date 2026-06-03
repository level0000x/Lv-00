"""
事件总线系统模块
================

本模块实现了发布-订阅模式的事件机制，用于解耦监控系统各组件间的通信。
支持同步和异步两种事件处理器，使用弱引用管理处理器避免内存泄漏。

核心组件：
  - EventType: 事件类型枚举（OUTPUT/STATUS_CHANGE/PROCESS_START 等）
  - Event: 事件对象（携带类型、进程ID、数据和时间戳）
  - EventBus: 事件总线（线程安全的发布-订阅中心）
  - get_event_bus(): 获取全局事件总线单例实例

设计要点：
  - 使用弱引用（weakref）管理处理器，避免循环引用导致内存泄漏
  - 使用 RLock 保证线程安全
  - 支持同步和异步处理器混合使用
  - 处理器被垃圾回收时自动清理
"""

from __future__ import annotations

import asyncio
import logging
import threading
import time
import weakref
from collections import defaultdict
from enum import Enum, auto
from typing import Any, Callable, Coroutine, TypeAlias

from .models import OutputLine

# 获取模块级日志记录器
logger = logging.getLogger(__name__)


class EventType(Enum):
    """事件类型枚举

    定义系统中所有可能的事件类型，使用 auto() 自动分配值。

    事件分类：
      - 输出事件: OUTPUT（进程输出行）
      - 状态事件: STATUS_CHANGE（状态变更通知）
      - 生命周期事件: PROCESS_START, PROCESS_END（进程开始/结束）
      - 系统事件: SYSTEM（系统消息）, ERROR（错误事件）
    """
    OUTPUT = auto()           # 进程输出（stdout/stderr 产生的新行）
    STATUS_CHANGE = auto()    # 状态变更（进程状态发生变化）
    PROCESS_START = auto()    # 进程开始（进程启动执行）
    PROCESS_END = auto()      # 进程结束（进程执行完毕）
    SYSTEM = auto()           # 系统消息（引擎内部通知）
    ERROR = auto()            # 错误事件（执行过程中的异常）


class Event:
    """
    事件对象

    封装事件的所有信息，包括类型、关联进程、携带数据和时间戳。
    使用 __slots__ 优化内存占用和属性访问速度。

    Attributes:
        type: 事件类型
        process_id: 关联的进程ID
        data: 事件携带的数据（可以是 OutputLine、dict 等任意类型）
        timestamp: 事件创建时间戳（秒级浮点数）
    """
    __slots__ = ["type", "process_id", "data", "timestamp"]

    def __init__(
        self,
        type: EventType,
        process_id: str,
        data: Any = None,
        timestamp: float | None = None,
    ) -> None:
        """
        初始化事件对象

        Args:
            type: 事件类型
            process_id: 关联的进程ID
            data: 事件数据（可选）
            timestamp: 时间戳（可选，默认为当前时间）
        """
        self.type: EventType = type
        self.process_id: str = process_id
        self.data: Any = data
        self.timestamp: float = timestamp if timestamp is not None else time.time()

    def __repr__(self) -> str:
        """返回事件的简短字符串表示"""
        return f"Event({self.type.name}, {self.process_id})"


# 事件处理器类型别名
# 使用 TypeAlias 进行正式的类型别名声明，提高类型检查的准确性
EventHandler: TypeAlias = Callable[[Event], None]
AsyncEventHandler: TypeAlias = Callable[[Event], Coroutine[Any, Any, None]]


class EventBus:
    """
    事件总线（线程安全）

    实现发布-订阅模式的核心组件，支持同步和异步事件处理器。
    使用弱引用管理处理器，当处理器被垃圾回收时自动清理。

    线程安全保证：
      - 所有对处理器列表的读写操作都通过 RLock 保护
      - 发布事件时先获取处理器快照，再逐个调用

    内存安全保证：
      - 使用 weakref.ref 存储处理器引用
      - 处理器被 GC 回收时自动从列表中移除

    Example:
        >>> bus = EventBus()
        >>>
        >>> # 订阅事件
        >>> def on_output(event: Event) -> None:
        ...     print(f"输出: {event.data}")
        >>> bus.subscribe(EventType.OUTPUT, on_output)
        >>>
        >>> # 发布事件
        >>> bus.publish(Event(EventType.OUTPUT, "proc_1", "Hello"))
    """

    def __init__(self) -> None:
        """初始化事件总线"""
        # 同步处理器映射: {EventType: [weakref, ...]}
        self._handlers: dict[EventType, list[weakref.ref[EventHandler]]] = defaultdict(list)
        # 异步处理器映射: {EventType: [weakref, ...]}
        self._async_handlers: dict[EventType, list[weakref.ref[AsyncEventHandler]]] = defaultdict(list)
        # 线程安全锁（可重入锁，支持同一线程嵌套获取）
        self._lock: threading.RLock = threading.RLock()
        # 事件循环引用（用于在没有运行中事件循环时调度异步处理器）
        self._loop: asyncio.AbstractEventLoop | None = None

    def subscribe(self, event_type: EventType, handler: EventHandler) -> None:
        """
        订阅事件（同步处理器）

        注册一个同步回调函数，当指定类型的事件被发布时调用。

        Args:
            event_type: 要订阅的事件类型
            handler: 事件处理函数，签名为 (Event) -> None
        """
        with self._lock:
            # 创建弱引用，并注册清理回调（当处理器被 GC 时自动移除）
            ref: weakref.ref[EventHandler] = weakref.ref(
                handler, self._make_cleanup_handler(event_type, False)
            )
            self._handlers[event_type].append(ref)
            logger.debug(f"订阅同步事件: {event_type.name}, 处理器: {handler.__name__}")

    def subscribe_async(self, event_type: EventType, handler: AsyncEventHandler) -> None:
        """
        订阅事件（异步处理器）

        注册一个异步回调函数（coroutine function），当指定类型的事件
        被发布时，会在事件循环中调度执行。

        Args:
            event_type: 要订阅的事件类型
            handler: 异步事件处理函数，签名为 async (Event) -> None
        """
        with self._lock:
            ref: weakref.ref[AsyncEventHandler] = weakref.ref(
                handler, self._make_cleanup_handler(event_type, True)
            )
            self._async_handlers[event_type].append(ref)
            logger.debug(f"订阅异步事件: {event_type.name}, 处理器: {handler.__name__}")

    def unsubscribe(self, event_type: EventType, handler: EventHandler | AsyncEventHandler) -> bool:
        """
        取消订阅事件

        从同步或异步处理器列表中移除指定的处理器。

        Args:
            event_type: 事件类型
            handler: 要移除的处理器函数

        Returns:
            bool: 是否成功移除（处理器不存在时返回 False）
        """
        with self._lock:
            # 先尝试从同步处理器列表中移除
            handlers: list[weakref.ref[EventHandler]] = self._handlers.get(event_type, [])
            for i, ref in enumerate(handlers):
                if ref() is handler:
                    handlers.pop(i)
                    logger.debug(f"取消订阅同步事件: {event_type.name}")
                    return True

            # 再尝试从异步处理器列表中移除
            async_handlers: list[weakref.ref[AsyncEventHandler]] = self._async_handlers.get(event_type, [])
            for i, ref in enumerate(async_handlers):
                if ref() is handler:
                    async_handlers.pop(i)
                    logger.debug(f"取消订阅异步事件: {event_type.name}")
                    return True

            return False

    def publish(self, event: Event) -> None:
        """
        发布事件（同步调用所有处理器，异步调度异步处理器）

        获取指定事件类型的所有有效处理器，同步处理器立即调用，
        异步处理器通过事件循环调度执行。

        Args:
            event: 要发布的事件对象
        """
        # 获取当前所有有效的处理器（同时清理已失效的弱引用）
        sync_handlers: list[EventHandler] = self._get_handlers(event.type, False)
        async_handlers: list[AsyncEventHandler] = self._get_handlers(event.type, True)

        # 立即调用所有同步处理器
        for handler in sync_handlers:
            try:
                handler(event)
            except Exception as e:
                logger.exception(f"事件处理器执行失败: {handler.__name__}, 错误: {e}")

        # 如果有异步处理器，调度它们到事件循环中执行
        if async_handlers:
            self._schedule_async_handlers(async_handlers, event)

    def publish_sync(self, event: Event) -> None:
        """
        发布事件（仅调用同步处理器，忽略异步处理器）

        用于需要确保同步执行的场景。

        Args:
            event: 要发布的事件对象
        """
        handlers: list[EventHandler] = self._get_handlers(event.type, False)
        for handler in handlers:
            try:
                handler(event)
            except Exception as e:
                logger.exception(f"事件处理器执行失败: {handler.__name__}, 错误: {e}")

    def _get_handlers(
        self, event_type: EventType, is_async: bool
    ) -> list[EventHandler | AsyncEventHandler]:
        """
        获取指定类型的所有有效处理器

        遍历弱引用列表，返回仍然存活的处理器，同时清理已失效的引用。

        Args:
            event_type: 事件类型
            is_async: 是否获取异步处理器

        Returns:
            list: 有效的处理器函数列表
        """
        handlers_dict: dict[EventType, list[weakref.ref[Any]]] = (
            self._async_handlers if is_async else self._handlers
        )
        refs: list[weakref.ref[Any]] = handlers_dict.get(event_type, [])
        valid_handlers: list[EventHandler | AsyncEventHandler] = []

        with self._lock:
            # 复制一份引用列表，避免迭代时修改
            for ref in refs[:]:
                handler: EventHandler | AsyncEventHandler | None = ref()
                if handler is not None:
                    valid_handlers.append(handler)
                else:
                    # 弱引用已失效（处理器被 GC），从列表中移除
                    refs.remove(ref)

        return valid_handlers

    def _schedule_async_handlers(
        self, handlers: list[AsyncEventHandler], event: Event
    ) -> None:
        """
        调度异步处理器执行

        如果当前有运行中的事件循环，使用 create_task 调度；
        否则使用 run_coroutine_threadsafe 调度到专用的事件循环线程。

        Args:
            handlers: 异步处理器列表
            event: 要处理的事件
        """
        try:
            # 尝试获取当前运行中的事件循环
            loop: asyncio.AbstractEventLoop = asyncio.get_running_loop()
            for handler in handlers:
                loop.create_task(self._safe_async_handler(handler, event))
        except RuntimeError:
            # 没有运行中的事件循环，使用专用循环线程
            for handler in handlers:
                asyncio.run_coroutine_threadsafe(
                    self._safe_async_handler(handler, event),
                    self._get_or_create_loop()
                )

    async def _safe_async_handler(self, handler: AsyncEventHandler, event: Event) -> None:
        """
        安全执行异步处理器

        捕获处理器中的所有异常，防止单个处理器失败影响其他处理器。

        Args:
            handler: 异步处理器函数
            event: 事件对象
        """
        try:
            await handler(event)
        except Exception as e:
            logger.exception(f"异步事件处理器执行失败: {handler.__name__}, 错误: {e}")

    def _get_or_create_loop(self) -> asyncio.AbstractEventLoop:
        """
        获取或创建专用的事件循环

        当没有运行中的事件循环时，创建一个新的事件循环并在
        守护线程中运行它，用于调度异步处理器。

        Returns:
            asyncio.AbstractEventLoop: 可用的事件循环
        """
        if self._loop is None or self._loop.is_closed():
            self._loop = asyncio.new_event_loop()
            # 在守护线程中运行事件循环，主线程退出时自动结束
            threading.Thread(target=self._loop.run_forever, daemon=True).start()
        return self._loop

    def _make_cleanup_handler(
        self, event_type: EventType, is_async: bool
    ) -> Callable[[weakref.ref[Any]], None]:
        """
        创建弱引用的清理回调函数

        当处理器被垃圾回收时，此回调会被自动调用，
        从对应的处理器列表中移除失效的弱引用。

        Args:
            event_type: 事件类型
            is_async: 是否为异步处理器

        Returns:
            Callable: 清理回调函数
        """
        def cleanup(ref: weakref.ref[Any]) -> None:
            """弱引用失效时的清理函数"""
            with self._lock:
                handlers_dict: dict[EventType, list[weakref.ref[Any]]] = (
                    self._async_handlers if is_async else self._handlers
                )
                handlers: list[weakref.ref[Any]] = handlers_dict.get(event_type, [])
                if ref in handlers:
                    handlers.remove(ref)

        return cleanup

    def clear(self) -> None:
        """清除所有事件处理器

        移除所有同步和异步处理器的订阅。
        """
        with self._lock:
            self._handlers.clear()
            self._async_handlers.clear()
            logger.info("事件总线已清空")

    def get_stats(self) -> dict[str, int]:
        """
        获取事件总线的统计信息

        Returns:
            dict: 包含 sync_handlers、async_handlers、total 的统计字典
        """
        with self._lock:
            sync_count: int = sum(len(refs) for refs in self._handlers.values())
            async_count: int = sum(len(refs) for refs in self._async_handlers.values())
            return {
                "sync_handlers": sync_count,
                "async_handlers": async_count,
                "total": sync_count + async_count,
            }


# ============================================================
# 全局事件总线单例
# ============================================================

# 全局事件总线实例（延迟初始化）
_global_event_bus: EventBus | None = None
# 全局事件总线的初始化锁（防止多线程同时创建）
_global_event_bus_lock: threading.Lock = threading.Lock()


def get_event_bus() -> EventBus:
    """
    获取全局事件总线实例（单例模式）

    使用双重检查锁定（Double-Checked Locking）确保线程安全的单例创建。
    首次调用时创建实例，后续调用返回同一实例。

    Returns:
        EventBus: 全局事件总线实例
    """
    global _global_event_bus
    # 第一次检查（无锁，快速路径）
    if _global_event_bus is None:
        # 加锁创建
        with _global_event_bus_lock:
            # 第二次检查（有锁，确保只有一个实例）
            if _global_event_bus is None:
                _global_event_bus = EventBus()
    return _global_event_bus
