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
  .header h1 { 
    font-size: 18px; 
    color: var(--accent); 
    display: flex;
    align-items: center;
    gap: 8px;
  }
  .header h1::before {
    content: "◉";
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

  /* 主体布局 */
  .main {
    display: flex;
    flex: 1;
    overflow: hidden;
  }
  .left-panel {
    width: 380px;
    flex-shrink: 0;
    background: var(--surface);
    border-right: 1px solid var(--border);
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

  /* 进程卡片 */
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

  /* 输出面板 */
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
  }
  .output-panel .line.stderr { color: var(--red); }
  .output-panel .line.system { color: var(--text-dim); font-style: italic; }
  .output-panel .line:hover {
    background: rgba(255,255,255,0.03);
  }

  /* 控制区 */
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

  /* 空状态 */
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

  /* 连接状态指示器 */
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

  /* 滚动条 */
  ::-webkit-scrollbar { width: 8px; height: 8px; }
  ::-webkit-scrollbar-track { background: transparent; }
  ::-webkit-scrollbar-thumb { 
    background: var(--border); 
    border-radius: 4px;
    border: 2px solid transparent;
    background-clip: padding-box;
  }
  ::-webkit-scrollbar-thumb:hover { background: var(--text-dim); }

  /* 响应式 */
  @media (max-width: 768px) {
    .left-panel { width: 100%; }
    .main { flex-direction: column; }
    .controls { flex-wrap: wrap; }
    .controls input { width: 100%; margin-bottom: 8px; }
  }
</style>
</head>
<body>

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

<div class="main">
  <div class="left-panel" id="procList">
    <div class="empty-state">
      <div class="empty-state-icon">📋</div>
      <div>暂无进程</div>
      <div style="font-size: 12px;">使用下方控制栏添加进程</div>
    </div>
  </div>
  <div class="right-panel" id="outputArea">
    <div class="empty-state">
      <div class="empty-state-icon">👈</div>
      <div>点击左侧进程查看实时输出</div>
    </div>
  </div>
</div>

<div class="controls">
  <input type="text" id="cmdInput" placeholder="输入命令... (例: ping -n 5 127.0.0.1)" onkeydown="if(event.key==='Enter')addProcess()">
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
</div>

<script>
// ===== 状态管理 =====
const state = {
  processes: {},
  selectedPid: null,
  outputBuffers: {},
  maxLines: 500,
  connectionStatus: 'connecting'
};

// ===== SSE 连接 =====
let es = null;
let reconnectAttempts = 0;
const maxReconnectAttempts = 5;

function connectSSE() {
  if (es) {
    es.close();
  }

  es = new EventSource('/stream');
  
  es.onopen = () => {
    console.log('SSE 连接已建立');
    state.connectionStatus = 'connected';
    reconnectAttempts = 0;
    updateConnectionStatus();
  };

  es.onmessage = (e) => {
    try {
      const msg = JSON.parse(e.data);
      if (msg.type === 'output') {
        handleOutput(msg.data);
      } else if (msg.type === 'summary') {
        handleSummary(msg.data);
      } else if (msg.type === 'error') {
        console.error('SSE 错误:', msg.message);
      }
    } catch (err) {
      console.error('解析 SSE 消息失败:', err);
    }
  };

  es.onerror = (e) => {
    console.error('SSE 连接错误');
    state.connectionStatus = 'disconnected';
    updateConnectionStatus();
    
    if (reconnectAttempts < maxReconnectAttempts) {
      reconnectAttempts++;
      setTimeout(connectSSE, 3000 * reconnectAttempts);
    }
  };
}

function updateConnectionStatus() {
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
}

function handleOutput(msg) {
  if (!state.outputBuffers[msg.process_id]) {
    state.outputBuffers[msg.process_id] = [];
  }
  const buf = state.outputBuffers[msg.process_id];
  buf.push(msg);
  if (buf.length > state.maxLines) buf.shift();

  if (state.selectedPid === msg.process_id) {
    appendLineToPanel(msg.process_id, msg);
  }
}

function handleSummary(data) {
  state.processes = {};
  data.processes.forEach(p => {
    state.processes[p.id] = p;
  });
  renderProcList();
  updateBadges(data.status_counts, data.total);
  
  // 如果选中的进程还在运行，更新显示
  if (state.selectedPid && state.processes[state.selectedPid]) {
    updateSelectedPanel();
  }
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
  const pids = Object.keys(state.processes);
  
  if (pids.length === 0) {
    container.innerHTML = `
      <div class="empty-state">
        <div class="empty-state-icon">📋</div>
        <div>暂无进程</div>
        <div style="font-size: 12px;">使用下方控制栏添加进程</div>
      </div>`;
    return;
  }
  
  container.innerHTML = pids.map(pid => {
    const p = state.processes[pid];
    const statusClass = p.status;
    const isActive = state.selectedPid === pid;
    return `
      <div class="proc-card ${isActive ? 'active' : ''}" onclick="selectProcess('${pid}')">
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
}

function selectProcess(pid) {
  state.selectedPid = pid;
  renderProcList();
  renderOutput(pid);
}

function updateSelectedPanel() {
  if (!state.selectedPid) return;
  renderOutput(state.selectedPid);
}

function renderOutput(pid) {
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
          ${p.status} | exit=${p.exit_code ?? '-'} | ${p.duration}
        </span>
      </div>
      <div class="panel-body" id="panelBody_${pid}">
        ${buf.length > 0 ? buf.map(l => formatLine(l)).join('') : '<div style="color:var(--text-dim);padding-top:20px;">等待输出...</div>'}
      </div>
    </div>
  `;
  
  scrollToBottom(pid);
}

function appendLineToPanel(pid, msg) {
  const body = document.getElementById('panelBody_' + pid);
  if (!body) return;
  
  const wasAtBottom = body.scrollHeight - body.scrollTop - body.clientHeight < 50;
  
  // 移除"等待输出..."提示
  if (body.querySelector('div[style*="等待输出"]')) {
    body.innerHTML = '';
  }
  
  body.insertAdjacentHTML('beforeend', formatLine(msg));
  
  if (wasAtBottom) {
    scrollToBottom(pid);
  }
}

function scrollToBottom(pid) {
  const body = document.getElementById('panelBody_' + pid);
  if (body) {
    body.scrollTop = body.scrollHeight;
  }
}

function formatLine(msg) {
  let cls = 'line';
  if (msg.stream === 'stderr') cls += ' stderr';
  else if (msg.stream === 'system') cls += ' system';
  return `<div class="${cls}">${escapeHtml(msg.content)}</div>`;
}

function escapeHtml(s) {
  if (!s) return '';
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
  
  try {
    const res = await fetch('/api/register', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({id: pid, command: cmd})
    });
    
    if (!res.ok) {
      const err = await res.json();
      alert('添加失败: ' + err.error);
      return;
    }
    
    input.value = '';
    // 自动选中新添加的进程
    setTimeout(() => selectProcess(pid), 100);
  } catch (e) {
    alert('网络错误: ' + e.message);
  }
}

async function startAll() {
  try {
    const res = await fetch('/api/start', {method: 'POST'});
    if (!res.ok) {
      const err = await res.json();
      alert('启动失败: ' + err.error);
    }
  } catch (e) {
    alert('网络错误: ' + e.message);
  }
}

async function stopAll() {
  try {
    const res = await fetch('/api/stop_all', {method: 'POST'});
    if (!res.ok) {
      const err = await res.json();
      alert('停止失败: ' + err.error);
    }
  } catch (e) {
    alert('网络错误: ' + e.message);
  }
}

async function clearAll() {
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
  } catch (e) {
    alert('清空失败: ' + e.message);
  }
}

async function loadDemo() {
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
  } catch (e) {
    alert('加载演示失败: ' + e.message);
  }
}

// ===== 初始化 =====
connectSSE();

// 定期刷新进程列表（作为 SSE 的备用）
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
