"""
测试配置管理模块
===============
"""
import os
import tempfile
import pytest
from concurrent_monitor.core.config import (
    EngineConfig,
    WebConfig,
    CLIConfig,
    LoggingConfig,
    Config,
    ConfigManager
)


class TestEngineConfig:
    """测试引擎配置"""

    def test_defaults(self):
        """测试默认值"""
        config = EngineConfig()
        assert config.max_concurrency == 20
        assert config.default_timeout is None
        assert config.max_output_lines == 500
        assert config.buffer_size == 4096

    def test_validation(self):
        """测试验证"""
        with pytest.raises(Exception):
            EngineConfig(max_concurrency=0)
        with pytest.raises(Exception):
            EngineConfig(max_output_lines=-1)


class TestWebConfig:
    """测试Web配置"""

    def test_defaults(self):
        """测试默认值"""
        config = WebConfig()
        assert config.host == "127.0.0.1"
        assert config.port == 5800
        assert config.open_browser is True
        assert config.max_sse_clients == 50
        assert config.cors_enabled is True
        assert config.debug is False

    def test_validation(self):
        """测试验证"""
        with pytest.raises(Exception):
            WebConfig(port=0)
        with pytest.raises(Exception):
            WebConfig(port=65536)


class TestCLIConfig:
    """测试CLI配置"""

    def test_defaults(self):
        """测试默认值"""
        config = CLIConfig()
        assert config.refresh_rate == 10.0
        assert config.max_lines_per_process == 200
        assert config.show_system_messages is True
        assert config.color_enabled is True


class TestLoggingConfig:
    """测试日志配置"""

    def test_defaults(self):
        """测试默认值"""
        config = LoggingConfig()
        assert config.level == "INFO"
        assert config.file is None
        assert config.console_enabled is True

    def test_invalid_level(self):
        """测试无效日志级别"""
        with pytest.raises(Exception):
            LoggingConfig(level="INVALID")


class TestConfig:
    """测试配置主类"""

    def test_default_config(self):
        """测试默认配置"""
        config = Config()
        assert isinstance(config.engine, EngineConfig)
        assert isinstance(config.web, WebConfig)
        assert isinstance(config.cli, CLIConfig)
        assert isinstance(config.logging, LoggingConfig)

    def test_to_dict_and_from_dict(self):
        """测试序列化和反序列化"""
        config1 = Config()
        config1.engine.max_concurrency = 100
        config1.web.port = 9000

        data = config1.to_dict()
        config2 = Config.from_dict(data)

        assert config2.engine.max_concurrency == 100
        assert config2.web.port == 9000

    def test_save_and_load_file(self):
        """测试文件保存和加载"""
        with tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.json') as f:
            config_path = f.name

        try:
            config1 = Config()
            config1.engine.max_concurrency = 50
            config1.save_to_file(config_path)

            config2 = Config.from_file(config_path)
            assert config2.engine.max_concurrency == 50
        finally:
            if os.path.exists(config_path):
                os.unlink(config_path)

    def test_apply_env_overrides(self):
        """测试环境变量覆盖"""
        config = Config()
        original_concurrency = config.engine.max_concurrency
        original_port = config.web.port

        try:
            os.environ['MONITOR_MAX_CONCURRENCY'] = '999'
            os.environ['MONITOR_WEB_PORT'] = '8888'
            config.apply_env_overrides()

            assert config.engine.max_concurrency == 999
            assert config.web.port == 8888
        finally:
            os.environ.pop('MONITOR_MAX_CONCURRENCY', None)
            os.environ.pop('MONITOR_WEB_PORT', None)


class TestConfigManager:
    """测试配置管理器"""

    def setup_method(self):
        """每个测试前重置"""
        ConfigManager._instance = None
        ConfigManager._config_path = None
        ConfigManager._initialized = False

    def test_get_config_default(self):
        """测试获取默认配置"""
        config = ConfigManager.get_config()
        assert isinstance(config, Config)

    def test_initialize_twice(self):
        """测试重复初始化"""
        config1 = ConfigManager.initialize()
        config2 = ConfigManager.initialize()
        assert config1 is config2

    def test_reset(self):
        """测试重置配置"""
        config = ConfigManager.initialize()
        config.engine.max_concurrency = 999
        ConfigManager.reset()
        ConfigManager._initialized = False
        new_config = ConfigManager.get_config()
        assert new_config.engine.max_concurrency == 20
