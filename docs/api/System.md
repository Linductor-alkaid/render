# System API 参考

[返回 API 目录](README.md) | [返回 ECS 概览](ECS.md)

---

## 📋 概述

System（系统）是 ECS 架构中负责处理具有特定组件的实体的逻辑单元。系统按优先级顺序执行，每帧调用一次 `Update()`。

**命名空间**：`Render::ECS`

**头文件**：
- `<render/ecs/system.h>` - System 基类
- `<render/ecs/systems.h>` - 内置系统

**最后更新**：2025-11-05

---

## 🎯 System 基类

所有系统都继承自 `System` 基类。

### 类定义

```cpp
class System {
public:
    virtual ~System() = default;
    
    // 生命周期
    virtual void OnCreate(World* world);
    virtual void OnDestroy();
    
    // 更新
    virtual void Update(float deltaTime) = 0;
    
    // 优先级
    virtual int GetPriority() const;
    
    // 启用/禁用
    void SetEnabled(bool enabled);
    bool IsEnabled() const;
    
protected:
    World* m_world = nullptr;
    bool m_enabled = true;
};
```

---

## 🔧 成员函数

### 生命周期

#### `OnCreate()`

系统创建时调用，用于初始化。

```cpp
virtual void OnCreate(World* world);
```

**参数**：
- `world` - World 指针

**说明**：
- 默认实现会设置 `m_world` 指针
- 可以重写以执行自定义初始化

**示例**：
```cpp
class MySystem : public System {
public:
    void OnCreate(World* world) override {
        System::OnCreate(world);  // 调用基类实现
        
        // 自定义初始化
        Logger::GetInstance().Info("MySystem created");
    }
};
```

#### `OnDestroy()`

系统销毁时调用，用于清理。

```cpp
virtual void OnDestroy();
```

**示例**：
```cpp
void OnDestroy() override {
    Logger::GetInstance().Info("MySystem destroyed");
}
```

---

### 更新

#### `Update()`

每帧调用，执行系统逻辑。

```cpp
virtual void Update(float deltaTime) = 0;
```

**参数**：
- `deltaTime` - 帧间隔时间（秒）

**示例**：
```cpp
void Update(float deltaTime) override {
    auto entities = m_world->Query<TransformComponent, VelocityComponent>();
    
    for (auto entity : entities) {
        auto& transform = m_world->GetComponent<TransformComponent>(entity);
        auto& velocity = m_world->GetComponent<VelocityComponent>(entity);
        
        Vector3 pos = transform.GetPosition();
        pos += velocity.velocity * deltaTime;
        transform.SetPosition(pos);
    }
}
```

---

### 优先级

#### `GetPriority()`

获取系统优先级，优先级越小越早执行。

```cpp
virtual int GetPriority() const;
```

**返回值**：优先级值（默认 100）

**推荐优先级**：
- 3: `WindowSystem`（窗口管理）
- 5: `CameraSystem`（最高优先级）
- 10: `TransformSystem`
- 15: `GeometrySystem`（几何生成）
- 20: `ResourceLoadingSystem`
- 50: `LightSystem`
- 90: `UniformSystem`（Uniform 管理）
- 100: `MeshRenderSystem`（默认）
- 200: `SpriteRenderSystem`
- 1000: `ResourceCleanupSystem`（资源清理）

**示例**：
```cpp
int GetPriority() const override { 
    return 15;  // 在 TransformSystem 之后，ResourceLoadingSystem 之前
}
```

---

### 启用/禁用

#### `SetEnabled()` / `IsEnabled()`

设置/获取系统启用状态。

```cpp
void SetEnabled(bool enabled);
bool IsEnabled() const;
```

**说明**：
- 禁用的系统不会执行 `Update()`
- 可以在运行时动态启用/禁用系统

**示例**：
```cpp
auto* system = world->GetSystem<MySystem>();
system->SetEnabled(false);  // 禁用系统
```

---

## 🏭 内置系统

### WindowSystem

监控窗口大小变化，自动更新相机宽高比和视口。**使用事件驱动的回调机制**（v1.1新特性）。

**优先级**：3（在相机系统之前）

```cpp
class WindowSystem : public System {
public:
    explicit WindowSystem(Renderer* renderer);
    
    void Update(float deltaTime) override;
    int GetPriority() const override { return 3; }
    
    void OnCreate(World* world) override;
    void OnDestroy() override;
    
private:
    void OnWindowResized(int width, int height);  // 窗口大小变化回调
};
```

