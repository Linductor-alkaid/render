# Texture API 参考

[返回 API 首页](README.md)

---

## 概述

`Texture` 类管理 OpenGL 纹理对象的创建、加载、参数设置和释放。支持从文件加载（使用 SDL_image）和从内存数据创建纹理。

**头文件**: `render/texture.h`  
**命名空间**: `Render`

---

## 类定义

```cpp
class Texture {
public:
    Texture();
    ~Texture();
    
    // 禁止拷贝
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    
    // 允许移动
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;
    
    // 加载和创建
    bool LoadFromFile(const std::string& filepath, bool generateMipmap = true);
    bool CreateFromData(const void* data, int width, int height,
                       TextureFormat format = TextureFormat::RGBA,
                       bool generateMipmap = true);
    bool CreateEmpty(int width, int height, TextureFormat format = TextureFormat::RGBA);
    
    // 绑定和状态设置
    void Bind(unsigned int unit = 0) const;
    void Unbind() const;
    void SetFilter(TextureFilter minFilter, TextureFilter magFilter);
    void SetWrap(TextureWrap wrapS, TextureWrap wrapT);
    void GenerateMipmap();
    void Release();
    
    // 获取信息
    GLuint GetID() const;
    int GetWidth() const;
    int GetHeight() const;
    TextureFormat GetFormat() const;
    bool IsValid() const;
};
```

---

## 枚举类型

### TextureFormat

纹理格式枚举。

```cpp
enum class TextureFormat {
    RGB,            // RGB 格式（24位）
    RGBA,           // RGBA 格式（32位，含透明度）
    RED,            // 单通道红色（8位）
    RG,             // 双通道 RG（16位）
    Depth,          // 深度格式
    DepthStencil    // 深度模板格式
};
```

### TextureFilter

纹理过滤模式枚举。

```cpp
enum class TextureFilter {
    Nearest,    // 最近邻过滤（像素风格，性能高）
    Linear,     // 线性过滤（平滑，质量高）
    Mipmap      // Mipmap 过滤（最佳质量）
};
```

### TextureWrap

纹理环绕模式枚举。

```cpp
enum class TextureWrap {
    Repeat,         // 重复纹理
    MirroredRepeat, // 镜像重复纹理
    ClampToEdge,    // 边缘截取（使用边缘像素）
    ClampToBorder   // 边界颜色
};
```

---

## 构造函数

### `Texture()`

创建空的纹理对象。

**示例**:
```cpp
auto texture = std::make_shared<Texture>();
```

---

## 从文件加载

### `LoadFromFile()`

从文件路径加载纹理（使用 SDL_image）。

```cpp
bool LoadFromFile(const std::string& filepath, bool generateMipmap = true)
```

**参数**:
- `filepath` - 纹理文件路径（PNG、JPG、BMP、TGA 等）
- `generateMipmap` - 是否生成 Mipmap（默认 `true`）

**返回值**: 成功返回 `true`，失败返回 `false`

**支持的格式**: PNG、JPEG、BMP、TGA

**示例**:
```cpp
auto texture = std::make_shared<Texture>();
if (texture->LoadFromFile("textures/wall.png", true)) {
    // 加载成功
    texture->Bind(0);
}
```

---

## 从内存创建纹理

### `CreateFromData()`

从内存数据创建纹理。

```cpp
bool CreateFromData(const void* data, int width, int height,
                   TextureFormat format = TextureFormat::RGBA,
                   bool generateMipmap = true)
```

**参数**:
- `data` - 纹理数据指针（RGBA、RGB 等格式）
- `width` - 纹理宽度
- `height` - 纹理高度
- `format` - 纹理格式（默认 `RGBA`）
- `generateMipmap` - 是否生成 Mipmap（默认 `true`）

**返回值**: 成功返回 `true`，失败返回 `false`

**示例**:
```cpp
// 创建棋盘格纹理
std::vector<unsigned char> data(256 * 256 * 4);
// ... 填充数据 ...

auto texture = std::make_shared<Texture>();
texture->CreateFromData(data.data(), 256, 256, TextureFormat::RGBA, true);
```

### `CreateEmpty()`

创建空纹理（通常用于渲染目标）。

```cpp
bool CreateEmpty(int width, int height, TextureFormat format = TextureFormat::RGBA)
```

**参数**:
- `width` - 纹理宽度
- `height` - 纹理高度
- `format` - 纹理格式（默认 `RGBA`）

**返回值**: 成功返回 `true`，失败返回 `false`

**示例**:
```cpp
// 创建深度纹理
auto depthTexture = std::make_shared<Texture>();
depthTexture->CreateEmpty(1920, 1080, TextureFormat::Depth);
```

---

## 绑定和状态

### `Bind()`

绑定纹理到指定纹理单元。

```cpp
void Bind(unsigned int unit = 0) const
```

**参数**:
- `unit` - 纹理单元索引（0-31，默认 0）

