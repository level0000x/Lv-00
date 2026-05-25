/**
 * @module components/common/ConstraintModal
 * @description 通用约束对话框组件 / Reusable constraint dialog component
 *
 * 用于 GraphPanel 中四种约束（incidence / betweenness / intersection / containment）
 * 的输入对话框，替代重复的模态框代码。
 *
 * Used for four types of constraints (incidence / betweenness / intersection / containment)
 * input dialogs in GraphPanel, replacing repetitive modal code.
 */

import React from 'react';
import Modal from './Modal';
import Button from './Button';

/** 约束模态框中的下拉选择字段 / Dropdown field in constraint modal */
export interface ConstraintField {
  /** 字段标签 / Field label */
  label: string;
  /** 字段名称（用作 state key）/ Field name (used as state key) */
  name: string;
  /** 可选项数组 / Array of options */
  options: { value: string; label: string }[];
}

/**
 * ConstraintModal 组件的 props 接口 / ConstraintModal component props interface
 * @property visible - 是否显示（保留用于向后兼容，实际显示由 Modal store 控制）/ Whether to show (kept for backward compatibility, actual visibility controlled by Modal store)
 * @property title - 对话框标题 / Dialog title
 * @property fields - 表单字段定义 / Form field definitions
 * @property values - 当前选中值 / Current selected values
 * @property onChange - 值变更回调 / Value change callback
 * @property onConfirm - 确认回调 / Confirm callback
 * @property onCancel - 取消回调 / Cancel callback
 */
interface ConstraintModalProps {
  /** 是否显示（保留用于向后兼容）/ Whether to show (kept for backward compatibility) */
  visible?: boolean;
  /** 对话框标题 / Dialog title */
  title: string;
  /** 表单字段定义数组 / Array of form field definitions */
  fields: ConstraintField[];
  /** 当前选中值映射 / Current selected values map */
  values: Record<string, string>;
  /** 值变更回调 / Value change callback */
  onChange: (name: string, value: string) => void;
  /** 确认回调 / Confirm callback */
  onConfirm: () => void;
  /** 取消回调 / Cancel callback */
  onCancel: () => void;
}

/**
 * ConstraintModal - 通用约束对话框 / Reusable constraint dialog
 *
 * 渲染一个包含多个下拉选择器的约束输入对话框。
 * 统一了四种约束对话框的 UI 结构和交互逻辑。
 * 使用 Button 组件替代原生 button 元素，保持样式一致性。
 *
 * Renders a constraint input dialog with multiple dropdown selectors.
 * Unifies the UI structure and interaction logic of four constraint dialogs.
 * Uses the Button component instead of native button elements for style consistency.
 *
 * @example
 * ```tsx
 * <ConstraintModal
 *   title="关联约束 / Incidence Constraint"
 *   fields={[
 *     { name: 'point', label: '点', options: pointOptions },
 *     { name: 'segment', label: '线段', options: segmentOptions },
 *   ]}
 *   values={{ point: '', segment: '' }}
 *   onChange={(name, value) => setValues(prev => ({ ...prev, [name]: value }))}
 *   onConfirm={handleConfirm}
 *   onCancel={handleCancel}
 * />
 * ```
 */
const ConstraintModal: React.FC<ConstraintModalProps> = ({
  visible: _visible,
  title,
  fields,
  values,
  onChange,
  onConfirm,
  onCancel,
}) => {
  return (
    <Modal id="constraint-modal" title={title} onCancel={onCancel}>
      <div className="constraint-modal-body">
        {/* 渲染所有下拉选择字段 / Render all dropdown fields */}
        {fields.map((field) => (
          <div className="constraint-field" key={field.name}>
            <label className="constraint-label" htmlFor={`constraint-${field.name}`}>
              {field.label}
            </label>
            <select
              id={`constraint-${field.name}`}
              className="constraint-select"
              value={values[field.name] ?? ''}
              onChange={(e) => onChange(field.name, e.target.value)}
              aria-label={field.label}
            >
              <option value="">-- 选择 / Select --</option>
              {field.options.map((opt) => (
                <option key={opt.value} value={opt.value}>
                  {opt.label}
                </option>
              ))}
            </select>
          </div>
        ))}

        {/* 操作按钮区域 / Action buttons area */}
        <div className="constraint-actions">
          <Button onClick={onConfirm}>OK</Button>
          <Button onClick={onCancel}>CANCEL</Button>
        </div>
      </div>
    </Modal>
  );
};

export default ConstraintModal;
