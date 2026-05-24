"""
LLM编程辅助系统 - API 服务器模块
====================================

本模块提供基于 FastAPI 的 RESTful API 和 WebSocket 接口，
支持代码分析、代码生成、代码调试和 AI 对话等功能。

核心组件：
  - FastAPI 应用实例 (app): HTTP 服务器，处理 REST 请求
  - ConnectionManager: WebSocket 连接管理器，维护活跃连接列表
  - Pydantic 数据模型: 请求验证（ChatRequest/CodeAnalysisRequest 等）
  - 生命周期管理器 (lifespan): 启动时初始化 AI 引擎，关闭时清理资源

API 端点：
  - GET  /: 根路径（返回 Web 界面或 API 信息）
  - GET  /api/status: 系统状态查询
  - POST /api/chat: AI 对话
  - POST /api/analyze: 代码分析
  - POST /api/generate: 代码生成
  - POST /api/debug: 代码调试
  - WS   /ws/chat: WebSocket 实时对话

安全特性：
  - 请求体大小限制（10MB）
  - CORS 跨域控制
  - 全局异常处理（不暴露内部错误细节）
  - Pydantic 数据验证
"""

import os
import sys
import json
import asyncio
import logging
import traceback
import time
import uuid
import secrets
import threading
from functools import wraps
from typing import Callable, Optional, Dict, Any, List, Awaitable
from starlette.middleware.base import BaseHTTPMiddleware, RequestResponseEndpoint
from starlette.responses import Response
from datetime import datetime
from pathlib import Path
from contextlib import asynccontextmanager

# 确保 llm_coding_assistant 目录在搜索路径中，以便 `from core import`
# 能正确找到 core 子模块。若通过 `python -m llm_coding_assistant.api_server`
# 启动则不需要手动修改 sys.path。
# 注意：修改 sys.path 会影响整个 Python 进程的模块搜索顺序，
# 仅在包内相对导入无法正常工作时才需要此变通方案。
_FRAME_DIR = Path(__file__).resolve().parent
if _FRAME_DIR not in sys.path:
    sys.path.insert(0, str(_FRAME_DIR))

try:
    from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect, Request
    from fastapi.middleware.cors import CORSMiddleware
    from fastapi.responses import HTMLResponse, JSONResponse
    from fastapi.staticfiles import StaticFiles
    from pydantic import BaseModel, Field, field_validator
    import uvicorn
except ImportError as exc:
    raise ImportError(
        "缺少必要的依赖。请安装: pip install fastapi uvicorn pydantic"
    ) from exc

from .core import AIEngine, CodeAnalyzer

# ============================================================
# 认证模块导入
# ============================================================
from .auth import (
    create_access_token,
    verify_access_token,
    get_current_user,
    hash_password,
    verify_password,
    User,
)

# ============================================================
# 版本号
# ============================================================
__version__ = "3.3.0"

# ============================================================
# 日志配置（必须在其他模块代码之前初始化，避免 logger 在定义前使用）
# ============================================================
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(name)s - %(message)s",
)
logger = logging.getLogger("api_server")

# ============================================================
# 认证配置
# ============================================================

# JWT 签名密钥（延迟初始化，在 _validate_config 中设置）
_SECRET_KEY_ENV: str = os.getenv("JWT_SECRET_KEY", "")
SECRET_KEY: str = ""

# 令牌过期时间（秒），默认 24 小时
ACCESS_TOKEN_EXPIRE_SECONDS: int = int(os.getenv("ACCESS_TOKEN_EXPIRE_SECONDS", "86400"))

# 用户存储（生产环境应使用数据库）
# 在 _ensure_admin_exists() 中从环境变量初始化管理员账户
USERS: Dict[str, User] = {}


def _validate_config() -> None:
    """验证必要的环境变量并初始化安全配置（应在应用启动时调用）。

    检查 JWT_SECRET_KEY 是否已设置，未设置时自动生成随机密钥并打印警告。
    自动生成的密钥仅在单次进程生命周期内有效，重启后所有已签发的令牌将失效。
    """
    global SECRET_KEY

    if _SECRET_KEY_ENV:
        SECRET_KEY = _SECRET_KEY_ENV
    else:
        SECRET_KEY = secrets.token_urlsafe(32)
        logger.warning(
            "[安全警告] JWT_SECRET_KEY 环境变量未设置，已自动生成随机密钥。"
            "生产环境请通过环境变量设置固定密钥，否则每次重启后所有令牌将失效。\n"
            "设置方法:\n"
            "  Linux/macOS: export JWT_SECRET_KEY='你的密钥'\n"
            "  Windows:     set JWT_SECRET_KEY=你的密钥\n"
            "  Docker:      -e JWT_SECRET_KEY='你的密钥'\n"
            "建议使用 openssl rand -hex 32 生成高强度密钥。",
        )


