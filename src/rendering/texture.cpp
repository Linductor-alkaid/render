/*
 * Copyright (c) 2025 Li Chaoyu
 * 
 * This file is part of Render.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * For commercial licensing, please contact: 2052046346@qq.com
 */
#include "render/texture.h"
#include "render/logger.h"
#include "render/error.h"
#include "render/gl_thread_checker.h"
#include <SDL3_image/SDL_image.h>
#include <SDL3/SDL.h>

namespace Render {

Texture::Texture()
    : m_textureID(0)
    , m_width(0)
    , m_height(0)
    , m_format(TextureFormat::RGBA)
    , m_hasMipmap(false)
{
}

Texture::~Texture() {
    Release();
}

Texture::Texture(Texture&& other) noexcept {
    std::lock_guard<std::mutex> lock(other.m_mutex);
    
    m_textureID = other.m_textureID;
    m_width = other.m_width;
    m_height = other.m_height;
    m_format = other.m_format;
    m_hasMipmap = other.m_hasMipmap;
    
    other.m_textureID = 0;
    other.m_width = 0;
    other.m_height = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        // 使用 scoped_lock 同时锁定两个互斥锁，避免死锁
        std::scoped_lock lock(m_mutex, other.m_mutex);
        
        // 释放当前纹理（内部实现，已持有锁）
        if (m_textureID != 0) {
            GL_THREAD_CHECK();
            glDeleteTextures(1, &m_textureID);
            Logger::GetInstance().Debug("释放纹理 ID: " + std::to_string(m_textureID));
        }

        m_textureID = other.m_textureID;
        m_width = other.m_width;
        m_height = other.m_height;
        m_format = other.m_format;
        m_hasMipmap = other.m_hasMipmap;

        other.m_textureID = 0;
        other.m_width = 0;
        other.m_height = 0;
    }
    return *this;
}

