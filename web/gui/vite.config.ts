import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import path from 'path';

/**
 * Lv-00 GUI 的 Vite 构建配置
 *
 * 核心配置：
 * - React 插件：启用 Fast Refresh 快速热更新
 * - 路径别名：@ 映射到 src/ 目录，简化导入路径
 * - 构建输出：dist/ 目录，启用 sourcemap 便于调试
 * - 开发服务器：端口 5173，启动时自动打开浏览器
 * - 注意：构建命令 "tsc -b && vite build" 先执行全量类型检查
 */
export default defineConfig({
  /** React 插件配置 —— 提供 JSX 转换和 Fast Refresh 支持 */
  plugins: [react()],

  /** 路径解析 —— @/ 别名指向 src/ 目录 */
  resolve: {
    alias: {
      '@': path.resolve(__dirname, './src'),
    },
  },

  /** 生产构建配置 */
  build: {
    /** 输出目录 */
    outDir: 'dist',
    /** 生成 sourcemap：开发环境完整 sourcemap 便于调试，生产环境使用 hidden 模式避免暴露源代码结构 */
    sourcemap: process.env.NODE_ENV === 'development' ? true : 'hidden',
  },

  /** 开发服务器配置 */
  server: {
    /** 监听端口 */
    port: 5173,
    /** 启动时自动打开浏览器 */
    open: true,
  },
});
