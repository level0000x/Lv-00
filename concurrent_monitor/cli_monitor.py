"""
[已废弃] 此文件为旧版独立脚本，已被 concurrent_monitor/ 包结构替代。
保留仅供参考和向后兼容，新代码请使用包结构导入。
@deprecated 请使用 `from concurrent_monitor.core.engine import MonitorEngine` 等替代。
"""

"""
命令行 TUI 实时监控界面
基于 rich 库实现多窗格并发输出实时显示
"""

import time
import signal
import sys
from collections import deque
from typing import Optional

# 修复：从包结构导入，替代旧版顶层 monitor_engine 模块
from concurrent_monitor.core.engine import MonitorEngine, OutputLine, ProcessStatus, ProcessInfo

# --- Rich 导入（可选依赖）---
try:
    from rich.live import Live
    from rich.layout import Layout
    from rich.panel import Panel
    from rich.table import Table
    from rich.text import Text
    from rich.console import Console, Group
    from rich import box
    from rich.align import Align
    RICH_AVAILABLE = True
except ImportError:
    RICH_AVAILABLE = False


# 状态对应的颜色和图标
STATUS_STYLE = {
    "pending":   ("dim",        "○"),
    "running":   ("yellow",     "◉"),
    "completed": ("green",      "✔"),
    "failed":    ("red",        "✘"),
    "timeout":   ("magenta",    "⏱"),
}


class CLIMonitor:
    """
    命令行实时多窗格监控器
    
    使用方式:
        engine = MonitorEngine()
        engine.register_process("ping", "ping -n 5 127.0.0.1")
        engine.register_process("dir", "dir /s /b C:\\")
        
        monitor = CLIMonitor(engine)
        monitor.run()
    """

    def __init__(
        self,
        engine: MonitorEngine,
        max_lines_per_process: int = 200,
        refresh_per_second: float = 10,
    ):
        if not RICH_AVAILABLE:
            print("[ERROR] 需要安装 rich 库: pip install rich")
            print("[FALLBACK] 使用简单文本模式...")
            self._rich = False
        else:
            self._rich = True

        self.engine = engine
        self.max_lines = max_lines_per_process
        self.refresh_rate = refresh_per_second

        # 每个进程保留最近 N 行的输出缓冲
        self._output_buffers: dict[str, deque] = {}
        # 系统消息记录
        self._system_messages: deque = deque(maxlen=100)

        # 注册输出回调
        self.engine.subscribe(self._on_output)

        if self._rich:
            self._console = Console()

    def _on_output(self, line: OutputLine):
        """接收引擎的输出事件"""
        if line.stream == "system":
            self._system_messages.append(
                f"[{line.process_id}] {line.content}"
            )
        else:
            if line.process_id not in self._output_buffers:
                self._output_buffers[line.process_id] = deque(maxlen=self.max_lines)
            stream_tag = "ERR" if line.stream == "stderr" else "OUT"
            self._output_buffers[line.process_id].append(
                f"[{stream_tag}] {line.content}"
            )

    # ========== Rich TUI ==========

    def _build_status_bar(self) -> Panel:
        """构建顶部状态栏"""
        summary = self.engine.get_summary()
        counts = summary.get("status_counts", {})
        total = summary["total"]

        parts = [f"[bold]总计: {total}[/bold]"]
        for status, (style_char, icon) in STATUS_STYLE.items():
            count = counts.get(status, 0)
            if count > 0:
                parts.append(f"[{style_char}]{icon} {status}: {count}[/{style_char}]")

        text = Text("  │  ").join(parts)
        return Panel(text, title="[bold cyan]📊 并发监控状态[/bold cyan]", box=box.ROUNDED)

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
        table.add_column("耗时", justify="right", width=8)

        for proc in self.engine.get_all_processes().values():
            style_char, icon = STATUS_STYLE.get(proc.status.value, ("white", "?"))
            status_text = f"[{style_char}]{icon} {proc.status.value}[/{style_char}]"
            cmd_short = proc.command[:50] + ("..." if len(proc.command) > 50 else "")
            duration = (
                f"{proc.end_time - proc.start_time:.1f}s"
                if proc.start_time and proc.end_time
                else ("..." if proc.status == ProcessStatus.RUNNING else "-")
            )
            err_style = "[red]" if proc.error_count > 0 else ""
            table.add_row(
                proc.process_id,
                status_text,
                cmd_short,
                str(len(proc.output_lines)),
                f"{err_style}{proc.error_count}[/]" if err_style else str(proc.error_count),
                duration,
            )
        return table

    def _build_output_panels(self) -> list[Panel]:
        """为每个进程构建输出面板"""
        panels = []
        processes = list(self.engine.get_all_processes().values())

        for proc in processes:
            lines = list(self._output_buffers.get(proc.process_id, []))
            if not lines:
                content = Text("(等待输出...)", style="dim italic")
            else:
                # 取最近 15 行
                recent = lines[-15:]
                content = Text()
                for i, line in enumerate(recent):
                    style = ""
                    if line.startswith("[ERR]"):
                        style = "red"
                    content.append(line + "\n", style=style)

            style_char, icon = STATUS_STYLE.get(proc.status.value, ("white", "?"))
            title = f"[{style_char}]{icon} {proc.process_id}[/{style_char}]"
            panels.append(Panel(content, title=title, box=box.ROUNDED, height=10))

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

        # 右侧：所有进程的输出面板纵向排列
        panels = self._build_output_panels()
        if not panels:
            layout["right"].update(
                Panel(
                    Align.center("[dim]待进程启动后显示输出...[/dim]", vertical="middle"),
                    title="输出",
                    box=box.ROUNDED,
                )
            )
        else:
            # 限制最多显示的面板数
            right_layout = Layout()
            # 把所有 panel 分成上下排列
            right_panels = []
            for i, p in enumerate(panels[:6]):  # 最多显示6个
                right_panels.append(p)

            if right_panels:
                # 简化：用 Group 组合所有面板
                right_layout.update(Group(*right_panels))

            layout["right"].update(right_layout)

        return layout

    def run(
        self,
        process_ids: Optional[list[str]] = None,
        timeout: Optional[float] = None,
    ):
        """启动监控主循环"""
        if not self._rich:
            self._run_simple(process_ids, timeout)
            return

        # 注册 Ctrl+C 处理
        def handle_interrupt(sig, frame):
            self.engine.stop_all()
            self._console.print("\n[yellow]⏹ 正在停止所有进程...[/yellow]")
            sys.exit(0)

        signal.signal(signal.SIGINT, handle_interrupt)

        # 在后台启动引擎
        self.engine.run_in_background(process_ids, timeout)

        # Rich Live 显示循环
        with Live(
            self._render(),
            console=self._console,
            refresh_per_second=self.refresh_rate,
            screen=True,
        ) as live:
            while self.engine._running:
                time.sleep(1.0 / self.refresh_rate)
                live.update(self._render())

            # 最终渲染
            time.sleep(0.5)
            live.update(self._render())

        # 输出最终摘要
        self._print_summary()

    def _run_simple(self, process_ids=None, timeout=None):
        """简单文本模式（无 rich 依赖时使用）"""
        print("=" * 60)
        print("  多并发输出实时监控 (简易模式)")
        print("=" * 60)
        print()

        self.engine.run_in_background(process_ids, timeout)

        # 简单轮询显示
        last_lines = {}
        while self.engine._running:
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

        self._print_summary()

    def _print_summary(self):
        """打印执行摘要"""
        summary = self.engine.get_summary()
        print()
        print("=" * 60)
        print("  执行摘要")
        print("=" * 60)
        for p in summary["processes"]:
            status_icon = STATUS_STYLE.get(p["status"], ("white", "?"))[1]
            print(
                f"  {status_icon} {p['id']:<15} "
                f"状态={p['status']:<10} "
                f"exit_code={p['exit_code']:<4} "
                f"输出行={p['lines']:<6} "
                f"错误={p['errors']:<4} "
                f"耗时={p['duration']}"
            )
        print("=" * 60)


