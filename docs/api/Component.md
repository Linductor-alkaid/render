# Component API 参考

[返回 API 目录](README.md) | [返回 ECS 概览](ECS.md)

---

## 📋 概述

Component（组件）是 ECS 架构中的纯数据结构，不包含逻辑。组件存储在 `ComponentRegistry` 中，通过 `ComponentArray` 进行管理。

**命名空间**：`Render::ECS`

**头文件**：
- `<render/ecs/components.h>` - 内置组件定义
- `<render/ecs/component_registry.h>` - 组件注册表和数组

---

## 🧩 内置组件

### TransformComponent

变换组件，使用 shared_ptr 复用 Transform 对象。

**父子关系管理**（方案B - 使用实体ID）:
- 使用 `parentEntity` 存储父实体ID而非直接的Transform指针
- 由 TransformSystem 负责同步实体ID到Transform指针
- 确保生命周期安全，父实体销毁时自动清除关系

```cpp
struct TransformComponent {
    Ref<Transform> transform;                  // 复用 Transform 对象（shared_ptr）
    EntityID parentEntity = EntityID::Invalid(); // 父实体ID（安全的父子关系管理）
    
    TransformComponent();
    explicit TransformComponent(const Ref<Transform>& t);
    
    // ==================== 快捷访问接口 ====================
    
    void SetPosition(const Vector3& pos);
    void SetRotation(const Quaternion& rot);
    void SetScale(const Vector3& scale);
    void SetScale(float uniformScale);
    
    Vector3 GetPosition() const;
    Quaternion GetRotation() const;
    Vector3 GetScale() const;
    
    Matrix4 GetLocalMatrix() const;
    Matrix4 GetWorldMatrix() const;
    
    void LookAt(const Vector3& target, const Vector3& up = Vector3::UnitY());
    
    // ==================== 父子关系（基于实体ID）====================
    
    bool SetParentEntity(World* world, EntityID parent);
    EntityID GetParentEntity() const;
    bool RemoveParent();
    bool ValidateParentEntity(World* world);
    
    // 兼容性接口（deprecated）
    Transform* GetParent() const;
    
    // ==================== 验证和调试 ====================
    
    bool Validate() const;
    std::string DebugString() const;
    int GetHierarchyDepth() const;
    int GetChildCount() const;
};
```

**方法说明**：

#### 基础变换方法

- `SetPosition(pos)` - 设置本地位置
- `SetRotation(rot)` - 设置本地旋转
- `SetScale(scale)` - 设置本地缩放
- `GetPosition()` - 获取本地位置
- `GetRotation()` - 获取本地旋转
- `GetScale()` - 获取本地缩放
- `GetLocalMatrix()` - 获取本地变换矩阵
- `GetWorldMatrix()` - 获取世界变换矩阵
- `LookAt(target, up)` - 朝向目标点

#### 父子关系方法（推荐使用）

- `SetParentEntity(world, parent)` - **设置父实体（通过实体ID）**
  - 参数：`world` - World 对象指针，`parent` - 父实体ID
  - 返回：成功返回 true，失败返回 false
  - 会验证父实体有效性、检测循环引用、检测层级深度限制
  
- `GetParentEntity()` - **获取父实体ID**
  - 返回：父实体ID，如果没有父实体返回 Invalid
  
- `RemoveParent()` - **移除父对象**
  - 返回：总是返回 true
  
- `ValidateParentEntity(world)` - **验证父实体有效性**
  - 如果父实体已销毁，会自动清除父子关系
  - 返回：如果父实体有效（或没有父实体）返回 true

#### 验证和调试方法

- `Validate()` - **验证 Transform 状态**
  - 检查四元数归一化、NaN/Inf、缩放范围、父指针有效性等
  
- `DebugString()` - **获取调试字符串**
  - 返回格式化的调试信息
  
- `GetHierarchyDepth()` - **获取层级深度**
  - 返回到根节点的深度（0 = 无父对象）
  
- `GetChildCount()` - **获取子对象数量**

#### 兼容性方法（不推荐）

- `GetParent()` - **获取父对象（原始指针）**
  - ⚠️ Deprecated: 建议使用 `GetParentEntity()`
  - 返回的指针可能会失效

**示例**：

