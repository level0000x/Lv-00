import React, { useEffect, useRef } from 'react';
import { ContextMenuAction } from '../L5-core/types';

interface ContextMenuProps {
  open: boolean;
  x: number;
  y: number;
  items: ContextMenuAction[];
  onClose: () => void;
  onAction: (id: string) => void;
}

const ContextMenu: React.FC<ContextMenuProps> = ({ open, x, y, items, onClose, onAction }) => {
  const ref = useRef<HTMLDivElement>(null);

  useEffect(() => {
    if (!open) return;
    const handler = (e: MouseEvent) => {
      if (ref.current && !ref.current.contains(e.target as Node)) {
        onClose();
      }
    };
    const keyHandler = (e: KeyboardEvent) => {
      if (e.key === 'Escape') onClose();
    };
    document.addEventListener('mousedown', handler);
    document.addEventListener('keydown', keyHandler);
    return () => {
      document.removeEventListener('mousedown', handler);
      document.removeEventListener('keydown', keyHandler);
    };
  }, [open, onClose]);

  if (!open) return null;

  return (
    <div ref={ref} className="context-menu" style={{ left: x, top: y }}>
      {items.map((item, i) => (
        <React.Fragment key={item.id}>
          {i > 0 && item.id === 'separator' ? (
            <div className="context-menu-separator" />
          ) : (
            <div
              className="context-menu-item"
              onClick={() => { onAction(item.id); onClose(); }}
            >
              <span>{item.label}</span>
              {item.shortcut && <span className="context-menu-shortcut">{item.shortcut}</span>}
            </div>
          )}
        </React.Fragment>
      ))}
    </div>
  );
};

export default ContextMenu;
