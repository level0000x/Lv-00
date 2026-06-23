import React from 'react';

interface KbdProps {
  children: React.ReactNode;
}

const Kbd: React.FC<KbdProps> = ({ children }) => (
  <kbd className="kbd">{children}</kbd>
);

export default Kbd;
