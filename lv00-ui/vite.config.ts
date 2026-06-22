import { defineConfig } from 'vite';
import path from 'path';

export default defineConfig({
  resolve: {
    alias: {
      '@lv00/protocol': path.resolve(__dirname, 'packages/protocol/src'),
      '@lv00/mock-kernel': path.resolve(__dirname, 'packages/mock-kernel/src'),
      '@lv00/scene-controller': path.resolve(__dirname, 'packages/scene-controller/src'),
      '@lv00/modal-canvas': path.resolve(__dirname, 'packages/modal-canvas/src'),
      '@lv00/modal-text': path.resolve(__dirname, 'packages/modal-text/src'),
      '@lv00/modal-table': path.resolve(__dirname, 'packages/modal-table/src'),
      '@lv00/modal-tree': path.resolve(__dirname, 'packages/modal-tree/src'),
      '@lv00/modal-terminal': path.resolve(__dirname, 'packages/modal-terminal/src'),
      '@lv00/modal-topology': path.resolve(__dirname, 'packages/modal-topology/src'),
      '@lv00/visual-language': path.resolve(__dirname, 'packages/visual-language/src'),
    },
  },
  test: {
    include: ['tests/**/*.test.ts'],
  },
});
