"""
数据模型定义模块
================

本模块定义了并发监控系统中使用的所有数据结构和枚举类型。
所有模型都支持完整的类型提示和序列化/反序列化。

核心数据结构：
  - ProcessStatus: 进程状态枚举（PENDING/RUNNING/COMPLETED/FAILED/TIMEOUT）
  - StreamType: 输出流类型枚举（STDOUT/STDERR/SYSTEM）
  - OutputLine: 单行输出数据（不可变对象，frozen dataclass）
  - ProcessInfo: 进程信息（可变对象，跟踪进程完整生命周期）
  - ProcessSummary: 进程执行摘要（汇总多个进程的执行结果）
"""

from __future__ import annotations

import time
from collections import deque
from dataclasses import dataclass, field
from enum import Enum
from typing import Any


class ProcessStatus(Enum):
    """
    进程状态枚举

    定义进程在其生命周期中可能处于的所有状态。

    状态流转规则：
        PENDING -> RUNNING -> [COMPLETED | FAILED | TIMEOUT]

    状态说明：
        PENDING:    进程已注册，等待执行
        RUNNING:    进程正在执行中
        COMPLETED:  进程成功完成（退出码为 0）
        FAILED:     进程执行失败（退出码非 0 或异常）
        TIMEOUT:    进程执行超时
    """
    PENDING = "pending"      # 等待执行
    RUNNING = "running"      # 运行中
    COMPLETED = "completed"  # 成功完成
    FAILED = "failed"        # 执行失败
    TIMEOUT = "timeout"      # 执行超时

    @property
    def is_terminal(self) -> bool:
        """检查是否为终止状态（COMPLETED/FAILED/TIMEOUT）

        Returns:
            bool: 如果是终止状态返回 True
        """
        return self in (ProcessStatus.COMPLETED, ProcessStatus.FAILED, ProcessStatus.TIMEOUT)

    @property
    def icon(self) -> str:
        """获取状态对应的图标字符（用于终端显示）

        Returns:
            str: 状态图标
        """
        icons: dict[ProcessStatus, str] = {
            ProcessStatus.PENDING: "○",
            ProcessStatus.RUNNING: "◉",
            ProcessStatus.COMPLETED: "✔",
            ProcessStatus.FAILED: "✘",
            ProcessStatus.TIMEOUT: "⏱",
        }
        return icons.get(self, "?")

    @property
    def color(self) -> str:
        """获取状态对应的颜色名称（用于终端 Rich 库着色）

        Returns:
            str: Rich 库颜色名称
        """
        colors: dict[ProcessStatus, str] = {
            ProcessStatus.PENDING: "dim",
            ProcessStatus.RUNNING: "yellow",
            ProcessStatus.COMPLETED: "green",
            ProcessStatus.FAILED: "red",
            ProcessStatus.TIMEOUT: "magenta",
        }
        return colors.get(self, "white")


class StreamType(Enum):
    """输出流类型枚举

    用于区分不同来源的输出：
        STDOUT: 标准输出流
        STDERR: 标准错误流
        SYSTEM: 系统消息（引擎内部产生的状态通知）
    """
    STDOUT = "stdout"
    STDERR = "stderr"
    SYSTEM = "system"


@dataclass(frozen=True, slots=True)
class OutputLine:
    """
    单行输出数据（不可变对象）

    表示进程产生的一行输出，使用 frozen=True 确保不可变性，
    使用 slots=True 优化内存占用。

    Attributes:
        process_id: 所属进程的唯一标识符
        stream: 输出流类型（"stdout"/"stderr"/"system"）
        content: 输出内容文本
        timestamp: 时间戳（秒级浮点数，默认为创建时间）

    Example:
        >>> line = OutputLine("proc_1", "stdout", "Hello World")
        >>> print(f"[{line.stream}] {line.content}")
    """
    process_id: str
    # 注意：此处使用 str 而非 StreamType 枚举，原因如下：
    # 1. 序列化兼容性：OutputLine 需要通过 to_dict()/from_dict() 进行 JSON 序列化，
    #    使用 str 可以直接映射为 JSON 字符串，避免枚举序列化/反序列化的额外转换
    # 2. 前端兼容性：SSE 推送到前端的事件数据中 stream 字段为纯字符串，
    #    前端 JavaScript 无需处理枚举映射
    # 3. 扩展性：未来如果需要支持自定义流类型，str 比 Enum 更灵活
    stream: str
    content: str
    timestamp: float = field(default_factory=time.time)

    def to_dict(self) -> dict[str, Any]:
        """将输出行转换为字典格式（用于 JSON 序列化）

        Returns:
            dict: 包含 process_id、stream、content、timestamp 的字典
        """
        return {
            "process_id": self.process_id,
            "stream": self.stream,
            "content": self.content,
            "timestamp": self.timestamp,
        }

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> OutputLine:
        """从字典创建 OutputLine 实例（用于反序列化）

        Args:
            data: 包含 process_id、stream、content 的字典，
                  timestamp 可选（默认为当前时间）

        Returns:
            OutputLine: 新创建的输出行实例
        """
        return cls(
            process_id=data["process_id"],
            stream=data["stream"],
            content=data["content"],
            timestamp=data.get("timestamp", time.time()),
        )