bool Texture::LoadFromFile(const std::string& filepath, bool generateMipmap) {
    // 参数验证
    if (filepath.empty()) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::InvalidArgument, 
                                 "Texture::LoadFromFile: 文件路径为空"));
        return false;
    }
    
    // 在锁外加载图片文件（避免长时间持锁）
    SDL_Surface* surface = IMG_Load(filepath.c_str());
    if (!surface) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::FileOpenFailed, 
                                 "Texture::LoadFromFile: 加载纹理失败: " + filepath + 
                                 " - " + std::string(SDL_GetError())));
        return false;
    }

    Logger::GetInstance().Info("加载纹理: " + filepath + " (" + 
                 std::to_string(surface->w) + "x" + std::to_string(surface->h) + ")");

    // 确定纹理格式 (SDL3 中 format 不再是指针)
    TextureFormat format = TextureFormat::RGBA;
    int bytesPerPixel = SDL_BYTESPERPIXEL(surface->format);
    
    if (bytesPerPixel == 4) {
        format = TextureFormat::RGBA;
    } else if (bytesPerPixel == 3) {
        format = TextureFormat::RGB;
    } else if (bytesPerPixel == 1) {
        format = TextureFormat::RED;
    } else {
        Logger::GetInstance().Warning("不支持的纹理格式，转换为 RGBA");
        // 转换到 RGBA 格式 (SDL3 使用 SDL_ConvertSurface)
        SDL_PixelFormat pixelFormat = SDL_PIXELFORMAT_RGBA32;
        SDL_Surface* convertedSurface = SDL_ConvertSurface(surface, pixelFormat);
        SDL_DestroySurface(surface);
        
        if (!convertedSurface) {
            HANDLE_ERROR(RENDER_ERROR(ErrorCode::ResourceInvalidFormat, 
                                     "Texture::LoadFromFile: 转换纹理格式失败: " + 
                                     std::string(SDL_GetError())));
            return false;
        }
        
        surface = convertedSurface;
        format = TextureFormat::RGBA;
    }
    
    // 现在在锁内创建 OpenGL 纹理
    std::lock_guard<std::mutex> lock(m_mutex);
    
    try {
        // 释放旧纹理（内部方法，无需再加锁）
        if (m_textureID != 0) {
            // 直接释放，不调用 Release()，因为已经持有锁
            glDeleteTextures(1, &m_textureID);
            Logger::GetInstance().Debug("释放纹理 ID: " + std::to_string(m_textureID));
            m_textureID = 0;
            m_width = 0;
            m_height = 0;
            m_hasMipmap = false;
        }

        // 创建纹理数据（内部实现，无需调用 CreateFromData 以避免重复加锁）
        const void* data = surface->pixels;
        int width = surface->w;
        int height = surface->h;
        
        if (width <= 0 || height <= 0) {
            HANDLE_ERROR(RENDER_ERROR(ErrorCode::InvalidArgument, 
                                     "Texture::LoadFromFile: 无效的纹理尺寸: " + 
                                     std::to_string(width) + "x" + std::to_string(height)));
            SDL_DestroySurface(surface);
            return false;
        }

        m_width = width;
        m_height = height;
        m_format = format;

        // ✅ 在绑定纹理前，清理OpenGL状态以避免冲突
        // 确保没有VAO绑定（VAO可能影响纹理操作）
        GLint currentVAO = 0;
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &currentVAO);
        if (currentVAO != 0) {
            glBindVertexArray(0);
        }
        
        // 确保激活正确的纹理单元
        glActiveTexture(GL_TEXTURE0);
        
        // 清除之前的OpenGL错误
        glGetError();

        // 生成纹理
        glGenTextures(1, &m_textureID);
        if (m_textureID == 0) {
            throw std::runtime_error("Failed to generate texture ID");
        }
        glBindTexture(GL_TEXTURE_2D, m_textureID);

        // 设置纹理数据
        GLenum glFormat = ToGLFormat(format);
        GLenum glInternalFormat = ToGLInternalFormat(format);

        glTexImage2D(GL_TEXTURE_2D, 0, glInternalFormat, width, height, 
                     0, glFormat, GL_UNSIGNED_BYTE, data);
        
        // 验证纹理数据是否上传成功
        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            Logger::GetInstance().Error("glTexImage2D 失败，OpenGL 错误: " + std::to_string(err));
            glBindTexture(GL_TEXTURE_2D, 0);
            glDeleteTextures(1, &m_textureID);
            m_textureID = 0;
            // ✅ 恢复之前的VAO（如果需要）
            if (currentVAO != 0) {
                glBindVertexArray(currentVAO);
            }
            SDL_DestroySurface(surface);
            return false;
        }

        // 设置默认过滤参数
        if (generateMipmap) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        } else {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }

        // 设置默认环绕模式
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        
        // 生成 Mipmap（必须在所有参数设置后）
        if (generateMipmap) {
            glGenerateMipmap(GL_TEXTURE_2D);
            m_hasMipmap = true;
            Logger::GetInstance().Debug("为纹理生成 Mipmap，ID: " + std::to_string(m_textureID));
        }

        glBindTexture(GL_TEXTURE_2D, 0);

        // ✅ 恢复之前的VAO（如果需要）
        if (currentVAO != 0) {
            glBindVertexArray(currentVAO);
        }

        Logger::GetInstance().Debug("从文件创建纹理: " + std::to_string(width) + "x" + 
                     std::to_string(height) + ", ID: " + std::to_string(m_textureID) + 
                     ", 格式: " + std::to_string(static_cast<int>(format)) + 
                     ", Mipmap: " + (m_hasMipmap ? "是" : "否"));

        // 释放 SDL Surface
        SDL_DestroySurface(surface);

        return true;
        
    } catch (const std::exception& e) {
        // 异常处理：清理部分创建的资源
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::Unknown, 
                                 "Texture::LoadFromFile: Exception during texture creation - " + std::string(e.what())));
        
        // 清理资源
        if (m_textureID != 0) {
            glDeleteTextures(1, &m_textureID);
            m_textureID = 0;
        }
        m_width = 0;
        m_height = 0;
        m_hasMipmap = false;
        
        SDL_DestroySurface(surface);
        return false;
        
    } catch (...) {
        // 捕获所有异常
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::Unknown, 
                                 "Texture::LoadFromFile: Unknown exception during texture creation"));
        
        // 清理资源
        if (m_textureID != 0) {
            glDeleteTextures(1, &m_textureID);
            m_textureID = 0;
        }
        m_width = 0;
        m_height = 0;
        m_hasMipmap = false;
        
        SDL_DestroySurface(surface);
        return false;
    }
}