```cpp
// 基础使用
TransformComponent transform;
transform.SetPosition(Vector3(0, 1, 0));
transform.SetRotation(MathUtils::FromEulerDegrees(0, 45, 0));
transform.SetScale(2.0f);

world->AddComponent(entity, transform);

// 父子关系（方案B - 推荐）
EntityID parent = world->CreateEntity();
EntityID child = world->CreateEntity();

world->AddComponent(parent, TransformComponent{});
world->AddComponent(child, TransformComponent{});

auto& childComp = world->GetComponent<TransformComponent>(child);
if (!childComp.SetParentEntity(&world, parent)) {
    Logger::Error("Failed to set parent (circular reference or invalid)");
}

// 更新后自动同步
world->Update(0.016f);

// 验证
if (!childComp.Validate()) {
    Logger::Warning("Invalid Transform: " + childComp.DebugString());
}
```

---

### MeshRenderComponent

3D 网格渲染组件，支持异步资源加载、材质属性覆盖、多纹理支持、实例化渲染。

```cpp
struct MeshRenderComponent {
    // ==================== 资源引用 ====================
    std::string meshName;          // 网格资源名称
    std::string materialName;      // 材质资源名称
    std::string shaderName;         // 着色器名称（可选，覆盖材质的着色器）
    
    Ref<Mesh> mesh;                // 网格对象（延迟加载）
    Ref<Material> material;        // 材质对象（延迟加载）
    
    // ==================== 渲染属性 ====================
    bool visible = true;           // 是否可见
    bool castShadows = true;       // 是否投射阴影
    bool receiveShadows = true;    // 是否接收阴影
    uint32_t layerID = 300;        // 渲染层级（WORLD_GEOMETRY）
    uint32_t renderPriority = 0;   // 渲染优先级
    
    // ==================== 材质属性覆盖 ====================
    struct MaterialOverride {
        std::optional<Color> diffuseColor;      // 漫反射颜色覆盖
        std::optional<Color> specularColor;      // 镜面反射颜色覆盖
        std::optional<Color> emissiveColor;      // 自发光颜色覆盖
        std::optional<float> shininess;         // 镜面反射强度覆盖
        std::optional<float> metallic;          // 金属度覆盖
        std::optional<float> roughness;         // 粗糙度覆盖
        std::optional<float> opacity;           // 不透明度覆盖
    };
    MaterialOverride materialOverride;  // 材质属性覆盖
    
    // ==================== 纹理设置 ====================
    struct TextureSettings {
        bool generateMipmaps = true;
        // 可以扩展纹理参数（过滤模式、包裹模式等）
    };
    std::unordered_map<std::string, TextureSettings> textureSettings;  // 纹理设置
    std::unordered_map<std::string, std::string> textureOverrides;      // 纹理覆盖（纹理名 -> 资源路径）
    
    // ==================== LOD 支持 ====================
    std::vector<float> lodDistances;  // LOD 距离阈值
    
    // ==================== 实例化渲染支持 ====================
    bool useInstancing = false;                      // 是否使用实例化渲染
    uint32_t instanceCount = 1;                      // 实例数量
    std::vector<Matrix4> instanceTransforms;         // 实例变换矩阵（可选）
    
    // ==================== 异步加载状态 ====================
    bool resourcesLoaded = false;     // 资源是否已加载
    bool asyncLoading = false;        // 是否正在异步加载
    
    // ==================== 便捷方法 ====================
    void SetDiffuseColor(const Color& color);
    void SetSpecularColor(const Color& color);
    void SetEmissiveColor(const Color& color);
    void SetShininess(float value);
    void SetMetallic(float value);
    void SetRoughness(float value);
    void SetOpacity(float value);
    void ClearMaterialOverrides();
};
```

