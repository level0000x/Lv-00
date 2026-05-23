"""
配置管理系统模块
================

本模块提供统一的配置加载、验证和持久化功能。
支持从 JSON 配置文件加载、环境变量覆盖和运行时修改。

核心组件：
  - EngineConfig: 引擎配置（并发数、超时、输出行数限制等）
  - WebConfig: Web 仪表盘配置（监听地址、端口、SSE 参数等）
  - CLIConfig: 命令行界面配置（刷新率、显示行数等）
  - LoggingConfig: 日志配置（级别、格式、文件输出等）
  - Config: 统一配置对象（聚合所有子系统配置）
  - ConfigManager: 配置管理器（全局单例，支持热重载）

配置优先级（从低到高）：
  1. 代码中的默认值
  2. JSON 配置文件
  3. 环境变量覆盖
  4. 运行时代码修改
"""

from __future__ import annotations

import json
import logging
import os
from dataclasses import dataclass, field, asdict
from pathlib import Path
from typing import Any

from ..utils.validators import validate_port, validate_positive_int

logger = logging.getLogger(__name__)


@dataclass
class EngineConfig:
    """
    引擎配置

    控制监控引擎的核心行为参数。

    Attributes:
        max_concurrency: 最大并发进程数
        default_timeout: 默认进程超时时间（秒），None 表示不限制
        max_output_lines: 每个进程保留的最大输出行数（超出时丢弃最早的行）
        buffer_size: 流读取缓冲区大小（字节）
    """
    max_concurrency: int = 20           # 最大并发数
    default_timeout: float | None = None  # 默认超时（秒）
    max_output_lines: int = 500         # 每进程最大输出行数
    buffer_size: int = 4096             # 流缓冲区大小

    def __post_init__(self) -> None:
        """初始化后验证配置值的合法性"""
        self.max_concurrency = validate_positive_int(self.max_concurrency, "max_concurrency")
        self.max_output_lines = validate_positive_int(self.max_output_lines, "max_output_lines")
        self.buffer_size = validate_positive_int(self.buffer_size, "buffer_size")


@dataclass
class WebConfig:
    """
    Web 仪表盘配置

    控制 Web 仪表盘服务器的行为参数。

    Attributes:
        host: HTTP 服务监听地址
        port: HTTP 服务监听端口
        open_browser: 是否自动在浏览器中打开仪表盘
        max_sse_clients: 最大 SSE（Server-Sent Events）客户端连接数
        sse_interval: SSE 心跳/摘要推送间隔（秒）
        cors_enabled: 是否启用跨域资源共享
        debug: 是否启用 Flask 调试模式
    """
    host: str = "127.0.0.1"             # 监听地址
    port: int = 5800                    # 监听端口
    open_browser: bool = True           # 自动打开浏览器
    max_sse_clients: int = 50           # 最大 SSE 客户端数
    sse_interval: float = 0.5           # SSE 推送间隔（秒）
    cors_enabled: bool = True           # 启用 CORS
    debug: bool = False                 # 调试模式

    def __post_init__(self) -> None:
        """初始化后验证配置值的合法性"""
        self.port = validate_port(self.port)
        self.max_sse_clients = validate_positive_int(self.max_sse_clients, "max_sse_clients")
        if self.sse_interval <= 0:
            raise ValueError("sse_interval 必须大于 0")


@dataclass
class CLIConfig:
    """
    命令行界面配置

    控制 CLI 监控界面的显示参数。

    Attributes:
        refresh_rate: 界面刷新率（每秒刷新次数）
        max_lines_per_process: 每个进程在界面中显示的最大输出行数
        show_system_messages: 是否显示系统消息
        color_enabled: 是否启用颜色输出（依赖 rich 库）
    """
    refresh_rate: float = 10.0          # 刷新率（每秒）
    max_lines_per_process: int = 200    # 每进程显示的最大行数
    show_system_messages: bool = True   # 显示系统消息
    color_enabled: bool = True          # 启用颜色输出

    def __post_init__(self) -> None:
        """初始化后验证配置值的合法性"""
        if self.refresh_rate <= 0:
            raise ValueError("refresh_rate 必须大于 0")
        self.max_lines_per_process = validate_positive_int(
            self.max_lines_per_process, "max_lines_per_process"
        )


