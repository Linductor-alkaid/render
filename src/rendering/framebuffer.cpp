/**
 * @file framebuffer.cpp
 * @brief 帧缓冲对象管理实现
 */

#include "render/framebuffer.h"
#include "render/logger.h"
#include "render/gl_thread_checker.h"
#include "render/error.h"
#include <sstream>

namespace Render {

// ============================================================================
// FramebufferAttachment 静态方法
// ============================================================================

FramebufferAttachment FramebufferAttachment::Color(int index, TextureFormat format, bool useRBO) {
    FramebufferAttachment attachment;
    attachment.type = static_cast<FramebufferAttachmentType>(static_cast<int>(FramebufferAttachmentType::Color0) + index);
    attachment.format = format;
    attachment.minFilter = TextureFilter::Linear;
    attachment.magFilter = TextureFilter::Linear;
    attachment.wrapS = TextureWrap::ClampToEdge;
    attachment.wrapT = TextureWrap::ClampToEdge;
    attachment.useRenderbuffer = useRBO;
    return attachment;
}

FramebufferAttachment FramebufferAttachment::Depth(bool useRBO) {
    FramebufferAttachment attachment;
    attachment.type = FramebufferAttachmentType::Depth;
    attachment.format = TextureFormat::Depth;
    attachment.minFilter = TextureFilter::Nearest;
    attachment.magFilter = TextureFilter::Nearest;
    attachment.wrapS = TextureWrap::ClampToEdge;
    attachment.wrapT = TextureWrap::ClampToEdge;
    attachment.useRenderbuffer = useRBO;
    return attachment;
}

FramebufferAttachment FramebufferAttachment::DepthStencil(bool useRBO) {
    FramebufferAttachment attachment;
    attachment.type = FramebufferAttachmentType::DepthStencil;
    attachment.format = TextureFormat::DepthStencil;
    attachment.minFilter = TextureFilter::Nearest;
    attachment.magFilter = TextureFilter::Nearest;
    attachment.wrapS = TextureWrap::ClampToEdge;
    attachment.wrapT = TextureWrap::ClampToEdge;
    attachment.useRenderbuffer = useRBO;
    return attachment;
}

// ============================================================================
// FramebufferConfig 方法
// ============================================================================

FramebufferConfig& FramebufferConfig::AddColorAttachment(TextureFormat format, bool useRBO) {
    int index = 0;
    for (const auto& att : attachments) {
        if (static_cast<int>(att.type) >= static_cast<int>(FramebufferAttachmentType::Color0) &&
            static_cast<int>(att.type) <= static_cast<int>(FramebufferAttachmentType::Color7)) {
            index++;
        }
    }
    attachments.push_back(FramebufferAttachment::Color(index, format, useRBO));
    return *this;
}

FramebufferConfig& FramebufferConfig::AddDepthAttachment(bool useRBO) {
    attachments.push_back(FramebufferAttachment::Depth(useRBO));
    return *this;
}

FramebufferConfig& FramebufferConfig::AddDepthStencilAttachment(bool useRBO) {
    attachments.push_back(FramebufferAttachment::DepthStencil(useRBO));
    return *this;
}

FramebufferConfig& FramebufferConfig::SetSize(int w, int h) {
    width = w;
    height = h;
    return *this;
}

FramebufferConfig& FramebufferConfig::SetSamples(int s) {
    samples = s;
    return *this;
}

FramebufferConfig& FramebufferConfig::SetName(const std::string& n) {
    name = n;
    return *this;
}

// ============================================================================
// Framebuffer 类
// ============================================================================

Framebuffer::Framebuffer()
    : m_fboID(0)
    , m_width(0)
    , m_height(0)
    , m_samples(1)
    , m_name("Framebuffer")
{
}

Framebuffer::~Framebuffer() {
    Release();
}

Framebuffer::Framebuffer(Framebuffer&& other) noexcept
    : m_fboID(0)
    , m_width(0)
    , m_height(0)
    , m_samples(1)
{
    std::scoped_lock lock(m_mutex, other.m_mutex);
    
    m_fboID = other.m_fboID;
    m_width = other.m_width;
    m_height = other.m_height;
    m_samples = other.m_samples;
    m_name = std::move(other.m_name);
    m_colorAttachmentTextures = std::move(other.m_colorAttachmentTextures);
    m_renderbuffers = std::move(other.m_renderbuffers);
    m_config = std::move(other.m_config);
    
    other.m_fboID = 0;
    other.m_width = 0;
    other.m_height = 0;
    other.m_samples = 1;
}

Framebuffer& Framebuffer::operator=(Framebuffer&& other) noexcept {
    if (this != &other) {
        std::scoped_lock lock(m_mutex, other.m_mutex);
        
        Release();
        
        m_fboID = other.m_fboID;
        m_width = other.m_width;
        m_height = other.m_height;
        m_samples = other.m_samples;
        m_name = std::move(other.m_name);
        m_colorAttachmentTextures = std::move(other.m_colorAttachmentTextures);
        m_renderbuffers = std::move(other.m_renderbuffers);
        m_config = std::move(other.m_config);
        
        other.m_fboID = 0;
        other.m_width = 0;
        other.m_height = 0;
        other.m_samples = 1;
    }
    return *this;
}

bool Framebuffer::Create(const FramebufferConfig& config) {
    GL_THREAD_CHECK();
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 释放旧资源
    if (m_fboID != 0) {
        Release();
    }
    
    // 保存配置
    m_config = config;
    m_width = config.width;
    m_height = config.height;
    m_samples = config.samples;
    m_name = config.name;
    
    // 创建 FBO
    glGenFramebuffers(1, &m_fboID);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fboID);
    
