# Phase 2: ECS 与 Renderable 渲染对象系统 - 开发文档

[返回文档首页](../README.md)

---

## 📋 目标概述

本阶段目标是构建一个现代化的 **ECS（Entity Component System）** 架构，并实现 **Renderable 渲染对象抽象层**，为引擎提供灵活、高效的对象管理和渲染系统。

### 核心目标

1. ✅ **引入 ECS 架构** - 实体-组件-系统设计模式
2. ✅ **Renderable 抽象层** - 统一的渲染对象接口
3. ✅ **资源复用优化** - 避免 Transform 等对象反复创建销毁
4. ✅ **异步资源加载集成** - 与现有 AsyncResourceLoader 深度集成
5. ✅ **线程安全设计** - 延续项目的线程安全传统 [[memory:7889023]]

---

## 🏗️ 架构设计

### 整体架构图

```
┌─────────────────────────────────────────────────────────────┐
│                        Application Layer                     │
│                    (Game Logic / Scene)                      │
└─────────────────────────────────────────────────────────────┘
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                          ECS Layer                           │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │   Entity     │  │  Component   │  │   System     │      │
│  │   Manager    │◄─┤   Registry   │─►│   Manager    │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                     Renderable Layer                         │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │  Renderable  │  │    Mesh      │  │   Sprite     │      │
│  │     Base     │  │  Renderable  │  │  Renderable  │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
│         ▲                 ▲                  ▲               │
│         └─────────────────┴──────────────────┘               │
└─────────────────────────────────────────────────────────────┘
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      Rendering Backend                       │
│   (Renderer, RenderState, Mesh, Material, Shader...)        │
└─────────────────────────────────────────────────────────────┘
```

### 设计原则

1. **数据导向设计（DOD）** - 组件存储紧凑，缓存友好
2. **组件组合优于继承** - 通过组件组合实现复杂行为
3. **系统解耦** - 系统之间通过组件通信，低耦合
4. **资源复用** - Transform、材质等对象池化管理
5. **渐进式异步加载** - 支持流式资源加载，不阻塞主线程

---

## 📐 核心模块设计

### 1. Entity（实体）

实体是一个轻量级的 ID，用于关联组件。

```cpp
// include/render/ecs/entity.h
namespace Render {
namespace ECS {

// 实体 ID 类型（64位：32位索引 + 32位版本号）
struct EntityID {
    uint32_t index;      // 实体索引
    uint32_t version;    // 版本号（用于检测悬空引用）
    
    bool IsValid() const { return index != INVALID_INDEX; }
    
    // 比较运算符
    bool operator==(const EntityID& other) const;
    bool operator!=(const EntityID& other) const;
    
    // 哈希支持
    struct Hash {
        size_t operator()(const EntityID& id) const;
    };
    
    static constexpr uint32_t INVALID_INDEX = 0xFFFFFFFF;
};

// 实体描述符（用于创建实体）
struct EntityDescriptor {
    std::string name;                    // 实体名称
    bool active = true;                  // 是否激活
    std::vector<std::string> tags;       // 标签列表
};

} // namespace ECS
} // namespace Render
```

**设计要点**：
- 使用版本号机制防止悬空引用（当实体被删除后，版本号递增）
- 轻量级设计，仅存储 ID 和版本
- 支持哈希，可用于 `std::unordered_map`

---

### 2. Component（组件）

组件是纯数据结构，不包含逻辑。

#### 2.1 核心组件定义

