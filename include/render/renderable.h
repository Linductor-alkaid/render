#pragma once

#include "render/transform.h"
#include "render/types.h"
#include "render/mesh.h"
#include "render/material.h"
#include "render/texture.h"
#include "render/material_sort_key.h"
#include "render/model.h"
#include <Eigen/Dense>
#include <shared_mutex>
#include <optional>
#include <cstdint>

namespace Render {

// 前向声明
class Renderer;
class Text;
class SpriteBatcher;
class Shader;
class Mesh;
class Material;
class Texture;
class Text;
struct TextRenderBatchData;

struct TextRenderBatchData {
    Ref<Texture> texture;
    Ref<Mesh> mesh;
    Ref<Shader> shader;
    Matrix4 modelMatrix = Matrix4::Identity();
    Matrix4 viewMatrix = Matrix4::Identity();
    Matrix4 projectionMatrix = Matrix4::Identity();
    Color color = Color::White();
    bool screenSpace = true;
    uint32_t viewHash = 0;
    uint32_t projectionHash = 0;
};

/**
 * @brief 渲染对象类型
 */
enum class RenderableType {
    Mesh,       ///< 3D 网格
    Model,      ///< 组合模型
    Sprite,     ///< 2D 精灵
    Text,       ///< 文本
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
     * @param renderState 渲染状态管理器（可选）
     *                    如果提供，材质将应用其渲染状态设置
     */
    virtual void Render(RenderState* renderState = nullptr) = 0;
    
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

    /**
     * @brief 更新矩阵缓存（强制重新计算）
     *
     * 通常不需要手动调用，系统会在适当时机自动更新缓存。
     * 在Transform被外部修改后，可以调用此方法强制更新缓存。
     */
    void UpdateMatrixCache() const;
    
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
    void SetRenderPriority(int32_t priority);
    
    /**
     * @brief 获取渲染优先级
     * @return 优先级
     */
    [[nodiscard]] int32_t GetRenderPriority() const;

    // ==================== 材质排序 ====================

    /**
     * @brief 设置材质排序键
     */
    void SetMaterialSortKey(const MaterialSortKey& key);

    /**
     * @brief 获取材质排序键（若未设置返回默认键）
     */
    [[nodiscard]] MaterialSortKey GetMaterialSortKey() const;

    /**
     * @brief 是否已经设置过材质排序键
     */
    [[nodiscard]] bool HasMaterialSortKey() const;

    /**
     * @brief 将材质排序标记为需要刷新
     */
    void MarkMaterialSortKeyDirty();

    /**
     * @brief 材质排序键是否处于待刷新状态
     */
    [[nodiscard]] bool IsMaterialSortKeyDirty() const;

    /**
     * @brief 设置透明排序的深度提示值
     * @param depth 深度提示（通常为到相机的距离，越大表示越远）
     */
    void SetDepthHint(float depth);

    /**
     * @brief 是否已经提供深度提示
     */
    [[nodiscard]] bool HasDepthHint() const;

    /**
     * @brief 获取深度提示
     */
    [[nodiscard]] float GetDepthHint() const;

    /**
     * @brief 清除深度提示
     */
    void ClearDepthHint();

    /**
     * @brief 设置透明提示标记
     */
    void SetTransparentHint(bool transparent);

    /**
     * @brief 读取透明提示标记
     */
    [[nodiscard]] bool GetTransparentHint() const;
    
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
    int32_t m_renderPriority = 0;         ///< 渲染优先级

    MaterialSortKey m_materialSortKey{};
    bool m_materialSortDirty = true;
    bool m_hasMaterialSortKey = false;
    bool m_transparentHint = false;
    float m_depthHint = 0.0f;
    bool m_hasDepthHint = false;

    // 矩阵缓存（阶段1.1优化）
    mutable Matrix4 m_cachedWorldMatrix;           ///< 缓存的世界变换矩阵
    mutable uint64_t m_cachedTransformVersion{0}; ///< Transform版本号缓存
    mutable bool m_matrixCacheValid{false};       ///< 矩阵缓存是否有效