**实现机制（v1.1）**：
- ✅ **事件驱动**：使用 `OpenGLContext` 的窗口大小变化回调机制
- ✅ **高效**：不再使用轮询检测，只在窗口大小真正变化时触发
- ✅ **即时响应**：窗口大小变化立即触发回调，无延迟

**前置条件**：
1. ⚠️ Renderer 必须已经初始化（调用 `Renderer::Initialize()`）
2. ⚠️ OpenGLContext 必须已经初始化
3. ⚠️ 必须在主线程（OpenGL线程）中注册和更新

**功能**：
- 监听窗口大小变化事件（通过 OpenGLContext 回调）
- 自动更新所有相机的宽高比
- 更新渲染器视口设置

**示例**：
```cpp
// 基本使用
world->RegisterSystem<WindowSystem>(renderer.get());

// ✅ 回调会自动注册，窗口大小变化时立即触发
// 无需手动轮询或调用任何方法

// 窗口大小变化时会自动：
// 1. 更新所有相机的宽高比
// 2. 更新渲染视口
// 3. 记录日志
```

**工作流程**：

```
用户调整窗口大小
        ↓
OpenGLContext 检测到变化
        ↓
触发 WindowSystem::OnWindowResized() 回调
        ↓
┌──────────────────────┬──────────────────────┐
│   更新相机宽高比      │   更新渲染视口        │
│  (CameraSystem)      │  (RenderState)       │
└──────────────────────┴──────────────────────┘
```

**注意事项**：
- ⚠️ 回调在 OpenGL 线程中执行，确保不阻塞主线程
- ⚠️ 如果 Renderer 未初始化，系统会记录错误日志并跳过
- ✅ 系统销毁时，回调会随 OpenGLContext 自动清理

**错误处理**：
```cpp
// 如果 Renderer 未初始化
[WindowSystem] Renderer is not initialized. 
Make sure to call Renderer::Initialize() before registering WindowSystem.

// 如果 OpenGLContext 为空
[WindowSystem] OpenGLContext is null.

// 正常工作
[WindowSystem] WindowSystem created (initial size: 1280x720, using resize callbacks)
[WindowSystem] Window resized to 1920x1080
[WindowSystem] Updated 1 camera(s) aspect ratio to 1.778
```

---

### CameraSystem

管理相机组件，更新视图矩阵和投影矩阵。支持主相机自动选择和验证。

**优先级**：5（最高）

**主相机管理策略**：
- 自动验证主相机有效性，如果无效会自动选择新的主相机
- 按照 `depth` 值选择主相机（depth越小优先级越高）
- 支持手动设置和清除主相机

```cpp
class CameraSystem : public System {
public:
    void Update(float deltaTime) override;
    int GetPriority() const override { return 5; }
    
    // ==================== 主相机查询 ====================
    EntityID GetMainCamera() const;
    Camera* GetMainCameraObject() const;          // 已废弃，使用下面的方法
    Ref<Camera> GetMainCameraSharedPtr() const;   // 推荐：返回智能指针
    
    // ==================== 主相机管理 ====================
    bool SetMainCamera(EntityID entity);          // 手动设置主相机
    void ClearMainCamera();                       // 清除主相机
    EntityID SelectMainCameraByDepth();           // 按depth选择主相机
    
private:
    bool ValidateMainCamera() const;              // 验证主相机有效性
};
```

**新增功能（v1.1）**：
- ✅ 自动验证主相机，失效时自动切换
- ✅ 按 depth 排序选择最佳相机
- ✅ 返回智能指针的安全接口
- ✅ 手动管理主相机的接口

**方法说明**：

| 方法 | 说明 | 推荐度 |
|------|------|--------|
| `GetMainCamera()` | 获取主相机实体ID | ⭐⭐⭐⭐⭐ |
| `GetMainCameraSharedPtr()` | 获取主相机对象（智能指针） | ⭐⭐⭐⭐⭐ 推荐 |
| `GetMainCameraObject()` | 获取主相机对象（裸指针） | ⭐⭐⭐☆☆ 已废弃 |
| `SetMainCamera()` | 手动设置主相机 | ⭐⭐⭐⭐☆ |
| `ClearMainCamera()` | 清除主相机（下次Update自动选择） | ⭐⭐⭐⭐☆ |
| `SelectMainCameraByDepth()` | 立即按depth选择主相机 | ⭐⭐⭐⭐☆ |

