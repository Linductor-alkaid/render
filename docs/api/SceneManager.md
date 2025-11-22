# SceneManager API 文档

## 目录
[返回API文档首页](README.md)

---

## 概述

`SceneManager` 是应用层的场景管理器，负责管理场景栈、场景切换、资源预加载和场景序列化。它支持场景栈式管理、热切换、资源预加载和场景保存/加载。

**命名空间**: `Render::Application`

**完整文档**: [场景API](../application/Scene_API.md)

---

## 类定义

```cpp
class SceneManager {
public:
    void Initialize(AppContext* appContext, ModuleRegistry* modules);
    void Shutdown();
    
    // 场景工厂注册
    void RegisterSceneFactory(std::string sceneId, SceneFactory factory);
    bool HasSceneFactory(std::string_view sceneId) const;
    
    // 场景切换
    bool PushScene(std::string_view sceneId, SceneEnterArgs args = {});
    bool ReplaceScene(std::string_view sceneId, SceneEnterArgs args = {});
    std::optional<SceneSnapshot> PopScene(SceneExitArgs args = {});
    
    // 场景更新
    void Update(const FrameUpdateArgs& frameArgs);
    
    // 场景查询
    Scene* GetActiveScene() noexcept;
    const Scene* GetActiveScene() const noexcept;
    bool IsTransitionInProgress() const noexcept;
    size_t SceneCount() const noexcept;
    
    // 场景序列化
    bool SaveActiveScene(const std::string& filePath);
    bool LoadSceneFromFile(const std::string& filePath, SceneEnterArgs args = {});
};
```

---

## 核心功能

### 1. 初始化与关闭

#### `Initialize(AppContext* appContext, ModuleRegistry* modules)`

初始化场景管理器。

**参数**:
- `appContext`: 应用上下文指针
- `modules`: 模块注册表指针

**注意**: 通常在ApplicationHost初始化时自动调用，无需手动调用。

#### `Shutdown()`

关闭场景管理器，清理所有场景。

**注意**: 会自动调用所有场景的 `OnDetach` 方法。

---

### 2. 场景工厂注册

#### `RegisterSceneFactory(std::string sceneId, SceneFactory factory)`

注册场景工厂函数，用于创建场景实例。

**参数**:
- `sceneId`: 场景标识符
- `factory`: 场景工厂函数（`std::function<ScenePtr()>`）

**示例**:
```cpp
auto& sceneManager = host.GetSceneManager();
sceneManager.RegisterSceneFactory("BootScene", []() {
    return std::make_unique<BootScene>();
});
```

#### `HasSceneFactory(std::string_view sceneId) const`

检查场景工厂是否已注册。

**返回值**: 如果工厂已注册返回 `true`

---

### 3. 场景切换

#### `PushScene(std::string_view sceneId, SceneEnterArgs args = {})`

将新场景压入场景栈并激活。

**参数**:
- `sceneId`: 场景标识符
- `args`: 场景进入参数（可选）

**返回值**: 成功返回 `true`，失败返回 `false`

**流程**:
1. 通过工厂创建场景实例
2. 调用场景的 `OnAttach`
3. 调用场景的 `BuildManifest` 获取资源清单
4. 检查资源可用性（必需资源就绪后调用 `OnEnter`）

**示例**:
```cpp
SceneEnterArgs args{};
args.deltaTime = 0.0f;
args.absoluteTime = 0.0;
if (!sceneManager.PushScene("BootScene", args)) {
    LOG_ERROR("Failed to push scene");
}
```

#### `ReplaceScene(std::string_view sceneId, SceneEnterArgs args = {})`

替换栈顶场景（保留快照传递）。

**参数**:
- `sceneId`: 场景标识符
- `args`: 场景进入参数（可选）

**返回值**: 成功返回 `true`，失败返回 `false`

**注意**: 会先调用旧场景的 `OnExit` 保存快照，新场景的 `OnEnter` 可以访问 `previousSnapshot`。

**示例**:
```cpp
if (!sceneManager.ReplaceScene("MenuScene")) {
    LOG_ERROR("Failed to replace scene");
}
```

#### `PopScene(SceneExitArgs args = {})`

退出当前场景并返回快照。

**参数**:
- `args`: 场景退出参数（可选）

**返回值**: 场景快照，如果场景栈为空返回 `std::nullopt`

**示例**:
```cpp
auto snapshot = sceneManager.PopScene();
if (snapshot.has_value()) {
    // 使用快照
    auto sceneName = snapshot->Get("sceneName");
}
```

---

### 4. 场景更新和查询

#### `Update(const FrameUpdateArgs& frameArgs)`

调用当前活动场景的 `OnUpdate` 方法。

**参数**:
- `frameArgs`: 帧更新参数

**注意**: 通常由ApplicationHost自动调用，无需手动调用。

#### `GetActiveScene()`

获取当前活动场景指针。

**返回值**: 活动场景指针，如果没有活动场景返回 `nullptr`