# ========== 便捷函数 ==========

def monitor_commands(
    commands: list[tuple[str, str]],
    timeout: Optional[float] = None,
    max_concurrency: int = 20,
) -> CLIMonitor:
    """
    快速启动监控的便捷函数
    
    Args:
        commands: [(process_id, command), ...] 列表
        timeout: 单个进程超时时间
        max_concurrency: 最大并发数
    
    Example:
        monitor_commands([
            ("ping1", "ping -n 10 127.0.0.1"),
            ("ping2", "ping -n 8 8.8.8.8"),
            ("dir", "dir /s /b C:\\Windows\\System32\\*.dll"),
        ]).run()
    """
    engine = MonitorEngine(max_concurrency=max_concurrency)
    for pid, cmd in commands:
        engine.register_process(pid, cmd)
    return CLIMonitor(engine)


# ========== 主入口 ==========

if __name__ == "__main__":
    # 演示示例
    demo_commands = [
        ("ping_local", "ping -n 6 127.0.0.1"),
        ("ping_dns", "ping -n 4 8.8.8.8"),
        ("dir_list", "powershell -Command \"Get-ChildItem -Path C:\\Windows\\System32 -Filter *.dll | Select-Object -First 20 Name,Length\""),
        ("counter", "powershell -Command \"for($i=1;$i -le 15;$i++){Write-Host \\\"计数: $i\\\"; Start-Sleep 0.3}\""),
    ]

    print("启动演示模式...")
    monitor_commands(demo_commands, timeout=30).run()
