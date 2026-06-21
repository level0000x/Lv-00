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

版本: 3.3.0
作者: Lv-00 Team
"""

__version__ = "3.3.0"
__author__ = "Lv-00 Team"

import warnings

# 核心组件导出
# [迁移说明] 以下 try/except ImportError 回退路径是为了向后兼容旧版单文件结构。
# 旧版代码将所有组件放在 concurrent_monitor/monitor_engine.py 单个文件中，
# 新版已重构为 concurrent_monitor/core/ 包结构。
# 回退路径引用的 .monitor_engine 文件为旧版遗留文件，新项目不应依赖此回退机制。
# 如果新包结构导入成功，旧文件将不会被加载。
try:
    from .core.engine import MonitorEngine
except ImportError:
    warnings.warn(
        "无法从 concurrent_monitor.core.engine 导入 MonitorEngine，"
        "正在回退到旧版 monitor_engine 模块。建议迁移到新版包结构。",
        ImportWarning,
        stacklevel=2,
    )
    from .monitor_engine import MonitorEngine  # 旧版单文件回退（已废弃，请迁移到 .core.engine）

try:
    from .core.models import ProcessInfo, OutputLine, ProcessStatus
except ImportError:
    warnings.warn(
        "无法从 concurrent_monitor.core.models 导入数据模型，"
        "正在回退到旧版 monitor_engine 模块。",
        ImportWarning,
        stacklevel=2,
    )
    from .monitor_engine import ProcessInfo, OutputLine, ProcessStatus  # 旧版单文件回退（已废弃）

try:
    from .core.events import EventBus, EventType
except ImportError:
    warnings.warn(
        "无法导入事件系统模块（EventBus/EventType），"
        "部分功能（事件驱动架构）将不可用。",
        ImportWarning,
        stacklevel=2,
    )
    # 事件系统是可选依赖，旧版可能不包含此模块
    # 设置为 None 后，使用方需自行检查是否可用
    EventBus = None
    EventType = None

try:
    from .core.config import Config, ConfigManager
except ImportError:
    warnings.warn(
        "无法导入配置管理模块（Config/ConfigManager），"
        "将使用硬编码的默认配置。",
        ImportWarning,
        stacklevel=2,
    )
    # 配置管理是可选依赖，旧版可能不包含此模块
    # 设置为 None 后，使用方需自行检查是否可用
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
    warnings.warn(
        "无法导入 CLI 监控模块，正在回退到旧版 cli_monitor。",
        ImportWarning,
        stacklevel=2,
    )
    from .cli_monitor import CLIMonitor, monitor_commands  # 旧版回退

try:
    from .web.dashboard import WebDashboard, launch_web_dashboard
except ImportError:
    warnings.warn(
        "无法导入 Web 仪表盘模块，正在回退到旧版 web_dashboard。",
        ImportWarning,
        stacklevel=2,
    )
    from .web_dashboard import WebDashboard, launch_web_dashboard  # 旧版回退

# 工具函数导出
# [迁移说明] 以下工具函数的 try/except 回退路径同样用于向后兼容。
# 新版已将工具函数拆分到 concurrent_monitor/utils/ 包中。
# 回退时将对应变量设为 None，调用方在使用前应进行 None 检查。
try:
    from .utils.logger import get_logger, setup_logging
except ImportError:
    warnings.warn(
        "无法导入日志模块（get_logger/setup_logging），"
        "系统将无法使用统一日志功能。",
        ImportWarning,
        stacklevel=2,
    )
    # 日志模块缺失时设为 None，系统将无法使用统一日志功能
    get_logger = None
    setup_logging = None

try:
    from .utils.validators import validate_process_id, validate_command
except ImportError:
    warnings.warn(
        "无法导入验证器模块（validate_process_id/validate_command），"
        "API 输入验证功能将不可用。",
        ImportWarning,
        stacklevel=2,
    )
    # 验证器模块缺失时设为 None，API 输入验证功能将不可用
    validate_process_id = None
    validate_command = None

try:
    from .utils.constants import DEFAULT_CONFIG
except ImportError:
    warnings.warn(
        "无法导入默认配置模块（DEFAULT_CONFIG），"
        "系统将使用硬编码的默认值。",
        ImportWarning,
        stacklevel=2,
    )
    # 默认配置缺失时设为 None，系统将使用硬编码的默认值
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
