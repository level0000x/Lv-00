// ============================================================
// lv00-ui/shells/vscode-extension — VSCode 插件外壳 (TypeScript)
// Webview 渲染 6 模态 + acquireVsCodeApi 桥接 + stdio 服务通信
// ============================================================

import * as vscode from 'vscode';

let panel: vscode.WebviewPanel | undefined;
let serviceProcess: { kill: () => void } | undefined;
let requestId = 0;
const callbacks = new Map<number, (result: any) => void>();

// ============ 激活入口 ============

export function activate(context: vscode.ExtensionContext) {
  console.log('[Lv-00] 插件已激活');

  // 注册命令
  context.subscriptions.push(
    vscode.commands.registerCommand('lv00.open', () => openPanel(co