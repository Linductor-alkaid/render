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
/**
 * @file framebuffer.h
 * @brief 帧缓冲对象管理
 * 
 * 提供完整的 OpenGL 帧缓冲对象 (FBO) 管理，支持多种附件类型、
 * 多重采样抗锯齿 (MSAA)、多渲染目标 (MRT) 等高级功能。
 * 
 * @author RenderEngine
 * @date 2025-11-03
 * @version 1.0.0
 */

#pragma once

#include "types.h"
#include "texture.h"
#include <glad/glad.h>
#include <memory>
#include <vector>
#include <string>
#include <mutex>

namespace Render {

/**
 * @brief 帧缓冲附件类型
 */
enum class FramebufferAttachmentType {
    Color0 = 0,     ///< 颜色附件 0
    Color1,         ///< 颜色附件 1
    Color2,         ///< 颜色附件 2
    Color3,         ///< 颜色附件 3
    Color4,         ///< 颜色附件 4
    Color5,         ///< 颜色附件 5
    Color6,         ///< 颜色附件 6
    Color7,         ///< 颜色附件 7
    Depth,          ///< 深度附件
    Stencil,        ///< 模板附件
    DepthStencil    ///< 深度模板组合附件
};

/**
 * @brief 帧缓冲附件配置
 */
struct FramebufferAttachment {
    FramebufferAttachmentType type;  ///< 附件类型
    TextureFormat format;             ///< 纹理格式
    TextureFilter minFilter;          ///< 缩小过滤
    TextureFilter magFilter;          ///< 放大过滤
    TextureWrap wrapS;                ///< S轴环绕模式
    TextureWrap wrapT;                ///< T轴环绕模式
    bool useRenderbuffer;             ///< 是否使用渲染缓冲对象（不能采样）
    
    /**
     * @brief 默认构造函数
     */
    FramebufferAttachment()
        : type(FramebufferAttachmentType::Color0)
        , format(TextureFormat::RGBA)
        , minFilter(TextureFilter::Linear)
        , magFilter(TextureFilter::Linear)
        , wrapS(TextureWrap::ClampToEdge)
        , wrapT(TextureWrap::ClampToEdge)
        , useRenderbuffer(false)
    {}
    
    /**
     * @brief 创建颜色附件配置
     */
    static FramebufferAttachment Color(int index = 0, 
                                       TextureFormat format = TextureFormat::RGBA,
                                       bool useRBO = false);
    
    /**
     * @brief 创建深度附件配置
     */
    static FramebufferAttachment Depth(bool useRBO = false);
    
    /**
     * @brief 创建深度模板附件配置
     */
    static FramebufferAttachment DepthStencil(bool useRBO = true);
};

/**
 * @brief 帧缓冲配置
 */
struct FramebufferConfig {
    int width;                                          ///< 宽度
    int height;                                         ///< 高度
    std::vector<FramebufferAttachment> attachments;    ///< 附件列表
    int samples;                                        ///< 多重采样数量（1=无MSAA）
    std::string name;                                   ///< 调试名称
    
    /**
     * @brief 默认构造函数
     */
    FramebufferConfig()
        : width(1280)
        , height(720)
        , samples(1)
        , name("Framebuffer")
    {}
    
    /**
     * @brief 添加颜色附件
     */
    FramebufferConfig& AddColorAttachment(TextureFormat format = TextureFormat::RGBA,
                                          bool useRBO = false);
    
    /**
     * @brief 添加深度附件
     */
    FramebufferConfig& AddDepthAttachment(bool useRBO = false);
    
    /**
     * @brief 添加深度模板附件
     */
    FramebufferConfig& AddDepthStencilAttachment(bool useRBO = true);
    
    /**
     * @brief 设置尺寸
     */
    FramebufferConfig& SetSize(int w, int h);
    
    /**
     * @brief 设置多重采样
     */
    FramebufferConfig& SetSamples(int s);
    
