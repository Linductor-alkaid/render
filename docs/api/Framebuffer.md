# Framebuffer API 参考

[返回 API 首页](README.md)

---

## 概述

`Framebuffer` 类管理 OpenGL 帧缓冲对象 (FBO)，实现离屏渲染、后处理效果、阴影映射等高级渲染技术。

**头文件**: `render/framebuffer.h`  
**命名空间**: `Render`

### 🔒 线程安全

**所有公共方法都是线程安全的**，可以从多个线程安全调用。内部使用互斥锁保护所有可变状态。

⚠️ **重要限制**：OpenGL 调用必须在创建上下文的线程（通常是主线程）中执行。

### ✨ 功能特性

- ✅ **多种附件类型**：支持颜色、深度、模板附件
- ✅ **多重采样抗锯齿 (MSAA)**：支持 1-16x MSAA
- ✅ **多渲染目标 (MRT)**：最多 8 个颜色附件
- ✅ **纹理和渲染缓冲对象**：灵活选择附件类型
- ✅ **动态调整大小**：运行时调整帧缓冲尺寸
- ✅ **Blit 操作**：快速复制到其他帧缓冲
- ✅ **完整性检查**：自动验证帧缓冲状态
- ✅ **线程安全**：所有操作都有互斥锁保护

---

## 类定义

```cpp
class Framebuffer {
public:
    Framebuffer();
    ~Framebuffer();
    
    bool Create(const FramebufferConfig& config);
    bool Resize(int width, int height);
    
    void Bind() const;
    void Unbind() const;
    void BindRead() const;
    void BindDraw() const;
    void Clear(bool colorBuffer = true, bool depthBuffer = true, bool stencilBuffer = false) const;
    void BlitTo(Framebuffer* dest, GLbitfield mask = GL_COLOR_BUFFER_BIT, GLenum filter = GL_NEAREST) const;
    void Release();
    
    // 查询方法...
};
```

---

## 枚举类型

### FramebufferAttachmentType

帧缓冲附件类型。

```cpp
enum class FramebufferAttachmentType {
    Color0 = 0,     // 颜色附件 0
    Color1,         // 颜色附件 1
    Color2,         // 颜色附件 2
    Color3,         // 颜色附件 3
    Color4,         // 颜色附件 4
    Color5,         // 颜色附件 5
    Color6,         // 颜色附件 6
    Color7,         // 颜色附件 7
    Depth,          // 深度附件
    Stencil,        // 模板附件
    DepthStencil    // 深度模板组合附件
};
```

---

## 配置结构

### FramebufferAttachment

附件配置结构。

```cpp
struct FramebufferAttachment {
    FramebufferAttachmentType type;
    TextureFormat format;
    TextureFilter minFilter;
    TextureFilter magFilter;
    TextureWrap wrapS;
    TextureWrap wrapT;
    bool useRenderbuffer;  // 使用 RBO（不能采样）或纹理（可采样）
    
    // 静态工厂方法
    static FramebufferAttachment Color(int index = 0, 
                                       TextureFormat format = TextureFormat::RGBA,
                                       bool useRBO = false);
    static FramebufferAttachment Depth(bool useRBO = false);
    static FramebufferAttachment DepthStencil(bool useRBO = true);
};
```

---

### FramebufferConfig

帧缓冲配置结构（构建器模式）。

```cpp
struct FramebufferConfig {
    int width;
    int height;
    std::vector<FramebufferAttachment> attachments;
    int samples;    // MSAA 采样数（1 = 无 MSAA）
    std::string name;
    
    // 构建器方法
    FramebufferConfig& AddColorAttachment(TextureFormat format = TextureFormat::RGBA, bool useRBO = false);
    FramebufferConfig& AddDepthAttachment(bool useRBO = false);
    FramebufferConfig& AddDepthStencilAttachment(bool useRBO = true);
    FramebufferConfig& SetSize(int w, int h);
    FramebufferConfig& SetSamples(int s);
    FramebufferConfig& SetName(const std::string& n);
};
```

**示例**:
```cpp
FramebufferConfig config;
config.SetSize(1920, 1080)
      .SetSamples(4)  // 4x MSAA
      .SetName("MyFramebuffer")
      .AddColorAttachment(TextureFormat::RGBA)
      .AddDepthAttachment();
```

