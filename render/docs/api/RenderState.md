# RenderState API 参考

[返回 API 首页](README.md)

---

## 概述

`RenderState` 管理 OpenGL 渲染状态，缓存状态以减少不必要的状态切换，提高渲染性能。

**头文件**: `render/render_state.h`  
**命名空间**: `Render`

---

## 枚举类型

### BlendMode

混合模式枚举。

```cpp
enum class BlendMode {
    None,       // 无混合
    Alpha,      // Alpha 混合（透明）
    Additive,   // 加法混合（发光效果）
    Multiply,   // 乘法混合
    Custom      // 自定义混合
};
```

---

### DepthFunc

深度测试函数枚举。

```cpp
enum class DepthFunc {
    Never,          // 永不通过
    Less,           // 小于（默认）
    Equal,          // 等于
    LessEqual,      // 小于或等于
    Greater,        // 大于
    NotEqual,       // 不等于
    GreaterEqual,   // 大于或等于
    Always          // 总是通过
};
```

---

### CullFace

面剔除模式枚举。

```cpp
enum class CullFace {
    None,           // 无剔除
    Front,          // 剔除正面
    Back,           // 剔除背面（默认）
    FrontAndBack    // 剔除所有面
};
```

---

## 深度测试

### SetDepthTest

启用/禁用深度测试。

```cpp
void SetDepthTest(bool enable);
```

**示例**:
```cpp
// 3D 场景启用深度测试
renderState->SetDepthTest(true);

// 2D UI 禁用深度测试
renderState->SetDepthTest(false);
```

---

### SetDepthFunc

设置深度比较函数。

```cpp
void SetDepthFunc(DepthFunc func);
```

**示例**:
```cpp
// 标准 3D 渲染
renderState->SetDepthFunc(DepthFunc::Less);

// 天空盒渲染（总是通过）
renderState->SetDepthFunc(DepthFunc::Always);
```

---

### SetDepthWrite

启用/禁用深度写入。

```cpp
void SetDepthWrite(bool enable);
```

**示例**:
```cpp
// 渲染半透明物体前禁用深度写入
renderState->SetDepthWrite(false);
renderState->SetBlendMode(BlendMode::Alpha);
// 渲染半透明物体...

// 恢复深度写入
renderState->SetDepthWrite(true);
```

---

## 混合模式

### SetBlendMode

设置混合模式。

```cpp
void SetBlendMode(BlendMode mode);
```

**示例**:
```cpp
// Alpha 混合（透明度）
renderState->SetBlendMode(BlendMode::Alpha);

// 加法混合（发光效果）
renderState->SetBlendMode(BlendMode::Additive);

// 禁用混合
renderState->SetBlendMode(BlendMode::None);
```

---

### SetBlendFunc

设置自定义混合函数。

```cpp
void SetBlendFunc(uint32_t srcFactor, uint32_t dstFactor);
```

**参数**: OpenGL 混合因子（GL_ONE, GL_SRC_ALPHA 等）

**示例**:
```cpp
#include <glad/glad.h>

// 自定义混合
renderState->SetBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
```

---

## 面剔除

### SetCullFace

设置面剔除模式。

```cpp
void SetCullFace(CullFace mode);
```

**示例**:
```cpp
// 剔除背面（标准）
renderState->SetCullFace(CullFace::Back);

// 双面渲染
renderState->SetCullFace(CullFace::None);

// 内部渲染（如天空盒）
renderState->SetCullFace(CullFace::Front);
```

---

## 视口和裁剪

### SetViewport

设置渲染视口。

```cpp
void SetViewport(int x, int y, int width, int height);
```

**示例**:
```cpp
// 全屏视口
renderState->SetViewport(0, 0, windowWidth, windowHeight);

// 分屏渲染（左半屏）
renderState->SetViewport(0, 0, windowWidth / 2, windowHeight);
```

---

### SetScissorTest

启用/禁用裁剪测试。

```cpp
void SetScissorTest(bool enable);
```

---

### SetScissorRect

设置裁剪区域。

```cpp
void SetScissorRect(int x, int y, int width, int height);
```

**示例**:
```cpp
// 启用裁剪
renderState->SetScissorTest(true);
renderState->SetScissorRect(100, 100, 200, 200);
// 只在 (100,100) 到 (300,300) 区域渲染

// 禁用裁剪
renderState->SetScissorTest(false);
```

---

## 清屏

### SetClearColor

设置清屏颜色。

```cpp
void SetClearColor(const Color& color);
```

**示例**:
```cpp
renderState->SetClearColor(Color(0.1f, 0.1f, 0.15f, 1.0f));
```

