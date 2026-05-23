/**
 * @module components/common/ConstraintModal
 * @description 通用约束对话框组件 / Reusable constraint dialog component
 *
 * 用于 GraphPanel 中四种约束（incidence / betweenness / intersection / containment）
 * 的输入对话框，替代重复的模态框代码。
 */

import React from 'react';
import Modal from './Modal';

/** 约束模态框中的下拉选择字段 / Dropdown field in constraint modal */
export interface ConstraintField {
  /** 字段标签 / Field label */
  label: string;
  /** 字段名称（用作 state key）/ Field name (used as state key) */
  name: string;
  /** 可选项数组 / Array of options */
  options: { value: string; label: string }[];
}

interface ConstraintModalProps {
  /** 是否显示 / Whether to show */
  visible: boolean;
  /** 对话框标题 / Dialog title */
  title: string;
  /** 表单字段定义 / Form field definitions */
  fields: ConstraintField[];
  /** 当前选中值 / Current selected values */
  values: Record<string, string>;
  /** 值变更回调 / Value change callback */
  onChange: (name: string, value: string) => void;
  /** 确认回调 / Confirm callback */
  onConfirm: () => void;
  /** 取消回调 / Cancel callback */
  onCancel: () => void;
}

/**
 * 通用约束对话框
 *
 * @description 渲染一个包含多个下拉选择器的约束输入对话框。
 *              统一了四种约束对话框的 UI 结构和交互逻辑。
 *
 * @param props - 组件属性
 * @returns 约束对话框 JSX
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
        {fields.map((field) => (
          <div className="constraint-field" key={field.name}>
            <label className="constraint-label" htmlFor={`constraint-${field.name}`}>
              {field.label}
            </label>
            <select
              id={`constraint-${field.name}`}
              className="constraint-select"
              value={values[field.name] || ''}
              onChange={(e) => onChange(field.name, e.target.value)}
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
        <div className="constraint-actions">
          <button className="btn" onClick={onConfirm}>
            OK
          </button>
          <button className="btn" onClick={onCancel}>
            CANCEL
          </button>
        </div>
      </div>
    </Modal>
  );
};

export default ConstraintModal;
