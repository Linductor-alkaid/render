#pragma once

#include "physics_config.h"
#include "physics_components.h"

namespace Render {

namespace ECS {
class World;  // 前向声明
}

namespace Physics {

/**
 * @brief 物理世界
 * 
 * 管理整个物理模拟系统的核心类
 * 
 * @note 这是一个初始框架，完整实现将在后续阶段添加
 */
class PhysicsWorld {
public:
    /**
     * @brief 构造函数
     * @param ecsWorld ECS 世界指针
     * @param config 物理配置
     */
    PhysicsWorld(ECS::World* ecsWorld, const PhysicsConfig& config = PhysicsConfig::Default())
        : m_ecsWorld(ecsWorld), m_config(config) {}
    
    /**
     * @brief 析构函数
     */
    ~PhysicsWorld() = default;
    
    /**
     * @brief 物理步进（每帧调用）
     * @param deltaTime 帧时间（秒）
     */
    void Step(float deltaTime) {
        // 实现将在后续添加
        (void)deltaTime;  // 避免未使用参数警告
    }
    
    /**
     * @brief 设置重力
     */
    void SetGravity(const Vector3& gravity) {
        m_config.gravity = gravity;
    }
    
    /**
     * @brief 获取重力
     */
    Vector3 GetGravity() const {
        return m_config.gravity;
    }
    
    /**
     * @brief 获取配置
     */
    const PhysicsConfig& GetConfig() const {
        return m_config;
    }
    
private:
    ECS::World* m_ecsWorld;
    PhysicsConfig m_config;
    
    // 更多成员变量将在后续阶段添加
};

} // namespace Physics
} // namespace Render

