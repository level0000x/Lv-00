"""
[已废弃] 此文件为旧版独立脚本，已被 concurrent_monitor/ 包结构替代。
保留仅供参考和向后兼容，新代码请使用包结构导入。
@deprecated 请使用 `from concurrent_monitor.core.engine import MonitorEngine` 等替代。
"""

"""
Web 仪表盘 - 基于 Flask + Server-Sent Events 的浏览器实时面板。
==================================================================
提供以下功能：
  1. SSE (Server-Sent Events) 实时推送进程输出到浏览器
  2. REST API：进程注册、启动、停止、状态查询
  3. 内嵌仪表盘 HTML 页面（进程卡片 + 输出面板 + 控制栏）
  4. WebDashboard 类封装，支持独立运行或嵌入到其他服务

依赖：Flask, monitor_engine (MonitorEngine, OutputLine, ProcessStatus)
端口：默认 5800
"""

import json
import re
import time
import threading
import logging
from typing import Any, Dict, List, Optional
from collections import deque

from flask import Flask, Response, jsonify, render_template_string, request

# [旧式导入] 此文件本身为废弃兼容文件，因此保留旧式导入以匹配其独立脚本定位。
# 新代码请使用包结构导入：from concurrent_monitor.core.engine import MonitorEngine
from monitor_engine import MonitorEngine, OutputLine, ProcessStatus

# ---------------------------------------------------------------------------
# 初始化 Flask 应用
# ---------------------------------------------------------------------------

app = Flask(__name__)

# ---------------------------------------------------------------------------
# 全局状态（模块级单例）
# ---------------------------------------------------------------------------

# 监控引擎实例（由 WebDashboard 启动时注入）
_engine: Optional[MonitorEngine] = None

# SSE 客户端队列列表：每个元素是一个 deque[Dict[str, Any]]，由对应的 /stream 端点轮询消费
# 使用 deque 而非 list 以保证 O(1) 的 append/popleft 操作效率
# 注意：_sse_clients 自身是 list，其元素才是 deque。与 client_queue (deque) 调用 popleft() 不冲突。
_sse_clients: List['deque[Dict[str, Any]]'] = []

# SSE 操作互斥锁（保护 _sse_clients 的并发读写）
_sse_lock = threading.Lock()

# SSE 客户端数量上限（防止无限制增长导致内存溢出）
_MAX_SSE_CLIENTS = 50

# SSE 摘要推送间隔（秒）
_SSE_SUMMARY_INTERVAL = 0.5

# 允许的 CORS 来源（限制为本地开发端口，避免通配符带来的安全风险）
# 生产环境建议: 通过环境变量 CORS_ORIGINS 配置允许的来源列表，
# 格式为逗号分隔的 URL，例如: CORS_ORIGINS="https://myapp.com,https://admin.myapp.com"
_CORS_ALLOWED_ORIGINS: List[str] = [
    "http://localhost:5800",
    "http://127.0.0.1:5800",
    "http://localhost:8000",
    "http://127.0.0.1:8000",
    "http://localhost:3000",
    "http://127.0.0.1:3000",
]


def _get_cors_origin(request_origin: Optional[str]) -> Optional[str]:
    """检查请求 Origin 是否在允许列表中，若在则返回该 Origin 用于 CORS 头。

    由于 Access-Control-Allow-Origin 不支持多值，需动态匹配请求头中的 Origin。
    """
    if request_origin and request_origin in _CORS_ALLOWED_ORIGINS:
        return request_origin
    return None


# ===========================================================================
# SSE 广播引擎
# ===========================================================================

def _broadcast_sse(data: Dict[str, Any]) -> None:
    """向所有已连接的 SSE 客户端广播数据。

    工作流程：
      1. 遍历 _sse_clients 中的每个客户端队列
      2. 将 data 追加到队列尾部（客户端线程将异步消费）
      3. 若某个客户端已断开连接（队列操作抛出异常），将其标记为 dead
      4. 广播结束后移除所有 dead 客户端

    线程安全：通过 _sse_lock 保护 _sse_clients 的并发访问。
    """
    dead: List[deque] = []
    with _sse_lock:
        for client_queue in _sse_clients:
            try:
                client_queue.append(data)
            except Exception:
                # 客户端已断开连接，队列无法再写入
                dead.append(client_queue)
        # 移除死亡客户端
        for dead_queue in dead:
            try:
                _sse_clients.remove(dead_queue)
            except ValueError:
                # 已被其他线程移除，忽略
                pass