```cpp
// include/render/ecs/components.h
namespace Render {
namespace ECS {

// ============================================================
// Transform 组件（避免反复创建销毁）
// ============================================================
struct TransformComponent {
    Ref<Transform> transform;    // 复用 Transform 对象（shared_ptr）
    
    // 快捷访问接口
    void SetPosition(const Vector3& pos);
    void SetRotation(const Quaternion& rot);
    void SetScale(const Vector3& scale);
    
    Vector3 GetPosition() const;
    Quaternion GetRotation() const;
    Vector3 GetScale() const;
    
    Matrix4 GetWorldMatrix() const;
};

// ============================================================
// Mesh 渲染组件
// ============================================================
struct MeshRenderComponent {
    // 资源引用（通过 ResourceManager 管理）
    std::string meshName;          // 网格资源名称
    std::string materialName;      // 材质资源名称
    
    Ref<Mesh> mesh;                // 网格对象（延迟加载）
    Ref<Material> material;        // 材质对象（延迟加载）
    
    // 渲染属性
    bool visible = true;           // 是否可见
    bool castShadows = true;       // 是否投射阴影
    bool receiveShadows = true;    // 是否接收阴影
    uint32_t layerID = 300;        // 渲染层级（默认 WORLD_GEOMETRY）
    int32_t renderPriority = 0;    // 渲染优先级
    
    // LOD 支持
    std::vector<float> lodDistances;  // LOD 距离阈值
    
    // 异步加载状态
    bool resourcesLoaded = false;     // 资源是否已加载
    bool asyncLoading = false;        // 是否正在异步加载
};

// ============================================================
// Sprite 渲染组件（2D）
// ============================================================
struct SpriteRenderComponent {
    std::string textureName;       // 纹理资源名称
    Ref<Texture> texture;          // 纹理对象（延迟加载）
    
    Rect sourceRect{0, 0, 1, 1};   // 源矩形（UV 坐标）
    Vector2 size{1.0f, 1.0f};      // 显示大小
    Color tintColor{1, 1, 1, 1};   // 着色
    
    bool visible = true;
    BlendMode blendMode = BlendMode::Alpha;
    uint32_t layerID = 800;        // UI_LAYER
    
    bool resourcesLoaded = false;
    bool asyncLoading = false;
};

// ============================================================
// Camera 组件
// ============================================================
struct CameraComponent {
    Ref<Camera> camera;            // 相机对象（复用）
    
    bool active = true;            // 是否激活
    uint32_t layerMask = 0xFFFFFFFF;  // 可见层级遮罩
    int32_t depth = 0;             // 渲染深度（深度越低越先渲染）
    Color clearColor{0.1f, 0.1f, 0.1f, 1.0f};
    
    // 渲染目标（可选）
    std::string renderTargetName;
    Ref<Framebuffer> renderTarget;
};

// ============================================================
// Light 组件
// ============================================================
enum class LightType {
    Directional,   // 定向光
    Point,         // 点光源
    Spot,          // 聚光灯
    Area           // 区域光（未来）
};

struct LightComponent {
    LightType type = LightType::Point;
    
    Color color{1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    
    // Point/Spot 光源
    float range = 10.0f;
    float attenuation = 1.0f;
    
    // Spot 光源
    float innerConeAngle = 30.0f;  // 内角（度）
    float outerConeAngle = 45.0f;  // 外角（度）
    
    // 阴影
    bool castShadows = false;
    uint32_t shadowMapSize = 1024;
    float shadowBias = 0.001f;
    
    bool enabled = true;
};

// ============================================================
// 标签组件（用于查询和分组）
// ============================================================
struct TagComponent {
    std::vector<std::string> tags;
    
    bool HasTag(const std::string& tag) const;
    void AddTag(const std::string& tag);
    void RemoveTag(const std::string& tag);
};

// ============================================================
// 名称组件
// ============================================================
struct NameComponent {
    std::string name;
};

// ============================================================
// 激活状态组件
// ============================================================
struct ActiveComponent {
    bool active = true;
};

} // namespace ECS
} // namespace Render
```

**设计要点**：
- **资源复用**：Transform、Camera 等对象使用 `Ref<T>`（`std::shared_ptr<T>`）复用 [[memory:7889023]]
- **延迟加载**：网格、纹理等资源通过 `resourceName` 标识，支持异步加载
- **渲染属性分离**：可见性、阴影、层级等属性与资源分离
- **支持 LOD**：预留 LOD 距离配置

---

### 3. ComponentRegistry（组件注册表）

管理组件的存储和访问。

```cpp
// include/render/ecs/component_registry.h
namespace Render {
namespace ECS {

// 组件数组基类（类型擦除）
class IComponentArray {
public:
    virtual ~IComponentArray() = default;
    virtual void RemoveEntity(EntityID entity) = 0;
    virtual size_t Size() const = 0;
};

// 具体类型的组件数组
template<typename T>
class ComponentArray : public IComponentArray {
public:
    void Add(EntityID entity, const T& component);
    void Remove(EntityID entity);
    T& Get(EntityID entity);
    const T& Get(EntityID entity) const;
    bool Has(EntityID entity) const;
    
    void RemoveEntity(EntityID entity) override;
    size_t Size() const override;
    
    // 迭代器支持
    auto begin() { return m_components.begin(); }
    auto end() { return m_components.end(); }
    
private:
    std::unordered_map<EntityID, T, EntityID::Hash> m_components;
    mutable std::shared_mutex m_mutex;  // 线程安全
};

// 组件注册表
class ComponentRegistry {
public:
    // 注册组件类型
    template<typename T>
    void RegisterComponent();
    
    // 添加/移除/获取组件
    template<typename T>
    void AddComponent(EntityID entity, const T& component);
    
    template<typename T>
    void RemoveComponent(EntityID entity);
    
    template<typename T>
    T& GetComponent(EntityID entity);
    
    template<typename T>
    const T& GetComponent(EntityID entity) const;
    
    template<typename T>
    bool HasComponent(EntityID entity) const;
    
    // 批量操作
    void RemoveAllComponents(EntityID entity);
    
    // 获取组件数组（用于系统遍历）
    template<typename T>
    ComponentArray<T>* GetComponentArray();
    
private:
    std::unordered_map<size_t, std::unique_ptr<IComponentArray>> m_componentArrays;
    mutable std::shared_mutex m_mutex;
};

} // namespace ECS
} // namespace Render
```

