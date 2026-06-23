import { useState, useCallback } from 'react';

export function useExport() {
  const [isExporting, setIsExporting] = useState(false);

  const exportJSON = useCallback((data: unknown, filename: string) => {
    setIsExporting(true);
    try {
      const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
      downloadBlob(blob, `${filename}.json`);
    } finally {
      setIsExporting(false);
    }
  }, []);

  const exportSVG = useCallback((svgElement: SVGSVGElement | null, filename: string) => {
    if (!svgElement) return;
    setIsExporting(true);
    try {
      const clone = svgElement.cloneNode(true) as SVGSVGElement;
      const serializer = new XMLSerializer();
      const svgString = serializer.serializeToString(clone);
      const blob = new Blob([svgString], { type: 'image/svg+xml' });
      downloadBlob(blob, `${filename}.svg`);
    } finally {
      setIsExporting(false);
    }
  }, []);

  const exportPNG = useCallback((canvas: HTMLCanvasElement | null, filename: string) => {
    if (!canvas) return;
    setIsExporting(true);
    try {
      canvas.toBlob((blob) => {
        if (blob) downloadBlob(blob, `${filename}.png`);
        setIsExporting(false);
      }, 'image/png');
    } catch {
      setIsExporting(false);
    }
  }, []);

  return { exportJSON, exportSVG, exportPNG, isExporting };
}

function downloadBlob(blob: Blob, filename: string) {
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = filename;
  a.click();
  URL.revokeObjectURL(url);
}
