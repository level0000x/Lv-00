/**
 * StreamMonitor - 多路并发输出实时监控仪表盘（服务端）
 *
 * @description 基于 Express + WebSocket 的实时日志监控服务。
 *              支持多路并发流管理、命令执行捕获、内置 Demo 数据、
 *              外部 API 推送和 WebSocket 直推。
 * @module server
 * @requires express
 * @requires http
 * @requires ws
 * @requires path
 * @requires fs
 * @requires child_process
 */

const http = require('http');
const crypto = require('crypto');  // 【安全措施】用于 API 密钥的恒定时间比较，防止时序攻击
const fs = require('fs');
const path = require('path');
const { spawn } = require('child_process');
const WebSocket = require('ws');

const express = require('express');
const app = express();
const server = http.createServer(app);
const wss = new WebSocket.Server({ server });

const PORT = process.env.PORT || 3456;

// ========== 安全配置 ==========

/**
 * 【安全措施】API 密钥认证
 * 通过环境变量 API_KEY 配置 API 密钥。
 * 设置后，所有 REST API 和 WebSocket 端点必须携带有效的 API 密钥才能访问。
 * 未设置时（开发模式），认证功能自动禁用，并在启动时输出警告。
 *
 * 使用方式：
 *   - REST API: 在请求头中添加 Authorization: Bearer <API_KEY>
 *   - WebSocket: 连接 URL 中添加 ?api_key=<API_KEY> 或在首条消息中携带 api_key 字段
 */
const API_KEY = process.env.API_KEY || '';

/**
 * 【安全措施】CORS 允许的来源
 * 通过环境变量 CORS_ORIGINS 配置允许的跨域来源，多个来源用逗号分隔。
 * 默认仅允许同源请求（空字符串表示不允许任何跨域请求）。
 * 示例: CORS_ORIGINS=http://localhost:3000,https://example.com
 *
 * 安全说明：生产环境严禁使用通配符 '*'，以防止 CSRF 攻击。
 */
const CORS_ORIGINS = (process.env.CORS_ORIGINS || '').split(',').map(s => s.trim()).filter(Boolean);

/**
 * 【安全措施】命令黑名单
 * 禁止执行的命令关键字列表。任何包含这些关键字的命令都将被拒绝执行。
 * 这可以有效防止命令注入攻击，阻止执行危险的系统命令。
 */
const COMMAND_BLACKLIST = [
  'rm -rf', 'rm -r', 'mkfs', 'dd if=', 'shutdown', 'reboot', 'halt',
  'init 0', 'init 6', 'poweroff',
  ':(){ :|:& };:',  // Fork 炸弹
  'chmod 777', 'chown root',
  'wget', 'curl', 'nc ', 'ncat', 'netcat',  // 网络工具（防止反弹 shell）
  'passwd', 'useradd', 'userdel', 'groupadd',
  'crontab', 'systemctl', 'service',
  'iptables', 'ufw',
  'sudo', 'su ', 'su\n',
  'eval', 'exec', 'source',
  '> /dev/', '>> /etc/',
  'python -c', 'node -e', 'perl -e', 'ruby -e',  // 脚本内联执行
  '/etc/passwd', '/etc/shadow', '/etc/sudoers',
  'kill -9', 'pkill', 'killall',
];

/**
 * 【安全措施】命令白名单（正则模式）
 * 允许执行的命令必须匹配白名单中的至少一个正则模式。
 * 白名单优先于黑名单：只有同时通过白名单和黑名单检查的命令才会被执行。
 *
 * 默认白名单允许常见的安全命令（如 npm, node, python, git, ping 等）。
 * 可通过环境变量 COMMAND_WHITELIST 覆盖，多个模式用逗号分隔。
 *
 * 安全说明：白名单采用严格匹配，仅允许已知的、安全的可执行程序名称。
 */
const DEFAULT_WHITELIST_PATTERNS = [
  '^(npm|npx|yarn|pnpm|bun)(\\s|$)',           // 包管理器
  '^(node|deno|ts-node|tsx)(\\s|$)',             // JS/TS 运行时
  '^(python[23]?|pip[23]?|python3\\.[0-9]+)(\\s|$)',  // Python
  '^(java|javac|mvn|gradle)(\\s|$)',             // Java
  '^(go run|go build|go test)(\\s|$)',           // Go
  '^(rustc|cargo)(\\s|$)',                       // Rust
  '^(gcc|g\\+\\+|clang|make|cmake)(\\s|$)',     // C/C++ 编译
  '^(git)(\\s|$)',                               // Git
  '^(docker|docker-compose|podman)(\\s|$)',      // 容器
  '^(ping|traceroute|nslookup|dig)(\\s|$)',      // 网络诊断
  '^(cat|ls|dir|echo|pwd|whoami|date|uname)(\\s|$)',  // 基本系统命令
  '^(tail|head|grep|find|wc|sort|uniq|awk|sed)(\\s|$)', // 文本处理
  '^(npm run)(\\s|$)',                           // npm 脚本
  '^(jest|mocha|vitest|pytest|cargo test)(\\s|$)', // 测试框架
];

const COMMAND_WHITELIST = (process.env.COMMAND_WHITELIST || '')
  ? (process.env.COMMAND_WHITELIST).split(',').map(s => s.trim()).filter(Boolean)
  : DEFAULT_WHITELIST_PATTERNS;

/**
 * 【安全措施】验证命令安全性
 * 对用户提交的命令进行白名单和黑名单双重检查。
 *
 * @param {string} command - 用户提交的命令字符串
 * @returns {{ safe: boolean, reason: string }} 验证结果，safe=true 表示安全
 */