---

### Clear

清空缓冲区。

```cpp
void Clear(bool colorBuffer = true, 
          bool depthBuffer = true, 
          bool stencilBuffer = false);
```

**示例**:
```cpp
// 清空所有
renderState->Clear();

// 只清空深度
renderState->Clear(false, true, false);
```

---

## 状态管理

### Reset

重置所有状态为默认值。

```cpp
void Reset();
```

**示例**:
```cpp
// 场景切换时重置状态
renderState->Reset();
```

---

### GetBlendMode / GetCullFace

获取当前状态。

```cpp
BlendMode GetBlendMode() const;
CullFace GetCullFace() const;
```

---

## 纹理绑定管理

### BindTexture

绑定纹理到指定纹理单元。

```cpp
void BindTexture(uint32_t unit, uint32_t textureId, uint32_t target = GL_TEXTURE_2D);
```

**参数**:
- `unit`: 纹理单元索引 (0-31)
- `textureId`: OpenGL 纹理 ID
- `target`: 纹理目标 (默认为 GL_TEXTURE_2D)

**特性**:
- 自动缓存，避免重复绑定
- 自动切换纹理单元
- 支持最多 32 个纹理单元

**示例**:
```cpp
// 绑定纹理到纹理单元 0
state->BindTexture(0, textureId1);

// 绑定多个纹理
state->BindTexture(0, diffuseTexture);
state->BindTexture(1, normalTexture);
state->BindTexture(2, specularTexture);

// 绑定立方体贴图
state->BindTexture(0, cubemapId, GL_TEXTURE_CUBE_MAP);
```

---

### UnbindTexture

解绑指定纹理单元。

```cpp
void UnbindTexture(uint32_t unit, uint32_t target = GL_TEXTURE_2D);
```

**示例**:
```cpp
state->UnbindTexture(0);
```

---

### SetActiveTextureUnit

设置当前活动纹理单元（用于高级操作）。

```cpp
void SetActiveTextureUnit(uint32_t unit);
```

---

### GetBoundTexture

获取当前绑定的纹理。

```cpp
uint32_t GetBoundTexture(uint32_t unit) const;
```

**示例**:
```cpp
uint32_t currentTexture = state->GetBoundTexture(0);
```

---

## 缓冲区绑定管理

### BufferTarget 枚举

```cpp
enum class BufferTarget {
    ArrayBuffer,        // VBO (顶点缓冲区)
    ElementArrayBuffer, // EBO (索引缓冲区)
    UniformBuffer,      // UBO (Uniform缓冲区)
    ShaderStorageBuffer // SSBO (着色器存储缓冲区)
};
```

---

### BindVertexArray

绑定 VAO (顶点数组对象)。

```cpp
void BindVertexArray(uint32_t vaoId);
```

**示例**:
```cpp
// 绑定 VAO
state->BindVertexArray(vaoId);

// 渲染操作...

// 解绑
state->BindVertexArray(0);
```

---

### BindBuffer

绑定缓冲区。

```cpp
void BindBuffer(BufferTarget target, uint32_t bufferId);
```

**示例**:
```cpp
// 绑定 VBO
state->BindBuffer(BufferTarget::ArrayBuffer, vboId);

// 绑定 EBO
state->BindBuffer(BufferTarget::ElementArrayBuffer, eboId);

// 绑定 UBO
state->BindBuffer(BufferTarget::UniformBuffer, uboId);
```

---

### GetBoundVertexArray

获取当前绑定的 VAO。

```cpp
uint32_t GetBoundVertexArray() const;
```

---

### GetBoundBuffer

获取当前绑定的缓冲区。

```cpp
uint32_t GetBoundBuffer(BufferTarget target) const;
```

**示例**:
```cpp
uint32_t currentVBO = state->GetBoundBuffer(BufferTarget::ArrayBuffer);
```

---

## 着色器程序管理

### UseProgram

绑定着色器程序。

```cpp
void UseProgram(uint32_t programId);
```

**特性**:
- 自动缓存，避免重复切换
- 配合 UniformManager 使用效果更佳

**示例**:
```cpp
// 使用着色器程序
state->UseProgram(shaderProgramId);

// 设置 uniforms...

// 渲染...
```

---

### GetCurrentProgram

获取当前使用的着色器程序。

```cpp
uint32_t GetCurrentProgram() const;
```

**示例**:
```cpp
uint32_t currentShader = state->GetCurrentProgram();
```

---

## 完整示例

### 3D 场景渲染

