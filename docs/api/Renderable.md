# Renderable API 参考

[返回 API 目录](README.md) | [返回 ECS 概览](ECS.md)

---

## 📋 概述

Renderable 是所有可渲染对象的基类，提供统一的渲染接口。它与 ECS 系统深度集成，通过 `MeshRenderSystem` 和 `SpriteRenderSystem` 自动创建和提交渲染对象。

**命名空间**：`Render`

**头文件**：`<render/renderable.h>`

---

## 🎨 RenderableType

渲染对象类型枚举。

```cpp
enum class RenderableType {
    Mesh,       // 3D 网格
    Sprite,     // 2D 精灵
    Text,       // 文本（未来）
    Particle,   // 粒子（未来）
    Custom      // 自定义
};
```

---

## 🏗️ Renderable 基类

所有可渲染对象的基类。

### 类定义

```cpp
class Renderable {
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    
public:
    explicit Renderable(RenderableType type);
    virtual ~Renderable() = default;
    
    // 禁止拷贝
    Renderable(const Renderable&) = delete;
    Renderable& operator=(const Renderable&) = delete;
    
    // 支持移动
    Renderable(Renderable&& other) noexcept;
    Renderable& operator=(Renderable&& other) noexcept;
    
    // 渲染接口
    virtual void Render() = 0;
    virtual void SubmitToRenderer(Renderer* renderer) = 0;
    
    // 变换
    void SetTransform(const Ref<Transform>& transform);
    Ref<Transform> GetTransform() const;
    Matrix4 GetWorldMatrix() const;
    
    // 可见性
    void SetVisible(bool visible);
    bool IsVisible() const;
    
    // 层级
    void SetLayerID(uint32_t layerID);
    uint32_t GetLayerID() const;
    
    void SetRenderPriority(int32_t priority);
    int32_t GetRenderPriority() const;
    
    // 类型
    RenderableType GetType() const;
    
    // 包围盒
    virtual AABB GetBoundingBox() const = 0;
    
protected:
    RenderableType m_type;
    Ref<Transform> m_transform;
    bool m_visible = true;
    uint32_t m_layerID = 300;      // WORLD_GEOMETRY
    int32_t m_renderPriority = 0;
    
    mutable std::shared_mutex m_mutex;
};
```

---

## 🔧 成员函数详解

### 构造函数

#### `Renderable()`

构造函数。

```cpp
explicit Renderable(RenderableType type);
```

**参数**：
- `type` - 渲染对象类型

**说明**：
- 仅供子类调用
- 自动设置对象类型

---

### 渲染接口

#### `Render()`

渲染对象（纯虚函数）。

```cpp
virtual void Render() = 0;
```

**说明**：
- 由子类实现具体的渲染逻辑
- 通常包括：绑定材质、设置 uniform、绘制网格

**示例**：
```cpp
// MeshRenderable 的实现
void MeshRenderable::Render() {
    if (!m_visible || !m_mesh || !m_material) {
        return;
    }
    
    m_material->Bind();
    
    auto shader = m_material->GetShader();
    if (shader && m_transform) {
        Matrix4 modelMatrix = m_transform->GetWorldMatrix();
        shader->GetUniformManager()->SetMatrix4("uModel", modelMatrix);
    }
    
    m_mesh->Draw();
}
```

#### `SubmitToRenderer()`

提交到渲染器（纯虚函数）。

```cpp
virtual void SubmitToRenderer(Renderer* renderer) = 0;
```

**参数**：
- `renderer` - 渲染器指针

**说明**：
- 由子类实现
- 通常调用 `renderer->SubmitRenderable(this)`

---

### 变换

#### `SetTransform()` / `GetTransform()`

设置/获取变换对象。

```cpp
void SetTransform(const Ref<Transform>& transform);
Ref<Transform> GetTransform() const;
```

**说明**：
- 使用 `std::shared_ptr` 复用 Transform 对象
- 支持父子层级关系