@dataclass
class ProcessInfo:
    """
    进程信息（可变对象，跟踪进程完整生命周期）

    记录进程从注册到结束的所有状态变化和输出数据。

    Attributes:
        process_id: 唯一标识符
        command: 执行的命令字符串
        status: 当前状态（默认 PENDING）
        exit_code: 退出码（None 表示进程尚未结束）
        start_time: 开始时间戳（None 表示尚未开始）
        end_time: 结束时间戳（None 表示尚未结束）
        output_lines: 输出行列表（使用 deque 实现高效的首尾操作）
        error_count: 错误行计数（stderr 输出行数）
        metadata: 额外元数据字典（存储错误信息等附加数据）

    Thread Safety:
        此类的实例不是线程安全的，外部需要保证同步访问。
        通常由 MonitorEngine 的锁机制保护。

    Example:
        >>> info = ProcessInfo(process_id="test", command="echo hello")
        >>> info.mark_started()
        >>> info.add_output("hello")
        >>> info.mark_completed(0)
        >>> print(info.status)  # ProcessStatus.COMPLETED
    """
    process_id: str
    command: str
    status: ProcessStatus = field(default=ProcessStatus.PENDING)
    exit_code: int | None = field(default=None)
    start_time: float | None = field(default=None)
    end_time: float | None = field(default=None)
    output_lines: deque[str] = field(default_factory=deque)
    error_count: int = field(default=0)
    metadata: dict[str, Any] = field(default_factory=dict)

    @property
    def duration(self) -> float | None:
        """计算执行时长（秒）

        如果进程已结束，返回 start_time 到 end_time 的差值；
        如果进程正在运行，返回 start_time 到当前时间的差值；
        如果进程尚未开始，返回 None。

        Returns:
            float | None: 执行时长（秒），未开始时返回 None
        """
        if self.start_time is None:
            return None
        end: float = self.end_time if self.end_time is not None else time.time()
        return end - self.start_time

    @property
    def duration_str(self) -> str:
        """获取格式化的时长字符串

        小于60秒显示为 "X.XXs"，大于60秒显示为 "XmXX.Xs"。

        Returns:
            str: 格式化的时长字符串，未开始时返回 "N/A"
        """
        dur: float | None = self.duration
        if dur is None:
            return "N/A"
        if dur < 60:
            return f"{dur:.2f}s"
        minutes: int = int(dur // 60)
        seconds: float = dur % 60
        return f"{minutes}m{seconds:.1f}s"

    @property
    def is_running(self) -> bool:
        """检查进程是否正在运行

        Returns:
            bool: 状态为 RUNNING 时返回 True
        """
        return self.status == ProcessStatus.RUNNING

    @property
    def is_finished(self) -> bool:
        """检查进程是否已结束（处于终止状态）

        Returns:
            bool: 状态为终止状态时返回 True
        """
        return self.status.is_terminal

    def to_dict(self) -> dict[str, Any]:
        """将进程信息转换为字典格式（用于 JSON 序列化）

        Returns:
            dict: 包含所有进程信息的字典
        """
        return {
            "process_id": self.process_id,
            "command": self.command,
            "status": self.status.value,
            "exit_code": self.exit_code,
            "start_time": self.start_time,
            "end_time": self.end_time,
            "lines": len(self.output_lines),
            "errors": self.error_count,
            "duration": self.duration_str,
            "metadata": self.metadata,
        }

    def add_output(self, content: str, is_error: bool = False) -> None:
        """
        添加一行输出记录

        Args:
            content: 输出内容文本
            is_error: 是否为错误输出（stderr），True 时递增 error_count
        """
        self.output_lines.append(content)
        if is_error:
            self.error_count += 1

    def mark_started(self) -> None:
        """标记进程为运行状态

        设置状态为 RUNNING，记录开始时间。
        """
        self.status = ProcessStatus.RUNNING
        self.start_time = time.time()

    def mark_completed(self, exit_code: int) -> None:
        """
        标记进程为完成状态

        根据退出码自动判断是成功完成还是失败：
          - 退出码 0: COMPLETED
          - 退出码非 0: FAILED

        Args:
            exit_code: 进程退出码
        """
        self.exit_code = exit_code
        self.end_time = time.time()
        self.status = ProcessStatus.COMPLETED if exit_code == 0 else ProcessStatus.FAILED

    def mark_timeout(self) -> None:
        """标记进程为超时状态

        设置退出码为 -1，记录结束时间。
        """
        self.status = ProcessStatus.TIMEOUT
        self.exit_code = -1
        self.end_time = time.time()

    def mark_failed(self, error_msg: str = "") -> None:
        """
        标记进程为失败状态

        Args:
            error_msg: 可选的错误信息，会保存到 metadata 中
        """
        self.status = ProcessStatus.FAILED
        self.exit_code = -1
        self.end_time = time.time()
        if error_msg:
            self.metadata["error_message"] = error_msg

    def reset(self) -> None:
        """
        重置进程状态，用于进程重启场景。

        将状态恢复为初始值：状态设为 PENDING，清空输出和错误计数，
        清除退出码和时间戳。保留 process_id 和 command 不变。

        注意：metadata 字段不会被清空。metadata 中存储的是进程级别的
        持久化信息（如错误消息、自定义标签等），这些信息在进程重启后
        仍然具有参考价值。如果需要完全清空 metadata，请在外部手动执行
        ``info.metadata.clear()``。
        """
        self.status = ProcessStatus.PENDING
        self.output_lines.clear()
        self.error_count = 0
        self.exit_code = None
        self.start_time = None
        self.end_time = None


@dataclass
class ProcessSummary:
    """
    进程执行摘要

    用于汇总多个进程的执行结果，提供整体统计信息。

    Attributes:
        total: 进程总数
        completed: 成功完成的进程数
        failed: 失败的进程数
        timeout: 超时的进程数
        running: 正在运行的进程数
        pending: 等待执行的进程数
    """
    total: int = 0
    completed: int = 0
    failed: int = 0
    timeout: int = 0
    running: int = 0
    pending: int = 0

    @property
    def success_rate(self) -> float:
        """计算成功率（百分比）

        Returns:
            float: 成功完成的进程占总数的百分比，总数为0时返回0.0
        """
        if self.total == 0:
            return 0.0
        return self.completed / self.total * 100

    def to_dict(self) -> dict[str, Any]:
        """将摘要转换为字典格式

        Returns:
            dict: 包含各状态计数和成功率的字典
        """
        return {
            "total": self.total,
            "completed": self.completed,
            "failed": self.failed,
            "timeout": self.timeout,
            "running": self.running,
            "pending": self.pending,
            "success_rate": f"{self.success_rate:.1f}%",
        }

    @classmethod
    def from_processes(cls, processes: dict[str, ProcessInfo]) -> ProcessSummary:
        """
        从进程字典创建摘要

        遍历所有进程，按状态分类计数。

        Args:
            processes: 进程ID到进程信息的字典

        Returns:
            ProcessSummary: 汇总后的摘要对象
        """
        summary: ProcessSummary = cls(total=len(processes))
        for proc in processes.values():
            if proc.status == ProcessStatus.COMPLETED:
                summary.completed += 1
            elif proc.status == ProcessStatus.FAILED:
                summary.failed += 1
            elif proc.status == ProcessStatus.TIMEOUT:
                summary.timeout += 1
            elif proc.status == ProcessStatus.RUNNING:
                summary.running += 1
            elif proc.status == ProcessStatus.PENDING:
                summary.pending += 1
        return summary