@dataclass
class LoggingConfig:
    """
    日志配置

    控制日志系统的输出格式和行为。

    Attributes:
        level: 日志级别（DEBUG/INFO/WARNING/ERROR/CRITICAL）
        format: 日志格式字符串
        file: 日志文件路径（None 表示不输出到文件）
        max_bytes: 单个日志文件的最大大小（字节），用于日志轮转
        backup_count: 保留的备份日志文件数量
        console_enabled: 是否启用控制台日志输出
    """
    level: str = "INFO"                 # 日志级别
    format: str = "%(asctime)s - %(name)s - %(levelname)s - %(message)s"
    file: str | None = None             # 日志文件路径
    max_bytes: int = 10 * 1024 * 1024   # 单个日志文件最大大小（10MB）
    backup_count: int = 5               # 保留的备份文件数
    console_enabled: bool = True        # 启用控制台输出

    def __post_init__(self) -> None:
        """初始化后验证日志级别是否合法"""
        valid_levels: list[str] = ["DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"]
        if self.level.upper() not in valid_levels:
            raise ValueError(f"无效的日志级别: {self.level}")
        # 统一转为大写
        self.level = self.level.upper()


@dataclass
class Config:
    """
    统一配置对象

    聚合所有子系统的配置，提供统一的加载、保存和环境变量覆盖接口。

    Attributes:
        engine: 引擎配置
        web: Web 仪表盘配置
        cli: 命令行界面配置
        logging: 日志配置

    Example:
        >>> config = Config()
        >>> config.engine.max_concurrency = 10
        >>> config.web.port = 8080
        >>>
        >>> # 从文件加载
        >>> config = Config.from_file("config.json")
        >>>
        >>> # 保存到文件
        >>> config.save_to_file("config.json")
    """
    engine: EngineConfig = field(default_factory=EngineConfig)
    web: WebConfig = field(default_factory=WebConfig)
    cli: CLIConfig = field(default_factory=CLIConfig)
    logging: LoggingConfig = field(default_factory=LoggingConfig)

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> Config:
        """
        从字典创建配置对象

        字典中每个键对应一个子配置（engine/web/cli/logging），
        缺失的子配置使用默认值。

        Args:
            data: 配置字典

        Returns:
            Config: 新创建的配置对象
        """
        return cls(
            engine=EngineConfig(**data.get("engine", {})),
            web=WebConfig(**data.get("web", {})),
            cli=CLIConfig(**data.get("cli", {})),
            logging=LoggingConfig(**data.get("logging", {})),
        )

    def to_dict(self) -> dict[str, Any]:
        """
        将配置转换为字典

        Returns:
            dict: 包含所有子配置的字典
        """
        return {
            "engine": asdict(self.engine),
            "web": asdict(self.web),
            "cli": asdict(self.cli),
            "logging": asdict(self.logging),
        }

    @classmethod
    def from_file(cls, path: str | Path) -> Config:
        """
        从 JSON 文件加载配置

        如果文件不存在，记录警告并返回默认配置。

        Args:
            path: JSON 配置文件路径

        Returns:
            Config: 加载的配置对象

        Raises:
            json.JSONDecodeError: 配置文件 JSON 格式错误
            Exception: 文件读取失败
        """
        path = Path(path)
        if not path.exists():
            logger.warning(f"配置文件不存在: {path}，使用默认配置")
            return cls()

        try:
            with open(path, "r", encoding="utf-8") as f:
                data: dict[str, Any] = json.load(f)
            logger.info(f"从 {path} 加载配置")
            return cls.from_dict(data)
        except json.JSONDecodeError as e:
            logger.error(f"配置文件格式错误: {e}")
            raise
        except Exception as e:
            logger.error(f"加载配置文件失败: {e}")
            raise

    def save_to_file(self, path: str | Path) -> None:
        """
        保存配置到 JSON 文件

        自动创建父目录（如果不存在）。

        Args:
            path: 目标文件路径

        Raises:
            Exception: 文件写入失败
        """
        path = Path(path)
        try:
            path.parent.mkdir(parents=True, exist_ok=True)
            with open(path, "w", encoding="utf-8") as f:
                json.dump(self.to_dict(), f, indent=2, ensure_ascii=False)
            logger.info(f"配置已保存到: {path}")
        except Exception as e:
            logger.error(f"保存配置文件失败: {e}")
            raise

    def apply_env_overrides(self) -> None:
        """
        应用环境变量覆盖

        读取特定环境变量并覆盖对应的配置项。
        环境变量格式为 MONITOR_<SECTION>_<KEY>。

        支持的环境变量：
            - MONITOR_MAX_CONCURRENCY: 最大并发数（整数）
            - MONITOR_WEB_PORT: Web 端口（整数）
            - MONITOR_WEB_HOST: Web 监听地址（字符串）
            - MONITOR_LOG_LEVEL: 日志级别（字符串）
        """
        # 引擎配置：最大并发数
        if max_conc := os.getenv("MONITOR_MAX_CONCURRENCY"):
            try:
                self.engine.max_concurrency = int(max_conc)
                logger.debug(f"环境变量覆盖: max_concurrency = {max_conc}")
            except ValueError:
                logger.warning(f"无效的环境变量值: MONITOR_MAX_CONCURRENCY={max_conc}")

        # Web 配置：端口
        if port := os.getenv("MONITOR_WEB_PORT"):
            try:
                self.web.port = int(port)
                logger.debug(f"环境变量覆盖: web.port = {port}")
            except ValueError:
                logger.warning(f"无效的环境变量值: MONITOR_WEB_PORT={port}")

        # Web 配置：监听地址
        if host := os.getenv("MONITOR_WEB_HOST"):
            self.web.host = host
            logger.debug(f"环境变量覆盖: web.host = {host}")

        # 日志配置：日志级别
        if level := os.getenv("MONITOR_LOG_LEVEL"):
            self.logging.level = level.upper()
            logger.debug(f"环境变量覆盖: logging.level = {level}")