function validateCommand(command) {
  if (!command || typeof command !== 'string') {
    return { safe: false, reason: '命令不能为空' };
  }

  // 去除首尾空白后检查
  const trimmed = command.trim();

  // 检查命令长度
  if (trimmed.length === 0) {
    return { safe: false, reason: '命令不能为空' };
  }

  // 【白名单检查】命令必须匹配至少一个白名单模式
  const whitelistMatch = COMMAND_WHITELIST.some(pattern => {
    try {
      return new RegExp(pattern, 'i').test(trimmed);
    } catch (e) {
      return false;
    }
  });
  if (!whitelistMatch) {
    return { safe: false, reason: `命令不在允许的白名单中。允许的命令模式: ${COMMAND_WHITELIST.join(', ')}` };
  }

  // 【黑名单检查】命令不能包含任何黑名单关键字
  const lowerCmd = trimmed.toLowerCase();
  for (const blocked of COMMAND_BLACKLIST) {
    if (lowerCmd.includes(blocked.toLowerCase())) {
      return { safe: false, reason: `命令包含被禁止的关键字: "${blocked}"` };
    }
  }

  // 【管道和重定向检查】防止通过管道符或重定向符进行命令注入
  // 当使用 shell: false 时，这些字符不会被执行，但仍做额外防护
  const dangerousChars = ['|', '`', '$(', '${', '&&', '||', ';', '\n', '\r'];
  for (const char of dangerousChars) {
    if (trimmed.includes(char)) {
      return { safe: false, reason: `命令包含危险字符: "${char}"` };
    }
  }

  return { safe: true, reason: '' };
}

/**
 * 【安全措施】验证命令参数安全性
 * 对命令参数数组中的每个参数进行检查，防止通过参数注入恶意内容。
 *
 * @param {string[]} args - 命令参数数组
 * @returns {{ safe: boolean, reason: string }} 验证结果
 */
function validateArgs(args) {
  if (!Array.isArray(args)) {
    return { safe: false, reason: '参数必须是数组' };
  }

  // 参数数量限制，防止参数炸弹
  if (args.length > 100) {
    return { safe: false, reason: '参数数量不能超过 100 个' };
  }

  const dangerousPatterns = ['|', '`', '$(', '${', '&&', '||', ';', '>', '<', '\n', '\r'];
  for (let i = 0; i < args.length; i++) {
    const arg = String(args[i]);
    if (arg.length > 2000) {
      return { safe: false, reason: `参数 #${i} 长度超过限制` };
    }
    for (const pattern of dangerousPatterns) {
      if (arg.includes(pattern)) {
        return { safe: false, reason: `参数 #${i} 包含危险字符: "${pattern}"` };
      }
    }
  }

  return { safe: true, reason: '' };
}

// ========== 日志系统 ==========

/**
 * 简易结构化日志记录器
 * 输出格式：[时间戳] [级别] 消息
 */
class Logger {
  /** 获取当前时间的格式化字符串 */
  static _timestamp() {
    const now = new Date();
    const pad = (n) => String(n).padStart(2, '0');
    return `${now.getFullYear()}-${pad(now.getMonth() + 1)}-${pad(now.getDate())} ` +
           `${pad(now.getHours())}:${pad(now.getMinutes())}:${pad(now.getSeconds())}`;
  }

  /** 普通信息日志 */
  static info(msg) {
    console.log(`[${Logger._timestamp()}] [INFO]  ${msg}`);
  }

  /** 警告日志 */
  static warn(msg) {
    console.warn(`[${Logger._timestamp()}] [WARN]  ${msg}`);
  }

  /** 错误日志 */
  static error(msg) {
    console.error(`[${Logger._timestamp()}] [ERROR] ${msg}`);
  }

  /** 调试日志（仅在 DEBUG 环境变量开启时输出） */
  static debug(msg) {
    if (process.env.DEBUG) {
      console.debug(`[${Logger._timestamp()}] [DEBUG] ${msg}`);
    }
  }
}

// ========== 配置常量 ==========

/** 每个流最多缓存的行数 */
const MAX_BUFFER_LINES = 5000;

/** 最大并发流数量（防止资源耗尽） */
const MAX_STREAMS = 100;

/** 流面板预定义颜色列表 */
const COLORS = [
  '#00e676', '#448aff', '#ff9100', '#e040fb',
  '#00bcd4', '#ff5252', '#69f0ae', '#ffd740',
  '#40c4ff', '#b2ff59', '#ff80ab', '#ffff00',
];

/** WebSocket 心跳间隔（毫秒） */
const WS_HEARTBEAT_INTERVAL = 30000;

/** WebSocket 心跳超时（毫秒）：超过此时间未收到 pong 则断开 */
const WS_HEARTBEAT_TIMEOUT = 10000;

/** 优雅退出强制超时（毫秒） */
const SHUTDOWN_TIMEOUT = 10000;

// ========== 流管理器 ==========

class StreamManager {
  constructor() {
    /** @type {Map<string, Stream>} 流映射表（使用 Map 保证原子操作） */
    this.streams = new Map();
    /** 自增 ID 计数器 */
    this.nextId = 1;
    /**
     * Demo 定时器映射表，key 为流 ID，value 为 setInterval 返回的定时器句柄。
     * 使用 Map 而非数组，以便在单个流被删除时能精确停止对应的定时器，
     * 避免定时器在流删除后继续空转造成资源泄漏。
     * @type {Map<string, NodeJS.Timeout>}
     */
    this.demoIntervals = new Map();
  }

  /**
   * 创建一个新的输出流
   * @param {string} name - 流显示名称
   * @param {string} [color] - 前端面板颜色
   * @returns {Stream|null} 新创建的流对象，超过上限时返回 null
   */
  createStream(name, color) {
    // 流数量上限保护，防止资源耗尽
    if (this.streams.size >= MAX_STREAMS) {
      Logger.warn(`创建流失败：已达到最大流数量 ${MAX_STREAMS}`);
      return null;
    }

    const id = String(this.nextId++);
    const stream = {
      id,
      name: name || `stream-${id}`,
      color: color || COLORS[(id - 1) % COLORS.length],
      buffer: [],            // 行缓冲区 [{text, timestamp}]
      subscribers: new Set(), // WebSocket 订阅者集合
      process: null,         // child_process 引用
      status: 'ready',       // 'ready' | 'running' | 'stopped' | 'error'
      startTime: null,       // 启动时间戳
    };
    this.streams.set(id, stream);
    Logger.info(`创建流 "${stream.name}" (id=${id})`);
    return stream;
  }

