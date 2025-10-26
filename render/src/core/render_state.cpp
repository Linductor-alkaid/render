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
    , m_cullFaceDirty(true) {
    // 不在构造函数中调用 Reset()，因为此时 OpenGL 上下文还未创建
    // Reset() 将在 Renderer::Initialize() 中调用
}

void RenderState::SetDepthTest(bool enable) {
    if (m_depthTest != enable) {
        m_depthTest = enable;
        m_depthTestDirty = true;
        ApplyDepthTest();
    }
}

void RenderState::SetDepthFunc(DepthFunc func) {
    if (m_depthFunc != func) {
        m_depthFunc = func;
        m_depthFuncDirty = true;
        ApplyDepthFunc();
    }
}

void RenderState::SetDepthWrite(bool enable) {
    if (m_depthWrite != enable) {
        m_depthWrite = enable;
        m_depthWriteDirty = true;
        ApplyDepthWrite();
    }
}

void RenderState::SetBlendMode(BlendMode mode) {
    if (m_blendMode != mode) {
        m_blendMode = mode;
        m_blendModeDirty = true;
        ApplyBlendMode();
    }
}

void RenderState::SetBlendFunc(uint32_t srcFactor, uint32_t dstFactor) {
    m_blendSrcFactor = srcFactor;
    m_blendDstFactor = dstFactor;
    m_blendMode = BlendMode::Custom;
    m_blendModeDirty = true;
    ApplyBlendMode();
}

void RenderState::SetCullFace(CullFace mode) {
    if (m_cullFace != mode) {
        m_cullFace = mode;
        m_cullFaceDirty = true;
        ApplyCullFace();
    }
}

void RenderState::SetViewport(int x, int y, int width, int height) {
    glViewport(x, y, width, height);
}

void RenderState::SetScissorTest(bool enable) {
    if (enable) {
        glEnable(GL_SCISSOR_TEST);
    } else {
        glDisable(GL_SCISSOR_TEST);
    }
}

void RenderState::SetScissorRect(int x, int y, int width, int height) {
    glScissor(x, y, width, height);
}

void RenderState::SetClearColor(const Color& color) {
    m_clearColor = color;
    glClearColor(color.r, color.g, color.b, color.a);
}

void RenderState::Clear(bool colorBuffer, bool depthBuffer, bool stencilBuffer) {
    GLbitfield mask = 0;
    if (colorBuffer) mask |= GL_COLOR_BUFFER_BIT;
    if (depthBuffer) mask |= GL_DEPTH_BUFFER_BIT;
    if (stencilBuffer) mask |= GL_STENCIL_BUFFER_BIT;
    
    if (mask != 0) {
        glClear(mask);
    }
}

void RenderState::Reset() {
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

} // namespace Render