class ConfigManager:
    """
    配置管理器（全局单例）

    管理全局配置实例，提供统一的配置访问入口。
    支持初始化、获取、热重载和持久化操作。

    Example:
        >>> # 初始化（从文件加载）
        >>> ConfigManager.initialize("config.json")
        >>>
        >>> # 获取配置
        >>> config = ConfigManager.get_config()
        >>>
        >>> # 热重载（重新从文件读取）
        >>> ConfigManager.reload()
    """

    _instance: Config | None = None
    _config_path: Path | None = None
    _initialized: bool = False

    @classmethod
    def initialize(
        cls,
        config_path: str | Path | None = None,
        apply_env: bool = True,
    ) -> Config:
        """
        初始化配置管理器

        只能初始化一次，重复调用会发出警告并返回已有实例。

        Args:
            config_path: 配置文件路径（None 使用默认配置）
            apply_env: 是否应用环境变量覆盖

        Returns:
            Config: 配置对象
        """
        if cls._initialized:
            logger.warning("配置管理器已经初始化")
            return cls._instance  # type: ignore[return-value]

        # 根据是否有配置文件路径决定加载方式
        if config_path:
            cls._config_path = Path(config_path)
            cls._instance = Config.from_file(cls._config_path)
        else:
            cls._instance = Config()

        # 应用环境变量覆盖
        if apply_env:
            cls._instance.apply_env_overrides()

        cls._initialized = True
        logger.info("配置管理器初始化完成")
        return cls._instance  # type: ignore[return-value]

    @classmethod
    def get_config(cls) -> Config:
        """
        获取当前配置

        如果尚未初始化，会自动使用默认配置进行初始化。

        Returns:
            Config: 当前配置对象
        """
        if not cls._initialized:
            cls.initialize()
        return cls._instance  # type: ignore[return-value]

    @classmethod
    def reload(cls) -> Config:
        """
        重新加载配置（热重载）

        从之前指定的配置文件重新读取配置，并应用环境变量覆盖。
        如果没有配置文件路径，发出警告。

        Returns:
            Config: 重新加载后的配置对象
        """
        if cls._config_path and cls._config_path.exists():
            cls._instance = Config.from_file(cls._config_path)
            cls._instance.apply_env_overrides()
            logger.info("配置已重新加载")
        else:
            logger.warning("没有配置文件，无法重载")
        return cls._instance  # type: ignore[return-value]

    @classmethod
    def save(cls, path: str | Path | None = None) -> None:
        """
        保存当前配置到文件

        Args:
            path: 保存路径（None 使用初始化时的配置文件路径）

        Raises:
            RuntimeError: 配置尚未初始化
            ValueError: 未指定保存路径且没有已知的配置文件路径
        """
        if cls._instance is None:
            raise RuntimeError("配置未初始化")

        save_path: Path | None = Path(path) if path else cls._config_path
        if save_path is None:
            raise ValueError("未指定保存路径")

        cls._instance.save_to_file(save_path)

    @classmethod
    def reset(cls) -> None:
        """
        重置为默认配置

        将配置恢复为所有默认值，清除配置文件路径引用。
        """
        cls._instance = Config()
        logger.info("配置已重置为默认值")
