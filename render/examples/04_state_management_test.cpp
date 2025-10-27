/**
 * @file 04_state_management_test.cpp
 * @brief 测试 RenderState 的状态管理功能
 * 
 * 本示例演示：
 * 1. VAO/VBO/EBO 绑定管理
 * 2. 着色器程序绑定管理
 * 3. 纹理绑定管理（程序生成纹理）
 * 4. 状态缓存和优化
 */

#include <render/renderer.h>
#include <render/shader.h>
#include <render/shader_cache.h>
#include <render/logger.h>
#include <glad/glad.h>
#include <iostream>
#include <vector>
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace Render;

// 全局资源
GLuint vao1, vao2;
GLuint vbo1, vbo2;
GLuint ebo1, ebo2;
GLuint texture1, texture2;
Ref<Shader> shader1, shader2;

/**
 * @brief 创建程序生成的纹理
 */
GLuint CreateProceduralTexture(int width, int height, const Color& baseColor) {
    std::vector<unsigned char> data(width * height * 4);
    
    // 生成棋盘格纹理
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int index = (y * width + x) * 4;
            bool isWhite = ((x / 16) + (y / 16)) % 2 == 0;
            
            if (isWhite) {
                data[index + 0] = static_cast<unsigned char>(baseColor.r * 255);
                data[index + 1] = static_cast<unsigned char>(baseColor.g * 255);
                data[index + 2] = static_cast<unsigned char>(baseColor.b * 255);
            } else {
                data[index + 0] = static_cast<unsigned char>(baseColor.r * 128);
                data[index + 1] = static_cast<unsigned char>(baseColor.g * 128);
                data[index + 2] = static_cast<unsigned char>(baseColor.b * 128);
            }
            data[index + 3] = 255;
        }
    }
    
    GLuint textureId;
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, 
                 GL_RGBA, GL_UNSIGNED_BYTE, data.data());
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    return textureId;
}

/**
 * @brief 创建三角形几何体（仅位置）
 */
void CreateTriangle(GLuint& vao, GLuint& vbo, GLuint& ebo, float offsetX) {
    // 顶点数据：仅位置 (x, y, z) - NDC 坐标系从 -1 到 1
    // 使用较小的三角形尺寸，确保完全在屏幕内
    float size = 0.4f;
    float vertices[] = {
        -size + offsetX, -size, 0.0f,  // 左下
         size + offsetX, -size, 0.0f,  // 右下
         0.0f + offsetX,  size, 0.0f   // 顶部
    };
    
    unsigned int indices[] = {
        0, 1, 2
    };
    
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    
    glBindVertexArray(vao);
    
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    
    // 位置属性 (location = 0) - solid_color.vert 只需要位置
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glBindVertexArray(0);
}

/**
 * @brief 测试状态管理
 */