---

## 创建和管理

### Create

从配置创建帧缓冲。

```cpp
bool Create(const FramebufferConfig& config);
```

**参数**:
- `config` - 帧缓冲配置

**返回值**: 成功返回 `true`，失败返回 `false`

**示例**:
```cpp
auto framebuffer = std::make_shared<Framebuffer>();

FramebufferConfig config;
config.SetSize(1280, 720)
      .AddColorAttachment()
      .AddDepthAttachment();

if (!framebuffer->Create(config)) {
    LOG_ERROR("Failed to create framebuffer");
}
```

---

### Resize

调整帧缓冲大小。

```cpp
bool Resize(int width, int height);
```

**参数**:
- `width` - 新宽度
- `height` - 新高度

**返回值**: 成功返回 `true`

**说明**: 会重新创建所有附件

**示例**:
```cpp
// 窗口大小改变时
framebuffer->Resize(newWidth, newHeight);
```

---

### Release

释放帧缓冲资源。

```cpp
void Release();
```

**说明**: 析构函数会自动调用

---

## 绑定操作

### Bind

绑定帧缓冲为当前渲染目标。

```cpp
void Bind() const;
```

**示例**:
```cpp
framebuffer->Bind();
// 渲染到帧缓冲...
framebuffer->Unbind();
```

---

### Unbind

解绑帧缓冲（绑定默认帧缓冲，即屏幕）。

```cpp
void Unbind() const;
```

---

### BindRead / BindDraw

分别绑定为读取或绘制帧缓冲。

```cpp
void BindRead() const;
void BindDraw() const;
```

**使用场景**: Blit 操作时需要分别指定源和目标

**示例**:
```cpp
srcFramebuffer->BindRead();
dstFramebuffer->BindDraw();
glBlitFramebuffer(...);
```

---

## 渲染操作

### Clear

清空帧缓冲。

```cpp
void Clear(bool colorBuffer = true, 
          bool depthBuffer = true, 
          bool stencilBuffer = false) const;
```

**参数**:
- `colorBuffer` - 是否清空颜色缓冲
- `depthBuffer` - 是否清空深度缓冲
- `stencilBuffer` - 是否清空模板缓冲

**示例**:
```cpp
framebuffer->Bind();
framebuffer->Clear();  // 清空所有
// 渲染...
```

---

### BlitTo

将此帧缓冲内容复制到另一个帧缓冲（或屏幕）。

```cpp
void BlitTo(Framebuffer* dest, 
           GLbitfield mask = GL_COLOR_BUFFER_BIT,
           GLenum filter = GL_NEAREST) const;
```

**参数**:
- `dest` - 目标帧缓冲（`nullptr` 表示屏幕）
- `mask` - 复制掩码
  - `GL_COLOR_BUFFER_BIT` - 复制颜色
  - `GL_DEPTH_BUFFER_BIT` - 复制深度
  - `GL_STENCIL_BUFFER_BIT` - 复制模板
- `filter` - 过滤模式
  - `GL_NEAREST` - 最近邻（快速）
  - `GL_LINEAR` - 线性插值（平滑）

**使用场景**: MSAA 解析、后处理、屏幕输出

**示例**:
```cpp
// 将 MSAA 帧缓冲解析到普通帧缓冲
msaaFramebuffer->BlitTo(resolveFramebuffer.get(), 
                        GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT,
                        GL_NEAREST);

// 将帧缓冲内容输出到屏幕
framebuffer->BlitTo(nullptr);
```

---

## 查询方法

### GetID

获取 OpenGL 帧缓冲 ID。

```cpp
GLuint GetID() const;
```

**返回值**: FBO ID

---

### GetWidth / GetHeight

获取帧缓冲尺寸。

```cpp
int GetWidth() const;
int GetHeight() const;
```

**示例**:
```cpp
int width = framebuffer->GetWidth();
int height = framebuffer->GetHeight();
```

---

### GetSamples

获取多重采样数量。

```cpp
int GetSamples() const;
```

**返回值**: 采样数量（1 表示无 MSAA）

---

### BindColorAttachment

绑定颜色附件纹理到指定纹理单元。

