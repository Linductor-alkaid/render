#pragma once

#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <mutex>
#include <chrono>
#include <iomanip>

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
 * @brief 简单的日志系统
 */
class Logger {
public:
    static Logger& GetInstance();
    
    void SetLogLevel(LogLevel level);
    void SetLogToFile(bool enable, const std::string& filename = "");
    void SetLogToConsole(bool enable);
    void SetLogDirectory(const std::string& directory);
    
    void Log(LogLevel level, const std::string& message);
    void Debug(const std::string& message);
    void Info(const std::string& message);
    void Warning(const std::string& message);
    void Error(const std::string& message);
    
    /**
     * @brief 获取当前日志文件路径
     */
    std::string GetCurrentLogFile() const { return m_currentLogFile; }
    
private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    std::string GetTimestamp();
    std::string GetFileTimestamp();
    std::string LevelToString(LogLevel level);
    void CreateLogDirectory();
    std::string GenerateLogFileName();
    
    LogLevel m_logLevel;
    bool m_logToConsole;
    bool m_logToFile;
    std::ofstream m_fileStream;
    std::mutex m_mutex;
    std::string m_logDirectory;
    std::string m_currentLogFile;
};

// 便捷宏
#define LOG_DEBUG(msg) Render::Logger::GetInstance().Debug(msg)
#define LOG_INFO(msg) Render::Logger::GetInstance().Info(msg)
#define LOG_WARNING(msg) Render::Logger::GetInstance().Warning(msg)
#define LOG_ERROR(msg) Render::Logger::GetInstance().Error(msg)

} // namespace Render

