# MeshRenderable API 参考

[返回 API 目录](README.md) | [返回 ECS 概览](ECS.md) | [返回 Renderable](Renderable.md)

---

## 📋 概述

MeshRenderable 是用于渲染 3D 网格的可渲染对象，继承自 `Renderable` 基类。它支持网格、材质、阴影、包围盒计算等功能。

**命名空间**：`Render`

**头文件**：`<render/renderable.h>`

---

## 🏗️ 类定义

```cpp
class MeshRenderable : public Renderable {
public:
    MeshRenderable();
    ~MeshRenderable() override = default;
    
    // 禁止拷贝
    MeshRenderable(const MeshRenderable&) = delete;
    MeshRenderable& operator=(const MeshRenderable&) = delete;
    
    // 支持移动
    MeshRenderable(MeshRenderable&& other) noexcept;
    MeshRenderable& operator=(MeshRenderable&& other) noexcept;
    
    // 渲染
    void Render() override;
    void SubmitToRenderer(Renderer* renderer) override;
    
    // 资源设置
    void SetMesh(const Ref<Mesh>& mesh);
    Ref<Mesh> GetMesh() const;
    
    void SetMaterial(const Ref<Material>& material);
    Ref<Material> GetMaterial() const;
    
    // 阴影
    void SetCastShadows(bool cast);
    bool GetCastShadows() const;
    
    void SetReceiveShadows(bool receive);
    bool GetReceiveShadows() const;
    
    // 包围盒
    AABB GetBoundingBox() const override;
    
private:
    Ref<Mesh> m_mesh;
    Ref<Material> m_material;
    bool m_castShadows = true;
    bool m_receiveShadows = true;
};
```

---

## 🔧 成员函数详解

### 构造函数

#### `MeshRenderable()`

构造函数，创建 3D 网格渲染对象。

```cpp
MeshRenderable();
```

**说明**：
- 自动设置类型为 `RenderableType::Mesh`
- 默认可见
- 默认层级为 `300`（WORLD_GEOMETRY）

**示例**：
```cpp
MeshRenderable renderable;
```

---

### 渲染

#### `Render()`

渲染网格。

```cpp
void Render() override;
```

**说明**：
- 检查可见性、网格和材质是否有效
- 绑定材质
- 设置模型矩阵 uniform
- 绘制网格

**实现**：
```cpp
void MeshRenderable::Render() {
    std::shared_lock lock(m_mutex);
    
    if (!m_visible || !m_mesh || !m_material) {
        return;
    }
    
    // 绑定材质
    m_material->Bind();
    
    // 获取着色器并设置模型矩阵
    auto shader = m_material->GetShader();
    if (shader && m_transform) {
        Matrix4 modelMatrix = m_transform->GetWorldMatrix();
        shader->GetUniformManager()->SetMatrix4("uModel", modelMatrix);
    }
    
    // 绘制网格
    m_mesh->Draw();
}
```

#### `SubmitToRenderer()`

提交到渲染器。

```cpp
void SubmitToRenderer(Renderer* renderer) override;
```

**参数**：
- `renderer` - 渲染器指针

**说明**：
- 将自己提交到渲染队列
- 通常由 `MeshRenderSystem` 调用

**示例**：
```cpp
renderable.SubmitToRenderer(renderer);
```

---

### 资源设置

#### `SetMesh()` / `GetMesh()`

设置/获取网格对象。

```cpp
void SetMesh(const Ref<Mesh>& mesh);
Ref<Mesh> GetMesh() const;
```

**参数**：
- `mesh` - 网格对象（`std::shared_ptr<Mesh>`）

**示例**：
```cpp
// 加载网格
auto mesh = MeshLoader::LoadFromFile("models/cube.obj");
renderable.SetMesh(mesh);

// 获取网格
auto mesh = renderable.GetMesh();
if (mesh) {
    size_t vertexCount = mesh->GetVertexCount();
}
```

#### `SetMaterial()` / `GetMaterial()`

设置/获取材质对象。

```cpp
void SetMaterial(const Ref<Material>& material);
Ref<Material> GetMaterial() const;
```

**参数**：
- `material` - 材质对象（`std::shared_ptr<Material>`）