def _ensure_admin_exists() -> None:
    """确保至少存在一个管理员账户（应在应用启动时调用）。

    从环境变量 LV00_ADMIN_USER 和 LV00_ADMIN_PASSWORD 读取管理员凭据。
    两个变量都必须设置才会创建管理员账户。
    未设置时打印警告，用户可通过 /api/auth/register 注册账户。
    密码使用 PBKDF2-HMAC-SHA256 加盐哈希（参见 auth.hash_password）。
    """
    admin_user = os.environ.get("LV00_ADMIN_USER", "")
    admin_pass = os.environ.get("LV00_ADMIN_PASSWORD", "")
    if admin_user and admin_pass:
        USERS[admin_user] = User(
            id="user-001",
            username=admin_user,
            role="admin",
            password_hash=hash_password(admin_pass),
        )
    else:
        logger.warning(
            "LV00_ADMIN_USER / LV00_ADMIN_PASSWORD 未设置，"
            "未创建默认管理员账户。请通过 /api/auth/register 注册账户，"
            "或设置环境变量后重启。",
        )

# ============================================================
# 配置常量
# ============================================================

# 允许的最大请求体大小（10MB）
MAX_REQUEST_BODY_SIZE = 10 * 1024 * 1024

# 速率限制配置：每个IP每分钟最多允许的请求数
RATE_LIMIT_MAX_REQUESTS = 60
# 速率限制时间窗口（秒）
RATE_LIMIT_WINDOW_SECONDS = 60

# 允许的CORS来源列表
# [安全说明] 以下默认值仅适用于本地开发环境，允许 localhost 和 127.0.0.1 的常见开发端口。
# 生产环境部署时，务必通过环境变量 CORS_ORIGINS 设置为实际的前端域名，
# 例如: CORS_ORIGINS=https://your-production-domain.com
# 注意：对每个来源进行 strip() 处理，避免因逗号后空格导致的匹配失败
ALLOWED_ORIGINS: List[str] = [
    origin.strip() 
    for origin in os.getenv(
        "CORS_ORIGINS",
        "http://localhost:8000,http://localhost:3000,http://127.0.0.1:8000,http://127.0.0.1:3000",
    ).split(",")
]

# WebSocket连接管理器
class ConnectionManager:
    """WebSocket连接管理器，维护所有活跃连接"""

    def __init__(self) -> None:
        """初始化连接管理器，创建空的连接列表和异步锁"""
        self.active_connections: List[WebSocket] = []
        self._lock: asyncio.Lock = asyncio.Lock()

    async def connect(self, websocket: WebSocket) -> None:
        """接受新的WebSocket连接"""
        await websocket.accept()
        async with self._lock:
            self.active_connections.append(websocket)
        logger.info("新WebSocket连接已建立，当前连接数: %d", len(self.active_connections))

    async def disconnect(self, websocket: WebSocket) -> None:
        """断开WebSocket连接"""
        async with self._lock:
            if websocket in self.active_connections:
                self.active_connections.remove(websocket)
        logger.info("WebSocket连接已断开，当前连接数: %d", len(self.active_connections))

    async def broadcast(self, message: str) -> None:
        """向所有活跃连接广播消息"""
        async with self._lock:
            connections = list(self.active_connections)
        for connection in connections:
            try:
                await connection.send_text(message)
            except (RuntimeError, ConnectionError):
                # 记录发送失败的日志，便于排查连接异常
                logger.warning(
                    "向WebSocket连接广播消息失败，可能该连接已断开: %s",
                    connection,
                    exc_info=True,
                )


# ============================================================
# Pydantic 数据模型（请求验证）
# ============================================================

class ChatRequest(BaseModel):
    """AI对话请求模型"""
    message: str = Field(..., min_length=1, max_length=32000, description="用户消息内容")
    stream: bool = Field(default=False, description="是否使用流式响应")
    provider: Optional[str] = Field(default=None, description="指定AI服务提供商")

    @field_validator("message")
    @classmethod
    def validate_message(cls, v: str) -> str:
        """验证消息内容不为空白"""
        if not v.strip():
            raise ValueError("消息内容不能为空白")
        return v


