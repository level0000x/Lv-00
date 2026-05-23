"""
Web 路由定义
=============

定义 Flask 应用的路由、API 端点和 SSE 流。
"""

from __future__ import annotations

import json
import os
import time
import threading
from collections import deque
from typing import Any

from flask import Flask, Response, jsonify, request, render_template_string

from ..core.engine import MonitorEngine
from ..core.events import Event, EventType
from ..core.config import Config
from ..utils.logger import get_logger
from ..utils.constants import HTTP_OK, HTTP_BAD_REQUEST, HTTP_SERVICE_UNAVAILABLE
from .templates import DASHBOARD_HTML

logger = get_logger(__name__)


def create_app(engine: MonitorEngine, config: Config) -> Flask:
    """
    创建 Flask 应用实例
    
    Args:
        engine: 监控引擎实例
        config: 配置对象
    
    Returns:
        Flask 应用实例
    """
    app = Flask(__name__)
    app.config["SECRET_KEY"] = os.environ.get("MONITOR_SECRET_KEY", os.urandom(32).hex())

    # SSE 客户端管理
    sse_clients: list[deque] = []
    sse_lock = threading.Lock()

    # 订阅引擎事件
    def on_engine_event(event: Event) -> None:
        """引擎事件回调"""
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
    def not_found(error):
        return jsonify({"error": "未找到", "error_code": "NOT_FOUND"}), 404

    @app.errorhandler(500)
    def internal_error(error):
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
        """注册新进程"""
        body = request.get_json(silent=True)
        if body is None:
            return jsonify({"error": "无效的 JSON", "error_code": "INVALID_JSON"}), HTTP_BAD_REQUEST

        process_id = body.get("id")
        command = body.get("command")

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

        try:
            engine.register_process(process_id, command)
            logger.info(f"通过 API 注册进程: {process_id}")
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
        """停止指定进程"""
        if engine.stop_process(process_id):
            logger.info(f"通过 API 停止进程: {process_id}")
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
        """注销进程"""
        if engine.unregister_process(process_id):
            logger.info(f"通过 API 注销进程: {process_id}")
            return jsonify({"ok": True})
        else:
            return jsonify({"error": "进程不存在", "error_code": "PROCESS_NOT_FOUND"}), 404

    # ========== SSE 路由 ==========

    @app.route("/stream")
    def stream():
        """SSE 实时流端点"""
        client_queue: deque = deque()

        # 检查客户端数上限
        with sse_lock:
            if len(sse_clients) >= config.web.max_sse_clients:
                return Response(
                    'data: {"type":"error","message":"已达到最大客户端连接数"}\n\n',
                    mimetype="text/event-stream",
                    status=HTTP_SERVICE_UNAVAILABLE,
                    headers={
                        "Cache-Control": "no-cache",
                        "Access-Control-Allow-Origin": "*",
                    },
                )
            sse_clients.append(client_queue)
            logger.debug(f"新 SSE 客户端连接，当前连接数: {len(sse_clients)}")

        def generate():
            """生成 SSE 事件流"""
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
                # 客户端断开
                pass
            finally:
                # 清理客户端队列
                with sse_lock:
                    if client_queue in sse_clients:
                        sse_clients.remove(client_queue)
                        logger.debug(f"SSE 客户端断开，当前连接数: {len(sse_clients)}")

        return Response(
            generate(),
            mimetype="text/event-stream",
            headers={
                "Cache-Control": "no-cache",
                "X-Accel-Buffering": "no",
                "Access-Control-Allow-Origin": "*",
            },
        )

    # ========== 健康检查 ==========

    @app.route("/health")
    def health_check():
        """健康检查端点"""
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