    mutable std::shared_mutex m_mutex;    ///< 线程安全锁
};

// ============================================================
// MeshRenderable（3D 网格渲染对象）
// ============================================================

/**
 * @brief 材质属性覆盖
 * 
 * 用于在渲染时临时覆盖材质属性，而不修改共享的Material对象
 * 这样多个实体可以共享同一个Material，但有不同的外观
 */
struct MaterialOverride {
    std::optional<Color> diffuseColor;      ///< 漫反射颜色覆盖
    std::optional<Color> specularColor;     ///< 镜面反射颜色覆盖
    std::optional<Color> emissiveColor;     ///< 自发光颜色覆盖
    std::optional<float> shininess;         ///< 镜面反射强度覆盖
    std::optional<float> metallic;          ///< 金属度覆盖
    std::optional<float> roughness;         ///< 粗糙度覆盖
    std::optional<float> opacity;           ///< 不透明度覆盖
    
    /// 检查是否有任何覆盖
    [[nodiscard]] bool HasAnyOverride() const {
        return diffuseColor.has_value() || specularColor.has_value() || 
               emissiveColor.has_value() || shininess.has_value() || 
               metallic.has_value() || roughness.has_value() || 
               opacity.has_value();
    }

    /// 计算覆盖内容的哈希值（用于材质排序键）
    [[nodiscard]] uint32_t ComputeHash() const;
    
    /// 清除所有覆盖
    void Clear() {
        diffuseColor.reset();
        specularColor.reset();
        emissiveColor.reset();
        shininess.reset();
        metallic.reset();
        roughness.reset();
        opacity.reset();
    }
};

/**
 * @brief MeshRenderable（3D 网格渲染对象）
 * 
 * 用于渲染 3D 网格，支持：
 * - 网格和材质设置
 * - 材质属性覆盖（MaterialOverride）
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
    
    void Render(RenderState* renderState = nullptr) override;
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
    
    // ==================== 材质属性覆盖 ====================
    
    /**
     * @brief 设置材质属性覆盖
     * @param override 材质属性覆盖
     * 
     * 这些覆盖会在渲染时应用，不会修改原始的Material对象
     * 这样允许多个实体共享同一个Material但有不同的外观
     */
    void SetMaterialOverride(const MaterialOverride& override);
    
    /**
     * @brief 获取材质属性覆盖
     * @return 材质属性覆盖
     */
    [[nodiscard]] MaterialOverride GetMaterialOverride() const;
    
    /**
     * @brief 检查是否有材质属性覆盖
     * @return 是否有覆盖
     */
    [[nodiscard]] bool HasMaterialOverride() const;
    
    /**
     * @brief 清除所有材质属性覆盖
     */
    void ClearMaterialOverride();
    
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
    MaterialOverride m_materialOverride;  ///< 材质属性覆盖
    bool m_castShadows = true;            ///< 是否投射阴影
    bool m_receiveShadows = true;         ///< 是否接收阴影
};

/**
 * @brief ModelRenderable（组合模型渲染对象）
 *
 * 负责渲染包含多个子网格/材质的 `Model`。该渲染对象会在渲染阶段遍历
 * `ModelPart`，依次绑定材质并绘制网格，同时支持统一的透明度提示与包围盒计算。
 */
class ModelRenderable : public Renderable {
public:
    ModelRenderable();
    ~ModelRenderable() override = default;

    ModelRenderable(const ModelRenderable&) = delete;
    ModelRenderable& operator=(const ModelRenderable&) = delete;

    ModelRenderable(ModelRenderable&& other) noexcept;
    ModelRenderable& operator=(ModelRenderable&& other) noexcept;

    void Render(RenderState* renderState = nullptr) override;
    void SubmitToRenderer(Renderer* renderer) override;