**示例**：
```cpp
// 基本使用
auto* cameraSystem = world->GetSystem<CameraSystem>();

// 获取主相机（推荐使用智能指针）
auto camera = cameraSystem->GetMainCameraSharedPtr();
if (camera) {
    Matrix4 viewMatrix = camera->GetViewMatrix();
}

// 获取主相机实体
EntityID mainCameraEntity = cameraSystem->GetMainCamera();
if (mainCameraEntity.IsValid()) {
    auto& cameraComp = world->GetComponent<CameraComponent>(mainCameraEntity);
    Logger::Info("Main camera depth: %d", cameraComp.depth);
}

// 手动设置主相机
EntityID myCameraEntity = world->CreateEntity({.name = "MyCamera"});
// ... 添加 CameraComponent ...
if (cameraSystem->SetMainCamera(myCameraEntity)) {
    Logger::Info("成功设置主相机");
}

// 清除主相机（下次Update会自动选择）
cameraSystem->ClearMainCamera();

// 立即按depth选择主相机
EntityID selectedCamera = cameraSystem->SelectMainCameraByDepth();
if (selectedCamera.IsValid()) {
    Logger::Info("选中相机: %u", selectedCamera.index);
}
```

**主相机选择规则**：
1. 首次选择：选择所有激活相机中 `depth` 最小的
2. 验证周期：每帧自动验证当前主相机
3. 自动切换：如果主相机被禁用/删除，自动选择下一个有效相机
4. 手动优先：手动设置的主相机会覆盖自动选择

**线程安全性**：
- ⚠️ 系统本身不是线程安全的，应在主线程调用
- ✅ Camera 对象本身是线程安全的

---

### TransformSystem

更新所有 TransformComponent 的层级关系，提供批量更新优化和父子关系同步。

**优先级**：10（高优先级，在其他系统之前运行）

```cpp
class TransformSystem : public System {
public:
    void Update(float deltaTime) override;
    int GetPriority() const override { return 10; }
    
    // 父子关系管理
    void SyncParentChildRelations();
    void BatchUpdateTransforms();
    size_t ValidateAll();
    
    // 配置
    void SetBatchUpdateEnabled(bool enable);
    
    // 统计信息
    struct UpdateStats {
        size_t totalEntities = 0;      ///< 总实体数
        size_t dirtyTransforms = 0;    ///< 需要更新的 Transform 数
        size_t syncedParents = 0;      ///< 同步的父子关系数
        size_t clearedParents = 0;     ///< 清除的无效父子关系数
    };
    const UpdateStats& GetStats() const;
};
```

**功能**：

1. **父子关系同步**（`SyncParentChildRelations`）
   - 将 TransformComponent 的 `parentEntity` 同步到 Transform 的父指针
   - 验证父实体有效性，自动清除无效的父子关系
   - 检测循环引用并拒绝

2. **批量更新优化**（`BatchUpdateTransforms`）
   - 收集所有标记为 dirty 的 Transform
   - 按层级深度排序（父对象先更新）
   - 批量更新减少开销（性能提升 3-5 倍）

3. **系统验证**（`ValidateAll`）
   - 验证所有 Transform 状态
   - 检查父实体一致性
   - 返回无效 Transform 数量

**Update 流程**：

```cpp
void TransformSystem::Update(float deltaTime) {
    // 1. 同步父子关系（实体ID -> Transform指针）
    SyncParentChildRelations();
    
    // 2. 批量更新 Transform
    if (m_batchUpdateEnabled) {
        BatchUpdateTransforms();
    }
    
    // 3. 定期验证（调试模式）
    #ifdef DEBUG
    // 每5秒验证一次
    #endif
}
```

**性能特性**：
- 批量更新比单独更新快 **3-5 倍**
- 只更新标记为 dirty 的 Transform
- 层级排序确保父对象先更新（避免重复计算）

**使用示例**：

```cpp
// 注册系统
world->RegisterSystem<TransformSystem>();

// 自动工作（无需手动调用）
world->Update(0.016f);

// 获取统计信息
auto* transformSystem = world->GetSystem<TransformSystem>();
const auto& stats = transformSystem->GetStats();
std::cout << "Updated " << stats.dirtyTransforms << " transforms" << std::endl;

// 禁用批量更新（如果需要）
transformSystem->SetBatchUpdateEnabled(false);

// 系统验证（调试）
size_t invalidCount = transformSystem->ValidateAll();
if (invalidCount > 0) {
    Logger::Warning("Found invalid transforms");
}
```

