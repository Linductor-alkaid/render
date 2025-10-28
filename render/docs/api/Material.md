# Material API 参考

[返回 API 首页](README.md)

---

## 概述

`Material` 类管理渲染材质的所有属性，包括颜色、物理参数、纹理贴图、着色器程序和渲染状态。Material 提供了统一的接口来控制物体的外观和渲染行为。

**头文件**: `render/material.h`  
**命名空间**: `Render`

### 线程安全性

✅ **Material 类现在是线程安全的**

- ✅ 所有公共方法都使用互斥锁保护
- ✅ Getter 方法返回副本以保证线程安全
- ✅ 移动操作使用 `std::scoped_lock` 避免死锁
- ✅ 可以在多线程环境中安全地访问和修改材质属性
- ⚠️ **OpenGL 上下文注意事项**: OpenGL 调用（如 `Bind()`）需要在创建上下文的线程中执行（通常是主线程）

---

## 类定义

```cpp
class Material {
public:
    Material();
    ~Material();
    
    // 禁止拷贝，允许移动
    Material(const Material&) = delete;
    Material& operator=(const Material&) = delete;
    Material(Material&& other) noexcept;
    Material& operator=(Material&& other) noexcept;
    
    // 名称管理
    void SetName(const std::string& name);
    const std::string& GetName() const;
    
    // 着色器管理
    void SetShader(std::shared_ptr<Shader> shader);
    std::shared_ptr<Shader> GetShader() const;
    
    // 材质颜色属性
    void SetAmbientColor(const Color& color);
    void SetDiffuseColor(const Color& color);
    void SetSpecularColor(const Color& color);
    void SetEmissiveColor(const Color& color);
    
    // 材质物理属性
    void SetShininess(float shininess);
    void SetOpacity(float opacity);
    void SetMetallic(float metallic);
    void SetRoughness(float roughness);
    
    // 纹理管理
    void SetTexture(const std::string& name, std::shared_ptr<Texture> texture);
    std::shared_ptr<Texture> GetTexture(const std::string& name) const;
    bool HasTexture(const std::string& name) const;
    void RemoveTexture(const std::string& name);
    void ClearTextures();
    
    // 自定义参数
    void SetInt(const std::string& name, int value);
    void SetFloat(const std::string& name, float value);
    void SetVector3(const std::string& name, const Vector3& value);
    void SetColor(const std::string& name, const Color& value);
    void SetMatrix4(const std::string& name, const Matrix4& value);
    
    // 渲染状态
    void SetBlendMode(BlendMode mode);
    void SetCullFace(CullFace mode);
    void SetDepthTest(bool enable);
    void SetDepthWrite(bool enable);
    
    // 应用和绑定
    void Bind(RenderState* renderState = nullptr);
    void Unbind();
    bool IsValid() const;
};
```

---

## 构造和析构

### Material()

构造函数，创建默认材质。

**默认值**:
- 环境色: `Color(0.2f, 0.2f, 0.2f, 1.0f)` - 深灰色
- 漫反射: `Color(0.8f, 0.8f, 0.8f, 1.0f)` - 浅灰色
- 镜面反射: `Color(1.0f, 1.0f, 1.0f, 1.0f)` - 白色
- 自发光: `Color(0.0f, 0.0f, 0.0f, 1.0f)` - 黑色（无发光）
- 镜面反射强度: `32.0f`
- 不透明度: `1.0f`
- 金属度: `0.0f`
- 粗糙度: `0.5f`
- 混合模式: `BlendMode::None`
- 面剔除: `CullFace::Back`
- 深度测试: `true`
- 深度写入: `true`

**示例**:
```cpp
auto material = std::make_shared<Material>();
material->SetName("MyMaterial");
```

---

## 名称管理

### SetName()

设置材质名称（用于调试和识别）。

```cpp
void SetName(const std::string& name);
```

**示例**:
```cpp
material->SetName("Wood Material");
```

### GetName()

获取材质名称。

```cpp
const std::string& GetName() const;
```

---

## 着色器管理

### SetShader()

设置材质使用的着色器。

```cpp
void SetShader(std::shared_ptr<Shader> shader);
```

**参数**:
- `shader` - 着色器对象

**说明**: 材质必须设置着色器才能正常渲染。

**示例**:
```cpp
auto shader = ShaderCache::GetInstance().LoadShader(
    "basic", "shaders/basic.vert", "shaders/basic.frag");
material->SetShader(shader);
```

