"""
[已废弃] 此文件为旧版独立脚本，已被 concurrent_monitor/ 包结构替代。
保留仅供参考和向后兼容，新代码请使用包结构导入。
@deprecated 请使用 `from concurrent_monitor.core.engine import MonitorEngine` 等替代。
"""

"""
多并发输出实时监控 - 核心引擎
支持同时启动和管理多个子进程，实时捕获 stdout/stderr 输出
"""

import asyncio
import logging
import shlex
import subprocess
import sys
import time
import threading
from dataclasses import dataclass, field
from enum import Enum
from queue import Queue
from typing import Callable, Optional

# 模块级日志记录器，用于记录回调异常等信息
_logger = logging.getLogger(__name__)


class ProcessStatus(Enum):
    PENDING = "pending"
    RUNNING = "running"
    COMPLETED = "completed"
    FAILED = "failed"
    TIMEOUT = "timeout"


@dataclass
class OutputLine:
    """单行输出数据"""
    process_id: str
    stream: str  # 'stdout' or 'stderr'
    content: str
    timestamp: float = field(default_factory=time.time)


@dataclass
class ProcessInfo:
    """进程信息"""
    process_id: str
    command: str
    status: ProcessStatus = ProcessStatus.PENDING
    exit_code: Optional[int] = None
    start_time: Optional[float] = None
    end_time: Optional[float] = None
    output_lines: list = field(default_factory=list)
    error_count: int = 0


