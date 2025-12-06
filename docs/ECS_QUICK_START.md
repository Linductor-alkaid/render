# ECS 系统快速入门指南

[返回文档首页](README.md)

---

## 📋 概述

本文档提供 ECS 系统的快速入门指南，涵盖所有新增功能和最佳实践。

**前置条件**：
- 已完成核心渲染器的基本配置
- 已创建 Renderer 实例
- 已初始化 AsyncResourceLoader
- 已加载着色器到 ShaderCache

---

## 🚀 快速开始

### 1. 创建 World 并注册系统

```cpp
#include <render/ecs/world.h>
#include <render/ecs/systems.h>
#include <render/ecs/components.h>

using namespace Render::ECS;

// 创建 World
auto world = std::make_shared<World>();

// 注册所有系统（按优先级自动排序）
world->RegisterSystem<WindowSystem>(renderer);         // 优先级 3 - 窗口管理
world->RegisterSystem<CameraSystem>();                 // 优先级 5 - 相机更新
world->RegisterSystem<TransformSystem>();              // 优先级 10 - 变换层级
world->RegisterSystem<GeometrySystem>();               // 优先级 15 - 几何生成
world->RegisterSystem<ResourceLoadingSystem>(asyncLoader);  // 优先级 20 - 资源加载
world->RegisterSystem<LightSystem>(renderer);          // 优先级 50 - 光源管理
world->RegisterSystem<UniformSystem>(renderer);        // 优先级 90 - 全局 Uniform
world->RegisterSystem<MeshRenderSystem>(renderer);     // 优先级 100 - 网格渲染
```

---

### 2. 创建相机实体

```cpp
// 创建相机实体
auto cameraEntity = world->CreateEntity();

// 添加 Transform 组件
auto& camTransform = world->AddComponent<TransformComponent>(cameraEntity);
camTransform.SetPosition(Vector3(0, 5, 10));
camTransform.LookAt(Vector3(0, 0, 0));

// 添加 Camera 组件
auto& cameraComp = world->AddComponent<CameraComponent>(cameraEntity);
cameraComp.camera = std::make_shared<Camera>();
cameraComp.camera->SetPerspective(45.0f, 16.0f/9.0f, 0.1f, 100.0f);
cameraComp.active = true;
cameraComp.clearColor = Color(0.1f, 0.1f, 0.15f, 1.0f);

// WindowSystem 会自动更新相机的宽高比
```

---

### 3. 创建光源实体

```cpp
// 创建点光源
auto lightEntity = world->CreateEntity();

// 设置光源位置
auto& lightTransform = world->AddComponent<TransformComponent>(lightEntity);
lightTransform.SetPosition(Vector3(5, 5, 5));

// 添加 Light 组件
auto& lightComp = world->AddComponent<LightComponent>(lightEntity);
lightComp.type = LightType::Point;
lightComp.color = Color(1, 1, 1);
lightComp.intensity = 1.0f;
lightComp.range = 20.0f;
lightComp.enabled = true;

// UniformSystem 会自动将光源数据上传到着色器
```

---

### 4. 创建 3D 物体（方式一：使用几何形状）

```cpp
// 创建球体实体
auto sphereEntity = world->CreateEntity();

// 添加 Transform
auto& transform = world->AddComponent<TransformComponent>(sphereEntity);
transform.SetPosition(Vector3(0, 0, 0));
transform.SetScale(2.0f);

// ✨ 新功能：使用 GeometryComponent 自动生成网格
auto& geom = world->AddComponent<GeometryComponent>(sphereEntity);
geom.type = GeometryType::Sphere;
geom.size = 1.0f;
geom.segments = 32;
geom.rings = 16;

// 添加 MeshRender 组件
auto& meshRender = world->AddComponent<MeshRenderComponent>(sphereEntity);
meshRender.materialName = "phong";  // 从 ResourceManager 获取材质

// ✨ 新功能：材质属性覆盖
meshRender.SetDiffuseColor(Color(0.8f, 0.2f, 0.2f));   // 红色
meshRender.SetMetallic(0.5f);
meshRender.SetRoughness(0.3f);
meshRender.SetShininess(64.0f);

// GeometrySystem 会自动生成球体网格
// UniformSystem 会自动设置相机和光源 uniform
```

---

### 5. 创建 3D 物体（方式二：加载外部模型）

```cpp
// 创建模型实体
auto modelEntity = world->CreateEntity();

// 添加 Transform
auto& transform = world->AddComponent<TransformComponent>(modelEntity);
transform.SetPosition(Vector3(0, 0, 0));

// 添加 MeshRender 组件
auto& meshRender = world->AddComponent<MeshRenderComponent>(modelEntity);
meshRender.meshName = "models/character.obj";      // ✨ 通过 ResourceManager 管理
meshRender.materialName = "character_material";    // ✨ 统一资源管理
meshRender.shaderName = "phong";                   // ✨ 从 ShaderCache 获取

// 设置渲染属性
meshRender.visible = true;
meshRender.castShadows = true;
meshRender.receiveShadows = true;

// ResourceLoadingSystem 会自动异步加载资源
// 资源会自动注册到 ResourceManager，可被其他实体复用
```

