/**
 * @file 17_model_with_resource_manager_test.cpp
 * @brief 使用资源管理器加载和管理模型、材质、纹理
 * 
 * 本示例演示：
 * 1. 使用 ResourceManager 统一管理所有资源
 * 2. 从模型文件加载网格、材质和纹理，并注册到资源管理器
 * 3. 通过 ResourceManager 获取和使用资源
 * 4. 资源引用计数和生命周期管理
 * 5. 实际场景中的资源管理最佳实践
 * 
 * 与 14_model_material_loader_test 的区别：
 * - 14号: 直接使用局部变量管理资源
 * - 17号: 使用 ResourceManager 统一管理，展示资源管理的优势
 * 
 * 控制：
 * - W：切换线框模式
 * - S：打印资源统计信息
 * - R：重新加载所有资源
 * - SPACE：清理未使用资源
 * - ESC：退出
 */

#include <render/renderer.h>
#include <render/resource_manager.h>
#include <render/shader_cache.h>
#include <render/texture_loader.h>
#include <render/mesh_loader.h>
#include <render/material.h>
#include <render/logger.h>
#include <glad/glad.h>
#include <iostream>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef _WIN32
#include <windows.h>
#endif

using namespace Render;

// 场景状态
float rotationAngle = 0.0f;
bool wireframeMode = false;
std::vector<std::string> meshNames;  // 模型各部件的网格名称
std::vector<std::string> materialNames;  // 对应的材质名称

/**
 * @brief 初始化场景（使用 ResourceManager）
 */
bool InitScene(Renderer& renderer) {
    Logger::GetInstance().Info("========================================");
    Logger::GetInstance().Info("使用资源管理器加载模型");
    Logger::GetInstance().Info("========================================");
    
    auto& resMgr = ResourceManager::GetInstance();
    
    // ========================================================================
    // 1. 注册着色器
    // ========================================================================
    Logger::GetInstance().Info("注册着色器...");
    auto phongShader = ShaderCache::GetInstance().LoadShader(
        "material_phong", "shaders/material_phong.vert", "shaders/material_phong.frag");
    if (!phongShader) {
        Logger::GetInstance().Error("Failed to load Phong shader");
        return false;
    }
    resMgr.RegisterShader("phong", phongShader);
    
    // ========================================================================
    // 2. 从文件加载模型（带材质和纹理）
    // ========================================================================
    Logger::GetInstance().Info("加载模型文件...");
    
    std::vector<std::string> modelPaths = {
        "models/miku/v4c5.0short.pmx",
        "models/miku/v4c5.0.pmx",
        "models/cube.obj",
        "../models/miku/v4c5.0short.pmx",
        "../models/miku/v4c5.0.pmx",
        "../models/cube.obj"
    };
    
    std::vector<MeshWithMaterial> modelParts;
    std::string usedPath;
    
    for (const auto& path : modelPaths) {
        Logger::GetInstance().Info("尝试: " + path);
        auto parts = MeshLoader::LoadFromFileWithMaterials(path, "", true, phongShader);
        if (!parts.empty()) {
            modelParts = std::move(parts);
            usedPath = path;
            Logger::GetInstance().Info("✅ 成功加载模型: " + path);
            break;
        }
    }
    
    // ========================================================================
    // 3. 将加载的资源注册到 ResourceManager
    // ========================================================================
    if (modelParts.empty()) {
        Logger::GetInstance().Warning("未能加载模型文件，创建默认网格");
        
        // 创建默认网格
        auto mesh = MeshLoader::CreateSphere(0.5f, 64, 32, Color::White());
        resMgr.RegisterMesh("default_mesh", mesh);
        meshNames.push_back("default_mesh");
        
        // 创建默认材质
        auto material = std::make_shared<Material>();
        material->SetName("default_material");
        material->SetShader(phongShader);
        material->SetDiffuseColor(Color(0.8f, 0.2f, 0.2f, 1.0f));
        material->SetSpecularColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
        material->SetShininess(32.0f);
        resMgr.RegisterMaterial("default_material", material);
        materialNames.push_back("default_material");
    } else {
        Logger::GetInstance().Info("\n========================================");
        Logger::GetInstance().Info("注册模型资源到 ResourceManager");
        Logger::GetInstance().Info("========================================");
        Logger::GetInstance().Info("网格部件数量: " + std::to_string(modelParts.size()));
        
        // 注册所有网格和材质
        for (size_t i = 0; i < modelParts.size(); ++i) {
            const auto& part = modelParts[i];
            
            // 生成唯一名称
            std::string meshName = "mesh_" + std::to_string(i) + "_" + part.name;
            std::string matName = "material_" + std::to_string(i) + "_" + part.name;
            
            // 注册网格
            if (part.mesh) {
                resMgr.RegisterMesh(meshName, part.mesh);
                meshNames.push_back(meshName);
            }
            
            // 注册材质
            if (part.material) {
                resMgr.RegisterMaterial(matName, part.material);
                materialNames.push_back(matName);
                
                // 打印材质信息
                Logger::GetInstance().Info("部件 " + std::to_string(i) + ": " + part.name);
                Logger::GetInstance().Info("  材质: " + part.material->GetName());
                
                // 注册材质中的纹理到 ResourceManager
                auto texNames = part.material->GetTextureNames();
                if (!texNames.empty()) {
                    Logger::GetInstance().Info("  纹理: " + std::to_string(texNames.size()) + " 个");
                    for (const auto& texName : texNames) {
                        auto texture = part.material->GetTexture(texName);
                        if (texture) {
                            std::string texResName = "tex_" + std::to_string(i) + "_" + texName;
                            resMgr.RegisterTexture(texResName, texture);
                            Logger::GetInstance().Info("    - " + texName);
                        }
                    }
                }
            }
        }
    }
    
    // ========================================================================
    // 4. 打印资源统计信息
    // ========================================================================
    Logger::GetInstance().Info("\n========================================");
    Logger::GetInstance().Info("资源注册完成");
    Logger::GetInstance().Info("========================================");
    resMgr.PrintStatistics();
    
    Logger::GetInstance().Info("\n场景初始化完成!");
    return true;
}

