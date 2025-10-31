# RenderState API 参考

[返回 API 首页](README.md)

---

## 概述

`RenderState` 管理 OpenGL 渲染状态，缓存状态以减少不必要的状态切换，提高渲染性能。

**头文件**: `render/render_state.h`  
**命名空间**: `Render`

### 线程安全性

✅ **所有方法都是线程安全的**

- 使用 `std::shared_mutex` 实现读写锁
- 读操作（Get*）支持并发访问
- 写操作（Set*, Bind*, Use*）独占访问
- 自动管理锁的获取和释放

⚠️ **OpenGL 上下文注意事项**

虽然 `RenderState` 本身是线程安全的，但 OpenGL 上下文通常要求在创建它的线程中使用。建议：
- 在渲染线程中调用所有 OpenGL 相关方法
- 多线程场景下，可以安全地从不同线程读取状态
- 状态修改应在单个渲染线程中进行

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

## 缓存同步管理

### 问题背景

`RenderState` 使用内部缓存来优化性能，避免重复的 OpenGL 状态切换。但如果外部代码直接调用 OpenGL API（例如使用第三方库），内部缓存可能与 OpenGL 实际状态不同步，导致后续渲染出错。

---

### InvalidateCache

使所有状态缓存失效。

```cpp
void InvalidateCache();
```

**使用场景**:
- 在使用第三方 OpenGL 库后（如 ImGui、Dear ImGui）
- 在直接调用 OpenGL API 后
- 在 OpenGL 上下文切换后
- 当怀疑缓存与实际状态不一致时

**示例**:
```cpp
// 使用第三方库前保存状态
auto savedState = renderer->GetRenderState();

// 使用第三方 OpenGL 库
ImGui::Render();
ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

// 清空缓存，因为第三方库可能改变了 OpenGL 状态
savedState->InvalidateCache();

// 继续渲染，此时状态会重新同步
savedState->SetDepthTest(true);
```

---

### InvalidateTextureCache

仅使纹理绑定缓存失效。

```cpp
void InvalidateTextureCache();
```

**示例**:
```cpp
// 外部代码直接绑定了纹理
glBindTexture(GL_TEXTURE_2D, someTexture);

// 清空纹理缓存
state->InvalidateTextureCache();

// 继续使用 RenderState
state->BindTexture(0, myTexture);
```

---

### InvalidateBufferCache

仅使缓冲区绑定缓存失效。

```cpp
void InvalidateBufferCache();
```

**示例**:
```cpp
// 外部代码直接绑定了 VAO/VBO
glBindVertexArray(someVAO);
glBindBuffer(GL_ARRAY_BUFFER, someVBO);

// 清空缓冲区缓存
state->InvalidateBufferCache();
```

---

### InvalidateShaderCache

仅使着色器程序缓存失效。

```cpp
void InvalidateShaderCache();
```

**示例**:
```cpp
// 外部代码直接使用了着色器
glUseProgram(someProgram);

// 清空着色器缓存
state->InvalidateShaderCache();
```

---

### InvalidateRenderStateCache

仅使渲染状态缓存失效（深度测试、混合模式等）。

```cpp
void InvalidateRenderStateCache();
```

**示例**:
```cpp
// 外部代码直接改变了渲染状态
glDisable(GL_DEPTH_TEST);
glEnable(GL_BLEND);

// 清空渲染状态缓存
state->InvalidateRenderStateCache();
```

---

### SyncFromGL

从 OpenGL 查询并同步状态到缓存。

```cpp
void SyncFromGL();
```

**功能**:
- 查询 OpenGL 当前的实际状态
- 更新内部缓存以匹配 OpenGL 状态
- 同步所有状态：深度、混合、剔除、纹理、缓冲区、着色器等

**注意**: 此操作相对耗时，不建议频繁调用。

**示例**:
```cpp
// 在不确定状态是否同步时
state->SyncFromGL();

// 现在缓存已与 OpenGL 状态同步
// 继续渲染...
```

---

### 严格模式

#### SetStrictMode

启用/禁用严格模式。

```cpp
void SetStrictMode(bool enable);
```

**严格模式说明**:
- **启用严格模式**: 所有状态设置都直接调用 OpenGL API，不使用缓存优化
- **禁用严格模式**: 使用缓存优化（默认）