### GetShader()

获取材质的着色器。

```cpp
std::shared_ptr<Shader> GetShader() const;
```

---

## 材质颜色属性

### SetAmbientColor()

设置环境光颜色（物体在无直接光照时的颜色）。

```cpp
void SetAmbientColor(const Color& color);
```

**示例**:
```cpp
// 深红色环境光
material->SetAmbientColor(Color(0.2f, 0.0f, 0.0f, 1.0f));
```

### SetDiffuseColor()

设置漫反射颜色（物体的主要颜色）。

```cpp
void SetDiffuseColor(const Color& color);
```

**示例**:
```cpp
// 红色漫反射
material->SetDiffuseColor(Color(0.8f, 0.1f, 0.1f, 1.0f));
```

### SetSpecularColor()

设置镜面反射颜色（高光颜色）。

```cpp
void SetSpecularColor(const Color& color);
```

**示例**:
```cpp
// 白色高光
material->SetSpecularColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
```

### SetEmissiveColor()

设置自发光颜色（物体自身发光）。

```cpp
void SetEmissiveColor(const Color& color);
```

**示例**:
```cpp
// 黄色发光
material->SetEmissiveColor(Color(1.0f, 1.0f, 0.0f, 1.0f));
```

---

## 材质物理属性

### SetShininess()

设置镜面反射强度（高光锐利度）。

```cpp
void SetShininess(float shininess);
```

**参数**:
- `shininess` - 镜面反射强度（推荐范围：1.0 - 128.0）
  - 值越小，高光越分散（如塑料）
  - 值越大，高光越集中（如金属）

**示例**:
```cpp
// 金属表面（锐利高光）
material->SetShininess(128.0f);

// 塑料表面（柔和高光）
material->SetShininess(16.0f);
```

### SetOpacity()

设置不透明度。

```cpp
void SetOpacity(float opacity);
```

**参数**:
- `opacity` - 不透明度（0.0 = 完全透明，1.0 = 完全不透明）

**说明**: 如果设置小于1.0，建议同时设置 `SetBlendMode(BlendMode::Alpha)`。

**示例**:
```cpp
// 半透明材质
material->SetOpacity(0.5f);
material->SetBlendMode(BlendMode::Alpha);
material->SetDepthWrite(false);  // 透明物体通常不写入深度
```

### SetMetallic()

设置金属度（PBR 材质，0.0 = 非金属，1.0 = 纯金属）。

```cpp
void SetMetallic(float metallic);
```

**示例**:
```cpp
// 金属材质
material->SetMetallic(1.0f);

// 塑料材质
material->SetMetallic(0.0f);
```

### SetRoughness()

设置粗糙度（PBR 材质，0.0 = 光滑，1.0 = 粗糙）。

```cpp
void SetRoughness(float roughness);
```

**示例**:
```cpp
// 光滑金属
material->SetRoughness(0.2f);

// 粗糙塑料
material->SetRoughness(0.8f);
```

---

## 纹理管理

### SetTexture()

设置纹理贴图。

```cpp
void SetTexture(const std::string& name, std::shared_ptr<Texture> texture);
```

**参数**:
- `name` - 纹理名称（与着色器中的 uniform 名称对应）
- `texture` - 纹理对象

**常用纹理名称**:
- `"diffuseMap"` - 漫反射贴图
- `"normalMap"` - 法线贴图
- `"specularMap"` - 镜面反射贴图
- `"emissiveMap"` - 自发光贴图
- `"aoMap"` - 环境遮蔽贴图

**示例**:
```cpp
auto diffuse = TextureLoader::GetInstance().LoadTexture("textures/wood.png");
material->SetTexture("diffuseMap", diffuse);

auto normal = TextureLoader::GetInstance().LoadTexture("textures/wood_normal.png");
material->SetTexture("normalMap", normal);
```

### GetTexture()

获取纹理贴图。

```cpp
std::shared_ptr<Texture> GetTexture(const std::string& name) const;
```

**返回值**: 纹理对象，如果不存在返回 `nullptr`

### HasTexture()

检查是否有指定纹理。

```cpp
bool HasTexture(const std::string& name) const;
```

### RemoveTexture()

移除指定纹理。

```cpp
void RemoveTexture(const std::string& name);
```

### ClearTextures()

清空所有纹理。

```cpp
void ClearTextures();
```

---

## 自定义参数

材质支持设置自定义 uniform 参数，这些参数会在 `Bind()` 时自动传递给着色器。