**示例**：
```cpp
// 异步加载模式
MeshRenderComponent mesh;
mesh.meshName = "models/cube.obj";      // 设置资源名称
mesh.materialName = "default";
mesh.visible = true;
mesh.castShadows = true;
// ResourceLoadingSystem 会自动加载资源
world->AddComponent(entity, mesh);

// 使用材质属性覆盖
MeshRenderComponent mesh;
mesh.meshName = "models/cube.obj";
mesh.materialName = "default";
mesh.SetDiffuseColor(Color(1, 0, 0, 1));  // 覆盖为红色
mesh.SetMetallic(0.8f);                   // 覆盖金属度
mesh.SetRoughness(0.2f);                  // 覆盖粗糙度
world->AddComponent(entity, mesh);

// 使用纹理覆盖
MeshRenderComponent mesh;
mesh.meshName = "models/cube.obj";
mesh.materialName = "default";
mesh.textureOverrides["diffuse"] = "textures/custom_diffuse.png";  // 覆盖漫反射纹理
mesh.textureOverrides["normal"] = "textures/custom_normal.png";    // 覆盖法线纹理
world->AddComponent(entity, mesh);

// 实例化渲染
MeshRenderComponent mesh;
mesh.meshName = "models/grass.obj";
mesh.materialName = "grass";
mesh.useInstancing = true;
mesh.instanceCount = 100;  // 渲染 100 个实例
// instanceTransforms 可以手动设置每个实例的变换
world->AddComponent(entity, mesh);

// 同步加载模式
MeshRenderComponent mesh;
mesh.mesh = MeshLoader::LoadFromFile("models/cube.obj");
mesh.material = myMaterial;
mesh.resourcesLoaded = true;
world->AddComponent(entity, mesh);
```

---

### SpriteRenderComponent

2D 精灵渲染组件。

```cpp
struct SpriteRenderComponent {
    std::string textureName;       // 纹理资源名称
    Ref<Texture> texture;          // 纹理对象（延迟加载）
    
    Rect sourceRect{0, 0, 1, 1};   // 源矩形（UV 坐标）
    Vector2 size{1.0f, 1.0f};      // 显示大小
    Color tintColor{1, 1, 1, 1};   // 着色
    
    bool visible = true;
    uint32_t layerID = 800;        // UI_LAYER
    
    bool resourcesLoaded = false;
    bool asyncLoading = false;
};
```

**示例**：
```cpp
SpriteRenderComponent sprite;
sprite.textureName = "textures/player.png";
sprite.size = Vector2(64, 64);
sprite.tintColor = Color(1, 1, 1, 1);
world->AddComponent(entity, sprite);
```

---

### CameraComponent

相机组件，使用 shared_ptr 复用 Camera 对象。支持离屏渲染。

**安全性特性**：
- 显式初始化camera为nullptr，避免未初始化问题
- 提供 `IsValid()` 快速验证
- 提供 `Validate()` 严格验证
- 提供 `DebugString()` 调试支持

```cpp
struct CameraComponent {
    Ref<Camera> camera;            // 相机对象（复用）
    
    bool active = true;            // 是否激活
    uint32_t layerMask = 0xFFFFFFFF;  // 可见层级遮罩
    int32_t depth = 0;             // 渲染深度（深度越低越先渲染）
    Color clearColor{0.1f, 0.1f, 0.1f, 1.0f};  // 清屏颜色
    bool clearDepth = true;        // 是否清除深度缓冲
    bool clearStencil = false;     // 是否清除模板缓冲
    
    // ==================== 渲染目标（离屏渲染）====================
    std::string renderTargetName;  // 渲染目标名称（可选，用于调试）
    Ref<Framebuffer> renderTarget; // 渲染目标（nullptr = 渲染到屏幕）
    
    // ==================== 构造函数 ====================
    CameraComponent();                          // 显式初始化camera为nullptr
    explicit CameraComponent(const Ref<Camera>& cam);
    
    // ==================== 便捷方法 ====================
    bool IsOffscreen() const;      // 判断是否渲染到离屏目标
    bool IsValid() const;          // 快速检查相机是否可用
    bool Validate() const;         // 严格验证组件状态
    std::string DebugString() const; // 获取调试信息
};
```

**方法说明**：

| 方法 | 说明 |
|------|------|
| `IsOffscreen()` | 判断是否渲染到离屏目标 |
| `IsValid()` | 快速检查：camera非空且active为true |
| `Validate()` | 严格验证：检查camera、renderTarget等 |
| `DebugString()` | 返回组件状态的字符串表示，用于日志 |