**设计要点**：
- **类型安全**：使用模板确保类型安全
- **缓存友好**：同类型组件连续存储
- **线程安全**：使用 `std::shared_mutex` 支持多读单写
- **快速访问**：O(1) 查询复杂度

---

### 4. EntityManager（实体管理器）

管理实体的创建、销毁和查询。

```cpp
// include/render/ecs/entity_manager.h
namespace Render {
namespace ECS {

class EntityManager {
public:
    EntityManager();
    ~EntityManager();
    
    // 实体创建/销毁
    EntityID CreateEntity(const EntityDescriptor& desc = {});
    void DestroyEntity(EntityID entity);
    bool IsValid(EntityID entity) const;
    
    // 实体信息
    void SetName(EntityID entity, const std::string& name);
    std::string GetName(EntityID entity) const;
    
    void SetActive(EntityID entity, bool active);
    bool IsActive(EntityID entity) const;
    
    // 标签系统
    void AddTag(EntityID entity, const std::string& tag);
    void RemoveTag(EntityID entity, const std::string& tag);
    bool HasTag(EntityID entity, const std::string& tag) const;
    
    // 查询
    std::vector<EntityID> GetAllEntities() const;
    std::vector<EntityID> GetEntitiesWithTag(const std::string& tag) const;
    std::vector<EntityID> GetActiveEntities() const;
    
    // 统计
    size_t GetEntityCount() const;
    size_t GetActiveEntityCount() const;
    
    void Clear();
    
private:
    struct EntityData {
        uint32_t version;
        bool active;
        std::string name;
        std::unordered_set<std::string> tags;
    };
    
    std::vector<EntityData> m_entities;           // 实体数据（索引对应）
    std::queue<uint32_t> m_freeIndices;           // 空闲索引队列（复用）
    std::unordered_map<std::string, std::unordered_set<EntityID, EntityID::Hash>> m_tagIndex;  // 标签索引
    
    mutable std::shared_mutex m_mutex;
};

} // namespace ECS
} // namespace Render
```

**设计要点**：
- **版本号机制**：检测悬空引用
- **索引复用**：删除实体后索引可复用
- **标签系统**：快速按标签查询实体
- **线程安全**：保护内部数据结构

---

### 5. System（系统）

系统负责处理具有特定组件的实体。

```cpp
// include/render/ecs/system.h
namespace Render {
namespace ECS {

// 前置声明
class World;

// 系统基类
class System {
public:
    virtual ~System() = default;
    
    // 生命周期
    virtual void OnCreate(World* world) {}
    virtual void OnDestroy() {}
    
    // 更新
    virtual void Update(float deltaTime) = 0;
    
    // 优先级（越小越早执行）
    virtual int GetPriority() const { return 100; }
    
    // 启用/禁用
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }
    
protected:
    World* m_world = nullptr;
    bool m_enabled = true;
};

// ============================================================
// Transform 更新系统（维护变换层级）
// ============================================================
class TransformSystem : public System {
public:
    void Update(float deltaTime) override;
    int GetPriority() const override { return 10; }  // 高优先级
};

// ============================================================
// 资源加载系统（处理异步资源加载）
// ============================================================
class ResourceLoadingSystem : public System {
public:
    ResourceLoadingSystem();
    
    void OnCreate(World* world) override;
    void Update(float deltaTime) override;
    int GetPriority() const override { return 20; }  // 次高优先级
    
    // 设置每帧最大处理任务数
    void SetMaxTasksPerFrame(size_t maxTasks) { m_maxTasksPerFrame = maxTasks; }
    
private:
    void LoadMeshResources();
    void LoadSpriteResources();
    void ProcessAsyncTasks();
    
    size_t m_maxTasksPerFrame = 10;
    AsyncResourceLoader* m_asyncLoader = nullptr;
};

// ============================================================
// Mesh 渲染系统
// ============================================================
class MeshRenderSystem : public System {
public:
    MeshRenderSystem(Renderer* renderer);
    
    void Update(float deltaTime) override;
    int GetPriority() const override { return 100; }
    
    // 渲染统计
    struct RenderStats {
        size_t visibleMeshes = 0;
        size_t culledMeshes = 0;
        size_t drawCalls = 0;
    };
    
    const RenderStats& GetStats() const { return m_stats; }
    
private:
    void SubmitRenderables();
    bool ShouldCull(const MeshRenderComponent& mesh, const TransformComponent& transform);
    
    Renderer* m_renderer;
    RenderStats m_stats;
};

// ============================================================
// Sprite 渲染系统（2D）
// ============================================================
class SpriteRenderSystem : public System {
public:
    SpriteRenderSystem(Renderer* renderer);
    
    void Update(float deltaTime) override;
    int GetPriority() const override { return 200; }
    
private:
    Renderer* m_renderer;
};

// ============================================================
// Camera 系统（管理相机和视锥体裁剪）
// ============================================================
class CameraSystem : public System {
public:
    void Update(float deltaTime) override;
    int GetPriority() const override { return 5; }  // 最高优先级
    
    // 获取主相机
    EntityID GetMainCamera() const;
    Camera* GetMainCameraObject() const;
    
private:
    EntityID m_mainCamera;
};

// ============================================================
// Light 系统（光照管理）
// ============================================================
class LightSystem : public System {
public:
    LightSystem(Renderer* renderer);
    
    void Update(float deltaTime) override;
    int GetPriority() const override { return 50; }
    
    // 光源查询
    std::vector<EntityID> GetVisibleLights(const Camera& camera) const;
    size_t GetLightCount() const;
    
private:
    void UpdateLightUniforms();
    
    Renderer* m_renderer;
};

} // namespace ECS
} // namespace Render
```