void TestStateManagement(RenderState* state) {
    Logger::GetInstance().Info("========================================");
    Logger::GetInstance().Info("开始测试状态管理功能");
    Logger::GetInstance().Info("========================================");
    
    // 测试 1: 着色器程序绑定
    Logger::GetInstance().Info("\n[测试 1] 着色器程序绑定管理");
    state->UseProgram(shader1->GetProgramID());
    Logger::GetInstance().Info("绑定 shader1 (ID: " + std::to_string(shader1->GetProgramID()) + ")");
    Logger::GetInstance().Info("当前程序: " + std::to_string(state->GetCurrentProgram()));
    
    // 重复绑定（应该被缓存跳过）
    state->UseProgram(shader1->GetProgramID());
    Logger::GetInstance().Info("重复绑定 shader1（应该被缓存）");
    
    state->UseProgram(shader2->GetProgramID());
    Logger::GetInstance().Info("切换到 shader2 (ID: " + std::to_string(shader2->GetProgramID()) + ")");
    Logger::GetInstance().Info("当前程序: " + std::to_string(state->GetCurrentProgram()));
    
    // 测试 2: 纹理绑定
    Logger::GetInstance().Info("\n[测试 2] 纹理绑定管理");
    state->BindTexture(0, texture1);
    Logger::GetInstance().Info("纹理单元 0 绑定 texture1 (ID: " + std::to_string(texture1) + ")");
    Logger::GetInstance().Info("纹理单元 0 当前纹理: " + std::to_string(state->GetBoundTexture(0)));
    
    state->BindTexture(1, texture2);
    Logger::GetInstance().Info("纹理单元 1 绑定 texture2 (ID: " + std::to_string(texture2) + ")");
    Logger::GetInstance().Info("纹理单元 1 当前纹理: " + std::to_string(state->GetBoundTexture(1)));
    
    // 重复绑定
    state->BindTexture(0, texture1);
    Logger::GetInstance().Info("重复绑定 texture1 到单元 0（应该被缓存）");
    
    // 测试 3: VAO/VBO 绑定
    Logger::GetInstance().Info("\n[测试 3] VAO/VBO 绑定管理");
    state->BindVertexArray(vao1);
    Logger::GetInstance().Info("绑定 VAO1 (ID: " + std::to_string(vao1) + ")");
    Logger::GetInstance().Info("当前 VAO: " + std::to_string(state->GetBoundVertexArray()));
    
    state->BindBuffer(BufferTarget::ArrayBuffer, vbo1);
    Logger::GetInstance().Info("绑定 VBO1 (ID: " + std::to_string(vbo1) + ")");
    Logger::GetInstance().Info("当前 VBO: " + std::to_string(state->GetBoundBuffer(BufferTarget::ArrayBuffer)));
    
    state->BindVertexArray(vao2);
    Logger::GetInstance().Info("切换到 VAO2 (ID: " + std::to_string(vao2) + ")");
    Logger::GetInstance().Info("当前 VAO: " + std::to_string(state->GetBoundVertexArray()));
    
    Logger::GetInstance().Info("\n========================================");
    Logger::GetInstance().Info("状态管理测试完成！");
    Logger::GetInstance().Info("========================================\n");
}

/**
 * @brief 渲染场景
 */