    /**
     * @brief 设置名称
     */
    FramebufferConfig& SetName(const std::string& n);
};

/**
 * @brief 帧缓冲对象
 * 
 * 管理 OpenGL 帧缓冲对象，支持：
 * - 多种附件类型（颜色、深度、模板）
 * - 多重采样抗锯齿 (MSAA)
 * - 多渲染目标 (MRT)
 * - 纹理和渲染缓冲对象附件
 * - 完整性检查
 * - 线程安全
 * 
 * @note 所有公共方法都是线程安全的
 * @note OpenGL 调用必须在创建上下文的线程中执行
 */
class Framebuffer {
public:
    /**
     * @brief 默认构造函数
     */
    Framebuffer();
    
    /**
     * @brief 析构函数
     */
    ~Framebuffer();
    
    // 禁止拷贝
    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;
    
    // 允许移动
    Framebuffer(Framebuffer&& other) noexcept;
    Framebuffer& operator=(Framebuffer&& other) noexcept;
    
    /**
     * @brief 从配置创建帧缓冲
     * 
     * @param config 帧缓冲配置
     * @return 成功返回 true
     * 
     * @note 必须在 OpenGL 上下文线程中调用
     * @threadsafe 是
     */
    bool Create(const FramebufferConfig& config);
    
    /**
     * @brief 调整帧缓冲大小
     * 
     * @param width 新宽度
     * @param height 新高度
     * @return 成功返回 true
     * 
     * @note 必须在 OpenGL 上下文线程中调用
     * @note 会重新创建所有附件
     * @threadsafe 是
     */
    bool Resize(int width, int height);
    
    /**
     * @brief 绑定帧缓冲
     * 
     * @note 必须在 OpenGL 上下文线程中调用
     * @threadsafe 是
     */
    void Bind() const;
    
    /**
     * @brief 解绑帧缓冲（绑定默认帧缓冲）
     * 
     * @note 必须在 OpenGL 上下文线程中调用
     * @threadsafe 是
     */
    void Unbind() const;
    
    /**
     * @brief 绑定为读取帧缓冲
     * 
     * @note 必须在 OpenGL 上下文线程中调用
     * @threadsafe 是
     */
    void BindRead() const;
    
    /**
     * @brief 绑定为绘制帧缓冲
     * 
     * @note 必须在 OpenGL 上下文线程中调用
     * @threadsafe 是
     */
    void BindDraw() const;
    
    /**
     * @brief 清空帧缓冲
     * 
     * @param clearColor 是否清空颜色缓冲
     * @param clearDepth 是否清空深度缓冲
     * @param clearStencil 是否清空模板缓冲
     * 
     * @note 必须在 OpenGL 上下文线程中调用
     * @threadsafe 是
     */
    void Clear(bool clearColor = true, 
               bool clearDepth = true, 
               bool clearStencil = false) const;
    
    /**
     * @brief 将此帧缓冲内容复制到另一个帧缓冲
     * 
     * @param dest 目标帧缓冲（nullptr 表示屏幕）
     * @param mask 复制掩码（GL_COLOR_BUFFER_BIT 等）
     * @param filter 过滤模式（GL_NEAREST 或 GL_LINEAR）
     * 
     * @note 必须在 OpenGL 上下文线程中调用
     * @threadsafe 是
     */
    void BlitTo(Framebuffer* dest, 
                GLbitfield mask = GL_COLOR_BUFFER_BIT,
                GLenum filter = GL_NEAREST) const;
    
    /**
     * @brief 释放帧缓冲资源
     * 
     * @note 必须在 OpenGL 上下文线程中调用
     * @note 析构函数会自动调用
     * @threadsafe 是
     */
    void Release();
    
    /**
     * @brief 获取 OpenGL 帧缓冲 ID
     * 
     * @return FBO ID
     * @threadsafe 是
     */
    GLuint GetID() const;
    
