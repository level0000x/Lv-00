/**
 * @module components/common/ContextMenu
 * @description 右键上下文菜单组件 / Right-click context menu component.
 *              从 Zustand store 中读取右键菜单状态并渲染。
 *              支持点、线段、空白空间的不同操作。
 */

import React, { useEffect, useCallback } from 'react';
import { useAppStore } from '@/stores';
import { generateUniqueId } from '@/utils/idGenerator';
import { MERGE_NEAREST_PIXEL_THRESHOLD, MIN_SEGMENT_LENGTH_PX } from '@/utils/constants';
import { parseAndExecuteFormula } from '@/utils/formulaParser';

/**
 * ContextMenu - 右键上下文菜单 / Right-click context menu
 *
 * 在鼠标位置渲染浮动的操作菜单。
 * 点击菜单外或按 Escape 键自动关闭。
 * 根据目标类型（点/线段/空白）执行不同的操作。
 *
 * Renders a floating action menu at the mouse position.
 * Automatically closes when clicking outside or pressing Escape.
 * Executes different actions based on target type (point/segment/empty).
 */
const ContextMenu: React.FC = () => {
  const contextMenu = useAppStore((s) => s.contextMenu);
  const hideContextMenu = useAppStore((s) => s.hideContextMenu);
  const addToast = useAppStore((s) => s.addToast);

  const handleAction = useCallback(
    (actionId: string) => {
      if (!contextMenu) return;

      const state = useAppStore.getState();
      const targetId = contextMenu.target;

      switch (actionId) {
        // ---- 点操作 / Point actions ----
        case 'delete-point': {
          if (targetId !== undefined) {
            state.saveUndoState();
            state.removePoint(targetId);
            addToast('success', `已删除点 n${targetId} / Deleted point n${targetId}`);
          }
          break;
        }
        case 'point-properties': {
          if (targetId !== undefined) {
            const pt = state.points.find((p) => p.id === targetId);
            if (pt) {
              addToast('info', `点 n${pt.id}: (${pt.x.toFixed(2)}, ${pt.y.toFixed(2)}) / Point n${pt.id}`);
            }
          }
          break;
        }
        case 'set-midpoint': {
          // 如果有 2 个选中的点，创建中点约束
          const selectedPts = state.selectedPoints;
          if (selectedPts.length === 2 && targetId !== undefined) {
            state.saveUndoState();
            state.addConstraint({
              id: generateUniqueId(),
              type: 'betweenness',
              args: [selectedPts[0]!.id, targetId, selectedPts[1]!.id],
            });
            addToast('success', `已设为中点约束 / Midpoint constraint set`);
          } else {
            addToast('warning', '请先选择 2 个点 / Select 2 points first');
          }
          break;
        }
        case 'merge-nearest': {
          if (targetId !== undefined) {
            const pt = state.points.find((p) => p.id === targetId);
            if (pt) {
              // 找到最近的点（排除自身）
              let nearest: typeof pt | null = null;
              let minDist = Infinity;
              for (const other of state.points) {
                if (other.id === targetId) continue;
                const dist = Math.sqrt((other.x - pt.x) ** 2 + (other.y - pt.y) ** 2);
                if (dist < minDist) {
                  minDist = dist;
                  nearest = other;
                }
              }
              if (nearest && minDist < MERGE_NEAREST_PIXEL_THRESHOLD) {
                state.saveUndoState();
                // 将引用该点的线段重定向到最近点
                const updatedSegments = state.segments.map((s) => {
                  if (s.p1 === targetId) return { ...s, p1: nearest.id };
                  if (s.p2 === targetId) return { ...s, p2: nearest.id };
                  return s;
                });
                state.setSegments(updatedSegments);
                state.removePoint(targetId);
                addToast('success', `已合并 n${targetId} -> n${nearest.id} / Merged`);
              } else {
                addToast('warning', '附近没有可合并的点 / No nearby point to merge');
              }
            }
          }
          break;
        }

        // ---- 线段操作 / Segment actions ----
        case 'delete-segment': {
          if (targetId !== undefined) {
            state.saveUndoState();
            state.removeSegment(targetId);
            addToast('success', `已删除线段 s${targetId} / Deleted segment s${targetId}`);
          }
          break;
        }
        case 'segment-properties': {
          if (targetId !== undefined) {
            const seg = state.segments.find((s) => s.id === targetId);
            if (seg) {
              const p1 = state.points.find((p) => p.id === seg.p1);
              const p2 = state.points.find((p) => p.id === seg.p2);
              addToast(
                'info',
                `线段 s${seg.id}: n${seg.p1} -> n${seg.p2}` +
                (p1 && p2 ? ` (${p1.x.toFixed(1)},${p1.y.toFixed(1)}) -> (${p2.x.toFixed(1)},${p2.y.toFixed(1)})` : ''),
              );
            }
          }
          break;
        }
        case 'add-midpoint': {
          if (targetId !== undefined) {
            const seg = state.segments.find((s) => s.id === targetId);
            if (seg) {
              const p1 = state.points.find((p) => p.id === seg.p1);
              const p2 = state.points.find((p) => p.id === seg.p2);
              if (p1 && p2) {
                state.saveUndoState();
                const midId = generateUniqueId();
                const midPoint = {
                  id: midId,
                  x: (p1.x + p2.x) / 2,
                  y: (p1.y + p2.y) / 2,
                };
                state.addPoint(midPoint);
                // 将原线段拆分为两段
                state.removeSegment(targetId);
                state.addSegment({ id: generateUniqueId(), p1: seg.p1, p2: midId });
                state.addSegment({ id: generateUniqueId(), p1: midId, p2: seg.p2 });
                // 添加 betweenness 约束
                state.addConstraint({
                  id: generateUniqueId(),
                  type: 'betweenness',
                  args: [seg.p1, midId, seg.p2],
                });
                addToast('success', `已添加中点 n${midId} / Midpoint added`);
              }
            }
          }
          break;
        }
        case 'find-perpendicular': {
          if (targetId !== undefined) {
            const seg = state.segments.find((s) => s.id === targetId);
            if (seg) {
              const p1 = state.points.find((p) => p.id === seg.p1);
              const p2 = state.points.find((p) => p.id === seg.p2);
              if (p1 && p2) {
                // 计算线段方向向量并旋转 90 度
                const dx = p2.x - p1.x;
                const dy = p2.y - p1.y;
                const len = Math.sqrt(dx * dx + dy * dy);
                if (len < MIN_SEGMENT_LENGTH_PX) {
                  addToast('warning', '线段太短 / Segment too short');
                  break;
                }
                // 在中点处作垂线，长度为线段的一半
                const midX = (p1.x + p2.x) / 2;
                const midY = (p1.y + p2.y) / 2;
                const perpLen = len / 2;
                const nx = -dy / len; // 法向量
                const ny = dx / len;
                const perpEndId = generateUniqueId();
                const perpEnd = {
                  id: perpEndId,
                  x: midX + nx * perpLen,
                  y: midY + ny * perpLen,
                };
                state.saveUndoState();
                state.addPoint(perpEnd);
                // 创建中点
                const midId = generateUniqueId();
                const midPoint = { id: midId, x: midX, y: midY };
                state.addPoint(midPoint);
                state.addSegment({ id: generateUniqueId(), p1: midId, p2: perpEndId });
                addToast('success', `已作垂线 / Perpendicular line created`);
              }
            }
          }
          break;
        }

        // ---- 空白空间操作 / Empty space actions ----
        case 'add-point-here': {
          if (contextMenu.worldX !== undefined && contextMenu.worldY !== undefined) {
            state.saveUndoState();
            const newId = generateUniqueId();
            state.addPoint({
              id: newId,
              x: contextMenu.worldX,
              y: contextMenu.worldY,
            });
            addToast('success', `已添加点 n${newId} / Point added`);
          }
          break;
        }
        case 'paste': {
          // 从剪贴板读取 DSL 文本并解析执行
          (async () => {
            try {
              const clipText = await navigator.clipboard.readText();
              if (!clipText || !clipText.trim()) {
                addToast('warning', '剪贴板为空 / Clipboard is empty');
                return;
              }

              state.saveUndoState();

              const result = parseAndExecuteFormula(clipText.trim(), state.points);

              // 将解析结果添加到画布
              result.createdPoints.forEach((p) => state.addPoint(p));
              result.createdSegments.forEach((s) => state.addSegment(s));
              result.createdConstraints.forEach((c) => state.addConstraint(c));

              if (result.createdPoints.length > 0 || result.createdSegments.length > 0) {
                addToast(
                  'success',
                  `粘贴成功: ${result.createdPoints.length} 点, ${result.createdSegments.length} 线段 / Pasted: ${result.createdPoints.length} pts, ${result.createdSegments.length} segs`,
                );
              } else if (result.errors.length > 0) {
                addToast('error', `粘贴失败: ${result.errors[0] ?? '无法解析剪贴板内容'} / Paste failed`);
              } else {
                addToast('info', '剪贴板内容未生成几何对象 / No geometry created from clipboard');
              }
            } catch (err) {
              const msg = err instanceof Error ? err.message : 'Unknown error';
              // 剪贴板 API 不可用（非 HTTPS 或权限被拒）
              if (msg.includes('permission') || msg.includes('denied')) {
                addToast('warning', '剪贴板权限被拒绝，请检查浏览器设置 / Clipboard permission denied');
              } else {
                addToast('error', `粘贴失败: ${msg} / Paste failed: ${msg}`);
              }
            }
          })();
          break;
        }
        case 'select-all': {
          state.setSelectedPoints([...state.points]);
          addToast('info', `已全选 ${state.points.length} 个点 / Selected all ${state.points.length} points`);
          break;
        }

        default:
          addToast('info', `操作: ${actionId} / Action: ${actionId}`);
          break;
      }

      hideContextMenu();
    },
    [contextMenu, addToast, hideContextMenu],
  );

  // 按 Escape 关闭
  useEffect(() => {
    if (!contextMenu) return;
    const handler = (e: KeyboardEvent) => {
      if (e.key === 'Escape') hideContextMenu();
    };
    document.addEventListener('keydown', handler);
    return () => document.removeEventListener('keydown', handler);
  }, [contextMenu, hideContextMenu]);

  if (!contextMenu) return null;

  return (
    <div
      className="context-menu"
      id="contextMenu"
      style={{
        left: contextMenu.x,
        top: contextMenu.y,
        display: 'block',
      }}
    >
      {contextMenu.items.map((item) => (
        <button
          key={item.id}
          className="context-menu-item"
          data-action={item.id}
          onClick={() => handleAction(item.id)}
        >
          {item.label}
          {item.shortcut && (
            <span style={{ marginLeft: 'auto', opacity: 0.5, fontSize: '10px' }}>
              {item.shortcut}
            </span>
          )}
        </button>
      ))}
    </div>
  );
};

export default ContextMenu;
