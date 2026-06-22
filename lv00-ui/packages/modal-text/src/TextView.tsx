// ============================================================
// @lv00/modal-text — M2 结构化文本编辑器 (TextView)
// Monaco Editor + Lv00 语法高亮 + 自动补全 + 双相同步
// ============================================================

import React, { useRef, useEffect, useState, useCallback } from 'react';
import { SceneController } from '@lv00/scene-controller';

// Monaco 类型声明（在运行时由 monaco-editor 提供）
declare const monaco: any;

interface TextViewProps {
  controller: SceneController;
  initialText?: string;
}

export const TextView: React.FC<TextViewProps> = ({ controller, initialText = '' }) => {
  const containerRef = useRef<HTMLDivElement>(null);
  const editorRef = useRef<any>(null);
  const [isSyncing, setIsSyncing] = useState(false);
  const modelRef = useRef<any>(null);
  const lastKnownContent = useRef('');

  // --- 初始化编辑器 ---
  useEffect(() => {
    if (!containerRef.current || typeof monaco === 'undefined') return;

    const initialContent = initialText || controller.getTextRepresentation();
    lastKnownContent.current = initialContent;

    // 注册语言
    monaco.languages.register({ id: 'lv00' });
    monaco.languages.setMonarchTokensProvider('lv00', {
      tokenizer: {
        root: [
          [/^point\b/, 'keyword'],
          [/^segment\b/, 'keyword'],
          [/^constraint\b/, 'keyword'],
          [/^prove\b/, 'keyword'],
          [/^function\b/, 'keyword'],
          [/^at\b/, 'keyword'],
          [/^between\b/, 'keyword'],
          [/^and\b/, 'keyword'],
          [/\b[A-Za-z_][A-Za-z0-9_]*\b/, 'identifier'],
          [/\d+\.?\d*/, 'number'],
          [/[(),]/, 'delimiter'],
          [/--.*$/, 'comment'],
        ],
      },
    });

    // 自动补全
    monaco.languages.registerCompletionItemProvider('lv00', {
      provideCompletionItems: () => {
        const keywords = ['point', 'segment', 'constraint', 'prove', 'function', 'at', 'between', 'and'];
        const nodeNames = controller.getNodeNames();
        return {
          suggestions: [
            ...keywords.map((k: string) => ({ label: k, kind: monaco.languages.CompletionItemKind.Keyword })),
            ...nodeNames.map((n: string) => ({ label: n, kind: monaco.languages.CompletionItemKind.Variable })),
          ],
        };
      },
    });

    const editor = monaco.editor.create(containerRef.current, {
      value: initialContent,
      language: 'lv00',
      theme: 'vs-dark',
      automaticLayout: true,
      fontSize: 14,
      fontFamily: "'Cascadia Code', 'Fira Code', monospace",
      minimap: { enabled: false },
      scrollBeyondLastLine: false,
      lineNumbers: 'on',
      tabSize: 4,
      insertSpaces: true,
      wordWrap: 'off',
    });

    editorRef.current = editor;
    modelRef.current = editor.getModel();

    // 内容变化 → 同步到 L3
    let debounceTimer: ReturnType<typeof setTimeout>;
    editor.onDidChangeModelContent(() => {
      clearTimeout(debounceTimer);
      debounceTimer = setTimeout(() => {
        if (!isSyncing) {
          const content = editor.getValue();
          lastKnownContent.current = content;
          controller.applyTextEdit(content);
        }
      }, 300);
    });

    return () => {
      clearTimeout(debounceTimer);
      editor.dispose();
    };
  }, [controller]);

  // --- 监听 L3 变化 → 更新编辑器（焦点不在编辑器时） ---
  useEffect(() => {
    const unsubscribe = controller.onStateChange(() => {
      if (!editorRef.current) return;
      const editorDom = containerRef.current?.querySelector('.monaco-editor');
      const isFocused = editorDom && document.activeElement && editorDom.contains(document.activeElement);

      if (!isFocused) {
        const newText = controller.getTextRepresentation();
        if (newText !== lastKnownContent.current) {
          setIsSyncing(true);
          lastKnownContent.current = newText;
          editorRef.current.setValue(newText);
          setIsSyncing(false);
        }
      }
    });

    return unsubscribe;
  }, [controller]);

  return (
    <div
      ref={containerRef}
      style={{
        width: '100%',
        height: '100%',
        background: '#0a0a0a',
        minHeight: 200,
      }}
    />
  );
};

// 无 Monaco 时的降级编辑器
export const TextViewFallback: React.FC<TextViewProps> = ({ controller }) => {
  const textareaRef = useRef<HTMLTextAreaElement>(null);
  const [text, setText] = useState(controller.getTextRepresentation());
  let debounceTimer: ReturnType<typeof setTimeout>;

  useEffect(() => {
    const unsubscribe = controller.onStateChange(() => {
      const newText = controller.getTextRepresentation();
      if (newText !== text && document.activeElement !== textareaRef.current) {
        setText(newText);
      }
    });
    return unsubscribe;
  }, [controller, text]);

  const handleChange = (e: React.ChangeEvent<HTMLTextAreaElement>) => {
    const val = e.target.value;
    setText(val);
    clearTimeout(debounceTimer);
    debounceTimer = setTimeout(() => controller.applyTextEdit(val), 300);
  };

  return (
    <textarea
      ref={textareaRef}
      value={text}
      onChange={handleChange}
      spellCheck={false}
      style={{
        width: '100%',
        height: '100%',
        background: '#0a0a0a',
        color: '#c8c8c8',
        border: 'none',
        resize: 'none',
        padding: 12,
        fontFamily: 'monospace',
        fontSize: 14,
        lineHeight: 1.6,
        outline: 'none',
      }}
    />
  );
};
