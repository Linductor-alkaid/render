/*
 * Copyright (c) 2025 Li Chaoyu
 * 
 * This file is part of Render.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * For commercial licensing, please contact: 2052046346@qq.com
 */
/**
 * @file 53_event_system_test.cpp
 * @brief 事件系统测试 - 测试 EventBus 过滤器、Blender 风格操作映射、手势事件
 */

#include "render/application/application_host.h"
#include "render/application/event_bus.h"
#include "render/application/events/input_events.h"
#include "render/application/modules/core_render_module.h"
#include "render/application/modules/input_module.h"
#include "render/application/operation_mapping.h"
#include "render/async_resource_loader.h"
#include "render/camera.h"
#include "render/logger.h"
#include "render/mesh.h"
#include "render/mesh_loader.h"
#include "render/renderer.h"
#include "render/resource_manager.h"
#include "render/shader_cache.h"
#include "render/transform.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <string>

using namespace Render;
using namespace Render::Application;
using namespace Render::Application::Events;

// 测试计数器
struct TestCounters {
    int keyEvents = 0;
    int mouseEvents = 0;
    int operationEvents = 0;
    int gestureEvents = 0;
    int filteredEvents = 0;
    int tagFilteredEvents = 0;
    int sceneFilteredEvents = 0;
} g_counters;

// 测试物体状态（用于可视化操作效果）
struct TestObject {
    Transform transform;
    Vector3 position{0.0f, 0.0f, 0.0f};  // 放在原点，相机在 Z=5 看向原点
    Vector3 rotation{0.0f, 0.0f, 0.0f};
    Vector3 scale{1.0f, 1.0f, 1.0f};
    bool isMoving = false;
    bool isRotating = false;
    bool isScaling = false;
} g_testObject;

// 测试事件监听器
void OnKeyEvent(const KeyEvent& event) {
    g_counters.keyEvents++;
    std::cout << "[KeyEvent] Scancode: " << event.scancode 
              << ", State: " << (event.state == KeyState::Pressed ? "Pressed" : "Released")
              << ", Repeat: " << (event.repeat ? "Yes" : "No") << std::endl;
}

void OnMouseButtonEvent(const MouseButtonEvent& event) {
    g_counters.mouseEvents++;
    std::cout << "[MouseButtonEvent] Button: " << static_cast<int>(event.button)
              << ", Position: (" << event.x << ", " << event.y << ")"
              << ", State: " << (event.state == MouseButtonState::Pressed ? "Pressed" : "Released") << std::endl;
}

void OnOperationEvent(const OperationEvent& event) {
    g_counters.operationEvents++;
    const char* opNames[] = {
        "Select", "Add", "Delete", "Move", "Rotate", 
        "Scale", "Duplicate", "Cancel", "Confirm"
    };
    std::cout << "[OperationEvent] Type: " << opNames[static_cast<int>(event.type)]
              << ", Context: " << event.context
              << ", IsStart: " << (event.isStart ? "Yes" : "No") << std::endl;
    
    // 更新测试物体状态（用于可视化）
    if (event.isStart) {
        switch (event.type) {
            case OperationType::Move:
                g_testObject.isMoving = true;
                std::cout << "  -> 开始移动模式" << std::endl;
                break;
            case OperationType::Rotate:
                g_testObject.isRotating = true;
                std::cout << "  -> 开始旋转模式" << std::endl;
                break;
            case OperationType::Scale:
                g_testObject.isScaling = true;
                std::cout << "  -> 开始缩放模式" << std::endl;
                break;
            case OperationType::Cancel:
                g_testObject.isMoving = false;
                g_testObject.isRotating = false;
                g_testObject.isScaling = false;
                std::cout << "  -> 取消操作" << std::endl;
                break;
            default:
                break;
        }
    } else {
        switch (event.type) {
            case OperationType::Move:
                g_testObject.isMoving = false;
                std::cout << "  -> 结束移动模式" << std::endl;
                break;
            case OperationType::Rotate:
                g_testObject.isRotating = false;
                std::cout << "  -> 结束旋转模式" << std::endl;
                break;
            case OperationType::Scale:
                g_testObject.isScaling = false;
                std::cout << "  -> 结束缩放模式" << std::endl;
                break;
            default:
                break;
        }
    }
}

