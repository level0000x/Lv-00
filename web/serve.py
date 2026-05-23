#!/usr/bin/env python3
"""
Lv-00 本地 HTTP 开发服务器
为 .js 和 .wasm 等文件设置正确的 MIME 类型，并禁用浏览器缓存。

用法:
    python serve.py [端口号]

示例:
    python serve.py          # 默认使用 8080 端口
    python serve.py 9000     # 使用 9000 端口

特性:
    - 多线程处理（ThreadingTCPServer），支持并发请求
    - 请求日志记录，便于安全审计和问题排查
    - 基础路径访问控制，阻止访问隐藏文件和敏感目录
    - 正确的 MIME 类型映射
    - 开发模式禁用浏览器缓存
"""

import http.server
import logging
import os
import posixpath
import socketserver
import sys
from datetime import datetime
from typing import Any
from urllib.parse import unquote

# ========== 配置 ==========

# 调试模式开关：通过环境变量 LV00_DEBUG=1 启用，默认关闭
_DEBUG = os.environ.get('LV00_DEBUG', '0') == '1'

# 默认端口号
_DEFAULT_PORT = 8080

# 从命令行参数获取端口号，未指定则使用默认值
try:
    PORT = int(sys.argv[1]) if len(sys.argv) > 1 else _DEFAULT_PORT
except (ValueError, IndexError):
    sys.stderr.write(f"错误: 无效的端口号 '{sys.argv[1]}'，请使用整数\n")
    sys.exit(1)

# 配置日志记录器
logging.basicConfig(
    level=logging.DEBUG if _DEBUG else logging.INFO,
    format='[%(asctime)s] %(levelname)s - %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S',
)
_log = logging.getLogger('serve')

# ========== 路径访问控制 ==========

# 禁止访问的路径前缀列表（大小写不敏感匹配）
# 阻止对隐藏文件、敏感目录和服务器脚本本身的访问
_BLOCKED_PATH_PREFIXES = [
    '.',            # 所有以点开头的隐藏文件/目录（.git, .env, .htaccess 等）
    '__pycache__',  # Python 字节码缓存目录
    'node_modules', # Node.js 依赖目录
    'serve.py',     # 本服务器脚本
]

# 允许的文件扩展名白名单（为空表示允许所有扩展名）
# 如果配置了白名单，只有这些扩展名的文件才能被访问
_ALLOWED_EXTENSIONS = []  # 留空允许所有，可配置如 ['.html', '.js', '.css', '.wasm', '.json', '.svg']


def _is_path_allowed(request_path: str) -> bool:
    """检查请求路径是否被允许访问。

    执行以下安全检查：
    1. 拒绝包含 '..' 的路径（目录遍历攻击）
    2. 拒绝以被禁止前缀开头的路径段
    3. 检查文件扩展名白名单（如果配置了）

    参数：
        request_path: 请求的 URL 路径

    返回：
        True 表示允许访问，False 表示拒绝
    """
    # 解码 URL 编码的路径
    decoded = unquote(request_path)

    # 规范化路径
    normalized = posixpath.normpath(decoded)

    # 拒绝包含 '..' 的路径（目录遍历攻击）
    if '..' in normalized:
        if _DEBUG:
            _log.debug("拒绝目录遍历请求: %s", request_path)
        return False

    # 检查路径中的每个段是否匹配被禁止的前缀
    segments = normalized.strip('/').split('/')
    for segment in segments:
        if not segment:
            continue
        segment_lower = segment.lower()
        for prefix in _BLOCKED_PATH_PREFIXES:
            if segment_lower.startswith(prefix.lower()):
                if _DEBUG:
                    _log.debug("拒绝被禁止路径: %s (匹配前缀 '%s')", request_path, prefix)
                return False

    # 如果配置了文件扩展名白名单，检查请求的文件扩展名
    if _ALLOWED_EXTENSIONS:
        _, ext = os.path.splitext(normalized)
        if ext.lower() not in [e.lower() for e in _ALLOWED_EXTENSIONS]:
            if _DEBUG:
                _log.debug("拒绝非白名单扩展名请求: %s (扩展名 '%s')", request_path, ext)
            return False

    return True


# ========== 请求处理器（修复猴子补丁签名问题） ==========

def _log_request(self: Any, code: object = '-', size: object = '-') -> None:
    """记录每个 HTTP 请求，便于安全审计和问题排查。

    修复了原实现签名不匹配的问题：BaseHTTPRequestHandler.send_response()
    会以 code 参数调用 self.log_request(code)，原实现只定义了 (self) 一个参数，
    导致 TypeError 异常且无日志输出。

    参数：
        self: HTTP 请求处理器实例
        code: HTTP 响应状态码
        size: 响应体大小
    """
    timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    print(
        f"[{timestamp}] {self.client_address[0]} - "
        f'"{self.command} {self.path}" {code} -'
    )