**示例**:
```cpp
// 绑定到纹理单元 0
texture->Bind(0);

// 绑定到纹理单元 1
texture->Bind(1 existing code ... )

// 在着色器中设置
shader->GetUniformManager()->SetInt("uTexture", 0);
```

### `Unbind()`

解绑当前纹理。

```cpp
void Unbind() const
```

**示例**:
```cpp
texture->Bind(0);
// ... 使用纹理 ...
texture->Unbind();
```

### `SetFilter()`

设置纹理过滤模式。

```cpp
void SetFilter(TextureFilter minFilter, TextureFilter magFilter)
```

**参数**:
- `minFilter` - 缩小过滤（纹理被缩小时的过滤）
- `magFilter` - 放大过滤（纹理被放大时的过滤）

**注意**: `Mipmap` 只适用于 `minFilter`

**示例接收**:
```cpp
// 线性过滤（推荐）
texture->SetFilter(TextureFilter::Linear, TextureFilter::Linear);

// 像素风格（最近邻）
texture->SetFilter(TextureFilter::Nearest, TextureFilter::Nearest);

// 使用 Mipmap（最佳质量）
texture->SetFilter(TextureFilter::Mipmap, TextureFilter::Linear);
```

### `SetWrap()`

设置纹理环绕模式。

```cpp
void SetWrap(TextureWrap wrapS, TextureWrap wrapT)
```

**参数**:
- `wrapS` - S 轴（水平方向）环绕模式
- `wrapT` - T 轴（垂直方向）环绕模式

**示例**:
```cpp
// 重复模式（适合地板、天空盒等）
texture->SetWrap(TextureWrap::Repeat, TextureWrap::Repeat);

// 边缘截取（适合 UI 纹理）
texture->SetWrap(TextureWrap::ClampToEdge, TextureWrap::ClampToEdge);

// 镜像重复
texture->SetWrap(TextureWrap::MirroredRepeat, TextureWrap::MirroredRepeat);
```

### `GenerateMipmap()`

手动生成 Mipmap（如果加载时未生成）。

```cpp
void GenerateMipmap()
```

**示例**:
```cpp
texture->LoadFromFile("test.png", false);  // 不生成 Mipmap
// ... 修改纹理数据 ...
texture->GenerateMipmap(); МipmapSemantic Misuse (may be intentional)  // 现在生成
```

---

## 资源管理

### `Release()`

释放纹理资源。

```cpp
void Release()
```

**注意**: 析构函数会自动调用此方法。

**示例**:
```cpp
auto texture = std::make_shared<Texture>();
texture->LoadFromFile("test.png");

// 不需要时手动释放（通常不需要，智能指针会自动管理）
texture->Release();
```

---

## 信息获取

### `GetID()`

获取 OpenGL 纹理 ID。

```cpp
GLuint GetID() const
```

**返回值**: OpenGL 纹理对象 ID

### `GetWidth()`

获取纹理宽度（像素）。

```cpp
int GetWidth() const
```

### `GetHeight()`

获取纹理高度（像素）。

```cpp
int GetHeight() const
```

### `GetFormat()`

获取纹理格式。

```cpp
TextureFormat GetFormat() const
```

**返回值**: 纹理格式枚举值

### `IsValid()`

检查纹理是否有效。

```cpp
bool IsValid() const
```

**返回值**: 纹理 ID 不为 0 时返回 `true`

**示例**:
```cpp
if (texture->LoadFromFile("test.png")) {
    if (texture->IsValid()) {
        std::cout << "纹理有效: " << texture->GetWidth() 
                  << "x" << texture->GetHeight() << std::endl;
    }
}
```

---

## 使用示例

### 基本使用

```cpp
#include "render/texture.h"

int main() {
    // 创建纹理
    auto texture = std::make_shared<Texture>();
    
    // 从文件加载
    if (!texture->LoadFromFile("textures/test.png", true)) {
        LOG_ERROR("纹理加载失败");
        return -1;
    }
    
    // 设置过滤和环绕模式
    texture->SetFilter(TextureFilter::Linear, TextureFilter::Linear);
    texture->SetWrap(TextureWrap::Repeat, TextureWrap::Repeat);
    
    // 绑定并使用
    texture->Bind(0);
    
    // 渲染...
    
    return 0;
}
```

### 程序化纹理

```cpp
#include "render/texture.h"

// 创建程序化纹理
auto CreateCheckerboard(int size, int checkerSize) -> TexturePtr {
    std::vector<unsigned char> data(size * size * 4);
    
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int index = (y * size + x) * 4;
            bool isWhite = ((x / checkerSize) + (y / checkerSize)) % 2 == 0;
            
            unsigned char color = isWhite ? 255 : 0;
            data[index + 0] = color;  // R
            data[index + 1] = color;  // G
            data[index + 2] = color;  // B
            data[index + 3] = 255;     // A
        }
    }
    
    auto texture = std::make_shared<Texture>();
    texture->CreateFromData(data.data(), size, size, TextureFormat::RGBA, false);
    return texture;
}
```

### 在着色器中使用

