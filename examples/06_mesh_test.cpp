/**
 * @file 06_mesh_test.cpp
 * @brief 测试 Mesh 和 MeshLoader 功能
 * 
 * 本示例演示：
 * 1. 使用 MeshLoader 创建各种几何形状
 * 2. Mesh 类的基本使用
 * 3. 网格渲染和变换
 * 4. 多个网格的场景渲染
 * 
 * 控制：
 * - 空格键/左右箭头：切换网格
 * - W：切换线框模式（调试用）
 * - C：切换背面剔除（调试用）
 * - ESC：退出
 */

#include <render/renderer.h>
#include <render/shader.h>
#include <render/shader_cache.h>
#include <render/mesh.h>
#include <render/mesh_loader.h>
#include <render/logger.h>
#include <glad/glad.h>
#include <iostream>
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace Render;

// 全局资源
Ref<Shader> shader;
std::vector<Ref<Mesh>> meshes;
int currentMeshIndex = 0;
float rotationAngle = 0.0f;
bool wireframeMode = false;
bool disableCulling = false;

// 网格名称
std::vector<std::string> meshNames = {
    "立方体 (Cube)",
    "球体 (Sphere)",
    "圆柱体 (Cylinder)",
    "圆锥体 (Cone)",
    "平面 (Plane)",
    "圆环 (Torus)",
    "胶囊体 (Capsule)",
    "四边形 (Quad)",
    "三角形 (Triangle)",
    "圆形 (Circle)"
};

/**
 * @brief 初始化场景
 */
bool InitScene(Renderer& renderer) {
    Logger::GetInstance().Info("=== 初始化网格测试场景 ===");
    
    // 加载着色器（使用网格测试专用着色器）
    shader = ShaderCache::GetInstance().LoadShader("mesh_test", "shaders/mesh_test.vert", "shaders/mesh_test.frag");
    if (!shader) {
        Logger::GetInstance().Error("Failed to load shader");
        return false;
    }
    
    // 创建各种几何形状
    Logger::GetInstance().Info("创建几何形状...");
    
    // 1. 立方体
    meshes.push_back(MeshLoader::CreateCube(1.0f, 1.0f, 1.0f, Color::White()));
    
    // 2. 球体
    meshes.push_back(MeshLoader::CreateSphere(0.5f, 32, 16, Color::White()));
    
    // 3. 圆柱体
    meshes.push_back(MeshLoader::CreateCylinder(0.4f, 0.4f, 1.0f, 32, Color::White()));
    
    // 4. 圆锥体
    meshes.push_back(MeshLoader::CreateCone(0.5f, 1.0f, 32, Color::White()));
    
    // 5. 平面
    meshes.push_back(MeshLoader::CreatePlane(1.5f, 1.5f, 2, 2, Color::White()));
    
    // 6. 圆环
    meshes.push_back(MeshLoader::CreateTorus(0.5f, 0.2f, 32, 16, Color::White()));
    
    // 7. 胶囊体
    meshes.push_back(MeshLoader::CreateCapsule(0.3f, 0.6f, 32, 8, Color::White()));
    
    // 8. 四边形
    meshes.push_back(MeshLoader::CreateQuad(1.0f, 1.0f, Color::White()));
    
    // 9. 三角形
    meshes.push_back(MeshLoader::CreateTriangle(1.0f, Color::White()));
    
    // 10. 圆形
    meshes.push_back(MeshLoader::CreateCircle(0.5f, 32, Color::White()));
    
    Logger::GetInstance().Info("创建了 " + std::to_string(meshes.size()) + " 个网格");
    
    // 设置渲染状态
    auto state = renderer.GetRenderState();
    state->SetDepthTest(true);
    state->SetCullFace(CullFace::Back);
    state->SetClearColor(Color(0.1f, 0.1f, 0.15f, 1.0f));
    
    Logger::GetInstance().Info("场景初始化完成");
    Logger::GetInstance().Info("控制：空格/左右箭头=切换网格, W=线框模式, C=背面剔除, ESC=退出");
    
    return true;
}

/**
 * @brief 更新场景
 */
void UpdateScene(float deltaTime) {
    // 自动旋转
    rotationAngle += deltaTime * 45.0f;  // 每秒旋转 45 度
    if (rotationAngle >= 360.0f) {
        rotationAngle -= 360.0f;
    }
}

/**
 * @brief 渲染场景
 */
