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

import os
import hmac
import hashlib
import json
import logging
import time
import base64
from typing import Any
from dataclasses import dataclass

# ============================================================
# 尝试导入 PyJWT，不可用时降级为纯 HMAC+SHA256 实现
# ============================================================
try:
    import jwt as _pyjwt
    PYJWT_AVAILABLE = True
except ImportError:
    PYJWT_AVAILABLE = False


# ============================================================
# 密码哈希常量
# ============================================================

# PBKDF2 迭代次数（符合 OWASP 推荐）
PBKDF2_ITERATIONS = 100000


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
        password_hash: PBKDF2-HMAC-SHA256 加盐哈希后的密码（格式: salt:hash）
    """
    id: str
    username: str
    role: str = "user"
    password_hash: str = ""

    def to_dict(self) -> dict[str, Any]:
        """转为可序列化的字典（不含密码哈希）"""
        return {
            "id": self.id,
            "username": self.username,
            "role": self.role,
        }


# ============================================================
# 密码哈希工具（PBKDF2-HMAC-SHA256，加盐）
# ============================================================

def hash_password(password: str) -> str:
    """
    使用 PBKDF2-HMAC-SHA256 加盐哈希密码

    返回格式为 "salt:hash" 的字符串，salt 和 hash 均为十六进制编码。
    迭代次数为 PBKDF2_ITERATIONS，符合 OWASP 推荐。

    Args:
        password: 明文密码

    Returns:
        格式为 "salt:hash" 的密码哈希字符串
    """
    salt = os.urandom(32).hex()
    key = hashlib.pbkdf2_hmac(
        'sha256',
        password.encode('utf-8'),
        salt.encode('utf-8'),
        PBKDF2_ITERATIONS,
    )
    return salt + ':' + key.hex()


def verify_password(password: str, password_hash: str) -> bool:
    """
    验证密码是否与存储的哈希匹配

    从 password_hash 中提取 salt，使用相同的 PBKDF2-HMAC-SHA256
    算法重新计算哈希，并使用 hmac.compare_digest 进行时间恒定比较。

    Args:
        password: 待验证的明文密码
        password_hash: 存储的密码哈希（格式: salt:hash）

    Returns:
        True 表示密码匹配
    """
    try:
        salt, hash_value = password_hash.split(':', 1)
        key = hashlib.pbkdf2_hmac(
            'sha256',
            password.encode('utf-8'),
            salt.encode('utf-8'),
            PBKDF2_ITERATIONS,
        )
        return hmac.compare_digest(key.hex(), hash_value)
    except (ValueError, AttributeError):
        return False


# ============================================================
# JWT 令牌生成（纯 HMAC+SHA256 实现）
# ============================================================
#
# 安全说明 - 纯 HMAC+SHA256 降级实现：
#
# 本模块在 PyJWT 不可用时，提供纯 Python 的 HMAC+SHA256 JWT 实现
# 作为降级方案。此实现仅支持 HS256 算法，功能上与 PyJWT 等价，
# 但存在以下安全注意事项：
#
# 1. 算法限制：仅支持 HMAC-SHA256 (HS256)，不支持 RS256/ES256 等非对称算法。
#    非对称算法在公钥/私钥分离的场景下更安全（如第三方验证令牌），
#    HS256 仅适用于服务端自己签发和验证的场景。
#
# 2. 无算法混淆防护：PyJWT 会自动拒绝 "alg":"none" 等不安全算法，
#    本实现通过硬编码 HS256 避免了此问题，但缺少对 JWT 头部 alg 字段的
#    验证。如果未来需要支持多算法，必须添加 alg 白名单检查。
#
# 3. 无 JWK/JWKS 支持：不支持 JSON Web Key Set，无法动态获取验证密钥。
#    在微服务架构中，建议使用 PyJWT + PyJWK 以支持 JWKS 密钥轮换。
#
# 4. 无 jti (JWT ID) 声明：本实现不生成 jti 字段，无法实现令牌撤销列表。
#    如果需要令牌撤销功能，请使用 PyJWT 并配合 Redis 等存储实现黑名单。
#
# 5. 时间验证精度：使用 time.time() 进行过期检查，依赖系统时钟。
#    在分布式系统中，各节点时钟不同步可能导致令牌在某些节点提前过期
#    或延迟过期。建议使用 PyJWT 的 leeway 参数处理时钟偏差。
#
# 推荐做法：生产环境请安装 PyJWT（pip install PyJWT）以获得更完整、
# 更安全的 JWT 处理能力。纯 HMAC 实现仅用于无外部依赖的轻量级部署场景。
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

def _pyjwt_create_token(payload: dict[str, Any], secret_key: str) -> str:
    """使用 PyJWT 生成令牌"""
    return _pyjwt.encode(payload, secret_key, algorithm="HS256")


def _pyjwt_verify_token(token: str, secret_key: str) -> dict[str, Any] | None:
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
    expires_delta: int | None = None,
    extra_data: dict[str, Any] | None = None,
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
        # 默认 1 小时。注意：api_server.py 中 ACCESS_TOKEN_EXPIRE_SECONDS 默认为 86400（24小时），
        # 调用方通常会显式传入该值。此默认值仅作为未指定时的安全回退。
        expires_delta = 3600

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


def verify_access_token(token: str, secret_key: str) -> dict[str, Any] | None:
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
    except Exception as e:
        logging.getLogger(__name__).debug("Token 验证异常: %s", e)
        return None


def get_current_user(
    authorization_header: str | None,
    secret_key: str,
) -> dict[str, Any] | None:
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