**性能影响**:
- 严格模式会降低性能（增加约 10-20% 的状态切换开销）
- 但保证状态总是正确的，适合调试和与第三方库混用

**示例**:
```cpp
// 调试模式或与第三方库混用时启用严格模式
#ifdef DEBUG
    state->SetStrictMode(true);
#endif

// 生产环境使用缓存优化
#ifdef RELEASE
    state->SetStrictMode(false);
#endif
```

---

#### IsStrictMode

检查是否处于严格模式。

```cpp
bool IsStrictMode() const;
```

**示例**:
```cpp
if (state->IsStrictMode()) {
    Logger::Info("严格模式已启用");
}
```

---

### 使用建议

#### 方案 1：选择性失效缓存（推荐）

```cpp
// 正常渲染
state->SetDepthTest(true);
state->BindTexture(0, texture);
// ...

// 使用第三方库
ImGui::Render();

// 只清空可能被修改的缓存
state->InvalidateTextureCache();
state->InvalidateRenderStateCache();

// 继续渲染
state->SetDepthTest(true);
```

**优点**: 性能最优，只清空必要的缓存

---

#### 方案 2：全部失效（简单）

```cpp
// 使用第三方库后
ImGui::Render();

// 清空所有缓存
state->InvalidateCache();

// 继续渲染
state->SetDepthTest(true);
```

**优点**: 简单可靠，适合调试

---

#### 方案 3：同步状态（保守）

```cpp
// 使用第三方库后
ImGui::Render();

// 从 OpenGL 同步状态
state->SyncFromGL();

// 继续渲染（不需要重新设置状态）
```

**优点**: 保留第三方库设置的状态，不需要重新设置
**缺点**: 性能开销较大（需要多次查询 OpenGL）

---

#### 方案 4：严格模式（调试）

```cpp
// 启用严格模式
state->SetStrictMode(true);

// 所有操作都不使用缓存，总是正确
state->SetDepthTest(true);
ImGui::Render();
state->SetDepthTest(true);  // 会重新调用 OpenGL API
```

**优点**: 总是正确，适合调试
**缺点**: 性能降低 10-20%

---

### 完整示例：与 ImGui 混用

```cpp
void Render() {
    auto state = renderer->GetRenderState();
    
    // === 1. 渲染 3D 场景 ===
    state->SetDepthTest(true);
    state->SetBlendMode(BlendMode::None);
    state->UseProgram(sceneShader);
    state->BindTexture(0, sceneTexture);
    
    // 渲染 3D 场景...
    RenderScene();
    
    // === 2. 渲染 ImGui（第三方库）===
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    // === 3. 清空缓存（ImGui 改变了 OpenGL 状态）===
    state->InvalidateCache();
    
    // === 4. 继续渲染自己的内容 ===
    state->SetDepthTest(false);
    state->SetBlendMode(BlendMode::Alpha);
    state->UseProgram(uiShader);
    
    // 渲染 UI...
    RenderUI();
}
```

---

### 性能对比

| 方法 | 性能影响 | 正确性 | 推荐场景 |
|------|---------|--------|---------|
| 不处理 | 0% | ❌ 可能错误 | 纯自己的渲染代码 |
| InvalidateCache | ~1% | ✅ 正确 | 偶尔使用第三方库 |
| SyncFromGL | ~5% | ✅ 正确 | 需要保留第三方状态 |
| 严格模式 | ~15% | ✅ 总是正确 | 调试和开发 |

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

### 状态管理优化

1. **状态缓存**: `RenderState` 自动缓存，避免重复设置
2. **批处理**: 按状态分组渲染对象，减少状态切换
3. **状态排序**: 按 shader → 纹理 → 材质 → 网格 的顺序排序
4. **纹理绑定**: 避免频繁切换纹理单元
5. **VAO 使用**: 使用 VAO 减少顶点属性设置开销
6. **着色器切换**: 着色器切换开销较大，尽量减少切换次数

### 线程安全与性能

7. **读操作并发**: 多个线程可以同时读取状态（使用共享锁）
8. **避免写竞争**: 尽量在单个渲染线程中进行状态修改
9. **锁开销**: 虽然有锁保护，但在单线程场景下开销极小
10. **OpenGL 线程**: 保持所有 OpenGL 调用在同一线程中，避免上下文切换

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