    void SetModel(const ModelPtr& model);
    [[nodiscard]] ModelPtr GetModel() const;

    [[nodiscard]] size_t GetPartCount() const;
    [[nodiscard]] bool HasSkinning() const;

    void SetCastShadows(bool cast);
    [[nodiscard]] bool GetCastShadows() const;

    void SetReceiveShadows(bool receive);
    [[nodiscard]] bool GetReceiveShadows() const;

    [[nodiscard]] AABB GetBoundingBox() const override;

private:
    void UpdateTransparencyHintLocked();

private:
    ModelPtr m_model;
    bool m_castShadows = true;
    bool m_receiveShadows = true;
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
    
    void Render(RenderState* renderState = nullptr) override;
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
    
    /**
     * @brief 设置视图/投影矩阵覆盖
     * @param view 视图矩阵
     * @param projection 投影矩阵
     */
    void SetViewProjectionOverride(const Matrix4& view, const Matrix4& projection);

    /**
     * @brief 清除视图/投影矩阵覆盖
     */
    void ClearViewProjectionOverride();
    
    // ==================== 包围盒 ====================
    
    [[nodiscard]] AABB GetBoundingBox() const override;

    /**
     * @brief 设置全局视图与投影矩阵
     *
     * 由 SpriteRenderSystem 在每帧开始时调用，用于屏幕空间渲染。
     */
    static void SetViewProjection(const Matrix4& view, const Matrix4& projection);

    /**
     * @brief 获取共享的精灵渲染资源（Quad Mesh 与 Shader）
     * @param outMesh 返回的四边形网格
     * @param outShader 返回的精灵着色器
     * @return 当资源可用时返回 true
     */
    static bool AcquireSharedResources(Ref<Mesh>& outMesh, Ref<Shader>& outShader);
    
private:
    Ref<Texture> m_texture;               ///< 纹理对象
    Rect m_sourceRect{0, 0, 1, 1};        ///< 源矩形（UV 坐标）
    Vector2 m_size{1.0f, 1.0f};           ///< 显示大小
    Color m_tintColor{1, 1, 1, 1};        ///< 着色颜色
    Matrix4 m_viewMatrixOverride = Matrix4::Identity();
    Matrix4 m_projectionMatrixOverride = Matrix4::Identity();
    bool m_useViewProjectionOverride = false;
};

// ============================================================
// TextRenderable（文本渲染对象）
// ============================================================

class TextRenderable : public Renderable {
public:
    TextRenderable();
    ~TextRenderable() override = default;

    TextRenderable(const TextRenderable&) = delete;
    TextRenderable& operator=(const TextRenderable&) = delete;

    TextRenderable(TextRenderable&& other) noexcept;
    TextRenderable& operator=(TextRenderable&& other) noexcept;

    void Render(RenderState* renderState = nullptr) override;
    void SubmitToRenderer(Renderer* renderer) override;

    void SetText(const Ref<Text>& text);
    [[nodiscard]] Ref<Text> GetText() const;

    void SetViewProjectionOverride(const Matrix4& view, const Matrix4& projection);
    void ClearViewProjectionOverride();

    [[nodiscard]] AABB GetBoundingBox() const override;

    static void SetViewProjection(const Matrix4& view, const Matrix4& projection);
    static bool AcquireSharedResources(Ref<Mesh>& outMesh, Ref<Shader>& outShader);
    static void GetSharedMatrices(Matrix4& outView, Matrix4& outProjection, bool& outInitialized);

    bool GatherBatchData(TextRenderBatchData& outData);

private:
    Ref<Text> m_text;
    Matrix4 m_viewMatrixOverride = Matrix4::Identity();
    Matrix4 m_projectionMatrixOverride = Matrix4::Identity();
    bool m_useViewProjectionOverride = false;
    mutable Vector2 m_cachedSize{0.0f, 0.0f};
};

} // namespace Render

