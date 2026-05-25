# 图标占位说明

此目录用于存放 Tauri 应用图标文件。在构建桌面应用之前，需要提供以下图标：

## 所需图标文件

| 文件名 | 尺寸 | 用途 |
|--------|------|------|
| `32x32.png` | 32x32 像素 | Linux 小图标 |
| `128x128.png` | 128x128 像素 | Linux 中图标 |
| `128x128@2x.png` | 256x256 像素 | Linux 高 DPI 图标 |
| `icon.icns` | 多尺寸 | macOS 应用图标 |
| `icon.ico` | 多尺寸 | Windows 应用图标 |

## 生成图标的方法

### 方法一：使用 Tauri CLI 自动生成
```bash
# 安装图标生成工具
cargo install tauri-utils
# 或使用 npm
npm install --save-dev @aspect-build/rules_js

# 从源图片生成所有尺寸
npx tauri icon path/to/source-icon.png
```

### 方法二：手动生成
1. 准备一个 1024x1024 或更大的 PNG 源图片
2. 使用图像编辑工具（如 GIMP、Photoshop）缩放为各尺寸
3. 使用 `icotool` 生成 `.ico` 文件
4. 使用 `iconutil`（macOS）生成 `.icns` 文件

### 方法三：在线工具
- https://tauri.app/v1/guides/features/icons/ 提供了详细的图标生成指南
- https://realfavicongenerator.net/ 可生成多种尺寸的图标

## 注意事项

- 源图片建议使用 PNG 格式，背景透明
- 图标应简洁清晰，在小尺寸下也能辨识
- Windows 的 `.ico` 文件需要包含 16x16、32x32、48x48、256x256 等多种尺寸
- macOS 的 `.icns` 文件需要包含 16x16 到 1024x1024 的多种尺寸
