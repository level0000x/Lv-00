"""
常量定义
=========

定义系统中使用的所有常量。
"""

from __future__ import annotations

from typing import Any

# 版本信息
VERSION = "2.0.0"
VERSION_INFO = (2, 0, 0)

# 默认配置
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

# 日志格式
DEFAULT_LOG_FORMAT = "%(asctime)s - %(name)s - %(levelname)s - %(message)s"
SIMPLE_LOG_FORMAT = "%(levelname)s: %(message)s"

# 状态样式映射（用于 CLI 显示）
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

# HTTP 状态码
HTTP_OK = 200
HTTP_BAD_REQUEST = 400
HTTP_NOT_FOUND = 404
HTTP_SERVICE_UNAVAILABLE = 503

# SSE 配置
SSE_RETRY_INTERVAL = 3000  # 毫秒
SSE_MAX_CLIENTS = 50
SSE_HEARTBEAT_INTERVAL = 30  # 秒

# 进程限制
MAX_PROCESS_ID_LENGTH = 64
MAX_COMMAND_LENGTH = 4096
MAX_OUTPUT_LINES_DEFAULT = 500
MAX_CONCURRENCY_DEFAULT = 20

# 时间配置
DEFAULT_TIMEOUT = None
PROCESS_KILL_TIMEOUT = 5.0  # 终止进程等待时间

# 文件路径
DEFAULT_CONFIG_FILE = "monitor_config.json"
DEFAULT_LOG_FILE = "monitor.log"
