#!/usr/bin/env python3
"""
[已废弃] 此文件为旧版独立脚本，功能已整合到 stream_bridge/stream_bridge.py。
保留仅供参考和向后兼容。

Lv-00 流式事件桥接脚本 (Stream Bridge)

功能：
  1. 运行 Lv-00 编译后的演示二进制文件
  2. 捕获 stdout 中的 JSON-RPC notification 流式事件
  3. 解析事件并转发到 stream-monitor WebSocket 服务器
  4. 支持多路并发：每种事件类型自动创建独立面板

用法：
  python stream_bridge.py                          # 默认连接 ws://localhost:3456
  python stream_bridge.py --ws ws://myhost:3456    # 指定 WebSocket 地址
  python stream_bridge.py --binary build/example_streaming.exe  # 指定二进制路径
  python stream_bridge.py --demo 1                 # 仅运行演示1

依赖：
  pip install websocket-client

@deprecated 请使用 stream_bridge/stream_bridge.py（包结构版本）。
"""

import subprocess
import sys
import json
import time
import argparse
import os
import threading
import signal
import logging

try:
    import websocket
except ImportError:
    print("[BRIDGE] ================================================")
    print("[BRIDGE] 缺少 websocket-client 库。")
    print("[BRIDGE] 请使用您信任的包管理器手动安装依赖：")
    print("[BRIDGE]   例如: pip install websocket-client")
    print("[BRIDGE] 注意：请确保从官方 PyPI 源安装，")
    print("[BRIDGE]       避免使用未经验证的第三方源。")
    print("[BRIDGE] ================================================")
    sys.exit(1)

# 模块级日志
logger = logging.getLogger(__name__)
EVENT_COLORS = {
    "ENGINE":        "#3fb950",  # 绿色: 引擎生命周期
    "NORMALIZE":     "#a371f7",  # 紫色: 归一化
    "REWRITE":       "#a371f7",  # 紫色: 重写
    "SOLVE":         "#a371f7",  # 紫色: 求解
    "PROOF":         "#a371f7",  # 紫色: 证明
    "FUNC_BLOCK":    "#39d353",  # 青绿: 函数块
    "ERROR":         "#f85149",  # 红色: 错误
    "WARNING":       "#d29922",  # 黄色: 警告
    "CONFLICT":      "#f0883e",  # 橙色: 冲突
    "INFO":          "#8b949e",  # 灰色: 信息
    "PROGRESS":      "#58a6ff",  # 蓝色: 进度
    "GRAPH_SNAPSHOT":"#c9d1d9",  # 浅灰: 图快照
}

# 每个事件类型对应的面板名称
EVENT_PANELS = {
    "ENGINE":        "引擎生命周期",
    "NORMALIZE":     "图归一化",
    "REWRITE":       "图重写引擎",
    "SOLVE":         "代数求解器",
    "PROOF":         "证明系统",
    "FUNC_BLOCK":    "函数块系统",
    "ERROR":         "错误日志",
    "WARNING":       "警告日志",
    "CONFLICT":      "冲突检测",
    "INFO":          "信息日志",
    "PROGRESS":      "进度追踪",
    "GRAPH_SNAPSHOT":"图快照",
}


