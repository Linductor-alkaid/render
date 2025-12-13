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
#pragma once

#include "render/texture.h"
#include <string>
#include <memory>
#include <mutex>
#include <vector>
#include <glad/glad.h>

namespace Render {

/**
 * @brief 立方体贴图面枚举
 */
enum class CubemapFace {
    PositiveX = GL_TEXTURE_CUBE_MAP_POSITIVE_X,  // +X 面（右）
    NegativeX = GL_TEXTURE_CUBE_MAP_NEGATIVE_X,  // -X 面（左）
    PositiveY = GL_TEXTURE_CUBE_MAP_POSITIVE_Y,  // +Y 面（上）
    NegativeY = GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,  // -Y 面（下）
    PositiveZ = GL_TEXTURE_CUBE_MAP_POSITIVE_Z,  // +Z 面（前）
    NegativeZ = GL_TEXTURE_CUBE_MAP_NEGATIVE_Z   // -Z 面（后）
};

/**
 * @brief 立方体贴图类
 * 
 * 负责立方体贴图的创建、加载、参数设置和释放。
 * 立方体贴图用于环境映射、IBL（基于图像的光照）等高级渲染技术。
 * 
 * 线程安全：
 * - 所有公共方法都是线程安全的
 * - 使用互斥锁保护所有成员变量的访问
 * - 注意：OpenGL 调用必须在创建上下文的线程中执行（通常是主线程）
 * 
 * 使用示例：
 * @code
 * // 从6个面加载
 * auto cubemap = std::make_shared<TextureCubemap>();
 * cubemap->LoadFromFiles({
 *     "textures/skybox/right.png",   // +X
 *     "textures/skybox/left.png",    // -X
 *     "textures/skybox/top.png",     // +Y
 *     "textures/skybox/bottom.png",  // -Y
 *     "textures/skybox/front.png",   // +Z
 *     "textures/skybox/back.png"     // -Z
 * });
 * 
 * // 从HDRI加载
 * cubemap->LoadFromHDRI("environments/sunset.hdr");
 * 
 * // 绑定使用
 * cubemap->Bind(0);
 * @endcode
 */
class TextureCubemap {
public:
    /**
     * @brief 构造函数
     */
    TextureCubemap();

    /**
     * @brief 析构函数
     */
    ~TextureCubemap();

    // 禁用拷贝
    TextureCubemap(const TextureCubemap&) = delete;
    TextureCubemap& operator=(const TextureCubemap&) = delete;

    // 允许移动
    TextureCubemap(TextureCubemap&& other) noexcept;
    TextureCubemap& operator=(TextureCubemap&& other) noexcept;

    /**
     * @brief 从6个图像文件加载立方体贴图
     * @param filepaths 6个面的文件路径，顺序为：+X, -X, +Y, -Y, +Z, -Z
     * @param generateMipmap 是否生成 Mipmap
     * @return 是否加载成功
     */
    bool LoadFromFiles(const std::vector<std::string>& filepaths, bool generateMipmap = true);

    /**
     * @brief 从单个面加载立方体贴图
     * @param face 立方体贴图面
     * @param filepath 文件路径
     * @return 是否加载成功
     */
    bool LoadFace(CubemapFace face, const std::string& filepath);

    /**
     * @brief 从HDRI文件加载并转换为立方体贴图
     * @param hdriPath HDRI文件路径（.hdr格式）
     * @param resolution 立方体贴图每面的分辨率（默认512）
     * @param generateMipmap 是否生成 Mipmap
     * @return 是否加载成功
     * 
     * @note 需要stb_image库支持HDR格式
     */
    bool LoadFromHDRI(const std::string& hdriPath, int resolution = 512, bool generateMipmap = true);

    /**
     * @brief 从内存数据创建立方体贴图面
     * @param face 立方体贴图面
     * @param data 纹理数据指针
     * @param width 纹理宽度
     * @param height 纹理高度
     * @param format 纹理格式
     * @return 是否创建成功
     */
    bool CreateFaceFromData(CubemapFace face, const void* data, int width, int height,
                           TextureFormat format = TextureFormat::RGBA);

    /**
     * @brief 创建空立方体贴图（用于渲染目标等）
     * @param resolution 每面的分辨率（立方体贴图是正方形）
     * @param format 纹理格式
     * @return 是否创建成功
     */
    bool CreateEmpty(int resolution, TextureFormat format = TextureFormat::RGBA);

    /**
     * @brief 绑定立方体贴图到指定纹理单元
     * @param unit 纹理单元索引（0-31）
     */
    void Bind(unsigned int unit = 0) const;

    /**
     * @brief 解绑立方体贴图
     */
    void Unbind() const;

    /**
     * @brief 设置纹理过滤模式
     * @param minFilter 缩小过滤模式
     * @param magFilter 放大过滤模式
     */
    void SetFilter(TextureFilter minFilter, TextureFilter magFilter);

    /**
     * @brief 设置纹理环绕模式
     * @param wrapS S轴环绕模式
     * @param wrapT T轴环绕模式
     * @param wrapR R轴环绕模式（立方体贴图特有）
     */
    void SetWrap(TextureWrap wrapS, TextureWrap wrapT, TextureWrap wrapR = TextureWrap::ClampToEdge);

    /**
     * @brief 生成 Mipmap
     */
    void GenerateMipmap();

    /**
     * @brief 释放立方体贴图资源
     */
    void Release();

    /**
     * @brief 获取立方体贴图 ID
     */
    GLuint GetID() const { return m_textureID; }

    /**
     * @brief 获取立方体贴图分辨率（每面的宽度/高度，立方体贴图是正方形）
     */
    int GetResolution() const { return m_resolution; }

    /**
     * @brief 获取纹理格式
     */
    TextureFormat GetFormat() const { return m_format; }

    /**
     * @brief 检查立方体贴图是否有效
     */
    bool IsValid() const { return m_textureID != 0; }

    /**
     * @brief 检查所有6个面是否都已加载
     */
    bool IsComplete() const;

    /**
     * @brief 获取立方体贴图内存使用量（估算，字节）
     */
    size_t GetMemoryUsage() const;

private:
    /**
     * @brief 转换纹理格式到 OpenGL 格式
     */
    GLenum ToGLFormat(TextureFormat format) const;

    /**
     * @brief 转换纹理格式到 OpenGL 内部格式
     */
    GLenum ToGLInternalFormat(TextureFormat format) const;

    /**
     * @brief 转换过滤模式到 OpenGL 常量
     */
    GLenum ToGLFilter(TextureFilter filter, bool isMipmap) const;

    /**
     * @brief 转换环绕模式到 OpenGL 常量
     */
    GLenum ToGLWrap(TextureWrap wrap) const;

    /**
     * @brief 从文件加载单个面（内部方法）
     */
    bool LoadFaceFromFile(CubemapFace face, const std::string& filepath);

    /**
     * @brief 检查立方体贴图是否完整（所有6个面都已加载）
     */
    void CheckCompleteness();

private:
    GLuint m_textureID;                    ///< OpenGL 立方体贴图 ID
    int m_resolution;                      ///< 立方体贴图分辨率（每面的宽度/高度）
    TextureFormat m_format;                 ///< 纹理格式
    bool m_hasMipmap;                      ///< 是否有 Mipmap
    std::vector<bool> m_faceLoaded;        ///< 标记每个面是否已加载（6个元素）
    mutable std::mutex m_mutex;            ///< 线程安全互斥锁
};

/**
 * @brief 立方体贴图智能指针类型
 */
using TextureCubemapPtr = std::shared_ptr<TextureCubemap>;

} // namespace Render