**安全特性**：
- ✅ 自动检测父实体销毁并清除关系
- ✅ 循环引用检测和拒绝
- ✅ 层级深度限制（1000层）
- ✅ 完整的验证和调试接口

---

### GeometrySystem

自动生成基本几何形状的网格。

**优先级**：15（在资源加载之前）

```cpp
class GeometrySystem : public System {
public:
    GeometrySystem() = default;
    
    void Update(float deltaTime) override;
    int GetPriority() const override { return 15; }
};
```

**功能**：
- 检测具有 `GeometryComponent` 但尚未生成网格的实体
- 调用 `MeshLoader` 生成对应形状的网格（Cube、Sphere、Cylinder 等）
- 将生成的网格赋值给 `MeshRenderComponent::mesh`
- 标记 `generated = true` 避免重复生成

**示例**：
```cpp
// 注册系统
world->RegisterSystem<GeometrySystem>();

// 创建几何形状实体
EntityID sphere = world->CreateEntity();
GeometryComponent geom;
geom.type = GeometryType::Sphere;
geom.size = 2.0f;
geom.segments = 32;
world->AddComponent(sphere, geom);

MeshRenderComponent mesh;
mesh.materialName = "default";
world->AddComponent(sphere, mesh);

// GeometrySystem 会自动生成网格
```

---

### ResourceLoadingSystem

处理 MeshRenderComponent 和 SpriteRenderComponent 的异步资源加载。支持多纹理加载和资源管理器集成。

**优先级**：20

```cpp
class ResourceLoadingSystem : public System {
public:
    ResourceLoadingSystem();
    explicit ResourceLoadingSystem(AsyncResourceLoader* asyncLoader);
    
    void OnCreate(World* world) override;
    void OnDestroy() override;
    void Update(float deltaTime) override;
    int GetPriority() const override { return 20; }
    
    // 配置方法
    void SetMaxTasksPerFrame(size_t maxTasks);
    size_t GetMaxTasksPerFrame() const;
    void SetAsyncLoader(AsyncResourceLoader* asyncLoader);
};
```

**功能**：
- 检测未加载的资源
- 通过 `ResourceManager` 统一管理所有资源（网格、纹理、材质）
- 通过 `ShaderCache` 管理着色器加载
- 支持多纹理加载（通过 `textureOverrides`）
- 提交异步加载任务
- 处理完成的加载任务
- 每帧限制处理任务数（避免卡顿）

**新增功能（v1.1）**：
- ✅ AsyncResourceLoader 初始化状态检查
- ✅ Sprite 纹理优先从 ResourceManager 缓存加载
- ✅ 在 OnDestroy 时清理所有待处理任务
- ✅ 使用可配置的 maxTasksPerFrame 值

**方法说明**：

#### `SetMaxTasksPerFrame()`

设置每帧最大处理任务数。

```cpp
void SetMaxTasksPerFrame(size_t maxTasks);
```

**参数**：
- `maxTasks` - 每帧最大处理任务数（默认 10）

**说明**：
- 控制每帧 GPU 上传任务的数量
- 避免单帧加载过多资源导致卡顿
- 较小的值更平滑，较大的值加载更快

#### `GetMaxTasksPerFrame()`

获取每帧最大处理任务数。

```cpp
size_t GetMaxTasksPerFrame() const;
```

**返回值**：当前设置的每帧最大处理任务数

**示例**：
```cpp
// 基本使用
auto& asyncLoader = AsyncResourceLoader::GetInstance();
asyncLoader.Initialize(4);  // ✅ 必须先初始化

auto* loader = world->RegisterSystem<ResourceLoadingSystem>(&asyncLoader);
loader->SetMaxTasksPerFrame(15);  // 每帧最多处理 15 个任务

// 查询当前设置
size_t maxTasks = loader->GetMaxTasksPerFrame();
Logger::InfoFormat("Max tasks per frame: %zu", maxTasks);

// 多纹理加载示例
MeshRenderComponent mesh;
mesh.meshName = "models/cube.obj";
mesh.materialName = "default";
mesh.textureOverrides["diffuse"] = "textures/custom_diffuse.png";
mesh.textureOverrides["normal"] = "textures/custom_normal.png";
world->AddComponent(entity, mesh);
// ResourceLoadingSystem 会自动加载所有纹理并应用到材质

// Sprite 纹理加载（优先从缓存）
SpriteRenderComponent sprite;
sprite.textureName = "textures/sprite.png";  // 如果已在 ResourceManager 中，直接使用缓存
world->AddComponent(entity, sprite);
```