bool Texture::CreateFromData(const void* data, int width, int height, 
                             TextureFormat format, bool generateMipmap) {
    // 参数验证
    if (width <= 0 || height <= 0) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::InvalidArgument, 
                                 "Texture::CreateFromData: 无效的纹理尺寸: " + 
                                 std::to_string(width) + "x" + std::to_string(height)));
        return false;
    }
    
    if (width > 8192 || height > 8192) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::OutOfRange, 
                                   "Texture::CreateFromData: 纹理尺寸超过推荐限制: " + 
                                   std::to_string(width) + "x" + std::to_string(height)));
    }
    
        std::lock_guard<std::mutex> lock(m_mutex);

    try {
        // 释放旧纹理（内部方法，无需再加锁）
        if (m_textureID != 0) {
            glDeleteTextures(1, &m_textureID);
            Logger::GetInstance().Debug("释放纹理 ID: " + std::to_string(m_textureID));
            m_textureID = 0;
            m_width = 0;
            m_height = 0;
            m_hasMipmap = false;
        }

        m_width = width;
        m_height = height;
        m_format = format;

        // ✅ 在绑定纹理前，清理OpenGL状态以避免冲突
        // 确保没有VAO绑定（VAO可能影响纹理操作）
        GLint currentVAO = 0;
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &currentVAO);
        if (currentVAO != 0) {
            glBindVertexArray(0);
        }
        
        // 确保激活正确的纹理单元
        glActiveTexture(GL_TEXTURE0);
        
        // 清除之前的OpenGL错误
        glGetError();

        // 生成纹理
        glGenTextures(1, &m_textureID);
        if (m_textureID == 0) {
            throw std::runtime_error("Failed to generate texture ID");
        }
        glBindTexture(GL_TEXTURE_2D, m_textureID);

        // 设置纹理数据
        GLenum glFormat = ToGLFormat(format);
        GLenum glInternalFormat = ToGLInternalFormat(format);

        glTexImage2D(GL_TEXTURE_2D, 0, glInternalFormat, width, height, 
                     0, glFormat, GL_UNSIGNED_BYTE, data);
        
        // 验证纹理数据是否上传成功
        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            Logger::GetInstance().Error("glTexImage2D 失败，OpenGL 错误: " + std::to_string(err));
            glBindTexture(GL_TEXTURE_2D, 0);
            glDeleteTextures(1, &m_textureID);
            m_textureID = 0;
            return false;
        }

        // 设置默认过滤参数
        if (generateMipmap) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        } else {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }

        // 设置默认环绕模式
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        
        // 生成 Mipmap（必须在所有参数设置后）
        if (generateMipmap) {
            glGenerateMipmap(GL_TEXTURE_2D);
            m_hasMipmap = true;
            Logger::GetInstance().Debug("为纹理生成 Mipmap，ID: " + std::to_string(m_textureID));
        }

        glBindTexture(GL_TEXTURE_2D, 0);

        // ✅ 恢复之前的VAO（如果需要）
        if (currentVAO != 0) {
            glBindVertexArray(currentVAO);
        }

        Logger::GetInstance().Debug("从数据创建纹理: " + std::to_string(width) + "x" + 
                     std::to_string(height) + ", ID: " + std::to_string(m_textureID) + 
                     ", 格式: " + std::to_string(static_cast<int>(format)) + 
                     ", Mipmap: " + (m_hasMipmap ? "是" : "否"));

        return true;
        
    } catch (const std::exception& e) {
        // 异常处理：清理部分创建的资源
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::Unknown, 
                                 "Texture::CreateFromData: Exception during texture creation - " + std::string(e.what())));
        
        // 清理资源
        if (m_textureID != 0) {
            glDeleteTextures(1, &m_textureID);
            m_textureID = 0;
        }
        m_width = 0;
        m_height = 0;
        m_hasMipmap = false;
        
        return false;
        
    } catch (...) {
        // 捕获所有异常
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::Unknown, 
                                 "Texture::CreateFromData: Unknown exception during texture creation"));
        
        // 清理资源
        if (m_textureID != 0) {
            glDeleteTextures(1, &m_textureID);
            m_textureID = 0;
        }
        m_width = 0;
        m_height = 0;
        m_hasMipmap = false;
        
        return false;
    }
}

