"""
日志系统
=========

提供统一的日志配置和管理功能。
支持控制台和文件输出，自动轮转。
"""

from __future__ import annotations

import logging
import logging.handlers
import sys
from pathlib import Path
from typing import TextIO

from .constants import DEFAULT_LOG_FORMAT


class ColoredFormatter(logging.Formatter):
    """带颜色的日志格式化器"""

    # ANSI 颜色码
    COLORS = {
        "DEBUG": "\033[36m",      # 青色
        "INFO": "\033[32m",       # 绿色
        "WARNING": "\033[33m",    # 黄色
        "ERROR": "\033[31m",      # 红色
        "CRITICAL": "\033[35m",   # 紫色
        "RESET": "\033[0m",       # 重置
    }

    def __init__(self, fmt: str | None = None, use_color: bool = True):
        super().__init__(fmt or DEFAULT_LOG_FORMAT)
        self.use_color = use_color and sys.platform != "win32"

    def format(self, record: logging.LogRecord) -> str:
        """格式化日志记录"""
        # 保存原始级别名
        levelname = record.levelname

        if self.use_color:
            # 添加颜色
            color = self.COLORS.get(levelname, self.COLORS["RESET"])
            record.levelname = f"{color}{levelname}{self.COLORS['RESET']}"

        result = super().format(record)
        record.levelname = levelname  # 恢复原始级别名
        return result


def setup_logging(
    level: str = "INFO",
    format_str: str | None = None,
    log_file: str | Path | None = None,
    max_bytes: int = 10 * 1024 * 1024,
    backup_count: int = 5,
    console: bool = True,
    color: bool = True,
) -> None:
    """
    配置日志系统

    Args:
        level: 日志级别 (DEBUG/INFO/WARNING/ERROR/CRITICAL)
        format_str: 日志格式字符串
        log_file: 日志文件路径
        max_bytes: 单个日志文件最大大小
        backup_count: 保留的备份文件数
        console: 是否输出到控制台
        color: 是否启用颜色

    Example:
        >>> setup_logging(level="DEBUG", log_file="app.log")
        >>> logger = logging.getLogger(__name__)
        >>> logger.info("应用启动")
    """
    # 获取根日志记录器
    root_logger = logging.getLogger()
    root_logger.setLevel(getattr(logging, level.upper()))

    # 清除现有处理器
    root_logger.handlers.clear()

    format_str = format_str or DEFAULT_LOG_FORMAT

    # 控制台处理器
    if console:
        console_handler = logging.StreamHandler(sys.stdout)
        console_handler.setLevel(logging.DEBUG)
        console_handler.setFormatter(ColoredFormatter(format_str, use_color=color))
        root_logger.addHandler(console_handler)

    # 文件处理器
    if log_file:
        log_path = Path(log_file)
        log_path.parent.mkdir(parents=True, exist_ok=True)

        file_handler = logging.handlers.RotatingFileHandler(
            log_path,
            maxBytes=max_bytes,
            backupCount=backup_count,
            encoding="utf-8",
        )
        file_handler.setLevel(logging.DEBUG)
        file_handler.setFormatter(logging.Formatter(format_str))
        root_logger.addHandler(file_handler)

    # 设置第三方库日志级别
    logging.getLogger("werkzeug").setLevel(logging.WARNING)
    logging.getLogger("asyncio").setLevel(logging.WARNING)


def get_logger(name: str) -> logging.Logger:
    """
    获取日志记录器

    Args:
        name: 记录器名称（通常使用 __name__）

    Returns:
        配置好的日志记录器
    """
    return logging.getLogger(name)