```cpp
void BindColorAttachment(int index = 0, unsigned int unit = 0) const;
```

**参数**:
- `index` - 附件索引（0-7）
- `unit` - 纹理单元（0-31）

**说明**: 
- 如果使用渲染缓冲对象则无操作
- 自动处理多重采样纹理和普通纹理

**示例**:
```cpp
// 绑定颜色附件0到纹理单元0
framebuffer->BindColorAttachment(0, 0);

// 在着色器中采样
shader->GetUniformManager()->SetInt("screenTexture", 0);
```

---

### GetColorAttachmentID

获取颜色附件的OpenGL纹理ID。

```cpp
GLuint GetColorAttachmentID(int index = 0) const;
```

**参数**:
- `index` - 附件索引（0-7）

**返回值**: OpenGL纹理ID，不存在返回0

**使用场景**: 需要直接访问纹理ID时

---

### IsValid

检查帧缓冲是否有效。

```cpp
bool IsValid() const;
```

**返回值**: FBO ID 不为 0 时返回 `true`

---

### IsComplete

检查帧缓冲是否完整。

```cpp
bool IsComplete() const;
```

**返回值**: 帧缓冲配置正确且完整时返回 `true`

**示例**:
```cpp
if (!framebuffer->IsComplete()) {
    LOG_ERROR("Framebuffer is not complete: " + framebuffer->GetStatusString());
}
```

---

### GetStatusString

获取帧缓冲状态描述。

```cpp
std::string GetStatusString() const;
```

**返回值**: 状态字符串（"Complete"、"Incomplete Attachment" 等）

---

### GetName / SetName

获取/设置调试名称。

```cpp
const std::string& GetName() const;
void SetName(const std::string& name);
```

---

### IsMultisampled

检查是否使用多重采样。

```cpp
bool IsMultisampled() const;
```

---

### GetColorAttachmentCount

获取颜色附件数量。

```cpp
int GetColorAttachmentCount() const;
```

---

## 使用示例

### 基础离屏渲染

```cpp
#include <render/framebuffer.h>
#include <render/renderer.h>

// 创建帧缓冲
auto framebuffer = std::make_shared<Framebuffer>();

FramebufferConfig config;
config.SetSize(1280, 720)
      .SetName("OffscreenBuffer")
      .AddColorAttachment()
      .AddDepthAttachment();

if (!framebuffer->Create(config)) {
    LOG_ERROR("Failed to create framebuffer");
    return;
}

// 主循环
while (running) {
    // 第一步：渲染到帧缓冲
    framebuffer->Bind();
    framebuffer->Clear();
    
    RenderScene();  // 渲染场景
    
    framebuffer->Unbind();
    
    // 第二步：使用帧缓冲纹理渲染到屏幕
    renderer->Clear();
    
    framebuffer->BindColorAttachment(0, 0);
    
    screenShader->Use();
    screenShader->GetUniformManager()->SetInt("screenTexture", 0);
    screenQuad->Draw();
    
    renderer->Present();
}
```

---

### MSAA 抗锯齿

```cpp
// 创建 MSAA 帧缓冲
auto msaaFramebuffer = std::make_shared<Framebuffer>();

FramebufferConfig msaaConfig;
msaaConfig.SetSize(1920, 1080)
          .SetSamples(4)  // 4x MSAA
          .SetName("MSAA Buffer")
          .AddColorAttachment(TextureFormat::RGBA, true)  // 使用 RBO
          .AddDepthAttachment(true);  // 使用 RBO

msaaFramebuffer->Create(msaaConfig);

// 创建解析目标帧缓冲（普通纹理，可采样）
auto resolveFramebuffer = std::make_shared<Framebuffer>();

FramebufferConfig resolveConfig;
resolveConfig.SetSize(1920, 1080)
             .SetName("Resolve Buffer")
             .AddColorAttachment(TextureFormat::RGBA, false);  // 使用纹理

resolveFramebuffer->Create(resolveConfig);

// 渲染
msaaFramebuffer->Bind();
msaaFramebuffer->Clear();
RenderScene();
msaaFramebuffer->Unbind();

// 解析 MSAA 到普通纹理
msaaFramebuffer->BlitTo(resolveFramebuffer.get(), 
                        GL_COLOR_BUFFER_BIT,
                        GL_NEAREST);

// 使用解析后的纹理
resolveFramebuffer->BindColorAttachment(0, 0);
```

