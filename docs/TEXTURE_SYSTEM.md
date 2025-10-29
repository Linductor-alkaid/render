# 纹理系统使用指南

[返回文档首页](README.md)

## 概述

纹理系统提供了完整的纹理加载、管理和缓存功能，包括：
- `Texture` 类：底层纹理对象
- `TextureLoader` 类：纹理加载器和缓存管理器
- 支持同步和异步加载
- 自动引用计数和资源管理

## Texture 类

### 基本使用

```cpp
#include "render/texture.h"

// 创建纹理对象
auto texture = std::make_shared<Texture>();

// 从文件加载
if (texture->LoadFromFile("textures/test.png", true)) {
    // 设置过滤模式
    texture->SetFilter(TextureFilter::Linear, TextureFilter::Linear);
    
    // 设置环绕模式
    texture->SetWrap(TextureWrap::Repeat, TextureWrap::Repeat);
    
    // 绑定到纹理单元 0
    texture->Bind(0);
}
```

### 从内存数据创建纹理

```cpp
// 准备纹理数据（例如程序化生成）
std::vector<unsigned char> data(256 * 256 * 4);
// ... 填充数据 ...

auto texture = std::make_shared<Texture>();
texture->CreateFromData(data.data(), 256, 256, TextureFormat::RGBA, true);
```

### 纹理格式支持

- `TextureFormat::RGB` - RGB 格式
- `TextureFormat::RGBA` - RGBA 格式（含透明度）
- `TextureFormat::RED` - 单通道
- `TextureFormat::RG` - 双通道
- `TextureFormat::Depth` - 深度纹理
- `TextureFormat::DepthStencil` - 深度模板纹理

### 纹理过滤模式

- `TextureFilter::Nearest` - 最近邻过滤（像素风格）
- `TextureFilter::Linear` - 线性过滤（平滑）
- `TextureFilter::Mipmap` - Mipmap 过滤（最佳质量）

### 纹理环绕模式

- `TextureWrap::Repeat` - 重复
- `TextureWrap::MirroredRepeat` - 镜像重复
- `TextureWrap::ClampToEdge` - 边缘截取
- `TextureWrap::ClampToBorder` - 边界颜色

## TextureLoader 类（推荐使用）

TextureLoader 提供了纹理缓存和资源管理功能。

### 同步加载（带缓存）

```cpp
#include "render/texture_loader.h"

auto& loader = TextureLoader::GetInstance();

// 首次加载（从文件）
auto texture1 = loader.LoadTexture("my_texture", "textures/test.png", true);

// 再次加载（从缓存，不会重复加载文件）
auto texture2 = loader.LoadTexture("my_texture", "textures/test.png", true);

// texture1 和 texture2 是同一个对象
assert(texture1.get() == texture2.get());
```

### 异步加载

```cpp
// 启动异步加载
auto future = loader.LoadTextureAsync("large_texture", "textures/large.png", true);

// 继续执行其他任务...
DoOtherWork();

// 等待加载完成并获取结果
AsyncTextureResult result = future.get();
if (result.success) {
    TexturePtr texture = result.texture;
    // 使用纹理...
} else {
    LOG_ERROR("加载失败: " + result.error);
}
```

### 从内存创建（带缓存）

```cpp
std::vector<unsigned char> checkerboard(256 * 256 * 4);
// ... 生成棋盘格数据 ...

auto texture = loader.CreateTexture("checkerboard", 
                                   checkerboard.data(), 
                                   256, 256,
                                   TextureFormat::RGBA, 
                                   true);
```

### 获取已缓存的纹理

```cpp
// 检查是否已缓存
if (loader.HasTexture("my_texture")) {
    auto texture = loader.GetTexture("my_texture");
    // 使用纹理...
}
```

### 预加载纹理列表

```cpp
// 定义纹理列表 {名称, 路径, 是否生成Mipmap}
std::vector<std::tuple<std::string, std::string, bool>> textures = {
    {"wall", "textures/wall.png", true},
    {"floor", "textures/floor.png", true},
    {"character", "textures/character.png", true}
};

// 批量预加载
size_t loaded = loader.PreloadTextures(textures);
LOG_INFO("预加载了 " + std::to_string(loaded) + " 个纹理");
```

### 资源管理

```cpp
// 打印缓存统计
loader.PrintStatistics();

// 清理未使用的纹理（引用计数=1，仅被缓存持有）
size_t cleaned = loader.CleanupUnused();

// 移除指定纹理
loader.RemoveTexture("unused_texture");

// 清空所有缓存
loader.Clear();

// 获取内存使用量
size_t memoryMB = loader.GetTotalMemoryUsage() / 1024 / 1024;
LOG_INFO("纹理总内存: " + std::to_string(memoryMB) + " MB");
```

