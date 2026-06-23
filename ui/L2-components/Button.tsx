import React from 'react';

export type ButtonVariant = 'default' | 'primary' | 'accent' | 'ghost' | 'danger';

export interface ButtonProps extends React.ButtonHTMLAttributes<HTMLButtonElement> {
  variant?: ButtonVariant;
  small?: boolean;
  tooltip?: string;
  shortcut?: string;
}

const Button = React.memo(React.forwardRef<HTMLButtonElement, ButtonProps>(
  function Button({
    variant = 'default',
    small = false,
    children,
    className = '',
    disabled = false,
    tooltip,
    shortcut,
    ...rest
  }, ref) {
    const cls = [
      'btn',
      variant === 'primary' ? 'btn-primary' : '',
      variant === 'danger' ? 'btn-danger' : '',
      small ? 'btn-small' : '',
      className,
    ].filter(Boolean).join(' ');

    return (
      <button
        ref={ref}
        className={cls}
        disabled={disabled}
        title={tooltip}
        {...rest}
      >
        {children}
        {shortcut && <span className="kbd">{shortcut}</span>}
      </button>
    );
  }
));

export default Button;
