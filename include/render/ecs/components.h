#pragma once

#include "render/transform.h"
#include "render/types.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_set>

namespace Render {

// 前向声明
class Camera;
class Mesh;
class Material;
class Texture;
class Framebuffer;

namespace ECS {

// ============================================================
// Transform 组件（避免反复创建销毁）
// ============================================================

/**
 * @brief Transform 组件
 * 
 * 使用 shared_ptr 复用 Transform 对象，避免频繁创建销毁
 * 提供快捷访问接口，简化使用
 */
struct TransformComponent {
    Ref<Transform> transform;    ///< 复用 Transform 对象（shared_ptr）
    
    // 默认构造函数 - 自动创建 Transform 对象
    TransformComponent() : transform(std::make_shared<Transform>()) {}
    
    // 使用现有 Transform 对象构造
    explicit TransformComponent(const Ref<Transform>& t) : transform(t) {}
    
    // ==================== 快捷访问接口 ====================
    
    void SetPosition(const Vector3& pos) {
        if (transform) transform->SetPosition(pos);
    }
    
    void SetRotation(const Quaternion& rot) {
        if (transform) transform->SetRotation(rot);
    }
    
    void SetScale(const Vector3& scale) {
        if (transform) transform->SetScale(scale);
    }
    
    void SetScale(float uniformScale) {
        if (transform) transform->SetScale(uniformScale);
    }
    
    [[nodiscard]] Vector3 GetPosition() const {
        return transform ? transform->GetPosition() : Vector3::Zero();
    }
    
    [[nodiscard]] Quaternion GetRotation() const {
        return transform ? transform->GetRotation() : Quaternion::Identity();
    }
    
    [[nodiscard]] Vector3 GetScale() const {
        return transform ? transform->GetScale() : Vector3::Ones();
    }
    
    [[nodiscard]] Matrix4 GetLocalMatrix() const {
        return transform ? transform->GetLocalMatrix() : Matrix4::Identity();
    }
    
    [[nodiscard]] Matrix4 GetWorldMatrix() const {
        return transform ? transform->GetWorldMatrix() : Matrix4::Identity();
    }
    
    // 朝向目标
    void LookAt(const Vector3& target, const Vector3& up = Vector3::UnitY()) {
        if (transform) transform->LookAt(target, up);
    }
    
    // 父子关系
    void SetParent(const Ref<Transform>& parent) {
        if (transform && parent) transform->SetParent(parent.get());
    }
    
    // 移除父对象
    void RemoveParent() {
        if (transform) transform->SetParent(nullptr);
    }
    
    // 获取父对象
    [[nodiscard]] Transform* GetParent() const {
        return transform ? transform->GetParent() : nullptr;
    }
};

// ============================================================
// 名称组件
// ============================================================

/**
 * @brief 名称组件
 * 
 * 为实体提供可读的名称，用于调试和显示
 */
struct NameComponent {
    std::string name;
    
    NameComponent() = default;
    explicit NameComponent(const std::string& n) : name(n) {}
};

// ============================================================
// 标签组件
// ============================================================

/**
 * @brief 标签组件
 * 
 * 为实体添加多个标签，用于查询和分组
 */
struct TagComponent {
    std::unordered_set<std::string> tags;
    
    TagComponent() = default;
    explicit TagComponent(const std::vector<std::string>& tagList) {
        for (const auto& tag : tagList) {
            tags.insert(tag);
        }
    }
    
    [[nodiscard]] bool HasTag(const std::string& tag) const {
        return tags.find(tag) != tags.end();
    }
    
    void AddTag(const std::string& tag) {
        tags.insert(tag);
    }
    
    void RemoveTag(const std::string& tag) {
        tags.erase(tag);
    }
    
    void Clear() {
        tags.clear();
    }
    
    [[nodiscard]] std::vector<std::string> GetTagList() const {
        return std::vector<std::string>(tags.begin(), tags.end());
    }
};

// ============================================================
// 激活状态组件
// ============================================================

/**
 * @brief 激活状态组件
 * 
 * 控制实体是否激活（激活的实体才会被系统处理）
 */
struct ActiveComponent {
    bool active = true;
    