def _on_engine_output(line: OutputLine) -> None:
    """引擎输出回调：将 OutputLine 转换为 SSE 事件并广播。

    该函数作为 MonitorEngine.subscribe() 的回调注册，
    每次进程产生新输出时自动触发 SSE 广播。
    """
    _broadcast_sse({
        "type": "output",
        "process_id": line.process_id,
        "stream": line.stream,
        "content": line.content,
        "timestamp": line.timestamp,
    })


@app.route("/stream")
def stream() -> Response:
    """SSE（Server-Sent Events）端点 —— 实时推送进程输出到浏览器。

    每次浏览器访问此端点时：
      1. 创建一个新的客户端队列（deque[dict]，支持 O(1) 的 append/popleft）
      2. 将队列注册到全局 _sse_clients（线程安全）
      3. 进入生成器循环：轮询队列中的消息，格式化为 SSE 事件流
      4. 客户端断开连接时，自动从 _sse_clients 注销

    SSE 事件格式：
      data: <JSON 字符串>\n\n

    事件类型：
      - "output": 进程输出行（含 process_id, stream, content, timestamp）
      - "summary": 定期推送的全局摘要（含所有进程状态）
    """
    # 获取请求的 Origin 并检查是否在允许列表中
    request_origin = request.headers.get("Origin", "")
    cors_origin = _get_cors_origin(request_origin)

    # 创建客户端专用队列（使用 deque 以支持 O(1) 的 popleft 操作）
    client_queue: 'deque[Dict[str, Any]]' = deque()

    # 注册客户端队列（含上限保护）
    with _sse_lock:
        if len(_sse_clients) >= _MAX_SSE_CLIENTS:
            # 客户端数已达上限，拒绝新连接
            return Response(
                "data: {\"type\":\"error\",\"message\":\"已达到最大客户端连接数\"}\n\n",
                mimetype="text/event-stream",
                status=503,
                headers={
                    "Cache-Control": "no-cache",
                    "Access-Control-Allow-Origin": cors_origin or _CORS_ALLOWED_ORIGINS[0],
                },
            )
        _sse_clients.append(client_queue)

    def generate():
        """SSE 事件生成器：持续产出 SSE 格式的事件流。"""
        # 连接建立后立即发送当前摘要快照
        if _engine is not None:
            summary = _engine.get_summary()
            yield f"data: {json.dumps({'type': 'summary', 'data': summary}, ensure_ascii=False)}\n\n"

        try:
            while True:
                if client_queue:
                    # 有积压消息时优先消费
                    msg = client_queue.popleft()  # deque O(1) 弹出
                    yield f"data: {json.dumps(msg, ensure_ascii=False)}\n\n"
                else:
                    # 空闲时发送定期摘要（心跳保活）
                    if _engine is not None:
                        summary = _engine.get_summary()
                        yield f"data: {json.dumps({'type': 'summary', 'data': summary}, ensure_ascii=False)}\n\n"
                    time.sleep(_SSE_SUMMARY_INTERVAL)
        except GeneratorExit:
            # 客户端断开连接，执行清理
            pass
        finally:
            # 从全局客户端列表中移除当前队列
            with _sse_lock:
                try:
                    _sse_clients.remove(client_queue)
                except ValueError:
                    pass

    return Response(
        generate(),
        mimetype="text/event-stream",
        headers={
            "Cache-Control": "no-cache",
            "X-Accel-Buffering": "no",    # 禁用 Nginx 缓冲
            "Access-Control-Allow-Origin": cors_origin or _CORS_ALLOWED_ORIGINS[0],
        },
    )


# ===========================================================================
# REST API 路由
# ===========================================================================

# 引擎未初始化时的统一错误响应
_ENGINE_NOT_INITIALIZED = jsonify({"error": "引擎未初始化", "error_code": "ENGINE_NOT_INIT"}), 400


def _require_engine():
    """检查引擎是否已初始化，未初始化时返回错误 Tuple，否则返回 None。

    用法：result = _require_engine(); if result: return result
    """
    if _engine is None:
        return _ENGINE_NOT_INITIALIZED
    return None