**前置条件**：
- ⚠️ `AsyncResourceLoader` 必须在创建系统之前初始化
- ⚠️ 如果 AsyncResourceLoader 未初始化，系统会自动禁用异步加载并输出警告

**资源加载流程**：
1. 检查 `ResourceManager` 缓存是否已有资源
2. 如果有缓存，直接使用（零开销）
3. 如果无缓存，提交异步加载任务
4. 异步加载完成后自动注册到 `ResourceManager`
5. 通过待处理队列更新组件（线程安全）

**安全特性**：
- ✅ 使用 `weak_ptr` 防止 World 销毁时的悬空指针
- ✅ 使用 `m_shuttingDown` 标志防止关闭时的竞态条件
- ✅ 异步回调中的多重验证（实体有效性、组件存在性）
- ✅ 在 OnDestroy 时清理所有待处理任务和队列

---

### LightSystem

管理光源组件，收集光源数据并上传到着色器。

**优先级**：50

```cpp
class LightSystem : public System {
public:
    explicit LightSystem(Renderer* renderer);
    
    void Update(float deltaTime) override;
    int GetPriority() const override { return 50; }
    
    std::vector<EntityID> GetVisibleLights() const;
    size_t GetLightCount() const;
    
    // 获取主光源数据
    Vector3 GetPrimaryLightPosition() const;
    Color GetPrimaryLightColor() const;
    float GetPrimaryLightIntensity() const;
};
```

**功能**：
- 收集所有光源
- 更新光源 uniform
- 提供光源查询接口

**示例**：
```cpp
auto* lightSystem = world->RegisterSystem<LightSystem>(renderer);

// 获取可见光源
auto lights = lightSystem->GetVisibleLights();
std::cout << "Light count: " << lightSystem->GetLightCount() << std::endl;
```

---

### UniformSystem

自动管理全局 shader uniform（相机矩阵、光照数据、时间等）。

**优先级**：90（在渲染系统之前，在光照系统之后）

```cpp
class UniformSystem : public System {
public:
    explicit UniformSystem(Renderer* renderer);
    
    void Update(float deltaTime) override;
    int GetPriority() const override { return 90; }
    
    void OnCreate(World* world) override;
    void OnDestroy() override;
    
    void SetEnabled(bool enable);
    bool IsEnabled() const;
};
```

**功能**：
- 自动设置相机 uniform（视图矩阵、投影矩阵、视图投影矩阵、相机位置）
- 自动设置光照 uniform（主光源方向、颜色、强度等）
- 自动设置时间 uniform（累计时间、deltaTime）
- 通过 `UniformManager` 统一管理所有 uniform

**示例**：
```cpp
// 注册系统
world->RegisterSystem<UniformSystem>(renderer.get());

// 着色器中可以直接使用这些 uniform：
// - uViewMatrix, uProjectionMatrix, uViewProjectionMatrix
// - uCameraPosition
// - uLightDirection, uLightColor, uLightIntensity
// - uTime, uDeltaTime
```

---

### MeshRenderSystem

遍历所有 MeshRenderComponent，创建 MeshRenderable 并提交渲染。支持材质属性覆盖、视锥体裁剪、透明物体排序。

**优先级**：100

```cpp
class MeshRenderSystem : public System {
public:
    explicit MeshRenderSystem(Renderer* renderer);
    
    void Update(float deltaTime) override;
    int GetPriority() const override { return 100; }
    
    struct RenderStats {
        size_t visibleMeshes = 0;
        size_t culledMeshes = 0;
        size_t drawCalls = 0;
    };
    
    const RenderStats& GetStats() const;
};
```

