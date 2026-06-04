"""
Web 路由定义
=============

定义 Flask 应用的路由、API 端点和 SSE 流。
包含安全验证、CORS 配置和安全审计日志功能。
"""

from __future__ import annotations

import json
import os
import secrets
import time
import threading
import warnings
from collections import deque
from typing import Any, Optional

from flask import Flask, Response, jsonify, request, render_template_string

from ..core.engine import MonitorEngine
from ..core.events import Event, EventType
from ..core.config import Config
from ..utils.logger import get_logger
from ..utils.constants import (
    HTTP_OK,
    HTTP_BAD_REQUEST,
    HTTP_SERVICE_UNAVAILABLE,
    MAX_COMMAND_LENGTH,
    MAX_PROCESS_ID_LENGTH,
)
from ..utils.validators import validate_process_id, validate_command, ValidationError
from .templates import DASHBOARD_HTML

logger = get_logger(__name__)

# [安全警告] 生产环境必须通过环境变量 MONITOR_SECRET_KEY 设置密钥！
# 以下代码已移除硬编码开发密钥（旧版 _DEV_SECRET_KEY），强制要求环境变量配置。
# 硬编码密钥的安全风险：
#   1. 密钥会随源代码提交到版本控制系统（Git 等），任何有代码访问权限的人都能看到
#   2. 无法在不修改代码的情况下为不同环境（开发/测试/生产）使用不同密钥
#   3. 一旦密钥泄露，必须修改源代码并重新部署，无法快速轮换
# 开发环境如需测试，请在启动前手动设置环境变量：
#   Windows: set MONITOR_SECRET_KEY=your-dev-secret && python -m concurrent_monitor
#   Linux/Mac: MONITOR_SECRET_KEY=your-dev-secret python -m concurrent_monitor
# 生产环境请使用高强度随机密钥，建议通过密钥管理服务（如 Vault、AWS Secrets Manager）注入。


def _build_cors_headers(cors_enabled: bool, request_origin: str = "") -> dict[str, str]:
    """
    构建 CORS 响应头（动态匹配请求来源）

    根据 cors_enabled 配置决定是否添加跨域头。
    启用时从请求的 Origin header 匹配白名单，匹配则返回该 Origin，
    避免固定返回第一个 origin 导致其他合法来源被拒绝的问题。
    禁用时不添加任何 CORS 头。

    Args:
        cors_enabled: 是否启用 CORS
        request_origin: 当前请求的 Origin header 值

    Returns:
        包含 CORS 头的字典
    """
    headers: dict[str, str] = {"Cache-Control": "no-cache"}
    if cors_enabled:
        # [安全说明] CORS 来源配置
        # 旧版代码曾使用通配符 "*" 作为 Access-Control-Allow-Origin 的值，
        # 这会导致任意域名都可以跨域访问本服务的 API，存在严重安全风险：
        #   1. 恶意网站可以在用户浏览器中冒充用户调用本服务 API
        #   2. 可能导致 CSRF（跨站请求伪造）攻击
        #   3. 用户敏感数据（进程命令、输出内容）可能被第三方网站窃取
        # 当前实现从环境变量 CORS_ORIGINS 读取允许的来源列表，
        # 并根据请求的 Origin 动态匹配，确保只允许白名单中的来源访问。
        # 生产环境部署时，请务必将 CORS_ORIGINS 设置为实际的前端域名。
        allowed_origins = [
            origin.strip()
            for origin in os.environ.get(
                'CORS_ORIGINS', 'http://localhost:5000,http://127.0.0.1:5000'
            ).split(',')
            if origin.strip()
        ]
        # 动态匹配请求来源：如果请求的 Origin 在白名单中，则返回该 Origin
        if request_origin and request_origin in allowed_origins:
            headers["Access-Control-Allow-Origin"] = request_origin
        elif allowed_origins:
            # 回退：如果没有 Origin header 或不匹配，返回第一个白名单来源
            headers["Access-Control-Allow-Origin"] = allowed_origins[0]
    return headers


