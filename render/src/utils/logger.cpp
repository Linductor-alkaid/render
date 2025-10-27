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
    , m_logDirectory("logs") {
    
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
    if (m_fileStream.is_open()) {
        m_fileStream.close();
    }
}

void Logger::SetLogLevel(LogLevel level) {
    m_logLevel = level;
}

void Logger::SetLogDirectory(const std::string& directory) {
    m_logDirectory = directory;
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
        
        // 打开文件（使用 UTF-8 编码）
        m_fileStream.open(logFile, std::ios::out | std::ios::trunc);
        m_logToFile = m_fileStream.is_open();
        
        if (m_logToFile) {
            // 写入 UTF-8 BOM（可选，但有助于某些编辑器识别）
            m_fileStream << "\xEF\xBB\xBF";
            
            // 写入文件头
            m_fileStream << "========================================" << std::endl;
            m_fileStream << "RenderEngine 日志文件" << std::endl;
            m_fileStream << "创建时间: " << GetTimestamp() << std::endl;
            m_fileStream << "========================================" << std::endl;
            m_fileStream.flush();
        }
    } else {
        if (m_fileStream.is_open()) {
            m_fileStream.close();
        }
        m_logToFile = false;
        m_currentLogFile.clear();
    }
}

void Logger::SetLogToConsole(bool enable) {
    m_logToConsole = enable;
}

void Logger::Log(LogLevel level, const std::string& message) {
    if (level < m_logLevel) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::string timestamp = GetTimestamp();
    std::string levelStr = LevelToString(level);
    std::string fullMessage = "[" + timestamp + "] [" + levelStr + "] " + message;
    
    if (m_logToConsole) {
        if (level == LogLevel::Error) {
            std::cerr << fullMessage << std::endl;
        } else {
            std::cout << fullMessage << std::endl;
        }
    }
    
    if (m_logToFile && m_fileStream.is_open()) {
        m_fileStream << fullMessage << std::endl;
        m_fileStream.flush();
    }
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

std::string Logger::GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

std::string Logger::GetFileTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
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

} // namespace Render

