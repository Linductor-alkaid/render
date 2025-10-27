# 纹理资源目录

此目录用于存放测试和开发中使用的纹理资源文件。

## 测试纹理

为了运行 `05_texture_test` 示例，您可以：

1. **放置您自己的测试图片**：
   - 将任何 PNG、JPG、BMP 或 TGA 格式的图片命名为 `test.png` 或 `test.jpg`
   - 放置在此目录中

2. **不提供图片**：
   - 如果没有提供图片文件，程序会自动生成一个黑白棋盘格纹理用于测试

## 支持的格式

当前 SDL_image 配置支持以下格式：
- PNG (推荐)
- JPEG/JPG
- BMP
- TGA

## 示例图片

您可以从以下来源获取免费的测试纹理：
- [Textures.com](https://www.textures.com/)
- [OpenGameArt.org](https://opengameart.org/)
- 或使用任何您自己的图片文件

## 注意事项

- 推荐使用 2 的幂次方尺寸的纹理（如 256x256, 512x512, 1024x1024）以获得最佳性能
- 如果需要透明度支持，请使用 PNG 格式