---

### 多渲染目标 (MRT)

```cpp
// 创建 MRT 帧缓冲
auto mrtFramebuffer = std::make_shared<Framebuffer>();

FramebufferConfig mrtConfig;
mrtConfig.SetSize(1920, 1080)
         .SetName("MRT Buffer")
         .AddColorAttachment(TextureFormat::RGBA)   // 颜色
         .AddColorAttachment(TextureFormat::RGBA)   // 法线
         .AddColorAttachment(TextureFormat::RGBA)   // 位置
         .AddDepthAttachment();

mrtFramebuffer->Create(mrtConfig);

// 着色器中输出多个目标
/*
// Fragment Shader
layout (location = 0) out vec4 gColor;
layout (location = 1) out vec4 gNormal;
layout (location = 2) out vec4 gPosition;

void main() {
    gColor = texture(diffuseMap, TexCoords);
    gNormal = vec4(normalize(Normal), 1.0);
    gPosition = vec4(FragPos, 1.0);
}
*/

// 渲染
mrtFramebuffer->Bind();
mrtFramebuffer->Clear();
RenderSceneGeometry();
mrtFramebuffer->Unbind();

// 绑定所有附件用于后续处理
mrtFramebuffer->BindColorAttachment(0, 0);  // 颜色
mrtFramebuffer->BindColorAttachment(1, 1);  // 法线
mrtFramebuffer->BindColorAttachment(2, 2);  // 位置

// 延迟渲染光照计算
lightingShader->Use();
lightingShader->GetUniformManager()->SetInt("gColor", 0);
lightingShader->GetUniformManager()->SetInt("gNormal", 1);
lightingShader->GetUniformManager()->SetInt("gPosition", 2);
RenderLighting();
```

---

### 后处理效果链

```cpp
class PostProcessChain {
public:
    void Setup(int width, int height) {
        // 两个帧缓冲用于乒乓交换
        for (int i = 0; i < 2; i++) {
            buffers[i] = std::make_shared<Framebuffer>();
            
            FramebufferConfig config;
            config.SetSize(width, height)
                  .SetName("PostProcess " + std::to_string(i))
                  .AddColorAttachment();
            
            buffers[i]->Create(config);
        }
    }
    
    void Apply(const std::vector<Ref<Shader>>& effects) {
        int current = 0;
        
        for (size_t i = 0; i < effects.size(); i++) {
            int next = (current + 1) % 2;
            
            buffers[next]->Bind();
            buffers[next]->Clear();
            
            auto inputTexture = buffers[current]->GetColorAttachment(0);
            inputTexture->Bind(0);
            
            effects[i]->Use();
            effects[i]->GetUniformManager()->SetInt("inputTexture", 0);
            RenderQuad();
            
            buffers[next]->Unbind();
            
            current = next;
        }
        
        // 最终结果输出到屏幕
        buffers[current]->BlitTo(nullptr);
    }
    
private:
    std::shared_ptr<Framebuffer> buffers[2];
};

// 使用
PostProcessChain chain;
chain.Setup(1920, 1080);

std::vector<Ref<Shader>> effects = {
    blurShader,
    bloomShader,
    tonemapShader
};

chain.Apply(effects);
```

---

### 阴影映射

```cpp
// 创建阴影贴图
auto shadowMap = std::make_shared<Framebuffer>();

FramebufferConfig shadowConfig;
shadowConfig.SetSize(2048, 2048)  // 高分辨率
            .SetName("Shadow Map")
            .AddDepthAttachment(false);  // 深度纹理，可采样

shadowMap->Create(shadowConfig);

// 第一步：从光源视角渲染深度
shadowMap->Bind();
shadowMap->Clear(false, true, false);  // 只清空深度

depthShader->Use();
depthShader->GetUniformManager()->SetMatrix4("lightSpaceMatrix", lightSpaceMatrix);

RenderScene();

shadowMap->Unbind();

// 第二步：正常渲染，使用阴影贴图
// 注意：深度附件如果使用纹理可以绑定
GLuint depthTexID = shadowMap->GetColorAttachmentID(0);  // 如果深度用的是颜色附件
if (depthTexID != 0) {
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, depthTexID);
}

sceneShader->Use();
sceneShader->GetUniformManager()->SetInt("shadowMap", 1);
sceneShader->GetUniformManager()->SetMatrix4("lightSpaceMatrix", lightSpaceMatrix);

RenderScene();
```