**设计要点**：
- **优先级排序**：系统按优先级顺序执行
- **可插拔**：系统可以动态启用/禁用
- **职责单一**：每个系统负责单一功能
- **资源加载集成**：ResourceLoadingSystem 与 AsyncResourceLoader 深度集成

---

### 6. World（世界）

World 是 ECS 的顶层容器，管理所有实体、组件和系统。

```cpp
// include/render/ecs/world.h
namespace Render {
namespace ECS {

class World {
public:
    World();
    ~World();
    
    // 初始化/清理
    void Initialize();
    void Shutdown();
    
    // ==================== 实体管理 ====================
    EntityID CreateEntity(const EntityDescriptor& desc = {});
    void DestroyEntity(EntityID entity);
    bool IsValidEntity(EntityID entity) const;
    
    // ==================== 组件管理 ====================
    template<typename T>
    void RegisterComponent();
    
    template<typename T>
    void AddComponent(EntityID entity, const T& component);
    
    template<typename T>
    void RemoveComponent(EntityID entity);
    
    template<typename T>
    T& GetComponent(EntityID entity);
    
    template<typename T>
    const T& GetComponent(EntityID entity) const;
    
    template<typename T>
    bool HasComponent(EntityID entity) const;
    
    // ==================== 系统管理 ====================
    template<typename T, typename... Args>
    T* RegisterSystem(Args&&... args);
    
    template<typename T>
    T* GetSystem();
    
    template<typename T>
    void RemoveSystem();
    
    // ==================== 查询 ====================
    // 查询具有特定组件的实体
    template<typename... Components>
    std::vector<EntityID> Query() const;
    
    // 查询具有特定标签的实体
    std::vector<EntityID> QueryByTag(const std::string& tag) const;
    
    // ==================== 更新 ====================
    void Update(float deltaTime);
    void Render();
    
    // ==================== 辅助接口 ====================
    EntityManager& GetEntityManager() { return m_entityManager; }
    ComponentRegistry& GetComponentRegistry() { return m_componentRegistry; }
    
    // 统计信息
    struct Statistics {
        size_t entityCount = 0;
        size_t activeEntityCount = 0;
        size_t systemCount = 0;
        float lastUpdateTime = 0.0f;
    };
    
    const Statistics& GetStatistics() const { return m_stats; }
    void PrintStatistics() const;
    
private:
    void SortSystems();
    
    EntityManager m_entityManager;
    ComponentRegistry m_componentRegistry;
    std::vector<std::unique_ptr<System>> m_systems;
    
    Statistics m_stats;
    bool m_initialized = false;
    
    mutable std::shared_mutex m_mutex;
};

} // namespace ECS
} // namespace Render
```

**设计要点**：
- **统一接口**：提供实体、组件、系统的统一管理接口
- **系统自动排序**：根据优先级自动排序系统
- **线程安全**：保护内部数据结构
- **统计信息**：提供性能监控接口

---

### 7. Renderable（渲染对象抽象）

Renderable 是所有可渲染对象的基类，提供统一的渲染接口。