class CodeAnalysisRequest(BaseModel):
    """代码分析请求模型"""
    code: str = Field(..., min_length=1, max_length=500000, description="待分析的代码")
    filename: Optional[str] = Field(default="", description="文件名，用于语言检测")
    format: str = Field(default="markdown", description="报告输出格式")

    @field_validator("code")
    @classmethod
    def validate_code(cls, v: str) -> str:
        """验证代码内容不为空白"""
        if not v.strip():
            raise ValueError("代码内容不能为空白")
        return v

    @field_validator("format")
    @classmethod
    def validate_format(cls, v: str) -> str:
        """验证输出格式"""
        allowed_formats = {"markdown", "json", "text"}
        if v.lower() not in allowed_formats:
            raise ValueError(f"不支持的格式，可选: {', '.join(allowed_formats)}")
        return v.lower()


class CodeGenerationRequest(BaseModel):
    """代码生成请求模型"""
    description: str = Field(..., min_length=1, max_length=16000, description="代码功能描述")
    language: str = Field(default="python", description="目标编程语言")
    context: Optional[str] = Field(default="", description="上下文或参考代码")

    @field_validator("description")
    @classmethod
    def validate_description(cls, v: str) -> str:
        """验证描述内容不为空白"""
        if not v.strip():
            raise ValueError("功能描述不能为空白")
        return v


class DebugRequest(BaseModel):
    """代码调试请求模型"""
    code: str = Field(..., min_length=1, max_length=500000, description="待调试的代码")
    error_message: Optional[str] = Field(default="", description="错误信息")
    language: str = Field(default="python", description="编程语言")

    @field_validator("code")
    @classmethod
    def validate_code(cls, v: str) -> str:
        """验证代码内容不为空白"""
        if not v.strip():
            raise ValueError("代码内容不能为空白")
        return v


class LoginRequest(BaseModel):
    """登录请求模型"""
    username: str = Field(..., min_length=1, max_length=64, description="用户名")
    password: str = Field(..., min_length=1, max_length=128, description="密码")


class RegisterRequest(BaseModel):
    """注册请求模型"""
    username: str = Field(..., min_length=3, max_length=64, description="用户名")
    password: str = Field(..., min_length=6, max_length=128, description="密码")
    role: str = Field(default="user", description="用户角色")


# ============================================================
# 认证中间件：auth_required 装饰器
# ============================================================

def auth_required(func: Callable) -> Callable:
    """
    认证中间件装饰器

    要求请求头中包含有效的 Bearer Token。
    验证通过后将用户 payload 注入到 request.state.current_user。
    验证失败返回 401 Unauthorized。

    用法:
        @app.post("/api/protected")
        @auth_required
        async def protected_route(request: Request):
            user = request.state.current_user
            ...
    """
    @wraps(func)
    async def wrapper(request: Request, *args: Any, **kwargs: Any) -> Any:
        auth_header = request.headers.get("Authorization")
        user_payload = get_current_user(auth_header, SECRET_KEY)
        if user_payload is None:
            raise HTTPException(
                status_code=401,
                detail="未授权访问：请提供有效的认证令牌",
                headers={"WWW-Authenticate": "Bearer"},
            )
        # 将用户信息注入到请求状态中
        request.state.current_user = user_payload
        return await func(request, *args, **kwargs)
    return wrapper  # type: ignore[return-value]


# ============================================================
# 全局状态
# ============================================================

# AI引擎实例（在 lifespan 中初始化）
ai_engine: Optional[AIEngine] = None
# 代码分析器实例（在 lifespan 中初始化）
code_analyzer: Optional[CodeAnalyzer] = None
# WebSocket连接管理器
ws_manager = ConnectionManager()
# 应用启动时间（在 lifespan 中设置）
_app_start_time: Optional[float] = None


# ============================================================
# 应用生命周期管理
# ============================================================

@asynccontextmanager
async def lifespan(app: FastAPI):
    """
    应用生命周期管理器
    启动时初始化核心组件，关闭时清理资源。
    替代已弃用的 @app.on_event("startup") / @app.on_event("shutdown")。
    """
    global ai_engine, code_analyzer, _app_start_time

    # ---------- 启动阶段 ----------
    _app_start_time = time.time()
    logger.info("正在初始化LLM编程辅助系统...")

    # 验证安全配置并初始化管理员账户
    _validate_config()
    _ensure_admin_exists()

    try:
        ai_engine = AIEngine()
        code_analyzer = CodeAnalyzer()
        logger.info("AI引擎初始化完成，可用提供商: %s", ai_engine.list_providers())
        logger.info("代码分析器初始化完成")
    except (ImportError, OSError, RuntimeError, ValueError) as exc:
        logger.error("核心组件初始化失败: %s", traceback.format_exc())
        # 记录具体的失败原因，便于运维排查
        logger.error(
            "AI引擎初始化失败原因: %s; 代码分析器初始化失败原因: %s。"
            "系统将以降级模式运行，AI对话和代码分析功能将不可用。",
            type(exc).__name__,
            str(exc),
        )
        # 初始化失败不应阻止应用启动，但功能将不可用
        ai_engine = None
        code_analyzer = None

    logger.info("LLM编程辅助系统已启动")

    # 将控制权交给应用
    yield

    # ---------- 关闭阶段 ----------
    logger.info("正在关闭LLM编程辅助系统...")
    # 断开所有WebSocket连接
    for conn in list(ws_manager.active_connections):
        await conn.close()
    ws_manager.active_connections.clear()
    ai_engine = None
    code_analyzer = None
    logger.info("LLM编程辅助系统已关闭")


