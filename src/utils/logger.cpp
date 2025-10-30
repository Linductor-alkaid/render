#include "render/logger.h"
#include <filesystem>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace Render {

Logger& Logger::GetInstance() {
    static Logger instance;
    return instance;
}

Logger::Logger() 
    : m_logLevel(LogLevel::Debug)
    , m_logToConsole(true)
    , m_logToFile(false)
    , m_colorOutput(true)
    , m_showThreadId(false)
    , m_maxFileSize(0)
    , m_logDirectory("logs")
    , m_currentFileSize(0) {
    
#ifdef _WIN32
    // Windows 控制台 UTF-8 支持
    SetConsoleOutputCP(CP_UTF8);
    // 启用 ANSI 转义序列支持（Windows 10+）
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
#endif
}

Logger::~Logger() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_fileStream.is_open()) {
        m_fileStream.close();
    }
}

void Logger::SetLogLevel(LogLevel level) {
    m_logLevel.store(level, std::memory_order_release);
}

void Logger::SetLogDirectory(const std::string& directory) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_logDirectory = directory;
}

void Logger::SetColorOutput(bool enable) {
    m_colorOutput.store(enable, std::memory_order_release);
}

void Logger::SetShowThreadId(bool enable) {
    m_showThreadId.store(enable, std::memory_order_release);
}

void Logger::SetMaxFileSize(size_t maxSize) {
    m_maxFileSize.store(maxSize, std::memory_order_release);
}

void Logger::SetLogCallback(LogCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callback = callback;
}

void Logger::SetLogToFile(bool enable, const std::string& filename) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (enable) {
        if (m_fileStream.is_open()) {
            m_fileStream.close();
        }
        
        // 创建日志目录
        CreateLogDirectory();
        
        // 生成日志文件名（如果未指定）
        std::string logFile;
        if (filename.empty()) {
            logFile = GenerateLogFileName();
        } else {
            // 如果指定了文件名，也放到 logs 目录下
            logFile = m_logDirectory + "/" + filename;
        }
        
        m_currentLogFile = logFile;
        m_currentFileSize = 0;
        
        // 打开文件（使用 UTF-8 编码）
        m_fileStream.open(logFile, std::ios::out | std::ios::trunc);
        bool success = m_fileStream.is_open();
        m_logToFile.store(success, std::memory_order_release);
        
        if (success) {
            // 写入 UTF-8 BOM（可选，但有助于某些编辑器识别）
            m_fileStream << "\xEF\xBB\xBF";
            
            // 写入文件头
            m_fileStream << "========================================" << std::endl;
            m_fileStream << "RenderEngine 日志文件" << std::endl;
            m_fileStream << "创建时间: " << GetTimestamp() << std::endl;
            m_fileStream << "========================================" << std::endl;
            m_fileStream.flush();
            
            // 更新文件大小
            m_currentFileSize = static_cast<size_t>(m_fileStream.tellp());
        }
    } else {
        if (m_fileStream.is_open()) {
            m_fileStream.close();
        }
        m_logToFile.store(false, std::memory_order_release);
        m_currentLogFile.clear();
        m_currentFileSize = 0;
    }
}

void Logger::SetLogToConsole(bool enable) {
    m_logToConsole.store(enable, std::memory_order_release);
}

void Logger::Log(LogLevel level, const std::string& message) {
    // 早期检查日志级别（无需加锁）
    if (level < m_logLevel.load(std::memory_order_acquire)) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    LogInternal(level, message);
}

void Logger::Debug(const std::string& message) {
    Log(LogLevel::Debug, message);
}

void Logger::Info(const std::string& message) {
    Log(LogLevel::Info, message);
}

void Logger::Warning(const std::string& message) {
    Log(LogLevel::Warning, message);
}

void Logger::Error(const std::string& message) {
    Log(LogLevel::Error, message);
}

// ========== 格式化日志方法 ==========

void Logger::LogFormat(LogLevel level, const char* format, ...) {
    if (level < m_logLevel.load(std::memory_order_acquire)) {
        return;
    }
    
    va_list args;
    va_start(args, format);
    std::string message = FormatString(format, args);
    va_end(args);
    
    std::lock_guard<std::mutex> lock(m_mutex);
    LogInternal(level, message);
}