**示例**：
```cpp
// 普通相机（渲染到屏幕）
CameraComponent cameraComp;
cameraComp.camera = std::make_shared<Camera>();
cameraComp.camera->SetPerspective(60.0f, 16.0f/9.0f, 0.1f, 1000.0f);
cameraComp.active = true;
cameraComp.depth = 0;  // 主相机（depth越小优先级越高）
cameraComp.clearDepth = true;  // 清除深度缓冲
world->AddComponent(entity, cameraComp);

// 离屏渲染相机（渲染到纹理）
auto fbo = std::make_shared<Framebuffer>(1024, 1024);
fbo->AttachColorTexture();
fbo->AttachDepthTexture();

CameraComponent offscreenCamera;
offscreenCamera.camera = std::make_shared<Camera>();
offscreenCamera.camera->SetPerspective(90.0f, 1.0f, 0.1f, 100.0f);
offscreenCamera.renderTarget = fbo;  // 设置渲染目标
offscreenCamera.renderTargetName = "shadowMap";
offscreenCamera.depth = 10;  // 低优先级相机
offscreenCamera.clearDepth = true;
world->AddComponent(entity, offscreenCamera);

// 使用验证方法（推荐）
CameraComponent& cameraComp = world->GetComponent<CameraComponent>(entity);
if (cameraComp.IsValid()) {
    // 安全使用相机
    Matrix4 viewMatrix = cameraComp.camera->GetViewMatrix();
} else {
    Logger::Warning("Camera component is invalid!");
}

// 调试输出
Logger::Debug(cameraComp.DebugString());
// 输出：CameraComponent{active=true, camera=valid, depth=0, layerMask=0xffffffff}
```

**主相机选择规则**：
- `CameraSystem` 会自动选择 `depth` 最小的激活相机作为主相机
- 如果主相机被禁用或删除，会自动切换到下一个有效相机
- 可以通过 `depth` 控制相机优先级（0 = 最高优先级）

---

### LightComponent

光源组件。

```cpp
enum class LightType {
    Directional,   // 定向光
    Point,         // 点光源
    Spot,          // 聚光灯
    Area           // 区域光（未来支持）
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
```

**示例**：
```cpp
// 定向光
LightComponent dirLight;
dirLight.type = LightType::Directional;
dirLight.color = Color(1.0f, 1.0f, 0.9f);
dirLight.intensity = 1.0f;
world->AddComponent(entity, dirLight);

// 点光源
LightComponent pointLight;
pointLight.type = LightType::Point;
pointLight.range = 10.0f;
pointLight.attenuation = 1.0f;
world->AddComponent(entity, pointLight);

// 聚光灯
LightComponent spotLight;
spotLight.type = LightType::Spot;
spotLight.innerConeAngle = 20.0f;
spotLight.outerConeAngle = 30.0f;
world->AddComponent(entity, spotLight);
```

---

### NameComponent

名称组件，为实体提供可读名称。

```cpp
struct NameComponent {
    std::string name;
    
    NameComponent() = default;
    explicit NameComponent(const std::string& n);
};
```

**示例**：
```cpp
NameComponent name("Player");
world->AddComponent(entity, name);
```

---

### TagComponent

标签组件，为实体添加多个标签。

```cpp
struct TagComponent {
    std::unordered_set<std::string> tags;
    
    bool HasTag(const std::string& tag) const;
    void AddTag(const std::string& tag);
    void RemoveTag(const std::string& tag);
    void Clear();
    std::vector<std::string> GetTagList() const;
};
```

**示例**：
```cpp
TagComponent tags({"enemy", "flying"});
tags.AddTag("boss");
tags.RemoveTag("flying");

if (tags.HasTag("boss")) {
    // 这是一个 boss
}
```

---

### ActiveComponent

激活状态组件。

```cpp
struct ActiveComponent {
    bool active = true;
    
    ActiveComponent() = default;
    explicit ActiveComponent(bool a);
};
```

**示例**：
```cpp
ActiveComponent active(true);
world->AddComponent(entity, active);
```

---

### GeometryComponent

几何形状组件，用于程序化生成基本几何形状。与 `MeshRenderComponent` 配合使用，由 `GeometrySystem` 自动生成网格。

