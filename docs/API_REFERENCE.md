# API 参考文档

## 目录
[返回文档首页](README.md)

## 核心 API

### 应用层系统 🆕 **Phase 2**

应用层提供了统一的应用框架，包括场景管理、模块系统、事件总线和工具链集成。

- **[ApplicationHost](api/ApplicationHost.md)** - 应用宿主，统一入口和生命周期管理
- **[ModuleRegistry](api/ModuleRegistry.md)** - 模块注册表，管理应用模块的生命周期和依赖
- **[AppContext](api/AppContext.md)** - 应用上下文，提供核心服务引用
- **[EventBus](api/EventBus.md)** - 事件总线，类型安全的事件订阅和发布
- **[SceneManager](api/SceneManager.md)** - 场景管理器，管理场景栈和热切换

**应用层详细文档**:
- [场景API](application/Scene_API.md) - 场景接口详细文档
- [模块开发指南](application/Module_Guide.md) - 模块开发指南
- [事件总线使用指南](application/EventBus_Guide.md) - 事件系统使用指南
- [工具链集成指南](application/Toolchain_Intergration_Guide.md) - 工具链集成指南

**快速开始示例**:
```cpp
#include "render/application/application_host.h"

// 创建ApplicationHost
ApplicationHost host;
ApplicationHost::Config config{};
config.renderer = renderer;
config.resourceManager = &ResourceManager::GetInstance();
config.asyncLoader = &AsyncResourceLoader::GetInstance();

if (!host.Initialize(config)) {
    return -1;
}

// 注册模块
auto& moduleRegistry = host.GetModuleRegistry();
moduleRegistry.RegisterModule(std::make_unique<CoreRenderModule>());

// 注册场景
auto& sceneManager = host.GetSceneManager();
sceneManager.RegisterSceneFactory("BootScene", []() {
    return std::make_unique<BootScene>();
});

// 加载场景
sceneManager.PushScene("BootScene");

// 主循环
while (running) {
    FrameUpdateArgs frameArgs{};
    frameArgs.deltaTime = renderer->GetDeltaTime();
    host.UpdateFrame(frameArgs);
    
    renderer->BeginFrame();
    host.UpdateWorld(frameArgs.deltaTime);
    renderer->EndFrame();
    renderer->Present();
}
```

---

### 状态管理（RenderState）

状态管理系统提供了 OpenGL 状态缓存，减少冗余的 API 调用，提高渲染性能。

```cpp
#include "render/renderer.h"
#include "render/render_state.h"

// 获取渲染状态管理器
RenderState* state = renderer->GetRenderState();

// 着色器程序管理
state->UseProgram(shaderProgramId);  // 自动缓存，重复调用会被跳过

// VAO/VBO 绑定管理
state->BindVertexArray(vaoId);       // 自动缓存
state->BindBuffer(RenderState::BufferTarget::ArrayBuffer, vboId);

// 纹理绑定管理（支持32个纹理单元）
state->BindTexture(0, diffuseTexture);   // 纹理单元 0
state->BindTexture(1, normalTexture);    // 纹理单元 1
state->BindTexture(2, specularTexture);  // 纹理单元 2

// 渲染状态设置
state->SetDepthTest(true);
state->SetDepthFunc(DepthFunc::Less);
state->SetBlendMode(BlendMode::Alpha);
state->SetCullFace(true);
state->SetCullMode(CullMode::Back);

// 批量渲染示例（状态缓存优化）
for (auto& mesh : meshes) {
    // 只有状态真正改变时才会调用 OpenGL API
    state->UseProgram(mesh.shaderId);
    state->BindTexture(0, mesh.textureId);
    state->BindVertexArray(mesh.vaoId);
    
    glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
}
```

**性能提示**：
- 状态切换有开销，按状态分组渲染可提高性能
- 纹理绑定和着色器切换是最昂贵的操作
- 使用状态缓存可减少 50-80% 的状态切换调用

详见：[RenderState API 文档](api/RenderState.md)

### Renderer 初始化

```cpp
#include "render/renderer.h"

Renderer* renderer = Renderer::Create();
if (!renderer->Initialize()) {
    // 初始化失败处理
    return false;
}

// 设置窗口属性
renderer->SetWindowSize(1920, 1080);
renderer->SetVSync(true);
renderer->SetClearColor(0.1f, 0.1f, 0.1f, 1.0f);
```

### 渲染层级管理

```cpp
// 创建渲染层级
RenderLayer* backgroundLayer = renderer->CreateLayer(100);
RenderLayer* gameLayer = renderer->CreateLayer(200);
RenderLayer* uiLayer = renderer->CreateLayer(300);

// 设置层级启用状态
backgroundLayer->SetEnabled(true);
gameLayer->SetEnabled(true);
uiLayer->SetEnabled(true);

// 渲染层级会在帧结束时按优先级排序并渲染
```