void Logger::DebugFormat(const char* format, ...) {
    if (LogLevel::Debug < m_logLevel.load(std::memory_order_acquire)) {
        return;
    }
    
    va_list args;
    va_start(args, format);
    std::string message = FormatString(format, args);
    va_end(args);
    
    std::lock_guard<std::mutex> lock(m_mutex);
    LogInternal(LogLevel::Debug, message);
}

void Logger::InfoFormat(const char* format, ...) {
    if (LogLevel::Info < m_logLevel.load(std::memory_order_acquire)) {
        return;
    }
    
    va_list args;
    va_start(args, format);
    std::string message = FormatString(format, args);
    va_end(args);
    
    std::lock_guard<std::mutex> lock(m_mutex);
    LogInternal(LogLevel::Info, message);
}

void Logger::WarningFormat(const char* format, ...) {
    if (LogLevel::Warning < m_logLevel.load(std::memory_order_acquire)) {
        return;
    }
    
    va_list args;
    va_start(args, format);
    std::string message = FormatString(format, args);
    va_end(args);
    
    std::lock_guard<std::mutex> lock(m_mutex);
    LogInternal(LogLevel::Warning, message);
}

void Logger::ErrorFormat(const char* format, ...) {
    if (LogLevel::Error < m_logLevel.load(std::memory_order_acquire)) {
        return;
    }
    
    va_list args;
    va_start(args, format);
    std::string message = FormatString(format, args);
    va_end(args);
    
    std::lock_guard<std::mutex> lock(m_mutex);
    LogInternal(LogLevel::Error, message);
}

void Logger::LogWithLocation(LogLevel level, const char* file, int line, const std::string& message) {
    if (level < m_logLevel.load(std::memory_order_acquire)) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    LogInternal(level, message, file, line);
}

std::string Logger::GetCurrentLogFile() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentLogFile;
}

std::string Logger::GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    // 使用线程安全的时间函数
    std::tm timeInfo;
#ifdef _WIN32
    localtime_s(&timeInfo, &time);
#else
    localtime_r(&time, &timeInfo);
#endif
    
    std::stringstream ss;
    ss << std::put_time(&timeInfo, "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

std::string Logger::GetFileTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    
    // 使用线程安全的时间函数
    std::tm timeInfo;
#ifdef _WIN32
    localtime_s(&timeInfo, &time);
#else
    localtime_r(&time, &timeInfo);
#endif
    
    std::stringstream ss;
    ss << std::put_time(&timeInfo, "%Y%m%d_%H%M%S");
    return ss.str();
}

std::string Logger::LevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:   return "DEBUG";
        case LogLevel::Info:    return "INFO";
        case LogLevel::Warning: return "WARNING";
        case LogLevel::Error:   return "ERROR";
        default:                return "UNKNOWN";
    }
}

void Logger::CreateLogDirectory() {
    if (!std::filesystem::exists(m_logDirectory)) {
        std::filesystem::create_directories(m_logDirectory);
    }
}

std::string Logger::GenerateLogFileName() {
    std::string timestamp = GetFileTimestamp();
    return m_logDirectory + "/render_" + timestamp + ".log";
}

// ========== 新增辅助方法 ==========

std::string Logger::GetColorCode(LogLevel level) {
    if (!m_colorOutput.load(std::memory_order_acquire)) {
        return "";
    }
    
    switch (level) {
        case LogLevel::Debug:   return "\033[36m";  // 青色
        case LogLevel::Info:    return "\033[32m";  // 绿色
        case LogLevel::Warning: return "\033[33m";  // 黄色
        case LogLevel::Error:   return "\033[31m";  // 红色
        default:                return "";
    }
}

std::string Logger::GetResetColor() {
    if (!m_colorOutput.load(std::memory_order_acquire)) {
        return "";
    }
    return "\033[0m";
}

std::string Logger::GetThreadIdString() {
    if (!m_showThreadId.load(std::memory_order_acquire)) {
        return "";
    }
    
    std::stringstream ss;
    ss << " [TID:" << std::this_thread::get_id() << "]";
    return ss.str();
}