void OnGestureEvent(const GestureEvent& event) {
    g_counters.gestureEvents++;
    const char* gestureNames[] = {
        "Drag", "Click", "DoubleClick", "Pan", "Rotate",
        "Zoom", "BoxSelect", "LassoSelect"
    };
    std::cout << "[GestureEvent] Type: " << gestureNames[static_cast<int>(event.type)]
              << ", Start: (" << event.startX << ", " << event.startY << ")"
              << ", Current: (" << event.currentX << ", " << event.currentY << ")"
              << ", Delta: (" << event.deltaX << ", " << event.deltaY << ")"
              << ", Active: " << (event.isActive ? "Yes" : "No") << std::endl;
}

// 带标签过滤的事件监听器
void OnTaggedKeyEvent(const KeyEvent& event) {
    g_counters.tagFilteredEvents++;
    std::cout << "[TaggedKeyEvent] Received tagged key event: " << event.scancode << std::endl;
}

// 场景过滤的事件监听器
void OnSceneFilteredEvent(const KeyEvent& event) {
    g_counters.sceneFilteredEvents++;
    std::cout << "[SceneFilteredEvent] Received scene-filtered key event: " << event.scancode << std::endl;
}

int main(int argc, char* argv[]) {
    // 设置日志（使用 Warning 级别以减少详细日志）
    Logger::GetInstance().SetLogToConsole(true);
    Logger::GetInstance().SetLogLevel(LogLevel::Warning);  // 只显示警告和错误
    
    // 使用 std::cout 输出测试信息（不受日志级别影响）
    std::cout << "========================================" << std::endl;
    std::cout << "Event System Test" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "测试内容：" << std::endl;
    std::cout << "1. EventBus 事件过滤器" << std::endl;
    std::cout << "2. Blender 风格操作映射" << std::endl;
    std::cout << "3. 快捷键上下文管理" << std::endl;
    std::cout << "4. 手势事件检测" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 创建渲染器
    Renderer* renderer = Renderer::Create();
    if (!renderer) {
        LOG_ERROR("Failed to create renderer");
        return -1;
    }
    
    if (!renderer->Initialize("53 - Event System Test", 1280, 720)) {
        LOG_ERROR("Failed to initialize renderer");
        Renderer::Destroy(renderer);
        return -1;
    }
    
    renderer->SetClearColor(0.1f, 0.12f, 0.16f, 1.0f);
    renderer->SetVSync(true);
    
    // 启用深度测试
    auto renderState = renderer->GetRenderState();
    if (renderState) {
        renderState->SetDepthTest(true);
        renderState->SetCullFace(CullFace::Back);
    }
    
    // 初始化资源管理器
    auto& resourceManager = ResourceManager::GetInstance();
    auto& asyncLoader = AsyncResourceLoader::GetInstance();
    asyncLoader.Initialize();
    
    // 创建 ApplicationHost
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
    
    // 注册模块（注意：必须先注册 CoreRenderModule，因为 InputModule 依赖它）
    // ModuleRegistry 的日志已经改为 Debug 级别，所以这里不需要再调整
    auto& moduleRegistry = host.GetModuleRegistry();
    moduleRegistry.RegisterModule(std::make_unique<CoreRenderModule>());
    moduleRegistry.RegisterModule(std::make_unique<InputModule>());
    
    auto& eventBus = host.GetEventBus();
    
    // 获取 InputModule
    auto* inputModule = static_cast<InputModule*>(moduleRegistry.GetModule("InputModule"));
    if (!inputModule) {
        LOG_ERROR("InputModule not found");
        return -1;
    }
    
    std::cout << "========================================" << std::endl;
    std::cout << "1. 测试基本事件订阅" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 订阅基本事件
    auto keyListenerId = eventBus.Subscribe<KeyEvent>(OnKeyEvent);
    auto mouseListenerId = eventBus.Subscribe<MouseButtonEvent>(OnMouseButtonEvent);
    
    std::cout << "已订阅 KeyEvent 和 MouseButtonEvent" << std::endl;
    
    std::cout << "========================================" << std::endl;
    std::cout << "2. 测试事件过滤器 - 标签过滤" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 使用标签过滤器订阅
    auto tagFilter = std::make_shared<TagEventFilter>("test");
    auto taggedListenerId = eventBus.Subscribe<KeyEvent>(OnTaggedKeyEvent, 0, tagFilter);
    
    std::cout << "已订阅带标签过滤的 KeyEvent（标签：test）" << std::endl;
    
    std::cout << "========================================" << std::endl;
    std::cout << "3. 测试事件过滤器 - 场景过滤" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 使用场景过滤器订阅
    auto sceneFilter = std::make_shared<SceneEventFilter>("TestScene");
    auto sceneListenerId = eventBus.Subscribe<KeyEvent>(OnSceneFilteredEvent, 0, sceneFilter);
    
    std::cout << "已订阅场景过滤的 KeyEvent（场景：TestScene）" << std::endl;
    
    std::cout << "========================================" << std::endl;
    std::cout << "4. 测试 Blender 风格操作映射" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 订阅操作事件
    auto operationListenerId = eventBus.Subscribe<OperationEvent>(OnOperationEvent);
    
    std::cout << "已订阅 OperationEvent" << std::endl;
    std::cout << "快捷键映射：" << std::endl;
    std::cout << "  G - 移动 (Move)" << std::endl;
    std::cout << "  R - 旋转 (Rotate)" << std::endl;
    std::cout << "  S - 缩放 (Scale)" << std::endl;
    std::cout << "  X - 删除 (Delete)" << std::endl;
    std::cout << "  Shift+D - 复制 (Duplicate)" << std::endl;
    std::cout << "  Esc - 取消 (Cancel)" << std::endl;
    std::cout << "  Enter - 确认 (Confirm)" << std::endl;
    
    std::cout << "========================================" << std::endl;
    std::cout << "5. 测试手势事件" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 订阅手势事件
    auto gestureListenerId = eventBus.Subscribe<GestureEvent>(OnGestureEvent);
    
    std::cout << "已订阅 GestureEvent" << std::endl;
    std::cout << "手势说明：" << std::endl;
    std::cout << "  左键拖拽 - Drag" << std::endl;
    std::cout << "  Shift+左键拖拽 - BoxSelect" << std::endl;
    std::cout << "  中键拖拽 - Pan" << std::endl;
    std::cout << "  Alt+中键拖拽 - Rotate" << std::endl;
    std::cout << "  滚轮 - Zoom" << std::endl;
    
    std::cout << "========================================" << std::endl;
    std::cout << "6. 测试快捷键上下文切换" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 测试上下文切换
    inputModule->SetShortcutContext("ObjectMode");
    std::cout << "当前上下文：ObjectMode" << std::endl;
    
    // 测试切换到 EditMode
    inputModule->SetShortcutContext("EditMode");
    std::cout << "切换到上下文：EditMode" << std::endl;
    
    // 切换回 ObjectMode
    inputModule->SetShortcutContext("ObjectMode");
    std::cout << "切换回上下文：ObjectMode" << std::endl;
    
    std::cout << "========================================" << std::endl;
    std::cout << "初始化渲染资源..." << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 创建测试立方体
    auto testCube = MeshLoader::CreateCube(1.0f, 1.0f, 1.0f, Color::Cyan());
    if (!testCube) {
        std::cerr << "Failed to create test cube" << std::endl;
        return -1;
    }
    
    // 加载着色器
    auto shader = ShaderCache::GetInstance().LoadShader("mesh_test", "shaders/mesh_test.vert", "shaders/mesh_test.frag");
    if (!shader) {
        std::cerr << "Failed to load shader" << std::endl;
        return -1;
    }
    
    // 创建相机（从 Z 轴负方向看向原点）
    Camera camera;
    camera.SetPerspective(45.0f, 1280.0f / 720.0f, 0.1f, 100.0f);
    camera.SetPosition(Vector3(0.0f, 0.0f, -5.0f));
    camera.LookAt(Vector3(0.0f, 0.0f, 0.0f));
    
    std::cout << "渲染资源初始化完成" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "开始测试 - 请操作键盘和鼠标" << std::endl;
    std::cout << "按 G/R/S 键测试移动/旋转/缩放操作" << std::endl;
    std::cout << "按 ESC 退出" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 初始化测试物体的 Transform
    g_testObject.transform.SetPosition(g_testObject.position);
    g_testObject.transform.SetRotationEulerDegrees(g_testObject.rotation);
    g_testObject.transform.SetScale(g_testObject.scale);
    
    // 主循环
    bool running = true;
    uint64_t frameCount = 0;
    double absoluteTime = 0.0;
    
    while (running) {
        renderer->BeginFrame();
        renderer->Clear();
        
        const float deltaTime = renderer->GetDeltaTime();
        absoluteTime += static_cast<double>(deltaTime);
        
        FrameUpdateArgs frameArgs{};
        frameArgs.deltaTime = deltaTime;
        frameArgs.absoluteTime = absoluteTime;
        frameArgs.frameIndex = frameCount++;
        
        // PreFrame 阶段（处理输入事件）
        host.GetModuleRegistry().InvokePhase(ModulePhase::PreFrame, frameArgs);
        
        // 检查退出请求
        if (inputModule->WasQuitRequested() || inputModule->IsKeyDown(SDL_SCANCODE_ESCAPE)) {
            running = false;
        }
        
        // 更新测试物体状态（根据操作模式）
        if (g_testObject.isMoving) {
            // 移动模式：根据鼠标移动
            g_testObject.position = g_testObject.position + Vector3(deltaTime * 0.5f, 0.0f, 0.0f);
        }
        if (g_testObject.isRotating) {
            // 旋转模式：持续旋转
            g_testObject.rotation = g_testObject.rotation + Vector3(0.0f, deltaTime * 90.0f, 0.0f);  // 每秒90度
        }
        if (g_testObject.isScaling) {
            // 缩放模式：脉冲缩放
            float scaleFactor = 1.0f + 0.3f * std::sin(static_cast<float>(absoluteTime) * 2.0f);
            g_testObject.scale = Vector3(scaleFactor, scaleFactor, scaleFactor);
        } else {
            g_testObject.scale = Vector3(1.0f, 1.0f, 1.0f);
        }
        
        // 更新变换
        g_testObject.transform.SetPosition(g_testObject.position);
        g_testObject.transform.SetRotationEulerDegrees(g_testObject.rotation);
        g_testObject.transform.SetScale(g_testObject.scale);
        
        // 测试发布带标签的事件
        if (frameCount % 300 == 0 && frameCount > 0) {
            KeyEvent taggedEvent;
            taggedEvent.scancode = 100;  // 测试用
            taggedEvent.AddTag("test");
            eventBus.Publish(taggedEvent);
        }
        
        // 测试发布场景过滤的事件
        if (frameCount % 600 == 0 && frameCount > 0) {
            KeyEvent sceneEvent;
            sceneEvent.scancode = 200;  // 测试用
            sceneEvent.targetSceneId = "TestScene";
            eventBus.Publish(sceneEvent, "TestScene");
        }
        
        // 渲染测试立方体（使用 Camera 类）
        shader->Use();
        
        // 使用 Camera 的方法获取矩阵
        Matrix4 viewMatrix = camera.GetViewMatrix();
        Matrix4 projMatrix = camera.GetProjectionMatrix();
        Matrix4 modelMatrix = g_testObject.transform.GetLocalMatrix();
        
        // 调试输出（仅第一帧）
        if (frameCount == 1) {
            // 检查 Transform 的世界矩阵
            Matrix4 worldMat = camera.GetTransform().GetWorldMatrix();
            
            // 手动计算逆矩阵进行验证
            Matrix3 rotation = worldMat.block<3, 3>(0, 0);
            Vector3 translation = worldMat.block<3, 1>(0, 3);
            Matrix3 rotationInv = rotation.transpose();
            
            // 分步计算以检查问题
            // R^T * T 应该将平移向量从世界空间转换到相机空间
            Vector3 rtTimesT = rotationInv * translation;
            Vector3 translationInv = -rtTimesT;
            
            // 验证：手动计算 R^T * T
            // R^T 的每一行是原旋转矩阵的对应列
            // R^T * T = [R^T的第0行·T, R^T的第1行·T, R^T的第2行·T]
            Vector3 manualRT_T(
                rotationInv.row(0).dot(translation),
                rotationInv.row(1).dot(translation),
                rotationInv.row(2).dot(translation)
            );
            
            std::cout << "[调试] 原始平移 T: (" << translation.x() << ", " 
                      << translation.y() << ", " << translation.z() << ")" << std::endl;
            std::cout << "[调试] R^T 第0行: (" << rotationInv.row(0).x() << ", " 
                      << rotationInv.row(0).y() << ", " << rotationInv.row(0).z() << ")" << std::endl;
            std::cout << "[调试] R^T 第1行: (" << rotationInv.row(1).x() << ", " 
                      << rotationInv.row(1).y() << ", " << rotationInv.row(1).z() << ")" << std::endl;
            std::cout << "[调试] R^T 第2行: (" << rotationInv.row(2).x() << ", " 
                      << rotationInv.row(2).y() << ", " << rotationInv.row(2).z() << ")" << std::endl;
            std::cout << "[调试] R^T * T (矩阵乘法): (" << rtTimesT.x() << ", " 
                      << rtTimesT.y() << ", " << rtTimesT.z() << ")" << std::endl;
            std::cout << "[调试] R^T * T (手动点积): (" << manualRT_T.x() << ", " 
                      << manualRT_T.y() << ", " << manualRT_T.z() << ")" << std::endl;
            std::cout << "[调试] -R^T * T (最终结果): (" << translationInv.x() << ", " 
                      << translationInv.y() << ", " << translationInv.z() << ")" << std::endl;
            
            // 打印完整的旋转矩阵
            std::cout << "[调试] 相机位置: (" << camera.GetPosition().x() << ", " 
                      << camera.GetPosition().y() << ", " << camera.GetPosition().z() << ")" << std::endl;
            std::cout << "[调试] 相机前方向: (" << camera.GetForward().x() << ", " 
                      << camera.GetForward().y() << ", " << camera.GetForward().z() << ")" << std::endl;
            std::cout << "[调试] Transform世界矩阵平移: (" << worldMat(0, 3) << ", " 
                      << worldMat(1, 3) << ", " << worldMat(2, 3) << ")" << std::endl;
            std::cout << "[调试] Transform世界矩阵旋转:" << std::endl;
            std::cout << "  X轴: (" << worldMat(0, 0) << ", " << worldMat(1, 0) << ", " << worldMat(2, 0) << ")" << std::endl;
            std::cout << "  Y轴: (" << worldMat(0, 1) << ", " << worldMat(1, 1) << ", " << worldMat(2, 1) << ")" << std::endl;
            std::cout << "  Z轴: (" << worldMat(0, 2) << ", " << worldMat(1, 2) << ", " << worldMat(2, 2) << ")" << std::endl;
            std::cout << "[调试] 旋转转置后:" << std::endl;
            std::cout << "  X轴: (" << rotationInv(0, 0) << ", " << rotationInv(1, 0) << ", " << rotationInv(2, 0) << ")" << std::endl;
            std::cout << "  Y轴: (" << rotationInv(0, 1) << ", " << rotationInv(1, 1) << ", " << rotationInv(2, 1) << ")" << std::endl;
            std::cout << "  Z轴: (" << rotationInv(0, 2) << ", " << rotationInv(1, 2) << ", " << rotationInv(2, 2) << ")" << std::endl;
            std::cout << "[调试] R^T * T = (" << (rotationInv * translation).x() << ", " 
                      << (rotationInv * translation).y() << ", " << (rotationInv * translation).z() << ")" << std::endl;
            std::cout << "[调试] 手动计算的平移逆: (" << translationInv.x() << ", " 
                      << translationInv.y() << ", " << translationInv.z() << ")" << std::endl;
            std::cout << "[调试] 视图矩阵平移部分: (" << viewMatrix(0, 3) << ", " 
                      << viewMatrix(1, 3) << ", " << viewMatrix(2, 3) << ")" << std::endl;
            std::cout << "[调试] 视图矩阵Z轴: (" << viewMatrix(0, 2) << ", " 
                      << viewMatrix(1, 2) << ", " << viewMatrix(2, 2) << ")" << std::endl;
        }
        
        // MVP 矩阵
        Matrix4 mvpMatrix = projMatrix * viewMatrix * modelMatrix;
        
        auto* uniformMgr = shader->GetUniformManager();
        if (uniformMgr) {
            // mesh_test 着色器使用 uMVP 和 uColor
            uniformMgr->SetMatrix4("uMVP", mvpMatrix);
            Color cubeColor(0.0f, 1.0f, 1.0f, 1.0f);  // 青色
            uniformMgr->SetColor("uColor", cubeColor);
            
            // 设置简单的光照方向
            Vector3 lightDir(-0.5f, -1.0f, -0.3f);
            lightDir.normalize();
            uniformMgr->SetVector3("uLightDir", lightDir);
        }
        
        if (testCube) {
            testCube->Draw();
        }
        shader->Unuse();
        
        // PostFrame 阶段
        host.GetModuleRegistry().InvokePhase(ModulePhase::PostFrame, frameArgs);
        
        host.GetContext().lastFrame = frameArgs;
        host.UpdateWorld(deltaTime);
        
        renderer->FlushRenderQueue();
        renderer->EndFrame();
        renderer->Present();
        
        asyncLoader.ProcessCompletedTasks(4);
        
        // 每5秒输出一次统计
        static float statsTimer = 0.0f;
        statsTimer += deltaTime;
        if (statsTimer >= 5.0f) {
            std::cout << "========================================" << std::endl;
            std::cout << "事件统计（过去5秒）：" << std::endl;
            std::cout << "  KeyEvent: " << g_counters.keyEvents << std::endl;
            std::cout << "  MouseButtonEvent: " << g_counters.mouseEvents << std::endl;
            std::cout << "  OperationEvent: " << g_counters.operationEvents << std::endl;
            std::cout << "  GestureEvent: " << g_counters.gestureEvents << std::endl;
            std::cout << "  标签过滤事件: " << g_counters.tagFilteredEvents << std::endl;
            std::cout << "  场景过滤事件: " << g_counters.sceneFilteredEvents << std::endl;
            std::cout << "========================================" << std::endl;
            
            // 重置计数器
            g_counters = TestCounters();
            statsTimer = 0.0f;
        }
    }
    
    std::cout << "========================================" << std::endl;
    std::cout << "测试完成" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 清理
    eventBus.Unsubscribe(keyListenerId);
    eventBus.Unsubscribe(mouseListenerId);
    eventBus.Unsubscribe(taggedListenerId);
    eventBus.Unsubscribe(sceneListenerId);
    eventBus.Unsubscribe(operationListenerId);
    eventBus.Unsubscribe(gestureListenerId);
    
    host.Shutdown();
    asyncLoader.Shutdown();
    Renderer::Destroy(renderer);
    
    return 0;
}