def _validate_json_body(expected_fields: List[str]):
    """从请求中解析 JSON body，验证必填字段是否齐全。

    Args:
        expected_fields: 必填字段列表（如 ["id", "command"]）

    Returns:
        (data, error): 若解析/验证通过，返回 (data_dict, None)；
                       若失败，返回 (None, (error_json, status_code))。
    """
    body = request.get_json(silent=True)
    if body is None:
        return None, (jsonify({"error": "请求体不是有效的 JSON", "error_code": "INVALID_JSON"}), 400)

    missing = [f for f in expected_fields if not body.get(f)]
    if missing:
        return None, (jsonify({
            "error": f"缺少必填字段: {', '.join(missing)}",
            "error_code": "MISSING_FIELDS",
            "missing_fields": missing,
        }), 400)

    return body, None


# ---------------------------------------------------------------------------
# 危险命令过滤（安全措施：防止远程命令执行 RCE）
# ---------------------------------------------------------------------------

# 安全日志记录器
_security_logger = logging.getLogger(__name__ + ".security")

# 危险命令模式列表：匹配到任一模式即拒绝注册
# 包括：系统破坏性命令、权限提升、网络攻击工具、反弹 shell 等
_DANGEROUS_COMMAND_PATTERNS: List[re.Pattern] = [
    re.compile(r'\b(?:rm|del|Remove-Item)\b.*(?:-r|-f|-recurse|-force)\b', re.IGNORECASE),
    re.compile(r'\b(?:mkfs|format|diskpart)\b', re.IGNORECASE),
    re.compile(r'\b(?:dd)\s+.*of=/dev/', re.IGNORECASE),
    re.compile(r'\b(?:shutdown|restart|reboot)\b', re.IGNORECASE),
    re.compile(r'>\s*/dev/(?:sd|nvme|vd)', re.IGNORECASE),
    re.compile(r'\b(?:chmod)\s+777\b', re.IGNORECASE),
    re.compile(r'\b(?:curl|wget|Invoke-WebRequest)\b.*\|\s*(?:sh|bash|powershell|cmd)', re.IGNORECASE),
    re.compile(r'\b(?:nc|ncat|netcat)\b.*-[el]', re.IGNORECASE),
    re.compile(r'\b(?:python|python3|perl|ruby)\b.*-[ec].*import\s+(?:os|subprocess|socket|sys)', re.IGNORECASE),
    re.compile(r'\\b(?:powershell|cmd|bash|sh)\\b.*-[ec]', re.IGNORECASE),
    re.compile(r'(?:;|&&|\|)\s*(?:rm|del|shutdown|mkfs|format)\b', re.IGNORECASE),
    re.compile(r'\$\([^)]*\)\s*(?:;|&&|\|)', re.IGNORECASE),  # 命令替换后接管道
    re.compile(r'`[^`]+`\s*(?:;|&&|\|)', re.IGNORECASE),       # 反引号命令替换
    re.compile(r'\b(?:reg\s+delete|regedit)\b', re.IGNORECASE),  # Windows 注册表操作
    re.compile(r'\b(?:net\s+user|net\s+localgroup)\b.*(?:/add|/delete)', re.IGNORECASE),
    re.compile(r'\b(?:taskkill|Stop-Process)\b.*/f', re.IGNORECASE),
    re.compile(r'\b(?:certutil|bitsadmin)\b', re.IGNORECASE),    # 常见下载/执行工具
]

# 进程 ID 合法字符白名单（仅允许字母、数字、下划线、连字符、点号）
_SAFE_PROCESS_ID_RE = re.compile(r'^[a-zA-Z0-9_\-.]+$')


def _validate_command_safety(command: str) -> Optional[tuple]:
    """验证命令字符串是否安全，防止远程命令执行（RCE）攻击。

    安全措施：
      1. 检查命令长度上限（防止超长命令攻击）
      2. 逐项匹配危险命令模式黑名单
      3. 记录安全审计日志

    Args:
        command: 用户提交的命令字符串

    Returns:
        None 表示命令安全；若命令危险则返回 (error_json, status_code) 元组
    """
    # 命令长度上限检查（防止超长命令导致缓冲区溢出等问题）
    if len(command) > 4096:
        _security_logger.warning("命令长度超限: %d 字符", len(command))
        return (jsonify({
            "error": "命令过长，最大允许 4096 个字符",
            "error_code": "COMMAND_TOO_LONG",
        }), 400)

    # 危险命令模式匹配
    for pattern in _DANGEROUS_COMMAND_PATTERNS:
        if pattern.search(command):
            matched_desc = pattern.pattern[:80]
            _security_logger.warning(
                "检测到危险命令被拦截: pattern=%r, command=%r",
                matched_desc, command[:200],
            )
            return (jsonify({
                "error": "命令包含危险模式，已被安全策略拦截。如需执行此命令，请直接在终端中操作。",
                "error_code": "DANGEROUS_COMMAND_BLOCKED",
            }), 403)

    return None