    // 清空附件列表
    m_colorAttachmentTextures.clear();
    m_renderbuffers.clear();
    
    // 创建附件
    std::vector<GLenum> drawBuffers;
    for (const auto& attachment : config.attachments) {
        bool success = false;
        
        if (attachment.useRenderbuffer) {
            success = CreateRenderbufferAttachment(attachment);
        } else {
            success = CreateTextureAttachment(attachment);
        }
        
        if (!success) {
            Logger::GetInstance().Error("Failed to create attachment for framebuffer: " + m_name);
            Release();
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return false;
        }
        
        // 收集颜色附件用于 MRT
        if (static_cast<int>(attachment.type) >= static_cast<int>(FramebufferAttachmentType::Color0) &&
            static_cast<int>(attachment.type) <= static_cast<int>(FramebufferAttachmentType::Color7)) {
            drawBuffers.push_back(AttachmentTypeToGL(attachment.type));
        }
    }
    
    // 设置 MRT 绘制缓冲
    if (!drawBuffers.empty()) {
        glDrawBuffers(static_cast<GLsizei>(drawBuffers.size()), drawBuffers.data());
    } else {
        glDrawBuffer(GL_NONE);
    }
    
    // 检查完整性
    bool complete = CheckFramebufferStatus();
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    if (complete) {
        Logger::GetInstance().Info("Created framebuffer '" + m_name + "' (" + 
                                  std::to_string(m_width) + "x" + std::to_string(m_height) + 
                                  ", " + std::to_string(m_samples) + "x MSAA)");
    }
    
    return complete;
}

bool Framebuffer::CreateTextureAttachment(const FramebufferAttachment& attachment) {
    GL_THREAD_CHECK();
    
    // 根据多重采样选择纹理目标
    GLenum target = (m_samples > 1) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(target, texID);
    
    // 获取内部格式
    GLenum internalFormat = TextureFormatToInternalFormat(attachment.format);
    GLenum format = GL_RGBA;
    GLenum type = GL_UNSIGNED_BYTE;
    
    // 根据纹理格式设置 format 和 type
    switch (attachment.format) {
        case TextureFormat::RGB:
            format = GL_RGB;
            break;
        case TextureFormat::RGBA:
            format = GL_RGBA;
            break;
        case TextureFormat::RED:
            format = GL_RED;
            break;
        case TextureFormat::RG:
            format = GL_RG;
            break;
        case TextureFormat::Depth:
            format = GL_DEPTH_COMPONENT;
            type = GL_FLOAT;
            break;
        case TextureFormat::DepthStencil:
            format = GL_DEPTH_STENCIL;
            type = GL_UNSIGNED_INT_24_8;
            break;
    }
    
    // 创建纹理
    if (m_samples > 1) {
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, m_samples, internalFormat,
                               m_width, m_height, GL_TRUE);
    } else {
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, m_width, m_height, 0,
                    format, type, nullptr);
        
        // 设置过滤和环绕模式
        GLint minFilter = (attachment.minFilter == TextureFilter::Linear) ? GL_LINEAR : GL_NEAREST;
        GLint magFilter = (attachment.magFilter == TextureFilter::Linear) ? GL_LINEAR : GL_NEAREST;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
        
        GLint wrapS = (attachment.wrapS == TextureWrap::Repeat) ? GL_REPEAT : 
                     (attachment.wrapS == TextureWrap::ClampToEdge) ? GL_CLAMP_TO_EDGE :
                     (attachment.wrapS == TextureWrap::MirroredRepeat) ? GL_MIRRORED_REPEAT : GL_CLAMP_TO_BORDER;
        GLint wrapT = (attachment.wrapT == TextureWrap::Repeat) ? GL_REPEAT :
                     (attachment.wrapT == TextureWrap::ClampToEdge) ? GL_CLAMP_TO_EDGE :
                     (attachment.wrapT == TextureWrap::MirroredRepeat) ? GL_MIRRORED_REPEAT : GL_CLAMP_TO_BORDER;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);
    }
    
    // 附加到 FBO
    GLenum glAttachment = AttachmentTypeToGL(attachment.type);
    glFramebufferTexture2D(GL_FRAMEBUFFER, glAttachment, target, texID, 0);
    
    // 保存纹理ID
    if (static_cast<int>(attachment.type) >= static_cast<int>(FramebufferAttachmentType::Color0) &&
        static_cast<int>(attachment.type) <= static_cast<int>(FramebufferAttachmentType::Color7)) {
        int index = static_cast<int>(attachment.type) - static_cast<int>(FramebufferAttachmentType::Color0);
        if (index >= static_cast<int>(m_colorAttachmentTextures.size())) {
            m_colorAttachmentTextures.resize(index + 1, 0);
        }
        m_colorAttachmentTextures[index] = texID;
    }
    
    return true;
}