std::string Logger::FormatString(const char* format, va_list args) {
    // 首先尝试使用固定大小的缓冲区
    char buffer[512];
    va_list argsCopy;
    va_copy(argsCopy, args);
    
    int size = vsnprintf(buffer, sizeof(buffer), format, argsCopy);
    va_end(argsCopy);
    
    if (size < 0) {
        return "[格式化错误]";
    }
    
    // 如果缓冲区足够大
    if (size < static_cast<int>(sizeof(buffer))) {
        return std::string(buffer, size);
    }
    
    // 否则使用动态分配
    std::vector<char> dynamicBuffer(size + 1);
    vsnprintf(dynamicBuffer.data(), dynamicBuffer.size(), format, args);
    return std::string(dynamicBuffer.data(), size);
}

void Logger::CheckAndRotateLogFile() {
    size_t maxSize = m_maxFileSize.load(std::memory_order_acquire);
    if (maxSize == 0 || m_currentFileSize < maxSize) {
        return;
    }
    
    // 需要轮转日志文件
    if (m_fileStream.is_open()) {
        m_fileStream.close();
    }
    
    // 生成新的日志文件名
    std::string newLogFile = GenerateLogFileName();
    m_currentLogFile = newLogFile;
    m_currentFileSize = 0;
    
    // 打开新文件
    m_fileStream.open(newLogFile, std::ios::out | std::ios::trunc);
    bool success = m_fileStream.is_open();
    
    if (success) {
        // 写入 UTF-8 BOM
        m_fileStream << "\xEF\xBB\xBF";
        
        // 写入文件头
        m_fileStream << "========================================" << std::endl;
        m_fileStream << "RenderEngine 日志文件（轮转）" << std::endl;
        m_fileStream << "创建时间: " << GetTimestamp() << std::endl;
        m_fileStream << "========================================" << std::endl;
        m_fileStream.flush();
        
        // 更新文件大小
        m_currentFileSize = static_cast<size_t>(m_fileStream.tellp());
    } else {
        m_logToFile.store(false, std::memory_order_release);
    }
}

void Logger::LogInternal(LogLevel level, const std::string& message, const char* file, int line) {
    // 此方法假定已经持有锁
    
    // 构建完整的日志消息
    std::string timestamp = GetTimestamp();
    std::string levelStr = LevelToString(level);
    std::string threadId = GetThreadIdString();
    
    std::stringstream ss;
    ss << "[" << timestamp << "]" << threadId << " [" << levelStr << "] ";
    
    // 如果有源文件位置信息
    if (file != nullptr) {
        // 只保留文件名，去掉路径
        std::string filename(file);
        size_t pos = filename.find_last_of("/\\");
        if (pos != std::string::npos) {
            filename = filename.substr(pos + 1);
        }
        ss << "[" << filename << ":" << line << "] ";
    }
    
    ss << message;
    std::string fullMessage = ss.str();
    
    // 输出到控制台（带颜色）
    if (m_logToConsole.load(std::memory_order_acquire)) {
        std::string colorCode = GetColorCode(level);
        std::string resetColor = GetResetColor();
        
        if (level == LogLevel::Error) {
            std::cerr << colorCode << fullMessage << resetColor << std::endl;
        } else {
            std::cout << colorCode << fullMessage << resetColor << std::endl;
        }
    }
    
    // 输出到文件（不带颜色）
    if (m_logToFile.load(std::memory_order_acquire) && m_fileStream.is_open()) {
        // 检查是否需要轮转日志
        CheckAndRotateLogFile();
        
        if (m_fileStream.is_open()) {
            m_fileStream << fullMessage << std::endl;
            m_fileStream.flush();
            
            // 更新文件大小
            m_currentFileSize = static_cast<size_t>(m_fileStream.tellp());
        }
    }
    
    // 调用回调函数
    if (m_callback) {
        try {
            m_callback(level, fullMessage);
        } catch (...) {
            // 忽略回调中的异常，避免影响日志系统
        }
    }
}

} // namespace Render