### SetInt() / SetFloat()

设置整型/浮点型参数。

```cpp
void SetInt(const std::string& name, int value);
void SetFloat(const std::string& name, float value);
```

**示例**:
```cpp
material->SetInt("useTexture", 1);
material->SetFloat("tiling", 2.0f);
```

### SetVector3() / SetColor()

设置向量/颜色参数。

```cpp
void SetVector3(const std::string& name, const Vector3& value);
void SetColor(const std::string& name, const Color& value);
```

**示例**:
```cpp
material->SetVector3("windDirection", Vector3(1.0f, 0.0f, 0.0f));
material->SetColor("tintColor", Color(1.0f, 0.5f, 0.5f, 1.0f));
```

### SetMatrix4()

设置矩阵参数。

```cpp
void SetMatrix4(const std::string& name, const Matrix4& value);
```

---

## 渲染状态

### SetBlendMode()

设置混合模式。

```cpp
void SetBlendMode(BlendMode mode);
```

**可选值**:
- `BlendMode::None` - 无混合（不透明物体）
- `BlendMode::Alpha` - Alpha 混合（透明物体）
- `BlendMode::Additive` - 加法混合（发光效果）
- `BlendMode::Multiply` - 乘法混合

**示例**:
```cpp
// 透明材质
material->SetBlendMode(BlendMode::Alpha);

// 发光材质
material->SetBlendMode(BlendMode::Additive);
```

### SetCullFace()

设置面剔除模式。

```cpp
void SetCullFace(CullFace mode);
```

**可选值**:
- `CullFace::Back` - 剔除背面（默认）
- `CullFace::Front` - 剔除正面（如天空盒内部）
- `CullFace::None` - 不剔除（双面渲染）

**示例**:
```cpp
// 双面渲染（如树叶）
material->SetCullFace(CullFace::None);
```

### SetDepthTest() / SetDepthWrite()

设置深度测试和深度写入。

```cpp
void SetDepthTest(bool enable);
void SetDepthWrite(bool enable);
```

**示例**:
```cpp
// 透明物体：开启深度测试，关闭深度写入
material->SetDepthTest(true);
material->SetDepthWrite(false);

// UI 元素：关闭深度测试
material->SetDepthTest(false);
```

---

## 应用和绑定

### Bind()

绑定材质到渲染管线（应用所有设置）。

```cpp
void Bind(RenderState* renderState = nullptr);
```

**参数**:
- `renderState` - 渲染状态管理器（可选）

**此方法会**:
1. 激活着色器
2. 设置所有材质属性到 uniform
3. 绑定所有纹理
4. 设置所有自定义参数
5. 应用渲染状态（如果提供了 renderState）

**示例**:
```cpp
material->Bind(renderer.GetRenderState());

// 设置变换矩阵
auto* uniformMgr = material->GetShader()->GetUniformManager();
uniformMgr->SetMatrix4("projection", projMatrix);
uniformMgr->SetMatrix4("view", viewMatrix);
uniformMgr->SetMatrix4("model", modelMatrix);

// 渲染
mesh->Draw();

material->Unbind();
```

### Unbind()

解绑材质。

```cpp
void Unbind();
```

### IsValid()

检查材质是否有效（有着色器且着色器有效）。

```cpp
bool IsValid() const;
```

**返回值**: 材质有效返回 `true`

---

## 完整示例

### 基础材质

```cpp
#include <render/material.h>
#include <render/shader_cache.h>

// 创建红色塑料材质
auto CreatePlasticMaterial() -> std::shared_ptr<Material> {
    auto material = std::make_shared<Material>();
    material->SetName("Red Plastic");
    
    // 加载着色器
    auto shader = ShaderCache::GetInstance().LoadShader(
        "phong", "shaders/phong.vert", "shaders/phong.frag");
    material->SetShader(shader);
    
    // 设置颜色
    material->SetAmbientColor(Color(0.2f, 0.0f, 0.0f, 1.0f));
    material->SetDiffuseColor(Color(0.8f, 0.1f, 0.1f, 1.0f));
    material->SetSpecularColor(Color(0.5f, 0.5f, 0.5f, 1.0f));
    
    // 设置物理属性
    material->SetShininess(32.0f);
    material->SetMetallic(0.0f);
    material->SetRoughness(0.6f);
    
    return material;
}
```

### 纹理材质

