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
#include "render/texture_cubemap.h"
#include "render/logger.h"
#include "render/error.h"
#include "render/gl_thread_checker.h"
#include <SDL3_image/SDL_image.h>
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstring>

namespace Render {

TextureCubemap::TextureCubemap()
    : m_textureID(0)
    , m_resolution(0)
    , m_format(TextureFormat::RGBA)
    , m_hasMipmap(false)
    , m_faceLoaded(6, false)
{
}

TextureCubemap::~TextureCubemap() {
    Release();
}

TextureCubemap::TextureCubemap(TextureCubemap&& other) noexcept {
    std::lock_guard<std::mutex> lock(other.m_mutex);
    
    m_textureID = other.m_textureID;
    m_resolution = other.m_resolution;
    m_format = other.m_format;
    m_hasMipmap = other.m_hasMipmap;
    m_faceLoaded = std::move(other.m_faceLoaded);
    
    other.m_textureID = 0;
    other.m_resolution = 0;
    other.m_hasMipmap = false;
    other.m_faceLoaded = std::vector<bool>(6, false);
}

TextureCubemap& TextureCubemap::operator=(TextureCubemap&& other) noexcept {
    if (this != &other) {
        // 使用 scoped_lock 同时锁定两个互斥锁，避免死锁
        std::scoped_lock lock(m_mutex, other.m_mutex);
        
        // 释放当前立方体贴图
        if (m_textureID != 0) {
            GL_THREAD_CHECK();
            glDeleteTextures(1, &m_textureID);
            Logger::GetInstance().Debug("释放立方体贴图 ID: " + std::to_string(m_textureID));
        }

        m_textureID = other.m_textureID;
        m_resolution = other.m_resolution;
        m_format = other.m_format;
        m_hasMipmap = other.m_hasMipmap;
        m_faceLoaded = std::move(other.m_faceLoaded);

        other.m_textureID = 0;
        other.m_resolution = 0;
        other.m_hasMipmap = false;
        other.m_faceLoaded = std::vector<bool>(6, false);
    }
    return *this;
}

bool TextureCubemap::LoadFromFiles(const std::vector<std::string>& filepaths, bool generateMipmap) {
    if (filepaths.size() != 6) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::InvalidArgument,
                                 "TextureCubemap::LoadFromFiles: 需要6个文件路径，提供: " +
                                 std::to_string(filepaths.size())));
        return false;
    }

    // 立方体贴图面的顺序：+X, -X, +Y, -Y, +Z, -Z
    CubemapFace faces[] = {
        CubemapFace::PositiveX,
        CubemapFace::NegativeX,
        CubemapFace::PositiveY,
        CubemapFace::NegativeY,
        CubemapFace::PositiveZ,
        CubemapFace::NegativeZ
    };

    std::lock_guard<std::mutex> lock(m_mutex);

    // 释放旧立方体贴图
    if (m_textureID != 0) {
        GL_THREAD_CHECK();
        glDeleteTextures(1, &m_textureID);
        m_textureID = 0;
    }

    // 重置状态
    m_resolution = 0;
    m_format = TextureFormat::RGBA;
    m_hasMipmap = false;
    std::fill(m_faceLoaded.begin(), m_faceLoaded.end(), false);

    // 创建OpenGL立方体贴图
    GL_THREAD_CHECK();
    glGenTextures(1, &m_textureID);
    if (m_textureID == 0) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::ResourceLoadFailed,
                                 "TextureCubemap::LoadFromFiles: 无法生成立方体贴图ID"));
        return false;
    }

    glBindTexture(GL_TEXTURE_CUBE_MAP, m_textureID);

    // 加载每个面
    bool allLoaded = true;
    for (size_t i = 0; i < 6; ++i) {
        if (!LoadFaceFromFile(faces[i], filepaths[i])) {
            Logger::GetInstance().Error("加载立方体贴图面失败: " + filepaths[i]);
            allLoaded = false;
            break;
        }
    }

    if (!allLoaded) {
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        glDeleteTextures(1, &m_textureID);
        m_textureID = 0;
        std::fill(m_faceLoaded.begin(), m_faceLoaded.end(), false);
        return false;
    }

    // 设置默认参数
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, 
                   generateMipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    // 生成Mipmap
    if (generateMipmap) {
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
        m_hasMipmap = true;
    }

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    Logger::GetInstance().Info("成功加载立方体贴图: " + std::to_string(m_resolution) + "x" + 
                 std::to_string(m_resolution) + " (6面)");

    return true;
}

