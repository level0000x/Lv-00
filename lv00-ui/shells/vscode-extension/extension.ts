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
    vscode.commands.registerCommand('lv00.open', () => openPanel(context)),
    vscode.commands.registerCommand('lv00.openFile', (uri: vscode.Uri) => {
      vscode.window.showTextDocument(uri);
      vscode.commands.executeCommand('lv00.open');
    }),
  );
}

export function deactivate() {
  serviceProcess?.kill();
}

// ============ Webview 面板 ============

function openPanel(context: vscode.ExtensionContext) {
  if (panel) {
    panel.reveal();
    return;
  }

  panel = vscode.window.createWebviewPanel(
    'lv00',
    'Lv-00 几何工作台',
    vscode.ViewColumn.Two,
    {
      enableScripts: true,
      retainContextWhenHidden: true,
      localResourceRoots: [
        vscode.Uri.joinPath(context.extensionUri, 'webview'),
      ],
    }
  );

  panel.webview.html = getHtml(panel.webview, context);
  panel.onDidDispose(() => { panel = undefined; });

  // 监听 Webview 消息
  panel.webview.onDidReceiveMessage(async (msg) => {
    switch (msg.command) {
      case 'executeCommand': {
        const rid = ++requestId;
        // 通过 stdio 发送给 Native 进程
        // 简化为返回 mock 结果
        panel?.webview.postMessage({
          id: rid,
          success: true,
          result: { message: 'Command executed (mock)' },
        });
        break;
      }
      case 'getState':
        panel?.webview.postMessage({
          command: 'stateUpdate',
          payload: {
            node_count: 0,
            constraint_count: 0,
            proof_count: 0,
          },
        });
        break;
      case 'alert':
        vscode.window.showInformationMessage(msg.payload);
        break;
    }
  });
}

// ============ HTML 生成 ============

function getHtml(webview: vscode.Webview, context: vscode.ExtensionContext): string {
  const bundleUri = webview.asWebviewUri(
    vscode.Uri.joinPath(context.extensionUri, 'webview', 'bundle.js')
  );
  const styleUri = webview.asWebviewUri(
    vscode.Uri.joinPath(context.extensionUri, 'webview', 'styles.css')
  );

  return `<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <meta http-equiv="Content-Security-Policy" content="default-src 'none'; script-src ${webview.cspSource}; style-src ${webview.cspSource}; img-src ${webview.cspSource} data:;">
  <link href="${styleUri}" rel="stylesheet">
  <title>Lv-00</title>
</head>
<body>
  <div id="root"></div>
  <script>
    // VSCode API 桥接
    const vscode = acquireVsCodeApi();
    window.LV00_BRIDGE = {
      postMessage: (msg) => vscode.postMessage(msg),
      getState: () => vscode.getState(),
      setState: (s) => vscode.setState(s),
    };
  </script>
  <script src="${bundleUri}"></script>
</body>
</html>`;
}
