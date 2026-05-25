# Lv-00 Web 可视化演示

基于 WebAssembly 的 Lv-00 几何构造可视化工具。

## 🌐 在线演示

访问: https://yourusername.github.io/lv00/

## 🏗️ 架构

```
web/
├── CMakeLists.txt          # Emscripten 构建配置
├── lv00_web_bindings.c     # WebAssembly C 绑定
├── index.html              # 主页面
├── js/
│   └── app.js             # JavaScript 应用程序
├── dist/                  # 构建输出目录
└── README.md              # 本文件
```

## 🚀 本地开发

### 前提条件

- [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html)
- 现代浏览器（支持 WebAssembly）

### 构建步骤

1. **激活 Emscripten 环境**
   ```bash
   source /path/to/emsdk/emsdk_env.sh
   ```

2. **构建 WebAssembly 模块**
   ```bash
   cd web
   emcmake cmake -B build
   cmake --build build
   ```

3. **启动本地服务器**
   ```bash
   # Python 3
   python -m http.server 8000
   
   # 或 Node.js
   npx serve .
   ```

4. **打开浏览器**
   访问 http://localhost:8000

## 🎯 功能特性

### 工具
- 👆 **选择** - 选择和查看几何对象
- 📍 **点** - 在画布上添加点
- 📏 **线段** - 连接两点创建线段
- ✋ **平移** - 拖动平移视图

### 操作
- **归一化** - 合并重复节点
- **清空** - 重置画布
- **缩放** - 鼠标滚轮缩放

### 示例
- 等边三角形
- 正方形
- 线段相交
- 中点构造

## 📊 技术细节

### WebAssembly 接口

C 函数通过 Emscripten 的 `ccall` 暴露给 JavaScript：

```javascript
// 创建图
const graph = Module.ccall('web_graph_create', 'number', [], []);

// 添加点
const id = Module.ccall('web_graph_add_point', 'number',
    ['number', 'number', 'number', 'number', 'number'],
    [graph, x_num, x_den, y_num, y_den]
);

// 归一化
const merged = Module.ccall('web_graph_normalize', 'number',
    ['number'], [graph]
);
```

### 坐标系统

- 世界坐标：数学坐标系（原点在中心）
- 屏幕坐标：Canvas 像素坐标
- 自动处理缩放和平移变换

### 渲染

使用 HTML5 Canvas 2D API：
- 网格背景
- 坐标轴
- 点（带标签）
- 线段

## 🔧 自定义构建

### 修改内存限制

编辑 `CMakeLists.txt`：
```cmake
-s INITIAL_MEMORY=64MB
-s MAXIMUM_MEMORY=256MB
```

### 添加新功能

1. 在 `lv00_web_bindings.c` 中添加 C 函数
2. 使用 `EMSCRIPTEN_KEEPALIVE` 宏导出
3. 在 `app.js` 中调用

## 🐛 调试

### 浏览器控制台

```javascript
// 访问应用实例
app.graph          // 当前图对象
app.points         // 点数组
app.segments       // 线段数组
app.module         // WebAssembly 模块

// 手动调用函数
app.addPoint(10, 20);
app.normalize();
app.render();
```

### 常见问题

1. **WASM 加载失败**
   - 确保使用本地服务器（不是 file:// 协议）
   - 检查浏览器是否支持 WebAssembly

2. **内存不足**
   - 增加 `INITIAL_MEMORY` 或 `MAXIMUM_MEMORY`

3. **性能问题**
   - 减少同时显示的点数
   - 使用 Release 模式构建（`-O3`）

## 📦 部署

自动部署到 GitHub Pages：

1. 推送代码到 `main` 分支
2. GitHub Actions 自动构建并部署
3. 访问 `https://<username>.github.io/lv00/`

手动部署：

```bash
# 构建
emcmake cmake -B build
cmake --build build

# 复制到 dist
cp build/lv00_web.* dist/
cp js/app.js dist/
cp index.html dist/

# 部署到任意静态托管
```

## 📝 许可证

MIT License - 同主项目
