/*
 * Copyright (c) 2025 Li Chaoyu
 * 
 * This file is part of Render.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * For commercial licensing, please contact: 2052046346@qq.com
 */
#include "render/physics/bullet_adapter/bullet_material_callback.h"
#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <BulletCollision/CollisionDispatch/btCollisionObject.h>

namespace Render::Physics::BulletAdapter {

// 静态成员初始化
std::function<std::shared_ptr<PhysicsMaterial>(ECS::EntityID)> BulletMaterialCallback::s_materialGetter;

BulletMaterialCallback::BulletMaterialCallback(
    const std::unordered_map<ECS::EntityID, btRigidBody*, ECS::EntityID::Hash>& entityToRigidBody,
    const std::unordered_map<btRigidBody*, ECS::EntityID>& rigidBodyToEntity)
    : m_entityToRigidBody(entityToRigidBody)
    , m_rigidBodyToEntity(rigidBodyToEntity) {
}

bool BulletMaterialCallback::ProcessContactPoint(btManifoldPoint& cp,
                                                 const btCollisionObject* colObj0,
                                                 const btCollisionObject* colObj1) const {
    // ==================== 2.4.3 实现材质组合模式 ====================
    
    // 转换为刚体
    const btRigidBody* body0 = btRigidBody::upcast(colObj0);
    const btRigidBody* body1 = btRigidBody::upcast(colObj1);
    
    if (!body0 || !body1) {
        return false;  // 不是刚体，使用默认处理
    }
    
    // 查找对应的实体ID
    auto it0 = m_rigidBodyToEntity.find(const_cast<btRigidBody*>(body0));
    auto it1 = m_rigidBodyToEntity.find(const_cast<btRigidBody*>(body1));
    
    if (it0 == m_rigidBodyToEntity.end() || it1 == m_rigidBodyToEntity.end()) {
        return false;  // 不是我们管理的实体，使用默认处理
    }
    
    ECS::EntityID entity0 = it0->second;
    ECS::EntityID entity1 = it1->second;
    
    // 获取材质
    std::shared_ptr<PhysicsMaterial> material0 = GetMaterial(entity0);
    std::shared_ptr<PhysicsMaterial> material1 = GetMaterial(entity1);
    
    // 如果没有材质，使用 Bullet 默认值（从碰撞对象获取）
    float friction0 = material0 ? material0->friction : colObj0->getFriction();
    float friction1 = material1 ? material1->friction : colObj1->getFriction();
    
    float restitution0 = material0 ? material0->restitution : colObj0->getRestitution();
    float restitution1 = material1 ? material1->restitution : colObj1->getRestitution();
    
    // 确定组合模式（使用第一个物体的组合模式，或默认使用 Average）
    PhysicsMaterial::CombineMode frictionMode = PhysicsMaterial::CombineMode::Average;
    PhysicsMaterial::CombineMode restitutionMode = PhysicsMaterial::CombineMode::Average;
    
    if (material0) {
        frictionMode = material0->frictionCombine;
        restitutionMode = material0->restitutionCombine;
    } else if (material1) {
        frictionMode = material1->frictionCombine;
        restitutionMode = material1->restitutionCombine;
    }
    
    // 计算组合后的值
    float combinedFriction = PhysicsMaterial::CombineValues(friction0, friction1, frictionMode);
    float combinedRestitution = PhysicsMaterial::CombineValues(restitution0, restitution1, restitutionMode);
    
    // 设置到接触点
    cp.m_combinedFriction = combinedFriction;
    cp.m_combinedRestitution = combinedRestitution;
    
    return true;  // 已处理
}

std::shared_ptr<PhysicsMaterial> BulletMaterialCallback::GetMaterial(ECS::EntityID entity) {
    if (s_materialGetter) {
        return s_materialGetter(entity);
    }
    return nullptr;
}

void BulletMaterialCallback::SetMaterialGetter(std::function<std::shared_ptr<PhysicsMaterial>(ECS::EntityID)> getter) {
    s_materialGetter = std::move(getter);
}

} // namespace Render::Physics::BulletAdapter

