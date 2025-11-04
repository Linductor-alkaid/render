/**
 * @file 33_ecs_async_test.cpp
 * @brief ECS + 异步资源加载集成测试
 * 
 * 测试内容：
 * - ECS 实体动态创建
 * - 异步加载网格资源
 * - 资源加载进度显示
 * - 加载完成后自动渲染
 */

#include <SDL3/SDL.h>
#include <iostream>
#include <atomic>
#include <vector>

#include "render/renderer.h"
#include "render/shader_cache.h"
#include "render/mesh_loader.h"
#include "render/material.h"
#include "render/logger.h"
#include "render/resource_manager.h"
#include "render/async_resource_loader.h"
#include "render/ecs/world.h"
#include "render/ecs/systems.h"
#include "render/ecs/components.h"
#include "render/math_utils.h"

using namespace Render;
using namespace Render::ECS;

// 全局变量
std::atomic<size_t> g_loadingTotal = 0;
std::atomic<size_t> g_loadingCompleted = 0;
bool g_allLoaded = false;

// 简单的旋转System（演示自定义System）
class SimpleRotationSystem : public System {
public:
    void Update(float deltaTime) override {
        if (!m_world) {
            Logger::GetInstance().Warning("[SimpleRotationSystem] m_world is null");
            return;
        }
        
        auto entities = m_world->Query<TransformComponent>();
        Logger::GetInstance().DebugFormat("[SimpleRotationSystem] Processing %zu entities", entities.size());
        
        static float totalTime = 0.0f;
        totalTime += deltaTime;
        
        size_t index = 0;
        for (const auto& entity : entities) {
            // 跳过相机
            if (m_world->HasComponent<CameraComponent>(entity)) {
                continue;
            }
            
            auto& transform = m_world->GetComponent<TransformComponent>(entity);
            float angle = totalTime * 50.0f + index * 72.0f;  // 每秒旋转50度
            
            try {
                Quaternion rotation = MathUtils::FromEulerDegrees(0, angle, 0);
                transform.SetRotation(rotation);
            } catch (const std::exception& e) {
                Logger::GetInstance().ErrorFormat("[SimpleRotationSystem] Exception in entity %u: %s", 
                                                  entity.index, e.what());
            }
            
            index++;
        }
    }
    
