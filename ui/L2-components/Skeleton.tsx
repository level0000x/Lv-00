import React from 'react';

interface SkeletonProps {
  width?: number | string;
  height?: number | string;
  className?: string;
}

const Skeleton: React.FC<SkeletonProps> = ({ width = '100%', height = 16, className = '' }) => (
  <div
    className={`skeleton ${className}`.trim()}
    style={{ width, height }}
  />
);

export default Skeleton;
