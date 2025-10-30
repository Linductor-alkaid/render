/**
 * @file 11_model_loader_test.cpp
 * @brief 模型加载器测试示例
 * 
 * 测试 MeshLoader 从外部文件加载 3D 模型的功能
 * 支持的格式：OBJ, FBX, GLTF/GLB, Collada, Blender, PMX/PMD (MMD), 3DS, PLY, STL 等
 * 
 * 控制：
 * - ESC：退出
 * - 模型会自动旋转
 */

#include <render/renderer.h>
#include <render/logger.h>
#include <render/mesh_loader.h>
#include <render/shader_cache.h>
#include <render/uniform_manager.h>
#include <SDL3/SDL.h>
#include <iostream>
#include <memory>
#include <vector>
#include <cmath>
#include <algorithm>

#ifdef _WIN32
#define NOMINMAX  // 防止 Windows.h 定义 min/max 宏
#include <windows.h>
#endif

using namespace Render;

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

#ifdef _WIN32
    // Windows UTF-8 控制台支持
    SetConsoleOutputCP(CP_UTF8);
#endif

    // 启用日志文件输出
    auto& logger = Logger::GetInstance();
    logger.SetLogToFile(true);
    logger.Info("========================================");
    logger.Info("11 - Model Loader Test");
    logger.Info("========================================");
    logger.Info("日志文件: " + logger.GetCurrentLogFile());
    
    // 初始化渲染器
    Renderer renderer;
    if (!renderer.Initialize("模型加载器测试 - Model Loader Test", 800, 600)) {
        logger.Error("Failed to initialize renderer");
        return -1;
    }
    
    // 设置渲染状态
    auto state = renderer.GetRenderState();
    state->SetDepthTest(true);
    state->SetCullFace(CullFace::Back);
    state->SetClearColor(Color(0.1f, 0.1f, 0.15f, 1.0f));
    
    // 加载着色器（使用 mesh_test 着色器）
    auto& shaderCache = ShaderCache::GetInstance();
    auto shader = shaderCache.LoadShader("mesh_test", "shaders/mesh_test.vert", "shaders/mesh_test.frag");
    if (!shader) {
        logger.Error("Failed to load mesh_test shader");
        return -1;
    }
    
    // 定义可能的模型文件路径
    std::vector<std::string> modelPaths = {
        "models/miku/v4c5.0short.pmx",  // Miku PMX 模型（短发版）
        "models/miku/v4c5.0.pmx",        // Miku PMX 模型（完整版）
        "models/test.obj",
        "models/cube.obj",
        "../models/miku/v4c5.0short.pmx",
        "../models/miku/v4c5.0.pmx",
        "../models/test.obj",
        "../models/cube.obj",
        "../../models/miku/v4c5.0short.pmx",
        "../../models/miku/v4c5.0.pmx",
        "../../models/test.obj"
    };
    
    std::vector<Ref<Mesh>> loadedMeshes;
    std::string usedPath;
    
    // 尝试加载模型文件
    logger.Info("Attempting to load model file...");
    for (const auto& path : modelPaths) {
        logger.Info("Trying: " + path);
        auto meshes = MeshLoader::LoadFromFile(path);
        if (!meshes.empty()) {
            loadedMeshes = std::move(meshes);
            usedPath = path;
            logger.Info("Successfully loaded model from: " + path);
            break;
        }
    }
    
    // 如果没有找到模型文件，创建一个默认的立方体
    if (loadedMeshes.empty()) {
        logger.Warning("No model file found. Creating default cube mesh for demonstration.");
        logger.Info("提示：您可以将 .obj, .fbx, .gltf, .pmx 等模型文件放在 models/ 目录中进行测试");
        logger.Info("提示：当前尝试加载 models/miku/v4c5.0short.pmx 或 v4c5.0.pmx");
        loadedMeshes.push_back(MeshLoader::CreateCube(1.0f, 1.0f, 1.0f));
    } else {
        logger.Info("成功加载模型: " + usedPath);
    }
    
    // 显示加载的网格信息
    logger.Info("========================================");
    logger.Info("加载了 " + std::to_string(loadedMeshes.size()) + " 个网格");
    
    size_t totalVertices = 0;
    size_t totalTriangles = 0;
    for (auto& mesh : loadedMeshes) {
        totalVertices += mesh->GetVertexCount();
        totalTriangles += mesh->GetTriangleCount();
    }
    
    logger.Info("总计顶点: " + std::to_string(totalVertices));
    logger.Info("总计三角形: " + std::to_string(totalTriangles));
    logger.Info("========================================");
    
    // 主循环变量
    bool running = true;
    SDL_Event event;
    float rotationAngle = 0.0f;
    
    Uint64 lastTime = SDL_GetTicks();
    
    logger.Info("进入渲染循环...");
    logger.Info("控制：");
    logger.Info("  ESC 或关闭窗口 - 退出");
    logger.Info("  模型将自动旋转");
    
    // 主循环
    while (running) {
        // 计算帧时间
        Uint64 currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;
        
        // 事件处理
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
        
        // 更新旋转（每秒45度）
        rotationAngle += 45.0f * deltaTime;
        if (rotationAngle > 360.0f) {
            rotationAngle -= 360.0f;
        }
        
        // 开始帧
        renderer.BeginFrame();
        
        // 清屏
        renderer.Clear(true, true, false);
        
        // 使用着色器
        shader->Use();
        
        // 模型矩阵（旋转 + 缩放 + 平移）
        float angleRad = rotationAngle * 3.14159265f / 180.0f;
        
        // 先创建旋转矩阵（绕 Y 轴）
        Matrix4 rotationY = Matrix4::Identity();
        rotationY(0, 0) = std::cos(angleRad);
        rotationY(0, 2) = std::sin(angleRad);
        rotationY(2, 0) = -std::sin(angleRad);
        rotationY(2, 2) = std::cos(angleRad);
        
        // 缩放矩阵（PMX 模型通常使用 0.08-0.12 的缩放）
        float scale = (loadedMeshes.size() > 10) ? 0.08f : 1.0f;
        Matrix4 scaleMatrix = Matrix4::Identity();
        scaleMatrix(0, 0) = scale;
        scaleMatrix(1, 1) = scale;
        scaleMatrix(2, 2) = scale;
        
        // 平移矩阵（将模型中心移到视野中）
        Matrix4 translateMatrix = Matrix4::Identity();
        // PMX 模型通常中心在脚底，向上抬一点
        if (loadedMeshes.size() > 10) {
            translateMatrix(1, 3) = -0.8f;  // 向下移动一点，让头部在视野中
        }
        
        // 组合变换：缩放 -> 旋转 -> 平移
        Matrix4 modelMatrix = translateMatrix * rotationY * scaleMatrix;
        
        // 视图矩阵（相机看向场景中心）
        Matrix4 viewMatrix = Matrix4::Identity();
        // 相机位置：稍微向上，向后
        viewMatrix(1, 3) = 0.5f;   // Y 轴向上 0.5 单位
        viewMatrix(2, 3) = -2.5f;  // Z 轴向后 2.5 单位
        
        // 投影矩阵（透视投影，正确的参数）
        const float fov = 60.0f * 3.14159265f / 180.0f;  // 稍大的视野
        const float aspect = 800.0f / 600.0f;
        const float nearPlane = 0.1f;
        const float farPlane = 100.0f;
        
        const float tanHalfFov = std::tan(fov / 2.0f);
        Matrix4 projMatrix = Matrix4::Zero();
        projMatrix(0, 0) = 1.0f / (aspect * tanHalfFov);
        projMatrix(1, 1) = 1.0f / tanHalfFov;
        projMatrix(2, 2) = -(farPlane + nearPlane) / (farPlane - nearPlane);
        projMatrix(2, 3) = -(2.0f * farPlane * nearPlane) / (farPlane - nearPlane);
        projMatrix(3, 2) = -1.0f;
        
        // MVP 矩阵（所有网格使用相同的变换）
        Matrix4 mvpMatrix = projMatrix * viewMatrix * modelMatrix;
        
        // 设置 uniform（只需设置一次）
        auto* uniformMgr = shader->GetUniformManager();
        uniformMgr->SetMatrix4("uMVP", mvpMatrix);
        
        // 设置物体颜色
        Color objectColor(0.8f, 0.85f, 0.9f, 1.0f);
        uniformMgr->SetColor("uColor", objectColor);
        
        // 设置光照（简单的方向光）
        Vector3 lightDir(-0.3f, -0.8f, -0.5f);
        lightDir.normalize();
        uniformMgr->SetVector3("uLightDir", lightDir);
        
        // 渲染所有加载的网格（PMX 的多个部件会叠加在一起形成完整模型）
        for (size_t i = 0; i < loadedMeshes.size(); i++) {
            auto& mesh = loadedMeshes[i];
            mesh->Draw();
        }
        
        // 解除着色器
        shader->Unuse();
        
        // 结束帧
        renderer.EndFrame();
        renderer.Present();
        
        // 限制帧率（60 FPS）
        SDL_Delay(16);
    }
    
    logger.Info("清理资源...");
    loadedMeshes.clear();
    shader.reset();
    ShaderCache::GetInstance().Clear();
    
    renderer.Shutdown();
    
    logger.Info("========================================");
    logger.Info("模型加载器测试完成");
    logger.Info("========================================");
    
    return 0;
}

