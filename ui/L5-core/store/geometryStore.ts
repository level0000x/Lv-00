import { create } from 'zustand';

export type GeoObjectType = 'point' | 'segment' | 'line' | 'ray' | 'circle' | 'arc' | 'polygon' | 'angle';

export interface GeoObject {
  id: string; type: GeoObjectType; label: string; color: string; visible: boolean;
  x?: number; y?: number;
  startId?: string; endId?: string;
  centerId?: string; radiusPointId?: string; radius?: number;
  vertexIds?: string[];
  length?: number; angle?: number; area?: number;
  createdAt: number;
}

export interface ConstraintEntry {
  id: string;
  type: 'distance' | 'angle' | 'horizontal' | 'vertical' | 'midpoint' | 'tangent' | 'parallel' | 'perpendicular';
  objIds: string[]; value?: number; label: string;
}

interface GeometryState {
  objects: GeoObject[]; constraints: ConstraintEntry[]; nextId: number; selectedIds: string[];
  addObject: (obj: Omit<GeoObject, 'id' | 'createdAt'>) => string;
  updateObject: (id: string, updates: Partial<GeoObject>) => void;
  removeObject: (id: string) => void;
  moveObject: (id: string, x: number, y: number) => void;
  selectObjects: (ids: string[]) => void;
  addConstraint: (c: Omit<ConstraintEntry, 'id'>) => void;
  removeConstraint: (id: string) => void;
  clearAll: () => void; loadDemoScene: () => void;
  getObjectById: (id: string) => GeoObject | undefined;
  getObjectsByType: (type: GeoObjectType) => GeoObject[];
  getPointById: (id: string) => { x: number; y: number } | undefined;
  computeDerived: () => void;
  getPointCoords: (id: string) => { x: number; y: number } | undefined;
  getDistance: (id1: string, id2: string) => number;
}

const TYPE_PREFIX: Record<GeoObjectType, string> = { point: 'pt', segment: 'seg', line: 'ln', ray: 'ray', circle: 'cir', arc: 'arc', polygon: 'poly', angle: 'ang' };
const DEFAULT_COLORS: Record<GeoObjectType, string> = { point: '#4fc3f7', segment: '#81c784', circle: '#ffd54f', line: '#90caf9', ray: '#ce93d8', arc: '#ffab91', polygon: '#a5d6a7', angle: '#80deea' };

