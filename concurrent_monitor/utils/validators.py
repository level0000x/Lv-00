"""
验证工具
=========

提供各种数据验证函数，包括进程ID、命令字符串、
端口号、正整数和超时时间的校验。
命令验证中集成了危险命令模式检测，防止执行破坏性操作。
"""

from __future__ import annotations

import re

from .constants import DANGEROUS_COMMAND_PATTERNS


class ValidationError(ValueError):
    """
    验证错误异常

    当数据验证失败时抛出，继承自 ValueError。
    """
    pass


def validate_process_id(process_id: str) -> str:
    """
    验证进程ID

    检查进程ID是否为非空字符串、长度不超过64个字符，
    且仅包含字母、数字、下划线和连字符。

    Args:
        process_id: 要验证的进程ID字符串

    Returns:
        str: 验证通过的原始进程ID

    Raises:
        ValidationError: 当进程ID为空、类型错误、
            长度超限或包含非法字符时抛出
    """
    if not process_id:
        raise ValidationError("进程ID不能为空")

    if not isinstance(process_id, str):
        raise ValidationError(f"进程ID必须是字符串，当前类型: {type(process_id).__name__}")

    if len(process_id) > 64:
        raise ValidationError(f"进程ID长度不能超过64个字符，当前: {len(process_id)}")

    # 只允许字母、数字、下划线、连字符
    if not re.match(r"^[a-zA-Z0-9_-]+$", process_id):
        raise ValidationError(
            "进程ID只能包含字母、数字、下划线和连字符"
        )

    return process_id


def validate_command(command: str) -> str:
    """
    验证命令字符串

    检查命令是否为非空字符串、长度不超过4096个字符，
    并通过正则匹配检测是否包含危险命令模式（包括
    Linux/Unix 和 Windows 平台的破坏性命令）。

    Args:
        command: 要验证的命令字符串

    Returns:
        str: 清理（去除首尾空白）后的命令字符串

    Raises:
        ValidationError: 当命令为空、类型错误、
            长度超限或包含危险操作模式时抛出
    """
    if not command:
        raise ValidationError("命令不能为空")

    if not isinstance(command, str):
        raise ValidationError(f"命令必须是字符串，当前类型: {type(command).__name__}")

    # 清理命令：去除首尾空白
    command = command.strip()

    if len(command) > 4096:
        raise ValidationError(f"命令长度不能超过4096个字符，当前: {len(command)}")

    # 检查危险命令模式（来自 constants.DANGEROUS_COMMAND_PATTERNS）
    for pattern in DANGEROUS_COMMAND_PATTERNS:
        if re.search(pattern, command, re.IGNORECASE):
            raise ValidationError(f"命令包含危险操作: {command[:50]}...")

    return command


def validate_port(port: int) -> int:
    """
    验证端口号

    检查端口号是否为1到65535之间的整数。

    Args:
        port: 要验证的端口号

    Returns:
        int: 验证通过的端口号（整数）

    Raises:
        ValidationError: 当端口号不是整数或不在有效范围内时抛出
    """
    try:
        port = int(port)
    except (TypeError, ValueError):
        raise ValidationError(f"端口号必须是整数，当前: {port}")

    if not 1 <= port <= 65535:
        raise ValidationError(f"端口号必须在 1-65535 之间，当前: {port}")

    return port


def validate_positive_int(value: int, name: str = "value") -> int:
    """
    验证正整数

    检查给定值是否为正整数。

    Args:
        value: 要验证的值
        name: 值的名称，用于生成可读的错误信息（默认为 "value"）

    Returns:
        int: 验证通过的正整数值

    Raises:
        ValidationError: 当值不是整数或不是正数时抛出
    """
    try:
        value = int(value)
    except (TypeError, ValueError):
        raise ValidationError(f"{name} 必须是整数，当前: {value}")

    if value <= 0:
        raise ValidationError(f"{name} 必须是正整数，当前: {value}")

    return value


def validate_timeout(timeout: float | None) -> float | None:
    """
    验证超时时间

    检查超时时间是否为正数且不超过24小时（86400秒）。
    允许传入 None 表示不设置超时。

    Args:
        timeout: 超时时间（秒），None 表示无超时限制

    Returns:
        float | None: 验证通过的超时时间（浮点数），
            若输入为 None 则返回 None

    Raises:
        ValidationError: 当超时时间不是数字、
            不是正数或超过24小时时抛出
    """
    if timeout is None:
        return None

    try:
        timeout = float(timeout)
    except (TypeError, ValueError):
        raise ValidationError(f"超时时间必须是数字，当前: {timeout}")

    if timeout <= 0:
        raise ValidationError(f"超时时间必须大于0，当前: {timeout}")

    if timeout > 86400:  # 24小时
        raise ValidationError(f"超时时间不能超过24小时，当前: {timeout}")

    return timeout
