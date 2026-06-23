import React, { useState, useRef, useEffect } from 'react';

interface TooltipProps {
  content: string;
  children: React.ReactElement;
  delay?: number;
}

const Tooltip: React.FC<TooltipProps> = ({ content, children, delay = 500 }) => {
  const [visible, setVisible] = useState(false);
  const [pos, setPos] = useState({ x: 0, y: 0 });
  const timer = useRef<ReturnType<typeof setTimeout>>();
  const childRef = useRef<HTMLElement>();

  const show = (e: React.MouseEvent) => {
    timer.current = setTimeout(() => {
      const rect = (e.currentTarget as HTMLElement).getBoundingClientRect();
      setPos({ x: rect.left + rect.width / 2, y: rect.bottom + 6 });
      setVisible(true);
    }, delay);
  };

  const hide = () => {
    clearTimeout(timer.current);
    setVisible(false);
  };

  useEffect(() => () => clearTimeout(timer.current), []);

  const child = React.cloneElement(children, {
    onMouseEnter: show,
    onMouseLeave: hide,
  } as Record<string, unknown>);

  return (
    <>
      {child}
      {visible && (
        <div className="tooltip" style={{ left: pos.x, top: pos.y, transform: 'translateX(-50%)' }}>
          {content}
        </div>
      )}
    </>
  );
};

export default Tooltip;
