#include "render/application/modules/physics_module.h"
#include "render/physics/physics_world.h"
#include "render/ecs/world.h"
#include "render/logger.h"

namespace Render::Application {

PhysicsModule::PhysicsModule(const Physics::PhysicsConfig& config)
    : m_config(config) {
    Logger::GetInstance().Info("[PhysicsModule] 物理引擎模块已创建");
}

PhysicsModule::~PhysicsModule() {
    Logger::GetInstance().Info("[PhysicsModule] 物理引擎模块已销毁");
}

int PhysicsModule::Priority(ModulePhase phase) const {
    switch (phase) {
        case ModulePhase::Register:
            return 100;  // 中等优先级
        case ModulePhase::PreFrame:
            return 200;  // 在渲染之前更新物理
        case ModulePhase::PostFrame:
            return 0;    // 不需要在帧后处理
        default:
            return 0;
    }
}

void PhysicsModule::OnRegister(ECS::World& world, AppContext& ctx) {
    if (m_registered) {
        Logger::GetInstance().Warning("[PhysicsModule] 模块已注册，跳过重复注册");
        return;
    }
    
    Logger::GetInstance().Info("[PhysicsModule] 注册物理引擎模块...");
    
    // 注册物理组件
    RegisterPhysicsComponents(world);
    
    // 创建物理世界
    m_physicsWorld = std::make_unique<Physics::PhysicsWorld>(&world, m_config);
    
    // 注册物理系统（将在后续阶段实现）
    // RegisterPhysicsSystems(world);
    
    m_registered = true;
    
    Logger::GetInstance().Info("[PhysicsModule] 物理引擎模块注册完成");
    Logger::GetInstance().InfoFormat("[PhysicsModule] - 重力: (%.2f, %.2f, %.2f)", 
                 m_config.gravity.x(), m_config.gravity.y(), m_config.gravity.z());
    Logger::GetInstance().InfoFormat("[PhysicsModule] - 固定时间步长: %.4f 秒", m_config.fixedDeltaTime);
    Logger::GetInstance().InfoFormat("[PhysicsModule] - 求解器迭代次数: %d", m_config.solverIterations);
}

void PhysicsModule::OnUnregister(ECS::World& world, AppContext& ctx) {
    if (!m_registered) {
        return;
    }
    
    Logger::GetInstance().Info("[PhysicsModule] 注销物理引擎模块...");
    
    // 清理物理世界
    m_physicsWorld.reset();
    
    m_registered = false;
    
    Logger::GetInstance().Info("[PhysicsModule] 物理引擎模块已注销");
}

void PhysicsModule::OnPreFrame(const FrameUpdateArgs& frame, AppContext& ctx) {
    if (!m_enabled || !m_physicsWorld) {
        return;
    }
    
    // 固定时间步长更新
    m_accumulator += frame.deltaTime;
    
    int stepCount = 0;
    const int maxSteps = m_config.maxSubSteps;
    
    while (m_accumulator >= m_config.fixedDeltaTime && stepCount < maxSteps) {
        // 物理世界更新
        m_physicsWorld->Step(m_config.fixedDeltaTime);
        
        m_accumulator -= m_config.fixedDeltaTime;
        stepCount++;
    }
    
    // 如果帧时间过长，重置累积器防止螺旋死亡
    if (stepCount >= maxSteps) {
        Logger::GetInstance().Warning("[PhysicsModule] 帧时间过长，重置物理累积器");
        m_accumulator = 0.0f;
    }
}

void PhysicsModule::OnPostFrame(const FrameUpdateArgs& frame, AppContext& ctx) {
    // 目前无需帧后处理
}

void PhysicsModule::SetConfig(const Physics::PhysicsConfig& config) {
    m_config = config;
    
    if (m_physicsWorld) {
        m_physicsWorld->SetGravity(config.gravity);
    }
}

const Physics::PhysicsConfig& PhysicsModule::GetConfig() const {
    return m_config;
}

void PhysicsModule::RegisterPhysicsComponents(ECS::World& world) {
    Logger::GetInstance().Info("[PhysicsModule] 注册物理组件...");
    
    // ECS 系统使用模板自动注册组件，无需显式注册
    // 组件类型：
    // - Physics::RigidBodyComponent
    // - Physics::ColliderComponent
    // - Physics::PhysicsMaterial (作为 shared_ptr 使用)
    
    Logger::GetInstance().Info("[PhysicsModule] 物理组件类型已定义，将在使用时自动注册");
}

void PhysicsModule::RegisterPhysicsSystems(ECS::World& world) {
    Logger::GetInstance().Info("[PhysicsModule] 注册物理系统...");
    
    // 系统注册将在后续阶段实现
    // TODO: world.RegisterSystem<Physics::PhysicsUpdateSystem>();
    // TODO: world.RegisterSystem<Physics::CollisionDetectionSystem>();
    
    Logger::GetInstance().Info("[PhysicsModule] 物理系统注册完成（占位符）");
}

} // namespace Render::Application

