#pragma once

#include "render/transform.h"
#include "render/types.h"
#include "render/mesh.h"
#include "render/material.h"
#include "render/texture.h"
#include <Eigen/Dense>
#include <shared_mutex>

namespace Render {

// 前向声明
class Renderer;

/**
 * @brief 渲染对象类型
 */
enum class RenderableType {
    Mesh,       ///< 3D 网格
    Sprite,     ///< 2D 精灵
    Text,       ///< 文本（未来）
    Particle,   ///< 粒子（未来）
    Custom      ///< 自定义
};

// ============================================================
// Renderable 基类
// ============================================================

/**
 * @brief Renderable 基类
 * 
 * 所有可渲染对象的基类，提供统一的渲染接口
 * - 使用 shared_ptr 复用 Transform 对象
 * - 提供包围盒接口用于视锥体裁剪
 * - 线程安全的所有操作
 */
class Renderable {
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    
public:
    explicit Renderable(RenderableType type);
    virtual ~Renderable() = default;
    
    // 禁止拷贝
    Renderable(const Renderable&) = delete;
    Renderable& operator=(const Renderable&) = delete;
    
    // 显式定义移动操作（因为 shared_mutex 不可移动）
    Renderable(Renderable&& other) noexcept;
    Renderable& operator=(Renderable&& other) noexcept;
    
    // ==================== 渲染接口 ====================
    
    /**
     * @brief 渲染对象（纯虚函数）
     */
    virtual void Render() = 0;
    
    /**
     * @brief 提交到渲染器（纯虚函数）
     * @param renderer 渲染器指针
     */
    virtual void SubmitToRenderer(Renderer* renderer) = 0;
    
    // ==================== 变换 ====================
    
    /**
     * @brief 设置变换对象
     * @param transform Transform 对象
     */
    void SetTransform(const Ref<Transform>& transform);
    
    /**
     * @brief 获取变换对象
     * @return Transform 对象
     */
    [[nodiscard]] Ref<Transform> GetTransform() const;
    
    /**
     * @brief 获取世界变换矩阵
     * @return 世界变换矩阵
     */
    [[nodiscard]] Matrix4 GetWorldMatrix() const;
    
    // ==================== 可见性 ====================
    
    /**
     * @brief 设置可见性
     * @param visible 是否可见
     */
    void SetVisible(bool visible);
    
    /**
     * @brief 获取可见性
     * @return 是否可见
     */
    [[nodiscard]] bool IsVisible() const;
    
    // ==================== 层级 ====================
    
    /**
     * @brief 设置渲染层级
     * @param layerID 层级 ID
     */
    void SetLayerID(uint32_t layerID);
    
    /**
     * @brief 获取渲染层级
     * @return 层级 ID
     */
    [[nodiscard]] uint32_t GetLayerID() const;
    
    /**
     * @brief 设置渲染优先级
     * @param priority 优先级
     */
    void SetRenderPriority(uint32_t priority);
    
    /**
     * @brief 获取渲染优先级
     * @return 优先级
     */
    [[nodiscard]] uint32_t GetRenderPriority() const;
    
    // ==================== 类型 ====================
    
    /**
     * @brief 获取渲染对象类型
     * @return 渲染对象类型
     */
    [[nodiscard]] RenderableType GetType() const { return m_type; }
    
    // ==================== 包围盒 ====================
    
    /**
     * @brief 获取包围盒（纯虚函数）
     * 
     * 用于视锥体裁剪优化
     * 
     * @return 轴对齐包围盒
     */
    [[nodiscard]] virtual AABB GetBoundingBox() const = 0;
    
protected:
    RenderableType m_type;                ///< 渲染对象类型
    Ref<Transform> m_transform;           ///< 变换对象（复用）
    bool m_visible = true;                ///< 可见性
    uint32_t m_layerID = 300;             ///< 渲染层级（WORLD_GEOMETRY）
    uint32_t m_renderPriority = 0;        ///< 渲染优先级
    
    mutable std::shared_mutex m_mutex;    ///< 线程安全锁
};

// ============================================================
// MeshRenderable（3D 网格渲染对象）
// ============================================================

/**
 * @brief MeshRenderable（3D 网格渲染对象）
 * 
 * 用于渲染 3D 网格，支持：
 * - 网格和材质设置
 * - 阴影投射和接收
 * - 包围盒计算
 */
class MeshRenderable : public Renderable {
public:
    MeshRenderable();
    ~MeshRenderable() override = default;
    
    // 禁止拷贝
    MeshRenderable(const MeshRenderable&) = delete;
    MeshRenderable& operator=(const MeshRenderable&) = delete;
    