**功能**：
- 遍历所有 MeshRenderComponent
- **材质属性覆盖**：应用 `materialOverride` 到材质（颜色、金属度、粗糙度等）
- **动态渲染状态**：根据材质属性自动调整混合模式、深度写入等
- **视锥体裁剪**：自动剔除相机不可见的对象
- **透明物体排序**：按深度从远到近排序透明物体，确保正确渲染
- **实例化渲染支持**：支持渲染多个实例（基础实现）
- **错误处理**：集成 `error.h` 宏进行健壮的错误处理
- 创建 MeshRenderable 对象
- 提交到渲染队列
- 提供渲染统计

**示例**：
```cpp
auto* meshSystem = world->RegisterSystem<MeshRenderSystem>(renderer);

// 获取渲染统计
const auto& stats = meshSystem->GetStats();
std::cout << "Visible: " << stats.visibleMeshes << std::endl;
std::cout << "Culled: " << stats.culledMeshes << std::endl;
std::cout << "Draw calls: " << stats.drawCalls << std::endl;
```

---

### SpriteRenderSystem

遍历所有 SpriteRenderComponent，创建 SpriteRenderable 并提交渲染。

**优先级**：200

```cpp
class SpriteRenderSystem : public System {
public:
    explicit SpriteRenderSystem(Renderer* renderer);
    
    void Update(float deltaTime) override;
    int GetPriority() const override { return 200; }
};
```

**功能**：
- 遍历所有具有 `TransformComponent + SpriteRenderComponent` 的实体
- 依据窗口大小构建屏幕空间正交矩阵并调用 `SpriteRenderable::SetViewProjection`
- 将组件数据写入对象池中的 `SpriteRenderable`（纹理、sourceRect、size、tintColor）
- 自动处理尺寸回退（缺省时使用纹理像素大小）与透明混合
- 将可见精灵提交到渲染队列，参与 Renderer 的统一排序与批处理

### SpriteAnimationSystem

驱动 `SpriteAnimationComponent`，按动画剪辑更新 `SpriteRenderComponent` 的显示帧。

**优先级**：180（在 `SpriteRenderSystem` 之前执行）

```cpp
class SpriteAnimationSystem : public System {
public:
    SpriteAnimationSystem() = default;
    
    void Update(float deltaTime) override;
    int GetPriority() const override { return 180; }
};
```

**功能**：
- 查询具有 `SpriteRenderComponent` 与 `SpriteAnimationComponent` 的实体
- 按剪辑帧时长推进 `currentFrame`，处理循环与停止
- 根据 `playbackSpeed` 控制播放倍率
- 将当前帧的 `Rect` 写回 `SpriteRenderComponent::sourceRect`
- 支持外部通过 `dirty` 标志强制刷新帧

**示例**：
```cpp
// 注册系统（保证动画系统在渲染系统之前）
world->RegisterSystem<SpriteAnimationSystem>();
world->RegisterSystem<SpriteRenderSystem>(renderer.get());

// 配置动画并播放
auto& anim = world->AddComponent<SpriteAnimationComponent>(entity);
SpriteAnimationClip walk;
walk.frames = {
    Rect(0.0f, 0.0f, 0.25f, 0.25f),
    Rect(0.25f, 0.0f, 0.25f, 0.25f),
};
walk.frameDuration = 0.1f;
anim.clips["walk"] = walk;
anim.Play("walk");
```

### ResourceCleanupSystem

定期清理未使用的资源，防止内存泄漏。

**优先级**：1000（最后执行，低优先级）

```cpp
class ResourceCleanupSystem : public System {
public:
    explicit ResourceCleanupSystem(float cleanupIntervalSeconds = 60.0f,
                                  uint32_t unusedFrameThreshold = 60);
    
    void Update(float deltaTime) override;
    int GetPriority() const override { return 1000; }
    
    void SetCleanupInterval(float seconds);
    float GetCleanupInterval() const;
    
    void ForceCleanup();
    
    void SetEnabled(bool enabled);
    bool IsEnabled() const;
    
    struct CleanupStats {
        size_t meshCleaned = 0;
        size_t textureCleaned = 0;
        size_t materialCleaned = 0;
        size_t shaderCleaned = 0;
        size_t totalCleaned = 0;
    };
    
    const CleanupStats& GetLastCleanupStats() const;
};
```

**功能**：
- 定期调用 `ResourceManager::CleanupUnused()` 清理未使用的资源
- 清理网格、纹理、材质、着色器等资源
- 防止内存泄漏
- 提供清理统计信息

