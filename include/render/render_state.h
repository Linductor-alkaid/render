#pragma once

#include "render/types.h"
#include "render/gl_thread_checker.h"
#include <array>
#include <shared_mutex>

namespace Render {

/**
 * @brief 混合模式
 */
enum class BlendMode {
    None,
    Alpha,
    Additive,
    Multiply,
    Custom
};

/**
 * @brief 深度测试函数
 */
enum class DepthFunc {
    Never,
    Less,
    Equal,
    LessEqual,
    Greater,
    NotEqual,
    GreaterEqual,
    Always
};

/**
 * @brief 面剔除模式
 */
enum class CullFace {
    None,
    Front,
    Back,
    FrontAndBack
};

/**
 * @brief 缓冲区绑定目标
 */
enum class BufferTarget {
    ArrayBuffer,        // VBO
    ElementArrayBuffer, // EBO/IBO
    UniformBuffer,      // UBO
    ShaderStorageBuffer // SSBO
};

/**
 * @brief 渲染状态管理类
 * 
 * 缓存和管理 OpenGL 渲染状态，减少不必要的状态切换
 * 
 * 线程安全：
 * - 所有公共方法都是线程安全的
 * - 使用读写锁支持并发读取和独占写入
 * - 注意：OpenGL 调用本身需要在创建上下文的线程中执行
 */
class RenderState {
public:
    RenderState();
    
    /**
     * @brief 启用/禁用深度测试
     */
    void SetDepthTest(bool enable);
    
    /**
     * @brief 设置深度比较函数
     */
    void SetDepthFunc(DepthFunc func);
    
    /**
     * @brief 启用/禁用深度写入
     */
    void SetDepthWrite(bool enable);
    
    /**
     * @brief 设置混合模式
     */
    void SetBlendMode(BlendMode mode);
    
    /**
     * @brief 设置自定义混合函数
     */
    void SetBlendFunc(uint32_t srcFactor, uint32_t dstFactor);
    
    /**
     * @brief 设置面剔除模式
     */
    void SetCullFace(CullFace mode);
    
    /**
     * @brief 设置视口
     */
    void SetViewport(int x, int y, int width, int height);
    
    /**
     * @brief 启用/禁用裁剪测试
     */
    void SetScissorTest(bool enable);
    
    /**
     * @brief 设置裁剪区域
     */
    void SetScissorRect(int x, int y, int width, int height);
    
    /**
     * @brief 设置清屏颜色
     */
    void SetClearColor(const Color& color);
    
    /**
     * @brief 清空缓冲区
     */
    void Clear(bool colorBuffer = true, bool depthBuffer = true, bool stencilBuffer = false);
    
    /**
     * @brief 重置所有状态为默认值
     */
    void Reset();
    
    /**
     * @brief 获取当前混合模式
     */
    BlendMode GetBlendMode() const;
    
    /**
     * @brief 获取当前面剔除模式
     */
    CullFace GetCullFace() const;
    
    // ========================================================================
    // 缓存同步管理
    // ========================================================================
    
    /**
     * @brief 使所有状态缓存失效
     * 
     * 当外部代码直接调用 OpenGL API 改变状态时，调用此方法清空缓存
     * 下次状态设置时会强制应用到 OpenGL
     * 
     * 使用场景：
     * - 在使用第三方 OpenGL 库后
     * - 在直接调用 OpenGL API 后
     * - 在上下文切换后
     */
    void InvalidateCache();
    
    /**
     * @brief 使特定类型的缓存失效
     */
    void InvalidateTextureCache();
    void InvalidateBufferCache();
    void InvalidateShaderCache();
    void InvalidateRenderStateCache();
    
    /**
     * @brief 从 OpenGL 查询并同步状态到缓存
     * 
     * 此方法会查询 OpenGL 当前的实际状态，并更新内部缓存
     * 用于在不确定缓存是否有效时重新同步
     * 
     * 注意：此操作相对耗时，不建议频繁调用
     */
    void SyncFromGL();
    
    /**
     * @brief 启用/禁用严格模式
     * 
     * 严格模式下，所有状态设置都会直接调用 OpenGL API，不使用缓存优化
     * 这会牺牲性能，但保证状态总是正确的
     * 
     * @param enable true 启用严格模式，false 使用缓存优化（默认）
     */
    void SetStrictMode(bool enable);
    