### 3D 渲染

#### 网格渲染

```cpp
// 创建网格
Mesh* mesh = renderer->CreateMesh();

// 设置顶点数据
std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
    {{ 0.5f, -0.5f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
    {{ 0.5f,  0.5f, 0.0f}, {1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}},
    {{-0.5f,  0.5f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}}
};
mesh->SetVertices(vertices);

// 设置索引数据
std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};
mesh->SetIndices(indices);

// 加载纹理
Texture* texture = renderer->LoadTexture("path/to/texture.png");

// 创建材质
Material* material = renderer->CreateMaterial();
material->SetTexture("diffuse", texture);
material->SetShader(renderer->LoadShader("shaders/base.vert", "shaders/base.frag"));

// 创建网格对象
MeshRenderable* meshObj = renderer->CreateMeshRenderable();
meshObj->SetMesh(mesh);
meshObj->SetMaterial(material);
meshObj->SetLayer(gameLayer);

// 添加到场景
renderer->Submit(meshObj);
```

#### 相机控制

```cpp
// 创建相机
Camera* camera = renderer->CreateCamera();
camera->SetPosition({0.0f, 0.0f, 5.0f});
camera->SetTarget({0.0f, 0.0f, 0.0f});
camera->SetUp({0.0f, 1.0f, 0.0f});
camera->SetProjection(Camera::PERSPECTIVE);
camera->SetFieldOfView(45.0f);
camera->SetNearPlane(0.1f);
camera->SetFarPlane(1000.0f);

// 设置主相机
renderer->SetMainCamera(camera);

// 相机更新
void UpdateCamera(float dt) {
    Vector3 position = camera->GetPosition();
    
    // 相机旋转
    float yaw = getMouseDeltaX() * cameraSensitivity;
    float pitch = getMouseDeltaY() * cameraSensitivity;
    
    camera->Rotate(yaw, pitch);
    
    // 相机移动
    Vector3 velocity;
    if (isKeyPressed('W')) velocity += camera->GetForward();
    if (isKeyPressed('S')) velocity -= camera->GetForward();
    if (isKeyPressed('D')) velocity += camera->GetRight();
    if (isKeyPressed('A')) velocity -= camera->GetRight();
    
    camera->Translate(velocity * moveSpeed * dt);
}
```

#### 光照系统

```cpp
// 创建定向光
DirectionalLight* dirLight = renderer->CreateDirectionalLight();
dirLight->SetDirection({0.5f, -1.0f, 0.3f});
dirLight->SetColor({1.0f, 1.0f, 0.9f});
dirLight->SetIntensity(1.0f);

// 创建点光源
PointLight* pointLight = renderer->CreatePointLight();
pointLight->SetPosition({2.0f, 2.0f, 2.0f});
pointLight->SetColor({1.0f, 0.5f, 0.5f});
pointLight->SetIntensity(1.0f);
pointLight->SetRadius(10.0f);

// 创建聚光灯
SpotLight* spotLight = renderer->CreateSpotLight();
spotLight->SetPosition({0.0f, 5.0f, 0.0f});
spotLight->SetDirection({0.0f, -1.0f, 0.0f});
spotLight->SetColor({1.0f, 1.0f, 1.0f});
spotLight->SetIntensity(2.0f);
spotLight->SetConeAngle(30.0f);

// 设置环境光
renderer->SetAmbientLight({0.2f, 0.2f, 0.2f});
```

#### 阴影渲染

```cpp
// 启用阴影
renderer->EnableShadows(true);
renderer->SetShadowResolution(2048);

// 设置阴影图
DirectionalLight* light = renderer->CreateDirectionalLight();
light->SetCastShadows(true);
light->SetShadowBias(0.001f);

// 材质支持阴影
Material* material = renderer->CreateMaterial();
material->SetCastShadow(true);
material->SetReceiveShadow(true);
```

### 2D 渲染

#### 精灵渲染

```cpp
// 创建精灵
Sprite* sprite = renderer->CreateSprite();
sprite->SetTexture(renderer->LoadTexture("sprite.png"));
sprite->SetPosition({100.0f, 100.0f});
sprite->SetSize({64.0f, 64.0f});
sprite->SetLayer(uiLayer);

// 创建UI元素
UIPanel* panel = renderer->CreateUIPanel();
panel->SetPosition({50.0f, 50.0f});
panel->SetSize({200.0f, 150.0f});
panel->SetColor({0.2f, 0.2f, 0.2f, 0.8f});
panel->SetLayer(uiLayer);

// 添加子元素
UIButton* button = renderer->CreateUIButton();
button->SetPosition({10.0f, 10.0f});
button->SetSize({100.0f, 40.0f});
button->SetText("Click Me");
button->SetOnClick([]() {
    std::cout << "Button clicked!" << std::endl;
});
panel->AddChild(button);
```

