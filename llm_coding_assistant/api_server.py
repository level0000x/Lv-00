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
from typing import Optional, Dict, Any, List
from datetime import datetime
from pathlib import Path
from contextlib import asynccontextmanager

# 确保 llm_coding_assistant 目录在搜索路径中，以便 `from core import`
# 能正确找到 core 子模块。若通过 `python -m llm_coding_assistant.api_server`
# 启动则不需要手动修改 sys.path。
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

from core import AIEngine, CodeAnalyzer

# ============================================================
# 日志配置
# ============================================================
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(name)s - %(message)s",
)
logger = logging.getLogger("api_server")

# ============================================================
# 配置常量
# ============================================================

# 允许的最大请求体大小（10MB）
MAX_REQUEST_BODY_SIZE = 10 * 1024 * 1024

# 速率限制配置：每个IP每分钟最多允许的请求数
RATE_LIMIT_MAX_REQUESTS = 60
# 速率限制时间窗口（秒）
RATE_LIMIT_WINDOW_SECONDS = 60

# 允许的CORS来源列表（生产环境应从配置文件或环境变量读取）
ALLOWED_ORIGINS: List[str] = os.getenv(
    "CORS_ORIGINS",
    "http://localhost:8000,http://localhost:3000,http://127.0.0.1:8000,http://127.0.0.1:3000",
).split(",")

# WebSocket连接管理器
class ConnectionManager:
    """WebSocket连接管理器，维护所有活跃连接"""

    def __init__(self) -> None:
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
            except Exception:
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


# ============================================================
# 全局状态
# ============================================================

# AI引擎实例（在 lifespan 中初始化）
ai_engine: Optional[AIEngine] = None
# 代码分析器实例（在 lifespan 中初始化）
code_analyzer: Optional[CodeAnalyzer] = None
# WebSocket连接管理器
ws_manager = ConnectionManager()


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
    global ai_engine, code_analyzer

    # ---------- 启动阶段 ----------
    logger.info("正在初始化LLM编程辅助系统...")
    try:
        ai_engine = AIEngine()
        code_analyzer = CodeAnalyzer()
        logger.info("AI引擎初始化完成，可用提供商: %s", ai_engine.list_providers())
        logger.info("代码分析器初始化完成")
    except Exception as exc:
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
    version="1.0.0",
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
async def limit_request_size(request: Request, call_next):
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


def _cleanup_rate_limit(ip: str, now: float) -> None:
    """清理指定IP超出时间窗口的旧请求记录"""
    cutoff = now - RATE_LIMIT_WINDOW_SECONDS
    _rate_limit_store[ip] = [
        ts for ts in _rate_limit_store.get(ip, []) if ts > cutoff
    ]


# ============================================================
# 速率限制中间件
# ============================================================

@app.middleware("http")
async def rate_limit_middleware(request: Request, call_next):
    """
    基于IP的速率限制中间件
    使用滑动窗口算法，限制每个IP在指定时间窗口内的请求数量。
    超过限制的请求将返回429状态码。
    """
    # 仅对HTTP请求进行速率限制，WebSocket连接不受此限制
    if request.url.path.startswith("/ws/"):
        return await call_next(request)

    client_ip = request.client.host if request.client else "unknown"
    now = time.time()

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
    """检查AI引擎是否已初始化，未初始化则抛出503异常"""
    if ai_engine is None:
        raise HTTPException(status_code=503, detail="AI引擎未初始化，请稍后重试")


def _check_code_analyzer() -> None:
    """检查代码分析器是否已初始化，未初始化则抛出503异常"""
    if code_analyzer is None:
        raise HTTPException(status_code=503, detail="代码分析器未初始化，请稍后重试")


