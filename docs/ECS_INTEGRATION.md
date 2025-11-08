# ECS 集成指南

## 目录
[返回文档首页](README.md)

## 概述

本文档介绍如何将渲染引擎集成到基于 ECS（Entity Component System，实体组件系统）架构的项目中。

## 核心概念

### 组件系统
渲染引擎提供了一系列预定义的组件，可以直接挂载到ECS实体上：

1. **MeshComponent**: 3D网格渲染
2. **SpriteComponent**: 2D精灵渲染
3. **TextComponent**: 文本渲染
4. **LightComponent**: 光源
5. **CameraComponent**: 相机
6. **ParticleComponent**: 粒子系统
7. **TransformComponent**: 变换（位置、旋转、缩放）

### 渲染系统
渲染系统负责从ECS中收集组件并提交到渲染引擎。

## 组件定义

### TransformComponent

```cpp
struct TransformComponent {
    Vector3 position{0.0f, 0.0f, 0.0f};
    Quaternion rotation{1.0f, 0.0f, 0.0f, 0.0f};
    Vector3 scale{1.0f, 1.0f, 1.0f};
    
    Matrix4 GetTransformMatrix() const;
    void Translate(const Vector3& delta);
    void Rotate(const Quaternion& delta);
};

class ECSContext; // 前置声明

void UpdateTransformSystem(ECSContext* context, float dt);
```

### MeshComponent

```cpp
struct MeshComponent {
    MeshHandle meshHandle = INVALID_HANDLE;
    MaterialHandle materialHandle = INVALID_HANDLE;
    
    bool castShadows = true;
    bool receiveShadows = true;
    
    // 层级信息
    uint32_t layerID = OPAQUE_OBJECTS;
    int32_t renderPriority = 0;
    
    // LOD设置
    std::vector<float> lodDistances{100.0f, 500.0f};
    
    // 可序列化数据
    std::string meshPath;
    std::string materialPath;
};

void RenderMeshSystem(ECSContext* context, Renderer* renderer);
```

### SpriteComponent

```cpp
struct SpriteComponent {
    TextureHandle textureHandle = INVALID_HANDLE;
    
    Rect sourceRect{0, 0, 1, 1}; // UV坐标
    Color tintColor{1.0f, 1.0f, 1.0f, 1.0f};
    Vector2 size{1.0f, 1.0f};
    
    // 渲染属性
    BlendMode blendMode = BlendMode::Alpha;
    uint32_t layerID = UI_LAYER;
    
    std::string texturePath;
};

void RenderSpriteSystem(ECSContext* context, Renderer* renderer);
```

### TextComponent

```cpp
struct TextComponent {
    FontHandle fontHandle = INVALID_HANDLE;
    std::string text;
    
    Color color{1.0f, 1.0f, 1.0f, 1.0f};
    float fontSize = 32.0f;
    
    // 对齐方式
    TextAlignment horizontalAlign = TextAlignment::Left;
    TextAlignment verticalAlign = TextAlignment::Top;
    
    // 换行和约束
    bool wordWrap = true;
    float maxWidth = 0.0f;
    
    std::string fontPath;
};

void RenderTextSystem(ECSContext* context, Renderer* renderer);
```

### LightComponent

```cpp
enum class LightType {
    Directional,
    Point,
    Spot,
    Area
};

struct LightComponent {
    LightType type = LightType::Point;
    
    Color color{1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    
    // Point/Spot光源属性
    float range = 10.0f;
    float attenuation = 1.0f;
    
    // Spot光源属性
    float innerCone = 30.0f;  // 内角
    float outerCone = 45.0f;  // 外角
    
    // 阴影
    bool castShadows = false;
    uint32_t shadowResolution = 1024;
    float shadowBias = 0.001f;
    
    // 影响层级
    uint32_t layerMask = 0xFFFFFFFF;
};

void UpdateLightSystem(ECSContext* context, Renderer* renderer);
```

### CameraComponent

