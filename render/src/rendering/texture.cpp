#include "render/texture.h"
#include "render/logger.h"
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
    std::lock_guard<std::mutex> lock(m_mutex);
    
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

    // 使用 SDL_image 加载图片
    SDL_Surface* surface = IMG_Load(filepath.c_str());
    if (!surface) {
        Logger::GetInstance().Error("从文件加载纹理失败: " + filepath + " - " + SDL_GetError());
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
            Logger::GetInstance().Error("转换纹理格式失败: " + std::string(SDL_GetError()));
            return false;
        }
        
        surface = convertedSurface;
        format = TextureFormat::RGBA;
    }

    // 创建纹理数据（内部实现，无需调用 CreateFromData 以避免重复加锁）
    const void* data = surface->pixels;
    int width = surface->w;
    int height = surface->h;
    
    if (width <= 0 || height <= 0) {
        Logger::GetInstance().Error("无效的纹理尺寸: " + 
                     std::to_string(width) + "x" + std::to_string(height));
        SDL_DestroySurface(surface);
        return false;
    }

    m_width = width;
    m_height = height;
    m_format = format;

    // 生成纹理
    glGenTextures(1, &m_textureID);
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

    Logger::GetInstance().Debug("从文件创建纹理: " + std::to_string(width) + "x" + 
                 std::to_string(height) + ", ID: " + std::to_string(m_textureID) + 
                 ", 格式: " + std::to_string(static_cast<int>(format)) + 
                 ", Mipmap: " + (m_hasMipmap ? "是" : "否"));

    // 释放 SDL Surface
    SDL_DestroySurface(surface);

    return true;
}

bool Texture::CreateFromData(const void* data, int width, int height, 
                             TextureFormat format, bool generateMipmap) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (width <= 0 || height <= 0) {
        Logger::GetInstance().Error("无效的纹理尺寸: " + 
                     std::to_string(width) + "x" + std::to_string(height));
        return false;
    }

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

    // 生成纹理
    glGenTextures(1, &m_textureID);
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

    Logger::GetInstance().Debug("从数据创建纹理: " + std::to_string(width) + "x" + 
                 std::to_string(height) + ", ID: " + std::to_string(m_textureID) + 
                 ", 格式: " + std::to_string(static_cast<int>(format)) + 
                 ", Mipmap: " + (m_hasMipmap ? "是" : "否"));

    return true;
}

bool Texture::CreateEmpty(int width, int height, TextureFormat format) {
    return CreateFromData(nullptr, width, height, format, false);
}

void Texture::Bind(unsigned int unit) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_textureID == 0) {
        Logger::GetInstance().Warning("尝试绑定无效纹理");
        return;
    }

    if (unit > 31) {
        Logger::GetInstance().Warning("纹理单元超出范围: " + std::to_string(unit));
        unit = 0;
    }

    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, m_textureID);
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

} // namespace Render