def _validate_process_id_safety(process_id: str) -> Optional[tuple]:
    """验证进程 ID 是否安全，防止注入攻击。

    安全措施：
      1. 白名单字符检查（防止 XSS 和命令注入）
      2. 长度限制

    Args:
        process_id: 用户提交的进程 ID

    Returns:
        None 表示安全；若不安全则返回 (error_json, status_code) 元组
    """
    if len(process_id) > 128:
        return (jsonify({
            "error": "进程 ID 过长，最大允许 128 个字符",
            "error_code": "PROCESS_ID_TOO_LONG",
        }), 400)

    if not _SAFE_PROCESS_ID_RE.match(process_id):
        _security_logger.warning("检测到非法进程 ID: %r", process_id)
        return (jsonify({
            "error": "进程 ID 仅允许字母、数字、下划线、连字符和点号",
            "error_code": "INVALID_PROCESS_ID",
        }), 400)

    return None


@app.route("/api/processes", methods=["GET"])
def api_get_processes():
    """获取所有已注册进程的状态摘要。

    GET /api/processes

    Returns:
        200: {"processes": [...], "total": N, "status_counts": {...}}
        400: {"error": "...", "error_code": "ENGINE_NOT_INIT"}
    """
    err = _require_engine()
    if err:
        return err
    assert _engine is not None
    return jsonify(_engine.get_summary())


@app.route("/api/register", methods=["POST"])
def api_register():
    """注册新进程到监控引擎。

    POST /api/register
    Body: {"id": "proc_1", "command": "ping -n 5 127.0.0.1"}

    Returns:
        200: {"ok": True, "id": "proc_1"}
        400: {"error": "...", "error_code": "..."}
    """
    err = _require_engine()
    if err:
        return err

    body, validation_err = _validate_json_body(["id", "command"])
    if validation_err:
        return validation_err
    assert body is not None
    assert _engine is not None

    process_id: str = body["id"]
    command: str = body["command"]

    # 安全验证：检查进程 ID 合法性（防止 XSS / 注入攻击）
    id_err = _validate_process_id_safety(process_id)
    if id_err:
        return id_err

    # 安全验证：检查命令是否包含危险模式（防止 RCE 攻击）
    cmd_err = _validate_command_safety(command)
    if cmd_err:
        return cmd_err

    _engine.register_process(process_id, command)
    _security_logger.info("进程已注册: id=%r, command=%r", process_id, command[:200])
    return jsonify({"ok": True, "id": process_id})


@app.route("/api/start", methods=["POST"])
def api_start():
    """启动已注册的进程（可在后台并发运行）。

    POST /api/start
    Body（可选）:
      - timeout: int   超时秒数
      - process_ids: list[str]  要启动的进程 ID 列表（None 表示全部）

    Returns:
        200: {"ok": True, "message": "已启动"}
        400: {"error": "...", "error_code": "..."}
    """
    err = _require_engine()
    if err:
        return err
    assert _engine is not None

    body = request.get_json(silent=True) or {}
    timeout = body.get("timeout")
    process_ids = body.get("process_ids")

    _engine.run_in_background(process_ids, timeout)
    return jsonify({"ok": True, "message": "已启动"})


@app.route("/api/stop/<process_id>", methods=["POST"])
def api_stop(process_id: str) -> Response:
    """停止指定的运行中进程。

    POST /api/stop/<process_id>

    Returns:
        200: {"ok": True}
        400: {"error": "...", "error_code": "..."}
    """
    err = _require_engine()
    if err:
        return err
    assert _engine is not None
    _engine.stop_process(process_id)
    return jsonify({"ok": True})


@app.route("/api/stop_all", methods=["POST"])
def api_stop_all() -> Response:
    """停止所有运行中的进程。

    POST /api/stop_all

    Returns:
        200: {"ok": True}
        400: {"error": "...", "error_code": "..."}
    """
    err = _require_engine()
    if err:
        return err
    assert _engine is not None
    _engine.stop_all()
    return jsonify({"ok": True})


# ===========================================================================
# 仪表盘 HTML 模板（内嵌）
# ===========================================================================