```cpp
enum class ProjectionType {
    Perspective,
    Orthographic
};

struct CameraComponent {
    ProjectionType projection = ProjectionType::Perspective;
    
    // Perspective
    float fieldOfView = 45.0f;
    float aspectRatio = 16.0f / 9.0f;
    
    // Orthographic
    float orthoSize = 10.0f;
    
    // 通用
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
    
    // 渲染目标
    RenderTextureHandle renderTarget = INVALID_HANDLE;
    Color clearColor{0.1f, 0.1f, 0.1f, 1.0f};
    
    // 可见性
    bool enabled = true;
    uint32_t layerMask = 0xFFFFFFFF;
    
    // 排序
    int32_t depth = 0;
};

void UpdateCameraSystem(ECSContext* context, Renderer* renderer);
```

### ParticleComponent

```cpp
struct ParticleComponent {
    // 发射器设置
    TextureHandle particleTexture = INVALID_HANDLE;
    float emissionRate = 100.0f;
    
    // 粒子属性
    float lifetime = 2.0f;
    Vector3 velocity{0.0f, 1.0f, 0.0f};
    float velocityRandomness = 0.5f;
    
    Vector2 size{0.1f, 0.2f};
    Color startColor{1.0f, 1.0f, 1.0f, 1.0f};
    Color endColor{1.0f, 1.0f, 1.0f, 0.0f};
    
    // 物理
    Vector3 gravity{0.0f, -9.8f, 0.0f};
    float damping = 0.95f;
    
    // 渲染
    BlendMode blendMode = BlendMode::Additive;
    uint32_t layerID = EFFECTS_LAYER;
    
    // 运行状态
    bool playing = true;
    float time = 0.0f;
};

void UpdateParticleSystem(ECSContext* context, float dt);
void RenderParticleSystem(ECSContext* context, Renderer* renderer);
```

## 系统实现

### 基础系统模板

```cpp
class RenderSystem {
public:
    RenderSystem(Renderer* renderer) : m_renderer(renderer) {}
    
    void Update(ECSContext* context, float dt) {
        // 更新逻辑
        UpdateEntities(context, dt);
        
        // 提交渲染
        SubmitToRenderer(context);
    }
    
private:
    Renderer* m_renderer;
    
    void UpdateEntities(ECSContext* context, float dt);
    void SubmitToRenderer(ECSContext* context);
};

template<typename Component>
class ComponentSystem : public RenderSystem {
public:
    ComponentSystem(Renderer* renderer) : RenderSystem(renderer) {}
    
    void Update(ECSContext* context, float dt) override {
        auto entities = context->GetEntitiesWithComponent<Component>();
        
        for (auto entity : entities) {
            Component& comp = context->GetComponent<Component>(entity);
            
            // 验证组件有效性
            if (!IsValid(comp)) {
                continue;
            }
            
            // 更新组件
            UpdateComponent(context, entity, comp, dt);
            
            // 提交渲染
            SubmitComponent(context, entity, comp);
        }
    }
    
protected:
    virtual void UpdateComponent(ECSContext* context, 
                                  EntityID entity, 
                                  Component& comp, 
                                  float dt) {}
    
    virtual void SubmitComponent(ECSContext* context,
                                  EntityID entity,
                                  const Component& comp) = 0;
    
    virtual bool IsValid(const Component& comp) const {
        return true;
    }
};
```

### 网格渲染系统实现