class MonitorEngine:
    """
    核心监控引擎
    - 管理多个并发子进程
    - 实时捕获并分发输出
    - 支持回调模式供 UI 层使用
    """

    def __init__(self, max_concurrency: int = 20):
        self._processes: dict[str, ProcessInfo] = {}
        self._subprocess_refs: dict[str, asyncio.subprocess.Process] = {}
        self._output_queue: Queue = Queue()
        self._callbacks: list[Callable] = []
        self._lock = threading.Lock()
        # 保存最大并发数，semaphore 延迟到事件循环内创建（避免 Python 3.10+ 在无事件循环时的 DeprecationWarning）
        self._semaphore_value: int = max_concurrency
        self._semaphore: Optional[asyncio.Semaphore] = None
        # 使用 threading.Event 替代裸 bool，确保跨线程（asyncio/后台线程）的可见性
        # Event.set()/clear() 提供内存屏障保证，避免 GIL 依赖带来的潜在竞态
        self._running: threading.Event = threading.Event()

    def register_process(self, process_id: str, command: str) -> ProcessInfo:
        """注册一个待执行的进程"""
        info = ProcessInfo(process_id=process_id, command=command)
        self._processes[process_id] = info
        return info

    def get_process(self, process_id: str) -> Optional[ProcessInfo]:
        return self._processes.get(process_id)

    def get_all_processes(self) -> dict[str, ProcessInfo]:
        return self._processes

    def subscribe(self, callback: Callable[[OutputLine], None]):
        """订阅输出事件"""
        with self._lock:
            self._callbacks.append(callback)

    def unsubscribe(self, callback: Callable[[OutputLine], None]):
        with self._lock:
            if callback in self._callbacks:
                self._callbacks.remove(callback)

    def _emit_output(self, line: OutputLine):
        """向所有订阅者分发输出行"""
        with self._lock:
            callbacks = list(self._callbacks)
        for cb in callbacks:
            try:
                cb(line)
            except Exception:
                # 记录回调异常到日志，但不中断其他订阅者的分发
                _logger.exception("输出回调 %s 执行异常，已跳过", cb)

    async def _read_stream(
        self, process_id: str, stream: asyncio.StreamReader, stream_name: str
    ):
        """异步读取进程的输出流"""
        while True:
            try:
                line_bytes = await stream.readline()
                if not line_bytes:
                    break
                line = line_bytes.decode("utf-8", errors="replace").rstrip("\n\r")
                if line:
                    output_line = OutputLine(
                        process_id=process_id,
                        stream=stream_name,
                        content=line,
                    )
                    proc = self._processes.get(process_id)
                    if proc:
                        proc.output_lines.append(output_line.content)
                        if stream_name == "stderr":
                            proc.error_count += 1
                    self._emit_output(output_line)
            except ValueError:
                break
            except asyncio.CancelledError:
                raise
            except Exception:
                break

    async def _run_single_process(
        self, process_id: str, timeout: Optional[float] = None
    ):
        """执行单个进程"""
        proc_info = self._processes.get(process_id)
        if not proc_info:
            return

        # 惰性初始化 semaphore：确保在事件循环运行时创建，避免 DeprecationWarning
        if self._semaphore is None:
            self._semaphore = asyncio.Semaphore(self._semaphore_value)

        async with self._semaphore:
            proc_info.status = ProcessStatus.RUNNING
            proc_info.start_time = time.time()

            try:
                # 启动子进程
                self._emit_output(OutputLine(
                    process_id=process_id,
                    stream="system",
                    content=f"[START] {proc_info.command}",
                ))

                # 根据平台选择命令分词方式
                # Windows 上 shlex.split 默认使用 POSIX 模式，会错误处理反斜杠路径
                # （如 C:\Windows\System32），将反斜杠视为转义符而非路径分隔符。
                # 使用 posix=False 切换为 Windows 兼容模式：
                #   - 双引号用于参数引用
                #   - 反斜杠不是特殊字符（不会转义后续字符）
                #   - 单引号没有特殊语义（视为普通字符）
                # 回退策略：Python < 3.8 不支持 posix 参数，退化为简单空格分词。
                if sys.platform == "win32":
                    try:
                        cmd_args = shlex.split(proc_info.command, posix=False)
                    except (TypeError, ValueError) as e:
                        # posix 参数不可用时的回退方案（Python < 3.8 或解析失败）
                        _logger.warning(
                            "shlex.split(..., posix=False) 不可用 (%s)，回退到空格分词: %s",
                            e, proc_info.command
                        )
                        cmd_args = proc_info.command.split()
                else:
                    # POSIX 系统使用 shlex 进行正确的 shell 分词（处理引号、转义等）
                    cmd_args = shlex.split(proc_info.command)

                subproc = await asyncio.create_subprocess_exec(
                    *cmd_args,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                )
                self._subprocess_refs[process_id] = subproc

                # 并发读取 stdout 和 stderr
                tasks = []
                if subproc.stdout:
                    tasks.append(
                        self._read_stream(process_id, subproc.stdout, "stdout")
                    )
                if subproc.stderr:
                    tasks.append(
                        self._read_stream(process_id, subproc.stderr, "stderr")
                    )

                # 等待进程完成或超时
                try:
                    wait_task = asyncio.create_task(subproc.wait())
                    if timeout:
                        await asyncio.wait_for(wait_task, timeout=timeout)
                    else:
                        await wait_task
                    proc_info.exit_code = subproc.returncode
                except asyncio.TimeoutError:
                    proc_info.status = ProcessStatus.TIMEOUT
                    proc_info.exit_code = -1
                    try:
                        subproc.kill()
                    except Exception:
                        pass
                    self._emit_output(OutputLine(
                        process_id=process_id,
                        stream="system",
                        content=f"[TIMEOUT] after {timeout}s",
                    ))
                    return

                # 等待流读取完成
                if tasks:
                    await asyncio.gather(*tasks, return_exceptions=True)

                proc_info.end_time = time.time()
                if proc_info.exit_code == 0:
                    proc_info.status = ProcessStatus.COMPLETED
                else:
                    proc_info.status = ProcessStatus.FAILED

                self._emit_output(OutputLine(
                    process_id=process_id,
                    stream="system",
                    content=(
                        f"[{'DONE' if proc_info.exit_code == 0 else 'FAIL'}] "
                        f"exit_code={proc_info.exit_code}, "
                        f"duration={proc_info.end_time - proc_info.start_time:.2f}s"
                    ),
                ))

            except Exception as e:
                proc_info.status = ProcessStatus.FAILED
                proc_info.exit_code = -1
                proc_info.end_time = time.time()
                self._emit_output(OutputLine(
                    process_id=process_id,
                    stream="system",
                    content=f"[ERROR] {str(e)}",
                ))

    async def run_all(
        self,
        process_ids: Optional[list[str]] = None,
        timeout: Optional[float] = None,
    ):
        """
        并发执行所有（或指定的）进程
        
        Args:
            process_ids: 要执行的进程 ID 列表，None 表示全部
            timeout: 单个进程的超时时间（秒）
        """
        self._running.set()
        targets = process_ids or list(self._processes.keys())
        tasks = [
            self._run_single_process(pid, timeout=timeout)
            for pid in targets
            if pid in self._processes
        ]
        if tasks:
            await asyncio.gather(*tasks, return_exceptions=True)
        self._running.clear()

    def stop_process(self, process_id: str):
        """停止指定进程"""
        subproc = self._subprocess_refs.get(process_id)
        if subproc and subproc.returncode is None:
            try:
                subproc.kill()
            except Exception:
                pass
            proc = self._processes.get(process_id)
            if proc:
                proc.status = ProcessStatus.FAILED
                proc.exit_code = -1

    def stop_all(self):
        """停止所有正在运行的进程"""
        for pid in list(self._subprocess_refs.keys()):
            self.stop_process(pid)

    def get_summary(self) -> dict:
        """获取运行摘要"""
        total = len(self._processes)
        status_counts = {}
        for p in self._processes.values():
            status_counts[p.status.value] = status_counts.get(p.status.value, 0) + 1
        return {
            "total": total,
            "status_counts": status_counts,
            "processes": [
                {
                    "id": p.process_id,
                    "command": p.command[:80],
                    "status": p.status.value,
                    "exit_code": p.exit_code,
                    "lines": len(p.output_lines),
                    "errors": p.error_count,
                    "duration": (
                        f"{p.end_time - p.start_time:.2f}s"
                        if p.start_time and p.end_time
                        else "N/A"
                    ),
                }
                for p in self._processes.values()
            ],
        }

    # ========== 同步便捷方法（供非异步上下文调用） ==========

    def run_all_sync(
        self,
        process_ids: Optional[list[str]] = None,
        timeout: Optional[float] = None,
    ):
        """同步版本的 run_all"""
        loop = asyncio.new_event_loop()
        try:
            return loop.run_until_complete(self.run_all(process_ids, timeout))
        finally:
            loop.close()

    def run_in_background(
        self,
        process_ids: Optional[list[str]] = None,
        timeout: Optional[float] = None,
    ):
        """在后台线程中运行所有进程"""
        def _runner():
            loop = asyncio.new_event_loop()
            asyncio.set_event_loop(loop)
            try:
                loop.run_until_complete(self.run_all(process_ids, timeout))
            finally:
                loop.close()

        t = threading.Thread(target=_runner, daemon=True)
        t.start()
        return t