```cpp
enum class GeometryType {
    Cube,       // 立方体
    Sphere,     // 球体
    Cylinder,   // 圆柱体
    Cone,       // 圆锥体
    Plane,      // 平面
    Quad,       // 四边形（2D）
    Torus,      // 圆环
    Capsule,    // 胶囊体
    Triangle,   // 三角形
    Circle      // 圆形（2D）
};

struct GeometryComponent {
    GeometryType type = GeometryType::Cube;  // 几何形状类型
    
    // 通用参数
    float size = 1.0f;                       // 大小（缩放因子）
    int segments = 16;                       // 分段数（影响精度）
    
    // Sphere/Cylinder/Cone 专用
    int rings = 16;                          // 环数（仅用于球体、圆柱等）
    
    // Cylinder/Cone 专用
    float height = 1.0f;                     // 高度
    
    // Torus 专用
    float innerRadius = 0.25f;               // 内半径
    float outerRadius = 0.5f;                // 外半径
    
    // Capsule 专用
    float radius = 0.5f;                     // 半径
    float cylinderHeight = 1.0f;             // 中间圆柱部分的高度
    
    bool generated = false;                  // 是否已生成网格（内部使用）
    
    GeometryComponent() = default;
    explicit GeometryComponent(GeometryType t) : type(t) {}
};
```

**示例**：
```cpp
// 创建球体
EntityID sphere = world->CreateEntity({.name = "Sphere"});

// 添加几何形状组件
GeometryComponent geom;
geom.type = GeometryType::Sphere;
geom.size = 2.0f;
geom.segments = 32;
geom.rings = 32;
world->AddComponent(sphere, geom);

// 添加变换组件
TransformComponent transform;
transform.SetPosition(Vector3(0, 2, 0));
world->AddComponent(sphere, transform);

// 添加网格渲染组件（GeometrySystem 会自动生成网格）
MeshRenderComponent mesh;
mesh.materialName = "default";
world->AddComponent(sphere, mesh);

// 创建圆柱体
EntityID cylinder = world->CreateEntity({.name = "Cylinder"});

GeometryComponent cylinderGeom;
cylinderGeom.type = GeometryType::Cylinder;
cylinderGeom.size = 1.0f;
cylinderGeom.height = 2.0f;
cylinderGeom.segments = 32;
cylinderGeom.rings = 16;
world->AddComponent(cylinder, cylinderGeom);

// 创建圆环
EntityID torus = world->CreateEntity({.name = "Torus"});

GeometryComponent torusGeom;
torusGeom.type = GeometryType::Torus;
torusGeom.size = 1.0f;
torusGeom.innerRadius = 0.3f;
torusGeom.outerRadius = 0.5f;
torusGeom.segments = 32;
world->AddComponent(torus, torusGeom);
```

**说明**：
- `GeometrySystem` 会在每帧检查所有具有 `GeometryComponent` 但 `generated == false` 的实体
- 自动调用 `MeshLoader` 生成对应形状的网格
- 将生成的网格赋值给同实体的 `MeshRenderComponent::mesh`
- 设置 `generated = true` 避免重复生成

---

## 📦 ComponentArray

具体类型的组件数组，使用 unordered_map 存储组件。

### 类定义

```cpp
template<typename T>
class ComponentArray : public IComponentArray {
public:
    // 添加/移除
    void Add(EntityID entity, const T& component);
    void Add(EntityID entity, T&& component);
    void Remove(EntityID entity);
    
    // 获取
    T& Get(EntityID entity);
    const T& Get(EntityID entity) const;
    bool Has(EntityID entity) const;
    
    // 统计
    size_t Size() const override;
    void Clear() override;
    
    // 遍历
    template<typename Func>
    void ForEach(Func&& func);
    
    std::vector<EntityID> GetEntities() const;
};
```

### 示例

```cpp
// 获取组件数组
auto* array = world->GetComponentRegistry().GetComponentArray<TransformComponent>();

// 遍历所有 Transform 组件
array->ForEach([](EntityID entity, TransformComponent& transform) {
    Vector3 pos = transform.GetPosition();
    std::cout << "Entity " << entity.index << " at " << pos << std::endl;
});

// 获取所有实体
auto entities = array->GetEntities();
```

---

## 📚 ComponentRegistry

组件注册表，管理所有组件类型的存储和访问。

**🆕 安全性改进（v1.1）**：新增了安全的迭代接口，推荐使用 `ForEachComponent` 等方法替代直接获取组件数组。

### 类定义

