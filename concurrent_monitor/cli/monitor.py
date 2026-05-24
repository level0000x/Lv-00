"""
CLI 监控界面
=============

基于 Rich 库实现的多窗格实时监控界面。
提供进程状态显示、输出捕获和用户交互功能。
"""

from __future__ import annotations

import os as _os_module
import signal
import sys
import threading
import time
from collections import deque
from typing import Any, Optional

# 键盘输入线程的平台适配
# Windows 使用 msvcrt 读取原始键码，Unix 使用 select+termios
if _os_module.name == "nt":
    import msvcrt
else:
    import select as _select
    import termios
    import tty

from ..core.engine import MonitorEngine
from ..core.models import OutputLine, ProcessStatus, ProcessInfo
from ..core.events import Event, EventType
from ..core.config import Config, ConfigManager
from ..utils.logger import get_logger
from ..utils.constants import STATUS_COLORS, STATUS_ICONS

logger = get_logger(__name__)

# Rich 库可选依赖
# [降级处理说明] Rich 是一个可选的第三方库，用于提供美观的终端 TUI 界面。
# 如果未安装 Rich，系统不会崩溃，而是自动降级为简单文本模式（_run_simple）。
# 降级模式下功能对比：
#   - Rich 模式：彩色输出、多窗格布局、实时刷新、状态表格、输出面板
#   - 简单模式：纯文本逐行输出、无颜色、无布局、通过轮询更新
# 安装 Rich：pip install rich
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
    # Rich 未安装，CLI 将降级为简单文本模式
    # 这不会影响核心监控功能，仅影响界面显示效果
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

        # ---- 键盘交互状态 ----
        self._selected_idx: int = 0          # 选中的进程索引（0开始）
        self._filter_text: str = ""          # 过滤关键词
        self._show_help: bool = False        # 是否显示帮助面板
        self._key_queue: deque[str] = deque()  # 按键事件队列
        self._keyboard_running: bool = False # 键盘监听线程运行标志
        self._keyboard_thread: Optional[threading.Thread] = None
        self._key_lock: threading.Lock = threading.Lock()  # 按键队列锁

        # 注册事件监听
        self._setup_event_handlers()

        logger.info("CLI 监控器初始化完成")

    def _setup_event_handlers(self) -> None:
        """设置事件处理器，订阅输出和系统事件"""
        self.engine.event_bus.subscribe(EventType.OUTPUT, self._on_output)
        self.engine.event_bus.subscribe(EventType.SYSTEM, self._on_output)

    def _on_output(self, event: Event) -> None:
        """
        处理输出事件

        将进程的标准输出、标准错误和系统消息分别缓存到
        对应的缓冲区中，供界面渲染时读取。

        Args:
            event: 事件对象，其 data 属性应为 OutputLine 类型
        """
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
        """
        构建顶部状态栏

        显示进程总数及各状态（运行中、已完成、失败、超时、等待中）的数量统计。

        Returns:
            Panel: 包含状态统计信息的 Rich 面板组件
        """
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
        """
        构建进程状态表格

        列出所有已注册进程的 ID、状态、命令、输出行数、错误数和耗时。
        支持选中行高亮（反转颜色）和关键词过滤。

        Returns:
            Table: 包含所有进程状态的 Rich 表格组件
        """
        table = Table(
            title="[bold]进程列表[/bold]",
            box=box.SIMPLE,
            expand=True,
            show_header=True,
            header_style="bold",
        )

        table.add_column("", width=2, no_wrap=True)  # 选中标记列
        table.add_column("ID", style="cyan", width=12, no_wrap=True)
        table.add_column("状态", width=10)
        table.add_column("命令", style="dim", width=35, no_wrap=False)
        table.add_column("输出行", justify="right", width=8)
        table.add_column("错误", justify="right", width=6)
        table.add_column("耗时", justify="right", width=10)

        processes = list(self.engine.get_all_processes().values())

        # 应用过滤
        if self._filter_text:
            processes = [
                p for p in processes
                if self._filter_text.lower() in p.process_id.lower()
                or self._filter_text.lower() in p.command.lower()
            ]

        for idx, proc in enumerate(processes):
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

            # 选中行高亮：使用箭头标记 + 反转颜色
            if idx == self._selected_idx:
                arrow = "[bold reverse yellow]>[/bold reverse yellow]"
                row_style = "reverse"
            else:
                arrow = " "
                row_style = ""

            table.add_row(
                arrow,
                proc.process_id,
                status_text,
                cmd_short,
                str(len(proc.output_lines)),
                err_text,
                proc.duration_str,
                style=row_style,
            )

        return table

    def _build_output_panels(self) -> list[Panel]:
        """
        构建进程输出面板

        为每个进程创建独立的输出面板，显示最近的输出行。
        最多同时显示6个面板，每个面板最多显示最近15行输出。

        Returns:
            list[Panel]: 进程输出面板列表
        """
        panels = []
        all_processes = list(self.engine.get_all_processes().values())

        # 应用过滤
        if self._filter_text:
            all_processes = [
                p for p in all_processes
                if self._filter_text.lower() in p.process_id.lower()
                or self._filter_text.lower() in p.command.lower()
            ]

        # 最多显示6个面板
        for proc in all_processes[:6]:
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
        """
        构建整体布局

        创建上下分区的布局结构：顶部为状态栏，底部左右分别
        放置进程表格和输出面板。

        Returns:
            Layout: Rich 布局组件
        """
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
        """
        渲染完整界面

        组装状态栏、进程表格和输出面板到整体布局中。
        支持帮助面板覆盖、过滤提示和快捷键提示条。

        Returns:
            Layout: 渲染完成的 Rich 布局组件
        """
        layout = self._build_layout()
        layout["header"].update(self._build_status_bar())

        # 左侧：进程表格
        layout["left"].update(self._build_process_table())

        # 右侧输出面板
        panels = self._build_output_panels()
        right_components = list(panels)

        # 如果显示帮助面板，插入到右侧顶部
        if self._show_help:
            right_components.insert(0, self._build_help_panel())

        if not right_components:
            layout["right"].update(Panel(
                Align.center("[dim]待进程启动后显示输出...[/dim]", vertical="middle"),
                title="输出",
                box=box.ROUNDED,
            ))
        else:
            layout["right"].update(Group(*right_components))

        # 底部状态栏：过滤提示 + 快捷键提示
        footer_parts = []
        if self._filter_text:
            footer_parts.append(
                f"[bold yellow]过滤: '{self._filter_text}'[/bold yellow]"
            )
        footer_parts.append(
            "[dim][q]退出 [k]杀进程 [r]重启 [↑/↓]选择 [f]过滤 [h]帮助[/dim]"
        )
        footer_text = Text("  |  ").join(
            [Text.from_markup(p) for p in footer_parts]
        )
        layout["header"].update(
            Group(self._build_status_bar(), footer_text)
        )

        return layout

    # ========== 帮助面板 ==========

    def _build_help_panel(self) -> Panel:
        """
        构建键盘快捷键帮助面板

        Returns:
            Panel: 包含所有快捷键说明的面板组件
        """
        help_text = Text()
        help_lines = [
            ("[bold cyan]键盘快捷键[/bold cyan]", ""),
            ("", ""),
            ("[bold]q[/bold]", "退出监控"),
            ("[bold]k[/bold]", "杀死选中的进程"),
            ("[bold]r[/bold]", "重启选中的进程"),
            ("[bold]↑/↓[/bold]", "切换选中的进程"),
            ("[bold]f[/bold]", "输入过滤关键词"),
            ("[bold]h[/bold]", "显示/隐藏帮助面板"),
        ]
        for key, desc in help_lines:
            if key:
                help_text.append(f"  {key:<10}", style="")
                help_text.append(f"{desc}\n", style="dim")
            else:
                help_text.append(f"{desc}\n")

        return Panel(
            help_text,
            title="[bold yellow]? 帮助[/bold yellow]",
            box=box.ROUNDED,
            width=30,
        )

    # ========== 键盘输入处理 ==========

    def _start_keyboard_listener(self) -> None:
        """
        启动键盘监听后台线程

        在独立线程中持续读取键盘输入，将按键事件放入队列。
        Windows 使用 msvcrt，Unix 使用 select + termios。
        """
        if self._keyboard_running:
            return
        self._keyboard_running = True

        def _listener_nt() -> None:
            """Windows 平台键盘监听"""
            import msvcrt as _msvcrt
            while self._keyboard_running:
                try:
                    if _msvcrt.kbhit():
                        ch = _msvcrt.getch()
                        if ch in (b'\xe0', b'\x00'):
                            # 扩展键码（方向键等）
                            ch2 = _msvcrt.getch()
                            with self._key_lock:
                                if ch2 == b'H':
                                    self._key_queue.append('up')
                                elif ch2 == b'P':
                                    self._key_queue.append('down')
                        else:
                            try:
                                key = ch.decode('utf-8').lower()
                                with self._key_lock:
                                    self._key_queue.append(key)
                            except UnicodeDecodeError:
                                pass
                    else:
                        time.sleep(0.05)
                except Exception:
                    time.sleep(0.1)

        def _listener_unix() -> None:
            """Unix 平台键盘监听"""
            import termios as _termios
            import tty as _tty_mod
            import select as _sel
            fd = sys.stdin.fileno()
            old_settings = _termios.tcgetattr(fd)
            try:
                _tty_mod.setraw(fd)
                while self._keyboard_running:
                    try:
                        r, _, _ = _sel.select([sys.stdin], [], [], 0.1)
                        if r:
                            ch = sys.stdin.read(1)
                            if ch == '\x1b':
                                # ANSI 转义序列
                                seq = sys.stdin.read(2)
                                if seq == '[A':
                                    with self._key_lock:
                                        self._key_queue.append('up')
                                elif seq == '[B':
                                    with self._key_lock:
                                        self._key_queue.append('down')
                            else:
                                with self._key_lock:
                                    self._key_queue.append(ch.lower())
                    except Exception:
                        time.sleep(0.1)
            finally:
                _termios.tcsetattr(fd, _termios.TCSADRAIN, old_settings)

        target = _listener_nt if _os_module.name == "nt" else _listener_unix
        self._keyboard_thread = threading.Thread(target=target, daemon=True)
        self._keyboard_thread.start()
        logger.info("键盘监听线程已启动")

    def _stop_keyboard_listener(self) -> None:
        """停止键盘监听后台线程"""
        self._keyboard_running = False
        if self._keyboard_thread and self._keyboard_thread.is_alive():
            self._keyboard_thread.join(timeout=1.0)
        self._keyboard_thread = None

    def _handle_keypress(self, key: str) -> bool:
        """
        处理单个键盘事件

        Args:
            key: 按键标识字符串（'q', 'k', 'r', 'up', 'down', 'f', 'h'）

        Returns:
            True 表示继续运行，False 表示退出监控
        """
        processes = list(self.engine.get_all_processes().values())

        if key == 'q':
            logger.info("用户按下 'q' 键，退出监控")
            return False

        elif key == 'k':
            if processes and 0 <= self._selected_idx < len(processes):
                pid = processes[self._selected_idx].process_id
                logger.info("用户杀死进程: %s", pid)
                self.engine.kill_process(pid)

        elif key == 'r':
            if processes and 0 <= self._selected_idx < len(processes):
                pid = processes[self._selected_idx].process_id
                logger.info("用户重启进程: %s", pid)
                self.engine.restart_process(pid)

        elif key == 'up':
            if self._selected_idx > 0:
                self._selected_idx -= 1

        elif key == 'down':
            if self._selected_idx < len(processes) - 1:
                self._selected_idx += 1

        elif key == 'f':
            # 过滤输入：暂停 Live 显示以读取用户输入
            if self._live and self._rich_available:
                self._live.stop()
                try:
                    filter_input = self._console.input(
                        "[bold yellow]输入过滤关键词（Enter 清除过滤）: [/bold yellow]"
                    )
                    self._filter_text = filter_input.strip()
                    logger.info("过滤关键词已更新: '%s'", self._filter_text)
                finally:
                    self._live.start()
                    self._live.refresh()

        elif key == 'h':
            self._show_help = not self._show_help

        # 确保 selected_idx 不越界
        if processes and self._selected_idx >= len(processes):
            self._selected_idx = max(0, len(processes) - 1)

        return True

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
            self._stop_keyboard_listener()
            if self._console:
                self._console.print("\n[yellow]正在停止所有进程...[/yellow]")
            sys.exit(0)

        signal.signal(signal.SIGINT, handle_interrupt)

        # 启动键盘监听线程
        self._start_keyboard_listener()

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
                    # 处理键盘事件队列
                    with self._key_lock:
                        keys = list(self._key_queue)
                        self._key_queue.clear()

                    for key in keys:
                        if not self._handle_keypress(key):
                            self.engine.stop_all()
                            break

                    time.sleep(1.0 / self.cli_config.refresh_rate)
                    live.update(self._render())

                # 最终渲染
                time.sleep(0.5)
                live.update(self._render())

        except Exception as e:
            logger.exception(f"TUI 运行异常: {e}")
        finally:
            self._stop_keyboard_listener()
            self._live = None

        # 输出最终摘要
        self._print_summary()

    def _run_simple(self, process_ids: list[str] | None = None, timeout: float | None = None) -> None:
        """
        简单文本模式运行（无 Rich 依赖时使用）

        通过轮询方式逐行打印各进程的新输出，适用于
        未安装 Rich 库的环境。轮询间隔基于配置中的
        refresh_rate 计算，最小间隔为 0.1 秒。

        Args:
            process_ids: 要监控的进程ID列表（None 表示全部）
            timeout: 超时时间（秒）
        """
        print("=" * 60)
        print("  多并发输出实时监控 (简易模式)")
        print("=" * 60)
        print()
        print("  按键: Ctrl+C 退出")
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

                time.sleep(max(0.1, 1.0 / self.cli_config.refresh_rate))

        except KeyboardInterrupt:
            print("\n正在停止...")
            self.engine.stop_all()

        self._print_summary()

    def _print_summary(self) -> None:
        """
        打印执行摘要

        在所有进程运行结束后，输出每个进程的最终状态、
        退出码、输出行数、错误数和耗时，以及整体统计信息。
        """
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
