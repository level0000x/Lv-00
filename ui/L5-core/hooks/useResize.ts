import { useEffect, useState } from 'react';

export function useResize(ref: React.RefObject<HTMLElement | null>) {
  const [size, setSize] = useState({ width: 0, height: 0 });

  useEffect(() => {
    const el = ref.current;
    if (!el) return;

    const observer = new ResizeObserver((entries) => {
      for (const entry of entries) {
        const { width, height } = entry.contentRect;
        setSize({ width: Math.round(width), height: Math.round(height) });
      }
    });

    observer.observe(el);
    setSize({ width: el.clientWidth, height: el.clientHeight });

    return () => observer.disconnect();
  }, [ref.current]);

  return size;
}
