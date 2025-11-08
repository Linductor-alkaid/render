# 材质系统使用指南

[返回文档首页](README.md)

---

## 概述

材质系统 (Material System) 是渲染引擎中用于管理物体外观的核心模块。Material类封装了渲染所需的所有属性，包括颜色、纹理、着色器和渲染状态。

**功能亮点**:
- ✅ 完整的材质属性管理（环境色、漫反射、镜面反射、自发光）
- ✅ 多纹理支持（漫反射贴图、法线贴图、镜面贴图等）
- ✅ 着色器集成
- ✅ 渲染状态控制（混合模式、面剔除、深度测试）
- ✅ PBR材质参数（金属度、粗糙度）
- ✅ 自定义 uniform 参数
- ✅ 简洁的API设计

---

## 快速开始

### 创建基础材质

```cpp
#include <render/material.h>
#include <render/shader_cache.h>

// 创建材质
auto material = std::make_shared<Material>();
material->SetName("MyMaterial");

// 加载着色器
auto shader = ShaderCache::GetInstance().LoadShader(
    "phong", "shaders/phong.vert", "shaders/phong.frag");
material->SetShader(shader);

// 设置颜色
material->SetDiffuseColor(Color(0.8f, 0.1f, 0.1f, 1.0f));  // 红色
material->SetSpecularColor(Color(1.0f, 1.0f, 1.0f, 1.0f)); // 白色高光
material->SetShininess(32.0f);  // 镜面反射强度
```

### 添加纹理

```cpp
#include <render/texture_loader.h>

// 加载纹理
auto diffuse = TextureLoader::GetInstance().LoadTexture(
    "wood_diffuse", "textures/wood.png");
material->SetTexture("diffuseMap", diffuse);

// 多纹理支持
auto normal = TextureLoader::GetInstance().LoadTexture(
    "wood_normal", "textures/wood_normal.png");
material->SetTexture("normalMap", normal);
```

### 使用材质渲染

```cpp
// 应用材质
material->Bind(renderer.GetRenderState());

// 设置变换矩阵
auto* uniformMgr = material->GetShader()->GetUniformManager();
uniformMgr->SetMatrix4("projection", projMatrix);
uniformMgr->SetMatrix4("view", viewMatrix);
uniformMgr->SetMatrix4("model", modelMatrix);

// 渲染网格
mesh->Draw();

// 解绑材质
material->Unbind();
```

---

## 材质类型示例

### 1. 金属材质

```cpp
auto metalMaterial = std::make_shared<Material>();
metalMaterial->SetName("Metal");
metalMaterial->SetShader(shader);

// 金属特性
metalMaterial->SetDiffuseColor(Color(0.6f, 0.6f, 0.7f, 1.0f));
metalMaterial->SetSpecularColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
metalMaterial->SetShininess(128.0f);    // 高光锐利
metalMaterial->SetMetallic(1.0f);       // 纯金属
metalMaterial->SetRoughness(0.2f);      // 光滑
```

### 2. 塑料材质

```cpp
auto plasticMaterial = std::make_shared<Material>();
plasticMaterial->SetName("Plastic");
plasticMaterial->SetShader(shader);

// 塑料特性
plasticMaterial->SetDiffuseColor(Color(0.2f, 0.4f, 0.8f, 1.0f));
plasticMaterial->SetSpecularColor(Color(0.5f, 0.5f, 0.5f, 1.0f));
plasticMaterial->SetShininess(16.0f);   // 高光柔和
plasticMaterial->SetMetallic(0.0f);     // 非金属
plasticMaterial->SetRoughness(0.6f);    // 较粗糙
```

### 3. 发光材质

```cpp
auto emissiveMaterial = std::make_shared<Material>();
emissiveMaterial->SetName("Neon");
emissiveMaterial->SetShader(shader);

// 发光特性
emissiveMaterial->SetDiffuseColor(Color(0.2f, 0.8f, 1.0f, 1.0f));
emissiveMaterial->SetEmissiveColor(Color(0.5f, 1.0f, 1.5f, 1.0f));  // 发光
emissiveMaterial->SetBlendMode(BlendMode::Additive);  // 加法混合
```