bool Texture::CreateEmpty(int width, int height, TextureFormat format) {
    return CreateFromData(nullptr, width, height, format, false);
}

void Texture::Bind(unsigned int unit) const {
    // ✅ 修复：两阶段锁定，OpenGL调用移到锁外
    GLuint textureID;
    
    // 阶段1：快速读取纹理ID（持锁）
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        textureID = m_textureID;
    }  // 锁释放
    
    // 阶段2：OpenGL调用（无锁）
    if (textureID == 0) {
        Logger::GetInstance().Warning("尝试绑定无效纹理");
        return;
    }

    if (unit > 31) {
        Logger::GetInstance().Warning("纹理单元超出范围: " + std::to_string(unit));
        unit = 0;
    }

    GL_THREAD_CHECK();
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, textureID);
}

void Texture::Unbind() const {
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::SetFilter(TextureFilter minFilter, TextureFilter magFilter) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_textureID == 0) {
        Logger::GetInstance().Warning("无法为无效纹理设置过滤器");
        return;
    }

    glBindTexture(GL_TEXTURE_2D, m_textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, 
                   ToGLFilter(minFilter, m_hasMipmap));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, 
                   ToGLFilter(magFilter, false));
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::SetWrap(TextureWrap wrapS, TextureWrap wrapT) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_textureID == 0) {
        Logger::GetInstance().Warning("无法为无效纹理设置环绕模式");
        return;
    }

    glBindTexture(GL_TEXTURE_2D, m_textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, ToGLWrap(wrapS));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, ToGLWrap(wrapT));
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::GenerateMipmap() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_textureID == 0) {
        Logger::GetInstance().Warning("无法为无效纹理生成 Mipmap");
        return;
    }

    glBindTexture(GL_TEXTURE_2D, m_textureID);
    glGenerateMipmap(GL_TEXTURE_2D);
    m_hasMipmap = true;
    glBindTexture(GL_TEXTURE_2D, 0);

    Logger::GetInstance().Debug("为纹理生成 Mipmap（外部调用），ID: " + std::to_string(m_textureID));
}

void Texture::Release() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_textureID != 0) {
        GL_THREAD_CHECK();
        glDeleteTextures(1, &m_textureID);
        Logger::GetInstance().Debug("释放纹理 ID: " + std::to_string(m_textureID));
        m_textureID = 0;
    }

    m_width = 0;
    m_height = 0;
    m_hasMipmap = false;
}

GLenum Texture::ToGLFormat(TextureFormat format) const {
    switch (format) {
        case TextureFormat::RGB:          return GL_RGB;
        case TextureFormat::RGBA:         return GL_RGBA;
        case TextureFormat::RED:          return GL_RED;
        case TextureFormat::RG:           return GL_RG;
        case TextureFormat::Depth:        return GL_DEPTH_COMPONENT;
        case TextureFormat::DepthStencil: return GL_DEPTH_STENCIL;
        default:                          return GL_RGBA;
    }
}