# ============================================================
# FastAPI 应用实例
# ============================================================

app = FastAPI(
    title="LLM编程辅助系统",
    description="智能代码分析、生成、调试助手 - 提供RESTful API和WebSocket接口",
    version=__version__,
    lifespan=lifespan,
)

# CORS中间件 - 使用具体来源列表，避免安全风险
app.add_middleware(
    CORSMiddleware,
    allow_origins=ALLOWED_ORIGINS,
    allow_credentials=True,
    allow_methods=["GET", "POST", "OPTIONS"],
    allow_headers=["Content-Type", "Authorization", "X-Request-ID"],
)


# ============================================================
# 请求大小限制中间件
# ============================================================

@app.middleware("http")
async def limit_request_size(request: Request, call_next: RequestResponseEndpoint) -> Response:
    """
    请求体大小限制中间件
    超过 MAX_REQUEST_BODY_SIZE 的请求将被拒绝，防止内存耗尽攻击。
    """
    content_length = request.headers.get("content-length")
    if content_length:
        try:
            size = int(content_length)
            if size > MAX_REQUEST_BODY_SIZE:
                logger.warning("请求体过大: %d 字节 (限制: %d 字节)", size, MAX_REQUEST_BODY_SIZE)
                return JSONResponse(
                    status_code=413,
                    content={"detail": f"请求体过大，最大允许 {MAX_REQUEST_BODY_SIZE // (1024*1024)}MB"},
                )
        except (ValueError, TypeError):
            pass
    return await call_next(request)


# ============================================================
# 速率限制数据结构（基于IP的滑动窗口计数器）
# ============================================================

# 存储格式: { "ip_address": [timestamp1, timestamp2, ...] }
# 仅保留时间窗口内的请求时间戳，超出窗口的记录在每次检查时清理
_rate_limit_store: Dict[str, List[float]] = {}
# 上次全局清理的时间戳
_last_rate_limit_cleanup: float = 0.0
# 线程安全锁：保护速率限制存储的并发访问
_rate_limit_lock: threading.Lock = threading.Lock()
# 速率限制存储中IP数量的上限，超过时触发全局清理
RATE_LIMIT_MAX_IPS = 10000
# 全局清理的最小间隔（秒）
RATE_LIMIT_CLEANUP_INTERVAL = 60


def _cleanup_rate_limit(ip: str, now: float) -> None:
    """
    清理指定IP超出时间窗口的旧请求记录。

    注意：调用此函数前必须持有 _rate_limit_lock。

    Args:
        ip: 客户端IP地址。
        now: 当前时间戳（秒）。
    """
    cutoff = now - RATE_LIMIT_WINDOW_SECONDS
    _rate_limit_store[ip] = [
        ts for ts in _rate_limit_store.get(ip, []) if ts > cutoff
    ]


def _cleanup_all_expired_rate_limits(now: float) -> None:
    """
    全局清理所有IP的过期速率限制记录。
    同时移除已无任何记录的IP条目，释放内存。
    清理完成后更新上次清理时间戳。

    注意：调用此函数前必须持有 _rate_limit_lock。

    Args:
        now: 当前时间戳（秒）。
    """
    global _last_rate_limit_cleanup
    cutoff = now - RATE_LIMIT_WINDOW_SECONDS
    expired_ips = []
    for ip, timestamps in _rate_limit_store.items():
        filtered = [ts for ts in timestamps if ts > cutoff]
        if filtered:
            _rate_limit_store[ip] = filtered
        else:
            expired_ips.append(ip)
    for ip in expired_ips:
        del _rate_limit_store[ip]
    _last_rate_limit_cleanup = now
    if expired_ips:
        logger.info("速率限制全局清理完成，移除 %d 个过期IP记录", len(expired_ips))


# ============================================================
# 速率限制中间件
# ============================================================