## 在着色器中使用纹理

### GLSL 着色器示例

```glsl
// 顶点着色器
#version 450 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

void main() {
    gl_Position = vec4(aPos, 1.0);
    // 翻转 Y 轴（图片加载时原点在左上角，OpenGL 纹理坐标原点在左下角）
    TexCoord = vec2(aTexCoord.x, 1.0 - aTexCoord.y);
}

// 片段着色器
#version 450 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D uTexture;

void main() {
    FragColor = texture(uTexture, TexCoord);
}
```

### C++ 绑定和渲染

```cpp
// 设置 uniform
shader->Use();
shader->GetUniformManager()->SetInt("uTexture", 0);

// 渲染循环
while (running) {
    // 绑定纹理到纹理单元 0
    texture->Bind(0);
    
    // 使用着色器
    shader->Use();
    
    // 绘制几何体
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}
```

## 重要注意事项

### 1. 纹理 Y 轴翻转

大多数图片格式（PNG、JPG）的原点在左上角，而 OpenGL 纹理坐标原点在左下角。需要在着色器或加载时翻转：

**方案 A：在顶点着色器中翻转（推荐）**
```glsl
TexCoord = vec2(aTexCoord.x, 1.0 - aTexCoord.y);
```

**方案 B：调整顶点数据的纹理坐标**
```cpp
// 将 UV 坐标上下翻转
const float vertices[] = {
    -0.5f, -0.5f, 0.0f,  0.0f, 1.0f,  // 左下 (原本是 0,0)
     0.5f, -0.5f, 0.0f,  1.0f, 1.0f,  // 右下 (原本是 1,0)
     0.5f,  0.5f, 0.0f,  1.0f, 0.0f,  // 右上 (原本是 1,1)
    -0.5f,  0.5f, 0.0f,  0.0f, 0.0f   // 左上 (原本是 0,1)
};
```

### 2. 2D 渲染设置

渲染 2D 纹理时需要禁用某些 3D 功能：

```cpp
// 禁用面剔除（2D 几何体通常只有一个面）
glDisable(GL_CULL_FACE);

// 禁用深度测试（2D 不需要深度）
glDisable(GL_DEPTH_TEST);
```

### 3. 线程安全

`TextureLoader` 是线程安全的，可以在多线程环境中使用：

```cpp
// 在多个线程中异步加载纹理
std::vector<std::future<AsyncTextureResult>> futures;

for (const auto& path : texturePaths) {
    futures.push_back(loader.LoadTextureAsync(name, path, true));
}

// 等待所有加载完成
for (auto& future : futures) {
    auto result = future.get();
    // 处理结果...
}
```

### 4. 引用计数和生命周期

```cpp
{
    // texture1 引用计数 = 2 (缓存1 + 变量1)
    auto texture1 = loader.LoadTexture("tex", "test.png");
    
    {
        // texture2 引用计数 = 3 (缓存1 + texture1 + texture2)
        auto texture2 = loader.GetTexture("tex");
    }
    // texture2 销毁，引用计数 = 2
}
// texture1 销毁，引用计数 = 1（仅缓存持有）

// 清理未使用的纹理（引用计数=1）
loader.CleanupUnused();  // 会清理 "tex"
```

## 支持的图片格式

当前配置支持：
- **PNG** - 推荐，支持透明度
- **JPEG/JPG** - 压缩格式
- **BMP** - 无压缩
- **TGA** - 支持透明度

## 性能建议

### 1. 纹理尺寸

- 优先使用 2 的幂次方尺寸（256, 512, 1024, 2048）
- 可以使用非 2 的幂，但可能影响 Mipmap 性能
- 过大的纹理会消耗大量显存

### 2. Mipmap

```cpp
// 开启 Mipmap（推荐，提升远距离渲染质量）
texture->LoadFromFile("test.png", true);

// 关闭 Mipmap（节省内存，适合不缩放的 UI 纹理）
texture->LoadFromFile("ui.png", false);
```

### 3. 预加载

```cpp
// 在关卡开始前预加载所有纹理
loader.PreloadTextures({
    {"level1_wall", "level1/wall.png", true},
    {"level1_floor", "level1/floor.png", true},
    // ...
});

// 关卡结束后清理
loader.Clear();
```

## 示例：完整的纹理渲染