---

### 6. 创建透明物体

```cpp
// 创建玻璃球体
auto glassEntity = world->CreateEntity();
world->AddComponent<TransformComponent>(glassEntity);

// 使用几何组件
auto& geom = world->AddComponent<GeometryComponent>(glassEntity);
geom.type = GeometryType::Sphere;

// 添加网格渲染
auto& meshRender = world->AddComponent<MeshRenderComponent>(glassEntity);
meshRender.materialName = "glass";

// ✨ 新功能：设置透明度会自动启用混合模式
meshRender.SetOpacity(0.3f);  // 30% 不透明度
meshRender.SetDiffuseColor(Color(0.2f, 0.3f, 0.8f));

// MeshRenderSystem 会自动：
// - 启用 BlendMode::Alpha
// - 禁用深度写入
// - 保持深度测试开启
```

---

### 7. 主循环

```cpp
// 主循环
while (!shouldQuit) {
    // 获取帧时间
    float deltaTime = timer.GetDeltaTime();
    
    // ✨ 更新 ECS - 所有系统自动执行
    world->Update(deltaTime);
    // 系统执行顺序：
    // 1. WindowSystem (3)    - 检测窗口变化，更新相机和视口
    // 2. CameraSystem (5)    - 同步相机位置和旋转
    // 3. TransformSystem (10) - 更新变换层级
    // 4. GeometrySystem (15) - 生成几何形状（如需要）
    // 5. ResourceLoadingSystem (20) - 异步加载资源
    // 6. LightSystem (50)    - 收集光源数据
    // 7. UniformSystem (90)  - 自动设置全局 uniform
    // 8. MeshRenderSystem (100) - 提交渲染对象
    
    // 渲染
    renderer->BeginFrame();
    renderer->Clear();
    renderer->FlushRenderQueue();  // 执行实际渲染
    renderer->EndFrame();
    renderer->Present();
}

// 清理
world.reset();  // World 会自动清理所有系统和实体
```

---

## 🎨 高级功能示例

### 实例化渲染（大量相同物体）

```cpp
// 创建实体
auto instancedEntity = world->CreateEntity();
world->AddComponent<TransformComponent>(instancedEntity);

// 使用几何组件
auto& geom = world->AddComponent<GeometryComponent>(instancedEntity);
geom.type = GeometryType::Cube;

// ✨ 启用实例化渲染
auto& meshRender = world->AddComponent<MeshRenderComponent>(instancedEntity);
meshRender.materialName = "default";
meshRender.useInstancing = true;
meshRender.instanceCount = 1000;

// 设置实例变换矩阵
meshRender.instanceTransforms.resize(1000);
for (int i = 0; i < 1000; i++) {
    float x = (i % 10) * 2.0f;
    float z = (i / 10) * 2.0f;
    meshRender.instanceTransforms[i] = Matrix4::Identity();
    meshRender.instanceTransforms[i].block<3,1>(0,3) = Vector3(x, 0, z);
}

// 注意：实例化渲染需要 Mesh 和 Material 支持（待实现）
```

---

### 离屏渲染（后处理、阴影贴图）

```cpp
// 创建离屏渲染相机
auto offscreenCam = world->CreateEntity();
auto& camTransform = world->AddComponent<TransformComponent>(offscreenCam);
camTransform.SetPosition(Vector3(0, 10, 0));
camTransform.LookAt(Vector3(0, 0, 0));

auto& cameraComp = world->AddComponent<CameraComponent>(offscreenCam);
cameraComp.camera = std::make_shared<Camera>();
cameraComp.camera->SetPerspective(45.0f, 1.0f, 0.1f, 100.0f);

// ✨ 创建离屏渲染目标
auto fbo = std::make_shared<Framebuffer>(1024, 1024);
fbo->AttachColorTexture();
fbo->AttachDepthTexture();

cameraComp.renderTarget = fbo;
cameraComp.renderTargetName = "shadowMap";
cameraComp.clearColor = Color(1, 1, 1, 1);
cameraComp.depth = -1;  // 先于主相机渲染

// 在材质中使用渲染结果
auto& resMgr = ResourceManager::GetInstance();
auto material = resMgr.GetMaterial("myMaterial");
if (material) {
    material->SetTexture("shadowMap", fbo->GetColorTexture(0));
}
```

---

### 动态材质属性

