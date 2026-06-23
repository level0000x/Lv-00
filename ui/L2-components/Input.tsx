import React from 'react';

type InputProps = React.InputHTMLAttributes<HTMLInputElement>;

const Input = React.forwardRef<HTMLInputElement, InputProps>(
  function Input({ className = '', ...rest }, ref) {
    return (
      <input ref={ref} className={`input ${className}`.trim()} {...rest} />
    );
  }
);

export default Input;