```cpp
class ComponentRegistry {
public:
    // 组件类型注册
    template<typename T>
    void RegisterComponent();
    
    // ==================== 基础组件操作 ====================
    
    template<typename T>
    void AddComponent(EntityID entity, const T& component);
    
    template<typename T>
    void AddComponent(EntityID entity, T&& component);
    
    template<typename T>
    void RemoveComponent(EntityID entity);
    
    template<typename T>
    T& GetComponent(EntityID entity);
    
    template<typename T>
    const T& GetComponent(EntityID entity) const;
    
    template<typename T>
    bool HasComponent(EntityID entity) const;
    
    void RemoveAllComponents(EntityID entity);
    void Clear();
    
    // ==================== 🆕 安全的迭代接口（推荐）====================
    
    /**
     * @brief 遍历指定类型的所有组件（安全接口）
     * @note 这是推荐的迭代方式，在回调期间持有锁确保线程安全
     */
    template<typename T, typename Func>
    void ForEachComponent(Func&& func);
    
    template<typename T, typename Func>
    void ForEachComponent(Func&& func) const;  // 只读版本
    
    /**
     * @brief 获取具有指定组件的所有实体（安全接口）
     * @return 实体 ID 列表快照
     */
    template<typename T>
    std::vector<EntityID> GetEntitiesWithComponent() const;
    
    /**
     * @brief 获取指定类型的组件数量
     */
    template<typename T>
    size_t GetComponentCount() const;
    
    // ==================== 兼容性接口（已废弃）====================
    
    /**
     * @brief 获取组件数组（已废弃）
     * @deprecated 请使用 ForEachComponent 或 GetEntitiesWithComponent 替代
     * @warning 返回裸指针，存在生命周期风险
     */
    template<typename T>
    [[deprecated("Use ForEachComponent or GetEntitiesWithComponent instead")]]
    ComponentArray<T>* GetComponentArray();
};
```

### 基础用法示例

```cpp
ComponentRegistry registry;

// 注册组件类型
registry.RegisterComponent<TransformComponent>();
registry.RegisterComponent<MeshRenderComponent>();

// 添加组件
EntityID entity = ...;
TransformComponent transform;
registry.AddComponent(entity, transform);

// 获取组件
auto& transform = registry.GetComponent<TransformComponent>(entity);
transform.SetPosition(Vector3(0, 1, 0));

// 检查组件
if (registry.HasComponent<MeshRenderComponent>(entity)) {
    auto& mesh = registry.GetComponent<MeshRenderComponent>(entity);
}

// 移除组件
registry.RemoveComponent<TransformComponent>(entity);

// 移除所有组件
registry.RemoveAllComponents(entity);
```

### 🆕 安全迭代接口示例（推荐）

#### ForEachComponent - 遍历所有组件

```cpp
// ✅ 推荐：使用 ForEachComponent 安全遍历
registry.ForEachComponent<TransformComponent>(
    [](EntityID entity, TransformComponent& transform) {
        // 在锁保护下安全访问组件
        transform.SetPosition(Vector3::Zero());
        Logger::Info("Updated entity " + std::to_string(entity.index));
    }
);

// ✅ 只读访问
registry.ForEachComponent<MeshRenderComponent>(
    [](EntityID entity, const MeshRenderComponent& mesh) {
        if (mesh.visible) {
            Logger::Info("Entity " + std::to_string(entity.index) + " is visible");
        }
    }
);
```

#### GetEntitiesWithComponent - 获取实体列表

```cpp
// ✅ 推荐：获取实体列表
auto entities = registry.GetEntitiesWithComponent<TransformComponent>();

Logger::Info("Found " + std::to_string(entities.size()) + " entities with Transform");

for (const auto& entity : entities) {
    // 注意：使用前应该检查实体有效性
    if (!world->IsValidEntity(entity)) continue;
    
    auto& transform = registry.GetComponent<TransformComponent>(entity);
    // 处理组件...
}
```

#### GetComponentCount - 获取组件数量

```cpp
// ✅ 推荐：获取组件数量
size_t transformCount = registry.GetComponentCount<TransformComponent>();
size_t meshCount = registry.GetComponentCount<MeshRenderComponent>();

Logger::Info("Transforms: " + std::to_string(transformCount) + 
             ", Meshes: " + std::to_string(meshCount));
```

### ⚠️ 旧接口（已废弃，不推荐）

```cpp
// ⚠️ 旧代码（会产生编译警告）：
auto* array = registry.GetComponentArray<TransformComponent>();  // deprecated
array->ForEach([](EntityID entity, TransformComponent& transform) {
    transform.SetPosition(Vector3::Zero());
});

// ✅ 新代码（推荐）：
registry.ForEachComponent<TransformComponent>(
    [](EntityID entity, TransformComponent& transform) {
        transform.SetPosition(Vector3::Zero());
    }
);
```

