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

#include <string>
#include <memory>
#include <mutex>
#include <glad/glad.h>

namespace Render {

/**
 * @brief 纹理过滤模式
 */
enum class TextureFilter {
    Nearest,    // 最近邻过滤
    Linear,     // 线性过滤
    Mipmap      // Mipmap 过滤
};

/**
 * @brief 纹理环绕模式
 */
enum class TextureWrap {
    Repeat,         // 重复
    MirroredRepeat, // 镜像重复
    ClampToEdge,    // 边缘截取
    ClampToBorder   // 边界颜色
};

/**
 * @brief 纹理格式
 */
enum class TextureFormat {
    RGB,            // RGB 格式
    RGBA,           // RGBA 格式
    RED,            // 单通道红色
    RG,             // 双通道
    Depth,          // 深度格式
    DepthStencil    // 深度模板格式
};

/**
 * @brief 纹理类
 * 
 * 负责纹理的创建、加载、参数设置和释放
 * 
 * 线程安全：
 * - 所有公共方法都是线程安全的
 * - 使用互斥锁保护所有成员变量的访问
 * - 注意：OpenGL 调用必须在创建上下文的线程中执行
 */
class Texture {
public:
    /**
     * @brief 构造函数
     */
    Texture();

    /**
     * @brief 析构函数
     */
    ~Texture();

    // 禁用拷贝
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    // 允许移动
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    /**
     * @brief 从文件加载纹理（使用 SDL_image）
     * @param filepath 纹理文件路径
     * @param generateMipmap 是否生成 Mipmap
     * @return 是否加载成功
     */
    bool LoadFromFile(const std::string& filepath, bool generateMipmap = true);

    /**
     * @brief 从内存数据创建纹理
     * @param data 纹理数据指针
     * @param width 纹理宽度
     * @param height 纹理高度
     * @param format 纹理格式
     * @param generateMipmap 是否生成 Mipmap
     * @return 是否创建成功
     */
    bool CreateFromData(const void* data, int width, int height, 
                       TextureFormat format = TextureFormat::RGBA,
                       bool generateMipmap = true);

    /**
     * @brief 创建空纹理（用于渲染目标等）
     * @param width 纹理宽度
     * @param height 纹理高度
     * @param format 纹理格式
     * @return 是否创建成功
     */
    bool CreateEmpty(int width, int height, TextureFormat format = TextureFormat::RGBA);

    /**
     * @brief 绑定纹理到指定纹理单元
     * @param unit 纹理单元索引（0-31）
     */
    void Bind(unsigned int unit = 0) const;

    /**
     * @brief 解绑纹理
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
     */
    void SetWrap(TextureWrap wrapS, TextureWrap wrapT);

    /**
     * @brief 生成 Mipmap
     */
    void GenerateMipmap();

    /**
     * @brief 释放纹理资源
     */
    void Release();

    /**
     * @brief 获取纹理 ID
     */
    GLuint GetID() const { return m_textureID; }

    /**
     * @brief 获取纹理宽度
     */
    int GetWidth() const { return m_width; }

    /**
     * @brief 获取纹理高度
     */
    int GetHeight() const { return m_height; }

    /**
     * @brief 获取纹理格式
     */
    TextureFormat GetFormat() const { return m_format; }

    /**
     * @brief 检查纹理是否有效
     */
    bool IsValid() const { return m_textureID != 0; }

    /**
     * @brief 获取纹理内存使用量（估算，字节）
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

private:
    GLuint m_textureID;          ///< OpenGL 纹理 ID
    int m_width;                 ///< 纹理宽度
    int m_height;                ///< 纹理高度
    TextureFormat m_format;      ///< 纹理格式
    bool m_hasMipmap;            ///< 是否有 Mipmap
    mutable std::mutex m_mutex;  ///< 线程安全互斥锁
};

/**
 * @brief 纹理智能指针类型
 */
using TexturePtr = std::shared_ptr<Texture>;

} // namespace Render