**参数**：
- `cleanupIntervalSeconds`：清理间隔（秒），默认 60 秒
- `unusedFrameThreshold`：资源未使用多少帧后清理，默认 60 帧（约 1 秒）

**示例**：
```cpp
// 注册系统（每 60 秒清理一次）
world->RegisterSystem<ResourceCleanupSystem>();

// 自定义清理间隔（每 30 秒清理一次）
world->RegisterSystem<ResourceCleanupSystem>(30.0f, 60);

// 手动触发清理
auto* cleanupSystem = world->GetSystem<ResourceCleanupSystem>();
cleanupSystem->ForceCleanup();

// 获取清理统计
const auto& stats = cleanupSystem->GetLastCleanupStats();
std::cout << "Cleaned: " << stats.totalCleaned << " resources" << std::endl;
```

---

## 🛠️ 创建自定义系统

### 基本模板

```cpp
class MyCustomSystem : public System {
public:
    void OnCreate(World* world) override {
        System::OnCreate(world);
        // 初始化
    }
    
    void Update(float deltaTime) override {
        // 查询需要的实体
        auto entities = m_world->Query<MyComponent1, MyComponent2>();
        
        for (auto entity : entities) {
            auto& comp1 = m_world->GetComponent<MyComponent1>(entity);
            auto& comp2 = m_world->GetComponent<MyComponent2>(entity);
            
            // 处理逻辑
        }
    }
    
    int GetPriority() const override {
        return 50;  // 设置合适的优先级
    }
    
    void OnDestroy() override {
        // 清理
    }
};
```

---

### 示例：移动系统

```cpp
// 定义速度组件
struct VelocityComponent {
    Vector3 velocity;
    float maxSpeed = 10.0f;
};

// 移动系统
class MovementSystem : public System {
public:
    void Update(float deltaTime) override {
        auto entities = m_world->Query<TransformComponent, VelocityComponent>();
        
        for (auto entity : entities) {
            auto& transform = m_world->GetComponent<TransformComponent>(entity);
            auto& velocity = m_world->GetComponent<VelocityComponent>(entity);
            
            // 限制速度
            float speed = velocity.velocity.norm();
            if (speed > velocity.maxSpeed) {
                velocity.velocity = velocity.velocity.normalized() * velocity.maxSpeed;
            }
            
            // 更新位置
            Vector3 pos = transform.GetPosition();
            pos += velocity.velocity * deltaTime;
            transform.SetPosition(pos);
        }
    }
    
    int GetPriority() const override {
        return 15;  // 在 TransformSystem 之后
    }
};

// 注册和使用
world->RegisterComponent<VelocityComponent>();
world->RegisterSystem<MovementSystem>();
```

---

### 示例：旋转系统

```cpp
class RotationSystem : public System {
private:
    float m_totalTime = 0.0f;
    
public:
    void Update(float deltaTime) override {
        m_totalTime += deltaTime;
        
        auto entities = m_world->Query<TransformComponent>();
        
        size_t index = 0;
        for (auto entity : entities) {
            // 跳过相机
            if (m_world->HasComponent<CameraComponent>(entity)) {
                continue;
            }
            
            auto& transform = m_world->GetComponent<TransformComponent>(entity);
            
            // 每秒旋转 50 度，每个实体偏移 72 度
            float angle = m_totalTime * 50.0f + index * 72.0f;
            Quaternion rotation = MathUtils::FromEulerDegrees(0, angle, 0);
            transform.SetRotation(rotation);
            
            index++;
        }
    }
    
    int GetPriority() const override {
        return 15;
    }
};
```

---

### 示例：碰撞检测系统