**示例**：
```cpp
auto transform = std::make_shared<Transform>();
transform->SetPosition(Vector3(0, 1, 0));
renderable->SetTransform(transform);

// 获取变换
auto t = renderable->GetTransform();
Vector3 pos = t->GetPosition();
```

#### `GetWorldMatrix()`

获取世界变换矩阵。

```cpp
Matrix4 GetWorldMatrix() const;
```

**返回值**：世界变换矩阵（4x4）。

**说明**：
- 如果没有设置 Transform，返回单位矩阵
- 自动处理父子关系

**示例**：
```cpp
Matrix4 worldMatrix = renderable->GetWorldMatrix();
```

---

### 可见性

#### `SetVisible()` / `IsVisible()`

设置/获取可见性。

```cpp
void SetVisible(bool visible);
bool IsVisible() const;
```

**说明**：
- 不可见的对象不会被渲染
- 用于临时隐藏对象

**示例**：
```cpp
renderable->SetVisible(false);  // 隐藏

if (renderable->IsVisible()) {
    // 对象可见
}
```

---

### 层级

#### `SetLayerID()` / `GetLayerID()`

设置/获取渲染层级。

```cpp
void SetLayerID(uint32_t layerID);
uint32_t GetLayerID() const;
```

**常用层级**：
- `100` - SKYBOX
- `200` - BACKGROUND
- `300` - WORLD_GEOMETRY（默认）
- `400` - WORLD_TRANSPARENT
- `500` - FOREGROUND
- `800` - UI_LAYER
- `900` - OVERLAY

**示例**：
```cpp
renderable->SetLayerID(300);  // WORLD_GEOMETRY
```

#### `SetRenderPriority()` / `GetRenderPriority()`

设置/获取渲染优先级。

```cpp
void SetRenderPriority(uint32_t priority);
uint32_t GetRenderPriority() const;
```

**说明**：
- 在同一层级内，优先级越小越先渲染
- 用于控制同层级内的渲染顺序

**示例**：
```cpp
renderable->SetRenderPriority(10);
```

---

### 类型

#### `GetType()`

获取渲染对象类型。

```cpp
RenderableType GetType() const;
```

**返回值**：渲染对象类型枚举。

**示例**：
```cpp
if (renderable->GetType() == RenderableType::Mesh) {
    auto* meshRenderable = static_cast<MeshRenderable*>(renderable);
    // ...
}
```

---

### 包围盒

#### `GetBoundingBox()`

获取包围盒（纯虚函数）。

```cpp
virtual AABB GetBoundingBox() const = 0;
```

**返回值**：轴对齐包围盒（AABB）。

**说明**：
- 用于视锥体裁剪优化
- 由子类实现具体的包围盒计算

---

## 🎯 使用示例

### 基本使用

```cpp
// 创建 MeshRenderable
MeshRenderable renderable;

// 设置变换
auto transform = std::make_shared<Transform>();
transform->SetPosition(Vector3(0, 1, 0));
transform->SetScale(2.0f);
renderable.SetTransform(transform);

// 设置网格和材质
auto mesh = MeshLoader::LoadFromFile("models/cube.obj");
auto material = std::make_shared<Material>();
material->SetShader(shader);

renderable.SetMesh(mesh);
renderable.SetMaterial(material);

// 设置可见性和层级
renderable.SetVisible(true);
renderable.SetLayerID(300);  // WORLD_GEOMETRY

// 渲染
renderable.Render();
```

### 与 ECS 集成

```cpp
// 在 ECS 中，Renderable 由系统自动创建和管理

// 创建实体
EntityID entity = world->CreateEntity();

// 添加 Transform 组件
TransformComponent transform;
transform.SetPosition(Vector3(0, 1, 0));
world->AddComponent(entity, transform);

// 添加 MeshRenderComponent
MeshRenderComponent mesh;
mesh.meshName = "models/cube.obj";  // 异步加载
mesh.materialName = "default";
mesh.visible = true;
world->AddComponent(entity, mesh);

// MeshRenderSystem 会自动：
// 1. 创建 MeshRenderable 对象
// 2. 设置 Transform
// 3. 设置 Mesh 和 Material
// 4. 提交到渲染队列
```

