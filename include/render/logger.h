#pragma once

#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <functional>
#include <memory>
#include <atomic>
#include <thread>
#include <cstdarg>
#include <queue>
#include <condition_variable>

namespace Render {

/**
 * @brief 日志级别
 */
enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

/**
 * @brief 日志回调函数类型
 * @param level 日志级别
 * @param message 格式化后的完整日志消息
 */
using LogCallback = std::function<void(LogLevel level, const std::string& message)>;

/**
 * @brief 日志消息结构
 */
struct LogMessage {
    LogLevel level;
    std::string message;
    const char* file;
    int line;
    std::chrono::system_clock::time_point timestamp;
    std::thread::id threadId;
    
    LogMessage(LogLevel lvl, std::string msg, const char* f = nullptr, int l = 0)
        : level(lvl)
        , message(std::move(msg))
        , file(f)
        , line(l)
        , timestamp(std::chrono::system_clock::now())
        , threadId(std::this_thread::get_id()) {}
};

/**
 * @brief 线程安全的日志系统
 * 
 * 特性：
 * - 完全线程安全，避免死锁
 * - 支持控制台和文件输出
 * - 支持格式化日志（类似printf）
 * - 支持控制台颜色输出
 * - 支持显示线程ID
 * - 支持日志回调机制
 * - 支持文件大小轮转
 * - 异步日志队列，避免阻塞和丢失日志
 * - 向后兼容旧API
 */
class Logger {
public:
    static Logger& GetInstance();
    
    // ========== 基本配置（线程安全） ==========
    void SetLogLevel(LogLevel level);
    void SetLogToFile(bool enable, const std::string& filename = "");
    void SetLogToConsole(bool enable);
    void SetLogDirectory(const std::string& directory);
    
    // ========== 新增配置项 ==========
    /**
     * @brief 启用/禁用控制台颜色输出
     * @param enable true启用，false禁用（默认启用）
     */
    void SetColorOutput(bool enable);
    
    /**
     * @brief 启用/禁用线程ID显示
     * @param enable true启用，false禁用（默认禁用）
     */
    void SetShowThreadId(bool enable);
    
    /**
     * @brief 设置日志文件最大大小（字节），超过后自动轮转
     * @param maxSize 最大字节数，0表示不限制（默认0）
     */
    void SetMaxFileSize(size_t maxSize);
    
    /**
     * @brief 设置日志回调函数
     * @param callback 回调函数，nullptr表示取消回调
     */
    void SetLogCallback(LogCallback callback);
    
    /**
     * @brief 启用/禁用异步日志
     * @param enable true启用异步（默认），false禁用（同步模式）
     * @note 更改此设置时会自动刷新队列
     */
    void SetAsyncLogging(bool enable);
    
    /**
     * @brief 刷新日志队列，确保所有日志都已写入
     * @note 此方法会阻塞直到队列为空
     */
    void Flush();
    
    /**
     * @brief 获取当前队列中的日志数量
     */
    size_t GetQueueSize() const;
    
    // ========== 基本日志方法（向后兼容） ==========
    void Log(LogLevel level, const std::string& message);
    void Debug(const std::string& message);
    void Info(const std::string& message);
    void Warning(const std::string& message);
    void Error(const std::string& message);
    
    // ========== 格式化日志方法（新增） ==========
    /**
     * @brief 格式化日志输出
     * @param level 日志级别
     * @param format 格式化字符串（printf风格）
     * @param ... 可变参数
     */
    void LogFormat(LogLevel level, const char* format, ...);
    void DebugFormat(const char* format, ...);
    void InfoFormat(const char* format, ...);
    void WarningFormat(const char* format, ...);
    void ErrorFormat(const char* format, ...);
    
    /**
     * @brief 带源文件位置的日志输出
     * @param level 日志级别
     * @param file 源文件名
     * @param line 行号
     * @param message 日志消息
     */
    void LogWithLocation(LogLevel level, const char* file, int line, const std::string& message);
    
