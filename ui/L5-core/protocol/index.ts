// L5-Core / protocol.ts — 内核通信协议（唯一跨边界类型）
// 与 core/include/lv00/lv00_protocol.h 结构一一对应

/* ================================================================
 * 颜色系统
 * ================================================================ */

export type TrustColor =
  | 'GREEN' | 'BLUE' | 'BLUE_RANGE'
  | 'YELLOW' | 'AMBER' | 'LIGHT_ORANGE' | 'ORANGE' | 'DARK_ORANGE'
  | 'RED' | 'GREY' | 'PURPLE' | 'CYAN';

export const TRUST_COLOR_RGBA: Record<TrustColor, number> = {
  GREEN:        0xFF3fb950,
  BLUE:         0xFF58a6ff,
  BLUE_RANGE:   0xFF90caf9,
  YELLOW:       0xFFd29922,
  AMBER:        0xFFffc107,
  LIGHT_ORANGE: 0xFFff9800,
  ORANGE:       0xFFf0883e,
  DARK_ORANGE:  0xFFdb6d28,
  RED:          0xFFf85149,
  GREY:         0xFF8b949e,
  PURPLE:       0xFFbc8cff,
  CYAN:         0xFF39c5cf,
};

export function trustColorToHex(c: TrustColor): string {
  const v = TRUST_COLOR_RGBA[c];
  return '#' + ((v >> 8) & 0xFFFFFF).toString(16).padStart(6, '0');
}

export function trustColorToCSS(c: TrustColor): string {
  const v = TRUST_COLOR_RGBA[c];
  const r = (v >> 16) & 0xFF, g = (v >> 8) & 0xFF, b = v & 0xFF;
  return `rgb(${r},${g},${b})`;
}

/* ================================================================
 * DisplayData：内核→UI
 * ================================================================ */

export type DrawCmdType = 'LINE' | 'POINT' | 'TEXT' | 'HIGHLIGHT';
export type LineStyle = 'SOLID' | 'DASHED' | 'DOTTED';

export interface DrawCmd {
  type: DrawCmdType;
  x1: number; y1: number;
  x2: number; y2: number;
  radius: number;
  text: string;
  colorRGBA: number;
  trustColor: TrustColor;
  lineWidth: number;
  style: LineStyle;
}

export interface DrawCmdList {
  cmds: DrawCmd[];
  viewportOffsetX: number;
  viewportOffsetY: number;
  viewportScale: number;
  canvasWidth: number;
  canvasHeight: number;
}

export interface TableRow {
  id: number;
  name: string;
  nodeType: string;
  coordX: string;
  coordY: string;
  constraintCount: number;
  colorRGBA: number;
  trustColor: TrustColor;
  status: string;
  parentBlockId: number;
}

export type TreeNodeStatus = 'proved' | 'pending' | 'failed' | 'assumed' | 'root';

export interface TreeNode {
  id: string;
  label: string;
  trustColor: TrustColor;
  status: TreeNodeStatus;
  nodeId: number;
  children: TreeNode[];
}

export interface TopoPort {
  id: number;
  name: string;
}