class Lv00StreamBridge:
    """Lv-00 流式事件桥接器"""

    # WebSocket 重连配置
    WS_RECONNECT_MAX_ATTEMPTS = 5        # 最大重连尝试次数
    WS_RECONNECT_BASE_DELAY = 1.0        # 基础重连延迟（秒）
    WS_RECONNECT_MAX_DELAY = 30.0        # 最大重连延迟（秒）
    WS_RECONNECT_BACKOFF = 2.0           # 退避因子

    def __init__(self, ws_url, binary_path, demo_num=None):
        self.ws_url = ws_url
        self.binary_path = binary_path
        self.demo_num = demo_num
        self.ws = None
        self.ws_connected = False
        self.process = None
        self.panel_ids = {}  # category → stream_id
        self.running = True
        self.event_count = 0
        self._ws_reconnect_count = 0

    def _get_event_category(self, event_type_id):
        """从事件类型ID提取类别"""
        if not event_type_id:
            return "INFO"
        for cat in ["ENGINE", "NORMALIZE", "REWRITE", "SOLVE", "PROOF",
                     "FUNC_BLOCK", "ERROR", "WARNING", "CONFLICT",
                     "PROGRESS", "GRAPH_SNAPSHOT", "INFO"]:
            if event_type_id.startswith(cat):
                return cat
        return "INFO"

    def _connect_ws(self):
        """连接 WebSocket 服务器（首次连接）"""
        try:
            self.ws = websocket.WebSocket()
            self.ws.connect(self.ws_url, timeout=5)
            self.ws_connected = True
            self._ws_reconnect_count = 0
            print(f"[BRIDGE] WebSocket 已连接: {self.ws_url}")
            return True
        except Exception as e:
            print(f"[BRIDGE] WebSocket 连接失败 ({e})，将仅输出到控制台")
            self.ws_connected = False
            return False

    def _try_reconnect_ws(self):
        """尝试重新连接 WebSocket（带指数退避）

        当连接意外断开时，按退避策略尝试重连。
        重连成功后重置面板映射以确保新连接正确初始化面板。
        """
        if not self.running:
            return False

        if self._ws_reconnect_count >= self.WS_RECONNECT_MAX_ATTEMPTS:
            print("[BRIDGE] WebSocket 重连已达最大尝试次数，放弃重连")
            return False

        # 先清理旧连接
        if self.ws:
            try:
                self.ws.close()
            except Exception as e:
                logger.debug("关闭旧WebSocket连接时异常（可忽略）: %s", e)
            self.ws = None

        # 计算退避延迟（指数退避，上限为 WS_RECONNECT_MAX_DELAY）
        delay = min(
            self.WS_RECONNECT_BASE_DELAY * (self.WS_RECONNECT_BACKOFF ** self._ws_reconnect_count),
            self.WS_RECONNECT_MAX_DELAY
        )
        self._ws_reconnect_count += 1

        print(f"[BRIDGE] WebSocket 断连，{delay:.1f}s 后尝试第 "
              f"{self._ws_reconnect_count}/{self.WS_RECONNECT_MAX_ATTEMPTS} 次重连...")
        time.sleep(delay)

        try:
            self.ws = websocket.WebSocket()
            self.ws.connect(self.ws_url, timeout=5)
            self.ws_connected = True
            # 重置面板映射：新连接需要重新创建面板
            self.panel_ids.clear()
            self._ws_reconnect_count = 0
            print(f"[BRIDGE] WebSocket 重连成功: {self.ws_url}")
            return True
        except Exception as e:
            print(f"[BRIDGE] WebSocket 重连失败 ({e})")
            self.ws_connected = False
            return False

    def _ensure_panel(self, category, panel_name, color):
        """确保事件类别对应的面板已创建"""
        if category in self.panel_ids:
            return self.panel_ids[category]

        # 使用类别名作为 stream_id
        stream_id = f"lv00-{category.lower()}"
        self.panel_ids[category] = stream_id

        if self.ws_connected:
            try:
                # 先推送一条初始化消息来创建面板
                init_msg = json.dumps({
                    "type": "push",
                    "streamId": stream_id,
                    "streamName": f"🔷 {panel_name}",
                    "text": f"═══ {panel_name} 已就绪 ═══"
                })
                self.ws.send(init_msg)
            except Exception as e:
                # 初始化消息发送失败不影响主流程
                logger.debug("面板初始化消息发送失败（可忽略）: %s", e)

        return stream_id

    def _send_to_ws(self, stream_id, text):
        """推送一行文本到 WebSocket（含断连重连机制）

        发送失败时自动尝试重连。重连成功则重试一次发送；
        重连失败则将消息输出到控制台以避免事件丢失。
        """
        if not self.ws_connected or not self.ws:
            return

        try:
            msg = json.dumps({
                "type": "push",
                "streamId": stream_id,
                "text": text
            })
            self.ws.send(msg)
        except Exception:
            # 连接意外断开，标记断开并尝试重连
            self.ws_connected = False
            if self._try_reconnect_ws():
                # 重连成功，重试发送当前消息
                try:
                    msg = json.dumps({
                        "type": "push",
                        "streamId": stream_id,
                        "text": text
                    })
                    self.ws.send(msg)
                except Exception as e:
                    self.ws_connected = False
                    logger.warning("重连后重试发送消息失败: %s", e)
                    print(f"[BRIDGE] (离线) {text}")
            else:
                # 重连失败，将消息回显到控制台避免丢失
                print(f"[BRIDGE] (离线) {text}")

    def _process_event(self, event):
        """处理单个流式事件"""
        self.event_count += 1

        event_type_id = event.get("type", "UNKNOWN")
        category = self._get_event_category(event_type_id)
        type_name = event.get("type_name", event_type_id)
        description = event.get("description", "")
        step = event.get("step", -1)
        total_steps = event.get("total_steps", -1)
        color = EVENT_COLORS.get(category, "#8b949e")
        panel_name = EVENT_PANELS.get(category, category)

        stream_id = self._ensure_panel(category, panel_name, color)

        # 构造可读的日志行
        parts = []
        if step >= 0:
            if total_steps > 0:
                parts.append(f"[{step}/{total_steps}]")
            else:
                parts.append(f"[#{step}]")

        parts.append(f"<{type_name}>")

        if description:
            parts.append(description)

        # 额外字段
        node_id = event.get("node_id", -1)
        constraint_id = event.get("constraint_id", -1)
        rule_id = event.get("rule_id", -1)
        if node_id >= 0:
            parts.append(f"node={node_id}")
        if constraint_id >= 0:
            parts.append(f"constraint={constraint_id}")
        if rule_id >= 0:
            parts.append(f"rule={rule_id}")

        progress = event.get("progress", -1)
        if progress >= 0:
            parts.append(f"({progress*100:.0f}%)")

        text = " ".join(parts)

        # 控制台输出（带颜色标记）
        time_str = time.strftime("%H:%M:%S")
        print(f"[{time_str}] [{category:12s}] {text}")

        # 推送到 WebSocket
        self._send_to_ws(stream_id, text)

    def _parse_stdout_line(self, line):
        """解析 stdout 中的一行 JSON-RPC notification"""
        line = line.strip()
        if not line:
            return

        # 过滤掉 stderr（stderr 也可能通过管道过来）
        if line.startswith("[") and ("ERR" in line or "OK" in line):
            # 这是 CHECK 宏的输出，忽略
            return
        if line.startswith("===") or line.startswith("---"):
            return  # 分隔线

        try:
            msg = json.loads(line)
        except json.JSONDecodeError:
            # 非 JSON 行 —— 可能是普通日志，发送到 info 面板
            self._send_to_ws("lv00-info", line)
            return

        # JSON-RPC notification 格式: {"jsonrpc":"2.0","method":"stream.event","params":{...}}
        if msg.get("method") == "stream.event" and "params" in msg:
            self._process_event(msg["params"])
        elif msg.get("type") == "stream.event":
            self._process_event(msg.get("params", msg))
        else:
            # 其他 JSON 消息
            self._send_to_ws("lv00-info", json.dumps(msg, ensure_ascii=False))

    def run_binary(self):
        """运行 Lv-00 二进制并捕获输出"""
        # 输入验证：确保 binary_path 指向一个有效的可执行文件
        binary_path = os.path.abspath(self.binary_path)
        if not os.path.isfile(binary_path):
            print(f"[BRIDGE] 错误: 路径无效或非文件 '{self.binary_path}'")
            return False
        # 安全检查：拒绝执行项目目录外的文件，防止路径遍历
        project_root = os.path.abspath(os.path.dirname(__file__))
        if not binary_path.startswith(project_root):
            print(f"[BRIDGE] 错误: 禁止执行项目目录外的文件 '{self.binary_path}'")
            return False

        args = [self.binary_path]
        if self.demo_num is not None:
            args.append(str(self.demo_num))

        print(f"[BRIDGE] 启动进程: {' '.join(args)}")
        try:
            self.process = subprocess.Popen(
                args,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                bufsize=1,  # 行缓冲
                encoding='utf-8',
                errors='replace',
            )
        except FileNotFoundError:
            print(f"[BRIDGE] 错误: 找不到二进制文件 '{self.binary_path}'")
            print("[BRIDGE] 请先构建项目: cd build && cmake --build . --target example_streaming")
            return False

        # 启动 stderr 读取线程
        def read_stderr():
            for line in self.process.stderr:
                if not self.running:
                    break
                line = line.rstrip()
                if line:
                    print(f"[STDERR] {line}")

        stderr_thread = threading.Thread(target=read_stderr, daemon=True)
        stderr_thread.start()

        # 读取 stdout（JSON-RPC 事件）
        for line in self.process.stdout:
            if not self.running:
                break
            self._parse_stdout_line(line)

        self.process.wait()
        print(f"\n[BRIDGE] 进程退出 (exitcode={self.process.returncode})")
        self._send_to_ws("lv00-info", f"═══ 引擎已完成 (退出码: {self.process.returncode}) ═══")
        return True

    def stop(self):
        """停止桥接器"""
        self.running = False
        if self.process and self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.process.kill()
        if self.ws:
            try:
                self.ws.close()
            except Exception as e:
                # 关闭时异常可忽略
                logger.debug("关闭WebSocket连接时异常（可忽略）: %s", e)
        print(f"\n[BRIDGE] 已停止。共处理 {self.event_count} 个事件。")


