"""
验证工具
=========

提供各种数据验证函数。
"""

from __future__ import annotations

import re


class ValidationError(ValueError):
    """验证错误异常"""
    pass


def validate_process_id(process_id: str) -> str:
    """
    验证进程ID

    Args:
        process_id: 要验证的ID

    Returns:
        验证通过的ID

    Raises:
        ValidationError: 验证失败
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

    Args:
        command: 要验证的命令

    Returns:
        清理后的命令

    Raises:
        ValidationError: 验证失败
    """
    if not command:
        raise ValidationError("命令不能为空")

    if not isinstance(command, str):
        raise ValidationError(f"命令必须是字符串，当前类型: {type(command).__name__}")

    # 清理命令
    command = command.strip()

    if len(command) > 4096:
        raise ValidationError(f"命令长度不能超过4096个字符，当前: {len(command)}")

    # 检查危险命令（可选的安全检查）
    dangerous_patterns = [
        r"rm\s+-rf\s+/",
        r":\(\)\{\s*:\|:&\s*\};:",  # fork bomb
    ]

    for pattern in dangerous_patterns:
        if re.search(pattern, command, re.IGNORECASE):
            raise ValidationError(f"命令包含危险操作: {command[:50]}...")

    return command


def validate_port(port: int) -> int:
    """
    验证端口号

    Args:
        port: 端口号

    Returns:
        验证通过的端口号

    Raises:
        ValidationError: 验证失败
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

    Args:
        value: 要验证的值
        name: 值的名称（用于错误信息）

    Returns:
        验证通过的值

    Raises:
        ValidationError: 验证失败
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

    Args:
        timeout: 超时时间（秒）

    Returns:
        验证通过的超时时间

    Raises:
        ValidationError: 验证失败
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
