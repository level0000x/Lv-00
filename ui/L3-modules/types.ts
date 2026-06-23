// L3-Modules types — 纯展示数据类型，零几何逻辑

export type DrawCmdType = 'LINE' | 'POINT' | 'TEXT' | 'HIGHLIGHT';
export type CmdLineStyle = 'SOLID' | 'DASHED' | 'DOTTED';

export interface DrawCmd {
  type: DrawCmdType;
  x1: number;
  y1: number;
  x2?: number;
  y2?: number;
  radius?: number;
  text?: string;
  color?: string;
  lineWidth?: number;
  style?: CmdLineStyle;
}

export interface TableRow {
  id: number;
  name: string;
  type: string;
  coordX: string;
  coordY: string;
  constraintCount: number;
  color?: string;
  status?: string;
}

export interface TreeNode {
  id: string;
  label: string;
  color: string;
  status: 'proved' | 'pending' | 'failed' | 'assumed' | 'root';
  children: TreeNode[];
  nodeId?: number;
}

export interface TermOutputLine {
  id: number;
  text: string;
  color: string;
}

export interface TopoBlock {
  id: number;
  name: string;
  inputs: { id: number; name: string }[];
  outputs: { id: number; name: string }[];
  layoutX: number;
  layoutY: number;
}

export interface TopoEdge {
  fromBlock: number;
  fromPort: number;
  toBlock: number;
  toPort: number;
}

export interface TopoGraph {
  blocks: TopoBlock[];
  edges: TopoEdge[];
}

export interface CanvasViewport {
  offsetX: number;
  offsetY: number;
  scale: number;
}

export type CanvasEventType = 'mousedown' | 'mousemove' | 'mouseup' | 'wheel';

export interface CanvasEvent {
  type: CanvasEventType;
  screenX: number;
  screenY: number;
  button?: number;
  shiftKey?: boolean;
  delta?: number;
}