bool Framebuffer::CreateRenderbufferAttachment(const FramebufferAttachment& attachment) {
    GL_THREAD_CHECK();
    
    GLuint rbo;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    
    GLenum internalFormat = TextureFormatToInternalFormat(attachment.format);
    
    if (m_samples > 1) {
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_samples, internalFormat,
                                        m_width, m_height);
    } else {
        glRenderbufferStorage(GL_RENDERBUFFER, internalFormat, m_width, m_height);
    }
    
    GLenum glAttachment = AttachmentTypeToGL(attachment.type);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, glAttachment, GL_RENDERBUFFER, rbo);
    
    m_renderbuffers.push_back(rbo);
    
    return true;
}

bool Framebuffer::Resize(int width, int height) {
    GL_THREAD_CHECK();
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_config.attachments.empty()) {
        Logger::GetInstance().Error("Cannot resize framebuffer: no configuration available");
        return false;
    }
    
    // 更新配置尺寸
    m_config.width = width;
    m_config.height = height;
    
    // 重新创建
    return Create(m_config);
}

void Framebuffer::Bind() const {
    GL_THREAD_CHECK();
    std::lock_guard<std::mutex> lock(m_mutex);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fboID);
}

void Framebuffer::Unbind() const {
    GL_THREAD_CHECK();
    // 注意：Unbind 不需要加锁，因为它只是重置 OpenGL 状态
    // 不访问任何成员变量
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::BindRead() const {
    GL_THREAD_CHECK();
    std::lock_guard<std::mutex> lock(m_mutex);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fboID);
}

void Framebuffer::BindDraw() const {
    GL_THREAD_CHECK();
    std::lock_guard<std::mutex> lock(m_mutex);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fboID);
}

void Framebuffer::Clear(bool clearColor, bool clearDepth, bool clearStencil) const {
    GL_THREAD_CHECK();
    
    GLbitfield mask = 0;
    if (clearColor) mask |= GL_COLOR_BUFFER_BIT;
    if (clearDepth) mask |= GL_DEPTH_BUFFER_BIT;
    if (clearStencil) mask |= GL_STENCIL_BUFFER_BIT;
    
    if (mask != 0) {
        glClear(mask);
    }
}

void Framebuffer::BlitTo(Framebuffer* dest, GLbitfield mask, GLenum filter) const {
    GL_THREAD_CHECK();
    
    // 先获取所有需要的信息，避免持有锁时调用其他对象的方法（防止死锁）
    GLuint srcFBO;
    int srcWidth, srcHeight;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        srcFBO = m_fboID;
        srcWidth = m_width;
        srcHeight = m_height;
    }
    
    GLuint destFBO = 0;
    int destWidth = srcWidth;
    int destHeight = srcHeight;
    
    // 在锁外调用 dest 的方法，避免死锁
    if (dest != nullptr) {
        destFBO = dest->GetID();
        destWidth = dest->GetWidth();
        destHeight = dest->GetHeight();
    }
    
    // 执行 Blit 操作
    glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, destFBO);
    
    glBlitFramebuffer(0, 0, srcWidth, srcHeight,
                     0, 0, destWidth, destHeight,
                     mask, filter);
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::Release() {
    // 注意：不在这里调用 GL_THREAD_CHECK()，因为析构函数可能在任意线程调用
    // 如果不在正确的线程，OpenGL 资源会泄漏，但不会崩溃
    
    if (m_fboID != 0) {
        glDeleteFramebuffers(1, &m_fboID);
        m_fboID = 0;
    }
    
    // 删除纹理
    for (GLuint texID : m_colorAttachmentTextures) {
        if (texID != 0) {
            glDeleteTextures(1, &texID);
        }
    }
    m_colorAttachmentTextures.clear();
    
    // 删除渲染缓冲
    for (GLuint rbo : m_renderbuffers) {
        glDeleteRenderbuffers(1, &rbo);
    }
    m_renderbuffers.clear();
    
    m_width = 0;
    m_height = 0;
}