```cpp
// 创建纹理材质
auto CreateTexturedMaterial() -> std::shared_ptr<Material> {
    auto material = std::make_shared<Material>();
    material->SetName("Wood");
    
    // 加载着色器
    auto shader = ShaderCache::GetInstance().LoadShader(
        "textured", "shaders/textured.vert", "shaders/textured.frag");
    material->SetShader(shader);
    
    // 加载纹理
    auto diffuse = TextureLoader::GetInstance().LoadTexture("textures/wood.png");
    material->SetTexture("diffuseMap", diffuse);
    
    auto normal = TextureLoader::GetInstance().LoadTexture("textures/wood_normal.png");
    material->SetTexture("normalMap", normal);
    
    // 设置基础属性
    material->SetDiffuseColor(Color::White());
    material->SetShininess(32.0f);
    
    return material;
}
```

### 透明材质

```cpp
// 创建半透明玻璃材质
auto CreateGlassMaterial() -> std::shared_ptr<Material> {
    auto material = std::make_shared<Material>();
    material->SetName("Glass");
    
    auto shader = ShaderCache::GetInstance().LoadShader(
        "glass", "shaders/glass.vert", "shaders/glass.frag");
    material->SetShader(shader);
    
    // 设置颜色和透明度
    material->SetDiffuseColor(Color(0.2f, 0.3f, 0.4f, 0.3f));
    material->SetSpecularColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
    material->SetShininess(128.0f);
    material->SetOpacity(0.3f);
    
    // 配置透明渲染
    material->SetBlendMode(BlendMode::Alpha);
    material->SetDepthTest(true);
    material->SetDepthWrite(false);  // 透明物体不写入深度
    
    return material;
}
```

### 发光材质

```cpp
// 创建发光材质
auto CreateEmissiveMaterial() -> std::shared_ptr<Material> {
    auto material = std::make_shared<Material>();
    material->SetName("Neon");
    
    auto shader = ShaderCache::GetInstance().LoadShader(
        "emissive", "shaders/emissive.vert", "shaders/emissive.frag");
    material->SetShader(shader);
    
    // 设置颜色
    material->SetDiffuseColor(Color(0.2f, 0.8f, 1.0f, 1.0f));
    material->SetEmissiveColor(Color(0.5f, 1.0f, 1.5f, 1.0f));  // 发光强度可以 > 1
    material->SetShininess(64.0f);
    
    // 使用加法混合增强发光效果
    material->SetBlendMode(BlendMode::Additive);
    
    return material;
}
```

### 使用材质渲染

```cpp
// 主渲染循环
void Render(Renderer& renderer, const std::vector<GameObject>& objects) {
    for (const auto& obj : objects) {
        auto material = obj.GetMaterial();
        auto mesh = obj.GetMesh();
        
        if (!material || !material->IsValid() || !mesh) {
            continue;
        }
        
        // 绑定材质
        material->Bind(renderer.GetRenderState());
        
        // 设置变换矩阵
        auto* uniformMgr = material->GetShader()->GetUniformManager();
        uniformMgr->SetMatrix4("model", obj.GetTransform());
        uniformMgr->SetMatrix4("view", camera.GetViewMatrix());
        uniformMgr->SetMatrix4("projection", camera.GetProjectionMatrix());
        
        // 设置光照（如果需要）
        uniformMgr->SetVector3("lightPos", lightPosition);
        uniformMgr->SetVector3("viewPos", camera.GetPosition());
        
        // 渲染网格
        mesh->Draw();
        
        // 解绑材质
        material->Unbind();
    }
}
```

---

## 着色器集成

材质会自动将属性传递给着色器的 uniform。以下是着色器中应该定义的 uniform：

### 基础 Phong 光照着色器

```glsl
// 顶点着色器 (phong.vert)
#version 450 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    TexCoord = aTexCoord;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
```

```glsl
// 片段着色器 (phong.frag)
#version 450 core

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

out vec4 FragColor;

// 材质属性
uniform vec4 material.ambient;
uniform vec4 material.diffuse;
uniform vec4 material.specular;
uniform vec4 material.emissive;
uniform float material.shininess;
uniform float material.opacity;

// 光照
uniform vec3 lightPos;
uniform vec3 viewPos;

void main() {
    // 环境光
    vec3 ambient = material.ambient.rgb;
    
    // 漫反射
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * material.diffuse.rgb;
    
    // 镜面反射
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = spec * material.specular.rgb;
    
    // 自发光
    vec3 emissive = material.emissive.rgb;
    
    vec3 result = ambient + diffuse + specular + emissive;
    FragColor = vec4(result, material.opacity);
}
```