DASHBOARD_HTML = r"""
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>并发输出实时监控</title>
<style>
  :root {
    --bg: #0d1117;
    --surface: #161b22;
    --border: #30363d;
    --text: #c9d1d9;
    --text-dim: #8b949e;
    --accent: #58a6ff;
    --green: #3fb950;
    --red: #f85149;
    --yellow: #d2991d;
    --magenta: #bc8cff;
  }
  * { margin: 0; padding: 0; box-sizing: border-box; }
  body {
    font-family: 'Segoe UI', 'Consolas', monospace;
    background: var(--bg);
    color: var(--text);
    height: 100vh;
    display: flex;
    flex-direction: column;
    overflow: hidden;
  }

  /* 顶部状态栏 */
  .header {
    background: var(--surface);
    border-bottom: 1px solid var(--border);
    padding: 12px 20px;
    display: flex;
    align-items: center;
    gap: 24px;
    flex-shrink: 0;
  }
  .header h1 { font-size: 18px; color: var(--accent); }
  .status-badges { display: flex; gap: 12px; margin-left: auto; }
  .badge {
    padding: 4px 12px;
    border-radius: 12px;
    font-size: 13px;
    font-weight: 600;
    background: var(--surface);
    border: 1px solid var(--border);
  }
  .badge.running { border-color: var(--yellow); color: var(--yellow); }
  .badge.completed { border-color: var(--green); color: var(--green); }
  .badge.failed { border-color: var(--red); color: var(--red); }
  .badge.total { border-color: var(--accent); color: var(--accent); }

  /* 主体布局 */
  .main {
    display: flex;
    flex: 1;
    overflow: hidden;
  }
  .left-panel {
    width: 340px;
    flex-shrink: 0;
    background: var(--surface);
    border-right: 1px solid var(--border);
    overflow-y: auto;
    padding: 8px;
  }
  .right-panel {
    flex: 1;
    overflow-y: auto;
    padding: 8px;
    display: flex;
    flex-direction: column;
    gap: 8px;
  }

  /* 进程卡片 */
  .proc-card {
    background: var(--bg);
    border: 1px solid var(--border);
    border-radius: 8px;
    margin-bottom: 8px;
    padding: 10px 12px;
    cursor: pointer;
    transition: border-color 0.2s;
  }
  .proc-card:hover { border-color: var(--accent); }
  .proc-card.active { border-color: var(--accent); box-shadow: 0 0 8px rgba(88,166,255,0.2); }
  .proc-card .name { font-weight: 600; font-size: 14px; }
  .proc-card .cmd { color: var(--text-dim); font-size: 12px; margin-top: 2px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
  .proc-card .meta { display: flex; gap: 12px; margin-top: 6px; font-size: 11px; color: var(--text-dim); }
  .proc-card .status-dot {
    display: inline-block; width: 8px; height: 8px; border-radius: 50%; margin-right: 4px;
  }
  .status-dot.running { background: var(--yellow); animation: pulse 1s infinite; }
  .status-dot.completed { background: var(--green); }
  .status-dot.failed { background: var(--red); }
  .status-dot.pending { background: var(--text-dim); }
  .status-dot.timeout { background: var(--magenta); }
  @keyframes pulse { 0%,100%{opacity:1} 50%{opacity:0.4} }

  /* 输出面板 */
  .output-panel {
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: 8px;
    overflow: hidden;
    flex: 1;
    min-height: 120px;
    display: flex;
    flex-direction: column;
  }
  .output-panel .panel-header {
    padding: 8px 14px;
    border-bottom: 1px solid var(--border);
    display: flex;
    align-items: center;
    gap: 8px;
    font-size: 13px;
    font-weight: 600;
    background: rgba(255,255,255,0.02);
  }
  .output-panel .panel-body {
    flex: 1;
    overflow-y: auto;
    padding: 8px 14px;
    font-family: 'Consolas', 'Courier New', monospace;
    font-size: 12px;
    line-height: 1.5;
    max-height: 300px;
  }
  .output-panel .line { white-space: pre-wrap; word-break: break-all; }
  .output-panel .line.stderr { color: var(--red); }
  .output-panel .line.system { color: var(--text-dim); font-style: italic; }

  /* 控制区 */
  .controls {
    padding: 10px 20px;
    background: var(--surface);
    border-top: 1px solid var(--border);
    display: flex;
    gap: 10px;
    align-items: center;
    flex-shrink: 0;
  }
  .controls input {
    flex: 1;
    padding: 8px 12px;
    background: var(--bg);
    border: 1px solid var(--border);
    border-radius: 6px;
    color: var(--text);
    font-size: 13px;
    font-family: 'Consolas', monospace;
  }
  .controls input:focus { outline: none; border-color: var(--accent); }
  .btn {
    padding: 8px 16px;
    border: 1px solid var(--border);
    border-radius: 6px;
    cursor: pointer;
    font-size: 13px;
    font-weight: 600;
    transition: all 0.2s;
    white-space: nowrap;
  }
  .btn-primary { background: var(--accent); color: #fff; border-color: var(--accent); }
  .btn-primary:hover { background: #4090e0; }
  .btn-danger { background: transparent; color: var(--red); border-color: var(--red); }
  .btn-danger:hover { background: rgba(248,81,73,0.1); }
  .btn-default { background: transparent; color: var(--text); }
  .btn-default:hover { background: rgba(255,255,255,0.05); }

  /* 滚动条 */
  ::-webkit-scrollbar { width: 6px; }
  ::-webkit-scrollbar-track { background: transparent; }
  ::-webkit-scrollbar-thumb { background: var(--border); border-radius: 3px; }
</style>
</head>
<body>

<div class="header">
  <h1>&#x25C9; 并发输出实时监控</h1>
  <div class="status-badges" id="statusBadges">
    <span class="badge total">总计: 0</span>
    <span class="badge running" id="badgeRunning">运行: 0</span>
    <span class="badge completed" id="badgeCompleted">完成: 0</span>
    <span class="badge failed" id="badgeFailed">失败: 0</span>
  </div>
</div>

<div class="main">
  <div class="left-panel" id="procList">
    <div style="padding:20px;text-align:center;color:var(--text-dim);">暂无进程</div>
  </div>
  <div class="right-panel" id="outputArea">
    <div style="display:flex;align-items:center;justify-content:center;height:100%;color:var(--text-dim);">
      点击左侧进程查看实时输出
    </div>
  </div>
</div>

<div class="controls">
  <input type="text" id="cmdInput" placeholder="输入命令... (例: ping -n 5 127.0.0.1)" onkeydown="if(event.key==='Enter')addProcess()">
  <button class="btn btn-primary" onclick="addProcess()">+ 添加进程</button>
  <button class="btn btn-primary" onclick="startAll()">&#x25B6; 启动全部</button>
  <button class="btn btn-danger" onclick="stopAll()">&#x25A0; 停止全部</button>
  <button class="btn btn-default" onclick="loadDemo()">演示</button>
</div>

<script>
// ===== 状态管理 =====
let processes = {};
let selectedPid = null;
let outputBuffers = {};
const MAX_LINES = 500;

// ===== SSE 连接 =====
const es = new EventSource('/stream');
es.onmessage = (e) => {
  const msg = JSON.parse(e.data);
  if (msg.type === 'output') {
    handleOutput(msg);
  } else if (msg.type === 'summary') {
    handleSummary(msg.data);
  }
};

function handleOutput(msg) {
  if (!outputBuffers[msg.process_id]) {
    outputBuffers[msg.process_id] = [];
  }
  const buf = outputBuffers[msg.process_id];
  buf.push(msg);
  if (buf.length > MAX_LINES) buf.shift();

  // 如果该进程正被选中，更新显示
  if (selectedPid === msg.process_id) {
    appendLineToPanel(msg.process_id, msg);
  }
}

function handleSummary(data) {
  processes = {};
  data.processes.forEach(p => {
    processes[p.id] = p;
  });
  if (!outputBuffers) outputBuffers = {};
  renderProcList();
  updateBadges(data.status_counts, data.total);
}

function updateBadges(counts, total) {
  document.querySelector('.badge.total').textContent = `总计: ${total}`;
  document.getElementById('badgeRunning').textContent = `运行: ${counts.running || 0}`;
  document.getElementById('badgeCompleted').textContent = `完成: ${counts.completed || 0}`;
  document.getElementById('badgeFailed').textContent = `失败: ${(counts.failed || 0) + (counts.timeout || 0)}`;
}

// ===== 渲染进程列表 =====
function renderProcList() {
  const container = document.getElementById('procList');
  const pids = Object.keys(processes);
  if (pids.length === 0) {
    container.innerHTML = '<div style="padding:20px;text-align:center;color:var(--text-dim);">暂无进程</div>';
    return;
  }
  container.innerHTML = pids.map(pid => {
    const p = processes[pid];
    const statusClass = p.status;
    // 安全措施：对 pid 进行 HTML 属性转义，防止 XSS 注入攻击
    // 攻击者可能通过注册包含单引号/双引号的进程 ID 来注入 JavaScript
    const safePid = escapeHtml(pid);
    const safeAttrPid = safePid.replace(/'/g, '&#39;').replace(/"/g, '&quot;');
    return `
      <div class="proc-card ${selectedPid === pid ? 'active' : ''}" onclick="selectProcess('${safeAttrPid}')">
        <div class="name">
          <span class="status-dot ${statusClass}"></span>${safePid}
        </div>
        <div class="cmd">${escapeHtml(p.command)}</div>
        <div class="meta">
          <span>状态: ${p.status}</span>
          <span>行数: ${p.lines}</span>
          <span>错误: ${p.errors}</span>
          <span>耗时: ${p.duration}</span>
          ${p.exit_code !== null ? `<span>退出码: ${p.exit_code}</span>` : ''}
        </div>
      </div>
    `;
  }).join('');
}

function selectProcess(pid) {
  selectedPid = pid;
  renderProcList();
  renderOutput(pid);
}

function renderOutput(pid) {
  const area = document.getElementById('outputArea');
  const p = processes[pid];
  if (!p) {
    area.innerHTML = '<div style="display:flex;align-items:center;justify-content:center;height:100%;color:var(--text-dim);">进程不存在</div>';
    return;
  }
  const buf = outputBuffers[pid] || [];
  const statusClass = p.status;
  // 安全措施：对 pid 进行 HTML 转义后再显示，防止 XSS
  const safePid = escapeHtml(pid);
  area.innerHTML = `
    <div class="output-panel" style="flex:1">
      <div class="panel-header">
        <span class="status-dot ${statusClass}"></span>
        <span>${safePid}</span>
        <span style="color:var(--text-dim);font-weight:400;font-size:12px;">${escapeHtml(p.command)}</span>
        <span style="margin-left:auto;font-size:11px;color:var(--text-dim);">
          ${p.status} | exit=${p.exit_code ?? '-'} | ${p.duration}
        </span>
      </div>
      <div class="panel-body" id="panelBody_${pid}">
        ${buf.map(l => formatLine(l)).join('') || '<div style="color:var(--text-dim);">等待输出...</div>'}
      </div>
    </div>
  `;
  // 滚动到底部
  const body = document.getElementById('panelBody_' + pid);
  if (body) body.scrollTop = body.scrollHeight;
}

function appendLineToPanel(pid, msg) {
  const body = document.getElementById('panelBody_' + pid);
  if (!body) return;
  const wasAtBottom = body.scrollHeight - body.scrollTop - body.clientHeight < 40;
  body.insertAdjacentHTML('beforeend', formatLine(msg));
  if (wasAtBottom) body.scrollTop = body.scrollHeight;
}

function formatLine(msg) {
  const cls = msg.stream === 'stderr' ? 'line stderr' : msg.stream === 'system' ? 'line system' : 'line';
  return `<div class="${cls}">${escapeHtml(msg.content)}</div>`;
}

function escapeHtml(s) {
  const div = document.createElement('div');
  div.textContent = s;
  return div.innerHTML;
}

// ===== 操作 =====
let procCounter = 0;
async function addProcess() {
  const input = document.getElementById('cmdInput');
  const cmd = input.value.trim();
  if (!cmd) return;
  procCounter++;
  const pid = 'proc_' + procCounter;
  await fetch('/api/register', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({id: pid, command: cmd})
  });
  input.value = '';
}

async function startAll() {
  await fetch('/api/start', {method: 'POST'});
}

async function stopAll() {
  await fetch('/api/stop_all', {method: 'POST'});
}

async function loadDemo() {
  const demos = [
    {id: 'ping_local', command: 'ping -n 6 127.0.0.1'},
    {id: 'ping_dns', command: 'ping -n 4 8.8.8.8'},
    {id: 'dir_list', command: 'powershell -Command "Get-ChildItem -Path C:\\Windows\\System32 -Filter *.dll | Select-Object -First 20 Name,Length"'},
    {id: 'counter', command: 'powershell -Command "for($i=1;$i -le 15;$i++){Write-Host \\"计数: $i\\"; Start-Sleep 0.3}"'},
  ];
  for (const d of demos) {
    await fetch('/api/register', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify(d)
    });
  }
  await fetch('/api/start', {method: 'POST'});
}
</script>
</body>
</html>
"""