```cpp
// 顶点着色器 (texture.vert)
#version 450 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

void main() {
    gl_Position = vec4(aPos, 1.0);
    // 翻转 Y 轴（图片原点在左上，OpenGL 坐标原点在左下）
    TexCoord = vec2(aTexCoord.x, 1.0 - aTexCoord.y);
}

// 片段着色器 (texture.frag)
#version 450 core
out vec4 FragColor;
in vec2 TexCoord;
uniform sampler2D uTexture;

void main() {
    FragColor = texture(uTexture, TexCoord);
}
```

```cpp
// C++ 代码
auto texture = std::make_shared<Texture>();
texture->LoadFromFile("textures/test.png", true);

auto shader = std::make_shared<Shader>();
shader->LoadFromFile("shaders/texture.vert", "shaders/texture.frag");

shader->Use();
shader->GetUniformManager()->SetInt("uTexture", 0);

texture->Bind(0);

// 渲染...
glBindVertexArray(VAO);
glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
```

---

## 注意事项

### 1. 纹理坐标翻转

大多数图片格式（PNG、JPG）的原点在左上角，而 OpenGL 纹理坐标原点在左下角。需要在着色器中翻转 Y 轴：

```glsl
TexCoord = vec2(aTexCoord.x, 1.0 - aTexCoord.y);
```

### 2. Mipmap

- 开启 Mipmap 可以提升远距离渲染质量，但会增加内存使用
- 对于 UI 纹理或不需要缩放的纹理，可以关闭 Mipmap
- Mipmap 纹理尺寸必须是 2 的幂（256、512、1024、2048 等）

### 3. 线程安全

**✅ `Texture` 类现在是线程安全的**（自 2025-10-28 更新）。

- 所有公共方法都使用互斥锁保护
- 可以在多线程环境中安全地访问纹理属性（`GetWidth`、`GetHeight`等）
- 可以从不同线程安全地调用 `Bind()`、`SetFilter()` 等方法
- 移动构造和赋值使用 `std::scoped_lock` 避免死锁

**注意事项**:
1. **OpenGL 上下文限制**: 虽然类本身是线程安全的，但 OpenGL 调用必须在创建上下文的线程中执行（通常是主线程）
2. **推荐使用 TextureLoader**: 在多线程环境中，建议通过 `TextureLoader` 管理纹理，它提供了更高级的缓存和管理功能

**线程安全示例**:
```cpp
// 主线程中加载纹理
auto texture = std::make_shared<Texture>();
texture->LoadFromFile("test.png", true);

// 工作线程中安全读取属性
std::thread worker([texture]() {
    int width = texture->GetWidth();   // 线程安全
    int height = texture->GetHeight(); // 线程安全
    bool valid = texture->IsValid();   // 线程安全
    
    // 注意：不要在工作线程中进行 OpenGL 调用
});

worker.join();
```

### 4. 生命周期管理

使用 `std::shared_ptr<Texture>`（`TexturePtr`）来管理纹理生命周期：

```cpp
std::shared_ptr<Texture> texture = std::make_shared<Texture>();
```

### 5. 2D 渲染设置

渲染 2D 纹理时需要禁用某些 3D 功能：

```cpp
glDisable(GL_CULL_FACE);   // 禁用面剔除
glDisable(GL_DEPTH_TEST);  // 禁用深度测试
```

---

## 性能建议

1. **纹理尺寸**: 优先使用 2 的幂次方尺寸
2. **Mipmap**: 对于会缩小的纹理，启用 Mipmap
3. **过滤模式**: 选择合适的过滤模式平衡性能和质量
4. **内存管理**: 及时释放不需要的纹理

---

## 常见错误

### 错误 1: 未绑定纹理单元

```cpp
// 错误
glActiveTexture(GL_TEXTURE0 unified code ... );  // 不需要，Bind() 会自动处理
texture->Bind();

// 正确
texture->Bind(0);  // Bind() 内部会调用 glActiveTexture 和 glBindTexture
```

### 错误 2: 纹理未生成 Mipmap 但使用了 Mipmap 过滤

```cpp
// 错误
texture->LoadFromFile("test.png", false);  // 不生成 Mipmap
texture->SetFilter(TextureFilter::Mipmap, TextureFilter::Linear);  // 需要 Mipmap

// 正确
texture->LoadFromFile("test.png", true);   // 生成 Mipmap
texture->SetFilter(TextureFilter::Mipmap, TextureFilter::Linear);
```

---

## 相关类

- **[TextureLoader](TextureLoader.md)** - 纹理加载器和缓存管理（推荐使用）
- **[Shader](Shader.md)** - 着色器程序管理
- **[UniformManager](UniformManager.md)** - Uniform 变量管理

---

## 参考

- [TextureLoader API](TextureLoader.md)
- [纹理系统使用指南](../TEXTURE_SYSTEM.md)
- [Shader API](Shader.md)
- [示例程序](../../examples/05_texture_test.cpp)

---

[返回 API 首页](README.md) | [上一篇: ShaderCache](ShaderCache.md) | [下一篇: TextureLoader](TextureLoader.md)