bool TextureCubemap::LoadFace(CubemapFace face, const std::string& filepath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return LoadFaceFromFile(face, filepath);
}

bool TextureCubemap::LoadFaceFromFile(CubemapFace face, const std::string& filepath) {
    if (filepath.empty()) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::InvalidArgument,
                                 "TextureCubemap::LoadFaceFromFile: 文件路径为空"));
        return false;
    }

    // 在锁外加载图片文件（避免长时间持锁）
    SDL_Surface* surface = IMG_Load(filepath.c_str());
    if (!surface) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::FileOpenFailed,
                                 "TextureCubemap::LoadFaceFromFile: 加载纹理失败: " + filepath +
                                 " - " + std::string(SDL_GetError())));
        return false;
    }

    // 确定纹理格式
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
        SDL_PixelFormat pixelFormat = SDL_PIXELFORMAT_RGBA32;
        SDL_Surface* convertedSurface = SDL_ConvertSurface(surface, pixelFormat);
        SDL_DestroySurface(surface);
        
        if (!convertedSurface) {
            HANDLE_ERROR(RENDER_ERROR(ErrorCode::ResourceInvalidFormat,
                                     "TextureCubemap::LoadFaceFromFile: 转换纹理格式失败: " +
                                     std::string(SDL_GetError())));
            return false;
        }
        
        surface = convertedSurface;
        format = TextureFormat::RGBA;
    }

    int width = surface->w;
    int height = surface->h;

    // 检查是否为正方形
    if (width != height) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::InvalidArgument,
                                 "TextureCubemap::LoadFaceFromFile: 立方体贴图面必须是正方形: " +
                                 std::to_string(width) + "x" + std::to_string(height)));
        SDL_DestroySurface(surface);
        return false;
    }

    // 如果是第一个面，设置分辨率；否则检查分辨率是否一致
    if (m_resolution == 0) {
        m_resolution = width;
        m_format = format;
    } else if (m_resolution != width) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::InvalidArgument,
                                 "TextureCubemap::LoadFaceFromFile: 立方体贴图面分辨率不一致: " +
                                 std::to_string(m_resolution) + " vs " + std::to_string(width)));
        SDL_DestroySurface(surface);
        return false;
    } else if (m_format != format) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::InvalidArgument,
                                 "TextureCubemap::LoadFaceFromFile: 立方体贴图面格式不一致"));
        SDL_DestroySurface(surface);
        return false;
    }

    // 如果立方体贴图还未创建，先创建
    if (m_textureID == 0) {
        GL_THREAD_CHECK();
        glGenTextures(1, &m_textureID);
        if (m_textureID == 0) {
            HANDLE_ERROR(RENDER_ERROR(ErrorCode::ResourceLoadFailed,
                                     "TextureCubemap::LoadFaceFromFile: 无法生成立方体贴图ID"));
            SDL_DestroySurface(surface);
            return false;
        }
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_textureID);
        
        // 设置默认参数
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    } else {
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_textureID);
    }

    // 上传纹理数据
    GLenum glFormat = ToGLFormat(format);
    GLenum glInternalFormat = ToGLInternalFormat(format);
    GLenum glFace = static_cast<GLenum>(face);

    glTexImage2D(glFace, 0, glInternalFormat, width, height,
                 0, glFormat, GL_UNSIGNED_BYTE, surface->pixels);

    // 验证是否成功
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        Logger::GetInstance().Error("glTexImage2D 失败，OpenGL 错误: " + std::to_string(err));
        SDL_DestroySurface(surface);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        return false;
    }

    // 标记该面已加载
    int faceIndex = static_cast<int>(face) - GL_TEXTURE_CUBE_MAP_POSITIVE_X;
    if (faceIndex >= 0 && faceIndex < 6) {
        m_faceLoaded[faceIndex] = true;
    }

    SDL_DestroySurface(surface);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    Logger::GetInstance().Debug("加载立方体贴图面: " + filepath + " (" +
                 std::to_string(width) + "x" + std::to_string(height) + ")");

    return true;
}

bool TextureCubemap::LoadFromHDRI(const std::string& hdriPath, int resolution, bool generateMipmap) {
    // TODO: 实现HDRI加载
    // 需要集成stb_image库来加载HDR文件
    // 然后使用等距柱状投影到立方体贴图的转换着色器
    HANDLE_ERROR(RENDER_ERROR(ErrorCode::NotImplemented,
                             "TextureCubemap::LoadFromHDRI: HDRI加载功能尚未实现，需要stb_image库支持"));
    return false;
}

