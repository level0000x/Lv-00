/**
 * @module utils/importExport
 * @description JSON 文件导入/导出工具函数。
 *              从 Layout.tsx 中提取，实现导入逻辑的关注点分离，
 *              提高可测试性和可维护性。
 *
 *              JSON file import/export utility functions.
 *              Extracted from Layout.tsx for better separation of concerns,
 *              testability, and maintainability.
 */

import { useAppStore } from '@/stores';
import type { Segment, Constraint } from '@/types';

// ================================================================
// 类型定义 / Type Definitions
// ================================================================

/**
 * JSON 导入文件中点的数据结构
 * @property id - 点的唯一标识符
 * @property x - X 坐标
 * @property y - Y 坐标
 */
interface ImportedPoint {
  id: number;
  x: number;
  y: number;
}

/**
 * JSON 导入文件中线段的数据结构
 */
type ImportedSegment = Segment;

/**
 * JSON 导入文件中约束的数据结构
 */
type ImportedConstraint = Constraint;

/**
 * JSON 导入文件中区域的数据结构
 * @property id - 区域的唯一标识符
 * @property points - 区域包含的点 ID 数组
 */
interface ImportedRegion {
  id: number;
  points?: number[];
}

/**
 * JSON 导入文件中视口状态的数据结构
 * @property scale - 缩放比例
 * @property offsetX - X 方向偏移
 * @property offsetY - Y 方向偏移
 */
interface ImportedViewport {
  scale?: number;
  offsetX?: number;
  offsetY?: number;
}

/**
 * 完整的 JSON 导入文件数据结构
 * @property points - 点数组（必填，不可为空）
 * @property segments - 线段数组（可选）
 * @property constraints - 约束数组（可选）
 * @property regions - 区域数组（可选）
 * @property viewport - 视口状态（可选）
 */
export interface ImportedGeometryData {
  points: ImportedPoint[];
  segments?: ImportedSegment[];
  constraints?: ImportedConstraint[];
  regions?: ImportedRegion[];
  viewport?: ImportedViewport;
}

// ================================================================
// 校验函数 / Validation Functions
// ================================================================

/**
 * 校验导入的 JSON 数据格式是否合法。
 *
 * 校验规则：
 * - 根对象必须存在且为 object
 * - 必须包含 points 字段，且为非空数组
 * - 每个 point 元素必须包含 id（number）、x（number）、y（number）三个字段
 *
 * @param data - 待校验的 JSON 解析结果
 * @returns 校验通过返回 true，失败抛出 Error
 * @throws {Error} 当数据格式不合法时抛出描述性错误
 */
export function validateImportedData(data: unknown): asserts data is ImportedGeometryData {
  // 校验根对象
  if (!data || typeof data !== 'object') {
    throw new Error('JSON 文件内容无效：根元素必须是对象');
  }

  const obj = data as Record<string, unknown>;

  // 核心校验：points 字段必须存在且为非空数组
  if (!Array.isArray(obj.points) || obj.points.length === 0) {
    throw new Error('JSON 文件格式无效：缺少 points 数组或数组为空');
  }

  // 校验 points 数组中每个元素的格式
  for (let i = 0; i < obj.points.length; i++) {
    const p = obj.points[i] as Record<string, unknown>;
    if (
      typeof p.id !== 'number' ||
      typeof p.x !== 'number' ||
      typeof p.y !== 'number'
    ) {
      throw new Error(
        `points[${i}] 格式无效：缺少 id、x 或 y 字段，或字段类型不正确`,
      );
    }
  }
}

// ================================================================
// 导入函数 / Import Functions
// ================================================================

/**
 * 将校验通过的 JSON 数据加载到应用 store 中。
 *
 * 加载内容包括：
 * - points（必填）：直接设置
 * - segments（可选）：仅在数据合法时加载
 * - constraints（可选）：仅在数据合法时加载
 * - regions（可选）：根据 point ID 重建 Region 对象
 * - viewport（可选）：恢复缩放和偏移状态
 *
 * @param data - 已通过校验的几何数据
 * @param fileName - 导入的文件名（用于日志记录）
 */
export function loadGeometryData(data: ImportedGeometryData, fileName: string): void {
  const store = useAppStore.getState();

  // 导入 points
  store.setPoints(data.points as any);

  // 导入 segments（可选）
  store.setSegments(
    Array.isArray(data.segments) ? data.segments : [],
  );

  // 导入 constraints（可选）
  store.setConstraints(
    Array.isArray(data.constraints) ? data.constraints : [],
  );

  // 导入 regions（可选，需要根据 point ID 重建 Region 对象）
  if (Array.isArray(data.regions)) {
    const importedPoints = data.points;
    const importedRegions = data.regions.map(
      (r: ImportedRegion) => ({
        id: r.id,
        points: (r.points || []).map((pid: number) => {
          const found = importedPoints.find(
            (p: ImportedPoint) => p.id === pid,
          );
          return found || { id: pid, x: 0, y: 0 };
        }),
      }),
    );
    store.setRegions(importedRegions as any);
  } else {
    store.setRegions([]);
  }

  // 导入 viewport 状态（可选）
  if (data.viewport && typeof data.viewport === 'object') {
    const viewport = data.viewport;
    store.setScale(typeof viewport.scale === 'number' ? viewport.scale : 1);
    store.setOffset(
      typeof viewport.offsetX === 'number' ? viewport.offsetX : 0,
      typeof viewport.offsetY === 'number' ? viewport.offsetY : 0,
    );
  }

  // 生成摘要信息并记录日志
  const summary = buildImportSummary(data);
  store.addToast('success', `导入成功: ${summary}`);
  store.appendLog(`导入成功: ${fileName} (${summary})`, 'info');
}

/**
 * 构建导入数据的摘要字符串。
 * 格式如: "点: 5, 线段: 3, 约束: 2, 区域: 1"
 *
 * @param data - 已导入的几何数据
 * @returns 格式化的摘要字符串
 */
export function buildImportSummary(data: ImportedGeometryData): string {
  const parts = [
    `点: ${data.points.length}`,
    data.segments?.length ? `线段: ${data.segments.length}` : null,
    data.constraints?.length ? `约束: ${data.constraints.length}` : null,
    data.regions?.length ? `区域: ${data.regions.length}` : null,
  ];
  return parts.filter(Boolean).join(', ');
}

/**
 * 处理 JSON 文件导入的完整流程。
 * 包含文件读取、JSON 解析、数据校验和 store 加载。
 *
 * @param file - 用户选择的 JSON 文件
 */
export function importJsonFile(file: File): void {
  const reader = new FileReader();

  reader.onload = (event) => {
    try {
      const text = event.target?.result as string;
      const data = JSON.parse(text);

      // 校验数据格式（校验失败会抛出 Error）
      validateImportedData(data);

      // 加载数据到 store
      loadGeometryData(data, file.name);
    } catch (err) {
      const errorMsg = `导入失败: ${(err as Error).message}`;
      useAppStore.getState().addToast('error', errorMsg);
      useAppStore.getState().appendLog(errorMsg, 'error');
    }
  };

  reader.onerror = () => {
    const errorMsg = `导入失败: 无法读取文件 "${file.name}"`;
    useAppStore.getState().addToast('error', errorMsg);
    useAppStore.getState().appendLog(errorMsg, 'error');
  };

  reader.readAsText(file);
}