**示例**：
```cpp
// 创建材质
auto material = std::make_shared<Material>();
material->SetShader(shader);
material->SetDiffuseColor(Color(1, 0, 0, 1));  // 红色
renderable.SetMaterial(material);

// 获取材质
auto material = renderable.GetMaterial();
```

---

### 阴影

#### `SetCastShadows()` / `GetCastShadows()`

设置/获取是否投射阴影。

```cpp
void SetCastShadows(bool cast);
bool GetCastShadows() const;
```

**参数**：
- `cast` - 是否投射阴影

**说明**：
- 默认为 `true`
- 用于阴影渲染 pass

**示例**：
```cpp
renderable.SetCastShadows(true);

if (renderable.GetCastShadows()) {
    // 渲染到阴影贴图
}
```

#### `SetReceiveShadows()` / `GetReceiveShadows()`

设置/获取是否接收阴影。

```cpp
void SetReceiveShadows(bool receive);
bool GetReceiveShadows() const;
```

**参数**：
- `receive` - 是否接收阴影

**说明**：
- 默认为 `true`
- 影响着色器中的阴影计算

**示例**：
```cpp
renderable.SetReceiveShadows(true);
```

---

### 包围盒

#### `GetBoundingBox()`

获取包围盒。

```cpp
AABB GetBoundingBox() const override;
```

**返回值**：轴对齐包围盒（AABB）。

**说明**：
- 计算网格在世界空间的包围盒
- 如果没有网格，返回空包围盒
- 考虑变换矩阵（位置、旋转、缩放）
- 用于视锥体裁剪

**实现原理**：
1. 从网格顶点计算局部空间包围盒
2. 应用世界变换矩阵
3. 变换包围盒的8个顶点
4. 计算新的轴对齐包围盒

**示例**：
```cpp
AABB bounds = renderable.GetBoundingBox();
std::cout << "Min: " << bounds.min << std::endl;
std::cout << "Max: " << bounds.max << std::endl;

// 视锥体裁剪
if (camera.IsVisible(bounds)) {
    renderable.Render();
}
```

---

## 🎯 完整使用示例

### 基本使用

```cpp
#include <render/renderable.h>
#include <render/mesh_loader.h>
#include <render/material.h>

// 创建 MeshRenderable
MeshRenderable renderable;

// 设置变换
auto transform = std::make_shared<Transform>();
transform->SetPosition(Vector3(0, 1, 0));
transform->SetRotation(MathUtils::FromEulerDegrees(0, 45, 0));
transform->SetScale(2.0f);
renderable.SetTransform(transform);

// 加载网格
auto mesh = MeshLoader::LoadFromFile("models/cube.obj");
renderable.SetMesh(mesh);

// 创建材质
auto material = std::make_shared<Material>();
material->SetShader(shader);
material->SetDiffuseColor(Color(1, 0, 0, 1));  // 红色
material->SetSpecularColor(Color(1, 1, 1, 1));
material->SetShininess(32.0f);
renderable.SetMaterial(material);

// 设置渲染属性
renderable.SetVisible(true);
renderable.SetLayerID(300);  // WORLD_GEOMETRY
renderable.SetCastShadows(true);
renderable.SetReceiveShadows(true);

// 渲染
renderable.Render();
```

---

### 与 ECS 集成

在 ECS 系统中，`MeshRenderable` 由 `MeshRenderSystem` 自动创建和管理：

```cpp
// 创建实体
EntityID entity = world->CreateEntity({.name = "Cube"});

// 添加 Transform 组件
TransformComponent transform;
transform.SetPosition(Vector3(0, 1, 0));
world->AddComponent(entity, transform);

// 添加 MeshRenderComponent
MeshRenderComponent mesh;
mesh.meshName = "models/cube.obj";  // 异步加载
mesh.materialName = "default";
mesh.visible = true;
mesh.castShadows = true;
mesh.receiveShadows = true;
mesh.layerID = 300;
world->AddComponent(entity, mesh);

// MeshRenderSystem 会在每帧：
// 1. 查询所有具有 TransformComponent 和 MeshRenderComponent 的实体
// 2. 创建 MeshRenderable 对象（使用对象池）
// 3. 设置 mesh、material、transform 等
// 4. 提交到渲染队列
```

