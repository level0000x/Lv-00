/**
 * @module components/panels/AssistantPanel
 * @description 增强型 AI 助手面板，支持聊天界面、提供商选择、模型参数、
 *              流式显示和 Markdown 渲染。
 *              Enhanced AI Assistant panel with chat interface,
 *              provider selection, model parameters, streaming display,
 *              and markdown rendering support.
 */

import React, { useState, useCallback, useRef, useEffect, useMemo } from 'react';
import Panel from './Panel';
import { useAppStore } from '@/stores';
import type { AIProvider } from '@/types';
import { sanitizeHtml } from '@/utils/sanitizeHtml';

// ================================================================
// 常量 / Constants
// ================================================================

/**
 * AI Provider 配置列表。
 * 以下为默认配置，实际应从环境变量或配置文件中读取。
 * Default AI provider configurations; in production these should be
 * read from environment variables or a configuration file.
 */
const AI_PROVIDERS: AIProvider[] = [
  {
    id: 'openai',
    name: 'OpenAI',
    nameZh: 'OpenAI',
    endpoint: 'https://api.openai.com/v1/chat/completions',
    apiKey: 'sk-***',
    enabled: true,
  },
  {
    id: 'claude',
    name: 'Claude',
    nameZh: 'Claude',
    endpoint: 'https://api.anthropic.com/v1/messages',
    apiKey: 'sk-ant-***',
    enabled: true,
  },
  {
    id: 'gemini',
    name: 'Gemini',
    nameZh: 'Gemini',
    endpoint: 'https://generativelanguage.googleapis.com/v1beta',
    apiKey: '***',
    enabled: true,
  },
  {
    id: 'deepseek',
    name: 'DeepSeek',
    nameZh: 'DeepSeek',
    endpoint: 'https://api.deepseek.com/v1/chat/completions',
    apiKey: 'sk-***',
    enabled: true,
  },
  {
    id: 'local',
    name: 'Local Model',
    nameZh: '本地模型',
    endpoint: 'http://localhost:11434/api/chat',
    apiKey: '',
    enabled: true,
  },
];

// ================================================================
// 简易 Markdown 渲染器 / Simple Markdown Renderer
// ================================================================