@app.middleware("http")
async def rate_limit_middleware(request: Request, call_next: RequestResponseEndpoint) -> Response:
    """
    基于IP的速率限制中间件
    使用滑动窗口算法，限制每个IP在指定时间窗口内的请求数量。
    超过限制的请求将返回429状态码。
    使用线程锁确保多线程环境下的安全访问。
    """
    # 仅对HTTP请求进行速率限制，WebSocket连接不受此限制
    if request.url.path.startswith("/ws/"):
        return await call_next(request)

    client_ip = request.headers.get("X-Forwarded-For", request.client.host if request.client else "unknown").split(",")[0].strip() if request.client else "unknown"
    now = time.time()

    # 使用线程锁保护速率限制存储的并发访问
    with _rate_limit_lock:
        # 定期全局清理：每 RATE_LIMIT_CLEANUP_INTERVAL 秒清理一次所有过期记录
        if (now - _last_rate_limit_cleanup >= RATE_LIMIT_CLEANUP_INTERVAL
                or len(_rate_limit_store) > RATE_LIMIT_MAX_IPS):
            _cleanup_all_expired_rate_limits(now)

        # 清理该IP的过期记录
        _cleanup_rate_limit(client_ip, now)

        # 获取当前窗口内的请求数
        request_times = _rate_limit_store.setdefault(client_ip, [])
        if len(request_times) >= RATE_LIMIT_MAX_REQUESTS:
            logger.warning(
                "速率限制触发: IP=%s 在 %d 秒内请求 %d 次 (限制: %d 次)",
                client_ip,
                RATE_LIMIT_WINDOW_SECONDS,
                len(request_times),
                RATE_LIMIT_MAX_REQUESTS,
            )
            return JSONResponse(
                status_code=429,
                content={
                    "detail": f"请求过于频繁，每分钟最多允许 {RATE_LIMIT_MAX_REQUESTS} 次请求，请稍后重试",
                },
            )

        # 记录本次请求时间戳
        request_times.append(now)

    return await call_next(request)


# ============================================================
# 全局异常处理器
# ============================================================

@app.exception_handler(Exception)
async def global_exception_handler(request: Request, exc: Exception):
    """
    全局异常处理器
    捕获所有未处理的异常，避免将内部异常信息暴露给客户端。
    """
    # 记录完整的异常信息到日志（仅服务端可见）
    logger.error(
        "未处理的异常 [%s %s]: %s\n%s",
        request.method,
        request.url.path,
        str(exc),
        traceback.format_exc(),
    )
    # 返回通用错误信息给客户端，不暴露内部细节
    return JSONResponse(
        status_code=500,
        content={"detail": "服务器内部错误，请稍后重试"},
    )


# ============================================================
# 辅助函数
# ============================================================

def _check_ai_engine() -> None:
    """
    检查AI引擎是否已初始化。
    未初始化则抛出HTTP 503异常，提示客户端服务暂不可用。

    Raises:
        HTTPException: 当AI引擎为None时，返回503状态码。
    """
    if ai_engine is None:
        raise HTTPException(status_code=503, detail="AI引擎未初始化，请稍后重试")


def _check_code_analyzer() -> None:
    """
    检查代码分析器是否已初始化。
    未初始化则抛出HTTP 503异常，提示客户端服务暂不可用。

    Raises:
        HTTPException: 当代码分析器为None时，返回503状态码。
    """
    if code_analyzer is None:
        raise HTTPException(status_code=503, detail="代码分析器未初始化，请稍后重试")


def _success_response(data: Dict[str, Any]) -> Dict[str, Any]:
    """
    构造统一成功响应。

    Args:
        data: 响应数据字典，将被包含在 "data" 字段中。

    Returns:
        包含 success、data 和 timestamp 字段的标准响应字典。
    """
    return {
        "success": True,
        "data": data,
        "timestamp": datetime.now().isoformat(),
    }


def _build_analysis_prompt(code: str, language: str) -> str:
    """
    构建代码分析的提示词。

    Args:
        code: 待分析的代码内容。
        language: 编程语言名称，用于提示AI关注特定语言的特性。

    Returns:
        构建好的提示词字符串。
    """
    return (
        f"请分析以下{language}代码：\n\n"
        f"```\n{code}\n```\n\n"
        f"请从以下方面进行分析：\n"
        f"1. 代码质量评估\n"
        f"2. 潜在问题和风险\n"
        f"3. 性能优化建议\n"
        f"4. 最佳实践建议\n"
        f"5. 安全性检查"
    )


def _build_generation_prompt(requirement: str, language: str) -> str:
    """
    构建代码生成的提示词。

    Args:
        requirement: 代码功能描述。
        language: 目标编程语言。

    Returns:
        构建好的提示词字符串。
    """
    return (
        f"请根据以下描述生成{language}代码：\n\n"
        f"描述：{requirement}\n\n"
        f"要求：\n"
        f"1. 生成完整、可运行的代码\n"
        f"2. 添加必要的注释说明\n"
        f"3. 遵循最佳实践\n"
        f"4. 处理边界情况和错误\n\n"
        f"请直接输出代码，使用markdown代码块包裹。"
    )