```cpp
class MeshRenderSystem : public ComponentSystem<MeshComponent> {
public:
    MeshRenderSystem(Renderer* renderer) 
        : ComponentSystem<MeshComponent>(renderer) {}
    
protected:
    void UpdateComponent(ECSContext* context,
                         EntityID entity,
                         MeshComponent& comp,
                         float dt) override {
        // 加载资源（如果尚未加载）
        if (comp.meshHandle == INVALID_HANDLE && !comp.meshPath.empty()) {
            comp.meshHandle = m_renderer->LoadMesh(comp.meshPath);
        }
        if (comp.materialHandle == INVALID_HANDLE && !comp.materialPath.empty()) {
            comp.materialHandle = m_renderer->LoadMaterial(comp.materialPath);
        }
    }
    
    void SubmitComponent(ECSContext* context,
                         EntityID entity,
                         const MeshComponent& comp) override {
        // 获取变换
        TransformComponent* transform = 
            context->GetComponentPtr<TransformComponent>(entity);
        
        if (!transform) {
            return;
        }
        
        // 创建渲染对象
        MeshRenderable* renderable = m_renderer->CreateMeshRenderable();
        renderable->SetMesh(comp.meshHandle);
        renderable->SetMaterial(comp.materialHandle);
        renderable->SetTransform(transform->GetTransformMatrix());
        
        // 设置层级
        RenderLayer* layer = m_renderer->GetLayer(comp.layerID);
        renderable->SetLayer(layer);
        renderable->SetRenderPriority(comp.renderPriority);
        
        // 阴影设置
        Material* material = m_renderer->GetMaterial(comp.materialHandle);
        if (material) {
            material->SetCastShadow(comp.castShadows);
            material->SetReceiveShadow(comp.receiveShadows);
        }
        
        // 提交
        m_renderer->Submit(renderable);
    }
};
```

### 变换系统

```cpp
class TransformSystem {
public:
    void Update(ECSContext* context, float dt) {
        auto entities = context->GetEntitiesWithComponent<TransformComponent>();
        
        // 更新全局变换
        for (auto entity : entities) {
            TransformComponent& transform = 
                context->GetComponent<TransformComponent>(entity);
            
            // 更新变换矩阵缓存
            transform.matrix = transform.GetTransformMatrix();
            
            // 如果有父节点，计算世界变换
            if (auto parent = context->GetParent(entity)) {
                TransformComponent& parentTransform = 
                    context->GetComponent<TransformComponent>(parent);
                transform.worldMatrix = 
                    parentTransform.worldMatrix * transform.matrix;
            } else {
                transform.worldMatrix = transform.matrix;
            }
        }
    }
};
```

## 集成示例

### 完整场景构建