```cpp
// include/render/renderable.h
namespace Render {

// 渲染对象类型
enum class RenderableType {
    Mesh,       // 3D 网格
    Sprite,     // 2D 精灵
    Text,       // 文本（未来）
    Particle,   // 粒子（未来）
    Custom      // 自定义
};

// ============================================================
// Renderable 基类
// ============================================================
class Renderable {
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    
public:
    Renderable(RenderableType type);
    virtual ~Renderable() = default;
    
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
    
    void SetRenderPriority(uint32_t priority);
    uint32_t GetRenderPriority() const;
    
    // 类型
    RenderableType GetType() const { return m_type; }
    
    // 包围盒（用于视锥体裁剪）
    virtual AABB GetBoundingBox() const = 0;
    
protected:
    RenderableType m_type;
    Ref<Transform> m_transform;    // 复用 Transform 对象
    bool m_visible = true;
    uint32_t m_layerID = 300;      // WORLD_GEOMETRY
    int32_t m_renderPriority = 0;
    
    mutable std::shared_mutex m_mutex;
};

// ============================================================
// MeshRenderable（3D 网格渲染对象）
// ============================================================
class MeshRenderable : public Renderable {
public:
    MeshRenderable();
    ~MeshRenderable() override = default;
    
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

// ============================================================
// SpriteRenderable（2D 精灵渲染对象）
// ============================================================
class SpriteRenderable : public Renderable {
public:
    SpriteRenderable();
    ~SpriteRenderable() override = default;
    
    // 渲染
    void Render() override;
    void SubmitToRenderer(Renderer* renderer) override;
    
    // 纹理
    void SetTexture(const Ref<Texture>& texture);
    Ref<Texture> GetTexture() const;
    
    // 显示属性
    void SetSourceRect(const Rect& rect);
    Rect GetSourceRect() const;
    
    void SetSize(const Vector2& size);
    Vector2 GetSize() const;
    
    void SetTintColor(const Color& color);
    Color GetTintColor() const;
    
    void SetBlendMode(BlendMode mode);
    BlendMode GetBlendMode() const;
    
    // 包围盒
    AABB GetBoundingBox() const override;
    
private:
    Ref<Texture> m_texture;
    Rect m_sourceRect{0, 0, 1, 1};
    Vector2 m_size{1.0f, 1.0f};
    Color m_tintColor{1, 1, 1, 1};
    BlendMode m_blendMode = BlendMode::Alpha;
};

} // namespace Render
```

**设计要点**：
- **统一接口**：所有渲染对象实现相同的接口
- **Transform 复用**：使用 `Ref<Transform>` 避免反复创建销毁 [[memory:7889023]]
- **包围盒支持**：用于视锥体裁剪优化
- **线程安全**：保护内部状态

---

## 📝 开发任务清单

### 阶段 1：ECS 核心框架（优先级：最高）

- [ ] **实体系统**
  - [ ] 实现 `EntityID` 结构体（索引 + 版本号）
  - [ ] 实现 `EntityDescriptor`
  - [ ] 实现 `EntityManager` 类
    - [ ] 实体创建/销毁（索引复用）
    - [ ] 版本号管理
    - [ ] 激活状态管理
    - [ ] 标签系统
    - [ ] 查询接口
  - [ ] 单元测试（`entity_test.cpp`）

- [ ] **组件系统**
  - [ ] 实现 `IComponentArray` 基类
  - [ ] 实现 `ComponentArray<T>` 模板类
  - [ ] 实现 `ComponentRegistry` 类
  - [ ] 线程安全保护（`std::shared_mutex`）
  - [ ] 单元测试（`component_test.cpp`）

- [ ] **核心组件定义**
  - [ ] `TransformComponent`（复用 Transform 对象）
  - [ ] `NameComponent`
  - [ ] `TagComponent`
  - [ ] `ActiveComponent`

- [ ] **World 管理器**
  - [ ] 实现 `World` 类
  - [ ] 实体管理接口
  - [ ] 组件管理接口
  - [ ] 系统管理接口
  - [ ] 查询接口（`Query<Components...>()`）
  - [ ] 统计信息接口

---

### 阶段 2：渲染组件与 Renderable（优先级：高）

- [ ] **Renderable 基类**
  - [ ] 实现 `Renderable` 基类
  - [ ] Transform 集成（使用 `Ref<Transform>`）
  - [ ] 可见性管理
  - [ ] 层级和优先级
  - [ ] 包围盒接口

- [ ] **MeshRenderable**
  - [ ] 实现 `MeshRenderable` 类
  - [ ] Mesh 和 Material 设置
  - [ ] 阴影属性
  - [ ] 渲染实现
  - [ ] 包围盒计算

- [ ] **SpriteRenderable**
  - [ ] 实现 `SpriteRenderable` 类
  - [ ] 纹理设置
  - [ ] UV 矩形和大小
  - [ ] 着色和混合模式
  - [ ] 渲染实现

- [ ] **渲染组件定义**
  - [ ] `MeshRenderComponent`
  - [ ] `SpriteRenderComponent`
  - [ ] `CameraComponent`（复用 Camera 对象）
  - [ ] `LightComponent`

---

### 阶段 3：系统实现（优先级：高）

- [ ] **System 基类**
  - [ ] 实现 `System` 基类
  - [ ] 生命周期接口（`OnCreate`, `OnDestroy`）
  - [ ] 更新接口（`Update`）
  - [ ] 优先级系统
  - [ ] 启用/禁用功能