  /**
   * 获取或创建流（用于 WebSocket 直推场景）
   * 使用 Map.set/get 原子操作，避免先创建再删除旧 key 的竞态条件
   * @param {string} id - 流 ID
   * @param {string} name - 流名称
   * @returns {Stream|null} 流对象，超过上限时返回 null
   */
  getOrCreate(id, name) {
    // 先尝试获取已存在的流
    const existing = this.streams.get(id);
    if (existing) return existing;

    // 流不存在时创建新流，直接使用调用方指定的 ID
    if (this.streams.size >= MAX_STREAMS) {
      Logger.warn(`getOrCreate 失败：已达到最大流数量 ${MAX_STREAMS}`);
      return null;
    }

    const stream = {
      id,
      name: name || `stream-${id}`,
      color: COLORS[(this.nextId - 1) % COLORS.length],
      buffer: [],
      subscribers: new Set(),
      process: null,
      status: 'ready',
      startTime: null,
    };
    this.streams.set(id, stream);
    Logger.info(`创建流 "${stream.name}" (id=${id})`);
    return stream;
  }

  /**
   * 向流追加一行输出，并广播给所有已连接的 WebSocket 客户端
   * @param {string} streamId - 流 ID
   * @param {string} text - 输出文本
   */
  pushLine(streamId, text) {
    const stream = this.streams.get(streamId);
    if (!stream) {
      Logger.warn(`pushLine 失败：流 ${streamId} 不存在`);
      return;
    }

    // 构建日志条目
    const entry = {
      text,
      timestamp: Date.now(),
      streamId,
      streamName: stream.name,
      color: stream.color,
    };

    // 写入缓冲区并限制大小
    stream.buffer.push(entry);
    if (stream.buffer.length > MAX_BUFFER_LINES) {
      stream.buffer = stream.buffer.slice(-MAX_BUFFER_LINES);
    }

    // 广播给所有已连接的 WebSocket 客户端
    const msg = JSON.stringify({ type: 'output', ...entry });
    wss.clients.forEach(client => {
      if (client.readyState === WebSocket.OPEN) {
        // 客户端未订阅任何流时接收全部；已订阅时仅推送给已订阅的流
        if (!client._subscriptions || client._subscriptions.size === 0 || client._subscriptions.has(streamId)) {
          try {
            client.send(msg);
          } catch (err) {
            // 单个客户端发送失败不影响其他客户端
            Logger.debug(`WebSocket 发送失败: ${err.message}`);
          }
        }
      }
    });
  }

  /**
   * 运行外部命令并捕获其 stdout/stderr 输出到指定流
   *
   * 【安全措施】
   * 1. 移除了 shell: true，直接执行命令程序，防止 shell 注入攻击。
   *    使用 shell: true 时，用户输入会经过 shell 解释器处理，攻击者可以通过
   *    分号、管道符、反引号等 shell 元字符注入任意系统命令。
   *    移除后，spawn 会将 command 作为可执行文件直接启动，args 作为参数传递，
   *    不经过任何 shell 解释，从根本上杜绝了命令注入漏洞。
   * 2. 在执行前进行命令白名单和黑名单双重验证。
   * 3. 不将完整命令记录到流输出中，防止信息泄露。
   *
   * @param {string} streamId - 流 ID
   * @param {string} command - 要执行的命令（仅可执行程序名或路径）
   * @param {string[]} [args] - 命令参数数组
   * @param {string} [cwd] - 工作目录
   */
  runCommand(streamId, command, args, cwd) {
    const stream = this.streams.get(streamId);
    if (!stream) {
      Logger.error(`runCommand 失败：流 ${streamId} 不存在`);
      return;
    }

    // 【安全检查】在执行前验证命令和参数的合法性
    const cmdValidation = validateCommand(command);
    if (!cmdValidation.safe) {
      stream.status = 'error';
      this.pushLine(streamId, `>>> [系统] 命令被拒绝: ${cmdValidation.reason}`);
      Logger.warn(`命令执行被拒绝 (流 "${stream.name}"): ${cmdValidation.reason}`);
      return;
    }

    const argsValidation = validateArgs(args);
    if (!argsValidation.safe) {
      stream.status = 'error';
      this.pushLine(streamId, `>>> [系统] 参数验证失败: ${argsValidation.reason}`);
      Logger.warn(`命令参数被拒绝 (流 "${stream.name}"): ${argsValidation.reason}`);
      return;
    }

    stream.status = 'running';
    stream.startTime = Date.now();

    // 【安全措施】仅记录命令名称（不含参数），防止敏感信息泄露到流输出中
    // 完整命令仅记录到服务端日志（仅管理员可见），不广播给前端用户
    const cmdName = command.trim().split(/\s+/)[0];
    this.pushLine(streamId, `>>> [系统] 启动命令: ${cmdName}`);
    Logger.info(`流 "${stream.name}" 启动命令: ${command} ${(args || []).join(' ')}`);

    try {
      // 【安全措施】移除 shell: true，使用 shell: false 直接执行命令
      // 当 shell: true 时，spawn 会通过系统 shell（cmd.exe / /bin/sh）解释命令，
      // 攻击者可通过分号、管道符、反引号、$() 等 shell 元字符注入任意系统命令。
      // 使用 shell: false 后，command 作为可执行文件路径，args 作为独立参数传递，
      // 不经过任何 shell 解释器，从根本上杜绝命令注入漏洞。
      //
      // 依赖的安全机制：
      //   1. shell: false — 禁用 shell 解释，command 必须是可执行程序名或绝对路径
      //   2. validateCommand() 白名单 — 仅允许已知安全的可执行程序
      //   3. validateArgs() — 验证每个参数不含危险字符
      //   4. COMMAND_BLACKLIST — 阻止已知危险命令
      //   5. 命令长度和参数数量限制
      //
      // 注意：当 shell: false 时，command 中的空格不会触发参数分隔，
      // 所有参数必须通过 args 数组传递。如有管道/重定向需求请在终端中执行。
      const child = spawn(command, args || [], {
        cwd: cwd || process.cwd(),
        shell: false,  // 【关键安全修复】禁用 shell，防止命令注入
        env: { ...process.env, FORCE_COLOR: '1', PYTHONUNBUFFERED: '1' },
      });
      stream.process = child;

      // 捕获标准输出
      child.stdout.on('data', (data) => {
        const lines = data.toString().split(/\r?\n/);
        lines.forEach(line => {
          if (line.length > 0 || lines.length > 1) {
            this.pushLine(streamId, line);
          }
        });
      });

      // 捕获标准错误
      child.stderr.on('data', (data) => {
        const lines = data.toString().split(/\r?\n/);
        lines.forEach(line => {
          if (line.length > 0 || lines.length > 1) {
            this.pushLine(streamId, `[STDERR] ${line}`);
          }
        });
      });

      // 进程退出处理
      child.on('close', (code) => {
        stream.status = 'stopped';
        this.pushLine(streamId, `>>> [系统] 进程退出，退出码: ${code}`);
        Logger.info(`流 "${stream.name}" 进程退出 (code=${code})`);
        // 广播流关闭事件给所有客户端
        const msg = JSON.stringify({ type: 'stream_closed', streamId, exitCode: code });
        wss.clients.forEach(c => {
          if (c.readyState === WebSocket.OPEN) {
            try {
              c.send(msg);
            } catch (err) {
              Logger.debug(`WebSocket 广播 stream_closed 失败: ${err.message}`);
            }
          }
        });
      });

      // 进程启动错误处理
      child.on('error', (err) => {
        stream.status = 'error';
        this.pushLine(streamId, `>>> [系统] 错误: ${err.message}`);
        Logger.error(`流 "${stream.name}" 进程错误: ${err.message}`);
        const msg = JSON.stringify({ type: 'stream_error', streamId, error: err.message });
        wss.clients.forEach(c => {
          if (c.readyState === WebSocket.OPEN) {
            try {
              c.send(msg);
            } catch (sendErr) {
              Logger.debug(`WebSocket 广播 stream_error 失败: ${sendErr.message}`);
            }
          }
        });
      });
    } catch (err) {
      stream.status = 'error';
      this.pushLine(streamId, `>>> [系统] 启动失败: ${err.message}`);
      Logger.error(`流 "${stream.name}" 启动失败: ${err.message}`);
    }
  }

