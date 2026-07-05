import React, { useRef, useState, useEffect, useCallback } from 'react';
import { useGeometryStore } from '../../L5-core/store/geometryStore';

export const TerminalView: React.FC = () => {
  const [lines, setLines] = useState<{ id: number; text: string; color: string }[]>([
    { id: 0, text: 'Lv-00 Geometry Terminal v3.4.0', color: 'var(--color-text-secondary)' },
    { id: 1, text: "Type 'help' for commands", color: 'var(--color-text-muted)' },
    { id: 2, text: '', color: 'var(--color-text-primary)' },
  ]);
  const [input, setInput] = useState('');
  const outputRef = useRef<HTMLDivElement>(null);
  const [history, setHistory] = useState<string[]>([]);
  const [historyIdx, setHistoryIdx] = useState(-1);

  useEffect(() => { if (outputRef.current) outputRef.current.scrollTop = outputRef.current.scrollHeight; }, [lines]);

  let _lid = 10;
  const addLine = (text: string, color?: string) => {
    setLines((prev) => [...prev, { id: _lid++, text, color: color ?? 'var(--color-text-primary)' }]);
  };

  const findPointByLabel = (label: string) => {
    return useGeometryStore.getState().objects.find((o) => o.type === 'point' && o.label.toLowerCase() === label.toLowerCase());
  };

  const exec = useCallback((raw: string) => {
    const cmd = raw.trim();
    if (!cmd) return;
    addLine('> ' + cmd, 'var(--color-accent)');
    setHistory((prev) => [cmd, ...prev.filter((h) => h !== cmd).slice(0, 99)]);
    setHistoryIdx(-1);
    const lower = cmd.toLowerCase();

    if (lower === 'help') {
      ['','Available commands:','  add point <name> at (<x>, <y>)','  add segment <name> between <A> and <B>','  add circle <name> center <A> radius <B>','  delete <label>','  move <label> to (<x>, <y>)','  select <label>, <label>','  list | clear | demo | stats | help',''].forEach(l => addLine(l, 'var(--color-text-secondary)'));
      return;
    }
    if (lower === 'clear' || lower === 'cls') { setLines([{ id: _lid++, text: 'Cleared', color: 'var(--color-text-muted)' }]); return; }
    if (lower === 'demo') { useGeometryStore.getState().loadDemoScene(); addLine('Demo scene loaded', 'var(--color-success)'); return; }
    if (lower === 'stats') {
      const s = useGeometryStore.getState();
      const c: Record<string,number> = {};
      s.objects.forEach(o => c[o.type] = (c[o.type]||0)+1);
      addLine(s.objects.length + ' objects, ' + s.constraints.length + ' constraints', 'var(--color-text-secondary)');
      Object.entries(c).forEach(([t,n]) => addLine('  ' + t + ': ' + n, 'var(--color-text-secondary)'));
      return;
    }
    if (lower === 'list') {
      const s = useGeometryStore.getState();
      if (!s.objects.length) { addLine('(empty)', 'var(--color-text-muted)'); return; }
      s.objects.forEach(o => {
        let d = '[' + o.type + '] ' + o.label + ' (' + o.id + ')';
        if (o.type === 'point') d += ' (' + o.x + ', ' + o.y + ')';
        if (o.type === 'segment') d += ' L=' + (o.length?.toFixed(1) ?? '?');
        if (o.type === 'circle') d += ' R=' + (o.radius?.toFixed(1) ?? '?');
        addLine('  ' + d, 'var(--color-text-primary)');
      });
      return;
    }

    const m1 = cmd.match(/^add\s+point\s+(\w+)\s+at\s*\(\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*\)$/i);
    if (m1) { const id = useGeometryStore.getState().addObject({type:'point',label:m1[1],x:parseFloat(m1[2]),y:parseFloat(m1[3]),visible:true}); addLine('Point ' + m1[1] + ' created [' + id + ']', 'var(--color-success)'); return; }

    const m2 = cmd.match(/^add\s+segment\s+(\w+)\s+between\s+(\w+)\s+and\s+(\w+)$/i);
    if (m2) { const a=findPointByLabel(m2[2]),b=findPointByLabel(m2[3]); if(!a||!b){addLine('Point not found','var(--color-danger)');return;} const id=useGeometryStore.getState().addObject({type:'segment',label:m2[1],startId:a.id,endId:b.id,visible:true}); addLine('Segment '+m2[1]+' created ['+id+']','var(--color-success)'); return; }

    const m3 = cmd.match(/^add\s+circle\s+(\w+)\s+center\s+(\w+)\s+radius\s+(\w+)$/i);
    if (m3) { const c=findPointByLabel(m3[2]),r=findPointByLabel(m3[3]); if(!c||!r){addLine('Point not found','var(--color-danger)');return;} const d=useGeometryStore.getState().getDistance(c.id,r.id); const id=useGeometryStore.getState().addObject({type:'circle',label:m3[1],centerId:c.id,radiusPointId:r.id,radius:d,visible:true}); addLine('Circle '+m3[1]+' r='+d.toFixed(1)+' ['+id+']','var(--color-success)'); return; }

    const m4 = cmd.match(/^midpoint\s+(\w+)\s*,\s*(\w+)$/i);
    if (m4) { const a=findPointByLabel(m4[1]),b=findPointByLabel(m4[2]); if(!a||!b){addLine('Point not found','var(--color-danger)');return;} const mx=((a.x??0)+(b.x??0))/2,my=((a.y??0)+(b.y??0))/2; const id=useGeometryStore.getState().addObject({type:'point',label:'M('+a.label+b.label+')',x:mx,y:my,visible:true}); addLine('Midpoint ('+mx.toFixed(1)+','+my.toFixed(1)+') ['+id+']','var(--color-success)'); return; }

    const m5 = cmd.match(/^delete\s+(\w+)$/i);
    if (m5) { const s=useGeometryStore.getState(); const o=s.objects.find(x=>x.label.toLowerCase()===m5[1].toLowerCase()); if(!o){addLine('Not found: '+m5[1],'var(--color-danger)');return;} s.removeObject(o.id); addLine('Deleted '+o.label+' ('+o.type+')','var(--color-warning)'); return; }

    const m6 = cmd.match(/^move\s+(\w+)\s+to\s*\(\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*\)$/i);
    if (m6) { const pt=findPointByLabel(m6[1]); if(!pt){addLine('Point not found','var(--color-danger)');return;} useGeometryStore.getState().moveObject(pt.id,parseFloat(m6[2]),parseFloat(m6[3])); addLine('Moved '+pt.label+' to ('+m6[2]+','+m6[3]+')','var(--color-success)'); return; }

    const m7 = cmd.match(/^select\s+(.+)$/i);
    if (m7) { const labels=m7[1].split(',').map(s=>s.trim()).filter(Boolean); const s=useGeometryStore.getState(); const ids=labels.map(l=>s.objects.find(o=>o.label.toLowerCase()===l.toLowerCase())).filter(Boolean).map(o=>o!.id); useGeometryStore.getState().selectObjects(ids); addLine('Selected '+ids.length+' object(s)','var(--color-success)'); return; }

    addLine("Unknown command: '" + cmd + "'. Type 'help'.", 'var(--color-danger)');
  }, []);

  const handleKeyDown = useCallback((e: React.KeyboardEvent<HTMLInputElement>) => {
    if (e.key === 'Enter') { exec(input); setInput(''); }
    else if (e.key === 'ArrowUp') { e.preventDefault(); if (history.length > 0) { const i = historyIdx === -1 ? 0 : Math.min(historyIdx + 1, history.length - 1); setHistoryIdx(i); setInput(history[i]); } }
    else if (e.key === 'ArrowDown') { e.preventDefault(); if (historyIdx > 0) { setHistoryIdx(historyIdx - 1); setInput(history[historyIdx - 1]); } else if (historyIdx === 0) { setHistoryIdx(-1); setInput(''); } }
  }, [input, history, historyIdx, exec]);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
      <div style={{ display: 'flex', gap: 8, padding: '4px 8px', borderBottom: '1px solid var(--color-border-secondary)', flexShrink: 0 }}>
        <button className="btn btn-small btn-ghost" onClick={() => setLines([])}>Clear</button>
        <span style={{ fontSize: 10, color: 'var(--color-text-muted)', fontFamily: 'var(--font-mono)' }}>Up/Down=History</span>
      </div>
      <div ref={outputRef} className="terminal-output" style={{ flex: 1 }}>
        {lines.map((l) => <div key={l.id} style={{ color: l.color, whiteSpace: 'pre-wrap' }}>{l.text}</div>)}
      </div>
      <div className="terminal-input-line">
        <span className="terminal-prompt">&gt;</span>
        <input ref={undefined} value={input} onChange={(e) => setInput(e.target.value)}
          onKeyDown={handleKeyDown} spellCheck={false} autoFocus
          placeholder="Type help..."
          style={{ flex: 1, background: 'transparent', border: 'none', color: 'var(--color-text-primary)', fontFamily: 'var(--font-mono)', fontSize: 13, outline: 'none' }} />
      </div>
    </div>
  );
};