- [ ] **TransformSystem**
  - [ ] 实现变换层级更新
  - [ ] 父子关系处理
  - [ ] 性能优化（避免重复计算）

- [ ] **ResourceLoadingSystem**
  - [ ] 集成 `AsyncResourceLoader`
  - [ ] 自动加载 MeshRenderComponent 资源
  - [ ] 自动加载 SpriteRenderComponent 资源
  - [ ] 进度追踪
  - [ ] 每帧任务限制（控制帧率）

- [ ] **MeshRenderSystem**
  - [ ] 遍历 MeshRenderComponent
  - [ ] 创建 MeshRenderable 对象
  - [ ] 视锥体裁剪
  - [ ] 提交到渲染器
  - [ ] 渲染统计

- [ ] **SpriteRenderSystem**
  - [ ] 遍历 SpriteRenderComponent
  - [ ] 创建 SpriteRenderable 对象
  - [ ] 提交到渲染器

- [ ] **CameraSystem**
  - [ ] 更新相机矩阵
  - [ ] 主相机管理
  - [ ] 多相机支持

- [ ] **LightSystem**
  - [ ] 收集光源数据
  - [ ] 上传光源 uniform
  - [ ] 可见光源查询

---

### 阶段 4：渲染器集成（优先级：中）

- [ ] **Renderer 扩展**
  - [ ] 添加 `SubmitRenderable(Renderable*)` 接口
  - [ ] 添加渲染队列管理
  - [ ] 按层级排序渲染
  - [ ] 视锥体裁剪集成

- [ ] **RenderQueue 实现**
  - [ ] 按层级排序
  - [ ] 按材质排序（减少状态切换）
  - [ ] 按深度排序（透明物体）
  - [ ] Flush 接口

---

### 阶段 5：工具和辅助功能（优先级：中）

- [ ] **场景序列化**
  - [ ] 场景保存（JSON 格式）
  - [ ] 场景加载
  - [ ] 组件序列化支持

- [ ] **调试工具**
  - [ ] 实体查看器
  - [ ] 组件检查器
  - [ ] 系统性能分析
  - [ ] 渲染统计面板

- [ ] **编辑器集成（可选）**
  - [ ] ImGui 集成
  - [ ] 实体树视图
  - [ ] 组件编辑面板
  - [ ] 场景层级视图

---

### 阶段 6：测试和示例（优先级：高）

- [ ] **单元测试**
  - [ ] `31_ecs_basic_test.cpp` - ECS 基础功能测试
  - [ ] `32_ecs_component_test.cpp` - 组件管理测试
  - [ ] `33_ecs_system_test.cpp` - 系统执行测试
  - [ ] `34_ecs_query_test.cpp` - 查询功能测试
  - [ ] `35_ecs_performance_test.cpp` - 性能测试

- [ ] **集成测试**
  - [ ] `36_ecs_renderable_test.cpp` - Renderable 集成测试
  - [ ] `37_ecs_async_loading_test.cpp` - 异步加载集成测试
  - [ ] `38_ecs_scene_test.cpp` - 完整场景测试

- [ ] **完整示例**
  - [ ] `40_ecs_demo_scene.cpp` - 演示场景（多个物体、光照、相机）
  - [ ] `41_ecs_game_scene.cpp` - 游戏场景示例

---

### 阶段 7：优化和文档（优先级：中）

- [ ] **性能优化**
  - [ ] 组件存储优化（SoA 结构）
  - [ ] 系统更新优化（并行处理）
  - [ ] 查询缓存优化
  - [ ] 内存对齐优化

- [ ] **API 文档**
  - [ ] `docs/api/ECS.md` - ECS 系统 API
  - [ ] `docs/api/Entity.md` - 实体 API
  - [ ] `docs/api/Component.md` - 组件 API
  - [ ] `docs/api/System.md` - 系统 API
  - [ ] `docs/api/World.md` - World API
  - [ ] `docs/api/Renderable.md` - Renderable API
  - [ ] `docs/api/MeshRenderable.md` - MeshRenderable API
  - [ ] `docs/api/SpriteRenderable.md` - SpriteRenderable API

- [ ] **使用指南**
  - [ ] `docs/ECS_USER_GUIDE.md` - ECS 使用指南
  - [ ] `docs/RENDERABLE_GUIDE.md` - Renderable 使用指南
  - [ ] `docs/ECS_BEST_PRACTICES.md` - 最佳实践

---

## 🎯 使用示例

### 示例 1：创建简单场景