@app.route("/")
def index():
    return render_template_string(DASHBOARD_HTML)


# ===========================================================================
# Web 仪表盘启动器
# ===========================================================================

class WebDashboard:
    """Web 仪表盘启动器 —— 封装 Flask 服务器的启动与生命周期管理。

    使用方式：
        dashboard = WebDashboard(engine, port=5800)
        dashboard.run()  # 阻塞直到服务器停止

    参数：
        engine:          MonitorEngine 监控引擎实例
        host:            监听地址（默认 127.0.0.1，仅本地访问）
        port:            监听端口（默认 5800）
        open_browser:    是否自动在浏览器中打开仪表盘页面（默认 True）
    """

    def __init__(
        self,
        engine: MonitorEngine,
        host: str = "127.0.0.1",
        port: int = 5800,
        open_browser: bool = True,
    ):
        global _engine
        _engine = engine
        self._engine: MonitorEngine = engine
        self._host: str = host
        self._port: int = port
        self._open_browser: bool = open_browser

        # 订阅引擎输出事件，通过 SSE 推送到浏览器
        self._engine.subscribe(_on_engine_output)

    def run(self) -> None:
        """启动 Flask Web 服务器（阻塞调用）。

        服务器启动后将：
          1. 打印仪表盘访问 URL 和 SSE 端点地址
          2. 若 open_browser=True，延迟 1 秒后自动打开浏览器
          3. 进入 Flask 主循环，处理 HTTP 请求
          4. Ctrl+C 可终止服务器
        """
        import webbrowser

        url = f"http://{self._host}:{self._port}"
        print(f"\n  ✅ Web 仪表盘已启动: {url}")
        print(f"  📡 SSE 端点: {url}/stream")
        print(f"  🛑 按 Ctrl+C 停止服务器\n")

        if self._open_browser:
            threading.Timer(1.0, lambda: webbrowser.open(url)).start()

        # 注意：Flask 内置开发服务器 (app.run) 不适用于生产环境。
        # 生产环境建议使用:
        #   - Waitress (Windows):  waitress-serve --host=0.0.0.0 --port=5800 web_dashboard:app
        #   - Gunicorn (Linux):    gunicorn -w 4 -b 0.0.0.0:5800 web_dashboard:app
        #   - 或通过 WSGI 容器 (如 uWSGI) 部署
        app.run(host=self._host, port=self._port, debug=False, threaded=True)


