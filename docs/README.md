# RenderEngine 文档

欢迎使用 RenderEngine 文档！这是一个基于 OpenGL 4.5+ 的现代 C++ 渲染引擎。

---

## 📚 文档导航

### 快速开始
- **[API 参考文档](api/README.md)** - 完整的 API 文档
- **[开发指南](DEVELOPMENT_GUIDE.md)** - 开发环境搭建和基础使用
- **[架构文档](ARCHITECTURE.md)** - 系统架构设计

### 核心系统

#### 渲染系统
- [Renderer API](api/Renderer.md) - 主渲染器
- [RenderState API](api/RenderState.md) - 渲染状态管理
- [OpenGLContext API](api/OpenGLContext.md) - OpenGL 上下文

#### 着色器系统
- [Shader API](api/Shader.md) - 着色器程序
- [ShaderCache API](api/ShaderCache.md) - 着色器缓存
- [UniformManager API](api/UniformManager.md) - Uniform 管理
- [着色器缓存使用指南](SHADER_CACHE_GUIDE.md)

#### 纹理系统
- [Texture API](api/Texture.md) - 纹理对象
- [TextureLoader API](api/TextureLoader.md) - 纹理加载
- [纹理系统使用指南](TEXTURE_SYSTEM.md)

#### 网格系统
- [Mesh API](api/Mesh.md) - 网格对象
- [MeshLoader API](api/MeshLoader.md) - 网格加载和几何生成

#### 材质系统
- [Material API](api/Material.md) - 材质管理
- [材质系统指南](MATERIAL_SYSTEM.md)

#### 资源管理
- [ResourceManager API](api/ResourceManager.md) - 统一资源管理

#### 数学库 ⚡ **新增**
- [Types API](api/Types.md) - 数学类型（Vector, Matrix, Quaternion, Plane, Ray）
- [MathUtils API](api/MathUtils.md) - 数学工具函数 ⚡ 性能优化
- [Transform API](api/Transform.md) - 3D变换类 ⚡ 高性能缓存

#### 工具类
- [Logger API](api/Logger.md) - 日志系统
- [FileUtils API](api/FileUtils.md) - 文件工具

### 专题文档

#### ECS 系统 🏗️ **已完善** ⭐⭐⭐⭐⭐
- **[ECS 快速入门](ECS_QUICK_START.md)** - 快速开始使用 ECS 系统 ⭐ **推荐新手阅读**
- **[ECS 安全性分析](ECS_SECURITY_ANALYSIS.md)** - 全面的安全性审查报告 🆕 **v1.1**
- **[ECS 安全性改进](ECS_SAFETY_IMPROVEMENTS.md)** - v1.1 安全性和性能改进详解 🆕
- [ECS 与 Renderable API](api/ECS.md) - ECS 系统 API 文档（v1.1 已更新）
- [ECS 核心功能利用分析](todolists/ECS_CORE_FEATURE_UTILIZATION.md) - 功能利用率分析

#### 线程安全 🔒
- [Renderer 线程安全](RENDERER_THREAD_SAFETY.md)
- [RenderState 线程安全](THREAD_SAFETY.md)
- [网格系统线程安全](MESH_THREAD_SAFETY.md)

### 项目管理
- [Phase 1 进度列表](todolists/PHASE1_BASIC_RENDERING.md)
- [功能列表](FEATURE_LIST.md)
- [贡献指南](CONTRIBUTING.md)

---

## 🚀 快速开始

### 1. 创建基础窗口

```cpp
#include <render/renderer.h>

int main() {
    Render::Renderer* renderer = Render::Renderer::Create();
    renderer->Initialize("My App", 1280, 720);
    
    while (running) {
        renderer->BeginFrame();
        renderer->Clear();
        // 渲染代码...
        renderer->EndFrame();
        renderer->Present();
    }
    
    Render::Renderer::Destroy(renderer);
    return 0;
}
```

### 2. 使用数学库 ⚡

