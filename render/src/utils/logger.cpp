#include "render/logger.h"

namespace Render {

Logger& Logger::GetInstance() {
    static Logger instance;
    return instance;
}

Logger::Logger() 
    : m_logLevel(LogLevel::Debug)
    , m_logToConsole(true)
    , m_logToFile(false) {
}

Logger::~Logger() {
    if (m_fileStream.is_open()) {
        m_fileStream.close();
    }
}

void Logger::SetLogLevel(LogLevel level) {
    m_logLevel = level;
}

void Logger::SetLogToFile(bool enable, const std::string& filename) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (enable) {
        if (m_fileStream.is_open()) {
            m_fileStream.close();
        }
        m_fileStream.open(filename, std::ios::out | std::ios::app);
        m_logToFile = m_fileStream.is_open();
    } else {
        if (m_fileStream.is_open()) {
            m_fileStream.close();
        }
        m_logToFile = false;
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

std::string Logger::LevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:   return "DEBUG";
        case LogLevel::Info:    return "INFO";
        case LogLevel::Warning: return "WARNING";
        case LogLevel::Error:   return "ERROR";
        default:                return "UNKNOWN";
    }
}

} // namespace Render