export interface TopoBlock {
  id: number;
  name: string;
  inputs: TopoPort[];
  outputs: TopoPort[];
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

export interface TerminalLine {
  id: number;
  text: string;
  color: TrustColor;
}

export interface CompletionItem {
  text: string;
}

export type ProofStepKind =
  | 'AXIOM' | 'THEOREM' | 'LEMMA' | 'COROLLARY'
  | 'TACTIC' | 'MERGE' | 'ROOT';

export interface ProofStep {
  stepId: number;
  stepIndex: number;
  kind: ProofStepKind;
  label: string;
  description: string;
  color: TrustColor;
  dependencyIds: number[];
  isBacktrackPoint: boolean;
  isExplored: boolean;
  strategy: string;
  nodeId: number;
  constraintId: number;
}

export interface ProofNavigator {
  steps: ProofStep[];
  stepCount: number;
  totalSteps: number;
  greenCount: number;
  finalColor: TrustColor;
  strategyLabel: string;
  nlSummary: string;
  isComplete: boolean;
}

export interface EngineStatus {
  nodeCount: number;
  constraintCount: number;
  proofCount: number;
  funcBlockCount: number;
  snapshotCount: number;
  undoDepth: number;
  redoDepth: number;
  lastSolveTimeMs: number;
  memoryUsageMb: number;
  engineState: string;
  backendInfo: string;
}

/* ================================================================
 * UserAction：UI→内核
 * ================================================================ */

export type CanvasEventType = 'MOUSE_DOWN' | 'MOUSE_MOVE' | 'MOUSE_UP' | 'MOUSE_WHEEL' | 'KEY_DOWN';

export interface CanvasEvent {
  type: CanvasEventType;
  screenX: number;
  screenY: number;
  button: number;
  shiftDown: boolean;
  ctrlDown: boolean;
  wheelDelta: number;
  key: string;
}

export interface TerminalCmd {
  command: string;
}

export interface TerminalResponse {
  requestId: number;
  success: boolean;
  errorCode: number;
  output: string;
}

export interface TableSelect {
  rowId: number;
  ctrlDown: boolean;
}

export type TreeActionType = 'TOGGLE' | 'SELECT';

export interface TreeAction {
  type: TreeActionType;
  nodeId: string;
  nodeIdInt: number;
}

export interface BlockDrag {
  blockId: number;
  newX: number;
  newY: number;
}

/* ================================================================
 * 协议 Shell：抽象通信接口
 * ================================================================ */

export interface KernelBridge {
  /* DisplayData */
  getDrawCommands(offsetX: number, offsetY: number, scale: number,
                  canvasW: number, canvasH: number): DrawCmdList;
  getTableRows(): TableRow[];
  getDslText(): string;
  getTree(): TreeNode | null;
  getTopology(): TopoGraph;
  getProofNavigator(): ProofNavigator;
  getEngineStatus(): EngineStatus;
  getCompletions(prefix: string): string[];
  executeTerminal(command: string): TerminalResponse;

  /* UserAction */
  sendCanvasEvent(event: CanvasEvent): void;
  sendTableSelect(select: TableSelect): void;
  sendTreeAction(action: TreeAction): void;
  sendBlockDrag(drag: BlockDrag): void;
}

/* ================================================================
 * Mock Bridge：测试/开发用，不依赖真实内核
 * ================================================================ */

export function createMockBridge(): KernelBridge {
  let _text = 'point A at (100, 200)\npoint B at (400, 150)\nsegment AB between A and B';
  let _rows: TableRow[] = [];
  let _cmdSeq = 0;

  return {
    getDrawCommands(_ox, _oy, _sc, _cw, _ch) {
      return { cmds: [], viewportOffsetX: 0, viewportOffsetY: 0, viewportScale: 1, canvasWidth: 800, canvasHeight: 600 };
    },
    getTableRows() { return _rows; },
    getDslText() { return _text; },
    getTree() {
      return { id: 'root', label: 'Proof Tree', trustColor: 'GREEN', status: 'root', nodeId: 0, children: [] };
    },
    getTopology() { return { blocks: [], edges: [] }; },
    getProofNavigator() {
      return { steps: [], stepCount: 0, totalSteps: 0, greenCount: 0, finalColor: 'GREY', strategyLabel: '', nlSummary: '', isComplete: false };
    },
    getEngineStatus() {
      return { nodeCount: 0, constraintCount: 0, proofCount: 0, funcBlockCount: 0, snapshotCount: 0, undoDepth: 0, redoDepth: 0, lastSolveTimeMs: 0, memoryUsageMb: 0, engineState: 'idle', backendInfo: 'mock' };
    },
    getCompletions(prefix: string) {
      const builtins = ['add point', 'add segment', 'normalize', 'undo', 'redo', 'help', 'clear'];
      return builtins.filter(c => c.startsWith(prefix.toLowerCase()));
    },
    executeTerminal(command: string) {
      _cmdSeq++;
      return { requestId: _cmdSeq, success: true, errorCode: 0, output: `ok: ${command}` };
    },
    sendCanvasEvent(_e) {},
    sendTableSelect(_s) {},
    sendTreeAction(_a) {},
    sendBlockDrag(_d) {},
  };
}