def _translate_path(self: Any, path: str) -> str:
    """重写路径翻译方法，防止目录遍历攻击。

    拒绝包含 '..' 的路径和绝对路径，确保请求不会逃逸出服务根目录。

    参数：
        self: HTTP 请求处理器实例
        path: 请求的 URL 路径

    返回：
        安全的文件系统路径
    """
    # 解码 URL 编码的路径
    decoded_path = unquote(path)

    # 拒绝包含 '..' 的路径，防止目录遍历
    if '..' in decoded_path:
        if _DEBUG:
            _log.debug("  [安全] 拒绝目录遍历请求: %s", path)
        return os.path.join(os.getcwd(), '__invalid__')

    # 规范化路径，防止编码绕过
    normalized = posixpath.normpath(decoded_path)

    # 拒绝绝对路径
    if normalized.startswith('/'):
        if _DEBUG:
            _log.debug("  [安全] 拒绝绝对路径请求: %s", path)
        return os.path.join(os.getcwd(), '__invalid__')

    # 调用原始方法完成正常路径翻译
    result = http.server.SimpleHTTPRequestHandler.translate_path(self, path)

    # 最终安全检查：确保解析后的路径仍在服务根目录内
    server_root = os.path.realpath(os.getcwd())
    resolved = os.path.realpath(result)
    if not resolved.startswith(server_root + os.sep) and resolved != server_root:
        if _DEBUG:
            _log.debug("  [安全] 拒绝逃逸根目录请求: %s -> %s", path, resolved)
        return os.path.join(os.getcwd(), '__invalid__')

    return result


def _end_headers(self: Any) -> None:
    """重写 end_headers 方法，添加禁用缓存的响应头。

    参数：
        self: HTTP 请求处理器实例
    """
    self.send_header('Cache-Control', 'no-cache, no-store, must-revalidate')
    self.send_header('Pragma', 'no-cache')
    self.send_header('Expires', '0')
    # 调用被保存的原始 end_headers 方法
    _orig_end_headers(self)


# 保存原始 end_headers 的引用（在设置猴子补丁之前）
_orig_end_headers = http.server.SimpleHTTPRequestHandler.end_headers

# 获取 Handler 类的引用（用于设置猴子补丁）
Handler = http.server.SimpleHTTPRequestHandler

# 将自定义方法绑定到 Handler 类
Handler.log_request = _log_request
Handler.translate_path = _translate_path
Handler.end_headers = _end_headers

# 修复 MIME 类型映射，确保浏览器正确识别文件类型
Handler.extensions_map.update({
    '.js': 'application/javascript',
    '.mjs': 'application/javascript',
    '.wasm': 'application/wasm',
    '.json': 'application/json',
    '.html': 'text/html',
    '.css': 'text/css',
    '.svg': 'image/svg+xml',
})


# ========== 自定义请求处理器（带访问控制） ==========

class SecureHTTPRequestHandler(Handler):
    """扩展的 HTTP 请求处理器，在文件服务前添加路径访问控制。

    继承自经过猴子补丁修改的 Handler，额外添加 _is_path_allowed() 检查。
    """

    def do_GET(self) -> None:
        """处理 GET 请求，先检查路径安全性再服务文件。"""
        if not _is_path_allowed(self.path):
            self.send_error(403, 'Forbidden: 路径访问被拒绝')
            return
        super().do_GET()

    def do_HEAD(self) -> None:
        """处理 HEAD 请求，先检查路径安全性再返回文件头。"""
        if not _is_path_allowed(self.path):
            self.send_error(403, 'Forbidden: 路径访问被拒绝')
            return
        super().do_HEAD()


# ========== 多线程服务器 ==========

class ReusableThreadingTCPServer(socketserver.ThreadingTCPServer):
    """支持地址复用和多线程的 TCP 服务器。

    修复问题：
    - 原实现直接设置 socketserver.TCPServer.allow_reuse_address 修改类级别属性，
      影响同一进程中所有 TCPServer 实例的行为。
    - 改为在线程安全服务器子类上设置实例属性，仅影响当前服务器实例。
    - 使用 ThreadingTCPServer 替代单线程 TCPServer，
      每个请求在独立线程中处理，避免阻塞其他请求。
    """
    allow_reuse_address = True  # 实例级别属性，不影响进程中的其他服务器


# ========== 启动服务器 ==========

if _DEBUG:
    _log.info("服务器启动在 http://localhost:%d", PORT)
    _log.info("按 Ctrl+C 停止")
    _log.info("访问控制: 已启用 (阻止隐藏文件和敏感目录)")
    _log.info("线程模式: 多线程 (ThreadingTCPServer)")

try:
    with ReusableThreadingTCPServer(("127.0.0.1", PORT), SecureHTTPRequestHandler) as httpd:
        _log.info("HTTP 开发服务器已启动: http://127.0.0.1:%d", PORT)
        httpd.serve_forever()
except PermissionError:
    sys.stderr.write(f"\n错误: 端口 {PORT} 需要管理员权限或已被占用\n")
    sys.stderr.write(f"请尝试其他端口: python {sys.argv[0]} 9000\n")
except OSError as e:
    sys.stderr.write(f"\n错误: 无法绑定端口 {PORT}: {e}\n")
    sys.stderr.write(f"请尝试其他端口: python {sys.argv[0]} 9000\n")
except KeyboardInterrupt:
    _log.info("服务器已停止")
