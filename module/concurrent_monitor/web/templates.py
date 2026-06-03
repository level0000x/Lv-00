"""
HTML 模板
==========

Web 仪表盘的 HTML/CSS/JS 模板。
"""

DASHBOARD_HTML = r"""
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>并发输出实时监控</title>
<style>
  /* ===== CSS 变量（暗色主题） ===== */
  :root {
    --bg: #0d1117;
    --surface: #161b22;
    --border: #30363d;
    --text: #c9d1d9;
    --text-dim: #8b949e;
    --accent: #58a6ff;
    --green: #3fb950;
    --red: #f85149;
    --yellow: #d29922;
    --magenta: #bc8cff;
    --cyan: #39c5cf;
  }

  * {
    margin: 0;
    padding: 0;
    box-sizing: border-box;
  }

  body {
    font-family: 'Segoe UI', 'Microsoft YaHei', 'Consolas', monospace;
    background: var(--bg);
    color: var(--text);
    height: 100vh;
    display: flex;
    flex-direction: column;
    overflow: hidden;
  }

  /* ===== 顶部状态栏 ===== */
  .header {
    background: var(--surface);
    border-bottom: 1px solid var(--border);
    padding: 12px 20px;
    display: flex;
    align-items: center;
    gap: 24px;
    flex-shrink: 0;
  }
  .header h1 {
    font-size: 18px;
    color: var(--accent);
    display: flex;
    align-items: center;
    gap: 8px;
  }
  .header h1::before {
    content: "\25C9";
    animation: pulse 2s infinite;
  }
  .status-badges {
    display: flex;
    gap: 12px;
    margin-left: auto;
  }
  .badge {
    padding: 4px 12px;
    border-radius: 12px;
    font-size: 13px;
    font-weight: 600;
    background: var(--surface);
    border: 1px solid var(--border);
    transition: all 0.2s;
  }
  .badge.running { border-color: var(--yellow); color: var(--yellow); }
  .badge.completed { border-color: var(--green); color: var(--green); }
  .badge.failed { border-color: var(--red); color: var(--red); }
  .badge.total { border-color: var(--accent); color: var(--accent); }
  .badge.timeout { border-color: var(--magenta); color: var(--magenta); }
  .badge.pending { border-color: var(--text-dim); color: var(--text-dim); }

  /* ===== 主体布局 ===== */
  .main {
    display: flex;
    flex: 1;
    overflow: hidden;
  }

  /* ===== 左侧面板 ===== */
  .left-panel {
    width: 380px;
    flex-shrink: 0;
    background: var(--surface);
    border-right: 1px solid var(--border);
    overflow-y: auto;
    display: flex;
    flex-direction: column;
  }

  /* 搜索框容器 */
  .search-box {
    padding: 10px 12px;
    border-bottom: 1px solid var(--border);
    flex-shrink: 0;
  }
  .search-box input {
    width: 100%;
    padding: 8px 12px;
    background: var(--bg);
    border: 1px solid var(--border);
    border-radius: 6px;
    color: var(--text);
    font-size: 13px;
    font-family: 'Consolas', monospace;
    transition: border-color 0.2s;
  }
  .search-box input:focus {
    outline: none;
    border-color: var(--accent);
  }
  .search-box input::placeholder {
    color: var(--text-dim);
  }

  /* 进程列表容器 */
  .proc-list-container {
    flex: 1;
    overflow-y: auto;
    padding: 12px;
  }

  .right-panel {
    flex: 1;
    overflow-y: auto;
    padding: 12px;
    display: flex;
    flex-direction: column;
    gap: 12px;
  }

  /* ===== 进程卡片 ===== */
  .proc-card {
    background: var(--bg);
    border: 1px solid var(--border);
    border-radius: 8px;
    margin-bottom: 10px;
    padding: 12px 14px;
    cursor: pointer;
    transition: all 0.2s;
    position: relative;
  }
  .proc-card:hover {
    border-color: var(--accent);
    transform: translateX(2px);
  }
  .proc-card.active {
    border-color: var(--accent);
    box-shadow: 0 0 0 1px var(--accent), 0 0 12px rgba(88,166,255,0.15);
  }
  .proc-card .name {
    font-weight: 600;
    font-size: 14px;
    display: flex;
    align-items: center;
    gap: 8px;
  }
  .proc-card .cmd {
    color: var(--text-dim);
    font-size: 12px;
    margin-top: 6px;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
    font-family: 'Consolas', monospace;
  }
  .proc-card .meta {
    display: flex;
    gap: 16px;
    margin-top: 8px;
    font-size: 11px;
    color: var(--text-dim);
  }
  .proc-card .meta span {
    display: flex;
    align-items: center;
    gap: 4px;
  }
  .status-dot {
    display: inline-block;
    width: 8px;
    height: 8px;
    border-radius: 50%;
  }
  .status-dot.running { background: var(--yellow); animation: pulse 1s infinite; }
  .status-dot.completed { background: var(--green); }
  .status-dot.failed { background: var(--red); }
  .status-dot.pending { background: var(--text-dim); }
  .status-dot.timeout { background: var(--magenta); }

  @keyframes pulse {
    0%, 100% { opacity: 1; }
    50% { opacity: 0.4; }
  }

  /* ===== 输出面板 ===== */
  .output-panel {
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: 8px;
    overflow: hidden;
    flex: 1;
    min-height: 200px;
    display: flex;
    flex-direction: column;
  }
  .output-panel .panel-header {
    padding: 10px 16px;
    border-bottom: 1px solid var(--border);
    display: flex;
    align-items: center;
    gap: 10px;
    font-size: 13px;
    font-weight: 600;
    background: rgba(255,255,255,0.02);
    flex-wrap: wrap;
  }
  .output-panel .panel-body {
    flex: 1;
    overflow-y: auto;
    padding: 12px 16px;
    font-family: 'Consolas', 'Courier New', monospace;
    font-size: 12px;
    line-height: 1.6;
  }
  .output-panel .line {
    white-space: pre-wrap;
    word-break: break-all;
    padding: 2px 0;
    display: flex;
  }
  .output-panel .line.stderr { color: var(--red); }
  .output-panel .line.system { color: var(--text-dim); font-style: italic; }
  .output-panel .line:hover {
    background: rgba(255,255,255,0.03);
  }

  /* 行号样式 */
  .line-number {
    color: var(--text-dim);
    min-width: 40px;
    text-align: right;
    padding-right: 12px;
    user-select: none;
    flex-shrink: 0;
    opacity: 0.6;
    font-size: 11px;
  }
  .line-content {
    flex: 1;
    min-width: 0;
  }

  /* 面板头部工具按钮 */
  .panel-tools {
    display: flex;
    gap: 6px;
    margin-left: auto;
    align-items: center;
  }
  .panel-tools .tool-btn {
    padding: 3px 8px;
    background: transparent;
    border: 1px solid var(--border);
    border-radius: 4px;
    color: var(--text-dim);
    cursor: pointer;
    font-size: 11px;
    transition: all 0.2s;
    display: flex;
    align-items: center;
    gap: 4px;
    white-space: nowrap;
  }
  .panel-tools .tool-btn:hover {
    border-color: var(--accent);
    color: var(--accent);
  }
  .panel-tools .tool-btn.active {
    border-color: var(--accent);
    color: var(--accent);
    background: rgba(88,166,255,0.1);
  }

  /* ===== 控制区 ===== */
  .controls {
    padding: 12px 20px;
    background: var(--surface);
    border-top: 1px solid var(--border);
    display: flex;
    gap: 10px;
    align-items: center;
    flex-shrink: 0;
  }
  .controls input {
    flex: 1;
    padding: 10px 14px;
    background: var(--bg);
    border: 1px solid var(--border);
    border-radius: 6px;
    color: var(--text);
    font-size: 13px;
    font-family: 'Consolas', monospace;
    transition: border-color 0.2s;
  }
  .controls input:focus {
    outline: none;
    border-color: var(--accent);
  }
  .controls input::placeholder {
    color: var(--text-dim);
  }
  .btn {
    padding: 10px 18px;
    border: 1px solid var(--border);
    border-radius: 6px;
    cursor: pointer;
    font-size: 13px;
    font-weight: 600;
    transition: all 0.2s;
    white-space: nowrap;
    display: flex;
    align-items: center;
    gap: 6px;
  }
  .btn-primary {
    background: var(--accent);
    color: #fff;
    border-color: var(--accent);
  }
  .btn-primary:hover {
    background: #4090e0;
    transform: translateY(-1px);
  }
  .btn-danger {
    background: transparent;
    color: var(--red);
    border-color: var(--red);
  }
  .btn-danger:hover {
    background: rgba(248,81,73,0.1);
  }
  .btn-default {
    background: transparent;
    color: var(--text);
  }
  .btn-default:hover {
    background: rgba(255,255,255,0.05);
  }
  .btn:disabled {
    opacity: 0.5;
    cursor: not-allowed;
    transform: none !important;
  }

  /* 快捷键帮助按钮 */
  .btn-help {
    padding: 8px 10px;
    background: transparent;
    border: 1px solid var(--border);
    border-radius: 6px;
    cursor: pointer;
    font-size: 16px;
    color: var(--text-dim);
    transition: all 0.2s;
    display: flex;
    align-items: center;
    justify-content: center;
    min-width: 34px;
  }
  .btn-help:hover {
    border-color: var(--accent);
    color: var(--accent);
  }

  /* ===== 空状态 ===== */
  .empty-state {
    display: flex;
    align-items: center;
    justify-content: center;
    height: 100%;
    color: var(--text-dim);
    flex-direction: column;
    gap: 12px;
  }
  .empty-state-icon {
    font-size: 48px;
    opacity: 0.3;
  }

  /* ===== 连接状态指示器 ===== */
  .connection-status {
    display: flex;
    align-items: center;
    gap: 6px;
    font-size: 12px;
    color: var(--text-dim);
  }
  .connection-status.connected { color: var(--green); }
  .connection-status.disconnected { color: var(--red); }
  .connection-dot {
    width: 8px;
    height: 8px;
    border-radius: 50%;
    background: currentColor;
  }

  /* ===== Toast 通知系统 ===== */
  .toast-container {
    position: fixed;
    top: 16px;
    right: 16px;
    z-index: 10000;
    display: flex;
    flex-direction: column;
    gap: 8px;
    pointer-events: none;
  }
  .toast {
    padding: 12px 20px;
    border-radius: 8px;
    font-size: 13px;
    font-weight: 500;
    color: #fff;
    pointer-events: auto;
    animation: toastIn 0.3s ease-out;
    box-shadow: 0 4px 12px rgba(0,0,0,0.4);
    max-width: 400px;
    word-break: break-word;
    border: 1px solid rgba(255,255,255,0.1);
    display: flex;
    align-items: center;
    gap: 8px;
  }
  .toast.success {
    background: rgba(63,185,80,0.9);
    border-color: var(--green);
  }
  .toast.error {
    background: rgba(248,81,73,0.9);
    border-color: var(--red);
  }
  .toast.info {
    background: rgba(88,166,255,0.9);
    border-color: var(--accent);
  }
  .toast.fade-out {
    animation: toastOut 0.3s ease-in forwards;
  }
  @keyframes toastIn {
    from { opacity: 0; transform: translateX(60px); }
    to { opacity: 1; transform: translateX(0); }
  }
  @keyframes toastOut {
    from { opacity: 1; transform: translateX(0); }
    to { opacity: 0; transform: translateX(60px); }
  }

  /* ===== 弹窗遮罩 ===== */
  .modal-overlay {
    position: fixed;
    top: 0;
    left: 0;
    right: 0;
    bottom: 0;
    background: rgba(0,0,0,0.6);
    z-index: 9000;
    display: flex;
    align-items: center;
    justify-content: center;
    animation: fadeIn 0.2s ease-out;
  }
  .modal-overlay.fade-out {
    animation: fadeOut 0.2s ease-in forwards;
  }
  @keyframes fadeIn {
    from { opacity: 0; }
    to { opacity: 1; }
  }
  @keyframes fadeOut {
    from { opacity: 1; }
    to { opacity: 0; }
  }

  /* ===== 弹窗内容 ===== */
  .modal {
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: 12px;
    padding: 0;
    min-width: 420px;
    max-width: 600px;
    max-height: 80vh;
    overflow-y: auto;
    box-shadow: 0 8px 32px rgba(0,0,0,0.5);
    animation: modalIn 0.25s ease-out;
  }
  .modal.fade-out {
    animation: modalOut 0.2s ease-in forwards;
  }
  @keyframes modalIn {
    from { opacity: 0; transform: scale(0.95) translateY(-10px); }
    to { opacity: 1; transform: scale(1) translateY(0); }
  }
  @keyframes modalOut {
    from { opacity: 1; transform: scale(1) translateY(0); }
    to { opacity: 0; transform: scale(0.95) translateY(-10px); }
  }
  .modal-header {
    padding: 16px 20px;
    border-bottom: 1px solid var(--border);
    display: flex;
    align-items: center;
    justify-content: space-between;
    font-size: 15px;
    font-weight: 600;
    color: var(--accent);
  }
  .modal-close {
    background: transparent;
    border: none;
    color: var(--text-dim);
    font-size: 20px;
    cursor: pointer;
    padding: 4px 8px;
    border-radius: 4px;
    transition: all 0.2s;
    line-height: 1;
  }
  .modal-close:hover {
    color: var(--text);
    background: rgba(255,255,255,0.05);
  }
  .modal-body {
    padding: 20px;
  }

  /* 进程详情弹窗 */
  .detail-grid {
    display: grid;
    grid-template-columns: 100px 1fr;
    gap: 12px 16px;
    font-size: 13px;
  }
  .detail-label {
    color: var(--text-dim);
    font-weight: 500;
    display: flex;
    align-items: center;
    gap: 6px;
  }
  .detail-value {
    color: var(--text);
    word-break: break-all;
    font-family: 'Consolas', monospace;
    font-size: 12px;
  }
  .detail-value.cmd-text {
    background: var(--bg);
    padding: 8px 12px;
    border-radius: 6px;
    border: 1px solid var(--border);
    line-height: 1.5;
  }

  /* 快捷键帮助弹窗 */
  .shortcut-list {
    display: flex;
    flex-direction: column;
    gap: 10px;
  }
  .shortcut-item {
    display: flex;
    align-items: center;
    justify-content: space-between;
    font-size: 13px;
  }
  .shortcut-item .desc {
    color: var(--text-dim);
  }
  .shortcut-keys {
    display: flex;
    gap: 4px;
  }
  .kbd {
    padding: 3px 8px;
    background: var(--bg);
    border: 1px solid var(--border);
    border-radius: 4px;
    font-size: 11px;
    font-family: 'Consolas', monospace;
    color: var(--text);
    box-shadow: 0 1px 0 var(--border);
  }

  /* ===== 滚动条 ===== */
  ::-webkit-scrollbar { width: 8px; height: 8px; }
  ::-webkit-scrollbar-track { background: transparent; }
  ::-webkit-scrollbar-thumb {
    background: var(--border);
    border-radius: 4px;
    border: 2px solid transparent;
    background-clip: padding-box;
  }
  ::-webkit-scrollbar-thumb:hover { background: var(--text-dim); }

  /* ===== 响应式 ===== */
  @media (max-width: 768px) {
    .left-panel { width: 100%; }
    .main { flex-direction: column; }
    .controls { flex-wrap: wrap; }
    .controls input { width: 100%; margin-bottom: 8px; }
    .modal { min-width: auto; margin: 16px; max-width: calc(100vw - 32px); }
  }
</style>
</head>
<body>

<!-- ===== 顶部状态栏 ===== -->
<div class="header">
  <h1>并发输出实时监控</h1>
  <div class="connection-status" id="connectionStatus">
    <span class="connection-dot"></span>
    <span>连接中...</span>
  </div>
  <div class="status-badges" id="statusBadges">
    <span class="badge total">总计: 0</span>
    <span class="badge running" id="badgeRunning">运行: 0</span>
    <span class="badge completed" id="badgeCompleted">完成: 0</span>
    <span class="badge failed" id="badgeFailed">失败: 0</span>
  </div>
</div>

<!-- ===== 主体区域 ===== -->
<div class="main">
  <!-- 左侧面板：搜索框 + 进程列表 -->
  <div class="left-panel">
    <!-- 搜索/过滤框 -->
    <div class="search-box">
      <input type="text" id="searchInput" placeholder="搜索进程 (ID 或命令)..." />
    </div>
    <!-- 进程列表 -->
    <div class="proc-list-container" id="procList">
      <div class="empty-state">
        <div class="empty-state-icon">📋</div>
        <div>暂无进程</div>
        <div style="font-size: 12px;">使用下方控制栏添加进程</div>
      </div>
    </div>
  </div>
  <!-- 右侧面板：输出区域 -->
  <div class="right-panel" id="outputArea">
    <div class="empty-state">
      <div class="empty-state-icon">👈</div>
      <div>点击左侧进程查看实时输出</div>
    </div>
  </div>
</div>

<!-- ===== 底部控制栏 ===== -->
<div class="controls">
  <input type="text" id="cmdInput" placeholder="输入命令... (例: ping -n 5 127.0.0.1)" />
  <button class="btn btn-primary" onclick="addProcess()">
    <span>+</span> 添加进程
  </button>
  <button class="btn btn-primary" onclick="startAll()">
    <span>▶</span> 启动全部
  </button>
  <button class="btn btn-danger" onclick="stopAll()">
    <span>■</span> 停止全部
  </button>
  <button class="btn btn-default" onclick="loadDemo()">演示</button>
  <button class="btn btn-default" onclick="clearAll()">清空</button>
  <!-- 快捷键帮助按钮 -->
  <button class="btn-help" onclick="showShortcutHelp()" title="快捷键帮助">?</button>
</div>

<!-- ===== Toast 通知容器 ===== -->
<div class="toast-container" id="toastContainer"></div>

<script>
// ===================================================================
// ===== 状态管理 =====
// ===================================================================
const state = {
  processes: {},          // 所有进程信息 { pid: { id, command, status, ... } }
  selectedPid: null,      // 当前选中的进程 ID
  outputBuffers: {},      // 输出缓冲区 { pid: [msg, ...] }
  maxLines: 500,          // 每个进程最大缓存行数
  connectionStatus: 'connecting',  // SSE 连接状态
  autoScroll: true,       // 自动滚动开关（默认开启）
  showLineNumbers: false, // 行号显示开关（默认关闭）
  searchQuery: ''         // 搜索过滤关键词
};

// ===================================================================
// ===== Toast 通知系统 =====
// ===================================================================

/**
 * 显示 Toast 通知
 * @param {string} message - 通知内容
 * @param {string} type - 通知类型: 'success' | 'error' | 'info'
 * @param {number} duration - 显示时长（毫秒），默认 3000
 */
const showToast = (message, type = 'info', duration = 3000) => {
  const container = document.getElementById('toastContainer');
  const toast = document.createElement('div');
  toast.className = `toast ${type}`;

  // 根据类型选择图标
  const icons = { success: '\u2714', error: '\u2718', info: '\u2139' };
  toast.innerHTML = `<span>${icons[type] || ''}</span><span>${escapeHtml(message)}</span>`;

  container.appendChild(toast);

  // 指定时间后自动消失
  setTimeout(() => {
    toast.classList.add('fade-out');
    // 动画结束后移除 DOM 元素
    toast.addEventListener('animationend', () => toast.remove());
  }, duration);
};

// ===================================================================
// ===== 弹窗系统 =====
// ===================================================================

/**
 * 显示弹窗
 * @param {string} title - 弹窗标题
 * @param {string} bodyHtml - 弹窗内容 HTML
 */
const showModal = (title, bodyHtml) => {
  // 移除已有弹窗
  closeModal();

  const overlay = document.createElement('div');
  overlay.className = 'modal-overlay';
  overlay.id = 'modalOverlay';
  // 点击遮罩关闭弹窗
  overlay.addEventListener('click', (e) => {
    if (e.target === overlay) closeModal();
  });

  const modal = document.createElement('div');
  modal.className = 'modal';
  modal.id = 'modalContent';
  modal.innerHTML = `
    <div class="modal-header">
      <span>${escapeHtml(title)}</span>
      <button class="modal-close" onclick="closeModal()">&times;</button>
    </div>
    <div class="modal-body">${bodyHtml}</div>
  `;

  overlay.appendChild(modal);
  document.body.appendChild(overlay);
};

/**
 * 关闭弹窗（带淡出动画）
 */
const closeModal = () => {
  const overlay = document.getElementById('modalOverlay');
  const modal = document.getElementById('modalContent');
  if (overlay && modal) {
    overlay.classList.add('fade-out');
    modal.classList.add('fade-out');
    // 动画结束后移除 DOM
    setTimeout(() => overlay.remove(), 200);
  }
};

// ===================================================================
// ===== 快捷键帮助弹窗 =====
// ===================================================================

/**
 * 显示快捷键帮助弹窗
 */
const showShortcutHelp = () => {
  const shortcuts = [
    { keys: ['Enter'], desc: '添加进程（输入框聚焦时）' },
    { keys: ['Ctrl', 'Shift', 'S'], desc: '启动全部进程' },
    { keys: ['Ctrl', 'Shift', 'X'], desc: '停止全部进程' },
    { keys: ['Esc'], desc: '取消选中进程 / 关闭弹窗' },
  ];

  const bodyHtml = `
    <div class="shortcut-list">
      ${shortcuts.map(s => `
        <div class="shortcut-item">
          <span class="desc">${s.desc}</span>
          <div class="shortcut-keys">
            ${s.keys.map(k => `<span class="kbd">${k}</span>`).join('<span style="color:var(--text-dim);">+</span>')}
          </div>
        </div>
      `).join('')}
    </div>
  `;

  showModal('快捷键帮助', bodyHtml);
};

// ===================================================================
// ===== 进程详情弹窗 =====
// ===================================================================

/**
 * 显示进程详情弹窗
 * @param {string} pid - 进程 ID
 */
const showProcessDetail = (pid) => {
  const p = state.processes[pid];
  if (!p) {
    showToast('进程不存在', 'error');
    return;
  }

  const buf = state.outputBuffers[pid] || [];
  // 统计 stderr 行数
  const stderrCount = buf.filter(l => l.stream === 'stderr').length;

  // 状态颜色映射
  const statusColors = {
    running: 'var(--yellow)',
    completed: 'var(--green)',
    failed: 'var(--red)',
    pending: 'var(--text-dim)',
    timeout: 'var(--magenta)'
  };

  const bodyHtml = `
    <div class="detail-grid">
      <div class="detail-label">进程 ID</div>
      <div class="detail-value">${escapeHtml(p.id)}</div>

      <div class="detail-label">状态</div>
      <div class="detail-value" style="color:${statusColors[p.status] || 'var(--text)'};font-weight:600;">
        <span class="status-dot ${p.status}" style="margin-right:6px;"></span>${escapeHtml(p.status)}
      </div>

      <div class="detail-label">命令</div>
      <div class="detail-value cmd-text">${escapeHtml(p.command)}</div>

      <div class="detail-label">开始时间</div>
      <div class="detail-value">${escapeHtml(p.start_time || '-')}</div>

      <div class="detail-label">结束时间</div>
      <div class="detail-value">${escapeHtml(p.end_time || '-')}</div>

      <div class="detail-label">退出码</div>
      <div class="detail-value">${p.exit_code !== null && p.exit_code !== undefined ? p.exit_code : '-'}</div>

      <div class="detail-label">输出行数</div>
      <div class="detail-value">${p.lines || 0}</div>

      <div class="detail-label">错误行数</div>
      <div class="detail-value" style="color:${stderrCount > 0 ? 'var(--red)' : 'var(--text)'};">${stderrCount}</div>

      <div class="detail-label">耗时</div>
      <div class="detail-value">${escapeHtml(p.duration || '-')}</div>
    </div>
  `;

  showModal(`进程详情 - ${pid}`, bodyHtml);
};

// ===================================================================
// ===== 键盘快捷键 =====
// ===================================================================

/**
 * 全局键盘事件监听
 * - Enter（输入框聚焦时）：添加进程
 * - Ctrl+Shift+S：启动全部
 * - Ctrl+Shift+X：停止全部
 * - Escape：取消选中进程 / 关闭弹窗
 */
document.addEventListener('keydown', (e) => {
  // Escape：关闭弹窗或取消选中
  if (e.key === 'Escape') {
    // 如果有弹窗打开，关闭弹窗
    if (document.getElementById('modalOverlay')) {
      closeModal();
      return;
    }
    // 否则取消选中进程
    if (state.selectedPid) {
      state.selectedPid = null;
      renderProcList();
      document.getElementById('outputArea').innerHTML = `
        <div class="empty-state">
          <div class="empty-state-icon">👈</div>
          <div>点击左侧进程查看实时输出</div>
        </div>`;
    }
    return;
  }

  // Ctrl+Shift+S：启动全部
  if (e.ctrlKey && e.shiftKey && e.key === 'S') {
    e.preventDefault();
    startAll();
    return;
  }

  // Ctrl+Shift+X：停止全部
  if (e.ctrlKey && e.shiftKey && e.key === 'X') {
    e.preventDefault();
    stopAll();
    return;
  }

  // Enter（输入框聚焦时）：添加进程
  if (e.key === 'Enter' && document.activeElement === document.getElementById('cmdInput')) {
    e.preventDefault();
    addProcess();
    return;
  }
});

// ===================================================================
// ===== SSE 连接 =====
// ===================================================================

let es = null;                      // EventSource 实例
let reconnectAttempts = 0;          // 重连计数
const maxReconnectAttempts = 5;     // 最大重连次数

/**
 * 建立 SSE（Server-Sent Events）连接
 * 用于接收服务器端的实时输出推送
 */
const connectSSE = () => {
  if (es) {
    es.close();
  }

  es = new EventSource('/stream');

  // 连接成功回调
  es.onopen = () => {
    console.log('SSE 连接已建立');
    state.connectionStatus = 'connected';
    reconnectAttempts = 0;
    updateConnectionStatus();
  };

  // 接收消息回调
  es.onmessage = (e) => {
    try {
      const msg = JSON.parse(e.data);
      if (msg.type === 'output') {
        handleOutput(msg.data);
      } else if (msg.type === 'summary') {
        handleSummary(msg.data);
      } else if (msg.type === 'error') {
        console.error('SSE 错误:', msg.message);
        showToast(msg.message || '服务器推送错误', 'error');
      }
    } catch (err) {
      console.error('解析 SSE 消息失败:', err);
    }
  };

  // 连接错误回调（自动重连）
  es.onerror = (e) => {
    console.error('SSE 连接错误');
    state.connectionStatus = 'disconnected';
    updateConnectionStatus();

    if (reconnectAttempts < maxReconnectAttempts) {
      reconnectAttempts++;
      setTimeout(connectSSE, 3000 * reconnectAttempts);
    }
  };
};

/**
 * 更新连接状态指示器 UI
 */
const updateConnectionStatus = () => {
  const el = document.getElementById('connectionStatus');
  const dot = el.querySelector('.connection-dot');
  const text = el.querySelector('span:last-child');

  el.className = 'connection-status ' + state.connectionStatus;

  if (state.connectionStatus === 'connected') {
    dot.style.background = 'var(--green)';
    text.textContent = '已连接';
  } else if (state.connectionStatus === 'disconnected') {
    dot.style.background = 'var(--red)';
    text.textContent = '已断开';
  } else {
    dot.style.background = 'var(--yellow)';
    text.textContent = '连接中...';
  }
};

// ===================================================================
// ===== SSE 消息处理 =====
// ===================================================================

/**
 * 处理输出消息
 * @param {Object} msg - 输出消息 { process_id, content, stream, ... }
 */
const handleOutput = (msg) => {
  // 初始化该进程的输出缓冲区
  if (!state.outputBuffers[msg.process_id]) {
    state.outputBuffers[msg.process_id] = [];
  }
  const buf = state.outputBuffers[msg.process_id];
  buf.push(msg);
  // 超过最大行数时移除最早的行
  if (buf.length > state.maxLines) buf.shift();

  // 如果当前正在查看该进程，追加到输出面板
  if (state.selectedPid === msg.process_id) {
    appendLineToPanel(msg.process_id, msg);
  }
};

/**
 * 处理摘要消息（全量进程状态更新）
 * @param {Object} data - 摘要数据 { processes: [...], status_counts: {...}, total: N }
 */
const handleSummary = (data) => {
  state.processes = {};
  data.processes.forEach(p => {
    state.processes[p.id] = p;
  });
  renderProcList();
  updateBadges(data.status_counts, data.total);

  // 如果选中的进程还在运行，更新输出面板头部信息
  if (state.selectedPid && state.processes[state.selectedPid]) {
    updateSelectedPanel();
  }
};

/**
 * 更新顶部状态徽章
 * @param {Object} counts - 各状态计数 { running, completed, failed, timeout, ... }
 * @param {number} total - 总进程数
 */
const updateBadges = (counts, total) => {
  document.querySelector('.badge.total').textContent = `总计: ${total}`;
  document.getElementById('badgeRunning').textContent = `运行: ${counts.running || 0}`;
  document.getElementById('badgeCompleted').textContent = `完成: ${counts.completed || 0}`;
  document.getElementById('badgeFailed').textContent = `失败: ${(counts.failed || 0) + (counts.timeout || 0)}`;
};

// ===================================================================
// ===== 搜索/过滤功能 =====
// ===================================================================

/**
 * 搜索框输入事件处理（实时过滤进程列表）
 */
document.addEventListener('DOMContentLoaded', () => {
  const searchInput = document.getElementById('searchInput');
  searchInput.addEventListener('input', (e) => {
    state.searchQuery = e.target.value.trim().toLowerCase();
    renderProcList();
  });
});

/**
 * 根据搜索关键词过滤进程列表
 * @returns {string[]} 过滤后的进程 ID 列表
 */
const getFilteredPids = () => {
  const pids = Object.keys(state.processes);
  const query = state.searchQuery;
  if (!query) return pids;

  return pids.filter(pid => {
    const p = state.processes[pid];
    // 按进程 ID 或命令内容匹配
    return pid.toLowerCase().includes(query) || p.command.toLowerCase().includes(query);
  });
};

// ===================================================================
// ===== 渲染进程列表 =====
// ===================================================================

/**
 * 渲染左侧进程列表（支持搜索过滤）
 */
const renderProcList = () => {
  const container = document.getElementById('procList');
  const filteredPids = getFilteredPids();

  if (Object.keys(state.processes).length === 0) {
    container.innerHTML = `
      <div class="empty-state">
        <div class="empty-state-icon">📋</div>
        <div>暂无进程</div>
        <div style="font-size: 12px;">使用下方控制栏添加进程</div>
      </div>`;
    return;
  }

  // 搜索无结果
  if (filteredPids.length === 0) {
    container.innerHTML = `
      <div class="empty-state">
        <div class="empty-state-icon">🔍</div>
        <div>无匹配进程</div>
        <div style="font-size: 12px;">尝试其他搜索关键词</div>
      </div>`;
    return;
  }

  container.innerHTML = filteredPids.map(pid => {
    const p = state.processes[pid];
    const statusClass = p.status;
    const isActive = state.selectedPid === pid;
    return `
      <div class="proc-card ${isActive ? 'active' : ''}"
           onclick="selectProcess('${pid}')"
           ondblclick="showProcessDetail('${pid}')"
           title="单击选中，双击查看详情">
        <div class="name">
          <span class="status-dot ${statusClass}"></span>${escapeHtml(pid)}
        </div>
        <div class="cmd" title="${escapeHtml(p.command)}">${escapeHtml(p.command)}</div>
        <div class="meta">
          <span>📄 ${p.lines}</span>
          <span>${p.errors > 0 ? '🔴' : '⚪'} ${p.errors}</span>
          <span>⏱ ${p.duration}</span>
        </div>
      </div>
    `;
  }).join('');
};

// ===================================================================
// ===== 进程选择与输出渲染 =====
// ===================================================================

/**
 * 选中一个进程并显示其输出
 * @param {string} pid - 进程 ID
 */
const selectProcess = (pid) => {
  state.selectedPid = pid;
  renderProcList();
  renderOutput(pid);
};

/**
 * 更新当前选中进程的输出面板（不重新渲染整个面板）
 */
const updateSelectedPanel = () => {
  if (!state.selectedPid) return;
  renderOutput(state.selectedPid);
};

/**
 * 渲染指定进程的输出面板
 * @param {string} pid - 进程 ID
 */
const renderOutput = (pid) => {
  const area = document.getElementById('outputArea');
  const p = state.processes[pid];

  if (!p) {
    area.innerHTML = `
      <div class="empty-state">
        <div class="empty-state-icon">❓</div>
        <div>进程不存在</div>
      </div>`;
    return;
  }

  const buf = state.outputBuffers[pid] || [];
  const statusClass = p.status;

  area.innerHTML = `
    <div class="output-panel">
      <div class="panel-header">
        <span class="status-dot ${statusClass}"></span>
        <span>${escapeHtml(pid)}</span>
        <span style="color:var(--text-dim);font-weight:400;font-size:12px;margin-left:8px;">${escapeHtml(p.command)}</span>
        <span style="margin-left:auto;font-size:11px;color:var(--text-dim);">
          ${p.status} | exit=${p.exit_code != null ? p.exit_code : '-'} | ${p.duration}
        </span>
      </div>
      <!-- 面板工具栏 -->
      <div style="padding:6px 16px;border-bottom:1px solid var(--border);display:flex;gap:6px;align-items:center;background:rgba(255,255,255,0.01);">
        <div class="panel-tools">
          <button class="tool-btn" onclick="copyAllOutput('${pid}')" title="复制全部输出">
            📋 复制
          </button>
          <button class="tool-btn ${state.autoScroll ? 'active' : ''}" onclick="toggleAutoScroll()" title="自动滚动" id="btnAutoScroll">
            ⬇ 自动滚动
          </button>
          <button class="tool-btn ${state.showLineNumbers ? 'active' : ''}" onclick="toggleLineNumbers()" title="行号显示" id="btnLineNumbers">
            # 行号
          </button>
        </div>
      </div>
      <div class="panel-body" id="panelBody_${pid}">
        ${buf.length > 0 ? buf.map((l, i) => formatLine(l, i)).join('') : '<div style="color:var(--text-dim);padding-top:20px;">等待输出...</div>'}
      </div>
    </div>
  `;

  scrollToBottom(pid);
};

/**
 * 向输出面板追加一行
 * @param {string} pid - 进程 ID
 * @param {Object} msg - 输出消息
 */
const appendLineToPanel = (pid, msg) => {
  const body = document.getElementById('panelBody_' + pid);
  if (!body) return;

  // 判断是否在底部附近（用于自动滚动）
  const wasAtBottom = body.scrollHeight - body.scrollTop - body.clientHeight < 50;

  // 移除"等待输出..."提示
  if (body.querySelector('div[style*="等待输出"]')) {
    body.innerHTML = '';
  }

  // 获取当前行号
  const buf = state.outputBuffers[pid] || [];
  const lineIndex = buf.length - 1;

  body.insertAdjacentHTML('beforeend', formatLine(msg, lineIndex));

  // 自动滚动到底部
  if (state.autoScroll && wasAtBottom) {
    scrollToBottom(pid);
  }
};

/**
 * 滚动输出面板到底部
 * @param {string} pid - 进程 ID
 */
const scrollToBottom = (pid) => {
  const body = document.getElementById('panelBody_' + pid);
  if (body) {
    body.scrollTop = body.scrollHeight;
  }
};

/**
 * 格式化一行输出为 HTML
 * @param {Object} msg - 输出消息 { content, stream }
 * @param {number} index - 行号索引
 * @returns {string} HTML 字符串
 */
const formatLine = (msg, index) => {
  let cls = 'line';
  if (msg.stream === 'stderr') cls += ' stderr';
  else if (msg.stream === 'system') cls += ' system';

  // 行号部分
  const lineNumHtml = state.showLineNumbers
    ? `<span class="line-number">${index + 1}</span>`
    : '';

  return `<div class="${cls}">${lineNumHtml}<span class="line-content">${escapeHtml(msg.content)}</span></div>`;
};

/**
 * HTML 转义，防止 XSS（跨站脚本攻击）
 * @param {string} s - 原始字符串
 * @returns {string} 转义后的字符串
 *
 * [XSS 防护说明] 此函数利用浏览器原生的 textContent 属性实现 HTML 转义。
 * 工作原理：
 *   1. 创建一个临时 div 元素
 *   2. 将原始字符串赋值给 div.textContent（非 innerHTML）
 *   3. 读取 div.innerHTML，此时浏览器已自动将特殊字符转义为 HTML 实体
 *      例如: < 变为 &lt;, > 变为 &gt;, & 变为 &amp;, " 变为 &quot;
 *
 * 安全性分析：
 *   - textContent 赋值不会解析 HTML 标签，因此恶意脚本不会被注入 DOM
 *   - 读取 innerHTML 时浏览器自动完成转义，无需手动替换字符
 *   - 此方法比手动正则替换更安全可靠，因为它利用了浏览器内置的转义逻辑
 *
 * 使用场景：
 *   - 所有用户可控数据（进程ID、命令、输出内容）在插入 HTML 前必须经过此函数
 *   - 详见下方 formatLine、renderProcList、showProcessDetail 等函数中的调用
 *
 * 注意：此函数仅防止 XSS，不防止 SQL 注入或其他类型的攻击。
 *       后端 API 输入验证由 validators.py 模块负责。
 */
const escapeHtml = (s) => {
  if (!s) return '';
  const div = document.createElement('div');
  div.textContent = s;
  return div.innerHTML;
};

// ===================================================================
// ===== 输出面板增强功能 =====
// ===================================================================

/**
 * 复制当前进程的全部输出到剪贴板
 * @param {string} pid - 进程 ID
 */
const copyAllOutput = (pid) => {
  const buf = state.outputBuffers[pid] || [];
  if (buf.length === 0) {
    showToast('暂无输出内容可复制', 'info');
    return;
  }

  const text = buf.map(l => l.content).join('\n');

  navigator.clipboard.writeText(text).then(() => {
    showToast(`已复制 ${buf.length} 行输出`, 'success');
  }).catch(() => {
    // 降级方案：使用 textarea 复制
    const textarea = document.createElement('textarea');
    textarea.value = text;
    textarea.style.position = 'fixed';
    textarea.style.opacity = '0';
    document.body.appendChild(textarea);
    textarea.select();
    try {
      document.execCommand('copy');
      showToast(`已复制 ${buf.length} 行输出`, 'success');
    } catch (e) {
      showToast('复制失败，请手动复制', 'error');
    }
    document.body.removeChild(textarea);
  });
};

/**
 * 切换自动滚动开关
 */
const toggleAutoScroll = () => {
  state.autoScroll = !state.autoScroll;
  const btn = document.getElementById('btnAutoScroll');
  if (btn) {
    btn.classList.toggle('active', state.autoScroll);
  }
  showToast(state.autoScroll ? '自动滚动已开启' : '自动滚动已关闭', 'info', 1500);
};

/**
 * 切换行号显示开关
 */
const toggleLineNumbers = () => {
  state.showLineNumbers = !state.showLineNumbers;
  const btn = document.getElementById('btnLineNumbers');
  if (btn) {
    btn.classList.toggle('active', state.showLineNumbers);
  }
  // 重新渲染当前输出以更新行号显示
  if (state.selectedPid) {
    renderOutput(state.selectedPid);
  }
  showToast(state.showLineNumbers ? '行号已开启' : '行号已关闭', 'info', 1500);
};

// ===================================================================
// ===== 操作函数 =====
// ===================================================================

let procCounter = 0; // 进程计数器，用于生成唯一 ID

/**
 * 添加新进程
 */
const addProcess = async () => {
  const input = document.getElementById('cmdInput');
  const cmd = input.value.trim();
  if (!cmd) {
    showToast('请输入命令', 'info');
    return;
  }

  procCounter++;
  const pid = 'proc_' + procCounter;

  try {
    const res = await fetch('/api/register', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({id: pid, command: cmd})
    });

    if (!res.ok) {
      const err = await res.json();
      showToast('添加失败: ' + err.error, 'error');
      return;
    }

    input.value = '';
    showToast(`进程 ${pid} 已添加`, 'success');
    // 自动选中新添加的进程
    setTimeout(() => selectProcess(pid), 100);
  } catch (e) {
    showToast('网络错误: ' + e.message, 'error');
  }
};

/**
 * 启动全部进程
 */
const startAll = async () => {
  try {
    const res = await fetch('/api/start', {method: 'POST'});
    if (!res.ok) {
      const err = await res.json();
      showToast('启动失败: ' + err.error, 'error');
    } else {
      showToast('已发送启动指令', 'success');
    }
  } catch (e) {
    showToast('网络错误: ' + e.message, 'error');
  }
};

/**
 * 停止全部进程
 */
const stopAll = async () => {
  try {
    const res = await fetch('/api/stop_all', {method: 'POST'});
    if (!res.ok) {
      const err = await res.json();
      showToast('停止失败: ' + err.error, 'error');
    } else {
      showToast('已发送停止指令', 'success');
    }
  } catch (e) {
    showToast('网络错误: ' + e.message, 'error');
  }
};

/**
 * 清空所有进程
 */
const clearAll = async () => {
  if (!confirm('确定要清空所有进程吗？')) return;

  try {
    const res = await fetch('/api/processes');
    const data = await res.json();

    for (const p of data.processes) {
      await fetch(`/api/unregister/${p.id}`, {method: 'DELETE'});
    }

    state.selectedPid = null;
    state.outputBuffers = {};
    document.getElementById('outputArea').innerHTML = `
      <div class="empty-state">
        <div class="empty-state-icon">👈</div>
        <div>点击左侧进程查看实时输出</div>
      </div>`;

    showToast('已清空所有进程', 'success');
  } catch (e) {
    showToast('清空失败: ' + e.message, 'error');
  }
};

/**
 * 加载演示进程
 */
const loadDemo = async () => {
  const demos = [
    {id: 'ping_local', command: 'ping -n 6 127.0.0.1'},
    {id: 'ping_dns', command: 'ping -n 4 8.8.8.8'},
    {id: 'dir_list', command: 'powershell -Command "Get-ChildItem -Path C:\\Windows\\System32 -Filter *.dll | Select-Object -First 20 Name,Length"'},
    {id: 'counter', command: 'powershell -Command "for($i=1;$i -le 15;$i++){Write-Host \\"计数: $i\\"; Start-Sleep 0.3}"'},
  ];

  try {
    for (const d of demos) {
      await fetch('/api/register', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(d)
      });
    }
    await fetch('/api/start', {method: 'POST'});
    showToast('演示进程已加载并启动', 'success');
  } catch (e) {
    showToast('加载演示失败: ' + e.message, 'error');
  }
};

// ===================================================================
// ===== 初始化 =====
// ===================================================================

// 建立 SSE 连接
connectSSE();

// 定期刷新进程列表（作为 SSE 的备用机制）
setInterval(async () => {
  try {
    const res = await fetch('/api/processes');
    const data = await res.json();
    handleSummary(data);
  } catch (e) {
    // 忽略错误，SSE 是主要更新方式
  }
}, 5000);
</script>
</body>
</html>
"""
