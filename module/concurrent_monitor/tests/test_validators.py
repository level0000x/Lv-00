"""
测试验证工具模块
================
"""
import pytest
from concurrent_monitor.utils.validators import (
    ValidationError,
    validate_process_id,
    validate_command,
    validate_port,
    validate_positive_int,
    validate_timeout
)


class TestValidateProcessId:
    """测试进程ID验证"""

    def test_valid_process_id(self):
        """测试有效的进程ID"""
        assert validate_process_id("valid_id-123") == "valid_id-123"
        assert validate_process_id("a") == "a"
        assert validate_process_id("A_B_C") == "A_B_C"

    def test_empty_process_id(self):
        """测试空进程ID"""
        with pytest.raises(ValidationError, match="不能为空"):
            validate_process_id("")

    def test_process_id_too_long(self):
        """测试过长的进程ID"""
        long_id = "a" * 65
        with pytest.raises(ValidationError, match="不能超过64个字符"):
            validate_process_id(long_id)

    def test_invalid_characters(self):
        """测试无效字符"""
        with pytest.raises(ValidationError, match="只能包含"):
            validate_process_id("invalid!id")
        with pytest.raises(ValidationError, match="只能包含"):
            validate_process_id("with space")
        with pytest.raises(ValidationError, match="只能包含"):
            validate_process_id("id@#$%")


class TestValidateCommand:
    """测试命令验证"""

    def test_valid_command(self):
        """测试有效命令"""
        assert validate_command("echo hello") == "echo hello"
        assert validate_command("  ls -la  ") == "ls -la"

    def test_empty_command(self):
        """测试空命令"""
        with pytest.raises(ValidationError, match="不能为空"):
            validate_command("")

    def test_command_too_long(self):
        """测试过长的命令"""
        long_cmd = "a" * 4097
        with pytest.raises(ValidationError, match="不能超过4096个字符"):
            validate_command(long_cmd)

    def test_dangerous_commands(self):
        """测试危险命令检测"""
        with pytest.raises(ValidationError, match="包含危险操作"):
            validate_command("rm -rf /")
        with pytest.raises(ValidationError, match="包含危险操作"):
            validate_command(":(){ :|:& };:")


class TestValidatePort:
    """测试端口验证"""

    def test_valid_port(self):
        """测试有效端口"""
        assert validate_port(80) == 80
        assert validate_port(443) == 443
        assert validate_port(8080) == 8080
        assert validate_port(65535) == 65535
        assert validate_port(1) == 1

    def test_invalid_port_type(self):
        """测试无效端口类型"""
        with pytest.raises(ValidationError, match="必须是整数"):
            validate_port("not a number")

    def test_port_out_of_range(self):
        """测试端口超出范围"""
        with pytest.raises(ValidationError, match="1-65535"):
            validate_port(0)
        with pytest.raises(ValidationError, match="1-65535"):
            validate_port(65536)
        with pytest.raises(ValidationError, match="1-65535"):
            validate_port(-1)


class TestValidatePositiveInt:
    """测试正整数验证"""

    def test_valid_positive_int(self):
        """测试有效正整数"""
        assert validate_positive_int(1) == 1
        assert validate_positive_int(100) == 100
        assert validate_positive_int(9999) == 9999

    def test_zero_or_negative(self):
        """测试零或负数"""
        with pytest.raises(ValidationError, match="必须是正整数"):
            validate_positive_int(0)
        with pytest.raises(ValidationError, match="必须是正整数"):
            validate_positive_int(-1)

    def test_invalid_type(self):
        """测试无效类型"""
        with pytest.raises(ValidationError, match="必须是整数"):
            validate_positive_int("not a number")

    def test_with_name(self):
        """测试带名称的验证"""
        with pytest.raises(ValidationError, match="max_concurrency"):
            validate_positive_int(0, "max_concurrency")


class TestValidateTimeout:
    """测试超时验证"""

    def test_none_timeout(self):
        """测试None超时"""
        assert validate_timeout(None) is None

    def test_valid_timeout(self):
        """测试有效超时"""
        assert validate_timeout(1.0) == 1.0
        assert validate_timeout(10) == 10.0
        assert validate_timeout(3600) == 3600.0

    def test_zero_or_negative(self):
        """测试零或负超时"""
        with pytest.raises(ValidationError, match="必须大于0"):
            validate_timeout(0)
        with pytest.raises(ValidationError, match="必须大于0"):
            validate_timeout(-1)

    def test_too_large_timeout(self):
        """测试过大的超时"""
        with pytest.raises(ValidationError, match="不能超过24小时"):
            validate_timeout(86401)

    def test_invalid_type(self):
        """测试无效类型"""
        with pytest.raises(ValidationError, match="必须是数字"):
            validate_timeout("not a number")