#### 文本渲染

```cpp
// 加载字体
Font* font = renderer->LoadFont("fonts/Roboto.ttf", 32);

// 创建文本对象
Text* text = renderer->CreateText();
text->SetFont(font);
text->SetText("Hello, World!");
text->SetPosition({100.0f, 100.0f});
text->SetColor({1.0f, 1.0f, 1.0f});
text->SetLayer(uiLayer);

// 文本格式化
Text* fpsText = renderer->CreateText();
fpsText->SetFont(font);
fpsText->SetPosition({10.0f, 10.0f});

void UpdateFPS(float fps) {
    fpsText->SetText(std::format("FPS: {:.1f}", fps));
}
```

#### 粒子系统

```cpp
// 创建粒子发射器
ParticleEmitter* emitter = renderer->CreateParticleEmitter();
emitter->SetPosition({0.0f, 0.0f, 0.0f});
emitter->SetTexture(renderer->LoadTexture("particle.png"));

// 设置粒子参数
emitter->SetEmissionRate(100.0f); // 每秒发射粒子数
emitter->SetLifetime(2.0f);       // 粒子寿命
emitter->SetVelocity({0.0f, 1.0f, 0.0f}, 0.5f); // 速度和随机性
emitter->SetSize(0.05f, 0.1f);     // 大小范围
emitter->SetColor({1.0f, 0.5f, 0.0f}, {1.0f, 1.0f, 0.0f}); // 颜色变化

// 设置重力
emitter->SetGravity({0.0f, -9.8f, 0.0f});

// 设置层级
emitter->SetLayer(gameLayer);
```

### 后处理效果

```cpp
// 启用后处理
renderer->EnablePostProcessing(true);

// 添加效果
renderer->AddPostProcessEffect("bloom");
renderer->AddPostProcessEffect("hdr_tone_mapping");
renderer->AddPostProcessEffect("color_grading");

// 设置效果参数
PostProcessEffect* bloom = renderer->GetPostProcessEffect("bloom");
bloom->SetParameter("intensity", 0.5f);
bloom->SetParameter("threshold", 1.0f);

// 自定义后处理
class CustomPostProcess : public PostProcessEffect {
public:
    void Process(RenderTexture* input, RenderTexture* output) override {
        // 自定义处理逻辑
    }
};

renderer->RegisterCustomEffect("custom", std::make_unique<CustomPostProcess>());
```

### 渲染循环

```cpp
void GameLoop() {
    while (isRunning) {
        float deltaTime = updateDeltaTime();
        
        // 更新逻辑
        UpdateGameLogic(deltaTime);
        
        // 开始帧
        renderer->BeginFrame();
        
        // 清空渲染目标
        renderer->Clear();
        
        // 更新相机
        camera->Update(deltaTime);
        
        // 渲染场景
        renderer->RenderScene(camera);
        
        // 渲染UI
        renderer->RenderUI();
        
        // 应用后处理
        renderer->ApplyPostProcessing();
        
        // 结束帧并呈现
        renderer->EndFrame();
        renderer->Present();
    }
}
```

### 资源管理

```cpp
// 预加载资源
ResourceManager* resourceManager = renderer->GetResourceManager();

// 异步加载
resourceManager->LoadAsync("models/character.obj", [](Mesh* mesh) {
    if (mesh) {
        // 使用加载的网格
    }
});

// 同步加载
Texture* texture = resourceManager->LoadTexture("textures/diffuse.png");

// 卸载资源
resourceManager->UnloadTexture(texture);

// 清理未使用的资源
resourceManager->CleanupUnusedResources();
```

### 性能监控

```cpp
// 启用性能监控
renderer->EnableProfiling(true);

// 获取统计信息
RenderStats stats = renderer->GetStats();
std::cout << "Draw Calls: " << stats.drawCalls << std::endl;
std::cout << "Triangles: " << stats.triangles << std::endl;
std::cout << "Vertices: " << stats.vertices << std::endl;
std::cout << "FPS: " << stats.fps << std::endl;
std::cout << "Frame Time: " << stats.frameTime << "ms" << std::endl;

// 设置性能目标
renderer->SetTargetFrameRate(60);
renderer->SetMaxFrameTime(16.67f);
```

---

[返回文档首页](README.md) | [上一篇: 架构设计](ARCHITECTURE.md) | [下一篇: 渲染层级管理](RENDERING_LAYERS.md)