void RenderScene(Renderer& renderer) {
    RenderState* state = renderer.GetRenderState();
    
    renderer.BeginFrame();
    renderer.Clear(true, true);
    
    // 获取窗口尺寸
    int width = renderer.GetWidth();
    int height = renderer.GetHeight();
    
    // 设置变换矩阵（2D 正交投影）
    Matrix4 model = Matrix4::Identity();
    Matrix4 view = Matrix4::Identity();
    
    // 设置正交投影矩阵（从 -1 到 1 的 NDC 空间）
    Matrix4 projection = Matrix4::Identity();
    
    GLenum err;
    
    // ========================================================================
    // 渲染第一个三角形（左侧，红色纹理）
    // ========================================================================
    
    // 1. 绑定着色器程序
    state->UseProgram(shader1->GetProgramID());
    
    // 2. 设置变换矩阵
    shader1->GetUniformManager()->SetMatrix4("model", model);
    shader1->GetUniformManager()->SetMatrix4("view", view);
    shader1->GetUniformManager()->SetMatrix4("projection", projection);
    
    // 3. 设置颜色（solid_color 着色器只需要 color uniform）
    shader1->GetUniformManager()->SetVector4("color", Vector4(1.0f, 0.0f, 0.0f, 1.0f));  // 纯红色
    
    // 5. 设置渲染状态
    state->SetDepthTest(false);  // 2D 不需要深度测试
    state->SetBlendMode(BlendMode::Alpha);
    
    // 6. 绑定 VAO 并渲染
    state->BindVertexArray(vao1);
    
    // 调试：检查绑定状态
    static bool firstRender = true;
    if (firstRender) {
        Logger::GetInstance().Info("=== 第一帧渲染调试 ===");
        Logger::GetInstance().Info("窗口尺寸: " + std::to_string(width) + "x" + std::to_string(height));
        Logger::GetInstance().Info("当前程序 ID: " + std::to_string(state->GetCurrentProgram()));
        Logger::GetInstance().Info("当前 VAO: " + std::to_string(state->GetBoundVertexArray()));
        Logger::GetInstance().Info("准备绘制三角形1 (模型矩阵: Identity, 投影矩阵: Identity)...");
        Logger::GetInstance().Info("注意: 使用 Identity 矩阵意味着顶点在 NDC [-1,1] 范围内");
        firstRender = false;
    }
    
    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, 0);
    
    // 检查 OpenGL 错误
    err = glGetError();
    if (err != GL_NO_ERROR) {
        Logger::GetInstance().Error("OpenGL Error after triangle 1: " + std::to_string(err));
    }
    
    // ========================================================================
    // 渲染第二个三角形（右侧，蓝色纹理）
    // ========================================================================
    
    // 1. 切换着色器（演示程序切换）
    state->UseProgram(shader2->GetProgramID());
    
    // 2. 设置变换矩阵
    shader2->GetUniformManager()->SetMatrix4("model", model);
    shader2->GetUniformManager()->SetMatrix4("view", view);
    shader2->GetUniformManager()->SetMatrix4("projection", projection);
    
    // 3. 设置颜色（solid_color 着色器只需要 color uniform）
    shader2->GetUniformManager()->SetVector4("color", Vector4(0.0f, 0.0f, 1.0f, 1.0f));  // 纯蓝色
    
    // 5. 渲染状态保持不变（展示状态缓存）
    state->SetBlendMode(BlendMode::Alpha);  // 重复设置，应该被缓存
    
    // 6. 切换 VAO 并渲染
    state->BindVertexArray(vao2);
    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, 0);
    
    // 检查 OpenGL 错误
    err = glGetError();
    if (err != GL_NO_ERROR) {
        Logger::GetInstance().Error("OpenGL Error after triangle 2: " + std::to_string(err));
    }
    
    // ========================================================================
    // 清理（可选，展示解绑）
    // ========================================================================
    state->BindVertexArray(0);
    state->UnbindTexture(0);
    
    renderer.EndFrame();
}

/**
 * @brief 初始化资源
 */
bool InitializeResources(Renderer& renderer) {
    auto& cache = ShaderCache::GetInstance();
    
    // 1. 加载着色器（使用简单的 solid_color 着色器）
    Logger::GetInstance().Info("加载着色器...");
    shader1 = cache.LoadShader("solid_triangle_1", 
                               "shaders/solid_color.vert", 
                               "shaders/solid_color.frag");
    if (!shader1) {
        Logger::GetInstance().Error("无法加载 shader1");
        return false;
    }
    
    shader2 = cache.LoadShader("solid_triangle_2", 
                               "shaders/solid_color.vert", 
                               "shaders/solid_color.frag");
    if (!shader2) {
        Logger::GetInstance().Error("无法加载 shader2");
        return false;
    }
    
    Logger::GetInstance().Info("着色器加载成功");
    
    // 2. 创建纹理
    Logger::GetInstance().Info("创建程序生成纹理...");
    texture1 = CreateProceduralTexture(128, 128, Color::Red());
    texture2 = CreateProceduralTexture(128, 128, Color::Blue());
    std::string texInfo = "纹理创建成功 (ID: " + std::to_string(texture1) + ", " + std::to_string(texture2) + ")";
    Logger::GetInstance().Info(texInfo);
    
    // 3. 创建几何体
    Logger::GetInstance().Info("创建几何体...");
    CreateTriangle(vao1, vbo1, ebo1, -0.3f);  // 左侧三角形（小偏移量）
    CreateTriangle(vao2, vbo2, ebo2,  0.3f);  // 右侧三角形（小偏移量）
    Logger::GetInstance().Info("几何体创建成功");
    std::string vaoInfo = "VAO1 ID: " + std::to_string(vao1) + ", VAO2 ID: " + std::to_string(vao2);
    Logger::GetInstance().Info(vaoInfo);
    
    return true;
}

/**
 * @brief 清理资源
 */