```cpp
#include <render/transform.h>
#include <render/math_utils.h>

using namespace Render;

// 创建变换
Transform transform;
transform.SetPosition(Vector3(10.0f, 5.0f, 0.0f));
transform.SetRotationEulerDegrees(Vector3(0.0f, 45.0f, 0.0f));
transform.SetScale(2.0f);

// 朝向目标
transform.LookAt(Vector3(0.0f, 0.0f, 0.0f));

// 获取变换矩阵
Matrix4 worldMatrix = transform.GetWorldMatrix();

// 使用数学工具
Quaternion rot = MathUtils::FromEulerDegrees(45.0f, 30.0f, 0.0f);
Matrix4 proj = MathUtils::PerspectiveDegrees(60.0f, aspect, 0.1f, 100.0f);
```

### 3. 加载和渲染模型

```cpp
#include <render/mesh_loader.h>
#include <render/resource_manager.h>

// 加载模型
auto results = MeshLoader::LoadFromFileWithMaterials("model.obj");

// 使用资源管理器管理资源
ResourceManager resourceMgr;
for (auto& result : results) {
    resourceMgr.RegisterMesh(result.name, result.mesh);
    if (result.material) {
        resourceMgr.RegisterMaterial(result.name + "_mat", result.material);
    }
}

// 渲染
for (auto& result : results) {
    if (result.material) {
        result.material->Bind();
    }
    result.mesh->Draw();
}
```

---

## 📊 系统特性

### 已完成功能

- ✅ OpenGL 4.5+ 抽象层
- ✅ 着色器系统（顶点/片段/几何着色器）
- ✅ 着色器缓存和热重载
- ✅ 纹理系统（PNG/JPG/BMP/TGA）
- ✅ 网格系统（10种几何形状 + 外部模型加载）
- ✅ 材质系统（Phong 光照）
- ✅ 资源管理器（统一管理）
- ✅ **数学库集成**（Transform + MathUtils）⚡ **新增**
- ✅ 渲染状态管理
- ✅ OpenGL 状态封装
- ✅ 日志系统
- ✅ **全面线程安全** 🔒
- ✅ **性能优化**（SIMD + 缓存 + 并行）⚡ **新增**

### 性能特性

- ⚡ AVX2 SIMD 指令集优化
- ⚡ 智能缓存机制（Transform 缓存提升 10-50x）
- ⚡ OpenMP 并行处理（批量操作提升 2-4x）
- ⚡ 高效的数学运算（FromEuler 快 40-50%）
- 🔒 全面线程安全设计

---

## 📖 示例程序

项目包含 19 个示例程序，涵盖所有功能：

| 编号 | 名称 | 说明 |
|------|------|------|
| 01 | basic_window | 基础窗口创建 |
| 02 | shader_test | 着色器系统 |
| 03 | geometry_shader_test | 几何着色器 |
| 04 | state_management_test | 状态管理 |
| 05 | texture_test | 纹理系统 |
| 06 | mesh_test | 网格和几何形状 |
| 07-10 | thread_safe_test | 各系统线程安全测试 🔒 |
| 11 | model_loader_test | 外部模型加载 |
| 12-13 | material_test | 材质系统 |
| 14 | model_material_loader_test | 模型材质加载 |
| 15-17 | resource_manager_test | 资源管理器 |
| **18** | **math_test** | **数学库功能测试** ⭐ |
| **19** | **math_benchmark** | **数学性能基准** ⚡ ⭐ |

---

## 🎯 下一步

### ✅ 已完成（ECS 系统）
- ✅ 相机系统（CameraSystem + UniformSystem）
- ✅ 光照系统（LightSystem + UniformSystem）
- ✅ 渲染层级（layerID + renderPriority）
- ✅ 材质系统完整集成
- ✅ 资源统一管理
- ✅ 视锥体裁剪优化
- ✅ 窗口响应系统
- ✅ 几何形状生成
- ✅ 错误处理集成

### 正在开发
- [ ] 阴影系统（离屏渲染已支持）
- [ ] 后处理效果（Framebuffer 已支持）
- [ ] 实例化渲染完整实现

### 计划中
- [ ] 粒子系统
- [ ] 骨骼动画
- [ ] PBR 材质工作流

---

## 💬 获取帮助

- 查看 [API 文档](api/README.md)
- 运行示例程序学习
- 阅读 [开发指南](DEVELOPMENT_GUIDE.md)

---

[返回项目主页](../README.md)