let _labelCounter = 0;
function nextPointLabel(): string { const L = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'; const l = L[_labelCounter % 26] + (_labelCounter >= 26 ? String(Math.floor(_labelCounter / 26)) : ''); _labelCounter++; return l; }
let _constraintId = 1;

export const useGeometryStore = create<GeometryState>((set, get) => ({
  objects: [], constraints: [], nextId: 1, selectedIds: [],

  addObject: (obj) => {
    const st = get(); const prefix = TYPE_PREFIX[obj.type] ?? 'obj';
    const id = prefix + '_' + st.nextId;
    const color = obj.color ?? DEFAULT_COLORS[obj.type] ?? '#4fc3f7';
    const n: GeoObject = { ...obj, id, color, createdAt: Date.now() };
    if (!n.label) { if (obj.type === 'point') { n.label = nextPointLabel(); } else { n.label = prefix + '_' + st.nextId; } }
    set((s) => ({ objects: [...s.objects, n], nextId: s.nextId + 1 }));
    get().computeDerived(); return id;
  },

  updateObject: (id, updates) => { set((s) => ({ objects: s.objects.map((o) => o.id === id ? { ...o, ...updates } : o) })); get().computeDerived(); },

  removeObject: (id) => {
    set((s) => {
      const obj = s.objects.find((o) => o.id === id); const rm = new Set([id]);
      if (obj?.type === 'point') { for (const o of s.objects) { if (o.id !== id && (o.startId === id || o.endId === id || o.centerId === id || o.radiusPointId === id || (o.vertexIds && o.vertexIds.includes(id)))) rm.add(o.id); } }
      return { objects: s.objects.filter((o) => !rm.has(o.id)), constraints: s.constraints.filter((c) => !c.objIds.some((oid) => rm.has(oid))), selectedIds: s.selectedIds.filter((sid) => !rm.has(sid)) };
    }); get().computeDerived();
  },

  moveObject: (id, x, y) => { set((s) => ({ objects: s.objects.map((o) => o.id !== id ? o : { ...o, x, y }) })); get().computeDerived(); },
  selectObjects: (ids) => { set({ selectedIds: [...ids] }); },
  addConstraint: (c) => { set((s) => ({ constraints: [...s.constraints, { ...c, id: 'con_' + _constraintId++ }] })); },
  removeConstraint: (id) => { set((s) => ({ constraints: s.constraints.filter((c) => c.id !== id) })); },
  clearAll: () => { _labelCounter = 0; set({ objects: [], constraints: [], selectedIds: [] }); },

  loadDemoScene: () => {
    _labelCounter = 0; const co: GeoObject[] = []; let nid = 1;
    const pts = [{l:'A',x:100,y:200},{l:'B',x:400,y:150},{l:'C',x:250,y:350},{l:'D',x:500,y:300},{l:'E',x:150,y:400},{l:'F',x:600,y:100}];
    for (const p of pts) { co.push({id:'pt_'+nid,type:'point',label:p.l,color:DEFAULT_COLORS.point,visible:true,x:p.x,y:p.y,createdAt:Date.now()+nid}); nid++; }
    const segs = [['AB','pt_1','pt_2'],['BC','pt_2','pt_3'],['CD','pt_3','pt_4'],['EA','pt_5','pt_1']];
    for (const [lb,s,e] of segs) { co.push({id:'seg_'+nid,type:'segment',label:lb,color:DEFAULT_COLORS.segment,visible:true,startId:s,endId:e,createdAt:Date.now()+nid}); nid++; }
    const rAB = Math.sqrt((400-100)**2+(150-200)**2);
    co.push({id:'cir_'+nid,type:'circle',label:'C1',color:DEFAULT_COLORS.circle,visible:true,centerId:'pt_1',radiusPointId:'pt_2',radius:rAB,createdAt:Date.now()+nid}); nid++;
    co.push({id:'pt_'+nid,type:'point',label:'M',color:DEFAULT_COLORS.point,visible:true,x:325,y:250,createdAt:Date.now()+nid}); nid++;
    set({objects:co,constraints:[],nextId:nid,selectedIds:[]}); _labelCounter=6;
  },

  getObjectById: (id) => get().objects.find((o) => o.id === id),
  getObjectsByType: (type) => get().objects.filter((o) => o.type === type),
  getPointById: (id) => { const o = get().objects.find((x) => x.id === id); if (o && o.type === 'point' && o.x != null && o.y != null) return {x:o.x,y:o.y}; return undefined; },

  computeDerived: () => {
    set((s) => ({
      objects: s.objects.map((o) => {
        const u = {...o};
        if (o.type === 'segment' && o.startId && o.endId) { const a=s.objects.find((x)=>x.id===o.startId),b=s.objects.find((x)=>x.id===o.endId); if(a&&b&&a.x!=null&&a.y!=null&&b.x!=null&&b.y!=null) u.length=Math.sqrt((b.x-a.x)**2+(b.y-a.y)**2); }
        if (o.type === 'circle' && o.centerId && o.radiusPointId) { const c=s.objects.find((x)=>x.id===o.centerId),r=s.objects.find((x)=>x.id===o.radiusPointId); if(c&&r&&c.x!=null&&c.y!=null&&r.x!=null&&r.y!=null) u.radius=Math.sqrt((r.x-c.x)**2+(r.y-c.y)**2); }
        if (o.type === 'line' && o.startId && o.endId) { const a=s.objects.find((x)=>x.id===o.startId),b=s.objects.find((x)=>x.id===o.endId); if(a&&b&&a.x!=null&&a.y!=null&&b.x!=null&&b.y!=null) u.length=Math.sqrt((b.x-a.x)**2+(b.y-a.y)**2); }
        if (o.type === 'polygon' && o.vertexIds && o.vertexIds.length >= 3) { let area=0; const vp=o.vertexIds.map((v)=>s.objects.find((x)=>x.id===v)).filter(Boolean); if(vp.length>=3&&vp.every((p)=>p!.x!=null&&p!.y!=null)){const n=vp.length;for(let i=0;i<n;i++){const j=(i+1)%n;area+=vp[i]!.x!*vp[j]!.y!-vp[j]!.x!*vp[i]!.y!;}u.area=Math.abs(area)/2;} }
        return u;
      })
    }));
  },

  getPointCoords: (id) => get().getPointById(id),
  getDistance: (id1, id2) => { const a=get().getPointById(id1),b=get().getPointById(id2); if(!a||!b)return 0; return Math.sqrt((b.x-a.x)**2+(b.y-a.y)**2); },
}));
