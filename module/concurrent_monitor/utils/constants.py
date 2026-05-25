"""
常量定义
=========

定义系统中使用的所有常量，包括版本信息、默认配置、
状态样式映射、HTTP 状态码、SSE 配置、进程限制、
时间配置、文件路径和安全策略等。
"""

from __future__ import annotations

from typing import Any


# ============================================================
# 版本信息
# ============================================================
# 版本号与 __init__.py 中的 __version__="3.3.0" 保持一致
VERSION = "3.3.0"
VERSION_INFO = (3, 3, 0)


# ============================================================
# 默认配置
# ============================================================
# 包含引擎、Web、CLI 和日志四个子模块的默认参数
DEFAULT_CONFIG: dict[str, Any] = {
    "engine": {
        "max_concurrency": 20,
        "default_timeout": None,
        "max_output_lines": 500,
        "buffer_size": 4096,
    },
    "web": {
        "host": "127.0.0.1",
        "port": 5800,
        "open_browser": True,
        "max_sse_clients": 50,
        "sse_interval": 0.5,
        "cors_enabled": True,
        "debug": False,
    },
    "cli": {
        "refresh_rate": 10.0,
        "max_lines_per_process": 200,
        "show_system_messages": True,
        "color_enabled": True,
    },
    "logging": {
        "level": "INFO",
        "format": "%(asctime)s - %(name)s - %(levelname)s - %(message)s",
        "file": None,
        "max_bytes": 10 * 1024 * 1024,  # 10MB
        "backup_count": 5,
        "console_enabled": True,
    },
}

# ============================================================
# 日志格式
# ============================================================
# 默认格式包含时间、模块名、级别和消息；简单格式仅包含级别和消息
DEFAULT_LOG_FORMAT = "%(asctime)s - %(name)s - %(levelname)s - %(message)s"
SIMPLE_LOG_FORMAT = "%(levelname)s: %(message)s"


# ============================================================
# 状态样式映射（用于 CLI 显示）
# ============================================================
# 定义进程各状态对应的 Rich 终端颜色和图标符号
STATUS_COLORS = {
    "pending": "dim",
    "running": "yellow",
    "completed": "green",
    "failed": "red",
    "timeout": "magenta",
}

STATUS_ICONS = {
    "pending": "○",
    "running": "◉",
    "completed": "✔",
    "failed": "✘",
    "timeout": "⏱",
}

# ============================================================
# HTTP 状态码
# ============================================================
HTTP_OK = 200
HTTP_BAD_REQUEST = 400
HTTP_NOT_FOUND = 404
HTTP_SERVICE_UNAVAILABLE = 503


# ============================================================
# SSE（Server-Sent Events）配置
# ============================================================
SSE_RETRY_INTERVAL = 3000  # 客户端重连间隔（毫秒）
SSE_MAX_CLIENTS = 50       # 最大 SSE 连接数
SSE_HEARTBEAT_INTERVAL = 30  # 心跳间隔（秒）


# ============================================================
# 进程限制
# ============================================================
MAX_PROCESS_ID_LENGTH = 64        # 进程ID最大长度
MAX_COMMAND_LENGTH = 4096         # 命令字符串最大长度
MAX_OUTPUT_LINES_DEFAULT = 500    # 每个进程默认最大输出行数
MAX_CONCURRENCY_DEFAULT = 20      # 默认最大并发进程数


# ============================================================
# 时间配置
# ============================================================
DEFAULT_TIMEOUT = None             # 默认无超时限制
PROCESS_KILL_TIMEOUT = 5.0        # 终止进程后的等待时间（秒）


# ============================================================
# 文件路径
# ============================================================
DEFAULT_CONFIG_FILE = "monitor_config.json"  # 默认配置文件名
DEFAULT_LOG_FILE = "monitor.log"             # 默认日志文件名


# ============================================================
# 安全策略 - 危险命令模式
# ============================================================
# 用于 validate_command() 中检测潜在危险的命令。
# 包含 Linux/Unix 和 Windows 平台常见的破坏性命令模式。
# 每个条目为正则表达式字符串，匹配时将拒绝执行。
DANGEROUS_COMMAND_PATTERNS: list[str] = [
    # --- Linux/Unix 危险命令 ---
    r"rm\s+-rf\s+/",                    # 递归强制删除根目录
    r":\(\)\{\s*:\|:&\s*\};:",          # Fork 炸弹
    # --- Windows 危险命令 ---
    r"del\s+/[fFsS]\s+/[aAcCqQ]\s+C:\\",  # 强制删除 C 盘文件
    r"format\s+",                          # 格式化磁盘
    r"reg\s+delete",                       # 删除注册表项
]