### 迁移指南

| 旧代码 | 新代码 | 说明 |
|-------|-------|------|
| `GetComponentArray<T>()->ForEach(...)` | `ForEachComponent<T>(...)` | 更安全，无需获取裸指针 |
| `GetComponentArray<T>()->GetEntities()` | `GetEntitiesWithComponent<T>()` | 直接获取实体列表 |
| `GetComponentArray<T>()->Size()` | `GetComponentCount<T>()` | 直接获取数量 |

---

## 🔧 自定义组件

你可以创建自己的组件：

### 定义组件

```cpp
// 简单组件
struct HealthComponent {
    float health = 100.0f;
    float maxHealth = 100.0f;
    
    void TakeDamage(float damage) {
        health = std::max(0.0f, health - damage);
    }
    
    bool IsAlive() const {
        return health > 0.0f;
    }
};

// 复杂组件
struct InventoryComponent {
    struct Item {
        std::string name;
        int quantity;
    };
    
    std::vector<Item> items;
    int maxSlots = 20;
    
    void AddItem(const std::string& name, int quantity) {
        // ...
    }
};
```

### 注册和使用

```cpp
// 注册组件
world->RegisterComponent<HealthComponent>();
world->RegisterComponent<InventoryComponent>();

// 使用组件
EntityID player = world->CreateEntity();

HealthComponent health;
health.health = 100.0f;
health.maxHealth = 100.0f;
world->AddComponent(player, health);

InventoryComponent inventory;
inventory.maxSlots = 30;
world->AddComponent(player, inventory);

// 访问组件
auto& playerHealth = world->GetComponent<HealthComponent>(player);
playerHealth.TakeDamage(25.0f);

if (!playerHealth.IsAlive()) {
    // 玩家死亡
}
```

---

## 💡 设计要点

### 1. 资源复用

使用 `std::shared_ptr` 复用大对象：

```cpp
// ✅ 好：复用 Transform 对象
TransformComponent comp;
comp.transform = std::make_shared<Transform>();  // 创建一次
comp.SetPosition(pos);  // 修改已有对象

// ❌ 差：每次创建新对象
Transform temp;  // 栈上创建
temp.SetPosition(pos);
// 销毁
```

### 2. 延迟加载

资源密集型组件支持延迟加载：

```cpp
MeshRenderComponent mesh;
mesh.meshName = "large_model.fbx";  // 只设置名称
mesh.asyncLoading = false;           // 尚未开始加载
// ResourceLoadingSystem 会异步加载资源
```

### 3. 数据导向设计

组件应该是纯数据，不包含逻辑：

```cpp
// ✅ 好：纯数据
struct VelocityComponent {
    Vector3 velocity;
    float maxSpeed;
};

// ❌ 差：包含逻辑
struct VelocityComponent {
    Vector3 velocity;
    
    void Update(float deltaTime) {  // 不要在组件中放逻辑！
        // ...
    }
};

// 逻辑应该在 System 中
class MovementSystem : public System {
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
};
```

---

## 🔒 线程安全

`ComponentArray` 和 `ComponentRegistry` 使用 `std::shared_mutex` 保护所有操作：

```cpp
// 读操作（共享锁）
std::shared_lock lock(m_mutex);
return m_components.find(entity) != m_components.end();

// 写操作（独占锁）
std::unique_lock lock(m_mutex);
m_components[entity] = component;
```

---

## 📊 性能优化

### 1. 缓存友好

相同类型的组件连续存储，提高缓存命中率：

```cpp
// ComponentArray 内部使用 unordered_map
std::unordered_map<EntityID, T, EntityID::Hash> m_components;
```

### 2. O(1) 访问

所有操作都是 O(1) 复杂度：

```cpp
// 添加：O(1)
array->Add(entity, component);

// 获取：O(1)
T& comp = array->Get(entity);

// 检查：O(1)
bool has = array->Has(entity);
```

---

## 📖 相关文档

- [ECS 概览](ECS.md)
- [Entity API](Entity.md)
- [System API](System.md)
- [World API](World.md)

---

[返回 API 目录](README.md) | [返回 ECS 概览](ECS.md)