  /**
   * 停止指定流（终止关联的 child_process）
   * @param {string} streamId - 流 ID
   */
  stopStream(streamId) {
    const stream = this.streams.get(streamId);
    if (!stream) return;
    if (stream.process) {
      stream.process.kill();
      stream.process = null;
    }
    stream.status = 'stopped';
    this.pushLine(streamId, '>>> [系统] 流已手动停止');
    Logger.info(`流 "${stream.name}" 已停止`);
  }

  /**
   * 删除指定流（先停止进程和关联 Demo 定时器，再移除流）
   * @param {string} streamId - 流 ID
   */
  deleteStream(streamId) {
    this.stopStream(streamId);
    // 清理与该流关联的 Demo 定时器，防止流删除后定时器继续空转造成资源泄漏
    const demoInterval = this.demoIntervals.get(streamId);
    if (demoInterval) {
      clearInterval(demoInterval);
      this.demoIntervals.delete(streamId);
      Logger.debug(`已停止流 ${streamId} 关联的 Demo 定时器`);
    }
    this.streams.delete(streamId);
    Logger.info(`流 ${streamId} 已删除`);
  }

  /**
   * 列出所有流的概要信息
   * @returns {Array<Object>} 流信息数组
   */
  listStreams() {
    return Array.from(this.streams.values()).map(s => ({
      id: s.id,
      name: s.name,
      color: s.color,
      status: s.status,
      bufferSize: s.buffer.length,
      startTime: s.startTime,
    }));
  }