```cpp
// 运行时修改材质
auto& meshRender = world->GetComponent<MeshRenderComponent>(entity);

// 方式一：修改 Material 对象（永久修改）
if (meshRender.material) {
    meshRender.material->SetDiffuseColor(Color(1, 0, 0));
    meshRender.material->SetMetallic(0.8f);
}

// 方式二：使用覆盖（每帧应用，不修改原材质）
meshRender.SetDiffuseColor(Color(1, 0, 0));
meshRender.SetMetallic(0.8f);
meshRender.SetRoughness(0.2f);

// 清除覆盖（恢复材质默认值）
meshRender.ClearMaterialOverrides();
```

---

### 多纹理支持

```cpp
// 在 MeshRenderComponent 中设置纹理覆盖
meshRender.textureOverrides["normalMap"] = "textures/brick_normal.png";
meshRender.textureOverrides["specularMap"] = "textures/brick_specular.png";

// 纹理设置
meshRender.textureSettings["diffuseMap"].generateMipmaps = true;

// 注意：纹理加载和应用需要在 ResourceLoadingSystem 中处理
```

---

## 🔧 常见模式

### 批量创建物体

```cpp
// 创建一组立方体
for (int i = 0; i < 10; i++) {
    auto entity = world->CreateEntity();
    
    // Transform
    auto& transform = world->AddComponent<TransformComponent>(entity);
    transform.SetPosition(Vector3(i * 2.0f, 0, 0));
    
    // 几何
    auto& geom = world->AddComponent<GeometryComponent>(entity);
    geom.type = GeometryType::Cube;
    geom.size = 1.0f;
    
    // 渲染
    auto& meshRender = world->AddComponent<MeshRenderComponent>(entity);
    meshRender.materialName = "default";
    
    // 随机颜色
    float hue = i / 10.0f;
    meshRender.SetDiffuseColor(Color(hue, 1.0f - hue, 0.5f));
}

// GeometrySystem 会自动生成所有立方体
// UniformSystem 会自动设置 uniform
```

---

### 资源复用

```cpp
// 第一个实体加载网格
auto entity1 = world->CreateEntity();
world->AddComponent<TransformComponent>(entity1);
auto& mesh1 = world->AddComponent<MeshRenderComponent>(entity1);
mesh1.meshName = "models/rock.obj";
mesh1.materialName = "rock";

// 第二个实体复用相同网格（不会重复加载）
auto entity2 = world->CreateEntity();
world->AddComponent<TransformComponent>(entity2).SetPosition(Vector3(5, 0, 0));
auto& mesh2 = world->AddComponent<MeshRenderComponent>(entity2);
mesh2.meshName = "models/rock.obj";  // ✅ 从 ResourceManager 缓存获取
mesh2.materialName = "rock";

// ✨ 但可以覆盖材质属性
mesh2.SetDiffuseColor(Color(0.5f, 0.8f, 0.3f));  // 不同颜色
```

---

### 运行时切换着色器

```cpp
auto& meshRender = world->GetComponent<MeshRenderComponent>(entity);

// ✨ 从 ShaderCache 获取并设置新着色器
meshRender.shaderName = "toon";  // 切换到卡通着色器

// ResourceLoadingSystem 会在下一帧应用
// 或者直接设置：
auto& shaderCache = ShaderCache::GetInstance();
auto shader = shaderCache.GetShader("toon");
if (shader && meshRender.material) {
    meshRender.material->SetShader(shader);
}
```

---

## 📊 性能优化提示

### 1. 视锥体裁剪（自动启用）

```cpp
// ✅ 视锥体裁剪已自动启用
// MeshRenderSystem 会自动剔除不可见物体
// 查看统计信息：
auto stats = meshRenderSystem->GetStats();
Logger::Info("可见: %zu, 裁剪: %zu", 
    stats.visibleMeshes, stats.culledMeshes);
```

---

### 2. 材质批处理

```cpp
// 使用相同材质的物体会自动批处理（如果着色器相同）
for (int i = 0; i < 100; i++) {
    auto entity = world->CreateEntity();
    // ...
    auto& meshRender = world->AddComponent<MeshRenderComponent>(entity);
    meshRender.materialName = "default";  // ✅ 使用相同材质
    
    // 只覆盖颜色（不改变材质，减少状态切换）
    meshRender.SetDiffuseColor(RandomColor());
}
```

---

### 3. LOD 距离（待实现）

```cpp
// LOD 接口已预留
auto& meshRender = world->AddComponent<MeshRenderComponent>(entity);
meshRender.lodDistances = {10.0f, 50.0f, 100.0f};
// 距离 < 10m: LOD 0 (高精度)
// 距离 10-50m: LOD 1 (中精度)
// 距离 50-100m: LOD 2 (低精度)
// 距离 > 100m: 不渲染或最低精度
```

---

## 🎯 系统优先级说明

