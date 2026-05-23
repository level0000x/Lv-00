"""
Web 仪表盘
============

封装 Flask 服务器的启动和生命周期管理。
"""

from __future__ import annotations

import logging
import threading
import webbrowser
from typing import Any

from ..core.engine import MonitorEngine
from ..core.config import Config, ConfigManager
from ..utils.logger import get_logger
from .routes import create_app

logger = get_logger(__name__)


class WebDashboard:
    """
    Web 仪表盘启动器
    
    封装 Flask 服务器的启动与生命周期管理，提供简洁的接口。
    
    Example:
        >>> engine = MonitorEngine()
        >>> engine.register_process("ping", "ping -n 5 127.0.0.1")
        >>> 
        >>> dashboard = WebDashboard(engine, port=5800)
        >>> dashboard.run()  # 阻塞直到服务器停止
    """

    def __init__(
        self,
        engine: MonitorEngine,
        config: Config | None = None,
        host: str | None = None,
        port: int | None = None,
        open_browser: bool | None = None,
    ):
        """
        初始化 Web 仪表盘
        
        Args:
            engine: 监控引擎实例
            config: 配置对象（None 使用默认配置）
            host: 监听地址（覆盖配置）
            port: 监听端口（覆盖配置）
            open_browser: 是否自动打开浏览器（覆盖配置）
        """
        self.engine = engine
        self.config = config or ConfigManager.get_config()

        # 应用参数覆盖
        self.host = host or self.config.web.host
        self.port = port or self.config.web.port
        self.open_browser = open_browser if open_browser is not None else self.config.web.open_browser

        # 创建 Flask 应用
        self.app = create_app(engine, self.config)

        # 服务器状态
        self._running = False
        self._server_thread: threading.Thread | None = None

        logger.info(f"Web 仪表盘初始化完成: {self.host}:{self.port}")

    def run(self, debug: bool = False, threaded: bool = True) -> None:
        """
        启动 Flask Web 服务器（阻塞调用）
        
        Args:
            debug: 是否启用调试模式
            threaded: 是否启用多线程
        """
        if self._running:
            raise RuntimeError("Web 仪表盘已经在运行")

        self._running = True

        url = f"http://{self.host}:{self.port}"
        logger.info(f"Web 仪表盘已启动: {url}")
        print(f"\n  ✅ Web 仪表盘已启动: {url}")
        print(f"  📡 SSE 端点: {url}/stream")
        print(f"  🔍 API 文档: {url}/api/processes")
        print(f"  🛑 按 Ctrl+C 停止服务器\n")

        # 自动打开浏览器
        if self.open_browser:
            threading.Timer(1.0, lambda: webbrowser.open(url)).start()

        try:
            # 启动 Flask 服务器
            self.app.run(
                host=self.host,
                port=self.port,
                debug=debug,
                threaded=threaded,
                use_reloader=False,  # 禁用重载器，避免子进程问题
            )
        except KeyboardInterrupt:
            logger.info("收到中断信号，正在停止服务器...")
        finally:
            self._running = False
            self.shutdown()

    def run_in_background(self, debug: bool = False) -> threading.Thread:
        """
        在后台线程中启动服务器
        
        Returns:
            服务器线程对象
        """
        if self._running:
            raise RuntimeError("Web 仪表盘已经在运行")

        def server_loop():
            self.run(debug=debug)

        self._server_thread = threading.Thread(target=server_loop, daemon=True)
        self._server_thread.start()

        logger.info("Web 仪表盘已在后台启动")
        return self._server_thread

    def shutdown(self) -> None:
        """关闭仪表盘"""
        if not self._running:
            return

        logger.info("正在关闭 Web 仪表盘...")
        self._running = False

        # 停止引擎
        try:
            self.engine.shutdown()
        except Exception as e:
            logger.warning(f"关闭引擎时出错: {e}")

        logger.info("Web 仪表盘已关闭")

    def __enter__(self) -> WebDashboard:
        """上下文管理器入口"""
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        """上下文管理器出口"""
        self.shutdown()


def launch_web_dashboard(
    engine: MonitorEngine,
    config: Config | None = None,
    host: str = "127.0.0.1",
    port: int = 5800,
    open_browser: bool = True,
) -> None:
    """
    快速启动 Web 仪表盘的便捷函数
    
    Args:
        engine: 监控引擎实例
        config: 配置对象
        host: 监听地址
        port: 监听端口
        open_browser: 是否自动打开浏览器
    
    Example:
        >>> engine = MonitorEngine()
        >>> engine.register_process("ping", "ping -n 5 127.0.0.1")
        >>> launch_web_dashboard(engine, port=8080)
    """
    dashboard = WebDashboard(
        engine=engine,
        config=config,
        host=host,
        port=port,
        open_browser=open_browser,
    )
    dashboard.run()