### 4. 透明材质

```cpp
auto glassMaterial = std::make_shared<Material>();
glassMaterial->SetName("Glass");
glassMaterial->SetShader(shader);

// 透明特性
glassMaterial->SetDiffuseColor(Color(0.2f, 0.3f, 0.4f, 0.3f));
glassMaterial->SetOpacity(0.3f);
glassMaterial->SetShininess(128.0f);

// 透明渲染配置
glassMaterial->SetBlendMode(BlendMode::Alpha);
glassMaterial->SetDepthTest(true);
glassMaterial->SetDepthWrite(false);  // 透明物体不写深度
```

---

## 材质属性说明

### 颜色属性

| 属性 | 说明 | 默认值 |
|------|------|--------|
| `AmbientColor` | 环境光颜色 | `Color(0.2, 0.2, 0.2, 1.0)` |
| `DiffuseColor` | 漫反射颜色（主要颜色） | `Color(0.8, 0.8, 0.8, 1.0)` |
| `SpecularColor` | 镜面反射颜色（高光） | `Color(1.0, 1.0, 1.0, 1.0)` |
| `EmissiveColor` | 自发光颜色 | `Color(0.0, 0.0, 0.0, 1.0)` |

### 物理属性

| 属性 | 说明 | 范围 | 默认值 |
|------|------|------|--------|
| `Shininess` | 镜面反射强度 | 1.0 - 128.0 | `32.0` |
| `Opacity` | 不透明度 | 0.0 - 1.0 | `1.0` |
| `Metallic` | 金属度 (PBR) | 0.0 - 1.0 | `0.0` |
| `Roughness` | 粗糙度 (PBR) | 0.0 - 1.0 | `0.5` |

### 渲染状态

| 状态 | 说明 | 默认值 |
|------|------|--------|
| `BlendMode` | 混合模式 | `BlendMode::None` |
| `CullFace` | 面剔除模式 | `CullFace::Back` |
| `DepthTest` | 深度测试 | `true` |
| `DepthWrite` | 深度写入 | `true` |

---

## 着色器集成

材质系统自动将属性传递给着色器。着色器中应定义以下 uniform：

```glsl
// 片段着色器示例
#version 450 core

// 材质属性（Material 自动设置）
uniform vec4 material.ambient;
uniform vec4 material.diffuse;
uniform vec4 material.specular;
uniform vec4 material.emissive;
uniform float material.shininess;
uniform float material.opacity;
uniform float material.metallic;   // PBR
uniform float material.roughness;  // PBR

// 纹理贴图
uniform sampler2D diffuseMap;
uniform sampler2D normalMap;
uniform sampler2D specularMap;

// 光照
uniform vec3 lightPos;
uniform vec3 viewPos;

void main() {
    // 使用材质属性进行光照计算...
}
```

---

## 纹理命名规范

推荐使用以下纹理名称与着色器中的 uniform 对应：

| 纹理名称 | 用途 | 着色器 uniform |
|----------|------|----------------|
| `diffuseMap` | 漫反射贴图 | `sampler2D diffuseMap` |
| `normalMap` | 法线贴图 | `sampler2D normalMap` |
| `specularMap` | 镜面反射贴图 | `sampler2D specularMap` |
| `emissiveMap` | 自发光贴图 | `sampler2D emissiveMap` |
| `aoMap` | 环境遮蔽贴图 | `sampler2D aoMap` |
| `roughnessMap` | 粗糙度贴图 | `sampler2D roughnessMap` |
| `metallicMap` | 金属度贴图 | `sampler2D metallicMap` |

---

## 自定义参数

材质支持设置自定义 uniform 参数：

```cpp
// 标量参数
material->SetFloat("tiling", 2.0f);
material->SetInt("useNormalMap", 1);

// 向量参数
material->SetVector3("windDirection", Vector3(1.0f, 0.0f, 0.0f));
material->SetColor("tintColor", Color(1.0f, 0.5f, 0.5f, 1.0f));

// 矩阵参数
material->SetMatrix4("customTransform", transformMatrix);
```

