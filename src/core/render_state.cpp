#include "render/render_state.h"
#include "render/logger.h"
#include <glad/glad.h>

namespace Render {

RenderState::RenderState()
    : m_depthTest(true)
    , m_depthFunc(DepthFunc::Less)
    , m_depthWrite(true)
    , m_blendMode(BlendMode::None)
    , m_blendSrcFactor(GL_SRC_ALPHA)
    , m_blendDstFactor(GL_ONE_MINUS_SRC_ALPHA)
    , m_cullFace(CullFace::Back)
    , m_clearColor(0.1f, 0.1f, 0.1f, 1.0f)
    , m_depthTestDirty(true)
    , m_depthFuncDirty(true)
    , m_depthWriteDirty(true)
    , m_blendModeDirty(true)
    , m_cullFaceDirty(true)
    , m_activeTextureUnit(0)
    , m_boundVAO(0)
    , m_boundArrayBuffer(0)
    , m_boundElementArrayBuffer(0)
    , m_boundUniformBuffer(0)
    , m_boundShaderStorageBuffer(0)
    , m_currentProgram(0) {
    // 初始化纹理绑定数组
    m_boundTextures.fill(0);
    
    // 不在构造函数中调用 Reset()，因为此时 OpenGL 上下文还未创建
    // Reset() 将在 Renderer::Initialize() 中调用
}

void RenderState::SetDepthTest(bool enable) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    if (m_depthTest != enable) {
        m_depthTest = enable;
        m_depthTestDirty = true;
        ApplyDepthTest();
    }
}

void RenderState::SetDepthFunc(DepthFunc func) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    if (m_depthFunc != func) {
        m_depthFunc = func;
        m_depthFuncDirty = true;
        ApplyDepthFunc();
    }
}

void RenderState::SetDepthWrite(bool enable) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    if (m_depthWrite != enable) {
        m_depthWrite = enable;
        m_depthWriteDirty = true;
        ApplyDepthWrite();
    }
}

void RenderState::SetBlendMode(BlendMode mode) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    if (m_blendMode != mode) {
        m_blendMode = mode;
        m_blendModeDirty = true;
        ApplyBlendMode();
    }
}

void RenderState::SetBlendFunc(uint32_t srcFactor, uint32_t dstFactor) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_blendSrcFactor = srcFactor;
    m_blendDstFactor = dstFactor;
    m_blendMode = BlendMode::Custom;
    m_blendModeDirty = true;
    ApplyBlendMode();
}

void RenderState::SetCullFace(CullFace mode) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    if (m_cullFace != mode) {
        m_cullFace = mode;
        m_cullFaceDirty = true;
        ApplyCullFace();
    }
}

void RenderState::SetViewport(int x, int y, int width, int height) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    glViewport(x, y, width, height);
}

void RenderState::SetScissorTest(bool enable) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    if (enable) {
        glEnable(GL_SCISSOR_TEST);
    } else {
        glDisable(GL_SCISSOR_TEST);
    }
}

void RenderState::SetScissorRect(int x, int y, int width, int height) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    glScissor(x, y, width, height);
}

void RenderState::SetClearColor(const Color& color) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_clearColor = color;
    glClearColor(color.r, color.g, color.b, color.a);
}

void RenderState::Clear(bool colorBuffer, bool depthBuffer, bool stencilBuffer) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    GLbitfield mask = 0;
    if (colorBuffer) mask |= GL_COLOR_BUFFER_BIT;
    if (depthBuffer) mask |= GL_DEPTH_BUFFER_BIT;
    if (stencilBuffer) mask |= GL_STENCIL_BUFFER_BIT;
    
    if (mask != 0) {
        glClear(mask);
    }
}

BlendMode RenderState::GetBlendMode() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_blendMode;
}

CullFace RenderState::GetCullFace() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_cullFace;
}

uint32_t RenderState::GetBoundVertexArray() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_boundVAO;
}