| 优先级 | 系统 | 说明 |
|--------|------|------|
| 3 | WindowSystem | 必须最先执行，更新视口和相机宽高比 |
| 5 | CameraSystem | 更新相机位置和旋转 |
| 10 | TransformSystem | 更新变换层级（父子关系） |
| 15 | GeometrySystem | 生成几何形状 |
| 20 | ResourceLoadingSystem | 加载异步资源 |
| 50 | LightSystem | 收集光源数据 |
| 90 | UniformSystem | 设置全局 uniform |
| 100 | MeshRenderSystem | 提交渲染对象 |

**执行顺序**：优先级**低的先执行**

---

## 🐛 调试技巧

### 1. 查看系统执行顺序

```cpp
// 在每个系统的 Update 中添加日志
void MySystem::Update(float deltaTime) {
    Logger::InfoFormat("[MySystem] Priority: %d, DeltaTime: %.4f", 
                      GetPriority(), deltaTime);
    // ...
}
```

---

### 2. 检查资源加载状态

```cpp
auto& meshRender = world->GetComponent<MeshRenderComponent>(entity);

Logger::InfoFormat("Resource Status: loaded=%d, loading=%d, hasMesh=%d, hasMaterial=%d",
    meshRender.resourcesLoaded,
    meshRender.asyncLoading,
    meshRender.mesh != nullptr,
    meshRender.material != nullptr);
```

---

### 3. 查看渲染统计

```cpp
// 在 MeshRenderSystem 中
auto stats = meshRenderSystem->GetStats();
Logger::InfoFormat("Render Stats: visible=%zu, culled=%zu, drawCalls=%zu",
    stats.visibleMeshes,
    stats.culledMeshes,
    stats.drawCalls);
```

---

### 4. 错误处理

```cpp
// ✅ ErrorHandler 已集成
// 所有 RENDER_ASSERT 失败会自动记录到日志

// 查看错误统计
auto& errorHandler = ErrorHandler::GetInstance();
auto stats = errorHandler.GetStats();
Logger::InfoFormat("Errors: %zu warnings, %zu errors, %zu critical",
    stats.warningCount,
    stats.errorCount,
    stats.criticalCount);
```

---

## ⚠️ 常见问题

### Q: 为什么物体不显示？

**检查清单**：
1. ✅ 资源是否加载完成？（`meshRender.resourcesLoaded`）
2. ✅ 物体是否可见？（`meshRender.visible = true`）
3. ✅ 材质和网格是否有效？（`mesh != nullptr`, `material != nullptr`）
4. ✅ 相机是否激活？（`cameraComp.active = true`）
5. ✅ 物体是否在视锥体内？（检查位置和裁剪统计）

---

### Q: Uniform 没有生效？

**检查清单**：
1. ✅ 是否注册了 UniformSystem？
2. ✅ 着色器是否定义了对应的 uniform？（`uView`, `uProjection`, `uLightPos` 等）
3. ✅ 着色器是否正确绑定到材质？
4. ✅ 使用 `uniformMgr->HasUniform("uView")` 检查 uniform 是否存在

---

### Q: 资源重复加载？

**解决方案**：
- ✅ 确保使用相同的 `meshName` / `materialName`
- ✅ ResourceManager 会自动缓存和复用
- ✅ 第一次加载后，后续实体会直接从缓存获取

```cpp
// 正确：使用相同名称
mesh1.meshName = "models/cube.obj";  // 第一次加载
mesh2.meshName = "models/cube.obj";  // ✅ 从缓存获取

// 错误：使用不同路径
mesh1.meshName = "models/cube.obj";
mesh2.meshName = "./models/cube.obj";  // ❌ 会重复加载
```

---

### Q: 窗口大小变化后画面拉伸？

**解决方案**：
- ✅ 已自动处理！WindowSystem 会自动更新相机宽高比
- ✅ 视口也会自动调整

---

## 📚 相关文档

- [ECS 安全性改进](ECS_SAFETY_IMPROVEMENTS.md)
- [ECS 核心功能利用分析](todolists/ECS_CORE_FEATURE_UTILIZATION.md)
- [Material API](api/Material.md)
- [UniformManager API](api/UniformManager.md)
- [RenderState API](api/RenderState.md)

---

## 🎉 总结

ECS 系统现已完整集成所有核心渲染功能：

✅ **自动化管理** - UniformSystem、WindowSystem  
✅ **资源统一** - ResourceManager、ShaderCache  
✅ **材质增强** - 属性覆盖、动态状态  
✅ **性能优化** - 视锥体裁剪、批处理  
✅ **高级功能** - 几何生成、离屏渲染、实例化  
✅ **错误处理** - 完整的异常和断言系统  

开始使用吧！🚀

---

[下一篇: ECS 安全性改进](ECS_SAFETY_IMPROVEMENTS.md) | [返回文档首页](README.md)

