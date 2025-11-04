#pragma once

namespace Render {
namespace ECS {

// 前向声明
class World;

/**
 * @brief 系统基类
 * 
 * 系统负责处理具有特定组件的实体
 * 系统按优先级顺序执行（优先级越小越早执行）
 */
class System {
public:
    virtual ~System() = default;
    
    // ==================== 生命周期 ====================
    
    /**
     * @brief 系统创建时调用
     * @param world World 指针
     */
    virtual void OnCreate(World* world) { 
        m_world = world; 
    }
    
    /**
     * @brief 系统销毁时调用
     */
    virtual void OnDestroy() {}
    
    // ==================== 更新 ====================
    
    /**
     * @brief 系统更新（每帧调用）
     * @param deltaTime 帧间隔时间（秒）
     */
    virtual void Update(float deltaTime) = 0;
    
    // ==================== 优先级 ====================
    
    /**
     * @brief 获取系统优先级
     * 
     * 优先级越小越早执行
     * 推荐优先级范围：
     *   - 5: CameraSystem（最高优先级）
     *   - 10: TransformSystem
     *   - 20: ResourceLoadingSystem
     *   - 50: LightSystem
     *   - 100: MeshRenderSystem
     *   - 200: SpriteRenderSystem
     * 
     * @return 优先级值
     */
    [[nodiscard]] virtual int GetPriority() const { return 100; }
    
    // ==================== 启用/禁用 ====================
    
    /**
     * @brief 设置系统启用状态
     * @param enabled 是否启用
     */
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    
    /**
     * @brief 获取系统启用状态
     * @return 是否启用
     */
    [[nodiscard]] bool IsEnabled() const { return m_enabled; }
    
protected:
    World* m_world = nullptr;      ///< World 指针
    bool m_enabled = true;         ///< 启用状态
};

} // namespace ECS
} // namespace Render