**示例**:
```cpp
auto* activeScene = sceneManager.GetActiveScene();
if (activeScene) {
    LOG_INFO("Active scene: %s", activeScene->Name().data());
}
```

#### `IsTransitionInProgress() const`

检查是否有场景切换正在进行中。

**返回值**: 如果有切换进行中返回 `true`

**注意**: 在场景资源预加载期间会返回 `true`。

#### `SceneCount() const`

返回场景栈中的场景数量。

**返回值**: 场景数量

---

### 5. 场景序列化

#### `SaveActiveScene(const std::string& filePath)`

将当前活动场景保存到JSON文件。

**参数**:
- `filePath`: 保存路径

**返回值**: 成功返回 `true`，失败返回 `false`

**注意**: 会序列化场景中的所有ECS实体和组件。

**示例**:
```cpp
if (sceneManager.SaveActiveScene("my_scene.json")) {
    LOG_INFO("Scene saved successfully");
} else {
    LOG_ERROR("Failed to save scene");
}
```

#### `LoadSceneFromFile(const std::string& filePath, SceneEnterArgs args = {})`

从JSON文件加载场景并推入场景栈。

**参数**:
- `filePath`: JSON文件路径
- `args`: 场景进入参数（可选）

**返回值**: 成功返回 `true`，失败返回 `false`

**注意**: 会自动创建实体和组件，并从ResourceManager获取资源指针。

**示例**:
```cpp
if (sceneManager.LoadSceneFromFile("my_scene.json")) {
    LOG_INFO("Scene loaded successfully");
} else {
    LOG_ERROR("Failed to load scene");
}
```

---

## 场景生命周期

场景的生命周期包括以下阶段：

1. **OnAttach**: 场景被挂载到SceneManager时调用，用于注册ECS组件和系统
2. **BuildManifest**: 构建资源清单，列出必需和可选资源
3. **资源预加载**: SceneManager检查资源可用性，等待必需资源就绪
4. **OnEnter**: 场景激活时调用，场景数据加载完成
5. **OnUpdate**: 每帧调用，运行场景逻辑
6. **OnExit**: 场景退出时调用，返回场景快照
7. **OnDetach**: 场景从SceneManager移除时调用，用于清理资源

---

## 事件系统

SceneManager会在以下时机发布事件：

- **SceneTransitionEvent**: 场景切换时（PushScene/ReplaceScene/PopScene）
- **SceneLifecycleEvent**: 场景生命周期事件（OnAttach/OnEnter/OnExit/OnDetach）
- **SceneManifestEvent**: 执行BuildManifest后
- **ScenePreloadProgressEvent**: 资源预加载进度更新

**示例**:
```cpp
auto& eventBus = host.GetEventBus();
eventBus.Subscribe<SceneLifecycleEvent>(
    [](const SceneLifecycleEvent& event) {
        LOG_INFO("Scene lifecycle: %s", ToString(event.phase));
    }
);
```

---

## 使用示例

### 基本场景管理

```cpp
// 获取SceneManager
auto& sceneManager = host.GetSceneManager();

// 注册场景工厂
sceneManager.RegisterSceneFactory("BootScene", []() {
    return std::make_unique<BootScene>();
});
sceneManager.RegisterSceneFactory("MenuScene", []() {
    return std::make_unique<MenuScene>();
});

// 加载初始场景
if (!sceneManager.PushScene("BootScene")) {
    LOG_ERROR("Failed to push BootScene");
    return;
}

// 场景切换
if (!sceneManager.ReplaceScene("MenuScene")) {
    LOG_ERROR("Failed to replace scene");
    return;
}

// 返回上一场景
auto snapshot = sceneManager.PopScene();
if (snapshot.has_value()) {
    LOG_INFO("Returned to previous scene");
}
```

### 场景序列化

```cpp
// 保存场景
if (sceneManager.SaveActiveScene("saved_scene.json")) {
    LOG_INFO("Scene saved");
}

// 加载场景
if (sceneManager.LoadSceneFromFile("saved_scene.json")) {
    LOG_INFO("Scene loaded");
}
```

---

## 注意事项

1. **资源预加载**: 场景切换时，SceneManager会等待必需资源就绪后才进入场景
2. **场景栈**: 使用PushScene可以在场景栈中保留多个场景，使用ReplaceScene会替换当前场景
3. **快照传递**: ReplaceScene会保存旧场景的快照，新场景可以通过SceneEnterArgs访问
4. **线程安全**: SceneManager不是线程安全的，所有调用应在主线程中进行
5. **资源释放**: 场景退出时，会根据ResourceScope标记释放资源（Scene范围自动释放，Shared范围保留）

---

## 相关文档

- [场景API](../application/Scene_API.md) - 场景接口详细文档
- [ApplicationHost API](ApplicationHost.md) - 应用宿主API
- [AppContext API](AppContext.md) - 应用上下文API

---

[返回API文档首页](README.md)

