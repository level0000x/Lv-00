import React, { useRef, useState, useEffect } from 'react';

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
  const [text, setText] = useState(externalValue);
  let timer: ReturnType<typeof setTimeout>;

  useEffect(() => {
    if (document.activeElement !== ref.current) {
      setText(externalValue);
    }
  }, [externalValue]);

  return (
    <textarea
      ref={ref}
      value={text}
      onChange={(e) => {
        setText(e.target.value);
        clearTimeout(timer);
        timer = setTimeout(() => onChange?.(e.target.value), 300);
      }}
      readOnly={readonly}
      spellCheck={false}
      placeholder={placeholder}
      style={{
        width: '100%',
        height: '100%',
        background: 'transparent',
        color: 'var(--color-text-primary)',
        border: 'none',
        resize: 'none',
        padding: '8px 0',
        fontFamily: 'var(--font-mono)',
        fontSize: 13,
        lineHeight: 1.7,
        outline: 'none',
        minHeight: 200,
      }}
    />
  );
};