def _success_response(data: Dict[str, Any]) -> Dict[str, Any]:
    """构造统一成功响应"""
    return {
        "success": True,
        "data": data,
        "timestamp": datetime.now().isoformat(),
    }


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
        "version": "1.0.0",
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
    """
    _check_ai_engine()
    return _success_response({
        "status": "online",
        "version": "1.0.0",
        "ai_engine": ai_engine.get_status(),
        "active_ws_connections": len(ws_manager.active_connections),
    })


# ============================================================
# 路由：AI对话
# ============================================================

@app.post("/api/chat", tags=["AI对话"])
async def chat(request: ChatRequest):
    """
    AI对话接口
    发送消息给AI引擎，获取编程相关的回答。
    流式响应请使用WebSocket连接 /ws/chat。
    """
    _check_ai_engine()

    if request.stream:
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
            message=request.message,
            provider=request.provider,
        )
        return _success_response({"response": response})
    except Exception:
        logger.error("AI对话失败: %s", traceback.format_exc())
        raise HTTPException(status_code=500, detail="AI对话处理失败，请稍后重试")


# ============================================================
# 路由：代码分析
# ============================================================

@app.post("/api/analyze", tags=["代码分析"])
async def analyze_code(request: CodeAnalysisRequest):
    """
    代码分析接口
    对提交的代码进行静态分析，返回问题列表、代码度量指标和改进建议。
    """
    _check_code_analyzer()

    try:
        result = code_analyzer.analyze(request.code, request.filename)
        report = code_analyzer.generate_report(result, request.format)
        return _success_response({
            "result": result.to_dict(),
            "report": report,
        })
    except Exception:
        logger.error("代码分析失败: %s", traceback.format_exc())
        raise HTTPException(status_code=500, detail="代码分析处理失败，请稍后重试")


# ============================================================
# 路由：代码生成
# ============================================================

@app.post("/api/generate", tags=["代码生成"])
async def generate_code(request: CodeGenerationRequest):
    """
    代码生成接口
    根据功能描述和可选的上下文代码，使用AI生成目标语言的代码。
    """
    _check_ai_engine()

    try:
        # 构建代码生成提示词
        context_section = ""
        if request.context and request.context.strip():
            context_section = f"\n上下文/参考代码：\n```\n{request.context}\n```\n"

        prompt = (
            f"请根据以下描述生成{request.language}代码：\n\n"
            f"描述：{request.description}\n"
            f"{context_section}\n"
            f"要求：\n"
            f"1. 生成完整、可运行的代码\n"
            f"2. 添加必要的注释说明\n"
            f"3. 遵循最佳实践\n"
            f"4. 处理边界情况和错误\n\n"
            f"请直接输出代码，使用markdown代码块包裹。"
        )

        response = await ai_engine.chat(prompt)
        return _success_response({
            "code": response,
            "language": request.language,
        })
    except Exception:
        logger.error("代码生成失败: %s", traceback.format_exc())
        raise HTTPException(status_code=500, detail="代码生成处理失败，请稍后重试")


# ============================================================
# 路由：代码调试
# ============================================================

@app.post("/api/debug", tags=["代码调试"])
async def debug_code(request: DebugRequest):
    """
    代码调试接口
    提交代码和可选的错误信息，AI将帮助分析问题原因并给出修复建议。
    """
    _check_ai_engine()

    try:
        error_section = ""
        if request.error_message and request.error_message.strip():
            error_section = f"\n错误信息：{request.error_message}\n"
        else:
            error_section = "\n代码似乎有bug，请帮助找出问题。\n"

        prompt = (
            f"请帮助调试以下{request.language}代码：\n\n"
            f"```\n{request.code}\n```\n"
            f"{error_section}\n"
            f"请：\n"
            f"1. 分析问题原因\n"
            f"2. 指出具体错误位置\n"
            f"3. 提供修复方案\n"
            f"4. 解释为什么会出现这个问题"
        )

        response = await ai_engine.chat(prompt)
        return _success_response({
            "analysis": response,
            "language": request.language,
        })
    except Exception:
        logger.error("代码调试失败: %s", traceback.format_exc())
        raise HTTPException(status_code=500, detail="代码调试处理失败，请稍后重试")


# ============================================================
# WebSocket：实时对话
# ============================================================

@app.websocket("/ws/chat")
async def websocket_chat(websocket: WebSocket):
    """
    WebSocket实时对话接口
    支持流式AI对话，客户端发送JSON消息，服务端逐块返回响应。
    消息格式：
      发送: {"message": "...", "provider": "可选"}
      接收: {"type": "chunk"|"done"|"error", "content": "..."}
    """
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

            except Exception:
                logger.error("WebSocket对话处理失败: %s", traceback.format_exc())
                await websocket.send_text(json.dumps({
                    "type": "error",
                    "content": "处理消息时发生错误，请稍后重试",
                }))

    except WebSocketDisconnect:
        logger.info("客户端主动断开WebSocket连接")
    except Exception:
        logger.error("WebSocket异常: %s", traceback.format_exc())
    finally:
        ws_manager.disconnect(websocket)


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
