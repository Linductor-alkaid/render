#include "render/logger.h"
#include <thread>
#include <chrono>

using namespace Render;

// 自定义日志回调示例
void MyLogCallback(LogLevel level, const std::string& message) {
    // 可以在这里将日志发送到远程服务器、数据库等
    // 示例：只统计错误日志数量
    static int errorCount = 0;
    if (level == LogLevel::Error) {
        errorCount++;
        // 可以在这里做其他处理，比如发送告警
    }
}

void ThreadFunction(int threadId) {
    for (int i = 0; i < 5; i++) {
        LOG_INFO_F("线程 %d 执行第 %d 次迭代", threadId, i);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int main() {
    Logger& logger = Logger::GetInstance();
    
    // ========== 基本配置 ==========
    logger.SetLogLevel(LogLevel::Debug);
    logger.SetLogToConsole(true);
    logger.SetLogToFile(true, "test.log");
    
    // ========== 新功能配置 ==========
    logger.SetColorOutput(true);      // 启用彩色输出
    logger.SetShowThreadId(true);     // 显示线程ID
    logger.SetMaxFileSize(1024 * 10); // 设置日志文件最大10KB（用于测试轮转）
    logger.SetLogCallback(MyLogCallback); // 设置回调函数
    
    std::cout << "\n========== 基本日志测试 ==========" << std::endl;
    
    // ========== 测试基本日志 ==========
    LOG_DEBUG("这是一条调试信息");
    LOG_INFO("这是一条普通信息");
    LOG_WARNING("这是一条警告信息");
    LOG_ERROR("这是一条错误信息");
    
    std::cout << "\n========== 格式化日志测试 ==========" << std::endl;
    
    // ========== 测试格式化日志 ==========
    int x = 42;
    float y = 3.14159f;
    std::string name = "渲染引擎";
    
    LOG_DEBUG_F("调试: x=%d, y=%.2f", x, y);
    LOG_INFO_F("信息: 名称=%s, 版本=%d.%d.%d", name.c_str(), 1, 0, 0);
    LOG_WARNING_F("警告: 内存使用率 %.1f%%", 75.5);
    LOG_ERROR_F("错误: 无法加载纹理 '%s'，错误代码: %d", "texture.png", -1);
    
    std::cout << "\n========== 带位置信息的日志测试 ==========" << std::endl;
    
    // ========== 测试带源文件位置的日志 ==========
    LOG_DEBUG_LOC("调试信息 - 带源文件位置");
    LOG_INFO_LOC("普通信息 - 带源文件位置");
    LOG_WARNING_LOC("警告信息 - 带源文件位置");
    LOG_ERROR_LOC("错误信息 - 带源文件位置");
    
    std::cout << "\n========== 多线程日志测试 ==========" << std::endl;
    
    // ========== 测试多线程安全性 ==========
    std::thread t1(ThreadFunction, 1);
    std::thread t2(ThreadFunction, 2);
    std::thread t3(ThreadFunction, 3);
    
    t1.join();
    t2.join();
    t3.join();
    
    std::cout << "\n========== 性能测试 ==========" << std::endl;
    
    // ========== 性能测试 ==========
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; i++) {
        LOG_INFO_F("性能测试日志 #%d", i);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    LOG_INFO_F("写入1000条日志耗时: %lld ms", duration.count());
    
    std::cout << "\n========== 测试完成 ==========" << std::endl;
    LOG_INFO_F("当前日志文件: %s", logger.GetCurrentLogFile().c_str());
    
    return 0;
}

