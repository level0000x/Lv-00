"""
多并发输出实时监控系统 - 统一启动入口
========================================

提供命令行界面，支持 Web 仪表盘和 CLI TUI 两种模式。

用法:
    # Web 仪表盘模式 (默认)
    python -m concurrent_monitor web [--port 5800] [--host 127.0.0.1]

    # 命令行 TUI 模式
    python -m concurrent_monitor cli [--timeout 30] [--concurrency 10]

    # 从配置文件启动
    python -m concurrent_monitor run config.json

    # 快速演示
    python -m concurrent_monitor demo [web|cli]

    # 生成默认配置文件
    python -m concurrent_monitor init
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Sequence

# 添加父目录到路径
sys.path.insert(0, str(Path(__file__).parent.parent))

from concurrent_monitor import (
    MonitorEngine,
    CLIMonitor,
    WebDashboard,
    Config,
    ConfigManager,
    setup_logging,
    get_logger,
)

logger = get_logger(__name__)


def build_demo_engine(max_concurrency: int = 10) -> MonitorEngine:
    """
    构建演示用的引擎
    
    创建包含多个示例进程的引擎，用于演示系统功能。
    
    Args:
        max_concurrency: 最大并发数
    
    Returns:
        配置好的监控引擎
    """
    engine = MonitorEngine(max_concurrency=max_concurrency)

    demos = [
        ("ping_local", "ping -n 6 127.0.0.1"),
        ("ping_dns", "ping -n 4 8.8.8.8"),
        (
            "dir_list",
            'powershell -Command "Get-ChildItem -Path C:\\Windows\\System32 '
            '-Filter *.dll | Select-Object -First 20 Name,Length"',
        ),
        (
            "counter",
            'powershell -Command "for($i=1;$i -le 15;$i++){Write-Host '
            "'计数:' $i; Start-Sleep 0.3}\"",
        ),
        (
            "echo_test",
            'powershell -Command "1..8 | ForEach-Object { '
            'Write-Host \\"行 $_ - 当前时间: $(Get-Date -Format HH:mm:ss)\\"; '
            'Start-Sleep 0.5 }"',
        ),
    ]

    for pid, cmd in demos:
        engine.register_process(pid, cmd)

    return engine


def cmd_web(args: argparse.Namespace) -> None:
    """
    Web 仪表盘模式
    
    启动 Web 服务器，提供浏览器访问的监控界面。
    """
    # 初始化配置
    config = ConfigManager.initialize(args.config, apply_env=True)

    # 覆盖配置
    if args.host:
        config.web.host = args.host
    if args.port:
        config.web.port = args.port
    if args.concurrency:
        config.engine.max_concurrency = args.concurrency

    # 设置日志
    setup_logging(level=config.logging.level)

    # 创建引擎
    engine = MonitorEngine(config=config)

    logger.info("启动 Web 仪表盘模式")
    print("🌐 启动 Web 仪表盘...")
    print("   在浏览器中打开后，通过界面添加和启动进程。")

    # 启动仪表盘
    dashboard = WebDashboard(
        engine=engine,
        config=config,
        open_browser=not args.no_browser,
    )
    dashboard.run()


def cmd_cli(args: argparse.Namespace) -> None:
    """
    命令行 TUI 模式
    
    启动终端界面，提供实时监控显示。
    """
    # 初始化配置
    config = ConfigManager.initialize(args.config, apply_env=True)

    # 覆盖配置
    if args.concurrency:
        config.engine.max_concurrency = args.concurrency

    # 设置日志
    setup_logging(level=config.logging.level)

    # 创建引擎
    engine = MonitorEngine(config=config)

    # 添加命令
    if args.commands:
        for i, cmd in enumerate(args.commands):
            engine.register_process(f"cmd_{i+1}", cmd)
    else:
        # 交互式添加
        print("📝 输入要监控的命令（每行一个，空行结束）:")
        i = 1
        while True:
            try:
                line = input(f"  [{i}]> ").strip()
                if not line:
                    break
                engine.register_process(f"proc_{i}", line)
                i += 1
            except (EOFError, KeyboardInterrupt):
                print()
                break

    if len(engine.get_all_processes()) == 0:
        print("⚠️ 未添加任何进程。使用 --cmd 指定命令，或通过交互输入。")
        return

    logger.info(f"启动 CLI 模式，进程数: {len(engine.get_all_processes())}")

    # 启动监控
    monitor = CLIMonitor(engine)
    monitor.run(timeout=args.timeout)


def cmd_demo(args: argparse.Namespace) -> None:
    """
    演示模式
    
    使用预定义的示例进程展示系统功能。
    """
    # 初始化配置
    config = ConfigManager.initialize(args.config, apply_env=True)

    if args.concurrency:
        config.engine.max_concurrency = args.concurrency

    setup_logging(level=config.logging.level)

    # 创建演示引擎
    engine = build_demo_engine(args.concurrency)

    if args.mode == "web":
        logger.info("启动 Web 演示模式")
        print("🚀 启动 Web 演示模式...")

        # 先启动进程
        engine.run_in_background(timeout=30)

        # 启动仪表盘
        dashboard = WebDashboard(
            engine=engine,
            config=config,
            host=args.host,
            port=args.port,
            open_browser=not args.no_browser,
        )
        dashboard.run()
    else:
        logger.info("启动 CLI 演示模式")
        print("🚀 启动 CLI 演示模式...")

        monitor = CLIMonitor(engine)
        monitor.run(timeout=30)


def cmd_run(args: argparse.Namespace) -> None:
    """
    从配置文件运行
    
    读取 JSON 配置文件并启动相应的监控模式。
    """
    config_path = Path(args.config)
    if not config_path.exists():
        print(f"❌ 配置文件不存在: {config_path}")
        sys.exit(1)

    # 加载配置
    try:
        with open(config_path, "r", encoding="utf-8") as f:
            file_config = json.load(f)
    except json.JSONDecodeError as e:
        print(f"❌ 配置文件格式错误: {e}")
        sys.exit(1)

    # 初始化配置管理器
    config = Config.from_dict(file_config)
    config.apply_env_overrides()
    ConfigManager._instance = config
    ConfigManager._initialized = True

    setup_logging(level=config.logging.level)

    # 提取进程配置
    processes = file_config.get("processes", [])
    mode = file_config.get("mode", "cli")
    timeout = file_config.get("timeout")

    if not processes:
        print("❌ 配置文件中没有定义进程 (processes 字段)")
        sys.exit(1)

    # 创建引擎
    engine = MonitorEngine(config=config)
    for item in processes:
        pid = item["id"]
        cmd = item["command"]
        engine.register_process(pid, cmd)

    logger.info(f"从配置加载了 {len(processes)} 个进程")

    # 启动对应模式
    if mode == "web":
        print(f"🌐 从配置加载了 {len(processes)} 个进程，启动 Web 仪表盘...")
        engine.run_in_background(timeout=timeout)
        dashboard = WebDashboard(
            engine=engine,
            config=config,
            open_browser=config.web.open_browser,
        )
        dashboard.run()
    else:
        print(f"🖥️ 从配置加载了 {len(processes)} 个进程，启动 CLI 监控...")
        monitor = CLIMonitor(engine)
        monitor.run(timeout=timeout)


def cmd_init(args: argparse.Namespace) -> None:
    """
    生成默认配置文件
    
    创建一个包含默认配置的 JSON 文件。
    """
    config_path = Path(args.output)

    if config_path.exists() and not args.force:
        print(f"⚠️ 配置文件已存在: {config_path}")
        print("   使用 --force 覆盖")
        sys.exit(1)

    # 创建默认配置
    default_config = {
        "mode": "cli",
        "concurrency": 10,
        "timeout": None,
        "host": "127.0.0.1",
        "port": 5800,
        "no_browser": False,
        "processes": [
            {
                "id": "example_1",
                "command": "ping -n 5 127.0.0.1",
            },
            {
                "id": "example_2",
                "command": "echo Hello World",
            },
        ],
        "engine": {
            "max_concurrency": 10,
            "max_output_lines": 500,
        },
        "web": {
            "host": "127.0.0.1",
            "port": 5800,
            "open_browser": True,
            "max_sse_clients": 50,
        },
        "cli": {
            "refresh_rate": 10.0,
            "max_lines_per_process": 200,
        },
        "logging": {
            "level": "INFO",
            "console_enabled": True,
        },
    }

    # 保存配置
    config_path.parent.mkdir(parents=True, exist_ok=True)
    with open(config_path, "w", encoding="utf-8") as f:
        json.dump(default_config, f, indent=2, ensure_ascii=False)

    print(f"✅ 配置文件已生成: {config_path}")
    print(f"   编辑此文件并运行: python -m concurrent_monitor run {config_path}")


def create_parser() -> argparse.ArgumentParser:
    """创建命令行参数解析器"""
    parser = argparse.ArgumentParser(
        prog="concurrent_monitor",
        description="多并发输出实时监控系统",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  # Web 模式
  python -m concurrent_monitor web --port 8080

  # CLI 模式
  python -m concurrent_monitor cli --cmd "ping -n 5 127.0.0.1"

  # 演示模式
  python -m concurrent_monitor demo web

  # 从配置运行
  python -m concurrent_monitor run my_config.json

  # 生成配置文件
  python -m concurrent_monitor init

更多信息: https://github.com/lv-00/monitor
        """,
    )

    parser.add_argument(
        "--version",
        action="version",
        version="%(prog)s 2.0.0",
    )
    parser.add_argument(
        "-c", "--config",
        help="配置文件路径",
        default=None,
    )
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="启用详细日志输出",
    )

    subparsers = parser.add_subparsers(dest="command", help="运行模式")

    # ---- web 子命令 ----
    web_parser = subparsers.add_parser("web", help="启动 Web 仪表盘")
    web_parser.add_argument(
        "--host",
        help="监听地址 (默认: 127.0.0.1)",
    )
    web_parser.add_argument(
        "--port", type=int,
        help="监听端口 (默认: 5800)",
    )
    web_parser.add_argument(
        "--concurrency", type=int,
        help="最大并发数",
    )
    web_parser.add_argument(
        "--no-browser", action="store_true",
        help="不自动打开浏览器",
    )
    web_parser.set_defaults(func=cmd_web)

    # ---- cli 子命令 ----
    cli_parser = subparsers.add_parser("cli", help="启动命令行 TUI 监控")
    cli_parser.add_argument(
        "--cmd", dest="commands", action="append",
        help="要执行的命令 (可多次指定)",
    )
    cli_parser.add_argument(
        "--timeout", type=float,
        help="单个进程超时时间(秒)",
    )
    cli_parser.add_argument(
        "--concurrency", type=int,
        help="最大并发数",
    )
    cli_parser.set_defaults(func=cmd_cli)

    # ---- demo 子命令 ----
    demo_parser = subparsers.add_parser("demo", help="运行演示")
    demo_parser.add_argument(
        "mode", nargs="?", default="cli", choices=["cli", "web"],
        help="演示模式",
    )
    demo_parser.add_argument(
        "--host",
        help="Web 模式监听地址",
    )
    demo_parser.add_argument(
        "--port", type=int,
        help="Web 模式监听端口",
    )
    demo_parser.add_argument(
        "--concurrency", type=int, default=10,
        help="最大并发数",
    )
    demo_parser.add_argument(
        "--no-browser", action="store_true",
        help="不自动打开浏览器",
    )
    demo_parser.set_defaults(func=cmd_demo)

    # ---- run 子命令 ----
    run_parser = subparsers.add_parser("run", help="从配置文件 JSON 运行")
    run_parser.add_argument(
        "config",
        help="配置文件路径 (JSON)",
    )
    run_parser.set_defaults(func=cmd_run)

    # ---- init 子命令 ----
    init_parser = subparsers.add_parser("init", help="生成默认配置文件")
    init_parser.add_argument(
        "-o", "--output", default="monitor_config.json",
        help="输出文件路径 (默认: monitor_config.json)",
    )
    init_parser.add_argument(
        "-f", "--force", action="store_true",
        help="覆盖已存在的文件",
    )
    init_parser.set_defaults(func=cmd_init)

    return parser


def main(argv: Sequence[str] | None = None) -> int:
    """
    主入口函数
    
    Args:
        argv: 命令行参数列表
    
    Returns:
        退出码
    """
    parser = create_parser()
    args = parser.parse_args(argv)

    # 设置详细日志
    if args.verbose:
        setup_logging(level="DEBUG")
    else:
        setup_logging(level="INFO")

    # 执行命令
    if args.command is None:
        parser.print_help()
        return 0

    try:
        args.func(args)
        return 0
    except KeyboardInterrupt:
        print("\n⏹️  用户中断")
        return 130
    except Exception as e:
        logger.exception("执行失败")
        print(f"\n❌ 错误: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