```cpp
#include "render/texture_loader.h"
#include "render/shader.h"

int main() {
    // 初始化 OpenGL 上下文...
    
    // 加载纹理
    auto& loader = TextureLoader::GetInstance();
    auto texture = loader.LoadTexture("test", "textures/test.png", true);
    
    if (!texture) {
        LOG_ERROR("纹理加载失败");
        return -1;
    }
    
    // 加载着色器
    auto shader = std::make_shared<Shader>();
    shader->LoadFromFile("shaders/texture.vert", "shaders/texture.frag");
    
    // 设置 uniform
    shader->Use();
    shader->GetUniformManager()->SetInt("uTexture", 0);
    shader->Unuse();
    
    // 渲染循环
    while (running) {
        glClear(GL_COLOR_BUFFER_BIT);
        
        texture->Bind(0);
        shader->Use();
        
        // 绘制几何体...
        
        SwapBuffers();
    }
    
    // 打印统计
    loader.PrintStatistics();
    
    // 清理
    loader.Clear();
    
    return 0;
}
```

## 常见问题

### Q: 为什么图片上下颠倒？

A: OpenGL 纹理坐标原点在左下角，图片文件原点在左上角。在顶点着色器中翻转：
```glsl
TexCoord = vec2(aTexCoord.x, 1.0 - aTexCoord.y);
```

### Q: 为什么 2D 矩形不显示？

A: 可能是面剔除问题。2D 渲染时禁用：
```cpp
glDisable(GL_CULL_FACE);
glDisable(GL_DEPTH_TEST);
```

### Q: 如何判断纹理是否成功加载？

A: 检查返回值和 IsValid()：
```cpp
auto texture = loader.LoadTexture("tex", "path.png");
if (texture && texture->IsValid()) {
    // 加载成功
}
```

### Q: 缓存何时清理？

A: 使用 `shared_ptr` 引用计数自动管理：
```cpp
// 手动清理未使用的纹理
loader.CleanupUnused();

// 或在场景切换时清空所有
loader.Clear();
```

## API 参考

### Texture 类

| 方法 | 说明 |
|------|------|
| `LoadFromFile(path, mipmap)` | 从文件加载纹理 |
| `CreateFromData(data, w, h, format, mipmap)` | 从内存创建纹理 |
| `CreateEmpty(w, h, format)` | 创建空纹理 |
| `Bind(unit)` | 绑定到纹理单元 |
| `SetFilter(min, mag)` | 设置过滤模式 |
| `SetWrap(s, t)` | 设置环绕模式 |
| `GenerateMipmap()` | 生成 Mipmap |
| `GetWidth() / GetHeight()` | 获取尺寸 |
| `IsValid()` | 检查有效性 |

### TextureLoader 类

| 方法 | 说明 |
|------|------|
| `LoadTexture(name, path, mipmap)` | 同步加载（带缓存） |
| `LoadTextureAsync(name, path, mipmap)` | 异步加载 |
| `CreateTexture(name, data, w, h, format, mipmap)` | 创建纹理（带缓存） |
| `GetTexture(name)` | 获取已缓存纹理 |
| `HasTexture(name)` | 检查是否已缓存 |
| `RemoveTexture(name)` | 移除纹理 |
| `Clear()` | 清空所有缓存 |
| `CleanupUnused()` | 清理未使用纹理 |
| `PreloadTextures(list)` | 批量预加载 |
| `PrintStatistics()` | 打印统计信息 |
| `GetTotalMemoryUsage()` | 获取内存使用量 |

## 调试技巧

### 1. 启用日志

```cpp
Logger::GetInstance().SetLogLevel(LogLevel::Debug);
Logger::GetInstance().SetLogToFile(true);
```

### 2. 查看缓存统计

```cpp
loader.PrintStatistics();
```

输出示例：
```
========================================
纹理缓存统计信息
========================================
缓存纹理数量: 3
总内存使用量（估算）: 6 MB
----------------------------------------
纹理详情:
  - wall: 1024x1024, 引用计数: 2, 内存: ~4096 KB
  - floor: 512x512, 引用计数: 1, 内存: ~1024 KB
  - char: 256x256, 引用计数: 3, 内存: ~256 KB
========================================
```

### 3. 检查 OpenGL 错误

```cpp
GLenum err = glGetError();
if (err != GL_NO_ERROR) {
    LOG_ERROR("OpenGL 错误: " + std::to_string(err));
}
```

## 相关文件

- `include/render/texture.h` - Texture 类定义
- `src/rendering/texture.cpp` - Texture 类实现
- `include/render/texture_loader.h` - TextureLoader 类定义
- `src/rendering/texture_loader.cpp` - TextureLoader 类实现
- `examples/05_texture_test.cpp` - 使用示例

---

[返回文档首页](README.md)