```cpp
// 碰撞体组件
struct ColliderComponent {
    enum class Type { Box, Sphere };
    
    Type type = Type::Box;
    Vector3 size{1, 1, 1};  // Box: half extents, Sphere: radius in x
    Vector3 offset{0, 0, 0};
    bool isTrigger = false;
};

// 碰撞检测系统
class CollisionSystem : public System {
public:
    using CollisionCallback = std::function<void(EntityID, EntityID)>;
    
    void SetCollisionCallback(CollisionCallback callback) {
        m_callback = callback;
    }
    
    void Update(float deltaTime) override {
        auto entities = m_world->Query<TransformComponent, ColliderComponent>();
        
        // 简单的 O(n²) 碰撞检测
        for (size_t i = 0; i < entities.size(); ++i) {
            for (size_t j = i + 1; j < entities.size(); ++j) {
                if (CheckCollision(entities[i], entities[j])) {
                    if (m_callback) {
                        m_callback(entities[i], entities[j]);
                    }
                }
            }
        }
    }
    
    int GetPriority() const override {
        return 60;  // 在物理更新之后
    }
    
private:
    CollisionCallback m_callback;
    
    bool CheckCollision(EntityID a, EntityID b) {
        auto& transformA = m_world->GetComponent<TransformComponent>(a);
        auto& transformB = m_world->GetComponent<TransformComponent>(b);
        auto& colliderA = m_world->GetComponent<ColliderComponent>(a);
        auto& colliderB = m_world->GetComponent<ColliderComponent>(b);
        
        Vector3 posA = transformA.GetPosition() + colliderA.offset;
        Vector3 posB = transformB.GetPosition() + colliderB.offset;
        
        // 简化：仅检测球体碰撞
        float radiusA = colliderA.size.x();
        float radiusB = colliderB.size.x();
        float distance = (posA - posB).norm();
        
        return distance < (radiusA + radiusB);
    }
};
```

---

## 💡 系统设计最佳实践

### 1. 单一职责

每个系统应该只负责一个功能：

```cpp
// ✅ 好：职责单一
class MovementSystem : public System {
    void Update(float deltaTime) override {
        // 只处理移动
    }
};

class RenderSystem : public System {
    void Update(float deltaTime) override {
        // 只处理渲染
    }
};

// ❌ 差：职责混杂
class GameSystem : public System {
    void Update(float deltaTime) override {
        // 移动、渲染、碰撞检测、AI... 都在这里
    }
};
```

### 2. 避免系统间直接依赖

```cpp
// ✅ 好：通过组件通信
class DamageSystem : public System {
    void Update(float deltaTime) override {
        auto entities = m_world->Query<HealthComponent>();
        for (auto entity : entities) {
            auto& health = m_world->GetComponent<HealthComponent>(entity);
            // 处理伤害
        }
    }
};

// ❌ 差：直接依赖其他系统
class DamageSystem : public System {
    MovementSystem* m_movementSystem;  // 不要这样！
};
```

### 3. 缓存查询结果

```cpp
// ✅ 好：缓存查询
class MySystem : public System {
private:
    std::vector<EntityID> m_cachedEntities;
    
public:
    void OnCreate(World* world) override {
        System::OnCreate(world);
        UpdateCache();
    }
    
    void UpdateCache() {
        m_cachedEntities = m_world->Query<MyComponent>();
    }
    
    void Update(float deltaTime) override {
        for (auto entity : m_cachedEntities) {
            // 使用缓存的结果
        }
    }
};
```

### 4. 使用优先级控制执行顺序

```cpp
// 相机系统必须最先更新（更新视图矩阵）
int GetPriority() const override { return 5; }

// 变换系统在相机之后
int GetPriority() const override { return 10; }

// 渲染系统最后执行
int GetPriority() const override { return 100; }
```

---

## 📊 性能优化

### 1. 减少查询次数

```cpp
// ✅ 好：一次查询
void Update(float deltaTime) override {
    auto entities = m_world->Query<TransformComponent, VelocityComponent>();
    // ...
}

// ❌ 差：多次查询
void Update(float deltaTime) override {
    for (int i = 0; i < 100; i++) {
        auto entities = m_world->Query<TransformComponent>();  // 每次都查询！
    }
}
```

### 2. 避免不必要的组件访问

```cpp
// ✅ 好：先检查
if (m_world->HasComponent<OptionalComponent>(entity)) {
    auto& comp = m_world->GetComponent<OptionalComponent>(entity);
    // ...
}

// ❌ 差：直接访问（可能抛异常）
auto& comp = m_world->GetComponent<OptionalComponent>(entity);  // 如果不存在会崩溃！
```

---

## 🔒 线程安全

系统在 `World::Update()` 中顺序执行，不会并发运行。但是：

- 系统内部访问组件是线程安全的（ComponentArray 使用锁）
- 如果自定义系统使用多线程，需要自行保证线程安全

---

## 📖 相关文档

- [ECS 概览](ECS.md)
- [Entity API](Entity.md)
- [Component API](Component.md)
- [World API](World.md)

---

[返回 API 目录](README.md) | [返回 ECS 概览](ECS.md)