def main():
    parser = argparse.ArgumentParser(
        description="Lv-00 流式事件桥接器 —— 将引擎事件实时转发到 Web 监控面板"
    )
    parser.add_argument(
        "--ws", default="ws://localhost:3456",
        help="stream-monitor WebSocket 地址 (默认: ws://localhost:3456)"
    )
    parser.add_argument(
        "--binary",
        default=None,
        help="Lv-00 演示二进制路径 (默认: build/example_streaming 或 build/example_streaming.exe)"
    )
    parser.add_argument(
        "--demo", type=int, default=None,
        help="指定演示编号 (1=三角形, 2=圆交点, 3=函数块, 不指定=全部)"
    )
    parser.add_argument(
        "--no-ws", action="store_true",
        help="不连接 WebSocket，仅输出到控制台"
    )

    args = parser.parse_args()

    # 自动检测二进制路径
    if args.binary is None:
        project_root = os.path.dirname(os.path.abspath(__file__))
        candidates = [
            os.path.join(project_root, "build", "example_streaming.exe"),
            os.path.join(project_root, "build", "Debug", "example_streaming.exe"),
            os.path.join(project_root, "build", "Release", "example_streaming.exe"),
            os.path.join(project_root, "build", "example_streaming"),
        ]
        binary_path = None
        for cand in candidates:
            if os.path.exists(cand):
                binary_path = cand
                break
        if binary_path is None:
            print("[BRIDGE] 错误: 找不到编译好的 example_streaming 二进制文件")
            print("[BRIDGE] 请先运行: cd build && cmake --build . --target example_streaming")
            sys.exit(1)
    else:
        binary_path = args.binary

    print("╔════════════════════════════════════════════╗")
    print("║  Lv-00 流式事件桥接器 v1.0                ║")
    print("╚════════════════════════════════════════════╝")
    print(f"  二进制: {binary_path}")
    print(f"  WebSocket: {args.ws if not args.no_ws else '禁用'}")
    print()

    bridge = Lv00StreamBridge(
        ws_url=args.ws,
        binary_path=binary_path,
        demo_num=args.demo,
    )

    # 信号处理 —— 使用 graceful shutdown 标志
    # 不在信号处理器中调用 sys.exit()，避免跳过 finally 块和资源清理。
    # 【修复 #4】使用非局部变量（nonlocal）而非类属性来设置关闭标志。
    # 原代码中 signal_handler() 内部设置 _shutdown_requested = True，
    # 但由于 Python 的作用域规则，这会在 signal_handler 函数内创建一个局部变量
    # 而非修改外层的 _shutdown_requested。同时末尾引用 Lv00StreamBridge._shutdown_requested
    # 也是错误的，因为 _shutdown_requested 并非类属性。
    # 修复方案：使用 nonlocal 声明确保修改的是外层变量。
    _shutdown_requested = False

    def signal_handler(sig, frame):
        nonlocal _shutdown_requested
        print("\n[BRIDGE] 收到中断信号，正在优雅关闭...")
        bridge.stop()
        _shutdown_requested = True

    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    # 连接 WebSocket
    if not args.no_ws:
        bridge._connect_ws()

    # 运行二进制（主循环会检查 shutdown 标志）
    try:
        bridge.run_binary()
    finally:
        # 确保无论何种退出路径都执行清理
        time.sleep(0.5)
        bridge.stop()

    # graceful shutdown: 让 Python 解释器自然退出，确保所有 finally 块执行完毕
    if _shutdown_requested:
        print("[BRIDGE] 优雅关闭完成。")


if __name__ == "__main__":
    main()