```cpp
#include <render/ecs/world.h>
#include <render/ecs/components.h>
#include <render/ecs/systems.h>
#include <render/renderer.h>

using namespace Render;
using namespace Render::ECS;

int main() {
    // 初始化渲染器
    Renderer renderer;
    renderer.Initialize("ECS Demo", 1920, 1080);
    
    // 创建 World
    World world;
    world.Initialize();
    
    // 注册组件
    world.RegisterComponent<TransformComponent>();
    world.RegisterComponent<MeshRenderComponent>();
    world.RegisterComponent<CameraComponent>();
    world.RegisterComponent<LightComponent>();
    
    // 注册系统（按优先级自动排序）
    world.RegisterSystem<CameraSystem>();
    world.RegisterSystem<TransformSystem>();
    world.RegisterSystem<ResourceLoadingSystem>();
    world.RegisterSystem<LightSystem>(&renderer);
    world.RegisterSystem<MeshRenderSystem>(&renderer);
    
    // ============================================================
    // 创建相机实体
    // ============================================================
    EntityID cameraEntity = world.CreateEntity({
        .name = "MainCamera",
        .active = true,
        .tags = {"camera", "main"}
    });
    
    // 添加 Transform 组件（复用 Transform 对象）
    TransformComponent cameraTransform;
    cameraTransform.transform = std::make_shared<Transform>();
    cameraTransform.SetPosition(Vector3(0, 2, 5));
    world.AddComponent(cameraEntity, cameraTransform);
    
    // 添加 Camera 组件
    CameraComponent cameraComp;
    cameraComp.camera = std::make_shared<Camera>();
    cameraComp.camera->SetPerspective(45.0f, 16.0f/9.0f, 0.1f, 1000.0f);
    cameraComp.active = true;
    world.AddComponent(cameraEntity, cameraComp);
    
    // ============================================================
    // 创建光源实体
    // ============================================================
    EntityID lightEntity = world.CreateEntity({
        .name = "DirectionalLight"
    });
    
    TransformComponent lightTransform;
    lightTransform.transform = std::make_shared<Transform>();
    lightTransform.SetRotation(
        MathUtils::FromEulerDegrees(Vector3(30, 45, 0))
    );
    world.AddComponent(lightEntity, lightTransform);
    
    LightComponent light;
    light.type = LightType::Directional;
    light.color = Color(1.0f, 1.0f, 0.9f);
    light.intensity = 1.0f;
    world.AddComponent(lightEntity, light);
    
    // ============================================================
    // 创建地面实体（异步加载）
    // ============================================================
    EntityID groundEntity = world.CreateEntity({
        .name = "Ground"
    });
    
    TransformComponent groundTransform;
    groundTransform.transform = std::make_shared<Transform>();
    groundTransform.SetPosition(Vector3(0, -1, 0));
    groundTransform.SetScale(Vector3(10, 1, 10));
    world.AddComponent(groundEntity, groundTransform);
    
    MeshRenderComponent groundMesh;
    groundMesh.meshName = "plane";        // 资源名称（由 ResourceLoadingSystem 加载）
    groundMesh.materialName = "default";
    groundMesh.visible = true;
    groundMesh.castShadows = false;
    groundMesh.receiveShadows = true;
    groundMesh.layerID = 300;  // WORLD_GEOMETRY
    world.AddComponent(groundEntity, groundMesh);
    
    // ============================================================
    // 创建立方体实体（异步加载）
    // ============================================================
    EntityID cubeEntity = world.CreateEntity({
        .name = "Cube"
    });
    
    TransformComponent cubeTransform;
    cubeTransform.transform = std::make_shared<Transform>();
    cubeTransform.SetPosition(Vector3(0, 1, 0));
    world.AddComponent(cubeEntity, cubeTransform);
    
    MeshRenderComponent cubeMesh;
    cubeMesh.meshName = "cube";
    cubeMesh.materialName = "default";
    world.AddComponent(cubeEntity, cubeMesh);
    
    // ============================================================
    // 主循环
    // ============================================================
    bool running = true;
    float time = 0.0f;
    
    while (running) {
        // 事件处理
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
        }
        
        float deltaTime = 0.016f;  // 60 FPS
        time += deltaTime;
        
        // 旋转立方体
        auto& transform = world.GetComponent<TransformComponent>(cubeEntity);
        transform.SetRotation(
            MathUtils::FromEulerDegrees(Vector3(0, time * 30, 0))
        );
        
        // 更新 World（自动调用所有系统）
        world.Update(deltaTime);
        
        // 渲染
        renderer.BeginFrame();
        renderer.Clear();
        world.Render();  // 提交所有 Renderable 到渲染器
        renderer.EndFrame();
        renderer.Present();
    }
    
    // 清理
    world.Shutdown();
    renderer.Shutdown();
    
    return 0;
}
```

---

### 示例 2：异步资源加载