    /**
     * @brief 获取宽度
     * 
     * @return 宽度（像素）
     * @threadsafe 是
     */
    int GetWidth() const;
    
    /**
     * @brief 获取高度
     * 
     * @return 高度（像素）
     * @threadsafe 是
     */
    int GetHeight() const;
    
    /**
     * @brief 获取多重采样数量
     * 
     * @return 采样数量（1表示无MSAA）
     * @threadsafe 是
     */
    int GetSamples() const;
    
    /**
     * @brief 绑定颜色附件纹理
     * 
     * @param index 附件索引（0-7）
     * @param unit 纹理单元
     * 
     * @note 如果使用渲染缓冲对象则无操作
     * @note 渲染到屏幕时，着色器需要翻转Y轴：TexCoord.y = 1.0 - aTexCoord.y
     * @threadsafe 是
     */
    void BindColorAttachment(int index = 0, unsigned int unit = 0) const;
    
    /**
     * @brief 获取颜色附件纹理ID
     * 
     * @param index 附件索引（0-7）
     * @return 纹理ID，不存在返回 0
     * 
     * @threadsafe 是
     */
    GLuint GetColorAttachmentID(int index = 0) const;
    
    /**
     * @brief 检查帧缓冲是否有效
     * 
     * @return 有效返回 true
     * @threadsafe 是
     */
    bool IsValid() const;
    
    /**
     * @brief 检查帧缓冲是否完整
     * 
     * @return 完整返回 true
     * 
     * @note 必须在 OpenGL 上下文线程中调用
     * @threadsafe 是
     */
    bool IsComplete() const;
    
    /**
     * @brief 获取帧缓冲状态描述
     * 
     * @return 状态描述字符串
     * 
     * @note 必须在 OpenGL 上下文线程中调用
     * @threadsafe 是
     */
    std::string GetStatusString() const;
    
    /**
     * @brief 获取调试名称
     * 
     * @return 名称
     * @threadsafe 是
     */
    const std::string& GetName() const;
    
    /**
     * @brief 设置调试名称
     * 
     * @param name 名称
     * @threadsafe 是
     */
    void SetName(const std::string& name);
    
    /**
     * @brief 检查是否使用多重采样
     * 
     * @return 使用MSAA返回 true
     * @threadsafe 是
     */
    bool IsMultisampled() const;
    
    /**
     * @brief 获取颜色附件数量
     * 
     * @return 颜色附件数量
     * @threadsafe 是
     */
    int GetColorAttachmentCount() const;

private:
    /**
     * @brief 创建纹理附件
     */
    bool CreateTextureAttachment(const FramebufferAttachment& attachment);
    
    /**
     * @brief 创建渲染缓冲附件
     */
    bool CreateRenderbufferAttachment(const FramebufferAttachment& attachment);
    
    /**
     * @brief 转换附件类型为 OpenGL 枚举
     */
    GLenum AttachmentTypeToGL(FramebufferAttachmentType type) const;
    
    /**
     * @brief 转换纹理格式为 OpenGL 内部格式
     */
    GLenum TextureFormatToInternalFormat(TextureFormat format) const;
    
    /**
     * @brief 检查并记录帧缓冲状态
     */
    bool CheckFramebufferStatus();

private:
    mutable std::mutex m_mutex;                      ///< 互斥锁
    GLuint m_fboID;                                   ///< 帧缓冲对象 ID
    int m_width;                                      ///< 宽度
    int m_height;                                     ///< 高度
    int m_samples;                                    ///< 多重采样数量
    std::string m_name;                               ///< 调试名称
    
    std::vector<GLuint> m_colorAttachmentTextures;   ///< 颜色附件纹理ID列表
    std::vector<GLuint> m_renderbuffers;              ///< 渲染缓冲对象列表
    FramebufferConfig m_config;                       ///< 配置（用于 Resize）
};

// 类型别名
using FramebufferPtr = std::shared_ptr<Framebuffer>;

} // namespace Render

