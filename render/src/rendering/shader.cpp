#include "render/shader.h"
#include "render/uniform_manager.h"
#include "render/file_utils.h"
#include "render/logger.h"
#include <glad/glad.h>

namespace Render {

Shader::Shader()
    : m_programID(0) {
}

Shader::~Shader() {
    DeleteProgram();
}

bool Shader::LoadFromFile(const std::string& vertexPath,
                          const std::string& fragmentPath,
                          const std::string& geometryPath) {
    LOG_INFO("Loading shader from files:");
    LOG_INFO("  Vertex: " + vertexPath);
    LOG_INFO("  Fragment: " + fragmentPath);
    if (!geometryPath.empty()) {
        LOG_INFO("  Geometry: " + geometryPath);
    }
    
    // 保存路径用于重载
    m_vertexPath = vertexPath;
    m_fragmentPath = fragmentPath;
    m_geometryPath = geometryPath;
    
    // 读取文件
    std::string vertexSource = FileUtils::ReadFile(vertexPath);
    if (vertexSource.empty()) {
        LOG_ERROR("Failed to read vertex shader: " + vertexPath);
        return false;
    }
    
    std::string fragmentSource = FileUtils::ReadFile(fragmentPath);
    if (fragmentSource.empty()) {
        LOG_ERROR("Failed to read fragment shader: " + fragmentPath);
        return false;
    }
    
    std::string geometrySource;
    if (!geometryPath.empty()) {
        geometrySource = FileUtils::ReadFile(geometryPath);
        if (geometrySource.empty()) {
            LOG_ERROR("Failed to read geometry shader: " + geometryPath);
            return false;
        }
    }
    
    // 从源码加载
    return LoadFromSource(vertexSource, fragmentSource, geometrySource);
}

bool Shader::LoadFromSource(const std::string& vertexSource,
                            const std::string& fragmentSource,
                            const std::string& geometrySource) {
    // 删除旧程序
    DeleteProgram();
    
    LOG_INFO("Compiling shaders...");
    
    // 编译顶点着色器
    uint32_t vertexShader = CompileShader(vertexSource, ShaderType::Vertex);
    if (vertexShader == 0) {
        LOG_ERROR("Failed to compile vertex shader");
        return false;
    }
    LOG_INFO("Vertex shader compiled successfully");
    
    // 编译片段着色器
    uint32_t fragmentShader = CompileShader(fragmentSource, ShaderType::Fragment);
    if (fragmentShader == 0) {
        LOG_ERROR("Failed to compile fragment shader");
        glDeleteShader(vertexShader);
        return false;
    }
    LOG_INFO("Fragment shader compiled successfully");
    
    // 编译几何着色器（如果有）
    uint32_t geometryShader = 0;
    if (!geometrySource.empty()) {
        geometryShader = CompileShader(geometrySource, ShaderType::Geometry);
        if (geometryShader == 0) {
            LOG_ERROR("Failed to compile geometry shader");
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
            return false;
        }
        LOG_INFO("Geometry shader compiled successfully");
    }
    
    // 链接程序
    LOG_INFO("Linking shader program...");
    m_programID = LinkProgram(vertexShader, fragmentShader, geometryShader);
    
    // 删除着色器对象（已链接到程序中）
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    if (geometryShader != 0) {
        glDeleteShader(geometryShader);
    }
    
    if (m_programID == 0) {
        LOG_ERROR("Failed to link shader program");
        return false;
    }
    
    LOG_INFO("Shader program linked successfully (ID: " + std::to_string(m_programID) + ")");
    
    // 创建 UniformManager
    m_uniformManager = std::make_unique<UniformManager>(m_programID);
    
    return true;
}

void Shader::Use() const {
    if (m_programID != 0) {
        glUseProgram(m_programID);
    }
}

void Shader::Unuse() const {
    glUseProgram(0);
}

bool Shader::Reload() {
    if (m_vertexPath.empty() || m_fragmentPath.empty()) {
        LOG_WARNING("Cannot reload shader: no source paths available");
        return false;
    }
    
    LOG_INFO("Reloading shader...");
    return LoadFromFile(m_vertexPath, m_fragmentPath, m_geometryPath);
}

uint32_t Shader::CompileShader(const std::string& source, ShaderType type) {
    GLenum glType;
    std::string typeName;
    
    switch (type) {
        case ShaderType::Vertex:
            glType = GL_VERTEX_SHADER;
            typeName = "Vertex";
            break;
        case ShaderType::Fragment:
            glType = GL_FRAGMENT_SHADER;
            typeName = "Fragment";
            break;
        case ShaderType::Geometry:
            glType = GL_GEOMETRY_SHADER;
            typeName = "Geometry";
            break;
        case ShaderType::Compute:
            glType = GL_COMPUTE_SHADER;
            typeName = "Compute";
            break;
        default:
            LOG_ERROR("Unknown shader type");
            return 0;
    }
    
    // 创建着色器对象
    uint32_t shader = glCreateShader(glType);
    
    // 设置源码
    const char* sourceCStr = source.c_str();
    glShaderSource(shader, 1, &sourceCStr, nullptr);
    
    // 编译
    glCompileShader(shader);
    
    // 检查错误
    if (!CheckCompileErrors(shader, type)) {
        glDeleteShader(shader);
        return 0;
    }
    
    return shader;
}

uint32_t Shader::LinkProgram(uint32_t vertexShader, 
                              uint32_t fragmentShader,
                              uint32_t geometryShader) {
    // 创建程序
    uint32_t program = glCreateProgram();
    
    // 附加着色器
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    if (geometryShader != 0) {
        glAttachShader(program, geometryShader);
    }
    
    // 链接
    glLinkProgram(program);
    
    // 检查错误
    if (!CheckLinkErrors(program)) {
        glDeleteProgram(program);
        return 0;
    }
    
    // 分离着色器（可选，已经链接完成）
    glDetachShader(program, vertexShader);
    glDetachShader(program, fragmentShader);
    if (geometryShader != 0) {
        glDetachShader(program, geometryShader);
    }
    
    return program;
}

bool Shader::CheckCompileErrors(uint32_t shader, ShaderType type) {
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    
    if (!success) {
        GLchar infoLog[1024];
        glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
        
        std::string typeName;
        switch (type) {
            case ShaderType::Vertex:   typeName = "Vertex"; break;
            case ShaderType::Fragment: typeName = "Fragment"; break;
            case ShaderType::Geometry: typeName = "Geometry"; break;
            case ShaderType::Compute:  typeName = "Compute"; break;
        }
        
        LOG_ERROR(typeName + " shader compilation failed:");
        LOG_ERROR(infoLog);
        return false;
    }
    
    return true;
}

bool Shader::CheckLinkErrors(uint32_t program) {
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    
    if (!success) {
        GLchar infoLog[1024];
        glGetProgramInfoLog(program, sizeof(infoLog), nullptr, infoLog);
        
        LOG_ERROR("Shader program linking failed:");
        LOG_ERROR(infoLog);
        return false;
    }
    
    return true;
}

void Shader::DeleteProgram() {
    if (m_programID != 0) {
        glDeleteProgram(m_programID);
        m_programID = 0;
        m_uniformManager.reset();
    }
}

} // namespace Render

