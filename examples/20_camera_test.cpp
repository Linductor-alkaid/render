/**
 * @file 20_camera_test.cpp
 * @brief 相机系统测试
 * 
 * 测试内容：
 * 1. 透视投影和正交投影
 * 2. 第一人称相机控制
 * 3. 轨道相机控制
 * 4. 第三人称相机控制
 * 5. 相机切换和平滑过渡
 * 6. 加载并渲染miku模型
 * 
 * 控制：
 * - WASD：前后左右移动（W后退，S前进）
 * - QE：上下移动
 * - 鼠标移动：旋转视角
 * - 鼠标滚轮：缩放
 * - 1/2/3：切换相机模式
 * - P：切换投影模式
 * - ESC：退出
 */

#include "render/renderer.h"
#include "render/camera.h"
#include "render/shader_cache.h"
#include "render/mesh_loader.h"
#include "render/resource_manager.h"
#include "render/material.h"
#include "render/logger.h"
#include <SDL3/SDL.h>
#include <memory>
#include <vector>
#include <sstream>

using namespace Render;

// 模型数据
std::vector<std::string> meshNames;
std::vector<std::string> materialNames;

// 相机控制模式
enum class CameraMode {
    FirstPerson,
    Orbit,
    ThirdPerson
};

int main(int argc, char* argv[]) {
    Logger::GetInstance().Info("=== 相机系统测试 ===");
    
    // 初始化渲染器
    auto renderer = Renderer::Create();
    if (!renderer->Initialize()) {
        Logger::GetInstance().Error("Failed to initialize renderer");
        return -1;
    }
    
    renderer->SetWindowSize(1920, 1080);  // 更大的窗口
    renderer->SetVSync(true);
    renderer->SetClearColor(0.1f, 0.15f, 0.2f, 1.0f);
    
    // 加载着色器
    auto& shaderCache = ShaderCache::GetInstance();
    auto shader = shaderCache.LoadShader("camera_test", 
        "shaders/camera_test.vert", 
        "shaders/camera_test.frag");
    
    if (!shader) {
        Logger::GetInstance().Error("Failed to load shader");
        return -1;
    }
    
    // 创建相机（更高精度）
    Camera camera;
    camera.SetPerspective(60.0f, 16.0f / 9.0f, 0.01f, 1000.0f);  // 近裁剪面从0.1改为0.01，精度更高
    camera.SetPosition(Vector3(0.0f, 10.0f, 20.0f));  // 更高更远的初始位置，方便观察模型
    camera.LookAt(Vector3(0.0f, 8.0f, 0.0f));  // 朝向模型的头部位置
    
    // 创建相机控制器（使用引用，而不是指针）
    CameraMode currentMode = CameraMode::FirstPerson;
    std::unique_ptr<CameraController> controller;
    
    auto firstPersonController = std::make_unique<FirstPersonCameraController>(camera);
    firstPersonController->SetMoveSpeed(10.0f);
    firstPersonController->SetMouseSensitivity(0.15f);  // 鼠标灵敏度保持不变，但方向会在控制器内部调换
    
    auto orbitController = std::make_unique<OrbitCameraController>(camera, Vector3::Zero());
    orbitController->SetDistance(15.0f);
    orbitController->SetMouseSensitivity(0.3f);
    
    auto thirdPersonController = std::make_unique<ThirdPersonCameraController>(camera);
    thirdPersonController->SetTarget(Vector3::Zero());
    thirdPersonController->SetDistance(10.0f);
    thirdPersonController->SetSmoothness(0.05f);
    
    controller = std::move(firstPersonController);
    
    // 资源管理器
    auto& resMgr = ResourceManager::GetInstance();
    
    // 创建材质着色器
    auto phongShader = shaderCache.LoadShader(
        "material_phong", "shaders/material_phong.vert", "shaders/material_phong.frag");
    if (!phongShader) {
        Logger::GetInstance().Warning("无法加载Phong着色器，使用基础着色器");
        phongShader = shader;
    }
    resMgr.RegisterShader("phong", phongShader);
    
    // 尝试加载miku模型
    Logger::GetInstance().Info("尝试加载miku模型...");
    std::vector<std::string> modelPaths = {
        "models/miku/v4c5.0short.pmx",
        "models/miku/v4c5.0.pmx",
        "../models/miku/v4c5.0short.pmx",
        "../models/miku/v4c5.0.pmx"
    };
    
    bool modelLoaded = false;
    for (const auto& path : modelPaths) {
        Logger::GetInstance().Info("尝试路径: " + path);
        auto parts = MeshLoader::LoadFromFileWithMaterials(path, "", true, phongShader);
        if (!parts.empty()) {
            std::ostringstream oss;
            oss << "成功加载模型: " << path << ", 部件数量: " << parts.size();
            Logger::GetInstance().Info(oss.str());
            
            // 注册所有网格和材质
            for (size_t i = 0; i < parts.size(); ++i) {
                const auto& part = parts[i];
                std::string meshName = "mesh_" + std::to_string(i);
                std::string matName = "material_" + std::to_string(i);
                
                if (part.mesh) {
                    resMgr.RegisterMesh(meshName, part.mesh);
                    meshNames.push_back(meshName);
                    
                    std::ostringstream meshInfo;
                    meshInfo << "  网格 " << i << ": " << part.name 
                            << " (顶点: " << part.mesh->GetVertexCount() << ")";
                    Logger::GetInstance().Info(meshInfo.str());
                }
                if (part.material) {
                    resMgr.RegisterMaterial(matName, part.material);
                    materialNames.push_back(matName);
                }
            }
            
            modelLoaded = true;
            break;
        }
    }
    
    std::ostringstream loadInfo;
    loadInfo << "模型加载状态: " << (modelLoaded ? "成功" : "失败") 
             << ", 网格数量: " << meshNames.size()
             << ", 材质数量: " << materialNames.size();
    Logger::GetInstance().Info(loadInfo.str());
    
    if (!modelLoaded) {
        Logger::GetInstance().Warning("未能加载miku模型，创建默认场景");
        
        // 创建默认网格
        auto mesh = MeshLoader::CreateSphere(1.0f, 64, 32, Color::White());
        resMgr.RegisterMesh("default_mesh", mesh);
        meshNames.push_back("default_mesh");
        
        auto material = std::make_shared<Material>();
        material->SetName("default_material");
        material->SetShader(phongShader);
        material->SetDiffuseColor(Color(0.8f, 0.2f, 0.8f, 1.0f));
        resMgr.RegisterMaterial("default_material", material);
        materialNames.push_back("default_material");
    }
    
    // 创建地面
    auto groundMesh = MeshLoader::CreatePlane(50.0f, 50.0f, 10, 10, Color(0.3f, 0.3f, 0.3f, 1.0f));
    Transform groundTransform;
    groundTransform.SetPosition(Vector3(0.0f, -0.01f, 0.0f));  // 稍微降低，避免Z-fighting
    
    // 模型变换
    Transform modelTransform;
    if (modelLoaded) {
        modelTransform.SetPosition(Vector3(0.0f, 0.0f, 0.0f));
        modelTransform.SetScale(0.08f);  // miku模型通常比较大，缩小一些
        Logger::GetInstance().Info("Miku模型位置: (0, 0, 0), 缩放: 0.08");
    } else {
        modelTransform.SetPosition(Vector3(0.0f, 1.0f, 0.0f));  // 默认球体在地面上方
        modelTransform.SetScale(2.0f);   // 默认球体放大一些，容易看见
        Logger::GetInstance().Info("默认球体位置: (0, 1, 0), 缩放: 2.0");
    }
    
    Logger::GetInstance().Info("初始化成功");
    Logger::GetInstance().Info("控制说明：");
    Logger::GetInstance().Info("  W - 向后移动 / S - 向前移动");
    Logger::GetInstance().Info("  A - 向左移动 / D - 向右移动");
    Logger::GetInstance().Info("  Q - 向下移动 / E - 向上移动");
    Logger::GetInstance().Info("  鼠标移动 - 旋转视角");
    Logger::GetInstance().Info("  鼠标滚轮 - 缩放");
    Logger::GetInstance().Info("  1 - 第一人称相机模式");
    Logger::GetInstance().Info("  2 - 轨道相机模式");
    Logger::GetInstance().Info("  3 - 第三人称相机模式");
    Logger::GetInstance().Info("  P - 切换投影模式（透视/正交）");
    Logger::GetInstance().Info("  I - 显示调试信息");
    Logger::GetInstance().Info("  ESC - 退出");
    
    // 启用相对鼠标模式（鼠标直接控制视角）
    SDL_SetWindowRelativeMouseMode(renderer->GetContext()->GetWindow(), true);
    
    // 主循环
    bool running = true;
    float lastTime = SDL_GetTicks() / 1000.0f;
    
    while (running) {
        float currentTime = SDL_GetTicks() / 1000.0f;
        float deltaTime = currentTime - lastTime;
        lastTime = currentTime;
        
        // 事件处理
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            else if (event.type == SDL_EVENT_KEY_DOWN) {
                switch (event.key.key) {
                    case SDLK_ESCAPE:
                        running = false;
                        break;
                    
                    case SDLK_1:
                        Logger::GetInstance().Info("切换到第一人称相机模式");
                        currentMode = CameraMode::FirstPerson;
                        firstPersonController = std::make_unique<FirstPersonCameraController>(camera);
                        firstPersonController->SetMoveSpeed(10.0f);
                        firstPersonController->SetMouseSensitivity(0.15f);
                        controller = std::move(firstPersonController);
                        break;
                    
                    case SDLK_2:
                        Logger::GetInstance().Info("切换到轨道相机模式");
                        currentMode = CameraMode::Orbit;
                        orbitController = std::make_unique<OrbitCameraController>(camera, Vector3::Zero());
                        orbitController->SetDistance(15.0f);
                        orbitController->SetMouseSensitivity(0.3f);
                        controller = std::move(orbitController);
                        break;
                    
                    case SDLK_3:
                        Logger::GetInstance().Info("切换到第三人称相机模式");
                        currentMode = CameraMode::ThirdPerson;
                        thirdPersonController = std::make_unique<ThirdPersonCameraController>(camera);
                        thirdPersonController->SetTarget(Vector3::Zero());
                        thirdPersonController->SetDistance(10.0f);
                        thirdPersonController->SetSmoothness(0.05f);
                        controller = std::move(thirdPersonController);
                        break;
                    
                    case SDLK_P: {
                        // 切换投影模式
                        if (camera.GetProjectionType() == ProjectionType::Perspective) {
                            Logger::GetInstance().Info("切换到正交投影");
                            camera.SetOrthographic(20.0f, 15.0f, 0.01f, 1000.0f);
                        } else {
                            Logger::GetInstance().Info("切换到透视投影");
                            camera.SetPerspective(60.0f, 16.0f / 9.0f, 0.01f, 1000.0f);
                        }
                        break;
                    }
                    
                    case SDLK_I: {
                        // 显示调试信息
                        Logger::GetInstance().Info("========================================");
                        Logger::GetInstance().Info("调试信息");
                        Logger::GetInstance().Info("========================================");
                        
                        std::ostringstream info;
                        Vector3 camPos = camera.GetPosition();
                        info << "相机位置: (" << camPos.x() << ", " << camPos.y() << ", " << camPos.z() << ")";
                        Logger::GetInstance().Info(info.str());
                        
                        Vector3 modelPos = modelTransform.GetPosition();
                        Vector3 modelScale = modelTransform.GetScale();
                        std::ostringstream modelInfo;
                        modelInfo << "模型位置: (" << modelPos.x() << ", " << modelPos.y() << ", " << modelPos.z() << ")";
                        Logger::GetInstance().Info(modelInfo.str());
                        
                        std::ostringstream scaleInfo;
                        scaleInfo << "模型缩放: (" << modelScale.x() << ", " << modelScale.y() << ", " << modelScale.z() << ")";
                        Logger::GetInstance().Info(scaleInfo.str());
                        
                        std::ostringstream meshInfo;
                        meshInfo << "加载的网格数量: " << meshNames.size();
                        Logger::GetInstance().Info(meshInfo.str());
                        
                        Logger::GetInstance().Info("========================================");
                        break;
                    }
                    
                    // 第一人称相机移动控制（W后退，S前进）
                    case SDLK_W:
                        if (auto* fpc = dynamic_cast<FirstPersonCameraController*>(controller.get()))
                            fpc->SetMoveBackward(true);  // W改为后退
                        break;
                    case SDLK_S:
                        if (auto* fpc = dynamic_cast<FirstPersonCameraController*>(controller.get()))
                            fpc->SetMoveForward(true);   // S改为前进
                        break;
                    case SDLK_A:
                        if (auto* fpc = dynamic_cast<FirstPersonCameraController*>(controller.get()))
                            fpc->SetMoveLeft(true);
                        break;
                    case SDLK_D:
                        if (auto* fpc = dynamic_cast<FirstPersonCameraController*>(controller.get()))
                            fpc->SetMoveRight(true);
                        break;
                    case SDLK_Q:
                        if (auto* fpc = dynamic_cast<FirstPersonCameraController*>(controller.get()))
                            fpc->SetMoveUp(true);
                        break;
                    case SDLK_E:
                        if (auto* fpc = dynamic_cast<FirstPersonCameraController*>(controller.get()))
                            fpc->SetMoveDown(true);
                        break;
                }
            }
            else if (event.type == SDL_EVENT_KEY_UP) {
                switch (event.key.key) {
                    case SDLK_W:
                        if (auto* fpc = dynamic_cast<FirstPersonCameraController*>(controller.get()))
                            fpc->SetMoveBackward(false);  // W改为后退
                        break;
                    case SDLK_S:
                        if (auto* fpc = dynamic_cast<FirstPersonCameraController*>(controller.get()))
                            fpc->SetMoveForward(false);   // S改为前进
                        break;
                    case SDLK_A:
                        if (auto* fpc = dynamic_cast<FirstPersonCameraController*>(controller.get()))
                            fpc->SetMoveLeft(false);
                        break;
                    case SDLK_D:
                        if (auto* fpc = dynamic_cast<FirstPersonCameraController*>(controller.get()))
                            fpc->SetMoveRight(false);
                        break;
                    case SDLK_Q:
                        if (auto* fpc = dynamic_cast<FirstPersonCameraController*>(controller.get()))
                            fpc->SetMoveUp(false);
                        break;
                    case SDLK_E:
                        if (auto* fpc = dynamic_cast<FirstPersonCameraController*>(controller.get()))
                            fpc->SetMoveDown(false);
                        break;
                }
            }
            else if (event.type == SDL_EVENT_MOUSE_MOTION) {
                // 鼠标移动直接控制视角（不需要按住按钮）
                if (controller) {
                    controller->OnMouseMove(event.motion.xrel, event.motion.yrel);
                }
            }
            else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
                if (controller) {
                    controller->OnMouseScroll(event.wheel.y);
                }
            }
        }
        
        // 更新相机控制器
        if (controller) {
            controller->Update(deltaTime);
        }
        
        // 更新第三人称相机目标
        if (currentMode == CameraMode::ThirdPerson) {
            if (auto* tpc = dynamic_cast<ThirdPersonCameraController*>(controller.get())) {
                tpc->SetTarget(modelTransform.GetPosition());
            }
        }
        
        // 渲染
        renderer->BeginFrame();
        renderer->Clear();
        
        // 确保深度测试启用
        auto renderState = renderer->GetRenderState();
        if (renderState) {
            renderState->SetDepthTest(true);
            renderState->SetDepthFunc(DepthFunc::Less);
        }
        
        // 使用phong着色器渲染模型
        phongShader->Use();
        
        // 设置相机矩阵
        Matrix4 view = camera.GetViewMatrix();
        Matrix4 projection = camera.GetProjectionMatrix();
        phongShader->GetUniformManager()->SetMatrix4("uView", view);
        phongShader->GetUniformManager()->SetMatrix4("uProjection", projection);
        
        // 设置光照（Phong着色器使用）
        Vector3 lightPos(10.0f, 15.0f, 10.0f);
        Vector3 viewPos = camera.GetPosition();
        phongShader->GetUniformManager()->SetVector3("uLightPos", lightPos);
        phongShader->GetUniformManager()->SetVector3("uViewPos", viewPos);
        
        // 渲染模型的所有部件
        if (!meshNames.empty()) {
            Matrix4 modelMatrix = modelTransform.GetWorldMatrix();
            auto* uniformMgr = phongShader->GetUniformManager();
            
            for (size_t i = 0; i < meshNames.size() && i < materialNames.size(); ++i) {
                auto mesh = resMgr.GetMesh(meshNames[i]);
                auto material = resMgr.GetMaterial(materialNames[i]);
                
                if (mesh && material && uniformMgr) {
                    // 绑定材质（绑定纹理等）
                    material->Bind();
                    
                    // 设置模型矩阵
                    uniformMgr->SetMatrix4("uModel", modelMatrix);
                    
                    // 设置材质属性
                    uniformMgr->SetColor("uAmbientColor", material->GetAmbientColor());
                    uniformMgr->SetColor("uDiffuseColor", material->GetDiffuseColor());
                    uniformMgr->SetColor("uSpecularColor", material->GetSpecularColor());
                    uniformMgr->SetFloat("uShininess", material->GetShininess());
                    
                    // 绘制网格
                    mesh->Draw();
                    
                    // 解绑材质
                    material->Unbind();
                }
            }
        }
        
        phongShader->Unuse();
        
        // 渲染地面（使用基础着色器）
        shader->Use();
        auto* groundUniformMgr = shader->GetUniformManager();
        if (groundUniformMgr) {
            groundUniformMgr->SetMatrix4("uView", view);
            groundUniformMgr->SetMatrix4("uProjection", projection);
            groundUniformMgr->SetVector3("uLightPos", lightPos);
            groundUniformMgr->SetVector3("uViewPos", viewPos);
            groundUniformMgr->SetVector3("uLightColor", Vector3(1.0f, 1.0f, 1.0f));
            
            Matrix4 groundModel = groundTransform.GetWorldMatrix();
            groundUniformMgr->SetMatrix4("uModel", groundModel);
        }
        groundMesh->Draw();
        
        shader->Unuse();
        
        renderer->EndFrame();
        renderer->Present();
    }
    
    Logger::GetInstance().Info("相机系统测试结束");
    
    renderer->Shutdown();
    delete renderer;
    
    return 0;
}