bool TextureCubemap::CreateFaceFromData(CubemapFace face, const void* data, int width, int height,
                                       TextureFormat format) {
    if (width <= 0 || height <= 0 || width != height) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::InvalidArgument,
                                 "TextureCubemap::CreateFaceFromData: 无效的纹理尺寸或非正方形"));
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // 如果是第一个面，设置分辨率；否则检查分辨率是否一致
    if (m_resolution == 0) {
        m_resolution = width;
        m_format = format;
    } else if (m_resolution != width) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::InvalidArgument,
                                 "TextureCubemap::CreateFaceFromData: 立方体贴图面分辨率不一致"));
        return false;
    } else if (m_format != format) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::InvalidArgument,
                                 "TextureCubemap::CreateFaceFromData: 立方体贴图面格式不一致"));
        return false;
    }

    // 如果立方体贴图还未创建，先创建
    if (m_textureID == 0) {
        GL_THREAD_CHECK();
        glGenTextures(1, &m_textureID);
        if (m_textureID == 0) {
            HANDLE_ERROR(RENDER_ERROR(ErrorCode::ResourceLoadFailed,
                                     "TextureCubemap::CreateFaceFromData: 无法生成立方体贴图ID"));
            return false;
        }
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_textureID);
        
        // 设置默认参数
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    } else {
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_textureID);
    }

    // 上传纹理数据
    GLenum glFormat = ToGLFormat(format);
    GLenum glInternalFormat = ToGLInternalFormat(format);
    GLenum glFace = static_cast<GLenum>(face);

    glTexImage2D(glFace, 0, glInternalFormat, width, height,
                 0, glFormat, GL_UNSIGNED_BYTE, data);

    // 验证是否成功
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        Logger::GetInstance().Error("glTexImage2D 失败，OpenGL 错误: " + std::to_string(err));
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        return false;
    }

    // 标记该面已加载
    int faceIndex = static_cast<int>(face) - GL_TEXTURE_CUBE_MAP_POSITIVE_X;
    if (faceIndex >= 0 && faceIndex < 6) {
        m_faceLoaded[faceIndex] = true;
    }

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    return true;
}

bool TextureCubemap::CreateEmpty(int resolution, TextureFormat format) {
    if (resolution <= 0) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::InvalidArgument,
                                 "TextureCubemap::CreateEmpty: 无效的分辨率"));
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // 释放旧立方体贴图
    if (m_textureID != 0) {
        GL_THREAD_CHECK();
        glDeleteTextures(1, &m_textureID);
        m_textureID = 0;
    }

    m_resolution = resolution;
    m_format = format;
    m_hasMipmap = false;
    std::fill(m_faceLoaded.begin(), m_faceLoaded.end(), false);

    GL_THREAD_CHECK();
    glGenTextures(1, &m_textureID);
    if (m_textureID == 0) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::ResourceLoadFailed,
                                 "TextureCubemap::CreateEmpty: 无法生成立方体贴图ID"));
        return false;
    }

    glBindTexture(GL_TEXTURE_CUBE_MAP, m_textureID);

    GLenum glFormat = ToGLFormat(format);
    GLenum glInternalFormat = ToGLInternalFormat(format);

    // 创建6个空面
    for (int i = 0; i < 6; ++i) {
        GLenum face = GL_TEXTURE_CUBE_MAP_POSITIVE_X + i;
        glTexImage2D(face, 0, glInternalFormat, resolution, resolution,
                     0, glFormat, GL_UNSIGNED_BYTE, nullptr);
    }

    // 设置默认参数
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    // 标记所有面已加载（虽然是空的）
    std::fill(m_faceLoaded.begin(), m_faceLoaded.end(), true);

    Logger::GetInstance().Debug("创建空立方体贴图: " + std::to_string(resolution) + "x" +
                 std::to_string(resolution));

    return true;
}

void TextureCubemap::Bind(unsigned int unit) const {
    GLuint textureID;
    
    // 阶段1：快速读取纹理ID（持锁）
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        textureID = m_textureID;
    }  // 锁释放
    
    // 阶段2：OpenGL调用（无锁）
    if (textureID == 0) {
        Logger::GetInstance().Warning("尝试绑定无效立方体贴图");
        return;
    }

    if (unit > 31) {
        Logger::GetInstance().Warning("纹理单元超出范围: " + std::to_string(unit));
        unit = 0;
    }

    GL_THREAD_CHECK();
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);
}