GLenum Texture::ToGLInternalFormat(TextureFormat format) const {
    switch (format) {
        case TextureFormat::RGB:          return GL_RGB8;
        case TextureFormat::RGBA:         return GL_RGBA8;
        case TextureFormat::RED:          return GL_R8;
        case TextureFormat::RG:           return GL_RG8;
        case TextureFormat::Depth:        return GL_DEPTH_COMPONENT24;
        case TextureFormat::DepthStencil: return GL_DEPTH24_STENCIL8;
        default:                          return GL_RGBA8;
    }
}

GLenum Texture::ToGLFilter(TextureFilter filter, bool isMipmap) const {
    switch (filter) {
        case TextureFilter::Nearest:
            return GL_NEAREST;
        case TextureFilter::Linear:
            return GL_LINEAR;
        case TextureFilter::Mipmap:
            return isMipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR;
        default:
            return GL_LINEAR;
    }
}

GLenum Texture::ToGLWrap(TextureWrap wrap) const {
    switch (wrap) {
        case TextureWrap::Repeat:         return GL_REPEAT;
        case TextureWrap::MirroredRepeat: return GL_MIRRORED_REPEAT;
        case TextureWrap::ClampToEdge:    return GL_CLAMP_TO_EDGE;
        case TextureWrap::ClampToBorder:  return GL_CLAMP_TO_BORDER;
        default:                          return GL_REPEAT;
    }
}

size_t Texture::GetMemoryUsage() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_textureID == 0) {
        return 0;
    }
    
    // 计算每像素字节数
    size_t bytesPerPixel = 0;
    switch (m_format) {
        case TextureFormat::RGB:
            bytesPerPixel = 3;
            break;
        case TextureFormat::RGBA:
            bytesPerPixel = 4;
            break;
        case TextureFormat::RED:
            bytesPerPixel = 1;
            break;
        case TextureFormat::RG:
            bytesPerPixel = 2;
            break;
        case TextureFormat::Depth:
            bytesPerPixel = 4;  // 通常是 32 位深度
            break;
        case TextureFormat::DepthStencil:
            bytesPerPixel = 4;  // 24 位深度 + 8 位模板
            break;
    }
    
    // 溢出检查：检查 width * height 是否溢出
    if (m_width > 0 && m_height > SIZE_MAX / static_cast<size_t>(m_width)) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::OutOfRange, 
                                   "Texture::GetMemoryUsage: Size calculation overflow (width=" + 
                                   std::to_string(m_width) + ", height=" + std::to_string(m_height) + ")"));
        return SIZE_MAX;
    }
    
    size_t pixelCount = static_cast<size_t>(m_width) * static_cast<size_t>(m_height);
    
    // 溢出检查：检查 pixelCount * bytesPerPixel 是否溢出
    if (pixelCount > SIZE_MAX / bytesPerPixel) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::OutOfRange, 
                                   "Texture::GetMemoryUsage: Memory calculation overflow (pixels=" + 
                                   std::to_string(pixelCount) + ", bytesPerPixel=" + std::to_string(bytesPerPixel) + ")"));
        return SIZE_MAX;
    }
    
    size_t baseMemory = pixelCount * bytesPerPixel;
    
    // 如果有 mipmap，大约增加 1/3 的内存（mipmap 链的总和）
    if (m_hasMipmap) {
        // 溢出检查：检查 mipmap 计算是否溢出
        if (baseMemory > SIZE_MAX / 4 * 3) {
            HANDLE_ERROR(RENDER_WARNING(ErrorCode::OutOfRange, 
                                       "Texture::GetMemoryUsage: Mipmap memory calculation overflow"));
            return SIZE_MAX;
        }
        baseMemory = baseMemory * 4 / 3;
    }
    
    return baseMemory;
}

} // namespace Render