```cpp
class GameScene {
public:
    void Initialize() {
        // 创建渲染器
        m_renderer = Renderer::Create();
        m_renderer->Initialize();
        
        // 创建ECS上下文
        m_ecs = CreateECSContext();
        
        // 注册组件
        m_ecs->RegisterComponent<TransformComponent>();
        m_ecs->RegisterComponent<MeshComponent>();
        m_ecs->RegisterComponent<SpriteComponent>();
        m_ecs->RegisterComponent<CameraComponent>();
        m_ecs->RegisterComponent<LightComponent>();
        
        // 注册系统
        m_ecs->RegisterSystem<TransformSystem>();
        m_ecs->RegisterSystem<MeshRenderSystem>(m_renderer);
        m_ecs->RegisterSystem<SpriteRenderSystem>(m_renderer);
        m_ecs->RegisterSystem<CameraSystem>(m_renderer);
        m_ecs->RegisterSystem<LightSystem>(m_renderer);
        
        // 创建场景
        CreateScene();
    }
    
    void Update(float dt) {
        // 更新ECS系统
        m_ecs->Update(dt);
        
        // 渲染
        m_renderer->BeginFrame();
        m_renderer->Clear();
        
        // 渲染所有相机
        auto cameras = m_ecs->GetEntitiesWithComponent<CameraComponent>();
        for (auto cameraEntity : cameras) {
            CameraComponent& camera = 
                m_ecs->GetComponent<CameraComponent>(cameraEntity);
            
            if (!camera.enabled) continue;
            
            m_renderer->RenderWithCamera(m_renderer->GetCameraFromComponent(camera));
        }
        
        m_renderer->EndFrame();
        m_renderer->Present();
    }
    
private:
    Renderer* m_renderer;
    ECSContext* m_ecs;
    
    void CreateScene();
};

void GameScene::CreateScene() {
    // 创建相机
    EntityID mainCamera = m_ecs->CreateEntity();
    m_ecs->AddComponent<CameraComponent>(mainCamera, {
        .projection = ProjectionType::Perspective,
        .fieldOfView = 45.0f,
        .nearPlane = 0.1f,
        .farPlane = 1000.0f,
        .enabled = true
    });
    
    TransformComponent& cameraTransform = 
        m_ecs->AddComponent<TransformComponent>(mainCamera);
    cameraTransform.position = {0.0f, 2.0f, 5.0f};
    
    // 创建光源
    EntityID light = m_ecs->CreateEntity();
    m_ecs->AddComponent<LightComponent>(light, {
        .type = LightType::Directional,
        .color = {1.0f, 1.0f, 0.9f},
        .intensity = 1.0f,
        .castShadows = true
    });
    
    TransformComponent& lightTransform = 
        m_ecs->AddComponent<TransformComponent>(light);
    lightTransform.rotation = Quaternion::FromEuler({30.0f, 45.0f, 0.0f});
    
    // 创建地面
    EntityID ground = m_ecs->CreateEntity();
    m_ecs->AddComponent<MeshComponent>(ground, {
        .meshPath = "models/ground.obj",
        .materialPath = "materials/ground.mat",
        .layerID = WORLD_GEOMETRY,
        .castShadows = false,
        .receiveShadows = true
    });
    
    TransformComponent& groundTransform = 
        m_ecs->AddComponent<TransformComponent>(ground);
    groundTransform.scale = {10.0f, 1.0f, 10.0f};
    
    // 创建角色
    EntityID character = m_ecs->CreateEntity();
    m_ecs->AddComponent<MeshComponent>(character, {
        .meshPath = "models/character.obj",
        .materialPath = "materials/character.mat",
        .layerID = OPAQUE_OBJECTS,
        .castShadows = true,
        .receiveShadows = true
    });
    
    TransformComponent& characterTransform = 
        m_ecs->AddComponent<TransformComponent>(character);
    characterTransform.position = {0.0f, 1.0f, 0.0f};
}
```

## 组件序列化

### 保存场景

```cpp
void SaveScene(ECSContext* context, const std::string& path) {
    nlohmann::json scene;
    
    // 序列化所有实体
    for (auto entity : context->GetAllEntities()) {
        nlohmann::json entityJson;
        entityJson["id"] = entity.id;
        
        // 序列化所有组件
        if (auto* transform = context->GetComponentPtr<TransformComponent>(entity)) {
            entityJson["transform"] = {
                {"position", transform->position},
                {"rotation", transform->rotation},
                {"scale", transform->scale}
            };
        }
        
        if (auto* mesh = context->GetComponentPtr<MeshComponent>(entity)) {
            entityJson["mesh"] = {
                {"meshPath", mesh->meshPath},
                {"materialPath", mesh->materialPath},
                {"layerID", mesh->layerID}
            };
        }
        
        // ... 其他组件
        
        scene["entities"].push_back(entityJson);
    }
    
    // 保存到文件
    std::ofstream file(path);
    file << scene.dump(4);
}

void LoadScene(ECSContext* context, const std::string& path) {
    std::ifstream file(path);
    nlohmann::json scene = nlohmann::json::parse(file);
    
    // 加载实体
    for (auto& entityJson : scene["entities"]) {
        EntityID entity = context->CreateEntity();
        
        // 加载组件
        if (entityJson.contains("transform")) {
            TransformComponent transform;
            transform.position = entityJson["transform"]["position"];
            transform.rotation = entityJson["transform"]["rotation"];
            transform.scale = entityJson["transform"]["scale"];
            context->AddComponent(entity, transform);
        }
        
        // ... 其他组件
    }
}
```

---

[返回文档首页](README.md) | [上一篇: 渲染层级管理](RENDERING_LAYERS.md) | [下一篇: 开发指南](DEVELOPMENT_GUIDE.md)

