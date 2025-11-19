#include "render/render_state.h"
#include "render/logger.h"
#include "render/error.h"
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
    , m_currentProgram(0)
    , m_viewMatrix(Matrix4::Identity())
    , m_projectionMatrix(Matrix4::Identity())
    , m_viewMatrixSet(false)
    , m_projectionMatrixSet(false)
    , m_strictMode(false) {  // 默认使用缓存优化
    // 初始化纹理绑定数组
    m_boundTextures.fill(0);
    
    // 不在构造函数中调用 Reset()，因为此时 OpenGL 上下文还未创建
    // Reset() 将在 Renderer::Initialize() 中调用
}

RenderState::ScopedStateGuard::ScopedStateGuard(RenderState* state)
    : m_state(state)
    , m_blendMode(BlendMode::None)
    , m_cullFace(CullFace::Back)
    , m_depthTest(true)
    , m_depthWrite(true)
    , m_active(false) {
    if (!m_state) {
        return;
    }

    m_blendMode = m_state->GetBlendMode();
    m_cullFace = m_state->GetCullFace();
    m_depthTest = m_state->GetDepthTest();
    m_depthWrite = m_state->GetDepthWrite();
    m_active = true;
}

RenderState::ScopedStateGuard::~ScopedStateGuard() {
    if (m_active && m_state) {
        m_state->SetBlendMode(m_blendMode);
        m_state->SetCullFace(m_cullFace);
        m_state->SetDepthTest(m_depthTest);
        m_state->SetDepthWrite(m_depthWrite);
    }
}

void RenderState::ScopedStateGuard::Release() {
    m_active = false;
    m_state = nullptr;
}

void RenderState::SetDepthTest(bool enable) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    if (m_depthTest != enable || m_strictMode) {
        m_depthTest = enable;
        m_depthTestDirty = true;
        ApplyDepthTest();
    }
}

void RenderState::SetDepthFunc(DepthFunc func) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    if (m_depthFunc != func || m_strictMode) {
        m_depthFunc = func;
        m_depthFuncDirty = true;
        ApplyDepthFunc();
    }
}

void RenderState::SetDepthWrite(bool enable) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    if (m_depthWrite != enable || m_strictMode) {
        m_depthWrite = enable;
        m_depthWriteDirty = true;
        ApplyDepthWrite();
    }
}

void RenderState::SetBlendMode(BlendMode mode) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    if (m_blendMode != mode || m_strictMode) {
        m_blendMode = mode;
        m_blendModeDirty = true;
        ApplyBlendMode();
    }
}

void RenderState::SetBlendFunc(uint32_t srcFactor, uint32_t dstFactor) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    if (m_blendSrcFactor != srcFactor || m_blendDstFactor != dstFactor || m_strictMode) {
        m_blendSrcFactor = srcFactor;
        m_blendDstFactor = dstFactor;
        m_blendMode = BlendMode::Custom;
        m_blendModeDirty = true;
        ApplyBlendMode();
    }
}

void RenderState::SetCullFace(CullFace mode) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    if (m_cullFace != mode || m_strictMode) {
        m_cullFace = mode;
        m_cullFaceDirty = true;
        ApplyCullFace();
    }
}

void RenderState::SetViewport(int x, int y, int width, int height) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    GL_THREAD_CHECK();
    glViewport(x, y, width, height);
}

void RenderState::SetScissorTest(bool enable) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    GL_THREAD_CHECK();
    if (enable) {
        glEnable(GL_SCISSOR_TEST);
    } else {
        glDisable(GL_SCISSOR_TEST);
    }
}

void RenderState::SetScissorRect(int x, int y, int width, int height) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    GL_THREAD_CHECK();
    glScissor(x, y, width, height);
}

void RenderState::SetClearColor(const Color& color) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_clearColor = color;
    GL_THREAD_CHECK();
    glClearColor(color.r, color.g, color.b, color.a);
}

void RenderState::Clear(bool colorBuffer, bool depthBuffer, bool stencilBuffer) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    GLbitfield mask = 0;
    if (colorBuffer) mask |= GL_COLOR_BUFFER_BIT;
    if (depthBuffer) mask |= GL_DEPTH_BUFFER_BIT;
    if (stencilBuffer) mask |= GL_STENCIL_BUFFER_BIT;
    
    if (mask != 0) {
        GL_THREAD_CHECK();
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

bool RenderState::GetDepthTest() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_depthTest;
}