function renderMarkdown(text: string): string {
  let html = text
    // 转义 HTML / Escape HTML
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    // 代码块 (```lang\n...\n```) / Code blocks (```lang\n...\n```)
    .replace(/```(\w*)\n([\s\S]*?)```/g, (_match, lang, code) => {
      return `<pre class="ai-code-block"><code class="ai-code-lang">${lang || ''}</code><code>${code.trim()}</code></pre>`;
    })
    // 行内代码 / Inline code
    .replace(/`([^`]+)`/g, '<code class="ai-inline-code">$1</code>')
    // 标题 / Headers
    .replace(/^### (.+)$/gm, '<h4 class="ai-h4">$1</h4>')
    .replace(/^## (.+)$/gm, '<h3 class="ai-h3">$1</h3>')
    .replace(/^# (.+)$/gm, '<h2 class="ai-h2">$1</h2>')
    // 粗体 / Bold
    .replace(/\*\*(.+?)\*\*/g, '<strong>$1</strong>')
    // 斜体 / Italic
    .replace(/\*(.+?)\*/g, '<em>$1</em>')
    // 引用块 / Blockquote
    .replace(/^&gt; (.+)$/gm, '<blockquote class="ai-blockquote">$1</blockquote>')
    // 无序列表项 / Unordered list items
    .replace(/^- (.+)$/gm, '<li class="ai-li">$1</li>')
    // 有序列表项 / Ordered list items
    .replace(/^\d+\. (.+)$/gm, '<li class="ai-li">$1</li>')
    // 换行 / Line breaks
    .replace(/\n\n/g, '<br/><br/>')
    .replace(/\n/g, '<br/>');

  return html;
}

// ================================================================
// 组件 / Component
// ================================================================

const AssistantPanel: React.FC = () => {
  const chatMessages = useAppStore((s) => s.chatMessages);
  const activeProvider = useAppStore((s) => s.activeProvider);
  const isStreaming = useAppStore((s) => s.isStreaming);
  const modelTemperature = useAppStore((s) => s.modelTemperature);
  const modelMaxTokens = useAppStore((s) => s.modelMaxTokens);

  const sendMessage = useAppStore((s) => s.sendMessage);
  const clearMessages = useAppStore((s) => s.clearMessages);
  const setActiveProvider = useAppStore((s) => s.setActiveProvider);
  const setModelTemperature = useAppStore((s) => s.setModelTemperature);
  const setModelMaxTokens = useAppStore((s) => s.setModelMaxTokens);
  const setIsStreaming = useAppStore((s) => s.setIsStreaming);

  const [input, setInput] = useState('');
  const [showSettings, setShowSettings] = useState(false);
  const messagesEndRef = useRef<HTMLDivElement>(null);
  const textareaRef = useRef<HTMLTextAreaElement>(null);

  // 新消息时自动滚动到底部 / Auto-scroll to bottom on new messages
  useEffect(() => {
    messagesEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [chatMessages]);

  // 自动调整文本域高度 / Auto-resize textarea
  useEffect(() => {
    if (textareaRef.current) {
      textareaRef.current.style.height = 'auto';
      textareaRef.current.style.height = `${Math.min(textareaRef.current.scrollHeight, 120)}px`;
    }
  }, [input]);

  const handleSend = useCallback(async () => {
    const trimmed = input.trim();
    if (!trimmed || isStreaming) return;
    setInput('');
    await sendMessage(trimmed);
  }, [input, isStreaming, sendMessage]);

  const handleKeyDown = useCallback(
    (e: React.KeyboardEvent<HTMLTextAreaElement>) => {
      if (e.key === 'Enter' && !e.shiftKey) {
        e.preventDefault();
        handleSend();
      }
    },
    [handleSend],
  );

  const handleClear = useCallback(() => {
    clearMessages();
  }, [clearMessages]);

  const handleProviderChange = useCallback(
    (e: React.ChangeEvent<HTMLSelectElement>) => {
      setActiveProvider(e.target.value);
    },
    [setActiveProvider],
  );

  const handleTemperatureChange = useCallback(
    (e: React.ChangeEvent<HTMLInputElement>) => {
      const value = parseFloat(e.target.value);
      // 防御性检查：NaN 或无效值时不更新状态
      if (!Number.isNaN(value)) {
        setModelTemperature(value);
      }
    },
    [setModelTemperature],
  );

  const handleMaxTokensChange = useCallback(
    (e: React.ChangeEvent<HTMLInputElement>) => {
      const value = parseInt(e.target.value, 10);
      // 防御性检查：NaN 或无效值时不更新状态
      if (!Number.isNaN(value)) {
        setModelMaxTokens(value);
      }
    },
    [setModelMaxTokens],
  );

  const currentProvider = useMemo(
    () => AI_PROVIDERS.find((p) => p.id === activeProvider),
    [activeProvider],
  );

  const isEmpty = chatMessages.length === 0;

  /** 缓存每条消息的 Markdown 渲染结果，并经过 HTML 消毒处理防止 XSS / Memoized Markdown rendering per message with XSS sanitization */
  const renderedMessages = useMemo(
    () => chatMessages.map((msg) => ({
      ...msg,
      renderedHtml: msg.role !== 'user' && msg.content
        ? sanitizeHtml(renderMarkdown(msg.content))
        : null,
    })),
    [chatMessages],
  );

  return (
    <>
      {/* 聊天面板 / Chat Panel */}
      <Panel title="AI ASSISTANT / AI 助手" panelId="assistant-chat">
        <div className="ai-assistant-container">
          {/* 提供商选择器 / Provider Selector */}
          <div className="ai-provider-row">
            <select
              className="select-field ai-provider-select"
              value={activeProvider}
              onChange={handleProviderChange}
              disabled={isStreaming}
            >
              {AI_PROVIDERS.map((provider) => (
                <option key={provider.id} value={provider.id}>
                  {provider.name} / {provider.nameZh}
                </option>
              ))}
            </select>
            <button
              className="btn btn-ghost ai-settings-btn"
              onClick={() => setShowSettings((prev) => !prev)}
              title="Settings / 设置"
            >
              {showSettings ? '\u25B2' : '\u2699'}
            </button>
          </div>

          {/* 设置（可折叠）/ Settings (collapsible) */}
          {showSettings && (
            <div className="ai-settings-panel">
              <div className="ai-setting-row">
                <label>
                  Temperature / 温度
                  <span className="ai-setting-value">{modelTemperature.toFixed(1)}</span>
                </label>
                <input
                  type="range"
                  min="0"
                  max="2"
                  step="0.1"
                  value={modelTemperature}
                  onChange={handleTemperatureChange}
                  className="ai-range-input"
                />
              </div>
              <div className="ai-setting-row">
                <label>
                  Max Tokens / 最大令牌
                  <span className="ai-setting-value">{modelMaxTokens}</span>
                </label>
                <input
                  type="range"
                  min="64"
                  max="32768"
                  step="64"
                  value={modelMaxTokens}
                  onChange={handleMaxTokensChange}
                  className="ai-range-input"
                />
              </div>
              {currentProvider && (
                <div className="ai-setting-info">
                  <span>Endpoint / 端点: </span>
                  <span className="ai-setting-detail">{currentProvider.endpoint}</span>
                </div>
              )}
            </div>
          )}

          {/* 消息区域 / Messages Area */}
          <div className="ai-messages" id="aiMessages">
            {isEmpty ? (
              <div className="ai-empty-state">
                <div className="ai-empty-icon">{'\uD83E\uDD16'}</div>
                <div className="ai-empty-title">
                  AI Assistant / AI 助手
                </div>
                <div className="ai-empty-desc">
                  Ask questions about geometry, formulas, or the Lv-00 engine.
                  <br />
                  输入关于几何、公式或 Lv-00 引擎的问题。
                </div>
                <div className="ai-empty-hints">
                  <div className="ai-hint-item" onClick={() => setInput('约束求解器是如何工作的？')}>
                    约束求解器是如何工作的？
                  </div>
                  <div className="ai-hint-item" onClick={() => setInput('解释符号几何方法')}>
                    解释符号几何方法
                  </div>
                  <div className="ai-hint-item" onClick={() => setInput('展示一个公式示例')}>
                    展示一个公式示例
                  </div>
                </div>
              </div>
            ) : (
              renderedMessages.map((msg) => (
                <div key={msg.id} className={`ai-msg ai-msg-${msg.role}`}>
                  <div className="ai-msg-role">
                    {msg.role === 'user'
                      ? 'YOU / 你'
                      : msg.role === 'assistant'
                        ? (currentProvider?.name ?? 'AI') + ' / AI'
                        : 'SYSTEM / 系统'}
                  </div>
                  <div className="ai-msg-content">
                    {msg.role === 'user' ? (
                      <span>{msg.content}</span>
                    ) : msg.renderedHtml ? (
                      <span dangerouslySetInnerHTML={{ __html: msg.renderedHtml }} />
                    ) : msg.isStreaming ? (
                      <span className="ai-streaming-cursor">
                        {'\u258C'}
                      </span>
                    ) : null}
                  </div>
                  <div className="ai-msg-time">
                    {new Date(msg.timestamp).toLocaleTimeString('zh-CN', {
                      hour12: false,
                      hour: '2-digit',
                      minute: '2-digit',
                      second: '2-digit',
                    })}
                  </div>
                </div>
              ))
            )}
            <div ref={messagesEndRef} />
          </div>

          {/* 输入区域 / Input Area */}
          <div className="ai-input-area">
            <textarea
              ref={textareaRef}
              className="input-field ai-textarea"
              placeholder={
                isStreaming
                  ? 'Generating... / 生成中...'
                  : 'Type a message... (Shift+Enter for new line) / 输入消息... (Shift+Enter 换行)'
              }
              value={input}
              onChange={(e) => setInput(e.target.value)}
              onKeyDown={handleKeyDown}
              disabled={isStreaming}
              rows={1}
            />
            <div className="ai-input-actions">
              <button
                className="btn btn-ghost ai-action-btn"
                onClick={handleClear}
                disabled={isEmpty || isStreaming}
                title="Clear / 清空"
              >
                {'\uD83D\uDDD1'}
              </button>
              {isStreaming ? (
                <button
                  className="btn btn-danger ai-send-btn"
                  onClick={() => {
                    /* 停止当前流式输出：设置流式状态为 false */
                    setIsStreaming(false);
                  }}
                  title="Stop / 停止"
                >
                  {'\u23F9'} STOP / 停止
                </button>
              ) : (
                <button
                  className="btn btn-primary ai-send-btn"
                  onClick={handleSend}
                  disabled={!input.trim()}
                  title="Send / 发送"
                >
                  {'\u27A4'} SEND / 发送
                </button>
              )}
            </div>
          </div>
        </div>
      </Panel>
    </>
  );
};

export default AssistantPanel;
