"""
监控引擎 - 核心实现模块
========================

本模块实现了并发进程监控系统的核心引擎，提供以下功能：
  - 进程生命周期管理（注册、启动、停止、注销）
  - 实时输出捕获（stdout/stderr 异步流读取）
  - 事件驱动的输出分发机制（基于 EventBus）
  - 并发控制（信号量限制最大并发数）
  - 线程安全设计（RLock + asyncio.Lock 双重保护）

核心类：
  - ProcessExecutor: 单个进程的执行器，负责进程的启动、监控和清理
  - MonitorEngine: 监控引擎主类，管理多个并发子进程

完全重写以解决原版的线程安全、资源泄漏和错误处理问题。
"""

from __future__ import annotations

import asyncio
import logging
import shlex
import subprocess
import threading
import time
import weakref
from types import TracebackType
from typing import Any, Callable, Optional, Type

from .models import ProcessInfo, OutputLine, ProcessStatus, ProcessSummary
from .events import EventBus, Event, EventType, get_event_bus
from .config import Config, ConfigManager

logger = logging.getLogger(__name__)


class ProcessExecutor:
    """
    单个进程的执行器

    负责管理单个进程的完整生命周期，包括：
      1. 命令解析与子进程启动
      2. stdout/stderr 输出流的异步读取
      3. 超时检测与进程终止
      4. 资源清理（僵尸进程回收）

    Attributes:
        process_id: 进程唯一标识符
        command: 要执行的命令字符串
        engine: 所属的监控引擎实例
        timeout: 单个进程的超时时间（秒），None 表示不限制
        _process: 底层 asyncio 子进程对象
        _cancelled: 是否已被取消的标志
        _lock: 异步锁，防止并发执行
    """

    def __init__(
        self,
        process_id: str,
        command: str,
        engine: MonitorEngine,
        timeout: float | None = None,
    ) -> None:
        self.process_id: str = process_id
        self.command: str = command
        self.engine: MonitorEngine = engine
        self.timeout: float | None = timeout
        self._process: asyncio.subprocess.Process | None = None
        self._cancelled: bool = False
        # 延迟创建异步锁，确保在事件循环内初始化
        self._lock: asyncio.Lock | None = None

    async def execute(self) -> None:
        """执行进程的入口方法

        执行流程：
          1. 检查进程是否已注册
          2. 检查是否已被取消
          3. 标记进程为运行状态并发出开始事件
          4. 调用 _run_process 执行实际进程
          5. 无论成功或失败，发出结束事件
        """
        proc_info = self.engine.get_process(self.process_id)
        if proc_info is None:
            logger.error(f"进程未注册: {self.process_id}")
            return

        if self._lock is None:
            self._lock = asyncio.Lock()
        async with self._lock:
            # 检查是否已被外部取消
            if self._cancelled:
                logger.debug(f"进程已被取消: {self.process_id}")
                return

            # 标记进程开始运行，并发出开始事件
            proc_info.mark_started()
            self._emit_event(EventType.PROCESS_START, {"command": self.command})

            try:
                await self._run_process(proc_info)
            except asyncio.CancelledError:
                logger.debug(f"进程执行被取消: {self.process_id}")
                await self._cleanup()
                raise
            except Exception as e:
                logger.exception(f"进程执行异常: {self.process_id}, 错误: {e}")
                proc_info.mark_failed(str(e))
                self._emit_event(EventType.ERROR, {"error": str(e)})
            finally:
                # 无论结果如何，都发出进程结束事件
                self._emit_event(EventType.PROCESS_END, {
                    "status": proc_info.status.value,
                    "exit_code": proc_info.exit_code,
                    "duration": proc_info.duration,
                })

    async def _run_process(self, proc_info: ProcessInfo) -> None:
        """运行进程的主逻辑

        步骤：
          1. 使用 shlex 解析命令字符串为参数列表
          2. 通过 asyncio.create_subprocess_exec 启动子进程
          3. 并发创建 stdout/stderr 读取任务
          4. 等待进程完成（支持超时控制）
          5. 等待所有输出读取任务完成
          6. 根据退出码更新进程状态
        """
        # 解析命令字符串为参数列表
        # [平台兼容性说明] shlex.split 使用 Unix Shell 风格的词法分析规则，
        # 在 Windows 平台上可能存在以下兼容性问题：
        #   1. Windows 路径中的反斜杠（\）会被 shlex 解释为转义字符，
        #      例如 "C:\Users\test" 中的 \U 和 \t 会被特殊处理
        #   2. Windows 原生命令（如 dir、copy）的参数格式与 Unix 不同
        #   3. 包含空格的 Windows 路径需要用双引号包裹，但 shlex 可能错误拆分
        # 如果需要在 Windows 上执行复杂命令，建议：
        #   - 使用列表形式直接传递参数（如 ["cmd", "/c", "dir", "C:\\Users"]）
        #   - 或在命令字符串中使用正斜杠（/）替代反斜杠
        #   - 对于 PowerShell 命令，建议以 ["powershell", "-Command", "..."] 形式传入
        try:
            cmd_parts: list[str] = shlex.split(self.command)
        except ValueError as e:
            raise RuntimeError(f"命令解析失败: {e}") from e

        if not cmd_parts:
            raise RuntimeError("空命令")

        # 启动子进程，分别捕获 stdout 和 stderr
        logger.debug(f"启动进程: {self.process_id}, 命令: {self.command}")
        self._emit_system_message(f"[START] {self.command}")

        try:
            self._process = await asyncio.create_subprocess_exec(
                *cmd_parts,
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE,
            )
        except FileNotFoundError as e:
            # 命令对应的可执行文件不存在
            raise RuntimeError(f"命令未找到: {cmd_parts[0]}") from e
        except PermissionError as e:
            # 没有执行权限
            raise RuntimeError(f"权限不足: {cmd_parts[0]}") from e

        # 将子进程引用注册到引擎，以便外部可以停止它
        self.engine._register_subprocess(self.process_id, self._process)

        # 并发创建 stdout 和 stderr 的异步读取任务
        stdout_task: asyncio.Task[None] = asyncio.create_task(
            self._read_stream(self._process.stdout, "stdout")
        )
        stderr_task: asyncio.Task[None] = asyncio.create_task(
            self._read_stream(self._process.stderr, "stderr")
        )

        # 等待进程完成或超时
        try:
            if self.timeout:
                # 带超时的等待
                exit_code: int = await asyncio.wait_for(
                    self._process.wait(),
                    timeout=self.timeout,
                )
            else:
                # 无超时限制，一直等待
                exit_code = await self._process.wait()

            # 等待所有输出流读取任务完成，避免丢失尾部输出
            await asyncio.gather(stdout_task, stderr_task, return_exceptions=True)

            # 根据退出码更新进程状态
            proc_info.mark_completed(exit_code)
            status_str: str = "DONE" if exit_code == 0 else "FAIL"
            self._emit_system_message(
                f"[{status_str}] exit_code={exit_code}, duration={proc_info.duration_str}"
            )

        except asyncio.TimeoutError:
            # 进程执行超时，需要终止它
            logger.warning(f"进程超时: {self.process_id}, 超时时间: {self.timeout}s")
            proc_info.mark_timeout()
            self._emit_system_message(f"[TIMEOUT] after {self.timeout}s")
            await self._kill_process()

    async def _read_stream(
        self, stream: asyncio.StreamReader | None, stream_name: str
    ) -> None:
        """异步读取进程的输出流

        逐行读取子进程的 stdout 或 stderr，将每行内容封装为 OutputLine
        对象后分发到事件总线。同时更新进程信息中的输出记录和错误计数。

        Args:
            stream: 异步流读取器，None 表示该流未被捕获
            stream_name: 流名称（"stdout" 或 "stderr"）
        """
        if stream is None:
            return

        proc_info: ProcessInfo | None = self.engine.get_process(self.process_id)
        if proc_info is None:
            return

        while True:
            try:
                # 按行读取，遇到 EOF 时返回空字节
                line_bytes: bytes = await stream.readline()
                if not line_bytes:
                    break

                # 解码字节为字符串，替换无法解码的字节，并去除行尾符
                line: str = line_bytes.decode("utf-8", errors="replace").rstrip("\r\n")
                if not line:
                    continue

                # 创建输出行对象
                output_line: OutputLine = OutputLine(
                    process_id=self.process_id,
                    stream=stream_name,
                    content=line,
                )

                # 更新进程信息：记录输出行和错误计数
                proc_info.add_output(line, is_error=(stream_name == "stderr"))

                # 限制每个进程的输出行数，超出时移除最早的记录
                max_lines: int = self.engine.config.engine.max_output_lines
                if len(proc_info.output_lines) > max_lines:
                    proc_info.output_lines.popleft()

                # 将输出行分发到事件总线
                self._emit_output(output_line)

            except asyncio.CancelledError:
                # 任务被取消时重新抛出，让上层处理
                raise
            except Exception as e:
                logger.warning(f"读取流时出错: {self.process_id}, {stream_name}, {e}")
                break

    async def _kill_process(self) -> None:
        """终止子进程

        使用跨平台方式终止进程：
          1. 首先尝试 terminate() 发送 SIGTERM（Windows 下为 TerminateProcess）
          2. 等待进程自行退出（最多等待3秒）
          3. 如果进程未退出，使用 kill() 强制终止
          4. 等待进程终止，避免产生僵尸进程
        """
        if self._process is None:
            return

        try:
            # 第一步：尝试优雅终止（发送 SIGTERM 或 TerminateProcess）
            self._process.terminate()
            try:
                await asyncio.wait_for(self._process.wait(), timeout=3.0)
                logger.debug(f"进程已优雅终止: {self.process_id}")
            except asyncio.TimeoutError:
                # 第二步：强制终止（发送 SIGKILL 或 TerminateProcess）
                logger.warning(f"进程未能优雅终止，尝试强制终止: {self.process_id}")
                try:
                    self._process.kill()
                    await asyncio.wait_for(self._process.wait(), timeout=2.0)
                except asyncio.TimeoutError:
                    logger.error(f"进程强制终止后仍未退出: {self.process_id}")
        except ProcessLookupError:
            # 进程已经终止，无需处理
            pass
        except Exception as e:
            logger.warning(f"终止进程时出错: {self.process_id}, {e}")

    async def _cleanup(self) -> None:
        """清理资源

        终止子进程并释放引用，防止资源泄漏。
        """
        if self._process is not None:
            await self._kill_process()
            self._process = None

    def cancel(self) -> None:
        """取消进程执行

        设置取消标志，执行器在下次检查时会跳过执行。
        """
        self._cancelled = True

    def _emit_output(self, line: OutputLine) -> None:
        """将输出行事件发布到事件总线

        Args:
            line: 输出行数据对象
        """
        self.engine.event_bus.publish(Event(
            type=EventType.OUTPUT,
            process_id=self.process_id,
            data=line,
        ))

    def _emit_system_message(self, content: str) -> None:
        """将系统消息发布到事件总线

        系统消息用于通知进程的启动、完成、超时等状态变化。

        Args:
            content: 系统消息内容
        """
        line: OutputLine = OutputLine(
            process_id=self.process_id,
            stream="system",
            content=content,
        )
        self.engine.event_bus.publish(Event(
            type=EventType.SYSTEM,
            process_id=self.process_id,
            data=line,
        ))

    def _emit_event(self, event_type: EventType, data: Any) -> None:
        """发布通用事件到事件总线

        Args:
            event_type: 事件类型
            data: 事件携带的数据
        """
        self.engine.event_bus.publish(Event(
            type=event_type,
            process_id=self.process_id,
            data=data,
        ))


