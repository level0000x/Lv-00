"""
LLM编程辅助系统 - JWT 认证模块
================================

本模块提供基于 JWT (JSON Web Token) 的用户认证功能，包括：
  - 访问令牌（Access Token）的生成与验证
  - 请求头中 Bearer Token 的提取与校验
  - 用户数据模型定义

支持两种实现方式：
  1. PyJWT 库（推荐，pip install PyJWT）
  2. 纯 HMAC+SHA256 实现（无需额外依赖的降级方案）
"""

from __future__ import annotations

import hmac
import hashlib
import json
import time
import base64
from typing import Optional, Dict, Any
from dataclasses import dataclass, field

# ============================================================
# 尝试导入 PyJWT，不可用时降级为纯 HMAC+SHA256 实现
# ============================================================
try:
    import jwt as _pyjwt
    PYJWT_AVAILABLE = True
except ImportError:
    PYJWT_AVAILABLE = False


# ============================================================
# 用户数据模型
# ============================================================

@dataclass
class User:
    """
    用户数据类

    Attributes:
        id: 用户唯一标识
        username: 用户名
        role: 用户角色（admin 或 user）
        password_hash: SHA256 哈希后的密码（不直接存储明文）
    """
    id: str
    username: str
    role: str = "user"
    password_hash: str = ""

    def to_dict(self) -> Dict[str, Any]:
        """转为可序列化的字典（不含密码哈希）"""
        return {
            "id": self.id,
            "username": self.username,
            "role": self.role,
        }


# ============================================================
# JWT 令牌生成（纯 HMAC+SHA256 实现）
# ============================================================

def _base64url_encode(data: bytes) -> str:
    """Base64URL 编码（无填充）"""
    return base64.urlsafe_b64encode(data).rstrip(b'=').decode('ascii')


def _base64url_decode(data: str) -> bytes:
    """Base64URL 解码（自动补齐填充）"""
    padding = 4 - len(data) % 4
    if padding != 4:
        data += '=' * padding
    return base64.urlsafe_b64decode(data)


def _hmac_sha256_sign(payload_b64: str, secret_key: str) -> str:
    """
    使用 HMAC+SHA256 对 payload 进行签名

    Args:
        payload_b64: Base64URL 编码的 payload
        secret_key: 密钥

    Returns:
        Base64URL 编码的签名
    """
    key_bytes = secret_key.encode('utf-8')
    data_bytes = payload_b64.encode('ascii')
    sig = hmac.new(key_bytes, data_bytes, hashlib.sha256).digest()
    return _base64url_encode(sig)


def _hmac_sha256_verify(payload_b64: str, signature: str, secret_key: str) -> bool:
    """
    使用 HMAC+SHA256 验证签名

    Args:
        payload_b64: Base64URL 编码的 payload
        signature: 声称的签名
        secret_key: 密钥

    Returns:
        True 表示签名有效
    """
    expected = _hmac_sha256_sign(payload_b64, secret_key)
    return hmac.compare_digest(expected, signature)


# ============================================================
# JWT 令牌生成与验证（PyJWT 实现）
# ============================================================

def _pyjwt_create_token(payload: Dict[str, Any], secret_key: str) -> str:
    """使用 PyJWT 生成令牌"""
    return _pyjwt.encode(payload, secret_key, algorithm="HS256")


def _pyjwt_verify_token(token: str, secret_key: str) -> Optional[Dict[str, Any]]:
    """使用 PyJWT 验证并解码令牌"""
    try:
        payload = _pyjwt.decode(token, secret_key, algorithms=["HS256"])
        return payload
    except _pyjwt.ExpiredSignatureError:
        return None
    except _pyjwt.InvalidTokenError:
        return None


# ============================================================
# 公共 API
# ============================================================

def create_access_token(
    user_id: str,
    secret_key: str,
    expires_delta: Optional[int] = None,
    extra_data: Optional[Dict[str, Any]] = None,
) -> str:
    """
    生成 JWT 访问令牌

    Args:
        user_id: 用户唯一标识
        secret_key: 签名密钥
        expires_delta: 过期时间（秒），默认 3600 秒（1小时）
        extra_data: 附加到 payload 的额外数据

    Returns:
        JWT 令牌字符串
    """
    if expires_delta is None:
        expires_delta = 3600  # 默认 1 小时

    now = int(time.time())
    payload = {
        "sub": user_id,
        "iat": now,
        "exp": now + expires_delta,
    }
    if extra_data:
        payload.update(extra_data)

    if PYJWT_AVAILABLE:
        return _pyjwt_create_token(payload, secret_key)

    # 纯 HMAC+SHA256 降级实现
    header = {"alg": "HS256", "typ": "JWT"}
    header_b64 = _base64url_encode(json.dumps(header, separators=(',', ':')).encode('utf-8'))
    payload_b64 = _base64url_encode(json.dumps(payload, separators=(',', ':')).encode('utf-8'))
    message = f"{header_b64}.{payload_b64}"
    signature = _hmac_sha256_sign(message, secret_key)
    return f"{message}.{signature}"


def verify_access_token(token: str, secret_key: str) -> Optional[Dict[str, Any]]:
    """
    验证 JWT 访问令牌

    Args:
        token: JWT 令牌字符串
        secret_key: 签名密钥

    Returns:
        解码后的 payload 字典；验证失败或过期时返回 None
    """
    if PYJWT_AVAILABLE:
        return _pyjwt_verify_token(token, secret_key)

    # 纯 HMAC+SHA256 降级实现
    try:
        parts = token.split('.')
        if len(parts) != 3:
            return None
        message = f"{parts[0]}.{parts[1]}"
        signature = parts[2]

        # 验证签名
        if not _hmac_sha256_verify(message, signature, secret_key):
            return None

        # 解码 payload
        payload = json.loads(_base64url_decode(parts[1]).decode('utf-8'))

        # 检查过期时间
        exp = payload.get("exp", 0)
        if exp and time.time() > exp:
            return None

        return payload
    except Exception:
        return None


def get_current_user(
    authorization_header: Optional[str],
    secret_key: str,
) -> Optional[Dict[str, Any]]:
    """
    从请求的 Authorization 头中提取并验证用户信息

    Args:
        authorization_header: HTTP Authorization 头值，格式为 "Bearer <token>"
        secret_key: 签名密钥

    Returns:
        用户 payload 字典；验证失败时返回 None
    """
    if not authorization_header:
        return None

    # 解析 Bearer Token
    parts = authorization_header.split()
    if len(parts) != 2 or parts[0].lower() != "bearer":
        return None

    token = parts[1]
    payload = verify_access_token(token, secret_key)
    return payload
