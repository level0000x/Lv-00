"""
日志系统
=========

提供统一的日志配置和管理功能。
支持控制台和文件输出，自动轮转。
控制台输出支持 ANSI 颜色码，兼容 Windows 10+ 终端。
"""

from __future__ import annotations

import logging
import logging.handlers
import os
import sys
from pathlib import Path
from typing import ClassVar

from .constants import DEFAULT_LOG_FORMAT


def _supports_ansi_color() -> bool:
    """
    检测当前终端环境是否支持 ANSI 颜色码

    通过以下条件综合判断：
    1. 环境变量 ANSICON 已设置（ANSI Console 模拟器）
    2. 环境变量 WT_SESSION 已设置（Windows Terminal）
    3. 标准输出是 TTY（交互式终端）
    4. 非 Windows 平台默认支持

    Returns:
        bool: 如果终端支持 ANSI 颜色码则返回 True
    """
    # Windows Terminal 或 ANSICON 环境明确支持 ANSI
    if os.environ.get("ANSICON") or os.environ.get("WT_SESSION"):
        return True
    # 标准输出为交互式终端
    if sys.stdout.isatty():
        return True
    # 非 Windows 平台（Linux/macOS）默认支持
    if sys.platform != "win32":
        return True
    # Windows 10+ 已支持 ANSI 转义序列，通过检查是否可启用虚拟终端处理来判断
    try:
        import ctypes
        kernel32 = ctypes.windll.kernel32
        # ENABLE_VIRTUAL_TERMINAL_PROCESSING = 0x0004
        handle = kernel32.GetStdHandle(-11)  # STD_OUTPUT_HANDLE
        mode = ctypes.c_ulong()
        if kernel32.GetConsoleMode(handle, ctypes.byref(mode)):
            kernel32.SetConsoleMode(handle, mode.value | 0x0004)
            return True
    except Exception:
        pass
    return False


class ColoredFormatter(logging.Formatter):
    """
    带颜色的日志格式化器

    为不同日志级别（DEBUG/INFO/WARNING/ERROR/CRITICAL）
    添加对应的 ANSI 颜色码，提升控制台日志的可读性。
    自动检测终端是否支持颜色，不支持时自动降级为纯文本。
    """

    # ANSI 颜色码映射（类变量，所有实例共享同一份颜色配置）
    COLORS: ClassVar[dict[str, str]] = {
        "DEBUG": "\033[36m",      # 青色
        "INFO": "\033[32m",       # 绿色
        "WARNING": "\033[33m",    # 黄色
        "ERROR": "\033[31m",      # 红色
        "CRITICAL": "\033[35m",   # 紫色
        "RESET": "\033[0m",       # 重置
    }

    def __init__(self, fmt: str | None = None, use_color: bool = True):
        """
        初始化颜色格式化器

        Args:
            fmt: 日志格式字符串（None 时使用默认格式）
            use_color: 是否启用颜色（默认 True），
                最终还会结合终端 ANSI 支持检测结果
        """
        super().__init__(fmt or DEFAULT_LOG_FORMAT)
        # 仅在用户启用颜色且终端支持 ANSI 时才使用颜色
        self.use_color = use_color and _supports_ansi_color()

    def format(self, record: logging.LogRecord) -> str:
        """
        格式化日志记录

        在日志级别名称前后添加 ANSI 颜色码，
        格式化完成后恢复原始级别名称以避免副作用。

        Args:
            record: 日志记录对象

        Returns:
            str: 格式化后的日志字符串
        """
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

    设置根日志记录器的级别、格式和输出目标。
    支持同时输出到控制台（带颜色）和文件（自动轮转）。

    Args:
        level: 日志级别，可选值: DEBUG/INFO/WARNING/ERROR/CRITICAL
        format_str: 自定义日志格式字符串（None 时使用默认格式）
        log_file: 日志文件路径（None 则不输出到文件）
        max_bytes: 单个日志文件最大字节数（默认 10MB），
            超过后自动轮转
        backup_count: 保留的备份日志文件数量（默认 5）
        console: 是否输出到控制台（默认 True）
        color: 是否启用控制台颜色输出（默认 True），
            会自动检测终端 ANSI 支持情况

    Example:
        >>> setup_logging(level="DEBUG", log_file="app.log")
        >>> logger = logging.getLogger(__name__)
        >>> logger.info("应用启动")
    """
    # 获取根日志记录器
    root_logger = logging.getLogger()
    root_logger.setLevel(getattr(logging, level.upper()))

    # 清除现有处理器，避免重复添加
    # [行为影响说明] 此操作会移除根日志记录器上的所有已注册 Handler，
    # 包括 Python 标准库或其他第三方库（如 werkzeug、uvicorn）自动添加的处理器。
    # 影响范围：
    #   1. 如果其他库在此之前已向根记录器添加了自定义 Handler，它们将被移除
    #   2. 多次调用 setup_logging() 是安全的（不会产生重复处理器）
    #   3. 在多线程/多进程环境中，此操作非原子性，可能存在短暂竞态条件
    # 风险说明：
    #   - 如果本函数在第三方库（如 Flask、uvicorn）初始化之后调用，
    #     可能会影响那些依赖自身 Handler 的库的日志输出。
    #     例如，Flask 的 werkzeug 日志处理器会被移除，导致 Flask 启动日志消失。
    #   - 在多进程场景（如 gunicorn --preload）中，子进程会继承父进程的 Handler，
    #     如果在 fork 后调用此函数，可能导致日志重复或丢失。
    #   - 某些库（如 logging.config.dictConfig）会设置 propagate=False 的子记录器，
    #     这些子记录器不受此操作影响。
    # 建议：在应用启动的最早阶段（导入第三方库之前）调用此函数，
    #   或者在调用后手动重新添加必要的第三方库 Handler。
    root_logger.handlers.clear()

    format_str = format_str or DEFAULT_LOG_FORMAT

    # 控制台处理器
    if console:
        console_handler = logging.StreamHandler(sys.stdout)
        console_handler.setLevel(logging.DEBUG)
        console_handler.setFormatter(ColoredFormatter(format_str, use_color=color))
        root_logger.addHandler(console_handler)

    # 文件处理器（使用 RotatingFileHandler 自动轮转）
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
        # 文件输出不使用颜色
        file_handler.setFormatter(logging.Formatter(format_str))
        root_logger.addHandler(file_handler)

    # 设置第三方库日志级别，减少噪音输出
    logging.getLogger("werkzeug").setLevel(logging.WARNING)
    logging.getLogger("asyncio").setLevel(logging.WARNING)


def get_logger(name: str) -> logging.Logger:
    """
    获取日志记录器

    获取指定名称的日志记录器实例。通常传入模块的 __name__
    以自动反映模块的层级结构。

    Args:
        name: 记录器名称（通常使用 __name__）

    Returns:
        logging.Logger: 配置好的日志记录器实例
    """
    return logging.getLogger(name)