    /**
     * @brief 获取当前日志文件路径（线程安全）
     */
    std::string GetCurrentLogFile() const;
    
private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    // 辅助方法
    std::string GetTimestamp();
    std::string GetTimestamp(const std::chrono::system_clock::time_point& timePoint);
    std::string GetFileTimestamp();
    std::string LevelToString(LogLevel level);
    std::string GetColorCode(LogLevel level);
    std::string GetResetColor();
    std::string GetThreadIdString(const std::thread::id& threadId);
    void CreateLogDirectory();
    std::string GenerateLogFileName();
    void CheckAndRotateLogFile();
    std::string FormatString(const char* format, va_list args);
    
    // 核心日志输出（已经在锁内）
    void LogInternal(LogLevel level, const std::string& message, const char* file = nullptr, int line = 0);
    
    // 异步日志相关
    void StartAsyncThread();
    void StopAsyncThread();
    void AsyncWorker();
    void ProcessLogMessage(const LogMessage& logMsg);
    void EnqueueLog(LogLevel level, const std::string& message, const char* file = nullptr, int line = 0);
    
    // 配置变量（使用atomic保证原子性）
    std::atomic<LogLevel> m_logLevel;
    std::atomic<bool> m_logToConsole;
    std::atomic<bool> m_logToFile;
    std::atomic<bool> m_colorOutput;
    std::atomic<bool> m_showThreadId;
    std::atomic<size_t> m_maxFileSize;
    std::atomic<bool> m_asyncLogging;
    
    // 需要锁保护的资源
    std::ofstream m_fileStream;
    std::string m_logDirectory;
    std::string m_currentLogFile;
    size_t m_currentFileSize;
    LogCallback m_callback;
    
    // 异步日志队列
    std::queue<LogMessage> m_logQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCV;
    
    // 后台写入线程
    std::thread m_asyncThread;
    std::atomic<bool> m_stopAsyncThread;
    
    // 互斥锁（使用mutable允许在const方法中使用）
    mutable std::mutex m_mutex;
};

// ========== 便捷宏（向后兼容） ==========
#define LOG_DEBUG(msg) Render::Logger::GetInstance().Debug(msg)
#define LOG_INFO(msg) Render::Logger::GetInstance().Info(msg)
#define LOG_WARNING(msg) Render::Logger::GetInstance().Warning(msg)
#define LOG_ERROR(msg) Render::Logger::GetInstance().Error(msg)

// ========== 格式化日志宏（新增） ==========
#define LOG_DEBUG_F(fmt, ...) Render::Logger::GetInstance().DebugFormat(fmt, ##__VA_ARGS__)
#define LOG_INFO_F(fmt, ...) Render::Logger::GetInstance().InfoFormat(fmt, ##__VA_ARGS__)
#define LOG_WARNING_F(fmt, ...) Render::Logger::GetInstance().WarningFormat(fmt, ##__VA_ARGS__)
#define LOG_ERROR_F(fmt, ...) Render::Logger::GetInstance().ErrorFormat(fmt, ##__VA_ARGS__)

// ========== 带源文件位置的日志宏（新增） ==========
#define LOG_DEBUG_LOC(msg) Render::Logger::GetInstance().LogWithLocation(Render::LogLevel::Debug, __FILE__, __LINE__, msg)
#define LOG_INFO_LOC(msg) Render::Logger::GetInstance().LogWithLocation(Render::LogLevel::Info, __FILE__, __LINE__, msg)
#define LOG_WARNING_LOC(msg) Render::Logger::GetInstance().LogWithLocation(Render::LogLevel::Warning, __FILE__, __LINE__, msg)
#define LOG_ERROR_LOC(msg) Render::Logger::GetInstance().LogWithLocation(Render::LogLevel::Error, __FILE__, __LINE__, msg)

} // namespace Render