  /**
   * 启动内置 Demo（4 路并发模拟输出）
   * 每个 Demo 流的定时器独立存储到 demoIntervals Map 中，
   * 以便 Delete API 单独删除某个流时可以精确停止对应的定时器。
   */
  startDemo() {
    this.stopDemo();

    // --- 流1：前端构建日志 ---
    const build = this.createStream('构建日志 (Vite)', COLORS[0]);
    build.status = 'running';
    const buildSteps = [
      'vite v5.4.0 building for production...',
      'transforming (1) src/main.tsx',
      'transforming (15) src/components/Header.tsx',
      'transforming (42) src/pages/Dashboard.tsx',
      'transforming (78) node_modules/@ant-design/icons/es/index.js',
      '128 modules transformed.',
      'rendering chunks (1)...',
      'computing gzip size (0)...',
      'dist/index.html                   0.45 kB  gzip: 0.30 kB',
      'dist/assets/index-DiwrgTda.css   15.82 kB  gzip: 3.74 kB',
      'dist/assets/index-C8aYN1Pn.js   234.17 kB  gzip: 72.35 kB',
      'built in 2.84s',
    ];
    let bi = 0;
    this.demoIntervals.set(build.id, setInterval(() => {
      if (bi < buildSteps.length && this.streams.has(build.id)) {
        this.pushLine(build.id, buildSteps[bi++]);
      }
      if (bi === buildSteps.length) {
        build.status = 'stopped';
        this.pushLine(build.id, '>>> [系统] 构建完成');
      }
    }, 800));

    // --- 流2：API 服务日志 ---
    const server2 = this.createStream('API 服务 (Express)', COLORS[1]);
    server2.status = 'running';
    const ips = ['192.168.1.101', '10.0.0.23', '172.16.0.5', '192.168.1.88', '10.0.0.42'];
    const methods = ['GET', 'POST', 'PUT', 'DELETE'];
    const paths = ['/api/users', '/api/orders', '/api/products', '/api/auth/login', '/api/reports'];
    const statuses = ['200', '200', '200', '201', '204', '400', '401', '404', '500'];
    let reqCount = 0;
    this.demoIntervals.set(server2.id, setInterval(() => {
      if (!this.streams.has(server2.id)) return;
      reqCount++;
      const ip = ips[Math.floor(Math.random() * ips.length)];
      const method = methods[Math.floor(Math.random() * methods.length)];
      const p = paths[Math.floor(Math.random() * paths.length)];
      const status = statuses[Math.floor(Math.random() * statuses.length)];
      const ms = Math.floor(Math.random() * 450) + 5;
      const now = new Date().toISOString();
      const line = `[${now}] ${ip} - "${method} ${p} HTTP/1.1" ${status} ${ms}ms`;
      this.pushLine(server2.id, line);

      // 偶尔输出警告
      if (reqCount % 15 === 0) {
        this.pushLine(server2.id, `[${now}] WARN: Connection pool nearing limit (48/50)`);
      }
      // 偶尔输出错误
      if (reqCount % 30 === 0) {
        const errMsgs = [
          'Error: connect ECONNREFUSED 127.0.0.1:5432',
          'UnhandledPromiseRejection: Query timeout after 30s',
          'Error: Redis connection lost - reconnecting...',
        ];
        this.pushLine(server2.id, `[${now}] ${errMsgs[Math.floor(Math.random() * errMsgs.length)]}`);
      }
    }, 400));

    // --- 流3：数据处理日志 ---
    const data = this.createStream('数据处理 (ETL)', COLORS[2]);
    data.status = 'running';
    let batch = 0;
    this.demoIntervals.set(data.id, setInterval(() => {
      if (!this.streams.has(data.id)) return;
      batch++;
      const total = 12740;
      const processed = Math.min(batch * 850, total);
      const pct = ((processed / total) * 100).toFixed(1);
      this.pushLine(data.id, `Batch #${batch}: 处理 ${Math.min(850, total - (batch - 1) * 850)} 条记录... (总计 ${processed}/${total}, ${pct}%)`);
      if (Math.random() < 0.25) {
        const skus = ['SKU-8842', 'SKU-1209', 'SKU-5531', 'SKU-9012'];
        this.pushLine(data.id, `  数据异常: ${skus[Math.floor(Math.random() * skus.length)]} 价格字段为空，已跳过`);
      }
      if (processed >= total) {
        this.pushLine(data.id, `全量数据同步完成! 总计 ${total} 条，耗时 ${(batch * 1.2).toFixed(1)}s`);
        data.status = 'stopped';
      }
    }, 1200));

    // --- 流4：系统监控日志 ---
    const sys = this.createStream('系统监控 (Metrics)', COLORS[3]);
    sys.status = 'running';
    let tick = 0;
    this.demoIntervals.set(sys.id, setInterval(() => {
      if (!this.streams.has(sys.id)) return;
      tick++;
      const cpu = (Math.random() * 60 + 20).toFixed(1);
      const mem = (Math.random() * 30 + 45).toFixed(1);
      const disk = (Math.random() * 10 + 55).toFixed(1);
      const netIn = (Math.random() * 800 + 200).toFixed(1);
      const netOut = (Math.random() * 300 + 50).toFixed(1);
      const now = new Date().toISOString();
      this.pushLine(sys.id, `[${now}] CPU:${cpu}% MEM:${mem}% DISK:${disk}% NET(in):${netIn}KB/s NET(out):${netOut}KB/s`);
      if (tick % 8 === 0 && parseFloat(cpu) > 70) {
        this.pushLine(sys.id, `[${now}] ALERT: CPU 使用率超过阈值 (${cpu}% > 70%)`);
      }
    }, 2000));

    Logger.info('Demo 启动完毕：4 路并发流');
  }

  /**
   * 停止所有 Demo 定时器
   */
  stopDemo() {
    this.demoIntervals.forEach((iv, streamId) => {
      clearInterval(iv);
      Logger.debug(`已停止 Demo 定时器 (流 ${streamId})`);
    });
    this.demoIntervals.clear();
  }
}

/** 全局流管理器实例 */
const manager = new StreamManager();

// ========== Express 中间件 ==========

// JSON 请求体解析，限制 1MB 防止恶意大请求耗尽内存
app.use(express.json({ limit: '1mb' }));
app.use(express.urlencoded({ limit: '1mb', extended: false }));

// 【安全措施】CORS 跨域配置
// 不再使用通配符 '*'，改为通过环境变量 CORS_ORIGINS 配置允许的来源列表。
// 仅在 CORS_ORIGINS 中列出的域名会被允许跨域访问，其他来源的请求将被拒绝。
// 如果请求的 Origin 不在允许列表中，则不返回 Access-Control-Allow-Origin 头。
app.use((req, res, next) => {
  const requestOrigin = req.headers.origin || '';

  // 检查请求来源是否在允许列表中
  if (requestOrigin && CORS_ORIGINS.includes(requestOrigin)) {
    res.header('Access-Control-Allow-Origin', requestOrigin);
    res.header('Vary', 'Origin');  // 提示代理服务器根据 Origin 进行缓存区分
  }
  // 如果没有配置 CORS_ORIGINS 或来源不匹配，则不设置 CORS 头，浏览器将阻止跨域请求

  res.header('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS');
  res.header('Access-Control-Allow-Headers', 'Content-Type, Authorization, X-Requested-With');
  // 预检请求直接返回 204
  if (req.method === 'OPTIONS') {
    return res.sendStatus(204);
  }
  next();
});

// 【安全措施】API 密钥认证中间件
// 当环境变量 API_KEY 已设置时，所有 /api/ 路径的请求必须携带有效的 API 密钥。
// 支持两种传递方式：
//   1. HTTP 请求头: Authorization: Bearer <API_KEY>
//   2. URL 查询参数: ?api_key=<API_KEY>
//
// 未设置 API_KEY 时（开发模式），认证中间件自动跳过，并在启动时输出警告日志。
// 静态文件和 WebSocket 连接不受此中间件影响（WebSocket 有独立的认证逻辑）。
app.use('/api', (req, res, next) => {
  // 未配置 API_KEY 时跳过认证（开发模式）
  if (!API_KEY) {
    return next();
  }

  // 从请求头或查询参数中提取 API 密钥
  const authHeader = req.headers.authorization || '';
  const bearerToken = authHeader.startsWith('Bearer ') ? authHeader.slice(7) : '';
  const queryToken = req.query.api_key || '';
  const token = bearerToken || queryToken;

  if (!token) {
    Logger.warn(`API 认证失败: 缺少 API 密钥 (${req.method} ${req.originalUrl})`);
    return res.status(401).json({ error: '未提供 API 密钥。请在 Authorization 头中携带 Bearer token 或通过 api_key 查询参数传递。' });
  }

  // 使用恒定时间比较，防止时序攻击（timing attack）
  if (token.length !== API_KEY.length || !crypto.timingSafeEqual(Buffer.from(token), Buffer.from(API_KEY))) {
    Logger.warn(`API 认证失败: API 密钥无效 (${req.method} ${req.originalUrl})`);
    return res.status(403).json({ error: 'API 密钥无效' });
  }

  next();
});