/**
 * @brief 更新场景
 */
void UpdateScene(float deltaTime) {
    rotationAngle += deltaTime * 30.0f;
    if (rotationAngle >= 360.0f) {
        rotationAngle -= 360.0f;
    }
}

/**
 * @brief 渲染场景（从 ResourceManager 获取资源）
 */
void RenderScene(Renderer& renderer) {
    auto& resMgr = ResourceManager::GetInstance();
    
    // 清空屏幕
    auto renderState = renderer.GetRenderState();
    renderState->SetClearColor(Color(0.15f, 0.15f, 0.2f, 1.0f));
    renderState->Clear();
    renderState->SetViewport(0, 0, renderer.GetWidth(), renderer.GetHeight());
    
    // 创建变换矩阵
    float aspect = static_cast<float>(renderer.GetWidth()) / static_cast<float>(renderer.GetHeight());
    
    // 投影矩阵
    Matrix4 projection = Matrix4::Identity();
    float fov = 45.0f * static_cast<float>(M_PI) / 180.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
    float f = 1.0f / std::tan(fov / 2.0f);
    projection(0, 0) = f / aspect;
    projection(1, 1) = f;
    projection(2, 2) = (farPlane + nearPlane) / (nearPlane - farPlane);
    projection(2, 3) = (2.0f * farPlane * nearPlane) / (nearPlane - farPlane);
    projection(3, 2) = -1.0f;
    projection(3, 3) = 0.0f;
    
    // 视图矩阵
    Matrix4 view = Matrix4::Identity();
    view(1, 3) = -0.2f;
    view(2, 3) = -3.5f;
    
    // 模型矩阵
    float angleRad = rotationAngle * static_cast<float>(M_PI) / 180.0f;
    Matrix4 rotationY = Matrix4::Identity();
    rotationY(0, 0) = std::cos(angleRad);
    rotationY(0, 2) = std::sin(angleRad);
    rotationY(2, 0) = -std::sin(angleRad);
    rotationY(2, 2) = std::cos(angleRad);
    
    // 缩放（根据网格数量判断是否为PMX模型）
    float scale = (meshNames.size() > 10) ? 0.08f : 1.0f;
    Matrix4 scaleMatrix = Matrix4::Identity();
    scaleMatrix(0, 0) = scale;
    scaleMatrix(1, 1) = scale;
    scaleMatrix(2, 2) = scale;
    
    // 平移
    Matrix4 translateMatrix = Matrix4::Identity();
    if (meshNames.size() > 10) {
        translateMatrix(1, 3) = -0.6f;
    }
    
    Matrix4 model = translateMatrix * rotationY * scaleMatrix;
    
    // ========================================================================
    // 渲染所有部件（从 ResourceManager 获取资源）
    // ========================================================================
    for (size_t i = 0; i < meshNames.size(); ++i) {
        // 从资源管理器获取网格
        auto mesh = resMgr.GetMesh(meshNames[i]);
        if (!mesh) {
            continue;
        }
        
        // 从资源管理器获取材质
        Ref<Material> material;
        if (i < materialNames.size()) {
            material = resMgr.GetMaterial(materialNames[i]);
        }
        
        // 如果没有材质，使用默认着色器
        if (material && material->IsValid()) {
            // 使用材质渲染
            material->Bind(renderState.get());
            
            auto* uniformMgr = material->GetShader()->GetUniformManager();
            if (uniformMgr) {
                uniformMgr->SetMatrix4("uModel", model);
                uniformMgr->SetMatrix4("uView", view);
                uniformMgr->SetMatrix4("uProjection", projection);
                
                uniformMgr->SetColor("uAmbientColor", material->GetAmbientColor());
                uniformMgr->SetColor("uDiffuseColor", material->GetDiffuseColor());
                uniformMgr->SetColor("uSpecularColor", material->GetSpecularColor());
                uniformMgr->SetFloat("uShininess", material->GetShininess());
                
                uniformMgr->SetVector3("uLightPos", Vector3(5.0f, 5.0f, 5.0f));
                uniformMgr->SetVector3("uViewPos", Vector3(0.0f, 0.0f, 5.0f));
            }
            
            mesh->Draw();
            material->Unbind();
        } else {
            // 使用默认着色器
            auto shader = resMgr.GetShader("phong");
            if (shader && shader->IsValid()) {
                shader->Use();
                
                auto* uniformMgr = shader->GetUniformManager();
                if (uniformMgr) {
                    uniformMgr->SetMatrix4("uModel", model);
                    uniformMgr->SetMatrix4("uView", view);
                    uniformMgr->SetMatrix4("uProjection", projection);
                    
                    uniformMgr->SetColor("uAmbientColor", Color(0.2f, 0.2f, 0.2f, 1.0f));
                    uniformMgr->SetColor("uDiffuseColor", Color(0.7f, 0.7f, 0.7f, 1.0f));
                    uniformMgr->SetColor("uSpecularColor", Color(1.0f, 1.0f, 1.0f, 1.0f));
                    uniformMgr->SetFloat("uShininess", 32.0f);
                    
                    uniformMgr->SetVector3("uLightPos", Vector3(5.0f, 5.0f, 5.0f));
                    uniformMgr->SetVector3("uViewPos", Vector3(0.0f, 0.0f, 5.0f));
                }
                
                mesh->Draw();
                shader->Unuse();
            }
        }
    }
}

