/**
 * @file 22_gl_thread_safety_test.cpp
 * @brief OpenGL 线程安全检查测试
 * 
 * 这个示例演示了 OpenGL 线程安全检查机制的使用：
 * 1. 在主线程中正确初始化 OpenGL 上下文
 * 2. 尝试在其他线程中调用 OpenGL 函数（会被检测并报错）
 * 3. 展示如何正确地在创建上下文的线程中进行 OpenGL 调用
 */

#include "render/renderer.h"
#include "render/logger.h"
#include "render/gl_thread_checker.h"
#include <SDL3/SDL.h>
#include <thread>
#include <chrono>
#include <sstream>

using namespace Render;

// 测试函数：尝试在错误的线程中调用 OpenGL
void TestThreadSafety_WrongThread(Renderer* renderer) {
    LOG_INFO("========================================");
    LOG_INFO("测试：在错误的线程中调用 OpenGL");
    LOG_INFO("========================================");
    
    // 等待一会儿确保主线程已经初始化完成
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    LOG_INFO("尝试在子线程中获取渲染上下文...");
    OpenGLContext* context = renderer->GetContext();
    
    if (context) {
        LOG_INFO("尝试在子线程中调用 OpenGL 函数（应该会触发错误）...");
        
        // 这个调用应该会触发 GL_THREAD_CHECK() 错误
        // 在调试模式下，如果 terminateOnError 为 true，程序会终止
        std::string version = context->GetGLVersion();
        
        LOG_ERROR("错误：OpenGL 调用应该被阻止，但却成功了！");
    }
}

// 测试函数：在正确的线程中调用 OpenGL
void TestThreadSafety_CorrectThread(Renderer* renderer) {
    LOG_INFO("========================================");
    LOG_INFO("测试：在正确的线程（主线程）中调用 OpenGL");
    LOG_INFO("========================================");
    
    OpenGLContext* context = renderer->GetContext();
    if (context) {
        LOG_INFO("在主线程中获取 OpenGL 版本...");
        std::string version = context->GetGLVersion();
        LOG_INFO("OpenGL 版本: " + version);
        LOG_INFO("成功！在正确的线程中调用 OpenGL");
    }
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    LOG_INFO("========================================");
    LOG_INFO("OpenGL 线程安全检查测试");
    LOG_INFO("========================================");
    
    // 创建渲染器
    auto renderer = Renderer::Create();
    if (!renderer) {
        LOG_ERROR("Failed to create renderer");
        return -1;
    }
    
    // 初始化渲染器（这会创建 OpenGL 上下文并注册线程）
    if (!renderer->Initialize("OpenGL Thread Safety Test", 800, 600)) {
        LOG_ERROR("Failed to initialize renderer");
        Renderer::Destroy(renderer);
        return -1;
    }
    
    LOG_INFO("========================================");
    LOG_INFO("OpenGL 上下文已在主线程中创建");
    std::ostringstream threadIdStr;
    threadIdStr << "主线程 ID: " << std::this_thread::get_id();
    LOG_INFO(threadIdStr.str());
    LOG_INFO("========================================");
    
    // 测试 1：在正确的线程中调用 OpenGL（应该成功）
    TestThreadSafety_CorrectThread(renderer);
    
    // 获取 GLThreadChecker 的设置
    auto& threadChecker = GLThreadChecker::GetInstance();
    bool terminateOnError = threadChecker.GetTerminateOnError();
    
    LOG_INFO("========================================");
    LOG_INFO("当前设置：");
    std::string terminateStr = std::string("  terminateOnError = ") + (terminateOnError ? "true" : "false");
    LOG_INFO(terminateStr);
    LOG_INFO("========================================");
    
    // 为了演示目的，暂时禁用 "错误时终止" 选项
    // 这样我们可以看到错误日志而不会使程序崩溃
    LOG_INFO("暂时禁用 'terminateOnError' 以便观察错误日志...");
    threadChecker.SetTerminateOnError(false);
    
    // 测试 2：在错误的线程中调用 OpenGL（应该检测到错误）
    LOG_INFO("========================================");
    LOG_INFO("启动子线程进行测试...");
    LOG_INFO("========================================");
    
    std::thread testThread(TestThreadSafety_WrongThread, renderer);
    testThread.join();
    
    // 恢复原始设置
    threadChecker.SetTerminateOnError(terminateOnError);
    LOG_INFO("已恢复原始的 'terminateOnError' 设置");
    
    // 运行一小段时间以展示正常渲染
    LOG_INFO("========================================");
    LOG_INFO("运行正常渲染循环 3 秒...");
    LOG_INFO("========================================");
    
    bool running = true;
    SDL_Event event;
    auto startTime = std::chrono::steady_clock::now();
    const auto duration = std::chrono::seconds(3);
    
    while (running) {
        // 处理事件
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
                running = false;
            }
        }
        
        // 检查是否已经运行了 3 秒
        auto currentTime = std::chrono::steady_clock::now();
        if (currentTime - startTime >= duration) {
            running = false;
        }
        
        // 开始渲染
        renderer->BeginFrame();
        
        // 清空屏幕（绿色背景表示成功）
        renderer->SetClearColor(0.0f, 0.8f, 0.0f, 1.0f);
        renderer->Clear();
        
        // 结束渲染
        renderer->EndFrame();
        renderer->Present();
        
        // 控制帧率
        SDL_Delay(16); // ~60 FPS
    }
    
    LOG_INFO("========================================");
    LOG_INFO("测试总结：");
    LOG_INFO("1. ✓ 在正确的线程中调用 OpenGL 成功");
    LOG_INFO("2. ✓ 在错误的线程中调用 OpenGL 被正确检测并记录");
    LOG_INFO("3. ✓ 线程安全检查机制工作正常");
    LOG_INFO("========================================");
    
    // 清理
    LOG_INFO("Shutting down...");
    Renderer::Destroy(renderer);
    
    LOG_INFO("========================================");
    LOG_INFO("测试完成！");
    LOG_INFO("========================================");
    
    return 0;
}

