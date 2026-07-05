import React, { useState, useCallback } from 'react';
import { useGeometryStore, GeoObject } from '../../L5-core/store/geometryStore';

const TYPE_COLOR: Record<string, string> = { point: '#4fc3f7', segment: '#81c784', circle: '#ffd54f', line: '#90caf9' };
const TYPE_TAG: Record<string, string> = { point: 'pt', segment: 'seg', circle: 'cir', line: 'ln' };

const ExpressionList: React.FC<{ objects: GeoObject[]; onToggle: (id: string) => void; onDelete: (id: string) => void }> = ({ objects, onToggle, onDelete }) => (
  <div style={{ display: 'flex', flexDirection: 'column', gap: 2 }}>
    {objects.map((obj) => {
      const color = obj.color || TYPE_COLOR[obj.type] || '#4fc3f7';
      const tag = TYPE_TAG[obj.type] || obj.type;
      let detail = '';
      if (obj.type === 'point') detail = '(' + (obj.x?.toFixed(0) ?? '?') + ', ' + (obj.y?.toFixed(0) ?? '?') + ')';
      else if (obj.type === 'segment') detail = obj.length !== undefined ? 'L=' + obj.length.toFixed(1) : '';
      else if (obj.type === 'circle') detail = obj.radius !== undefined ? 'R=' + obj.radius.toFixed(1) : '';
      return (
        <div key={obj.id} style={{ display: 'flex', alignItems: 'center', gap: 6, padding: '4px 6px', borderRadius: 4, opacity: obj.visible ? 1 : 0.5 }}>
          <button onClick={() => onToggle(obj.id)} style={{ background: 'none', border: 'none', cursor: 'pointer', color: obj.visible ? color : '#555', fontSize: 12, width: 18, height: 18, display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
            {obj.visible ? '\u25CF' : '\u25CB'}
          </button>
          <span style={{ width: 8, height: 8, borderRadius: '50%', background: color, flexShrink: 0 }} />
          <span style={{ flex: 1, color: 'var(--color-text-primary)', fontFamily: 'var(--font-mono)', fontSize: 12 }}>
            <span style={{ color: 'var(--color-text-muted)', fontSize: 10 }}>[{tag}]</span> {obj.label} <span style={{ color: 'var(--color-text-muted)', fontSize: 11 }}>{detail}</span>
          </span>
          <button onClick={() => onDelete(obj.id)} style={{ background: 'none', border: 'none', cursor: 'pointer', color: '#555', fontSize: 12, padding: 0 }}>\u00D7</button>
        </div>
      );
    })}
  </div>
);

export const FormulaPanel: React.FC = () => {
  const objects = useGeometryStore((s) => s.objects);
  const updateObject = useGeometryStore((s) => s.updateObject);
  const removeObject = useGeometryStore((s) => s.removeObject);
  const addObject = useGeometryStore((s) => s.addObject);
  const [input, setInput] = useState('');
  const [error, setError] = useState<string | null>(null);
  const [lastResult, setLastResult] = useState<string | null>(null);

  const findPointByLabel = (label: string) => objects.find((o) => o.type === 'point' && o.label.toLowerCase() === label.toLowerCase());

  const evaluate = useCallback(() => {
    if (!input.trim()) return;
    setError(null); setLastResult(null);
    const cmds = input.split('\n').map(l => l.trim()).filter(Boolean);
    const results: string[] = [];
    for (const line of cmds) {
      const pm = line.match(/^point\s+(\w+)\s+at\s*\(\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*\)$/i);
      if (pm) { addObject({ type: 'point', label: pm[1], x: parseFloat(pm[2]), y: parseFloat(pm[3]), visible: true }); results.push('point ' + pm[1]); continue; }
      const sm = line.match(/^segment\s+(\w+)\s+between\s+(\w+)\s+and\s+(\w+)$/i);
      if (sm) { const a=findPointByLabel(sm[2]),b=findPointByLabel(sm[3]); if(!a||!b){setError('Point not found: '+(a?sm[3]:sm[2]));return;} addObject({type:'segment',label:sm[1],startId:a.id,endId:b.id,visible:true}); results.push('segment '+sm[1]); continue; }
      const cm = line.match(/^circle\s+(\w+)\s+center\s+(\w+)\s+through\s+(\w+)$/i);
      if (cm) { const c=findPointByLabel(cm[2]),r=findPointByLabel(cm[3]); if(!c||!r){setError('Point not found');return;} const d=useGeometryStore.getState().getDistance(c.id,r.id); addObject({type:'circle',label:cm[1],centerId:c.id,radiusPointId:r.id,radius:d,visible:true}); results.push('circle '+cm[1]+' r='+d.toFixed(1)); continue; }
      const mm = line.match(/^midpoint\s+(\w+)\s*,\s*(\w+)$/i);
      if (mm) { const a=findPointByLabel(mm[1]),b=findPointByLabel(mm[2]); if(!a||!b){setError('Point not found');return;} const mx=((a.x??0)+(b.x??0))/2,my=((a.y??0)+(b.y??0))/2; addObject({type:'point',label:'M('+a.label+b.label+')',x:mx,y:my,visible:true}); results.push('midpoint'); continue; }
      setError('Syntax error: ' + line); return;
    }
    setLastResult(results.join('\n')); setInput('');
  }, [input, objects, addObject, findPointByLabel]);

  const toggleVisibility = (id: string) => { const obj = objects.find((o) => o.id === id); if (obj) updateObject(id, { visible: !obj.visible }); };
  const deleteObj = (id: string) => removeObject(id);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', gap: 8 }}>
      <div style={{ display: 'flex', gap: 8 }}>
        <span style={{ fontSize: 12, color: 'var(--color-text-secondary)' }}>Expressions ({objects.length})</span>
      </div>
      <div style={{ flex: 1, overflowY: 'auto', borderBottom: '1px solid var(--color-border-secondary)' }}>
        {objects.length > 0 ? <ExpressionList objects={objects} onToggle={toggleVisibility} onDelete={deleteObj} />
          : <div style={{ color: 'var(--color-text-muted)', fontSize: 12, padding: '8px 0' }}>No objects</div>}
      </div>
      {lastResult && <div style={{ padding: '4px 8px', background: 'rgba(0,200,83,0.08)', borderRadius: 4, fontSize: 11, color: 'var(--color-success)', fontFamily: 'var(--font-mono)', whiteSpace: 'pre-wrap' }}>{lastResult}</div>}
      {error && <div style={{ padding: '4px 8px', background: 'rgba(248,81,73,0.08)', borderRadius: 4, fontSize: 11, color: 'var(--color-danger)', fontFamily: 'var(--font-mono)' }}>{error}</div>}
      <div style={{ display: 'flex', flexDirection: 'column', gap: 6 }}>
        <span style={{ fontSize: 11, color: 'var(--color-text-secondary)' }}>Command Input (Lv-00 DSL)</span>
        <textarea className="formula-editor" value={input} onChange={(e) => { setInput(e.target.value); setError(null); }}
          placeholder={'point A at (100, 200);\nsegment AB between A and B;\ncircle C1 center A through B;'}
          spellCheck={false} rows={4} />
        <button className="btn btn-primary" onClick={evaluate}>Evaluate</button>
      </div>
    </div>
  );
};
