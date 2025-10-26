#include "render/uniform_manager.h"
#include "render/logger.h"
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
        glUniform1i(location, value);
    }
}

void UniformManager::SetFloat(const std::string& name, float value) {
    int location = GetOrFindUniformLocation(name);
    if (location != -1) {
        glUniform1f(location, value);
    }
}

void UniformManager::SetBool(const std::string& name, bool value) {
    int location = GetOrFindUniformLocation(name);
    if (location != -1) {
        glUniform1i(location, value ? 1 : 0);
    }
}

void UniformManager::SetVector2(const std::string& name, const Vector2& value) {
    int location = GetOrFindUniformLocation(name);
    if (location != -1) {
        glUniform2f(location, value.x(), value.y());
    }
}

void UniformManager::SetVector3(const std::string& name, const Vector3& value) {
    int location = GetOrFindUniformLocation(name);
    if (location != -1) {
        glUniform3f(location, value.x(), value.y(), value.z());
    }
}

void UniformManager::SetVector4(const std::string& name, const Vector4& value) {
    int location = GetOrFindUniformLocation(name);
    if (location != -1) {
        glUniform4f(location, value.x(), value.y(), value.z(), value.w());
    }
}

void UniformManager::SetMatrix3(const std::string& name, const Matrix3& value) {
    int location = GetOrFindUniformLocation(name);
    if (location != -1) {
        glUniformMatrix3fv(location, 1, GL_FALSE, value.data());
    }
}

void UniformManager::SetMatrix4(const std::string& name, const Matrix4& value) {
    int location = GetOrFindUniformLocation(name);
    if (location != -1) {
        glUniformMatrix4fv(location, 1, GL_FALSE, value.data());
    }
}

void UniformManager::SetColor(const std::string& name, const Color& value) {
    int location = GetOrFindUniformLocation(name);
    if (location != -1) {
        glUniform4f(location, value.r, value.g, value.b, value.a);
    }
}

void UniformManager::SetIntArray(const std::string& name, const int* values, uint32_t count) {
    int location = GetOrFindUniformLocation(name);
    if (location != -1) {
        glUniform1iv(location, count, values);
    }
}

void UniformManager::SetFloatArray(const std::string& name, const float* values, uint32_t count) {
    int location = GetOrFindUniformLocation(name);
    if (location != -1) {
        glUniform1fv(location, count, values);
    }
}

void UniformManager::SetVector3Array(const std::string& name, const Vector3* values, uint32_t count) {
    int location = GetOrFindUniformLocation(name);
    if (location != -1) {
        glUniform3fv(location, count, reinterpret_cast<const float*>(values));
    }
}

void UniformManager::SetMatrix4Array(const std::string& name, const Matrix4* values, uint32_t count) {
    int location = GetOrFindUniformLocation(name);
    if (location != -1) {
        glUniformMatrix4fv(location, count, GL_FALSE, reinterpret_cast<const float*>(values));
    }
}

bool UniformManager::HasUniform(const std::string& name) const {
    // 先检查缓存
    if (m_uniformLocationCache.find(name) != m_uniformLocationCache.end()) {
        return m_uniformLocationCache[name] != -1;
    }
    
    // 查询 OpenGL
    int location = glGetUniformLocation(m_programID, name.c_str());
    m_uniformLocationCache[name] = location;
    return location != -1;
}

int UniformManager::GetUniformLocation(const std::string& name) {
    return GetOrFindUniformLocation(name);
}

void UniformManager::ClearCache() {
    m_uniformLocationCache.clear();
}

std::vector<std::string> UniformManager::GetAllUniformNames() const {
    GLint numUniforms = 0;
    glGetProgramiv(m_programID, GL_ACTIVE_UNIFORMS, &numUniforms);
    
    std::vector<std::string> uniformNames;
    uniformNames.reserve(numUniforms);
    
    for (GLint i = 0; i < numUniforms; ++i) {
        GLchar name[256];
        GLsizei length;
        GLint size;
        GLenum type;
        
        glGetActiveUniform(m_programID, i, sizeof(name), &length, &size, &type, name);
        uniformNames.push_back(std::string(name, length));
    }
    
    return uniformNames;
}

void UniformManager::PrintUniformInfo() const {
    GLint numUniforms = 0;
    glGetProgramiv(m_programID, GL_ACTIVE_UNIFORMS, &numUniforms);
    
    LOG_INFO("Shader Uniforms (" + std::to_string(numUniforms) + " total):");
    
    for (GLint i = 0; i < numUniforms; ++i) {
        GLchar name[256];
        GLsizei length;
        GLint size;
        GLenum type;
        
        glGetActiveUniform(m_programID, i, sizeof(name), &length, &size, &type, name);
        
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
        
        int location = glGetUniformLocation(m_programID, name);
        LOG_INFO("  [" + std::to_string(location) + "] " + 
                std::string(name) + " : " + typeName + 
                (size > 1 ? "[" + std::to_string(size) + "]" : ""));
    }
}

int UniformManager::GetOrFindUniformLocation(const std::string& name) {
    // 检查缓存
    auto it = m_uniformLocationCache.find(name);
    if (it != m_uniformLocationCache.end()) {
        return it->second;
    }
    
    // 查询位置
    int location = glGetUniformLocation(m_programID, name.c_str());
    
    if (location == -1) {
        // 只在首次查找时警告，避免重复警告
        static std::unordered_map<std::string, bool> warnedUniforms;
        if (warnedUniforms.find(name) == warnedUniforms.end()) {
            LOG_WARNING("Uniform '" + name + "' not found in shader program " + std::to_string(m_programID));
            warnedUniforms[name] = true;
        }
    }
    
    // 缓存结果
    m_uniformLocationCache[name] = location;
    return location;
}

} // namespace Render