GLuint Framebuffer::GetID() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_fboID;
}

int Framebuffer::GetWidth() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_width;
}

int Framebuffer::GetHeight() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_height;
}

int Framebuffer::GetSamples() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_samples;
}

void Framebuffer::BindColorAttachment(int index, unsigned int unit) const {
    GL_THREAD_CHECK();
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (index >= 0 && index < static_cast<int>(m_colorAttachmentTextures.size())) {
        GLuint texID = m_colorAttachmentTextures[index];
        if (texID != 0) {
            glActiveTexture(GL_TEXTURE0 + unit);
            GLenum target = (m_samples > 1) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
            glBindTexture(target, texID);
        }
    }
}

GLuint Framebuffer::GetColorAttachmentID(int index) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (index >= 0 && index < static_cast<int>(m_colorAttachmentTextures.size())) {
        return m_colorAttachmentTextures[index];
    }
    return 0;
}

bool Framebuffer::IsValid() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_fboID != 0;
}

bool Framebuffer::IsComplete() const {
    GL_THREAD_CHECK();
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_fboID == 0) return false;
    
    glBindFramebuffer(GL_FRAMEBUFFER, m_fboID);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    return status == GL_FRAMEBUFFER_COMPLETE;
}

std::string Framebuffer::GetStatusString() const {
    GL_THREAD_CHECK();
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_fboID == 0) {
        return "Invalid (FBO ID = 0)";
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, m_fboID);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    switch (status) {
        case GL_FRAMEBUFFER_COMPLETE:
            return "Complete";
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            return "Incomplete Attachment";
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            return "Missing Attachment";
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
            return "Incomplete Draw Buffer";
        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
            return "Incomplete Read Buffer";
        case GL_FRAMEBUFFER_UNSUPPORTED:
            return "Unsupported";
        case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
            return "Incomplete Multisample";
        default:
            return "Unknown Status (" + std::to_string(status) + ")";
    }
}

const std::string& Framebuffer::GetName() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_name;
}

void Framebuffer::SetName(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_name = name;
}

bool Framebuffer::IsMultisampled() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_samples > 1;
}

int Framebuffer::GetColorAttachmentCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<int>(m_colorAttachmentTextures.size());
}

GLenum Framebuffer::AttachmentTypeToGL(FramebufferAttachmentType type) const {
    switch (type) {
        case FramebufferAttachmentType::Color0: return GL_COLOR_ATTACHMENT0;
        case FramebufferAttachmentType::Color1: return GL_COLOR_ATTACHMENT1;
        case FramebufferAttachmentType::Color2: return GL_COLOR_ATTACHMENT2;
        case FramebufferAttachmentType::Color3: return GL_COLOR_ATTACHMENT3;
        case FramebufferAttachmentType::Color4: return GL_COLOR_ATTACHMENT4;
        case FramebufferAttachmentType::Color5: return GL_COLOR_ATTACHMENT5;
        case FramebufferAttachmentType::Color6: return GL_COLOR_ATTACHMENT6;
        case FramebufferAttachmentType::Color7: return GL_COLOR_ATTACHMENT7;
        case FramebufferAttachmentType::Depth: return GL_DEPTH_ATTACHMENT;
        case FramebufferAttachmentType::Stencil: return GL_STENCIL_ATTACHMENT;
        case FramebufferAttachmentType::DepthStencil: return GL_DEPTH_STENCIL_ATTACHMENT;
        default: return GL_COLOR_ATTACHMENT0;
    }
}

GLenum Framebuffer::TextureFormatToInternalFormat(TextureFormat format) const {
    switch (format) {
        case TextureFormat::RGB: return GL_RGB8;
        case TextureFormat::RGBA: return GL_RGBA8;
        case TextureFormat::RED: return GL_R8;
        case TextureFormat::RG: return GL_RG8;
        case TextureFormat::Depth: return GL_DEPTH_COMPONENT24;
        case TextureFormat::DepthStencil: return GL_DEPTH24_STENCIL8;
        default: return GL_RGBA8;
    }
}

bool Framebuffer::CheckFramebufferStatus() {
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::string statusStr;
        switch (status) {
            case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
                statusStr = "Incomplete Attachment";
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
                statusStr = "Missing Attachment";
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
                statusStr = "Incomplete Draw Buffer";
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
                statusStr = "Incomplete Read Buffer";
                break;
            case GL_FRAMEBUFFER_UNSUPPORTED:
                statusStr = "Unsupported";
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
                statusStr = "Incomplete Multisample";
                break;
            default:
                statusStr = "Unknown (" + std::to_string(status) + ")";
                break;
        }
        
        Logger::GetInstance().Error("Framebuffer '" + m_name + "' is not complete: " + statusStr);
        return false;
    }
    
    return true;
}

} // namespace Render