/**
 * @brief 处理输入事件
 */
void HandleInput(SDL_Event& event, bool& running, Renderer& renderer) {
    auto& resMgr = ResourceManager::GetInstance();
    
    if (event.type == SDL_EVENT_QUIT) {
        running = false;
    }
    else if (event.type == SDL_EVENT_KEY_DOWN) {
        switch (event.key.key) {
            case SDLK_ESCAPE:
                running = false;
                break;
                
            case SDLK_W:
                wireframeMode = !wireframeMode;
                glPolygonMode(GL_FRONT_AND_BACK, wireframeMode ? GL_LINE : GL_FILL);
                Logger::GetInstance().Info(wireframeMode ? "线框模式: 开启" : "线框模式: 关闭");
                break;
                
            case SDLK_S:
                {
                    // 打印详细统计信息
                    Logger::GetInstance().Info("\n========================================");
                    Logger::GetInstance().Info("资源管理器详细统计");
                    Logger::GetInstance().Info("========================================");
                    resMgr.PrintStatistics();
                    
                    // 打印各类资源列表
                    Logger::GetInstance().Info("\n--- 网格资源 ---");
                    auto meshes = resMgr.ListMeshes();
                    for (const auto& name : meshes) {
                        long refCount = resMgr.GetReferenceCount(ResourceType::Mesh, name);
                        auto mesh = resMgr.GetMesh(name);
                        if (mesh) {
                            Logger::GetInstance().Info("  " + name + 
                                " (顶点: " + std::to_string(mesh->GetVertexCount()) +
                                ", 引用: " + std::to_string(refCount) + ")");
                        }
                    }
                    
                    Logger::GetInstance().Info("\n--- 材质资源 ---");
                    auto materials = resMgr.ListMaterials();
                    for (const auto& name : materials) {
                        long refCount = resMgr.GetReferenceCount(ResourceType::Material, name);
                        auto material = resMgr.GetMaterial(name);
                        if (material) {
                            auto texCount = material->GetTextureNames().size();
                            Logger::GetInstance().Info("  " + name + 
                                " (纹理: " + std::to_string(texCount) +
                                ", 引用: " + std::to_string(refCount) + ")");
                        }
                    }
                    
                    Logger::GetInstance().Info("\n--- 纹理资源 ---");
                    auto textures = resMgr.ListTextures();
                    for (const auto& name : textures) {
                        long refCount = resMgr.GetReferenceCount(ResourceType::Texture, name);
                        auto texture = resMgr.GetTexture(name);
                        if (texture) {
                            size_t memKB = texture->GetMemoryUsage() / 1024;
                            Logger::GetInstance().Info("  " + name + 
                                " (" + std::to_string(texture->GetWidth()) + "x" + std::to_string(texture->GetHeight()) +
                                ", " + std::to_string(memKB) + " KB" +
                                ", 引用: " + std::to_string(refCount) + ")");
                        }
                    }
                    
                    Logger::GetInstance().Info("\n--- 着色器资源 ---");
                    auto shaders = resMgr.ListShaders();
                    for (const auto& name : shaders) {
                        long refCount = resMgr.GetReferenceCount(ResourceType::Shader, name);
                        Logger::GetInstance().Info("  " + name + " (引用: " + std::to_string(refCount) + ")");
                    }
                    
                    Logger::GetInstance().Info("========================================\n");
                }
                break;
                
            case SDLK_R:
                {
                    // 重新加载所有资源
                    Logger::GetInstance().Info("\n重新加载资源...");
                    meshNames.clear();
                    materialNames.clear();
                    resMgr.Clear();
                    ShaderCache::GetInstance().Clear();
                    TextureLoader::GetInstance().Clear();
                    InitScene(renderer);
                }
                break;
                
            case SDLK_SPACE:
                {
                    // 清理未使用资源
                    Logger::GetInstance().Info("\n清理未使用资源...");
                    size_t cleaned = resMgr.CleanupUnused();
                    Logger::GetInstance().Info("清理了 " + std::to_string(cleaned) + " 个未使用资源");
                    resMgr.PrintStatistics();
                }
                break;
        }
    }
}