def _build_debug_prompt(code: str, error_message: str, language: str) -> str:
    """
    构建代码调试的提示词。

    Args:
        code: 待调试的代码内容。
        error_message: 错误信息，为空时将提示AI主动查找bug。
        language: 编程语言名称。

    Returns:
        构建好的提示词字符串。
    """
    if error_message and error_message.strip():
        error_section = f"\n错误信息：{error_message}\n"
    else:
        error_section = "\n代码似乎有bug，请帮助找出问题。\n"

    return (
        f"请帮助调试以下{language}代码：\n\n"
        f"```\n{code}\n```\n"
        f"{error_section}\n"
        f"请：\n"
        f"1. 分析问题原因\n"
        f"2. 指出具体错误位置\n"
        f"3. 提供修复方案\n"
        f"4. 解释为什么会出现这个问题"
    )


# ============================================================
# 路由：认证
# ============================================================

@app.post("/api/auth/login", tags=["认证"])
async def login(request: LoginRequest):
    """
    用户登录

    接受 username/password，验证后返回 JWT 访问令牌。
    """
    user = USERS.get(request.username)
    if user is None:
        raise HTTPException(
            status_code=401,
            detail="用户名或密码错误",
        )

    # 验证密码哈希
    if not verify_password(request.password, user.password_hash):
        raise HTTPException(
            status_code=401,
            detail="用户名或密码错误",
        )

    # 生成 JWT 令牌
    token = create_access_token(
        user_id=user.id,
        secret_key=SECRET_KEY,
        expires_delta=ACCESS_TOKEN_EXPIRE_SECONDS,
        extra_data={
            "username": user.username,
            "role": user.role,
        },
    )

    return _success_response({
        "access_token": token,
        "token_type": "bearer",
        "user": user.to_dict(),
    })


# ============================================================
# 注册端点速率限制（基于内存的IP计数器，每分钟最多5次注册）
# ============================================================

# 存储格式: { "ip_address": [timestamp1, timestamp2, ...] }
_register_rate_limit_store: Dict[str, List[float]] = {}
# 注册速率限制：每个IP每分钟最多允许的注册次数
REGISTER_RATE_LIMIT_MAX = 5
# 注册速率限制时间窗口（秒）
REGISTER_RATE_LIMIT_WINDOW = 60


def _check_register_rate_limit(client_ip: str) -> bool:
    """
    检查指定IP是否超过注册速率限制。

    Args:
        client_ip: 客户端IP地址

    Returns:
        True 表示允许注册，False 表示超过限制
    """
    now = time.time()
    cutoff = now - REGISTER_RATE_LIMIT_WINDOW
    # 清理过期记录
    timestamps = _register_rate_limit_store.get(client_ip, [])
    timestamps = [ts for ts in timestamps if ts > cutoff]
    _register_rate_limit_store[client_ip] = timestamps

    if len(timestamps) >= REGISTER_RATE_LIMIT_MAX:
        logger.warning(
            "注册速率限制触发: IP=%s 在 %d 秒内注册 %d 次 (限制: %d 次)",
            client_ip,
            REGISTER_RATE_LIMIT_WINDOW,
            len(timestamps),
            REGISTER_RATE_LIMIT_MAX,
        )
        return False

    # 记录本次请求
    timestamps.append(now)
    return True


@app.post("/api/auth/register", tags=["认证"])
async def register(request: Request, reg_req: RegisterRequest):
    """
    用户注册

    注册新用户账号并返回 JWT 访问令牌。
    用户名已存在时返回 409 错误。
    每个IP每分钟最多允许5次注册，超过限制返回 429 错误。
    """
    # 注册端点速率限制检查
    client_ip = request.client.host if request.client else "unknown"
    if not _check_register_rate_limit(client_ip):
        raise HTTPException(
            status_code=429,
            detail="注册请求过于频繁，每分钟最多允许5次注册，请稍后重试",
        )

    if reg_req.username in USERS:
        raise HTTPException(
            status_code=409,
            detail="用户名已存在",
        )

    # 创建新用户
    user_id = f"user-{uuid.uuid4().hex[:8]}"
    new_user = User(
        id=user_id,
        username=reg_req.username,
        role=reg_req.role,
        password_hash=hash_password(reg_req.password),
    )
    USERS[reg_req.username] = new_user
    logger.info("新用户注册: %s (role=%s)", reg_req.username, reg_req.role)

    # 生成 JWT 令牌
    token = create_access_token(
        user_id=user_id,
        secret_key=SECRET_KEY,
        expires_delta=ACCESS_TOKEN_EXPIRE_SECONDS,
        extra_data={
            "username": reg_req.username,
            "role": reg_req.role,
        },
    )

    return _success_response({
        "access_token": token,
        "token_type": "bearer",
        "user": new_user.to_dict(),
    })