    /**
     * @brief 检查是否处于严格模式
     */
    bool IsStrictMode() const;
    
    // ========================================================================
    // 纹理绑定管理
    // ========================================================================
    
    /**
     * @brief 绑定纹理到指定纹理单元
     * @param unit 纹理单元索引 (0-31)
     * @param textureId OpenGL 纹理 ID
     * @param target 纹理目标 (GL_TEXTURE_2D 等)
     */
    void BindTexture(uint32_t unit, uint32_t textureId, uint32_t target = 0x0DE1 /* GL_TEXTURE_2D */);
    
    /**
     * @brief 解绑指定纹理单元
     * @param unit 纹理单元索引
     */
    void UnbindTexture(uint32_t unit, uint32_t target = 0x0DE1 /* GL_TEXTURE_2D */);
    
    /**
     * @brief 设置当前活动纹理单元
     * @param unit 纹理单元索引
     */
    void SetActiveTextureUnit(uint32_t unit);
    
    /**
     * @brief 获取当前绑定的纹理
     * @param unit 纹理单元索引
     * @return 纹理 ID
     */
    uint32_t GetBoundTexture(uint32_t unit) const;
    
    // ========================================================================
    // 缓冲区绑定管理
    // ========================================================================
    
    /**
     * @brief 绑定 VAO (顶点数组对象)
     * @param vaoId VAO ID
     */
    void BindVertexArray(uint32_t vaoId);
    
    /**
     * @brief 绑定缓冲区
     * @param target 缓冲区目标
     * @param bufferId 缓冲区 ID
     */
    void BindBuffer(BufferTarget target, uint32_t bufferId);
    
    /**
     * @brief 获取当前绑定的 VAO
     */
    uint32_t GetBoundVertexArray() const;
    
    /**
     * @brief 获取当前绑定的缓冲区
     * @param target 缓冲区目标
     */
    uint32_t GetBoundBuffer(BufferTarget target) const;
    
    // ========================================================================
    // 着色器程序管理
    // ========================================================================
    
    /**
     * @brief 绑定着色器程序
     * @param programId 着色器程序 ID
     */
    void UseProgram(uint32_t programId);
    
    /**
     * @brief 获取当前使用的着色器程序
     */
    uint32_t GetCurrentProgram() const;
    
private:
    void ApplyDepthTest();
    void ApplyDepthFunc();
    void ApplyDepthWrite();
    void ApplyBlendMode();
    void ApplyCullFace();
    
    uint32_t GetGLBufferTarget(BufferTarget target) const;
    
    // ========================================================================
    // 基础渲染状态缓存
    // ========================================================================
    bool m_depthTest;
    DepthFunc m_depthFunc;
    bool m_depthWrite;
    BlendMode m_blendMode;
    uint32_t m_blendSrcFactor;
    uint32_t m_blendDstFactor;
    CullFace m_cullFace;
    Color m_clearColor;
    
    // 避免重复设置
    bool m_depthTestDirty;
    bool m_depthFuncDirty;
    bool m_depthWriteDirty;
    bool m_blendModeDirty;
    bool m_cullFaceDirty;
    
    // ========================================================================
    // 纹理绑定状态
    // ========================================================================
    static constexpr uint32_t MAX_TEXTURE_UNITS = 32;
    std::array<uint32_t, MAX_TEXTURE_UNITS> m_boundTextures;
    uint32_t m_activeTextureUnit;
    
    // ========================================================================
    // 缓冲区绑定状态
    // ========================================================================
    uint32_t m_boundVAO;
    uint32_t m_boundArrayBuffer;         // VBO
    uint32_t m_boundElementArrayBuffer;  // EBO
    uint32_t m_boundUniformBuffer;       // UBO
    uint32_t m_boundShaderStorageBuffer; // SSBO
    
    // ========================================================================
    // 着色器程序状态
    // ========================================================================
    uint32_t m_currentProgram;
    
    // ========================================================================
    // 缓存同步控制
    // ========================================================================
    bool m_strictMode;  // 严格模式：true = 不使用缓存，false = 使用缓存优化
    
    // ========================================================================
    // 线程安全
    // ========================================================================
    mutable std::shared_mutex m_mutex;  // 读写锁，支持多读单写
};

} // namespace Render

