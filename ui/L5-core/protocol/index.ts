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
  let _cmdSeq = 0;

  /* ---------- helper: zero‑value DrawCmd ---------- */
  const emptyCmd = (): DrawCmd => ({
    type: 'LINE', x1: 0, y1: 0, x2: 0, y2: 0,
    radius: 0, text: '', colorRGBA: 0, trustColor: 'GREEN',
    lineWidth: 2, style: 'SOLID',
  });

  /* ---------- point coordinates (scene) ---------- */
  const pts: Record<string, { x: number; y: number }> = {
    A: { x: 200, y: 120 },
    B: { x: 420, y: 350 },
    C: { x: 100, y: 380 },
    D: { x: 480, y: 100 },
    E: { x: 300, y: 230 },
    F: { x: 560, y: 260 },
  };

  /* ---------- compute RGBA from TrustColor ---------- */
  const rgba = (tc: TrustColor) => TRUST_COLOR_RGBA[tc];

  /* ---------- build draw commands ---------- */
  function buildDrawCommands(): DrawCmd[] {
    const cmds: DrawCmd[] = [];

    // Helper to make a point
    const mkPt = (name: string, tc: TrustColor, r = 5) => {
      const p = pts[name];
      const c = emptyCmd();
      c.type = 'POINT'; c.x1 = p.x; c.y1 = p.y;
      c.radius = r; c.trustColor = tc; c.colorRGBA = rgba(tc);
      c.text = name; cmds.push(c);
    };

    // Helper to make a segment
    const mkSeg = (from: string, to: string, tc: TrustColor, style: LineStyle = 'SOLID') => {
      const a = pts[from], b = pts[to];
      const c = emptyCmd();
      c.type = 'LINE'; c.x1 = a.x; c.y1 = a.y; c.x2 = b.x; c.y2 = b.y;
      c.trustColor = tc; c.colorRGBA = rgba(tc); c.lineWidth = 2; c.style = style;
      cmds.push(c);
    };

    // Helper to make a text label
    const mkLabel = (name: string, tc: TrustColor) => {
      const p = pts[name];
      const c = emptyCmd();
      c.type = 'TEXT'; c.x1 = p.x + 10; c.y1 = p.y - 10;
      c.text = name; c.trustColor = tc; c.colorRGBA = rgba(tc);
      cmds.push(c);
    };

    // Helper to make a highlight circle around a point
    const mkHighlight = (name: string, tc: TrustColor, r = 14) => {
      const p = pts[name];
      const c = emptyCmd();
      c.type = 'HIGHLIGHT'; c.x1 = p.x; c.y1 = p.y;
      c.radius = r; c.trustColor = tc; c.colorRGBA = rgba(tc);
      c.lineWidth = 2; cmds.push(c);
    };

    // 1) Segments — triangle ABC
    mkSeg('A', 'B', 'BLUE');
    mkSeg('B', 'C', 'BLUE');
    mkSeg('C', 'A', 'BLUE');

    // 2) Segment BD (extended line)
    mkSeg('B', 'D', 'CYAN', 'DASHED');

    // 3) Segment EF
    mkSeg('E', 'F', 'ORANGE');

    // 4) Circle centered at A, radius through B
    const abDist = Math.sqrt((pts.B.x - pts.A.x) ** 2 + (pts.B.y - pts.A.y) ** 2);
    // Approximate circle with 36 segments
    const circleSteps = 36;
    for (let i = 0; i < circleSteps; i++) {
      const a1 = (2 * Math.PI * i) / circleSteps;
      const a2 = (2 * Math.PI * (i + 1)) / circleSteps;
      const c = emptyCmd();
      c.type = 'LINE';
      c.x1 = pts.A.x + Math.cos(a1) * abDist;
      c.y1 = pts.A.y + Math.sin(a1) * abDist;
      c.x2 = pts.A.x + Math.cos(a2) * abDist;
      c.y2 = pts.A.y + Math.sin(a2) * abDist;
      c.trustColor = 'YELLOW'; c.colorRGBA = rgba('YELLOW');
      c.lineWidth = 1.5; c.style = 'DOTTED';
      cmds.push(c);
    }

    // 5) Points
    mkPt('A', 'GREEN', 6);
    mkPt('B', 'GREEN', 6);
    mkPt('C', 'GREEN', 5);
    mkPt('D', 'CYAN', 5);
    mkPt('E', 'ORANGE', 5);
    mkPt('F', 'ORANGE', 5);

    // 6) Labels
    mkLabel('A', 'GREEN');
    mkLabel('B', 'GREEN');
    mkLabel('C', 'GREEN');
    mkLabel('D', 'CYAN');
    mkLabel('E', 'ORANGE');
    mkLabel('F', 'ORANGE');

    // 7) Highlights on selected points
    mkHighlight('A', 'YELLOW', 16);
    mkHighlight('E', 'YELLOW', 14);

    return cmds;
  }

  /* ---------- build table rows ---------- */
  function buildTableRows(): TableRow[] {
    const mkPtRow = (name: string, tc: TrustColor, blockId = 1) => {
      const p = pts[name];
      return {
        id: name.charCodeAt(0), name, nodeType: 'Point',
        coordX: p.x.toFixed(1), coordY: p.y.toFixed(1),
        constraintCount: 0, colorRGBA: rgba(tc), trustColor: tc,
        status: 'free', parentBlockId: blockId,
      };
    };
    const mkSegRow = (name: string, tc: TrustColor, blockId = 1) => ({
      id: 100 + name.charCodeAt(0), name, nodeType: 'Segment',
      coordX: '--', coordY: '--',
      constraintCount: 2, colorRGBA: rgba(tc), trustColor: tc,
      status: 'constrained', parentBlockId: blockId,
    });
    const mkCircRow = (name: string, tc: TrustColor, blockId = 1) => ({
      id: 200, name, nodeType: 'Circle',
      coordX: pts['A'].x.toFixed(1), coordY: pts['A'].y.toFixed(1),
      constraintCount: 1, colorRGBA: rgba(tc), trustColor: tc,
      status: 'constrained', parentBlockId: blockId,
    });
    return [
      mkPtRow('A', 'GREEN'), mkPtRow('B', 'GREEN'), mkPtRow('C', 'GREEN'),
      mkPtRow('D', 'CYAN', 2), mkPtRow('E', 'ORANGE', 3), mkPtRow('F', 'ORANGE', 3),
      mkSegRow('AB', 'BLUE'), mkSegRow('BC', 'BLUE'), mkSegRow('CA', 'BLUE'),
      mkSegRow('BD', 'CYAN', 2), mkSegRow('EF', 'ORANGE', 3),
      mkCircRow('circle_A(B)', 'YELLOW'),
    ];
  }

  /* ---------- proof tree ---------- */
  const proofTree: TreeNode = {
    id: 'root', label: 'Proof Root', trustColor: 'GREEN', status: 'root', nodeId: 0,
    children: [
      {
        id: 'constructions', label: 'Construction', trustColor: 'BLUE', status: 'proved', nodeId: 1,
        children: [
          { id: 'c1', label: 'Define triangle ABC', trustColor: 'BLUE', status: 'proved', nodeId: 10, children: [] },
          { id: 'c2', label: 'Point D on ray AB', trustColor: 'BLUE', status: 'proved', nodeId: 11, children: [] },
          { id: 'c3', label: 'Midpoint E of AB', trustColor: 'BLUE', status: 'proved', nodeId: 12, children: [] },
          { id: 'c4', label: 'Circle with center A', trustColor: 'BLUE', status: 'pending', nodeId: 13, children: [] },
        ],
      },
      {
        id: 'constraints', label: 'Constraints', trustColor: 'YELLOW', status: 'proved', nodeId: 2,
        children: [
          { id: 'k1', label: 'AB = AC (isosceles)', trustColor: 'GREEN', status: 'proved', nodeId: 20, children: [] },
          { id: 'k2', label: 'D lies on line AB', trustColor: 'GREEN', status: 'proved', nodeId: 21, children: [] },
          { id: 'k3', label: 'E is midpoint of AB', trustColor: 'YELLOW', status: 'pending', nodeId: 22, children: [] },
        ],
      },
      {
        id: 'theorems', label: 'Theorems', trustColor: 'PURPLE', status: 'failed', nodeId: 3,
        children: [
          { id: 't1', label: 'Triangle ABC is isosceles', trustColor: 'GREEN', status: 'proved', nodeId: 30, children: [] },
          { id: 't2', label: 'Angle bisector theorem', trustColor: 'ORANGE', status: 'failed', nodeId: 31, children: [] },
          { id: 't3', label: 'Perpendicular bisector', trustColor: 'YELLOW', status: 'pending', nodeId: 32, children: [] },
        ],
      },
    ],
  };

  /* ---------- topology ---------- */
  const topology: TopoGraph = {
    blocks: [
      { id: 1, name: 'Construct', inputs: [{ id: 1, name: 'params' }, { id: 2, name: 'config' }], outputs: [{ id: 3, name: 'points' }, { id: 4, name: 'segments' }], layoutX: 40, layoutY: 60 },
      { id: 2, name: 'Solve', inputs: [{ id: 5, name: 'graph' }], outputs: [{ id: 6, name: 'solution' }, { id: 7, name: 'status' }], layoutX: 280, layoutY: 60 },
      { id: 3, name: 'Verify', inputs: [{ id: 8, name: 'proof' }], outputs: [{ id: 9, name: 'result' }], layoutX: 520, layoutY: 60 },
      { id: 4, name: 'Export', inputs: [{ id: 10, name: 'data' }], outputs: [{ id: 11, name: 'file' }], layoutX: 280, layoutY: 200 },
    ],
    edges: [
      { fromBlock: 1, fromPort: 4, toBlock: 2, toPort: 5 },
      { fromBlock: 2, fromPort: 7, toBlock: 3, toPort: 8 },
      { fromBlock: 1, fromPort: 3, toBlock: 4, toPort: 10 },
    ],
  };

  /* ---------- proof navigator ---------- */
  const proofNav: ProofNavigator = {
    steps: [
      { stepId: 1, stepIndex: 0, kind: 'AXIOM', label: 'Axiom: Euclidean parallel', description: 'Given a line L and point P not on L, there exists exactly one line through P parallel to L.',
        color: 'GREEN', dependencyIds: [], isBacktrackPoint: false, isExplored: true, strategy: 'forward', nodeId: 50, constraintId: 0 },
      { stepId: 2, stepIndex: 1, kind: 'THEOREM', label: 'Thm: Isosceles triangle', description: 'If two sides of a triangle are equal, the angles opposite those sides are equal.',
        color: 'GREEN', dependencyIds: [1], isBacktrackPoint: false, isExplored: true, strategy: 'forward', nodeId: 51, constraintId: 1 },
      { stepId: 3, stepIndex: 2, kind: 'TACTIC', label: 'Tactic: Apply SSS', description: 'Side-side-side congruence criterion for triangles ABC and ABD.',
        color: 'BLUE', dependencyIds: [1, 2], isBacktrackPoint: true, isExplored: true, strategy: 'backward', nodeId: 52, constraintId: 2 },
      { stepId: 4, stepIndex: 3, kind: 'THEOREM', label: 'Thm: Angle bisector', description: 'The angle bisector divides the opposite side in the ratio of the adjacent sides.',
        color: 'YELLOW', dependencyIds: [2, 3], isBacktrackPoint: false, isExplored: true, strategy: 'forward', nodeId: 53, constraintId: 3 },
      { stepId: 5, stepIndex: 4, kind: 'TACTIC', label: 'Tactic: Midpoint construction', description: 'Construct midpoint E of segment AB using perpendicular bisector.',
        color: 'BLUE', dependencyIds: [3], isBacktrackPoint: false, isExplored: true, strategy: 'construct', nodeId: 54, constraintId: 4 },
      { stepId: 6, stepIndex: 5, kind: 'AXIOM', label: 'Axiom: Circle uniqueness', description: 'Given center A and radius AB, the circle is uniquely determined.',
        color: 'GREEN', dependencyIds: [], isBacktrackPoint: false, isExplored: true, strategy: 'forward', nodeId: 55, constraintId: 5 },
      { stepId: 7, stepIndex: 6, kind: 'THEOREM', label: 'Thm: Perpendicular bisector', description: 'The perpendicular bisector of a chord passes through the center of the circle.',
        color: 'ORANGE', dependencyIds: [4, 5, 6], isBacktrackPoint: true, isExplored: false, strategy: 'backward', nodeId: 56, constraintId: 6 },
      { stepId: 8, stepIndex: 7, kind: 'TACTIC', label: 'Tactic: Reductio ad absurdum', description: 'Assume the converse is false and derive a contradiction.',
        color: 'RED', dependencyIds: [4, 7], isBacktrackPoint: false, isExplored: false, strategy: 'backward', nodeId: 57, constraintId: 7 },
    ],
    stepCount: 6,
    totalSteps: 8,
    greenCount: 3,
    finalColor: 'ORANGE',
    strategyLabel: 'Forward + Backward chaining',
    nlSummary: 'We proved that triangle ABC is isosceles by applying the SSS congruence criterion. The angle bisector theorem was used to establish the relationship between segments BD and DC. A perpendicular bisector construction is in progress but has not yet been fully verified.',
    isComplete: false,
  };

  /* ---------- completions ---------- */
  const ALL_COMMANDS = [
    'add point', 'add segment', 'add circle', 'add line',
    'normalize', 'undo', 'redo', 'help', 'clear',
    'measure distance', 'measure angle', 'show grid', 'hide grid',
    'export svg', 'zoom fit',
  ];

  /* ---------- terminal execution ---------- */
  const HELP_TEXT =
    'Available commands:\n' +
    '  add point       — Add a new point\n' +
    '  add segment     — Add a segment between two points\n' +
    '  add circle      — Add a circle with center and radius\n' +
    '  add line        — Add an infinite line\n' +
    '  normalize       — Normalize all coordinates\n' +
    '  undo / redo     — Undo or redo last action\n' +
    '  measure distance— Measure distance between two points\n' +
    '  measure angle   — Measure an angle at a vertex\n' +
    '  show grid       — Show coordinate grid\n' +
    '  hide grid       — Hide coordinate grid\n' +
    '  export svg      — Export canvas to SVG\n' +
    '  zoom fit        — Fit view to all objects\n' +
    '  clear           — Clear terminal output\n' +
    '  help            — Show this help message';

  function execTerminal(command: string): TerminalResponse {
    _cmdSeq++;
    const cmd = command.trim().toLowerCase();
    if (cmd === 'help') {
      return { requestId: _cmdSeq, success: true, errorCode: 0, output: HELP_TEXT };
    }
    if (cmd === 'clear') {
      return { requestId: _cmdSeq, success: true, errorCode: 0, output: '' };
    }
    if (cmd === 'undo') {
      return { requestId: _cmdSeq, success: true, errorCode: 0, output: 'Undo: last action reverted.' };
    }
    if (cmd === 'redo') {
      return { requestId: _cmdSeq, success: true, errorCode: 0, output: 'Redo: re-applied undone action.' };
    }
    if (cmd === 'normalize') {
      return { requestId: _cmdSeq, success: true, errorCode: 0, output: 'Normalized all coordinates to rational form.' };
    }
    if (cmd.startsWith('add point')) {
      return { requestId: _cmdSeq, success: true, errorCode: 0, output: 'Point added. Click canvas to place.' };
    }
    if (cmd.startsWith('add segment')) {
      return { requestId: _cmdSeq, success: true, errorCode: 0, output: 'Segment tool activated. Select two endpoints.' };
    }
    if (cmd.startsWith('add circle')) {
      return { requestId: _cmdSeq, success: true, errorCode: 0, output: 'Circle tool activated. Click center then radius point.' };
    }
    if (cmd.startsWith('add line')) {
      return { requestId: _cmdSeq, success: true, errorCode: 0, output: 'Line tool activated. Select two points to define the line.' };
    }
    if (cmd.startsWith('measure distance')) {
      return { requestId: _cmdSeq, success: true, errorCode: 0, output: 'Measure distance mode. Select two points.' };
    }
    if (cmd.startsWith('measure angle')) {
      return { requestId: _cmdSeq, success: true, errorCode: 0, output: 'Measure angle mode. Select three points (vertex last).' };
    }
    if (cmd === 'show grid') {
      return { requestId: _cmdSeq, success: true, errorCode: 0, output: 'Grid enabled.' };
    }
    if (cmd === 'hide grid') {
      return { requestId: _cmdSeq, success: true, errorCode: 0, output: 'Grid disabled.' };
    }
    if (cmd === 'export svg') {
      return { requestId: _cmdSeq, success: true, errorCode: 0, output: 'Exported scene.svg (2.4 KB).' };
    }
    if (cmd === 'zoom fit') {
      return { requestId: _cmdSeq, success: true, errorCode: 0, output: 'Zoom adjusted to fit all objects.' };
    }
    return { requestId: _cmdSeq, success: false, errorCode: 1, output: `Unknown command: "${command}". Type "help" for available commands.` };
  }

  /* ---------- DSL text ---------- */
  const dslText =
    'point A (200, 120)\n' +
    'point B (420, 350)\n' +
    'point C (100, 380)\n' +
    'point D (480, 100)\n' +
    'point E (300, 230)\n' +
    'point F (560, 260)\n' +
    'segment AB\n' +
    'segment BC\n' +
    'segment CA\n' +
    'line BD\n' +
    'segment EF\n' +
    'circle center=A radius=B\n';

  /* ---------- build and return bridge ---------- */
  const cachedCmds = buildDrawCommands();
  const cachedRows = buildTableRows();

  return {
    getDrawCommands(_ox, _oy, _sc, _cw, _ch) {
      return {
        cmds: cachedCmds,
        viewportOffsetX: 0, viewportOffsetY: 0,
        viewportScale: 1, canvasWidth: 800, canvasHeight: 600,
      };
    },
    getTableRows() { return cachedRows; },
    getDslText() { return dslText; },
    getTree() { return proofTree; },
    getTopology() { return topology; },
    getProofNavigator() { return proofNav; },
    getEngineStatus() {
      return {
        nodeCount: 12, constraintCount: 8, proofCount: 3, funcBlockCount: 4,
        snapshotCount: 7, undoDepth: 5, redoDepth: 2, lastSolveTimeMs: 42,
        memoryUsageMb: 24.7, engineState: 'idle', backendInfo: 'wasm-rs',
      };
    },
    getCompletions(prefix: string) {
      return ALL_COMMANDS.filter(c => c.startsWith(prefix.toLowerCase()));
    },
    executeTerminal(command: string) { return execTerminal(command); },
    sendCanvasEvent(_e) {},
    sendTableSelect(_s) {},
    sendTreeAction(_a) {},
    sendBlockDrag(_d) {},
  };
}