    ActiveComponent() = default;
    explicit ActiveComponent(bool a) : active(a) {}
};

// ============================================================
// Mesh 渲染组件
// ============================================================

/**
 * @brief Mesh 渲染组件
 * 
 * 用于 3D 网格渲染
 * 支持异步资源加载和 LOD
 */
struct MeshRenderComponent {
    // 资源引用（通过 ResourceManager 管理）
    std::string meshName;          ///< 网格资源名称
    std::string materialName;      ///< 材质资源名称
    
    Ref<Mesh> mesh;                ///< 网格对象（延迟加载）
    Ref<Material> material;        ///< 材质对象（延迟加载）
    
    // 渲染属性
    bool visible = true;           ///< 是否可见
    bool castShadows = true;       ///< 是否投射阴影
    bool receiveShadows = true;    ///< 是否接收阴影
    uint32_t layerID = 300;        ///< 渲染层级（默认 WORLD_GEOMETRY）
    uint32_t renderPriority = 0;   ///< 渲染优先级
    
    // LOD 支持
    std::vector<float> lodDistances;  ///< LOD 距离阈值
    
    // 异步加载状态
    bool resourcesLoaded = false;     ///< 资源是否已加载
    bool asyncLoading = false;        ///< 是否正在异步加载
    
    MeshRenderComponent() = default;
};

// ============================================================
// Sprite 渲染组件（2D）
// ============================================================

/**
 * @brief Sprite 渲染组件
 * 
 * 用于 2D 精灵渲染
 */
struct SpriteRenderComponent {
    std::string textureName;       ///< 纹理资源名称
    Ref<Texture> texture;          ///< 纹理对象（延迟加载）
    
    Rect sourceRect{0, 0, 1, 1};   ///< 源矩形（UV 坐标）
    Vector2 size{1.0f, 1.0f};      ///< 显示大小
    Color tintColor{1, 1, 1, 1};   ///< 着色
    
    bool visible = true;
    uint32_t layerID = 800;        ///< UI_LAYER
    
    bool resourcesLoaded = false;
    bool asyncLoading = false;
    
    SpriteRenderComponent() = default;
};

// ============================================================
// Camera 组件
// ============================================================

/**
 * @brief Camera 组件
 * 
 * 使用 shared_ptr 复用 Camera 对象
 */
struct CameraComponent {
    Ref<Camera> camera;            ///< 相机对象（复用）
    
    bool active = true;            ///< 是否激活
    uint32_t layerMask = 0xFFFFFFFF;  ///< 可见层级遮罩
    int32_t depth = 0;             ///< 渲染深度（深度越低越先渲染）
    Color clearColor{0.1f, 0.1f, 0.1f, 1.0f};
    
    // 渲染目标（可选）
    std::string renderTargetName;
    Ref<Framebuffer> renderTarget;
    
    CameraComponent() = default;
    explicit CameraComponent(const Ref<Camera>& cam) : camera(cam) {}
};

// ============================================================
// Light 组件
// ============================================================

/**
 * @brief 光源类型
 */
enum class LightType {
    Directional,   ///< 定向光
    Point,         ///< 点光源
    Spot,          ///< 聚光灯
    Area           ///< 区域光（未来支持）
};

/**
 * @brief Light 组件
 * 
 * 用于场景光照
 */
struct LightComponent {
    LightType type = LightType::Point;
    
    Color color{1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    
    // Point/Spot 光源
    float range = 10.0f;
    float attenuation = 1.0f;
    
    // Spot 光源
    float innerConeAngle = 30.0f;  ///< 内角（度）
    float outerConeAngle = 45.0f;  ///< 外角（度）
    
    // 阴影
    bool castShadows = false;
    uint32_t shadowMapSize = 1024;
    float shadowBias = 0.001f;
    
    bool enabled = true;
    
    LightComponent() = default;
};

} // namespace ECS
} // namespace Render