---

## 注意事项

### 1. 材质必须有着色器

```cpp
// 错误：未设置着色器
auto material = std::make_shared<Material>();
material->Bind();  // 会警告并返回

// 正确
auto shader = ShaderCache::GetInstance().LoadShader(...);
material->SetShader(shader);
material->Bind();  // OK
```

### 2. 纹理绑定顺序

纹理按照添加顺序绑定到纹理单元（0, 1, 2...）：

```cpp
material->SetTexture("diffuseMap", tex1);   // 纹理单元 0
material->SetTexture("normalMap", tex2);    // 纹理单元 1
material->SetTexture("specularMap", tex3);  // 纹理单元 2
```

### 3. 透明物体渲染顺序

透明物体需要从后向前排序渲染：

```cpp
// 1. 先渲染不透明物体
for (auto& obj : opaqueObjects) {
    obj.material->Bind();
    obj.mesh->Draw();
    obj.material->Unbind();
}

// 2. 按深度排序透明物体
std::sort(transparentObjects.begin(), transparentObjects.end(),
    [](const auto& a, const auto& b) {
        return a.distance > b.distance;  // 远到近
    });

// 3. 渲染透明物体
for (auto& obj : transparentObjects) {
    obj.material->Bind();
    obj.mesh->Draw();
    obj.material->Unbind();
}
```

### 4. 自定义 Uniform 与着色器一致

自定义参数名称必须与着色器中的 uniform 名称一致：

```cpp
// 着色器中: uniform float myParam;
material->SetFloat("myParam", 1.0f);  // 正确

// 着色器中: uniform float myParameter;
material->SetFloat("myParam", 1.0f);  // 不会生效，名称不匹配
```

---

## 性能优化

1. **材质排序**: 按材质分组渲染，减少状态切换
2. **纹理复用**: 多个材质共享纹理
3. **着色器复用**: 相似材质使用同一着色器
4. **批处理**: 使用相同材质的物体批量渲染

---

## 线程安全使用示例

### 多线程访问材质属性

```cpp
// 主线程中创建材质
auto material = std::make_shared<Material>();
material->SetDiffuseColor(Color::Red());
material->SetShininess(32.0f);

// 工作线程中安全读取属性
std::thread worker([material]() {
    Color diffuse = material->GetDiffuseColor();   // 线程安全
    float shininess = material->GetShininess();     // 线程安全
    bool valid = material->IsValid();               // 线程安全
});

worker.join();
```

### 多线程修改材质

```cpp
auto material = std::make_shared<Material>();

// 多个线程同时修改不同属性
std::thread t1([material]() {
    material->SetDiffuseColor(Color::Blue());  // 线程安全
});

std::thread t2([material]() {
    material->SetShininess(64.0f);  // 线程安全
});

std::thread t3([material]() {
    material->SetMetallic(1.0f);   // 线程安全
});

t1.join();
t2.join();
t3.join();
```

### 注意事项

1. **Getter 返回副本**: 为保证线程安全，所有 getter 方法返回副本而不是引用
   ```cpp
   // 返回副本，线程安全
   Color color = material->GetDiffuseColor();
   
   // 不要缓存引用（旧版本API）
   // const Color& color = material->GetDiffuseColor();  // 不再支持
   ```

2. **OpenGL 上下文限制**: 虽然 Material 类本身是线程安全的，但 OpenGL 调用需要在主线程：
   ```cpp
   // 主线程中使用材质
   material->Bind(renderState);  // OpenGL 调用，必须在主线程
   mesh->Draw();
   material->Unbind();
   
   // 工作线程中只读取属性
   std::thread worker([material]() {
       Color color = material->GetDiffuseColor();  // OK
       // material->Bind();  // 错误！OpenGL调用必须在主线程
   });
   ```

3. **性能考虑**: Getter 返回副本会有轻微性能开销，但换来了线程安全性

---

## 相关文档

- [Shader API](Shader.md)
- [Texture API](Texture.md)
- [RenderState API](RenderState.md)
- [UniformManager API](UniformManager.md)
- [示例程序: 12_material_test](../../examples/12_material_test.cpp)
- [线程安全测试: 13_material_thread_safe_test](../../examples/13_material_thread_safe_test.cpp) 🔒

---

[上一篇: MeshLoader](MeshLoader.md) | [下一篇: RenderState](RenderState.md)

