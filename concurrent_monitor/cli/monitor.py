"""
CLI 监控界面
=============

基于 Rich 库实现的多窗格实时监控界面。
提供进程状态显示、输出捕获和用户交互功能。
"""

from __future__ import annotations

import signal
import sys
import time
from collections import deque
from typing import Any

from ..core.engine import MonitorEngine
from ..core.models import OutputLine, ProcessStatus, ProcessInfo
from ..core.events import Event, EventType
from ..core.config import Config, ConfigManager
from ..utils.logger import get_logger
from ..utils.constants import STATUS_COLORS, STATUS_ICONS

logger = get_logger(__name__)

# Rich 库可选依赖
try:
    from rich.live import Live
    from rich.layout import Layout
    from rich.panel import Panel
    from rich.table import Table
    from rich.text import Text
    from rich.console import Console, Group
    from rich import box
    from rich.align import Align
    from rich.progress import Progress, SpinnerColumn, TextColumn
    RICH_AVAILABLE = True
except ImportError:
    RICH_AVAILABLE = False
    logger.warning("Rich 库未安装，CLI 将使用简单文本模式")


class CLIMonitor:
    """
    命令行实时监控器
    
    提供进程状态表格、输出面板和系统消息显示。
    支持 Rich TUI 和简单文本两种模式。
    
    Example:
        >>> engine = MonitorEngine()
        >>> engine.register_process("ping", "ping -n 5 127.0.0.1")
        >>> 
        >>> monitor = CLIMonitor(engine)
        >>> monitor.run()
    """

    def __init__(
        self,
        engine: MonitorEngine,
        config: Config | None = None,
    ):
        """
        初始化 CLI 监控器
        
        Args:
            engine: 监控引擎实例
            config: 配置对象（None 使用默认配置）
        """
        self.engine = engine
        self.config = config or ConfigManager.get_config()
        self.cli_config = self.config.cli

        # 检查 Rich 可用性
        self._rich_available = RICH_AVAILABLE and self.cli_config.color_enabled

        # 输出缓冲
        self._output_buffers: dict[str, deque[str]] = {}
        self._system_messages: deque[str] = deque(maxlen=100)

        # Rich 组件
        if self._rich_available:
            self._console = Console()
            self._live: Live | None = None

        # 注册事件监听
        self._setup_event_handlers()

        logger.info("CLI 监控器初始化完成")

    def _setup_event_handlers(self) -> None:
        """设置事件处理器"""
        self.engine.event_bus.subscribe(EventType.OUTPUT, self._on_output)
        self.engine.event_bus.subscribe(EventType.SYSTEM, self._on_output)

    def _on_output(self, event: Event) -> None:
        """处理输出事件"""
        if not isinstance(event.data, OutputLine):
            return

        line = event.data

        if line.stream == "system":
            self._system_messages.append(f"[{line.process_id}] {line.content}")
        else:
            # 添加到进程输出缓冲
            if line.process_id not in self._output_buffers:
                max_lines = self.cli_config.max_lines_per_process
                self._output_buffers[line.process_id] = deque(maxlen=max_lines)

            stream_tag = "ERR" if line.stream == "stderr" else "OUT"
            self._output_buffers[line.process_id].append(
                f"[{stream_tag}] {line.content}"
            )

    # ========== Rich TUI 组件 ==========

    def _build_status_bar(self) -> Panel:
        """构建顶部状态栏"""
        summary = self.engine.get_summary()
        counts = summary.get("status_counts", {})
        total = summary["total"]

        parts = [f"[bold]总计: {total}[/bold]"]
        for status in ["running", "completed", "failed", "timeout", "pending"]:
            count = counts.get(status, 0)
            if count > 0:
                color = STATUS_COLORS.get(status, "white")
                icon = STATUS_ICONS.get(status, "?")
                parts.append(f"[{color}]{icon} {status}: {count}[/{color}]")

        text = Text("  │  ").join(parts)
        return Panel(
            text,
            title="[bold cyan]📊 并发监控状态[/bold cyan]",
            box=box.ROUNDED,
        )

    def _build_process_table(self) -> Table:
        """构建进程状态表格"""
        table = Table(
            title="[bold]进程列表[/bold]",
            box=box.SIMPLE,
            expand=True,
            show_header=True,
            header_style="bold",
        )

        table.add_column("ID", style="cyan", width=12, no_wrap=True)
        table.add_column("状态", width=10)
        table.add_column("命令", style="dim", width=35, no_wrap=False)
        table.add_column("输出行", justify="right", width=8)
        table.add_column("错误", justify="right", width=6)
        table.add_column("耗时", justify="right", width=10)

        for proc in self.engine.get_all_processes().values():
            color = STATUS_COLORS.get(proc.status.value, "white")
            icon = STATUS_ICONS.get(proc.status.value, "?")
            status_text = f"[{color}]{icon} {proc.status.value}[/{color}]"

            # 截断命令显示
            cmd_short = proc.command[:50]
            if len(proc.command) > 50:
                cmd_short += "..."

            # 错误数样式
            err_style = "[red]" if proc.error_count > 0 else ""
            err_text = f"{err_style}{proc.error_count}[/]" if err_style else str(proc.error_count)

            table.add_row(
                proc.process_id,
                status_text,
                cmd_short,
                str(len(proc.output_lines)),
                err_text,
                proc.duration_str,
            )

        return table

    def _build_output_panels(self) -> list[Panel]:
        """构建进程输出面板"""
        panels = []
        processes = list(self.engine.get_all_processes().values())

        # 最多显示6个面板
        for proc in processes[:6]:
            lines = list(self._output_buffers.get(proc.process_id, []))

            if not lines:
                content = Text("(等待输出...)", style="dim italic")
            else:
                # 取最近15行
                recent = lines[-15:]
                content = Text()
                for line in recent:
                    style = "red" if line.startswith("[ERR]") else ""
                    content.append(line + "\n", style=style)

            color = STATUS_COLORS.get(proc.status.value, "white")
            icon = STATUS_ICONS.get(proc.status.value, "?")
            title = f"[{color}]{icon} {proc.process_id}[/{color}]"

            panels.append(Panel(
                content,
                title=title,
                box=box.ROUNDED,
                height=10,
            ))

        return panels

    def _build_layout(self) -> Layout:
        """构建整体布局"""
        layout = Layout()
        layout.split(
            Layout(name="header", size=3),
            Layout(name="body"),
        )
        layout["body"].split_row(
            Layout(name="left", ratio=2),
            Layout(name="right", ratio=3),
        )
        return layout

    def _render(self) -> Layout:
        """渲染完整界面"""
        layout = self._build_layout()
        layout["header"].update(self._build_status_bar())
        layout["left"].update(self._build_process_table())

        # 右侧输出面板
        panels = self._build_output_panels()
        if not panels:
            layout["right"].update(Panel(
                Align.center("[dim]待进程启动后显示输出...[/dim]", vertical="middle"),
                title="输出",
                box=box.ROUNDED,
            ))
        else:
            layout["right"].update(Group(*panels))

        return layout

    # ========== 运行控制 ==========

    def run(
        self,
        process_ids: list[str] | None = None,
        timeout: float | None = None,
    ) -> None:
        """
        启动监控主循环
        
        Args:
            process_ids: 要监控的进程ID列表（None 表示全部）
            timeout: 超时时间
        """
        if not self._rich_available:
            self._run_simple(process_ids, timeout)
            return

        # 设置 Ctrl+C 处理
        def handle_interrupt(sig, frame):
            logger.info("收到中断信号，正在停止...")
            self.engine.stop_all()
            if self._console:
                self._console.print("\n[yellow]⏹ 正在停止所有进程...[/yellow]")
            sys.exit(0)

        signal.signal(signal.SIGINT, handle_interrupt)

        # 启动引擎
        self.engine.run_in_background(process_ids, timeout)

        # Rich Live 显示循环
        try:
            with Live(
                self._render(),
                console=self._console,
                refresh_per_second=self.cli_config.refresh_rate,
                screen=True,
            ) as live:
                self._live = live
                while self.engine.is_running:
                    time.sleep(1.0 / self.cli_config.refresh_rate)
                    live.update(self._render())

                # 最终渲染
                time.sleep(0.5)
                live.update(self._render())

        except Exception as e:
            logger.exception(f"TUI 运行异常: {e}")
        finally:
            self._live = None

        # 输出最终摘要
        self._print_summary()

    def _run_simple(self, process_ids: list[str] | None = None, timeout: float | None = None) -> None:
        """简单文本模式（无 Rich 依赖时使用）"""
        print("=" * 60)
        print("  多并发输出实时监控 (简易模式)")
        print("=" * 60)
        print()

        # 启动引擎
        self.engine.run_in_background(process_ids, timeout)

        # 简单轮询显示
        last_lines: dict[str, int] = {}

        try:
            while self.engine.is_running:
                for pid, proc in self.engine.get_all_processes().items():
                    lines = proc.output_lines
                    total = len(lines)

                    if pid not in last_lines:
                        last_lines[pid] = 0

                    new_count = total - last_lines[pid]
                    if new_count > 0:
                        for i in range(last_lines[pid], total):
                            print(f"[{pid}] {lines[i]}")
                        last_lines[pid] = total

                time.sleep(0.3)

        except KeyboardInterrupt:
            print("\n正在停止...")
            self.engine.stop_all()

        self._print_summary()

    def _print_summary(self) -> None:
        """打印执行摘要"""
        summary = self.engine.get_summary()

        print()
        print("=" * 60)
        print("  执行摘要")
        print("=" * 60)

        for p in summary.get("processes", []):
            icon = STATUS_ICONS.get(p["status"], "?")
            print(
                f"  {icon} {p['id']:<15} "
                f"状态={p['status']:<10} "
                f"exit_code={p.get('exit_code', '-'):<4} "
                f"输出行={p['lines']:<6} "
                f"错误={p['errors']:<4} "
                f"耗时={p['duration']}"
            )

        print("=" * 60)

        # 统计信息
        counts = summary.get("status_counts", {})
        print(f"总计: {summary.get('total', 0)} | "
              f"成功: {counts.get('completed', 0)} | "
              f"失败: {counts.get('failed', 0)} | "
              f"超时: {counts.get('timeout', 0)}")


# ========== 便捷函数 ==========

def monitor_commands(
    commands: list[tuple[str, str]],
    timeout: float | None = None,
    max_concurrency: int = 20,
    config: Config | None = None,
) -> CLIMonitor:
    """
    快速启动监控的便捷函数
    
    Args:
        commands: [(process_id, command), ...] 列表
        timeout: 单个进程超时时间
        max_concurrency: 最大并发数
        config: 配置对象
    
    Returns:
        CLIMonitor 实例（已配置但未运行）
    
    Example:
        >>> monitor = monitor_commands([
        ...     ("ping1", "ping -n 10 127.0.0.1"),
        ...     ("ping2", "ping -n 8 8.8.8.8"),
        ... ])
        >>> monitor.run()
    """
    engine = MonitorEngine(max_concurrency=max_concurrency, config=config)

    for pid, cmd in commands:
        engine.register_process(pid, cmd)

    return CLIMonitor(engine, config=config)
