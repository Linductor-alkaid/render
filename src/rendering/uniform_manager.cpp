#include "render/uniform_manager.h"
#include "render/logger.h"
#include "render/error.h"
#include "render/gl_thread_checker.h"
#include <glad/glad.h>

namespace Render {

UniformManager::UniformManager(uint32_t programID)
    : m_programID(programID) {
}

UniformManager::~UniformManager() {
    m_uniformLocationCache.clear();
}

void UniformManager::SetInt(const std::string& name, int value) {
    int location = GetOrFindUniformLocation(name);
    if (location != -1) {
        GL_THREAD_CHECK();
        glUniform1i(location, value);
    }
}

void UniformManager::SetFloat(const std::string& name, float value) {
    int location = GetOrFindUniformLocation(name);
    if (location != -1) {
        GL_THREAD_CHECK();
        glUniform1f(location, value);
    }
}

void UniformManager::SetBool(const std::string& name, bool value) {
    int location = GetOrFindUniformLocation(name);
    if (location != -1) {
        GL_THREAD_CHECK();
        glUniform1i(location, value ? 1 : 0);
    }
}

void UniformManager::SetVector2(const std::string& name, const Vector2& value) {
    int location = GetOrFindUniformLocation(name);
    if (location != -1) {
        GL_THREAD_CHECK();
        glUniform2f(location, value.x(), value.y());
    }
}

void UniformManager::SetVector3(const std::string& name, const Vector3& value) {
    int location = GetOrFindUniformLocation(name);
    if (location != -1) {
        GL_THREAD_CHECK();
        glUniform3f(location, value.x(), value.y(), value.z());
    }
}

void UniformManager::SetVector4(const std::string& name, const Vector4& value) {
    int location = GetOrFindUniformLocation(name);
    if (location != -1) {
        GL_THREAD_CHECK();
        glUniform4f(location, value.x(), value.y(), value.z(), value.w());
    }
}

void UniformManager::SetMatrix3(const std::string& name, const Matrix3& value) {
    int location = GetOrFindUniformLocation(name);
    if (location != -1) {
        GL_THREAD_CHECK();
        glUniformMatrix3fv(location, 1, GL_FALSE, value.data());
    }
}

void UniformManager::SetMatrix4(const std::string& name, const Matrix4& value) {
    int location = GetOrFindUniformLocation(name);
    if (location != -1) {
        GL_THREAD_CHECK();
        glUniformMatrix4fv(location, 1, GL_FALSE, value.data());
    }
}

void UniformManager::SetColor(const std::string& name, const Color& value) {
    int location = GetOrFindUniformLocation(name);
    if (location != -1) {
        GL_THREAD_CHECK();
        glUniform4f(location, value.r, value.g, value.b, value.a);
    }
}

void UniformManager::SetIntArray(const std::string& name, const int* values, uint32_t count) {
    // 添加参数验证
    if (!values) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument, 
                                   "UniformManager::SetIntArray: values pointer is null"));
        return;
    }
    
    if (count == 0) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument, 
                                   "UniformManager::SetIntArray: count is zero"));
        return;
    }
    
    int location = GetOrFindUniformLocation(name);
    if (location != -1) {
        GL_THREAD_CHECK();
        glUniform1iv(location, count, values);
    }
}

void UniformManager::SetFloatArray(const std::string& name, const float* values, uint32_t count) {
    // 添加参数验证
    if (!values) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument, 
                                   "UniformManager::SetFloatArray: values pointer is null"));
        return;
    }
    
    if (count == 0) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument, 
                                   "UniformManager::SetFloatArray: count is zero"));
        return;
    }
    
    int location = GetOrFindUniformLocation(name);
    if (location != -1) {
        GL_THREAD_CHECK();
        glUniform1fv(location, count, values);
    }
}

void UniformManager::SetVector3Array(const std::string& name, const Vector3* values, uint32_t count) {
    // 添加参数验证
    if (!values) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument, 
                                   "UniformManager::SetVector3Array: values pointer is null"));
        return;
    }
    
    if (count == 0) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument, 
                                   "UniformManager::SetVector3Array: count is zero"));
        return;
    }
    
    int location = GetOrFindUniformLocation(name);
    if (location != -1) {
        GL_THREAD_CHECK();
        glUniform3fv(location, count, reinterpret_cast<const float*>(values));
    }
}

void UniformManager::SetMatrix4Array(const std::string& name, const Matrix4* values, uint32_t count) {
    // 添加参数验证
    if (!values) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument, 
                                   "UniformManager::SetMatrix4Array: values pointer is null"));
        return;
    }
    
    if (count == 0) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument, 
                                   "UniformManager::SetMatrix4Array: count is zero"));
        return;
    }
    
    int location = GetOrFindUniformLocation(name);
    if (location != -1) {
        GL_THREAD_CHECK();
        glUniformMatrix4fv(location, count, GL_FALSE, reinterpret_cast<const float*>(values));
    }
}