    int GetPriority() const override { return 15; }  // 在渲染之前
};

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    Logger::GetInstance().InfoFormat("[ECS Async Test] === ECS + Async Loading Test ===");
    
    // ============================================================
    // 1. 初始化渲染器
    // ============================================================
    auto renderer = std::make_unique<Renderer>();
    if (!renderer->Initialize("ECS 异步加载测试", 1280, 720)) {
        Logger::GetInstance().ErrorFormat("[ECS Async Test] Failed to initialize renderer");
        return -1;
    }
    Logger::GetInstance().InfoFormat("[ECS Async Test] Renderer initialized");
    
    // 设置渲染状态
    auto renderState = renderer->GetRenderState();
    renderState->SetDepthTest(true);
    renderState->SetCullFace(CullFace::Back);
    renderState->SetClearColor(Color(0.05f, 0.05f, 0.1f, 1.0f));
    
    // ============================================================
    // 2. 初始化异步资源加载器
    // ============================================================
    auto& asyncLoader = AsyncResourceLoader::GetInstance();
    asyncLoader.Initialize(4);  // 4个工作线程
    Logger::GetInstance().InfoFormat("[ECS Async Test] AsyncResourceLoader initialized");
    
    // ============================================================
    // 3. 加载着色器和材质
    // ============================================================
    auto& shaderCache = ShaderCache::GetInstance();
    // ✅ 使用 Phong 着色器支持光照和纹理
    auto shader = shaderCache.LoadShader("phong", "shaders/material_phong.vert", "shaders/material_phong.frag");
    if (!shader) {
        Logger::GetInstance().ErrorFormat("[ECS Async Test] Failed to load shader");
        renderer->Shutdown();
        return -1;
    }
    Logger::GetInstance().InfoFormat("[ECS Async Test] Phong shader loaded");
    
    // 创建材质（Phong 光照材质）
    auto material = std::make_shared<Material>();
    material->SetName("PhongMaterial");
    material->SetShader(shader);
    // 设置材质属性
    material->SetAmbientColor(Color(0.2f, 0.2f, 0.2f, 1.0f));   // 环境光
    material->SetDiffuseColor(Color(1.0f, 1.0f, 1.0f, 1.0f));   // 漫反射（白色，使用纹理颜色）
    material->SetSpecularColor(Color(0.5f, 0.5f, 0.5f, 1.0f));  // 镜面反射
    material->SetShininess(32.0f);                               // 光泽度
    
    // ============================================================
    // 4. 创建 ECS World（使用shared_ptr管理生命周期）
    // ============================================================
    auto world = std::make_shared<World>();
    world->Initialize();
    
    // 注册组件
    world->RegisterComponent<TransformComponent>();
    world->RegisterComponent<NameComponent>();
    world->RegisterComponent<ActiveComponent>();
    world->RegisterComponent<MeshRenderComponent>();
    world->RegisterComponent<SpriteRenderComponent>();  // ✅ 添加：ResourceLoadingSystem需要
    world->RegisterComponent<CameraComponent>();
    
    // 添加系统
    world->RegisterSystem<CameraSystem>();  // ✅ 必须：更新相机的view矩阵
    world->RegisterSystem<TransformSystem>();
    world->RegisterSystem<ResourceLoadingSystem>(&asyncLoader);
    world->RegisterSystem<SimpleRotationSystem>();  // 旋转系统
    world->RegisterSystem<MeshRenderSystem>(renderer.get());
    
    // 后初始化（允许系统安全地获取其他系统的引用）
    world->PostInitialize();
    
    Logger::GetInstance().InfoFormat("[ECS Async Test] World initialized (managed by shared_ptr)");
    
    // ============================================================
    // 4.5. 配置加载模式（必须在使用前声明）
    // ============================================================
    const bool USE_REAL_ASYNC_LOADING = false; // ❌ Miku模型需要同步加载所有网格
    const bool USE_MIKU_MODEL = true;          // ✅ 启用miku模型（完整加载所有25个部件）
    const bool USE_MULTIPLE_CUBES = false;     // ❌ 关闭多个cube
    
    // ============================================================
    // 5. 创建相机
    // ============================================================
    EntityDescriptor cameraDesc;
    cameraDesc.name = "MainCamera";
    auto cameraEntity = world->CreateEntity(cameraDesc);
    
    TransformComponent cameraTransform;
    
    if (USE_MIKU_MODEL) {
        // miku模型：相机从前方看向模型（恢复到能看见的位置）
        cameraTransform.SetPosition(Vector3(0, 1.2f, 2.5f));  // 相机在模型前方
        cameraTransform.LookAt(Vector3(0, 1.0f, 0));  // 看向模型中心
    } else {
        // 普通模型：标准相机位置
        cameraTransform.SetPosition(Vector3(0, 2, 8));
        cameraTransform.LookAt(Vector3(0, 0, 0));
    }
    
    world->AddComponent(cameraEntity, cameraTransform);
    
    auto camera = std::make_shared<Camera>();
    camera->SetPerspective(60.0f, 1280.0f / 720.0f, 0.01f, 1000.0f);  // 近裁剪面改为0.01，精度更高
    
    CameraComponent cameraComp;
    cameraComp.camera = camera;
    cameraComp.active = true;
    world->AddComponent(cameraEntity, cameraComp);
    
    Logger::GetInstance().InfoFormat("[ECS Async Test] Camera created");
    
    // ============================================================
    // 6. 异步加载多个模型并创建实体
    // ============================================================
    
    std::vector<std::string> modelPaths;
    
    if (USE_MIKU_MODEL) {
        // ✅ 使用miku模型（参考20测试）
        // 注意：PMX模型包含多个网格部件，当前异步加载只支持单个mesh
        // TODO: 需要扩展异步加载系统支持LoadFromFileWithMaterials
        modelPaths = {
            "models/miku/v4c5.0short.pmx",  // miku模型
        };
        Logger::GetInstance().InfoFormat("[ECS Async Test] Will load Miku model (sync mode: all 25 parts)");
    } else if (USE_MULTIPLE_CUBES) {
        // ✅ 使用多个cube展示异步加载系统
        modelPaths = {
            "models/cube.obj",  // 这个文件存在
        };
        Logger::GetInstance().InfoFormat("[ECS Async Test] Will load multiple cube models asynchronously");
    } else {
        // 其他模型
        modelPaths = {
            "models/cube.obj",
        };
    }
    
    std::vector<EntityID> entities;
    
    if (USE_REAL_ASYNC_LOADING) {
        // ============================================================
        // 方式A：真正的异步加载（通过ResourceLoadingSystem）
        // ============================================================
        Logger::GetInstance().InfoFormat("[ECS Async Test] Using REAL async loading via ResourceLoadingSystem");
        
        size_t entityCount = USE_MIKU_MODEL ? 1 : (USE_MULTIPLE_CUBES ? 10 : 5);  // 多cube模式创建10个实体
        g_loadingTotal = entityCount;
        g_loadingCompleted = 0;
        
        Logger::GetInstance().InfoFormat("[ECS Async Test] Will create %zu entities for async loading", entityCount);
        
        // 使用weak_ptr捕获world，用于回调中更新计数
        std::weak_ptr<World> worldWeak = world;
        
        for (size_t i = 0; i < entityCount; ++i) {
            EntityDescriptor entityDesc;
            entityDesc.name = "AsyncModel_" + std::to_string(i);
            auto entity = world->CreateEntity(entityDesc);
            
            // 添加Transform
            TransformComponent transform;
            
            if (USE_MIKU_MODEL) {
                // miku模型：放在中心，适当调整高度和缩放
                transform.SetPosition(Vector3(0, 0, 0));
                transform.SetScale(0.12f);  // miku模型通常很大，稍微调大一点
            } else {
                // 普通模型：圆形排列或网格排列
                if (USE_MULTIPLE_CUBES && entityCount > 5) {
                    // 网格排列（更多实体时）
                    int cols = 5;
                    int row = i / cols;
                    int col = i % cols;
                    float spacing = 2.5f;
                    float x = (col - cols/2.0f) * spacing;
                    float z = (row - entityCount/cols/2.0f) * spacing;
                    transform.SetPosition(Vector3(x, 0, z));
                } else {
                    // 圆形排列
                    float angle = (float)i * (360.0f / entityCount);
                    float radius = 4.0f;
                    float x = radius * std::cos(angle * 3.14159f / 180.0f);
                    float z = radius * std::sin(angle * 3.14159f / 180.0f);
                    transform.SetPosition(Vector3(x, 0, z));
                }
            }
            
            world->AddComponent(entity, transform);
            
            // ✅ 正确方式：设置meshName，让ResourceLoadingSystem自动加载
            MeshRenderComponent meshComp;
            meshComp.meshName = modelPaths[i % modelPaths.size()];  // 设置要加载的文件
            meshComp.material = material;                            // 直接设置material
            meshComp.resourcesLoaded = false;                        // 标记为未加载
            meshComp.asyncLoading = false;                           // 还未开始
            meshComp.visible = true;                                 // 确保可见
            // 不设置mesh - ResourceLoadingSystem会异步加载
            world->AddComponent(entity, meshComp);
            
            Logger::GetInstance().InfoFormat("[ECS Async Test] Entity %zu: will load %s", 
                                             i, meshComp.meshName.c_str());
            
            entities.push_back(entity);
        }
        
        // ✅ 添加一个简单的加载完成检查机制
        Logger::GetInstance().InfoFormat("[ECS Async Test] ========================================");
        Logger::GetInstance().InfoFormat("[ECS Async Test] 已提交 %zu 个异步加载任务", entityCount);
        Logger::GetInstance().InfoFormat("[ECS Async Test] ResourceLoadingSystem将在Update中自动处理");
        Logger::GetInstance().InfoFormat("[ECS Async Test] ========================================");
        
        Logger::GetInstance().InfoFormat("[ECS Async Test] Created %zu entities for async loading", entities.size());
        Logger::GetInstance().InfoFormat("[ECS Async Test] ResourceLoadingSystem will load meshes asynchronously");
        
    } else {
        // ============================================================
        // 方式B：同步加载（用于Miku等多网格模型）
        // ============================================================
        Logger::GetInstance().InfoFormat("[ECS Async Test] Using synchronous loading with materials");
        
        if (USE_MIKU_MODEL && !modelPaths.empty()) {
            // 加载Miku模型的所有部件（使用LoadFromFileWithMaterials）
            Logger::GetInstance().InfoFormat("[ECS Async Test] Loading Miku model with all parts...");
            auto parts = MeshLoader::LoadFromFileWithMaterials(modelPaths[0], "", true, shader);
            
            if (parts.empty()) {
                Logger::GetInstance().ErrorFormat("[ECS Async Test] Failed to load Miku model!");
            } else {
                Logger::GetInstance().InfoFormat("[ECS Async Test] Loaded %zu mesh parts", parts.size());
                
                // 为每个网格部件创建一个实体
                // ✅ 关键：所有部件的顶点坐标已在模型空间中包含相对位置
                // 因此它们都应该用相同的世界Transform（作为一个整体变换）
                for (size_t i = 0; i < parts.size(); ++i) {
                    const auto& part = parts[i];
                    
                    EntityDescriptor entityDesc;
                    entityDesc.name = "MikuPart_" + std::to_string(i);
                    auto entity = world->CreateEntity(entityDesc);
                    
                    // 所有部件使用相同的世界Transform
                    TransformComponent transform;
                    transform.SetPosition(Vector3(0, 0, 0));
                    transform.SetScale(0.08f);  // Miku模型缩放（参考20测试）
                    world->AddComponent(entity, transform);
                    
                    // 设置Mesh和Material
                    MeshRenderComponent meshComp;
                    meshComp.mesh = part.mesh;
                    meshComp.material = part.material ? part.material : material;  // 使用自带材质或默认材质
                    meshComp.resourcesLoaded = true;
                    meshComp.visible = true;
                    world->AddComponent(entity, meshComp);
                    
                    entities.push_back(entity);
                    
                    if (i < 3) {
                        // 输出前几个部件的调试信息
                        Logger::GetInstance().InfoFormat("[ECS Async Test]   Part %zu: %s, vertices=%zu", 
                                                         i, part.name.c_str(), 
                                                         part.mesh ? part.mesh->GetVertexCount() : 0);
                    }
                }
            }
        } else {
            // 普通模型：创建测试立方体
            for (size_t i = 0; i < 5; ++i) {
                EntityDescriptor entityDesc;
                entityDesc.name = "SyncModel_" + std::to_string(i);
                auto entity = world->CreateEntity(entityDesc);
                
                TransformComponent transform;
                float angle = (float)i * (360.0f / 5.0f);
                float radius = 3.0f;
                float x = radius * std::cos(angle * 3.14159f / 180.0f);
                float z = radius * std::sin(angle * 3.14159f / 180.0f);
                transform.SetPosition(Vector3(x, 0, z));
                world->AddComponent(entity, transform);
                
                MeshRenderComponent meshComp;
                meshComp.mesh = MeshLoader::CreateCube(1.0f);
                meshComp.material = material;
                meshComp.resourcesLoaded = true;
                world->AddComponent(entity, meshComp);
                
                entities.push_back(entity);
            }
        }
        
        Logger::GetInstance().InfoFormat("[ECS Async Test] Created %zu entities", entities.size());
    }
    
    // 注意：旧的异步加载方式（直接调用asyncLoader）已被移除
    // 现在使用ResourceLoadingSystem统一管理异步加载，更安全可靠
    
    // ============================================================
    // 7. 设置全局uniform
    // ============================================================
    Matrix4 view = camera->GetViewMatrix();
    Matrix4 projection = camera->GetProjectionMatrix();
    
    // ============================================================
    // 8. 摄像机控制设置（ECS 方式）
    // ============================================================
    float cameraSpeed = 5.0f;       // 移动速度（单位/秒）
    float cameraSensitivity = 0.1f; // 鼠标灵敏度
    bool rightMousePressed = false;
    
    // 初始化相机的 yaw 和 pitch（根据 LookAt 方向计算）
    auto& initialCameraTransform = world->GetComponent<TransformComponent>(cameraEntity);
    Vector3 initialForward = initialCameraTransform.transform->GetForward();
    float cameraYaw = std::atan2(initialForward.z(), initialForward.x()) * 180.0f / 3.14159f - 90.0f;
    float cameraPitch = std::asin(initialForward.y()) * 180.0f / 3.14159f;
    
    // 光源位置（靠近Miku模型）
    Vector3 lightPos(2.0f, 3.0f, 2.0f);
    
    // ============================================================
    // 9. 主渲染循环
    // ============================================================
    Logger::GetInstance().InfoFormat("[ECS Async Test] Starting render loop...");
    Logger::GetInstance().InfoFormat("[ECS Async Test] ===== 控制说明 =====");
    Logger::GetInstance().InfoFormat("[ECS Async Test] WASD: 移动相机");
    Logger::GetInstance().InfoFormat("[ECS Async Test] QE: 上下移动");
    Logger::GetInstance().InfoFormat("[ECS Async Test] 右键拖拽: 旋转视角");
    Logger::GetInstance().InfoFormat("[ECS Async Test] 空格: 显示加载进度");
    Logger::GetInstance().InfoFormat("[ECS Async Test] ESC: 退出");
    Logger::GetInstance().InfoFormat("[ECS Async Test] ===================");
    
    bool running = true;
    int frameCount = 0;
    Uint64 lastTime = SDL_GetTicks();
    
    while (running) {
        // 事件处理
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) {
                    running = false;
                }
                // 按空格打印加载进度
                if (event.key.key == SDLK_SPACE) {
                    Logger::GetInstance().InfoFormat("[ECS Async Test] Loading: %zu/%zu", 
                                 g_loadingCompleted.load(), g_loadingTotal.load());
                    asyncLoader.PrintStatistics();
                }
            }
            // ✅ 鼠标右键控制（不使用 SDL_SetRelativeMouseMode）
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_RIGHT) {
                rightMousePressed = true;
            }
            if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_RIGHT) {
                rightMousePressed = false;
            }
            // 鼠标移动（使用相对移动量）
            if (event.type == SDL_EVENT_MOUSE_MOTION && rightMousePressed) {
                float xOffset = event.motion.xrel * cameraSensitivity;
                float yOffset = event.motion.yrel * cameraSensitivity;
                
                cameraYaw += xOffset;
                cameraPitch -= yOffset;  // 反转Y轴
                
                // 限制俯仰角，防止万向锁
                if (cameraPitch > 89.0f) cameraPitch = 89.0f;
                if (cameraPitch < -89.0f) cameraPitch = -89.0f;
            }
        }
        
        // ✅ 计算帧时间（必须在使用 deltaTime 之前）
        Uint64 currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;
        
        // ✅ ECS 方式：通过 TransformComponent 控制相机
        auto& cameraTransform = world->GetComponent<TransformComponent>(cameraEntity);
        
        // 根据 yaw 和 pitch 更新相机旋转
        float yawRad = cameraYaw * 3.14159f / 180.0f;
        float pitchRad = cameraPitch * 3.14159f / 180.0f;
        
        // 计算前方向向量
        Vector3 front;
        front.x() = std::cos(yawRad) * std::cos(pitchRad);
        front.y() = std::sin(pitchRad);
        front.z() = std::sin(yawRad) * std::cos(pitchRad);
        front.normalize();
        
        // 计算右方向和上方向
        Vector3 worldUp(0, 1, 0);
        Vector3 right = front.cross(worldUp).normalized();
        Vector3 up = right.cross(front).normalized();
        
        // 键盘移动控制（基于相机的局部坐标系）
        const bool* keyState = SDL_GetKeyboardState(nullptr);
        Vector3 cameraPos = cameraTransform.GetPosition();
        
        float moveSpeed = cameraSpeed * deltaTime;
        if (keyState[SDL_SCANCODE_W]) cameraPos += front * moveSpeed;   // 前进
        if (keyState[SDL_SCANCODE_S]) cameraPos -= front * moveSpeed;   // 后退
        if (keyState[SDL_SCANCODE_A]) cameraPos -= right * moveSpeed;   // 左移
        if (keyState[SDL_SCANCODE_D]) cameraPos += right * moveSpeed;   // 右移
        if (keyState[SDL_SCANCODE_Q]) cameraPos -= worldUp * moveSpeed; // 下降
        if (keyState[SDL_SCANCODE_E]) cameraPos += worldUp * moveSpeed; // 上升
        
        // ✅ 更新 ECS 的 TransformComponent（CameraSystem 会自动同步到 Camera）
        cameraTransform.SetPosition(cameraPos);
        cameraTransform.LookAt(cameraPos + front);
        
        // 开始渲染帧
        if (frameCount == 0) {
            Logger::GetInstance().InfoFormat("[ECS Async Test] First frame: BeginFrame...");
        }
        renderer->BeginFrame();
        renderer->Clear();
        
        // ECS 更新（旋转由SimpleRotationSystem自动处理）
        if (frameCount == 0) {
            Logger::GetInstance().InfoFormat("[ECS Async Test] First frame: Calling World.Update()...");
        }
        
        try {
            world->Update(deltaTime);
        } catch (const std::exception& e) {
            Logger::GetInstance().ErrorFormat("[ECS Async Test] Exception in World.Update(): %s", e.what());
            running = false;
            continue;
        }
        
        if (frameCount == 0) {
            Logger::GetInstance().InfoFormat("[ECS Async Test] First frame: World.Update() completed");
        }
        
        // ✅ 在FlushRenderQueue之前，设置全局 uniform（Phong 光照）
        shader->Use();
        auto uniformMgr = shader->GetUniformManager();
        if (uniformMgr) {
            // 更新相机矩阵（确保使用最新的view矩阵）
            view = camera->GetViewMatrix();
            uniformMgr->SetMatrix4("uView", view);
            uniformMgr->SetMatrix4("uProjection", projection);
            
            // ✅ Phong 光照 uniform
            Vector3 cameraPos = cameraTransform.GetPosition();
            uniformMgr->SetVector3("uLightPos", lightPos);      // 光源位置
            uniformMgr->SetVector3("uViewPos", cameraPos);      // 相机位置
            
            // 材质属性（也可以由 Material::Bind 设置）
            uniformMgr->SetColor("uAmbientColor", Color(0.5f, 0.5f, 0.5f, 1.0f));  // 增加环境光亮度
            uniformMgr->SetColor("uDiffuseColor", Color(1.0f, 1.0f, 1.0f, 1.0f));
            uniformMgr->SetColor("uSpecularColor", Color(0.5f, 0.5f, 0.5f, 1.0f));
            uniformMgr->SetFloat("uShininess", 32.0f);
            
            if (frameCount < 5) {
                Logger::GetInstance().InfoFormat("[ECS Async Test] Frame %d: Set Phong uniforms", frameCount);
                Logger::GetInstance().InfoFormat("[ECS Async Test]   Camera: (%.1f, %.1f, %.1f), Light: (%.1f, %.1f, %.1f)", 
                                                 cameraPos.x(), cameraPos.y(), cameraPos.z(),
                                                 lightPos.x(), lightPos.y(), lightPos.z());
            }
        }
        
        // 提交渲染
        size_t queueSize = renderer->GetRenderQueueSize();
        renderer->FlushRenderQueue();
        
        // 显示加载进度（如果启用了异步加载）
        if (frameCount < 240 && g_loadingTotal > 0) {
            // ✅ 统计已加载完成的实体数量
            size_t loadedCount = 0;
            for (const auto& entity : entities) {
                if (world->HasComponent<MeshRenderComponent>(entity)) {
                    const auto& meshComp = world->GetComponent<MeshRenderComponent>(entity);
                    if (meshComp.resourcesLoaded && meshComp.mesh) {
                        loadedCount++;
                    }
                }
            }
            
            float progress = entities.empty() ? 100.0f : (float)loadedCount * 100.0f / entities.size();
            
            // 每10帧显示一次进度
            if (frameCount % 10 == 0) {
                size_t pending = asyncLoader.GetPendingTaskCount();
                size_t loading = asyncLoader.GetLoadingTaskCount();
                size_t waiting = asyncLoader.GetWaitingUploadCount();
                
                Logger::GetInstance().InfoFormat(
                    "[ECS Async Test] Frame %d: 加载进度 %.1f%% (%zu/%zu) | AsyncLoader: 待处理:%zu 加载中:%zu 等待上传:%zu | 渲染队列:%zu", 
                    frameCount, progress, 
                    loadedCount, entities.size(),
                    pending, loading, waiting, queueSize);
            }
            
            // 加载完成时显示
            if (frameCount > 0 && loadedCount == entities.size() && !g_allLoaded) {
                g_allLoaded = true;
                Logger::GetInstance().InfoFormat("[ECS Async Test] ========================================");
                Logger::GetInstance().InfoFormat("[ECS Async Test] 🎉 所有资源加载完成！（%zu个实体）", loadedCount);
                Logger::GetInstance().InfoFormat("[ECS Async Test] ========================================");
                asyncLoader.PrintStatistics();
            }
        }
        
        // 每60帧输出一次信息
        if (frameCount % 60 == 0 && frameCount > 0) {
            Logger::GetInstance().InfoFormat("[ECS Async Test] Frame %d: Queue: %zu objects", 
                         frameCount, queueSize);
        }
        
        // 结束渲染帧
        renderer->EndFrame();
        renderer->Present();
        
        frameCount++;
        
        // 简单的帧率限制
        SDL_Delay(16);  // ~60 FPS
    }
    
    Logger::GetInstance().InfoFormat("[ECS Async Test] Rendered %d frames", frameCount);
    
    // ============================================================
    // 9. 清理（注意顺序很重要！）
    // ============================================================
    
    Logger::GetInstance().InfoFormat("[ECS Async Test] ========================================");
    Logger::GetInstance().InfoFormat("[ECS Async Test] Starting safe shutdown sequence...");
    Logger::GetInstance().InfoFormat("[ECS Async Test] ========================================");
    
    // ✅ 步骤1: 等待所有异步加载任务完成
    Logger::GetInstance().InfoFormat("[ECS Async Test] Step 1: Waiting for async tasks to complete...");
    size_t pendingBefore = asyncLoader.GetPendingTaskCount();
    size_t loadingBefore = asyncLoader.GetLoadingTaskCount();
    size_t waitingBefore = asyncLoader.GetWaitingUploadCount();
    
    Logger::GetInstance().InfoFormat("[ECS Async Test]   Pending: %zu, Loading: %zu, Waiting Upload: %zu",
                         pendingBefore, loadingBefore, waitingBefore);
    
    if (pendingBefore > 0 || loadingBefore > 0 || waitingBefore > 0) {
        bool completed = asyncLoader.WaitForAll(5.0f);  // 最多等待5秒
        if (!completed) {
            Logger::GetInstance().WarningFormat("[ECS Async Test] Warning: Some async tasks did not complete in time");
        } else {
            Logger::GetInstance().InfoFormat("[ECS Async Test] All async loading tasks completed");
        }
    }
    
    // ✅ 步骤2: 处理所有已完成但未上传的任务（清空completedTasks队列）
    Logger::GetInstance().InfoFormat("[ECS Async Test] Step 2: Processing remaining completed tasks...");
    size_t remainingProcessed = asyncLoader.ProcessCompletedTasks(999999);  // 处理所有剩余任务
    if (remainingProcessed > 0) {
        Logger::GetInstance().InfoFormat("[ECS Async Test]   Processed %zu remaining tasks", remainingProcessed);
    }
    
    // ✅ 步骤3: 关闭AsyncResourceLoader（等待工作线程退出）
    Logger::GetInstance().InfoFormat("[ECS Async Test] Step 3: Shutting down AsyncResourceLoader...");
    asyncLoader.Shutdown();
    Logger::GetInstance().InfoFormat("[ECS Async Test]   AsyncResourceLoader shutdown complete");
    
    // ✅ 步骤4: 关闭World（此时不会有新的异步回调）
    Logger::GetInstance().InfoFormat("[ECS Async Test] Step 4: Shutting down World...");
    world->Shutdown();
    Logger::GetInstance().InfoFormat("[ECS Async Test]   World shutdown complete");
    
    // 释放World的shared_ptr引用
    Logger::GetInstance().InfoFormat("[ECS Async Test]   Releasing World shared_ptr (use_count: %ld)", world.use_count());
    world.reset();
    Logger::GetInstance().InfoFormat("[ECS Async Test]   World destroyed");
    
    // ✅ 步骤5: 关闭Renderer
    Logger::GetInstance().InfoFormat("[ECS Async Test] Step 5: Shutting down Renderer...");
    renderer->Shutdown();
    Logger::GetInstance().InfoFormat("[ECS Async Test]   Renderer shutdown complete");
    
    Logger::GetInstance().InfoFormat("[ECS Async Test] ========================================");
    Logger::GetInstance().InfoFormat("[ECS Async Test] === Test Completed Successfully ===");
    Logger::GetInstance().InfoFormat("[ECS Async Test] ========================================");
    
    return 0;
}