// 请求日志中间件
app.use((req, res, next) => {
  const start = Date.now();
  res.on('finish', () => {
    const duration = Date.now() - start;
    Logger.info(`${req.method} ${req.originalUrl} -> ${res.statusCode} (${duration}ms)`);
  });
  next();
});

// 静态文件服务（public 目录下的 HTML/CSS/JS）
app.use(express.static(path.join(__dirname, 'public')));

// ========== 请求验证辅助函数 ==========

/**
 * 验证请求体中的必填字段
 * @param {Object} body - 请求体
 * @param {string[]} fields - 必填字段列表
 * @returns {{ valid: boolean, missing: string[] }} 验证结果
 */
function validateBody(body, fields) {
  const missing = [];
  for (const field of fields) {
    if (body[field] === undefined || body[field] === null || body[field] === '') {
      missing.push(field);
    }
  }
  return { valid: missing.length === 0, missing };
}

/**
 * 检查并确保参数为合法的正整数
 * @param {*} value - 待检查的值
 * @param {number} defaultValue - 默认值
 * @param {number} [max=10000] - 最大值
 * @returns {number} 合法的数值
 */
function sanitizeInt(value, defaultValue, max = 10000) {
  const num = parseInt(value, 10);
  if (isNaN(num) || num < 0) return defaultValue;
  return Math.min(num, max);
}

// ========== Express 路由 ==========