---

## 💡 设计要点

### 1. Transform 复用

使用 `std::shared_ptr` 复用 Transform 对象，避免频繁创建销毁：

```cpp
// ✅ 好：复用 Transform
auto transform = std::make_shared<Transform>();
renderable1.SetTransform(transform);
renderable2.SetTransform(transform);  // 共享同一个 Transform

// ❌ 差：每次创建新对象
Transform temp;
temp.SetPosition(pos);
// 销毁
```

### 2. 线程安全

所有操作都使用 `std::shared_mutex` 保护：

```cpp
// 读操作（共享锁）
bool Renderable::IsVisible() const {
    std::shared_lock lock(m_mutex);
    return m_visible;
}

// 写操作（独占锁）
void Renderable::SetVisible(bool visible) {
    std::unique_lock lock(m_mutex);
    m_visible = visible;
}
```

### 3. 包围盒用于裁剪

`GetBoundingBox()` 用于视锥体裁剪：

```cpp
AABB bounds = renderable->GetBoundingBox();
if (camera.IsVisible(bounds)) {
    renderable->Render();  // 只渲染可见对象
}
```

---

## 🔄 与 ECS 的关系

### 组件 → Renderable

ECS 组件描述数据，Renderable 负责渲染：

```
MeshRenderComponent (组件)
  ├─ meshName: "cube.obj"
  ├─ material: Material
  ├─ visible: true
  └─ ...

      ↓ MeshRenderSystem 创建

MeshRenderable (渲染对象)
  ├─ SetMesh(mesh)
  ├─ SetMaterial(material)
  ├─ SetTransform(transform)
  └─ Render()
```

### 系统自动管理

```cpp
// MeshRenderSystem 的简化逻辑
void MeshRenderSystem::Update(float deltaTime) {
    auto entities = m_world->Query<TransformComponent, MeshRenderComponent>();
    
    for (auto entity : entities) {
        auto& transform = m_world->GetComponent<TransformComponent>(entity);
        auto& meshComp = m_world->GetComponent<MeshRenderComponent>(entity);
        
        if (!meshComp.visible || !meshComp.resourcesLoaded) {
            continue;
        }
        
        // 创建 Renderable（通常使用对象池）
        MeshRenderable renderable;
        renderable.SetTransform(transform.transform);
        renderable.SetMesh(meshComp.mesh);
        renderable.SetMaterial(meshComp.material);
        renderable.SetVisible(meshComp.visible);
        
        // 提交到渲染队列
        renderable.SubmitToRenderer(m_renderer);
    }
}
```

---

## 📊 性能优化

### 1. 对象池

避免每帧创建销毁 Renderable：

```cpp
class MeshRenderSystem : public System {
private:
    std::vector<MeshRenderable> m_renderables;  // 对象池
    
public:
    void Update(float deltaTime) override {
        m_renderables.clear();  // 不释放内存
        
        auto entities = m_world->Query<TransformComponent, MeshRenderComponent>();
        m_renderables.reserve(entities.size());
        
        for (auto entity : entities) {
            m_renderables.emplace_back();  // 复用内存
            auto& renderable = m_renderables.back();
            // 设置 renderable...
        }
    }
};
```

### 2. 视锥体裁剪

使用包围盒进行裁剪：

```cpp
AABB bounds = renderable->GetBoundingBox();
if (!camera.IsVisible(bounds)) {
    continue;  // 跳过不可见对象
}
```

---

## 🔒 线程安全

Renderable 使用 `std::shared_mutex` 保护所有成员变量：

```cpp
mutable std::shared_mutex m_mutex;
```

所有 getter 使用共享锁，setter 使用独占锁，保证线程安全。

---

## 📖 相关文档

- [ECS 概览](ECS.md)
- [MeshRenderable API](MeshRenderable.md)
- [SpriteRenderable API](SpriteRenderable.md)
- [Transform API](Transform.md)
- [Material API](Material.md)

---

[返回 API 目录](README.md) | [返回 ECS 概览](ECS.md)

