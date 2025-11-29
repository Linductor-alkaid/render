/**
 * @file 59_lod_instanced_rendering_test.cpp
 * @brief LOD 实例化渲染测试 - 测试阶段2.2和阶段2.3的LOD实例化渲染功能
 * 
 * 测试内容：
 * 1. 创建大量相同网格的实体（用于测试实例化）
 * 2. 配置LOD组件，自动计算LOD级别
 * 3. 启用LOD实例化渲染，验证批量渲染效果
 * 4. 对比启用/禁用实例化渲染的性能差异
 * 
 * 阶段2.3新增测试：
 * 5. 测试Renderer级别的LOD实例化设置
 * 6. 测试LOD实例化统计信息获取
 * 7. 测试兼容性检查
 * 8. 测试与批处理模式的交互
 */

#include <render/renderer.h>
#include <render/logger.h>
#include <render/shader_cache.h>
#include <render/model_loader.h>
#include <render/mesh_loader.h>
#include <render/resource_manager.h>
#include <render/ecs/world.h>
#include <render/ecs/components.h>
#include <render/ecs/systems.h>
#include <render/lod_generator.h>
#include <render/lod_system.h>
#include <render/math_utils.h>
#include <render/geometry_preset.h>

#include <SDL3/SDL.h>
#include <glad/glad.h>

#include <algorithm>
#include <memory>
#include <vector>
#include <random>

using namespace Render;
using namespace Render::ECS;

namespace {

struct SceneConfig {
    Vector3 cameraPosition{0.0f, 10.0f, 20.0f};
    Vector3 cameraTarget{0.0f, 0.0f, 0.0f};
    Vector3 lightPosition{5.0f, 10.0f, 5.0f};
    Color ambientColor{0.2f, 0.2f, 0.25f, 1.0f};
    Color diffuseColor{1.0f, 1.0f, 1.0f, 1.0f};
    Color specularColor{0.6f, 0.6f, 0.6f, 1.0f};
    float shininess = 48.0f;
    
    // 实例化测试配置
    int instanceCount = 100;  // 实例数量
    float gridSize = 30.0f;   // 网格大小（增大以确保可见）
    bool enableInstancing = true;  // 是否启用实例化渲染
    
    // 阶段2.3：批处理模式测试
    BatchingMode batchingMode = BatchingMode::GpuInstancing;  // 默认使用GPU实例化
};

} // namespace