def launch_web_dashboard(
    engine: MonitorEngine,
    host: str = "127.0.0.1",
    port: int = 5800,
    open_browser: bool = True,
) -> None:
    """快速启动 Web 仪表盘的便捷函数。

    Args:
        engine:         MonitorEngine 监控引擎实例
        host:           监听地址（默认 127.0.0.1）
        port:           监听端口（默认 5800）
        open_browser:   是否自动打开浏览器（默认 True）
    """
    dashboard = WebDashboard(engine, host=host, port=port, open_browser=open_browser)
    dashboard.run()


# ===========================================================================
# 独立运行入口（用于快速演示）
# ===========================================================================

if __name__ == "__main__":
    """独立运行演示：注册 4 个示例进程，自动启动并打开浏览器仪表盘。

    示例进程：
      - ping_local:  ping 本机回环地址（6 次）
      - ping_dns:    ping Google DNS（4 次）
      - dir_list:    列出 System32 下前 20 个 .dll 文件
      - counter:     1 到 15 计数（每步间隔 0.3 秒）
    """
    print("🚀 启动 Web 演示模式...")

    # 创建引擎（最大 10 个并发进程）
    engine = MonitorEngine(max_concurrency=10)

    # 注册演示进程
    engine.register_process("ping_local", "ping -n 6 127.0.0.1")
    engine.register_process("ping_dns", "ping -n 4 8.8.8.8")
    engine.register_process(
        "dir_list",
        (
            'powershell -Command '
            '"Get-ChildItem -Path C:\\Windows\\System32 -Filter *.dll '
            '| Select-Object -First 20 Name,Length"'
        ),
    )
    engine.register_process(
        "counter",
        (
            'powershell -Command '
            '"for($i=1;$i -le 15;$i++){Write-Host \'计数:\' $i; Start-Sleep 0.3}"'
        ),
    )

    # 先启动进程（后台运行），再启动 Web 服务器
    engine.run_in_background(timeout=30)
    launch_web_dashboard(engine)