---

### 窗口大小调整

```cpp
class Application {
public:
    void OnWindowResize(int width, int height) {
        // 调整帧缓冲大小
        if (framebuffer) {
            framebuffer->Resize(width, height);
        }
        
        // 更新视口
        renderer->GetRenderState()->SetViewport(0, 0, width, height);
        
        // 更新相机宽高比
        camera.SetAspectRatio(static_cast<float>(width) / height);
    }
    
private:
    std::shared_ptr<Framebuffer> framebuffer;
};
```

---

## 性能建议

### 1. 选择合适的附件类型

```cpp
// ✅ 好：深度附件不需要采样时使用 RBO（更快）
config.AddDepthAttachment(true);  // 使用 RBO

// ⚠️ 差：如果需要采样深度，必须使用纹理
config.AddDepthAttachment(false);  // 使用纹理
```

### 2. MSAA 性能

```cpp
// MSAA 会显著增加内存和带宽消耗
// 移动设备建议 2x，桌面设备建议 4x

// 移动设备
config.SetSamples(2);

// 桌面设备
config.SetSamples(4);
```

### 3. 纹理格式选择

```cpp
// ✅ 好：根据需求选择格式
config.AddColorAttachment(TextureFormat::RGB);   // 省内存
config.AddColorAttachment(TextureFormat::RGBA);  // 需要透明

// ⚠️ 差：总是使用最高精度
config.AddColorAttachment(TextureFormat::RGBA);  // 不必要时浪费内存
```

### 4. 避免频繁重建

```cpp
// ✅ 好：使用 Resize 而不是重新创建
framebuffer->Resize(newWidth, newHeight);

// ⚠️ 差：每次都重新创建
framebuffer->Release();
framebuffer->Create(config);
```

### 5. Blit 优化

```cpp
// ✅ 好：同尺寸使用 NEAREST（快速）
sameSizeFramebuffer->BlitTo(dest, GL_COLOR_BUFFER_BIT, GL_NEAREST);

// ⚠️ 差：不同尺寸仍使用 NEAREST（质量差）
// ✅ 应使用：
differentSizeFramebuffer->BlitTo(dest, GL_COLOR_BUFFER_BIT, GL_LINEAR);
```

---

## 常见问题

### 帧缓冲不完整

**问题**: `IsComplete()` 返回 `false`

**解决方法**:
1. 检查所有附件尺寸是否一致
2. 至少有一个附件
3. 颜色附件格式支持渲染
4. 深度/模板格式正确

```cpp
if (!framebuffer->IsComplete()) {
    LOG_ERROR("Framebuffer status: " + framebuffer->GetStatusString());
}
```

### MSAA 纹理不能直接采样

**问题**: 多重采样纹理不能在着色器中直接采样

**解决方法**: 使用 `BlitTo()` 解析到普通纹理

```cpp
msaaFramebuffer->BlitTo(resolveFramebuffer.get());
auto texture = resolveFramebuffer->GetColorAttachment(0);
texture->Bind(0);  // 现在可以采样了
```

### 内存不足

**问题**: 创建大尺寸或多个帧缓冲导致内存不足

**解决方法**:
1. 降低分辨率
2. 减少MSAA采样数
3. 使用 RBO 代替纹理（如果不需要采样）
4. 及时释放不用的帧缓冲

---

## 线程安全

### 概述

`Framebuffer` 类全面实现线程安全，所有公共方法都可以从多个线程安全调用。

### 保证

✅ **线程安全保证**：
- 所有公共方法使用互斥锁保护
- 移动操作使用 `std::scoped_lock` 避免死锁
- 可以从多个线程同时调用不同方法

### 限制

⚠️ **OpenGL 限制**：
- OpenGL 调用必须在创建上下文的线程中执行
- 这是 OpenGL 的固有限制