def _log_security_audit(action: str, process_id: str, detail: str = "") -> None:
    """
    记录安全审计日志

    用于记录关键操作（注册、停止、注销进程等）的安全审计信息，
    便于事后追溯和安全审查。

    Args:
        action: 操作类型（如 register、stop、unregister）
        process_id: 涉及的进程ID
        detail: 额外详细信息
    """
    client_ip = request.remote_addr or "unknown"
    log_msg = f"[安全审计] 操作={action}, 进程ID={process_id}, 客户端IP={client_ip}"
    if detail:
        log_msg += f", 详情={detail}"
    logger.warning(log_msg)


def create_app(engine: MonitorEngine, config: Config) -> Flask:
    """
    创建 Flask 应用实例

    初始化 Flask 应用，配置安全密钥、CORS 策略、
    SSE 事件流和所有 API 路由。

    Args:
        engine: 监控引擎实例
        config: 配置对象

    Returns:
        Flask 应用实例
    """
    app = Flask(__name__)

    # 安全密钥：优先从环境变量 MONITOR_SECRET_KEY 读取
    # 未设置时生成临时随机密钥并打印警告
    # ⚠️ 安全警告：生产环境必须通过环境变量 MONITOR_SECRET_KEY 配置密钥
    # WARNING: Production must set MONITOR_SECRET_KEY env var.
    secret_key = os.environ.get("MONITOR_SECRET_KEY")
    if not secret_key:
        secret_key = secrets.token_hex(32)
        warnings.warn(
            "MONITOR_SECRET_KEY 环境变量未设置，已生成临时密钥。"
            "生产环境请务必设置此环境变量。",
            RuntimeWarning,
            stacklevel=2
        )
    app.config["SECRET_KEY"] = secret_key

    # SSE 客户端管理
    sse_clients: list[deque] = []
    sse_lock = threading.Lock()

    # 订阅引擎事件
    def on_engine_event(event: Event) -> None:
        """引擎事件回调，将输出和系统事件广播到所有 SSE 客户端"""
        if event.type in (EventType.OUTPUT, EventType.SYSTEM):
            data = {
                "type": "output",
                "process_id": event.process_id,
                "data": event.data.to_dict() if hasattr(event.data, "to_dict") else event.data,
            }
            _broadcast_sse(data, sse_clients, sse_lock)

    engine.event_bus.subscribe(EventType.OUTPUT, on_engine_event)
    engine.event_bus.subscribe(EventType.SYSTEM, on_engine_event)

    # ========== 错误处理 ==========

    @app.errorhandler(404)
    def not_found(error: Any) -> tuple:
        """
        处理 404 未找到错误
        
        Args:
            error: Flask 传递的错误对象（未使用，但必须接受）
            
        Returns:
            包含错误信息的 JSON 响应和状态码
        """
        return jsonify({"error": "未找到", "error_code": "NOT_FOUND"}), 404

    @app.errorhandler(500)
    def internal_error(error: Any) -> tuple:
        """
        处理 500 服务器内部错误
        
        Args:
            error: Flask 传递的错误对象（未使用，但必须接受）
            
        Returns:
            包含错误信息的 JSON 响应和状态码
        """
        logger.exception("服务器内部错误")
        return jsonify({"error": "服务器内部错误", "error_code": "INTERNAL_ERROR"}), 500

    # ========== 页面路由 ==========

    @app.route("/")
    def index():
        """仪表盘主页"""
        return render_template_string(DASHBOARD_HTML)

    # ========== API 路由 ==========

    @app.route("/api/processes", methods=["GET"])
    def api_get_processes():
        """获取所有进程状态"""
        return jsonify(engine.get_summary())

    @app.route("/api/processes/<process_id>", methods=["GET"])
    def api_get_process(process_id: str):
        """获取单个进程详情"""
        proc = engine.get_process(process_id)
        if proc is None:
            return jsonify({"error": "进程不存在", "error_code": "PROCESS_NOT_FOUND"}), 404
        return jsonify(proc.to_dict())

    @app.route("/api/register", methods=["POST"])
    def api_register():
        """
        注册新进程

        对请求参数进行安全验证，包括：
        - 进程ID格式验证（只允许字母、数字、下划线、连字符，长度不超过64）
        - 命令长度验证（不超过 MAX_COMMAND_LENGTH）
        - 危险命令模式检测（rm -rf /, fork bomb 等）
        """
        body = request.get_json(silent=True)
        if body is None:
            return jsonify({"error": "无效的 JSON", "error_code": "INVALID_JSON"}), HTTP_BAD_REQUEST

        process_id = body.get("id")
        command = body.get("command")

        # 检查必填字段
        if not process_id or not command:
            missing = []
            if not process_id:
                missing.append("id")
            if not command:
                missing.append("command")
            return jsonify({
                "error": f"缺少必填字段: {', '.join(missing)}",
                "error_code": "MISSING_FIELDS",
                "missing_fields": missing,
            }), HTTP_BAD_REQUEST

        # 安全验证：验证进程ID格式（只允许字母、数字、下划线、连字符，长度不超过64）
        try:
            process_id = validate_process_id(process_id)
        except ValidationError as e:
            _log_security_audit("register", str(process_id or ""), f"进程ID验证失败: {e}")
            return jsonify({"error": str(e), "error_code": "VALIDATION_ERROR"}), HTTP_BAD_REQUEST

        # 安全验证：验证命令（长度限制、危险命令模式检测）
        try:
            command = validate_command(command)
        except ValidationError as e:
            _log_security_audit("register", process_id, f"命令验证失败: {e}")
            return jsonify({"error": str(e), "error_code": "VALIDATION_ERROR"}), HTTP_BAD_REQUEST

        try:
            engine.register_process(process_id, command)
            logger.info(f"通过 API 注册进程: {process_id}")
            # 记录安全审计日志
            _log_security_audit("register", process_id, "注册成功")
            return jsonify({"ok": True, "id": process_id})
        except ValueError as e:
            return jsonify({"error": str(e), "error_code": "VALIDATION_ERROR"}), HTTP_BAD_REQUEST
        except Exception as e:
            logger.exception(f"注册进程失败: {e}")
            return jsonify({"error": "注册失败", "error_code": "REGISTER_FAILED"}), 500

    @app.route("/api/start", methods=["POST"])
    def api_start():
        """启动进程"""
        body = request.get_json(silent=True) or {}
        timeout = body.get("timeout")
        process_ids = body.get("process_ids")

        try:
            engine.run_in_background(process_ids, timeout)
            logger.info(f"通过 API 启动进程: {process_ids or '全部'}")
            return jsonify({"ok": True, "message": "已启动"})
        except Exception as e:
            logger.exception(f"启动进程失败: {e}")
            return jsonify({"error": str(e), "error_code": "START_FAILED"}), 500

    @app.route("/api/stop/<process_id>", methods=["POST"])
    def api_stop(process_id: str):
        """
        停止指定进程

        在执行停止操作前验证进程ID格式是否合法。
        """
        # 安全验证：验证进程ID格式
        try:
            validate_process_id(process_id)
        except ValidationError as e:
            _log_security_audit("stop", process_id, f"进程ID验证失败: {e}")
            return jsonify({"error": str(e), "error_code": "VALIDATION_ERROR"}), HTTP_BAD_REQUEST

        if engine.stop_process(process_id):
            logger.info(f"通过 API 停止进程: {process_id}")
            # 记录安全审计日志
            _log_security_audit("stop", process_id, "停止成功")
            return jsonify({"ok": True})
        else:
            return jsonify({"error": "进程不存在或未运行", "error_code": "STOP_FAILED"}), HTTP_BAD_REQUEST

    @app.route("/api/stop_all", methods=["POST"])
    def api_stop_all():
        """停止所有进程"""
        count = engine.stop_all()
        logger.info(f"通过 API 停止 {count} 个进程")
        return jsonify({"ok": True, "stopped_count": count})

    @app.route("/api/unregister/<process_id>", methods=["DELETE"])
    def api_unregister(process_id: str):
        """
        注销进程

        在执行注销操作前验证进程ID格式是否合法。
        """
        # 安全验证：验证进程ID格式
        try:
            validate_process_id(process_id)
        except ValidationError as e:
            _log_security_audit("unregister", process_id, f"进程ID验证失败: {e}")
            return jsonify({"error": str(e), "error_code": "VALIDATION_ERROR"}), HTTP_BAD_REQUEST

        if engine.unregister_process(process_id):
            logger.info(f"通过 API 注销进程: {process_id}")
            # 记录安全审计日志
            _log_security_audit("unregister", process_id, "注销成功")
            return jsonify({"ok": True})
        else:
            return jsonify({"error": "进程不存在", "error_code": "PROCESS_NOT_FOUND"}), 404

    # ========== SSE 路由 ==========

    @app.route("/stream")
    def stream():
        """
        SSE 实时流端点

        根据配置决定是否添加 CORS 头。
        客户端连接数受 max_sse_clients 限制。
        """
        client_queue: deque = deque()

        # 检查客户端数上限
        with sse_lock:
            if len(sse_clients) >= config.web.max_sse_clients:
                # 达到上限时返回错误，根据 CORS 配置决定是否添加跨域头
                error_headers = _build_cors_headers(
                    config.web.cors_enabled,
                    request_origin=request.headers.get("Origin", ""),
                )
                return Response(
                    'data: {"type":"error","message":"已达到最大客户端连接数"}\n\n',
                    mimetype="text/event-stream",
                    status=HTTP_SERVICE_UNAVAILABLE,
                    headers=error_headers,
                )
            sse_clients.append(client_queue)
            logger.debug(f"新 SSE 客户端连接，当前连接数: {len(sse_clients)}")

        def generate():
            """生成 SSE 事件流，定期推送心跳和进程摘要"""
            # 发送初始摘要
            summary = engine.get_summary()
            yield f'data: {json.dumps({"type": "summary", "data": summary}, ensure_ascii=False)}\n\n'

            try:
                while True:
                    if client_queue:
                        msg = client_queue.popleft()
                        yield f'data: {json.dumps(msg, ensure_ascii=False)}\n\n'
                    else:
                        # 定期发送心跳/摘要
                        summary = engine.get_summary()
                        yield f'data: {json.dumps({"type": "summary", "data": summary}, ensure_ascii=False)}\n\n'
                        time.sleep(config.web.sse_interval)
            except GeneratorExit:
                # 客户端断开连接
                pass
            finally:
                # 清理客户端队列
                with sse_lock:
                    if client_queue in sse_clients:
                        sse_clients.remove(client_queue)
                        logger.debug(f"SSE 客户端断开，当前连接数: {len(sse_clients)}")

        # 根据 CORS 配置构建响应头（动态匹配请求来源）
        response_headers = _build_cors_headers(
            config.web.cors_enabled,
            request_origin=request.headers.get("Origin", ""),
        )
        response_headers["X-Accel-Buffering"] = "no"

        return Response(
            generate(),
            mimetype="text/event-stream",
            headers=response_headers,
        )

    # ========== 健康检查 ==========

    @app.route("/health")
    def health_check():
        """健康检查端点，返回引擎运行状态和进程数量"""
        return jsonify({
            "status": "ok",
            "engine_running": engine.is_running,
            "process_count": len(engine.get_all_processes()),
        })

    return app


def _broadcast_sse(
    data: dict[str, Any],
    clients: list[deque],
    lock: threading.Lock,
) -> None:
    """
    向所有 SSE 客户端广播数据

    遍历所有已连接的客户端队列，将数据推送到每个队列中。
    发送失败的客户端会被标记并在广播结束后移除。

    Args:
        data: 要广播的数据
        clients: 客户端队列列表
        lock: 线程锁
    """
    dead_clients = []

    with lock:
        for client_queue in clients:
            try:
                client_queue.append(data)
            except Exception:
                dead_clients.append(client_queue)

        # 移除失效客户端
        for dead in dead_clients:
            try:
                clients.remove(dead)
            except ValueError:
                pass
