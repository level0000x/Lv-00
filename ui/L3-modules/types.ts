// L3-Modules types — 从协议层重导出（单一事实来源）

export type {
  DrawCmd, DrawCmdType, LineStyle,
  TableRow, TreeNode, TreeNodeStatus,
  TopoPort, TopoBlock, TopoEdge, TopoGraph,
  TerminalLine, CompletionItem,
  ProofStep, ProofStepKind, ProofNavigator,
  EngineStatus,
  TrustColor,
  KernelBridge,
} from '../L5-core/protocol';

export { trustColorToHex, trustColorToCSS, createMockBridge } from '../L5-core/protocol';

export interface CanvasEventHandler {
  onMouseDown?: (screenX: number, screenY: number, button: number) => void;
  onMouseMove?: (screenX: number, screenY: number) => void;
  onMouseUp?: (screenX: number, screenY: number, button: number) => void;
  onWheel?: (delta: number) => void;
}
