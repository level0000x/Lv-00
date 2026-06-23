import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import path from 'path';

export default defineConfig({
  plugins: [react()],
  resolve: {
    alias: {
      '@L1': path.resolve(__dirname, 'L1-base'),
      '@L2': path.resolve(__dirname, 'L2-components'),
      '@L3': path.resolve(__dirname, 'L3-modules'),
      '@L4': path.resolve(__dirname, 'L4-shell'),
      '@L5': path.resolve(__dirname, 'L5-core'),
    },
  },
  test: { include: ['tests/**/*.test.ts'] },
});