uint32_t RenderState::GetCurrentProgram() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_currentProgram;
}

void RenderState::Reset() {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    
    // 重置所有状态为默认值
    m_depthTest = true;
    m_depthFunc = DepthFunc::Less;
    m_depthWrite = true;
    m_blendMode = BlendMode::None;
    m_cullFace = CullFace::Back;
    m_clearColor = Color(0.1f, 0.1f, 0.1f, 1.0f);
    
    // 应用到 OpenGL
    ApplyDepthTest();
    ApplyDepthFunc();
    ApplyDepthWrite();
    ApplyBlendMode();
    ApplyCullFace();
    
    glClearColor(m_clearColor.r, m_clearColor.g, m_clearColor.b, m_clearColor.a);
    glDisable(GL_SCISSOR_TEST);
    
    // 重置纹理、缓冲区、程序绑定
    m_boundTextures.fill(0);
    m_activeTextureUnit = 0;
    m_boundVAO = 0;
    m_boundArrayBuffer = 0;
    m_boundElementArrayBuffer = 0;
    m_boundUniformBuffer = 0;
    m_boundShaderStorageBuffer = 0;
    m_currentProgram = 0;
}

void RenderState::ApplyDepthTest() {
    if (m_depthTest) {
        glEnable(GL_DEPTH_TEST);
    } else {
        glDisable(GL_DEPTH_TEST);
    }
    m_depthTestDirty = false;
}

void RenderState::ApplyDepthFunc() {
    GLenum func = GL_LESS;
    switch (m_depthFunc) {
        case DepthFunc::Never:        func = GL_NEVER; break;
        case DepthFunc::Less:         func = GL_LESS; break;
        case DepthFunc::Equal:        func = GL_EQUAL; break;
        case DepthFunc::LessEqual:    func = GL_LEQUAL; break;
        case DepthFunc::Greater:      func = GL_GREATER; break;
        case DepthFunc::NotEqual:     func = GL_NOTEQUAL; break;
        case DepthFunc::GreaterEqual: func = GL_GEQUAL; break;
        case DepthFunc::Always:       func = GL_ALWAYS; break;
    }
    glDepthFunc(func);
    m_depthFuncDirty = false;
}

void RenderState::ApplyDepthWrite() {
    glDepthMask(m_depthWrite ? GL_TRUE : GL_FALSE);
    m_depthWriteDirty = false;
}

void RenderState::ApplyBlendMode() {
    switch (m_blendMode) {
        case BlendMode::None:
            glDisable(GL_BLEND);
            break;
            
        case BlendMode::Alpha:
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            break;
            
        case BlendMode::Additive:
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            break;
            
        case BlendMode::Multiply:
            glEnable(GL_BLEND);
            glBlendFunc(GL_DST_COLOR, GL_ZERO);
            break;
            
        case BlendMode::Custom:
            glEnable(GL_BLEND);
            glBlendFunc(m_blendSrcFactor, m_blendDstFactor);
            break;
    }
    m_blendModeDirty = false;
}

void RenderState::ApplyCullFace() {
    switch (m_cullFace) {
        case CullFace::None:
            glDisable(GL_CULL_FACE);
            break;
            
        case CullFace::Front:
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
            break;
            
        case CullFace::Back:
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            break;
            
        case CullFace::FrontAndBack:
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT_AND_BACK);
            break;
    }
    m_cullFaceDirty = false;
}

// ============================================================================
// 纹理绑定管理
// ============================================================================

void RenderState::BindTexture(uint32_t unit, uint32_t textureId, uint32_t target) {
    if (unit >= MAX_TEXTURE_UNITS) {
        Logger::GetInstance().Error("Texture unit " + std::to_string(unit) + 
                                    " exceeds maximum " + std::to_string(MAX_TEXTURE_UNITS));
        return;
    }
    
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    
    // 检查是否需要切换
    if (m_boundTextures[unit] != textureId) {
        // 激活纹理单元（如果需要）
        if (m_activeTextureUnit != unit) {
            glActiveTexture(GL_TEXTURE0 + unit);
            m_activeTextureUnit = unit;
        }
        
        // 绑定纹理
        glBindTexture(target, textureId);
        m_boundTextures[unit] = textureId;
    }
}