### 最佳实践

```cpp
// ✅ 推荐：在主渲染线程中使用
framebuffer->Bind();
framebuffer->Clear();
RenderScene();
framebuffer->Unbind();

// ✅ 安全：从其他线程查询属性
std::thread worker([&]() {
    int width = framebuffer->GetWidth();    // 线程安全
    int height = framebuffer->GetHeight();  // 线程安全
    bool valid = framebuffer->IsValid();    // 线程安全
});
```

---

## 注意事项

1. **线程安全**: 所有方法都是线程安全的，但 OpenGL 调用必须在主线程
2. **资源管理**: 使用智能指针管理帧缓冲生命周期
3. **完整性检查**: 创建后务必检查 `IsComplete()`
4. **MSAA 解析**: 多重采样帧缓冲需要 Blit 到普通帧缓冲才能采样
5. **附件类型**: RBO 性能更好但不能采样，纹理可以采样但开销更大
6. **尺寸限制**: 帧缓冲尺寸受 GPU 限制，通常最大 16384x16384
7. **MRT 数量**: 最多支持 8 个颜色附件（受 GPU 限制）
8. **⭐ Y轴翻转**: 渲染帧缓冲到屏幕时需要翻转Y轴（见下文详细说明）

---

## ⭐ 重要：帧缓冲Y轴翻转问题

### 问题原因

OpenGL 帧缓冲纹理的坐标系统原点在**左下角**，而常规图片文件原点在左上角。当将帧缓冲内容渲染到屏幕时，如果不翻转Y轴，画面会上下颠倒。

### 标准解决方案（推荐）

**使用提供的屏幕着色器**（已内置Y轴翻转）：

**顶点着色器** (`shaders/screen.vert`):
```glsl
#version 450 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

void main() {
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
    // 关键：翻转Y轴
    TexCoord = vec2(aTexCoord.x, 1.0 - aTexCoord.y);
}
```

**片段着色器** (`shaders/screen.frag`):
```glsl
#version 450 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D uTexture;

void main() {
    FragColor = texture(uTexture, TexCoord);
}
```

**使用示例**:
```cpp
// 使用标准屏幕着色器，自动处理Y轴翻转
auto screenShader = shaderCache.LoadShader("screen",
    "shaders/screen.vert",
    "shaders/screen.frag");

// 渲染到屏幕
framebuffer->BindColorAttachment(0, 0);
screenShader->Use();
screenShader->GetUniformManager()->SetInt("uTexture", 0);
screenQuad->Draw();  // ✅ 显示正确
```

### 自定义着色器

如果您使用自定义着色器，需要手动添加Y轴翻转：

```glsl
// 在顶点着色器中
out vec2 TexCoord;
void main() {
    // ... 其他代码 ...
    TexCoord = vec2(aTexCoord.x, 1.0 - aTexCoord.y);  // 翻转Y轴
}
```

### 何时需要翻转

| 使用场景 | 是否需要翻转 | 说明 |
|---------|------------|------|
| 帧缓冲 → 屏幕 | ✅ 需要 | 渲染最终结果到窗口 |
| 帧缓冲 → 帧缓冲 | ❌ 不需要 | 后处理链中的中间步骤 |
| 帧缓冲 → 纹理采样 | ❌ 不需要 | 作为普通纹理使用（如阴影贴图）|

### 错误示例

```cpp
// ❌ 错误：使用普通纹理着色器，画面上下颠倒
framebuffer->BindColorAttachment(0, 0);
normalTextureShader->Use();  // 没有Y轴翻转
screenQuad->Draw();  // 画面颠倒

// ✅ 正确：使用screen着色器
framebuffer->BindColorAttachment(0, 0);
screenShader->Use();  // 内置Y轴翻转
screenQuad->Draw();  // 画面正常
```

---

## 相关文档

### API 文档
- [Texture API](Texture.md)
- [Shader API](Shader.md)
- [RenderState API](RenderState.md)
- [Renderer API](Renderer.md)

### 使用指南
- [开发指南](../DEVELOPMENT_GUIDE.md)
- [架构文档](../ARCHITECTURE.md)

---

[上一篇: Texture](Texture.md) | [返回 API 首页](README.md)