```cpp
// ResourceLoadingSystem 自动处理异步加载
void MyScene::LoadLevel() {
    World& world = GetWorld();
    
    // 批量创建实体（资源将异步加载）
    for (int i = 0; i < 100; i++) {
        EntityID entity = world.CreateEntity({
            .name = "Building_" + std::to_string(i)
        });
        
        TransformComponent transform;
        transform.transform = std::make_shared<Transform>();
        transform.SetPosition(Vector3(i * 5.0f, 0, 0));
        world.AddComponent(entity, transform);
        
        MeshRenderComponent mesh;
        mesh.meshName = "building_lod0";  // 🔄 异步加载
        mesh.materialName = "building_material";  // 🔄 异步加载
        world.AddComponent(entity, mesh);
    }
    
    // ResourceLoadingSystem 会在后台线程加载资源
    // 每帧在主线程上传少量 GPU 数据，不阻塞渲染
}
```

---

### 示例 3：查询和遍历

```cpp
void MyGameSystem::Update(float deltaTime) {
    World* world = GetWorld();
    
    // 查询所有具有 Transform 和 MeshRenderComponent 的实体
    auto entities = world->Query<TransformComponent, MeshRenderComponent>();
    
    for (EntityID entity : entities) {
        auto& transform = world->GetComponent<TransformComponent>(entity);
        auto& mesh = world->GetComponent<MeshRenderComponent>(entity);
        
        // 更新逻辑...
        if (mesh.visible) {
            // 做一些事情...
        }
    }
    
    // 按标签查询
    auto enemies = world->QueryByTag("enemy");
    for (EntityID enemy : enemies) {
        // 处理敌人...
    }
}
```

---

## 🔧 技术细节

### 1. Transform 复用策略

**问题**：频繁创建销毁 Transform 对象导致性能问题。

**解决方案**：
- 使用 `std::shared_ptr<Transform>` 在组件中存储
- Transform 对象在实体销毁后仍可复用（如果有其他引用）
- 避免每帧创建/销毁

```cpp
// ✅ 好：复用 Transform 对象
TransformComponent comp;
comp.transform = std::make_shared<Transform>();  // 创建一次
comp.SetPosition(pos);  // 修改已有对象

// ❌ 差：每次创建新对象
for (int i = 0; i < 1000; i++) {
    Transform temp;  // 栈上创建
    temp.SetPosition(pos);
    // 销毁
}
```

---

### 2. 异步资源加载流程

```
用户创建实体 + MeshRenderComponent
         ↓
  meshName = "model.fbx"
  asyncLoading = false
         ↓
ResourceLoadingSystem::Update()
         ↓
  检测到 meshName 非空且 mesh == nullptr
         ↓
  调用 AsyncResourceLoader::LoadMeshAsync()
  设置 asyncLoading = true
         ↓
  [后台线程] 加载文件、解析数据
         ↓
  [主线程] ProcessCompletedTasks() 上传 GPU
         ↓
  回调：mesh = result, resourcesLoaded = true
         ↓
MeshRenderSystem::Update()
         ↓
  检测到 resourcesLoaded == true
         ↓
  创建 MeshRenderable 并提交渲染
```

---

### 3. 系统执行顺序

系统按优先级排序执行：

```
优先级     系统                      职责
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
5         CameraSystem             更新相机矩阵、主相机管理
10        TransformSystem          更新变换层级
20        ResourceLoadingSystem    异步资源加载
50        LightSystem              光照数据更新
100       MeshRenderSystem         提交 3D 网格渲染
200       SpriteRenderSystem       提交 2D 精灵渲染
```

---

### 4. 线程安全保证

- **EntityManager**: `std::shared_mutex` 保护实体数据
- **ComponentRegistry**: 每个 `ComponentArray` 独立锁
- **World**: 锁保护系统列表和查询操作
- **AsyncResourceLoader**: 工作线程与主线程分离

---

## 📊 性能目标

| 指标 | 目标 | 备注 |
|------|------|------|
| 实体数量 | 10,000+ | 不包含组件 |
| 带组件实体 | 5,000+ | 每个实体 3-5 个组件 |
| 查询速度 | < 1ms | 查询 10,000 实体 |
| 系统更新 | < 5ms | 5 个活跃系统 |
| 内存占用 | < 100MB | 10,000 实体 + 组件 |

---

## 🎨 设计哲学

1. **数据优先**：组件是纯数据，系统是纯逻辑
2. **性能优先**：缓存友好、内存对齐、SIMD 优化
3. **易用性**：简洁的 API，清晰的概念
4. **可扩展性**：易于添加新组件和系统
5. **线程安全**：多线程友好，无数据竞争

---

## 📚 参考资料

### ECS 设计模式
- [Data-Oriented Design](https://en.wikipedia.org/wiki/Data-oriented_design)
- [Unity ECS](https://docs.unity3d.com/Packages/com.unity.entities@latest)
- [EnTT Library](https://github.com/skypjack/entt)

### 项目现有文档
- [Transform API](../api/Transform.md) - Transform 使用参考 [[memory:7889023]]
- [AsyncResourceLoader API](../api/AsyncResourceLoader.md) - 异步资源加载