---

### 异步资源加载

```cpp
// ECS 方式（推荐）
MeshRenderComponent mesh;
mesh.meshName = "models/large_model.fbx";  // 设置资源名称
mesh.materialName = "default";
mesh.resourcesLoaded = false;  // 尚未加载
mesh.asyncLoading = false;     // 尚未开始

world->AddComponent(entity, mesh);

// ResourceLoadingSystem 会：
// 1. 检测到 meshName 非空且 mesh == nullptr
// 2. 调用 AsyncResourceLoader::LoadMeshAsync()
// 3. 在后台线程加载网格
// 4. 在主线程上传 GPU 数据
// 5. 回调设置 mesh.mesh 和 mesh.resourcesLoaded = true
// 6. MeshRenderSystem 检测到 resourcesLoaded，开始渲染
```

---

### 动态修改

```cpp
// 运行时修改网格
auto newMesh = MeshLoader::LoadFromFile("models/sphere.obj");
renderable.SetMesh(newMesh);

// 运行时修改材质
auto newMaterial = std::make_shared<Material>();
newMaterial->SetShader(newShader);
renderable.SetMaterial(newMaterial);

// 切换可见性
renderable.SetVisible(false);  // 隐藏
SDL_Delay(1000);
renderable.SetVisible(true);   // 显示

// 修改阴影属性
renderable.SetCastShadows(false);  // 不投射阴影
```

---

## 💡 使用建议

### 1. 复用对象

使用对象池避免频繁创建销毁：

```cpp
// ✅ 好：对象池
std::vector<MeshRenderable> renderables;
renderables.reserve(1000);

for (auto entity : entities) {
    renderables.emplace_back();  // 复用内存
    auto& renderable = renderables.back();
    // 设置 renderable...
}

renderables.clear();  // 不释放内存

// ❌ 差：每次创建
for (auto entity : entities) {
    MeshRenderable renderable;  // 每次都创建销毁
}
```

### 2. 检查资源有效性

```cpp
// ✅ 好：检查资源
auto mesh = renderable.GetMesh();
auto material = renderable.GetMaterial();

if (mesh && material) {
    renderable.Render();
}

// ❌ 差：不检查
renderable.Render();  // 如果 mesh 或 material 为空，会跳过渲染
```

### 3. 视锥体裁剪

```cpp
// ✅ 好：裁剪不可见对象
AABB bounds = renderable.GetBoundingBox();
if (camera.IsVisible(bounds)) {
    renderable.Render();  // 只渲染可见对象
}

// ❌ 差：渲染所有对象
renderable.Render();  // 浪费 GPU 资源
```

### 4. 材质批次

```cpp
// ✅ 好：按材质分组渲染（减少状态切换）
std::sort(renderables.begin(), renderables.end(), 
    [](const MeshRenderable& a, const MeshRenderable& b) {
        return a.GetMaterial() < b.GetMaterial();
    });

// ❌ 差：随机顺序渲染
for (auto& renderable : renderables) {
    renderable.Render();  // 频繁切换材质
}
```

---

## 🔧 包围盒计算详解

### 算法流程