void UniformManager::SetVector4Array(const std::string& name, const Vector4* values, uint32_t count) {
    if (!values) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument,
                                   "UniformManager::SetVector4Array: values pointer is null"));
        return;
    }

    if (count == 0) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument,
                                   "UniformManager::SetVector4Array: count is zero"));
        return;
    }

    int location = GetOrFindUniformLocation(name);
    if (location != -1) {
        GL_THREAD_CHECK();
        glUniform4fv(location, count, reinterpret_cast<const float*>(values));
    }
}

bool UniformManager::HasUniform(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    // 先检查缓存
    auto it = m_uniformLocationCache.find(name);
    if (it != m_uniformLocationCache.end()) {
        return it->second != -1;
    }
    
    // 查询 OpenGL
    GL_THREAD_CHECK();
    int location = glGetUniformLocation(m_programID, name.c_str());
    m_uniformLocationCache[name] = location;
    return location != -1;
}

int UniformManager::GetUniformLocation(const std::string& name) {
    return GetOrFindUniformLocation(name);
}

void UniformManager::ClearCache() {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    m_uniformLocationCache.clear();
}

std::vector<std::string> UniformManager::GetAllUniformNames() const {
    GL_THREAD_CHECK();
    
    GLint numUniforms = 0;
    glGetProgramiv(m_programID, GL_ACTIVE_UNIFORMS, &numUniforms);
    
    // 查询最大 uniform 名称长度，避免栈溢出
    GLint maxLength = 0;
    glGetProgramiv(m_programID, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxLength);
    
    // 如果查询失败，使用一个合理的默认值
    if (maxLength <= 0) {
        maxLength = 256;
    }
    
    std::vector<std::string> uniformNames;
    uniformNames.reserve(numUniforms);
    
    // 使用动态分配的缓冲区
    std::vector<GLchar> nameBuffer(maxLength);
    
    for (GLint i = 0; i < numUniforms; ++i) {
        GLsizei length;
        GLint size;
        GLenum type;
        
        glGetActiveUniform(m_programID, i, maxLength, &length, &size, &type, nameBuffer.data());
        uniformNames.push_back(std::string(nameBuffer.data(), length));
    }
    
    return uniformNames;
}

void UniformManager::PrintUniformInfo() const {
    GL_THREAD_CHECK();
    
    GLint numUniforms = 0;
    glGetProgramiv(m_programID, GL_ACTIVE_UNIFORMS, &numUniforms);
    
    // 查询最大 uniform 名称长度，避免栈溢出
    GLint maxLength = 0;
    glGetProgramiv(m_programID, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxLength);
    
    // 如果查询失败，使用一个合理的默认值
    if (maxLength <= 0) {
        maxLength = 256;
    }
    
    LOG_INFO("Shader Uniforms (" + std::to_string(numUniforms) + " total):");
    
    // 使用动态分配的缓冲区
    std::vector<GLchar> nameBuffer(maxLength);
    
    for (GLint i = 0; i < numUniforms; ++i) {
        GLsizei length;
        GLint size;
        GLenum type;
        
        glGetActiveUniform(m_programID, i, maxLength, &length, &size, &type, nameBuffer.data());
        
        std::string typeName;
        switch (type) {
            case GL_FLOAT:        typeName = "float"; break;
            case GL_FLOAT_VEC2:   typeName = "vec2"; break;
            case GL_FLOAT_VEC3:   typeName = "vec3"; break;
            case GL_FLOAT_VEC4:   typeName = "vec4"; break;
            case GL_INT:          typeName = "int"; break;
            case GL_BOOL:         typeName = "bool"; break;
            case GL_FLOAT_MAT3:   typeName = "mat3"; break;
            case GL_FLOAT_MAT4:   typeName = "mat4"; break;
            case GL_SAMPLER_2D:   typeName = "sampler2D"; break;
            case GL_SAMPLER_CUBE: typeName = "samplerCube"; break;
            default:              typeName = "unknown"; break;
        }
        
        int location = glGetUniformLocation(m_programID, nameBuffer.data());
        LOG_INFO("  [" + std::to_string(location) + "] " + 
                std::string(nameBuffer.data(), length) + " : " + typeName + 
                (size > 1 ? "[" + std::to_string(size) + "]" : ""));
    }
}

int UniformManager::GetOrFindUniformLocation(const std::string& name) {
    // 使用静态局部变量和互斥锁保护警告映射
    static std::mutex warnedMutex;
    static std::unordered_map<std::string, bool> warnedUniforms;
    
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    // 检查缓存
    auto it = m_uniformLocationCache.find(name);
    if (it != m_uniformLocationCache.end()) {
        return it->second;
    }
    
    // 查询位置
    GL_THREAD_CHECK();
    int location = glGetUniformLocation(m_programID, name.c_str());
    
    if (location == -1) {
        // 只在首次查找时警告，避免重复警告
        std::lock_guard<std::mutex> warnLock(warnedMutex);
        if (warnedUniforms.find(name) == warnedUniforms.end()) {
            HANDLE_ERROR(RENDER_WARNING(ErrorCode::ShaderUniformNotFound, 
                                       "UniformManager: Uniform '" + name + 
                                       "' 未在着色器程序 " + std::to_string(m_programID) + " 中找到"));
            warnedUniforms[name] = true;
        }
    }
    
    // 缓存结果
    m_uniformLocationCache[name] = location;
    return location;
}

} // namespace Render

