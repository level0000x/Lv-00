
"""
测试日志工具模块
===============
"""
import os
import tempfile
import logging
from pathlib import Path
import pytest
from concurrent_monitor.utils.logger import (
    setup_logging,
    get_logger,
    ColoredFormatter,
    _supports_ansi_color,
)


class TestSupportsAnsiColor:
    """测试 ANSI 颜色支持检测"""

    def test_basic_call(self):
        """测试基本调用不抛出异常"""
        result = _supports_ansi_color()
        assert isinstance(result, bool)


class TestColoredFormatter:
    """测试带颜色的日志格式化器"""

    def test_creation(self):
        """测试创建格式化器"""
        formatter = ColoredFormatter()
        assert formatter is not None
        assert isinstance(formatter.use_color, bool)

    def test_format(self):
        """测试格式化日志记录"""
        formatter = ColoredFormatter(use_color=False)
        record = logging.LogRecord(
            name="test",
            level=logging.INFO,
            pathname="test.py",
            lineno=10,
            msg="test message",
            args=(),
            exc_info=None,
        )
        formatted = formatter.format(record)
        assert "test message" in formatted


class TestSetupLogging:
    """测试日志系统配置"""

    def test_setup_basic(self):
        """测试基本配置"""
        setup_logging(level="INFO", console=True)
        logger = logging.getLogger()
        assert logger.level == logging.INFO

    def test_setup_with_file(self):
        """测试配置文件输出"""
        with tempfile.TemporaryDirectory() as tmpdir:
            log_path = Path(tmpdir) / "test.log"
            setup_logging(
                level="DEBUG",
                log_file=str(log_path),
                console=True,
            )
            # 测试写日志
            logger = get_logger("test")
            logger.info("test log message")
            # 验证文件存在
            assert log_path.exists()

    def test_get_logger(self):
        """测试获取日志记录器"""
        logger = get_logger("test_module")
        assert logger is not None
        assert logger.name == "test_module"