int main(int argc, char* argv[]) {

    Logger::GetInstance().SetLogToFile(true);
    
    (void)argc;
    (void)argv;

    Logger::GetInstance().Info("[LODInstancedRenderingTest] === LOD Instanced Rendering Test ===");

    // 解析命令行参数
    SceneConfig sceneConfig{};
    if (argc > 1) {
        sceneConfig.instanceCount = std::atoi(argv[1]);
        if (sceneConfig.instanceCount <= 0) {
            sceneConfig.instanceCount = 100;
        }
    }
    if (argc > 2) {
        sceneConfig.enableInstancing = (std::atoi(argv[2]) != 0);
    }

    Logger::GetInstance().InfoFormat(
        "[LODInstancedRenderingTest] Configuration: instances=%d, instancing=%s",
        sceneConfig.instanceCount,
        sceneConfig.enableInstancing ? "enabled" : "disabled"
    );
    
    // 阶段2.3：测试Renderer级别的LOD实例化设置
    Logger::GetInstance().Info("[LODInstancedRenderingTest] === Phase 2.3: Testing Renderer-level LOD Instancing ===");

    Renderer* renderer = Renderer::Create();
    if (!renderer->Initialize("LOD Instanced Rendering Test", 1600, 900)) {
        Logger::GetInstance().Error("[LODInstancedRenderingTest] Failed to initialize renderer");
        return -1;
    }
    renderer->SetClearColor(Color(0.05f, 0.05f, 0.1f, 1.0f));  // 深蓝色背景，与红色网格形成对比
    renderer->SetVSync(true);
    
    // 阶段2.3：设置批处理模式
    renderer->SetBatchingMode(sceneConfig.batchingMode);
    Logger::GetInstance().InfoFormat(
        "[LODInstancedRenderingTest] Batching mode set to: %d (0=Disabled, 1=CpuMerge, 2=GpuInstancing)",
        static_cast<int>(sceneConfig.batchingMode)
    );
    
    // 阶段2.3：测试Renderer级别的LOD实例化设置
    renderer->SetLODInstancingEnabled(sceneConfig.enableInstancing);
    Logger::GetInstance().InfoFormat(
        "[LODInstancedRenderingTest] Renderer LOD instancing: %s",
        renderer->IsLODInstancingEnabled() ? "enabled" : "disabled"
    );
    
    // 阶段2.3：测试兼容性检查
    bool lodInstancingAvailable = renderer->IsLODInstancingAvailable();
    Logger::GetInstance().InfoFormat(
        "[LODInstancedRenderingTest] LOD instancing available: %s",
        lodInstancingAvailable ? "yes" : "no"
    );
    
    if (auto context = renderer->GetContext()) {
        SDL_SetWindowRelativeMouseMode(context->GetWindow(), true);
    }

    auto& shaderCache = ShaderCache::GetInstance();
    auto phongShader = shaderCache.LoadShader(
        "material_phong",
        "shaders/material_phong.vert",
        "shaders/material_phong.frag"
    );
    if (!phongShader || !phongShader->IsValid()) {
        Logger::GetInstance().Error("[LODInstancedRenderingTest] Failed to load Phong shader");
        Renderer::Destroy(renderer);
        return -1;
    }

    // 获取测试网格（使用球体）
    Logger::GetInstance().Info("[LODInstancedRenderingTest] Getting test mesh (sphere)...");
    auto& resourceManager = ResourceManager::GetInstance();
    auto sourceMesh = GeometryPreset::GetMesh(resourceManager, "geometry::sphere");
    if (!sourceMesh) {
        Logger::GetInstance().Error("[LODInstancedRenderingTest] Failed to get sphere mesh from GeometryPreset");
        Renderer::Destroy(renderer);
        return -1;
    }

    Logger::GetInstance().InfoFormat(
        "[LODInstancedRenderingTest] Source mesh: %zu vertices, %zu triangles",
        sourceMesh->GetVertexCount(),
        sourceMesh->GetTriangleCount()
    );

    // 生成 LOD 级别
    Logger::GetInstance().Info("[LODInstancedRenderingTest] Generating LOD levels...");
    auto lodOptions = LODGenerator::GetRecommendedOptions(sourceMesh);
    auto lodMeshes = LODGenerator::GenerateLODLevels(sourceMesh, lodOptions);
    
    if (lodMeshes.empty() || !lodMeshes[0] || !lodMeshes[1] || !lodMeshes[2]) {
        Logger::GetInstance().Error("[LODInstancedRenderingTest] Failed to generate LOD levels");
        Renderer::Destroy(renderer);
        return -1;
    }

    // 打印 LOD 统计信息
    std::vector<Ref<Mesh>> allLODMeshes(4);
    allLODMeshes[0] = sourceMesh;  // LOD0
    allLODMeshes[1] = lodMeshes[0];  // LOD1
    allLODMeshes[2] = lodMeshes[1];  // LOD2
    allLODMeshes[3] = lodMeshes[2];  // LOD3

    for (int lod = 0; lod <= 3; ++lod) {
        if (allLODMeshes[lod]) {
            Logger::GetInstance().InfoFormat(
                "[LODInstancedRenderingTest] LOD%d: %zu vertices, %zu triangles",
                lod,
                allLODMeshes[lod]->GetVertexCount(),
                allLODMeshes[lod]->GetTriangleCount()
            );
            
            // 确保网格已上传到GPU
            if (!allLODMeshes[lod]->IsUploaded()) {
                Logger::GetInstance().InfoFormat("[LODInstancedRenderingTest] Uploading LOD%d mesh to GPU...", lod);
                allLODMeshes[lod]->Upload();
            }
        }
    }

    // 创建材质（使用更鲜艳的颜色，确保可见）
    auto material = std::make_shared<Material>();
    material->SetShader(phongShader);
    material->SetDiffuseColor(Color(0.8f, 0.3f, 0.3f, 1.0f));  // 红色，更容易看到
    material->SetAmbientColor(Color(0.3f, 0.1f, 0.1f, 1.0f));
    material->SetSpecularColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
    material->SetShininess(64.0f);

    // 创建 ECS World
    auto world = std::make_shared<World>();
    world->Initialize();

    world->RegisterComponent<TransformComponent>();
    world->RegisterComponent<MeshRenderComponent>();
    world->RegisterComponent<LODComponent>();
    world->RegisterComponent<CameraComponent>();
    world->RegisterComponent<NameComponent>();
    world->RegisterComponent<ActiveComponent>();

    world->RegisterSystem<TransformSystem>();
    world->RegisterSystem<CameraSystem>();
    world->RegisterSystem<UniformSystem>(renderer);
    
    // 注册 MeshRenderSystem
    world->RegisterSystem<MeshRenderSystem>(renderer);

    world->PostInitialize();
    
    // 获取 MeshRenderSystem
    auto* meshRenderSystem = world->GetSystem<MeshRenderSystem>();
    
    // 阶段2.3：MeshRenderSystem现在从Renderer获取设置，不需要单独设置
    // 但为了向后兼容，仍然可以调用（会同步到Renderer）
    if (meshRenderSystem) {
        // 验证MeshRenderSystem的设置与Renderer一致
        bool meshSystemEnabled = meshRenderSystem->IsLODInstancingEnabled();
        bool rendererEnabled = renderer->IsLODInstancingEnabled();
        if (meshSystemEnabled != rendererEnabled) {
            Logger::GetInstance().WarningFormat(
                "[LODInstancedRenderingTest] Warning: MeshRenderSystem and Renderer settings mismatch! "
                "MeshSystem=%s, Renderer=%s",
                meshSystemEnabled ? "enabled" : "disabled",
                rendererEnabled ? "enabled" : "disabled"
            );
        } else {
            Logger::GetInstance().InfoFormat(
                "[LODInstancedRenderingTest] MeshRenderSystem and Renderer settings synchronized: %s",
                meshSystemEnabled ? "enabled" : "disabled"
            );
        }
    }

    // 创建相机（使用与58测试相同的初始化方式）
    EntityID cameraEntity = world->CreateEntity({ .name = "MainCamera", .active = true });
    TransformComponent cameraTransform;
    cameraTransform.SetPosition(sceneConfig.cameraPosition);
    // 计算初始相机朝向（与58测试一致）
    Vector3 toTargetInit = (sceneConfig.cameraTarget - sceneConfig.cameraPosition).normalized();
    float cameraYawInit = MathUtils::RadiansToDegrees(std::atan2(toTargetInit.x(), -toTargetInit.z()));
    float cameraPitchInit = MathUtils::RadiansToDegrees(std::asin(std::clamp(toTargetInit.y(), -1.0f, 1.0f)));
    Quaternion yawRotation = MathUtils::AngleAxis(MathUtils::DegreesToRadians(cameraYawInit), Vector3::UnitY());
    Quaternion pitchRotation = MathUtils::AngleAxis(MathUtils::DegreesToRadians(cameraPitchInit), Vector3::UnitX());
    cameraTransform.SetRotation(yawRotation * pitchRotation);
    world->AddComponent(cameraEntity, cameraTransform);

    CameraComponent cameraComp;
    cameraComp.camera = std::make_shared<Camera>();
    cameraComp.camera->SetPerspective(55.0f, static_cast<float>(renderer->GetWidth()) / renderer->GetHeight(), 0.1f, 200.0f);
    cameraComp.active = true;
    world->AddComponent(cameraEntity, cameraComp);

    // 创建大量实例（网格排列）
    Logger::GetInstance().InfoFormat(
        "[LODInstancedRenderingTest] Creating %d instances in a grid...",
        sceneConfig.instanceCount
    );

    std::vector<EntityID> instanceEntities;
    instanceEntities.reserve(sceneConfig.instanceCount);

    int gridWidth = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(sceneConfig.instanceCount))));
    float spacing = sceneConfig.gridSize / static_cast<float>(gridWidth);
    float startX = -sceneConfig.gridSize * 0.5f;
    float startZ = -sceneConfig.gridSize * 0.5f;
    
    Logger::GetInstance().InfoFormat(
        "[LODInstancedRenderingTest] Grid layout: %dx%d, spacing=%.2f, size=%.2f",
        gridWidth, gridWidth, spacing, sceneConfig.gridSize
    );

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> scaleDist(0.5f, 1.5f);
    std::uniform_real_distribution<float> rotDist(0.0f, 360.0f);

    for (int i = 0; i < sceneConfig.instanceCount; ++i) {
        EntityID entity = world->CreateEntity({ 
            .name = "Instance_" + std::to_string(i), 
            .active = true 
        });

        // 计算网格位置
        int row = i / gridWidth;
        int col = i % gridWidth;
        float x = startX + static_cast<float>(col) * spacing;
        float z = startZ + static_cast<float>(row) * spacing;
        float y = 0.0f;

        // 设置变换
        TransformComponent transform;
        transform.SetPosition(Vector3(x, y, z));
        transform.SetRotation(MathUtils::FromEulerDegrees(0.0f, rotDist(gen), 0.0f));
        // 增大缩放，确保网格可见（球体默认半径约1.0，放大3倍）
        float scale = scaleDist(gen) * 3.0f;
        transform.SetScale(scale);
        world->AddComponent(entity, transform);

        // 设置网格渲染组件
        MeshRenderComponent meshComp;
        meshComp.mesh = sourceMesh;  // 使用原始网格
        meshComp.material = material;
        meshComp.visible = true;  // 确保可见
        meshComp.layerID = 0;
        meshComp.castShadows = true;
        meshComp.receiveShadows = true;
        meshComp.resourcesLoaded = true;
        world->AddComponent(entity, meshComp);

        // 配置 LOD 组件
        LODComponent lodComp;
        lodComp.config.enabled = true;
        // ✅ 调整距离阈值：LOD0范围应该更大，确保靠近球体时能显示LOD0
        // 相机初始位置约(0, 10, 20)，到原点的距离约22.36
        // 增大LOD0阈值到50，确保在相机初始位置附近的球体能显示LOD0
        lodComp.config.distanceThresholds = {50.0f, 100.0f, 150.0f, 200.0f};
        lodComp.config.transitionDistance = 5.0f;
        
        // 配置完整的 LOD 网格列表
        lodComp.config.lodMeshes.resize(4);
        lodComp.config.lodMeshes[0] = allLODMeshes[0];  // LOD0
        lodComp.config.lodMeshes[1] = allLODMeshes[1];  // LOD1
        lodComp.config.lodMeshes[2] = allLODMeshes[2];  // LOD2
        lodComp.config.lodMeshes[3] = allLODMeshes[3];  // LOD3
        
        world->AddComponent(entity, lodComp);

        instanceEntities.push_back(entity);
    }

    Logger::GetInstance().InfoFormat(
        "[LODInstancedRenderingTest] All %zu instances created successfully",
        instanceEntities.size()
    );
    
    // 验证网格和材质
    if (sourceMesh && sourceMesh->GetVertexCount() > 0) {
        Logger::GetInstance().InfoFormat(
            "[LODInstancedRenderingTest] Source mesh verified: %zu vertices, %zu triangles",
            sourceMesh->GetVertexCount(),
            sourceMesh->GetTriangleCount()
        );
    } else {
        Logger::GetInstance().Error("[LODInstancedRenderingTest] Source mesh is invalid!");
    }
    
    if (material && material->IsValid()) {
        Logger::GetInstance().Info("[LODInstancedRenderingTest] Material verified: valid");
    } else {
        Logger::GetInstance().Error("[LODInstancedRenderingTest] Material is invalid!");
    }
    
    Logger::GetInstance().Info("[LODInstancedRenderingTest] Controls: ESC to exit");
    Logger::GetInstance().Info("[LODInstancedRenderingTest] Controls: WASD 前后左右, Q/E 上下, Shift 加速");
    Logger::GetInstance().Info("[LODInstancedRenderingTest] Controls: Tab 捕获/释放鼠标, I 切换实例化渲染");
    Logger::GetInstance().Info("[LODInstancedRenderingTest] Controls: B 切换批处理模式 (阶段2.3)");

    bool running = true;
    Uint64 prevTicks = SDL_GetTicks();
    float accumTime = 0.0f;
    Vector3 cameraPosition = sceneConfig.cameraPosition;
    Vector3 toTarget = (sceneConfig.cameraTarget - sceneConfig.cameraPosition).normalized();
    float cameraYaw = MathUtils::RadiansToDegrees(std::atan2(toTarget.x(), -toTarget.z()));
    float cameraPitch = MathUtils::RadiansToDegrees(std::asin(std::clamp(toTarget.y(), -1.0f, 1.0f)));
    bool mouseCaptured = true;

    // 性能统计
    struct PerformanceStats {
        float frameTime = 0.0f;
        float avgFrameTime = 0.0f;
        int frameCount = 0;
        size_t drawCalls = 0;
        size_t visibleMeshes = 0;
    } perfStats;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
                running = false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_TAB) {
                mouseCaptured = !mouseCaptured;
                if (auto context = renderer->GetContext()) {
                    SDL_SetWindowRelativeMouseMode(context->GetWindow(), mouseCaptured);
                }
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_I) {
                // 阶段2.3：使用Renderer级别的设置
                sceneConfig.enableInstancing = !sceneConfig.enableInstancing;
                renderer->SetLODInstancingEnabled(sceneConfig.enableInstancing);
                
                // 验证MeshRenderSystem同步了设置
                bool meshSystemEnabled = meshRenderSystem ? meshRenderSystem->IsLODInstancingEnabled() : false;
                bool rendererEnabled = renderer->IsLODInstancingEnabled();
                
                Logger::GetInstance().InfoFormat(
                    "[LODInstancedRenderingTest] LOD Instancing %s (Renderer: %s, MeshSystem: %s, Available: %s)",
                    sceneConfig.enableInstancing ? "enabled" : "disabled",
                    rendererEnabled ? "enabled" : "disabled",
                    meshSystemEnabled ? "enabled" : "disabled",
                    renderer->IsLODInstancingAvailable() ? "yes" : "no"
                );
            }
            
            // 阶段2.3：切换批处理模式
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_B) {
                // 循环切换批处理模式
                switch (sceneConfig.batchingMode) {
                    case BatchingMode::Disabled:
                        sceneConfig.batchingMode = BatchingMode::CpuMerge;
                        break;
                    case BatchingMode::CpuMerge:
                        sceneConfig.batchingMode = BatchingMode::GpuInstancing;
                        break;
                    case BatchingMode::GpuInstancing:
                        sceneConfig.batchingMode = BatchingMode::Disabled;
                        break;
                }
                renderer->SetBatchingMode(sceneConfig.batchingMode);
                
                // 检查LOD实例化是否仍然可用
                bool lodInstancingAvailable = renderer->IsLODInstancingAvailable();
                
                Logger::GetInstance().InfoFormat(
                    "[LODInstancedRenderingTest] Batching mode: %d (0=Disabled, 1=CpuMerge, 2=GpuInstancing), "
                    "LOD Instancing available: %s",
                    static_cast<int>(sceneConfig.batchingMode),
                    lodInstancingAvailable ? "yes" : "no"
                );
            }
            if (mouseCaptured && event.type == SDL_EVENT_MOUSE_MOTION) {
                constexpr float sensitivity = 0.15f;
                cameraYaw -= static_cast<float>(event.motion.xrel) * sensitivity;
                cameraPitch -= static_cast<float>(event.motion.yrel) * sensitivity;
                cameraPitch = std::clamp(cameraPitch, -89.0f, 89.0f);
            }
        }

        Uint64 currentTicks = SDL_GetTicks();
        float deltaTime = static_cast<float>(currentTicks - prevTicks) / 1000.0f;
        prevTicks = currentTicks;
        deltaTime = std::min(deltaTime, 0.033f);
        accumTime += deltaTime;

        perfStats.frameTime = deltaTime;
        perfStats.frameCount++;
        perfStats.avgFrameTime = (perfStats.avgFrameTime * (perfStats.frameCount - 1) + deltaTime) / perfStats.frameCount;

        const bool* keyboard = SDL_GetKeyboardState(nullptr);
        float moveSpeed = (keyboard[SDL_SCANCODE_LSHIFT] ? 6.0f : 3.0f) * deltaTime;

        const float yawRad = MathUtils::DegreesToRadians(cameraYaw);
        const float pitchRad = MathUtils::DegreesToRadians(cameraPitch);

        Quaternion yawRotation = MathUtils::AngleAxis(yawRad, Vector3::UnitY());
        Quaternion pitchRotation = MathUtils::AngleAxis(pitchRad, Vector3::UnitX());
        Quaternion viewRotation = yawRotation * pitchRotation;

        Vector3 front = viewRotation * (-Vector3::UnitZ());
        front.normalize();
        Vector3 right = front.cross(Vector3::UnitY()).normalized();
        Vector3 up = right.cross(front).normalized();

        if (keyboard[SDL_SCANCODE_W]) cameraPosition += front * moveSpeed;
        if (keyboard[SDL_SCANCODE_S]) cameraPosition -= front * moveSpeed;
        if (keyboard[SDL_SCANCODE_A]) cameraPosition -= right * moveSpeed;
        if (keyboard[SDL_SCANCODE_D]) cameraPosition += right * moveSpeed;
        if (keyboard[SDL_SCANCODE_Q]) cameraPosition -= Vector3::UnitY() * moveSpeed;
        if (keyboard[SDL_SCANCODE_E]) cameraPosition += Vector3::UnitY() * moveSpeed;

        auto& cameraTransformComp = world->GetComponent<TransformComponent>(cameraEntity);
        cameraTransformComp.SetPosition(cameraPosition);
        cameraTransformComp.SetRotation(viewRotation);

        // 让所有实例旋转（可选，用于测试）
        // 如果禁用旋转后条纹消失，说明问题与旋转+法线变换有关
        // 如果仍然存在，说明问题与旋转无关
        bool enableInstanceRotation = false;  // 设置为 false 禁用旋转
        if (enableInstanceRotation) {
            for (auto entity : instanceEntities) {
                auto& transform = world->GetComponent<TransformComponent>(entity);
                Quaternion baseRotation = transform.GetRotation();
                Quaternion spin = MathUtils::FromEulerDegrees(0.0f, accumTime * 30.0f, 0.0f);
                transform.SetRotation(baseRotation * spin);
            }
        }

        renderer->BeginFrame();
        renderer->Clear();

        if (auto uniformMgr = phongShader->GetUniformManager()) {
            uniformMgr->SetVector3("uLightPos", sceneConfig.lightPosition);
            uniformMgr->SetColor("uAmbientColor", sceneConfig.ambientColor);
            uniformMgr->SetColor("uDiffuseColor", sceneConfig.diffuseColor);
            uniformMgr->SetColor("uSpecularColor", sceneConfig.specularColor);
            uniformMgr->SetFloat("uShininess", sceneConfig.shininess);
            if (uniformMgr->HasUniform("uUseVertexColor")) {
                uniformMgr->SetBool("uUseVertexColor", false);
            }
        }

        // 输出相机位置（第一帧）
        if (perfStats.frameCount == 1) {
            Logger::GetInstance().InfoFormat(
                "[LODInstancedRenderingTest] Camera position: (%.2f, %.2f, %.2f), target: (%.2f, %.2f, %.2f)",
                cameraPosition.x(), cameraPosition.y(), cameraPosition.z(),
                sceneConfig.cameraTarget.x(), sceneConfig.cameraTarget.y(), sceneConfig.cameraTarget.z()
            );
        }
        
        world->Update(deltaTime);
        renderer->FlushRenderQueue();

        // 获取统计信息
        const auto& stats = meshRenderSystem->GetStats();
        perfStats.drawCalls = stats.drawCalls;
        perfStats.visibleMeshes = stats.visibleMeshes;
        
        // 阶段2.3：获取Renderer级别的LOD实例化统计信息
        auto lodInstancingStats = renderer->GetLODInstancingStats();

        // 前10帧或每60帧输出一次性能统计
        if (perfStats.frameCount <= 10 || perfStats.frameCount % 60 == 0) {
            float fps = 1.0f / perfStats.avgFrameTime;
            
            // 阶段2.3：输出Renderer级别的LOD实例化统计信息
            Logger::GetInstance().InfoFormat(
                "[LODInstancedRenderingTest] Frame %d | FPS: %.1f | Frame: %.3fms | "
                "Draw Calls: %zu | Visible: %zu | Culled: %zu | "
                "LOD: enabled=%zu, LOD0=%zu, LOD1=%zu, LOD2=%zu, LOD3=%zu, culled=%zu | "
                "Instancing: %s | Batching: %d | "
                "LOD Stats (Renderer): groups=%zu, instances=%zu, drawCalls=%zu",
                perfStats.frameCount,
                fps,
                perfStats.avgFrameTime * 1000.0f,
                perfStats.drawCalls,
                perfStats.visibleMeshes,
                stats.culledMeshes,
                stats.lodEnabledEntities,
                stats.lod0Count,
                stats.lod1Count,
                stats.lod2Count,
                stats.lod3Count,
                stats.lodCulledCount,
                sceneConfig.enableInstancing ? "ON" : "OFF",
                static_cast<int>(renderer->GetBatchingMode()),
                lodInstancingStats.lodGroupCount,
                lodInstancingStats.totalInstances,
                lodInstancingStats.drawCalls
            );
            
            // 第一帧输出详细信息
            if (perfStats.frameCount == 1) {
                Logger::GetInstance().InfoFormat(
                    "[LODInstancedRenderingTest] First frame stats: "
                    "Total entities: %zu, Visible: %zu, Culled: %zu, Draw Calls: %zu",
                    instanceEntities.size(),
                    perfStats.visibleMeshes,
                    stats.culledMeshes,
                    perfStats.drawCalls
                );
                
                // 阶段2.3：输出Renderer级别的LOD实例化详细统计
                Logger::GetInstance().InfoFormat(
                    "[LODInstancedRenderingTest] Phase 2.3 - Renderer LOD Instancing Stats: "
                    "Groups=%zu, Total Instances=%zu, Draw Calls=%zu, "
                    "LOD0=%zu, LOD1=%zu, LOD2=%zu, LOD3=%zu, Culled=%zu",
                    lodInstancingStats.lodGroupCount,
                    lodInstancingStats.totalInstances,
                    lodInstancingStats.drawCalls,
                    lodInstancingStats.lod0Instances,
                    lodInstancingStats.lod1Instances,
                    lodInstancingStats.lod2Instances,
                    lodInstancingStats.lod3Instances,
                    lodInstancingStats.culledCount
                );
                
                // 阶段2.3：验证兼容性
                bool lodInstancingAvailable = renderer->IsLODInstancingAvailable();
                Logger::GetInstance().InfoFormat(
                    "[LODInstancedRenderingTest] Phase 2.3 - Compatibility Check: "
                    "LOD Instancing Enabled=%s, Available=%s, Batching Mode=%d",
                    renderer->IsLODInstancingEnabled() ? "yes" : "no",
                    lodInstancingAvailable ? "yes" : "no",
                    static_cast<int>(renderer->GetBatchingMode())
                );
                
                if (perfStats.visibleMeshes == 0 && stats.culledMeshes == 0) {
                    Logger::GetInstance().Warning(
                        "[LODInstancedRenderingTest] No meshes rendered! "
                        "Check if entities are properly created and visible."
                    );
                }
            }
        }

        renderer->EndFrame();
        renderer->Present();

        SDL_Delay(1);
    }

    world->Shutdown();
    Renderer::Destroy(renderer);

    Logger::GetInstance().Info("[LODInstancedRenderingTest] Shutdown complete");
    return 0;
}