bool RenderState::GetDepthWrite() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_depthWrite;
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
    
    GL_THREAD_CHECK();
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
    
    // 重置相机矩阵（不清除矩阵值，只清除标志，以便后续可以检查是否已设置）
    m_viewMatrixSet = false;
    m_projectionMatrixSet = false;
}

void RenderState::ApplyDepthTest() {
    GL_THREAD_CHECK();
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
    GL_THREAD_CHECK();
    glDepthFunc(func);
    m_depthFuncDirty = false;
}

void RenderState::ApplyDepthWrite() {
    GL_THREAD_CHECK();
    glDepthMask(m_depthWrite ? GL_TRUE : GL_FALSE);
    m_depthWriteDirty = false;
}

void RenderState::ApplyBlendMode() {
    GL_THREAD_CHECK();
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
    GL_THREAD_CHECK();
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
    
    // 严格模式或缓存不匹配时才执行
    if (m_boundTextures[unit] != textureId || m_strictMode) {
        GL_THREAD_CHECK();
        // 激活纹理单元（如果需要）
        if (m_activeTextureUnit != unit || m_strictMode) {
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
    
    if (m_activeTextureUnit != unit || m_strictMode) {
        GL_THREAD_CHECK();
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
    if (m_boundVAO != vaoId || m_strictMode) {
        GL_THREAD_CHECK();
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
    
    if (cachedId && (*cachedId != bufferId || m_strictMode)) {
        GL_THREAD_CHECK();
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
    if (m_currentProgram != programId || m_strictMode) {
        GL_THREAD_CHECK();
        glUseProgram(programId);
        m_currentProgram = programId;
    }
}

// ============================================================================
// 缓存同步管理
// ============================================================================

void RenderState::InvalidateCache() {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    
    // 标记所有渲染状态为脏
    m_depthTestDirty = true;
    m_depthFuncDirty = true;
    m_depthWriteDirty = true;
    m_blendModeDirty = true;
    m_cullFaceDirty = true;
    
    // 清空所有绑定缓存
    m_boundTextures.fill(0);
    m_activeTextureUnit = 0;
    m_boundVAO = 0;
    m_boundArrayBuffer = 0;
    m_boundElementArrayBuffer = 0;
    m_boundUniformBuffer = 0;
    m_boundShaderStorageBuffer = 0;
    m_currentProgram = 0;
    
    Logger::GetInstance().Debug("RenderState: 已清空所有状态缓存");
}

void RenderState::InvalidateTextureCache() {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_boundTextures.fill(0);
    m_activeTextureUnit = 0;
    Logger::GetInstance().Debug("RenderState: 已清空纹理绑定缓存");
}

void RenderState::InvalidateBufferCache() {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_boundVAO = 0;
    m_boundArrayBuffer = 0;
    m_boundElementArrayBuffer = 0;
    m_boundUniformBuffer = 0;
    m_boundShaderStorageBuffer = 0;
    Logger::GetInstance().Debug("RenderState: 已清空缓冲区绑定缓存");
}

void RenderState::InvalidateShaderCache() {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_currentProgram = 0;
    Logger::GetInstance().Debug("RenderState: 已清空着色器程序缓存");
}

void RenderState::InvalidateRenderStateCache() {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_depthTestDirty = true;
    m_depthFuncDirty = true;
    m_depthWriteDirty = true;
    m_blendModeDirty = true;
    m_cullFaceDirty = true;
    Logger::GetInstance().Debug("RenderState: 已清空渲染状态缓存");
}

void RenderState::SyncFromGL() {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    GL_THREAD_CHECK();
    
    // 查询深度测试状态
    GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    m_depthTest = (depthTestEnabled == GL_TRUE);
    
    // 查询深度函数
    GLint depthFunc;
    glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
    switch (depthFunc) {
        case GL_NEVER:   m_depthFunc = DepthFunc::Never; break;
        case GL_LESS:    m_depthFunc = DepthFunc::Less; break;
        case GL_EQUAL:   m_depthFunc = DepthFunc::Equal; break;
        case GL_LEQUAL:  m_depthFunc = DepthFunc::LessEqual; break;
        case GL_GREATER: m_depthFunc = DepthFunc::Greater; break;
        case GL_NOTEQUAL: m_depthFunc = DepthFunc::NotEqual; break;
        case GL_GEQUAL:  m_depthFunc = DepthFunc::GreaterEqual; break;
        case GL_ALWAYS:  m_depthFunc = DepthFunc::Always; break;
    }
    
    // 查询深度写入
    GLboolean depthWriteMask;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWriteMask);
    m_depthWrite = (depthWriteMask == GL_TRUE);
    
    // 查询混合状态
    GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    if (!blendEnabled) {
        m_blendMode = BlendMode::None;
    } else {
        GLint srcFactor, dstFactor;
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &srcFactor);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &dstFactor);
        
        // 尝试识别常见混合模式
        if (srcFactor == GL_SRC_ALPHA && dstFactor == GL_ONE_MINUS_SRC_ALPHA) {
            m_blendMode = BlendMode::Alpha;
        } else if (srcFactor == GL_SRC_ALPHA && dstFactor == GL_ONE) {
            m_blendMode = BlendMode::Additive;
        } else if (srcFactor == GL_DST_COLOR && dstFactor == GL_ZERO) {
            m_blendMode = BlendMode::Multiply;
        } else {
            m_blendMode = BlendMode::Custom;
            m_blendSrcFactor = srcFactor;
            m_blendDstFactor = dstFactor;
        }
    }
    
    // 查询面剔除状态
    GLboolean cullFaceEnabled = glIsEnabled(GL_CULL_FACE);
    if (!cullFaceEnabled) {
        m_cullFace = CullFace::None;
    } else {
        GLint cullFaceMode;
        glGetIntegerv(GL_CULL_FACE_MODE, &cullFaceMode);
        switch (cullFaceMode) {
            case GL_FRONT: m_cullFace = CullFace::Front; break;
            case GL_BACK:  m_cullFace = CullFace::Back; break;
            case GL_FRONT_AND_BACK: m_cullFace = CullFace::FrontAndBack; break;
            default: m_cullFace = CullFace::Back; break;
        }
    }
    
    // 查询清屏颜色
    GLfloat clearColor[4];
    glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);
    m_clearColor = Color(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
    
    // 查询当前绑定的纹理
    GLint activeTexture;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
    m_activeTextureUnit = activeTexture - GL_TEXTURE0;
    
    // 查询每个纹理单元的绑定（只查询常用的前8个单元以避免性能开销）
    for (uint32_t i = 0; i < std::min(8u, MAX_TEXTURE_UNITS); ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        GLint boundTexture;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &boundTexture);
        m_boundTextures[i] = boundTexture;
    }
    
    // 恢复活动纹理单元
    glActiveTexture(GL_TEXTURE0 + m_activeTextureUnit);
    
    // 查询缓冲区绑定
    GLint boundVAO;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &boundVAO);
    m_boundVAO = boundVAO;
    
    GLint boundArrayBuffer;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &boundArrayBuffer);
    m_boundArrayBuffer = boundArrayBuffer;
    
    GLint boundElementArrayBuffer;
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &boundElementArrayBuffer);
    m_boundElementArrayBuffer = boundElementArrayBuffer;
    
    GLint boundUniformBuffer;
    glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &boundUniformBuffer);
    m_boundUniformBuffer = boundUniformBuffer;
    
    GLint boundShaderStorageBuffer;
    glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING, &boundShaderStorageBuffer);
    m_boundShaderStorageBuffer = boundShaderStorageBuffer;
    
    // 查询当前着色器程序
    GLint currentProgram;
    glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
    m_currentProgram = currentProgram;
    
    // 清除所有脏标志
    m_depthTestDirty = false;
    m_depthFuncDirty = false;
    m_depthWriteDirty = false;
    m_blendModeDirty = false;
    m_cullFaceDirty = false;
    
    Logger::GetInstance().Info("RenderState: 已从 OpenGL 同步所有状态");
}

void RenderState::SetStrictMode(bool enable) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_strictMode = enable;
    
    if (enable) {
        Logger::GetInstance().Info("RenderState: 已启用严格模式（不使用缓存优化）");
    } else {
        Logger::GetInstance().Info("RenderState: 已禁用严格模式（使用缓存优化）");
    }
}

bool RenderState::IsStrictMode() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_strictMode;
}

// ============================================================================
// 相机矩阵管理
// ============================================================================

void RenderState::SetViewMatrix(const Matrix4& viewMatrix) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_viewMatrix = viewMatrix;
    m_viewMatrixSet = true;
}

void RenderState::SetProjectionMatrix(const Matrix4& projectionMatrix) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_projectionMatrix = projectionMatrix;
    m_projectionMatrixSet = true;
}

Matrix4 RenderState::GetViewMatrix() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_viewMatrix;
}

Matrix4 RenderState::GetProjectionMatrix() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_projectionMatrix;
}

bool RenderState::IsViewMatrixSet() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_viewMatrixSet;
}

bool RenderState::IsProjectionMatrixSet() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_projectionMatrixSet;
}

} // namespace Render

