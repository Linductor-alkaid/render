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
#include "render/texture_loader.h"
#include "render/shader.h"
#include "render/framebuffer.h"
#include "render/types.h"
#include "render/math_utils.h"
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

    // 确定纹理格式并转换为标准格式（确保像素顺序正确）
    // 无论原始格式如何，都转换为标准格式以确保像素顺序正确
    // 这对于避免BGR/BGRA等格式导致的渲染问题很重要
    TextureFormat format = TextureFormat::RGBA;
    int bytesPerPixel = SDL_BYTESPERPIXEL(surface->format);
    SDL_Surface* convertedSurface = nullptr;
    
    if (bytesPerPixel == 4) {
        // 转换为RGBA32格式（确保像素顺序为RGBA，SDL3会根据平台自动处理字节序）
        convertedSurface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
        if (convertedSurface) {
            SDL_DestroySurface(surface);
            surface = convertedSurface;
            format = TextureFormat::RGBA;
        } else {
            HANDLE_ERROR(RENDER_ERROR(ErrorCode::ResourceInvalidFormat,
                                     "TextureCubemap::LoadFaceFromFile: 转换RGBA格式失败: " +
                                     std::string(SDL_GetError())));
            SDL_DestroySurface(surface);
            return false;
        }
    } else if (bytesPerPixel == 3) {
        // 对于RGB格式，也转换为RGBA32（添加alpha通道，更安全）
        // 这样可以确保像素顺序正确，避免BGR格式的问题
        convertedSurface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
        if (convertedSurface) {
            SDL_DestroySurface(surface);
            surface = convertedSurface;
            format = TextureFormat::RGBA;
        } else {
            HANDLE_ERROR(RENDER_ERROR(ErrorCode::ResourceInvalidFormat,
                                     "TextureCubemap::LoadFaceFromFile: 转换RGB到RGBA格式失败: " +
                                     std::string(SDL_GetError())));
            SDL_DestroySurface(surface);
            return false;
        }
    } else if (bytesPerPixel == 1) {
        // 单通道格式，转换为RGBA32（添加RGB通道，alpha设为255）
        convertedSurface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
        if (convertedSurface) {
            SDL_DestroySurface(surface);
            surface = convertedSurface;
            format = TextureFormat::RGBA;
        } else {
            HANDLE_ERROR(RENDER_ERROR(ErrorCode::ResourceInvalidFormat,
                                     "TextureCubemap::LoadFaceFromFile: 转换单通道到RGBA格式失败: " +
                                     std::string(SDL_GetError())));
            SDL_DestroySurface(surface);
            return false;
        }
    } else {
        // 不支持的格式，转换为RGBA
        Logger::GetInstance().Warning("不支持的纹理格式，转换为 RGBA");
        convertedSurface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
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
    // 注意：SDL_ConvertSurface已经确保了像素格式正确
    // SDL_PIXELFORMAT_RGBA32在小端系统上是ABGR8888，但SDL_ConvertSurface会处理字节序
    // 直接使用surface->pixels，SDL已经确保格式正确
    GLenum glFormat = ToGLFormat(format);
    GLenum glInternalFormat = ToGLInternalFormat(format);
    GLenum glFace = static_cast<GLenum>(face);

    glTexImage2D(glFace, 0, glInternalFormat, width, height,
                 0, glFormat, GL_UNSIGNED_BYTE, surface->pixels);

    // 验证是否成功
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        Logger::GetInstance().Error("glTexImage2D 失败，OpenGL 错误: " + std::to_string(err) + 
                                   " (面: " + std::to_string(static_cast<int>(face)) + 
                                   ", 格式: " + std::to_string(glFormat) + 
                                   ", 内部格式: " + std::to_string(glInternalFormat) + ")");
        SDL_DestroySurface(surface);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        return false;
    }
    
    // 调试信息：检查SDL surface格式和像素值（仅第一个面）
    if (face == CubemapFace::PositiveX && surface->pixels) {
        Logger::GetInstance().Debug("立方体贴图面 +X 格式信息:");
        Logger::GetInstance().Debug("  SDL格式: " + std::to_string(static_cast<int>(surface->format)));
        Logger::GetInstance().Debug("  OpenGL格式: " + std::to_string(glFormat));
        Logger::GetInstance().Debug("  OpenGL内部格式: " + std::to_string(glInternalFormat));
        
        const unsigned char* pixels = static_cast<const unsigned char*>(surface->pixels);
        Logger::GetInstance().Debug("  前4个像素值 (RGBA顺序): " +
                                   std::to_string(pixels[0]) + ", " + std::to_string(pixels[1]) + ", " +
                                   std::to_string(pixels[2]) + ", " + std::to_string(pixels[3]));
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
    if (hdriPath.empty()) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::InvalidArgument,
                                 "TextureCubemap::LoadFromHDRI: HDRI文件路径为空"));
        return false;
    }

    if (resolution <= 0) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::InvalidArgument,
                                 "TextureCubemap::LoadFromHDRI: 无效的分辨率"));
        return false;
    }

    GL_THREAD_CHECK();

    std::lock_guard<std::mutex> lock(m_mutex);

    // 释放旧立方体贴图
    if (m_textureID != 0) {
        glDeleteTextures(1, &m_textureID);
        m_textureID = 0;
    }

    // 重置状态
    m_resolution = 0;
    m_format = TextureFormat::RGBA;
    m_hasMipmap = false;
    std::fill(m_faceLoaded.begin(), m_faceLoaded.end(), false);

    // 使用 TextureLoader 加载等距柱状投影图像
    // 注意：SDL_image 可能不支持 HDR 格式，但我们可以先尝试加载
    // 如果失败，会返回 nullptr，我们可以给出错误提示
    Logger::GetInstance().Info("加载等距柱状投影图像: " + hdriPath);
    
    auto equirectangularTexture = TextureLoader::GetInstance().LoadTexture(
        "equirectangular_temp_" + hdriPath, hdriPath, false);
    
    if (!equirectangularTexture) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::FileOpenFailed,
                                 "TextureCubemap::LoadFromHDRI: 无法加载等距柱状投影图像: " + hdriPath +
                                 " (SDL_image 可能不支持 HDR 格式，请使用 PNG/JPG 等格式的等距柱状投影图像)"));
        return false;
    }

    // 创建转换着色器
    auto conversionShader = std::make_unique<Shader>();
    if (!conversionShader->LoadFromFile(
            "shaders/equirectangular_to_cubemap.vert",
            "shaders/equirectangular_to_cubemap.frag")) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::ResourceLoadFailed,
                                 "TextureCubemap::LoadFromHDRI: 无法加载转换着色器"));
        return false;
    }

    // 创建立方体贴图
    glGenTextures(1, &m_textureID);
    if (m_textureID == 0) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::ResourceLoadFailed,
                                 "TextureCubemap::LoadFromHDRI: 无法生成立方体贴图ID"));
        return false;
    }

    glBindTexture(GL_TEXTURE_CUBE_MAP, m_textureID);
    
    // 为每个面创建空的纹理
    for (int i = 0; i < 6; ++i) {
        GLenum face = GL_TEXTURE_CUBE_MAP_POSITIVE_X + i;
        glTexImage2D(face, 0, GL_RGBA8, resolution, resolution, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }

    // 设置纹理参数
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER,
                   generateMipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    // 创建帧缓冲对象用于渲染到立方体贴图的每个面
    FramebufferConfig fboConfig;
    fboConfig.SetSize(resolution, resolution)
             .AddColorAttachment(TextureFormat::RGBA)
             .SetName("EquirectangularToCubemap");

    Framebuffer fbo;
    if (!fbo.Create(fboConfig)) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::ResourceLoadFailed,
                                 "TextureCubemap::LoadFromHDRI: 无法创建帧缓冲对象"));
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        glDeleteTextures(1, &m_textureID);
        m_textureID = 0;
        return false;
    }

    // 创建立方体顶点数据（用于渲染）
    // 立方体的8个顶点
    float cubeVertices[] = {
        // 位置 (x, y, z)
        -1.0f, -1.0f, -1.0f,  // 0: 左下后
         1.0f, -1.0f, -1.0f,  // 1: 右下后
         1.0f,  1.0f, -1.0f,  // 2: 右上后
        -1.0f,  1.0f, -1.0f,  // 3: 左上后
        -1.0f, -1.0f,  1.0f,  // 4: 左下前
         1.0f, -1.0f,  1.0f,  // 5: 右下前
         1.0f,  1.0f,  1.0f,  // 6: 右上前
        -1.0f,  1.0f,  1.0f   // 7: 左上前
    };

    // 立方体的索引（每个面2个三角形）
    unsigned int cubeIndices[] = {
        // 后面 (-Z)
        0, 1, 2,  2, 3, 0,
        // 前面 (+Z)
        4, 5, 6,  6, 7, 4,
        // 左面 (-X)
        0, 3, 7,  7, 4, 0,
        // 右面 (+X)
        1, 5, 6,  6, 2, 1,
        // 下面 (-Y)
        0, 4, 5,  5, 1, 0,
        // 上面 (+Y)
        3, 7, 6,  6, 2, 3
    };

    // 创建 VAO, VBO, EBO
    GLuint VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);

    // 设置顶点属性
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // 设置视口
    glViewport(0, 0, resolution, resolution);

    // 为每个立方体贴图面设置视图矩阵
    using namespace MathUtils;
    Matrix4 captureProjection = PerspectiveDegrees(90.0f, 1.0f, 0.1f, 10.0f);
    Matrix4 captureViews[] = {
        LookAt(Vector3(0.0f, 0.0f, 0.0f), Vector3( 1.0f,  0.0f,  0.0f), Vector3(0.0f, -1.0f,  0.0f)), // +X
        LookAt(Vector3(0.0f, 0.0f, 0.0f), Vector3(-1.0f,  0.0f,  0.0f), Vector3(0.0f, -1.0f,  0.0f)), // -X
        LookAt(Vector3(0.0f, 0.0f, 0.0f), Vector3( 0.0f,  1.0f,  0.0f), Vector3(0.0f,  0.0f,  1.0f)), // +Y
        LookAt(Vector3(0.0f, 0.0f, 0.0f), Vector3( 0.0f, -1.0f,  0.0f), Vector3(0.0f,  0.0f, -1.0f)), // -Y
        LookAt(Vector3(0.0f, 0.0f, 0.0f), Vector3( 0.0f,  0.0f,  1.0f), Vector3(0.0f, -1.0f,  0.0f)), // +Z
        LookAt(Vector3(0.0f, 0.0f, 0.0f), Vector3( 0.0f,  0.0f, -1.0f), Vector3(0.0f, -1.0f,  0.0f))  // -Z
    };

    // 使用着色器
    conversionShader->Use();
    auto* uniformMgr = conversionShader->GetUniformManager();
    
    // 绑定等距柱状投影纹理
    equirectangularTexture->Bind(0);
    uniformMgr->SetInt("uEquirectangularMap", 0);

    // 设置投影矩阵（90度视野，用于立方体贴图）
    uniformMgr->SetMatrix4("uProjection", captureProjection);

    // 渲染到每个立方体贴图面
    for (int i = 0; i < 6; ++i) {
        // 绑定帧缓冲，将颜色附件绑定到立方体贴图面
        fbo.Bind();
        
        // 将帧缓冲的颜色附件绑定到立方体贴图面
        GLenum face = GL_TEXTURE_CUBE_MAP_POSITIVE_X + i;
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, face, m_textureID, 0);
        
        // 检查帧缓冲完整性
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            Logger::GetInstance().Error("帧缓冲不完整，面: " + std::to_string(i));
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glBindVertexArray(0);
            glDeleteVertexArrays(1, &VAO);
            glDeleteBuffers(1, &VBO);
            glDeleteBuffers(1, &EBO);
            glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
            glDeleteTextures(1, &m_textureID);
            m_textureID = 0;
            return false;
        }

        // 清空帧缓冲
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // 设置视图矩阵（每个面使用不同的视图）
        uniformMgr->SetMatrix4("uView", captureViews[i]);

        // 渲染立方体
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        
        // 标记该面已加载
        m_faceLoaded[i] = true;
    }

    // 清理
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindVertexArray(0);
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);

    // 生成 Mipmap
    if (generateMipmap) {
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_textureID);
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
        m_hasMipmap = true;
    }

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    m_resolution = resolution;
    m_format = TextureFormat::RGBA;

    Logger::GetInstance().Info("成功从等距柱状投影图像转换为立方体贴图: " + hdriPath +
                 " (" + std::to_string(resolution) + "x" + std::to_string(resolution) + ")");

    return true;
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