void CleanupResources() {
    Logger::GetInstance().Info("清理资源...");
    
    glDeleteVertexArrays(1, &vao1);
    glDeleteVertexArrays(1, &vao2);
    glDeleteBuffers(1, &vbo1);
    glDeleteBuffers(1, &vbo2);
    glDeleteBuffers(1, &ebo1);
    glDeleteBuffers(1, &ebo2);
    glDeleteTextures(1, &texture1);
    glDeleteTextures(1, &texture2);
    
    Logger::GetInstance().Info("资源清理完成");
}

int main(int argc, char* argv[]) {
    // 设置控制台输出为 UTF-8（Windows）
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    
    // 启用文件日志
    Logger::GetInstance().SetLogToFile(true);
    Logger::GetInstance().Info("Log file: " + Logger::GetInstance().GetCurrentLogFile());
    
    // 初始化渲染器
    Renderer renderer;
    if (!renderer.Initialize("04 - 状态管理测试", 1280, 720)) {
        Logger::GetInstance().Error("渲染器初始化失败");
        return -1;
    }
    
    Logger::GetInstance().Info("========================================");
    Logger::GetInstance().Info("状态管理测试示例");
    Logger::GetInstance().Info("========================================");
    Logger::GetInstance().Info("本示例展示：");
    Logger::GetInstance().Info("  1. 着色器程序绑定和缓存");
    Logger::GetInstance().Info("  2. 纹理绑定管理（多纹理单元）");
    Logger::GetInstance().Info("  3. VAO/VBO 绑定管理");
    Logger::GetInstance().Info("  4. 状态切换优化");
    Logger::GetInstance().Info("========================================");
    
    // 初始化资源
    if (!InitializeResources(renderer)) {
        Logger::GetInstance().Error("资源初始化失败");
        renderer.Shutdown();
        return -1;
    }
    
    // 运行状态管理测试
    TestStateManagement(renderer.GetRenderState());
    
    // 调试：打印着色器 uniform 信息
    Logger::GetInstance().Info("");
    Logger::GetInstance().Info("=== 着色器 Uniform 调试信息 ===");
    Logger::GetInstance().Info("Shader1 Uniforms:");
    shader1->GetUniformManager()->PrintUniformInfo();
    Logger::GetInstance().Info("");
    Logger::GetInstance().Info("Shader2 Uniforms:");
    shader2->GetUniformManager()->PrintUniformInfo();
    Logger::GetInstance().Info("================================");
    
    // 主循环
    Logger::GetInstance().Info("");
    Logger::GetInstance().Info("进入渲染循环...");
    Logger::GetInstance().Info("窗口应该显示两个三角形：");
    Logger::GetInstance().Info("  - 左侧：纯红色三角形 (使用 solid_color 着色器)");
    Logger::GetInstance().Info("  - 右侧：纯蓝色三角形 (使用 solid_color 着色器)");
    Logger::GetInstance().Info("如果看不到内容，请检查：");
    Logger::GetInstance().Info("  1. 三角形是否被裁剪");
    Logger::GetInstance().Info("  2. uniform 是否正确设置");
    Logger::GetInstance().Info("  3. VAO/VBO 是否正确绑定");
    Logger::GetInstance().Info("控制：");
    Logger::GetInstance().Info("  - ESC: 退出");
    Logger::GetInstance().Info("  - T: 手动触发状态测试");
    
    bool running = true;
    
    while (running) {
        // 处理事件
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) {
                    running = false;
                } else if (event.key.key == SDLK_T) {
                    // 按 T 键手动触发状态测试
                    Logger::GetInstance().Info("");
                    Logger::GetInstance().Info("=== 手动触发状态管理测试 ===");
                    TestStateManagement(renderer.GetRenderState());
                }
            }
        }
        
        // 渲染场景
        RenderScene(renderer);
        
        // 呈现渲染结果（交换缓冲区）
        renderer.Present();
    }
    
    // 清理
    CleanupResources();
    renderer.Shutdown();
    
    Logger::GetInstance().Info("");
    Logger::GetInstance().Info("程序正常退出");
    return 0;
}

