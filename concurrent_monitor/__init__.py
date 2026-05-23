"""
多并发输出实时监控系统 - 统一入口包
================================================

提供完整的进程监控解决方案，支持 CLI TUI 和 Web 仪表盘两种界面模式。

主要模块:
    - core: 核心引擎和数据模型
    - cli: 命令行界面
    - web: Web 仪表盘
    - utils: 工具函数和常量
    - config: 配置管理

使用示例:
    >>> from concurrent_monitor import MonitorEngine, WebDashboard, CLIMonitor
    >>> 
    >>> # 创建引擎
    >>> engine = MonitorEngine(max_concurrency=10)
    >>> engine.register_process("ping", "ping -n 5 127.0.0.1")
    >>> 
    >>> # Web 模式
    >>> dashboard = WebDashboard(engine, port=5800)
    >>> dashboard.run()
    >>> 
    >>> # CLI 模式
    >>> monitor = CLIMonitor(engine)
    >>> monitor.run()

版本: 3.0.1
作者: Lv-00 Team
"""

__version__ = "3.0.1"
__author__ = "Lv-00 Team"

# 核心组件导出
try:
    from .core.engine import MonitorEngine
except ImportError:
    from .monitor_engine import MonitorEngine

try:
    from .core.models import ProcessInfo, OutputLine, ProcessStatus
except ImportError:
    from .monitor_engine import ProcessInfo, OutputLine, ProcessStatus

try:
    from .core.events import EventBus, EventType
except ImportError:
    EventBus = None
    EventType = None

try:
    from .core.config import Config, ConfigManager
except ImportError:
    Config = None
    ConfigManager = None

# UI 组件导出
# ================== 旧模块迁移说明 ==================
# 以下 catch-all ImportError 回退路径引用的是旧版独立脚本文件
# (cli_monitor.py / web_dashboard.py)，这些文件已被标记为 [已废弃]。
#
# 迁移计划:
#   - 当前阶段: 保留旧文件作为向后兼容层，通过 fallback 导入继续工作
#   - 未来版本 (v3.2+): 移除旧文件，仅保留包结构导入路径
#   - 旧模块路径:  concurrent_monitor.cli_monitor -> 新版: concurrent_monitor.cli.monitor
#                   concurrent_monitor.web_dashboard -> 新版: concurrent_monitor.web.dashboard
#
# 新代码请始终使用包结构路径:
#   from concurrent_monitor.cli.monitor import CLIMonitor
#   from concurrent_monitor.web.dashboard import WebDashboard
# ====================================================
try:
    from .cli.monitor import CLIMonitor, monitor_commands
except ImportError:
    from .cli_monitor import CLIMonitor, monitor_commands  # 旧版回退

try:
    from .web.dashboard import WebDashboard, launch_web_dashboard
except ImportError:
    from .web_dashboard import WebDashboard, launch_web_dashboard  # 旧版回退

# 工具函数导出
try:
    from .utils.logger import get_logger, setup_logging
except ImportError:
    get_logger = None
    setup_logging = None

try:
    from .utils.validators import validate_process_id, validate_command
except ImportError:
    validate_process_id = None
    validate_command = None

try:
    from .utils.constants import DEFAULT_CONFIG
except ImportError:
    DEFAULT_CONFIG = None

__all__ = [
    # 版本信息
    "__version__",
    "__author__",
    # 核心引擎
    "MonitorEngine",
    "ProcessInfo",
    "OutputLine",
    "ProcessStatus",
    # 事件系统
    "EventBus",
    "EventType",
    # 配置管理
    "Config",
    "ConfigManager",
    # CLI 界面
    "CLIMonitor",
    "monitor_commands",
    # Web 界面
    "WebDashboard",
    "launch_web_dashboard",
    # 工具函数
    "get_logger",
    "setup_logging",
    "validate_process_id",
    "validate_command",
    "DEFAULT_CONFIG",
]
