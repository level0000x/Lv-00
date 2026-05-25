"""
Web 模块 - Web 仪表盘
======================

提供基于 Flask 的 Web 界面和 REST API。
"""

from .dashboard import WebDashboard, launch_web_dashboard
from .routes import create_app

__all__ = ["WebDashboard", "launch_web_dashboard", "create_app"]