void TextureCubemap::Unbind() const {
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

void TextureCubemap::SetFilter(TextureFilter minFilter, TextureFilter magFilter) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_textureID == 0) {
        Logger::GetInstance().Warning("无法为无效立方体贴图设置过滤器");
        return;
    }

    GL_THREAD_CHECK();
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_textureID);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER,
                   ToGLFilter(minFilter, m_hasMipmap));
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER,
                   ToGLFilter(magFilter, false));
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

void TextureCubemap::SetWrap(TextureWrap wrapS, TextureWrap wrapT, TextureWrap wrapR) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_textureID == 0) {
        Logger::GetInstance().Warning("无法为无效立方体贴图设置环绕模式");
        return;
    }

    GL_THREAD_CHECK();
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_textureID);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, ToGLWrap(wrapS));
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, ToGLWrap(wrapT));
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, ToGLWrap(wrapR));
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

void TextureCubemap::GenerateMipmap() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_textureID == 0) {
        Logger::GetInstance().Warning("无法为无效立方体贴图生成 Mipmap");
        return;
    }

    GL_THREAD_CHECK();
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_textureID);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    m_hasMipmap = true;
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    Logger::GetInstance().Debug("为立方体贴图生成 Mipmap，ID: " + std::to_string(m_textureID));
}

void TextureCubemap::Release() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_textureID != 0) {
        GL_THREAD_CHECK();
        glDeleteTextures(1, &m_textureID);
        Logger::GetInstance().Debug("释放立方体贴图 ID: " + std::to_string(m_textureID));
        m_textureID = 0;
    }

    m_resolution = 0;
    m_hasMipmap = false;
    std::fill(m_faceLoaded.begin(), m_faceLoaded.end(), false);
}

bool TextureCubemap::IsComplete() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return std::all_of(m_faceLoaded.begin(), m_faceLoaded.end(), [](bool loaded) { return loaded; });
}

GLenum TextureCubemap::ToGLFormat(TextureFormat format) const {
    switch (format) {
        case TextureFormat::RGB:          return GL_RGB;
        case TextureFormat::RGBA:         return GL_RGBA;
        case TextureFormat::RED:          return GL_RED;
        case TextureFormat::RG:           return GL_RG;
        case TextureFormat::Depth:       return GL_DEPTH_COMPONENT;
        case TextureFormat::DepthStencil: return GL_DEPTH_STENCIL;
        default:                          return GL_RGBA;
    }
}

GLenum TextureCubemap::ToGLInternalFormat(TextureFormat format) const {
    switch (format) {
        case TextureFormat::RGB:          return GL_RGB8;
        case TextureFormat::RGBA:         return GL_RGBA8;
        case TextureFormat::RED:          return GL_R8;
        case TextureFormat::RG:           return GL_RG8;
        case TextureFormat::Depth:       return GL_DEPTH_COMPONENT24;
        case TextureFormat::DepthStencil: return GL_DEPTH24_STENCIL8;
        default:                          return GL_RGBA8;
    }
}

GLenum TextureCubemap::ToGLFilter(TextureFilter filter, bool isMipmap) const {
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

GLenum TextureCubemap::ToGLWrap(TextureWrap wrap) const {
    switch (wrap) {
        case TextureWrap::Repeat:         return GL_REPEAT;
        case TextureWrap::MirroredRepeat: return GL_MIRRORED_REPEAT;
        case TextureWrap::ClampToEdge:    return GL_CLAMP_TO_EDGE;
        case TextureWrap::ClampToBorder:  return GL_CLAMP_TO_BORDER;
        default:                          return GL_REPEAT;
    }
}

size_t TextureCubemap::GetMemoryUsage() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_textureID == 0 || m_resolution == 0) {
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
            bytesPerPixel = 4;
            break;
        case TextureFormat::DepthStencil:
            bytesPerPixel = 4;
            break;
    }
    
    // 立方体贴图有6个面
    size_t faceSize = static_cast<size_t>(m_resolution) * static_cast<size_t>(m_resolution) * bytesPerPixel;
    size_t totalSize = faceSize * 6;
    
    // 如果有 mipmap，大约增加 1/3 的内存
    if (m_hasMipmap) {
        totalSize = totalSize * 4 / 3;
    }
    
    return totalSize;
}

} // namespace Render