```cpp
RenderState* state = renderer->GetRenderState();

// 不透明物体
state->SetDepthTest(true);
state->SetDepthWrite(true);
state->SetDepthFunc(DepthFunc::Less);
state->SetBlendMode(BlendMode::None);
state->SetCullFace(CullFace::Back);

// 渲染不透明物体...

// 半透明物体
state->SetDepthWrite(false);  // 禁用深度写入
state->SetBlendMode(BlendMode::Alpha);

// 渲染半透明物体...

// 恢复状态
state->SetDepthWrite(true);
state->SetBlendMode(BlendMode::None);
```

---

### UI 渲染

```cpp
// 保存 3D 状态
auto oldBlend = state->GetBlendMode();
auto oldCull = state->GetCullFace();

// UI 状态
state->SetDepthTest(false);
state->SetBlendMode(BlendMode::Alpha);
state->SetCullFace(CullFace::None);

// 渲染 UI...

// 恢复状态
state->SetDepthTest(true);
state->SetBlendMode(oldBlend);
state->SetCullFace(oldCull);
```

---

### 完整渲染流程（包含新功能）

```cpp
// 1. 设置着色器程序
state->UseProgram(shaderProgramId);

// 2. 绑定纹理
state->BindTexture(0, diffuseMap);
state->BindTexture(1, normalMap);

// 3. 设置渲染状态
state->SetDepthTest(true);
state->SetDepthFunc(DepthFunc::Less);
state->SetBlendMode(BlendMode::None);
state->SetCullFace(CullFace::Back);

// 4. 绑定 VAO 和缓冲区
state->BindVertexArray(vaoId);
state->BindBuffer(BufferTarget::ElementArrayBuffer, eboId);

// 5. 渲染
glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);

// 6. 清理（可选）
state->BindVertexArray(0);
state->UnbindTexture(0);
state->UnbindTexture(1);
```

---

### 多纹理渲染

```cpp
// 设置着色器
state->UseProgram(multiTexShader);

// 绑定多个纹理
state->BindTexture(0, baseColor);
state->BindTexture(1, normalMap);
state->BindTexture(2, metallic);
state->BindTexture(3, roughness);
state->BindTexture(4, aoMap);

// 设置纹理 uniform（通过 UniformManager）
shader->GetUniformManager()->SetInt("u_BaseColor", 0);
shader->GetUniformManager()->SetInt("u_NormalMap", 1);
shader->GetUniformManager()->SetInt("u_Metallic", 2);
shader->GetUniformManager()->SetInt("u_Roughness", 3);
shader->GetUniformManager()->SetInt("u_AO", 4);

// 渲染...
```

---

### 批量渲染优化

```cpp
// 按材质分组渲染，减少状态切换
for (auto& batch : renderBatches) {
    // 只在材质变化时切换状态
    if (batch.material != currentMaterial) {
        state->UseProgram(batch.shader);
        state->BindTexture(0, batch.texture);
        state->SetBlendMode(batch.blendMode);
        currentMaterial = batch.material;
    }
    
    // 渲染批次中的所有对象
    for (auto& mesh : batch.meshes) {
        state->BindVertexArray(mesh.vao);
        glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
    }
}
```

---

## 性能提示

1. **状态缓存**: `RenderState` 自动缓存，避免重复设置
2. **批处理**: 按状态分组渲染对象，减少状态切换
3. **状态排序**: 按 shader → 纹理 → 材质 → 网格 的顺序排序
4. **纹理绑定**: 避免频繁切换纹理单元
5. **VAO 使用**: 使用 VAO 减少顶点属性设置开销
6. **着色器切换**: 着色器切换开销较大，尽量减少切换次数

### 状态切换成本排序（从高到低）

1. **着色器程序** (UseProgram) - 开销最大
2. **帧缓冲** (FBO)
3. **纹理绑定** (BindTexture)
4. **VAO 切换** (BindVertexArray)
5. **混合/深度状态** (SetBlendMode, SetDepthTest)
6. **Uniform 设置** - 开销最小

---

## 新增功能总结

### 纹理管理
- ✅ 多纹理单元支持（最多32个）
- ✅ 自动纹理单元切换
- ✅ 纹理绑定缓存

### 缓冲区管理
- ✅ VAO 绑定管理
- ✅ VBO/EBO 绑定管理
- ✅ UBO/SSBO 支持

### 着色器管理
- ✅ 着色器程序绑定缓存
- ✅ 避免重复 glUseProgram 调用

---

[上一篇: UniformManager](UniformManager.md) | [下一篇: OpenGLContext](OpenGLContext.md)

