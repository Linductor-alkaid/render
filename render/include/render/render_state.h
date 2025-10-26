#pragma once

#include "render/types.h"

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
 * @brief 渲染状态管理类
 * 
 * 缓存和管理 OpenGL 渲染状态，减少不必要的状态切换
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
    BlendMode GetBlendMode() const { return m_blendMode; }
    
    /**
     * @brief 获取当前面剔除模式
     */
    CullFace GetCullFace() const { return m_cullFace; }
    
private:
    void ApplyDepthTest();
    void ApplyDepthFunc();
    void ApplyDepthWrite();
    void ApplyBlendMode();
    void ApplyCullFace();
    
    // 状态缓存
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
};

} // namespace Render