这些参数会在 `Bind()` 时自动传递给着色器。

---

## 渲染顺序建议

为了获得正确的渲染效果，建议按照以下顺序渲染：

### 1. 不透明物体

```cpp
// 开启深度测试和深度写入
renderState->SetDepthTest(true);
renderState->SetDepthWrite(true);

for (auto& obj : opaqueObjects) {
    obj.material->Bind(renderState);
    obj.mesh->Draw();
    obj.material->Unbind();
}
```

### 2. 透明物体（从远到近）

```cpp
// 按距离排序（远到近）
std::sort(transparentObjects.begin(), transparentObjects.end(),
    [&camera](const auto& a, const auto& b) {
        float distA = (a.position - camera.position).norm();
        float distB = (b.position - camera.position).norm();
        return distA > distB;  // 远到近
    });

// 开启深度测试，关闭深度写入
renderState->SetDepthTest(true);
renderState->SetDepthWrite(false);

for (auto& obj : transparentObjects) {
    obj.material->Bind(renderState);
    obj.mesh->Draw();
    obj.material->Unbind();
}

// 恢复深度写入
renderState->SetDepthWrite(true);
```

---

## 性能优化

### 1. 材质排序

按材质分组渲染，减少状态切换：

```cpp
// 按着色器和材质分组
std::sort(objects.begin(), objects.end(),
    [](const auto& a, const auto& b) {
        if (a.material->GetShader() != b.material->GetShader())
            return a.material->GetShader() < b.material->GetShader();
        return a.material < b.material;
    });
```

### 2. 材质复用

相似物体共享材质：

```cpp
// 创建一次
auto woodMaterial = std::make_shared<Material>();
// 配置材质...

// 多个物体共享
obj1.SetMaterial(woodMaterial);
obj2.SetMaterial(woodMaterial);
obj3.SetMaterial(woodMaterial);
```

### 3. 纹理复用

多个材质共享纹理：

```cpp
auto brickTexture = TextureLoader::GetInstance().LoadTexture(
    "brick", "textures/brick.png");

material1->SetTexture("diffuseMap", brickTexture);
material2->SetTexture("diffuseMap", brickTexture);
```

---

## 常见问题

### Q: 材质没有显示？

**A**: 检查以下几点：
1. 是否设置了着色器？`material->SetShader(shader)`
2. 着色器是否有效？`shader->IsValid()`
3. 是否调用了 `material->Bind()`？
4. 变换矩阵是否正确设置？

### Q: 透明材质不正确？

**A**: 确保：
1. 设置了正确的混合模式：`SetBlendMode(BlendMode::Alpha)`
2. 关闭深度写入：`SetDepthWrite(false)`
3. 从远到近排序渲染透明物体

### Q: 纹理不显示？

**A**: 检查：
1. 纹理是否加载成功？`texture->IsValid()`
2. 纹理名称是否与着色器 uniform 匹配？
3. 着色器中是否设置了正确的纹理单元？

### Q: 自定义参数不生效？

**A**: 确保：
1. 参数名称与着色器中的 uniform 名称完全一致
2. 参数类型匹配（int/float/vec3等）
3. 调用 `Bind()` 之后参数才会传递给着色器

---

## 示例程序

完整的材质系统示例程序：[12_material_test.cpp](../examples/12_material_test.cpp)

该示例演示：
- 6种不同类型的材质（红色、金属、塑料、发光、透明、纹理）
- 材质属性的配置
- 材质的应用和渲染
- 渲染状态的控制

**运行示例**:
```bash
cd build/bin/Release
./12_material_test.exe
```

**控制**:
- 空格键/左右箭头：切换材质
- W：切换线框模式
- ESC：退出

---

## API 文档

详细的 API 文档请参考：[Material API 参考](api/Material.md)

---

## 相关文档

- [着色器缓存指南](SHADER_CACHE_GUIDE.md)
- [纹理系统](TEXTURE_SYSTEM.md)
- [渲染状态管理](api/RenderState.md)
- [开发指南](DEVELOPMENT_GUIDE.md)

---

[返回文档首页](README.md)

