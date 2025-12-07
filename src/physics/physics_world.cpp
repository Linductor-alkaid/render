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
#include "render/physics/physics_world.h"
#include "render/physics/physics_systems.h"
#include "render/ecs/world.h"

namespace Render {
namespace Physics {

void PhysicsWorld::Step(float deltaTime) {
    if (!m_ecsWorld) {
        return;
    }

    auto* physicsSystem = m_ecsWorld->GetSystem<PhysicsUpdateSystem>();

    if (physicsSystem) {
        physicsSystem->SetGravity(m_config.gravity);
        physicsSystem->SetFixedDeltaTime(m_config.fixedDeltaTime);
        physicsSystem->SetSolverIterations(m_config.solverIterations);
        physicsSystem->SetPositionIterations(m_config.positionIterations);
        physicsSystem->Update(deltaTime);
    }
}

} // namespace Physics
} // namespace Render