void RenderScene(Renderer& renderer) {
    // 清屏
    renderer.Clear(true, true, false);
    
    // 对于平面图形（Quad, Triangle, Circle），需要禁用背面剔除
    auto state = renderer.GetRenderState();
    if (disableCulling || (currentMeshIndex >= 7 && currentMeshIndex <= 9)) {
        // 索引 7=Quad, 8=Triangle, 9=Circle 是单面的
        // 或者用户按 C 键禁用剔除
        state->SetCullFace(CullFace::None);
    } else {
        state->SetCullFace(CullFace::Back);
    }
    
    // 设置线框模式（用于调试）
    if (wireframeMode) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    
    // 使用着色器
    shader->Use();
    
    // 创建变换矩阵
    Uint64 time = SDL_GetTicks();
    
    // 模型矩阵（旋转）
    float angleRad = rotationAngle * 3.14159265f / 180.0f;
    Matrix4 modelMatrix = Matrix4::Identity();
    
    // 绕 Y 轴旋转
    modelMatrix(0, 0) = std::cos(angleRad);
    modelMatrix(0, 2) = std::sin(angleRad);
    modelMatrix(2, 0) = -std::sin(angleRad);
    modelMatrix(2, 2) = std::cos(angleRad);
    
    // 额外绕 X 轴旋转一点，方便观察
    float tiltAngle = 20.0f * 3.14159265f / 180.0f;
    Matrix4 tiltMatrix = Matrix4::Identity();
    tiltMatrix(1, 1) = std::cos(tiltAngle);
    tiltMatrix(1, 2) = -std::sin(tiltAngle);
    tiltMatrix(2, 1) = std::sin(tiltAngle);
    tiltMatrix(2, 2) = std::cos(tiltAngle);
    
    modelMatrix = tiltMatrix * modelMatrix;
    
    // 视图矩阵（相机在 Z 轴正方向，看向原点）
    Matrix4 viewMatrix = Matrix4::Identity();
    viewMatrix(2, 3) = -3.0f;  // 相机向后移动 3 个单位
    
    // 投影矩阵（透视投影）
    const float fov = 45.0f * 3.14159265f / 180.0f;
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
    
    // MVP 矩阵
    Matrix4 mvpMatrix = projMatrix * viewMatrix * modelMatrix;
    
    // 设置 uniform
    auto uniformMgr = shader->GetUniformManager();
    uniformMgr->SetMatrix4("uMVP", mvpMatrix);
    
    // 设置物体颜色（根据索引变化）
    float hue = static_cast<float>(currentMeshIndex) / static_cast<float>(meshes.size());
    Color objectColor(
        std::abs(std::sin(hue * 6.28f)),
        std::abs(std::sin((hue + 0.33f) * 6.28f)),
        std::abs(std::sin((hue + 0.67f) * 6.28f)),
        1.0f
    );
    uniformMgr->SetColor("uColor", objectColor);
    
    // 设置光照（简单的方向光）
    Vector3 lightDir(-0.5f, -1.0f, -0.3f);
    lightDir.normalize();
    uniformMgr->SetVector3("uLightDir", lightDir);
    
    // 渲染当前网格
    if (currentMeshIndex < static_cast<int>(meshes.size())) {
        meshes[currentMeshIndex]->Draw();
    }
    
    shader->Unuse();
}

/**
 * @brief 处理输入
 */
void HandleInput(SDL_Event& event, bool& running) {
    if (event.type == SDL_EVENT_KEY_DOWN) {
        switch (event.key.key) {
            case SDLK_ESCAPE:
                running = false;
                break;
                
            case SDLK_SPACE:
                // 切换网格
                currentMeshIndex = (currentMeshIndex + 1) % static_cast<int>(meshes.size());
                Logger::GetInstance().Info("切换到: " + meshNames[currentMeshIndex]);
                break;
                
            case SDLK_LEFT:
                // 上一个网格
                currentMeshIndex = (currentMeshIndex - 1 + static_cast<int>(meshes.size())) % static_cast<int>(meshes.size());
                Logger::GetInstance().Info("切换到: " + meshNames[currentMeshIndex]);
                break;
                
            case SDLK_RIGHT:
                // 下一个网格
                currentMeshIndex = (currentMeshIndex + 1) % static_cast<int>(meshes.size());
                Logger::GetInstance().Info("切换到: " + meshNames[currentMeshIndex]);
                break;
                
            case SDLK_W:
                // 切换线框模式
                wireframeMode = !wireframeMode;
                Logger::GetInstance().Info(wireframeMode ? "线框模式" : "填充模式");
                break;
                
            case SDLK_C:
                // 切换背面剔除
                disableCulling = !disableCulling;
                Logger::GetInstance().Info(disableCulling ? "禁用背面剔除" : "启用背面剔除");
                break;
        }
    }
}

/**
 * @brief 清理资源
 */
void Cleanup() {
    Logger::GetInstance().Info("清理资源...");
    meshes.clear();
    shader.reset();
    ShaderCache::GetInstance().Clear();
}

/**
 * @brief 主函数
 */
int main(int argc, char* argv[]) {
#ifdef _WIN32
    // Windows UTF-8 控制台支持
    SetConsoleOutputCP(CP_UTF8);
#endif

    // 启用日志文件输出
    Logger::GetInstance().SetLogToFile(true);
    Logger::GetInstance().Info("=== 网格系统测试 (06_mesh_test) ===");
    Logger::GetInstance().Info("日志文件: " + Logger::GetInstance().GetCurrentLogFile());
    
    // 创建渲染器
    Renderer renderer;
    if (!renderer.Initialize("网格系统测试 - Mesh Test", 800, 600)) {
        Logger::GetInstance().Error("Failed to initialize renderer");
        return -1;
    }
    
    // 初始化场景
    if (!InitScene(renderer)) {
        Logger::GetInstance().Error("Failed to initialize scene");
        renderer.Shutdown();
        return -1;
    }
    
    Logger::GetInstance().Info("当前网格: " + meshNames[currentMeshIndex]);
    
    // 主循环
    bool running = true;
    SDL_Event event;
    
    Uint64 lastTime = SDL_GetTicks();
    
    while (running) {
        // 计算帧时间
        Uint64 currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;
        
        // 处理事件
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            HandleInput(event, running);
        }
        
        // 更新场景
        UpdateScene(deltaTime);
        
        // 渲染场景
        renderer.BeginFrame();
        RenderScene(renderer);
        renderer.EndFrame();
        
        // 呈现
        renderer.Present();
        
        // 限制帧率（60 FPS）
        SDL_Delay(16);
    }
    
    // 清理
    Cleanup();
    renderer.Shutdown();
    
    Logger::GetInstance().Info("程序正常退出");
    return 0;
}