    // 显式定义移动操作
    MeshRenderable(MeshRenderable&& other) noexcept;
    MeshRenderable& operator=(MeshRenderable&& other) noexcept;
    
    // ==================== 渲染 ====================
    
    void Render() override;
    void SubmitToRenderer(Renderer* renderer) override;
    
    // ==================== 资源设置 ====================
    
    /**
     * @brief 设置网格
     * @param mesh 网格对象
     */
    void SetMesh(const Ref<Mesh>& mesh);
    
    /**
     * @brief 获取网格
     * @return 网格对象
     */
    [[nodiscard]] Ref<Mesh> GetMesh() const;
    
    /**
     * @brief 设置材质
     * @param material 材质对象
     */
    void SetMaterial(const Ref<Material>& material);
    
    /**
     * @brief 获取材质
     * @return 材质对象
     */
    [[nodiscard]] Ref<Material> GetMaterial() const;
    
    // ==================== 阴影 ====================
    
    /**
     * @brief 设置是否投射阴影
     * @param cast 是否投射阴影
     */
    void SetCastShadows(bool cast);
    
    /**
     * @brief 获取是否投射阴影
     * @return 是否投射阴影
     */
    [[nodiscard]] bool GetCastShadows() const;
    
    /**
     * @brief 设置是否接收阴影
     * @param receive 是否接收阴影
     */
    void SetReceiveShadows(bool receive);
    
    /**
     * @brief 获取是否接收阴影
     * @return 是否接收阴影
     */
    [[nodiscard]] bool GetReceiveShadows() const;
    
    // ==================== 包围盒 ====================
    
    [[nodiscard]] AABB GetBoundingBox() const override;
    
private:
    Ref<Mesh> m_mesh;                     ///< 网格对象
    Ref<Material> m_material;             ///< 材质对象
    bool m_castShadows = true;            ///< 是否投射阴影
    bool m_receiveShadows = true;         ///< 是否接收阴影
};

// ============================================================
// SpriteRenderable（2D 精灵渲染对象）
// ============================================================

/**
 * @brief SpriteRenderable（2D 精灵渲染对象）
 * 
 * 用于渲染 2D 精灵，支持：
 * - 纹理设置
 * - UV 矩形和大小
 * - 着色和混合模式
 */
class SpriteRenderable : public Renderable {
public:
    SpriteRenderable();
    ~SpriteRenderable() override = default;
    
    // 禁止拷贝
    SpriteRenderable(const SpriteRenderable&) = delete;
    SpriteRenderable& operator=(const SpriteRenderable&) = delete;
    
    // 显式定义移动操作
    SpriteRenderable(SpriteRenderable&& other) noexcept;
    SpriteRenderable& operator=(SpriteRenderable&& other) noexcept;
    
    // ==================== 渲染 ====================
    
    void Render() override;
    void SubmitToRenderer(Renderer* renderer) override;
    
    // ==================== 纹理 ====================
    
    /**
     * @brief 设置纹理
     * @param texture 纹理对象
     */
    void SetTexture(const Ref<Texture>& texture);
    
    /**
     * @brief 获取纹理
     * @return 纹理对象
     */
    [[nodiscard]] Ref<Texture> GetTexture() const;
    
    // ==================== 显示属性 ====================
    
    /**
     * @brief 设置源矩形（UV 坐标）
     * @param rect 源矩形
     */
    void SetSourceRect(const Rect& rect);
    
    /**
     * @brief 获取源矩形
     * @return 源矩形
     */
    [[nodiscard]] Rect GetSourceRect() const;
    
    /**
     * @brief 设置显示大小
     * @param size 显示大小
     */
    void SetSize(const Vector2& size);
    
    /**
     * @brief 获取显示大小
     * @return 显示大小
     */
    [[nodiscard]] Vector2 GetSize() const;
    
    /**
     * @brief 设置着色颜色
     * @param color 着色颜色
     */
    void SetTintColor(const Color& color);
    
    /**
     * @brief 获取着色颜色
     * @return 着色颜色
     */
    [[nodiscard]] Color GetTintColor() const;
    
    // ==================== 包围盒 ====================
    
    [[nodiscard]] AABB GetBoundingBox() const override;
    
private:
    Ref<Texture> m_texture;               ///< 纹理对象
    Rect m_sourceRect{0, 0, 1, 1};        ///< 源矩形（UV 坐标）
    Vector2 m_size{1.0f, 1.0f};           ///< 显示大小
    Color m_tintColor{1, 1, 1, 1};        ///< 着色颜色
};

} // namespace Render