```cpp
AABB MeshRenderable::GetBoundingBox() const {
    std::shared_lock lock(m_mutex);
    
    if (!m_mesh) {
        return AABB();  // 空包围盒
    }
    
    // 1. 计算局部空间包围盒
    AABB localBounds;
    bool boundsValid = false;
    
    m_mesh->AccessVertices([&](const std::vector<Vertex>& vertices) {
        if (vertices.empty()) {
            return;
        }
        
        Vector3 minPoint = vertices[0].position;
        Vector3 maxPoint = vertices[0].position;
        
        for (const auto& vertex : vertices) {
            minPoint = minPoint.cwiseMin(vertex.position);
            maxPoint = maxPoint.cwiseMax(vertex.position);
        }
        
        localBounds = AABB(minPoint, maxPoint);
        boundsValid = true;
    });
    
    if (!boundsValid) {
        return AABB();
    }
    
    // 2. 如果有变换，转换到世界空间
    if (m_transform) {
        Matrix4 worldMatrix = m_transform->GetWorldMatrix();
        
        // 变换包围盒的8个顶点
        std::vector<Vector3> corners = {
            Vector3(localBounds.min.x(), localBounds.min.y(), localBounds.min.z()),
            Vector3(localBounds.max.x(), localBounds.min.y(), localBounds.min.z()),
            Vector3(localBounds.min.x(), localBounds.max.y(), localBounds.min.z()),
            Vector3(localBounds.max.x(), localBounds.max.y(), localBounds.min.z()),
            Vector3(localBounds.min.x(), localBounds.min.y(), localBounds.max.z()),
            Vector3(localBounds.max.x(), localBounds.min.y(), localBounds.max.z()),
            Vector3(localBounds.min.x(), localBounds.max.y(), localBounds.max.z()),
            Vector3(localBounds.max.x(), localBounds.max.y(), localBounds.max.z())
        };
        
        // 计算新的轴对齐包围盒
        Vector3 worldMin = (worldMatrix * Vector4(corners[0].x(), corners[0].y(), corners[0].z(), 1.0f)).head<3>();
        Vector3 worldMax = worldMin;
        
        for (const auto& corner : corners) {
            Vector3 transformed = (worldMatrix * Vector4(corner.x(), corner.y(), corner.z(), 1.0f)).head<3>();
            worldMin = worldMin.cwiseMin(transformed);
            worldMax = worldMax.cwiseMax(transformed);
        }
        
        return AABB(worldMin, worldMax);
    }
    
    return localBounds;
}
```

### 使用包围盒

```cpp
// 视锥体裁剪
bool IsVisible(const Camera& camera, const MeshRenderable& renderable) {
    AABB bounds = renderable.GetBoundingBox();
    
    // 检查包围盒是否在视锥体内
    return camera.GetFrustum().Intersects(bounds);
}

// 碰撞检测
bool CheckCollision(const MeshRenderable& a, const MeshRenderable& b) {
    AABB boundsA = a.GetBoundingBox();
    AABB boundsB = b.GetBoundingBox();
    
    return boundsA.Intersects(boundsB);
}

// 拾取检测
bool RayCast(const Ray& ray, const MeshRenderable& renderable, float& distance) {
    AABB bounds = renderable.GetBoundingBox();
    
    if (bounds.Intersects(ray, distance)) {
        // 进一步进行精确的网格拾取
        return true;
    }
    
    return false;
}
```

---

## 📊 性能优化

### 1. 对象池

```cpp
// MeshRenderSystem 中使用对象池
class MeshRenderSystem : public System {
private:
    std::vector<MeshRenderable> m_renderables;
    
public:
    void Update(float deltaTime) override {
        m_renderables.clear();  // 不释放内存
        
        auto entities = m_world->Query<TransformComponent, MeshRenderComponent>();
        m_renderables.reserve(entities.size());
        
        for (auto entity : entities) {
            m_renderables.emplace_back();
            // ...
        }
    }
};
```

### 2. 视锥体裁剪

```cpp
// 跳过不可见对象
AABB bounds = renderable.GetBoundingBox();
if (!camera.IsVisible(bounds)) {
    stats.culledMeshes++;
    continue;
}
stats.visibleMeshes++;
```

### 3. 材质批次

```cpp
// 按材质排序，减少状态切换
std::sort(renderables.begin(), renderables.end(),
    [](const MeshRenderable& a, const MeshRenderable& b) {
        return a.GetMaterial().get() < b.GetMaterial().get();
    });
```

---

## 🔒 线程安全

`MeshRenderable` 使用 `std::shared_mutex` 保护所有成员变量：

- 所有 getter 使用共享锁（`std::shared_lock`）
- 所有 setter 使用独占锁（`std::unique_lock`）
- `Render()` 使用共享锁（只读操作）

---

## 📖 相关文档

- [Renderable 基类](Renderable.md)
- [SpriteRenderable](SpriteRenderable.md)
- [Mesh API](Mesh.md)
- [Material API](Material.md)
- [Transform API](Transform.md)
- [ECS 概览](ECS.md)

---

[返回 API 目录](README.md) | [返回 ECS 概览](ECS.md) | [返回 Renderable](Renderable.md)