# ============================================================
# 路由：根路径
# ============================================================

@app.get("/", tags=["根路径"])
async def root():
    """
    根路径
    如果存在前端页面则返回Web界面，否则返回API基本信息。
    """
    html_path = Path(__file__).parent / "web" / "index.html"
    if html_path.exists():
        return HTMLResponse(content=html_path.read_text(encoding="utf-8"))
    return {
        "name": "LLM编程辅助系统 API",
        "version": __version__,
        "docs": "/docs",
        "status": "/api/status",
    }


# ============================================================
# 路由：系统状态
# ============================================================

@app.get("/api/status", tags=["系统"])
async def get_status():
    """
    获取系统状态
    返回服务运行状态、AI引擎信息、版本号等。
    当AI引擎不可用时，仍返回基本状态信息（版本号、运行时间等），
    并将AI引擎状态标注为不可用。
    """
    # 计算运行时间
    uptime_seconds = 0
    if _app_start_time is not None:
        uptime_seconds = int(time.time() - _app_start_time)

    # 构建基本状态信息
    status_data: Dict[str, Any] = {
        "status": "online",
        "version": __version__,
        "uptime_seconds": uptime_seconds,
        "active_ws_connections": len(ws_manager.active_connections),
    }

    # 根据AI引擎可用性补充信息
    if ai_engine is not None:
        status_data["ai_engine"] = ai_engine.get_status()
    else:
        status_data["ai_engine"] = {
            "status": "unavailable",
            "message": "AI引擎未初始化，系统以降级模式运行",
        }

    return _success_response(status_data)


# ============================================================
# 路由：AI对话
# ============================================================

@app.post("/api/chat", tags=["AI对话"])
@auth_required
async def chat(request: Request, chat_req: ChatRequest):
    """
    AI对话接口
    发送消息给AI引擎，获取编程相关的回答。
    流式响应请使用WebSocket连接 /ws/chat。
    """
    _check_ai_engine()

    if chat_req.stream:
        return JSONResponse(
            status_code=200,
            content={
                "success": True,
                "message": "流式响应请使用WebSocket连接 /ws/chat",
                "timestamp": datetime.now().isoformat(),
            },
        )

    try:
        response = await ai_engine.chat(
            message=chat_req.message,
            provider=chat_req.provider,
        )
        return _success_response({"response": response})
    except (RuntimeError, ConnectionError, ValueError, KeyError) as exc:
        logger.error("AI对话失败: %s\n%s", exc, traceback.format_exc())
        raise HTTPException(status_code=500, detail="AI对话处理失败，请稍后重试")


# ============================================================
# 路由：代码分析
# ============================================================

@app.post("/api/analyze", tags=["代码分析"])
@auth_required
async def analyze_code(request: Request, analysis_req: CodeAnalysisRequest):
    """
    代码分析接口
    对提交的代码进行静态分析，返回问题列表、代码度量指标和改进建议。
    """
    _check_code_analyzer()

    try:
        result = code_analyzer.analyze(analysis_req.code, analysis_req.filename)
        report = code_analyzer.generate_report(result, analysis_req.format)
        return _success_response({
            "result": result.to_dict(),
            "report": report,
        })
    except (ValueError, RuntimeError, KeyError) as exc:
        logger.error("代码分析失败: %s\n%s", exc, traceback.format_exc())
        raise HTTPException(status_code=500, detail="代码分析处理失败，请稍后重试")


# ============================================================
# 路由：代码生成
# ============================================================

@app.post("/api/generate", tags=["代码生成"])
@auth_required
async def generate_code(request: Request, gen_req: CodeGenerationRequest):
    """
    代码生成接口
    根据功能描述和可选的上下文代码，使用AI生成目标语言的代码。
    """
    _check_ai_engine()

    try:
        # 构建代码生成提示词
        context_section = ""
        if gen_req.context and gen_req.context.strip():
            context_section = f"\n上下文/参考代码：\n```\n{gen_req.context}\n```\n"

        prompt = _build_generation_prompt(gen_req.description, gen_req.language)
        # 如果有上下文代码，追加到提示词中
        if context_section:
            prompt = prompt.replace(
                f"描述：{gen_req.description}\n\n",
                f"描述：{gen_req.description}\n{context_section}\n",
            )

        response = await ai_engine.chat(prompt)
        return _success_response({
            "code": response,
            "language": gen_req.language,
        })
    except (RuntimeError, ConnectionError, ValueError, KeyError) as exc:
        logger.error("代码生成失败: %s\n%s", exc, traceback.format_exc())
        raise HTTPException(status_code=500, detail="代码生成处理失败，请稍后重试")