/**
 * @brief 主函数
 */
int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    // 开启日志
    Logger::GetInstance().SetLogToFile(true);
    Logger::GetInstance().SetLogToConsole(true);
    
    Logger::GetInstance().Info("========================================");
    Logger::GetInstance().Info("模型资源管理器测试");
    Logger::GetInstance().Info("========================================");
    
    try {
        // 初始化渲染器
        Renderer renderer;
        if (!renderer.Initialize("Model with ResourceManager - 模型资源管理器测试", 1280, 720)) {
            Logger::GetInstance().Error("Failed to initialize renderer");
            return -1;
        }
        
        // 初始化场景
        if (!InitScene(renderer)) {
            Logger::GetInstance().Error("Failed to initialize scene");
            renderer.Shutdown();
            return -1;
        }
        
        // 打印控制说明
        Logger::GetInstance().Info("\n========================================");
        Logger::GetInstance().Info("控制说明");
        Logger::GetInstance().Info("========================================");
        Logger::GetInstance().Info("W     - 切换线框模式");
        Logger::GetInstance().Info("S     - 打印资源详细统计");
        Logger::GetInstance().Info("R     - 重新加载所有资源");
        Logger::GetInstance().Info("SPACE - 清理未使用资源");
        Logger::GetInstance().Info("ESC   - 退出");
        Logger::GetInstance().Info("========================================");
        Logger::GetInstance().Info("模型将自动旋转\n");
        
        // 主循环
        bool running = true;
        Uint64 lastTime = SDL_GetTicks();
        
        while (running) {
            // 计算 deltaTime
            Uint64 currentTime = SDL_GetTicks();
            float deltaTime = (currentTime - lastTime) / 1000.0f;
            lastTime = currentTime;
            
            // 处理事件
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                HandleInput(event, running, renderer);
            }
            
            // 更新场景
            UpdateScene(deltaTime);
            
            // 渲染场景
            renderer.BeginFrame();
            RenderScene(renderer);
            renderer.EndFrame();
            
            // 呈现
            renderer.Present();
        }
        
        // 清理
        Logger::GetInstance().Info("\n========================================");
        Logger::GetInstance().Info("清理资源");
        Logger::GetInstance().Info("========================================");
        
        // 打印最终统计
        ResourceManager::GetInstance().PrintStatistics();
        
        // 清空资源管理器
        meshNames.clear();
        materialNames.clear();
        ResourceManager::GetInstance().Clear();
        
        renderer.Shutdown();
        Logger::GetInstance().Info("\n程序正常退出");
        Logger::GetInstance().Info("日志已保存到: " + Logger::GetInstance().GetCurrentLogFile());
        
        return 0;
    }
    catch (const std::exception& e) {
        Logger::GetInstance().Error(std::string("Exception: ") + e.what());
        return -1;
    }
}