class MonitorEngine:
    """
    监控引擎（线程安全）

    管理多个并发子进程，提供实时输出捕获和事件分发功能。
    是整个并发监控系统的核心入口类。

    主要特性：
      1. 完全线程安全的设计（RLock + asyncio.Lock）
      2. 完善的资源清理机制（上下文管理器支持）
      3. 结构化日志记录
      4. 可配置的行为（Config 对象）
      5. 事件驱动的架构（EventBus）

    Attributes:
        config: 配置对象，控制引擎行为
        event_bus: 事件总线，用于发布-订阅事件

    Example:
        >>> engine = MonitorEngine(max_concurrency=10)
        >>>
        >>> # 注册进程
        >>> engine.register_process("ping", "ping -n 5 127.0.0.1")
        >>>
        >>> # 订阅事件
        >>> def on_output(event):
        ...     print(event.data.content)
        >>> engine.subscribe(on_output)
        >>>
        >>> # 运行
        >>> await engine.run_all()
    """

    def __init__(
        self,
        max_concurrency: int | None = None,
        config: Config | None = None,
        event_bus: EventBus | None = None,
    ) -> None:
        """
        初始化监控引擎

        Args:
            max_concurrency: 最大并发进程数（覆盖配置文件中的值）
            config: 配置对象（None 使用默认配置）
            event_bus: 事件总线实例（None 使用全局事件总线）
        """
        # 加载配置：优先使用传入的配置，否则使用默认配置
        self.config: Config = config or ConfigManager.get_config()
        if max_concurrency is not None:
            self.config.engine.max_concurrency = max_concurrency

        # 初始化事件总线：优先使用传入的总线，否则使用全局单例
        self.event_bus: EventBus = event_bus or get_event_bus()

        # 进程管理字典
        self._processes: dict[str, ProcessInfo] = {}
        self._subprocesses: dict[str, asyncio.subprocess.Process] = {}
        self._executors: dict[str, ProcessExecutor] = {}

        # 并发控制信号量（延迟到首次使用时创建，避免在无事件循环时触发弃用警告）
        self._semaphore: asyncio.Semaphore | None = None
        self._max_concurrency: int = self.config.engine.max_concurrency

        # 线程安全锁：RLock 支持同一线程的可重入获取
        self._lock: threading.RLock = threading.RLock()
        # 异步锁：用于保护异步上下文中的临界区（延迟创建，避免在无事件循环时触发弃用警告）
        self._async_lock: asyncio.Lock | None = None

        # 运行状态标志
        self._running: bool = False
        self._run_task: asyncio.Task | None = None
        self._background_thread: threading.Thread | None = None

        # 输出回调列表（使用弱引用避免内存泄漏，兼容旧接口）
        self._output_callbacks: list[weakref.ref[Callable[[OutputLine], None]]] = []

        # 订阅输出事件和系统事件，转发给旧式回调
        self.event_bus.subscribe(EventType.OUTPUT, self._on_output_event)
        self.event_bus.subscribe(EventType.SYSTEM, self._on_output_event)

        logger.info(f"监控引擎初始化完成，最大并发数: {self.config.engine.max_concurrency}")

    # ========== 进程管理 ==========

    def register_process(self, process_id: str, command: str) -> ProcessInfo:
        """
        注册一个待执行的进程

        Args:
            process_id: 进程唯一标识符
            command: 要执行的命令字符串

        Returns:
            ProcessInfo: 创建的进程信息对象

        Raises:
            ValueError: 进程ID已存在时抛出
        """
        with self._lock:
            if process_id in self._processes:
                raise ValueError(f"进程ID已存在: {process_id}")

            info: ProcessInfo = ProcessInfo(process_id=process_id, command=command)
            self._processes[process_id] = info
            logger.debug(f"注册进程: {process_id}, 命令: {command}")
            return info

    def get_process(self, process_id: str) -> ProcessInfo | None:
        """
        获取指定进程的信息

        Args:
            process_id: 进程唯一标识符

        Returns:
            进程信息对象，不存在时返回 None
        """
        with self._lock:
            return self._processes.get(process_id)

    def get_all_processes(self) -> dict[str, ProcessInfo]:
        """
        获取所有已注册进程的信息（返回副本）

        Returns:
            进程ID到进程信息的字典（副本，修改不影响内部状态）
        """
        with self._lock:
            return self._processes.copy()

    def unregister_process(self, process_id: str) -> bool:
        """
        注销（移除）指定进程

        如果进程正在运行，会先尝试停止它。

        Args:
            process_id: 要注销的进程ID

        Returns:
            bool: 是否成功移除（进程不存在时返回 False）
        """
        with self._lock:
            if process_id not in self._processes:
                return False

            # 如果进程正在运行，先停止它
            if process_id in self._subprocesses:
                self.stop_process(process_id)

            del self._processes[process_id]
            self._executors.pop(process_id, None)
            logger.debug(f"注销进程: {process_id}")
            return True

    def _register_subprocess(
        self, process_id: str, subprocess: asyncio.subprocess.Process
    ) -> None:
        """
        注册子进程引用（内部方法，由 ProcessExecutor 调用）

        Args:
            process_id: 进程ID
            subprocess: asyncio 子进程对象
        """
        with self._lock:
            self._subprocesses[process_id] = subprocess

    # ========== 事件订阅 ==========

    def subscribe(self, callback: Callable[[OutputLine], None]) -> None:
        """
        订阅输出事件（兼容旧接口）

        使用弱引用管理回调，避免循环引用导致内存泄漏。
        建议新代码使用 event_bus.subscribe() 直接订阅事件。

        Args:
            callback: 回调函数，接收 OutputLine 参数
        """
        with self._lock:
            ref: weakref.ref[Callable[[OutputLine], None]] = weakref.ref(callback)
            self._output_callbacks.append(ref)

    def unsubscribe(self, callback: Callable[[OutputLine], None]) -> bool:
        """
        取消订阅输出事件

        Args:
            callback: 要移除的回调函数

        Returns:
            bool: 是否成功移除
        """
        with self._lock:
            for i, ref in enumerate(self._output_callbacks):
                if ref() is callback:
                    self._output_callbacks.pop(i)
                    return True
            return False

    def _on_output_event(self, event: Event) -> None:
        """
        处理输出事件的内部回调

        将事件总线中的 OUTPUT 和 SYSTEM 事件转发给旧式回调函数。

        Args:
            event: 事件对象
        """
        if not isinstance(event.data, OutputLine):
            return

        # 获取所有有效的弱引用回调
        with self._lock:
            callbacks: list[Callable[[OutputLine], None] | None] = [
                ref() for ref in self._output_callbacks
            ]

        # 逐个调用回调，捕获异常防止一个回调失败影响其他回调
        for callback in callbacks:
            if callback is not None:
                try:
                    callback(event.data)
                except Exception as e:
                    logger.exception(f"输出回调执行失败: {e}")

    # ========== 执行控制 ==========

    async def run_all(
        self,
        process_ids: list[str] | None = None,
        timeout: float | None = None,
    ) -> None:
        """
        并发执行所有（或指定的）进程

        使用 asyncio 信号量控制并发数，每个进程在独立的
        ProcessExecutor 中执行。

        Args:
            process_ids: 要执行的进程ID列表（None 表示执行全部已注册进程）
            timeout: 单个进程的超时时间（秒），None 使用配置中的默认值

        Raises:
            RuntimeError: 引擎已经在运行中时抛出
        """
        with self._lock:
            if self._running:
                raise RuntimeError("引擎已经在运行中")
            self._running = True

        try:
            # 确定要执行的进程列表
            targets: list[str] = process_ids or list(self._processes.keys())
            logger.info(f"开始执行 {len(targets)} 个进程")

            # 为每个待执行的进程创建执行器
            if self._async_lock is None:
                self._async_lock = asyncio.Lock()
            async with self._async_lock:
                for pid in targets:
                    if pid in self._processes:
                        proc_info: ProcessInfo = self._processes[pid]
                        if proc_info.status == ProcessStatus.PENDING:
                            executor: ProcessExecutor = ProcessExecutor(
                                process_id=pid,
                                command=proc_info.command,
                                engine=self,
                                timeout=timeout or self.config.engine.default_timeout,
                            )
                            self._executors[pid] = executor

            # 定义带信号量控制的执行包装函数
            async def run_with_semaphore(pid: str) -> None:
                """在信号量控制下执行指定进程"""
                executor = self._executors.get(pid)
                if executor is None:
                    return
                # 延迟创建信号量，确保在事件循环内
                if self._semaphore is None:
                    self._semaphore = asyncio.Semaphore(self._max_concurrency)
                async with self._semaphore:
                    await executor.execute()

            # 创建并发任务列表
            tasks: list[asyncio.Task[None]] = [
                asyncio.create_task(run_with_semaphore(pid))
                for pid in targets
                if pid in self._executors
            ]

            # 并发执行所有任务，return_exceptions=True 防止一个任务失败取消其他任务
            if tasks:
                await asyncio.gather(*tasks, return_exceptions=True)

            logger.info("所有进程执行完成")

        finally:
            with self._lock:
                self._running = False

    def run_all_sync(
        self,
        process_ids: list[str] | None = None,
        timeout: float | None = None,
    ) -> None:
        """
        同步版本的 run_all

        在内部创建新的事件循环并运行所有进程。
        适用于非异步上下文中调用。

        Args:
            process_ids: 要执行的进程ID列表
            timeout: 单个进程的超时时间
        """
        asyncio.run(self.run_all(process_ids, timeout))

    def run_in_background(
        self,
        process_ids: list[str] | None = None,
        timeout: float | None = None,
    ) -> threading.Thread:
        """
        在后台线程中运行所有进程

        创建守护线程执行异步任务，立即返回线程对象。
        适用于需要非阻塞启动的场景（如 CLI/Web 界面）。

        Args:
            process_ids: 要执行的进程ID列表
            timeout: 单个进程的超时时间

        Returns:
            threading.Thread: 后台线程对象
        """
        def runner() -> None:
            """后台线程的运行函数"""
            try:
                asyncio.run(self.run_all(process_ids, timeout))
            except Exception as e:
                logger.exception(f"后台执行失败: {e}")

        thread: threading.Thread = threading.Thread(target=runner, daemon=True)
        thread.start()
        self._background_thread = thread
        logger.info("后台执行线程已启动")
        return thread

    # ========== 进程控制 ==========

    def stop_process(self, process_id: str) -> bool:
        """
        停止指定进程

        使用跨平台方式终止进程：
          1. 首先尝试 terminate() 优雅终止
          2. 等待进程退出
          3. 如果进程未退出，使用 kill() 强制终止

        Args:
            process_id: 要停止的进程ID

        Returns:
            bool: 是否成功停止（进程不存在或已结束时返回 False）
        """
        with self._lock:
            subprocess = self._subprocesses.get(process_id)
            if subprocess is None:
                return False

            # 通知执行器取消
            executor: ProcessExecutor | None = self._executors.get(process_id)
            if executor:
                executor.cancel()

            try:
                # 第一步：尝试优雅终止
                subprocess.terminate()
                logger.info(f"已发送终止信号: {process_id}")

                # 更新进程状态为失败
                proc_info: ProcessInfo | None = self._processes.get(process_id)
                if proc_info and not proc_info.is_finished:
                    proc_info.mark_failed("手动停止")

                return True
            except Exception as e:
                logger.warning(f"停止进程失败: {process_id}, {e}")
                return False

    def stop_all(self) -> int:
        """
        停止所有正在运行的进程

        Returns:
            int: 成功停止的进程数量
        """
        with self._lock:
            pids: list[str] = list(self._subprocesses.keys())

        stopped: int = 0
        for pid in pids:
            if self.stop_process(pid):
                stopped += 1

        logger.info(f"已停止 {stopped} 个进程")
        return stopped

    def restart_process(self, process_id: str) -> bool:
        """
        重启指定进程

        先停止进程（如果正在运行），然后重新启动。

        Args:
            process_id: 要重启的进程ID

        Returns:
            bool: 是否成功重启（进程不存在时返回 False）
        """
        with self._lock:
            proc_info = self._processes.get(process_id)
            if proc_info is None:
                return False

            command = proc_info.command

        # 停止进程（如果正在运行）
        self.stop_process(process_id)

        # 重新注册并启动
        try:
            # 使用 asyncio 运行异步启动
            loop = asyncio.get_event_loop()
            if loop.is_running():
                # 如果事件循环已在运行，创建任务
                asyncio.create_task(self._run_single_async(process_id))
            else:
                # 否则直接运行
                asyncio.run(self._run_single_async(process_id))
            return True
        except Exception as e:
            logger.error(f"重启进程失败: {process_id}, {e}")
            return False

    async def _run_single_async(self, process_id: str) -> None:
        """异步启动单个进程的内部方法

        作为 restart_process 的异步桥接方法，
        在已有事件循环中通过 create_task 调用，
        或在新事件循环中通过 asyncio.run 调用。

        Args:
            process_id: 要启动的进程ID
        """
        await self.run_process(process_id)

    async def run_process(self, process_id: str) -> None:
        """运行单个进程

        为指定进程创建 ProcessExecutor 并执行。该方法可被
        restart_process 及外部调用者使用，用于启动单个进程。

        Args:
            process_id: 要运行的进程ID
        """
        proc_info = self.get_process(process_id)
        if proc_info is None:
            logger.warning("进程不存在，无法运行: %s", process_id)
            return

        # 将进程状态重置为 PENDING（如果不是已结束状态）
        if proc_info.status not in (ProcessStatus.PENDING, ProcessStatus.FAILED,
                                     ProcessStatus.COMPLETED, ProcessStatus.TIMEOUT):
            return

        proc_info.status = ProcessStatus.PENDING
        proc_info.output_lines.clear()
        proc_info.error_count = 0
        proc_info.exit_code = None
        proc_info._start_time = None
        proc_info._end_time = None

        executor = ProcessExecutor(
            process_id=process_id,
            command=proc_info.command,
            engine=self,
            timeout=self.config.engine.default_timeout,
        )

        with self._lock:
            self._executors[process_id] = executor

        if self._semaphore is None:
            self._semaphore = asyncio.Semaphore(self._max_concurrency)

        async with self._semaphore:
            await executor.execute()

    def clear_output(self, process_id: str) -> bool:
        """
        清空指定进程的输出缓冲区

        Args:
            process_id: 进程ID

        Returns:
            bool: 是否成功清空（进程不存在时返回 False）
        """
        with self._lock:
            proc_info = self._processes.get(process_id)
            if proc_info is None:
                return False

            # 清空输出行列表
            proc_info.output_lines.clear()
            logger.debug(f"已清空进程输出: {process_id}")
            return True

    # ========== 状态查询 ==========

    def get_summary(self) -> dict[str, Any]:
        """
        获取运行摘要

        返回包含所有进程状态统计和详细信息的字典。

        Returns:
            dict: 包含 total、status_counts、success_rate、processes 等字段
        """
        with self._lock:
            processes: dict[str, ProcessInfo] = dict(self._processes)

        summary: ProcessSummary = ProcessSummary.from_processes(processes)

        return {
            "total": summary.total,
            "status_counts": {
                "pending": summary.pending,
                "running": summary.running,
                "completed": summary.completed,
                "failed": summary.failed,
                "timeout": summary.timeout,
            },
            "success_rate": summary.success_rate,
            "processes": [p.to_dict() for p in processes.values()],
        }

    @property
    def is_running(self) -> bool:
        """
        检查引擎是否正在运行

        Returns:
            bool: 引擎运行状态
        """
        with self._lock:
            return self._running

    # ========== 资源清理 ==========

    def shutdown(self) -> None:
        """
        关闭引擎，清理所有资源

        执行以下操作：
          1. 停止所有正在运行的进程
          2. 取消事件总线订阅
          3. 清理所有内部引用
        """
        logger.info("正在关闭监控引擎...")

        # 停止所有进程
        self.stop_all()

        with self._lock:
            # 取消事件总线订阅
            try:
                self.event_bus.unsubscribe(EventType.OUTPUT, self._on_output_event)
                self.event_bus.unsubscribe(EventType.SYSTEM, self._on_output_event)
            except Exception as e:
                logger.warning(f"取消事件订阅失败: {e}")

            # 清理所有内部引用
            self._processes.clear()
            self._subprocesses.clear()
            self._executors.clear()
            self._output_callbacks.clear()
            self._running = False

        logger.info("监控引擎已关闭")

    def __enter__(self) -> MonitorEngine:
        """上下文管理器入口，支持 with 语法"""
        return self

    def __exit__(
        self,
        exc_type: Optional[Type[BaseException]],
        exc_val: Optional[BaseException],
        exc_tb: Optional[TracebackType],
    ) -> None:
        """
        上下文管理器出口，退出时自动清理资源
        
        Args:
            exc_type: 异常类型（无异常时为 None）
            exc_val: 异常值（无异常时为 None）
            exc_tb: 异常回溯（无异常时为 None）
        """
        self.shutdown()
