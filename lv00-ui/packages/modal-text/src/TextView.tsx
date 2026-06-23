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
  