/**
 * @file 57_toolchain_integration_test.cpp
 * @brief 工具链集成测试 - 测试工具链数据源接口
 * 
 * 测试功能：
 * 1. ModuleRegistry 模块状态查询接口
 * 2. MaterialShaderPanelDataSource 材质/Shader面板数据源
 * 3. LayerMaskEditorDataSource LayerMask编辑器集成
 * 4. SceneGraphVisualizerDataSource 场景图可视化工具
 */

#include "render/application/application_host.h"
#include "render/application/modules/core_render_module.h"
#include "render/application/modules/debug_hud_module.h"
#include "render/application/modules/input_module.h"
#include "render/application/toolchain/material_shader_panel.h"
#include "render/application/toolchain/layermask_editor.h"
#include "render/application/toolchain/scene_graph_visualizer.h"
#include "render/application/scenes/boot_scene.h"
#include "render/application/app_context.h"
#include "render/async_resource_loader.h"
#include "render/logger.h"
#include "render/renderer.h"
#include "render/resource_manager.h"
#include "render/material.h"
#include "render/shader.h"
#include "render/mesh.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <iomanip>

using namespace Render;
using namespace Render::Application;

namespace {

void ConfigureLogger() {
    auto& logger = Logger::GetInstance();
    logger.SetLogToConsole(true);
    logger.SetLogToFile(false);
    logger.SetLogLevel(LogLevel::Info);
}

void PrintSeparator() {
    std::cout << "========================================" << std::endl;
}

// ========================================================================
// 测试 1: ModuleRegistry 模块状态查询
// ========================================================================
void TestModuleRegistryStatus(ApplicationHost& host) {
    PrintSeparator();
    std::cout << "Test 1: ModuleRegistry Module Status Query" << std::endl;
    PrintSeparator();
    
    auto& moduleRegistry = host.GetModuleRegistry();
    
    // 获取所有模块状态
    auto allStates = moduleRegistry.GetAllModuleStates();
    std::cout << "Total modules: " << allStates.size() << std::endl;
    std::cout << std::endl;
    
    for (const auto& state : allStates) {
        std::cout << "Module: " << state.name << std::endl;
        std::cout << "  Active: " << (state.active ? "Yes" : "No") << std::endl;
        std::cout << "  Registered: " << (state.registered ? "Yes" : "No") << std::endl;
        std::cout << "  PreFrame Priority: " << state.preFramePriority << std::endl;
        std::cout << "  PostFrame Priority: " << state.postFramePriority << std::endl;
        
        if (!state.dependencies.empty()) {
            std::cout << "  Dependencies: ";
            for (size_t i = 0; i < state.dependencies.size(); ++i) {
                std::cout << state.dependencies[i];
                if (i + 1 < state.dependencies.size()) {
                    std::cout << ", ";
                }
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
    }
    
    // 测试单个模块查询
    auto coreState = moduleRegistry.GetModuleState("CoreRenderModule");
    if (coreState.has_value()) {
        std::cout << "✓ Successfully queried CoreRenderModule state" << std::endl;
    } else {
        std::cout << "✗ Failed to query CoreRenderModule state" << std::endl;
    }
    
    // 测试模块状态检查
    bool isActive = moduleRegistry.IsModuleActive("CoreRenderModule");
    bool isRegistered = moduleRegistry.IsModuleRegistered("CoreRenderModule");
    std::cout << "CoreRenderModule - Active: " << (isActive ? "Yes" : "No")
              << ", Registered: " << (isRegistered ? "Yes" : "No") << std::endl;
    std::cout << std::endl;
}

// ========================================================================
// 测试 2: MaterialShaderPanelDataSource 材质/Shader面板数据源
// ========================================================================
void TestMaterialShaderPanelDataSource() {
    PrintSeparator();
    std::cout << "Test 2: MaterialShaderPanelDataSource" << std::endl;
    PrintSeparator();
    
    auto& resourceManager = ResourceManager::GetInstance();
    
    // 创建一些测试资源
    // 注意：这里只是演示接口，实际项目中资源应该通过加载器加载
    
    // 创建数据源
    MaterialShaderPanelDataSource dataSource(resourceManager);
    
    // 测试材质列表
    auto materialNames = dataSource.GetMaterialNames();
    std::cout << "Materials (" << materialNames.size() << "):" << std::endl;
    for (const auto& name : materialNames) {
        auto info = dataSource.GetMaterialInfo(name);
        if (info.has_value()) {
            std::cout << "  " << name << std::endl;
            std::cout << "    Shader: " << info->shaderName << std::endl;
            std::cout << "    Metallic: " << info->metallic << std::endl;
            std::cout << "    Roughness: " << info->roughness << std::endl;
        }
    }
    std::cout << std::endl;
    
    // 测试着色器列表
    auto shaderNames = dataSource.GetShaderNames();
    std::cout << "Shaders (" << shaderNames.size() << "):" << std::endl;
    for (const auto& name : shaderNames) {
        auto info = dataSource.GetShaderInfo(name);
        if (info.has_value()) {
            std::cout << "  " << name << std::endl;
            std::cout << "    Program ID: " << info->programID << std::endl;
            std::cout << "    Uniforms: " << info->uniforms.size() << std::endl;
            
            // 显示前5个Uniform
            size_t count = std::min(size_t(5), info->uniforms.size());
            for (size_t i = 0; i < count; ++i) {
                std::cout << "      - " << info->uniforms[i].name 
                         << " (location: " << info->uniforms[i].location << ")" << std::endl;
            }
            if (info->uniforms.size() > 5) {
                std::cout << "      ... (" << (info->uniforms.size() - 5) << " more)" << std::endl;
            }
        }
    }
    std::cout << std::endl;
    
    // 测试遍历接口
    std::cout << "Testing ForEachMaterial..." << std::endl;
    size_t materialCount = 0;
    dataSource.ForEachMaterial([&](const std::string& name, const MaterialInfo& info) {
        materialCount++;
        if (materialCount <= 3) {
            std::cout << "  " << name << " - Shader: " << info.shaderName << std::endl;
        }
    });
    std::cout << "Total materials processed: " << materialCount << std::endl;
    std::cout << std::endl;
}

// ========================================================================
// 测试 3: LayerMaskEditorDataSource LayerMask编辑器集成
// ========================================================================
void TestLayerMaskEditorDataSource(Renderer& renderer) {
    PrintSeparator();
    std::cout << "Test 3: LayerMaskEditorDataSource" << std::endl;
    PrintSeparator();
    
    // 获取RenderLayerRegistry
    auto& layerRegistry = renderer.GetLayerRegistry();
    
    // 创建数据源
    LayerMaskEditorDataSource dataSource(layerRegistry);
    
    // 获取所有层级
    auto allLayers = dataSource.GetAllLayers();
    std::cout << "Registered Layers (" << allLayers.size() << "):" << std::endl;
    for (const auto& record : allLayers) {
        std::cout << "  " << record.descriptor.name << std::endl;
        std::cout << "    ID: " << record.descriptor.id.value << std::endl;
        std::cout << "    Priority: " << record.descriptor.priority << std::endl;
        std::cout << "    Mask Index: " << static_cast<int>(record.descriptor.maskIndex) << std::endl;
        std::cout << "    Enabled: " << (record.state.enabled ? "Yes" : "No") << std::endl;
    }
    std::cout << std::endl;
    
    // 测试LayerMask操作
    uint32_t testMask = dataSource.CreateFullMask();
    std::cout << "Full Mask: 0x" << std::hex << std::setw(8) << std::setfill('0') 
              << testMask << std::dec << std::endl;
    
    uint32_t emptyMask = dataSource.CreateEmptyMask();
    std::cout << "Empty Mask: 0x" << std::hex << std::setw(8) << std::setfill('0') 
              << emptyMask << std::dec << std::endl;
    
    // 测试LayerMask到层级列表的转换
    if (!allLayers.empty()) {
        auto firstLayer = allLayers[0].descriptor.id;
        testMask = dataSource.SetLayerInMask(emptyMask, firstLayer, true);
        auto enabledLayers = dataSource.LayerMaskToLayers(testMask);
        std::cout << "Mask with first layer enabled: " << enabledLayers.size() << " layers" << std::endl;
        
        // 验证
        bool isInMask = dataSource.IsLayerInMask(testMask, firstLayer);
        std::cout << "First layer in mask: " << (isInMask ? "Yes" : "No") << std::endl;
    }
    
    // 测试LayerMask验证
    bool isValid = dataSource.ValidateLayerMask(testMask);
    std::cout << "Test mask is valid: " << (isValid ? "Yes" : "No") << std::endl;
    std::cout << std::endl;
}

// ========================================================================
// 测试 4: SceneGraphVisualizerDataSource 场景图可视化工具
// ========================================================================
void TestSceneGraphVisualizerDataSource(SceneManager& sceneManager) {
    PrintSeparator();
    std::cout << "Test 4: SceneGraphVisualizerDataSource" << std::endl;
    PrintSeparator();
    
    // 创建可视化器
    SceneGraphVisualizerDataSource visualizer;
    
    // 获取当前活动场景
    auto* activeScene = sceneManager.GetActiveScene();
    if (activeScene) {
        std::cout << "Active scene: " << activeScene->Name() << std::endl;
        
        // 注意：由于Scene基类没有直接暴露SceneGraph的方法，
        // 这里只能测试空场景图的情况
        // 在实际应用中，可以通过类型转换访问具体场景类的SceneGraph成员
        
        // 测试空场景图
        if (visualizer.IsEmpty()) {
            std::cout << "Scene graph is empty (not set, as expected)" << std::endl;
        }
        
        // 获取统计信息
        auto stats = visualizer.GetStats();
        std::cout << "Scene Graph Stats:" << std::endl;
        std::cout << "  Total Nodes: " << stats.totalNodes << std::endl;
        std::cout << "  Active Nodes: " << stats.activeNodes << std::endl;
        std::cout << "  Max Depth: " << stats.maxDepth << std::endl;
        
        // 测试树形结构输出
        auto treeStructure = visualizer.GetTreeStructure();
        std::cout << "Tree Structure:" << std::endl;
        std::cout << treeStructure << std::endl;
    } else {
        std::cout << "No active scene" << std::endl;
    }
    
    std::cout << std::endl;
    std::cout << "Note: To fully test SceneGraph visualization, " << std::endl;
    std::cout << "      Scene classes need to expose SceneGraph accessor." << std::endl;
    std::cout << std::endl;
}

} // namespace

int main(int argc, char* argv[]) {
    ConfigureLogger();
    
    LOG_INFO("========================================");
    LOG_INFO("Toolchain Integration Test");
    LOG_INFO("========================================");
    
    // 创建Renderer
    Renderer* renderer = Renderer::Create();
    if (!renderer) {
        LOG_ERROR("Failed to create renderer");
        return -1;
    }
    
    if (!renderer->Initialize("57 - Toolchain Integration Test", 1280, 720)) {
        LOG_ERROR("Failed to initialize renderer");
        Renderer::Destroy(renderer);
        return -1;
    }
    
    renderer->SetClearColor(0.1f, 0.12f, 0.16f, 1.0f);
    renderer->SetVSync(true);
    
    // 创建ApplicationHost
    auto& resourceManager = ResourceManager::GetInstance();
    auto& asyncLoader = AsyncResourceLoader::GetInstance();
    asyncLoader.Initialize();
    
    ApplicationHost host;
    ApplicationHost::Config hostConfig{};
    hostConfig.renderer = renderer;
    hostConfig.resourceManager = &resourceManager;
    hostConfig.asyncLoader = &asyncLoader;
    hostConfig.uniformManager = nullptr;
    
    if (!host.Initialize(hostConfig)) {
        LOG_ERROR("Failed to initialize ApplicationHost");
        asyncLoader.Shutdown();
        Renderer::Destroy(renderer);
        return -1;
    }
    
    // 注册模块
    auto& moduleRegistry = host.GetModuleRegistry();
    moduleRegistry.RegisterModule(std::make_unique<CoreRenderModule>());
    moduleRegistry.RegisterModule(std::make_unique<InputModule>());
    moduleRegistry.RegisterModule(std::make_unique<DebugHUDModule>());
    
    // 注册BootScene
    auto& sceneManager = host.GetSceneManager();
    sceneManager.RegisterSceneFactory("BootScene", []() {
        return std::make_unique<BootScene>();
    });
    
    // 加载初始场景
    if (!sceneManager.PushScene("BootScene")) {
        LOG_ERROR("Failed to push BootScene");
        host.Shutdown();
        asyncLoader.Shutdown();
        Renderer::Destroy(renderer);
        return -1;
    }
    
    LOG_INFO("========================================");
    LOG_INFO("Running Toolchain Integration Tests");
    LOG_INFO("========================================");
    std::cout << std::endl;
    
    // 运行测试
    TestModuleRegistryStatus(host);
    TestMaterialShaderPanelDataSource();
    TestLayerMaskEditorDataSource(*renderer);
    TestSceneGraphVisualizerDataSource(sceneManager);
    
    std::cout << std::endl;
    PrintSeparator();
    std::cout << "All Tests Completed!" << std::endl;
    PrintSeparator();
    
    LOG_INFO("========================================");
    LOG_INFO("Press ESC or close window to exit");
    LOG_INFO("========================================");
    
    // 主循环（保持窗口打开，允许查看结果）
    bool running = true;
    uint32_t frameCount = 0;
    double absoluteTime = 0.0;
    
    while (running) {
        // 处理事件
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) {
                    running = false;
                }
            }
        }
        
        // 获取帧时间
        const float deltaTime = renderer->GetDeltaTime();
        absoluteTime += static_cast<double>(deltaTime);
        
        // 准备帧更新参数
        FrameUpdateArgs frameArgs{};
        frameArgs.deltaTime = deltaTime;
        frameArgs.absoluteTime = absoluteTime;
        
        // 更新应用
        host.UpdateFrame(frameArgs);
        
        // 渲染
        renderer->BeginFrame();
        renderer->Clear();
        
        // 更新世界（ECS系统）
        host.UpdateWorld(deltaTime);
        
        renderer->FlushRenderQueue();
        renderer->EndFrame();
        renderer->Present();
        
        frameCount++;
        
        // 限制帧率（减少CPU占用）
        SDL_Delay(16);  // ~60 FPS
    }
    
    LOG_INFO("Total frames: " + std::to_string(frameCount));
    
    // 清理
    host.Shutdown();
    asyncLoader.Shutdown();
    Renderer::Destroy(renderer);
    
    LOG_INFO("Exiting...");
    return 0;
}

