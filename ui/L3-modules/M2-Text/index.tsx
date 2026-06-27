import React, { useRef, useState, useEffect, useMemo } from 'react';

interface TextViewProps {
  value: string;
  onChange?: (text: string) => void;
  placeholder?: string;
  readonly?: boolean;
}

export const TextView: React.FC<TextViewProps> = ({
  value: externalValue,
  onChange,
  placeholder = 'point A at (100, 200)\npoint B at (400, 150)\nsegment AB between A and B',
  readonly = false,
}) => {
  const ref = useRef<HTMLTextAreaElement>(null);
  const gutterRef = useRef<HTMLDivElement>(null);
  const [text, setText] = useState(externalValue);
  let timer: ReturnType<typeof setTimeout>;

  useEffect(() => {
    if (document.activeElement !== ref.current) {
      setText(externalValue);
    }
  }, [externalValue]);

  /* ---- Line numbers ---- */
  const lineCount = useMemo(() => {
    const lines = text.split('\n');
    return lines.length;
  }, [text]);

  const lineNumbers = useMemo(() => {
    return Array.from({ length: lineCount }, (_, i) => i + 1);
  }, [lineCount]);

  /* ---- Sync scroll ---- */
  const handleScroll = () => {
    if (gutterRef.current && ref.current) {
      gutterRef.current.scrollTop = ref.current.scrollTop;
    }
  };

  return (
    <div style={{ display: 'flex', width: '100%', height: '100%', minHeight: 200 }}>
      {/* Line number gutter */}
      <div
        ref={gutterRef}
        style={{
          width: 44,
          flexShrink: 0,
          overflow: 'hidden',
          background: 'var(--color-bg-secondary)',
          borderRight: '1px solid var(--color-border-primary)',
          padding: '8px 0',
          userSelect: 'none',
          textAlign: 'right',
          paddingRight: 8,
          color: 'var(--color-text-muted)',
          fontFamily: 'var(--font-mono)',
          fontSize: 12,
          lineHeight: '1.7',
        }}
      >
        {lineNumbers.map((n) => (
          <div key={n} style={{ height: '1.7em' }}>{n}</div>
        ))}
      </div>
      {/* Textarea */}
      <textarea
        ref={ref}
        value={text}
        onChange={(e) => {
          setText(e.target.value);
          clearTimeout(timer);
          timer = setTimeout(() => onChange?.(e.target.value), 300);
          handleScroll();
        }}
        onScroll={handleScroll}
        readOnly={readonly}
        spellCheck={false}
        placeholder={placeholder}
        style={{
          flex: 1,
          background: 'transparent',
          color: 'var(--color-text-primary)',
          border: 'none',
          borderLeft: 'none',
          resize: 'none',
          padding: '8px 8px 8px 12px',
          fontFamily: 'var(--font-mono)',
          fontSize: 13,
          lineHeight: '1.7',
          outline: 'none',
          minHeight: 200,
          tabSize: 2,
        }}
      />
    </div>
  );
};