void RenderState::UnbindTexture(uint32_t unit, uint32_t target) {
    BindTexture(unit, 0, target);
}

void RenderState::SetActiveTextureUnit(uint32_t unit) {
    if (unit >= MAX_TEXTURE_UNITS) {
        Logger::GetInstance().Error("Texture unit " + std::to_string(unit) + 
                                    " exceeds maximum " + std::to_string(MAX_TEXTURE_UNITS));
        return;
    }
    
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    
    if (m_activeTextureUnit != unit) {
        glActiveTexture(GL_TEXTURE0 + unit);
        m_activeTextureUnit = unit;
    }
}

uint32_t RenderState::GetBoundTexture(uint32_t unit) const {
    if (unit >= MAX_TEXTURE_UNITS) {
        Logger::GetInstance().Error("Texture unit " + std::to_string(unit) + 
                                    " exceeds maximum " + std::to_string(MAX_TEXTURE_UNITS));
        return 0;
    }
    
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_boundTextures[unit];
}

// ============================================================================
// 缓冲区绑定管理
// ============================================================================

void RenderState::BindVertexArray(uint32_t vaoId) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    if (m_boundVAO != vaoId) {
        glBindVertexArray(vaoId);
        m_boundVAO = vaoId;
    }
}

void RenderState::BindBuffer(BufferTarget target, uint32_t bufferId) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    
    GLenum glTarget = GetGLBufferTarget(target);
    
    // 根据目标检查缓存
    uint32_t* cachedId = nullptr;
    switch (target) {
        case BufferTarget::ArrayBuffer:
            cachedId = &m_boundArrayBuffer;
            break;
        case BufferTarget::ElementArrayBuffer:
            cachedId = &m_boundElementArrayBuffer;
            break;
        case BufferTarget::UniformBuffer:
            cachedId = &m_boundUniformBuffer;
            break;
        case BufferTarget::ShaderStorageBuffer:
            cachedId = &m_boundShaderStorageBuffer;
            break;
    }
    
    if (cachedId && *cachedId != bufferId) {
        glBindBuffer(glTarget, bufferId);
        *cachedId = bufferId;
    }
}

uint32_t RenderState::GetBoundBuffer(BufferTarget target) const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    
    switch (target) {
        case BufferTarget::ArrayBuffer:
            return m_boundArrayBuffer;
        case BufferTarget::ElementArrayBuffer:
            return m_boundElementArrayBuffer;
        case BufferTarget::UniformBuffer:
            return m_boundUniformBuffer;
        case BufferTarget::ShaderStorageBuffer:
            return m_boundShaderStorageBuffer;
        default:
            return 0;
    }
}

uint32_t RenderState::GetGLBufferTarget(BufferTarget target) const {
    switch (target) {
        case BufferTarget::ArrayBuffer:
            return GL_ARRAY_BUFFER;
        case BufferTarget::ElementArrayBuffer:
            return GL_ELEMENT_ARRAY_BUFFER;
        case BufferTarget::UniformBuffer:
            return GL_UNIFORM_BUFFER;
        case BufferTarget::ShaderStorageBuffer:
            return GL_SHADER_STORAGE_BUFFER;
        default:
            Logger::GetInstance().Error("Unknown buffer target");
            return GL_ARRAY_BUFFER;
    }
}

// ============================================================================
// 着色器程序管理
// ============================================================================

void RenderState::UseProgram(uint32_t programId) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    if (m_currentProgram != programId) {
        glUseProgram(programId);
        m_currentProgram = programId;
    }
}

} // namespace Render