// 首页
app.get('/', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

// API: 获取所有流
app.get('/api/streams', (req, res) => {
  try {
    res.json(manager.listStreams());
  } catch (err) {
    Logger.error(`GET /api/streams 异常: ${err.message}`);
    res.status(500).json({ error: '服务器内部错误' });
  }
});

// API: 获取指定流的历史记录
app.get('/api/streams/:id/history', (req, res) => {
  try {
    const { id } = req.params;
    const stream = manager.streams.get(id);
    if (!stream) {
      return res.status(404).json({ error: '指定流不存在' });
    }

    const lines = sanitizeInt(req.query.lines, stream.buffer.length);
    const offset = sanitizeInt(req.query.offset, 0);
    const start = Math.max(0, stream.buffer.length - lines - offset);

    res.json({
      streamId: stream.id,
      streamName: stream.name,
      status: stream.status,
      totalLines: stream.buffer.length,
      lines: stream.buffer.slice(start, stream.buffer.length - offset),
    });
  } catch (err) {
    Logger.error(`GET /api/streams/${req.params.id}/history 异常: ${err.message}`);
    res.status(500).json({ error: '服务器内部错误' });
  }
});

// API: 创建新流（通过命令）
app.post('/api/streams', (req, res) => {
  try {
    const { name, command, args, cwd } = req.body;

    // 请求验证：command 为必填字段
    const validation = validateBody(req.body, ['command']);
    if (!validation.valid) {
      return res.status(400).json({
        error: `缺少必填字段: ${validation.missing.join(', ')}`,
      });
    }

    // 命令长度限制，防止过长命令
    if (command.length > 2000) {
      return res.status(400).json({ error: '命令长度不能超过 2000 个字符' });
    }

    // 【安全措施】在路由层也进行命令验证，提供更友好的错误信息
    const cmdCheck = validateCommand(command);
    if (!cmdCheck.safe) {
      Logger.warn(`POST /api/streams 命令被拒绝: ${cmdCheck.reason}`);
      return res.status(403).json({ error: `命令被拒绝: ${cmdCheck.reason}` });
    }

    // 【安全措施】验证命令参数
    const argsCheck = validateArgs(args);
    if (!argsCheck.safe) {
      Logger.warn(`POST /api/streams 参数被拒绝: ${argsCheck.reason}`);
      return res.status(403).json({ error: `参数被拒绝: ${argsCheck.reason}` });
    }

    // 流名称长度限制
    const safeName = (name && name.length <= 200) ? name : command;
    const color = req.body.color || COLORS[manager.streams.size % COLORS.length];
    const stream = manager.createStream(safeName, color);
    if (!stream) {
      return res.status(429).json({ error: `已达到最大流数量限制 (${MAX_STREAMS})` });
    }
    manager.runCommand(stream.id, command, args || [], cwd);

    res.json({ id: stream.id, name: stream.name, color: stream.color, status: stream.status });
  } catch (err) {
    Logger.error(`POST /api/streams 异常: ${err.message}`);
    res.status(500).json({ error: '服务器内部错误' });
  }
});

// API: 停止指定流
app.post('/api/streams/:id/stop', (req, res) => {
  try {
    const { id } = req.params;
    if (!manager.streams.has(id)) {
      return res.status(404).json({ error: '指定流不存在' });
    }
    manager.stopStream(id);
    res.json({ ok: true });
  } catch (err) {
    Logger.error(`POST /api/streams/${req.params.id}/stop 异常: ${err.message}`);
    res.status(500).json({ error: '服务器内部错误' });
  }
});

// API: 删除指定流
app.delete('/api/streams/:id', (req, res) => {
  try {
    const { id } = req.params;
    if (!manager.streams.has(id)) {
      return res.status(404).json({ error: '指定流不存在' });
    }
    manager.deleteStream(id);
    res.json({ ok: true });
  } catch (err) {
    Logger.error(`DELETE /api/streams/${req.params.id} 异常: ${err.message}`);
    res.status(500).json({ error: '服务器内部错误' });
  }
});

// API: 启动 Demo
app.post('/api/demo/start', (req, res) => {
  try {
    manager.startDemo();
    res.json({ ok: true, streams: manager.listStreams() });
  } catch (err) {
    Logger.error(`POST /api/demo/start 异常: ${err.message}`);
    res.status(500).json({ error: '服务器内部错误' });
  }
});

// API: 停止 Demo
app.post('/api/demo/stop', (req, res) => {
  try {
    manager.stopDemo();
    res.json({ ok: true });
  } catch (err) {
    Logger.error(`POST /api/demo/stop 异常: ${err.message}`);
    res.status(500).json({ error: '服务器内部错误' });
  }
});

// API: 外部推送数据到指定流
app.post('/api/streams/:id/push', (req, res) => {
  try {
    const { text } = req.body;
    const { id } = req.params;

    // 请求验证：text 为必填字段
    const validation = validateBody(req.body, ['text']);
    if (!validation.valid) {
      return res.status(400).json({
        error: `缺少必填字段: ${validation.missing.join(', ')}`,
      });
    }

    // 文本长度限制
    if (text.length > 10000) {
      return res.status(400).json({ error: '文本内容不能超过 10000 个字符' });
    }

    let stream = manager.streams.get(id);
    if (!stream) {
      // 流不存在时自动创建
      stream = manager.getOrCreate(id, `input-${id}`);
      if (!stream) {
        return res.status(429).json({ error: `已达到最大流数量限制 (${MAX_STREAMS})` });
      }
    }
    manager.pushLine(id, text);
    res.json({ ok: true });
  } catch (err) {
    Logger.error(`POST /api/streams/${req.params.id}/push 异常: ${err.message}`);
    res.status(500).json({ error: '服务器内部错误' });
  }
});

// ========== 全局错误处理中间件 ==========

/** 捕获未匹配的路由（404） */
app.use((req, res) => {
  Logger.warn(`404 Not Found: ${req.method} ${req.originalUrl}`);
  res.status(404).json({ error: '接口不存在', path: req.originalUrl });
});

/** 全局错误处理中间件 */
app.use((err, req, res, _next) => {
  Logger.error(`未捕获异常: ${err.message}\n${err.stack}`);
  res.status(500).json({ error: '服务器内部错误' });
});

// ========== WebSocket 连接管理 ==========

/**
 * 推荐的前端 WebSocket 重连策略（指数退避 + 抖动）：
 *
 * 前端在 WebSocket 连接断开时，应采用指数退避算法进行重连，避免服务端雪崩：
 *
 *   1. 初始延迟：1 秒
 *   2. 每次重连失败后，延迟时间乘以 2（上限 30 秒）
 *   3. 在延迟时间上叠加随机抖动（±20%），防止多客户端同时重连
 *   4. 达到最大重试次数（建议 10 次）后停止重连，提示用户手动刷新
 *
 * 示例实现（前端伪代码）：
 *   let retryDelay = 1000;
 *   const maxDelay = 30000;
 *   const maxRetries = 10;
 *   let retries = 0;
 *
 *   function reconnect() {
 *     if (retries >= maxRetries) return;
 *     const jitter = retryDelay * 0.2 * (Math.random() * 2 - 1);
 *     setTimeout(() => {
 *       retries++;
 *       ws = new WebSocket(url);
 *       ws.onopen = () => { retryDelay = 1000; retries = 0; };
 *       ws.onclose = reconnect;
 *     }, retryDelay + jitter);
 *     retryDelay = Math.min(retryDelay * 2, maxDelay);
 *   }
 */

wss.on('connection', (ws, req) => {
  const clientIp = req.socket.remoteAddress || 'unknown';

  // 【安全措施】WebSocket API 密钥认证
  // 当配置了 API_KEY 时，WebSocket 连接也需要通过认证。
  // 支持两种认证方式：
  //   1. 连接 URL 查询参数: ws://host:port/?api_key=<API_KEY>
  //   2. 首条消息中携带 { type: 'auth', api_key: '<API_KEY>' }
  // 未认证的连接会被标记，仅允许发送 auth 消息，其他操作将被拒绝。
  if (API_KEY) {
    const queryToken = new URL(req.url, `http://${req.headers.host}`).searchParams.get('api_key') || '';

    if (queryToken && queryToken.length === API_KEY.length &&
        crypto.timingSafeEqual(Buffer.from(queryToken), Buffer.from(API_KEY))) {
      ws._authenticated = true;
      Logger.info(`WebSocket 客户端已通过 URL 参数认证 (${clientIp})`);
    } else {
      // 未通过 URL 参数认证，等待首条 auth 消息
      ws._authenticated = false;
      Logger.info(`WebSocket 客户端等待认证 (${clientIp})`);
    }
  } else {
    // 未配置 API_KEY，自动通过认证
    ws._authenticated = true;
  }

  Logger.info(`WebSocket 客户端连接 (${clientIp})，当前连接数: ${wss.clients.size}`);

  // 初始化客户端状态
  ws._subscriptions = new Set();
  ws._isAlive = true;    // 心跳存活标记
  ws._connectedAt = Date.now();

  // --- 心跳监听：客户端回复 pong 时标记存活 ---
  ws.on('pong', () => {
    ws._isAlive = true;
  });

  // --- 消息处理 ---
  ws.on('message', (data) => {
    try {
      const msg = JSON.parse(data.toString());

      // 【安全措施】WebSocket 认证消息处理
      // 允许未认证的客户端发送 auth 消息来完成认证
      if (msg.type === 'auth') {
        if (!API_KEY) {
          ws._authenticated = true;
          ws.send(JSON.stringify({ type: 'auth_result', success: true, message: '未配置 API_KEY，自动通过认证' }));
          return;
        }
        const token = msg.api_key || '';
        if (token.length === API_KEY.length &&
            crypto.timingSafeEqual(Buffer.from(token), Buffer.from(API_KEY))) {
          ws._authenticated = true;
          Logger.info(`WebSocket 客户端通过消息认证成功 (${clientIp})`);
          ws.send(JSON.stringify({ type: 'auth_result', success: true }));
        } else {
          Logger.warn(`WebSocket 客户端认证失败: API 密钥无效 (${clientIp})`);
          ws.send(JSON.stringify({ type: 'auth_result', success: false, error: 'API 密钥无效' }));
        }
        return;
      }

      // 【安全措施】未认证的客户端不允许执行其他操作
      if (!ws._authenticated) {
        ws.send(JSON.stringify({ type: 'auth_required', error: '请先通过 auth 消息进行认证' }));
        return;
      }

      switch (msg.type) {
        case 'subscribe':
          // 客户端订阅指定流
          ws._subscriptions = new Set(msg.streams || []);
          Logger.debug(`客户端订阅流: ${[...ws._subscriptions].join(', ') || '全部'}`);
          break;

        case 'push':
          // WebSocket 直推数据到指定流
          if (msg.streamId && msg.text) {
            // 文本长度限制
            const safeText = typeof msg.text === 'string' ? msg.text.substring(0, 10000) : String(msg.text);
            const stream = manager.getOrCreate(String(msg.streamId), msg.streamName || '');
            if (stream) {
              manager.pushLine(stream.id, safeText);
            } else {
              Logger.warn(`WebSocket push 失败：已达到最大流数量限制`);
            }
          }
          break;

        case 'get_history':
          // 客户端请求历史记录
          if (msg.streamId) {
            const stream = manager.streams.get(msg.streamId);
            if (stream) {
              const lines = sanitizeInt(msg.lines, 200);
              ws.send(JSON.stringify({
                type: 'history',
                streamId: msg.streamId,
                lines: stream.buffer.slice(-lines),
              }));
            }
          }
          break;

        default:
          Logger.debug(`未知 WebSocket 消息类型: ${msg.type}`);
      }
    } catch (e) {
      Logger.warn(`WebSocket 消息解析失败: ${e.message}`);
    }
  });

  // --- 连接关闭处理 ---
  ws.on('close', (code, reason) => {
    Logger.info(`WebSocket 客户端断开 (code=${code})，剩余连接数: ${wss.clients.size - 1}`);
  });

  // --- 连接错误处理 ---
  ws.on('error', (err) => {
    Logger.error(`WebSocket 客户端错误: ${err.message}`);
  });

  // 向新连接客户端发送当前所有流的列表
  try {
    ws.send(JSON.stringify({
      type: 'streams_list',
      streams: manager.listStreams(),
    }));
  } catch (e) {
    Logger.error(`发送流列表失败: ${e.message}`);
  }
});

// ========== WebSocket 心跳检测 ==========

/**
 * 定期对所有 WebSocket 连接执行心跳检测
 * 发送 ping 帧，如果客户端在超时时间内未回复 pong，则终止连接
 */
const heartbeatTimer = setInterval(() => {
  wss.clients.forEach(ws => {
    if (ws._isAlive === false) {
      Logger.warn(`WebSocket 心跳超时，终止连接 (已连接 ${Date.now() - ws._connectedAt}ms)`);
      return ws.terminate();
    }
    ws._isAlive = false;
    ws.ping();
  });
}, WS_HEARTBEAT_INTERVAL);

// 当 WebSocket 服务器关闭时，清理心跳定时器
wss.on('close', () => {
  clearInterval(heartbeatTimer);
});

// ========== 进程优雅退出 ==========

/**
 * 优雅退出处理函数
 * 依次：停止 Demo → 杀死子进程 → 关闭 WebSocket → 关闭 HTTP 服务器
 */
function gracefulShutdown(signal) {
  Logger.info(`收到 ${signal} 信号，开始优雅退出...`);

  // 停止所有 Demo 定时器
  manager.stopDemo();

  // 杀死所有正在运行的子进程
  manager.streams.forEach(s => {
    if (s.process) {
      try {
        s.process.kill('SIGTERM');
      } catch (e) {
        Logger.warn(`终止进程失败 (${s.name}): ${e.message}`);
      }
    }
  });

  // 通知所有 WebSocket 客户端并关闭连接
  wss.clients.forEach(client => {
    client.close(1001, '服务器正在关闭');
  });

  // 关闭 WebSocket 服务器
  wss.close(() => {
    // 关闭 HTTP 服务器
    server.close(() => {
      Logger.info('服务器已关闭');
      process.exit(0);
    });
  });

  // 强制退出超时：10 秒后仍未退出则强制结束
  setTimeout(() => {
    Logger.error('优雅退出超时，强制结束进程');
    process.exit(1);
  }, SHUTDOWN_TIMEOUT);
}

// 注册进程信号监听
process.on('SIGTERM', () => gracefulShutdown('SIGTERM'));
process.on('SIGINT', () => gracefulShutdown('SIGINT'));

// 捕获未处理的 Promise 拒绝
process.on('unhandledRejection', (reason, promise) => {
  Logger.error(`未处理的 Promise 拒绝: ${reason}`);
});

// 捕获未捕获的异常（记录后退出）
process.on('uncaughtException', (err) => {
  Logger.error(`未捕获的异常: ${err.message}\n${err.stack}`);
  process.exit(1);
});

// ========== 启动服务器 ==========

server.listen(PORT, () => {
  Logger.info('');
  Logger.info('  ================================================');
  Logger.info(`   多路并发输出实时监控仪表盘`);
  Logger.info(`   地址: http://localhost:${PORT}`);
  Logger.info('  ================================================');
  Logger.info('');

  // 【安全措施】启动时输出安全配置状态
  if (API_KEY) {
    Logger.info(`  [安全] API 密钥认证: 已启用`);
  } else {
    Logger.warn('  [安全] API 密钥认证: 未启用 (API_KEY 环境变量未设置)');
    Logger.warn('  [安全] 警告: 所有 API 端点无需认证即可访问，请勿在生产环境中使用！');
  }

  if (CORS_ORIGINS.length > 0) {
    Logger.info(`  [安全] CORS 允许来源: ${CORS_ORIGINS.join(', ')}`);
  } else {
    Logger.info('  [安全] CORS 允许来源: 未配置 (仅允许同源请求)');
  }

  Logger.info(`  [安全] 命令白名单: 已启用 (${COMMAND_WHITELIST.length} 条规则)`);
  Logger.info(`  [安全] 命令黑名单: 已启用 (${COMMAND_BLACKLIST.length} 条规则)`);
  Logger.info(`  [安全] shell 注入防护: 已启用 (shell: false)`);
  Logger.info('');

  // 自动启动 Demo
  manager.startDemo();
  Logger.info(`Demo 已自动启动，浏览器访问 http://localhost:${PORT}`);
});