# ============================================================
# 路由：代码调试
# ============================================================

@app.post("/api/debug", tags=["代码调试"])
@auth_required
async def debug_code(request: Request, debug_req: DebugRequest):
    """
    代码调试接口
    提交代码和可选的错误信息，AI将帮助分析问题原因并给出修复建议。
    """
    _check_ai_engine()

    try:
        prompt = _build_debug_prompt(debug_req.code, debug_req.error_message, debug_req.language)

        response = await ai_engine.chat(prompt)
        return _success_response({
            "analysis": response,
            "language": debug_req.language,
        })
    except (RuntimeError, ConnectionError, ValueError, KeyError) as exc:
        logger.error("代码调试失败: %s\n%s", exc, traceback.format_exc())
        raise HTTPException(status_code=500, detail="代码调试处理失败，请稍后重试")


# ============================================================
# WebSocket：实时对话
# ============================================================

@app.websocket("/ws/chat")
async def websocket_chat(websocket: WebSocket):
    """
    WebSocket实时对话接口（需要认证）
    
    认证方式：通过查询参数传递 JWT token
    连接示例：ws://host/ws/chat?token=<your_jwt_token>
    支持流式AI对话，客户端发送JSON消息，服务端逐块返回响应。
    消息格式：
      发送: {"message": "...", "provider": "可选"}
      接收: {"type": "chunk"|"done"|"error", "content": "..."}
    """
    # WebSocket 认证：从查询参数获取 token
    token = websocket.query_params.get("token")
    if not token:
        await websocket.close(code=4001, reason="缺少认证令牌")
        return
    
    user_payload = get_current_user(f"Bearer {token}", SECRET_KEY)
    if user_payload is None:
        await websocket.close(code=4001, reason="认证令牌无效或已过期")
        return
    
    await ws_manager.connect(websocket)

    try:
        while True:
            # 接收客户端消息
            raw_data = await websocket.receive_text()
            try:
                data = json.loads(raw_data)
            except json.JSONDecodeError:
                await websocket.send_text(json.dumps({
                    "type": "error",
                    "content": "消息格式错误，请发送JSON格式数据",
                }))
                continue

            message = data.get("message", "").strip()
            if not message:
                await websocket.send_text(json.dumps({
                    "type": "error",
                    "content": "消息内容不能为空",
                }))
                continue

            provider = data.get("provider")

            # 检查AI引擎是否可用
            if ai_engine is None:
                await websocket.send_text(json.dumps({
                    "type": "error",
                    "content": "AI引擎未初始化，请稍后重试",
                }))
                continue

            try:
                # 调用AI引擎（流式模式）
                response = await ai_engine.chat(
                    message=message,
                    provider=provider,
                    stream=True,
                )
                # 流式返回结果
                if isinstance(response, str):
                    # 非流式回退：一次性发送
                    await websocket.send_text(json.dumps({
                        "type": "chunk",
                        "content": response,
                    }))
                else:
                    # 流式生成器：逐块发送
                    async for chunk in response:
                        await websocket.send_text(json.dumps({
                            "type": "chunk",
                            "content": chunk,
                        }))

                await websocket.send_text(json.dumps({
                    "type": "done",
                    "content": "",
                }))

            except (RuntimeError, ConnectionError, ValueError, KeyError) as exc:
                logger.error("WebSocket对话处理失败: %s\n%s", exc, traceback.format_exc())
                await websocket.send_text(json.dumps({
                    "type": "error",
                    "content": "处理消息时发生错误，请稍后重试",
                }))

    except WebSocketDisconnect:
        logger.info("客户端主动断开WebSocket连接")
    except (RuntimeError, ConnectionError, ValueError) as exc:
        logger.error("WebSocket异常: %s\n%s", exc, traceback.format_exc())
    finally:
        await ws_manager.disconnect(websocket)


# ============================================================
# 启动入口
# ============================================================

if __name__ == "__main__":
    port = int(os.getenv("API_PORT", "8000"))
    host = os.getenv("API_HOST", "127.0.0.1")
    logger.info("正在启动API服务器 %s:%d ...", host, port)
    # 使用 app 对象引用而非字符串 "api_server:app"，
    # 避免模块名硬编码导致的维护问题，并支持作为模块导入后复用。
    uvicorn.run(
        app,
        host=host,
        port=port,
        reload=False,
        log_level="info",
    )
